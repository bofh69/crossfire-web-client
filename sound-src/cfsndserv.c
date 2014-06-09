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
 * @file sound-src/cfsndserv.c
 * Implements a server for sound support in the client using SDL_mixer.
 */

#include "config.h"

#include <SDL.h>
#include <SDL_mixer.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "shared/newclient.h"
#include "client-types.h"
#include "sndproto.h"
#include "common.h"

static Mix_Chunk **chunk = NULL;
static Mix_Music *music = NULL;
static sound_settings settings = { 0, 1, 0, 11025, 512, 4 };

/**
 * Initialize the sound subsystem.
 *
 * Currently, this means calling SDL_Init() and Mix_OpenAudio().
 *
 * @return Zero on success, anything else on failure.
 */
int init_audio() {
    Uint16 audio_format;
    const int mix_flags = MIX_INIT_OGG;
    int audio_channels, mix_init;

    /* Set appropriate audio format based on settings. */
    if (settings.bit8) {
        audio_format = settings.sign ? AUDIO_S8 : AUDIO_U8;
    } else {
        audio_format = settings.sign ? AUDIO_S16 : AUDIO_U16;
    }

    audio_channels = (settings.stereo) ? 1 : 2;

    /* Initialize sound library and output device. */
    printf("Initializing sound using %s %d-bit %s channel @ %d Hz...\n",
            settings.sign ? "signed" : "unsigned",
            settings.bit8 ? 8 : 16,
            settings.stereo ? "stereo" : "mono",
            settings.frequency);

    if (SDL_Init(SDL_INIT_AUDIO) == -1) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    if (Mix_OpenAudio(settings.frequency, audio_format, audio_channels,
                settings.buflen)) {
        fprintf(stderr, "Mix_OpenAudio: %s\n", SDL_GetError());
        return 2;
    }

    /* Determine if OGG is supported. */
    mix_init = Mix_Init(mix_flags);

    if ((mix_init & mix_flags) != mix_flags) {
        fprintf(stderr,
                "OGG support in SDL_mixer is required for sound; aborting!\n");
        return 3;
    }

    /* Allocate channels and resize buffers accordingly. */
    Mix_AllocateChannels(settings.max_chunk);
    chunk = calloc(settings.max_chunk, sizeof(Mix_Chunk *));

    if (!chunk) {
        fprintf(stderr, "Could not allocate sound buffers.\n");
        return 4;
    }

    return 0;
}

/**
 * Play a sound effect using the SDL_mixer sound system.
 *
 * @param soundnum  The sound to play.
 * @param soundtype 0 for normal sounds, 1 for spell_sounds.
 * @param x         Offset (assumed from player) to play sound used to
 *                  determine value and left vs. right speaker balance.
 * @param y         Offset (assumed from player) to play sound used to
 *                  determine value and left vs. right speaker balance.
 */
static void play_sound(int soundnum, int soundtype, int x, int y) {
    Sound_Info *si;
    char path[MAXSOCKBUF];
    int i;
    static int channel_next = 0;

    /* Ignore commands to play invalid sound types. */
    if (soundnum >= MAX_SOUNDS || soundnum < 0) {
        fprintf(stderr, "play_sound: Invalid sound number: %d\n", soundnum);
        return;
    }

    /* Do not play sound if the channel limit has been reached. */
    if (Mix_Playing(-1) >= settings.max_chunk) {
        fprintf(stderr, "play_sound: too many sounds are already playing\n");
        return;
    }

    /* Find a channel that isn't playing anything. */
    /* TODO: Replace this with a channel group. */
    for (i = 0; i < settings.max_chunk; i++) {
        /* Go back to the first channel if there are no more. */
        if (channel_next >= settings.max_chunk - 1) {
            channel_next = 0;
        } else {
            channel_next++;
        }

        if (!Mix_Playing(channel_next)) {
            /* Free next channel if it isn't already empty. */
            if (!chunk[channel_next]) {
                Mix_FreeChunk(chunk[channel_next]);
                chunk[channel_next] = NULL;
            }

            break;
        }
    }

    /* Refuse to play a sound with an invalid type. */
    if (soundtype == SOUND_NORMAL || soundtype == SOUND_SPELL) {
        si = &sounds[soundnum];
    } else {
        fprintf(stderr,"play_sound: Unknown soundtype: %d\n", soundtype);
        return;
    }

    /* Refuse to play a sound without a name. */
    if (!si->filename) {
        fprintf(stderr, "play_sound: Sound %d (type %d) missing\n",
                soundnum, soundtype);
        return;
    }

    /* Try to load and play the sound. */
    snprintf(path, sizeof(path), "%s%s.wav", CLIENT_SOUNDS_PATH, si->filename);
    chunk[channel_next] = Mix_LoadWAV(path);

    if (!chunk[channel_next]) {
        fprintf(stderr, "Could not load sound from '%s': %s\n",
                path, SDL_GetError());
        return;
    }

    Mix_PlayChannel(-1, chunk[channel_next], 0);
}

/**
 * Play a music file.
 *
 * @param name Name of the song to play, without file paths or extensions.
 */
static void play_music(const char* music_name) {
    char path[MAXSOCKBUF];

    snprintf(path, sizeof(path), "%s%s%s.ogg", g_getenv("HOME"),
            USER_SOUNDS_PATH, music_name);

    music = Mix_LoadMUS(path);
    Mix_PlayMusic(music, 0);
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
static int parse_input(char *data, int len) {
    char* dptr;                         /* Pointer used when parsing data */
    char* sound = NULL;                 /* Points to a sound or music name */
    char* source = NULL;
    int   soundlen;
    int   spacelen;
    int   type = 0;
    int   dir = 0;
    int   vol = 0;
    int   x = 0;
    int   y = 0;
    int   i = 0;

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
        /* Play sound effect. */
        fprintf(stderr, "Playing sound "
                "%d,%d dir=%d vol=%d type=%d source=\"%s\" sound=\"%s\"\n",
                x, y, dir, vol, type, source, sound);
        play_sound(sound_to_soundnum(sound, type),
                type_to_soundtype(type), x, y);
        return 0;
    } else {
        /* Play music. */
#ifdef SOUND_DEBUG
        fprintf(stderr, "Playing music \"%s\"\n", sound);
#endif
        play_music(sound);
    }

    return 0;
}

/**
 * Clean up after itself.
 */
static void cleanup() {
    int i;

    printf("Cleaning up...\n");

    Mix_HaltMusic();
    Mix_FreeMusic(music);

    /* Halt all channels that are playing and free remaining samples. */
    Mix_HaltChannel(-1);

    for (i = 0; i < settings.max_chunk; i++) {
        if (chunk[i]) {
            Mix_FreeChunk(chunk[i]);
        }
    }

    /* Call Mix_Quit() for each time Mix_Init() was called. */
    while(Mix_Init(0)) {
        Mix_Quit();
    }
}

/**
 * Implement the SDL_mixer sound server.
 */
void sdl_mixer_server() {
    char inbuf[1024];

    printf("Starting SDL_mixer server...\n");
    atexit(cleanup);

    while (fgets(inbuf, sizeof(inbuf), stdin) != NULL) {
        /* Parse input and sleep to avoid hogging CPU. */
        parse_input(inbuf, strlen(inbuf));
        SDL_Delay(50);
    }
}
