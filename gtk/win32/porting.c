/*
 * static char *rcsid_sdl_c =
 *   "$Id$";
 */

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

#include <config.h>
#include <time.h>

#ifdef WIN32
#include <sys/stat.h>
#include <stdio.h>
#include <mmsystem.h>
#include "client.h"
#include "soundsdef.h"

#ifdef HAVE_SDL
#include <math.h>
#include "sdl_mixer.h"
#pragma comment( lib, "sdl.lib" )
#pragma comment( lib, "sdl_mixer.lib" )
#endif /* HAVE_SDL */

#define MAX_SOUNDS 1024
#define SOUND_NORMAL 0
#define SOUND_SPELL 1

typedef struct Sound_Info {
	char *filename;
	char *symbolic;
	unsigned char volume;
	int size;
#ifdef HAVE_SDL
	Mix_Chunk *data;
#else
    char* data;
#endif
} Sound_Info;

Sound_Info normal_sounds[MAX_SOUNDS], spell_sounds[MAX_SOUNDS],
	default_normal, default_spell;


#define SOUND_DECREASE 0.1

/* Parse a line from the sound config file.
 */
static void parse_sound_line(char *line, int lineno)
{
	static int readtype=0, lastnum=0;
	int newnum, len;
	char *cp,*volume,*symbolic,*cp1,filename[512];

	if (line[0]=='#' || line[0]=='\n') return;

	if ( !strcmp(line,"Standard Sounds:\n")) {
		lastnum = 0;
		readtype = 1;
		return;
	}
	if ( !strcmp(line,"Spell Sounds:\n")) {
		lastnum = 0;
		readtype = 2;
		return;
	}
	if ( !readtype ) {
		fprintf(stderr,"Got input without finding section header yet:\n%d:%s\n",
			lineno, line);
		return;
	}

	if (line[strlen(line)-1]=='\n') {
		line[strlen(line)-1]='\0';
	}

	len = strcspn(line," \t");
	line[len]='\0';
	cp = line+len+1;

	/* Skip all whitespace for the next field */
	while(*cp != '\0' && (*cp==' ' || *cp=='\t')) {
		cp++;
	}

	volume=cp;

	/* No symbolic name or number - that is ok */
	cp1=cp;
	if (!(cp=strchr(cp1,' ')) && !(cp=strchr(cp1,'\t'))) {
		newnum=lastnum+1;
		symbolic=NULL;
	} else {
		/* We think we have a symbolic name */
		/* Don't need to nulterm the volume, since we atoi it anyways */
		while(*cp != '\0' && (*cp==' ' || *cp=='\t')) {
			cp++;
		}
		symbolic = cp;
		/* Some symbolic names are double quote protected.  If, do some
		 * special processing.  We strip off the quotes.
		 */
		if ( *symbolic=='"') {
			symbolic++;
			for (cp=symbolic; *cp != '\0' && *cp !='"'; cp++ );
			*cp = '\0';
			cp++;
		}
		/* Let's try to find the sound number now */
		cp1 = cp;
		if ( !(cp=strchr(cp1,' ')) && !(cp=strchr(cp1,'\t'))) {
			newnum=lastnum+1;
		} else {
			*cp++='\0';
			while(*cp!='\0' && (*cp==' ' || *cp=='\t')) {
				cp++;
			}
			if ( isdigit(*cp)) {
				newnum=atoi(cp);
			} else {
				newnum=lastnum+1;
			}
		}
	}
	if ( newnum < 0 || newnum >MAX_SOUNDS ) {
		fprintf(stderr,"Invalid sound number %d, line %d, buf %s\n",
			newnum, lineno, line );
		return;
	}

	strcpy(filename, line);

	if ( symbolic && !strcmp(symbolic,"DEFAULT")) {
		if (readtype==1) {
			default_normal.filename=strdup(filename);
			default_normal.volume = atoi(volume);
		} else if (readtype==2) {
			default_spell.filename = strdup(filename);
			default_spell.volume = atoi(volume);
		}
		return;
	} else {
		if ( readtype==1 ) {
			normal_sounds[newnum].filename = strdup(filename);
			normal_sounds[newnum].volume = atoi(volume);
			if ( symbolic ) {
				normal_sounds[newnum].symbolic = strdup(symbolic);
			} else {
				normal_sounds[newnum].symbolic = NULL;
			}
		} else if (readtype==2) {
			spell_sounds[newnum].filename = strdup(filename);
			spell_sounds[newnum].volume = atoi(volume);
			if ( symbolic ) {
				spell_sounds[newnum].symbolic = strdup(symbolic);
			} else {
				spell_sounds[newnum].symbolic = NULL;
			}
		}
		lastnum = newnum;
	}
}

void load_sounds_file( )
    {
    int i;
	FILE *fp;
	char path[256], buf[512];

    for ( i=0;i<MAX_SOUNDS; i++ ) {
		normal_sounds[i].filename = NULL;
		spell_sounds[i].filename = NULL;
		normal_sounds[i].size = -1;
		spell_sounds[i].size = -1;
	}
	default_normal.filename = NULL;
	default_spell.filename = NULL;

	sprintf(path,"%s/sounds", getenv("HOME"));
	i=0;
	if ( !(fp=fopen(path,"r"))) {
		fprintf(stderr,"Unable to open %s - will use built in defaults\n", path);
		for (; i<sizeof(def_sounds)/sizeof(char*); i++ ) {
			strcpy(buf,def_sounds[i]);
			parse_sound_line(buf,i);
		}
	} else {
		while(fgets(buf,511,fp)!=NULL) {
			buf[511] = '\0';
			parse_sound_line(buf,++i);
		}
        fclose( fp );
	}
	/* Note in both cases below, we leave the symbolic name untouched */
	for ( i=0; i<MAX_SOUNDS; i++ ) {
		if ( !normal_sounds[i].filename ) {
			normal_sounds[i].filename = strdup( default_normal.filename );
			normal_sounds[i].volume = default_normal.volume;
		}
		if ( !spell_sounds[i].filename ) {
			spell_sounds[i].filename= strdup( default_spell.filename );
			spell_sounds[i].volume = default_spell.volume;
		}
		normal_sounds[i].data = NULL;
		spell_sounds[i].volume = 0;
	}
    }

#ifdef HAVE_SDL  /* HAVE_SDL */

#include <SDL.h>
#include <SDL_Mixer.h>

/* mixer variables */
char *buffers = NULL;
int *sounds_in_buffer=NULL;
int current_buffer=0; /* Next buffer we will write out */
int first_free_buffer=0; /* So we know when to stop playing sounds */

/* sound device parameters */
int stereo=0,bit8=0,sample_size=0,frequency=0,sign=0,zerolevel=0;

struct sound_settings{
	int stereo,bit8,sign,frequency,buffers,buflen,simultaneously;
} settings={0,1,0,11025,100,1024,4};


/* Background music */
Mix_Music *music = NULL;


/* Initialize SDL sound.
 */
int init_sounds()
{
#ifdef DEBUG
	fprintf( stderr,"Settings: bits: %i, ",settings.bit8?8:16);
	fprintf( stderr,"%s, ",settings.sign?"signed":"unsigned");
	fprintf( stderr,"%s, ",settings.stereo?"stereo":"mono");
	fprintf( stderr,"frequency: %i\n ",settings.frequency);
#endif

	/* If SDL audio wasn't init'ed, init it. */
	if( SDL_WasInit(SDL_INIT_AUDIO) == 0) {
		int audio_rate = 22050;
		uint16 audio_format = AUDIO_S16;
		int audio_channels = 2;
		int audio_buffers = 4096;

		if( SDL_InitSubSystem(SDL_INIT_AUDIO) < 0)
		{
			fprintf( stderr, "Could not initialize SDL audio: %s\n", SDL_GetError());
			return -1;
		}

		/* init SDL_mixer */
		if (Mix_OpenAudio(audio_rate, audio_format, audio_channels, audio_buffers)) {
			fprintf(stderr,"SDL_MIXER: Unable to open audio: %s\n", SDL_GetError());
			return -1;
		} else {
			int numtimesopened;
			char format_str[8];
			numtimesopened = Mix_QuerySpec(&audio_rate, &audio_format, &audio_channels);

			sprintf(format_str,"unknown");
			switch(audio_format) {
			case AUDIO_U8: sprintf(format_str,"U8"); break;
			case AUDIO_S8: sprintf(format_str,"S8"); break;
			case AUDIO_U16LSB: sprintf(format_str,"U16LSB"); break;
			case AUDIO_S16LSB: sprintf(format_str,"S16LSB"); break;
			case AUDIO_U16MSB: sprintf(format_str,"U16MSB"); break;
			case AUDIO_S16MSB: sprintf(format_str,"S16MSB"); break;
			}
			fprintf(stderr,"SDL_MIXER: Using SDL Mixer for audio [ %d kHz | %d channels | Audio Format %s ].\n",
				audio_rate, audio_channels, format_str );
			/* 
			 * start playing the background music
			 * Possibly have different background music for each map?
			 */
			Mix_HaltChannel(-1);
			Mix_HaltMusic();

			if ( music == NULL ) {
				music = Mix_LoadMUS("sfx/backg1.wav");
				if ( music ) {
					Mix_PlayMusic(music,-1);
				}
			} else {
				Mix_PlayMusic(music,-1);
			}
		}
	}


	buffers = (char *)malloc(settings.buffers * settings.buflen );
	if ( !buffers ) {
		return -1;
	}
	sounds_in_buffer = (int*)calloc(settings.buffers,sizeof(int));
	if ( !sounds_in_buffer ) {
		return -1;
	}

	if ( sign ) {
		zerolevel = 0;
	} else {
		zerolevel = bit8?0x80:0x8000;
	}

	memset(buffers,zerolevel,settings.buflen * settings.buffers);


#ifdef DEBUG
	fprintf( stderr,"bits: %i, ",bit8?8:16);
	fprintf( stderr,"%s, ",sign?"signed":"unsigned");
	fprintf( stderr,"%s, ",stereo?"stereo":"mono");
	fprintf( stderr,"freq: %i, ",frequency);
	fprintf( stderr,"smpl_size: %i, ",sample_size);
	fprintf( stderr,"0level: %i\n",zerolevel);
#endif

    load_sounds_file( );
	PlaySound(NULL,NULL,SND_ASYNC);

	return 0;
}

/* Plays sound 'soundnum', soundtype is 0 for normal sounds, 1 for
 * spell sounds.  This might get extended in the futur.  x,y are offset
 * (assumed from player) to play sound.  This information is used to
 * determine value and left vs right speaker balance.
 * This doesn't really play a sound, rather it just adds it to
 * the buffer to be played later on.
 */

static void play_sound(int soundnum, int soundtype, sint8 x, sint8 y )
{
	Sound_Info *si;
	double dist;
	int index;

	if (!use_config[CONFIG_SOUND]) return;

	if ( soundnum>=MAX_SOUNDS || soundnum<0 ) {
		fprintf(stderr,"Invalid sound number: %d\n", soundnum );
		return;
	}
	if (soundtype == SOUND_NORMAL ) {
		si = &normal_sounds[soundnum];
	} else if ( soundtype == SOUND_SPELL ) {
		si = &spell_sounds[soundnum];
	} else {
		fprintf(stderr,"Unknown soundtype: %d\n", soundtype);
		return;
	}

	if (!si->data) {
		si->data = Mix_LoadWAV(si->filename);
		if ( !si->data ) {
			fprintf(stderr, "SDL_MIXER: Couldn't load %s: %s\n", si->filename, SDL_GetError());
		}
	} 
	if (si->data) {
		int playchannel;
        int angle;

		playchannel = 0;
		si->data->volume = si->volume;
		for ( index=0; index<MIX_CHANNELS; index++ ) {
			if ( !Mix_Playing(index) ) {
				playchannel = index;
				break;
			}
		}
		dist = sqrt(x*x+y*y);
        if ( x == 0 )
            {
            angle = ( y < 0 ) ? 0 : 180;
            }
        else
            {
            angle = ( asin( ( double )x / ( double )dist ) ) * 180. / 3.14159;
            if ( y < 0 )
                angle = - angle;
            }
		
		if ( Mix_Playing(playchannel) ) {
			Mix_HaltChannel(playchannel);
		}

		/*Mix_SetDistance(playchannel,dist);*/
        Mix_SetPosition( playchannel, angle, dist );

		Mix_PlayChannel(playchannel,si->data,0);
		
		return;
	}

	/*
	 * Load the sound if it is not loaded yet
	 *
	 */
/*	if ( !si->data ) {
		FILE *f;
		struct stat sbuf;
		fprintf(stderr,"Loading file: %s\n",si->filename );
		if (stat(si->filename,&sbuf)) {
			perror(si->filename);
			return;
		}
		si->size=sbuf.st_size;
		if ( si->size<=0) {
			return;
		}
		if ( si->size * sample_size > settings.buflen*(settings.buffers-1)) {
			fprintf(stderr,"Sound %s too long (%i > %i)\n",si->filename,si->size,
				settings.buflen*(settings.buffers-1)/sample_size);
			return;
		}
		si->data=(unsigned char*)malloc(si->size);
		f=fopen(si->filename,"r");
		if ( !f ) {
			perror(si->filename);
			return;
		}
		fread(si->data,1,si->size,f);
		fclose(f);
	}
#ifdef DEBUG
	fprintf(stderr,"Playing sound %i (%s), volume %i, x,y=%d,%d\n",soundnum,si->symbolic,si->volume,x,y);
#endif

	PlaySound(NULL,NULL,SND_ASYNC);

	PlaySound(si->data,NULL,SND_ASYNC | SND_MEMORY );

	return;
    */
}	

void SoundCmd(unsigned char *data, int len)
{
	int num, type;
    sint8 x, y;

    if (len!=5) {
	fprintf(stderr,"Got invalid length on sound command: %d\n", len);
	return;
    }
    x = data[0];
    y = data[1];
    num = GetShort_String(data+2);
    type = data[4];

#ifdef DEBUG
    fprintf(stderr,"Playing sound %d (type %d), offset %d, %x\n",
	    num, type, x ,y);
#endif

    play_sound(num, type, x, y);
}

#else  /* HAVE_SDL */
/* No SDL, let's use dumb PlaySound (better than nothing).
 */

int init_sounds() 
{
    LOG(LOG_INFO,"init_sounds","using regular Windows PlaySound");
	PlaySound(NULL,NULL,SND_ASYNC);
    load_sounds_file( );
	return 0;
}

void SoundCmd(unsigned char *data, int len) 
{
	int num, type;
    Sound_Info* si;

    if (len!=5) {
	LOG(LOG_WARNING,"SoundCmd(win)","Got invalid length on sound command: %d\n", len);
	return;
    }
    num = GetShort_String(data+2);
    type = data[4];

	if (type == SOUND_NORMAL ) {
		si = &normal_sounds[ num ];
	} else if ( type == SOUND_SPELL ) {
		si = &spell_sounds[ num ];
    } else {
        LOG(LOG_WARNING,"SoundCmd(win)","invalid sound type %d",type);
        return;
    }

    if ( !si->filename )
        /* Already tried to load sound, failed, let's stop here. */
        return;

    if ( !si->data )
        {
        /* Let's try to load the sound */
        FILE* fsound;
        struct stat sbuf;

		if ( ( stat( si->filename, &sbuf ) == -1 ) || ( ( fsound = fopen( si->filename, "rb" ) ) == NULL ) )
            {
            // Failed to load it, clear name & such so we don't try again.
            LOG( LOG_WARNING, "SoundCmd(win)", "Can't open sound %s", si->filename );
			perror( si->filename );
            free( si->filename );
            si->filename = NULL;
			return;
		    }

		si->size=sbuf.st_size;
        si->data = malloc( si->size );
        fread( si->data, si->size, 1, fsound );
        fclose( fsound );
        }

    PlaySound( si->data, NULL, SND_ASYNC | SND_MEMORY | SND_NOWAIT);
}

#endif /* HAVE_SDL */


/* The only gettimeofday calls appears to be ones added for purposes of 
 * timing the map redraws, so for practical purposes, it should just 
 * always return 0 with no real harm.
 */
void gettimeofday(struct timeval *tv, void* unused)
{
	memset(tv, 0, sizeof(struct timeval));
}


/* This is functionally equivalent to Sleep(x) under win32.
void Sleep(long SleepMilliseconds)
{
    static struct timeval t;
    t.tv_sec = SleepMilliseconds/1000;
    t.tv_usec = (SleepMilliseconds %1000000) *1000;

    select(0, NULL, NULL, NULL, &t);
}
*/

int strncasecmp(const char *s1, const char *s2, int n)
{
  register char c1, c2;

  while (*s1 && *s2 && n) {
    c1 = tolower(*s1);
    c2 = tolower(*s2);
    if (c1 != c2)
      return (c1 - c2);
    s1++;
    s2++;
    n--;
  }
  if (!n)
    return(0);
  return (int) (*s1 - *s2);
}

int strcasecmp(const char *s1, const char*s2)
{
  register char c1, c2;

  while (*s1 && *s2) {
    c1 = tolower(*s1);
    c2 = tolower(*s2);
    if (c1 != c2)
      return (c1 - c2);
    s1++;
    s2++;
  }
  if (*s1=='\0' && *s2=='\0')
	return 0;
  return (int) (*s1 - *s2);
}
#endif /* WIN32 */
