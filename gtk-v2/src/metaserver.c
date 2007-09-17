char *rcsid_gtk2_metaserver_c =
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

/**
 * @file metaserver.c
 * Supports the client's metaserver dialog used to connect to available
 * servers.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>
#include <glade/glade.h>

#include "client.h"

#include "image.h"
#include "gtk2proto.h"
#include "metaserver.h"
#include "main.h"
#include <pthread.h>

static GtkWidget *metaserver_window, *treeview_metaserver, *metaserver_button,
    *metaserver_status, *metaserver_entry;
static GtkListStore    *store_metaserver;
static GtkTreeSelection  *metaserver_selection;

enum {
    LIST_HOSTNAME, LIST_IPADDR, LIST_IDLETIME, LIST_PLAYERS, LIST_VERSION, LIST_COMMENT
};

/**
 * Enables the connect button and clears the server entry box when a server is
 * navigated to or otherwise selected.
 *
 * @param selection
 * @param model
 * @param path
 * @param path_currently_selected
 * @ user_data
 * @return TRUE
 */
gboolean metaserver_selection_func (
                      GtkTreeSelection *selection,
                      GtkTreeModel     *model,
                      GtkTreePath      *path,
                      gboolean          path_currently_selected,
                      gpointer          userdata)
{
    gtk_widget_set_sensitive(metaserver_button, TRUE);
    gtk_entry_set_text(GTK_ENTRY(metaserver_entry), "");

    return TRUE;
}

/**
 * Constructs the metaserver dialog and handles metaserver selection.
 */
void get_metaserver()
{
    static int has_init=0;
    GtkTreeIter iter;
    int i, j;
    const gchar *metaserver_txt;
    GladeXML *xml_tree;
    GtkWidget *widget;

    if (!has_init) {
        GtkTreeViewColumn *column;
        GtkCellRenderer *renderer;

        metaserver_window = glade_xml_get_widget (dialog_xml,
            "metaserver_window");
        xml_tree = glade_get_widget_tree(GTK_WIDGET(metaserver_window));

        gtk_window_set_transient_for(GTK_WINDOW(metaserver_window), GTK_WINDOW(window_root));

        treeview_metaserver = glade_xml_get_widget(xml_tree,
            "treeview_metaserver");
        metaserver_button =
            glade_xml_get_widget(xml_tree, "metaserver_select");
        metaserver_status =
            glade_xml_get_widget(xml_tree, "metaserver_status");
        metaserver_entry =
            glade_xml_get_widget(xml_tree, "metaserver_text_entry");

        g_signal_connect ((gpointer) metaserver_window, "destroy",
            G_CALLBACK (on_window_destroy_event), NULL);
        g_signal_connect ((gpointer) treeview_metaserver, "row_activated",
            G_CALLBACK (on_treeview_metaserver_row_activated), NULL);
        g_signal_connect ((gpointer) metaserver_entry, "activate",
            G_CALLBACK (on_metaserver_text_entry_activate), NULL);
        g_signal_connect ((gpointer) metaserver_entry, "key_press_event",
            G_CALLBACK (on_metaserver_text_entry_key_press_event), NULL);
        g_signal_connect ((gpointer) metaserver_button, "clicked",
            G_CALLBACK (on_metaserver_select_clicked), NULL);

        widget = glade_xml_get_widget(xml_tree, "button_metaserver_quit");
        g_signal_connect ((gpointer) widget, "pressed",
            G_CALLBACK (on_button_metaserver_quit_pressed), NULL);
        g_signal_connect ((gpointer) widget, "activate",
            G_CALLBACK (on_button_metaserver_quit_pressed), NULL);

        store_metaserver = gtk_list_store_new (6,
                                G_TYPE_STRING,
                                G_TYPE_STRING,
                                G_TYPE_INT,
                                G_TYPE_INT,
                                G_TYPE_STRING,
                                G_TYPE_STRING);

        gtk_tree_view_set_model(GTK_TREE_VIEW(treeview_metaserver), GTK_TREE_MODEL(store_metaserver));

        renderer = gtk_cell_renderer_text_new ();
        column = gtk_tree_view_column_new_with_attributes ("Hostname", renderer,
                                                      "text", LIST_HOSTNAME,
                                                      NULL);
        gtk_tree_view_column_set_sort_column_id(column, LIST_HOSTNAME);
        gtk_tree_view_append_column (GTK_TREE_VIEW (treeview_metaserver), column);

        renderer = gtk_cell_renderer_text_new ();
        column = gtk_tree_view_column_new_with_attributes ("Updated (Sec)", renderer,
                                                      "text", LIST_IDLETIME,
                                                      NULL);
        gtk_tree_view_column_set_sort_column_id(column, LIST_IDLETIME);
        gtk_tree_view_append_column (GTK_TREE_VIEW (treeview_metaserver), column);

        renderer = gtk_cell_renderer_text_new ();
        column = gtk_tree_view_column_new_with_attributes ("Players", renderer,
                                                      "text", LIST_PLAYERS,
                                                      NULL);
        gtk_tree_view_column_set_sort_column_id(column, LIST_PLAYERS);
        gtk_tree_view_append_column (GTK_TREE_VIEW (treeview_metaserver), column);

        renderer = gtk_cell_renderer_text_new ();
        column = gtk_tree_view_column_new_with_attributes ("Version", renderer,
                                                      "text", LIST_VERSION,
                                                      NULL);
        gtk_tree_view_column_set_sort_column_id(column, LIST_VERSION);
        gtk_tree_view_append_column (GTK_TREE_VIEW (treeview_metaserver), column);

        renderer = gtk_cell_renderer_text_new ();
        column = gtk_tree_view_column_new_with_attributes ("Server Comment", renderer,
                                                      "text", LIST_COMMENT,
                                                      NULL);
        gtk_tree_view_append_column (GTK_TREE_VIEW (treeview_metaserver), column);

        gtk_widget_realize(metaserver_window);
        metaserver_selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview_metaserver));
        gtk_tree_selection_set_mode (metaserver_selection, GTK_SELECTION_BROWSE);
        gtk_tree_selection_set_select_function(metaserver_selection, metaserver_selection_func, NULL, NULL);

        has_init=1;
    }
    gtk_widget_show(metaserver_window);

    gtk_label_set_text(GTK_LABEL(metaserver_status), "Waiting for data from metaserver");

    metaserver_txt = gtk_entry_get_text(GTK_ENTRY(metaserver_entry));
    if (*metaserver_txt == '\0') {
        gtk_widget_set_sensitive(metaserver_button, FALSE);
    } else {
        gtk_widget_set_sensitive(metaserver_button, TRUE);
    }

    gtk_list_store_clear(store_metaserver);

    while (metaserver_check_status()) {
        usleep(100);
        gtk_main_iteration_do(FALSE);
    }
        
    pthread_mutex_lock(&ms2_info_mutex);

    if (cached_servers_num) {
        for ( i = 0; i < cached_servers_num; i++ ) {
            for (j=0; j < meta_numservers; j++) {
                if (!strcmp(cached_servers_name[i], meta_servers[j].hostname))
                    break;
            }
            if (j == meta_numservers) {
                gtk_list_store_append(store_metaserver, &iter);
                gtk_list_store_set(store_metaserver, &iter,
                               LIST_HOSTNAME, cached_servers_name[i],
                               LIST_IPADDR, cached_servers_ip[i],
                               LIST_COMMENT, "Cached server entry",
                               -1);
            }
        }
    }

    qsort(meta_servers, meta_numservers, sizeof(Meta_Info), (int (*)(const void *, const void *))meta_sort);

    for (i=0; i<meta_numservers; i++) {
        if (check_server_version(i)){
            gtk_list_store_append(store_metaserver, &iter);
            gtk_list_store_set(store_metaserver, &iter,
                               LIST_HOSTNAME, meta_servers[i].hostname,
                               LIST_IPADDR, meta_servers[i].hostname,
                               LIST_IDLETIME,  meta_servers[i].idle_time,
                               LIST_PLAYERS, meta_servers[i].num_players,
                               LIST_VERSION, meta_servers[i].version,
                               LIST_COMMENT, meta_servers[i].text_comment,
                               -1);
        }
    }
    pthread_mutex_unlock(&ms2_info_mutex);
    if (server) {
        gtk_list_store_append(store_metaserver, &iter);
        gtk_list_store_set(store_metaserver, &iter,
                               LIST_HOSTNAME, server,
                               LIST_COMMENT, "default server",
                               -1);
    }

    cpl.input_state = Metaserver_Select;
    gtk_label_set_text(GTK_LABEL(metaserver_status), "Waiting for user selection");

    enable_menu_items(FALSE);
    /* draw_map() is to just redraw the splash */
    draw_map(FALSE);
    gtk_main();

    gtk_widget_hide(metaserver_window);
    enable_menu_items(TRUE);
}

/**
 * Establish a connection with the server when pressing the connect button.
 *
 * @param button
 * @param user_data
 */
void
on_metaserver_select_clicked           (GtkButton       *button,
                                        gpointer         user_data)
{
    GtkTreeModel    *model;
    GtkTreeIter iter;
    char    *name=NULL, *ip=NULL, buf[256], *metaserver_txt;

    metaserver_txt = (char*)gtk_entry_get_text(GTK_ENTRY(metaserver_entry));
    if (gtk_tree_selection_get_selected (metaserver_selection, &model, &iter)) {
        gtk_tree_model_get(model, &iter, LIST_HOSTNAME, &name, LIST_IPADDR, &ip, -1);

    } else if (*metaserver_txt == '\0') {

        /* This can happen if user blanks out server name text field then hits
         * the connect button.
         */
        gtk_label_set_text(GTK_LABEL(metaserver_status), "Error - nothing selected!\n");
        gtk_widget_set_sensitive(metaserver_button, FALSE);
        return;
    } else {
        /* This shouldn't happen because the button should not be pressable
         * until the user selects something
         */
        gtk_label_set_text(GTK_LABEL(metaserver_status), "Error - nothing selected!\n");
    }
    if (!name) name = metaserver_txt;

    snprintf(buf, 255, "Trying to connect to %s", name);

    gtk_label_set_text(GTK_LABEL(metaserver_status), buf);

    csocket.fd=init_connection(ip?ip:name, use_config[CONFIG_PORT]);
    if (csocket.fd==-1) {
        snprintf(buf, 255, "Unable to connect to %s!", name);
        gtk_label_set_text(GTK_LABEL(metaserver_status), buf);
    } else {
        snprintf(buf, 255, "Connected to %s!", name);
        gtk_label_set_text(GTK_LABEL(metaserver_status), buf);
        gtk_main_quit();
        cpl.input_state = Playing;
    }
}

/**
 * Establish a connection to a server when the server name was entered.  Either
 * a DNS name or IP address may be specified, but if both are supplied, use the
 * IP address.
 *
 * @param name The DNS name of a server to connect to.
 * @param ip An IP address of a server to connect to.
 */
static void metaserver_connect_to(const char *name, const char *ip)
{
    char  buf[256], *cp, newname[256];
    int port=use_config[CONFIG_PORT];

    snprintf(buf, 255, "Trying to connect to %s", name);

    gtk_label_set_text(GTK_LABEL(metaserver_status), buf);
    strncpy(newname, name, 255);
    newname[255]=0;

    if ((cp=strchr(newname,':'))!=NULL) {
	port = atoi(cp+1);
	*cp=0;
    }

    csocket.fd=init_connection((char*)(ip?ip:newname), port);
    if (csocket.fd==-1) {
        snprintf(buf, 255, "Unable to connect to %s!", name);
        gtk_label_set_text(GTK_LABEL(metaserver_status), buf);
    } else {
        snprintf(buf, 255, "Connected to %s!", name);
        gtk_label_set_text(GTK_LABEL(metaserver_status), buf);
        gtk_main_quit();
        cpl.input_state = Playing;
    }
}

/**
 * Selects and attempts a connection to a server if the player activates one of
 * the server entries.
 *
 * @param treeview
 * @param path
 * @param column
 * @param user_data
 */
void
on_treeview_metaserver_row_activated   (GtkTreeView     *treeview,
                                        GtkTreePath     *path,
                                        GtkTreeViewColumn *column,
                                        gpointer         user_data)
{
    GtkTreeIter iter;
    GtkTreeModel    *model;
    char    *name, *ip;

    model = gtk_tree_view_get_model(treeview);
    if (gtk_tree_model_get_iter(model, &iter, path)) {
        gtk_tree_model_get(model, &iter, LIST_HOSTNAME, &name, LIST_IPADDR, &ip, -1);

        metaserver_connect_to(name, ip);
    }
}

/**
 * This callback handles the user entering text into the metaserver freeform
 * entry box.
 *
 * @param entry
 * @param user_data
 */
void
on_metaserver_text_entry_activate      (GtkEntry        *entry,
                                        gpointer         user_data)
{
    const gchar *entry_text;

    entry_text = gtk_entry_get_text(GTK_ENTRY(entry));

    metaserver_connect_to(entry_text, NULL);
}

/**
 * Quits the client application if the quit button is pressed.  This is also
 * used to quit the client if the button's accelerator is pressed.
 *
 * @param button
 * @param user_data
 */
void
on_button_metaserver_quit_pressed      (GtkButton       *button,
                                        gpointer         user_data)
{
#ifdef WIN32
        script_killall();
#endif
        exit(0);

}

/**
 * Activate the connect button and unselect servers if keys are pressed to
 * enter a server name.
 *
 * @param widget
 * @param event
 * @param user_data
 * @return FALSE
 */
gboolean
on_metaserver_text_entry_key_press_event
                                        (GtkWidget       *widget,
                                        GdkEventKey     *event,
                                        gpointer         user_data)
{

    gtk_widget_set_sensitive(metaserver_button, TRUE);
    gtk_tree_selection_unselect_all(metaserver_selection);
    return FALSE;
}

