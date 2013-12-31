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
 * @file gtk-v2/src/keys.c
 * Handles most of the keyboard related functions - binding and unbinding keys,
 * and handling keypresses and looking up the keys.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <stdlib.h>
#include <sys/stat.h>

#ifndef WIN32
#include <unistd.h>
#include <gdk/gdkx.h>
#else
#include <gdk/gdkwin32.h>
#define NoSymbol 0L                     /**< Special KeySym */
typedef int KeyCode;                    /**< Undefined type */
#endif

#include "client-types.h"
#include "client.h"
#include "main.h"
#include "proto.h"
#include "def-keys.h"

#include "image.h"
#include "gtk2proto.h"
#include "p_cmd.h"

struct keybind;
static int keybind_remove(struct keybind *entry);
static void keybind_free(struct keybind **entry);

/**
 * @{
 * @name UI Widgets
 * Widgets for the keybinding dialog
 */
static GtkWidget *fire_label, *run_label, *keybinding_window,
       *kb_scope_togglebutton_global, *kb_scope_togglebutton_character,
       *keybinding_checkbutton_any,
       *keybinding_checkbutton_control, *keybinding_checkbutton_shift,
       *keybinding_checkbutton_alt, *keybinding_checkbutton_meta,
       *keybinding_checkbutton_edit, *keybinding_entry_key,
       *keybinding_entry_command, *keybinding_treeview,
       *keybinding_button_remove, *keybinding_button_update,
       *keybinding_button_bind;

static GtkListStore *keybinding_store;  /**<Bound key list for bind dialog.*/
static GtkTreeSelection *keybinding_selection;

GtkWidget *spinbutton_count;
GtkWidget *entry_commands;
/**
 * @} EndOf UI Widgets
 */

/**
 * @{
 * @name KList Enum
 * Changed to KLIST_* to avoid conflicts in Win2000 and up
 */
enum {
    KLIST_ENTRY, KLIST_KEY, KLIST_MODS, KLIST_SCOPE, KLIST_EDIT, KLIST_COMMAND,
    KLIST_KEY_ENTRY
};
/**
 * @} EndOf KList Enum
 */

/**
 * @{
 * @name Bind Log
 */
#define MAX_HISTORY 50
#define MAX_COMMAND_LEN 256
char history[MAX_HISTORY][MAX_COMMAND_LEN];

static int cur_history_position = 0, scroll_history_position = 0;
/**
 * @} EndOf Bind Log
 */

/**
 * @{
 * @name key_entry struct
 * A keybinding hash record structure.
 */
struct keybind {
    uint8       flags;                  /**< KEYF_* flags set for the record.*/
    sint8       direction;              /**< -1 non-direction key, else >= 0.*/
    uint32      keysym;                 /**< Key this binding record is for. */
    char        *command;               /**< Command string bound to a key. */
    struct keybind *next;
};


/***********************************************************************
 *
 * Key board input translations are handled here.  We don't deal with
 * the events, but rather KeyCodes and KeySyms.
 *
 * It would be nice to deal with only KeySyms, but many keyboards
 * have keys that do not correspond to a KeySym, so we do need to
 * support KeyCodes.
 *
 ***********************************************************************/

static uint32 firekeysym[2], runkeysym[2], commandkeysym, *bind_keysym,
       prevkeysym, nextkeysym, completekeysym, altkeysym[2], metakeysym[2],
       cancelkeysym;

static int bind_flags = 0;
static char bind_buf[MAX_BUF];

/*
 * Key modifiers
 *
 * The Run, Fire, Alt and/or Meta keys can be used to qualify a key
 * (i.e. a keybinding of the key 's' with the KEYF_RUN and KEYF_FIRE
 * flags set will only match if both Run and Fire are held while 's' is
 * pressed).
 *
 * If the user wants a key to match no matter the state of the modifier
 * keys, the KEYF_ANY flag must be set in the binding.
 */

#define KEYF_MOD_SHIFT  (1 << 0)            /**< Used in fire mode */
#define KEYF_MOD_CTRL   (1 << 1)            /**< Used in run mode */
#define KEYF_MOD_ALT    (1 << 2)            /**< For ALT key modifier */
#define KEYF_MOD_META   (1 << 3)            /**< For Meta key modifier */
#define KEYF_MOD_MASK   (KEYF_MOD_SHIFT |   \
                         KEYF_MOD_CTRL |    \
                         KEYF_MOD_ALT |     \
                         KEYF_MOD_META)

#define KEYF_ANY        (1 << 4)            /**< Don't care about modifiers */
#define KEYF_EDIT       (1 << 5)            /**< Enter command mode */

/* Keybinding's scope, decides where the binding will be saved */
#define KEYF_R_GLOBAL   (1 << 6)            /**< Save at user's file */
#define KEYF_R_CHAR     (1 << 7)            /**< Character specific */

extern const char *const directions[9];

#define KEYHASH 257
/**
 * Will hold the keybindings into two separate hashes depending on
 * the scope they afect (global or character).
 * This allows editting both scopes at the same time and switch scopes
 * for a certain binding with ease.
 *
 * Platform independence defines that we can't use keycodes.  Instead,
 * make it a hash, and set KEYHASH to a prime number for this purpose.
 */
static struct keybind *keys_global[KEYHASH], *keys_char[KEYHASH];

/**
 * @defgroup GtkV2KeyBinding GTK-V2 client keybinding functions.
 * @{
 */

#define EKEYBIND_NOMEM               1

/**
 * Find a keybinding for keysym.
 *
 * Make it possible to match a specific keysym-and-key-modifier combo
 * (useful in game play), or to match keysym regardless of modifier.
 *
 * @param flags If flags has got KEYF_ANY set, the keybinding's own
 *              flags are ignored and any match is returned.
 *              If a keybinding matching keysym which has got KEYF_ANY
 *              set is found, the flags param is ignored and the binding
 *              is returned.
 *              Otherwise, return only bindings with matching modifier
 *              flags.
 * @param scope Determines which scope to search for the binding.
 *              0 meaning char scope, non zero meaning global scope.
 */
static struct keybind *keybind_find(uint32 keysym, unsigned int flags, int scope)
{
    struct keybind *kb;
    kb = scope?keys_global[keysym % KEYHASH]:keys_char[keysym % KEYHASH];
    while (kb != NULL) {
        if (kb->keysym == 0 || kb->keysym == keysym) {
            if ((kb->flags & KEYF_ANY) || (flags & KEYF_ANY))
                return kb;
            if ((kb->flags & KEYF_MOD_MASK) == (flags & KEYF_MOD_MASK))
                return kb;
        }
        kb = kb->next;
    }

    return NULL;
}

/**
 * Updates the keys array with the keybinding that is passed.  It allocates
 * memory for the array entry, then uses strdup_local() to allocate memory
 * for the command being bound. This function is common to both gdk and x11
 * client.
 *
 * @param keysym A key to bind.
 * @param flags State that the keyboard is in.
 * @param command A command to bind to the key specified in keysym.
 */
static int keybind_insert(uint32 keysym, unsigned int flags, const char *command)
{
    struct keybind **next_ptr, *kb;
    int slot;
    int i;
    int dir;

    kb = keybind_find(keysym, flags, (flags & KEYF_R_GLOBAL));
    while (kb != NULL) {
        /*
         * Keep the last binding instead of the first (backwards compatible).
         *
         * Also, if the new binding has the ANY flag, remove all matching
         * previous bindings and keep this one.
         */
        LOG(LOG_WARNING, "gtk-v2::keybind_insert",
            "Overwriting previous binding for key %s with command %s ",
            gdk_keyval_name(keysym), kb->command);
        keybind_remove(kb);
        keybind_free(&kb);
        kb = keybind_find(keysym, flags, (flags & KEYF_R_GLOBAL));
    }

    slot = keysym % KEYHASH;

    next_ptr = (flags & KEYF_R_GLOBAL) ? &keys_global[slot] : &keys_char[slot];
    while (*next_ptr)
        next_ptr = &(*next_ptr)->next;
    *next_ptr = calloc(1, sizeof(**next_ptr));
    if (*next_ptr == NULL)
        return -EKEYBIND_NOMEM;

    (*next_ptr)->keysym = keysym;
    (*next_ptr)->flags = flags;
    (*next_ptr)->command = strdup_local(command);

    /*
     * Try to find out if the command is a direction command.  If so, keep
     * track of this fact, so in fire or run mode, things work correctly.
     */
    dir = -1;
    for (i = 0; i < 9; i++) {
        if (!strcmp(command, directions[i])) {
            dir = i;
            break;
        }
    }
    (*next_ptr)->direction = dir;

    return 0;
}

static int keybind_remove(struct keybind *entry)
{
    struct keybind **next_ptr;
    int slot;

    slot = entry->keysym % KEYHASH;

    next_ptr = (entry->flags & KEYF_R_GLOBAL) ? &keys_global[slot] : &keys_char[slot];
    while (*next_ptr) {
        if (*next_ptr == entry) {
            *next_ptr = entry->next;
            return 0;
        }
        next_ptr = &(*next_ptr)->next;
    }

    /* No such key entry */
    return -1;
}

static void keybind_free(struct keybind **entry)
{
    free((*entry)->command);
    (*entry)->command = NULL;
    free(*entry);
    *entry = NULL;
}

/**
 * This function is common to both gdk and x11 client
 *
 * @param buf
 * @param line
 * @param scope_flag  KEYF_R_GLOBAL or KEYF_R_CHAR determining scope.
 */
static void parse_keybind_line(char *buf, int line, unsigned int scope_flag)
{
    char *cp, *cpnext;
    uint32 keysym, low_keysym;
    int flags;

    /*
     * There may be a rare error case when cp is used uninitialized. So let's
     * be safe
     */
    cp = NULL;

    if (buf[0] == '#' || buf[0] == '\n') {
        return;
    }
    cpnext = strchr(buf,' ');
    if (cpnext == NULL) {
        LOG(LOG_WARNING, "gtk-v2::parse_keybind_line",
            "Line %d (%s) corrupted in keybinding file.", line, buf);
        return;
    }
    /* Special keybinding line */
    if (buf[0] == '!') {
        char *cp1;
        while (*cpnext == ' ') {
            ++cpnext;
        }
        cp = strchr(cpnext, ' ');
        if (!cp) {
            LOG(LOG_WARNING, "gtk-v2::parse_keybind_line",
                "Line %d (%s) corrupted in keybinding file.", line, buf);
            return;
        }
        *cp++ = 0;  /* Null terminate it */
        cp1 = strchr(cp, ' ');
        if (!cp1) {
            LOG(LOG_WARNING, "gtk-v2::parse_keybind_line",
                "Line %d (%s) corrupted in keybinding file.", line, buf);
            return;
        }
        *cp1++ = 0; /* Null terminate it */
        keysym = gdk_keyval_from_name(cp);
        /* As of now, all these keys must have keysyms */
        if (keysym == 0) {
            LOG(LOG_WARNING, "gtk-v2::parse_keybind_line",
                "Could not convert %s into keysym", cp);
            return;
        }
        if (!strcmp(cpnext, "commandkey")) {
            commandkeysym = keysym;
            return;
        }
        if (!strcmp(cpnext, "altkey0")) {
            altkeysym[0] = keysym;
            return;
        }
        if (!strcmp(cpnext, "altkey1")) {
            altkeysym[1] = keysym;
            return;
        }
        if (!strcmp(cpnext, "firekey0")) {
            firekeysym[0] = keysym;
            return;
        }
        if (!strcmp(cpnext, "firekey1")) {
            firekeysym[1] = keysym;
            return;
        }
        if (!strcmp(cpnext, "metakey0")) {
            metakeysym[0] = keysym;
            return;
        }
        if (!strcmp(cpnext, "metakey1")) {
            metakeysym[1] = keysym;
            return;
        }
        if (!strcmp(cpnext, "runkey0")) {
            runkeysym[0] = keysym;
            return;
        }
        if (!strcmp(cpnext, "runkey1")) {
            runkeysym[1] = keysym;
            return;
        }
        if (!strcmp(cpnext, "completekey")) {
            completekeysym = keysym;
            return;
        }
        if (!strcmp(cpnext, "nextkey")) {
            nextkeysym = keysym;
            return;
        }
        if (!strcmp(cpnext, "prevkey")) {
            prevkeysym = keysym;
            return;
        }
    } else {
        *cpnext++ = '\0';
        keysym = gdk_keyval_from_name(buf);
        if (!keysym) {
            LOG(LOG_WARNING, "gtk-v2::parse_keybind_line",
                "Unable to convert line %d (%s) into keysym", line, cp);
            return;
        }
        cp = cpnext;
        cpnext = strchr(cp, ' ');
        if (cpnext == NULL) {
            LOG(LOG_WARNING, "gtk-v2::parse_keybind_line",
                "Line %d (%s) corrupted in keybinding file.", line, cp);
            return;
        }
        *cpnext++ = '\0';

        cp = cpnext;
        cpnext = strchr(cp, ' ');
        if (cpnext == NULL) {
            LOG(LOG_WARNING, "gtk-v2::parse_keybind_line",
                "Line %d (%s) corrupted in keybinding file.", line, cp);
            return;
        }
        *cpnext++ = '\0';

        flags = 0;
        low_keysym = gdk_keyval_to_lower(keysym);
        if (low_keysym != keysym) {
            /* This binding is uppercase, switch to lowercase and flag the shift modifier */
            flags |= KEYF_MOD_SHIFT;
            keysym = low_keysym;
        }
        while (*cp != '\0') {
            switch (*cp) {
            case 'A':
                flags |= KEYF_ANY;
                break;
            case 'E':
                flags |= KEYF_EDIT;
                break;
            case 'F':
                flags |= KEYF_MOD_SHIFT;
                break;
            case 'L':   /* A is used, so using L for alt */
                flags |= KEYF_MOD_ALT;
                break;
            case 'M':
                flags |= KEYF_MOD_META;
                break;
            case 'N':
                /* Nothing to do */
                break;
            case 'R':
                flags |= KEYF_MOD_CTRL;
                break;
            case 'S':
                LOG(LOG_WARNING, "gtk-v2::parse_keybind_line",
                    "Deprecated flag (S) ignored at line %d in key binding file", line);
                break;
            default:
                LOG(LOG_WARNING, "gtk-v2::parse_keybind_line",
                    "Unknown flag (%c) line %d in key binding file",
                    *cp, line);
            }
            cp++;
        }

        /* Rest of the line is the actual command.  Lets kill the newline */
        cpnext[strlen(cpnext) - 1] = '\0';
        if (strlen(cpnext) > (sizeof(bind_buf) - 1)) {
            cpnext[sizeof(bind_buf) - 1] = '\0';
            LOG(LOG_WARNING, "gtk-v2::parse_keybind_line",
                "Command too long! Truncated.");
        }

        flags |= scope_flag;  /* add the corresponding scope flag */
        keybind_insert(keysym, flags, cpnext);

    } /* else if not special binding line */
}

/**
 * Initialize the built-in default keybindings from the 'def-keys.h' file.
 */
static void init_default_keybindings(void)
{
    char buf[MAX_BUF];
    int i;

    LOG(LOG_DEBUG, "gtk-v2::init_default_keybindings",
        "Using built-in defaults");

    for (i = 0; i < sizeof(def_keys) / sizeof(char *); i++) {
        strcpy(buf, def_keys[i]);
        parse_keybind_line(buf, i, KEYF_R_GLOBAL);
    }
}

/**
 * Opens a file and loads the keybinds contained in it.
 *
 * @param filename   Name of the file to open.
 * @param scope_flag The scope this bindings should be loaded with.
 *                   Should be one of KEYF_R_GLOBAL or KEYF_R_CHAR.
 *                   Every binding in the file will have the same scope.
 */
static int parse_keys_file(char *filename, unsigned int scope_flag)
{
    int line = 0;
    FILE *fp;
    char buf[BIG_BUF];

    CONVERT_FILESPEC_TO_OS_FORMAT(filename);
    LOG(LOG_INFO, "gtk-v2::init_keys",
        "Trying to open keybinding file %s", filename);

    fp = fopen(filename, "r");
    if (fp == NULL) {
        return -1;
    }

    while (fgets(buf, BIG_BUF, fp)) {
        line++;
        buf[BIG_BUF - 1] = '\0';
        parse_keybind_line(buf, line, scope_flag);
    }

    fclose(fp);
    return 0;
}

/**
 * Reads in the keybindings, and initializes special values. Called
 * from main() as part of the client start up. The function is common to both
 * the x11 and gdk clients.
 */
void keybindings_init(const char *character_name)
{
    int i;
    char buf[BIG_BUF];

    for (i = 0; i < MAX_HISTORY; i++) { /* Clear out the bind history log */
        history[i][0] = 0;
    }

    commandkeysym  = GDK_apostrophe;
    firekeysym[0]  = GDK_Shift_L;
    firekeysym[1]  = GDK_Shift_R;
    runkeysym[0]   = GDK_Control_L;
    runkeysym[1]   = GDK_Control_R;
    metakeysym[0]  = GDK_Meta_L;
    metakeysym[1]  = GDK_Meta_R;
    altkeysym[0]   = GDK_Alt_L;
    altkeysym[1]   = GDK_Alt_R;

    completekeysym = GDK_Tab;
    cancelkeysym   = GDK_Escape;

    /*
     * Don't set these to anything by default.  At least on Sun keyboards, the
     * keysym for up on both the keypad and arrow keys is the same, so player
     * needs to rebind this so we get proper keycode.  Very unfriendly to log
     * in and not be able to move north/south.
     */
    nextkeysym = NoSymbol;
    prevkeysym = NoSymbol;

    for (i = 0; i < KEYHASH; i++) {
      while (keys_global[i])
            keybind_remove(keys_global[i]);
      while (keys_char[i])
            keybind_remove(keys_char[i]);
    }

    /*
     * If we were supplied with a character name, store it so that we
     * can load and save a character-specific keys file.
     */
    if (cpl.name) {
        free(cpl.name);
        cpl.name = NULL;
    }
    if (character_name) {
        cpl.name = strdup(character_name);
    }

    /*
     * We now try to load the keybindings.  First load defaults.
     * Then go through the more specific files in the home directory:
     *   1) user wide "~/.crossfire/keys".
     *   2) and, if character name is known, "~/.crossfire/<name>.keys"
     *
     * The format is described in the def_keys file.  Note that this file is
     * the same as what it was in the server distribution.  To convert bindings
     * in character files to this format, all that needs to be done is remove
     * the 'key ' at the start of each line.
     */

    init_default_keybindings();

    /* Try the global keys file */
    snprintf(buf, sizeof(buf), "%s/.crossfire/keys", getenv("HOME"));
    parse_keys_file(buf, KEYF_R_GLOBAL);

    if (cpl.name) {
        /* Try the character-specific keys file */
      snprintf(buf, sizeof(buf), "%s/.crossfire/\"%s.%s.keys\"",
               getenv("HOME"), csocket.servername, cpl.name);
        parse_keys_file(buf, KEYF_R_CHAR);
    }
}


/**
 * One-time initialization of windows and signals for the keybindings
 * dialog. It is called from main() as part of the client start up. The
 * function is common to both the x11 and gdk clients.
 *
 * @param window_root The client's main window.
 */
void keys_init(GtkWidget *window_root)
{
    GtkTreeViewColumn *column;
    GtkCellRenderer *renderer;
    GtkWidget *widget;
    int i;

    fire_label = GTK_WIDGET(gtk_builder_get_object(window_xml, "fire_label"));
    run_label = GTK_WIDGET(gtk_builder_get_object(window_xml, "run_label"));
    entry_commands = GTK_WIDGET(gtk_builder_get_object(window_xml,
            "entry_commands"));
    spinbutton_count = GTK_WIDGET(gtk_builder_get_object(window_xml,
            "spinbutton_count"));

    g_signal_connect((gpointer) entry_commands, "activate",
                     G_CALLBACK(on_entry_commands_activate), NULL);

    keybinding_window = GTK_WIDGET(gtk_builder_get_object(dialog_xml, "keybinding_window"));

    kb_scope_togglebutton_global =
        GTK_WIDGET(gtk_builder_get_object(dialog_xml, "kb_scope_togglebutton_global"));
    kb_scope_togglebutton_character =
        GTK_WIDGET(gtk_builder_get_object(dialog_xml, "kb_scope_togglebutton_character"));
    keybinding_checkbutton_any =
        GTK_WIDGET(gtk_builder_get_object(dialog_xml, "keybinding_checkbutton_any"));
    keybinding_checkbutton_control =
        GTK_WIDGET(gtk_builder_get_object(dialog_xml, "keybinding_checkbutton_control"));
    keybinding_checkbutton_shift =
        GTK_WIDGET(gtk_builder_get_object(dialog_xml, "keybinding_checkbutton_shift"));
    keybinding_checkbutton_alt =
        GTK_WIDGET(gtk_builder_get_object(dialog_xml, "keybinding_checkbutton_alt"));
    keybinding_checkbutton_meta =
        GTK_WIDGET(gtk_builder_get_object(dialog_xml, "keybinding_checkbutton_meta"));
    keybinding_checkbutton_edit =
        GTK_WIDGET(gtk_builder_get_object(dialog_xml, "keybinding_checkbutton_stayinedit"));
    keybinding_entry_key =
        GTK_WIDGET(gtk_builder_get_object(dialog_xml, "keybinding_entry_key"));
    keybinding_entry_command =
        GTK_WIDGET(gtk_builder_get_object(dialog_xml, "keybinding_entry_command"));
    keybinding_treeview =
        GTK_WIDGET(gtk_builder_get_object(dialog_xml, "keybinding_treeview"));
    keybinding_button_remove =
        GTK_WIDGET(gtk_builder_get_object(dialog_xml, "keybinding_button_remove"));
    keybinding_button_update =
        GTK_WIDGET(gtk_builder_get_object(dialog_xml, "keybinding_button_update"));
    keybinding_button_bind =
        GTK_WIDGET(gtk_builder_get_object(dialog_xml, "keybinding_button_bind"));

    g_signal_connect((gpointer) keybinding_window, "delete_event",
                     G_CALLBACK(gtk_widget_hide_on_delete), NULL);
    g_signal_connect((gpointer) keybinding_entry_key, "key_press_event",
                     G_CALLBACK(on_keybinding_entry_key_key_press_event), NULL);
    g_signal_connect((gpointer) keybinding_button_remove, "clicked",
                     G_CALLBACK(on_keybinding_button_remove_clicked), NULL);
    g_signal_connect((gpointer) keybinding_button_update, "clicked",
                     G_CALLBACK(on_keybinding_button_update_clicked), NULL);
    g_signal_connect((gpointer) keybinding_button_bind, "clicked",
                     G_CALLBACK(on_keybinding_button_bind_clicked), NULL);

    g_signal_connect((gpointer) kb_scope_togglebutton_character, "toggled",
                     G_CALLBACK(on_kb_scope_togglebutton_character_toggled), NULL);
    g_signal_connect((gpointer) kb_scope_togglebutton_global, "toggled",
                     G_CALLBACK(on_kb_scope_togglebutton_global_toggled), NULL);

    g_signal_connect((gpointer) keybinding_checkbutton_any, "clicked",
                     G_CALLBACK(on_keybinding_checkbutton_any_clicked), NULL);

    widget = GTK_WIDGET(gtk_builder_get_object(dialog_xml, "keybinding_button_clear"));
    g_signal_connect((gpointer) widget, "clicked",
                     G_CALLBACK(on_keybinding_button_clear_clicked), NULL);

    widget = GTK_WIDGET(gtk_builder_get_object(dialog_xml, "keybinding_button_close"));
    g_signal_connect((gpointer) widget, "clicked",
                     G_CALLBACK(on_keybinding_button_close_clicked), NULL);

    gtk_widget_set_sensitive(keybinding_button_remove, FALSE);
    gtk_widget_set_sensitive(keybinding_button_update, FALSE);
    keybinding_store = gtk_list_store_new(7,
                                          G_TYPE_INT,
                                          G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
                                          G_TYPE_POINTER
                                         );
    gtk_tree_view_set_model(GTK_TREE_VIEW(keybinding_treeview), GTK_TREE_MODEL(keybinding_store));

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes ("Key", renderer,
             "text", KLIST_KEY,
             NULL);
    gtk_tree_view_column_set_sort_column_id(column, KLIST_KEY);
    gtk_tree_view_append_column (GTK_TREE_VIEW (keybinding_treeview), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes ("Modifiers", renderer,
             "text", KLIST_MODS,
             NULL);
    gtk_tree_view_column_set_sort_column_id(column, KLIST_MODS);
    gtk_tree_view_append_column (GTK_TREE_VIEW (keybinding_treeview), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes ("Scope", renderer,
             "text", KLIST_SCOPE,
             NULL);
    gtk_tree_view_column_set_sort_column_id(column, KLIST_SCOPE);
    gtk_tree_view_append_column (GTK_TREE_VIEW (keybinding_treeview), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes ("Edit Mode", renderer,
             "text", KLIST_EDIT,
             NULL);
    gtk_tree_view_column_set_sort_column_id(column, KLIST_EDIT);
    gtk_tree_view_append_column (GTK_TREE_VIEW (keybinding_treeview), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes ("Command", renderer,
             "text", KLIST_COMMAND,
             NULL);
    gtk_tree_view_column_set_sort_column_id(column, KLIST_COMMAND);
    gtk_tree_view_append_column(GTK_TREE_VIEW (keybinding_treeview), column);


    keybinding_selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(keybinding_treeview));
    gtk_tree_selection_set_mode(keybinding_selection, GTK_SELECTION_BROWSE);
    gtk_tree_selection_set_select_function(keybinding_selection, keybinding_selection_func, NULL, NULL);

    gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(keybinding_store),
                                         KLIST_KEY,
                                         GTK_SORT_ASCENDING);

    for (i = 0; i < KEYHASH; i++) {
        keys_global[i] = NULL;
        keys_char[i] = NULL;
    }

    /*
     * Old servers (e.g. 1.12) starts game play without a login
     * process. We can't get the character name on such a server, so
     * load default and global key bindings here in case we
     * don't get the character-specific one later.
     */
    keybindings_init(NULL);
}

/**
 * The only things we actually care about is the run and fire keys.  Other key
 * releases are not important.  If it is the release of a run or fire key, we
 * tell the client to stop firing or running.  In some cases, it is possible
 * that we actually are not running or firing, and in such cases, the server
 * will just ignore the command.
 *
 * This code is used by gdk and x11 client, but has a fair number of ifdefs to
 * get the right behavior.
 *
 * @param ks
 */
static void parse_key_release(uint32 keysym)
{

    /*
     * Only send stop firing/running commands if we are in actual play mode.
     * Something smart does need to be done when the character enters a non
     * play mode with fire or run mode already set, however.
     */
    if (keysym == firekeysym[0] || keysym == firekeysym[1]) {
        cpl.fire_on = 0;
        clear_fire();
        gtk_label_set(GTK_LABEL(fire_label), "    ");
    } else if (keysym == runkeysym[0] || keysym == runkeysym[1]) {
        cpl.run_on = 0;
        if (use_config[CONFIG_ECHO])
            draw_ext_info(NDI_BLACK, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_NOTICE,
                          "stop run");
        clear_run();
        gtk_label_set(GTK_LABEL(run_label), "   ");
    } else if (keysym == altkeysym[0] || keysym == altkeysym[1]) {
        cpl.alt_on = 0;
    } else if (keysym == metakeysym[0] || keysym == metakeysym[1]) {
        cpl.meta_on = 0;
    }
    /*
     * Firing is handled on server side.  However, to keep more like the old
     * version, if you release the direction key, you want the firing to stop.
     * This should do that.
     */
    else if (cpl.fire_on) {
        clear_fire();
    }
}

/**
 * Parses a keypress.  It should only be called when in Playing mode.
 *
 * @param key
 * @param keysym
 */
static void parse_key(char key, uint32 keysym)
{
    struct keybind *kb;
    int present_flags = 0;
    char buf[MAX_BUF], tmpbuf[MAX_BUF];

    /* We handle the Shift key separately */
    keysym = gdk_keyval_to_lower(keysym);

    if (keysym == commandkeysym) {
        gtk_widget_grab_focus(GTK_WIDGET(entry_commands));
        gtk_entry_set_visibility(GTK_ENTRY(entry_commands), 1);
        cpl.input_state = Command_Mode;
        cpl.no_echo = FALSE;
        return;
    }
    if (keysym == altkeysym[0] || keysym == altkeysym[1]) {
        cpl.alt_on = 1;
        return;
    }
    if (keysym == metakeysym[0] || keysym == metakeysym[1]) {
        cpl.meta_on = 1;
        return;
    }
    if (keysym == firekeysym[0] || keysym == firekeysym[1]) {
        cpl.fire_on = 1;
        gtk_label_set(GTK_LABEL(fire_label), "Fire");
        return;
    }
    if (keysym == runkeysym[0] || keysym == runkeysym[1]) {
        cpl.run_on = 1;
        gtk_label_set(GTK_LABEL(run_label), "Run");
        return;
    }

    present_flags = 0;
    if (cpl.run_on) {
        present_flags |= KEYF_MOD_CTRL;
    }
    if (cpl.fire_on) {
        present_flags |= KEYF_MOD_SHIFT;
    }
    if (cpl.alt_on) {
        present_flags |= KEYF_MOD_ALT;
    }
    if (cpl.meta_on) {
        present_flags |= KEYF_MOD_META;
    }

    kb = keybind_find(keysym, present_flags, 0); /* char scope */
    if (kb == NULL)
        kb = keybind_find(keysym, present_flags, 1); /* global scope */
    if (kb != NULL) {
        if (kb->flags & KEYF_EDIT) {
            strcpy(cpl.input_text, kb->command);
            cpl.input_state = Command_Mode;
            gtk_entry_set_text(GTK_ENTRY(entry_commands), cpl.input_text);
            gtk_widget_grab_focus(GTK_WIDGET(entry_commands));
            gtk_editable_select_region(GTK_EDITABLE(entry_commands), 0, 0);
            gtk_editable_set_position(GTK_EDITABLE(entry_commands), -1);
            return;
        }

        /* Some spells (dimension door) need a valid count value */
        cpl.count = gtk_spin_button_get_value_as_int(
                                   GTK_SPIN_BUTTON(spinbutton_count));

        if (kb->direction >= 0) {
            if (cpl.fire_on) {
                snprintf(buf, sizeof(buf), "fire %s", kb->command);
                fire_dir(kb->direction);
            } else if (cpl.run_on) {
                snprintf(buf, sizeof(buf), "run %s", kb->command);
                run_dir(kb->direction);
            } else {
                extended_command(kb->command);
            }
            if (use_config[CONFIG_ECHO])
                draw_ext_info(
                    NDI_BLACK, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_NOTICE,
                    kb->command);
        } else {
            if (use_config[CONFIG_ECHO])
                draw_ext_info(
                    NDI_BLACK, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_NOTICE,
                    kb->command);
            extended_command(kb->command);
        }
        return;
    }

    if (key >= '0' && key <= '9') {
        cpl.count = cpl.count * 10 + (key - '0');
        if (cpl.count > 100000) {
            cpl.count %= 100000;
        }
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinbutton_count), (float) cpl.count);
        return;
    }
    tmpbuf[0] = 0;
    if (cpl.fire_on) {
        strcat(tmpbuf, "fire+");
    }
    if (cpl.run_on) {
        strcat(tmpbuf, "run+");
    }
    if (cpl.alt_on) {
        strcat(tmpbuf, "alt+");
    }
    if (cpl.meta_on) {
        strcat(tmpbuf, "meta+");
    }

    snprintf(buf, sizeof(buf),
             "Key %s%s is not bound to any command. Use 'bind' to associate this keypress with a command",
             tmpbuf, keysym == NoSymbol ? "unknown" : gdk_keyval_name(keysym));
#ifdef WIN32
    if ((65513 != keysym) && (65511 != keysym))
#endif
        draw_ext_info(NDI_BLACK, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_NOTICE, buf);
    cpl.count = 0;
}

static void get_key_modchars(struct keybind *kb, int save_mode, char *buf)
{
    int bi = 0;

    if (kb->flags & KEYF_ANY) {
        buf[bi++] = 'A';
    }
    if (save_mode || !(kb->flags & KEYF_ANY)) {
        if ((kb->flags & KEYF_MOD_MASK) == 0) {
            buf[bi++] = 'N';
        }
        if (kb->flags & KEYF_MOD_SHIFT) {
            buf[bi++] = 'F';
        }
        if (kb->flags & KEYF_MOD_CTRL) {
            buf[bi++] = 'R';
        }
        if (kb->flags & KEYF_MOD_ALT) {
            buf[bi++] = 'L';
        }
        if (kb->flags & KEYF_MOD_META) {
            buf[bi++] = 'M';
        }
    }
    if (kb->flags & KEYF_EDIT) {
        buf[bi++] = 'E';
    }

    buf[bi] = '\0';
}

/**
 *
 * @param key
 * @param save_mode If true, it means that the format used for saving the
 * information is used, instead of the usual format for displaying the
 * information in a friendly manner.
 * @return A character string describing the key.
 */
static char *get_key_info(struct keybind *kb, int save_mode)
{
    /* bind buf is the maximum space allowed for a
     * bound command. We will add additional data to
     * it so we increase its size by MAX_BUF*/
    static char buf[MAX_BUF + sizeof(bind_buf)];

    char buff[MAX_BUF];

    get_key_modchars(kb, save_mode, buff);

    if (save_mode) {
        if (kb->keysym == NoSymbol) {
            snprintf(buf, sizeof(buf), "(null) %i %s %s",
                     0, buff, kb->command);
        } else {
            snprintf(buf, sizeof(buf), "%s %i %s %s",
                     gdk_keyval_name(kb->keysym),
                     0, buff, kb->command);
        }
    } else {
        if (kb->keysym == NoSymbol) {
            snprintf(buf, sizeof(buf), "key (null) %s %s",
                     buff, kb->command);
        } else {
            snprintf(buf, sizeof(buf), "key %s %s %s",
                     gdk_keyval_name(kb->keysym),
                     buff, kb->command);
        }
    }
    return buf;
}

/**
 * Shows all the keybindings.
 *
 * @param allbindings Also shows the standard (default) keybindings.
 */
static void show_keys(void)
{
    int i, j, count = 1;
    struct keybind *kb;
    char buf[MAX_BUF];

    snprintf(buf, sizeof(buf), "Commandkey %s",
             commandkeysym == NoSymbol ? "unknown" : gdk_keyval_name(commandkeysym));
    draw_ext_info(NDI_BLACK, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_NOTICE, buf);

    snprintf(buf, sizeof(buf), "Firekeys 1: %s, 2: %s",
             firekeysym[0] == NoSymbol ? "unknown" : gdk_keyval_name(firekeysym[0]),
             firekeysym[1] == NoSymbol ? "unknown" : gdk_keyval_name(firekeysym[1]));
    draw_ext_info(NDI_BLACK, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_NOTICE, buf);

    snprintf(buf, sizeof(buf), "Altkeys 1: %s, 2: %s",
             altkeysym[0] == NoSymbol ? "unknown" : gdk_keyval_name(altkeysym[0]),
             altkeysym[1] == NoSymbol ? "unknown" : gdk_keyval_name(altkeysym[1]));
    draw_ext_info(NDI_BLACK, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_NOTICE, buf);

    snprintf(buf, sizeof(buf), "Metakeys 1: %s, 2: %s",
             metakeysym[0] == NoSymbol ? "unknown" : gdk_keyval_name(metakeysym[0]),
             metakeysym[1] == NoSymbol ? "unknown" : gdk_keyval_name(metakeysym[1]));
    draw_ext_info(NDI_BLACK, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_NOTICE, buf);

    snprintf(buf, sizeof(buf), "Runkeys 1: %s, 2: %s",
             runkeysym[0] == NoSymbol ? "unknown" : gdk_keyval_name(runkeysym[0]),
             runkeysym[1] == NoSymbol ? "unknown" : gdk_keyval_name(runkeysym[1]));
    draw_ext_info(NDI_BLACK, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_NOTICE, buf);

    snprintf(buf, sizeof(buf), "Command Completion Key %s",
             completekeysym == NoSymbol ? "unknown" : gdk_keyval_name(completekeysym));
    draw_ext_info(NDI_BLACK, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_NOTICE, buf);

    snprintf(buf, sizeof(buf), "Next Command in History Key %s",
             nextkeysym == NoSymbol ? "unknown" : gdk_keyval_name(nextkeysym));
    draw_ext_info(NDI_BLACK, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_NOTICE, buf);

    snprintf(buf, sizeof(buf), "Previous Command in History Key %s",
             prevkeysym == NoSymbol ? "unknown" : gdk_keyval_name(prevkeysym));
    draw_ext_info(NDI_BLACK, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_NOTICE, buf);

    /*
     * Perhaps we should start at 8, so that we only show 'active' keybindings?
     */
    for (i = 0; i < KEYHASH; i++) {
        for (j=0; j<2; j++) {
            for (kb=(j==0)?keys_global[i]:keys_char[i]; kb != NULL; kb = kb->next) {
                snprintf(buf, sizeof(buf), "%3d %s", count, get_key_info(kb, 0));
                draw_ext_info(
                    NDI_BLACK, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_NOTICE, buf);
                count++;
            }
        }
    }
}

/**
 * Implements the "bind" command when entered as a text command.  It parses the
 * command options, records the command to bind, then prompts the user to press
 * a key to bind.  It also shows help for the bind command if the player types
 * bind with no parameters.
 *
 * @param params If null, show bind command help in the message pane.
 */
void bind_key(char *params)
{
    char buf[MAX_BUF + 16];

    if (!params) {
        draw_ext_info(NDI_BLACK, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_NOTICE,
"Usage: 'bind -ei {<commandline>,commandkey,firekey{1,2},runkey{1,2},altkey{1,2},metakey{1,2},completekey,nextkey,prevkey}'\n"
"Where\n"
"      -e means enter edit mode\n"
"      -g means this binding should be global (used for all your characters)\n"
"      -i means ignore modifier keys (keybinding works no matter if Ctrl/Alt etc are held)");
        return;
    }

    /* Skip over any spaces we may have */
    while (*params == ' ') {
        params++;
    }

    if (!strcmp(params, "commandkey")) {
        bind_keysym = &commandkeysym;
        draw_ext_info(NDI_BLACK, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_QUERY,
                      "Push key to bind new commandkey.");
        cpl.input_state = Configure_Keys;
        return;
    }

    if (!strcmp(params, "firekey1")) {
        bind_keysym = &firekeysym[0];
        draw_ext_info(NDI_BLACK, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_QUERY,
                      "Push key to bind new firekey 1.");
        cpl.input_state = Configure_Keys;
        return;
    }
    if (!strcmp(params, "firekey2")) {
        bind_keysym = &firekeysym[1];
        draw_ext_info(NDI_BLACK, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_QUERY,
                      "Push key to bind new firekey 2.");
        cpl.input_state = Configure_Keys;
        return;
    }
    if (!strcmp(params, "metakey1")) {
        bind_keysym = &metakeysym[0];
        draw_ext_info(NDI_BLACK, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_QUERY,
                      "Push key to bind new metakey 1.");
        cpl.input_state = Configure_Keys;
        return;
    }
    if (!strcmp(params, "metakey2")) {
        bind_keysym = &metakeysym[1];
        draw_ext_info(NDI_BLACK, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_QUERY,
                      "Push key to bind new metakey 2.");
        cpl.input_state = Configure_Keys;
        return;
    }
    if (!strcmp(params, "altkey1")) {
        bind_keysym = &altkeysym[0];
        draw_ext_info(NDI_BLACK, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_QUERY,
                      "Push key to bind new altkey 1.");
        cpl.input_state = Configure_Keys;
        return;
    }
    if (!strcmp(params, "altkey2")) {
        bind_keysym = &altkeysym[1];
        draw_ext_info(NDI_BLACK, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_QUERY,
                      "Push key to bind new altkey 2.");
        cpl.input_state = Configure_Keys;
        return;
    }
    if (!strcmp(params, "runkey1")) {
        bind_keysym = &runkeysym[0];
        draw_ext_info(NDI_BLACK, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_QUERY,
                      "Push key to bind new runkey 1.");
        cpl.input_state = Configure_Keys;
        return;
    }

    if (!strcmp(params, "runkey2")) {
        bind_keysym = &runkeysym[1];
        draw_ext_info(NDI_BLACK, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_QUERY,
                      "Push key to bind new runkey 2.");
        cpl.input_state = Configure_Keys;
        return;
    }

    if (!strcmp(params, "completekey")) {
        bind_keysym = &completekeysym;
        draw_ext_info(NDI_BLACK, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_QUERY,
                      "Push key to bind new command completion key");
        cpl.input_state = Configure_Keys;
        return;
    }

    if (!strcmp(params, "prevkey")) {
        bind_keysym = &prevkeysym;
        draw_ext_info(NDI_BLACK, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_QUERY,
                      "Push key to bind new previous command in history key.");
        cpl.input_state = Configure_Keys;
        return;
    }

    if (!strcmp(params, "nextkey")) {
        bind_keysym = &nextkeysym;
        draw_ext_info(NDI_BLACK, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_QUERY,
                      "Push key to bind new next command in history key.");
        cpl.input_state = Configure_Keys;
        return;
    }
    bind_keysym = NULL;

    bind_flags = 0;
    if (params[0] == '-') {
        for (params++; *params != ' '; params++)
            switch (*params) {
            case 'e':
                bind_flags |= KEYF_EDIT;
                break;
            case 'i':
                bind_flags |= KEYF_ANY;
                break;
            case 'g':
                bind_flags |= KEYF_R_GLOBAL;
                break;
            case '\0':
                draw_ext_info(
                    NDI_BLACK, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_NOTICE,
                    "Use unbind to remove bindings.");
                return;
            default:
                snprintf(buf, sizeof(buf),
                         "Unsupported or invalid bind flag: '%c'", *params);
                draw_ext_info(NDI_BLACK, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_ERROR, buf);
                    return;
            }
        params++;
    }

    if (!params[0]) {
        draw_ext_info(NDI_BLACK, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_NOTICE,
                      "Use unbind to remove bindings.");
        return;
    }

    if (strlen(params) >= sizeof(bind_buf)) {
        params[sizeof(bind_buf) - 1] = '\0';
        draw_ext_info(NDI_RED, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_ERROR,
                      "Keybinding too long! Truncated:");
        draw_ext_info(NDI_RED, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_ERROR, params);
    }
    snprintf(buf, sizeof(buf), "Push key to bind '%s'.", params);
    draw_ext_info(NDI_BLACK, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_QUERY, buf);

    strcpy(bind_buf, params);
    cpl.input_state = Configure_Keys;
    return;
}

/**
 * A recursive function that saves all the entries for a particular entry.  We
 * save the first element first, and then go through and save the rest of the
 * elements.  In this way, the ordering of the key entries in the file remains
 * the same.
 *
 * @param fp  Pointer to an open file for writing key bind settings into.
 * @param key Pointer of a key hash record to save to the key bind file.
 *            During recursion, key takes the value key->next, and then
 *            returns when it becomes a NULL pointer.
 * @param kc
 */
static void save_individual_key(FILE *fp, struct keybind *kb, KeyCode kc)
{
    while (kb) {
        fprintf(fp, "%s\n", get_key_info(kb, 1));
        kb = kb->next;
    }
}

/**
 * Saves the keybindings into the user's .crossfire/keys file.  The output
 * file is opened, then the special shift/modifier keys are written first.
 * Next, the entire key hash is traversed and the contents of each slot is
 * dumped to the file, and the output file is closed.  Success or failure is
 * reported to the message pane.
 */
static void save_keys(void)
{
    char buf[MAX_BUF], buf2[MAX_BUF];
    int i;
    FILE *fp;

    /* If we are logged in open file to save character specific bindings */
    if (cpl.name) {
        snprintf(buf, sizeof(buf), "%s/.crossfire/%s.%s.keys",
                 getenv("HOME"), csocket.servername, cpl.name);
        CONVERT_FILESPEC_TO_OS_FORMAT(buf);
        LOG(LOG_INFO, "gtk-v2::save_keys",
            "Saving character specific keybindings to %s", buf);

        if (make_path_to_file(buf) == -1)
            LOG(LOG_WARNING, "gtk-v2::save_keys", "Could not create %s", buf);

        fp = fopen(buf, "w");
        if (fp == NULL) {
            snprintf(buf2, sizeof(buf2),
                     "Could not open %s, character bindings not saved\n", buf);
            draw_ext_info(NDI_BLACK, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_ERROR, buf2);
        }
        else {
            for (i = 0; i < KEYHASH; i++)
                save_individual_key(fp, keys_char[i], 0);
            fclose(fp);
        }
    }

    /* Open file to save global user bindings */
    snprintf(buf, sizeof(buf), "%s/.crossfire/keys", getenv("HOME"));
    CONVERT_FILESPEC_TO_OS_FORMAT(buf);
    LOG(LOG_INFO, "gtk-v2::save_keys",
        "Saving global user's keybindings to %s", buf);

    if (make_path_to_file(buf) == -1)
        LOG(LOG_WARNING, "gtk-v2::save_keys", "Could not create %s", buf);
    else {
        fp = fopen(buf, "w");
        if (fp == NULL) {
            snprintf(buf2, sizeof(buf2),
                     "Could not open %s, global key bindings not saved\n", buf);
            draw_ext_info(NDI_BLACK, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_ERROR, buf2);
        }
        else {
            /* Save default bindings as part of the global scope */
            if (firekeysym[0] != GDK_Shift_L && firekeysym[0] != NoSymbol)
                fprintf(fp, "! firekey0 %s %d\n",
                        gdk_keyval_name(firekeysym[0]), 0);
            if (firekeysym[1] != GDK_Shift_R && firekeysym[1] != NoSymbol)
                fprintf(fp, "! firekey1 %s %d\n",
                        gdk_keyval_name(firekeysym[1]), 0);
            if (metakeysym[0] != GDK_Shift_L && metakeysym[0] != NoSymbol)
                fprintf(fp, "! metakey0 %s %d\n",
                        gdk_keyval_name(metakeysym[0]), 0);
            if (metakeysym[1] != GDK_Shift_R && metakeysym[1] != NoSymbol)
                fprintf(fp, "! metakey1 %s %d\n",
                        gdk_keyval_name(metakeysym[1]), 0);
            if (altkeysym[0] != GDK_Shift_L && altkeysym[0] != NoSymbol)
                fprintf(fp, "! altkey0 %s %d\n",
                        gdk_keyval_name(altkeysym[0]), 0);
            if (altkeysym[1] != GDK_Shift_R && altkeysym[1] != NoSymbol)
                fprintf(fp, "! altkey1 %s %d\n",
                        gdk_keyval_name(altkeysym[1]), 0);
            if (runkeysym[0] != GDK_Control_L && runkeysym[0] != NoSymbol)
                fprintf(fp, "! runkey0 %s %d\n",
                        gdk_keyval_name(runkeysym[0]), 0);
            if (runkeysym[1] != GDK_Control_R && runkeysym[1] != NoSymbol)
                fprintf(fp, "! runkey1 %s %d\n",
                        gdk_keyval_name(runkeysym[1]), 0);
            if (completekeysym != GDK_Tab && completekeysym != NoSymbol)
                fprintf(fp, "! completekey %s %d\n",
                        gdk_keyval_name(completekeysym), 0);
            /* No defaults for these, so if it is set to anything, assume its valid */
            if (nextkeysym != NoSymbol)
                fprintf(fp, "! nextkey %s %d\n",
                        gdk_keyval_name(nextkeysym), 0);
            if (prevkeysym != NoSymbol)
                fprintf(fp, "! prevkey %s %d\n",
                        gdk_keyval_name(prevkeysym), 0);

            for (i = 0; i < KEYHASH; i++)
                save_individual_key(fp, keys_global[i], 0);
            fclose(fp);
        }
    }

    /* Should probably check return value on all writes to be sure, but... */
    draw_ext_info(NDI_BLACK, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_CONFIG,
                  "Key bindings saved.");
}

/**
 *
 * @param keysym
 */
static void configure_keys(uint32 keysym)
{
    char buf[MAX_BUF];
    struct keybind *kb;

    /* We handle the Shift key separately */
    keysym = gdk_keyval_to_lower(keysym);

    /*
     * I think that basically if we are not rebinding the special control keys
     * (in which case bind_keysym would be set to something) we just want to
     * handle these keypresses as normal events.
     */
    if (bind_keysym == NULL) {
        if (keysym == altkeysym[0] || keysym == altkeysym[1]) {
            cpl.alt_on = 1;
            return;
        }
        if (keysym == metakeysym[0] || keysym == metakeysym[1]) {
            cpl.meta_on = 1;
            return;
        }
        if (keysym == firekeysym[0] || keysym == firekeysym[1]) {
            cpl.fire_on = 1;
            draw_message_window(0);
            return;
        }
        if (keysym == runkeysym[0] || keysym == runkeysym[1]) {
            cpl.run_on = 1;
            draw_message_window(0);
            return;
        }
    }

    /*
     * Take shift/control keys into account when binding keys.
     */
    if (!(bind_flags & KEYF_ANY)) {
        if (cpl.fire_on) {
            bind_flags |= KEYF_MOD_SHIFT;
        }
        if (cpl.run_on) {
            bind_flags |= KEYF_MOD_CTRL;
        }
        if (cpl.meta_on) {
            bind_flags |= KEYF_MOD_META;
        }
        if (cpl.alt_on) {
            bind_flags |= KEYF_MOD_ALT;
        }
    }

    /* Reset state now. We might return early if bind fails. */
    cpl.input_state = Playing;

    if (bind_keysym != NULL) {
        *bind_keysym = keysym;
        bind_keysym = NULL;
    } else {
        kb = keybind_find(keysym, bind_flags, (bind_flags & KEYF_R_GLOBAL));
        if (kb) {
            snprintf(buf, sizeof(buf),
                     "Error: Key already used for command \"%s\". Use unbind first.",
                     kb->command);
            draw_ext_info(
                NDI_RED, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_ERROR, buf);
            return;
        }
        else
            keybind_insert(keysym, bind_flags, bind_buf);
    }

    snprintf(buf, sizeof(buf), "Bound to key '%s' (%i)",
             keysym == NoSymbol ? "unknown" : gdk_keyval_name(keysym), keysym);
    draw_ext_info(NDI_BLACK, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_CONFIG, buf);
    draw_message_window(0);

    /*
     * Do this each time a new key is bound.  This way, we are never actually
     * storing any information that needs to be saved when the connection dies
     * or the player quits.
     */
    save_keys();
    return;
}

/**
 * Show help for the unbind command in the message pane.
 */
static void unbind_usage(void)
{
    draw_ext_info(NDI_BLACK, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_NOTICE,
                  "Usage: 'unbind <entry_number>' or");
    draw_ext_info(NDI_BLACK, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_NOTICE,
                  "Usage: 'unbind' to show existing bindings");
}

/**
 *
 * @param params
 */
void unbind_key(const char *params)
{
    int count = 0, keyentry, slot, j;
    int res;
    struct keybind *kb;
    char buf[MAX_BUF];

    if (params == NULL) {
        show_keys();
        return;
    }

    /* Skip over any spaces we may have */
    while (*params == ' ') {
        params++;
    }

    if (params[0] == '\0') {
        show_keys();
        return;
    }

    if ((keyentry = atoi(params)) == 0) {
        unbind_usage();
        return;
    }

    for (slot = 0; slot < KEYHASH; slot++) {
        for (j = 0; j < 2; j++) {
            for (kb=(j==0)?keys_global[slot]:keys_char[slot]; kb != NULL; kb=kb->next) {
                count++;

                if (keyentry == count) {
                    /* We found the key we want to unbind */
                    snprintf(buf, sizeof(buf), "Removing binding: %3d %s",
                             count, get_key_info(kb, 0));
                    draw_ext_info(NDI_BLACK, MSG_TYPE_CLIENT,
                                  MSG_TYPE_CLIENT_CONFIG, buf);
                    res = keybind_remove(kb);
                    if (res < 0)
                        LOG(LOG_ERROR, "gtk-v2::unbind_key",
                            "found number entry, but could not find actual key");
                    keybind_free(&kb);
                    save_keys();
                    return;
                }
            }
        }
    }

    /* Not found */
    /* Makes things look better to draw the blank line */
    draw_ext_info(NDI_BLACK, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_NOTICE, "");
    draw_ext_info(NDI_BLACK, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_NOTICE,
                  "Not found. Try 'unbind' with no options to find entry.");
    return;
}

/**
 * When the main window looses its focus, act as if all keys have been released
 */
void focusoutfunc(GtkWidget *widget, GdkEventKey *event, GtkWidget *window)
{
    if (cpl.fire_on == 1) {
        cpl.fire_on = 0;
        clear_fire();
        gtk_label_set(GTK_LABEL(fire_label), "    ");
    }
    if (cpl.run_on == 1) {
        cpl.run_on = 0;
        if (use_config[CONFIG_ECHO])
            draw_ext_info(NDI_BLACK, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_NOTICE,
                          "stop run");
        clear_run();
        gtk_label_set(GTK_LABEL(run_label), "   ");
    }
    if (cpl.alt_on == 1) {
        cpl.alt_on = 0;
    }
    if (cpl.meta_on == 1) {
        cpl.meta_on = 0;
    }
}

/**
 * GTK callback function used to handle client key release events.
 *
 * @param widget
 * @param event  GDK Key Release Event
 * @param window
 */
void keyrelfunc(GtkWidget *widget, GdkEventKey *event, GtkWidget *window)
{
    if (event->keyval > 0 && !GTK_WIDGET_HAS_FOCUS(entry_commands)) {
        parse_key_release(event->keyval);
    }
    g_signal_stop_emission_by_name(GTK_OBJECT(window), "key_release_event");
}

/**
 * GTK Callback function used to handle client key press events.
 *
 * @param widget
 * @param event  GDK Key Press Event
 * @param window
 */
void keyfunc(GtkWidget *widget, GdkEventKey *event, GtkWidget *window)
{
    char *text;

    if (!use_config[CONFIG_POPUPS]) {
        if ( ((cpl.input_state == Reply_One) || (cpl.input_state == Reply_Many))
                && (event->keyval == cancelkeysym) ) {

            /*
             * Player hit cancel button during input. Disconnect it (code from
             * menubar)
             */

            close_server_connection();

            if (csocket_fd) {
                gdk_input_remove(csocket_fd);
                csocket_fd = 0;
                gtk_main_quit();
            }
            g_signal_stop_emission_by_name(
                GTK_OBJECT(window), "key_press_event");
            return;
        }
        if  (cpl.input_state == Reply_One) {
            text = gdk_keyval_name(event->keyval);
            send_reply(text);
            cpl.input_state = Playing;
            g_signal_stop_emission_by_name(
                GTK_OBJECT(window), "key_press_event");
            return;
        } else if (cpl.input_state == Reply_Many) {
            if (GTK_WIDGET_HAS_FOCUS (entry_commands)) {
                gtk_widget_event(GTK_WIDGET(entry_commands), (GdkEvent*)event);
            } else {
                gtk_widget_grab_focus(GTK_WIDGET(entry_commands));
            }
            g_signal_stop_emission_by_name(
                GTK_OBJECT(window), "key_press_event");
            return;
        }
    }
    /*
     * Better check for really weirdo keys, X doesnt like keyval 0 so avoid
     * handling these key values.
     */
    if (event->keyval > 0) {
        if (GTK_WIDGET_HAS_FOCUS(entry_commands)) {
            if (event->keyval == completekeysym) {
                gtk_complete_command();
            }
            if (event->keyval == prevkeysym || event->keyval == nextkeysym) {
                gtk_command_history(event->keyval == nextkeysym?0:1);
            } else {
                gtk_widget_event(GTK_WIDGET(entry_commands), (GdkEvent*)event);
            }
        } else {
            switch(cpl.input_state) {
            case Playing:
                /*
                 * Specials - do command history - many times, the player
                 * will want to go the previous command when nothing is
                 * entered in the command window.
                 */
                if ((event->keyval == prevkeysym)
                        || (event->keyval == nextkeysym)) {
                    gtk_command_history(event->keyval == nextkeysym ? 0 : 1);
                } else {
                    if (cpl.run_on) {
                        if (!(event->state & GDK_CONTROL_MASK)) {
                            /* printf("Run is on while ctrl is not\n"); */
                            gtk_label_set(GTK_LABEL(run_label), "   ");
                            cpl.run_on = 0;
                            stop_run();
                        }
                    }
                    if (cpl.fire_on) {
                        if (!(event->state & GDK_SHIFT_MASK)) {
                            /* printf("Fire is on while shift is not\n");*/
                            gtk_label_set(GTK_LABEL(fire_label), "   ");
                            cpl.fire_on = 0;
                            stop_fire();
                        }
                    }

                    if ( (event->state & GDK_CONTROL_MASK)
                            && (event->state & GDK_SHIFT_MASK)
                            && (event->keyval == GDK_i || event->keyval == GDK_I) ) {
                        reset_map();
                    }

                    parse_key(event->string[0], event->keyval);
                }
                break;

            case Configure_Keys:
                configure_keys(event->keyval);
                break;

            case Command_Mode:
                if (event->keyval == completekeysym) {
                    gtk_complete_command();
                }
                if ((event->keyval == prevkeysym)
                        || (event->keyval == nextkeysym)) {
                    gtk_command_history(event->keyval == nextkeysym ? 0 : 1);
                } else {
                    gtk_widget_grab_focus(GTK_WIDGET(entry_commands));
                    /*
                     * When running in split windows mode, entry_commands
                     * can't get focus because it is in a different
                     * window.  So we have to pass the event to it
                     * explicitly.
                     */
                    if (GTK_WIDGET_HAS_FOCUS(entry_commands) == 0)
                        gtk_widget_event(
                            GTK_WIDGET(entry_commands), (GdkEvent*)event);
                }
                /*
                 * Don't pass signal along to default handlers -
                 * otherwise, we get get crashes in the clist area (gtk
                 * fault I believe)
                 */
                break;

            case Metaserver_Select:
                gtk_widget_grab_focus(GTK_WIDGET(entry_commands));
                break;

            default:
                LOG(LOG_ERROR, "gtk-v2::keyfunc",
                    "Unknown input state: %d", cpl.input_state);
            }
        }
    }
    g_signal_stop_emission_by_name(
        GTK_OBJECT(window), "key_press_event");
}

/**
 * Output the current list of keybindings to the message pane.
 *
 * @param keylist
 */
void draw_keybindings(GtkWidget *keylist)
{
    int i, j, count = 1;
    struct keybind *kb;
    char buff[MAX_BUF];
    char buffer[5][MAX_BUF];
    char *buffers[5];

    gtk_clist_clear(GTK_CLIST(keylist));
    for (i = 0; i < KEYHASH; i++) {
        for (j = 0; j < 2; j++) {
            for (kb=(j==0)?keys_global[i]:keys_char[i]; kb != NULL; kb = kb->next) {
                get_key_modchars(kb, 0, buff);

                if (kb->keysym != NoSymbol) {
                    snprintf(buffer[0], sizeof(buffer[0]), "%i", count);
                    snprintf(buffer[1], sizeof(buffer[1]), "%s", gdk_keyval_name(kb->keysym));
                    snprintf(buffer[2], sizeof(buffer[2]), "%i", i);
                    snprintf(buffer[3], sizeof(buffer[3]), "%s", buff);
                    snprintf(buffer[4], sizeof(buffer[4]), "%s", kb->command);
                    buffers[0] = buffer[0];
                    buffers[1] = buffer[1];
                    buffers[2] = buffer[2];
                    buffers[3] = buffer[3];
                    buffers[4] = buffer[4];
                    gtk_clist_append(GTK_CLIST(keylist), buffers);
                }
                count++;
            }
        }
    }
}

/**
 *
 */
void x_set_echo(void)
{
    gtk_entry_set_visibility(GTK_ENTRY(entry_commands), !cpl.no_echo);
}

/**
 * Draws a prompt.  Don't deal with popups for the time being.
 *
 * @param str
 */
void draw_prompt(const char *str)
{
    draw_ext_info(NDI_WHITE, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_QUERY, str);
    gtk_widget_grab_focus(GTK_WIDGET(entry_commands));
}

/**
 * Deals with command history.
 *
 * @param direction If 0, we are going backwards, if 1, we are moving forward.
 */
void gtk_command_history(int direction)
{
    int i = scroll_history_position;
    if (direction) {
        i--;
        if (i < 0) {
            i += MAX_HISTORY;
        }
        if (i == cur_history_position) {
            return;
        }
    } else {
        i++;
        if (i >= MAX_HISTORY) {
            i = 0;
        }
        if (i == cur_history_position) {
            /*
             * User has forwarded to what should be current entry - reset it
             * now.
             */
            gtk_entry_set_text(GTK_ENTRY(entry_commands), "");
            gtk_entry_set_position(GTK_ENTRY(entry_commands), 0);
            scroll_history_position = cur_history_position;
            return;
        }
    }

    if (history[i][0] == 0) {
        return;
    }

    scroll_history_position=i;
    /*  fprintf(stderr, "resetting postion to %d, data = %s\n", i, history[i]);*/
    gtk_entry_set_text(GTK_ENTRY(entry_commands), history[i]);
    gtk_widget_grab_focus(GTK_WIDGET(entry_commands));
    gtk_editable_select_region(GTK_EDITABLE(entry_commands), 0, 0);
    gtk_editable_set_position(GTK_EDITABLE(entry_commands), -1);

    cpl.input_state = Command_Mode;
}

/**
 * Executes when the TAB key is pressed while the command input box has focus
 * to give hints on what commands begin with the text already entered to this
 * point. It is almost like tab completion, except for the completion.  The TAB
 * key is also known by GDK_Tab, completekey, or completekeysym.
 */
void gtk_complete_command(void)
{
    const gchar *entry_text, *newcommand;

    entry_text = gtk_entry_get_text(GTK_ENTRY(entry_commands));
    newcommand = complete_command(entry_text);
    /* value differ, so update window */
    if (newcommand != NULL) {
        gtk_entry_set_text(GTK_ENTRY(entry_commands), newcommand);
        gtk_widget_grab_focus(GTK_WIDGET(entry_commands));
        gtk_editable_select_region(GTK_EDITABLE(entry_commands), 0, 0);
        gtk_editable_set_position(GTK_EDITABLE(entry_commands), -1);
    }
}

/**
 * Used to process keyboard input whenever the player types commands into the
 * command entry box.
 *
 * @param entry
 * @param user_data
 */
void on_entry_commands_activate(GtkEntry *entry, gpointer user_data)
{
    const gchar *entry_text;
    extern GtkWidget *treeview_look;

    /* Next reply will reset this as necessary */
    if (!use_config[CONFIG_POPUPS]) {
        gtk_entry_set_visibility(GTK_ENTRY(entry), TRUE);
    }

    entry_text = gtk_entry_get_text(GTK_ENTRY(entry));

    if (cpl.input_state == Metaserver_Select) {
        strcpy(cpl.input_text, entry_text);
    } else if (cpl.input_state == Reply_One || cpl.input_state == Reply_Many) {
        cpl.input_state = Playing;
        strcpy(cpl.input_text, entry_text);
        if (cpl.input_state == Reply_One) {
            cpl.input_text[1] = 0;
        }

        send_reply(cpl.input_text);

    } else {
        cpl.input_state = Playing;
        /* Some spells (dimension door) need a valid count value */
        cpl.count = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spinbutton_count));

        /* No reason to do anything for a null string */
        if (entry_text[0] != 0) {
            strncpy(history[cur_history_position], entry_text, MAX_COMMAND_LEN);
            history[cur_history_position][MAX_COMMAND_LEN - 1] = 0;
            cur_history_position++;
            cur_history_position %= MAX_HISTORY;
            scroll_history_position = cur_history_position;
            extended_command(entry_text);
        }
    }
    gtk_entry_set_text(GTK_ENTRY(entry), "");

    /*
     * This grab focus is really just so the entry box doesn't have focus -
     * this way, keypresses are used to play the game, and not as stuff that
     * goes into the entry box.  It doesn't make much difference what widget
     * this is set to, as long as it is something that can get focus.
     */
    gtk_widget_grab_focus(GTK_WIDGET(treeview_look));

    if( cpl.input_state == Metaserver_Select) {
        cpl.input_state = Playing;
        /*
         * This is the gtk_main that is started up by get_metaserver The client
         * will start another one once it is connected to a crossfire server
         */
        gtk_main_quit();
    }
}

/**
 * @} EndOf GtkV2KeyBinding
 */

/**
 * @defgroup GtkV2KeyBindingWindow GTK-V2 client keybinding window functions.
 * @{
 */

/**
 * Update the keybinding dialog to reflect the current state of the keys file.
 */
void update_keybinding_list(void)
{
    int i, j;
    struct keybind *kb;
    char *modifier_label, *scope_label;
    GtkTreeIter iter;

    gtk_list_store_clear(keybinding_store);
    for (i = 0; i < KEYHASH; i++) {
        for (j = 0; j < 2; j++) {
            for (kb=(j==0)?keys_global[i]:keys_char[i]; kb != NULL; kb = kb->next) {
                if (j==0)
                    kb->flags |= KEYF_R_GLOBAL;
                else
                    kb->flags |= KEYF_R_CHAR;

                if (kb->flags & KEYF_ANY) {
                    modifier_label = "Any";
                } else if ((kb->flags & KEYF_MOD_MASK) == 0) {
                    modifier_label = "None";
                } else {
                    if (kb->flags & KEYF_MOD_ALT) {
                        modifier_label = "Alt";
                        if (kb->flags & (KEYF_MOD_SHIFT | KEYF_MOD_CTRL | KEYF_MOD_META))
                            modifier_label = " + ";
                    }
                    if (kb->flags & KEYF_MOD_SHIFT) {
                        modifier_label = "Fire";
                        if (kb->flags & (KEYF_MOD_CTRL | KEYF_MOD_META))
                            modifier_label = " + ";
                    }
                    if (kb->flags & KEYF_MOD_CTRL) {
                        modifier_label = "Run";
                        if (kb->flags & KEYF_MOD_META)
                            modifier_label = " + ";
                    }
                    if (kb->flags & KEYF_MOD_META) {
                        modifier_label = "Meta";
                    }
                }
                if (!(kb->flags & KEYF_R_GLOBAL)) {
                    scope_label = " char ";
                } else {
                    scope_label = "global";
                }
                gtk_list_store_append(keybinding_store, &iter);
                gtk_list_store_set(keybinding_store, &iter,
                                   KLIST_ENTRY, i,
                                   KLIST_KEY, gdk_keyval_name(kb->keysym),
                                   KLIST_MODS, modifier_label,
                                   KLIST_SCOPE, scope_label,
                                   KLIST_EDIT, (kb->flags & KEYF_EDIT) ? "Yes":"No",
                                   KLIST_COMMAND, kb->command,
                                   KLIST_KEY_ENTRY, kb,
                                   -1);
            }
        }
    }
    reset_keybinding_status();
}

/**
 * Menubar item to activate keybindings window
 *
 * @param menuitem
 * @param user_data
 */
void on_keybindings_activate(GtkMenuItem *menuitem, gpointer user_data)
{
    gtk_widget_show(keybinding_window);
    update_keybinding_list();
}

/**
 * Respond to a key press in the "Key" input box.  If the keyboard has modifier
 * keys pressed, set the appropriate "Keybinding Modifiers" checkboxes if the
 * shift or control keys happens to be pressed at the time.  Oddly, the Alt and
 * Meta keys are not similarly handled.  Checkboxes are never cleared here in
 * case the user had just set the checkboxes ahead of time.
 *
 * @param widget
 * @param event
 * @param user_data
 * @return TRUE (Returning TRUE prevents widget from getting this event.)
 */
gboolean
on_keybinding_entry_key_key_press_event(GtkWidget       *widget,
                                        GdkEventKey     *event,
                                        gpointer         user_data)
{
    gtk_entry_set_text(
        GTK_ENTRY(keybinding_entry_key), gdk_keyval_name(event->keyval));
    /*
     * This code is basically taken from the GTKv1 client.  However, at some
     * level it is broken, since the control/shift/etc masks are hardcoded, yet
     * we do let the users redefine them.
     *
     * The clearing of the modifiers is disabled.  In my basic testing, I
     * checked the modifiers and then pressed the key - have those modifiers go
     * away I think is less intuitive.
     */
    if (event->state & GDK_CONTROL_MASK)
        gtk_toggle_button_set_active(
            GTK_TOGGLE_BUTTON(keybinding_checkbutton_control), TRUE);

#if 0
    else
        gtk_toggle_button_set_active(
            GTK_TOGGLE_BUTTON(keybinding_checkbutton_control), FALSE);
#endif

    if (event->state & GDK_SHIFT_MASK)
        gtk_toggle_button_set_active(
            GTK_TOGGLE_BUTTON(keybinding_checkbutton_shift), TRUE);

#if 0
    else {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(keybinding_checkbutton_shift), FALSE);
    }

    /* The GDK_MOD_MASK* will likely correspond to alt and meta, yet there is
     * no way to be sure what goes to what, so easiest to just not allow them.
     */
    gtk_toggle_button_set_active(
        GTK_TOGGLE_BUTTON(keybinding_checkbutton_alt), FALSE);
    gtk_toggle_button_set_active(
        GTK_TOGGLE_BUTTON(keybinding_checkbutton_meta), FALSE);
#endif

    /* Returning TRUE prevents widget from getting this event */
    return TRUE;
}

/**
 * Toggles buttons state to reflect a scope state.
 * Both togglebuttons change accordingly.
 * @param scope - State to apply to the "All characters" togglebutton.
 *                The "This character" togglebutton will get the opposite state.
 */
void toggle_buttons_scope(int scope) {
    int state_u, state_c;

    state_u = gtk_toggle_button_get_active(
                  GTK_TOGGLE_BUTTON(kb_scope_togglebutton_global));
    state_c = gtk_toggle_button_get_active(
                  GTK_TOGGLE_BUTTON(kb_scope_togglebutton_character));
    /* If the states of the buttons are not already what we are asked for, or if
     * they are equal (which is inconsistent) then update them. Deactivate the
     * callbacks for the "toggled" events temporarily to avoid an infinite loop.
     */
    if (state_u != scope || state_u == state_c){
        g_signal_handlers_block_by_func(
            GTK_TOGGLE_BUTTON(kb_scope_togglebutton_character),
            G_CALLBACK(on_kb_scope_togglebutton_character_toggled), NULL);
        g_signal_handlers_block_by_func(
            GTK_TOGGLE_BUTTON(kb_scope_togglebutton_global),
            G_CALLBACK(on_kb_scope_togglebutton_global_toggled), NULL);

        gtk_toggle_button_set_active(
            GTK_TOGGLE_BUTTON(kb_scope_togglebutton_character), !scope);
        gtk_toggle_button_set_active(
            GTK_TOGGLE_BUTTON(kb_scope_togglebutton_global), scope);

        g_signal_handlers_unblock_by_func(
            GTK_TOGGLE_BUTTON(kb_scope_togglebutton_character),
            G_CALLBACK(on_kb_scope_togglebutton_character_toggled), NULL);
        g_signal_handlers_unblock_by_func(
            GTK_TOGGLE_BUTTON(kb_scope_togglebutton_global),
            G_CALLBACK(on_kb_scope_togglebutton_global_toggled), NULL);
    }
}

/**
 * Shows a dialog that prompts for confirmation before overwriting a keybind,
 * showing details of the keybind we are about to overwrite.
 * @param kb The keybind we are about to overwrite.
 *
 * @return TRUE if the user chooses to overwrite kb, else FALSE.
 */
static int keybind_overwrite_confirm(struct keybind *kb){
    GtkWidget *dialog, *label;
    int result;
    char buf[MAX_BUF], buf2[MAX_BUF];

    dialog = gtk_dialog_new_with_buttons(
            "Key already in use",
            GTK_WINDOW(keybinding_window),
            GTK_DIALOG_MODAL,
            GTK_STOCK_YES, 1,
            GTK_STOCK_NO, 2,
            NULL);
    get_key_modchars(kb, 1, buf2);
    snprintf(buf, sizeof(buf), "Overwrite this binding?\n  (%s) %s\n%s",
             buf2, gdk_keyval_name(kb->keysym), kb->command);
    label = gtk_label_new(buf);
    gtk_box_pack_start(
        GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),
        label, TRUE, TRUE, 0);
    gtk_widget_show_all(dialog);

    result = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    return (result == 1);
}

/**
 * Toggles a keybinding's scope to the desired value.
 * First checks for existance of another binding with the desired scope
 * and asks for confirmation before overwriting anything.
 *
 * @scope The new scope to apply to this keybind,
 *        0 meaning char scope, non zero meaning global scope.
 * @kb    Keybinding to modify scope.
 */
void toggle_keybind_scope(int scope, struct keybind *kb) {
    struct keybind *kb_old, **next_ptr;
    int ret, flags;
    char buf[MAX_BUF];

    /* First check for matching bindings in the new scope */
    kb_old = keybind_find(kb->keysym, kb->flags, scope);
    while (kb_old) {
        if (!keybind_overwrite_confirm(kb_old)) {
            /* Restore all bindings and buttons state.
             * Need to call keybindings_init() because we may have already
             * removed some bindings from memory */
            toggle_buttons_scope(!scope);
            keybindings_init(strdup(cpl.name));
            update_keybinding_list();
            return;
        }
        /* Completely remove the old binding */
        keybind_remove(kb_old);
        keybind_free(&kb_old);
        kb_old = keybind_find(kb->keysym, kb->flags, scope);
    }

    /* If the new scope is 'global' we remove the binding from keys_char (don't
     * free it), switch scope flags and rehash in keys_global.
     *
     * Else just make a copy into keys_char only switching the state flags.
     */
    if (scope) {
        if ((kb->flags & KEYF_R_GLOBAL)==0) {
            /* Remove kb from keys_char, don't free it. */
            ret = keybind_remove(kb);
            if (ret == -1) {
                draw_ext_info(NDI_BLACK, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_ERROR,
                          "\nCould not remove keybind. Operation failed.\n");
                toggle_buttons_scope(!scope);
                keybindings_init(strdup(cpl.name));
                update_keybinding_list();
                return;
            }
            /* Place the modified kb in keys_global */
            kb->flags ^= KEYF_R_CHAR;
            kb->flags |= KEYF_R_GLOBAL;
            next_ptr = &keys_global[kb->keysym % KEYHASH];
            kb->next = NULL;

            if (*next_ptr) {
                while ((*next_ptr)->next)
                    next_ptr = &(*next_ptr)->next;
                (*next_ptr)->next = kb;
            }
            else
                keys_global[kb->keysym % KEYHASH] = kb;
        }
    }
    else {
        if ((kb->flags & KEYF_R_GLOBAL)!=0) {
            /* Copy the selected binding in the char's scope with the right flags. */
            snprintf(buf, sizeof(buf), "%s", kb->command);
            flags = kb->flags;
            flags |= KEYF_R_CHAR;
            flags ^= KEYF_R_GLOBAL;
            keybind_insert(kb->keysym, flags, buf);
        }
    }
    save_keys();
    update_keybinding_list();
}

/**
 * Called when "This character" is clicked.
 * Toggles scope of the selected binding and handles the togglebuttons' state.
 *
 * @param toggle_button
 * @param user_data
 */
void on_kb_scope_togglebutton_character_toggled(GtkToggleButton *toggle_button, gpointer user_data)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    struct keybind *kb;
    gboolean scope;
    if (gtk_tree_selection_get_selected(keybinding_selection, &model, &iter)) {
        gtk_tree_model_get(model, &iter, KLIST_KEY_ENTRY, &kb, -1);
        scope = !gtk_toggle_button_get_active(
                    GTK_TOGGLE_BUTTON(kb_scope_togglebutton_character));
        toggle_buttons_scope(scope);
        toggle_keybind_scope(scope, kb);
    }
}

/**
 * Called when "All characters" is clicked.
 * Toggles scope of the selected binding and handles the togglebuttons' state.
 *
 * @param toggle_button
 * @param user_data
 */
void on_kb_scope_togglebutton_global_toggled(GtkToggleButton *toggle_button, gpointer user_data)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    struct keybind *kb;
    gboolean scope;
    if (gtk_tree_selection_get_selected(keybinding_selection, &model, &iter)) {
        gtk_tree_model_get(model, &iter, KLIST_KEY_ENTRY, &kb, -1);
        scope = gtk_toggle_button_get_active(
                    GTK_TOGGLE_BUTTON(kb_scope_togglebutton_global));
        toggle_buttons_scope(scope);
        toggle_keybind_scope(scope, kb);
    }
}

/**
 * Implements the "Remove Binding" button function that unbinds the currently
 * selected keybinding.
 *
 * @param button
 * @param user_data
 */
void on_keybinding_button_remove_clicked(GtkButton *button, gpointer user_data)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    struct keybind *kb;
    int res;

    if (!gtk_tree_selection_get_selected(keybinding_selection, &model, &iter)) {
        LOG(LOG_ERROR, "keys.c::on_keybinding_button_remove_clicked",
            "Function called with nothing selected\n");
        return;
    }
    gtk_tree_model_get(model, &iter, KLIST_KEY_ENTRY, &kb, -1);
    res = keybind_remove(kb);
    if (res < 0)
        LOG(LOG_ERROR, "keys.c::on_keybinding_button_remove_clicked",
            "Unable to find matching key entry\n");
    keybind_free(&kb);

    save_keys();
    update_keybinding_list();
}

/**
 * Gets the state information from checkboxes and other data in the window
 * and puts it in the variables passed.  This is used by both the update
 * and add functions.
 *
 * @param keysym
 * @param flags
 * @param command
 */
static void keybinding_get_data(uint32 *keysym, uint8 *flags, const char **command)
{
    static char bind_buf[MAX_BUF];
    const char *ed;
    *flags = 0;

    if (gtk_toggle_button_get_active(
                GTK_TOGGLE_BUTTON(keybinding_checkbutton_any))) {
        *flags |= KEYF_ANY;
    }

    if (gtk_toggle_button_get_active(
                GTK_TOGGLE_BUTTON(kb_scope_togglebutton_global)))
        *flags |= KEYF_R_GLOBAL;

    if (gtk_toggle_button_get_active(
                GTK_TOGGLE_BUTTON(keybinding_checkbutton_control))) {
        *flags |= KEYF_MOD_CTRL;
    }
    if (gtk_toggle_button_get_active(
                GTK_TOGGLE_BUTTON(keybinding_checkbutton_shift))) {
        *flags |= KEYF_MOD_SHIFT;
    }
    if (gtk_toggle_button_get_active(
                GTK_TOGGLE_BUTTON(keybinding_checkbutton_alt))) {
        *flags |= KEYF_MOD_ALT;
    }
    if (gtk_toggle_button_get_active(
                GTK_TOGGLE_BUTTON(keybinding_checkbutton_meta))) {
        *flags |= KEYF_MOD_META;
    }
    if (gtk_toggle_button_get_active(
                GTK_TOGGLE_BUTTON(keybinding_checkbutton_edit))) {
        *flags |= KEYF_EDIT;
    }

    ed = gtk_entry_get_text(GTK_ENTRY(keybinding_entry_command));
    if (strlen(ed) >= sizeof(bind_buf)) {
        draw_ext_info(NDI_RED, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_ERROR,
                      "Keybinding too long! Truncated.");
        strncpy(bind_buf, ed, MAX_BUF - 1);
        bind_buf[MAX_BUF - 1] = 0;
        *command = bind_buf;
    } else {
        *command = ed;
    }

    /*
     * This isn't ideal - when the key is pressed, we convert it to a string,
     * and now we are converting it back.  It'd be nice to tuck the keysym
     * itself away someplace.
     */
    *keysym = gdk_keyval_from_name(
                  gtk_entry_get_text(GTK_ENTRY(keybinding_entry_key)));
    if (*keysym == GDK_VoidSymbol) {
        LOG(LOG_ERROR, "keys.c::keybinding_get_data",
            "Cannot get valid keysym from selection");
    }
}

/**
 * Sets up a new binding when the "Add" button is clicked.
 *
 * @param button
 * @param user_data
 */
void on_keybinding_button_bind_clicked(GtkButton *button, gpointer user_data)
{
    uint32 keysym;
    uint8 flags;
    const char *command;
    struct keybind *kb;

    keybinding_get_data(&keysym, &flags, &command);

    /* keybind_insert will do a strdup of command for us */
    kb = keybind_find(keysym, flags, (flags & KEYF_R_GLOBAL));
    if (kb && (!keybind_overwrite_confirm(kb)))
        return;
    keybind_insert(keysym, flags, command);

    /*
     * I think it is more appropriate to clear the fields once the user adds
     * it.  I suppose the ideal case would be to select the newly inserted
     * keybinding.
     */
    reset_keybinding_status();
    save_keys();
    update_keybinding_list();
}

/**
 * Implements the "Update Binding" button to update the currently selected
 * keybinding to match the currently shown identifiers, key, or command input
 * fields.  If a keybinding is highlighted, so something.  If not, log an error
 * since the "Update Binding" button should have been disabled.
 *
 * @param button
 * @param user_data
 */
void on_keybinding_button_update_clicked(GtkButton *button, gpointer user_data)
{
    GtkTreeIter iter;
    struct keybind *kb;
    GtkTreeModel *model;
    uint32 keysym;
    uint8 flags;
    const char *buf;
    int res;

    if (gtk_tree_selection_get_selected(keybinding_selection, &model, &iter)) {
        gtk_tree_model_get(model, &iter, KLIST_KEY_ENTRY, &kb, -1);

        if (!kb) {
            LOG(LOG_ERROR, "keys.c::on_keybinding_button_update_clicked",
                "Unable to get key_entry structure\n");
            return;
        }

        /* We need to rehash the binding (remove the old and add the
         * new) since the keysym might have changed. */

        keybind_remove(kb);

        keybinding_get_data(&keysym, &flags, &buf);

        res = keybind_insert(keysym, flags, buf);
        if (res == 0) {
            keybind_free(&kb);
        } else {
            /* Re-enter old binding if the update failed */
            keybind_insert(kb->keysym, kb->flags, kb->command);
            // FIXME: Popup dialog key in use
        }

        save_keys();
        update_keybinding_list();
    } else {
        LOG(LOG_ERROR, "keys.c::on_keybinding_button_update_clicked",
            "Nothing selected to update\n");
    }
}

/**
 * Deactivates the keybinding dialog when the "Close Window" button is clicked.
 *
 * @param button
 * @param user_data
 */
void on_keybinding_button_close_clicked(GtkButton *button, gpointer user_data)
{
    gtk_widget_hide(keybinding_window);
}

/**
 * Deactivate the modifier checkboxes if "Any" is selected.
 *
 * @param button
 * @param user_data
 */
void on_keybinding_checkbutton_any_clicked(GtkCheckButton *cb, gpointer user_data)
{
    gboolean enabled;

    enabled = !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cb));

    gtk_widget_set_sensitive(GTK_WIDGET(keybinding_checkbutton_control), enabled);
    gtk_widget_set_sensitive(GTK_WIDGET(keybinding_checkbutton_shift), enabled);
    gtk_widget_set_sensitive(GTK_WIDGET(keybinding_checkbutton_alt), enabled);
    gtk_widget_set_sensitive(GTK_WIDGET(keybinding_checkbutton_meta), enabled);
}

/**
 * Called when the user clicks one of the entries in the list of keybindings
 * and places information about it into the input fields on the dialog.  This
 * allows the player to edit and update, or remove bindings.
 *
 * @param selection
 * @param model
 * @param path
 * @param path_currently_selected
 * @param userdata
 * @return TRUE
 */
gboolean keybinding_selection_func(
    GtkTreeSelection *selection,
    GtkTreeModel     *model,
    GtkTreePath      *path,
    gboolean          path_currently_selected,
    gpointer          userdata)
{
    GtkTreeIter iter;
    struct keybind *kb;

    gtk_widget_set_sensitive(keybinding_button_remove, TRUE);
    gtk_widget_set_sensitive(keybinding_button_update, TRUE);

    if (gtk_tree_model_get_iter(model, &iter, path)) {

        gtk_tree_model_get(model, &iter, KLIST_KEY_ENTRY, &kb, -1);

        if (!kb) {
            LOG(LOG_ERROR, "keys.c::keybinding_selection_func",
                "Unable to get key_entry structure\n");
            return FALSE;
        }
        if (kb->flags & KEYF_ANY)
            gtk_toggle_button_set_active(
                GTK_TOGGLE_BUTTON(keybinding_checkbutton_any), TRUE);
        else
            gtk_toggle_button_set_active(
                GTK_TOGGLE_BUTTON(keybinding_checkbutton_any), FALSE);

        if (kb->flags & KEYF_MOD_CTRL)
            gtk_toggle_button_set_active(
                GTK_TOGGLE_BUTTON(keybinding_checkbutton_control), TRUE);
        else
            gtk_toggle_button_set_active(
                GTK_TOGGLE_BUTTON(keybinding_checkbutton_control), FALSE);

        if (kb->flags & KEYF_MOD_SHIFT)
            gtk_toggle_button_set_active(
                GTK_TOGGLE_BUTTON(keybinding_checkbutton_shift), TRUE);
        else
            gtk_toggle_button_set_active(
                GTK_TOGGLE_BUTTON(keybinding_checkbutton_shift), FALSE);

        if (kb->flags & KEYF_MOD_ALT)
            gtk_toggle_button_set_active(
                GTK_TOGGLE_BUTTON(keybinding_checkbutton_alt), TRUE);
        else
            gtk_toggle_button_set_active(
                GTK_TOGGLE_BUTTON(keybinding_checkbutton_alt), FALSE);

        if (kb->flags & KEYF_MOD_META)
            gtk_toggle_button_set_active(
                GTK_TOGGLE_BUTTON(keybinding_checkbutton_meta), TRUE);
        else
            gtk_toggle_button_set_active(
                GTK_TOGGLE_BUTTON(keybinding_checkbutton_meta), FALSE);

        if (kb->flags & KEYF_EDIT)
            gtk_toggle_button_set_active(
                GTK_TOGGLE_BUTTON(keybinding_checkbutton_edit), TRUE);
        else
            gtk_toggle_button_set_active(
                GTK_TOGGLE_BUTTON(keybinding_checkbutton_edit), FALSE);

        gtk_entry_set_text(
            GTK_ENTRY(keybinding_entry_key), gdk_keyval_name(kb->keysym));
        gtk_entry_set_text(
            GTK_ENTRY(keybinding_entry_command), kb->command);

        toggle_buttons_scope((kb->flags & KEYF_R_GLOBAL)!=0);
    }
    return TRUE;
}

/**
 * Reset the state of the keybinding dialog.  Uncheck all modifier checkboxes,
 * clear the key input box, clear the command input box, and disable the two
 * update and remove keybinding buttons.
 */
void reset_keybinding_status(void)
{
    gtk_toggle_button_set_active(
        GTK_TOGGLE_BUTTON(keybinding_checkbutton_any), FALSE);
    gtk_toggle_button_set_active(
        GTK_TOGGLE_BUTTON(keybinding_checkbutton_control), FALSE);
    gtk_toggle_button_set_active(
        GTK_TOGGLE_BUTTON(keybinding_checkbutton_shift), FALSE);
    gtk_toggle_button_set_active(
        GTK_TOGGLE_BUTTON(keybinding_checkbutton_alt), FALSE);
    gtk_toggle_button_set_active(
        GTK_TOGGLE_BUTTON(keybinding_checkbutton_meta), FALSE);
    gtk_toggle_button_set_active(
        GTK_TOGGLE_BUTTON(keybinding_checkbutton_edit), FALSE);
    gtk_entry_set_text (GTK_ENTRY(keybinding_entry_key), "");
    gtk_entry_set_text (GTK_ENTRY(keybinding_entry_command), "");

    toggle_buttons_scope(FALSE);

    gtk_widget_set_sensitive(keybinding_button_remove, FALSE);
    gtk_widget_set_sensitive(keybinding_button_update, FALSE);
}

/**
 * Implements the "Clear Fields" button function on the keybinding dialog.  If
 * a keybinding is highlighted (selected), de-select it first, then clear all
 * of * the input boxes and reset any buttons to an appropriate state.
 *
 * @param button
 * @param user_data
 */
void on_keybinding_button_clear_clicked(GtkButton *button, gpointer user_data)
{
    GtkTreeModel *model;
    GtkTreeIter iter;

    /*
     * As the cleared state is not supposed to have a keybinding selected,
     * deselect the currently selected keybinding if there is one.
     */
    if (gtk_tree_selection_get_selected(keybinding_selection, &model, &iter)) {
        gtk_tree_selection_unselect_iter(keybinding_selection, &iter);
    }
    reset_keybinding_status();          /* Clear inputs and reset buttons. */
}

/**
 * @} EndOf GtkV2KeyBindingWindow
 */
