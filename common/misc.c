/*
 * static char *rcsid_misc_c =
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

/*
 * static char *rcsid_misc_c =
 *   "$Id$";
 */


/* Contains misc useful functions that may be useful to various parts
 * of code, but are not especially tied to it.
 */

#include "client.h"
#include <stdarg.h>

#include <sys/stat.h>


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
	    if (mkdir (buf, 0777)) {
		perror ("Couldn't make path to file");
		return -1;
	    }
	} else
	    *cp = '/';
    }
    /* Need to make the final component */
    if (stat (buf, &statbuf) || !S_ISDIR (statbuf.st_mode)) {
	if (mkdir (buf, 0777)) {
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
	    if (mkdir (buf, 0777)) {
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

char *strdup_local(char *str) {
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
void clearLogListener(){
    loglist=NULL;
}
static char* LogLevelTexts[]={" DEBUG  ",
                              "  INFO  ",
                              "WARNING ",
                              " ERROR  ",
                              "CRITICAL",
                              "UNKNOWN "};
static inline char * getLogLevelText(LogLevel level){
    return LogLevelTexts[level>LOG_CRITICAL?LOG_CRITICAL+1:level];
}
char* getLogTextRaw(LogLevel level, char* origin, char*message){
    static char mybuf[20480];
    mybuf[0]='\0';
    sprintf(mybuf,"[%s] (%s) %s\n",getLogLevelText(level),origin,message);
    return mybuf;
}

char* getLogText(LogEntry* le){
    return getLogTextRaw(le->level,le->origin,le->message);
}
/*
 * Logs a message to stderr and save it in memory.
 * Or discards the message if it is of no importanse, and none have
 * asked to hear messages of that logLevel.
 *
 * See client.h for possible logLevels.
 */
#ifdef DEBUG
#define MINLOG 0
#else
#define MINLOG 1
#endif
void LOG (LogLevel level, char* origin, char *format, ...)
{
  if (level<MINLOG)
    return;
  char buf[20480];  /* This needs to be really really big - larger
		     * than any other buffer, since that buffer may
		     * need to be put in this one.
		     */

  va_list ap;
  va_start(ap, format);

  buf[0] = '\0';
  vsprintf(buf, format, ap);
  //fprintf(stderr,getLogTextRaw(level,origin,buf));
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


