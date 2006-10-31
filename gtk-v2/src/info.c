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

#include "image.h"
#include "gtk2proto.h"
#include "interface.h"
#include "support.h"

#include "main.h"

struct Info_Pane 
{
    GtkWidget	    *textview;
    GtkWidget	    *scrolled_window;
    GtkTextBuffer   *textbuffer;
    GtkTextMark	    *textmark;
    GtkAdjustment   *adjustment;
    GtkTextTag	    *text_tags[NUM_COLORS];
} info_pane[2];


static void message_callback(int flag, int type, int subtype, char *message);

/* Index into the different fonts.  font_families holds the names to
 * use for the font with pango - pango seems to take a comma seperated
 * listed.  What really should be done is let the use choose the fonts
 * to use, since most systems do not have most of the font families
 * listed (taken from gtk1 client)
 */
#define FONT_NORMAL	0
#define FONT_ARCANE	1
#define	FONT_STRANGE	2
#define	FONT_FIXED	3
#define FONT_HAND	4
#define NUM_FONTS	5

/* A fair number of these fonts can be found at:
 * http://www.dafont.com
 * This is in no way an endorsement of that site - just a place to go
 * to find them.
 * Fonts I found there:  blackforest, annstone (note, it doesn't have numbers!),
 *  dobkin
 * For a reason I'm not sure of, it doesn't seem to use the other fonts - I notice
 * with xfontsel, the options for weight and size are not enabled, so may be related
 * to that.
 * MSW 2006-09-17
 */
static char *font_families[NUM_FONTS] = {
    "arial,bookman,agate", 
    "cuneifontlight,linotext,blackforest,becker,arnoldboecklin,caligula,helvetica",
    "annstone,shalomstick",
    /* fixed doesn't scale, so put it at the end */
    "courier,andale mono,urw bookman l,fixed",
    "dobkin,coronetscript,muriel,genoa,parkavenue,rechtmanscript,luxi serif"
};

extern  char *colorname[NUM_COLORS];

void info_init(GtkWidget *window_root)
{
    int i;
    GtkTextIter end;

    info_pane[0].textview = lookup_widget(window_root,"textview_info1");
    info_pane[1].textview = lookup_widget(window_root,"textview_info2");
    info_pane[0].scrolled_window = lookup_widget(window_root, "scrolledwindow_textview1");
    info_pane[1].scrolled_window = lookup_widget(window_root, "scrolledwindow_textview2");
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(info_pane[0].textview), GTK_WRAP_WORD);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(info_pane[1].textview), GTK_WRAP_WORD);
    info_pane[0].textbuffer=gtk_text_view_get_buffer(GTK_TEXT_VIEW(info_pane[0].textview));
    info_pane[1].textbuffer=gtk_text_view_get_buffer(GTK_TEXT_VIEW(info_pane[1].textview));
    info_pane[0].adjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(info_pane[0].scrolled_window));
    info_pane[1].adjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(info_pane[1].scrolled_window));

    gtk_text_buffer_get_end_iter(info_pane[0].textbuffer, &end);
    info_pane[0].textmark =  gtk_text_buffer_create_mark(info_pane[0].textbuffer, NULL, &end, FALSE);

    gtk_text_buffer_get_end_iter(info_pane[1].textbuffer, &end);
    info_pane[1].textmark =  gtk_text_buffer_create_mark(info_pane[1].textbuffer, NULL, &end, FALSE);

    for (i=0; i<NUM_COLORS; i++) {
	info_pane[0].text_tags[i] = gtk_text_buffer_create_tag(info_pane[0].textbuffer, NULL, "foreground", colorname[i],NULL);
	info_pane[1].text_tags[i] = gtk_text_buffer_create_tag(info_pane[1].textbuffer, NULL, "foreground", colorname[i],NULL);
    }

    /* Register callbacks for all message types */
    for (i=0; i<MSG_TYPE_LAST; i++)
	setTextManager(i,message_callback);
}


static void add_to_textbuf(int pane, char *message, int weight, int style, int font, char *color, int underline)
{
    GtkTextIter end;
    GdkRectangle rect;
    int scroll_to_end=0;
    gdouble scale=1.0;
    GtkTextTag	    *tag;
    char    tagname[MAX_BUF];

    gtk_text_view_get_visible_rect(GTK_TEXT_VIEW(info_pane[pane].textview), &rect);
    if ((info_pane[pane].adjustment->value + rect.height) >= info_pane[pane].adjustment->upper ) scroll_to_end=1;

    /* A bit of a hack, but relative to the proportional fonts, the fixed font is
     * bigger, and thus needs to be scaled down some for things to look
     * right
     */
    if (font == FONT_FIXED)  scale=0.9;

    /* In order not to create thousands of tags (memory leak) or have to do logic to
     * periodically clean up old text tags, give each text tag a name that describes
     * all the attributes.  If that tag exists, re-use it.  If it doesn't, create
     * a new tag of that name.
     */
    sprintf(tagname, "weight%d-style%d-family%s-foreground%s-scale%f-underline%d",
	    weight, style, font_families[font], color, scale, underline);

    if ((tag=gtk_text_tag_table_lookup(gtk_text_buffer_get_tag_table(info_pane[pane].textbuffer),tagname))==NULL) {
	tag = gtk_text_buffer_create_tag(info_pane[pane].textbuffer, tagname,
		"weight", weight,
		"style", style,
		"family", font_families[font],
		"foreground", color,
		"scale", scale,
		"underline", underline,
		 NULL);
    }

    gtk_text_buffer_get_end_iter(info_pane[pane].textbuffer, &end);

    gtk_text_buffer_insert_with_tags(info_pane[pane].textbuffer, &end, message , strlen(message),
	    tag, NULL);

    if (scroll_to_end)
	gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(info_pane[pane].textview), info_pane[pane].textmark);

}

/* Unlike the gtk client, we don't do anything tricky like popups
 * with different message types.
 * However, we will choose different fonts, etc, based on this information -
 * for this reason, we just use one callback, and change those minor
 * things based on the callback.
 * We also need to parse the data.
 */
static void message_callback(int orig_color, int type, int subtype, char *message) {
    char *marker, *current, *original;
    int	weight=PANGO_WEIGHT_NORMAL;
    int style=PANGO_STYLE_NORMAL;
    int	font=FONT_NORMAL, underline=PANGO_UNDERLINE_NONE;
    char *color=colorname[orig_color];

    current = strdup(message);
    original = current;		/* Just so we know what to free */

    while ((marker = strchr(current,'['))!= NULL) {
	*marker = 0;
	add_to_textbuf(0,current, weight, style, font, color, underline);
	current=marker+1;
	if ((marker = strchr(current,']')) == NULL) {
	    free(original);
	    return;
	}
	*marker = 0;
	if (!strcmp(current,"b"))	weight = PANGO_WEIGHT_BOLD;
	else if (!strcmp(current,"/b"))	weight = PANGO_WEIGHT_NORMAL;
	else if (!strcmp(current,"i"))	style = PANGO_STYLE_ITALIC;
	else if (!strcmp(current,"/i"))	style = PANGO_STYLE_NORMAL;
	else if (!strcmp(current,"ul"))		underline=PANGO_UNDERLINE_SINGLE;
	else if (!strcmp(current,"/ul"))	underline=PANGO_UNDERLINE_NONE;
	else if (!strcmp(current,"fixed"))	font = FONT_FIXED;
	else if (!strcmp(current,"arcane"))	font = FONT_ARCANE;
	else if (!strcmp(current,"hand"))	font = FONT_HAND;
	else if (!strcmp(current,"strange"))	font = FONT_STRANGE;
	else if (!strcmp(current,"print"))	font = FONT_NORMAL;
	else if (!strcmp(current,"/color"))	color = colorname[orig_color];
	else if (!strncmp(current,"color=",6))	color = current + 6;
	else LOG(LOG_INFO, "info.c::message_callback", "unidentified message: %s\n", current);
	current = marker+1;

    }
    add_to_textbuf(0,current, weight, style, font, color, underline);
    add_to_textbuf(0, "\n", weight, style, font, color, underline);
    free(original);
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
    GdkRectangle rect;
    int scroll_to_end=0;
  
    if (ncolor==NDI_WHITE) {
	ncolor=NDI_BLACK;
    }

    /* This seems more complicated than it should be, but we need to see if
     * the window is scrolled at the end.  If it is, we want to keep scrolling
     * it down with new info.  If not, we don't want to change position - otherwise,
     * it makes it very difficult to look back at the old info (like old messages
     * missed during combat, looking at the shop listing while people are chatting,
     * etc)
     * We need to find out the position before putting in new text -
     * otherwise, that operation will mess up our position, and
     * not give us right info.
     */
    gtk_text_view_get_visible_rect(GTK_TEXT_VIEW(info_pane[0].textview), &rect);
    if ((info_pane[0].adjustment->value + rect.height) >= info_pane[0].adjustment->upper ) scroll_to_end=1;

    if (color == NDI_BLACK) {
	gtk_text_buffer_get_end_iter(info_pane[0].textbuffer, &end);
	gtk_text_buffer_insert(info_pane[0].textbuffer, &end, str , strlen(str));
	gtk_text_buffer_insert(info_pane[0].textbuffer, &end, "\n" , 1);

	if (scroll_to_end)
	    gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(info_pane[0].textview), info_pane[0].textmark);
    } else {
	gtk_text_buffer_get_end_iter(info_pane[0].textbuffer, &end);
	gtk_text_buffer_insert_with_tags(info_pane[0].textbuffer, &end, str , strlen(str), info_pane[0].text_tags[ncolor], NULL);
	gtk_text_buffer_insert(info_pane[0].textbuffer, &end, "\n" , 1);

	if (scroll_to_end)
	    gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(info_pane[0].textview), info_pane[0].textmark);

	gtk_text_view_get_visible_rect(GTK_TEXT_VIEW(info_pane[1].textview), &rect);
	if ((info_pane[1].adjustment->value + rect.height) >= info_pane[1].adjustment->upper ) scroll_to_end=1;
	else scroll_to_end=0;

	gtk_text_buffer_get_end_iter(info_pane[1].textbuffer, &end);
	gtk_text_buffer_insert_with_tags(info_pane[1].textbuffer, &end, str , strlen(str), info_pane[1].text_tags[ncolor], NULL);
	gtk_text_buffer_insert(info_pane[1].textbuffer, &end, "\n" , 1);

	if (scroll_to_end)
	    gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(info_pane[1].textview), info_pane[1].textmark);
    }
}

void draw_color_info(int colr, const char *buf){
        draw_info(buf,colr);
}


/* Clears all the message.  Not sure why someone would use it, 
 * but it is called from the common area, so might as well
 * support it.
 */
void menu_clear() {
    gtk_text_buffer_set_text(info_pane[0].textbuffer, "", 0);
    gtk_text_buffer_set_text(info_pane[1].textbuffer, "", 0);
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

