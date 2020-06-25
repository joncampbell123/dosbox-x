/*
 *  Copyright (C) 2018-2020 Jon Campbell
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

#ifndef __ISP_UTILS_V4_GUID_H
#define __ISP_UTILS_V4_GUID_H

#include <stdint.h>

#include "config.h"

#include "informational.h"

#if defined(_MSC_VER)
# pragma pack(push,1)
#endif

/* [doc] windows_GUID
 *
 * Packed portable representation of the Microsoft Windows GUID
 * structure.
 */
typedef struct {					/* (sizeof) (offset hex) (offset dec) */
	uint32_t _Little_Endian_	a;		/* (4)   +0x00 +0 */
	uint16_t _Little_Endian_	b,c;		/* (2,2) +0x04 +4 */
	uint8_t _Little_Endian_		d[2];		/* (2)   +0x08 +8 */
	uint8_t _Little_Endian_		e[6];		/* (6)   +0x0A +10 */
} GCC_ATTRIBUTE(packed) windows_GUID;			/* (16)  =0x10 =16 */
#define windows_GUID_size (16)

#if defined(_MSC_VER)
# pragma pack(pop)
#endif

unsigned char windows_IsEqualGUID(const windows_GUID *a,const windows_GUID *b);

#endif

