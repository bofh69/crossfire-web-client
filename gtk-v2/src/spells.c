char *rcsid_gtk2_spells_c =
    "$Id$";
/*
    Crossfire client, a client program for the crossfire program.

    Copyright (C) 2006-2007 Mark Wedel & Crossfire Development Team

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
#include <glade/glade.h>

#include "client.h"

#include "image.h"
#include "gtk2proto.h"
#include "metaserver.h"
#include "main.h"

enum Styles {
   Style_Attuned, Style_Repelled, Style_Denied, Style_Normal, Style_Last
};

static GtkWidget *spell_window, *spell_treeview, *spell_invoke, *spell_cast,
    *spell_options, *spell_label[Style_Last], *spell_eventbox[Style_Last];
static GtkListStore    *spell_store;
static GtkTreeSelection  *spell_selection;

enum {LIST_IMAGE, LIST_NAME, LIST_LEVEL, LIST_TIME, LIST_COST,
    LIST_DAMAGE, LIST_SKILL, LIST_PATH, LIST_DESCRIPTION, LIST_BACKGROUND, LIST_MAX_SP, LIST_TAG,
    LIST_FOREGROUND, LIST_FONT};


/* The name of these styles in the rc file */
static const char *Style_Names[Style_Last] = {
    "spell_attuned", "spell_repelled", "spell_denied", "spell_normal"
};

/* Actual styles as loaded.  May be null if no style found. */
static GtkStyle    *spell_styles[Style_Last];

static int has_init=0;

/* This gets the style information for the inventory windows.  This is a separate
 * function because if the user changes styles, it can be nice to re-load the configuration.
 * The style for the inventory/look is a bit special.  That is because with gtk, styles
 * are widget wide - all rows in the widget would use the same style.  We want to adjust
 * the styles based on other attributes.
 */
void spell_get_styles()
{
    int i;
    GtkStyle    *tmp_style;
    static int style_has_init=0;

    for (i=0; i < Style_Last; i++) {
        if (style_has_init && spell_styles[i]) g_object_unref(spell_styles[i]);
        tmp_style = gtk_rc_get_style_by_paths(gtk_settings_get_default(), NULL,
					      Style_Names[i], G_TYPE_NONE);
        if (tmp_style) {
            spell_styles[i] = g_object_ref(tmp_style);
	}
        else {
            LOG(LOG_INFO, "spells.c::spell_get_styles", "Unable to find style for %s",
                Style_Names[i]);
            spell_styles[i] = NULL;
	}
    }
    style_has_init=1;
}

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
    GtkStyle    *row_style;
    PangoFontDescription    *font=NULL;
    GdkColor    *foreground=NULL, *background=NULL;
    int i;

    /* If the window/spellstore hasn't been created, return. */
    if (!has_init) return;
    cpl.spells_updated=0;

    /* We could try to do this in spell_get_styles, but if the window
     * isn't active, it won't work.  This is called whenever the window
     * is made active, so we know it will work, and the time
     * to set this info here, even though it may not change often,
     * is pretty trivial.
     */
    for (i=0; i < Style_Last; i++) {
	if (spell_styles[i]) {
	    gtk_widget_modify_fg(spell_label[i],GTK_STATE_NORMAL,
				 &spell_styles[i]->text[GTK_STATE_NORMAL]);
	    gtk_widget_modify_font(spell_label[i], spell_styles[i]->font_desc);
	    gtk_widget_modify_bg(spell_eventbox[i],GTK_STATE_NORMAL,
				 &spell_styles[i]->base[GTK_STATE_NORMAL]);
	} else {
	    gtk_widget_modify_fg(spell_label[i],GTK_STATE_NORMAL, NULL);
	    gtk_widget_modify_font(spell_label[i], NULL);
	    gtk_widget_modify_bg(spell_eventbox[i],GTK_STATE_NORMAL, NULL);
	}
    }

    gtk_list_store_clear(spell_store);
    for (spell = cpl.spelldata; spell; spell=spell->next) {
	gtk_list_store_append(spell_store, &iter);

	buf[0] = 0;
	if (spell->sp) sprintf(buf,"%d Mana ", spell->sp);
	if (spell->grace) sprintf(buf + strlen(buf), "%d Grace", spell->grace);

	if (spell->path & cpl.stats.denied) { row_style = spell_styles[Style_Denied]; }
	else if (spell->path & cpl.stats.repelled) { row_style = spell_styles[Style_Repelled]; }
	else if (spell->path & cpl.stats.attuned) { row_style = spell_styles[Style_Attuned]; }
	else row_style = spell_styles[Style_Normal];

	if (row_style) {
	    foreground = &row_style->text[GTK_STATE_NORMAL];
	    background = &row_style->base[GTK_STATE_NORMAL];
	    font = row_style->font_desc;
	} else {
	    foreground=NULL;
	    background=NULL;
	    font=NULL;
	}

	gtk_list_store_set(spell_store, &iter,
			   LIST_NAME, spell->name,
			   LIST_LEVEL, spell->level,
			   LIST_COST, buf,
			   LIST_DAMAGE, spell->dam,
			   LIST_SKILL, spell->skill,
			   LIST_DESCRIPTION, spell->message,
			   LIST_BACKGROUND, background,
			   LIST_FOREGROUND, foreground,
			   LIST_FONT, font,
			   LIST_MAX_SP, (spell->sp > spell->grace)?spell->sp:spell->grace,
			   LIST_TAG, spell->tag,
			   -1);
    }
}

void
on_spells_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    GladeXML *xml_tree;
    GtkWidget *widget;

    if (!has_init) {
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

        spell_window = glade_xml_get_widget(dialog_xml, "spell_window");
        xml_tree = glade_get_widget_tree(GTK_WIDGET(spell_window));

        spell_invoke = glade_xml_get_widget(xml_tree,"spell_invoke");
        spell_cast = glade_xml_get_widget(xml_tree,"spell_cast");
        spell_options = glade_xml_get_widget(xml_tree,"spell_options");
        spell_treeview = glade_xml_get_widget(xml_tree, "spell_treeview");

        g_signal_connect ((gpointer) spell_treeview, "row_activated",
            G_CALLBACK (on_spell_treeview_row_activated), NULL);
        g_signal_connect ((gpointer) spell_cast, "clicked",
            G_CALLBACK (on_spell_cast_clicked), NULL);
        g_signal_connect ((gpointer) spell_invoke, "clicked",
            G_CALLBACK (on_spell_invoke_clicked), NULL);

        widget = glade_xml_get_widget(xml_tree, "spell_close");
        g_signal_connect ((gpointer) widget, "clicked",
            G_CALLBACK (on_spell_close_clicked), NULL);

	spell_store = gtk_list_store_new(14,
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
				G_TYPE_INT,
				GDK_TYPE_COLOR,
				PANGO_TYPE_FONT_DESCRIPTION
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
	gtk_tree_view_column_add_attribute(column, renderer, "foreground-gdk", LIST_FOREGROUND);
	gtk_tree_view_column_add_attribute(column, renderer, "font-desc", LIST_FONT);

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes ("Level", renderer,
						"text", LIST_LEVEL,
						NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (spell_treeview), column);
	gtk_tree_view_column_set_sort_column_id(column, LIST_LEVEL);
	gtk_tree_view_column_add_attribute(column, renderer, "background-gdk", LIST_BACKGROUND);
	gtk_tree_view_column_add_attribute(column, renderer, "foreground-gdk", LIST_FOREGROUND);
	gtk_tree_view_column_add_attribute(column, renderer, "font-desc", LIST_FONT);

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
	gtk_tree_view_column_add_attribute(column, renderer, "foreground-gdk", LIST_FOREGROUND);
	gtk_tree_view_column_add_attribute(column, renderer, "font-desc", LIST_FONT);

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes ("Damage", renderer,
						"text", LIST_DAMAGE,
						NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (spell_treeview), column);
	gtk_tree_view_column_set_sort_column_id(column, LIST_DAMAGE);
	gtk_tree_view_column_add_attribute(column, renderer, "background-gdk", LIST_BACKGROUND);
	gtk_tree_view_column_add_attribute(column, renderer, "foreground-gdk", LIST_FOREGROUND);
	gtk_tree_view_column_add_attribute(column, renderer, "font-desc", LIST_FONT);

	column = gtk_tree_view_column_new_with_attributes ("Skill", renderer,
						"text", LIST_SKILL,
						NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (spell_treeview), column);
	gtk_tree_view_column_set_sort_column_id(column, LIST_SKILL);
	gtk_tree_view_column_add_attribute(column, renderer, "background-gdk", LIST_BACKGROUND);
	gtk_tree_view_column_add_attribute(column, renderer, "foreground-gdk", LIST_FOREGROUND);
	gtk_tree_view_column_add_attribute(column, renderer, "font-desc", LIST_FONT);

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes ("Description", renderer,
						"text", LIST_DESCRIPTION,
						NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (spell_treeview), column);
	gtk_tree_view_column_add_attribute(column, renderer, "background-gdk", LIST_BACKGROUND);
	gtk_tree_view_column_add_attribute(column, renderer, "foreground-gdk", LIST_FOREGROUND);
	gtk_tree_view_column_add_attribute(column, renderer, "font-desc", LIST_FONT);

	spell_selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(spell_treeview));
	gtk_tree_selection_set_mode (spell_selection, GTK_SELECTION_BROWSE);
	gtk_tree_selection_set_select_function(spell_selection, spell_selection_func,
					       NULL, NULL);


	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(spell_store),
					     LIST_NAME,
					     GTK_SORT_ASCENDING);

	/* the style code will set the colors for these */
        spell_label[Style_Attuned] =
            glade_xml_get_widget(xml_tree, "spell_label_attuned");
        spell_label[Style_Repelled] =
            glade_xml_get_widget(xml_tree, "spell_label_repelled");
        spell_label[Style_Denied] =
            glade_xml_get_widget(xml_tree, "spell_label_denied");
        spell_label[Style_Normal] =
            glade_xml_get_widget(xml_tree, "spell_label_normal");

	/* We use eventboxes because the label widget is a transparent widget -
	 * we can't set the background in it and have it work.
	 * But we can set the background in the event box, and put the label
	 * widget in the eventbox.
	 */
        spell_eventbox[Style_Attuned] =
            glade_xml_get_widget(xml_tree, "spell_eventbox_attuned");
        spell_eventbox[Style_Repelled] =
            glade_xml_get_widget(xml_tree, "spell_eventbox_repelled");
        spell_eventbox[Style_Denied] =
            glade_xml_get_widget(xml_tree, "spell_eventbox_denied");
        spell_eventbox[Style_Normal] =
            glade_xml_get_widget(xml_tree, "spell_eventbox_normal");
    }
    gtk_widget_set_sensitive(spell_invoke, FALSE);
    gtk_widget_set_sensitive(spell_cast, FALSE);
    gtk_widget_show(spell_window);
    spell_get_styles();

    has_init=1;
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
