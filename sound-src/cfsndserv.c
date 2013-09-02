static char *rcsid_sound_src_cfsndserv_c =
    "$Id$";

/*
 * Crossfire -- cooperative multi-player graphical RPG and adventure game
 *
 * Copyright (c) 1999-2013 Mark Wedel and the Crossfire Development Team
 * Copyright (c) 1992 Frank Tore Johansen
 *
 * Crossfire is free software and comes with ABSOLUTELY NO WARRANTY. You are
 * welcome to redistribute it under certain conditions. For details, please
 * see COPYING and LICENSE.
 *
 * The authors can be reached via e-mail at <crossfire@metalforge.org>.
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
#include "client-types.h"
#include "def_sounds.h"
#include "sndproto.h"
#include "common.h"

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

#define SOUND_DECREASE 0.1

int  *sounds_in_buffer = NULL;
int   current_buffer = 0;               /**< The next buffer to write out   */
int   first_free_buffer = 0;            /**< Help know when to stop playing */

#ifdef SDL_SOUND                        /* SDL sound and music declarations */
/*
 * Mixer variables
 */
int            audio_channels = 0;      /**< Channels in use by SDL_mixer   */
Uint16         audio_format = 0;        /**< Format of the SDL_mixer audio  */
Mix_Music     *music = NULL;            /**< A music file to play           */
Mix_Chunk    **chunk = NULL;            /**< Loaded sounds to play          */
int            max_chunk = 0;           /**< Max count of concurrent sounds */

sound_settings settings = { 0, 1, 0, 11025, 100, 4096, 4, AUDIODEV };

#else                                   /* Legacy, sound-only declarations. */

int soundfd=0;

#ifdef SUN_SOUND                        /* Sun support legacy declarations. */

sound_settings settings = { 0, 1, 1, 11025, 100, 4096, 4, AUDIODEV };

#else                                   /* Alsa/OSS/SGI legacy declarations */

sound_settings settings = { 0, 1, 0, 11025, 100, 1024, 4, AUDIODEV };

#endif                                  /* End of legacy-only declarations. */
#endif                                  /* End of old and new declarations. */

#if defined(SDL_SOUND)                  /* Begin init_audio(), audio_play() */

/**
 * Initialize the SDL_mixer sound system.
 *
 * @return Zero if audio initialized successfully, otherwise -1.
 */
int init_audio(void)
{

    printf("cfsndserv for the SDL_mixer sound system\n");
    printf("supports sound effects and background music.\n");
    fflush(stdout);

#ifdef SOUND_DEBUG
    fprintf(stderr, "init_audio: SDL_SOUND\n");
    fflush(stderr);
#endif

    if (SDL_Init(SDL_INIT_AUDIO) == -1) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        exit(1);
    }

    frequency = settings.frequency;
    bit8 = settings.bit8;
    if (settings.bit8) {
        audio_format = settings.sign ? AUDIO_S8 : AUDIO_U8;
    } else {
        audio_format = settings.sign ? AUDIO_S16 : AUDIO_U16;
    }
    audio_channels = (stereo = settings.stereo) ? 1 : 2;
    /*
     * This is where we open up our audio device.  Mix_OpenAudio takes as its
     * parameters the audio format we'd /like/ to have.
     */
    if (Mix_OpenAudio(frequency,audio_format,audio_channels,settings.buflen)) {
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
 * Play a sound effect using the SDL_mixer sound system.
 *
 * @todo Add SDL mixer sound effect support.
 *
 * @param buffer
 * @param off
 * @return The return value is always zero and is unused by SDL_SOUND support.
 *         It is provided to maintain consistency with the legacy sound system
 *         functions of the same name.
 */
int audio_play(int buffer, int off)
{

#ifdef SOUND_DEBUG
    fprintf(stderr, "audio_play: SDL_SOUND\n");
    fflush(stderr);
#endif

    return 0;
}

#elif defined(ALSA_SOUND)               /* End of SDL_mixer sound section.  */

/**
 * Initialize the Alsa sound system.
 *
 * @return Zero if audio initialized successfully, otherwise -1.
 */
int init_audio(void)
{
    int card=0, device=0, err;
    snd_pcm_channel_params_t params;

    printf("cfsndserv for older ALSA sound systems\n");
    printf("supports sound effects only (no background music).\n");
    fflush(stdout);

#ifdef SOUND_DEBUG
    fprintf(stderr, "init_audio: ALSA\n");
    fflush(stderr);
#endif

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
int audio_play(int buffer, int off)
{

#ifdef SOUND_DEBUG
    fprintf(stderr, "audio_play: ALSA\n");
    fflush(stderr);
#endif

    return
        snd_pcm_write
        (handle, buffers+settings.buflen*buffer+off, settings.buflen-off);
}

#elif defined(OSS_SOUND)                /* End of Alsa sound section.       */

/**
 * Initialize the OSS sound system.
 *
 * @return Zero if audio initialized successfully, otherwise -1.
 */
int init_audio(void)
{

    const char *audiodev;
    int value,format,tmp;

    printf("cfsndserv for the OSS sound system\n");
    printf("supports sound effects only (no background music).\n");
    fflush(stdout);

#ifdef SOUND_DEBUG
    fprintf(stderr, "init_audio: OSS\n");
    fflush(stderr);
#endif

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
    for (tmp = settings.buflen / 2; tmp; tmp >>= 1) {
        value++;
    }

    value |= 0x00020000;
    if (ioctl(soundfd, SNDCTL_DSP_SETFRAGMENT, &value) < 0) {
        fprintf(stderr, "Could not set audio fragment spec\n");
        return(-1);
    }
    if (settings.bit8) {
        format = settings.sign ? AFMT_S8 : AFMT_U8;
    } else {
        format = settings.sign ? AFMT_S16_LE : AFMT_U16_LE;
    }

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
int audio_play(int buffer, int off)
{
    int wrote;

#ifdef SOUND_DEBUG
    fprintf(stderr, "audio_play: OSS\n");
    fflush(stderr);

    printf("audio play: write starting at %d, %d bytes",
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

#elif defined(SGI_SOUND)                /* End of OSS sound section.        */

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

    printf("cfsndserv for the SGI sound systems\n");
    printf("supports sound effects only (no background music).\n");
    fflush(stdout);

#ifdef SOUND_DEBUG
    fprintf(stderr, "init_audio: SGI\n");
    fflush(stderr);
#endif

    /* Allocate ALconfig structure. */

    if ((soundconfig = ALnewconfig()) == 0) {
        fprintf(stderr, "Could not allocate ALconfig structure.\n");
        return -1;
    }

    /* Set number of channels */

    if (ALsetchannels(soundconfig, (stereo = settings.stereo) ? 2 : 1) == -1) {
        fprintf(stderr, "Could not set number of channels.\n");
        return -1;
    }

    /* Set sample format */

    if (ALsetsampfmt(soundconfig, AL_SAMPFMT_TWOSCOMP) == -1) {
        fprintf(stderr, "Could not set audio sample format.\n");
        return -1;
    }
    sign = 1;

    /* Set sample width */
    if (ALsetwidth(
                soundconfig, (bit8 = settings.bit8)?AL_SAMPLE_8:AL_SAMPLE_16) == -1) {
        fprintf(stderr,"Could not set audio sample width.\n");
        return -1;
    }
    sample_size = (stereo ? 2 : 1) * (bit8 ? 1 : 2);

    /* Set frequency */

    params[0] = AL_OUTPUT_RATE;
    params[1] = frequency = settings.frequency;
    if (ALsetparams(AL_DEFAULT_DEVICE, params, 2) == -1) {
        fprintf(stderr, "Could not set output rate of default device.\n");
        return -1;
    }

    /* Open audio port */

    if ((soundport = ALopenport("cfsndserv port", "w", soundconfig)) == NULL) {
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
#ifdef SOUND_DEBUG
    fprintf(stderr, "audio_play: SGI\n");
    fflush(stderr);
#endif

    ALwritesamps(
        soundport, buffers + settings.buflen * buffer+off,
        (settings.buflen - off) / sample_size);
    return settings.buflen-off;
}

#elif defined(SUN_SOUND)                /* End of SGI sound section.        */

/**
 * Initialize the Sun sound system.
 *
 * @return Zero if audio initialized successfully, otherwise -1.
 */
int init_audio(void)
{

    const char *audiodev;
    int value, format, tmp;
    audio_info_t audio_info;
    audio_device_t audio_device;

    printf("cfsndserv for the Sun sound systems\n");
    printf("supports sound effects only (no background music).\n");
    fflush(stdout);

#ifdef SOUND_DEBUG
    fprintf(stderr, "init_audio: Sun\n");
    fflush(stderr);
#endif

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
int audio_play(int buffer,int off)
{
    int wrote;

#ifdef SOUND_DEBUG
    fprintf(stderr, "audio_play: Sun\n");
    fflush(stderr);

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
/* End of Sun sound section.        */
#endif                                  /* End init_audio() & audio_play(). */

#ifdef SDL_SOUND                        /* Begin play_sound(), play_music() */

/**
 * Play a sound effect using the SDL_mixer sound system.
 *
 * @param soundnum  The sound to play.
 * @param soundtype 0 for normal sounds, 1 for spell_sounds.
 * @param x         Offset (assumed from player) to play sound used to
 *                  determine value and left vs. right speaker balance.
 * @param y         Offset (assumed from player) to play sound used to
 *                  determine value and left vs. right speaker balance.
 */
void play_sound(int soundnum, int soundtype, int x, int y)
{
    char path[MAXSOCKBUF];
    int         channel = 0;
    int         playing = 0;
    Sound_Info *si;

#ifdef SOUND_DEBUG
    fprintf(stderr, "play_sound: SDL_mixer\n");
    fflush(stderr);
#endif

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

    /* TODO: To fix a compatability issue with the client, SOUND_SPELL is the
     * same as '2'. This should be fixed in the client code soon. */
    if (soundtype == SOUND_NORMAL) {
        si = &normal_sounds[soundnum];
    } else if (soundtype == SOUND_SPELL || soundtype == 2) {
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
    snprintf(path, sizeof(path), "%s%s.ogg", client_sounds_path, si->filename);
    fprintf(stderr, "Attempting to load sound from '%s'\n", path);

    chunk[channel] = Mix_LoadWAV(path);
    if (! chunk[channel]) {
        fprintf(stderr, "play_sound: Mix_LoadWAV: %s\n", Mix_GetError());
        return;
    }

    if (Mix_PlayChannel(channel, chunk[channel], 0) == -1) {
        fprintf(stderr, "Mix_PlayChannel: %s\n",Mix_GetError());
    }
}

/**
 * Play music with the SDL_mixer sound system.
 *
 * @param name A name of a song to play that does not include anything like
 *             path or file extensions.  It is up to this function to map the
 *             name to a file.
 */
void play_music(const char* music_name)
{
    char path[MAXSOCKBUF];
    struct stat statbuf;
    int  namelen;
    int  pathlen;
    int  fd = -1;

#ifdef SOUND_DEBUG
    fprintf(stderr, "play_music: SDL_mixer\n");
    fflush(stderr);
#endif

    namelen = strlen(music_name);
    pathlen = strlen(user_sounds_path);
    if (pathlen + namelen + 5 > sizeof(path)) {
        return;
    }
    path[sizeof(path) - 1] = '\0';
    snprintf(path, sizeof(path), "%s%s.ogg", user_sounds_path, music_name);

    if (stat(path, &statbuf) != -1) {
        if ((statbuf.st_mode & S_IFMT) == S_IFREG) {
            fd = open (path, O_RDONLY);
            if (fd != -1) {
                close(fd);
            }
        }
    }

    if (fd == -1) {
#ifdef SOUND_DEBUG
        fprintf(stderr, "play_music: %s not found.\n", path);
#endif
        pathlen = strlen(client_sounds_path);
        if (pathlen + namelen + 5 > sizeof(path)) {
            return;
        }
        path[sizeof(path) - 1] = '\0';
        snprintf(path, sizeof(path), "%s%s.ogg", client_sounds_path, music_name);

        if (stat(path, &statbuf) != -1) {
            if (statbuf.st_mode == S_IFREG) {
                fd = open (path, O_RDONLY);
                if (fd != -1) {
                    close(fd);
                }
            }
        }
    }

    if (fd == -1) {
#ifdef SOUND_DEBUG
        fprintf(stderr, "play_music: %s not found.\n", path);
#else
        fprintf(stderr, "play_music: music %x.ogg not found\n", music_name);
#endif
        return;
    }

    music = Mix_LoadMUS(path);
    Mix_PlayMusic(music, 0);
    return ;
}

/**
 * A sound server that is specific to the SDL_mixer library, and not based on
 * the use of file descriptors.
 *
 */
void sdl_mixer_server(void)
{
    int        infd;
    fd_set     inset;
    char       inbuf[1024];
    int        inbuf_pos = 0;
    int        mix_flags = MIX_INIT_OGG;
    int        mix_init = 0;
    int        channel = 0;

#ifdef SOUND_DEBUG
    fprintf(stderr, "sdl_mixer_server: starting.\n");
    fflush(stderr);
#endif

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

#if 1
    play_music("sample");
#endif

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
                if (result == -1) {
                    perror("read");
                }
                break;
            }
            /*
             * Check to see if the last character was an end-of-line marker.
             * If so, analyze the buffer contents, otherwise just collect
             * the data while monitoring it to be sure it doesn't overflow.
             */
            if (inbuf[inbuf_pos] == '\n') {
                inbuf[inbuf_pos++] = 0;
                if (! StdinCmd((char*) inbuf, inbuf_pos)) {
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

#else                                   /* Begin legacy play_sound(),
                                         * play_music(), and fd_server().
                                         */
/**
 * Add a sound to the buffer to be played later on.  This function is common
 * to all systems except SDL_SOUND.
 *
 * @param soundnum  The sound to play.
 * @param soundtype 0 for normal sounds, 1 for spell_sounds.
 * @param x         Offset (assumed from player) to play sound used to
 *                  determine value and left vs. right speaker balance.
 * @param y         Offset (assumed from player) to play sound used to
 *                  determine value and left vs. right speaker balance.
 */
void play_sound(int soundnum, int soundtype, int x, int y)
{
    Sound_Info *si;
    int buf, off;
    int i;
    unsigned left_ratio, right_ratio;
    double dist;

#ifdef SOUND_DEBUG
    fprintf(stderr, "play_sound: legacy sound systems\n");
    fflush(stderr);
#endif

    /*
     * Switch to the next buffer in a circular fashion, wrapping around back
     * to the first as needed).
     */
    buf = current_buffer;
    if (buf >= settings.buffers) {
        buf = 1;
    }
    if (buf == 0) {
        buf++;
    }
    /*
     * Check if the buffer is "full".  If more than a specified number are
     * already buffered, do not add more.
     */
#ifdef SOUND_DEBUG
    fprintf(stderr,
            "Sounds in buffer %i: %i\n", buf, sounds_in_buffer[buf]);
#endif
    if (sounds_in_buffer[buf] > settings.simultaneously) {
        return;
    }
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
    } else if (soundtype == SOUND_SPELL) {
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
        if (si->size <= 0) {
            return;
        }
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

        if (dist) {
            diff = (1.0 - fabs((double) x / dist));
        } else {
            diff = 1;
        }

#ifdef SOUND_DEBUG
        printf("diff: %f\n", diff);
        fflush(stdout);
#endif
        if (x < 0) {
            right_ratio *= diff;
        } else {
            left_ratio *= diff;
        }
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
            }
            else {
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
    if (buf+1 > current_buffer)
    {
        if ((buf+1 > first_free_buffer) && (first_free_buffer >= current_buffer)) {
            first_free_buffer = buf+1;
        }
    } else      /* Buffers did wrap */
    {
        if (((buf+1 > first_free_buffer)
                &&   (first_free_buffer < current_buffer))
                ||  (first_free_buffer >= current_buffer)) {
            first_free_buffer = buf + 1;
        }
    }
    if (first_free_buffer >= settings.buffers)
    {
        first_free_buffer = 0;
    }
}

/**
 * Music is not supported by the legacy (non-SDL_mixer) sound systems.
 *
 * @param name A name of a song to play that does not include anything like
 *             path or file extensions.  It is up to this function to map the
 *             name to a file.
 */
void play_music(const char* name)
{

#ifdef SOUND_DEBUG
    fprintf(stderr, "play_music: no music support for this sound system.\n");
    fflush(stderr);
#endif

    return;
}

/**
 * A sound server that is based on the use of file descriptors.
 *
 */
void fd_server(void)
{
    int infd;
    char inbuf[1024];
    int inbuf_pos = 0, sndbuf_pos = 0;
    fd_set inset, outset;

#ifdef SOUND_DEBUG
    fprintf(stderr, "fd_server: starting.\n");
    fflush(stderr);
#endif

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
            if (current_buffer == first_free_buffer) {
                FD_CLR(soundfd, &outset);
            } else {
                int wrote;
                wrote = audio_play(current_buffer, sndbuf_pos);
                if (wrote < settings.buflen - sndbuf_pos) {
                    sndbuf_pos += wrote;
                } else {
                    /* clean the buffer */
                    memset(buffers + settings.buflen * current_buffer,
                           zerolevel, settings.buflen);
                    sounds_in_buffer[current_buffer] = 0;
                    sndbuf_pos = 0;
                    current_buffer++;
                    if (current_buffer >= settings.buffers) {
                        current_buffer = 0;
                    }
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
                if (err < 0) {
                    perror("read");
                }
                break;
            }
            if (inbuf[inbuf_pos] == '\n') {
                inbuf[inbuf_pos++] = 0;
                if (!StdinCmd(inbuf, inbuf_pos)) {
                    FD_SET(soundfd, &outset);
                }
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

#endif                                  /* End of play_sound(), play_music(),
                                         * and servers.
                                         */
/**
 *
 * @param argc
 * @param argv
 * @return
 */
int main(int argc, char *argv[])
{
    printf("%s\n", rcsid_sound_src_cfsndserv_c);
    fflush(stdout);

    if (read_settings()) {
        write_settings();
    }

    if (init_sounds()) {
        return 1;
    }

#ifdef SDL_SOUND
    sdl_mixer_server();
#else
    if (! soundfd) {
#ifdef SOUND_DEBUG
        fprintf(stderr, "A file descriptor is not assigned.\n");
#endif
        return 1;
    } else {
        fd_server();
    }
#endif

    return 0;
}

