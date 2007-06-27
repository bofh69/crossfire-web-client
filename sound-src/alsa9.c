static char *rcsid_cfsndserv_c =
    "$Id$";
/*
    Crossfire client, a client program for the crossfire program.

    Copyright (C) 2001-2005,  Mark Wedel & Crossfire Development Team

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


/* This file is only used for alsa 0.9 and later sound.  Alsa sound code is
 * sufficiently different from the rest that trying to keep it common didn't
 * make much sense.
 */

#define SOUND_DEBUG

/* Debugs the actual writing of data - this generally isn't that interesting,
 * and generates a lot of messages which tends to obscure the more interesting
 * ones.
 */
/*#define SOUND_DEBUG_WRITES */

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


#include <alsa/asoundlib.h>
#include <alsa/pcm_plugin.h>


#define ALSA_PCM_NEW_HW_PARAMS_API
#define AUDIODEV "default:0,0"
snd_pcm_t *handle = NULL;
int sndbuf_pos=0;

#define ALSA9_ERROR(str,err) { \
		fprintf(stderr,"ALSA9 Error: %s %s\n",str,snd_strerror(err)); }

#define CONFIG_FILE "/.crossfire/sndconfig"


#define SOUND_DECREASE 0.1

/* mixer variables */
int *sounds_in_buffer=NULL;
int current_buffer=0; /* Next buffer we will write out */
int first_free_buffer=0; /* So we know when to stop playing sounds */

int soundfd=0;

/* sound device parameters */
int stereo=0,sample_size=0,frequency=0,sign=0,zerolevel=0;

struct sound_settings{
    int stereo, bit8, sign, frequency, buffers, buflen,simultaneously;
    const char *audiodev;
} settings={0,1, 0, 8000,200,2048,4,AUDIODEV};

#include "common.c"


snd_pcm_hw_params_t *params;
static snd_pcm_uframes_t chunk_size = 0;
int err = 0;

void alsa_audio_close(void) { snd_pcm_close(handle); }

int init_audio(void) {
    snd_pcm_sw_params_t *sw_params;
    unsigned int format;

    printf("cfsndserv compiled for ALSA9 sound system\n");
    fflush(stdout);

    /* open the PCM device */
    if ((err = snd_pcm_open(&handle,AUDIODEV,SND_PCM_STREAM_PLAYBACK,0)) <0) {
	ALSA9_ERROR("init_audio(): ",err);
	exit(1);
    }

    atexit(alsa_audio_close);

    /* allocate and zero out params */
    snd_pcm_hw_params_alloca(&params);

    if ((err = snd_pcm_hw_params_any(handle,params)) <0) {
	ALSA9_ERROR("init_audio(): ",err);
	exit(1);
    }

    /* set the access mode (interleaved) */
    if ((err = snd_pcm_hw_params_set_access(handle,params,SND_PCM_ACCESS_RW_INTERLEAVED)) <0) {
	ALSA9_ERROR("init_audio(): ",err);
	exit(1);
    }

    /* set the format */

   if (settings.bit8)
       format = settings.sign?SND_PCM_FORMAT_S8:SND_PCM_FORMAT_U8;
   else
       format = SND_PCM_FORMAT_U16;


  if ((err = snd_pcm_hw_params_set_format(handle,params,format))<0) {
	ALSA9_ERROR("init_audio(): ",err);
	exit(1);
  }

  /* set the number of channels */
  if ((err = snd_pcm_hw_params_set_channels(handle,params,settings.stereo?2:1))<0) {
	ALSA9_ERROR("init_audio(): ",err);
	exit(1);
   }

  stereo = settings.stereo?1:0;

  /* set the rate (our frequency, or closest match) */
  unsigned int r = (unsigned int)settings.frequency;
  if (r == 0) r = 41100;
  int dir = 0;
  frequency = snd_pcm_hw_params_set_rate_near(handle,params,&r,&dir);

  /* get sample size */
  sample_size = (snd_pcm_format_physical_width(format) * (settings.stereo?2:1)) / 8;
  #ifdef SOUND_DEBUG
	printf("init_audio(): sample_size = %d\n",sample_size);
    fflush(stdout);
  #endif


    /* apply the settings */
    if ((err = snd_pcm_hw_params(handle,params))<0) {
	ALSA9_ERROR("init_audio(): ",err);
	exit(1);
    }

    err=snd_pcm_nonblock(handle, 1);
    if (err < 0) {
	ALSA9_ERROR("nonblock setting error: %s", err);
	exit(1);
    }

    if ((err = snd_pcm_sw_params_malloc (&sw_params)) < 0) {
	    fprintf (stderr, "cannot allocate software parameters structure (%s)\n",
				 snd_strerror (err));
	    exit (1);
    }
    if ((err = snd_pcm_sw_params_current (handle, sw_params)) < 0) {
	    fprintf (stderr, "cannot initialize software parameters structure (%s)\n",
				 snd_strerror (err));
	    exit (1);
    }
    if ((err = snd_pcm_sw_params_set_avail_min (handle, sw_params, 4096)) < 0) {
	    fprintf (stderr, "cannot set minimum available count (%s)\n",
				 snd_strerror (err));
	    exit (1);
    }
    if ((err = snd_pcm_sw_params_set_start_threshold (handle, sw_params, 0U)) < 0) {
	    fprintf (stderr, "cannot set start mode (%s)\n",
				 snd_strerror (err));
	    exit (1);
    }
    if ((err = snd_pcm_sw_params (handle, sw_params)) < 0) {
	    fprintf (stderr, "cannot set software parameters (%s)\n",
				 snd_strerror (err));
	    exit (1);
    }

    /* Zerolevel=0x80 seems to work best for both 8 and 16 bit audio */
    if (sign) zerolevel=0;
    else zerolevel=0x80;

    snd_pcm_hw_params_get_period_size(params, &chunk_size, 0);

    return 0;
}

void alsa_recover(int e) {
	/* Recover from various errors */
	if (e == -EAGAIN) {
	    return;
	} else if (e == -EPIPE) {
		err = snd_pcm_prepare(handle);
		if (err < 0) {
			ALSA9_ERROR("alsa_recover(): Unable to recover from underrun. ",err);
			return;
		}
	} else if (e == -ESTRPIPE) {
		while ((err = snd_pcm_resume(handle)) == -EAGAIN) sleep(1);
		if (err < 0) {
			err = snd_pcm_prepare(handle);
			if (err < 0) {
				ALSA9_ERROR("alsa_recover(): Unable to recover from suspend. ",err);
				return;
			}
		}
	} else ALSA9_ERROR("alsa_recover(): ",e);

}



/* Does the actual task of writing the data to the socket.
 * The ALSA write logic is a bit odd, in that the count you pass in
 * is not the number of bytes you are writing, but the number of
 * samples you are writing.  Thus, if you have stereo with 1 byte/channel,
 * you'd divide the number by 2.  If you have 16 bit audio with stereo,
 * you'd divide the number by 4.
 */
int audio_play(int buffer, int off) {
    int count = (settings.buflen - off) / sample_size;

    if (count > chunk_size) count=chunk_size;

#ifdef SOUND_DEBUG_WRITES
    printf("audio play - writing starting at %d, %d bytes, off=%d\n",
          settings.buflen*buffer+off,count, off);
    fflush(stdout);
#endif

    err = snd_pcm_writei(handle,buffers+settings.buflen*buffer+off, count);

    if (err < 0) {
	alsa_recover(err);
	return 0;
    } else {
	return err * sample_size;
    }
}







/* Plays sound 'soundnum'.  soundtype is 0 for normal sounds, 1 for
 * spell_sounds.  This might get extended in the future.  x,y are offset
 * (assumed from player) to play sound.  This information is used to
 * determine value and left vs right speaker balance.
 * This doesn't really play a sound, rather it just addes it to
 * the buffer to be played later on.
 */

void play_sound(int soundnum, int soundtype, int x, int y)
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

    /* calculate volume multiplers */
    dist=sqrt(x*x+y*y);
#ifdef SOUND_DEBUG
    fprintf(stderr,"Playing sound %i (%s), volume %i, x,y=%d,%d, dist=%f\n",soundnum,si->symbolic,si->volume,x,y, dist);
#endif
    right_ratio=left_ratio=((1<<16)*si->volume)/(100*(1+SOUND_DECREASE*dist));

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
	if (settings.bit8) {
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


int main(int argc, char *argv[])
{
    int infd;
    char inbuf[1024];
    int inbuf_pos=0, sfd, frames, maxframes;
    fd_set inset;
    struct timeval timeout;

    printf ("%s\n",rcsid_cfsndserv_c);

    fflush(stdout);

    if (read_settings()) write_settings();
    if (init_sounds()) return 1;

    infd=fileno(stdin);
    FD_ZERO(&inset);
    FD_SET(infd,&inset);

    /* need to know max amount of space */
    maxframes = snd_pcm_avail_update (handle);

    while(1){
	timeout.tv_sec=0;
	timeout.tv_usec=10000;
	FD_SET(infd,&inset);

	sfd = select(FD_SETSIZE,&inset,NULL,NULL,&timeout);

	/* ALSA9 doesn't provide us with an fd to use to make
	 * sure we have space for writing.  So instead, we use
	 * a timeout with select and if there is space, make send
	 * more data.
	 */
	frames = snd_pcm_avail_update (handle);
/*	fprintf(stderr,"frames=%d\n", frames);*/
	while (((settings.buflen+frames)> maxframes) || (frames == -EPIPE)) {

	    if (current_buffer != first_free_buffer) {
		int wrote;

		if (frames == -EPIPE)	snd_pcm_prepare(handle);
		wrote = audio_play(current_buffer, sndbuf_pos);
#ifdef SOUND_DEBUG_WRITES
		printf("play_sound(): wrote %d\n",wrote);
		fflush(stdout);
#endif
		if (wrote < settings.buflen-sndbuf_pos) {
		    sndbuf_pos+=wrote;
		}
		else{
		    /* clean the buffer */
		    memset(buffers+settings.buflen*current_buffer,zerolevel,settings.buflen);
		    sounds_in_buffer[current_buffer]=0;
		    sndbuf_pos=0;
		    current_buffer++;
		    if (current_buffer>=settings.buffers) current_buffer=0;
		}
	    } else
		break;
	    frames = snd_pcm_avail_update (handle);
	}


	if (sfd > 0) {
	    if (FD_ISSET(infd,&inset)){
		int err=read(infd,inbuf+inbuf_pos,1);
		if (err<1 && errno!=EINTR){
		    if (err<0) perror("read");
		    break;
		}
		if (inbuf[inbuf_pos]=='\n'){
		    inbuf[inbuf_pos++]=0;
		    SoundCmd((unsigned char *)inbuf,inbuf_pos);
		    inbuf_pos=0;
		}
		else {
		    inbuf_pos++;
		    if (inbuf_pos>=1024){
			fprintf(stderr,"Input buffer overflow!\n");
			inbuf_pos=0;
		    }
		}
	    }
	}
    }

    return 0;
}
