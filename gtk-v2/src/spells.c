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
 * @file gtk-v2/src/spells.c
 * Handles spell related functionality.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <assert.h>
#include <gtk/gtk.h>

#include "client.h"
#include "image.h"
#include "metaserver.h"
#include "main.h"
#include "gtk2proto.h"

enum Styles {
    Style_Attuned, Style_Repelled, Style_Denied, Style_Normal, Style_Last
};

static GtkListStore     *spell_store;
static GtkTreeSelection *spell_selection;
static GtkWidget        *spell_window, *spell_invoke,
       *spell_cast, *spell_options, *spell_treeview,
       *spell_label[Style_Last], *spell_eventbox[Style_Last];

enum {
    LIST_IMAGE,  LIST_NAME,  LIST_LEVEL,      LIST_TIME,        LIST_COST,
    LIST_DAMAGE, LIST_SKILL, LIST_PATH,       LIST_DESCRIPTION, LIST_BACKGROUND,
    LIST_MAX_SP, LIST_TAG,   LIST_FOREGROUND, LIST_FONT
};

static const char *Style_Names[Style_Last] = {
    "spell_attuned", "spell_repelled", "spell_denied", "spell_normal"
}; /**< The names of theme file styles that are used in the spell dialog. */

static gpointer description_renderer = NULL; /**< The cell renderer for the
                                              *   spell dialog descriptions.
                                              */
static GtkStyle *spell_styles[Style_Last];   /**< The actual styles loaded, or
                                              *   NULL if no styles were found.
                                              */
static int has_init = 0;                     /**< Whether or not the spell
                                              *   dialog initialized since
                                              *   the client started up.
                                              */
/**
 * Gets the style information for the inventory windows.  This is a separate
 * function because if the user changes styles, it can be nice to re-load the
 * configuration.  The style for the inventory/look is a bit special.  That is
 * because with gtk, styles are widget wide - all rows in the widget would use
 * the same style.  We want to adjust the styles based on other attributes.
 */
void spell_get_styles(void)
{
    int i;
    GtkStyle *tmp_style;
    static int style_has_init=0;

    for (i=0; i < Style_Last; i++) {
        if (style_has_init && spell_styles[i]) {
            g_object_unref(spell_styles[i]);
        }
        tmp_style =
            gtk_rc_get_style_by_paths(
                gtk_settings_get_default(), NULL, Style_Names[i], G_TYPE_NONE);
        if (tmp_style) {
            spell_styles[i] = g_object_ref(tmp_style);
        } else {
            LOG(LOG_INFO, "spells.c::spell_get_styles",
                "Unable to find style for %s", Style_Names[i]);
            spell_styles[i] = NULL;
        }
    }
    style_has_init = 1;
}

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
static gboolean spell_selection_func(GtkTreeSelection *selection,
                                     GtkTreeModel     *model,
                                     GtkTreePath      *path,
                                     gboolean          path_currently_selected,
                                     gpointer          userdata)
{
    gtk_widget_set_sensitive(spell_invoke, TRUE);
    gtk_widget_set_sensitive(spell_cast, TRUE);
    return TRUE;
}

/**
 * Adjust the line wrap width used by the spells dialog Description column
 * text renderer and force redraw of the rows to cause row height adjustment.
 * To compute the new wrap width, the widths of all other columns are
 * subtracted from the width of the spells window to determine the available
 * width for the description column.  The remaining space is then configured
 * as the new wrap width.  Once the new wrap is computed, mark all the rows
 * changed so that the renderer adjusts the row height to expand or contract
 * to fit the reformatted description.
 *
 * @param widget
 * @param user_data
 */
void on_spell_window_size_allocate(GtkWidget *widget, gpointer user_data)
{
    guint i;
    guint width;
    gboolean valid;
    GtkTreeIter iter;
    guint column_count;
    GList *column_list;
    GtkTreeViewColumn *column;

    /* If the spell window has not been set up yet, do nothing. */
    if (!has_init) {
        return;
    }
    /*
     * How wide is the spell window?
     */
    width = spell_treeview->allocation.width;
    /*
     * How many columns are in the spell window tree view?
     */
    column_list = gtk_tree_view_get_columns(GTK_TREE_VIEW(spell_treeview));
    column_count = g_list_length(column_list);
    /*
     * Subtract the width of all but the last (Description) column from the
     * total window width to figure out how much may be used for the final
     * description column.
     */
    for (i = 0; i < column_count - 1; i += 1) {
        column = g_list_nth_data(column_list, i);
        width -= gtk_tree_view_column_get_width(column);
    }
    /*
     * The column list allocated by gtk_tree_view_get_columns must be freed
     * when it is no longer needed.
     */
    g_list_free(column_list);
    /*
     * Update the global variable used to configure the wrap-width for the
     * spell dialog description column, then apply it to the cell renderer.
     */
    g_object_set(G_OBJECT(description_renderer), "wrap-width", width, NULL);
    /*
     * Traverse the spell store, and mark each row as changed.  Get the first
     * row, mark it, and then process the rest of the rows (if there are any).
     * This re-flows the spell descriptions to the new wrap-width, and adjusts
     * the height of each row as needed to optimize the vertical space used.
     */
    valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(spell_store), &iter);
    while (valid) {
        GtkTreePath *tree_path;

        tree_path =
            gtk_tree_model_get_path(GTK_TREE_MODEL(spell_store), &iter);
        gtk_tree_model_row_changed(
            GTK_TREE_MODEL(spell_store), tree_path, &iter);
        gtk_tree_path_free(tree_path);
        valid =
            gtk_tree_model_iter_next(GTK_TREE_MODEL(spell_store), &iter);
    }
}

/**
 * When spell information updates, the treeview is cleared and re-populated.
 * The clear/re-populate is easier than "editing" the contents.
 */
void update_spell_information(void)
{
    int i;
    Spell *spell;
    GtkTreeIter iter;
    char buf[MAX_BUF];
    GtkStyle *row_style;
    GdkColor *foreground=NULL;
    GdkColor *background=NULL;
    PangoFontDescription *font=NULL;

    /* If the window/spellstore hasn't been created, return. */
    if (!has_init) {
        return;
    }

    cpl.spells_updated = 0;

    /* We could try to do this in spell_get_styles, but if the window isn't
     * active, it won't work.  This is called whenever the window is made
     * active, so we know it will work, and the time to set this info here,
     * even though it may not change often, is pretty trivial.
     */
    for (i=0; i < Style_Last; i++) {
        if (spell_styles[i]) {
            gtk_widget_modify_fg(spell_label[i],
                                 GTK_STATE_NORMAL, &spell_styles[i]->text[GTK_STATE_NORMAL]);
            gtk_widget_modify_font(spell_label[i], spell_styles[i]->font_desc);
            gtk_widget_modify_bg(spell_eventbox[i],
                                 GTK_STATE_NORMAL, &spell_styles[i]->base[GTK_STATE_NORMAL]);
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
        if (spell->sp) {
            snprintf(buf, sizeof(buf), "%d Mana ", spell->sp);
        }
        if (spell->grace)
            snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
                     "%d Grace", spell->grace);

        if (spell->path & cpl.stats.denied) {
            row_style = spell_styles[Style_Denied];
        } else if (spell->path & cpl.stats.repelled) {
            row_style = spell_styles[Style_Repelled];
        } else if (spell->path & cpl.stats.attuned) {
            row_style = spell_styles[Style_Attuned];
        } else {
            row_style = spell_styles[Style_Normal];
        }

        if (row_style) {
            foreground = &row_style->text[GTK_STATE_NORMAL];
            background = &row_style->base[GTK_STATE_NORMAL];
            font = row_style->font_desc;
        } else {
            foreground=NULL;
            background=NULL;
            font=NULL;
        }

        gtk_list_store_set(
            spell_store, &iter,
            LIST_NAME, spell->name,
            LIST_LEVEL, spell->level,
            LIST_COST, buf,
            LIST_DAMAGE, spell->dam,
            LIST_SKILL, spell->skill,
            LIST_DESCRIPTION, spell->message,
            LIST_BACKGROUND, background,
            LIST_FOREGROUND, foreground,
            LIST_FONT, font,
            LIST_MAX_SP, (spell->sp > spell->grace) ? spell->sp : spell->grace,
            LIST_TAG, spell->tag,
            -1
        );
    }
}

/**
 *
 * @param menuitem
 * @param user_data
 */
void on_spells_activate(GtkMenuItem *menuitem, gpointer user_data) {
    GtkWidget *widget;

    if (!has_init) {
        GtkCellRenderer *renderer;
        GtkTreeViewColumn *column;

        spell_window = GTK_WIDGET(gtk_builder_get_object(dialog_xml, "spell_window"));

        spell_invoke = GTK_WIDGET(gtk_builder_get_object(dialog_xml, "spell_invoke"));
        spell_cast = GTK_WIDGET(gtk_builder_get_object(dialog_xml, "spell_cast"));
        spell_options = GTK_WIDGET(gtk_builder_get_object(dialog_xml, "spell_options"));
        spell_treeview = GTK_WIDGET(gtk_builder_get_object(dialog_xml, "spell_treeview"));

        g_signal_connect((gpointer) spell_window, "size-allocate",
                         G_CALLBACK(on_spell_window_size_allocate), NULL);
        g_signal_connect((gpointer) spell_window, "delete-event",
                         G_CALLBACK(gtk_widget_hide_on_delete), NULL);
        g_signal_connect((gpointer) spell_treeview, "row_activated",
                         G_CALLBACK(on_spell_treeview_row_activated), NULL);
        g_signal_connect((gpointer) spell_cast, "clicked",
                         G_CALLBACK(on_spell_cast_clicked), NULL);
        g_signal_connect((gpointer) spell_invoke, "clicked",
                         G_CALLBACK(on_spell_invoke_clicked), NULL);

        widget = GTK_WIDGET(gtk_builder_get_object(dialog_xml, "spell_close"));
        g_signal_connect((gpointer) widget, "clicked",
                         G_CALLBACK(on_spell_close_clicked), NULL);

        spell_store =
            gtk_list_store_new(
                14,
                G_TYPE_OBJECT,  /* Image - not used */
                G_TYPE_STRING,  /* Name */
                G_TYPE_INT,     /* Level */
                G_TYPE_INT,     /* Time */
                G_TYPE_STRING,  /* SP/Grace */
                G_TYPE_INT,     /* Damage */
                G_TYPE_STRING,  /* Skill name */
                G_TYPE_INT,     /* Spell path */
                G_TYPE_STRING,  /* Description */
                GDK_TYPE_COLOR, /* Background color of the entry */
                G_TYPE_INT,
                G_TYPE_INT,
                GDK_TYPE_COLOR,
                PANGO_TYPE_FONT_DESCRIPTION
            );

        gtk_tree_view_set_model(
            GTK_TREE_VIEW(spell_treeview), GTK_TREE_MODEL(spell_store));
        gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(spell_treeview), TRUE);

        /* Note: it is intentional we don't show (render) some fields:
         * image - we don't have images right now it seems.
         * time - not sure if it worth the space.
         * spell path - done by color
         *
         * Note: Cell alignment is set to top right instead of the default,
         * to improve readability when descriptions wrap to multiple lines.
         */
        renderer = gtk_cell_renderer_text_new();
        renderer->xalign = 0;
        renderer->yalign = 0;
        column = gtk_tree_view_column_new_with_attributes(
                     "Spell", renderer, "text", LIST_NAME, NULL);
        gtk_tree_view_append_column(GTK_TREE_VIEW(spell_treeview), column);
        gtk_tree_view_column_set_sort_column_id(column, LIST_NAME);
        gtk_tree_view_column_add_attribute(
            column, renderer, "background-gdk", LIST_BACKGROUND);
        gtk_tree_view_column_add_attribute(
            column, renderer, "foreground-gdk", LIST_FOREGROUND);
        gtk_tree_view_column_add_attribute(
            column, renderer, "font-desc", LIST_FONT);

        renderer = gtk_cell_renderer_text_new();
        renderer->xalign = 0.4;
        renderer->yalign = 0;
        column = gtk_tree_view_column_new_with_attributes(
                     "Level", renderer, "text", LIST_LEVEL, NULL);
        gtk_tree_view_append_column(GTK_TREE_VIEW(spell_treeview), column);
        gtk_tree_view_column_set_sort_column_id(column, LIST_LEVEL);
        gtk_tree_view_column_add_attribute(
            column, renderer, "background-gdk", LIST_BACKGROUND);
        gtk_tree_view_column_add_attribute(
            column, renderer, "foreground-gdk", LIST_FOREGROUND);
        gtk_tree_view_column_add_attribute(
            column, renderer, "font-desc", LIST_FONT);

        renderer = gtk_cell_renderer_text_new();
        renderer->xalign = 0.4;
        renderer->yalign = 0;
        column = gtk_tree_view_column_new_with_attributes(
                     "Cost/Cast", renderer, "text", LIST_COST, NULL);
        gtk_tree_view_append_column(GTK_TREE_VIEW(spell_treeview), column);

        /* Since this is a string column, it would do a string sort.  Instead,
         * we set up a int column and tie this column to sort on that.
         */
        gtk_tree_view_column_set_sort_column_id(column, LIST_MAX_SP);
        gtk_tree_view_column_add_attribute(
            column, renderer, "background-gdk", LIST_BACKGROUND);
        gtk_tree_view_column_add_attribute(
            column, renderer, "foreground-gdk", LIST_FOREGROUND);
        gtk_tree_view_column_add_attribute(
            column, renderer, "font-desc", LIST_FONT);

        renderer = gtk_cell_renderer_text_new();
        renderer->xalign = 0.4;
        renderer->yalign = 0;
        column = gtk_tree_view_column_new_with_attributes(
                     "Damage", renderer, "text", LIST_DAMAGE, NULL);
        gtk_tree_view_append_column(GTK_TREE_VIEW(spell_treeview), column);
        gtk_tree_view_column_set_sort_column_id(column, LIST_DAMAGE);
        gtk_tree_view_column_add_attribute(
            column, renderer, "background-gdk", LIST_BACKGROUND);
        gtk_tree_view_column_add_attribute(
            column, renderer, "foreground-gdk", LIST_FOREGROUND);
        gtk_tree_view_column_add_attribute(
            column, renderer, "font-desc", LIST_FONT);

        column = gtk_tree_view_column_new_with_attributes(
                     "Skill", renderer, "text", LIST_SKILL, NULL);
        gtk_tree_view_append_column(GTK_TREE_VIEW(spell_treeview), column);
        gtk_tree_view_column_set_sort_column_id(column, LIST_SKILL);
        gtk_tree_view_column_add_attribute(
            column, renderer, "background-gdk", LIST_BACKGROUND);
        gtk_tree_view_column_add_attribute(
            column, renderer, "foreground-gdk", LIST_FOREGROUND);
        gtk_tree_view_column_add_attribute(
            column, renderer, "font-desc", LIST_FONT);

        renderer = gtk_cell_renderer_text_new();
        renderer->xalign = 0;
        renderer->yalign = 0;
        column = gtk_tree_view_column_new_with_attributes(
                     "Description", renderer, "text", LIST_DESCRIPTION, NULL);
        gtk_tree_view_append_column(GTK_TREE_VIEW(spell_treeview), column);
        gtk_tree_view_column_add_attribute(
            column, renderer, "background-gdk", LIST_BACKGROUND);
        gtk_tree_view_column_add_attribute(
            column, renderer, "foreground-gdk", LIST_FOREGROUND);
        gtk_tree_view_column_add_attribute(
            column, renderer, "font-desc", LIST_FONT);
        /*
         * Set up the description column so it wraps lengthy descriptions over
         * multiple lines and at word boundaries.  A default wrap-width is
         * applied to constrain the column width to a reasonable value.  The
         * actual value used here is somewhat unimportant since a corrected
         * width is computed and applied later, but, it does approximate the
         * column size that is appropriate for the dialog's default width.
         */
        g_object_set(G_OBJECT(renderer),
                     "wrap-width", 300, "wrap-mode", PANGO_WRAP_WORD, NULL);
        /*
         * Preserve the description text cell renderer pointer to facilitate
         * setting the wrap-width relative to the dialog's size and content.
         */
        description_renderer = renderer;

        spell_selection =
            gtk_tree_view_get_selection(GTK_TREE_VIEW(spell_treeview));
        gtk_tree_selection_set_mode(spell_selection, GTK_SELECTION_BROWSE);
        gtk_tree_selection_set_select_function(
            spell_selection, spell_selection_func, NULL, NULL);

        gtk_tree_sortable_set_sort_column_id(
            GTK_TREE_SORTABLE(spell_store), LIST_NAME, GTK_SORT_ASCENDING);

        /* The style code will set the colors for these */
        spell_label[Style_Attuned] =
            GTK_WIDGET(gtk_builder_get_object(dialog_xml, "spell_label_attuned"));
        spell_label[Style_Repelled] =
            GTK_WIDGET(gtk_builder_get_object(dialog_xml, "spell_label_repelled"));
        spell_label[Style_Denied] =
            GTK_WIDGET(gtk_builder_get_object(dialog_xml, "spell_label_denied"));
        spell_label[Style_Normal] =
            GTK_WIDGET(gtk_builder_get_object(dialog_xml, "spell_label_normal"));

        /* We use eventboxes because the label widget is a transparent widget.
         * We can't set the background in it and have it work.  But we can set
         * the background in the event box, and put the label widget in the
         * eventbox.
         */
        spell_eventbox[Style_Attuned] =
            GTK_WIDGET(gtk_builder_get_object(dialog_xml, "spell_eventbox_attuned"));
        spell_eventbox[Style_Repelled] =
            GTK_WIDGET(gtk_builder_get_object(dialog_xml, "spell_eventbox_repelled"));
        spell_eventbox[Style_Denied] =
            GTK_WIDGET(gtk_builder_get_object(dialog_xml, "spell_eventbox_denied"));
        spell_eventbox[Style_Normal] =
            GTK_WIDGET(gtk_builder_get_object(dialog_xml, "spell_eventbox_normal"));
    }
    gtk_widget_set_sensitive(spell_invoke, FALSE);
    gtk_widget_set_sensitive(spell_cast, FALSE);
    gtk_widget_show(spell_window);
    spell_get_styles();

    has_init = 1;

    /* Must be called after has_init is set to 1 */
    update_spell_information();
}

/**
 *
 * @param treeview
 * @param path
 * @param column
 * @param user_data
 */
void on_spell_treeview_row_activated(GtkTreeView       *treeview,
                                     GtkTreePath       *path,
                                     GtkTreeViewColumn *column,
                                     gpointer           user_data)
{
    int tag;
    char command[MAX_BUF];
    const char *options=NULL;
    GtkTreeIter iter;
    GtkTreeModel *model;

    model = gtk_tree_view_get_model(treeview);
    if (gtk_tree_model_get_iter(model, &iter, path)) {
        gtk_tree_model_get(model, &iter, LIST_TAG, &tag, -1);

        if (!tag) {
            LOG(LOG_ERROR, "spells.c::on_spell_cast_clicked",
                "Unable to get spell tag\n");
            return;
        }

        assert(options != NULL);
        snprintf(command, MAX_BUF-1, "cast %d %s", tag, options);
        send_command(command, -1, 1);
    }
}

/**
 *
 * @param button
 * @param user_data
 */
void on_spell_cast_clicked(GtkButton *button, gpointer user_data)
{
    int tag;
    char command[MAX_BUF];
    const char *options = NULL;
    GtkTreeIter iter;
    GtkTreeModel *model;

    options = gtk_entry_get_text(GTK_ENTRY(spell_options));

    if (gtk_tree_selection_get_selected(spell_selection, &model, &iter)) {
        gtk_tree_model_get(model, &iter, LIST_TAG, &tag, -1);

        if (!tag) {
            LOG(LOG_ERROR, "spells.c::on_spell_cast_clicked",
                "Unable to get spell tag\n");
            return;
        }
        snprintf(command, MAX_BUF-1, "cast %d %s", tag, options);
        send_command(command, -1, 1);
    }
}

/**
 *
 * @param button
 * @param user_data
 */
void on_spell_invoke_clicked(GtkButton *button, gpointer user_data)
{
    int tag;
    char command[MAX_BUF];
    const char *options=NULL;
    GtkTreeIter iter;
    GtkTreeModel *model;

    options = gtk_entry_get_text(GTK_ENTRY(spell_options));

    if (gtk_tree_selection_get_selected(spell_selection, &model, &iter)) {
        gtk_tree_model_get(model, &iter, LIST_TAG, &tag, -1);

        if (!tag) {
            LOG(LOG_ERROR, "spells.c::on_spell_invoke_clicked",
                "Unable to get spell tag\n");
            return;
        }
        snprintf(command, MAX_BUF-1, "invoke %d %s", tag, options);
        send_command(command, -1, 1);
    }
}

/**
 *
 * @param button
 * @param user_data
 */
void on_spell_close_clicked(GtkButton *button, gpointer user_data)
{
    gtk_widget_hide(spell_window);
}

