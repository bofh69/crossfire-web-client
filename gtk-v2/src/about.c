/*
 * Crossfire -- cooperative multi-player graphical RPG and adventure game
 *
 * Copyright (c) 1999-2013 Mark Wedel and the Crossfire Development Team
 * Copyright (c) 1992 Frank Tore Johansen
 *
 * Crossfire is free software and comes with ABSOLUTELY NO WARRANTY. You are
 * welcome to redistribute it under certain conditions. For details, please
 * see COPYING and LICENSE.
 *
 * The authors can be reached via e-mail at <crossfire@metalforge.org>.
 */

/**
 * @file gtk-v2/src/about.c
 * Supports the client's about box dialog.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <ctype.h>
#include <glade/glade.h>
#include <gtk/gtk.h>

#include "client.h"

#include "main.h"
#include "image.h"
#include "gtk2proto.h"
#include "../../pixmaps/crossfiretitle.xpm"

static GtkWidget *about_window = NULL;

/**
 * Instantiates and displays the client's about box dialog.
 *
 * @param menuitem  The menu item that launches the about box
 * @param user_data
 */
void menu_about(GtkMenuItem *menuitem, gpointer user_data) {
    if (!about_window) {
        GtkImage *about_image;

        about_window = GTK_WIDGET(gtk_builder_get_object(dialog_xml, "about_window"));
        about_image = GTK_IMAGE(gtk_builder_get_object(dialog_xml, "about_image"));

        g_signal_connect((gpointer) about_window, "delete_event",
                         G_CALLBACK(gtk_widget_hide_on_delete), NULL);

        /* Load Crossfire image from inline XPM. */
        gtk_image_set_from_pixbuf(about_image, gdk_pixbuf_new_from_xpm_data(
                    (const char **)crossfiretitle_xpm));

        gtk_widget_show(about_window);
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
void on_about_close_clicked(GtkButton *button, gpointer user_data) {
    gtk_widget_hide(about_window);
}
