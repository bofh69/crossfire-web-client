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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "client.h"
#include "common.h"
#include "sndproto.h"

Sound_Info sounds[MAX_SOUNDS];

/**
 * Load sound definitions from a file.
 */
static void init_sounds() {
    FILE *fp;
    char buf[512];
    int i;

    /* Initialize by setting all sounds to NULL. */
    for (i = 0; i < MAX_SOUNDS; i++) {
        sounds[i].filename = NULL;
    }

    /* Try to open the sound definitions file. */
    printf("Loading sounds from '%s'...\n", g_getenv("CF_SOUND_DIR"));
    fp = fopen(g_getenv("CF_SOUND_CONF"), "r");

    if (fp == NULL) {
        fprintf(stderr, "Could not find sound definitions; aborting!\n");
        exit(EXIT_FAILURE);
    }

    /* Use 'i' as index tracker, so set it to zero. */
    i = 0;

    /* Parse the sound definitions file, line by line. */
    while (fgets(buf, sizeof(buf), fp) != NULL) {
        char *line;
        line = &buf[0];

        /* Ignore all lines that start with a comment or newline. */
        if (buf[0] == '#' || buf[0] == '\n') {
            continue;
        }

        /* Trim the trailing newline if it exists (see CERT FIO36-C). */
        char *newline;
        newline = strchr(buf, '\n');

        if (newline != NULL) {
            *newline = '\0';
        }

        /* FIXME: No error checking; potential segfaults here. */
        sounds[i].symbolic = g_strdup(strsep(&line, ":"));
        sounds[i].volume = atoi(strsep(&line, ":"));
        sounds[i].filename = g_strdup(strsep(&line, ":"));

        /* Move on to the next sound. */
        i++;
    }

    fclose(fp);
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
    if (g_getenv("HOME") == NULL) {
        fprintf(stderr, "Couldn't read $HOME environmental variable.\n"
                "Please set it to something reasonable.\n");
        return -1;
    }

    /* Set $CF_SOUND_DIR to something reasonable, if not already set. */
    if (g_setenv("CF_SOUND_DIR", CLIENT_SOUNDS_PATH, 0) != 0) {
        perror("Couldn't set $CF_SOUND_DIR");
        return -1;
    }

    /* Set $CF_SOUND_CONF to something reasonable, if not already set. */
    snprintf(path, sizeof(path), "%s/sounds.conf", g_getenv("CF_SOUND_DIR"));

    if (g_setenv("CF_SOUND_CONF", path, 0) != 0) {
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
