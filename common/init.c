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
 * @file common/init.c
 * Functions for initializing the client.
 */

#include "client.h"
#include "p_cmd.h"

/* Makes the load/save code trivial - basically, the
 * entries here match the same numbers as the CONFIG_ values defined
 * in common/client.h - this means the load and save just does
 * something like a fprintf(outifle, "%s: %d", config_names[i],
 *			    want_config[i]);
 */
const char *const config_names[CONFIG_NUMS] = {
    NULL, "download_all_images", "echo_bindings",
    "fasttcpsend", "command_window", "cacheimages", "fog_of_war", "iconscale",
    "mapscale", "popups", "displaymode", "showicon", "tooltips", "sound", "splitinfo",
    "split", "show_grid", "lighting", "trim_info_window",
    "map_width", "map_height", "foodbeep", "darkness", "port",
    "grad_color_bars", "resistances", "smoothing", "nosplash",
    "auto_apply_container", "mapscroll", "sign_popups", "message_timestamping"
};

gint16 want_config[CONFIG_NUMS], use_config[CONFIG_NUMS];

#define FREE_AND_CLEAR(xyz) { free(xyz); xyz=NULL; }

void VersionCmd(char *data, int len) {
    char *cp;

    csocket.cs_version = atoi(data);
    /* set sc_version in case it is an old server supplying only one version */
    csocket.sc_version = csocket.cs_version;
    if (csocket.cs_version != VERSION_CS) {
        LOG(LOG_WARNING, "common::VersionCmd", "Differing C->S version numbers (%d,%d)",
            VERSION_CS, csocket.cs_version);
        /*	exit(1);*/
    }
    cp = strchr(data, ' ');
    if (!cp) {
        return;
    }
    csocket.sc_version = atoi(cp);
    if (csocket.sc_version != VERSION_SC) {
        LOG(LOG_WARNING, "common::VersionCmd", "Differing S->C version numbers (%d,%d)",
            VERSION_SC, csocket.sc_version);
    }
    cp = strchr(cp + 1, ' ');
    if (cp) {
        LOG(LOG_INFO, "common::VersionCmd", "Playing on server type %s", cp);
    }
}

void SendVersion(ClientSocket csock) {
    cs_print_string(csock.fd, "version %d %d %s",
            VERSION_CS, VERSION_SC, VERSION_INFO);
}

void SendAddMe(ClientSocket csock) {
    cs_print_string(csock.fd, "addme");
}

static void init_paths() {
    // Set and create configuration and cache directories.
    GString *app_config_dir = g_string_new(g_get_user_config_dir());
    g_string_append(app_config_dir, "/crossfire");
    config_dir = g_string_free(app_config_dir, FALSE);
    g_mkdir_with_parents(config_dir, 0755);

    GString *app_cache_dir = g_string_new(g_get_user_cache_dir());
    g_string_append(app_cache_dir, "/crossfire");
    cache_dir = g_string_free(app_cache_dir, FALSE);
    g_mkdir_with_parents(cache_dir, 0755);
}

/**
 * Initialize or reset client variables. This function is called by
 * client_init() and client_reset().
 */
static void reset_vars_common() {
    cpl.count_left = 0;
    cpl.container = NULL;
    memset(&cpl.stats, 0, sizeof(Stats));
    cpl.stats.maxsp = 1;	/* avoid div by 0 errors */
    cpl.stats.maxhp = 1;	/* ditto */
    cpl.stats.maxgrace = 1;	/* ditto */

    /* ditto - displayed weapon speed is weapon speed/speed */
    cpl.stats.speed = 1;
    cpl.input_text[0] = '\0';
    cpl.title[0] = '\0';
    cpl.range[0] = '\0';
    cpl.last_command[0] = '\0';

    for (int i = 0; i < range_size; i++) {
        cpl.ranges[i] = NULL;
    }

    csocket.command_sent = 0;
    csocket.command_received = 0;
    csocket.command_time = 0;

    cpl.magicmap = NULL;
    cpl.showmagic = 0;

    face_info.bmaps_checksum = 0;
    face_info.cache_hits = 0;
    face_info.cache_misses = 0;
    face_info.faceset = 0;
    face_info.have_faceset_info = 0;
    face_info.num_images = 0;

    stat_points = 0;
    stat_min = 0;
    stat_maximum = 0;

    mapdata_init();
    reset_player_data();
}

/**
 * Initialize client settings with built-in defaults.
 */
static void init_config() {
    want_config[CONFIG_APPLY_CONTAINER] = TRUE;
    want_config[CONFIG_CACHE] = FALSE;
    want_config[CONFIG_CWINDOW] = COMMAND_WINDOW;
    want_config[CONFIG_DARKNESS] = TRUE;
    want_config[CONFIG_DISPLAYMODE] = CFG_DM_PIXMAP;
    want_config[CONFIG_DOWNLOAD] = FALSE;
    want_config[CONFIG_ECHO] = FALSE;
    want_config[CONFIG_FASTTCP] = TRUE;
    want_config[CONFIG_FOGWAR] = TRUE;
    want_config[CONFIG_FOODBEEP] = FALSE;
    want_config[CONFIG_GRAD_COLOR] = FALSE;
    want_config[CONFIG_ICONSCALE] = 100;
    want_config[CONFIG_LIGHTING] = CFG_LT_TILE;
    want_config[CONFIG_MAPHEIGHT] = 20;
    want_config[CONFIG_MAPSCALE] = 100;
    want_config[CONFIG_MAPSCROLL] = TRUE;
    want_config[CONFIG_MAPWIDTH] = 20;
    want_config[CONFIG_POPUPS] = FALSE;
    want_config[CONFIG_PORT] = EPORT;
    want_config[CONFIG_RESISTS] = 0;
    want_config[CONFIG_RESISTS] = 0;
    want_config[CONFIG_SHOWGRID] = FALSE;
    want_config[CONFIG_SHOWICON] = FALSE;
    want_config[CONFIG_SIGNPOPUP] = TRUE;
    want_config[CONFIG_SMOOTH] = 0;
    want_config[CONFIG_SOUND] = TRUE;
    want_config[CONFIG_SPLASH] = TRUE;
    want_config[CONFIG_SPLITINFO] = FALSE;
    want_config[CONFIG_SPLITWIN] = FALSE;
    want_config[CONFIG_TIMESTAMP] = FALSE;
    want_config[CONFIG_TOOLTIPS] = TRUE;
    want_config[CONFIG_TRIMINFO] = FALSE;

    for (int i = 0; i < CONFIG_NUMS; i++) {
        use_config[i] = want_config[i];
    }
}

/**
 * Called ONCE during client startup to initialize configuration and other
 * variables to reasonable defaults. Future resets (i.e. after a connection)
 * should use client_reset() instead.
 */
void client_init() {
    // Initialize experience tables.
    exp_table = NULL;
    exp_table_max = 0;

    last_used_skills[MAX_SKILL] = -1;

    // Clear out variables related to image face caching.
    face_info.old_bmaps_checksum = 0;
    face_info.want_faceset = NULL;

    for (int i = 0; i < MAX_FACE_SETS; i++) {
        face_info.facesets[i].prefix = NULL;
        face_info.facesets[i].fullname = NULL;
        face_info.facesets[i].fallback = 0;
        face_info.facesets[i].size = NULL;
        face_info.facesets[i].extension = NULL;
        face_info.facesets[i].comment = NULL;
    }

    // Allocate memory for player-related objects.
    cpl.ob = player_item();
    cpl.below = map_item();

    reset_vars_common();

    for (int i = 0; i < MAX_SKILL; i++) {
        skill_names[i] = NULL;
        last_used_skills[i] = -1;
    }

    init_commands();
    init_config();
    init_metaserver();
    init_paths();

    // Seed pseudo-random number generator.
    srandom(time(NULL));
}

/**
 * Reset player experience data.
 */
void reset_player_data() {
    int i;

    for (i = 0; i < MAX_SKILL; i++) {
        cpl.stats.skill_exp[i] = 0;
        cpl.stats.skill_level[i] = 0;
    }
}

/**
 * Clear client variables between connections to different servers. This MUST
 * be called AFTER client_init() because that performs some allocations.
 */
void client_reset() {
    int i;

    /* Keep old checksum to compare it with the next server's checksum. */
    face_info.old_bmaps_checksum = face_info.bmaps_checksum;

    for (i = 0; i < MAX_FACE_SETS; i++) {
        FREE_AND_CLEAR(face_info.facesets[i].prefix);
        FREE_AND_CLEAR(face_info.facesets[i].fullname);
        face_info.facesets[i].fallback = 0;
        FREE_AND_CLEAR(face_info.facesets[i].size);
        FREE_AND_CLEAR(face_info.facesets[i].extension);
        FREE_AND_CLEAR(face_info.facesets[i].comment);
    }

    reset_vars_common();

    for (i = 0; i < MAX_SKILL; i++) {
        FREE_AND_CLEAR(skill_names[i]);
    }

    if (motd) {
        FREE_AND_CLEAR(motd);
    }

    if (news) {
        FREE_AND_CLEAR(news);
    }

    if (rules) {
        FREE_AND_CLEAR(rules);
    }

    if (races) {
        free_all_race_class_info(races, num_races);
        num_races = 0;
        used_races = 0;
        races = NULL;
    }

    if (classes) {
        free_all_race_class_info(classes, num_classes);
        num_classes = 0;
        used_classes = 0;
        classes = NULL;
    }

    serverloginmethod = 0;
}
