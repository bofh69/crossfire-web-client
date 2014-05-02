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
 * @file main.c
 * Implements client startup functions. Command-line parameters are parsed and
 * handled. GtkBuilder XML layout files are loaded. Windows and dialogs are
 * initialized. The server connection is managed.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#include <windows.h>
#else
#include <signal.h>
#endif

#include <errno.h>
#include <gtk/gtk.h>
#include <stdio.h>

#include "client.h"
#include "main.h"
#include "image.h"
#include "gtk2proto.h"
#include "script.h"
#include "metaserver.h"
#include "mapdata.h"

GtkWidget *window_root, *magic_map;
GtkBuilder *dialog_xml, *window_xml;

/* Sets up the basic colors. */
const char *const colorname[NUM_COLORS] = {
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

/* These are the names as set by the user within the rc file.
 * We use lower case to be consistent, but also change the names
 * to be more generic instead of specific X11 color names.
 */
const char *const usercolorname[NUM_COLORS] = {
    "black",                /* 0  */
    "white",                /* 1  */
    "darkblue",             /* 2  */
    "red",                  /* 3  */
    "orange",               /* 4  */
    "lightblue",            /* 5  */
    "darkorange",           /* 6  */
    "green",                /* 7  */
    "darkgreen",            /* 8  *//* Used for window background color */
    "grey",                 /* 9  */
    "brown",                /* 10 */
    "yellow",               /* 11 */
    "tan"                   /* 12 */
};

char dialog_xml_file[MAX_BUF] = DIALOG_XML_FILENAME;
char dialog_xml_path[MAX_BUF] = "";     /**< Dialog layout file with path. */
/** The file name of the window layout in use by the client. The base name,
 * without dot extention, is re-used when saving the window positions. */
char window_xml_file[MAX_BUF];
char window_xml_path[MAX_BUF] = "";     /**< Window layout file with path. */
GdkColor root_color[NUM_COLORS];
struct timeval timeout;
extern int maxfd;
gint    csocket_fd = 0;
static uint8 updatekeycodes = FALSE;
extern int time_map_redraw;

#ifdef WIN32 /* Win32 scripting support */
#define PACKAGE_DATA_DIR "."

int do_scriptout() {
    script_process(NULL);
    return (TRUE);
}
#endif /* WIN32 */

/**
 * Map, spell, and inventory maintenance.
 * @return TRUE
 */
int do_timeout() {
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
    return TRUE;
}

/**
 * X11 client doesn't care about this
 */
void client_tick(uint32 tick) {
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
void do_network() {
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
void event_loop() {
    extern int do_timeout(void);
    int tag;

    if (MAX_TIME == 0) {
        timeout.tv_sec = 0;
        timeout.tv_usec = 0;
    }
    maxfd = csocket.fd + 1;

    if (MAX_TIME != 0) {
        timeout.tv_sec = 0;/* MAX_TIME / 1000000;*/
        timeout.tv_usec = 0;/* MAX_TIME % 1000000;*/
    }

    gtk_timeout_add(10, (GtkFunction) do_timeout, NULL);

#ifdef WIN32
    gtk_timeout_add(25, (GtkFunction) do_scriptout, NULL);
#endif

    if (csocket.fd == -1) {
        if (csocket_fd) {
            gdk_input_remove(csocket_fd);
            csocket_fd = 0;
            gtk_main_quit();
        }
        return;
    }
    csocket_fd = gdk_input_add((gint) csocket.fd,
                               GDK_INPUT_READ,
                               (GdkInputFunction) do_network, &csocket);
    tag = csocket_fd;

    gtk_main();
    gtk_timeout_remove(tag);

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
 * Usage routine.  All clients should support server, port and display
 * options, with -pix and -xpm also suggested.  -split does not need to be
 * supported - it is in this copy because the old code supported it.
 * @param *progname Not used, but should be.
 */
static void usage(char *progname) {
    puts("Usage of crossfire-client-gtk2:\n");
    puts("-cache           - Cache images for future use.");
    puts("-nocache         - Do not cache images (default action).");
    puts("-darkness        - Enables darkness code (default)");
    puts("-nodarkness      - Disables darkness code");
    puts("-display <name>  - Use <name> instead if DISPLAY environment variable.");
    puts("-download_all_faces - Download all needed faces before play starts");
    puts("-echo            - Echo the bound commands");
    puts("-noecho          - Do not echo the bound commands (default)");
    puts("-faceset <name>  - Use faceset <name> if available");
    puts("-fasttcpsend     - Send data immediately to server, may increase bandwidth");
    puts("-nofasttcpsend   - Disables fasttcpsend");
    puts("-fog             - Enable fog of war code");
    puts("-help            - Display this message.");
    puts("-loglevel <val>  - Set default logging level (0 is most verbose)");
    puts("-iconscale %%    - Set icon scale percentage");
    puts("-mapscale %%     - Set map scale percentage");
    puts("-mapsize xXy     - Set the mapsize to be X by Y spaces. (default 11x11)");
    puts("-splash          - Display the splash screen (default)");
    puts("-nosplash        - Don't display the splash screen (startup logo)");
    puts("-opengl          - Use opengl drawing code");
    puts("-pixmap          - Use pixmap drawing code");
    puts("-port <number>   - Use port <number> instead of the standard port number");
    puts("-sdl             - Use sdl for drawing png (may not work on all hardware");
    puts("-server <name>   - Connect to <name> instead of localhost.");
    puts("-showicon        - Print status icons in inventory window");
    puts("-smooth          - Enable smooth");
    puts("-nosmooth        - Disable smooth (default)");
    puts("-sound           - Enable sound output (default).");
    puts("-nosound         - Disable sound output.");
    puts("-sound_server <path> - Executable to use to play sounds.");
    puts("-resists <val>   - Control look of resistances.");
    puts("-split           - Use split windows.");
    puts("-splitinfo       - Use two information windows, segregated by information type.");
    puts("-timemapredraw   - Print out timing information for map generation");
    puts("-triminfowindow  - Trims size of information window(s)");
    puts("-notriminfowindow  - Do not trims size of information window(s) (default)");
    puts("-updatekeycodes  - Update the saved bindings for this keyboard.");
    puts("-window_xml <file> - Glade Designer client UI layout XML file.");
    puts("-dialog_xml <file> - Glade Designer popup dialog XML file.");

    exit(0);
}

/**
 * parse_args: Parses command line options, and does variable initialization.
 * @param argc
 * @param argv
 * @return Returns 0 on success, nonzero on failure.
 */
static int parse_args(int argc, char *argv[]) {
    int on_arg = 1;

    snprintf(VERSION_INFO, MAX_BUF, "GTKv2 Client %s", FULL_VERSION);
    config_load();

    for (on_arg = 1; on_arg < argc; on_arg++) {
        if (!strcmp(argv[on_arg], "-cache")) {
            want_config[CONFIG_CACHE] = TRUE;
            continue;
        } else if (!strcmp(argv[on_arg], "-nocache")) {
            want_config[CONFIG_CACHE] = FALSE;
            continue;
        } else if (!strcmp(argv[on_arg], "-darkness")) {
            want_config[CONFIG_DARKNESS] = TRUE;
            continue;
        } else if (!strcmp(argv[on_arg], "-nodarkness")) {
            want_config[CONFIG_DARKNESS] = FALSE;
            continue;
        } else if (!strcmp(argv[on_arg], "-display")) {
            if (++on_arg == argc) {
                LOG(LOG_WARNING, "main.c::init_windows",
                    "-display requires a display name");
                return 1;
            }
            continue;
        } else if (!strcmp(argv[on_arg], "-download_all_faces")) {
            want_config[CONFIG_DOWNLOAD] = TRUE;
            continue;
        } else if (!strcmp(argv[on_arg], "-echo")) {
            want_config[CONFIG_ECHO] = TRUE;
            continue;
        } else if (!strcmp(argv[on_arg], "-noecho")) {
            want_config[CONFIG_ECHO] = FALSE;
            continue;
        } else if (!strcmp(argv[on_arg], "-faceset")) {
            if (++on_arg == argc) {
                LOG(LOG_WARNING, "main.c::init_windows",
                    "-faceset requires a faceset name/number");
                return 1;
            }
            face_info.want_faceset = argv[on_arg];
            continue;
        } else if (!strcmp(argv[on_arg], "-fog")) {
            want_config[CONFIG_FOGWAR] = TRUE;
            continue;
        } else if (!strcmp(argv[on_arg], "-nofog")) {
            want_config[CONFIG_FOGWAR] = FALSE;
            continue;
        } else if (!strcmp(argv[on_arg], "-help")) {
            usage(argv[0]);
            continue;
        } else if (!strcmp(argv[on_arg], "-iconscale")) {
            if (++on_arg == argc) {
                LOG(LOG_WARNING, "main.c::init_windows",
                    "-iconscale requires a percentage value");
                return 1;
            }
            want_config[CONFIG_ICONSCALE] = atoi(argv[on_arg]);
            if (want_config[CONFIG_ICONSCALE] < 25 || want_config[CONFIG_ICONSCALE] > 200) {
                LOG(LOG_WARNING, "main.c::init_windows",
                    "Valid range for -iconscale is 25 through 200");
                want_config[CONFIG_ICONSCALE] = 100;
                return 1;
            }
            continue;
        } else if (!strcmp(argv[on_arg], "-mapscale")) {
            if (++on_arg == argc) {
                LOG(LOG_WARNING, "main.c::init_windows",
                    "-mapscale requires a percentage value");
                return 1;
            }
            want_config[CONFIG_MAPSCALE] = atoi(argv[on_arg]);
            if (want_config[CONFIG_MAPSCALE] < 25 || want_config[CONFIG_MAPSCALE] > 200) {
                LOG(LOG_WARNING, "main.c::init_windows",
                    "Valid range for -mapscale is 25 through 200");
                want_config[CONFIG_MAPSCALE] = 100;
                return 1;
            }
            continue;
        } else if (!strcmp(argv[on_arg], "-mapsize")) {
            char *cp, x, y = 0;

            if (++on_arg == argc) {
                LOG(LOG_WARNING, "main.c::init_windows",
                    "-mapsize requires a XxY value");
                return 1;
            }
            x = atoi(argv[on_arg]);
            for (cp = argv[on_arg]; *cp != '\0'; cp++)
                if (*cp == 'x' || *cp == 'X') {
                    break;
                }

            if (*cp == 0) {
                LOG(LOG_WARNING, "main.c::init_windows", "-mapsize requires "
                    "both X and Y values (ie, XxY - note the\nx in between.");
            } else {
                y = atoi(cp + 1);
            }
            if (x < 9 || y < 9) {
                LOG(LOG_WARNING, "main.c::init_windows",
                    "Map size must be positive values of at least 9");
            } else if (x > MAP_MAX_SIZE || y > MAP_MAX_SIZE) {
                LOG(LOG_WARNING, "main.c::init_windows", "Map size cannot be "
                    "larger than %d x %d", MAP_MAX_SIZE, MAP_MAX_SIZE);

            } else {
                want_config[CONFIG_MAPWIDTH] = x;
                want_config[CONFIG_MAPHEIGHT] = y;
            }
            continue;
        } else if (!strcmp(argv[on_arg], "-fasttcpsend")) {
            want_config[CONFIG_FASTTCP] = TRUE;
            continue;
        } else if (!strcmp(argv[on_arg], "-nofasttcpsend")) {
            want_config[CONFIG_FASTTCP] = FALSE;
            continue;
        } else if (!strcmp(argv[on_arg], "-opengl")) {
#ifndef HAVE_OPENGL
            LOG(LOG_WARNING, "main.c::init_windows", "client not compiled "
                "with opengl support.  Ignoring -opengl");
#else
            want_config[CONFIG_DISPLAYMODE] = CFG_DM_OPENGL;
#endif
            continue;
        } else if (!strcmp(argv[on_arg], "-pixmap")) {
            want_config[CONFIG_DISPLAYMODE] = CFG_DM_PIXMAP;
        } else if (!strcmp(argv[on_arg], "-port")) {
            if (++on_arg == argc) {
                LOG(LOG_WARNING, "main.c::init_windows",
                    "-port requires a port number");
                return 1;
            }
            want_config[CONFIG_PORT] = atoi(argv[on_arg]);
            continue;
        } else if (!strcmp(argv[on_arg], "-sdl")) {
#ifndef HAVE_SDL
            LOG(LOG_WARNING, "main.c::init_windows",
                "client not compiled with sdl support.  Ignoring -sdl");
#else
            want_config[CONFIG_DISPLAYMODE] = CFG_DM_SDL;
#endif
            continue;
        } else if (!strcmp(argv[on_arg], "-server")) {
            if (++on_arg == argc) {
                LOG(LOG_WARNING, "main.c::init_windows",
                    "-server requires a host name");
                return 1;
            }
            server = argv[on_arg];
            continue;
        } else if (!strcmp(argv[on_arg], "-showicon")) {
            want_config[CONFIG_SHOWICON] = TRUE;
            continue;
        } else if (!strcmp(argv[on_arg], "-smooth")) {
            want_config[CONFIG_SMOOTH] = TRUE;
        } else if (!strcmp(argv[on_arg], "-nosmooth")) {
            want_config[CONFIG_SMOOTH] = FALSE;
        } else if (!strcmp(argv[on_arg], "-sound")) {
            want_config[CONFIG_SOUND] = TRUE;
            continue;
        } else if (!strcmp(argv[on_arg], "-nosound")) {
            want_config[CONFIG_SOUND] = FALSE;
            continue;
        } else if (!strcmp(argv[on_arg], "-sound_server")) {
            if (++on_arg == argc) {
                LOG(LOG_WARNING, "main.c::init_windows",
                    "-sound_server requires an executable pathname");
                return 1;
            }
            sound_server = argv[on_arg];
            continue;
        } else if (!strcmp(argv[on_arg], "-split")) {
            want_config[CONFIG_SPLITWIN] = TRUE;
            continue;
        } else if (!strcmp(argv[on_arg], "-nosplit")) {
            want_config[CONFIG_SPLITWIN] = FALSE;
            continue;
        } else if (!strcmp(argv[on_arg], "-resists")) {
            if (++on_arg == argc) {
                LOG(LOG_WARNING, "main.c::init_windows",
                    "-resists requires a value");
                return 1;
            }
            want_config[CONFIG_RESISTS] = atoi(argv[on_arg]);
            continue;
        } else if (!strcmp(argv[on_arg], "-loglevel")) {
            extern int MINLOG;

            if (++on_arg == argc) {
                LOG(LOG_WARNING, "main.c::init_windows",
                    "-loglevel requires a value");
                return 1;
            }
            MINLOG = atoi(argv[on_arg]);
            continue;
        } else if (!strcmp(argv[on_arg], "-splitinfo")) {
            want_config[CONFIG_SPLITINFO] = TRUE;
            continue;
        } else if (!strcmp(argv[on_arg], "-timemapredraw")) {
            time_map_redraw = TRUE;
            continue;
        } else if (!strcmp(argv[on_arg], "-triminfowindow")) {
            want_config[CONFIG_TRIMINFO] = TRUE;
            continue;
        } else if (!strcmp(argv[on_arg], "-notriminfowindow")) {
            want_config[CONFIG_TRIMINFO] = FALSE;
            continue;
        } else if (!strcmp(argv[on_arg], "-updatekeycodes")) {
            updatekeycodes = TRUE;
            continue;
        } else if (!strcmp(argv[on_arg], "-splash")) {
            want_config[CONFIG_SPLASH] = TRUE;
            continue;
        } else if (!strcmp(argv[on_arg], "-nosplash")) {
            want_config[CONFIG_SPLASH] = FALSE;
            continue;
        } else if (!strcmp(argv[on_arg], "-window_xml")) {
            if (++on_arg == argc) {
                LOG(LOG_WARNING, "main.c::init_windows",
                    "-window_xml requires a xml file name");
                return 1;
            }
            strncpy(window_xml_path, argv[on_arg], MAX_BUF - 1);
            continue;
        } else if (!strcmp(argv[on_arg], "-dialog_xml")) {
            if (++on_arg == argc) {
                LOG(LOG_WARNING, "main.c::init_windows",
                    "-dialog_xml requires a xml file name");
                return 1;
            }
            strncpy(dialog_xml_path, argv[on_arg], MAX_BUF - 1);
            continue;
        } else {
            LOG(LOG_WARNING, "main.c::init_windows",
                "Do not understand option %s", argv[on_arg]);
            usage(argv[0]);
            return 1;
        }
    }

    /*
     * Move this after the parsing of command line options, since that can
     * change the default log level.
     */
    LOG(LOG_INFO, "Client Version", VERSION_INFO);

    /* Now copy over the values just loaded */
    for (on_arg = 0; on_arg < CONFIG_NUMS; on_arg++) {
        use_config[on_arg] = want_config[on_arg];
    }

    image_size = DEFAULT_IMAGE_SIZE * use_config[CONFIG_ICONSCALE] / 100;
    map_image_size = DEFAULT_IMAGE_SIZE * use_config[CONFIG_MAPSCALE] / 100;
    map_image_half_size = DEFAULT_IMAGE_SIZE * use_config[CONFIG_MAPSCALE] / 200;
    if (!use_config[CONFIG_CACHE]) {
        use_config[CONFIG_DOWNLOAD] = FALSE;
    }

    mapdata_init();

    return 0;
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
    sleep(1);
}

static void init_sockets() {
    /* Use the 'new' login method. */
    wantloginmethod = 2;

    csocket.inbuf.buf = malloc(MAXSOCKBUF);

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

static void init_ui() {
    GError *error = NULL;
    GdkGeometry geometry;
    int i;

    /* Set path to the UI files if they weren't set from the command line. */
    if (dialog_xml_path[0] == '\0') {
        snprintf(dialog_xml_path, sizeof(dialog_xml_path), "%s%s",
                XML_PATH_DEFAULT, dialog_xml_file);
    }

    if (window_xml_path[0] == '\0') {
        snprintf(window_xml_path, sizeof(window_xml_path), "%s%s",
                XML_PATH_DEFAULT, window_xml_file);
    }

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
    if (!gtk_builder_add_from_file(window_xml, window_xml_path, &error)) {
        LOG(LOG_WARNING, "main.c::init_ui",
                "Couldn't load '%s'; using default.", window_xml_path);
        error = NULL;

        snprintf(window_xml_path, sizeof(window_xml_path),
                XML_PATH_DEFAULT "gtk-v2.ui");

        if (!gtk_builder_add_from_file(window_xml, window_xml_path, &error)) {
            error_dialog("Couldn't load client window.", error->message);
            g_error_free(error);
            exit(EXIT_FAILURE);
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

    load_window_positions(window_root);

    init_theme();
    load_theme(TRUE);
    init_menu_items();
}

/**
 * Main client entry point.
 */
int main(int argc, char *argv[]) {
#ifdef ENABLE_NLS
    bindtextdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
    bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
    textdomain(GETTEXT_PACKAGE);
#endif

    gtk_init(&argc, &argv);

    /* Initialize client configuration to something reasonable. */
    init_client_vars();
    use_config[CONFIG_MAPWIDTH] = want_config[CONFIG_MAPWIDTH] = 25;
    use_config[CONFIG_MAPHEIGHT] = want_config[CONFIG_MAPHEIGHT] = 25;

    /* This MUST come after init_client_vars(). */
    parse_args(argc, argv);

    /* Initialize UI. */
    init_ui();

    /* Initialize sockets. */
    init_sockets();

    /* Initialize sound server. */
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
    while (1) {
        reset_client_vars();
        clear_stat_mapping();
        csocket.inbuf.len = 0;
        csocket.cs_version = 0;

        /* Pick a server from the list if not specified on the command line. */
        if (server == NULL) {
            draw_splash();
            metaserver_get_info(meta_server, meta_port);
            get_metaserver();
        } else {
            csocket.fd = init_connection(server, use_config[CONFIG_PORT]);

            /* Set server back to NULL so metaserver is used the next time. */
            server = NULL;

            /* If unable to connect to server, return to server selection. */
            if (csocket.fd == -1) {
                continue;
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

        mapdata_reset();
        /*
         * Need to reset the images so they match up properly and prevent
         * memory leaks.
         */
        reset_image_data();
    }

    /* This statement should never be reached. */
    exit(EXIT_SUCCESS);
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
