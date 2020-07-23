/*
 *  Copyright (C) 2002-2020  The DOSBox Team
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
#include "callback.h"
#include "bios.h"
#include "bios_disk.h"
#include "regs.h"
#include "mem.h"
#include "dos_inc.h" /* for Drives[] */
#include "../dos/drives.h"
#include "mapper.h"
#include "ide.h"

#if defined(_MSC_VER)
# pragma warning(disable:4244) /* const fmath::local::uint64_t to double possible loss of data */
#endif

extern int bootdrive;
extern bool int13_extensions_enable, bootguest, bootvm, use_quick_reboot;

diskGeo DiskGeometryList[] = {
    { 160,  8, 1, 40, 0, 512,  64, 1, 0xFE},      // IBM PC double density 5.25" single-sided 160KB
    { 180,  9, 1, 40, 0, 512,  64, 2, 0xFC},      // IBM PC double density 5.25" single-sided 180KB
    { 200, 10, 1, 40, 0, 512,   0, 0,    0},      // DEC Rainbow double density 5.25" single-sided 200KB (I think...)
    { 320,  8, 2, 40, 1, 512, 112, 2, 0xFF},      // IBM PC double density 5.25" double-sided 320KB
    { 360,  9, 2, 40, 1, 512, 112, 2, 0xFD},      // IBM PC double density 5.25" double-sided 360KB
    { 400, 10, 2, 40, 1, 512,   0, 0,    0},      // DEC Rainbow double density 5.25" double-sided 400KB (I think...)
    { 640,  8, 2, 80, 3, 512, 112, 2, 0xFB},      // IBM PC double density 3.5" double-sided 640KB
    { 720,  9, 2, 80, 3, 512, 112, 2, 0xF9},      // IBM PC double density 3.5" double-sided 720KB
    {1200, 15, 2, 80, 2, 512, 224, 1, 0xF9},      // IBM PC double density 5.25" double-sided 1.2MB
    {1440, 18, 2, 80, 4, 512, 224, 1, 0xF0},      // IBM PC high density 3.5" double-sided 1.44MB
    {2880, 36, 2, 80, 6, 512, 240, 2, 0xF0},      // IBM PC high density 3.5" double-sided 2.88MB

    {1232,  8, 2, 77, 7, 1024,192, 1, 0xFE},      // NEC PC-98 high density 3.5" double-sided 1.2MB "3-mode"

    {   0,  0, 0,  0, 0,    0,  0, 0,    0}
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
bool imageDiskChange[MAX_DISK_IMAGES]={false};
imageDisk *imageDiskList[MAX_DISK_IMAGES]={NULL};
imageDisk *diskSwap[MAX_SWAPPABLE_DISKS]={NULL};
Bit32s swapPosition;

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
            if (i >= 2) IDE_Hard_Disk_Detach(i);
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

//update BIOS disk parameter tables for first two hard drives
void updateDPT(void) {
    Bit32u tmpheads, tmpcyl, tmpsect, tmpsize;
    PhysPt dpphysaddr[2] = { CALLBACK_PhysPointer(diskparm0), CALLBACK_PhysPointer(diskparm1) };
    for (int i = 0; i < 2; i++) {
        tmpheads = 0; tmpcyl = 0; tmpsect = 0; tmpsize = 0;
        if (imageDiskList[i + 2] != NULL) {
            imageDiskList[i + 2]->Get_Geometry(&tmpheads, &tmpcyl, &tmpsect, &tmpsize);
        }
        phys_writew(dpphysaddr[i], (Bit16u)tmpcyl);
        phys_writeb(dpphysaddr[i] + 0x2, (Bit8u)tmpheads);
        phys_writew(dpphysaddr[i] + 0x3, 0);
        phys_writew(dpphysaddr[i] + 0x5, tmpcyl == 0 ? 0 : (Bit16u)-1);
        phys_writeb(dpphysaddr[i] + 0x7, 0);
        phys_writeb(dpphysaddr[i] + 0x8, tmpcyl == 0 ? 0 : (0xc0 | (((tmpheads) > 8) << 3)));
        phys_writeb(dpphysaddr[i] + 0x9, 0);
        phys_writeb(dpphysaddr[i] + 0xa, 0);
        phys_writeb(dpphysaddr[i] + 0xb, 0);
        phys_writew(dpphysaddr[i] + 0xc, (Bit16u)tmpcyl);
        phys_writeb(dpphysaddr[i] + 0xe, (Bit8u)tmpsect);
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

int swapInDisksSpecificDrive = -1;
// -1 = swap across A: and B: (DOSBox / DOSBox-X default behavior)
//  0 = swap across A: only
//  1 = swap across B: only

void swapInDisks(void) {
    bool allNull = true;
    Bit32s diskcount = 0;
    Bits diskswapcount = 2;
    Bits diskswapdrive = 0;
    Bit32s swapPos = swapPosition;
    Bit32s i;

    /* Check to make sure that  there is at least one setup image */
    for(i=0;i<MAX_SWAPPABLE_DISKS;i++) {
        if(diskSwap[i]!=NULL) {
            allNull = false;
            break;
        }
    }

    /* No disks setup... fail */
    if (allNull) return;

    /* if a specific drive is to be swapped, then adjust to focus on it */
    if (swapInDisksSpecificDrive >= 0 && swapInDisksSpecificDrive <= 1) {
        diskswapdrive = swapInDisksSpecificDrive;
        diskswapcount = 1;
    }

    /* If only one disk is loaded, this loop will load the same disk in dive A and drive B */
    while(diskcount < diskswapcount) {
        if(diskSwap[swapPos] != NULL) {
            LOG_MSG("Loaded drive %d disk %d from swaplist position %d - \"%s\"", (int)diskswapdrive, (int)diskcount, (int)swapPos, diskSwap[swapPos]->diskname.c_str());

            if (imageDiskList[diskswapdrive] != NULL)
                imageDiskList[diskswapdrive]->Release();

            imageDiskList[diskswapdrive] = diskSwap[swapPos];
            imageDiskList[diskswapdrive]->Addref();

            imageDiskChange[diskswapdrive] = true;

            diskcount++;
            diskswapdrive++;
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


Bit8u imageDisk::Read_Sector(Bit32u head,Bit32u cylinder,Bit32u sector,void * data,unsigned int req_sector_size) {
    Bit32u sectnum;

    if (req_sector_size == 0)
        req_sector_size = sector_size;
    if (req_sector_size != sector_size)
        return 0x05;

    sectnum = ( (cylinder * heads + head) * sectors ) + sector - 1L;

    return Read_AbsoluteSector(sectnum, data);
}

Bit8u imageDisk::Read_AbsoluteSector(Bit32u sectnum, void * data) {
    Bit64u bytenum,res;
    int got;

    bytenum = (Bit64u)sectnum * (Bit64u)sector_size;
    if ((bytenum + sector_size) > this->image_length) {
        LOG_MSG("Attempt to read invalid sector in Read_AbsoluteSector for sector %lu.\n", (unsigned long)sectnum);
        return 0x05;
    }
    bytenum += image_base;

    //LOG_MSG("Reading sectors %ld at bytenum %I64d", sectnum, bytenum);

    fseeko64(diskimg,(fseek_ofs_t)bytenum,SEEK_SET);
    res = (Bit64u)ftello64(diskimg);
    if (res != bytenum) {
        LOG_MSG("fseek() failed in Read_AbsoluteSector for sector %lu. Want=%llu Got=%llu\n",
            (unsigned long)sectnum,(unsigned long long)bytenum,(unsigned long long)res);
        return 0x05;
    }

    got = (int)fread(data, 1, sector_size, diskimg);
    if ((unsigned int)got != sector_size) {
        LOG_MSG("fread() failed in Read_AbsoluteSector for sector %lu. Want=%u got=%d\n",
            (unsigned long)sectnum,(unsigned int)sector_size,(unsigned int)got);
        return 0x05;
    }

    return 0x00;
}

Bit8u imageDisk::Write_Sector(Bit32u head,Bit32u cylinder,Bit32u sector,const void * data,unsigned int req_sector_size) {
    Bit32u sectnum;

    if (req_sector_size == 0)
        req_sector_size = sector_size;
    if (req_sector_size != sector_size)
        return 0x05;

    sectnum = ( (cylinder * heads + head) * sectors ) + sector - 1L;

    return Write_AbsoluteSector(sectnum, data);
}


Bit8u imageDisk::Write_AbsoluteSector(Bit32u sectnum, const void *data) {
    Bit64u bytenum;

    bytenum = (Bit64u)sectnum * sector_size;
    if ((bytenum + sector_size) > this->image_length) {
        LOG_MSG("Attempt to read invalid sector in Write_AbsoluteSector for sector %lu.\n", (unsigned long)sectnum);
        return 0x05;
    }
    bytenum += image_base;

    //LOG_MSG("Writing sectors to %ld at bytenum %d", sectnum, bytenum);

    fseeko64(diskimg,(fseek_ofs_t)bytenum,SEEK_SET);
    if ((Bit64u)ftello64(diskimg) != bytenum)
        LOG_MSG("WARNING: fseek() failed in Write_AbsoluteSector for sector %lu\n",(unsigned long)sectnum);

    size_t ret=fwrite(data, sector_size, 1, diskimg);

    return ((ret>0)?0x00:0x05);

}

void imageDisk::Set_Reserved_Cylinders(Bitu resCyl) {
    reserved_cylinders = resCyl;
}

Bit32u imageDisk::Get_Reserved_Cylinders() {
    return reserved_cylinders;
}

imageDisk::imageDisk(IMAGE_TYPE class_id) : class_id(class_id) {
}

imageDisk::imageDisk(FILE* diskimg, const char* diskName, Bit32u cylinders, Bit32u heads, Bit32u sectors, Bit32u sector_size, bool hardDrive) {
    if (diskName) this->diskname = diskName;
    this->cylinders = cylinders;
    this->heads = heads;
    this->sectors = sectors;
    image_base = 0;
    this->image_length = (Bit64u)cylinders * heads * sectors * sector_size;
    refcount = 0;
    this->sector_size = sector_size;
    this->diskSizeK = this->image_length / 1024;
    reserved_cylinders = 0;
    this->diskimg = diskimg;
    class_id = ID_BASE;
    active = true;
    this->hardDrive = hardDrive;
    floppytype = 0;
}

/* .HDI and .FDI header (NP2) */
#pragma pack(push,1)
typedef struct {
    uint8_t dummy[4];           // +0x00
    uint8_t hddtype[4];         // +0x04
    uint8_t headersize[4];      // +0x08
    uint8_t hddsize[4];         // +0x0C
    uint8_t sectorsize[4];      // +0x10
    uint8_t sectors[4];         // +0x14
    uint8_t surfaces[4];        // +0x18
    uint8_t cylinders[4];       // +0x1C
} HDIHDR;                       // =0x20

typedef struct {
	uint8_t	dummy[4];           // +0x00
	uint8_t	fddtype[4];         // +0x04
	uint8_t	headersize[4];      // +0x08
	uint8_t	fddsize[4];         // +0x0C
	uint8_t	sectorsize[4];      // +0x10
	uint8_t	sectors[4];         // +0x14
	uint8_t	surfaces[4];        // +0x18
	uint8_t	cylinders[4];       // +0x1C
} FDIHDR;                       // =0x20

typedef struct {
	char	sig[16];            // +0x000
	char	comment[0x100];     // +0x010
	UINT8	headersize[4];      // +0x110
    uint8_t prot;               // +0x114
    uint8_t nhead;              // +0x115
    uint8_t _unknown_[10];      // +0x116
} NFDHDR;                       // =0x120

typedef struct {
	char	sig[16];            // +0x000
	char	comment[0x100];     // +0x010
	UINT8	headersize[4];      // +0x110
    uint8_t prot;               // +0x114
    uint8_t nhead;              // +0x115
    uint8_t _unknown_[10];      // +0x116
    uint32_t trackheads[164];   // +0x120
    uint32_t addinfo;           // +0x3b0
    uint8_t _unknown2_[12];     // +0x3b4
} NFDHDRR1;                     // =0x3c0

typedef struct {
    uint8_t log_cyl;            // +0x0
    uint8_t log_head;           // +0x1
    uint8_t log_rec;            // +0x2
    uint8_t sec_len_pow2;       // +0x3         sz = 128 << len_pow2
    uint8_t flMFM;              // +0x4
    uint8_t flDDAM;             // +0x5
    uint8_t byStatus;           // +0x6
    uint8_t bySTS0;             // +0x7
    uint8_t bySTS1;             // +0x8
    uint8_t bySTS2;             // +0x9
    uint8_t byRetry;            // +0xA
    uint8_t byPDA;              // +0xB
    uint8_t _unknown_[4];       // +0xC
} NFDHDR_ENTRY;                 // =0x10

typedef struct {
    char        szFileID[15];                 // 識別ID "T98HDDIMAGE.R0"
    char        Reserve1[1];                  // 予約
    char        szComment[0x100];             // イメージコメント(ASCIIz)
    uint32_t    dwHeadSize;                   // ヘッダ部のサイズ
    uint32_t    dwCylinder;                   // シリンダ数
    uint16_t    wHead;                        // ヘッド数
    uint16_t    wSect;                        // １トラックあたりのセクタ数
    uint16_t    wSectLen;                     // セクタ長
    char        Reserve2[2];                  // 予約
    char        Reserve3[0xe0];               // 予約
}NHD_FILE_HEAD,*LP_NHD_FILE_HEAD;
#pragma pack(pop)

imageDisk::imageDisk(FILE* imgFile, Bit8u* imgName, Bit32u imgSizeK, bool isHardDisk) : diskSizeK(imgSizeK), diskimg(imgFile), image_length((Bit64u)imgSizeK * 1024) {
    if (imgName != NULL)
        diskname = (const char*)imgName;

    active = false;
    hardDrive = isHardDisk;
    if(!isHardDisk) {
        bool founddisk = false;

        if (imgName != NULL) {
            const char *ext = strrchr((char*)imgName,'.');
            if (ext != NULL) {
                if (!strcasecmp(ext,".fdi")) {
                    if (imgSizeK >= 160) {
                        FDIHDR fdihdr;

                        // PC-98 .FDI images appear to be 4096 bytes of a short header and many zeros.
                        // followed by a straight sector dump of the disk. The header is NOT NECESSARILY
                        // 4KB in size, but usually is.
                        LOG_MSG("Image file has .FDI extension, assuming FDI image and will take on parameters in header.");

                        assert(sizeof(fdihdr) == 0x20);
                        if (fseek(imgFile,0,SEEK_SET) == 0 && ftell(imgFile) == 0 &&
                            fread(&fdihdr,sizeof(fdihdr),1,imgFile) == 1) {
                            uint32_t ofs = host_readd(fdihdr.headersize);
                            uint32_t fddsize = host_readd(fdihdr.fddsize); /* includes header */
                            uint32_t sectorsize = host_readd(fdihdr.sectorsize);

                            if (sectorsize != 0 && ((sectorsize & (sectorsize - 1)) == 0/*is power of 2*/) &&
                                sectorsize >= 256 && sectorsize <= 1024 &&
                                ofs != 0 && (ofs % sectorsize) == 0/*offset is nonzero and multiple of sector size*/ &&
                                (ofs % 1024) == 0/*offset is a multiple of 1024 because of imgSizeK*/ &&
                                fddsize >= sectorsize && (fddsize/1024) <= (imgSizeK+4)) {

                                founddisk = true;
                                sector_size = sectorsize;
                                imgSizeK -= (ofs / 1024);
                                image_base = ofs;
                                image_length -= ofs;
                                LOG_MSG("FDI header: sectorsize is %u bytes/sector, header is %u bytes, fdd size (plus header) is %u bytes",
                                    (unsigned int)sectorsize,(unsigned int)ofs,(unsigned int)fddsize);

                                /* take on the geometry. */
                                sectors = host_readd(fdihdr.sectors);
                                heads = host_readd(fdihdr.surfaces);
                                cylinders = host_readd(fdihdr.cylinders);
                                LOG_MSG("FDI: Geometry is C/H/S %u/%u/%u",
                                    (unsigned int)cylinders,(unsigned int)heads,(unsigned int)sectors);
                            }
                            else {
                                LOG_MSG("FDI header rejected. sectorsize=%u headersize=%u fddsize=%u",
                                    (unsigned int)sectorsize,(unsigned int)ofs,(unsigned int)fddsize);
                            }
                        }
                        else {
                            LOG_MSG("Unable to read .FDI header");
                        }
                    }
                }
            }
        }

        if (sectors == 0 && heads == 0 && cylinders == 0) {
            Bit8u i=0;
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
        }
        if(!founddisk) {
            active = false;
        }
    }
    else { /* hard disk */
        if (imgName != NULL) {
            char *ext = strrchr((char*)imgName,'.');
            if (ext != NULL) {
                if (!strcasecmp(ext,".nhd")) {
                    if (imgSizeK >= 160) {
                        NHD_FILE_HEAD nhdhdr;

                        LOG_MSG("Image file has .NHD extension, assuming NHD image and will take on parameters in header.");

                        assert(sizeof(nhdhdr) == 0x200);
                        if (fseek(imgFile,0,SEEK_SET) == 0 && ftell(imgFile) == 0 &&
                            fread(&nhdhdr,sizeof(nhdhdr),1,imgFile) == 1 &&
                            host_readd((ConstHostPt)(&nhdhdr.dwHeadSize)) >= 0x200 &&
                            !memcmp(nhdhdr.szFileID,"T98HDDIMAGE.R0\0",15)) {
                            uint32_t ofs = host_readd((ConstHostPt)(&nhdhdr.dwHeadSize));
                            uint32_t sectorsize = host_readw((ConstHostPt)(&nhdhdr.wSectLen));

                            if (sectorsize != 0 && ((sectorsize & (sectorsize - 1)) == 0/*is power of 2*/) &&
                                sectorsize >= 256 && sectorsize <= 1024 &&
                                ofs != 0 && (ofs % sectorsize) == 0/*offset is nonzero and multiple of sector size*/) {

                                sector_size = sectorsize;
                                imgSizeK -= (ofs / 1024);
                                image_base = ofs;
                                image_length -= ofs;
                                LOG_MSG("NHD header: sectorsize is %u bytes/sector, header is %u bytes",
                                        (unsigned int)sectorsize,(unsigned int)ofs);

                                /* take on the geometry.
                                 * PC-98 IPL1 support will need it to make sense of the partition table. */
                                sectors = host_readw((ConstHostPt)(&nhdhdr.wSect));
                                heads = host_readw((ConstHostPt)(&nhdhdr.wHead));
                                cylinders = host_readd((ConstHostPt)(&nhdhdr.dwCylinder));
                                LOG_MSG("NHD: Geometry is C/H/S %u/%u/%u",
                                        (unsigned int)cylinders,(unsigned int)heads,(unsigned int)sectors);
                            }
                            else {
                                LOG_MSG("NHD header rejected. sectorsize=%u headersize=%u",
                                        (unsigned int)sectorsize,(unsigned int)ofs);
                            }
                        }
                        else {
                            LOG_MSG("Unable to read .NHD header");
                        }
                    }
                }
                if (!strcasecmp(ext,".hdi")) {
                    if (imgSizeK >= 160) {
                        HDIHDR hdihdr;

                        // PC-98 .HDI images appear to be 4096 bytes of a short header and many zeros.
                        // followed by a straight sector dump of the disk. The header is NOT NECESSARILY
                        // 4KB in size, but usually is.
                        LOG_MSG("Image file has .HDI extension, assuming HDI image and will take on parameters in header.");

                        assert(sizeof(hdihdr) == 0x20);
                        if (fseek(imgFile,0,SEEK_SET) == 0 && ftell(imgFile) == 0 &&
                            fread(&hdihdr,sizeof(hdihdr),1,imgFile) == 1) {
                            uint32_t ofs = host_readd(hdihdr.headersize);
                            uint32_t hddsize = host_readd(hdihdr.hddsize); /* includes header */
                            uint32_t sectorsize = host_readd(hdihdr.sectorsize);

                            if (sectorsize != 0 && ((sectorsize & (sectorsize - 1)) == 0/*is power of 2*/) &&
                                sectorsize >= 256 && sectorsize <= 1024 &&
                                ofs != 0 && (ofs % sectorsize) == 0/*offset is nonzero and multiple of sector size*/ &&
                                (ofs % 1024) == 0/*offset is a multiple of 1024 because of imgSizeK*/ &&
                                hddsize >= sectorsize && (hddsize/1024) <= (imgSizeK+4)) {

                                sector_size = sectorsize;
                                image_base = ofs;
                                image_length -= ofs;
                                LOG_MSG("HDI header: sectorsize is %u bytes/sector, header is %u bytes, hdd size (plus header) is %u bytes",
                                    (unsigned int)sectorsize,(unsigned int)ofs,(unsigned int)hddsize);

                                /* take on the geometry.
                                 * PC-98 IPL1 support will need it to make sense of the partition table. */
                                sectors = host_readd(hdihdr.sectors);
                                heads = host_readd(hdihdr.surfaces);
                                cylinders = host_readd(hdihdr.cylinders);
                                LOG_MSG("HDI: Geometry is C/H/S %u/%u/%u",
                                    (unsigned int)cylinders,(unsigned int)heads,(unsigned int)sectors);
                            }
                            else {
                                LOG_MSG("HDI header rejected. sectorsize=%u headersize=%u hddsize=%u",
                                    (unsigned int)sectorsize,(unsigned int)ofs,(unsigned int)hddsize);
                            }
                        }
                        else {
                            LOG_MSG("Unable to read .HDI header");
                        }
                    }
                }
            }
        }

        if (sectors == 0 || heads == 0 || cylinders == 0)
            active = false;
    }
}

void imageDisk::Set_Geometry(Bit32u setHeads, Bit32u setCyl, Bit32u setSect, Bit32u setSectSize) {
    Bitu bigdisk_shift = 0;

    if (IS_PC98_ARCH) {
        /* TODO: PC-98 has it's own 4096 cylinder limit */
    }
    else {
        if(setCyl > 16384) LOG_MSG("This disk image is too big.");
        else if(setCyl > 8192) bigdisk_shift = 4;
        else if(setCyl > 4096) bigdisk_shift = 3;
        else if(setCyl > 2048) bigdisk_shift = 2;
        else if(setCyl > 1024) bigdisk_shift = 1;
    }

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

static Bit8u GetDosDriveNumber(Bit8u biosNum) {
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

static bool driveInactive(Bit8u driveNum) {
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
    Bit8u  drivenum;
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

    // NOTE: the 0xff error code returned in some cases is questionable; 0x01 seems more correct
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
            if (drivenum >= DOS_DRIVES || !Drives[drivenum] || Drives[drivenum]->isRemovable()) {
                reg_ah = 0x01;
                CALLBACK_SCF(true);
                return CBRET_NONE;
            }
            // Inherit the Earth cdrom and Amberstar use it as a disk test
            if (((reg_dl&0x80)==0x80) && (reg_dh==0) && ((reg_cl&0x3f)==1)) {
                if (reg_ch==0) {
                    PhysPt ptr = PhysMake(SegValue(es),reg_bx);
                    // write some MBR data into buffer for Amberstar installer
                    mem_writeb(ptr+0x1be,0x80); // first partition is active
                    mem_writeb(ptr+0x1c2,0x06); // first partition is FAT16B
                }
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

        /* INT 13h is limited to 512 bytes/sector (as far as I know).
         * The sector buffer in this function is limited to 512 bytes/sector,
         * so this is also a protection against overruning the stack if you
         * mount a PC-98 disk image (1024 bytes/sector) and try to read it with INT 13h. */
        if (imageDiskList[drivenum]->sector_size > sizeof(sectbuf)) {
            LOG(LOG_MISC,LOG_DEBUG)("INT 13h: Read failed because disk bytes/sector on drive %c is too large",(char)drivenum+'A');

            imageDiskChange[drivenum] = false;

            reg_ah = 0x80; /* timeout */
            CALLBACK_SCF(true);
            return CBRET_NONE;
        }

        /* If the disk changed, the first INT 13h read will signal an error and set AH = 0x06 to indicate disk change */
        if (drivenum < 2 && imageDiskChange[drivenum]) {
            LOG(LOG_MISC,LOG_DEBUG)("INT 13h: Failing first read of drive %c to indicate disk change",(char)drivenum+'A');

            imageDiskChange[drivenum] = false;

            reg_ah = 0x06; /* diskette changed or removed */
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
        
        if(driveInactive(drivenum) || !imageDiskList[drivenum]) {
            reg_ah = 0xff;
            CALLBACK_SCF(true);
            return CBRET_NONE;
        }

        /* INT 13h is limited to 512 bytes/sector (as far as I know).
         * The sector buffer in this function is limited to 512 bytes/sector,
         * so this is also a protection against overruning the stack if you
         * mount a PC-98 disk image (1024 bytes/sector) and try to read it with INT 13h. */
        if (imageDiskList[drivenum]->sector_size > sizeof(sectbuf)) {
            LOG(LOG_MISC,LOG_DEBUG)("INT 13h: Write failed because disk bytes/sector on drive %c is too large",(char)drivenum+'A');

            imageDiskChange[drivenum] = false;

            reg_ah = 0x80; /* timeout */
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
        if(driveInactive(drivenum)) {
            reg_ah = last_status;
            return CBRET_NONE;
        }

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
        if (driveInactive(drivenum)) {
            reg_ah = 0xff;
            CALLBACK_SCF(true);
            return CBRET_NONE;
        }
        CALLBACK_SCF(false);
        reg_ah = 0x00;
        break;
    case 0x06: /* Format track set bad sector flags */
        /* ignore it. I just fucking want FORMAT.COM to write the FAT structure for God's sake */
        LOG_MSG("WARNING: Format track set bad sector flags ignored (6)\n");
        if (driveInactive(drivenum)) {
            reg_ah = 0xff;
            CALLBACK_SCF(true);
            return CBRET_NONE;
        }
        CALLBACK_SCF(false);
        reg_ah = 0x00;
        break;
    case 0x07: /* Format track set bad sector flags */
        /* ignore it. I just fucking want FORMAT.COM to write the FAT structure for God's sake */
        LOG_MSG("WARNING: Format track set bad sector flags ignored (7)\n");
        if (driveInactive(drivenum)) {
            reg_ah = 0xff;
            CALLBACK_SCF(true);
            return CBRET_NONE;
        }
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
        else tmpcyl--;      // cylinder count -> max cylinder
        if (tmpheads==0) LOG(LOG_BIOS,LOG_ERROR)("INT13 DrivParm: head count zero!");
        else tmpheads--;    // head count -> max head

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
        if (reg_dl&0x80) {  // harddisks
            reg_dl = 0;
            for (int index = 2; index < MAX_DISK_IMAGES; index++) {
                if (imageDiskList[index] != NULL) reg_dl++;
            }
        } else {        // floppy disks
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
    case 0x15: /* Get disk type */
        /* Korean Powerdolls uses this to detect harddrives */
        LOG(LOG_BIOS,LOG_WARN)("INT13: Get disktype used!");
        if (any_images) {
            if(driveInactive(drivenum)) {
                last_status = 0x07;
                reg_ah = last_status;
                CALLBACK_SCF(true);
                return CBRET_NONE;
            }
            imageDiskList[drivenum]->Get_Geometry(&tmpheads, &tmpcyl, &tmpsect, &tmpsize);
            Bit64u largesize = tmpheads*tmpcyl*tmpsect*tmpsize;
            largesize/=512;
            Bit32u ts = static_cast<Bit32u>(largesize);
            reg_ah = (drivenum <2)?1:3; //With 2 for floppy MSDOS starts calling int 13 ah 16
            if(reg_ah == 3) {
                reg_cx = static_cast<Bit16u>(ts >>16);
                reg_dx = static_cast<Bit16u>(ts & 0xffff);
            }
            CALLBACK_SCF(false);
        } else {
            if (drivenum <DOS_DRIVES && (Drives[drivenum] != 0 || drivenum <2)) {
                if (drivenum <2) {
                    //TODO use actual size (using 1.44 for now).
                    reg_ah = 0x1; // type
//                  reg_cx = 0;
//                  reg_dx = 2880; //Only set size for harddrives.
                } else {
                    //TODO use actual size (using 105 mb for now).
                    reg_ah = 0x3; // type
                    reg_cx = 3;
                    reg_dx = 0x4800;
                }
                CALLBACK_SCF(false);
            } else { 
                LOG(LOG_BIOS,LOG_WARN)("INT13: no images, but invalid drive for call 15");
                reg_ah=0xff;
                CALLBACK_SCF(true);
            }
        }
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
            reg_ah=0x1; /* 1.x extension supported */
            reg_bx=0xaa55;  /* Extensions installed */
            reg_cx=0x1; /* Extended disk access functions (AH=42h-44h,47h,48h) supported */
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

        imageDiskList[drivenum]->Get_Geometry(&tmpheads, &tmpcyl, &tmpsect, &tmpsize);

        real_writew(segat,bufptr+0x00,bufsz);
        real_writew(segat,bufptr+0x02,0x0003);  /* C/H/S valid, DMA boundary errors handled */
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
    unsigned int i;

    if (IS_PC98_ARCH) {
        // TODO
        return;
    }

/* TODO Start the time correctly */
    call_int13=CALLBACK_Allocate(); 
    CALLBACK_Setup(call_int13,&INT13_DiskHandler,CB_INT13,"Int 13 Bios disk");
    RealSetVec(0x13,CALLBACK_RealPointer(call_int13));

    //release the drives after a soft reset
    if (!bootguest&&(bootvm||!use_quick_reboot)||bootdrive<0) FreeBIOSDiskList();

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

Bit8u imageDiskVFD::Read_Sector(Bit32u head,Bit32u cylinder,Bit32u sector,void * data,unsigned int req_sector_size) {
    const vfdentry *ent;

    if (req_sector_size == 0)
        req_sector_size = sector_size;

//    LOG_MSG("VFD read sector: CHS %u/%u/%u sz=%u",cylinder,head,sector,req_sector_size);

    ent = findSector(head,cylinder,sector,req_sector_size);
    if (ent == NULL) return 0x05;
    if (ent->getSectorSize() != req_sector_size) return 0x05;

    if (ent->hasSectorData()) {
        fseek(diskimg,(long)ent->data_offset,SEEK_SET);
        if ((uint32_t)ftell(diskimg) != ent->data_offset) return 0x05;
        if (fread(data,req_sector_size,1,diskimg) != 1) return 0x05;
        return 0;
    }
    else if (ent->hasFill()) {
        memset(data,ent->fillbyte,req_sector_size);
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

imageDiskVFD::vfdentry *imageDiskVFD::findSector(Bit8u head,Bit8u track,Bit8u sector/*TODO: physical head?*/,unsigned int req_sector_size) {
    std::vector<imageDiskVFD::vfdentry>::iterator i = dents.begin();
    unsigned char szb=0xFF;

    if (req_sector_size == 0)
        req_sector_size = sector_size;

    if (req_sector_size != ~0U) {
        unsigned int c = req_sector_size;
        while (c >= 128U) {
            c >>= 1U;
            szb++;
        }

//        LOG_MSG("req=%u c=%u szb=%u",req_sector_size,c,szb);

        if (szb > 8 || c != 64U)
            return NULL;
    }

    while (i != dents.end()) {
        const imageDiskVFD::vfdentry &ent = *i;

        if (ent.head == head &&
            ent.track == track &&
            ent.sector == sector &&
            (ent.sizebyte == szb || req_sector_size == ~0U))
            return &(*i);

        ++i;
    }

    return NULL;
}

Bit8u imageDiskVFD::Write_Sector(Bit32u head,Bit32u cylinder,Bit32u sector,const void * data,unsigned int req_sector_size) {
    unsigned long new_offset;
    unsigned char tmp[12];
    vfdentry *ent;

//    LOG_MSG("VFD write sector: CHS %u/%u/%u",cylinder,head,sector);

    if (req_sector_size == 0)
        req_sector_size = sector_size;

    ent = findSector(head,cylinder,sector,req_sector_size);
    if (ent == NULL) return 0x05;
    if (ent->getSectorSize() != req_sector_size) return 0x05;

    if (ent->hasSectorData()) {
        fseek(diskimg,(long)ent->data_offset,SEEK_SET);
        if ((uint32_t)ftell(diskimg) != ent->data_offset) return 0x05;
        if (fwrite(data,req_sector_size,1,diskimg) != 1) return 0x05;
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
                    if ((++i) == req_sector_size) {
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
            fseek(diskimg,(long)ent->entry_offset,SEEK_SET);
            if ((uint32_t)ftell(diskimg) != ent->entry_offset) return 0x05;
            if (fread(tmp,12,1,diskimg) != 1) return 0x05;

            tmp[0x04] = ((unsigned char*)data)[0]; // change the fill byte

            LOG_MSG("VFD write: 'fill' sector changing fill byte to 0x%x",tmp[0x04]);

            fseek(diskimg,(long)ent->entry_offset,SEEK_SET);
            if ((uint32_t)ftell(diskimg) != ent->entry_offset) return 0x05;
            if (fwrite(tmp,12,1,diskimg) != 1) return 0x05;
        }
        else {
            fseek(diskimg,0,SEEK_END);
            new_offset = (unsigned long)ftell(diskimg);

            /* we have to change it from a fill sector to an actual sector */
            LOG_MSG("VFD write: changing 'fill' sector to one with data (data at %lu)",(unsigned long)new_offset);

            fseek(diskimg,(long)ent->entry_offset,SEEK_SET);
            if ((uint32_t)ftell(diskimg) != ent->entry_offset) return 0x05;
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

            fseek(diskimg,(long)ent->entry_offset,SEEK_SET);
            if ((uint32_t)ftell(diskimg) != ent->entry_offset) return 0x05;
            if (fwrite(tmp,12,1,diskimg) != 1) return 0x05;

            fseek(diskimg,(long)ent->data_offset,SEEK_SET);
            if ((uint32_t)ftell(diskimg) != ent->data_offset) return 0x05;
            if (fwrite(data,req_sector_size,1,diskimg) != 1) return 0x05;
        }
    }

    return 0x05;
}

Bit8u imageDiskVFD::Write_AbsoluteSector(Bit32u sectnum,const void *data) {
    unsigned int c,h,s;

    if (sectors == 0 || heads == 0)
        return 0x05;

    s = (sectnum % sectors) + 1;
    h = (sectnum / sectors) % heads;
    c = (sectnum / sectors / heads);
    return Write_Sector(h,c,s,data);
}

imageDiskVFD::imageDiskVFD(FILE *imgFile, Bit8u *imgName, Bit32u imgSizeK, bool isHardDisk) : imageDisk(ID_VFD) {
    (void)isHardDisk;//UNUSED
    unsigned char tmp[16];

    heads = 1;
    cylinders = 0;
    image_base = 0;
    sectors = 0;
    active = false;
    sector_size = 0;
    reserved_cylinders = 0;
    diskSizeK = imgSizeK;
    diskimg = imgFile;

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
    size_t readResult = fread(tmp,1,8,diskimg);
    if (readResult != 8) {
            LOG(LOG_IO, LOG_ERROR) ("Reading error in imageDiskVFD constructor\n");
            return;
    }

    if (!memcmp(tmp,"VFD1.",5)) {
        uint32_t stop_at = 0xC3FC;
        unsigned long entof;

        // load table.
        // we have to determine as we go where to stop reading.
        // the source of info I read assumes the whole header (and table)
        // is 0xC3FC bytes. I'm not inclined to assume that, so we go by
        // that OR the first sector offset whichever is smaller.
        // the table seems to trail off into a long series of 0xFF at the end.
        fseek(diskimg,0xDC,SEEK_SET);
        while ((entof=((unsigned long)ftell(diskimg)+12ul)) <= stop_at) {
            memset(tmp,0xFF,12);
            readResult = fread(tmp,12,1,diskimg);
            if (readResult != 1) {
                LOG(LOG_IO, LOG_ERROR) ("Reading error in imageDiskVFD constructor\n");
                return;
            }

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
            const vfdentry *ent;

            ent = findSector(/*head*/0,/*track*/0,/*sector*/1,~0U);
            if (ent != NULL) {
                if (ent->sizebyte <= 3) /* x <= 1024 */
                    sector_size = ent->getSectorSize();
            }

            /* oh yeah right, sure.
             * I suppose you're one of those FDD images where the sector size is 128 bytes/sector
             * in the boot sector and the rest is 256 bytes/sector elsewhere. I have no idea why
             * but quite a few FDD images have this arrangement. */
            if (sector_size != 0 && sector_size < 512) {
                ent = findSector(/*head*/0,/*track*/1,/*sector*/1,~0U);
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

            Bit8u i;
            if (sector_size != 0) {
                i=0;
                while (DiskGeometryList[i].ksize != 0) {
                    const diskGeo &diskent = DiskGeometryList[i];

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
                    const diskGeo &diskent = DiskGeometryList[i];

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

            bool founddisk = false;
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

// D88 *.D88 floppy disk format support

enum {
    D88_TRACKMAX        = 164,
    D88_HEADERSIZE      = 0x20 + (D88_TRACKMAX * 4)
};

#pragma pack(push,1)
typedef struct D88HEAD {
    char            fd_name[17];                // +0x00 Disk Name
    unsigned char   reserved1[9];               // +0x11 Reserved
    unsigned char   protect;                    // +0x1A Write Protect bit:4
    unsigned char   fd_type;                    // +0x1B Disk Format
    uint32_t        fd_size;                    // +0x1C Disk Size
    uint32_t        trackp[D88_TRACKMAX];       // +0x20 <array of DWORDs>     164 x 4 = 656 = 0x290
} D88HEAD;                                      // =0x2B0 total

typedef struct D88SEC {
    unsigned char   c;                          // +0x00
    unsigned char   h;                          // +0x01
    unsigned char   r;                          // +0x02
    unsigned char   n;                          // +0x03
    uint16_t        sectors;                    // +0x04 Sector Count
    unsigned char   mfm_flg;                    // +0x06 sides
    unsigned char   del_flg;                    // +0x07 DELETED DATA
    unsigned char   stat;                       // +0x08 STATUS (FDC ret)
    unsigned char   seektime;                   // +0x09 Seek Time
    unsigned char   reserved[3];                // +0x0A Reserved
    unsigned char   rpm_flg;                    // +0x0D rpm          0:1.2  1:1.44
    uint16_t        size;                       // +0x0E Sector Size
                                                // <sector contents follow>
} D88SEC;                                       // =0x10 total
#pragma pack(pop)

Bit8u imageDiskD88::Read_Sector(Bit32u head,Bit32u cylinder,Bit32u sector,void * data,unsigned int req_sector_size) {
    const vfdentry *ent;

    if (req_sector_size == 0)
        req_sector_size = sector_size;

//    LOG_MSG("D88 read sector: CHS %u/%u/%u sz=%u",cylinder,head,sector,req_sector_size);

    ent = findSector(head,cylinder,sector,req_sector_size);
    if (ent == NULL) return 0x05;
    if (ent->getSectorSize() != req_sector_size) return 0x05;

    fseek(diskimg,(long)ent->data_offset,SEEK_SET);
    if ((uint32_t)ftell(diskimg) != ent->data_offset) return 0x05;
    if (fread(data,req_sector_size,1,diskimg) != 1) return 0x05;
    return 0;
}

Bit8u imageDiskD88::Read_AbsoluteSector(Bit32u sectnum, void * data) {
    unsigned int c,h,s;

    if (sectors == 0 || heads == 0)
        return 0x05;

    s = (sectnum % sectors) + 1;
    h = (sectnum / sectors) % heads;
    c = (sectnum / sectors / heads);
    return Read_Sector(h,c,s,data);
}

imageDiskD88::vfdentry *imageDiskD88::findSector(Bit8u head,Bit8u track,Bit8u sector/*TODO: physical head?*/,unsigned int req_sector_size) {
    if ((size_t)track >= dents.size())
        return NULL;

    std::vector<imageDiskD88::vfdentry>::iterator i = dents.begin();

    if (req_sector_size == 0)
        req_sector_size = sector_size;

    while (i != dents.end()) {
        const imageDiskD88::vfdentry &ent = *i;

        if (ent.head == head &&
            ent.track == track &&
            ent.sector == sector &&
            (ent.sector_size == req_sector_size || req_sector_size == ~0U))
            return &(*i);

        ++i;
    }

    return NULL;
}

Bit8u imageDiskD88::Write_Sector(Bit32u head,Bit32u cylinder,Bit32u sector,const void * data,unsigned int req_sector_size) {
    const vfdentry *ent;

    if (req_sector_size == 0)
        req_sector_size = sector_size;

//    LOG_MSG("D88 read sector: CHS %u/%u/%u sz=%u",cylinder,head,sector,req_sector_size);

    ent = findSector(head,cylinder,sector,req_sector_size);
    if (ent == NULL) return 0x05;
    if (ent->getSectorSize() != req_sector_size) return 0x05;

    fseek(diskimg,(long)ent->data_offset,SEEK_SET);
    if ((uint32_t)ftell(diskimg) != ent->data_offset) return 0x05;
    if (fwrite(data,req_sector_size,1,diskimg) != 1) return 0x05;
    return 0;
}

Bit8u imageDiskD88::Write_AbsoluteSector(Bit32u sectnum,const void *data) {
    unsigned int c,h,s;

    if (sectors == 0 || heads == 0)
        return 0x05;

    s = (sectnum % sectors) + 1;
    h = (sectnum / sectors) % heads;
    c = (sectnum / sectors / heads);
    return Write_Sector(h,c,s,data);
}

imageDiskD88::imageDiskD88(FILE *imgFile, Bit8u *imgName, Bit32u imgSizeK, bool isHardDisk) : imageDisk(ID_D88) {
    (void)isHardDisk;//UNUSED
    D88HEAD head;

    fd_type_major = DISKTYPE_2D;
    fd_type_minor = 0;

    assert(sizeof(D88HEAD) == 0x2B0);
    assert(sizeof(D88SEC) == 0x10);

    heads = 0;
    cylinders = 0;
    image_base = 0;
    sectors = 0;
    active = false;
    sector_size = 0;
    reserved_cylinders = 0;
    diskSizeK = imgSizeK;
    diskimg = imgFile;

    if (imgName != NULL)
        diskname = (const char*)imgName;

    // NOTES:
    // 
    //  +0x000: D88 header
    //  +0x020: Offset of D88 tracks, per track
    //  +0x2B0: <begin data>
    //
    // Track offsets are sequential, always
    //
    // Each track is an array of:
    //
    //  ENTRY:
    //   <D88 sector head>
    //   <sector contents>
    //
    // Array of ENTRY from offset until next track
    fseek(diskimg,0,SEEK_END);
    off_t fsz = ftell(diskimg);

    fseek(diskimg,0,SEEK_SET);
    if (fread(&head,sizeof(head),1,diskimg) != 1) return;

    // validate fd_size
    if ((uint32_t)host_readd((ConstHostPt)(&head.fd_size)) > (uint32_t)fsz) return;

    fd_type_major = head.fd_type >> 4U;
    fd_type_minor = head.fd_type & 0xFU;

    // validate that none of the track offsets extend past the file
    {
        for (unsigned int i=0;i < D88_TRACKMAX;i++) {
            uint32_t trackoff = host_readd((ConstHostPt)(&head.trackp[i]));

            if (trackoff == 0) continue;

            if ((trackoff + 16U) > (uint32_t)fsz) {
                LOG_MSG("D88: track starts past end of file");
                return;
            }
        }
    }

    // read each track
    for (unsigned int track=0;track < D88_TRACKMAX;track++) {
        uint32_t trackoff = host_readd((ConstHostPt)(&head.trackp[track]));

        if (trackoff != 0) {
            fseek(diskimg, (long)trackoff, SEEK_SET);
            if ((off_t)ftell(diskimg) != (off_t)trackoff) continue;

            D88SEC s;
            unsigned int count = 0;

            do {
                if (fread(&s,sizeof(s),1,diskimg) != 1) break;

                uint16_t sector_count = host_readw((ConstHostPt)(&s.sectors));
                uint16_t sector_size = host_readw((ConstHostPt)(&s.size));

                if (sector_count == 0U || sector_size < 128U) break;
                if (sector_count > 128U || sector_size > 16384U) break;
                if (s.n > 8U) s.n = 8U;

                vfdentry vent;
                vent.sector_size = 128 << s.n;
                vent.data_offset = (uint32_t)ftell(diskimg);
                vent.entry_offset = vent.data_offset - (uint32_t)16;
                vent.track = s.c;
                vent.head = s.h;
                vent.sector = s.r;

                LOG_MSG("D88: trackindex=%u C/H/S/sz=%u/%u/%u/%u data-at=0x%lx",
                    track,vent.track,vent.head,vent.sector,vent.sector_size,(unsigned long)vent.data_offset);

                dents.push_back(vent);
                if ((++count) >= sector_count) break;

                fseek(diskimg, (long)sector_size, SEEK_CUR);
            } while (1);
        }
    }

    if (!dents.empty()) {
        /* okay, now to figure out what the geometry of the disk is.
         * we cannot just work from an "absolute" disk image model
         * because there's no D88 header to just say what the geometry is.
         * Like the IBM PC BIOS, we have to look at the disk and figure out
         * which geometry to apply to it, even if the FDD format allows
         * sectors on other tracks to have wild out of range sector, track,
         * and head numbers or odd sized sectors.
         *
         * First, determine sector size according to the boot sector. */
        bool founddisk = false;
        const vfdentry *ent;

        ent = findSector(/*head*/0,/*track*/0,/*sector*/1,~0U);
        if (ent != NULL) {
            if (ent->getSectorSize() <= 1024) /* x <= 1024 */
                sector_size = ent->getSectorSize();
        }

        /* oh yeah right, sure.
         * I suppose you're one of those FDD images where the sector size is 128 bytes/sector
         * in the boot sector and the rest is 256 bytes/sector elsewhere. I have no idea why
         * but quite a few FDD images have this arrangement. */
        if (sector_size != 0 && sector_size < 512) {
            ent = findSector(/*head*/0,/*track*/1,/*sector*/1,~0U);
            if (ent != NULL) {
                if (ent->getSectorSize() <= 1024) { /* x <= 1024 */
                    unsigned int nsz = ent->getSectorSize();
                    if (sector_size != nsz)
                        LOG_MSG("D88 warning: sector size changes between track 0 and 1");
                    if (sector_size < nsz)
                        sector_size = nsz;
                }
            }
        }

        if (sector_size != 0) {
            unsigned int i = 0;
            while (DiskGeometryList[i].ksize != 0) {
                const diskGeo &diskent = DiskGeometryList[i];

                if (diskent.bytespersect == sector_size) {
                    ent = findSector(0,0,diskent.secttrack);
                    if (ent != NULL) {
                        LOG_MSG("D88 disk probe: %u/%u/%u exists",0,0,diskent.secttrack);
                        if (sectors < diskent.secttrack)
                            sectors = diskent.secttrack;
                    }
                }

                i++;
            }
        }

        if (sector_size != 0 && sectors != 0) {
            unsigned int i = 0;
            while (DiskGeometryList[i].ksize != 0) {
                const diskGeo &diskent = DiskGeometryList[i];

                if (diskent.bytespersect == sector_size && diskent.secttrack >= sectors) {
                    ent = findSector(0,diskent.cylcount-1,sectors);
                    if (ent != NULL) {
                        LOG_MSG("D88 disk probe: %u/%u/%u exists",0,diskent.cylcount-1,sectors);
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
                LOG_MSG("D88 disk probe: %u/%u/%u exists",1,0,sectors);
                heads = 2;
            }
        }

        // TODO: drive_fat.cpp should use an extension to this API to allow changing the sectors/track
        //       according to what it reads from the MS-DOS BIOS parameter block, just like real MS-DOS.
        //       This would allow better representation of strange disk formats such as the "extended"
        //       floppy format that Microsoft used to use for Word 95 and Windows 95 install floppies.

        LOG_MSG("D88 geometry detection: C/H/S %u/%u/%u %u bytes/sector",
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

imageDiskD88::~imageDiskD88() {
    if(diskimg != NULL) {
        fclose(diskimg);
        diskimg=NULL; 
    }
}

/*--------------------------------*/

Bit8u imageDiskNFD::Read_Sector(Bit32u head,Bit32u cylinder,Bit32u sector,void * data,unsigned int req_sector_size) {
    const vfdentry *ent;

    if (req_sector_size == 0)
        req_sector_size = sector_size;

//    LOG_MSG("NFD read sector: CHS %u/%u/%u sz=%u",cylinder,head,sector,req_sector_size);

    ent = findSector(head,cylinder,sector,req_sector_size);
    if (ent == NULL) return 0x05;
    if (ent->getSectorSize() != req_sector_size) return 0x05;

    fseek(diskimg,(long)ent->data_offset,SEEK_SET);
    if ((uint32_t)ftell(diskimg) != ent->data_offset) return 0x05;
    if (fread(data,req_sector_size,1,diskimg) != 1) return 0x05;
    return 0;
}

Bit8u imageDiskNFD::Read_AbsoluteSector(Bit32u sectnum, void * data) {
    unsigned int c,h,s;

    if (sectors == 0 || heads == 0)
        return 0x05;

    s = (sectnum % sectors) + 1;
    h = (sectnum / sectors) % heads;
    c = (sectnum / sectors / heads);
    return Read_Sector(h,c,s,data);
}

imageDiskNFD::vfdentry *imageDiskNFD::findSector(Bit8u head,Bit8u track,Bit8u sector/*TODO: physical head?*/,unsigned int req_sector_size) {
    if ((size_t)track >= dents.size())
        return NULL;

    std::vector<imageDiskNFD::vfdentry>::iterator i = dents.begin();

    if (req_sector_size == 0)
        req_sector_size = sector_size;

    while (i != dents.end()) {
        const imageDiskNFD::vfdentry &ent = *i;

        if (ent.head == head &&
            ent.track == track &&
            ent.sector == sector &&
            (ent.sector_size == req_sector_size || req_sector_size == ~0U))
            return &(*i);

        ++i;
    }

    return NULL;
}

Bit8u imageDiskNFD::Write_Sector(Bit32u head,Bit32u cylinder,Bit32u sector,const void * data,unsigned int req_sector_size) {
    const vfdentry *ent;

    if (req_sector_size == 0)
        req_sector_size = sector_size;

//    LOG_MSG("NFD read sector: CHS %u/%u/%u sz=%u",cylinder,head,sector,req_sector_size);

    ent = findSector(head,cylinder,sector,req_sector_size);
    if (ent == NULL) return 0x05;
    if (ent->getSectorSize() != req_sector_size) return 0x05;

    fseek(diskimg,(long)ent->data_offset,SEEK_SET);
    if ((uint32_t)ftell(diskimg) != ent->data_offset) return 0x05;
    if (fwrite(data,req_sector_size,1,diskimg) != 1) return 0x05;
    return 0;
}

Bit8u imageDiskNFD::Write_AbsoluteSector(Bit32u sectnum,const void *data) {
    unsigned int c,h,s;

    if (sectors == 0 || heads == 0)
        return 0x05;

    s = (sectnum % sectors) + 1;
    h = (sectnum / sectors) % heads;
    c = (sectnum / sectors / heads);
    return Write_Sector(h,c,s,data);
}

imageDiskNFD::imageDiskNFD(FILE *imgFile, Bit8u *imgName, Bit32u imgSizeK, bool isHardDisk, unsigned int revision) : imageDisk(ID_NFD) {
    (void)isHardDisk;//UNUSED
    union {
        NFDHDR head;
        NFDHDRR1 headr1;
    }; // these occupy the same location of memory

    assert(sizeof(NFDHDR) == 0x120);
    assert(sizeof(NFDHDRR1) == 0x3C0);
    assert(sizeof(NFDHDR_ENTRY) == 0x10);

    heads = 0;
    cylinders = 0;
    image_base = 0;
    sectors = 0;
    active = false;
    sector_size = 0;
    reserved_cylinders = 0;
    diskSizeK = imgSizeK;
    diskimg = imgFile;

    if (imgName != NULL)
        diskname = (const char*)imgName;

    // NOTES:
    // 
    //  +0x000: NFD header
    //  +0x020: Offset of NFD tracks, per track
    //  +0x2B0: <begin data>
    //
    // Track offsets are sequential, always
    //
    // Each track is an array of:
    //
    //  ENTRY:
    //   <NFD sector head>
    //   <sector contents>
    //
    // Array of ENTRY from offset until next track
    fseek(diskimg,0,SEEK_END);
    off_t fsz = ftell(diskimg);

    fseek(diskimg,0,SEEK_SET);
    if (revision == 0) {
        if (fread(&head,sizeof(head),1,diskimg) != 1) return;
    }
    else if (revision == 1) {
        if (fread(&headr1,sizeof(headr1),1,diskimg) != 1) return;
    }
    else {
        abort();
    }

    // validate fd_size
    if ((uint32_t)host_readd((ConstHostPt)(&head.headersize)) < sizeof(head)) return;
    if ((uint32_t)host_readd((ConstHostPt)(&head.headersize)) > (uint32_t)fsz) return;

    unsigned int data_offset = host_readd((ConstHostPt)(&head.headersize));

    std::vector< std::pair<uint32_t,NFDHDR_ENTRY> > seclist;

    if (revision == 0) {
        unsigned int secents = (host_readd((ConstHostPt)(&head.headersize)) - sizeof(head)) / sizeof(NFDHDR_ENTRY);
        if (secents == 0) return;
        secents--;
        if (secents == 0) return;

        for (unsigned int i=0;i < secents;i++) {
            uint32_t ofs = (uint32_t)ftell(diskimg);
            NFDHDR_ENTRY e;

            if (fread(&e,sizeof(e),1,diskimg) != 1) return;
            seclist.push_back( std::pair<uint32_t,NFDHDR_ENTRY>(ofs,e) );

            if (e.log_cyl == 0xFF || e.log_head == 0xFF || e.log_rec == 0xFF || e.sec_len_pow2 > 7)
                continue;

            LOG_MSG("NFD %u/%u: ofs=%lu data=%lu cyl=%u head=%u sec=%u len=%u",
                    (unsigned int)i,
                    (unsigned int)secents,
                    (unsigned long)ofs,
                    (unsigned long)data_offset,
                    e.log_cyl,
                    e.log_head,
                    e.log_rec,
                    128 << e.sec_len_pow2);

            vfdentry vent;
            vent.sector_size = 128 << e.sec_len_pow2;
            vent.data_offset = (uint32_t)data_offset;
            vent.entry_offset = (uint32_t)ofs;
            vent.track = e.log_cyl;
            vent.head = e.log_head;
            vent.sector = e.log_rec;
            dents.push_back(vent);

            data_offset += 128u << e.sec_len_pow2;
            if (data_offset > (unsigned int)fsz) return;
        }
    }
    else {
        /* R1 has an array of offsets to where each tracks begins.
         * The end of the track is an entry like 0x1A 0x00 0x00 0x00 0x00 0x00 0x00 .... */
        /* The R1 images I have as reference always have offsets in ascending order. */
        for (unsigned int ti=0;ti < 164;ti++) {
            uint32_t trkoff = host_readd((ConstHostPt)(&headr1.trackheads[ti]));

            if (trkoff == 0) break;

            fseek(diskimg,(long)trkoff,SEEK_SET);
            if ((off_t)ftell(diskimg) != (off_t)trkoff) return;

            NFDHDR_ENTRY e;

            // track id
            if (fread(&e,sizeof(e),1,diskimg) != 1) return;
            unsigned int sectors = host_readw((ConstHostPt)(&e) + 0);
            unsigned int diagcount = host_readw((ConstHostPt)(&e) + 2);

            LOG_MSG("NFD R1 track ent %u offset %lu sectors %u diag %u",ti,(unsigned long)trkoff,sectors,diagcount);

            for (unsigned int s=0;s < sectors;s++) {
                uint32_t ofs = (uint32_t)ftell(diskimg);

                if (fread(&e,sizeof(e),1,diskimg) != 1) return;

                LOG_MSG("NFD %u/%u: ofs=%lu data=%lu cyl=%u head=%u sec=%u len=%u rep=%u",
                        (unsigned int)s,
                        (unsigned int)sectors,
                        (unsigned long)ofs,
                        (unsigned long)data_offset,
                        e.log_cyl,
                        e.log_head,
                        e.log_rec,
                        128 << e.sec_len_pow2,
                        e.byRetry);

                vfdentry vent;
                vent.sector_size = 128 << e.sec_len_pow2;
                vent.data_offset = (uint32_t)data_offset;
                vent.entry_offset = (uint32_t)ofs;
                vent.track = e.log_cyl;
                vent.head = e.log_head;
                vent.sector = e.log_rec;
                dents.push_back(vent);

                data_offset += 128u << e.sec_len_pow2;
                if (data_offset > (unsigned int)fsz) return;
            }

            for (unsigned int d=0;d < diagcount;d++) {
                if (fread(&e,sizeof(e),1,diskimg) != 1) return;

                unsigned int retry = e.byRetry;
                unsigned int len = host_readd((ConstHostPt)(&e) + 10);

                LOG_MSG("NFD diag %u/%u: retry=%u len=%u data=%lu",d,diagcount,retry,len,(unsigned long)data_offset);

                data_offset += (1+retry) * len;
            }
        }
    }

    if (!dents.empty()) {
        /* okay, now to figure out what the geometry of the disk is.
         * we cannot just work from an "absolute" disk image model
         * because there's no NFD header to just say what the geometry is.
         * Like the IBM PC BIOS, we have to look at the disk and figure out
         * which geometry to apply to it, even if the FDD format allows
         * sectors on other tracks to have wild out of range sector, track,
         * and head numbers or odd sized sectors.
         *
         * First, determine sector size according to the boot sector. */
        bool founddisk = false;
        const vfdentry *ent;

        ent = findSector(/*head*/0,/*track*/0,/*sector*/1,~0U);
        if (ent != NULL) {
            if (ent->getSectorSize() <= 1024) /* x <= 1024 */
                sector_size = ent->getSectorSize();
        }

        /* oh yeah right, sure.
         * I suppose you're one of those FDD images where the sector size is 128 bytes/sector
         * in the boot sector and the rest is 256 bytes/sector elsewhere. I have no idea why
         * but quite a few FDD images have this arrangement. */
        if (sector_size != 0 && sector_size < 512) {
            ent = findSector(/*head*/0,/*track*/1,/*sector*/1,~0U);
            if (ent != NULL) {
                if (ent->getSectorSize() <= 1024) { /* x <= 1024 */
                    unsigned int nsz = ent->getSectorSize();
                    if (sector_size != nsz)
                        LOG_MSG("NFD warning: sector size changes between track 0 and 1");
                    if (sector_size < nsz)
                        sector_size = nsz;
                }
            }
        }

        if (sector_size != 0) {
            unsigned int i = 0;
            while (DiskGeometryList[i].ksize != 0) {
                const diskGeo &diskent = DiskGeometryList[i];

                if (diskent.bytespersect == sector_size) {
                    ent = findSector(0,0,diskent.secttrack);
                    if (ent != NULL) {
                        LOG_MSG("NFD disk probe: %u/%u/%u exists",0,0,diskent.secttrack);
                        if (sectors < diskent.secttrack)
                            sectors = diskent.secttrack;
                    }
                }

                i++;
            }
        }

        if (sector_size != 0 && sectors != 0) {
            unsigned int i = 0;
            while (DiskGeometryList[i].ksize != 0) {
                const diskGeo &diskent = DiskGeometryList[i];

                if (diskent.bytespersect == sector_size && diskent.secttrack >= sectors) {
                    ent = findSector(0,diskent.cylcount-1,sectors);
                    if (ent != NULL) {
                        LOG_MSG("NFD disk probe: %u/%u/%u exists",0,diskent.cylcount-1,sectors);
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
                LOG_MSG("NFD disk probe: %u/%u/%u exists",1,0,sectors);
                heads = 2;
            }
        }

        // TODO: drive_fat.cpp should use an extension to this API to allow changing the sectors/track
        //       according to what it reads from the MS-DOS BIOS parameter block, just like real MS-DOS.
        //       This would allow better representation of strange disk formats such as the "extended"
        //       floppy format that Microsoft used to use for Word 95 and Windows 95 install floppies.

        LOG_MSG("NFD geometry detection: C/H/S %u/%u/%u %u bytes/sector",
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

imageDiskNFD::~imageDiskNFD() {
    if(diskimg != NULL) {
        fclose(diskimg);
        diskimg=NULL; 
    }
}

