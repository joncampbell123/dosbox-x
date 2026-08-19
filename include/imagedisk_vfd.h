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

#ifndef DOSBOX_IMAGE_DISK_VFD_H
#define DOSBOX_IMAGE_DISK_VFD_H

#include "bios_disk.h"

class imageDiskVFD : public imageDisk {
public:
	Int13Status Read_Sector(uint32_t head,uint32_t cylinder,uint32_t sector,void * data,unsigned int req_sector_size=0) override;
	Int13Status Write_Sector(uint32_t head,uint32_t cylinder,uint32_t sector,const void * data,unsigned int req_sector_size=0) override;
	Int13Status Read_AbsoluteSector(uint32_t sectnum, void * data) override;
	Int13Status Write_AbsoluteSector(uint32_t sectnum, const void * data) override;

	imageDiskVFD(FILE *imgFile, const char *imgName, uint32_t imgSizeK, bool isHardDisk);
	virtual ~imageDiskVFD();

    struct vfdentry {
        uint8_t         track = 0,head = 0,sector = 0,sizebyte = 0;
        uint8_t         fillbyte = 0xFF;

        uint32_t        data_offset = 0;
        uint32_t        entry_offset = 0; // offset of the 12-byte entry this came from (if nonzero)

        vfdentry() {}

        bool hasSectorData(void) const {
            return fillbyte == 0xFF && data_offset != 0xFFFFFFFFUL;
        }

        bool hasFill(void) const {
            return fillbyte != 0xFF || data_offset == 0xFFFFFFFFUL;
        }

        uint16_t getSectorSize(void) const {
            return 128 << sizebyte;
        }
    };

    vfdentry *findSector(uint8_t head,uint8_t track,uint8_t sector/*TODO: physical head?*/,unsigned int req_sector_size=0);

    std::vector<vfdentry> dents;
};

#endif
