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

#include "client.h"

#include <SDL.h>
#include <SDL_mixer.h>
#include <glib-object.h>

#include "common.h"
#include "snd.h"

typedef struct sound_settings {
    int buflen;     //< how big the buffers should be
    int max_chunk;  //< number of sounds that can be played at the same time
} sound_settings;

static Mix_Music *music = NULL;
static sound_settings settings = { 512, 4 };

static GHashTable* chunk_cache;
static GHashTable* sounds;

/**
 * Initialize the sound subsystem.
 *
 * Currently, this means calling SDL_Init() and Mix_OpenAudio().
 *
 * @return Zero on success, anything else on failure.
 */
static int init_audio() {
    if (SDL_Init(SDL_INIT_AUDIO) == -1) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    if (Mix_OpenAudio(MIX_DEFAULT_FREQUENCY, MIX_DEFAULT_FORMAT, 2,
                      settings.buflen)) {
        fprintf(stderr, "Mix_OpenAudio: %s\n", SDL_GetError());
        return 2;
    }

    /* Determine if OGG is supported. */
    const int mix_flags = MIX_INIT_OGG;
    int mix_init = Mix_Init(mix_flags);
    if ((mix_init & mix_flags) != mix_flags) {
        fprintf(stderr,
                "OGG support in SDL_mixer is required for sound; aborting!\n");
        return 3;
    }

    /* Allocate channels and resize buffers accordingly. */
    Mix_AllocateChannels(settings.max_chunk);
    return 0;
}

/**
 * Initialize sound server.
 *
 * Initialize resource paths, load sound definitions, and ready the sound
 * subsystem.
 *
 * @return Zero on success, anything else on failure.
 */
int cf_snd_init() {
    /* Set $CF_SOUND_DIR to something reasonable, if not already set. */
    if (!g_setenv("CF_SOUND_DIR", CF_SOUND_DIR, FALSE)) {
        perror("Couldn't set $CF_SOUND_DIR");
        return -1;
    }

    /* Initialize sound definitions. */
    chunk_cache = g_hash_table_new_full(g_str_hash, g_str_equal, NULL,
                                        (void *)Mix_FreeChunk);
    sounds = load_snd_config();
    if (!sounds) {
        return -1;
    }

    /* Initialize audio library. */
    if (init_audio()) {
        return -1;
    }

    return 0;
}

static Mix_Chunk* load_chunk(char const name[static 1]) {
    Mix_Chunk* chunk = g_hash_table_lookup(chunk_cache, name);
    if (chunk != NULL) {
        return chunk;
    }

    char path[MAXSOCKBUF];
    snprintf(path, sizeof(path), "%s/%s", g_getenv("CF_SOUND_DIR"), name);
    chunk = Mix_LoadWAV(path);
    if (!chunk) {
        fprintf(stderr, "Could not load sound from '%s': %s\n", path,
                SDL_GetError());
        return NULL;
    }
    g_hash_table_insert(chunk_cache, &name, chunk);
    return chunk;
}

/**
 * Play a sound effect using the SDL_mixer sound system.
 *
 * @param sound     The sound to play.
 * @param type      0 for normal sounds, 1 for spell_sounds.
 * @param x         Offset (assumed from player) to play sound used to
 *                  determine value and left vs. right speaker balance.
 * @param y         Offset (assumed from player) to play sound used to
 *                  determine value and left vs. right speaker balance.
 */
void cf_play_sound(gint8 x, gint8 y, guint8 dir, guint8 vol, guint8 type,
                   char const sound[static 1], char const source[static 1]) {
    SoundInfo* si = g_hash_table_lookup(sounds, sound);
    if (si == NULL) {
        fprintf(stderr, "play_sound: sound not defined: %s\n", sound);
        return;
    }

    Mix_Chunk* chunk = load_chunk(si->file);
    if (chunk == NULL) {
        return;
    }
    Mix_VolumeChunk(chunk, si->vol * MIX_MAX_VOLUME / 100);

    int channel = Mix_GroupAvailable(-1);
    if (channel == -1) {
        g_warning("No free channels available to play sound");
        return;
    }
    Mix_Volume(channel, vol * MIX_MAX_VOLUME / 100);
    Mix_PlayChannel(channel, chunk, 0);
}

static bool music_is_different(char const music[static 1]) {
    static char last_played[MAXSOCKBUF] = "";
    if (strcmp(music, last_played) != 0) {
        g_strlcpy(last_played, music, MAXSOCKBUF);
        return true;
    }
    return false;
}

/**
 * Play a music file.
 *
 * @param name Name of the song to play, without file paths or extensions.
 */
void cf_play_music(const char* music_name) {
    if (!music_is_different(music_name)) {
        return;
    }

    Mix_FadeOutMusic(500);
    if (music != NULL) {
        Mix_FreeMusic(music);
    }

    if (strcmp(music_name, "NONE") == 0) {
        return;
    }
    char path[MAXSOCKBUF];
    snprintf(path, sizeof(path), "%s/music/%s.ogg", g_getenv("CF_SOUND_DIR"),
             music_name);
    music = Mix_LoadMUS(path);
    if (!music) {
        fprintf(stderr, "Could not load music: %s\n", Mix_GetError());
        return;
    }
    Mix_VolumeMusic(MIX_MAX_VOLUME * 3/4);
    Mix_FadeInMusic(music, -1, 500);
}

void cf_snd_exit() {
    Mix_HaltMusic();
    Mix_FreeMusic(music);

    /* Halt all channels that are playing and free remaining samples. */
    Mix_HaltChannel(-1);
    g_hash_table_destroy(chunk_cache);
    g_hash_table_destroy(sounds);

    /* Call Mix_Quit() for each time Mix_Init() was called. */
    while(Mix_Init(0)) {
        Mix_Quit();
    }
}
