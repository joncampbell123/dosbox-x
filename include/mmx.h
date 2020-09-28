/*
 *  Copyright (C) 2002-2020  The DOSBox Team
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

typedef union {

	Bit64u q;

#ifndef WORDS_BIGENDIAN
	struct {
		uint32_t d0,d1;
	} ud;

	struct {
		Bit32s d0,d1;
	} sd;

	struct {
		uint16_t w0,w1,w2,w3;
	} uw;

	struct {
		int16_t w0,w1,w2,w3;
	} sw;

	struct {
		uint8_t b0,b1,b2,b3,b4,b5,b6,b7;
	} ub;

	struct {
		int8_t b0,b1,b2,b3,b4,b5,b6,b7;
	} sb;
#else
	struct {
		uint32_t d1,d0;
	} ud;

	struct {
		Bit32s d1,d0;
	} sd;

	struct {
		uint16_t w3,w2,w1,w0;
	} uw;

	struct {
		uint16_t w3,w2,w1,w0;
	} sw;

	struct {
		uint8_t b7,b6,b5,b4,b3,b2,b1,b0;
	} ub;

	struct {
		uint8_t b7,b6,b5,b4,b3,b2,b1,b0;
	} sb;
#endif

} MMX_reg;

extern MMX_reg * reg_mmx[8];
extern MMX_reg * lookupRMregMM[256];


int8_t  SaturateWordSToByteS(int16_t value);
int16_t SaturateDwordSToWordS(Bit32s value);
uint8_t  SaturateWordSToByteU(int16_t value);
uint16_t SaturateDwordSToWordU(Bit32s value);

void   setFPUTagEmpty();

#endif

#endif
