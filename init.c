
/* This handles the initialization of the client.  This includes making
 * the I_IMAGE and I_ARCH commands.
 */

#include <client.h>


void VersionCmd(char *data, int len)
{
    char *cp;

    csocket.cs_version = atoi(data);
    /* set sc_version in case it is an old server supplying only one version */
    csocket.sc_version = csocket.cs_version;
    if (csocket.cs_version != atoi(data)) {
	fprintf(stderr,"Differing C->S version numbers (%d,%d)\n",
	   VERSION_CS,csocket.cs_version);
/*	exit(1);*/
    }
    cp = strchr(data,' ');
    if (!cp) return;
    csocket.sc_version = atoi(cp);
    if (csocket.sc_version != VERSION_SC) {
	fprintf(stderr,"Differing S->C version numbers (%d,%d)\n",
	   VERSION_SC,csocket.sc_version);
    }
    cp = strchr(cp+1, ' ');
    if (cp)
	fprintf(stderr,"Playing on server type %s\n", cp);
}

void SendVersion(ClientSocket csock)
{
    char buf[MAX_BUF];

    sprintf(buf,"version %d %d %s", VERSION_CS, VERSION_SC, VERSION_INFO);
    cs_write_string(csock.fd, buf, strlen(buf));
}


void SendAddMe(ClientSocket csock)
{

    cs_write_string(csock.fd, "addme",5);
}


void SendSetFaceMode(ClientSocket csock,int mode)
{
    char buf[MAX_BUF];

    sprintf(buf,"setfacemode %d", mode);
    cs_write_string(csock.fd, buf, strlen(buf));
}


void init_client_vars()
{
    int i;


    /* I think environemental variables should be more important than
     * compiled in defaults, so these probably should be reversed. 
     */
    client_libdir=getenv("CFCLIENT_LIBDIR");
#ifdef CLIENT_LIBDIR
    if (client_libdir==NULL)
	client_libdir=CLIENT_LIBDIR;
#endif

    cpl.count_left = 0;
    cpl.container = NULL;
    memset(&cpl.stats,0, sizeof(Stats));
    cpl.stats.maxsp=1;	/* avoid div by 0 errors */
    cpl.stats.maxhp=1;	/* ditto */
    cpl.stats.maxgrace=1;	/* ditto */
    /* ditto - displayed weapon speed is weapon speed/speed */
    cpl.stats.speed=1;
    cpl.input_text[0]='\0';
    cpl.title[0] = '\0';
    cpl.range[0] = '\0';
    cpl.last_command[0] = '\0';

    for (i=0; i<range_size; i++)
	cpl.ranges[i]=NULL;

    cpl.map_x=0;
    cpl.map_y=0;

    cpl.ob = player_item();
    cpl.below = map_item();
    cpl.magicmap=NULL;
    cpl.showmagic=0;
    cpl.command_window = COMMAND_WINDOW;

    csocket.command_sent=0;
    csocket.command_received=0;
    csocket.command_time=0;

}

/* This is used to clear values between connections to different
 * servers.  This needs to be called after init_client_vars has
 * been called because it does not re-allocated some values.
 */

void reset_client_vars()
{
    int i;


    cpl.count_left = 0;
    cpl.container = NULL;
    memset(&cpl.stats,0, sizeof(Stats));
    cpl.stats.maxsp=1;	/* avoid div by 0 errors */
    cpl.stats.maxhp=1;	/* ditto */
    cpl.stats.maxgrace=1;	/* ditto */
    /* ditto - displayed weapon speed is weapon speed/speed */
    cpl.stats.speed=1;
    cpl.input_text[0]='\0';
    cpl.title[0] = '\0';
    cpl.range[0] = '\0';
    cpl.last_command[0] = '\0';

    for (i=0; i<range_size; i++)
	cpl.ranges[i]=NULL;

    cpl.map_x=0;
    cpl.map_y=0;

    cpl.magicmap=NULL;
    cpl.showmagic=0;

    csocket.command_sent=0;
    csocket.command_received=0;
    csocket.command_time=0;

}
