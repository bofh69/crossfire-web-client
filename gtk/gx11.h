/*
 * static char *rcsid_gx11_h =
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

typedef struct 
{
  int x;
  int y;
} PlayerPosition;

extern PlayerPosition pl_pos;

extern int fog_of_war;
extern int map_size;
extern int map_image_size, map_image_half_size;
extern int per_pixel_lighting;
extern int per_tile_lighting;
extern int show_grid;
extern uint8 sdlimage,cache_images,split_windows,map_did_scroll,keepcache;
extern uint8 nopopups,updatekeycodes, time_map_redraw;
extern int map_scale, icon_scale, updatelock;

/* Pixmap data.  This is abstracted in the sense that the
 * code here does not care what the data points to (hence the
 * void).  The module using this data should know whether it 
 * is these point to png data or image data of whatever form.
 * The module is not required to use all these fileds - 
 * as png data includes transperancy, it will generally not
 * use the mask fields and instead just put its data into the
 * appropiate image fields.
 *
 * As images can now be of variable size (and potentially re-sized),
 * the size information is stored here.
 */
#define MAXPIXMAPNUM 10000
typedef struct {
    void	*icon_mask, *icon_image;
    uint16	icon_width, icon_height;
    void	*map_mask, *map_image;
    uint16	map_width, map_height;
} PixmapInfo;

extern PixmapInfo pixmaps[MAXPIXMAPNUM];

/* Some global widgetws */
extern GtkWidget    *gtkwin_root,*drawingarea,*run_label,*fire_label;
extern GtkWidget    *gtkwin_info;
extern GtkWidget    *entrytext, *counttext;
extern GdkPixmap    *mapwindow,*dark;
extern GdkBitmap    *dark1, *dark2, *dark3;
extern GdkGC	    *mapgc;
extern GtkWidget *ckentrytext, *ckeyentrytext, *cmodentrytext,*cnumentrytext, *cclist;

#endif
