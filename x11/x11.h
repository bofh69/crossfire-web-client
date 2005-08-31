/*
 * static char *rcsid_x11_h =
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

#ifndef GX11_H
#define GX11_H

#include "client-types.h"

extern int map_size;
extern uint8 updatekeycodes;
extern int updatelock;

struct PixmapInfo {
  Pixmap pixmap,mask;
  uint8 width, height;		/* size of pixmap in tiles */
};

extern struct PixmapInfo *pixmaps[MAXPIXMAPNUM];
extern Display *display;
extern uint8   image_size;
extern Window win_root,win_game;
extern GC gc_game;
extern Colormap colormap;
extern Window win_stats,win_message;


#endif
