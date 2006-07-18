/*
 * static char *rcsid_commands_c =
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

/* This contains various configuration options.  Most all of thse
 * can be overridden via command line options, but setting them
 * here can provide nice defaults.
 */

/* This is how often the client checks for X events, as well as how often
 * it performs animations (or will).  This value can be most anything.
 * IT is only configurable because the exact value it needs to be set to
 * has to be figured out.  This value is in microseconds (100,000 microseconds=
 * 0.1 seconds
 */

#define MAX_TIME 100000


/* This is the default port to connect to the server with. */
#define EPORT 13327

/* This is the default port to connect to the server with in string form. */
#define DEFPORT "13327"

/* Set to default server you want the client to connect to.  This can
 * be especially useful if your installing the client binary on a LAN
 * and want people to just be able to run it without options and connect
 * to some server.  localhost is the default.  Remember to use double
 * quotes around your server name.
 * Comment this out - by default, things connect to metaserver so
 * this is normally ignored in any case.  If this is not commented out,
 * the the client will automatically try to connect to this - useful
 * if inside a firewall and have a local server.  Using -server ""
 * can then bypass this setting
 */

/* #define SERVER "localhost" */

/* Server to contact to get information about crossfire servers.
 * This is not the server you play on, but rather a central repository
 * that lists the servers.
 * METASERVER controls default behaviour (same as -metaserver options) -
 * if set to TRUE, we try to get metaserver information, if false, we do
 * not.  If you are behind a firewall, you probably want this off by
 * default.
 */

#define META_SERVER "crossfire.real-time.com"
#define META_PORT   13326
#define METASERVER  TRUE

/* If you uncomment this, the gtk client will dump information about
 * how long it took to update the display.  If your having
 * performance problems, this may be a more useful way to see
 * what your performance really is like.  The data will be dumped
 * to stderr, with timings in microseconds.  A general rule of thumb
 * is you want the update times to be less than 100,000 microseconds
 */
/*
#define TIME_MAP_REDRAW
*/
