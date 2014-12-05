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
 * @file gtk-v2/src/menubar.c
 * Sets up menu connections and implements core menu items in the top menubar.
 *
 * Quick notes on the menubar:
 * 1) Using the stock Quit menu item for some reason causes it to take several
 *    seconds of 100% cpu utilization to show the menu.  So I don't use the
 *    stock item.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>

#include "client.h"

#ifdef WIN32
# include <windows.h>
# include "script.h"
#endif

#include "p_cmd.h"
#include "main.h"
#include "image.h"
#include "gtk2proto.h"

/**
 * Client | Disconnect
 * Triggers the client to disconnect from the server.
 *
 * @param menuitem
 * @param user_data
 */
static void on_disconnect_activate(GtkMenuItem *menuitem, gpointer user_data) {
    close_server_connection();

    if (csocket_fd) {
        gdk_input_remove(csocket_fd);
        csocket_fd=0;
        gtk_main_quit();
    }
}

/**
 * File | Quit
 * Shuts down the client application.
 *
 * @param menuitem
 * @param user_data
 */
static void menu_quit_program(GtkMenuItem *menuitem, gpointer user_data) {
#ifdef WIN32
    script_killall();
#endif

    LOG(LOG_INFO,"gtk-v2::client_exit","Exiting with return value 0.");
    exit(0);
}

/**
 * File | Quit Character
 * Causes the client to ask the server to delete the current character.
 *
 * @param menuitem
 * @param user_data
 */
static void menu_quit_character(GtkMenuItem *menuitem, gpointer user_data) {
    extended_command("quit");
}

/**
 * Display client about dialog.
 */
static void menu_about(GtkMenuItem *menuitem, gpointer user_data) {
    GtkWidget *about_window;
    about_window = GTK_WIDGET(gtk_builder_get_object(dialog_xml, "about_window"));
    gtk_dialog_run(GTK_DIALOG(about_window));
    gtk_widget_hide(about_window);
}

/**
 * Initialize menu bar items and connect their signals to their handlers.
 */
void init_menu_items() {
    GtkWidget *widget;

    widget = GTK_WIDGET(gtk_builder_get_object(window_xml, "quit_character"));
    g_signal_connect ((gpointer) widget, "activate",
                      G_CALLBACK (menu_quit_character), NULL);

    widget = GTK_WIDGET(gtk_builder_get_object(window_xml, "quit"));
    g_signal_connect ((gpointer) widget, "activate",
                      G_CALLBACK (menu_quit_program), NULL);

    widget = GTK_WIDGET(gtk_builder_get_object(window_xml, "configure"));
    g_signal_connect ((gpointer) widget, "activate",
                      G_CALLBACK (on_configure_activate), NULL);

    widget = GTK_WIDGET(gtk_builder_get_object(window_xml, "disconnect"));
    g_signal_connect ((gpointer) widget, "activate",
                      G_CALLBACK (on_disconnect_activate), NULL);

    widget = GTK_WIDGET(gtk_builder_get_object(window_xml, "keybindings"));
    g_signal_connect ((gpointer) widget, "activate",
                      G_CALLBACK (on_keybindings_activate), NULL);

    widget = GTK_WIDGET(gtk_builder_get_object(window_xml, "msgctrl"));
    g_signal_connect ((gpointer) widget, "activate",
                      G_CALLBACK (on_msgctrl_activate), NULL);

    widget = GTK_WIDGET(gtk_builder_get_object(window_xml, "save_window_position"));
    g_signal_connect ((gpointer) widget, "activate",
                      G_CALLBACK (on_save_window_position_activate), NULL);

    widget = GTK_WIDGET(gtk_builder_get_object(window_xml, "spells"));
    g_signal_connect ((gpointer) widget, "activate",
                      G_CALLBACK (on_spells_activate), NULL);

    widget = GTK_WIDGET(gtk_builder_get_object(window_xml, "skills"));
    g_signal_connect ((gpointer) widget, "activate",
                      G_CALLBACK (on_skills_activate), NULL);

    widget = GTK_WIDGET(gtk_builder_get_object(window_xml, "do_not_pickup"));
    g_signal_connect ((gpointer) widget, "activate",
                      G_CALLBACK (on_menu_dont_pickup_activate), NULL);

    widget = GTK_WIDGET(gtk_builder_get_object(window_xml, "stop_before_pickup"));
    g_signal_connect ((gpointer) widget, "activate",
                      G_CALLBACK (on_menu_stop_before_pickup_activate), NULL);

    widget = GTK_WIDGET(gtk_builder_get_object(window_xml, "body_armor"));
    g_signal_connect ((gpointer) widget, "activate",
                      G_CALLBACK (on_menu_body_armor_activate), NULL);

    widget = GTK_WIDGET(gtk_builder_get_object(window_xml, "boots"));
    g_signal_connect ((gpointer) widget, "activate",
                      G_CALLBACK (on_menu_boots_activate), NULL);

    widget = GTK_WIDGET(gtk_builder_get_object(window_xml, "cloaks"));
    g_signal_connect ((gpointer) widget, "activate",
                      G_CALLBACK (on_menu_cloaks_activate), NULL);

    widget = GTK_WIDGET(gtk_builder_get_object(window_xml, "gloves"));
    g_signal_connect ((gpointer) widget, "activate",
                      G_CALLBACK (on_menu_gloves_activate), NULL);

    widget = GTK_WIDGET(gtk_builder_get_object(window_xml, "helmets"));
    g_signal_connect ((gpointer) widget, "activate",
                      G_CALLBACK (on_menu_helmets_activate), NULL);

    widget = GTK_WIDGET(gtk_builder_get_object(window_xml, "shields"));
    g_signal_connect ((gpointer) widget, "activate",
                      G_CALLBACK (on_menu_shields_activate), NULL);

    widget = GTK_WIDGET(gtk_builder_get_object(window_xml, "skillscrolls"));
    g_signal_connect ((gpointer) widget, "activate",
                      G_CALLBACK (on_menu_skillscrolls_activate), NULL);

    widget = GTK_WIDGET(gtk_builder_get_object(window_xml, "normal_book_scrolls"));
    g_signal_connect ((gpointer) widget, "activate",
                      G_CALLBACK (on_menu_normal_book_scrolls_activate), NULL);

    widget = GTK_WIDGET(gtk_builder_get_object(window_xml, "spellbooks"));
    g_signal_connect ((gpointer) widget, "activate",
                      G_CALLBACK (on_menu_spellbooks_activate), NULL);

    widget = GTK_WIDGET(gtk_builder_get_object(window_xml, "drinks"));
    g_signal_connect ((gpointer) widget, "activate",
                      G_CALLBACK (on_menu_drinks_activate), NULL);

    widget = GTK_WIDGET(gtk_builder_get_object(window_xml, "food"));
    g_signal_connect ((gpointer) widget, "activate",
                      G_CALLBACK (on_menu_food_activate), NULL);

    widget = GTK_WIDGET(gtk_builder_get_object(window_xml, "flesh"));
    g_signal_connect ((gpointer) widget, "activate",
                      G_CALLBACK (on_menu_flesh_activate), NULL);

    widget = GTK_WIDGET(gtk_builder_get_object(window_xml, "keys"));
    g_signal_connect ((gpointer) widget, "activate",
                      G_CALLBACK (on_menu_keys_activate), NULL);

    widget = GTK_WIDGET(gtk_builder_get_object(window_xml, "magical_items"));
    g_signal_connect ((gpointer) widget, "activate",
                      G_CALLBACK (on_menu_magical_items_activate), NULL);

    widget = GTK_WIDGET(gtk_builder_get_object(window_xml, "potions"));
    g_signal_connect ((gpointer) widget, "activate",
                      G_CALLBACK (on_menu_potions_activate), NULL);

    widget = GTK_WIDGET(gtk_builder_get_object(window_xml, "valuables"));
    g_signal_connect ((gpointer) widget, "activate",
                      G_CALLBACK (on_menu_valuables_activate), NULL);

    widget = GTK_WIDGET(gtk_builder_get_object(window_xml, "wands_rods_horns"));
    g_signal_connect ((gpointer) widget, "activate",
                      G_CALLBACK (on_menu_wands_rods_horns_activate), NULL);

    widget = GTK_WIDGET(gtk_builder_get_object(window_xml, "jewels"));
    g_signal_connect ((gpointer) widget, "activate",
                      G_CALLBACK (on_menu_jewels_activate), NULL);

    widget = GTK_WIDGET(gtk_builder_get_object(window_xml, "containers"));
    g_signal_connect ((gpointer) widget, "activate",
                      G_CALLBACK (on_menu_containers_activate), NULL);

    widget = GTK_WIDGET(gtk_builder_get_object(window_xml, "all_weapons"));
    g_signal_connect ((gpointer) widget, "activate",
                      G_CALLBACK (on_menu_all_weapons_activate), NULL);

    widget = GTK_WIDGET(gtk_builder_get_object(window_xml, "missile_weapons"));
    g_signal_connect ((gpointer) widget, "activate",
                      G_CALLBACK (on_menu_missile_weapons_activate), NULL);

    widget = GTK_WIDGET(gtk_builder_get_object(window_xml, "bows"));
    g_signal_connect ((gpointer) widget, "activate",
                      G_CALLBACK (on_menu_bows_activate), NULL);

    widget = GTK_WIDGET(gtk_builder_get_object(window_xml, "arrows"));
    g_signal_connect ((gpointer) widget, "activate",
                      G_CALLBACK (on_menu_arrows_activate), NULL);

    widget = GTK_WIDGET(gtk_builder_get_object(window_xml, "ratio_pickup_off"));
    g_signal_connect ((gpointer) widget, "activate",
                      G_CALLBACK (on_menu_ratio_pickup_off_activate), NULL);

    widget = GTK_WIDGET(gtk_builder_get_object(window_xml, "ratio_5"));
    g_signal_connect ((gpointer) widget, "activate",
                      G_CALLBACK (on_menu_ratio_5_activate), NULL);

    widget = GTK_WIDGET(gtk_builder_get_object(window_xml, "ratio_10"));
    g_signal_connect ((gpointer) widget, "activate",
                      G_CALLBACK (on_menu_ratio_10_activate), NULL);

    widget = GTK_WIDGET(gtk_builder_get_object(window_xml, "ratio_15"));
    g_signal_connect ((gpointer) widget, "activate",
                      G_CALLBACK (on_menu_ratio_15_activate), NULL);

    widget = GTK_WIDGET(gtk_builder_get_object(window_xml, "ratio_20"));
    g_signal_connect ((gpointer) widget, "activate",
                      G_CALLBACK (on_menu_ratio_20_activate), NULL);

    widget = GTK_WIDGET(gtk_builder_get_object(window_xml, "ratio_25"));
    g_signal_connect ((gpointer) widget, "activate",
                      G_CALLBACK (on_menu_ratio_25_activate), NULL);

    widget = GTK_WIDGET(gtk_builder_get_object(window_xml, "ratio_30"));
    g_signal_connect ((gpointer) widget, "activate",
                      G_CALLBACK (on_menu_ratio_35_activate), NULL);

    widget = GTK_WIDGET(gtk_builder_get_object(window_xml, "ratio_35"));
    g_signal_connect ((gpointer) widget, "activate",
                      G_CALLBACK (on_menu_ratio_35_activate), NULL);

    widget = GTK_WIDGET(gtk_builder_get_object(window_xml, "ratio_40"));
    g_signal_connect ((gpointer) widget, "activate",
                      G_CALLBACK (on_menu_ratio_40_activate), NULL);

    widget = GTK_WIDGET(gtk_builder_get_object(window_xml, "ratio_45"));
    g_signal_connect ((gpointer) widget, "activate",
                      G_CALLBACK (on_menu_ratio_45_activate), NULL);

    widget = GTK_WIDGET(gtk_builder_get_object(window_xml, "ratio_50"));
    g_signal_connect ((gpointer) widget, "activate",
                      G_CALLBACK (on_menu_ratio_50_activate), NULL);

    widget = GTK_WIDGET(gtk_builder_get_object(window_xml, "not_cursed"));
    g_signal_connect ((gpointer) widget, "activate",
                      G_CALLBACK (on_menu_not_cursed_activate), NULL);

    widget = GTK_WIDGET(gtk_builder_get_object(window_xml, "about"));
    g_signal_connect ((gpointer) widget, "activate",
                      G_CALLBACK (menu_about), NULL);
}
