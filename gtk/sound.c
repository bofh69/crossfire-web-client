char *rcsid_gtk_sound_c =
    "$Id$";

/*
    CrossFire, A Multiplayer game for X-windows

    Copyright (C) 2001 Mark Wedel & Crossfire Development Team
    Copyright (C) 1992 Frank Tore Johansen

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


/*
 * This file contains the sound support for the client.  It does not
 * actually play sounds, but rather tries to run cfsndserver, which
 * is responsible for playing sounds.
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>
#include <client-types.h>
#include "client.h"


/* Got a pipe signal.  As of now, only thing we are piped to is the
 * sound client.
 */
static void signal_pipe(int i) {
    /* do nothing, but perhaps do something more in the future */
}

static FILE *sound_pipe = NULL;
static ChildProcess *sound_process;
/* init_sounds open the audio device, and reads any configuration files
 * that need to be.  It returns 0 on success.  On failure, the calling
 * function will likely disable sound support/requests from the server.
 */

#ifndef WIN32
int init_sounds(void)
{
    char sound_path[MAX_BUF];

    /* Easy trick - global nosound is set in the arg processing - if set,
     * just return -1 - this way, the calling function only needs to check
     * the value of init_sounds, and not worry about checking nosound.
     */
    if (!want_config[CONFIG_SOUND]) return -1;

    if (sound_server[0] == '\0') {
        LOG(LOG_ERROR,"init_sounds:", "sound-server variable not set to anything");
	return -1;
    }
    /* if an absolute path is given, we use it unadorned.  Otherwise, we
     * use the path in the BINDIR.
     */
    if (sound_server[0] == '/')
	strcpy(sound_path, sound_server);
    else
	sprintf(sound_path, "%s/%s", BINDIR, sound_server);

    if (access(sound_path, X_OK)<0) {
	fprintf(stderr,"Unable to access %s sound server process\n", sound_path);
	return -1;
    }

    sound_process=raiseChild(sound_path,CHILD_STDIN|CHILD_STDOUT|CHILD_STDERR);
    logChildPipe(sound_process, LOG_INFO, CHILD_STDOUT|CHILD_STDERR);

    if (fcntl(sound_process->tube[0], F_SETFL, O_NONBLOCK)<0) {
	/* setting non blocking isn't 100% critical, but a good thing if
	 * we can do it	
	 */
	perror("init_sounds: Warning - unable to set non blocking on sound pipe\n");
    }
    sound_pipe=fdopen(sound_process->tube[0],"w");
    signal(SIGPIPE, signal_pipe);/*perhaps throwing this out :\*/
    return 0;
}
#endif


/* Plays sound 'soundnum'.  soundtype is 0 for normal sounds, 1 for
 * spell_sounds.  This might get extended in the future.  x,y are offset
 * (assumed from player) to play sound.  This information is used to
 * determine value and left vs right speaker balance.
 *
 * This procedure seems to be very slow - much slower than I would
 * expect. Might need to run this is a thread or fork off.
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


#ifndef WIN32
void SoundCmd(unsigned char *data,  int len)
{
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
}
#endif
