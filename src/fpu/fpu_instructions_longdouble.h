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

static long double FPU_FLD80(PhysPt addr) {
    FPU_Reg_80 result;
    result.raw.l = mem_readq(addr);
    result.raw.h = mem_readw(addr+8ul);
	return result.v;
}

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

static void FPU_FSIN(void){
	fpu.regs_80[TOP].v = sinl(fpu.regs_80[TOP].v);
	FPU_SET_C2(0);
	//flags and such :)
	return;
}

static void FPU_FSINCOS(void){
	long double temp = fpu.regs_80[TOP].v;
	fpu.regs_80[TOP].v = sinl(temp);
	FPU_PUSH(cosl(temp));
	FPU_SET_C2(0);
	//flags and such :)
	return;
}

static void FPU_FSQRT(void){
	fenv_t buf;
	std::feholdexcept(&buf);
	fpu.regs_80[TOP].v = sqrtl(fpu.regs_80[TOP].v);
	//flags and such :)
	return;
}
static void FPU_FPATAN(void){
	fpu.regs_80[STV(1)].v = atan2l(fpu.regs_80[STV(1)].v,fpu.regs_80[TOP].v);
	FPU_FPOP();
	//flags and such :)
	return;
}
static void FPU_FPTAN(void){
	fpu.regs_80[TOP].v = tanl(fpu.regs_80[TOP].v);
	FPU_PUSH(1.0);
	FPU_SET_C2(0);
	//flags and such :)
	return;
}
static void FPU_FSUBR(Bitu st, Bitu other){
	fenv_t buf;
	std::feholdexcept(&buf);
	fpu.regs_80[st].v = fpu.regs_80[other].v - fpu.regs_80[st].v;
	//flags and such :)
	return;
}

static void FPU_FRNDINT(void){
	int64_t temp= static_cast<int64_t>(FROUND(fpu.regs_80[TOP].v));
	fpu.regs_80[TOP].v=static_cast<long double>(temp);
}

static void FPU_FPREM(void){
	long double valtop = fpu.regs_80[TOP].v;
	long double valdiv = fpu.regs_80[STV(1)].v;
	int64_t ressaved = static_cast<int64_t>( (valtop/valdiv) );
// Some backups
//	long double res=valtop - ressaved*valdiv; 
//      res= fmod(valtop,valdiv);
	fpu.regs_80[TOP].v = valtop - ressaved*valdiv;
	FPU_SET_C0(static_cast<Bitu>(ressaved&4));
	FPU_SET_C3(static_cast<Bitu>(ressaved&2));
	FPU_SET_C1(static_cast<Bitu>(ressaved&1));
	FPU_SET_C2(0);
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

static void FPU_FXAM(void){
	if(fpu.regs_80[TOP].raw.h & 0x8000u)	//sign
	{ 
		FPU_SET_C1(1);
	} 
	else 
	{
		FPU_SET_C1(0);
	}
	if(!fpu.regvalid[TOP])
	{
		FPU_SET_C3(1);FPU_SET_C2(0);FPU_SET_C0(1);
		return;
	}
	if(fpu.regs_80[TOP].v == 0.0l)		//zero or normalized number.
	{ 
		FPU_SET_C3(1);FPU_SET_C2(0);FPU_SET_C0(0);
	}
	else
	{
		FPU_SET_C3(0);FPU_SET_C2(1);FPU_SET_C0(0);
	}
}


static void FPU_F2XM1(void){
	fpu.regs_80[TOP].v = powl(2.0l,fpu.regs_80[TOP].v) - 1;
	return;
}

static void FPU_FYL2X(void){
	fpu.regs_80[STV(1)].v *= logl(fpu.regs_80[TOP].v)/logl(static_cast<long double>(2.0));
	FPU_FPOP();
	return;
}

static void FPU_FYL2XP1(void){
	fpu.regs_80[STV(1)].v *= logl(fpu.regs_80[TOP].v+1.0l)/logl(static_cast<long double>(2.0));
	FPU_FPOP();
	return;
}

static void FPU_FSCALE(void){
	fpu.regs_80[TOP].v *= powl(2.0,static_cast<long double>(static_cast<int64_t>(fpu.regs_80[STV(1)].v)));
	return; //2^x where x is chopped.
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


static INLINE void FPU_FSUBR_EA(Bitu op1){
	FPU_FSUBR(op1,8);
}
