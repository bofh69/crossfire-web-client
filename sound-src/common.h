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
    int stereo;
    int bit8;
    int sign;
    int frequency;
    int buflen;
    int max_chunk;                 /**< Max number of sounds to queue. */
} sound_settings;

extern int init_audio();

#endif
