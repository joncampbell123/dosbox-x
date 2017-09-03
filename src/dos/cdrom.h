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


#ifndef __CDROM_INTERFACE__
#define __CDROM_INTERFACE__

#define MAX_ASPI_CDROM	5

#include <string.h>
#include <string>
#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include "dosbox.h"
#include "mem.h"
#include "mixer.h"
#include "SDL.h"
#include "SDL_thread.h"

#define RAW_SECTOR_SIZE		2352
#define COOKED_SECTOR_SIZE	2048

enum { CDROM_USE_SDL, CDROM_USE_ASPI, CDROM_USE_IOCTL_DIO, CDROM_USE_IOCTL_DX, CDROM_USE_IOCTL_MCI };

typedef struct SMSF {
	unsigned char min;
	unsigned char sec;
	unsigned char fr;
} TMSF;

typedef struct SCtrl {
	Bit8u	out[4];			// output channel
	Bit8u	vol[4];			// channel volume
} TCtrl;

extern int CDROM_GetMountType(char* path, int force);

class CDROM_Interface
{
public:
//	CDROM_Interface						(void);
	virtual ~CDROM_Interface			(void) {};

	virtual bool	SetDevice			(char* path, int forceCD) = 0;

	virtual bool	GetUPC				(unsigned char& attr, char* upc) = 0;

	virtual bool	GetAudioTracks		(int& stTrack, int& end, TMSF& leadOut) = 0;
	virtual bool	GetAudioTrackInfo	(int track, TMSF& start, unsigned char& attr) = 0;
	virtual bool	GetAudioSub			(unsigned char& attr, unsigned char& track, unsigned char& index, TMSF& relPos, TMSF& absPos) = 0;
	virtual bool	GetAudioStatus		(bool& playing, bool& pause) = 0;
	virtual bool	GetMediaTrayStatus	(bool& mediaPresent, bool& mediaChanged, bool& trayOpen) = 0;

	virtual bool	PlayAudioSector		(unsigned long start,unsigned long len) = 0;
	virtual bool	PauseAudio			(bool resume) = 0;
	virtual bool	StopAudio			(void) = 0;
	virtual void	ChannelControl		(TCtrl ctrl) = 0;
	
	virtual bool	ReadSectors			(PhysPt buffer, bool raw, unsigned long sector, unsigned long num) = 0;
	/* This is needed for IDE hack, who's buffer does not exist in DOS physical memory */
	virtual bool	ReadSectorsHost			(void* buffer, bool raw, unsigned long sector, unsigned long num) = 0;

	virtual bool	LoadUnloadMedia		(bool unload) = 0;
	
	virtual void	InitNewMedia		(void) {};
};	

class CDROM_Interface_Fake : public CDROM_Interface
{
public:
	bool	SetDevice			(char* /*path*/, int /*forceCD*/) { return true; };
	bool	GetUPC				(unsigned char& attr, char* upc) { attr = 0; strcpy(upc,"UPC"); return true; };
	bool	GetAudioTracks		(int& stTrack, int& end, TMSF& leadOut);
	bool	GetAudioTrackInfo	(int track, TMSF& start, unsigned char& attr);
	bool	GetAudioSub			(unsigned char& attr, unsigned char& track, unsigned char& index, TMSF& relPos, TMSF& absPos);
	bool	GetAudioStatus		(bool& playing, bool& pause);
	bool	GetMediaTrayStatus	(bool& mediaPresent, bool& mediaChanged, bool& trayOpen);
	bool	PlayAudioSector		(unsigned long /*start*/,unsigned long /*len*/) { return true; };
	bool	PauseAudio			(bool /*resume*/) { return true; };
	bool	StopAudio			(void) { return true; };
	void	ChannelControl		(TCtrl ctrl) { return; };
	bool	ReadSectors			(PhysPt /*buffer*/, bool /*raw*/, unsigned long /*sector*/, unsigned long /*num*/) { return true; };
	/* This is needed for IDE hack, who's buffer does not exist in DOS physical memory */
	bool	ReadSectorsHost			(void* buffer, bool raw, unsigned long sector, unsigned long num);

	bool	LoadUnloadMedia		(bool /*unload*/) { return true; };
};	

class CDROM_Interface_Image : public CDROM_Interface
{
private:
	class TrackFile {
	public:
		virtual bool read(Bit8u *buffer, int seek, int count) = 0;
		virtual int getLength() = 0;
		virtual ~TrackFile() { };
	};
	
	class BinaryFile : public TrackFile {
	public:
		BinaryFile(const char *filename, bool &error);
		~BinaryFile();
		bool read(Bit8u *buffer, int seek, int count);
		int getLength();
	private:
		BinaryFile();
		std::ifstream *file;
	};

	struct Track {
		int number;
		int attr;
		int start;
		int length;
		int skip;
		int sectorSize;
		bool mode2;
		TrackFile *file;
	};
	
public:
	CDROM_Interface_Image		(Bit8u subUnit);
	virtual ~CDROM_Interface_Image	(void);
	void	InitNewMedia		(void);
	bool	SetDevice		(char* path, int forceCD);
	bool	GetUPC			(unsigned char& attr, char* upc);
	bool	GetAudioTracks		(int& stTrack, int& end, TMSF& leadOut);
	bool	GetAudioTrackInfo	(int track, TMSF& start, unsigned char& attr);
	bool	GetAudioSub		(unsigned char& attr, unsigned char& track, unsigned char& index, TMSF& relPos, TMSF& absPos);
	bool	GetAudioStatus		(bool& playing, bool& pause);
	bool	GetMediaTrayStatus	(bool& mediaPresent, bool& mediaChanged, bool& trayOpen);
	bool	PlayAudioSector		(unsigned long start,unsigned long len);
	bool	PauseAudio		(bool resume);
	bool	StopAudio		(void);
	void	ChannelControl		(TCtrl ctrl);
	bool	ReadSectors		(PhysPt buffer, bool raw, unsigned long sector, unsigned long num);
	/* This is needed for IDE hack, who's buffer does not exist in DOS physical memory */
	bool	ReadSectorsHost			(void* buffer, bool raw, unsigned long sector, unsigned long num);
	bool	LoadUnloadMedia		(bool unload);
	bool	ReadSector		(Bit8u *buffer, bool raw, unsigned long sector);
	bool	HasDataTrack		(void);
	
static bool images_init;
static	CDROM_Interface_Image* images[26];

private:
	// player
static	void	CDAudioCallBack(Bitu len);
	int	GetTrack(int sector);

static  struct imagePlayer {
		CDROM_Interface_Image *cd;
		MixerChannel   *channel;
		SDL_mutex 	*mutex;
		Bit8u   buffer[8192];
		int     bufLen;
		int     currFrame;	
		int     targetFrame;
		bool    isPlaying;
		bool    isPaused;
		bool    ctrlUsed;
		TCtrl   ctrlData;
	} player;
	
	void 	ClearTracks();
	bool	LoadIsoFile(char *filename);
	bool	CanReadPVD(TrackFile *file, int sectorSize, bool mode2);
	// cue sheet processing
	bool	LoadCueSheet(char *cuefile);
	bool	GetRealFileName(std::string& filename, std::string& pathname);
	bool	GetCueKeyword(std::string &keyword, std::istream &in);
	bool	GetCueFrame(int &frames, std::istream &in);
	bool	GetCueString(std::string &str, std::istream &in);
	bool	AddTrack(Track &curr, int &shift, int prestart, int &totalPregap, int currPregap);

static	int	refCount;
	std::vector<Track>	tracks;
typedef	std::vector<Track>::iterator	track_it;
	std::string	mcn;
	Bit8u	subUnit;
};

// replacement for SDL 1.x function.
// FIXME: Test heavily.
static inline unsigned long MSF_TO_FRAMES(const unsigned int M,const unsigned int S,const unsigned int F) {
    unsigned long sec = (M * 60UL) + S;
    unsigned long fr = (sec * 75UL) + F;

    if (fr >= 150UL) fr -= 150UL;
    else fr = 0;

    return fr;
}

static inline void FRAMES_TO_MSF(unsigned long sec,unsigned char *M,unsigned char *S,unsigned char *F) {
    sec += 150UL;
    *F = (sec % 75UL);
    sec /= 75UL;
    *S = (sec % 60UL);
    sec /= 60UL;
    *M = sec;
}

#if defined (WIN32)	/* Win 32 */

#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers

#include <windows.h>
#include "wnaspi32.h"			// Aspi stuff 

class CDROM_Interface_Aspi : public CDROM_Interface
{
public:
	CDROM_Interface_Aspi		(void);
	virtual ~CDROM_Interface_Aspi(void);

	bool	SetDevice			(char* path, int forceCD);

	bool	GetUPC				(unsigned char& attr, char* upc);

	bool	GetAudioTracks		(int& stTrack, int& end, TMSF& leadOut);
	bool	GetAudioTrackInfo	(int track, TMSF& start, unsigned char& attr);
	bool	GetAudioSub			(unsigned char& attr, unsigned char& track, unsigned char& index, TMSF& relPos, TMSF& absPos);
	bool	GetAudioStatus		(bool& playing, bool& pause);
	bool	GetMediaTrayStatus	(bool& mediaPresent, bool& mediaChanged, bool& trayOpen);

	bool	PlayAudioSector		(unsigned long start,unsigned long len);
	bool	PauseAudio			(bool resume);
	bool	StopAudio			(void);
	void	ChannelControl		(TCtrl ctrl) { return; };
	
	bool	ReadSectors			(PhysPt buffer, bool raw, unsigned long sector, unsigned long num);
	/* This is needed for IDE hack, who's buffer does not exist in DOS physical memory */
	bool	ReadSectorsHost			(void* buffer, bool raw, unsigned long sector, unsigned long num);
	
	bool	LoadUnloadMedia		(bool unload);
	
private:
	DWORD	GetTOC				(LPTOC toc);
	HANDLE	OpenIOCTLFile		(char cLetter, BOOL bAsync);
	void	GetIOCTLAdapter		(HANDLE hF,int * iDA,int * iDT,int * iDL);
	bool	ScanRegistryFindKey	(HKEY& hKeyBase);
	bool	ScanRegistry		(HKEY& hKeyBase);
	BYTE	GetHostAdapter		(char* hardwareID);
	bool	GetVendor			(BYTE HA_num, BYTE SCSI_Id, BYTE SCSI_Lun, char* szBuffer);
		
	// ASPI stuff
	BYTE	haId;
	BYTE	target;
	BYTE	lun;
	char	letter;

	// Windows stuff
	HINSTANCE	hASPI;
	HANDLE		hEvent;											// global event
	DWORD		(*pGetASPI32SupportInfo)	(void);             // ptrs to aspi funcs
	DWORD		(*pSendASPI32Command)		(LPSRB);
	TMSF		oldLeadOut;
};

#endif /* WIN 32 */

#endif /* __CDROM_INTERFACE__ */
