
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "client.h"
#include "proto.h"
#include "newclient.h"
#include "libgnome/libgnome.h"

#define MAX_SOUNDS 1024

int nosound = 0;

int
init_sounds()
{
	if (nosound)
		return -1;
	return 0;
}

static void
play_sound(int soundnum, int soundtype, int x, int y)
{
	char confname[2048] = { 0 };
	char filename[2048] = { 0 };
	if (soundnum >= MAX_SOUNDS || soundnum < 0) {
		return;
	}
	strcpy(confname, "/sound/events/gnome-cfclient.soundlist/");
	if (soundtype == SOUND_NORMAL) {
		strcat(confname, "normal_");
		strcat(confname, g_strdup_printf("%d", soundnum));
	} else if (soundtype == SOUND_SPELL) {
		strcat(confname, "spell_");
		strcat(confname, g_strdup_printf("%d", soundnum));
	} else {
		return;
	}
	strcat(confname, "/file");
	if (gnome_config_get_string(confname) != NULL && strcmp(gnome_config_get_string(confname), "")) {
		if (*(gnome_config_get_string(confname)) != '/') {
			strcpy(filename, DATADIR "/sounds/");
			strcat(filename, gnome_config_get_string(confname));
		} else
			strcpy(filename, gnome_config_get_string(confname));
		gnome_sound_play(filename);
		return;
	} else if (gnome_config_get_string(confname) == NULL) {
		strcpy(confname, "=" SYSCONFDIR "/sound/events/gnome-cfclient.soundlist=/");
		if (soundtype == SOUND_NORMAL) {
			strcat(confname, "normal_");
			strcat(confname, g_strdup_printf("%d", soundnum));
		} else if (soundtype == SOUND_SPELL) {
			strcat(confname, "spell_");
			strcat(confname, g_strdup_printf("%d", soundnum));
		} else {
			return;
		}
		strcat(confname, "/file");
		if (gnome_config_get_string(confname) != NULL && strcmp(gnome_config_get_string(confname), "")) {
			if (*(gnome_config_get_string(confname)) != '/') {
				strcpy(filename, DATADIR "/sounds/");
				strcat(filename, gnome_config_get_string(confname));
			} else
				strcpy(filename, gnome_config_get_string(confname));
			gnome_sound_play(filename);
			return;
		}
	}
}

void
SoundCmd(unsigned char *data, int len)
{
	int x, y, num, type;
	if (len != 5) {
		fprintf(stderr, "Got invalid length on sound command: %d\n", len);
		return;
	}
	x = data[0];
	y = data[1];
	num = GetShort_String(data + 2);
	type = data[4];
	play_sound(num, type, x, y);
}
