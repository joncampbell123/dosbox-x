/*
 * SDL_sound -- An abstract sound format decoding API.
 * Copyright (C) 2001  Ryan C. Gordon.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 * Internal function/structure declaration. Do NOT include in your
 *  application.
 *
 * Please see the file LICENSE.txt in the source's root directory.
 *
 *  This file written by Ryan C. Gordon. (icculus@icculus.org)
 */

#ifndef _INCLUDE_SDL_SOUND_INTERNAL_H_
#define _INCLUDE_SDL_SOUND_INTERNAL_H_

#ifndef __SDL_SOUND_INTERNAL__
#error Do not include this header from your applications.
#endif

#include <SDL.h>

/* SDL 1.2.4 defines this, but better safe than sorry. */
#if (!defined(__inline__))
#  define __inline__
#endif

#if (defined DEBUG_CHATTER)
#define SNDDBG(x) printf x
#else
#define SNDDBG(x)
#endif

#if HAVE_ASSERT_H
#  include <assert.h>
#endif

#ifdef _WIN32_WCE
    extern char *strrchr(const char *s, int c);
#   ifdef NDEBUG
#       define assert(x)
#   else
#       define assert(x) if(!x) { fprintf(stderr,"Assertion failed in %s, line %s.\n",__FILE__,__LINE__); fclose(stderr); fclose(stdout); exit(1); }
#   endif
#endif


#if (!defined assert)  /* if all else fails. */
#  define assert(x)
#endif


/*
 * SDL itself only supports mono and stereo output, but hopefully we can
 *  raise this value someday...there's probably a lot of assumptions in
 *  SDL_sound that rely on it, though.
 */
#define MAX_CHANNELS 2


typedef struct __SOUND_DECODERFUNCTIONS__
{
        /* This is a block of info about your decoder. See SDL_sound.h. */
    const Sound_DecoderInfo info;

        /*
         * This is called during the Sound_Init() function. Use this to
         *  set up any global state that your decoder needs, such as
         *  initializing an external library, etc.
         *
         * Return non-zero if initialization is successful, zero if there's
         *  a fatal error. If this method fails, then this decoder is
         *  flagged as unavailable until SDL_sound() is shut down and
         *  reinitialized, in which case this method will be tried again.
         *
         * Note that the decoders quit() method won't be called if this
         *  method fails, so if you can't intialize, you'll have to clean
         *  up the half-initialized state in this method.
         */
    int (*init)(void);

        /*
         * This is called during the Sound_Quit() function. Use this to
         *  clean up any global state that your decoder has used during its
         *  lifespan.
         */
    void (*quit)(void);

        /*
         * Returns non-zero if (sample) has a valid fileformat that this
         *  driver can handle. Zero if this driver can NOT handle the data.
         *
         * Extension, which may be NULL, is just a hint as to the form of
         *  data that is being passed in. Most decoders should determine if
         *  they can handle the data by the data itself, but others, like
         *  the raw data handler, need this hint to know if they should
         *  accept the data in the first place.
         *
         * (sample)'s (opaque) field should be cast to a Sound_SampleInternal
         *  pointer:
         *
         *   Sound_SampleInternal *internal;
         *   internal = (Sound_SampleInternal *) sample->opaque;
         *
         * Certain fields of sample will be filled in for the decoder before
         *  this call, and others should be filled in by the decoder. Some
         *  fields are offlimits, and should NOT be modified. The list:
         *
         * in Sound_SampleInternal section:
         *    Sound_Sample *next;  (offlimits)
         *    Sound_Sample *prev;  (offlimits)
         *    SDL_RWops *rw;       (can use, but do NOT close it)
         *    const Sound_DecoderFunctions *funcs; (that's this structure)
         *    Sound_AudioCVT sdlcvt; (offlimits)
         *    void *buffer;        (offlimits until read() method)
         *    Uint32 buffer_size;  (offlimits until read() method)
         *    void *decoder_private; (read and write access)
         *
         * in rest of Sound_Sample:
         *    void *opaque;        (this was internal section, above)
         *    const Sound_DecoderInfo *decoder;  (read only)
         *    Sound_AudioInfo desired; (read only, usually not needed here)
         *    Sound_AudioInfo actual;  (please fill this in)
         *    void *buffer;            (offlimits)
         *    Uint32 buffer_size;      (offlimits)
         *    Sound_SampleFlags flags; (set appropriately)
         */
    int (*open)(Sound_Sample *sample, const char *ext);

        /*
         * Clean up. SDL_sound is done with this sample, so the decoder should
         *  clean up any resources it allocated. Anything that wasn't
         *  explicitly allocated by the decoder should be LEFT ALONE, since
         *  the higher-level SDL_sound layer will clean up its own mess.
         */
    void (*close)(Sound_Sample *sample);

        /*
         * Get more data from (sample). The decoder should get a pointer to
         *  the internal structure...
         *
         *   Sound_SampleInternal *internal;
         *   internal = (Sound_SampleInternal *) sample->opaque;
         *
         *  ...and then start decoding. Fill in up to internal->buffer_size
         *  bytes of decoded sound in the space pointed to by
         *  internal->buffer. The encoded data is read in from internal->rw.
         *  Data should be decoded in the format specified during the
         *  decoder's open() method in the sample->actual field. The
         *  conversion to the desired format is done at a higher level.
         *
         * The return value is the number of bytes decoded into
         *  internal->buffer, which can be no more than internal->buffer_size,
         *  but can be less. If it is less, you should set a state flag:
         *
         *   If there's just no more data (end of file, etc), then do:
         *      sample->flags |= SOUND_SAMPLEFLAG_EOF;
         *
         *   If there's an unrecoverable error, then do:
         *      __Sound_SetError(ERR_EXPLAIN_WHAT_WENT_WRONG);
         *      sample->flags |= SOUND_SAMPLEFLAG_ERROR;
         *
         *   If there's more data, but you'd have to block for considerable
         *    amounts of time to get at it, or there's a recoverable error,
         *    then do:
         *      __Sound_SetError(ERR_EXPLAIN_WHAT_WENT_WRONG);
         *      sample->flags |= SOUND_SAMPLEFLAG_EAGAIN;
         *
         * SDL_sound will not call your read() method for any samples with
         *  SOUND_SAMPLEFLAG_EOF or SOUND_SAMPLEFLAG_ERROR set. The
         *  SOUND_SAMPLEFLAG_EAGAIN flag is reset before each call to this
         *  method.
         */
    Uint32 (*read)(Sound_Sample *sample);

        /*
         * Reset the decoding to the beginning of the stream. Nonzero on
         *  success, zero on failure.
         *
         * The purpose of this method is to allow for higher efficiency than
         *  an application could get by just recreating the sample externally;
         *  not only do they not have to reopen the RWops, reallocate buffers,
         *  and potentially pass the data through several rejecting decoders,
         *  but certain decoders will not have to recreate their existing
         *  state (search for metadata, etc) since they already know they
         *  have a valid audio stream with a given set of characteristics.
         *
         * The decoder is responsible for calling seek() on the associated
         *  SDL_RWops. A failing call to seek() should be the ONLY reason that
         *  this method should ever fail!
         */
    int (*rewind)(Sound_Sample *sample);

        /*
         * Reposition the decoding to an arbitrary point. Nonzero on
         *  success, zero on failure.
         *
         * The purpose of this method is to allow for higher efficiency than
         *  an application could get by just rewinding the sample and
         *  decoding to a given point.
         *
         * The decoder is responsible for calling seek() on the associated
         *  SDL_RWops.
         *
         * If there is an error, try to recover so that the next read will
         *  continue as if nothing happened.
         */
    int (*seek)(Sound_Sample *sample, Uint32 ms);
} Sound_DecoderFunctions;


/* A structure to hold a set of audio conversion filters and buffers */
typedef struct Sound_AudioCVT
{
    int    needed;                  /* Set to 1 if conversion possible */
    Uint16 src_format;              /* Source audio format */
    Uint16 dst_format;              /* Target audio format */
    double rate_incr;               /* Rate conversion increment */
    Uint8  *buf;                    /* Buffer to hold entire audio data */
    int    len;                     /* Length of original audio buffer */
    int    len_cvt;                 /* Length of converted audio buffer */
    int    len_mult;                /* buffer must be len*len_mult big */
    double len_ratio;       /* Given len, final size is len*len_ratio */
    void   (*filters[20])(struct Sound_AudioCVT *cvt, Uint16 *format);
    int    filter_index;            /* Current audio conversion function */
} Sound_AudioCVT;

extern SNDDECLSPEC int Sound_BuildAudioCVT(Sound_AudioCVT *cvt,
                        Uint16 src_format, Uint8 src_channels, Uint32 src_rate,
                        Uint16 dst_format, Uint8 dst_channels, Uint32 dst_rate,
                        Uint32 dst_size);

extern SNDDECLSPEC int Sound_ConvertAudio(Sound_AudioCVT *cvt);


typedef void (*MixFunc)(float *dst, void *src, Uint32 frames, float *gains);

typedef struct __SOUND_SAMPLEINTERNAL__
{
    Sound_Sample *next;
    Sound_Sample *prev;
    SDL_RWops *rw;
    const Sound_DecoderFunctions *funcs;
    Sound_AudioCVT sdlcvt;
    Uint8 *buffer;
    Uint32 buffer_size;
    void *decoder_private;
    Sint32 total_time;
    Uint32 mix_position;
    MixFunc mix;
} Sound_SampleInternal;


/* error messages... */
#define ERR_IS_INITIALIZED       "Already initialized"
#define ERR_NOT_INITIALIZED      "Not initialized"
#define ERR_INVALID_ARGUMENT     "Invalid argument"
#define ERR_OUT_OF_MEMORY        "Out of memory"
#define ERR_NOT_SUPPORTED        "Operation not supported"
#define ERR_UNSUPPORTED_FORMAT   "Sound format unsupported"
#define ERR_NOT_A_HANDLE         "Not a file handle"
#define ERR_NO_SUCH_FILE         "No such file"
#define ERR_PAST_EOF             "Past end of file"
#define ERR_IO_ERROR             "I/O error"
#define ERR_COMPRESSION          "(De)compression error"
#define ERR_PREV_ERROR           "Previous decoding already caused an error"
#define ERR_PREV_EOF             "Previous decoding already triggered EOF"
#define ERR_CANNOT_SEEK          "Sample is not seekable"

/*
 * Call this to set the message returned by Sound_GetError().
 *  Please only use the ERR_* constants above, or add new constants to the
 *  above group, but I want these all in one place.
 *
 * Calling this with a NULL argument is a safe no-op.
 */
void __Sound_SetError(const char *err);

/*
 * Call this to convert milliseconds to an actual byte position, based on
 *  audio data characteristics.
 */
Uint32 __Sound_convertMsToBytePos(Sound_AudioInfo *info, Uint32 ms);

/*
 * Use this if you need a cross-platform stricmp().
 */
int __Sound_strcasecmp(const char *x, const char *y);


/* These get used all over for lessening code clutter. */
#define BAIL_MACRO(e, r) { __Sound_SetError(e); return r; }
#define BAIL_IF_MACRO(c, e, r) if (c) { __Sound_SetError(e); return r; }




/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/
/*------------                                              ----------------*/
/*------------  You MUST implement the following functions  ----------------*/
/*------------        if porting to a new platform.         ----------------*/
/*------------     (see platform/unix.c for an example)     ----------------*/
/*------------                                              ----------------*/
/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/


/* (None, right now.)  */


#ifdef __cplusplus
extern "C"
#endif

#endif /* defined _INCLUDE_SDL_SOUND_INTERNAL_H_ */

/* end of SDL_sound_internal.h ... */
