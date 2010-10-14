const char * rcsid_sound_src_common_c =
    "$Id$";
/*
    Crossfire client, a client program for the crossfire program.

    Copyright (C) 2001 Mark Wedel & Crossfire Development Team

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    The author can be reached via e-mail to crossfire-devel@real-time.com
*/

/**
 * @file sound-src/common.c
 *
 */
#include "config.h"

#include <stdio.h>
#include <stdlib.h>

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <ctype.h>

#include "client-types.h"
#include "newclient.h"
#include "client.h"

#include "def_sounds.h"
#include "common.h"

Sound_Info normal_sounds[MAX_SOUNDS];
Sound_Info spell_sounds[MAX_SOUNDS];
Sound_Info default_normal;
Sound_Info default_spell;

char *client_sounds_path = NULL;        /* Client sound file folder         */
char *user_sounds_path   = NULL;        /* User sound file folder           */
char *user_sounds_file   = NULL;        /* User sound mappings              */
char *user_config_file   = NULL;        /* User sndconfig file.             */

char *buffers = NULL;

/*
 * Sound device parameters.  See also sound_settings.
 */
int stereo = 0;
int bit8 = 0;
int sample_size = 0;
int frequency = 0;
int sign = 0;
int zerolevel = 0;

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
static void parse_sound_line(char *line, int lineno) {
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

    if (line[0] == '#' || line[0] == '\n')
        return;

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
    if (line[strlen(line)-1] == '\n')
        line[strlen(line)-1] = '\0';
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
    while (*cp != '\0' && (*cp == ' ' || *cp == '\t'))
        cp++;
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
        while (*cp != '\0' && (*cp == ' ' || *cp == '\t'))
            cp++;
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
            while (*cp != '\0' && (*cp == ' ' || *cp == '\t'))
                cp++;
            if (isdigit(*cp))
                newnum = atoi(cp);
            else
                newnum = lastnum + 1;
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
    if (!strcmp(cp, ".au"))
        strcpy(cp, ".raw");
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
        if (symbolic)
            normal_sounds[newnum].symbolic = strdup_local(symbolic);
        else
            normal_sounds[newnum].symbolic = NULL;
    } else if (readtype == 2) {
        /*
         * Spell Sounds
         */
        spell_sounds[newnum].filename = strdup_local(filename);
        spell_sounds[newnum].volume = atoi(volume);
        if (symbolic)
            spell_sounds[newnum].symbolic = strdup_local(symbolic);
        else
            spell_sounds[newnum].symbolic = NULL;
    }
    /*
     * Retain the assigned sound number for possible use in subsquent data
     * lines.
     */
    lastnum = newnum;
}

/**
 * Opens the audio device, allocates buffers, and reads any configuration
 * files that need to be.
 *
 * http://en.wikipedia.org/wiki/Comparison_of_file_systems seems to show that
 * 255 characters is the maximum file name length for most file systems.  The
 * same page notes that many file systems have no defined limit to directory
 * depth.  Some operating environments have a maximum that is quite large -
 * for example, Windows NT can handle paths up to 32,767 bytes.  This data,
 * along with the fact that the server Music command from the server has no
 * inherent limitation (other than MAXSOCKBUF), is why MAXSOCKBUF is chosen
 * for the maximum size of the path buffer.  MAXSOCKBUF is rather large, but
 * the buffer size is practically only temporarily allocated (via the stack).
 *
 * @return Zero on success and on failure, the calling function will likely
 *         disable sound support/requests from the server.
 */
int init_sounds(void) {
    FILE *fp;
    char  path[MAXSOCKBUF];
    char  buf[512];
    int   i;

#ifdef SOUND_DEBUG
    fprintf( stderr, "Settings: bits: %i, ", settings.bit8 ? 8 : 16);
    fprintf( stderr, "%s, ",settings.sign ? "signed" : "unsigned");
    fprintf( stderr, "%s, ",settings.stereo ? "stereo" : "mono");
    fprintf( stderr, "frequency: %i, ", settings.frequency);
    fprintf( stderr, "device: %s\n", settings.audiodev);
#endif

    /*
     * Force the last char of the buffer to null in case strn* cuts off the
     * terminating null while copying file information.
     */
    path[sizeof(path) - 1] = '\0';
    /*
     * Initialize paths to various sound system resources.  Bail if any of
     * the buffer allocations fail.
     */
    strncpy(path, getenv("HOME"), sizeof(path) - 1);
    strncat(path, USER_CONFIG_FILE, sizeof(path) - 1);
    CONVERT_FILESPEC_TO_OS_FORMAT(path);
    user_config_file = (char *) malloc(strlen(path));
    if (user_config_file)
        strcpy(user_config_file, path);
    else
        return -1;

    strncpy(path, getenv("HOME"), sizeof(path) - 1);
    strncat(path, USER_SOUNDS_FILE, sizeof(path) - 1);
    CONVERT_FILESPEC_TO_OS_FORMAT(path);
    user_sounds_file = (char *) malloc(strlen(path));
    if (user_sounds_file)
        strcpy(user_sounds_file, path);
    else
        return -1;

    strncpy(path, getenv("HOME"), sizeof(path) - 1);
    strncat(path, USER_SOUNDS_PATH, sizeof(path) - 1);
    CONVERT_FILESPEC_TO_OS_FORMAT(path);
    user_sounds_path = (char *) malloc(strlen(path));
    if (user_sounds_path)
        strcpy(user_sounds_path, path);
    else
        return -1;

    strncpy(path, CLIENT_SOUNDS_PATH, sizeof(path) - 1);
    CONVERT_FILESPEC_TO_OS_FORMAT(path);
    client_sounds_path = (char *) malloc(strlen(path));
    if (client_sounds_path)
        strcpy(client_sounds_path, path);
    else
        return -1;

    buffers = (char *) malloc(settings.buffers * settings.buflen);
    if (!buffers)
        return -1;

    sounds_in_buffer = (int *) calloc(settings.buffers, sizeof(int));
    if (!sounds_in_buffer)
        return -1;

    if (init_audio())
        return -1;

    if (sign)
        zerolevel = 0;
    else
        zerolevel = bit8 ? 0x80 : 0x00;

    memset(buffers, zerolevel, settings.buflen * settings.buffers);

#ifdef SOUND_DEBUG
    fprintf( stderr, "bits: %i, ", bit8 ? 8 : 16);
    fprintf( stderr, "%s, ", sign ? "signed" : "unsigned");
    fprintf( stderr, "%s, ", stereo ? "stereo" : "mono");
    fprintf( stderr, "freq: %i, ", frequency);
    fprintf( stderr, "smpl_size: %i, ", sample_size);
    fprintf( stderr, "0level: %i\n", zerolevel);
#endif

    for (i = 0; i < MAX_SOUNDS; i++) {
        normal_sounds[i].filename = NULL;
        spell_sounds[i].filename = NULL;
        normal_sounds[i].size = -1;
        spell_sounds[i].size = -1;
    }
    default_normal.filename = NULL;
    default_spell.filename = NULL;

    i = 0;
    if (!(fp = fopen(user_sounds_file, "r"))) {
        fprintf(stderr,
            "Unable to open %s - using built-in defaults\n",
                user_sounds_file);
        for (; i < sizeof(def_sounds) / sizeof(char*); i++) {
            strcpy(buf, def_sounds[i]);
            parse_sound_line(buf, i);
        }
    } else while (fgets(buf, 511, fp) != NULL) {
        buf[511] = '\0';
        parse_sound_line(buf, ++i);
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
        normal_sounds[i].data = NULL;
        spell_sounds[i].data = NULL;
    }
    return 0;
}

/**
 * Convert a sound name to a sound number to help with the transition of the
 * sound server from sound support to sound2 capability.  This is not an end
 * solution, but one that gets the sound server working a little bit until a
 * better one can be implemented.
 *
 * @param name
 * @param type
 */
int sound_to_soundnum(const char *name, uint8 type) {

    fprintf(stderr, "name=%s type=%d\n", name, type);
    /**
     * @todo Implement conversion to legacy soundnum.
     * @todo Replace conversion to legacy soundnum.
     */
    return 1;
}

/**
 * Convert a sound type to legacy type to help with the transition of the
 * sound server from sound support to sound2 capability.  This is not an end
 * solution, but one that gets the sound server working a little bit until a
 * better one can be implemented.  Basically, all types except 2 get changed
 * to 1.
 *
 * @param type
 */
int type_to_soundtype(uint8 type) {

#ifdef SOUND_DEBUG
    fprintf(stderr,
        "Converted type %d to legacy type %d.\n", type, (type == 2) ? 2 : 1);
#endif
    /**
     * @todo Replace conversion to legacy soundtype.
     */
    return (type == 2) ? 2 : 1;
}

/**
 * Preliminary handler for Crossfire server sound2 and music commands that are
 * received from the client via stdin.
 *
 * The sound player differentiates sound2 and music commands by looking at the
 * first parameter that comes in.  Music commands consist of a single, quoted
 * string that identifies the music to play, while a sound effect command has
 * various numeric parameters followed by strings that identify what to play.
 *
 * Sound2 data consists of whitespace delimited values:  x, y, dir, vol, type,
 * sound, and source.  Type, sound, and source define what to play, while the
 * other parameters may be used to figure out how to play it.  x and y are
 * offsets from the player to identify where the sound originated. dir can be
 * set to indicate a direction that the source is travelling in. vol is an
 * attenuation factor (0-100) that may be applied to the sound volume to make
 * it possible, for example, to give map-designers the ability to suggest
 * relative loudness of sounds in the environment.
 *
 * FIXME: This is a work-in-progress.  The sound2 was put into the server
 * without a plan to fix the clients.  cfsndserv is basically made obsolete by
 * sound2.  The basic fix resurrects some sound support but does not fully
 * implement the features sound2 is supposed to provide.
 *
 * @param data A text buffer that (hopefully) has a sound or music command.
 * @param len  The length of the text data in the command buffer.
 * @return     0 if the buffer contains a well-formed command, otherwise -1.
 */
int StdinCmd(char *data, int len) {
    char* dptr;                         /* Pointer used when parsing data */
    char* fptr;
    char* sound = NULL;                 /* Points to a sound or music name */
    char* source = NULL;
    char  soundfile[MAXSOCKBUF];
    int   sourcelen;
    int   soundlen;
    int   spacelen;
    int   type = 0;
    int   dir = 0;
    int   vol = 0;
    int   x = 0;
    int   y = 0;
    int   i = 0;

    fptr = soundfile;
    dptr = strtok(data, "\"");
    /*
     * Is data a blank line (ending with LF) or is it a quoted, empty string?
     */
    if (dptr == NULL) {
        fprintf(stderr, "Sound/music command does not contain any data.\n");
        return -1;
    }
    /*
     * If the first character is not a quote character, a sound command is
     * expected.
     */
    if (data[0] != '\"') {
        /*
         * There are 5 numeric values expected and required.  Technically, if
         * cfsndserv was new, and the client old, 4 might be present, but the
         * player does not attempt to support old clients.
         */
        i = sscanf(dptr, "%d %d %d %d %d", &x, &y, &dir, &vol, &type);

        if ((i != 5)
        ||  (dir < 0)
        ||  (dir > 8)
        ||  (vol < 0)
        ||  (vol > 100)
        ||  (type < 1)) {
            /*
             * There is not much point in trying to work with data that does
             * not fit some basic rules known at the time of development.
             */
            fprintf(stderr, "Unrecognized sound command data format.\n");
#ifdef SOUND_DEBUG
            fprintf(stderr,
                "(%d valid items read) x=%d y=%d dir=%d vol=%d type=%d\n",
                    i, x, y, dir, vol, type);
#endif
            return -1;
        }
    }
    /*
     * Below this point, when type == 0, a music command is expected, and when
     * type != 0, a sound command is required.
     */
    if (type) {
        /*
         * dptr points to the numerics already read, so advance to the string
         * following the first quote delimiter.  A sound source name is
         * expected.
         */
        dptr = strtok(NULL, "\"");
        if (dptr == NULL) {
            fprintf(stderr, "Sound command is missing sound/source names.\n");
            return -1;
        }
        source = dptr;
        sourcelen = strlen(dptr);
       /*
        * Verify there is whitespace between source and sound names.
        */
        dptr = strtok(NULL, "\"");
        if (dptr == NULL) {
             fprintf(stderr, "Sound command is missing the sound name.\n");
             return -1;
        }
        spacelen = strlen(dptr);
        for (i = 0; i < spacelen; i++) {
            if (dptr[i] != ' ' && dptr[i] != '\t') {
                fprintf(stderr, "Invalid characters after source name.\n");
                return -1;
            }
        }
        /*
         * Advance the data pointer to the following sound name.
         */
         dptr = strtok(NULL, "\"");
         if (dptr == NULL) {
             fprintf(stderr, "Sound command is missing the sound name.\n");
             return -1;
        }
    }
    /*
     * Record the sound or music name here (type determines which it is).
     */
    sound = dptr;
    soundlen = strlen(dptr);
    /*
     * If there was a trailing quote after the sound or music name, there will
     * be a null there now, and sound[soundlen] should point to the character
     * just before another null at data[len-1] (that terminates the command).
     */
    i = sound - data + soundlen + 1 + 1;
    if (i - 1 == len) {
        fprintf(stderr, "Sound or music name does not end with a quote.\n");
        return -1;
    }
    if (i > len) {
        fprintf(stderr,
            "Invalid data after sound/music name (a quoted string needed)\n");
        return -1;
    }

    if (type) {
        /*
         * Play sound effect here.
         */
#ifdef SOUND_DEBUG
        fprintf(stderr, "Playing sound "
            "%d,%d dir=%d vol=%d type=%d source=\"%s\" sound=\"%s\"\n",
                x, y, dir, vol, type, source, sound);
#endif
        play_sound(sound_to_soundnum(sound, type),
                   type_to_soundtype(type), x, y);
        return 0;
    } else {
        /*
         * Play music here.
         */
#ifdef SOUND_DEBUG
        fprintf(stderr,
            "Playing music \"%s\"\n", sound);
#endif
        play_music(sound);
    }

    return 0;
}

/**
 * Update the player .crossfire/sndconfig file.
 *
 * @return
 */
int write_settings(void) {
    FILE *f;

    f = fopen(user_config_file, "w");
    if (!f)
        return -1;

    fprintf(f, "# Crossfire sound server settings\n");
    fprintf(f, "# Please note, that not everything will work\n\n");
    fprintf(f, "stereo: %i\n", settings.stereo);
    fprintf(f, "bits: %i\n", settings.bit8?8:16);
    fprintf(f, "signed: %i\n", settings.sign);
    fprintf(f, "frequency: %i\n", settings.frequency);
    fprintf(f, "buffers: %i\n", settings.buffers);
    fprintf(f, "buflen: %i\n", settings.buflen);
    fprintf(f, "simultaneously: %i\n", settings.simultaneously);
    /* fprintf(f,"device: %s\n",settings.audiodev); */
    fclose(f);
    return 0;
}

/**
 * Read the player .crossfire/sndconfig file.
 *
 * @return
 */
int read_settings(void) {
    char linebuf[1024];
    FILE *f;

    if (user_config_file == NULL)
        return 0;

    f = fopen(user_config_file, "r");
    if (!f)
        return -1;

    while(fgets(linebuf, 1023, f) != NULL) {
        linebuf[1023] = 0;
        /* Strip off the newline */
        linebuf[strlen(linebuf) - 1] = 0;

        if (strncmp(linebuf, "stereo:", strlen("stereo:")) == 0)
            settings.stereo = atoi(linebuf + strlen("stereo:")) ? 1 : 0;
        else if (strncmp(linebuf, "bits:", strlen("bits:")) == 0)
            settings.bit8 = (atoi(linebuf + strlen("bits:"))==8) ? 1 : 0;
        else if (strncmp(linebuf, "signed:", strlen("signed:")) == 0)
            settings.sign = atoi(linebuf + strlen("signed:")) ? 1 : 0;
        else if (strncmp(linebuf, "buffers:", strlen("buffers:")) == 0)
            settings.buffers = atoi(linebuf + strlen("buffers:"));
        else if (strncmp(linebuf, "buflen:", strlen("buflen:")) == 0)
            settings.buflen = atoi(linebuf + strlen("buflen:"));
        else if (strncmp(linebuf, "frequency:", strlen("frequency:")) == 0)
            settings.frequency = atoi(linebuf + strlen("frequency:"));
        else if (strncmp(linebuf, "simultaneously:", strlen("simultaneously:")) == 0)
            settings.simultaneously = atoi(linebuf + strlen("simultaneously:"));
#if 0
        else if (strncmp(linebuf,"device: ",strlen("device: "))==0)
                settings.audiodev=strdup_local(linebuf+strlen("device: "));
#endif
    }
    fclose(f);
    return 0;
}

