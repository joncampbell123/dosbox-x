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

#include <string.h>
#include <vector>

#include "dosbox.h"
#include "mem.h"
#include "bios_disk.h"
#include "imagedisk_vfd.h"

// VFD *.FDD floppy disk format support

Int13Status imageDiskVFD::Read_Sector(uint32_t head,uint32_t cylinder,uint32_t sector,void * data,unsigned int req_sector_size) {
    const vfdentry *ent;

    if (req_sector_size == 0)
        req_sector_size = sector_size;

//    LOG_MSG("VFD read sector: CHS %u/%u/%u sz=%u",cylinder,head,sector,req_sector_size);

    ent = findSector(head,cylinder,sector,req_sector_size);
    if (ent == NULL) return Int13Status::SectorNotFound;
    if (ent->getSectorSize() != req_sector_size) return Int13Status::SectorNotFound;

    if (ent->hasSectorData()) {
        fseek(diskimg,(long)ent->data_offset,SEEK_SET);
        if ((uint32_t)ftell(diskimg) != ent->data_offset) return Int13Status::SeekFailed;
        if (fread(data,req_sector_size,1,diskimg) != 1) return Int13Status::ControllerFailure;
        return Int13Status::NoError;
    }
    else if (ent->hasFill()) {
        memset(data,ent->fillbyte,req_sector_size);
        return Int13Status::NoError;
    }

    return Int13Status::SectorNotFound;
}

Int13Status imageDiskVFD::Read_AbsoluteSector(uint32_t sectnum, void * data) {
    unsigned int c,h,s;

    if (sectors == 0 || heads == 0)
        return Int13Status::SectorNotFound;

    s = (sectnum % sectors) + 1;
    h = (sectnum / sectors) % heads;
    c = (sectnum / sectors / heads);
    return Read_Sector(h,c,s,data);
}

imageDiskVFD::vfdentry *imageDiskVFD::findSector(uint8_t head,uint8_t track,uint8_t sector/*TODO: physical head?*/,unsigned int req_sector_size) {
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

Int13Status imageDiskVFD::Write_Sector(uint32_t head,uint32_t cylinder,uint32_t sector,const void * data,unsigned int req_sector_size) {
    unsigned long new_offset;
    unsigned char tmp[12];
    vfdentry *ent;

//    LOG_MSG("VFD write sector: CHS %u/%u/%u",cylinder,head,sector);

    if (req_sector_size == 0)
        req_sector_size = sector_size;

    ent = findSector(head,cylinder,sector,req_sector_size);
    if (ent == NULL) return Int13Status::SectorNotFound;
    if (ent->getSectorSize() != req_sector_size) return Int13Status::SectorNotFound;

    if (ent->hasSectorData()) {
        fseek(diskimg,(long)ent->data_offset,SEEK_SET);
        if ((uint32_t)ftell(diskimg) != ent->data_offset) return Int13Status::SeekFailed;
        if (fwrite(data,req_sector_size,1,diskimg) != 1) return Int13Status::ControllerFailure;
        return Int13Status::NoError;
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

        if (ent->entry_offset == 0) return Int13Status::SectorNotFound;

        if (isfill) {
            fseek(diskimg,(long)ent->entry_offset,SEEK_SET);
            if ((uint32_t)ftell(diskimg) != ent->entry_offset) return Int13Status::SeekFailed;
            if (fread(tmp,12,1,diskimg) != 1) return Int13Status::ControllerFailure;

            tmp[0x04] = ((unsigned char*)data)[0]; // change the fill byte

            LOG_MSG("VFD write: 'fill' sector changing fill byte to 0x%x",tmp[0x04]);

            fseek(diskimg,(long)ent->entry_offset,SEEK_SET);
            if ((uint32_t)ftell(diskimg) != ent->entry_offset) return Int13Status::SeekFailed;
            if (fwrite(tmp,12,1,diskimg) != 1) return Int13Status::ControllerFailure;
        }
        else {
            fseek(diskimg,0,SEEK_END);
            new_offset = (unsigned long)ftell(diskimg);

            /* we have to change it from a fill sector to an actual sector */
            LOG_MSG("VFD write: changing 'fill' sector to one with data (data at %lu)",(unsigned long)new_offset);

            fseek(diskimg,(long)ent->entry_offset,SEEK_SET);
            if ((uint32_t)ftell(diskimg) != ent->entry_offset) return Int13Status::SeekFailed;
            if (fread(tmp,12,1,diskimg) != 1) return Int13Status::ControllerFailure;

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
            if ((uint32_t)ftell(diskimg) != ent->entry_offset) return Int13Status::SeekFailed;
            if (fwrite(tmp,12,1,diskimg) != 1) return Int13Status::ControllerFailure;

            fseek(diskimg,(long)ent->data_offset,SEEK_SET);
            if ((uint32_t)ftell(diskimg) != ent->data_offset) return Int13Status::SeekFailed;
            if (fwrite(data,req_sector_size,1,diskimg) != 1) return Int13Status::ControllerFailure;
        }

        return Int13Status::NoError;
    }

    return Int13Status::SectorNotFound;
}

Int13Status imageDiskVFD::Write_AbsoluteSector(uint32_t sectnum,const void *data) {
    unsigned int c,h,s;

    if (sectors == 0 || heads == 0)
        return Int13Status::SectorNotFound;

    s = (sectnum % sectors) + 1;
    h = (sectnum / sectors) % heads;
    c = (sectnum / sectors / heads);
    return Write_Sector(h,c,s,data);
}

imageDiskVFD::imageDiskVFD(FILE *imgFile, const char *imgName, uint32_t imgSizeK, bool isHardDisk) : imageDisk(ID_VFD) {
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
        diskname = imgName;

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

            uint8_t i;
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
                updateFloppyDPT();
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
