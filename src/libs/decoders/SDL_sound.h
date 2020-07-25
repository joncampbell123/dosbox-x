/*
 *  Modified SDL Sound API
 *  ----------------------
 *  The basic gist of SDL_sound is that you use an SDL_RWops to get sound data
 *  into this library, and SDL_sound will take that data, in one of several
 *  popular formats, and decode it into raw waveform data in the format of
 *  your choice. This gives you a nice abstraction for getting sound into your
 *  game or application; just feed it to SDL_sound, and it will handle
 *  decoding and converting, so you can just pass it to your SDL audio
 *  callback (or whatever). Since it gets data from an SDL_RWops, you can get
 *  the initial sound data from any number of sources: file, memory buffer,
 *  network connection, etc.
 *
 *  As the name implies, this library depends on SDL2: Simple Directmedia Layer,
 *  which is a powerful, free, and cross-platform multimedia library. It can
 *  be found at http://www.libsdl.org/
 *
 * Support is in place for the following sound formats:
 *   - .WAV/.W64 (Microsoft WAVfile RIFF and Sony Wave64 data, via the dr_wav single-header codec)
 *   - .MP3  (MPEG-1 Layer 3 support via the dr_mp3 single-header decoder)
 *   - .OGG  (Ogg Vorbis support via the std_vorbis single-header decoder)
 *   - .OPUS (Ogg Opus support via the Opusfile and SpeexDSP libraries)
 *   - .FLAC (Free Lossless Audio Codec support via the dr_flac single-header decoder)
 *
 *  Copyright (C) 2020       The dosbox-staging team
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

#ifndef _INCLUDE_SDL_SOUND_H_
#define _INCLUDE_SDL_SOUND_H_

#include <SDL.h>
#include <SDL_endian.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef DOXYGEN_SHOULD_IGNORE_THIS

#ifndef SDLCALL  /* may not be defined with older SDL releases. */
#define SDLCALL
#endif

#ifdef SDL_SOUND_DLL_EXPORTS
#  define SNDDECLSPEC __declspec(dllexport)
#elif (__GNUC__ >= 3)
#  define SNDDECLSPEC __attribute__((visibility("default")))
#else
#  define SNDDECLSPEC
#endif

#define SOUND_VER_MAJOR 1
#define SOUND_VER_MINOR 0
#define SOUND_VER_PATCH 1
#endif


/**
 * \enum Sound_SampleFlags
 * \brief Flags that are used in a Sound_Sample to show various states.
 *
 * To use:
 * \code
 * if (sample->flags & SOUND_SAMPLEFLAG_ERROR) { dosomething(); }
 * \endcode
 *
 * \sa Sound_SampleNew
 * \sa Sound_SampleNewFromFile
 * \sa Sound_SampleDecode
 * \sa Sound_SampleSeek
 */
typedef enum
{
    SOUND_SAMPLEFLAG_NONE    = 0x0, /**< No special attributes. */

    /* these are set at sample creation time... */
    SOUND_SAMPLEFLAG_CANSEEK = 0x1, /**< Sample can seek to arbitrary points. */

    /* these are set during decoding... */
    SOUND_SAMPLEFLAG_EOF     = 0x2, /**< End of input stream. */
    SOUND_SAMPLEFLAG_ERROR   = 0x4, /**< Unrecoverable error. */
    SOUND_SAMPLEFLAG_EAGAIN  = 0x8  /**< Function would block, or temp error. */
} Sound_SampleFlags;

/**
 * \struct Sound_AudioInfo
 * \brief Information about an existing sample's format.
 *
 * These are the basics of a decoded sample's data structure: data format
 *  (see AUDIO_U8 and friends in SDL_audio.h), number of channels, and sample
 *  rate. If you need more explanation than that, you should stop developing
 *  sound code right now.
 *
 * \sa Sound_SampleNew
 * \sa Sound_SampleNewFromFile
 */
typedef struct
{
    Uint16 format;  /**< Equivalent of SDL_AudioSpec.format. */
    Uint8 channels; /**< Number of sound channels. 1 == mono, 2 == stereo. */
    Uint32 rate;    /**< Sample rate; frequency of sample points per second. */
} Sound_AudioInfo;


/**
 * \struct Sound_DecoderInfo
 * \brief Information about available soudn decoders.
 *
 * Each decoder sets up one of these structs, which can be retrieved via
 *  the Sound_AvailableDecoders() function. EVERY FIELD IN THIS IS READ-ONLY.
 *
 * The extensions field is a NULL-terminated list of ASCIZ strings. You
 *  should read it like this:
 *
 * \code
 * const char **ext;
 * for (ext = info->extensions; *ext != NULL; ext++) {
 *     printf("   File extension \"%s\"\n", *ext);
 * }
 * \endcode
 *
 * \sa Sound_AvailableDecoders
 */
typedef struct
{
    const char **extensions; /**< File extensions, list ends with NULL. */
    const char *description; /**< Human readable description of decoder. */
    const char *author;      /**< "Name Of Author \<email@emailhost.dom\>" */
    const char *url;         /**< URL specific to this decoder. */
} Sound_DecoderInfo;



/**
 * \struct Sound_Sample
 * \brief Represents sound data in the process of being decoded.
 *
 * The Sound_Sample structure is the heart of SDL_sound. This holds
 *  information about a source of sound data as it is being decoded.
 *  EVERY FIELD IN THIS IS READ-ONLY. Please use the API functions to
 *  change them.
 */
typedef struct
{
    void *opaque;  /**< Internal use only. Don't touch. */
    const Sound_DecoderInfo *decoder;  /**< Decoder used for this sample. */
    Sound_AudioInfo desired;  /**< Desired audio format for conversion. */
    Sound_AudioInfo actual;  /**< Actual audio format of sample. */
    Uint32 flags;  /**< Flags relating to this sample. */
} Sound_Sample;


/**
 * \struct Sound_Version
 * \brief Information the version of SDL_sound in use.
 *
 * Represents the library's version as three levels: major revision
 *  (increments with massive changes, additions, and enhancements),
 *  minor revision (increments with backwards-compatible changes to the
 *  major revision), and patchlevel (increments with fixes to the minor
 *  revision).
 *
 * \sa SOUND_VERSION
 * \sa Sound_GetLinkedVersion
 */
typedef struct
{
    int major; /**< major revision */
    int minor; /**< minor revision */
    int patch; /**< patchlevel */
} Sound_Version;


/* functions and macros... */

/**
 * \def SOUND_VERSION(x)
 * \brief Macro to determine SDL_sound version program was compiled against.
 *
 * This macro fills in a Sound_Version structure with the version of the
 *  library you compiled against. This is determined by what header the
 *  compiler uses. Note that if you dynamically linked the library, you might
 *  have a slightly newer or older version at runtime. That version can be
 *  determined with Sound_GetLinkedVersion(), which, unlike SOUND_VERSION,
 *  is not a macro.
 *
 * \param x A pointer to a Sound_Version struct to initialize.
 *
 * \sa Sound_Version
 * \sa Sound_GetLinkedVersion
 */
#define SOUND_VERSION(x) \
{ \
    (x)->major = SOUND_VER_MAJOR; \
    (x)->minor = SOUND_VER_MINOR; \
    (x)->patch = SOUND_VER_PATCH; \
}


/**
 * \fn void Sound_GetLinkedVersion(Sound_Version *ver)
 * \brief Get the version of SDL_sound that is linked against your program.
 *
 * If you are using a shared library (DLL) version of SDL_sound, then it is
 *  possible that it will be different than the version you compiled against.
 *
 * This is a real function; the macro SOUND_VERSION tells you what version
 *  of SDL_sound you compiled against:
 *
 * \code
 * Sound_Version compiled;
 * Sound_Version linked;
 *
 * SOUND_VERSION(&compiled);
 * Sound_GetLinkedVersion(&linked);
 * printf("We compiled against SDL_sound version %d.%d.%d ...\n",
 *           compiled.major, compiled.minor, compiled.patch);
 * printf("But we linked against SDL_sound version %d.%d.%d.\n",
 *           linked.major, linked.minor, linked.patch);
 * \endcode
 *
 * This function may be called safely at any time, even before Sound_Init().
 *
 * \param ver Sound_Version structure to fill with shared library's version.
 *
 * \sa Sound_Version
 * \sa SOUND_VERSION
 */
SNDDECLSPEC void SDLCALL Sound_GetLinkedVersion(Sound_Version *ver);


/**
 * \fn Sound_Init(void)
 * \brief Initialize SDL_sound.
 *
 * This must be called before any other SDL_sound function (except perhaps
 *  Sound_GetLinkedVersion()). You should call SDL_Init() before calling this.
 *  Sound_Init() will attempt to call SDL_Init(SDL_INIT_AUDIO), just in case.
 *  This is a safe behaviour, but it may not configure SDL to your liking by
 *  itself.
 *
 * \return nonzero on success, zero on error. Specifics of the
 *         error can be gleaned from Sound_GetError().
 *
 * \sa Sound_Quit
 */
SNDDECLSPEC int SDLCALL Sound_Init(void);


/**
 * \fn Sound_Quit(void)
 * \brief Shutdown SDL_sound.
 *
 * This closes any SDL_RWops that were being used as sound sources, and frees
 *  any resources in use by SDL_sound.
 *
 * All Sound_Sample pointers you had prior to this call are INVALIDATED.
 *
 * Once successfully deinitialized, Sound_Init() can be called again to
 *  restart the subsystem. All default API states are restored at this
 *  point.
 *
 * You should call this BEFORE SDL_Quit(). This will NOT call SDL_Quit()
 *  for you!
 *
 *  \return nonzero on success, zero on error. Specifics of the error
 *          can be gleaned from Sound_GetError(). If failure, state of
 *          SDL_sound is undefined, and probably badly screwed up.
 *
 * \sa Sound_Init
 */
SNDDECLSPEC int SDLCALL Sound_Quit(void);


/**
 * \fn const Sound_DecoderInfo **Sound_AvailableDecoders(void)
 * \brief Get a list of sound formats supported by this version of SDL_sound.
 *
 * This is for informational purposes only. Note that the extension listed is
 *  merely convention: if we list "MP3", you can open an MPEG-1 Layer 3 audio
 *  file with an extension of "XYZ", if you like. The file extensions are
 *  informational, and only required as a hint to choosing the correct
 *  decoder, since the sound data may not be coming from a file at all, thanks
 *  to the abstraction that an SDL_RWops provides.
 *
 * The returned value is an array of pointers to Sound_DecoderInfo structures,
 *  with a NULL entry to signify the end of the list:
 *
 * \code
 * Sound_DecoderInfo **i;
 *
 * for (i = Sound_AvailableDecoders(); *i != NULL; i++)
 * {
 *     printf("Supported sound format: [%s], which is [%s].\n",
 *              i->extension, i->description);
 *     // ...and other fields...
 * }
 * \endcode
 *
 * The return values are pointers to static internal memory, and should
 *  be considered READ ONLY, and never freed.
 *
 * \return READ ONLY Null-terminated array of READ ONLY structures.
 *
 * \sa Sound_DecoderInfo
 */
SNDDECLSPEC const Sound_DecoderInfo ** SDLCALL Sound_AvailableDecoders(void);


/**
 * \fn const char *Sound_GetError(void)
 * \brief Get the last SDL_sound error message as a null-terminated string.
 *
 * This will be NULL if there's been no error since the last call to this
 *  function. The pointer returned by this call points to an internal buffer,
 *  and should not be deallocated. Each thread has a unique error state
 *  associated with it, but each time a new error message is set, it will
 *  overwrite the previous one associated with that thread. It is safe to call
 *  this function at anytime, even before Sound_Init().
 *
 * \return READ ONLY string of last error message.
 *
 * \sa Sound_ClearError
 */
SNDDECLSPEC const char * SDLCALL Sound_GetError(void);


/**
 * \fn void Sound_ClearError(void)
 * \brief Clear the current error message.
 *
 * The next call to Sound_GetError() after Sound_ClearError() will return NULL.
 *
 * \sa Sound_GetError
 */
SNDDECLSPEC void SDLCALL Sound_ClearError(void);


/**
 * \fn Sound_Sample *Sound_NewSample(SDL_RWops *rw, const char *ext, Sound_AudioInfo *desired, Uint32 bufferSize)
 * \brief Start decoding a new sound sample.
 *
 * The data is read via an SDL_RWops structure (see SDL_rwops.h in the SDL
 *  include directory), so it may be coming from memory, disk, network stream,
 *  etc. The (ext) parameter is merely a hint to determining the correct
 *  decoder; if you specify, for example, "mp3" for an extension, and one of
 *  the decoders lists that as a handled extension, then that decoder is given
 *  first shot at trying to claim the data for decoding. If none of the
 *  extensions match (or the extension is NULL), then every decoder examines
 *  the data to determine if it can handle it, until one accepts it. In such a
 *  case your SDL_RWops will need to be capable of rewinding to the start of
 *  the stream.
 *
 * If no decoders can handle the data, a NULL value is returned, and a human
 *  readable error message can be fetched from Sound_GetError().
 *
 * Optionally, a desired audio format can be specified. If the incoming data
 *  is in a different format, SDL_sound will convert it to the desired format
 *  on the fly. Note that this can be an expensive operation, so it may be
 *  wise to convert data before you need to play it back, if possible, or
 *  make sure your data is initially in the format that you need it in.
 *  If you don't want to convert the data, you can specify NULL for a desired
 *  format. The incoming format of the data, preconversion, can be found
 *  in the Sound_Sample structure.
 *
 * Note that the raw sound data "decoder" needs you to specify both the
 *  extension "RAW" and a "desired" format, or it will refuse to handle
 *  the data. This is to prevent it from catching all formats unsupported
 *  by the other decoders.
 *
 * Finally, specify an initial buffer size; this is the number of bytes that
 *  will be allocated to store each read from the sound buffer. The more you
 *  can safely allocate, the more decoding can be done in one block, but the
 *  more resources you have to use up, and the longer each decoding call will
 *  take. Note that different data formats require more or less space to
 *  store. This buffer can be resized via Sound_SetBufferSize() ...
 *
 * The buffer size specified must be a multiple of the size of a single
 *  sample point. So, if you want 16-bit, stereo samples, then your sample
 *  point size is (2 channels * 16 bits), or 32 bits per sample, which is four
 *  bytes. In such a case, you could specify 128 or 132 bytes for a buffer,
 *  but not 129, 130, or 131 (although in reality, you'll want to specify a
 *  MUCH larger buffer).
 *
 * When you are done with this Sound_Sample pointer, you can dispose of it
 *  via Sound_FreeSample().
 *
 * You do not have to keep a reference to (rw) around. If this function
 *  suceeds, it stores (rw) internally (and disposes of it during the call
 *  to Sound_FreeSample()). If this function fails, it will dispose of the
 *  SDL_RWops for you.
 *
 *    \param rw SDL_RWops with sound data.
 *    \param ext File extension normally associated with a data format.
 *               Can usually be NULL.
 *    \param desired Format to convert sound data into. Can usually be NULL,
 *                   if you don't need conversion.
 *    \param bufferSize Size, in bytes, to allocate for the decoding buffer.
 *   \return Sound_Sample pointer, which is used as a handle to several other
 *           SDL_sound APIs. NULL on error. If error, use
 *           Sound_GetError() to see what went wrong.
 *
 * \sa Sound_NewSampleFromFile
 * \sa Sound_SetBufferSize
 * \sa Sound_Decode
 * \sa Sound_DecodeAll
 * \sa Sound_Seek
 * \sa Sound_Rewind
 * \sa Sound_FreeSample
 */
SNDDECLSPEC Sound_Sample * SDLCALL Sound_NewSample(SDL_RWops *rw,
                                                   const char *ext,
                                                   Sound_AudioInfo *desired);

/**
 * \fn Sound_Sample *Sound_NewSampleFromFile(const char *filename, Sound_AudioInfo *desired, Uint32 bufferSize)
 * \brief Start decoding a new sound sample from a file on disk.
 *
 * This is identical to Sound_NewSample(), but it creates an SDL_RWops for you
 *  from the file located in (filename). Note that (filename) is specified in
 *  platform-dependent notation. ("C:\\music\\mysong.mp3" on windows, and
 *  "/home/icculus/music/mysong.mp3" or whatever on Unix, etc.)
 * Sound_NewSample()'s "ext" parameter is gleaned from the contents of
 *  (filename).
 *
 * This can pool RWops structures, so it may fragment the heap less over time
 *  than using SDL_RWFromFile().
 *
 *    \param filename file containing sound data.
 *    \param desired Format to convert sound data into. Can usually be NULL,
 *                   if you don't need conversion.
 *    \param bufferSize size, in bytes, of initial read buffer.
 *   \return Sound_Sample pointer, which is used as a handle to several other
 *           SDL_sound APIs. NULL on error. If error, use
 *           Sound_GetError() to see what went wrong.
 *
 * \sa Sound_NewSample
 * \sa Sound_SetBufferSize
 * \sa Sound_Decode
 * \sa Sound_DecodeAll
 * \sa Sound_Seek
 * \sa Sound_Rewind
 * \sa Sound_FreeSample
 */
SNDDECLSPEC Sound_Sample * SDLCALL Sound_NewSampleFromFile(const char *fname,
                                                      Sound_AudioInfo *desired);

/**
 * \fn void Sound_FreeSample(Sound_Sample *sample)
 * \brief Dispose of a Sound_Sample.
 *
 * This will also close/dispose of the SDL_RWops that was used at creation
 *  time, so there's no need to keep a reference to that around.
 * The Sound_Sample pointer is invalid after this call, and will almost
 *  certainly result in a crash if you attempt to keep using it.
 *
 *    \param sample The Sound_Sample to delete.
 *
 * \sa Sound_NewSample
 * \sa Sound_NewSampleFromFile
 */
SNDDECLSPEC void SDLCALL Sound_FreeSample(Sound_Sample *sample);


/**
 * \fn Sint32 Sound_GetDuration(Sound_Sample *sample)
 * \brief Retrieve total play time of sample, in milliseconds.
 *
 * Report total time length of sample, in milliseconds. This is a fast
 *  call. Duration is calculated during Sound_NewSample*, so this is just
 *  an accessor into otherwise opaque data.
 *
 * Please note that not all formats can determine a total time, some can't
 *  be exact without fully decoding the data, and thus will estimate the
 *  duration. Many decoders will require the ability to seek in the data
 *  stream to calculate this, so even if we can tell you how long an .ogg
 *  file will be, the same data set may fail if it's, say, streamed over an
 *  HTTP connection. Plan accordingly.
 *
 * Most people won't need this function to just decode and playback, but it
 *  can be useful for informational purposes in, say, a music player's UI.
 *
 *    \param sample Sound_Sample from which to retrieve duration information.
 *   \return Sample length in milliseconds, or -1 if duration can't be
 *           determined for any reason.
 */
SNDDECLSPEC Sint32 SDLCALL Sound_GetDuration(Sound_Sample *sample);

/**
 * \fn Uint32 Sound_Decode_Direct(Sound_Sample *sample)
 * \brief Decode more of the sound data in a Sound_Sample directly into
 * the supplied buffer.
 *
 * It will decode at most desired_frames into buffer, and return the number
 * frames decoded.
 * If the number of desired_frames could not be decoded, then please refer to
 *  sample->flags to determine if this was an end-of-stream or error condition.
 *
 *    \param sample Do more decoding to this Sound_Sample.
 *    \param buffer PCM frames into this buffer.
 *    \param desired_frames indicates how many PCM should be decoded.
 *   \return number of frames decoded into buffer. If it is less than
 *           desired_frames, then you should check sample->flags to see
 *           what the current state of the sample is (EOF, error, read again).
 *
 * \sa Sound_Seek
 * \sa Sound_Rewind
 */
SNDDECLSPEC Uint32 SDLCALL Sound_Decode_Direct(Sound_Sample *sample, void* buffer, Uint32 desired_frames);

/**
 * \fn int Sound_Rewind(Sound_Sample *sample)
 * \brief Rewind a sample to the start.
 *
 * Restart a sample at the start of its waveform data, as if newly
 *  created with Sound_NewSample(). If successful, the next call to
 *  Sound_Decode[All]() will give audio data from the earliest point
 *  in the stream.
 *
 * Beware that this function will fail if the SDL_RWops that feeds the
 *  decoder can not be rewound via it's seek method, but this can
 *  theoretically be avoided by wrapping it in some sort of buffering
 *  SDL_RWops.
 *
 * This function should ONLY fail if the RWops is not seekable, or
 *  SDL_sound is not initialized. Both can be controlled by the application,
 *  and thus, it is up to the developer's paranoia to dictate whether this
 *  function's return value need be checked at all.
 *
 * If this function fails, the state of the sample is undefined, but it
 *  is still safe to call Sound_FreeSample() to dispose of it.
 *
 * On success, ERROR, EOF, and EAGAIN are cleared from sample->flags. The
 *  ERROR flag is set on error.
 *
 *    \param sample The Sound_Sample to rewind.
 *   \return nonzero on success, zero on error. Specifics of the
 *           error can be gleaned from Sound_GetError().
 *
 * \sa Sound_Seek
 */
SNDDECLSPEC int SDLCALL Sound_Rewind(Sound_Sample *sample);


/**
 * \fn int Sound_Seek(Sound_Sample *sample, Uint32 ms)
 * \brief Seek to a different point in a sample.
 *
 * Reposition a sample's stream. If successful, the next call to
 *  Sound_Decode[All]() will give audio data from the offset you
 *  specified.
 *
 * The offset is specified in milliseconds from the start of the
 *  sample.
 *
 * Beware that this function can fail for several reasons. If the
 *  SDL_RWops that feeds the decoder can not seek, this call will almost
 *  certainly fail, but this can theoretically be avoided by wrapping it
 *  in some sort of buffering SDL_RWops. Some decoders can never seek,
 *  others can only seek with certain files. The decoders will set a flag
 *  in the sample at creation time to help you determine this.
 *
 * You should check sample->flags & SOUND_SAMPLEFLAG_CANSEEK
 *  before attempting. Sound_Seek() reports failure immediately if this
 *  flag isn't set. This function can still fail for other reasons if the
 *  flag is set.
 *
 * This function can be emulated in the application with Sound_Rewind()
 *  and predecoding a specific amount of the sample, but this can be
 *  extremely inefficient. Sound_Seek() accelerates the seek on a
 *  with decoder-specific code.
 *
 * If this function fails, the sample should continue to function as if
 *  this call was never made. If there was an unrecoverable error,
 *  sample->flags & SOUND_SAMPLEFLAG_ERROR will be set, which you regular
 *  decoding loop can pick up.
 *
 * On success, ERROR, EOF, and EAGAIN are cleared from sample->flags.
 *
 *    \param sample The Sound_Sample to seek.
 *    \param ms The new position, in milliseconds from start of sample.
 *   \return nonzero on success, zero on error. Specifics of the
 *           error can be gleaned from Sound_GetError().
 *
 * \sa Sound_Rewind
 */
SNDDECLSPEC int SDLCALL Sound_Seek(Sound_Sample *sample, Uint32 ms);


#ifdef __cplusplus
}
#endif

#endif  /* !defined _INCLUDE_SDL_SOUND_H_ */

/* end of SDL_sound.h ... */

