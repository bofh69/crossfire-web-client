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
 * Implements a server for sound support in the client using SDL_mixer.
 */

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

#  include "SDL.h"
#  include "SDL_mixer.h"
#  define AUDIODEV "/foo/bar"

int  *sounds_in_buffer = NULL;
int   current_buffer = 0;               /**< The next buffer to write out   */
int   first_free_buffer = 0;            /**< Help know when to stop playing */

/*
 * Mixer variables
 */
int            audio_channels = 0;      /**< Channels in use by SDL_mixer   */
Uint16         audio_format = 0;        /**< Format of the SDL_mixer audio  */
Mix_Music     *music = NULL;            /**< A music file to play           */
Mix_Chunk    **chunk = NULL;            /**< Loaded sounds to play          */
int            max_chunk = 0;           /**< Max count of concurrent sounds */

sound_settings settings = { 0, 1, 0, 11025, 100, 4096, 4, AUDIODEV };

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

    sdl_mixer_server();
    return 0;
}
