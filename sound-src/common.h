#ifndef _SOUND_SRC_COMMON_H
#define _SOUND_SRC_COMMON_H

extern int cf_snd_init();
extern void cf_snd_exit();

extern void cf_play_music(const char *music_name);
extern void cf_play_sound(gint8 x, gint8 y, guint8 dir, guint8 vol, guint8 type,
                          char const sound[static 1],
                          char const source[static 1]);

#endif
