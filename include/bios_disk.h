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
};
extern diskGeo DiskGeometryList[];

class imageDisk {
public:
	enum {
		ID_BASE=0,
		ID_EL_TORITO_FLOPPY
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
	imageDisk(FILE *imgFile, Bit8u *imgName, Bit32u imgSizeK, bool isHardDisk);
	virtual ~imageDisk() { if(diskimg != NULL) { fclose(diskimg); }	};

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

#endif
