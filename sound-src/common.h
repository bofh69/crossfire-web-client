#ifndef _SOUND_SRC_COMMON_H
#define _SOUND_SRC_COMMON_H

/* Just do this so I don't have to go hunt down the macro */
#define CLIENT_SOUNDS_PATH CF_SOUND_DIR

#define USER_SOUNDS_PATH "/.crossfire/sound.cache/"
#define MAX_SOUNDS 1024

#define SOUND_DEBUG

typedef struct Sound_Info {
    char *filename;
    char *symbolic;
    unsigned char volume;
} Sound_Info;

extern Sound_Info sounds[MAX_SOUNDS];

typedef struct sound_settings {
    int buflen;     //< how big the buffers should be
    int max_chunk;  //< number of sounds that can be played at the same time
} sound_settings;

extern int init_audio();
extern void sdl_mixer_server();

#endif
