char *rcsid_gtk2_spells_c =
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>

#include "client.h"

#include "image.h"
#include "gtk2proto.h"
#include "interface.h"
#include "support.h"
#include "metaserver.h"
#include "main.h"

static GtkWidget *spell_window, *spell_treeview, *spell_invoke, *spell_cast,
    *spell_drawingarea_attuned, *spell_drawingarea_repelled, *spell_drawingarea_denied,
    *spell_options;
static GtkListStore    *spell_store;
static GtkTreeSelection  *spell_selection;

enum {LIST_IMAGE, LIST_NAME, LIST_LEVEL, LIST_TIME, LIST_COST, 
    LIST_DAMAGE, LIST_SKILL, LIST_PATH, LIST_DESCRIPTION, LIST_BACKGROUND, LIST_MAX_SP, LIST_TAG};

static int has_init=0;


/* This is used if a user just single clicks on an
 * entry - at which point, we enable the cast &
 * invoke buttons.
 */
static gboolean spell_selection_func (
                      GtkTreeSelection *selection,
                      GtkTreeModel     *model,
                      GtkTreePath      *path,
                      gboolean          path_currently_selected,
                      gpointer          userdata)
{
    gtk_widget_set_sensitive(spell_invoke, TRUE);
    gtk_widget_set_sensitive(spell_cast, TRUE);
    return TRUE;
}

void update_spell_information()
{
    Spell *spell;
    GtkTreeIter iter;
    char buf[MAX_BUF];
    int color;

    /* If the window/spellstore hasn't been created, return. */
    if (!has_init) return;
    cpl.spells_updated=0;

    gtk_list_store_clear(spell_store);
    for (spell = cpl.spelldata; spell; spell=spell->next) {
	gtk_list_store_append(spell_store, &iter);

	buf[0] = 0;
	if (spell->sp) sprintf(buf,"%d Mana ", spell->sp);
	if (spell->grace) sprintf(buf + strlen(buf), "%d Grace", spell->grace);

	if (spell->path & cpl.stats.denied) { color = NDI_RED; }
	else if (spell->path & cpl.stats.repelled) { color = NDI_ORANGE; }
	else if (spell->path & cpl.stats.attuned) { color = NDI_GREEN; }
	else color=NDI_WHITE;

	gtk_list_store_set(spell_store, &iter,
			   LIST_NAME, spell->name,
			   LIST_LEVEL, spell->level,
			   LIST_COST, buf,
			   LIST_DAMAGE, spell->dam,
			   LIST_SKILL, spell->skill,
			   LIST_DESCRIPTION, spell->message,
			   LIST_BACKGROUND, &root_color[color],
			   LIST_MAX_SP, (spell->sp > spell->grace)?spell->sp:spell->grace,
			   LIST_TAG, spell->tag,
			   -1);
    }
}

void
on_spells_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    if (!has_init) {
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	spell_window = create_spell_window();
	spell_invoke = lookup_widget(spell_window,"spell_invoke");
	spell_cast = lookup_widget(spell_window,"spell_cast");

	spell_options = lookup_widget(spell_window,"spell_options");

	spell_treeview = lookup_widget(spell_window, "spell_treeview");

	spell_store = gtk_list_store_new(12,
				G_TYPE_OBJECT,	/* Image - not used */
				G_TYPE_STRING,	/* Name */
				G_TYPE_INT,	/* Level */
				G_TYPE_INT,	/* Time */
				G_TYPE_STRING,	/* SP/Grace */
				G_TYPE_INT,	/* Damage */
				G_TYPE_STRING,	/* Skill name */
				G_TYPE_INT,	/* Spell path */
				G_TYPE_STRING,	/* Description */
				GDK_TYPE_COLOR,	/* Change the background color of the entry */
				G_TYPE_INT,
				G_TYPE_INT
			      );

        gtk_tree_view_set_model(GTK_TREE_VIEW(spell_treeview), GTK_TREE_MODEL(spell_store));
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(spell_treeview), TRUE);

	/* Note it is intentional we don't show (render) some fields:
	 * image - we don't have images right now it seems.
	 * time - not sure if it worth the space.
	 * spell path - done by color
	 */
	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes ("Spell", renderer,
						"text", LIST_NAME,
						NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (spell_treeview), column);
	gtk_tree_view_column_set_sort_column_id(column, LIST_NAME);
	gtk_tree_view_column_add_attribute(column, renderer, "background-gdk", LIST_BACKGROUND);

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes ("Level", renderer,
						"text", LIST_LEVEL,
						NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (spell_treeview), column);
	gtk_tree_view_column_set_sort_column_id(column, LIST_LEVEL);
	gtk_tree_view_column_add_attribute(column, renderer, "background-gdk", LIST_BACKGROUND);

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes ("SP/Mana Cost", renderer,
						"text", LIST_COST,
						NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (spell_treeview), column);

	/* since this is a string column, it would do a string sort.  Instead,
	 * we set up a int column and tie this column to sort on that.
	 */
	gtk_tree_view_column_set_sort_column_id(column, LIST_MAX_SP);
	gtk_tree_view_column_add_attribute(column, renderer, "background-gdk", LIST_BACKGROUND);

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes ("Damage", renderer,
						"text", LIST_DAMAGE,
						NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (spell_treeview), column);
	gtk_tree_view_column_set_sort_column_id(column, LIST_DAMAGE);
	gtk_tree_view_column_add_attribute(column, renderer, "background-gdk", LIST_BACKGROUND);

	column = gtk_tree_view_column_new_with_attributes ("Skill", renderer,
						"text", LIST_SKILL,
						NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (spell_treeview), column);
	gtk_tree_view_column_set_sort_column_id(column, LIST_SKILL);
	gtk_tree_view_column_add_attribute(column, renderer, "background-gdk", LIST_BACKGROUND);

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes ("Description", renderer,
						"text", LIST_DESCRIPTION,
						NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (spell_treeview), column);
	gtk_tree_view_column_add_attribute(column, renderer, "background-gdk", LIST_BACKGROUND);

	spell_selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(spell_treeview));
	gtk_tree_selection_set_mode (spell_selection, GTK_SELECTION_BROWSE);
	gtk_tree_selection_set_select_function(spell_selection, spell_selection_func,
					       NULL, NULL);


	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(spell_store),
					     LIST_NAME, 
					     GTK_SORT_ASCENDING);
    }
    gtk_widget_set_sensitive(spell_invoke, FALSE);
    gtk_widget_set_sensitive(spell_cast, FALSE);
    gtk_widget_show(spell_window);

    if (!has_init) {
	has_init=1;

	/* This block is to handle the drawing of the color keys on what
	 * the colors in the list mean.  I don't see a convenient way
	 * to set this in glade.  Which is fine - by using the root_color[]
	 * values, we are assured the colors matching at least.
	 * This has to be done after the gtk_widget_show() call, as drawingarea->window
	 * doesn't exist until then.
	 */
	spell_drawingarea_attuned = lookup_widget(spell_window,"spell_drawingarea_attuned");
	gdk_window_set_background(spell_drawingarea_attuned->window, &root_color[NDI_GREEN]);
	gdk_window_clear(spell_drawingarea_attuned->window);

	spell_drawingarea_repelled = lookup_widget(spell_window,"spell_drawingarea_repelled");
	gdk_window_set_background(spell_drawingarea_repelled->window, &root_color[NDI_ORANGE]);
	gdk_window_clear(spell_drawingarea_repelled->window);

	spell_drawingarea_denied = lookup_widget(spell_window,"spell_drawingarea_denied");
	gdk_window_set_background(spell_drawingarea_denied->window, &root_color[NDI_RED]);
	gdk_window_clear(spell_drawingarea_denied->window);
    }

    /* has to be called after has_init is set to 1 */
    update_spell_information();

}


void
on_spell_treeview_row_activated        (GtkTreeView     *treeview,
                                        GtkTreePath     *path,
                                        GtkTreeViewColumn *column,
                                        gpointer         user_data)
{
    GtkTreeIter iter;
    GtkTreeModel    *model;
    int	tag;
    char    command[MAX_BUF];
    const char *options = NULL;

    model = gtk_tree_view_get_model(treeview);
    if (gtk_tree_model_get_iter(model, &iter, path)) {
        gtk_tree_model_get(model, &iter, LIST_TAG, &tag, -1);

        if (!tag) {
            LOG(LOG_ERROR,"spells.c:on_spell_cast_clicked", "Unable to get spell tag\n");
            return;
	}
	snprintf(command, MAX_BUF-1, "cast %d %s", tag, options);
	send_command(command, -1, 1);
    }
}


void
on_spell_cast_clicked                  (GtkButton       *button,
                                        gpointer         user_data)
{
    const char *options = NULL;
    char    command[MAX_BUF];
    GtkTreeIter iter;
    GtkTreeModel    *model;
    int	tag;

    options = gtk_entry_get_text(GTK_ENTRY(spell_options));

    if (gtk_tree_selection_get_selected (spell_selection, &model, &iter)) {
        gtk_tree_model_get(model, &iter, LIST_TAG, &tag, -1);

        if (!tag) {
            LOG(LOG_ERROR,"spells.c:on_spell_cast_clicked", "Unable to get spell tag\n");
            return;
	}
	snprintf(command, MAX_BUF-1, "cast %d %s", tag, options);
	send_command(command, -1, 1);
    }

}


void
on_spell_invoke_clicked                 (GtkButton       *button,
                                        gpointer         user_data)
{
    const char *options = NULL;
    char    command[MAX_BUF];
    GtkTreeIter iter;
    GtkTreeModel    *model;
    int	tag;

    options = gtk_entry_get_text(GTK_ENTRY(spell_options));

    if (gtk_tree_selection_get_selected (spell_selection, &model, &iter)) {
        gtk_tree_model_get(model, &iter, LIST_TAG, &tag, -1);

        if (!tag) {
            LOG(LOG_ERROR,"spells.c:on_spell_invoke_clicked", "Unable to get spell tag\n");
            return;
	}
	snprintf(command, MAX_BUF-1, "invoke %d %s", tag, options);
	send_command(command, -1, 1);
    }
}


void
on_spell_close_clicked                 (GtkButton       *button,
                                        gpointer         user_data)
{
    gtk_widget_hide(spell_window);

}

