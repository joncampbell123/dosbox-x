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

#ifndef DOSBOX_FPU_TYPES_H
#define DOSBOX_FPU_TYPES_H

#include <cstddef>
#include <cstdint>

enum class FPUTag
{
	Valid   = 0,
	Zero    = 1,
	Special = 2,
	Empty   = 3
};

#pragma pack(push,1)
typedef union alignas(8) {
	struct {
		uint64_t	mantissa:52;		// [51:0]
		uint64_t	exponent:11;		// [62:52]
		uint64_t	sign:1;			// [63:63]
	} f;
	double			v;
	uint64_t		raw;

	static_assert( sizeof(f) == 8, "FPU_Reg_64 error" );
	static_assert( sizeof(v) == 8, "FPU_Reg_64 error" );
	static_assert( sizeof(raw) == 8, "FPU_Reg_64 error" );
} FPU_Reg_64;
static_assert( sizeof(FPU_Reg_64) == 8, "FPU_Reg_64 error" );
#pragma pack(pop)

#define FPU_Reg_64_exponent_bias	(1023)
static const uint64_t FPU_Reg_64_implied_bit = ((uint64_t)1ULL << (uint64_t)52ULL);

#pragma pack(push,1)
typedef union alignas(4) {
	struct {
		uint32_t	mantissa:23;		// [22:0]
		uint32_t	exponent:8;		// [30:23]
		uint32_t	sign:1;			// [31:31]
	} f;
	float			v;
	uint32_t		raw;

	static_assert( sizeof(f) == 4, "FPU_Reg_32 error" );
	static_assert( sizeof(v) == 4, "FPU_Reg_32 error" );
	static_assert( sizeof(raw) == 4, "FPU_Reg_32 error" );
} FPU_Reg_32;
static_assert( sizeof(FPU_Reg_32) == 4, "FPU_Reg_32 error" );
#pragma pack(pop)

#define FPU_Reg_32_exponent_bias	(127)
static const uint32_t FPU_Reg_32_implied_bit = ((uint32_t)1UL << (uint32_t)23UL);

#pragma pack(push,1)
union alignas(16) MMX_reg {

	uint64_t q;

#ifndef WORDS_BIGENDIAN
	struct fpu_t {
		uint64_t m;
		uint16_t e;
	} fpu;
	static_assert(sizeof(fpu) == 10, "MMX packing error");
	static_assert(offsetof(fpu_t,m) == 0, "MMX packing error");
	static_assert(offsetof(fpu_t,e) == 8, "MMX packing error");

	struct {
		uint32_t d0,d1;
	} ud;
	static_assert(sizeof(ud) == 8, "MMX packing error");

	struct {
		int32_t d0,d1;
	} sd;
	static_assert(sizeof(sd) == 8, "MMX packing error");

	struct uw_t {
		uint16_t w0,w1,w2,w3;
	} uw;
	static_assert(sizeof(uw) == 8, "MMX packing error");

	uint8_t uba[8];
	uint16_t uwa[4];
	uint32_t uda[2];
	static_assert(sizeof(uba) == 8, "MMX packing error");
	static_assert(sizeof(uwa) == 8, "MMX packing error");
	static_assert(sizeof(uda) == 8, "MMX packing error");
	static_assert(offsetof(uw_t,w0) == 0, "MMX packing error");
	static_assert(offsetof(uw_t,w1) == 2, "MMX packing error");
	static_assert(offsetof(uw_t,w2) == 4, "MMX packing error");
	static_assert(offsetof(uw_t,w3) == 6, "MMX packing error");

	struct {
		int16_t w0,w1,w2,w3;
	} sw;
	static_assert(sizeof(sw) == 8, "MMX packing error");

	struct {
		uint8_t b0,b1,b2,b3,b4,b5,b6,b7;
	} ub;
	static_assert(sizeof(ub) == 8, "MMX packing error");

	struct {
		int8_t b0,b1,b2,b3,b4,b5,b6,b7;
	} sb;
	static_assert(sizeof(sb) == 8, "MMX packing error");

	struct alignas(4) { /* MMX registers can contain single precision float if the program uses AMD 3DNow! instructions */
		FPU_Reg_32 f0,f1;
	} f32;
	static_assert(sizeof(f32) == 8, "MMX packing error");
#else
	struct fpu_t {
		uint64_t m;
		uint16_t e;
	} fpu;
	static_assert(sizeof(fpu) == 10, "MMX packing error");
	static_assert(offsetof(fpu_t,m) == 0, "MMX packing error");
	static_assert(offsetof(fpu_t,e) == 8, "MMX packing error");

	struct {
		uint32_t d1,d0;
	} ud;
	static_assert(sizeof(ud) == 8, "MMX packing error");

	struct {
		int32_t d1,d0;
	} sd;
	static_assert(sizeof(sd) == 8, "MMX packing error");

	struct {
		uint16_t w3,w2,w1,w0;
	} uw;
	static_assert(sizeof(uw) == 8, "MMX packing error");

	uint8_t uba[8];
	uint16_t uwa[4];
	uint32_t uda[2];

	struct {
		uint16_t w3,w2,w1,w0;
	} sw;
	static_assert(sizeof(sw) == 8, "MMX packing error");

	struct {
		uint8_t b7,b6,b5,b4,b3,b2,b1,b0;
	} ub;
	static_assert(sizeof(ub) == 8, "MMX packing error");

	struct {
		uint8_t b7,b6,b5,b4,b3,b2,b1,b0;
	} sb;
	static_assert(sizeof(sb) == 8, "MMX packing error");

	struct { /* MMX registers can contain single precision float if the program uses AMD 3DNow! instructions */
		FPU_Reg_32 f1,f0;
	} f32;
	static_assert(sizeof(f32) == 8, "MMX packing error");
#endif

};
static_assert(sizeof(MMX_reg) == 16, "MMX packing error");
#pragma pack(pop)

#pragma pack(push,1)
union alignas(16) XMM_Reg {
	FPU_Reg_32		f32[4];
	FPU_Reg_64		f64[2];

	int8_t			i8[16];
	int16_t			i16[8];
	int32_t			i32[4];
	int64_t			i64[2];

	uint8_t			u8[16];
	uint16_t		u16[8];
	uint32_t		u32[4];
	uint64_t		u64[2];

	static_assert( sizeof(u8)  == 16 /* 128-bit */, "XMM reg struct error" );
	static_assert( sizeof(u16) == 16 /* 128-bit */, "XMM reg struct error" );
	static_assert( sizeof(u32) == 16 /* 128-bit */, "XMM reg struct error" );
	static_assert( sizeof(u64) == 16 /* 128-bit */, "XMM reg struct error" );
	static_assert( sizeof(i8)  == 16 /* 128-bit */, "XMM reg struct error" );
	static_assert( sizeof(i16) == 16 /* 128-bit */, "XMM reg struct error" );
	static_assert( sizeof(i32) == 16 /* 128-bit */, "XMM reg struct error" );
	static_assert( sizeof(i64) == 16 /* 128-bit */, "XMM reg struct error" );
	static_assert( sizeof(f32) == 16 /* 128-bit */, "XMM reg struct error" );
	static_assert( sizeof(f64) == 16 /* 128-bit */, "XMM reg struct error" );
};
static_assert( sizeof(XMM_Reg)     == 16 /* 128-bit */, "XMM reg struct error" );
#pragma pack(pop)

extern MMX_reg * reg_mmx[8];
extern MMX_reg * lookupRMregMM[256];

#pragma pack(push,1)
typedef union alignas(16) {
// TODO: The configure script needs to use "long double" on x86/x86_64 and verify sizeof(long double) == 10,
//       else undef a macro to let the code emulate long double 80-bit IEEE. Also needs to determine host
//       byte order here so host long double matches our struct.
	struct f_t {
		uint64_t	mantissa;		// [63:0]
		uint16_t	exponent:15;	// [78:64]
		uint16_t	sign:1;			// [79:79]
	} f;
#if defined(HAS_LONG_DOUBLE)
	long double		v;			// [79:0]
#endif
	struct raw_t {
		uint64_t	l;
		uint16_t	h;
	} raw;

	MMX_reg reg_mmx;
	static_assert( sizeof(reg_mmx) == 16, "FPU_Reg error" );

	static_assert( offsetof(f_t,mantissa) == 0, "oops" );
	static_assert( offsetof(raw_t,l) == 0, "oops" );
	static_assert( offsetof(raw_t,h) == 8, "oops" );
	static_assert( offsetof(MMX_reg,q) == 0, "oops" );
} FPU_Reg_80;
static_assert( sizeof(FPU_Reg_80) == 16, "FPU_Reg_80 error" );/*NTS: GCC can and often will define long double as 16 bytes or at least align by 16 bytes*/
// ^ Remember that in 80-bit extended, the mantissa contains both the fraction and integer bit. There is no
//   "implied bit" like 32-bit and 64-bit formats.
#pragma pack(pop)

#define FPU_Reg_80_exponent_bias	(16383)

/* Floating point register, in the form the native host uses for "double".
 * This is slightly less precise than the 80-bit extended IEEE used by Intel,
 * but can be faster using the host processor "double" support. Most DOS games
 * using the FPU for 3D rendering are unaffected by the loss of precision.
 * However, there are cases where the full 80-bit precision is required such
 * as the "Fast Pentium memcpy trick" using the 80-bit versions of FLD/FST to
 * copy memory. */
#pragma pack(push,1)
union alignas(8) FPU_Reg {
    double d;
    struct
    {
        uint64_t mantissa:52;       // [51:0]
        uint64_t exponent:11;       // [62:52]
        uint64_t sign:1;            // [63:63]
    } f;
#ifndef WORDS_BIGENDIAN
    struct {
        uint32_t lower;
        int32_t upper;
    } l;
#else
    struct {
        int32_t upper;
        uint32_t lower;
    } l;
#endif
    int64_t ll;

	static_assert( sizeof(d) == 8, "FPU_Reg error" );
	static_assert( sizeof(l) == 8, "FPU_Reg error" );
	static_assert( sizeof(ll) == 8, "FPU_Reg error" );
};
static_assert( sizeof(FPU_Reg) == 8, "FPU_Reg error" );
#pragma pack(pop)

// dynamic x86 core needs this
typedef struct {
    // 80-bit extended float (m2:m1 = 64-bit mantissa  m3 = sign:exponent)
    uint32_t m1;
    uint32_t m2;
    uint16_t m3;
    // Padding to make the structure 16 bytes so the inline asm in fpu_instructions_x86.h can shift by 4 to index FPU registers
    uint16_t d1;
    uint32_t d2;
} FPU_P_Reg;
static_assert( sizeof(FPU_P_Reg) == 16, "FPU_P_Reg error" );

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

#endif // DOSBOX_FPU_TYPES_H
