/*
    Crossfire client, a client program for the crossfire program.

    Copyright (C) 2001 Mark Wedel & Crossfire Development Team

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    The author can be reached via e-mail to crossfire-devel@real-time.com
*/

/**
 * @file sound-src/common.h
 *
 */

#ifndef _SOUND_SRC_COMMON_H
#define _SOUND_SRC_COMMON_H

#define CONFIG_FILE "/.crossfire/sndconfig"
#define MAX_SOUNDS 1024

extern char *def_sounds[];

extern char *buffers;

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
    int buffers;
    int buflen;
    int simultaneously;                 /**< Max number of sounds to queue. */
    const char *audiodev;
} sound_settings;

extern sound_settings settings;

/*
 * Sound device parameters
 */
extern int stereo;
extern int bit8;
extern int sample_size;
extern int frequency;
extern int sign;
extern int zerolevel;

extern int *sounds_in_buffer;

/* From ../common/libcfclient.a */
extern char *strdup_local(const char *str);

#endif /* _SOUND_SRC_COMMON_H */

