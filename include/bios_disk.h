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
	virtual Bit8u Format();

	imageDiskMemory(Bit32u imgSizeK);
	imageDiskMemory(Bit32u cylinders, Bit32u heads, Bit32u sectors, Bit32u sectorSize);
	imageDiskMemory(diskGeo floppyGeometry);
	imageDiskMemory(imageDisk* underylingImage);
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

void updateDPT(void);
void incrementFDD(void);

#define MAX_HDD_IMAGES 2

extern imageDisk *imageDiskList[2 + MAX_HDD_IMAGES];
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
