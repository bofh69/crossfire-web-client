char *rcsid_gtk2_info_c =
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

/* 
 * This covers drawing text to the info window.
 */
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>

#include "client.h"

#include "gtk2proto.h"
#include "interface.h"
#include "support.h"

#include "main.h"

GtkWidget *textview_info1, *textview_info2;
GtkTextBuffer *textbuf1, *textbuf2;
GtkTextMark *textmark1, *textmark2;

/* text_tag2 to represent it for for textbuffer 2 */
GtkTextTag	*text_tag1[NUM_COLORS], *text_tag2[NUM_COLORS];

void info_init(GtkWidget *window_root)
{
    int i;
    GtkTextIter end;
    extern  char *colorname[NUM_COLORS];

    textview_info1 = lookup_widget(window_root,"textview_info1");
    textview_info2 = lookup_widget(window_root,"textview_info2");
    textbuf1=gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview_info1));
    textbuf2=gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview_info2));

    gtk_text_buffer_get_end_iter(textbuf1, &end);
    textmark1 =  gtk_text_buffer_create_mark(textbuf1, NULL, &end, FALSE);

    gtk_text_buffer_get_end_iter(textbuf2, &end);
    textmark2 =  gtk_text_buffer_create_mark(textbuf2, NULL, &end, FALSE);

    for (i=0; i<NUM_COLORS; i++) {
	text_tag1[i] = gtk_text_buffer_create_tag(textbuf1, NULL, "foreground", colorname[i],NULL);
	text_tag2[i] = gtk_text_buffer_create_tag(textbuf2, NULL, "foreground", colorname[i],NULL);
    }
}

/* draw_info adds a line to the info window.
 * note that with the textbufs, it seems you need to manually set
 * it to the bottom of the screen - otherwise, the scrollbar just
 * stays at the top.  However, I could see this not being ideal
 * if you are trying to scroll back while new stuff comes in.
 */

void draw_info(const char *str, int color) {
    int ncolor = color;
    GtkTextIter end;
  
    if (ncolor==NDI_WHITE) {
	ncolor=NDI_BLACK;
    }


    if (color == NDI_BLACK) {
	gtk_text_buffer_get_end_iter(textbuf1, &end);
	gtk_text_buffer_insert(textbuf1, &end, str , strlen(str));
	gtk_text_buffer_insert(textbuf1, &end, "\n" , 1);
	gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(textview_info1), textmark1);
    } else {
	gtk_text_buffer_get_end_iter(textbuf1, &end);
	gtk_text_buffer_insert_with_tags(textbuf1, &end, str , strlen(str), text_tag1[ncolor], NULL);
	gtk_text_buffer_insert(textbuf1, &end, "\n" , 1);
	gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(textview_info1), textmark1);

	gtk_text_buffer_get_end_iter(textbuf2, &end);
	gtk_text_buffer_insert_with_tags(textbuf2, &end, str , strlen(str), text_tag2[ncolor], NULL);
	gtk_text_buffer_insert_at_cursor(textbuf2, "\n" , 1);
	gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(textview_info2), textmark2);
    }
}

void draw_color_info(int colr, const char *buf){
    if (use_config[CONFIG_COLORTXT]){
        draw_info(buf,colr);
    }
    else {
	/* White draws as black, but will also put it into the alternate text pane */
        draw_info("==========================================",NDI_WHITE);
        draw_info(buf,NDI_WHITE);
        draw_info("==========================================",NDI_WHITE);
    }
}


/* Clears all the message.  Not sure why someone would use it, 
 * but it is called from the common area, so might as well
 * support it.
 */
void menu_clear() {
    gtk_text_buffer_set_text(textbuf1, "", 0);
    gtk_text_buffer_set_text(textbuf2, "", 0);
}

/* All the following are 'dummy' functions.  Basically, there are callbacks
 * to these from the common area, but they are not implemented in gtk,
 * either because it makes no sense (set_scroll for example), or because
 * it may not be technically possible to do so if we limit ourselves
 * to proper GTK2 code (Eg, don't mess with the internals of X or
 * platform specific issues)
 */
void set_scroll(char *s)
{
}

void set_autorepeat(char *s)
{
}

/* FIXME: should be better than hardcoded value.  This is used by the common
 * help system to determine when to wrap.  Should be able to get width of
 * window, and divide by character width - however, still not perfect if 
 * we are using a variable width font.
 */
int get_info_width()
{
    return 40;
}

