/*
 * Crossfire -- cooperative multi-player graphical RPG and adventure game
 *
 * Copyright (c) 1999-2013 Mark Wedel and the Crossfire Development Team
 * Copyright (c) 1992 Frank Tore Johansen
 *
 * Crossfire is free software and comes with ABSOLUTELY NO WARRANTY. You are
 * welcome to redistribute it under certain conditions. For details, see the
 * 'LICENSE' and 'COPYING' files.
 *
 * The authors can be reached via e-mail to crossfire-devel@real-time.com
 */

/**
 * @file gtk-v2/src/about.c
 * Supports the client's about box dialog.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>
#include <glade/glade.h>
#include <ctype.h>

#include "client.h"

#include "main.h"
#include "image.h"
#include "gtk2proto.h"
#include "about.h"
#include "../../pixmaps/crossfiretitle.xpm"

static GtkWidget   *about_window=NULL;

/**
 * Instantiates and displays the client's about box dialog.
 *
 * @param menuitem  The menu item that launches the about box
 * @param user_data
 */
void
menu_about                             (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    GladeXML *xml_tree;
    GtkWidget *widget;

    if (!about_window) {
        GtkWidget   *textview;
        GtkTextBuffer   *textbuf;
        GtkTextIter end;
        GtkWidget *hbox;
        GtkWidget *aboutgtkpixmap;
        GdkPixmap *aboutgdkpixmap;
        GdkBitmap *aboutgdkmask;

        about_window = glade_xml_get_widget(dialog_xml, "about_window");
        xml_tree = glade_get_widget_tree(GTK_WIDGET(about_window));

        textview = glade_xml_get_widget(xml_tree, "about_textview");

        g_signal_connect((gpointer) about_window, "delete_event",
                         G_CALLBACK(gtk_widget_hide_on_delete), NULL);

        widget = glade_xml_get_widget(xml_tree, "about_close");
        g_signal_connect((gpointer) widget, "clicked",
                         G_CALLBACK(on_about_close_clicked), NULL);

        textbuf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));

        gtk_text_buffer_get_end_iter(textbuf, &end);
        gtk_text_buffer_insert(textbuf, &end, VERSION_INFO,
                               strlen(VERSION_INFO));
        gtk_text_buffer_insert(textbuf, &end, "\n", 1);
        gtk_text_buffer_insert(textbuf, &end, text, strlen(text));

        /* The window must be realized before we can create the pixmap below */
        gtk_widget_show(about_window);

        aboutgdkpixmap = gdk_pixmap_create_from_xpm_d(about_window->window,
                         &aboutgdkmask,
                         NULL,
                         (gchar **)crossfiretitle_xpm);

        aboutgtkpixmap= gtk_image_new_from_pixmap (aboutgdkpixmap,
                        aboutgdkmask);
        /*
         * Use of hbox is a bit of a hack - isn't any easy way to add this
         * image as the first entry of the box once other fields have been
         * filled in.  So instead, we create a hbox in that first entry just to
         * hold this image.
         */
        hbox = glade_xml_get_widget(xml_tree, "about_hbox_image");

        gtk_box_pack_start (GTK_BOX (hbox),aboutgtkpixmap, TRUE, TRUE, 0);

        gtk_widget_show(aboutgtkpixmap);

    } else {
        gtk_widget_show(about_window);
    }

}

/**
 * Closes and hides the client's about box dialog.
 *
 * @param button    The about dialog's close button.
 * @param user_data
 */
void
on_about_close_clicked                 (GtkButton       *button,
                                        gpointer         user_data)
{
    gtk_widget_hide(about_window);
}
