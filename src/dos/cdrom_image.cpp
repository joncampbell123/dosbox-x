/*
 *  Copyright (C) 2002-2020  The DOSBox Team
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

#include "cdrom.h"
#include <cassert>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <sstream>
#include <vector>
#include <sys/stat.h>

#if !defined(WIN32)
#include <libgen.h>
#else
#include <cstring>
#endif

#include "drives.h"
#include "support.h"
#include "setup.h"
#include "src/libs/decoders/SDL_sound.c"
#include "src/libs/decoders/vorbis.c"
#include "src/libs/decoders/flac.c"
#include "src/libs/decoders/wav.c"
#include "src/libs/decoders/mp3_seek_table.cpp"
#include "src/libs/decoders/mp3.cpp"

using namespace std;

// String maximums, local to this file
#define MAX_LINE_LENGTH 512
#define MAX_FILENAME_LENGTH 256

std::string get_basename(const std::string& filename) {
	// Guard against corner cases: '', '/', '\', 'a'
	if (filename.length() <= 1)
		return filename;

	// Find the last slash, but if not is set to zero
	size_t slash_pos = filename.find_last_of("/\\");

	// If the slash is the last character
	if (slash_pos == filename.length() - 1)
		slash_pos = 0;

	// Otherwise if the slash is found mid-string
	else if (slash_pos > 0)
		slash_pos++;
	return filename.substr(slash_pos);
}

// STL type shorteners, local to this file
using track_iter       = vector<CDROM_Interface_Image::Track>::iterator;
using track_const_iter = vector<CDROM_Interface_Image::Track>::const_iterator;
using tracks_size_t    = vector<CDROM_Interface_Image::Track>::size_type;

// Report bad seeks that would go beyond the end of the track
bool CDROM_Interface_Image::TrackFile::offsetInsideTrack(const uint32_t offset)
{
	if (static_cast<int>(offset) >= getLength()) {
		LOG_MSG("CDROM: attempted to seek to byte %u, beyond the "
		        "track's %d byte-length",
		        offset, length_redbook_bytes);
		return false;
	}
	return true;
}

// Trim requested read sizes that will spill beyond the track ending
uint32_t CDROM_Interface_Image::TrackFile::adjustOverRead(const uint32_t offset,
                                                          const uint32_t requested_bytes)
{
	// The most likely scenario is read is valid and no adjustment is needed
	uint32_t adjusted_bytes = requested_bytes;

	// If we seek beyond the end of the track, then we can't read any bytes...
	if (static_cast<int>(offset) >= getLength()) {
		adjusted_bytes = 0;
		LOG_MSG("CDROM: can't read audio because requested offset %u "
		        "is beyond the track length, %u",
		        offset, getLength());
	}
	// Otherwise if our seek + requested bytes goes beyond, then prune back
	// the request
	else if (static_cast<int>(offset + requested_bytes) > getLength()) {
		adjusted_bytes = static_cast<uint32_t>(getLength()) - offset;
		LOG_MSG("CDROM: reducing read-length by %u bytes to avoid "
		        "reading beyond track.",
		        requested_bytes - adjusted_bytes);
	}
	return adjusted_bytes;
}

CDROM_Interface_Image::BinaryFile::BinaryFile(const char *filename, bool &error)
        : TrackFile(BYTES_PER_RAW_REDBOOK_FRAME),
          file(nullptr)
{
	file = new ifstream(filename, ios::in | ios::binary);
	// If new fails, an exception is generated and scope leaves this constructor
	error = file->fail();
}

CDROM_Interface_Image::BinaryFile::~BinaryFile()
{
	// Guard: only cleanup if needed
	if (file == nullptr)
		return;

	delete file;
	file = nullptr;
}

bool CDROM_Interface_Image::BinaryFile::read(uint8_t *buffer,
                                             const uint32_t offset,
                                             const uint32_t requested_bytes)
{
	// Check for logic bugs and illegal values
	assertm(file && buffer, "The file and/or buffer pointer is invalid");
	assertm(offset <= MAX_REDBOOK_BYTES, "Requested offset exceeds CDROM size");
	assertm(requested_bytes <= MAX_REDBOOK_BYTES, "Requested bytes exceeds CDROM size");

	if (!seek(offset))
		return false;

	const uint32_t adjusted_bytes = adjustOverRead(offset, requested_bytes);
	if (adjusted_bytes == 0) // no work to do!
		return true;

	file->read((char *)buffer, adjusted_bytes);
	return !file->fail();
}

int CDROM_Interface_Image::BinaryFile::getLength()
{
	// Return our cached result if we've already been asked before
	if (length_redbook_bytes < 0 && file) {
		file->seekg(0, ios::end);
		/**
		 *  All read(..) operations involve an absolute position and
		 *  this function isn't called in other threads, therefore
		 *  we don't need to restore the original file position.
		 */
		length_redbook_bytes = static_cast<int>(file->tellg());

		assertm(length_redbook_bytes >= 0,
		        "Track length could not be determined");
		assertm(static_cast<uint32_t>(length_redbook_bytes) <= MAX_REDBOOK_BYTES,
		        "Track length exceeds the maximum CDROM size");
	}
	return length_redbook_bytes;
}

Bit16u CDROM_Interface_Image::BinaryFile::getEndian()
{
	// Image files are always little endian
	return AUDIO_S16LSB;
}

bool CDROM_Interface_Image::BinaryFile::seek(const uint32_t offset)
{
	// Check for logic bugs and illegal values
	assertm(file, "The file pointer needs to be valid, but is the nullptr");
	assertm(offset <= MAX_REDBOOK_BYTES, "Requested offset exceeds CDROM size");

	if (!offsetInsideTrack(offset))
		return false;

	file->seekg(offset, ios::beg);

	// If the first seek attempt failed, then try harder
	if (file->fail()) {
		file->clear();                 // clear fail and eof bits
		file->seekg(0, std::ios::beg); // "I have returned."
		file->seekg(offset, ios::beg); // "It will be done."
	}
	return !file->fail();
}

uint32_t CDROM_Interface_Image::BinaryFile::decode(int16_t *buffer,
                                                   const uint32_t desired_track_frames)
{
	// Guard against logic bugs and illegal values
	assertm(buffer && file, "The file pointer or buffer are invalid");
	assertm(desired_track_frames <= MAX_REDBOOK_FRAMES,
	        "Requested number of frames exceeds the maximum for a CDROM");

	file->read((char*)buffer, desired_track_frames * BYTES_PER_REDBOOK_PCM_FRAME);
	/**
	 *  Note: gcount returns a signed type, but according to specification:
	 *  "Except in the constructors of std::strstreambuf, negative values of
	 *  std::streamsize are never used."; so we store it as unsigned.
	 */
	const uint32_t bytes_read = static_cast<uint32_t>(file->gcount());

	// Return the number of decoded Redbook frames
	return ceil_udivide(bytes_read, BYTES_PER_REDBOOK_PCM_FRAME);
}

CDROM_Interface_Image::AudioFile::AudioFile(const char *filename, bool &error)
	: TrackFile(4096)
{
	// Use the audio file's sample rate and number of channels as-is
	Sound_AudioInfo desired = {AUDIO_S16, 0, 0};
	sample = Sound_NewSampleFromFile(filename, &desired);
	const std::string filename_only = get_basename(filename);
	if (sample) {
		error = false;
		LOG_MSG("CDROM: Loaded %s [%d Hz, %d-channel, %2.1f minutes]",
		        filename_only.c_str(), getRate(), getChannels(),
		        getLength() / static_cast<double>(REDBOOK_PCM_BYTES_PER_MIN));
	} else {
		LOG_MSG("CDROM: Failed adding '%s' as CDDA track!", filename_only.c_str());
		error = true;
	}
}

CDROM_Interface_Image::AudioFile::~AudioFile()
{
	// Guard to prevent double-free or nullptr free
	if (sample == nullptr)
		return;

	Sound_FreeSample(sample);
	sample = nullptr;
}

/**
 *  Seek takes in a Redbook CD-DA byte offset relative to the track's start
 *  time and returns true if the seek succeeded.
 * 
 *  When dealing with a raw bin/cue file, this requested byte position maps
 *  one-to-one with the bytes in raw binary image, as we see used in the
 *  BinaryTrack::seek() function.  However, when dealing with codec-based
 *  tracks, we need the codec's help to seek to the equivalent redbook position
 *  within the track, regardless of the track's sampling rate, bit-depth,
 *  or number of channels.  To do this, we convert the byte offset to a
 *  time-offset, and use the Sound_Seek() function to move the read position.
 */
bool CDROM_Interface_Image::AudioFile::seek(const uint32_t requested_pos)
{
	// Check for logic bugs and if the track is already positioned as requested
	assertm(sample, "Audio sample needs to be valid, but is the nullptr");
	assertm(requested_pos <= MAX_REDBOOK_BYTES, "Requested offset exceeds CDROM size");

	if (!offsetInsideTrack(requested_pos))
		return false;

	if (track_pos == requested_pos) {
		return true;
	}

	// Convert the position from a byte offset to time offset, in milliseconds.
	const uint32_t ms_per_s = 1000;
	const uint32_t pos_in_frames = ceil_udivide(requested_pos, BYTES_PER_RAW_REDBOOK_FRAME);
	const uint32_t pos_in_ms = ceil_udivide(pos_in_frames * ms_per_s, REDBOOK_FRAMES_PER_SECOND);

	// Perform the seek
	const bool result = Sound_Seek(sample, pos_in_ms);

	// Only store the track's new position if the seek was successful
	if (result)
		track_pos = requested_pos;
	return result;
}

bool CDROM_Interface_Image::AudioFile::read(uint8_t *buffer,
                                            const uint32_t requested_pos,
                                            const uint32_t requested_bytes)
{
	// Guard again logic bugs and the no-op case
	assertm(buffer != nullptr, "buffer needs to be allocated but is the nullptr");
	assertm(sample != nullptr, "Audio sample needs to be valid, but is the nullptr");
	assertm(requested_pos <= MAX_REDBOOK_BYTES, "Requested offset exceeds CDROM size");
	assertm(requested_bytes <= MAX_REDBOOK_BYTES,
	        "Requested bytes exceeds CDROM size");

	/**
	 *  Support DAE for stereo and mono 44.1 kHz tracks, otherwise inform the user.
	 *  Also, we allow up to 10 DAE-attempts because some CD Player software will query
	 *  this interface before using CDROM-directed playback (not DAE), therefore we
	 *  don't want to fail in those cases.
	 */
	if (getRate() != REDBOOK_PCM_FRAMES_PER_SECOND) {
		static uint8_t dae_attempts = 0;
		if (dae_attempts++ > 10) {
			E_Exit("\n"
			       "CDROM: Digital Audio Extration (DAE) was attempted with a %u kHz\n"
			       "       track, but DAE is only compatible with %u kHz tracks.",
			       getRate(),
			       REDBOOK_PCM_FRAMES_PER_SECOND);
		}
		return false; // we always correctly return false to the application in this case.
	}

	if (!seek(requested_pos))
		return false;

	const uint32_t adjusted_bytes = adjustOverRead(requested_pos, requested_bytes);
	if (adjusted_bytes == 0) // no work to do!
		return true;

	// Setup characteristics about our track and the request
	const uint8_t channels = getChannels();
	const uint8_t bytes_per_frame = channels * REDBOOK_BPS;
	const uint32_t requested_frames = ceil_udivide(adjusted_bytes,
	                                               BYTES_PER_REDBOOK_PCM_FRAME);

	uint32_t decoded_bytes = 0;
	uint32_t decoded_frames = 0;
	while (decoded_frames < requested_frames) {
		const uint32_t decoded = Sound_Decode_Direct(sample,
		                                             buffer + decoded_bytes,
		                                             requested_frames - decoded_frames);
		if (sample->flags & (SOUND_SAMPLEFLAG_ERROR | SOUND_SAMPLEFLAG_EOF) || !decoded)
			break;
		decoded_frames += decoded;
		decoded_bytes = decoded_frames * bytes_per_frame;
	}
	// Zero out any remainining frames that we didn't fill
	if (decoded_frames < requested_frames)
		memset(buffer + decoded_bytes, 0, adjusted_bytes - decoded_bytes);

	// If the track is mono, convert to stereo
	if (channels == 1 && decoded_frames) {
		/**
		 *  Convert to stereo in-place:
		 *  The current buffer is half full of mono samples:
		 *  0. [mmmmmmmm00000000]
		 * 
		 *  We walk backward from the last mono sample to the first,
		 *  duplicating them to left and right samples at the rear
		 *  of the buffer:
		 *  1. [mmmmmmm(m)000000(LR)]
		 *              ^________\/
		 * 
		 *  2. [mmmmmm(m)m0000(LR)LR]
		 *             ^_______\/
		 * 
		 *  Eventually the left and right samples overwrite the prior
		 *  mono samples until the buffer is full of LR samples (where
		 *  the first mono samples is simply the 'left' value):
		 * 
		 *  (N - 1). [(m)(R)LRLRLRLRLRLRLR]
		 *             ^_/
		 */
		int16_t* pcm_buffer = reinterpret_cast<int16_t*>(buffer);
		const uint32_t mono_samples = decoded_frames;
		for (uint32_t i = mono_samples - 1; i > 0; --i) {
			pcm_buffer[i * REDBOOK_CHANNELS + 1] = pcm_buffer[i]; // right
			pcm_buffer[i * REDBOOK_CHANNELS + 0] = pcm_buffer[i]; // left
		}

		// Taken into account that we've now fill both (stereo) channels
		decoded_bytes *= REDBOOK_CHANNELS;
	}
	// Increment the track's position by the Redbook-sized decoded bytes
	track_pos += decoded_bytes;
	return !(sample->flags & SOUND_SAMPLEFLAG_ERROR);
}

uint32_t CDROM_Interface_Image::AudioFile::decode(int16_t *buffer,
                                                  const uint32_t desired_track_frames)
{
	// Sound_Decode_Direct returns frames (agnostic of bitrate and channels)
	const uint32_t frames_decoded =
	    Sound_Decode_Direct(sample, (void*)buffer, desired_track_frames);

	// Increment the track's in terms of Redbook-equivalent bytes
	const uint32_t redbook_bytes = frames_decoded * BYTES_PER_REDBOOK_PCM_FRAME;
	track_pos += redbook_bytes;

	return frames_decoded;
}

Bit16u CDROM_Interface_Image::AudioFile::getEndian()
{
	return sample ? sample->actual.format : AUDIO_S16SYS;
}

Bit32u CDROM_Interface_Image::AudioFile::getRate()
{
	return sample ? sample->actual.rate : 0;
}

Bit8u CDROM_Interface_Image::AudioFile::getChannels()
{
	return sample ? sample->actual.channels : 0;
}

int CDROM_Interface_Image::AudioFile::getLength()
{
	if (length_redbook_bytes < 0 && sample) {
		/*
		 *  Sound_GetDuration returns milliseconds but getLength()
		 *  needs to return bytes, so we covert using PCM bytes/s
		 */
		length_redbook_bytes = static_cast<int>(static_cast<float>(Sound_GetDuration(sample)) * REDBOOK_PCM_BYTES_PER_MS);
	}
	assertm(length_redbook_bytes >= 0,
	        "Track length could not be determined");
	assertm(static_cast<uint32_t>(length_redbook_bytes) <= MAX_REDBOOK_BYTES,
	        "Track length exceeds the maximum CDROM size");
	return length_redbook_bytes;
}

// initialize static members
int CDROM_Interface_Image::refCount = 0;
CDROM_Interface_Image* CDROM_Interface_Image::images[26] = {};
CDROM_Interface_Image::imagePlayer CDROM_Interface_Image::player;

CDROM_Interface_Image::CDROM_Interface_Image(Bit8u subUnit)
	: tracks({}),
	  mcn("")
	  //subUnit(_subUnit)
{
	images[subUnit] = this;
	if (refCount == 0) {
		if (!player.mutex)
			player.mutex = SDL_CreateMutex();

		if (!player.channel) {
			player.channel = player.mixerChannel.Install(&CDAudioCallBack, 0, "CDAUDIO");
			player.channel->Enable(false); // only enabled during playback periods
		}
	}
	refCount++;
}

CDROM_Interface_Image::~CDROM_Interface_Image()
{
	refCount--;
	tracks.clear();
	// Stop playback before wiping out the CD Player
	if (refCount == 0 && player.cd) {
		StopAudio();
		SDL_DestroyMutex(player.mutex);
		player.mutex = nullptr;
		if (player.channel) {
			player.channel->Enable(false);
			MIXER_DelChannel(player.channel);
			player.channel = nullptr;
		}
	}
	if (player.cd == this) {
		player.cd = nullptr;
	}
}

bool CDROM_Interface_Image::SetDevice(char* path, int forceCD)
{
	(void)forceCD;//UNUSED
	const bool result = LoadCueSheet(path) || LoadIsoFile(path);
	if (!result) {
		// print error message on dosbox console
		char buf[MAX_LINE_LENGTH];
		snprintf(buf, MAX_LINE_LENGTH, "Could not load image file: %s\r\n", path);
		Bit16u size = (Bit16u)strlen(buf);
		DOS_WriteFile(STDOUT, (Bit8u*)buf, &size);
	}
	return result;
}

bool CDROM_Interface_Image::GetUPC(unsigned char& attr, char* upc)
{
	attr = 0;
	strcpy(upc, this->mcn.c_str());
	return true;
}

bool CDROM_Interface_Image::GetAudioTracks(int& start_track_num,
                                           int& end_track_num,
                                           TMSF& lead_out_msf)
{
	/**
	 *  Guard: A valid CD has atleast two tracks: the first plus the lead-out,
	 *  so bail out if we have fewer than 2 tracks
	 */
	if (tracks.size() < MIN_REDBOOK_TRACKS) {
		return false;
	}
	start_track_num = tracks.front().number;
	end_track_num = next(tracks.crbegin())->number; // next(crbegin) == [vec.size - 2]
	lead_out_msf = frames_to_msf(tracks.back().start + REDBOOK_FRAME_PADDING);
	return true;
}

bool CDROM_Interface_Image::GetAudioTrackInfo(int requested_track_num,
                                              TMSF& start_msf,
                                              unsigned char& attr)
{
	if (tracks.size() < MIN_REDBOOK_TRACKS
	    || requested_track_num < 1
	    || requested_track_num > 99
	    || requested_track_num >= tracks.size()) {
		return false;
	}

	const int requested_track_index = static_cast<int>(requested_track_num) - 1;
	track_const_iter track = tracks.begin() + requested_track_index;
	start_msf = frames_to_msf(track->start + REDBOOK_FRAME_PADDING);
	attr = track->attr;
	return true;
}

bool CDROM_Interface_Image::GetAudioSub(unsigned char& attr,
                                        unsigned char& track_num,
                                        unsigned char& index,
                                        TMSF& relative_msf,
                                        TMSF& absolute_msf) {
	// Setup valid defaults to handle all scenarios
	attr = 0;
	track_num = 1;
	index = 1;
	uint32_t absolute_sector = 0;
	uint32_t relative_sector = 0;

	if (!tracks.empty()) { 	// We have a useable CD; get a valid play-position
		track_iter track = tracks.begin();
		// the CD's current track is valid

		 // reserve the track_file as a shared_ptr to avoid deletion in another thread
		const auto track_file = player.trackFile.lock();
		if (track_file) {
			const uint32_t sample_rate = track_file->getRate();
			const uint32_t played_frames = ceil_udivide(player.playedTrackFrames
			                               * REDBOOK_FRAMES_PER_SECOND, sample_rate);
			absolute_sector = player.startSector + played_frames;
			track_iter current_track = GetTrack(absolute_sector);
			if (current_track != tracks.end()) {
				track = current_track;
				relative_sector = absolute_sector >= track->start ?
				                  absolute_sector - track->start : 0;
			} else { // otherwise fallback to the beginning track
				absolute_sector = track->start;
				// relative_sector is zero because we're at the start of the track
			}
		// the CD hasn't been played yet or has an invalid track_pos
		} else {
			for (track_iter it = tracks.begin(); it != tracks.end(); ++it) {
				if (it->attr == 0) {	// Found an audio track
					track = it;
					absolute_sector = it->start;
					break;
				} // otherwise fallback to the beginning track
			}
		}
		attr = track->attr;
		track_num = track->number;
	}
	absolute_msf = frames_to_msf(absolute_sector + REDBOOK_FRAME_PADDING);
	relative_msf = frames_to_msf(relative_sector);
	return true;
}

bool CDROM_Interface_Image::GetAudioStatus(bool& playing, bool& pause)
{
	playing = player.isPlaying;
	pause = player.isPaused;

	return true;
}

bool CDROM_Interface_Image::GetMediaTrayStatus(bool& mediaPresent, bool& mediaChanged, bool& trayOpen)
{
	mediaPresent = true;
	mediaChanged = false;
	trayOpen = false;
	return true;
}

bool CDROM_Interface_Image::PlayAudioSector(unsigned long start, unsigned long len)
{
	// Find the track that holds the requested sector
	track_const_iter track = GetTrack(start);
	std::shared_ptr<TrackFile> track_file;
	if (track != tracks.end())
		track_file = track->file;

	// Guard: sanity check the request beyond what GetTrack already checks
	if (len == 0
	   || track == tracks.end()
	   || !track_file
	   || track->attr == 0x40
	   || !player.channel
	   || !player.mutex) {
		StopAudio();
		return false;
	}
	/**
	 *  If the request falls in the pregap we deduct the difference from the
	 *  playback duration, because we skip the pre-gap area and jump straight to
	 *  the track start.
	 */
	if (start < track->start)
		len -= (track->start - start);

	// Seek to the calculated byte offset, bounded to the valid byte offsets
	const uint32_t offset = (track->skip
	                        + clamp((const uint32_t)start - static_cast<uint32_t>(track->start),
	                                0u, track->length - 1)
	                        * track->sectorSize);

	// Guard: Bail if our track could not be seeked
	if (!track_file->seek(offset)) {
		LOG_MSG("CDROM: Track %d failed to seek to byte %u, so cancelling playback",
		        track->number,
		        offset);
		StopAudio();
		return false;
	}

	// Get properties about the current track
	const uint8_t track_channels = track_file->getChannels();
	const uint32_t track_rate = track_file->getRate();

	/**
	 *  Guard: Before we update our player object with new track details, we
	 *  lock access to it to prevent the Callback (which runs in a separate
	 *  thread) from getting inconsistent or partial values.
	 */
	if (SDL_LockMutex(player.mutex) < 0) {
		LOG_MSG("CDROM: PlayAudioSector couldn't lock our player for exclusive access");
		StopAudio();
		return false;
	}

	// Update our player with properties about this playback sequence
	player.cd = this;
	player.trackFile = track_file;
	player.startSector = start;
	player.totalRedbookFrames = len;
	player.isPlaying = true;
	player.isPaused = false;

	// Assign the mixer function associated with this track's content type
	if (track_file->getEndian() == AUDIO_S16SYS) {
		player.addFrames = track_channels ==  2  ? &MixerChannel::AddSamples_s16 \
		                                         : &MixerChannel::AddSamples_m16;
	} else {
		player.addFrames = track_channels ==  2  ? &MixerChannel::AddSamples_s16_nonnative \
		                                         : &MixerChannel::AddSamples_m16_nonnative;
	}

	/**
	 *  Convert Redbook frames (len) to Track PCM frames, rounding up to whole
	 *  integer frames. Note: the intermediate numerator in the calculation
	 *  below can overflow uint32_t, so the variable types used must stay
	 *  64-bit.
	 */
	player.playedTrackFrames = 0;
	player.totalTrackFrames = ceil_udivide(track_rate * player.totalRedbookFrames,
	                                      REDBOOK_FRAMES_PER_SECOND);

	// start the channel!
	player.channel->SetFreq(track_rate);
	player.channel->Enable(true);

	// Guard: release the lock in this data
    if (SDL_UnlockMutex(player.mutex) < 0) {
        LOG_MSG("CDROM: PlayAudioSector couldn't unlock this thread");
		StopAudio();
		return false;
    }
	return true;
}

bool CDROM_Interface_Image::PauseAudio(bool resume)
{
	player.isPaused = !resume;
	if (player.channel)
		player.channel->Enable(resume);
	return true;
}

bool CDROM_Interface_Image::StopAudio(void)
{
	player.isPlaying = false;
	player.isPaused = false;
	if (player.channel)
		player.channel->Enable(false);
	return true;
}

void CDROM_Interface_Image::ChannelControl(TCtrl ctrl)
{
	// Guard: Bail if our mixer channel hasn't been allocated
	if (!player.channel)
		return;

	player.ctrlUsed = (ctrl.out[0]!=0 || ctrl.out[1]!=1 || ctrl.vol[0]<0xfe || ctrl.vol[1]<0xfe);
	player.ctrlData = ctrl;

	// Adjust the volume of our mixer channel as defined by the application
	player.channel->SetScale(static_cast<float>(ctrl.vol[0]),  // left vol
	                         static_cast<float>(ctrl.vol[1])); // right vol
}

bool CDROM_Interface_Image::ReadSectors(PhysPt buffer,
                                        const bool raw,
                                        unsigned long sector,
                                        unsigned long num)
{
	const uint16_t sectorSize = (raw ? BYTES_PER_RAW_REDBOOK_FRAME
	                                 : BYTES_PER_COOKED_REDBOOK_FRAME);
	const uint32_t requested_bytes = num * sectorSize;

	// Resize our underlying vector if it's not big enough
	if (readBuffer.size() < requested_bytes)
		readBuffer.resize(requested_bytes);

	// Setup state-tracking variables to be used in the read-loop
	bool success = true; //Gobliiins reads 0 sectors
	uint32_t bytes_read = 0;
	uint32_t current_sector = sector;
	uint8_t* buffer_position = readBuffer.data();

	// Read until we have enough or fail
	while (bytes_read < requested_bytes) {
		success = ReadSector(buffer_position, raw, current_sector);
		if (!success)
			break;
		current_sector++;
		bytes_read += sectorSize;
		buffer_position += sectorSize;
	}
	// Write only the successfully read bytes
	MEM_BlockWrite(buffer, readBuffer.data(), bytes_read);
	return success;
}

bool CDROM_Interface_Image::ReadSectorsHost(void *buffer, bool raw, unsigned long sector, unsigned long num)
{
	unsigned int sectorSize = raw ? RAW_SECTOR_SIZE : COOKED_SECTOR_SIZE;
	bool success = true; //Gobliiins reads 0 sectors
	for(unsigned long i = 0; i < num; i++) {
		success = ReadSector((Bit8u*)buffer + (i * (Bitu)sectorSize), raw, sector + i);
		if (!success) break;
	}

	return success;
}

bool CDROM_Interface_Image::LoadUnloadMedia(bool unload)
{
	(void)unload; // unused by part of the API
	return true;
}

track_iter CDROM_Interface_Image::GetTrack(const uint32_t sector)
{
	// Guard if we have no tracks or the sector is beyond the lead-out
	if (sector > MAX_REDBOOK_SECTOR ||
	    tracks.size() < MIN_REDBOOK_TRACKS ||
	    sector >= tracks.back().start) {
		LOG_MSG("CDROM: GetTrack at sector %u is outside the"
		        " playable range", sector);
		return tracks.end();
	}

	/**
	 *  Walk the tracks checking if the desired sector falls inside of a given
	 *  track's range, which starts at the end of the prior track and goes to
	 *  the current track's (start + length).
	 */
	track_iter track = tracks.begin();
	uint32_t lower_bound = track->start;
	while (track != tracks.end()) {
		const uint32_t upper_bound = track->start + track->length;
		if (lower_bound <= sector && sector < upper_bound) {
			break;
		}
		++track;
		lower_bound = upper_bound;
	}

	return track;
}

bool CDROM_Interface_Image::ReadSector(uint8_t *buffer, bool raw, const uint32_t sector)
{
	track_const_iter track = GetTrack(sector);

	// Guard: Bail if the requested sector fell outside our tracks
	if (track == tracks.end() || track->file == nullptr) {
		return false;
	}
	uint32_t offset = track->skip + (sector - track->start) * track->sectorSize;
	const uint16_t length = (raw ? BYTES_PER_RAW_REDBOOK_FRAME : BYTES_PER_COOKED_REDBOOK_FRAME);
	if (track->sectorSize != BYTES_PER_RAW_REDBOOK_FRAME && raw) {
		return false;
	}
	if (track->sectorSize == BYTES_PER_RAW_REDBOOK_FRAME && !track->mode2 && !raw)
		offset += 16;
	if (track->mode2 && !raw)
		offset += 24;

	return track->file->read(buffer, offset, length);
}


void CDROM_Interface_Image::CDAudioCallBack(Bitu desired_track_frames)
{
	/**
	 *  This callback runs in SDL's mixer thread, so there's a risk
	 *  our track_file pointer could be removed by the main thread.
	 *  We reserve the track_file up-front for the scope of this call.
	 */
	std::shared_ptr<TrackFile> track_file = player.trackFile.lock();

	// Guards: Bail if the request or our player is invalid
	if (desired_track_frames == 0
	   || !player.cd
	   || !player.mutex
	   || !track_file) {

		if (player.cd)
			player.cd->StopAudio();
		return;
	}

	// Ensure we have exclusive access to update our player members
	if (SDL_LockMutex(player.mutex) < 0) {
		LOG_MSG("CDROM: CDAudioCallBack couldn't lock this thread");
		return;
	}

	const uint32_t decoded_track_frames = track_file->decode(player.buffer,
	                                                         static_cast<uint32_t>(desired_track_frames));
	player.playedTrackFrames += decoded_track_frames;

	/**
	 *  Uses either the stereo or mono and native or nonnative
	 *  AddSamples call assigned during construction
	 */
	(player.channel->*player.addFrames)(decoded_track_frames, player.buffer);

	if (player.playedTrackFrames >= player.totalTrackFrames) {
		player.cd->StopAudio();

	} else if (decoded_track_frames == 0) {
		// Our track has run dry but we still have more music left to play!
		const double percent_played = static_cast<double>(
		                              player.playedTrackFrames)
		                              / player.totalTrackFrames;
		const Bit32u played_redbook_frames = static_cast<Bit32u>(ceil(
		                                     percent_played
		                                     * player.totalRedbookFrames));
		const Bit32u new_redbook_start_frame = player.startSector
		                                       + played_redbook_frames;
		const Bit32u remaining_redbook_frames = player.totalRedbookFrames
		                                        - played_redbook_frames;
		if (SDL_UnlockMutex(player.mutex) < 0) {
			LOG_MSG("CDROM: CDAudioCallBack couldn't unlock to move to next track");
			return;
		}
		player.cd->PlayAudioSector(new_redbook_start_frame, remaining_redbook_frames);
		return;
	}
	if (SDL_UnlockMutex(player.mutex) < 0) {
		LOG_MSG("CDROM: CDAudioCallBack couldn't unlock our player before returning");
	}
}

bool CDROM_Interface_Image::LoadIsoFile(char* filename)
{
	tracks.clear();

	// data track (track 1)
	Track track;
	bool error;
	track.file = make_shared<BinaryFile>(filename, error);

	if (error) {
		return false;
	}
	track.number = 1;
	track.attr = 0x40;//data

	// try to detect iso type
	if (CanReadPVD(track.file.get(), BYTES_PER_COOKED_REDBOOK_FRAME, false)) {
		track.sectorSize = BYTES_PER_COOKED_REDBOOK_FRAME;
		assert(track.mode2 == false);
	} else if (CanReadPVD(track.file.get(), BYTES_PER_RAW_REDBOOK_FRAME, false)) {
		track.sectorSize = BYTES_PER_RAW_REDBOOK_FRAME;
		assert(track.mode2 == false);
	} else if (CanReadPVD(track.file.get(), 2336, true)) {
		track.sectorSize = 2336;
		track.mode2 = true;
	} else if (CanReadPVD(track.file.get(), BYTES_PER_RAW_REDBOOK_FRAME, true)) {
		track.sectorSize = BYTES_PER_RAW_REDBOOK_FRAME;
		track.mode2 = true;
	} else {
		return false;
	}
	const int32_t track_bytes = track.file->getLength();
	if (track_bytes < 0)
		return false;

	track.length = static_cast<uint32_t>(track_bytes) / track.sectorSize;

	tracks.push_back(track);

	// lead-out track (track 2)
	Track leadout_track;
	leadout_track.number = 2;
	leadout_track.start = track.length;
	tracks.push_back(leadout_track);
	return true;
}

bool CDROM_Interface_Image::CanReadPVD(TrackFile *file,
                                       const uint16_t sectorSize,
                                       const bool mode2)
{
	// Guard: Bail if our file pointer is empty
	if (file == nullptr) return false;

	// Initialize our array in the event file->read() doesn't fully write it
	Bit8u pvd[BYTES_PER_COOKED_REDBOOK_FRAME] = {0};

	uint32_t seek = 16 * sectorSize;  // first vd is located at sector 16
	if (sectorSize == BYTES_PER_RAW_REDBOOK_FRAME && !mode2) seek += 16;
	if (mode2) seek += 24;
	file->read(pvd, seek, BYTES_PER_COOKED_REDBOOK_FRAME);
	// pvd[0] = descriptor type, pvd[1..5] = standard identifier,
	// pvd[6] = iso version (+8 for High Sierra)
	return ((pvd[0] == 1 && !strncmp((char*)(&pvd[1]), "CD001", 5) && pvd[6]  == 1) ||
	        (pvd[8] == 1 && !strncmp((char*)(&pvd[9]), "CDROM", 5) && pvd[14] == 1));
}

#if defined(WIN32)
static string dirname(char * file) {
	char * sep = strrchr(file, '\\');
	if (sep == nullptr)
		sep = strrchr(file, '/');
	if (sep == nullptr)
		return "";
	else {
		int len = (int)(sep - file);
		char tmp[MAX_FILENAME_LENGTH];
		safe_strncpy(tmp, file, len+1);
		return tmp;
	}
}
#endif

bool CDROM_Interface_Image::LoadCueSheet(char *cuefile)
{
	tracks.clear();

	Track track;
	uint32_t shift = 0;
	uint32_t currPregap = 0;
	uint32_t totalPregap = 0;
	int32_t prestart = -1;
	int track_number;
	bool success;
	bool canAddTrack = false;
	char tmp[MAX_FILENAME_LENGTH];  // dirname can change its argument
	safe_strncpy(tmp, cuefile, MAX_FILENAME_LENGTH);
	string pathname(dirname(tmp));
	ifstream in;
	in.open(cuefile, ios::in);
	if (in.fail()) {
		return false;
	}

	while (!in.eof()) {
		// get next line
		char buf[MAX_LINE_LENGTH];
		in.getline(buf, MAX_LINE_LENGTH);
		if (in.fail() && !in.eof()) {
			return false;  // probably a binary file
		}
		istringstream line(buf);

		string command;
		GetCueKeyword(command, line);

		if (command == "TRACK") {
			if (canAddTrack) success = AddTrack(track, shift, prestart, totalPregap, currPregap);
			else success = true;

			track.start = 0;
			track.skip = 0;
			currPregap = 0;
			prestart = -1;

			line >> track_number; // (cin) read into a true int first
			track.number = static_cast<uint8_t>(track_number);
			string type;
			GetCueKeyword(type, line);

			if (type == "AUDIO") {
				track.sectorSize = BYTES_PER_RAW_REDBOOK_FRAME;
				track.attr = 0;
				track.mode2 = false;
			} else if (type == "MODE1/2048") {
				track.sectorSize = BYTES_PER_COOKED_REDBOOK_FRAME;
				track.attr = 0x40;
				track.mode2 = false;
			} else if (type == "MODE1/2352") {
				track.sectorSize = BYTES_PER_RAW_REDBOOK_FRAME;
				track.attr = 0x40;
				track.mode2 = false;
			} else if (type == "MODE2/2336") {
				track.sectorSize = 2336;
				track.attr = 0x40;
				track.mode2 = true;
			} else if (type == "MODE2/2352") {
				track.sectorSize = BYTES_PER_RAW_REDBOOK_FRAME;
				track.attr = 0x40;
				track.mode2 = true;
			} else success = false;

			canAddTrack = true;
		}
		else if (command == "INDEX") {
			int index;
			line >> index;
			uint32_t frame;
			success = GetCueFrame(frame, line);

			if (index == 1) track.start = frame;
			else if (index == 0) prestart = static_cast<int32_t>(frame);
			// ignore other indices
		}
		else if (command == "FILE") {
			if (canAddTrack) success = AddTrack(track, shift, prestart, totalPregap, currPregap);
			else success = true;
			canAddTrack = false;

			string filename;
			GetCueString(filename, line);
			GetRealFileName(filename, pathname);
			string type;
			GetCueKeyword(type, line);

			bool error = true;
			if (type == "BINARY") {
				track.file = make_shared<BinaryFile>(filename.c_str(), error);
			}
			else {
				track.file = make_shared<AudioFile>(filename.c_str(), error);
				/**
				 *  SDL_Sound first tries using a decoder having a matching
				 *  registered extension as the filename, and then falls back to
				 *  trying each decoder before finally giving up.
				 */
			}
			if (error) {
				success = false;
			}
		}
		else if (command == "PREGAP") success = GetCueFrame(currPregap, line);
		else if (command == "CATALOG") success = GetCueString(mcn, line);
		// ignored commands
		else if (command == "CDTEXTFILE" || command == "FLAGS"   || command == "ISRC" ||
		         command == "PERFORMER"  || command == "POSTGAP" || command == "REM" ||
		         command == "SONGWRITER" || command == "TITLE"   || command.empty()) {
			success = true;
		}
		// failure
		else {
			success = false;
		}
		if (!success) {
			return false;
		}
	}
	// add last track
	if (!AddTrack(track, shift, prestart, totalPregap, currPregap)) {
		return false;
	}

	// add lead-out track
	track.number++;
	track.attr = 0;//sync with load iso
	track.start = 0;
	track.length = 0;
	track.file.reset();
	if (!AddTrack(track, shift, -1, totalPregap, 0)) {
		return false;
	}
	return true;
}



bool CDROM_Interface_Image::AddTrack(Track &curr,
                                     uint32_t &shift,
                                     const int32_t prestart,
                                     uint32_t &totalPregap,
                                     uint32_t currPregap)
{
	uint32_t skip = 0;

	// frames between index 0(prestart) and 1(curr.start) must be skipped
	if (prestart >= 0) {
		if (prestart > static_cast<int>(curr.start)) {
			LOG_MSG("CDROM: AddTrack => prestart %d cannot be > curr.start %u",
			        prestart, curr.start);
			return false;
		}
		skip = static_cast<uint32_t>(static_cast<int>(curr.start) - prestart);
	}

	// Add the first track, if our vector is empty
	if (tracks.empty()) {
		assertm(curr.number == 1, "The first track must be labelled number 1 [BUG!]");
		curr.skip = skip * curr.sectorSize;
		curr.start += currPregap;
		totalPregap = currPregap;
		tracks.push_back(curr);
		return true;
	}

	// Guard against undefined behavior in subsequent tracks.back() call
	assert(!tracks.empty());
	Track &prev = tracks.back();

	// current track consumes data from the same file as the previous
	if (prev.file == curr.file) {
		curr.start += shift;
		if (!prev.length) {
			prev.length = curr.start + totalPregap - prev.start - skip;
		}
		curr.skip += prev.skip + prev.length * prev.sectorSize + skip * curr.sectorSize;
		totalPregap += currPregap;
		curr.start += totalPregap;
	// current track uses a different file as the previous track
	} else {
		const uint32_t tmp = static_cast<uint32_t>
		                     (prev.file->getLength()) - prev.skip;
		prev.length = tmp / prev.sectorSize;
		if (tmp % prev.sectorSize != 0)
			prev.length++; // padding

		curr.start += prev.start + prev.length + currPregap;
		curr.skip = skip * curr.sectorSize;
		shift += prev.start + prev.length;
		totalPregap = currPregap;
	}
	// error checks
	if (curr.number <= 1
	    || prev.number + 1 != curr.number
	    || curr.start < prev.start + prev.length) {
		LOG_MSG("AddTrack: failed consistency checks\n"
		"\tcurr.number (%d) <= 1\n"
		"\tprev.number (%d) + 1 != curr.number (%d)\n"
		"\tcurr.start (%d) < prev.start (%d) + prev.length (%d)\n",
		curr.number, prev.number, curr.number,
		curr.start, prev.start, prev.length);
		return false;
	}

	tracks.push_back(curr);
	return true;
}

bool CDROM_Interface_Image::HasDataTrack(void)
{
	//Data track has attribute 0x40
	for (const auto &track : tracks) {
		if (track.attr == 0x40) {
			return true;
		}
	}
	return false;
}


bool CDROM_Interface_Image::GetRealFileName(string &filename, string &pathname)
{
	// check if file exists
	struct stat test;
	if (stat(filename.c_str(), &test) == 0) {
		return true;
	}

	// check if file with path relative to cue file exists
	string tmpstr(pathname + "/" + filename);
	if (stat(tmpstr.c_str(), &test) == 0) {
		filename = tmpstr;
		return true;
	}
	// finally check if file is in a dosbox local drive
	char fullname[CROSS_LEN];
	char tmp[CROSS_LEN];
	safe_strncpy(tmp, filename.c_str(), CROSS_LEN);
	Bit8u drive;
	if (!DOS_MakeName(tmp, fullname, &drive)) {
		return false;
	}

	localDrive *ldp = dynamic_cast<localDrive*>(Drives[drive]);
	if (ldp) {
		ldp->GetSystemFilename(tmp, fullname);
		if (stat(tmp, &test) == 0) {
			filename = tmp;
			return true;
		}
	}

#if !defined (WIN32)
	/**
	 *  Consider the possibility that the filename has a windows directory
	 *  seperator (inside the CUE file) which is common for some commercial
	 *  rereleases of DOS games using DOSBox
	 */
	string copy = filename;
	size_t l = copy.size();
	for (size_t i = 0; i < l;i++) {
		if (copy[i] == '\\') copy[i] = '/';
	}

	if (stat(copy.c_str(), &test) == 0) {
		filename = copy;
		return true;
	}

	tmpstr = pathname + "/" + copy;
	if (stat(tmpstr.c_str(), &test) == 0) {
		filename = tmpstr;
		return true;
	}
#endif
	return false;
}

bool CDROM_Interface_Image::GetCueKeyword(string &keyword, istream &in)
{
	in >> keyword;
	for (Bitu i = 0; i < keyword.size(); i++) {
		keyword[i] = static_cast<char>(toupper(keyword[i]));
	}
	return true;
}

bool CDROM_Interface_Image::GetCueFrame(uint32_t &frames, istream &in)
{
	std::string msf;
	in >> msf;
	TMSF tmp = {0, 0, 0};
	bool success = sscanf(msf.c_str(), "%hhu:%hhu:%hhu", &tmp.min, &tmp.sec, &tmp.fr) == 3;
	frames = (int)MSF_TO_FRAMES(tmp.min, tmp.sec, tmp.fr);
	return success;
}

bool CDROM_Interface_Image::GetCueString(string &str, istream &in)
{
	int pos = (int)in.tellg();
	in >> str;
	if (str[0] == '\"') {
		if (str[str.size() - 1] == '\"') {
			str.assign(str, 1, str.size() - 2);
		} else {
			in.seekg(pos, ios::beg);
			char buffer[MAX_FILENAME_LENGTH];
			in.getline(buffer, MAX_FILENAME_LENGTH, '\"');	// skip
			in.getline(buffer, MAX_FILENAME_LENGTH, '\"');
			str = buffer;
		}
	}
	return true;
}

void CDROM_Image_ShutDown(Section* /*sec*/) {
	Sound_Quit();
}

void CDROM_Image_Init() {
	Sound_Init();
	AddExitFunction(AddExitFunctionFuncPair(CDROM_Image_ShutDown));
}
