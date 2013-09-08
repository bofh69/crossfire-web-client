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
#include "sndproto.h"

Sound_Info normal_sounds[MAX_SOUNDS];
Sound_Info spell_sounds[MAX_SOUNDS];
Sound_Info default_normal = {NULL, NULL, 0};
Sound_Info default_spell = {NULL, NULL, 0};

/**
 * Load sound definitions from a file.
 */
static void init_sounds() {
    FILE *fp;
    char buf[512];
    int i;

    /* First, initialize by setting all sounds to NULL. */
    for (i = 0; i < MAX_SOUNDS; i++) {
        normal_sounds[i].filename = NULL;
        spell_sounds[i].filename = NULL;
    }

    /* Try to open the sound definitions file. */
    printf("Loading sounds from '%s'...\n", getenv("CF_SOUND_DIR"));
    fp = fopen(getenv("CF_SOUND_CONF"), "r");

    if (fp == NULL) {
        fprintf(stderr, "Could not find sound definitions; aborting!\n");
        exit(EXIT_FAILURE);
    }

    /* Use 'i' as a line number tracker, so set it to zero. */
    i = 0;

    /* Parse the sound definitions file, line by line. */
    while (fgets(buf, sizeof(buf), fp) != NULL) {
        parse_sound_line(buf, i++);
    }

    /* Set unread sounds to the default sound. */
    for (i = 0; i < MAX_SOUNDS; i++) {
        if (normal_sounds[i].filename == NULL) {
            normal_sounds[i].filename = default_normal.filename;
            normal_sounds[i].volume = default_normal.volume;
        }

        if (spell_sounds[i].filename == NULL) {
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
    char path[MAXSOCKBUF];

    /* Sanity check for $HOME environmental variable. */
    if (getenv("HOME") == NULL) {
        fprintf(stderr, "Couldn't read $HOME environmental variable.\n"
                "Please set it to something reasonable.\n");
        return -1;
    }

    /* Set $CF_SOUND_DIR to something reasonable, if not already set. */
    if (setenv("CF_SOUND_DIR", CLIENT_SOUNDS_PATH, 0) != 0) {
        perror("Couldn't set $CF_SOUND_DIR");
        return -1;
    }

    /* Set $CF_SOUND_CONF to something reasonable, if not already set. */
    snprintf(path, sizeof(path), "%s/sounds.conf", getenv("CF_SOUND_DIR"));

    if (setenv("CF_SOUND_CONF", path, 0) != 0) {
        perror("Couldn't set $CF_SOUND_CONF");
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
