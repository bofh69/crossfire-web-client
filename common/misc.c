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


