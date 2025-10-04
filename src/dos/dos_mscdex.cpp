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


#include <string.h>
#include <assert.h>
#include <ctype.h>
#include "regs.h"
#include "callback.h"
#include "dos_system.h"
#include "dos_inc.h"
#include "setup.h"
#include "control.h"
#include "support.h"
#include "bios_disk.h"
#include "cpu.h"

#include "cdrom.h"

#define MSCDEX_LOG LOG(LOG_MISC, LOG_DEBUG)
#define MSCDEX_LOG_ERROR LOG(LOG_MISC, LOG_ERROR)

#define MSCDEX_VERSION_HIGH	2
#define MSCDEX_VERSION_LOW	23
#define MSCDEX_MAX_DRIVES	16

// Request Status
#define	REQUEST_STATUS_DONE		0x0100
#define	REQUEST_STATUS_ERROR	0x8000

// Use cdrom Interface
int useCdromInterface	= CDROM_USE_SDL;
int forceCD				= -1;
extern int bootdrive;
extern bool dos_kernel_disabled, bootguest, bootvm, use_quick_reboot, enable_network_redirector;

static Bitu MSCDEX_Strategy_Handler(void); 
static Bitu MSCDEX_Interrupt_Handler(void);

class DOS_DeviceHeader:public MemStruct {
public:
	DOS_DeviceHeader(PhysPt ptr)				{ pt = ptr; };
	
	void	SetNextDeviceHeader	(RealPt ptr)	{ sSave(sDeviceHeader,nextDeviceHeader,ptr);	};
	RealPt	GetNextDeviceHeader	(void)			{ return sGet(sDeviceHeader,nextDeviceHeader);	};
	void	SetAttribute		(uint16_t atr)	{ sSave(sDeviceHeader,devAttributes,atr);		};
	void	SetDriveLetter		(uint8_t letter)	{ sSave(sDeviceHeader,driveLetter,letter);		};
	void	SetNumSubUnits		(uint8_t num)		{ sSave(sDeviceHeader,numSubUnits,num);			};
	uint8_t	GetNumSubUnits		(void)			{ return sGet(sDeviceHeader,numSubUnits);		};
	void	SetName				(char const* _name)	{ MEM_BlockWrite(pt+offsetof(sDeviceHeader,name),_name,8); };
	void	SetInterrupt		(uint16_t ofs)	{ sSave(sDeviceHeader,interrupt,ofs);			};
	void	SetStrategy			(uint16_t ofs)	{ sSave(sDeviceHeader,strategy,ofs);			};

	#ifdef _MSC_VER
	#pragma pack(1)
	#endif
	struct sDeviceHeader{
		RealPt	nextDeviceHeader;
		uint16_t	devAttributes;
		uint16_t	strategy;
		uint16_t	interrupt;
		uint8_t	name[8];
		uint16_t  wReserved;
		uint8_t	driveLetter;
		uint8_t	numSubUnits;
    } GCC_ATTRIBUTE(packed) TDeviceHeader = {};
	#ifdef _MSC_VER
	#pragma pack()
	#endif
};

class CMscdex {
public:
	CMscdex		(const char *name);
	~CMscdex	(void);

	uint16_t		GetVersion			(void)	{ return (MSCDEX_VERSION_HIGH<<8)+MSCDEX_VERSION_LOW; };
	uint16_t		GetNumDrives		(void)	{ return numDrives;			};
	uint16_t		GetFirstDrive		(void)	{ return dinfo[0].drive; };
	uint8_t		GetSubUnit			(uint16_t _drive);
	bool		GetUPC				(uint8_t subUnit, uint8_t& attr, char* upc);

	void		InitNewMedia		(uint8_t subUnit);
	bool		PlayAudioSector		(uint8_t subUnit, uint32_t sector, uint32_t length);
	bool		PlayAudioMSF		(uint8_t subUnit, uint32_t start, uint32_t length);
	bool		StopAudio			(uint8_t subUnit);
	bool		GetAudioStatus		(uint8_t subUnit, bool& playing, bool& pause, TMSF& start, TMSF& end);

	bool		GetQChannelData	(uint8_t subUnit, uint8_t& attr, uint8_t& track, uint8_t &index, TMSF& rel, TMSF& abs);

	int			RemoveDrive			(uint16_t _drive);
	int			AddDrive			(uint16_t _drive, char* physicalPath, uint8_t& subUnit);
	bool 		HasDrive			(uint16_t drive);
	void		ReplaceDrive		(CDROM_Interface* newCdrom, uint8_t subUnit);
	void		GetDrives			(PhysPt data);
	void		GetDriverInfo		(PhysPt data);
	bool		GetVolumeName		(uint8_t subUnit, char* data);
	bool		GetFileName			(uint16_t drive, uint16_t pos, PhysPt data);
	bool		GetDirectoryEntry	(uint16_t drive, bool copyFlag, PhysPt pathname, PhysPt buffer, uint16_t& error);
	bool		ReadVTOC			(uint16_t drive, uint16_t volume, PhysPt data, uint16_t& offset, uint16_t& error);
	bool		ReadSectors			(uint16_t drive, uint32_t sector, uint16_t num, PhysPt data);
	bool		ReadSectors			(uint8_t subUnit, bool raw, uint32_t sector, uint16_t num, PhysPt data);
	bool		ReadSectorsMSF		(uint8_t subUnit, bool raw, uint32_t start, uint16_t num, PhysPt data);
	bool		SendDriverRequest	(uint16_t drive, PhysPt data);
	bool		IsValidDrive		(uint16_t _drive);
	bool		GetCDInfo			(uint8_t subUnit, uint8_t& tr1, uint8_t& tr2, TMSF& leadOut);
	uint32_t		GetVolumeSize		(uint8_t subUnit);
	bool		GetTrackInfo		(uint8_t subUnit, uint8_t track, uint8_t& attr, TMSF& start);
	uint16_t		GetStatusWord		(uint8_t subUnit,uint16_t status);
	bool		GetCurrentPos		(uint8_t subUnit, TMSF& pos);
	uint32_t		GetDeviceStatus		(uint8_t subUnit);
	bool		GetMediaStatus		(uint8_t subUnit, uint8_t& status);
	bool		LoadUnloadMedia		(uint8_t subUnit, bool unload);
	bool		ResumeAudio			(uint8_t subUnit);
	bool		GetMediaStatus		(uint8_t subUnit, bool& media, bool& changed, bool& trayOpen);
    bool        Seek                (uint8_t subUnit, uint32_t sector);
 
	PhysPt		GetDefaultBuffer	(void);
	PhysPt		GetTempBuffer		(void);

	void SaveState( std::ostream& stream );
	void LoadState( std::istream& stream );

	uint16_t		numDrives = 0;

	typedef struct SDriveInfo {
		uint8_t	drive;			// drive letter in dosbox
		uint8_t	physDrive;		// drive letter in system
		bool	audioPlay;		// audio playing active
		bool	audioPaused;	// audio playing paused
		uint32_t	audioStart;		// StartLoc for resume
		uint32_t	audioEnd;		// EndLoc for resume
		bool	locked;			// drive locked ?
		bool	lastResult;		// last operation success ?
		uint32_t	volumeSize;		// for media change
		TCtrl	audioCtrl;		// audio channel control
	} TDriveInfo;

	uint16_t				defaultBufSeg = 0;
	TDriveInfo			dinfo[MSCDEX_MAX_DRIVES];
	CDROM_Interface*		cdrom[MSCDEX_MAX_DRIVES];
	uint16_t		rootDriverHeaderSeg = 0;

	char*			name = NULL;

	bool		ChannelControl		(uint8_t subUnit, TCtrl ctrl);
	bool		GetChannelControl	(uint8_t subUnit, TCtrl& ctrl);
};

CMscdex::CMscdex(const char *_name) {
	assert(_name != NULL);
	assert(strlen(_name) <= 8);
	memset(dinfo,0,sizeof(dinfo));
	for (uint32_t i=0; i<MSCDEX_MAX_DRIVES; i++) cdrom[i] = nullptr;
	name = new char[strlen(_name)+1];
	strcpy(name,_name);
}

CMscdex::~CMscdex(void) {
	if ((bootguest||(use_quick_reboot&&!bootvm))&&bootdrive>=0) return;
	defaultBufSeg = 0;
	for (uint16_t i=0; i<GetNumDrives(); i++) {
		delete cdrom[i];
		cdrom[i] = nullptr;
	}
	delete[] name;
}

void CMscdex::GetDrives(PhysPt data)
{
	for (uint16_t i=0; i<GetNumDrives(); i++) mem_writeb(data+i,dinfo[i].drive);
}

bool CMscdex::IsValidDrive(uint16_t _drive)
{
	_drive &= 0xff; //Only lowerpart (Ultimate domain)
	for (uint16_t i=0; i<GetNumDrives(); i++) if (dinfo[i].drive==_drive) return true;
	return false;
}

uint8_t CMscdex::GetSubUnit(uint16_t _drive)
{
	_drive &= 0xff; //Only lowerpart (Ultimate domain)
	for (uint16_t i=0; i<GetNumDrives(); i++) if (dinfo[i].drive==_drive) return (uint8_t)i;
	return 0xff;
}

int CMscdex::RemoveDrive(uint16_t _drive)
{
	uint16_t idx = MSCDEX_MAX_DRIVES;
	for (uint16_t i=0; i<GetNumDrives(); i++) {
		if (dinfo[i].drive == _drive) {
			idx = i;
			break;
		}
	}

	if (idx == MSCDEX_MAX_DRIVES || (idx!=0 && idx!=GetNumDrives()-1)) return 0;
	delete cdrom[idx];
	if (idx==0) {
		for (uint16_t i=0; i<GetNumDrives(); i++) {
			if (i == MSCDEX_MAX_DRIVES-1) {
				cdrom[i] = nullptr;
				memset(&dinfo[i],0,sizeof(TDriveInfo));
			} else {
				dinfo[i] = dinfo[i+1];
				cdrom[i] = cdrom[i+1];
			}
		}
	} else {
		cdrom[idx] = nullptr;
		memset(&dinfo[idx],0,sizeof(TDriveInfo));
	}
	numDrives--;

	if (GetNumDrives() == 0) {
		DOS_DeviceHeader devHeader(PhysMake(rootDriverHeaderSeg,0));
		uint16_t off = sizeof(DOS_DeviceHeader::sDeviceHeader);
		devHeader.SetStrategy(off+4);		// point to the RETF (To deactivate MSCDEX)
		devHeader.SetInterrupt(off+4);		// point to the RETF (To deactivate MSCDEX)
		devHeader.SetDriveLetter(0);
	} else if (idx==0) {
		DOS_DeviceHeader devHeader(PhysMake(rootDriverHeaderSeg,0));
		devHeader.SetDriveLetter(GetFirstDrive()+1);
	}
	return 1;
}

int CMscdex::AddDrive(uint16_t _drive, char* physicalPath, uint8_t& subUnit)
{
	subUnit = 0;
	if ((Bitu)GetNumDrives()+1>=MSCDEX_MAX_DRIVES) return 4;
	if (GetNumDrives()) {
		// Error check, driveletter have to be in a row
		if (dinfo[0].drive-1!=_drive && dinfo[numDrives-1].drive+1!=_drive) 
			return 1;
	}
	// Set return type to ok
	int result = 0;
	// Get Mounttype and init needed cdrom interface
	switch (CDROM_GetMountType(physicalPath,forceCD)) {
	case 0x00: {	
		LOG(LOG_MISC,LOG_NORMAL)("MSCDEX: Mounting physical cdrom: %s"	,physicalPath);
#if defined (WIN32)
		// Check OS
		OSVERSIONINFO osi;
		osi.dwOSVersionInfoSize = sizeof(osi);
		GetVersionEx(&osi);
		if ((osi.dwPlatformId==VER_PLATFORM_WIN32_NT) && (osi.dwMajorVersion>4)) {
			// only WIN NT/200/XP
			if (useCdromInterface==CDROM_USE_IOCTL_DIO) {
				cdrom[numDrives] = new CDROM_Interface_Ioctl(CDROM_Interface_Ioctl::CDIOCTL_CDA_DIO);
				LOG(LOG_MISC,LOG_NORMAL)("MSCDEX: IOCTL Interface.");
				break;
			}
			if (useCdromInterface==CDROM_USE_IOCTL_DX) {
				cdrom[numDrives] = new CDROM_Interface_Ioctl(CDROM_Interface_Ioctl::CDIOCTL_CDA_DX);
				LOG(LOG_MISC,LOG_NORMAL)("MSCDEX: IOCTL Interface (digital audio extraction).");
				break;
			}
			if (useCdromInterface==CDROM_USE_IOCTL_MCI) {
				cdrom[numDrives] = new CDROM_Interface_Ioctl(CDROM_Interface_Ioctl::CDIOCTL_CDA_MCI);
				LOG(LOG_MISC,LOG_NORMAL)("MSCDEX: IOCTL Interface (media control interface).");
				break;
			}
		}
		if (useCdromInterface==CDROM_USE_ASPI) {
			// all Wins - ASPI
			cdrom[numDrives] = new CDROM_Interface_Aspi();
			LOG(LOG_MISC,LOG_NORMAL)("MSCDEX: ASPI Interface.");
			break;
		}
#endif
#if defined (LINUX) || defined(OS2)
		// Always use IOCTL in Linux or OS/2
		cdrom[numDrives] = new CDROM_Interface_Ioctl();
		LOG(LOG_MISC,LOG_NORMAL)("MSCDEX: IOCTL Interface.");
#else
		// Default case windows and other oses
		cdrom[numDrives] = new CDROM_Interface_SDL();
		LOG(LOG_MISC,LOG_NORMAL)("MSCDEX: SDL Interface.");
#endif
		} break;
	case 0x01:	// iso cdrom interface	
		LOG(LOG_MISC,LOG_NORMAL)("MSCDEX: Mounting iso file as cdrom: %s", physicalPath);
		cdrom[numDrives] = new CDROM_Interface_Image((uint8_t)numDrives);
		break;
	case 0x02:	// fake cdrom interface (directories)
		{
			CDROM_Interface_Fake *fake = new CDROM_Interface_Fake;
			cdrom[numDrives] = fake;
			assert(fake->class_id == CDROM_Interface::INTERFACE_TYPE::ID_FAKE);
			if (!strcmp(physicalPath,"empty")) {
				fake->isEmpty = true;
			}
			else {
				LOG(LOG_MISC,LOG_NORMAL)("MSCDEX: Mounting directory as cdrom: %s",physicalPath);
				LOG(LOG_MISC,LOG_NORMAL)("MSCDEX: You won't have full MSCDEX support !");
				result = 5;
			}
		}
		break;
	default	:	// weird result
		return 6;
	}

	if (!cdrom[numDrives]->SetDevice(physicalPath,forceCD)) {
//		delete cdrom[numDrives] ; mount seems to delete it
		return 3;
	}


	if (rootDriverHeaderSeg==0) {
		
		uint16_t driverSize = sizeof(DOS_DeviceHeader::sDeviceHeader) + 10; // 10 = Bytes for 3 callbacks

		/* should have been assigned by SetName() in constructor, with copy stored by DOS_File::SetName */
		assert(name != NULL);
		/* should have been either given the user's name or a default name */
		assert(*name != 0);

		// Create Device Header
		uint16_t seg = DOS_GetMemory(driverSize/16u+((driverSize%16u)>0u),"MSCDEX device header");
		DOS_DeviceHeader devHeader(PhysMake(seg,0u));
		devHeader.SetNextDeviceHeader		(0xFFFFFFFFul);
		devHeader.SetAttribute			(0xc800u);
		devHeader.SetDriveLetter		(_drive+1u);
		devHeader.SetNumSubUnits		(1u);
		devHeader.SetName			(name);

		//Link it in the device chain
		uint32_t start = dos_infoblock.GetDeviceChain();
		uint16_t segm  = (uint16_t)(start>>16ul);
		uint16_t offm  = (uint16_t)(start&0xFFFFu);
		while(start != 0xFFFFFFFFul) {
			segm  = (uint16_t)(start>>16u);
			offm  = (uint16_t)(start&0xFFFFu);
			start = real_readd(segm,offm);
		}
		real_writed(segm,offm,(unsigned int)seg<<16u);

		// Create Callback Strategy
		uint16_t off = sizeof(DOS_DeviceHeader::sDeviceHeader);
		uint16_t call_strategy=CALLBACK_Allocate();
		CallBack_Handlers[call_strategy]=MSCDEX_Strategy_Handler;
		real_writeb(seg,off+0,(uint8_t)0xFE);		//GRP 4
		real_writeb(seg,off+1,(uint8_t)0x38);		//Extra Callback instruction
		real_writew(seg,off+2,call_strategy);	//The immediate word
		real_writeb(seg,off+4,(uint8_t)0xCB);		//A RETF Instruction
		devHeader.SetStrategy(off);
		
		// Create Callback Interrupt
		off += 5;
		uint16_t call_interrupt=CALLBACK_Allocate();
		CallBack_Handlers[call_interrupt]=MSCDEX_Interrupt_Handler;
		real_writeb(seg,off+0,(uint8_t)0xFE);		//GRP 4
		real_writeb(seg,off+1,(uint8_t)0x38);		//Extra Callback instruction
		real_writew(seg,off+2,call_interrupt);	//The immediate word
		real_writeb(seg,off+4,(uint8_t)0xCB);		//A RETF Instruction
		devHeader.SetInterrupt(off);
		
		rootDriverHeaderSeg = seg;
	
	} else if (GetNumDrives() == 0) {
		DOS_DeviceHeader devHeader(PhysMake(rootDriverHeaderSeg,0));
		uint16_t off = sizeof(DOS_DeviceHeader::sDeviceHeader);
		devHeader.SetDriveLetter(_drive+1);
		devHeader.SetStrategy(off);
		devHeader.SetInterrupt(off+5);
	}

	// Set drive
	DOS_DeviceHeader devHeader(PhysMake(rootDriverHeaderSeg,0));
	devHeader.SetNumSubUnits(devHeader.GetNumSubUnits()+1);

	if (dinfo[0].drive-1==_drive) {
		CDROM_Interface *_cdrom = cdrom[numDrives];
		CDROM_Interface_Image *_cdimg = CDROM_Interface_Image::images[numDrives];
		for (uint16_t i=GetNumDrives(); i>0; i--) {
			dinfo[i] = dinfo[i-1];
			cdrom[i] = cdrom[i-1];
			CDROM_Interface_Image::images[i] = CDROM_Interface_Image::images[i-1];
		}
		cdrom[0] = _cdrom;
		CDROM_Interface_Image::images[0] = _cdimg;
		dinfo[0].drive		= (uint8_t)_drive;
		dinfo[0].physDrive	= (uint8_t)toupper(physicalPath[0]);
		subUnit = 0;
	} else {
		dinfo[numDrives].drive		= (uint8_t)_drive;
		dinfo[numDrives].physDrive	= (uint8_t)toupper(physicalPath[0]);
		subUnit = (uint8_t)numDrives;
	}
	numDrives++;
	// init channel control
	for (uint8_t chan=0;chan<4;chan++) {
		dinfo[subUnit].audioCtrl.out[chan]=chan;
		dinfo[subUnit].audioCtrl.vol[chan]=0xff;
	}
	// stop audio
	StopAudio(subUnit);
	return result;
}

bool CMscdex::HasDrive(uint16_t drive) {
	return (GetSubUnit(drive) != 0xff);
}

void CMscdex::ReplaceDrive(CDROM_Interface* newCdrom, uint8_t subUnit) {
	if (cdrom[subUnit] != NULL) {
		StopAudio(subUnit);
		delete cdrom[subUnit];
	}
	cdrom[subUnit] = newCdrom;
}

PhysPt CMscdex::GetDefaultBuffer(void) {
	if (defaultBufSeg==0 && !dos_kernel_disabled) {
		uint16_t size = (2352*2+15)/16;
		defaultBufSeg = DOS_GetMemory(size,"MSCDEX default buffer");
	}
	return PhysMake(defaultBufSeg,2352);
}

PhysPt CMscdex::GetTempBuffer(void) {
	if (defaultBufSeg==0) {
		uint16_t size = (2352*2+15)/16;
		defaultBufSeg = DOS_GetMemory(size,"MSCDEX temp buffer");
	}
	return PhysMake(defaultBufSeg,0);
}

void CMscdex::GetDriverInfo	(PhysPt data) {
	for (uint16_t i=0; i<GetNumDrives(); i++) {
		mem_writeb(data  ,(uint8_t)i);	// subunit
		mem_writed(data+1,RealMake(rootDriverHeaderSeg,0));
		data+=5;
	}
}

bool CMscdex::GetCDInfo(uint8_t subUnit, uint8_t& tr1, uint8_t& tr2, TMSF& leadOut) {
	if (subUnit>=numDrives) return false;
	int tr1i,tr2i;
	// Assume Media change
	cdrom[subUnit]->InitNewMedia();
	dinfo[subUnit].lastResult = cdrom[subUnit]->GetAudioTracks(tr1i,tr2i,leadOut);
	if (!dinfo[subUnit].lastResult) {
		tr1 = tr2 = 0;
        leadOut.fr = leadOut.min = leadOut.sec = 0;
	} else {
		tr1 = (uint8_t) tr1i;
		tr2 = (uint8_t) tr2i;
	}
	return dinfo[subUnit].lastResult;
}

bool CMscdex::GetTrackInfo(uint8_t subUnit, uint8_t track, uint8_t& attr, TMSF& start) {
	if (subUnit>=numDrives) return false;
	dinfo[subUnit].lastResult = cdrom[subUnit]->GetAudioTrackInfo(track,start,attr);	
	if (!dinfo[subUnit].lastResult) {
		attr = 0;
        start.fr = start.min = start.sec = 0;
	}
	return dinfo[subUnit].lastResult;
}

bool CMscdex::PlayAudioSector(uint8_t subUnit, uint32_t sector, uint32_t length) {
	if (subUnit>=numDrives) return false;
	// If value from last stop is used, this is meant as a resume
	// better start using resume command
	if (dinfo[subUnit].audioPaused && (sector==dinfo[subUnit].audioStart) && (dinfo[subUnit].audioEnd!=0)) {
		dinfo[subUnit].lastResult = cdrom[subUnit]->PauseAudio(true);
	} else 
		dinfo[subUnit].lastResult = cdrom[subUnit]->PlayAudioSector(sector,length);

	if (dinfo[subUnit].lastResult) {
		dinfo[subUnit].audioPlay	= true;
		dinfo[subUnit].audioPaused	= false;
		dinfo[subUnit].audioStart	= sector;
		dinfo[subUnit].audioEnd		= length;
	}
	return dinfo[subUnit].lastResult;
}

bool CMscdex::PlayAudioMSF(uint8_t subUnit, uint32_t start, uint32_t length) {
	if (subUnit>=numDrives) return false;
	uint8_t min		= (uint8_t)(start>>16) & 0xFF;
	uint8_t sec		= (uint8_t)(start>> 8) & 0xFF;
	uint8_t fr		= (uint8_t)(start>> 0) & 0xFF;
	uint32_t sector	= min*60u*75u+sec*75u+fr - 150u;
	return dinfo[subUnit].lastResult = PlayAudioSector(subUnit,sector,length);
}

bool CMscdex::Seek(uint8_t subUnit, uint32_t sector)
{
    if(subUnit >= numDrives) {
        return false;
    }
    dinfo[subUnit].lastResult = cdrom[subUnit]->StopAudio();
    if(dinfo[subUnit].lastResult) {
        dinfo[subUnit].audioPlay = false;
        dinfo[subUnit].audioPaused = false;
        dinfo[subUnit].audioStart = sector;
        dinfo[subUnit].audioEnd = 0;
    }
    return dinfo[subUnit].lastResult;
}

bool CMscdex::GetQChannelData(uint8_t subUnit, uint8_t& attr, uint8_t& track, uint8_t &index, TMSF& rel, TMSF& abs) {
	if (subUnit>=numDrives) return false;
	dinfo[subUnit].lastResult = cdrom[subUnit]->GetAudioSub(attr,track,index,rel,abs);
	if (!dinfo[subUnit].lastResult) {
		attr = track = index = 0;
        rel.fr = rel.min = rel.sec = 0;
        abs.fr = abs.min = abs.sec = 0;
	}
	return dinfo[subUnit].lastResult;
}

bool CMscdex::GetAudioStatus(uint8_t subUnit, bool& playing, bool& pause, TMSF& start, TMSF& end) {
	if (subUnit>=numDrives) return false;
	dinfo[subUnit].lastResult = cdrom[subUnit]->GetAudioStatus(playing,pause);
	if (dinfo[subUnit].lastResult) {
		if (playing) {
			// Start
			uint32_t addr	= dinfo[subUnit].audioStart + 150;
			start.fr	= (uint8_t)(addr%75);	addr/=75;
			start.sec	= (uint8_t)(addr%60); 
			start.min	= (uint8_t)(addr/60);
			// End
			addr		= dinfo[subUnit].audioEnd + 150;
			end.fr		= (uint8_t)(addr%75);	addr/=75;
			end.sec		= (uint8_t)(addr%60); 
			end.min		= (uint8_t)(addr/60);
		} else {
            start.fr = start.min = start.sec = 0;
            end.fr = end.min = end.sec = 0;
		}
	} else {
		playing		= false;
		pause		= false;
        start.fr = start.min = start.sec = 0;
        end.fr = end.min = end.sec = 0;
	}
	
	return dinfo[subUnit].lastResult;
}

bool CMscdex::StopAudio(uint8_t subUnit) {
	if (subUnit>=numDrives) return false;
	if (dinfo[subUnit].audioPlay) {
		// Check if audio is still playing....
		TMSF start,end;
		bool playing,pause;
		if (GetAudioStatus(subUnit,playing,pause,start,end))
			dinfo[subUnit].audioPlay = playing;
		else
			dinfo[subUnit].audioPlay = false;
	}
	if (dinfo[subUnit].audioPlay)
		dinfo[subUnit].lastResult = cdrom[subUnit]->PauseAudio(false);
	else
		dinfo[subUnit].lastResult = cdrom[subUnit]->StopAudio();
	
	if (dinfo[subUnit].lastResult) {
		if (dinfo[subUnit].audioPlay) {
			TMSF pos;
			GetCurrentPos(subUnit,pos);
			dinfo[subUnit].audioStart	= pos.min*60u*75u+pos.sec*75u+pos.fr - 150u;
			dinfo[subUnit].audioPaused  = true;
		} else {	
			dinfo[subUnit].audioPaused  = false;
			dinfo[subUnit].audioStart	= 0;
			dinfo[subUnit].audioEnd		= 0;
		}
		dinfo[subUnit].audioPlay = false;
	}
	return dinfo[subUnit].lastResult;
}

bool CMscdex::ResumeAudio(uint8_t subUnit) {
	if (subUnit>=numDrives) return false;
	return dinfo[subUnit].lastResult = PlayAudioSector(subUnit,dinfo[subUnit].audioStart,dinfo[subUnit].audioEnd);
}

uint32_t CMscdex::GetVolumeSize(uint8_t subUnit) {
	if (subUnit>=numDrives) return false;
	uint8_t tr1,tr2;
	TMSF leadOut;
	dinfo[subUnit].lastResult = GetCDInfo(subUnit,tr1,tr2,leadOut);
	if (dinfo[subUnit].lastResult) return (leadOut.min*60u*75u)+(leadOut.sec*75u)+leadOut.fr;
	return 0;
}

bool CMscdex::ReadVTOC(uint16_t drive, uint16_t volume, PhysPt data, uint16_t& offset, uint16_t& error) {
	uint8_t subunit = GetSubUnit(drive);
/*	if (subunit>=numDrives) {
		error=DOSERR_INVALID_DRIVE;
		return false;
	} */
	if (!ReadSectors(subunit,false,16u+volume,1u,data)) {
		error=DOSERR_DRIVE_NOT_READY;
		return false;
	}
	char id[5];
	MEM_BlockRead(data + 1, id, 5);
	if (strncmp("CD001", id, 5)==0) offset = 0;
	else {
		MEM_BlockRead(data + 9, id, 5);
		if (strncmp("CDROM", id, 5)==0) offset = 8;
		else {
			error = DOSERR_FORMAT_INVALID;
			return false;
		}
	}
	uint8_t type = mem_readb(data + offset);
	error = (type == 1) ? 1 : (type == 0xFF) ? 0xFF : 0;
	return true;
}

bool CMscdex::GetVolumeName(uint8_t subUnit, char* data) {	
	if (subUnit>=numDrives) return false;
	uint16_t drive = dinfo[subUnit].drive;

	uint16_t offset = 0, error;
	bool success = false;
	PhysPt ptoc = GetTempBuffer();
	success = ReadVTOC(drive,0x00,ptoc,offset,error);
	if (success) {
		MEM_StrCopy(ptoc+offset+40,data,31);
		data[31] = 0;
		rtrim(data);
	}

	return success; 
}

bool CMscdex::GetFileName(uint16_t drive, uint16_t pos, PhysPt data) {
	uint16_t offset = 0, error;
	bool success = false;
	PhysPt ptoc = GetTempBuffer();
	success = ReadVTOC(drive,0x00,ptoc,offset,error);
	if (success) {
		uint8_t len;
		for (len=0;len<37;len++) {
			uint8_t c=mem_readb(ptoc+offset+pos+len);
			if (c==0 || c==0x20) break;
		}
		MEM_BlockCopy(data,ptoc+offset+pos,len);
		mem_writeb(data+len,0);
	}
	return success; 
}

bool CMscdex::GetUPC(uint8_t subUnit, uint8_t& attr, char* upc)
{
	if (subUnit>=numDrives) return false;
	return dinfo[subUnit].lastResult = cdrom[subUnit]->GetUPC(attr,&upc[0]);
}

bool CMscdex::ReadSectors(uint8_t subUnit, bool raw, uint32_t sector, uint16_t num, PhysPt data) {
	if (subUnit>=numDrives) return false;
	if ((cpu_cycles_count_t)(4u*num*2048u+5u) < CPU_Cycles) CPU_Cycles -= cpu_cycles_count_t(4u*num*2048u);
	else CPU_Cycles = 5u;
	dinfo[subUnit].lastResult = cdrom[subUnit]->ReadSectors(data,raw,sector,num);
	return dinfo[subUnit].lastResult;
}

bool CMscdex::ReadSectorsMSF(uint8_t subUnit, bool raw, uint32_t start, uint16_t num, PhysPt data) {
	if (subUnit>=numDrives) return false;
	uint8_t min		= (uint8_t)(start>>16) & 0xFF;
	uint8_t sec		= (uint8_t)(start>> 8) & 0xFF;
	uint8_t fr		= (uint8_t)(start>> 0) & 0xFF;
	uint32_t sector	= min*60u*75u+sec*75u+fr - 150u;
	return ReadSectors(subUnit,raw,sector,num,data);
}

// Called from INT 2F
bool CMscdex::ReadSectors(uint16_t drive, uint32_t sector, uint16_t num, PhysPt data) {
	return ReadSectors(GetSubUnit(drive),false,sector,num,data);
}

bool CMscdex::GetDirectoryEntry(uint16_t drive, bool copyFlag, PhysPt pathname, PhysPt buffer, uint16_t& error) {
	char	volumeID[6] = {0};
	char	searchName[256];
	char	entryName[256];
	bool	foundComplete = false;
	bool	nextPart = true;
    const char* useName = NULL;
    uint8_t   entryLength, nameLength;
	// clear error
	error = 0;
	MEM_StrCopy(pathname+1,searchName,mem_readb(pathname));
	upcase(searchName);
	char* searchPos = searchName;

	//strip of tailing . (XCOM APOCALYPSE)
	size_t searchlen = strlen(searchName);
	if (searchlen > 1 && strcmp(searchName,".."))
		if (searchName[searchlen-1] =='.')  searchName[searchlen-1] = 0;

	//LOG(LOG_MISC,LOG_ERROR)("MSCDEX: Get DirEntry : Find : %s",searchName);
	// read vtoc
	PhysPt defBuffer = GetDefaultBuffer();
	if (!ReadSectors(GetSubUnit(drive),false,16,1,defBuffer)) return false;
	MEM_StrCopy(defBuffer+1,volumeID,5); volumeID[5] = 0;
	bool iso = (strcmp("CD001",volumeID)==0);
	if (!iso) {
		MEM_StrCopy(defBuffer+9,volumeID,5);
		if (strcmp("CDROM",volumeID)!=0) E_Exit("MSCDEX: GetDirEntry: Not an ISO 9660 or HSF CD.");
	}
	uint16_t offset = iso ? 156:180;
	// get directory position
	uint32_t dirEntrySector	= mem_readd(defBuffer+offset+2);
	Bits dirSize		= (int32_t)mem_readd(defBuffer+offset+10);
	while (dirSize>0) {
		uint16_t index = 0;
		if (!ReadSectors(GetSubUnit(drive),false,dirEntrySector,1,defBuffer)) return false;
		// Get string part
		bool foundName = false;
		if (nextPart) {
			if (searchPos) { 
				useName = searchPos; 
				searchPos = strchr_dbcs(searchPos,'\\');
			}
			if (searchPos) { *searchPos = 0; searchPos++; }
			else foundComplete = true;
		}

		do {
			entryLength = mem_readb(defBuffer+index);
			if (entryLength==0) break;
			if (mem_readb(defBuffer + index + (iso?0x19:0x18) ) & 4) {
				// skip associated files
				index += entryLength;
				continue;
			}
			nameLength  = mem_readb(defBuffer+index+32);
			MEM_StrCopy(defBuffer+index+33,entryName,nameLength);
			// strip separator and file version number
			char* separator = strchr(entryName,';');
			if (separator) *separator = 0;
			// strip trailing period
			size_t entrylen = strlen(entryName);
			if (entrylen>0 && entryName[entrylen-1]=='.') entryName[entrylen-1] = 0;

			if (strcmp(entryName,useName)==0) {
				//LOG(LOG_MISC,LOG_ERROR)("MSCDEX: Get DirEntry : Found : %s",useName);
				foundName = true;
				break;
			}
			index += entryLength;
		} while (index+33<=2048);
		
		if (foundName) {
			if (foundComplete) {
				if (copyFlag) {
					LOG(LOG_MISC,LOG_WARN)("MSCDEX: GetDirEntry: Copyflag structure not entirely accurate maybe");
					uint8_t readBuf[256];
					uint8_t writeBuf[256];
					MEM_BlockRead( defBuffer+index, readBuf, entryLength );
					writeBuf[0] = readBuf[1];						// 00h	BYTE	length of XAR in Logical Block Numbers
					memcpy( &writeBuf[1], &readBuf[0x2], 4);		// 01h	DWORD	Logical Block Number of file start
					writeBuf[5] = 0;writeBuf[6] = 8;				// 05h	WORD	size of disk in logical blocks
					memcpy( &writeBuf[7], &readBuf[0xa], 4);		// 07h	DWORD	file length in bytes
					memcpy( &writeBuf[0xb], &readBuf[0x12], 6);		// 0bh	BYTEs	date and time
					writeBuf[0x11] = iso ? readBuf[0x18]:0;			// 11h	BYTE	time zone
					writeBuf[0x12] = readBuf[iso ? 0x19:0x18];		// 12h	BYTE	bit flags
					writeBuf[0x13] = readBuf[0x1a];					// 13h	BYTE	interleave size
					writeBuf[0x14] = readBuf[0x1b];					// 14h	BYTE	interleave skip factor
					memcpy( &writeBuf[0x15], &readBuf[0x1c], 2);	// 15h	WORD	volume set sequence number
					writeBuf[0x17] = readBuf[0x20];
					memcpy( &writeBuf[0x18], &readBuf[21], readBuf[0x20] <= 38 ? readBuf[0x20] : 38 );
					MEM_BlockWrite( buffer, writeBuf, 0x18 + 40 );
				} else {
					// Direct copy
					MEM_BlockCopy(buffer,defBuffer+index,entryLength);
				}
				error = iso ? 1:0;
				return true;
			}
			// change directory
			dirEntrySector = mem_readd(defBuffer+index+2);
			dirSize	= (int32_t)mem_readd(defBuffer+index+10);
			nextPart = true;
		} else {
			// continue search in next sector
			dirSize -= 2048;
			dirEntrySector++;
			nextPart = false;
		}
	}
    error = DOSERR_FILE_NOT_FOUND;
	return false; // not found
}

bool CMscdex::GetCurrentPos(uint8_t subUnit, TMSF& pos) {
	if (subUnit>=numDrives) return false;
    if(!dinfo[subUnit].audioPlay) {
        FRAMES_TO_MSF((dinfo[subUnit].audioStart + REDBOOK_FRAME_PADDING), &pos.min, &pos.sec, &pos.fr);
        return true;
    }
    TMSF rel;
	uint8_t attr,track,index;
	dinfo[subUnit].lastResult = GetQChannelData(subUnit, attr, track, index, rel, pos);
    if(!dinfo[subUnit].lastResult) pos.fr = pos.min = pos.sec = 0;
	return dinfo[subUnit].lastResult;
}

bool CMscdex::GetMediaStatus(uint8_t subUnit, bool& media, bool& changed, bool& trayOpen) {
	if (subUnit>=numDrives) return false;
	dinfo[subUnit].lastResult = cdrom[subUnit]->GetMediaTrayStatus(media,changed,trayOpen);
	return dinfo[subUnit].lastResult;
}

uint32_t CMscdex::GetDeviceStatus(uint8_t subUnit) {
	if (subUnit>=numDrives) return false;
	bool media,changed,trayOpen;

	dinfo[subUnit].lastResult = GetMediaStatus(subUnit,media,changed,trayOpen);
	if (dinfo[subUnit].audioPlay) {
		// Check if audio is still playing....
		TMSF start,end;
		bool playing,pause;
		if (GetAudioStatus(subUnit,playing,pause,start,end))
			dinfo[subUnit].audioPlay = playing;
		else
			dinfo[subUnit].audioPlay = false;
	}

// Reference: https://oldlinux.superglobalmegacorp.com/Linux.old/docs/interrupts/inter61/INTERRUP.G
// Reference (Microsoft MS-DOS CD-ROM Extensions 2.1): https://tinyurl.com/3spx4dfn
// Bits marked with * have the given function listed in INTERRUP.G, but are listed as "Reserved (all 0)" in Microsoft MS-DOS CD-ROM Extensions 2.1
    uint32_t status =
        ((trayOpen ? 1u : 0u) << 0u) |                  // Bit 0:       0 = Door closed                         1 = Door opened
        ((dinfo[subUnit].locked ? 0u : 1u) << 1u) |     // Bit 1:       0 = Door locked                         1 = Door unlocked
        (1u << 2u) |                                    // Bit 2:       0 = Supports only cooked reading        1 = Supports cooked and raw reading
                                                        // Bit 3:       0 = Read only                           1 = Read/write
        (1u << 4u) |                                    // Bit 4:       0 = Data read only                      1 = Data read and plays audio/video tracks
                                                        // Bit 5:       0 = No interleaving                     1 = Supports interleaving
                                                        // Bit 6:       Reserved
                                                        // Bit 7:       0 = No prefetching                      1 = Supports prefetching requests
        (1u << 8u) |                                    // Bit 8:       0 = No audio channel manipulation       1 = Supports audio channel manipulation
        (1u << 9u) |                                    // Bit 9:       0 = Supports HSG addressing mode        1 = Supports HSG and Red Book addressing modes
        ((dinfo[subUnit].audioPlay ? 1u : 0u) << 10u) | // Bit 10*:     0 = Audio is not playing                1 = Audio is playing
        ((media ? 0u : 1u) << 11u);                     // Bit 11*:     0 = Disk in drive                       1 = No disk in drive
                                                        // Bit 12*:     0 = Does not support R-W subchannels    1 = Supports R-W subchannels
                                                        // Bit 13-31:   Reserved (all 0)
    return status;
}

bool CMscdex::GetMediaStatus(uint8_t subUnit, uint8_t& status) {
	if (subUnit>=numDrives) return false;
/*	bool media,changed,open,result;
	result = GetMediaStatus(subUnit,media,changed,open);
	status = changed ? 0xFF : 0x01;
	return result; */
	status = getSwapRequest() ? 0xFF : 0x01;
	return true;
}

bool CMscdex::LoadUnloadMedia(uint8_t subUnit, bool unload) {
	if (subUnit>=numDrives) return false;
	dinfo[subUnit].lastResult = cdrom[subUnit]->LoadUnloadMedia(unload);
	return dinfo[subUnit].lastResult;
}

bool CMscdex::SendDriverRequest(uint16_t drive, PhysPt data) {
	uint8_t subUnit = GetSubUnit(drive);
	if (subUnit>=numDrives) return false;
	// Get SubUnit
	mem_writeb(data+1,subUnit);
	// Call Strategy / Interrupt
	MSCDEX_Strategy_Handler();
	MSCDEX_Interrupt_Handler();
	return true;
}

uint16_t CMscdex::GetStatusWord(uint8_t subUnit,uint16_t status) {
	if (subUnit>=numDrives) return REQUEST_STATUS_ERROR | 0x02; // error : Drive not ready

	if (dinfo[subUnit].lastResult)	status |= REQUEST_STATUS_DONE;				// ok
	else							status |= REQUEST_STATUS_ERROR; 

	if (dinfo[subUnit].audioPlay) {
		// Check if audio is still playing....
		TMSF start,end;
		bool playing,pause;
		if (GetAudioStatus(subUnit,playing,pause,start,end)) {
			dinfo[subUnit].audioPlay = playing;
		} else
			dinfo[subUnit].audioPlay = false;

		status |= (dinfo[subUnit].audioPlay<<9);
	} 
	dinfo[subUnit].lastResult	= true;
	return status;
}

void CMscdex::InitNewMedia(uint8_t subUnit) {
	if (subUnit<numDrives) {
		// Reopen new media
		cdrom[subUnit]->InitNewMedia();
	}
}

bool CMscdex::ChannelControl(uint8_t subUnit, TCtrl ctrl) {
	if (subUnit>=numDrives) return false;
	// adjust strange channel mapping
	if (ctrl.out[0]>1) ctrl.out[0]=0;
	if (ctrl.out[1]>1) ctrl.out[1]=1;
	dinfo[subUnit].audioCtrl=ctrl;
	cdrom[subUnit]->ChannelControl(ctrl);
	return true;
}

bool CMscdex::GetChannelControl(uint8_t subUnit, TCtrl& ctrl) {
	if (subUnit>=numDrives) return false;
	ctrl=dinfo[subUnit].audioCtrl;
	return true;
}

static CMscdex* mscdex = nullptr;
static PhysPt curReqheaderPtr = 0;

bool GetMSCDEXDrive(unsigned char drive_letter,CDROM_Interface **_cdrom) {
	Bitu i;

	if (mscdex == NULL) {
		if (_cdrom) *_cdrom = NULL;
		return false;
	}

	for (i=0;i < MSCDEX_MAX_DRIVES;i++) {
		if (mscdex->cdrom[i] == NULL) continue;
		if (mscdex->dinfo[i].drive == drive_letter) {
			if (_cdrom) *_cdrom = mscdex->cdrom[i];
			return true;
		}
	}

	return false;
}

// Reference: https://oldlinux.superglobalmegacorp.com/Linux.old/docs/interrupts/inter61/INTERRUP.G
static uint16_t MSCDEX_IOCTL_Input(PhysPt buffer, uint8_t drive_unit) {
    uint8_t ioctl_fct = mem_readb(buffer);
    MSCDEX_LOG("MSCDEX: IOCTL INPUT Subfunction %02X", (int)ioctl_fct);
    switch(ioctl_fct) {
    case 0x00: /* Device driver header address */
        mem_writed(buffer + 1, RealMake(mscdex->rootDriverHeaderSeg, 0));
        break;
    case 0x01: /* Drive head location */
    {
        TMSF pos;
        mscdex->GetCurrentPos(drive_unit, pos);
        uint8_t addr_mode = mem_readb(buffer + 1);
        if(addr_mode == 0) { // HSG
            uint32_t frames = MSF_TO_FRAMES(pos.min, pos.sec, pos.fr);
            if(frames < REDBOOK_FRAME_PADDING) MSCDEX_LOG_ERROR("MSCDEX: Get position: invalid position %d:%d:%d", pos.min, pos.sec, pos.fr);
            else frames -= REDBOOK_FRAME_PADDING;
            mem_writed(buffer + 2, frames);
        }
        else if(addr_mode == 1) { // Red Book
            mem_writeb(buffer + 2, pos.fr);
            mem_writeb(buffer + 3, pos.sec);
            mem_writeb(buffer + 4, pos.min);
            mem_writeb(buffer + 5, 0x00);
        }
        else {
            MSCDEX_LOG_ERROR("MSCDEX: Get position: invalid address mode %x", addr_mode);
            return 0x03; // Invalid function
        }
        break;
    }
    case 0x02: /* Reserved */
        LOG(LOG_MISC, LOG_ERROR)("MSCDEX: Unsupported IOCTL INPUT Subfunction %02X", (int)ioctl_fct);
        return 0x03; // Invalid function
    case 0x03: /* Error statistics */
        // Undefined as of 5 Aug 88 specification
        LOG(LOG_MISC, LOG_ERROR)("MSCDEX: Unsupported IOCTL INPUT Subfunction %02X", (int)ioctl_fct);
        return 0x03; // Invalid function
    case 0x04: /* Audio channel info */
        TCtrl ctrl;
        if(!mscdex->GetChannelControl(drive_unit, ctrl)) return 0x01;
        for(uint8_t chan = 0; chan < 4; chan++) {
            mem_writeb(buffer + chan * 2u + 1u, ctrl.out[chan]);
            mem_writeb(buffer + chan * 2u + 2u, ctrl.vol[chan]);
        }
        break;
    case 0x05: /* Raw drive bytes (uninterpreted and device-specific) */
        LOG(LOG_MISC, LOG_ERROR)("MSCDEX: Unsupported IOCTL INPUT Subfunction %02X", (int)ioctl_fct);
        return 0x03; // Invalid function
    case 0x06: /* Device status */
        mem_writed(buffer + 1, mscdex->GetDeviceStatus(drive_unit));
        break;
    case 0x07: /* Sector size */
    {
        uint8_t read_mode = mem_readb(buffer + 1);
        if(read_mode == 0) // Cooked
            mem_writew(buffer + 2, 2048);
        else if(read_mode == 1) // Raw
            mem_writew(buffer + 2, 2352);
        else
        {
            MSCDEX_LOG_ERROR("MSCDEX: Sector size: invalid read mode %x", read_mode);
            return 0x03; // Invalid function
        }
        break;
    }
    case 0x08: /* Volume size */
        mem_writed(buffer + 1, mscdex->GetVolumeSize(drive_unit));
        break;
    case 0x09: /* Media change status */
        uint8_t status;
        if(!mscdex->GetMediaStatus(drive_unit, status)) {
            status = 0; // Status unknown
        }
        mem_writeb(buffer + 1, status);
        break;
    case 0x0A: /* Audio disk info */
    {
        uint8_t tr1, tr2; TMSF leadOut;
        if(!mscdex->GetCDInfo(drive_unit, tr1, tr2, leadOut)) return 0x05;
        mem_writeb(buffer + 1, tr1);
        mem_writeb(buffer + 2, tr2);
        mem_writeb(buffer + 3, leadOut.fr);
        mem_writeb(buffer + 4, leadOut.sec);
        mem_writeb(buffer + 5, leadOut.min);
        mem_writeb(buffer + 6, 0x00);
        break;
    }
    case 0x0B: /* Audio track info */
    {
        uint8_t attr = 0; TMSF start;
        uint8_t track = mem_readb(buffer + 1);
        mscdex->GetTrackInfo(drive_unit, track, attr, start);
        mem_writeb(buffer + 2, start.fr);
        mem_writeb(buffer + 3, start.sec);
        mem_writeb(buffer + 4, start.min);
        mem_writeb(buffer + 5, 0x00);
        mem_writeb(buffer + 6, attr);
        break;
    }
    case 0x0C: /* Audio Q-Channel info */
    {
        uint8_t attr = 0, track, index;
        TMSF abs, rel;
        mscdex->GetQChannelData(drive_unit, attr, track, index, rel, abs);
        mem_writeb(buffer + 1, attr);
        mem_writeb(buffer + 2, ((track / 10) << 4) | (track % 10)); // track in BCD
        mem_writeb(buffer + 3, index);
        mem_writeb(buffer + 4, rel.min);
        mem_writeb(buffer + 5, rel.sec);
        mem_writeb(buffer + 6, rel.fr);
        mem_writeb(buffer + 7, 0x00);
        mem_writeb(buffer + 8, abs.min);
        mem_writeb(buffer + 9, abs.sec);
        mem_writeb(buffer + 10, abs.fr);
        break;
    }
    case 0x0D: /* Audio sub-channel info */
        LOG(LOG_MISC, LOG_ERROR)("MSCDEX: Unsupported IOCTL INPUT Subfunction %02X", (int)ioctl_fct);
        return 0x03; // Invalid function
    case 0x0E: /* UPC code */
    {
        uint8_t attr = 0; char upc[8] = {};
        mscdex->GetUPC(drive_unit, attr, &upc[0]);
        mem_writeb(buffer + 1u, attr);
        for(unsigned int i = 0; i < 7; i++) mem_writeb(buffer + 2u + i, (unsigned char)upc[i]);
        mem_writeb(buffer + 9, 0x00u);
        break;
    }
    case 0x0F: /* Audio status info */
    {
        bool playing = false;
        bool paused = false;
        TMSF resStart, resEnd;
        mscdex->GetAudioStatus(drive_unit, playing, paused, resStart, resEnd);
        mem_writew(buffer + 1u, paused);
        mem_writeb(buffer + 3u, resStart.min);
        mem_writeb(buffer + 4u, resStart.sec);
        mem_writeb(buffer + 5u, resStart.fr);
        mem_writeb(buffer + 6u, 0x00u);
        mem_writeb(buffer + 7u, resEnd.min);
        mem_writeb(buffer + 8u, resEnd.sec);
        mem_writeb(buffer + 9u, resEnd.fr);
        mem_writeb(buffer + 10u, 0x00u);
        break;
    }
    default:
        LOG(LOG_MISC, LOG_ERROR)("MSCDEX: Unsupported IOCTL INPUT Subfunction %02X", (int)ioctl_fct);
        return 0x03; // Invalid function
    }
    return 0x00; // Success
}

// Reference: https://oldlinux.superglobalmegacorp.com/Linux.old/docs/interrupts/inter61/INTERRUP.G
static uint16_t MSCDEX_IOCTL_Output(PhysPt buffer, uint8_t drive_unit) {
    uint8_t ioctl_fct = mem_readb(buffer);
    // MSCDEX_LOG("MSCDEX: IOCTL OUTPUT Subfunction %02X",ioctl_fct);
    switch(ioctl_fct) {
    case 0x00: // Eject disk
        if(!mscdex->LoadUnloadMedia(drive_unit, true)) return 0x02;
        break;
    case 0x01: // Lock/unlock door
        // do nothing -> report as success
        break;
    case 0x02: // Reset drive
        LOG(LOG_MISC, LOG_WARN)("cdromDrive reset");
        if(!mscdex->StopAudio(drive_unit))  return 0x02;
        break;
    case 0x03: // Control audio channel
        TCtrl ctrl;
        for(uint8_t chan = 0; chan < 4; chan++) {
            ctrl.out[chan] = mem_readb(buffer + chan * 2u + 1u);
            ctrl.vol[chan] = mem_readb(buffer + chan * 2u + 2u);
        }
        if(!mscdex->ChannelControl(drive_unit, ctrl)) return 0x01;
        break;
    case 0x04: // Write device control string
        LOG(LOG_MISC, LOG_ERROR)("MSCDEX: Unsupported IOCTL OUTPUT Subfunction %02X", (int)ioctl_fct);
        return 0x03; // Invalid function
    case 0x05: // Close tray
        if(!mscdex->LoadUnloadMedia(drive_unit, false)) return 0x02;
        break;
    default:
        LOG(LOG_MISC, LOG_ERROR)("MSCDEX: Unsupported IOCTL OUTPUT Subfunction %02X", (int)ioctl_fct);
        return 0x03; // Invalid function
    }
    return 0x00; // Success
}

static Bitu MSCDEX_Strategy_Handler(void) {
	curReqheaderPtr = PhysMake(SegValue(es),reg_bx);
//	MSCDEX_LOG("MSCDEX: Device Strategy Routine called, request header at %x",curReqheaderPtr);
	return CBRET_NONE;
}

static Bitu MSCDEX_Interrupt_Handler(void) {
	if (curReqheaderPtr==0) {
		MSCDEX_LOG_ERROR("MSCDEX: invalid call to interrupt handler");
		return CBRET_NONE;
	}
	uint8_t	subUnit		= mem_readb(curReqheaderPtr+1);
	uint8_t	funcNr		= mem_readb(curReqheaderPtr+2);
	uint16_t	errcode		= 0;
	PhysPt	buffer		= 0;

	MSCDEX_LOG("MSCDEX: Driver Function %02X",funcNr);

	if ((funcNr==0x03) || (funcNr==0x0c) || (funcNr==0x80) || (funcNr==0x82)) {
		buffer = PhysMake(mem_readw(curReqheaderPtr+0x10),mem_readw(curReqheaderPtr+0x0E));
	}

// Reference (Microsoft MS-DOS CD-ROM Extensions 2.1): https://tinyurl.com/3spx4dfn
    switch(funcNr) {
    case 0x00:      /* INIT */
        MSCDEX_LOG_ERROR("Unsupported Driver Request %02X", funcNr);
        break;
    case 0x01:      /* MEDIA CHECK (block devices) */
        MSCDEX_LOG_ERROR("Unsupported Driver Request %02X", funcNr);
        break;
    case 0x02:      /* BUILD BPB (block devices) */
        MSCDEX_LOG_ERROR("Unsupported Driver Request %02X", funcNr);
        break;
    case 0x03:      /* IOCTL INPUT */
    {
        uint16_t error = MSCDEX_IOCTL_Input(buffer, subUnit);
        if(error) errcode = error;
        break;
    }
    case 0x04:      /* INPUT (read) */
        MSCDEX_LOG_ERROR("Unsupported Driver Request %02X", funcNr);
        break;
    case 0x05:      /* NONDESTRUCTIVE INPUT NO WAIT */
        MSCDEX_LOG_ERROR("Unsupported Driver Request %02X", funcNr);
        break;
    case 0x06:      /* INPUT STATUS */
        MSCDEX_LOG_ERROR("Unsupported Driver Request %02X", funcNr);
        break;
    case 0x07:      /* INPUT FLUSH */
        MSCDEX_LOG_ERROR("Unsupported Driver Request %02X", funcNr);
        break;
    case 0x08:      /* OUTPUT (write) */
        MSCDEX_LOG_ERROR("Unsupported Driver Request %02X", funcNr);
        break;
    case 0x09:      /* OUTPUT WITH VERIFY */
        MSCDEX_LOG_ERROR("Unsupported Driver Request %02X", funcNr);
        break;
    case 0x0A:      /* OUTPUT STATUS */
        MSCDEX_LOG_ERROR("Unsupported Driver Request %02X", funcNr);
        break;
    case 0x0B:      /* OUTPUT FLUSH */
        MSCDEX_LOG_ERROR("Unsupported Driver Request %02X", funcNr);
        break;
    case 0x0C:      /* IOCTL OUTPUT */
    {
        uint16_t error = MSCDEX_IOCTL_Output(buffer, subUnit);
        if(error) errcode = error;
        break;
    }
    case 0x0D:      /* DEVICE OPEN */
    case 0x0E:      /* DEVICE CLOSE */
        break;
    case 0x0F:      /* REMOVABLE MEDIA (block devices) */
        MSCDEX_LOG_ERROR("Unsupported Driver Request %02X", funcNr);
        break;
    case 0x10:      /* OUTPUT UNTIL BUSY */
        MSCDEX_LOG_ERROR("Unsupported Driver Request %02X", funcNr);
        break;
    case 0x80:      /* READ LONG */
    {
        uint32_t start = mem_readd(curReqheaderPtr + 0x14);
        uint16_t len = mem_readw(curReqheaderPtr + 0x12);
        bool raw = (mem_readb(curReqheaderPtr + 0x18) == 1);
        if(mem_readb(curReqheaderPtr + 0x0D) == 0x00) // HSG
            mscdex->ReadSectors(subUnit, raw, start, len, buffer);
        else
            mscdex->ReadSectorsMSF(subUnit, raw, start, len, buffer);
        break;
    }
    case 0x81:      /* Reserved */
        MSCDEX_LOG_ERROR("Unsupported Driver Request %02X", funcNr);
        break;
    case 0x82:      /* READ LONG PREFETCH */
    {
        uint32_t start = mem_readd(curReqheaderPtr + 0x14);
        uint16_t len = mem_readw(curReqheaderPtr + 0x12);
        bool raw = (mem_readb(curReqheaderPtr + 0x18) == 1);
        if(mem_readb(curReqheaderPtr + 0x0D) == 0x00) // HSG
            mscdex->ReadSectors(subUnit, raw, start, len, buffer);
        else
            mscdex->ReadSectorsMSF(subUnit, raw, start, len, buffer);
        break;
    }
    case 0x83:      /* SEEK */
    {
        uint32_t start = mem_readd(curReqheaderPtr + 0x14);
        if(mem_readb(curReqheaderPtr + 0x0D)) { //Redbook if non-zero
            TMSF msf = {};
            msf.min = (start >> 16) & 0xFF;
            msf.sec = (start >> 8) & 0xFF;
            msf.fr = (start >> 0) & 0xFF;
            start = MSF_TO_FRAMES(msf.min, msf.sec, msf.fr);
            if(start < REDBOOK_FRAME_PADDING) {
                MSCDEX_LOG("MSCDEX: Seek: invalid position %d:%d:%d", msf.min, msf.sec, msf.fr);
                start = 0;
            }
            else {
                start -= REDBOOK_FRAME_PADDING;
            }
        }
        mscdex->Seek(subUnit, start);
        break;
    }

        break;
    case 0x84:      /* PLAY AUDIO */
    {
        uint32_t start = mem_readd(curReqheaderPtr + 0x0E);
        uint32_t len = mem_readd(curReqheaderPtr + 0x12);
        if(mem_readb(curReqheaderPtr + 0x0D) == 0x00) // HSG
            mscdex->PlayAudioSector(subUnit, start, len);
        else // RED BOOK
            mscdex->PlayAudioMSF(subUnit, start, len);
        break;
    }
    case 0x85:      /* STOP AUDIO */
        mscdex->StopAudio(subUnit);
        break;
    case 0x86:      /* WRITE LONG */
        MSCDEX_LOG_ERROR("Unsupported Driver Request %02X", funcNr);
        break;
    case 0x87:      /* WRITE LONG VERIFY */
        MSCDEX_LOG_ERROR("Unsupported Driver Request %02X", funcNr);
        break;
    case 0x88:      /* RESUME AUDIO */
        mscdex->ResumeAudio(subUnit);
        break;
    default:
        MSCDEX_LOG_ERROR("Unsupported Driver Request %02X", funcNr);
        break;
    }
	
	// Set Statusword
	mem_writew(curReqheaderPtr+3,mscdex->GetStatusWord(subUnit,errcode));
	MSCDEX_LOG("MSCDEX: Status : %04X",mem_readw(curReqheaderPtr+3));						
	return CBRET_NONE;
}

static bool MSCDEX_Handler(void) {
	if(reg_ah == 0x11) {
		if(reg_al == 0x00) {
			if(real_readw(SegValue(ss),reg_sp+6) == 0xDADA) {
				if (mscdex->rootDriverHeaderSeg==0) return false;
				//MSCDEX sets word on stack to ADAD if it DADA on entry.
				real_writew(SegValue(ss),reg_sp+6,0xADAD);
				reg_al = 0xff;
			} else {
				if (!enable_network_redirector) return false;
				reg_al = 0xff;
			}
			return true;
		} else
			return false;
	}

	if (reg_ah!=0x15) return false;		// not handled here, continue chain
	if (mscdex->rootDriverHeaderSeg==0) return false;	// not handled if MSCDEX not installed

	PhysPt data = PhysMake(SegValue(es),reg_bx);
	MSCDEX_LOG("MSCDEX: INT 2F AX=%04X BX=%04X CX=%04X",reg_ax,reg_bx,reg_cx);
	CALLBACK_SCF(false); // carry flag cleared for all functions (undocumented); only set on error
	switch (reg_ax) {
		case 0x1500:	/* Install check */
						reg_bx = mscdex->GetNumDrives();
						if (reg_bx>0) reg_cx = mscdex->GetFirstDrive();
						reg_al = 0xff;
						break;
		case 0x1501:	/* Get cdrom driver info */
						mscdex->GetDriverInfo(data);
						break;
		case 0x1502:	/* Get Copyright filename */
		case 0x1503:	/* Get Abstract filename */
		case 0x1504:	/* Get Documentation filename */
						if (!mscdex->GetFileName(reg_cx,702+(reg_al-2)*37,data)) {
							reg_ax = DOSERR_INVALID_DRIVE;
							CALLBACK_SCF(true);						
						}
						break;		
		case 0x1505: {	// read vtoc 
						uint16_t offset = 0, error = 0;
						bool success = mscdex->ReadVTOC(reg_cx,reg_dx,data,offset,error);
						reg_ax = error;
						if (!success) CALLBACK_SCF(true);
					}
						break;
		case 0x1506:	/* Debugging on */
		case 0x1507:	/* Debugging off */
						// not functional in production MSCDEX
						break;
		case 0x1508: {	// read sectors 
						uint32_t sector = ((uint32_t)reg_si << 16u) + (uint32_t)reg_di;
						if (mscdex->ReadSectors(reg_cx,sector,reg_dx,data)) {
							reg_ax = 0;
						} else {
							// possibly: DOSERR_DRIVE_NOT_READY if sector is beyond total length
							reg_ax = DOSERR_INVALID_DRIVE;
							CALLBACK_SCF(true);
						}
					}
						break;
		case 0x1509:	// write sectors - not supported 
						reg_ax = DOSERR_FUNCTION_NUMBER_INVALID;
						CALLBACK_SCF(true);
						break;
		case 0x150A:	/* Reserved */
						break;
		case 0x150B:	/* Valid CDROM drive ? */
						reg_ax = (mscdex->IsValidDrive(reg_cx) ? 0x5ad8 : 0x0000);
						reg_bx = 0xADAD;
						break;
		case 0x150C:	/* Get MSCDEX Version */
						reg_bx = mscdex->GetVersion();
						break;
		case 0x150D:	/* Get drives */
						mscdex->GetDrives(data);
						break;
		case 0x150E:	/* Get/Set Volume Descriptor Preference */
						if (mscdex->IsValidDrive(reg_cx)) {
							if (reg_bx == 0) {
								// get preference
								reg_dx = 0x100;	// preference?
							} else if (reg_bx == 1) {
								// set preference
								if (reg_dh != 1) {
									reg_ax = DOSERR_FUNCTION_NUMBER_INVALID;
									CALLBACK_SCF(true);
								}
							} else {
								reg_ax = DOSERR_FUNCTION_NUMBER_INVALID;
								CALLBACK_SCF(true);
							}
						} else {
							reg_ax = DOSERR_INVALID_DRIVE;
							CALLBACK_SCF(true);
						}
						break;
		case 0x150F: {	// Get directory entry
						uint16_t error;
						bool success = mscdex->GetDirectoryEntry(reg_cl,reg_ch&1,data,PhysMake(reg_si,reg_di),error);
						reg_ax = error;
						if (!success) CALLBACK_SCF(true);
					 }
						break;
		case 0x1510:	/* Device driver request */
						if (!mscdex->SendDriverRequest(reg_cx,data)) {
							reg_ax = DOSERR_INVALID_DRIVE;
							CALLBACK_SCF(true);
						}
						break;
		default:		LOG(LOG_MISC,LOG_ERROR)("MSCDEX: Unknown call : %04X",reg_ax);
						reg_ax = DOSERR_FUNCTION_NUMBER_INVALID;
						CALLBACK_SCF(true);
						break;
	}
	return true;
}

static bool MSCDEX_ValidDevName(const char *s) {
	if (*s == 0) return false;
	if (strlen(s) > 8) return false;

	while (*s != 0) {
		if ((*s >= 'A' && *s <= 'Z') || (*s >= '0' && *s <= '9')) {
			s++;
		}
		else {
			return false;
		}
	}

	return true;
}

class device_MSCDEX : public DOS_Device {
public:
	device_MSCDEX(const char *devname) { SetName(MSCDEX_ValidDevName(devname) ? devname : "MSCD001"); }
	bool Read (uint8_t * /*data*/,uint16_t * /*size*/) override { return false;}
	bool Write(const uint8_t * /*data*/,uint16_t * /*size*/) override {
		LOG(LOG_ALL,LOG_NORMAL)("Write to mscdex device");	
		return false;
	}
	bool Seek(uint32_t * /*pos*/,uint32_t /*type*/) override {return false;}
	bool Close() override {return false;}
	uint16_t GetInformation(void) override
	{
		return DeviceInfoFlags::Device | DeviceInfoFlags::IoctlSupport | DeviceInfoFlags::OpenCloseSupport;
	}
	bool ReadFromControlChannel(PhysPt bufptr,uint16_t size,uint16_t * retcode) override;
	bool WriteToControlChannel(PhysPt bufptr,uint16_t size,uint16_t * retcode) override;
// private:
//  uint8_t cache;
};

bool device_MSCDEX::ReadFromControlChannel(PhysPt bufptr,uint16_t size,uint16_t * retcode) { 
	if (MSCDEX_IOCTL_Input(bufptr,0)==0) {
		*retcode=size;
		return true;
	}
	return false;
}

bool device_MSCDEX::WriteToControlChannel(PhysPt bufptr,uint16_t size,uint16_t * retcode) { 
	if (MSCDEX_IOCTL_Output(bufptr,0)==0) {
		*retcode=size;
		return true;
	}
	return false;
}

int MSCDEX_AddDrive(char driveLetter, const char* physicalPath, uint8_t& subUnit)
{
	int result = mscdex->AddDrive(driveLetter-'A',(char*)physicalPath,subUnit);
	return result;
}

int MSCDEX_RemoveDrive(char driveLetter)
{
	if(!mscdex) return 0;
	return mscdex->RemoveDrive(driveLetter-'A');
}

bool MSCDEX_HasDrive(char driveLetter)
{
	return mscdex->HasDrive(driveLetter-'A');
}

void MSCDEX_ReplaceDrive(CDROM_Interface* cdrom, uint8_t subUnit)
{
	mscdex->ReplaceDrive(cdrom, subUnit);
}

uint8_t MSCDEX_GetSubUnit(char driveLetter)
{
	return mscdex->GetSubUnit(driveLetter-'A');
}

bool MSCDEX_GetVolumeName(uint8_t subUnit, char* name)
{
    if(dos_kernel_disabled) return false;
    return mscdex->GetVolumeName(subUnit,name);
}

bool MSCDEX_HasMediaChanged(uint8_t subUnit)
{
	static TMSF leadOut[MSCDEX_MAX_DRIVES];

	TMSF leadnew;
	uint8_t tr1,tr2;
	if (mscdex->GetCDInfo(subUnit,tr1,tr2,leadnew)) {
		bool changed = (leadOut[subUnit].min!=leadnew.min) || (leadOut[subUnit].sec!=leadnew.sec) || (leadOut[subUnit].fr!=leadnew.fr);
		if (changed) {
			leadOut[subUnit].min = leadnew.min;
			leadOut[subUnit].sec = leadnew.sec;
			leadOut[subUnit].fr	 = leadnew.fr;
			mscdex->InitNewMedia(subUnit);
		}
		return changed;
	}
	if (subUnit<MSCDEX_MAX_DRIVES) {
		leadOut[subUnit].min = 0;
		leadOut[subUnit].sec = 0;
		leadOut[subUnit].fr	 = 0;
	}
	return true;
}

void MSCDEX_SetCDInterface(int intNr, int numCD) {
	useCdromInterface = intNr;
	forceCD	= numCD;
}

void MSCDEX_ShutDown(Section* /*sec*/) {
	if ((bootguest||(use_quick_reboot&&!bootvm))&&bootdrive>=0) return;
	if (mscdex != NULL) {
		delete mscdex;
		mscdex = NULL;
	}

	curReqheaderPtr = 0;
}

/* HACK: The IDE emulation is messily tied into calling MSCDEX.EXE!
 *       We cannot shut down the mscdex object when booting into a guest OS!
 *       Need to fix this, this is backwards! */
void MSCDEX_DOS_ShutDown(Section* /*sec*/) {
	curReqheaderPtr = 0;
}

void MSCDEX_Startup(Section* sec) {
    (void)sec;//UNUSED
	if (mscdex == NULL) {
		LOG(LOG_MISC,LOG_DEBUG)("Allocating MSCDEX.EXE emulation");

		const Section_prop * dos_section=static_cast<Section_prop *>(control->GetSection("dos"));

		const char *mscdex_devname = dos_section->Get_string("mscdex device name");

		/* Register the mscdex device */
		DOS_Device * newdev = new device_MSCDEX(mscdex_devname);
		DOS_AddDevice(newdev);
		curReqheaderPtr = 0;
		/* Add Multiplexer */
		DOS_AddMultiplexHandler(MSCDEX_Handler);
		/* Create MSCDEX */
		LOG(LOG_MISC,LOG_DEBUG)("MSCDEX.EXE device name is '%s'",newdev->name);
		mscdex = new CMscdex(newdev->name);
	}
}

void MSCDEX_Init() {
	LOG(LOG_MISC,LOG_DEBUG)("Initializing MSCDEX.EXE emulation");

	AddExitFunction(AddExitFunctionFuncPair(MSCDEX_ShutDown));

	/* in any event that the DOS kernel is shutdown or abruptly wiped from memory */
	AddVMEventFunction(VM_EVENT_RESET,AddVMEventFunctionFuncPair(MSCDEX_ShutDown));
	AddVMEventFunction(VM_EVENT_DOS_EXIT_BEGIN,AddVMEventFunctionFuncPair(MSCDEX_DOS_ShutDown));
}

void CMscdex::SaveState( std::ostream& stream )
{
	// - pure data
	WRITE_POD( &defaultBufSeg, defaultBufSeg );
	WRITE_POD( &rootDriverHeaderSeg, rootDriverHeaderSeg );
}

void CMscdex::LoadState( std::istream& stream )
{
	// - pure data
	READ_POD( &defaultBufSeg, defaultBufSeg );
	READ_POD( &rootDriverHeaderSeg, rootDriverHeaderSeg );
}

void POD_Save_DOS_Mscdex( std::ostream& stream )
{
	if (!dos_kernel_disabled) {
		uint16_t dnum=mscdex->GetNumDrives();
		WRITE_POD( &dnum, dnum);
		for (uint8_t drive_unit=0; drive_unit<dnum; drive_unit++) {
			TMSF pos, start, end;
			bool playing, pause;

			mscdex->GetAudioStatus(drive_unit, playing, pause, start, end);
			mscdex->GetCurrentPos(drive_unit,pos);


			WRITE_POD( &playing, playing );
			WRITE_POD( &pause, pause );
			WRITE_POD( &pos, pos );
			WRITE_POD( &start, start );
			WRITE_POD( &end, end );
		}

		mscdex->SaveState(stream);
	}
}


void POD_Load_DOS_Mscdex( std::istream& stream )
{
	if (!dos_kernel_disabled) {
		uint16_t dnum;
		READ_POD( &dnum, dnum);
        if (mscdex->GetNumDrives()>dnum) {
            mscdex->numDrives=dnum;
            for (uint16_t i=dnum; i<mscdex->GetNumDrives(); i++) {
                delete mscdex->cdrom[i];
                mscdex->cdrom[i] = nullptr;
            }
        }
		for (uint8_t drive_unit=0; drive_unit<dnum; drive_unit++) {
			TMSF pos, start, end;
			uint32_t msf_time, play_len;
			bool playing, pause;


			READ_POD( &playing, playing );
			READ_POD( &pause, pause );
			READ_POD( &pos, pos );
			READ_POD( &start, start );
			READ_POD( &end, end );


			// end = total play time (GetAudioStatus adds +150)
			// pos = current play cursor
			// start = start play cursor
			play_len = end.min * 75 * 60 + ( end.sec * 75 ) + end.fr - 150;
			play_len -= ( pos.min - start.min ) * 75 * 60 + ( pos.sec - start.sec ) * 75 + ( pos.fr - start.fr );
			msf_time = ( pos.min << 16 ) + ( pos.sec << 8 ) + ( pos.fr );


			// first play, then simulate pause
			mscdex->StopAudio(drive_unit);

			if( playing ) mscdex->PlayAudioMSF(drive_unit, msf_time, play_len);
			if( pause ) mscdex->PlayAudioMSF(drive_unit, msf_time, 0);
		}

		mscdex->LoadState(stream);
	}
}
