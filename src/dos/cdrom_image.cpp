/*
 *  Copyright (C) 2002-2019  The DOSBox Team
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335, USA.
 */

#include <cctype>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <vector>
#include <sys/stat.h>
#include "cdrom.h"
#include "drives.h"
#include "support.h"
#include "control.h"
#include "setup.h"

#if !defined(WIN32)
#include <libgen.h>
#else
#include <string.h>
#endif

using namespace std;

#define MAX_LINE_LENGTH 512
#define MAX_FILENAME_LENGTH 256

CDROM_Interface_Image::BinaryFile::BinaryFile(const char *filename, bool &error)
{
	file = new ifstream(filename, ios::in | ios::binary);
	error = (file == NULL) || (file->fail());
}

CDROM_Interface_Image::BinaryFile::~BinaryFile()
{
	delete file;
	file = NULL;
}

bool CDROM_Interface_Image::BinaryFile::read(Bit8u *buffer, int seek, int count)
{
	file->seekg(seek, ios::beg);
	file->read((char*)buffer, count);
	return !(file->fail());
}

int CDROM_Interface_Image::BinaryFile::getLength()
{
	file->seekg(0, ios::end);
	int length = (int)file->tellg();
	if (file->fail()) return -1;
	return length;
}

// initialize static members
int CDROM_Interface_Image::refCount = 0;
CDROM_Interface_Image* CDROM_Interface_Image::images[26] = {NULL};
CDROM_Interface_Image::imagePlayer CDROM_Interface_Image::player = {
	NULL, NULL, NULL, {0}, 0, 0, 0, false, false, false, { {0}, {0} } };

	
CDROM_Interface_Image::CDROM_Interface_Image(Bit8u subUnit)
//                      :subUnit(subUnit)
{
	images[subUnit] = this;
	if (refCount == 0) {
		player.mutex = SDL_CreateMutex();
		if (player.channel == NULL)
			player.channel = MIXER_AddChannel(&CDAudioCallBack, 44100, "CDAUDIO");
		player.channel->Enable(true);
	}
	refCount++;
}

CDROM_Interface_Image::~CDROM_Interface_Image()
{
	refCount--;
	if (player.cd == this) player.cd = NULL;
	ClearTracks();
	if (refCount == 0) {
		SDL_DestroyMutex(player.mutex);
		if (player.channel) {
			player.channel->Enable(false);
			MIXER_DelChannel(player.channel);
			player.channel = NULL;
		}
	}
}

void CDROM_Interface_Image::InitNewMedia()
{
}

bool CDROM_Interface_Image::SetDevice(char* path, int forceCD)
{
    (void)forceCD;//UNUSED
    (void)path;//UNUSED
	if (LoadCueSheet(path)) return true;
	if (LoadIsoFile(path)) return true;
	
	// print error message on dosbox console
	/*
	char buf[MAX_LINE_LENGTH];
	snprintf(buf, MAX_LINE_LENGTH, "Could not load image file: %s\n", path);
	Bit16u size = (Bit16u)strlen(buf);
	DOS_WriteFile(STDOUT, (Bit8u*)buf, &size);
	*/
	LOG_MSG("Could not load image file: %s", path);
	return false;
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
	end = (int)(tracks.size() - 1u);
	FRAMES_TO_MSF((unsigned int)tracks[tracks.size() - 1u].start + 150u, &leadOut.min, &leadOut.sec, &leadOut.fr);
	return true;
}

bool CDROM_Interface_Image::GetAudioTrackInfo(int track, TMSF& start, unsigned char& attr)
{
	if (track < 1 || track > (int)tracks.size()) return false;
	FRAMES_TO_MSF((unsigned int)tracks[(unsigned int)track - 1u].start + 150u, &start.min, &start.sec, &start.fr);
	attr = tracks[(Bitu)track - 1u].attr;
	return true;
}

bool CDROM_Interface_Image::GetAudioSub(unsigned char& attr, unsigned char& track, unsigned char& index, TMSF& relPos, TMSF& absPos)
{
	int cur_track = GetTrack(player.currFrame);
	if (cur_track < 1) return false;
	track = (unsigned char)cur_track;
	attr = tracks[track - 1u].attr;
	index = 1;
	FRAMES_TO_MSF((unsigned int)player.currFrame + 150u, &absPos.min, &absPos.sec, &absPos.fr);
	FRAMES_TO_MSF((unsigned int)player.currFrame - (unsigned int)tracks[track - 1u].start, &relPos.min, &relPos.sec, &relPos.fr);
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

bool CDROM_Interface_Image::PlayAudioSector(unsigned long start,unsigned long len)
{
	// We might want to do some more checks. E.g valid start and length
	SDL_mutexP(player.mutex);
	player.cd = this;
	player.currFrame = int(start);
	player.targetFrame = int(start + len);
	int track = GetTrack((int)start) - 1;
	if(track >= 0 && tracks[(unsigned int)track].attr == 0x40) {
		LOG(LOG_MISC,LOG_WARN)("Game tries to play the data track. Not doing this");
		player.isPlaying = false;
		//Unclear wether return false should be here. 
		//specs say that this function returns at once and games should check the status wether the audio is actually playing
		//Real drives either fail or succeed as well
	} else player.isPlaying = true;
	player.isPaused = false;
	SDL_mutexV(player.mutex);
	return true;
}

bool CDROM_Interface_Image::PauseAudio(bool resume)
{
	player.isPaused = !resume;
	return true;
}

bool CDROM_Interface_Image::StopAudio(void)
{
	player.isPlaying = false;
	player.isPaused = false;
	return true;
}

void CDROM_Interface_Image::ChannelControl(TCtrl ctrl)
{
	player.ctrlUsed = (ctrl.out[0]!=0 || ctrl.out[1]!=1 || ctrl.vol[0]<0xfe || ctrl.vol[1]<0xfe);
	player.ctrlData = ctrl;
}

bool CDROM_Interface_Image::ReadSectors(PhysPt buffer, bool raw, unsigned long sector, unsigned long num)
{
	unsigned int sectorSize = raw ? RAW_SECTOR_SIZE : COOKED_SECTOR_SIZE;
	Bitu buflen = num * sectorSize;
	Bit8u* buf = new Bit8u[buflen];
	
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
		success = ReadSector((Bit8u*)buffer + (i * (Bitu)sectorSize), raw, sector + i);
		if (!success) break;
	}

	return success;
}

bool CDROM_Interface_Image::LoadUnloadMedia(bool unload)
{
    (void)unload;//UNUSED
	return true;
}

int CDROM_Interface_Image::GetTrack(int sector)
{
	vector<Track>::iterator i = tracks.begin();
	vector<Track>::iterator end = tracks.end() - 1;
	
	while(i != end) {
		const Track &curr = *i;
		const Track &next = *(i + 1);
		if (curr.start <= sector && sector < next.start) return curr.number;
		++i;
	}
	return -1;
}

bool CDROM_Interface_Image::ReadSector(Bit8u *buffer, bool raw, unsigned long sector)
{
	int track = GetTrack((int)sector) - 1;
	if (track < 0) return false;

	if (tracks[(unsigned int)track].sectorSize != RAW_SECTOR_SIZE && raw) return false;

	/* we must reject non-raw reads against CD audio sectors.
	 * not just for correctness, but also to avoid a weird bug in MSCDEX.EXE
	 * that reads the non-data sectors one-by-one looking for a volume label
	 * that doesn't exist on pure CD audio emulated images */
	if (tracks[(unsigned int)track].sectorSize == RAW_SECTOR_SIZE && !raw) {
		if (((unsigned char)tracks[(unsigned int)track].attr&0x40u) == 0x00u) {
			LOG_MSG("Rejecting cooked read from raw audio CD sector\n");
			return false;
		}
	}

	unsigned int length = (raw ? RAW_SECTOR_SIZE : COOKED_SECTOR_SIZE);

	if (sector >= (unsigned long)(tracks[(unsigned int)track].start + tracks[(unsigned int)track].length)) {
		memset(buffer, 0, length);
		return true;
	}

	unsigned long seek = (unsigned long)tracks[(unsigned int)track].skip +
        (sector - (unsigned long)tracks[(unsigned int)track].start) * (unsigned long)tracks[(unsigned int)track].sectorSize;
	if (tracks[(unsigned int)track].sectorSize == RAW_SECTOR_SIZE && !tracks[(unsigned int)track].mode2 && !raw) seek += 16ul;
	if (tracks[(unsigned int)track].mode2 && !raw) seek += 24ul;

	return tracks[(unsigned int)track].file->read(buffer, (int)seek, (int)length);
}

void CDROM_Interface_Image::CDAudioCallBack(Bitu len)
{
	len *= 4u;       // 16 bit, stereo
	if (!len) return;
	if (!player.isPlaying || player.isPaused) {
		player.channel->AddSilence();
		return;
	}
	
	SDL_mutexP(player.mutex);
	while (player.bufLen < (Bits)len) {
		bool success;
		if (player.targetFrame > player.currFrame)
			success = player.cd->ReadSector(&player.buffer[player.bufLen], true, (unsigned long)player.currFrame);
		else success = false;
		
		if (success) {
			player.currFrame++;
			player.bufLen += RAW_SECTOR_SIZE;
		} else {
			memset(&player.buffer[player.bufLen], 0, (size_t)(len - (Bitu)player.bufLen));
			player.bufLen = (int)len;
			player.isPlaying = false;
		}
	}
	SDL_mutexV(player.mutex);
	if (player.ctrlUsed) {
		Bit16s * samples=(Bit16s *)&player.buffer;
		for (Bitu pos=0;pos<len/4;pos++) {
			Bit16s sample0,sample1;
#if defined(WORDS_BIGENDIAN)
			sample0=(Bit16s)host_readw((HostPt)&samples[pos*2+player.ctrlData.out[0]]);
			sample1=(Bit16s)host_readw((HostPt)&samples[pos*2+player.ctrlData.out[1]]);
#else
			sample0=samples[pos*2+player.ctrlData.out[0]];
			sample1=samples[pos*2+player.ctrlData.out[1]];
#endif
			samples[pos*2+0]=(Bit16s)(sample0*player.ctrlData.vol[0]/255.0);
			samples[pos*2+1]=(Bit16s)(sample1*player.ctrlData.vol[1]/255.0);
		}
#if defined(WORDS_BIGENDIAN)
		player.channel->AddSamples_s16(len/4,(Bit16s *)player.buffer);
	} else	player.channel->AddSamples_s16_nonnative(len/4,(Bit16s *)player.buffer);
#else
	}
	player.channel->AddSamples_s16(len/4,(Bit16s *)player.buffer);
#endif
	memmove(player.buffer, &player.buffer[len], (size_t)((Bitu)player.bufLen - len));
	player.bufLen -= (int)len;
}

bool CDROM_Interface_Image::LoadIsoFile(const char* filename)
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
	} else return false;
	
	track.length = track.file->getLength() / track.sectorSize;
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
	Bit8u pvd[COOKED_SECTOR_SIZE];
	int seek = 16 * sectorSize;	// first vd is located at sector 16
	if (sectorSize == RAW_SECTOR_SIZE && !mode2) seek += 16;
	if (mode2) seek += 24;
	file->read(pvd, seek, COOKED_SECTOR_SIZE);
	// pvd[0] = descriptor type, pvd[1..5] = standard identifier, pvd[6] = iso version (+8 for High Sierra)
	return ((pvd[0] == 1 && !strncmp((char*)(&pvd[1]), "CD001", 5) && pvd[6] == 1) ||
			(pvd[8] == 1 && !strncmp((char*)(&pvd[9]), "CDROM", 5) && pvd[14] == 1));
}

#if defined(WIN32)
static string dirname(char * file) {
	const char * sep = strrchr(file, '\\');
	if (sep == NULL)
		sep = strrchr(file, '/');
	if (sep == NULL)
		return "";
	else {
		int len = (int)(sep - file);
		char tmp[MAX_FILENAME_LENGTH];
		safe_strncpy(tmp, file, len+1);
		return tmp;
	}
}
#endif

bool CDROM_Interface_Image::LoadCueSheet(const char *cuefile)
{
	Track track = {0, 0, 0, 0, 0, 0, false, NULL};
	tracks.clear();
	int shift = 0;
	int currPregap = 0;
	int totalPregap = 0;
	int prestart = 0;
	bool success;
	bool canAddTrack = false;
	char tmp[MAX_FILENAME_LENGTH];	// dirname can change its argument
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
			prestart = 0;
	
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
		else success = false;

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
	if(!AddTrack(track, shift, 0, totalPregap, 0)) return false;

	return true;
}

bool CDROM_Interface_Image::AddTrack(Track &curr, int &shift, int prestart, int &totalPregap, int currPregap)
{
	// frames between index 0(prestart) and 1(curr.start) must be skipped
	int skip;
	if (prestart > 0) {
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
		prev.length = curr.start + totalPregap - prev.start - skip;
		curr.skip += prev.skip + prev.length * prev.sectorSize + skip * curr.sectorSize;		
		totalPregap += currPregap;
		curr.start += totalPregap;
	// current track uses a different file as the previous track
	} else {
		int tmp = prev.file->getLength() - prev.skip;
		prev.length = tmp / prev.sectorSize;
		if (tmp % prev.sectorSize != 0) prev.length++; // padding
		
		curr.start += prev.start + prev.length + currPregap;
		curr.skip = skip * curr.sectorSize;
		shift += prev.start + prev.length;
		totalPregap = currPregap;
	}
	
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
	for(track_it it = tracks.begin(); it != tracks.end(); ++it) {
		if ((*it).attr == 0x40) return true;
	}
	return false;
}


bool CDROM_Interface_Image::GetRealFileName(string &filename, const string &pathname)
{
	// check if file exists
	struct stat test;
	if (stat(filename.c_str(), &test) == 0) return true;
	
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
	if (!DOS_MakeName(tmp, fullname, &drive)) return false;
	
	localDrive *ldp = dynamic_cast<localDrive*>(Drives[drive]);
	if (ldp) {
		ldp->GetSystemFilename(tmp, fullname);
		if (stat(tmp, &test) == 0) {
			filename = tmp;
			return true;
		}
	}
#if defined (WIN32) || defined(OS2)
	//Nothing
#else
	//Consider the possibility that the filename has a windows directory seperator (inside the CUE file) 
	//which is common for some commercial rereleases of DOS games using DOSBox

	string copy = filename;
	size_t l = copy.size();
	for (size_t i = 0; i < l;i++) {
		if(copy[i] == '\\') copy[i] = '/';
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
	for(Bitu i = 0; i < keyword.size(); i++) keyword[i] = toupper(keyword[i]);
	
	return true;
}

bool CDROM_Interface_Image::GetCueFrame(int &frames, istream &in)
{
	string msf;
	in >> msf;
	unsigned int min, sec, fr;
	bool success = sscanf(msf.c_str(), "%u:%u:%u", &min, &sec, &fr) == 3;
	frames = (int)MSF_TO_FRAMES(min, sec, fr);
	
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

	const TrackFile* last = NULL;
	while(i != end) {
		const Track &curr = *i;
		if (curr.file != last) {
			delete curr.file;
			last = curr.file;
		}
		++i;
	}
	tracks.clear();
}

void CDROM_Image_Destroy(const Section*) {
}

