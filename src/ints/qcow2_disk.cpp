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


#include "qcow2_disk.h"

#if defined(_MSC_VER)
# pragma warning(disable:4244) /* const fmath::local::uint64_t to double possible loss of data */
#endif

using namespace std;


//Public constant.
	const uint32_t QCow2Image::magic = 0x514649FB;


//Public function to read a QCow2 header.
	QCow2Image::QCow2Header QCow2Image::read_header(FILE* qcow2File){
		QCow2Header header;
		fseeko64(qcow2File, 0, SEEK_SET);
		if (1 != fread(&header, sizeof header, 1, qcow2File)){
			clearerr(qcow2File);  /*If we fail, reset the file stream's status*/
			return QCow2Header(); /*Return an empty header*/
		}
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
	QCow2Image::QCow2Image(QCow2Image::QCow2Header& qcow2Header, FILE *qcow2File, const char* imageName, uint32_t sectorSizeBytes) : file(qcow2File), header(qcow2Header), sector_size(sectorSizeBytes), backing_image(NULL)
	{
		cluster_mask = mask64(header.cluster_bits);
		cluster_size = cluster_mask + 1;
		sectors_per_cluster = cluster_size / sector_size;
		l2_bits = header.cluster_bits - 3;
		l2_mask = mask64(l2_bits);
		l1_bits = header.cluster_bits + l2_bits;
		refcount_bits = header.cluster_bits - 1;
		refcount_mask = mask64(refcount_bits);
		if (header.backing_file_offset != 0 && header.backing_file_size != 0){
			char* backing_file_name = new char[header.backing_file_size + 1];
			backing_file_name[header.backing_file_size] = 0;
			fseeko64(file, (off_t)header.backing_file_offset, SEEK_SET);
            size_t readResult = fread(backing_file_name, header.backing_file_size, 1, file);
            if (readResult != 1) {
                LOG(LOG_IO, LOG_ERROR) ("Reading error in QCow2Image constructor\n");
                delete[] backing_file_name;
                return;
            }
			if (backing_file_name[0] != 0x2F){
				for (int image_name_index = (int)strlen(imageName); image_name_index > -1; image_name_index--){
					if (imageName[image_name_index] == 0x2F){
						int full_name_length = (int)((unsigned int)header.backing_file_size + (unsigned int)image_name_index + 2u);
						char* full_name = new char[full_name_length];
						for(int full_name_index = 0; full_name_index < full_name_length; full_name_index++){
							if (full_name_index <= image_name_index){
								full_name[full_name_index] = imageName[full_name_index];
							} else {
								full_name[full_name_index] = backing_file_name[full_name_index - (image_name_index + 1)];
							}
						}
						delete[] backing_file_name;
						backing_file_name = full_name;
						break;
					}
				}
			}
			FILE* backing_file = fopen(backing_file_name, "rb");
			if (backing_file != NULL){
				QCow2Header backing_header = read_header(backing_file);
				backing_image = new QCow2Image(backing_header, backing_file, backing_file_name, sectorSizeBytes);
			} else {
				LOG_MSG("Failed to load QCow2 backing image: %s", backing_file_name);
			}
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
	uint8_t QCow2Image::read_sector(uint32_t sectnum, uint8_t* data){
		const uint64_t address = (uint64_t)sectnum * sector_size;
		if (address >= header.size){
			return 0x05;
		}
		uint64_t l2_table_offset;
		if (0 != read_l1_table(address, l2_table_offset)){
			return 0x05;
		}
		if (0 == l2_table_offset){
			return read_unallocated_sector(sectnum, data);
		}
		uint64_t data_cluster_offset;
		if (0 != read_l2_table(l2_table_offset, address, data_cluster_offset)){
			return 0x05;
		}
		if (0 == data_cluster_offset){
			return read_unallocated_sector(sectnum, data);
		}
		return read_allocated_data(data_cluster_offset + (address & cluster_mask), data, sector_size);
	}


//Public function to a write a sector.
	uint8_t QCow2Image::write_sector(uint32_t sectnum, const uint8_t* data){
		const uint64_t address = (uint64_t)sectnum * sector_size;
		if (address >= header.size){
			return 0x05;
		}
		uint64_t l2_table_offset;
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
			uint8_t* cluster_buffer = new uint8_t[cluster_size];
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
		uint64_t data_cluster_offset;
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
			uint8_t* cluster_buffer = new uint8_t[cluster_size];
			if ( 0 != read_unallocated_cluster(address/cluster_size, cluster_buffer)){
				delete[] cluster_buffer;
				return 0x05;
			}
			const uint64_t cluster_buffer_sector_offset = address & cluster_mask;
			for (uint64_t i = 0; i < sector_size; i++){
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


//Private constants.
	const uint64_t QCow2Image::copy_flag = 0x8000000000000000;
	const uint64_t QCow2Image::empty_mask = 0xFFFFFFFFFFFFFFFF;
	const uint64_t QCow2Image::table_entry_mask = 0x00FFFFFFFFFFFFFF;


//Helper functions for endianness. QCOW format is big endian so we need different functions than those defined in mem.h.
#if defined(WORDS_BIGENDIAN) || !defined(C_UNALIGNED_MEMORY)
    inline uint16_t QCow2Image::host_read16(uint16_t buffer) {
        return buffer;
    }

    inline uint32_t QCow2Image::host_read32(uint32_t buffer) {
        return buffer;
    }

    inline uint64_t QCow2Image::host_read64(uint64_t buffer) {
        return buffer;
    }
#else
    inline uint16_t QCow2Image::host_read16(uint16_t buffer) {
        return (buffer >> 8) | (buffer << 8);
    }

    inline uint32_t QCow2Image::host_read32(uint32_t buffer) {
        return (buffer >> 24) | (((buffer >> 16) & 0xff) << 8) |
            (((buffer >> 8) & 0xff) << 16) | (buffer << 24);
    }

    inline uint64_t QCow2Image::host_read64(uint64_t buffer) {
        return
            (buffer >> 56) | (((buffer >> 48) & 0xff) << 8) |
            (((buffer >> 40) & 0xff) << 16) | (((buffer >> 32) & 0xff) << 24) |
            (((buffer >> 24) & 0xff) << 32) | (((buffer >> 16) & 0xff) << 40) |
            (((buffer >> 8) & 0xff) << 48) | (buffer << 56);
    }
#endif


//Generate a mask for a given number of bits.
	inline uint64_t QCow2Image::mask64(uint64_t bits){
		return (1ull << bits) - 1ull;
	}


//Pad a file with zeros if it doesn't end on a cluster boundary.
	uint8_t QCow2Image::pad_file(uint64_t& new_file_length){
		if (0 != fseeko64(file, 0, SEEK_END)){
			return 0x05;
		}
		const uint64_t old_file_length = (uint64_t)ftello64(file);
		const uint64_t padding_size = (cluster_size - (old_file_length % cluster_size)) % cluster_size;
		new_file_length = old_file_length + padding_size;
		if (0 == padding_size){
			return 0;
		}
		uint8_t* padding = new uint8_t[padding_size];
		std::fill(padding, padding + padding_size, 0);
		uint8_t result = write_data(old_file_length, padding, padding_size);
		delete[] padding;
		return result;
	}


//Read data of arbitrary length that is present in the image file.
	uint8_t QCow2Image::read_allocated_data(uint64_t file_offset, uint8_t* data, uint64_t data_size)
	{
		if (0 != fseeko64(file, (off_t)file_offset, SEEK_SET)){
			return 0x05;
		}
		if (1 != fread(data, data_size, 1, file)){
			return 0x05;
		}
		return 0;
	}


//Read an entire cluster that may or may not be allocated in the image file.
	uint8_t QCow2Image::read_cluster(uint64_t data_cluster_number, uint8_t* data)
	{
		const uint64_t address = data_cluster_number * cluster_size;
		if (address >= header.size){
			return 0x05;
		}
		uint64_t l2_table_offset;
		if (0 != read_l1_table(address, l2_table_offset)){
			return 0x05;
		}
		if (0 == l2_table_offset){
			return read_unallocated_cluster(data_cluster_number, data);
		}
		uint64_t data_cluster_offset;
		if (0 != read_l2_table(l2_table_offset, address, data_cluster_offset)){
			return 0x05;
		}
		if (0 == data_cluster_offset){
			return read_unallocated_cluster(data_cluster_number, data);
		}
		return read_allocated_data(data_cluster_offset, data, cluster_size);
	}


//Read the L1 table to get the offset of the L2 table for a given address.
	inline uint8_t QCow2Image::read_l1_table(uint64_t address, uint64_t& l2_table_offset){
		const uint64_t l1_entry_offset = header.l1_table_offset + ((address >> l1_bits) << 3);
		return read_table(l1_entry_offset, table_entry_mask, l2_table_offset);
	}


//Read an L2 table to get the offset of the data cluster for a given address.
	inline uint8_t QCow2Image::read_l2_table(uint64_t l2_table_offset, uint64_t address, uint64_t& data_cluster_offset){
		const uint64_t l2_entry_offset = l2_table_offset + (((address >> header.cluster_bits) & l2_mask) << 3);
		return read_table(l2_entry_offset, table_entry_mask, data_cluster_offset);
	}


//Read the refcount table to get the offset of the refcount cluster for a given address.
	inline uint8_t QCow2Image::read_refcount_table(uint64_t data_cluster_offset, uint64_t& refcount_cluster_offset){
		const uint64_t refcount_entry_offset = header.refcount_table_offset + (((data_cluster_offset/cluster_size) >> refcount_bits) << 3);
		return read_table(refcount_entry_offset, empty_mask, refcount_cluster_offset);
	}


//Read a table entry at the given offset.
	inline uint8_t QCow2Image::read_table(uint64_t entry_offset, uint64_t entry_mask, uint64_t& entry_value){
        (void)entry_mask;//UNUSED
		uint64_t buffer;
		if (0 != read_allocated_data(entry_offset, (uint8_t*)&buffer, sizeof buffer)){
			return 0x05;
		}
		entry_value = host_read64(buffer) & table_entry_mask;
		return 0;
	}


//Read a cluster not currently allocated in the image file.
	inline uint8_t QCow2Image::read_unallocated_cluster(uint64_t data_cluster_number, uint8_t* data)
	{
		if(backing_image == NULL){
			std::fill(data, data + cluster_size, 0);
			return 0;
		}
		return backing_image->read_cluster(data_cluster_number, data);
	}


//Read a sector not currently allocated in the image file.
	inline uint8_t QCow2Image::read_unallocated_sector(uint32_t sectnum, uint8_t* data){
		if(backing_image == NULL){
			std::fill(data, data+sector_size, 0);
			return 0;
		}
		return backing_image->read_sector(sectnum, data);
	}

//Update the reference count for a cluster.
	uint8_t QCow2Image::update_reference_count(uint64_t cluster_offset, uint8_t* cluster_buffer){
		uint64_t refcount_cluster_offset;
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
	uint8_t QCow2Image::write_data(uint64_t file_offset, const uint8_t* data, uint64_t data_size){
		if (0 != fseeko64(file, (off_t)file_offset, SEEK_SET)){
			return 0x05;
		}
		if (1 != fwrite(data, data_size, 1, file)){
			return 0x05;
		}
		return 0;
	}


//Write an L2 table offset into the L1 table.
	inline uint8_t QCow2Image::write_l1_table_entry(uint64_t address, uint64_t l2_table_offset){
		const uint64_t l1_entry_offset = header.l1_table_offset + ((address >> l1_bits) << 3);
		return write_table_entry(l1_entry_offset, l2_table_offset | copy_flag);
	}


//Write a data cluster offset into an L2 table.
	inline uint8_t QCow2Image::write_l2_table_entry(uint64_t l2_table_offset, uint64_t address, uint64_t data_cluster_offset){
		const uint64_t l2_entry_offset = l2_table_offset + (((address >> header.cluster_bits) & l2_mask) << 3);
		return write_table_entry(l2_entry_offset, data_cluster_offset | copy_flag);
	}


//Write a refcount.
	inline uint8_t QCow2Image::write_refcount(uint64_t cluster_offset, uint64_t refcount_cluster_offset, uint16_t refcount){
		const uint64_t refcount_offset = refcount_cluster_offset + (((cluster_offset/cluster_size) & refcount_mask) << 1);
		uint16_t buffer = host_read16(refcount);
		return write_data(refcount_offset, (uint8_t*)&buffer, sizeof buffer);
	}


//Write a refcount table entry.
	inline uint8_t QCow2Image::write_refcount_table_entry(uint64_t cluster_offset, uint64_t refcount_cluster_offset){
		const uint64_t refcount_entry_offset = header.refcount_table_offset + (((cluster_offset/cluster_size) >> refcount_bits) << 3);
		return write_table_entry(refcount_entry_offset, refcount_cluster_offset);
	}


//Write a table entry at the given offset.
	inline uint8_t QCow2Image::write_table_entry(uint64_t entry_offset, uint64_t entry_value){
		uint64_t buffer = host_read64(entry_value);
		return write_data(entry_offset, (uint8_t*)&buffer, sizeof buffer);
	}


//Public Constructor.
	QCow2Disk::QCow2Disk(QCow2Image::QCow2Header& qcow2Header, FILE *qcow2File, const char *imgName, uint32_t imgSizeK, uint32_t sectorSizeBytes, bool isHardDisk) : imageDisk(qcow2File, (const char*)imgName, imgSizeK, isHardDisk), qcowImage(qcow2Header, qcow2File, (const char*) imgName, sectorSizeBytes){
	}


//Public Destructor.
	QCow2Disk::~QCow2Disk(){
	}


//Public function to a read a sector.
	uint8_t QCow2Disk::Read_AbsoluteSector(uint32_t sectnum, void* data){
		return qcowImage.read_sector(sectnum, (uint8_t*)data);
	}


//Public function to a write a sector.
	uint8_t QCow2Disk::Write_AbsoluteSector(uint32_t sectnum,const void* data){
		return qcowImage.write_sector(sectnum, (const uint8_t*)data);
	}
