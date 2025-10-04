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

#include <stdint.h>
#include <stdio.h>

#include "bios_disk.h"

class QCow2Image{

public:

	static const uint32_t magic;
	
	typedef struct QCow2Header {
		uint32_t magic;
		uint32_t version;
		uint64_t backing_file_offset;
		uint32_t backing_file_size;
		uint32_t cluster_bits;
		uint64_t size; /* in bytes */
		uint32_t crypt_method;
		uint32_t l1_size;
		uint64_t l1_table_offset;
		uint64_t refcount_table_offset;
		uint32_t refcount_table_clusters;
		uint32_t nb_snapshots;
		uint64_t snapshots_offset;
	} QCow2Header;
	
	static QCow2Header read_header(FILE* qcow2File);

	QCow2Image(QCow2Header& qcow2Header, FILE *qcow2File, const char* imageName, uint32_t sectorSizeBytes);

	virtual ~QCow2Image();
	
	uint8_t read_sector(uint32_t sectnum, uint8_t* data);

	uint8_t write_sector(uint32_t sectnum, const uint8_t* data);
	
private:

	FILE* file;
	QCow2Header header;
	static const uint64_t copy_flag;
	static const uint64_t empty_mask;
	static const uint64_t table_entry_mask;
	uint32_t sector_size;
	uint64_t cluster_mask;
	uint64_t cluster_size;
	uint64_t sectors_per_cluster;
	uint64_t l2_mask;
	uint64_t l2_bits;
	uint64_t l1_bits;
	uint64_t refcount_mask;
	uint64_t refcount_bits;
	QCow2Image* backing_image;

	static uint16_t host_read16(uint16_t buffer);

	static uint32_t host_read32(uint32_t buffer);

	static uint64_t host_read64(uint64_t buffer);

	static uint64_t mask64(uint64_t bits);
	
	uint8_t pad_file(uint64_t& new_file_length);

	uint8_t read_allocated_data(uint64_t file_offset, uint8_t* data, uint64_t data_size);

	uint8_t read_cluster(uint64_t data_cluster_number, uint8_t* data);

	uint8_t read_l1_table(uint64_t address, uint64_t& l2_table_offset);

	uint8_t read_l2_table(uint64_t l2_table_offset, uint64_t address, uint64_t& data_cluster_offset);

	uint8_t read_refcount_table(uint64_t data_cluster_offset, uint64_t& refcount_cluster_offset);

	uint8_t read_table(uint64_t entry_offset, uint64_t entry_mask, uint64_t& entry_value);

	uint8_t read_unallocated_cluster(uint64_t data_cluster_number, uint8_t* data);

	uint8_t read_unallocated_sector(uint32_t sectnum, uint8_t* data);

	uint8_t update_reference_count(uint64_t cluster_offset, uint8_t* cluster_buffer);

	uint8_t write_data(uint64_t file_offset, const uint8_t* data, uint64_t data_size);

	uint8_t write_l1_table_entry(uint64_t address, uint64_t l2_table_offset);

	uint8_t write_l2_table_entry(uint64_t l2_table_offset, uint64_t address, uint64_t data_cluster_offset);

	uint8_t write_refcount(uint64_t cluster_offset, uint64_t refcount_cluster_offset, uint16_t refcount);

	uint8_t write_refcount_table_entry(uint64_t cluster_offset, uint64_t refcount_cluster_offset);

	uint8_t write_table_entry(uint64_t entry_offset, uint64_t entry_value);
};

class QCow2Disk : public imageDisk{

public:
	
	QCow2Disk(QCow2Image::QCow2Header& qcow2Header, FILE *qcow2File, const char *imgName, uint32_t imgSizeK, uint32_t sectorSizeBytes, bool isHardDisk);

	~QCow2Disk();
	
	uint8_t Read_AbsoluteSector(uint32_t sectnum, void* data) override;

	uint8_t Write_AbsoluteSector(uint32_t sectnum, const void* data) override;

private:

	QCow2Image qcowImage;
};

#endif
