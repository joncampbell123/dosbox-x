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
	// The fatDrive class is hard-coded to assume that disks 2880KB or smaller are floppies,
	//   whether or not they are attached to a floppy controller.  So, let's enforce a minimum
	//   size of 4096kb for hard drives.  Use the other constructor for floppy drives.
	if (imgSizeK < 4096) imgSizeK = 4096;

	//this code always returns drives with 512 byte sectors
	//first, it primarily prefers 16 sectors, and never more than 255
	//second, as many cylinders as possible less than 1024
	//third, as many heads as possible, up to a maximum of 63
	//
	//the code will round up in case it cannot make an exact match
	//it also forces a minimum drive size of 32kb, since a hard drive cannot be formatted as FAT12 with a smaller parition
	//the code works properly throughout the range of a 32-bit unsigned integer

	//so, these are examples of results: (assuming no 4096k minimum)
	//
	// KB requested      C     H     S
	//-----------------------------------
	//          0        4     1    16
	//          1        4     1    16
	//         32        4     1    16     <<---minimum enforced image size
	//        100       13     1    16
	//       1024      128     1    16
	//       4096      512     1    16     <<---however, with first line of code enforcing 4096kb drives, this is what gets returned
	//       8184     1023     1    16     <<---at 8mb the head count is increased
	//       8185      512     2    16
	//      65472     1023     8    16
	//      65473      910     9    16
	//     515592     1023    63    16     <<---at 503mb the sector count is increased
	//     515593      963    63    17
	//    8217247     1023    63   255     <<---at 8gb the cylinder count is increased beyond 1023 cylinders
	//    8217248     1024    63   255
	// 4294967295   534699    63   255     <<---can create drives up to MAXULONGLONG kb

	//use 32kb as minimum drive size, since it is not possible to format a smaller drive that has 16 sectors
	if (imgSizeK < 32) imgSizeK = 32;
	//set the sector size to 512 bytes
	Bit32u sector_size = 512;
	//calculate the total number of sectors on the drive (imgSizeK * 2)
	Bit64u total_sectors = ((Bit64u)imgSizeK * 1024 + sector_size - 1) / sector_size;
	//assume 1023 cylinders and 16 sectors minimum; calculate the number of heads (round up)
	Bit32u heads = (total_sectors + (1023 * 16 - 1)) / (1023 * 16);
	//cap heads at 63
	if (heads > 63) heads = 63;
	//now calculate the sectors (again, assuming 1023 cylinders) (round up)
	Bit32u sectors = (total_sectors + (heads * 1023 - 1)) / (heads * 1023);
	//set sectors to a minimum of 16 and maximum of 255
	if (sectors < 16) sectors = 16;
	if (sectors > 255) sectors = 255;
	//now calculate the correct number of cylinders (round up)
	Bit32u cylinders = (total_sectors + (heads * sectors - 1)) / (heads * sectors);
	//set minimum number of cylinders to be 4 (which equals a 32kb image)
	if (cylinders < 4) cylinders = 4;
	//it is possible for the cylinders to be > 1023 at this point - to be trapped within init()
	//minimum image size possible with this formula is 32kb (16 sectors, 1 head, 4 cylinders), which is enough to format 

	init(cylinders, heads, sectors, sector_size, true, 0, 0);
}

// Create a floppy image of a specified geometry
imageDiskMemory::imageDiskMemory(diskGeo floppyGeometry) {
	init(floppyGeometry.cylcount, floppyGeometry.headscyl, floppyGeometry.secttrack, floppyGeometry.bytespersect, false, floppyGeometry.biosval, 0);
}

// Return the BIOS type of the floppy image, or 0 for hard drives
Bit8u imageDiskMemory::GetBiosType(void) {
	return this->hardDrive ? 0 : bios_type;
}

// Create a hard drive image of a specified geometry
imageDiskMemory::imageDiskMemory(Bit32u cylinders, Bit32u heads, Bit32u sectors, Bit32u sector_size, bool isHardDrive, Bit8u floppyBiosMediaType) {
	init(cylinders, heads, sectors, sector_size, isHardDrive, floppyBiosMediaType, 0);
}

// Create a copy-on-write memory image of an existing image
imageDiskMemory::imageDiskMemory(imageDisk* underlyingImage) {
	init(underlyingImage->cylinders, underlyingImage->heads, underlyingImage->sectors, underlyingImage->sector_size, underlyingImage->hardDrive, underlyingImage->GetBiosType(), underlyingImage);
}

// Internal initialization code to create a image of a specified geometry
void imageDiskMemory::init(Bit32u cylinders, Bit32u heads, Bit32u sectors, Bit32u sector_size, bool isHardDrive, Bit8u floppyBiosMediaType, imageDisk* underlyingImage) {
	//initialize internal variables in case we fail out
	this->active = false;
	this->cylinders = 0;
	this->heads = 0;
	this->sectors = 0;
	this->sector_size = 0;
	this->total_sectors = 0;
	this->underlyingImage = underlyingImage;
	if (underlyingImage) underlyingImage->Addref();

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
	//assuming a maximum of 63 heads and 255 sectors, this formula gives a nice chunk size that scales with the drive size
	//for any drives under 64mb, the chunks are 8kb each, and the map consumes 1/1000 of the total drive size
	//then the chunk size grows up to a 8gb drive, while the map consumes 30-60k of memory (on 64bit PCs)
	//with a 8gb drive, the chunks are about 1mb each, and the map consumes about 60k of memory 
	//and for larger drives (with over 1023 cylinders), the chunks remain at 1mb each while the map grows
	this->sectors_per_chunk = (heads + 7) / 8 * sectors; 
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
	this->hardDrive = isHardDrive;
	this->bios_type = isHardDrive ? 0 : floppyBiosMediaType;
	this->active = true;
}

// imageDiskMemory destructor will release all allocated memory
imageDiskMemory::~imageDiskMemory() {
	//quit if the map is already not allocated
	if (!active) return;
	//release the underlying image
	if (this->underlyingImage) this->underlyingImage->Release();
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

	//if the chunk has not yet been allocated, return underlying image if any, or else zeros
	if (datalocation == 0) {
		if (this->underlyingImage) {
			return this->underlyingImage->Read_AbsoluteSector(sectnum, data);
		}
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
		//if no underlying image, first check if we are actually saving anything
		if (this->underlyingImage == NULL) {
			Bit8u anyData = 0;
			for (int i = 0; i < sector_size; i++) {
				anyData |= ((Bit8u*)data)[i];
			}
			//if it's all zeros, return success
			if (anyData == 0) return 0x00;
		}

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
		//if there is an underlying image, read from the underlying image to fill the chunk
		if (this->underlyingImage) {
			Bitu chunkFirstSector = chunknum * this->sectors_per_chunk;
			Bitu sectorsToCopy = this->sectors_per_chunk;
			//if this is the last chunk, don't read past the end of the original image
			if ((chunknum + 1) == this->total_chunks) sectorsToCopy = this->total_sectors - chunkFirstSector;
			//copy the sectors
			Bit8u* target = datalocation;
			for (Bitu i = 0; i < sectorsToCopy; i++) {
				this->underlyingImage->Read_AbsoluteSector(i + chunkFirstSector, target);
				datalocation += this->sector_size;
			}
		}
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
		LOG_MSG("imageDiskMemory::Format only designed for disks with 512-byte sectors.\n");
		return 0x01;
	}
	if (this->sectors > 63) {
		LOG_MSG("imageDiskMemory::Format only designed for disks with <= 63 sectors.\n");
		return 0x02;
	}
	if (this->heads > 255) {
		LOG_MSG("imageDiskMemory::Format only designed for disks with <= 255 heads.\n");
		return 0x05;
	}
	if (this->cylinders >= 1024) {
		LOG_MSG("imageDiskMemory::Format only designed for disks with < 1024 cylinders.\n");
		return 0x03;
	}
	if (!this->hardDrive && this->total_sectors >= 65535) {
		LOG_MSG("imageDiskMemory::Format only designed for floppies with < 65535 total sectors.\n");
		return 0x04;
	}
	
	//plan the drive
	//write a MBR?
	bool writeMBR = this->hardDrive;
	Bitu partitionStart = writeMBR ? this->sectors : 0; //must be aligned with the start of a head (multiple of this->sectors)
	Bitu partitionLength = this->total_sectors - partitionStart; //must be aligned with the start of a head (multiple of this->sectors)
	//figure out the media id
	Bit8u mediaID = 0xF0;
	//figure out the number of root entries and minimum number of sectors per cluster
	Bitu root_ent = 512;
	Bitu sectors_per_cluster = 1;
	if (!this->hardDrive)
	{
		switch (mediaID) {
			case 0xF0: root_ent = this->sectors == 36 ? 512 : 224; break;
			case 0xF9: root_ent = this->sectors == 15 ? 224 : 112; break;
			case 0xFC: root_ent = 56; break;
			case 0xFD: root_ent = 112; break;
			case 0xFE: root_ent = 56; break;
			case 0xFF: root_ent = 112; sectors_per_cluster = 2; break;
		}
	}

	//calculate the number of:
	//  root sectors
	//  FAT sectors
	//  reserved sectors
	//  sectors per cluster
	//  if this is a FAT16 or FAT12 drive
	Bitu root_sectors;
	bool isFat16;
	Bitu fatSectors;
	Bitu reservedSectors;
	if (!this->CalculateFAT(partitionStart, partitionLength, this->hardDrive, root_ent, &root_sectors, &sectors_per_cluster, &isFat16, &fatSectors, &reservedSectors)) {
		LOG_MSG("imageDiskMemory::Format could not calculate FAT sectors.\n");
		return 0x06;
	}

	//write MBR if applicable
	Bit8u sbuf[512];
	if (writeMBR) {
		// initialize sbuf with the freedos MBR
		memcpy(sbuf, freedos_mbr, 512);
		// active partition
		sbuf[0x1be] = 0x80;
		//ASSUMING that partitionStart==this->sectors;
		// start head - head 0 has the partition table, head 1 first partition
		sbuf[0x1bf] = this->heads > 1 ? 1 : 0;
		// start sector with bits 8-9 of start cylinder in bits 6-7
		sbuf[0x1c0] = this->heads > 1 ? 1 : (this->sectors > 1 ? 2 : 1);
		// start cylinder bits 0-7
		sbuf[0x1c1] = this->heads > 1 || this->sectors > 1 ? 0 : 1;
		// OS indicator: DOS what else ;)
		sbuf[0x1c2] = 0x06;
		//ASSUMING that partitionLength==(total_sectors-partitionStart)
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
	}

	// initialize boot sector values
	memset(sbuf, 0, 512);
	// TODO boot code jump
	sbuf[0] = 0xEB; sbuf[1] = 0x3c; sbuf[2] = 0x90;
	// OEM
	sprintf((char*)&sbuf[0x03], "MSDOS5.0");
	// bytes per sector: always 512
	host_writew(&sbuf[0x0b], 512);
	// sectors per cluster: 1,2,4,8,16,...
	sbuf[0x0d] = sectors_per_cluster;
	// reserved sectors: 1 for floppies (the boot sector), or align to 4k boundary for hard drives (not required to align to 4k boundary)
	host_writew(&sbuf[0x0e], reservedSectors);
	// Number of FATs - always 2
	sbuf[0x10] = 2;
	// Root directory entries
	host_writew(&sbuf[0x11], root_ent);
	// sectors (under 32MB) - will OSes be sore if all HD's use large size?
	if (!this->hardDrive) host_writew(&sbuf[0x13], partitionLength);
	// media descriptor
	sbuf[0x15] = mediaID;
	// sectors per FAT
	host_writew(&sbuf[0x16], fatSectors);
	// sectors per track
	host_writew(&sbuf[0x18], this->sectors);
	// heads
	host_writew(&sbuf[0x1a], this->heads);
	// hidden sectors
	//todo: check this -- shouldn't it be 1?
	host_writed(&sbuf[0x1c], partitionStart);
	// sectors (large disk) - this is the same as partition length in MBR
	if (this->hardDrive) host_writed(&sbuf[0x20], partitionLength);
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
	if (isFat16) sprintf((char*)&sbuf[0x36], "FAT16   ");
	else sprintf((char*)&sbuf[0x36], "FAT12   ");
	// boot sector signature (indicates disk is bootable)
	host_writew(&sbuf[0x1fe], 0xAA55);

	// write the boot sector
	this->Write_AbsoluteSector(partitionStart, sbuf);

	// initialize the FATs and root sectors
	memset(sbuf, 0, sector_size);
	// erase both FATs and the root directory sectors
	for (int i = partitionStart + 1; i < partitionStart + reservedSectors + fatSectors + fatSectors + root_sectors; i++) {
		this->Write_AbsoluteSector(i, sbuf);
	}
	// set the special markers for cluster 0 and cluster 1
	if (isFat16) {
		host_writed(&sbuf[0], 0xFFFFFF00 | mediaID);
	}
	else {
		host_writed(&sbuf[0], 0xFFFF00 | mediaID);
	}
	this->Write_AbsoluteSector(partitionStart + reservedSectors, sbuf);
	this->Write_AbsoluteSector(partitionStart + reservedSectors + fatSectors, sbuf);

	//success
	return 0x00;
}

// Calculate the number of sectors per cluster, the number of sectors per fat, and if it is FAT12/FAT16
// Note that sectorsPerCluster is required to be set to the minimum, which will be 2 for certain types of floppies
bool imageDiskMemory::CalculateFAT(Bitu partitionStartingSector, Bitu partitionLength, bool isHardDrive, Bitu rootEntries, Bitu* rootSectors, Bitu* sectorsPerCluster, bool* isFat16, Bitu* fatSectors, Bitu* reservedSectors) {
	//check for null references
	if (rootSectors == NULL || sectorsPerCluster == NULL || isFat16 == NULL || fatSectors == NULL || reservedSectors == NULL) return false;
	//make sure variables all make sense
	switch (*sectorsPerCluster) {
		case 1: case 2: case 4: case 8: case 16: case 32: case 64: case 128: break;
		default: return false;
	}
	if (((Bit64u)partitionStartingSector + partitionLength) > 0xfffffffful) return false;
	if (rootEntries > (isHardDrive ? 512 : 240)) return false;
	if ((rootEntries / 16) * 16 != rootEntries) return false;
	if (rootEntries == 0) return false;
	//set the number of root sectors
	*rootSectors = rootEntries * 32;
	//make sure there is a minimum number of sectors available
	//  minimum sectors = root sectors + 1 for boot sector + 1 for fat #1 + 1 for fat #2 + 1 for data sector + add 7 for hard drives due to allow for 4k alignment
	if (partitionLength < (*rootSectors + 4 + (isHardDrive ? 7 : 0))) return false;


	//set the minimum number of reserved sectors
	//the first sector of a partition is always reserved, of course
	*reservedSectors = 1;
	//if this is a hard drive, we will align to a 4kb boundary,
	//  so set (partitionStartingSector + reservedSectors) to an even number.
	//Additional alignment can be made by increasing the number of reserved sectors,
	//  or by increasing the reservedSectors value -- both done after the calculation
	if (isHardDrive && ((partitionStartingSector + *reservedSectors) % 2 == 1)) (*reservedSectors)++;


	//compute the number of fat sectors and data sectors
	Bitu dataSectors;
	//first try FAT12 - compute the minimum number of fat sectors necessary using the minimum number of sectors per cluster
	*fatSectors = (((Bit64u)partitionLength - *reservedSectors - *rootSectors + 2) * 3 + *sectorsPerCluster * 1024 + 5) / (*sectorsPerCluster * 1024 + 6);
	dataSectors = partitionLength - *reservedSectors - *rootSectors - *fatSectors - *fatSectors;
	//check if this calculation makes the drive too big to fit within a FAT12 drive
	if (dataSectors >= 4085) {
		//so instead let's calculate for FAT16, and increase the number of sectors per cluster if necessary
		//note: changing the drive to FAT16 will reducing the number of data sectors, which might change the drive
		//  from a FAT16 drive to a FAT12 drive.  however, the only difference is that there are unused fat sectors
		//  allocated.  we cannot allocate them, or else the drive will change back to a FAT16 drive, so, just leave it as-is
		do {
			//compute the minimum number of fat sectors necessary starting with the minimum number of sectors per cluster
			//  the +2 is for the two reserved data sectors (sector #0 and sector #1) which are not stored on the drive, but are in the map
			//  rounds up and assumes 512 byte sector size
			*fatSectors = ((Bit64u)partitionLength - *reservedSectors - *rootSectors + 2 + (*sectorsPerCluster * 256 + 1)) / (*sectorsPerCluster * 256 + 2);

			//compute the number of data sectors with this arrangement
			dataSectors = partitionLength - *reservedSectors - *rootSectors - *fatSectors - *fatSectors;

			//check and see if this makes a valid FAT16 drive
			if (dataSectors < 65525) break;

			//otherwise, double the number of sectors per cluster and try again
			*sectorsPerCluster <<= 1;
		} while (true);
		//determine if the drive is too large for FAT16
		if (*sectorsPerCluster >= 256) return false;
	}

	//add padding to align hard drives to 4kb boundary
	if (isHardDrive) {
		//update the reserved sector count
		*reservedSectors = (partitionStartingSector + *reservedSectors + *rootSectors + *fatSectors + *fatSectors) % 8;
		//recompute the number of data sectors
		dataSectors = partitionLength - *reservedSectors - *rootSectors - *fatSectors - *fatSectors;

		//note: by reducing the number of data sectors, the drive might have changed from a FAT16 drive to a FAT12 drive.
		//  however, the only difference is that there are unused fat sectors allocated.  we cannot allocate them, or else
		//  the drive alignment will change again, and most likely the drive would change back into a FAT16 drive.
		//  so, just leave it as-is
	}

	//determine whether the drive is FAT16 or not
	//note: this is AFTER adding padding for 4kb alignment
	*isFat16 = (dataSectors >= 4085);

	//success
	return true;
}