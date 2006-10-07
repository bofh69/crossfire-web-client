const char *rcsid_common_metaserver_c =
    "$Id$";
/*
    Crossfire client, a client program for the crossfire program.

    Copyright (C) 2001 Mark Wedel & Crossfire Development Team

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    The author can be reached via e-mail to crossfire-devel@real-time.com
*/

/* This file deals with contact the metaserver, getting a list of hosts,
 * displaying/returning them to calling function, and then connecting
 * to the server when requested.
 */

#ifndef WIN32
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#else
#include <windows.h>
#endif /* WIN32 */
#include <ctype.h>
#include <stdio.h>

#include <client.h>
#include <cconfig.h>
#include <external.h>

#include <metaserver.h>

Meta_Info *meta_servers = NULL;

int meta_numservers = 0;

int cached_servers_num = 0;

char *cached_servers_name[CACHED_SERVERS_MAX];
char *cached_servers_ip[CACHED_SERVERS_MAX];
static int cached_servers_loaded = 0;

const char *cached_server_file = NULL;

static int meta_sort(Meta_Info *m1, Meta_Info *m2) { return strcasecmp(m1->hostname, m2->hostname); }

#ifdef WIN32
/* Need script.h for script_killall */
#include <script.h>

/* This gets input from a socket, and returns it one line at a time.
 */
/* This is a Windows-specific function, since you can't use fgets under Win32 */
char *get_line_from_sock(char *s, size_t n, int fd) {
    static long charsleft = 0;
    static char inbuf[MS_LARGE_BUF*4];
    char *cp;
    int ct;

    if (!s)
        return s;
    if (n != MS_LARGE_BUF*4-1) {
        LOG(LOG_CRITICAL, "common::get_line_from_sock", "Serious program logic error in get_line_from_sock().");
        exit(-1);
    }

    if (charsleft > MS_LARGE_BUF*4-3 && strchr(inbuf, '\n') == NULL) {
        draw_info("Metaserver returned an overly long line.", NDI_BLACK);
        return NULL;
    }

    /* If there is no line in the buffer */
    while (charsleft == 0 || (cp = strchr(inbuf, '\n')) == NULL) {
        FD_SET fdset = {1, fd};
        TIMEVAL tv = {3, 0}; /* 3 second timeout on reads */
        int nlen;
        if (select(0, &fdset, NULL, NULL, &tv) == 0) {
            draw_info("Metaserver timed out.", NDI_BLACK);
            return NULL;
        }

        nlen = recv(fd, inbuf+charsleft-1, MS_LARGE_BUF*4-1-charsleft, 0);
        if (nlen == SOCKET_ERROR || nlen <= 0) /* Probably EOF */
            return NULL;

        charsleft += nlen;
    }

    /* OK, inbuf contains a null terminated string with at least one \n
     * Copy the string up to the \n to s, and then move the rest of the
     * inbuf string to the beginning of the buffer.  And finally, set
     * charsleft to the number of characters left in inbuf, or 0.
     * Oh, and cp contains the location of the \n.
     */

    memcpy(s, inbuf, cp-inbuf+1); /* Extract the line, including the \n. */
    s[cp-inbuf+1] = 0; /* null terminate it */

    /* Copy cp to inbuf up to the \0, (skipping the \n) */
    ct = 0;
    while (cp[++ct] != 0) {
        inbuf[ct-1] = cp[ct];
    }
    inbuf[ct-1] = 0;
    charsleft = ct;    /* And keep track of how many characters are left. */

    return s;
}

#endif /* Win32 */

static void metaserver_load_cache(void) {
    FILE *cache;
    char buf[ MS_LARGE_BUF ];
    int name;

    if (cached_servers_loaded || !cached_server_file)
        return;

    /* If failure, we don't want to load again */
    cached_servers_loaded = 1;
    cached_servers_num = 0;

    cache = fopen(cached_server_file, "r");
    if (!cache)
        return;

    name = 0;
    while (fgets(buf, MS_LARGE_BUF, cache) != NULL && cached_servers_num < CACHED_SERVERS_MAX) {
        buf[strlen(buf)-1] = 0;
        if (!name) {
            name = 1;
            cached_servers_name[cached_servers_num] = strdup(buf);
        } else {
            name = 0;
            cached_servers_ip[cached_servers_num++] = strdup(buf);
        }
    }
    fclose(cache);
    if (name) {
        /* Missing IP? */
        cached_servers_num--;
    }
}

static void metaserver_save_cache(void) {
    FILE *cache;
    int server;

    if (!cached_server_file)
        return;

    cache = fopen(cached_server_file, "w");
    if (!cache)
        return;

    for (server = 0; server < cached_servers_num; server++) {
        fprintf(cache, "%s\n", cached_servers_name[server]);
        fprintf(cache, "%s\n", cached_servers_ip[server]);
    }
    fclose(cache);
}

/* This contacts the metaserver and gets the list of servers.  returns 0
 * on success, 1 on failure.  Errors will get dumped to stderr,
 * so most errors should be reasonably clear.
 * metaserver and meta_port are the server name and port number
 * to connect to.
 */

int metaserver_get_info(char *metaserver, int meta_port) {
    struct protoent *protox;
    int fd;
    struct sockaddr_in insock;
#ifndef WIN32
    FILE *fp;
#endif
    char inbuf[MS_LARGE_BUF*4];
    Meta_Info *current;

    if (!metaserver_on) {
        meta_numservers = 0;
        return 0;
    }

    metaserver_load_cache();

    protox = getprotobyname("tcp");
    if (protox == NULL) {
        LOG(LOG_WARNING, "common::metaserver_get_info", "Error getting protobyname (tcp)");
        return 1;
    }

    fd = socket(PF_INET, SOCK_STREAM, protox->p_proto);
    if (fd == -1) {
        perror("get_metaserver_info:  Error on socket command.\n");
        return 1;
    }
    insock.sin_family = AF_INET;
    insock.sin_port = htons((unsigned short)meta_port);
    if (isdigit(*metaserver))
        insock.sin_addr.s_addr = inet_addr(metaserver);
    else {
        struct hostent *hostbn = gethostbyname(metaserver);
        if (hostbn == NULL) {
            LOG(LOG_WARNING, "common::metaserver_get_info", "Unknown metaserver hostname: %s", metaserver);
            return 1;
        }
        memcpy(&insock.sin_addr, hostbn->h_addr, hostbn->h_length);
    }
    if (connect(fd, (struct sockaddr *)&insock, sizeof(insock)) == -1) {
        perror("Can't connect to metaserver");
        draw_info("\nCan't connect to metaserver.", NDI_BLACK);
	return 1;
    }

#ifndef WIN32 /* Windows doesn't support this */
    /* Turn this into a file handle - this will break it on newlines
     * for us, which makes our processing much easier - it basically
     * means one line/server
     */
    if ((fp = fdopen(fd, "r")) == NULL) {
        perror("fdopen failed.");
        return 1;
    }
#endif

    /* Don't reset this until here - that way if we successfully got
     * a list before, we can still use it.
     */
    meta_numservers = 0;
    if (!meta_servers)
        meta_servers = malloc(sizeof(Meta_Info)*MAX_METASERVER);

    /* The loop goes through and unpacks the data from the metaserver
     * into its individual components.  We do a little extra work and
     * put the |'s back in the string after we are done with that section -
     * this is so if there is a corrupt entry, it gets displayed as
     * originally received from the server.
     */
#ifndef WIN32 /* Windows doesn't support this */
    while (fgets(inbuf, MS_LARGE_BUF*4-1, fp) != NULL) {
#else
    while (get_line_from_sock(inbuf, MS_LARGE_BUF*4-1, fd) != NULL) {
#endif
        char *cp, *cp1;

        cp = strchr(inbuf, '|');
        if (cp == NULL) {
            LOG(LOG_WARNING, "common::metaserver_get_info", "Corrupt line from server: %s", inbuf);
            break;
        }
        *cp = 0;

        current = &meta_servers[meta_numservers];

        strncpy(current->ip_addr, inbuf, sizeof(current->ip_addr)-1);
        current->ip_addr[sizeof(current->ip_addr)-1] = '\0';
        *cp++ = '|';

        current->idle_time = atoi(cp);

        cp1 = strchr(cp, '|');
        if (cp1 == NULL) {
            LOG(LOG_WARNING, "common::metaserver_get_info", "Corrupt line from server: %s", inbuf);
            break;
        }
        *cp1 = 0;

        cp = strchr(cp1+1, '|');
        if (cp == NULL) {
            LOG(LOG_WARNING, "common::metaserver_get_info", "Corrupt line from server: %s", inbuf);
            break;
        }
        *cp = 0;
        /* cp1 points at start of comment, cp points at end */
        strncpy(current->hostname, cp1+1, sizeof(current->hostname)-1);
        current->hostname[sizeof(current->hostname)-1] = '\0';

        *cp1++ = '|';
        *cp++ = '|';  /* cp now points to num players */

        current->players = atoi(cp);

        cp1 = strchr(cp, '|');
        if (cp1 == NULL) {
            LOG(LOG_WARNING, "common::metaserver_get_info", "Corrupt line from server: %s", inbuf);
            break;
        }
        *cp1 = 0;

        cp = strchr(cp1+1, '|');
        if (cp == NULL) {
            LOG(LOG_WARNING, "common::metaserver_get_info", "Corrupt line from server: %s", inbuf);
            break;
        }
        *cp = 0;
        /* cp1 is start of version, cp is end */
        strncpy(current->version, cp1+1, sizeof(current->version)-1);
        current->version[sizeof(current->version)-1] = '\0';

        *cp1++ = '|';
        *cp++ = '|';  /* cp now points to comment */

        cp1 = strchr(cp, '\n');
        if (cp1 == NULL) {
            LOG(LOG_WARNING, "common::metaserver_get_info", "Corrupt line from server: %s", inbuf);
            break;
        }
        *cp1 = 0;
        /* There is extra info included, like the bytes to/from the server
         * that we dont' care about, so strip them off so they don't show up in
         * the comment.
         */
        cp1 = strchr(cp, '|');
        if (cp1 != NULL)
            *cp1 = 0;

        strncpy(current->comment, cp, sizeof(current->comment)-1);
        current->comment[sizeof(current->comment)-1] = '\0';

        meta_numservers++;
        /* has to be 1 less than array size, since array starts counting
         * at 0.
         */
        if (meta_numservers >= MAX_METASERVER-1) {
            LOG(LOG_WARNING, "common:metaserver_get_info", "Have reached maximum metaserver count\n");
            break;
        }
    }
#ifdef WIN32
    closesocket(fd);
#else
    fclose(fp);
#endif
    qsort(meta_servers, meta_numservers, sizeof(Meta_Info), (int (*)(const void *, const void *))meta_sort);

    return 0;
}

/* show the metaservers to the player.  we use the draw_info to do
 * that, and also let the player know they can enter their own host name.
 */
void metaserver_show(int show_selection) {
    int i;
    char buf[256];

    if (cached_servers_num) {
        draw_info("\nLast servers you connected to:\n", NDI_BLACK);
        for (i = 0; i < cached_servers_num; i++) {
            sprintf(buf, "%2d) %-20.20s %-20.20s", i+1, cached_servers_name[i], cached_servers_ip[i]);
            draw_info(buf, NDI_BLACK);
        }
        draw_info(" ", NDI_BLACK);
    }

    draw_info(" #)     Server        #     version   idle", NDI_BLACK);
    draw_info("         Name      players           seconds", NDI_BLACK);
    for (i = 0; i < meta_numservers; i++) {
        sprintf(buf, "%2d)  %-15.15s %2d   %-12.12s %2d",
            i+1+cached_servers_num, meta_servers[i].hostname,
            meta_servers[i].players, meta_servers[i].version,
            meta_servers[i].idle_time);
        draw_info(buf, NDI_BLACK);
    }
    if (show_selection) {
        /* Show default/current server */
	if (server) {
	    sprintf(buf, "%2d)  %s (default)", meta_numservers+1+cached_servers_num, server);
	    draw_info(buf, NDI_BLACK);
	}

        draw_info("Choose one of the entries above", NDI_BLACK);
        draw_info("or type in a hostname/ip address", NDI_BLACK);
        draw_info("Hit enter to re-update this list", NDI_BLACK);
        draw_info("Enter 0 to exit the program.", NDI_BLACK);
    }
}

/* String contains the selection that the player made for the metaserver.
 * this may not be a a selection, but could be a host name or ip address.
 * this returns 0 on sucessful selection, 1 if failure (invalid selection
 * or the like.
 */
int metaserver_select(char *sel) {
    int num = atoi(sel);
    int port;
    char buf[MAX_BUF], buf2[MAX_BUF];
    char *server_name = NULL, *server_ip;

    /* User hit return */
    if (sel[0] == 0) {
        metaserver_get_info(meta_server, meta_port);
        metaserver_show(TRUE);
        return 1;
    }

    /* Special case - player really entered a 0, so exit the
     * program.
     */
    if (num == 0 && sel[0] == '0') {
#ifdef WIN32
        script_killall();
#endif
        exit(0);
    }

    /* if the entry is not a number (selection from the list),
     * or is a selection but also has a dot (suggesting
     * a.b.c.d selection), just try to connect with given name.
     */
    if (num == 0 || strchr(sel, '.') != NULL) {
        server_name = sel;
        server_ip = sel;
    } else {
        if (num <= 0 || num > meta_numservers+cached_servers_num+1) {
            draw_info("Invalid selection. Try again", NDI_BLACK);
            return 1;
        }

        if (num == meta_numservers+cached_servers_num+1) {
            server_name = server;
            server_ip = server;
        } else if (num > cached_servers_num) {
            server_name = meta_servers[num-cached_servers_num-1 ].hostname;
            server_ip = meta_servers[num-cached_servers_num-1 ].ip_addr;
        } else {
            server_name = cached_servers_name[num-1];
            server_ip = cached_servers_ip[num-1];
        }
    }
    if (!server_name) {
        draw_info("Bad selection. Try again", NDI_BLACK);
        return 1;
    }

    /* check for :port suffix, and use it */
    if ((sel = strrchr(server_name, ':')) != NULL && (port = atoi(sel+1)) > 0) {
        snprintf(buf2, sizeof(buf2), "%s", server_name);
        buf2[sel-server_name] = '\0';
        server_name = buf2;
    }
    else {
        port = use_config[CONFIG_PORT];
    }

    sprintf(buf, "Trying to connect to %s:%d", server_name, port);
    draw_info(buf, NDI_BLACK);
#ifdef MULTKEYS
    csocket.fd = init_connection(server_name, port);
#else
    csocket.fd = init_connection(server_ip, port);
#endif
    if (csocket.fd == -1) {
        draw_info("Unable to connect to server.", NDI_BLACK);
        return 1;
    }

    /* Add server to cache */
    if ((num <= meta_numservers) && (num != meta_numservers + cached_servers_num + 1)) {
        int index;
        for (index = 0; index < cached_servers_num; index++) {
            if (strcmp(server_name, cached_servers_name[index]) == 0)
                break;
        }
        /* If server is first in cache, no need to re-add id */
        if (index != 0 || !cached_servers_num) {
            char *name;
            char *ip;
            int copy;

            if (index == cached_servers_num) {
                name = strdup(server_name);
                ip = strdup(server_ip);
                cached_servers_num++;
                if (cached_servers_num > CACHED_SERVERS_MAX) {
                    cached_servers_num--;
                    free(cached_servers_name[cached_servers_num-1]);
                    free(cached_servers_ip[cached_servers_num-1]);
                }
            } else {
                name = cached_servers_name[index];
                ip = cached_servers_ip[index];
            }
            for (copy = MIN(index, CACHED_SERVERS_MAX-1); copy > 0; copy--) {
                cached_servers_name[copy] = cached_servers_name[copy-1];
                cached_servers_ip[copy] = cached_servers_ip[copy-1];
            }
            cached_servers_name[0] = name;
            cached_servers_ip[0] = ip;
            metaserver_save_cache();
        }
    }

    return 0;
}

#ifdef MS_STANDALONE
/* This is here just to verify that the code seems to be working
 * properly
 * To use this code, compile as:
 *  gcc -o metaserver -I. -DMS_STANDALONE metaserver.c
 */

int main(int argc, char *argv[])
{
    int i;

    metaserver_get_info(META_SERVER, META_PORT);
    for (i = 0; i < meta_numservers; i++) {
        printf("%s:%d:%s:%d:%s:%s\n",
            meta_servers[i].ip_addr,
            meta_servers[i].idle_time,
            meta_servers[i].hostname,
            meta_servers[i].players,
            meta_servers[i].version,
            meta_servers[i].comment);
    }
}

#endif
