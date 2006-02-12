/* char *rcsid_common_metaserver_h =
 *   "$Id$";
 */

/*
    Crossfire client, a client program for the crossfire program.

    Copyright (C) 2005 Mark Wedel & Crossfire Development Team

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


/* Arbitrary size.  At some point, we would need to cut this off simply
 * for display/selection reasons.
 */
#define MAX_METASERVER 100

/* Various constants we use in the structure */
#define MS_SMALL_BUF	20
#define MS_LARGE_BUF	256

typedef struct Meta_Info {
    char    ip_addr[MS_SMALL_BUF];
    int	    idle_time;
    char    hostname[MS_LARGE_BUF];
    int	    players;
    char    version[MS_SMALL_BUF];
    char    comment[MS_LARGE_BUF];
} Meta_Info;


extern Meta_Info *meta_servers;

extern int meta_numservers;

extern int cached_servers_num;

#define CACHED_SERVERS_MAX  10
extern char* cached_servers_name[ CACHED_SERVERS_MAX ];
extern char* cached_servers_ip[ CACHED_SERVERS_MAX ];
extern const char* cached_server_file;
