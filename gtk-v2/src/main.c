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
 * Client startup and main loop.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef WIN32
#include <windows.h>
#else
#include <signal.h>
#endif

#include <errno.h>
#include <gtk/gtk.h>
#include <stdbool.h>
#include <stdio.h>

#include "client.h"
#include "main.h"
#include "image.h"
#include "gtk2proto.h"
#include "script.h"
#include "metaserver.h"

/* Sets up the basic colors. */
static const char *const colorname[NUM_COLORS] = {
    "Black",                /* 0  */
    "White",                /* 1  */
    "Navy",                 /* 2  */
    "Red",                  /* 3  */
    "Orange",               /* 4  */
    "DodgerBlue",           /* 5  */
    "DarkOrange2",          /* 6  */
    "SeaGreen",             /* 7  */
    "DarkSeaGreen",         /* 8  *//* Used for window background color */
    "Grey50",               /* 9  */
    "Sienna",               /* 10 */
    "Gold",                 /* 11 */
    "Khaki"                 /* 12 */
};

/** Path to dialog layout file. */
static char dialog_xml_path[MAX_BUF] = XML_PATH_DEFAULT DIALOG_XML_FILENAME;

static struct timeval timeout;
static gboolean updatekeycodes = FALSE;

/* TODO: Move these declarations to actual header files. */
extern int time_map_redraw;
extern int MINLOG;

/** Command line options, descriptions, and parameters. */
static GOptionEntry options[] = {
    { "server", 's', 0, G_OPTION_ARG_STRING, &server,
        "Connect to the given server", "SERVER" },
    { "port", 'p', 0, G_OPTION_ARG_INT, &want_config[CONFIG_PORT],
        "Use the given port number", "PORT" },

    { "cache", 0, 0, G_OPTION_ARG_NONE, &want_config[CONFIG_CACHE],
        "Cache images", NULL },
    { "prefetch", 0, 0, G_OPTION_ARG_NONE, &want_config[CONFIG_DOWNLOAD],
        "Download images before playing", NULL },
    { "faceset", 0, 0, G_OPTION_ARG_STRING, &face_info.want_faceset,
        "Use the given faceset (if available)", "FACESET" },

    { "sound_server", 0, 0, G_OPTION_ARG_FILENAME, &sound_server,
        "Path to the sound server", "PATH" },
    { "updatekeycodes", 0, 0, G_OPTION_ARG_NONE, &updatekeycodes,
        "Update the saved bindings for this keyboard", NULL },

    { "timemapredraw", 0, 0, G_OPTION_ARG_NONE, &time_map_redraw,
        "Print map redraw profiling information", NULL },
    { "verbose", 'v', 0, G_OPTION_ARG_INT, &MINLOG,
        "Set verbosity (0 is the most verbose)", "LEVEL" },
    { NULL }
};

/** The file name of the window layout in use by the client. The base name,
 * without dot extention, is re-used when saving the window positions. */
char window_xml_file[MAX_BUF];

GdkColor root_color[NUM_COLORS];
gint csocket_fd = 0;

GtkBuilder *dialog_xml, *window_xml;
GtkWidget *window_root, *magic_map;

#ifdef WIN32 /* Win32 scripting support */
#define PACKAGE_DATA_DIR "."

static int do_scriptout() {
    script_process(NULL);
    return (TRUE);
}
#endif /* WIN32 */

/**
 * Map, spell, and inventory maintenance.
 * @return TRUE
 */
static int do_timeout() {
    if (cpl.showmagic) {
        magic_map_flash_pos();
    }
    if (cpl.spells_updated) {
        update_spell_information();
    }
    if (!tick) {
        inventory_tick();
        mapdata_animation();
    }

    if (cpl.input_state == Playing) {
        beat_check();
    }
    return TRUE;
}

/**
 * X11 client doesn't care about this
 */
void client_tick(guint32 tick) {
    info_buffer_tick();                 /* Maintain the info output buffers */
    inventory_tick();
    mapdata_animation();

    /* If we have new images to display, we need to do a complete redraw
     * periodically - to keep performance up, we don't want to do it every
     * tick, but every 5 (about half a second) is still pretty fast but should
     * also keep reasonable performance.
     */
    if (have_new_image && !(tick % 5)) {
        if (cpl.container) {
            cpl.container->inv_updated = 1;
        }
        cpl.ob->inv_updated = 1;

        have_new_image = 0;
        draw_map(1);
        draw_lists();
    } else {
        draw_map(0);
    }
}

/**
 * Called from disconnect command - that closes the socket - we just need to
 * do the gtk cleanup.
 */
void cleanup_connection() {
    if (csocket_fd) {
        gdk_input_remove(csocket_fd);
        csocket_fd = 0;
        gtk_main_quit();
    }
}

/**
 * Handles client shutdown.
 */
void on_window_destroy_event(GtkObject *object, gpointer user_data) {
#ifdef WIN32
    script_killall();
#endif

    LOG(LOG_INFO, "main.c::client_exit", "Exiting with return value 0.");
    exit(0);
}

/**
 * main loop iteration related stuff
 */
static void do_network() {
    fd_set tmp_read;
    int pollret;

    if (csocket.fd == -1) {
        if (csocket_fd) {
            gdk_input_remove(csocket_fd);
            csocket_fd = 0;
            gtk_main_quit();
        }
        return;
    }

    FD_ZERO(&tmp_read);
    FD_SET(csocket.fd, &tmp_read);
    script_fdset(&maxfd, &tmp_read);
    pollret = select(maxfd, &tmp_read, NULL, NULL, &timeout);
    if (pollret == -1) {
        LOG(LOG_WARNING, "main.c::do_network",
            "Got errno %d on select call.", errno);
    } else if (pollret > 0) {
        if (FD_ISSET(csocket.fd, &tmp_read)) {
            DoClient(&csocket);
#ifndef WIN32
            if (pollret > 1) {
                script_process(&tmp_read);
            }
#endif
        } else {
            script_process(&tmp_read);
        }
    }
    /* DoClient now closes the socket, so we need to check for this here -
     * with the socket being closed, this function will otherwise never be
     * called again. */
    if (csocket.fd == -1) {
        if (csocket_fd) {
            gdk_input_remove(csocket_fd);
            csocket_fd = 0;
            gtk_main_quit();
        }
        return;
    }
#ifdef HAVE_SDL
    if (use_config[CONFIG_DISPLAYMODE] == CFG_DM_SDL) {
        sdl_gen_map(FALSE);
    } else
#endif
#ifdef HAVE_OPENGL
        if (use_config[CONFIG_DISPLAYMODE] == CFG_DM_OPENGL) {
            opengl_gen_map(FALSE);
        } else
#endif
            draw_map(FALSE);

    draw_lists();
}

/**
 * Event loop iteration stuff
 */
static void event_loop() {
    if (MAX_TIME == 0) {
        timeout.tv_sec = 0;
        timeout.tv_usec = 0;
    }
    maxfd = csocket.fd + 1;

    if (MAX_TIME != 0) {
        timeout.tv_sec = 0;/* MAX_TIME / 1000000;*/
        timeout.tv_usec = 0;/* MAX_TIME % 1000000;*/
    }

    guint timeout_id = g_timeout_add(100, (GtkFunction) do_timeout, NULL);

#ifdef WIN32
    g_timeout_add(250, (GtkFunction) do_scriptout, NULL);
#endif

    if (csocket.fd == -1) {
        if (csocket_fd) {
            gdk_input_remove(csocket_fd);
            csocket_fd = 0;
            gtk_main_quit();
        }
        return;
    }
    csocket_fd = gdk_input_add((gint) csocket.fd, GDK_INPUT_READ,
            (GdkInputFunction)do_network, &csocket);
    int tag = csocket_fd;

    gtk_main();
    g_source_remove(tag);
    g_source_remove(timeout_id);

    LOG(LOG_INFO, "main.c::event_loop",
        "gtk_main exited, returning from event_loop");
}

#ifndef WIN32
/**
 * Handler for SIGPIPE.  We may receive this signal while piping data to a
 * sound server or to a script.  In both cases, we ignore the signal because
 * the failure will be reported by the system call that tried to send the
 * data.
 *
 * @param sig The signal number.
 */
static void sigpipe_handler(int sig) {
    /* ignore that signal for now */
}
#endif

/**
 * parse_args: Parses command line options, and does variable initialization.
 * @param argc
 * @param argv
 */

/**
 * Parse command-line arguments and store settings in want_config.
 *
 * This function should be called after config_load().
 */
static void parse_args(int argc, char *argv[]) {
    GOptionContext *context = g_option_context_new("- Crossfire GTKv2 Client");
    GError *error = NULL;

    g_option_context_add_main_entries(context, options, NULL);
    g_option_context_add_group(context, gtk_get_option_group(TRUE));

    if (!g_option_context_parse(context, &argc, &argv, &error)) {
        g_print("%s\n", error->message);
        g_error_free(error);
        exit(EXIT_FAILURE);
    }

    g_option_context_free(context);

    /*
     * Move this after the parsing of command line options, since that can
     * change the default log level.
     */
    LOG(LOG_INFO, "Client Version", VERSION_INFO);
}

/**
 * Display an error message dialog. The dialog contains a multi-line, bolded
 * heading that includes the client version information, an error description,
 * and information relevant to the error condition.
 *
 * @param description
 * A C-string, displayed in bold text, that describes the type of the error
 * condition.
 *
 * @param information
 * A C-string, displayed in normal text, that provides additional information
 * about the error condition.
 */
void error_dialog(char *description, char *information) {
    GtkWidget *dialog;

    gtk_init(NULL, NULL);
    dialog =
        gtk_message_dialog_new(NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
                               GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, "Crossfire %s\n%s",
                               VERSION_INFO, description);
    gtk_message_dialog_format_secondary_markup(GTK_MESSAGE_DIALOG(dialog),
            "%s", information);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

/**
 * This goes with the g_log_set_handler below in main().  I leave it here
 * since it may be useful - basically, it can prove handy to try and track
 * down error messages like:
 *
 * file gtklabel.c: line 1845: assertion `GTK_IS_LABEL (label)' failed
 *
 * In the debugger, you can set a breakpoint in this function, and then see
 * the stacktrace on what is trying to access a widget that isn't set or
 * otherwise having issues.
 */

void my_log_handler(const gchar *log_domain, GLogLevelFlags log_level,
                    const gchar *message, gpointer user_data) {
    g_usleep(1 * 1e6);
}

static void init_sockets() {
    /* Use the 'new' login method. */
    wantloginmethod = 2;

    csocket.inbuf.buf = g_malloc(MAXSOCKBUF);

#ifdef WIN32
    maxfd = 0; /* This is ignored on win32 platforms */

    /* This is required for sockets to be used under win32 */
    WORD Version = 0x0202;
    WSADATA wsaData;

    if (WSAStartup(Version, &wsaData) != 0) {
        LOG(LOG_CRITICAL, "main.c::main", "Could not load winsock!");
        exit(1);
    }
#else /* def WIN32 */
    signal(SIGPIPE, sigpipe_handler);
#ifdef HAVE_SYSCONF
    maxfd = sysconf(_SC_OPEN_MAX);
#else
    maxfd = getdtablesize();
#endif
#endif /* def WIN32 */
}

/**
 * Load the given layout file.
 */
static gboolean init_ui_layout(const char *name) {
    GString *path = g_string_new(XML_PATH_DEFAULT);
    gboolean result;

    strncpy(window_xml_file, name, sizeof(window_xml_file));

    g_string_append(path, name);
    result = gtk_builder_add_from_file(window_xml, path->str, NULL);
    g_string_free(path, TRUE);

    return result;
}

static void init_ui() {
    GError *error = NULL;
    GdkGeometry geometry;
    int i;

    /* Load dialog windows using GtkBuilder. */
    dialog_xml = gtk_builder_new();
    if (!gtk_builder_add_from_file(dialog_xml, dialog_xml_path, &error)) {
        error_dialog("Couldn't load UI dialogs.", error->message);
        g_warning("Couldn't load UI dialogs: %s", error->message);
        g_error_free(error);
        exit(EXIT_FAILURE);
    }

    /* Load main window using GtkBuilder. */
    window_xml = gtk_builder_new();

    /* Try to load default if selected layout doesn't work. */
    if (!init_ui_layout(window_xml_file)) {
        LOG(LOG_DEBUG, "init_ui", "Using default layout.");

        if (init_ui_layout("gtk-v2.ui") != TRUE) {
            g_error("Could not load default layout.");
        }
    }

    /* Begin connecting signals for the root window. */
    window_root = GTK_WIDGET(gtk_builder_get_object(window_xml, "window_root"));

    /* Request the window to receive focus in and out events */
    gtk_widget_add_events((gpointer) window_root, GDK_FOCUS_CHANGE_MASK);
    g_signal_connect((gpointer) window_root, "focus-out-event",
                     G_CALLBACK(focusoutfunc), NULL);

    g_signal_connect_swapped((gpointer) window_root, "key_press_event",
                             G_CALLBACK(keyfunc), GTK_OBJECT(window_root));
    g_signal_connect_swapped((gpointer) window_root, "key_release_event",
                             G_CALLBACK(keyrelfunc), GTK_OBJECT(window_root));
    g_signal_connect((gpointer) window_root, "destroy",
                     G_CALLBACK(on_window_destroy_event), NULL);

    /* Purely arbitrary min window size */
    geometry.min_width=640;
    geometry.min_height=480;

    gtk_window_set_geometry_hints(GTK_WINDOW(window_root), window_root,
                                  &geometry, GDK_HINT_MIN_SIZE);

    magic_map = GTK_WIDGET(gtk_builder_get_object(window_xml,
                "drawingarea_magic_map"));

    g_signal_connect((gpointer) magic_map, "expose_event",
                     G_CALLBACK(on_drawingarea_magic_map_expose_event), NULL);

    /* Set up colors before doing the other initialization functions */
    for (i = 0; i < NUM_COLORS; i++) {
        if (!gdk_color_parse(colorname[i], &root_color[i])) {
            fprintf(stderr, "gdk_color_parse failed (%s)\n", colorname[i]);
        }
        if (!gdk_color_alloc(gtk_widget_get_colormap(window_root),
                             &root_color[i])) {
            fprintf(stderr, "gdk_color_alloc failed\n");
        }
    }

    inventory_init(window_root);
    info_init(window_root);
    keys_init(window_root);
    stats_init(window_root);
    config_init(window_root);
    pickup_init(window_root);
    msgctrl_init(window_root);
    init_create_character_window();
    metaserver_ui_init();

    load_window_positions(window_root);

    init_theme();
    load_theme(TRUE);
    init_menu_items();
}

/**
 * Main client entry point.
 */
int main(int argc, char *argv[]) {
    // Whether or not to use the metaserver pop-up dialog.
    bool use_metaserver = true;

#ifdef ENABLE_NLS
    bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
    bindtextdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
    textdomain(GETTEXT_PACKAGE);
#endif

    // Initialize GTK and client library.
    gtk_init(&argc, &argv);
    client_init();

    // Set defaults, load configuration, and parse arguments.
    snprintf(VERSION_INFO, MAX_BUF, "GTKv2 Client " FULL_VERSION);
    use_config[CONFIG_MAPWIDTH] = 25;
    use_config[CONFIG_MAPHEIGHT] = 25;

    config_load();
    parse_args(argc, argv);
    config_check();

    // Initialize UI, sockets, and sound server.
    init_ui();
    init_sockets();

    if (init_sounds() == -1) {
        use_config[CONFIG_SOUND] = FALSE;
    } else {
        use_config[CONFIG_SOUND] = TRUE;
    }

    /* Load cached pixmaps. */
    init_image_cache_data();

    /* Show main client window as late as possible. */
    gtk_widget_show(window_root);
    map_init(window_root);

    /* Loop to connect to server/metaserver and play the game */
    while (use_metaserver) {
        clear_stat_mapping();

        /* Pick a server from the list if not specified on the command line. */
        if (server == NULL) {
            prompt_metaserver();
        } else {
            use_metaserver = false;

            csocket.fd = init_connection(server, use_config[CONFIG_PORT]);
            g_free(server);

            // Exit with an error if unable to connect to server.
            if (csocket.fd == -1) {
                g_error("Unable to connect to server.");
            }
        }

        negotiate_connection(use_config[CONFIG_SOUND]);

        /* The event_loop will block until connection to the server is lost. */
        event_loop();

        remove_item_inventory(cpl.ob);
        /*
         * We know the following is the private map structure in item.c.  But
         * we don't have direct access to it, so we still use locate.
         */
        remove_item_inventory(locate_item(0));
        draw_look_list();

        /*
         * Need to reset the images so they match up properly and prevent
         * memory leaks.
         */
        reset_image_data();
        client_reset();
    }
}

/**
 * Gets the coordinates of a specified window.
 *
 * @param win Pass in a GtkWidget pointer to get its coordinates.
 * @param x Parent-relative window x coordinate
 * @param y Parent-relative window y coordinate
 * @param wx ?
 * @param wy ?
 * @param w Window width
 * @param h Window height
 */
void get_window_coord(GtkWidget *win, int *x, int *y, int *wx, int *wy,
        int *w, int *h) {
    /* Position of a window relative to its parent window. */
    gdk_window_get_geometry(win->window, x, y, w, h, NULL);
    /* Position of the window in root window coordinates. */
    gdk_window_get_origin(win->window, wx, wy);
    *wx -= *x;
    *wy -= *y;
}
