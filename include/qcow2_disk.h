/*
 *  Copyright (C) 2016  Michael Greger
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


#ifndef DOSBOX_QCOW2_DISK_H
#define DOSBOX_QCOW2_DISK_H


#include <iostream>
#include <fstream>
#include <iomanip>
#include <stdint.h>
#include "config.h"
#include "bios_disk.h"

class QCow2Image{

public:

	static const Bit32u magic;
	
	typedef struct QCow2Header {
		Bit32u magic;
		Bit32u version;
		Bit64u backing_file_offset;
		Bit32u backing_file_size;
		Bit32u cluster_bits;
		Bit64u size; /* in bytes */
		Bit32u crypt_method;
		Bit32u l1_size;
		Bit64u l1_table_offset;
		Bit64u refcount_table_offset;
		Bit32u refcount_table_clusters;
		Bit32u nb_snapshots;
		Bit64u snapshots_offset;
	} QCow2Header;
	
	static QCow2Header read_header(FILE* qcow2File);

	QCow2Image(QCow2Header& qcow2Header, FILE *qcow2File, const char* imageName, Bit32u sectorSizeBytes);

	virtual ~QCow2Image();
	
	uint8_t read_sector(Bit32u sectnum, uint8_t* data);

	uint8_t write_sector(Bit32u sectnum, uint8_t* data);
	
private:

	FILE* file;
	QCow2Header header;
	static const Bit64u copy_flag;
	static const Bit64u empty_mask;
	static const Bit64u table_entry_mask;
	Bit32u sector_size;
	Bit64u cluster_mask;
	Bit64u cluster_size;
	Bit64u sectors_per_cluster;
	Bit64u l2_mask;
	Bit64u l2_bits;
	Bit64u l1_bits;
	Bit64u refcount_mask;
	Bit64u refcount_bits;
	QCow2Image* backing_image;

	static uint16_t host_read16(uint16_t buffer);

	static Bit32u host_read32(Bit32u buffer);

	static Bit64u host_read64(Bit64u buffer);

	static Bit64u mask64(Bit64u bits);
	
	uint8_t pad_file(Bit64u& new_file_length);

	uint8_t read_allocated_data(Bit64u file_offset, uint8_t* data, Bit64u data_size);

	uint8_t read_cluster(Bit64u data_cluster_number, uint8_t* data);

	uint8_t read_l1_table(Bit64u address, Bit64u& l2_table_offset);

	uint8_t read_l2_table(Bit64u l2_table_offset, Bit64u address, Bit64u& data_cluster_offset);

	uint8_t read_refcount_table(Bit64u data_cluster_offset, Bit64u& refcount_cluster_offset);

	uint8_t read_table(Bit64u entry_offset, Bit64u entry_mask, Bit64u& entry_value);

	uint8_t read_unallocated_cluster(Bit64u data_cluster_number, uint8_t* data);

	uint8_t read_unallocated_sector(Bit32u sectnum, uint8_t* data);

	uint8_t update_reference_count(Bit64u cluster_offset, uint8_t* cluster_buffer);

	uint8_t write_data(Bit64u file_offset, uint8_t* data, Bit64u data_size);

	uint8_t write_l1_table_entry(Bit64u address, Bit64u l2_table_offset);

	uint8_t write_l2_table_entry(Bit64u l2_table_offset, Bit64u address, Bit64u data_cluster_offset);

	uint8_t write_refcount(Bit64u cluster_offset, Bit64u refcount_cluster_offset, uint16_t refcount);

	uint8_t write_refcount_table_entry(Bit64u cluster_offset, Bit64u refcount_cluster_offset);

	uint8_t write_table_entry(Bit64u entry_offset, Bit64u entry_value);
};

class QCow2Disk : public imageDisk{

public:
	
	QCow2Disk(QCow2Image::QCow2Header& qcow2Header, FILE *qcow2File, uint8_t *imgName, Bit32u imgSizeK, Bit32u sectorSizeBytes, bool isHardDisk);

	virtual ~QCow2Disk();
	
	virtual uint8_t Read_AbsoluteSector(Bit32u sectnum, void* data);

	virtual uint8_t Write_AbsoluteSector(Bit32u sectnum, const void* data);

private:

	QCow2Image qcowImage;
};

#endif
