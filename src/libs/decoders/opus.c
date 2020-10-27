/*
 * This DOSBox Ogg Opus decoder backend is written and copyright 2019 Kevin R Croft (krcroft@gmail.com)
 *
 * This decoders makes use of:
 *   - libopusfile, for .opus file handing and frame decoding
 *   - speexdsp, for resampling to the original input rate, if needed
 *
 * Source links
 *   - libogg:     https://github.com/xiph/ogg
 *   - libopus:    https://github.com/xiph/opus
 *   - opusfile:   https://github.com/xiph/opusfile
 *   - speexdsp:   https://github.com/xiph/speexdsp
 *   - opus-tools: https://github.com/xiph/opus-tools

 * Documentation references
 *   - Ogg Opus:  https://www.opus-codec.org/docs
 *   - OpusFile:  https://mf4.xiph.org/jenkins/view/opus/job/opusfile-unix/ws/doc/html/index.html
 *   - Resampler: https://www.speex.org/docs/manual/speex-manual/node7.html
 *
 * Please see the file LICENSE.txt in the source's root directory.
 *
 *  This DOSBox Ogg Opus decoder backend is free software: you can redistribute
 *  it and/or modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This DOSBox Ogg Opus decoder backend is distributed in the hope that it
 *  will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 *  of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  along with DOSBox.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif
#include <stdlib.h> // getenv()

#include "SDL_sound.h"
#define __SDL_SOUND_INTERNAL__
#include "SDL_sound_internal.h"

#include "opusfile.h"
#include "speex/speex_resampler.h"

// The minimum buffer samples per channel: 120 ms @ 48 samples/ms, defined by opus
#define OPUS_MIN_BUFFER_SAMPLES_PER_CHANNEL 5760

// Opus's internal sample rates, to which all encoded streams get resampled
#define OPUS_SAMPLE_RATE 48000
#define OPUS_SAMPLE_RATE_PER_MS 48

static Sint32 opus_init   (void);
static void   opus_quit   (void);
static Sint32 opus_open   (Sound_Sample* sample, const char* ext);
static void   opus_close  (Sound_Sample* sample);
static Uint32 opus_read   (Sound_Sample* sample);
static Sint32 opus_rewind (Sound_Sample* sample);
static Sint32 opus_seek   (Sound_Sample* sample, const Uint32 ms);

static const char* extensions_opus[] = { "OPUS", NULL };

const Sound_DecoderFunctions __Sound_DecoderFunctions_OPUS =
{
	{
		extensions_opus,
		"Ogg Opus audio using libopusfile",
		"Kevin R Croft <krcroft@gmail.com>",
		"https://www.opus-codec.org/"
	},

	opus_init,   /*   init() method */
	opus_quit,   /*   quit() method */
	opus_open,   /*   open() method */
	opus_close,  /*  close() method */
	opus_read,   /*   read() method */
	opus_rewind, /* rewind() method */
	opus_seek    /*   seek() method */
};


// Our private-decoder structure where we hold the opusfile, resampler,
// circular buffer, and buffer tracking variables.
typedef struct
{
	Uint64 of_pcm;                  // absolute position in consumed Opus samples
	OggOpusFile* of;                // the actual opusfile we open/read/seek within
	opus_int16* buffer;             // pointer to the start of our circular buffer
	SpeexResamplerState* resampler; // pointer to an instantiated resampler
	float rate_ratio;               // OPUS_RATE (48KHz) divided by desired sample rate
	Uint16 buffer_size;             // maximum number of samples we can hold in our buffer
	Uint16 decoded;                 // number of samples decoded in our buffer
	Uint16 consumed;                // number of samples consumed in our buffer
	Uint16 frame_size;              // number of samples decoded in one opus frame
	SDL_bool eof;                   // indicates if we've hit end-of-file decoding
} opus_t;


static Sint32 opus_init(void)
{
	SNDDBG(("Opus init:              done\n"));
	return 1; /* always succeeds. */
} /* opus_init */


static void opus_quit(void){
	SNDDBG(("Opus quit:              done\n"));
} // no-op


/*
 * Read-Write Ops Read Callback Wrapper
 * ====================================
 *
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
static Sint32 RWops_opus_read(void* stream, unsigned char* ptr, Sint32 nbytes)
{
	const Sint32 bytes_read = SDL_RWread((SDL_RWops*)stream,
	                                     (void*)ptr,
	                                     sizeof(unsigned char),
	                                     (size_t)nbytes);
	SNDDBG(("Opus ops read:          "
	        "{wanted: %d, returned: %ld}\n", nbytes, bytes_read));

	return bytes_read;
} /* RWops_opus_read */


/*
 * Read-Write Ops Seek Callback Wrapper
 * ====================================
 *
 * OPUS: typedef int(* op_seek_func)
 *       void*      _stream, --> The stream to seek in
 *       opus_int64 _offset, --> Sets the position indicator for _stream in bytes
 *       int _whence         --> If whence is set to SEEK_SET, SEEK_CUR, or SEEK_END,
 *                               the offset is relative to the start of the stream,
 *                               the current position indicator, or end-of-file,
 *                               respectively
 *    Returns: 0  Success, or -1 if seeking is not supported or an error occurred.
 *      define	SEEK_SET	0
 *      define	SEEK_CUR	1
 *      define	SEEK_END	2
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
static Sint32 RWops_opus_seek(void* stream, const opus_int64 offset, const Sint32 whence)
{
	const Sint64 offset_after_seek = SDL_RWseek((SDL_RWops*)stream, offset, whence);

	SNDDBG(("Opus ops seek:          "
	        "{requested offset: %ld, seeked offset: %ld}\n",
	        offset, offset_after_seek));

	return (offset_after_seek != -1 ? 0 : -1);
} /* RWops_opus_seek */


/*
 * Read-Write Ops Close Callback Wrapper
 * =====================================
 * OPUS: typedef int(* op_close_func)(void *_stream)
 * SDL:  Sint32 SDL_RWclose(struct SDL_RWops* context)
 */
static Sint32 RWops_opus_close(void* stream)
{
	/* SDL closes this for us */
	// return SDL_RWclose((SDL_RWops*)stream);
	return 0;
} /* RWops_opus_close */


/*
 * Read-Write Ops Tell Callback Wrapper
 * ====================================
 * OPUS: typedef opus_int64(* op_tell_func)(void *_stream)
 * SDL:  Sint64 SDL_RWtell(struct SDL_RWops* context)
 */
static opus_int64 RWops_opus_tell(void* stream)
{
	const Sint64 current_offset = SDL_RWtell((SDL_RWops*)stream);

	SNDDBG(("Opus ops tell:          "
	        "%ld\n", current_offset));

	return current_offset;
} /* RWops_opus_tell */


// Populate the opus callback object (in perscribed order), with our callback functions.
static const OpusFileCallbacks RWops_opus_callbacks =
{
	RWops_opus_read,
	RWops_opus_seek,
	RWops_opus_tell,
	RWops_opus_close
};

/* Return a human readable version of an OpusFile error code... */
#if (defined DEBUG_CHATTER)
static const char* opus_error(const Sint8 errnum)
{
	switch(errnum)
	{
	case OP_FALSE:         // -1
		return("A request did not succeed");
	case OP_EOF:           // -2
		return("End of file");
	case OP_HOLE:          // -3
		return("There was a hole in the page sequence numbers "
		       "(e.g., a page was corrupt or missing)");
	case OP_EREAD:         // -128
		return("An underlying read, seek, or tell operation "
		       "failed when it should have succeeded");
	case OP_EFAULT:        // -129
		return("A NULL pointer was passed where one was unexpected, or an "
		       "internal memory allocation failed, or an internal library "
		       "error was encountered");
	case OP_EIMPL:         // -130
		return("The stream used a feature that is not implemented,"
		       " such as an unsupported channel family");
	case OP_EINVAL:        // -131
		return("One or more parameters to a function were invalid");
	case OP_ENOTFORMAT:    // -132
		return("A purported Ogg Opus stream did not begin with an Ogg page, a "
		       "purported header packet did not start with one of the required "
		       "strings, `OpusHead` or `OpusTags`, or a link in a chained file "
		       "was encoun tered that did not contain any logical Opus streams");
	case OP_EBADHEADER:    // -133
		return("A required header packet was not properly formatted, contained illegal "
		       "values, or was missing altogether");
	case OP_EVERSION:      // -134
		return("The ID header contained an unrecognized version number");
	case OP_ENOTAUDIO:     // -135
		return("Not valid audio");
	case OP_EBADPACKET:    // -136
		return("An audio packet failed to decode properly");
	case OP_EBADLINK:      // -137
		return("We failed to find data we had seen before, or the bitstream structure was "
		       "sufficiently malformed that seeking to the target destination was impossible");
	case OP_ENOSEEK:       // -138
		return("An operation that requires seeking was requested on an unseekable stream");
	case OP_EBADTIMESTAMP: // -139
		return("The first or last granule position of a link failed basic validity checks");
	} /* switch */
	return "unknown error";
} /* opus_error */
#endif

static __inline__ void output_opus_info(const OggOpusFile* of, const OpusHead* oh)
{
#if (defined DEBUG_CHATTER)
	const OpusTags* ot = op_tags(of, -1);

	// Guard
	if (    of == NULL
	     || oh == NULL
	     || ot == NULL) return;

	// Dump info
	SNDDBG(("Opus serial number:     %u\n", op_serialno(of, -1)));
	SNDDBG(("Opus format version:    %d\n", oh->version));
	SNDDBG(("Opus channel count:     %d\n", oh->channel_count ));
	SNDDBG(("Opus seekable:          %s\n", op_seekable(of) ? "True" : "False"));
	SNDDBG(("Opus pre-skip samples:  %u\n", oh->pre_skip));
	SNDDBG(("Opus input sample rate: %u\n", oh->input_sample_rate));
	SNDDBG(("Opus logical streams:   %d\n", oh->stream_count));
	SNDDBG(("Opus vendor:            %s\n", ot->vendor));
	for (int i = 0; i < ot->comments; i++)
		SNDDBG(("Opus: user comment:     '%s'\n", ot->user_comments[i]));
#endif
} /* output_opus_comments */

/*
 * Opus Open
 * =========
 *  - Creates a new opus file object by using our our callback structure for all IO operations.
 *  - We also intialize and allocate memory for fields in the opus_t decode structure.
 *  - SDL expects a returns of 1 on success
 */
static Sint32 opus_open(Sound_Sample* sample, const char* ext)
{
	Sint32 rcode;
	Sound_SampleInternal* internal = (Sound_SampleInternal*)sample->opaque;

	// Open the Opus File and print some info
	OggOpusFile* of = op_open_callbacks(internal->rw, &RWops_opus_callbacks, NULL, 0, &rcode);
	if (rcode != 0) {
		op_free(of);
		of = NULL;
		SNDDBG(("Opus open error:        "
		        "'Could not open opus file: %s'\n", opus_error(rcode)));
		BAIL_MACRO("Opus open fatal: 'Not a valid Ogg Opus file'", 0);
	}
	const OpusHead* oh = op_head(of, -1);
	output_opus_info(of, oh);

	// Initialize our decoder struct elements
	opus_t* decoder = (opus_t*)SDL_malloc(sizeof(opus_t));
	decoder->of = of;
	decoder->of_pcm = 0;
	decoder->decoded = 0;
	decoder->consumed = 0;
	decoder->frame_size = 0;
	decoder->eof = SDL_FALSE;
	decoder->buffer = NULL;

	// Connect our long-lived internal decoder to the one we're building here
	internal->decoder_private = decoder;

	if (   sample->desired.rate != 0
	    && sample->desired.rate != OPUS_SAMPLE_RATE
	    && getenv("SDL_DONT_RESAMPLE") == NULL){

		// Opus resamples all inputs to 48kHz. By default (if env-var SDL_DONT_RESAMPLE doesn't exist)
		// we resample to the desired rate so the recieving SDL_sound application doesn't have to.
		// This avoids breaking applications that don't expect 48kHz audio and also gives us
		// quality-control by using the speex resampler, which has a noise floor of -140 dB, which
		// is ~40dB lower than the -96dB offered by 16-bit CD-quality audio.
		//
		sample->actual.rate = sample->desired.rate;
		decoder->rate_ratio = OPUS_SAMPLE_RATE / (float)(sample->desired.rate);
		decoder->resampler = speex_resampler_init(oh->channel_count,
		                                          OPUS_SAMPLE_RATE,
		                                          sample->desired.rate,
		                                          // SPEEX_RESAMPLER_QUALITY_VOIP,    // consumes ~20 Mhz
		                                          SPEEX_RESAMPLER_QUALITY_DEFAULT,    // consumes ~40 Mhz
		                                          // SPEEX_RESAMPLER_QUALITY_DESKTOP, // consumes ~80 Mhz
		                                          &rcode);

		// If we failed to initialize the resampler, then tear down
		if (rcode < 0) {
			opus_close(sample);
			BAIL_MACRO("Opus: failed initializing the resampler", 0);
		}

	// Otherwise use native sampling
	} else {
		sample->actual.rate = OPUS_SAMPLE_RATE;
		decoder->rate_ratio = 1.0;
		decoder->resampler = NULL;
	}

	// Allocate our buffer to hold PCM samples from the Opus decoder
	decoder->buffer_size = oh->channel_count * OPUS_MIN_BUFFER_SAMPLES_PER_CHANNEL * 1.5;
	decoder->buffer = (opus_int16 *)SDL_malloc(decoder->buffer_size * sizeof(opus_int16));

	// Gather static properties about our stream (channels, seek-ability, format, and duration)
	sample->actual.channels = (Uint8)(oh->channel_count);
	sample->flags = (Sound_SampleFlags)(op_seekable(of) ? SOUND_SAMPLEFLAG_CANSEEK: 0);
	sample->actual.format = AUDIO_S16LSB; // returns least-significant-byte order regardless of architecture

	ogg_int64_t total_time = op_pcm_total(of, -1); // total PCM samples in the stream
	internal->total_time = total_time == OP_EINVAL ? -1 : // total milliseconds in the stream
		                                 (Sint32)( (double)total_time / OPUS_SAMPLE_RATE_PER_MS);

	return 1;
} /* opus_open */


/*
 * Opus Close
 * ==========
 * Free and NULL all allocated memory pointers.
 */
static void opus_close(Sound_Sample* sample)
{
	/* From the Opus docs: if opening a stream/file/or using op_test_callbacks() fails
	 * then we are still responsible for freeing the OggOpusFile with op_free().
	 */
	Sound_SampleInternal* internal = (Sound_SampleInternal*) sample->opaque;

	opus_t* d = (opus_t*)internal->decoder_private;
	if (d != NULL) {

		if (d->of != NULL) {
			op_free(d->of);
			d->of = NULL;
		}

		if(d->resampler != NULL) {
			speex_resampler_destroy(d->resampler);
			d->resampler = NULL;
		}

		if(d->buffer != NULL) {
			SDL_free(d->buffer);
			d->buffer = NULL;
		}

		SDL_free(d);
		d = NULL;
	}
	return;

} /* opus_close */


/*
 * Opus Read
 * =========
 * Decode, resample (if needed), and write the output to the
 * requested buffer.
 */
static Uint32 opus_read(Sound_Sample* sample)
{
	Sound_SampleInternal* internal = (Sound_SampleInternal*) sample->opaque;
	opus_t* d = (opus_t*)internal->decoder_private;

	opus_int16* output_buffer = (opus_int16*)internal->buffer;
	const Uint16 requested_output_size = internal->buffer_size / sizeof(opus_int16);
	const Uint16 derived_consumption_size = (requested_output_size * d->rate_ratio) + 0.5;

	// Three scenarios in order of probabilty:
	//
	// 1. consume: resample (if needed) a chunk from our decoded queue
	//             sufficient to fill the requested buffer.
	//
	//             If the decoder has hit the end-of-file, drain any
	//             remaining decoded data before setting the EOF flag.
	//
	// 2. decode:  decode chunks unil our buffer is full or we hit EOF.
	//
	// 3. wrap:    we've decoded and consumed to edge of our buffer
	//             so wrap any remaining decoded samples back around.

	Sint32 rcode = 1;
	SDL_bool have_consumed = SDL_FALSE;
	while (! have_consumed){

		// consume ...
		const Uint16 unconsumed_size = d->decoded - d->consumed;
		if (unconsumed_size >= derived_consumption_size || d->eof) {

			// If we're at the start of the stream, ignore 'pre-skip' samples
			// per-channel.  Pre-skip describes how much data must be decoded
			// before valid output is obtained.
			//
			const OpusHead* oh = op_head(d->of, -1);
			if (d->of_pcm == 0)
				d->consumed += oh->pre_skip * oh->channel_count;

			// We use these to record the actual consumed and output sizes
			Uint32 actual_consumed_size = unconsumed_size;
			Uint32 actual_output_size = requested_output_size;

			// If we need to resample
			if (d->resampler)
				(void) speex_resampler_process_int(d->resampler, 0,
				                                   d->buffer + d->consumed,
				                                   &actual_consumed_size,
				                                   output_buffer,
				                                   &actual_output_size);

			// Otherwise copy the bytes
			else {
				if (unconsumed_size < requested_output_size)
					actual_output_size = unconsumed_size;
				actual_consumed_size = actual_output_size;
				SDL_memcpy(output_buffer, d->buffer + d->consumed, actual_output_size * sizeof(opus_int16));
			}

			// bump our comsumption count and absolute pcm position
			d->consumed += actual_consumed_size;
			d->of_pcm += actual_consumed_size;

			SNDDBG(("Opus read consuming:    "
			        "{output: %u, so_far: %u, remaining_buffer: %u}\n",
			        actual_output_size, d->consumed, d->decoded - d->consumed));

			// if we wrote less than requested then we're at the end-of-file
			if (actual_output_size < requested_output_size) {
				sample->flags |= SOUND_SAMPLEFLAG_EOF;
				SNDDBG(("Opus read consuming:    "
				        "{end_of_buffer: True, requested: %u, resampled_output: %u}\n",
				        requested_output_size, actual_output_size));
			}

			rcode = actual_output_size * sizeof(opus_int16); // covert from samples to bytes
			have_consumed = SDL_TRUE;
		}

		else {
			// wrap ...
			if (d->frame_size > 0) {
				SDL_memcpy(d->buffer,
				           d->buffer + d->consumed,
				           (d->decoded - d->consumed)*sizeof(opus_int16));

				d->decoded -= d->consumed;
				d->consumed = 0;

				SNDDBG(("Opus read wrapping:     "
				        "{wrapped: %u}\n", d->decoded));
			}

			// decode ...
			while (rcode > 0 && d->buffer_size - d->decoded >= d->frame_size) {

				rcode = sample->actual.channels * op_read(d->of,
				                                          d->buffer      + d->decoded,
				                                          d->buffer_size - d->decoded, NULL);
				// Use the largest decoded frame to know when
				// our buffer is too small to hold a frame, to
				// avoid constraining the decoder to fill sizes
				// smaller than the stream's frame-size
				if (rcode > d->frame_size){

					SNDDBG(("Opus read decoding:     "
					        "{frame_previous: %u, frame_new: %u}\n",
					        d->frame_size, rcode));

					d->frame_size = rcode;
				}

				// assess the validity of the return code
				if      (rcode  > 0)       d->decoded += rcode;  // reading
				else if (rcode == 0)       d->eof = SDL_TRUE;    // done
				else if (rcode == OP_HOLE) rcode = 1;            // hole in the data, carry on
				else // (rcode  < 0)                             // error
					sample->flags |= SOUND_SAMPLEFLAG_ERROR;

				SNDDBG(("Opus read decoding:     "
				        "{decoded: %u, remaining buffer: %u, end_of_file: %s}\n",
				        rcode, d->buffer_size - d->decoded, d->eof ? "True" : "False"));
			}
		}
	} // end while.
	return rcode;
} /* opus_read */


/*
 * Opus Rewind
 * ===========
 * Sets the current position of the stream to 0.
 */
static Sint32 opus_rewind(Sound_Sample* sample)
{
	const Sint32 rcode = opus_seek(sample, 0);
	BAIL_IF_MACRO(rcode < 0, ERR_IO_ERROR, 0);
	return rcode;
} /* opus_rewind */


/*
 * Opus Seek
 * =========
 * Set the current position of the stream to the indicated
 * integer offset in milliseconds.
 */
static Sint32 opus_seek(Sound_Sample* sample, const Uint32 ms)
{
	Sound_SampleInternal* internal = (Sound_SampleInternal*) sample->opaque;
	opus_t* d = (opus_t *)internal->decoder_private;
	int rcode = -1;

	#if (defined DEBUG_CHATTER)
	const float total_seconds = (float)ms/1000;
	uint8_t minutes = total_seconds / 60;
	const float seconds = ((int)total_seconds % 60) + (total_seconds - (int)total_seconds);
	const uint8_t hours = minutes / 60;
	minutes = minutes % 60;
	#endif

	// convert the desired ms offset into OPUS PCM samples
	const ogg_int64_t desired_pcm = ms * OPUS_SAMPLE_RATE_PER_MS;

	// Is our stream already positioned at the requested offset?
	if (d->of_pcm == desired_pcm){

		SNDDBG(("Opus seek avoided:      "
		        "{requested_time: '%02d:%02d:%.2f', becomes_opus_pcm: %ld, actual_pcm_pos: %ld}\n",
		        hours, minutes, seconds, desired_pcm, d->of_pcm));

		rcode = 1;
	}

	// If not, check if we can jump within our circular buffer (and not actually seek!)
	// In this scenario, we don't have to waste our currently decoded samples
	// or incur the cost of 80ms of pre-roll decoding behind the scene in libopus.
	else {
		Uint64 pcm_start = d->of_pcm - d->consumed;
		Uint64 pcm_end = pcm_start + d->decoded;

		// In both scenarios below we're going to seek, in which case
		// our sample flags should be reset and let the read function
		// re-assess the flag.
		//

		// Is the requested pcm offset within our decoded range?
		if (desired_pcm >= pcm_start && desired_pcm <= pcm_end) {

			SNDDBG(("Opus seek avoided:      "
			        "{requested_time: '%02d:%02d:%.2f', becomes_opus_pcm: %ld, buffer_start: %ld, buffer_end: %ld}\n",
			        hours, minutes, seconds, desired_pcm, pcm_start, pcm_end));

			// Yes, so simply adjust our existing pcm offset and consumption position
			// No seeks or pre-roll needed!
			d->consumed = desired_pcm - pcm_start;
			d->of_pcm = desired_pcm;

			// reset our sample flags and let our consumption state re-apply
			// the flags per its own rules
			if (op_seekable(d->of))
				sample->flags = SOUND_SAMPLEFLAG_CANSEEK;

			// note, we don't reset d->eof because our decode state is unchanged
			rcode = 1;
			// rcode is 1, confirming we successfully seeked
		}

		// No; the requested pcm offset is outside our circular decode buffer,
		// so actually seek and reset our decode and consumption counters.
		else {
			rcode = op_pcm_seek(d->of, desired_pcm) + 1;

			// op_pcm_seek(..) returns 0, to which we add 1, on success
			// ... or a negative value on error.
			if (rcode > 0) {
				d->of_pcm = desired_pcm;
				d->consumed = 0;
				d->decoded = 0;
				d->eof = SDL_FALSE;
				SNDDBG(("Opus seek in file:      "
				        "{requested_time: '%02d:%02d:%.2f', becomes_opus_pcm: %ld}\n",
				        hours, minutes, seconds, desired_pcm));

				// reset our sample flags and let the read function re-apply
				// sample flags as it hits them from our our offset
				if (op_seekable(d->of))
					sample->flags = SOUND_SAMPLEFLAG_CANSEEK;

			}
			// otherwise we failed to seek.. so leave everything as-is.
		}
	}

	BAIL_IF_MACRO(rcode < 0, ERR_IO_ERROR, 0);
	return rcode;
} /* opus_seek */

/* end of ogg_opus.c ... */

