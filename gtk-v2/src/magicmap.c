char *rcsid_gtk2_magicmap_c =
    "$Id$";
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

    The author can be reached via e-mail to crossfire@metalforge.org
*/

/* This file is here to cover drawing the magic map.
 */
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>

#include "client.h"

#include "callbacks.h"
#include "interface.h"
#include "support.h"

#include "main.h"

/* in main.c - this is the drawing area for the magic map */
extern GtkWidget *magic_map, *map_notebook;
extern GdkColor root_color[NUM_COLORS];


static GdkGC *magic_map_gc=NULL;


/* This function draws the magic map - basically, it is just a simple encoding
 * of space X is color C.
 */
void draw_magic_map(void)
{
    int x=0, y=0;

    /* This can happen if a person selects the magic map pane before
     * actually getting any magic map data
     */
    if (!cpl.magicmap) return;

    /* Have to set this so that the gtk_widget_show below actually
     * creates teh widget.  also nice to switch to this page when
     * person actually casts magic map spell.
     */
    gtk_notebook_set_current_page(GTK_NOTEBOOK(map_notebook), MAGIC_MAP_PAGE);

    gtk_widget_show(magic_map);
 
    if (!magic_map_gc) magic_map_gc = gdk_gc_new (magic_map->window);

    gdk_gc_set_foreground (magic_map_gc, &root_color[0]);
    gdk_draw_rectangle (magic_map->window, magic_map_gc,	       
		       TRUE,
		       0,
		       0,
		       magic_map->allocation.width,
		       magic_map->allocation.height);
    cpl.mapxres = magic_map->allocation.width/cpl.mmapx;
    cpl.mapyres = magic_map->allocation.height/cpl.mmapy;

    if (cpl.mapxres < 1 || cpl.mapyres<1) {
	LOG(LOG_WARNING,"gtk::draw_magic_map","magic map resolution less than 1, map is %dx%d",
	      cpl.mmapx, cpl.mmapy);
	return;
    }

    /* In theory, cpl.mapxres and cpl.mapyres do not have to be the same.  However,
     * it probably makes sense to keep them the same value.
     * Need to take the smaller value.
     */
    if (cpl.mapxres>cpl.mapyres) cpl.mapxres=cpl.mapyres;
    else cpl.mapyres=cpl.mapxres;

    
    /* this is keeping the same unpacking scheme that the server uses
     * to pack it up.
     */
    for (y = 0; y < cpl.mmapy; y++) {
      for (x = 0; x < cpl.mmapx; x++) {
	uint8 val = cpl.magicmap[y*cpl.mmapx + x];

	gdk_gc_set_foreground (magic_map_gc, &root_color[val&FACE_COLOR_MASK]);

	gdk_draw_rectangle (magic_map->window, magic_map_gc,
			    TRUE,
			    cpl.mapxres*x,
			    cpl.mapyres*y,
			    cpl.mapxres,
			    cpl.mapyres);
      }
    }
}


/* Basically, this just flashes the player position on the magic map */
void magic_map_flash_pos(void)
{

    /* Don't want to keep doing this if the user switches back
     * to the map window.
     */
    if (gtk_notebook_get_current_page(GTK_NOTEBOOK(map_notebook))!=MAGIC_MAP_PAGE) {
	cpl.showmagic=0;
    }

    if (!cpl.showmagic) return;

    cpl.showmagic ^=2;
    if (cpl.showmagic & 2) {
	gdk_gc_set_foreground (magic_map_gc, &root_color[0]);
    } else {
	gdk_gc_set_foreground (magic_map_gc, &root_color[1]);
    }
    gdk_draw_rectangle (magic_map->window, magic_map_gc,
		      TRUE,
		      cpl.mapxres*cpl.pmapx,
		      cpl.mapyres*cpl.pmapy,
		      cpl.mapxres,
		      cpl.mapyres);
}


gboolean
on_drawingarea_magic_map_expose_event  (GtkWidget       *widget,
                                        GdkEventExpose  *event,
                                        gpointer         user_data)
{
    draw_magic_map();
    return FALSE;
}

