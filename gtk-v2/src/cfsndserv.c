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

#include "client-vala.h"
#include "sound.h"

typedef struct sound_settings {
    int buflen;     //< how big the buffers should be
    int max_chunk;  //< number of sounds that can be played at the same time
} sound_settings;

static Mix_Music *music = NULL; //< current music sample
static char *music_playing; //< current or next music name

static sound_settings settings = { 512, 4 };
static const int fade_time_ms = 1000;

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
    LOG(LOG_DEBUG, "cf_play_sound",
        "Playing sound2 x=%hhd y=%hhd dir=%hhd volume=%hhd type=%hhd sound=%s "
        "source=%s", x, y, dir, vol, type, sound, source);

    SoundInfo* si = g_hash_table_lookup(sounds, sound);
    if (si == NULL) {
        LOG(LOG_WARNING, "cf_play_sound", "sound not defined: %s", sound);
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
    if (music_playing == NULL)
        return true;
    if (strcmp(music, music_playing) != 0) {
        return true;
    }
    return false;
}

static bool find_music_path(const char *name, char *path, size_t len) {
    snprintf(path, len, "%s/music/%s.ogg", g_getenv("CF_SOUND_DIR"), name);
    if (g_file_test(path, G_FILE_TEST_EXISTS))
        return true;
    snprintf(path, len, "%s/music/%s.mp3", g_getenv("CF_SOUND_DIR"), name);
    if (g_file_test(path, G_FILE_TEST_EXISTS))
        return true;
    return false;
}

void set_music_volume() {
    Mix_VolumeMusic(MIX_MAX_VOLUME * 3/4 * MIN(use_config[CONFIG_MUSIC_VOL], 100) / 100);
}

void cf_play_music_cb() {
    Mix_HookMusicFinished(NULL);
    if (music != NULL) {
        Mix_FreeMusic(music);
        music = NULL;
    }

    if (strcmp(music_playing, "NONE") == 0) {
        return;
    }
    char path[MAXSOCKBUF];
    if (!find_music_path(music_playing, path, sizeof(path))) {
        fprintf(stderr, "Could not find a music file (ogg or mp3) for %s in %s\n", music_playing, path);
        return;
    }
    music = Mix_LoadMUS(path);
    if (!music) {
        fprintf(stderr, "Could not load music: %s\n", Mix_GetError());
        return;
    }
    set_music_volume();
    Mix_FadeInMusic(music, -1, fade_time_ms);
}

/**
 * Play a music file.
 *
 * @param name Name of the song to play, without file paths or extensions.
 */
void cf_play_music(const char* music_name) {
    LOG(LOG_DEBUG, "cf_play_music", "\"%s\"", music_name);
    if (!music_is_different(music_name)) {
        return;
    }

    if (music_playing != NULL)
        g_free(music_playing);
    music_playing = g_strdup(music_name);
    if (Mix_FadeOutMusic(fade_time_ms)) {
        Mix_HookMusicFinished(cf_play_music_cb); // handle in callback to avoid blocking during fade-out
    } else {
        cf_play_music_cb(); // nothing playing, just do it
    }
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
