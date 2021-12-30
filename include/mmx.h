/*
 *  Copyright (C) 2002-2021  The DOSBox Team
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

#ifndef DOSBOX_MMX_H
#define DOSBOX_MMX_H

#if C_FPU

#include <stddef.h>

typedef union alignas(8) MMX_reg {

	uint64_t q;

#ifndef WORDS_BIGENDIAN
	struct {
		uint32_t d0,d1;
	} ud;
	static_assert(sizeof(ud) == 8, "MMX packing error");

	struct {
		int32_t d0,d1;
	} sd;
	static_assert(sizeof(sd) == 8, "MMX packing error");

	struct uw_t {
		uint16_t w0,w1,w2,w3;
	} uw;
	static_assert(sizeof(uw) == 8, "MMX packing error");

	uint16_t uwa[4]; /* for PSHUFW */
	static_assert(sizeof(uwa) == 8, "MMX packing error");
	static_assert(offsetof(uw_t,w0) == 0, "MMX packing error");
	static_assert(offsetof(uw_t,w1) == 2, "MMX packing error");
	static_assert(offsetof(uw_t,w2) == 4, "MMX packing error");
	static_assert(offsetof(uw_t,w3) == 6, "MMX packing error");

	struct {
		int16_t w0,w1,w2,w3;
	} sw;
	static_assert(sizeof(sw) == 8, "MMX packing error");

	struct {
		uint8_t b0,b1,b2,b3,b4,b5,b6,b7;
	} ub;
	static_assert(sizeof(ub) == 8, "MMX packing error");

	struct {
		int8_t b0,b1,b2,b3,b4,b5,b6,b7;
	} sb;
	static_assert(sizeof(sb) == 8, "MMX packing error");
#else
	struct {
		uint32_t d1,d0;
	} ud;
	static_assert(sizeof(ud) == 8, "MMX packing error");

	struct {
		int32_t d1,d0;
	} sd;
	static_assert(sizeof(sd) == 8, "MMX packing error");

	struct {
		uint16_t w3,w2,w1,w0;
	} uw;
	static_assert(sizeof(uw) == 8, "MMX packing error");

	struct {
		uint16_t w3,w2,w1,w0;
	} sw;
	static_assert(sizeof(sw) == 8, "MMX packing error");

	struct {
		uint8_t b7,b6,b5,b4,b3,b2,b1,b0;
	} ub;
	static_assert(sizeof(ub) == 8, "MMX packing error");

	struct {
		uint8_t b7,b6,b5,b4,b3,b2,b1,b0;
	} sb;
	static_assert(sizeof(sb) == 8, "MMX packing error");
#endif

};
static_assert(sizeof(MMX_reg) == 8, "MMX packing error");

extern MMX_reg * reg_mmx[8];
extern MMX_reg * lookupRMregMM[256];


int8_t  SaturateWordSToByteS(int16_t value);
int16_t SaturateDwordSToWordS(int32_t value);
uint8_t  SaturateWordSToByteU(int16_t value);
uint16_t SaturateDwordSToWordU(int32_t value);

void   setFPUTagEmpty();

#endif

#endif
