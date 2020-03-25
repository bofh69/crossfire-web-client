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
 * Handles various player related functions.  This includes both things that
 * operate on the player item, cpl structure, or various commands that the
 * player issues.
 *
 * Most of the handling of commands from the client to server (see commands.c
 * for server->client) is handled here.
 *
 * Most of the work for sending messages to the server is done here.  Again,
 * most of these appear self explanatory.  Most send a bunch of commands like
 * apply, examine, fire, run, etc.  This looks like it was done by Mark to
 * remove the old keypress stupidity I used.
 */

#include "client.h"
#include "external.h"
#include "script.h"

bool profile_latency = false;
int64_t *profile_time = NULL; /** 256-length array to keep track of when
                                commands were sent to the server */

/** Array for direction strings for each numeric direction. */
const char *const directions[] = {"stay",      "north",     "northeast",
                                  "east",      "southeast", "south",
                                  "southwest", "west",      "northwest"};

/**
 * Initialize player object using information from the server.
 */
void new_player(long tag, char *name, long weight, long face) {
    Spell *spell, *spnext;

    cpl.ob->tag = tag;
    cpl.ob->nrof = 1;
    copy_name(cpl.ob->d_name, name);

    /* Right after player exit server will send this with empty name. */
    if (strlen(name) != 0) {
        keybindings_init(name);
    }

    cpl.ob->weight = (float)weight / 1000;
    cpl.ob->face = face;

    if (cpl.spelldata) {
        for (spell = cpl.spelldata; spell; spell = spnext) {
            spnext = spell->next;
            free(spell);
        }
        cpl.spelldata = NULL;
    }
}

void look_at(int x, int y) {
    cs_print_string(csocket.fd, "lookat %d %d", x, y);
}

void client_send_apply(int tag) {
    cs_print_string(csocket.fd, "apply %d", tag);
}

void client_send_examine(int tag) {
    cs_print_string(csocket.fd, "examine %d", tag);
}

/**
 * Request to move 'nrof' objects with 'tag' to 'loc'.
 */
void client_send_move(int loc, int tag, int nrof) {
    cs_print_string(csocket.fd, "move %d %d %d", loc, tag, nrof);
}

/* Fire & Run code.  The server handles repeating of these actions, so
 * we only need to send a run or fire command for a particular direction
 * once - we use the drun and dfire to keep track if we need to send
 * the full command.
 */
static int drun=-1, dfire=-1;

void stop_fire() {
    if (cpl.input_state != Playing) {
        return;
    }
    dfire |= 0x100;
}

void clear_fire() {
    if (dfire != -1) {
        send_command("fire_stop", -1, SC_FIRERUN);
        dfire = -1;
    }
}

void clear_run() {
    if (drun != -1) {
        send_command("run_stop", -1, SC_FIRERUN);
        drun = -1;
    }
}

void fire_dir(int dir) {
    if (cpl.input_state != Playing) {
        return;
    }
    if (dir != dfire) {
        char buf[MAX_BUF];
        snprintf(buf, sizeof(buf), "fire %d", dir);
        if (send_command(buf, cpl.count, SC_NORMAL)) {
            dfire = dir;
            cpl.count = 0;
        }
    } else {
        dfire &= 0xff; /* Mark it so that we need a stop_fire */
    }
}

void stop_run() {
    send_command("run_stop", -1, SC_FIRERUN);
    drun |= 0x100;
}

void run_dir(int dir) {
    if (dir != drun) {
        char buf[MAX_BUF];
        snprintf(buf, sizeof(buf), "run %d", dir);
        if (send_command(buf, -1, SC_NORMAL)) {
            drun = dir;
        }
    } else {
        drun &= 0xff;
    }
}

/* This should be used for all 'command' processing.  Other functions should
 * call this so that proper windowing will be done.
 * command is the text command, repeat is a count value, or -1 if none
 * is desired and we don't want to reset the current count.
 * must_send means we must send this command no matter what (ie, it is
 * an administrative type of command like fire_stop, and failure to send
 * it will cause definate problems
 * return 1 if command was sent, 0 if not sent.
 */
int send_command(const char *command, int repeat, int must_send) {
    static char last_command[MAX_BUF]="";

    script_monitor(command,repeat,must_send);
    if (cpl.input_state==Reply_One) {
        LOG(LOG_ERROR,"common::send_command","Wont send command '%s' - since in reply mode!",
            command);
        cpl.count=0;
        return 0;
    }

    /* Does the server understand 'ncom'? If so, special code */
    if (csocket.cs_version >= 1021) {
        int commdiff=csocket.command_sent - csocket.command_received;

        if (commdiff<0) {
            commdiff +=256;
        }

        /* if too many unanswered commands, not a must send, and command is
         * the same, drop it
         */
        if (commdiff>use_config[CONFIG_CWINDOW] && !must_send && !strcmp(command, last_command)) {
            if (repeat!=-1) {
                cpl.count=0;
            }
            return 0;
#if 0 /* Obnoxious warning message we don't need */
            fprintf(stderr,"Wont send command %s - window oversized %d %d\n",
                    command, csocket.command_sent, csocket.command_received);
#endif
        } else {
            SockList sl;
            guint8 buf[MAX_BUF];

            /* Don't want to copy in administrative commands */
            if (!must_send) {
                strcpy(last_command, command);
            }
            csocket.command_sent++;
            csocket.command_sent &= 0xff;   /* max out at 255 */

            SockList_Init(&sl, buf);
            SockList_AddString(&sl, "ncom ");
            SockList_AddShort(&sl, csocket.command_sent);
            SockList_AddInt(&sl, repeat);
            SockList_AddString(&sl, command);
            SockList_Send(&sl, csocket.fd);
            if (profile_latency) {
                if (profile_time == NULL) {
                    profile_time = calloc(256, sizeof(int64_t));
                }
                profile_time[csocket.command_sent] = g_get_monotonic_time();
                printf("profile/com\t%d\t%s\n", csocket.command_sent, command);
            }
        }
    } else {
        cs_print_string(csocket.fd, "command %d %s", repeat,command);
    }
    if (repeat!=-1) {
        cpl.count=0;
    }
    return 1;
}

void CompleteCmd(unsigned char *data, int len) {
    if (len !=6) {
        LOG(LOG_ERROR,"common::CompleteCmd","Invalid length %d - ignoring", len);
        return;
    }
    csocket.command_received = GetShort_String(data);
    csocket.command_time = GetInt_String(data+2);
    const int in_flight = csocket.command_sent - csocket.command_received;
    if (profile_latency) {
        gint64 now = g_get_monotonic_time();
        if (profile_time != NULL) {
            printf("profile/comc\t%d\t%" G_GINT64_FORMAT "\t%d\t%d\n",
                   csocket.command_received,
                   (now - profile_time[csocket.command_received])/1000,
                   csocket.command_time, in_flight);
        }
    }
    script_sync(in_flight);
}

/* This does special processing on the 'take' command.  If the
 * player has a container open, we want to specifiy what object
 * to move from that since we've sorted it.  command is
 * the command as tped, cpnext is any optional params.
 */
void command_take(const char *command, const char *cpnext) {
    /* If the player has specified optional data, or the player
     * does not have a container open, just issue the command
     * as normal
     */
    if (cpnext || cpl.container == NULL) {
        send_command(command, cpl.count, 0);
    } else {
        if (cpl.container->inv == NULL)
            draw_ext_info(NDI_BLACK, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_NOTICE,
                          "There is nothing in the container to move");
        else
            cs_print_string(csocket.fd,"move %d %d %d", cpl.ob->tag,
                            cpl.container->inv->tag, cpl.count);
    }
}
