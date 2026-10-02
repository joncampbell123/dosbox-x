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

static void FPU_ST80(PhysPt addr,Bitu reg,FPU_Reg_80 &raw,bool use80);

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

static void FPU_FSAVE(PhysPt addr, bool op16){
	FPU_FSTENV(addr, op16);
	uint8_t start = op16 ? 14:28;
	for(uint8_t i = 0;i < 8;i++){
		FPU_ST80(addr+start,STV(i),/*&*/fpu.regs_80[STV(i)],fpu.use80[STV(i)]);
		start += 10;
	}
	FPU_FINIT();
}

static void FPU_ST80(PhysPt addr,Bitu reg,FPU_Reg_80 &raw,bool use80) {
	if (use80) {
		// we have the raw 80-bit IEEE float value. we can just store
		mem_writed(addr,(uint32_t)raw.raw.l);
		mem_writed(addr+4,(uint32_t)(raw.raw.l >> (uint64_t)32));
		mem_writew(addr+8,(uint16_t)raw.raw.h);
	}
	else {
		// convert the "double" type to 80-bit IEEE and store
		struct {
			int16_t begin;
			FPU_Reg_64 eind;
		} test;
		int64_t sign80 = ((uint64_t)fpu.regs[reg].raw&ULONGTYPE(0x8000000000000000))?1:0;
		int64_t exp80 =  fpu.regs[reg].raw&LONGTYPE(0x7ff0000000000000);
		int64_t exp80final = (exp80>>52);
		int64_t mant80 = fpu.regs[reg].raw&LONGTYPE(0x000fffffffffffff);
		int64_t mant80final = (mant80 << 11);
		if(fpu.regs[reg].v != 0){ //Zero is a special case
			// Elvira wants the 8 and tcalc doesn't
			mant80final |= (int64_t)ULONGTYPE(0x8000000000000000);
			//Ca-cyber doesn't like this when result is zero.
			exp80final += (BIAS80 - BIAS64);
		}
		test.begin = (static_cast<int16_t>(sign80)<<15)| static_cast<int16_t>(exp80final);
		test.eind.raw = static_cast<uint64_t>(mant80final);
		mem_writed(addr, static_cast<uint32_t>(test.eind.raw));
		mem_writed(addr + 4, static_cast<uint32_t>(test.eind.raw >> 32));
		mem_writew(addr+8,(uint16_t)test.begin);
	}
}

// WARNING: UNTESTED. Original contributed code only focused on the x86 FPU case.
static void FPU_FSTT_I16(PhysPt addr) {
	double val = fpu.regs[TOP].v; /* chop rounding mode */
	mem_writew(addr,(val < 32768.0 && val >= -32768.0)?static_cast<int16_t>(val):0x8000);
	FPU_FPOP();
}

// WARNING: UNTESTED. Original contributed code only focused on the x86 FPU case.
static void FPU_FSTT_I32(PhysPt addr) {
	double val = fpu.regs[TOP].v; /* chop rounding mode */
	mem_writed(addr,(val < 2147483648.0 && val >= -2147483648.0)?static_cast<int32_t>(val):0x80000000);
	FPU_FPOP();
}

// WARNING: UNTESTED. Original contributed code only focused on the x86 FPU case.
static void FPU_FSTT_I64(PhysPt addr) {
	FPU_Reg_64 blah;
	if (fpu.use80[TOP] && (fpu.regs_80[TOP].raw.h & 0x7FFFu) == (0x0000u + FPU_Reg_80_exponent_bias + 63u)) {
		// FIXME: This works so far for DOS demos that use the "Pentium memcpy trick" to copy 64 bits at a time.
		//        What this code needs to do is take the exponent into account and then clamp the 64-bit int within range.
		//        This cheap hack is good enough for now.
		mem_writed(addr,(uint32_t)(fpu.regs_80[TOP].raw.l));
		mem_writed(addr+4,(uint32_t)(fpu.regs_80[TOP].raw.l >> (uint64_t)32));
	}
	else {
		double val = fpu.regs[TOP].v; /* chop rounding mode */
		blah.raw = static_cast<uint64_t>((val < 9223372036854775808.0 &&
		                                      val >= -9223372036854775808.0)
		                                             ? static_cast<int64_t>(val)
		                                             : LONGTYPE(0x8000000000000000));

		mem_writed(addr, static_cast<uint32_t>(blah.raw));
		mem_writed(addr + 4, static_cast<uint32_t>(blah.raw >> 32));
	}
	FPU_FPOP();
}
