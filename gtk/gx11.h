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
#include "item.h"


extern int map_size, image_size, map_image_size, map_image_half_size;
extern uint8 map_did_scroll;
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
} PixmapInfo;

extern PixmapInfo *pixmaps[MAXPIXMAPNUM];

/* Some global widgetws */
extern GtkWidget    *gtkwin_root,*drawingarea,*run_label,*fire_label;
extern GtkWidget    *gtkwin_info, *gtkwin_stats, *gtkwin_message;
extern GtkWidget    *gtkwin_look, *gtkwin_inv, *gtkwin_config;
extern GtkWidget    *entrytext, *counttext;
extern GdkPixmap    *mapwindow,*dark;
extern GdkBitmap    *dark1, *dark2, *dark3;
extern GdkGC	    *mapgc;
extern GtkWidget    *ckentrytext, *ckeyentrytext, *cmodentrytext,*cnumentrytext, *cclist;
extern GtkTooltips  *tooltips;

#define TYPE_LISTS 9
/*
 *  This is similar obwin, but totally redone for client
 */
typedef struct {
  item *env;		  /* Environment shown in window */
  char title[MAX_BUF];  /* title of item list */
  char old_title[MAX_BUF];  /* previos title (avoid redrawns) */
  
  Window win;		  /* for X-windows */
  GtkWidget *label;
  GtkWidget *weightlabel;
  GtkWidget *maxweightlabel;

  /*  gint invrows;
  gint appliedrows;
  gint unappliedrows;
  gint unpaidrows;
  gint cursedrows;
  gint magicalrows;
  gint nonmagicalrows;
  gint lockedrows;*/

  float pos[TYPE_LISTS];

  GtkWidget *gtk_list[TYPE_LISTS];
  GtkWidget *gtk_lists[TYPE_LISTS];

  GC gc_text;
  GC gc_icon;
  GC gc_status;
  
  uint8 multi_list:1;     /* view is multi type */
  uint8 show_icon:1;	  /* show status icons */
  uint8 show_weight:1;  /* show item's weight */
  
  char format_nw[20];	  /* sprintf-format for text (name and weight) */
  char format_nwl[20];    /* sprintf-format for text (name, weight, limit) */
  char format_n[20];	  /* sprintf-format for text (only name) */
  sint16 text_len;	  /* How wide the text-field is */
  
  sint16 width;	  /* How wide the window is in pixels */
  sint16 height;	  /* How height the window is in pixels */
   
  sint16 item_pos;	  /* The sequence number of the first drawn item */
  sint16 item_used;	  /* How many items actually drawn. (0 - size) */
  
  sint16 size;	  /* How many items there is room to display */
  sint16 *faces;	  /* [size] */
  sint8 *icon1;	  /* status icon : locked */
  sint8 *icon2;	  /* status icon : applied / unpaid */
  sint8 *icon3;	  /* status icon : magic */
  sint8 *icon4;	  /* status icon : damned / cursed */
  char **names;	  /* [size][NAME_LEN] */
  
  /* The scrollbar */
  sint16 bar_length; 	  /* the length of scrollbar in pixels */
  sint16 bar_size;	  /* the current size of scrollbar in pixels */
  sint16 bar_pos;	  /* the starting position of scrollbar in pixels */
  uint32 weight_limit;   /* Weight limit for this list - used for title */
  sint16 row_height, column_width;  /* height/width of pixmaps space as last drawn */
} itemlist;

extern itemlist inv_list, look_list;

/* This was just a test I put in to try different redraw methods */
#define ALTERNATE_MAP_REDRAW	0


#endif

