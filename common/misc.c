/*
 * Crossfire -- cooperative multi-player graphical RPG and adventure game
 *
 * Copyright (c) 1999-2013 Mark Wedel and the Crossfire Development Team
 * Copyright (c) 1992 Frank Tore Johansen
 *
 * Crossfire is free software and comes with ABSOLUTELY NO WARRANTY. You are
 * welcome to redistribute it under certain conditions. For details, please
 * see COPYING and LICENSE.
 *
 * The authors can be reached via e-mail at <crossfire@metalforge.org>.
 */

/**
 * @file common/misc.c
 * Contains misc useful functions that may be useful to various parts of code,
 * but are not especially tied to it.
 */

#include "client.h"
#include <stdarg.h>
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifndef WIN32
#include <sys/wait.h>
#else
#include <direct.h>
#include <io.h>
#endif

/**
 * Convert a buffer of a specified maximum size by replacing token characters
 * with a provided string.  Given a buffered template string "/input/to/edit",
 * the maximum size of the buffer, a token '/', and a replacement string ":",
 * the input string is transformed to ":input:to:edit".  If the replacement
 * string is empty, the token characters are simply removed.  The template is
 * processed from left to right, replacing token characters as they are found.
 * Replacement strings are always inserted whole.  If token replacement would
 * overflow the size of the conversion buffer, the token is not replaced, and
 * the remaining portion of the input string is appended after truncating it
 * as required to avoid overfilling the buffer.
 * @param buffer      A string to perform a find and replace operation on.
 * @param buffer_size Allocated buffer size (used to avoid buffer overflow).
 * @param find        A token character to find and replace in the buffer.
 * @param replace     A string that is to replace each token in the buffer.
 */
void replace_chars_with_string(char*        buffer,
                               const guint16 buffer_size,
                               const char   find,
                               const char*  replace      )
{

    guint16 buffer_len, expand, i, replace_len, replace_limit, template_len;
    char*  template;

    replace_limit = buffer_size - 1;
    replace_len = strlen(replace);
    template_len = strlen(buffer);
    template = g_strdup(buffer);
    buffer[0] = '\0';

    buffer_len = 0;
    for (i = 0; i <= template_len; i++) {
        expand = buffer_len + replace_len < replace_limit ? replace_len : 1;
        if (expand == 1 && buffer_len == replace_limit) {
            break;
        }
        if ((template[i] != find) || ((expand == 1) && (replace_len > 1))) {
            buffer[buffer_len++] = template[i];
            buffer[buffer_len] = '\0';
        } else {
            strcat(buffer, replace);
            buffer_len += replace_len;
        }
    }
    free(template);
}

/**
 * Verifies that the directory exists, creates it if necessary
 * Returns -1 on failure
 */
int make_path_to_dir (char *directory)
{
    char buf[MAX_BUF], *cp = buf;
    struct stat statbuf;

    if (!directory || !*directory) {
        return -1;
    }
    strcpy (buf, directory);
    while ((cp = strchr (cp + 1, (int) '/'))) {
        *cp = '\0';
        if (stat (buf, &statbuf) || !S_ISDIR (statbuf.st_mode)) {
#ifdef WIN32
            if (mkdir (buf)) {
#else
            if (mkdir (buf, 0777)) {
#endif
                perror ("Couldn't make path to file");
                return -1;
            }
        } else {
            *cp = '/';
        }
    }
    /* Need to make the final component */
    if (stat (buf, &statbuf) || !S_ISDIR (statbuf.st_mode)) {
#ifdef WIN32
        if (mkdir (buf)) {
#else
        if (mkdir (buf, 0777)) {
#endif
            perror ("Couldn't make path to file");
            return -1;
        }
    }
    return 0;
}

/**
 * If any directories in the given path doesn't exist, they are created.
 */
int make_path_to_file (char *filename)
{
    char buf[MAX_BUF], *cp = buf;
    struct stat statbuf;

    if (!filename || !*filename) {
        return -1;
    }
    strcpy (buf, filename);
    while ((cp = strchr (cp + 1, (int) '/'))) {
        *cp = '\0';
        if (stat (buf, &statbuf) || !S_ISDIR (statbuf.st_mode)) {
#ifdef WIN32
            if (mkdir (buf)) {
                LOG(LOG_ERROR, "misc.c::make_path_to_file",
                    "Couldn't make path to file: %s", strerror(errno));
#else
            if (mkdir (buf, 0777)) {
                perror ("Couldn't make path to file");
#endif
                return -1;
            }
        }
        *cp = '/';
    }
    return 0;
}

static const char *const LogLevelTexts[] = {
    "\x1b[34;1m" "DD" "\x1b[0m",
    "\x1b[32;1m" "II" "\x1b[0m",
    "\x1b[35;1m" "WW" "\x1b[0m",
    "\x1b[31;1m" "EE" "\x1b[0m",
    "\x1b[31;1m" "!!" "\x1b[0m",
    "\x1b[30;1m" "??" "\x1b[0m",
};

static const char *getLogLevelText(LogLevel level) {
    return LogLevelTexts[level > LOG_CRITICAL ? LOG_CRITICAL + 1 : level];
}

int MINLOG = MINLOGLEVEL;

/**
 * Log messages of a certain importance to stderr. See 'client.h' for a full
 * list of possible log levels.
 */
void LOG(LogLevel level, const char *origin, const char *format, ...) {
    va_list ap;

    /* This buffer needs to be very big - larger than any other buffer. */
    char buf[20480];

    /* Don't log messages that the user doesn't want. */
    if (level < MINLOG) {
        return;
    }

    va_start(ap, format);
    vsnprintf(buf, sizeof(buf), format, ap);

    if (strlen(buf) > 0) {
        fprintf(stderr, "[%s] (%s) %s\n", getLogLevelText(level), origin, buf);
    }

    va_end(ap);
}

ChildProcess* FirstChild=NULL;
ChildProcess* LastChild=NULL;

/**
 *
 */
void purgePipe(ChildProcess* cp, int pipe)
{
    char buf[512];
    int len;
    len=read (cp->tube[pipe],buf,511);
    if (len<1) {
        if (errno==EAGAIN) {
            return;
        }
        LOG(LOG_ERROR,"common::purgePipe","Child %s: could not read from pipe %d!",cp->name?cp->name:"UNKNOWN",pipe);
    }
    if (len>0) {
        char* next;
        char* current=buf;
        buf[len<512?len:511]='\0';
        if (strlen(buf)==0) {
            return;
        }
        for (;;) {
            if (!current) {
                return;
            }
            next=strstr(current,"\n");
            if (next) {
                next[0]='\0';
                next+=strlen("\n");
            }
            LOG(cp->logger[pipe].level,cp->logger[pipe].name,current);
            current=next;
        }
    }
}

/**
 *
 */
void monitorChilds(void)
{
#ifndef WIN32
    ChildProcess* cp=FirstChild;
    ChildProcess* last=NULL;
    for (;;) {
        if (!cp) {
            return;    /*no child to monitor*/
        }
        if (waitpid(cp->pid,NULL,WNOHANG)) {
            ChildProcess* next;

            /*pid is dead*/
            LOG(LOG_INFO,"common::monitorChilds","Child %s died. Removing and closing pipes",cp->name?cp->name:"UNKNOWN");
            if (cp==LastChild) {
                LastChild=last;
            }
            next=cp->next;
            if (last) {
                last->next=next;
            } else {
                FirstChild=cp->next;
            }
            cp=next;
            continue;
        }
        if (cp->logger[1].log) {
            purgePipe(cp,1);
        }
        if (cp->logger[2].log) {
            purgePipe(cp,2);
        }
        last=cp;
        cp=cp->next;
    }
#endif
}

/**
 *
 */
void logPipe(ChildProcess *child, LogLevel level, int pipe)
{
#ifndef WIN32
    char buf[1024];
    if ( (pipe<1) || (pipe>2)) { /*can't log stdin as it's write only*/
        return;
    }
    if (!child->logger[pipe].name) {
        snprintf(buf, sizeof(buf), "Child%d::%s::%d",child->pid,child->name?child->name:"NONAME",pipe);
        child->logger[pipe].name=g_strdup(buf);
    }
    if (fcntl(child->tube[pipe], F_SETFL, O_NDELAY)==-1) {
        LOG(LOG_WARNING,"common::logPipe","Error on fcntl.");
        child->logger[pipe].log=0; /*We don't log it*/
        return;
    }
    child->logger[pipe].log=1; /*We log it*/
    child->logger[pipe].level=level;
#endif
}

/**
 *
 */
void logChildPipe(ChildProcess* child, LogLevel level, int flag)
{
    if (child->flag & flag & CHILD_STDOUT) {
        logPipe(child,level,1);
    }
    if (child->flag & flag & CHILD_STDERR) {
        logPipe(child,level,2);
    }
}

/**
 *
 */
ChildProcess* raiseChild(char* name, int flag)
{
#ifndef WIN32
    ChildProcess* cp;
    int pipe_in[2];
    int pipe_out[2];
    int pipe_err[2];
    int pid;
    char *args;
    LOG(LOG_INFO,"common::raiseChild","Raising %s with flags %d",name,flag);
    flag=flag & (~CHILD_SILENTFAIL);
    if (flag & (~CHILD_TUBE)) {
        LOG(LOG_ERROR,"common::raiseChild",
            "Serious CHILD error, unknown pipe requested: 0x%X for %s",
            flag,name);
        return NULL; /**/
    }
    cp = (ChildProcess*)calloc(1,sizeof(ChildProcess));
    if (cp==NULL) {
        return NULL;    /*No log here, we are out of memory for a few DWORDs, no chance to log*/
    }

    /* Separate name and args */
    args=name;
    while ( *args && *args!=' ' ) {
        ++args;
    }
    while ( *args && *args==' ' ) {
        ++args;
    }
    if ( *args==0 ) {
        args=NULL;
    } else {
        args[-1]=0;
    }
    /*let's pipe a bit*/
    if (flag&CHILD_STDERR)
        if ( pipe(pipe_err) ) {
            LOG(LOG_ERROR,"common::raiseChild","Couldn't create stderr pipe for %s",name);
            free(cp);
            return NULL;
        }
    if (flag&CHILD_STDIN)
        if ( pipe(pipe_in) ) {
            LOG(LOG_ERROR,"common::raiseChild","Couldn't create stdin pipe for %s",name);
            if (flag&CHILD_STDERR) {
                close(pipe_err[0]);
                close(pipe_err[1]);
            }
            free(cp);
            return NULL;
        }
    if (flag&CHILD_STDOUT)
        if ( pipe(pipe_out) ) {
            LOG(LOG_ERROR,"common::raiseChild","Couldn't create stdout pipe for %s",name);
            if (flag&CHILD_STDERR) {
                close(pipe_err[0]);
                close(pipe_err[1]);
            }
            if (flag&CHILD_STDIN) {
                close(pipe_in[0]);
                close(pipe_in[1]);
            }
            free(cp);
            return NULL;
        }

    pid=fork();
    if (pid==-1) { /*failed to fork*/
        LOG(LOG_ERROR,"common::raiseChild","Couldn't create child for %s. Closing pipes",name);
        if (flag&CHILD_STDIN) {
            close(pipe_in[0]);
            close(pipe_in[1]);
        }
        if (flag&CHILD_STDOUT) {
            close(pipe_out[0]);
            close(pipe_out[1]);
        }
        if (flag&CHILD_STDERR) {
            close(pipe_err[0]);
            close(pipe_err[1]);
        }
        free(cp);
        return NULL;
    }
    if (pid==0) { /*we are the child (yeah))*/
        int i;
        int r;
        char *argv[256];

        /* Fill in argv[] */
        argv[0]=name;
        i=1;
        while (args && *args) {
            argv[i]=args;
            ++i;
            while ( *args && *args!=' ' ) {
                ++args;
            }
            if ( *args ) {
                *args=0;
                ++args;
            }
            while ( *args && *args==' ' ) {
                ++args;
            }
        }
        argv[i]=NULL;

        /* Clean up file descriptor space */
        if (flag&CHILD_STDERR) {
            r=dup2(pipe_err[1],2);
            close(pipe_err[0]);
            if ( r != 2 ) {
                /*No call to log, we are the child, don't mess! Console is only soluce.*/
                fprintf(stderr,"common::raiseChild Failed to set pipe_err as stderr\n");
            }
        }
        if (flag&CHILD_STDOUT) {
            r=dup2(pipe_out[1],1);
            close(pipe_out[0]);
            if ( r != 1 ) {
                /*No call to log Father will catch us if he cares of our stderr*/
                fprintf(stderr,"common::raiseChild Failed to set pipe_out as stdout\n");
            }
        }
        if (flag&CHILD_STDIN) {
            r=dup2(pipe_in[0],0);
            close(pipe_in[1]);
            if ( r != 0 ) {
                /*No call to log Father will catch us if he cares of our stderr*/
                fprintf(stderr,"common::raiseChild Failed to set pipe_in as stdin\n");
            }
        }
        for (i=3; i<100; ++i) {
            close(i);
        }

        /* EXEC */
        execvp(argv[0],argv);
        exit(-1); /* Should not be reached */
    }
    /*We are in father here*/
    if (flag&CHILD_STDIN) {
        close (pipe_in[0]); /*close read access to stdin, we are the writer*/
        CHILD_PIPEIN(cp)=pipe_in[1];
    } else {
        CHILD_PIPEIN(cp)=-1;
    }

    if (flag&CHILD_STDOUT) {
        close (pipe_out[1]); /*close write access to stdout, we are the reader*/
        CHILD_PIPEOUT(cp)=pipe_out[0];
    } else {
        CHILD_PIPEOUT(cp)=-1;
    }

    if (flag&CHILD_STDERR) {
        close (pipe_err[1]); /*close write access to stderr, we are the reader*/
        CHILD_PIPEERR(cp)=pipe_err[0];
    } else {
        CHILD_PIPEERR(cp)=-1;
    }
    cp->pid=pid;
    cp->name=g_strdup(name);
    cp->flag=flag;
    /*add to chained list*/
    if (FirstChild) {
        LastChild->next=cp;
    } else {
        FirstChild=cp;
    }
    LastChild=cp;
    cp->next=NULL;
    return cp;
#else
    return NULL;
#endif
}

