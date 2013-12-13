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
 * @file gtk-v2/src/account.c
 * Handles account login, creation, and character selection.
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

static GtkWidget *add_character_window, *choose_char_window,
       *create_account_window, *login_window, *account_password_window;

/* These are in the login_window */
static GtkWidget *button_login, *button_create_account,
       *button_go_metaserver, *button_exit_client,
       *entry_account_name,
       *entry_account_password, *label_account_login_status;

/* These are in the create_account_window */
static GtkWidget *button_new_create_account, *button_new_cancel,
       *entry_new_account_name,
       *entry_new_account_password, *entry_new_confirm_password,
       *label_create_account_status;

/* These are in the choose_character window */
static GtkWidget *button_play_character, *button_create_character,
       *button_add_character, *button_return_login, *button_account_password,
       *treeview_choose_character;

/* These are in the new_character window */
static GtkWidget *entry_new_character_name, *new_character_window,
       *label_new_char_status, *button_create_new_char,
       *button_new_char_cancel;

/* These are in the account_password window */
static GtkWidget *entry_account_password_current, *entry_account_password_new,
       *entry_account_password_confirm, *button_account_password_confirm,
       *button_account_password_cancel, *label_account_password_status;

GtkListStore    *character_store;

/* create_char.c also uses this */
char account_password[256];

/* This enum just maps the columns in the list store to their position.
 */
enum {CHAR_IMAGE, CHAR_NAME, CHAR_CLASS, CHAR_RACE, CHAR_LEVEL, CHAR_PARTY,
      CHAR_MAP, CHAR_ICON
     };
#define  CHAR_NUM_COLUMNS 8

/* These are in the add_character window */
static GtkWidget *button_do_add_character,
       *button_return_character_select, *entry_character_name,
       *entry_character_password, *label_add_status;

GtkTextBuffer *textbuf_motd, *textbuf_news, *textbuf_rules_account,
              *textbuf_rules_char;

/* These are used as offsets for num_text_views - we share the drawing code in
 * info.c if more textviews are added, note that NUM_TEXT_VIEWS in info.c
 * needs to be increased.
 */
#define TEXTVIEW_MOTD           0
#define TEXTVIEW_NEWS           1
#define TEXTVIEW_RULES_ACCOUNT  2
#define TEXTVIEW_RULES_CHAR     3

Info_Pane login_pane[4];

extern int num_text_views;

static int has_init = 0;

/**
 * Hides all the login related windows.  This is needed in case the client
 * loses the connection to the server (either through player going to
 * client/disconnect or network failure).  get_metaserver() calls this, as
 * well as AddMeSuccess
 */
void hide_all_login_windows() {
    extern GtkWidget *treeview_look;

    if (has_init) {
        /* If we have not initialized, nothing to hide */
        gtk_widget_hide(login_window);
        gtk_widget_hide(add_character_window);
        gtk_widget_hide(choose_char_window);
        gtk_widget_hide(create_account_window);
        gtk_widget_hide(new_character_window);
        gtk_widget_hide(account_password_window);
        create_character_window_hide(); /* create_char.c */

        /* If the player has started playing (this function being called from
         * AddMeSuccess), we want to make sure that the extended command entry
         * widget is not activated - we want normal command entry.  Where this
         * shows up is if the player was playing before and uses a savebed -
         * now the last thing activated is that entry widget.
         */
        gtk_widget_grab_focus(GTK_WIDGET(treeview_look));
    }
}

/**
 * Prevent delete_event closure and/or hiding of account windows.  All account
 * system windows ignore delete events and remain visible unless the user
 * clicks an appropriate button.
 *
 * @param window    Pointer to an account window that received a delete_event.
 * @param user_data Unused.
 */
gboolean on_window_delete_event(GtkWidget *window, gpointer *user_data) {
    return TRUE;
}

/*****************************************************************************
 * New character window functions
 *****************************************************************************/

/**
 * Pop up a dialog window with the error from the server.
 * Since both v1 and v2 character creation are supported,
 * either the new_character_window or the create_character_window
 * may be up, so we can not easily display an in window message -
 * a pop up is probably better, but it will also work no matter
 * what window is up.
 *
 * @param message
 * message - this comes from the server.
 */
void create_new_character_failure(char *message) {
    GtkWidget *dialog;

    dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
                                    GTK_MESSAGE_QUESTION, GTK_BUTTONS_OK,
                                    "Error: %s", message);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

static void create_new_character() {
    const char *name;
    uint8 buf[MAX_BUF];
    SockList sl;

    SockList_Init(&sl, buf);

    name =  gtk_entry_get_text(GTK_ENTRY(entry_new_character_name));

    if (!name || *name == 0) {
        gtk_label_set_text(GTK_LABEL(label_new_char_status),
                           "You must enter a character name.");
        return;
    } else {
        gtk_label_set_text(GTK_LABEL(label_new_char_status), "");

        SockList_AddString(&sl, "createplayer ");
        SockList_AddChar(&sl, strlen(name));
        SockList_AddString(&sl, name);
        SockList_AddChar(&sl, strlen(account_password));
        SockList_AddString(&sl, account_password);
        SockList_Send(&sl, csocket.fd);
    }
}

/**
 * User hit the create character button.  Get data, send to server.
 * @param button
 * @param user_data
 */
void
on_button_create_new_char_clicked(GtkButton *button, gpointer user_data) {
    create_new_character();
}

/**
 * User hit return in the new character name box.  Like above, get data and
 * send to server.
 * @param entry
 * @param user_data
 */
void on_entry_new_character_name(GtkEntry *entry, gpointer user_data) {
    create_new_character();
}

/**
 * User his hit the cancel button in the new character window, so hide the new
 * character window, show the choose character window.
 * @param button
 * @param user_data
 */
void
on_button_new_char_cancel_clicked(GtkButton *button, gpointer user_data) {
    gtk_widget_hide(new_character_window);
    gtk_widget_show(choose_char_window);
}

/**
 * Initializes the new character window.
 */
static void init_new_character_window() {
    new_character_window =
        GTK_WIDGET(gtk_builder_get_object(dialog_xml, "create_character_window"));

    gtk_window_set_transient_for(
        GTK_WINDOW(new_character_window), GTK_WINDOW(window_root));

    button_create_new_char =
        GTK_WIDGET(gtk_builder_get_object(dialog_xml, "button_create_character"));
    button_new_char_cancel =
        GTK_WIDGET(gtk_builder_get_object(dialog_xml, "button_cc_cancel"));
    entry_new_character_name =
        GTK_WIDGET(gtk_builder_get_object(dialog_xml, "cc_entry_new_character_name"));
    label_new_char_status =
        GTK_WIDGET(gtk_builder_get_object(dialog_xml, "label_cc_status_update"));

    g_signal_connect((gpointer) new_character_window, "delete_event",
                     G_CALLBACK(on_window_delete_event), NULL);
    g_signal_connect((gpointer) button_create_new_char, "clicked",
                     G_CALLBACK(on_button_create_new_char_clicked), NULL);
    g_signal_connect((gpointer) button_new_char_cancel, "clicked",
                     G_CALLBACK(on_button_new_char_cancel_clicked), NULL);
    g_signal_connect((gpointer) entry_new_character_name, "activate",
                     G_CALLBACK(on_entry_new_character_name), NULL);
}

/******************************************************************************
 * add_character_window functions
 *****************************************************************************/

/**
 * Sends a request to the server add add the character to this account.
 * @param name
 * @param password
 * @param force
 */
static void add_character_to_account(const char *name, const char *password,
                                     int force) {
    SockList sl;
    uint8 buf[MAX_BUF];

    if (!name || !password || *name == 0 || *password == 0) {
        gtk_label_set_text(GTK_LABEL(label_add_status),
                           "You must enter both a name and password!");
    } else {
        gtk_label_set_text(GTK_LABEL(label_add_status), "");

        SockList_Init(&sl, buf);
        SockList_AddString(&sl, "accountaddplayer ");
        SockList_AddChar(&sl, force);
        SockList_AddChar(&sl, strlen(name));
        SockList_AddString(&sl, name);
        SockList_AddChar(&sl, strlen(password));
        SockList_AddString(&sl, password);
        SockList_Send(&sl, csocket.fd);
    }
}

/**
 * Handles a failure from the server - pretty basic - just throw up the
 * message and let the user try again.  This is a response to the 'failure
 * accountaddplayer' command.  Calling this account_add_character_failure may
 * be a little bit of a misnomer, but all the other routines in this area
 * refer to character, not player.
 *
 * @param message  Message to display.  Unlike other messages, the first word
 * of this message should be an integer, which denotes if using the 'force'
 * option would allow the user to override this.
 */
void account_add_character_failure(char *message) {
    char *cp;
    int retry;

    retry = atoi(message);
    cp = strchr(message, ' ');
    if (cp) {
        cp++;
    } else {
        cp = message;
    }

    if (!retry) {
        gtk_label_set_text(GTK_LABEL(label_add_status), cp);
    } else {
        /* In this case, we can retry it and it should work if we set force.
         * So bring up a dialog, as the user what to do - if they enter yes,
         * we use force.  If not, we clear the entry fields and just continue
         * onward.
         */
        GtkWidget *dialog;
        int result;
        const char *name, *password;

        /* Bring up a dialog window */
        dialog =
            gtk_message_dialog_new(NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_QUESTION,
                                   GTK_BUTTONS_YES_NO, "%s\n%s", cp, "Apply anyways?");
        result = gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);

        if (result == GTK_RESPONSE_YES) {
            name =  gtk_entry_get_text(GTK_ENTRY(entry_character_name));
            password =  gtk_entry_get_text(GTK_ENTRY(entry_character_password));
            add_character_to_account(name, password, 1);
        } else {
            gtk_entry_set_text(GTK_ENTRY(entry_character_name), "");
            gtk_entry_set_text(GTK_ENTRY(entry_character_password), "");
            gtk_widget_grab_focus(entry_character_name);
        }
    }
}

/**
 * User has hit the add character button.  Let add_character_to_account()
 * do all the work.
 *
 * @param button
 * @param user_data
 */
void
on_button_do_add_character_clicked(GtkButton *button, gpointer user_data) {
    add_character_to_account(
        gtk_entry_get_text(GTK_ENTRY(entry_character_name)),
        gtk_entry_get_text(GTK_ENTRY(entry_character_password)), 0);
}

/**
 * User has hit the return to character selection button.  Pretty simple -
 * just hide this window, activate the other window.
 *
 * @param button
 * @param user_data
 */
void
on_button_return_character_select_clicked(GtkButton *button,
        gpointer user_data) {
    gtk_widget_hide(add_character_window);
    gtk_widget_show(choose_char_window);
}

/**
 * User has hit return in either name or password box.  If both boxes have non
 * empty data, process request.  Otherwise, either stay in same box if this
 * box is empty, or move to the other box.
 *
 * @param entry     Entry widget which generated this callback.
 * @param user_data
 */
void on_entry_character(GtkEntry *entry, gpointer user_data) {
    const char *name, *password;

    name =  gtk_entry_get_text(GTK_ENTRY(entry_character_name));
    password =  gtk_entry_get_text(GTK_ENTRY(entry_character_password));

    if (name && name[0] && password && password[0]) {
        add_character_to_account(name, password, 0);
    } else {
        const char *cp;

        /* First case - this widget is empty - do nothing */
        cp = gtk_entry_get_text(entry);
        if (!cp || !cp[0]) {
            return;
        }

        /* In this case, this widget is not empty - means the other one is.
         */
        if (entry == GTK_ENTRY(entry_character_name)) {
            gtk_widget_grab_focus(entry_character_password);
        } else {
            gtk_widget_grab_focus(entry_character_name);
        }
    }
}

static void init_add_character_window() {
    add_character_window =
        GTK_WIDGET(gtk_builder_get_object(dialog_xml, "add_character_window"));

    gtk_window_set_transient_for(
        GTK_WINDOW(add_character_window), GTK_WINDOW(window_root));

    button_do_add_character =
        GTK_WIDGET(gtk_builder_get_object(dialog_xml, "button_do_add_character"));
    button_return_character_select =
        GTK_WIDGET(gtk_builder_get_object(dialog_xml,
                                          "button_return_character_select"));
    entry_character_name =
        GTK_WIDGET(gtk_builder_get_object(dialog_xml, "entry_character_name"));
    entry_character_password =
        GTK_WIDGET(gtk_builder_get_object(dialog_xml, "entry_character_password"));
    label_add_status =
        GTK_WIDGET(gtk_builder_get_object(dialog_xml, "label_add_status"));

    g_signal_connect((gpointer) add_character_window, "delete_event",
                     G_CALLBACK(on_window_delete_event), NULL);
    g_signal_connect((gpointer) button_do_add_character, "clicked",
                     G_CALLBACK(on_button_do_add_character_clicked), NULL);
    g_signal_connect((gpointer) button_return_character_select, "clicked",
                     G_CALLBACK(on_button_return_character_select_clicked), NULL);
    g_signal_connect((gpointer) entry_character_name, "activate",
                     G_CALLBACK(on_entry_character), NULL);
    g_signal_connect((gpointer) entry_character_password, "activate",
                     G_CALLBACK(on_entry_character), NULL);
}

/*****************************************************************************
 * choose_char_window
 ****************************************************************************/

/**
 * Called when we get the accountplayers command from the server (indirectly
 * via AccountPlayersCmd).  This tells us to wipe any data from the treeview,
 * but also hide any other windows and make the choose_character_window
 * visible.
 */
void choose_character_init() {
    gtk_widget_hide(login_window);
    gtk_widget_hide(add_character_window);
    gtk_widget_hide(create_account_window);
    gtk_widget_show(choose_char_window);

    /* Store any old/stale entries */
    gtk_list_store_clear(character_store);
}

/**
 * Basic little function - this is used because
 * we make the choose_char_window widget private to this
 * file, but the create_char.c file will need to show
 * this if the user decides to abandon creation of a new
 * character.
 */
void choose_char_window_show() {
    gtk_widget_show(choose_char_window);
}


/**
 * User has done necessary steps to play a character.
 * @param name
 */
static void play_character(const char *name) {
    SockList sl;
    uint8 buf[MAX_BUF];

    SockList_Init(&sl, buf);
    SockList_AddString(&sl, "accountplay ");
    SockList_AddString(&sl, name);
    SockList_Send(&sl, csocket.fd);

    keybindings_init(name);
}

/**
 * User has hit the play character button.  Grab the selected entry, if there
 * is one.
 * @param button
 * @param user_data
 */
void
on_button_play_character_clicked(GtkButton *button, gpointer user_data) {
    GtkTreeSelection *selected;
    GtkTreeModel    *model;
    GtkTreeIter iter;
    char *name;

    selected = gtk_tree_view_get_selection(GTK_TREE_VIEW(
            treeview_choose_character));

    if (gtk_tree_selection_get_selected(selected, &model, &iter)) {
        gtk_tree_model_get(model, &iter, CHAR_NAME, &name, -1);

        play_character(name);
    }
}

/**
 *
 * @param button
 * @param user_data
 */
void
on_button_create_character_clicked(GtkButton *button, gpointer user_data) {
    gtk_widget_hide(choose_char_window);
    if (serverloginmethod >= 2) {
        create_character_window_show();
    } else {
        gtk_widget_show(new_character_window);
        gtk_entry_set_text(GTK_ENTRY(entry_new_character_name), "");
    }
}

/**
 * User has hit the add character button, so hide this window, show the add
 * character window.
 * @param button
 * @param user_data
 */
void
on_button_add_character_clicked(GtkButton *button, gpointer user_data) {
    gtk_widget_hide(choose_char_window);
    gtk_widget_show(add_character_window);
    gtk_entry_set_text(GTK_ENTRY(entry_character_name), "");
    gtk_entry_set_text(GTK_ENTRY(entry_character_password), "");
    gtk_widget_grab_focus(entry_character_name);
}

/**
 * User has hit the return to login window, so hide this window, show the
 * account login window.
 * @param button
 * @param user_data
 */
void
on_button_return_login_clicked(GtkButton *button, gpointer user_data) {
    gtk_widget_hide(choose_char_window);
    gtk_widget_show(login_window);
}

/**
 * User has hit the change account password, so hide this window, show the
 * account password change dialog.
 * @param button
 * @param user_data
 */
void
on_button_account_password_clicked(GtkButton *button, gpointer user_data) {
    gtk_widget_hide(choose_char_window);
    gtk_widget_show(account_password_window);
    /* reset previous values */
    gtk_entry_set_text(GTK_ENTRY(entry_account_password_current), "");
    gtk_entry_set_text(GTK_ENTRY(entry_account_password_new), "");
    gtk_entry_set_text(GTK_ENTRY(entry_account_password_confirm), "");
}

/**
 * This gets data and adds it to the list store.  This is called from
 * AccountPlayersCmd and data is from the accountplayers protocol command.
 * The parameters are data to add to the list store.
 *
 * @param name
 * @param class
 * @param race
 * @param face
 * @param party
 * @param map
 * @param level
 * @param faceno
 */
void update_character_choose(const char *name, const char *class,
                             const char *race, const char *face,
                             const char *party, const char *map,
                             int level, int faceno) {
    GtkTreeIter iter;

    gtk_list_store_append(character_store, &iter);

    /* If this pixmap matches pixmap[0], it means we are caching images and
     * this image hasn't been set up.  It looks better in this case to just
     * leave that area of the window blank vs drawing a question mark there.
     */
    if (pixmaps[faceno] == pixmaps[0]) {
        gtk_list_store_set(character_store, &iter,
                           CHAR_NAME, name,
                           CHAR_CLASS, class,
                           CHAR_RACE, race,
                           CHAR_IMAGE, face,
                           CHAR_PARTY, party,
                           CHAR_MAP, map,
                           CHAR_LEVEL, level,
                           -1);
    } else {
        gtk_list_store_set(character_store, &iter,
                           CHAR_ICON, pixmaps[faceno]->icon_image,
                           CHAR_NAME, name,
                           CHAR_CLASS, class,
                           CHAR_RACE, race,
                           CHAR_IMAGE, face,
                           CHAR_PARTY, party,
                           CHAR_MAP, map,
                           CHAR_LEVEL, level,
                           -1);
    }
}

/**
 * User has double clicked one of the character rows, so use that character as
 * the one to play.
 *
 * @param treeview  Treeview which activated that (should always be
 *                  treeview_choose_character)
 * @param path      Mechanism to get to selected entry
 * @param column    Activated column?
 * @param user_data Not set
 */
void on_treeview_choose_character_activated(GtkTreeView       *treeview,
        GtkTreePath       *path,
        GtkTreeViewColumn *column,
        gpointer          user_data) {
    GtkTreeIter iter;
    GtkTreeModel    *model;
    char *name;

    model = gtk_tree_view_get_model(treeview);
    if (gtk_tree_model_get_iter(model, &iter, path)) {
        gtk_tree_model_get(model, &iter, CHAR_NAME, &name, -1);

        if (!name) {
            LOG(LOG_ERROR, "account.c::on_treeview_choose_character_activated",
                "unable to get character name");
            return;
        }
        play_character(name);
    }
}

/**
 *
 */
static void init_choose_char_window() {
    GtkTextIter end;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;

    choose_char_window =
        GTK_WIDGET(gtk_builder_get_object(dialog_xml, "choose_character_window"));

    gtk_window_set_transient_for(
        GTK_WINDOW(choose_char_window), GTK_WINDOW(window_root));

    button_play_character =
        GTK_WIDGET(gtk_builder_get_object(dialog_xml, "button_play_character"));
    button_create_character =
        GTK_WIDGET(gtk_builder_get_object(dialog_xml, "button_create_character"));
    button_add_character =
        GTK_WIDGET(gtk_builder_get_object(dialog_xml, "button_add_character"));
    button_return_login =
        GTK_WIDGET(gtk_builder_get_object(dialog_xml, "button_return_login"));
    button_account_password =
        GTK_WIDGET(gtk_builder_get_object(dialog_xml, "button_account_password"));
    login_pane[TEXTVIEW_RULES_CHAR].textview =
        GTK_WIDGET(gtk_builder_get_object(dialog_xml, "textview_rules_char"));

    textbuf_rules_char =
        gtk_text_view_get_buffer(
            GTK_TEXT_VIEW(login_pane[TEXTVIEW_RULES_CHAR].textview));

    treeview_choose_character =
        GTK_WIDGET(gtk_builder_get_object(dialog_xml, "treeview_choose_character"));

    add_tags_to_textbuffer(
        &login_pane[TEXTVIEW_RULES_CHAR], textbuf_rules_char);
    add_style_to_textbuffer(&login_pane[TEXTVIEW_RULES_CHAR], NULL);
    gtk_text_buffer_get_end_iter(
        login_pane[TEXTVIEW_RULES_CHAR].textbuffer, &end);
    login_pane[TEXTVIEW_RULES_CHAR].textmark =
        gtk_text_buffer_create_mark(
            login_pane[TEXTVIEW_RULES_CHAR].textbuffer, NULL, &end, FALSE);

    g_signal_connect((gpointer) choose_char_window, "delete_event",
                     G_CALLBACK(on_window_delete_event), NULL);
    g_signal_connect((gpointer) button_play_character, "clicked",
                     G_CALLBACK(on_button_play_character_clicked), NULL);
    g_signal_connect((gpointer) button_create_character, "clicked",
                     G_CALLBACK(on_button_create_character_clicked), NULL);
    g_signal_connect((gpointer) button_add_character, "clicked",
                     G_CALLBACK(on_button_add_character_clicked), NULL);
    g_signal_connect((gpointer) button_return_login, "clicked",
                     G_CALLBACK(on_button_return_login_clicked), NULL);
    g_signal_connect((gpointer) button_account_password, "clicked",
                     G_CALLBACK(on_button_account_password_clicked), NULL);
    g_signal_connect((gpointer) treeview_choose_character, "row_activated",
                     G_CALLBACK(on_treeview_choose_character_activated), NULL);

    character_store = gtk_list_store_new(CHAR_NUM_COLUMNS,
                                         G_TYPE_STRING, G_TYPE_STRING,
                                         G_TYPE_STRING, G_TYPE_STRING,
                                         G_TYPE_INT, G_TYPE_STRING,
                                         G_TYPE_STRING, G_TYPE_OBJECT);
    gtk_tree_view_set_model(GTK_TREE_VIEW(treeview_choose_character),
                            GTK_TREE_MODEL(character_store));

    renderer = gtk_cell_renderer_pixbuf_new();
    column = gtk_tree_view_column_new_with_attributes("?", renderer,
             "pixbuf", CHAR_ICON,
             NULL);

    gtk_tree_view_column_set_min_width(column, image_size);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview_choose_character), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Character Name", renderer,
             "text", CHAR_NAME, NULL);
    gtk_tree_view_column_set_sort_column_id(column, CHAR_NAME);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview_choose_character), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Class", renderer,
             "text", CHAR_CLASS, NULL);
    gtk_tree_view_column_set_sort_column_id(column, CHAR_CLASS);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview_choose_character), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Race", renderer,
             "text", CHAR_RACE, NULL);
    gtk_tree_view_column_set_sort_column_id(column, CHAR_RACE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview_choose_character), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Level", renderer,
             "text", CHAR_LEVEL, NULL);
    gtk_tree_view_column_set_sort_column_id(column, CHAR_LEVEL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview_choose_character), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Party", renderer,
             "text", CHAR_PARTY, NULL);
    gtk_tree_view_column_set_sort_column_id(column, CHAR_PARTY);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview_choose_character), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Map", renderer,
             "text", CHAR_MAP, NULL);
    gtk_tree_view_column_set_sort_column_id(column, CHAR_MAP);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview_choose_character), column);
}

/******************************************************************************
 * create_account_window
 ******************************************************************************/

/**
 * Handles a failure from the server - pretty basic - just throw up the
 * message and let the user try again.
 * @param message
 */
void account_creation_failure(char *message) {
    gtk_label_set_text(GTK_LABEL(label_create_account_status), message);
}

/**
 * This does sanity checking of the passed in data, and if all is good, sends
 * the request to the server to create an account.  If all the data isn't
 * good, it puts up an error message.  In this routine, none of the entries
 * should be NULL - the caller should verify that before callin
 * do_account_create();
 *
 * @param name Desired account name - must not be NULL.
 * @param p1   First password - must not be NULL
 * @param p2   Second (confirmed) password.  This routine checks that p1 & p2
 *             are the same, and if not, puts up an error.  p2 must not be NULL
 */
static void do_account_create(const char *name, const char *p1,
                              const char *p2) {
    SockList sl;
    uint8 buf[MAX_BUF];

    if (strcmp(p1, p2)) {
        gtk_label_set_text(GTK_LABEL(label_create_account_status),
                           "The passwords you entered do not match!");
        return;
    } else {
        gtk_label_set_text(GTK_LABEL(label_create_account_status), "");
        SockList_Init(&sl, buf);
        SockList_AddString(&sl, "accountnew ");
        SockList_AddChar(&sl, strlen(name));
        SockList_AddString(&sl, name);
        SockList_AddChar(&sl, strlen(p1));
        SockList_AddString(&sl, p1);
        SockList_Send(&sl, csocket.fd);
        /* Store password away for new character creation */
        snprintf(account_password, sizeof(account_password), "%s", p1);
    }
}

/**
 * User clicked on the create account button.  In this case, we just process
 * the data and call do_account_create();
 * @param button
 * @param user_data
 */
void
on_button_new_create_account_clicked(GtkButton *button, gpointer user_data) {
    const char *password1, *password2, *name;

    password1 = gtk_entry_get_text(GTK_ENTRY(entry_new_account_password));
    password2 = gtk_entry_get_text(GTK_ENTRY(entry_new_confirm_password));
    name = gtk_entry_get_text(GTK_ENTRY(entry_new_account_name));

    if (name && name[0] && password1 && password1[0] && password2 && password2[0]) {
        do_account_create(name, password1, password2);
    } else {
        gtk_label_set_text(GTK_LABEL(label_create_account_status),
                           "You must fill in all three entries!");
    }
}

/**
 *
 * @param button
 * @param user_data
 */
void
on_button_new_cancel_clicked(GtkButton *button, gpointer user_data) {
    gtk_widget_hide(create_account_window);
    gtk_widget_show(login_window);
}

/**
 * This handles cases where the user hits return in one of the entry boxes.
 * We use the same callback for all 3 entry boxes, since the processing is
 * basically the same - if there is valid data in all of them, we try to
 * create an account - otherwise, we move to the next box.
 *
 * @param entry Entry box used to figure out what the next box is.
 * @param user_data Not used.
 */
void
on_entry_new_account(GtkEntry *entry, gpointer user_data) {

    const char *password1, *password2, *name, *cp;

    password1 = gtk_entry_get_text(GTK_ENTRY(entry_new_account_password));
    password2 = gtk_entry_get_text(GTK_ENTRY(entry_new_confirm_password));
    name = gtk_entry_get_text(GTK_ENTRY(entry_new_account_name));
    if (name && name[0] && password1 && password1[0] && password2 && password2[0]) {
        do_account_create(name, password1, password2);
    } else {
        /* In this case, one, or more, of the fields is blank.  If there were
         * more than 3 widgets, I might but them into an array to make cycling
         * easier
         */

        /* First case - if the currently active one is blank, no reason to
         * move onward.
         */
        cp = gtk_entry_get_text(entry);
        if (!cp || !cp[0]) {
            return;
        }

        /* I'm not sure if it would make more sense to advance to the first
         * NULL entry - but in that case, the pointer may hop in non intuitive
         * ways - in this case, the user may just need to hit return a few
         * times - MSW 2010/03/29
         */
        if (entry == GTK_ENTRY(entry_new_account_name)) {
            gtk_widget_grab_focus(entry_new_account_password);
        } else if (entry == GTK_ENTRY(entry_new_account_password)) {
            gtk_widget_grab_focus(entry_new_confirm_password);
        } else if (entry == GTK_ENTRY(entry_new_confirm_password)) {
            gtk_widget_grab_focus(entry_new_account_name);
        }
    }
}

/**
 * This initializes the create account window and sets up the various
 * callbacks.
 */
static void init_create_account_window() {
    GtkTextIter end;

    create_account_window =
        GTK_WIDGET(gtk_builder_get_object(dialog_xml, "create_account_window"));

    gtk_window_set_transient_for(
        GTK_WINDOW(create_account_window), GTK_WINDOW(window_root));

    button_new_create_account =
        GTK_WIDGET(gtk_builder_get_object(dialog_xml, "button_new_create_account"));
    button_new_cancel =
        GTK_WIDGET(gtk_builder_get_object(dialog_xml, "button_new_cancel"));
    login_pane[TEXTVIEW_RULES_ACCOUNT].textview =
        GTK_WIDGET(gtk_builder_get_object(dialog_xml, "textview_rules_account"));

    textbuf_rules_account =
        gtk_text_view_get_buffer(
            GTK_TEXT_VIEW(login_pane[TEXTVIEW_RULES_ACCOUNT].textview));

    entry_new_account_name =
        GTK_WIDGET(gtk_builder_get_object(dialog_xml, "entry_new_account_name"));
    entry_new_account_password =
        GTK_WIDGET(gtk_builder_get_object(dialog_xml, "entry_new_account_password"));
    entry_new_confirm_password =
        GTK_WIDGET(gtk_builder_get_object(dialog_xml, "entry_new_confirm_password"));
    label_create_account_status =
        GTK_WIDGET(gtk_builder_get_object(dialog_xml, "label_create_account_status"));

    add_tags_to_textbuffer(
        &login_pane[TEXTVIEW_RULES_ACCOUNT], textbuf_rules_account);
    add_style_to_textbuffer(&login_pane[TEXTVIEW_RULES_ACCOUNT], NULL);
    gtk_text_buffer_get_end_iter(
        login_pane[TEXTVIEW_RULES_ACCOUNT].textbuffer, &end);
    login_pane[TEXTVIEW_RULES_ACCOUNT].textmark =
        gtk_text_buffer_create_mark(
            login_pane[TEXTVIEW_RULES_ACCOUNT].textbuffer, NULL, &end, FALSE);

    g_signal_connect((gpointer) create_account_window, "delete_event",
                     G_CALLBACK(on_window_delete_event), NULL);
    g_signal_connect((gpointer) button_new_create_account, "clicked",
                     G_CALLBACK(on_button_new_create_account_clicked), NULL);
    g_signal_connect((gpointer) button_new_cancel, "clicked",
                     G_CALLBACK(on_button_new_cancel_clicked), NULL);
    g_signal_connect((gpointer) entry_new_account_name, "activate",
                     G_CALLBACK(on_entry_new_account), NULL);
    g_signal_connect((gpointer) entry_new_account_password, "activate",
                     G_CALLBACK(on_entry_new_account), NULL);
    g_signal_connect((gpointer) entry_new_confirm_password, "activate",
                     G_CALLBACK(on_entry_new_account), NULL);
}

/*****************************************************************************
 * login_window
 *****************************************************************************/

/**
 * Handles a failure from the server - pretty basic - just throw up the
 * message and let the user try again.
 * @param message
 */
void account_login_failure(char *message) {
    gtk_label_set_text(GTK_LABEL(label_account_login_status), message);
}

/**
 * User hit the create account button.  So we need to hide the login window
 * and bring up the create login window.
 * @param button
 * @param user_data
 */
void
on_button_create_account_clicked(GtkButton *button, gpointer user_data) {
    gtk_widget_hide(login_window);
    gtk_label_set_text(GTK_LABEL(label_create_account_status), "");
    gtk_entry_set_text(GTK_ENTRY(entry_new_account_name), "");
    gtk_entry_set_text(GTK_ENTRY(entry_new_account_password), "");
    gtk_entry_set_text(GTK_ENTRY(entry_new_confirm_password), "");
    gtk_widget_show(create_account_window);
}

/**
 * User hit the go to metaserver button.  Need to disconnect from The server,
 * and by clearing the csocket_fd, the main loop routine will bring up the
 * metaserver window.
 * @param button
 * @param user_data
 */
void
on_button_go_metaserver_clicked(GtkButton *button, gpointer user_data) {
    close_server_connection();

    if (csocket_fd) {
        gdk_input_remove(csocket_fd);
        csocket_fd = 0;
        gtk_main_quit();
    }
}

/**
 * User hit the exit client button.  Pretty simple in this case.
 * @param button
 * @param user_data
 */
void
on_button_exit_client_clicked(GtkButton *button, gpointer user_data) {
#ifdef WIN32
    script_killall();
#endif
    exit(0);
}

/**
 * This does the work of doing the login - mostly it just sends the request to
 * the server.  However, this might be called from either hitting the login
 * button or entering data in name/password and hitting return.
 * @param name
 * @param password
 */
static void do_account_login(const char *name, const char *password) {
    SockList sl;
    uint8 buf[MAX_BUF];

    if (!name || !password || *name == 0 || *password == 0) {
        gtk_label_set_text(GTK_LABEL(label_account_login_status),
                           "You must enter both a name and password!");
    } else {
        gtk_label_set_text(GTK_LABEL(label_account_login_status), "");

        SockList_Init(&sl, buf);
        SockList_AddString(&sl, "accountlogin ");
        SockList_AddChar(&sl, strlen(name));
        SockList_AddString(&sl, name);
        SockList_AddChar(&sl, strlen(password));
        SockList_AddString(&sl, password);
        SockList_Send(&sl, csocket.fd);
        /* Store password away for new character creation */
        snprintf(account_password, sizeof(account_password), "%s", password);
    }
}

/**
 * User hit the login button - just call do_account_login()
 * @param button
 * @param user_data
 */
void
on_button_login_clicked(GtkButton *button, gpointer user_data) {
    do_account_login(gtk_entry_get_text(GTK_ENTRY(entry_account_name)),
                     gtk_entry_get_text(GTK_ENTRY(entry_account_password)));
}

/**
 * User hit return in the name entry box.  If there is data in the password
 * box, attempt login, otherwise make the password box active.
 * @param entry
 * @param user_data
 */
void
on_entry_account_name_activate(GtkEntry *entry, gpointer user_data) {
    const char *password;

    password = gtk_entry_get_text(GTK_ENTRY(entry_account_password));

    if (!password || *password == 0) {
        gtk_widget_grab_focus(entry_account_password);
    } else {
        do_account_login(gtk_entry_get_text(GTK_ENTRY(entry_account_name)), password);
    }
}

/**
 * user hit return in the password box.  Like above, if name data, do login,
 * otherwise make the name box active.
 * @param entry
 * @param user_data
 */
void
on_entry_account_password_activate(GtkEntry *entry, gpointer user_data) {
    const char *name;

    name = gtk_entry_get_text(GTK_ENTRY(entry_account_name));

    if (!name || *name == 0) {
        gtk_widget_grab_focus(entry_account_name);
    } else {
        do_account_login(name, gtk_entry_get_text(GTK_ENTRY(entry_account_password)));
    }
}

/**
 * Sets up all the widget pointers, as well as setting up the callbacks for
 * the login windows widgets.
 */
static void init_login_window() {
    GtkTextIter end;

    login_window = GTK_WIDGET(gtk_builder_get_object(dialog_xml, "login_window"));

    if (!login_window) {
        error_dialog("Out of date dialog.glade", "Did you run 'make install'?");
        exit(1);
    }

    gtk_window_set_transient_for(
        GTK_WINDOW(login_window), GTK_WINDOW(window_root));

    button_login =
        GTK_WIDGET(gtk_builder_get_object(dialog_xml, "button_login"));
    button_create_account =
        GTK_WIDGET(gtk_builder_get_object(dialog_xml, "button_create_account"));
    button_go_metaserver =
        GTK_WIDGET(gtk_builder_get_object(dialog_xml, "button_go_metaserver"));
    button_exit_client =
        GTK_WIDGET(gtk_builder_get_object(dialog_xml, "button_exit_client"));
    label_account_login_status =
        GTK_WIDGET(gtk_builder_get_object(dialog_xml, "label_account_login_status"));
    login_pane[TEXTVIEW_MOTD].textview =
        GTK_WIDGET(gtk_builder_get_object(dialog_xml, "textview_motd"));

    textbuf_motd =
        gtk_text_view_get_buffer(
            GTK_TEXT_VIEW(login_pane[TEXTVIEW_MOTD].textview));

    add_tags_to_textbuffer(&login_pane[TEXTVIEW_MOTD], textbuf_motd);
    add_style_to_textbuffer(&login_pane[TEXTVIEW_MOTD], NULL);
    gtk_text_buffer_get_end_iter(login_pane[TEXTVIEW_MOTD].textbuffer, &end);
    login_pane[TEXTVIEW_MOTD].textmark =
        gtk_text_buffer_create_mark(
            login_pane[TEXTVIEW_MOTD].textbuffer, NULL, &end, FALSE);

    login_pane[TEXTVIEW_NEWS].textview =
        GTK_WIDGET(gtk_builder_get_object(dialog_xml, "textview_news"));

    textbuf_news =
        gtk_text_view_get_buffer(
            GTK_TEXT_VIEW(login_pane[TEXTVIEW_NEWS].textview));

    add_tags_to_textbuffer(&login_pane[TEXTVIEW_NEWS], textbuf_news);
    add_style_to_textbuffer(&login_pane[TEXTVIEW_NEWS], NULL);
    gtk_text_buffer_get_end_iter(login_pane[TEXTVIEW_NEWS].textbuffer, &end);
    login_pane[TEXTVIEW_NEWS].textmark =
        gtk_text_buffer_create_mark(
            login_pane[TEXTVIEW_NEWS].textbuffer, NULL, &end, FALSE);

    entry_account_name =
        GTK_WIDGET(gtk_builder_get_object(dialog_xml, "entry_account_name"));
    entry_account_password =
        GTK_WIDGET(gtk_builder_get_object(dialog_xml, "entry_account_password"));

    g_signal_connect((gpointer) login_window, "delete_event",
                     G_CALLBACK(on_window_delete_event), NULL);
    g_signal_connect((gpointer) entry_account_name, "activate",
                     G_CALLBACK(on_entry_account_name_activate), NULL);
    g_signal_connect((gpointer) entry_account_password, "activate",
                     G_CALLBACK(on_entry_account_password_activate), NULL);
    g_signal_connect((gpointer) button_login, "clicked",
                     G_CALLBACK(on_button_login_clicked), NULL);
    g_signal_connect((gpointer) button_create_account, "clicked",
                     G_CALLBACK(on_button_create_account_clicked), NULL);
    g_signal_connect((gpointer) button_go_metaserver, "clicked",
                     G_CALLBACK(on_button_go_metaserver_clicked), NULL);
    g_signal_connect((gpointer) button_exit_client, "clicked",
                     G_CALLBACK(on_button_exit_client_clicked), NULL);
}

/*****************************************************************************
 * Account password change
 ****************************************************************************/

/**
 * This does sanity checking of the passed in data, and if all is good, sends
 * the request to the server to change an account password.  If all the data isn't
 * good, it puts up an error message.  In this routine, none of the entries
 * should be NULL - the caller should verify that before callin
 * do_account_change();
 *
 * @param old  Current password.
 * @param p1   First password - must not be NULL
 * @param p2   Second (confirmed) password.  This routine checks that p1 & p2
 *             are the same, and if not, puts up an error.  p2 must not be NULL
 */
static void do_account_change(const char *old, const char *p1, const char *p2) {
    SockList sl;
    uint8 buf[MAX_BUF];

    if (strcmp(p1, p2)) {
        gtk_label_set_text(GTK_LABEL(label_account_password_status),
                           "The passwords you entered do not match!");
        return;
    } else {
        gtk_label_set_text(GTK_LABEL(label_account_password_status), "");
        SockList_Init(&sl, buf);
        SockList_AddString(&sl, "accountpw ");
        SockList_AddChar(&sl, strlen(old));
        SockList_AddString(&sl, old);
        SockList_AddChar(&sl, strlen(p1));
        SockList_AddString(&sl, p1);
        SockList_Send(&sl, csocket.fd);
        /* Store password away for new character creation */
        snprintf(account_password, sizeof(account_password), "%s", p1);
    }
}

/**
 * User has hit the cancel account password, so hide this window, show the
 * account main window.
 * @param button
 * @param user_data
 */
void
on_button_account_password_cancel_clicked(GtkButton *button,
        gpointer user_data) {
    gtk_widget_hide(account_password_window);
    gtk_widget_show(choose_char_window);
}

/**
 * User has hit the validate account password, so handle that.
 * @param button
 * @param user_data
 */
void
on_button_account_password_confirm_clicked(GtkButton *button,
        gpointer user_data) {
    do_account_change(gtk_entry_get_text(GTK_ENTRY(entry_account_password_current)),
                      gtk_entry_get_text(GTK_ENTRY(entry_account_password_new)),
                      gtk_entry_get_text(GTK_ENTRY(entry_account_password_confirm)));
}

/**
 * This handles cases where the user hits return in one of the entry boxes.
 * We use the same callback for all 3 entry boxes, since the processing is
 * basically the same - if there is valid data in all of them, we try to
 * create an account - otherwise, we move to the next box.
 *
 * @param entry Entry box used to figure out what the next box is.
 * @param user_data Not used.
 */
void
on_entry_account_password(GtkEntry *entry, gpointer user_data) {

    const char *old, *password1, *password2, *cp;

    old = gtk_entry_get_text(GTK_ENTRY(entry_account_password_current));
    password1 = gtk_entry_get_text(GTK_ENTRY(entry_account_password_new));
    password2 = gtk_entry_get_text(GTK_ENTRY(entry_account_password_confirm));
    if (old && old[0] && password1 && password1[0] && password2 && password2[0]) {
        do_account_change(old, password1, password2);
    } else {
        /* In this case, one, or more, of the fields is blank.  If there were
         * more than 3 widgets, I might but them into an array to make cycling
         * easier
         */

        /* First case - if the currently active one is blank, no reason to
         * move onward.
         */
        cp = gtk_entry_get_text(entry);
        if (!cp || !cp[0]) {
            return;
        }

        if (entry == GTK_ENTRY(entry_account_password_current)) {
            gtk_widget_grab_focus(entry_account_password_new);
        } else if (entry == GTK_ENTRY(entry_account_password_new)) {
            gtk_widget_grab_focus(entry_account_password_confirm);
        } else if (entry == GTK_ENTRY(entry_account_password_confirm)) {
            gtk_widget_grab_focus(entry_account_password_current);
        }
    }
}

void account_change_password_failure(char *message) {
    gtk_label_set_text(GTK_LABEL(label_account_password_status), message);
}

/**
 * This initializes the change account password window and sets up the various
 * callbacks.
 */
static void init_account_password_window() {
    account_password_window =
        GTK_WIDGET(gtk_builder_get_object(dialog_xml, "account_password_window"));

    gtk_window_set_transient_for(
        GTK_WINDOW(account_password_window), GTK_WINDOW(window_root));

    button_account_password_confirm =
        GTK_WIDGET(gtk_builder_get_object(dialog_xml,
                                          "button_account_password_confirm"));
    button_account_password_cancel =
        GTK_WIDGET(gtk_builder_get_object(dialog_xml,
                                          "button_account_password_cancel"));

    entry_account_password_current =
        GTK_WIDGET(gtk_builder_get_object(dialog_xml,
                                          "entry_account_password_current"));
    entry_account_password_new =
        GTK_WIDGET(gtk_builder_get_object(dialog_xml, "entry_account_password_new"));
    entry_account_password_confirm =
        GTK_WIDGET(gtk_builder_get_object(dialog_xml,
                                          "entry_account_password_confirm"));
    label_account_password_status =
        GTK_WIDGET(gtk_builder_get_object(dialog_xml, "label_account_password_status"));

    g_signal_connect((gpointer) account_password_window, "delete_event",
                     G_CALLBACK(on_window_delete_event), NULL);
    g_signal_connect((gpointer) button_account_password_confirm, "clicked",
                     G_CALLBACK(on_button_account_password_confirm_clicked), NULL);
    g_signal_connect((gpointer) button_account_password_cancel, "clicked",
                     G_CALLBACK(on_button_account_password_cancel_clicked), NULL);
    g_signal_connect((gpointer) entry_account_password_current, "activate",
                     G_CALLBACK(on_entry_account_password), NULL);
    g_signal_connect((gpointer) entry_account_password_new, "activate",
                     G_CALLBACK(on_entry_account_password), NULL);
    g_signal_connect((gpointer) entry_account_password_confirm, "activate",
                     G_CALLBACK(on_entry_account_password), NULL);
}


/*****************************************************************************
 * Common/generic functions
 ****************************************************************************/

/**
 * This is called from ReplyInfoCmd when it gets a response from
 * news/motd/rules.  It is very possible that the window will get displayed
 * before we got a reply response, so this tells the client to update it.
 *
 *@param type What data just got updated - text string of motd/news/rules
 */
void update_login_info(int type) {
    if (!has_init) {
        return;
    }

    /* In all cases, we clear the buffer, and if we have data, then set it to
     * that data.  This routine could be smarter an
     */
    if (type == INFO_NEWS) {
        gtk_text_buffer_set_text(textbuf_news, "", 0);
        if (news) {
            /* the format of the news entry is special - there are a series of
             * %entries, and they are in reverse older (newest last) we want
             * to get rid of the %, make them more visible (convert them to
             * bold) and reverse the order.
             */
            char *mynews, *cp, *el, big_buf[BIG_BUF], *cp1;

            mynews = strdup(news);
            /* We basically work from the end of the string going towards the
             * start looking for % characters.  If we find one, we have to
             * make sure it is at the start of the line or start of the buffer
             */
            for (cp = mynews + strlen(mynews); cp > mynews; cp--) {
                if (*cp == '%' && (*(cp - 1) == '\n' || cp == mynews)) {
                    /* Find the end of the line */
                    el = strchr(cp, '\n');
                    /* null out the newline, put el one char beyond it */
                    if (el) {
                        *el = 0;
                        el++;
                    }
                    /* There isn't a clear standard - % news may be valid, as
                     * might be %news.  If % news is used, it looks better to
                     * get rid of that leading space.
                     */
                    cp1 = cp + 1;
                    while (isspace(*cp1)) {
                        cp1++;
                    }

                    /* since we've null out the newline, this snprintf will
                     * only get the % line and that is it.  Mark it up
                     */
                    snprintf(big_buf, BIG_BUF, "[b]%s[/b]", cp1);
                    add_marked_text_to_pane(&login_pane[TEXTVIEW_NEWS], big_buf, 0, 0, 0);
                    /* Now we draw the text that goes with it, if it exists */
                    if (el) {
                        add_marked_text_to_pane(&login_pane[TEXTVIEW_NEWS], el, 0, 0, 0);
                    }

                    /* Now we wipe the % out.  In this way, the news buffer is
                     * shorter, so when it draws the ext, there will just be
                     * that between the % and the one we just wiped out.
                     */
                    *cp = 0;
                }
            }
            /* If there are remnants left over, or perhaps the news file isn't
             * formatted with % headers, display what we have got.
             */
            if (*mynews != 0) {
                add_marked_text_to_pane(&login_pane[TEXTVIEW_NEWS], mynews, 0, 0, 0);
            }
        }
    } else if (type == INFO_MOTD) {
        gtk_text_buffer_set_text(textbuf_motd, "", 0);
        if (motd) {
            add_marked_text_to_pane(&login_pane[TEXTVIEW_MOTD], motd, 0, 0, 0);
        }
    } else if (type == INFO_RULES) {
        gtk_text_buffer_set_text(textbuf_rules_account, "", 0);
        gtk_text_buffer_set_text(textbuf_rules_char, "", 0);

        if (rules) {
            add_marked_text_to_pane(&login_pane[TEXTVIEW_RULES_ACCOUNT], rules, 0, 0, 0);
            add_marked_text_to_pane(&login_pane[TEXTVIEW_RULES_CHAR], rules, 0, 0, 0);
        }
    }
}

/**
 * Starts the login process.  If not already done, gets widgets, sets up
 * callboacks, etc.  This is at the end of the file so all the callbacks are
 * defined before this function - in that way, we do not need forward
 * declarations.  This is called from SetupCmd in common/commands.c
 *
 * @param method Login method that the server suppots.
 */
void start_login(int method) {
    /* Store this away - if method is only 1, we can not do smart character
     * creation.
     */
    serverloginmethod = method;

    if (!has_init) {
        /* Since there are 4 windows associated with account and character
         * login, to make life a little easier, each section here does all the
         * work for one window, so it is easier to see that everything for a
         * window is done - don't need to hunt through what would otherwise be
         * a long routine looking for entries.
         */
        init_login_window();

        init_add_character_window();

        init_choose_char_window();

        init_create_account_window();

        init_new_character_window();

        init_account_password_window();

        has_init = 1;

        /* In case we have gotten news/motd/rules before getting here, update
         * it now.
         */
        update_login_info(INFO_NEWS);
        update_login_info(INFO_RULES);
        update_login_info(INFO_MOTD);
    }

    gtk_entry_set_text(GTK_ENTRY(entry_account_name), "");
    gtk_entry_set_text(GTK_ENTRY(entry_account_password), "");
    /* We set focus to account name - this makes the most sense if user is
     * logging in again - it is possible that the password is active, but both
     * fields are blank, which is not what is expected.
     */
    gtk_widget_grab_focus(entry_account_name);
    gtk_widget_show(login_window);
}

