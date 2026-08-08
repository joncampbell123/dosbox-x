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

#ifndef DOSBOX_IMAGE_DISK_TELEDISK_H
#define DOSBOX_IMAGE_DISK_TELEDISK_H

#include "bios_disk.h"

/* TeleDisk sector flags (sector header, spec section 6.5). Values are OR-combined. */
enum class td0_sector_flags : uint8_t {
	none          = 0x00,
	duplicate     = 0x01, /* sector duplicated within a track */
	crc_error     = 0x02, /* sector was read with a CRC error */
	deleted_mark  = 0x04, /* deleted-data address mark */
	dos_skipped   = 0x10, /* skipped via DOS allocation; NO data block follows */
	id_no_data    = 0x20, /* had ID field but no data; NO data block follows */
	data_no_id    = 0x40  /* had data but no ID field (bogus header) */
};

static inline bool operator&(td0_sector_flags lhs, td0_sector_flags rhs) {
	return (static_cast<uint8_t>(lhs) & static_cast<uint8_t>(rhs)) != 0;
}

#pragma pack(push,1)
/* On-disk TeleDisk (.TD0) structures. All multi-byte fields are little-endian;
 * read them through the td0_read_u16le() helper rather than dereferencing directly. */
struct td0_image_header {
	uint8_t  signature[2];    /* "TD" normal, "td" advanced compression */
	uint8_t  sequence;        /* 0 for first/only volume */
	uint8_t  check_sequence;
	uint8_t  version;         /* high.low nibble, e.g. 0x15 == 1.5 */
	uint8_t  data_rate;
	uint8_t  drive_type;
	uint8_t  stepping;        /* bit 7 set => optional comment block present */
	uint8_t  dos_alloc_flag;
	uint8_t  sides;           /* 1 == one side, otherwise two */
	uint8_t  crc[2];
};

struct td0_comment_header {
	uint8_t  crc[2];
	uint8_t  length[2];       /* size of comment data block that follows */
	uint8_t  year;            /* years since 1900 */
	uint8_t  month;           /* 0 == January */
	uint8_t  day;
	uint8_t  hour;
	uint8_t  minute;
	uint8_t  second;
};

struct td0_track_header {
	uint8_t  num_sectors;     /* 0xFF marks end of image */
	uint8_t  cylinder;        /* physical cylinder */
	uint8_t  head;            /* bit 0 == side; bit 7 == single-density */
	uint8_t  crc;
};

struct td0_sector_header {
	uint8_t          cylinder;        /* logical cylinder in the sector ID field */
	uint8_t          head;            /* logical side/head */
	uint8_t          sector;          /* logical sector number */
	uint8_t          size_code;       /* sector size = 128 << size_code */
	td0_sector_flags flags;
	uint8_t          crc;
};

struct td0_data_header {
	uint8_t  block_size[2];   /* size of data block including the method byte */
	uint8_t  method;          /* sector_encoding_method */
};
#pragma pack(pop)

static_assert(sizeof(td0_image_header)   == 12, "td0_image_header must match on-disk layout");
static_assert(sizeof(td0_comment_header) == 10, "td0_comment_header must match on-disk layout");
static_assert(sizeof(td0_track_header)   ==  4, "td0_track_header must match on-disk layout");
static_assert(sizeof(td0_sector_header)  ==  6, "td0_sector_header must match on-disk layout");
static_assert(sizeof(td0_data_header)    ==  3, "td0_data_header must match on-disk layout");

class imageDiskTeledisk : public imageDisk {
public:
	enum class sector_encoding_method : uint8_t {
		raw                    = 0,
		repeated_2byte_pattern = 1,
		rle                    = 2
	};

	uint8_t Read_Sector(uint32_t head,uint32_t cylinder,uint32_t sector,void * data,unsigned int req_sector_size=0) override;
	uint8_t Write_Sector(uint32_t head,uint32_t cylinder,uint32_t sector,const void * data,unsigned int req_sector_size=0) override;
	uint8_t Read_AbsoluteSector(uint32_t sectnum, void * data) override;
	uint8_t Write_AbsoluteSector(uint32_t sectnum, const void * data) override;

	imageDiskTeledisk(FILE *imgFile, const char *imgName, uint32_t imgSizeK, bool isHardDisk);
	virtual ~imageDiskTeledisk();

	struct td0entry {
		uint8_t phys_track = 0, phys_head = 0, phys_sector = 0;
		uint8_t logical_track = 0, logical_head = 0;
		uint16_t sector_size = 0;
		td0_sector_flags flags = td0_sector_flags::none;
		bool has_data = false;
		std::vector<uint8_t> data;

		td0entry() {}
		uint16_t getSectorSize(void) const { return sector_size; }
	};

	td0entry *findSector(uint8_t head,uint8_t track,uint8_t sector,unsigned int req_sector_size=0);
	const td0entry *findSector(uint8_t head,uint8_t track,uint8_t sector,unsigned int req_sector_size=0) const;

	std::vector<td0entry> dents;
};

#endif
