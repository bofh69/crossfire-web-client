/*
 * Crossfire -- cooperative multi-player graphical RPG and adventure game
 *
 * Copyright (c) 1999-2014 Mark Wedel and the Crossfire Development Team
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
 * Deals with contacting the metaserver, getting a list of hosts, displaying
 * and returning them to calling function, and then connecting to the server
 * when requested.
 */

#include "client.h"

#ifdef HAVE_CURL_CURL_H
#include <curl/curl.h>
#include <curl/easy.h>
#endif

#include "metaserver.h"

/* list of metaserver URL to get information from - this should generally
 * correspond to the value in the metaserver2 server file, but instead
 * of meta_update.php, use meta_client.php.
 *
 * These could perhaps be in some other file (config.h or the like), but
 * it seems unlikely that these will change very often, and certainly not
 * at a level where we would expect users to go about changing the values.
 */
static const char *metaservers[] = {
    "http://crossfire.real-time.com/metaserver2/meta_client.php",
    "http://metaserver.eu.cross-fire.org/meta_client.php",
    "http://metaserver.us.cross-fire.org/meta_client.php",
};

/**
 * Memory buffer used in write_mbuf().
 */
struct mbuf {
    char *memory;
    size_t size;
};

/**
 * Curl write callback function for downloading data into memory instead of
 * a file. Copied from Curl's 'getinmemory.c' example under the MIT License.
 */
static size_t mbuf_write(void *contents, size_t size, size_t nmemb,
                         void *userp) {
    size_t realsize = size * nmemb;
    struct mbuf *mem = (struct mbuf *)userp;

    mem->memory = realloc(mem->memory, mem->size + realsize + 1);
    if (mem->memory == NULL) {
        /* out of memory! */
        printf("not enough memory (realloc returned NULL)\n");
        return 0;
    }

    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;
    return realsize;
}

/**
 * This checks the servers sc_version and cs_version to see
 * if they are compatible.
 * @param entry
 * entry number in the metaservers array to check.
 * @return
 * 1 if this entry is compatible, 0 if it is not.  Note that this can
 * only meaningfully check metaserver2 data - metaserver1 doesn't
 * include protocol version number, so treats all of those as
 * OK.
 */

static bool ms_check_version(Meta_Info *server) {
    /* No version information - nothing to do. */
    if (!server->sc_version || !server->cs_version) {
        return true;
    }

    if (server->sc_version != VERSION_SC) {
        /* 1027->1028 removed a bunch of old commands, so a 1028
         * version client can still play on a 1027 server, so
         * special hard code that.
         *
         * Likewise, 1028->1029 just changed how weapon_speed
         * should be interperted on the client - the client
         * does the right thing, so not problem with a 1029
         * client playing on 1028 or 1027 server.
         *
         * A 1028 client could in practice play on a 1029
         * server, since at the protocol level, data is the same -
         * the client would just have screwed up weapon_sp values.
         */
        if ((VERSION_SC == 1028 || VERSION_SC == 1029) &&
                (server->sc_version == 1027 ||
                server->sc_version == 1028)) {
            return true;
        }
    }
    if (server->cs_version != VERSION_CS) {
        return false;
    }

    return true;
}

static void parse_meta(char inbuf[static 1], ms_callback callback) {
    Meta_Info metaserver;
    char *newline;
    for (char *cp = inbuf; cp != NULL && *cp != 0; cp = newline) {
        newline = strchr(cp, '\n');
        if (newline) {
            *newline = 0;
            newline++;
        } else {
            /* If we didn't get a newline, then this is the
             * end of the block of data for this call - store
             * away the extra for the next call.
             */
            // Can no longer happen after removing ms_writer().
            return;
        }

        char *eq = strchr(cp, '=');
        if (eq) {
            *eq = 0;
            eq++;
        }

        if (!strcmp(cp, "START_SERVER_DATA")) {
            /* Clear out all data - MS2 doesn't necessarily use all the
             * fields, so blank out any that we are not using.
             */
            memset(&metaserver, 0, sizeof (Meta_Info));
        } else if (!strcmp(cp, "END_SERVER_DATA")) {
            char buf[MS_LARGE_BUF];

            // If server is running custom port, add it to server name.
            if (metaserver.port != EPORT) {
                snprintf(buf, sizeof(buf), "%s:%d",
                        metaserver.hostname, metaserver.port);
            } else {
                snprintf(buf, sizeof(buf), "%s", metaserver.hostname);
            }

            callback(buf, metaserver.idle_time, metaserver.num_players,
                    metaserver.version, metaserver.text_comment,
                    ms_check_version(&metaserver));
        } else {
            /* If we get here, these should be variable=value pairs.
             * if we don't have a value, can't do anything, and
             * report an error.  This would normally be incorrect
             * data from the server.
             */
            if (!eq) {
                LOG(LOG_ERROR, "common::metaserver2_writer", "Unknown line: %s", cp);
                continue;
            }
            if (!strcmp(cp, "hostname")) {
                strncpy(metaserver.hostname, eq, sizeof (metaserver.hostname));
            } else if (!strcmp(cp, "port")) {
                metaserver.port = atoi(eq);
            } else if (!strcmp(cp, "html_comment")) {
                strncpy(metaserver.html_comment, eq, sizeof (metaserver.html_comment));
            } else if (!strcmp(cp, "text_comment")) {
                strncpy(metaserver.text_comment, eq, sizeof (metaserver.text_comment));
            } else if (!strcmp(cp, "archbase")) {
                strncpy(metaserver.archbase, eq, sizeof (metaserver.archbase));
            } else if (!strcmp(cp, "mapbase")) {
                strncpy(metaserver.mapbase, eq, sizeof (metaserver.mapbase));
            } else if (!strcmp(cp, "codebase")) {
                strncpy(metaserver.codebase, eq, sizeof (metaserver.codebase));
            } else if (!strcmp(cp, "flags")) {
                strncpy(metaserver.flags, eq, sizeof (metaserver.flags));
            } else if (!strcmp(cp, "version")) {
                strncpy(metaserver.version, eq, sizeof (metaserver.version));
            } else if (!strcmp(cp, "num_players")) {
                metaserver.num_players = atoi(eq);
            } else if (!strcmp(cp, "in_bytes")) {
                metaserver.in_bytes = atoi(eq);
            } else if (!strcmp(cp, "out_bytes")) {
                metaserver.out_bytes = atoi(eq);
            } else if (!strcmp(cp, "uptime")) {
                metaserver.uptime = atoi(eq);
            } else if (!strcmp(cp, "sc_version")) {
                metaserver.sc_version = atoi(eq);
            } else if (!strcmp(cp, "cs_version")) {
                metaserver.cs_version = atoi(eq);
            } else if (!strcmp(cp, "last_update")) {
                /* MS2 reports update time as when it last got an update,
                 * where as we want actual elapsed time since last update.
                 * So do the conversion.  Second check is because of clock
                 * skew - my clock may be fast, and we don't want negative times.
                 */
                metaserver.idle_time = time(NULL) - atoi(eq);
                if (metaserver.idle_time < 0) {
                    metaserver.idle_time = 0;
                }
            } else {
                LOG(LOG_ERROR, "common::metaserver2_writer", "Unknown line: %s=%s", cp, eq);
            }
        }
    }
}

/**
 * Fetch a list of servers from the given URL.
 *
 * @param metaserver2
 * @return
 */
static bool ms_fetch_server(const char *metaserver2, ms_callback callback) {
#ifdef HAVE_CURL_CURL_H
    CURL *curl = curl_easy_init();
    if (!curl) {
        return false;
    }

    struct mbuf chunk;
    chunk.memory = g_malloc(1);
    chunk.size = 0;

    curl_easy_setopt(curl, CURLOPT_URL, metaserver2);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, mbuf_write);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (!res) {
        parse_meta(chunk.memory, callback);
    }
    g_free(chunk.memory);
    return !res;
#else
    return true;
#endif
}

/**
 * Fetch a list of servers from the built-in official metaservers.
 *
 * Because this function can query multiple metaservers, the same servers may
 * be listed multiple times in the results.
 */
void ms_fetch(ms_callback callback) {
    for (size_t i = 0; i < sizeof(metaservers) / sizeof(char *); i++) {
        ms_fetch_server(metaservers[i], callback);
    }
}

/**
 * Initialize metaserver client. This function should be called before any
 * other metaserver functions.
 */
void ms_init() {
#ifdef HAVE_CURL_CURL_H
    curl_global_init(CURL_GLOBAL_ALL);
#endif
}
