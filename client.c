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

/* actually declare the globals */

char *server=SERVER,*client_libdir=NULL,*meta_server=META_SERVER;
int port_num=EPORT, meta_port=META_PORT;
FILE *fpin,*fpout;
int fdin, fdout, basenrofpixmaps, pending_images=0,maxfiledescriptor,
	pending_archs=0,maxfd;
Client_Player cpl;
ClientSocket csocket;

char *resists_name[NUM_RESISTS] = {
"armor", "magic", "fire", "elec", 
"cold", "conf", "acid", "drain",
"ghit", "pois", "slow", "para",
"t undead", "fear", "depl","death", 
"hword", "blind"};

typedef void (*CmdProc)(unsigned char *, int len);

struct CmdMapping {
  char *cmdname;
  void (*cmdproc)(unsigned char *, int );
};


struct CmdMapping commands[] = {
    /* Order of this table doesn't make a difference.  I tried to sort
     * of cluster the related stuff together.
     */
    { "map", MapCmd },
    { "map_scroll", (CmdProc)map_scrollCmd },
    { "magicmap", MagicMapCmd},

    { "item", ItemCmd },
    { "item1", Item1Cmd },
    { "upditem", UpdateItemCmd },
    { "delitem", DeleteItem },
    { "delinv",	DeleteInventory },

    { "drawinfo", (CmdProc)DrawInfoCmd },
    { "stats", StatsCmd },

    { "pixmap", PixMapCmd },
    { "bitmap", BitMapCmd },
    { "image", ImageCmd },
    { "face", FaceCmd},


    { "sound", SoundCmd},
    { "anim", AnimCmd},

    { "player", PlayerCmd },
    { "comc", CompleteCmd},

    { "addme_failed", (CmdProc)AddMeFail },
    { "addme_success", (CmdProc)AddMeSuccess },
    { "version", (CmdProc)VersionCmd },
    { "goodbye", (CmdProc)GoodbyeCmd },

    { "query", (CmdProc)handle_query},
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
	    fprintf(stderr,"Got error on read (error %d)\n", errno);
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
	LOG(0,"Command:%s\n",csocket->inbuf.buf+2);
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

    int cache;

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


    cache = display_willcache();
    if (cache) cache = CF_FACE_CACHE;

    if (display_usebitmaps()) 
	SendSetFaceMode(csocket,CF_FACE_BITMAP | cache); 
    else if (display_usexpm()) 
	SendSetFaceMode(csocket,CF_FACE_XPM | cache);
    else if (display_usepng()) {
	if (csocket.sc_version<1023) {
	    SendSetFaceMode(csocket,CF_FACE_XPM | cache);
	    draw_info("Server does not support PNG images.  Will use XPM instead", NDI_RED);
	}
	else SendSetFaceMode(csocket,CF_FACE_PNG | cache);
    }

    if (sound<0)
	cs_write_string(csocket.fd,"setsound 0", 10);
    else
	cs_write_string(csocket.fd,"setsound 1", 10);

    SendAddMe(csocket);
}


int main(int argc, char *argv[])
{
    int sound,got_one=0;

    /* This needs to be done first.  In addition to being quite quick,
     * it also sets up some paths (client_libdir) that are needed by
     * the other functions.
     */

    init_client_vars();
    
    /* Call this very early.  It should parse all command
     * line arguments and set the pertinent ones up in
     * globals.  Also call it early so that if it can't set up
     * the windowing system, we get an error before trying to
     * to connect to the server.  And command line options will
     * likely change on the server we connect to.
     */
    if (init_windows(argc, argv)) {	/* x11.c */
	fprintf(stderr,"Failure to init windows.\n");
	exit(1);
    }
    csocket.inbuf.buf=malloc(MAXSOCKBUF);

#ifdef HAVE_SYSCONF
    maxfd = sysconf(_SC_OPEN_MAX);
#else
    maxfd = getdtablesize();
#endif

    sound = init_sounds();

    /* Loop to connect to server/metaserver and play the game */
    while (1) {
	reset_client_vars();
	csocket.inbuf.len=0;
	csocket.cs_version=0;

	/* Perhaps not the best assumption, but we are taking it that
	 * if the player has not specified a server (ie, server
	 * matches compiled in default), we use the meta server.
	 * otherwise, use the server provided, bypassing metaserver.
	 * Also, if the player has already played on a server once (defined
	 * by got_one), go to the metaserver.  That gives them the oppurtunity
	 * to quit the client or select another server.  We should really add
	 * an entry for the last server there also.
	 */

	if (!strcmp(server, SERVER) || got_one) {
	    char *ms;
	    metaserver_get_info(meta_server, meta_port);
	    metaserver_show();
	    do {
		ms=get_metaserver();
	    } while (metaserver_select(ms));
	    negotiate_connection(sound);
	} else {
	    csocket.fd=init_connection(server, port_num);
	    negotiate_connection(sound);
	}

	got_one=1;
	event_loop();
	/* if event_loop has exited, we most of lost our connection, so we
	 * loop again to establish a new one.
	 */

	/* Need to reset the images so they match up properly and prevent
	 * memory leaks.
	 */
	reset_image_data();
    }
    exit(0);	/* never reached */
}
