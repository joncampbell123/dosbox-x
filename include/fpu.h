/*
 *  Copyright (C) 2002-2019  The DOSBox Team
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335, USA.
 */

#ifndef DOSBOX_FPU_H
#define DOSBOX_FPU_H

#ifndef DOSBOX_MEM_H
#include "mem.h"
#endif

#include "mmx.h"

void FPU_ESC0_Normal(Bitu rm);
void FPU_ESC0_EA(Bitu rm,PhysPt addr);
void FPU_ESC1_Normal(Bitu rm);
void FPU_ESC1_EA(Bitu rm,PhysPt addr);
void FPU_ESC2_Normal(Bitu rm);
void FPU_ESC2_EA(Bitu rm,PhysPt addr);
void FPU_ESC3_Normal(Bitu rm);
void FPU_ESC3_EA(Bitu rm,PhysPt addr);
void FPU_ESC4_Normal(Bitu rm);
void FPU_ESC4_EA(Bitu rm,PhysPt addr);
void FPU_ESC5_Normal(Bitu rm);
void FPU_ESC5_EA(Bitu rm,PhysPt addr);
void FPU_ESC6_Normal(Bitu rm);
void FPU_ESC6_EA(Bitu rm,PhysPt addr);
void FPU_ESC7_Normal(Bitu rm);
void FPU_ESC7_EA(Bitu rm,PhysPt addr);

/* Floating point register, in the form the native host uses for "double".
 * This is slightly less precise than the 80-bit extended IEEE used by Intel,
 * but can be faster using the host processor "double" support. Most DOS games
 * using the FPU for 3D rendering are unaffected by the loss of precision.
 * However, there are cases where the full 80-bit precision is required such
 * as the "Fast Pentium memcpy trick" using the 80-bit versions of FLD/FST to
 * copy memory. */
typedef union {
    double d;
#ifndef WORDS_BIGENDIAN
    struct {
        Bit32u lower;
        Bit32s upper;
    } l;
#else
    struct {
        Bit32s upper;
        Bit32u lower;
    } l;
#endif
    Bit64s ll;
	MMX_reg reg_mmx;
} FPU_Reg;

// dynamic x86 core needs this
typedef struct {
    Bit32u m1;
    Bit32u m2;
    Bit16u m3;

    Bit16u d1;
    Bit32u d2;
} FPU_P_Reg;

// memory barrier macro. to ensure that reads/stores to one half of the FPU reg struct
// do not overlap with reads/stores from the other half. things can go wrong if the
// compiler writes code to write the mantissa, then load the overall as float, then store
// the exponent. note this is not a hardware level memory barrier, this is a compiler
// level memory barrier against the optimization engine.
#if defined(__GCC__)
# define FPU_Reg_m_barrier()	__asm__ __volatile__ ("":::"memory")
#else
# define FPU_Reg_m_barrier()
#endif

#pragma pack(push,1)
typedef union {
// TODO: The configure script needs to use "long double" on x86/x86_64 and verify sizeof(long double) == 10,
//       else undef a macro to let the code emulate long double 80-bit IEEE. Also needs to determine host
//       byte order here so host long double matches our struct.
	struct {
		uint64_t	mantissa;		// [63:0]
		unsigned int	exponent:15;		// [78:64]
		unsigned int	sign:1;			// [79:79]
	} f;
#if defined(HAS_LONG_DOUBLE)
	long double		v;			// [79:0]
#endif
	struct {
		uint64_t	l;
		uint16_t	h;
	} raw;
} FPU_Reg_80;
// ^ Remember that in 80-bit extended, the mantissa contains both the fraction and integer bit. There is no
//   "implied bit" like 32-bit and 64-bit formats.
#pragma pack(pop)

#define FPU_Reg_80_exponent_bias	(16383)

#pragma pack(push,1)
typedef union {
	struct {
		uint64_t	mantissa:52;		// [51:0]
		uint64_t	exponent:11;		// [62:52]
		uint64_t	sign:1;			// [63:63]
	} f;
	double			v;
	uint64_t		raw;
} FPU_Reg_64;
#pragma pack(pop)

#define FPU_Reg_64_exponent_bias	(1023)
static const uint64_t FPU_Reg_64_implied_bit = ((uint64_t)1ULL << (uint64_t)52ULL);

#pragma pack(push,1)
typedef union {
	struct {
		uint32_t	mantissa:23;		// [22:0]
		uint32_t	exponent:8;		// [30:23]
		uint32_t	sign:1;			// [31:31]
	} f;
	float			v;
	uint32_t		raw;
} FPU_Reg_32;
#pragma pack(pop)

#define FPU_Reg_32_exponent_bias	(127)
static const uint32_t FPU_Reg_32_implied_bit = ((uint32_t)1UL << (uint32_t)23UL);

enum FPU_Tag {
	TAG_Valid = 0,
	TAG_Zero  = 1,
	TAG_Weird = 2,
	TAG_Empty = 3
};

enum FPU_Round {
	ROUND_Nearest = 0,		
	ROUND_Down    = 1,
	ROUND_Up      = 2,	
	ROUND_Chop    = 3
};

typedef struct {
#if defined(HAS_LONG_DOUBLE)//probably shouldn't allow struct to change size based on this
	FPU_Reg		_do_not_use__regs[9];
#else
	FPU_Reg		regs[9];
#endif
	FPU_P_Reg	p_regs[9];
	FPU_Reg_80	regs_80[9];
#if defined(HAS_LONG_DOUBLE)//probably shouldn't allow struct to change size based on this
	bool		_do_not_use__use80[9];		// if set, use the 80-bit precision version
#else
	bool		use80[9];		// if set, use the 80-bit precision version
#endif
	FPU_Tag		tags[9];
	Bit16u		cw,cw_mask_all;
	Bit16u		sw;
	Bit32u		top;
	FPU_Round	round;
} FPU_rec;


//get pi from a real library
#define PI		3.14159265358979323846
#define L2E		1.4426950408889634
#define L2T		3.3219280948873623
#define LN2		0.69314718055994531
#define LG2		0.3010299956639812


extern FPU_rec fpu;

#define TOP fpu.top
#define STV(i)  ( (fpu.top+ (i) ) & 7 )


Bit16u FPU_GetTag(void);
void FPU_FLDCW(PhysPt addr);

static INLINE void FPU_SetTag(Bit16u tag){
	for(Bitu i=0;i<8;i++)
		fpu.tags[i] = static_cast<FPU_Tag>((tag >>(2*i))&3);
}

static INLINE void FPU_SetCW(Bitu word){
	// HACK: Bits 13-15 are not defined. Apparently, one program likes to test for
	//       Cyrix EMC87 by trying to set bit 15. We want the test program to see
	//       us as an Intel 287 when cputype == 286.
	word &= 0x7FFF;

	fpu.cw = (Bit16u)word;
	fpu.cw_mask_all = (Bit16u)(word | 0x3f);
	fpu.round = (FPU_Round)((word >> 10) & 3);
}


static INLINE Bit8u FPU_GET_TOP(void) {
	return (fpu.sw & 0x3800U) >> 11U;
}

static INLINE void FPU_SET_TOP(Bitu val){
	fpu.sw &= ~0x3800U;
	fpu.sw |= (val & 7U) << 11U;
}


static INLINE void FPU_SET_C0(Bitu C){
	fpu.sw &= ~0x0100U;
	if(C) fpu.sw |=  0x0100U;
}

static INLINE void FPU_SET_C1(Bitu C){
	fpu.sw &= ~0x0200U;
	if(C) fpu.sw |=  0x0200U;
}

static INLINE void FPU_SET_C2(Bitu C){
	fpu.sw &= ~0x0400U;
	if(C) fpu.sw |=  0x0400U;
}

static INLINE void FPU_SET_C3(Bitu C){
	fpu.sw &= ~0x4000U;
	if(C) fpu.sw |= 0x4000U;
}

static INLINE void FPU_SET_D(Bitu C){
	fpu.sw &= ~0x0002U;
	if(C) fpu.sw |= 0x0002U;
}


#endif
