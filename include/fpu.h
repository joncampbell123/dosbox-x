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

#ifndef DOSBOX_FPU_H
#define DOSBOX_FPU_H

#include "cpu.h"
#include "logging.h"
#include "mmx.h"

#include <stddef.h>

void FPU_ESC0_Normal(Bitu rm);
void FPU_ESC0_EA(Bitu rm,PhysPt addr);
void FPU_ESC1_Normal(Bitu rm);
void FPU_ESC1_EA(Bitu rm,PhysPt addr, bool op16);
void FPU_ESC2_Normal(Bitu rm);
void FPU_ESC2_EA(Bitu rm,PhysPt addr);
void FPU_ESC3_Normal(Bitu rm);
void FPU_ESC3_EA(Bitu rm,PhysPt addr);
void FPU_ESC4_Normal(Bitu rm);
void FPU_ESC4_EA(Bitu rm,PhysPt addr);
void FPU_ESC5_Normal(Bitu rm);
void FPU_ESC5_EA(Bitu rm,PhysPt addr, bool op16);
void FPU_ESC6_Normal(Bitu rm);
void FPU_ESC6_EA(Bitu rm,PhysPt addr);
void FPU_ESC7_Normal(Bitu rm);
void FPU_ESC7_EA(Bitu rm,PhysPt addr);

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

union alignas(8) MMX_reg {

	uint64_t q;

#ifndef WORDS_BIGENDIAN
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

	uint16_t uwa[4]; /* for PSHUFW */
	static_assert(sizeof(uwa) == 8, "MMX packing error");
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

	struct { /* MMX registers can contain single precision float if the program uses AMD 3DNow! instructions */
		FPU_Reg_32 f0,f1;
	} f32;
	static_assert(sizeof(f32) == 8, "MMX packing error");
#else
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
static_assert(sizeof(MMX_reg) == 8, "MMX packing error");

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


int8_t  SaturateWordSToByteS(int16_t value);
int16_t SaturateDwordSToWordS(int32_t value);
uint8_t  SaturateWordSToByteU(int16_t value);
uint16_t SaturateDwordSToWordU(int32_t value);

void   setFPUTagEmpty();

/* Floating point register, in the form the native host uses for "double".
 * This is slightly less precise than the 80-bit extended IEEE used by Intel,
 * but can be faster using the host processor "double" support. Most DOS games
 * using the FPU for 3D rendering are unaffected by the loss of precision.
 * However, there are cases where the full 80-bit precision is required such
 * as the "Fast Pentium memcpy trick" using the 80-bit versions of FLD/FST to
 * copy memory. */
#pragma pack(push,1)
typedef union alignas(8) {
    double d;
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
	MMX_reg reg_mmx;

	static_assert( sizeof(d) == 8, "FPU_Reg error" );
	static_assert( sizeof(l) == 8, "FPU_Reg error" );
	static_assert( sizeof(ll) == 8, "FPU_Reg error" );
	static_assert( sizeof(reg_mmx) == 8, "FPU_Reg error" );
} FPU_Reg;
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

enum FPU_Tag {
	TAG_Valid = 0,
	TAG_Zero  = 1,
	TAG_Weird = 2,
	TAG_Empty = 3
};

template<class T, unsigned bitno, unsigned nbits=1>
struct RegBit
{
	enum { basemask = (1 << nbits) - 1 };
	enum { mask = basemask << bitno };
	T data;
	RegBit(const T& reg) : data(reg) {}
	template <class T2> RegBit& operator=(T2 val)
	{
		data = (data & ~mask) | ((nbits > 1 ? val & basemask : !!val) << bitno);
		return *this;
	}
	operator unsigned() const { return (data & mask) >> bitno; }
};

struct FPUControlWord
{
	union
	{
		uint16_t reg;
		RegBit<decltype(reg), 0>     IM;  // Invalid operation mask
		RegBit<decltype(reg), 1>     DM;  // Denormalized operand mask
		RegBit<decltype(reg), 2>     ZM;  // Zero divide mask
		RegBit<decltype(reg), 3>     OM;  // Overflow mask
		RegBit<decltype(reg), 4>     UM;  // Underflow mask
		RegBit<decltype(reg), 5>     PM;  // Precision mask
		RegBit<decltype(reg), 7>     M;   // Interrupt mask   (8087-only)
		RegBit<decltype(reg), 8, 2>  PC;  // Precision control
		RegBit<decltype(reg), 10, 2> RC;  // Rounding control
		RegBit<decltype(reg), 12>    IC;  // Infinity control (8087/80287-only)
	};

	enum
	{
		mask8087     = 0x1fff,
		maskNon8087  = 0x1f7f,
		reservedMask = 0x40,
		initValue    = 0x37f
	};
	enum RoundMode
	{
		Nearest = 0,
		Down    = 1,
		Up      = 2,
		Chop    = 3
	};

	FPUControlWord() : reg(initValue) {}
	FPUControlWord(const FPUControlWord& other) = default;
	FPUControlWord& operator=(const FPUControlWord& other)
	{
		reg = other.reg;
		return *this;
	}
	template<class T>
	FPUControlWord& operator=(T val)
	{
		reg = (val & (FPU_ArchitectureType<=FPU_ARCHTYPE_8087 ? mask8087 : maskNon8087)) | reservedMask;
		return *this;
	}
	operator unsigned() const
	{
		return reg;
	}
	template <class T>
	FPUControlWord& operator |=(T val)
	{
		*this = reg | val;
		return *this;
	}
	void init() { reg = initValue; }
	FPUControlWord allMasked() const
	{
		auto masked = *this;
		masked |= IM.mask | DM.mask | ZM.mask | OM.mask | UM.mask | PM.mask;
		return masked;
	}
};

struct FPUStatusWord
{
	union
	{
		uint16_t reg;
		RegBit<decltype(reg), 0>     IE;  // Invalid operation
		RegBit<decltype(reg), 1>     DE;  // Denormalized operand
		RegBit<decltype(reg), 2>     ZE;  // Divide-by-zero
		RegBit<decltype(reg), 3>     OE;  // Overflow
		RegBit<decltype(reg), 4>     UE;  // Underflow
		RegBit<decltype(reg), 5>     PE;  // Precision
		RegBit<decltype(reg), 6>     SF;  // Stack Flag (non-8087/802087)
		RegBit<decltype(reg), 7>     IR;  // Interrupt request (8087-only)
		RegBit<decltype(reg), 7>     ES;  // Error summary     (non-8087)
		RegBit<decltype(reg), 8>     C0;  // Condition flag
		RegBit<decltype(reg), 9>     C1;  // Condition flag
		RegBit<decltype(reg), 10>    C2;  // Condition flag
		RegBit<decltype(reg), 11, 3> top; // Top of stack pointer
		RegBit<decltype(reg), 14>    C3;  // Condition flag
		RegBit<decltype(reg), 15>    B;   // Busy flag
	};

	FPUStatusWord() : reg(0) {}
	FPUStatusWord(const FPUStatusWord& other) = default;
	FPUStatusWord& operator=(const FPUStatusWord& other)
	{
		reg = other.reg;
		return *this;
	}
	template<class T>
	FPUStatusWord& operator=(T val)
	{
		reg = val;
		return *this;
	}
	operator unsigned() const
	{
		return reg;
	}
	template <class T>
	FPUStatusWord& operator |=(T val)
	{
		*this |= reg | val;
		return *this;
	}
	void init() { reg = 0; }
	void clearExceptions()
	{
		IE = false; DE = false; ZE = false; OE = false; UE = false; PE = false;
		ES = false;
	}
	enum
	{
		conditionMask = 0x4700,
		conditionAndExceptionMask = 0x47bf
	};
	std::string to_string() const;
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
	FPUControlWord  cw;
	FPUStatusWord   sw;
	XMM_Reg			xmmreg[8]; // SSE emulation
	uint32_t		mxcsr; // SSE control register
} FPU_rec;

// Replace with std::numbers when c++20 is available
#define PI		3.141592653589793238462L
#define L2E		1.4426950408889634073605L
#define L2T		3.3219280948873623478693L
#define LN2		0.69314718055994530941683L
#define LG2		0.30102999566398119521379L


extern FPU_rec fpu;

// TOP = macro for use in C/C++ for top of FPU stack
// FPUSW = macro for the entire FPU status word for use in dynamic core
// NTS: DOSBox-X until 2023/03/11 and all other forks have dynamic core code that generates memory loads
//      from (&TOP). That code is flawed because while you think you are loading the top of the stack,
//      what you are actually doing is taking the address of a bitfield (which doesn't do what you think
//      it does!) and generating code to read that address. Code that you think is using the FPU top of
//      stack is in reality using the entire FPU status word as top of stack!
//
//      This issue has since been resolved by adding a right shift instruction after the load. To make
//      what is actually happening clearer for development going forward, all dynamic core code has been
//      changed to use &FPUSW instead of &TOP.
//
//      FPUSW is a macro that resolves to the "reg" union field which is a plain 16-bit unsigned integer
//      containing all FPU status word bits.
#define TOP fpu.sw.top
#define FPUSW fpu.sw.reg
#define STV(i)  ( (fpu.sw.top + (i) ) & 7 )


uint16_t FPU_GetTag(void);
void FPU_FLDCW(PhysPt addr);

static INLINE void FPU_SetTag(uint16_t tag){
	for(Bitu i=0;i<8;i++)
		fpu.tags[i] = static_cast<FPU_Tag>((tag >>(2*i))&3);
}

static INLINE void FPU_SET_C0(Bitu C){
	fpu.sw.C0 = !!C;
}

static INLINE void FPU_SET_C1(Bitu C){
	fpu.sw.C1 = !!C;
}

static INLINE void FPU_SET_C2(Bitu C){
	fpu.sw.C2 = !!C;
}

static INLINE void FPU_SET_C3(Bitu C){
	fpu.sw.C3 = !!C;
}

static INLINE void FPU_SET_D(Bitu C){
	fpu.sw.DE = !!C;
}

static INLINE void FPU_LOG_WARN(Bitu tree, bool ea, Bitu group, Bitu sub) {
	LOG(LOG_FPU,LOG_WARN)("ESC %lu%s:Unhandled group %lu subfunction %lu",(long unsigned int)tree,ea?" EA":"",(long unsigned int)group,(long unsigned int)sub);
}

#endif
