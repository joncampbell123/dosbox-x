/*
 *  Copyright (C) 2002-2019  The DOSBox Team
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335, USA.
 */


#include <cctype>
#include <cstring>
#include "cdrom.h"
#include "dosbox.h"
#include "dos_system.h"
#include "support.h"
#include "drives.h"

#define FLAGS1	((iso) ? de.fileFlags : de.timeZone)
#define FLAGS2	((iso) ? de->fileFlags : de->timeZone)

char fullname[LFN_NAMELENGTH];

using namespace std;

class isoFile : public DOS_File {
public:
	isoFile(isoDrive *drive, const char *name, FileStat_Block *stat, Bit32u offset);
	bool Read(Bit8u *data, Bit16u *size);
	bool Write(const Bit8u *data, Bit16u *size);
	bool Seek(Bit32u *pos, Bit32u type);
	bool Close();
	Bit16u GetInformation(void);
	Bit32u GetSeekPos(void);
private:
	isoDrive *drive;
    Bit8u buffer[ISO_FRAMESIZE] = {};
	int cachedSector;
	Bit32u fileBegin;
	Bit32u filePos;
	Bit32u fileEnd;
//	Bit16u info;
};

isoFile::isoFile(isoDrive *drive, const char *name, FileStat_Block *stat, Bit32u offset) {
	this->drive = drive;
	time = stat->time;
	date = stat->date;
	attr = stat->attr;
	fileBegin = offset;
	filePos = fileBegin;
	fileEnd = fileBegin + stat->size;
	cachedSector = -1;
	open = true;
	this->name = NULL;
	SetName(name);
}

bool isoFile::Read(Bit8u *data, Bit16u *size) {
	if (filePos + *size > fileEnd)
		*size = (Bit16u)(fileEnd - filePos);
	
	Bit16u nowSize = 0;
	int sector = (int)(filePos / ISO_FRAMESIZE);
	Bit16u sectorPos = (Bit16u)(filePos % ISO_FRAMESIZE);
	
	if (sector != cachedSector) {
		if (drive->readSector(buffer, (unsigned int)sector)) cachedSector = sector;
		else { *size = 0; cachedSector = -1; }
	}
	while (nowSize < *size) {
		Bit16u remSector = ISO_FRAMESIZE - sectorPos;
		Bit16u remSize = *size - nowSize;
		if(remSector < remSize) {
			memcpy(&data[nowSize], &buffer[sectorPos], remSector);
			nowSize += remSector;
			sectorPos = 0;
			sector++;
			cachedSector++;
			if (!drive->readSector(buffer, (unsigned int)sector)) {
				*size = nowSize;
				cachedSector = -1;
			}
		} else {
			memcpy(&data[nowSize], &buffer[sectorPos], remSize);
			nowSize += remSize;
		}
			
	}
	
	*size = nowSize;
	filePos += *size;
	return true;
}

bool isoFile::Write(const Bit8u* /*data*/, Bit16u* /*size*/) {
	return false;
}

bool isoFile::Seek(Bit32u *pos, Bit32u type) {
	switch (type) {
		case DOS_SEEK_SET:
			filePos = fileBegin + *pos;
			break;
		case DOS_SEEK_CUR:
			filePos += *pos;
			break;
		case DOS_SEEK_END:
			filePos = fileEnd + *pos;
			break;
		default:
			return false;
	}
	if (filePos > fileEnd || filePos < fileBegin)
		filePos = fileEnd;
	
	*pos = filePos - fileBegin;
	return true;
}

bool isoFile::Close() {
	if (refCtr == 1) open = false;
	return true;
}

Bit16u isoFile::GetInformation(void) {
	return 0x40;		// read-only drive
}

Bit32u isoFile::GetSeekPos() {
	return filePos - fileBegin;
}


int   MSCDEX_RemoveDrive(char driveLetter);
int   MSCDEX_AddDrive(char driveLetter, const char* physicalPath, Bit8u& subUnit);
void  MSCDEX_ReplaceDrive(CDROM_Interface* cdrom, Bit8u subUnit);
bool  MSCDEX_HasDrive(char driveLetter);
bool  MSCDEX_GetVolumeName(Bit8u subUnit, char* name);
Bit8u MSCDEX_GetSubUnit(char driveLetter);

bool CDROM_Interface_Image::images_init = false;

isoDrive::isoDrive(char driveLetter, const char *fileName, Bit8u mediaid, int &error)
         :iso(false),
          dataCD(false),
          mediaid(0),
          subUnit(0),
          driveLetter('\0')
 {

    if (!CDROM_Interface_Image::images_init) {
        CDROM_Interface_Image::images_init = true;
        for (size_t i=0;i < 26;i++)
            CDROM_Interface_Image::images[i] = NULL;
    }

	this->fileName[0]  = '\0';
	this->discLabel[0] = '\0';
	subUnit = 0;
	nextFreeDirIterator = 0;
	memset(dirIterators, 0, sizeof(dirIterators));
	memset(sectorHashEntries, 0, sizeof(sectorHashEntries));
	memset(&rootEntry, 0, sizeof(isoDirEntry));
	
	safe_strncpy(this->fileName, fileName, CROSS_LEN);
	error = UpdateMscdex(driveLetter, fileName, subUnit);

	if (!error) {
		if (loadImage()) {
			strcpy(info, "isoDrive ");
			strcat(info, fileName);
			this->driveLetter = driveLetter;
			this->mediaid = mediaid;
			char buffer[32] = { 0 };
			if (!MSCDEX_GetVolumeName(subUnit, buffer)) strcpy(buffer, "");
			Set_Label(buffer,discLabel,true);

		} else if (CDROM_Interface_Image::images[subUnit]->HasDataTrack() == false) { //Audio only cdrom
			strcpy(info, "isoDrive ");
			strcat(info, fileName);
			this->driveLetter = driveLetter;
			this->mediaid = mediaid;
			char buffer[32] = { 0 };
			strcpy(buffer, "Audio_CD");
			Set_Label(buffer,discLabel,true);
		} else error = 6; //Corrupt image
	}
}

isoDrive::~isoDrive() { }

int isoDrive::UpdateMscdex(char driveLetter, const char* path, Bit8u& subUnit) {
	if (MSCDEX_HasDrive(driveLetter)) {
		subUnit = MSCDEX_GetSubUnit(driveLetter);
		CDROM_Interface_Image* oldCdrom = CDROM_Interface_Image::images[subUnit];
		CDROM_Interface* cdrom = new CDROM_Interface_Image(subUnit);
		char pathCopy[CROSS_LEN];
		safe_strncpy(pathCopy, path, CROSS_LEN);
		if (!cdrom->SetDevice(pathCopy, 0)) {
			CDROM_Interface_Image::images[subUnit] = oldCdrom;
			delete cdrom;
			return 3;
		}
		MSCDEX_ReplaceDrive(cdrom, subUnit);
		return 0;
	} else {
		return MSCDEX_AddDrive(driveLetter, path, subUnit);
	}
}

void isoDrive::Activate(void) {
	UpdateMscdex(driveLetter, fileName, subUnit);
}

bool isoDrive::FileOpen(DOS_File **file, const char *name, Bit32u flags) {
	if ((flags & 0x0f) == OPEN_WRITE) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}
	
	isoDirEntry de;
	bool success = lookup(&de, name) && !IS_DIR(FLAGS1);

	if (success) {
		FileStat_Block file_stat;
		file_stat.size = DATA_LENGTH(de);
		file_stat.attr = DOS_ATTR_ARCHIVE | DOS_ATTR_READ_ONLY;
		file_stat.date = DOS_PackDate(1900 + de.dateYear, de.dateMonth, de.dateDay);
		file_stat.time = DOS_PackTime(de.timeHour, de.timeMin, de.timeSec);
		*file = new isoFile(this, name, &file_stat, EXTENT_LOCATION(de) * ISO_FRAMESIZE);
		(*file)->flags = flags;
	}
	return success;
}

bool isoDrive::FileCreate(DOS_File** /*file*/, const char* /*name*/, Bit16u /*attributes*/) {
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool isoDrive::FileUnlink(const char* /*name*/) {
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool isoDrive::RemoveDir(const char* /*dir*/) {
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool isoDrive::MakeDir(const char* /*dir*/) {
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool isoDrive::TestDir(const char *dir) {
	isoDirEntry de;	
	return (lookup(&de, dir) && IS_DIR(FLAGS1));
}

bool isoDrive::FindFirst(const char *dir, DOS_DTA &dta, bool fcb_findfirst) {
	isoDirEntry de;
	if (!lookup(&de, dir)) {
		DOS_SetError(DOSERR_PATH_NOT_FOUND);
		return false;
	}
	
	// get a directory iterator and save its id in the dta
	int dirIterator = GetDirIterator(&de);
	bool isRoot = (*dir == 0);
	dirIterators[dirIterator].root = isRoot;
	dta.SetDirID((Bit16u)dirIterator);

	Bit8u attr;
	char pattern[CROSS_LEN];
    dta.GetSearchParams(attr, pattern, true);
   
	if (attr == DOS_ATTR_VOLUME) {
		dta.SetResult(discLabel, discLabel, 0, 0, 0, DOS_ATTR_VOLUME);
		return true;
	} else if ((attr & DOS_ATTR_VOLUME) && isRoot && !fcb_findfirst) {
		if (WildFileCmp(discLabel,pattern)) {
			// Get Volume Label (DOS_ATTR_VOLUME) and only in basedir and if it matches the searchstring
			dta.SetResult(discLabel, discLabel, 0, 0, 0, DOS_ATTR_VOLUME);
			return true;
		}
	}

	return FindNext(dta);
}

bool isoDrive::FindNext(DOS_DTA &dta) {
	Bit8u attr;
	char pattern[CROSS_LEN], findName[DOS_NAMELENGTH_ASCII], lfindName[ISO_MAXPATHNAME];
    dta.GetSearchParams(attr, pattern, true);
	
	int dirIterator = dta.GetDirID();
	bool isRoot = dirIterators[dirIterator].root;
	
    isoDirEntry de = {};
	while (GetNextDirEntry(dirIterator, &de)) {
		Bit8u findAttr = 0;
		if (IS_DIR(FLAGS1)) findAttr |= DOS_ATTR_DIRECTORY;
		else findAttr |= DOS_ATTR_ARCHIVE;
		if (IS_HIDDEN(FLAGS1)) findAttr |= DOS_ATTR_HIDDEN;

		if (strcmp((char*)de.ident,(char*)fullname))
			strcpy(lfindName,fullname);
		else
			GetLongName((char*)de.ident,lfindName);

        if (!IS_ASSOC(FLAGS1) && !(isRoot && de.ident[0]=='.') && (WildFileCmp((char*)de.ident, pattern) || LWildFileCmp(lfindName, pattern))
			&& !(~attr & findAttr & (DOS_ATTR_DIRECTORY | DOS_ATTR_HIDDEN | DOS_ATTR_SYSTEM))) {
			
			/* file is okay, setup everything to be copied in DTA Block */
			findName[0] = 0;
			if(strlen((char*)de.ident) < DOS_NAMELENGTH_ASCII) {
				strcpy(findName, (char*)de.ident);
				upcase(findName);
			}
			Bit32u findSize = DATA_LENGTH(de);
			Bit16u findDate = DOS_PackDate(1900 + de.dateYear, de.dateMonth, de.dateDay);
			Bit16u findTime = DOS_PackTime(de.timeHour, de.timeMin, de.timeSec);
            dta.SetResult(findName, lfindName, findSize, findDate, findTime, findAttr);
			return true;
		}
	}
	// after searching the directory, free the iterator
	FreeDirIterator(dirIterator);
	
	DOS_SetError(DOSERR_NO_MORE_FILES);
	return false;
}

bool isoDrive::Rename(const char* /*oldname*/, const char* /*newname*/) {
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool isoDrive::GetFileAttr(const char *name, Bit16u *attr) {
	*attr = 0;
	isoDirEntry de;
	bool success = lookup(&de, name);
	if (success) {
		*attr = DOS_ATTR_ARCHIVE | DOS_ATTR_READ_ONLY;
		if (IS_HIDDEN(FLAGS1)) *attr |= DOS_ATTR_HIDDEN;
		if (IS_DIR(FLAGS1)) *attr |= DOS_ATTR_DIRECTORY;
	}
	return success;
}

bool isoDrive::GetFileAttrEx(char* name, struct stat *status) {
    (void)name;
    (void)status;
	return false;
}

unsigned long isoDrive::GetCompressedSize(char* name) {
    (void)name;
	return 0;
}

#if defined (WIN32)
HANDLE isoDrive::CreateOpenFile(const char* name) {
    (void)name;
	DOS_SetError(1);
	return INVALID_HANDLE_VALUE;
}
#endif

bool isoDrive::AllocationInfo(Bit16u *bytes_sector, Bit8u *sectors_cluster, Bit16u *total_clusters, Bit16u *free_clusters) {
	*bytes_sector = 2048;
	*sectors_cluster = 1; // cluster size for cdroms ?
	*total_clusters = 65535;
	*free_clusters = 0;
	return true;
}

bool isoDrive::FileExists(const char *name) {
	isoDirEntry de;
	return (lookup(&de, name) && !IS_DIR(FLAGS1));
}

bool isoDrive::FileStat(const char *name, FileStat_Block *const stat_block) {
	isoDirEntry de;
	bool success = lookup(&de, name);
	
	if (success) {
		stat_block->date = DOS_PackDate(1900 + de.dateYear, de.dateMonth, de.dateDay);
		stat_block->time = DOS_PackTime(de.timeHour, de.timeMin, de.timeSec);
		stat_block->size = DATA_LENGTH(de);
		stat_block->attr = DOS_ATTR_ARCHIVE | DOS_ATTR_READ_ONLY;
		if (IS_DIR(FLAGS1)) stat_block->attr |= DOS_ATTR_DIRECTORY;
	}
	
	return success;
}

Bit8u isoDrive::GetMediaByte(void) {
	return mediaid;
}

bool isoDrive::isRemote(void) {
	return true;
}

bool isoDrive::isRemovable(void) {
	return true;
}

Bits isoDrive::UnMount(void) {
	if(MSCDEX_RemoveDrive(driveLetter)) {
		delete this;
		return 0;
	}
	return 2;
}

int isoDrive::GetDirIterator(const isoDirEntry* de) {
	int dirIterator = nextFreeDirIterator;
	
	// get start and end sector of the directory entry (pad end sector if necessary)
	dirIterators[dirIterator].currentSector = EXTENT_LOCATION(*de);
	dirIterators[dirIterator].endSector =
		EXTENT_LOCATION(*de) + DATA_LENGTH(*de) / ISO_FRAMESIZE - 1;
	if (DATA_LENGTH(*de) % ISO_FRAMESIZE != 0)
		dirIterators[dirIterator].endSector++;
	
	// reset position and mark as valid
	dirIterators[dirIterator].pos = 0;
	dirIterators[dirIterator].valid = true;

	// advance to next directory iterator (wrap around if necessary)
	nextFreeDirIterator = (nextFreeDirIterator + 1) % MAX_OPENDIRS;
	
	return dirIterator;
}

bool isoDrive::GetNextDirEntry(const int dirIteratorHandle, isoDirEntry* de) {
	bool result = false;
	Bit8u* buffer = NULL;
	DirIterator& dirIterator = dirIterators[dirIteratorHandle];
	
	// check if the directory entry is valid
	if (dirIterator.valid && ReadCachedSector(&buffer, dirIterator.currentSector)) {
		// check if the next sector has to be read
		if ((dirIterator.pos >= ISO_FRAMESIZE)
		 || (buffer[dirIterator.pos] == 0)
		 || (dirIterator.pos + buffer[dirIterator.pos] > ISO_FRAMESIZE)) {
		 	
			// check if there is another sector available
		 	if (dirIterator.currentSector < dirIterator.endSector) {
			 	dirIterator.pos = 0;
			 	dirIterator.currentSector++;
			 	if (!ReadCachedSector(&buffer, dirIterator.currentSector)) {
			 		return false;
			 	}
		 	} else {
		 		return false;
		 	}
		 }
		 // read sector and advance sector pointer
		 int length = readDirEntry(de, &buffer[dirIterator.pos]);
		 result = length >= 0;
         if (length > 0) dirIterator.pos += (unsigned int)length;
	}
	return result;
}

void isoDrive::FreeDirIterator(const int dirIterator) {
	dirIterators[dirIterator].valid = false;
	
	// if this was the last aquired iterator decrement nextFreeIterator
	if ((dirIterator + 1) % MAX_OPENDIRS == nextFreeDirIterator) {
		if (nextFreeDirIterator>0) {
			nextFreeDirIterator--;
		} else {
			nextFreeDirIterator = MAX_OPENDIRS-1;
		}
	}
}

bool isoDrive::ReadCachedSector(Bit8u** buffer, const Bit32u sector) {
	// get hash table entry
	unsigned int pos = sector % ISO_MAX_HASH_TABLE_SIZE;
	SectorHashEntry& he = sectorHashEntries[pos];
	
	// check if the entry is valid and contains the correct sector
	if (!he.valid || he.sector != sector) {
		if (!CDROM_Interface_Image::images[subUnit]->ReadSector(he.data, false, sector)) {
			return false;
		}
		he.valid = true;
		he.sector = sector;
	}
	
	*buffer = he.data;
	return true;
}

inline bool isoDrive :: readSector(Bit8u *buffer, Bit32u sector) {
	return CDROM_Interface_Image::images[subUnit]->ReadSector(buffer, false, sector);
}

int isoDrive :: readDirEntry(isoDirEntry *de, Bit8u *data) {	
	// copy data into isoDirEntry struct, data[0] = length of DirEntry
//	if (data[0] > sizeof(isoDirEntry)) return -1;//check disabled as isoDirentry is currently 258 bytes large. So it always fits
	memcpy(de, data, data[0]);//Perharps care about a zero at the end.
	
	// xa not supported
	if (de->extAttrLength != 0) return -1;
	// interleaved mode not supported
	if (de->fileUnitSize != 0 || de->interleaveGapSize != 0) return -1;
	
	// modify file identifier for use with dosbox
	if (de->length < 33 + de->fileIdentLength) return -1;
	if (IS_DIR(FLAGS2)) {
		if (de->fileIdentLength == 1 && de->ident[0] == 0) strcpy((char*)de->ident, ".");
		else if (de->fileIdentLength == 1 && de->ident[0] == 1) strcpy((char*)de->ident, "..");
		else {
			if (de->fileIdentLength > 200) return -1;
			de->ident[de->fileIdentLength] = 0;
		}
	} else {
		if (de->fileIdentLength > 200) return -1;
		de->ident[de->fileIdentLength] = 0;	
		// remove any file version identifiers as there are some cdroms that don't have them
		strreplace((char*)de->ident, ';', 0);	
		// if file has no extension remove the trailing dot
		size_t tmp = strlen((char*)de->ident);
		if (tmp > 0) {
			if (de->ident[tmp - 1] == '.') de->ident[tmp - 1] = 0;
		}
	}
	char* dotpos = strchr((char*)de->ident, '.');
	if (dotpos!=NULL) {
		if (strlen(dotpos)>4) dotpos[4]=0;
		if (dotpos-(char*)de->ident>8) {
			strcpy((char*)(&de->ident[8]),dotpos);
		}
	} else if (strlen((char*)de->ident)>8) de->ident[8]=0;
	return de->length;
}

bool isoDrive :: loadImage() {
	Bit8u pvd[COOKED_SECTOR_SIZE];
	dataCD = false;
	readSector(pvd, ISO_FIRST_VD);
	if (pvd[0] == 1 && !strncmp((char*)(&pvd[1]), "CD001", 5) && pvd[6] == 1) iso = true;
	else if (pvd[8] == 1 && !strncmp((char*)(&pvd[9]), "CDROM", 5) && pvd[14] == 1) iso = false;
	else return false;
	Bit16u offset = iso ? 156 : 180;
	if (readDirEntry(&this->rootEntry, &pvd[offset])>0) {
		dataCD = true;
		return true;
	}
	return false;
}

bool isoDrive :: lookup(isoDirEntry *de, const char *path) {
	if (!dataCD) return false;
	*de = this->rootEntry;
	if (!strcmp(path, "")) return true;
	
	char isoPath[ISO_MAXPATHNAME], longname[ISO_MAXPATHNAME];
	safe_strncpy(isoPath, path, ISO_MAXPATHNAME);
	strreplace(isoPath, '\\', '/');
	
	// iterate over all path elements (name), and search each of them in the current de
	for(char* name = strtok(isoPath, "/"); NULL != name; name = strtok(NULL, "/")) {

		bool found = false;	
		// current entry must be a directory, abort otherwise
		if (IS_DIR(FLAGS2)) {
			
			// remove the trailing dot if present
			size_t nameLength = strlen(name);
			if (nameLength > 0) {
				if (name[nameLength - 1] == '.') name[nameLength - 1] = 0;
			}
			
			// look for the current path element
			int dirIterator = GetDirIterator(de);
			while (!found && GetNextDirEntry(dirIterator, de)) {
				GetLongName((char*)de->ident,longname);
				if (!IS_ASSOC(FLAGS2) && ((0 == strncasecmp((char*) de->ident, name, ISO_MAX_FILENAME_LENGTH)) || 0 == strncasecmp((char*) longname, name, ISO_MAXPATHNAME))) {
					found = true;
				}
			}
			FreeDirIterator(dirIterator);
		}
		if (!found) return false;
	}
	return true;
}

void IDE_ATAPI_MediaChangeNotify(unsigned char drive_index);

void isoDrive :: MediaChange() {
	IDE_ATAPI_MediaChangeNotify(toupper(driveLetter) - 'A'); /* ewwww */
}

void isoDrive :: GetLongName(char *ident, char *lfindName) {
    char *c=ident+strlen(ident);
    int i,j=222-strlen(ident)-6;
    for (i=5;i<j;i++) {
        if (*(c+i)=='N'&&*(c+i+1)=='M'&&*(c+i+2)>0&&*(c+i+3)==1&&*(c+i+4)==0&&*(c+i+5)>0)
            break;
        }
    if (i<j&&strcmp(ident,".")&&strcmp(ident,"..")) {
        strncpy(lfindName,c+i+5,*(c+i+2)-5);
        lfindName[*(c+i+2)-5]=0;
    } else
        strcpy(lfindName,ident);
}


