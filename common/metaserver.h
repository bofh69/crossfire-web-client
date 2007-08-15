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
#define MS_SMALL_BUF	60
#define MS_LARGE_BUF	512

/**
 * Structure that contains data we get from metaservers
 * This is used by both metaserver1 and metaserver2
 * support - fields used by only one metaserver type
 * or the other are noted with MS1 or MS2 comments.
 * Note that the client doesn't necessary do anything with
 * all of these fields, but might as well store them around
 * just in case
 */
typedef struct Meta_Info {
    char    ip_addr[MS_SMALL_BUF];	/* MS1 */
    char    hostname[MS_LARGE_BUF];	/* MS1 & MS2 */
    int	    port;			/* MS2 - port server is on */
    char    html_comment[MS_LARGE_BUF];	/* MS2 */
    char    text_comment[MS_LARGE_BUF];	/* MS1 & MS2 - for MS1, presumed */
					/* all comments are text */
    char    archbase[MS_SMALL_BUF];	/* MS2 */
    char    mapbase[MS_SMALL_BUF];	/* MS2 */
    char    codebase[MS_SMALL_BUF];	/* MS2 */
    char    flags[MS_SMALL_BUF];	/* MS2 */
    int	    num_players;		/* MS1 & MS2 */
    uint32  in_bytes;			/* MS2 */
    uint32  out_bytes;			/* MS2 */
    int	    idle_time;			/* MS1 - for MS2, calculated from */
					/* last_update value */
    int	    uptime;			/* MS2 */
    char    version[MS_SMALL_BUF];	/* MS1 & MS2 */
    int	    sc_version;			/* MS2 */
    int	    cs_version;			/* MS2 */
} Meta_Info;

extern Meta_Info *meta_servers;

/* Before accessing the metaservers structure,
 * a lock against this is needed
 */
extern pthread_mutex_t ms2_info_mutex;

/* Needs to be here because gtk2 client needs to resort for example */
static int meta_sort(Meta_Info *m1, Meta_Info *m2) { return strcasecmp(m1->hostname, m2->hostname); }

extern int meta_numservers;

extern int cached_servers_num;

#define CACHED_SERVERS_MAX  10
extern char* cached_servers_name[ CACHED_SERVERS_MAX ];
extern char* cached_servers_ip[ CACHED_SERVERS_MAX ];
extern const char* cached_server_file;
