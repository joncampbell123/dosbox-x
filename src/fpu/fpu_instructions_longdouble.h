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

#include <math.h> /* for isinf, etc */
#include "cpu/lazyflags.h"

#ifdef __GNUC__
# if defined(__MINGW32__) || defined(MACOSX)
#  include "fpu_control_x86.h"
# else
#  include <fpu_control.h>
# endif
static inline void FPU_SyncCW(void) {
    _FPU_SETCW(fpu.cw);
}
#else
static inline void FPU_SyncCW(void) {
    /* nothing */
}
#endif

static void FPU_FINIT(void) {
	FPU_SetCW(0x37F);
    FPU_SyncCW();
    fpu.sw = 0;
	TOP=FPU_GET_TOP();
	fpu.tags[0] = TAG_Empty;
	fpu.tags[1] = TAG_Empty;
	fpu.tags[2] = TAG_Empty;
	fpu.tags[3] = TAG_Empty;
	fpu.tags[4] = TAG_Empty;
	fpu.tags[5] = TAG_Empty;
	fpu.tags[6] = TAG_Empty;
	fpu.tags[7] = TAG_Empty;
	fpu.tags[8] = TAG_Valid; // is only used by us (FIXME: why?)
}

static void FPU_FCLEX(void){
	fpu.sw &= 0x7f00;			//should clear exceptions
}

static void FPU_FNOP(void){
	return;
}

static void FPU_PUSH(long double in){
	TOP = (TOP - 1) &7;
	//actually check if empty
	fpu.tags[TOP] = TAG_Valid;
	fpu.regs_80[TOP].v = in;
//	LOG(LOG_FPU,LOG_ERROR)("Pushed at %d  %g to the stack",newtop,in);
	return;
}

static void FPU_PREP_PUSH(void){
	TOP = (TOP - 1) &7;
	fpu.tags[TOP] = TAG_Valid;
}

static void FPU_FPOP(void){
	fpu.tags[TOP]=TAG_Empty;
	//maybe set zero in it as well
	TOP = ((TOP+1)&7);
//	LOG(LOG_FPU,LOG_ERROR)("popped from %d  %g off the stack",top,fpu.regs[top].d);
	return;
}

static long double FROUND(long double in){
	switch(fpu.round){
	case ROUND_Nearest:	
		if (in-floorl(in)>0.5) return (floorl(in)+1);
		else if (in-floorl(in)<0.5) return (floorl(in));
		else return (((static_cast<Bit64s>(floorl(in)))&1)!=0)?(floorl(in)+1):(floorl(in));
		break;
	case ROUND_Down:
		return (floorl(in));
		break;
	case ROUND_Up:
		return (ceill(in));
		break;
	case ROUND_Chop:
		return in; //the cast afterwards will do it right maybe cast here
		break;
	default:
		return in;
		break;
	}
}

// TODO: Incorporate into paging.h
static inline uint64_t mem_readq(PhysPt addr) {
    uint64_t tmp;

    tmp  = (uint64_t)mem_readd(addr);
    tmp |= (uint64_t)mem_readd(addr+4ul) << (uint64_t)32ul;

    return tmp;
}

static inline void mem_writeq(PhysPt addr,uint64_t v) {
    mem_writed(addr,    (uint32_t)v);
    mem_writed(addr+4ul,(uint32_t)(v >> (uint64_t)32ul));
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


static void FPU_FLD_F32(PhysPt addr,Bitu store_to) {
    FPU_Reg_32 result;
    result.raw = mem_readd(addr);
	fpu.regs_80[store_to].v = static_cast<long double>(result.v);
}

static void FPU_FLD_F64(PhysPt addr,Bitu store_to) {
    FPU_Reg_64 result;
    result.raw = mem_readq(addr);
	fpu.regs_80[store_to].v = static_cast<long double>(result.v);
}

static void FPU_FLD_F80(PhysPt addr) {
	fpu.regs_80[TOP].v = FPU_FLD80(addr);
}

static void FPU_FLD_I16(PhysPt addr,Bitu store_to) {
	int16_t blah = (int16_t)mem_readw(addr);
	fpu.regs_80[store_to].v = static_cast<long double>(blah);
}

static void FPU_FLD_I32(PhysPt addr,Bitu store_to) {
	int32_t blah = (int32_t)mem_readd(addr);
	fpu.regs_80[store_to].v = static_cast<long double>(blah);
}

static void FPU_FLD_I64(PhysPt addr,Bitu store_to) {
    Bit64s blah = (Bit64s)mem_readq(addr);
	fpu.regs_80[store_to].v = static_cast<long double>(blah);
}

static void FPU_FBLD(PhysPt addr,Bitu store_to) {
	uint64_t val = 0;
	Bitu in = 0;
	uint64_t base = 1;
	for(Bitu i = 0;i < 9;i++){
		in = mem_readb(addr + i);
		val += ( (in&0xf) * base); //in&0xf shouldn't be higher then 9
		base *= 10;
		val += ((( in>>4)&0xf) * base);
		base *= 10;
	}

	//last number, only now convert to float in order to get
	//the best signification
	long double temp = static_cast<long double>(val);
	in = mem_readb(addr + 9);
	temp += ( (in&0xf) * base );
	if(in&0x80) temp *= -1.0l;
	fpu.regs_80[store_to].v = temp;
}


static INLINE void FPU_FLD_F32_EA(PhysPt addr) {
	FPU_FLD_F32(addr,8);
}
static INLINE void FPU_FLD_F64_EA(PhysPt addr) {
	FPU_FLD_F64(addr,8);
}
static INLINE void FPU_FLD_I32_EA(PhysPt addr) {
	FPU_FLD_I32(addr,8);
}
static INLINE void FPU_FLD_I16_EA(PhysPt addr) {
	FPU_FLD_I16(addr,8);
}


static void FPU_FST_F32(PhysPt addr) {
    FPU_Reg_32 result;
    result.v = static_cast<float>(fpu.regs_80[TOP].v);
	mem_writed(addr,result.raw);
}

static void FPU_FST_F64(PhysPt addr) {
    FPU_Reg_64 result;
    result.v = static_cast<double>(fpu.regs_80[TOP].v);
	mem_writeq(addr,result.raw);
}

static void FPU_FST_F80(PhysPt addr) {
	FPU_ST80(addr,TOP);
}

static void FPU_FST_I16(PhysPt addr) {
	mem_writew(addr,(uint16_t)static_cast<int16_t>(FROUND(fpu.regs_80[TOP].v)));
}

static void FPU_FST_I32(PhysPt addr) {
	mem_writed(addr,(uint32_t)static_cast<int32_t>(FROUND(fpu.regs_80[TOP].v)));
}

static void FPU_FST_I64(PhysPt addr) {
	mem_writeq(addr,(uint64_t)static_cast<Bit64s>(FROUND(fpu.regs_80[TOP].v)));
}

static void FPU_FBST(PhysPt addr) {
	FPU_Reg_80 val = fpu.regs_80[TOP];
	bool sign = false;
	if(fpu.regs_80[TOP].raw.h & 0x8000u) { //sign
		sign=true;
		val.v=-val.v;
	}
	//numbers from back to front
	long double temp=val.v;
	Bitu p;
	for(Bitu i=0;i<9;i++){
		val.v=temp;
		temp = static_cast<long double>(static_cast<Bit64s>(floor(val.v/10.0l)));
		p = static_cast<Bitu>(val.v - 10.0l*temp);  
		val.v=temp;
		temp = static_cast<long double>(static_cast<Bit64s>(floor(val.v/10.0l)));
		p |= (static_cast<Bitu>(val.v - 10.0l*temp)<<4);

		mem_writeb(addr+i,p);
	}
	val.v=temp;
	temp = static_cast<long double>(static_cast<Bit64s>(floor(val.v/10.0)));
	p = static_cast<Bitu>(val.v - 10.0l*temp);
	if(sign)
		p|=0x80;
	mem_writeb(addr+9,p);
}

#if defined(WIN32) && defined(_MSC_VER) && (_MSC_VER < 1910)
/* std::isinf is C99 standard how could you NOT have this VS2008??? */
# include <math.h>
/* the purpose of this macro is to test for -/+inf. NaN is not inf. If finite or NaN it's not infinity */
# define isinf(x) (!(_finite(x) || _isnan(x)))
# define isdenormal(x) (_fpclass(x) == _FPCLASS_ND || _fpclass(x) == _FPCLASS_PD)
#else
# include <math.h>
# include <cmath>
# define isdenormal(x) (!std::isnormal(x))
#endif

static void FPU_FADD(Bitu op1, Bitu op2){
    FPU_SyncCW();
	// HACK: Set the denormal flag according to whether the source or final result is a denormalized number.
	//       This is vital if we don't want certain DOS programs to mis-detect our FPU emulation as an IIT clone chip when cputype == 286
	bool was_not_normal = isdenormal(fpu.regs_80[op1].v);
	fpu.regs_80[op1].v+=fpu.regs_80[op2].v;
	FPU_SET_D(was_not_normal || isdenormal(fpu.regs_80[op1].v) || isdenormal(fpu.regs_80[op2].v));
	//flags and such :)
	return;
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

static void FPU_FCOS(void){
	fpu.regs_80[TOP].v = cosl(fpu.regs_80[TOP].v);
	FPU_SET_C2(0);
	//flags and such :)
	return;
}

static void FPU_FSQRT(void){
    FPU_SyncCW();
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
static void FPU_FDIV(Bitu st, Bitu other){
    FPU_SyncCW();
	fpu.regs_80[st].v = fpu.regs_80[st].v/fpu.regs_80[other].v;
	//flags and such :)
	return;
}

static void FPU_FDIVR(Bitu st, Bitu other){
    FPU_SyncCW();
	fpu.regs_80[st].v = fpu.regs_80[other].v/fpu.regs_80[st].v;
	// flags and such :)
	return;
}

static void FPU_FMUL(Bitu st, Bitu other){
    FPU_SyncCW();
	fpu.regs_80[st].v *= fpu.regs_80[other].v;
	//flags and such :)
	return;
}

static void FPU_FSUB(Bitu st, Bitu other){
    FPU_SyncCW();
	fpu.regs_80[st].v = fpu.regs_80[st].v - fpu.regs_80[other].v;
	//flags and such :)
	return;
}

static void FPU_FSUBR(Bitu st, Bitu other){
    FPU_SyncCW();
	fpu.regs_80[st].v = fpu.regs_80[other].v - fpu.regs_80[st].v;
	//flags and such :)
	return;
}

static void FPU_FXCH(Bitu st, Bitu other){
	FPU_Reg_80 reg80 = fpu.regs_80[other];
	FPU_Tag tag = fpu.tags[other];

	fpu.regs_80[other] = fpu.regs_80[st];
	fpu.tags[other] = fpu.tags[st];

	fpu.regs_80[st] = reg80;
	fpu.tags[st] = tag;
}

static void FPU_FST(Bitu st, Bitu other){
	fpu.regs_80[other] = fpu.regs_80[st];
	fpu.tags[other] = fpu.tags[st];
}

static inline void FPU_FCMOV(Bitu st, Bitu other){
	fpu.regs_80[st] = fpu.regs_80[other];
	fpu.tags[st] = fpu.tags[other];
}

static void FPU_FCOM(Bitu st, Bitu other){
	if(((fpu.tags[st] != TAG_Valid) && (fpu.tags[st] != TAG_Zero)) || 
		((fpu.tags[other] != TAG_Valid) && (fpu.tags[other] != TAG_Zero))){
		FPU_SET_C3(1);FPU_SET_C2(1);FPU_SET_C0(1);return;
	}

	/* HACK: If emulating a 286 processor we want the guest to think it's talking to a 287.
	 *       For more info, read [http://www.intel-assembler.it/portale/5/cpu-identification/asm-source-to-find-intel-cpu.asp]. */
	/* TODO: This should eventually become an option, say, a dosbox.conf option named fputype where the user can enter
	 *       "none" for no FPU, 287 or 387 for cputype=286 and cputype=386, or "auto" to match the CPU (8086 => 8087).
	 *       If the FPU type is 387 or auto, then skip this hack. Else for 8087 and 287, use this hack. */
	if (CPU_ArchitectureType<CPU_ARCHTYPE_386) {
		if ((std::isinf)(fpu.regs_80[st].v) && (std::isinf)(fpu.regs_80[other].v)) {
			/* 8087/287 consider -inf == +inf and that's what DOS programs test for to detect 287 vs 387 */
			FPU_SET_C3(1);FPU_SET_C2(0);FPU_SET_C0(0);return;
		}
	}

	if(fpu.regs_80[st].v == fpu.regs_80[other].v){
		FPU_SET_C3(1);FPU_SET_C2(0);FPU_SET_C0(0);return;
	}
	if(fpu.regs_80[st].v < fpu.regs_80[other].v){
		FPU_SET_C3(0);FPU_SET_C2(0);FPU_SET_C0(1);return;
	}
	// st > other
	FPU_SET_C3(0);FPU_SET_C2(0);FPU_SET_C0(0);return;
}

static void FPU_FUCOM(Bitu st, Bitu other){
	//does atm the same as fcom 
	FPU_FCOM(st,other);
}

static void FPU_FUCOMI(Bitu st, Bitu other){
	
	FillFlags();
	SETFLAGBIT(OF,false);

	if(fpu.regs_80[st].v == fpu.regs_80[other].v){
		SETFLAGBIT(ZF,true);SETFLAGBIT(PF,false);SETFLAGBIT(CF,false);return;
	}
	if(fpu.regs_80[st].v < fpu.regs_80[other].v){
		SETFLAGBIT(ZF,false);SETFLAGBIT(PF,false);SETFLAGBIT(CF,true);return;
	}
	// st > other
	SETFLAGBIT(ZF,false);SETFLAGBIT(PF,false);SETFLAGBIT(CF,false);return;
}

static inline void FPU_FCOMI(Bitu st, Bitu other){
	FPU_FUCOMI(st,other);

	if(((fpu.tags[st] != TAG_Valid) && (fpu.tags[st] != TAG_Zero)) || 
		((fpu.tags[other] != TAG_Valid) && (fpu.tags[other] != TAG_Zero))){
		SETFLAGBIT(ZF,true);SETFLAGBIT(PF,true);SETFLAGBIT(CF,true);return;
	}

}

static void FPU_FRNDINT(void){
	Bit64s temp= static_cast<Bit64s>(FROUND(fpu.regs_80[TOP].v));
	fpu.regs_80[TOP].v=static_cast<long double>(temp);
}

static void FPU_FPREM(void){
	long double valtop = fpu.regs_80[TOP].v;
	long double valdiv = fpu.regs_80[STV(1)].v;
	Bit64s ressaved = static_cast<Bit64s>( (valtop/valdiv) );
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
	Bit64s ressaved;
	if (quot-quotf>0.5) ressaved = static_cast<Bit64s>(quotf+1);
	else if (quot-quotf<0.5) ressaved = static_cast<Bit64s>(quotf);
	else ressaved = static_cast<Bit64s>((((static_cast<Bit64s>(quotf))&1)!=0)?(quotf+1):(quotf));
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
	if(fpu.tags[TOP] == TAG_Empty)
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
	fpu.regs_80[TOP].v *= powl(2.0,static_cast<long double>(static_cast<Bit64s>(fpu.regs_80[STV(1)].v)));
	return; //2^x where x is chopped.
}

static void FPU_FSTENV(PhysPt addr){
	FPU_SET_TOP(TOP);
	if(!cpu.code.big) {
		mem_writew(addr+0,static_cast<uint16_t>(fpu.cw));
		mem_writew(addr+2,static_cast<uint16_t>(fpu.sw));
		mem_writew(addr+4,static_cast<uint16_t>(FPU_GetTag()));
	} else { 
		mem_writed(addr+0,static_cast<uint32_t>(fpu.cw));
		mem_writed(addr+4,static_cast<uint32_t>(fpu.sw));
		mem_writed(addr+8,static_cast<uint32_t>(FPU_GetTag()));
	}
}

static void FPU_FLDENV(PhysPt addr){
	uint16_t tag;
	uint32_t tagbig;
	Bitu cw;
	if(!cpu.code.big) {
		cw     = mem_readw(addr+0);
		fpu.sw = mem_readw(addr+2);
		tag    = mem_readw(addr+4);
	} else { 
		cw     = mem_readd(addr+0);
		fpu.sw = (uint16_t)mem_readd(addr+4);
		tagbig = mem_readd(addr+8);
		tag    = static_cast<uint16_t>(tagbig);
	}
	FPU_SetTag(tag);
	FPU_SetCW(cw);
    FPU_SyncCW();
	TOP = FPU_GET_TOP();
}

static void FPU_FSAVE(PhysPt addr){
	FPU_FSTENV(addr);
	Bitu start = (cpu.code.big?28:14);
	for(Bitu i = 0;i < 8;i++){
		FPU_ST80(addr+start,STV(i));
		start += 10;
	}
	FPU_FINIT();
}

static void FPU_FRSTOR(PhysPt addr){
	FPU_FLDENV(addr);
	Bitu start = (cpu.code.big?28:14);
	for(Bitu i = 0;i < 8;i++){
		fpu.regs_80[STV(i)].v = FPU_FLD80(addr+start);
		start += 10;
	}
}

static void FPU_FXTRACT(void) {
	// function stores real bias in st and 
	// pushes the significant number onto the stack
	// if double ever uses a different base please correct this function

	FPU_Reg_80 test = fpu.regs_80[TOP];
	Bit64s exp80 = test.raw.h & 0x7FFFu;
	Bit64s exp80final = exp80 - FPU_Reg_80_exponent_bias;
	long double mant = test.v / (powl(2.0,static_cast<long double>(exp80final)));
	fpu.regs_80[TOP].v = static_cast<long double>(exp80final);
	FPU_PUSH(mant);
}

static void FPU_FCHS(void){
	fpu.regs_80[TOP].v = -1.0l*(fpu.regs_80[TOP].v);
}

static void FPU_FABS(void){
	fpu.regs_80[TOP].v = fabsl(fpu.regs_80[TOP].v);
}

static void FPU_FTST(void){
	fpu.regs_80[8].v = 0.0;
	FPU_FCOM(TOP,8);
}

static void FPU_FLD1(void){
	FPU_PREP_PUSH();
	fpu.regs_80[TOP].v = 1.0;
}

static void FPU_FLDL2T(void){
	FPU_PREP_PUSH();
	fpu.regs_80[TOP].v = L2T;
}

static void FPU_FLDL2E(void){
	FPU_PREP_PUSH();
	fpu.regs_80[TOP].v = L2E;
}

static void FPU_FLDPI(void){
	FPU_PREP_PUSH();
	fpu.regs_80[TOP].v = PI;
}

static void FPU_FLDLG2(void){
	FPU_PREP_PUSH();
	fpu.regs_80[TOP].v = LG2;
}

static void FPU_FLDLN2(void){
	FPU_PREP_PUSH();
	fpu.regs_80[TOP].v = LN2;
}

static void FPU_FLDZ(void){
	FPU_PREP_PUSH();
	fpu.regs_80[TOP].v = 0.0;
	fpu.tags[TOP] = TAG_Zero;
}


static INLINE void FPU_FADD_EA(Bitu op1){
	FPU_FADD(op1,8);
}
static INLINE void FPU_FMUL_EA(Bitu op1){
	FPU_FMUL(op1,8);
}
static INLINE void FPU_FSUB_EA(Bitu op1){
	FPU_FSUB(op1,8);
}
static INLINE void FPU_FSUBR_EA(Bitu op1){
	FPU_FSUBR(op1,8);
}
static INLINE void FPU_FDIV_EA(Bitu op1){
	FPU_FDIV(op1,8);
}
static INLINE void FPU_FDIVR_EA(Bitu op1){
	FPU_FDIVR(op1,8);
}
static INLINE void FPU_FCOM_EA(Bitu op1){
	FPU_FCOM(op1,8);
}

