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

#include "dosbox.h"
#include "bios_disk.h"
#include "imagedisk_emptydrive.h"

Int13Status imageDiskEmptyDrive::Read_Sector(uint32_t /*head*/,uint32_t /*cylinder*/,uint32_t /*sector*/,void * /*data*/,unsigned int /*req_sector_size*/) {
	return Int13Status::DriveNotReady;
}

Int13Status imageDiskEmptyDrive::Write_Sector(uint32_t /*head*/,uint32_t /*cylinder*/,uint32_t /*sector*/,const void * /*data*/,unsigned int /*req_sector_size*/) {
	return Int13Status::DriveNotReady;
}

Int13Status imageDiskEmptyDrive::Read_AbsoluteSector(uint32_t /*sectnum*/, void * /*data*/) {
	return Int13Status::DriveNotReady;
}

Int13Status imageDiskEmptyDrive::Write_AbsoluteSector(uint32_t /*sectnum*/, const void * /*data*/) {
	return Int13Status::DriveNotReady;
}

imageDiskEmptyDrive::imageDiskEmptyDrive() : imageDisk(ID_EMPTY_DRIVE) {
	active = true;
	sector_size = 512;
	heads = 2;
	cylinders = 80;
	sectors = 18;
	diskSizeK = 1440;
}

imageDiskEmptyDrive::~imageDiskEmptyDrive() {
}
