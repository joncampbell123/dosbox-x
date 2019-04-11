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

#if defined(C_SDL2) /* SDL 1.x defines this, SDL 2.x does not */
/** @name Frames / MSF Conversion Functions
 *  Conversion functions from frames to Minute/Second/Frames and vice versa
 */
/*@{*/
#define CD_FPS	75
#define FRAMES_TO_MSF(f, M,S,F)	{					\
	int value = f;							\
	*(F) = value%CD_FPS;						\
	value /= CD_FPS;						\
	*(S) = value%60;						\
	value /= 60;							\
	*(M) = value;							\
}
#define MSF_TO_FRAMES(M, S, F)	((M)*60*CD_FPS+(S)*CD_FPS+(F))
#endif /* C_SDL2 */

#define RAW_SECTOR_SIZE		2352
#define COOKED_SECTOR_SIZE	2048

enum { CDROM_USE_SDL, CDROM_USE_ASPI, CDROM_USE_IOCTL_DIO, CDROM_USE_IOCTL_DX, CDROM_USE_IOCTL_MCI };

//! \brief CD-ROM time stamp
//!
//! \description CD-ROM time is represented as minutes, seconds, and frames (75 per second)
typedef struct SMSF {
    //! \brief Time, minutes field
    unsigned char   min;
    //! \brief Time, seconds field
    unsigned char   sec;
    //! \brief Time, frame field
    unsigned char   fr;
} TMSF;

//! \brief Output and channel control state
typedef struct SCtrl {
    //! \brief output channel
    Bit8u           out[4];
    //! \brief channel volume
    Bit8u           vol[4];
} TCtrl;

extern int CDROM_GetMountType(char* path, int force);

//! \brief Base CD-ROM interface class
//!
//! \brief This provides the base C++ class for a CD-ROM interface in CD-ROM emulation
class CDROM_Interface
{
public:
//	CDROM_Interface						(void);
	virtual ~CDROM_Interface			(void) {};

    //! \brief Set the device associated with this interface, if supported by emulation
	virtual bool	SetDevice			(char* path, int forceCD) = 0;

    //! \brief Get UPC string from the CD-ROM
	virtual bool	GetUPC				(unsigned char& attr, char* upc) = 0;

    //! \brief Retrieve start and end tracks and lead out position
	virtual bool	GetAudioTracks		(int& stTrack, int& end, TMSF& leadOut) = 0;

    //! \brief Retrieve start and attributes for a specific track
	virtual bool	GetAudioTrackInfo	(int track, TMSF& start, unsigned char& attr) = 0;

    //! \brief Get subchannel data of the sectors at the current position, and retrieve current position
	virtual bool	GetAudioSub			(unsigned char& attr, unsigned char& track, unsigned char& index, TMSF& relPos, TMSF& absPos) = 0;

    //! \brief Get audio playback status
	virtual bool	GetAudioStatus		(bool& playing, bool& pause) = 0;

    //! \brief Get media tray status
	virtual bool	GetMediaTrayStatus	(bool& mediaPresent, bool& mediaChanged, bool& trayOpen) = 0;

    //! \brief Initiate audio playback starting at sector and for how many
	virtual bool	PlayAudioSector		(unsigned long start,unsigned long len) = 0;

    //! \brief Pause audio playback
	virtual bool	PauseAudio			(bool resume) = 0;

    //! \brief Stop audio playback
	virtual bool	StopAudio			(void) = 0;

    //! \brief Set channel control data (TODO: clarify)
	virtual void	ChannelControl		(TCtrl ctrl) = 0;

    //! \brief Read sector data into guest memory
	virtual bool	ReadSectors			(PhysPt buffer, bool raw, unsigned long sector, unsigned long num) = 0;

    //! \brief Read sector data into host memory (for IDE emulation)
	virtual bool	ReadSectorsHost			(void* buffer, bool raw, unsigned long sector, unsigned long num) = 0;

    //! \brief Load (close/spin up) or unload (eject/spin down) media
	virtual bool	LoadUnloadMedia		(bool unload) = 0;

    //! \brief TODO?
	virtual void	InitNewMedia		(void) {};
};	

//! \brief Dummy CD-ROM interface
//!
//! \brief CD-ROM emulation when no actual emulation is available
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
	void	ChannelControl		(TCtrl ctrl) { (void)ctrl; return; };
	bool	ReadSectors			(PhysPt /*buffer*/, bool /*raw*/, unsigned long /*sector*/, unsigned long /*num*/) { return true; };
	/* This is needed for IDE hack, who's buffer does not exist in DOS physical memory */
	bool	ReadSectorsHost			(void* buffer, bool raw, unsigned long sector, unsigned long num);

	bool	LoadUnloadMedia		(bool /*unload*/) { return true; };
};	

//! \brief Image CD-ROM interface
//!
//! \brief This provides CD-ROM emulation from .ISO and .BIN/.CUE images on the host system
class CDROM_Interface_Image : public CDROM_Interface
{
private:
    //! \brief Base C++ class for reading the image
	class TrackFile {
	public:
		virtual bool read(Bit8u *buffer, int seek, int count) = 0;
		virtual int getLength() = 0;
		virtual ~TrackFile() { };
	};

    //! \brief Binary file reader for the image
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

    //! \brief CD-ROM track definition
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
    //! \brief Constructor, with parameter for subunit
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

    //! \brief Sector read (one sector), where the image decoding is done.
	bool	ReadSector		(Bit8u *buffer, bool raw, unsigned long sector);

    //! \brief Indicate whether the image has a data track
	bool	HasDataTrack		(void);

    //! \brief Flag to track if images have been initialized
    //!
    //! \description Whether images[] has been initialized.
    //!              Note that images_init and images[] are static and
    //!              they are not specific to any one C++ class instance.
static bool images_init;
    //! \brief Array of CD-ROM images, one per drive letter.
    //!
    //! \description images[] is static and not specific to any C++ class instance.
static	CDROM_Interface_Image* images[26];

private:
	// player
static	void	CDAudioCallBack(Bitu len);
	int	GetTrack(int sector);

    //! \brief Virtual CD audio "player"
    //!
    //! \description This struct is used to maintain state to emulate playing CD audio
    //!              tracks from the image.
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

#endif /* __CDROM_INTERFACE__ */
