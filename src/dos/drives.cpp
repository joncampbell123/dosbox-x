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


#include "dosbox.h"
#include "dos_system.h"
#include "drives.h"
#include "setup.h"
#include "mapper.h"
#include "support.h"
#include "control.h"

bool wild_match(char *haystack, char *needle) {
	size_t max, i;
    for (; *needle != '\0'; ++needle) {
        switch (*needle) {
        case '?':
            if (*haystack == '\0')
                return false;
            ++haystack;
            break;
        case '*':
            if (needle[1] == '\0')
                return true;
            max = strlen(haystack);
            for (i = 0; i < max; i++)
                if (wild_match(haystack + i, needle + 1))
                    return true;
            return false;
        default:
            if (toupper(*haystack) != *needle)
                return false;
            ++haystack;
        }
    }
    return *haystack == '\0';
}

bool WildFileCmp(const char * file, const char * wild) 
{
	char file_name[9];
	char file_ext[4];
    char wild_name[10];
    char wild_ext[5];
	const char * find_ext;
	Bitu r;

    for (r=0;r<9;r++) {
        file_name[r]=0;
        wild_name[r]=0;
    }
    wild_name[9]=0;
    for (r=0;r<4;r++) {
        file_ext[r]=0;
        wild_ext[r]=0;
    }
    wild_ext[4]=0;

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
		if (size>9) size=9;
		memcpy(wild_name,wild,size);
		find_ext++;
		memcpy(wild_ext,find_ext,(strlen(find_ext)>4) ? 4 : strlen(find_ext));
	} else {
		memcpy(wild_name,wild,(strlen(wild) > 9) ? 9 : strlen(wild));
	}
	upcase(wild_name);upcase(wild_ext);
	/* Names are right do some checking */
	if (uselfn&&strchr(wild_name, '*')) {
		if (strchr(wild,'.')) {
			if (!wild_match(file_name, wild_name)) return false;
			goto checkext;
		} else
			return wild_match((char *)file, wild_name);
	} else {
		r=0;
		while (r<8) {
			if (wild_name[r]=='*') goto checkext;
			if (wild_name[r]!='?' && wild_name[r]!=file_name[r]) return false;
			r++;
		}
		if (wild_name[r]&&wild_name[r]!='*') return false;
	}
checkext:
	if (uselfn&&strchr(wild_ext, '*'))
		return wild_match(file_ext, wild_ext);
	else {
		r=0;
		while (r<3) {
			if (wild_ext[r]=='*') return true;
			if (wild_ext[r]!='?' && wild_ext[r]!=file_ext[r]) return false;
			r++;
		}
		if (wild_ext[r]&&wild_ext[r]!='*') return false;
		return true;
	}
}

bool LWildFileCmp(const char * file, const char * wild)
{
    if (!uselfn||*file == 0) return false;
    char file_name[256];
    char file_ext[256];
    char wild_name[256];
    char wild_ext[256];
    const char * find_ext;
    Bitu r;

    for (r=0;r<256;r++) {
      file_name[r]=0;
      wild_name[r]=0;
    }
    for (r=0;r<256;r++) {
      file_ext[r]=0;
      wild_ext[r]=0;
    }

    Bitu size,elen;
    find_ext=strrchr(file,'.');
    if (find_ext) {
            size=(Bitu)(find_ext-file);
            if (size>255) size=255;
            memcpy(file_name,file,size);
            find_ext++;
            elen=strlen(find_ext);
            memcpy(file_ext,find_ext,(strlen(find_ext)>255) ? 255 : strlen(find_ext));
    } else {
            size=strlen(file);
            elen=0;
            memcpy(file_name,file,(strlen(file) > 255) ? 255 : strlen(file));
    }
    upcase(file_name);upcase(file_ext);
    char nwild[LFN_NAMELENGTH+2];
    strcpy(nwild,wild);
    if (strrchr(nwild,'*')&&strrchr(nwild,'.')==NULL) strcat(nwild,".*");
    find_ext=strrchr(nwild,'.');
    if (find_ext) {
            Bitu size=(Bitu)(find_ext-nwild);
            if (size>255) size=255;
            memcpy(wild_name,nwild,size);
            find_ext++;
            memcpy(wild_ext,find_ext,(strlen(find_ext)>255) ? 255 : strlen(find_ext));
    } else {
            memcpy(wild_name,nwild,(strlen(nwild) > 255) ? 255 : strlen(nwild));
    }
    upcase(wild_name);upcase(wild_ext);
    /* Names are right do some checking */
	if (strchr(wild_name, '*')) {
		if (strchr(wild,'.')) {
			if (!wild_match(file_name, wild_name)) return false;
			goto checkext;
		} else
			return wild_match((char *)file, wild_name);
	} else {
		r=0;
		while (r<size) {
				if (wild_name[r]=='*') goto checkext;
				if (wild_name[r]!='?' && wild_name[r]!=file_name[r]) return false;
				r++;
		}
		if (wild_name[r]&&wild_name[r]!='*') return false;
	}
checkext:
	if (strchr(wild_ext, '*'))
		return wild_match(file_ext, wild_ext);
	else {
		r=0;
		while (r<elen) {
				if (wild_ext[r]=='*') return true;
				if (wild_ext[r]!='?' && wild_ext[r]!=file_ext[r]) return false;
				r++;
		}
		if (wild_ext[r]&&wild_ext[r]!='*') return false;
		return true;
	}
}

void Set_Label(char const * const input, char * const output, bool cdrom) {
    /* I don't know what MSCDEX.EXE does but don't put dots in the 11-char volume label for non-CD-ROM drives */
    if (!cdrom) {
        Bitu togo     = 11;
        Bitu vnamePos = 0;
        Bitu labelPos = 0;

        while (togo > 0) {
            if (input[vnamePos]==0) break;
            //Another mscdex quirk. Label is not always uppercase. (Daggerfall)
            output[labelPos] = toupper(input[vnamePos]);
            labelPos++;
            vnamePos++;
            togo--;
        }
        output[labelPos] = 0;
        return;
    }

	Bitu togo     = 8;
	Bitu vnamePos = 0;
	Bitu labelPos = 0;
	bool point    = false;

	//spacepadding the filenamepart to include spaces after the terminating zero is more closely to the specs. (not doing this now)
	// HELLO\0' '' '

	while (togo > 0) {
		if (input[vnamePos]==0) break;
		if (!point && (input[vnamePos]=='.')) {	togo=4; point=true; }

		output[labelPos] = input[vnamePos];

		labelPos++; vnamePos++;
		togo--;
		if ((togo==0) && !point) {
			if (input[vnamePos]=='.') vnamePos++;
			output[labelPos]='.'; labelPos++; point=true; togo=3;
		}
	}
	output[labelPos]=0;

	//Remove trailing dot. except when on cdrom and filename is exactly 8 (9 including the dot) letters. MSCDEX feature/bug (fifa96 cdrom detection)
	if((labelPos > 0) && (output[labelPos-1] == '.') && !(cdrom && labelPos ==9))
		output[labelPos-1] = 0;
}



DOS_Drive::DOS_Drive() {
    nocachedir=false;
    readonly=false;
	curdir[0]=0;
	info[0]=0;
}

const char * DOS_Drive::GetInfo(void) {
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
		if (driveInfo.disks.size() > 1) disk->Activate();
        disk->UpdateDPB(currentDrive);
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

void DriveManager::CycleDisks(int drive, bool notify) {
	unsigned int numDisks = (unsigned int)driveInfos[drive].disks.size();
	if (numDisks > 1) {
		// cycle disk
		unsigned int currentDisk = (unsigned int)driveInfos[drive].currentDisk;
        const DOS_Drive* oldDisk = driveInfos[drive].disks[(unsigned int)currentDisk];
		currentDisk = ((unsigned int)currentDisk + 1u) % (unsigned int)numDisks;
		DOS_Drive* newDisk = driveInfos[drive].disks[currentDisk];
		driveInfos[drive].currentDisk = currentDisk;
		
		// copy working directory, acquire system resources and finally switch to next drive		
		strcpy(newDisk->curdir, oldDisk->curdir);
		newDisk->Activate();
        newDisk->UpdateDPB(currentDrive);
		Drives[drive] = newDisk;
		if (notify) LOG_MSG("Drive %c: disk %d of %d now active", 'A'+drive, currentDisk+1, numDisks);
	}
}

void DriveManager::CycleAllDisks(void) {
	for (int idrive=0; idrive<2; idrive++) CycleDisks(idrive, true); /* Cycle all DISKS meaning A: and B: */
}

void DriveManager::CycleAllCDs(void) {
	for (unsigned int idrive=2; idrive<DOS_DRIVES; idrive++) { /* Cycle all CDs in C: D: ... Z: */
		unsigned int numDisks = (unsigned int)driveInfos[idrive].disks.size();
		if (numDisks > 1) {
			// cycle disk
			unsigned int currentDisk = driveInfos[idrive].currentDisk;
            const DOS_Drive* oldDisk = driveInfos[idrive].disks[currentDisk];
            if (dynamic_cast<const isoDrive*>(oldDisk) == NULL) continue;
			currentDisk = ((unsigned int)currentDisk + 1u) % (unsigned int)numDisks;		
			DOS_Drive* newDisk = driveInfos[idrive].disks[currentDisk];
			driveInfos[idrive].currentDisk = currentDisk;
			
			// copy working directory, acquire system resources and finally switch to next drive		
			strcpy(newDisk->curdir, oldDisk->curdir);
			newDisk->Activate();
            newDisk->UpdateDPB(currentDrive);
            Drives[idrive] = newDisk;
			LOG_MSG("Drive %c: disk %d of %d now active", 'A'+idrive, currentDisk+1, numDisks);
		}
	}
}

int DriveManager::UnmountDrive(int drive) {
	int result = 0;
	// unmanaged drive
	if (driveInfos[drive].disks.size() == 0) {
		result = (int)Drives[drive]->UnMount();
	} else {
		// managed drive
		unsigned int currentDisk = driveInfos[drive].currentDisk;
		result = (int)driveInfos[drive].disks[currentDisk]->UnMount();
		// only delete on success, current disk set to NULL because of UnMount
		if (result == 0) {
			driveInfos[drive].disks[currentDisk] = NULL;
			for (unsigned int i = 0; i < (unsigned int)driveInfos[drive].disks.size(); i++) {
				delete driveInfos[drive].disks[i];
			}
			driveInfos[drive].disks.clear();
		}
	}
	
	return result;
}

char swappos[10];
char * DriveManager::GetDrivePosition(int drive) {
    sprintf(swappos, "%d / %d", driveInfos[drive].currentDisk+1, (int)driveInfos[drive].disks.size());
    return swappos;
}

bool drivemanager_init = false;
bool int13_extensions_enable = true;

void DriveManager::Init(Section* s) {
    const Section_prop* section = static_cast<Section_prop*>(s);

	drivemanager_init = true;

	int13_extensions_enable = section->Get_bool("int 13 extensions");
	
	// setup driveInfos structure
	currentDrive = 0;
	for(int i = 0; i < DOS_DRIVES; i++) {
		driveInfos[i].currentDisk = 0;
	}
	
//	MAPPER_AddHandler(&CycleDisk, MK_f3, MMOD1, "cycledisk", "Cycle Disk");
//	MAPPER_AddHandler(&CycleDrive, MK_f3, MMOD2, "cycledrive", "Cycle Drv");
}

void DRIVES_Startup(Section *s) {
    (void)s;//UNUSED
	if (!drivemanager_init) {
		LOG(LOG_MISC,LOG_DEBUG)("Initializing drive system");
		DriveManager::Init(control->GetSection("dos"));
	}
}

void DRIVES_Init() {
	LOG(LOG_MISC,LOG_DEBUG)("Initializing OOS drives");

	// TODO: DOS kernel exit, reset, guest booting handler
}

char * DOS_Drive::GetBaseDir(void) {
	return info + 16;
}

// save state support
void DOS_Drive::SaveState( std::ostream& stream )
{
	// - pure data
	WRITE_POD( &nocachedir, nocachedir );
	WRITE_POD( &readonly, readonly );
	WRITE_POD( &curdir, curdir );
	WRITE_POD( &info, info );
}

void DOS_Drive::LoadState( std::istream& stream )
{
	// - pure data
	READ_POD( &nocachedir, nocachedir );
	READ_POD( &readonly, readonly );
	READ_POD( &curdir, curdir );
	READ_POD( &info, info );
}

void DriveManager::SaveState( std::ostream& stream )
{
	// - pure data
	WRITE_POD( &currentDrive, currentDrive );
}

void DriveManager::LoadState( std::istream& stream )
{
	// - pure data
	READ_POD( &currentDrive, currentDrive );
}

void POD_Save_DOS_DriveManager( std::ostream& stream )
{
	DriveManager::SaveState(stream);
}

void POD_Load_DOS_DriveManager( std::istream& stream )
{
	DriveManager::LoadState(stream);
}
