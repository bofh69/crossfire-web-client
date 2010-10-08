const char * const rcsid_gtk2_sound_c =
    "$Id$";

/*
    CrossFire, A Multiplayer game for X-windows

    Copyright (C) 2005,2010 Mark Wedel & Crossfire Development Team

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

    The author can be reached via e-mail to crossfire@metalforge.org
*/

/**
 * @file gtk-v2/src/sound.c
 * This file contains the sound support for the GTK V2 client.  It does not
 * actually play sounds, but rather tries to run cfsndserve, which is
 * responsible for playing sounds.
 */

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <client-types.h>
#include "client.h"

FILE *sound_pipe=NULL;
ChildProcess* sound_process;

/**
 * Opens the audio device, and reads relevant configuration files.
 *
 * @return
 * Returns 0 on success.  On failure, the calling function will likely disable
 * sound support/requests from the server.
 */
int init_sounds(void)
{
#ifndef WIN32
    char sound_path[MAX_BUF];

    /*
     * Easy trick - global nosound is set in the arg processing - if set, just
     * return -1 - this way, the calling function only needs to check the value
     * of init_sounds, and not worry about checking nosound.
     */
    if (!want_config[CONFIG_SOUND]) return -1;

    if (sound_server[0] == '\0') {
        LOG(LOG_ERROR,"init_sounds:", "sound-server variable not set to anything");
        return -1;
    }
    /*
     * If an absolute path is given, we use it unadorned.  Otherwise, we use
     * the path in the BINDIR.
     */
    if (sound_server[0] == '/')
        strcpy(sound_path, sound_server);
    else
        snprintf(sound_path, sizeof(sound_path),"%s/%s", BINDIR, sound_server);

    if (access(sound_path, X_OK)<0) {
        fprintf(stderr,"Unable to access %s sound server process\n", sound_path);
        return -1;
    }

    sound_process=raiseChild(sound_path,CHILD_STDIN|CHILD_STDOUT|CHILD_STDERR);
    logChildPipe(sound_process, LOG_INFO, CHILD_STDOUT|CHILD_STDERR);

    if (fcntl(sound_process->tube[0], F_SETFL, O_NONBLOCK)<0) {
        /*
         * Setting non-blocking isn't 100% critical, but a good thing if
         * possible.
         */
        perror("init_sounds: Warning - unable to set non blocking on sound pipe\n");
    }
    sound_pipe=fdopen(sound_process->tube[0],"w");
    return 0;
#else
    return -1;
#endif
}

/**
 * Initiates playing of a sound effect, specified by name and type, to
 * cfsndserv via a pipe.
 *
 * @param x      Offset of the sound relative to the player.
 * @param y      Offset of the sound relative to the player.
 * @param dir    The direction the sound is moving toward, where north = 1,
 *               northeast = 2, and so on.  0 indicates a stationary source.
 * @param vol    A value from 0 through 100 inclusive that suggests the
 *               relative loudness of the sound effect.
 * @param type   See server doc/Developers/sound for details.  1 is a sound
 *               related to living things.  2 is a spell-related sound.  3 is
 *               is made by an item.  4 is created by the environment.  5 is a
 *               sound of an attack.  6 is a sound of a incoming hit.  This is
 *               list may grow over time.
 * @param sound  A descriptive name for the sound effect to play.  It usually
 *               describes the sound itself, and may be combined with the type
 *               and source name to select the file to play.
 * @param source The name of the sound emitter.  It is used in combination
 *               with type and sound to determine which file to play.
 */
void play_sound_effect(sint8 x, sint8 y, uint8 dir, uint8 vol, uint8 type,
                       const char *sound, const char *source) {
#ifndef WIN32
    /**
     * cfsndserv recognizes sound commands by seeing the numeric parameters at
     * the beginning of the command line.
     */
    char format[] = "%4x %4x %4x %4x %4x \"%s\" \"%s\"\n";

    if (! use_config[CONFIG_SOUND])
        return;

    /*
     * Pass the sound command on to the player.
     *
     * NOTE: Sound and source are reversed with respect to how the server sent
     * data to the client.  This is intentional, so that the sound/music name
     * is always the last quoted string on the command sent to cfsndserv.
     */
    if ((fprintf(sound_pipe, format, x, y, dir, vol, type, source, sound) <= 0)
    ||  (fflush(sound_pipe) != 0)) {
        LOG(LOG_ERROR,
            "gtk-v2::play_sound_effect", "Cannot write sound pipe: %d", errno);
        use_config[CONFIG_SOUND] = 0;
        fclose(sound_pipe);
        sound_process = NULL;
        return;
    }
#if 1
    else
        LOG(LOG_INFO, "gtk-v2::play_sound_effect",
            format, x, y, dir, vol, type, sound, source);
#endif
#endif
}

/**
 * Parse the data contained by a sound2 command coming from the server and
 * handle playing the specified sound.  See server doc/Developers/sound for
 * details.
 *
 * @param data Data provided following the sound2 command from the server.
 * @param len  Length of the sound2 command data.
 */
void Sound2Cmd(unsigned char *data, int len) {
#ifndef WIN32
    sint8 x, y;
    uint8 dir, vol, type, len_sound, len_source;
    char* sound = NULL;
    char* source = NULL;

    /**
     * Format of the sound2 command recieved in data:
     *
     * <pre>
     * sound2 {x}{y}{dir}{volume}{type}{len_sound}{sound}{len_source}{source}
     *         b  b  b    b       b     b          str    b           str
     * </pre>
     */
    if (len < 8) {
        LOG(LOG_WARNING,
            "gtk-v2::Sound2Cmd", "Sound command too short: %d\n bytes", len);
        return;
    }

    x = data[0];
    y = data[1];
    dir = data[2];
    vol = data[3];
    type = data[4];
    len_sound = data[5];
    /*
     * The minimum size of data is 1 for each byte in the command (7) plus the
     * size of the sound string.  If we do not have that, the data is bogus.
     */
    if (6 + len_sound + 1 > len) {
        LOG(LOG_WARNING,
            "gtk-v2::Sound2Cmd", "sound length check: %i len: %i\n",
                len_sound, len);
        return;
    }

    len_source = data[6 + len_sound];
    if (len_sound != 0) {
        sound = (char*) data + 6;
        data[6 + len_sound] = '\0';
    }
    /*
     * The minimum size of data is 1 for each byte in the command (7) plus the
     * size of the sound string, and the size of the source string.
     */
    if (6 + len_sound + 1 + len_source > len) {
        LOG(LOG_WARNING,
            "gtk-v2::Sound2Cmd", "source length check: %i len: %i\n",
                len_source, len);
        return;
    }
    /*
     * Though it looks like there is potential for writing a null off the end
     * of the buffer, there is always room for a null (see do_client()).
     */
    if (len_source != 0) {
        source = (char*) data + 6 + len_sound + 1;
        data[6 + len_sound + 1 + len_source] = '\0';
    }

#if 1
    LOG(LOG_INFO, "gtk-v2::Sound2Cmd", "Playing sound2 x=%hhd y=%hhd dir=%hhd volume=%hhd type=%hhd",
        x, y, dir, vol, type);
    LOG(LOG_INFO, "gtk-v2::Sound2Cmd", "               len_sound=%hhd sound=%s", len_sound, sound);
    LOG(LOG_INFO, "gtk-v2::Sound2Cmd", "               len_source=%hhd source=%s", len_source, source);
#endif

    play_sound_effect(x, y, dir, vol, type, sound, source);
#endif
}

/**
 * Parse the data contained by a music command coming from the server and
 * pass the name along to cfsndserv as a quoted string.
 *
 * @param data Data provided following the music command from the server
 *             that hints what kind of music should play.  NONE is an
 *             indication that music should stop playing.
 * @param len  Length of the string describing the music to play.
 */
void MusicCmd(const char *data, int len) {
#ifndef WIN32
    /**
     * Format of the music command received in data:
     *
     * <pre>
     * music {string}
     * </pre>
     */
    if (! use_config[CONFIG_SOUND])
        return;
    /*
     * The client puts a null character at the end of the data.  If one is not
     * there, ignore the command.
     */
    if (data[len]) {
        LOG(LOG_ERROR,
            "gtk-v2::MusicCmd", "Music command buffer not null-terminated.");
        return;
    }
    /**
     * cfsndserv recognizes music commands by seeing the quoted string as the
     * first item on the command line.
     */
    if ((fprintf(sound_pipe, "\"%s\"\n", data) <= 0)
    ||  (fflush(sound_pipe) != 0)) {
        LOG(LOG_ERROR,
            "gtk-v2::MusicCmd", "Cannot write sound pipe: %d", errno);
        use_config[CONFIG_SOUND] = 0;
        fclose(sound_pipe);
        sound_process = NULL;
        return;
    }
#if 1
    else
        LOG(LOG_INFO, "gtk-v2::MusicCmd", "\"%s\"", data);
#endif
#endif
}
