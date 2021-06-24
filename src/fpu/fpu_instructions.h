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

#include <math.h> /* for isinf, etc */
#include "cpu/lazyflags.h"
static void FPU_FINIT(void) {
	unsigned int i;

	FPU_SetCW(0x37F);
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
	for (i=0;i < 9;i++) fpu.use80[i] = false;
}

static void FPU_FCLEX(void){
	fpu.sw &= 0x7f00;			//should clear exceptions
}

static void FPU_FNOP(void){
	return;
}

static void FPU_PREP_PUSH(void){
	TOP = (TOP - 1) &7;
//	if (GCC_UNLIKELY(fpu.tags[TOP] != TAG_Empty)) E_Exit("FPU stack overflow");
	fpu.tags[TOP] = TAG_Valid;
	fpu.use80[TOP] = false; // the value given is already 64-bit precision, it's useless to emulate 80-bit precision
}

static void FPU_PUSH(double in){
	FPU_PREP_PUSH();
	fpu.regs[TOP].d = in;
	fpu.use80[TOP] = false; // the value given is already 64-bit precision, it's useless to emulate 80-bit precision
//	LOG(LOG_FPU,LOG_ERROR)("Pushed at %d  %g to the stack",newtop,in);
	return;
}


static void FPU_FPOP(void){
//	if (GCC_UNLIKELY(fpu.tags[TOP] == TAG_Empty)) E_Exit("FPU stack underflow");
	fpu.tags[TOP]=TAG_Empty;
	fpu.use80[TOP] = false; // the value given is already 64-bit precision, it's useless to emulate 80-bit precision
	//maybe set zero in it as well
	TOP = ((TOP+1)&7);
//	LOG(LOG_FPU,LOG_ERROR)("popped from %d  %g off the stack",top,fpu.regs[top].d);
	return;
}

static double FROUND(double in){
	switch(fpu.round){
	case ROUND_Nearest:	
		if (in-floor(in)>0.5) return (floor(in)+1);
		else if (in-floor(in)<0.5) return (floor(in));
		else return (((static_cast<int64_t>(floor(in)))&1)!=0)?(floor(in)+1):(floor(in));
		break;
	case ROUND_Down:
		return (floor(in));
		break;
	case ROUND_Up:
		return (ceil(in));
		break;
	case ROUND_Chop:
		return in; //the cast afterwards will do it right maybe cast here
		break;
	default:
		return in;
		break;
	}
}

#define BIAS80 16383
#define BIAS64 1023

static double FPU_FLD80(PhysPt addr,FPU_Reg_80 &raw) {
	struct {
		int16_t begin;
		FPU_Reg eind;
	} test;
	test.eind.l.lower = mem_readd(addr);
	test.eind.l.upper = (int32_t)mem_readd(addr+4);
	test.begin = (int16_t)mem_readw(addr+8);
   
	int64_t exp64 = (((test.begin&0x7fff) - (int64_t)BIAS80));
	int64_t blah = ((exp64 >0)?exp64:-exp64)&0x3ff;
	int64_t exp64final = ((exp64 >0)?blah:-blah) +BIAS64;

	int64_t mant64 = (test.eind.ll >> 11) & LONGTYPE(0xfffffffffffff);
	int64_t sign = (test.begin&0x8000)?1:0;
	FPU_Reg result;
	result.ll = (sign <<63)|(exp64final << 52)| mant64;

	if(test.eind.l.lower == 0 && (uint32_t)test.eind.l.upper == (uint32_t)0x80000000UL && (test.begin&0x7fff) == 0x7fff) {
		//Detect INF and -INF (score 3.11 when drawing a slur.)
		result.d = sign?-HUGE_VAL:HUGE_VAL;
	}

	/* store the raw value. */
	raw.raw.l = (uint64_t)test.eind.ll;
	raw.raw.h = (uint16_t)test.begin;

	return result.d;

	//mant64= test.mant80/2***64    * 2 **53 
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
			FPU_Reg eind;
		} test;
		int64_t sign80 = ((uint64_t)fpu.regs[reg].ll&ULONGTYPE(0x8000000000000000))?1:0;
		int64_t exp80 =  fpu.regs[reg].ll&LONGTYPE(0x7ff0000000000000);
		int64_t exp80final = (exp80>>52);
		int64_t mant80 = fpu.regs[reg].ll&LONGTYPE(0x000fffffffffffff);
		int64_t mant80final = (mant80 << 11);
		if(fpu.regs[reg].d != 0){ //Zero is a special case
			// Elvira wants the 8 and tcalc doesn't
			mant80final |= (int64_t)ULONGTYPE(0x8000000000000000);
			//Ca-cyber doesn't like this when result is zero.
			exp80final += (BIAS80 - BIAS64);
		}
		test.begin = (static_cast<int16_t>(sign80)<<15)| static_cast<int16_t>(exp80final);
		test.eind.ll = mant80final;
		mem_writed(addr,test.eind.l.lower);
		mem_writed(addr+4,(uint32_t)test.eind.l.upper);
		mem_writew(addr+8,(uint16_t)test.begin);
	}
}


static void FPU_FLD_F32(PhysPt addr,Bitu store_to) {
	union {
		float f;
		uint32_t l;
	}	blah;
	blah.l = mem_readd(addr);
	fpu.regs[store_to].d = static_cast<double>(blah.f);
	fpu.use80[store_to] = false;
}

static void FPU_FLD_F64(PhysPt addr,Bitu store_to) {
	fpu.regs[store_to].l.lower = mem_readd(addr);
	fpu.regs[store_to].l.upper = (int32_t)mem_readd(addr+4);
	fpu.use80[store_to] = false;
}

static void FPU_FLD_F80(PhysPt addr) {
	fpu.regs[TOP].d = FPU_FLD80(addr,/*&*/fpu.regs_80[TOP]);
	fpu.use80[TOP] = true;
}

static void FPU_FLD_I16(PhysPt addr,Bitu store_to) {
	int16_t blah = (int16_t)mem_readw(addr);
	fpu.regs[store_to].d = static_cast<double>(blah);
	fpu.use80[store_to] = false;
}

static void FPU_FLD_I32(PhysPt addr,Bitu store_to) {
	int32_t blah = (int32_t)mem_readd(addr);
	fpu.regs[store_to].d = static_cast<double>(blah);
	fpu.use80[store_to] = false;
}

static void FPU_FLD_I64(PhysPt addr,Bitu store_to) {
	FPU_Reg blah;
	blah.l.lower = mem_readd(addr);
	blah.l.upper = (int32_t)mem_readd(addr+4);
	fpu.regs[store_to].d = static_cast<double>(blah.ll);
	// store the signed 64-bit integer in the 80-bit format mantissa with faked exponent.
	// this is needed for DOS and Windows games that use the Pentium fast memcpy trick, using FLD/FST to copy 64 bits at a time.
	// I wonder if that trick is what helped spur Intel to make the MMX extensions :)
	fpu.regs_80[store_to].raw.l = (uint64_t)blah.ll;
	fpu.regs_80[store_to].raw.h = ((blah.ll/*sign bit*/ >> (uint64_t)63) ? 0x8000u : 0x0000u) + FPU_Reg_80_exponent_bias + 63u; // FIXME: Verify this is correct!
	fpu.use80[store_to] = true;
}

static void FPU_FBLD(PhysPt addr,Bitu store_to) {
	uint64_t val = 0;
	uint8_t in = 0;
	uint64_t base = 1;
	for(uint8_t i = 0;i < 9;i++){
		in = mem_readb(addr + i);
		val += ( (in&0xf) * base); //in&0xf shouldn't be higher then 9
		base *= 10;
		val += ((( in>>4)&0xf) * base);
		base *= 10;
	}

	//last number, only now convert to float in order to get
	//the best signification
	double temp = static_cast<double>(val);
	in = mem_readb(addr + 9);
	temp += ( (in&0xf) * base );
	if(in&0x80) temp *= -1.0;
	fpu.regs[store_to].d = temp;
	fpu.use80[store_to] = false;
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
	union {
		float f;
		uint32_t l;
	}	blah;
	//should depend on rounding method
	blah.f = static_cast<float>(fpu.regs[TOP].d);
	mem_writed(addr,blah.l);
}

static void FPU_FST_F64(PhysPt addr) {
	mem_writed(addr,(uint32_t)fpu.regs[TOP].l.lower);
	mem_writed(addr+4,(uint32_t)fpu.regs[TOP].l.upper);
}

static void FPU_FST_F80(PhysPt addr) {
	FPU_ST80(addr,TOP,/*&*/fpu.regs_80[TOP],fpu.use80[TOP]);
}

static void FPU_FST_I16(PhysPt addr) {
	double val = FROUND(fpu.regs[TOP].d);
	mem_writew(addr,(val < 32768.0 && val >= -32768.0)?static_cast<int16_t>(val):0x8000);
}

static void FPU_FST_I32(PhysPt addr) {
	double val = FROUND(fpu.regs[TOP].d);
	mem_writed(addr,(val < 2147483648.0 && val >= -2147483648.0)?static_cast<int32_t>(val):0x80000000);
}

static void FPU_FST_I64(PhysPt addr) {
	FPU_Reg blah;
	if (fpu.use80[TOP] && (fpu.regs_80[TOP].raw.h & 0x7FFFu) == (0x0000u + FPU_Reg_80_exponent_bias + 63u)) {
		// FIXME: This works so far for DOS demos that use the "Pentium memcpy trick" to copy 64 bits at a time.
		//        What this code needs to do is take the exponent into account and then clamp the 64-bit int within range.
		//        This cheap hack is good enough for now.
		mem_writed(addr,(uint32_t)(fpu.regs_80[TOP].raw.l));
		mem_writed(addr+4,(uint32_t)(fpu.regs_80[TOP].raw.l >> (uint64_t)32));
	}
	else {
		double val = FROUND(fpu.regs[TOP].d);
		blah.ll = (val < 9223372036854775808.0 && val >= -9223372036854775808.0)?static_cast<int64_t>(val):LONGTYPE(0x8000000000000000);

		mem_writed(addr,(uint32_t)blah.l.lower);
		mem_writed(addr+4,(uint32_t)blah.l.upper);
	}
}

static void FPU_FBST(PhysPt addr) {
	FPU_Reg val = fpu.regs[TOP];
	if(val.ll & LONGTYPE(0x8000000000000000)) { // MSB = sign
		mem_writeb(addr+9,0x80);
		val.d = -val.d;
	} else mem_writeb(addr+9,0);

	uint64_t rndint = static_cast<uint64_t>(FROUND(val.d));
	// BCD (18 decimal digits) overflow? (0x0DE0B6B3A763FFFF max)
	if (rndint > LONGTYPE(999999999999999999)) {
		// write BCD integer indefinite value
		mem_writed(addr+0,0);
		mem_writed(addr+4,0xC0000000);
		mem_writew(addr+8,0xFFFF);
		return;
	}

	//numbers from back to front
	for(uint8_t i=0;i<9;i++){
		uint64_t temp = rndint / 10;
		uint8_t p = static_cast<uint8_t>(rndint % 10);
		rndint = temp / 10;
		p |= (static_cast<uint8_t>(temp % 10)) << 4;
		mem_writeb(addr++,p);
	}
	// flags? C1 should indicate if value was rounded up
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
	// HACK: Set the denormal flag according to whether the source or final result is a denormalized number.
	//       This is vital if we don't want certain DOS programs to mis-detect our FPU emulation as an IIT clone chip when cputype == 286
	bool was_not_normal = isdenormal(fpu.regs[op1].d);
	fpu.regs[op1].d+=fpu.regs[op2].d;
	FPU_SET_D(was_not_normal || isdenormal(fpu.regs[op1].d) || isdenormal(fpu.regs[op2].d));
	fpu.use80[op1] = false; // we used the less precise version, drop the 80-bit precision
	//flags and such :)
	return;
}

static void FPU_FSIN(void){
	fpu.use80[TOP] = false; // we used the less precise version, drop the 80-bit precision
	fpu.regs[TOP].d = sin(fpu.regs[TOP].d);
	FPU_SET_C2(0);
	//flags and such :)
	return;
}

static void FPU_FSINCOS(void){
	double temp = fpu.regs[TOP].d;
	fpu.use80[TOP] = false; // we used the less precise version, drop the 80-bit precision
	fpu.regs[TOP].d = sin(temp);
	FPU_PUSH(cos(temp));
	FPU_SET_C2(0);
	//flags and such :)
	return;
}

static void FPU_FCOS(void){
	fpu.use80[TOP] = false; // we used the less precise version, drop the 80-bit precision
	fpu.regs[TOP].d = cos(fpu.regs[TOP].d);
	FPU_SET_C2(0);
	//flags and such :)
	return;
}

static void FPU_FSQRT(void){
	fpu.use80[TOP] = false; // we used the less precise version, drop the 80-bit precision
	fpu.regs[TOP].d = sqrt(fpu.regs[TOP].d);
	//flags and such :)
	return;
}
static void FPU_FPATAN(void){
	fpu.use80[STV(1)] = false; // we used the less precise version, drop the 80-bit precision
	fpu.regs[STV(1)].d = atan2(fpu.regs[STV(1)].d,fpu.regs[TOP].d);
	FPU_FPOP();
	//flags and such :)
	return;
}
static void FPU_FPTAN(void){
	fpu.use80[TOP] = false; // we used the less precise version, drop the 80-bit precision
	fpu.regs[TOP].d = tan(fpu.regs[TOP].d);
	FPU_PUSH(1.0);
	FPU_SET_C2(0);
	//flags and such :)
	return;
}
static void FPU_FDIV(Bitu st, Bitu other){
	fpu.use80[st] = false; // we used the less precise version, drop the 80-bit precision
	fpu.regs[st].d= fpu.regs[st].d/fpu.regs[other].d;
	//flags and such :)
	return;
}

static void FPU_FDIVR(Bitu st, Bitu other){
	fpu.use80[st] = false; // we used the less precise version, drop the 80-bit precision
	fpu.regs[st].d= fpu.regs[other].d/fpu.regs[st].d;
	// flags and such :)
	return;
}

static void FPU_FMUL(Bitu st, Bitu other){
	fpu.use80[st] = false; // we used the less precise version, drop the 80-bit precision
	fpu.regs[st].d*=fpu.regs[other].d;
	//flags and such :)
	return;
}

static void FPU_FSUB(Bitu st, Bitu other){
	fpu.use80[st] = false; // we used the less precise version, drop the 80-bit precision
	fpu.regs[st].d = fpu.regs[st].d - fpu.regs[other].d;
	//flags and such :)
	return;
}

static void FPU_FSUBR(Bitu st, Bitu other){
	fpu.use80[st] = false; // we used the less precise version, drop the 80-bit precision
	fpu.regs[st].d= fpu.regs[other].d - fpu.regs[st].d;
	//flags and such :)
	return;
}

static void FPU_FXCH(Bitu st, Bitu other){
	FPU_Reg_80 reg80 = fpu.regs_80[other];
	FPU_Tag tag = fpu.tags[other];
	FPU_Reg reg = fpu.regs[other];
	bool use80 = fpu.use80[other];

	fpu.regs_80[other] = fpu.regs_80[st];
	fpu.use80[other] = fpu.use80[st];
	fpu.tags[other] = fpu.tags[st];
	fpu.regs[other] = fpu.regs[st];

	fpu.regs_80[st] = reg80;
	fpu.use80[st] = use80;
	fpu.tags[st] = tag;
	fpu.regs[st] = reg;
}

static void FPU_FST(Bitu st, Bitu other){
	fpu.regs_80[other] = fpu.regs_80[st];
	fpu.use80[other] = fpu.use80[st];
	fpu.tags[other] = fpu.tags[st];
	fpu.regs[other] = fpu.regs[st];
}

static inline void FPU_FCMOV(Bitu st, Bitu other){
	fpu.regs_80[st] = fpu.regs_80[other];
	fpu.use80[st] = fpu.use80[other];
	fpu.tags[st] = fpu.tags[other];
	fpu.regs[st] = fpu.regs[other];
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
		if ((std::isinf)(fpu.regs[st].d) && (std::isinf)(fpu.regs[other].d)) {
			/* 8087/287 consider -inf == +inf and that's what DOS programs test for to detect 287 vs 387 */
			FPU_SET_C3(1);FPU_SET_C2(0);FPU_SET_C0(0);return;
		}
	}

	if(fpu.regs[st].d == fpu.regs[other].d){
		FPU_SET_C3(1);FPU_SET_C2(0);FPU_SET_C0(0);return;
	}
	if(fpu.regs[st].d < fpu.regs[other].d){
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

	if(fpu.regs[st].d == fpu.regs[other].d){
		SETFLAGBIT(ZF,true);SETFLAGBIT(PF,false);SETFLAGBIT(CF,false);return;
	}
	if(fpu.regs[st].d < fpu.regs[other].d){
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
	int64_t temp= static_cast<int64_t>(FROUND(fpu.regs[TOP].d));
	fpu.regs[TOP].d=static_cast<double>(temp);
	fpu.use80[TOP] = false;
}

static void FPU_FPREM(void){
	double valtop = fpu.regs[TOP].d;
	double valdiv = fpu.regs[STV(1)].d;
	int64_t ressaved = static_cast<int64_t>( (valtop/valdiv) );
// Some backups
//	double res=valtop - ressaved*valdiv; 
//      res= fmod(valtop,valdiv);
	fpu.use80[TOP] = false; // we used the less precise version, drop the 80-bit precision
	fpu.regs[TOP].d = valtop - ressaved*valdiv;
	FPU_SET_C0(static_cast<Bitu>(ressaved&4));
	FPU_SET_C3(static_cast<Bitu>(ressaved&2));
	FPU_SET_C1(static_cast<Bitu>(ressaved&1));
	FPU_SET_C2(0);
}

static void FPU_FPREM1(void){
	double valtop = fpu.regs[TOP].d;
	double valdiv = fpu.regs[STV(1)].d;
	double quot = valtop/valdiv;
	double quotf = floor(quot);
	int64_t ressaved;
	if (quot-quotf>0.5) ressaved = static_cast<int64_t>(quotf+1);
	else if (quot-quotf<0.5) ressaved = static_cast<int64_t>(quotf);
	else ressaved = static_cast<int64_t>((((static_cast<int64_t>(quotf))&1)!=0)?(quotf+1):(quotf));
	fpu.use80[TOP] = false; // we used the less precise version, drop the 80-bit precision
	fpu.regs[TOP].d = valtop - ressaved*valdiv;
	FPU_SET_C0(static_cast<Bitu>(ressaved&4));
	FPU_SET_C3(static_cast<Bitu>(ressaved&2));
	FPU_SET_C1(static_cast<Bitu>(ressaved&1));
	FPU_SET_C2(0);
}

static void FPU_FXAM(void){
	if((uint64_t)fpu.regs[TOP].ll & ULONGTYPE(0x8000000000000000))	//sign
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
	if(fpu.regs[TOP].d == 0.0)		//zero or normalized number.
	{ 
		FPU_SET_C3(1);FPU_SET_C2(0);FPU_SET_C0(0);
	}
	else
	{
		FPU_SET_C3(0);FPU_SET_C2(1);FPU_SET_C0(0);
	}
}


static void FPU_F2XM1(void){
	fpu.use80[TOP] = false; // we used the less precise version, drop the 80-bit precision
	fpu.regs[TOP].d = pow(2.0,fpu.regs[TOP].d) - 1;
	return;
}

static void FPU_FYL2X(void){
	fpu.use80[STV(1)] = false; // we used the less precise version, drop the 80-bit precision
	fpu.regs[STV(1)].d*=log(fpu.regs[TOP].d)/log(static_cast<double>(2.0));
	FPU_FPOP();
	return;
}

static void FPU_FYL2XP1(void){
	fpu.use80[STV(1)] = false; // we used the less precise version, drop the 80-bit precision
	fpu.regs[STV(1)].d*=log(fpu.regs[TOP].d+1.0)/log(static_cast<double>(2.0));
	FPU_FPOP();
	return;
}

static void FPU_FSCALE(void){
	fpu.use80[TOP] = false; // we used the less precise version, drop the 80-bit precision
	fpu.regs[TOP].d *= pow(2.0,static_cast<double>(static_cast<int64_t>(fpu.regs[STV(1)].d)));
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
	TOP = FPU_GET_TOP();
}

static void FPU_FSAVE(PhysPt addr){
	FPU_FSTENV(addr);
	uint8_t start = (cpu.code.big?28:14);
	for(uint8_t i = 0;i < 8;i++){
		FPU_ST80(addr+start,STV(i),/*&*/fpu.regs_80[STV(i)],fpu.use80[STV(i)]);
		start += 10;
	}
	FPU_FINIT();
}

static void FPU_FRSTOR(PhysPt addr){
	FPU_FLDENV(addr);
	uint8_t start = (cpu.code.big?28:14);
	for(uint8_t i = 0;i < 8;i++){
		fpu.regs[STV(i)].d = FPU_FLD80(addr+start,/*&*/fpu.regs_80[STV(i)]);
		fpu.use80[STV(i)] = true;
		start += 10;
	}
}

static void FPU_FXTRACT(void) {
	// function stores real bias in st and 
	// pushes the significant number onto the stack
	// if double ever uses a different base please correct this function

	FPU_Reg test = fpu.regs[TOP];
	int64_t exp80 =  test.ll&LONGTYPE(0x7ff0000000000000);
	int64_t exp80final = (exp80>>52) - BIAS64;
	double mant = test.d / (pow(2.0,static_cast<double>(exp80final)));
	fpu.use80[TOP] = false; // we used the less precise version, drop the 80-bit precision
	fpu.regs[TOP].d = static_cast<double>(exp80final);
	FPU_PUSH(mant);
}

static void FPU_FCHS(void){
	fpu.use80[TOP] = false; // we used the less precise version, drop the 80-bit precision
	fpu.regs[TOP].d = -1.0*(fpu.regs[TOP].d);
}

static void FPU_FABS(void){
	fpu.use80[TOP] = false; // we used the less precise version, drop the 80-bit precision
	fpu.regs[TOP].d = fabs(fpu.regs[TOP].d);
}

static void FPU_FTST(void){
	fpu.use80[8] = false; // we used the less precise version, drop the 80-bit precision
	fpu.regs[8].d = 0.0;
	FPU_FCOM(TOP,8);
}

static void FPU_FLD1(void){
	FPU_PREP_PUSH();
	fpu.use80[TOP] = false; // we used the less precise version, drop the 80-bit precision
	fpu.regs[TOP].d = 1.0;
}

static void FPU_FLDL2T(void){
	FPU_PREP_PUSH();
	fpu.use80[TOP] = false; // we used the less precise version, drop the 80-bit precision
	fpu.regs[TOP].d = L2T;
}

static void FPU_FLDL2E(void){
	FPU_PREP_PUSH();
	fpu.use80[TOP] = false; // we used the less precise version, drop the 80-bit precision
	fpu.regs[TOP].d = L2E;
}

static void FPU_FLDPI(void){
	FPU_PREP_PUSH();
	fpu.use80[TOP] = false; // we used the less precise version, drop the 80-bit precision
	fpu.regs[TOP].d = PI;
}

static void FPU_FLDLG2(void){
	FPU_PREP_PUSH();
	fpu.use80[TOP] = false; // we used the less precise version, drop the 80-bit precision
	fpu.regs[TOP].d = LG2;
}

static void FPU_FLDLN2(void){
	FPU_PREP_PUSH();
	fpu.use80[TOP] = false; // we used the less precise version, drop the 80-bit precision
	fpu.regs[TOP].d = LN2;
}

static void FPU_FLDZ(void){
	FPU_PREP_PUSH();
	fpu.use80[TOP] = false; // we used the less precise version, drop the 80-bit precision
	fpu.regs[TOP].d = 0.0;
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

