/*
 *  Copyright (C) 2002-2015  The DOSBox Team
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

#ifndef DOSBOX_BIOS_DISK_H
#define DOSBOX_BIOS_DISK_H

#include <stdio.h>
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
	enum {
		ID_BASE=0,
		ID_EL_TORITO_FLOPPY,
        ID_VFD
	};
public:
	virtual Bit8u Read_Sector(Bit32u head,Bit32u cylinder,Bit32u sector,void * data);
	virtual Bit8u Write_Sector(Bit32u head,Bit32u cylinder,Bit32u sector,void * data);
	virtual Bit8u Read_AbsoluteSector(Bit32u sectnum, void * data);
	virtual Bit8u Write_AbsoluteSector(Bit32u sectnum, void * data);

	virtual void Set_Reserved_Cylinders(Bitu resCyl);
	virtual Bit32u Get_Reserved_Cylinders();
	virtual void Set_Geometry(Bit32u setHeads, Bit32u setCyl, Bit32u setSect, Bit32u setSectSize);
	virtual void Get_Geometry(Bit32u * getHeads, Bit32u *getCyl, Bit32u *getSect, Bit32u *getSectSize);
	virtual Bit8u GetBiosType(void);
	virtual Bit32u getSectSize(void);
    imageDisk();
	imageDisk(FILE *imgFile, Bit8u *imgName, Bit32u imgSizeK, bool isHardDisk);
	virtual ~imageDisk() { if(diskimg != NULL) { fclose(diskimg); diskimg=NULL; } };

	int class_id;

	bool hardDrive;
	bool active;
	FILE *diskimg;
	std::string diskname;
	Bit8u floppytype;

	Bit32u sector_size;
	Bit32u heads,cylinders,sectors;
	Bit32u reserved_cylinders;
	Bit64u current_fpos;
    Bit64u image_base;
    Bit32u diskSizeK;

	volatile int refcount;
	bool auto_delete_on_refcount_zero;

	int Addref() {
		return ++refcount;
	}
	int Release() {
		int ret = --refcount;
		if (ret < 0) {
			fprintf(stderr,"WARNING: imageDisk Release() changed refcount to %d\n",ret);
			abort();
		}
		if (ret == 0 && auto_delete_on_refcount_zero) delete this;
		return ret;
	}
};

class imageDiskVFD : public imageDisk {
public:
	virtual Bit8u Read_Sector(Bit32u head,Bit32u cylinder,Bit32u sector,void * data);
	virtual Bit8u Write_Sector(Bit32u head,Bit32u cylinder,Bit32u sector,void * data);
	virtual Bit8u Read_AbsoluteSector(Bit32u sectnum, void * data);
	virtual Bit8u Write_AbsoluteSector(Bit32u sectnum, void * data);

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
            return fillbyte == 0xFF;
        }

        bool hasFill(void) const {
            return fillbyte != 0xFF;
        }

        uint16_t getSectorSize(void) const {
            return 128 << sizebyte;
        }
    };

    vfdentry *findSector(Bit8u head,Bit8u track,Bit8u sector/*TODO: physical head?*/);

    std::vector<vfdentry> dents;
};

class imageDiskMemory : public imageDisk {
public:
	virtual Bit8u Read_AbsoluteSector(Bit32u sectnum, void * data);
	virtual Bit8u Write_AbsoluteSector(Bit32u sectnum, void * data);
	virtual Bit8u GetBiosType(void);
	virtual void Set_Geometry(Bit32u setHeads, Bit32u setCyl, Bit32u setSect, Bit32u setSectSize);
	// Parition and format the ramdrive
	virtual Bit8u Format();

	// Create a hard drive image of a specified size; automatically select c/h/s
	imageDiskMemory(Bit32u imgSizeK);
	// Create a hard drive image of a specified geometry
	imageDiskMemory(Bit32u cylinders, Bit32u heads, Bit32u sectors, Bit32u sectorSize);
	// Create a floppy image of a specified geometry
	imageDiskMemory(diskGeo floppyGeometry);
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
	Bit32u total_sectors;
	imageDisk* underlyingImage;

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
	VHDTypes vhdType;
	virtual Bit8u Read_AbsoluteSector(Bit32u sectnum, void * data);
	virtual Bit8u Write_AbsoluteSector(Bit32u sectnum, void * data);
	static ErrorCodes Open(const char* fileName, const bool readOnly, imageDisk** imageDisk);
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

	imageDiskVHD() : parentDisk(NULL), copiedFooter(false), currentBlock(0xFFFFFFFF), currentBlockAllocated(false), currentBlockDirtyMap(NULL) { }
	static ErrorCodes TryOpenParent(const char* childFileName, const ParentLocatorEntry &entry, Bit8u* data, const Bit32u dataLength, imageDisk** disk, const Bit8u* uniqueId);
	static ErrorCodes Open(const char* fileName, const bool readOnly, imageDisk** imageDisk, const Bit8u* matchUniqueId);
	virtual bool loadBlock(const Bit32u blockNumber);

	imageDisk* parentDisk;// = 0;
	Bit64u footerPosition;
	VHDFooter footer;
	VHDFooter originalFooter;
	bool copiedFooter;// = false;
	DynamicHeader dynamicHeader;
	Bit32u sectorsPerBlock;
	Bit32u blockMapSectors;
	Bit32u blockMapSize;
	Bit32u currentBlock;// = 0xFFFFFFFF;
	bool currentBlockAllocated;// = false;
	Bit32u currentBlockSectorOffset;
	Bit8u* currentBlockDirtyMap;// = 0;
};

void updateDPT(void);
void incrementFDD(void);

//in order to attach to the virtual IDE controllers, the disk must be mounted
//  in the BIOS first (the imageDiskList array), so the IDE controller can obtain
//  a reference to the drive in the imageDiskList array
#define MAX_HDD_IMAGES 4
#define MAX_DISK_IMAGES 6 //MAX_HDD_IMAGES + 2

extern imageDisk *imageDiskList[MAX_DISK_IMAGES];
extern imageDisk *diskSwap[20];
extern Bits swapPosition;
extern Bit16u imgDTASeg; /* Real memory location of temporary DTA pointer for fat image disk access */
extern RealPt imgDTAPtr; /* Real memory location of temporary DTA pointer for fat image disk access */
extern DOS_DTA *imgDTA;

void swapInDisks(void);
void swapInNextDisk(void);
bool getSwapRequest(void);
imageDisk *GetINT13HardDrive(unsigned char drv);
imageDisk *GetINT13FloppyDrive(unsigned char drv);

#endif
