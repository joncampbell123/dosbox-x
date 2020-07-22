/*
 *  Modified SDL Sound API implementation
 *  -------------------------------------
 *  This file implements the API, the documentation for which can
 *  be found in SDL_sound.h.  This API has been changed from its
 *  original implementation as follows:
 *    - Cut down in size; most notably exclusion of the conversion routines
 *    - Small bug fixes and warnings cleaned up
 *    - Elimination of intermediate buffers, allowing direct decoding
 *    - Moved from sample-based logic to frame-based (channel-agnostic)
 *
 *  Copyright (C) 2020       The DOSBox Team
 *  Copyright (C) 2018-2019  Kevin R. Croft <krcroft@gmail.com>
 *  Copyright (C) 2001-2017  Ryan C. Gordon <icculus@icculus.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <stdio.h>
#include <ctype.h>

#if HAVE_CONFIG_H
#  include <config.h>
#endif

#include <SDL.h>
#include <SDL_thread.h>
#include "SDL_sound.h"

#define __SDL_SOUND_INTERNAL__
#include "SDL_sound_internal.h"

typedef struct
{
    int available;
    const Sound_DecoderFunctions *funcs;
} decoder_element;

/* Supported decoder drivers... */
extern const Sound_DecoderFunctions __Sound_DecoderFunctions_FLAC;
extern const Sound_DecoderFunctions __Sound_DecoderFunctions_MP3;
#ifdef USE_OPUS
extern const Sound_DecoderFunctions __Sound_DecoderFunctions_OPUS;
#endif
extern const Sound_DecoderFunctions __Sound_DecoderFunctions_VORBIS;
extern const Sound_DecoderFunctions __Sound_DecoderFunctions_WAV;

static decoder_element decoders[] =
{
    { 0, &__Sound_DecoderFunctions_FLAC },
    { 0, &__Sound_DecoderFunctions_MP3 },
#ifdef USE_OPUS
    { 0, &__Sound_DecoderFunctions_OPUS },
#endif
    { 0, &__Sound_DecoderFunctions_VORBIS },
    { 0, &__Sound_DecoderFunctions_WAV },
    { 0, NULL }
};



/* General SDL_sound state ... */

typedef struct __SOUND_ERRMSGTYPE__
{
    Uint32 tid;
    int error_available;
    char error_string[128];
    struct __SOUND_ERRMSGTYPE__ *next;
} ErrMsg;

static ErrMsg *error_msgs = NULL;
static SDL_mutex *errorlist_mutex = NULL;

static Sound_Sample *sample_list = NULL;  /* this is a linked list. */
static SDL_mutex *samplelist_mutex = NULL;

static const Sound_DecoderInfo **available_decoders = NULL;
static int initialized = 0;


/* functions ... */

void Sound_GetLinkedVersion(Sound_Version *ver)
{
    if (ver != NULL)
    {
        ver->major = SOUND_VER_MAJOR;
        ver->minor = SOUND_VER_MINOR;
        ver->patch = SOUND_VER_PATCH;
    } /* if */
} /* Sound_GetLinkedVersion */


int Sound_Init(void)
{
    size_t i;
    size_t pos = 0;
    size_t total = sizeof (decoders) / sizeof (decoders[0]);
    BAIL_IF_MACRO(initialized, ERR_IS_INITIALIZED, 0);

    sample_list = NULL;
    error_msgs = NULL;

    available_decoders = (const Sound_DecoderInfo **)
                            malloc((total) * sizeof (Sound_DecoderInfo *));
    BAIL_IF_MACRO(available_decoders == NULL, ERR_OUT_OF_MEMORY, 0);

    SDL_InitSubSystem(SDL_INIT_AUDIO);

    errorlist_mutex = SDL_CreateMutex();
    samplelist_mutex = SDL_CreateMutex();

    for (i = 0; decoders[i].funcs != NULL; i++)
    {
        decoders[i].available = decoders[i].funcs->init();
        if (decoders[i].available)
        {
            available_decoders[pos] = &(decoders[i].funcs->info);
            pos++;
        } /* if */
    } /* for */

    available_decoders[pos] = NULL;

    initialized = 1;
    return(1);
} /* Sound_Init */


int Sound_Quit(void)
{
    ErrMsg *err;
    ErrMsg *nexterr = NULL;
    size_t i;

    BAIL_IF_MACRO(!initialized, ERR_NOT_INITIALIZED, 0);

    while (((volatile Sound_Sample *) sample_list) != NULL)
    {
        Sound_Sample *sample = sample_list;
        Sound_FreeSample(sample); /* Updates sample_list. */
    }

    initialized = 0;

    SDL_DestroyMutex(samplelist_mutex);
    samplelist_mutex = NULL;
    sample_list = NULL;

    for (i = 0; decoders[i].funcs != NULL; i++)
    {
        if (decoders[i].available)
        {
            decoders[i].funcs->quit();
            decoders[i].available = 0;
        } /* if */
    } /* for */

    if (available_decoders != NULL)
        free((void *) available_decoders);
    available_decoders = NULL;

    /* clean up error state for each thread... */
    SDL_LockMutex(errorlist_mutex);
    for (err = error_msgs; err != NULL; err = nexterr)
    {
        nexterr = err->next;
        free(err);
    } /* for */
    error_msgs = NULL;
    SDL_UnlockMutex(errorlist_mutex);
    SDL_DestroyMutex(errorlist_mutex);
    errorlist_mutex = NULL;

    return(1);
} /* Sound_Quit */


const Sound_DecoderInfo **Sound_AvailableDecoders(void)
{
    return(available_decoders);  /* READ. ONLY. */
} /* Sound_AvailableDecoders */


static ErrMsg *findErrorForCurrentThread(void)
{
    ErrMsg *i;
    Uint32 tid;

    if (error_msgs != NULL)
    {
        tid = SDL_ThreadID();

        SDL_LockMutex(errorlist_mutex);
        for (i = error_msgs; i != NULL; i = i->next)
        {
            if (i->tid == tid)
            {
                SDL_UnlockMutex(errorlist_mutex);
                return(i);
            } /* if */
        } /* for */
        SDL_UnlockMutex(errorlist_mutex);
    } /* if */

    return(NULL);   /* no error available. */
} /* findErrorForCurrentThread */


const char *Sound_GetError(void)
{
    const char *retval = NULL;
    ErrMsg *err;

    if (!initialized)
        return(ERR_NOT_INITIALIZED);

    err = findErrorForCurrentThread();
    if ((err != NULL) && (err->error_available))
    {
        retval = err->error_string;
        err->error_available = 0;
    } /* if */

    return(retval);
} /* Sound_GetError */


void Sound_ClearError(void)
{
    ErrMsg *err;

    if (!initialized)
        return;

    err = findErrorForCurrentThread();
    if (err != NULL)
        err->error_available = 0;
} /* Sound_ClearError */


/*
 * This is declared in the internal header.
 */
void __Sound_SetError(const char *str)
{
    ErrMsg *err;

    if (str == NULL)
        return;

    SNDDBG(("__Sound_SetError(\"%s\");%s\n", str,
              (initialized) ? "" : " [NOT INITIALIZED!]"));

    if (!initialized)
        return;

    err = findErrorForCurrentThread();
    if (err == NULL)
    {
        err = (ErrMsg *) malloc(sizeof (ErrMsg));
        if (err == NULL)
            return;   /* uhh...? */

        memset((void *) err, '\0', sizeof (ErrMsg));
        err->tid = SDL_ThreadID();

        SDL_LockMutex(errorlist_mutex);
        err->next = error_msgs;
        error_msgs = err;
        SDL_UnlockMutex(errorlist_mutex);
    } /* if */

    err->error_available = 1;
    snprintf(err->error_string, sizeof (err->error_string), "%s", str);
} /* __Sound_SetError */


Uint32 __Sound_convertMsToBytePos(Sound_AudioInfo *info, Uint32 ms)
{
    /* "frames" == "sample frames" */
    float frames_per_ms = ((float) info->rate) / 1000.0f;
    Uint32 frame_offset = (Uint32) (frames_per_ms * ((float) ms));
    Uint32 frame_size = (Uint32) ((info->format & 0xFF) / 8) * info->channels;
    return(frame_offset * frame_size);
} /* __Sound_convertMsToBytePos */


/*
 * -ansi and -pedantic flags prevent use of strcasecmp() on Linux, and
 *  I honestly don't want to mess around with figuring out if a given
 *  platform has "strcasecmp", "stricmp", or
 *  "compare_two_damned_strings_case_insensitive", which I hear is in the
 *  next release of Carbon.  :)  This is exported so decoders may use it if
 *  they like.
 */
int __Sound_strcasecmp(const char *x, const char *y)
{
    int ux, uy;

    if (x == y)  /* same pointer? Both NULL? */
        return(0);

    if (x == NULL)
        return(-1);

    if (y == NULL)
        return(1);
       
    do
    {
        ux = toupper((int) *x);
        uy = toupper((int) *y);
        if (ux > uy)
            return(1);
        else if (ux < uy)
            return(-1);
        x++;
        y++;
    } while ((ux) && (uy));

    return(0);
} /* __Sound_strcasecmp */


/*
 * Allocate a Sound_Sample, and fill in most of its fields. Those that need
 *  to be filled in later, by a decoder, will be initialized to zero.
 */
static Sound_Sample *alloc_sample(SDL_RWops *rw, Sound_AudioInfo *desired)
{
    /*
     * !!! FIXME: We're going to need to pool samples, since the mixer
     * !!! FIXME:  might be allocating tons of these on a regular basis.
     */
    Sound_Sample *retval = NULL;
    Sound_Sample *sample = (Sound_Sample *)malloc(sizeof (Sound_Sample));
    Sound_SampleInternal *internal = (Sound_SampleInternal *)malloc(sizeof (Sound_SampleInternal));
    if (sample && internal) {
        memset(sample, '\0', sizeof (Sound_Sample));
        memset(internal, '\0', sizeof (Sound_SampleInternal));
        if (desired != NULL) {
            memcpy(&sample->desired, desired, sizeof (Sound_AudioInfo));
        }
        internal->rw = rw;
        sample->opaque = internal;
        retval = sample;
    } else {
        __Sound_SetError(ERR_OUT_OF_MEMORY);
        free(sample);
        free(internal);
    }
    return retval;
} /* alloc_sample */


#if (defined DEBUG_CHATTER)
static __inline__ const char *fmt_to_str(Uint16 fmt)
{
    switch(fmt)
    {
        case AUDIO_U8:
            return("U8");
        case AUDIO_S8:
            return("S8");
        case AUDIO_U16LSB:
            return("U16LSB");
        case AUDIO_S16LSB:
            return("S16LSB");
        case AUDIO_U16MSB:
            return("U16MSB");
        case AUDIO_S16MSB:
            return("S16MSB");
    } /* switch */

    return("Unknown");
} /* fmt_to_str */
#endif


/*
 * The bulk of the Sound_NewSample() work is done here...
 *  Ask the specified decoder to handle the data in (rw), and if
 *  so, construct the Sound_Sample. Otherwise, try to wind (rw)'s stream
 *  back to where it was, and return false.
 */
static int init_sample(const Sound_DecoderFunctions *funcs,
                        Sound_Sample *sample, const char *ext,
                        Sound_AudioInfo *_desired)
{
    Sound_SampleInternal *internal = (Sound_SampleInternal *) sample->opaque;
    Sound_AudioInfo desired;
    const Sint64 pos = SDL_RWtell(internal->rw);

        /* fill in the funcs for this decoder... */
    sample->decoder = &funcs->info;
    internal->funcs = funcs;
    if (!funcs->open(sample, ext))
    {
        SDL_RWseek(internal->rw, (int)pos, SEEK_SET);  /* set for next try... */
        return(0);
    } /* if */

    /* success; we've got a decoder! */

    memcpy(&desired, (_desired != NULL) ? _desired : &sample->actual,
            sizeof (Sound_AudioInfo));

    if (desired.format == 0)
        desired.format = sample->actual.format;
    if (desired.channels == 0)
        desired.channels = sample->actual.channels;
    if (desired.rate == 0)
        desired.rate = sample->actual.rate;


        /* these pointers are all one and the same. */
    memcpy(&sample->desired, &desired, sizeof (Sound_AudioInfo));

    /* Prepend our new Sound_Sample to the sample_list... */
    SDL_LockMutex(samplelist_mutex);
    internal->next = sample_list;
    if (sample_list != NULL)
        ((Sound_SampleInternal *) sample_list->opaque)->prev = sample;
    sample_list = sample;
    SDL_UnlockMutex(samplelist_mutex);

    SNDDBG(("New sample DESIRED format: %s format, %d rate, %d channels.\n",
            fmt_to_str(sample->desired.format),
            sample->desired.rate,
            sample->desired.channels));

    SNDDBG(("New sample ACTUAL format: %s format, %d rate, %d channels.\n",
            fmt_to_str(sample->actual.format),
            sample->actual.rate,
            sample->actual.channels));
    return(1);
} /* init_sample */


Sound_Sample *Sound_NewSample(SDL_RWops *rw, const char *ext,
                              Sound_AudioInfo *desired)
{
    Sound_Sample *retval;
    decoder_element *decoder;

    /* sanity checks. */
    BAIL_IF_MACRO(!initialized, ERR_NOT_INITIALIZED, NULL);
    BAIL_IF_MACRO(rw == NULL, ERR_INVALID_ARGUMENT, NULL);

    retval = alloc_sample(rw, desired);
    if (!retval)
        return(NULL);  /* alloc_sample() sets error message... */

    if (ext != NULL)
    {
        for (decoder = &decoders[0]; decoder->funcs != NULL; decoder++)
        {
            if (decoder->available)
            {
                const char **decoderExt = decoder->funcs->info.extensions;
                while (*decoderExt)
                {
                    if (__Sound_strcasecmp(*decoderExt, ext) == 0)
                    {
                        if (init_sample(decoder->funcs, retval, ext, desired))
                            return(retval);
                        break;  /* done with this decoder either way. */
                    } /* if */
                    decoderExt++;
                } /* while */
            } /* if */
        } /* for */
    } /* if */

    /* no direct extension match? Try everything we've got... */
    for (decoder = &decoders[0]; decoder->funcs != NULL; decoder++)
    {
        if (decoder->available)
        {
            int should_try = 1;
            const char **decoderExt = decoder->funcs->info.extensions;

                /* skip if we would have tried decoder above... */
            while (*decoderExt)
            {
                if (__Sound_strcasecmp(*decoderExt, ext) == 0)
                {
                    should_try = 0;
                    break;
                } /* if */
                decoderExt++;
            } /* while */

            if (should_try)
            {
                if (init_sample(decoder->funcs, retval, ext, desired))
                    return(retval);
            } /* if */
        } /* if */
    } /* for */

    /* nothing could handle the sound data... */
    free(retval->opaque);
    free(retval);
    SDL_RWclose(rw);
    __Sound_SetError(ERR_UNSUPPORTED_FORMAT);
    return(NULL);
} /* Sound_NewSample */


Sound_Sample *Sound_NewSampleFromFile(const char *filename,
                                      Sound_AudioInfo *desired)
{
    const char *ext;
    SDL_RWops *rw;

    BAIL_IF_MACRO(!initialized, ERR_NOT_INITIALIZED, NULL);
    BAIL_IF_MACRO(filename == NULL, ERR_INVALID_ARGUMENT, NULL);

    ext = strrchr(filename, '.');


    SNDDBG(("Sound_NewSampleFromFile ext = `%s`", ext));

    rw = SDL_RWFromFile(filename, "rb");
    /* !!! FIXME: rw = RWops_FromFile(filename, "rb");*/
    BAIL_IF_MACRO(rw == NULL, SDL_GetError(), NULL);

    if (ext != NULL)
        ext++;

    return(Sound_NewSample(rw, ext, desired));
} /* Sound_NewSampleFromFile */

void Sound_FreeSample(Sound_Sample *sample)
{
    Sound_SampleInternal *internal;

    if (!initialized)
    {
        __Sound_SetError(ERR_NOT_INITIALIZED);
        return;
    } /* if */

    if (sample == NULL)
    {
        __Sound_SetError(ERR_INVALID_ARGUMENT);
        return;
    } /* if */

    internal = (Sound_SampleInternal *) sample->opaque;

    SDL_LockMutex(samplelist_mutex);

    /* update the sample_list... */
    if (internal->prev != NULL)
    {
        Sound_SampleInternal *prevInternal;
        prevInternal = (Sound_SampleInternal *) internal->prev->opaque;
        prevInternal->next = internal->next;
    } /* if */
    else
    {
        assert(sample_list == sample);
        sample_list = internal->next;
    } /* else */

    if (internal->next != NULL)
    {
        Sound_SampleInternal *nextInternal;
        nextInternal = (Sound_SampleInternal *) internal->next->opaque;
        nextInternal->prev = internal->prev;
    } /* if */

    SDL_UnlockMutex(samplelist_mutex);

    /* nuke it... */
    internal->funcs->close(sample);

    if (internal->rw != NULL)  /* this condition is a "just in case" thing. */
        SDL_RWclose(internal->rw);

    free(internal);
    free(sample);
} /* Sound_FreeSample */

Uint32 Sound_Decode_Direct(Sound_Sample *sample, void* buffer, Uint32 desired_frames)
{
    Sound_SampleInternal *internal = NULL;

        /* a boatload of sanity checks... */
    BAIL_IF_MACRO(!initialized, ERR_NOT_INITIALIZED, 0);
    BAIL_IF_MACRO(sample == NULL, ERR_INVALID_ARGUMENT, 0);
    BAIL_IF_MACRO(sample->flags & SOUND_SAMPLEFLAG_ERROR, ERR_PREV_ERROR, 0);
    BAIL_IF_MACRO(sample->flags & SOUND_SAMPLEFLAG_EOF, ERR_PREV_EOF, 0);

    internal = (Sound_SampleInternal *) sample->opaque;

        /* reset EAGAIN. Decoder can flip it back on if it needs to. */
    sample->flags &= ~SOUND_SAMPLEFLAG_EAGAIN;
    return internal->funcs->read(sample, buffer, desired_frames);
} /* Sound_Decode */


int Sound_Rewind(Sound_Sample *sample)
{
    Sound_SampleInternal *internal;
    BAIL_IF_MACRO(!initialized, ERR_NOT_INITIALIZED, 0);

    internal = (Sound_SampleInternal *) sample->opaque;
    if (!internal->funcs->rewind(sample))
    {
        sample->flags |= SOUND_SAMPLEFLAG_ERROR;
        return(0);
    } /* if */

    sample->flags &= ~SOUND_SAMPLEFLAG_EAGAIN;
    sample->flags &= ~SOUND_SAMPLEFLAG_ERROR;
    sample->flags &= ~SOUND_SAMPLEFLAG_EOF;

    return(1);
} /* Sound_Rewind */


int Sound_Seek(Sound_Sample *sample, Uint32 ms)
{
    Sound_SampleInternal *internal;

    BAIL_IF_MACRO(!initialized, ERR_NOT_INITIALIZED, 0);
    if (!(sample->flags & SOUND_SAMPLEFLAG_CANSEEK))
        BAIL_MACRO(ERR_CANNOT_SEEK, 0);

    internal = (Sound_SampleInternal *) sample->opaque;
    BAIL_IF_MACRO(!internal->funcs->seek(sample, ms), NULL, 0);

    sample->flags &= ~SOUND_SAMPLEFLAG_EAGAIN;
    sample->flags &= ~SOUND_SAMPLEFLAG_ERROR;
    sample->flags &= ~SOUND_SAMPLEFLAG_EOF;

    return(1);
} /* Sound_Rewind */


Sint32 Sound_GetDuration(Sound_Sample *sample)
{
    Sound_SampleInternal *internal;
    BAIL_IF_MACRO(!initialized, ERR_NOT_INITIALIZED, -1);
    internal = (Sound_SampleInternal *) sample->opaque;
    return(internal->total_time);
} /* Sound_GetDuration */

/* end of SDL_sound.c ... */
