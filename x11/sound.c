const char *rcsid_x11_sound_c =
    "$Id$";
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


/* This is a very lightweight file.  Basically, it just runs the
 * cfsndserv program and sends the writes to it.
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <signal.h>
#include <client-types.h>
#include "client.h"


int nosound=0;

/* Got a pipe signal.  As of now, only thing we are piped to is the
 * sound client.
 */
void signal_pipe(int i) {
    /* do nothing, but perhaps do something more in the future */
}

FILE *sound_pipe;
ChildProcess* sound_process;


/* init_sounds open the audio device, and reads any configuration files
 * that need to be.  It returns 0 on success.  On failure, the calling
 * function will likely disable sound support/requests from the server.
 */

int init_sounds(void)
{
    /* Easy trick - global nosound is set in the arg processing - if set,
     * just return -1 - this way, the calling function only needs to check
     * the value of init_sounds, and not worry about checking nosound.
     */
    if (!want_config[CONFIG_SOUND]) return -1;

    /*if (sound_process) //kill*/

    sound_process=raiseChild(BINDIR "/cfsndserv",CHILD_STDIN|CHILD_STDOUT|CHILD_STDERR);
    logChildPipe(sound_process, LOG_INFO, CHILD_STDOUT|CHILD_STDERR);
    sound_pipe=fdopen(sound_process->tube[0],"w");
    signal(SIGPIPE, signal_pipe);/*perhaps throwing this out :\*/
    return 0;
}



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
    if (nosound) return;

    if (fprintf(sound_pipe,"%4x %4x %4x %4x\n",soundnum,soundtype,x,y)<=0) {
	nosound=1;
	fclose(sound_pipe);
	return;
    }
    if (fflush(sound_pipe)!=0) {
	nosound=1;
	fclose(sound_pipe);
	return;
   }
}


void SoundCmd(unsigned char *data,  int len)
{  
    int x, y, num, type;

    if (len!=5) {
	fprintf(stderr,"Got invalid length on sound command: %d\n", len);
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

