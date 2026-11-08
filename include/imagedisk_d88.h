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

#ifndef DOSBOX_IMAGE_DISK_D88_H
#define DOSBOX_IMAGE_DISK_D88_H

#include "bios_disk.h"

class imageDiskD88 : public imageDisk {
public:
	Int13Status Read_Sector(uint32_t head,uint32_t cylinder,uint32_t sector,void * data,unsigned int req_sector_size=0) override;
	Int13Status Write_Sector(uint32_t head,uint32_t cylinder,uint32_t sector,const void * data,unsigned int req_sector_size=0) override;
	Int13Status Read_AbsoluteSector(uint32_t sectnum, void * data) override;
	Int13Status Write_AbsoluteSector(uint32_t sectnum, const void * data) override;
	bool Get_Track(uint32_t cylinder,uint32_t head,std::vector<imageDiskTrackSector> &sectors) override;
	Int13Status Read_Track_Sector(uint32_t cylinder,uint32_t head,unsigned int index,void *data,unsigned int size) override;
	Int13Status Write_Track_Sector(uint32_t cylinder,uint32_t head,unsigned int index,const void *data,unsigned int size) override;
	bool Is_Write_Protected(void) override { return write_protected; }

	imageDiskD88(FILE *imgFile, const char *imgName, uint32_t imgSizeK, bool isHardDisk);
	virtual ~imageDiskD88();

    unsigned char fd_type_major;
    unsigned char fd_type_minor;
    bool write_protected = false;

    enum { /* major */
        DISKTYPE_2D			= 0,
        DISKTYPE_2DD,
        DISKTYPE_2HD
    };

    struct vfdentry {
        uint8_t         track = 0,head = 0,sector = 0;
        uint16_t        sector_size = 0;

        uint32_t        data_offset = 0;
        uint32_t        entry_offset = 0; // offset of the 12-byte entry this came from (if nonzero)

        uint8_t         trackindex = 0;     // physical track (cylinder * 2 + head) the sector is on
        uint8_t         n = 0;              // sector size code in the ID field
        uint16_t        data_size = 0;      // bytes of data recorded in the image
        bool            fm = false;         // single density
        bool            deleted = false;    // deleted data address mark
        uint8_t         status = 0;         // status as recorded (PC-98 disk BIOS result code)

        vfdentry() {}

        uint16_t getSectorSize(void) const {
            return sector_size;
        }
    };

    vfdentry *findSector(uint8_t head,uint8_t track,uint8_t sector/*TODO: physical head?*/,unsigned int req_sector_size=0);
    vfdentry *trackSector(uint32_t cylinder,uint32_t head,unsigned int index);

    std::vector<vfdentry> dents;
};

#endif
