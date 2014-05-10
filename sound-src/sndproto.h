#ifndef _SNDPROTO_H
#define _SNDPROTO_H

extern int StdinCmd(char *data, int len);
extern int init();
extern int sound_to_soundnum(const char *name, guint8 type);
extern int type_to_soundtype(guint8 type);
extern void sdl_mixer_server();

#endif
