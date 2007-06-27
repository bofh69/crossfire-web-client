char *rcsid_gtk2_about_c =
    "$Id$";
/*
    Crossfire client, a client program for the crossfire program.

    Copyright (C) 2006 Mark Wedel & Crossfire Development Team

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


/* This file is here to cover configuration issues.
 */
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>
#include <ctype.h>

#include "client.h"

#include "interface.h"
#include "support.h"

#include "main.h"
#include "image.h"
#include "gtk2proto.h"
#include "about.h"
#include "../../pixmaps/crossfiretitle.xpm"


static GtkWidget   *about_window=NULL;

void
menu_about                       (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    if (!about_window) {
	GtkWidget   *textview;
	GtkTextBuffer	*textbuf;
	GtkTextIter end;
	GtkWidget *hbox;
	GtkWidget *aboutgtkpixmap;
	GdkPixmap *aboutgdkpixmap;
	GdkBitmap *aboutgdkmask;


	about_window=create_about_window();

	textview = lookup_widget(about_window, "about_textview");
	textbuf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));

        gtk_text_buffer_get_end_iter(textbuf, &end);
	gtk_text_buffer_insert(textbuf, &end, VERSION_INFO, strlen(VERSION_INFO));
	gtk_text_buffer_insert(textbuf, &end, "\n", 1);
	gtk_text_buffer_insert(textbuf, &end, text, strlen(text));

	/* The window must be realized before we can create the pixmap below */
	gtk_widget_show(about_window);

	aboutgdkpixmap = gdk_pixmap_create_from_xpm_d(about_window->window,
                                                  &aboutgdkmask,
                                                  NULL,
                                                  (gchar **)crossfiretitle);
	aboutgtkpixmap= gtk_image_new_from_pixmap (aboutgdkpixmap, aboutgdkmask);

	/* Use of hbox is a bit of a hack - isn't any easy way to add
	 * this image as the first entry of the box once other fields have been
	 * filled in.  So instead, we create a hbox in that first entry just
	 * to hold this image.
	 */
	hbox = lookup_widget(about_window, "about_hbox_image");
	gtk_box_pack_start (GTK_BOX (hbox),aboutgtkpixmap, TRUE, TRUE, 0);

	gtk_widget_show(aboutgtkpixmap);

    } else {
	gtk_widget_show(about_window);
    }

}


void
on_about_close_clicked                 (GtkButton       *button,
                                        gpointer         user_data)
{
    gtk_widget_hide(about_window);
}
