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
 * @file sound-src/init.c
 * Initialize various parts of the sound server.
 */

#include <stdio.h>
#include <stdlib.h>

#include "config.h"

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "client.h"

#include "common.h"
#include "def_sounds.h"
#include "sndproto.h"

Sound_Info normal_sounds[MAX_SOUNDS];
Sound_Info spell_sounds[MAX_SOUNDS];
Sound_Info default_normal;
Sound_Info default_spell;

char *client_sounds_path = NULL;    /* Client sound directory */
char *user_sounds_path = NULL;      /* User sound directory */

static char *user_sounds_file  = NULL;     /* User sound definitions */

/**
 * Initialize paths to various resources, such as sound config files.
 *
 * Currently, this means to append $HOME to each path after resizing the array
 * using malloc(). This is grossly inefficient and uses potentially unsafe
 * functions. This should be a TODO and a FIXME.
 * 
 * @return Zero on success, anything else on failure.
 */
static int init_paths() {
    char path[MAXSOCKBUF];

    /* Manually set the last character of the buffer to NUL in case strn* cuts
     * off the terminating NUL while copying file information. */
    path[sizeof(path) - 1] = '\0';

    /* Sanity check for a $HOME environmental variable set. */
    if (getenv("HOME") == NULL) {
        fprintf(stderr,
                "error: couldn't read $HOME environmental variable\n"
                "Please run again with $HOME set to something reasonable.\n");
        return -1;
    }

    /* Initialize paths to various sound system resources.  Bail if any of
     * the buffer allocations fail. */
    snprintf(path, sizeof(path), "%s%s", getenv("HOME"), USER_SOUNDS_FILE);
    CONVERT_FILESPEC_TO_OS_FORMAT(path);
    user_sounds_file = (char *) malloc(strlen(path));
    if (user_sounds_file) {
        strcpy(user_sounds_file, path);
    } else {
        return -1;
    }

    snprintf(path, sizeof(path), "%s%s", getenv("HOME"), USER_SOUNDS_PATH);
    CONVERT_FILESPEC_TO_OS_FORMAT(path);
    user_sounds_path = (char *) malloc(strlen(path));
    if (user_sounds_path) {
        strcpy(user_sounds_path, path);
    } else {
        return -1;
    }

    strncpy(path, CLIENT_SOUNDS_PATH, sizeof(path) - 1);
    CONVERT_FILESPEC_TO_OS_FORMAT(path);
    client_sounds_path = (char *) malloc(strlen(path));
    if (client_sounds_path) {
        strcpy(client_sounds_path, path);
    } else {
        return -1;
    }

    return 0;
}

/**
 * Load sound definitions from a file or use built-in defaults.
 */
static void init_sounds() {
    FILE *fp;
    char buf[512];
    int i;

    default_normal.filename = NULL;
    default_spell.filename = NULL;

    for (i = 0; i < MAX_SOUNDS; i++) {
        normal_sounds[i].filename = NULL;
        spell_sounds[i].filename = NULL;
    }

    if (!(fp = fopen(user_sounds_file, "r"))) {
        fprintf(stderr,
                "Unable to open %s - using built-in defaults\n",
                user_sounds_file);
        for (i = 0; i < sizeof(def_sounds) / sizeof(char*); i++) {
            strcpy(buf, def_sounds[i]);
            parse_sound_line(buf, i);
        }
    } else {
        while (fgets(buf, 511, fp) != NULL) {
            buf[511] = '\0';
            parse_sound_line(buf, ++i);
        }
    }

    /* Note in both cases below, we leave the symbolic name untouched. */
    for (i = 0; i < MAX_SOUNDS; i++) {
        if (!normal_sounds[i].filename) {
            normal_sounds[i].filename = default_normal.filename;
            normal_sounds[i].volume = default_normal.volume;
        }
        if (!spell_sounds[i].filename) {
            spell_sounds[i].filename = default_spell.filename;
            spell_sounds[i].volume = default_spell.volume;
        }
    }
}

/**
 * Initialize sound server.
 *
 * Initialize resource paths, load sound definitions, and ready the sound
 * subsystem.
 *
 * @return Zero on success, anything else on failure.
 */
int init() {
    /* Initialize paths to various resources. */
    if (init_paths() != 0) {
        return -1;
    }

    /* Initialize sound definitions. */
    init_sounds();

    /* Initialize audio library. */
    if (init_audio()) {
        return -1;
    }

    return 0;
}
