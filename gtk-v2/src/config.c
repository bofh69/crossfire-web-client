/*
 * Crossfire -- cooperative multi-player graphical RPG and adventure game
 *
 * Copyright (c) 1999-2013 Mark Wedel and the Crossfire Development Team
 * Copyright (c) 1992 Frank Tore Johansen
 *
 * Crossfire is free software and comes with ABSOLUTELY NO WARRANTY. You are
 * welcome to redistribute it under certain conditions. For details, please
 * see COPYING and LICENSE.
 *
 * The authors can be reached via e-mail at <crossfire@metalforge.org>.
 */

/**
 * @file
 * Implement client configuration dialog
 */

#include "client.h"

#include <ctype.h>
#include <gtk/gtk.h>

#include "image.h"
#include "main.h"
#include "mapdata.h"
#include "gtk2proto.h"

static GKeyFile *config;
static GString *config_path;

GtkWidget *config_dialog, *config_button_echo, *config_button_fasttcp,
    *config_button_timestamp, *config_button_grad_color,
    *config_button_foodbeep, *config_button_sound, *config_button_cache,
    *config_button_download, *config_button_fog, *config_button_smoothing;

GtkFileChooser *ui_filechooser, *theme_filechooser;
GtkComboBoxText *config_combobox_faceset;
GtkComboBox *config_combobox_displaymode, *config_combobox_lighting;

#define THEME_DEFAULT CF_DATADIR "/themes/Standard"
static char *theme = THEME_DEFAULT;

static void on_config_close(GtkButton *button, gpointer user_data);

/**
 * Return the basename of the current UI file.
 */
static char *ui_name() {
    return g_path_get_basename(window_xml_file);
}

/**
 * Sets up player-specific client and layout rc files and handles loading of a
 * client theme if one is selected.  First, the player-specific rc files are
 * added to the GTK rc default files list.  ${HOME}/.crossfire/gtkrc is added
 * first.  All client sessions are affected by this rc file if it exists.
 * Next, ${HOME}/.crossfire/[layout].gtkrc is added, where [layout] is the
 * name of the layout file that is loaded.  IE. If gtk-v2.ui is loaded,
 * [layout] is "gtk-v2".  This sets up the possibility for a player to make a
 * layout-specific rc file.  Finally, if the client theme is not "None", the
 * client theme file is added.  In most cases, the player-specific files are
 * probably not going to exist, so the theme system will continue to work the
 * way it always has.  The player will have to "do something" to get the extra
 * functionality.  At some point, conceptually the client itself could be
 * enhanced to allow it to save some basic settings to either or both of the
 * player-specific rc files.
 *
 * @param reload
 * If true, user has changed theme after initial startup.  In this mode, we
 * need to call the routines that store away private theme data.  When program
 * is starting up, this is false, because all the widgets haven't been realized
 * yet, and the initialize routines will get the theme data at that time.
 */
static char **default_files = NULL;
void init_theme() {
    char path[MAX_BUF];
    char **tmp;
    int i;

    /*
     * The GTK man page says copy of this data should be made, so do that.
     */
    tmp = gtk_rc_get_default_files();
    i = 0;
    while (tmp[i]) {
        i++;
    }
    /*
     * Add two more GTK rc files that may be used by a player to customize
     * the client appearance in general, or to customize the appearance
     * of a specific layout.  Allocate pointers to the local copy
     * of the entire list.
     */
    i += 2;
    default_files = g_malloc(sizeof(char *) * (i + 1));
    /*
     * Copy in GTK's default list which probably contains system paths
     * like <SYSCONFDIR>/gtk-2.0/gtkrc and user-specific files like
     * ${HOME}/.gtkrc, or even LANGuage-specific ones like
     * ${HOME}/.gtkrc.en, etc.
     */
    i = 0;
    while (tmp[i]) {
        default_files[i] = g_strdup(tmp[i]);
        i++;
    }
    /*
     * Add a player-specific gtkrc to the list of default rc files.  This
     * file is probably reserved for player use, though in all liklihood
     * will not get used that much.  Still, it makes it easy for someone
     * to make their own theme without having to have access to the
     * system-wide theme folder.  This is the lowest priority client rc
     * file as either a <layout>.gtkrc file or a client-configured theme
     * settings can over-ride it.
     */
    snprintf(path, sizeof(path), "%s/gtkrc", config_dir);
    default_files[i] = g_strdup(path);
    i++;
    /*
     * Add a UI layout-specific rc file to the list of default list.  It
     * seems reasonable to allow client code to have access to this file
     * to make some basic changes to fonts, via a graphical interface.
     * Truncate window_xml_file to remove a .extension if one exists, so
     * that the window positions file can be created with a .gtkrc suffix.
     * This is a mid-priority client rc file as its settings supersede the
     * client gtkrc file, but are overridden by a client-configured theme.
     */
    snprintf(path, sizeof(path), "%s/%s.gtkrc", config_dir, ui_name());
    default_files[i] = g_strdup(path);
    i++;
    /*
     * Mark the end of the list of default rc files.
     */
    default_files[i] = NULL;
}

void load_theme(int reload) {
    /*
     * Whether or not this is default and initial run, we want to register
     * the modified rc search path list, so GTK needs to get the changes.
     * It is necessary to reset the the list each time through here each
     * theme change grows the list.  Only one theme should be in the list
     * at a time.
     */
    gtk_rc_set_default_files(default_files);

    /*
     * If a client-configured theme has been selected (something other than
     * "None"), then add it to the list of GTK rc files to process.  Since
     * this file is added last, it takes priority over both the gtkrc and
     * <layout>.gtkrc files.  Remember, strcmp returns zero on a match, and
     * a theme file should not be registered if "None" is selected.
     */
    if (theme != NULL) {
        /*
         * Check for existence of the client theme file.  Unfortunately, at
         * initial run time, the window may not be realized yet, so the
         * message cannot be sent to the user directly.  It doesn't hurt to
         * add the path even if the file isn't there, but the player might
         * still want to know something is wrong since they picked a theme.
         */
        if (access(theme, R_OK) == -1) {
            LOG(LOG_ERROR, "load_theme", "Unable to find theme file %s", theme);
            theme = THEME_DEFAULT;
        }
        gtk_rc_add_default_file(theme);
    }

    /*
     * Require GTK to reparse and rebind all the widget data.
     */
    gtk_rc_reparse_all_for_settings(
        gtk_settings_get_for_screen(gdk_screen_get_default()), TRUE);
    gtk_rc_reset_styles(
        gtk_settings_get_for_screen(gdk_screen_get_default()));
    /*
     * Call client functions to reparse the custom widgets it controls.
     */
    info_get_styles();
    inventory_get_styles();
    stats_get_styles();
    spell_get_styles();
    update_spell_information();
    /*
     * Set inv_updated to force a redraw - otherwise it will not
     * necessarily bind the lists with the new widgets.
     */
    cpl.below->inv_updated = 1;
    cpl.ob->inv_updated = 1;
    draw_lists();
    draw_stats(TRUE);
    draw_message_window(TRUE);
}

/**
 * Load settings from the legacy file format.
 */
static void config_load_legacy() {
    char path[MAX_BUF], inbuf[MAX_BUF], *cp;
    FILE *fp;
    int i, val;

    LOG(LOG_DEBUG, "config_load_legacy", "Trying to load legacy settings...");

    snprintf(path, sizeof(path), "%s/.crossfire/gdefaults2", g_getenv("HOME"));
    if ((fp = fopen(path, "r")) == NULL) {
        return;
    }
    while (fgets(inbuf, MAX_BUF - 1, fp)) {
        inbuf[MAX_BUF - 1] = '\0';
        inbuf[strlen(inbuf) - 1] = '\0'; /* kill newline */

        if (inbuf[0] == '#') {
            continue;
        }
        /* Skip any setting line that does not contain a colon character */
        if (!(cp = strchr(inbuf, ':'))) {
            continue;
        }
        *cp = '\0';
        cp += 2;    /* colon, space, then value */

        val = -1;
        if (isdigit(*cp)) {
            val = atoi(cp);
        } else if (!strcmp(cp, "True")) {
            val = TRUE;
        } else if (!strcmp(cp, "False")) {
            val = FALSE;
        }

        for (i = 1; i < CONFIG_NUMS; i++) {
            if (!strcmp(config_names[i], inbuf)) {
                if (val == -1) {
                    LOG(LOG_WARNING, "config.c::load_defaults",
                        "Invalid value/line: %s: %s", inbuf, cp);
                } else {
                    want_config[i] = val;
                }
                break;  /* Found a match - won't find another */
            }
        }
        /* We found a match in the loop above, so do not do anything more */
        if (i < CONFIG_NUMS) {
            continue;
        }

        /*
         * Legacy - now use the map_width and map_height values Don't do sanity
         * checking - that will be done below
         */
        if (!strcmp(inbuf, "mapsize")) {
            if (sscanf(cp, "%hdx%hd", &want_config[CONFIG_MAPWIDTH],
                       &want_config[CONFIG_MAPHEIGHT]) != 2) {
                LOG(LOG_WARNING, "config.c::load_defaults",
                    "Malformed mapsize option in gdefaults2.  Ignoring");
            }
        } else if (!strcmp(inbuf, "theme")) {
            theme = g_strdup(cp);   /* memory leak ! */
            continue;
        } else if (!strcmp(inbuf, "window_layout")) {
            strncpy(window_xml_file, cp, MAX_BUF - 1);
            continue;
        } else if (!strcmp(inbuf, "nopopups")) {
            /* Changed name from nopopups to popups, so inverse value */
            want_config[CONFIG_POPUPS] = !val;
            continue;
        } else if (!strcmp(inbuf, "nosplash")) {
            want_config[CONFIG_SPLASH] = !val;
            continue;
        } else if (!strcmp(inbuf, "splash")) {
            want_config[CONFIG_SPLASH] = val;
            continue;
        } else if (!strcmp(inbuf, "faceset")) {
            face_info.want_faceset = g_strdup(cp);  /* memory leak ! */
            continue;
        }
        /* legacy, as this is now just saved as 'lighting' */
        else if (!strcmp(inbuf, "per_tile_lighting")) {
            if (val) {
                want_config[CONFIG_LIGHTING] = CFG_LT_TILE;
            }
        } else if (!strcmp(inbuf, "per_pixel_lighting")) {
            if (val) {
                want_config[CONFIG_LIGHTING] = CFG_LT_PIXEL;
            }
        } else if (!strcmp(inbuf, "resists")) {
            if (val) {
                want_config[CONFIG_RESISTS] = val;
            }
        } else if (!strcmp(inbuf, "sdl")) {
            if (val) {
                want_config[CONFIG_DISPLAYMODE] = CFG_DM_SDL;
            }
        } else LOG(LOG_WARNING, "config.c::load_defaults",
                       "Unknown line in gdefaults2: %s %s", inbuf, cp);
    }
    fclose(fp);
}

/**
 * Sanity check values set in want_config and copy them over to use_config
 * when all of them are acceptable.
 *
 * This function should be called after config_load() and parse_args().
 */
void config_check() {
    if (want_config[CONFIG_ICONSCALE] < 25 ||
            want_config[CONFIG_ICONSCALE] > 200) {
        LOG(LOG_WARNING, "config_check",
                "Ignoring invalid 'iconscale' value '%d'; "
                "must be between 25 and 200.\n",
                want_config[CONFIG_ICONSCALE]);
        want_config[CONFIG_ICONSCALE] = use_config[CONFIG_ICONSCALE];
    }

    if (want_config[CONFIG_MAPSCALE] < 25 ||
            want_config[CONFIG_MAPSCALE] > 200) {
        LOG(LOG_WARNING, "config_check",
                "Ignoring invalid 'mapscale' value '%d'; "
                "must be between 25 and 200.\n",
                want_config[CONFIG_MAPSCALE]);
        want_config[CONFIG_MAPSCALE] = use_config[CONFIG_MAPSCALE];
    }

    if (!want_config[CONFIG_LIGHTING]) {
        LOG(LOG_WARNING, "config_check",
            "No lighting mechanism selected - will not use darkness code");
        want_config[CONFIG_DARKNESS] = FALSE;
    }

    if (want_config[CONFIG_RESISTS] > 2) {
        LOG(LOG_WARNING, "config_check",
                "Ignoring invalid 'resists' value '%d'; "
                "must be either 0, 1, or 2.\n",
                want_config[CONFIG_RESISTS]);
        want_config[CONFIG_RESISTS] = 0;
    }

    /* Make sure the map size os OK */
    if (want_config[CONFIG_MAPWIDTH] < 9 ||
            want_config[CONFIG_MAPWIDTH] > MAP_MAX_SIZE) {
        LOG(LOG_WARNING, "config_check", "Invalid map width (%d) "
            "option in gdefaults2. Valid range is 9 to %d",
            want_config[CONFIG_MAPWIDTH], MAP_MAX_SIZE);
        want_config[CONFIG_MAPWIDTH] = use_config[CONFIG_MAPWIDTH];
    }

    if (want_config[CONFIG_MAPHEIGHT] < 9 ||
            want_config[CONFIG_MAPHEIGHT] > MAP_MAX_SIZE) {
        LOG(LOG_WARNING, "config_check", "Invalid map height (%d) "
            "option in gdefaults2. Valid range is 9 to %d",
            want_config[CONFIG_MAPHEIGHT], MAP_MAX_SIZE);
        want_config[CONFIG_MAPHEIGHT] = use_config[CONFIG_MAPHEIGHT];
    }

#if !defined(HAVE_OPENGL)
    if (want_config[CONFIG_DISPLAYMODE] == CFG_DM_OPENGL) {
        want_config[CONFIG_DISPLAYMODE] = CFG_DM_PIXMAP;
        LOG(LOG_ERROR, "config_check",
            "Display mode is set to OpenGL, but client "
            "is not compiled with OpenGL support.  Reverting to pixmap mode.");
    }
#endif

#if !defined(HAVE_SDL)
    if (want_config[CONFIG_DISPLAYMODE] == CFG_DM_SDL) {
        want_config[CONFIG_DISPLAYMODE] = CFG_DM_PIXMAP;
        LOG(LOG_ERROR, "config_check",
            "Display mode is set to SDL, but client "
            "is not compiled with SDL support.  Reverting to pixmap mode.");
    }
#endif

    /* Copy sanitized user settings to current settings. */
    memcpy(use_config, want_config, sizeof(use_config));

    image_size = DEFAULT_IMAGE_SIZE * use_config[CONFIG_ICONSCALE] / 100;
    map_image_size = DEFAULT_IMAGE_SIZE * use_config[CONFIG_MAPSCALE] / 100;
    map_image_half_size = DEFAULT_IMAGE_SIZE * use_config[CONFIG_MAPSCALE] / 200;
    if (!use_config[CONFIG_CACHE]) {
        use_config[CONFIG_DOWNLOAD] = FALSE;
    }

    mapdata_init();
}

/**
 * Load settings from the user's configuration file into want_config.
 */
void config_load() {
    GError *error = NULL;

    /* Copy initial desired settings from current settings. */
    memcpy(want_config, use_config, sizeof(want_config));

    g_assert(g_file_test(config_dir, G_FILE_TEST_IS_DIR) == TRUE);

    /* Load existing or create new configuration file. */
    config = g_key_file_new();
    config_path = g_string_new(config_dir);
    g_string_append(config_path, "/client.ini");

    g_key_file_load_from_file(config, config_path->str, G_KEY_FILE_NONE, &error);

    /* Load configuration values into settings array. */
    if (error == NULL) {
        for (int i = 1; i < CONFIG_NUMS; i++) {
            want_config[i] = g_key_file_get_integer(config, "Client",
                    config_names[i], NULL);
        }

        /* Load additional settings. */
        /* TODO: Both of these below are one-time memory leaks. */
        theme = g_key_file_get_string(config, "GTKv2",
                "theme", NULL);
        face_info.want_faceset = g_key_file_get_string(config, "GTKv2",
                "faceset", NULL);

        char *layout =
            g_key_file_get_string(config, "GTKv2", "window_layout", NULL);
        g_strlcpy(window_xml_file, layout, sizeof(window_xml_file));
        free(layout);
    } else {
        g_error_free(error);

        /* Load legacy configuration file. */
        config_load_legacy();
    }
}

/**
 * This function saves user settings chosen using the configuration popup
 * dialog.
 */
void save_defaults() {
    GError *error = NULL;

    /* Save GTKv2 specific client settings. */
    g_key_file_set_string(config, "GTKv2", "theme", theme);
    g_key_file_set_string(config, "GTKv2", "faceset", face_info.want_faceset);
    g_key_file_set_string(config, "GTKv2", "window_layout", window_xml_file);

    /* Save the rest of the client settings. */
    for (int i = 1; i < CONFIG_NUMS; i++) {
        g_key_file_set_integer(config, "Client", config_names[i], want_config[i]);
    }

    g_file_set_contents(config_path->str,
            g_key_file_to_data(config, NULL, NULL), -1, &error);

    if (error != NULL) {
        draw_ext_info(NDI_RED, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_CONFIG,
                "Could not save settings!");
        g_warning("Could not save settings: %s", error->message);
        g_error_free(error);
    }
}

void config_init(GtkWidget *window_root) {
    config_dialog =
        GTK_WIDGET(gtk_builder_get_object(dialog_xml, "config_dialog"));

    // Initialize file choosers and set filename filters.
    ui_filechooser =
        GTK_FILE_CHOOSER(gtk_builder_get_object(dialog_xml, "ui_filechooser"));
    theme_filechooser = GTK_FILE_CHOOSER(
        gtk_builder_get_object(dialog_xml, "theme_filechooser"));

    GtkFileFilter *ui_filter = gtk_file_filter_new();
    gtk_file_filter_add_pattern(ui_filter, "*.ui");
    gtk_file_chooser_set_filter(ui_filechooser, ui_filter);

    config_button_echo =
        GTK_WIDGET(gtk_builder_get_object(dialog_xml, "config_button_echo"));
    config_button_fasttcp =
        GTK_WIDGET(gtk_builder_get_object(dialog_xml, "config_button_fasttcp"));
    config_button_timestamp =
        GTK_WIDGET(gtk_builder_get_object(dialog_xml, "config_button_timestamp"));
    config_button_grad_color =
        GTK_WIDGET(gtk_builder_get_object(dialog_xml, "config_button_grad_color"));
    config_button_foodbeep =
        GTK_WIDGET(gtk_builder_get_object(dialog_xml, "config_button_foodbeep"));
    config_button_sound =
        GTK_WIDGET(gtk_builder_get_object(dialog_xml, "config_button_sound"));
    config_button_cache =
        GTK_WIDGET(gtk_builder_get_object(dialog_xml, "config_button_cache"));
    config_button_download =
        GTK_WIDGET(gtk_builder_get_object(dialog_xml, "config_button_download"));
    config_button_fog =
        GTK_WIDGET(gtk_builder_get_object(dialog_xml, "config_button_fog"));
    config_button_smoothing =
        GTK_WIDGET(gtk_builder_get_object(dialog_xml, "config_button_smoothing"));

    config_combobox_displaymode = GTK_COMBO_BOX(
        gtk_builder_get_object(dialog_xml, "config_combobox_displaymode"));
    config_combobox_faceset = GTK_COMBO_BOX_TEXT(
        gtk_builder_get_object(dialog_xml, "config_combobox_faceset"));
    config_combobox_lighting = GTK_COMBO_BOX(
        gtk_builder_get_object(dialog_xml, "config_combobox_lighting"));

    GtkWidget *config_button_close =
        GTK_WIDGET(gtk_builder_get_object(dialog_xml, "config_button_close"));
    g_signal_connect(config_button_close, "clicked",
                     G_CALLBACK(on_config_close), NULL);
    g_signal_connect(config_dialog, "delete_event", G_CALLBACK(on_config_close),
                     NULL);

    // Initialize available rendering modes.
    GtkListStore *display_list =
        GTK_LIST_STORE(gtk_combo_box_get_model(config_combobox_displaymode));
    GtkTreeIter iter;
#ifdef HAVE_OPENGL
    gtk_list_store_append(display_list, &iter);
    gtk_list_store_set(display_list, &iter, 0, "OpenGL", 1, CFG_DM_OPENGL, -1);
#endif
#ifdef HAVE_SDL
    gtk_list_store_append(display_list, &iter);
    gtk_list_store_set(display_list, &iter, 0, "SDL", 1, CFG_DM_SDL, -1);
#endif
    gtk_list_store_append(display_list, &iter);
    gtk_list_store_set(display_list, &iter, 0, "Pixmap", 1, CFG_DM_PIXMAP, -1);
}

/**
 * Removes all the text entries from the combo box. This function is not
 * available in GTK+2, so implement it ourselves.
 */
static void combo_box_text_remove_all(GtkComboBoxText *combo_box) {
    int count = gtk_tree_model_iter_n_children(
        gtk_combo_box_get_model(GTK_COMBO_BOX(combo_box)), NULL);
    for (int i = 0; i < count; i++) {
        gtk_combo_box_text_remove(combo_box, 0);
    }
}

/*
 * Setup config_dialog sets the buttons, combos, etc, to the state that matches
 * the want_config[] values.
 */
static void setup_config_dialog() {
    GtkTreeIter iter;
    gchar *buf;
    int count;

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(config_button_echo),
                                 want_config[CONFIG_ECHO]);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(config_button_fasttcp),
                                 want_config[CONFIG_FASTTCP]);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(config_button_timestamp),
                                 want_config[CONFIG_TIMESTAMP]);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(config_button_grad_color),
                                 want_config[CONFIG_GRAD_COLOR]);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(config_button_foodbeep),
                                 want_config[CONFIG_FOODBEEP]);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(config_button_sound),
                                 want_config[CONFIG_SOUND]);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(config_button_cache),
                                 want_config[CONFIG_CACHE]);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(config_button_download),
                                 want_config[CONFIG_DOWNLOAD]);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(config_button_fog),
                                 want_config[CONFIG_FOGWAR]);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(config_button_smoothing),
                                 want_config[CONFIG_SMOOTH]);

    // Fill face set combo box with available face sets from the server.
    combo_box_text_remove_all(config_combobox_faceset);

    /* If we have real faceset info from the server, use it */
    if (face_info.have_faceset_info) {
        for (int i = 0; i < MAX_FACE_SETS; i++)
            if (face_info.facesets[i].fullname)
                gtk_combo_box_text_append_text(config_combobox_faceset,
                                               face_info.facesets[i].fullname);
    }

    GtkTreeModel *model =
        gtk_combo_box_get_model(GTK_COMBO_BOX(config_combobox_faceset));
    count = gtk_tree_model_iter_n_children(model, NULL);
    for (int i = 0; i < count; i++) {
        if (!gtk_tree_model_iter_nth_child(model, &iter, NULL, i)) {
            LOG(LOG_ERROR, "setup_config_dialog", "Cannot iterate facesets\n");
            break;
        }
        gtk_tree_model_get(model, &iter, 0, &buf, -1);

        if (face_info.want_faceset &&
            !g_ascii_strcasecmp(face_info.want_faceset, buf)) {
            gtk_combo_box_set_active(GTK_COMBO_BOX(config_combobox_faceset), i);
            g_free(buf);
            break;
        }
        g_free(buf);
    }

    // Set current display mode.
    model = gtk_combo_box_get_model(config_combobox_displaymode);
    bool next = gtk_tree_model_get_iter_first(model, &iter);
    while (next) {
        int current;
        gtk_tree_model_get(model, &iter, 1, &current, -1);
        if (current == want_config[CONFIG_DISPLAYMODE]) {
            gtk_combo_box_set_active_iter(config_combobox_displaymode, &iter);
            break;
        }
        next = gtk_tree_model_iter_next(model, &iter);
    }

    // Lighting option indexes never change, so set option using index.
    gtk_combo_box_set_active(config_combobox_lighting,
                             want_config[CONFIG_LIGHTING]);

    gtk_file_chooser_set_filename(ui_filechooser, window_xml_file);
    gtk_file_chooser_set_filename(theme_filechooser, theme);
}

#define IS_DIFFERENT(TYPE) (want_config[TYPE] != use_config[TYPE])

/**
 * Get an integer value from 'column' of the active field in 'combobox'.
 */
static int combobox_get_value(GtkComboBox *combobox, int column) {
    GtkTreeModel *model = gtk_combo_box_get_model(combobox);
    GtkTreeIter iter;
    int result;

    gtk_combo_box_get_active_iter(combobox, &iter);
    gtk_tree_model_get(model, &iter, column, &result, -1);
    return result;
}

/**
 * This is basically the opposite of setup_config_dialog() above - instead of
 * setting the display state appropriately, we read the display state and
 * update the want_config values.
 */
static void read_config_dialog(void) {
    want_config[CONFIG_ECHO] =
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(config_button_echo));
    want_config[CONFIG_FASTTCP] =
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(config_button_fasttcp));
    want_config[CONFIG_TIMESTAMP] = gtk_toggle_button_get_active(
        GTK_TOGGLE_BUTTON(config_button_timestamp));
    want_config[CONFIG_GRAD_COLOR] = gtk_toggle_button_get_active(
        GTK_TOGGLE_BUTTON(config_button_grad_color));
    want_config[CONFIG_FOODBEEP] =
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(config_button_foodbeep));
    want_config[CONFIG_SOUND] =
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(config_button_sound));
    want_config[CONFIG_CACHE] =
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(config_button_cache));
    want_config[CONFIG_DOWNLOAD] =
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(config_button_download));
    want_config[CONFIG_FOGWAR] =
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(config_button_fog));
    want_config[CONFIG_SMOOTH] = gtk_toggle_button_get_active(
        GTK_TOGGLE_BUTTON(config_button_smoothing));

    gchar *buf;

    buf = gtk_combo_box_text_get_active_text(config_combobox_faceset);
    if (buf) {
        free(face_info.want_faceset);
        face_info.want_faceset = g_strdup(buf);
        g_free(buf);
    }

    want_config[CONFIG_DISPLAYMODE] =
        combobox_get_value(config_combobox_displaymode, 1);

    // Lighting option indexes never change, so get option using index.
    want_config[CONFIG_LIGHTING] =
        gtk_combo_box_get_active(config_combobox_lighting);

    // Enable darkness if lighting is not 'None'.
    if (want_config[CONFIG_LIGHTING] != CFG_LT_NONE) {
        want_config[CONFIG_DARKNESS] = 1;
        use_config[CONFIG_DARKNESS] = 1;
    }

    // Set UI file.
    buf = gtk_file_chooser_get_filename(ui_filechooser);
    if (buf != NULL) {
        g_strlcpy(window_xml_file, buf, sizeof(window_xml_file));
        g_free(buf);
    }

    // Set and load theme file.
    buf = gtk_file_chooser_get_filename(theme_filechooser);
    if (buf != NULL && g_ascii_strcasecmp(buf, theme) != 0) {
        g_free(theme);
        theme = buf;
        load_theme(TRUE);
    }

    /*
     * Some values can take effect right now, others not.  Code below handles
     * these cases - largely grabbed from gtk/config.c
     */
    if (IS_DIFFERENT(CONFIG_SOUND)) {
        int tmp;
        if (want_config[CONFIG_SOUND]) {
            tmp = init_sounds();
            if (csocket.fd) {
                cs_print_string(csocket.fd, "setup sound %d", tmp >= 0);
            }
        } else {
            if (csocket.fd) {
                cs_print_string(csocket.fd, "setup sound 0");
            }
        }
        use_config[CONFIG_SOUND] = want_config[CONFIG_SOUND];
    }
    if (IS_DIFFERENT(CONFIG_FASTTCP)) {
#ifdef TCP_NODELAY
#ifndef WIN32
        int q = want_config[CONFIG_FASTTCP];

        if (csocket.fd &&
                setsockopt(csocket.fd, SOL_TCP, TCP_NODELAY, &q, sizeof(q)) == -1) {
            perror("TCP_NODELAY");
        }
#else
        int q = want_config[CONFIG_FASTTCP];

        if (csocket.fd &&
                setsockopt(csocket.fd, SOL_TCP, TCP_NODELAY, (const char *)&q,
                           sizeof(q)) == -1) {
            perror("TCP_NODELAY");
        }
#endif
#endif
        use_config[CONFIG_FASTTCP] = want_config[CONFIG_FASTTCP];
    }

    if (IS_DIFFERENT(CONFIG_LIGHTING)) {
#ifdef HAVE_SDL
        if (use_config[CONFIG_DISPLAYMODE] == CFG_DM_SDL)
            /* This is done to make the 'lightmap' in the proper format */
        {
            init_SDL(NULL, 1);
        }
#endif
    }
    /*
     * Nothing to do, but we can switch immediately without problems.  do force
     * a redraw
     */
    if (IS_DIFFERENT(CONFIG_GRAD_COLOR)) {
        use_config[CONFIG_GRAD_COLOR] = want_config[CONFIG_GRAD_COLOR];
        draw_stats(TRUE);
    }
}

void on_configure_activate(GtkMenuItem *menuitem, gpointer user_data) {
    gtk_widget_show(config_dialog);
    setup_config_dialog();
}

static void on_config_close(GtkButton *button, gpointer user_data) {
    read_config_dialog();
    save_defaults();
    gtk_widget_hide(config_dialog);
}

/**
 * Save client window positions to a file unique to each layout.
 */
void save_winpos() {
    GSList *pane_list, *list_loop;
    int x, y, w, h, wx, wy;

    /* Save window position and size. */
    get_window_coord(window_root, &x, &y, &wx, &wy, &w, &h);

    GString *window_root_info = g_string_new(NULL);
    g_string_printf(window_root_info, "+%d+%dx%dx%d", wx, wy, w, h);

    g_key_file_set_string(config, ui_name(),
            "window_root", window_root_info->str);
    g_string_free(window_root_info, TRUE);

    /* Save the positions of all the HPANEDs and VPANEDs. */
    pane_list = gtk_builder_get_objects(window_xml);

    for (list_loop = pane_list; list_loop != NULL; list_loop = list_loop->next) {
        GType type = G_OBJECT_TYPE(list_loop->data);

        if (type == GTK_TYPE_HPANED || type == GTK_TYPE_VPANED) {
            g_key_file_set_integer(config, ui_name(),
                    gtk_buildable_get_name(list_loop->data),
                    gtk_paned_get_position(GTK_PANED(list_loop->data)));
        }
    }

    g_slist_free(pane_list);
    save_defaults();

    draw_ext_info(NDI_BLUE, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_CONFIG,
                  "Window positions saved!");
}

/**
 * Handles saving of the window positions when the Client | Save Window
 * Position menu item is activated.
 *
 * @param menuitem
 * @param user_data
 */
void on_save_window_position_activate(GtkMenuItem *menuitem,
        gpointer user_data) {
    save_winpos();
    /*
     * The following prevents multiple saves per menu activation.
     */
    g_signal_stop_emission_by_name(GTK_OBJECT(menuitem), "activate");
}

/**
 * Resize the client window and its panels using saved window positions.
 *
 * @param window_root The client's main window.
 */
void load_window_positions(GtkWidget *window_root) {
    GSList *pane_list, *list;
    pane_list = gtk_builder_get_objects(window_xml);

    // Load and set main window dimensions.
    gchar *root_size = g_key_file_get_string(config, ui_name(),
            "window_root", NULL);

    if (root_size != NULL) {
        int w, h;

        if (sscanf(root_size, "+%*d+%*dx%dx%d", &w, &h) == 2) {
            gtk_window_set_default_size(GTK_WINDOW(window_root), w, h);
        }

        g_free(root_size);
    }

    // Load and set panel positions.
    for (list = pane_list; list != NULL; list = list->next) {
        GType type = G_OBJECT_TYPE(list->data);

        if (type == GTK_TYPE_HPANED || type == GTK_TYPE_VPANED) {
            int position = g_key_file_get_integer(config, ui_name(),
                    gtk_buildable_get_name(list->data), NULL);

            if (position != 0) {
                gtk_paned_set_position(GTK_PANED(list->data), position);
            }
        }
    }

    g_slist_free(pane_list);
}
