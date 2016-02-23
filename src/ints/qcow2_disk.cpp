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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */


#include "qcow2_disk.h"


using namespace std;


//Public function to read a QCow2 header.
	QCow2Image::QCow2Header QCow2Image::read_header(FILE* qcow2File){
		QCow2Header header = QCow2Header();
		if (1 != fseek(qcow2File, 0, SEEK_SET)){
			clearerr(qcow2File);
		}
		fread(&header, sizeof header, 1, qcow2File);
		header.magic = host_read32(header.magic);
		header.version = host_read32(header.version);
		header.backing_file_offset = host_read64(header.backing_file_offset);
		header.backing_file_size = host_read32(header.backing_file_size);
		header.cluster_bits = host_read32(header.cluster_bits);
		header.size = host_read64(header.size);
		header.crypt_method = host_read32(header.crypt_method);
		header.l1_size = host_read32(header.l1_size);
		header.l1_table_offset = host_read64(header.l1_table_offset);
		header.refcount_table_offset = host_read64(header.refcount_table_offset);
		header.refcount_table_clusters = host_read32(header.refcount_table_clusters);
		header.nb_snapshots = host_read32(header.nb_snapshots);
		header.snapshots_offset = host_read64(header.snapshots_offset);
		return header;
	}


//Public Constructor.
	QCow2Image::QCow2Image(QCow2Image::QCow2Header qcow2_header, FILE *qcow2File) : file(qcow2File), header(qcow2_header)
	{
		cluster_mask = mask64(header.cluster_bits);
		cluster_size = cluster_mask + 1;
		sectors_per_cluster = cluster_size / sector_size;
		disk_sectors_total = header.size/sector_size;
		l2_bits = header.cluster_bits - 3;
		l2_mask = mask64(l2_bits);
		l1_bits = header.cluster_bits + l2_bits;
		refcount_bits = header.cluster_bits - 1;
		refcount_mask = mask64(refcount_bits);
		if (header.backing_file_offset != 0 && header.backing_file_size != 0){
			char* backing_file_name = new char[header.backing_file_size];
			fseek(file, header.backing_file_offset, SEEK_SET);
			fread(backing_file_name, header.backing_file_size, 1, file);
			FILE* backing_file = fopen(backing_file_name, "rb");
			QCow2Header backing_header = read_header(backing_file);
			backing_image = new QCow2Image(backing_header, backing_file);
			delete[] backing_file_name;
		}
	}


//Public Destructor.
	QCow2Image::~QCow2Image(){
		if (backing_image != NULL){
			fclose(backing_image->file);
			delete backing_image;
		}
	}


//Public function to a read a sector.
	Bit8u QCow2Image::read_sector(Bit32u sectnum, Bit8u* data){
		const Bit64u address = (Bit64u)sectnum * sector_size;
		if (address >= header.size){
			return 0x05;
		}
		Bit64u l2_table_offset;
		if (0 != read_l1_table(address, l2_table_offset)){
			return 0x05;
		}
		if (0 == l2_table_offset){
			return read_unallocated_sector(sectnum, data);
		}
		Bit64u data_cluster_offset;
		if (0 != read_l2_table(l2_table_offset, address, data_cluster_offset)){
			return 0x05;
		}
		if (0 == data_cluster_offset){
			return read_unallocated_sector(sectnum, data);
		}
		return read_allocated_data(data_cluster_offset + (address & cluster_mask), data, sector_size);
	}


//Public function to a write a sector.
	Bit8u QCow2Image::write_sector(Bit32u sectnum, Bit8u* data){
		const Bit64u address = (Bit64u)sectnum * sector_size;
		if (address >= header.size){
			return 0x05;
		}
		Bit64u l2_table_offset;
		if (0 != read_l1_table(address, l2_table_offset)){
			return 0x05;
		}
		if (0 == l2_table_offset){
			if (0 != pad_file(l2_table_offset)){
				return 0x05;
			}
			if (0 != write_l1_table_entry(address, l2_table_offset)){
				return 0x05;
			}
			Bit8u* cluster_buffer = new Bit8u[cluster_size];
			std::fill(cluster_buffer, cluster_buffer + cluster_size, 0);
			if (0 != write_data(l2_table_offset, cluster_buffer, cluster_size)){
				delete[] cluster_buffer;
				return 0x05;
			}
			if (0 != update_reference_count(l2_table_offset, cluster_buffer)){
				delete[] cluster_buffer;
				return 0x05;
			}
			delete[] cluster_buffer;
		}
		Bit64u data_cluster_offset;
		if (0 != read_l2_table(l2_table_offset, address, data_cluster_offset)){
			return 0x05;
		}
		if (data_cluster_offset == 0){
			if (0 != pad_file(data_cluster_offset)){
				return 0x05;
			}
			if (0 != write_l2_table_entry(l2_table_offset, address, data_cluster_offset)){
				return 0x05;
			}
			Bit8u* cluster_buffer = new Bit8u[cluster_size];
			if ( 0 != read_unallocated_cluster(address/cluster_size, cluster_buffer)){
				delete[] cluster_buffer;
				return 0x05;
			}
			const Bit64u cluster_buffer_sector_offset = address & cluster_mask;
			for (Bit64u i = 0; i < sector_size; i++){
				cluster_buffer[cluster_buffer_sector_offset + i] = data[i];
			}
			if (0 != write_data(data_cluster_offset, cluster_buffer, cluster_size)){
				delete[] cluster_buffer;
				return 0x05;
			}
			if (0 != update_reference_count(data_cluster_offset, cluster_buffer)){
				delete[] cluster_buffer;
				return 0x05;
			}
			delete[] cluster_buffer;
			return 0;
		}
		return write_data(data_cluster_offset + (address & cluster_mask), data, sector_size);
	}


//Helper functions for endianness. QCOW format is big endian so we need different functions than those defined in mem.h.
#if defined(WORDS_BIGENDIAN) || !defined(C_UNALIGNED_MEMORY)


	Bit16u QCow2Image::host_read16(Bit16u buffer) {
		return buffer;
	}


	Bit32u QCow2Image::host_read32(Bit32u buffer) {
		return buffer;
	}


	Bit64u QCow2Image::host_read64(Bit64u buffer) {
		return buffer;
	}


#else


	Bit16u QCow2Image::host_read16(Bit16u buffer) {
		Bit8u* b = (Bit8u*)&buffer;
		return  b[1] | (b[0] << 8);
	}


	Bit32u QCow2Image::host_read32(Bit32u buffer) {
		Bit8u* b = (Bit8u*)&buffer;
		return b[3] | (b[2] << 8) | (b[1] << 16) | (b[0] << 24);
	}


	Bit64u QCow2Image::host_read64(Bit64u buffer) {
		Bit8u* b = (Bit8u*)&buffer;
		return b[7] | (b[6] << 8) | (b[5] << 16) | (b[4] << 24) |  ((Bit64u)b[3] << 32) | ((Bit64u)b[2] << 40) | ((Bit64u)b[1] << 48) | ((Bit64u)b[0] << 56);
	}


#endif


//Generate a mask for a given number of bits.
	Bit64u QCow2Image::mask64(Bit64u bits){
		return (1 << bits) - 1;
	}


//Pad a file with zeros if it doesn't end on a cluster boundary.
	Bit8u QCow2Image::pad_file(Bit64u& new_file_length){
		if (0 != fseek(file, 0, SEEK_END)){
			return 0x05;
		}
		const Bit64u old_file_length = ftell(file);
		const Bit64u padding_size = (cluster_size - (old_file_length % cluster_size)) % cluster_size;
		new_file_length = old_file_length + padding_size;
		if (0 == padding_size){
			return 0;
		}
		Bit8u* padding = new Bit8u[padding_size];
		std::fill(padding, padding + padding_size, 0);
		Bit8u result = write_data(old_file_length, padding, padding_size);
		delete[] padding;
		return result;
	}


//Read data of arbitrary length that is present in the image file.
	Bit8u QCow2Image::read_allocated_data(Bit64u file_offset, Bit8u* data, Bit64u data_size)
	{
		if (0 != fseek(file, file_offset, SEEK_SET)){
			return 0x05;
		}
		if (1 != fread(data, data_size, 1, file)){
			return 0x05;
		}
		return 0;
	}


//Read an entire cluster that may or may not be allocated in the image file.
	inline Bit8u QCow2Image::read_cluster(Bit64u data_cluster_number, Bit8u* data)
	{
		const Bit64u address = data_cluster_number * cluster_size;
		if (address >= header.size){
			return 0x05;
		}
		Bit64u l2_table_offset;
		if (0 != read_l1_table(address, l2_table_offset)){
			return 0x05;
		}
		if (0 == l2_table_offset){
			return read_unallocated_cluster(data_cluster_number, data);
		}
		Bit64u data_cluster_offset;
		if (0 != read_l2_table(l2_table_offset, address, data_cluster_offset)){
			return 0x05;
		}
		if (0 == data_cluster_offset){
			return read_unallocated_cluster(data_cluster_number, data);
		}
		return read_allocated_data(data_cluster_offset, data, cluster_size);
	}


//Read the L1 table to get the offset of the L2 table for a given address.
	inline Bit8u QCow2Image::read_l1_table(Bit64u address, Bit64u& l2_table_offset){
		const Bit64u l1_entry_offset = header.l1_table_offset + ((address >> l1_bits) << 3);
		return read_table(l1_entry_offset, table_entry_mask, l2_table_offset);
	}


//Read an L2 table to get the offset of the data cluster for a given address.
	inline Bit8u QCow2Image::read_l2_table(Bit64u l2_table_offset, Bit64u address, Bit64u& data_cluster_offset){
		const Bit64u l2_entry_offset = l2_table_offset + (((address >> header.cluster_bits) & l2_mask) << 3);
		return read_table(l2_entry_offset, table_entry_mask, data_cluster_offset);
	}


//Read the refcount table to get the offset of the refcount cluster for a given address.
	inline Bit8u QCow2Image::read_refcount_table(Bit64u data_cluster_offset, Bit64u& refcount_cluster_offset){
		const Bit64u refcount_entry_offset = header.refcount_table_offset + (((data_cluster_offset/cluster_size) >> refcount_bits) << 3);
		return read_table(refcount_entry_offset, empty_mask, refcount_cluster_offset);
	}


//Read a table entry at the given offset.
	inline Bit8u QCow2Image::read_table(Bit64u entry_offset, Bit64u entry_mask, Bit64u& entry_value){
		Bit64u buffer;
		if (0 != read_allocated_data(entry_offset, (Bit8u*)&buffer, sizeof buffer)){
			return 0x05;
		}
		entry_value = host_read64(buffer) & table_entry_mask;
		return 0;
	}


//Read a cluster not currently allocated in the image file.
	inline Bit8u QCow2Image::read_unallocated_cluster(Bit64u data_cluster_number, Bit8u* data)
	{
		if(backing_image == NULL){
			std::fill(data, data + cluster_size, 0);
			return 0;
		}
		return backing_image->read_cluster(data_cluster_number, data);
	}


//Read a sector not currently allocated in the image file.
	inline Bit8u QCow2Image::read_unallocated_sector(Bit32u sectnum, Bit8u* data){
		if(backing_image == NULL){
			std::fill(data, data+sector_size, 0);
			return 0;
		}
		return backing_image->read_sector(sectnum, data);
	}

//Update the reference count for a cluster.
	inline Bit8u QCow2Image::update_reference_count(Bit64u cluster_offset, Bit8u* cluster_buffer){
		Bit64u refcount_cluster_offset;
		if (0 != read_refcount_table(cluster_offset, refcount_cluster_offset)){
			return 0x05;
		}
		if (0 == refcount_cluster_offset){
			refcount_cluster_offset = cluster_offset + cluster_size;
			std::fill(cluster_buffer, cluster_buffer + cluster_size, 0);
			if (0 != write_refcount_table_entry(cluster_offset, refcount_cluster_offset)){
				return 0x05;
			}
			if (0 != write_data(refcount_cluster_offset, cluster_buffer, cluster_size)){
			return 0x05;
			}
			if (0 != write_refcount(refcount_cluster_offset, refcount_cluster_offset, 0x1)){
				return 0x05;
			}
		}
		if (0 != write_refcount(cluster_offset, refcount_cluster_offset, 0x1)){
			return 0x05;
		}
		return 0;
	}


//Write data of arbitrary length to the image file.
	inline Bit8u QCow2Image::write_data(Bit64u file_offset, Bit8u* data, Bit64u data_size){
		if (0 != fseek(file, file_offset, SEEK_SET)){
			return 0x05;
		}
		if (1 != fwrite(data, data_size, 1, file)){
			return 0x05;
		}
		return 0;
	}


//Write an L2 table offset into the L1 table.
	inline Bit8u QCow2Image::write_l1_table_entry(Bit64u address, Bit64u l2_table_offset){
		const Bit64u l1_entry_offset = header.l1_table_offset + ((address >> l1_bits) << 3);
		return write_table_entry(l1_entry_offset, l2_table_offset | copy_flag);
	}


//Write a data cluster offset into an L2 table.
	inline Bit8u QCow2Image::write_l2_table_entry(Bit64u l2_table_offset, Bit64u address, Bit64u data_cluster_offset){
		const Bit64u l2_entry_offset = l2_table_offset + (((address >> header.cluster_bits) & l2_mask) << 3);
		return write_table_entry(l2_entry_offset, data_cluster_offset | copy_flag);
	}


//Write a refcount.
	inline Bit8u QCow2Image::write_refcount(Bit64u cluster_offset, Bit64u refcount_cluster_offset, Bit16u refcount){
		const Bit64u refcount_offset = refcount_cluster_offset + (((cluster_offset/cluster_size) & refcount_mask) << 1);
		Bit16u buffer = host_read16(refcount);
		return write_data(refcount_offset, (Bit8u*)&buffer, sizeof buffer);
	}


//Write a refcount table entry.
	inline Bit8u QCow2Image::write_refcount_table_entry(Bit64u cluster_offset, Bit64u refcount_cluster_offset){
		const Bit64u refcount_entry_offset = header.refcount_table_offset + (((cluster_offset/cluster_size) >> refcount_bits) << 3);
		return write_table_entry(refcount_entry_offset, refcount_cluster_offset);
	}


//Write a table entry at the given offset.
	inline Bit8u QCow2Image::write_table_entry(Bit64u entry_offset, Bit64u entry_value){
		Bit64u buffer = host_read64(entry_value);
		return write_data(entry_offset, (Bit8u*)&buffer, sizeof buffer);
	}


//Public Constructor.
	QCow2Disk::QCow2Disk(QCow2Image::QCow2Header header, FILE *qcow2File, Bit8u *imgName, Bit32u imgSizeK, bool isHardDisk) : imageDisk(qcow2File, imgName, imgSizeK, isHardDisk), qcowImage(header, qcow2File){
	}


//Public Destructor.
	QCow2Disk::~QCow2Disk(){
	}


//Public function to a read a sector.
	Bit8u QCow2Disk::Read_AbsoluteSector(Bit32u sectnum, void* data){
		return qcowImage.read_sector(sectnum, (Bit8u*)data);
	}


//Public function to a write a sector.
	Bit8u QCow2Disk::Write_AbsoluteSector(Bit32u sectnum, void* data){
		return qcowImage.write_sector(sectnum, (Bit8u*)data);
	}
