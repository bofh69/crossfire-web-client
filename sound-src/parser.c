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
#include "newclient.h"
#include "client.h"

#include "common.h"
#include "sndproto.h"

/**
 * Parse a line from the sound file.  This is a little ugly because static
 * values are stored in the function so we know what we are doing - however,
 * it is somewhat necessary so that we can use this same function to parse
 * both files and the compiled in data.  The linefeed delimited sound file
 * lines that are empty, or begin with hash (#) characters are ignored. Data
 * lines are space/tab-delimited fields:
 *
 * sound_file<TAB>default_volume<TAB>SOUND_SYMBOLIC_NAME<TAB>sound_number
 *
 * sound_file is an absolute path (not at all friendly or resilient to client
 *     changes like changing from a distro version to SVN, etc).
 *
 * default_volume is an integer from 0 to 100 (a percentage) that is left
 *     padded with spaces to produce a fixed, three-character width.
 *
 * SOUND_SYMBOLIC_NAME is unused and may be omitted, but could be used for
 *     client/server communication regarding a sound to play.
 *
 * sound_number is a zero-based integer that identifies the sound.  If it is
 *     omitted, the last sound number used is incremented by one.  The server
 *     used to communicate what sound to play via the sound number, so the
 *     order was not arbitrary, but the number and order is now obsolete from
 *     a server perspective.
 *
 * @param line   A line of data from the .crossfire/sounds configuration file.
 *               Note that this data may be modified by parse_sound_line().
 * @param lineno The line number of the passed data used for error tracking.
 */
void parse_sound_line(char *line, int lineno) {
    static int readtype=0;              /**< Identifies the last section title
                                         *   found in the .crossfire/sounds
                                         *   file.  0 indicates a section
                                         *   title was not found yet.
                                         */
    static int lastnum=0;               /**< The number of items processed in
                                         *   the current readtype section.
                                         */
    int        newnum, len;
    char      *cp, *volume, *symbolic, *cp1, filename[512];

    if (line[0] == '#' || line[0] == '\n') {
        return;
    }

    if (!strcmp(line, "Standard Sounds:\n")) {
        lastnum = 0;
        readtype = 1;
        return;
    }

    if (!strcmp(line, "Spell Sounds:\n")) {
        lastnum = 0;
        readtype = 2;
        return;
    }
    if (!readtype) {
#ifdef SOUND_DEBUG
        fprintf(stderr,
                "parse_sound_line: Ignored file header:\n%d:%s\n", lineno, line);
#endif
        return;
    }
    /*
     * Change the LF delimiter at the end of the line to a null terminator.
     */
    if (line[strlen(line)-1] == '\n') {
        line[strlen(line)-1] = '\0';
    }
    /*
     * Convert the first whitespace found to a null terminator.
     */
    len = strcspn(line, " \t");
    line[len] = '\0';
    cp = line + len + 1;
    /*
     * Skip all the following whitespace to locate the next field, and save a
     * pointer to the volume data.
     */
    while (*cp != '\0' && (*cp == ' ' || *cp == '\t')) {
        cp++;
    }
    volume = cp;
    /*
     * There is no need to null terminate the volume since it is processed
     * with atoi.
     *
     * Next, check to see if the unprocessed portion of the line has any
     * whitespace following the default volume.
     */
    cp1 = cp;
    if (!(cp = strchr(cp1, ' ')) && !(cp = strchr(cp1, '\t'))) {
        /*
         * If not, there cannot be a sound number, and any data left is an
         * unused symbolic name, so the sound number is auto-assigned.
         */
        newnum = lastnum + 1;
        symbolic = NULL;
    } else {
        /*
         * Since there is more whitespace, there might be a symbolic name and
         * sound number.  Ignore any additional whitespace following the
         * volume, and treat the next character as the beginning of a symbolic
         * name.
         */
        while (*cp != '\0' && (*cp == ' ' || *cp == '\t')) {
            cp++;
        }
        symbolic = cp;
        /*
         * Some symbolic names are double-quoted to allow them to contain
         * whitespace.  If a quote starts the name, advance the name pointer
         * to effectively strip the quote, and convert the final quote to a
         * null terminator.
         */
        if (*symbolic == '"') {
            symbolic++;
            for (cp = symbolic; *cp != '\0' && *cp != '"'; cp++);
            *cp = '\0';
            cp++;
        }
        /*
         * cp is either the beginning of an unquoted symbolic name or is
         * pointing to the whitespace between a quoted symbolic name and the
         * sound number.
         */
        cp1 = cp;
        if (!(cp = strchr(cp1, ' ')) && !(cp = strchr(cp1, '\t'))) {
            /*
             * There is no more whitespace on the line.  If the name was
             * quoted, there should have been whitespace following that cp was
             * pointing to, so there cannot be a sound number present.  On the
             * other hand, if the name was not quoted, cp would point at the
             * symbolic name and whitespace should follow if a sound number is
             * present.  Either way, the sound number must be auto-assigned.
             */
            newnum = lastnum + 1;
        } else {
            /*
             * If there was whitespace left, cp points to it now, whether or
             * not the symbolic name was quoted.  A sound number should follow
             * the whitespace.  First, try to null terminate the prior data,
             * then skip all subsequent whitespace, and point at what should
             * be the sound number.  If numeric data is found, read the sound
             * number, otherwise auto-assign the sound number.  This is a bit
             * dodgy as invalid data is silently ignored.
             */
            *cp++ = '\0';
            while (*cp != '\0' && (*cp == ' ' || *cp == '\t')) {
                cp++;
            }
            if (isdigit(*cp)) {
                newnum = atoi(cp);
            } else {
                newnum = lastnum + 1;
            }
        }
    }
    if (newnum < 0 || newnum > MAX_SOUNDS) {
        fprintf(stderr,
                "Invalid sound number %d, line %d, buf %s\n",
                newnum, lineno, line);
        return;
    }
    /*
     * Compatibility processing for older files and/or the SDL_mixer setup.
     * If the filename ends in .au, convert the ending to a more appropriate
     * one as .au files are not distributed by the project.
     *
     * Use .raw instead of .au for most sound setups, as this is what has been
     * supported by the clients for a long time.
     *
     * As SDL_mixer does not support .raw, change the extension to .ogg for
     * systems other than Windows, or .wav for Windows.  Technically, it would
     * be okay to use either .wav or .ogg whatever the platform, so it is a
     * FIXME in that it would probably be best for the file extension to be a
     * configurable option.
     *
     * Overriding the content of the sound file is a bit of a kludge, but
     * allows legacy .crossfire/sound files to work with the current client.
     * The dodgy part is that if someone looks in the file, it will not
     * necessarily indicate the actual file being played.
     */
    strcpy(filename, line);
    cp = filename + strlen(filename) - 3;
    if (!strcmp(cp, ".au")) {
        strcpy(cp, ".raw");
    }
#ifdef SDL_SOUND
    cp = filename + strlen(filename) - 4;
    if (!strcmp(cp, ".raw"))
#ifndef WIN32
        strcpy(cp, ".ogg");
#else
        strcpy(cp, ".wav");
#endif
#endif
    /*
     * One symbolic name is used: DEFAULT.  If it is found, the sound file
     * becomes the default sound for any undefined sound number, so set the
     * appropriate default, and ignore any sound number that may follow.
     */
    if (symbolic && !strcmp(symbolic, "DEFAULT")) {
        if (readtype == 1) {
            /*
             * Standard Sounds
             */
            default_normal.filename = strdup_local(filename);
            default_normal.volume = atoi(volume);
        } else if (readtype == 2) {
            /*
             * Spell Sounds
             */
            default_spell.filename = strdup_local(filename);
            default_spell.volume = atoi(volume);
        }
        return;
    }
    /*
     * The only way for processing to reach this point is if valid sound data
     * was found in a section.  Process it according to the section it is in.
     */
    if (readtype == 1) {
        /*
         * Standard Sounds
         */
        normal_sounds[newnum].filename = strdup_local(filename);
        normal_sounds[newnum].volume = atoi(volume);
        if (symbolic) {
            normal_sounds[newnum].symbolic = strdup_local(symbolic);
        } else {
            normal_sounds[newnum].symbolic = NULL;
        }
    } else if (readtype == 2) {
        /*
         * Spell Sounds
         */
        spell_sounds[newnum].filename = strdup_local(filename);
        spell_sounds[newnum].volume = atoi(volume);
        if (symbolic) {
            spell_sounds[newnum].symbolic = strdup_local(symbolic);
        } else {
            spell_sounds[newnum].symbolic = NULL;
        }
    }
    /*
     * Retain the assigned sound number for possible use in subsquent data
     * lines.
     */
    lastnum = newnum;
}

/**
 * Convert a sound name to a sound number to help with the transition of the
 * sound server from sound support to sound2 capability.  This is not an end
 * solution, but one that gets the sound server working a little bit until a
 * better one can be implemented.
 */
int sound_to_soundnum(const char *name, uint8 type) {
    Sound_Info *sounds;

    if (type == SOUND_NORMAL) {
        sounds = normal_sounds;
    } else {
        sounds = spell_sounds;
    }

    for (int i = 0; i < MAX_SOUNDS; i++) {
        if (sounds[i].symbolic != NULL) {
            if (strcmp(sounds[i].symbolic, name) == 0) {
                return i;
            }
        }
    }

    printf("Could not find matching sound for '%s'; using default.\n", name);
    return 1;
}

/**
 * Convert a legacy sound type to the sound2 equivalent.
 *
 * This is intended to help ease the transition from old sound to sound2
 * capability.
 */
int type_to_soundtype(uint8 type) {
    uint8 new_type;

    if (type == 2) {
        new_type = 1;
    } else {
        new_type = type;
    }

    printf("Converted legacy sound type %d to %d.\n", type, new_type);
    return new_type;
}
