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

#include <cmath> /* for isinf, etc */
#include <cstdint>

#include "cpu/lazyflags.h"
#include "cross.h"
#include "fpu.h"

#define BIAS80 16383
#define BIAS64 1023

static inline uint16_t FPU_GetTag()
{
	uint16_t tags = 0;
	for (auto i=0; i<8; i++) {
        FPUTag tag;
        if (!fpu.regvalid[i]) {
            tag = FPUTag::Empty;
        } else if (fpu.use80[i] ? IsZero(fpu.regs_80[i]) : IsZero(fpu.regs[i])) {
            tag = FPUTag::Zero;
        } else if (fpu.use80[i] ? IsSpecial(fpu.regs_80[i]) : IsSpecial(fpu.regs[i])) {
            tag = FPUTag::Special;
        } else {
            tag = FPUTag::Valid;
        }
        tags |= static_cast<uint8_t>(tag) << (2*i);
    }
	return tags;
}

static void FPU_PREP_PUSH(void){
	TOP = (TOP - 1) &7;
	fpu.regvalid[TOP] = true;
	fpu.use80[TOP] = false; // the value given is already 64-bit precision, it's useless to emulate 80-bit precision
}

static void FPU_FNOP(void){
	return;
}

static void FPU_PUSH(double in){
	FPU_PREP_PUSH();
	fpu.regs[TOP].v = in;
	fpu.use80[TOP] = false; // the value given is already 64-bit precision, it's useless to emulate 80-bit precision
//	LOG(LOG_FPU,LOG_ERROR)("Pushed at %d  %g to the stack",newtop,in);
	return;
}
