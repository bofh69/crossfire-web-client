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

/* gtk */
#include <gtk/gtk.h>
#ifndef WIN32
#include <gdk/gdkx.h>
#else
#include <gdk/gdkwin32.h>
#endif
#include <gdk/gdkkeysyms.h>

#include "client-types.h"
#include "item.h"

extern int map_size, image_size, map_image_size, map_image_half_size;
extern uint8 updatekeycodes, time_map_redraw,redraw_needed;
extern int updatelock;

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
#define DEFAULT_IMAGE_SIZE	32
#define MAXPIXMAPNUM 10000
typedef struct {
    void	*icon_mask, *icon_image;
    uint16	icon_width, icon_height;
    void	*map_mask, *map_image;
    uint16	map_width, map_height;
    void	*fog_image;
    uint16      smooth_face;
} PixmapInfo;

extern PixmapInfo *pixmaps[MAXPIXMAPNUM];

/* Some global widgetws */
extern GtkWidget    *gtkwin_root,*drawingarea,*run_label,*fire_label;
extern GtkWidget    *gtkwin_info, *gtkwin_stats, *gtkwin_message;
extern GtkWidget    *gtkwin_look, *gtkwin_inv;
extern GtkWidget    *entrytext, *counttext;
extern GdkPixmap    *mapwindow,*dark;
extern GdkBitmap    *dark1, *dark2, *dark3;
extern GdkGC	    *mapgc;
extern GtkWidget    *ckentrytext, *ckeyentrytext, *cmodentrytext,*cnumentrytext, *cclist;
extern GtkTooltips  *tooltips;

/*
 *  This is similar obwin, but totally redone for client
 */
typedef struct {
  item *env;		  /* Environment shown in window */
  char title[MAX_BUF];  /* title of item list */
  char last_title[MAX_BUF];
  char last_weight[MAX_BUF];
  char last_maxweight[MAX_BUF];
  
  GtkWidget *label; /* e.g. "Inventory:", "keyring:" */
  GtkWidget *weightlabel;
  GtkWidget *maxweightlabel;

  uint8 show_icon:1;	  /* show status icons */
  uint8 show_weight:1;  /* show item's weight */
  
  uint32 weight_limit;   /* Weight limit for this list - used for title */
} itemlist;

extern itemlist inv_list, look_list;

typedef struct news_entry{
    char* title;
    char* content;
    struct news_entry* next;
} news_entry;

typedef struct media_state{
    GdkColor fore;
    GdkFont **style;
    int has_color:1;
    int flavor;
} media_state;
#endif

