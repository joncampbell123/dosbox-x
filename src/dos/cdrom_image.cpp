/*
 *  Copyright (C) 2002-2021  The DOSBox Team
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

#if defined(WORDS_BIGENDIAN)
#define IS_BIGENDIAN true
#else
#define IS_BIGENDIAN false
#endif

#include "drives.h"
#include "support.h"
#include "setup.h"
#include "src/libs/decoders/audio_convert.c"
#include "src/libs/decoders/SDL_sound.c"
#include "src/libs/decoders/vorbis.c"
#include "src/libs/decoders/flac.c"
#include "src/libs/decoders/opus.c"
#include "src/libs/decoders/wav.c"
#include "src/libs/decoders/mp3_seek_table.cpp"
#include "src/libs/decoders/mp3.cpp"
#include "src/libs/libchdr/chd.h"
#include "src/libs/libchdr/libchdr_chd.c"
#include "src/libs/libchdr/libchdr_cdrom.c"
#include "src/libs/libchdr/libchdr_flac.c"
#include "src/libs/libchdr/libchdr_huffman.c"
#include "src/libs/libchdr/libchdr_bitstream.c"
#include "src/libs/libchdr/FLAC/stream_decoder.c"
#include "src/libs/libchdr/FLAC/bitreader.c"
#include "src/libs/libchdr/FLAC/format.c"
#include "src/libs/libchdr/FLAC/cpu.c"
#include "src/libs/libchdr/FLAC/crc.c"
#include "src/libs/libchdr/FLAC/fixed.c"
#include "src/libs/libchdr/FLAC/lpc.c"
#include "src/libs/libchdr/FLAC/md5.c"
#include "src/libs/libchdr/FLAC/memory.c"
#if defined(WIN32) && !defined(HX_DOS)
#include "src/libs/libchdr/FLAC/windows_unicode_filenames.c"
#endif
#include "src/libs/libchdr/lzma/LzmaDec.c"
#include "src/libs/libchdr/lzma/LzmaEnc.c"
#include "src/libs/libchdr/lzma/LzFind.c"

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
CDROM_Interface_Image::BinaryFile::BinaryFile(const char *filename, bool &error)
        :TrackFile(RAW_SECTOR_SIZE)
{
	file = new ifstream(filename, ios::in | ios::binary);
	// If new fails, an exception is generated and scope leaves this constructor
	error = file->fail();
#if defined(WIN32) && !defined(__MINGW32__) // Wengier: disable for MinGW for now but it appears to work on my MinGW version
    if (error) {
        typedef wchar_t host_cnv_char_t;
        host_cnv_char_t *CodePageGuestToHost(const char *s);
        const host_cnv_char_t* host_name = CodePageGuestToHost(filename);
        if (host_name != NULL) {
            file = new ifstream(host_name, ios::in | ios::binary);
            error = file->fail();
        }
    }
#endif
}

CDROM_Interface_Image::BinaryFile::~BinaryFile()
{
	// Guard: only cleanup if needed
	if (file == nullptr)
		return;

	delete file;
	file = nullptr;
}

bool CDROM_Interface_Image::BinaryFile::read(uint8_t *buffer,int64_t offset, int count)
{
    if (!seek(offset)) return false;
	file->seekg((streampos)offset, ios::beg);
	file->read((char*)buffer, count);
	return !(file->fail());
}

int64_t CDROM_Interface_Image::BinaryFile::getLength()
{
	file->seekg(0, ios::end);
	int64_t length = (int64_t)file->tellg();
	if (file->fail()) return -1;
	return length;
}

uint16_t CDROM_Interface_Image::BinaryFile::getEndian()
{
	// Image files are read into native-endian byte-order
	#if defined(WORDS_BIGENDIAN)
	return AUDIO_S16MSB;
	#else
	return AUDIO_S16LSB;
	#endif
}

bool CDROM_Interface_Image::BinaryFile::seek(int64_t offset)
{
	const auto pos = static_cast<std::streamoff>(offset);
	if (file->tellg() == pos)
		return true;

	file->seekg(pos, std::ios::beg);

	// If the first seek attempt failed, then try harder
	if (file->fail()) {
		file->clear();                   // clear fail and eof bits
		file->seekg(0, std::ios::beg);   // "I have returned."
		file->seekg(pos, std::ios::beg); // "It will be done."
	}
	return !file->fail();
}

uint16_t CDROM_Interface_Image::BinaryFile::decode(uint8_t *buffer)
{
    if (static_cast<uint32_t>(file->tellg()) != audio_pos)
		if (!seek(audio_pos)) return 0;

	file->read((char*)buffer, chunkSize);
    const uint16_t bytes_read = (uint16_t)file->gcount();
    audio_pos += bytes_read;
	return bytes_read;
}

CDROM_Interface_Image::AudioFile::AudioFile(const char *filename, bool &error)
	: TrackFile(4096)
{
	// Use the audio file's actual sample rate and number of channels as opposed to overriding
	Sound_AudioInfo desired = {AUDIO_S16, 0, 0};
	sample = Sound_NewSampleFromFile(filename, &desired, chunkSize);
	if (sample) {
		error = false;
		std::string filename_only(filename);
		filename_only = filename_only.substr(filename_only.find_last_of("\\/") + 1);
		LOG_MSG("CDROM: Loaded %s [%d Hz %d-channel]", filename_only.c_str(), this->getRate(), this->getChannels());
	} else
		error = true;
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
bool CDROM_Interface_Image::AudioFile::seek(int64_t offset)
{
	#ifdef DEBUG
	const auto begin = std::chrono::steady_clock::now();
	#endif

	if (audio_pos == (uint32_t)offset) {
#ifdef DEBUG
		LOG_MSG("CDROM: seek to %u avoided with position-tracking", offset);
#endif
		return true;
    }

	// Convert the byte-offset to a time offset (milliseconds)
	const bool result = Sound_Seek(sample, lround(offset/176.4f));
	audio_pos = result ? (uint32_t)offset : UINT32_MAX;

	#ifdef DEBUG
	const auto end = std::chrono::steady_clock::now();
	LOG_MSG("%s CDROM: seek(%u) took %f ms", get_time(), offset, chrono::duration <double, milli> (end - begin).count());
	#endif

	return result;
}

uint16_t CDROM_Interface_Image::AudioFile::decode(uint8_t *buffer)
{
	const uint16_t bytes = Sound_Decode(sample);
    audio_pos += bytes;
	memcpy(buffer, sample->buffer, bytes);
	return bytes;
}

uint16_t CDROM_Interface_Image::AudioFile::getEndian()
{
	return sample ? sample->actual.format : AUDIO_S16SYS;
}

uint32_t CDROM_Interface_Image::AudioFile::getRate()
{
	uint32_t rate(0);
	if (sample) {
		rate = sample->actual.rate;
	}
	return rate;
}

uint8_t CDROM_Interface_Image::AudioFile::getChannels()
{
	uint8_t channels(0);
	if (sample) {
		channels = sample->actual.channels;
	}
	return channels;
}

int64_t CDROM_Interface_Image::AudioFile::getLength()
{
	int64_t length(-1);

	// GetDuration returns milliseconds ... but getLength needs Red Book bytes.
	const int duration_ms = Sound_GetDuration(sample);
	if (duration_ms > 0) {
		// ... so convert ms to "Red Book bytes" by multiplying with 176.4f,
		// which is 44,100 samples/second * 2-channels * 2 bytes/sample
		// / 1000 milliseconds/second
		length = (int64_t)round(duration_ms * 176.4f);
	}
    #ifdef DEBUG
    LOG_MSG("%s CDROM: AudioFile::getLength is %ld bytes", get_time(), length);
    #endif

	return length;
}

CDROM_Interface_Image::CHDFile::CHDFile(const char* filename, bool& error)
    :TrackFile(RAW_SECTOR_SIZE) // CDAudioCallBack needs 2352
{
    error = chd_open(filename, CHD_OPEN_READ, NULL, &this->chd) != CHDERR_NONE;
    if (!error) {
        this->header           = chd_get_header(this->chd);
        this->hunk_buffer      = new uint8_t[this->header->hunkbytes];
        this->hunk_buffer_next = new uint8_t[this->header->hunkbytes];
    }
}

CDROM_Interface_Image::CHDFile::~CHDFile()
{
    // Guard: only cleanup if needed
    if (this->chd) {
        chd_close(this->chd);
        this->chd = nullptr;
    }

    if (this->hunk_buffer) {
        delete[] this->hunk_buffer;
        this->hunk_buffer = nullptr;
    }

#if !defined(HX_DOS) && !(defined(__MINGW32__) && !defined(__MINGW64_VERSION_MAJOR))
    if (this->hunk_thread) {
        this->hunk_thread->join();
        delete hunk_thread;
        this->hunk_thread = nullptr;
    }
#endif
}

#if !defined(HX_DOS) && !(defined(__MINGW32__) && !defined(__MINGW64_VERSION_MAJOR))
void hunk_thread_func(chd_file* chd, int hunk_index, uint8_t* buffer, bool* error)
{
    // loads one hunk into buffer
    *error = chd_read(chd, hunk_index, buffer) != CHDERR_NONE;
}
#endif

bool CDROM_Interface_Image::CHDFile::read(uint8_t* buffer,int64_t offset, int count)
{
    // we can not read more than a single sector currently
    if (count > RAW_SECTOR_SIZE) {
        return false;
    }

    uint64_t needed_hunk = (uint64_t)offset / (uint64_t)this->header->hunkbytes;

    // EOF
    if (needed_hunk > this->header->totalhunks) {
        return false;
    }

    // read new hunk if needed
    if ((int)needed_hunk != this->hunk_buffer_index) {
#if defined(HX_DOS) || defined(__MINGW32__) && !defined(__MINGW64_VERSION_MAJOR)
        if (chd_read(this->chd, needed_hunk, this->hunk_buffer) != CHDERR_NONE)
            return false;
#else
        // make sure our thread is done
        if (this->hunk_thread) this->hunk_thread->join();

        // can we use our prefetched hunk
        if (((int)needed_hunk == (this->hunk_buffer_index + 1)) && (!this->hunk_thread_error)) {
            // swap pointers and we're good :)
            std::swap(this->hunk_buffer, this->hunk_buffer_next);

        } else {
            // read new hunk
            bool error = true;
            hunk_thread_func(this->chd, (int)needed_hunk, this->hunk_buffer, &error);
            if (error) {
                return false;
            }
        }
#endif

        this->hunk_buffer_index = (int)needed_hunk;

#if !defined(HX_DOS) && !(defined(__MINGW32__) && !defined(__MINGW64_VERSION_MAJOR))
        // prefetch: let our thread decode the next hunk
        if (hunk_thread) delete this->hunk_thread;
        this->hunk_thread = new std::thread(
            [this, needed_hunk]() {
                hunk_thread_func(this->chd, (int)(needed_hunk + 1), this->hunk_buffer_next, &this->hunk_thread_error);
            }
        );
#endif
    }

    // copy data
    // the overlying read code thinks there is a sync header
    // so for 2048 sector size images we need to subtract 16 from the offset to account for the missing sync header
    uint8_t* source = this->hunk_buffer + ((uint64_t)offset - (uint64_t)needed_hunk * this->header->hunkbytes) - ((uint64_t)16 * this->skip_sync);
    memcpy(buffer, source, min(count, RAW_SECTOR_SIZE));

    return true;
}

int64_t CDROM_Interface_Image::CHDFile::getLength()
{
    return this->header->logicalbytes;
}

uint16_t CDROM_Interface_Image::CHDFile::getEndian()
{
    // CHD: no idea about this, chaning this did not fix the cd audio noise
    // Image files are read into native-endian byte-order
#if defined(WORDS_BIGENDIAN)
    return AUDIO_S16MSB;
#else
    return AUDIO_S16LSB;
#endif
}

bool CDROM_Interface_Image::CHDFile::seek(int64_t offset)
{
    // only checks if seek range is valid ? only used for audio ?
    // only used by PlayAudioSector ?
    if ((uint32_t)((uint64_t)offset / this->header->hunkbytes) < this->header->hunkcount) {
        return true;
    } else {
        return false;
    }
}

static void Endian_A16_Swap(void* src, uint32_t nelements)
{
    uint32_t i;
    uint8_t* nsrc = (uint8_t*)src;

    for (i = 0; i < nelements; i++)
    {
        uint8_t tmp = nsrc[i * 2];

        nsrc[i * 2] = nsrc[i * 2 + 1];
        nsrc[i * 2 + 1] = tmp;
    }
}

uint16_t CDROM_Interface_Image::CHDFile::decode(uint8_t* buffer)
{
    // reads one sector of CD audio ?

    assert(this->audio_pos % 2448 == 0);

    if (this->read(buffer, this->audio_pos, RAW_SECTOR_SIZE)) {
        // chd uses 2448
        this->audio_pos += 2448;

        // no idea if other platforms need this but on windows this is needed or there is only noise
        Endian_A16_Swap(buffer, 588 * 2);

        // we only read the raw audio nothing else
        return RAW_SECTOR_SIZE;
    }
    return 0;
}

// initialize static members
int CDROM_Interface_Image::refCount = 0;
CDROM_Interface_Image* CDROM_Interface_Image::images[26] = {};
CDROM_Interface_Image::imagePlayer CDROM_Interface_Image::player;

CDROM_Interface_Image::CDROM_Interface_Image(uint8_t subUnit)
		      :subUnit(subUnit)
{
	images[subUnit] = this;
	if (refCount == 0) {
		if (player.channel == NULL) {
			// channel is kept dormant except during cdrom playback periods
			player.channel = MIXER_AddChannel(&CDAudioCallBack, 0, "CDAUDIO");
			player.channel->Enable(false);
			// LOG_MSG("CDROM: Initialized with %d-byte circular buffer", AUDIO_DECODE_BUFFER_SIZE);
		}
	}
	refCount++;
}

CDROM_Interface_Image::~CDROM_Interface_Image()
{
	refCount--;
	if (player.cd == this) player.cd = NULL;
	ClearTracks();
	// Stop playback before wiping out the CD Player
	if (refCount == 0) {
		StopAudio();
		MIXER_DelChannel(player.channel);
		player.channel = NULL;
		// LOG_MSG("CDROM: Audio channel freed");
	}
}

bool CDROM_Interface_Image::SetDevice(char* path, int forceCD)
{
	(void)forceCD;//UNUSED
	const bool result = LoadCueSheet(path) || LoadIsoFile(path) || LoadChdFile(path);
	if (!result) {
		// print error message on dosbox console
		char buf[MAX_LINE_LENGTH];
		snprintf(buf, MAX_LINE_LENGTH, "Could not load image file: %s\r\n", path);
		uint16_t size = (uint16_t)strlen(buf);
		DOS_WriteFile(STDOUT, (uint8_t*)buf, &size);
	}
	return result;
}

bool CDROM_Interface_Image::GetUPC(unsigned char& attr, char* upc)
{
	attr = 0;
	strcpy(upc, this->mcn.c_str());
	return true;
}

bool CDROM_Interface_Image::GetAudioTracks(int& stTrack, int& end, TMSF& leadOut)
{
	stTrack = 1;
	end = (int)(tracks.size() - 1);
	FRAMES_TO_MSF(tracks[tracks.size() - 1].start + 150, &leadOut.min, &leadOut.sec, &leadOut.fr);

	#ifdef DEBUG
	LOG_MSG("%s CDROM: GetAudioTracks, stTrack=%d, end=%d, leadOut.min=%d, leadOut.sec=%d, leadOut.fr=%d",
      get_time(),
	  stTrack,
	  end,
	  leadOut.min,
	  leadOut.sec,
	  leadOut.fr);
	#endif

	return true;
}

bool CDROM_Interface_Image::GetAudioTrackInfo(int track, TMSF& start, unsigned char& attr)
{
	if (track < 1 || track > (int)tracks.size()) return false;
	FRAMES_TO_MSF(tracks[track - 1].start + 150, &start.min, &start.sec, &start.fr);
	attr = tracks[track - 1].attr;

	#ifdef DEBUG
	LOG_MSG("%s CDROM: GetAudioTrackInfo track=%d MSF %02d:%02d:%02d, attr=%u",
	  get_time(),
	  track,
	  start.min,
	  start.sec,
	  start.fr,
	  attr
	);
	#endif

	return true;
}

bool CDROM_Interface_Image::GetAudioSub(unsigned char& attr, unsigned char& track, unsigned char& index, TMSF& relPos, TMSF& absPos)
{
	int cur_track = GetTrack(player.currFrame);
	if (cur_track < 1) return false;
	track = (unsigned char)cur_track;
	attr = tracks[track - 1].attr;
	index = 1;
	FRAMES_TO_MSF(player.currFrame + 150, &absPos.min, &absPos.sec, &absPos.fr);
	FRAMES_TO_MSF(player.currFrame - tracks[track - 1].start + 150, &relPos.min, &relPos.sec, &relPos.fr);

	#ifdef DEBUG
	LOG_MSG("%s CDROM: GetAudioSub attr=%u, track=%u, index=%u", get_time(), attr, track, index);

	LOG_MSG("%s CDROM: GetAudioSub absoute  offset (%d), MSF=%d:%d:%d",
      get_time(),
	  player.currFrame + 150,
	  absPos.min,
	  absPos.sec,
	  absPos.fr);
	LOG_MSG("%s CDROM: GetAudioSub relative offset (%d), MSF=%d:%d:%d",
      get_time(),
	  player.currFrame - tracks[track - 1].start + 150,
	  relPos.min,
	  relPos.sec,
	  relPos.fr);
	#endif

	return true;
}

bool CDROM_Interface_Image::GetAudioStatus(bool& playing, bool& pause)
{
	playing = player.isPlaying;
	pause = player.isPaused;

	#ifdef DEBUG
	LOG_MSG("%s CDROM: GetAudioStatus playing=%d, paused=%d", get_time(), playing, pause);
	#endif

	return true;
}

bool CDROM_Interface_Image::GetMediaTrayStatus(bool& mediaPresent, bool& mediaChanged, bool& trayOpen)
{
	mediaPresent = true;
	mediaChanged = false;
	trayOpen = false;

	#ifdef DEBUG
	LOG_MSG("%s CDROM: GetMediaTrayStatus present=%d, changed=%d, open=%d", get_time(), mediaPresent, mediaChanged, trayOpen);
	#endif

	return true;
}

bool CDROM_Interface_Image::PlayAudioSector(unsigned long start, unsigned long len)
{
	bool is_playable(false);
	const int track = GetTrack(start) - 1;

	// The CDROM Red Book standard allows up to 99 tracks, which includes the data track
	if ( track < 0 || track > 99 )
		LOG_MSG("Game tried to load track #%d, which is invalid", track);

	// Attempting to play zero sectors is a no-op
	else if (len == 0)
		LOG_MSG("Game tried to play zero sectors, skipping");

	// The maximum storage achieved on a CDROM was ~900MB or just under 100 minutes
	// with overburning, so use this threshold to sanity-check the start sector.
	else if (start > 450000)
		LOG_MSG("Game tried to read sector %lu, which is beyond the 100-minute maximum of a CDROM", start);

	// We can't play audio from a data track (as it would result in garbage/static)
	else if(track >= 0 && tracks[track].attr == 0x40)
		LOG_MSG("Game tries to play the data track. Not doing this");

	// Checks passed, setup the audio stream
	else {
		TrackFile* trackFile = tracks[track].file;

		// Convert the playback start sector to a time offset (milliseconds) relative to the track
		const uint32_t offset = tracks[track].skip + (start - tracks[track].start) * tracks[track].sectorSize;
		is_playable = trackFile->seek(offset);

		// only initialize the player elements if our track is playable
		if (is_playable) {
            trackFile->setAudioPosition(offset);
			const uint8_t channels = trackFile->getChannels();
			const uint32_t rate = trackFile->getRate();

			player.cd = this;
			player.trackFile = trackFile;
			player.startFrame = start;
			player.currFrame = start;
			player.numFrames = len;
			player.bufferPos = 0;
			player.bufferConsumed = 0;
			player.isPlaying = true;
			player.isPaused = false;

			if ( (!IS_BIGENDIAN && trackFile->getEndian() == AUDIO_S16SYS) ||
			     ( IS_BIGENDIAN && trackFile->getEndian() != AUDIO_S16SYS) )
				player.addSamples = channels ==  2  ? &MixerChannel::AddSamples_s16 \
				                                    : &MixerChannel::AddSamples_m16;
			else
				player.addSamples = channels ==  2  ? &MixerChannel::AddSamples_s16_nonnative \
				                                    : &MixerChannel::AddSamples_m16_nonnative;

			const float bytesPerMs = (float)(rate * channels * 2 / 1000.0);
			player.playbackTotal = lround(len * tracks[track].sectorSize * bytesPerMs / 176.4);
			player.playbackRemaining = player.playbackTotal;

			#ifdef DEBUG
			LOG_MSG(
			   "%s CDROM: Playing track %d at %.1f KHz %d-channel at start sector %lu (%.1f minute-mark), seek %u (skip=%d,dstart=%d,secsize=%d), for %lu sectors (%.1f seconds)",
			   get_time(),
			   track,
			   rate/1000.0,
			   channels,
			   start,
			   offset * (1/10584000.0),
			   offset,
			   tracks[track].skip,
			   tracks[track].start,
			   tracks[track].sectorSize,
			   len,
			   player.playbackRemaining / (1000 * bytesPerMs)
			);
			#endif

			// start the channel!
			player.channel->SetFreq(rate);
			player.channel->Enable(true);
		}
	}
	if (!is_playable) StopAudio();
	return is_playable;
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

bool CDROM_Interface_Image::ReadSectors(PhysPt buffer, bool raw, unsigned long sector, unsigned long num)
{
	int sectorSize = raw ? RAW_SECTOR_SIZE : COOKED_SECTOR_SIZE;
	Bitu buflen = num * sectorSize;
	uint8_t* buf = new uint8_t[buflen];

	bool success = true; //Gobliiins reads 0 sectors
	for(unsigned long i = 0; i < num; i++) {
		success = ReadSector(&buf[i * sectorSize], raw, sector + i);
		if (!success) break;
	}

	MEM_BlockWrite(buffer, buf, buflen);
	delete[] buf;
	return success;
}

bool CDROM_Interface_Image::ReadSectorsHost(void *buffer, bool raw, unsigned long sector, unsigned long num)
{
	unsigned int sectorSize = raw ? RAW_SECTOR_SIZE : COOKED_SECTOR_SIZE;
	bool success = true; //Gobliiins reads 0 sectors
	for(unsigned long i = 0; i < num; i++) {
		success = ReadSector((uint8_t*)buffer + (i * (Bitu)sectorSize), raw, sector + i);
		if (!success) break;
	}

	return success;
}

bool CDROM_Interface_Image::LoadUnloadMedia(bool unload)
{
	(void)unload; // unused by part of the API
	return true;
}

int CDROM_Interface_Image::GetTrack(int sector)
{
	vector<Track>::iterator i = tracks.begin();
	vector<Track>::iterator end = tracks.end() - 1;

	while(i != end) {
		Track &curr = *i;
		Track &next = *(i + 1);
		if (curr.start <= sector && sector < next.start) return curr.number;
		i++;
	}
	return -1;
}

bool CDROM_Interface_Image::ReadSector(uint8_t *buffer, bool raw, unsigned long sector)
{
	int track = GetTrack(sector) - 1;
	if (track < 0) return false;

	int64_t seek = (int64_t)tracks[track].skip + ((int64_t)(sector - tracks[track].start)) * (int64_t)tracks[track].sectorSize;
	int length = (raw ? RAW_SECTOR_SIZE : COOKED_SECTOR_SIZE);
	if (tracks[track].sectorSize != RAW_SECTOR_SIZE && raw) return false;
	if ((tracks[track].sectorSize == RAW_SECTOR_SIZE || tracks[track].sectorSize == 2448) && !tracks[track].mode2 && !raw) seek += 16;
	if (tracks[track].mode2 && !raw) seek += 24;

	// LOG_MSG("CDROM: ReadSector track=%d, desired raw=%s, sector=%ld, length=%d", track, raw ? "true":"false", sector, length);
	return tracks[track].file->read(buffer, seek, length);
}

void CDROM_Interface_Image::CDAudioCallBack(Bitu len)
{
	// Our member object "playbackRemaining" holds the
	// exact number of stream-bytes we need to play before meeting the
	// DOS program's desired playback duration in sectors. We simply
	// decrement this counter each callback until we're done.
	if (len == 0 || !player.isPlaying || player.isPaused) return;


	// determine bytes per request (16-bit samples)
	const uint8_t channels = player.trackFile->getChannels();
	const uint8_t bytes_per_request = channels * 2;
	uint16_t total_requested = (uint16_t)(len * bytes_per_request);

	while (total_requested > 0) {
		uint16_t requested = total_requested;

		// Every now and then the callback wants a big number of bytes,
		// which can exceed our circular buffer. In these cases we need
		// read through as many iteration of our circular buffer as needed.
		if (total_requested > AUDIO_DECODE_BUFFER_SIZE) {
			requested = AUDIO_DECODE_BUFFER_SIZE;
			total_requested -= AUDIO_DECODE_BUFFER_SIZE;
		}
		else {
			total_requested = 0;
		}

		// Three scenarios in order of probabilty:
		//
		// 1. Consume: If our decoded circular buffer is sufficiently filled to
		//	     satify the requested size, then feed the callback with
		//	     the requested number of bytes.
		//
		// 2. Wrap: If we've decoded and consumed to edge of our buffer, then
		//	     we need to wrap any remaining decoded-but-not-consumed
		//	     samples back around to the front of the buffer.
		//
		// 3. Fill: When out circular buffer is too depleted to satisfy the
		//	     requested size, then perform chunked-decode reads from
		//	     the audio-codec to either fill our buffer or satify our
		//	     remaining playback - whichever is smaller.
		//
		while (true) {

			// 1. Consume
			// ==========
			if (player.bufferPos - player.bufferConsumed >= requested) {
				if (player.ctrlUsed) {
					for (uint8_t i=0; i < channels; i++) {
						int16_t  sample;
						int16_t* samples = (int16_t*)&player.buffer[player.bufferConsumed];
						for (Bitu pos = 0; pos < requested / bytes_per_request; pos++) {
							#if defined(WORDS_BIGENDIAN)
							sample = (int16_t)host_readw((HostPt) & samples[pos * 2 + player.ctrlData.out[i]]);
							#else
							sample = samples[pos * 2 + player.ctrlData.out[i]];
							#endif
							samples[pos * 2 + i] = (int16_t)(sample * player.ctrlData.vol[i] / 255.0);
						}
					}
				}
				// uses either the stereo or mono and native or nonnative AddSamples call assigned during construction
				(player.channel->*player.addSamples)(requested / bytes_per_request, (int16_t*)(player.buffer + player.bufferConsumed) );
				player.bufferConsumed += requested;

				player.playbackRemaining -= requested;
				if (player.playbackRemaining < 0)
					player.playbackRemaining = 0;

				// Games can query the current Red Book MSF frame-position, so we keep that up-to-date here.
				// We scale the final number of frames by the percent complete, which
				// avoids having to keep track of the euivlent number of Red Book frames
				// read (which would involve coverting the compressed streams data-rate into
				// CDROM Red Book rate, which is more work than simply scaling).
				//
				const float playbackPercentSoFar = static_cast<float>(player.playbackTotal - player.playbackRemaining) / player.playbackTotal;
				player.currFrame = (uint32_t)(player.startFrame + ceil(player.numFrames * playbackPercentSoFar));
				break;
				// printProgress( (player.bufferPos - player.bufferConsumed)/(float)AUDIO_DECODE_BUFFER_SIZE, "consume");
			}

			// 2. Wrap
			// =======
			else {
				memcpy(player.buffer,
					   player.buffer + player.bufferConsumed,
					   player.bufferPos - player.bufferConsumed);
				player.bufferPos -= player.bufferConsumed;
				player.bufferConsumed = 0;
				// printProgress( (player.bufferPos - player.bufferConsumed)/(float)AUDIO_DECODE_BUFFER_SIZE, "wrap");
			}

			// 3. Fill
			// =======
			const uint16_t chunkSize = player.trackFile->chunkSize;
			while(AUDIO_DECODE_BUFFER_SIZE - player.bufferPos >= chunkSize &&
			      (player.bufferPos - player.bufferConsumed < player.playbackRemaining ||
				   player.bufferPos - player.bufferConsumed < requested) ) {

				const uint16_t decoded = player.trackFile->decode(player.buffer + player.bufferPos);
				player.bufferPos += decoded;

				// if we decoded less than expected, which could be due to EOF or if the CUE file specified
				// an exact "INDEX 01 MIN:SEC:FRAMES" value but the compressed track is ever-so-slightly less than
				// that specified, then simply pad with zeros.
				const int16_t underDecode = chunkSize - decoded;
				if (underDecode > 0) {

                    #ifdef DEBUG
					LOG_MSG("%s CDROM: Underdecoded by %d. Feeding mixer with zeros.", get_time(), underDecode);
                    #endif

					memset(player.buffer + player.bufferPos, 0, underDecode);
					player.bufferPos += underDecode;
				}
				// printProgress( (player.bufferPos - player.bufferConsumed)/(float)AUDIO_DECODE_BUFFER_SIZE, "fill");
			} // end of fill-while
		} // end of decode and fill loop
	} // end while total_requested

	if (player.playbackRemaining <= 0) {
		player.cd->StopAudio();
		// printProgress( (player.bufferPos - player.bufferConsumed)/(float)AUDIO_DECODE_BUFFER_SIZE, "stop");
	}
}

bool CDROM_Interface_Image::LoadIsoFile(char* filename)
{
	tracks.clear();
	// data track
	Track track = {0, 0, 0, 0, 0, 0, false, NULL};
	bool error;
	track.file = new BinaryFile(filename, error);
	if (error) {
		delete track.file;
		track.file = NULL;
		return false;
	}
	track.number = 1;
	track.attr = 0x40;//data

	// try to detect iso type
	if (CanReadPVD(track.file, COOKED_SECTOR_SIZE, false)) {
		track.sectorSize = COOKED_SECTOR_SIZE;
		track.mode2 = false;
	} else if (CanReadPVD(track.file, RAW_SECTOR_SIZE, false)) {
		track.sectorSize = RAW_SECTOR_SIZE;
		track.mode2 = false;
	} else if (CanReadPVD(track.file, 2336, true)) {
		track.sectorSize = 2336;
		track.mode2 = true;
	} else if (CanReadPVD(track.file, RAW_SECTOR_SIZE, true)) {
		track.sectorSize = RAW_SECTOR_SIZE;
		track.mode2 = true;
	} else if (CanReadPVD(track.file, 2448, false)) {
		track.sectorSize = 2448;
		track.mode2 = false;
	} else {
        delete track.file;
        track.file = NULL;
		return false;
	}
    int64_t len=track.file->getLength();
	track.length = (int)(len / track.sectorSize);
	// LOG_MSG("LoadIsoFile: %s, track 1, 0x40, sectorSize=%d, mode2=%s", filename, track.sectorSize, track.mode2 ? "true":"false");

	tracks.push_back(track);

	// leadout track
	track.number = 2;
	track.attr = 0;
	track.start = track.length;
	track.length = 0;
	track.file = NULL;
	tracks.push_back(track);
	return true;
}

bool CDROM_Interface_Image::CanReadPVD(TrackFile *file, int sectorSize, bool mode2)
{
	uint8_t pvd[COOKED_SECTOR_SIZE];
	int seek = 16 * sectorSize;	// first vd is located at sector 16
	if ((sectorSize == RAW_SECTOR_SIZE || sectorSize == 2448) && !mode2) seek += 16;
	if (mode2) seek += 24;
	file->read(pvd, seek, COOKED_SECTOR_SIZE);
	// pvd[0] = descriptor type, pvd[1..5] = standard identifier, pvd[6] = iso version (+8 for High Sierra)
	return ((pvd[0] == 1 && !strncmp((char*)(&pvd[1]), "CD001", 5) && pvd[6] == 1) ||
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
	Track track = {0, 0, 0, 0, 0, 0, false, NULL};
	tracks.clear();
	int shift = 0;
	int currPregap = 0;
	int totalPregap = 0;
	int prestart = -1;
	bool success;
	bool canAddTrack = false;
	char tmp[MAX_FILENAME_LENGTH];  // dirname can change its argument
	safe_strncpy(tmp, cuefile, MAX_FILENAME_LENGTH);
	string pathname(dirname(tmp));
	ifstream in;
	in.open(cuefile, ios::in);
	if (in.fail()) return false;

	while(!in.eof()) {
		// get next line
		char buf[MAX_LINE_LENGTH];
		in.getline(buf, MAX_LINE_LENGTH);
		if (in.fail() && !in.eof()) return false;  // probably a binary file
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

			line >> track.number;
			string type;
			GetCueKeyword(type, line);

			if (type == "AUDIO") {
				track.sectorSize = RAW_SECTOR_SIZE;
				track.attr = 0;
				track.mode2 = false;
			} else if (type == "MODE1/2048") {
				track.sectorSize = COOKED_SECTOR_SIZE;
				track.attr = 0x40;
				track.mode2 = false;
			} else if (type == "MODE1/2352") {
				track.sectorSize = RAW_SECTOR_SIZE;
				track.attr = 0x40;
				track.mode2 = false;
			} else if (type == "MODE2/2336") {
				track.sectorSize = 2336;
				track.attr = 0x40;
				track.mode2 = true;
			} else if (type == "MODE2/2352") {
				track.sectorSize = RAW_SECTOR_SIZE;
				track.attr = 0x40;
				track.mode2 = true;
			} else if (type == "MODE1/2448") {
				track.sectorSize = 2448;
				track.attr = 0x40;
				track.mode2 = false;
			} else success = false;

			canAddTrack = true;
		}
		else if (command == "INDEX") {
			int index;
			line >> index;
			int frame;
			success = GetCueFrame(frame, line);

			if (index == 1) track.start = frame;
			else if (index == 0) prestart = frame;
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

			track.file = NULL;
			bool error = true;
			if (type == "BINARY") {
				track.file = new BinaryFile(filename.c_str(), error);
			}
            else
                track.file = new AudioFile(filename.c_str(), error);
                // SDL_Sound first tries using a decoder having a matching registered extension
                // as the filename, and then falls back to trying each decoder before finally
                // giving up.
            if (error) {
                delete track.file;
                track.file = NULL;
                success = false;
            }
		}
		else if (command == "PREGAP") success = GetCueFrame(currPregap, line);
		else if (command == "CATALOG") success = GetCueString(mcn, line);
		// ignored commands
		else if (command == "CDTEXTFILE" || command == "FLAGS" || command == "ISRC"
			|| command == "PERFORMER" || command == "POSTGAP" || command == "REM"
			|| command == "SONGWRITER" || command == "TITLE" || command == "") success = true;
		// failure
		else {
			delete track.file;
			track.file = NULL;
			success = false;
		}
		if (!success) return false;
	}
	// add last track
	if (!AddTrack(track, shift, prestart, totalPregap, currPregap)) return false;

	// add leadout track
	track.number++;
	track.attr = 0;//sync with load iso
	track.start = 0;
	track.length = 0;
	track.file = NULL;
	if(!AddTrack(track, shift, -1, totalPregap, 0)) return false;

	return true;
}

std::vector<string> split_string_to_list(const std::string& str, const std::string& delim)
{
    std::vector<string> tokens;
    size_t prev = 0, pos = 0;
    do
    {
        pos = str.find(delim, prev);
        if (pos == string::npos) pos = str.length();
        string token = str.substr(prev, pos - prev);
        if (!token.empty()) tokens.push_back(token);
        prev = pos + delim.length();
    } while (pos < str.length() && prev < str.length());
    return tokens;
}

bool CDROM_Interface_Image::LoadChdFile(char* chdfile)
{
    /*
        ToDo:
            - check if this is a CD and not an HDD CHD
            - check for CDROM_TRACK_METADATA_TAG or CDROM_OLD_METADATA_TAG (?)
    */
    tracks.clear();
    bool     error       = true;
    CHDFile* file        = new CHDFile(chdfile, error);
    Track    track       = { 0, 0, 0, 0, 0, 0, false, file };
    int      shift       = 0;
    int      currPregap  = 0;
    int      totalPregap = 0;
    int      prestart    = -1;
    if (!error) {
        // iterate over all track entries
        char   metadata[256];
        int    index        = 0;
        UINT32 count        = 0;
        UINT32 tag          = 0;
        UINT8  flags        = 0;
        int    total_frames = 0;
        while (chd_get_metadata(file->getChd(), CDROM_TRACK_METADATA2_TAG, index, &metadata, 256, &count, &tag, &flags) == CHDERR_NONE) {
            // parse track
            // TRACK:1 TYPE:MODE1_RAW SUBTYPE:NONE FRAMES:206931 PREGAP:0 P
            std::vector<string> tokens = split_string_to_list(metadata, " ");
            track.start                = 0;
            track.skip                 = 0;
            currPregap                 = 0;
            prestart                   = -1;
            for (int i = 0; i < tokens.size(); i++) {
                // "TRACK:1" > "TRACK" "1"
                std::vector<string> track_meta = split_string_to_list(tokens[i], ":");
                std::string         key        = track_meta[0];
                std::string         value      = track_meta[1];

                if (key == "TRACK") {
                    // track index
                    track.number = std::stoi(value);
                } else if (key == "TYPE") {
                    // track type
                    if (value == "AUDIO") {
                        track.sectorSize = RAW_SECTOR_SIZE;
                        track.attr       = 0;
                        track.mode2      = false;
                    } else if (value == "MODE1") {
                        track.sectorSize = COOKED_SECTOR_SIZE;
                        track.attr       = 0x40;
                        track.mode2      = false;
                        file->skip_sync  = true;
                    } else if (value == "MODE1_RAW") {
                        track.sectorSize = RAW_SECTOR_SIZE;
                        track.attr       = 0x40;
                        track.mode2      = false;
                    } else if (value == "MODE2") {
                        track.sectorSize = 2336;
                        track.attr       = 0x40;
                        track.mode2      = true;
                    } else if (value == "MODE2_FORM1") {
                        track.sectorSize = COOKED_SECTOR_SIZE;
                        track.attr       = 0x40;
                        track.mode2      = true;
                        file->skip_sync  = true;
                    } else if (value == "MODE2_FORM2") {
                        track.sectorSize = 2336;
                        track.attr       = 0x40;
                        track.mode2      = true;
                    } else if (value == "MODE2_RAW") {
                        track.sectorSize = RAW_SECTOR_SIZE;
                        track.attr       = 0x40;
                        track.mode2      = true;
                    } else {
                        // unknown track mode
                        return false;
                    };
                } else if (key == "SUBTYPE") {
                    if (value == "RAW") {
                        //track.sectorSize += 96;
                        // is subchannel data even supported?
                    }
                } else if (key == "FRAMES") {
                    int frames    = std::stoi(value);
                    track.start   = total_frames;
                    total_frames += frames;
                    track.length  = frames;

                } else if (key == "PREGAP") {
                    currPregap = std::stoi(value);
                }

            }

            // chd has the pregap added to FRAMES
            // this is needed or track.start and track.length do not match with LoadCueSheet for the same image
            // creates the same track list as LoadCueSheet
            if (currPregap) {
                prestart      = track.start;
                track.start  += currPregap;
                track.length -= currPregap;
            }

            track.sectorSize = 2448; // chd always uses 2448
            if (!AddTrack(track, shift, prestart, totalPregap, 0)) {
                return false;
            }

            index += 1;
        }

        // no tracks found
        if (tracks.empty()) return false;

        // add leadout track
        track.number++;
        track.attr   = 0; // sync with load iso
        track.start  = 0;
        track.length = 0;
        track.file   = NULL;
        if (!AddTrack(track, shift, -1, totalPregap, 0)) return false;

        return true;

    } else {
        return false;
    }
}

bool CDROM_Interface_Image::AddTrack(Track &curr, int &shift, int prestart, int &totalPregap, int currPregap)
{
	// frames between index 0(prestart) and 1(curr.start) must be skipped
	int skip;
	if (prestart >= 0) {
		if (prestart > curr.start) return false;
		skip = curr.start - prestart;
	} else skip = 0;

	// first track (track number must be 1)
	if (tracks.empty()) {
		if (curr.number != 1) return false;
		curr.skip = skip * curr.sectorSize;
		curr.start += currPregap;
		totalPregap = currPregap;
		tracks.push_back(curr);
		return true;
	}

	Track &prev = *(tracks.end() - 1);

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
		if (!prev.length) {
			int64_t tmp = prev.file->getLength() - prev.skip;
			prev.length = (int)(tmp / prev.sectorSize);
			if (tmp % prev.sectorSize != 0) prev.length++; // padding
		}
		curr.start += prev.start + prev.length + currPregap;
		curr.skip = skip * curr.sectorSize;
		shift += prev.start + prev.length;
		totalPregap = currPregap;
	}

	#ifdef DEBUG
	LOG_MSG("%s CDROM: AddTrack cur.start=%d cur.len=%d cur.start+len=%d | prev.start=%d prev.len=%d prev.start+len=%d",
	        get_time(),
            curr.start, curr.length, curr.start + curr.length,
	        prev.start, prev.length, prev.start + prev.length);
	#endif

	// error checks
	if (curr.number <= 1) return false;
	if (prev.number + 1 != curr.number) return false;
	if (curr.start < prev.start + prev.length) return false;
	if (curr.length < 0) return false;

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
	uint8_t drive;
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

bool CDROM_Interface_Image::GetCueFrame(int &frames, istream &in)
{
	string msf;
	in >> msf;
	int min, sec, fr;
	bool success = sscanf(msf.c_str(), "%d:%d:%d", &min, &sec, &fr) == 3;
	frames = MSF_TO_FRAMES(min, sec, fr);
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

void CDROM_Interface_Image::ClearTracks()
{
	vector<Track>::iterator i = tracks.begin();
	vector<Track>::iterator end = tracks.end();

	TrackFile* last = NULL;
	while(i != end) {
		Track &curr = *i;
		if (curr.file != last) {
			delete curr.file;
			last = curr.file;
		}
		i++;
	}
	tracks.clear();
}

void CDROM_Image_ShutDown(Section* /*sec*/) {
	Sound_Quit();
}

void CDROM_Image_Init() {
	Sound_Init();
	AddExitFunction(AddExitFunctionFuncPair(CDROM_Image_ShutDown));
}
