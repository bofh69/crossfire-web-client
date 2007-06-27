static char *rcsid_cfsndserv_c =
    "$Id$";
/*
    Crossfire client, a client program for the crossfire program.

    Copyright (C) 2001 Mark Wedel & Crossfire Development Team
    Copyright (C) 2003 Tim Hentenaar

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

/* Comment from the original author is below.  In addition to OSS
 * and ALSA, sun sound is also supported.
 */

/*
 * (c) 1998 Jacek Konieczny <jajcus@zeus.polsl.gliwice.pl>
 *
 * This file contains the server for sound support for the client.
 * It supports both ALSA_SOUND and OSS_SOUND. Any other sound system support
 * can be easily added - only new init_audio and audio_play
 * need be written.
 *
 * If you have any problems please e-mail me.
 */


/*#define ALSA_SOUND*/
/*#define OSS_SOUND*/
/*#define SGI_SOUND*/
/*#define SUN_SOUND*/

/*#define SOUND_DEBUG*/

#include <config.h>

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <math.h>

#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <stdlib.h>
#include <ctype.h>
#include <errno.h>

#include "newclient.h"
#include "soundsdef.h"


#if defined(ALSA_SOUND)
#  include <sys/asoundlib.h>
#  define AUDIODEV "/dev/dsp"
    snd_pcm_t *handle=NULL;
#elif defined(OSS_SOUND)
#  include <sys/soundcard.h>
#  define AUDIODEV "/dev/dsp"
#elif defined(SGI_SOUND)
#  include <audio.h>
#  define AUDIODEV "/foo/bar"
#elif defined(SUN_SOUND)
#  include <sys/audioio.h>
#  define AUDIODEV "/dev/audio"
#else
#error Not known sound system defined
#endif

#define CONFIG_FILE "/.crossfire/sndconfig"
#define MAX_SOUNDS 1024

/*
 * A replacement of strdup(), since it's not defined at some
 * unix variants.
 */

char *strdup_local(char *str) {
  char *c=(char *)malloc(sizeof(char)*strlen(str)+1);
  strcpy(c,str);
  return c;
}



typedef struct Sound_Info {
    char *filename;
    char *symbolic;
    unsigned char volume;
    int size;
    unsigned char *data;
} Sound_Info;

Sound_Info normal_sounds[MAX_SOUNDS], spell_sounds[MAX_SOUNDS],
default_normal, default_spell;

#define SOUND_DECREASE 0.1

/* mixer variables */
char *buffers=NULL;
int *sounds_in_buffer=NULL;
int current_buffer=0; /* Next buffer we will write out */
int first_free_buffer=0; /* So we know when to stop playing sounds */

int soundfd=0;

/* sound device parameters */
int stereo=0,bit8=0,sample_size=0,frequency=0,sign=0,zerolevel=0;

#ifdef SUN_SOUND
struct sound_settings{
    int stereo, bit8, sign, frequency, buffers, buflen,simultaneously;
    const char *audiodev;
} settings={0,1,1,11025,100,4096,4,AUDIODEV};

#else

struct sound_settings{
    int stereo, bit8, sign, frequency, buffers, buflen,simultaneously;
    const char *audiodev;
} settings={0,1,0,11025,100,1024,4,AUDIODEV};

#endif

/* parses a line from the sound file.  This is a little uglier because
 * we store some static values in the function so we know what we are doing -
 * however, it is somewhat necessary so that we can use this same function
 * to parse both files and the compiled in data.
 *
 * Note that this function will modify the data in line.  lineno is just
 * for error tracking purposes.
 */

static void parse_sound_line(char *line, int lineno) {
    static int readtype=0, lastnum=0;
    int newnum, len;
    char *cp,*volume,*symbolic,*cp1,filename[512];

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
	fprintf(stderr,"Invalid sound number %d, line %d, buf %s\n",
		newnum, lineno, line);
	return;
    }

    /* Compatibility processing for older files - if the file ends in
     * .au, convert to .raw.  A bit of a hack, but probably better than
     * trying to play an au file.
     */
    strcpy(filename, line);
    cp = filename + strlen(filename)-3;
    if (!strcmp(cp, ".au"))
	strcpy(cp, ".raw");

    if (symbolic && !strcmp(symbolic,"DEFAULT")) {
	if (readtype==1) {
	    default_normal.filename=strdup_local(filename);
	    default_normal.volume=atoi(volume);
	} else if (readtype==2) {
	    default_spell.filename=strdup_local(filename);
	    default_spell.volume=atoi(volume);
	}
	return;
    }
    else {
	if (readtype==1) {
	    normal_sounds[newnum].filename = strdup_local(filename);
	    normal_sounds[newnum].volume = atoi(volume);
	    if (symbolic) normal_sounds[newnum].symbolic=strdup_local(symbolic);
	    else normal_sounds[newnum].symbolic=NULL;
	} else if (readtype==2) {
	    spell_sounds[newnum].filename = strdup_local(filename);
	    spell_sounds[newnum].volume = atoi(volume);
	    if (symbolic) spell_sounds[newnum].symbolic=strdup_local(symbolic);
	    else spell_sounds[newnum].symbolic=NULL;
	}
	lastnum=newnum;
    }
}

#if defined(ALSA_SOUND)
int init_audio(){

    int card=0,device=0,err;
    snd_pcm_channel_params_t params;

    printf("cfsndserv compiled for ALSA sound system\n");
    fflush(stdout);

    if ( (err = snd_pcm_open( &handle, card, device, SND_PCM_OPEN_PLAYBACK )) <0 ) {
	fprintf( stderr, "open failed: %s\n", snd_strerror( err ) );
	return -1;
    }

    params.channel = SND_PCM_CHANNEL_PLAYBACK;
    params.mode = SND_PCM_MODE_BLOCK;

    if (settings.bit8)
	params.format.format = settings.sign?SND_PCM_SFMT_S8:SND_PCM_SFMT_U8;
    else
	params.format.format = settings.sign?SND_PCM_SFMT_S16_LE:SND_PCM_SFMT_U16_LE;

     params.format.rate = settings.frequency;
     params.format.voices = settings.stereo?2:1;
     params.buf.block.frag_size = settings.buflen/2;
     params.buf.block.frags_max = 2;
     params.buf.block.frags_min = 1;

     if ( (err = snd_pcm_channel_params( handle, &params )) < 0 ) {
	fprintf( stderr, "format setup failed: %s\nTrying defaults\n"
                                 , snd_strerror( err ) );
	params.format.format = SND_PCM_SFMT_U8;
	params.format.rate = 11025;
	params.format.voices = 1;
	if ( (err = snd_pcm_channel_params( handle, &params )) < 0 ) {
	    fprintf( stderr, "format setup failed: %s\n", snd_strerror( err ) );
	    snd_pcm_close( handle );
	    return -1;
	}
     }
     switch(params.format.format){
	case SND_PCM_SFMT_S8:
  	   bit8=1;
	   sign=1;
	   break;
	case SND_PCM_SFMT_U8:
  	   bit8=1;
	   sign=0;
	   break;
	case SND_PCM_SFMT_S16_LE:
  	   bit8=0;
	   sign=1;
	   break;
	case SND_PCM_SFMT_U16_LE:
  	   bit8=0;
	   sign=0;
	   break;
        default:
           fprintf(stderr,"Coulnd't set proper format\n");
           return -1;
     }

     sample_size=params.format.voices*(bit8?1:2);
     stereo=(params.format.voices==1)?0:1;
     frequency=params.format.rate;

     soundfd=snd_pcm_file_descriptor(handle,SND_PCM_CHANNEL_PLAYBACK);
     snd_pcm_nonblock_mode( handle, 1 );

     return 0;
}

int audio_play(int buffer,int off){

  return snd_pcm_write(handle,buffers+settings.buflen*buffer+off,settings.buflen-off);
}

#elif defined(OSS_SOUND)

int init_audio(void){

  const char *audiodev;
  int value,format,tmp;

  printf("cfsndserv compiled for OSS sound system\n");
  fflush(stdout);

  /* Open the audio device */
  if ( (audiodev=getenv("AUDIODEV")) == NULL ) {
          audiodev = settings.audiodev;
  }
  soundfd = open(audiodev, (O_WRONLY|O_NONBLOCK), 0);
  if ( soundfd < 0 ) {
           fprintf(stderr,"Couldn't open %s: %s\n", audiodev, strerror(errno));                return(-1);
  }

  /* Set the audio buffering parameters */
  value=0;
  for(tmp=settings.buflen/2;tmp;tmp>>=1) value++;

  value |= 0x00020000;
  if ( ioctl(soundfd, SNDCTL_DSP_SETFRAGMENT, &value) < 0 ) {
            fprintf(stderr,"Couldn't set audio fragment spec\n");
            return(-1);
  }
  if (settings.bit8)
    format=settings.sign?AFMT_S8:AFMT_U8;
  else
    format=settings.sign?AFMT_S16_LE:AFMT_U16_LE;

  value=format;
  if ( (ioctl(soundfd, SNDCTL_DSP_SETFMT,&value) < 0) ||
                                            (value != format) ) {
             fprintf(stderr,"Couldn't set audio format\n");
  }

  switch(value){
     case AFMT_S16_LE:
         bit8=0;
         sign=1;
         break;
     case AFMT_U16_LE:
         bit8=0;
         sign=0;
         break;
     case AFMT_S8:
         bit8=1;
         sign=1;
         break;
     case AFMT_U8:
         bit8=1;
         sign=0;
         break;
     default:
         return -1;
  }

  stereo = settings.stereo;
  ioctl(soundfd, SNDCTL_DSP_STEREO, &stereo);

  frequency = settings.frequency;
  if ( ioctl(soundfd, SOUND_PCM_WRITE_RATE, &frequency) < 0 ) {
          fprintf(stderr,"Couldn't set audio frequency\n");
          return(-1);
  }
  sample_size=(bit8?1:2)*(stereo?2:1);
  return 0;
}

int audio_play(int buffer,int off){
    int wrote;
#ifdef SOUND_DEBUG
    printf("audio play - writing starting at %d, %d bytes",
	  settings.buflen*buffer+off,settings.buflen-off);
    fflush(stdout);
#endif
  wrote=write(soundfd,buffers+settings.buflen*buffer+off,settings.buflen-off);
#ifdef SOUND_DEBUG
    printf("...wrote %d bytes\n", wrote);
    fflush(stdout);
#endif
  return wrote;
}
/* End of OSS sound */

#elif defined(SGI_SOUND)

ALconfig	soundconfig;
ALport	soundport;

int init_audio()
{
	long		params[2];

	printf("cfsndserv compiled for SGI sound system\n");
	fflush(stdout);

	/* Allocate ALconfig structure. */

	if ((soundconfig=ALnewconfig())==0)
	{
		fprintf(stderr,"Could not allocate ALconfig structure.\n");
		return -1;
	}

	/* Set number of channels */

	if (ALsetchannels(soundconfig,(stereo=settings.stereo)?2:1)==-1)
	{
		fprintf(stderr,"Could not set number of channels.\n");
		return -1;
	}

	/* Set sample format */

	if (ALsetsampfmt(soundconfig,AL_SAMPFMT_TWOSCOMP)==-1)
	{
		fprintf(stderr,"Could not set audio sample format.\n");
		return -1;
	}
	sign=1;

	/* Set sample width */

	if (ALsetwidth(soundconfig,(bit8=settings.bit8)?AL_SAMPLE_8:AL_SAMPLE_16)==-1)
	{
		fprintf(stderr,"Could not set audio sample width.\n");
		return -1;
	}
	sample_size=(stereo?2:1)*(bit8?1:2);

	/* Set frequency */

	params[0]=AL_OUTPUT_RATE;
	params[1]=frequency=settings.frequency;
	if (ALsetparams(AL_DEFAULT_DEVICE,params,2)==-1)
	{
		fprintf(stderr,"Could not set output rate of default device.\n");
		return -1;
	}

	/* Open audio port */

	if ((soundport=ALopenport("cfsndserv port","w",soundconfig))==NULL)
	{
		fprintf(stderr,"Could not open audio port.\n");
		return -1;
	}
	soundfd=ALgetfd(soundport);
	return 0;
}

int audio_play(int buffer,int off)
{
	ALwritesamps(soundport,buffers+settings.buflen*buffer+off,(settings.buflen-off)/sample_size);
	return settings.buflen-off;
}

#elif defined(SUN_SOUND)

int init_audio(){

  const char *audiodev;
  int value,format,tmp;
  audio_info_t	audio_info;
  audio_device_t audio_device;

  printf("cfsndserv compiled for SUN sound system\n");
  fflush(stdout);

  /* Open the audio device */
  if ( (audiodev=getenv("AUDIODEV")) == NULL ) {
          audiodev = settings.audiodev;
  }
  soundfd = open(audiodev, (O_WRONLY|O_NONBLOCK), 0);
  if ( soundfd < 0 ) {
           fprintf(stderr,"Couldn't open %s: %s\n", audiodev, strerror(errno));                return(-1);
  }

  if (ioctl(soundfd, AUDIO_GETDEV, &audio_device) < 0) {
    fprintf(stderr,"Couldn't get audio device ioctl\n");
    return(-1);
  }
  if ( ioctl(soundfd, AUDIO_GETINFO, &audio_info) < 0 ) {
    fprintf(stderr,"Couldn't get audio information ioctl\n");
    return(-1);
  }
  /* The capabilities on different sun hardware vary wildly.
   * We attempt to get a working setup no matter what hardware we are
   * running on.
   */

   /* This is sparc 10, sparc 20 class systems */
   if (!strcmp(audio_device.name, "SUNW,dbri")) {
	/* To use linear encoding, we must use 16 bit and some fixed
	 * frequencies.  11025 matches what the rest of the systems use
	 */
	audio_info.play.precision = 16;
	audio_info.play.encoding=AUDIO_ENCODING_LINEAR;
	audio_info.play.sample_rate=11025;
   }
    /* This is used on many of the ultra machines */
   else if (!strcmp(audio_device.name, "SUNW,CS4231")) {
	/* To use linear encoding, we must use 16 bit and some fixed
	 * frequencies.  11025 matches what the rest of the systems use
	 */
	audio_info.play.precision = 16;
	audio_info.play.encoding=AUDIO_ENCODING_LINEAR;
	audio_info.play.sample_rate=11025;
   }

  audio_info.play.channels = settings.stereo?2:1;
  stereo= settings.stereo;

  bit8=(audio_info.play.precision==8)?1:0;
  frequency=settings.frequency;
  sample_size=(bit8?1:2)*(stereo?2:1);
  fprintf(stderr,"SUN_SOUND: bit8=%d, stereo=%d, freq=%d, sample_size=%d\n",
	  bit8, stereo, frequency, sample_size);

  if ( ioctl(soundfd, AUDIO_SETINFO, &audio_info) < 0 ) {
            perror("Couldn't set audio information ioctl");
            return(-1);
  }
  return 0;
}

int audio_play(int buffer,int off){
    int wrote;
#ifdef SOUND_DEBUG
    printf("audio play - writing starting at %d, %d bytes",
	  settings.buflen*buffer+off,settings.buflen-off);
    fflush(stdout);
#endif
  wrote=write(soundfd,buffers+settings.buflen*buffer+off,settings.buflen-off);
#ifdef SOUND_DEBUG
    printf("...wrote %d bytes\n", wrote);
    fflush(stdout);
#endif
  return wrote;
}
/* End of Sun sound */

#endif



/* init_sounds open the audio device, and reads any configuration files
 * that need to be.  It returns 0 on success.  On failure, the calling
 * function will likely disable sound support/requests from the server.
 */

int init_sounds(void)
{
    int i;
    FILE *fp;
    char path[256], buf[512];

#ifdef SOUND_DEBUG
    fprintf( stderr,"Settings: bits: %i, ",settings.bit8?8:16);
    fprintf( stderr,"%s, ",settings.sign?"signed":"unsigned");
    fprintf( stderr,"%s, ",settings.stereo?"stereo":"mono");
    fprintf( stderr,"frequency: %i, ",settings.frequency);
    fprintf( stderr,"device: %s\n",settings.audiodev);
#endif

    buffers = (char *)malloc( settings.buffers * settings.buflen );
    if ( !buffers ) return -1;
    sounds_in_buffer = (int *)calloc( settings.buffers,sizeof(int) );
    if ( !sounds_in_buffer ) return -1;

    if (init_audio()) return -1;

    if (sign) zerolevel=0;
    else zerolevel=bit8?0x80:0x8000;

    memset(buffers,zerolevel,settings.buflen*settings.buffers);

#ifdef SOUND_DEBUG
    fprintf( stderr,"bits: %i, ",bit8?8:16);
    fprintf( stderr,"%s, ",sign?"signed":"unsigned");
    fprintf( stderr,"%s, ",stereo?"stereo":"mono");
    fprintf( stderr,"freq: %i, ",frequency);
    fprintf( stderr,"smpl_size: %i, ",sample_size);
    fprintf( stderr,"0level: %i\n",zerolevel);
#endif

    for (i=0; i<MAX_SOUNDS; i++) {
	normal_sounds[i].filename=NULL;
	spell_sounds[i].filename=NULL;
	normal_sounds[i].size=-1;
	spell_sounds[i].size=-1;
    }
    default_normal.filename=NULL;
    default_spell.filename=NULL;

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
	normal_sounds[i].data=NULL;
	spell_sounds[i].data=NULL;
    }
    return 0;
}

/* Plays sound 'soundnum'.  soundtype is 0 for normal sounds, 1 for
 * spell_sounds.  This might get extended in the future.  x,y are offset
 * (assumed from player) to play sound.  This information is used to
 * determine value and left vs right speaker balance.
 * This doesn't really play a sound, rather it just addes it to
 * the buffer to be played later on.
 */

static void play_sound(int soundnum, int soundtype, int x, int y)
{
    Sound_Info *si;
    int buf,off;
    int i;
    unsigned left_ratio,right_ratio;
    double dist;

    buf=current_buffer;
    if (buf>=settings.buffers) buf=1;

    if (buf == 0) buf++;

    /* check if the buffer isn't full */
#ifdef SOUND_DEBUG
    fprintf(stderr,"Sounds in buffer %i: %i\n",buf,sounds_in_buffer[buf]);
#endif
    if (sounds_in_buffer[buf]>settings.simultaneously) return;

    if (soundnum>=MAX_SOUNDS || soundnum<0) {
	fprintf(stderr,"Invalid sound number: %d\n", soundnum);
	return;
    }
    if (soundfd==-1) {
	fprintf(stderr,"Sound device is not open\n");
	return;
    }

    if (soundtype < SOUND_NORMAL || soundtype == 0) soundtype = SOUND_NORMAL;

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

    /*
     *   Load the sound if it is not loaded yet.
     *
     */
    if (!si->data){
       FILE *f;
       struct stat sbuf;
#ifdef SOUND_DEBUG
       fprintf(stderr,"Loading file: %s\n",si->filename);
#endif
       if (stat(si->filename,&sbuf)){
           perror(si->filename);
           return;
       }
       si->size=sbuf.st_size;
       if (si->size <=0 ) return;
       if (si->size*sample_size > settings.buflen*(settings.buffers-1) ){
          fprintf(stderr,"Sound %s too long (%i > %i)\n",si->filename,si->size,
                    settings.buflen*(settings.buffers-1)/sample_size);
          return;
       }
       si->data=(unsigned char *)malloc(si->size);
       f=fopen(si->filename,"r");
       if (!f){
           perror(si->filename);
           return;
       }
       fread(si->data,1,si->size,f);
       fclose(f);
    }

#ifdef SOUND_DEBUG
    fprintf(stderr,"Playing sound %i (%s), volume %i, x,y=%d,%d\n",soundnum,si->symbolic,si->volume,x,y);
#endif
    /* calculate volume multiplers */
    dist=sqrt(x*x+y*y);
    right_ratio=left_ratio=((1<<16)*si->volume)/(100*settings.simultaneously*(1+SOUND_DECREASE*dist));
    if (stereo){
      double diff;
      if (dist)
        diff=(1.0-fabs((double)x/dist));
      else
        diff=1;
#ifdef SOUND_DEBUG
      printf("diff: %f\n",diff);
      fflush(stdout);
#endif
      if (x<0) right_ratio*=diff;
      else left_ratio*=diff;
    }

#ifdef SOUND_DEBUG
    fprintf(stderr,"Ratio: %i, %i\n",left_ratio,right_ratio);
#endif

    /* insert the sound to the buffers */
    sounds_in_buffer[buf]++;
    off=0;
    for(i=0;i<si->size;i++){
        int dat=si->data[i]-0x80;

        if (bit8){
	  if (!stereo){
	     buffers[buf*settings.buflen+off]+=(dat*left_ratio)>>16;
	  }
          else{
	    buffers[buf*settings.buflen+off]+=(dat*left_ratio)>>16;
	    buffers[buf*settings.buflen+off+1]+=(dat*right_ratio)>>16;
	  }
        }
        else{ /* 16 bit output */
	  if (!stereo){
#ifdef WORDS_BIGENDIAN
	     buffers[buf*settings.buflen+off+1]+=((dat*left_ratio)>>8)&0xff;
	     buffers[buf*settings.buflen+off]+=(dat*left_ratio)>>16;
	  }
          else{
	    buffers[buf*settings.buflen+off+1]+=((dat*left_ratio)>>8)&0xff;
	    buffers[buf*settings.buflen+off]+=(dat*left_ratio)>>16;
	    buffers[buf*settings.buflen+off+3]+=((dat*right_ratio)>>8)&0xff;
	    buffers[buf*settings.buflen+off+2]+=(dat*right_ratio)>>16;
	  }
#else
	     buffers[buf*settings.buflen+off]+=((dat*left_ratio)>>8)&0xff;
	     buffers[buf*settings.buflen+off+1]+=(dat*left_ratio)>>16;
	  }
          else{
	    buffers[buf*settings.buflen+off]+=((dat*left_ratio)>>8)&0xff;
	    buffers[buf*settings.buflen+off+1]+=(dat*left_ratio)>>16;
	    buffers[buf*settings.buflen+off+2]+=((dat*right_ratio)>>8)&0xff;
	    buffers[buf*settings.buflen+off+3]+=(dat*right_ratio)>>16;
	  }
#endif
        }

        off+=sample_size;

        if (off>=settings.buflen){
          off=0;
          buf++;
          if (buf>=settings.buffers) {
	    buf=0;
	  }
        }
    }
#ifdef SOUND_DEBUG
    fprintf(stderr,"Added %d bytes, last buffer=%d, lastpos=%d\n",
	    si->size, buf, off);
#endif
    /* This write did not wrap the buffers */
    if (buf+1 > current_buffer) {
	if ((buf+1 > first_free_buffer) && (first_free_buffer >= current_buffer))
	    first_free_buffer = buf+1;
    } else {	/* Buffers did wrap */
	if (((buf+1 > first_free_buffer) && (first_free_buffer < current_buffer)) ||
	    (first_free_buffer >= current_buffer))
		first_free_buffer = buf+1;
    }
    if (first_free_buffer >= settings.buffers) first_free_buffer=0;

}

int SoundCmd(unsigned char *data,  int len)
{
    int x, y, num, type;
    int i;

    i=sscanf((char *)data,"%x %x %x %x",&num,&type,&x,&y);
    if (i!=4){
        fprintf(stderr,"Wrong input!\n");
	return -1;
    }
#ifdef SOUND_DEBUG
    fprintf(stderr,"Playing sound %d (type %d), offset %d, %d\n",
	    num, type, x ,y);
#endif
    play_sound(num, type, x, y);
    return 0;
}

int write_settings(void) {
FILE *f;
char *home;
char *path;

  if ( (home=getenv("HOME")) == NULL ) return -1;
  path=(char *)malloc(strlen(home)+strlen(CONFIG_FILE)+1);
  if (!path) return -1;
  strcpy(path,home);
  strcat(path,CONFIG_FILE);
  f=fopen(path,"w");
  if (!f) return -1;
  fprintf(f,"# Crossfire sound server settings\n");
  fprintf(f,"# Please note, that not everything will work\n\n");
  fprintf(f,"stereo: %i\n",settings.stereo);
  fprintf(f,"bits: %i\n",settings.bit8?8:16);
  fprintf(f,"signed: %i\n",settings.sign);
  fprintf(f,"frequency: %i\n",settings.frequency);
  fprintf(f,"buffers: %i\n",settings.buffers);
  fprintf(f,"buflen: %i\n",settings.buflen);
  fprintf(f,"simultaneously: %i\n",settings.simultaneously);
/*  fprintf(f,"device: %s\n",settings.audiodev);*/
  fclose(f);
  return 0;
}

int read_settings(void) {
    FILE *f;
    char *home;
    char *path;
    char linebuf[1024];
    if ( (home=getenv("HOME")) == NULL ) return 0;

    path=(char *)malloc(strlen(home)+strlen(CONFIG_FILE)+1);
    if (!path) return 0;

    strcpy(path,home);
    strcat(path,CONFIG_FILE);

    f=fopen(path,"r");
    if (!f) return -1;

    while(fgets(linebuf,1023,f)!=NULL) {
	linebuf[1023]=0;
	/* Strip off the newline */
	linebuf[strlen(linebuf)-1]=0;

	if (strncmp(linebuf,"stereo:",strlen("stereo:"))==0)
	    settings.stereo=atoi(linebuf+strlen("stereo:"))?1:0;
	else if (strncmp(linebuf,"bits:",strlen("bits:"))==0)
	    settings.bit8=(atoi(linebuf+strlen("bits:"))==8)?1:0;
	else if (strncmp(linebuf,"signed:",strlen("signed:"))==0)
	    settings.sign=atoi(linebuf+strlen("signed:"))?1:0;
	else if (strncmp(linebuf,"buffers:",strlen("buffers:"))==0)
	    settings.buffers=atoi(linebuf+strlen("buffers:"));
	else if (strncmp(linebuf,"buflen:",strlen("buflen:"))==0)
	    settings.buflen=atoi(linebuf+strlen("buflen:"));
	else if (strncmp(linebuf,"frequency:",strlen("frequency:"))==0)
	    settings.frequency=atoi(linebuf+strlen("frequency:"));
	else if (strncmp(linebuf,"simultaneously:",strlen("simultaneously:"))==0)
	    settings.simultaneously=atoi(linebuf+strlen("simultaneously:"));
#if 0
	else if (strncmp(linebuf,"device: ",strlen("device: "))==0)
		settings.audiodev=strdup_local(linebuf+strlen("device: "));
#endif
    }
    fclose(f);
    return 0;
}

int main(int argc, char *argv[])
{
    int infd;
    char inbuf[1024];
    int inbuf_pos=0,sndbuf_pos=0;
    fd_set inset,outset;

    printf ("%s\n",rcsid_cfsndserv_c);
    fflush(stdout);
    if (read_settings()) write_settings();
    if (init_sounds()) return 1;
    /* we don't use the file descriptor method */
    if (!soundfd) return 1;
    infd=fileno(stdin);
    FD_ZERO(&inset);
    FD_ZERO(&outset);
    FD_SET(soundfd,&outset);
    FD_SET(infd,&inset);
    while(1){
#if defined(SGI_SOUND)
      /*
      The buffer of an audio port can hold 100000 samples. If we allow sounds to
      be written to the port whenever there is enough room in the buffer, all
      sounds will be played sequentially, which is wrong. We can set the
      fillpoint to a high value to prevent this.
      */
      ALsetfillpoint(soundport,100000);
#endif


      select(FD_SETSIZE,&inset,&outset,NULL,NULL);

      if (FD_ISSET(soundfd,&outset)){
         /* no sounds to play */
         if (current_buffer==first_free_buffer) FD_CLR(soundfd,&outset);
	 else{
	   int wrote;
           wrote=audio_play(current_buffer,sndbuf_pos);
           if (wrote<settings.buflen-sndbuf_pos) sndbuf_pos+=wrote;
           else{
             /* clean the buffer */
	     memset(buffers+settings.buflen*current_buffer,zerolevel,settings.buflen);
             sounds_in_buffer[current_buffer]=0;
	     sndbuf_pos=0;
	     current_buffer++;
             if (current_buffer>=settings.buffers) current_buffer=0;
           }
	}
      } else {
	/* We need to reset this if it is not set - otherwise, we will never
	 * finish playing the sounds
	 */
	FD_SET(soundfd,&outset);
      }

      if (FD_ISSET(infd,&inset)){
        int err=read(infd,inbuf+inbuf_pos,1);
	if (err<1 && errno!=EINTR){
	  if (err<0) perror("read");
	  break;
	}
        if (inbuf[inbuf_pos]=='\n'){
	  inbuf[inbuf_pos++]=0;
          if (!SoundCmd((unsigned char*)inbuf,inbuf_pos)) FD_SET(soundfd,&outset);
          inbuf_pos=0;
        }
        else{
          inbuf_pos++;
          if (inbuf_pos>=1024){
             fprintf(stderr,"Input buffer overflow!\n");
             inbuf_pos=0;
          }
        }
      }
      FD_SET(infd,&inset);
    }

    return 0;
}
