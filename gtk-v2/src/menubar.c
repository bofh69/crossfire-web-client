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

#include "client.h"

#include "callbacks.h"
#include "interface.h"
#include "support.h"
#include "p_cmd.h"
#include "main.h"

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

    gtk_widget_set_sensitive(lookup_widget(window_root,"quit_character1"), enable);
    gtk_widget_set_sensitive(lookup_widget(window_root,"disconnect"), enable);
    gtk_widget_set_sensitive(lookup_widget(window_root,"spells"), enable);
    gtk_widget_set_sensitive(lookup_widget(window_root,"pickup1"), enable);

}
