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
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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
imageDiskMemory::imageDiskMemory(uint32_t imgSizeK) : imageDisk(ID_MEMORY) {
	//notes:
	//  this code always returns HARD DRIVES with 512 byte sectors
	//  the code will round up in case it cannot make an exact match
	//  it enforces a minimum drive size of 32kb, since a hard drive cannot be formatted as FAT12 with a smaller parition
	//  the code works properly throughout the range of a 32-bit unsigned integer, however:
	//    a) for drives requesting more than 8,225,280kb, the number of cylinders will exceed 1024
	//    b) for drives requesting ULONG_MAX kb, the drive it creates will be slightly larger than ULONG_MAX kb, due to rounding
    
	//BIOS and MBR c/h/s limits:      1024 / 255 /  63     8,225,280kb
	//ATA c/h/s limits:              65536 /  16 / 255   267,386,880kb
	//combined limits:                1024 /  16 /  63       516,096kb

	//since this is a virtual drive, we are not restricted to ATA limits, so go with BIOS/MBR limits

	//it targets c/h/s values as follows:
	//  minimum:                         4 /   1 /  16            32kb
	//  through:                      1024 /   1 /  16         8,192kb
	//  through:                      1024 /  16 /  16       131,072kb
	//  through:                      1024 /  16 /  63       516,096kb
	//  max FAT16 drive size:         1024 / 130 /  63     4,193,280kb
	//  through:                      1024 / 255 /  63     8,225,280kb
	//  maximum:                    534699 / 255 /  63             4tb
	
	//use 32kb as minimum drive size, since it is not possible to format a smaller drive that has 16 sectors
	if (imgSizeK < 32) imgSizeK = 32;
	//set the sector size to 512 bytes
	uint32_t sector_size = 512;
	//calculate the total number of sectors on the drive (imgSizeK * 2)
	uint64_t total_sectors = ((uint64_t)imgSizeK * 1024 + sector_size - 1) / sector_size;

	//calculate cylinders, sectors, and heads according to the targets above
	uint32_t sectors, heads, cylinders;
	if (total_sectors <= 1024 * 16 * 16) {
		sectors = 16;
		heads = (uint32_t)((total_sectors + (1024 * sectors - 1)) / (1024 * sectors));
		cylinders = (uint32_t)((total_sectors + (sectors * heads - 1)) / (sectors * heads));
	}
	else if (total_sectors <= 1024 * 16 * 63) {
		heads = 16;
		sectors = (uint32_t)((total_sectors + (1024 * heads - 1)) / (1024 * heads));
		cylinders = (uint32_t)((total_sectors + (sectors * heads - 1)) / (sectors * heads));
	}
	else if (total_sectors <= 1024 * 255 * 63) {
		sectors = 63;
		heads = (uint32_t)((total_sectors + (1024 * sectors - 1)) / (1024 * sectors));
		cylinders = (uint32_t)((total_sectors + (sectors * heads - 1)) / (sectors * heads));
	}
	else {
		sectors = 63;
		heads = 255;
		cylinders = (uint32_t)((total_sectors + (sectors * heads - 1)) / (sectors * heads));
	}

	LOG_MSG("Creating ramdrive as C/H/S %u/%u/%u with %u bytes/sector\n",
		(unsigned int)cylinders, (unsigned int)heads, (unsigned int)sectors, (unsigned int)sector_size);

	diskGeo diskParams;
	diskParams.secttrack = sectors;
	diskParams.cylcount = cylinders;
	diskParams.headscyl = heads;
	diskParams.bytespersect = sector_size;
	diskParams.biosval = 0;
	diskParams.ksize = imgSizeK;
	diskParams.mediaid = 0xF0;
	diskParams.rootentries = 512;
	diskParams.biosval = 0;
	diskParams.sectcluster = 1;
	init(diskParams, true, 0);
}

// Create a floppy image of a specified geometry
imageDiskMemory::imageDiskMemory(const diskGeo& floppyGeometry) : imageDisk(ID_MEMORY) {
	init(floppyGeometry, false, 0);
}

// Create a hard drive image of a specified geometry
imageDiskMemory::imageDiskMemory(uint16_t cylinders, uint16_t heads, uint16_t sectors, uint16_t sector_size) : imageDisk(ID_MEMORY) {
	diskGeo diskParams;
	diskParams.secttrack = sectors;
	diskParams.cylcount = cylinders;
	diskParams.headscyl = heads;
	diskParams.bytespersect = sector_size;
	diskParams.biosval = 0;
	diskParams.ksize = 0;
	diskParams.mediaid = 0xF0;
	diskParams.rootentries = 512;
	diskParams.biosval = 0;
	diskParams.sectcluster = 1;
	init(diskParams, true, 0);
}

// Create a copy-on-write memory image of an existing image
imageDiskMemory::imageDiskMemory(imageDisk* underlyingImage) : imageDisk(ID_MEMORY) {
	diskGeo diskParams;
	uint32_t heads, cylinders, sectors, bytesPerSector;
	underlyingImage->Get_Geometry(&heads, &cylinders, &sectors, &bytesPerSector);
	diskParams.headscyl = (uint16_t)heads;
	diskParams.cylcount = (uint16_t)cylinders;
	diskParams.secttrack = (uint16_t)sectors;
	diskParams.bytespersect = (uint16_t)bytesPerSector;
	diskParams.biosval = 0;
	diskParams.ksize = 0;
	diskParams.mediaid = 0xF0;
	diskParams.rootentries = 0;
	diskParams.biosval = 0;
	diskParams.sectcluster = 0;
	init(diskParams, true, underlyingImage);
}

// Internal initialization code to create a image of a specified geometry
void imageDiskMemory::init(diskGeo diskParams, bool isHardDrive, imageDisk* underlyingImage) {
	//initialize internal variables in case we fail out
	this->total_sectors = 0;
	this->underlyingImage = underlyingImage;
	if (underlyingImage) underlyingImage->Addref();

	//calculate the total number of sectors on the drive, and check for overflows
	uint64_t absoluteSectors = (uint64_t)diskParams.cylcount * (uint64_t)diskParams.headscyl;
	if (absoluteSectors > 0x100000000u) {
		LOG_MSG("Image size too large in imageDiskMemory constructor.\n");
		return;
	}
	absoluteSectors *= (uint64_t)diskParams.secttrack;
	if (absoluteSectors > 0x100000000u) {
		LOG_MSG("Image size too large in imageDiskMemory constructor.\n");
		return;
	}
	//make sure that the number of total sectors on the drive is not zero
	if (absoluteSectors == 0) {
		LOG_MSG("Image size too small in imageDiskMemory constructor.\n");
		return;
	}

	//calculate total size of the drive in kilobytes, and check for overflow
	uint64_t diskSizeK = ((uint64_t)diskParams.headscyl * (uint64_t)diskParams.cylcount * (uint64_t)diskParams.secttrack * (uint64_t)diskParams.bytespersect + (uint64_t)1023) / (uint64_t)1024;
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
	this->sectors_per_chunk = (diskParams.headscyl + 7u) / 8u * diskParams.secttrack;
	this->total_chunks = (uint32_t)((absoluteSectors + sectors_per_chunk - 1u) / sectors_per_chunk);
	this->chunk_size = sectors_per_chunk * diskParams.bytespersect;
	//allocate a map of chunks that have been allocated and their memory addresses
	ChunkMap = (uint8_t**)malloc(total_chunks * sizeof(uint8_t*));
	if (ChunkMap == NULL) {
		LOG_MSG("Error allocating memory map in imageDiskMemory constructor for %lu clusters.\n", (unsigned long)total_chunks);
		return;
	}
	//clear memory map
	memset((void*)ChunkMap, 0, total_chunks * sizeof(uint8_t*));

	//set internal variables
	this->diskname = "RAM drive";
	this->heads = diskParams.headscyl;
	this->cylinders = diskParams.cylcount;
	this->sectors = diskParams.secttrack;
	this->sector_size = diskParams.bytespersect;
	this->diskSizeK = diskSizeK;
	this->total_sectors = (uint32_t)absoluteSectors;
	this->reserved_cylinders = 0;
	this->hardDrive = isHardDrive;
	this->floppyInfo = diskParams;
	this->active = true;
}

// imageDiskMemory destructor will release all allocated memory
imageDiskMemory::~imageDiskMemory() {
	//quit if the map is already not allocated
	if (!active) return;
	//release the underlying image
	if (this->underlyingImage) this->underlyingImage->Release();
	//loop through each chunk and release it if it has been allocated
	for (uint32_t i = 0; i < total_chunks; i++) {
		uint8_t* chunk = ChunkMap[i];
		if (chunk) free(chunk);
	}
	//release the memory map
	free(ChunkMap);
	//reset internal variables
	ChunkMap = 0;
	total_sectors = 0;
	active = false;
}

// Return the BIOS type of the floppy image, or 0 for hard drives
uint8_t imageDiskMemory::GetBiosType(void) {
	return this->hardDrive ? 0 : this->floppyInfo.biosval;
}

void imageDiskMemory::Set_Geometry(uint32_t setHeads, uint32_t setCyl, uint32_t setSect, uint32_t setSectSize) {
	if (setHeads != this->heads || setCyl != this->cylinders || setSect != this->sectors || setSectSize != this->sector_size) {
		LOG_MSG("imageDiskMemory::Set_Geometry not implemented");
		//validate geometry and resize ramdrive
	}
}

// Read a specific sector from the ramdrive
uint8_t imageDiskMemory::Read_AbsoluteSector(uint32_t sectnum, void * data) {
	//sector number is a zero-based offset

	//verify the sector number is valid
	if (sectnum >= total_sectors) {
		LOG_MSG("Invalid sector number in Read_AbsoluteSector for sector %lu.\n", (unsigned long)sectnum);
		return 0x05;
	}

	//calculate which chunk the sector is located within, and which sector within the chunk
	uint32_t chunknum, chunksect;
	chunknum = sectnum / sectors_per_chunk;
	chunksect = sectnum % sectors_per_chunk;

	//retrieve the memory address of the chunk
	uint8_t* datalocation;
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
uint8_t imageDiskMemory::Write_AbsoluteSector(uint32_t sectnum, const void * data) {
	//sector number is a zero-based offset

	//verify the sector number is valid
	if (sectnum >= total_sectors) {
		LOG_MSG("Invalid sector number in Write_AbsoluteSector for sector %lu.\n", (unsigned long)sectnum);
		return 0x05;
	}

	//calculate which chunk the sector is located within, and which sector within the chunk
	uint32_t chunknum, chunksect;
	chunknum = sectnum / sectors_per_chunk;
	chunksect = sectnum % sectors_per_chunk;

	//retrieve the memory address of the chunk
	uint8_t* datalocation;
	datalocation = ChunkMap[chunknum];

	//if the chunk has not yet been allocated, allocate the chunk
	if (datalocation == NULL) {
		//if no underlying image, first check if we are actually saving anything
		if (this->underlyingImage == NULL) {
			uint8_t anyData = 0;
			for (uint32_t i = 0; i < sector_size; i++) {
				anyData |= ((uint8_t*)data)[i];
			}
			//if it's all zeros, return success
			if (anyData == 0) return 0x00;
		}

		//allocate a new memory chunk
		datalocation = (uint8_t*)malloc(chunk_size);
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
			uint32_t chunkFirstSector = chunknum * this->sectors_per_chunk;
			uint32_t sectorsToCopy = this->sectors_per_chunk;
			//if this is the last chunk, don't read past the end of the original image
			if ((chunknum + 1) == this->total_chunks) sectorsToCopy = this->total_sectors - chunkFirstSector;
			//copy the sectors
			uint8_t* target = datalocation;
			for (uint32_t i = 0; i < sectorsToCopy; i++) {
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

// Parition and format the ramdrive
uint8_t imageDiskMemory::Format() {
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
		return 0x03;
	}
	if (this->cylinders <= this->Get_Reserved_Cylinders()) {
		LOG_MSG("Invalid number of reserved cylinders in imageDiskMemory::Format\n");
		return 0x06;
	}
	uint32_t reported_cylinders = this->cylinders - this->Get_Reserved_Cylinders();
	if (reported_cylinders > 1024) {
		LOG_MSG("imageDiskMemory::Format only designed for disks with <= 1024 cylinders.\n");
		return 0x04;
	}
	uint32_t reported_total_sectors = reported_cylinders * this->heads * this->sectors;

	//plan the drive
	//write a MBR?
	bool writeMBR = this->hardDrive;
	uint32_t partitionStart = writeMBR ? this->sectors : 0; //must be aligned with the start of a head (multiple of this->sectors)
	uint32_t partitionLength = reported_total_sectors - partitionStart; //must be aligned with the start of a head (multiple of this->sectors)
	//figure out the media id
	uint8_t mediaID = this->hardDrive ? 0xF8 : this->floppyInfo.mediaid;
	//figure out the number of root entries and minimum number of sectors per cluster
	uint32_t root_ent = this->hardDrive ? 512 : this->floppyInfo.rootentries;
	uint32_t sectors_per_cluster = this->hardDrive ? 4 : this->floppyInfo.sectcluster; //fat requires 2k clusters minimum on hard drives

	//calculate the number of:
	//  root sectors
	//  FAT sectors
	//  reserved sectors
	//  sectors per cluster
	//  if this is a FAT16 or FAT12 drive
	uint32_t root_sectors;
	bool isFat16;
	uint32_t fatSectors;
	uint32_t reservedSectors;
	if (!this->CalculateFAT(partitionStart, partitionLength, this->hardDrive, root_ent, &root_sectors, &sectors_per_cluster, &isFat16, &fatSectors, &reservedSectors)) {
		LOG_MSG("imageDiskMemory::Format could not calculate FAT sectors.\n");
		return 0x05;
	}

	LOG_MSG("Formatting FAT%u %s drive C/H/S %u/%u/%u with %u bytes/sector, %u root entries, %u-byte clusters, media id 0x%X\n",
		(unsigned int)(isFat16 ? 16 : 12), this->hardDrive ? "hard" : "floppy",
		(unsigned int)reported_cylinders, (unsigned int)this->heads, (unsigned int)this->sectors, (unsigned int)this->sector_size,
		(unsigned int)root_ent, (unsigned int)(sectors_per_cluster * this->sector_size), (unsigned int)mediaID);

	//write MBR if applicable
	uint8_t sbuf[512];
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
		//ASSUMING that partitionLength==(reported_total_sectors-partitionStart)
		// end head (0-based)
		sbuf[0x1c3] = this->heads - 1;
		// end sector with bits 8-9 of end cylinder (0-based) in bits 6-7
		sbuf[0x1c4] = this->sectors | (((this->cylinders - 1 - this->Get_Reserved_Cylinders()) & 0x300) >> 2);
		// end cylinder (0-based) bits 0-7
		sbuf[0x1c5] = (this->cylinders - 1 - this->Get_Reserved_Cylinders()) & 0xFF;
		// sectors preceding partition1 (one head)
		host_writed(&sbuf[0x1c6], this->sectors);
		// length of partition1, align to chs value
		// ASSUMING that partitionLength aligns to chs value
		host_writed(&sbuf[0x1ca], partitionLength);

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
	// total sectors (<= 65535)
	if (partitionLength < 0x10000u) host_writew(&sbuf[0x13], (uint16_t)partitionLength);
	// media descriptor
	sbuf[0x15] = mediaID;
	// sectors per FAT
	host_writew(&sbuf[0x16], fatSectors);
	// sectors per track
	host_writew(&sbuf[0x18], this->sectors);
	// heads
	host_writew(&sbuf[0x1a], this->heads);
	// hidden sectors
	host_writed(&sbuf[0x1c], partitionStart);
	// sectors (>= 65536)
	if (partitionLength >= 0x10000u) host_writed(&sbuf[0x20], partitionLength);
	// BIOS drive
	sbuf[0x24] = this->hardDrive ? 0x80 : 0x00;
	// ext. boot signature
	sbuf[0x26] = 0x29;
	// volume serial number
	// let's use the BIOS time (cheap, huh?)
	host_writed(&sbuf[0x27], mem_readd(BIOS_TIMER));
	// Volume label
	sprintf((char*)&sbuf[0x2b], "RAMDISK    ");
	// file system type
	sprintf((char*)&sbuf[0x36], isFat16 ? "FAT16   " : "FAT12   ");
	// boot sector signature (indicates disk is bootable)
	host_writew(&sbuf[0x1fe], 0xAA55);

	// write the boot sector
	this->Write_AbsoluteSector(partitionStart, sbuf);

	// initialize the FATs and root sectors
	memset(sbuf, 0, sector_size);
	// erase both FATs and the root directory sectors
	for (uint32_t i = partitionStart + 1; i < partitionStart + reservedSectors + fatSectors + fatSectors + root_sectors; i++) {
		this->Write_AbsoluteSector(i, sbuf);
	}
	// set the special markers for cluster 0 and cluster 1
	if (isFat16) {
		host_writed(&sbuf[0], 0xFFFFFF00u | mediaID);
	}
	else {
		host_writed(&sbuf[0], 0xFFFF00u | mediaID);
	}
	this->Write_AbsoluteSector(partitionStart + reservedSectors, sbuf);
	this->Write_AbsoluteSector(partitionStart + reservedSectors + fatSectors, sbuf);

	//success
	return 0x00;
}

// Calculate the number of sectors per cluster, the number of sectors per fat, and if it is FAT12/FAT16
// Note that sectorsPerCluster is required to be set to the minimum, which will be 2 for certain types of floppies
bool imageDiskMemory::CalculateFAT(uint32_t partitionStartingSector, uint32_t partitionLength, bool isHardDrive, uint32_t rootEntries, uint32_t* rootSectors, uint32_t* sectorsPerCluster, bool* isFat16, uint32_t* fatSectors, uint32_t* reservedSectors) {
	//check for null references
	if (rootSectors == NULL || sectorsPerCluster == NULL || isFat16 == NULL || fatSectors == NULL || reservedSectors == NULL) return false;
	//make sure variables all make sense
	switch (*sectorsPerCluster) {
		case 1: case 2: case 4: case 8: case 16: case 32: case 64: case 128: break;
		default:
			LOG_MSG("Invalid number of sectors per cluster\n");
			return false;
	}
	if (((uint64_t)partitionStartingSector + partitionLength) > 0xfffffffful) {
		LOG_MSG("Invalid partition size\n");
		return false;
	}
	if ((rootEntries > (isHardDrive ? 512u : 240u)) ||
		(rootEntries / 16) * 16 != rootEntries ||
		rootEntries == 0) {
		LOG_MSG("Invalid number of root entries\n");
		return false;
	}
	//set the number of root sectors
	*rootSectors = rootEntries * 32 / 512;
	//make sure there is a minimum number of sectors available
	//  minimum sectors = root sectors + 1 for boot sector + 1 for fat #1 + 1 for fat #2 + 1 cluster for data + add 7 for hard drives due to allow for 4k alignment
	if (partitionLength < (*rootSectors + 3 + *sectorsPerCluster + (isHardDrive ? 7 : 0))) {
		LOG_MSG("Partition too small to format\n");
		return false;
	}


	//set the minimum number of reserved sectors
	//the first sector of a partition is always reserved, of course
	*reservedSectors = 1;
	//if this is a hard drive, we will align to a 4kb boundary,
	//  so set (partitionStartingSector + reservedSectors) to an even number.
	//Additional alignment can be made by increasing the number of reserved sectors,
	//  or by increasing the reservedSectors value -- both done after the calculation
	if (isHardDrive && ((partitionStartingSector + *reservedSectors) % 2 == 1)) (*reservedSectors)++;


	//compute the number of fat sectors and data sectors
	uint32_t dataSectors;
	uint32_t dataClusters;
	//first try FAT12 - compute the minimum number of fat sectors necessary using the minimum number of sectors per cluster
	*fatSectors = (((uint64_t)partitionLength - *reservedSectors - *rootSectors + 2) * 3 + *sectorsPerCluster * 1024 + 5) / (*sectorsPerCluster * 1024 + 6);
	dataSectors = partitionLength - *reservedSectors - *rootSectors - *fatSectors - *fatSectors;
	dataClusters = dataSectors / *sectorsPerCluster;
	//check if this calculation makes the drive too big to fit within a FAT12 drive
	if (dataClusters >= 4085) {
		//so instead let's calculate for FAT16, and increase the number of sectors per cluster if necessary
		//note: changing the drive to FAT16 will reducing the number of data sectors, which might change the drive
		//  from a FAT16 drive to a FAT12 drive.  however, the only difference is that there are unused fat sectors
		//  allocated.  we cannot allocate them, or else the drive will change back to a FAT16 drive, so, just leave it as-is
		do {
			//compute the minimum number of fat sectors necessary starting with the minimum number of sectors per cluster
			//  the +2 is for the two reserved data sectors (sector #0 and sector #1) which are not stored on the drive, but are in the map
			//  rounds up and assumes 512 byte sector size
			*fatSectors = ((uint64_t)partitionLength - *reservedSectors - *rootSectors + 2 + (*sectorsPerCluster * 256 + 1)) / (*sectorsPerCluster * 256 + 2);

			//compute the number of data sectors and clusters with this arrangement
			dataSectors = partitionLength - *reservedSectors - *rootSectors - *fatSectors - *fatSectors;
			dataClusters = dataSectors / *sectorsPerCluster;

			//check and see if this makes a valid FAT16 drive
			if (dataClusters < 65525) break;

			//otherwise, double the number of sectors per cluster and try again
			*sectorsPerCluster <<= 1;
		} while (true);
		//determine if the drive is too large for FAT16
		if (*sectorsPerCluster >= 256) {
			LOG_MSG("Partition too large to format as FAT16\n");
			return false;
		}
	}

	//add padding to align hard drives to 4kb boundary
	if (isHardDrive) {
		//update the reserved sector count
		*reservedSectors = (partitionStartingSector + *reservedSectors + *rootSectors + *fatSectors + *fatSectors) % 8;
		if (*reservedSectors == 0) *reservedSectors = 8;
		//recompute the number of data sectors
		dataSectors = partitionLength - *reservedSectors - *rootSectors - *fatSectors - *fatSectors;
		dataClusters = dataSectors / *sectorsPerCluster;

		//note: by reducing the number of data sectors, the drive might have changed from a FAT16 drive to a FAT12 drive.
		//  however, the only difference is that there are unused fat sectors allocated.  we cannot allocate them, or else
		//  the drive alignment will change again, and most likely the drive would change back into a FAT16 drive.
		//  so, just leave it as-is
	}

	//determine whether the drive is FAT16 or not
	//note: this is AFTER adding padding for 4kb alignment
	*isFat16 = (dataClusters >= 4085);

	//success
	return true;
}
