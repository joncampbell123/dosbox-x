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
#include <string.h>
#include <sys/types.h>
#include <vector>

#include "dosbox.h"
#include "mem.h"
#include "bios_disk.h"
#include "imagedisk_d88.h"

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

Int13Status imageDiskD88::Read_Sector(uint32_t head,uint32_t cylinder,uint32_t sector,void * data,unsigned int req_sector_size) {
    const vfdentry *ent;

    if (req_sector_size == 0)
        req_sector_size = sector_size;

//    LOG_MSG("D88 read sector: CHS %u/%u/%u sz=%u",cylinder,head,sector,req_sector_size);

    ent = findSector(head,cylinder,sector,req_sector_size);
    if (ent == NULL) return Int13Status::SectorNotFound;
    if (ent->getSectorSize() != req_sector_size) return Int13Status::SectorNotFound;

    fseek(diskimg,(long)ent->data_offset,SEEK_SET);
    if ((uint32_t)ftell(diskimg) != ent->data_offset) return Int13Status::SeekFailed;
    if (fread(data,req_sector_size,1,diskimg) != 1) return Int13Status::ControllerFailure;
    return Int13Status::NoError;
}

Int13Status imageDiskD88::Read_AbsoluteSector(uint32_t sectnum, void * data) {
    unsigned int c,h,s;

    if (sectors == 0 || heads == 0)
        return Int13Status::SectorNotFound;

    s = (sectnum % sectors) + 1;
    h = (sectnum / sectors) % heads;
    c = (sectnum / sectors / heads);
    return Read_Sector(h,c,s,data);
}

imageDiskD88::vfdentry *imageDiskD88::findSector(uint8_t head,uint8_t track,uint8_t sector/*TODO: physical head?*/,unsigned int req_sector_size) {
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

Int13Status imageDiskD88::Write_Sector(uint32_t head,uint32_t cylinder,uint32_t sector,const void * data,unsigned int req_sector_size) {
    const vfdentry *ent;

    if (req_sector_size == 0)
        req_sector_size = sector_size;

//    LOG_MSG("D88 read sector: CHS %u/%u/%u sz=%u",cylinder,head,sector,req_sector_size);

    ent = findSector(head,cylinder,sector,req_sector_size);
    if (ent == NULL) return Int13Status::SectorNotFound;
    if (ent->getSectorSize() != req_sector_size) return Int13Status::SectorNotFound;

    fseek(diskimg,(long)ent->data_offset,SEEK_SET);
    if ((uint32_t)ftell(diskimg) != ent->data_offset) return Int13Status::SeekFailed;
    if (fwrite(data,req_sector_size,1,diskimg) != 1) return Int13Status::ControllerFailure;
    return Int13Status::NoError;
}

Int13Status imageDiskD88::Write_AbsoluteSector(uint32_t sectnum,const void *data) {
    unsigned int c,h,s;

    if (sectors == 0 || heads == 0)
        return Int13Status::SectorNotFound;

    s = (sectnum % sectors) + 1;
    h = (sectnum / sectors) % heads;
    c = (sectnum / sectors / heads);
    return Write_Sector(h,c,s,data);
}

imageDiskD88::imageDiskD88(FILE *imgFile, const char *imgName, uint32_t imgSizeK, bool isHardDisk) : imageDisk(ID_D88) {
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
        diskname = imgName;

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
    if (host_readd((ConstHostPt)(&head.fd_size)) > (uint32_t)fsz) return;

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
            updateFloppyDPT();
        }
    }
}

imageDiskD88::~imageDiskD88() {
    if(diskimg != NULL) {
        fclose(diskimg);
        diskimg=NULL;
    }
}
