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
 * @file gtk-v2/src/create_char.c
 * Handles creation of new characters
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>
#include <glade/glade.h>
#include <ctype.h>

#include "client.h"

#include "image.h"

#include "main.h"
#include "gtk2proto.h"
#include "metaserver.h"

/* This corresponds to the number of opt_.. fields in the
 * create_character_window.  In theory, the vbox could be resized
 * (or add a sub box), and adjust accordingly,
 * but it is unlikely that number of optional choices is going
 * to rapidly grow, and making it static makes some things much
 * easier.
 * Instead of having two different sets of fields named different,
 * we just have one set and split it in half, for race and
 * class.  This is done so that boxes don't need to be moved
 * around - imagine cas where one race has options and
 * another doesn't, but the currently selected class does -
 * as player chooses different races, that race choice will
 * come and go, but we want the class box to remain and to keep
 * its same value.
 */
#define NUM_OPT_FIELDS  6
#define RACE_OPT_START  0
#define CLASS_OPT_START NUM_OPT_FIELDS/2
#define RACE_OPT_END    CLASS_OPT_START - 1
#define CLASS_OPT_ENG   NUM_OPT_FIELDS - 1


/* These are in the create_character_window */
static GtkWidget *spinbutton_cc[NUM_NEW_CHAR_STATS], *label_rs[NUM_NEW_CHAR_STATS],
       *label_cs[NUM_NEW_CHAR_STATS], *label_tot[NUM_NEW_CHAR_STATS],
       *label_cc_unspent, *textview_rs_desc, *label_cc_desc, *label_cc_status_update,
       *button_cc_cancel, *button_cc_done, *create_character_window, *combobox_rs,
       *combobox_cs, *textview_cs_desc, *entry_new_character_name, *button_choose_starting_map,
       *opt_label[NUM_OPT_FIELDS], *opt_combobox[NUM_OPT_FIELDS];

static GtkTextMark *text_mark_cs, *text_mark_rs;

/* These are in the choose starting map window */
static GtkWidget *choose_starting_map_window,
       *button_csm_done, *button_csm_cancel, *combobox_starting_map;

GtkTextBuffer *textbuf_starting_map;

static int has_init=0, negative_stat=0;

#define STARTING_MAP_PANE   0
Info_Pane create_char_pane[1];

#define WINDOW_NONE             0
#define WINDOW_CREATE_CHARACTER 1
#define WINDOW_CHOOSE_MAP       2

/**
 * This is a little helper window which shows the window
 * specified and hides the other window(s).  This is just
 * a bit cleaner than having a bunch if gtk_widget_show()/
 * gtk_wdiget_hide() calls.
 * Also, if more than 2 windows are ever used, this will also
 * make it easier to make sure the correct window is displayed.
 *
 * @param window
 * WINDOW_... define as listed at top of this file which defines
 * the winow.  Special is WINDOW_NONE, which will just result
 * in this hiding all windows.
 */
static void show_window(int window)
{
    switch (window) {

    case WINDOW_NONE:
        gtk_widget_hide(create_character_window);
        gtk_widget_hide(choose_starting_map_window);
        break;

    case WINDOW_CREATE_CHARACTER:
        gtk_widget_show(create_character_window);
        gtk_widget_hide(choose_starting_map_window);
        break;

    case WINDOW_CHOOSE_MAP:
        gtk_widget_hide(create_character_window);
        gtk_widget_show(choose_starting_map_window);
        break;
    }
}


/**
 * This function makes the widgets in the window sensitive (or not).
 * This is used because having the player fiddle with the attributes
 * before we get the information from the server doesn't make sense.
 *
 * @param sensitive
 * passed to gtk_widget_set_sensitive, to either make the widget
 * sensitive or not
 */
static void create_character_set_sensitive(int sensitive)
{
    int i;

    gtk_widget_set_sensitive(button_cc_done, sensitive);
    gtk_widget_set_sensitive(button_choose_starting_map, sensitive);
    gtk_widget_set_sensitive(entry_new_character_name, sensitive);
    gtk_widget_set_sensitive(combobox_rs, sensitive);
    gtk_widget_set_sensitive(combobox_cs, sensitive);
    /* Note we do not change status of cancel button - let
     * the player cancel out of the window if they want -
     * no harm in doing so.
     */

    for (i=0; i<NUM_NEW_CHAR_STATS; i++) {
        gtk_widget_set_sensitive(spinbutton_cc[i], sensitive);
    }

    /* If we do not have any starting maps, no reason to show
     * that button to the player.
     */
    if (starting_map_number) {
        gtk_widget_show(button_choose_starting_map);
    } else {
        gtk_widget_hide(button_choose_starting_map);
    }
}

/**
 * This function is here so that other files, in particular
 * account.c:on_button_create_character_clicked() can pop
 * up this window.  This function also requests necessary
 * data from server if we don't already have it.
 */
void create_character_window_show()
{
    int reset_needed = 0;

    /* If we don't have race/class/stat_point values, get them now.
     * those values are reset if we switch between servers, so there
     * should never be any danger of them being wrong.
     * In theory, if one of these is true, all of them should be true
     * because it shouldn't be possible to get in a case where we have
     * gotten race info but not class.
     */
    if (!races) {
        cs_print_string(csocket.fd, "requestinfo race_list");
        reset_needed = 1;
    }
    if (!classes) {
        cs_print_string(csocket.fd, "requestinfo class_list");
        reset_needed = 1;
    }
    if (!stat_points) {
        cs_print_string(csocket.fd, "requestinfo newcharinfo");
        reset_needed = 1;
    }
    /* In this case, we are getting copies of some of the data
     * from the server - we need to discard any data we currently
     * have then (mainly, clear out things like the pulldown list
     * for classes
     */
    if (reset_needed) {
        gtk_label_set_text(GTK_LABEL(label_cc_status_update),
                           "Getting race & class information from the server");
        create_character_set_sensitive(FALSE);
    }

    /* This will be set true once we get all the data */
    gtk_widget_show(create_character_window);
}

/**
 * Basically opposite as above - hide the create character windows -
 * this is called from hide_all_login_windows(), which is called
 * when the client gets an addme success command.
 */
void create_character_window_hide()
{
    show_window(WINDOW_NONE);
}

/**
 * Whenever something in the window changes, this is called to update
 * everything - in the case of most any value, we need to recalculate
 * everything - trying to do a delta from the old to new is probably
 * more effort than it is worth and is more prone to errors.
 */
static void update_all_stats()
{
    int i, stat_points_used=0, statval, tmp;
    const gchar *tval;
    char buf[MAX_BUF];

    negative_stat = 0;
    for (i=0; i<NUM_NEW_CHAR_STATS; i++) {

        tmp = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spinbutton_cc[i]));
        stat_points_used += tmp;
        statval = tmp;

        /* We presume the label value is correct here - it should
         * be - this is easier than tracking down the corresponding
         * race/class structure and getting the value there.
         */
        tval = gtk_label_get_text(GTK_LABEL(label_cs[i]));
        statval += atoi(tval);

        tval = gtk_label_get_text(GTK_LABEL(label_rs[i]));
        statval += atoi(tval);

        /* Might it be good to draw nonpositive stats in red?  Rather
         * than hardcode that, it should be a style
         * Used to only check for negative stats, but zero is not allowed either.
         */
        if (statval < 1) {
            negative_stat = 1;
        }

        sprintf(buf, "%d", statval);
        gtk_label_set_text(GTK_LABEL(label_tot[i]), buf);
    }

    tmp = stat_points - stat_points_used;
    sprintf(buf,"%d", tmp);
    gtk_label_set_text(GTK_LABEL(label_cc_unspent), buf);

    /* Display some warning messages - we could try and display all the
     * different warnings at once, but one at a time should be good enough.
     * perhaps the done button should be inactivated if there are errors.
     */
    if (tmp < 0) {
        gtk_label_set_text(GTK_LABEL(label_cc_status_update),
                           "You have used more than your allotted total attribute points");
    } else if (negative_stat) {
        gtk_label_set_text(GTK_LABEL(label_cc_status_update),
                           "Attributes less than 1 are not allowed - adjust your selections before finishing");

    } else {
        gtk_label_set_text(GTK_LABEL(label_cc_status_update), "Waiting for player selections");
    }


}

static void send_create_player_to_server()
{
    const gchar *char_name;
    int i, on_choice, tmp;
    SockList sl;
    char buf[MAX_BUF];
    uint8 sockbuf[MAX_BUF];

    char_name = gtk_entry_get_text(GTK_ENTRY(entry_new_character_name));

    SockList_Init(&sl, sockbuf);
    SockList_AddString(&sl, "createplayer ");
    SockList_AddChar(&sl, strlen(char_name));
    SockList_AddString(&sl, char_name);
    SockList_AddChar(&sl, strlen(account_password));

    SockList_AddString(&sl, account_password);

    /* The client should never be popping up the new client creation
     * window unless the server supports loginmethod >= 2, so
     * we do not have any check here for that, but these
     * attributes are only valid for loginmethod >= 2
     */
    i = gtk_combo_box_get_active(GTK_COMBO_BOX(combobox_rs));
    snprintf(buf, MAX_BUF, "race %s", races[i].arch_name);
    SockList_AddChar(&sl, strlen(buf)+1);
    SockList_AddString(&sl, buf);
    SockList_AddChar(&sl, 0);

    /* From a practical standpoint, the server should never send
     * race/class choices unless it also supports the receipt of
     * those.  So no special checks are needed here.
     */
    for (on_choice = 0; on_choice < races[i].num_rc_choice; on_choice++) {
        int j;

        j = gtk_combo_box_get_active(GTK_COMBO_BOX(opt_combobox[on_choice + RACE_OPT_START]));

        snprintf(buf, MAX_BUF, "choice %s %s", races[i].rc_choice[on_choice].choice_name,
                 races[i].rc_choice[on_choice].value_arch[j]);

        SockList_AddChar(&sl, strlen(buf)+1);
        SockList_AddString(&sl, buf);
        SockList_AddChar(&sl, 0);
    }


    i = gtk_combo_box_get_active(GTK_COMBO_BOX(combobox_cs));
    snprintf(buf, MAX_BUF, "class %s", classes[i].arch_name);
    SockList_AddChar(&sl, strlen(buf)+1);
    SockList_AddString(&sl, buf);
    SockList_AddChar(&sl, 0);

    for (on_choice = 0; on_choice < classes[i].num_rc_choice; on_choice++) {
        int j;

        j = gtk_combo_box_get_active(GTK_COMBO_BOX(opt_combobox[on_choice + CLASS_OPT_START]));

        snprintf(buf, MAX_BUF, "choice %s %s", classes[i].rc_choice[on_choice].choice_name,
                 classes[i].rc_choice[on_choice].value_arch[j]);

        SockList_AddChar(&sl, strlen(buf)+1);
        SockList_AddString(&sl, buf);
        SockList_AddChar(&sl, 0);
    }

    /* Its possible that the server does not provide a choice of
     * starting maps - if that is the case, then we will never
     * display the starting map window.  So check for that here.
     */
    if (starting_map_number) {
        i = gtk_combo_box_get_active(GTK_COMBO_BOX(combobox_starting_map));
        if (i != -1) {
            snprintf(buf, MAX_BUF, "starting_map %s", starting_map_info[i].arch_name);
            SockList_AddChar(&sl, strlen(buf)+1);
            SockList_AddString(&sl, buf);
            SockList_AddChar(&sl, 0);
        }
    }

    for (i=0; i<NUM_NEW_CHAR_STATS; i++) {
        tmp = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spinbutton_cc[i]));
        snprintf(buf, MAX_BUF, "%s %d", stat_mapping[i].widget_suffix, tmp);
        SockList_AddChar(&sl, strlen(buf)+1);
        SockList_AddString(&sl, buf);
        SockList_AddChar(&sl, 0);
    }

    SockList_Send(&sl, csocket.fd);

    if (cpl.name) {
        free(cpl.name);
    }
    cpl.name = strdup(char_name);
    keybindings_init();
}


/**
 * User has hit the 'return to character selection' button - basically
 * meaning that we have to hide the current window and show the choose_char_window.
 *
 * @param button    ignored
 * @param user_data ignored
 */
void
on_button_cc_cancel(GtkButton *button, gpointer user_data)
{
    show_window(WINDOW_NONE);
    choose_char_window_show();
}


/**
 * User has hit the choose starting map button.
 * Not much to do, other than make that window visible
 *
 * @param button    ignored
 * @param user_data ignored
 */
void
on_button_choose_starting_map(GtkButton *button, gpointer user_data)
{
    show_window(WINDOW_CHOOSE_MAP);
}


/**
 * This checks the various spin buttons, combox boxes, etc
 * to make sure everything that should be set has been set,
 * and in the case of attributes, that they are within
 * range.  This will show/hide the relevant window
 * and throw up dialog windows if necessary
 *
 * @return
 * TRUE if everything checks out, FALSE if not.
 */
static int character_data_ok()
{
    const gchar *char_name;
    int i, stat_points_used=0, tmp[NUM_NEW_CHAR_STATS], negative_stat=0;

    char_name = gtk_entry_get_text(GTK_ENTRY(entry_new_character_name));

    if (!char_name || char_name[0] == 0) {
        gtk_label_set_text(GTK_LABEL(label_cc_status_update),
                           "You must enter a character name");
        show_window(WINDOW_CREATE_CHARACTER);
        return FALSE;
    }

    /* We get the stat values here - we also total up how many
     * points are used.  If everything checks out, we will
     * need these stat values later.
     */
    for (i=0; i<NUM_NEW_CHAR_STATS; i++) {
        tmp[i] = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spinbutton_cc[i]));
        stat_points_used += tmp[i];
    }

    if (stat_points_used > stat_points) {
        gtk_label_set_text(GTK_LABEL(label_cc_status_update),
                           "You have used more than your allotted total attribute points");
        show_window(WINDOW_CREATE_CHARACTER);
        return FALSE;
    }
    /* negative_stat is a global to this file.  update_all_stats()
     * sets it/clears it - rather than doing that work again, just
     * re-use that value.
     */
    if (negative_stat) {
        gtk_label_set_text(GTK_LABEL(label_cc_status_update),
                           "Attributes less than 1 are not allowed - adjust your selections before finishing");
        show_window(WINDOW_CREATE_CHARACTER);
        return FALSE;
    }

    /* No message is normally displayed for this - the player is
     * always going to get this case when starting out, but if
     * they hit done, we want to warn them that they have points
     * left to spend, since at present time there is no way to spend
     * these points later.
     */
    if (stat_points_used < stat_points) {
        GtkWidget *dialog;
        int result;

        dialog =
            gtk_message_dialog_new(GTK_WINDOW(create_character_window),
                                   GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_QUESTION,
                                   GTK_BUTTONS_YES_NO,
                                   "%s\n%s\n%s",
                                   "You have not spent all your attribute points.",
                                   "You will be unable to spend these later.",
                                   "Create character anyways?");
        result = gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        if (result == GTK_RESPONSE_NO) {
            show_window(WINDOW_CREATE_CHARACTER);
            return FALSE;
        }
        /* Otherwise, fall through below */
    }

    /* Check to see starting map - note that start_map_number could
     * be zero, which means that the server does not have a choice,
     * and thus we don't have to get anything from the player.
     * Is throwing a dialog box up here perhaps overkill?
     */
    i = gtk_combo_box_get_active(GTK_COMBO_BOX(combobox_starting_map));
    if (starting_map_number && i == -1) {
        GtkWidget *dialog;
        int result;

        show_window(WINDOW_CHOOSE_MAP);
        dialog =
            gtk_message_dialog_new(GTK_WINDOW(choose_starting_map_window),
                                   GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_WARNING,
                                   GTK_BUTTONS_OK,
                                   "You must choose a starting map before you can start playing");
        result = gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return FALSE;
    }
    /* Everything checks out OK */
    return TRUE;
}

/**
 * User has hit the done button.  Need to verify input, and if it
 * looks good, send it to the server.
 * Note: This callback is used for the 'Done' button in both
 * the character character window and starting map window.
 *
 * @param button    ignored
 * @param user_data ignored
 */
void
on_button_cc_done(GtkButton *button, gpointer user_data)
{
    if (character_data_ok()) {
        /* If we get here, everything checks out - now we have to
         * send the data to the server.
         */
        gtk_label_set_text(GTK_LABEL(label_cc_status_update),
                           "Sending new character information to server");
        show_window(WINDOW_CREATE_CHARACTER);
        send_create_player_to_server();
    }
}

/**
 * User has changed one of the spinbutton values.  We need to total
 * back up the values.
 */
void
on_spinbutton_cc (GtkSpinButton *spinbutton, gpointer user_data)
{

    update_all_stats();
}

/**
 * User has changed one of the fields in the race/class
 * combobox.  Since the logic for the two is somewhat
 * the same, we use one function to handle the event -
 * as such, we really need to pay attention to what
 * box is set to.
 *
 * @param box
 * The combobox that generated the event.
 * @param user_data
 * ignored
 */
void
on_combobox_rcs_changed(GtkComboBox *box, gpointer user_data)
{
    int active_entry, i, opt_start;
    GtkWidget **label_stat;
    Race_Class_Info *rc;
    char buf[256];

    active_entry = gtk_combo_box_get_active(box);

    /* I don't think this can ever happen - if we get here,
     * something should be active.
     */
    if (active_entry == -1) {
        return;
    }

    /* since we are using a list store, and we are not re-arranging the order,
     * the entry number should match our array number.
     */
    if (box == GTK_COMBO_BOX(combobox_cs)) {
        gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview_cs_desc)),
                                 classes[active_entry].description,
                                 strlen(classes[active_entry].description));
        gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(textview_cs_desc),
                                     text_mark_cs, 0.0, TRUE, 0.0, 0.0);

        rc = &classes[active_entry];
        label_stat = label_cs;
        opt_start = CLASS_OPT_START;

    } else if (box == GTK_COMBO_BOX(combobox_rs)) {
        gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview_rs_desc)),
                                 races[active_entry].description,
                                 strlen(races[active_entry].description));
        gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(textview_rs_desc),
                                     text_mark_rs, 0.0, TRUE, 0.0, 0.0);
        rc = &races[active_entry];
        label_stat = label_rs;
        opt_start = RACE_OPT_START;
    } else {
        LOG(LOG_ERROR, "gtk-v2/src/create_char.c:on_combobox_rcs_changed",
            "Passed in combobox does not match any combobox");
        return;
    }

    for (i=0; i < rc->num_rc_choice; i++) {
        int j;
        GtkTreeModel *store;
        GtkTreeIter iter;

        if (i == (NUM_OPT_FIELDS/2)) {
            LOG(LOG_ERROR, "gtk-v2/src/create_char.c:on_combobox_rcs_changed",
                "Number of racial option exceeds allocated amount (%d > %d)",
                i, NUM_OPT_FIELDS/2);
            break;
        }
        /* Set up the races combobox */
        store = gtk_combo_box_get_model(GTK_COMBO_BOX(opt_combobox[i + opt_start]));
        gtk_list_store_clear(GTK_LIST_STORE(store));

        for (j=0; j<rc->rc_choice[i].num_values; j++) {
            gtk_list_store_append(GTK_LIST_STORE(store), &iter);
            gtk_list_store_set(GTK_LIST_STORE(store), &iter, 0, rc->rc_choice[i].value_desc[j], -1);
        }
        gtk_combo_box_set_active(GTK_COMBO_BOX(opt_combobox[i+opt_start]), 0);

        gtk_label_set(GTK_LABEL(opt_label[i+opt_start]), rc->rc_choice[i].choice_desc);
        gtk_widget_show(opt_label[i+opt_start]);
        gtk_widget_show(opt_combobox[i+opt_start]);
        /* No signals are connected - the value of the combo
         * box will be when we send the data to the server.
         */
    }

    /* Hide any unused fields */
    for ( ; i < (NUM_OPT_FIELDS/2); i++) {
        gtk_widget_hide(opt_label[i + opt_start]);
        gtk_widget_hide(opt_combobox[i + opt_start]);
    }


    /* label_stat now points at the array of stats to update, and rc points
     * at either the race or class to get values from.
     */
    for (i=0; i < NUM_NEW_CHAR_STATS; i++) {
        sprintf(buf, "%+d", rc->stat_adj[stat_mapping[i].rc_offset]);
        gtk_label_set_text(GTK_LABEL(label_stat[i]), buf);
    }
    update_all_stats();
}

/**
 * We have gotten some new information from
 * the server, so we need to update the information -
 * race/class choices or stat points/min stat/max stat
 * information.
 *
 */
void new_char_window_update_info()
{
    char buf[256];
    GtkListStore *store;
    GtkTreeIter iter;
    GtkCellRenderer *renderer;
    int i;

    /* We could do the update as we get the data, but it shouldn't take
     * too long to get all the data, and simpler to just do one update
     */
    if (!stat_points || num_races != used_races || num_classes != used_classes) {
        return;
    }

    gtk_label_set_text(GTK_LABEL(label_cc_status_update), "Waiting for player selections");

    sprintf(buf,"%d", stat_points);
    gtk_label_set_text(GTK_LABEL(label_cc_unspent), buf);

    /* Set up the races combobox */
    store = gtk_list_store_new(1, G_TYPE_STRING);

    for (i=0; i<num_races; i++) {
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter, 0, races[i].public_name, -1);
    }

    gtk_combo_box_set_model(GTK_COMBO_BOX(combobox_rs), GTK_TREE_MODEL(store));
    gtk_cell_layout_clear(GTK_CELL_LAYOUT(combobox_rs));

    renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combobox_rs), renderer, FALSE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(combobox_rs), renderer,
                                   "text", 0, NULL);

    g_signal_connect ((gpointer) combobox_rs,  "changed",
                      G_CALLBACK (on_combobox_rcs_changed), NULL);

    gtk_combo_box_set_active(GTK_COMBO_BOX(combobox_rs), 0);
    /* Set up the classes combobox */
    store = gtk_list_store_new(1, G_TYPE_STRING);

    for (i=0; i<num_classes; i++) {
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter, 0, classes[i].public_name, -1);

    }

    gtk_combo_box_set_model(GTK_COMBO_BOX(combobox_cs), GTK_TREE_MODEL(store));
    gtk_cell_layout_clear(GTK_CELL_LAYOUT(combobox_cs));

    renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combobox_cs), renderer, FALSE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(combobox_cs), renderer,
                                   "text", 0, NULL);
    g_signal_connect ((gpointer) combobox_cs,  "changed",
                      G_CALLBACK (on_combobox_rcs_changed), NULL);
    gtk_combo_box_set_active(GTK_COMBO_BOX(combobox_cs), 0);

    /* Reset to minimum/maximum values for the spinbutton.
     */
    for (i=0; i<NUM_NEW_CHAR_STATS; i++) {
        /* Reset any stat values - just makes more sense, but also
         * possible that starting value set in the glade file may
         * be outside of this range.
         */
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinbutton_cc[i]), stat_min);
        gtk_spin_button_set_range(GTK_SPIN_BUTTON(spinbutton_cc[i]), stat_min,
                                  stat_maximum);
    }

    create_character_set_sensitive(TRUE);
}

/******************************************************************************
 * This section is related to the starting map window.
 *****************************************************************************/

/**
 * User has hit the 'return to character creation' button - basically
 * meaning that we have to hide the current window and show the
 * create_character_window.
 *
 * @param button    ignored
 * @param user_data ignored
 */
void
on_button_csm_cancel(GtkButton *button, gpointer user_data)
{
    show_window(WINDOW_CREATE_CHARACTER);
}


void
on_combobox_starting_map_changed(GtkComboBox *box, gpointer user_data)
{
    int active_entry;

    active_entry = gtk_combo_box_get_active(box);

    /* I don't think this can ever happen - if we get here,
     * something should be active.
     */
    if (active_entry == -1) {
        return;
    }

    /* since we are using a list store, and we are not re-arranging the order,
     * the entry number should match our array number.
     */
    gtk_text_buffer_set_text(textbuf_starting_map, "", 0);
    add_marked_text_to_pane(&create_char_pane[STARTING_MAP_PANE],
                            starting_map_info[active_entry].description, 0, 0, 0);

}
/**
 * We have gotten starting map information from the server - now
 * update the combo boxes.
 */
void starting_map_update_info()
{
    GtkListStore *store;
    GtkTreeIter iter;
    GtkCellRenderer *renderer;
    int i;

    /* Set up the races combobox */
    store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);

    for (i=0; i<=starting_map_number; i++) {
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter, 0, starting_map_info[i].public_name, -1);
        gtk_list_store_set(store, &iter, 1, starting_map_info[i].arch_name, -1);
    }

    gtk_combo_box_set_model(GTK_COMBO_BOX(combobox_starting_map), GTK_TREE_MODEL(store));
    gtk_cell_layout_clear(GTK_CELL_LAYOUT(combobox_starting_map));

    renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combobox_starting_map), renderer, FALSE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(combobox_starting_map), renderer,
                                   "text", 0, NULL);

    g_signal_connect ((gpointer) combobox_starting_map,  "changed",
                      G_CALLBACK (on_combobox_starting_map_changed), NULL);

    gtk_combo_box_set_active(GTK_COMBO_BOX(combobox_starting_map), -1);

    /* If we get called, we presume we have data to show, so activate button */
    gtk_widget_show(button_choose_starting_map);

}


/**
 * Initializes the create character window.
 */
void init_create_character_window()
{
    GladeXML *xml_tree;
    char tmpbuf[80];
    int i;
    GtkTextIter iter;
    GtkCellRenderer *renderer;

    if (has_init) {
        return;
    }
    has_init=1;

    create_character_window = glade_xml_get_widget(dialog_xml, "create_character_window");
    gtk_window_set_transient_for(GTK_WINDOW(create_character_window), GTK_WINDOW(window_root));

    xml_tree = glade_get_widget_tree(GTK_WIDGET(create_character_window));

    button_cc_done = glade_xml_get_widget(dialog_xml,"button_cc_done");
    button_cc_cancel = glade_xml_get_widget(dialog_xml,"button_cc_cancel");
    button_choose_starting_map = glade_xml_get_widget(dialog_xml,"button_choose_starting_map");
    label_cc_status_update = glade_xml_get_widget(dialog_xml,"label_cc_status_update");
    label_cc_desc = glade_xml_get_widget(dialog_xml,"label_cc_desc");
    label_cc_unspent = glade_xml_get_widget(dialog_xml,"label_cc_unspent");
    combobox_rs = glade_xml_get_widget(dialog_xml,"combobox_rs");
    combobox_cs = glade_xml_get_widget(dialog_xml,"combobox_cs");
    entry_new_character_name = glade_xml_get_widget(dialog_xml,"cc_entry_new_character_name");

    textview_rs_desc = glade_xml_get_widget(dialog_xml,"textview_rs_desc");
    text_mark_rs = gtk_text_mark_new("rs_start", TRUE);
    gtk_text_buffer_get_start_iter(gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview_rs_desc)),
                                   &iter);
    gtk_text_buffer_add_mark(gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview_rs_desc)),
                             text_mark_rs, &iter);

    textview_cs_desc = glade_xml_get_widget(dialog_xml,"textview_cs_desc");
    text_mark_cs = gtk_text_mark_new("cs_start", TRUE);
    gtk_text_buffer_get_start_iter(gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview_cs_desc)),
                                   &iter);
    gtk_text_buffer_add_mark(gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview_cs_desc)),
                             text_mark_cs, &iter);

    for (i=0; i<NUM_NEW_CHAR_STATS; i++) {
        snprintf(tmpbuf, 80, "spinbutton_cc_%s", stat_mapping[i].widget_suffix);
        spinbutton_cc[i] = glade_xml_get_widget(dialog_xml, tmpbuf);

        g_signal_connect ((gpointer) spinbutton_cc[i], "value-changed",
                          G_CALLBACK (on_spinbutton_cc), (void*)i);

        snprintf(tmpbuf, 80, "label_rs_%s", stat_mapping[i].widget_suffix);
        label_rs[i] = glade_xml_get_widget(dialog_xml, tmpbuf);

        snprintf(tmpbuf, 80, "label_cs_%s", stat_mapping[i].widget_suffix);
        label_cs[i] = glade_xml_get_widget(dialog_xml, tmpbuf);

        snprintf(tmpbuf, 80, "label_tot_%s", stat_mapping[i].widget_suffix);
        label_tot[i] = glade_xml_get_widget(dialog_xml, tmpbuf);
    }

    /* Note that in the glade file, the numbering starts at 1 */
    for (i=0; i < NUM_OPT_FIELDS; i++ ) {
        GtkListStore *store;

        snprintf(tmpbuf, 80, "opt_label%d", i+1);
        opt_label[i] = glade_xml_get_widget(dialog_xml, tmpbuf);

        snprintf(tmpbuf, 80, "opt_combobox%d", i+1);
        opt_combobox[i] = glade_xml_get_widget(dialog_xml, tmpbuf);

        gtk_cell_layout_clear(GTK_CELL_LAYOUT(opt_combobox[i]));
        renderer = gtk_cell_renderer_text_new();
        gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(opt_combobox[i]), renderer, FALSE);
        gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(opt_combobox[i]), renderer,
                                       "text", 0, NULL);
        store = gtk_list_store_new(1, G_TYPE_STRING);
        gtk_combo_box_set_model(GTK_COMBO_BOX(opt_combobox[i]), GTK_TREE_MODEL(store));

    }

    g_signal_connect ((gpointer) button_cc_done, "clicked",
                      G_CALLBACK (on_button_cc_done), NULL);
    g_signal_connect ((gpointer) button_cc_cancel, "clicked",
                      G_CALLBACK (on_button_cc_cancel), NULL);
    g_signal_connect ((gpointer) button_choose_starting_map, "clicked",
                      G_CALLBACK (on_button_choose_starting_map), NULL);

    /* For starting map window */
    choose_starting_map_window =  glade_xml_get_widget(dialog_xml, "choose_starting_map_window");

    gtk_window_set_transient_for(GTK_WINDOW(choose_starting_map_window), GTK_WINDOW(window_root));
    xml_tree = glade_get_widget_tree(GTK_WIDGET(create_character_window));

    create_char_pane[STARTING_MAP_PANE].textview = glade_xml_get_widget(dialog_xml,"textview_starting_map");
    textbuf_starting_map = gtk_text_view_get_buffer(
                               GTK_TEXT_VIEW(create_char_pane[STARTING_MAP_PANE].textview));
    add_tags_to_textbuffer(&create_char_pane[STARTING_MAP_PANE], textbuf_starting_map);
    add_style_to_textbuffer(&create_char_pane[STARTING_MAP_PANE], NULL);

    gtk_text_buffer_get_end_iter(create_char_pane[STARTING_MAP_PANE].textbuffer, &iter);
    create_char_pane[STARTING_MAP_PANE].textmark = gtk_text_buffer_create_mark(
                create_char_pane[STARTING_MAP_PANE].textbuffer, NULL, &iter, FALSE);

    button_csm_done = glade_xml_get_widget(dialog_xml,"button_csm_done");
    button_csm_cancel = glade_xml_get_widget(dialog_xml,"button_csm_cancel");
    combobox_starting_map = glade_xml_get_widget(dialog_xml,"combobox_starting_map");

    g_signal_connect ((gpointer) button_csm_done, "clicked",
                      G_CALLBACK (on_button_cc_done), NULL);
    g_signal_connect ((gpointer) button_csm_cancel, "clicked",
                      G_CALLBACK (on_button_csm_cancel), NULL);

}

