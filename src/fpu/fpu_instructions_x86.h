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

#include "cpu.h"
#include "cpu/lazyflags.h"
#include "fpu.h"
#include "mem.h"
#include "regs.h"

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
}

static void FPU_PREP_PUSH(void){
	TOP = (TOP - 1) &7;
	fpu.regvalid[TOP] = true;
}

static void FPU_FSTT_I16(PhysPt addr) {
    FPUD_STORE(fisttp, WORD, s)
    mem_writew(addr, (uint16_t)fpu.p_regs[8].m1);
}

static void FPU_FSTT_I32(PhysPt addr) {
    FPUD_STORE(fisttp, DWORD, l)
    mem_writed(addr, fpu.p_regs[8].m1);
}

static void FPU_FSTT_I64(PhysPt addr) {
    FPUD_STORE(fisttp, QWORD, q)
    mem_writed(addr, fpu.p_regs[8].m1);
    mem_writed(addr + 4, fpu.p_regs[8].m2);
}


static void FPU_FSIN(void){
	FPUD_TRIG(fsin)
}

static void FPU_FSINCOS(void){
	FPUD_SINCOS()
}

static void FPU_FCOS(void){
	FPUD_TRIG(fcos)
}

static void FPU_FSQRT(void){
	FPUD_ARITH2(fsqrt)
}

static void FPU_FPATAN(void){
	FPUD_WITH_POP(fpatan)
}

static void FPU_FPTAN(void){
	FPUD_PTAN()
}


static void FPU_FDIV(Bitu op1, Bitu op2){
	FPUD_ARITH3(fdivp)
}

static void FPU_FDIV_EA(Bitu op1){
    FPU_FDIV(op1, 8);
}

static void FPU_FDIVR(Bitu op1, Bitu op2){
	FPUD_ARITH3(fdivrp)
}

static void FPU_FDIVR_EA(Bitu op1){
    FPU_FDIVR(op1, 8);
}

static void FPU_FMUL(Bitu op1, Bitu op2){
	FPUD_ARITH1(fmulp)
}

static void FPU_FMUL_EA(Bitu op1){
    FPU_FMUL(op1, 8);
}

static void FPU_FSUB(Bitu op1, Bitu op2){
	FPUD_ARITH1(fsubp)
}

static void FPU_FSUB_EA(Bitu op1){
    FPU_FSUB(op1, 8);
}

static void FPU_FSUBR(Bitu op1, Bitu op2){
	FPUD_ARITH1(fsubrp)
}

static void FPU_FSUBR_EA(Bitu op1){
    FPU_FSUBR(op1, 8);
}

static void FPU_FXCH(Bitu stv, Bitu other){
    std::swap(fpu.regvalid[stv], fpu.regvalid[other]);

	uint32_t m1s = fpu.p_regs[other].m1;
	uint32_t m2s = fpu.p_regs[other].m2;
	uint16_t m3s = fpu.p_regs[other].m3;
	fpu.p_regs[other].m1 = fpu.p_regs[stv].m1;
	fpu.p_regs[other].m2 = fpu.p_regs[stv].m2;
	fpu.p_regs[other].m3 = fpu.p_regs[stv].m3;
	fpu.p_regs[stv].m1 = m1s;
	fpu.p_regs[stv].m2 = m2s;
	fpu.p_regs[stv].m3 = m3s;

	FPU_SET_C1(0);
}

/* FPU_P_Reg holds the raw data fed to the host x86 FPU registers.
 * We can't guarantee that std::isinf() can handle that or that anything
 * in the host C++ compiler supports long double, so do it ourself */
static inline bool fpu_p_inf(const FPU_P_Reg &r) {
	/* Infinity is exponent == 0x7FFF and mantissa bits [63:61] == 100b (4) */
	return (r.m3 & 0x7FFFu) == 0x7FFFu && (r.m2 & 0xE0000000u) == 0x80000000u;
}

static inline bool FPUD_286_FCOM_INF(Bitu op1, Bitu op2) {
	/* HACK: If emulating a 286 processor we want the guest to think it's talking to a 287.
	 *       For more info, read [http://www.intel-assembler.it/portale/5/cpu-identification/asm-source-to-find-intel-cpu.asp]. */
	/* TODO: This should eventually become an option, say, a dosbox.conf option named fputype where the user can enter
	 *       "none" for no FPU, 287 or 387 for cputype=286 and cputype=386, or "auto" to match the CPU (8086 => 8087).
	 *       If the FPU type is 387 or auto, then skip this hack. Else for 8087 and 287, use this hack. */
	if (FPU_ArchitectureType<FPU_ARCHTYPE_387) {
		if (fpu_p_inf(fpu.p_regs[op1]) && fpu_p_inf(fpu.p_regs[op2])) {
			/* 8087/287 consider -inf == +inf and that's what DOS programs test for to detect 287 vs 387 */
			FPU_SET_C3(1);FPU_SET_C2(0);FPU_SET_C0(0);return true;
		}
	}

	return false;
}

static void FPU_FCOM(Bitu op1, Bitu op2){
	if (FPUD_286_FCOM_INF(op1,op2)) return;
	FPUD_COMPARE(fcompp)
}

static void FPU_FUCOM(Bitu op1, Bitu op2);

static void FPU_FCOMI(Bitu st, Bitu other, bool raise_invalid_for_nan = true){
	FillFlags();
	SETFLAGBIT(OF,false);
	SETFLAGBIT(SF,false);
	SETFLAGBIT(AF,false);
	fpu.sw.C1 = 0;

	if (!fpu.regvalid[st] || !fpu.regvalid[other]) {
		FPU_SetException(FPU_EX_INVALID | FPU_EX_STACKFAULT);
		SETFLAGBIT(ZF,true);
		SETFLAGBIT(PF,true);
		SETFLAGBIT(CF,true);
		return;
	}

	const auto old_c0 = fpu.sw.C0;
	const auto old_c2 = fpu.sw.C2;
	const auto old_c3 = fpu.sw.C3;

	if (raise_invalid_for_nan)
		FPU_FCOM(st, other);
	else
		FPU_FUCOM(st, other);

	const auto compare_c0 = fpu.sw.C0;
	const auto compare_c2 = fpu.sw.C2;
	const auto compare_c3 = fpu.sw.C3;

	// FCOMI and FUCOMI leave C0, C2, and C3 unchanged and always clear C1.
	fpu.sw.C0 = old_c0;
	fpu.sw.C1 = 0;
	fpu.sw.C2 = old_c2;
	fpu.sw.C3 = old_c3;

	if (compare_c3 && compare_c2 && compare_c0) {
		SETFLAGBIT(ZF,true);
		SETFLAGBIT(PF,true);
		SETFLAGBIT(CF,true);
	} else if (compare_c3) {
		SETFLAGBIT(ZF,true);
		SETFLAGBIT(PF,false);
		SETFLAGBIT(CF,false);
	} else if (compare_c0) {
		SETFLAGBIT(ZF,false);
		SETFLAGBIT(PF,false);
		SETFLAGBIT(CF,true);
	} else {
		SETFLAGBIT(ZF,false);
		SETFLAGBIT(PF,false);
		SETFLAGBIT(CF,false);
	}
}

static inline void FPU_FUCOMI(Bitu st, Bitu other){
	FPU_FCOMI(st, other, false);
}

static void FPU_FCOM_EA(Bitu op1){
    FPU_FCOM(op1, 8);
}

static void FPU_FUCOM(Bitu op1, Bitu op2){
	if (FPUD_286_FCOM_INF(op1,op2)) return;
	FPUD_COMPARE(fucompp)
}

static void FPU_FRNDINT(void){
	FPUD_ARITH2(frndint)
}

static void FPU_FPREM(void){
	FPUD_REMAINDER(fprem)
}

static void FPU_FPREM1(void){
	FPUD_REMAINDER(fprem1)
}

static void FPU_FXAM(void){
	FPUD_EXAMINE(fxam)
	// handle empty registers (C1 set to sign in any way!)
	if(!fpu.regvalid[TOP]) {
		FPU_SET_C3(1);FPU_SET_C2(0);FPU_SET_C0(1);
		return;
	}
}

static void FPU_F2XM1(void){
	FPUD_TRIG(f2xm1)
}

static void FPU_FYL2X(void){
	FPUD_FYL2X(fyl2x)
}

static void FPU_FYL2XP1(void){
	FPUD_WITH_POP(fyl2xp1)
}

static void FPU_FSCALE(void){
	FPUD_REMAINDER(fscale)
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
	PhysPt start = op16 ? 14:28;
	for(unsigned i=0;i<8;i++){
		mem_writed(addr+start,fpu.p_regs[STV(i)].m1);
		mem_writed(addr+start+4,fpu.p_regs[STV(i)].m2);
		mem_writew(addr+start+8,fpu.p_regs[STV(i)].m3);
		start+=10;
	}
	FPU_FINIT();
}


static void FPU_FXTRACT(void) {
	FPUD_XTRACT
}

static void FPU_FTST(void){
	FPUD_EXAMINE(ftst)
}
