#ifndef _SOUND_SRC_COMMON_H
#define _SOUND_SRC_COMMON_H

#define CLIENT_SOUNDS_PATH CF_DATADIR "/sounds/"
#define USER_SOUNDS_PATH "/.crossfire/sound.cache/"
#define USER_SOUNDS_FILE "/.crossfire/sounds"
#define MAX_SOUNDS 1024

#define SOUND_DEBUG

extern char *client_sounds_path;
extern char *user_sounds_path;
extern char *user_sounds_file;

typedef struct Sound_Info {
    char *filename;
    char *symbolic;
    unsigned char volume;
    int size;
    unsigned char *data;
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
    int simultaneously;                 /**< Max number of sounds to queue. */
} sound_settings;

extern sound_settings settings;

/*
 * Sound device parameters
 */
extern int stereo;
extern int bit8;
extern int frequency;
extern int sign;

extern int init_audio();

void play_sound(int soundnum, int soundtype, int x, int y);
void play_music(const char *name);

#endif
