#ifndef _SOUND_SRC_COMMON_H
#define _SOUND_SRC_COMMON_H

#define CLIENT_SOUNDS_PATH CF_DATADIR "/sounds/"
#define USER_SOUNDS_PATH "/.crossfire/sound.cache/"
#define MAX_SOUNDS 1024

#define SOUND_DEBUG

typedef struct Sound_Info {
    char *filename;
    char *symbolic;
    unsigned char volume;
} Sound_Info;

extern Sound_Info normal_sounds[MAX_SOUNDS];
extern Sound_Info spell_sounds[MAX_SOUNDS];
extern Sound_Info default_normal;
extern Sound_Info default_spell;

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
