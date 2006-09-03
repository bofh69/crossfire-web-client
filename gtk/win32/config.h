/* win32/config.h hand tweaked defines and modified in other ways to work with Visual Studio */
#ifndef __CROSSFIRE_CLIENT_CONFIG
#define __CROSSFIRE_CLIENT_CONFIG

/* common/config.h.  Generated automatically by configure.  */
/* config.h.in.  Generated automatically from configure.in by autoheader 2.13.  */

/* Define to empty if the keyword does not work.  */
/* #undef const */

/* Define if you don't have vprintf but do have _doprnt.  */
/* #undef HAVE_DOPRNT */

/* Define if you have the vprintf function.  */
#define HAVE_VPRINTF 1

/* Define if you have the ANSI C header files.  */
#define STDC_HEADERS 1

/* Define if you can safely include both <sys/time.h> and <time.h>.  */
#define TIME_WITH_SYS_TIME 1

/* Define if your processor stores words with the most significant
   byte first (like Motorola and SPARC, unlike Intel and VAX).  */
/* #undef WORDS_BIGENDIAN */

/* Define if the X Window System is missing or not being used.  */
/* #undef X_DISPLAY_MISSING */

/* #undef ENABLE_NLS */
/* #undef HAVE_CATGETS */
/* #undef HAVE_GETTEXT */
/* #undef HAVE_LC_MESSAGES */
/* #undef HAVE_STPCPY */
/* #undef HAVE_LIBSM */
/* #undef HAVE_LIBXPM */

/* Define if you have the mkdir function.  */
#define HAVE_MKDIR 1

/* Define if you have the socket function.  */
#define HAVE_SOCKET 1

/* Define if you have the strcspn function.  */
#define HAVE_STRCSPN 1

/* Define if you have the sysconf function.  */
/* #define HAVE_SYSCONF 1 */

/* Define if you have the <X11/SM/SMlib.h> header file.  */
/* #undef HAVE_X11_SM_SMLIB_H */

/* Define if you have the <dmalloc.h> header file.  */
/* #undef HAVE_DMALLOC_H */

/* Define if you have the <fcntl.h> header file.  */
#define HAVE_FCNTL_H 1

/* Define if you have the <string.h> header file.  */
#define HAVE_STRING_H 1

/* Define if you have the <sys/ioctl.h> header file.  */
#define HAVE_SYS_IOCTL_H 1

/* Define if you have the <sys/select.h> header file.  */
#define HAVE_SYS_SELECT_H 1

/* Define if you have the <sys/time.h> header file.  */
/* #define HAVE_SYS_TIME_H 1 */

/* Define if you have the <unistd.h> header file.  */
/* #define HAVE_UNISTD_H 1 */

/* Define if you have the m library (-lm).  */
#define HAVE_LIBM 1

/* Define if you have the png library (-lpng).  */
#define HAVE_LIBPNG 1

//#define HAVE_SDL 1

/* The size of a `long', as computed by sizeof. */
#define SIZEOF_LONG 8

/* The size of a `long long', as computed by sizeof. */
#undef SIZEOF_LONG_LONG

/* Name of package */
#define PACKAGE "gcfclient"

/* Version number of package */
#define VERSION "1.9.1 (snapshot)"

/***********************/
#ifdef WIN32
/* Define Win32 specific stuff. */

/* Exclude rarely-used stuff from Windows headers */
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <winbase.h>

#define BINDIR "."
#define DATADIR "."

#define KeySym guint

#define R_OK 04
#define W_OK 02
#define X_OK 04
#define F_OK 00

#define None NULL

#define access(x,y) _access(x,y)

/* Function prototypes */
void gettimeofday(struct timeval *tv, void* unused);
int strcasecmp(const char *s1, const char*s2);
int strncasecmp(const char *s1, const char *s2, int n);

/* Sleep(x) [Win32] sleeps x milliseconds.  sleep(x) [Unix] sleeps x seconds */
#define usleep(x) Sleep((x)/1000)
#define sleep(x) Sleep((x)*1000)
#define S_ISDIR(x) (((x) & S_IFMT) == S_IFDIR)
#define srandom srand
#define random rand

#define CFGTK2
/* For GTK 2.2 Compatibility */
#define GTK_WINDOW_DIALOG GTK_WINDOW_TOPLEVEL
#define GC GdkGC

#define snprintf _snprintf

#define MINLOGLEVEL 0

#endif /* WIN32 */

#ifdef _MSC_VER
/* Ignore spurious warnings */
#pragma warning(disable: 4018) /* signed/unsigned mismatch */
#pragma warning(disable: 4244) /* conversion from 'x' to 'x', possible loss of data */
#pragma warning(disable: 4761) /* integral size mismatch in argument; conversion supplied */
#endif

#endif /* __CROSSFIRE_CLIENT_CONFIG */
