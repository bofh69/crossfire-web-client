#ifndef CLIENT_TYPES_H
#define CLIENT_TYPES_H
#include <cconfig.h>


/* If using autoconf, use it to pick up the necessary files.  Otherwise,
 * we will draw on includes.h
 */
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#ifdef HAVE_SYS_TIME_H
#   include <sys/time.h>
#endif

#include <time.h>

#ifdef HAVE_STRING_H
#   include <string.h>
#endif

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#ifdef HAVE_FCNTL_H
#  include <fcntl.h>
#endif

/* Just some handy ones I like to use */
#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif


/* Set of common types used through the program and modules */
typedef unsigned int    uint32;
typedef signed int      sint32;
typedef unsigned short  uint16;
typedef signed short    sint16;
typedef unsigned char   uint8;
typedef signed char     sint8;

#define MAX_BUF 256
#define BIG_BUF 1024

#endif
