/*
 * static char *rcsid_client_c =
 *   "$Id$";
 */
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

 /* Client interface main routine.
  * this file sets up a few global variables, connects to the server,
  * tells it what kind of pictures it wants, adds the client and enters
  * the main dispatch loop
  *
  * the main event loop (event_loop()) checks the tcp socket for input and
  * then polls for x events.  This should be fixed since you can just block
  * on both filedescriptors.
  *
  * The DoClient function recieves a message (an ArgList), unpacks it, and
  * in a slow for loop dispatches the command to the right function through
  * the commands table.   ArgLists are essentially like RPC things, only 
  * they don't require going through RPCgen, and it's easy to get variable
  * length lists.  They are just lists of longs, strings, characters, and 
  * byte arrays that can be converted to a machine independent format
 */


#include <client.h>
#include <external.h>
#include <errno.h>

/* actually declare the globals */

char *server=SERVER,*client_libdir=NULL,*meta_server=META_SERVER;
char *image_file="";

int meta_port=META_PORT, want_skill_exp=0,
    replyinfo_status=0, requestinfo_sent=0, replyinfo_last_face=0,
    maxfd,map1cmd=0,metaserver_on=METASERVER;
FILE *fpin,*fpout;
Client_Player cpl;
ClientSocket csocket;

char *resists_name[NUM_RESISTS] = {
"armor", "magic", "fire", "elec", 
"cold", "conf", "acid", "drain",
"ghit", "pois", "slow", "para",
"t undead", "fear", "depl","death", 
"hword", "blind"};

char *skill_names[MAX_SKILL] = {
"agility", "personality", "mental", "physique", "magic", "wisdom"
};

typedef void (*CmdProc)(unsigned char *, int len);

struct CmdMapping {
  char *cmdname;
  void (*cmdproc)(unsigned char *, int );
};


struct CmdMapping commands[] = {
    /* Order of this table doesn't make a difference.  I tried to sort
     * of cluster the related stuff together.
     */
    { "map1", Map1Cmd },
    { "map1a", Map1aCmd },
    { "map_scroll", (CmdProc)map_scrollCmd },
    { "magicmap", MagicMapCmd},
    { "newmap", NewmapCmd },

    { "item1", Item1Cmd },
    { "item2", Item2Cmd },
    { "upditem", UpdateItemCmd },
    { "delitem", DeleteItem },
    { "delinv",	DeleteInventory },

    { "drawinfo", (CmdProc)DrawInfoCmd },
    { "stats", StatsCmd },

    { "image", ImageCmd },
    { "image2", Image2Cmd },
    { "face", FaceCmd},
    { "face1", Face1Cmd},
    { "face2", Face2Cmd},


    { "sound", SoundCmd},
    { "anim", AnimCmd},

    { "player", PlayerCmd },
    { "comc", CompleteCmd},

    { "addme_failed", (CmdProc)AddMeFail },
    { "addme_success", (CmdProc)AddMeSuccess },
    { "version", (CmdProc)VersionCmd },
    { "goodbye", (CmdProc)GoodbyeCmd },
    { "setup", (CmdProc)SetupCmd},

    { "query", (CmdProc)handle_query},
    { "replyinfo", (CmdProc)ReplyInfoCmd},
};

#define NCOMMANDS (sizeof(commands)/sizeof(struct CmdMapping))

void DoClient(ClientSocket *csocket)
{
    int i,len;
    unsigned char *data;

    while (1) {
	i=SockList_ReadPacket(csocket->fd, &csocket->inbuf, MAXSOCKBUF-1);
	if (i==-1) {
	    /* Need to add some better logic here */
	    /*ET: not an error.  It's EOF!  At least errno isn't valid.
	    fprintf(stderr,"Got error on read (error %d)\n", errno);
	    */
	    csocket->fd=-1;
	    return;
	}
	if (i==0) return;   /* Don't have a full packet */
	csocket->inbuf.buf[csocket->inbuf.len]='\0';
        data = (unsigned char *)strchr((char*)csocket->inbuf.buf +2, ' ');
	if (data) {
	    *data='\0';
	    data++;
	}
        len = csocket->inbuf.len - (data - csocket->inbuf.buf);
	/* Terminate the buffer */
	LOG(0,"Command:%s (%d)\n",csocket->inbuf.buf+2, len);
	for(i=0;i < NCOMMANDS;i++) {
	    if (strcmp((char*)csocket->inbuf.buf+2,commands[i].cmdname)==0) {
		    commands[i].cmdproc(data,len);
		    break;
	    }
	}
	csocket->inbuf.len=0;
	if (i == NCOMMANDS) {
	    printf("Bad command from server (%s)\n",csocket->inbuf.buf+2);
	}
    }
}

#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <ctype.h>
#include <arpa/inet.h>

/* returns the fd of the connected socket, -1 on failure. */

int init_connection(char *host, int port)
{
    struct protoent *protox;
    int fd, oldbufsize, newbufsize=65535, buflen=sizeof(int);
    struct sockaddr_in insock;

    protox = getprotobyname("tcp");
    if (protox == (struct protoent  *) NULL)
    {
	fprintf(stderr, "Error getting prorobyname (tcp)\n");
	return -1;
    }
    fd = socket(PF_INET, SOCK_STREAM, protox->p_proto);
    if (fd==-1) {
	perror("init_connection:  Error on socket command.\n");
	return -1;
    }
    insock.sin_family = AF_INET;
    insock.sin_port = htons((unsigned short)port);
    if (isdigit(*host))
	insock.sin_addr.s_addr = inet_addr(host);
    else {
	struct hostent *hostbn = gethostbyname(host);
	if (hostbn == (struct hostent *) NULL)
	{
	    fprintf(stderr,"Unknown host: %s\n",host);
	    return -1;
	}
	memcpy(&insock.sin_addr, hostbn->h_addr, hostbn->h_length);
    }
    if (connect(fd,(struct sockaddr *)&insock,sizeof(insock)) == (-1))
    {
	perror("Can't connect to server");
	return -1;
    }
    if (fcntl(fd, F_SETFL, O_NDELAY)==-1) {
	fprintf(stderr,"InitConnection:  Error on fcntl.\n");
    }

#ifdef TCP_NODELAY
    /* turn off nagle algorithm */
    if (use_config[CONFIG_FASTTCP]) {
	int i=1;

	if (setsockopt(fd, SOL_TCP, TCP_NODELAY, &i, sizeof(i)) == -1)
	    perror("TCP_NODELAY");
    }
#endif

    if (getsockopt(fd,SOL_SOCKET,SO_RCVBUF, (char*)&oldbufsize, &buflen)==-1)
        oldbufsize=0;

    if (oldbufsize<newbufsize) {
	if(setsockopt(fd,SOL_SOCKET,SO_RCVBUF, (char*)&newbufsize, sizeof(&newbufsize))) {
            LOG(1,"InitConnection: setsockopt unable to set output buf size to %d\n", newbufsize);
	    setsockopt(fd,SOL_SOCKET,SO_RCVBUF, (char*)&oldbufsize, sizeof(&oldbufsize));
	}
    }
    return fd;
}

/* This function negotiates/establishes the connection with the
 * server.
 */

void negotiate_connection(int sound)
{
    SendVersion(csocket);

    /* We need to get the version command fairly early on because
     * we need to know if the server will support a request to use
     * png images.  This isn't done the best, because if the server
     * never sends the version command, we can loop here forever.
     * However, if it doesn't send the version command, we have no idea
     * what we are dealing with.
     */
    while (csocket.cs_version==0) {
	DoClient(&csocket);
    }

    if (csocket.sc_version<1023) {
	fprintf(stderr,"Server does not support PNG images, yet that is all this client\n");
	fprintf(stderr,"supports.  Either the server needs to be upgraded, or you need to\n");
	fprintf(stderr,"downgrade your client.\n");
	exit(1);
    }

    /* If the user has specified a numeric face id, use it. If it is a string
     * like base, then that resolves to 0, so no real harm in that.
     */
    if (face_info.want_faceset) face_info.faceset = atoi(face_info.want_faceset);
    cs_print_string(csocket.fd,
	    "setup map1acmd 1 sound %d sexp %d darkness %d newmapcmd 1 faceset %d facecache %d itemcmd 2",
	    sound>=0, want_skill_exp, 
		    want_config[CONFIG_FOGWAR], face_info.faceset,
		    want_config[CONFIG_CACHE]);

    use_config[CONFIG_MAPHEIGHT]=want_config[CONFIG_MAPHEIGHT];
    use_config[CONFIG_MAPWIDTH]=want_config[CONFIG_MAPWIDTH];
    if (use_config[CONFIG_MAPHEIGHT]!=11 || use_config[CONFIG_MAPWIDTH]!=11)
	cs_print_string(csocket.fd,"setup mapsize %dx%d",use_config[CONFIG_MAPWIDTH], use_config[CONFIG_MAPHEIGHT]);


    /* If the server will answer the requestinfo for image_info and image_data,
     * send it and wait for the response.
     */
    if (csocket.sc_version >= 1027) {
	/* last_start is -99.  This means the first face requested will
	 * be 1 (not 0) - this is OK because 0 is defined as the blank
	 * face.
	 */
	int last_end=0, last_start=-99;

	cs_print_string(csocket.fd,"requestinfo image_info");
	requestinfo_sent = RI_IMAGE_INFO;
	replyinfo_status = 0;
	replyinfo_last_face = 0;

	do {
	    DoClient(&csocket);

	    /* it's rare, the connection can die while getting
	     * this info.
	     */
	    if (csocket.fd == -1) return;

	    if (use_config[CONFIG_DOWNLOAD]) {
		/* we need to know how many faces to
		 * be able to make the request intelligently.
		 * So only do the following block if we have that info.
		 * By setting the sent flag, we will never exit
		 * this loop until that happens.
		 */
		requestinfo_sent |= RI_IMAGE_SUMS;
		if (face_info.num_images != 0) {
		    /* Sort of fake things out - if we have sent the
		     * request for image sums but have not got them all answered
		     * yet, we then clear the bit from the status
		     * so we continue to loop.
		     */
		    if (last_end == face_info.num_images) {
			/* Mark that we're all done */
			if (replyinfo_last_face == last_end) {
			    replyinfo_status |= RI_IMAGE_SUMS;
			    image_update_download_status(face_info.num_images, face_info.num_images, face_info.num_images);
			}
		    } else {
			/* If we are all caught up, request another
			 * 100 sums.
			 */
			if (last_end == replyinfo_last_face) {
			    last_start += 100;
			    last_end += 100;
			    if (last_end > face_info.num_images) last_end = face_info.num_images;
			    cs_print_string(csocket.fd,"requestinfo image_sums %d %d", last_start, last_end);
			    image_update_download_status(last_start, last_end, face_info.num_images);
			}
		    }
		} /* Still have image_sums request to send */
	    } /* endif download all faces */
	} while (replyinfo_status != requestinfo_sent);
    }
    if (use_config[CONFIG_DOWNLOAD]) {
	char buf[MAX_BUF];

	sprintf(buf,"Download of images complete.  Found %d locally, downloaded %d from server\n",
		face_info.cache_hits, face_info.cache_misses);
	draw_info(buf, NDI_GOLD);
    }

    /* This needs to get changed around - we really don't want to send
     * the SendAddMe until we do all of our negotiation, which may include
     * things like downloading all the images and whatnot - this is more an
     * issue if the user is not using the default face set, as in that case,
     * we might end up building images from the wrong set.
     */
    SendAddMe(csocket);
}


