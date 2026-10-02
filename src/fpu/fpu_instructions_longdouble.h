/*
 *  Copyright (C) 2002-2024  The DOSBox Team
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

#include <cfenv> /* for std::feholdexcept */
#include <cmath> /* for isinf, etc */

#include "cpu/lazyflags.h"
#include "fpu.h"

static inline uint16_t FPU_GetTag()
{
	uint16_t tags = 0;
	for (auto i=0; i<8; i++) {
        FPUTag tag;
        if (!fpu.regvalid[i]) {
            tag = FPUTag::Empty;
        } else if (IsZero(fpu.regs_80[i])) {
            tag = FPUTag::Zero;
        } else if (IsSpecial(fpu.regs_80[i])) {
            tag = FPUTag::Special;
        } else {
            tag = FPUTag::Valid;
        }
        tags |= static_cast<uint8_t>(tag) << (2*i);
    }
	return tags;
}

static void FPU_FNOP(void){
	return;
}

static void FPU_PUSH(long double in){
	TOP = (TOP - 1) &7;
	//actually check if empty
	fpu.regvalid[TOP] = true;
	fpu.regs_80[TOP].v = in;
	return;
}

static void FPU_PREP_PUSH(void){
	TOP = (TOP - 1) &7;
	fpu.regvalid[TOP] = false;
}

#define BIAS80 16383
#define BIAS64 1023

// WARNING: UNTESTED. Original contributed code only focused on the x86 FPU case.
static void FPU_FSTT_I32(PhysPt addr) {
	mem_writed(addr,(uint32_t)static_cast<int32_t>(fpu.regs_80[TOP].v));
	FPU_FPOP();
}

// WARNING: UNTESTED. Original contributed code only focused on the x86 FPU case.
static void FPU_FSTT_I64(PhysPt addr) {
	mem_writeq(addr,(uint64_t)static_cast<int64_t>(fpu.regs_80[TOP].v));
	FPU_FPOP();
}
