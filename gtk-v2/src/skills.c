

/*
    Crossfire client, a client program for the crossfire program.

    Copyright (C) 2006-2007,2010 Mark Wedel & Crossfire Development Team

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
 * @file gtk-v2/src/skills.c
 * Handles The callbacks for the skill window.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>
#include <glade/glade.h>

#include "client.h"

#include "image.h"
#include "metaserver.h"
#include "main.h"
#include "gtk2proto.h"


static GtkWidget *skill_window, *skill_treeview, *skill_use, *skill_ready;
static GtkListStore *skill_store;
static GtkTreeSelection *skill_selection;

enum {LIST_NAME, LIST_LEVEL, LIST_EXP, LIST_NEXTLEVEL};

static int has_init = 0;

/**
 * Used if a user just single clicks on an entry - at which point, we enable
 * the cast & invoke buttons.
 *
 * @param selection
 * @param model
 * @param path
 * @param path_currently_selected
 * @param userdata
 */
static gboolean skill_selection_func(GtkTreeSelection *selection,
                                     GtkTreeModel     *model,
                                     GtkTreePath      *path,
                                     gboolean          path_currently_selected,
                                     gpointer          userdata) {
    gtk_widget_set_sensitive(skill_ready, TRUE);
    gtk_widget_set_sensitive(skill_use, TRUE);
    return TRUE;
}

/**
 * Called whenever the skill window is opened or a stats packet is received.
 * If the skills window has been created and is currently visible, it rebuilds
 * the list store otherwise nothing happens, because it will be called again
 * next time the window is opened anyway.
 */
void update_skill_information(void) {
    GtkTreeIter iter;
    char buf[MAX_BUF];
    int i, sk, level;
    uint64 exp_to_next_level;

    /* If the window/spellstore hasn't been created, or isn't currently being
     * shown, return.
     */
    if (! has_init
    ||  ! GTK_WIDGET_VISIBLE(glade_xml_get_widget(dialog_xml, "skill_window")))
        return;

    gtk_list_store_clear(skill_store);
    for (i = 0; i<MAX_SKILL; i++) {
        sk = skill_mapping[i].value;
        level = cpl.stats.skill_level[sk];
        if (level > 0) {
            gtk_list_store_append(skill_store, &iter);
            buf[0] = 0;
            if (level>= exp_table_max) {
                /* we can't advance any more, so display 0*/
                exp_to_next_level = 0;
            } else {
                exp_to_next_level = 
                    exp_table[level + 1] - cpl.stats.skill_exp[sk];
            }
            gtk_list_store_set(skill_store, &iter,
                               LIST_NAME, skill_mapping[i].name,
                               LIST_LEVEL, level,
                               LIST_EXP, cpl.stats.skill_exp[sk],
                               LIST_NEXTLEVEL, exp_to_next_level,
                               -1);
        }
    }
}

/**
 *
 * @param menuitem
 * @param user_data
 */
void on_skills_activate(GtkMenuItem *menuitem, gpointer user_data) {
    GladeXML *xml_tree;
    GtkWidget *widget;

    if (! has_init) {
        GtkCellRenderer *renderer;
        GtkTreeViewColumn *column;

        skill_window = glade_xml_get_widget(dialog_xml, "skill_window");
        xml_tree = glade_get_widget_tree(GTK_WIDGET(skill_window));

        skill_use = glade_xml_get_widget(xml_tree,"skill_use");
        skill_ready = glade_xml_get_widget(xml_tree,"skill_ready");
        skill_treeview = glade_xml_get_widget(xml_tree, "skill_treeview");

        g_signal_connect((gpointer) skill_window, "delete_event",
            G_CALLBACK(gtk_widget_hide_on_delete), NULL);
        g_signal_connect((gpointer) skill_treeview, "row_activated",
            G_CALLBACK(on_skill_treeview_row_activated), NULL);
        g_signal_connect((gpointer) skill_ready, "clicked",
            G_CALLBACK(on_skill_ready_clicked), NULL);
        g_signal_connect((gpointer) skill_use, "clicked",
            G_CALLBACK(on_skill_use_clicked), NULL);

        widget = glade_xml_get_widget(xml_tree, "skill_close");
        g_signal_connect((gpointer) widget, "clicked",
            G_CALLBACK(on_skill_close_clicked), NULL);

        skill_store = gtk_list_store_new(4,
                                         G_TYPE_STRING, /* Name */
                                         G_TYPE_INT,    /* Level */
                                         G_TYPE_INT64,  /* EXP */
                                         G_TYPE_INT64   /* Exp to Next Level */
                                        );

        gtk_tree_view_set_model(
            GTK_TREE_VIEW(skill_treeview), GTK_TREE_MODEL(skill_store));
        gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(skill_treeview), TRUE);

        renderer = gtk_cell_renderer_text_new();
        column = gtk_tree_view_column_new_with_attributes(
                     "Skill", renderer, "text", LIST_NAME, NULL);
        gtk_tree_view_append_column(GTK_TREE_VIEW(skill_treeview), column);
        gtk_tree_view_column_set_sort_column_id(column, LIST_NAME);

        renderer = gtk_cell_renderer_text_new();
        column = gtk_tree_view_column_new_with_attributes(
                     "Level", renderer, "text", LIST_LEVEL, NULL);
        gtk_tree_view_append_column(GTK_TREE_VIEW(skill_treeview), column);
        gtk_tree_view_column_set_sort_column_id(column, LIST_LEVEL);


        renderer = gtk_cell_renderer_text_new();
        column = gtk_tree_view_column_new_with_attributes(
                     "Exp", renderer, "text", LIST_EXP, NULL);
        gtk_tree_view_append_column(GTK_TREE_VIEW(skill_treeview), column);
        gtk_tree_view_column_set_sort_column_id(column, LIST_EXP);

        renderer = gtk_cell_renderer_text_new();
        column = gtk_tree_view_column_new_with_attributes(
                     "Needed for next level", renderer, "text",
                         LIST_NEXTLEVEL, NULL);
        gtk_tree_view_append_column(GTK_TREE_VIEW(skill_treeview), column);
        gtk_tree_view_column_set_sort_column_id(column, LIST_NEXTLEVEL);

        skill_selection = gtk_tree_view_get_selection(
                              GTK_TREE_VIEW(skill_treeview));
        gtk_tree_selection_set_mode(skill_selection, GTK_SELECTION_BROWSE);
        gtk_tree_selection_set_select_function(
            skill_selection, skill_selection_func, NULL, NULL);

        gtk_tree_sortable_set_sort_column_id(
            GTK_TREE_SORTABLE(skill_store), LIST_NAME, GTK_SORT_ASCENDING);

    }
    gtk_widget_set_sensitive(skill_ready, FALSE);
    gtk_widget_set_sensitive(skill_use, FALSE);
    gtk_widget_show(skill_window);

    has_init=1;
    /* has to be called after has_init is set to 1 */
    update_skill_information();
}

/**
 * This is where we actually do something with the skill. model and iter tell us
 * which skill we want to trigger, use_skill is 1 to use the skill, 0 to ready it.
 * @param iter
 * @param model
 * @param use_skill
  */

void trigger_skill(GtkTreeIter iter, GtkTreeModel *model, int use_skill) {
        gchar *skname;
        char command[MAX_BUF];
        char *commandname;

        gtk_tree_model_get(model, &iter, LIST_NAME, &skname, -1);
        if (! skname) {
            LOG(LOG_ERROR, "skills.c::trigger_skill",
                "Unable to get skill name\n");
            return;
        }
        commandname = use_skill?"use_skill":"ready_skill";
        snprintf(command, MAX_BUF-1, "%s %s", commandname, skname);
        send_command(command, -1, 1);
        g_free(skname);
}

/**
 *
 * @param treeview
 * @param path
 * @param column
 * @param user_data
 */
void on_skill_treeview_row_activated(GtkTreeView *treeview, GtkTreePath *path,
                                 GtkTreeViewColumn *column, gpointer user_data) {
    GtkTreeIter iter;
    GtkTreeModel *model;

    model = gtk_tree_view_get_model(treeview);
    if (gtk_tree_model_get_iter(model, &iter, path)) {
        trigger_skill(iter, model, 0);
    }
    gtk_widget_hide(skill_window);
}

/**
 *
 * @param button
 * @param user_data
 */
void on_skill_ready_clicked(GtkButton *button, gpointer user_data) {
    GtkTreeIter iter;
    GtkTreeModel *model;

    if (gtk_tree_selection_get_selected(skill_selection, &model, &iter)) {
        trigger_skill(iter, model, 0);
    }
    gtk_widget_hide(skill_window);
}

/**
 *
 * @param button
 * @param user_data
 */
void on_skill_use_clicked(GtkButton *button, gpointer user_data) {
    GtkTreeIter iter;
    GtkTreeModel *model;

    if (gtk_tree_selection_get_selected(skill_selection, &model, &iter)) {
        trigger_skill(iter, model, 1);
    }
}

/**
 *
 * @param button
 * @param user_data
 */
void on_skill_close_clicked(GtkButton *button, gpointer user_data) {
    gtk_widget_hide(skill_window);
}

