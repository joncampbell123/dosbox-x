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

constexpr double X87_TRIG_ARG_LIMIT = 0x1p63; // 2^63

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
#define STV(i) FPU_StackIndex(i)


void FPU_FABS();
void FPU_F2XM1();
void FPU_FADD(int op1, int op2);
void FPU_FADD_EA(int op1);
void FPU_FBLD(PhysPt addr);
void FPU_FBST(PhysPt addr);
void FPU_FCHS();
void FPU_FCLEX();
void FPU_FCMOV_B(Bitu dst, Bitu src);
void FPU_FCMOV_BE(Bitu dst, Bitu src);
void FPU_FCMOV_E(Bitu dst, Bitu src);
void FPU_FCMOV_NB(Bitu dst, Bitu src);
void FPU_FCMOV_NBE(Bitu dst, Bitu src);
void FPU_FCMOV_NE(Bitu dst, Bitu src);
void FPU_FCMOV_NU(Bitu dst, Bitu src);
void FPU_FCMOV_U(Bitu dst, Bitu src);
void FPU_FCOM(int op1, int op2);
void FPU_FCOM_EA(int op1);
void FPU_FCOMI(int op1, int op2);
void FPU_FCOS();
void FPU_FDIV(int op1, int op2);
void FPU_FDIV_EA(int op1);
void FPU_FDIVR(int op1, int op2);
void FPU_FDIVR_EA(int op1);
void FPU_FMUL(int op1, int op2);
void FPU_FMUL_EA(int op1);
void FPU_FSUB(int op1, int op2);
void FPU_FSUB_EA(int op1);
void FPU_FSUBR(int op1, int op2);
void FPU_FSUBR_EA(int op1);
void FPU_FFREE(int st);
void FPU_FINIT();
void FPU_FLD_F32(PhysPt addr);
void FPU_FLD_F32_EA(PhysPt addr);
void FPU_FLD_F64(PhysPt addr);
void FPU_FLD_F64_EA(PhysPt addr);
void FPU_FLD_F80(PhysPt addr);
void FPU_FLD_I16(PhysPt addr);
void FPU_FLD_I16_EA(PhysPt addr);
void FPU_FLD_I32(PhysPt addr);
void FPU_FLD_I32_EA(PhysPt addr);
void FPU_FLD_I64(PhysPt addr);
void FPU_FLD_I64_EA(PhysPt addr);
void FPU_FLD1();
void FPU_FLDCW(PhysPt addr);
void FPU_FLDENV(PhysPt addr, bool op16);
void FPU_FLDL2T();
void FPU_FLDL2E();
void FPU_FLDLG2();
void FPU_FLDLN2();
void FPU_FLDPI();
void FPU_FLDZ();
void FPU_FPATAN();
void FPU_FPOP();
void FPU_FPTAN();
void FPU_FPREM();
void FPU_FPREM1();
void FPU_FRNDINT();
void FPU_FRSTOR(PhysPt addr, bool op16);
void FPU_FSCALE();
void FPU_FSIN();
void FPU_FSINCOS();
void FPU_FSQRT();
void FPU_FST(int src, int dst);
void FPU_FSTENV(PhysPt addr, bool op16);
void FPU_FSAVE(PhysPt addr, bool op16);
void FPU_FST_F32(PhysPt addr);
void FPU_FST_F64(PhysPt addr);
void FPU_FST_F80(PhysPt addr);
void FPU_FST_I16(PhysPt addr);
void FPU_FISTTP_I16(PhysPt addr);
void FPU_FST_I32(PhysPt addr);
void FPU_FISTTP_I32(PhysPt addr);
void FPU_FST_I64(PhysPt addr);
void FPU_FISTTP_I64(PhysPt addr);
void FPU_FTST();
void FPU_FUCOM(int op1, int op2);
void FPU_FUCOMI(int op1, int op2);
void FPU_FXAM();
void FPU_FXCH(int op1, int op2);
void FPU_FXTRACT();
void FPU_FYL2X();
void FPU_FYL2XP1();

static INLINE void FPU_SetTag(uint16_t tags){
	for (auto i=0; i<8; i++)
    {
        auto tag = static_cast<FPUTag>(tags & 0x3);
        fpu.regvalid[i] = (tag != FPUTag::Empty);
        tags >>= 2;
    }
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

void FPU_LOG_WARN(Bitu tree, bool ea, Bitu group, Bitu sub);

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
