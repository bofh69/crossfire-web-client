/*
 * Crossfire -- cooperative multi-player graphical RPG and adventure game
 *
 * Copyright (c) 1999-2013 Mark Wedel and the Crossfire Development Team
 * Copyright (c) 1992 Frank Tore Johansen
 *
 * Crossfire is free software and comes with ABSOLUTELY NO WARRANTY. You are
 * welcome to redistribute it under certain conditions. For details, see the
 * 'LICENSE' and 'COPYING' files.
 *
 * The authors can be reached via e-mail to crossfire-devel@real-time.com
 */

/**
 * @file
 * This file contains the sound support for the GTK V2 client.  It does not
 * actually play sounds, but rather tries to run cfsndserve, which is
 * responsible for playing sounds.
 */

#include "client.h"

#include "client-vala.h"

SoundServer* server;

int init_sounds() {
    server = sound_server_new();
    return sound_server_spawn(server, sound_server);
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
    gint8 x, y;
    guint8 dir, vol, type, len_sound, len_source;
    char *sound = NULL;
    char *source = NULL;

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
        sound = (char *) data + 6;
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
        source = (char *) data + 6 + len_sound + 1;
        data[6 + len_sound + 1 + len_source] = '\0';
    }

    LOG(LOG_DEBUG, "gtk-v2::Sound2Cmd",
        "Playing sound2 x=%hhd y=%hhd dir=%hhd volume=%hhd type=%hhd",
        x, y, dir, vol, type);
    LOG(LOG_DEBUG, "gtk-v2::Sound2Cmd", "               len_sound=%hhd sound=%s",
        len_sound, sound);
    LOG(LOG_DEBUG, "gtk-v2::Sound2Cmd", "               len_source=%hhd source=%s",
        len_source, source);
    if (use_config[CONFIG_SOUND]) {
        sound_server_play(server, x, y, dir, vol, type, sound, source);
    }
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
    /**
     * Format of the music command received in data:
     *
     * <pre>
     * music {string}
     * </pre>
     */
    // Check for null terminator.
    if (data[len]) {
        LOG(LOG_ERROR, "gtk-v2::MusicCmd",
            "Music command string is not null-terminated.");
        return;
    }
    LOG(LOG_DEBUG, "gtk-v2::MusicCmd", "\"%s\"", data);
    if (use_config[CONFIG_SOUND]) {
        sound_server_play_music(server, data);
    }
}
