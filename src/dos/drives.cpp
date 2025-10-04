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


#include "dosbox.h"
#include "dos_system.h"
#include "bios_disk.h"
#include "drives.h"
#include "setup.h"
#include "mapper.h"
#include "support.h"
#include "control.h"
#include "ide.h"

char *DBCS_upcase(char *);

bool wildmount = false;

bool wild_match(const char *haystack, char *needle) {
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
	if (!file||!wild) return false;
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
			return wild_match(file, wild_name);
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
    if ((!uselfn&&!wildmount)||!file||!wild||(*file&&!*wild)||strlen(wild)>LFN_NAMELENGTH) return false;
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
            if (wild_match(file, nwild)) return true;
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
			return wild_match(file, wild_name);
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

host_cnv_char_t *CodePageGuestToHost(const char *s);
char *CodePageHostToGuest(const host_cnv_char_t *s);
int get_expanded_files(const std::string &path, std::vector<std::string> &paths, bool readonly) {
    std::vector<std::string> files, names;
    if (!path.size()) return 0;
    char full[CROSS_LEN], pdir[DOS_PATHLENGTH], pattern[DOS_PATHLENGTH], *r;
    strcpy(full, path.c_str());
    r=strrchr_dbcs(full, CROSS_FILESPLIT);
    if (r!=NULL) {
        *r=0;
        strcpy(pdir, full);
        strcpy(pattern, r+1);
        *r=CROSS_FILESPLIT;
    } else {
        strcpy(pdir, "");
        strcpy(pattern, full);
    }
    if (!strchr(pattern, '*')&&!strchr(pattern, '?')) return 0;

#if defined(WIN32)
    HANDLE hFind;
    WIN32_FIND_DATA fd;
    WIN32_FIND_DATAW fdw;
    host_cnv_char_t *host_name = CodePageGuestToHost((std::string(pdir)+"\\*.*").c_str());
    if (host_name != NULL) hFind = FindFirstFileW(host_name, &fdw);
    else hFind = FindFirstFile((std::string(pdir)+"\\*.*").c_str(), &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            const char* temp_name = NULL;
            if (host_name != NULL) temp_name = CodePageHostToGuest(fdw.cFileName);
            if (!((host_name != NULL ? fdw.dwFileAttributes : fd.dwFileAttributes) & FILE_ATTRIBUTE_DIRECTORY)) {
                if (host_name == NULL)
                    names.emplace_back(fd.cFileName);
                else if (temp_name != NULL)
                    names.emplace_back(temp_name);
            }
        } while(host_name != NULL ? FindNextFileW(hFind, &fdw) : FindNextFile(hFind, &fd));
        FindClose(hFind);
    }
#else
    std::string homedir(pdir);
    Cross::ResolveHomedir(homedir);
    strcpy(pdir, homedir.c_str());
    struct dirent *dir;
    host_cnv_char_t *host_name = CodePageGuestToHost(pdir);
    DIR *d = opendir(host_name != NULL ? host_name : pdir);
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            host_cnv_char_t *temp_name = CodePageHostToGuest(dir->d_name);
#if defined(HAIKU)
            struct stat path_stat;
            stat(dir->d_name, &path_stat);
            bool is_regular_file = S_ISREG(path_stat.st_mode);
#else
            bool is_regular_file = (dir->d_type == DT_REG);
#endif
            if (is_regular_file)
                names.push_back(temp_name!=NULL?temp_name:dir->d_name);
        }
        closedir(d);
    }
#endif
	wildmount = true;
	for (std::string name: names) {
		if (LWildFileCmp(name.c_str(), pattern))
			files.push_back((readonly ? ":":"") + std::string(pdir) + std::string(1, CROSS_FILESPLIT) + name);
	}
	wildmount = false;

	if (files.size()) {
		sort(files.begin(), files.end());
		paths.insert(paths.end(), files.begin(), files.end());
		return files.size();
	} else {
		return 0;
	}
}

void Set_Label(char const * const input, char * const output, bool cdrom) {
    uint8_t togo = 11;
    uint8_t vnamePos = 0;
    uint8_t labelPos = 0;
    char upcasebuf[12];
    bool str_end = false; // True if end of string is detected
    strncpy(upcasebuf, input, 11);
    //DBCS_upcase(upcasebuf);  /* Another mscdex quirk. Label is not always uppercase. (Daggerfall) */ 

    while (togo > 0) {
        if(upcasebuf[vnamePos] == 0) str_end = true;
        else if(cdrom && vnamePos == 8 && !str_end && upcasebuf[vnamePos] != '.') {
            output[labelPos] = '.'; // add a dot between 8th and 9th character (Descent 2 installer needs this)
            labelPos++;
        }
        output[labelPos] = !str_end ? upcasebuf[vnamePos] : 0x0; // Pad empty characters with 0x00
        labelPos++;
        vnamePos++;
        togo--;
    }
    output[labelPos] = 0;
    return;
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
extern int32_t swapPosition;
extern int swapInDisksSpecificDrive;
extern bool dos_kernel_disabled;
int DriveManager::currentDrive;
DriveManager::DriveInfo DriveManager::driveInfos[26];

void DriveManager::AppendDisk(int drive, DOS_Drive* disk) {
	driveInfos[drive].disks.push_back(disk);
}

void DriveManager::ChangeDisk(int drive, DOS_Drive* disk) {
    DriveInfo& driveInfo = driveInfos[drive];
    if (Drives[drive]==NULL||disk==NULL||!driveInfo.disks.size()) return;
    isoDrive *cdrom = dynamic_cast<isoDrive*>(Drives[drive]);
    signed char index=-1;
    bool slave=false;
    if (cdrom) IDE_CDROM_Detach_Ret(index,slave,drive);
    strcpy(disk->curdir,driveInfo.disks[driveInfo.currentDisk]->curdir);
    disk->Activate();
    if (!dos_kernel_disabled) disk->UpdateDPB(currentDrive);
    else if (cdrom) cdrom->loadImage();
    driveInfo.disks[driveInfo.currentDisk] = disk;
    fatDrive *old = dynamic_cast<fatDrive*>(Drives[drive]);
    Drives[drive] = disk;
    if (cdrom && index>-1) IDE_CDROM_Attach(index,slave,drive);
    Drives[drive]->EmptyCache();
    Drives[drive]->MediaChange();
    if (cdrom && !dos_kernel_disabled) {
        IDE_CDROM_Detach_Ret(index,slave,drive);
        if (index>-1) IDE_CDROM_Attach(index,slave,drive);
    }
    fatDrive *fdp = dynamic_cast<fatDrive*>(Drives[drive]);
    if (drive<2 && fdp && fdp->loadedDisk) {
        if (imageDiskList[drive]) {
            imageDiskList[drive]->Release();
            imageDiskList[drive] = fdp->loadedDisk;
            imageDiskList[drive]->Addref();
            imageDiskChange[drive] = true;
        }
        if (swapInDisksSpecificDrive == drive && diskSwap[swapPosition]) {
            diskSwap[swapPosition]->Release();
            diskSwap[swapPosition] = fdp->loadedDisk;
            diskSwap[swapPosition]->Addref();
        }
        if (!dos_kernel_disabled) {
            char name[DOS_NAMELENGTH_ASCII],lname[LFN_NAMELENGTH];
            uint32_t size,hsize;uint16_t date;uint16_t time;uint8_t attr;
            RealPt save_dta = dos.dta();
            dos.dta(dos.tables.tempdta);
            DOS_DTA dta(dos.dta());
            char root[7] = {(char)('A'+drive),':','\\','*','.','*',0};
            bool ret = DOS_FindFirst(root,DOS_ATTR_VOLUME);
            if (ret) {
                dta.GetResult(name,lname,size,hsize,date,time,attr);
                DOS_FindNext();
            } else name[0] = 0;
            dos.dta(save_dta);
        }
    }
    if (old) old->UnMount();
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

void DriveManager::CycleDisks(int drive, bool notify, unsigned int position) {
	unsigned int numDisks = (unsigned int)driveInfos[drive].disks.size();
	if (numDisks > 1) {
		// cycle disk
		unsigned int currentDisk = driveInfos[drive].currentDisk;
        const DOS_Drive* oldDisk = driveInfos[drive].disks[currentDisk];
        if (position<1)
            currentDisk = (currentDisk + 1u) % numDisks;
        else if (position>numDisks)
            currentDisk = 0;
        else
            currentDisk = position - 1;
		DOS_Drive* newDisk = driveInfos[drive].disks[currentDisk];
		driveInfos[drive].currentDisk = currentDisk;
		if (drive < MAX_DISK_IMAGES && imageDiskList[drive] != NULL) {
			imageDiskList[drive]->Release();
			imageDiskList[drive] = NULL;

			if (strncmp(newDisk->GetInfo(),"fatDrive",8) == 0)
				imageDiskList[drive] = ((fatDrive *)newDisk)->loadedDisk;
			else
				imageDiskList[drive] = (imageDisk *)newDisk;

			if (imageDiskList[drive] != NULL) imageDiskList[drive]->Addref();
			if ((drive == 2 || drive == 3) && imageDiskList[drive]->hardDrive) updateDPT();
		}
		
		// copy working directory, acquire system resources and finally switch to next drive
		strcpy(newDisk->curdir, oldDisk->curdir);
		newDisk->Activate();
		if (!dos_kernel_disabled) newDisk->UpdateDPB(currentDrive);
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
			currentDisk = (currentDisk + 1u) % numDisks;
			DOS_Drive* newDisk = driveInfos[idrive].disks[currentDisk];
			driveInfos[idrive].currentDisk = currentDisk;
			
			// copy working directory, acquire system resources and finally switch to next drive
			strcpy(newDisk->curdir, oldDisk->curdir);
			newDisk->Activate();
            if (!dos_kernel_disabled) newDisk->UpdateDPB(currentDrive);
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

int DriveManager::GetDisksSize(int drive) {
    return (int)driveInfos[drive].disks.size();
}

char swappos[10];
char * DriveManager::GetDrivePosition(int drive) {
    sprintf(swappos, "%d / %d", driveInfos[drive].currentDisk+1, (int)driveInfos[drive].disks.size());
    return swappos;
}

bool drivemanager_init = false;
bool int13_extensions_enable = true;
bool int13_disk_change_detect_enable = true;

void DriveManager::Init(Section* s) {
    const Section_prop* section = static_cast<Section_prop*>(s);

	drivemanager_init = true;

	int13_extensions_enable = section->Get_bool("int 13 extensions");
	int13_disk_change_detect_enable = section->Get_bool("int 13 disk change detect");

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
		LOG(LOG_DOSMISC,LOG_DEBUG)("Initializing drive system");
		DriveManager::Init(control->GetSection("dos"));
	}
}

void DRIVES_Init() {
	LOG(LOG_DOSMISC,LOG_DEBUG)("Initializing DOS drives");

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
