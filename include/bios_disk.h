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

#ifndef DOSBOX_BIOS_DISK_H
#define DOSBOX_BIOS_DISK_H

#include <stdio.h>
#include <stdlib.h>
#ifndef DOSBOX_MEM_H
#include "mem.h"
#endif
#ifndef DOSBOX_DOS_INC_H
#include "dos_inc.h"
#endif
#ifndef DOSBOX_BIOS_H
#include "bios.h"
#endif

/* The Section handling Bios Disk Access */
#define BIOS_MAX_DISK 10

#define MAX_SWAPPABLE_DISKS 20
struct diskGeo {
	Bit32u ksize;  /* Size in kilobytes */
	Bit16u secttrack; /* Sectors per track */
	Bit16u headscyl;  /* Heads per cylinder */
	Bit16u cylcount;  /* Cylinders per side */
	Bit16u biosval;   /* Type to return from BIOS */
    Bit16u bytespersect; /* Bytes per sector */
	Bit16u rootentries;  /* Root directory entries */
	Bit8u sectcluster;   /* Sectors per cluster */
	Bit8u mediaid;       /* Media ID */
};
extern diskGeo DiskGeometryList[];

extern const Bit8u freedos_mbr[];

class imageDisk {
public:
	enum IMAGE_TYPE {
		ID_BASE=0,
		ID_EL_TORITO_FLOPPY,
        ID_VFD,
		ID_MEMORY,
		ID_VHD,
        ID_D88,
        ID_NFD
	};

	virtual Bit8u Read_Sector(Bit32u head,Bit32u cylinder,Bit32u sector,void * data,unsigned int req_sector_size=0);
	virtual Bit8u Write_Sector(Bit32u head,Bit32u cylinder,Bit32u sector,const void * data,unsigned int req_sector_size=0);
	virtual Bit8u Read_AbsoluteSector(Bit32u sectnum, void * data);
	virtual Bit8u Write_AbsoluteSector(Bit32u sectnum, const void * data);

	virtual void Set_Reserved_Cylinders(Bitu resCyl);
	virtual Bit32u Get_Reserved_Cylinders();
	virtual void Set_Geometry(Bit32u setHeads, Bit32u setCyl, Bit32u setSect, Bit32u setSectSize);
	virtual void Get_Geometry(Bit32u * getHeads, Bit32u *getCyl, Bit32u *getSect, Bit32u *getSectSize);
	virtual Bit8u GetBiosType(void);
	virtual Bit32u getSectSize(void);
	imageDisk(FILE *imgFile, Bit8u *imgName, Bit32u imgSizeK, bool isHardDisk);
	imageDisk(FILE* diskimg, const char* diskName, Bit32u cylinders, Bit32u heads, Bit32u sectors, Bit32u sector_size, bool hardDrive);
	virtual ~imageDisk() { if(diskimg != NULL) { fclose(diskimg); diskimg=NULL; } };

    IMAGE_TYPE class_id = ID_BASE;
	std::string diskname;
    bool active = false;
    Bit32u sector_size = 512;
    Bit32u heads = 0;
    Bit32u cylinders = 0;
    Bit32u sectors = 0;
    bool hardDrive = false;
    Bit64u diskSizeK = 0;

protected:
	imageDisk(IMAGE_TYPE class_id);
    FILE* diskimg = NULL;
    Bit8u floppytype = 0;

    Bit32u reserved_cylinders = 0;
    Bit64u image_base = 0;
    Bit64u image_length = 0;

private:
    volatile int refcount = 0;

public:
	int Addref() {
		return ++refcount;
	}
	int Release() {
		int ret = --refcount;
		if (ret < 0) {
			fprintf(stderr,"WARNING: imageDisk Release() changed refcount to %d\n",ret);
			abort();
		}
		if (ret == 0) delete this;
		return ret;
	}
};

class imageDiskD88 : public imageDisk {
public:
	virtual Bit8u Read_Sector(Bit32u head,Bit32u cylinder,Bit32u sector,void * data,unsigned int req_sector_size=0);
	virtual Bit8u Write_Sector(Bit32u head,Bit32u cylinder,Bit32u sector,const void * data,unsigned int req_sector_size=0);
	virtual Bit8u Read_AbsoluteSector(Bit32u sectnum, void * data);
	virtual Bit8u Write_AbsoluteSector(Bit32u sectnum, const void * data);

	imageDiskD88(FILE *imgFile, Bit8u *imgName, Bit32u imgSizeK, bool isHardDisk);
	virtual ~imageDiskD88();

    unsigned char fd_type_major;
    unsigned char fd_type_minor;

    enum { /* major */
        DISKTYPE_2D			= 0,
        DISKTYPE_2DD,
        DISKTYPE_2HD
    };

    struct vfdentry {
        uint8_t         track,head,sector;
        uint16_t        sector_size;

        uint32_t        data_offset;
        uint32_t        entry_offset; // offset of the 12-byte entry this came from (if nonzero)

        vfdentry() : track(0), head(0), sector(0), sector_size(0), data_offset(0), entry_offset(0) {
        }

        uint16_t getSectorSize(void) const {
            return sector_size;
        }
    };

    vfdentry *findSector(Bit8u head,Bit8u track,Bit8u sector/*TODO: physical head?*/,unsigned int req_sector_size=0);

    std::vector<vfdentry> dents;
};

class imageDiskNFD : public imageDisk {
public:
	virtual Bit8u Read_Sector(Bit32u head,Bit32u cylinder,Bit32u sector,void * data,unsigned int req_sector_size=0);
	virtual Bit8u Write_Sector(Bit32u head,Bit32u cylinder,Bit32u sector,const void * data,unsigned int req_sector_size=0);
	virtual Bit8u Read_AbsoluteSector(Bit32u sectnum, void * data);
	virtual Bit8u Write_AbsoluteSector(Bit32u sectnum, const void * data);

	imageDiskNFD(FILE *imgFile, Bit8u *imgName, Bit32u imgSizeK, bool isHardDisk, unsigned int revision);
	virtual ~imageDiskNFD();

    struct vfdentry {
        uint8_t         track,head,sector;
        uint16_t        sector_size;

        uint32_t        data_offset;
        uint32_t        entry_offset; // offset of the 12-byte entry this came from (if nonzero)

        vfdentry() : track(0), head(0), sector(0), sector_size(0), data_offset(0), entry_offset(0) {
        }

        uint16_t getSectorSize(void) const {
            return sector_size;
        }
    };

    vfdentry *findSector(Bit8u head,Bit8u track,Bit8u sector/*TODO: physical head?*/,unsigned int req_sector_size=0);

    std::vector<vfdentry> dents;
};

class imageDiskVFD : public imageDisk {
public:
	virtual Bit8u Read_Sector(Bit32u head,Bit32u cylinder,Bit32u sector,void * data,unsigned int req_sector_size=0);
	virtual Bit8u Write_Sector(Bit32u head,Bit32u cylinder,Bit32u sector,const void * data,unsigned int req_sector_size=0);
	virtual Bit8u Read_AbsoluteSector(Bit32u sectnum, void * data);
	virtual Bit8u Write_AbsoluteSector(Bit32u sectnum, const void * data);

	imageDiskVFD(FILE *imgFile, Bit8u *imgName, Bit32u imgSizeK, bool isHardDisk);
	virtual ~imageDiskVFD();

    struct vfdentry {
        uint8_t         track,head,sector,sizebyte;
        uint8_t         fillbyte;

        uint32_t        data_offset;
        uint32_t        entry_offset; // offset of the 12-byte entry this came from (if nonzero)

        vfdentry() : track(0), head(0), sector(0), sizebyte(0), fillbyte(0xFF), data_offset(0), entry_offset(0) {
        }

        bool hasSectorData(void) const {
            return fillbyte == 0xFF && data_offset != 0xFFFFFFFFUL;
        }

        bool hasFill(void) const {
            return fillbyte != 0xFF || data_offset == 0xFFFFFFFFUL;
        }

        uint16_t getSectorSize(void) const {
            return 128 << sizebyte;
        }
    };

    vfdentry *findSector(Bit8u head,Bit8u track,Bit8u sector/*TODO: physical head?*/,unsigned int req_sector_size=0);

    std::vector<vfdentry> dents;
};

class imageDiskMemory : public imageDisk {
public:
	virtual Bit8u Read_AbsoluteSector(Bit32u sectnum, void * data);
	virtual Bit8u Write_AbsoluteSector(Bit32u sectnum, const void * data);
	virtual Bit8u GetBiosType(void);
	virtual void Set_Geometry(Bit32u setHeads, Bit32u setCyl, Bit32u setSect, Bit32u setSectSize);
	// Parition and format the ramdrive
	virtual Bit8u Format();

	// Create a hard drive image of a specified size; automatically select c/h/s
	imageDiskMemory(Bit32u imgSizeK);
	// Create a hard drive image of a specified geometry
	imageDiskMemory(Bit16u cylinders, Bit16u heads, Bit16u sectors, Bit16u sector_size);
	// Create a floppy image of a specified geometry
    imageDiskMemory(const diskGeo& floppyGeometry);
	// Create a copy-on-write memory image of an existing image
	imageDiskMemory(imageDisk* underlyingImage);
	virtual ~imageDiskMemory();

private:
	void init(diskGeo diskParams, bool isHardDrive, imageDisk* underlyingImage);
	bool CalculateFAT(Bit32u partitionStartingSector, Bit32u partitionLength, bool isHardDrive, Bit32u rootEntries, Bit32u* rootSectors, Bit32u* sectorsPerCluster, bool* isFat16, Bit32u* fatSectors, Bit32u* reservedSectors);

	Bit8u * * ChunkMap;
	Bit32u sectors_per_chunk;
	Bit32u chunk_size;
	Bit32u total_chunks;
    Bit32u total_sectors = 0;
    imageDisk* underlyingImage = NULL;

	diskGeo floppyInfo;
};

class imageDiskVHD : public imageDisk {
public:
	enum ErrorCodes : int
	{
		OPEN_SUCCESS = 0,
		ERROR_OPENING = 1,
		INVALID_DATA = 2,
		UNSUPPORTED_TYPE = 3,
		INVALID_MATCH = 4,
		INVALID_DATE = 5,
		PARENT_ERROR = 0x10,
		ERROR_OPENING_PARENT = 0x11,
		PARENT_INVALID_DATA = 0x12,
		PARENT_UNSUPPORTED_TYPE = 0x13,
		PARENT_INVALID_MATCH = 0x14,
		PARENT_INVALID_DATE = 0x15
	};
	enum VHDTypes : Bit32u
	{
		VHD_TYPE_NONE = 0,
		VHD_TYPE_FIXED = 2,
		VHD_TYPE_DYNAMIC = 3,
		VHD_TYPE_DIFFERENCING = 4
	};
    VHDTypes vhdType = VHD_TYPE_NONE;
	virtual Bit8u Read_AbsoluteSector(Bit32u sectnum, void * data);
	virtual Bit8u Write_AbsoluteSector(Bit32u sectnum, const void * data);
	static ErrorCodes Open(const char* fileName, const bool readOnly, imageDisk** disk);
	static VHDTypes GetVHDType(const char* fileName);
	virtual ~imageDiskVHD();

private:
	struct Geometry {
		Bit16u cylinders;
		Bit8u heads;
		Bit8u sectors;
	};
	struct VHDFooter {
		char cookie[8];
		Bit32u features;
		Bit32u fileFormatVersion;
		Bit64u dataOffset;
		Bit32u timeStamp;
		char creatorApp[4];
		Bit32u creatorVersion;
		Bit32u creatorHostOS;
		Bit64u originalSize;
		Bit64u currentSize;
		Geometry geometry;
		VHDTypes diskType;
		Bit32u checksum;
		char uniqueId[16];
		char savedState;
		char reserved[427];

		void SwapByteOrder();
		Bit32u CalculateChecksum();
		bool IsValid();
	};
	struct ParentLocatorEntry {
		Bit32u platformCode;
		Bit32u platformDataSpace;
		Bit32u platformDataLength;
		Bit32u reserved;
		Bit64u platformDataOffset;
	};
	struct DynamicHeader {
		char cookie[8];
		Bit64u dataOffset;
		Bit64u tableOffset;
		Bit32u headerVersion;
		Bit32u maxTableEntries;
		Bit32u blockSize;
		Bit32u checksum;
		Bit8u parentUniqueId[16];
		Bit32u parentTimeStamp;
		Bit32u reserved;
		Bit16u parentUnicodeName[256];
		ParentLocatorEntry parentLocatorEntry[8];
		char reserved2[256];

		void SwapByteOrder();
		Bit32u CalculateChecksum();
		bool IsValid();
	};

	imageDiskVHD() : imageDisk(ID_VHD) { }
    static ErrorCodes TryOpenParent(const char* childFileName, const ParentLocatorEntry& entry, const Bit8u* data, const Bit32u dataLength, imageDisk** disk, const Bit8u* uniqueId);
	static ErrorCodes Open(const char* fileName, const bool readOnly, imageDisk** disk, const Bit8u* matchUniqueId);
	virtual bool loadBlock(const Bit32u blockNumber);
	static bool convert_UTF16_for_fopen(std::string &string, const void* data, const Bit32u dataLength);

    imageDisk* parentDisk = NULL;
	Bit64u footerPosition = 0;
    VHDFooter footer = {};
    VHDFooter originalFooter = {};
    bool copiedFooter = false;
    DynamicHeader dynamicHeader = {};
	Bit32u sectorsPerBlock = 0;
	Bit32u blockMapSectors = 0;
	Bit32u blockMapSize = 0;
	Bit32u currentBlock = 0xFFFFFFFF;
    bool currentBlockAllocated = false;
	Bit32u currentBlockSectorOffset = 0;
	Bit8u* currentBlockDirtyMap = 0;
};

void updateDPT(void);
void incrementFDD(void);

//in order to attach to the virtual IDE controllers, the disk must be mounted
//  in the BIOS first (the imageDiskList array), so the IDE controller can obtain
//  a reference to the drive in the imageDiskList array
#define MAX_HDD_IMAGES 4
#define MAX_DISK_IMAGES 6 //MAX_HDD_IMAGES + 2

extern bool imageDiskChange[MAX_DISK_IMAGES];
extern imageDisk *imageDiskList[MAX_DISK_IMAGES];
extern imageDisk *diskSwap[MAX_SWAPPABLE_DISKS];
extern Bit32s swapPosition;
extern Bit16u imgDTASeg; /* Real memory location of temporary DTA pointer for fat image disk access */
extern RealPt imgDTAPtr; /* Real memory location of temporary DTA pointer for fat image disk access */
extern DOS_DTA *imgDTA;

void swapInDisks(void);
bool getSwapRequest(void);
imageDisk *GetINT13HardDrive(unsigned char drv);
imageDisk *GetINT13FloppyDrive(unsigned char drv);

#endif
