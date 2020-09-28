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


#if defined (WIN32) && 0

// *****************************************************************
// Windows IOCTL functions (not suitable for 95/98/Me)
// *****************************************************************

#include <windows.h>
#include <io.h>

#if (defined (_MSC_VER)) || (defined __MINGW64_VERSION_MAJOR)
#include <winioctl.h>			// Ioctl stuff
//#include <ntddcdrm.h>			// Ioctl stuff
#else 
#include "ddk/ntddcdrm.h"		// Ioctl stuff
#endif

#include <mmsystem.h>

#include "cdrom.h"

// for a more sophisticated implementation of the mci cdda functionality
// see the SDL sources, which the mci_ functions are based on

/* General ioctl() CD-ROM command function */
bool CDROM_Interface_Ioctl::mci_CDioctl(UINT msg, DWORD flags, void *arg) {
	MCIERROR mci_error = mciSendCommand(mci_devid, msg, flags, (DWORD_PTR)arg);
	if (mci_error!=MMSYSERR_NOERROR) {
		char error[256];
		mciGetErrorString(mci_error, error, 256);
		LOG_MSG("mciSendCommand() error: %s", error);
		return true;
	}
	return false;
}

bool CDROM_Interface_Ioctl::mci_CDOpen(char drive) {
	MCI_OPEN_PARMS mci_open;
	MCI_SET_PARMS mci_set;
	char device[3];
	DWORD flags;

	/* Open the requested device */
	mci_open.lpstrDeviceType = (LPCSTR) MCI_DEVTYPE_CD_AUDIO;
	device[0] = drive;
	device[1] = ':';
	device[2] = '\0';
	mci_open.lpstrElementName = device;
	flags = (MCI_OPEN_TYPE|MCI_OPEN_TYPE_ID|MCI_OPEN_SHAREABLE|MCI_OPEN_ELEMENT);
	if (mci_CDioctl(MCI_OPEN, flags, &mci_open)) {
		flags &= ~MCI_OPEN_SHAREABLE;
		if (mci_CDioctl(MCI_OPEN, flags, &mci_open)) {
			return true;
		}
	}
	mci_devid = mci_open.wDeviceID;

	/* Set the minute-second-frame time format */
	mci_set.dwTimeFormat = MCI_FORMAT_MSF;
	mci_CDioctl(MCI_SET, MCI_SET_TIME_FORMAT, &mci_set);

	return false;
}

bool CDROM_Interface_Ioctl::mci_CDClose(void) {
	return mci_CDioctl(MCI_CLOSE, MCI_WAIT, NULL);
}

bool CDROM_Interface_Ioctl::mci_CDPlay(int start, int length) {
	DWORD flags = MCI_FROM | MCI_TO | MCI_NOTIFY;
	MCI_PLAY_PARMS mci_play;
	mci_play.dwCallback = 0;

	int m, s, f;
	FRAMES_TO_MSF(start, &m, &s, &f);
	mci_play.dwFrom = MCI_MAKE_MSF(m, s, f);

	FRAMES_TO_MSF(start+length, &m, &s, &f);
	mci_play.dwTo = MCI_MAKE_MSF(m, s, f);

	return mci_CDioctl(MCI_PLAY, flags, &mci_play);
}

bool CDROM_Interface_Ioctl::mci_CDPause(void) {
	return mci_CDioctl(MCI_PAUSE, MCI_WAIT, NULL);
}

bool CDROM_Interface_Ioctl::mci_CDResume(void) {
	return mci_CDioctl(MCI_RESUME, MCI_WAIT, NULL);
}

bool CDROM_Interface_Ioctl::mci_CDStop(void) {
	return mci_CDioctl(MCI_STOP, MCI_WAIT, NULL);
}

int CDROM_Interface_Ioctl::mci_CDStatus(void) {
	int status;
	MCI_STATUS_PARMS mci_status;

	DWORD flags = MCI_STATUS_ITEM | MCI_WAIT;
	mci_status.dwItem = MCI_STATUS_MODE;
	if (mci_CDioctl(MCI_STATUS, flags, &mci_status)) {
		status = -1;
	} else {
		switch (mci_status.dwReturn) {
			case MCI_MODE_NOT_READY:
			case MCI_MODE_OPEN:
				status = 0;
				break;
			case MCI_MODE_STOP:
				status = 1;
				break;
			case MCI_MODE_PLAY:
				status = 2;
				break;
			case MCI_MODE_PAUSE:
				status = 3;
				break;
			default:
				status = -1;
				break;
		}
	}

	return status;
}

bool CDROM_Interface_Ioctl::mci_CDPosition(int *position) {
	*position = 0;

	DWORD flags = MCI_STATUS_ITEM | MCI_WAIT;

	MCI_STATUS_PARMS mci_status;
	mci_status.dwItem = MCI_STATUS_MODE;
	if (mci_CDioctl(MCI_STATUS, flags, &mci_status)) return true;
	switch (mci_status.dwReturn) {
		case MCI_MODE_NOT_READY:
		case MCI_MODE_OPEN:
		case MCI_MODE_STOP:
			return true;	// not ready/undefined status
		case MCI_MODE_PLAY:
		case MCI_MODE_PAUSE:
			mci_status.dwItem = MCI_STATUS_POSITION;
			if (!mci_CDioctl(MCI_STATUS, flags, &mci_status)) {
				*position = MSF_TO_FRAMES(
					MCI_MSF_MINUTE(mci_status.dwReturn),
					MCI_MSF_SECOND(mci_status.dwReturn),
					MCI_MSF_FRAME(mci_status.dwReturn));
			}
			return false;	// no error, position read
		default:
			break;
	}
	return false;
}


CDROM_Interface_Ioctl::dxPlayer CDROM_Interface_Ioctl::player = {
	NULL, NULL, NULL, {0}, 0, 0, 0, false, false, false, {0} };

CDROM_Interface_Ioctl::CDROM_Interface_Ioctl(cdioctl_cdatype ioctl_cda) {
	pathname[0] = 0;
	hIOCTL = INVALID_HANDLE_VALUE;
	memset(&oldLeadOut,0,sizeof(oldLeadOut));
	cdioctl_cda_selected = ioctl_cda;
}

CDROM_Interface_Ioctl::~CDROM_Interface_Ioctl() {
	StopAudio();
	if (use_mciplay) mci_CDStop();
	Close();
	if (use_mciplay) mci_CDClose();
}

bool CDROM_Interface_Ioctl::GetUPC(unsigned char& attr, char* upc) {
	// FIXME : To Do
	return true;
}

bool CDROM_Interface_Ioctl::GetAudioTracks(int& stTrack, int& endTrack, TMSF& leadOut) {
	CDROM_TOC toc;
	DWORD	byteCount;
	BOOL	bStat = DeviceIoControl(hIOCTL,IOCTL_CDROM_READ_TOC, NULL, 0, 
									&toc, sizeof(toc), &byteCount,NULL);
	if (!bStat) return false;

	stTrack		= toc.FirstTrack;
	endTrack	= toc.LastTrack;
	leadOut.min = toc.TrackData[endTrack].Address[1];
	leadOut.sec	= toc.TrackData[endTrack].Address[2];
	leadOut.fr	= toc.TrackData[endTrack].Address[3];

	if ((use_mciplay || use_dxplay) && (!track_start_valid)) {
		Bits track_num = 0;
		// get track start address of all tracks
		for (Bits i=toc.FirstTrack; i<=toc.LastTrack+1; i++) {
			if (((toc.TrackData[i].Control&1)==0) || (i==toc.LastTrack+1)) {
				track_start[track_num] = MSF_TO_FRAMES(toc.TrackData[track_num].Address[1],toc.TrackData[track_num].Address[2],toc.TrackData[track_num].Address[3])-150;
				track_start[track_num] += 150;
				track_num++;
			}
		}
		track_start_first = 0;
		track_start_last = track_num-1;
		track_start_valid = true;
	}

	return true;
}

bool CDROM_Interface_Ioctl::GetAudioTrackInfo(int track, TMSF& start, unsigned char& attr) {
	CDROM_TOC toc;
	DWORD	byteCount;
	BOOL	bStat = DeviceIoControl(hIOCTL,IOCTL_CDROM_READ_TOC, NULL, 0, 
									&toc, sizeof(toc), &byteCount,NULL);
	if (!bStat) return false;
	
	attr		= (toc.TrackData[track-1].Control << 4) & 0xEF;
	start.min	= toc.TrackData[track-1].Address[1];
	start.sec	= toc.TrackData[track-1].Address[2];
	start.fr	= toc.TrackData[track-1].Address[3];
	return true;
}	

bool CDROM_Interface_Ioctl::GetAudioTracksAll(void) {
	if (track_start_valid) return true;

	CDROM_TOC toc;
	DWORD	byteCount;
	BOOL	bStat = DeviceIoControl(hIOCTL,IOCTL_CDROM_READ_TOC, NULL, 0, 
									&toc, sizeof(toc), &byteCount,NULL);
	if (!bStat) return false;

	Bits track_num = 0;
	// get track start address of all tracks
	for (Bits i=toc.FirstTrack; i<=toc.LastTrack+1; i++) {
		if (((toc.TrackData[i].Control&1)==0) || (i==toc.LastTrack+1)) {
			track_start[track_num] = MSF_TO_FRAMES(toc.TrackData[track_num].Address[1],toc.TrackData[track_num].Address[2],toc.TrackData[track_num].Address[3])-150;
			track_start[track_num] += 150;
			track_num++;
		}
	}
	track_start_first = 0;
	track_start_last = track_num-1;
	track_start_valid = true;
	return true;
}

bool CDROM_Interface_Ioctl::GetAudioSub(unsigned char& attr, unsigned char& track, unsigned char& index, TMSF& relPos, TMSF& absPos) {
	if (use_dxplay) {
		track = 1;
		FRAMES_TO_MSF(player.currFrame + 150, &absPos.min, &absPos.sec, &absPos.fr);
		FRAMES_TO_MSF(player.currFrame + 150, &relPos.min, &relPos.sec, &relPos.fr);

		if (GetAudioTracksAll()) {
			// get track number from current frame
			for (int i=track_start_first; i<=track_start_last; i++) {
				if ((player.currFrame + 150<track_start[i+1]) && (player.currFrame + 150>=track_start[i])) {
					// track found, calculate relative position
					track = i;
					FRAMES_TO_MSF(player.currFrame + 150-track_start[i],&relPos.min,&relPos.sec,&relPos.fr);
					break;
				}
			}
		}

		return true;
	}

	CDROM_SUB_Q_DATA_FORMAT insub;
	SUB_Q_CHANNEL_DATA sub;
	DWORD	byteCount;

	insub.Format = IOCTL_CDROM_CURRENT_POSITION;

	BOOL	bStat = DeviceIoControl(hIOCTL,IOCTL_CDROM_READ_Q_CHANNEL, &insub, sizeof(insub), 
									&sub, sizeof(sub), &byteCount,NULL);
	if (!bStat) return false;

	attr		= (sub.CurrentPosition.Control << 4) & 0xEF;
	track		= sub.CurrentPosition.TrackNumber;
	index		= sub.CurrentPosition.IndexNumber;
	relPos.min	= sub.CurrentPosition.TrackRelativeAddress[1];
	relPos.sec	= sub.CurrentPosition.TrackRelativeAddress[2];
	relPos.fr	= sub.CurrentPosition.TrackRelativeAddress[3];
	absPos.min	= sub.CurrentPosition.AbsoluteAddress[1];
	absPos.sec	= sub.CurrentPosition.AbsoluteAddress[2];
	absPos.fr	= sub.CurrentPosition.AbsoluteAddress[3];

	if (use_mciplay) {
		int cur_pos;
		if (!mci_CDPosition(&cur_pos)) {
			// absolute position read, try to calculate the track-relative position
			if (GetAudioTracksAll()) {
				for (int i=track_start_first; i<=track_start_last; i++) {
					if ((cur_pos<track_start[i+1]) && (cur_pos>=track_start[i])) {
						// track found, calculate relative position
						FRAMES_TO_MSF(cur_pos-track_start[i],&relPos.min,&relPos.sec,&relPos.fr);
						break;
					}
				}
			}
			FRAMES_TO_MSF(cur_pos,&absPos.min,&absPos.sec,&absPos.fr);
		}
	}

	return true;
}

bool CDROM_Interface_Ioctl::GetAudioStatus(bool& playing, bool& pause) {
	if (use_mciplay) {
		int status = mci_CDStatus();
		if (status<0) return false;
		playing	= (status==2);
		pause	= (status==3);
		return true;
	}
	if (use_dxplay) {
		playing = player.isPlaying;
		pause = player.isPaused;
		return true;
	}

	CDROM_SUB_Q_DATA_FORMAT insub;
	SUB_Q_CHANNEL_DATA sub;
	DWORD byteCount;

	insub.Format = IOCTL_CDROM_CURRENT_POSITION;

	BOOL	bStat = DeviceIoControl(hIOCTL,IOCTL_CDROM_READ_Q_CHANNEL, &insub, sizeof(insub), 
									&sub, sizeof(sub), &byteCount,NULL);
	if (!bStat) return false;

	playing = (sub.CurrentPosition.Header.AudioStatus == AUDIO_STATUS_IN_PROGRESS);
	pause	= (sub.CurrentPosition.Header.AudioStatus == AUDIO_STATUS_PAUSED);

	return true;
}

bool CDROM_Interface_Ioctl::GetMediaTrayStatus(bool& mediaPresent, bool& mediaChanged, bool& trayOpen) {
	// Seems not possible to get this values using ioctl...
	int		track1,track2;
	TMSF	leadOut;
	// If we can read, there's a media
	mediaPresent = GetAudioTracks(track1, track2, leadOut),
	trayOpen	 = !mediaPresent;
	mediaChanged = (oldLeadOut.min!=leadOut.min) || (oldLeadOut.sec!=leadOut.sec) || (oldLeadOut.fr!=leadOut.fr);
	if (mediaChanged) {
		Close();
		if (use_mciplay) mci_CDClose();
		// Open new medium
		Open();

		if (cdioctl_cda_selected == CDIOCTL_CDA_MCI) {
			// check this (what to do if cd is ejected):
			use_mciplay = false;
			if (!mci_CDOpen(pathname[4])) use_mciplay = true;
		}
		track_start_valid = false;
	}
	// Save old values
	oldLeadOut.min = leadOut.min;
	oldLeadOut.sec = leadOut.sec;
	oldLeadOut.fr  = leadOut.fr;
	// always success
	return true;
}

bool CDROM_Interface_Ioctl::PlayAudioSector	(unsigned long start,unsigned long len) {
	if (use_mciplay) {
		if (!mci_CDPlay(start+150, len))	return true;
		if (!mci_CDPlay(start+150, len-1))	return true;
		return false;
	}
	if (use_dxplay) {
		SDL_mutexP(player.mutex);
		player.cd = this;
		player.currFrame = start;
		player.targetFrame = start + len;
		player.isPlaying = true;
		player.isPaused = false;
		SDL_mutexV(player.mutex);
		return true;
	}

	CDROM_PLAY_AUDIO_MSF audio;
	DWORD	byteCount;
	// Start
	unsigned long addr	= start + 150;
	audio.StartingF = (UCHAR)(addr%75); addr/=75;
	audio.StartingS = (UCHAR)(addr%60); 
	audio.StartingM = (UCHAR)(addr/60);
	// End
	addr			= start + len + 150;
	audio.EndingF	= (UCHAR)(addr%75); addr/=75;
	audio.EndingS	= (UCHAR)(addr%60); 
	audio.EndingM	= (UCHAR)(addr/60);

	BOOL	bStat = DeviceIoControl(hIOCTL,IOCTL_CDROM_PLAY_AUDIO_MSF, &audio, sizeof(audio), 
									NULL, 0, &byteCount,NULL);
	return bStat>0;
}

bool CDROM_Interface_Ioctl::PauseAudio(bool resume) {
	if (use_mciplay) {
		if (resume) {
			if (!mci_CDResume()) return true;
		} else {
			if (!mci_CDPause()) return true;
		}
		return false;
	}
	if (use_dxplay) {
		player.isPaused = !resume;
		return true;
	}

	BOOL bStat; 
	DWORD byteCount;
	if (resume) bStat = DeviceIoControl(hIOCTL,IOCTL_CDROM_RESUME_AUDIO, NULL, 0, 
										NULL, 0, &byteCount,NULL);	
	else		bStat = DeviceIoControl(hIOCTL,IOCTL_CDROM_PAUSE_AUDIO, NULL, 0, 
										NULL, 0, &byteCount,NULL);
	return bStat>0;
}

bool CDROM_Interface_Ioctl::StopAudio(void) {
 	if (use_mciplay) {
		if (!mci_CDStop()) return true;
		return false;
	}
	if (use_dxplay) {
		player.isPlaying = false;
		player.isPaused = false;
		return true;
	}

	BOOL bStat; 
	DWORD byteCount;
	bStat = DeviceIoControl(hIOCTL,IOCTL_CDROM_STOP_AUDIO, NULL, 0, 
							NULL, 0, &byteCount,NULL);	
	return bStat>0;
}

void CDROM_Interface_Ioctl::ChannelControl(TCtrl ctrl)
{
	player.ctrlUsed = (ctrl.out[0]!=0 || ctrl.out[1]!=1 || ctrl.vol[0]<0xfe || ctrl.vol[1]<0xfe);
	player.ctrlData = ctrl;
}

bool CDROM_Interface_Ioctl::LoadUnloadMedia(bool unload) {
	BOOL bStat; 
	DWORD byteCount;
	if (unload) bStat = DeviceIoControl(hIOCTL,IOCTL_STORAGE_EJECT_MEDIA, NULL, 0, 
										NULL, 0, &byteCount,NULL);	
	else		bStat = DeviceIoControl(hIOCTL,IOCTL_STORAGE_LOAD_MEDIA, NULL, 0, 
										NULL, 0, &byteCount,NULL);	
	track_start_valid = false;
	return bStat>0;
}

bool CDROM_Interface_Ioctl::ReadSector(uint8_t *buffer, bool raw, unsigned long sector) {
	BOOL  bStat;
	DWORD byteCount = 0;

	Bitu	buflen	= raw ? RAW_SECTOR_SIZE : COOKED_SECTOR_SIZE;

	if (!raw) {
		// Cooked
		int	  success = 0;
		DWORD newPos  = SetFilePointer(hIOCTL, sector*COOKED_SECTOR_SIZE, 0, FILE_BEGIN);
		if (newPos != 0xFFFFFFFF) success = ReadFile(hIOCTL, buffer, buflen, &byteCount, NULL);
		bStat = (success!=0);
	} else {
		// Raw
		RAW_READ_INFO in;
		in.DiskOffset.LowPart	= sector*COOKED_SECTOR_SIZE;
		in.DiskOffset.HighPart	= 0;
		in.SectorCount			= 1;
		in.TrackMode			= CDDA;		
		bStat = DeviceIoControl(hIOCTL,IOCTL_CDROM_RAW_READ, &in, sizeof(in), 
								buffer, buflen, &byteCount,NULL);
	}

	return (byteCount==buflen) && (bStat>0);
}

bool CDROM_Interface_Ioctl::ReadSectors(PhysPt buffer, bool raw, unsigned long sector, unsigned long num) {
	BOOL  bStat;
	DWORD byteCount = 0;

	Bitu	buflen	= raw ? num*RAW_SECTOR_SIZE : num*COOKED_SECTOR_SIZE;
	uint8_t*	bufdata = new uint8_t[buflen];

	if (!raw) {
		// Cooked
		int	  success = 0;
		DWORD newPos  = SetFilePointer(hIOCTL, sector*COOKED_SECTOR_SIZE, 0, FILE_BEGIN);
		if (newPos != 0xFFFFFFFF) success = ReadFile(hIOCTL, bufdata, buflen, &byteCount, NULL);
		bStat = (success!=0);
	} else {
		// Raw
		RAW_READ_INFO in;
		in.DiskOffset.LowPart	= sector*COOKED_SECTOR_SIZE;
		in.DiskOffset.HighPart	= 0;
		in.SectorCount			= num;
		in.TrackMode			= CDDA;		
		bStat = DeviceIoControl(hIOCTL,IOCTL_CDROM_RAW_READ, &in, sizeof(in), 
								bufdata, buflen, &byteCount,NULL);
	}

	MEM_BlockWrite(buffer,bufdata,buflen);
	delete[] bufdata;

	return (byteCount==buflen) && (bStat>0);
}

void CDROM_Interface_Ioctl::dx_CDAudioCallBack(Bitu len) {
	len *= 4;       // 16 bit, stereo
	if (!len) return;
	if (!player.isPlaying || player.isPaused) {
		player.channel->AddSilence();
		return;
	}
	SDL_mutexP(player.mutex);
	while (player.bufLen < (Bits)len) {
		bool success;
		if (player.targetFrame > player.currFrame)
			success = player.cd->ReadSector(&player.buffer[player.bufLen], true, player.currFrame);
		else success = false;
		
		if (success) {
			player.currFrame++;
			player.bufLen += RAW_SECTOR_SIZE;
		} else {
			memset(&player.buffer[player.bufLen], 0, len - player.bufLen);
			player.bufLen = len;
			player.isPlaying = false;
		}
	}
	SDL_mutexV(player.mutex);
	if (player.ctrlUsed) {
		Bit16s sample0,sample1;
		Bit16s * samples=(Bit16s *)&player.buffer;
		for (Bitu pos=0;pos<len/4;pos++) {
			sample0=samples[pos*2+player.ctrlData.out[0]];
			sample1=samples[pos*2+player.ctrlData.out[1]];
			samples[pos*2+0]=(Bit16s)(sample0*player.ctrlData.vol[0]/255.0);
			samples[pos*2+1]=(Bit16s)(sample1*player.ctrlData.vol[1]/255.0);
		}
	}
	player.channel->AddSamples_s16(len/4,(Bit16s *)player.buffer);
	memmove(player.buffer, &player.buffer[len], player.bufLen - len);
	player.bufLen -= len;
}

bool CDROM_Interface_Ioctl::SetDevice(char* path, int forceCD) {
	mci_devid = 0;
	use_mciplay = false;
	use_dxplay = false;
	track_start_valid = false;
	if (GetDriveType(path)==DRIVE_CDROM) {
		char letter [3] = { 0, ':', 0 };
		letter[0] = path[0];
		strcpy(pathname,"\\\\.\\");
		strcat(pathname,letter);
		if (Open()) {
			if (cdioctl_cda_selected == CDIOCTL_CDA_MCI) {
				// check if MCI-interface can be used for cd audio
				if (!mci_CDOpen(path[0])) use_mciplay = true;
			}
			if (!use_mciplay) {
				if (cdioctl_cda_selected == CDIOCTL_CDA_DX) {
					// use direct sector access for cd audio routines
					player.mutex = SDL_CreateMutex();
					if (!player.channel) {
						player.channel = MIXER_AddChannel(&dx_CDAudioCallBack, 44100, "CDAUDIO");
					}
					player.channel->Enable(true);
					use_dxplay = true;
				}
			}
			return true;
		};
	}
	return false;
}

bool CDROM_Interface_Ioctl::Open(void) {
	hIOCTL	= CreateFile(pathname,			// drive to open
						GENERIC_READ,		// read access
						FILE_SHARE_READ |	// share mode
						FILE_SHARE_WRITE, 
						NULL,				// default security attributes
						OPEN_EXISTING,		// disposition
						0,					// file attributes
						NULL);				// do not copy file attributes
	return (hIOCTL!=INVALID_HANDLE_VALUE);
}

void CDROM_Interface_Ioctl::Close(void) {
	CloseHandle(hIOCTL);
}

bool CDROM_Interface_Ioctl::ReadSectorsHost(void *buffer, bool raw, unsigned long sector, unsigned long num)
{
	return false;/*TODO*/
};

#endif
