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

/* Specifies the font to use.  This isn't retally supported, as there is
 * no way in changing images in the font - this then requires that the
 * client has the exact same font the server is using (which is
 * possible if the server site is also running a font server.
 */
#define FONTNAME "crossfire"
#define FONTDIR "/usr/lib/X11R6/lib/fonts/misc/"


extern int errno;
