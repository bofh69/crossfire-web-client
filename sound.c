/*
 * static char *rcsid_sound_c =
 *   "$Id$";
 */

/*
 * This file contains the sound support for the client.  It is written to
 * be optimized for linux, however it should probably work on any system
 * with /dev/audio support (will just lose the stereo and volume adjustments.)
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <sys/ioctl.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <signal.h>

#ifndef SOUNDTEST
#include "client.h"
#include "proto.h"
#endif

#include "newclient.h"
#include "soundsdef.h"


int nosound=0;

#ifndef NEW_SOUND
#ifdef linux
#include <sys/soundcard.h>
#endif

/*#define SOUND_DEBUG*/
/* Change if appropriate.  Should probably put this in #ifdefs for various
 * machine types.
 */

#define AUDIODEV "/dev/audio"

/* mixer device isn't strictly needed - it allows us to do volume control
 * and stereo/non stereo.
 */
#define MIXERDEV "/dev/mixer"

/* Maximum sound number we are likely to ever receive from the server/
 * be requested to play.
 */
#define MAX_SOUNDS 1024

/* Determines the buffer size for the read & writes from sound file to sound
 * device. Large sizes should give marginally better perfomance.
 */
#define SOUND_BUF 65536

int fragsize = SOUND_BUF;
/*
 * SOUND_DECREASE determines by how much the sound decreases for each space
 * away from the player.  Volumes are from 0 to 100 - a decrease of 8.0
 * means that a sound 12.5 spaces away can not be heard.  8.0 is what the
 * server used for rplay, so it seemed reasonable to use it here. 
 */
#define SOUND_DECREASE 8.0

#define MAX_VOLUME 100

/* I found that at least on my hardware, the sound volumes as controlled
 * by the card are note very linear (anything below volume 50 or so is
 * barely perciptible.)  So instead, we use this lookup table to give
 * reasonable volumes compared to what we calculate.
 *
 * If using soundtest, the volumes for volumetest are 100, 92, 84, 76,
 * 68, 60, 52, 44, 36, 28
 */
static char volume_table[MAX_VOLUME+1] = {
50,				/* 0 */
50,50,50,50,50,51,51,51,51,51,	/* 10 */
52,52,52,53,53,53,54,54,54,55,	/* 20 */
55,56,56,57,57,58,58,59,59,60,	/* 30 */
60,61,61,62,62,63,63,64,64,65,	/* 40 */
65,66,66,67,67,68,68,69,69,70,	/* 50 */
70,71,71,72,72,73,73,74,74,75,	/* 60 */
75,76,76,77,77,78,78,79,79,80,	/* 70 */
80,81,81,82,82,83,83,84,84,85,	/* 80 */
85,86,86,87,87,88,88,89,89,90,	/* 90 */
91,92,93,94,95,96,97,98,99,100 };   /* 100 */


/*
 * Nothing after here should be modified - start of data structures we use.
 */

int soundfd=-1,mixerfd=-1, volumecontrol=0, stereo=0;
extern int errno;

typedef struct Sound_Info {
    char *filename;
    char *symbolic;
    unsigned char volume;
    int size;
} Sound_Info;

Sound_Info normal_sounds[MAX_SOUNDS], spell_sounds[MAX_SOUNDS],
default_normal, default_spell;


#ifndef SOUNDTEST
/* parses a line from the sound file.  This is a little uglier because
 * we store some static values in the function so we know what we are doing -
 * however, it is somewhat necessary so that we can use this same function
 * to parse both files and the compiled in data.
 * 
 * Note that this function will modify the data in line.  lineno is just
 * for error tracking purposes.
 *
 * This function uses strdup_local, which will not be available if compiled
 * standalone - that function only exists when we link into the rest of the
 * client.
 */

static void parse_sound_line(char *line, int lineno) {
    static int readtype=0, lastnum=0;
    int newnum, len;
    char *cp,*volume,*symbolic,*cp1;

    if (line[0]=='#' || line[0]=='\n') return;

    if (!strcmp(line,"Standard Sounds:\n")) {
	lastnum=0; 
	readtype=1;
	return;
    }
    if (!strcmp(line,"Spell Sounds:\n")) {
	lastnum=0; 
	readtype=2;
	return;
    }
    if (!readtype) {
#ifdef SOUND_DEBUG
	fprintf(stderr,"Got input without finding section header yet:\n%d:%s\n",
		lineno, line);
#endif
	return;
    }

    if (line[strlen(line)-1]=='\n') line[strlen(line)-1]='\0';

    len=strcspn(line, " \t");
    line[len]='\0';
    cp = line+len+1;

/*    fprintf(stderr,"line now equals %s\n", line);*/
    cp++;

    /* Skip all whitespace for the next field */
    while (*cp!='\0' && (*cp==' ' || *cp=='\t'))
	cp++;
    volume=cp;

    /* No symbolic name or number - that is ok */
    cp1=cp;
    if (!(cp=strchr(cp1,' ')) && !(cp=strchr(cp1,'\t'))) {
	newnum=lastnum+1;
	symbolic=NULL;
    } else {	/* We think we have a symbolic name */
	/* Don't need to nulterm the volume, since we atoi it anyways */
	while (*cp!='\0' && (*cp==' ' || *cp=='\t')) 
	    cp++;

	symbolic=cp;
	/* Some symbolc names are double quote protected.  If, do some
	 * special processing.  We strip off the quotes.
	 */
	if (*symbolic=='"') {
	    symbolic++;
	    for (cp=symbolic; *cp!='\0' && *cp!='"'; cp++) ;
	    *cp='\0';
	    cp++;
	}
	/* Lets try to find the sound number now */
	cp1 = cp;
	if (!(cp=strchr(cp1,' '))  && !(cp=strchr(cp1,'\t')))
	    newnum=lastnum+1;
	else {
	    *cp++='\0';
	    while (*cp!='\0' && (*cp==' ' || *cp=='\t')) 
		cp++;
	    if (isdigit(*cp))
		newnum=atoi(cp);
	    else newnum=lastnum+1;
	}
    }
    if (newnum < 0 || newnum>MAX_SOUNDS) {
#ifdef SOUND_DEBUG
	fprintf(stderr,"Invalid sound number %d, line %d, buf %s\n",
		newnum, lineno, line);
#endif
	return;
    }

    if (symbolic && !strcmp(symbolic,"DEFAULT")) {
	if (readtype==1) {
	    default_normal.filename=strdup_local(line);
	    default_normal.volume=atoi(volume);
	} else if (readtype==2) {
	    default_spell.filename=strdup_local(line);
	    default_spell.volume=atoi(volume);
	}
	return;
    }
    else {
	if (readtype==1) {
	    normal_sounds[newnum].filename = strdup_local(line);
	    normal_sounds[newnum].volume = atoi(volume);
	    if (symbolic) normal_sounds[newnum].symbolic=strdup_local(symbolic);
	    else normal_sounds[newnum].symbolic=NULL;
/*	    fprintf(stderr,"Added normal sound %d:%s (%d)\n", newnum, line, normal_sounds[newnum].volume);*/
	} else if (readtype==2) {
	    spell_sounds[newnum].filename = strdup_local(line);
	    spell_sounds[newnum].volume = atoi(volume);
	    if (symbolic) spell_sounds[newnum].symbolic=strdup_local(symbolic);
	    else spell_sounds[newnum].symbolic=NULL;
/*	    fprintf(stderr,"Added spell sound %d:%s (%d)\n", newnum, line, spell_sounds[newnum].volume);*/
	}
	lastnum=newnum;
    }
}
#endif

#else  /* NEW_SOUND */

/* Got a pipe signal.  As of now, only thing we are piped to is the
 * sound client.
 */
void signal_pipe(int i) {
    /* do nothing, but perhaps do something more in the future */
}

FILE *sound_pipe;

#endif /* NEW_SOUND */

/* init_sounds open the audio device, and reads any configuration files
 * that need to be.  It returns 0 on success.  On failure, the calling
 * function will likely disable sound support/requests from the server.
 */

int init_sounds()
{
#ifndef NEW_SOUND
    int i,mask;
    FILE *fp;
    char path[256], buf[512];
#endif 

    /* Easy trick - global nosound is set in the arg processing - if set,
     * just return -1 - this way, the calling function only needs to check
     * the value of init_sounds, and not worry about checking nosound.
     */
    if (nosound) return -1;
    
#ifndef NEW_SOUND
    soundfd=open(AUDIODEV, O_WRONLY|O_NDELAY);
    if (soundfd==-1) {
	fprintf(stderr,"Unable to open %s, errno %d\n", AUDIODEV, errno);
	return -1;
    }
#ifdef OPEN_SOUND_SYSTEM
    mixerfd=open(MIXERDEV, O_WRONLY|O_NDELAY);
    /* Lack of a mixer isn't critical - print out a error message, but
     * continue onward.
     */
    if (mixerfd==-1) {
	fprintf(stderr,"Unable to open mixer %s, errno %d.  Volume & channel control is disabled\n",
		MIXERDEV, errno);
    }
    if (mixerfd!=-1) {
	if (ioctl(mixerfd, SOUND_MIXER_READ_DEVMASK, &mask)==-1)
	    fprintf(stderr,"Unable to read mixer capabilities\n");
	else {
	    fprintf(stderr,"mixer mask returned %0x\n", mask);
	    if (mask & (1<<SOUND_MIXER_VOLUME)) {
		fprintf(stderr,"We have volume control\n");
		volumecontrol=1;
	    }
	}
	if (ioctl(mixerfd, SOUND_MIXER_READ_STEREODEVS, &mask)==-1)
	    fprintf(stderr,"Failed ioctl on stereo inquiry, %d\n", errno);
	else if (mask & 0x01) {
	    fprintf(stderr,"Volume control can do stereo\n");
	    stereo=1;
	}
    }

   if (ioctl(soundfd, SNDCTL_DSP_GETBLKSIZE, &fragsize) == -1) {
    fprintf(stderr,"Unable to get fragment size of sound device\n");
    fragsize = SOUND_BUF;
   } else {
    if (fragsize > SOUND_BUF) fragsize=SOUND_BUF;
    fprintf(stderr,"Sound: Using %d fragment size\n",fragsize);
   }
#endif

    for (i=0; i<MAX_SOUNDS; i++) {
	normal_sounds[i].filename=NULL;
	spell_sounds[i].filename=NULL;
	normal_sounds[i].size=-1;
	spell_sounds[i].size=-1;
    }
    default_normal.filename=NULL;
    default_spell.filename=NULL;

#ifndef SOUNDTEST
    /* SOUNDTEST is really meant for debugging the audio driver, and not
     * the actual loading of data - as such, might as well minimize what
     * we are testing/doing.
     */
    sprintf(path,"%s/.crossfire/sounds", getenv("HOME"));
    i=0;
    if (!(fp=fopen(path,"r"))) {
	fprintf(stderr,"Unable to open %s - will use built in defaults\n", path);
	for (; i<sizeof(def_sounds)/sizeof(char*); i++) {
	    strcpy(buf, def_sounds[i]);
	    parse_sound_line(buf,i);
	}
    } else while (fgets(buf, 511, fp)!=NULL) {
	buf[511]='\0';
	parse_sound_line(buf, ++i);
    }
    /* Note in both cases below, we leave the symbolic name untouched. */
    for (i=0; i<MAX_SOUNDS; i++) {
	if (!normal_sounds[i].filename) {
	    normal_sounds[i].filename=default_normal.filename;
	    normal_sounds[i].volume=default_normal.volume;
	}
	if (!spell_sounds[i].filename) {
	    spell_sounds[i].filename=default_spell.filename;
	    spell_sounds[i].volume=default_spell.volume;
	}
    }
#endif

#else /* NEW_SOUND */

    sound_pipe=popen("cfsndserv","w");
    if (!sound_pipe){
      perror("cfsndserver");
      return -1;
    }
    signal(SIGPIPE, signal_pipe);

#endif /* NEW_SOUND */
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
#ifndef NEW_SOUND
    int readfd=-1,writecount=0, readcount=0, volume=0,offset;
    char soundbuf[SOUND_BUF];
    Sound_Info *si;

#ifdef OPEN_SOUND_SYSTEM
    audio_buf_info bufinfo;
#endif

    if (soundnum>=MAX_SOUNDS || soundnum<0) {
	fprintf(stderr,"Invalid sound number: %d\n", soundnum);
	return;
    }
    if (soundfd==-1) {
	fprintf(stderr,"Sound device is not open\n");
	return;
    }
    if (soundtype==SOUND_NORMAL) {
	si = &normal_sounds[soundnum];
    }
    else if (soundtype==SOUND_SPELL) {
	si = &spell_sounds[soundnum];
    }
    else {
	fprintf(stderr,"Unknown soundtype: %d\n", soundtype);
	return;
    }
    if (!si->filename) {
	fprintf(stderr,"Sound %d (type %d) is not defined\n", soundnum, soundtype);
	return;
    }
#ifdef OPEN_SOUND_SYSTEM
    /* Opening  the device nonblocking under linux does no good - it ignores
     * it.  So instead, we check the sound buffer output space here.
     * the stat call is a little costly, but we only call it once for each
     * sound, so it shouldn't be that bad.
     */
    if (ioctl(soundfd, SNDCTL_DSP_GETOSPACE, &bufinfo)==-1)
	fprintf(stderr,"Error getting buffering information\n");
    else {
	if (si->size==-1) {
	    struct stat statbuf;

	    if (stat(si->filename, &statbuf)==-1) {
		fprintf(stderr,"Unable to stat %s\n", si->filename);
		si->filename=NULL;  /* Don't want to retry */
		return;
	    }
	    si->size = statbuf.st_size;
	}
	if (bufinfo.bytes < si->size) {
#ifdef SOUND_DEBUG
	    fprintf(stderr,"sound: buffers %d/%d - won't writed\n", 
		bufinfo.bytes, si->size);
#endif
	    return;
	}
    }

    /* One bug I have noticed in this - new volume controls take effect
     * immediately, even if it is still playing some other sound.  short of
     * aborting the sound we are currently playing, I don't know if there is
     * a good way around it.
     */
    if (volumecontrol && mixerfd!=-1) {
	float distance;

	/* Server uses isqrt - using the floating point version makes sense to
	 * me, since ware going to multiply it with another value, so this
	 * should give a bit better resolution.
	 */
	distance=sqrt(x*x + y*y);
	volume = si->volume - (int)(SOUND_DECREASE*distance);

#ifdef SOUNDTEST
	fprintf(stderr,"Calculated volume is 0x%02x\n", volume);
#endif

	/* In stereo, low 8 bits is left channel, next 8 bits is right. */
	/* We determine left & right by the x value (how the screen physically
	 * looks to the player, not left & right relative to the character -
         * first case is easier, and probalby makes more sense to the player.
         */
	if (stereo) {
	    int leftvol, rightvol;

	    if (distance==0.0) {
		leftvol=volume;
		rightvol=volume;
	    }
	    else {
		/* diff will be near zero for something directly above/below,
		 * and near 1 for something directly to the left or right.
		 * diff can basically be used to determine how much to
		 * volume to reduce the off channel by (percentage.)  This,
		 * for a diff near one (directly left/right), the off channel
		 * is reduced to virtually nothing.  For something directly
		 * above/below us, diff is near zero, and the off channel is
		 * hardly reduced at all.
		 */
		float diff = (1-fabs((float)x / distance));
		if (x<0) {
		    leftvol=volume;
		    rightvol = volume * diff;
		} else {
		    leftvol = volume * diff;
		    rightvol=volume;
		}
	    }
#ifdef SOUND_DEBUG
	    fprintf(stderr,"Stereo volumes = %d/%d\n", leftvol, rightvol);
#endif
	    if (leftvol>MAX_VOLUME) leftvol=MAX_VOLUME;
	    if (rightvol>MAX_VOLUME) rightvol=MAX_VOLUME;
	    volume = volume_table[leftvol] | (volume_table[rightvol]<<8);
	}
	else {
	    if (volume>MAX_VOLUME) volume=MAX_VOLUME;
	    volume=volume_table[volume];
	}
	if (ioctl(mixerfd, SOUND_MIXER_WRITE_VOLUME, &volume)==-1)
               /* An undefined mixer channel was accessed */
	    fprintf(stderr,"Unable to set volume, errno %d\n", errno);
    }
#endif
    readfd=open(si->filename, O_RDONLY);
    if (readfd==-1) {
	fprintf(stderr,"Unable to open %s, error %d\n", si->filename, errno);
	return;
    }
#ifdef SOUND_DEBUG
    fprintf(stderr,"Playing sound %s at volume 0x%02x\n", si->filename, volume);
#endif

    /* Skip over the garbage we don't want.  This only applies for
     * for au files, which is all we currently play.
     */
    read(readfd,soundbuf,8);
    offset = soundbuf[4]<<24 | soundbuf[5]<<16 | soundbuf[6]<<8 | soundbuf[7];
#ifdef SOUND_DEBUG
    fprintf(stderr,"Sound: offset is %x\n", offset);
#endif
    read(readfd, soundbuf, offset);

    /* fragmentsize should be optimum for the sound device driver */
    while ((readcount=read(readfd, soundbuf, fragsize))>0) {
	writecount=write(soundfd, soundbuf, readcount);
	if (writecount==-1) {
	    fprintf(stderr,"Error writing to sound file: %d\n", errno);
	    close(readfd);
	    return;
	}
    }
    if (readcount==-1) {
	fprintf(stderr,"Error reading sound file: %d\n", errno);
    }
    close(readfd);
#ifdef OPEN_SOUND_SYSTEM
    if (ioctl(soundfd, SNDCTL_DSP_POST, 0)==-1) 
	fprintf(stderr,"Error with DSP_POST ioctl\n");
#endif
#else /* NEW_SOUND */
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
#endif /* NEW_SOUND */
}

/* SOUNDTEST is defined if you do a 'make sound'.  This creates a standalone
 * program called sound which does some simple testing - useful for testing
 * when porting to a new system.
 */

#ifdef SOUNDTEST
/* Filename of a good test sound. */
char *testsound="/usr/local/lib/sounds/su-fanf.au";

int main(int argc, char *argv)
{
    int stereotest=0,volumetest=1;

    if (init_sounds()) exit(1);
    normal_sounds[0].filename=testsound;
    normal_sounds[0].volume=100;
    if (stereotest) {
	fprintf(stderr,"playing sound at full volume, centered on player\n");
	play_sound(0, 0, 0, 0);
	sleep(10);

	fprintf(stderr,"playing sound, offset by x 5 to player (right speaker only)\n");
	play_sound(0, 0, 5, 0);
	sleep(10);

	fprintf(stderr,"playing sound, offset by x -5 to player (left speaker only)\n");
	play_sound(0, 0, -5, 0);
	sleep(10);

	fprintf(stderr,"playing sound, offset by y 5 to player (both speakers same volume)\n");
	play_sound(0, 0, 0, 5);
	sleep(10);

	fprintf(stderr,"playing sound, offset by 5,5 to player (both speakers, righ slightly louder)\n");
	play_sound(0, 0, 5, 5);
	sleep(10);
    }
    if (volumetest) {
	int i;

	for (i=0; i<13; i++) {
	    fprintf(stderr,"Playing sound, y=%d\n", i);
	    play_sound(0,0,0,i);
	    sleep(8);
	}
    }

    exit(0);
}
#else

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

#endif
