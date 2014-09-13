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
 * Deals with contacting the metaserver, getting a list of hosts, displaying
 * and returning them to calling function, and then connecting to the server
 * when requested.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include "client.h"
#include "external.h"
#include "metaserver.h"

#ifdef HAVE_CURL_CURL_H
#include <curl/curl.h>
#include <curl/easy.h>
#endif

Meta_Info *meta_servers = NULL;

int meta_numservers = 0;

int meta_sort(Meta_Info *m1, Meta_Info *m2) {
    return g_ascii_strcasecmp(m1->hostname, m2->hostname);
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
int metaserver_check_version(int entry) {
    /* No version information - nothing to do. */
    if (!meta_servers[entry].sc_version || !meta_servers[entry].cs_version) {
        return 1;
    }

    if (meta_servers[entry].sc_version != VERSION_SC) {
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
                (meta_servers[entry].sc_version == 1027 ||
                meta_servers[entry].sc_version == 1028)) {
            return 1;
        }
    }
    if (meta_servers[entry].cs_version != VERSION_CS) {
        return 0;
    }

    return 1;
}

/*****************************************************************************
 * Start of cache related functions.
 *****************************************************************************/
int cached_servers_num = 0;
char *cached_servers_name[CACHED_SERVERS_MAX];
char *cached_servers_ip[CACHED_SERVERS_MAX];
static int cached_servers_loaded = 0;
const char *cached_server_file = NULL;

/**
 * Load server names and addresses or DNS names from a cache file found in the
 * player's client data folder.  The cache file has traditionally been named
 * "servers.cache".  The server cache file is a plain text file that is
 * line-feed delimited.  Cache entries consist of two lines each and if the
 * file has an odd number of lines, the last entry is ignored.  The first
 * line of a cache entry is the name of the server, and the second line is an
 * IP address or DNS hostname.  Metaserver uses both entries.  Metaserver 2
 * uses only the name since most servers set the name to a hostname anyway.
 * The load function does no parsing, so the entries must be in the correct
 * order.  There is no mechanism to support comments.  If a file has an odd
 * number of lines, the loader assumes the last line is an incomplete entry
 * and silently discards it.
 */
static void metaserver_load_cache(void) {
    char name[MS_LARGE_BUF], ip[MS_LARGE_BUF];
    FILE *cache;

    if (cached_servers_loaded || !cached_server_file) {
        return;
    }

    /* If failure, we don't want to load again */
    cached_servers_loaded = 1;
    cached_servers_num = 0;

    cache = fopen(cached_server_file, "r");
    if (!cache) {
        return;
    }

    while (cached_servers_num < CACHED_SERVERS_MAX
            && fgets(name, MS_LARGE_BUF, cache) != NULL
            && fgets(ip, MS_LARGE_BUF, cache) != NULL) {
        ip[strlen(ip) - 1] = 0;
        name[strlen(name) - 1] = 0;
        cached_servers_ip[cached_servers_num] = g_strdup(ip);
        cached_servers_name[cached_servers_num++] = g_strdup(name);
    }
    fclose(cache);
}

/**
 *
 */
static void metaserver_save_cache(void) {
    FILE *cache;
    int server;

    if (!cached_server_file) {
        return;
    }

    cache = fopen(cached_server_file, "w");
    if (!cache) {
        return;
    }

    for (server = 0; server < cached_servers_num; server++) {
        fprintf(cache, "%s\n", cached_servers_name[server]);
        fprintf(cache, "%s\n", cached_servers_ip[server]);
    }
    fclose(cache);
}

/**
 * Add a server to the players server cache file.
 * @param server_name
 * @param server_ip
 */
void metaserver_update_cache(const char *server_name, const char *server_ip) {
    int index;

    /*
     * Try to find the given server name in the existing server cache.  If the
     * zero-based index ends up equal to the one-based number of cached
     * servers, it was not found.
     */
    for (index = 0; index < cached_servers_num; index++) {
        if (strcmp(server_name, cached_servers_name[index]) == 0) {
            break;
        }
    }

    /*
     * If server is already first in the cache list, nothing else needs to be
     * done, otherwise, the server needs to be cached.
     */
    if (index != 0 || !cached_servers_num) {
        char *name;
        char *ip;
        int copy;

        if (index == cached_servers_num) {
            /*
             * If the server was not found in the cache, expand the cache size
             * by one unless that creates too many entries.
             */
            name = g_strdup(server_name);
            ip = g_strdup(server_ip);
            cached_servers_num++;
            if (cached_servers_num > CACHED_SERVERS_MAX) {
                cached_servers_num--;
                free(cached_servers_name[cached_servers_num - 1]);
                free(cached_servers_ip[cached_servers_num - 1]);
            }
        } else {
            /*
             * If the server was already listed in the cache, grab a copy of
             * the prior listing.
             */
            name = cached_servers_name[index];
            ip = cached_servers_ip[index];
        }

        /*
         * If the server as already listed, move all the cached items above
         * the listing down a slot, otherwise, move the whole list down a
         * notch.  This "empties" the top slot.
         */
        for (copy = MIN(index, CACHED_SERVERS_MAX - 1); copy > 0; copy--) {
            cached_servers_name[copy] = cached_servers_name[copy - 1];
            cached_servers_ip[copy] = cached_servers_ip[copy - 1];
        }

        /*
         * Put the added server information at the top of the cache list, and
         * save the changes.
         */
        cached_servers_name[0] = name;
        cached_servers_ip[0] = ip;
        metaserver_save_cache();
    }
}

/*****************************************************************************
 * End of cache related functions.
 *****************************************************************************/

/******************************************************************************
 * Metaserver2 support starts here.
 *
 ******************************************************************************/

pthread_mutex_t ms2_info_mutex;

/* we use threads so that the GUI keeps responding while we wait for
 * data.  But we need to note if the thread is running or not,
 * so we store it here.  This, like the other metaserver2 data,
 * should be protected by using the ms2_info_mutext.
 */
static int ms2_is_running = 0;

/* list of metaserver URL to get information from - this should generally
 * correspond to the value in the metaserver2 server file, but instead
 * of meta_update.php, use meta_client.php.
 *
 * These could perhaps be in some other file (config.h or the like), but
 * it seems unlikely that these will change very often, and certainly not
 * at a level where we would expect users to go about changing the values.
 */
static char *metaservers[] = {"http://crossfire.real-time.com/metaserver2/meta_client.php"};

/**
 * Curl doesn't really have any built in way to get data
 * from the URL into string data - instead, we get a blob
 * of data which we need to find the newlines, etc
 * from.  Curl also provides the data in multiple calls
 * if there is lots of data, and does not break the data on
 * newline, so we need to store the extra (unprocessed) data
 * from one call to the next.
 *
 * @param ptr
 * pointer to data to process.
 * @param size
 * @param nmemb
 * the size of each piece of data, and the number of these elements.
 * We always presume the data is byte sized, and just multiple these
 * together to get total amount of data.
 * @param data
 * user supplied data pointer - in this case, it points to a buffer
 * which is used to store unprocessed information from one call to the
 * next.
 * @return
 * Number of bytes processed.  We always return the total number of
 * bytes supplied - returning anything else is an error to CURL
 */
static size_t metaserver2_writer(void *ptr, size_t size, size_t nmemb, void *data) {
#ifdef HAVE_CURL_CURL_H
    size_t realsize = size * nmemb;
    char *cp, *newline, *eq, inbuf[CURL_MAX_WRITE_SIZE * 2 + 1], *leftover;

    leftover = (char*) data;

    if (realsize > CURL_MAX_WRITE_SIZE) {
        LOG(LOG_CRITICAL, "common::metaserver2_writer", "Function called with more data than allowed!");
    }

    /* This memcpy here is to just give us a null terminated character
     * array - easier to do with than having to check lengths as well as other
     * values.  Also, it makes it easier to deal with unprocessed data from
     * the last call.
     */
    memcpy(inbuf, leftover, strlen(leftover));
    memcpy(inbuf + strlen(leftover), ptr, realsize);
    inbuf[strlen(leftover) + realsize] = 0;
    leftover[0] = 0;

    /* Processing this block of data shouldn't take very long, even on
     * slow machines, so putting the lock here, instead of each time
     * we update a variable is cleaner
     */
    pthread_mutex_lock(&ms2_info_mutex);

    for (cp = inbuf; cp != NULL && *cp != 0; cp = newline) {
        newline = strchr(cp, '\n');
        if (newline) {
            *newline = 0;
            newline++;
        } else {
            /* If we didn't get a newline, then this is the
             * end of the block of data for this call - store
             * away the extra for the next call.
             */
            strncpy(leftover, cp, CURL_MAX_WRITE_SIZE - 1);
            leftover[CURL_MAX_WRITE_SIZE - 1] = 0;
            break;
        }

        eq = strchr(cp, '=');
        if (eq) {
            *eq = 0;
            eq++;
        }

        if (!strcmp(cp, "START_SERVER_DATA")) {
            /* Clear out all data - MS2 doesn't necessarily use all the
             * fields, so blank out any that we are not using.
             */
            memset(&meta_servers[meta_numservers], 0, sizeof (Meta_Info));
        } else if (!strcmp(cp, "END_SERVER_DATA")) {
            int i;

            /* we can get data from both metaserver1 & 2 - no reason to keep
             * both.  So check for duplicates, and consider metaserver2
             * data 'better'.
             */
            for (i = 0; i < meta_numservers; i++) {
                if (!g_ascii_strcasecmp(meta_servers[i].hostname, meta_servers[meta_numservers].hostname)) {
                    memcpy(&meta_servers[i], &meta_servers[meta_numservers], sizeof (Meta_Info));
                    break;
                }
            }
            if (i >= meta_numservers) {
                meta_numservers++;
            }
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
                strncpy(meta_servers[meta_numservers].hostname, eq, sizeof (meta_servers[meta_numservers].hostname));
            } else if (!strcmp(cp, "port")) {
                meta_servers[meta_numservers].port = atoi(eq);
            } else if (!strcmp(cp, "html_comment")) {
                strncpy(meta_servers[meta_numservers].html_comment, eq, sizeof (meta_servers[meta_numservers].html_comment));
            } else if (!strcmp(cp, "text_comment")) {
                strncpy(meta_servers[meta_numservers].text_comment, eq, sizeof (meta_servers[meta_numservers].text_comment));
            } else if (!strcmp(cp, "archbase")) {
                strncpy(meta_servers[meta_numservers].archbase, eq, sizeof (meta_servers[meta_numservers].archbase));
            } else if (!strcmp(cp, "mapbase")) {
                strncpy(meta_servers[meta_numservers].mapbase, eq, sizeof (meta_servers[meta_numservers].mapbase));
            } else if (!strcmp(cp, "codebase")) {
                strncpy(meta_servers[meta_numservers].codebase, eq, sizeof (meta_servers[meta_numservers].codebase));
            } else if (!strcmp(cp, "flags")) {
                strncpy(meta_servers[meta_numservers].flags, eq, sizeof (meta_servers[meta_numservers].flags));
            } else if (!strcmp(cp, "version")) {
                strncpy(meta_servers[meta_numservers].version, eq, sizeof (meta_servers[meta_numservers].version));
            } else if (!strcmp(cp, "num_players")) {
                meta_servers[meta_numservers].num_players = atoi(eq);
            } else if (!strcmp(cp, "in_bytes")) {
                meta_servers[meta_numservers].in_bytes = atoi(eq);
            } else if (!strcmp(cp, "out_bytes")) {
                meta_servers[meta_numservers].out_bytes = atoi(eq);
            } else if (!strcmp(cp, "uptime")) {
                meta_servers[meta_numservers].uptime = atoi(eq);
            } else if (!strcmp(cp, "sc_version")) {
                meta_servers[meta_numservers].sc_version = atoi(eq);
            } else if (!strcmp(cp, "cs_version")) {
                meta_servers[meta_numservers].cs_version = atoi(eq);
            } else if (!strcmp(cp, "last_update")) {
                /* MS2 reports update time as when it last got an update,
                 * where as we want actual elapsed time since last update.
                 * So do the conversion.  Second check is because of clock
                 * skew - my clock may be fast, and we don't want negative times.
                 */
                meta_servers[meta_numservers].idle_time = time(NULL) - atoi(eq);
                if (meta_servers[meta_numservers].idle_time < 0) {
                    meta_servers[meta_numservers].idle_time = 0;
                }
            } else {
                LOG(LOG_ERROR, "common::metaserver2_writer", "Unknown line: %s=%s", cp, eq);
            }
        }
    }
    pthread_mutex_unlock(&ms2_info_mutex);
    return realsize;
#else
    return 0;
#endif
}

/**
 * Connects to the URL and gets metaserver data.
 * @param metaserver2
 * metaserver to connect to.
 * @return
 * TRUE if successfull, false is failed for some
 * reason.
 */
static int get_metaserver2_data(char *metaserver2) {
#ifdef HAVE_CURL_CURL_H
    CURL *curl;
    CURLcode res;
    char leftover[CURL_MAX_WRITE_SIZE];

    curl = curl_easy_init();
    if (!curl) {
        return 0;
    }
    leftover[0] = 0;
    curl_easy_setopt(curl, CURLOPT_URL, metaserver2);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, metaserver2_writer);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, leftover);
    res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res) {
        return 0;
    } else {
        return 1;
    }
#else
    return 1;
#endif
}

/**
 * Thread function that goes off and collects metaserver
 * data.
 * @return
 * exits when job is done, no return value.
 */
static void *metaserver2_thread(void *junk) {
    int metaserver_choice, tries = 0;

    do {
        metaserver_choice = g_random_int() % (sizeof (metaservers) / sizeof (char*));
        tries++;
        if (tries > 5) {
            break;
        }
    } while (!get_metaserver2_data(metaservers[metaserver_choice]));

    pthread_mutex_lock(&ms2_info_mutex);
    qsort(meta_servers, meta_numservers, sizeof (Meta_Info), (int (*)(const void *, const void *))meta_sort);
    ms2_is_running = 0;
    pthread_mutex_unlock(&ms2_info_mutex);
    pthread_exit(NULL);
    // never reached, just to make the compiler happy.
    return NULL;
}

/**
 * Contact the official metaserver for a list of public servers.
 */
int metaserver_get() {
    pthread_t thread_id;
    int ret;

    meta_numservers = 0;

    if (!metaserver2_on) {
        return 0;
    }
#ifndef HAVE_CURL_CURL_H
    return 0;
#endif

    metaserver_load_cache();

    pthread_mutex_lock(&ms2_info_mutex);
    if (!meta_servers) {
        meta_servers = calloc(MAX_METASERVER, sizeof (Meta_Info));
    }

    ms2_is_running = 1;
    pthread_mutex_unlock(&ms2_info_mutex);

    ret = pthread_create(&thread_id, NULL, metaserver2_thread, NULL);
    if (ret) {
        LOG(LOG_ERROR, "common::metaserver2_get_info", "Thread creation failed.");
        pthread_mutex_lock(&ms2_info_mutex);
        ms2_is_running = 0;
        pthread_mutex_unlock(&ms2_info_mutex);
    }

    return 0;
}

/**
 * Does single use initalization of metaserver2 variables.
 */
void metaserver_init(void) {
    pthread_mutex_init(&ms2_info_mutex, NULL);
#ifdef HAVE_CURL_CURL_H
    curl_global_init(CURL_GLOBAL_ALL);
#endif
}

/******************************************************************************
 * End of Metasever2 functions.
 ******************************************************************************/

/******************************************************************************
 * This is start of common logic - the above sections are actually getting
 * the data.  The code below here is just displaying the data we got
 */

/**
 * Sees if we are gathering data or not.  Note that we don't have to check
 * to see what update methods are being used - the is_running flag
 * is initialized to zero no matter if we are using that method to get
 * the data, and unless we are using ms1 or ms2, the is_running flag
 * will never get changed to be non-zero.
 *
 * @return
 * Returns 1 if if we are getting data, 0 if nothing is going on right now.
 */
int metaserver_check_status() {
    int status;

    pthread_mutex_lock(&ms2_info_mutex);
    status = ms2_is_running;
    pthread_mutex_unlock(&ms2_info_mutex);

    return status;
}