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


#include <cctype>
#include <cstring>
#include <assert.h>
#if defined(WIN32)
#include <winsock2.h>
#endif
#include "cdrom.h"
#include "dosbox.h"
#include "byteorder.h"
#include "dos_system.h"
#include "logging.h"
#include "support.h"
#include "drives.h"

#define FLAGS1	((iso) ? de.fileFlags : de.timeZone)
#define FLAGS2	((iso) ? de->fileFlags : de->timeZone)

char fullname[LFN_NAMELENGTH];
static uint16_t sdid[256];
extern int lfn_filefind_handle;
extern bool gbk, isDBCSCP(), isKanji1_gbk(uint8_t chr), shiftjis_lead_byte(int c);
extern bool filename_not_8x3(const char *n), filename_not_strict_8x3(const char *n);
extern bool CodePageHostToGuestUTF16(char *d/*CROSS_LEN*/,const uint16_t *s/*CROSS_LEN*/);

using namespace std;

////////////////////////////////////

/*From Linux kernel source*/
/** CRC table for the CRC ITU-T V.41 0x1021 (x^16 + x^12 + x^15 + 1) */
const uint16_t UDF_crc_itu_t_table[256] = {
	0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
	0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
	0x1231, 0x0210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6,
	0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de,
	0x2462, 0x3443, 0x0420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485,
	0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
	0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6, 0x5695, 0x46b4,
	0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc,
	0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
	0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b,
	0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50, 0x3a33, 0x2a12,
	0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a,
	0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41,
	0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49,
	0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70,
	0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78,
	0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f,
	0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
	0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e,
	0x02b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256,
	0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
	0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
	0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c,
	0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
	0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab,
	0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882, 0x28a3,
	0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
	0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92,
	0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9,
	0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1,
	0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8,
	0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0x0ed1, 0x1ef0
};

uint16_t UDF_crc_itu_t(uint16_t crc, const uint8_t *buffer, size_t len)
{
	while (len--)
		crc = UDF_crc_itu_t_byte(crc, *buffer++);

	return crc;
}

////////////////////////////////////

void UDFTagId::parse(const unsigned int sz,const unsigned char *b) {
	if (sz >= 16) {
		TagIdentifier = le16toh( *((uint16_t*)(&b[0])) );
		DescriptorVersion = le16toh( *((uint16_t*)(&b[2])) );
		TagChecksum = b[4];
		Reserved = b[5];
		TagSerialNumber = le16toh( *((uint16_t*)(&b[6])) );
		DescriptorCRC = le16toh( *((uint16_t*)(&b[8])) );
		DescriptorCRCLength = le16toh( *((uint16_t*)(&b[10])) );
		TagLocation = le32toh( *((uint32_t*)(&b[12])) );
	}
	else {
		TagIdentifier = 0;
		TagChecksum = 0;
		Reserved = 0;
		TagLocation = 0;
	}
}

bool UDFTagId::tagChecksumOK(const unsigned int sz,const unsigned char *b) {
	uint8_t chksum = 0;

	if (sz < 16)
		return false;

	for (unsigned int i=0;i <= 3;i++)
		chksum += b[i];
	for (unsigned int i=5;i <= 15;i++)
		chksum += b[i];

	if (chksum != TagChecksum)
		return false;

	return true;
}

bool UDFTagId::dataChecksumOK(const unsigned int sz,const unsigned char *b) {
	if (DescriptorCRCLength != 0) {
		if ((DescriptorCRCLength+16u) > sz)
			return false;
		if (DescriptorCRC != UDF_crc_itu_t(0,b+16,DescriptorCRCLength))
			return false;
	}

	return true;
}

bool UDFTagId::get(const unsigned int sz,const unsigned char *b) {
	parse(sz,b);

	if (TagLocation == 0 && TagIdentifier == 0)
		return false;

	return tagChecksumOK(sz,b) && dataChecksumOK(sz,b);
}

UDFTagId::UDFTagId(const unsigned int sz,const unsigned char *b) {
	parse(sz,b);
}

UDFTagId::UDFTagId() {
}

////////////////////////////////////

void UDFextent_ad::get(const unsigned int sz,const unsigned char *b) {
	if (sz >= 8u) {
		ExtentLength = le32toh( *((uint32_t*)(&b[0])) );
		ExtentLocation = le32toh( *((uint32_t*)(&b[4])) );
	}
}

UDFextent_ad::UDFextent_ad(const unsigned int sz,const unsigned char *b) {
	get(sz,b);
}

UDFextent_ad::UDFextent_ad() {
}

////////////////////////////////////

void UDFAnchorVolumeDescriptorPointer::get(UDFTagId &tag,const unsigned int sz,const unsigned char *b) {
	if (sz >= 32u) {
		DescriptorTag = tag;
		MainVolumeDescriptorSequenceExtent.get(8,b+16);
		ReserveVolumeDescriptorSequenceExtent.get(8,b+24);
	}
}

UDFAnchorVolumeDescriptorPointer::UDFAnchorVolumeDescriptorPointer(UDFTagId &tag,const unsigned int sz,const unsigned char *b) {
	get(tag,sz,b);
}

UDFAnchorVolumeDescriptorPointer::UDFAnchorVolumeDescriptorPointer() {
}

////////////////////////////////////

class isoFile : public DOS_File {
public:
    isoFile(isoDrive* drive, const char* name, const FileStat_Block* stat, uint32_t offset);
	bool Read(uint8_t *data, uint16_t *size);
	bool Write(const uint8_t *data, uint16_t *size);
	bool Seek(uint32_t *pos, uint32_t type);
	bool Close();
	uint16_t GetInformation(void);
	uint32_t GetSeekPos(void);
private:
	isoDrive *drive;
    uint8_t buffer[ISO_FRAMESIZE] = {};
    int cachedSector = -1;
	uint32_t fileBegin;
	uint32_t filePos;
	uint32_t fileEnd;
//	uint16_t info;
};

isoFile::isoFile(isoDrive* drive, const char* name, const FileStat_Block* stat, uint32_t offset) : drive(drive), fileBegin(offset) {
	time = stat->time;
	date = stat->date;
	attr = stat->attr;
	filePos = fileBegin;
	fileEnd = fileBegin + stat->size;
	open = true;
	this->name = NULL;
	SetName(name);
}

bool isoFile::Read(uint8_t *data, uint16_t *size) {
	if (filePos + *size > fileEnd)
		*size = (uint16_t)(fileEnd - filePos);
	
	uint16_t nowSize = 0;
	int sector = (int)(filePos / ISO_FRAMESIZE);
	uint16_t sectorPos = (uint16_t)(filePos % ISO_FRAMESIZE);
	
	if (sector != cachedSector) {
		if (drive->readSector(buffer, (unsigned int)sector)) cachedSector = sector;
		else { *size = 0; cachedSector = -1; }
	}
	while (nowSize < *size) {
		uint16_t remSector = ISO_FRAMESIZE - sectorPos;
		uint16_t remSize = *size - nowSize;
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

bool isoFile::Write(const uint8_t* /*data*/, uint16_t* /*size*/) {
	return false;
}

bool isoFile::Seek(uint32_t *pos, uint32_t type) {
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

uint16_t isoFile::GetInformation(void) {
	return 0x40;		// read-only drive
}

uint32_t isoFile::GetSeekPos() {
	return filePos - fileBegin;
}

int   MSCDEX_RemoveDrive(char driveLetter);
int   MSCDEX_AddDrive(char driveLetter, const char* physicalPath, uint8_t& subUnit);
void  MSCDEX_ReplaceDrive(CDROM_Interface* cdrom, uint8_t subUnit);
bool  MSCDEX_HasDrive(char driveLetter);
bool  MSCDEX_GetVolumeName(uint8_t subUnit, char* name);
uint8_t MSCDEX_GetSubUnit(char driveLetter);

bool CDROM_Interface_Image::images_init = false;

isoDrive::isoDrive(char driveLetter, const char* fileName, uint8_t mediaid, int& error, std::vector<std::string>& options) {
    enable_udf = (dos.version.major > 7 || (dos.version.major == 7 && dos.version.minor >= 10));//default
    enable_rock_ridge = (dos.version.major >= 7 || uselfn);//default
    enable_joliet = (dos.version.major >= 7 || uselfn);//default
    is_rock_ridge = false;
    is_joliet = false;
    is_udf = false;
    for (const auto &opt : options) {
        size_t equ = opt.find_first_of('=');
        std::string name,value;

        if (equ != std::string::npos) {
            name = opt.substr(0,equ);
            value = opt.substr(equ+1);
        }
        else {
            name = opt;
            value.clear();
        }

        if (name == "rr") { // Enable/disable Rock Ridge extensions
            if (value == "1" || value == "") enable_rock_ridge = true; // "-o rr" or "-o rr=1"
            else if (value == "0") enable_rock_ridge = false;
        }
        else if (name == "joliet") { // Enable/disable Joliet extensions
            if (value == "1" || value == "") enable_joliet = true; // "-o joliet" or "-o joliet=1"
            else if (value == "0") enable_joliet = false;
	}
        else if (name == "udf") { // Enable/disable UDF
            if (value == "1" || value == "") enable_udf = true; // "-o udf" or "-o udf=1"
            else if (value == "0") enable_udf = false;
	}
    }

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

void isoDrive::setFileName(const char* fileName) {
	safe_strncpy(this->fileName, fileName, CROSS_LEN);
	strcpy(info, "isoDrive ");
	strcat(info, fileName);
}

int isoDrive::UpdateMscdex(char driveLetter, const char* path, uint8_t& subUnit) {
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

bool isoDrive::FileOpen(DOS_File **file, const char *name, uint32_t flags) {
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

bool isoDrive::FileCreate(DOS_File** /*file*/, const char* /*name*/, uint16_t /*attributes*/) {
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
	if (lfn_filefind_handle>=LFN_FILEFIND_MAX)
		dta.SetDirID((uint16_t)dirIterator);
	else
		sdid[lfn_filefind_handle]=dirIterator;

	uint8_t attr;
	char pattern[CROSS_LEN];
    dta.GetSearchParams(attr, pattern, false);
   
	if (attr == DOS_ATTR_VOLUME) {
		dta.SetResult(discLabel, discLabel, 0, 0, 0, 0, DOS_ATTR_VOLUME);
		return true;
	} else if ((attr & DOS_ATTR_VOLUME) && isRoot && !fcb_findfirst) {
		if (WildFileCmp(discLabel,pattern)) {
			// Get Volume Label (DOS_ATTR_VOLUME) and only in basedir and if it matches the searchstring
			dta.SetResult(discLabel, discLabel, 0, 0, 0, 0, DOS_ATTR_VOLUME);
			return true;
		}
	}

	return FindNext(dta);
}

bool isoDrive::FindNext(DOS_DTA &dta) {
	uint8_t attr;
	char pattern[CROSS_LEN], findName[DOS_NAMELENGTH_ASCII], lfindName[ISO_MAXPATHNAME];
    dta.GetSearchParams(attr, pattern, false);
	
	int dirIterator = lfn_filefind_handle>=LFN_FILEFIND_MAX?dta.GetDirID():sdid[lfn_filefind_handle];
	bool isRoot = dirIterators[dirIterator].root;
	
    isoDirEntry de = {};
	while (GetNextDirEntry(dirIterator, &de)) {
		uint8_t findAttr = DOS_ATTR_ARCHIVE | DOS_ATTR_READ_ONLY;
		if (IS_DIR(FLAGS1)) findAttr |= DOS_ATTR_DIRECTORY;
		if (IS_HIDDEN(FLAGS1)) findAttr |= DOS_ATTR_HIDDEN;
		strcpy(lfindName,fullname);
        if (!IS_ASSOC(FLAGS1) && !(isRoot && de.ident[0]=='.') && (WildFileCmp((char*)de.ident, pattern) || LWildFileCmp(lfindName, pattern))
			&& !(~attr & findAttr & (DOS_ATTR_DIRECTORY | DOS_ATTR_HIDDEN | DOS_ATTR_SYSTEM))) {
			
			/* file is okay, setup everything to be copied in DTA Block */
			findName[0] = 0;
			if(strlen((char*)de.ident) < DOS_NAMELENGTH_ASCII) {
				strcpy(findName, (char*)de.ident);
				upcase(findName);
			}
			uint32_t findSize = DATA_LENGTH(de);
			uint16_t findDate = DOS_PackDate(1900 + de.dateYear, de.dateMonth, de.dateDay);
			uint16_t findTime = DOS_PackTime(de.timeHour, de.timeMin, de.timeSec);
            dta.SetResult(findName, lfindName, findSize, 0, findDate, findTime, findAttr);
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

bool isoDrive::SetFileAttr(const char * name,uint16_t attr) {
    (void)attr;
	isoDirEntry de;
	DOS_SetError(lookup(&de, name) ? DOSERR_ACCESS_DENIED : DOSERR_FILE_NOT_FOUND);
	return false;
}

bool isoDrive::GetFileAttr(const char *name, uint16_t *attr) {
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

bool isoDrive::AllocationInfo(uint16_t *bytes_sector, uint8_t *sectors_cluster, uint16_t *total_clusters, uint16_t *free_clusters) {
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

uint8_t isoDrive::GetMediaByte(void) {
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
	dirIterators[dirIterator].index = 0;
	dirIterators[dirIterator].valid = true;

	// advance to next directory iterator (wrap around if necessary)
	nextFreeDirIterator = (nextFreeDirIterator + 1) % MAX_OPENDIRS;
	
	return dirIterator;
}

bool isoDrive::GetNextDirEntry(const int dirIteratorHandle, isoDirEntry* de) {
	bool result = false;
	uint8_t* buffer = NULL;
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
		 ++dirIterator.index;
		 int length = readDirEntry(de, &buffer[dirIterator.pos], dirIterator.index);
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

bool isoDrive::ReadCachedSector(uint8_t** buffer, const uint32_t sector) {
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

inline bool isoDrive :: readSector(uint8_t *buffer, uint32_t sector) {
	return CDROM_Interface_Image::images[subUnit]->ReadSector(buffer, false, sector);
}

int isoDrive::readDirEntry(isoDirEntry* de, const uint8_t* data,unsigned int dirIteratorIndex) {
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
			if (is_joliet) {
				de->ident[de->fileIdentLength+1] = 0; // for Joliet UCS-16
				// The string is big Endian UCS-16, convert to host Endian UCS-16
				for (size_t i=0;((const uint16_t*)de->ident)[i] != 0;i++) ((uint16_t*)de->ident)[i] = be16toh(((uint16_t*)de->ident)[i]);
				// finally, convert from UCS-16 to local code page, using C++ string construction to make a copy first
				CodePageHostToGuestUTF16((char*)de->ident,std::basic_string<uint16_t>((const uint16_t*)de->ident).c_str());
			}
		}
	} else {
		if (de->fileIdentLength > 200) return -1;
		de->ident[de->fileIdentLength] = 0;	
		if (is_joliet) {
			de->ident[de->fileIdentLength+1] = 0; // for Joliet UCS-16
			// remove any file version identifiers as there are some cdroms that don't have them
			uint16_t *w = (uint16_t*)(de->ident); // remember two NULs were written to make a UCS-16 NUL
			while (*w != 0) {
				if (be16toh(*w) == (uint16_t)(';')) *w = 0;
				w++;
			}
			// w happens to be at the end of the string now, step back one char and remove trailing period
			if (w != (uint16_t*)(de->ident)) {
				w--;
				if (be16toh(*w) == (uint16_t)('.')) *w = 0;
			}
			// The string is big Endian UCS-16, convert to host Endian UCS-16
			for (size_t i=0;((const uint16_t*)de->ident)[i] != 0;i++) ((uint16_t*)de->ident)[i] = be16toh(((uint16_t*)de->ident)[i]);
			// finally, convert from UCS-16 to local code page, using C++ string construction to make a copy first
			CodePageHostToGuestUTF16((char*)de->ident,std::basic_string<uint16_t>((const uint16_t*)de->ident).c_str());
		}
		else {
			// remove any file version identifiers as there are some cdroms that don't have them
			strreplace((char*)de->ident, ';', 0);	
			// if file has no extension remove the trailing dot
			size_t tmp = strlen((char*)de->ident);
			if (tmp > 0) {
				if (de->ident[tmp - 1] == '.') de->ident[tmp - 1] = 0;
			}
		}
	}

	bool is_rock_ridge_name = false;
	if (is_joliet) {
		strcpy((char*)fullname,(char*)de->ident);
	}
	else {
		strcpy((char*)fullname,(char*)de->ident);
		if (is_rock_ridge) {
			/* LEN_SKP bytes into the System Use Field (bytes after the final NUL byte of the identifier) */
			/* NTS: This code could never work with Joliet extensions because the code above (currently)
			 *      overwrites the first byte of the SUSP entries in order to make a NUL-terminated UCS-16
			 *      string */
			unsigned char *p = (unsigned char*)de->ident + de->fileIdentLength + 1/*NUL*/ + rr_susp_skip;
			unsigned char *f = (unsigned char*)de + de->length;

			/* SUSP basic structure:
			 *
			 * BP1/BP2   +0    <2 char signature>
			 * BP3       +2    <length including header>
			 * BP4       +3    <SUSP version>
			 * [payload] */
			while ((p+4) <= f) {
				unsigned char *entry = p;
				uint8_t len = entry[2];
				uint8_t ver = entry[3];

				if (len < 4) break;
				if ((p+len) > f) break;
				p += len;

				if (!memcmp(entry,"NM",2)) {
					/* BP5       +4         flags
					 *                         bit 0 = CONTINUE
					 *                         bit 1 = CURRENT
					 *                         bit 2 = PARENT
					 *                         bit [7:3] = RESERVED */
					if (len >= 5/*flags and at least one char*/ && ver == 1) {
						uint8_t flags = entry[4];

						if (flags & 0x7) {
							/* CONTINUE (bit 0) not supported yet, or CURRENT directory (bit 1), or PARENT directory (bit 2) */
						}
						else {
							/* BP6-payload end is the alternate name */
							size_t altnl = len - 5;

							if (altnl > 0) memcpy(fullname,entry+5,altnl);
							fullname[altnl] = 0;

							is_rock_ridge_name = true;
						}
					}
				}
			}
		}
	}

	bool jolietrr = is_joliet || (is_rock_ridge_name && filename_not_strict_8x3((char*)de->ident));
	if (!jolietrr && !(dos.version.major >= 7 || uselfn)) {
		char* dotpos = strchr((char*)de->ident, '.');
		if (dotpos!=NULL) {
			if (strlen(dotpos)>4) dotpos[4]=0;
			if (dotpos-(char*)de->ident>8) {
				strcpy((char*)(&de->ident[8]),dotpos);
			}
		} else if (strlen((char*)de->ident)>8) de->ident[8]=0;
	}
	if (jolietrr || filename_not_8x3((char*)de->ident)) {
		const char *ext = NULL;
		size_t tailsize = 0;
		bool lfn = false;
		char tail[128];

		if (strcmp((char*)fullname,".") != 0 && strcmp((char*)fullname,"..")) {
			size_t nl = 0,el = 0,periods = 0;
			const char *s = (const char*)fullname;
			while (*s != 0) {
				if (*s == '.') break;
				if (*s == ' ' || *s == '\'' || *s == '\"') lfn = true;
				nl++;
				s++;
			}
			if (!nl || nl > 8) lfn = true;
			if (*s == '.') {
				periods++;
				if (nl) ext = s+1;
				s++;
			}
			while (*s != 0) {
				if (*s == '.') {
					periods++;
					ext = s+1;
				}
				if (*s == ' ' || *s == '\'' || *s == '\"') lfn = true;
				el++;
				s++;
			}
			if (periods > 1 || el > 3) lfn = true;
		}

		/* Windows 95 adds a ~number to the 8.3 name if effectively an LFN.
		 * I'm not 100% certain but the index appears to be related to the index of the ISO 9660 dirent.
		 * This is a guess as to how it works. */
		if (lfn) {
			tailsize = sprintf(tail,"~%u",dirIteratorIndex);
			const char *s = (const char*)fullname;
			char *d = (char*)de->ident;
			size_t c;

			c = 0;
			while (*s == '.'||*s == ' ') s++;
			bool lead = false;
			while (*s != 0) {
				if (s == ext) break; // doesn't match if ext == NULL, so no harm in that case
                if (!lead && ((IS_PC98_ARCH && shiftjis_lead_byte(*s & 0xFF)) || (isDBCSCP() && isKanji1_gbk(*s & 0xFF)))) {
                    if (c >= (7-tailsize)) break;
                    lead = true;
                    *d++ = *s;
                    c++;
				} else if ((unsigned char)*s <= 32 || (unsigned char)*s == 127 || *s == '.' || *s == '\"' || *s == '+' || *s == '=' || *s == ',' || *s == ';' || *s == ':' || *s == '<' || *s == '>' || ((*s == '[' || *s == ']' || *s == '|' || *s == '\\')&&(!lead||((dos.loaded_codepage==936||IS_PDOSV)&&!gbk)))||*s=='?'||*s=='*') {
                    lead = false;
                } else if (c >= (8-tailsize)) break;
                else {
                    lead = false;
					*d++ = *s;
					c++;
				}
				s++;
			}
			if (tailsize != 0) {
				for (c=0;c < tailsize;c++) *d++ = tail[c];
			}
			lead = false;
			if (s == ext) { /* ext points to char after '.' */
				if (*s != 0) { /* anything after? */
					*d++ = '.';
					c = 0;
					while (*s != 0) {
						if (!lead && ((IS_PC98_ARCH && shiftjis_lead_byte(*s & 0xFF)) || (isDBCSCP() && isKanji1_gbk(*s & 0xFF)))) {
                            if (c >= 2) break;
                            lead = true;
                            *d++ = *s;
                            c++;
						} else if ((unsigned char)*s <= 32 || (unsigned char)*s == 127 || *s == '.' || *s == '\"' || *s == '+' || *s == '=' || *s == ',' || *s == ';' || *s == ':' || *s == '<' || *s == '>' || ((*s == '[' || *s == ']' || *s == '|' || *s == '\\')&&(!lead||((dos.loaded_codepage==936||IS_PDOSV)&&!gbk)))||*s=='?'||*s=='*') {
                            lead = false;
                        } else if (c >= 3) break;
                        else {
							*d++ = *s;
							c++;
						}
						s++;
					}
				}
			}
			*d = 0;
		}
	}
	return de->length;
}

static bool escape_is_joliet(const unsigned char *esc) {
	if (    !memcmp(esc,"\x25\x2f\x40",3) ||
		!memcmp(esc,"\x25\x2f\x43",3) ||
		!memcmp(esc,"\x25\x2f\x45",3)) {
		return true;
	}

	return false;
}

bool isoDrive :: loadImageUDF() {
	uint8_t pvd[COOKED_SECTOR_SIZE];
	UDFextent_ad avdex;

	/* look for the anchor volume descriptor */
	{
		UDFAnchorVolumeDescriptorPointer avdp;
		UDFTagId aid;
		memset(pvd,0,16);
		readSector(pvd,256);
		if (!aid.get(COOKED_SECTOR_SIZE,pvd)) return false;
		if (aid.TagIdentifier != 2/*Anchor volume descriptor*/ || aid.TagLocation != 256) return false;
		avdp.get(aid,COOKED_SECTOR_SIZE,pvd);
		avdex = avdp.MainVolumeDescriptorSequenceExtent;
		if (avdex.ExtentLocation == 0 || avdex.ExtentLength == 0) return false;
	}

	LOG(LOG_MISC,LOG_DEBUG)("UDF Anchor Volume Descriptor points to location=%lu len=%lu",
		(unsigned long)avdex.ExtentLocation,
		(unsigned long)avdex.ExtentLength);
	return false;

	return true;
}

bool isoDrive :: loadImage() {
	uint8_t pvd[COOKED_SECTOR_SIZE];
	unsigned int choose_iso9660 = 0;
	unsigned int choose_joliet = 0;
	unsigned int sector = 16;

	is_rock_ridge = false;
	is_udf = false;
	dataCD = false;

	if (enable_udf && loadImageUDF()) {
		LOG(LOG_MISC,LOG_DEBUG)("ISO: UDF filesystem detected");
		is_udf = true;
		dataCD = true;
		return true;
	}

	/* scan the volume descriptors */
	while (sector < 256) {
		pvd[0] = 0xFF;
		readSector(pvd,sector);

		if (pvd[0] == 1) { // primary volume
			if (!strncmp((char*)(&pvd[1]), "CD001", 5) && pvd[6] == 1) {
				if (choose_iso9660 == 0) {
					choose_iso9660 = sector;
					iso = true;
				}
			}
		}
		else if (pvd[0] == 2) { // supplementary volume (usually Joliet, but not necessarily)
			if (!strncmp((char*)(&pvd[1]), "CD001", 5) && pvd[6] == 1) {
				bool joliet = escape_is_joliet(&pvd[88]);

				if (joliet) {
					if (choose_joliet == 0) {
						choose_joliet = sector;
					}
				}
				else {
					if (choose_iso9660 == 0) {
						choose_iso9660 = sector;
						iso = true;
					}
				}
			}
		}
		else if (pvd[0] == 0xFF) { // terminating descriptor
			break;
		}
		else if (pvd[0] >= 0x20) { // oddly out of range, maybe we're lost
			break;
		}

		// unknown check inherited from existing code
		if (pvd[8] == 1 && !strncmp((char*)(&pvd[9]), "CDROM", 5) && pvd[14] == 1) {
			if (choose_iso9660 == 0) {
				choose_iso9660 = sector;
				iso = false;
			}
		}

		sector++;
	}

	if (choose_joliet && enable_joliet) {
		sector = choose_joliet;
		LOG(LOG_MISC,LOG_DEBUG)("ISO 9660: Choosing Joliet (unicode) volume at sector %u",sector);
		is_joliet = true;
		iso = true;
	}
	else if (choose_iso9660) {
		sector = choose_iso9660;
		LOG(LOG_MISC,LOG_DEBUG)("ISO 9660: Choosing ISO 9660 volume at sector %u",sector);
		is_joliet = false;
	}
	else {
		return false;
	}

	readSector(pvd, sector);
	uint16_t offset = iso ? 156 : 180;
	if (readDirEntry(&this->rootEntry, &pvd[offset], 0) <= 0)
		return false;

	/* read the first root directory entry, look for Rock Ridge and/or System Use Sharing Protocol extensions.
	 * It is rare for Rock Ridge extensions to exist on the Joliet supplementary volume as far as I know.
	 * Furthermore the way this code is currently written, Rock Ridge extensions would be ignored anyway for
	 * Joliet extensions. */
	if (!is_joliet && enable_rock_ridge) {
		if (DATA_LENGTH(this->rootEntry) >= 33 && EXTENT_LOCATION(this->rootEntry) != 0) {
			readSector(pvd, EXTENT_LOCATION(this->rootEntry));

			isoDirEntry rde;
			static_assert(sizeof(rde) >= 255, "isoDirEntry less than 255 bytes");
			if (pvd[0] >= 33) {
				memcpy(&rde,pvd,pvd[0]);
				if (IS_DIR(rde.fileFlags) && (rde.fileIdentLength == 1 && rde.ident[0] == 0x00)/*The "." directory*/) {
					/* Good. Is there a SUSP "SP" signature following the 1-byte file identifier? */
					/* @Wengier: If your RR-based ISO images put the "SP" signature somewhere else, feel free to adapt this code.
					 *           The SUSP protocol says it's supposed to be at byte position 1 of the System User Field
					 *           of the First Directory Record of the root directory. */
					unsigned char *p = &rde.ident[1];/*first byte following file ident */
					unsigned char *f = &pvd[pvd[0]];

					if ((p+7/*SP minimum*/) <= f && p[2] >= 7/*length*/ && p[3] == 1/*version*/ &&
						p[4] == 0xBE && p[5] == 0xEF && !memcmp(p,"SP",2)) {
						/* NTS: The spec counts from 1. We count from zero. Both are provided.
						 *
						 * BP1/BP2  +0   "SP"
						 * BP3      +2   7
						 * BP4      +3   1
						 * BP5/BP6  +4   0xBE 0xEF
						 * BP7      +6   LEN_SKP
						 *
						 * LEN_SKP = number of bytes to skip in System Use field of each directory record
						 *           to get to SUSP entries.
						 *
						 *           @Wengier This is the value you should use when searching for "NM"
						 *           entries, rather than byte by byte scanning.
						 */
						LOG(LOG_MISC,LOG_DEBUG)("ISO 9660: Rock Ridge extensions detected");
						is_rock_ridge = true;
						rr_susp_skip = p[6];
					}
				}
			}
		}
	}

	/* Sanity check: This code does NOT support Rock Ridge extensions when reading the Joliet supplementary volume! */
	if (is_joliet) {
		assert(!is_rock_ridge);
	}

	dataCD = true;
	return true;
}

bool isoDrive :: lookup(isoDirEntry *de, const char *path) {
	if (!dataCD) return false;
	*de = this->rootEntry;
	if (!strcmp(path, "")) return true;
	
	char isoPath[ISO_MAXPATHNAME];
	safe_strncpy(isoPath, path, ISO_MAXPATHNAME);
	strreplace_dbcs(isoPath, '\\', '/');
	
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
				if (!IS_ASSOC(FLAGS2) && ((0 == strncasecmp((char*) de->ident, name, ISO_MAX_FILENAME_LENGTH)) || 0 == strncasecmp((char*) fullname, name, ISO_MAXPATHNAME))) {
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

void isoDrive :: EmptyCache(void) {
	enable_rock_ridge = dos.version.major >= 7 || uselfn;
	enable_joliet = dos.version.major >= 7 || uselfn;
	is_joliet = false;
	this->fileName[0]  = '\0';
	this->discLabel[0] = '\0';
	nextFreeDirIterator = 0;
	memset(dirIterators, 0, sizeof(dirIterators));
	memset(sectorHashEntries, 0, sizeof(sectorHashEntries));
	memset(&rootEntry, 0, sizeof(isoDirEntry));
	safe_strncpy(this->fileName, fileName, CROSS_LEN);
	loadImage();
}
