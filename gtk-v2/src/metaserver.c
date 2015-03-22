/*
 * Crossfire -- cooperative multi-player graphical RPG and adventure game
 *
 * Copyright (c) 1999-2014 Mark Wedel and the Crossfire Development Team
 * Copyright (c) 1992 Frank Tore Johansen
 *
 * Crossfire is free software and comes with ABSOLUTELY NO WARRANTY. You are
 * welcome to redistribute it under certain conditions. For details, please
 * see COPYING and LICENSE.
 *
 * The authors can be reached via e-mail at <crossfire@metalforge.org>.
 */

/**
 * @file gtk-v2/src/metaserver.c
 * Supports the client's metaserver dialog used to connect to available
 * servers.
 */

#include "client.h"

#include <gtk/gtk.h>
#include <stdbool.h>

#include "image.h"
#include "main.h"
#include "metaserver.h"
#include "gtk2proto.h"

static GtkWidget *metaserver_window, *treeview_metaserver, *metaserver_button,
       *metaserver_status, *metaserver_entry;
static GtkListStore *store_metaserver;
static GtkTreeSelection *metaserver_selection;

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
 * @param userdata
 * @return TRUE
 */
gboolean metaserver_selection_func(GtkTreeSelection *selection,
        GtkTreeModel *model, GtkTreePath *path,
        gboolean path_currently_selected, gpointer userdata) {
    gtk_widget_set_sensitive(metaserver_button, TRUE);
    gtk_entry_set_text(GTK_ENTRY(metaserver_entry), "");
    return TRUE;
}

/**
 * Initialize the metaserver user interface.
 */
void metaserver_ui_init() {
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    GtkWidget *widget;

    metaserver_window = GTK_WIDGET(gtk_builder_get_object(dialog_xml,
                                   "metaserver_window"));

    gtk_window_set_transient_for(GTK_WINDOW(metaserver_window),
                                 GTK_WINDOW(window_root));

    treeview_metaserver = GTK_WIDGET(gtk_builder_get_object(dialog_xml,
                                     "treeview_metaserver"));
    metaserver_button = GTK_WIDGET(gtk_builder_get_object(dialog_xml,
                                   "metaserver_select"));
    metaserver_status = GTK_WIDGET(gtk_builder_get_object(dialog_xml,
                                   "metaserver_status"));
    metaserver_entry = GTK_WIDGET(gtk_builder_get_object(dialog_xml,
                                  "metaserver_text_entry"));

    g_signal_connect((gpointer) metaserver_window, "destroy",
                     G_CALLBACK(on_window_destroy_event), NULL);
    g_signal_connect((gpointer) treeview_metaserver, "row_activated",
                     G_CALLBACK(on_treeview_metaserver_row_activated), NULL);
    g_signal_connect((gpointer) metaserver_entry, "activate",
                     G_CALLBACK(on_metaserver_text_entry_activate), NULL);
    g_signal_connect((gpointer) metaserver_entry, "key_press_event",
                     G_CALLBACK(on_metaserver_text_entry_key_press_event), NULL);
    g_signal_connect((gpointer) metaserver_button, "clicked",
                     G_CALLBACK(on_metaserver_select_clicked), NULL);

    widget = GTK_WIDGET(gtk_builder_get_object(dialog_xml,
                        "button_metaserver_quit"));
    g_signal_connect((gpointer) widget, "pressed",
                     G_CALLBACK(on_button_metaserver_quit_pressed), NULL);
    g_signal_connect((gpointer) widget, "activate",
                     G_CALLBACK(on_button_metaserver_quit_pressed), NULL);

    store_metaserver = gtk_list_store_new(6,
                                          G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT,
                                          G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING);

    gtk_tree_view_set_model(GTK_TREE_VIEW(treeview_metaserver),
                            GTK_TREE_MODEL(store_metaserver));

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Server", renderer,
             "text", LIST_HOSTNAME, NULL);
    gtk_tree_view_column_set_sort_column_id(column, LIST_HOSTNAME);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview_metaserver), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Last Update (sec)", renderer,
             "text", LIST_IDLETIME, NULL);
    gtk_tree_view_column_set_sort_column_id(column, LIST_IDLETIME);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview_metaserver), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Players", renderer,
             "text", LIST_PLAYERS, NULL);
    gtk_tree_view_column_set_sort_column_id(column, LIST_PLAYERS);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview_metaserver), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Version", renderer,
             "text", LIST_VERSION, NULL);
    gtk_tree_view_column_set_sort_column_id(column, LIST_VERSION);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview_metaserver), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Description", renderer,
             "text", LIST_COMMENT, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview_metaserver), column);

    gtk_widget_realize(metaserver_window);
    metaserver_selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(
                               treeview_metaserver));
    gtk_tree_selection_set_mode(metaserver_selection, GTK_SELECTION_BROWSE);
    gtk_tree_selection_set_select_function(metaserver_selection,
                                           metaserver_selection_func, NULL, NULL);
}

/**
 * Determine whether the given server exists in the metaserver list.
 * @param server
 * @return
 */
static bool server_exists(const char *server) {
    GtkTreeIter iter;
    char *name;

    bool valid = gtk_tree_model_get_iter_first(
            GTK_TREE_MODEL(store_metaserver), &iter);

    while (valid) {
        gtk_tree_model_get(GTK_TREE_MODEL(store_metaserver), &iter,
                LIST_HOSTNAME, &name, -1);

        if (strcmp(server, name) == 0) {
            return true;
        }

        valid = gtk_tree_model_iter_next(
                GTK_TREE_MODEL(store_metaserver), &iter);
    }

    return false;
}

static void server_add(char *server, int update, int players, char *version,
        char *comment, bool compatible) {
    GtkTreeIter iter;

    if (compatible && !server_exists(server)) {
        gtk_list_store_append(store_metaserver, &iter);
        gtk_list_store_set(store_metaserver, &iter,
                LIST_HOSTNAME, server,
                LIST_IPADDR, server,
                LIST_IDLETIME, update,
                LIST_PLAYERS, players,
                LIST_VERSION, version,
                LIST_COMMENT, comment,
                -1);
    }
}

/**
 * Wrapper on top of ms_fetch() for a GThread.
 * @return NULL
 */
static gpointer server_fetch() {
    ms_fetch();
    return NULL;
}

/**
 * Constructs the metaserver dialog and handles metaserver selection.  If the
 * player has a servers.cache file in their .crossfire folder, the cached
 * server list is added to the contents of the metaserver dialog.
 */
void prompt_metaserver() {
    const gchar *metaserver_txt;

    hide_all_login_windows();
    gtk_widget_show(metaserver_window);
    gtk_label_set_text(GTK_LABEL(metaserver_status),
            "Waiting for data from metaserver");

    metaserver_txt = gtk_entry_get_text(GTK_ENTRY(metaserver_entry));
    if (*metaserver_txt == '\0') {
        gtk_widget_set_sensitive(metaserver_button, FALSE);
    } else {
        gtk_widget_set_sensitive(metaserver_button, TRUE);
    }

    gtk_list_store_clear(store_metaserver);

    // Start fetching server information in a separate thread.
    ms_set_callback(server_add);
    g_thread_new("server_fetch", server_fetch, NULL);

    cpl.input_state = Metaserver_Select;
    gtk_label_set_text(GTK_LABEL(metaserver_status), "Waiting for user selection");

    gtk_main();
    gtk_widget_hide(metaserver_window);
}

/**
 * Connect to a server with the given hostname and optional port number.
 *
 * @param name The DNS name of a server to connect to.  If the server operates
 *             on a non-standard port, a colon and the port number is appended
 *             to the DNS name.  For example:  localhost:8000
 */
static void metaserver_connect_to(const char *name) {
    char buf[256];
    /* Set client status and update GUI before continuing. */
    snprintf(buf, sizeof(buf), "Connecting to '%s'...", name);
    gtk_label_set_text(GTK_LABEL(metaserver_status), buf);
    gtk_main_iteration();

    csocket.fd = init_connection(name);
    if (csocket.fd != -1) {
        LOG(LOG_DEBUG, "metaserver_connect_to", "Connected to '%s'!", name);
        gtk_main_quit();
        cpl.input_state = Playing;
    } else {
        snprintf(buf, sizeof(buf), "Unable to connect to %s!", name);
        gtk_label_set_text(GTK_LABEL(metaserver_status), buf);
    }
}

/**
 * Establish a connection with the server when pressing the connect button.
 *
 * @param button
 * @param user_data
 */
void on_metaserver_select_clicked(GtkButton *button, gpointer user_data) {
    GtkTreeModel *model;
    GtkTreeIter iter;
    char *name = NULL, *ip = NULL, *metaserver_txt;

    metaserver_txt = (char *)gtk_entry_get_text(GTK_ENTRY(metaserver_entry));
    if (gtk_tree_selection_get_selected(metaserver_selection, &model, &iter)) {
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
    if (!name) {
        name = metaserver_txt;
    }

    metaserver_connect_to(name);
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
void on_treeview_metaserver_row_activated(GtkTreeView *treeview,
        GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data) {
    GtkTreeIter iter;
    GtkTreeModel *model;
    char *name, *ip;

    model = gtk_tree_view_get_model(treeview);
    if (gtk_tree_model_get_iter(model, &iter, path)) {
        gtk_tree_model_get(model, &iter, LIST_HOSTNAME, &name, LIST_IPADDR, &ip, -1);
        metaserver_connect_to(name);
    }
}

/**
 * This callback handles the user entering text into the metaserver freeform
 * entry box.
 *
 * @param entry
 * @param user_data
 */
void on_metaserver_text_entry_activate(GtkEntry *entry, gpointer user_data) {
    const gchar *entry_text;
    entry_text = gtk_entry_get_text(GTK_ENTRY(entry));
    metaserver_connect_to(entry_text);
}

/**
 * Quits the client application if the quit button is pressed.  This is also
 * used to quit the client if the button's accelerator is pressed.
 *
 * @param button
 * @param user_data
 */
void on_button_metaserver_quit_pressed(GtkButton *button, gpointer user_data) {
    on_window_destroy_event(GTK_OBJECT(button), user_data);
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
gboolean on_metaserver_text_entry_key_press_event(GtkWidget *widget,
        GdkEventKey *event, gpointer user_data) {
    gtk_widget_set_sensitive(metaserver_button, TRUE);
    gtk_tree_selection_unselect_all(metaserver_selection);
    return FALSE;
}
