/*
 * static char *rcsid_script_h =
 *   "$Id$";
 */
/*
    Crossfire client, a client program for the crossfire program.

    Copyright (C) 2003 Mark Wedel & Crossfire Development Team
    This source file also Copyright (C) 2003 Preston Crow

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
#ifndef SCRIPT_H
#define SCRIPT_H

enum CmdFormat {
   ASCII,
   SHORT_ARRAY,
   INT_ARRAY,
   SHORT_INT, /* one short, one int */
   MIXED, /* weird ones like magic map */
   STATS,
   NODATA
};

void script_init(char *params);
void script_list(void);
void script_sync(int cmddiff);
void script_kill(char *params);
void script_fdset(int *maxfd,fd_set *set);
void script_process(fd_set *set);
void script_watch(char *cmd,char *data, int len, enum CmdFormat format);
void script_monitor(const char *command, int repeat, int must_send);
void script_monitor_str(const char *command);
void script_tell(char *params);

#endif /*  SCRIPT_H */
