/*
 *  Copyright (C) 2002-2010  The DOSBox Team
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

/* $Id: drives.cpp,v 1.15 2009-05-27 09:15:41 qbix79 Exp $ */

#include "dosbox.h"
#include "dos_system.h"
#include "drives.h"
#include "mapper.h"
#include "support.h"

bool WildFileCmp(const char * file, const char * wild) 
{
	char file_name[9];
	char file_ext[4];
	char wild_name[9];
	char wild_ext[4];
	const char * find_ext;
	Bitu r;

	strcpy(file_name,"        ");
	strcpy(file_ext,"   ");
	strcpy(wild_name,"        ");
	strcpy(wild_ext,"   ");

	find_ext=strrchr(file,'.');
	if (find_ext) {
		Bitu size=(Bitu)(find_ext-file);
		if (size>8) size=8;
		memcpy(file_name,file,size);
		find_ext++;
		memcpy(file_ext,find_ext,(strlen(find_ext)>3) ? 3 : strlen(find_ext)); 
	} else {
		memcpy(file_name,file,(strlen(file) > 8) ? 8 : strlen(file));
	}
	upcase(file_name);upcase(file_ext);
	find_ext=strrchr(wild,'.');
	if (find_ext) {
		Bitu size=(Bitu)(find_ext-wild);
		if (size>8) size=8;
		memcpy(wild_name,wild,size);
		find_ext++;
		memcpy(wild_ext,find_ext,(strlen(find_ext)>3) ? 3 : strlen(find_ext));
	} else {
		memcpy(wild_name,wild,(strlen(wild) > 8) ? 8 : strlen(wild));
	}
	upcase(wild_name);upcase(wild_ext);
	/* Names are right do some checking */
	r=0;
	while (r<8) {
		if (wild_name[r]=='*') goto checkext;
		if (wild_name[r]!='?' && wild_name[r]!=file_name[r]) return false;
		r++;
	}
checkext:
    r=0;
	while (r<3) {
		if (wild_ext[r]=='*') return true;
		if (wild_ext[r]!='?' && wild_ext[r]!=file_ext[r]) return false;
		r++;
	}
	return true;
}

void Set_Label(char const * const input, char * const output, bool cdrom) {
	Bitu togo     = 8;
	Bitu vnamePos = 0;
	Bitu labelPos = 0;
	bool point    = false;

	//spacepadding the filenamepart to include spaces after the terminating zero is more closely to the specs. (not doing this now)
	// HELLO\0' '' '

	while (togo > 0) {
		if (input[vnamePos]==0) break;
		if (!point && (input[vnamePos]=='.')) {	togo=4; point=true; }

		//another mscdex quirk. Label is not always uppercase. (Daggerfall)
		output[labelPos] = (cdrom?input[vnamePos]:toupper(input[vnamePos]));

		labelPos++; vnamePos++;
		togo--;
		if ((togo==0) && !point) {
			if (input[vnamePos]=='.') vnamePos++;
			output[labelPos]='.'; labelPos++; point=true; togo=3;
		}
	};
	output[labelPos]=0;

	//Remove trailing dot. except when on cdrom and filename is exactly 8 (9 including the dot) letters. MSCDEX feature/bug (fifa96 cdrom detection)
	if((labelPos > 0) && (output[labelPos-1] == '.') && !(cdrom && labelPos ==9))
		output[labelPos-1] = 0;
}



DOS_Drive::DOS_Drive() {
	curdir[0]=0;
	info[0]=0;
}

char * DOS_Drive::GetInfo(void) {
	return info;
}

// static members variables
int DriveManager::currentDrive;
DriveManager::DriveInfo DriveManager::driveInfos[26];

void DriveManager::AppendDisk(int drive, DOS_Drive* disk) {
	driveInfos[drive].disks.push_back(disk);
}

void DriveManager::InitializeDrive(int drive) {
	currentDrive = drive;
	DriveInfo& driveInfo = driveInfos[currentDrive];
	if (driveInfo.disks.size() > 0) {
		driveInfo.currentDisk = 0;
		DOS_Drive* disk = driveInfo.disks[driveInfo.currentDisk];
		Drives[currentDrive] = disk;
		disk->Activate();
	}
}

/*
void DriveManager::CycleDrive(bool pressed) {
	if (!pressed) return;
		
	// do one round through all drives or stop at the next drive with multiple disks
	int oldDrive = currentDrive;
	do {
		currentDrive = (currentDrive + 1) % DOS_DRIVES;
		int numDisks = driveInfos[currentDrive].disks.size();
		if (numDisks > 1) break;
	} while (currentDrive != oldDrive);
}

void DriveManager::CycleDisk(bool pressed) {
	if (!pressed) return;
	
	int numDisks = driveInfos[currentDrive].disks.size();
	if (numDisks > 1) {
		// cycle disk
		int currentDisk = driveInfos[currentDrive].currentDisk;
		DOS_Drive* oldDisk = driveInfos[currentDrive].disks[currentDisk];
		currentDisk = (currentDisk + 1) % numDisks;		
		DOS_Drive* newDisk = driveInfos[currentDrive].disks[currentDisk];
		driveInfos[currentDrive].currentDisk = currentDisk;
		
		// copy working directory, acquire system resources and finally switch to next drive		
		strcpy(newDisk->curdir, oldDisk->curdir);
		newDisk->Activate();
		Drives[currentDrive] = newDisk;
	}
}
*/

void DriveManager::CycleAllDisks(void) {
	for (int idrive=0; idrive<DOS_DRIVES; idrive++) {
		int numDisks = (int)driveInfos[idrive].disks.size();
		if (numDisks > 1) {
			// cycle disk
			int currentDisk = driveInfos[idrive].currentDisk;
			DOS_Drive* oldDisk = driveInfos[idrive].disks[currentDisk];
			currentDisk = (currentDisk + 1) % numDisks;		
			DOS_Drive* newDisk = driveInfos[idrive].disks[currentDisk];
			driveInfos[idrive].currentDisk = currentDisk;
			
			// copy working directory, acquire system resources and finally switch to next drive		
			strcpy(newDisk->curdir, oldDisk->curdir);
			newDisk->Activate();
			Drives[idrive] = newDisk;
			LOG_MSG("Drive %c: disk %d of %d now active", 'A'+idrive, currentDisk+1, numDisks);
		}
	}
}

int DriveManager::UnmountDrive(int drive) {
	int result = 0;
	// unmanaged drive
	if (driveInfos[drive].disks.size() == 0) {
		result = Drives[drive]->UnMount();
	} else {
		// managed drive
		int currentDisk = driveInfos[drive].currentDisk;
		result = driveInfos[drive].disks[currentDisk]->UnMount();
		// only delete on success, current disk set to NULL because of UnMount
		if (result == 0) {
			driveInfos[drive].disks[currentDisk] = NULL;
			for (int i = 0; i < (int)driveInfos[drive].disks.size(); i++) {
				delete driveInfos[drive].disks[i];
			}
			driveInfos[drive].disks.clear();
		}
	}
	
	return result;
}

void DriveManager::Init(Section* /* sec */) {
	
	// setup driveInfos structure
	currentDrive = 0;
	for(int i = 0; i < DOS_DRIVES; i++) {
		driveInfos[i].currentDisk = 0;
	}
	
//	MAPPER_AddHandler(&CycleDisk, MK_f3, MMOD1, "cycledisk", "Cycle Disk");
//	MAPPER_AddHandler(&CycleDrive, MK_f3, MMOD2, "cycledrive", "Cycle Drv");
}

void DRIVES_Init(Section* sec) {
	DriveManager::Init(sec);
}
