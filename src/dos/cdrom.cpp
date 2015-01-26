/*
 *  Copyright (C) 2002-2015  The DOSBox Team
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */


// ******************************************************
// SDL CDROM 
// ******************************************************

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "dosbox.h"
#include "SDL.h"
#include "support.h"
#include "cdrom.h"

CDROM_Interface_SDL::CDROM_Interface_SDL(void) {
	driveID		= 0;
	oldLeadOut	= 0;
	cd			= 0;
}

CDROM_Interface_SDL::~CDROM_Interface_SDL(void) {
	StopAudio();
	SDL_CDClose(cd);
	cd		= 0;
}

bool CDROM_Interface_SDL::SetDevice(char* path, int forceCD) { 
	char buffer[512];
	strcpy(buffer,path);
	upcase(buffer);

	int num = SDL_CDNumDrives();
	if ((forceCD>=0) && (forceCD<num)) {
		driveID = forceCD;
	        cd = SDL_CDOpen(driveID);
	        SDL_CDStatus(cd);
	   	return true;
	};	
	
	const char* cdname = 0;
	for (int i=0; i<num; i++) {
		cdname = SDL_CDName(i);
		if (strcmp(buffer,cdname)==0) {
			cd = SDL_CDOpen(i);
			SDL_CDStatus(cd);
			driveID = i;
			return true;
		};
	};
	return false; 
}

bool CDROM_Interface_SDL::ReadSectorsHost(void *buffer, bool raw, unsigned long sector, unsigned long num)
{
	return false;/*TODO*/
};

bool CDROM_Interface_SDL::GetAudioTracks(int& stTrack, int& end, TMSF& leadOut) {

	if (CD_INDRIVE(SDL_CDStatus(cd))) {
		stTrack		= 1;
		end			= cd->numtracks;
		FRAMES_TO_MSF(cd->track[cd->numtracks].offset,&leadOut.min,&leadOut.sec,&leadOut.fr);
	}
	return CD_INDRIVE(SDL_CDStatus(cd));
}

bool CDROM_Interface_SDL::GetAudioTrackInfo(int track, TMSF& start, unsigned char& attr) {
	if (CD_INDRIVE(SDL_CDStatus(cd))) {
		FRAMES_TO_MSF(cd->track[track-1].offset,&start.min,&start.sec,&start.fr);
		attr	= cd->track[track-1].type<<4;//sdl uses 0 for audio and 4 for data. instead of 0x00 and 0x40
	}
	return CD_INDRIVE(SDL_CDStatus(cd));	
}

bool CDROM_Interface_SDL::GetAudioSub(unsigned char& attr, unsigned char& track, unsigned char& index, TMSF& relPos, TMSF& absPos) {
	if (CD_INDRIVE(SDL_CDStatus(cd))) {
		track	= cd->cur_track;
		index	= cd->cur_track;
		attr	= cd->track[track].type<<4;
		FRAMES_TO_MSF(cd->cur_frame,&relPos.min,&relPos.sec,&relPos.fr);
		FRAMES_TO_MSF(cd->cur_frame+cd->track[track].offset,&absPos.min,&absPos.sec,&absPos.fr);
	}
	return CD_INDRIVE(SDL_CDStatus(cd));		
}

bool CDROM_Interface_SDL::GetAudioStatus(bool& playing, bool& pause){
	if (CD_INDRIVE(SDL_CDStatus(cd))) {
		playing = (cd->status==CD_PLAYING);
		pause	= (cd->status==CD_PAUSED);
	}
	return CD_INDRIVE(SDL_CDStatus(cd));
}
	
bool CDROM_Interface_SDL::GetMediaTrayStatus(bool& mediaPresent, bool& mediaChanged, bool& trayOpen) {
	SDL_CDStatus(cd);
	mediaPresent = (cd->status!=CD_TRAYEMPTY) && (cd->status!=CD_ERROR);
	mediaChanged = (oldLeadOut!=cd->track[cd->numtracks].offset);
	trayOpen	 = !mediaPresent;
	oldLeadOut	 = cd->track[cd->numtracks].offset;
	if (mediaChanged) SDL_CDStatus(cd);
	return true;
}

bool CDROM_Interface_SDL::PlayAudioSector(unsigned long start,unsigned long len) { 
	// Has to be there, otherwise wrong cd status report (dunno why, sdl bug ?)
	SDL_CDClose(cd);
	cd = SDL_CDOpen(driveID);
	bool success = (SDL_CDPlay(cd,start+150,len)==0);
	return success;
}

bool CDROM_Interface_SDL::PauseAudio(bool resume) { 
	bool success;
	if (resume) success = (SDL_CDResume(cd)==0);
	else		success = (SDL_CDPause (cd)==0);
	return success;
}

bool CDROM_Interface_SDL::StopAudio(void) {
	// Has to be there, otherwise wrong cd status report (dunno why, sdl bug ?)
	SDL_CDClose(cd);
	cd = SDL_CDOpen(driveID);
	bool success = (SDL_CDStop(cd)==0);
	return success;
}

bool CDROM_Interface_SDL::LoadUnloadMedia(bool unload) {
	bool success = (SDL_CDEject(cd)==0);
	return success;
}

int CDROM_GetMountType(char* path, int forceCD) {
// 0 - physical CDROM
// 1 - Iso file
// 2 - subdirectory
	// 1. Smells like a real cdrom 
	// if ((strlen(path)<=3) && (path[2]=='\\') && (strchr(path,'\\')==strrchr(path,'\\')) && 	(GetDriveType(path)==DRIVE_CDROM)) return 0;

	const char* cdName;
	char buffer[512];
	strcpy(buffer,path);
#if defined (WIN32) || defined(OS2)
	upcase(buffer);
#endif

	int num = SDL_CDNumDrives();
	// If cd drive is forced then check if its in range and return 0
	if ((forceCD>=0) && (forceCD<num)) {
		LOG(LOG_ALL,LOG_ERROR)("CDROM: Using drive %d",forceCD);
		return 0;
	}

	// compare names
	for (int i=0; i<num; i++) {
		cdName = SDL_CDName(i);
		if (strcmp(buffer,cdName)==0) return 0;
	};
	
	// Detect ISO
	struct stat file_stat;
	if ((stat(path, &file_stat) == 0) && (file_stat.st_mode & S_IFREG)) return 1; 
	return 2;
}

// ******************************************************
// Fake CDROM
// ******************************************************

bool CDROM_Interface_Fake :: GetAudioTracks(int& stTrack, int& end, TMSF& leadOut) {
	stTrack = end = 1;
	leadOut.min	= 60;
	leadOut.sec = leadOut.fr = 0;
	return true;
}

bool CDROM_Interface_Fake :: GetAudioTrackInfo(int track, TMSF& start, unsigned char& attr) {
	if (track>1) return false;
	start.min = start.fr = 0;
	start.sec = 2;
	attr	  = 0x60; // data / permitted
	return true;
}

bool CDROM_Interface_Fake :: GetAudioSub(unsigned char& attr, unsigned char& track, unsigned char& index, TMSF& relPos, TMSF& absPos){
	attr	= 0;
	track	= index = 1;
	relPos.min = relPos.fr = 0; relPos.sec = 2;
	absPos.min = absPos.fr = 0; absPos.sec = 2;
	return true;
}

bool CDROM_Interface_Fake :: GetAudioStatus(bool& playing, bool& pause) {
	playing = pause = false;
	return true;
}

bool CDROM_Interface_Fake :: GetMediaTrayStatus(bool& mediaPresent, bool& mediaChanged, bool& trayOpen) {
	mediaPresent = true;
	mediaChanged = false;
	trayOpen     = false;
	return true;
}

bool CDROM_Interface_Fake::ReadSectorsHost(void *buffer, bool raw, unsigned long sector, unsigned long num)
{
	return false;/*TODO*/
};


