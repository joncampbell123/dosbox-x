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


#ifndef __CDROM_INTERFACE__
#define __CDROM_INTERFACE__

#define MAX_ASPI_CDROM	5

#include <string.h>
#include <string>
#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#if !defined(HX_DOS) && !(defined(__MINGW32__) && !defined(__MINGW64_VERSION_MAJOR))
#include <thread>
#endif

#include "mem.h"
#include "mixer.h"
#include "../libs/decoders/SDL_sound.h"
#include "../libs/libchdr/chd.h"

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

#include "../../vs/sdl/src/cdrom/compat_SDL_cdrom.h"
#endif /* C_SDL2 */

#define RAW_SECTOR_SIZE		2352
#define COOKED_SECTOR_SIZE	2048
#define AUDIO_DECODE_BUFFER_SIZE 16512

#define REDBOOK_FRAME_PADDING 150u  // The relationship between High Sierra sectors and Redbook
                                    // frames is described by the equation:
                                    // Sector = Minute * 60 * 75 + Second * 75 + Frame - 150

enum { CDROM_USE_SDL, CDROM_USE_ASPI, CDROM_USE_IOCTL_DIO, CDROM_USE_IOCTL_DX, CDROM_USE_IOCTL_MCI };

//! \brief CD-ROM time stamp
//!
//! \description CD-ROM time is represented as minutes, seconds, and frames (75 per second)
typedef struct SMSF {
    //! \brief Time, minutes field
    unsigned char   min = 0;
    //! \brief Time, seconds field
    unsigned char   sec = 0;
    //! \brief Time, frame field
    unsigned char   fr = 0;
} TMSF;

//! \brief Output and channel control state
typedef struct SCtrl {
    //! \brief output channel
    uint8_t           out[4];
    //! \brief channel volume
    uint8_t           vol[4];
} TCtrl;

template<typename T1, typename T2>
inline constexpr T1 ceil_udivide(const T1 x, const T2 y) noexcept {
	static_assert(std::is_unsigned<T1>::value, "First parameter should be unsigned");
	static_assert(std::is_unsigned<T2>::value, "Second parameter should be unsigned");
	return (x != 0) ? 1 + ((x - 1) / y) : 0;
}

extern int CDROM_GetMountType(const char* path, int forceCD);

//! \brief Base CD-ROM interface class
//!
//! \brief This provides the base C++ class for a CD-ROM interface in CD-ROM emulation
class CDROM_Interface
{
public:
	enum INTERFACE_TYPE {
		ID_BASE=0,
		ID_FAKE,
		ID_IMAGE
	};

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

	INTERFACE_TYPE class_id = ID_BASE;
};	

//! \brief CD-ROM interface to SDL 1.x CD-ROM support
//!
//! \brief This connects CD-ROM emulation to the CD-ROM functions provided by SDL 1.x
class CDROM_Interface_SDL : public CDROM_Interface
{
public:
	CDROM_Interface_SDL			(void);
	virtual ~CDROM_Interface_SDL(void);

    /* base C++ class overrides, no documentation needed */
	bool	SetDevice			(char* path, int forceCD) override;
	bool	GetUPC				(unsigned char& attr, char* upc) override { attr = 0; strcpy(upc,"UPC"); return true; };
	bool	GetAudioTracks		(int& stTrack, int& end, TMSF& leadOut) override;
	bool	GetAudioTrackInfo	(int track, TMSF& start, unsigned char& attr) override;
	bool	GetAudioSub			(unsigned char& attr, unsigned char& track, unsigned char& index, TMSF& relPos, TMSF& absPos) override;
	bool	GetAudioStatus		(bool& playing, bool& pause) override;
	bool	GetMediaTrayStatus	(bool& mediaPresent, bool& mediaChanged, bool& trayOpen) override;
	bool	PlayAudioSector		(unsigned long start,unsigned long len) override;
	bool	PauseAudio			(bool resume) override;
	bool	StopAudio			(void) override;
	void	ChannelControl		(TCtrl ctrl) override { (void)ctrl; return; };
	bool	ReadSectors			(PhysPt /*buffer*/, bool /*raw*/, unsigned long /*sector*/, unsigned long /*num*/) override { return false; };
	/* This is needed for IDE hack, who's buffer does not exist in DOS physical memory */
	bool	ReadSectorsHost			(void* buffer, bool raw, unsigned long sector, unsigned long num) override;
	bool	LoadUnloadMedia		(bool unload) override;

private:
    //! \brief Open the device
	bool	Open				(void);

    //! \brief Close the device
	void	Close				(void);

    //! \brief SDL 1.x CD-ROM device object
    SDL_CD* cd = NULL;
    int driveID = 0;
    Uint32 oldLeadOut = 0;
};

//! \brief Dummy CD-ROM interface
//!
//! \brief CD-ROM emulation when no actual emulation is available
class CDROM_Interface_Fake : public CDROM_Interface
{
public:
	bool	SetDevice			(char* /*path*/, int /*forceCD*/) override { return true; };
	bool	GetUPC				(unsigned char& attr, char* upc) override { attr = 0; strcpy(upc,"UPC"); return true; };
	bool	GetAudioTracks		(int& stTrack, int& end, TMSF& leadOut) override;
	bool	GetAudioTrackInfo	(int track, TMSF& start, unsigned char& attr) override;
	bool	GetAudioSub			(unsigned char& attr, unsigned char& track, unsigned char& index, TMSF& relPos, TMSF& absPos) override;
	bool	GetAudioStatus		(bool& playing, bool& pause) override;
	bool	GetMediaTrayStatus	(bool& mediaPresent, bool& mediaChanged, bool& trayOpen) override;
	bool	PlayAudioSector		(unsigned long /*start*/,unsigned long /*len*/) override { return true; };
	bool	PauseAudio			(bool /*resume*/) override { return true; };
	bool	StopAudio			(void) override { return true; };
	void	ChannelControl		(TCtrl ctrl) override { (void)ctrl; return; };
	bool	ReadSectors			(PhysPt /*buffer*/, bool /*raw*/, unsigned long /*sector*/, unsigned long /*num*/) override { return true; };
	/* This is needed for IDE hack, who's buffer does not exist in DOS physical memory */
	bool	ReadSectorsHost			(void* buffer, bool raw, unsigned long sector, unsigned long num) override;

	bool	LoadUnloadMedia		(bool /*unload*/) override { return true; };

	CDROM_Interface_Fake() { class_id = ID_FAKE; }

	bool	isEmpty = false;
};	

//! \brief Image CD-ROM interface
//!
//! \brief This provides CD-ROM emulation from .ISO and .BIN/.CUE images on the host system
class CDROM_Interface_Image : public CDROM_Interface
{
private:
	// Nested Class Definitions
	class TrackFile {
	protected:
		TrackFile(uint16_t _chunkSize) : chunkSize(_chunkSize) {}
	public:
		virtual          ~TrackFile() = default;
		virtual bool     read(uint8_t *buffer,int64_t seek, int count) = 0;
		virtual bool     seek(int64_t offset) = 0;
		virtual uint16_t   decode(uint8_t *buffer) = 0;
		virtual uint16_t   getEndian() = 0;
		virtual uint32_t   getRate() = 0;
		virtual uint8_t    getChannels() = 0;
		virtual int64_t    getLength() = 0;
		virtual void setAudioPosition(uint32_t pos) = 0;
		const uint16_t chunkSize = 0;
		uint32_t audio_pos = UINT32_MAX; // last position when playing audio
	};

    //! \brief Binary file reader for the image
	class BinaryFile : public TrackFile {
	public:
		BinaryFile      (const char *filename, bool &error);
		~BinaryFile     ();

		BinaryFile      () = delete;
		BinaryFile      (const BinaryFile&) = delete; // prevent copying
		BinaryFile&     operator= (const BinaryFile&) = delete; // prevent assignment

		bool            read(uint8_t *buffer,int64_t seek, int count) override;
		bool            seek(int64_t offset) override;
		uint16_t          decode(uint8_t *buffer) override;
		uint16_t          getEndian() override;
		uint32_t          getRate() override { return 44100; }
		uint8_t           getChannels() override { return 2; }
		int64_t           getLength() override;
		void setAudioPosition(uint32_t pos) override { audio_pos = pos; }
	private:
		std::ifstream   *file;
	};

	class AudioFile : public TrackFile {
	public:
		AudioFile       (const char *filename, bool &error);
		~AudioFile      ();

		AudioFile       () = delete;
		AudioFile       (const AudioFile&) = delete; // prevent copying
		AudioFile&      operator= (const AudioFile&) = delete; // prevent assignment

		bool            read(uint8_t *buffer,int64_t seek, int count) override { (void)buffer; (void)seek; (void)count; return false; }
		bool            seek(int64_t offset) override;
		uint16_t          decode(uint8_t *buffer) override;
		uint16_t          getEndian() override;
		uint32_t          getRate() override;
		uint8_t           getChannels() override;
		int64_t           getLength() override;
        void setAudioPosition(uint32_t pos) override { (void)pos;/*unused*/ }
	private:
		Sound_Sample    *sample = nullptr;
	};

    class CHDFile : public TrackFile {
    public:
        CHDFile(const char* filename, bool& error);
        ~CHDFile();

        CHDFile() = delete;
        CHDFile(const CHDFile&) = delete;
        CHDFile& operator= (const CHDFile&) = delete;

        bool            read(uint8_t* buffer, int64_t seek, int count) override;
        bool            seek(int64_t offset) override;
        uint16_t        decode(uint8_t* buffer) override;
        uint16_t        getEndian() override;
        uint32_t        getRate() override { return 44100; }
        uint8_t         getChannels() override { return 2; }
        int64_t         getLength() override;
        void setAudioPosition(uint32_t pos) override { audio_pos = pos; }
        chd_file*       getChd() { return this->chd; }
    private:
              chd_file*   chd               = nullptr;
        const chd_header* header            = nullptr; // chd header
                /*
                    TODO: cache more than one hunk
                    or wait for https://github.com/rtissera/libchdr/issues/36
                */
              uint8_t*     hunk_buffer       = nullptr; // buffer to hold one hunk // size of hunks in CHD up to 1 MiB
              uint8_t*     hunk_buffer_next  = nullptr; // index + 1 prefetch
              int          hunk_buffer_index = -1;      // hunk index for buffer
#if !defined(HX_DOS) && !(defined(__MINGW32__) && !defined(__MINGW64_VERSION_MAJOR))
              std::thread* hunk_thread       = nullptr; // used for prefetch
              bool         hunk_thread_error = true;
#endif
    public:
              bool         skip_sync         = false;   // this will fail if a CHD contains 2048 and 2352 sector tracks
     };

public:
	// Nested struct definition
	struct Track {
		int number;      // number of tracks (max 99)
		int attr;
        uint32_t start;  // sector number where track starts
        uint32_t length;
        uint32_t skip;
		int sectorSize;
        uint32_t pregap; // sector number where pregap starts
		bool mode2;
		TrackFile *file;
	};
    //! \brief Constructor, with parameter for subunit
	CDROM_Interface_Image           (uint8_t subUnit);
	virtual ~CDROM_Interface_Image  (void);
	void	InitNewMedia            (void) override {};
	bool	SetDevice               (char *path, int forceCD) override;
	bool	GetUPC                  (unsigned char& attr, char* upc) override;
	bool	GetAudioTracks          (int& stTrack, int& end, TMSF& leadOut) override;
	bool	GetAudioTrackInfo       (int track, TMSF& start, unsigned char& attr) override;
	bool	GetAudioSub             (unsigned char& attr, unsigned char& track, unsigned char& index, TMSF& relPos, TMSF& absPos) override;
	bool	GetAudioStatus          (bool& playing, bool& pause) override;
	bool	GetMediaTrayStatus      (bool& mediaPresent, bool& mediaChanged, bool& trayOpen) override;
	bool	PlayAudioSector         (unsigned long start, unsigned long len) override;
	bool	PauseAudio              (bool resume) override;
	bool	StopAudio               (void) override;
	void	ChannelControl          (TCtrl ctrl) override;
	bool	ReadSectors             (PhysPt buffer, bool raw, unsigned long sector, unsigned long num) override;
	/* This is needed for IDE hack, who's buffer does not exist in DOS physical memory */
	bool	ReadSectorsHost			(void* buffer, bool raw, unsigned long sector, unsigned long num) override;
	bool	LoadUnloadMedia         (bool unload) override;
	//! \brief Indicate whether the image has a data track
	bool	ReadSector              (uint8_t *buffer, bool raw, unsigned long sector);
	//! \brief Indicate whether the image has a data track
	bool	HasDataTrack            (void) const;
	bool	HasAudioTrack           (void) const;
    //! \brief Flag to track if images have been initialized
    //!
    //! \description Whether images[] has been initialized.
    //!              Note that images_init and images[] are static and
    //!              they are not specific to any one C++ class instance.
	static bool images_init;
    //! \brief Array of CD-ROM images, one per drive letter.
    //!
    //! \description images[] is static and not specific to any C++ class instance.
	static CDROM_Interface_Image* images[26];

private:
	static struct imagePlayer {
		CDROM_Interface_Image *cd;
		MixerChannel   *channel;
		SDL_mutex                *mutex             = nullptr;
		void (MixerChannel::*addFrames) (Bitu, const int16_t*) = nullptr;
		uint32_t                 playedTrackFrames  = 0;
		uint32_t                 totalTrackFrames   = 0;
		uint32_t                 startSector        = 0;
		uint32_t                 totalRedbookFrames = 0;
		uint8_t   buffer[AUDIO_DECODE_BUFFER_SIZE];
		uint32_t  startFrame;
		uint32_t  currFrame;
		uint32_t  numFrames;
		bool    isPlaying;
		bool    isPaused;
		bool    ctrlUsed;
		TCtrl   ctrlData;
		TrackFile* trackFile;
		void     (MixerChannel::*addSamples) (Bitu, const int16_t*);
		uint32_t   playbackTotal;
		int      playbackRemaining;
		uint16_t   bufferPos;
		uint16_t   bufferConsumed;
	} player;

	// Private utility functions
	void  ClearTracks();
	bool  LoadIsoFile(char *filename);
	bool  CanReadPVD(TrackFile *file, int sectorSize, bool mode2) const;
	int	  GetTrack(unsigned long sector);
	static void CDAudioCallBack (Bitu len);

	// Private functions for cue sheet processing
	bool  LoadCueSheet(char *cuefile);
	bool  LoadChdFile(char* chdfile);
	bool  LoadCloneCDSheet(char *cuefile);
	bool  GetRealFileName(std::string& filename, std::string& pathname) const;
	bool  GetCueKeyword(std::string &keyword, std::istream &in) const;
	bool  GetCueFrame(int &frames, std::istream &in) const;
	bool  GetCueString(std::string &str, std::istream &in) const;
	bool  AddTrack(Track &curr, int &shift, int prestart, int &totalPregap, int currPregap);
	// member variables
	std::vector<Track>   tracks;
	std::vector<uint8_t> readBuffer;
	std::string          mcn;
	static int           refCount;
    uint8_t                subUnit;
};

#if defined (WIN32)	/* Win 32 */

#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers

#include <windows.h>
#include "wnaspi32.h"			// Aspi stuff 

class CDROM_Interface_Aspi : public CDROM_Interface
{
public:
	virtual ~CDROM_Interface_Aspi(void);

	bool	SetDevice			(char* path, int forceCD) override;

	bool	GetUPC				(unsigned char& attr, char* upc) override;

	bool	GetAudioTracks		(int& stTrack, int& end, TMSF& leadOut) override;
	bool	GetAudioTrackInfo	(int track, TMSF& start, unsigned char& attr) override;
	bool	GetAudioSub			(unsigned char& attr, unsigned char& track, unsigned char& index, TMSF& relPos, TMSF& absPos) override;
	bool	GetAudioStatus		(bool& playing, bool& pause) override;
	bool	GetMediaTrayStatus	(bool& mediaPresent, bool& mediaChanged, bool& trayOpen) override;

	bool	PlayAudioSector		(unsigned long start,unsigned long len) override;
	bool	PauseAudio			(bool resume) override;
	bool	StopAudio			(void) override;
	void	ChannelControl		(TCtrl ctrl) override { (void)ctrl; return; };
	
	bool	ReadSectors			(PhysPt buffer, bool raw, unsigned long sector, unsigned long num) override;
	/* This is needed for IDE hack, who's buffer does not exist in DOS physical memory */
	bool	ReadSectorsHost			(void* buffer, bool raw, unsigned long sector, unsigned long num) override;
	
	bool	LoadUnloadMedia		(bool unload) override;
	
private:
	DWORD	GetTOC				(LPTOC toc);
	HANDLE	OpenIOCTLFile		(char cLetter, BOOL bAsync);
	void	GetIOCTLAdapter		(HANDLE hF,int * iDA,int * iDT,int * iDL);
	bool	ScanRegistryFindKey	(HKEY& hKeyBase);
	bool	ScanRegistry		(HKEY& hKeyBase);
	BYTE	GetHostAdapter		(char* hardwareID);
	bool	GetVendor			(BYTE HA_num, BYTE SCSI_Id, BYTE SCSI_Lun, char* szBuffer);
		
	// ASPI stuff
	BYTE	haId = 0;
	BYTE	target = 0;
	BYTE	lun = 0;
	char	letter = 0;

	// Windows stuff
	HINSTANCE	hASPI = NULL;
	HANDLE		hEvent = NULL;											// global event
	DWORD		(*pGetASPI32SupportInfo)	(void) = NULL;             // ptrs to aspi funcs
	DWORD		(*pSendASPI32Command)		(LPSRB) = NULL;
    TMSF        oldLeadOut = {};
};

class CDROM_Interface_Ioctl : public CDROM_Interface
{
public:
	enum cdioctl_cdatype { CDIOCTL_CDA_DIO, CDIOCTL_CDA_MCI, CDIOCTL_CDA_DX };
	cdioctl_cdatype cdioctl_cda_selected;

	CDROM_Interface_Ioctl		(cdioctl_cdatype ioctl_cda);
	virtual ~CDROM_Interface_Ioctl(void);

	bool	SetDevice			(char* path, int forceCD) override;

	bool	GetUPC				(unsigned char& attr, char* upc) override;

	bool	GetAudioTracks		(int& stTrack, int& end, TMSF& leadOut) override;
	bool	GetAudioTrackInfo	(int track, TMSF& start, unsigned char& attr) override;
	bool	GetAudioSub			(unsigned char& attr, unsigned char& track, unsigned char& index, TMSF& relPos, TMSF& absPos) override;
	bool	GetAudioStatus		(bool& playing, bool& pause) override;
	bool	GetMediaTrayStatus	(bool& mediaPresent, bool& mediaChanged, bool& trayOpen) override;

	bool	PlayAudioSector		(unsigned long start,unsigned long len) override;
	bool	PauseAudio			(bool resume) override;
	bool	StopAudio			(void) override;
	void	ChannelControl		(TCtrl ctrl) override;
	
	bool	ReadSector			(uint8_t *buffer, bool raw, unsigned long sector);
	bool	ReadSectors			(PhysPt buffer, bool raw, unsigned long sector, unsigned long num) override;
	/* This is needed for IDE hack, who's buffer does not exist in DOS physical memory */
	bool	ReadSectorsHost			(void* buffer, bool raw, unsigned long sector, unsigned long num) override;
	
	bool	LoadUnloadMedia		(bool unload) override;

	void	InitNewMedia		(void) override { Close(); Open(); };
private:

	bool	Open				(void);
	void	Close				(void);

	char	pathname[32];
	HANDLE	hIOCTL;
	TMSF	oldLeadOut;


	/* track start/length data */
	bool	track_start_valid;
	int		track_start_first,track_start_last;
	int		track_start[128];

	bool	GetAudioTracksAll	(void);


	/* mci audio cd interface */
	bool	use_mciplay;
	int		mci_devid;

	bool	mci_CDioctl				(UINT msg, DWORD flags, void *arg);
	bool	mci_CDOpen				(char drive);
	bool	mci_CDClose				(void);
	bool	mci_CDPlay				(int start, int length);
	bool	mci_CDPause				(void);
	bool	mci_CDResume			(void);
	bool	mci_CDStop				(void);
	int		mci_CDStatus			(void);
	bool	mci_CDPosition			(int *position);


	/* digital audio extraction cd interface */
	static void dx_CDAudioCallBack(Bitu len);

	bool	use_dxplay;
	static  struct dxPlayer {
		CDROM_Interface_Ioctl *cd;
		MixerChannel	*channel;
		SDL_mutex		*mutex;
		uint8_t   buffer[8192];
		int     bufLen;
		int     currFrame;	
		int     targetFrame;
		bool    isPlaying;
		bool    isPaused;
		bool    ctrlUsed;
		TCtrl   ctrlData;
	} player;

};

#endif /* WIN 32 */

#if defined (LINUX) || defined(OS2)

class CDROM_Interface_Ioctl : public CDROM_Interface_SDL
{
public:
	CDROM_Interface_Ioctl		(void);

	bool	SetDevice		(char* path, int forceCD) override;
	bool	GetUPC			(unsigned char& attr, char* upc) override;
	bool	ReadSectors		(PhysPt buffer, bool raw, unsigned long sector, unsigned long num) override;
	/* This is needed for IDE hack, who's buffer does not exist in DOS physical memory */
	bool	ReadSectorsHost			(void* buffer, bool raw, unsigned long sector, unsigned long num) override;

private:
	char	device_name[512];
};

#endif /* LINUX */

#endif /* __CDROM_INTERFACE__ */
