/*
 * static char *rcsid_metaserver_c =
 *   "$Id$";
 */
/*
    Crossfire client, a client program for the crossfire program.

    Copyright (C) 2000 Mark Wedel

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

    The author can be reached via e-mail to mwedel@scruz.net
*/

/* This file deals with contact the metaserver, getting a list of hosts,
 * displaying/returning them to calling function, and then connecting
 * to the server when requested.
 */

#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <stdio.h>

#include <client.h>
#include <cconfig.h>

/* Arbitrary size.  At some point, we would need to cut this off simply
 * for display/selection reasons.
 */
#define MAX_METASERVER 100

/* Various constants we use in the structure */
#define MS_SMALL_BUF	20
#define MS_LARGE_BUF	256

typedef struct Meta_Info {
    char    ip_addr[MS_SMALL_BUF];
    int	    idle_time;
    char    hostname[MS_LARGE_BUF];
    int	    players;
    char    version[MS_SMALL_BUF];
    char    comment[MS_LARGE_BUF];
} Meta_Info;



Meta_Info meta_servers[MAX_METASERVER];
int meta_numservers=0;

/* This contacts the metaserver and gets the list of servers.  returns 0
 * on success, 1 on failure.  Errors will get dumped to stderr,
 * so most errors should be reasonably clear.
 * metaserver and meta_port are the server name and port number
 * to connect to.
 */
 
int metaserver_get_info(char *metaserver, int meta_port)
{
    struct protoent *protox;
    int fd;
    struct sockaddr_in insock;
    FILE *fp;
    char    inbuf[MS_LARGE_BUF*4];

    protox = getprotobyname("tcp");
    if (protox == (struct protoent  *) NULL)
    {
        fprintf(stderr, "Error getting protobyname (tcp)\n");
        return 1;
    }
    fd = socket(PF_INET, SOCK_STREAM, protox->p_proto);
    if (fd==-1) {
        perror("get_metaserver_info:  Error on socket command.\n");
	return 1;
    }
    insock.sin_family = AF_INET;
    insock.sin_port = htons((unsigned short)meta_port);
    if (isdigit(*metaserver))
        insock.sin_addr.s_addr = inet_addr(metaserver);
    else {
        struct hostent *hostbn = gethostbyname(metaserver);
        if (hostbn == (struct hostent *) NULL)
        {
            fprintf(stderr,"Unknown metaserver hostname: %s\n",metaserver);
	    return 1;
	}
        memcpy(&insock.sin_addr, hostbn->h_addr, hostbn->h_length);
    }
    if (connect(fd,(struct sockaddr *)&insock,sizeof(insock)) == (-1))
    {
        perror("Can't connect to server");
	return 1;
    }
    /* Turn this into a file handle - this will break it on newlines
     * for us, which makes our processing much easier - it basically
     * means one line/server
     */
    if ((fp=fdopen(fd, "r"))==NULL) {
	perror("fdopen failed.");
	return 1;
    }
    /* Don't reset this until here - that way if we successfully got
     * a list before, we can still use it.
     */
    meta_numservers=0;

    /* The loop goes through and unpacks the data from the metaserver
     * into its individual components.  We do a little extra work and
     * put the |'s back in the string after we are done with that section -
     * this is so if there is a corrupt entry, it gets displayed as
     * originally received from the server.
     */
    while (fgets(inbuf, MS_LARGE_BUF*4-1, fp)!=NULL) {
	char *cp,*cp1;

	if ((cp=strchr(inbuf,'|'))==NULL) {
	    fprintf(stderr,"Corrupt line from server: %s\n", inbuf);
	    break;
	}
	*cp=0;
	strncpy(meta_servers[meta_numservers].ip_addr, inbuf, MS_SMALL_BUF);
	meta_servers[meta_numservers].ip_addr[MS_SMALL_BUF]=0;
	*cp++='|';

	meta_servers[meta_numservers].idle_time = atoi(cp);

	if ((cp1=strchr(cp,'|'))==NULL) {
	    fprintf(stderr,"Corrupt line from server: %s\n", inbuf);
	    break;
	}
	*cp1=0;

	if ((cp=strchr(cp1+1,'|'))==NULL) {
	    fprintf(stderr,"Corrupt line from server: %s\n", inbuf);
	    break;
	}
	*cp=0;
	/* cp1 points at start of comment, cp points at end */
	strncpy(meta_servers[meta_numservers].hostname, cp1+1, MS_LARGE_BUF);
	meta_servers[meta_numservers].hostname[MS_LARGE_BUF]=0;

	*cp1++='|';
	*cp++='|';  /* cp now points to num players */
	
	meta_servers[meta_numservers].players = atoi(cp);

	if ((cp1=strchr(cp,'|'))==NULL) {
	    fprintf(stderr,"Corrupt line from server: %s\n", inbuf);
	    break;
	}
	*cp1=0;

	if ((cp=strchr(cp1+1,'|'))==NULL) {
	    fprintf(stderr,"Corrupt line from server: %s\n", inbuf);
	    break;
	}
	*cp=0;
	/* cp1 is start of version, cp is end */
	strncpy(meta_servers[meta_numservers].version, cp1+1, MS_SMALL_BUF);
	meta_servers[meta_numservers].version[MS_SMALL_BUF]=0;

	*cp1++='|';
	*cp++='|';  /* cp now points to comment */

	if ((cp1=strchr(cp,'\n'))==NULL) {
	    fprintf(stderr,"Corrupt line from server: %s\n", inbuf);
	    break;
	}
	*cp1=0;
	
	strncpy(meta_servers[meta_numservers].comment, cp, MS_LARGE_BUF);
	meta_servers[meta_numservers].comment[MS_LARGE_BUF]=0;

	meta_numservers++;
    }
    fclose(fp);
    return 0;
}

/* show the metaservers to the player.  we use the draw_info to do
 * that, and also let the player know they can enter their own host name.
 */
void metaserver_show()
{
    int i;
    char buf[256];

    draw_info(" #)     Server        #     version   idle", NDI_BLACK);
    draw_info("         Name      players           seconds", NDI_BLACK);
    for (i=0; i<meta_numservers; i++) {
	sprintf(buf,"%2d)  %-15.15s %2d   %-12.12s %2d",
		i+1, meta_servers[i].hostname,
		meta_servers[i].players, meta_servers[i].version,
		meta_servers[i].idle_time);
	draw_info(buf, NDI_BLACK);
    }
    /* Show default/current server */
    sprintf(buf,"%2d)  %s (default)", meta_numservers+1, server);
    draw_info(buf, NDI_BLACK);

    draw_info("Choose one of the entries above", NDI_BLACK);
    draw_info("or type in a hostname/ip address", NDI_BLACK);
    draw_info("Enter 0 to exit the program.", NDI_BLACK);
}

/* String contains the selection that the player made for the metaserver.
 * this may not be a a selection, but could be a host name or ip address.
 * this returns 0 on sucessful selection, 1 if failure (invalid selection
 * or the like.
 */
int metaserver_select(char *sel)
{
    int num=atoi(sel);
    char buf[MAX_BUF], *server_name=NULL,*server_ip;


    /* Special case - player really entered a 0, so exit the
     * program.
     */
    if (num==0 && sel[0]=='0') exit(0);

    /* if the entry is not a number (selection from the list), 
     * or is a selection but also has a dot (suggesting 
     * a.b.c.d selection), just try to connect with given name.
     */
    if ((num==0) || (num && strchr(sel,'.'))) {
	server_name=sel;
	server_ip=sel;
    } else {
	num--;
	if (num<0 || num>meta_numservers) {
	    draw_info("Invalid selection. Try again", NDI_BLACK);
	    return 1;
	}
	if (num==meta_numservers){
	    server_name=server;
	    server_ip=server;
	}
	else {
	    server_name=meta_servers[num].hostname;
	    server_ip = meta_servers[num].ip_addr;
	}
    }
    if (!server_name) {
	draw_info("Bad selection. Try again", NDI_BLACK);
	return 1;
    }
	
    sprintf(buf,"Trying to connect to %s", server_name);
    draw_info(buf,NDI_BLACK);
    csocket.fd=init_connection(server_ip, port_num);
    if (csocket.fd==-1) {
	draw_info("Unable to connect to server.", NDI_RED);
	return 1;
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
    for (i=0; i<meta_numservers; i++) {
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
