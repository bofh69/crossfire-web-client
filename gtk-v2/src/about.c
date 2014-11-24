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

#include <gtk/gtk.h>

#include "main.h"

/**
 * Instantiates and displays the client's about box dialog.
 *
 * @param menuitem  The menu item that launches the about box
 * @param user_data
 */
void menu_about(GtkMenuItem *menuitem, gpointer user_data) {
    GtkWidget *about_window;
    about_window = GTK_WIDGET(gtk_builder_get_object(dialog_xml, "about_window"));
    gtk_dialog_run(GTK_DIALOG(about_window));
    gtk_widget_hide(about_window);
}
