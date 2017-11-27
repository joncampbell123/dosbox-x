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

#include "dosbox.h"
#include "callback.h"
#include "bios.h"
#include "bios_disk.h"
#include "regs.h"
#include "mem.h"
#include "dos_inc.h" /* for Drives[] */
#include "../dos/drives.h"
#include "mapper.h"

#define MAX_DISK_IMAGES 4

extern bool int13_extensions_enable;

diskGeo DiskGeometryList[] = {
    { 160,  8, 1, 40, 0, 512},      // IBM PC double density 5.25" single-sided 160KB
    { 180,  9, 1, 40, 0, 512},      // IBM PC double density 5.25" single-sided 180KB
    { 200, 10, 1, 40, 0, 512},      // DEC Rainbow double density 5.25" single-sided 200KB (I think...)
    { 320,  8, 2, 40, 1, 512},      // IBM PC double density 5.25" double-sided 320KB
    { 360,  9, 2, 40, 1, 512},      // IBM PC double density 5.25" double-sided 360KB
    { 400, 10, 2, 40, 1, 512},      // DEC Rainbow double density 5.25" double-sided 400KB (I think...)
    { 640,  8, 2, 80, 3, 512},      // IBM PC double density 3.5" double-sided 640KB
    { 720,  9, 2, 80, 3, 512},      // IBM PC double density 3.5" double-sided 720KB
    {1200, 15, 2, 80, 2, 512},      // IBM PC double density 5.25" double-sided 1.2MB
    {1440, 18, 2, 80, 4, 512},      // IBM PC high density 3.5" double-sided 1.44MB
    {2880, 36, 2, 80, 6, 512},      // IBM PC high density 3.5" double-sided 2.88MB

    {1232,  8, 2, 77, 7, 1024},     // NEC PC-98 high density 3.5" double-sided 1.2MB "3-mode"

    {0, 0, 0, 0, 0, 0}
};

Bitu call_int13 = 0;
Bitu diskparm0 = 0, diskparm1 = 0;
static Bit8u last_status;
static Bit8u last_drive;
Bit16u imgDTASeg;
RealPt imgDTAPtr;
DOS_DTA *imgDTA;
bool killRead;
static bool swapping_requested;

void CMOS_SetRegister(Bitu regNr, Bit8u val); //For setting equipment word

/* 2 floppys and 2 harddrives, max */
imageDisk *imageDiskList[MAX_DISK_IMAGES]={NULL};
imageDisk *diskSwap[MAX_SWAPPABLE_DISKS]={NULL};
Bits swapPosition;

imageDisk *GetINT13FloppyDrive(unsigned char drv) {
	if (drv >= 2)
		return NULL;
	return imageDiskList[drv];
}

imageDisk *GetINT13HardDrive(unsigned char drv) {
	if (drv < 0x80 || drv >= (0x80+MAX_DISK_IMAGES-2))
		return NULL;

	return imageDiskList[drv-0x80];
}

void FreeBIOSDiskList() {
	for (int i=0;i < MAX_DISK_IMAGES;i++) {
		if (imageDiskList[i] != NULL) {
			imageDiskList[i]->Release();
			imageDiskList[i] = NULL;
		}
	}

	for (int j=0;j < MAX_SWAPPABLE_DISKS;j++) {
		if (diskSwap[j] != NULL) {
			diskSwap[j]->Release();
			diskSwap[j] = NULL;
		}
	}
}

void updateDPT(void) {
	Bit32u tmpheads, tmpcyl, tmpsect, tmpsize;
	if(imageDiskList[2] != NULL) {
		PhysPt dp0physaddr=CALLBACK_PhysPointer(diskparm0);
		imageDiskList[2]->Get_Geometry(&tmpheads, &tmpcyl, &tmpsect, &tmpsize);
		phys_writew(dp0physaddr,(Bit16u)tmpcyl);
		phys_writeb(dp0physaddr+0x2,(Bit8u)tmpheads);
		phys_writew(dp0physaddr+0x3,0);
		phys_writew(dp0physaddr+0x5,(Bit16u)-1);
		phys_writeb(dp0physaddr+0x7,0);
		phys_writeb(dp0physaddr+0x8,(0xc0 | (((imageDiskList[2]->heads) > 8) << 3)));
		phys_writeb(dp0physaddr+0x9,0);
		phys_writeb(dp0physaddr+0xa,0);
		phys_writeb(dp0physaddr+0xb,0);
		phys_writew(dp0physaddr+0xc,(Bit16u)tmpcyl);
		phys_writeb(dp0physaddr+0xe,(Bit8u)tmpsect);
	}
	if(imageDiskList[3] != NULL) {
		PhysPt dp1physaddr=CALLBACK_PhysPointer(diskparm1);
		imageDiskList[3]->Get_Geometry(&tmpheads, &tmpcyl, &tmpsect, &tmpsize);
		phys_writew(dp1physaddr,(Bit16u)tmpcyl);
		phys_writeb(dp1physaddr+0x2,(Bit8u)tmpheads);
		phys_writeb(dp1physaddr+0xe,(Bit8u)tmpsect);
	}
}

void incrementFDD(void) {
	Bit16u equipment=mem_readw(BIOS_CONFIGURATION);
	if(equipment&1) {
		Bitu numofdisks = (equipment>>6)&3;
		numofdisks++;
		if(numofdisks > 1) numofdisks=1;//max 2 floppies at the moment
		equipment&=~0x00C0;
		equipment|=(numofdisks<<6);
	} else equipment|=1;
	mem_writew(BIOS_CONFIGURATION,equipment);
	CMOS_SetRegister(0x14, (Bit8u)(equipment&0xff));
}

void swapInDisks(void) {
	bool allNull = true;
	Bits diskcount = 0;
	Bits swapPos = swapPosition;
	int i;

	/* Check to make sure that  there is at least one setup image */
	for(i=0;i<MAX_SWAPPABLE_DISKS;i++) {
		if(diskSwap[i]!=NULL) {
			allNull = false;
			break;
		}
	}

	/* No disks setup... fail */
	if (allNull) return;

	/* If only one disk is loaded, this loop will load the same disk in dive A and drive B */
	while(diskcount<2) {
		if(diskSwap[swapPos] != NULL) {
			LOG_MSG("Loaded disk %d from swaplist position %d - \"%s\"", (int)diskcount, (int)swapPos, diskSwap[swapPos]->diskname.c_str());

			if (imageDiskList[diskcount] != NULL)
				imageDiskList[diskcount]->Release();

			imageDiskList[diskcount] = diskSwap[swapPos];
			imageDiskList[diskcount]->Addref();

			diskcount++;
		}
		swapPos++;
		if(swapPos>=MAX_SWAPPABLE_DISKS) swapPos=0;
	}
}

bool getSwapRequest(void) {
	bool sreq=swapping_requested;
	swapping_requested = false;
	return sreq;
}

void swapInNextDisk(bool pressed) {
	if (!pressed)
		return;
	DriveManager::CycleAllDisks();
	/* Hack/feature: rescan all disks as well */
	LOG_MSG("Diskcaching reset for floppy drives.");
	for(Bitu i=0;i<2;i++) { /* Swap A: and B: where DOSBox mainline would run through ALL drive letters */
		if (Drives[i] != NULL) {
			Drives[i]->EmptyCache();
			Drives[i]->MediaChange();
		}
	}
	swapPosition++;
	if(diskSwap[swapPosition] == NULL) swapPosition = 0;
	swapInDisks();
	swapping_requested = true;
}

void swapInNextCD(bool pressed) {
	if (!pressed)
		return;
	DriveManager::CycleAllCDs();
	/* Hack/feature: rescan all disks as well */
	LOG_MSG("Diskcaching reset for normal mounted drives.");
	for(Bitu i=2;i<DOS_DRIVES;i++) { /* Swap C: D: .... Z: TODO: Need to swap ONLY if a CD-ROM drive! */
		if (Drives[i] != NULL) {
			Drives[i]->EmptyCache();
			Drives[i]->MediaChange();
		}
	}
}


Bit8u imageDisk::Read_Sector(Bit32u head,Bit32u cylinder,Bit32u sector,void * data) {
	Bit32u sectnum;

	sectnum = ( (cylinder * heads + head) * sectors ) + sector - 1L;

	return Read_AbsoluteSector(sectnum, data);
}

Bit8u imageDisk::Read_AbsoluteSector(Bit32u sectnum, void * data) {
	Bit64u bytenum,res;
	int got;

	bytenum = (Bit64u)sectnum * (Bit64u)sector_size;
    bytenum += image_base;

	//LOG_MSG("Reading sectors %ld at bytenum %I64d", sectnum, bytenum);

	fseeko64(diskimg,bytenum,SEEK_SET);
	res = ftello64(diskimg);
	if (res != bytenum) {
		LOG_MSG("fseek() failed in Read_AbsoluteSector for sector %lu. Want=%llu Got=%llu\n",
			(unsigned long)sectnum,(unsigned long long)bytenum,(unsigned long long)res);
		return 0x05;
	}

	got = fread(data, 1, sector_size, diskimg);
	if ((unsigned int)got != sector_size) {
		LOG_MSG("fread() failed in Read_AbsoluteSector for sector %lu. Want=%u got=%d\n",
			(unsigned long)sectnum,(unsigned int)sector_size,(unsigned int)got);
		return 0x05;
	}

	return 0x00;
}

Bit8u imageDisk::Write_Sector(Bit32u head,Bit32u cylinder,Bit32u sector,void * data) {
	Bit32u sectnum;

	sectnum = ( (cylinder * heads + head) * sectors ) + sector - 1L;

	return Write_AbsoluteSector(sectnum, data);
}


Bit8u imageDisk::Write_AbsoluteSector(Bit32u sectnum, void *data) {
	Bit64u bytenum;

	bytenum = (Bit64u)sectnum * sector_size;
    bytenum += image_base;

	//LOG_MSG("Writing sectors to %ld at bytenum %d", sectnum, bytenum);

	fseeko64(diskimg,bytenum,SEEK_SET);
	if ((Bit64u)ftello64(diskimg) != bytenum)
		LOG_MSG("WARNING: fseek() failed in Read_AbsoluteSector for sector %lu\n",(unsigned long)sectnum);

	size_t ret=fwrite(data, sector_size, 1, diskimg);

	return ((ret>0)?0x00:0x05);

}

void imageDisk::Set_Reserved_Cylinders(Bitu resCyl) {
	reserved_cylinders = resCyl;
}

Bit32u imageDisk::Get_Reserved_Cylinders() {
	return reserved_cylinders;
}

imageDisk::imageDisk() {
	heads = 0;
	cylinders = 0;
    image_base = 0;
    sectors = 0;
	refcount = 0;
	sector_size = 512;
	reserved_cylinders = 0;
	auto_delete_on_refcount_zero = true;
	diskimg = NULL;
	class_id = ID_BASE;
	active = false;
	hardDrive = false;
}

imageDisk::imageDisk(FILE *imgFile, Bit8u *imgName, Bit32u imgSizeK, bool isHardDisk) {
	heads = 0;
	cylinders = 0;
    image_base = 0;
    sectors = 0;
	refcount = 0;
	sector_size = 512;
	reserved_cylinders = 0;
	auto_delete_on_refcount_zero = true;
	diskimg = imgFile;
	class_id = ID_BASE;

	if (imgName != NULL)
		diskname = (const char*)imgName;

	active = false;
	hardDrive = isHardDisk;
	if(!isHardDisk) {
		Bit8u i=0;
		bool founddisk = false;

        {
            char *ext = strrchr((char*)imgName,'.');
            if (ext != NULL) {
                if (!strcasecmp(ext,".fdi")) {
                    if (imgSizeK >= 160) {
                        // PC-98 .FDI images appear to be 4096 bytes of unknown and mostly zeros,
                        // followed by a straight sector dump of the disk.
                        imgSizeK -= 4; // minus 4K
                        image_base += 4096; // +4K
                        LOG_MSG("Image file has .FDI extension, assuming 4K offset");
                    }
                }
            }
        }

		while (DiskGeometryList[i].ksize!=0x0) {
			if ((DiskGeometryList[i].ksize==imgSizeK) ||
				(DiskGeometryList[i].ksize+1==imgSizeK)) {
				if (DiskGeometryList[i].ksize!=imgSizeK)
					LOG_MSG("ImageLoader: image file with additional data, might not load!");
				founddisk = true;
				active = true;
				floppytype = i;
				heads = DiskGeometryList[i].headscyl;
				cylinders = DiskGeometryList[i].cylcount;
				sectors = DiskGeometryList[i].secttrack;
                sector_size = DiskGeometryList[i].bytespersect;
                LOG_MSG("Identified '%s' as C/H/S %u/%u/%u %u bytes/sector",
                    imgName,cylinders,heads,sectors,sector_size);
				break;
			}
			i++;
		}
		if(!founddisk) {
			active = false;
		} else {
			incrementFDD();
		}
	}
}

void imageDisk::Set_Geometry(Bit32u setHeads, Bit32u setCyl, Bit32u setSect, Bit32u setSectSize) {
	Bitu bigdisk_shift = 0;
	if(setCyl > 16384 ) LOG_MSG("This disk image is too big.");
	else if(setCyl > 8192) bigdisk_shift = 4;
	else if(setCyl > 4096) bigdisk_shift = 3;
	else if(setCyl > 2048) bigdisk_shift = 2;
	else if(setCyl > 1024) bigdisk_shift = 1;

	heads = setHeads << bigdisk_shift;
	cylinders = setCyl >> bigdisk_shift;
	sectors = setSect;
	sector_size = setSectSize;
	active = true;
}

void imageDisk::Get_Geometry(Bit32u * getHeads, Bit32u *getCyl, Bit32u *getSect, Bit32u *getSectSize) {
	*getHeads = heads;
	*getCyl = cylinders;
	*getSect = sectors;
	*getSectSize = sector_size;
}

Bit8u imageDisk::GetBiosType(void) {
	if(!hardDrive) {
		return (Bit8u)DiskGeometryList[floppytype].biosval;
	} else return 0;
}

Bit32u imageDisk::getSectSize(void) {
	return sector_size;
}

static Bitu GetDosDriveNumber(Bitu biosNum) {
	switch(biosNum) {
		case 0x0:
			return 0x0;
		case 0x1:
			return 0x1;
		case 0x80:
			return 0x2;
		case 0x81:
			return 0x3;
		case 0x82:
			return 0x4;
		case 0x83:
			return 0x5;
		default:
			return 0x7f;
	}
}

static bool driveInactive(Bitu driveNum) {
	if(driveNum>=(2 + MAX_HDD_IMAGES)) {
		LOG(LOG_BIOS,LOG_ERROR)("Disk %d non-existant", (int)driveNum);
		last_status = 0x01;
		CALLBACK_SCF(true);
		return true;
	}
	if(imageDiskList[driveNum] == NULL) {
		LOG(LOG_BIOS,LOG_ERROR)("Disk %d not active", (int)driveNum);
		last_status = 0x01;
		CALLBACK_SCF(true);
		return true;
	}
	if(!imageDiskList[driveNum]->active) {
		LOG(LOG_BIOS,LOG_ERROR)("Disk %d not active", (int)driveNum);
		last_status = 0x01;
		CALLBACK_SCF(true);
		return true;
	}
	return false;
}

static struct {
	Bit8u sz;
	Bit8u res;
	Bit16u num;
	Bit16u off;
	Bit16u seg;
	Bit32u sector;
} dap;

static void readDAP(Bit16u seg, Bit16u off) {
	dap.sz = real_readb(seg,off++);
	dap.res = real_readb(seg,off++);
	dap.num = real_readw(seg,off); off += 2;
	dap.off = real_readw(seg,off); off += 2;
	dap.seg = real_readw(seg,off); off += 2;

	/* Although sector size is 64-bit, 32-bit 2TB limit should be more than enough */
	dap.sector = real_readd(seg,off); off +=4;

	if (real_readd(seg,off)) {
		E_Exit("INT13: 64-bit sector addressing not supported");
	}
}

void IDE_ResetDiskByBIOS(unsigned char disk);
void IDE_EmuINT13DiskReadByBIOS(unsigned char disk,unsigned int cyl,unsigned int head,unsigned sect);
void IDE_EmuINT13DiskReadByBIOS_LBA(unsigned char disk,uint64_t lba);

static Bitu INT13_DiskHandler(void) {
	Bit16u segat, bufptr;
	Bit8u sectbuf[512];
	Bitu  drivenum;
	Bitu  i,t;
	last_drive = reg_dl;
	drivenum = GetDosDriveNumber(reg_dl);
	bool any_images = false;
	for(i = 0;i < MAX_DISK_IMAGES;i++) {
		if(imageDiskList[i]) any_images=true;
	}

	// unconditionally enable the interrupt flag
	CALLBACK_SIF(true);

	/* map out functions 0x40-0x48 if not emulating INT 13h extensions */
	if (!int13_extensions_enable && reg_ah >= 0x40 && reg_ah <= 0x48) {
		LOG_MSG("Warning: Guest is attempting to use INT 13h extensions (AH=0x%02X). Set 'int 13 extensions=1' if you want to enable them.\n",reg_ah);
		reg_ah=0xff;
		CALLBACK_SCF(true);
		return CBRET_NONE;
	}

	//drivenum = 0;
	//LOG_MSG("INT13: Function %x called on drive %x (dos drive %d)", reg_ah,  reg_dl, drivenum);
	switch(reg_ah) {
	case 0x0: /* Reset disk */
		{
			/* if there aren't any diskimages (so only localdrives and virtual drives)
			 * always succeed on reset disk. If there are diskimages then and only then
			 * do real checks
			 */
			if (any_images && driveInactive(drivenum)) {
				/* driveInactive sets carry flag if the specified drive is not available */
				if ((machine==MCH_CGA) || (machine==MCH_AMSTRAD) || (machine==MCH_PCJR)) {
					/* those bioses call floppy drive reset for invalid drive values */
					if (((imageDiskList[0]) && (imageDiskList[0]->active)) || ((imageDiskList[1]) && (imageDiskList[1]->active))) {
						if (machine!=MCH_PCJR && reg_dl<0x80) reg_ip++;
						last_status = 0x00;
						CALLBACK_SCF(false);
					}
				}
				return CBRET_NONE;
			}
			if (machine!=MCH_PCJR && reg_dl<0x80) reg_ip++;
			if (reg_dl >= 0x80) IDE_ResetDiskByBIOS(reg_dl);
			last_status = 0x00;
			CALLBACK_SCF(false);
		}
        break;
	case 0x1: /* Get status of last operation */

		if(last_status != 0x00) {
			reg_ah = last_status;
			CALLBACK_SCF(true);
		} else {
			reg_ah = 0x00;
			CALLBACK_SCF(false);
		}
		break;
	case 0x2: /* Read sectors */
		if (reg_al==0) {
			reg_ah = 0x01;
			CALLBACK_SCF(true);
			return CBRET_NONE;
		}
		if (!any_images) {
			// Inherit the Earth cdrom (uses it as disk test)
			if (((reg_dl&0x80)==0x80) && (reg_dh==0) && ((reg_cl&0x3f)==1)) {
				reg_ah = 0;
				CALLBACK_SCF(false);
				return CBRET_NONE;
			}
		}
		if (driveInactive(drivenum)) {
			reg_ah = 0xff;
			CALLBACK_SCF(true);
			return CBRET_NONE;
		}

		segat = SegValue(es);
		bufptr = reg_bx;
		for(i=0;i<reg_al;i++) {
			last_status = imageDiskList[drivenum]->Read_Sector((Bit32u)reg_dh, (Bit32u)(reg_ch | ((reg_cl & 0xc0)<< 2)), (Bit32u)((reg_cl & 63)+i), sectbuf);

			/* IDE emulation: simulate change of IDE state that would occur on a real machine after INT 13h */
			IDE_EmuINT13DiskReadByBIOS(reg_dl, (Bit32u)(reg_ch | ((reg_cl & 0xc0)<< 2)), (Bit32u)reg_dh, (Bit32u)((reg_cl & 63)+i));

			if((last_status != 0x00) || (killRead)) {
				LOG_MSG("Error in disk read");
				killRead = false;
				reg_ah = 0x04;
				CALLBACK_SCF(true);
				return CBRET_NONE;
			}
			for(t=0;t<512;t++) {
				real_writeb(segat,bufptr,sectbuf[t]);
				bufptr++;
			}
		}
		reg_ah = 0x00;
		CALLBACK_SCF(false);
		break;
	case 0x3: /* Write sectors */
		
		if(driveInactive(drivenum)) {
			reg_ah = 0xff;
			CALLBACK_SCF(true);
			return CBRET_NONE;
        }                     


		bufptr = reg_bx;
		for(i=0;i<reg_al;i++) {
			for(t=0;t<imageDiskList[drivenum]->getSectSize();t++) {
				sectbuf[t] = real_readb(SegValue(es),bufptr);
				bufptr++;
			}

			last_status = imageDiskList[drivenum]->Write_Sector((Bit32u)reg_dh, (Bit32u)(reg_ch | ((reg_cl & 0xc0) << 2)), (Bit32u)((reg_cl & 63) + i), &sectbuf[0]);
			if(last_status != 0x00) {
            CALLBACK_SCF(true);
				return CBRET_NONE;
			}
        }
		reg_ah = 0x00;
		CALLBACK_SCF(false);
        break;
	case 0x04: /* Verify sectors */
		if (reg_al==0) {
			reg_ah = 0x01;
			CALLBACK_SCF(true);
			return CBRET_NONE;
		}
		if(driveInactive(drivenum)) return CBRET_NONE;

		/* TODO: Finish coding this section */
		/*
		segat = SegValue(es);
		bufptr = reg_bx;
		for(i=0;i<reg_al;i++) {
			last_status = imageDiskList[drivenum]->Read_Sector((Bit32u)reg_dh, (Bit32u)(reg_ch | ((reg_cl & 0xc0)<< 2)), (Bit32u)((reg_cl & 63)+i), sectbuf);
			if(last_status != 0x00) {
				LOG_MSG("Error in disk read");
				CALLBACK_SCF(true);
				return CBRET_NONE;
			}
			for(t=0;t<512;t++) {
				real_writeb(segat,bufptr,sectbuf[t]);
				bufptr++;
			}
		}*/
		reg_ah = 0x00;
		//Qbix: The following codes don't match my specs. al should be number of sector verified
		//reg_al = 0x10; /* CRC verify failed */
		//reg_al = 0x00; /* CRC verify succeeded */
		CALLBACK_SCF(false);
          
		break;
	case 0x05: /* Format track */
		/* ignore it. I just fucking want FORMAT.COM to write the FAT structure for God's sake */
		LOG_MSG("WARNING: Format track ignored\n");
		CALLBACK_SCF(false);
		reg_ah = 0x00;
		break;
	case 0x06: /* Format track set bad sector flags */
		/* ignore it. I just fucking want FORMAT.COM to write the FAT structure for God's sake */
		LOG_MSG("WARNING: Format track set bad sector flags ignored (6)\n");
		CALLBACK_SCF(false);
		reg_ah = 0x00;
		break;
	case 0x07: /* Format track set bad sector flags */
		/* ignore it. I just fucking want FORMAT.COM to write the FAT structure for God's sake */
		LOG_MSG("WARNING: Format track set bad sector flags ignored (7)\n");
		CALLBACK_SCF(false);
		reg_ah = 0x00;
		break;
	case 0x08: /* Get drive parameters */
		if(driveInactive(drivenum)) {
			last_status = 0x07;
			reg_ah = last_status;
			CALLBACK_SCF(true);
			return CBRET_NONE;
		}
		reg_ax = 0x00;
		reg_bl = imageDiskList[drivenum]->GetBiosType();
		Bit32u tmpheads, tmpcyl, tmpsect, tmpsize;
		imageDiskList[drivenum]->Get_Geometry(&tmpheads, &tmpcyl, &tmpsect, &tmpsize);
		if (tmpcyl==0) LOG(LOG_BIOS,LOG_ERROR)("INT13 DrivParm: cylinder count zero!");
		else tmpcyl--;		// cylinder count -> max cylinder
		if (tmpheads==0) LOG(LOG_BIOS,LOG_ERROR)("INT13 DrivParm: head count zero!");
		else tmpheads--;	// head count -> max head

		/* older BIOSes were known to subtract 1 or 2 additional "reserved" cylinders.
		 * some code, such as Windows 3.1 WDCTRL, might assume that fact. emulate that here */
		{
			Bit32u reserv = imageDiskList[drivenum]->Get_Reserved_Cylinders();
			if (tmpcyl > reserv) tmpcyl -= reserv;
			else tmpcyl = 0;
		}

		reg_ch = (Bit8u)(tmpcyl & 0xff);
		reg_cl = (Bit8u)(((tmpcyl >> 2) & 0xc0) | (tmpsect & 0x3f)); 
		reg_dh = (Bit8u)tmpheads;
		last_status = 0x00;
		if (reg_dl&0x80) {	// harddisks
			reg_dl = 0;
			if(imageDiskList[2] != NULL) reg_dl++;
			if(imageDiskList[3] != NULL) reg_dl++;
		} else {		// floppy disks
			reg_dl = 0;
			if(imageDiskList[0] != NULL) reg_dl++;
			if(imageDiskList[1] != NULL) reg_dl++;
		}
		CALLBACK_SCF(false);
		break;
	case 0x11: /* Recalibrate drive */
		reg_ah = 0x00;
		CALLBACK_SCF(false);
		break;
	case 0x17: /* Set disk type for format */
		/* Pirates! needs this to load */
		killRead = true;
		reg_ah = 0x00;
		CALLBACK_SCF(false);
		break;
	case 0x41: /* Check Extensions Present */
		if ((reg_bx == 0x55aa) && !(driveInactive(drivenum))) {
			LOG_MSG("INT13: Check Extensions Present for drive: 0x%x", reg_dl);
			reg_ah=0x1;	/* 1.x extension supported */
			reg_bx=0xaa55;	/* Extensions installed */
			reg_cx=0x1;	/* Extended disk access functions (AH=42h-44h,47h,48h) supported */
			CALLBACK_SCF(false);
			break;
		}
		LOG_MSG("INT13: AH=41h, Function not supported 0x%x for drive: 0x%x", reg_bx, reg_dl);
		CALLBACK_SCF(true);
		break;
	case 0x42: /* Extended Read Sectors From Drive */
		/* Read Disk Address Packet */
		readDAP(SegValue(ds),reg_si);

		if (dap.num==0) {
			reg_ah = 0x01;
			CALLBACK_SCF(true);
			return CBRET_NONE;
		}
		if (!any_images) {
			// Inherit the Earth cdrom (uses it as disk test)
			if (((reg_dl&0x80)==0x80) && (reg_dh==0) && ((reg_cl&0x3f)==1)) {
				reg_ah = 0;
				CALLBACK_SCF(false);
				return CBRET_NONE;
			}
		}
		if (driveInactive(drivenum)) {
			reg_ah = 0xff;
			CALLBACK_SCF(true);
			return CBRET_NONE;
		}

		segat = dap.seg;
		bufptr = dap.off;
		for(i=0;i<dap.num;i++) {
			last_status = imageDiskList[drivenum]->Read_AbsoluteSector(dap.sector+i, sectbuf);

			IDE_EmuINT13DiskReadByBIOS_LBA(reg_dl,dap.sector+i);

			if((last_status != 0x00) || (killRead)) {
				LOG_MSG("Error in disk read");
				killRead = false;
				reg_ah = 0x04;
				CALLBACK_SCF(true);
				return CBRET_NONE;
			}
			for(t=0;t<512;t++) {
				real_writeb(segat,bufptr,sectbuf[t]);
				bufptr++;
			}
		}
		reg_ah = 0x00;
		CALLBACK_SCF(false);
		break;
	case 0x43: /* Extended Write Sectors to Drive */
		if(driveInactive(drivenum)) {
			reg_ah = 0xff;
			CALLBACK_SCF(true);
			return CBRET_NONE;
		}

		/* Read Disk Address Packet */
		readDAP(SegValue(ds),reg_si);
		bufptr = dap.off;
		for(i=0;i<dap.num;i++) {
			for(t=0;t<imageDiskList[drivenum]->getSectSize();t++) {
				sectbuf[t] = real_readb(dap.seg,bufptr);
				bufptr++;
			}

			last_status = imageDiskList[drivenum]->Write_AbsoluteSector(dap.sector+i, &sectbuf[0]);
			if(last_status != 0x00) {
				CALLBACK_SCF(true);
				return CBRET_NONE;
			}
		}
		reg_ah = 0x00;
		CALLBACK_SCF(false);
		break;
	case 0x48: { /* get drive parameters */
		uint16_t bufsz;

		if(driveInactive(drivenum)) {
			reg_ah = 0xff;
			CALLBACK_SCF(true);
			return CBRET_NONE;
		}

		segat = SegValue(ds);
		bufptr = reg_si;
		bufsz = real_readw(segat,bufptr+0);
		if (bufsz < 0x1A) {
			reg_ah = 0xff;
			CALLBACK_SCF(true);
			return CBRET_NONE;
		}
		if (bufsz > 0x1E) bufsz = 0x1E;
		else bufsz = 0x1A;

		Bit32u tmpheads, tmpcyl, tmpsect, tmpsize;
		imageDiskList[drivenum]->Get_Geometry(&tmpheads, &tmpcyl, &tmpsect, &tmpsize);

		real_writew(segat,bufptr+0x00,bufsz);
		real_writew(segat,bufptr+0x02,0x0003);	/* C/H/S valid, DMA boundary errors handled */
		real_writed(segat,bufptr+0x04,tmpcyl);
		real_writed(segat,bufptr+0x08,tmpheads);
		real_writed(segat,bufptr+0x0C,tmpsect);
		real_writed(segat,bufptr+0x10,tmpcyl*tmpheads*tmpsect);
		real_writed(segat,bufptr+0x14,0);
		real_writew(segat,bufptr+0x18,512);
		if (bufsz >= 0x1E)
			real_writed(segat,bufptr+0x1A,0xFFFFFFFF); /* no EDD information available */

		reg_ah = 0x00;
		CALLBACK_SCF(false);
		} break;
	default:
		LOG(LOG_BIOS,LOG_ERROR)("INT13: Function %x called on drive %x (dos drive %d)", (int)reg_ah, (int)reg_dl, (int)drivenum);
		reg_ah=0xff;
		CALLBACK_SCF(true);
	}
	return CBRET_NONE;
}

void CALLBACK_DeAllocate(Bitu in);

void BIOS_UnsetupDisks(void) {
    if (call_int13 != 0) {
        CALLBACK_DeAllocate(call_int13);
        RealSetVec(0x13,0); /* zero INT 13h for now */
        call_int13 = 0;
    }
    if (diskparm0 != 0) {
        CALLBACK_DeAllocate(diskparm0);
        diskparm0 = 0;
    }
    if (diskparm1 != 0) {
        CALLBACK_DeAllocate(diskparm1);
        diskparm1 = 0;
    }
}

void BIOS_SetupDisks(void) {
	int i;

    if (IS_PC98_ARCH) {
        // TODO
        return;
    }

/* TODO Start the time correctly */
	call_int13=CALLBACK_Allocate();	
	CALLBACK_Setup(call_int13,&INT13_DiskHandler,CB_INT13,"Int 13 Bios disk");
	RealSetVec(0x13,CALLBACK_RealPointer(call_int13));

    /* FIXME: I see a potential problem here: We're just zeroing out the array. Didn't I rewrite disk images with refcounting? --J.C. */
	for(i=0;i<MAX_DISK_IMAGES;i++)
		imageDiskList[i] = NULL;
	for(i=0;i<MAX_SWAPPABLE_DISKS;i++)
		diskSwap[i] = NULL;

    /* FIXME: Um... these aren't callbacks. Why are they allocated as callbacks? We have ROM general allocation now. */
	diskparm0 = CALLBACK_Allocate();
	CALLBACK_SetDescription(diskparm0,"BIOS Disk 0 parameter table");
	diskparm1 = CALLBACK_Allocate();
	CALLBACK_SetDescription(diskparm1,"BIOS Disk 1 parameter table");
	swapPosition = 0;

	RealSetVec(0x41,CALLBACK_RealPointer(diskparm0));
	RealSetVec(0x46,CALLBACK_RealPointer(diskparm1));

	PhysPt dp0physaddr=CALLBACK_PhysPointer(diskparm0);
	PhysPt dp1physaddr=CALLBACK_PhysPointer(diskparm1);
	for(i=0;i<16;i++) {
		phys_writeb(dp0physaddr+i,0);
		phys_writeb(dp1physaddr+i,0);
	}

	imgDTASeg = 0;

/* Setup the Bios Area */
	mem_writeb(BIOS_HARDDISK_COUNT,2);

	killRead = false;
	swapping_requested = false;
}

// VFD *.FDD floppy disk format support

Bit8u imageDiskVFD::Read_Sector(Bit32u head,Bit32u cylinder,Bit32u sector,void * data) {
    vfdentry *ent;

    LOG_MSG("VFD read sector: CHS %u/%u/%u",cylinder,head,sector);

    ent = findSector(head,cylinder,sector);
    if (ent == NULL) return 0x05;
    if (ent->getSectorSize() != sector_size) return 0x05;

    if (ent->hasSectorData()) {
        fseek(diskimg,ent->data_offset,SEEK_SET);
        if (ftell(diskimg) != ent->data_offset) return 0x05;
        if (fread(data,sector_size,1,diskimg) != 1) return 0x05;
        return 0;
    }
    else if (ent->hasFill()) {
        memset(data,ent->fillbyte,sector_size);
        return 0x00;
    }

	return 0x05;
}

Bit8u imageDiskVFD::Read_AbsoluteSector(Bit32u sectnum, void * data) {
    unsigned int c,h,s;

    if (sectors == 0 || heads == 0)
        return 0x05;

    s = (sectnum % sectors) + 1;
    h = (sectnum / sectors) % heads;
    c = (sectnum / sectors / heads);
    return Read_Sector(h,c,s,data);
}

imageDiskVFD::vfdentry *imageDiskVFD::findSector(Bit8u head,Bit8u track,Bit8u sector/*TODO: physical head?*/) {
    std::vector<imageDiskVFD::vfdentry>::iterator i = dents.begin();

    while (i != dents.end()) {
        imageDiskVFD::vfdentry &ent = *i;

        if (ent.head == head &&
            ent.track == track &&
            ent.sector == sector)
            return &(*i);

        i++;
    }

    return NULL;
}

Bit8u imageDiskVFD::Write_Sector(Bit32u head,Bit32u cylinder,Bit32u sector,void * data) {
    unsigned long new_offset;
    unsigned char tmp[12];
    vfdentry *ent;

    LOG_MSG("VFD write sector: CHS %u/%u/%u",cylinder,head,sector);

    ent = findSector(head,cylinder,sector);
    if (ent == NULL) return 0x05;
    if (ent->getSectorSize() != sector_size) return 0x05;

    if (ent->hasSectorData()) {
        fseek(diskimg,ent->data_offset,SEEK_SET);
        if (ftell(diskimg) != ent->data_offset) return 0x05;
        if (fwrite(data,sector_size,1,diskimg) != 1) return 0x05;
        return 0;
    }
    else if (ent->hasFill()) {
        bool isfill = false;

        /* well, is the data provided one character repeated?
         * note the format cannot represent a fill byte of 0xFF */
        if (((unsigned char*)data)[0] != 0xFF) {
            unsigned int i=1;

            do {
                if (((unsigned char*)data)[i] == ((unsigned char*)data)[0]) {
                    if ((++i) == sector_size) {
                        isfill = true;
                        break; // yes!
                    }
                }
                else {
                    break; // nope
                }
            } while (1);
        }

        if (ent->entry_offset == 0) return 0x05;

        if (isfill) {
            fseek(diskimg,ent->entry_offset,SEEK_SET);
            if (ftell(diskimg) != ent->entry_offset) return 0x05;
            if (fread(tmp,12,1,diskimg) != 1) return 0x05;

            tmp[0x04] = ((unsigned char*)data)[0]; // change the fill byte

            LOG_MSG("VFD write: 'fill' sector changing fill byte to 0x%x",tmp[0x04]);

            fseek(diskimg,ent->entry_offset,SEEK_SET);
            if (ftell(diskimg) != ent->entry_offset) return 0x05;
            if (fwrite(tmp,12,1,diskimg) != 1) return 0x05;
        }
        else {
            fseek(diskimg,0,SEEK_END);
            new_offset = ftell(diskimg);

            /* we have to change it from a fill sector to an actual sector */
            LOG_MSG("VFD write: changing 'fill' sector to one with data (data at %lu)",(unsigned long)new_offset);

            fseek(diskimg,ent->entry_offset,SEEK_SET);
            if (ftell(diskimg) != ent->entry_offset) return 0x05;
            if (fread(tmp,12,1,diskimg) != 1) return 0x05;

            tmp[0x00] = ent->track;
            tmp[0x01] = ent->head;
            tmp[0x02] = ent->sector;
            tmp[0x03] = ent->sizebyte;
            tmp[0x04] = 0xFF; // no longer a fill byte
            tmp[0x05] = 0x00; // TODO ??
            tmp[0x06] = 0x00; // TODO ??
            tmp[0x07] = 0x00; // TODO ??
            *((uint32_t*)(tmp+8)) = new_offset;
            ent->fillbyte = 0xFF;
            ent->data_offset = (uint32_t)new_offset;

            fseek(diskimg,ent->entry_offset,SEEK_SET);
            if (ftell(diskimg) != ent->entry_offset) return 0x05;
            if (fwrite(tmp,12,1,diskimg) != 1) return 0x05;

            fseek(diskimg,ent->data_offset,SEEK_SET);
            if (ftell(diskimg) != ent->data_offset) return 0x05;
            if (fwrite(data,sector_size,1,diskimg) != 1) return 0x05;
        }
    }

	return 0x05;
}

Bit8u imageDiskVFD::Write_AbsoluteSector(Bit32u sectnum, void *data) {
    unsigned int c,h,s;

    if (sectors == 0 || heads == 0)
        return 0x05;

    s = (sectnum % sectors) + 1;
    h = (sectnum / sectors) % heads;
    c = (sectnum / sectors / heads);
    return Write_Sector(h,c,s,data);
}

imageDiskVFD::imageDiskVFD(FILE *imgFile, Bit8u *imgName, Bit32u imgSizeK, bool isHardDisk) : imageDisk() {
    unsigned char tmp[16];

	heads = 1;
	cylinders = 0;
    image_base = 0;
    sectors = 0;
	refcount = 0;
	active = false;
	sector_size = 0;
	reserved_cylinders = 0;
	auto_delete_on_refcount_zero = true;
	diskimg = imgFile;
	class_id = ID_VFD;

	if (imgName != NULL)
		diskname = (const char*)imgName;

    // NOTES:
    // 
    //  +0x000: "VFD1.00"
    //  +0x0DC: array of 12-byte entries each describing a sector
    //
    //  Each entry:
    //  +0x0: track
    //  +0x1: head
    //  +0x2: sector
    //  +0x3: sector size (128 << this byte)
    //  +0x4: fill byte, or 0xFF
    //  +0x5: unknown
    //  +0x6: unknown
    //  +0x7: unknown
    //  +0x8: absolute data offset (32-bit integer) or 0xFFFFFFFF if the entire sector is that fill byte
    fseek(diskimg,0,SEEK_SET);
    memset(tmp,0,8);
    fread(tmp,1,8,diskimg);

    if (!memcmp(tmp,"VFD1.",5)) {
		Bit8u i=0;
		bool founddisk = false;
        uint32_t stop_at = 0xC3FC;
        unsigned long entof;

        // load table.
        // we have to determine as we go where to stop reading.
        // the source of info I read assumes the whole header (and table)
        // is 0xC3FC bytes. I'm not inclined to assume that, so we go by
        // that OR the first sector offset whichever is smaller.
        // the table seems to trail off into a long series of 0xFF at the end.
        fseek(diskimg,0xDC,SEEK_SET);
        while ((entof=(ftell(diskimg)+12)) <= stop_at) {
            memset(tmp,0xFF,12);
            fread(tmp,12,1,diskimg);

            if (!memcmp(tmp,"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF",12))
                continue;
            if (!memcmp(tmp,"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",12))
                continue;

            struct vfdentry v;

            v.track = tmp[0];
            v.head = tmp[1];
            v.sector = tmp[2];
            v.sizebyte = tmp[3];
            v.fillbyte = tmp[4];
            v.data_offset = *((uint32_t*)(tmp+8));
            v.entry_offset = (uint32_t)entof;

            // maybe the table can end sooner than 0xC3FC?
            // if we see sectors appear at an offset lower than our stop_at point
            // then adjust the stop_at point. assume the table cannot mix with
            // sector data.
            if (v.hasSectorData()) {
                if (stop_at > v.data_offset)
                    stop_at = v.data_offset;
            }

            dents.push_back(v);

            LOG_MSG("VFD entry: track=%u head=%u sector=%u size=%u fill=0x%2X has_data=%u has_fill=%u entoff=%lu dataoff=%lu",
                v.track,
                v.head,
                v.sector,
                v.getSectorSize(),
                v.fillbyte,
                v.hasSectorData(),
                v.hasFill(),
                (unsigned long)v.entry_offset,
                (unsigned long)v.data_offset);
        }

        if (!dents.empty()) {
            /* okay, now to figure out what the geometry of the disk is.
             * we cannot just work from an "absolute" disk image model
             * because there's no VFD header to just say what the geometry is.
             * Like the IBM PC BIOS, we have to look at the disk and figure out
             * which geometry to apply to it, even if the FDD format allows
             * sectors on other tracks to have wild out of range sector, track,
             * and head numbers or odd sized sectors.
             *
             * First, determine sector size according to the boot sector. */
            vfdentry *ent;

            ent = findSector(/*head*/0,/*track*/0,/*sector*/1+i);
            if (ent != NULL) {
                if (ent->sizebyte <= 3) /* x <= 1024 */
                    sector_size = ent->getSectorSize();
            }

            /* oh yeah right, sure.
             * I suppose you're one of those FDD images where the sector size is 128 bytes/sector
             * in the boot sector and the rest is 256 bytes/sector elsewhere. I have no idea why
             * but quite a few FDD images have this arrangement. */
            if (sector_size != 0 && sector_size < 512) {
                ent = findSector(/*head*/0,/*track*/1,/*sector*/1+i);
                if (ent != NULL) {
                    if (ent->sizebyte <= 3) { /* x <= 1024 */
                        unsigned int nsz = ent->getSectorSize();
                        if (sector_size != nsz)
                            LOG_MSG("VFD warning: sector size changes between track 0 and 1");
                        if (sector_size < nsz)
                            sector_size = nsz;
                    }
                }
           }

            if (sector_size != 0) {
                i=0;
                while (DiskGeometryList[i].ksize != 0) {
                    diskGeo &diskent = DiskGeometryList[i];

                    if (diskent.bytespersect == sector_size) {
                        ent = findSector(0,0,diskent.secttrack);
                        if (ent != NULL) {
                            LOG_MSG("VFD disk probe: %u/%u/%u exists",0,0,diskent.secttrack);
                            if (sectors < diskent.secttrack)
                                sectors = diskent.secttrack;
                        }
                    }

                    i++;
                }
            }

            if (sector_size != 0 && sectors != 0) {
                i=0;
                while (DiskGeometryList[i].ksize != 0) {
                    diskGeo &diskent = DiskGeometryList[i];

                    if (diskent.bytespersect == sector_size && diskent.secttrack >= sectors) {
                        ent = findSector(0,diskent.cylcount-1,sectors);
                        if (ent != NULL) {
                            LOG_MSG("VFD disk probe: %u/%u/%u exists",0,diskent.cylcount-1,sectors);
                            if (cylinders < diskent.cylcount)
                                cylinders = diskent.cylcount;
                        }
                    }

                    i++;
                }
            }

            if (sector_size != 0 && sectors != 0 && cylinders != 0) {
                ent = findSector(1,0,sectors);
                if (ent != NULL) {
                    LOG_MSG("VFD disk probe: %u/%u/%u exists",1,0,sectors);
                    heads = 2;
                }
            }

            // TODO: drive_fat.cpp should use an extension to this API to allow changing the sectors/track
            //       according to what it reads from the MS-DOS BIOS parameter block, just like real MS-DOS.
            //       This would allow better representation of strange disk formats such as the "extended"
            //       floppy format that Microsoft used to use for Word 95 and Windows 95 install floppies.

            LOG_MSG("VFD geometry detection: C/H/S %u/%u/%u %u bytes/sector",
                cylinders, heads, sectors, sector_size);

            if (sector_size != 0 && sectors != 0 && cylinders != 0 && heads != 0)
                founddisk = true;

            if(!founddisk) {
                active = false;
            } else {
                incrementFDD();
            }
        }
	}
}

imageDiskVFD::~imageDiskVFD() {
    if(diskimg != NULL) {
        fclose(diskimg);
        diskimg=NULL; 
    }
}

