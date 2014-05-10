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
 * Handles the initialization of the client.  This includes making the I_IMAGE
 * and I_ARCH commands.
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

sint16 want_config[CONFIG_NUMS], use_config[CONFIG_NUMS];

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


void init_client_vars() {
    char buf[FILENAME_MAX];
    int i;

    if (exp_table) {
        free(exp_table);
        exp_table = NULL;
    }
    exp_table_max = 0;

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

    for (i = 0; i < range_size; i++) {
        cpl.ranges[i] = NULL;
    }

    for (i = 0; i < MAX_SKILL; i++) {
        cpl.stats.skill_exp[i] = 0;
        cpl.stats.skill_level[i] = 0;
        skill_names[i] = NULL;
        last_used_skills[i] = -1;
    }
    last_used_skills[MAX_SKILL] = -1;

    cpl.ob = player_item();
    cpl.below = map_item();
    cpl.magicmap = NULL;
    cpl.showmagic = 0;

    csocket.command_sent = 0;
    csocket.command_received = 0;
    csocket.command_time = 0;

    face_info.faceset = 0;
    face_info.num_images = 0;
    face_info.bmaps_checksum = 0;
    face_info.old_bmaps_checksum = 0;
    face_info.want_faceset = NULL;
    face_info.cache_hits = 0;
    face_info.cache_misses = 0;
    face_info.have_faceset_info = 0;

    for (i = 0; i < MAX_FACE_SETS; i++) {
        face_info.facesets[i].prefix = NULL;
        face_info.facesets[i].fullname = NULL;
        face_info.facesets[i].fallback = 0;
        face_info.facesets[i].size = NULL;
        face_info.facesets[i].extension = NULL;
        face_info.facesets[i].comment = NULL;
    }

    use_config[CONFIG_DOWNLOAD] = FALSE;
    use_config[CONFIG_ECHO] = FALSE;
    use_config[CONFIG_FASTTCP] = TRUE;
    use_config[CONFIG_CWINDOW] = COMMAND_WINDOW;
    use_config[CONFIG_CACHE] = FALSE;
    use_config[CONFIG_FOGWAR] = TRUE;
    use_config[CONFIG_ICONSCALE] = 100;
    use_config[CONFIG_MAPSCALE] = 100;
    use_config[CONFIG_POPUPS] = FALSE;
    use_config[CONFIG_DISPLAYMODE] = CFG_DM_PIXMAP;
    use_config[CONFIG_SHOWICON] = FALSE;
    use_config[CONFIG_TOOLTIPS] = TRUE;
    use_config[CONFIG_SOUND] = TRUE;
    use_config[CONFIG_SPLITINFO] = FALSE;
    use_config[CONFIG_SPLITWIN] = FALSE;
    use_config[CONFIG_SHOWGRID] = FALSE;
    use_config[CONFIG_LIGHTING] = CFG_LT_TILE;
    use_config[CONFIG_TRIMINFO] = FALSE;
    use_config[CONFIG_MAPWIDTH] = 11;
    use_config[CONFIG_MAPHEIGHT] = 11;
    use_config[CONFIG_FOODBEEP] = FALSE;
    use_config[CONFIG_DARKNESS] = TRUE;
    use_config[CONFIG_PORT] = EPORT;
    use_config[CONFIG_GRAD_COLOR] = FALSE;
    use_config[CONFIG_RESISTS] = 0;
    use_config[CONFIG_RESISTS] = 0;
    use_config[CONFIG_SMOOTH] = 0;
    use_config[CONFIG_SPLASH] = TRUE;
    use_config[CONFIG_APPLY_CONTAINER] = TRUE;
    use_config[CONFIG_MAPSCROLL] = TRUE;
    use_config[CONFIG_SIGNPOPUP] = TRUE;
    use_config[CONFIG_TIMESTAMP] = FALSE;

#ifdef WIN32
    /* If HOME is not set, set it to the current directory. */
    if (!getenv("HOME")) {
        if (getenv("APPDATA")) {
            char env[ MAX_BUF ];
            _snprintf(env, MAX_BUF, "HOME=%s", getenv("APPDATA"));
            LOG(LOG_INFO, "common::init.c", "init_client_vars: HOME set to %APPDATA%.\n");
            putenv(env);
        } else {
            LOG(LOG_INFO, "common::init.c",
                "init_client_vars: HOME not set, setting it to .\n");
            putenv("HOME=.");
        }
    }
#endif

    /* Initialize XDG base directories. */
    if ((xdg_config_dir = getenv("XDG_CONFIG_HOME")) == NULL) {
        xdg_config_dir = ".config";
    }

    if ((xdg_cache_dir = getenv("XDG_CACHE_HOME")) == NULL) {
        xdg_cache_dir = ".cache";
    }

    /* Right now config dir is not used, so don't bother creating it. */
    snprintf(buf, sizeof(buf), "%s/%s/crossfire", getenv("HOME"),
            xdg_config_dir);
    /* make_path_to_dir(buf); */

    snprintf(buf, sizeof(buf), "%s/%s/crossfire", getenv("HOME"),
            xdg_cache_dir);
    make_path_to_dir(buf);

    init_commands();
    init_metaserver();

    srandom(time(NULL));
}

/* This is basically called each time a new player logs
 * on - reset all the player data
 */
void reset_player_data() {
    int i;

    for (i = 0; i < MAX_SKILL; i++) {
        cpl.stats.skill_exp[i] = 0;
        cpl.stats.skill_level[i] = 0;
    }
}

/**
 * This is used to clear values between connections to different
 * servers.  This needs to be called after init_client_vars has
 * been called because it does not re-allocated some values.
 */

void reset_client_vars() {
    int i;

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

    for (i = 0; i < range_size; i++) {
        cpl.ranges[i] = NULL;
    }

    cpl.magicmap = NULL;
    cpl.showmagic = 0;

    csocket.command_sent = 0;
    csocket.command_received = 0;
    csocket.command_time = 0;
    csocket.cs_version = 0;
    csocket.inbuf.len = 0;

    face_info.faceset = 0;
    face_info.num_images = 0;
    /* Preserve the old one - this can be used to see if the next
     * server has the same name -> number mapping so that we don't
     * need to rebuild all the images.
     */
    face_info.old_bmaps_checksum = face_info.bmaps_checksum;
    face_info.bmaps_checksum = 0;
    face_info.cache_hits = 0;
    face_info.cache_misses = 0;
    face_info.have_faceset_info = 0;
    for (i = 0; i < MAX_FACE_SETS; i++) {
        FREE_AND_CLEAR(face_info.facesets[i].prefix);
        FREE_AND_CLEAR(face_info.facesets[i].fullname);
        face_info.facesets[i].fallback = 0;
        FREE_AND_CLEAR(face_info.facesets[i].size);
        FREE_AND_CLEAR(face_info.facesets[i].extension);
        FREE_AND_CLEAR(face_info.facesets[i].comment);
    }
    reset_player_data();
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
    stat_points = 0;
    stat_min = 0;
    stat_maximum = 0;

    serverloginmethod = 0;
}
