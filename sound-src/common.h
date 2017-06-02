#ifndef _SOUND_SRC_COMMON_H
#define _SOUND_SRC_COMMON_H

/* Just do this so I don't have to go hunt down the macro */
#define CLIENT_SOUNDS_PATH CF_SOUND_DIR

#define USER_SOUNDS_PATH "/.crossfire/sound.cache/"

#define SOUND_DEBUG

extern int cf_snd_init();
extern void cf_snd_exit();

extern void cf_play_music(const char *music_name);
extern void cf_play_sound(gint8 x, gint8 y, guint8 dir, guint8 vol, guint8 type,
                          char const sound[static 1],
                          char const source[static 1]);

#endif
