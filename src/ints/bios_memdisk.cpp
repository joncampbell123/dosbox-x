/*
*
*  Copyright (c) 2018 Shane Krueger
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

/* imageDiskMemory simulates a hard drive image or floppy drive image in RAM
*
* It can be initialized as a floppy, using one of the predefined floppy
*   geometries, or can be initialized as a hard drive, accepting either
*   a size in kilobytes or a specific set of chs values to emulate
* It will then split the image into 64k chunks that are allocated on-demand.
* Initially the only RAM required is for the chunk map
* The image is effectively intialized to all zeros, as each chunk is zeroed
*   upon allocation
* Writes of all zeros do not allocate memory if none has yet been assigned
*
*/

// Create a hard drive image of a specified size; automatically select c/h/s
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

// Create a floppy image of a specified geometry
imageDiskMemory::imageDiskMemory(diskGeo floppyGeometry) {
	init(floppyGeometry.cylcount, floppyGeometry.headscyl, floppyGeometry.secttrack, floppyGeometry.bytespersect);
	this->hardDrive = false;
	this->bios_type = floppyGeometry.biosval;
}

// Return the BIOS type of the floppy image (or 0xF8 for hard drives)
Bit8u imageDiskMemory::GetBiosType(void) {
	return bios_type;
}

// Create a hard drive image of a specified geometry
imageDiskMemory::imageDiskMemory(Bit32u cylinders, Bit32u heads, Bit32u sectors, Bit32u sector_size) {
	init(cylinders, heads, sectors, sector_size);
}

// Internal initialization code to create a image of a specified geometry
void imageDiskMemory::init(Bit32u cylinders, Bit32u heads, Bit32u sectors, Bit32u sector_size) {
	//initialize internal variables in case we fail out
	this->active = false;
	this->cylinders = 0;
	this->heads = 0;
	this->sectors = 0;
	this->sector_size = 0;
	this->total_sectors = 0;

	//calculate the total number of sectors on the drive, and check for overflows
	Bit64u absoluteSectors = (Bit64u)cylinders * (Bit64u)heads;
	if (absoluteSectors > 0x100000000u) {
		LOG_MSG("Image size too large in imageDiskMemory constructor.\n");
		return;
	}
	absoluteSectors *= (Bit64u)sectors;
	if (absoluteSectors > 0x100000000u) {
		LOG_MSG("Image size too large in imageDiskMemory constructor.\n");
		return;
	}
	//make sure that the number of total sectors on the drive is not zero
	if (absoluteSectors == 0) {
		LOG_MSG("Image size too small in imageDiskMemory constructor.\n");
		return;
	}

	//calculate total size of the drive in kilobyts, and check for overflow
	Bit64u diskSizeK = ((Bit64u)heads * (Bit64u)cylinders * (Bit64u)sectors * (Bit64u)sector_size + (Bit64u)1023) / (Bit64u)1024;
	if (diskSizeK >= 0x100000000u)
	{
		LOG_MSG("Image size too large in imageDiskMemory constructor.\n");
		return;
	}

	//split the sectors into chunks, which are allocated together on an as-needed basis
	this->sectors_per_chunk = heads;
	this->total_chunks = (absoluteSectors + sectors_per_chunk - 1) / sectors_per_chunk;
	this->chunk_size = sectors_per_chunk * sector_size;
	//allocate a map of chunks that have been allocated and their memory addresses
	ChunkMap = (Bit8u**)malloc(total_chunks * sizeof(Bit8u*));
	if (ChunkMap == NULL) {
		LOG_MSG("Error allocating memory map in imageDiskMemory constructor for %lu clusters.\n", (unsigned long)total_chunks);
		return;
	}
	//clear memory map
	memset((void*)ChunkMap, 0, total_chunks * sizeof(Bit8u*));

	//set internal variables
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

// imageDiskMemory destructor will release all allocated memory
imageDiskMemory::~imageDiskMemory() {
	//quit if the map is already not allocated
	if (!active) return;
	//loop through each chunk and release it if it has been allocated
	Bit8u* chunk;
	for (int i = 0; i < total_chunks; i++) {
		chunk = ChunkMap[i];
		if (chunk) free(chunk);
	}
	//release the memory map
	free(ChunkMap);
	//reset internal variables
	ChunkMap = 0;
	total_sectors = 0;
	active = false;
}

// Read a specific sector from the ramdrive
Bit8u imageDiskMemory::Read_AbsoluteSector(Bit32u sectnum, void * data) {
	//sector number is a zero-based offset

	//verify the sector number is valid
	if (sectnum >= total_sectors) {
		LOG_MSG("Invalid sector number in Read_AbsoluteSector for sector %lu.\n", (unsigned long)sectnum);
		return 0x05;
	}

	//calculate which chunk the sector is located within, and which sector within the chunk
	Bit32u chunknum, chunksect;
	chunknum = sectnum / sectors_per_chunk;
	chunksect = sectnum % sectors_per_chunk;

	//retrieve the memory address of the chunk
	Bit8u* datalocation;
	datalocation = ChunkMap[chunknum];

	//if the chunk has not yet been allocated, return zeros
	if (datalocation == 0) {
		memset(data, 0, sector_size);
		return 0x00;
	}

	//update the address to the specific sector within the chunk
	datalocation = &datalocation[chunksect * sector_size];

	//copy the data to the output and return success
	memcpy(data, datalocation, sector_size);
	return 0x00;
}

// Write a specific sector from the ramdrive
Bit8u imageDiskMemory::Write_AbsoluteSector(Bit32u sectnum, void * data) {
	//sector number is a zero-based offset

	//verify the sector number is valid
	if (sectnum >= total_sectors) {
		LOG_MSG("Invalid sector number in Write_AbsoluteSector for sector %lu.\n", (unsigned long)sectnum);
		return 0x05;
	}

	//calculate which chunk the sector is located within, and which sector within the chunk
	Bit32u chunknum, chunksect;
	chunknum = sectnum / sectors_per_chunk;
	chunksect = sectnum % sectors_per_chunk;

	//retrieve the memory address of the chunk
	Bit8u* datalocation;
	datalocation = ChunkMap[chunknum];

	//if the chunk has not yet been allocated, allocate the chunk
	if (datalocation == NULL) {
		//first check if we are actually saving anything
		Bit8u anyData = 0;
		for (int i = 0; i < sector_size; i++) {
			anyData |= ((Bit8u*)data)[i];
		}
		//if it's all zeros, return success
		if (anyData == 0) return 0x00;

		//allocate a new memory chunk
		datalocation = (Bit8u*)malloc(chunk_size);
		if (datalocation == NULL) {
			LOG_MSG("Could not allocate memory in Write_AbsoluteSector for sector %lu.\n", (unsigned long)sectnum);
			return 0x05;
		}
		//save the memory chunk address within the memory map
		ChunkMap[chunknum] = datalocation;
		//initialize the memory chunk to all zeros (since we are only writing to a single sector within this chunk)
		memset((void*)datalocation, 0, chunk_size);
	}

	//update the address to the specific sector within the chunk
	datalocation = &datalocation[chunksect * sector_size];

	//write the sector to the chunk and return success
	memcpy(datalocation, data, sector_size);
	return 0x00;
}

// Parition and format the ramdrive (code is adapted from IMGMAKE code within dos_programs.cpp)
// - Assumes that the ramdrive is all zeros to begin with (since the root directory is not zeroed)
Bit8u imageDiskMemory::Format() {
	//verify that the geometry of the drive is valid
	if (this->sector_size != 512) {
		LOG_MSG("imageDiskMemory->Format only designed for disks with 512-byte sectors.\n");
		return 0x01;
	}
	if (this->sectors > 63) {
		LOG_MSG("imageDiskMemory->Format only designed for disks with <= 63 sectors.\n");
		return 0x02;
	}
	if (this->heads > 255) {
		LOG_MSG("imageDiskMemory->Format only designed for disks with <= 255 heads.\n");
		return 0x05;
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
	Bitu sectors_per_cluster = 1;
	// Microsoft defines FAT12 as having 0-4084 clusters,
	//   and FAT16 as having 4085-65524 clusters
	// To quote: "This is the one and only way that the FAT type is determined"
	Bitu max_clusters = this->hardDrive ? 65524 : 4084;
	//ESTIMATE the total number of clusters and check if the maximum cluster number (as stored in the FAT) is too large
	//note: this does not count on the fact that the number of available sectors are reduced based on the size of the FAT
	while (((total_sectors + sectors_per_cluster - 1) / sectors_per_cluster) > max_clusters) sectors_per_cluster <<= 1;
	if (sectors_per_cluster > 255) {
		if (this->hardDrive) {
			LOG_MSG("imageDiskMemory->Format only designed for hard drives with < 8,386,176 total sectors (FAT16 limitation).\n");
		}
		else {
			LOG_MSG("imageDiskMemory->Format only designed for hard drives with < 522,112 total sectors (FAT12 limitation).\n");
		}
		return 0x06;
	}
	sbuf[0x0d] = sectors_per_cluster;
	// reserved sectors: 1 (the boot sector)
	host_writew(&sbuf[0x0e], 1);
	// Number of FATs - always 2
	sbuf[0x10] = 2;
	// Root directory entries
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
	Bitu root_sectors = ((root_ent * 32 + sector_size - 1) / sector_size);
	host_writew(&sbuf[0x11], root_ent);
	// sectors (under 32MB) - will OSes be sore if all HD's use large size?
	if (!this->hardDrive) host_writew(&sbuf[0x13], this->total_sectors);
	// media descriptor
	sbuf[0x15] = this->hardDrive ? 0xF8 : this->GetBiosType();
	// sectors per FAT
	// fat_clusters = (total sectors - mbr - reserved sectors (boot sector) - root directory sectors) / sectors_per_cluster
	// NOTE this is just an estimate, as once the FAT is allocated, there are less clusters...
	Bitu clusters = (this->total_sectors - bootsect_pos - sbuf[0x0e] - root_sectors) / sbuf[0x0d];
	Bitu sect_per_fat = ((clusters >= 4085 ? clusters * 2 : (clusters * 3) / 2) + sector_size - 1) / sector_size; // FAT16 = (clusters >= 4085)
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
	sprintf((char*)&sbuf[0x2b], "RAMDISK    ");
	// file system type
	if (this->hardDrive) sprintf((char*)&sbuf[0x36], "FAT16   ");
	else sprintf((char*)&sbuf[0x36], "FAT12   ");
	// boot sector signature (indicates disk is bootable)
	host_writew(&sbuf[0x1fe], 0xAA55);

	// write the boot sector
	this->Write_AbsoluteSector(bootsect_pos, sbuf);

	// write FATs
	memset(sbuf, 0, sector_size);
	// erase both FATs and the root directory sectors
	for (int i = bootsect_pos + 1; i < bootsect_pos + 1 + sect_per_fat + sect_per_fat + root_sectors; i++) {
		this->Write_AbsoluteSector(i, sbuf);
	}
	// This code is probably for FAT32 drives...not sure.  The first cluster (identified as cluster id 2) starts immediately after the root directory entries
	//if (this->hardDrive) host_writed(&sbuf[0], 0xFFFFFFF8);
	//else host_writed(&sbuf[0], 0xFFFFF0);
	// write to each FAT
	//this->Write_AbsoluteSector(bootsect_pos + 1, sbuf);
	//this->Write_AbsoluteSector(bootsect_pos + 1 + sect_per_fat, sbuf);

	//success
	return 0x00;
}