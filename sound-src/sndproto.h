/* common.c */
extern int init_sounds(void);
extern int sound_to_soundnum(const char *name, uint8 type);
extern int type_to_soundtype(uint8 type);
extern int StdinCmd(char *data, int len);
extern int write_settings(void);
extern int read_settings(void);
