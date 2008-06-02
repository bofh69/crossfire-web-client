const char * const rcsid_gtk2_sound_c =
    "$Id$";

/*
    CrossFire, A Multiplayer game for X-windows

    Copyright (C) 2005 Mark Wedel & Crossfire Development Team

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

    The author can be reached via e-mail to crossfire@metalforge.org
*/

/**
 * @file gtk-v2/src/sound.c
 * This file contains the sound support for the GTK V2 client.  It does not
 * actually play sounds, but rather tries to run cfsndserve, which is
 * responsible for playing sounds.
 */

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <client-types.h>
#include "client.h"

FILE *sound_pipe=NULL;
ChildProcess* sound_process;

/**
 * Opens the audio device, and reads relevant configuration files.
 *
 * @return
 * Returns 0 on success.  On failure, the calling function will likely disable
 * sound support/requests from the server.
 */
int init_sounds(void)
{
#ifndef WIN32
    char sound_path[MAX_BUF];

    /*
     * Easy trick - global nosound is set in the arg processing - if set, just
     * return -1 - this way, the calling function only needs to check the value
     * of init_sounds, and not worry about checking nosound.
     */
    if (!want_config[CONFIG_SOUND]) return -1;

    if (sound_server[0] == '\0') {
        LOG(LOG_ERROR,"init_sounds:", "sound-server variable not set to anything");
        return -1;
    }
    /*
     * If an absolute path is given, we use it unadorned.  Otherwise, we use
     * the path in the BINDIR.
     */
    if (sound_server[0] == '/')
        strcpy(sound_path, sound_server);
    else
        snprintf(sound_path, sizeof(sound_path),"%s/%s", BINDIR, sound_server);

    if (access(sound_path, X_OK)<0) {
        fprintf(stderr,"Unable to access %s sound server process\n", sound_path);
        return -1;
    }

    sound_process=raiseChild(sound_path,CHILD_STDIN|CHILD_STDOUT|CHILD_STDERR);
    logChildPipe(sound_process, LOG_INFO, CHILD_STDOUT|CHILD_STDERR);

    if (fcntl(sound_process->tube[0], F_SETFL, O_NONBLOCK)<0) {
        /*
         * Setting non-blocking isn't 100% critical, but a good thing if
         * possible.
         */
        perror("init_sounds: Warning - unable to set non blocking on sound pipe\n");
    }
    sound_pipe=fdopen(sound_process->tube[0],"w");
    return 0;
#else
    return -1;
#endif
}

/**
 * Plays sound 'soundnum'.  This procedure seems to be very slow - much slower
 * than expected. It might need to run in a thread or fork off.
 *
 * @param soundnum
 * The sound to play.
 * @param soundtype
 * 0 for normal sounds, 1 for spell_sounds.  This might get extended in the
 * future.
 * @param x
 * Offset (assumed from player) to play sound used to determine value and left
 * vs right speaker balance.
 * @param y
 * Offset (assumed from player) to play sound used to determine value and left
 * vs right speaker balance.
 */
static void play_sound(int soundnum, int soundtype, int x, int y)
{
#ifndef WIN32

    if (!use_config[CONFIG_SOUND]) return;
    if ( (fprintf(sound_pipe,"%4x %4x %4x %4x\n",soundnum,soundtype,x,y)<=0) ||
         (fflush(sound_pipe)!=0) ){
        LOG(LOG_ERROR,"gtk::play_sound","couldn't write to sound pipe: %d",errno);
        use_config[CONFIG_SOUND]=0;
        fclose(sound_pipe);
        sound_process=NULL;
        return;
    }
#endif
}

/**
 * ?
 *
 * @param data
 * @param len
 */
void SoundCmd(unsigned char *data,  int len)
{
#ifndef WIN32
    int x, y, num, type;

    if (len!=5) {
        LOG(LOG_WARNING,"gtk::SoundCmd","Got invalid length on sound command: %d", len);
        return;
    }
    x = data[0];
    y = data[1];
    num = GetShort_String(data+2);
    type = data[4];

#if 0
    fprintf(stderr,"Playing sound %d (type %d), offset %d, %x\n",
            num, type, x ,y);
#endif
    play_sound(num, type, x, y);
#endif
}

void Sound2Cmd(unsigned char *data, int len)
{
#ifndef WIN32
    uint8 x, y, dir, volume, type, len_action;
    char* action = NULL;
    uint8 len_name;
    char* name = NULL;
    /* sound2 <x><y><dir><volume><type><len of action>action<len of name>name */
    /*         b  b  b    b       b     b             str    b           str*/
    if (len<8) {
        LOG(LOG_WARNING, "gtk::Sound2Cmd", "Got too short length on sound2 command: %d\n", len);
        return;
    }
    x = data[0];
    y = data[1];
    dir = data[2];
    volume = data[3];
    type = data[4];
    len_action = data[5];
    /* Prevent invald index. */
    if (len_action >= (len-8)) {
        LOG(LOG_WARNING, "gtk::Sound2Cmd", "Bad length of \"len of action\" in sound2 command: %d\n", len);
        return;
    }
    if (len_action != 0) {
        action = (char*)data+6;
        data[6+len_action]='\0';
    }
    /* Lets make it readable, compiler will optimize the addition order anyway*/
    len_name = data[6+len_action+1];
    if (len_name >= (len-8-len_action)) {
        LOG(LOG_WARNING, "gtk::Sound2Cmd", "Bad length of \"len of name\" in sound2 command: %d\n", len);
        return;
    }
    if (len_name != 0) {
        name = (char*)data+6+len_action+1;
        data[6+len_action+1+len_name]='\0';
    }
    LOG(LOG_WARNING, "gtk::Sound2Cmd", "Playing sound2 x=%hhd y=%hhd dir=%hhd volume=%hhd type=%hhd\n",
        x, y, dir, volume, type);
    LOG(LOG_WARNING, "gtk::Sound2Cmd", "               len_action=%hhd action=%s\n", len_action, action);
    LOG(LOG_WARNING, "gtk::Sound2Cmd", "               len_name=%hhd name=%s\n", len_name, name);
    LOG(LOG_WARNING, "gtk::Sound2Cmd", "Please impement sound2!");
    /* TODO: Play sound here. Can't implement/test as server never actually
     * sends this yet it seems. As this code is mostly duplicated between the
     * different clients, make sure to update the other ones too.
     */
#endif
}

void MusicCmd(const char *data, int len) {
#ifndef WIN32
    if (!strncmp(data, "NONE", len)) {
        /* TODO stop music */
    } else {
        LOG(LOG_WARNING, "gtk::MusicCmd", "music command: %s (Implement me!)\n", data);
        /* TODO: Play music. Can't impmement/test as server doesn't send this
         * version of the command yet it seems.
         */
    }
#endif
}
