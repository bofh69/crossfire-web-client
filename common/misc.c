const char *rcsid_common_misc_c =
    "$Id$";
/*
    Crossfire client, a client program for the crossfire program.

    Copyright (C) 2006,2001 Mark Wedel & Crossfire Development Team

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

/*
 * static char *rcsid_misc_c =
 *   "$Id$";
 */


/* Contains misc useful functions that may be useful to various parts
 * of code, but are not especially tied to it.
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

/*
 * Verifies that the directory exists, creates it if necessary
 * Returns -1 on failure
 */

int make_path_to_dir (char *directory)
{
    char buf[MAX_BUF], *cp = buf;
    struct stat statbuf;

    if (!directory || !*directory)
	return -1;
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
	} else
	    *cp = '/';
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


/*
 * If any directories in the given path doesn't exist, they are created.
 */

int make_path_to_file (char *filename)
{
    char buf[MAX_BUF], *cp = buf;
    struct stat statbuf;

    if (!filename || !*filename)
	return -1;
    strcpy (buf, filename);
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
	} 
	*cp = '/';
    }
    return 0;
}
/*
 * A replacement of strdup(), since it's not defined at some
 * unix variants.
 */

char *strdup_local(const char *str) {
  char *c=(char *)malloc(sizeof(char)*strlen(str)+1);
  strcpy(c,str);
  return c;
}


/* logging stuff */
LogEntry* LogFirst=NULL;
LogEntry* LogLast=NULL;
LogListener loglist=NULL;
int setLogListener(LogListener li){
    if (loglist)
        return 0;
    loglist=li;
    return 1;
}
void clearLogListener(void) {
    loglist=NULL;
}
static const char *const LogLevelTexts[] = {
    " DEBUG  ",
    "  INFO  ",
    "WARNING ",
    " ERROR  ",
    "CRITICAL",
    "UNKNOWN ",
};
static const char *getLogLevelText(LogLevel level) {
    return LogLevelTexts[level>LOG_CRITICAL?LOG_CRITICAL+1:level];
}
char *getLogTextRaw(LogLevel level, const char *origin, const char *message) {
    static char mybuf[20480];
    mybuf[0]='\0';
    sprintf(mybuf,"[%s] (%s) %s\n",getLogLevelText(level),origin,message);
    return mybuf;
}

char *getLogText(const LogEntry *le) {
    return getLogTextRaw(le->level,le->origin,le->message);
}
/*
 * Logs a message to stderr and save it in memory.
 * Or discards the message if it is of no importanse, and none have
 * asked to hear messages of that logLevel.
 *
 * See client.h for possible logLevels.
 */
int MINLOG=MINLOGLEVEL;



void LOG(LogLevel level, const char *origin, const char *format, ...)
{

  va_list ap;
  static char buf[20480];  /* This needs to be really really big - larger
		     * than any other buffer, since that buffer may
		     * need to be put in this one.
		     */
  if (level<MINLOG)
    return;

  va_start(ap, format);

  buf[0] = '\0';
  vsprintf(buf, format, ap);
  /*fprintf(stderr,getLogTextRaw(level,origin,buf));*/
  if (strlen(buf)>0){
    LogEntry *le = LOG_NEW_ENTRY;
    LOG_APPEND(le);
    LOG_SETMESSAGE(le,buf);
    LOG_SETORIGIN(le,origin);
    le->level=level;
    fprintf(stderr,getLogText(le));
    if (loglist)
        (*loglist)(le);
  }
  va_end(ap);
}

ChildProcess* FirstChild=NULL;
ChildProcess* LastChild=NULL;

void purgePipe(ChildProcess* cp, int pipe){
    char buf[512];
    int len;
    len=read (cp->tube[pipe],buf,511);
    if (len<1){
        if (errno==EAGAIN)
            return;
        LOG(LOG_ERROR,"common::purgePipe","Child %s: could not read from pipe %d!",cp->name?cp->name:"UNKNOWN",pipe);
    }
    if (len>0){
        char* next;
        char* current=buf;
        buf[len<512?len:511]='\0';
        if (strlen(buf)==0)
            return;
        for (;;){
            if (!current)
                return;
            next=strstr(current,"\n");
            if (next){
                next[0]='\0';
                next+=strlen("\n");
            }
            LOG(cp->logger[pipe].level,cp->logger[pipe].name,current);
            current=next;
        }
    }
}

void monitorChilds(void) {
#ifndef WIN32
    ChildProcess* cp=FirstChild;
    ChildProcess* last=NULL;
    for (;;){
        if (!cp)
            return; /*no child to monitor*/
        if (waitpid(cp->pid,NULL,WNOHANG)){
            ChildProcess* next;

            /*pid is dead*/
            LOG(LOG_INFO,"common::monitorChilds","Child %s died. Removing and closing pipes",cp->name?cp->name:"UNKNOWN");
            if (cp==LastChild)
                LastChild=last;
            next=cp->next;
            if (last)
                last->next=next;
            else
                FirstChild=cp->next;
            cp=next;
            continue;
        }
        if (cp->logger[1].log)
            purgePipe(cp,1);
        if (cp->logger[2].log)
            purgePipe(cp,2);
        last=cp;
        cp=cp->next;
    }
#endif
}

void logPipe(ChildProcess *child, LogLevel level, int pipe){
#ifndef WIN32
    char buf[1024];
    if ( (pipe<1) || (pipe>2))/*can't log stdin as it's write only*/
        return;
    if (!child->logger[pipe].name){
        sprintf(buf,"Child%d::%s::%d",child->pid,child->name?child->name:"NONAME",pipe);
        child->logger[pipe].name=strdup(buf);
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

void logChildPipe(ChildProcess* child, LogLevel level, int flag){
    if (child->flag & flag & CHILD_STDOUT)
        logPipe(child,level,1);
    if (child->flag & flag & CHILD_STDERR)
        logPipe(child,level,2);
}

ChildProcess* raiseChild(char* name, int flag){
#ifndef WIN32
    ChildProcess* cp;
    int pipe_in[2];
    int pipe_out[2];
    int pipe_err[2];
    int pid;
    LogLevel deferror;
    char *args;
    deferror=(flag & CHILD_SILENTFAIL)?LOG_INFO:LOG_ERROR;
    LOG(LOG_INFO,"common::raiseChild","Raising %s with flags %d",name,flag);
    flag=flag & (~CHILD_SILENTFAIL);
    if (flag & (~CHILD_TUBE)){
        LOG(LOG_ERROR,"common::raiseChild",
                "Serious CHILD error, unknown pipe requested: 0x%X for %s",
                flag,name);
        return NULL; /**/
    }
    cp = (ChildProcess*)calloc(1,sizeof(ChildProcess));
    if (cp==NULL)
        return NULL; /*No log here, we are out of memory for a few DWORDs, no chance to log*/

    /* Separate name and args */
    args=name;
    while ( *args && *args!=' ' ) ++args;
    while ( *args && *args==' ' ) ++args;
    if ( *args==0 )
        args=NULL;
    else
        args[-1]=0;
    /*let's pipe a bit*/
    if (flag&CHILD_STDERR)
        if ( pipe(pipe_err) ){
            LOG(LOG_ERROR,"common::raiseChild","Couldn't create stderr pipe for %s",name);
            free(cp);
            return NULL;
        }
    if (flag&CHILD_STDIN)
        if ( pipe(pipe_in) ){
            LOG(LOG_ERROR,"common::raiseChild","Couldn't create stdin pipe for %s",name);
            if (flag&CHILD_STDERR){
                close(pipe_err[0]);
                close(pipe_err[1]);
            }
            free(cp);
            return NULL;
        }
    if (flag&CHILD_STDOUT)
        if ( pipe(pipe_out) ){
            LOG(LOG_ERROR,"common::raiseChild","Couldn't create stdout pipe for %s",name);
            if (flag&CHILD_STDERR){
                close(pipe_err[0]);
                close(pipe_err[1]);
            }
            if (flag&CHILD_STDIN){
                close(pipe_in[0]);
                close(pipe_in[1]);
            }
            free(cp);
            return NULL;
        }

    pid=fork();
    if (pid==-1){/*failed to fork*/
        LOG(LOG_ERROR,"common::raiseChild","Couldn't create child for %s. Closing pipes",name);
        if (flag&CHILD_STDIN){
            close(pipe_in[0]);
            close(pipe_in[1]);
        }
        if (flag&CHILD_STDOUT){
            close(pipe_out[0]);
            close(pipe_out[1]);
        }
        if (flag&CHILD_STDERR){
            close(pipe_err[0]);
            close(pipe_err[1]);
        }
        free(cp);
        return NULL;
    }
    if (pid==0){ /*we are the child (yeah))*/
        int i;
        int r;
        char *argv[256];

        /* Fill in argv[] */
        argv[0]=name;
        i=1;
        while (args && *args)
        {
            argv[i]=args;
            ++i;
            while ( *args && *args!=' ' ) ++args;
            if ( *args )
            {
                *args=0;
                ++args;
            }
            while ( *args && *args==' ' ) ++args;
        }
        argv[i]=NULL;

        /* Clean up file descriptor space */
        if (flag&CHILD_STDERR){
            r=dup2(pipe_err[1],2);
            close(pipe_err[0]);
            if ( r != 2 ) {
                /*No call to log, we are the child, don't mess! Console is only soluce.*/
                fprintf(stderr,"common::raiseChild Failed to set pipe_err as stderr\n");
            }
        }
        if (flag&CHILD_STDOUT){
            r=dup2(pipe_out[1],1);
            close(pipe_out[0]);
            if ( r != 1 ) {
                /*No call to log Father will catch us if he cares of our stderr*/
                fprintf(stderr,"common::raiseChild Failed to set pipe_out as stdout\n");
            }
        }
        if (flag&CHILD_STDIN){
            r=dup2(pipe_in[0],0);
            close(pipe_in[1]);
            if ( r != 0 ) {
                /*No call to log Father will catch us if he cares of our stderr*/
                fprintf(stderr,"common::raiseChild Failed to set pipe_in as stdin\n");
            }
        }
        for (i=3;i<100;++i) close(i);

        /* EXEC */
        execvp(argv[0],argv);
        exit(-1); /* Should not be reached */
    }
    /*We are in father here*/
    if (flag&CHILD_STDIN){
        close (pipe_in[0]); /*close read access to stdin, we are the writer*/
        CHILD_PIPEIN(cp)=pipe_in[1];
    } else
        CHILD_PIPEIN(cp)=-1;

    if (flag&CHILD_STDOUT){
        close (pipe_out[1]); /*close write access to stdout, we are the reader*/
        CHILD_PIPEOUT(cp)=pipe_out[0];
    } else
        CHILD_PIPEOUT(cp)=-1;

    if (flag&CHILD_STDERR){
        close (pipe_err[1]); /*close write access to stderr, we are the reader*/
        CHILD_PIPEERR(cp)=pipe_err[0];
    } else
        CHILD_PIPEERR(cp)=-1;
    cp->pid=pid;
    cp->name=strdup(name);
    cp->flag=flag;
    /*add to chained list*/
    if (FirstChild)
        LastChild->next=cp;
    else
        FirstChild=cp;
    LastChild=cp;
    cp->next=NULL;
    return cp;
#else
    return NULL;
#endif
}
