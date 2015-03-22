/*
 * Crossfire -- cooperative multi-player graphical RPG and adventure game
 *
 * Copyright (c) 1999-2013 Mark Wedel and the Crossfire Development Team
 * Copyright (c) 1992 Frank Tore Johansen
 *
 * Crossfire is free software and comes with ABSOLUTELY NO WARRANTY. You are
 * welcome to redistribute it under certain conditions. For details, please
 * see COPYING and LICENSE.
 *
 * The authors can be reached via e-mail at <crossfire@metalforge.org>.
 */

/**
 * @file
 * Client interface main routine.  Sets up a few global variables, connects to
 * the server, tells it what kind of pictures it wants, adds the client and
 * enters the main dispatch loop.
 *
 * The main event loop (event_loop()) checks the TCP socket for input and then
 * polls for x events.  This should be fixed since you can just block on both
 * filedescriptors.
 *
 * The DoClient function receives a message (an ArgList), unpacks it, and in a
 * slow for loop dispatches the command to the right function through the
 * commands table.   ArgLists are essentially like RPC things, only they don't
 * require going through RPCgen, and it's easy to get variable length lists.
 * They are just lists of longs, strings, characters, and byte arrays that can
 * be converted to a machine independent format
 */

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <gio/gio.h>
#include <gio/gnetworking.h>

#include "client.h"
#include "external.h"
#include "mapdata.h"
#include "metaserver.h"
#include "script.h"

/* actually declare the globals */

char VERSION_INFO[MAX_BUF];

char *skill_names[MAX_SKILL];
char *sound_server = "cfsndserv";
const char *config_dir;
const char *cache_dir;

int last_used_skills[MAX_SKILL+1];

int want_skill_exp = 0, replyinfo_status = 0, requestinfo_sent = 0,
        replyinfo_last_face = 0, maxfd,
        wantloginmethod = 0, serverloginmethod = 0;

guint32 tick=0;

guint16 exp_table_max=0;
guint64 *exp_table=NULL;

NameMapping skill_mapping[MAX_SKILL], resist_mapping[NUM_RESISTS];

Client_Player cpl;
ClientSocket csocket;
static GSocketConnection *connection;
static GInputStream *in;
static GOutputStream *out;

/** Timer to track when the last message was sent to the server. */
static GTimer *beat_timer;

/** Interval between client beats, zero means that beating is disabled. */
int beat_interval = 0;

const char *const resists_name[NUM_RESISTS] = {
    "armor", "magic", "fire", "elec",
    "cold", "conf", "acid", "drain",
    "ghit", "pois", "slow", "para",
    "t undead", "fear", "depl","death",
    "hword", "blind"
};

typedef void (*CmdProc)(unsigned char *, int len);

/**
 * Links server commands to client functions that implement them, and gives a
 * rough indication of the type of data that the server supplies with the
 * command.
 */
struct CmdMapping {
    const char *cmdname;
    void (*cmdproc)(unsigned char *, int );
    enum CmdFormat cmdformat;
};

/**
 * The list of server commands that this client supports along with pointers
 * to the function that handles the command.  The table also gives a rough
 * indication of the type of data that the server should send with each
 * command.  If the client receives a command not listed in the table, a
 * complaint is output on stdout.
 */
struct CmdMapping commands[] = {
    /*
     * The order of this table does not make much of a difference.  Related
     * commands are listed in groups.
     */
    { "map2",            Map2Cmd, SHORT_ARRAY },
    { "map_scroll",      (CmdProc)map_scrollCmd, ASCII },
    { "magicmap",        MagicMapCmd, MIXED    /* ASCII, then binary */},
    { "newmap",          NewmapCmd, NODATA },
    { "mapextended",     MapExtendedCmd, MIXED /* chars, then SHORT_ARRAY */ },

    { "item2",           Item2Cmd, MIXED },
    { "upditem",         UpdateItemCmd, MIXED },
    { "delitem",         DeleteItem, INT_ARRAY },
    { "delinv",          DeleteInventory, ASCII },

    { "addspell",        AddspellCmd, MIXED },
    { "updspell",        UpdspellCmd, MIXED },
    { "delspell",        DeleteSpell, INT_ARRAY },

    { "drawinfo",        (CmdProc)DrawInfoCmd, ASCII },
    { "drawextinfo",     (CmdProc)DrawExtInfoCmd, ASCII},
    {
        "stats",           StatsCmd, STATS       /* Array of: int8, (int?s for
                                                * that stat)
                                                */
    },
    { "image2",          Image2Cmd, MIXED      /* int, int8, int, PNG */ },
    {
        "face2",           Face2Cmd, MIXED       /* int16, int8, int32, string
                                                */
    },
    { "tick",            TickCmd, INT_ARRAY    /* uint32 */},

    { "music",           (CmdProc)MusicCmd, ASCII },
    {
        "sound2",          Sound2Cmd, MIXED      /* int8, int8, int8,  int8,
                                                * int8, int8, chars, int8,
                                                * chars
                                                */
    },
    { "anim",            AnimCmd, SHORT_ARRAY},
    { "smooth",          SmoothCmd, SHORT_ARRAY},

    { "player",          PlayerCmd, MIXED      /* 3 ints, int8, str */ },
    { "comc",            CompleteCmd, SHORT_INT },

    { "addme_failed",    (CmdProc)AddMeFail, NODATA },
    { "addme_success",   (CmdProc)AddMeSuccess, NODATA },
    { "version",         (CmdProc)VersionCmd, ASCII },
    { "goodbye",         (CmdProc)GoodbyeCmd, NODATA },
    { "setup",           (CmdProc)SetupCmd, ASCII},
    { "failure",         (CmdProc)FailureCmd, ASCII},
    { "accountplayers",  (CmdProc)AccountPlayersCmd, ASCII},

    { "query",           (CmdProc)handle_query, ASCII},
    { "replyinfo",       ReplyInfoCmd, ASCII},
    { "ExtendedTextSet", (CmdProc)SinkCmd, NODATA},
    { "ExtendedInfoSet", (CmdProc)SinkCmd, NODATA},

    { "pickup",          PickupCmd, INT_ARRAY  /* uint32 */},
};

/**
 * The number of entries in #commands.
 */
#define NCOMMANDS ((int)(sizeof(commands)/sizeof(struct CmdMapping)))

void client_mapsize(int width, int height) {
    // Store desired size in use_config to check results from the server.
    use_config[CONFIG_MAPWIDTH] = width;
    use_config[CONFIG_MAPHEIGHT] = height;

    // Set map size in case we receive 'map' before 'setup' commands.
    mapdata_set_size(width, height);
    cs_print_string(csocket.fd, "setup mapsize %dx%d", width, height);
}

void client_disconnect() {
    LOG(LOG_DEBUG, "close_server_connection", "Closing server connection");
    csocket.fd = -1;
    g_io_stream_close(G_IO_STREAM(connection), NULL, NULL);
    g_object_unref(connection);
}

void client_run() {
    int i, len;
    unsigned char *data;

    while (1) {
        i = SockList_ReadPacket(csocket.fd, &csocket.inbuf, MAXSOCKBUF - 1);
        /*
         * If a socket error occurred while reading the packet, drop the
         * server connection.  Is there a better way to handle this?
         */
        if (i == -1) {
            client_disconnect();
            return;
        }
        /*
         * Drop incomplete packets without attempting to process the contents.
         */
        if (i == 0) {
            return;
        }
        /*
         * Null-terminate the buffer, and set the data pointer so it points
         * to the first character of the data (following the packet length).
         */
        csocket.inbuf.buf[csocket.inbuf.len] = '\0';
        data = csocket.inbuf.buf + 2;
        /*
         * Commands that provide data are always followed by a space.  Find
         * the space and convert it to a null character.  If no spaces are
         * found, the packet contains a command with no associatd data.
         */
        while ((*data != ' ') && (*data != '\0')) {
            ++data;
        }
        if (*data == ' ') {
            *data = '\0';
            data++;
            len = csocket.inbuf.len - (data - csocket.inbuf.buf);
        } else {
            len = 0;
        }
        /*
         * Search for the command in the list of supported server commands.
         * If the server command is supported by the client, let the script
         * watcher know what command was received, then process it and quit
         * searching the command list.
         */
        for(i = 0; i < NCOMMANDS; i++) {
            const char *cmdin = (char *)csocket.inbuf.buf + 2;
            if (strcmp(cmdin, commands[i].cmdname) == 0) {
                script_watch(cmdin, data, len, commands[i].cmdformat);
                commands[i].cmdproc(data, len);
                break;
            }
        }
        /*
         * After processing the command, mark the socket input buffer empty.
         */
        csocket.inbuf.len=0;
        /*
         * Complain about unsupported commands to facilitate troubleshooting.
         * The client and server should negotiate a connection such that the
         * server does not send commands the client does not support.
         */
        if (i == NCOMMANDS) {
            printf("Unrecognized command from server (%s)\n",
                   csocket.inbuf.buf+2);
        }
    }
}

int client_connect(const char *hostname) {
    GSocketClient *sclient = g_socket_client_new();
    g_socket_client_set_timeout(sclient, 10);

    // Store server hostname.
    if (csocket.servername != NULL) {
        g_free(csocket.servername);
    }
    csocket.servername = g_strdup(hostname);

    // Try to connect to server.
    connection = g_socket_client_connect_to_host(
            sclient, hostname, use_config[CONFIG_PORT], NULL, NULL);
    g_object_unref(sclient);
    if (connection == NULL) {
        return -1;
    }

    GSocket *socket = g_socket_connection_get_socket(connection);
    int i = 1, fd = g_socket_get_fd(socket);
    if (use_config[CONFIG_FASTTCP]) {
        if (setsockopt(fd, SOL_TCP, TCP_NODELAY, &i, sizeof(i)) == -1) {
            perror("TCP_NODELAY");
        }
    }
    in = g_io_stream_get_input_stream(G_IO_STREAM(connection));
    out = g_io_stream_get_output_stream(G_IO_STREAM(connection));
    return fd;
}

bool client_write(const void *buf, int len) {
    return g_output_stream_write_all(out, buf, len, NULL, NULL, NULL);
}

bool client_is_connected() {
    return connection != NULL && g_socket_connection_is_connected(connection);
}

GSource *client_get_source() {
    return g_pollable_input_stream_create_source(
            G_POLLABLE_INPUT_STREAM(in), NULL);
}

void client_negotiate(int sound) {
    int tries;

    SendVersion(csocket);

    /* We need to get the version command fairly early on because we need to
     * know if the server will support a request to use png images.  This
     * isn't done the best, because if the server never sends the version
     * command, we can loop here forever.  However, if it doesn't send the
     * version command, we have no idea what we are dealing with.
     */
    tries=0;
    while (csocket.cs_version==0) {
        client_run();
        if (csocket.fd == -1) {
            return;
        }

        usleep(10*1000);    /* 10 milliseconds */
        tries++;
        /* If we haven't got a response in 10 seconds, bail out */
        if (tries > 1000) {
            LOG (LOG_ERROR,"common::negotiate_connection", "Connection timed out");
            client_disconnect();
            return;
        }
    }

    if (csocket.sc_version<1023) {
        LOG (LOG_WARNING,"common::negotiate_connection","Server does not support PNG images, yet that is all this client");
        LOG (LOG_WARNING,"common::negotiate_connection","supports.  Either the server needs to be upgraded, or you need to");
        LOG (LOG_WARNING,"common::negotiate_connection","downgrade your client.");
        exit(1);
    }

    /* If the user has specified a numeric face id, use it. If it is a string
     * like base, then that resolves to 0, so no real harm in that.
     */
    if (face_info.want_faceset) {
        face_info.faceset = atoi(face_info.want_faceset);
    }

    /* For sound, a value following determines which sound features are
     * wanted.  The value is 1 for sound effects, and 2 for background music,
     * or the sum of 1 + 2 (3) for both.
     *
     * For spellmon, try each acceptable level, but make sure the one the
     * client prefers is last.
     */
    cs_print_string(csocket.fd, "setup "
            "map2cmd 1 tick 1 sound2 %d darkness %d spellmon 1 spellmon 2 "
            "faceset %d facecache %d want_pickup 1 loginmethod %d newmapcmd 1 "
            "beat 1",
            (sound >= 0) ? 3 : 0, want_config[CONFIG_LIGHTING] ? 1 : 0,
            face_info.faceset, want_config[CONFIG_CACHE], wantloginmethod);

    /*
     * We can do this right now also.  There is not any reason to wait.
     */
    cs_print_string(csocket.fd, "requestinfo skill_info");
    cs_print_string(csocket.fd,"requestinfo exp_table");
    /*
     * While these are only used for new login method, they should become
     * standard fairly soon.  All of these are pretty small, and do not add
     * much to the cost.  They make it more likely that the information is
     * ready when the window that needs it is raised.
     */
    cs_print_string(csocket.fd,"requestinfo motd");
    cs_print_string(csocket.fd,"requestinfo news");
    cs_print_string(csocket.fd,"requestinfo rules");

    client_mapsize(want_config[CONFIG_MAPWIDTH], want_config[CONFIG_MAPHEIGHT]);
    use_config[CONFIG_SMOOTH]=want_config[CONFIG_SMOOTH];

    /* If the server will answer the requestinfo for image_info and image_data,
     * send it and wait for the response.
     */
    if (csocket.sc_version >= 1027) {
        /* last_start is -99.  This means the first face requested will be 1
         * (not 0) - this is OK because 0 is defined as the blank face.
         */
        int last_end=0, last_start=-99;

        cs_print_string(csocket.fd,"requestinfo image_info");
        requestinfo_sent = RI_IMAGE_INFO;
        replyinfo_status = 0;
        replyinfo_last_face = 0;

        do {
            client_run();
            /*
             * It is rare, but the connection can die while getting this info.
             */
            if (csocket.fd == -1) {
                return;
            }

            if (use_config[CONFIG_DOWNLOAD]) {
                /*
                 * We need to know how many faces to be able to make the
                 * request intelligently.  So only do the following block if
                 * we have that info.  By setting the sent flag, we will never
                 * exit this loop until that happens.
                 */
                requestinfo_sent |= RI_IMAGE_SUMS;
                if (face_info.num_images != 0) {
                    /*
                     * Sort of fake things out - if we have sent the request
                     * for image sums but have not got them all answered yet,
                     * we then clear the bit from the status so we continue to
                     * loop.
                     */
                    if (last_end == face_info.num_images) {
                        /* Mark that we're all done */
                        if (replyinfo_last_face == last_end) {
                            replyinfo_status |= RI_IMAGE_SUMS;
                            image_update_download_status(face_info.num_images, face_info.num_images, face_info.num_images);
                        }
                    } else {
                        /*
                         * If we are all caught up, request another 100 sums.
                         */
                        if (last_end <= (replyinfo_last_face+100)) {
                            last_start += 100;
                            last_end += 100;
                            if (last_end > face_info.num_images) {
                                last_end = face_info.num_images;
                            }
                            cs_print_string(csocket.fd,"requestinfo image_sums %d %d", last_start, last_end);
                            image_update_download_status(last_start, last_end, face_info.num_images);
                        }
                    }
                } /* Still have image_sums request to send */
            } /* endif download all faces */

            usleep(10*1000);    /* 10 milliseconds */
            /*
             * Do not put in an upper time limit with tries like we did above.
             * If the player is downloading all the images, the time this
             * takes could be considerable.
             */
        } while (replyinfo_status != requestinfo_sent);
    }
    if (use_config[CONFIG_DOWNLOAD]) {
        char buf[MAX_BUF];

        snprintf(buf, sizeof(buf), "Download of images complete.  Found %d locally, downloaded %d from server\n",
                 face_info.cache_hits, face_info.cache_misses);
        draw_ext_info(NDI_GOLD, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_CONFIG, buf);
    }

    /* This needs to get changed around - we really don't want to send the
     * SendAddMe until we do all of our negotiation, which may include things
     * like downloading all the images and whatnot - this is more an issue if
     * the user is not using the default face set, as in that case, we might
     * end up building images from the wrong set.
     * Only run this if not using new login method
     */
    if (!serverloginmethod) {
        SendAddMe(csocket);
    }
}

/**
 * Initialize the heartbeat feature.
 *
 * @param interval The maximum interval between client messages.
 */
void beat_init(int interval) {
    beat_interval = interval;
    beat_timer = g_timer_new();
}

/**
 * Reset the beat timer, called after a message is sent to the server.
 */
void beat_reset() {
    if (beat_interval != 0) {
        g_timer_start(beat_timer);
    }
}

/**
 * Check to see if it is time to send a beat, and send it if necessary.
 */
void beat_check() {
    assert(csocket.fd != -1);

    if (beat_interval != 0) {
        double elapsed = g_timer_elapsed(beat_timer, NULL);

        if (elapsed > beat_interval) {
            LOG(LOG_DEBUG, "beat_check", "Sending beat!");
            cs_print_string(csocket.fd, "beat");
        }
    }
}
