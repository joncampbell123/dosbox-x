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
	d.u32[0] = d.u32[1] = s.u32[0];
	d.u32[2] = d.u32[3] = s.u32[1];
}

////

static INLINE void SSE_UNPCKHPS(XMM_Reg &d,const XMM_Reg &s) {
	d.u32[0] = d.u32[1] = s.u32[2];
	d.u32[2] = d.u32[3] = s.u32[3];
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

static INLINE void MMX_PINSRW(MMX_reg &d,const uint32_t &s,const uint8_t i) {
	d.uwa[i&3u] = (uint16_t)s;
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

#include "helpers.h"
#if CPU_CORE <= CPU_ARCHTYPE_8086
# include "table_ea_8086.h"
#else
# include "table_ea.h"
#endif
#include "../modrm.h"


