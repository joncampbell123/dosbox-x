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

#ifndef DOSBOX_IMAGE_DISK_INT13_H
#define DOSBOX_IMAGE_DISK_INT13_H

#include "bios_disk.h"

#if !defined(OSFREE)
class imageDiskINT13Drive : public imageDisk {
public:
	Int13Status Read_Sector(uint32_t head,uint32_t cylinder,uint32_t sector,void * data,unsigned int req_sector_size=0) override;
	Int13Status Write_Sector(uint32_t head,uint32_t cylinder,uint32_t sector,const void * data,unsigned int req_sector_size=0) override;
	Int13Status Read_AbsoluteSector(uint32_t sectnum, void * data) override;
	Int13Status Write_AbsoluteSector(uint32_t sectnum, const void * data) override;

	void UpdateFloppyType(void) override;
	void Set_Reserved_Cylinders(Bitu resCyl) override;
	uint32_t Get_Reserved_Cylinders() override;
	void Set_Geometry(uint32_t setHeads, uint32_t setCyl, uint32_t setSect, uint32_t setSectSize) override;
	void Get_Geometry(uint32_t * getHeads, uint32_t *getCyl, uint32_t *getSect, uint32_t *getSectSize) override;
	uint8_t GetBiosType(void) override;
	uint32_t getSectSize(void) override;
	bool detectDiskChange(void) override;

	imageDiskINT13Drive(imageDisk *sdisk);
	virtual ~imageDiskINT13Drive();

	uint8_t bios_disk = 0;
	bool enable_int13 = false;
	imageDisk* subdisk = NULL;
	bool busy = false;
};
#endif

#endif
