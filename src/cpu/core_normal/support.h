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

#include <math.h>
#include <string.h>
#include "lock.h"

#define LoadMbs(off) (int8_t)(LoadMb(off))
#define LoadMws(off) (int16_t)(LoadMw(off))
#define LoadMds(off) (int32_t)(LoadMd(off))

#define LoadRb(reg) reg
#define LoadRw(reg) reg
#define LoadRd(reg) reg

#define SaveRb(reg,val)	reg=((uint8_t)(val))
#define SaveRw(reg,val)	reg=((uint16_t)(val))
#define SaveRd(reg,val)	reg=((uint32_t)(val))

static INLINE int8_t Fetchbs() {
	return (int8_t)Fetchb();
}
static INLINE int16_t Fetchws() {
	return (int16_t)Fetchw();
}

static INLINE int32_t Fetchds() {
	return (int32_t)Fetchd();
}


#define RUNEXCEPTION() {										\
	PRE_EXCEPTION \
	CPU_Exception(cpu.exception.which,cpu.exception.error);		\
	continue;													\
}

#define EXCEPTION(blah)										\
	{														\
		PRE_EXCEPTION \
		CPU_Exception(blah);								\
		continue;											\
	}

/* NTS: At first glance, this looks like code that will only fetch the delta for conditional jumps
 *      if the condition is true. Further examination shows that DOSBox's core has two different
 *      CS:IP variables, reg_ip and core.cseip which Fetchb() modifies. */
//TODO Could probably make all byte operands fast?
#define JumpCond16_b(COND) {						\
	const uint32_t adj=(uint32_t)Fetchbs();						\
	SAVEIP;								\
	if (COND) reg_ip+=adj;						\
	continue;							\
}

#define JumpCond16_w(COND) {						\
	const uint32_t adj=(uint32_t)Fetchws();						\
	SAVEIP;								\
	if (COND) reg_ip+=adj;						\
	continue;							\
}

#define JumpCond32_b(COND) {						\
	const uint32_t adj=(uint32_t)Fetchbs();						\
	SAVEIP;								\
	if (COND) reg_eip+=adj;						\
	continue;							\
}

#define JumpCond32_d(COND) {						\
	const uint32_t adj=(uint32_t)Fetchds();						\
	SAVEIP;								\
	if (COND) reg_eip+=adj;						\
	continue;							\
}

#define MoveCond16(COND) {							\
	GetRMrw;										\
	if (rm >= 0xc0 ) {GetEArw; if (COND) *rmrw=*earw;}\
	else {GetEAa; if (COND) *rmrw=LoadMw(eaa);}		\
}

#define MoveCond32(COND) {							\
	GetRMrd;										\
	if (rm >= 0xc0 ) {GetEArd; if (COND) *rmrd=*eard;}\
	else {GetEAa; if (COND) *rmrd=LoadMd(eaa);}		\
}

#if CPU_CORE >= CPU_ARCHTYPE_80386

/* Most SSE instructions require memory access aligned to the SSE data type, else
 * an exception occurs. */
static INLINE bool SSE_REQUIRE_ALIGNMENT(const PhysPt v) {
	return ((unsigned int)v & 15u) == 0; /* 16 bytes * 8 bits = 128 bits */
}

/* Throw GPF on SSE misalignment [https://c9x.me/x86/html/file_module_x86_id_180.html] */
/* NTS: This macro intended for use in normal core */
#define SSE_ALIGN_EXCEPTION() EXCEPTION(EXCEPTION_GP)

#define STEP(i) SSE_MULPS_i(d.f32[i],s.f32[i])
static INLINE void SSE_MULPS_i(FPU_Reg_32 &d,const FPU_Reg_32 &s) {
	d.v *= s.v;
}

static INLINE void SSE_MULPS(XMM_Reg &d,const XMM_Reg &s) {
	STEP(0);
	STEP(1);
	STEP(2);
	STEP(3);
}

static INLINE void SSE_MULSS(XMM_Reg &d,const XMM_Reg &s) {
	STEP(0);
}
#undef STEP

////

#define STEP(i) SSE_ANDPS_i(d.f32[i],s.f32[i])
static INLINE void SSE_ANDPS_i(FPU_Reg_32 &d,const FPU_Reg_32 &s) {
	d.raw &= s.raw;
}

static INLINE void SSE_ANDPS(XMM_Reg &d,const XMM_Reg &s) {
	STEP(0);
	STEP(1);
	STEP(2);
	STEP(3);
}
#undef STEP

////

#define STEP(i) SSE_ANDNPS_i(d.f32[i],s.f32[i])
static INLINE void SSE_ANDNPS_i(FPU_Reg_32 &d,const FPU_Reg_32 &s) {
	d.raw = (~d.raw) & s.raw;
}

static INLINE void SSE_ANDNPS(XMM_Reg &d,const XMM_Reg &s) {
	STEP(0);
	STEP(1);
	STEP(2);
	STEP(3);
}
#undef STEP

////

#define STEP(i) SSE_XORPS_i(d.f32[i],s.f32[i])
static INLINE void SSE_XORPS_i(FPU_Reg_32 &d,const FPU_Reg_32 &s) {
	d.raw ^= s.raw;
}

static INLINE void SSE_XORPS(XMM_Reg &d,const XMM_Reg &s) {
	STEP(0);
	STEP(1);
	STEP(2);
	STEP(3);
}
#undef STEP

////

#define STEP(i) SSE_SQRTPS_i(d.f32[i],s.f32[i])
static INLINE void SSE_SQRTPS_i(FPU_Reg_32 &d,const FPU_Reg_32 &s) {
	d.v = sqrtf(s.v);
}

static INLINE void SSE_SQRTPS(XMM_Reg &d,const XMM_Reg &s) {
	STEP(0);
	STEP(1);
	STEP(2);
	STEP(3);
}

static INLINE void SSE_SQRTSS(XMM_Reg &d,const XMM_Reg &s) {
	STEP(0);
}
#undef STEP

////

#define STEP(i) SSE_MOVAPS_i(d.f32[i],s.f32[i])
static INLINE void SSE_MOVAPS_i(FPU_Reg_32 &d,const FPU_Reg_32 &s) {
	d.raw = s.raw;
}

static INLINE void SSE_MOVAPS(XMM_Reg &d,const XMM_Reg &s) {
	STEP(0);
	STEP(1);
	STEP(2);
	STEP(3);
}
#undef STEP

////

#define STEP(i) SSE_MOVUPS_i(d.f32[i],s.f32[i])
static INLINE void SSE_MOVUPS_i(FPU_Reg_32 &d,const FPU_Reg_32 &s) {
	d.raw = s.raw;
}

static INLINE void SSE_MOVUPS(XMM_Reg &d,const XMM_Reg &s) {
	STEP(0);
	STEP(1);
	STEP(2);
	STEP(3);
}

static INLINE void SSE_MOVSS(XMM_Reg &d,const XMM_Reg &s) {
	STEP(0);
}
#undef STEP

////

static INLINE void SSE_MOVHLPS(XMM_Reg &d,const XMM_Reg &s) {
	d.u64[0] = s.u64[1];
}

static INLINE void SSE_MOVLPS(XMM_Reg &d,const XMM_Reg &s) {
	d.u64[0] = s.u64[0];
}

////

static INLINE void SSE_UNPCKLPS(XMM_Reg &d,const XMM_Reg &s) {
	//d.u32[0] = d.u32[0];
    d.u32[2] = d.u32[1];
    d.u32[1] = s.u32[0];
	d.u32[3] = s.u32[1];
}

////

static INLINE void SSE_UNPCKHPS(XMM_Reg &d,const XMM_Reg &s) {
	d.u32[0] = d.u32[2];
    d.u32[2] = d.u32[3];
    d.u32[1] = s.u32[2];
	d.u32[3] = s.u32[3];
}

////

static INLINE void SSE_MOVLHPS(XMM_Reg &d,const XMM_Reg &s) {
	d.u64[1] = s.u64[0];
}

static INLINE void SSE_MOVHPS(XMM_Reg &d,const XMM_Reg &s) {
	d.u64[1] = s.u64[0];
}

////

static INLINE void SSE_CVTPI2PS_i(FPU_Reg_32 &d,const int32_t s) {
	d.v = (float)s;
}

static INLINE void SSE_CVTPI2PS(XMM_Reg &d,const MMX_reg &s) {
	SSE_CVTPI2PS_i(d.f32[0],s.sd.d0);
	SSE_CVTPI2PS_i(d.f32[1],s.sd.d1);
}

static INLINE void SSE_CVTSI2SS(XMM_Reg &d,const uint32_t s) {
	SSE_CVTPI2PS_i(d.f32[0],(int32_t)s);
}

////

static INLINE void SSE_CVTTPS2PI_i(int32_t &d,const FPU_Reg_32 &s) {
	if (s.v < -0x7FFFFFFF || s.v > 0x7FFFFFFF)
		d = (int32_t)0x80000000;
	else if (s.v > 0) // truncate towards zero
		d = (int32_t)floor(s.v);
	else // truncate towards zero
		d = -((int32_t)floor(-s.v));
}

static INLINE void SSE_CVTTPS2PI(MMX_reg &d,const XMM_Reg &s) {
	SSE_CVTTPS2PI_i(d.sd.d0,s.f32[0]);
	SSE_CVTTPS2PI_i(d.sd.d1,s.f32[1]);
	d.fpu.e = 0xFFFF;
}

static INLINE void SSE_CVTTSS2SI(uint32_t &d,const XMM_Reg &s) {
	SSE_CVTTPS2PI_i((int32_t&)d,s.f32[0]);
}

////

static INLINE void SSE_CVTPS2PI_i(int32_t &d,const FPU_Reg_32 &s) {
	if (s.v < -0x7FFFFFFF || s.v > 0x7FFFFFFF)
		d = (int32_t)0x80000000;
	else // based on rounding mode in MXCSR (TODO)
		d = (int32_t)s.v;
}

static INLINE void SSE_CVTPS2PI(MMX_reg &d,const XMM_Reg &s) {
	SSE_CVTPS2PI_i(d.sd.d0,s.f32[0]);
	SSE_CVTPS2PI_i(d.sd.d1,s.f32[1]);
	d.fpu.e = 0xFFFF;
}

static INLINE void SSE_CVTSS2SI(uint32_t &d,const XMM_Reg &s) {
	SSE_CVTPS2PI_i((int32_t&)d,s.f32[0]);
}

////

static INLINE void SSE_COMISS_common(const XMM_Reg &d,const XMM_Reg &s) {
	FillFlags();
	reg_flags &= ~(FLAG_CF|FLAG_PF|FLAG_AF|FLAG_ZF|FLAG_SF|FLAG_OF);

	if (isnan(d.f32[0].v) || isnan(s.f32[0].v))
		reg_flags |= FLAG_ZF|FLAG_PF|FLAG_CF; /* unordered compare */
	else if (d.f32[0].v == s.f32[0].v)
		reg_flags |= FLAG_ZF;
	else if (d.f32[0].v > s.f32[0].v)
		{ /* no change */ }
	else /* d < s */
		reg_flags |= FLAG_CF;
}

static INLINE void SSE_UCOMISS(const XMM_Reg &d,const XMM_Reg &s) {
	SSE_COMISS_common(d,s);
}

static INLINE void SSE_COMISS(const XMM_Reg &d,const XMM_Reg &s) {
	SSE_COMISS_common(d,s);
}

////

static INLINE void SSE_MOVMSKPS(uint32_t &d,const XMM_Reg &s) {
	/* take sign bits from floats, stick into low bits of register, zero extend */
	d =
		(s.f32[0].f.sign ? 0x1 : 0x0) +
		(s.f32[1].f.sign ? 0x2 : 0x0) +
		(s.f32[2].f.sign ? 0x4 : 0x0) +
		(s.f32[3].f.sign ? 0x8 : 0x0);

}

////

#define STEP(i) SSE_RSQRTPS_i(d.f32[i],s.f32[i])
static INLINE void SSE_RSQRTPS_i(FPU_Reg_32 &d,const FPU_Reg_32 &s) {
	d.v = 1.0f / sqrtf(s.v);
}

static INLINE void SSE_RSQRTPS(XMM_Reg &d,const XMM_Reg &s) {
	STEP(0);
	STEP(1);
	STEP(2);
	STEP(3);
}

static INLINE void SSE_RSQRTSS(XMM_Reg &d,const XMM_Reg &s) {
	STEP(0);
}
#undef STEP

////

#define STEP(i) SSE_RCPPS_i(d.f32[i],s.f32[i])
static INLINE void SSE_RCPPS_i(FPU_Reg_32 &d,const FPU_Reg_32 &s) {
	d.v = 1.0f / s.v;
}

static INLINE void SSE_RCPPS(XMM_Reg &d,const XMM_Reg &s) {
	STEP(0);
	STEP(1);
	STEP(2);
	STEP(3);
}

static INLINE void SSE_RCPSS(XMM_Reg &d,const XMM_Reg &s) {
	STEP(0);
}
#undef STEP

////

#define STEP(i) SSE_ORPS_i(d.f32[i],s.f32[i])
static INLINE void SSE_ORPS_i(FPU_Reg_32 &d,const FPU_Reg_32 &s) {
	d.raw |= s.raw;
}

static INLINE void SSE_ORPS(XMM_Reg &d,const XMM_Reg &s) {
	STEP(0);
	STEP(1);
	STEP(2);
	STEP(3);
}
#undef STEP

////

#define STEP(i) SSE_ADDPS_i(d.f32[i],s.f32[i])
static INLINE void SSE_ADDPS_i(FPU_Reg_32 &d,const FPU_Reg_32 &s) {
	d.v += s.v;
}

static INLINE void SSE_ADDPS(XMM_Reg &d,const XMM_Reg &s) {
	STEP(0);
	STEP(1);
	STEP(2);
	STEP(3);
}

static INLINE void SSE_ADDSS(XMM_Reg &d,const XMM_Reg &s) {
	STEP(0);
}
#undef STEP

////

#define STEP(i) SSE_SUBPS_i(d.f32[i],s.f32[i])
static INLINE void SSE_SUBPS_i(FPU_Reg_32 &d,const FPU_Reg_32 &s) {
	d.v -= s.v;
}

static INLINE void SSE_SUBPS(XMM_Reg &d,const XMM_Reg &s) {
	STEP(0);
	STEP(1);
	STEP(2);
	STEP(3);
}

static INLINE void SSE_SUBSS(XMM_Reg &d,const XMM_Reg &s) {
	STEP(0);
}
#undef STEP

////

#define STEP(i) SSE_DIVPS_i(d.f32[i],s.f32[i])
static INLINE void SSE_DIVPS_i(FPU_Reg_32 &d,const FPU_Reg_32 &s) {
	d.v /= s.v;
}

static INLINE void SSE_DIVPS(XMM_Reg &d,const XMM_Reg &s) {
	STEP(0);
	STEP(1);
	STEP(2);
	STEP(3);
}

static INLINE void SSE_DIVSS(XMM_Reg &d,const XMM_Reg &s) {
	STEP(0);
}
#undef STEP

////

#define STEP(i) SSE_MINPS_i(d.f32[i],s.f32[i])
static INLINE void SSE_MINPS_i(FPU_Reg_32 &d,const FPU_Reg_32 &s) {
	d.v = std::min(d.v,s.v);
}

static INLINE void SSE_MINPS(XMM_Reg &d,const XMM_Reg &s) {
	STEP(0);
	STEP(1);
	STEP(2);
	STEP(3);
}

static INLINE void SSE_MINSS(XMM_Reg &d,const XMM_Reg &s) {
	STEP(0);
}
#undef STEP

////

#define STEP(i) SSE_MAXPS_i(d.f32[i],s.f32[i])
static INLINE void SSE_MAXPS_i(FPU_Reg_32 &d,const FPU_Reg_32 &s) {
	d.v = std::max(d.v,s.v);
}

static INLINE void SSE_MAXPS(XMM_Reg &d,const XMM_Reg &s) {
	STEP(0);
	STEP(1);
	STEP(2);
	STEP(3);
}

static INLINE void SSE_MAXSS(XMM_Reg &d,const XMM_Reg &s) {
	STEP(0);
}
#undef STEP

////

#define STEP(i) SSE_CMPPS_i(d.u32[i],d.f32[i],s.f32[i],cf)
static INLINE void SSE_CMPPS_i(uint32_t &d,const FPU_Reg_32 &s1,const FPU_Reg_32 &s2,const uint8_t cf) {
	switch (cf) {
		case 0:/*CMPEQPS*/	d = (s1.v == s2.v) ? (uint32_t)0xFFFFFFFFul : (uint32_t)0x00000000ul; break;
		case 1:/*CMPLTPS*/	d = (s1.v <  s2.v) ? (uint32_t)0xFFFFFFFFul : (uint32_t)0x00000000ul; break;
		case 2:/*CMPLEPS*/	d = (s1.v <= s2.v) ? (uint32_t)0xFFFFFFFFul : (uint32_t)0x00000000ul; break;
		case 3:/*CMPUNORDPS*/	d = ( isnan(s1.v) ||  isnan(s2.v)) ? (uint32_t)0xFFFFFFFFul : (uint32_t)0x00000000ul; break;
		case 4:/*CMPNEQPS*/	d = (s1.v != s2.v) ? (uint32_t)0xFFFFFFFFul : (uint32_t)0x00000000ul; break;
		case 5:/*CMPNLTPS*/	d = (s1.v >= s2.v) ? (uint32_t)0xFFFFFFFFul : (uint32_t)0x00000000ul; break;
		case 6:/*CMPNLEPS*/	d = (s1.v >  s2.v) ? (uint32_t)0xFFFFFFFFul : (uint32_t)0x00000000ul; break;
		case 7:/*CMPORDPS*/	d = (!isnan(s1.v) && !isnan(s2.v)) ? (uint32_t)0xFFFFFFFFul : (uint32_t)0x00000000ul; break;
	}
}

static INLINE void SSE_CMPPS(XMM_Reg &d,const XMM_Reg &s,const uint8_t cf) {
	STEP(0);
	STEP(1);
	STEP(2);
	STEP(3);
}

static INLINE void SSE_CMPSS(XMM_Reg &d,const XMM_Reg &s,const uint8_t cf) {
	STEP(0);
}
#undef STEP

////

//// SSE2

/* The SSE2 helpers and memory accesses are kept out of line. INLINE is always_inline
 * with GCC, and inlined into every case of the opcode switch they made the core too big
 * for the 32-bit compilers of the lowend MinGW and HX-DOS builds (cc1plus: out of memory). */
#define SSE2_HELPER static inline GCC_ATTRIBUTE(noinline)

/* only the cores that define LoadMq/SaveMq (386 and up) run the SSE2 cases */
#if defined(LoadMq) && defined(SaveMq)
SSE2_HELPER uint64_t SSE_LoadMq(const PhysPt off) {
	return LoadMq(off);
}

SSE2_HELPER void SSE_SaveMq(const PhysPt off,const uint64_t val) {
	SaveMq(off,val);
}
#endif

//// SSE2: double precision

#define SSE2_ARITH(NAME,EXPR) \
SSE2_HELPER void SSE2_##NAME##PD(XMM_Reg &d,const XMM_Reg &s) { \
	for (unsigned int i=0;i < 2;i++) { const double a = d.f64[i].v, b = s.f64[i].v; (void)a; (void)b; d.f64[i].v = (EXPR); } \
} \
SSE2_HELPER void SSE2_##NAME##SD(XMM_Reg &d,const XMM_Reg &s) { \
	const double a = d.f64[0].v, b = s.f64[0].v; (void)a; (void)b; d.f64[0].v = (EXPR); \
}

SSE2_ARITH(ADD,a + b)
SSE2_ARITH(SUB,a - b)
SSE2_ARITH(MUL,a * b)
SSE2_ARITH(DIV,a / b)
SSE2_ARITH(SQRT,sqrt(b))
SSE2_ARITH(MIN,(a < b) ? a : b)	/* NaN or both zero: the second (source) operand, as the SDM says */
SSE2_ARITH(MAX,(a > b) ? a : b)
#undef SSE2_ARITH

SSE2_HELPER void SSE2_ANDPD(XMM_Reg &d,const XMM_Reg &s) {
	d.u64[0] &= s.u64[0];
	d.u64[1] &= s.u64[1];
}

SSE2_HELPER void SSE2_ANDNPD(XMM_Reg &d,const XMM_Reg &s) {
	d.u64[0] = ~d.u64[0] & s.u64[0];
	d.u64[1] = ~d.u64[1] & s.u64[1];
}

SSE2_HELPER void SSE2_ORPD(XMM_Reg &d,const XMM_Reg &s) {
	d.u64[0] |= s.u64[0];
	d.u64[1] |= s.u64[1];
}

SSE2_HELPER void SSE2_XORPD(XMM_Reg &d,const XMM_Reg &s) {
	d.u64[0] ^= s.u64[0];
	d.u64[1] ^= s.u64[1];
}

SSE2_HELPER void SSE2_UNPCKLPD(XMM_Reg &d,const XMM_Reg &s) {
	d.u64[1] = s.u64[0];
}

SSE2_HELPER void SSE2_UNPCKHPD(XMM_Reg &d,const XMM_Reg &s) {
	d.u64[0] = d.u64[1];
	d.u64[1] = s.u64[1];
}

SSE2_HELPER void SSE2_SHUFPD(XMM_Reg &d,const XMM_Reg &s,const uint8_t i) {
	XMM_Reg tmp;
	tmp.u64[0] = d.u64[i & 1u];
	tmp.u64[1] = s.u64[(i >> 1u) & 1u];
	d = tmp;
}

SSE2_HELPER void SSE2_MOVMSKPD(uint32_t &d,const XMM_Reg &s) {
	d = (uint32_t)(s.u64[0] >> 63u) | ((uint32_t)(s.u64[1] >> 63u) << 1u);
}

SSE2_HELPER void SSE2_COMISD(const XMM_Reg &d,const XMM_Reg &s) {
	FillFlags();
	reg_flags &= ~(FLAG_CF|FLAG_PF|FLAG_AF|FLAG_ZF|FLAG_SF|FLAG_OF);

	if (isnan(d.f64[0].v) || isnan(s.f64[0].v))
		reg_flags |= FLAG_ZF|FLAG_PF|FLAG_CF; /* unordered compare */
	else if (d.f64[0].v == s.f64[0].v)
		reg_flags |= FLAG_ZF;
	else if (d.f64[0].v > s.f64[0].v)
		{ /* no change */ }
	else /* d < s */
		reg_flags |= FLAG_CF;
}

SSE2_HELPER uint64_t SSE2_CMP_i(const double a,const double b,const uint8_t cf) {
	bool r = false;
	switch (cf) {
		case 0:/*EQ*/	r = (a == b); break;
		case 1:/*LT*/	r = (a <  b); break;
		case 2:/*LE*/	r = (a <= b); break;
		case 3:/*UNORD*/r = isnan(a) || isnan(b); break;
		case 4:/*NEQ*/	r = !(a == b); break;
		case 5:/*NLT*/	r = !(a <  b); break;
		case 6:/*NLE*/	r = !(a <= b); break;
		case 7:/*ORD*/	r = !isnan(a) && !isnan(b); break;
	}
	return r ? ~(uint64_t)0 : (uint64_t)0;
}

SSE2_HELPER void SSE2_CMPPD(XMM_Reg &d,const XMM_Reg &s,const uint8_t cf) {
	d.u64[0] = SSE2_CMP_i(d.f64[0].v,s.f64[0].v,cf);
	d.u64[1] = SSE2_CMP_i(d.f64[1].v,s.f64[1].v,cf);
}

SSE2_HELPER void SSE2_CMPSD(XMM_Reg &d,const XMM_Reg &s,const uint8_t cf) {
	d.u64[0] = SSE2_CMP_i(d.f64[0].v,s.f64[0].v,cf);
}

SSE2_HELPER void SSE2_CVTSI2SD(XMM_Reg &d,const uint32_t s) {
	d.f64[0].v = (double)(int32_t)s;
}

/* double -> int32; NaN and out of range give the "integer indefinite" 0x80000000;
 * CVTSD2SI rounds as MXCSR.RC says (0 nearest even, 1 down, 2 up, 3 toward zero), CVTTSD2SI truncates */
SSE2_HELPER void SSE2_CVT_i32(uint32_t &d,const double v,const bool truncate) {
	double r;
	if (truncate) r = trunc(v);
	else switch ((fpu.mxcsr >> 13u) & 3u) {
		case 1:		r = floor(v); break;
		case 2:		r = ceil(v); break;
		case 3:		r = trunc(v); break;
		default:	r = nearbyint(v); break;
	}
	if (isnan(r) || r >= 2147483648.0 || r < -2147483648.0) d = 0x80000000u;
	else d = (uint32_t)(int32_t)r;
}

SSE2_HELPER void SSE2_CVTTSD2SI(uint32_t &d,const XMM_Reg &s) {
	SSE2_CVT_i32(d,s.f64[0].v,true);
}

SSE2_HELPER void SSE2_CVTSD2SI(uint32_t &d,const XMM_Reg &s) {
	SSE2_CVT_i32(d,s.f64[0].v,false);
}

SSE2_HELPER void SSE2_CVTSS2SD(XMM_Reg &d,const XMM_Reg &s) {
	d.f64[0].v = (double)s.f32[0].v;
}

SSE2_HELPER void SSE2_CVTSD2SS(XMM_Reg &d,const XMM_Reg &s) {
	d.f32[0].v = (float)s.f64[0].v;
}

SSE2_HELPER void SSE2_CVTPS2PD(XMM_Reg &d,const XMM_Reg &s) {
	const double a = (double)s.f32[0].v, b = (double)s.f32[1].v;
	d.f64[0].v = a;
	d.f64[1].v = b;
}

SSE2_HELPER void SSE2_CVTPD2PS(XMM_Reg &d,const XMM_Reg &s) {
	const float a = (float)s.f64[0].v, b = (float)s.f64[1].v;
	d.f32[0].v = a;
	d.f32[1].v = b;
	d.u64[1] = 0;
}

////

//// SSE2: integer XMM

template <typename T,typename F> SSE2_HELPER void SSE2_LANES(XMM_Reg &d,const XMM_Reg &s,F f) {
	T a[16/sizeof(T)],b[16/sizeof(T)];
	memcpy((void*)a,(const void*)&d,16);
	memcpy((void*)b,(const void*)&s,16);
	for (unsigned int i=0;i < 16/sizeof(T);i++) a[i] = (T)f(a[i],b[i]);
	memcpy((void*)&d,(const void*)a,16);
}

#define SSE2_L(T,EXPR) [](T a,T b) -> T { (void)a; (void)b; return (T)(EXPR); }

/* interleave the low (or high) halves of d and s */
template <typename T> SSE2_HELPER void SSE2_UNPACK(XMM_Reg &d,const XMM_Reg &s,const bool high) {
	const unsigned int n = 16/sizeof(T), off = high ? n/2 : 0;
	T a[16/sizeof(T)],b[16/sizeof(T)],r[16/sizeof(T)];
	memcpy((void*)a,(const void*)&d,16);
	memcpy((void*)b,(const void*)&s,16);
	for (unsigned int i=0;i < n/2;i++) { r[2*i] = a[off+i]; r[2*i+1] = b[off+i]; }
	memcpy((void*)&d,(const void*)r,16);
}

/* narrow with saturation: the elements of d, then those of s */
template <typename S,typename D> SSE2_HELPER void SSE2_PACK(XMM_Reg &d,const XMM_Reg &s,const int64_t lo,const int64_t hi) {
	const unsigned int n = 16/sizeof(S);
	S a[16/sizeof(S)],b[16/sizeof(S)];
	D r[16/sizeof(D)];
	memcpy((void*)a,(const void*)&d,16);
	memcpy((void*)b,(const void*)&s,16);
	for (unsigned int i=0;i < n;i++) {
		int64_t x = (int64_t)a[i], y = (int64_t)b[i];
		r[i]   = (D)(x < lo ? lo : (x > hi ? hi : x));
		r[n+i] = (D)(y < lo ? lo : (y > hi ? hi : y));
	}
	memcpy((void*)&d,(const void*)r,16);
}

/* kind: 0 left, 1 logical right, 2 arithmetic right; a count above the width gives 0 (or the sign) */
template <typename U,typename S> SSE2_HELPER void SSE2_SHIFT(XMM_Reg &d,const uint64_t count,const int kind) {
	const unsigned int bits = sizeof(U)*8u;
	U a[16/sizeof(U)];
	memcpy((void*)a,(const void*)&d,16);
	for (unsigned int i=0;i < 16/sizeof(U);i++) {
		if (kind == 2) a[i] = (U)(((S)a[i]) >> (count >= bits ? bits-1u : (unsigned int)count));
		else if (count >= bits) a[i] = 0;
		else if (kind == 0) a[i] = (U)(a[i] << (unsigned int)count);
		else a[i] = (U)(a[i] >> (unsigned int)count);
	}
	memcpy((void*)&d,(const void*)a,16);
}

SSE2_HELPER void SSE2_BYTESHIFT(XMM_Reg &d,const unsigned int n,const bool left) {
	uint8_t r[16];
	for (unsigned int i=0;i < 16;i++) {
		const int j = left ? (int)i - (int)n : (int)i + (int)n;
		r[i] = (n < 16 && j >= 0 && j < 16) ? d.u8[j] : 0;
	}
	memcpy((void*)&d,(const void*)r,16);
}

/* 66 0F 71/72/73 /op imm8: shifts by an immediate; false for an invalid /op */
SSE2_HELPER bool SSE2_PSHIFTI(const uint8_t opc,const uint8_t op,XMM_Reg &x,const uint8_t n) {
	switch (opc) {
		case 0x71:
			if (op == 2) SSE2_SHIFT<uint16_t,int16_t>(x,n,1);
			else if (op == 4) SSE2_SHIFT<uint16_t,int16_t>(x,n,2);
			else if (op == 6) SSE2_SHIFT<uint16_t,int16_t>(x,n,0);
			else return false;
			return true;
		case 0x72:
			if (op == 2) SSE2_SHIFT<uint32_t,int32_t>(x,n,1);
			else if (op == 4) SSE2_SHIFT<uint32_t,int32_t>(x,n,2);
			else if (op == 6) SSE2_SHIFT<uint32_t,int32_t>(x,n,0);
			else return false;
			return true;
		case 0x73:
			if (op == 2) SSE2_SHIFT<uint64_t,int64_t>(x,n,1);
			else if (op == 6) SSE2_SHIFT<uint64_t,int64_t>(x,n,0);
			else if (op == 3) SSE2_BYTESHIFT(x,n,false);
			else if (op == 7) SSE2_BYTESHIFT(x,n,true);
			else return false;
			return true;
	}
	return false;
}

/* 66 0F xx reg, xmm/m128 of the opcodes that exist as MMX: false for anything else */
SSE2_HELPER bool SSE2_PINT(const uint8_t op,XMM_Reg &d,const XMM_Reg &s) {
	switch (op) {
		case 0x60: SSE2_UNPACK<uint8_t>(d,s,false); break;
		case 0x61: SSE2_UNPACK<uint16_t>(d,s,false); break;
		case 0x62: SSE2_UNPACK<uint32_t>(d,s,false); break;
		case 0x6C: SSE2_UNPACK<uint64_t>(d,s,false); break;
		case 0x68: SSE2_UNPACK<uint8_t>(d,s,true); break;
		case 0x69: SSE2_UNPACK<uint16_t>(d,s,true); break;
		case 0x6A: SSE2_UNPACK<uint32_t>(d,s,true); break;
		case 0x6D: SSE2_UNPACK<uint64_t>(d,s,true); break;
		case 0x63: SSE2_PACK<int16_t,int8_t>(d,s,-128,127); break;
		case 0x67: SSE2_PACK<int16_t,uint8_t>(d,s,0,255); break;
		case 0x6B: SSE2_PACK<int32_t,int16_t>(d,s,-32768,32767); break;
		case 0x64: SSE2_LANES<uint8_t>(d,s,SSE2_L(uint8_t,((int8_t)a > (int8_t)b) ? 0xFF : 0)); break;
		case 0x65: SSE2_LANES<uint16_t>(d,s,SSE2_L(uint16_t,((int16_t)a > (int16_t)b) ? 0xFFFF : 0)); break;
		case 0x66: SSE2_LANES<uint32_t>(d,s,SSE2_L(uint32_t,((int32_t)a > (int32_t)b) ? 0xFFFFFFFFu : 0)); break;
		case 0x74: SSE2_LANES<uint8_t>(d,s,SSE2_L(uint8_t,(a == b) ? 0xFF : 0)); break;
		case 0x75: SSE2_LANES<uint16_t>(d,s,SSE2_L(uint16_t,(a == b) ? 0xFFFF : 0)); break;
		case 0x76: SSE2_LANES<uint32_t>(d,s,SSE2_L(uint32_t,(a == b) ? 0xFFFFFFFFu : 0)); break;
		case 0xD1: SSE2_SHIFT<uint16_t,int16_t>(d,s.u64[0],1); break;
		case 0xD2: SSE2_SHIFT<uint32_t,int32_t>(d,s.u64[0],1); break;
		case 0xD3: SSE2_SHIFT<uint64_t,int64_t>(d,s.u64[0],1); break;
		case 0xE1: SSE2_SHIFT<uint16_t,int16_t>(d,s.u64[0],2); break;
		case 0xE2: SSE2_SHIFT<uint32_t,int32_t>(d,s.u64[0],2); break;
		case 0xF1: SSE2_SHIFT<uint16_t,int16_t>(d,s.u64[0],0); break;
		case 0xF2: SSE2_SHIFT<uint32_t,int32_t>(d,s.u64[0],0); break;
		case 0xF3: SSE2_SHIFT<uint64_t,int64_t>(d,s.u64[0],0); break;
		case 0xD4: SSE2_LANES<uint64_t>(d,s,SSE2_L(uint64_t,a + b)); break;
		case 0xFB: SSE2_LANES<uint64_t>(d,s,SSE2_L(uint64_t,a - b)); break;
		case 0xFC: SSE2_LANES<uint8_t>(d,s,SSE2_L(uint8_t,a + b)); break;
		case 0xFD: SSE2_LANES<uint16_t>(d,s,SSE2_L(uint16_t,a + b)); break;
		case 0xFE: SSE2_LANES<uint32_t>(d,s,SSE2_L(uint32_t,a + b)); break;
		case 0xF8: SSE2_LANES<uint8_t>(d,s,SSE2_L(uint8_t,a - b)); break;
		case 0xF9: SSE2_LANES<uint16_t>(d,s,SSE2_L(uint16_t,a - b)); break;
		case 0xFA: SSE2_LANES<uint32_t>(d,s,SSE2_L(uint32_t,a - b)); break;
		case 0xDC: SSE2_LANES<uint8_t>(d,s,SSE2_L(uint8_t,(a + b > 0xFF) ? 0xFF : a + b)); break;
		case 0xDD: SSE2_LANES<uint16_t>(d,s,SSE2_L(uint16_t,(a + b > 0xFFFF) ? 0xFFFF : a + b)); break;
		case 0xD8: SSE2_LANES<uint8_t>(d,s,SSE2_L(uint8_t,(a > b) ? a - b : 0)); break;
		case 0xD9: SSE2_LANES<uint16_t>(d,s,SSE2_L(uint16_t,(a > b) ? a - b : 0)); break;
		case 0xEC: SSE2_LANES<uint8_t>(d,s,SSE2_L(uint8_t,[](int x){ return x < -128 ? -128 : (x > 127 ? 127 : x); }((int)(int8_t)a + (int)(int8_t)b))); break;
		case 0xED: SSE2_LANES<uint16_t>(d,s,SSE2_L(uint16_t,[](int x){ return x < -32768 ? -32768 : (x > 32767 ? 32767 : x); }((int)(int16_t)a + (int)(int16_t)b))); break;
		case 0xE8: SSE2_LANES<uint8_t>(d,s,SSE2_L(uint8_t,[](int x){ return x < -128 ? -128 : (x > 127 ? 127 : x); }((int)(int8_t)a - (int)(int8_t)b))); break;
		case 0xE9: SSE2_LANES<uint16_t>(d,s,SSE2_L(uint16_t,[](int x){ return x < -32768 ? -32768 : (x > 32767 ? 32767 : x); }((int)(int16_t)a - (int)(int16_t)b))); break;
		case 0xD5: SSE2_LANES<uint16_t>(d,s,SSE2_L(uint16_t,(int32_t)(int16_t)a * (int32_t)(int16_t)b)); break;
		case 0xE5: SSE2_LANES<uint16_t>(d,s,SSE2_L(uint16_t,((int32_t)(int16_t)a * (int32_t)(int16_t)b) >> 16)); break;
		case 0xDB: SSE2_LANES<uint64_t>(d,s,SSE2_L(uint64_t,a & b)); break;
		case 0xDF: SSE2_LANES<uint64_t>(d,s,SSE2_L(uint64_t,~a & b)); break;
		case 0xEB: SSE2_LANES<uint64_t>(d,s,SSE2_L(uint64_t,a | b)); break;
		case 0xEF: SSE2_LANES<uint64_t>(d,s,SSE2_L(uint64_t,a ^ b)); break;
		case 0xF4: {	/* PMULUDQ: dwords 0 and 2 */
			const uint64_t r0 = (uint64_t)d.u32[0] * s.u32[0], r1 = (uint64_t)d.u32[2] * s.u32[2];
			d.u64[0] = r0; d.u64[1] = r1;
			} break;
		case 0xF5: {	/* PMADDWD */
			uint32_t r[4];
			for (unsigned int i=0;i < 4;i++)
				r[i] = (uint32_t)((int64_t)d.i16[2*i] * s.i16[2*i] + (int64_t)d.i16[2*i+1] * s.i16[2*i+1]);
			for (unsigned int i=0;i < 4;i++) d.u32[i] = r[i];
			} break;
		default: return false;
	}
	return true;
}

SSE2_HELPER void SSE2_PSHUFD(XMM_Reg &d,const XMM_Reg &s,const uint8_t i) {
	XMM_Reg t;
	for (unsigned int k=0;k < 4;k++) t.u32[k] = s.u32[(i >> (2u*k)) & 3u];
	d = t;
}

SSE2_HELPER void SSE2_PSHUFLW(XMM_Reg &d,const XMM_Reg &s,const uint8_t i) {
	XMM_Reg t = s;
	for (unsigned int k=0;k < 4;k++) t.u16[k] = s.u16[(i >> (2u*k)) & 3u];
	d = t;
}

SSE2_HELPER void SSE2_PSHUFHW(XMM_Reg &d,const XMM_Reg &s,const uint8_t i) {
	XMM_Reg t = s;
	for (unsigned int k=0;k < 4;k++) t.u16[4+k] = s.u16[4 + ((i >> (2u*k)) & 3u)];
	d = t;
}

////

//// SSE2: integer <-> floating point conversions of XMM and MMX registers

SSE2_HELPER void SSE2_CVTDQ2PS(XMM_Reg &d,const XMM_Reg &s) {
	for (unsigned int i=0;i < 4;i++) d.f32[i].v = (float)s.i32[i];
}

SSE2_HELPER void SSE2_CVTPS2DQ_i(XMM_Reg &d,const XMM_Reg &s,const bool truncate) {
	uint32_t r[4];
	for (unsigned int i=0;i < 4;i++) SSE2_CVT_i32(r[i],(double)s.f32[i].v,truncate);
	for (unsigned int i=0;i < 4;i++) d.u32[i] = r[i];
}

SSE2_HELPER void SSE2_CVTPS2DQ(XMM_Reg &d,const XMM_Reg &s) { SSE2_CVTPS2DQ_i(d,s,false); }
SSE2_HELPER void SSE2_CVTTPS2DQ(XMM_Reg &d,const XMM_Reg &s) { SSE2_CVTPS2DQ_i(d,s,true); }

SSE2_HELPER void SSE2_CVTDQ2PD(XMM_Reg &d,const XMM_Reg &s) {
	const double a = (double)s.i32[0], b = (double)s.i32[1];
	d.f64[0].v = a;
	d.f64[1].v = b;
}

SSE2_HELPER void SSE2_CVTPD2DQ_i(XMM_Reg &d,const XMM_Reg &s,const bool truncate) {
	uint32_t a,b;
	SSE2_CVT_i32(a,s.f64[0].v,truncate);
	SSE2_CVT_i32(b,s.f64[1].v,truncate);
	d.u32[0] = a;
	d.u32[1] = b;
	d.u64[1] = 0;
}

SSE2_HELPER void SSE2_CVTPD2DQ(XMM_Reg &d,const XMM_Reg &s) { SSE2_CVTPD2DQ_i(d,s,false); }
SSE2_HELPER void SSE2_CVTTPD2DQ(XMM_Reg &d,const XMM_Reg &s) { SSE2_CVTPD2DQ_i(d,s,true); }

SSE2_HELPER void SSE2_CVTPI2PD(XMM_Reg &d,const MMX_reg &s) {
	d.f64[0].v = (double)s.sd.d0;
	d.f64[1].v = (double)s.sd.d1;
}

SSE2_HELPER void SSE2_CVTPD2PI_i(MMX_reg &d,const XMM_Reg &s,const bool truncate) {
	uint32_t a,b;
	SSE2_CVT_i32(a,s.f64[0].v,truncate);
	SSE2_CVT_i32(b,s.f64[1].v,truncate);
	d.ud.d0 = a;
	d.ud.d1 = b;
	d.fpu.e = 0xFFFF;
}

SSE2_HELPER void SSE2_CVTPD2PI(MMX_reg &d,const XMM_Reg &s) { SSE2_CVTPD2PI_i(d,s,false); }
SSE2_HELPER void SSE2_CVTTPD2PI(MMX_reg &d,const XMM_Reg &s) { SSE2_CVTPD2PI_i(d,s,true); }
#undef SSE2_HELPER

////

static INLINE void MMX_PINSRW(MMX_reg &d,const uint32_t &s,const uint8_t i) {
	d.uwa[i&3u] = (uint16_t)s;
	d.fpu.e = 0xFFFF;
}

static INLINE void SSE_PINSRW(XMM_Reg &d,const uint32_t &s,const uint8_t i) {
	d.u16[i&7u] = (uint16_t)s;
}

////

static INLINE void MMX_PEXTRW(uint32_t &d,const MMX_reg &s,const uint8_t i) {
	d = s.uwa[i&3u];
}

static INLINE void SSE_PEXTRW(uint32_t &d,const XMM_Reg &s,const uint8_t i) {
	d = s.u16[i&7u];
}

////

static INLINE void SSE_SHUFPS(XMM_Reg &d,const XMM_Reg &s,const uint8_t i) {
	/* d is reg, s is rm */
	XMM_Reg tmp;

	tmp.u32[0] = d.u32[(i>>0u)&3u];
	tmp.u32[1] = d.u32[(i>>2u)&3u];
	tmp.u32[2] = s.u32[(i>>4u)&3u];
	tmp.u32[3] = s.u32[(i>>6u)&3u];
	d = tmp;
}

////

static INLINE void MMX_PMOVMSKB(uint32_t &d,const MMX_reg &s) {
	d =
		((s.ub.b7 & 0x80u) ? 0x80 : 0x00) |
		((s.ub.b6 & 0x80u) ? 0x40 : 0x00) |
		((s.ub.b5 & 0x80u) ? 0x20 : 0x00) |
		((s.ub.b4 & 0x80u) ? 0x10 : 0x00) |
		((s.ub.b3 & 0x80u) ? 0x08 : 0x00) |
		((s.ub.b2 & 0x80u) ? 0x04 : 0x00) |
		((s.ub.b1 & 0x80u) ? 0x02 : 0x00) |
		((s.ub.b0 & 0x80u) ? 0x01 : 0x00);
}

static INLINE void SSE_PMOVMSKB(uint32_t &d,const XMM_Reg &s) {
	d =
		((s.u8[15] & 0x80u) ? 0x8000 : 0x0000) |
		((s.u8[14] & 0x80u) ? 0x4000 : 0x0000) |
		((s.u8[13] & 0x80u) ? 0x2000 : 0x0000) |
		((s.u8[12] & 0x80u) ? 0x1000 : 0x0000) |
		((s.u8[11] & 0x80u) ? 0x0800 : 0x0000) |
		((s.u8[10] & 0x80u) ? 0x0400 : 0x0000) |
		((s.u8[ 9] & 0x80u) ? 0x0200 : 0x0000) |
		((s.u8[ 8] & 0x80u) ? 0x0100 : 0x0000) |
		((s.u8[ 7] & 0x80u) ? 0x0080 : 0x0000) |
		((s.u8[ 6] & 0x80u) ? 0x0040 : 0x0000) |
		((s.u8[ 5] & 0x80u) ? 0x0020 : 0x0000) |
		((s.u8[ 4] & 0x80u) ? 0x0010 : 0x0000) |
		((s.u8[ 3] & 0x80u) ? 0x0008 : 0x0000) |
		((s.u8[ 2] & 0x80u) ? 0x0004 : 0x0000) |
		((s.u8[ 1] & 0x80u) ? 0x0002 : 0x0000) |
		((s.u8[ 0] & 0x80u) ? 0x0001 : 0x0000);
}

////

static INLINE void MMX_PMINUB(MMX_reg &d,MMX_reg &s) {
#define STEP(i) d.ub.b##i = std::min(d.ub.b##i,s.ub.b##i)
	STEP(0);
	STEP(1);
	STEP(2);
	STEP(3);
	STEP(4);
	STEP(5);
	STEP(6);
	STEP(7);
	d.fpu.e = 0xFFFF;
#undef STEP
}

static INLINE void SSE_PMINUB(XMM_Reg &d,XMM_Reg &s) {
#define STEP(i) d.u8[i] = std::min(d.u8[i],s.u8[i])
	STEP(0);
	STEP(1);
	STEP(2);
	STEP(3);
	STEP(4);
	STEP(5);
	STEP(6);
	STEP(7);
	STEP(8);
	STEP(9);
	STEP(10);
	STEP(11);
	STEP(12);
	STEP(13);
	STEP(14);
	STEP(15);
#undef STEP
}

////

static INLINE void MMX_PMAXUB(MMX_reg &d,MMX_reg &s) {
#define STEP(i) d.ub.b##i = std::max(d.ub.b##i,s.ub.b##i)
	STEP(0);
	STEP(1);
	STEP(2);
	STEP(3);
	STEP(4);
	STEP(5);
	STEP(6);
	STEP(7);
	d.fpu.e = 0xFFFF;
#undef STEP
}

static INLINE void SSE_PMAXUB(XMM_Reg &d,XMM_Reg &s) {
#define STEP(i) d.u8[i] = std::max(d.u8[i],s.u8[i])
	STEP(0);
	STEP(1);
	STEP(2);
	STEP(3);
	STEP(4);
	STEP(5);
	STEP(6);
	STEP(7);
	STEP(8);
	STEP(9);
	STEP(10);
	STEP(11);
	STEP(12);
	STEP(13);
	STEP(14);
	STEP(15);
#undef STEP
}

////

static INLINE void MMX_PAVGB(MMX_reg &d,MMX_reg &s) {
#define STEP(i) d.ub.b##i = (uint8_t)(((uint16_t)(d.ub.b##i) + (uint16_t)(s.ub.b##i) + 1u) >> 1u)
	STEP(0);
	STEP(1);
	STEP(2);
	STEP(3);
	STEP(4);
	STEP(5);
	STEP(6);
	STEP(7);
	d.fpu.e = 0xFFFF;
#undef STEP
}

static INLINE void SSE_PAVGB(XMM_Reg &d,XMM_Reg &s) {
#define STEP(i) d.u8[i] = (uint8_t)(((uint16_t)(d.u8[i]) + (uint16_t)(s.u8[i]) + 1u) >> 1u)
	STEP(0);
	STEP(1);
	STEP(2);
	STEP(3);
	STEP(4);
	STEP(5);
	STEP(6);
	STEP(7);
	STEP(8);
	STEP(9);
	STEP(10);
	STEP(11);
	STEP(12);
	STEP(13);
	STEP(14);
	STEP(15);
#undef STEP
}

////

static INLINE void MMX_PAVGW(MMX_reg &d,MMX_reg &s) {
#define STEP(i) d.uw.w##i = (uint16_t)(((uint32_t)(d.uw.w##i) + (uint32_t)(s.uw.w##i) + 1u) >> 1u)
	STEP(0);
	STEP(1);
	STEP(2);
	STEP(3);
	d.fpu.e = 0xFFFF;
#undef STEP
}

static INLINE void SSE_PAVGW(XMM_Reg &d,XMM_Reg &s) {
#define STEP(i) d.u16[i] = (uint16_t)(((uint32_t)(d.u16[i]) + (uint32_t)(s.u16[i]) + 1u) >> 1u)
	STEP(0);
	STEP(1);
	STEP(2);
	STEP(3);
	STEP(4);
	STEP(5);
	STEP(6);
	STEP(7);
#undef STEP
}

////

static INLINE void MMX_PMULHUW(MMX_reg &d,MMX_reg &s) {
#define STEP(i) d.uwa[i] = (uint16_t)(((uint32_t)(d.uwa[i]) * (uint32_t)(s.uwa[i])) >> (uint32_t)16u)
	STEP(0);
	STEP(1);
	STEP(2);
	STEP(3);
	d.fpu.e = 0xFFFF;
#undef STEP
}

static INLINE void SSE_PMULHUW(XMM_Reg &d,XMM_Reg &s) {
#define STEP(i) d.u16[i] = (uint16_t)(((uint32_t)(d.u16[i]) * (uint32_t)(s.u16[i])) >> (uint32_t)16u)
	STEP(0);
	STEP(1);
	STEP(2);
	STEP(3);
	STEP(4);
	STEP(5);
	STEP(6);
	STEP(7);
#undef STEP
}

////

static INLINE void MMX_PMINSW(MMX_reg &d,MMX_reg &s) {
#define STEP(i) d.sw.w##i = std::min(d.sw.w##i,s.sw.w##i)
	STEP(0);
	STEP(1);
	STEP(2);
	STEP(3);
	d.fpu.e = 0xFFFF;
#undef STEP
}

static INLINE void SSE_PMINSW(XMM_Reg &d,XMM_Reg &s) {
#define STEP(i) d.i16[i] = std::min(d.i16[i],s.i16[i])
	STEP(0);
	STEP(1);
	STEP(2);
	STEP(3);
	STEP(4);
	STEP(5);
	STEP(6);
	STEP(7);
#undef STEP
}

////

static INLINE void MMX_PMAXSW(MMX_reg &d,MMX_reg &s) {
#define STEP(i) d.sw.w##i = std::max(d.sw.w##i,s.sw.w##i)
	STEP(0);
	STEP(1);
	STEP(2);
	STEP(3);
	d.fpu.e = 0xFFFF;
#undef STEP
}

static INLINE void SSE_PMAXSW(XMM_Reg &d,XMM_Reg &s) {
#define STEP(i) d.i16[i] = std::max(d.i16[i],s.i16[i])
	STEP(0);
	STEP(1);
	STEP(2);
	STEP(3);
	STEP(4);
	STEP(5);
	STEP(6);
	STEP(7);
#undef STEP
}

////

static INLINE void MMX_PSADBW(MMX_reg &d,MMX_reg &s) {
#define STEP(i) (uint16_t)abs((int16_t)(d.ub.b##i) - (int16_t)(s.ub.b##i))
	d.uw.w0 = STEP(0) + STEP(1) + STEP(2) + STEP(3) + STEP(4) + STEP(5) + STEP(6) + STEP(7);
	d.uw.w1 = d.uw.w2 = d.uw.w3 = 0;
	d.fpu.e = 0xFFFF;
#undef STEP
}

static INLINE void SSE_PSADBW(XMM_Reg &d,XMM_Reg &s) {
#define STEP(i) (uint16_t)abs((int16_t)(d.u8[i]) - (int16_t)(s.u8[i]))
	d.u16[0] = STEP(0) + STEP(1) + STEP(2) + STEP(3) + STEP(4) + STEP(5) + STEP(6) + STEP(7);
    d.u16[4] = STEP(8) + STEP(9) + STEP(10) + STEP(11) + STEP(12) + STEP(13) + STEP(14) + STEP(15);
	d.u16[1] = d.u16[2] = d.u16[3] = d.u16[5] = d.u16[6] = d.u16[7] = 0;
#undef STEP
}

#endif // 386+

#define SETcc(cc)							\
	{								\
		GetRM;							\
		if (rm >= 0xc0 ) {GetEArb;*earb=(cc) ? 1 : 0;}		\
		else {GetEAa;SaveMb(eaa,(cc) ? 1 : 0);}			\
	}

void CPU_FXSAVE(PhysPt eaa);
void CPU_FXRSTOR(PhysPt eaa);
bool CPU_LDMXCSR(PhysPt eaa);
bool CPU_STMXCSR(PhysPt eaa);

typedef PhysPt (*EA_LookupHandler)(void);

#include "helpers.h"
#if CPU_CORE <= CPU_ARCHTYPE_8086
# define EATable EATable8086
# include "table_ea_8086.h"
#else
# ifndef CPU_OMIT_8086
#  include "table_ea_8086.h"
# endif
# include "table_ea.h"
#endif
#include "../modrm.h"
