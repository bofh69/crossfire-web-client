/* This is a config file for the client.  Many of the options are
 * taken from the standard config.h file.  However, a few new ones unique
 * to the client are here.  Also, if the client ever became a totally
 * seperate distibution, this would be needed in any case.
 */


/* X_PROG_NAME is the name that is used to read X resources. */
#define X_PROG_NAME "cfclient"

/* Directory to store cached images (only makes a difference if run
 * with the -cache option.)  If not set, then we store in ~/.crossfire/images
 * This should be good enough for most people.  However, if you have a site
 * with many people running the client, you might want to make a standard
 * depository (note that permissions on the repository might need to be
 * pretty wide open.
 */

/*#define IMAGECACHEDIR "/tmp" */

/* This is how often the client checks for X events, as well as how often
 * it performs animations (or will).  This value can be most anything.
 * IT is only configurable because the exact value it needs to be set to
 * has to be figured out.  This value is in microseconds (100,000 microseconds=
 * 0.1 seconds
 */

#define MAX_TIME 100000


/* This is the default port to connect to the server with. */
#define EPORT 13327

/* This is the default port to connect to the server with in string form. */
#define DEFPORT "13327"

/* Set to default display mode you want (one of Pix_Display, Xpm_Display, or
 * Png_Display).
 */
#define DISPLAY_MODE Xpm_Display

/* Set to default server you want the client to connect to.  This can
 * be especially useful if your installing the client binary on a LAN
 * and want people to just be able to run it without options and connect
 * to some server.  localhost is the default.  Remember to use double
 * quotes around your server name.
 */

#define SERVER "localhost"

/* Server to contact to get information about crossfire servers.
 * This is not the server you play on, but rather a central repository
 * that lists the servers.
 * 
 */

#define META_SERVER "crossfire.real-time.com"
#define META_PORT   13326
