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


#include <string.h>
#include <ctype.h>
#include "regs.h"
#include "callback.h"
#include "dos_system.h"
#include "dos_inc.h"
#include "setup.h"
#include "support.h"
#include "bios_disk.h"
#include "cpu.h"

#include "cdrom.h"

#define MSCDEX_LOG LOG(LOG_MISC, LOG_DEBUG)
#define MSCDEX_LOG_ERROR LOG(LOG_MISC, LOG_ERROR)

#define MSCDEX_VERSION_HIGH	2
#define MSCDEX_VERSION_LOW	23
#define MSCDEX_MAX_DRIVES	16

// Error Codes
#define MSCDEX_ERROR_INVALID_FUNCTION	1
#define MSCDEX_ERROR_BAD_FORMAT			11
#define MSCDEX_ERROR_UNKNOWN_DRIVE		15
#define MSCDEX_ERROR_DRIVE_NOT_READY	21

// Request Status
#define	REQUEST_STATUS_DONE		0x0100
#define	REQUEST_STATUS_ERROR	0x8000

// Use cdrom Interface
int useCdromInterface	= CDROM_USE_SDL;
int forceCD				= -1;
extern int bootdrive;
extern bool dos_kernel_disabled, bootguest, bootvm, use_quick_reboot;

static Bitu MSCDEX_Strategy_Handler(void); 
static Bitu MSCDEX_Interrupt_Handler(void);

class DOS_DeviceHeader:public MemStruct {
public:
	DOS_DeviceHeader(PhysPt ptr)				{ pt = ptr; };
	
	void	SetNextDeviceHeader	(RealPt ptr)	{ sSave(sDeviceHeader,nextDeviceHeader,ptr);	};
	RealPt	GetNextDeviceHeader	(void)			{ return sGet(sDeviceHeader,nextDeviceHeader);	};
	void	SetAttribute		(Bit16u atr)	{ sSave(sDeviceHeader,devAttributes,atr);		};
	void	SetDriveLetter		(Bit8u letter)	{ sSave(sDeviceHeader,driveLetter,letter);		};
	void	SetNumSubUnits		(Bit8u num)		{ sSave(sDeviceHeader,numSubUnits,num);			};
	Bit8u	GetNumSubUnits		(void)			{ return sGet(sDeviceHeader,numSubUnits);		};
	void	SetName				(char const* _name)	{ MEM_BlockWrite(pt+offsetof(sDeviceHeader,name),_name,8); };
	void	SetInterrupt		(Bit16u ofs)	{ sSave(sDeviceHeader,interrupt,ofs);			};
	void	SetStrategy			(Bit16u ofs)	{ sSave(sDeviceHeader,strategy,ofs);			};

	#ifdef _MSC_VER
	#pragma pack(1)
	#endif
	struct sDeviceHeader{
		RealPt	nextDeviceHeader;
		Bit16u	devAttributes;
		Bit16u	strategy;
		Bit16u	interrupt;
		Bit8u	name[8];
		Bit16u  wReserved;
		Bit8u	driveLetter;
		Bit8u	numSubUnits;
    } GCC_ATTRIBUTE(packed) TDeviceHeader = {};
	#ifdef _MSC_VER
	#pragma pack()
	#endif
};

class CMscdex {
public:
	CMscdex		(void);
	~CMscdex	(void);

	Bit16u		GetVersion			(void)	{ return (MSCDEX_VERSION_HIGH<<8)+MSCDEX_VERSION_LOW; };
	Bit16u		GetNumDrives		(void)	{ return numDrives;			};
	Bit16u		GetFirstDrive		(void)	{ return dinfo[0].drive; };
	Bit8u		GetSubUnit			(Bit16u _drive);
	bool		GetUPC				(Bit8u subUnit, Bit8u& attr, char* upc);

	void		InitNewMedia		(Bit8u subUnit);
	bool		PlayAudioSector		(Bit8u subUnit, Bit32u sector, Bit32u length);
	bool		PlayAudioMSF		(Bit8u subUnit, Bit32u start, Bit32u length);
	bool		StopAudio			(Bit8u subUnit);
	bool		GetAudioStatus		(Bit8u subUnit, bool& playing, bool& pause, TMSF& start, TMSF& end);

	bool		GetSubChannelData	(Bit8u subUnit, Bit8u& attr, Bit8u& track, Bit8u &index, TMSF& rel, TMSF& abs);

	int			RemoveDrive			(Bit16u _drive);
	int			AddDrive			(Bit16u _drive, char* physicalPath, Bit8u& subUnit);
	bool 		HasDrive			(Bit16u drive);
	void		ReplaceDrive		(CDROM_Interface* newCdrom, Bit8u subUnit);
	void		GetDrives			(PhysPt data);
	void		GetDriverInfo		(PhysPt data);
	bool		GetVolumeName		(Bit8u subUnit, char* data);
	bool		GetFileName			(Bit16u drive, Bit16u pos, PhysPt data);
	bool		GetDirectoryEntry	(Bit16u drive, bool copyFlag, PhysPt pathname, PhysPt buffer, Bit16u& error);
	bool		ReadVTOC			(Bit16u drive, Bit16u volume, PhysPt data, Bit16u& offset, Bit16u& error);
	bool		ReadSectors			(Bit16u drive, Bit32u sector, Bit16u num, PhysPt data);
	bool		ReadSectors			(Bit8u subUnit, bool raw, Bit32u sector, Bit16u num, PhysPt data);
	bool		ReadSectorsMSF		(Bit8u subUnit, bool raw, Bit32u start, Bit16u num, PhysPt data);
	bool		SendDriverRequest	(Bit16u drive, PhysPt data);
	bool		IsValidDrive		(Bit16u _drive);
	bool		GetCDInfo			(Bit8u subUnit, Bit8u& tr1, Bit8u& tr2, TMSF& leadOut);
	Bit32u		GetVolumeSize		(Bit8u subUnit);
	bool		GetTrackInfo		(Bit8u subUnit, Bit8u track, Bit8u& attr, TMSF& start);
	Bit16u		GetStatusWord		(Bit8u subUnit,Bit16u status);
	bool		GetCurrentPos		(Bit8u subUnit, TMSF& pos);
	Bit32u		GetDeviceStatus		(Bit8u subUnit);
	bool		GetMediaStatus		(Bit8u subUnit, Bit8u& status);
	bool		LoadUnloadMedia		(Bit8u subUnit, bool unload);
	bool		ResumeAudio			(Bit8u subUnit);
	bool		GetMediaStatus		(Bit8u subUnit, bool& media, bool& changed, bool& trayOpen);

	PhysPt		GetDefaultBuffer	(void);
	PhysPt		GetTempBuffer		(void);

	void SaveState( std::ostream& stream );
	void LoadState( std::istream& stream );

	Bit16u		numDrives = 0;

	typedef struct SDriveInfo {
		Bit8u	drive;			// drive letter in dosbox
		Bit8u	physDrive;		// drive letter in system
		bool	audioPlay;		// audio playing active
		bool	audioPaused;	// audio playing paused
		Bit32u	audioStart;		// StartLoc for resume
		Bit32u	audioEnd;		// EndLoc for resume
		bool	locked;			// drive locked ?
		bool	lastResult;		// last operation success ?
		Bit32u	volumeSize;		// for media change
		TCtrl	audioCtrl;		// audio channel control
	} TDriveInfo;

	Bit16u				defaultBufSeg = 0;
	TDriveInfo			dinfo[MSCDEX_MAX_DRIVES];
	CDROM_Interface*		cdrom[MSCDEX_MAX_DRIVES];
	Bit16u		rootDriverHeaderSeg = 0;

	bool		ChannelControl		(Bit8u subUnit, TCtrl ctrl);
	bool		GetChannelControl	(Bit8u subUnit, TCtrl& ctrl);
};

CMscdex::CMscdex(void) {
	memset(dinfo,0,sizeof(dinfo));
	for (Bit32u i=0; i<MSCDEX_MAX_DRIVES; i++) cdrom[i] = 0;
}

CMscdex::~CMscdex(void) {
	if ((bootguest||(use_quick_reboot&&!bootvm))&&bootdrive>=0) return;
	defaultBufSeg = 0;
	for (Bit16u i=0; i<GetNumDrives(); i++) {
		delete cdrom[i];
		cdrom[i] = 0;
	}
}

void CMscdex::GetDrives(PhysPt data)
{
	for (Bit16u i=0; i<GetNumDrives(); i++) mem_writeb(data+i,dinfo[i].drive);
}

bool CMscdex::IsValidDrive(Bit16u _drive)
{
	_drive &= 0xff; //Only lowerpart (Ultimate domain)
	for (Bit16u i=0; i<GetNumDrives(); i++) if (dinfo[i].drive==_drive) return true;
	return false;
}

Bit8u CMscdex::GetSubUnit(Bit16u _drive)
{
	_drive &= 0xff; //Only lowerpart (Ultimate domain)
	for (Bit16u i=0; i<GetNumDrives(); i++) if (dinfo[i].drive==_drive) return (Bit8u)i;
	return 0xff;
}

int CMscdex::RemoveDrive(Bit16u _drive)
{
	Bit16u idx = MSCDEX_MAX_DRIVES;
	for (Bit16u i=0; i<GetNumDrives(); i++) {
		if (dinfo[i].drive == _drive) {
			idx = i;
			break;
		}
	}

	if (idx == MSCDEX_MAX_DRIVES || (idx!=0 && idx!=GetNumDrives()-1)) return 0;
	delete cdrom[idx];
	if (idx==0) {
		for (Bit16u i=0; i<GetNumDrives(); i++) {
			if (i == MSCDEX_MAX_DRIVES-1) {
				cdrom[i] = 0;
				memset(&dinfo[i],0,sizeof(TDriveInfo));
			} else {
				dinfo[i] = dinfo[i+1];
				cdrom[i] = cdrom[i+1];
			}
		}
	} else {
		cdrom[idx] = 0;
		memset(&dinfo[idx],0,sizeof(TDriveInfo));
	}
	numDrives--;

	if (GetNumDrives() == 0) {
		DOS_DeviceHeader devHeader(PhysMake(rootDriverHeaderSeg,0));
		Bit16u off = sizeof(DOS_DeviceHeader::sDeviceHeader);
		devHeader.SetStrategy(off+4);		// point to the RETF (To deactivate MSCDEX)
		devHeader.SetInterrupt(off+4);		// point to the RETF (To deactivate MSCDEX)
		devHeader.SetDriveLetter(0);
	} else if (idx==0) {
		DOS_DeviceHeader devHeader(PhysMake(rootDriverHeaderSeg,0));
		devHeader.SetDriveLetter(GetFirstDrive()+1);
	}
	return 1;
}

int CMscdex::AddDrive(Bit16u _drive, char* physicalPath, Bit8u& subUnit)
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
//				cdrom[numDrives] = new CDROM_Interface_Ioctl(CDROM_Interface_Ioctl::CDIOCTL_CDA_DIO);
//				LOG(LOG_MISC,LOG_NORMAL)("MSCDEX: IOCTL Interface.");
//				break;
			}
			if (useCdromInterface==CDROM_USE_IOCTL_DX) {
//				cdrom[numDrives] = new CDROM_Interface_Ioctl(CDROM_Interface_Ioctl::CDIOCTL_CDA_DX);
//				LOG(LOG_MISC,LOG_NORMAL)("MSCDEX: IOCTL Interface (digital audio extraction).");
//				break;
			}
			if (useCdromInterface==CDROM_USE_IOCTL_MCI) {
//				cdrom[numDrives] = new CDROM_Interface_Ioctl(CDROM_Interface_Ioctl::CDIOCTL_CDA_MCI);
//				LOG(LOG_MISC,LOG_NORMAL)("MSCDEX: IOCTL Interface (media control interface).");
//				break;
			}
		}
		if (useCdromInterface==CDROM_USE_ASPI) {
			// all Wins - ASPI
//			cdrom[numDrives] = new CDROM_Interface_Aspi();
//			LOG(LOG_MISC,LOG_NORMAL)("MSCDEX: ASPI Interface.");
//			break;
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
		cdrom[numDrives] = new CDROM_Interface_Image((Bit8u)numDrives);
		break;
	case 0x02:	// fake cdrom interface (directories)
		cdrom[numDrives] = new CDROM_Interface_Fake;
		LOG(LOG_MISC,LOG_NORMAL)("MSCDEX: Mounting directory as cdrom: %s",physicalPath);
		LOG(LOG_MISC,LOG_NORMAL)("MSCDEX: You wont have full MSCDEX support !");
		result = 5;
		break;
	default	:	// weird result
		return 6;
	}

	if (!cdrom[numDrives]->SetDevice(physicalPath,forceCD)) {
//		delete cdrom[numDrives] ; mount seems to delete it
		return 3;
	}


	if (rootDriverHeaderSeg==0) {
		
		Bit16u driverSize = sizeof(DOS_DeviceHeader::sDeviceHeader) + 10; // 10 = Bytes for 3 callbacks
		
		// Create Device Header
		Bit16u seg = DOS_GetMemory(driverSize/16u+((driverSize%16u)>0u),"MSCDEX device header");
		DOS_DeviceHeader devHeader(PhysMake(seg,0u));
		devHeader.SetNextDeviceHeader	(0xFFFFFFFFul);
		devHeader.SetAttribute(0xc800u);
		devHeader.SetDriveLetter		(_drive+1u);
		devHeader.SetNumSubUnits		(1u);
		devHeader.SetName				("MSCD001 ");

		//Link it in the device chain
		Bit32u start = dos_infoblock.GetDeviceChain();
		Bit16u segm  = (Bit16u)(start>>16ul);
		Bit16u offm  = (Bit16u)(start&0xFFFFu);
		while(start != 0xFFFFFFFFul) {
			segm  = (Bit16u)(start>>16u);
			offm  = (Bit16u)(start&0xFFFFu);
			start = real_readd(segm,offm);
		}
		real_writed(segm,offm,(unsigned int)seg<<16u);

		// Create Callback Strategy
		Bit16u off = sizeof(DOS_DeviceHeader::sDeviceHeader);
		Bit16u call_strategy=CALLBACK_Allocate();
		CallBack_Handlers[call_strategy]=MSCDEX_Strategy_Handler;
		real_writeb(seg,off+0,(Bit8u)0xFE);		//GRP 4
		real_writeb(seg,off+1,(Bit8u)0x38);		//Extra Callback instruction
		real_writew(seg,off+2,call_strategy);	//The immediate word
		real_writeb(seg,off+4,(Bit8u)0xCB);		//A RETF Instruction
		devHeader.SetStrategy(off);
		
		// Create Callback Interrupt
		off += 5;
		Bit16u call_interrupt=CALLBACK_Allocate();
		CallBack_Handlers[call_interrupt]=MSCDEX_Interrupt_Handler;
		real_writeb(seg,off+0,(Bit8u)0xFE);		//GRP 4
		real_writeb(seg,off+1,(Bit8u)0x38);		//Extra Callback instruction
		real_writew(seg,off+2,call_interrupt);	//The immediate word
		real_writeb(seg,off+4,(Bit8u)0xCB);		//A RETF Instruction
		devHeader.SetInterrupt(off);
		
		rootDriverHeaderSeg = seg;
	
	} else if (GetNumDrives() == 0) {
		DOS_DeviceHeader devHeader(PhysMake(rootDriverHeaderSeg,0));
		Bit16u off = sizeof(DOS_DeviceHeader::sDeviceHeader);
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
		for (Bit16u i=GetNumDrives(); i>0; i--) {
			dinfo[i] = dinfo[i-1];
			cdrom[i] = cdrom[i-1];
			CDROM_Interface_Image::images[i] = CDROM_Interface_Image::images[i-1];
		}
		cdrom[0] = _cdrom;
		CDROM_Interface_Image::images[0] = _cdimg;
		dinfo[0].drive		= (Bit8u)_drive;
		dinfo[0].physDrive	= (Bit8u)toupper(physicalPath[0]);
		subUnit = 0;
	} else {
		dinfo[numDrives].drive		= (Bit8u)_drive;
		dinfo[numDrives].physDrive	= (Bit8u)toupper(physicalPath[0]);
		subUnit = (Bit8u)numDrives;
	}
	numDrives++;
	// init channel control
	for (Bit8u chan=0;chan<4;chan++) {
		dinfo[subUnit].audioCtrl.out[chan]=chan;
		dinfo[subUnit].audioCtrl.vol[chan]=0xff;
	}
	// stop audio
	StopAudio(subUnit);
	return result;
}

bool CMscdex::HasDrive(Bit16u drive) {
	return (GetSubUnit(drive) != 0xff);
}

void CMscdex::ReplaceDrive(CDROM_Interface* newCdrom, Bit8u subUnit) {
	if (cdrom[subUnit] != NULL) {
		StopAudio(subUnit);
		delete cdrom[subUnit];
	}
	cdrom[subUnit] = newCdrom;
}

PhysPt CMscdex::GetDefaultBuffer(void) {
	if (defaultBufSeg==0) {
		Bit16u size = (2352*2+15)/16;
		defaultBufSeg = DOS_GetMemory(size,"MSCDEX default buffer");
	}
	return PhysMake(defaultBufSeg,2352);
}

PhysPt CMscdex::GetTempBuffer(void) {
	if (defaultBufSeg==0) {
		Bit16u size = (2352*2+15)/16;
		defaultBufSeg = DOS_GetMemory(size,"MSCDEX temp buffer");
	}
	return PhysMake(defaultBufSeg,0);
}

void CMscdex::GetDriverInfo	(PhysPt data) {
	for (Bit16u i=0; i<GetNumDrives(); i++) {
		mem_writeb(data  ,(Bit8u)i);	// subunit
		mem_writed(data+1,RealMake(rootDriverHeaderSeg,0));
		data+=5;
	}
}

bool CMscdex::GetCDInfo(Bit8u subUnit, Bit8u& tr1, Bit8u& tr2, TMSF& leadOut) {
	if (subUnit>=numDrives) return false;
	int tr1i,tr2i;
	// Assume Media change
	cdrom[subUnit]->InitNewMedia();
	dinfo[subUnit].lastResult = cdrom[subUnit]->GetAudioTracks(tr1i,tr2i,leadOut);
	if (!dinfo[subUnit].lastResult) {
		tr1 = tr2 = 0;
		memset(&leadOut,0,sizeof(leadOut));
	} else {
		tr1 = (Bit8u) tr1i;
		tr2 = (Bit8u) tr2i;
	}
	return dinfo[subUnit].lastResult;
}

bool CMscdex::GetTrackInfo(Bit8u subUnit, Bit8u track, Bit8u& attr, TMSF& start) {
	if (subUnit>=numDrives) return false;
	dinfo[subUnit].lastResult = cdrom[subUnit]->GetAudioTrackInfo(track,start,attr);	
	if (!dinfo[subUnit].lastResult) {
		attr = 0;
		memset(&start,0,sizeof(start));
	}
	return dinfo[subUnit].lastResult;
}

bool CMscdex::PlayAudioSector(Bit8u subUnit, Bit32u sector, Bit32u length) {
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

bool CMscdex::PlayAudioMSF(Bit8u subUnit, Bit32u start, Bit32u length) {
	if (subUnit>=numDrives) return false;
	Bit8u min		= (Bit8u)(start>>16) & 0xFF;
	Bit8u sec		= (Bit8u)(start>> 8) & 0xFF;
	Bit8u fr		= (Bit8u)(start>> 0) & 0xFF;
	Bit32u sector	= min*60u*75u+sec*75u+fr - 150u;
	return dinfo[subUnit].lastResult = PlayAudioSector(subUnit,sector,length);
}

bool CMscdex::GetSubChannelData(Bit8u subUnit, Bit8u& attr, Bit8u& track, Bit8u &index, TMSF& rel, TMSF& abs) {
	if (subUnit>=numDrives) return false;
	dinfo[subUnit].lastResult = cdrom[subUnit]->GetAudioSub(attr,track,index,rel,abs);
	if (!dinfo[subUnit].lastResult) {
		attr = track = index = 0;
		memset(&rel,0,sizeof(rel));
		memset(&abs,0,sizeof(abs));
	}
	return dinfo[subUnit].lastResult;
}

bool CMscdex::GetAudioStatus(Bit8u subUnit, bool& playing, bool& pause, TMSF& start, TMSF& end) {
	if (subUnit>=numDrives) return false;
	dinfo[subUnit].lastResult = cdrom[subUnit]->GetAudioStatus(playing,pause);
	if (dinfo[subUnit].lastResult) {
		if (playing) {
			// Start
			Bit32u addr	= dinfo[subUnit].audioStart + 150;
			start.fr	= (Bit8u)(addr%75);	addr/=75;
			start.sec	= (Bit8u)(addr%60); 
			start.min	= (Bit8u)(addr/60);
			// End
			addr		= dinfo[subUnit].audioEnd + 150;
			end.fr		= (Bit8u)(addr%75);	addr/=75;
			end.sec		= (Bit8u)(addr%60); 
			end.min		= (Bit8u)(addr/60);
		} else {
			memset(&start,0,sizeof(start));
			memset(&end,0,sizeof(end));
		}
	} else {
		playing		= false;
		pause		= false;
		memset(&start,0,sizeof(start));
		memset(&end,0,sizeof(end));
	}
	
	return dinfo[subUnit].lastResult;
}

bool CMscdex::StopAudio(Bit8u subUnit) {
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

bool CMscdex::ResumeAudio(Bit8u subUnit) {
	if (subUnit>=numDrives) return false;
	return dinfo[subUnit].lastResult = PlayAudioSector(subUnit,dinfo[subUnit].audioStart,dinfo[subUnit].audioEnd);
}

Bit32u CMscdex::GetVolumeSize(Bit8u subUnit) {
	if (subUnit>=numDrives) return false;
	Bit8u tr1,tr2;
	TMSF leadOut;
	dinfo[subUnit].lastResult = GetCDInfo(subUnit,tr1,tr2,leadOut);
	if (dinfo[subUnit].lastResult) return (leadOut.min*60u*75u)+(leadOut.sec*75u)+leadOut.fr;
	return 0;
}

bool CMscdex::ReadVTOC(Bit16u drive, Bit16u volume, PhysPt data, Bit16u& offset, Bit16u& error) {
	Bit8u subunit = GetSubUnit(drive);
/*	if (subunit>=numDrives) {
		error=MSCDEX_ERROR_UNKNOWN_DRIVE;
		return false;
	} */
	if (!ReadSectors(subunit,false,16u+volume,1u,data)) {
		error=MSCDEX_ERROR_DRIVE_NOT_READY;
		return false;
	}
	char id[5];
	MEM_BlockRead(data + 1, id, 5);
	if (strncmp("CD001", id, 5)==0) offset = 0;
	else {
		MEM_BlockRead(data + 9, id, 5);
		if (strncmp("CDROM", id, 5)==0) offset = 8;
		else {
			error = MSCDEX_ERROR_BAD_FORMAT;
			return false;
		}
	}
	Bit8u type = mem_readb(data + offset);
	error = (type == 1) ? 1 : (type == 0xFF) ? 0xFF : 0;
	return true;
}

bool CMscdex::GetVolumeName(Bit8u subUnit, char* data) {	
	if (subUnit>=numDrives) return false;
	Bit16u drive = dinfo[subUnit].drive;

	Bit16u offset = 0, error;
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

bool CMscdex::GetFileName(Bit16u drive, Bit16u pos, PhysPt data) {
	Bit16u offset = 0, error;
	bool success = false;
	PhysPt ptoc = GetTempBuffer();
	success = ReadVTOC(drive,0x00,ptoc,offset,error);
	if (success) {
		Bit8u len;
		for (len=0;len<37;len++) {
			Bit8u c=mem_readb(ptoc+offset+pos+len);
			if (c==0 || c==0x20) break;
		}
		MEM_BlockCopy(data,ptoc+offset+pos,len);
		mem_writeb(data+len,0);
	}
	return success; 
}

bool CMscdex::GetUPC(Bit8u subUnit, Bit8u& attr, char* upc)
{
	if (subUnit>=numDrives) return false;
	return dinfo[subUnit].lastResult = cdrom[subUnit]->GetUPC(attr,&upc[0]);
}

bool CMscdex::ReadSectors(Bit8u subUnit, bool raw, Bit32u sector, Bit16u num, PhysPt data) {
	if (subUnit>=numDrives) return false;
	if ((cpu_cycles_count_t)(4u*num*2048u+5u) < CPU_Cycles) CPU_Cycles -= cpu_cycles_count_t(4u*num*2048u);
	else CPU_Cycles = 5u;
	dinfo[subUnit].lastResult = cdrom[subUnit]->ReadSectors(data,raw,sector,num);
	return dinfo[subUnit].lastResult;
}

bool CMscdex::ReadSectorsMSF(Bit8u subUnit, bool raw, Bit32u start, Bit16u num, PhysPt data) {
	if (subUnit>=numDrives) return false;
	Bit8u min		= (Bit8u)(start>>16) & 0xFF;
	Bit8u sec		= (Bit8u)(start>> 8) & 0xFF;
	Bit8u fr		= (Bit8u)(start>> 0) & 0xFF;
	Bit32u sector	= min*60u*75u+sec*75u+fr - 150u;
	return ReadSectors(subUnit,raw,sector,num,data);
}

// Called from INT 2F
bool CMscdex::ReadSectors(Bit16u drive, Bit32u sector, Bit16u num, PhysPt data) {
	return ReadSectors(GetSubUnit(drive),false,sector,num,data);
}

bool CMscdex::GetDirectoryEntry(Bit16u drive, bool copyFlag, PhysPt pathname, PhysPt buffer, Bit16u& error) {
	char	volumeID[6] = {0};
	char	searchName[256];
	char	entryName[256];
	bool	foundComplete = false;
	bool	nextPart = true;
    const char* useName = NULL;
    Bit8u   entryLength, nameLength;
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
	Bit16u offset = iso ? 156:180;
	// get directory position
	Bit32u dirEntrySector	= mem_readd(defBuffer+offset+2);
	Bits dirSize		= (Bit32s)mem_readd(defBuffer+offset+10);
	while (dirSize>0) {
		Bit16u index = 0;
		if (!ReadSectors(GetSubUnit(drive),false,dirEntrySector,1,defBuffer)) return false;
		// Get string part
		bool foundName = false;
		if (nextPart) {
			if (searchPos) { 
				useName = searchPos; 
				searchPos = strchr(searchPos,'\\'); 
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
					Bit8u readBuf[256];
					Bit8u writeBuf[256];
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
			dirSize	= (Bit32s)mem_readd(defBuffer+index+10);
			nextPart = true;
		} else {
			// continue search in next sector
			dirSize -= 2048;
			dirEntrySector++;
			nextPart = false;
		}
	}
	error = 2; // file not found
	return false; // not found
}

bool CMscdex::GetCurrentPos(Bit8u subUnit, TMSF& pos) {
	if (subUnit>=numDrives) return false;
	TMSF rel;
	Bit8u attr,track,index;
	dinfo[subUnit].lastResult = GetSubChannelData(subUnit, attr, track, index, rel, pos);
	if (!dinfo[subUnit].lastResult) memset(&pos,0,sizeof(pos));
	return dinfo[subUnit].lastResult;
}

bool CMscdex::GetMediaStatus(Bit8u subUnit, bool& media, bool& changed, bool& trayOpen) {
	if (subUnit>=numDrives) return false;
	dinfo[subUnit].lastResult = cdrom[subUnit]->GetMediaTrayStatus(media,changed,trayOpen);
	return dinfo[subUnit].lastResult;
}

Bit32u CMscdex::GetDeviceStatus(Bit8u subUnit) {
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

	Bit32u status = ((trayOpen?1u:0u) << 0u)					|	// Drive is open ?
					((dinfo[subUnit].locked?1u:0u) << 1u)		|	// Drive is locked ?
					(1u<<2u)									|	// raw + cooked sectors
					(1u<<4u)									|	// Can read sudio
					(1u<<8u)									|	// Can control audio
					(1u<<9u)									|	// Red book & HSG
					((dinfo[subUnit].audioPlay?1u:0u) << 10u)	|	// Audio is playing ?
					((media?0u:1u) << 11u);						// Drive is empty ?
	return status;
}

bool CMscdex::GetMediaStatus(Bit8u subUnit, Bit8u& status) {
	if (subUnit>=numDrives) return false;
/*	bool media,changed,open,result;
	result = GetMediaStatus(subUnit,media,changed,open);
	status = changed ? 0xFF : 0x01;
	return result; */
	status = getSwapRequest() ? 0xFF : 0x01;
	return true;
}

bool CMscdex::LoadUnloadMedia(Bit8u subUnit, bool unload) {
	if (subUnit>=numDrives) return false;
	dinfo[subUnit].lastResult = cdrom[subUnit]->LoadUnloadMedia(unload);
	return dinfo[subUnit].lastResult;
}

bool CMscdex::SendDriverRequest(Bit16u drive, PhysPt data) {
	Bit8u subUnit = GetSubUnit(drive);
	if (subUnit>=numDrives) return false;
	// Get SubUnit
	mem_writeb(data+1,subUnit);
	// Call Strategy / Interrupt
	MSCDEX_Strategy_Handler();
	MSCDEX_Interrupt_Handler();
	return true;
}

Bit16u CMscdex::GetStatusWord(Bit8u subUnit,Bit16u status) {
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

void CMscdex::InitNewMedia(Bit8u subUnit) {
	if (subUnit<numDrives) {
		// Reopen new media
		cdrom[subUnit]->InitNewMedia();
	}
}

bool CMscdex::ChannelControl(Bit8u subUnit, TCtrl ctrl) {
	if (subUnit>=numDrives) return false;
	// adjust strange channel mapping
	if (ctrl.out[0]>1) ctrl.out[0]=0;
	if (ctrl.out[1]>1) ctrl.out[1]=1;
	dinfo[subUnit].audioCtrl=ctrl;
	cdrom[subUnit]->ChannelControl(ctrl);
	return true;
}

bool CMscdex::GetChannelControl(Bit8u subUnit, TCtrl& ctrl) {
	if (subUnit>=numDrives) return false;
	ctrl=dinfo[subUnit].audioCtrl;
	return true;
}

static CMscdex* mscdex = 0;
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

static Bit16u MSCDEX_IOCTL_Input(PhysPt buffer,Bit8u drive_unit) {
	Bit8u ioctl_fct = mem_readb(buffer);
	MSCDEX_LOG("MSCDEX: IOCTL INPUT Subfunction %02X",(int)ioctl_fct);
	switch (ioctl_fct) {
		case 0x00 : /* Get Device Header address */
					mem_writed(buffer+1,RealMake(mscdex->rootDriverHeaderSeg,0));
					break;
		case 0x01 :{/* Get current position */
					TMSF pos;
					mscdex->GetCurrentPos(drive_unit,pos);
					Bit8u addr_mode = mem_readb(buffer+1);
					if (addr_mode==0) {			// HSG
						Bit32u frames=MSF_TO_FRAMES(pos.min, pos.sec, pos.fr);
						if (frames<150) MSCDEX_LOG_ERROR("MSCDEX: Get position: invalid position %d:%d:%d", pos.min, pos.sec, pos.fr);
						else frames-=150;
						mem_writed(buffer+2,frames);
					} else if (addr_mode==1) {	// Red book
						mem_writeb(buffer+2,pos.fr);
						mem_writeb(buffer+3,pos.sec);
						mem_writeb(buffer+4,pos.min);
						mem_writeb(buffer+5,0x00);
					} else {
						MSCDEX_LOG_ERROR("MSCDEX: Get position: invalid address mode %x",addr_mode);
						return 0x03;		// invalid function
					}
				   }break;
		case 0x04 : /* Audio Channel control */
					TCtrl ctrl;
					if (!mscdex->GetChannelControl(drive_unit,ctrl)) return 0x01;
					for (Bit8u chan=0;chan<4;chan++) {
						mem_writeb(buffer+chan*2u+1u,ctrl.out[chan]);
						mem_writeb(buffer+chan*2u+2u,ctrl.vol[chan]);
					}
					break;
		case 0x06 : /* Get Device status */
					mem_writed(buffer+1,mscdex->GetDeviceStatus(drive_unit)); 
					break;
		case 0x07 : /* Get sector size */
					if (mem_readb(buffer+1)==0) mem_writew(buffer+2,2048);
					else if (mem_readb(buffer+1)==1) mem_writew(buffer+2,2352);
					else return 0x03;		// invalid function
					break;
		case 0x08 : /* Get size of current volume */
					mem_writed(buffer+1,mscdex->GetVolumeSize(drive_unit));
					break;
		case 0x09 : /* Media change ? */
					Bit8u status;
					if (!mscdex->GetMediaStatus(drive_unit,status)) {
						status = 0;		// state unknown
					}
					mem_writeb(buffer+1,status);
					break;
		case 0x0A : /* Get Audio Disk info */	
					Bit8u tr1,tr2; TMSF leadOut;
					if (!mscdex->GetCDInfo(drive_unit,tr1,tr2,leadOut)) return 0x05;
					mem_writeb(buffer+1,tr1);
					mem_writeb(buffer+2,tr2);
					mem_writeb(buffer+3,leadOut.fr);
					mem_writeb(buffer+4,leadOut.sec);
					mem_writeb(buffer+5,leadOut.min);
					mem_writeb(buffer+6,0x00);
					break;
		case 0x0B :{/* Audio Track Info */
					Bit8u attr; TMSF start;
					Bit8u track = mem_readb(buffer+1);
					mscdex->GetTrackInfo(drive_unit,track,attr,start);		
					mem_writeb(buffer+2,start.fr);
					mem_writeb(buffer+3,start.sec);
					mem_writeb(buffer+4,start.min);
					mem_writeb(buffer+5,0x00);
					mem_writeb(buffer+6,attr);
					break; }
		case 0x0C :{/* Get Audio Sub Channel data */
					Bit8u attr,track,index; 
					TMSF abs,rel;
					mscdex->GetSubChannelData(drive_unit,attr,track,index,rel,abs);
					mem_writeb(buffer+1,attr);
					mem_writeb(buffer+2,track);
					mem_writeb(buffer+3,index);
					mem_writeb(buffer+4,rel.min);
					mem_writeb(buffer+5,rel.sec);
					mem_writeb(buffer+6,rel.fr);
					mem_writeb(buffer+7,0x00);
					mem_writeb(buffer+8,abs.min);
					mem_writeb(buffer+9,abs.sec);
					mem_writeb(buffer+10,abs.fr);
					break;
				   }
		case 0x0E :{ /* Get UPC */	
					Bit8u attr; char upc[8];
					mscdex->GetUPC(drive_unit,attr,&upc[0]);
					mem_writeb(buffer+1u,attr);
					for (unsigned int i=0; i<7; i++) mem_writeb(buffer+2u+i,(unsigned char)upc[i]);
					mem_writeb(buffer+9,0x00u);
					break;
				   }
		case 0x0F :{ /* Get Audio Status */	
					bool playing,pause;
					TMSF resStart,resEnd;
					mscdex->GetAudioStatus(drive_unit,playing,pause,resStart,resEnd);
					mem_writeb(buffer+1u,pause);
					mem_writeb(buffer+3u,resStart.min);
					mem_writeb(buffer+4u,resStart.sec);
					mem_writeb(buffer+5u,resStart.fr);
					mem_writeb(buffer+6u,0x00u);
					mem_writeb(buffer+7u,resEnd.min);
					mem_writeb(buffer+8u,resEnd.sec);
					mem_writeb(buffer+9u,resEnd.fr);
					mem_writeb(buffer+10u,0x00u);
					break;
				   }
		default :	LOG(LOG_MISC,LOG_ERROR)("MSCDEX: Unsupported IOCTL INPUT Subfunction %02X",(int)ioctl_fct);
					return 0x03;	// invalid function
	}
	return 0x00;	// success
}

static Bit16u MSCDEX_IOCTL_Optput(PhysPt buffer,Bit8u drive_unit) {
	Bit8u ioctl_fct = mem_readb(buffer);
//	MSCDEX_LOG("MSCDEX: IOCTL OUTPUT Subfunction %02X",ioctl_fct);
	switch (ioctl_fct) {
		case 0x00 :	// Unload /eject media
					if (!mscdex->LoadUnloadMedia(drive_unit,true)) return 0x02;
					break;
		case 0x03: //Audio Channel control
					TCtrl ctrl;
					for (Bit8u chan=0;chan<4;chan++) {
						ctrl.out[chan]=mem_readb(buffer+chan*2u+1u);
						ctrl.vol[chan]=mem_readb(buffer+chan*2u+2u);
					}
					if (!mscdex->ChannelControl(drive_unit,ctrl)) return 0x01;
					break;
		case 0x01 : // (un)Lock door 
					// do nothing -> report as success
					break;
		case 0x02 : // Reset Drive
					LOG(LOG_MISC,LOG_WARN)("cdromDrive reset");
					if (!mscdex->StopAudio(drive_unit))  return 0x02;
					break;
		case 0x05 :	// load media
					if (!mscdex->LoadUnloadMedia(drive_unit,false)) return 0x02;
					break;
		default	:	LOG(LOG_MISC,LOG_ERROR)("MSCDEX: Unsupported IOCTL OUTPUT Subfunction %02X",(int)ioctl_fct);
					return 0x03;	// invalid function
	}
	return 0x00;	// success
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
	Bit8u	subUnit		= mem_readb(curReqheaderPtr+1);
	Bit8u	funcNr		= mem_readb(curReqheaderPtr+2);
	Bit16u	errcode		= 0;
	PhysPt	buffer		= 0;

	MSCDEX_LOG("MSCDEX: Driver Function %02X",funcNr);

	if ((funcNr==0x03) || (funcNr==0x0c) || (funcNr==0x80) || (funcNr==0x82)) {
		buffer = PhysMake(mem_readw(curReqheaderPtr+0x10),mem_readw(curReqheaderPtr+0x0E));
	}

 	switch (funcNr) {
		case 0x03	: {	/* IOCTL INPUT */
						Bit16u error=MSCDEX_IOCTL_Input(buffer,subUnit);
						if (error) errcode = error;
						break;
					  }
		case 0x0C	: {	/* IOCTL OUTPUT */
						Bit16u error=MSCDEX_IOCTL_Optput(buffer,subUnit);
						if (error) errcode = error;
						break;
					  }
		case 0x0D	:	// device open
		case 0x0E	:	// device close - dont care :)
						break;
		case 0x80	:	// Read long
		case 0x82	: { // Read long prefetch -> both the same here :)
						Bit32u start = mem_readd(curReqheaderPtr+0x14);
						Bit16u len	 = mem_readw(curReqheaderPtr+0x12);
						bool raw	 = (mem_readb(curReqheaderPtr+0x18)==1);
						if (mem_readb(curReqheaderPtr+0x0D)==0x00) // HSG
							mscdex->ReadSectors(subUnit,raw,start,len,buffer);
						else 
							mscdex->ReadSectorsMSF(subUnit,raw,start,len,buffer);
						break;
					  }
		case 0x83	:	// Seek - dont care :)
						break;
		case 0x84	: {	/* Play Audio Sectors */
						Bit32u start = mem_readd(curReqheaderPtr+0x0E);
						Bit32u len	 = mem_readd(curReqheaderPtr+0x12);
						if (mem_readb(curReqheaderPtr+0x0D)==0x00) // HSG
							mscdex->PlayAudioSector(subUnit,start,len);
						else // RED BOOK
							mscdex->PlayAudioMSF(subUnit,start,len);
						break;
					  }
		case 0x85	:	/* Stop Audio */
						mscdex->StopAudio(subUnit);
						break;
		case 0x88	:	/* Resume Audio */
						mscdex->ResumeAudio(subUnit);
						break;
		default		:	MSCDEX_LOG_ERROR("Unsupported Driver Request %02X",funcNr);
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
			if (mscdex->rootDriverHeaderSeg==0) return false;
			PhysPt check = PhysMake(SegValue(ss),reg_sp);
			if(mem_readw(check+6) == 0xDADA) {
				//MSCDEX sets word on stack to ADAD if it DADA on entry.
				mem_writew(check+6,0xADAD);
			}
			reg_al = 0xff;
			return true;
		} else {
			LOG(LOG_MISC,LOG_ERROR)("NETWORK REDIRECTOR USED!!!");
			reg_ax = 0x49;//NETWERK SOFTWARE NOT INSTALLED
			CALLBACK_SCF(true);
			return true;
		}
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
							reg_ax = MSCDEX_ERROR_UNKNOWN_DRIVE;
							CALLBACK_SCF(true);						
						}
						break;		
		case 0x1505: {	// read vtoc 
						Bit16u offset = 0, error = 0;
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
						Bit32u sector = ((Bit32u)reg_si << 16u) + (Bit32u)reg_di;
						if (mscdex->ReadSectors(reg_cx,sector,reg_dx,data)) {
							reg_ax = 0;
						} else {
							// possibly: MSCDEX_ERROR_DRIVE_NOT_READY if sector is beyond total length
							reg_ax = MSCDEX_ERROR_UNKNOWN_DRIVE;
							CALLBACK_SCF(true);
						}
					}
						break;
		case 0x1509:	// write sectors - not supported 
						reg_ax = MSCDEX_ERROR_INVALID_FUNCTION;
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
									reg_ax = MSCDEX_ERROR_INVALID_FUNCTION;
									CALLBACK_SCF(true);
								}
							} else {
								reg_ax = MSCDEX_ERROR_INVALID_FUNCTION;
								CALLBACK_SCF(true);
							}
						} else {
							reg_ax = MSCDEX_ERROR_UNKNOWN_DRIVE;
							CALLBACK_SCF(true);
						}
						break;
		case 0x150F: {	// Get directory entry
						Bit16u error;
						bool success = mscdex->GetDirectoryEntry(reg_cl,reg_ch&1,data,PhysMake(reg_si,reg_di),error);
						reg_ax = error;
						if (!success) CALLBACK_SCF(true);
					 }
						break;
		case 0x1510:	/* Device driver request */
						if (!mscdex->SendDriverRequest(reg_cx,data)) {
							reg_ax = MSCDEX_ERROR_UNKNOWN_DRIVE;
							CALLBACK_SCF(true);
						}
						break;
		default:		LOG(LOG_MISC,LOG_ERROR)("MSCDEX: Unknown call : %04X",reg_ax);
						reg_ax = MSCDEX_ERROR_INVALID_FUNCTION;
						CALLBACK_SCF(true);
						break;
	}
	return true;
}

class device_MSCDEX : public DOS_Device {
public:
	device_MSCDEX() { SetName("MSCD001"); }
	bool Read (Bit8u * /*data*/,Bit16u * /*size*/) { return false;}
	bool Write(const Bit8u * /*data*/,Bit16u * /*size*/) { 
		LOG(LOG_ALL,LOG_NORMAL)("Write to mscdex device");	
		return false;
	}
	bool Seek(Bit32u * /*pos*/,Bit32u /*type*/){return false;}
	bool Close(){return false;}
	Bit16u GetInformation(void){return 0xc880;}
	bool ReadFromControlChannel(PhysPt bufptr,Bit16u size,Bit16u * retcode);
	bool WriteToControlChannel(PhysPt bufptr,Bit16u size,Bit16u * retcode);
// private:
//  Bit8u cache;
};

bool device_MSCDEX::ReadFromControlChannel(PhysPt bufptr,Bit16u size,Bit16u * retcode) { 
	if (MSCDEX_IOCTL_Input(bufptr,0)==0) {
		*retcode=size;
		return true;
	}
	return false;
}

bool device_MSCDEX::WriteToControlChannel(PhysPt bufptr,Bit16u size,Bit16u * retcode) { 
	if (MSCDEX_IOCTL_Optput(bufptr,0)==0) {
		*retcode=size;
		return true;
	}
	return false;
}

int MSCDEX_AddDrive(char driveLetter, const char* physicalPath, Bit8u& subUnit)
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

void MSCDEX_ReplaceDrive(CDROM_Interface* cdrom, Bit8u subUnit)
{
	mscdex->ReplaceDrive(cdrom, subUnit);
}

Bit8u MSCDEX_GetSubUnit(char driveLetter)
{
	return mscdex->GetSubUnit(driveLetter-'A');
}

bool MSCDEX_GetVolumeName(Bit8u subUnit, char* name)
{
	return mscdex->GetVolumeName(subUnit,name);
}

bool MSCDEX_HasMediaChanged(Bit8u subUnit)
{
	static TMSF leadOut[MSCDEX_MAX_DRIVES];

	TMSF leadnew;
	Bit8u tr1,tr2;
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

		/* Register the mscdex device */
		DOS_Device * newdev = new device_MSCDEX();
		DOS_AddDevice(newdev);
		curReqheaderPtr = 0;
		/* Add Multiplexer */
		DOS_AddMultiplexHandler(MSCDEX_Handler);
		/* Create MSCDEX */
		mscdex = new CMscdex;
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
		Bit16u dnum=mscdex->GetNumDrives();
		WRITE_POD( &dnum, dnum);
		for (Bit8u drive_unit=0; drive_unit<dnum; drive_unit++) {
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
		Bit16u dnum;
		READ_POD( &dnum, dnum);
        if (mscdex->GetNumDrives()>dnum) {
            mscdex->numDrives=dnum;
            for (Bit16u i=dnum; i<mscdex->GetNumDrives(); i++) {
                delete mscdex->cdrom[i];
                mscdex->cdrom[i] = 0;
            }
        }
		for (Bit8u drive_unit=0; drive_unit<dnum; drive_unit++) {
			TMSF pos, start, end;
			Bit32u msf_time, play_len;
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
