char *rcsid_gtk2_menubar_c =
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

/* This file is here to cover the core selections from the top
 * menubar.
 */
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifdef WIN32
#include <windows.h>
#endif
#include <gtk/gtk.h>
#include <glade/glade.h>

#include "client.h"

#include "p_cmd.h"
#include "main.h"
#include "image.h"
#include "gtk2proto.h"

/* Few quick notes on the menubar:
 * 1) Using the stock Quit menu item for some reason causes it to
 *    take several seconds of 100% cpu utilization to show the menu.
 *    So I don't use the stock item.
 */

void
on_disconnect_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    extern gint csocket_fd;

#ifdef WIN32
    closesocket(csocket.fd);
#else
    close(csocket.fd);
#endif
    csocket.fd = -1;
    if (csocket_fd) {
        gdk_input_remove(csocket_fd);
        csocket_fd=0;
        gtk_main_quit();
    }
}



void
menu_quit_program                       (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
#ifdef WIN32
    script_killall();
#endif

    LOG(LOG_INFO,"gtk::client_exit","Exiting with return value 0.");
    exit(0);

}

void
menu_quit_character                       (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    extended_command("quit");

}

/* This function enables/disables some of the menubar options.  Some do
 * not make sense if not connected to the server, so should be
 * disabled until connected.
 * enable is a true/false value. If true, enable the items, if false,
 * disable them.
 */

void enable_menu_items(int enable)
{
    GladeXML *xml_tree;
    GtkWidget *widget;

    xml_tree = glade_get_widget_tree(GTK_WIDGET(window_root));

    widget = glade_xml_get_widget(xml_tree, "quit_character1");
    g_signal_connect ((gpointer) widget, "activate",
        G_CALLBACK (menu_quit_character), NULL);

    widget = glade_xml_get_widget(xml_tree, "quit2");
    g_signal_connect ((gpointer) widget, "activate",
        G_CALLBACK (menu_quit_program), NULL);

    widget = glade_xml_get_widget(xml_tree, "configure1");
    g_signal_connect ((gpointer) widget, "activate",
        G_CALLBACK (on_configure_activate), NULL);

    widget = glade_xml_get_widget(xml_tree, "disconnect");
    g_signal_connect ((gpointer) widget, "activate",
        G_CALLBACK (on_disconnect_activate), NULL);

    widget = glade_xml_get_widget(xml_tree, "keybindings");
    g_signal_connect ((gpointer) widget, "activate",
        G_CALLBACK (on_keybindings_activate), NULL);

    widget = glade_xml_get_widget(xml_tree, "save_window_position");
    g_signal_connect ((gpointer) widget, "activate",
        G_CALLBACK (on_save_window_position_activate), NULL);

    widget = glade_xml_get_widget(xml_tree, "spells");
    g_signal_connect ((gpointer) widget, "activate",
        G_CALLBACK (on_spells_activate), NULL);

    widget = glade_xml_get_widget(xml_tree, "do_not_pickup");
    g_signal_connect ((gpointer) widget, "activate",
        G_CALLBACK (on_menu_dont_pickup_activate), NULL);

    widget = glade_xml_get_widget(xml_tree, "stop_before_pickup1");
    g_signal_connect ((gpointer) widget, "activate",
        G_CALLBACK (on_menu_stop_before_pickup_activate), NULL);

    widget = glade_xml_get_widget(xml_tree, "body_armor1");
    g_signal_connect ((gpointer) widget, "activate",
        G_CALLBACK (on_menu_body_armor_activate), NULL);

    widget = glade_xml_get_widget(xml_tree, "boots1");
    g_signal_connect ((gpointer) widget, "activate",
        G_CALLBACK (on_menu_boots_activate), NULL);

    widget = glade_xml_get_widget(xml_tree, "cloaks1");
    g_signal_connect ((gpointer) widget, "activate",
        G_CALLBACK (on_menu_cloaks_activate), NULL);

    widget = glade_xml_get_widget(xml_tree, "gloves1");
    g_signal_connect ((gpointer) widget, "activate",
        G_CALLBACK (on_menu_gloves_activate), NULL);

    widget = glade_xml_get_widget(xml_tree, "helmets1");
    g_signal_connect ((gpointer) widget, "activate",
        G_CALLBACK (on_menu_helmets_activate), NULL);

    widget = glade_xml_get_widget(xml_tree, "shields1");
    g_signal_connect ((gpointer) widget, "activate",
        G_CALLBACK (on_menu_shields_activate), NULL);

    widget = glade_xml_get_widget(xml_tree, "skillscrolls1");
    g_signal_connect ((gpointer) widget, "activate",
        G_CALLBACK (on_menu_skillscrolls_activate), NULL);

    widget = glade_xml_get_widget(xml_tree, "normal_book_scrolls1");
    g_signal_connect ((gpointer) widget, "activate",
        G_CALLBACK (on_menu_normal_book_scrolls_activate), NULL);

    widget = glade_xml_get_widget(xml_tree, "spellbooks1");
    g_signal_connect ((gpointer) widget, "activate",
        G_CALLBACK (on_menu_spellbooks_activate), NULL);

    widget = glade_xml_get_widget(xml_tree, "drinks1");
    g_signal_connect ((gpointer) widget, "activate",
        G_CALLBACK (on_menu_drinks_activate), NULL);

    widget = glade_xml_get_widget(xml_tree, "food1");
    g_signal_connect ((gpointer) widget, "activate",
        G_CALLBACK (on_menu_food_activate), NULL);

    widget = glade_xml_get_widget(xml_tree, "flesh1");
    g_signal_connect ((gpointer) widget, "activate",
        G_CALLBACK (on_menu_flesh_activate), NULL);

    widget = glade_xml_get_widget(xml_tree, "keys1");
    g_signal_connect ((gpointer) widget, "activate",
        G_CALLBACK (on_menu_keys_activate), NULL);

    widget = glade_xml_get_widget(xml_tree, "magical_items");
    g_signal_connect ((gpointer) widget, "activate",
        G_CALLBACK (on_menu_magical_items_activate), NULL);

    widget = glade_xml_get_widget(xml_tree, "potions");
    g_signal_connect ((gpointer) widget, "activate",
        G_CALLBACK (on_menu_potions_activate), NULL);

    widget = glade_xml_get_widget(xml_tree, "valuables");
    g_signal_connect ((gpointer) widget, "activate",
        G_CALLBACK (on_menu_valuables_activate), NULL);

    widget = glade_xml_get_widget(xml_tree, "wands_rods_horns");
    g_signal_connect ((gpointer) widget, "activate",
        G_CALLBACK (on_menu_wands_rods_horns_activate), NULL);

    widget = glade_xml_get_widget(xml_tree, "jewels1");
    g_signal_connect ((gpointer) widget, "activate",
        G_CALLBACK (on_menu_jewels_activate), NULL);

    widget = glade_xml_get_widget(xml_tree, "all_weapons");
    g_signal_connect ((gpointer) widget, "activate",
        G_CALLBACK (on_menu_all_weapons_activate), NULL);

    widget = glade_xml_get_widget(xml_tree, "missile_weapons1");
    g_signal_connect ((gpointer) widget, "activate",
        G_CALLBACK (on_menu_missile_weapons_activate), NULL);

    widget = glade_xml_get_widget(xml_tree, "bows1");
    g_signal_connect ((gpointer) widget, "activate",
        G_CALLBACK (on_menu_bows_activate), NULL);

    widget = glade_xml_get_widget(xml_tree, "arrows1");
    g_signal_connect ((gpointer) widget, "activate",
        G_CALLBACK (on_menu_arrows_activate), NULL);

    widget = glade_xml_get_widget(xml_tree, "ratio_pickup_off1");
    g_signal_connect ((gpointer) widget, "activate",
        G_CALLBACK (on_menu_ratio_pickup_off_activate), NULL);

    widget = glade_xml_get_widget(xml_tree, "ratio_5");
    g_signal_connect ((gpointer) widget, "activate",
        G_CALLBACK (on_menu_ratio_5_activate), NULL);

    widget = glade_xml_get_widget(xml_tree, "ratio_10");
    g_signal_connect ((gpointer) widget, "activate",
        G_CALLBACK (on_menu_ratio_10_activate), NULL);

    widget = glade_xml_get_widget(xml_tree, "ratio_15");
    g_signal_connect ((gpointer) widget, "activate",
        G_CALLBACK (on_menu_ratio_15_activate), NULL);

    widget = glade_xml_get_widget(xml_tree, "ratio_20");
    g_signal_connect ((gpointer) widget, "activate",
        G_CALLBACK (on_menu_ratio_20_activate), NULL);

    widget = glade_xml_get_widget(xml_tree, "ratio_25");
    g_signal_connect ((gpointer) widget, "activate",
        G_CALLBACK (on_menu_ratio_25_activate), NULL);

    widget = glade_xml_get_widget(xml_tree, "ratio_30");
    g_signal_connect ((gpointer) widget, "activate",
        G_CALLBACK (on_menu_ratio_35_activate), NULL);

    widget = glade_xml_get_widget(xml_tree, "ratio_35");
    g_signal_connect ((gpointer) widget, "activate",
        G_CALLBACK (on_menu_ratio_35_activate), NULL);

    widget = glade_xml_get_widget(xml_tree, "ratio_40");
    g_signal_connect ((gpointer) widget, "activate",
        G_CALLBACK (on_menu_ratio_40_activate), NULL);

    widget = glade_xml_get_widget(xml_tree, "ratio_45");
    g_signal_connect ((gpointer) widget, "activate",
        G_CALLBACK (on_menu_ratio_45_activate), NULL);

    widget = glade_xml_get_widget(xml_tree, "ratio_50");
    g_signal_connect ((gpointer) widget, "activate",
        G_CALLBACK (on_menu_ratio_50_activate), NULL);

    widget = glade_xml_get_widget(xml_tree, "not_cursed1");
    g_signal_connect ((gpointer) widget, "activate",
        G_CALLBACK (on_menu_not_cursed_activate), NULL);

    widget = glade_xml_get_widget(xml_tree, "about1");
    g_signal_connect ((gpointer) widget, "activate",
        G_CALLBACK (menu_about), NULL);
}
