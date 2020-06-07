/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2001-2017  Ryan C. Gordon <icculus@icculus.org>
 *  Copyright (C) 2018-2019  Kevin R. Croft <krcroft@gmail.com>
 *  Copyright (C) 2020-2020  The DOSBox-X project
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

/*
 *  DOSBox-X MP3 decoder API implementation
 *  -------------------------------------
 *  This decoder makes use of the dr_mp3 library by David Reid (mackron@gmail.com)
 *    - dr_libs: https://github.com/mackron/dr_libs (source)
 *    - dr_mp3:  http://mackron.github.io/dr_mp3.html (website)
 */
#include <support.h>

//#include "mp3_seek_table.h"
#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"

#include "SDL_sound.h"
#define __SDL_SOUND_INTERNAL__
#include "SDL_sound_internal.h"

static constexpr char fast_seek_filename[] = "fastseek.lut";

static size_t mp3_read(void* const pUserData, void* const pBufferOut, const size_t bytesToRead)
{
    Uint8* ptr = static_cast<Uint8*>(pBufferOut);
    Sound_Sample* const sample = static_cast<Sound_Sample*>(pUserData);
    const Sound_SampleInternal* const internal = static_cast<const Sound_SampleInternal*>(sample->opaque);
    SDL_RWops* rwops = internal->rw;
    size_t bytes_read = 0;

    while (bytes_read < bytesToRead)
    {
        const size_t rc = SDL_RWread(rwops, ptr, 1, bytesToRead - bytes_read);
        if (rc == 0) {
            sample->flags |= SOUND_SAMPLEFLAG_EOF;
            break;
        } /* if */
        bytes_read += rc;
        ptr += rc;
    } /* while */

    return bytes_read;
} /* mp3_read */

static drmp3_bool32 mp3_seek(void* const pUserData, const Sint32 offset, const drmp3_seek_origin origin)
{
    const Sint32 whence = (origin == drmp3_seek_origin_start) ? RW_SEEK_SET : RW_SEEK_CUR;
    Sound_Sample* const sample = static_cast<Sound_Sample*>(pUserData);
    Sound_SampleInternal* const internal = static_cast<Sound_SampleInternal*>(sample->opaque);
    return (SDL_RWseek(internal->rw, offset, whence) != -1) ? DRMP3_TRUE : DRMP3_FALSE;
} /* mp3_seek */


static Sint32 MP3_init(void)
{
    return 1;  /* always succeeds. */
} /* MP3_init */


static void MP3_quit(void)
{
    /* it's a no-op. */
} /* MP3_quit */

static void MP3_close(Sound_Sample* const sample)
{
    Sound_SampleInternal* const internal = static_cast<Sound_SampleInternal*>(sample->opaque);
    mp3_t* p_mp3 = static_cast<mp3_t*>(internal->decoder_private);
    if (p_mp3) {
        SDL_free(p_mp3->p_dr);
        SDL_free(p_mp3);
    }
    internal->decoder_private = nullptr;
} /* MP3_close */

static Uint32 MP3_read(Sound_Sample* const sample, void* buffer, Uint32 desired_frames)
{
    Sound_SampleInternal* const internal = static_cast<Sound_SampleInternal*>(sample->opaque);
    mp3_t* p_mp3 = static_cast<mp3_t*>(internal->decoder_private);

    // LOG_MSG("read-while: num_frames: %u", num_frames);
    return static_cast<Uint32>(drmp3_read_pcm_frames_s16(p_mp3->p_dr,
                                                         static_cast<drmp3_uint64>(desired_frames),
                                                         static_cast<drmp3_int16*>(buffer)));
} /* MP3_read */

static int32_t MP3_open(Sound_Sample* const sample, const char* const ext)
{
    (void) ext; // deliberately unused
    Sound_SampleInternal* const internal = static_cast<Sound_SampleInternal*>(sample->opaque);
    bool result = false; // assume failure until proven otherwise
    mp3_t* p_mp3 = (mp3_t*) SDL_calloc(1, sizeof (mp3_t));
    if (p_mp3) {
        p_mp3->p_dr = (drmp3*) SDL_calloc(1, sizeof (drmp3));
        if (p_mp3->p_dr) {
            if (drmp3_init(p_mp3->p_dr, mp3_read, mp3_seek, sample, nullptr) == DRMP3_TRUE) {
                SNDDBG(("MP3: Accepting data stream.\n"));
                sample->flags = SOUND_SAMPLEFLAG_CANSEEK;
                sample->actual.channels = static_cast<uint8_t>(p_mp3->p_dr->channels);
                sample->actual.rate = p_mp3->p_dr->sampleRate;
                sample->actual.format = AUDIO_S16SYS;  // native byte-order based on architecture

                // frame count is agnostic of sample size and number of channels
                const uint64_t num_frames =
                    populate_seek_points(internal->rw, p_mp3, fast_seek_filename, result);

                // total_time needs milliseconds
                internal->total_time = (num_frames != 0) ? 
                    static_cast<int32_t>(ceil_udivide(num_frames * 1000u, sample->actual.rate))
                    : -1;
            }
        }
    }

    // Assign our internal decoder to the mp3 object we've just populated
    internal->decoder_private = p_mp3;

    // if anything went wrong then tear down our private structure
    if (!result) {
        SNDDBG(("MP3: Failed to open the data stream.\n"));
        MP3_close(sample);
    }

    return static_cast<int32_t>(result);
} /* MP3_open */

static Sint32 MP3_rewind(Sound_Sample* const sample)
{
    Sound_SampleInternal* const internal = static_cast<Sound_SampleInternal*>(sample->opaque);
    mp3_t* p_mp3 = static_cast<mp3_t*>(internal->decoder_private);
    return (drmp3_seek_to_start_of_stream(p_mp3->p_dr) == DRMP3_TRUE);
} /* MP3_rewind */

static Sint32 MP3_seek(Sound_Sample* const sample, const Uint32 ms)
{
    Sound_SampleInternal* const internal = static_cast<Sound_SampleInternal*>(sample->opaque);
    mp3_t* p_mp3 = static_cast<mp3_t*>(internal->decoder_private);
    const uint64_t sample_rate = sample->actual.rate;
    const drmp3_uint64 pcm_frame = ceil_udivide(sample_rate * ms, 1000u);
    const drmp3_bool32 result = drmp3_seek_to_pcm_frame(p_mp3->p_dr, pcm_frame);
    return (result == DRMP3_TRUE);
} /* MP3_seek */

/* dr_mp3 will play layer 1 and 2 files, too */
static const char* extensions_mp3[] = { "MP3", "MP2", "MP1", nullptr };

extern const Sound_DecoderFunctions __Sound_DecoderFunctions_MP3 = {
    {
        extensions_mp3,
        "MPEG-1 Audio Layer I-III",
        "The DOSBox-X project"
    },

    MP3_init,       /*   init() method */
    MP3_quit,       /*   quit() method */
    MP3_open,       /*   open() method */
    MP3_close,      /*  close() method */
    MP3_read,       /*   read() method */
    MP3_rewind,     /* rewind() method */
    MP3_seek        /*   seek() method */
};
/* end of SDL_sound_mp3.c ... */
