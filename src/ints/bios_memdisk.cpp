/*
 *
 *  Copyright (c) 2015 Shane Krueger
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

imageDiskMemory::imageDiskMemory(Bit32u imgSizeK) {
	// always use sector size of 512, with 255 heads and 63 sectors, which equates to about 8MB increments
	// round up to the next "8MB" increment and determine the number of cylinders necessary
	// ideally the number of cylinders should be <= 1024 for full compatibility with DOS (the "8GB limit")
	Bit32u sector_size = 512; //bytes per sector
	Bit32u sectors = 63; //sectors per track
	Bit32u heads = 255; //tracks per cylinder
	Bit64u imgSizeBytes = (Bit64u)imgSizeK * 1024;
	Bit64u divBy = sector_size * sectors * heads;
	Bit32u cylinders = (Bit32u)((imgSizeBytes + divBy - 1) / divBy); //cylinders
	init(cylinders, heads, sectors, sector_size);
}
imageDiskMemory::imageDiskMemory(diskGeo floppyGeometry) {
	init(floppyGeometry.cylcount, floppyGeometry.headscyl, floppyGeometry.secttrack, floppyGeometry.bytespersect);
	this->hardDrive = false;
	this->bios_type = floppyGeometry.biosval;
}
Bit8u imageDiskMemory::GetBiosType(void) {
	return bios_type;
}
imageDiskMemory::imageDiskMemory(Bit32u cylinders, Bit32u heads, Bit32u sectors, Bit32u sector_size) {
	init(cylinders, heads, sectors, sector_size);
}
void imageDiskMemory::init(Bit32u cylinders, Bit32u heads, Bit32u sectors, Bit32u sector_size) {
	this->active = false;
	this->cylinders = 0;
	this->heads = 0;
	this->sectors = 0;
	this->sector_size = 0;
	this->total_sectors = 0;

	Bit64u absoluteSectors = (Bit64u)cylinders * (Bit64u)heads * (Bit64u)sectors;
	if (absoluteSectors > 0x100000000u) {
		LOG_MSG("Image size too large in imageDiskMemory constructor.\n");
		return;
	}

	Bit64u diskSizeK = ((Bit64u)heads * (Bit64u)cylinders * (Bit64u)sectors * (Bit64u)sector_size + (Bit64u)1023) / (Bit64u)1024;
	if (diskSizeK >= 0x100000000u)
	{
		LOG_MSG("Image size too large in imageDiskMemory constructor.\n");
		return;
	}

	this->sectors_per_ramcluster = heads;
	this->total_ramclusters = (absoluteSectors + sectors_per_ramcluster - 1) / sectors_per_ramcluster;
	this->ramcluster_size = sectors_per_ramcluster * sector_size;
	//allocate a map for storage in 'clusters', allocated upon read/write
	MemMap = (Bit8u**)malloc(total_ramclusters * sizeof(Bit8u*));
	if (MemMap == NULL) {
		LOG_MSG("Error allocating memory map in imageDiskMemory constructor for %lu clusters.\n", (unsigned long)total_ramclusters);
		return;
	}
	//clear memory map
	memset((void*)MemMap, 0, total_ramclusters * sizeof(Bit8u*));

	this->heads = heads;
	this->cylinders = cylinders;
	this->sectors = sectors;
	this->sector_size = sector_size;
	this->diskSizeK = diskSizeK;
	this->total_sectors = absoluteSectors;
	this->reserved_cylinders = 0;
	this->hardDrive = true;
	this->bios_type = 0xF8;
	this->active = true;
}
imageDiskMemory::~imageDiskMemory() {
	Bit8u* cluster;
	for (int i = 0; i < total_ramclusters; i++) {
		cluster = MemMap[i];
		if (cluster) free(cluster);
	}
	free(MemMap);
	MemMap = 0;
	total_sectors = 0;
	active = false;
}
Bit8u imageDiskMemory::Read_AbsoluteSector(Bit32u sectnum, void * data) {
	if (sectnum >= total_sectors) {
		LOG_MSG("Invalid sector number in Read_AbsoluteSector for sector %lu.\n", (unsigned long)sectnum);
		return 0x05;
	}

	Bit32u ramclusternum, ramclustersect;
	ramclusternum = sectnum / sectors_per_ramcluster;
	ramclustersect = sectnum % sectors_per_ramcluster;

	Bit8u* datalocation;
	datalocation = MemMap[ramclusternum];

	//if the location to be read has not yet been allocated, return zeros
	if (datalocation == 0) {
		memset(data, 0, sector_size);
		return 0x00;
	}

	datalocation = &datalocation[ramclustersect * sector_size];

	memcpy(data, datalocation, sector_size);
	return 0x00;
}

Bit8u imageDiskMemory::Write_AbsoluteSector(Bit32u sectnum, void * data) {
	if (sectnum >= total_sectors) {
		LOG_MSG("Invalid sector number in Write_AbsoluteSector for sector %lu.\n", (unsigned long)sectnum);
		return 0x05;
	}

	Bit32u clusternum, clustersect;
	clusternum = sectnum / sectors_per_ramcluster;
	clustersect = sectnum % sectors_per_ramcluster;

	Bit8u* datalocation;
	datalocation = MemMap[clusternum];

	if (datalocation == NULL) {
		//for unallocated memory, first check if we are actually saving anything, or if it's just zeros
		Bit8u anyData = 0;
		for (int i = 0; i < sector_size; i++) {
			anyData |= ((Bit8u*)data)[i];
		}
		if (anyData == 0) return 0x00;

		datalocation = (Bit8u*)malloc(ramcluster_size);
		if (datalocation == NULL) {
			LOG_MSG("Could not allocate memory in Write_AbsoluteSector for sector %lu.\n", (unsigned long)sectnum);
			return 0x05;
		}
		MemMap[clusternum] = datalocation;
		memset((void*)datalocation, 0, ramcluster_size);
	}

	datalocation = &datalocation[clustersect * sector_size];

	memcpy(datalocation, data, sector_size);
	return 0x00;
}
const Bit8u freedos_mbr[] = {
	0x33,0xC0,0x8E,0xC0,0x8E,0xD8,0x8E,0xD0,0xBC,0x00,0x7C,0xFC,0x8B,0xF4,0xBF,0x00,
	0x06,0xB9,0x00,0x01,0xF2,0xA5,0xEA,0x67,0x06,0x00,0x00,0x8B,0xD5,0x58,0xA2,0x4F, // 10h
	0x07,0x3C,0x35,0x74,0x23,0xB4,0x10,0xF6,0xE4,0x05,0xAE,0x04,0x8B,0xF0,0x80,0x7C, // 20h
	0x04,0x00,0x74,0x44,0x80,0x7C,0x04,0x05,0x74,0x3E,0xC6,0x04,0x80,0xE8,0xDA,0x00,
	0x8A,0x74,0x01,0x8B,0x4C,0x02,0xEB,0x08,0xE8,0xCF,0x00,0xB9,0x01,0x00,0x32,0xD1, // 40h
	0xBB,0x00,0x7C,0xB8,0x01,0x02,0xCD,0x13,0x72,0x1E,0x81,0xBF,0xFE,0x01,0x55,0xAA,
	0x75,0x16,0xEA,0x00,0x7C,0x00,0x00,0x80,0xFA,0x81,0x74,0x02,0xB2,0x80,0x8B,0xEA,
	0x42,0x80,0xF2,0xB3,0x88,0x16,0x41,0x07,0xBF,0xBE,0x07,0xB9,0x04,0x00,0xC6,0x06,
	0x34,0x07,0x31,0x32,0xF6,0x88,0x2D,0x8A,0x45,0x04,0x3C,0x00,0x74,0x23,0x3C,0x05, // 80h
	0x74,0x1F,0xFE,0xC6,0xBE,0x31,0x07,0xE8,0x71,0x00,0xBE,0x4F,0x07,0x46,0x46,0x8B,
	0x1C,0x0A,0xFF,0x74,0x05,0x32,0x7D,0x04,0x75,0xF3,0x8D,0xB7,0x7B,0x07,0xE8,0x5A,
	0x00,0x83,0xC7,0x10,0xFE,0x06,0x34,0x07,0xE2,0xCB,0x80,0x3E,0x75,0x04,0x02,0x74,
	0x0B,0xBE,0x42,0x07,0x0A,0xF6,0x75,0x0A,0xCD,0x18,0xEB,0xAC,0xBE,0x31,0x07,0xE8,
	0x39,0x00,0xE8,0x36,0x00,0x32,0xE4,0xCD,0x1A,0x8B,0xDA,0x83,0xC3,0x60,0xB4,0x01,
	0xCD,0x16,0xB4,0x00,0x75,0x0B,0xCD,0x1A,0x3B,0xD3,0x72,0xF2,0xA0,0x4F,0x07,0xEB,
	0x0A,0xCD,0x16,0x8A,0xC4,0x3C,0x1C,0x74,0xF3,0x04,0xF6,0x3C,0x31,0x72,0xD6,0x3C,
	0x35,0x77,0xD2,0x50,0xBE,0x2F,0x07,0xBB,0x1B,0x06,0x53,0xFC,0xAC,0x50,0x24,0x7F, //100h
	0xB4,0x0E,0xCD,0x10,0x58,0xA8,0x80,0x74,0xF2,0xC3,0x56,0xB8,0x01,0x03,0xBB,0x00, //110h
	0x06,0xB9,0x01,0x00,0x32,0xF6,0xCD,0x13,0x5E,0xC6,0x06,0x4F,0x07,0x3F,0xC3,0x0D, //120h
	0x8A,0x0D,0x0A,0x46,0x35,0x20,0x2E,0x20,0x2E,0x20,0x2E,0xA0,0x64,0x69,0x73,0x6B,
	0x20,0x32,0x0D,0x0A,0x0A,0x44,0x65,0x66,0x61,0x75,0x6C,0x74,0x3A,0x20,0x46,0x31, //140h
	0xA0,0x00,0x01,0x00,0x04,0x00,0x06,0x03,0x07,0x07,0x0A,0x0A,0x63,0x0E,0x64,0x0E,
	0x65,0x14,0x80,0x19,0x81,0x19,0x82,0x19,0x83,0x1E,0x93,0x24,0xA5,0x2B,0x9F,0x2F,
	0x75,0x33,0x52,0x33,0xDB,0x36,0x40,0x3B,0xF2,0x41,0x00,0x44,0x6F,0xF3,0x48,0x70,
	0x66,0xF3,0x4F,0x73,0xB2,0x55,0x6E,0x69,0xF8,0x4E,0x6F,0x76,0x65,0x6C,0xEC,0x4D, //180h
	0x69,0x6E,0x69,0xF8,0x4C,0x69,0x6E,0x75,0xF8,0x41,0x6D,0x6F,0x65,0x62,0xE1,0x46,
	0x72,0x65,0x65,0x42,0x53,0xC4,0x42,0x53,0x44,0xE9,0x50,0x63,0x69,0xF8,0x43,0x70,
	0xED,0x56,0x65,0x6E,0x69,0xF8,0x44,0x6F,0x73,0x73,0x65,0xE3,0x3F,0xBF,0x00,0x00, //1B0h
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x55,0xAA
};
Bit8u imageDiskMemory::Format() {
	if (this->sector_size != 512) {
		LOG_MSG("imageDiskMemory->Format only designed for disks with 512-byte sectors.\n");
		return 0x01;
	}
	if (this->sectors > 255) {
		LOG_MSG("imageDiskMemory->Format only designed for disks with <= 255 sectors.\n");
		return 0x02;
	}
	if (this->cylinders >= 1024) {
		LOG_MSG("imageDiskMemory->Format only designed for disks with < 1024 cylinders.\n");
		return 0x03;
	}
	if (!this->hardDrive && this->total_sectors >= 65535) {
		LOG_MSG("imageDiskMemory->Format only designed for floppies with < 65535 total sectors.\n");
		return 0x04;
	}
	Bit8u sbuf[512];
	int bootsect_pos = 0;
	if (this->hardDrive) {
		// is a harddisk: write MBR
		memcpy(sbuf, freedos_mbr, 512);
		// active partition
		sbuf[0x1be] = 0x80;
		// start head - head 0 has the partition table, head 1 first partition
		sbuf[0x1bf] = 1;
		// start sector with bits 8-9 of start cylinder in bits 6-7
		sbuf[0x1c0] = 1;
		// start cylinder bits 0-7
		sbuf[0x1c1] = 0;
		// OS indicator: DOS what else ;)
		sbuf[0x1c2] = 0x06;
		// end head (0-based)
		sbuf[0x1c3] = this->heads - 1;
		// end sector with bits 8-9 of end cylinder (0-based) in bits 6-7
		sbuf[0x1c4] = this->sectors | (((this->cylinders - 1) & 0x300) >> 2);
		// end cylinder (0-based) bits 0-7
		sbuf[0x1c5] = (this->cylinders - 1) & 0xFF;
		// sectors preceding partition1 (one head)
		host_writed(&sbuf[0x1c6], this->sectors);
		// length of partition1, align to chs value
		host_writed(&sbuf[0x1ca], ((this->cylinders - 1)*this->heads + (this->heads - 1))*this->sectors);

		// write partition table
		this->Write_AbsoluteSector(0, sbuf);
		bootsect_pos = this->sectors;
	}

	// set boot sector values
	memset(sbuf, 0, 512);
	// TODO boot code jump
	sbuf[0] = 0xEB; sbuf[1] = 0x3c; sbuf[2] = 0x90;
	// OEM
	sprintf((char*)&sbuf[0x03], "MSDOS5.0");
	// bytes per sector: always 512
	host_writew(&sbuf[0x0b], 512);
	// sectors per cluster: 1,2,4,8,16,...
	sbuf[0x0d] = this->sectors;
	/*
	if (this->hardDrive) {
		Bitu cval = 1;
		while ((sectors / cval) >= 65525) cval <<= 1;
		sbuf[0x0d] = cval;
	}
	else sbuf[0x0d] = sectors / 0x1000 + 1; // FAT12 can hold 0x1000 entries TODO
											// TODO small floppys have 2 sectors per cluster?
											// reserverd sectors: 1 ( the boot sector)
	*/
	host_writew(&sbuf[0x0e], 1);
	// Number of FATs - always 2
	sbuf[0x10] = 2;
	// Root entries - how are these made up? - TODO
	Bitu root_ent = 512;
	if (!this->hardDrive)
	{
		switch (this->GetBiosType()) {
		case 0xF0: root_ent = this->sectors == 36 ? 512 : 224; break;
		case 0xF9: root_ent = this->sectors == 15 ? 224 : 112; break;
		case 0xFC: root_ent = 56; break;
		case 0xFD: root_ent = 112; break;
		case 0xFE: root_ent = 56; break;
		case 0xFF: root_ent = 112; break;
		}
	}
	host_writew(&sbuf[0x11], root_ent);
	// sectors (under 32MB) - will OSes be sore if all HD's use large size?
	if (!this->hardDrive) host_writew(&sbuf[0x13], this->total_sectors);
	// media descriptor
	sbuf[0x15] = this->hardDrive ? 0xF8 : this->GetBiosType();
	// sectors per FAT
	// needed entries: (sectors per cluster)
	Bitu sect_per_fat = 0;
	Bitu clusters = (this->total_sectors - 1) / sbuf[0x0d]; // TODO subtract root dir too maybe
	if (this->hardDrive) sect_per_fat = (clusters * 2) / 512 + 1;
	else sect_per_fat = ((clusters * 3) / 2) / 512 + 1;
	host_writew(&sbuf[0x16], sect_per_fat);
	// sectors per track
	host_writew(&sbuf[0x18], this->sectors);
	// heads
	host_writew(&sbuf[0x1a], this->heads);
	// hidden sectors
	host_writed(&sbuf[0x1c], bootsect_pos);
	// sectors (large disk) - this is the same as partition length in MBR
	if (this->hardDrive) host_writed(&sbuf[0x20], this->total_sectors - this->sectors);
	// BIOS drive
	if (this->hardDrive) sbuf[0x24] = 0x80;
	else sbuf[0x24] = 0x00;
	// ext. boot signature
	sbuf[0x26] = 0x29;
	// volume serial number
	// let's use the BIOS time (cheap, huh?)
	host_writed(&sbuf[0x27], mem_readd(BIOS_TIMER));
	// Volume label
	sprintf((char*)&sbuf[0x2b], "NO NAME    ");
	// file system type
	if (this->hardDrive) sprintf((char*)&sbuf[0x36], "FAT16   ");
	else sprintf((char*)&sbuf[0x36], "FAT12   ");
	// boot sector signature
	host_writew(&sbuf[0x1fe], 0xAA55);

	// write the boot sector
	this->Write_AbsoluteSector(bootsect_pos, sbuf);

	// write FATs
	// FATs default to zeros, which will be the default for ram-based disks anyway, so this code is redundant
	memset(sbuf, 0, 512);
	if (this->hardDrive) host_writed(&sbuf[0], 0xFFFFFFF8);
	else host_writed(&sbuf[0], 0xFFFFF0);
	// erase both FATs, and not just the first sector of each FAT
	for (int i = bootsect_pos + 1; i < bootsect_pos + 1 + sect_per_fat + sect_per_fat; i++) {
		this->Write_AbsoluteSector(bootsect_pos + 1, sbuf);
	}
	// 1st FAT
	//this->Write_AbsoluteSector(bootsect_pos + 1, sbuf);
	// 2nd FAT
	//this->Write_AbsoluteSector(bootsect_pos + 1 + sect_per_fat, sbuf);

	//success
	return 0x00;
}