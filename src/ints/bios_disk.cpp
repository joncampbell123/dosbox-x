/*
 *  Copyright (C) 2002-2013  The DOSBox Team
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

diskGeo DiskGeometryList[] = {
	{ 160,  8, 1, 40, 0},
	{ 180,  9, 1, 40, 0},
	{ 200, 10, 1, 40, 0},
	{ 320,  8, 2, 40, 1},
	{ 360,  9, 2, 40, 1},
	{ 400, 10, 2, 40, 1},
	{ 720,  9, 2, 80, 3},
	{1200, 15, 2, 80, 2},
	{1440, 18, 2, 80, 4},
	{2880, 36, 2, 80, 6},
	{0, 0, 0, 0, 0}
};

Bitu call_int13;
Bitu diskparm0, diskparm1;
static Bit8u last_status;
static Bit8u last_drive;
Bit16u imgDTASeg;
RealPt imgDTAPtr;
DOS_DTA *imgDTA;
bool killRead;
static bool swapping_requested;

void CMOS_SetRegister(Bitu regNr, Bit8u val); //For setting equipment word

/* 2 floppys and 2 harddrives, max */
imageDisk *imageDiskList[MAX_DISK_IMAGES];
imageDisk *diskSwap[MAX_SWAPPABLE_DISKS];
Bits swapPosition;

imageDisk *GetINT13HardDrive(unsigned char drv) {
	if (drv < 0x80 || drv >= (0x80+MAX_DISK_IMAGES-2))
		return NULL;

	return imageDiskList[drv-0x80];
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
			LOG_MSG("Loaded disk %d from swaplist position %d - \"%s\"", diskcount, swapPos, diskSwap[swapPos]->diskname);
			imageDiskList[diskcount] = diskSwap[swapPos];
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
	LOG_MSG("Diskcaching reset for normal mounted drives.");
	for(Bitu i=0;i<DOS_DRIVES;i++) {
		if (Drives[i]) Drives[i]->EmptyCache();
	}
	swapPosition++;
	if(diskSwap[swapPosition] == NULL) swapPosition = 0;
	swapInDisks();
	swapping_requested = true;
}


Bit8u imageDisk::Read_Sector(Bit32u head,Bit32u cylinder,Bit32u sector,void * data) {
	Bit32u sectnum;

	sectnum = ( (cylinder * heads + head) * sectors ) + sector - 1L;

	return Read_AbsoluteSector(sectnum, data);
}

Bit8u imageDisk::Read_AbsoluteSector(Bit32u sectnum, void * data) {
	Bit64u bytenum;

	bytenum = (Bit64u)sectnum * sector_size;

	//LOG_MSG("Reading sectors %ld at bytenum %I64d", sectnum, bytenum);

	fseeko64(diskimg,bytenum,SEEK_SET);
	fread(data, 1, sector_size, diskimg);

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

	//LOG_MSG("Writing sectors to %ld at bytenum %d", sectnum, bytenum);

	fseeko64(diskimg,bytenum,SEEK_SET);
	size_t ret=fwrite(data, sector_size, 1, diskimg);

	return ((ret>0)?0x00:0x05);

}

imageDisk::imageDisk(FILE *imgFile, Bit8u *imgName, Bit32u imgSizeK, bool isHardDisk) {
	heads = 0;
	cylinders = 0;
	sectors = 0;
	sector_size = 512;
	diskimg = imgFile;
	
	memset(diskname,0,512);
	if(strlen((const char *)imgName) > 511) {
		memcpy(diskname, imgName, 511);
	} else {
		strcpy((char *)diskname, (const char *)imgName);
	}

	active = false;
	hardDrive = isHardDisk;
	if(!isHardDisk) {
		Bit8u i=0;
		bool founddisk = false;
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
		LOG(LOG_BIOS,LOG_ERROR)("Disk %d non-existant", driveNum);
		last_status = 0x01;
		CALLBACK_SCF(true);
		return true;
	}
	if(imageDiskList[driveNum] == NULL) {
		LOG(LOG_BIOS,LOG_ERROR)("Disk %d not active", driveNum);
		last_status = 0x01;
		CALLBACK_SCF(true);
		return true;
	}
	if(!imageDiskList[driveNum]->active) {
		LOG(LOG_BIOS,LOG_ERROR)("Disk %d not active", driveNum);
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
		fprintf(stderr,"WARNING: Format track ignored\n");
		CALLBACK_SCF(false);
		reg_ah = 0x00;
		break;
	case 0x06: /* Format track set bad sector flags */
		/* ignore it. I just fucking want FORMAT.COM to write the FAT structure for God's sake */
		fprintf(stderr,"WARNING: Format track set bad sector flags ignored (6)\n");
		CALLBACK_SCF(false);
		reg_ah = 0x00;
		break;
	case 0x07: /* Format track set bad sector flags */
		/* ignore it. I just fucking want FORMAT.COM to write the FAT structure for God's sake */
		fprintf(stderr,"WARNING: Format track set bad sector flags ignored (7)\n");
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
	default:
		LOG(LOG_BIOS,LOG_ERROR)("INT13: Function %x called on drive %x (dos drive %d)", reg_ah,  reg_dl, drivenum);
		reg_ah=0xff;
		CALLBACK_SCF(true);
	}
	return CBRET_NONE;
}


void BIOS_SetupDisks(void) {
/* TODO Start the time correctly */
	call_int13=CALLBACK_Allocate();	
	CALLBACK_Setup(call_int13,&INT13_DiskHandler,CB_INT13,"Int 13 Bios disk");
	RealSetVec(0x13,CALLBACK_RealPointer(call_int13));
	int i;
	for(i=0;i<4;i++) {
		imageDiskList[i] = NULL;
	}

	for(i=0;i<MAX_SWAPPABLE_DISKS;i++) {
		diskSwap[i] = NULL;
	}

	diskparm0 = CALLBACK_Allocate();
	diskparm1 = CALLBACK_Allocate();
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

	MAPPER_AddHandler(swapInNextDisk,MK_f4,MMOD1,"swapimg","Swap Image");
	killRead = false;
	swapping_requested = false;
}
