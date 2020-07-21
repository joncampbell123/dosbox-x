/*
 *  DOSBox-X MP3 decoder API implementation
 *  ---------------------------------------
 *  It makes use of the dr_wav library by David Reid (mackron@gmail.com)
 *  Source links:
 *   - dr_libs: https://github.com/mackron/dr_libs (source)
 *   - dr_wav: http://mackron.github.io/dr_wav.html (website)
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

#if HAVE_CONFIG_H
#  include <config.h>
#endif

#include <math.h> /* llroundf */

#include "SDL_sound.h"
#define __SDL_SOUND_INTERNAL__
#include "SDL_sound_internal.h"

/* Map dr_wav's memory routines to SDL's */
#define DRWAV_FREE(p)                   SDL_free((p))
#define DRWAV_MALLOC(sz)                SDL_malloc((sz))
#define DRWAV_REALLOC(p, sz)            SDL_realloc((p), (sz))
#define DRWAV_ZERO_MEMORY(p, sz)        SDL_memset((p), 0, (sz))
#define DRWAV_COPY_MEMORY(dst, src, sz) SDL_memcpy((dst), (src), (sz))

#define DR_WAV_NO_STDIO
#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

static size_t wav_read(void* pUserData, void* pBufferOut, size_t bytesToRead)
{
    Uint8 *ptr = (Uint8 *) pBufferOut;
    Sound_Sample *sample = (Sound_Sample *) pUserData;
    Sound_SampleInternal *internal = (Sound_SampleInternal *) sample->opaque;
    SDL_RWops *rwops = internal->rw;
    size_t bytes_read = 0;

    while (bytes_read < bytesToRead) {
        const size_t rc = SDL_RWread(rwops, ptr, 1, (int)(bytesToRead - bytes_read));
        if (rc == 0) {
            sample->flags |= SOUND_SAMPLEFLAG_EOF;
            break;
        } /* if */
        bytes_read += rc;
        ptr += rc;
    } /* while */

    return bytes_read;
} /* wav_read */

static drwav_bool32 wav_seek(void* pUserData, int offset, drwav_seek_origin origin)
{
    const int whence = (origin == drwav_seek_origin_start) ? RW_SEEK_SET : RW_SEEK_CUR;
    Sound_Sample *sample = (Sound_Sample *) pUserData;
    Sound_SampleInternal *internal = (Sound_SampleInternal *) sample->opaque;
    return (SDL_RWseek(internal->rw, offset, whence) != -1) ? DRWAV_TRUE : DRWAV_FALSE;
} /* wav_seek */


static int WAV_init(void)
{
    return 1;  /* always succeeds. */
} /* WAV_init */


static void WAV_quit(void)
{
    /* it's a no-op. */
} /* WAV_quit */

static void WAV_close(Sound_Sample *sample)
{
    Sound_SampleInternal *internal = (Sound_SampleInternal *) sample->opaque;
    drwav *dr = (drwav *) internal->decoder_private;
    if (dr != NULL) {
        (void) drwav_uninit(dr);
        SDL_free(dr);
        internal->decoder_private = NULL;
    }
    return;
} /* WAV_close */

static int WAV_open(Sound_Sample *sample, const char *ext)
{
    (void) ext; // deliberately unused, but present for API compliance
    Sound_SampleInternal *internal = (Sound_SampleInternal *) sample->opaque;
    drwav* dr = (drwav*)SDL_malloc(sizeof(drwav));
    drwav_result result = drwav_init_ex(dr, wav_read, wav_seek, NULL, sample, NULL, 0, NULL);
    internal->decoder_private = dr;

    if (result == DRWAV_TRUE) {
        SNDDBG(("WAV: Codec accepted the data stream.\n"));
        sample->flags = SOUND_SAMPLEFLAG_CANSEEK;
        sample->actual.rate = dr->sampleRate;
        sample->actual.format = AUDIO_S16SYS;
        sample->actual.channels = (Uint8)(dr->channels);

        const Uint64 frames = (Uint64) dr->totalPCMFrameCount;
        if (frames == 0) {
            internal->total_time = -1;
        }
        else {
            const Uint32 rate = (Uint32) dr->sampleRate;
            internal->total_time = ( (Sint32)frames / rate) * 1000;
            internal->total_time += ((frames % rate) * 1000) / rate;
        }  /* else */

    } /* if result != DRWAV_TRUE */
    else {
        SNDDBG(("WAV: Codec could not parse the data stream.\n"));
        WAV_close(sample);
    }
    return result;
} /* WAV_open */


static Uint32 WAV_read(Sound_Sample *sample, void* buffer, Uint32 desired_frames)
{
    Sound_SampleInternal *internal = (Sound_SampleInternal *) sample->opaque;
    drwav *dr = (drwav *) internal->decoder_private;
    const drwav_uint64 frames_read = drwav_read_pcm_frames_s16(dr,
                                                               desired_frames,
                                                               (drwav_int16 *) buffer);
    return (Uint32)frames_read;
} /* WAV_read */


static int WAV_rewind(Sound_Sample *sample)
{
    Sound_SampleInternal *internal = (Sound_SampleInternal *) sample->opaque;
    drwav *dr = (drwav *) internal->decoder_private;
    return (drwav_seek_to_pcm_frame(dr, 0) == DRWAV_TRUE);
} /* WAV_rewind */

static int WAV_seek(Sound_Sample *sample, Uint32 ms)
{
    Sound_SampleInternal *internal = (Sound_SampleInternal *) sample->opaque;
    drwav *dr = (drwav *) internal->decoder_private;
    const float frames_per_ms = ((float) sample->actual.rate) / 1000.0f;
    const drwav_uint64 frame_offset = llroundf(frames_per_ms * ms);
    return (drwav_seek_to_pcm_frame(dr, frame_offset) == DRWAV_TRUE);
} /* WAV_seek */

static const char *extensions_wav[] = { "WAV", "W64", NULL };

const Sound_DecoderFunctions __Sound_DecoderFunctions_WAV =
{
    {
        extensions_wav,
        "WAV Audio Codec",
        "The DOSBox-X project"
    },

    WAV_init,       /*   init() method */
    WAV_quit,       /*   quit() method */
    WAV_open,       /*   open() method */
    WAV_close,      /*  close() method */
    WAV_read,       /*   read() method */
    WAV_rewind,     /* rewind() method */
    WAV_seek        /*   seek() method */
};
/* end of wav.c ... */
