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

#include "common.h"

#define MAX_SOUNDS 1024

typedef struct Sound_Info {
    char *filename;
    char *symbolic;
    unsigned char volume;
} Sound_Info;

typedef struct sound_settings {
    int buflen;     //< how big the buffers should be
    int max_chunk;  //< number of sounds that can be played at the same time
} sound_settings;

static Mix_Music *music = NULL;
static sound_settings settings = { 512, 4 };

static Sound_Info sounds[MAX_SOUNDS];

static GHashTable* chunk_cache;

/**
 * Convert a sound name to a sound number to help with the transition of the
 * sound server from sound support to sound2 capability.  This is not an end
 * solution, but one that gets the sound server working a little bit until a
 * better one can be implemented.
 */
static int sound_to_soundnum(const char *name, guint8 type) {
    for (int i = 0; i < MAX_SOUNDS; i++) {
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
static int type_to_soundtype(guint8 type) {
    if (type == 2) {
        return 1;
    } else {
        return 0;
    }
}

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
 * Load sound definitions from a file.
 */
static void init_sounds() {
    /* Initialize by setting all sounds to NULL. */
    for (int i = 0; i < MAX_SOUNDS; i++) {
        sounds[i].filename = NULL;
    }

    /* Try to open the sound definitions file. */
    FILE *fp = fopen(g_getenv("CF_SOUND_CONF"), "r");
    if (fp == NULL) {
        fprintf(stderr, "Could not find sound definitions; aborting!\n");
        exit(EXIT_FAILURE);
    }
    printf("Loaded sounds from '%s'\n", g_getenv("CF_SOUND_DIR"));

    /* Use 'i' as index tracker, so set it to zero. */
    int i = 0;

    /* Parse the sound definitions file, line by line. */
    char buf[512];
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
int cf_snd_init() {
    /* Set $CF_SOUND_DIR to something reasonable, if not already set. */
    if (!g_setenv("CF_SOUND_DIR", CF_SOUND_DIR, FALSE)) {
        perror("Couldn't set $CF_SOUND_DIR");
        return -1;
    }

    /* Set $CF_SOUND_CONF to something reasonable, if not already set. */
    char path[MAXSOCKBUF];
    snprintf(path, sizeof(path), "%s/sounds.conf", g_getenv("CF_SOUND_DIR"));

    if (!g_setenv("CF_SOUND_CONF", path, FALSE)) {
        perror("Couldn't set $CF_SOUND_CONF");
        return -1;
    }

    /* Initialize sound definitions. */
    chunk_cache = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
                                        (void *)Mix_FreeChunk);
    init_sounds();

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
 * @param soundnum  The sound to play.
 * @param soundtype 0 for normal sounds, 1 for spell_sounds.
 * @param x         Offset (assumed from player) to play sound used to
 *                  determine value and left vs. right speaker balance.
 * @param y         Offset (assumed from player) to play sound used to
 *                  determine value and left vs. right speaker balance.
 */
static void play_sound(int soundnum, int soundtype, int x, int y) {
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

    /* Refuse to play a sound with an invalid type. */
    Sound_Info *si;
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

    Mix_Chunk* chunk = load_chunk(si->filename);
    if (chunk != NULL) {
        Mix_PlayChannel(-1, chunk, 0);
    }
}

void cf_play_sound(gint8 x, gint8 y, guint8 dir, guint8 vol, guint8 type,
                   char const sound[static 1], char const source[static 1]) {
    play_sound(sound_to_soundnum(sound, type), type_to_soundtype(type), x, y);
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
    Mix_FadeInMusic(music, -1, 500);
}

void cf_snd_exit() {
#ifdef SOUND_DEBUG
    puts("Cleaning up...");
#endif
    Mix_HaltMusic();
    Mix_FreeMusic(music);

    /* Halt all channels that are playing and free remaining samples. */
    Mix_HaltChannel(-1);
    g_hash_table_destroy(chunk_cache);

    /* Call Mix_Quit() for each time Mix_Init() was called. */
    while(Mix_Init(0)) {
        Mix_Quit();
    }
}
