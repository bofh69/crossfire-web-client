/*
 * static char *rcsid_sound_c =
 *   "$Id$";
 */

/*
 * This file contains the sound support for the client.  It is written to
 * be optimized for linux, however it should probably work on any system
 * with /dev/audio support (will just lose the stereo and volume adjustments.)
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


/* init_sounds open the audio device, and reads any configuration files
 * that need to be.  It returns 0 on success.  On failure, the calling
 * function will likely disable sound support/requests from the server.
 */

int init_sounds()
{
    /* Easy trick - global nosound is set in the arg processing - if set,
     * just return -1 - this way, the calling function only needs to check
     * the value of init_sounds, and not worry about checking nosound.
     */
    if (nosound) return -1;
    

    sound_pipe=popen(BINDIR "cfsndserv","w");
    /* if its not in its proper place, let the users shell
     * try to find it - it may have a better idea.
     */
    if (!sound_pipe)
	sound_pipe=popen("cfsndserv","w");

    if (!sound_pipe){
      perror("cfsndserver");
      return -1;
    }
    signal(SIGPIPE, signal_pipe);

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

