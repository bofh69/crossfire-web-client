/**
 * @file
 * Client type definitions.
 */

#ifndef CLIENT_TYPES_H
#define CLIENT_TYPES_H

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>

#ifdef HAVE_SYS_TIME_H
#   include <sys/time.h>
#endif

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#ifdef HAVE_FCNTL_H
#  include <fcntl.h>
#endif

#include <glib.h>

#ifdef WIN32
#  include <winsock2.h>
#endif

#ifdef HAVE_PTHREAD_H
#  include <pthread.h>
#endif

#ifdef WIN32
/* mingw defines rand and srand only */
#ifndef random
#define random rand
#endif
#ifndef srandom
#define srandom srand
#endif

#ifndef sleep
#define sleep(x) Sleep((x)*1000)
#endif
#endif

#ifndef SOL_TCP
#define SOL_TCP IPPROTO_TCP
#endif

#define MAX_BUF 256
#define BIG_BUF 1024

/* used to register gui callbacks to extended texts
 * (which are supposed to be handled more friendly than raw text)*/
typedef void (*ExtTextManager)(int flag, int type, int subtype, char* message);

typedef struct TextManager{
    int type;
    ExtTextManager callback;
    struct TextManager* next;
} TextManager;

#endif
