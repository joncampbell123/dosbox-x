/*
 *  Copyright (C) 2026 The DOSBox-X Team
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

#include <cstdint>
#include <cstring>
#include <vector>

#include "dosbox.h"
#include "bios_disk.h"
#include "imagedisk_teledisk.h"

static inline uint16_t td0_read_u16le(const uint8_t *p) {
	return (uint16_t)p[0] | ((uint16_t)p[1] << 8U);
}

static bool td0_decode_method_raw(const std::vector<uint8_t> &encoded, const uint16_t sector_size, std::vector<uint8_t> &decoded) {
	if (encoded.size() != sector_size)
		return false;
	decoded = encoded;
	return true;
}

static bool td0_decode_method_repeated_2byte_pattern(const std::vector<uint8_t> &encoded, const uint16_t sector_size, std::vector<uint8_t> &decoded) {
	decoded.clear();
	decoded.reserve(sector_size);

	size_t pos = 0;
	while (decoded.size() < sector_size) {
		if ((pos + 4U) > encoded.size())
			return false;

		const uint16_t count = td0_read_u16le(&encoded[pos]); pos += 2;
		const uint8_t b0 = encoded[pos++];
		const uint8_t b1 = encoded[pos++];

		for (uint16_t i = 0; i < count && decoded.size() < sector_size; i++) {
			decoded.push_back(b0);
			if (decoded.size() >= sector_size)
				break;
			decoded.push_back(b1);
		}
	}

	return decoded.size() == sector_size;
}

static bool td0_decode_method_rle(const std::vector<uint8_t> &encoded, const uint16_t sector_size, std::vector<uint8_t> &decoded) {
	decoded.clear();
	decoded.reserve(sector_size);

	size_t pos = 0;
	while (decoded.size() < sector_size) {
		if (pos >= encoded.size())
			return false;

		const uint8_t token = encoded[pos++];
		if (token == 0) {
			if (pos >= encoded.size())
				return false;

			const uint8_t n = encoded[pos++];
			if ((pos + n) > encoded.size())
				return false;

			for (uint8_t i = 0; i < n && decoded.size() < sector_size; i++)
				decoded.push_back(encoded[pos++]);
		}
		else {
			const size_t block_len = (size_t)token * 2U;
			if (pos >= encoded.size())
				return false;
			const uint8_t repeat_count = encoded[pos++];
			if ((pos + block_len) > encoded.size())
				return false;

			std::vector<uint8_t> block(block_len);
			memcpy(&block[0], &encoded[pos], block_len);
			pos += block_len;

			for (uint8_t r = 0; r < repeat_count && decoded.size() < sector_size; r++) {
				for (size_t i = 0; i < block_len && decoded.size() < sector_size; i++)
					decoded.push_back(block[i]);
			}
		}
	}

	return decoded.size() == sector_size;
}

static bool td0_decode_sector_data(imageDiskTeledisk::sector_encoding_method method, const std::vector<uint8_t> &encoded, const uint16_t sector_size, std::vector<uint8_t> &decoded) {
	switch (method) {
	case imageDiskTeledisk::sector_encoding_method::raw:
		return td0_decode_method_raw(encoded, sector_size, decoded);
	case imageDiskTeledisk::sector_encoding_method::repeated_2byte_pattern:
		return td0_decode_method_repeated_2byte_pattern(encoded, sector_size, decoded);
	case imageDiskTeledisk::sector_encoding_method::rle:
		return td0_decode_method_rle(encoded, sector_size, decoded);
	default:
		return false;
	}
}

imageDiskTeledisk::td0entry *imageDiskTeledisk::findSector(uint8_t head,uint8_t track,uint8_t sector,unsigned int req_sector_size) {
	/* Delegate to the const overload; safe to cast away const because this object is non-const here. */
	const imageDiskTeledisk *self = this;
	return const_cast<td0entry *>(self->findSector(head, track, sector, req_sector_size));
}

const imageDiskTeledisk::td0entry *imageDiskTeledisk::findSector(uint8_t head,uint8_t track,uint8_t sector,unsigned int req_sector_size) const {
	const td0entry *best = NULL;

	for (const auto &entry : dents) {
		if (entry.phys_head != head || entry.phys_track != track || entry.phys_sector != sector)
			continue;
		if (req_sector_size != ~0U && req_sector_size != 0 && entry.sector_size != req_sector_size)
			continue;
		if (best == NULL || (!best->has_data && entry.has_data))
			best = &entry;
	}

	return best;
}

uint8_t imageDiskTeledisk::Read_Sector(uint32_t head,uint32_t cylinder,uint32_t sector,void * data,unsigned int req_sector_size) {
	const td0entry *ent;

	ent = findSector((uint8_t)head, (uint8_t)cylinder, (uint8_t)sector, req_sector_size);

	if (ent == NULL || !ent->has_data)
		return 0x05;

	if (req_sector_size == 0) req_sector_size = ent->getSectorSize();

	if (ent->getSectorSize() != req_sector_size)
		return 0x05;

	memcpy(data, ent->data.data(), req_sector_size);
	return 0;
}

uint8_t imageDiskTeledisk::Write_Sector(uint32_t head,uint32_t cylinder,uint32_t sector,const void * data,unsigned int req_sector_size) {
	(void)head; (void)cylinder; (void)sector; (void)data; (void)req_sector_size;
	/* TeleDisk images are read-only: writes cannot be re-encoded back into the .td0 archive,
	 * so accepting a write in memory only would silently discard the guest's data on remount.
	 * Report the disk as write-protected instead. */
	return 0x03;
}

uint8_t imageDiskTeledisk::Read_AbsoluteSector(uint32_t sectnum, void * data) {
	unsigned int c,h,s;

	if (sectors == 0 || heads == 0)
		return 0x05;

	s = (sectnum % sectors) + 1;
	h = (sectnum / sectors) % heads;
	c = (sectnum / sectors / heads);
	return Read_Sector(h,c,s,data);
}

uint8_t imageDiskTeledisk::Write_AbsoluteSector(uint32_t sectnum,const void *data) {
	unsigned int c,h,s;

	if (sectors == 0 || heads == 0)
		return 0x05;

	s = (sectnum % sectors) + 1;
	h = (sectnum / sectors) % heads;
	c = (sectnum / sectors / heads);
	return Write_Sector(h,c,s,data);
}

imageDiskTeledisk::imageDiskTeledisk(FILE *imgFile, const char *imgName, uint32_t imgSizeK, bool isHardDisk) : imageDisk(ID_TELEDISK) {
	(void)isHardDisk;//UNUSED
	(void)imgSizeK;//UNUSED
	td0_image_header header;

	heads = 1;
	cylinders = 0;
	image_base = 0;
	sectors = 0;
	active = false;
	sector_size = 0;
	reserved_cylinders = 0;
	diskSizeK = 0;
	diskimg = imgFile;

	if (imgName != NULL)
		diskname = imgName;

	if (diskimg == NULL)
		return;

	fseek(diskimg,0,SEEK_SET);
	if (fread(&header,1,sizeof(header),diskimg) != sizeof(header))
		return;

	/* Only support "TD" (normal compression), not "td" advanced compression */
	if (header.signature[0] != 'T' || header.signature[1] != 'D')
		return;

	/* Optional comment block, flagged by the high bit of the stepping field */
	if (header.stepping & 0x80U) {
		td0_comment_header comment;
		if (fread(&comment,1,sizeof(comment),diskimg) != sizeof(comment))
			return;

		const uint16_t comment_len = td0_read_u16le(comment.length);
		if (fseek(diskimg, (long)comment_len, SEEK_CUR) != 0)
			return;
	}

	bool parse_ok = false;
	for (;;) {
		td0_track_header track_header;
		if (fread(&track_header,1,sizeof(track_header),diskimg) != sizeof(track_header))
			return;

		const uint8_t num_sectors = track_header.num_sectors;
		if (num_sectors == 0xFFU) {
			parse_ok = true;
			break;
		}

		const uint8_t phys_track = track_header.cylinder;
		const uint8_t phys_head = track_header.head & 1U;

		for (uint8_t i = 0; i < num_sectors; i++) {
			td0_sector_header sector_header;
			if (fread(&sector_header,1,sizeof(sector_header),diskimg) != sizeof(sector_header))
				return;

			td0entry ent;
			const uint8_t size_code = (sector_header.size_code > 6U) ? 0U : sector_header.size_code;

			ent.phys_track = phys_track;
			ent.phys_head = phys_head;
			ent.phys_sector = sector_header.sector;
			ent.logical_track = sector_header.cylinder;
			ent.logical_head = sector_header.head;
			ent.sector_size = (uint16_t)(128U << size_code);
			ent.flags = sector_header.flags;

			if ((ent.flags & td0_sector_flags::id_no_data) || (sector_header.size_code & 0xF8U)) {
				ent.has_data = false;
				ent.data.assign(ent.sector_size, 0);
			}
			else if (ent.flags & td0_sector_flags::dos_skipped) {
				ent.has_data = true;
				ent.data.assign(ent.sector_size, 0);
			}
			else {
				td0_data_header data_header;
				const long data_header_offset = ftell(diskimg);
				if (fread(&data_header,1,sizeof(data_header),diskimg) != sizeof(data_header))
					return;

				const uint16_t data_block_size = td0_read_u16le(data_header.block_size);
				if (data_block_size < 1U)
					return;

				const uint16_t encoded_size = (uint16_t)(data_block_size - 1U);
				std::vector<uint8_t> encoded(encoded_size);
				if (encoded_size != 0 && fread(&encoded[0],1,encoded_size,diskimg) != encoded_size)
					return;

				const imageDiskTeledisk::sector_encoding_method method =
					static_cast<imageDiskTeledisk::sector_encoding_method>(data_header.method);

				ent.has_data = td0_decode_sector_data(method, encoded, ent.sector_size, ent.data);
				if (!ent.has_data) {
					LOG_MSG("TD0: failed to decode sector C/H/S=%u/%u/%u method=%u file_ofs=%ld",
						(unsigned int)ent.phys_track,
						(unsigned int)ent.phys_head,
						(unsigned int)ent.phys_sector,
						(unsigned int)method,
						data_header_offset);
					ent.data.assign(ent.sector_size, 0);
				}
			}

			dents.push_back(ent);
			diskSizeK += ent.sector_size;
		}
	}

	diskSizeK /= 1024;

	if (parse_ok && !dents.empty()) {
		bool founddisk = false;
		const td0entry *ent;

		ent = findSector(0,0,1,~0U);
		if (ent != NULL && ent->getSectorSize() <= 1024)
			sector_size = ent->getSectorSize();

		if (sector_size != 0 && sector_size < 512) {
			ent = findSector(0,1,1,~0U);
			if (ent != NULL && ent->getSectorSize() <= 1024) {
				const unsigned int nsz = ent->getSectorSize();
				if (sector_size < nsz)
					sector_size = (uint16_t)nsz;
			}
		}

		if (sector_size != 0) {
			unsigned int i = 0;
			while (DiskGeometryList[i].ksize != 0) {
				const diskGeo &diskent = DiskGeometryList[i];

				if (diskent.bytespersect == sector_size) {
					ent = findSector(0,0,(uint8_t)diskent.secttrack,sector_size);
					if (ent != NULL && sectors < diskent.secttrack)
						sectors = diskent.secttrack;
				}

				i++;
			}
		}

		if (sector_size != 0 && sectors != 0) {
			unsigned int i = 0;
			while (DiskGeometryList[i].ksize != 0) {
				const diskGeo &diskent = DiskGeometryList[i];

				if (diskent.bytespersect == sector_size && diskent.secttrack >= sectors) {
					ent = findSector(0,(uint8_t)(diskent.cylcount-1),(uint8_t)sectors,sector_size);
					if (ent != NULL && cylinders < diskent.cylcount)
						cylinders = diskent.cylcount;
				}

				i++;
			}
		}

		if (sector_size != 0 && sectors != 0 && cylinders != 0) {
			ent = findSector(1,0,(uint8_t)sectors,sector_size);
			if (ent != NULL)
				heads = 2;
		}

		if (sector_size != 0 && sectors != 0 && cylinders != 0 && heads != 0)
			founddisk = true;

		/* Fallback for nonstandard geometries */
		if (!founddisk) {
			uint16_t max_sector = 0, max_cyl = 0, max_head = 0, max_ss = 0;
			for (const auto &entry : dents) {
				if (entry.sector_size > max_ss)
					max_ss = entry.sector_size;
				if (entry.phys_sector > max_sector)
					max_sector = entry.phys_sector;
				if (entry.phys_track > max_cyl)
					max_cyl = entry.phys_track;
				if (entry.phys_head > max_head)
					max_head = entry.phys_head;
			}

			if (max_sector != 0 && max_ss != 0) {
				sector_size = max_ss;
				sectors = max_sector;
				cylinders = max_cyl + 1;
				heads = max_head + 1;
				founddisk = (sectors != 0 && cylinders != 0 && heads != 0);
			}
		}

		LOG_MSG("TD0 geometry detection: C/H/S %u/%u/%u %u bytes/sector entries=%u",
			cylinders, heads, sectors, sector_size, (unsigned int)dents.size());

		/* active stays false (as initialized) unless we resolved a usable geometry */
		if (founddisk) {
			active = true;
			incrementFDD();
			updateFloppyDPT();
		}
	}
}

imageDiskTeledisk::~imageDiskTeledisk() {
	if(diskimg != NULL) {
		fclose(diskimg);
		diskimg=NULL;
	}
}
