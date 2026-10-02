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

static long double FROUND(long double in){
	switch(fpu.cw.RC){
	case FPUControlWord::RoundMode::Nearest:
		if (in-floorl(in)>0.5) return (floorl(in)+1);
		else if (in-floorl(in)<0.5) return (floorl(in));
		else return (((static_cast<int64_t>(floorl(in)))&1)!=0)?(floorl(in)+1):(floorl(in));
		break;
	case FPUControlWord::RoundMode::Down:
		return (floorl(in));
		break;
	case FPUControlWord::RoundMode::Up:
		return (ceill(in));
		break;
	case FPUControlWord::RoundMode::Chop:
		return in; //the cast afterwards will do it right maybe cast here
		break;
	default:
		return in;
		break;
	}
}

#define BIAS80 16383
#define BIAS64 1023

static void FPU_ST80(PhysPt addr,Bitu reg) {
    mem_writeq(addr    ,fpu.regs_80[reg].raw.l);
    mem_writew(addr+8ul,fpu.regs_80[reg].raw.h);
}

// WARNING: UNTESTED. Original contributed code only focused on the x86 FPU case.
static void FPU_FSTT_I16(PhysPt addr) {
	mem_writew(addr,(uint16_t)static_cast<int16_t>(fpu.regs_80[TOP].v));
	FPU_FPOP();
}

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

static void FPU_FRNDINT(void){
	int64_t temp= static_cast<int64_t>(FROUND(fpu.regs_80[TOP].v));
	fpu.regs_80[TOP].v=static_cast<long double>(temp);
}

static void FPU_FPREM1(void){
	long double valtop = fpu.regs_80[TOP].v;
	long double valdiv = fpu.regs_80[STV(1)].v;
	long double quot = valtop/valdiv;
	long double quotf = floorl(quot);
	int64_t ressaved;
	if (quot-quotf>0.5) ressaved = static_cast<int64_t>(quotf+1);
	else if (quot-quotf<0.5) ressaved = static_cast<int64_t>(quotf);
	else ressaved = static_cast<int64_t>((((static_cast<int64_t>(quotf))&1)!=0)?(quotf+1):(quotf));
	fpu.regs_80[TOP].v = valtop - ressaved*valdiv;
	FPU_SET_C0(static_cast<Bitu>(ressaved&4));
	FPU_SET_C3(static_cast<Bitu>(ressaved&2));
	FPU_SET_C1(static_cast<Bitu>(ressaved&1));
	FPU_SET_C2(0);
}

static void FPU_FSAVE(PhysPt addr, bool op16){
	FPU_FSTENV(addr, op16);
	Bitu start = op16 ? 14:28;
	for(Bitu i = 0;i < 8;i++){
		FPU_ST80(addr+start,STV(i));
		start += 10;
	}
	FPU_FINIT();
}

static void FPU_FXTRACT(void) {
	// function stores real bias in st and 
	// pushes the significant number onto the stack
	// if double ever uses a different base please correct this function

	FPU_Reg_80 test = fpu.regs_80[TOP];
	int64_t exp80 = test.raw.h & 0x7FFFu;
	int64_t exp80final = exp80 - FPU_Reg_80_exponent_bias;
	long double mant = test.v / (powl(2.0,static_cast<long double>(exp80final)));
	fpu.regs_80[TOP].v = static_cast<long double>(exp80final);
	FPU_PUSH(mant);
}
