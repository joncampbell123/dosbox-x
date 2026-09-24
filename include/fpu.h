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

#include "fpu_state.h"
#include "logging.h"

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

int8_t  SaturateWordSToByteS(int16_t value);
int16_t SaturateDwordSToWordS(int32_t value);
uint8_t  SaturateWordSToByteU(int16_t value);
uint16_t SaturateDwordSToWordU(int32_t value);

void   setFPUTagEmpty();

// Replace with std::numbers when c++20 is available
#define PI		3.141592653589793238462L
#define L2E		1.4426950408889634073605L
#define L2T		3.3219280948873623478693L
#define LN2		0.69314718055994530941683L
#define LG2		0.30102999566398119521379L
constexpr double X87_TRIG_ARG_LIMIT = 0x1p63; // 2^63

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

/* FPU exception flags */
enum {
    FPU_EX_INVALID = 0x0001,    // IE
    FPU_EX_DENORMAL = 0x0002,   // DE
    FPU_EX_ZERODIVIDE = 0x0004, // ZE
    FPU_EX_OVERFLOW = 0x0008,   // OE
    FPU_EX_UNDERFLOW = 0x0010,  // UE
    FPU_EX_PRECISION = 0x0020,  // PE
    FPU_EX_STACKFAULT = 0x0040  // SF 
};

static INLINE void FPU_SetException(uint16_t ex) {
    FPUSW |= ex;
}

#endif
