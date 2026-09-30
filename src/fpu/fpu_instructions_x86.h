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

#include "cpu.h"
#include "cpu/lazyflags.h"
#include "fpu.h"
#include "mem.h"
#include "regs.h"

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
}

static void FPU_PREP_PUSH(void){
	TOP = (TOP - 1) &7;
	fpu.regvalid[TOP] = true;
}

static void FPU_FSTT_I16(PhysPt addr) {
    FPUD_STORE(fisttp, WORD, s)
    mem_writew(addr, (uint16_t)fpu.p_regs[8].m1);
}

static void FPU_FSTT_I32(PhysPt addr) {
    FPUD_STORE(fisttp, DWORD, l)
    mem_writed(addr, fpu.p_regs[8].m1);
}

static void FPU_FSTT_I64(PhysPt addr) {
    FPUD_STORE(fisttp, QWORD, q)
    mem_writed(addr, fpu.p_regs[8].m1);
    mem_writed(addr + 4, fpu.p_regs[8].m2);
}


static void FPU_FSIN(void){
	FPUD_TRIG(fsin)
}

static void FPU_FSINCOS(void){
	FPUD_SINCOS()
}

static void FPU_FSQRT(void){
	FPUD_ARITH2(fsqrt)
}

static void FPU_FPATAN(void){
	FPUD_WITH_POP(fpatan)
}

static void FPU_FPTAN(void){
	FPUD_PTAN()
}


static void FPU_FRNDINT(void){
	FPUD_ARITH2(frndint)
}

static void FPU_FPREM(void){
	FPUD_REMAINDER(fprem)
}

static void FPU_FPREM1(void){
	FPUD_REMAINDER(fprem1)
}

static void FPU_FXAM(void){
	FPUD_EXAMINE(fxam)
	// handle empty registers (C1 set to sign in any way!)
	if(!fpu.regvalid[TOP]) {
		FPU_SET_C3(1);FPU_SET_C2(0);FPU_SET_C0(1);
		return;
	}
}

static void FPU_F2XM1(void){
	FPUD_TRIG(f2xm1)
}

static void FPU_FYL2X(void){
	FPUD_FYL2X(fyl2x)
}

static void FPU_FYL2XP1(void){
	FPUD_WITH_POP(fyl2xp1)
}

static void FPU_FSCALE(void){
	FPUD_REMAINDER(fscale)
}


static void FPU_FSTENV(PhysPt addr, bool op16){
	if (op16) {
		mem_writew(addr+0,static_cast<uint16_t>(fpu.cw));
		mem_writew(addr+2,static_cast<uint16_t>(fpu.sw));
		mem_writew(addr+4,static_cast<uint16_t>(FPU_GetTag()));
	} else { 
		mem_writed(addr+0,static_cast<uint32_t>(fpu.cw));
		mem_writed(addr+4,static_cast<uint32_t>(fpu.sw));
		mem_writed(addr+8,static_cast<uint32_t>(FPU_GetTag()));
	}
	// FNSTENV masks all floating-point exceptions after saving the environment.
	fpu.cw = fpu.cw.allMasked();
}

static void FPU_FSAVE(PhysPt addr, bool op16){
	FPU_FSTENV(addr, op16);
	PhysPt start = op16 ? 14:28;
	for(unsigned i=0;i<8;i++){
		mem_writed(addr+start,fpu.p_regs[STV(i)].m1);
		mem_writed(addr+start+4,fpu.p_regs[STV(i)].m2);
		mem_writew(addr+start+8,fpu.p_regs[STV(i)].m3);
		start+=10;
	}
	FPU_FINIT();
}


static void FPU_FXTRACT(void) {
	FPUD_XTRACT
}
