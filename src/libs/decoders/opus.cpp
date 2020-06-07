/*
 *  DOSBox Opus decoder API implementation
 *  --------------------------------------
 *  This decoders makes use of:
 *    - libopusfile, for .opus file handing and frame decoding
 *
 *  Source links
 *    - opusfile:   https://github.com/xiph/opusfile
 *    - opus-tools: https://github.com/xiph/opus-tools
 *
 * Documentation references
 *    - Ogg Opus:  https://www.opus-codec.org/docs
 *    - OpusFile:  https://mf4.xiph.org/jenkins/view/opus/job/opusfile-unix/ws/doc/html/index.html
 *
 *  Copyright (C) 2020       The DOSBox Team
 *  Copyright (C) 2018-2019  Kevin R. Croft <krcroft@gmail.com>
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

// #define DEBUG_CHATTER 1

#if HAVE_CONFIG_H
#  include <config.h>
#endif

#include <cassert>
#include <opusfile.h>
#include <SDL.h>

#include "support.h"

#include "SDL_sound.h"
#define __SDL_SOUND_INTERNAL__
#include "SDL_sound_internal.h"

// Opus's internal sampling rates to which all encoded streams get resampled
#define OPUS_SAMPLE_RATE        48000u
#define OPUS_SAMPLE_RATE_PER_MS    48u

static int32_t opus_init(void)
{
    SNDDBG(("Opus init:              done\n"));
    return 1; /* always succeeds. */
} /* opus_init */


static void opus_quit(void)
{
    SNDDBG(("Opus quit:              done\n"));
} // no-op


/*
 * Read-Write Ops Read Callback Wrapper
 * ------------------------------------
 * OPUS: typedef int(*op_read_func)
 *       void*       _stream  --> The stream to read from
 *       unsigned char* _ptr  --> The buffer to store the data in
 *       int         _nbytes  --> The maximum number of bytes to read.
 *    Returns: The number of bytes successfully read, or a negative value on error.
 *
 * SDL: size_t SDL_RWread
 *      struct SDL_RWops* context --> a pointer to an SDL_RWops structure
 *      void*             ptr     --> a pointer to a buffer to read data into
 *      size_t            size    --> the size of each object to read, in bytes
 *      size_t            maxnum  --> the maximum number of objects to be read
 */
static int RWops_opus_read(void * stream, uint8_t * buffer, int32_t requested_bytes)
{
    // Guard against invalid inputs and the no-op scenario
    assertm(stream && buffer, "OPUS: Inputs are not initialized");
    if (requested_bytes <= 0)
        return 0;
    
    uint8_t *buf_pos = buffer;
    int32_t bytes_read = 0;
    auto *sample = static_cast<Sound_Sample*>(stream);
    while (bytes_read < requested_bytes) {
        const size_t rc = SDL_RWread(static_cast<SDL_RWops*>(stream),
                                     static_cast<void*>(buf_pos),
                                     1,
                                     static_cast<size_t>(requested_bytes - bytes_read));

        if (rc == 0) {
            sample->flags |= SOUND_SAMPLEFLAG_EOF;
            break;
        }
        buf_pos += rc;
        bytes_read += rc;
    } /* while */

    return bytes_read;
} /* RWops_opus_read */


/*
 * Read-Write Ops Seek Callback Wrapper
 * ------------------------------------
 *
 * OPUS: typedef int(* op_seek_func)
 *       void*      _stream, --> The stream to seek in
 *       opus_int64 _offset, --> Sets the position indicator for _stream in bytes
 *       int _whence         --> If whence is set to SEEK_SET, SEEK_CUR, or SEEK_END,
 *                               the offset is relative to the start of the stream,
 *                               the current position indicator, or end-of-file,
 *                               respectively
 *    Returns: 0  Success, or -1 if seeking is not supported or an error occurred.
 *      define  SEEK_SET    0
 *      define  SEEK_CUR    1
 *      define  SEEK_END    2
 *
 * SDL: Sint64 SDL_RWseek
 *      SDL_RWops* context --> a pointer to an SDL_RWops structure
 *      Sint64     offset, --> offset, in bytes
 *      Sint32     whence  --> an offset in bytes, relative to whence location; can be negative
 *   Returns the final offset in the data stream after the seek or -1 on error.
 *      RW_SEEK_SET   0
 *      RW_SEEK_CUR   1
 *      RW_SEEK_END   2
 */
static int32_t RWops_opus_seek(void * stream, const opus_int64 offset, const int32_t whence)
{
    // Guard against invalid inputs
    assertm(stream, "OPUS: Input is not initialized");
    assertm(whence == SEEK_SET || whence == SEEK_CUR || whence == SEEK_END,
            "OPUS: The position from where to seek is invalid");

    const int64_t offset_after_seek = SDL_RWseek(static_cast<SDL_RWops*>(stream),
                                                 static_cast<int32_t>(offset),
                                                 whence);
    SNDDBG(("Opus ops seek:          "
            "{requested offset: %ld, seeked offset: %ld}\n",
            offset, offset_after_seek));
    return (offset_after_seek != -1 ? 0 : -1);
} /* RWops_opus_seek */


/*
 * Read-Write Ops Close Callback Wrapper
 * -------------------------------------
 * OPUS: typedef int(* op_close_func)(void *_stream)
 * SDL:  Sint32 SDL_RWclose(struct SDL_RWops* context)
 */
static int32_t RWops_opus_close(void * stream)
{
    (void) stream; // deliberately unused, but present for API compliance
    /* SDL closes this for us */
    // return SDL_RWclose((SDL_RWops*)stream);
    return 0;
} /* RWops_opus_close */


/*
 * Read-Write Ops Tell Callback Wrapper
 * ------------------------------------
 * OPUS: typedef opus_int64(* op_tell_func)(void *_stream)
 * SDL:  Sint64 SDL_RWtell(struct SDL_RWops* context)
 */
static opus_int64 RWops_opus_tell(void * stream)
{
    // Guard against invalid input
    assertm(stream, "OPUS: Input is not initialized");

    const int64_t current_offset = SDL_RWtell(static_cast<SDL_RWops*>(stream));
    SNDDBG(("Opus ops tell:          "
            "%ld\n", current_offset));
    return current_offset;
} /* RWops_opus_tell */


// Populate the opus IO callback object (in prescribed order)
static const OpusFileCallbacks RWops_opus_callbacks =
{
    RWops_opus_read, // .read member
    RWops_opus_seek, // .seek member
    RWops_opus_tell, // .tell member
    RWops_opus_close // .close member
};

static __inline__ void output_opus_info(const OggOpusFile * of, const OpusHead * oh)
{
#if (defined DEBUG_CHATTER)
    // Guard against invalid input
    assertm(of && oh, "OPUS: Inputs are not initialized");

    const OpusTags* ot = op_tags(of, -1);
    if (ot != nullptr) {
        SNDDBG(("Opus serial number:     %u\n", op_serialno(of, -1)));
        SNDDBG(("Opus format version:    %d\n", oh->version));
        SNDDBG(("Opus channel count:     %d\n", oh->channel_count ));
        SNDDBG(("Opus seekable:          %s\n", op_seekable(of) ? "True" : "False"));
        SNDDBG(("Opus pre-skip samples:  %u\n", oh->pre_skip));
        SNDDBG(("Opus input sample rate: %u\n", oh->input_sample_rate));
        SNDDBG(("Opus logical streams:   %d\n", oh->stream_count));
        SNDDBG(("Opus vendor:            %s\n", ot->vendor));
        for (int i = 0; i < ot->comments; i++) {
            SNDDBG(("Opus: user comment:     '%s'\n", ot->user_comments[i]));
        }
    }
#else
    (void) of; // unused if DEBUG_CHATTER not defined
    (void) oh; // unused if DEBUG_CHATTER not defined
#endif
} /* output_opus_comments */

/*
 * Opus Close
 * ----------
 * Free and nullptr all heap-allocated codec objects.
 */
static void opus_close(Sound_Sample * sample)
{
    // Guard against the no-op case
    if (!sample
       || !sample->opaque
       || !static_cast<Sound_SampleInternal*>(sample->opaque)->decoder_private)
        return;

    /* From the Opus docs: if opening a stream/file/or using op_test_callbacks() fails
     * then we are still responsible for freeing the OggOpusFile with op_free().
     */
    auto *internal = static_cast<Sound_SampleInternal*>(sample->opaque);
    auto *of = static_cast<OggOpusFile*>(internal->decoder_private);
    if (of != nullptr) {
        op_free(of);
        internal->decoder_private = nullptr;
    }
    return;

} /* opus_close */

/*
 * Opus Open
 * ---------
 *  - Creates a new opus file object by using our our callback structure for all IO operations.
 *  - SDL expects a returns of 1 on success
 */
static int32_t opus_open(Sound_Sample * sample, const char * ext)
{
    // Guard against invalid input
    assertm(sample, "OPUS: Input is not initialized");
    (void) ext; // deliberately unused, but present for API compliance

    int32_t rcode = 0; // assume failure until determined otherwise
    auto *internal = static_cast<Sound_SampleInternal*>(sample->opaque);
    OggOpusFile *of = op_open_callbacks(internal->rw, &RWops_opus_callbacks, nullptr, 0, &rcode);
    internal->decoder_private = of;

    // Had a problem during the open
    if (rcode != 0) { // op_open will set rcode to non-zero
        opus_close(sample);
        SNDDBG(("OPUS: open error:        "
                "'Could not open file due errno: %d'\n", rcode));
        return 0; // We return 0 to indicate failure from opus_open
    } else
        rcode = 1; // Otherwise open succeeded, so set rcode to 1

    const OpusHead* oh = op_head(of, -1);
    output_opus_info(of, oh);

    // Populate track properties
    sample->actual.rate = OPUS_SAMPLE_RATE;
    sample->actual.channels = static_cast<Uint8>(oh->channel_count);
    sample->flags = op_seekable(of) ? SOUND_SAMPLEFLAG_CANSEEK: 0;
    sample->actual.format = AUDIO_S16SYS;

    // Populate the track's duration in milliseconds (or -1 if bad)
    const auto pcm_result = static_cast<int32_t>(op_pcm_total(of, -1)); 
    if (pcm_result == OP_EINVAL)
        internal->total_time = -1;
    else {
        constexpr auto frames_per_ms = static_cast<int32_t>(OPUS_SAMPLE_RATE_PER_MS);
        internal->total_time = ceil_sdivide(pcm_result, frames_per_ms);
    }
    return rcode;
} /* opus_open */

/*
 * Opus Read
 * ---------
 */
static uint32_t opus_read(Sound_Sample * sample, void * buffer, uint32_t requested_frames)
{
    // Guard against invalid inputs and the no-op case
    assertm(sample && buffer, "OPUS: Inputs are not initialized");
    if (requested_frames == 0)
        return 0u;

    auto *internal = static_cast<Sound_SampleInternal*>(sample->opaque);
    auto *of = static_cast<OggOpusFile*>(internal->decoder_private);
    const uint32_t channels = sample->actual.channels;

    // Initial state-tracking variables
    uint32_t total_decoded_samples = 0;
    auto *buf_pos = static_cast<opus_int16*>(buffer);
    int32_t remaining_samples = static_cast<int32_t>(requested_frames * channels);

    // Start the decode loop, incrementing as we go
    while(remaining_samples > 0) {
        const int result = op_read(of, buf_pos, remaining_samples, nullptr);
        if (result == 0) {
            sample->flags |= SOUND_SAMPLEFLAG_EOF;
            break;
        }
        if (result == OP_HOLE)
            continue;  // hole in the data; keeping going!

        if (result  < 0) {
            sample->flags |= SOUND_SAMPLEFLAG_ERROR;
            break;
        }
        // If good, the result contains the number samples decoded per channel (ie: frames)
        const uint32_t decoded_samples = static_cast<uint32_t>(result) * channels;
        buf_pos               += decoded_samples;
        remaining_samples     -= decoded_samples;
        total_decoded_samples += decoded_samples;
    }

    // Finally, we return the number of frames decoded 
    const uint32_t decoded_frames = ceil_udivide(total_decoded_samples, channels);
    return decoded_frames;
} /* opus_read */

/*
 * Opus Seek
 * ---------
 * Set the current position of the stream to the indicated
 * integer offset in milliseconds.
 */
static int32_t opus_seek(Sound_Sample * sample, const uint32_t ms)
{
    // Guard against invalid input
    assertm(sample, "OPUS: Input is not initialized");

    int rcode = -1;

    auto *internal = static_cast<Sound_SampleInternal*>(sample->opaque);
    auto *of = static_cast<OggOpusFile*>(internal->decoder_private);

#if (defined DEBUG_CHATTER)
    const float total_seconds = ms / 1000.0;
    uint8_t minutes = total_seconds / 60;
    const double seconds =
        static_cast<int>(total_seconds) % 60
        + total_seconds
        - static_cast<int>(total_seconds);
    const uint8_t hours = minutes / 60;
    minutes = minutes % 60;
#endif

    // convert the desired ms offset into OPUS PCM samples
    const ogg_int64_t desired_pcm = ms * OPUS_SAMPLE_RATE_PER_MS;
    rcode = op_pcm_seek(of, desired_pcm);

    if (rcode != 0) {
        SNDDBG(("Opus seek problem, see errno:        %d\n", rcode));
        sample->flags |= SOUND_SAMPLEFLAG_ERROR;
    } else {
        SNDDBG(("Opus seek in file:      "
                "{requested_time: '%02d:%02d:%.2f', becomes_opus_pcm: %ld}\n",
                hours, minutes, seconds, desired_pcm));
    }
    return (rcode == 0);
} /* opus_seek */

/*
 * Opus Rewind
 * -----------
 * Sets the current position of the stream to 0.
 */
static int32_t opus_rewind(Sound_Sample* sample)
{
    // Guard against invalid input
    assertm(sample, "OPUS: Input is not initialized");
    return opus_seek(sample, 0);
} /* opus_rewind */



static const char* extensions_opus[] = { "OPUS", nullptr };

extern const Sound_DecoderFunctions __Sound_DecoderFunctions_OPUS =
{
    {
        extensions_opus,
        "Ogg Opus audio using libopusfile",
        "The DOSBox Team"
    },

    opus_init,   /*   init() method */
    opus_quit,   /*   quit() method */
    opus_open,   /*   open() method */
    opus_close,  /*  close() method */
    opus_read,   /*   read() method */
    opus_rewind, /* rewind() method */
    opus_seek    /*   seek() method */
}; }
/* end of opus.cpp ... */
