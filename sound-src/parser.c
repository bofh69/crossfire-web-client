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
 * @file sound-src/parser.c
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include "config.h"

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "client-types.h"
#include "client.h"
#include "common.h"
#include "newclient.h"
#include "sndproto.h"

/**
 * Convert a sound name to a sound number to help with the transition of the
 * sound server from sound support to sound2 capability.  This is not an end
 * solution, but one that gets the sound server working a little bit until a
 * better one can be implemented.
 */
int sound_to_soundnum(const char *name, guint8 type) {
    int i;

    for (i = 0; i < MAX_SOUNDS; i++) {
        if (sounds[i].symbolic != NULL) {
            if (strcmp(sounds[i].symbolic, name) == 0) {
                return i;
            }
        }
    }

    printf("Could not find matching sound for '%s'.\n", name);
    return -1;
}

/**
 * Convert a legacy sound type to the sound2 equivalent.
 *
 * This is intended to help ease the transition from old sound to sound2
 * capability.
 */
int type_to_soundtype(guint8 type) {
    guint8 new_type;

    if (type == 2) {
        new_type = 1;
    } else {
        new_type = type;
    }

    printf("Converted legacy sound type %d to %d.\n", type, new_type);
    return new_type;
}
