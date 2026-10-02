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
