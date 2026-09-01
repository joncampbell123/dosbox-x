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

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <utility>
#include <vector>

#include "dosbox.h"
#include "mem.h"
#include "bios_disk.h"
#include "imagedisk_nfd.h"

// NFD *.NFD floppy disk format support

#pragma pack(push,1)
typedef struct {
	char	sig[16];            // +0x000
	char	comment[0x100];     // +0x010
	uint8_t	headersize[4];      // +0x110
    uint8_t prot;               // +0x114
    uint8_t nhead;              // +0x115
    uint8_t _unknown_[10];      // +0x116
} NFDHDR;                       // =0x120

typedef struct {
	char	sig[16];            // +0x000
	char	comment[0x100];     // +0x010
	uint8_t	headersize[4];      // +0x110
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
#pragma pack(pop)

Int13Status imageDiskNFD::Read_Sector(uint32_t head,uint32_t cylinder,uint32_t sector,void * data,unsigned int req_sector_size) {
    const vfdentry *ent;

    if (req_sector_size == 0)
        req_sector_size = sector_size;

//    LOG_MSG("NFD read sector: CHS %u/%u/%u sz=%u",cylinder,head,sector,req_sector_size);

    ent = findSector(head,cylinder,sector,req_sector_size);
    if (ent == NULL) return Int13Status::SectorNotFound;
    if (ent->getSectorSize() != req_sector_size) return Int13Status::SectorNotFound;

    fseek(diskimg,(long)ent->data_offset,SEEK_SET);
    if ((uint32_t)ftell(diskimg) != ent->data_offset) return Int13Status::SeekFailed;
    if (fread(data,req_sector_size,1,diskimg) != 1) return Int13Status::ControllerFailure;
    return Int13Status::NoError;
}

Int13Status imageDiskNFD::Read_AbsoluteSector(uint32_t sectnum, void * data) {
    unsigned int c,h,s;

    if (sectors == 0 || heads == 0)
        return Int13Status::SectorNotFound;

    s = (sectnum % sectors) + 1;
    h = (sectnum / sectors) % heads;
    c = (sectnum / sectors / heads);
    return Read_Sector(h,c,s,data);
}

imageDiskNFD::vfdentry *imageDiskNFD::findSector(uint8_t head,uint8_t track,uint8_t sector/*TODO: physical head?*/,unsigned int req_sector_size) {
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

Int13Status imageDiskNFD::Write_Sector(uint32_t head,uint32_t cylinder,uint32_t sector,const void * data,unsigned int req_sector_size) {
    const vfdentry *ent;

    if (req_sector_size == 0)
        req_sector_size = sector_size;

//    LOG_MSG("NFD read sector: CHS %u/%u/%u sz=%u",cylinder,head,sector,req_sector_size);

    ent = findSector(head,cylinder,sector,req_sector_size);
    if (ent == NULL) return Int13Status::SectorNotFound;
    if (ent->getSectorSize() != req_sector_size) return Int13Status::SectorNotFound;

    fseek(diskimg,(long)ent->data_offset,SEEK_SET);
    if ((uint32_t)ftell(diskimg) != ent->data_offset) return Int13Status::SeekFailed;
    if (fwrite(data,req_sector_size,1,diskimg) != 1) return Int13Status::ControllerFailure;
    return Int13Status::NoError;
}

Int13Status imageDiskNFD::Write_AbsoluteSector(uint32_t sectnum,const void *data) {
    unsigned int c,h,s;

    if (sectors == 0 || heads == 0)
        return Int13Status::SectorNotFound;

    s = (sectnum % sectors) + 1;
    h = (sectnum / sectors) % heads;
    c = (sectnum / sectors / heads);
    return Write_Sector(h,c,s,data);
}

imageDiskNFD::imageDiskNFD(FILE *imgFile, const char *imgName, uint32_t imgSizeK, bool isHardDisk, unsigned int revision) : imageDisk(ID_NFD) {
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
        diskname = imgName;

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
    if (host_readd((ConstHostPt)(&head.headersize)) < sizeof(head)) return;
    if (host_readd((ConstHostPt)(&head.headersize)) > (uint32_t)fsz) return;

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
            vent.entry_offset = ofs;
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
                        s,
                        sectors,
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
                vent.entry_offset = ofs;
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
            updateFloppyDPT();
        }
    }
}

imageDiskNFD::~imageDiskNFD() {
    if(diskimg != NULL) {
        fclose(diskimg);
        diskimg=NULL;
    }
}
