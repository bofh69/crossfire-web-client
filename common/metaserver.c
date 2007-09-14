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

#ifdef HAVE_CURL_CURL_H
#include <curl/curl.h>
#include <curl/types.h>
#include <curl/easy.h>
#endif

#include <pthread.h>

Meta_Info *meta_servers = NULL;

int meta_numservers = 0;

/* This checks the servers sc_version and cs_version to see
 * if they are compatible.
 * @parm entry
 * entry number in the metaservers array to check.
 * @return
 * 1 if this entry is compatible, 0 if it is not.  Note that this can
 * only meaningfully check metaserver2 data - metaserver1 doesn't
 * include protocol version number, so treats all of those as
 * OK.
 */
int check_server_version(int entry)
{

    /* No version information - nothing to do. */
    if (!meta_servers[entry].sc_version || !meta_servers[entry].cs_version)
	return 1;

    if (meta_servers[entry].sc_version != VERSION_SC) {
	/* 1027->1028 removed a bunch of old commands, so a 1028
	 * version client can still play on a 1027 server, so
	 * special hard code that.
	 */
	 /* This could perhaps get extended in the future, if other protocol
	 * revision that maintain compatibility.
	 */
	if (VERSION_SC != 1028 && meta_servers[entry].sc_version != 1027) return 0;
    }
    if (meta_servers[entry].cs_version != VERSION_CS) return 0;

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
static int ms2_is_running=0;

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
 * Curle doesn't really have any built in way to get data
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
size_t metaserver2_writer(void *ptr, size_t size, size_t nmemb, void *data)
{
#ifdef HAVE_CURL_CURL_H
    size_t realsize = size * nmemb;
    char    *cp, *newline, *eq, inbuf[CURL_MAX_WRITE_SIZE*2+1], *leftover;

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
    memcpy(inbuf+strlen(leftover), ptr, realsize);
    inbuf[realsize] = 0;

    /* Processing this block of data shouldn't take very long, even on
     * slow machines, so putting the lock here, instead of each time
     * we update a variable is cleaner
     */
    pthread_mutex_lock(&ms2_info_mutex);

    for (cp = inbuf; cp != NULL && *cp!=0; cp=newline) {
	newline=strchr(cp, '\n');
	if (newline) {
	    *newline = 0;
	    newline++;
	} else {
	    /* If we didn't get a newline, then this is the
	     * end of the block of data for this call - store
	     * away the extra for the next call.
	     */
	    strncpy(leftover, cp, CURL_MAX_WRITE_SIZE-1);
	    leftover[CURL_MAX_WRITE_SIZE-1] = 0;
	    break;
	}

	eq = strchr(cp,'=');
	if (eq) {
	    *eq = 0;
	    eq++;
	}

	if (!strcmp(cp, "START_SERVER_DATA")) {
	    /* Clear out all data - MS2 doesn't necessarily use all the
	     * fields, so blank out any that we are not using.
	     */
	    memset(&meta_servers[meta_numservers], 0, sizeof(Meta_Info));
	}
	else if (!strcmp(cp, "END_SERVER_DATA")) {
	    int i;

	    /* we can get data from both metaserver1 & 2 - no reason to keep
	     * both.  So check for duplicates, and consider metaserver2
	     * data 'better'.
	     */
	    for (i=0; i<meta_numservers; i++) {
		if (!strcasecmp(meta_servers[i].hostname, meta_servers[meta_numservers].hostname)) {
		    memcpy(&meta_servers[i], &meta_servers[meta_numservers], sizeof(Meta_Info));
		    break;
		}
	    }
	    if (i>=meta_numservers) {
		meta_numservers++;
	    }
	} else {
	    /* If we get here, these should be variable=value pairs.
	     * if we don't have a value, can't do anything, and
	     * report an error.  This would normally be incorrect
	     * data from the server.
	     */
	    if (!eq) {
		LOG(LOG_ERROR, "common::metaserver2_writer", "Unknown line: %s",cp);
		continue;
	    }
	    if (!strcmp(cp,"hostname")) {
		strncpy(meta_servers[meta_numservers].hostname, eq, sizeof(meta_servers[meta_numservers].hostname));
	    }
	    else if (!strcmp(cp,"port")) {
		meta_servers[meta_numservers].port = atoi(eq);
	    }
	    else if (!strcmp(cp,"html_comment")) {
		strncpy(meta_servers[meta_numservers].html_comment, eq, sizeof(meta_servers[meta_numservers].html_comment));
	    }
	    else if (!strcmp(cp,"text_comment")) {
		strncpy(meta_servers[meta_numservers].text_comment, eq, sizeof(meta_servers[meta_numservers].text_comment));
	    }
	    else if (!strcmp(cp,"archbase")) {
		strncpy(meta_servers[meta_numservers].archbase, eq, sizeof(meta_servers[meta_numservers].archbase));
	    }
	    else if (!strcmp(cp,"mapbase")) {
		strncpy(meta_servers[meta_numservers].mapbase, eq, sizeof(meta_servers[meta_numservers].mapbase));
	    }
	    else if (!strcmp(cp,"codebase")) {
		strncpy(meta_servers[meta_numservers].codebase, eq, sizeof(meta_servers[meta_numservers].codebase));
	    }
	    else if (!strcmp(cp,"flags")) {
		strncpy(meta_servers[meta_numservers].flags, eq, sizeof(meta_servers[meta_numservers].flags));
	    }
	    else if (!strcmp(cp,"version")) {
		strncpy(meta_servers[meta_numservers].version, eq, sizeof(meta_servers[meta_numservers].version));
	    }
	    else if (!strcmp(cp,"num_players")) {
		meta_servers[meta_numservers].num_players = atoi(eq);
	    }
	    else if (!strcmp(cp,"in_bytes")) {
		meta_servers[meta_numservers].in_bytes = atoi(eq);
	    }
	    else if (!strcmp(cp,"out_bytes")) {
		meta_servers[meta_numservers].out_bytes = atoi(eq);
	    }
	    else if (!strcmp(cp,"uptime")) {
		meta_servers[meta_numservers].uptime = atoi(eq);
	    }
	    else if (!strcmp(cp,"sc_version")) {
		meta_servers[meta_numservers].sc_version = atoi(eq);
	    }
	    else if (!strcmp(cp,"cs_version")) {
		meta_servers[meta_numservers].cs_version = atoi(eq);
	    }
	    else if (!strcmp(cp,"last_update")) {
		/* MS2 reports update time as when it last got an update,
		 * where as we want actual elapsed time since last update.
		 * So do the conversion.  Second check is because of clock
		 * skew - my clock may be fast, and we don't want negative times.
		 */
		meta_servers[meta_numservers].idle_time = time(NULL) - atoi(eq);
		if (meta_servers[meta_numservers].idle_time < 0)
		     meta_servers[meta_numservers].idle_time = 0;
	    }
	    else {
		LOG(LOG_ERROR, "common::metaserver2_writer", "Unknown line: %s=%s",cp,eq);
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
    char    leftover[CURL_MAX_WRITE_SIZE];

    curl = curl_easy_init();
    if (!curl) return 0;
    leftover[0] =0;
    curl_easy_setopt(curl, CURLOPT_URL, metaserver2);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, metaserver2_writer);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, leftover);
    res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res) return 0;
    else return 1;
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
void *metaserver2_thread(void *junk)
{
    int metaserver_choice;

    do {
	metaserver_choice = random() % (sizeof(metaservers) / sizeof(char*));

        fprintf(stderr,"Choosing server %d\n", metaserver_choice);
    } while (!get_metaserver2_data(metaservers[metaserver_choice]));

    pthread_mutex_lock(&ms2_info_mutex);
    qsort(meta_servers, meta_numservers, sizeof(Meta_Info), (int (*)(const void *, const void *))meta_sort);
    ms2_is_running=0;
    pthread_mutex_unlock(&ms2_info_mutex);
    pthread_exit(NULL);
}


/**
 * this is basically a replacement to the metaserver_get_info -
 * idea being that when metaserver 1 support goes away,
 * just yank that entire function and replace it with
 * this.
 * @return
 * best I can tell, always returns 0
 */
int metaserver2_get_info() {
    pthread_t   thread_id;
    int	    ret;

    if (!metaserver2_on) {
        return 0;
    }
#ifndef HAVE_CURL_CURL_H
    return 0;
#endif

    metaserver_load_cache();

    pthread_mutex_lock(&ms2_info_mutex);
    if (!meta_servers)
        meta_servers = malloc(sizeof(Meta_Info)*MAX_METASERVER);

    ms2_is_running=1;
    pthread_mutex_unlock(&ms2_info_mutex);

    ret=pthread_create(&thread_id, NULL, metaserver2_thread, NULL);
    if (ret) {
        LOG(LOG_ERROR, "common::metaserver2_get_info", "Thread creation failed.");
	pthread_mutex_lock(&ms2_info_mutex);
	ms2_is_running=0;
	pthread_mutex_unlock(&ms2_info_mutex);
    }

    return 0;
}    



/**
 * Does single use initalization of metaserver2 variables.
 */
void init_metaserver()
{
    pthread_mutex_init(&ms2_info_mutex, NULL);
#ifdef HAVE_CURL_CURL_H
    curl_global_init(CURL_GLOBAL_ALL);
#endif
}

/******************************************************************************
 * End of Metasever2 functions.
 ******************************************************************************/

/******************************************************************************
 * Start of metaserver1 logic
 *
 * Note that this shares the same mutex as metaserver2, since it is updating
 * most of the same structures.
 *******************************************************************************/

static int ms1_is_running=0;


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

void *metaserver1_thread(void *junk)
{
    struct protoent *protox;
    int fd;
    struct sockaddr_in insock;
#ifndef WIN32
    FILE *fp;
#endif
    char inbuf[MS_LARGE_BUF*4];
    Meta_Info *current;

    protox = getprotobyname("tcp");
    if (protox == NULL) {
        LOG(LOG_WARNING, "common::metaserver_get_info", "Error getting protobyname (tcp)");
	pthread_mutex_lock(&ms2_info_mutex);
	ms1_is_running=0;
	pthread_mutex_unlock(&ms2_info_mutex);
	pthread_exit(NULL);
    }

    fd = socket(PF_INET, SOCK_STREAM, protox->p_proto);
    if (fd == -1) {
        perror("get_metaserver_info:  Error on socket command.\n");
	pthread_mutex_lock(&ms2_info_mutex);
	ms1_is_running=0;
	pthread_mutex_unlock(&ms2_info_mutex);
	pthread_exit(NULL);
    }
    insock.sin_family = AF_INET;
    insock.sin_port = htons((unsigned short)meta_port);
    if (isdigit(*meta_server))
        insock.sin_addr.s_addr = inet_addr(meta_server);
    else {
        struct hostent *hostbn = gethostbyname(meta_server);
        if (hostbn == NULL) {
            LOG(LOG_WARNING, "common::metaserver_get_info", "Unknown metaserver hostname: %s", meta_server);
	    pthread_mutex_lock(&ms2_info_mutex);
	    ms1_is_running=0;
	    pthread_mutex_unlock(&ms2_info_mutex);
	    pthread_exit(NULL);
        }
        memcpy(&insock.sin_addr, hostbn->h_addr, hostbn->h_length);
    }
    if (connect(fd, (struct sockaddr *)&insock, sizeof(insock)) == -1) {
        perror("Can't connect to metaserver");
        draw_info("\nCan't connect to metaserver.", NDI_BLACK);
	pthread_mutex_lock(&ms2_info_mutex);
	ms1_is_running=0;
	pthread_mutex_unlock(&ms2_info_mutex);
	pthread_exit(NULL);
    }

#ifndef WIN32 /* Windows doesn't support this */
    /* Turn this into a file handle - this will break it on newlines
     * for us, which makes our processing much easier - it basically
     * means one line/server
     */
    if ((fp = fdopen(fd, "r")) == NULL) {
        perror("fdopen failed.");
	pthread_mutex_lock(&ms2_info_mutex);
	ms1_is_running=0;
	pthread_mutex_unlock(&ms2_info_mutex);
	pthread_exit(NULL);
    }
#endif

    pthread_mutex_lock(&ms2_info_mutex);
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

        current->num_players = atoi(cp);

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

        strncpy(current->text_comment, cp, sizeof(current->text_comment)-1);
        current->text_comment[sizeof(current->text_comment)-1] = '\0';

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
    ms1_is_running=0;
    pthread_mutex_unlock(&ms2_info_mutex);
    pthread_exit(NULL);
}


int metaserver1_get_info() {
    pthread_t   thread_id;
    int	    ret;

    if (!metaserver_on) {
        return 0;
    }
    metaserver_load_cache();

    pthread_mutex_lock(&ms2_info_mutex);
    if (!meta_servers)
        meta_servers = malloc(sizeof(Meta_Info)*MAX_METASERVER);

    ms1_is_running=1;
    pthread_mutex_unlock(&ms2_info_mutex);

    ret=pthread_create(&thread_id, NULL, metaserver1_thread, NULL);
    if (ret) {
        LOG(LOG_ERROR, "common::metaserver1_get_info", "Thread creation failed.");
	pthread_mutex_lock(&ms2_info_mutex);
	ms1_is_running=0;
	pthread_mutex_unlock(&ms2_info_mutex);
    }

    return 0;
}    
/******************************************************************************
 * End of metaserver1 logic
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
    status = ms2_is_running | ms1_is_running;
    pthread_mutex_unlock(&ms2_info_mutex);

    return status;
}

/* This contacts the metaserver and gets the list of servers.  returns 0
 * on success, 1 on failure.  Errors will get dumped to stderr,
 * so most errors should be reasonably clear.
 * metaserver and meta_port are the server name and port number
 * to connect to.
 */

int metaserver_get_info(char *metaserver, int meta_port) {

    meta_numservers = 0;

    metaserver2_get_info();

    if (metaserver_on) {
	metaserver1_get_info();
    }

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

    while(metaserver_check_status()) {
	usleep(100);
    }

    draw_info(" #)     Server        #     version   idle", NDI_BLACK);
    draw_info("         Name      players           seconds", NDI_BLACK);
    pthread_mutex_lock(&ms2_info_mutex);

    /* Re-sort the data - may get different data from ms1 and ms2, so 
     * order of this is somewhat random.
     */
    qsort(meta_servers, meta_numservers, sizeof(Meta_Info), (int (*)(const void *, const void *))meta_sort);
    for (i = 0; i < meta_numservers; i++) {
	if (check_server_version(i)) {
	    sprintf(buf, "%2d)  %-15.15s %2d   %-12.12s %2d",
		    i+1+cached_servers_num, meta_servers[i].hostname,
		    meta_servers[i].num_players, meta_servers[i].version,
		    meta_servers[i].idle_time);
	    draw_info(buf, NDI_BLACK);
	}
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
    pthread_mutex_unlock(&ms2_info_mutex);
}

/* String contains the selection that the player made for the metaserver.
 * this may not be a a selection, but could be a host name or ip address.
 * this returns 0 on sucessful selection, 1 if failure (invalid selection
 * or the like.
 */
int metaserver_select(char *sel) {
    int num = atoi(sel);
    int port=0;
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

    pthread_mutex_lock(&ms2_info_mutex);

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
	    port = meta_servers[num-cached_servers_num-1 ].port;
        } else {
            server_name = cached_servers_name[num-1];
            server_ip = cached_servers_ip[num-1];
        }
    }
    pthread_mutex_unlock(&ms2_info_mutex);
    if (!server_name) {
        draw_info("Bad selection. Try again", NDI_BLACK);
        return 1;
    }

    /* check for :port suffix, and use it */
    if (!port) {
	if ((sel = strrchr(server_name, ':')) != NULL && (port = atoi(sel+1)) > 0) {
	    snprintf(buf2, sizeof(buf2), "%s", server_name);
	    buf2[sel-server_name] = '\0';
	    server_name = buf2;
	}
	else {
	    port = use_config[CONFIG_PORT];
	}
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
            meta_servers[i].num_players,
            meta_servers[i].version,
            meta_servers[i].text_comment);
    }
}

#endif

#ifdef MS2_STANDALONE
/* This is here just to verify that the code seems to be working
 * properly
 * To use this code, compile as:
 *  gcc -o metaserver -I. -DMS2_STANDALONE metaserver.c misc.o -lcurl -lpthread
 */

/* Following lines are to cover external symbols not
 * defined - trying to bring in the files the are defined
 * in just causes more dependencies, etc.
 */
void draw_info(const char *str, int color) { }
int init_connection(char *host, int port) {} 

int metaserver_on=1, meta_port=0;
char *server=NULL, *meta_server;
sint16 use_config[CONFIG_NUMS];
ClientSocket csocket;

int main(int argc, char *argv[])
{
    int i;

    init_metaserver();
    metaserver2_get_info();
    fprintf(stderr,"Collecting data.");
    while (metaserver2_check_status()) {
	fprintf(stderr,".");
	sleep(1);
    }
    fprintf(stderr,"\n");
    for (i = 0; i < meta_numservers; i++) {
        printf("%s:%d:%s:%d:%s:%s\n",
            meta_servers[i].ip_addr,
            meta_servers[i].idle_time,
            meta_servers[i].hostname,
            meta_servers[i].num_players,
            meta_servers[i].version,
            meta_servers[i].text_comment);
    }
}

#endif
