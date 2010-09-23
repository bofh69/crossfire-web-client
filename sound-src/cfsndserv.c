static char *rcsid_sound_src_cfsndserv_c =
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

/**
 * @file sound-src/cfsndserv.c
 * The server for sound support for the client.  In addition to OSS and ALSA,
 * sun sound is also supported.  Comment from the original author is below.
 *
 * (c) 1998 Jacek Konieczny <jajcus@zeus.polsl.gliwice.pl>
 *
 * This file contains the server for sound support for the client.  It
 * supports both ALSA_SOUND and OSS_SOUND. Any other sound system support can
 * be easily added - only new init_audio and audio_play need be written.
 *
 * If you have any problems please e-mail me.
 */

/*#define SDL_SOUND*/
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

#include "shared/newclient.h"
#include "soundsdef.h"

#if defined(SDL_SOUND)
#  include "SDL.h"
#  include "SDL_mixer.h"
#  define AUDIODEV "/foo/bar"
#elif defined(ALSA_SOUND)
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

/**
 * A replacement of strdup(), since it's not defined at some unix variants.
 *
 * @param str
 * @return
 */
char *strdup_local(char *str) {
  char *c=(char *) malloc(sizeof(char) * strlen(str) + 1);
  strcpy(c, str);
  return c;
}

typedef struct Sound_Info {
    char          *filename;
    char          *symbolic;
    unsigned char  volume;
    int            size;
    unsigned char *data;
} Sound_Info;

Sound_Info normal_sounds[MAX_SOUNDS], spell_sounds[MAX_SOUNDS],
default_normal, default_spell;

#define SOUND_DECREASE 0.1

/* Mixer variables */
char *buffers = NULL;
int  *sounds_in_buffer = NULL;
int   current_buffer = 0;               /**< The next buffer to write out   */
int   first_free_buffer = 0;            /**< Help know when to stop playing */

int   soundfd=0;

/* Sound device parameters */
int stereo=0, bit8=0, sample_size=0, frequency=0, sign=0, zerolevel=0;

#ifdef SUN_SOUND

struct sound_settings {
    int stereo;
    int bit8;
    int sign;
    int frequency;
    int buffers;
    int buflen;
    int simultaneously;
    const char *audiodev;
} settings = { 0, 1, 1, 11025, 100, 4096, 4, AUDIODEV };

#elif defined(SDL_SOUND)

int             audio_channels = 0;     /**< Channels in use by SDL_mixer   */
Uint16          audio_format = 0;       /**< Format of the SDL_mixer audio  */
Mix_Music      *music = NULL;           /**< A music file to play           */
Mix_Chunk     **chunk = NULL;           /**< Loaded sounds to play          */
int             max_chunk = 0;          /**< Max count of concurrent sounds */

struct sound_settings {
    int         stereo;
    int         bit8;
    int         sign;
    int         frequency;
    int         buffers;
    int         buflen;
    int         simultaneously;         /**< Max number of sounds to queue. */
    const char *audiodev;
} settings = { 0, 1, 0, 11025, 100, 4096, 4, AUDIODEV };

#else

struct sound_settings {
    int stereo;
    int bit8;
    int sign;
    int frequency;
    int buffers;
    int buflen;
    int simultaneously;
    const char *audiodev;
} settings = { 0, 1, 0, 11025, 100, 1024, 4, AUDIODEV };

#endif

/**
 * Parse a line from the sound file.  This is a little ugly because static
 * values are stored in the function so we know what we are doing - however,
 * it is somewhat necessary so that we can use this same function to parse
 * both files and the compiled in data.  The linefeed delimited sound file
 * lines that are empty, or begin with hash (#) characters are ignored. Data
 * lines are space/tab-delimited fields:
 *
 * sound_file<TAB>default_volume<TAB>SOUND_SYMBOLIC_NAME<TAB>sound_number
 *
 * sound_file is an absolute path (not at all friendly or resilient to client
 *     changes like changing from a distro version to SVN, etc).
 *
 * default_volume is an integer from 0 to 100 (a percentage) that is left
 *     padded with spaces to produce a fixed, three-character width.
 *
 * SOUND_SYMBOLIC_NAME is unused and may be omitted, but could be used for
 *     client/server communication regarding a sound to play.
 *
 * sound_number is a zero-based integer that identifies the sound.  If it is
 *     omitted, the last sound number used is incremented by one.  The server
 *     communicates what sound to play via the sound number, so the order is
 *     not arbitrary.
 *
 * @param line   A line of data from the .crossfire/sounds configuration file.
 *               Note that this data may be modified by parse_sound_line().
 * @param lineno The line number of the passed data used for error tracking.
 */
static void parse_sound_line(char *line, int lineno) {
    static int readtype=0;              /**< Identifies the last section title
                                         *   found in the .crossfire/sounds
                                         *   file.  0 indicates a section
                                         *   title was not found yet.
                                         */
    static int lastnum=0;               /**< The number of items processed in
                                         *   the current readtype section.
                                         */
    int        newnum, len;
    char      *cp, *volume, *symbolic, *cp1, filename[512];

    if (line[0] == '#' || line[0] == '\n')
        return;

    if (!strcmp(line, "Standard Sounds:\n")) {
        lastnum = 0;
        readtype = 1;
        return;
    }

    if (!strcmp(line, "Spell Sounds:\n")) {
        lastnum = 0;
        readtype = 2;
        return;
    }
    if (!readtype) {
#ifdef SOUND_DEBUG
        fprintf(stderr,
            "parse_sound_line: Ignored file header:\n%d:%s\n", lineno, line);
#endif
        return;
    }
    /*
     * Change the LF delimiter at the end of the line to a null terminator.
     */
    if (line[strlen(line)-1] == '\n')
        line[strlen(line)-1] = '\0';
    /*
     * Convert the first whitespace found to a null terminator.
     */
    len = strcspn(line, " \t");
    line[len] = '\0';
    cp = line + len + 1;
    /*
     * Skip all the following whitespace to locate the next field, and save a
     * pointer to the volume data.
     */
    while (*cp != '\0' && (*cp == ' ' || *cp == '\t'))
        cp++;
    volume = cp;
    /*
     * There is no need to null terminate the volume since it is processed
     * with atoi.
     *
     * Next, check to see if the unprocessed portion of the line has any
     * whitespace following the default volume.
     */
    cp1 = cp;
    if (!(cp = strchr(cp1, ' ')) && !(cp = strchr(cp1, '\t'))) {
        /*
         * If not, there cannot be a sound number, and any data left is an
         * unused symbolic name, so the sound number is auto-assigned.
         */
        newnum = lastnum + 1;
        symbolic = NULL;
    } else {
        /*
         * Since there is more whitespace, there might be a symbolic name and
         * sound number.  Ignore any additional whitespace following the
         * volume, and treat the next character as the beginning of a symbolic
         * name.
         */
        while (*cp != '\0' && (*cp == ' ' || *cp == '\t'))
            cp++;
        symbolic = cp;
        /*
         * Some symbolic names are double-quoted to allow them to contain
         * whitespace.  If a quote starts the name, advance the name pointer
         * to effectively strip the quote, and convert the final quote to a
         * null terminator.
         */
        if (*symbolic == '"') {
            symbolic++;
            for (cp = symbolic; *cp != '\0' && *cp != '"'; cp++);
            *cp = '\0';
            cp++;
        }
        /*
         * cp is either the beginning of an unquoted symbolic name or is
         * pointing to the whitespace between a quoted symbolic name and the
         * sound number.
         */
        cp1 = cp;
        if (!(cp = strchr(cp1, ' ')) && !(cp = strchr(cp1, '\t'))) {
            /*
             * There is no more whitespace on the line.  If the name was
             * quoted, there should have been whitespace following that cp was
             * pointing to, so there cannot be a sound number present.  On the
             * other hand, if the name was not quoted, cp would point at the
             * symbolic name and whitespace should follow if a sound number is
             * present.  Either way, the sound number must be auto-assigned.
             */
            newnum = lastnum + 1;
        } else {
            /*
             * If there was whitespace left, cp points to it now, whether or
             * not the symbolic name was quoted.  A sound number should follow
             * the whitespace.  First, try to null terminate the prior data,
             * then skip all subsequent whitespace, and point at what should
             * be the sound number.  If numeric data is found, read the sound
             * number, otherwise auto-assign the sound number.  This is a bit
             * dodgy as invalid data is silently ignored.
             */
            *cp++ = '\0';
            while (*cp != '\0' && (*cp == ' ' || *cp == '\t'))
                cp++;
            if (isdigit(*cp))
                newnum = atoi(cp);
            else
                newnum = lastnum + 1;
        }
    }
    if (newnum < 0 || newnum > MAX_SOUNDS) {
        fprintf(stderr,
            "Invalid sound number %d, line %d, buf %s\n",
                newnum, lineno, line);
        return;
    }
    /*
     * Compatibility processing for older files and/or the SDL_mixer setup.
     * If the filename ends in .au, convert the ending to a more appropriate
     * one as .au files are not distributed by the project.
     *
     * Use .raw instead of .au for most sound setups, as this is what has been
     * supported by the clients for a long time.
     *
     * As SDL_mixer does not support .raw, change the extension to .ogg for
     * systems other than Windows, or .wav for Windows.  Technically, it would
     * be okay to use either .wav or .ogg whatever the platform, so it is a
     * FIXME in that it would probably be best for the file extension to be a
     * configurable option.
     *
     * Overriding the content of the sound file is a bit of a kludge, but
     * allows legacy .crossfire/sound files to work with the current client.
     * The dodgy part is that if someone looks in the file, it will not
     * necessarily indicate the actual file being played.
     */
    strcpy(filename, line);
    cp = filename + strlen(filename) - 3;
    if (!strcmp(cp, ".au"))
        strcpy(cp, ".raw");
#ifdef SDL_SOUND
    cp = filename + strlen(filename) - 4;
    if (!strcmp(cp, ".raw"))
#ifndef WIN32
        strcpy(cp, ".ogg");
#else
        strcpy(cp, ".wav");
#endif
#endif
    /*
     * One symbolic name is used: DEFAULT.  If it is found, the sound file
     * becomes the default sound for any undefined sound number, so set the
     * appropriate default, and ignore any sound number that may follow.
     */
    if (symbolic && !strcmp(symbolic, "DEFAULT")) {
        if (readtype == 1) {
            /*
             * Standard Sounds
             */
            default_normal.filename = strdup_local(filename);
            default_normal.volume = atoi(volume);
        } else if (readtype == 2) {
            /*
             * Spell Sounds
             */
            default_spell.filename = strdup_local(filename);
            default_spell.volume = atoi(volume);
        }
        return;
    }
    /*
     * The only way for processing to reach this point is if valid sound data
     * was found in a section.  Process it according to the section it is in.
     */
    if (readtype == 1) {
        /*
         * Standard Sounds
         */
        normal_sounds[newnum].filename = strdup_local(filename);
        normal_sounds[newnum].volume = atoi(volume);
        if (symbolic)
            normal_sounds[newnum].symbolic = strdup_local(symbolic);
        else
            normal_sounds[newnum].symbolic = NULL;
    } else if (readtype == 2) {
        /*
         * Spell Sounds
         */
        spell_sounds[newnum].filename = strdup_local(filename);
        spell_sounds[newnum].volume = atoi(volume);
        if (symbolic)
            spell_sounds[newnum].symbolic = strdup_local(symbolic);
        else
            spell_sounds[newnum].symbolic = NULL;
    }
    /*
     * Retain the assigned sound number for possible use in subsquent data
     * lines.
     */
    lastnum = newnum;
}

#if defined(SDL_SOUND)

/**
 * Initialize the SDL_mixer sound system.
 *
 * @return Zero if audio initialized successfully, otherwise -1.
 */
int init_audio(void) {

#ifdef SOUND_DEBUG
    fprintf(stderr, "SDL_mixer init_audio()\n");
#endif

    if (SDL_Init(SDL_INIT_AUDIO) == -1) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        exit(1);
    }

    frequency = settings.frequency;
    bit8 = settings.bit8;
    if (settings.bit8)
        audio_format = settings.sign ? AUDIO_S8 : AUDIO_U8;
    else
        audio_format = settings.sign ? AUDIO_S16 : AUDIO_U16;
    audio_channels = (stereo = settings.stereo) ? 1 : 2;
    /*
     * This is where we open up our audio device.  Mix_OpenAudio takes as its
     * parameters the audio format we'd /like/ to have.
     */
    if (Mix_OpenAudio(frequency,audio_format,audio_channels,settings.buflen)){
        fprintf(stderr, "Mix_OpenAudio: Unable to open audio!\n");
        exit(1);
    }
    /*
     * Find out what configuration we got.
     */
    Mix_QuerySpec(&frequency, &audio_format, &audio_channels);

    switch (audio_format) {
       case AUDIO_U16LSB:
       case AUDIO_U16MSB:
           bit8 = 0;
           sign = 0;
           break;
       case AUDIO_S16LSB:
       case AUDIO_S16MSB:
           bit8 = 0;
           sign = 1;
           break;
       case AUDIO_U8:
           bit8 = 1;
           sign = 0;
           break;
       case AUDIO_S8:
           bit8 = 1;
           sign = 1;
           break;
       default:
           fprintf(stderr, "init_audio: Unexpected audio format\n");
           return -1;
    }

    switch (audio_channels) {
        case 1:
            stereo = 0;
            break;
        case 2:
            stereo = 1;
            break;
        default:
            fprintf(stderr, "init_audio: Unexpected number of channels\n");
            return -1;
    }

    return 0;
}

/**
 * Play a sound using the SDL_mixer sound system.
 *
 * @param buffer
 * @param off
 * @return
 */
int audio_play(int buffer, int off) {

#ifdef SOUND_DEBUG
    fprintf(stderr, "audio_play: SDL_mixer audio_play()\n");
#endif

    return settings.buflen - off;
}

#elif defined(ALSA_SOUND)

/**
 * Initialize the Alsa sound system.
 *
 * @return Zero if audio initialized successfully, otherwise -1.
 */
int init_audio(void) {
    int card=0, device=0, err;
    snd_pcm_channel_params_t params;

    printf("cfsndserv compiled for ALSA sound system\n");
    fflush(stdout);

    if ((err = snd_pcm_open(&handle, card, device, SND_PCM_OPEN_PLAYBACK))<0) {
        fprintf(stderr, "open failed: %s\n", snd_strerror(err));
        return -1;
    }

    params.channel = SND_PCM_CHANNEL_PLAYBACK;
    params.mode = SND_PCM_MODE_BLOCK;

    if (settings.bit8)
        params.format.format =
            settings.sign ? SND_PCM_SFMT_S8 : SND_PCM_SFMT_U8;
    else
        params.format.format =
            settings.sign ? SND_PCM_SFMT_S16_LE : SND_PCM_SFMT_U16_LE;

     params.format.rate = settings.frequency;
     params.format.voices = settings.stereo ? 2 : 1;
     params.buf.block.frag_size = settings.buflen / 2;
     params.buf.block.frags_max = 2;
     params.buf.block.frags_min = 1;

     if ((err = snd_pcm_channel_params(handle, &params)) < 0) {
        fprintf(stderr,
            "format setup failed: %s\nTrying defaults\n", snd_strerror(err));
        params.format.format = SND_PCM_SFMT_U8;
        params.format.rate = 11025;
        params.format.voices = 1;
        if ((err = snd_pcm_channel_params(handle, &params )) < 0) {
            fprintf(stderr, "format setup failed: %s\n", snd_strerror(err));
            snd_pcm_close(handle);
            return -1;
        }
     }
     switch (params.format.format) {
        case SND_PCM_SFMT_S8:
           bit8 = 1;
           sign = 1;
           break;
        case SND_PCM_SFMT_U8:
           bit8 = 1;
           sign = 0;
           break;
        case SND_PCM_SFMT_S16_LE:
           bit8 = 0;
           sign = 1;
           break;
        case SND_PCM_SFMT_U16_LE:
           bit8 = 0;
           sign = 0;
           break;
        default:
           fprintf(stderr, "Could not set proper format\n");
           return -1;
     }

     sample_size = params.format.voices * (bit8 ? 1 : 2);
     stereo = (params.format.voices == 1) ? 0 : 1;
     frequency = params.format.rate;

     soundfd = snd_pcm_file_descriptor(handle, SND_PCM_CHANNEL_PLAYBACK);
     snd_pcm_nonblock_mode(handle, 1);

     return 0;
}

/**
 * Play a sound using the Alsa sound system.
 *
 * @param buffer
 * @param off
 * @return
 */
int audio_play(int buffer, int off) {

    return
        snd_pcm_write
            (handle, buffers+settings.buflen*buffer+off, settings.buflen-off);
}

#elif defined(OSS_SOUND)

/**
 * Initialize the OSS sound system.
 *
 * @return Zero if audio initialized successfully, otherwise -1.
 */
int init_audio(void){

    const char *audiodev;
    int value,format,tmp;

    printf("cfsndserv compiled for OSS sound system\n");
    fflush(stdout);

    /* Open the audio device */
    if ((audiodev = getenv("AUDIODEV")) == NULL) {
        audiodev = settings.audiodev;
    }
    soundfd = open(audiodev, (O_WRONLY|O_NONBLOCK), 0);
    if (soundfd < 0) {
        fprintf(stderr, "Could not open %s: %s\n", audiodev, strerror(errno));
        return(-1);
    }

    /* Set the audio buffering parameters */
    value = 0;
    for (tmp = settings.buflen / 2; tmp; tmp >>= 1)
        value++;

    value |= 0x00020000;
    if (ioctl(soundfd, SNDCTL_DSP_SETFRAGMENT, &value) < 0) {
        fprintf(stderr, "Could not set audio fragment spec\n");
        return(-1);
    }
    if (settings.bit8)
        format = settings.sign ? AFMT_S8 : AFMT_U8;
    else
        format = settings.sign ? AFMT_S16_LE : AFMT_U16_LE;

    value = format;
    if ((ioctl(soundfd, SNDCTL_DSP_SETFMT,&value) < 0)
    ||  (value != format) ) {
        fprintf(stderr, "Could not set audio format\n");
    }

    switch (value) {
       case AFMT_S16_LE:
           bit8 = 0;
           sign = 1;
           break;
       case AFMT_U16_LE:
           bit8 = 0;
           sign = 0;
           break;
       case AFMT_S8:
           bit8 = 1;
           sign = 1;
           break;
       case AFMT_U8:
           bit8 = 1;
           sign = 0;
           break;
       default:
           return -1;
    }

    stereo = settings.stereo;
    ioctl(soundfd, SNDCTL_DSP_STEREO, &stereo);

    frequency = settings.frequency;
    if (ioctl(soundfd, SNDCTL_DSP_SPEED, &frequency) < 0) {
        fprintf(stderr, "Could not set audio frequency\n");
        return(-1);
    }
    sample_size = (bit8 ? 1 : 2) * (stereo ? 2 : 1);
    return 0;
}

/**
 * Play a sound using the OSS sound system.
 *
 * @param buffer
 * @param off
 * @return
 */
int audio_play(int buffer, int off){
    int wrote;
#ifdef SOUND_DEBUG
    printf("audio play - writing starting at %d, %d bytes",
        settings.buflen * buffer + off, settings.buflen - off);
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

ALconfig soundconfig;
ALport   soundport;

/**
 * Initialize the SGI sound system.
 *
 * @return Zero if audio initialized successfully, otherwise -1.
 */
int init_audio(void)
{
    long params[2];

    printf("cfsndserv compiled for SGI sound system\n");
    fflush(stdout);

    /* Allocate ALconfig structure. */

    if ((soundconfig = ALnewconfig()) == 0)
    {
        fprintf(stderr, "Could not allocate ALconfig structure.\n");
        return -1;
    }

    /* Set number of channels */

    if (ALsetchannels(soundconfig, (stereo = settings.stereo) ? 2 : 1) == -1)
    {
        fprintf(stderr, "Could not set number of channels.\n");
        return -1;
    }

    /* Set sample format */

    if (ALsetsampfmt(soundconfig, AL_SAMPFMT_TWOSCOMP) == -1)
    {
        fprintf(stderr, "Could not set audio sample format.\n");
        return -1;
    }
    sign = 1;

    /* Set sample width */
    if (ALsetwidth(
        soundconfig, (bit8 = settings.bit8)?AL_SAMPLE_8:AL_SAMPLE_16) == -1)
    {
        fprintf(stderr,"Could not set audio sample width.\n");
        return -1;
    }
    sample_size = (stereo ? 2 : 1) * (bit8 ? 1 : 2);

    /* Set frequency */

    params[0] = AL_OUTPUT_RATE;
    params[1] = frequency = settings.frequency;
    if (ALsetparams(AL_DEFAULT_DEVICE, params, 2) == -1)
    {
        fprintf(stderr, "Could not set output rate of default device.\n");
        return -1;
    }

    /* Open audio port */

    if ((soundport = ALopenport("cfsndserv port", "w", soundconfig)) == NULL)
    {
        fprintf(stderr, "Could not open audio port.\n");
        return -1;
    }
    soundfd = ALgetfd(soundport);
    return 0;
}

/**
 * Play a sound using the SGI sound system.
 *
 * @param buffer
 * @param off
 * @return
 */
int audio_play(int buffer,int off)
{
    ALwritesamps(
        soundport, buffers + settings.buflen * buffer+off,
            (settings.buflen - off) / sample_size);
    return settings.buflen-off;
}

#elif defined(SUN_SOUND)

/**
 * Initialize the Sun sound system.
 *
 * @return Zero if audio initialized successfully, otherwise -1.
 */
int init_audio(void) {

    const char *audiodev;
    int value, format, tmp;
    audio_info_t audio_info;
    audio_device_t audio_device;

    printf("cfsndserv compiled for SUN sound system\n");
    fflush(stdout);

    /* Open the audio device */
    if ((audiodev = getenv("AUDIODEV")) == NULL) {
        audiodev = settings.audiodev;
    }
    soundfd = open(audiodev, (O_WRONLY|O_NONBLOCK), 0);
    if (soundfd < 0) {
        fprintf(stderr, "Could not open %s: %s\n", audiodev, strerror(errno));
        return(-1);
    }

    if (ioctl(soundfd, AUDIO_GETDEV, &audio_device) < 0) {
        fprintf(stderr, "Could not get audio device ioctl\n");
        return(-1);
    }

    if (ioctl(soundfd, AUDIO_GETINFO, &audio_info) < 0) {
      fprintf(stderr, "Could not get audio information ioctl\n");
      return(-1);
    }
    /*
     * The capabilities on different sun hardware vary wildly.  We attempt to
     * get a working setup no matter what hardware we are running on.
     *
     * This is sparc 10, sparc 20 class systems
     */
     if (!strcmp(audio_device.name, "SUNW,dbri")) {
         /*
          * To use linear encoding, we must use 16 bit and some fixed
          * frequencies.  11025 matches what the rest of the systems use
          */
          audio_info.play.precision = 16;
          audio_info.play.encoding = AUDIO_ENCODING_LINEAR;
          audio_info.play.sample_rate = 11025;
     }
      /* This is used on many of the ultra machines */
     else if (!strcmp(audio_device.name, "SUNW, CS4231")) {
          /*
           * To use linear encoding, we must use 16 bit and some fixed
           * frequencies.  11025 matches what the rest of the systems use
           */
          audio_info.play.precision = 16;
          audio_info.play.encoding = AUDIO_ENCODING_LINEAR;
          audio_info.play.sample_rate = 11025;
     }

    audio_info.play.channels = settings.stereo ? 2 : 1;
    stereo = settings.stereo;

    bit8 = (audio_info.play.precision == 8) ? 1 : 0;
    frequency = settings.frequency;
    sample_size = (bit8 ? 1 : 2) * (stereo ? 2 : 1);
    fprintf(stderr,
        "SUN_SOUND: bit8=%d, stereo=%d, freq=%d, sample_size=%d\n",
             bit8, stereo, frequency, sample_size);

    if (ioctl(soundfd, AUDIO_SETINFO, &audio_info) < 0) {
        perror("Could not set audio information ioctl");
        return(-1);
    }
    return 0;
}

/**
 * Play a sound using the Sun sound system.
 *
 * @param buffer
 * @param off
 * @return
 */
int audio_play(int buffer,int off){
    int wrote;

#ifdef SOUND_DEBUG
    printf("audio play - writing starting at %d, %d bytes",
    settings.buflen * buffer + off, settings.buflen - off);
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

/**
 * Opens the audio device, allocates buffers, and reads any configuration
 * files that need to be.
 *
 * @return Zero on success and on failure, the calling function will likely
 *         disable sound support/requests from the server.
 */
int init_sounds(void)
{
    int   i;
    FILE *fp;
    char  path[256], buf[512];

#ifdef SOUND_DEBUG
    fprintf( stderr, "Settings: bits: %i, ", settings.bit8 ? 8 : 16);
    fprintf( stderr, "%s, ",settings.sign ? "signed" : "unsigned");
    fprintf( stderr, "%s, ",settings.stereo ? "stereo" : "mono");
    fprintf( stderr, "frequency: %i, ", settings.frequency);
    fprintf( stderr, "device: %s\n", settings.audiodev);
#endif

    buffers = (char *) malloc(settings.buffers * settings.buflen);
    if (!buffers)
        return -1;

    sounds_in_buffer = (int *) calloc(settings.buffers, sizeof(int));
    if (!sounds_in_buffer)
        return -1;

    if (init_audio())
        return -1;

    if (sign)
        zerolevel = 0;
    else
        zerolevel = bit8 ? 0x80 : 0x00;

    memset(buffers, zerolevel, settings.buflen * settings.buffers);

#ifdef SOUND_DEBUG
    fprintf( stderr, "bits: %i, ", bit8 ? 8 : 16);
    fprintf( stderr, "%s, ", sign ? "signed" : "unsigned");
    fprintf( stderr, "%s, ", stereo ? "stereo" : "mono");
    fprintf( stderr, "freq: %i, ", frequency);
    fprintf( stderr, "smpl_size: %i, ", sample_size);
    fprintf( stderr, "0level: %i\n", zerolevel);
#endif

    for (i = 0; i < MAX_SOUNDS; i++) {
        normal_sounds[i].filename = NULL;
        spell_sounds[i].filename = NULL;
        normal_sounds[i].size = -1;
        spell_sounds[i].size = -1;
    }
    default_normal.filename = NULL;
    default_spell.filename = NULL;

    sprintf(path, "%s/.crossfire/sounds", getenv("HOME"));
    i = 0;
    if (!(fp = fopen(path, "r"))) {
        fprintf(stderr,
            "Unable to open %s - will use built in defaults\n", path);
        for (; i < sizeof(def_sounds) / sizeof(char*); i++) {
            strcpy(buf, def_sounds[i]);
            parse_sound_line(buf, i);
        }
    } else while (fgets(buf, 511, fp) != NULL) {
        buf[511] = '\0';
        parse_sound_line(buf, ++i);
    }
    /* Note in both cases below, we leave the symbolic name untouched. */
    for (i = 0; i < MAX_SOUNDS; i++) {
        if (!normal_sounds[i].filename) {
            normal_sounds[i].filename = default_normal.filename;
            normal_sounds[i].volume = default_normal.volume;
        }
        if (!spell_sounds[i].filename) {
            spell_sounds[i].filename = default_spell.filename;
            spell_sounds[i].volume = default_spell.volume;
        }
        normal_sounds[i].data = NULL;
        spell_sounds[i].data = NULL;
    }
    return 0;
}

#ifndef SDL_SOUND

/**
 * Add a sound to the buffer to be played later on.
 *
 * @param soundnum  The sound to play.
 * @param soundtype 0 for normal sounds, 1 for spell_sounds.
 * @param x         Offset (assumed from player) to play sound used to
 *                  determine value and left vs. right speaker balance.
 * @param y         Offset (assumed from player) to play sound used to
 *                  determine value and left vs. right speaker balance.
 */
static void play_sound(int soundnum, int soundtype, int x, int y) {
    Sound_Info *si;
    int buf, off;
    int i;
    unsigned left_ratio, right_ratio;
    double dist;

    /*
     * Switch to the next buffer in a circular fashion, wrapping around back
     * to the first as needed).
     */
    buf = current_buffer;
    if (buf >= settings.buffers)
        buf = 1;
    if (buf == 0)
        buf++;
    /*
     * Check if the buffer is "full".  If more than a specified number are
     * already buffered, do not add more.
     */
#ifdef SOUND_DEBUG
    fprintf(stderr,
        "Sounds in buffer %i: %i\n", buf, sounds_in_buffer[buf]);
#endif
    if (sounds_in_buffer[buf] > settings.simultaneously)
        return;
    /*
     * Ignore commands to play invalid/unsupported sound numbers.
     */
    if (soundnum >= MAX_SOUNDS || soundnum < 0) {
        fprintf(stderr, "Invalid sound number: %d\n", soundnum);
        return;
    }
    /*
     * If a sound device is not open, ignore the command to play a sound.
     */
    if (soundfd == -1) {
        fprintf(stderr, "Sound device is not open\n");
        return;
    }
    /*
     * Instead of fussing about a bad sound type being passed in, assume that
     * unsupported soundtype commands should play a standard sound.
     */
    if (soundtype < SOUND_NORMAL || soundtype == 0) {
        soundtype = SOUND_NORMAL;
    }
    /*
     * Get a pointer to the sound information for the given sound, and if it
     * does not include a filename, ignore the command to play the sound.
     */
    if (soundtype == SOUND_NORMAL) {
        si = &normal_sounds[soundnum];
    } else
        if (soundtype == SOUND_SPELL) {
            si = &spell_sounds[soundnum];
        } else {
            fprintf(stderr,"Unknown soundtype: %d\n", soundtype);
            return;
        }

    if (! si->filename) {
        fprintf(stderr,
            "Sound %d (type %d) is not defined\n", soundnum, soundtype);
        return;
    }
    /*
     * Attempt to load the sound data if it has not already been loaded.
     */
    if (! si->data) {
        FILE *f;
        struct stat sbuf;

#ifdef SOUND_DEBUG
        fprintf(stderr, "Loading file: %s\n", si->filename);
#endif

        /*
         * If the file isn't found as specified, report an error.  FIXME:  It
         * seems silly to require a full path to the sound file.  Surely it is
         * better to reference sounds from standard locations as is done with
         * themes, glade files, etc.
         */
        if (stat(si->filename, &sbuf)) {
            perror(si->filename);
            return;
        }
        /*
         * Save the size of the sound data.  If for some reason it is negative
         * ignore the sound file content.
         */
        si->size = sbuf.st_size;
        if (si->size <= 0)
            return;
        /*
         * If the sound file contains more data than can fit in the allocated
         * number of sound buffers, do not bother loading it.
         */
        if (si->size*sample_size > settings.buflen * (settings.buffers - 1)) {
            fprintf(stderr,
                "Sound %s too long (%i > %i)\n", si->filename, si->size,
                    settings.buflen * (settings.buffers - 1) / sample_size);
            return;
        }
        /*
         * Allocate space for reading the sound data, open the file, then
         * load the sound from the file.
         */
        si->data = (unsigned char *)malloc(si->size);
        f = fopen(si->filename, "r");
        if (! f) {
            perror(si->filename);
            return;
        }
        fread(si->data, 1, si->size, f);
        fclose(f);
    }

#ifdef SOUND_DEBUG
    fprintf(stderr,
        "Playing sound %i (%s), volume %i, x,y=%d,%d\n",
            soundnum, si->symbolic, si->volume, x, y);
#endif

    /* Calculate volume multiplers */
    dist = sqrt(x * x + y * y);
    right_ratio = left_ratio = ((1 << 16) * si->volume) /
        (100 * settings.simultaneously * (1 + SOUND_DECREASE * dist));
    if (stereo) {
        double diff;

        if (dist)
          diff = (1.0 - fabs((double) x / dist));
        else
          diff = 1;

#ifdef SOUND_DEBUG
        printf("diff: %f\n", diff);
        fflush(stdout);
#endif
        if (x < 0)
            right_ratio *= diff;
        else
            left_ratio *= diff;
    }

#ifdef SOUND_DEBUG
    fprintf(stderr, "Ratio: %i, %i\n", left_ratio, right_ratio);
#endif

    /* Insert the sound into the buffers */
    sounds_in_buffer[buf]++;
    off = 0;
    for(i = 0; i < si->size; i++) {
        int dat = si->data[i] - 0x80;

        if (bit8) {
            if (!stereo) {
                buffers[buf*settings.buflen+off]+=(dat*left_ratio)>>16;
            } else {
                buffers[buf*settings.buflen+off]+=(dat*left_ratio)>>16;
                buffers[buf*settings.buflen+off+1]+=(dat*right_ratio)>>16;
            }
        } else { /* 16 bit output */
            if (!stereo) {
#ifdef WORDS_BIGENDIAN
                buffers[buf*settings.buflen+off+1]+=((dat*left_ratio)>>8)&0xff;
                buffers[buf*settings.buflen+off]+=(dat*left_ratio)>>16;
            } else {
                buffers[buf*settings.buflen+off+1]+=((dat*left_ratio)>>8)&0xff;
                buffers[buf*settings.buflen+off]+=(dat*left_ratio)>>16;
                buffers[buf*settings.buflen+off+3]+=((dat*right_ratio)>>8)&0xff;
                buffers[buf*settings.buflen+off+2]+=(dat*right_ratio)>>16;
            }
#else
                buffers[buf*settings.buflen+off]+=((dat*left_ratio)>>8)&0xff;
                buffers[buf*settings.buflen+off+1]+=(dat*left_ratio)>>16;
            } else {
                buffers[buf*settings.buflen+off]+=((dat*left_ratio)>>8)&0xff;
                buffers[buf*settings.buflen+off+1]+=(dat*left_ratio)>>16;
                buffers[buf*settings.buflen+off+2]+=((dat*right_ratio)>>8)&0xff;
                buffers[buf*settings.buflen+off+3]+=(dat*right_ratio)>>16;
            }
#endif
        }

        off += sample_size;

        if (off >= settings.buflen) {
            off = 0;
            buf++;
            if (buf >= settings.buffers) {
                buf = 0;
            }
        }
    }
#ifdef SOUND_DEBUG
    fprintf(stderr,
        "Added %d bytes, last buffer=%d, lastpos=%d\n",
            si->size, buf, off);
#endif
    /* This write did not wrap the buffers */
    if (buf+1 > current_buffer) {
        if ((buf+1 > first_free_buffer) && (first_free_buffer >= current_buffer))
            first_free_buffer = buf+1;
    } else {    /* Buffers did wrap */
        if (((buf+1 > first_free_buffer)
        &&   (first_free_buffer < current_buffer))
        ||  (first_free_buffer >= current_buffer))
            first_free_buffer = buf + 1;
    }
    if (first_free_buffer >= settings.buffers)
        first_free_buffer = 0;
}

#else

/**
 * Add a sound to the buffer to be played later on.
 *
 * @param soundnum  The sound to play.
 * @param soundtype 0 for normal sounds, 1 for spell_sounds.
 * @param x         Offset (assumed from player) to play sound used to
 *                  determine value and left vs. right speaker balance.
 * @param y         Offset (assumed from player) to play sound used to
 *                  determine value and left vs. right speaker balance.
 */
static void play_sound(int soundnum, int soundtype, int x, int y) {
    int         channel = 0;
    int         playing = 0;
    Sound_Info *si;

    /*
     * Ignore commands to play invalid/unsupported sound numbers.
     */
    if (soundnum >= MAX_SOUNDS || soundnum < 0) {
        fprintf(stderr, "play_sound: Invalid sound number: %d\n", soundnum);
        return;
    }

     /*
     * Check if the channel limit is reached.  If more than a specified number
     * are already playing, do not add more sounds.
     */
    playing = Mix_Playing(-1);
#ifdef SOUND_DEBUG
    fprintf(stderr,
        "Channels playing: %i of %i\n", playing, max_chunk);
#endif
    if (playing >= max_chunk) {
        /*
         * Bail. Only max_chunk channels were set up for use.  It might not
         * be that bad to allocate more, but then nobody wants a cachophony.
         * Where does one stop?
         */
         return;
    }
    /*
     * Find a channel that is not playing anything...
     */
    for (channel = 1; channel < max_chunk; channel++) {
        if (! Mix_Playing(channel)) {
            if (! chunk[channel]) {
                Mix_FreeChunk(chunk[channel]);
                chunk[channel] = NULL;
            }
            break;
        }
    }
    /*
     * Get a pointer to the sound information for the given sound, and if it
     * does not include a filename, ignore the command to play the sound.
     */
    if (soundtype == SOUND_NORMAL) {
        si = &normal_sounds[soundnum];
    } else
        if (soundtype == SOUND_SPELL) {
            si = &spell_sounds[soundnum];
        } else {
            fprintf(stderr,"play_sound: Unknown soundtype: %d\n", soundtype);
            return;
        }

    if (! si->filename) {
        fprintf(stderr,
            "play_sound: Sound %d (type %d) missing\n", soundnum, soundtype);
        return;
    }
    /*
     * Attempt to load the sound data.
     */
    chunk[channel] = Mix_LoadWAV(si->filename);
    if (! chunk[channel]) {
        fprintf(stderr, "play_sound: Mix_LoadWAV: %s\n", Mix_GetError());
        return;
    }

    if (Mix_PlayChannel(channel, chunk[channel], 0) == -1) {
        fprintf(stderr, "Mix_PlayChannel: %s\n",Mix_GetError());
    }
}

#endif

/**
 * Handle sound-related commands that are received from stdin.  Sound commands
 * consist of four whitespace delimited values:  sound_number, sound_type, x,
 * and y.  The sound_number and sound_type identify which sound should play,
 * while x and y represents the map coordinate difference between the sound
 * source and the player/character.
 *
 * @param data A text buffer that (hopefully) contains a sound command.
 * @param len  The length of the text data in the sound command buffer.
 * @return     0 if the buffer is a well-formed sound command, otherwise -1.
 */
int SoundCmd(unsigned char *data, int len) {
    int x, y, num, type;
    int i;

    i = sscanf((char *)data,"%x %x %x %x", &num, &type, &x, &y);
    if (i != 4){
        fprintf(stderr, "Wrong input!\n");
        return -1;
    }

#ifdef SOUND_DEBUG
    fprintf(stderr,
        "Playing sound %d (type %d), offset %d, %d\n", num, type, x ,y);
#endif

    play_sound(num, type, x, y);
    return 0;
}

/**
 * Update the player .crossfire/sndconfig file.
 *
 * @return
 */
int write_settings(void) {
    FILE *f;
    char *home;
    char *path;

    if ((home = getenv("HOME")) == NULL)
        return -1;
    path = (char *)malloc(strlen(home) + strlen(CONFIG_FILE) + 1);
    if (!path)
        return -1;

    strcpy(path, home);
    strcat(path, CONFIG_FILE);
    f = fopen(path, "w");

    if (!f)
        return -1;

    fprintf(f, "# Crossfire sound server settings\n");
    fprintf(f, "# Please note, that not everything will work\n\n");
    fprintf(f, "stereo: %i\n", settings.stereo);
    fprintf(f, "bits: %i\n", settings.bit8?8:16);
    fprintf(f, "signed: %i\n", settings.sign);
    fprintf(f, "frequency: %i\n", settings.frequency);
    fprintf(f, "buffers: %i\n", settings.buffers);
    fprintf(f, "buflen: %i\n", settings.buflen);
    fprintf(f, "simultaneously: %i\n", settings.simultaneously);
    /* fprintf(f,"device: %s\n",settings.audiodev); */
    fclose(f);
    return 0;
}

/**
 * Read the player .crossfire/sndconfig file.
 *
 * @return
 */
int read_settings(void) {
    FILE *f;
    char *home;
    char *path;
    char linebuf[1024];

    if ((home = getenv("HOME")) == NULL )
        return 0;

    path = (char *)malloc(strlen(home) + strlen(CONFIG_FILE) + 1);
    if (!path)
        return 0;

    strcpy(path, home);
    strcat(path, CONFIG_FILE);
    f = fopen(path, "r");
    if (!f)
        return -1;

    while(fgets(linebuf, 1023, f) != NULL) {
        linebuf[1023] = 0;
        /* Strip off the newline */
        linebuf[strlen(linebuf) - 1] = 0;

        if (strncmp(linebuf, "stereo:", strlen("stereo:")) == 0)
            settings.stereo = atoi(linebuf + strlen("stereo:")) ? 1 : 0;
        else if (strncmp(linebuf, "bits:", strlen("bits:")) == 0)
            settings.bit8 = (atoi(linebuf + strlen("bits:"))==8) ? 1 : 0;
        else if (strncmp(linebuf, "signed:", strlen("signed:")) == 0)
            settings.sign = atoi(linebuf + strlen("signed:")) ? 1 : 0;
        else if (strncmp(linebuf, "buffers:", strlen("buffers:")) == 0)
            settings.buffers = atoi(linebuf + strlen("buffers:"));
        else if (strncmp(linebuf, "buflen:", strlen("buflen:")) == 0)
            settings.buflen = atoi(linebuf + strlen("buflen:"));
        else if (strncmp(linebuf, "frequency:", strlen("frequency:")) == 0)
            settings.frequency = atoi(linebuf + strlen("frequency:"));
        else if (strncmp(linebuf, "simultaneously:", strlen("simultaneously:")) == 0)
            settings.simultaneously = atoi(linebuf + strlen("simultaneously:"));
#if 0
        else if (strncmp(linebuf,"device: ",strlen("device: "))==0)
                settings.audiodev=strdup_local(linebuf+strlen("device: "));
#endif
    }
    fclose(f);
    return 0;
}

/**
 * A sound server that is based on the use of file descriptors.
 *
 */
void fd_server(void) {
    int infd;
    char inbuf[1024];
    int inbuf_pos = 0, sndbuf_pos = 0;
    fd_set inset, outset;

    infd = fileno(stdin);
    FD_ZERO(&inset);
    FD_ZERO(&outset);
    FD_SET(soundfd, &outset);
    FD_SET(infd, &inset);
    while (1) {
#if defined(SGI_SOUND)
        /*
         * The buffer of an audio port can hold 100000 samples. If we allow
         * sounds to be written to the port whenever there is enough room in the
         * buffer, all sounds will be played sequentially, which is wrong. We
         * can set the fillpoint to a high value to prevent this.
         */
        ALsetfillpoint(soundport, 100000);
#endif

        select(FD_SETSIZE, &inset, &outset, NULL, NULL);

        if (FD_ISSET(soundfd, &outset)) {
            /* no sounds to play */
            if (current_buffer == first_free_buffer) FD_CLR(soundfd, &outset);
            else {
                int wrote;
                wrote = audio_play(current_buffer, sndbuf_pos);
                if (wrote < settings.buflen - sndbuf_pos) sndbuf_pos += wrote;
                else {
                   /* clean the buffer */
                   memset(buffers + settings.buflen * current_buffer,
                       zerolevel, settings.buflen);
                   sounds_in_buffer[current_buffer] = 0;
                   sndbuf_pos = 0;
                   current_buffer++;
                   if (current_buffer >= settings.buffers)
                       current_buffer = 0;
                }
            }
        } else {
            /*
             * We need to reset this if it is not set - otherwise, we will never
             * finish playing the sounds
             */
            FD_SET(soundfd, &outset);
        }

        if (FD_ISSET(infd, &inset)) {
            int err = read(infd, inbuf + inbuf_pos, 1);

            if (err < 1 && errno != EINTR) {
                if (err < 0)
                    perror("read");
                break;
            }
            if (inbuf[inbuf_pos] == '\n') {
                inbuf[inbuf_pos++] = 0;
                if (!SoundCmd((unsigned char*) inbuf, inbuf_pos))
                    FD_SET(soundfd, &outset);
                inbuf_pos = 0;
            } else {
                inbuf_pos++;
                if (inbuf_pos >= 1024) {
                    fprintf(stderr, "Input buffer overflow!\n");
                    inbuf_pos = 0;
                }
            }
        }
        FD_SET(infd, &inset);
    }
}

/**
 * A sound server that is specific to the SDL_mixer library, and not based on
 * the use of file descriptors.
 *
 */
void sdl_mixer_server(void) {
    int        infd;
    fd_set     inset;
    char       inbuf[1024];
    int        inbuf_pos = 0;
    int        mix_flags = MIX_INIT_OGG;
    int        mix_init = 0;
    int        channel = 0;

    mix_init = Mix_Init(MIX_INIT_OGG);

    if ((mix_init & mix_flags) != mix_flags) {
#ifdef SOUND_DEBUG
        printf("Mix_Init: Failed to init required ogg support!\n");
        printf("Mix_Init: %s\n", Mix_GetError());
#endif
        return;
    }

    max_chunk = Mix_AllocateChannels(settings.simultaneously);
    chunk = calloc(max_chunk, sizeof(chunk));
    if (! chunk) {
        return;
    }

    infd = fileno(stdin);
    /*
     * Initialize the file descriptor set "inset" to be the empty set, then
     * add stdin to the file descriptor set.
     */
    FD_ZERO(&inset);
    FD_SET(infd, &inset);

    music = Mix_LoadMUS("sample.ogg");
    Mix_PlayMusic(music, 0);

    while (1) {
        /*
         * Handle sound commands from stdin
         *
         * If stdin is (still) a member of the file descriptor set "inset",
         * process input from it.
         */
        if (FD_ISSET(infd, &inset)) {
            int result;

            /*
             * Read a character from stdin, or wait for input if there isn't
             * any yet.  Append it to the other data already buffered up.
             */
            result = read(infd, inbuf + inbuf_pos, 1);
            if (result == -1 && errno != EINTR) {
                if (result == -1)
                    perror("read");
                break;
            }
            /*
             * Check to see if the last character was an end-of-line marker.
             * If so, analyze the buffer contents, otherwise just collect
             * the data while monitoring it to be sure it doesn't overflow.
             */
            if (inbuf[inbuf_pos] == '\n') {
                inbuf[inbuf_pos++] = 0;
                if (! SoundCmd((unsigned char*) inbuf, inbuf_pos)) {
                    /*
                     *
                     */
                }
                inbuf_pos = 0;
            } else {
                inbuf_pos++;
                if (inbuf_pos >= sizeof(inbuf)) {
                    fprintf(stderr, "Input buffer overflow!\n");
                    inbuf_pos = 0;
                }
            }
        }
        FD_SET(infd, &inset);

        if (! Mix_PlayingMusic()) {
            break;
        }

        /* So we don't hog the CPU */
        SDL_Delay(50);
    }

    Mix_HaltMusic();
    Mix_FreeMusic(music);
    music = NULL;

    /*
     * Halt all channels playing, and de-allocate any Mix_Chunks that exist.
     */
    Mix_HaltChannel(-1);
    for (channel = 0; channel < max_chunk; channel++) {
        if (chunk[channel]) {
            Mix_FreeChunk(chunk[channel]);
            chunk[channel] = NULL;
        }
    }
    /*
     * As long as Mix_Init() was only called once, Mix_Quit() should only need
     * to be called once, but this covers all the bases.
     */
    while(Mix_Init(0)) {
        Mix_Quit();
    }
}

/**
 *
 * @param argc
 * @param argv
 * @return
 */
int main(int argc, char *argv[]) {
    printf("%s\n", rcsid_sound_src_cfsndserv_c);
    fflush(stdout);

    if (read_settings())
        write_settings();

    if (init_sounds())
        return 1;

    if (!soundfd) {
#ifdef SDL_SOUND
        sdl_mixer_server();
#else
#ifdef SOUND_DEBUG
        fprintf(stderr, "A file descriptor is not assigned.\n");
#endif
        return 1;
#endif
    } else {
        fd_server();
    }

    return 0;
}

