/*
*
*  Copyright (c) 2018 Shane Krueger. Fixes and upgrades (c) 2023 maxpat78.
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

#include <assert.h>
#include <stdlib.h>
#include <time.h>
#include "dosbox.h"
#include "callback.h"
#include "bios.h"
#include "bios_disk.h"
#include "regs.h"
#include "mem.h"
#include "dos_inc.h" /* for Drives[] */
#include "../dos/drives.h"
#include "mapper.h"
#include "SDL.h"

/*
* imageDiskVHD supports fixed, dynamic, and differential VHD file formats
*
* Public members:  Open(), GetVHDType()
*
* Notes:
* - create instance using Open()
* - code assumes that c/h/s values stored within VHD file are accurate, and does not scan the MBR
* - VHDs with 511 byte footers are not yet supported
* - VHDX files are not supported
* - checksum values are verified; and the backup header is used if the footer cannot be found or has an invalid checksum
* - code does not prevent loading if the VHD is marked as being part of a saved state
* - code prevents loading if parent does not match correct guid
* - code does not prevent loading if parent does not match correct datestamp
* - for differencing disks, parent paths are converted to ASCII; unicode characters will cancel loading
* - differencing disks only support absolute paths on Windows platforms
*
*/

imageDiskVHD::ErrorCodes imageDiskVHD::Open(const char* fileName, const bool readOnly, imageDisk** disk) {
	return Open(fileName, readOnly, disk, 0);
}

FILE * fopen_lock(const char * fname, const char * mode, bool &readonly);
imageDiskVHD::ErrorCodes imageDiskVHD::Open(const char* fileName, const bool readOnly, imageDisk** disk, const uint8_t* matchUniqueId) {
	//validate input parameters
	if (fileName == NULL) return ERROR_OPENING;
	if (disk == NULL) return ERROR_OPENING;
	//verify that C++ didn't add padding to the structure layout
	assert(sizeof(VHDFooter) == 512);
	assert(sizeof(DynamicHeader) == 1024);
	//open file and clear C++ buffering
	bool roflag = readOnly;
	FILE* file = fopen_lock(fileName, readOnly ? "rb" : "rb+", roflag);
	if (!file) return ERROR_OPENING;
	setbuf(file, NULL);
	//check that length of file is > 512 bytes
	if (fseeko64(file, 0L, SEEK_END)) { fclose(file); return INVALID_DATA; }
	if (ftello64(file) < 512) { fclose(file); return INVALID_DATA; }
	//read footer
	if (fseeko64(file, -512L, SEEK_CUR)) { fclose(file); return INVALID_DATA; }
	uint64_t footerPosition = (uint64_t)((int64_t)ftello64(file)); /* make sure to sign extend! */
	if ((int64_t)footerPosition < 0LL) { fclose(file); return INVALID_DATA; }
	VHDFooter originalfooter;
	VHDFooter footer;
	if (fread(&originalfooter, 512, 1, file) != 1) { fclose(file); return INVALID_DATA; }
	//convert from big-endian if necessary
	footer = originalfooter;
	footer.SwapByteOrder();
	//verify checksum on footer
	if (!footer.IsValid()) {
		//if invalid, read header, and verify checksum on header
		if (fseeko64(file, 0L, SEEK_SET)) { fclose(file); return INVALID_DATA; }
		if (fread(&originalfooter, sizeof(uint8_t), 512, file) != 512) { fclose(file); return INVALID_DATA; }
		//convert from big-endian if necessary
		footer = originalfooter;
		footer.SwapByteOrder();
		//verify checksum on header
		if (!footer.IsValid()) { fclose(file); return INVALID_DATA; }
	}
	//check that uniqueId matches
	if (matchUniqueId && memcmp(matchUniqueId, footer.uniqueId, 16)) { fclose(file); return INVALID_MATCH; }
	//calculate disk size
	uint64_t calcDiskSize = (uint64_t)footer.geometry.cylinders * (uint64_t)footer.geometry.heads * (uint64_t)footer.geometry.sectors * (uint64_t)512;
    if (!calcDiskSize) { fclose(file); return INVALID_DATA; }

    //set up imageDiskVHD here
    imageDiskVHD* vhd = new imageDiskVHD();
    vhd->footerPosition = footerPosition;
    vhd->footer = footer;
    vhd->vhdType = footer.diskType;
    vhd->originalFooter = originalfooter;
    vhd->cylinders = footer.geometry.cylinders;
    vhd->heads = footer.geometry.heads;
    vhd->sectors = footer.geometry.sectors;
    vhd->sector_size = 512;
    vhd->diskSizeK = calcDiskSize / 1024;
    vhd->diskimg = file;
    vhd->diskname = fileName;
    vhd->hardDrive = true;
    vhd->active = true;
    //use delete vhd from now on to release the disk image upon failure

    //if fixed image, store a plain imageDisk also
	if (footer.diskType == VHD_TYPE_FIXED) {
		//make sure that the image size is at least as big as the geometry
		if (calcDiskSize > footerPosition) {
			fclose(file);
			return INVALID_DATA;
		}
        vhd->fixedDisk = new imageDisk(file, fileName, footer.geometry.cylinders, footer.geometry.heads, footer.geometry.sectors, 512, true);
        *disk = vhd;
		return !readOnly && roflag ? UNSUPPORTED_WRITE : OPEN_SUCCESS;
	}

	//if not dynamic or differencing, fail
	if (footer.diskType != VHD_TYPE_DYNAMIC && footer.diskType != VHD_TYPE_DIFFERENCING) {
		delete vhd;
		return UNSUPPORTED_TYPE;
	}
	//read dynamic disk header (applicable for dynamic and differencing types)
	DynamicHeader dynHeader;
	if (fseeko64(file, (off_t)footer.dataOffset, SEEK_SET)) { delete vhd; return INVALID_DATA; }
	if (fread(&dynHeader, sizeof(uint8_t), 1024, file) != 1024) { delete vhd; return INVALID_DATA; }
	//swap byte order and validate checksum
	dynHeader.SwapByteOrder();
	if (!dynHeader.IsValid()) { delete vhd; return INVALID_DATA; }
	//check block size is valid (should be a power of 2, and at least equal to one sector)
	if (dynHeader.blockSize < 512 || (dynHeader.blockSize & (dynHeader.blockSize - 1))) { delete vhd; return INVALID_DATA; }

	//if differencing, try to open parent
	if (footer.diskType == VHD_TYPE_DIFFERENCING) {

		//loop through each reference and try to find one we can open
		for (int i = 0; i < 8; i++) {
			//ignore entries with platform code 'none'
			if (dynHeader.parentLocatorEntry[i].platformCode != 0) {
				//load the platform data, if there is any
				uint64_t dataOffset = dynHeader.parentLocatorEntry[i].platformDataOffset;
				uint32_t dataLength = dynHeader.parentLocatorEntry[i].platformDataLength;
				uint8_t* buffer = 0;
				if (dataOffset && dataLength && ((uint64_t)dataOffset + dataLength) <= footerPosition) {
					if (fseeko64(file, (off_t)dataOffset, SEEK_SET)) { delete vhd; return INVALID_DATA; }
					buffer = (uint8_t*)malloc(dataLength + 2);
					if (buffer == 0) { delete vhd; return INVALID_DATA; }
					if (fread(buffer, sizeof(uint8_t), dataLength, file) != dataLength) { free(buffer); delete vhd; return INVALID_DATA; }
					buffer[dataLength] = 0; //append null character, just in case
					buffer[dataLength + 1] = 0; //two bytes, because this might be UTF-16
				}
				ErrorCodes ret = TryOpenParent(fileName, dynHeader.parentLocatorEntry[i], buffer, buffer ? dataLength : 0, &(vhd->parentDisk), dynHeader.parentUniqueId);
				if (buffer) free(buffer);
				if (vhd->parentDisk) { //if successfully opened
					vhd->parentDisk->Addref();
					break;
				}
				if (ret != ERROR_OPENING) {
					delete vhd;
					return (ErrorCodes)(ret | PARENT_ERROR);
				}
			}
		}
		//check and see if we were successful in opening a file
		if (vhd->parentDisk == 0) { delete vhd; return ERROR_OPENING_PARENT; }
	}

	//calculate sectors per block
	uint32_t sectorsPerBlock = dynHeader.blockSize / 512;
	//calculate block map size
	uint32_t blockMapSectors = ((
		((sectorsPerBlock /* 4096 sectors/block (typ) = 4096 bits required */
		+ 7) / 8) /* convert to bytes and round up to nearest byte; 4096/8 = 512 bytes required */
		+ 511) / 512); /* convert to sectors and round up to nearest sector; 512/512 = 1 sector */
	//check that the BAT is large enough for the disk
	uint32_t tablesRequired = (uint32_t)((calcDiskSize + (dynHeader.blockSize - 1)) / dynHeader.blockSize);
	if (dynHeader.maxTableEntries < tablesRequired) { delete vhd; return INVALID_DATA; }
	//check that the BAT is contained within the file
	if (((uint64_t)dynHeader.tableOffset + ((uint64_t)dynHeader.maxTableEntries * (uint64_t)4)) > footerPosition) { delete vhd; return INVALID_DATA; }

	//set remaining variables
	vhd->dynamicHeader = dynHeader;
	vhd->blockMapSectors = blockMapSectors;
	vhd->blockMapSize = blockMapSectors * 512;
	vhd->sectorsPerBlock = sectorsPerBlock;
	vhd->currentBlockDirtyMap = (uint8_t*)malloc(vhd->blockMapSize);
	if (vhd->currentBlockDirtyMap == 0) { delete vhd; return INVALID_DATA; }

	//try loading the first block
	if (!vhd->loadBlock(0)) { 
		delete vhd; 
		return INVALID_DATA; 
	}

	*disk = vhd;
	return !readOnly && roflag ? UNSUPPORTED_WRITE : OPEN_SUCCESS;
}

int iso8859_1_encode(int utf32code) {
	return ((utf32code >= 32 && utf32code <= 126) || (utf32code >= 160 && utf32code <= 255)) ? utf32code : -1;
}

//this function (1) converts data from UTF-16 to a native string for fopen (depending on the host OS), and (2) converts slashes, if necessary
bool imageDiskVHD::convert_UTF16_for_fopen(std::string &string, const void* data, const uint32_t dataLength) {
	//note: data is UTF-16 and always little-endian, with no null terminator
	//dataLength is not the number of characters, but the number of bytes

	string.reserve((size_t)(string.size() + (dataLength / 2) + 10)); //allocate a few extra bytes
	char* indata = (char*)data;
    const char* lastchar = indata + dataLength;
#if !defined (WIN32) && !defined(OS2)
	char temp[10];
	char* tempout;
	char* tempout2;
	char* tempoutmax = &temp[10];
#endif
	while (indata < lastchar) {
		//decode the character
		int utf32code = utf16le_decode((const char**)&indata, lastchar);
		if (utf32code < 0) return false;
#if defined (WIN32) || defined(OS2)
		//MSDN docs define fopen to accept strings in the windows default code page, which is typically Windows-1252
		//convert unicode string to ISO-8859, which is a subset of Windows-1252, and a lot easier to implement
		int iso8859_1code = iso8859_1_encode(utf32code);
		if (iso8859_1code < 0) return false;
		//and note that backslashes stay as backslashes on windows
		string += (char)iso8859_1code;
#else
		//backslashes become slashes on Linux
		if (utf32code == '\\') utf32code = '/';
		//Linux ext3 filenames are byte strings, typically in UTF-8
		//encode the character to a temporary buffer
		tempout = temp;
		if (utf8_encode(&tempout, tempoutmax, (unsigned int)utf32code) < 0) return false;
		//and append the byte(s) to the string
		tempout2 = temp;
		while (tempout2 < tempout) string += *tempout2++;
#endif
	}
	return true;
}

imageDiskVHD::ErrorCodes imageDiskVHD::TryOpenParent(const char* childFileName, const imageDiskVHD::ParentLocatorEntry& entry, const uint8_t* data, const uint32_t dataLength, imageDisk** disk, const uint8_t* uniqueId) {
	std::string str = "";
	const char* slashpos = NULL;

	switch (entry.platformCode) {
	case 0x57327275:
		//Unicode relative pathname (UTF-16) on Windows

#if defined (WIN32) || defined(OS2)
		/* Windows */
		slashpos = strrchr(childFileName, '\\');
#else
		/* Linux */
		slashpos = strrchr(childFileName, '/');
#endif
		if (slashpos != NULL) {
			//copy all characters up to and including the slash, to str
            for (const char* pos = (char*)childFileName; pos <= slashpos; pos++) {
				str += *pos;
			}
		}

		//convert byte order, and UTF-16 to ASCII, and change backslashes to slashes if on Linux
		if (!convert_UTF16_for_fopen(str, data, dataLength)) break;

		return imageDiskVHD::Open(str.c_str(), true, disk, uniqueId);

	case 0x57326B75:
		//Unicode absolute pathname (UTF-16) on Windows

#if defined (WIN32) || defined(OS2)
		/* nothing */
#else
		// Linux
		// Todo: convert absolute pathname to something applicable for Linux
		break;
#endif

		//convert byte order, and UTF-16 to ASCII, and change backslashes to slashes if on Linux
		if (!convert_UTF16_for_fopen(str, data, dataLength)) break;

		return imageDiskVHD::Open(str.c_str(), true, disk, uniqueId);

	case 0x4D616320:
		//Mac OS alias stored as blob
		break;
	case 0x4D616358:
		//Mac OSX file url (RFC 2396)
		break;
	}
	return ERROR_OPENING; //return ERROR_OPENING if the file does not exist, cannot be accessed, etc
}

uint8_t imageDiskVHD::Read_AbsoluteSector(uint32_t sectnum, void * data) {
    if(vhdType == VHD_TYPE_FIXED) return fixedDisk->Read_AbsoluteSector(sectnum, data);
	uint32_t blockNumber = sectnum / sectorsPerBlock;
	uint32_t sectorOffset = sectnum % sectorsPerBlock;
	if (!loadBlock(blockNumber)) return 0x05; //can't load block
	if (currentBlockAllocated) {
		uint32_t byteNum = sectorOffset / 8;
		uint32_t bitNum = sectorOffset % 8;
		bool hasData = currentBlockDirtyMap[byteNum] & (1 << (7 - bitNum));
		if (hasData) {
			if (fseeko64(diskimg, (off_t)(((uint64_t)currentBlockSectorOffset + blockMapSectors + sectorOffset) * 512ull), SEEK_SET)) return 0x05; //can't seek
			if (fread(data, sizeof(uint8_t), 512, diskimg) != 512) return 0x05; //can't read
			return 0;
		}
	}
    //NOTE: should come first?
	if (parentDisk) {
		return parentDisk->Read_AbsoluteSector(sectnum, data);
	}
	else {
		memset(data, 0, 512);
		return 0;
	}
}

bool imageDiskVHD::is_zeroed_sector(const void* data) {
    uint32_t* p = (uint32_t*) data;
    uint8_t* q = ((uint8_t*)data + 512);
    while((void*)p < (void*)q && *p++ == 0);
    if((void*)p < (void*)q) return false;
    return true;
}

uint8_t imageDiskVHD::Write_AbsoluteSector(uint32_t sectnum, const void * data) {
    if(vhdType == VHD_TYPE_FIXED) return fixedDisk->Write_AbsoluteSector(sectnum, data);
	uint32_t blockNumber = sectnum / sectorsPerBlock;
	uint32_t sectorOffset = sectnum % sectorsPerBlock;
	if (!loadBlock(blockNumber)) return 0x05; //can't load block
	if (!currentBlockAllocated) {
        //an unallocated block is kept virtualized until zeroed
        if(is_zeroed_sector(data)) return 0;

		if (!copiedFooter) {
			//write backup of footer at start of file (should already exist, but we never checked to be sure it is readable or matches the footer we used)
			if (fseeko64(diskimg, (off_t)0, SEEK_SET)) return 0x05;
			if (fwrite(&originalFooter, sizeof(uint8_t), 512, diskimg) != 512) return 0x05;
			copiedFooter = true;
			//flush the data to disk after writing the backup footer
			if (fflush(diskimg)) return 0x05;
		}
		//calculate new location of footer, and round up to nearest 512 byte increment "just in case"
        uint64_t newFooterPosition = (((footerPosition + blockMapSize + dynamicHeader.blockSize) + 511ull) / 512ull) * 512ull;
		//attempt to extend the length appropriately first (on some operating systems this will extend the file)
		if (fseeko64(diskimg, (off_t)newFooterPosition + 512, SEEK_SET)) return 0x05;
		//now write the footer
		if (fseeko64(diskimg, (off_t)newFooterPosition, SEEK_SET)) return 0x05;
		if (fwrite(&originalFooter, sizeof(uint8_t), 512, diskimg) != 512) return 0x05;
		//save the new block location and new footer position
		uint32_t newBlockSectorNumber = (uint32_t)((footerPosition + 511ul) / 512ul);
		footerPosition = newFooterPosition;
		//clear the dirty flags for the new footer position
		for (uint32_t i = 0; i < blockMapSize; i++) currentBlockDirtyMap[i] = 0;
		//write the dirty map
		if (fseeko64(diskimg, (off_t)(newBlockSectorNumber * 512ull), SEEK_SET)) return 0x05;
		if (fwrite(currentBlockDirtyMap, sizeof(uint8_t), blockMapSize, diskimg) != blockMapSize) return 0x05;
		//flush the data to disk after expanding the file, before allocating the block in the BAT
		if (fflush(diskimg)) return 0x05;
		//update the BAT
		if (fseeko64(diskimg, (off_t)(dynamicHeader.tableOffset + (blockNumber * 4ull)), SEEK_SET)) return 0x05;
		uint32_t newBlockSectorNumberBE = SDL_SwapBE32(newBlockSectorNumber);
		if (fwrite(&newBlockSectorNumberBE, sizeof(uint8_t), 4, diskimg) != 4) return false;
		currentBlockAllocated = true;
		currentBlockSectorOffset = newBlockSectorNumber;
		//flush the data to disk after allocating a block
		if (fflush(diskimg)) return 0x05;
	}
	//current block has now been allocated
	uint32_t byteNum = sectorOffset / 8;
	uint32_t bitNum = sectorOffset % 8;
	bool hasData = currentBlockDirtyMap[byteNum] & (1 << (7 - bitNum));
	//if the sector hasn't been marked as dirty, mark it as dirty
	if (!hasData) {
		currentBlockDirtyMap[byteNum] |= 1 << (7 - bitNum);
		if (fseeko64(diskimg, (off_t)(currentBlockSectorOffset * 512ull), SEEK_SET)) return 0x05; //can't seek
		if (fwrite(currentBlockDirtyMap, sizeof(uint8_t), blockMapSize, diskimg) != blockMapSize) return 0x05;
	}
	//current sector has now been marked as dirty
	//write the sector
	if (fseeko64(diskimg, (off_t)(((uint64_t)currentBlockSectorOffset + (uint64_t)blockMapSectors + (uint64_t)sectorOffset) * 512ull), SEEK_SET)) return 0x05; //can't seek
	if (fwrite(data, sizeof(uint8_t), 512, diskimg) != 512) return 0x05; //can't write
	return 0;
}

imageDiskVHD::VHDTypes imageDiskVHD::GetVHDType(void) const {
	return footer.diskType;
}

imageDiskVHD::VHDTypes imageDiskVHD::GetVHDType(const char* fileName) {
	imageDisk* disk;
	if (Open(fileName, true, &disk)) return VHD_TYPE_NONE;
	const imageDiskVHD* vhd = dynamic_cast<imageDiskVHD*>(disk);
	VHDTypes ret = VHD_TYPE_FIXED;
	if (vhd) ret = vhd->footer.diskType; //get the actual type
	delete disk;
	return ret;
}

bool imageDiskVHD::loadBlock(const uint32_t blockNumber) {
	if (currentBlock == blockNumber) return true;
	if (blockNumber >= dynamicHeader.maxTableEntries) return false;
	if (fseeko64(diskimg, (off_t)(dynamicHeader.tableOffset + (blockNumber * 4ull)), SEEK_SET)) return false;
	uint32_t blockSectorOffset;
	if (fread(&blockSectorOffset, sizeof(uint8_t), 4, diskimg) != 4) return false;
	blockSectorOffset = SDL_SwapBE32(blockSectorOffset);
	if (blockSectorOffset == 0xFFFFFFFFul) {
		currentBlock = blockNumber;
		currentBlockAllocated = false;
	}
	else {
		if (fseeko64(diskimg, (off_t)(blockSectorOffset * (uint64_t)512), SEEK_SET)) return false;
		currentBlock = 0xFFFFFFFFul;
		currentBlockAllocated = true;
		currentBlockSectorOffset = blockSectorOffset;
		if (fread(currentBlockDirtyMap, sizeof(uint8_t), blockMapSize, diskimg) != blockMapSize) return false;
		currentBlock = blockNumber;
	}
	return true;
}

imageDiskVHD::~imageDiskVHD() {
	if (currentBlockDirtyMap) {
		free(currentBlockDirtyMap);
		currentBlockDirtyMap = 0;
	}
	if (parentDisk) {
		parentDisk->Release();
		parentDisk = 0;
	}
    if(fixedDisk) {
        delete fixedDisk;
        fixedDisk = 0;
    }
}

void imageDiskVHD::VHDFooter::SwapByteOrder() {
	features = SDL_SwapBE32(features);
	fileFormatVersion = SDL_SwapBE32(fileFormatVersion);
	dataOffset = SDL_SwapBE64(dataOffset);
	timeStamp = SDL_SwapBE32(timeStamp);
	creatorVersion = SDL_SwapBE32(creatorVersion);
	creatorHostOS = SDL_SwapBE32(creatorHostOS);
	originalSize = SDL_SwapBE64(originalSize);
	currentSize = SDL_SwapBE64(currentSize);
	geometry.cylinders = SDL_SwapBE16(geometry.cylinders);
	diskType = (VHDTypes)SDL_SwapBE32((uint32_t)diskType);
	checksum = SDL_SwapBE32(checksum);
	//guid might need the byte order swapped also
	//however, for our purposes (comparing to the parent guid on differential disks),
	//  it doesn't matter so long as we are consistent
}

uint32_t imageDiskVHD::VHDFooter::CalculateChecksum() {
	//checksum is one's complement of sum of bytes excluding the checksum
	//because of that, the byte order doesn't matter when calculating the checksum
	//however, the checksum must be stored in the correct byte order or it will not match

	uint32_t ret = 0;
    const uint8_t* dat = (uint8_t*)this->cookie;
	uint32_t oldChecksum = checksum;
	checksum = 0;
	for (size_t i = 0; i < sizeof(VHDFooter); i++) {
		ret += dat[i];
	}
	checksum = oldChecksum;
	return ~ret;
}

bool imageDiskVHD::VHDFooter::IsValid() {
	return (
		memcmp(cookie, "conectix", 8) == 0 &&
		fileFormatVersion >= 0x00010000 &&
		fileFormatVersion <= 0x0001FFFF &&
		checksum == CalculateChecksum());
}

void imageDiskVHD::VHDFooter::SetDefaults() {
    memset(this, 0, 512);
    memcpy(cookie, "conectix", 8);
    features = 2;
    fileFormatVersion = 0x10000;
    dataOffset = 512;
    time_t T;
    time(&T);
    timeStamp = mktime(gmtime(&T)) - 946681200; // ss from 1/1/2000 UTC
    memcpy(creatorApp, "DBox", 4);
    creatorVersion = 0x10000;
    creatorHostOS = 0x5769326B; // Wi2k
}

void imageDiskVHD::DynamicHeader::SwapByteOrder() {
	dataOffset = SDL_SwapBE64(dataOffset);
	tableOffset = SDL_SwapBE64(tableOffset);
	headerVersion = SDL_SwapBE32(headerVersion);
	maxTableEntries = SDL_SwapBE32(maxTableEntries);
	blockSize = SDL_SwapBE32(blockSize);
	checksum = SDL_SwapBE32(checksum);
	parentTimeStamp = SDL_SwapBE32(parentTimeStamp);
	for (int i = 0; i < 256; i++) {
		parentUnicodeName[i] = SDL_SwapBE16(parentUnicodeName[i]);
	}
	for (int i = 0; i < 8; i++) {
		parentLocatorEntry[i].platformCode = SDL_SwapBE32(parentLocatorEntry[i].platformCode);
		parentLocatorEntry[i].platformDataSpace = SDL_SwapBE32(parentLocatorEntry[i].platformDataSpace);
		parentLocatorEntry[i].platformDataLength = SDL_SwapBE32(parentLocatorEntry[i].platformDataLength);
		parentLocatorEntry[i].reserved = SDL_SwapBE32(parentLocatorEntry[i].reserved);
		parentLocatorEntry[i].platformDataOffset = SDL_SwapBE64(parentLocatorEntry[i].platformDataOffset);
	}
	//parent guid might need the byte order swapped also
	//however, for our purposes (comparing to the parent guid on differential disks),
	//  it doesn't matter so long as we are consistent
}

uint32_t imageDiskVHD::DynamicHeader::CalculateChecksum() {
	//checksum is one's complement of sum of bytes excluding the checksum
	//because of that, the byte order doesn't matter when calculating the checksum
	//however, the checksum must be stored in the correct byte order or it will not match

	uint32_t ret = 0;
    const uint8_t* dat = (uint8_t*)this->cookie;
	uint32_t oldChecksum = checksum;
	checksum = 0;
	for (size_t i = 0; i < sizeof(DynamicHeader); i++) {
		ret += dat[i];
	}
	checksum = oldChecksum;
	return ~ret;
}

bool imageDiskVHD::DynamicHeader::IsValid() {
    return (
        memcmp(cookie, "cxsparse", 8) == 0 &&
        headerVersion >= 0x00010000 &&
        headerVersion <= 0x0001FFFF &&
        checksum == CalculateChecksum());
}

void imageDiskVHD::DynamicHeader::SetDefaults() {
    memset(this, 0, 1024);
    memcpy(cookie, "cxsparse", 8);
    dataOffset = 0xFFFFFFFFFFFFFFFF;
    tableOffset = 1536;
    headerVersion = 0x10000;
    blockSize = (2 << 20);
}

//fills a buffer with a random 16-bytes UUID
void imageDiskVHD::mk_uuid(uint8_t* buf) {
    srand(time(NULL));
    for(uint16_t* r = (uint16_t*)buf; r < (uint16_t*)(buf + 16); r++)
        *r = rand();
}
//updates UUID (i.e. for a merged parent disk)
bool imageDiskVHD::UpdateUUID() {
    mk_uuid((uint8_t*)footer.uniqueId);
    footer.checksum = footer.CalculateChecksum();
    footer.SwapByteOrder();
    memcpy(&originalFooter, &footer, 512);
    footer.SwapByteOrder();
    if (fseeko64(diskimg, footerPosition, SEEK_SET)) return false;
    if (fwrite(&originalFooter, 1, 512, diskimg) != 512) return false;
    if(vhdType != VHD_TYPE_FIXED) {
        if(fseeko64(diskimg, 0, SEEK_SET)) return false;
        if(fwrite(&originalFooter, 1, 512, diskimg) != 512) return false;
    }
    return true;
}

//computates pseudo CHS geometry according to MS VHD specification
void imageDiskVHD::SizeToCHS(uint64_t size, uint16_t* c, uint8_t* h, uint8_t* s)
{
    uint32_t sectors = size / 512;
    uint32_t spt, hh;
    uint32_t cth, cyls;

    if(sectors > 65535 * 16 * 255)
        sectors = 65535 * 16 * 255;
    if(sectors >= 65535 * 16 * 63) {
        spt = 255;
        hh = 16;
        cth = sectors / spt;
    }
    else {
        spt = 17;
        cth = sectors / spt;
        hh = (cth + 1023) / 1024;
        if(hh < 4) hh = 4;
        if(cth >= hh * 1024 || hh > 16) {
            spt = 31;
            hh = 16;
            cth = sectors / spt;
        }
        if(cth >= hh * 1024) {
            spt = 63;
            hh = 16;
            cth = sectors / spt;
        }
    }
    cyls = cth / hh;
    *c = (uint16_t) cyls;
    *h = (uint8_t) hh;
    *s = (uint8_t) spt;
}

//creates a Dynamic VHD image
uint32_t imageDiskVHD::CreateDynamic(const char* filename, uint64_t size) {
    uint32_t STATUS = OPEN_SUCCESS;
    if(filename == NULL) return ERROR_OPENING;
    if(size < 3145728 || size > 2190433320960) // 2040GB is the Windows 11 mounter limit
        return UNSUPPORTED_SIZE;
    FILE* vhd = fopen(filename, "wb");
    if(!vhd) return ERROR_OPENING;

    //setup footer
    VHDFooter footer;
    footer.SetDefaults();
    footer.originalSize = footer.currentSize = size;
    SizeToCHS(size, &footer.geometry.cylinders, &footer.geometry.heads, &footer.geometry.sectors);
    footer.diskType = VHD_TYPE_DYNAMIC;
    mk_uuid((uint8_t*)footer.uniqueId);
    footer.checksum = footer.CalculateChecksum();
    footer.SwapByteOrder();

    //write footer copy
    if (fwrite(&footer, 1, 512, vhd) != 512) STATUS = ERROR_WRITING;

    //setup dynamic header
    DynamicHeader header;
    header.SetDefaults();
    uint32_t dwMaxTableEntries = (size + (header.blockSize - 1)) / header.blockSize;
    header.maxTableEntries = dwMaxTableEntries;
    header.checksum = header.CalculateChecksum();
    header.SwapByteOrder();

    //write dynamic header
    if (fwrite(&header, 1, 1024, vhd) != 1024) STATUS = ERROR_WRITING;

    //creates the empty BAT (max 4MB) - must span sectors
    uint8_t sect[512];
    memset(sect, 255, 512);
    uint32_t table_size = (4 * dwMaxTableEntries + 511) / 512 * 512;
    while(table_size && STATUS == OPEN_SUCCESS) {
        if(fwrite(sect, 1, 512, vhd) != 512) {
            STATUS = ERROR_WRITING;
            break;
        }
        table_size -= 512;
    }

    //write main footer
    if(fwrite(&footer, 1, 512, vhd) != 512) STATUS = ERROR_WRITING;
    fclose(vhd);
    return STATUS;
}

//creates a Differencing VHD image
uint32_t imageDiskVHD::CreateDifferencing(const char* filename, const char* basename) {
    if(filename == NULL || basename == NULL) return ERROR_OPENING;
    imageDiskVHD* base_vhd;
    if(Open(basename, true, (imageDisk**)&base_vhd) != OPEN_SUCCESS) return ERROR_OPENING_PARENT;
    FILE* vhd = fopen(filename, "wb");
    if(!vhd) return ERROR_OPENING;
    uint32_t STATUS = OPEN_SUCCESS;

    //clone parent's VHD structures
    VHDFooter footer;
    memcpy(&footer, &base_vhd->footer, 512);
    DynamicHeader header;
    if(base_vhd->vhdType != VHD_TYPE_FIXED) {
        memcpy(&header, &base_vhd->dynamicHeader, 1024);
    }
    else {
        footer.dataOffset = 512;
        header.SetDefaults();
        header.maxTableEntries = (base_vhd->diskSizeK + 2047) / 2048;
    }
    //update
    footer.diskType = VHD_TYPE_DIFFERENCING;
    memcpy(header.parentUniqueId, footer.uniqueId, 16);
    mk_uuid((uint8_t*)footer.uniqueId);
    header.parentTimeStamp = footer.timeStamp;
    time_t T;
    time(&T);
    footer.timeStamp = (uint32_t)T - 946681200;
    footer.checksum = footer.CalculateChecksum();
    footer.SwapByteOrder();

    //write footer copy
    if(fwrite(&footer, 1, 512, vhd) != 512) STATUS = ERROR_WRITING;

    //BAT size
    uint32_t table_size = (4 * header.maxTableEntries + 511) / 512 * 512;

    //Locators - Windows 11 wants at least the relative W2ru locator, or won't mount!
    uint32_t l_basename = strlen(basename);
    uint32_t platsize = (2 * l_basename + 511) / 512 * 512;
    header.parentLocatorEntry[0].platformCode = 0x57326B75; //W2ku
    header.parentLocatorEntry[0].platformDataLength = 2 * l_basename;
    header.parentLocatorEntry[0].platformDataSpace = platsize;
    header.parentLocatorEntry[0].platformDataOffset = 1536 + table_size;

    header.parentLocatorEntry[1].platformCode = 0x57327275; // W2ru
    header.parentLocatorEntry[1].platformDataLength = 2 * l_basename;
    header.parentLocatorEntry[1].platformDataSpace = platsize;
    header.parentLocatorEntry[1].platformDataOffset = 2048 + table_size;

    //write dynamic Header
    header.checksum = header.CalculateChecksum();
    header.SwapByteOrder();
    if(fwrite(&header, 1, 1024, vhd) != 1024) STATUS = ERROR_WRITING;

    //write BAT sectors
    uint8_t sect[512];
    memset(sect, 255, 512);
    while(table_size && STATUS == OPEN_SUCCESS) {
        if(fwrite(sect, 1, 512, vhd) != 512) {
            STATUS = ERROR_WRITING;
            break;
        }
        table_size -= 512;
    }
    //write Parent Locator sectors
    wchar_t* w_basename = (wchar_t*)malloc(platsize);
    memset(w_basename, 0, platsize);
    for(uint32_t i = 0; i < l_basename; i++)
        //dirty hack to quickly convert ASCII -> UTF-16 *LE* and fix slashes
        w_basename[i] = SDL_SwapLE16(basename[i]=='/'? (uint16_t)'\\' : (uint16_t)basename[i]);
    if (fwrite(w_basename, 1, platsize, vhd) != platsize) STATUS = ERROR_WRITING;
    if (fwrite(w_basename, 1, platsize, vhd) != platsize) STATUS = ERROR_WRITING;
 
    //write footer copy
    if(fwrite(&footer, 1, 512, vhd) != 512) STATUS = ERROR_WRITING;

    delete base_vhd;
    free(w_basename);
    fclose(vhd);
    return STATUS;
}

//converts a raw hard disk image into a Fixed VHD
uint32_t imageDiskVHD::ConvertFixed(const char* filename) {
    if(filename == NULL) return ERROR_OPENING;
    FILE* vhd = fopen(filename, "r+b");
    if(vhd == NULL) return ERROR_OPENING;
    fseeko64(vhd, 0, SEEK_END);
    uint64_t size = ftello64(vhd);
    if(size < 3145728 || size > 2190433320960) {
        LOG_MSG("Bad VHD size: valid range 3 MB - 2040 GB");
        fclose(vhd);
        return UNSUPPORTED_SIZE;
    }

    uint8_t mbr[512];
    bool partFound = false;
    uint32_t STATUS = OPEN_SUCCESS;
    uint32_t c, h, s;
    uint32_t heads, spc;
    uint8_t* part;

    //since IMGMOUNT fails when VHD pseudo CHS does not match BPB,
    //we're coerced doing detection ourselves...
 
    //detects recorded geometry (MBR only)
    fseeko64(vhd, 0, SEEK_SET);
    if(fread(mbr, 1, 512, vhd) != 512) STATUS = ERROR_OPENING;
    //search for the first DOS partition
    for(part = mbr + 0x1BE; part < mbr + 0x1FE; part += 16) {
        uint8_t partType = *((uint8_t*)(part + 4));
        if(partType != 1 && partType != 4 && partType != 6 &&
            partType != 11 && partType != 12 && partType != 14) continue;
        partFound = true;
        break;
    }
    if (!partFound) {
        //if there's no DOS partition yet, we create the VHD and MBR geometry
        SizeToCHS(size, (uint16_t*)&c, (uint8_t*)&h, (uint8_t*)&s);
    }
    else {
        //partition end
        h = (unsigned)*(part + 5);
        s = (unsigned)*(part + 6) & 0x3F;
        c = (unsigned)*(part + 6) & 0xC0 | *(part + 7);
        //since a MBR partition *should* be aligned to a cylinder boundary,
        //the last sector CHS reveals total heads and sectors per cylinder
        heads = h + 1;
        spc = s;
        // partition start (0,1,1... what?)
        h = (unsigned)*(part + 1);
        s = (unsigned)*(part + 2) & 0x3F;
        c = (unsigned)*(part + 2) & 0xC0 | *(part + 3);
        uint64_t lba = ((c * heads + h) * spc + s - 1) * 512;
        if(lba < 1 || lba > (size - 512)) {
            LOG_MSG("Bad CHS partition start in MBR");
            lba = 0;
        }
        //prefer LBA, if present (PC-DOS 2.0+ FDISK)
        if(*((uint32_t*)(part + 8))) {
            uint64_t lba2 = SDL_SwapLE32(*((uint32_t*)(part + 8))) * 512;
            if(lba2 < 1 || lba2 > (size - 512)) {
                LOG_MSG("Bad LBA partition start in MBR");
                lba2 = 0;
            }
            if(!lba && !lba2) {
                STATUS = ERROR_OPENING;
            }
            if(lba != lba2) {
                LOG_MSG("CHS and LBA partition start differ, using LBA");
            }
            if(fseeko64(vhd, (uint64_t)lba, SEEK_SET)) STATUS = ERROR_OPENING;
        }

        if(fseeko64(vhd, (uint64_t)lba, SEEK_SET)) STATUS = ERROR_OPENING;
        //load FAT boot sector and scan BPB
        if(fread(mbr, 1, 512, vhd) != 512) STATUS = ERROR_OPENING;
        uint16_t s2 = *((uint16_t*)(mbr + 0x18));
        uint16_t h2 = *((uint16_t*)(mbr + 0x1A));
        if(!s2 || !h2 || s2 > 63 || h2 > 255) {
            LOG_MSG("Bad geometry detected in FAT BPB, using MBR");
            s = spc;
            h = heads;
        }
        else {
            s = s2;
            h = h2;
        }
    }
    //new VHD Footer initialization
    VHDFooter footer;
    footer.SetDefaults();
    footer.dataOffset = 0xFFFFFFFFFFFFFFFF;
    footer.originalSize = footer.currentSize = size;
    c = size / 512 / (h * s);
    footer.geometry.cylinders = (uint16_t)c;
    footer.geometry.heads = (uint8_t)h;
    footer.geometry.sectors = (uint8_t)s;
    footer.diskType = VHD_TYPE_FIXED;
    mk_uuid((uint8_t*)footer.uniqueId);
    footer.checksum = footer.CalculateChecksum();
    footer.SwapByteOrder();

    if(STATUS == OPEN_SUCCESS) {
        if(fseeko64(vhd, 0, SEEK_END)) STATUS = ERROR_WRITING;
        if(fwrite(&footer, 1, 512, vhd) != 512) STATUS = ERROR_WRITING;
    }
    fclose(vhd);
    return STATUS;
}

uint32_t imageDiskVHD::GetInfo(VHDInfo* info) {
    uint32_t STATUS = 0;
    if(info == NULL) info = new VHDInfo();
    info->vhdType = vhdType;
    info->vhdSizeMB = (float)diskSizeK / 1024.0;
    info->diskname = diskname;
    if(vhdType != VHD_TYPE_FIXED) {
        info->blockSize = dynamicHeader.blockSize;
        info->totalBlocks = dynamicHeader.maxTableEntries;
        fseeko64(diskimg, dynamicHeader.tableOffset, SEEK_SET);
        for(int i = 0; i < info->totalBlocks; i++) {
            uint32_t n;
            if(fread(&n, 1, 4, diskimg) != 4) return ERROR_OPENING;
            if(n != 0xFFFFFFFF) info->allocatedBlocks++;
        }
    }
    else {
        info->blockSize = info->totalBlocks = info->allocatedBlocks = 0;
    }
    if(vhdType == VHD_TYPE_DIFFERENCING) {
        info->parentInfo = new VHDInfo();
        STATUS = ((imageDiskVHD*)parentDisk)->GetInfo(info->parentInfo);
    }
    return STATUS;
}

uint32_t imageDiskVHD::GetInfo(const char* filename, VHDInfo** info) {
    imageDiskVHD* vhd;
    if (filename == NULL)
        return ERROR_OPENING;
    if(imageDiskVHD::Open(filename, true, (imageDisk**)&vhd) != imageDiskVHD::OPEN_SUCCESS) {
        return ERROR_OPENING;
    }
    *info = new VHDInfo();
    uint32_t ret = vhd->GetInfo(*info);
    delete vhd;
    return ret;
}

//merge a Differencing disk to its parent
bool imageDiskVHD::MergeSnapshot(uint32_t* totalSectorsMerged, uint32_t* totalBlocksUpdated) {
    if(totalSectorsMerged == NULL || totalBlocksUpdated == NULL) return false;
    if(vhdType != VHD_TYPE_DIFFERENCING) {
        LOG_MSG("VHD is not Differencing, can't merge!");
        return false;
    }
    std::string name = parentDisk->diskname;
    parentDisk->Release();
    if(Open(name.c_str(), false, &parentDisk) != OPEN_SUCCESS) {
        LOG_MSG("Couldn't re-open parent in RW mode!");
        return false;
    }
    parentDisk->Addref();
    //scan BAT
    uint32_t sectorsPerBlock = dynamicHeader.blockSize / 512;
    *totalSectorsMerged = 0;
    *totalBlocksUpdated = 0;
    for(uint32_t block = 0; block < dynamicHeader.maxTableEntries; block++) {
        uint32_t n;
        fseeko64(diskimg, dynamicHeader.tableOffset+block*4, SEEK_SET);
        if(fread(&n, 1, 4, diskimg) != 4) { return false; }
        if(n == 0xFFFFFFFF) continue;
        loadBlock(block);
        bool blockUpdated = false;
        //scan bitmap
        for(uint32_t sector = 0; sector < sectorsPerBlock; sector++) {
            uint32_t byteNum = sector / 8;
            uint32_t bitNum = sector % 8;
            bool hasData = currentBlockDirtyMap[byteNum] & (1 << (7 - bitNum));
            if(hasData) {
                uint8_t data[512];
                if(fseeko64(diskimg, (off_t)(((uint64_t)currentBlockSectorOffset + blockMapSectors + sector) * 512ull), SEEK_SET)) return false;
                if(fread(data, sizeof(uint8_t), 512, diskimg) != 512) return false;
                uint32_t absoluteSector = block * sectorsPerBlock + sector;
                //LOG_MSG("Merging sector %d", absoluteSector);
                (*totalSectorsMerged)++;
                blockUpdated = true;
                if(parentDisk->Write_AbsoluteSector(absoluteSector, data)) {
                    LOG_MSG("Couldn't update parent's sector %d, merging aborted!", absoluteSector);
                    return false;
                }
            }
        }
        if(blockUpdated) (*totalBlocksUpdated)++;
    }
    LOG_MSG("Merged %d sectors in %d blocks", *totalSectorsMerged, *totalBlocksUpdated);
    if (! ((imageDiskVHD*)parentDisk)->UpdateUUID() )
        LOG_MSG("Warning: parent UUID not updated, invalid childs could exist around!");
    return true;
}

//creates a snapshot of current VHD state, with a random name
uint32_t imageDiskVHD::CreateSnapshot() {
    uint8_t buf[16];
    mk_uuid(buf);
    char name[255];
    sprintf(name, "{%08x-%08x-%08x-%08x}.vhd", *((uint32_t*)(buf)), *((uint32_t*)(buf+4)), *((uint32_t*)(buf+8)), *((uint32_t*)(buf+12)));
    uint32_t ret = CreateDifferencing(name, diskname.c_str());
    return ret;
}
