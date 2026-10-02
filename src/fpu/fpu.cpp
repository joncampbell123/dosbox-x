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


#include "dosbox.h"
#if C_FPU

#include <array>
#include <cfenv>
#include <limits>
#include <string>

#include "cpu.h"
#include "cpu/lazyflags.h"
#include "fpu.h"
#include "fpu_float80.h"
#include "fpu_helpers.h"
#include "logging.h"
#include "mem.h"
#if C_FPU_X86
#include "fpu_x86_assembly.h"
#endif

FPU fpu;

static void fpu_GetST80(FPU_Reg_80& value)
{
#ifdef HAS_LONG_DOUBLE
    value = fpu.regs_80[TOP];
#else
    if (fpu.use80[TOP]) {
        value = fpu.regs_80[TOP];
    } else {
        FPU_Reg_64 source = {};
        source.raw = fpu.regs[TOP].raw;
        float80::convertFrom(value, source);
    }
#endif
}

void fpu_Push(const FPU_Reg_80& input)
{
    auto val = input;   // 32-bit ARM MSVC does not gaurantee 16-byte stack alignment so have to pass by ref
                        // and create a copy here
    TOP = (TOP-1) & 7;
    if (fpu.regvalid[TOP]) {
        fpu.sw.IE = 1;
        fpu.sw.SF = 1;
        fpu.sw.C1 = 1;
        fpu_detail::CheckException();
        val = FPU_Reg_80::QNaN;
    }
    fpu.regs_80[TOP] = val;
    fpu.regvalid[TOP] = true;
#ifndef HAS_LONG_DOUBLE
    fpu.use80[TOP] = true;
    fpu.regs[TOP].v = float80::convertToDouble(val);
#endif
}

static void fpu_PushReal(const FPU_Reg_32& source)
{
    FPU_Reg_80 value;
    const auto denormal = IsSubnormal(source);
    const auto signaling_nan = IsSNaN(source);
    float80::convertFrom(value, source);
    if (signaling_nan) float80::setQuietBit(value);
    fpu_Push(value);
    fpu_detail::RaiseLoadExceptions(denormal, signaling_nan);
}

static void fpu_PushReal(const FPU_Reg_64& source)
{
    FPU_Reg_80 value;
    const auto denormal = IsSubnormal(source);
    const auto signaling_nan = IsSNaN(source);
    float80::convertFrom(value, source);
    if (signaling_nan) float80::setQuietBit(value);
    fpu_Push(value);
    fpu_detail::RaiseLoadExceptions(denormal, signaling_nan);
}

void FPU_LOG_WARN(Bitu tree, bool ea, Bitu group, Bitu sub)
{
	LOG(LOG_FPU, LOG_WARN)("ESC %lu%s:Unhandled group %lu subfunction %lu",
	                        (long unsigned int)tree,
	                        ea ? " EA" : "",
	                        (long unsigned int)group,
	                        (long unsigned int)sub);
}

void FPU_FABS()
{
    if (fpu_detail::StackValid(TOP)) {
        fpu.regs_80[TOP].f.sign = 0;
#ifndef HAS_LONG_DOUBLE
        fpu.regs[TOP].f.sign = 0;
#endif
        fpu.sw.C1 = 0;
    }
}

void FPU_FADD(int op1, int op2)
{
    fpu_detail::CheckInputs(op1, op2);

#if C_FPU_X86
	FPUD_ARITH1(faddp);
#else
    std::feclearexcept(FE_ALL_EXCEPT);
 #ifdef HAS_LONG_DOUBLE
    fpu.regs_80[op1].v += fpu.regs_80[op2].v;
 #else
    fpu.use80[op1] = false;
    fpu.regs[op1].v += fpu.regs[op2].v;
 #endif
    fpu_detail::SetStatusFromHostExceptions();
#endif
    fpu_detail::CheckException();
}

void FPU_FADD_EA(int op1)
{
    FPU_FADD(op1, 8);
}

void FPU_FBLD(PhysPt addr)
{
    std::array<uint8_t, 10> bcd = {};
    const auto low = mem_readq(addr);
    const auto high = mem_readw(addr + 8);

    for (uint8_t i = 0; i < 8; ++i)
        bcd[i] = static_cast<uint8_t>(low >> (i * 8U));
    bcd[8] = static_cast<uint8_t>(high);
    bcd[9] = static_cast<uint8_t>(high >> 8U);

    uint64_t magnitude = 0;
    uint64_t decimal_place = 1;

    // Packed BCD has 18 digits in bytes 0 through 8. Both guest-memory reads
    // complete before modifying the FPU stack, so a page fault leaves it unchanged.
    for (uint8_t i = 0; i < 9; ++i) {
        const auto digits = bcd[i];
        magnitude += (digits & 0x0FU) * decimal_place;
        decimal_place *= 10;
        magnitude += ((digits >> 4) & 0x0FU) * decimal_place;
        decimal_place *= 10;
    }

    // Bit 7 of the final byte is the sign; the other bits are reserved.
    const auto negative = (bcd[9] & 0x80U) != 0;
    auto value = static_cast<int64_t>(magnitude);
    if (negative && value != 0)
        value = -value;

    FPU_Reg_80 result;
    float80::convertFrom(result, value);
    if (negative && value == 0)
        result.f.sign = 1;

    fpu.sw.C1 = 0;
    fpu_Push(result);
}

void FPU_FBST(PhysPt addr)
{
    constexpr uint64_t bcd_max = 999'999'999'999'999'999ULL;

    fpu_detail::StackValid(TOP);
    FPU_Reg_80 value = {};
    fpu_GetST80(value);
    const auto conversion = float80::convertToI64(value);
    auto exceptions = conversion.exceptions;
    auto rounded_up = conversion.rounded_up;

    uint64_t magnitude = 0;
    if (!(exceptions & FPU_EX_INVALID)) {
        magnitude = static_cast<uint64_t>(conversion.value);
        if (conversion.value < 0)
            magnitude = 0ULL - magnitude;

        if (magnitude > bcd_max) {
            exceptions = FPU_EX_INVALID;
            rounded_up = false;
        }
    }

    fpu.sw.C1 = rounded_up;
    if (exceptions) {
        FPU_SetException(exceptions);
        fpu_detail::CheckException();
    }

    if (exceptions & FPU_EX_INVALID) {
        // Packed-BCD integer indefinite: 00 00 00 00 00 00 C0 00 FF FF.
        mem_writeq(addr, 0xC000'0000'0000'0000ULL);
        mem_writew(addr + 8, 0xFFFFU);
        return;
    }

    uint64_t lower = 0;
    uint16_t upper = value.f.sign ? 0x8000U : 0;
    for (uint8_t i = 0; i < 9; ++i) {
        auto digit_pair = static_cast<uint8_t>(magnitude % 10U);
        magnitude /= 10U;
        digit_pair |= static_cast<uint8_t>(magnitude % 10U) << 4U;
        magnitude /= 10U;

        if (i < 8)
            lower |= static_cast<uint64_t>(digit_pair) << (i * 8U);
        else
            upper |= digit_pair;
    }
    mem_writeq(addr, lower);
    mem_writew(addr + 8, upper);
}

void FPU_FCHS()
{
    if (fpu_detail::StackValid(TOP)) {
        fpu.regs_80[TOP].f.sign ^= 1;
#ifndef HAS_LONG_DOUBLE
        fpu.regs[TOP].f.sign ^= 1;
#endif
        fpu.sw.C1 = 0;
    }
}

void FPU_FCLEX()
{
	fpu.sw.clearExceptions();
}

void FPU_FCMOV_B  (Bitu dst, Bitu src) { if (TFLG_B)   FPU_FST(src, dst); }
void FPU_FCMOV_BE (Bitu dst, Bitu src) { if (TFLG_BE)  FPU_FST(src, dst); }
void FPU_FCMOV_E  (Bitu dst, Bitu src) { if (TFLG_Z)   FPU_FST(src, dst); }
void FPU_FCMOV_NB (Bitu dst, Bitu src) { if (TFLG_NB)  FPU_FST(src, dst); }
void FPU_FCMOV_NBE(Bitu dst, Bitu src) { if (TFLG_NBE) FPU_FST(src, dst); }
void FPU_FCMOV_NE (Bitu dst, Bitu src) { if (TFLG_NZ)  FPU_FST(src, dst); }
void FPU_FCMOV_NU (Bitu dst, Bitu src) { if (TFLG_NP)  FPU_FST(src, dst); }
void FPU_FCMOV_U  (Bitu dst, Bitu src) { if (TFLG_P)   FPU_FST(src, dst); }

void FPU_FCOM(int op1, int op2)
{
    fpu_detail::Compare(op1, op2, true);
}

void FPU_FCOM_EA(int op1)
{
    fpu_detail::Compare(op1, 8, true);
}

void FPU_FCOMI(int op1, int op2)
{
    fpu_detail::CompareToCpuFlags(op1, op2, true);
}

void FPU_FCOS()
{
    fpu_detail::CheckInputs(TOP);
    fpu.sw.C1 = 0;
    fpu.sw.C2 = 0;

    if (fpu_detail::InputIsInfinity(TOP)) {
        fpu.sw.IE = 1;
        fpu_detail::CheckException();
        fpu.regs_80[TOP] = FPU_Reg_80::QNaN;
#ifndef HAS_LONG_DOUBLE
        fpu.use80[TOP] = true;
        fpu.regs[TOP] = FPU_Reg_64::QNaN;
#endif
        return;
    }

#if C_FPU_X86
    FPUD_TRIG(fcos);
#else
 #ifdef HAS_LONG_DOUBLE
    const auto input = fpu.regs_80[TOP].v;
    if (std::fabs(input) >= X87_TRIG_ARG_LIMIT) {
        fpu.sw.C2 = 1;
        return;
    }

    std::feclearexcept(FE_ALL_EXCEPT);
    fpu.regs_80[TOP].v = std::cos(input);
 #else
    const auto input = fpu.regs[TOP].v;
    if (std::fabs(input) >= X87_TRIG_ARG_LIMIT) {
        fpu.sw.C2 = 1;
        return;
    }

    std::feclearexcept(FE_ALL_EXCEPT);
    fpu.use80[TOP] = false;
    fpu.regs[TOP].v = std::cos(input);
 #endif
    fpu_detail::SetStatusFromHostExceptions();
#endif

    fpu_detail::CheckException();
}

void FPU_FDIV(int op1, int op2)
{
    const auto op2IsZero = fpu.regvalid[op2] && fpu_detail::InputIsZero(op2);
    fpu_detail::CheckInputs(op1, op2, !op2IsZero);

#if C_FPU_X86
    FPUD_ARITH3(fdivp)
#else
    std::feclearexcept(FE_ALL_EXCEPT);
  #ifdef HAS_LONG_DOUBLE
    fpu.regs_80[op1].v /= fpu.regs_80[op2].v;
  #else
    fpu.use80[op1] = false;
    fpu.regs[op1].v /= fpu.regs[op2].v;
  #endif
    fpu_detail::SetStatusFromHostExceptions();
#endif
    fpu_detail::CheckException();
}

void FPU_FDIV_EA(int op1)
{
    FPU_FDIV(op1, 8);
}

void FPU_FDIVR(int op1, int op2)
{
    const auto op1IsZero = fpu.regvalid[op1] && fpu_detail::InputIsZero(op1);
    fpu_detail::CheckInputs(op1, op2, !op1IsZero);

#if C_FPU_X86
    FPUD_ARITH3(fdivrp)
#else
    std::feclearexcept(FE_ALL_EXCEPT);
  #ifdef HAS_LONG_DOUBLE
    fpu.regs_80[op1].v = fpu.regs_80[op2].v / fpu.regs_80[op1].v;
  #else
    fpu.use80[op1] = false;
    fpu.regs[op1].v = fpu.regs[op2].v / fpu.regs[op1].v;
  #endif
    fpu_detail::SetStatusFromHostExceptions();
#endif
    fpu_detail::CheckException();
}

void FPU_FDIVR_EA(int op1)
{
    FPU_FDIVR(op1, 8);
}

void FPU_FFREE(int st)
{
	fpu.regvalid[st] = false;
}

void FPU_FINIT()
{
	fpu.cw.init();
	fpu.sw.init();
    fpu.regvalid = {};
    fpu.regvalid[8] = true; // the 9th register is always valid, it's used for temporary storage

}

void FPU_FLD_F32(PhysPt addr)
{
    FPU_Reg_32 val;
    val.raw = mem_readd(addr);
    fpu.sw.C1 = 0;
    fpu_PushReal(val);
}

void FPU_FLD_F32_EA(PhysPt addr)
{
    FPU_Reg_32 val;
    val.raw = mem_readd(addr);
#ifdef HAS_LONG_DOUBLE
	fpu.regs_80[8].v = static_cast<long double>(val.v);
#else
    fpu.regs[8].v = static_cast<double>(val.v);
    fpu.use80[8] = false;
#endif
    if (IsSNaN(val)) {
        fpu.sw.IE = 1;
        fpu_detail::CheckException();
    } else if (IsSubnormal(val) &&
               fpu.regvalid[TOP] &&
               !fpu_detail::InputIsNaN(TOP)) {
        // Only trigger denormal exception if stack operand is not a NaN
        fpu.sw.DE = 1;
        fpu_detail::CheckException();
    }
}

void FPU_FLD_F64(PhysPt addr)
{
    FPU_Reg_64 val;
    val.raw = mem_readq(addr);
    fpu.sw.C1 = 0;
    fpu_PushReal(val);
}

void FPU_FLD_F64_EA(PhysPt addr)
{
    FPU_Reg_64 val;
    val.raw = mem_readq(addr);
#ifdef HAS_LONG_DOUBLE
	fpu.regs_80[8].v = static_cast<long double>(val.v);
#else
    fpu.regs[8].v = val.v;
    fpu.use80[8] = false;
#endif
    if (IsSNaN(val)) {
        fpu.sw.IE = 1;
        fpu_detail::CheckException();
    } else if (IsSubnormal(val) &&
               fpu.regvalid[TOP] &&
               !fpu_detail::InputIsNaN(TOP)) {
        // Only trigger denormal exception if stack operand is not a NaN
        fpu.sw.DE = 1;
        fpu_detail::CheckException();
    }
}

void FPU_FLD_F80(PhysPt addr)
{
    FPU_Reg_80 val;
	val.raw.l = mem_readq(addr);
	val.raw.h = mem_readw(addr+8);
    fpu.sw.C1 = 0;
    fpu_Push(val);
}

void FPU_FLD_I16(PhysPt addr)
{
    FPU_Reg_80 val;
    int64_t integer = static_cast<int16_t>(mem_readw(addr));
    float80::convertFrom(val, integer);
    fpu.sw.C1 = 0;
    fpu_Push(val);
}

void FPU_FLD_I16_EA(PhysPt addr)
{
    int64_t integer = static_cast<int16_t>(mem_readw(addr));
    float80::convertFrom(fpu.regs_80[8], integer);
#ifndef HAS_LONG_DOUBLE
    fpu.regs[8].v = float80::convertToDouble(fpu.regs_80[8]);
    fpu.use80[8] = true;
#endif
}

void FPU_FLD_I32(PhysPt addr)
{
    FPU_Reg_80 val;
    int64_t integer = static_cast<int32_t>(mem_readd(addr));
    float80::convertFrom(val, integer);
    fpu.sw.C1 = 0;
    fpu_Push(val);
}

void FPU_FLD_I32_EA(PhysPt addr)
{
    int64_t integer = static_cast<int32_t>(mem_readd(addr));
    float80::convertFrom(fpu.regs_80[8], integer);
#ifndef HAS_LONG_DOUBLE
    fpu.regs[8].v = float80::convertToDouble(fpu.regs_80[8]);
    fpu.use80[8] = true;
#endif
}

void FPU_FLD_I64(PhysPt addr)
{
    FPU_Reg_80 val;
    int64_t integer = mem_readq(addr);
    float80::convertFrom(val, integer);
    fpu.sw.C1 = 0;
    fpu_Push(val);
}

void FPU_FLD_I64_EA(PhysPt addr)
{
    int64_t integer = mem_readq(addr);
    float80::convertFrom(fpu.regs_80[8], integer);
#ifndef HAS_LONG_DOUBLE
    fpu.regs[8].v = float80::convertToDouble(fpu.regs_80[8]);
    fpu.use80[8] = true;
#endif
}

void FPU_FLD1()
{
    FPU_Reg_80 val;
    val.raw = float80::const1;
    fpu.sw.C1 = 0;
    fpu_Push(val);
}

void FPU_FLDCW(PhysPt addr)
{
	fpu.cw = mem_readw(addr);
}

void FPU_FLDENV(PhysPt addr, bool op16)
{
    uint16_t tag;
    if (op16) {
        fpu.cw = mem_readw(addr+0);
        fpu.sw = mem_readw(addr+2);
        tag    = mem_readw(addr+4);
    } else {
        fpu.cw = static_cast<uint16_t>(mem_readd(addr+0));
        fpu.sw = static_cast<uint16_t>(mem_readd(addr+4));
        tag    = static_cast<uint16_t>(mem_readd(addr+8));
    }
    FPU_SetTag(tag);
}

void FPU_FLDL2E()
{
    FPU_Reg_80 val;
    val.raw = float80::L2E;
    float80::round(val, float80::L2E_Extra2);
    fpu.sw.C1 = 0;
    fpu_Push(val);
}

void FPU_FLDL2T()
{
    FPU_Reg_80 val;
    val.raw = float80::L2T;
    float80::round(val, float80::L2T_Extra2);
    fpu.sw.C1 = 0;
    fpu_Push(val);
}

void FPU_FLDLG2()
{
    FPU_Reg_80 val;
    val.raw = float80::LG2;
    float80::round(val, float80::LG2_Extra2);
    fpu.sw.C1 = 0;
    fpu_Push(val);
}

void FPU_FLDLN2()
{
    FPU_Reg_80 val;
    val.raw = float80::LN2;
    float80::round(val, float80::LN2_Extra2);
    fpu.sw.C1 = 0;
    fpu_Push(val);
}

void FPU_FLDPI()
{
    FPU_Reg_80 val;
    val.raw = float80::PI;
    float80::round(val, float80::PI_Extra2);
    fpu.sw.C1 = 0;
    fpu_Push(val);
}

void FPU_FLDZ()
{
    FPU_Reg_80 val = {};
    fpu.sw.C1 = 0;
    fpu_Push(val);
}

void FPU_FMUL(int op1, int op2)
{
    fpu_detail::CheckInputs(op1, op2);

#if C_FPU_X86
    FPUD_ARITH1(fmulp)
#else
    std::feclearexcept(FE_ALL_EXCEPT);
  #ifdef HAS_LONG_DOUBLE
    fpu.regs_80[op1].v *= fpu.regs_80[op2].v;
  #else
    fpu.use80[op1] = false;
    fpu.regs[op1].v *= fpu.regs[op2].v;
  #endif
    fpu_detail::SetStatusFromHostExceptions();
#endif
    fpu_detail::CheckException();
}

void FPU_FMUL_EA(int op1)
{
    FPU_FMUL(op1, 8);
}

void FPU_FPATAN()
{
    fpu_detail::CheckInputs(STV(1), TOP);
    fpu.sw.C1 = 0;

#if C_FPU_X86
    FPUD_WITH_POP(fpatan)
#else
  #ifdef HAS_LONG_DOUBLE
    const auto y = fpu.regs_80[STV(1)].v;
    const auto x = fpu.regs_80[TOP].v;

    std::feclearexcept(FE_ALL_EXCEPT);
    fpu.regs_80[STV(1)].v = std::atan2(y, x);
  #else
    const auto y = fpu.regs[STV(1)].v;
    const auto x = fpu.regs[TOP].v;

    std::feclearexcept(FE_ALL_EXCEPT);
    fpu.use80[STV(1)] = false;
    fpu.regs[STV(1)].v = std::atan2(y, x);
  #endif
    fpu_detail::SetStatusFromHostExceptions();
    FPU_FPOP();
#endif

    fpu_detail::CheckException();
}

void FPU_FPOP()
{
    fpu_detail::StackValid(TOP);
	fpu.regvalid[TOP] = false;
	TOP = (TOP+1) & 7;
}

void FPU_FPTAN()
{
    fpu_detail::CheckInputs(TOP);
    fpu.sw.C1 = 0;
    fpu.sw.C2 = 0;

    FPU_Reg_80 one = {};
    float80::convertFrom(one, int64_t{1});

    if (fpu_detail::InputIsInfinity(TOP)) {
        fpu.sw.IE = 1;
        fpu_detail::CheckException();
        fpu.regs_80[TOP] = FPU_Reg_80::QNaN;
#ifndef HAS_LONG_DOUBLE
        fpu.use80[TOP] = true;
        fpu.regs[TOP] = FPU_Reg_64::QNaN;
#endif
        fpu_Push(one);
        return;
    }

#if C_FPU_X86
    const auto output = (TOP - 1) & 7;
    FPUD_PTAN();
    if (!fpu.sw.C2) fpu_Push(fpu.regs_80[output]);
#else
  #ifdef HAS_LONG_DOUBLE
    const auto input = fpu.regs_80[TOP].v;
    if (std::fabs(input) >= X87_TRIG_ARG_LIMIT) {
        fpu.sw.C2 = 1;
        return;
    }

    std::feclearexcept(FE_ALL_EXCEPT);
    fpu.regs_80[TOP].v = std::tan(input);
    fpu_Push(one);
  #else
    const auto input = fpu.regs[TOP].v;
    if (std::fabs(input) >= X87_TRIG_ARG_LIMIT) {
        fpu.sw.C2 = 1;
        return;
    }

    std::feclearexcept(FE_ALL_EXCEPT);
    fpu.use80[TOP] = false;
    fpu.regs[TOP].v = std::tan(input);
    fpu_Push(one);
  #endif
    fpu_detail::SetStatusFromHostExceptions();
#endif

    fpu_detail::CheckException();
}

void FPU_FRSTOR(PhysPt addr, bool op16)
{
	FPU_FLDENV(addr, op16);

	auto start = op16 ? 14:28;
	for(auto i = 0; i < 8; i++) {
		fpu.regs_80[STV(i)].raw.l = mem_readq(addr+start);
		fpu.regs_80[STV(i)].raw.h = mem_readw(addr+start+8);
#ifndef HAS_LONG_DOUBLE
        fpu.regs[STV(i)].v = float80::convertToDouble(fpu.regs_80[STV(i)]);
		fpu.use80[STV(i)] = true;
#endif
		start += 10;
	}
}

void FPU_FSIN()
{
    fpu_detail::CheckInputs(TOP);
    fpu.sw.C1 = 0;
    fpu.sw.C2 = 0;

    if (fpu_detail::InputIsInfinity(TOP)) {
        fpu.sw.IE = 1;
        fpu_detail::CheckException();
        fpu.regs_80[TOP] = FPU_Reg_80::QNaN;
#ifndef HAS_LONG_DOUBLE
        fpu.use80[TOP] = true;
        fpu.regs[TOP] = FPU_Reg_64::QNaN;
#endif
        return;
    }

#if C_FPU_X86
    FPUD_TRIG(fsin);
#else
  #ifdef HAS_LONG_DOUBLE
    const auto input = fpu.regs_80[TOP].v;
    if (std::fabs(input) >= X87_TRIG_ARG_LIMIT) {
        fpu.sw.C2 = 1;
        return;
    }

    std::feclearexcept(FE_ALL_EXCEPT);
    fpu.regs_80[TOP].v = std::sin(input);
  #else
    const auto input = fpu.regs[TOP].v;
    if (std::fabs(input) >= X87_TRIG_ARG_LIMIT) {
        fpu.sw.C2 = 1;
        return;
    }

    std::feclearexcept(FE_ALL_EXCEPT);
    fpu.use80[TOP] = false;
    fpu.regs[TOP].v = std::sin(input);
  #endif
    fpu_detail::SetStatusFromHostExceptions();
#endif

    fpu_detail::CheckException();
}

void FPU_FSINCOS()
{
    fpu_detail::CheckInputs(TOP);
    fpu.sw.C1 = 0;
    fpu.sw.C2 = 0;

    if (fpu_detail::InputIsInfinity(TOP)) {
        fpu.sw.IE = 1;
        fpu_detail::CheckException();
        fpu.regs_80[TOP] = FPU_Reg_80::QNaN;
#ifndef HAS_LONG_DOUBLE
        fpu.use80[TOP] = true;
        fpu.regs[TOP] = FPU_Reg_64::QNaN;
#endif
        fpu_Push(fpu.regs_80[TOP]);
        return;
    }

#if C_FPU_X86
    const auto output = (TOP - 1) & 7;
    FPUD_SINCOS();
    if (!fpu.sw.C2) fpu_Push(fpu.regs_80[output]);
#else
  #ifdef HAS_LONG_DOUBLE
    const auto input = fpu.regs_80[TOP].v;
    if (std::fabs(input) >= X87_TRIG_ARG_LIMIT) {
        fpu.sw.C2 = 1;
        return;
    }

    std::feclearexcept(FE_ALL_EXCEPT);
    const auto sine = std::sin(input);
    FPU_Reg_80 cosine = {};
    cosine.v = std::cos(input);
    fpu.regs_80[TOP].v = sine;
    fpu_Push(cosine);
  #else
    const auto input = fpu.regs[TOP].v;
    if (std::fabs(input) >= X87_TRIG_ARG_LIMIT) {
        fpu.sw.C2 = 1;
        return;
    }

    std::feclearexcept(FE_ALL_EXCEPT);
    const auto sine = std::sin(input);
    FPU_Reg_64 cosine = {};
    cosine.v = std::cos(input);
    fpu.use80[TOP] = false;
    fpu.regs[TOP].v = sine;

    FPU_Reg_80 cosine_80 = {};
    float80::convertFrom(cosine_80, cosine);
    const auto output = (TOP - 1) & 7;
    const auto stack_overflow = fpu.regvalid[output];
    fpu_Push(cosine_80);
    if (!stack_overflow) {
        fpu.use80[TOP] = false;
        fpu.regs[TOP] = cosine;
    }
  #endif
    fpu_detail::SetStatusFromHostExceptions();
#endif

    fpu_detail::CheckException();
}

void FPU_FSQRT()
{
    fpu_detail::CheckInputs(TOP);
    fpu.sw.C1 = 0;

    if (fpu_detail::InputIsNegative(TOP) &&
        !fpu_detail::InputIsZero(TOP) && !fpu_detail::InputIsNaN(TOP)) {
        fpu.sw.IE = 1;
        fpu_detail::CheckException();
    }

#if C_FPU_X86
    FPUD_ARITH2(fsqrt)
#else
  #ifdef HAS_LONG_DOUBLE
    std::feclearexcept(FE_ALL_EXCEPT);
    fpu.regs_80[TOP].v = std::sqrt(fpu.regs_80[TOP].v);
  #else
    std::feclearexcept(FE_ALL_EXCEPT);
    fpu.use80[TOP] = false;
    fpu.regs[TOP].v = std::sqrt(fpu.regs[TOP].v);
  #endif
    fpu_detail::SetStatusFromHostExceptions();
#endif

    fpu_detail::CheckException();
}

void FPU_FST(int src, int dst)
{
    fpu.sw.C1 = 0;
    fpu_detail::StackValid(src);

    fpu.regvalid[dst] = fpu.regvalid[src];
    fpu.regs_80[dst] = fpu.regs_80[src];
#ifndef HAS_LONG_DOUBLE
    fpu.regs[dst] = fpu.regs[src];
    fpu.use80[dst] = fpu.use80[src];
#endif
}

void FPU_FST_F32(PhysPt addr)
{
    fpu_detail::StackValid(TOP);

    FPU_Reg_32 result = {};
    float80::F32ConversionResult conversion = {};

#ifdef HAS_LONG_DOUBLE
    conversion = float80::convertToF32(fpu.regs_80[TOP]);
    result = conversion.value;
#else
    if (fpu.use80[TOP]) {
        conversion = float80::convertToF32(fpu.regs_80[TOP]);
        result = conversion.value;
    } else {
        FPU_Reg_64 source = {};
        FPU_Reg_80 value = {};
        source.raw = fpu.regs[TOP].raw;
        float80::convertFrom(value, source);
        conversion = float80::convertToF32(value);
        result = conversion.value;
    }
#endif

    fpu.sw.C1 = conversion.rounded_up;

    if (conversion.exceptions) {
        FPU_SetException(conversion.exceptions);
        fpu_detail::CheckException();
    }

    mem_writed(addr, result.raw);
}

void FPU_FST_F64(PhysPt addr)
{
    fpu_detail::StackValid(TOP);

    FPU_Reg_64 result = {};
    float80::F64ConversionResult conversion = {};

#ifdef HAS_LONG_DOUBLE
    conversion = float80::convertToF64(fpu.regs_80[TOP]);
    result = conversion.value;
#else
    if (fpu.use80[TOP]) {
        conversion = float80::convertToF64(fpu.regs_80[TOP]);
        result = conversion.value;
    } else {
        result.raw = fpu.regs[TOP].raw;
    }
#endif

    fpu.sw.C1 = conversion.rounded_up;

    if (conversion.exceptions) {
        FPU_SetException(conversion.exceptions);
        fpu_detail::CheckException();
    }

    mem_writeq(addr, result.raw);
}

void FPU_FST_F80(PhysPt addr)
{
    fpu.sw.C1 = 0;
    fpu_detail::StackValid(TOP);
    FPU_Reg_80 val;
#ifdef HAS_LONG_DOUBLE
    val = fpu.regs_80[TOP];
#else
    if (fpu.use80[TOP]) {
        val = fpu.regs_80[TOP];
    } else {
        FPU_Reg_64 source = {};
        source.raw = fpu.regs[TOP].raw;
        float80::convertFrom(val, source);
    }
#endif
    mem_writeq(addr  , val.raw.l);
    mem_writew(addr+8, val.raw.h);
}

void FPU_FST_I16(PhysPt addr)
{
    fpu_detail::StackValid(TOP);
    FPU_Reg_80 value = {};
    fpu_GetST80(value);
    const auto conversion = float80::convertToI16(value);
    fpu.sw.C1 = conversion.rounded_up;

    if (conversion.exceptions) {
        FPU_SetException(conversion.exceptions);
        fpu_detail::CheckException();
    }

    mem_writew(addr, static_cast<uint16_t>(conversion.value));
}

void FPU_FST_I32(PhysPt addr)
{
    fpu_detail::StackValid(TOP);
    FPU_Reg_80 value = {};
    fpu_GetST80(value);
    const auto conversion = float80::convertToI32(value);
    fpu.sw.C1 = conversion.rounded_up;

    if (conversion.exceptions) {
        FPU_SetException(conversion.exceptions);
        fpu_detail::CheckException();
    }

    mem_writed(addr, static_cast<uint32_t>(conversion.value));
}

void FPU_FST_I64(PhysPt addr)
{
    fpu_detail::StackValid(TOP);
    FPU_Reg_80 value = {};
    fpu_GetST80(value);
    const auto conversion = float80::convertToI64(value);
    fpu.sw.C1 = conversion.rounded_up;

    if (conversion.exceptions) {
        FPU_SetException(conversion.exceptions);
        fpu_detail::CheckException();
    }

    mem_writeq(addr, static_cast<uint64_t>(conversion.value));
}

void FPU_FSUB(int op1, int op2)
{
    fpu_detail::CheckInputs(op1, op2);

#if C_FPU_X86
    FPUD_ARITH1(fsubp)
#else
    std::feclearexcept(FE_ALL_EXCEPT);
  #ifdef HAS_LONG_DOUBLE
    fpu.regs_80[op1].v -= fpu.regs_80[op2].v;
  #else
    fpu.use80[op1] = false;
    fpu.regs[op1].v -= fpu.regs[op2].v;
  #endif
    fpu_detail::SetStatusFromHostExceptions();
#endif
    fpu_detail::CheckException();
}

void FPU_FSUB_EA(int op1)
{
    FPU_FSUB(op1, 8);
}

void FPU_FSUBR(int op1, int op2)
{
    fpu_detail::CheckInputs(op1, op2);

#if C_FPU_X86
    FPUD_ARITH1(fsubrp)
#else
    std::feclearexcept(FE_ALL_EXCEPT);
  #ifdef HAS_LONG_DOUBLE
    fpu.regs_80[op1].v = fpu.regs_80[op2].v - fpu.regs_80[op1].v;
  #else
    fpu.use80[op1] = false;
    fpu.regs[op1].v = fpu.regs[op2].v - fpu.regs[op1].v;
  #endif
    fpu_detail::SetStatusFromHostExceptions();
#endif
    fpu_detail::CheckException();
}

void FPU_FSUBR_EA(int op1)
{
    FPU_FSUBR(op1, 8);
}

void FPU_FTST()
{
#ifdef HAS_LONG_DOUBLE
    fpu.regs_80[8].v = 0.0L;
#else
    fpu.use80[8] = false;
    fpu.regs[8].v = 0.0;
#endif
    FPU_FCOM(TOP, 8);
}

void FPU_FUCOM(int op1, int op2)
{
    fpu_detail::Compare(op1, op2, false);
}

void FPU_FUCOMI(int op1, int op2)
{
    fpu_detail::CompareToCpuFlags(op1, op2, false);
}

void FPU_FXCH(int op1, int op2)
{
    fpu.sw.C1 = 0;
    std::swap(fpu.regvalid[op1], fpu.regvalid[op2]);
    std::swap(fpu.regs_80[op1], fpu.regs_80[op2]);
#ifndef HAS_LONG_DOUBLE
    std::swap(fpu.regs[op1], fpu.regs[op2]);
    std::swap(fpu.use80[op1], fpu.use80[op2]);
#endif
    if (!fpu.regvalid[op1] || !fpu.regvalid[op2]) {
        fpu.sw.IE = 1;
        fpu.sw.SF = 1;
        fpu.sw.C1 = 0;
        fpu_detail::CheckException();
    }
}

void FPU_FYL2X()
{
    const auto x = TOP;
    const auto y = STV(1);
    fpu_detail::CheckInputs(y, x);
    fpu.sw.C1 = 0;

    if (fpu_detail::InputIsNaN(x) || fpu_detail::InputIsNaN(y)) {
        fpu.regs_80[y] = FPU_Reg_80::QNaN;
#ifndef HAS_LONG_DOUBLE
        fpu.use80[y] = true;
        fpu.regs[y] = FPU_Reg_64::QNaN;
#endif
        FPU_FPOP();
        return;
    }

    const auto x_is_zero = fpu_detail::InputIsZero(x);
    const auto y_is_zero = fpu_detail::InputIsZero(y);
    const auto y_is_infinity = fpu_detail::InputIsInfinity(y);
    if (fpu_detail::InputIsNegative(x) &&
        !x_is_zero) {
        fpu.sw.IE = 1;
        fpu_detail::CheckException();
    }
    if (x_is_zero && y_is_zero) {
        fpu.sw.IE = 1;
        fpu_detail::CheckException();
    } else if (x_is_zero && !y_is_infinity) {
        fpu.sw.ZE = 1;
        fpu_detail::CheckException();
    }

#if C_FPU_X86
    FPUD_FYL2X(fyl2x)
#else
  #ifdef HAS_LONG_DOUBLE
    const auto input = fpu.regs_80[x].v;
    const auto multiplier = fpu.regs_80[y].v;

    std::feclearexcept(FE_ALL_EXCEPT);
    const auto logarithm = x_is_zero ? -std::numeric_limits<long double>::infinity()
                                     : std::log2(input);
    fpu.regs_80[y].v = multiplier * logarithm;
  #else
    const auto input = fpu.regs[x].v;
    const auto multiplier = fpu.regs[y].v;

    std::feclearexcept(FE_ALL_EXCEPT);
    fpu.use80[y] = false;
    const auto logarithm = x_is_zero ? -std::numeric_limits<double>::infinity()
                                     : std::log2(input);
    fpu.regs[y].v = multiplier * logarithm;
  #endif
    fpu_detail::SetStatusFromHostExceptions();
    FPU_FPOP();
#endif

    fpu_detail::CheckException();
}

void FPU_FYL2XP1()
{
    const auto x = TOP;
    const auto y = STV(1);
    fpu_detail::CheckInputs(y, x);
    fpu.sw.C1 = 0;

    if (fpu_detail::InputIsZero(x) && fpu_detail::InputIsInfinity(y)) {
        fpu.sw.IE = 1;
        fpu_detail::CheckException();
    }

#if C_FPU_X86
    FPUD_WITH_POP(fyl2xp1)
#else
  #ifdef HAS_LONG_DOUBLE
    const auto input = fpu.regs_80[x].v;
    const auto multiplier = fpu.regs_80[y].v;

    std::feclearexcept(FE_ALL_EXCEPT);
    fpu.regs_80[y].v = multiplier * std::log1p(input) / std::log(2.0L);
  #else
    const auto input = fpu.regs[x].v;
    const auto multiplier = fpu.regs[y].v;

    std::feclearexcept(FE_ALL_EXCEPT);
    fpu.use80[y] = false;
    fpu.regs[y].v = multiplier * std::log1p(input) / std::log(2.0);
  #endif
    fpu_detail::SetStatusFromHostExceptions();
    FPU_FPOP();
#endif

    fpu_detail::CheckException();
}


#if C_FPU_X86
#include "fpu_instructions_x86.h"
#elif defined(HAS_LONG_DOUBLE)
#include "fpu_instructions_longdouble.h"
#else
#include "fpu_instructions.h"
#endif

/* MMX instructions set the top of stack to zero---Intel explicitly documents this.
 * There is code out there, including in Windows ME and Windows Media Player, that
 * will show minor artifacts without this. */
void EnterMMX(void) {
	fpu.sw.top = 0;
	FPU_SetTag(0);
#if !defined(HAS_LONG_DOUBLE)
	fpu.use80[0] = true;
	fpu.use80[1] = true;
	fpu.use80[2] = true;
	fpu.use80[3] = true;
	fpu.use80[4] = true;
	fpu.use80[5] = true;
	fpu.use80[6] = true;
	fpu.use80[7] = true;
	fpu.use80[8] = true;
#endif
}

static void EATREE(Bitu _rm){
	Bitu group=(_rm >> 3) & 7;
	switch(group){
		case 0x00:	/* FADD */
			FPU_FADD_EA(TOP);
			break;
		case 0x01:	/* FMUL  */
			FPU_FMUL_EA(TOP);
			break;
		case 0x02:	/* FCOM */
			FPU_FCOM_EA(TOP);
			break;
		case 0x03:	/* FCOMP */
			FPU_FCOM_EA(TOP);
			FPU_FPOP();
			break;
		case 0x04:	/* FSUB */
			FPU_FSUB_EA(TOP);
			break;
		case 0x05:	/* FSUBR */
			FPU_FSUBR_EA(TOP);
			break;
		case 0x06:	/* FDIV */
			FPU_FDIV_EA(TOP);
			break;
		case 0x07:	/* FDIVR */
			FPU_FDIVR_EA(TOP);
			break;
		default:
			break;
	}
}

void FPU_ESC0_EA(Bitu rm,PhysPt addr) {
	/* REGULAR TREE WITH 32 BITS REALS */
	FPU_FLD_F32_EA(addr);
	EATREE(rm);
}

void FPU_ESC0_Normal(Bitu rm) {
	Bitu group=(rm >> 3) & 7;
	Bitu sub=(rm & 7);
	switch (group){
	case 0x00:		/* FADD ST,STi */
		FPU_FADD(TOP,STV(sub));
		break;
	case 0x01:		/* FMUL  ST,STi */
		FPU_FMUL(TOP,STV(sub));
		break;
	case 0x02:		/* FCOM  STi */
		FPU_FCOM(TOP,STV(sub));
		break;
	case 0x03:		/* FCOMP STi */
		FPU_FCOM(TOP,STV(sub));
		FPU_FPOP();
		break;
	case 0x04:		/* FSUB  ST,STi */
		FPU_FSUB(TOP,STV(sub));
		break;	
	case 0x05:		/* FSUBR ST,STi */
		FPU_FSUBR(TOP,STV(sub));
		break;
	case 0x06:		/* FDIV  ST,STi */
		FPU_FDIV(TOP,STV(sub));
		break;
	case 0x07:		/* FDIVR ST,STi */
		FPU_FDIVR(TOP,STV(sub));
		break;
	default:
		break;
	}
}

void FPU_ESC1_EA(Bitu rm,PhysPt addr, bool op16) {
// floats
	Bitu group=(rm >> 3) & 7;
	Bitu sub=(rm & 7);
	switch(group){
	case 0x00: /* FLD float*/
        FPU_FLD_F32(addr);
		break;
	case 0x01: /* UNKNOWN */
		LOG(LOG_FPU,LOG_WARN)("ESC EA 1:Unhandled group %d subfunction %d",(int)group,(int)sub);
		break;
	case 0x02: /* FST float*/
		FPU_FST_F32(addr);
		break;
	case 0x03: /* FSTP float*/
		FPU_FST_F32(addr);
		FPU_FPOP();
		break;
	case 0x04: /* FLDENV */
		FPU_FLDENV(addr, op16);
		break;
	case 0x05: /* FLDCW */
		FPU_FLDCW(addr);
		break;
	case 0x06: /* FSTENV */
		FPU_FSTENV(addr, op16);
		break;
	case 0x07:  /* FNSTCW*/
		mem_writew(addr,fpu.cw);
		break;
	default:
		LOG(LOG_FPU,LOG_WARN)("ESC EA 1:Unhandled group %d subfunction %d",(int)group,(int)sub);
		break;
	}
}

void FPU_ESC1_Normal(Bitu rm) {
	Bitu group=(rm >> 3) & 7;
	Bitu sub=(rm & 7);
	switch (group){
	case 0x00: /* FLD STi */
		{
			Bitu reg_from=STV(sub);
			FPU_PREP_PUSH();
			FPU_FST(reg_from, TOP);
			break;
		}
	case 0x01: /* FXCH STi */
		FPU_FXCH(TOP,STV(sub));
		break;
	case 0x02: /* FNOP */
		FPU_FNOP();
		break;
	case 0x03: /* FSTP STi */
		FPU_FST(TOP,STV(sub));
		FPU_FPOP();
		break;   
	case 0x04:
		switch(sub){
		case 0x00:       /* FCHS */
			FPU_FCHS();
			break;
		case 0x01:       /* FABS */
			FPU_FABS();
			break;
		case 0x02:       /* UNKNOWN */
		case 0x03:       /* ILLEGAL */
			LOG(LOG_FPU,LOG_WARN)("ESC 1:Unhandled group %X subfunction %X",(int)group,(int)sub);
			break;
		case 0x04:       /* FTST */
			FPU_FTST();
			break;
		case 0x05:       /* FXAM */
			FPU_FXAM();
			break;
		case 0x06:       /* FTSTP (cyrix)*/
		case 0x07:       /* UNKNOWN */
			LOG(LOG_FPU,LOG_WARN)("ESC 1:Unhandled group %X subfunction %X",(int)group,(int)sub);
			break;
		}
		break;
	case 0x05:
		switch(sub){	
		case 0x00:       /* FLD1 */
			FPU_FLD1();
			break;
		case 0x01:       /* FLDL2T */
			FPU_FLDL2T();
			break;
		case 0x02:       /* FLDL2E */
			FPU_FLDL2E();
			break;
		case 0x03:       /* FLDPI */
			FPU_FLDPI();
			break;
		case 0x04:       /* FLDLG2 */
			FPU_FLDLG2();
			break;
		case 0x05:       /* FLDLN2 */
			FPU_FLDLN2();
			break;
		case 0x06:       /* FLDZ*/
			FPU_FLDZ();
			break;
		case 0x07:       /* ILLEGAL */
			LOG(LOG_FPU,LOG_WARN)("ESC 1:Unhandled group %X subfunction %X",(int)group,(int)sub);
			break;
		}
		break;
	case 0x06:
		switch(sub){
		case 0x00:	/* F2XM1 */
			FPU_F2XM1();
			break;
		case 0x01:	/* FYL2X */
			FPU_FYL2X();
			break;
		case 0x02:	/* FPTAN  */
			FPU_FPTAN();
			break;
		case 0x03:	/* FPATAN */
			FPU_FPATAN();
			break;
		case 0x04:	/* FXTRACT */
			FPU_FXTRACT();
			break;
		case 0x05:	/* FPREM1 */
			FPU_FPREM1();
			break;
		case 0x06:	/* FDECSTP */
			TOP = (TOP - 1) & 7;
			break;
		case 0x07:	/* FINCSTP */
			TOP = (TOP + 1) & 7;
			break;
		default:
			LOG(LOG_FPU,LOG_WARN)("ESC 1:Unhandled group %X subfunction %X",(int)group,(int)sub);
			break;
		}
		break;
	case 0x07:
		switch(sub){
		case 0x00:		/* FPREM */
			FPU_FPREM();
			break;
		case 0x01:		/* FYL2XP1 */
			FPU_FYL2XP1();
			break;
		case 0x02:		/* FSQRT */
			FPU_FSQRT();
			break;
		case 0x03:		/* FSINCOS */
			FPU_FSINCOS();
			break;
		case 0x04:		/* FRNDINT */
			FPU_FRNDINT();
			break;
		case 0x05:		/* FSCALE */
			FPU_FSCALE();
			break;
		case 0x06:		/* FSIN */
			FPU_FSIN();
			break;
		case 0x07:		/* FCOS */
			FPU_FCOS();
			break;
		default:
			LOG(LOG_FPU,LOG_WARN)("ESC 1:Unhandled group %X subfunction %X",(int)group,(int)sub);
			break;
		}
		break;
		default:
			LOG(LOG_FPU,LOG_WARN)("ESC 1:Unhandled group %X subfunction %X",(int)group,(int)sub);
	}
}


void FPU_ESC2_EA(Bitu rm,PhysPt addr) {
	/* 32 bits integer operands */
	FPU_FLD_I32_EA(addr);
	EATREE(rm);
}

void FPU_ESC2_Normal(Bitu rm) {
	Bitu group=(rm >> 3) & 7;
	Bitu sub=(rm & 7);
	switch(group){
	case 0x00: /* FCMOVB STi */
		FPU_FCMOV_B(TOP,STV(sub));
		break;
	case 0x01: /* FCMOVE STi */
		FPU_FCMOV_E(TOP,STV(sub));
		break;
	case 0x02: /* FCMOVBE STi */
		FPU_FCMOV_BE(TOP,STV(sub));
		break;
	case 0x03: /* FCMOVU STi */
		FPU_FCMOV_U(TOP,STV(sub));
		break;
	case 0x05:
		switch(sub){
		case 0x01:		/* FUCOMPP */
			FPU_FUCOM(TOP,STV(1));
			FPU_FPOP();
			FPU_FPOP();
			break;
		default:
			LOG(LOG_FPU,LOG_WARN)("ESC 2:Unhandled group %d subfunction %d",(int)group,(int)sub); 
			break;
		}
		break;
	default:
	   	LOG(LOG_FPU,LOG_WARN)("ESC 2:Unhandled group %d subfunction %d",(int)group,(int)sub);
		break;
	}
}


void FPU_ESC3_EA(Bitu rm,PhysPt addr) {
	Bitu group=(rm >> 3) & 7;
	Bitu sub=(rm & 7);

	switch(group){
	case 0x00:	/* FILD */
        FPU_FLD_I32(addr);
		break;
	case 0x01:	/* FISTTP */
        if(CPU_ArchitectureType == CPU_ARCHTYPE_EXPERIMENTAL)
        {
            FPU_FSTT_I32(addr);
            FPU_FPOP();
        }
        else
            LOG(LOG_FPU, LOG_WARN)("ESC 3 EA:Unhandled group %d subfunction %d", (int)group, (int)sub);
		break;
	case 0x02:	/* FIST */
		FPU_FST_I32(addr);
		break;
	case 0x03:	/* FISTP */
		FPU_FST_I32(addr);
		FPU_FPOP();
		break;
	case 0x05:	/* FLD 80 Bits Real */
        FPU_FLD_F80(addr);
		break;
	case 0x07:	/* FSTP 80 Bits Real */
		FPU_FST_F80(addr);
		FPU_FPOP();
		break;
	default:
		LOG(LOG_FPU,LOG_WARN)("ESC 3 EA:Unhandled group %d subfunction %d",(int)group,(int)sub);
	}
}

void FPU_ESC3_Normal(Bitu rm) {
	Bitu group=(rm >> 3) & 7;
	Bitu sub=(rm & 7);
	switch (group) {
	case 0x00: /* FCMOVNB STi */
		FPU_FCMOV_NB(TOP,STV(sub));
		break;
	case 0x01: /* FCMOVNE STi */
		FPU_FCMOV_NE(TOP,STV(sub));
		break;
	case 0x02: /* FCMOVNBE STi */
		FPU_FCMOV_NBE(TOP,STV(sub));
		break;
	case 0x03: /* FCMOVNU STi */
		FPU_FCMOV_NU(TOP,STV(sub));
		break;
	case 0x04:
		switch (sub) {
		case 0x00:				//FNENI
			if (FPU_ArchitectureType<=FPU_ARCHTYPE_8087)
				fpu.cw.M = false;
			else
				LOG(LOG_FPU,LOG_ERROR)("8087 only fpu code used esc 3: group 4: subfunction :%d",(int)sub);
			break;
		case 0x01:				//FNDIS
			if (FPU_ArchitectureType<=FPU_ARCHTYPE_8087)
				fpu.cw.M = true;
			else
				LOG(LOG_FPU,LOG_ERROR)("8087 only fpu code used esc 3: group 4: subfunction :%d",(int)sub);
			break;
		case 0x02:				//FNCLEX FCLEX
			FPU_FCLEX();
			break;
		case 0x03:				//FNINIT FINIT
			FPU_FINIT();
			break;
		case 0x04:				//FNSETPM
		case 0x05:				//FRSTPM
//			LOG(LOG_FPU,LOG_ERROR)("80267 protected mode (un)set. Nothing done");
			FPU_FNOP();
			break;
		default:
			E_Exit("ESC 3:ILLEGAL OPCODE group %d subfunction %d",(int)group,(int)sub);
		}
		break;
	case 0x05:		/* FUCOMI STi */
		FPU_FUCOMI(TOP,STV(sub));
		break;
	case 0x06:		/* FCOMI STi */
		FPU_FCOMI(TOP,STV(sub));
		break;
	default:
		LOG(LOG_FPU,LOG_WARN)("ESC 3:Unhandled group %d subfunction %d",(int)group,(int)sub);
		break;
	}
	return;
}


void FPU_ESC4_EA(Bitu rm,PhysPt addr) {
	/* REGULAR TREE WITH 64 BITS REALS */
	FPU_FLD_F64_EA(addr);
	EATREE(rm);
}

void FPU_ESC4_Normal(Bitu rm) {
	/* LOOKS LIKE number 6 without popping */
	Bitu group=(rm >> 3) & 7;
	Bitu sub=(rm & 7);
	switch(group){
	case 0x00:	/* FADD STi,ST*/
		FPU_FADD(STV(sub),TOP);
		break;
	case 0x01:	/* FMUL STi,ST*/
		FPU_FMUL(STV(sub),TOP);
		break;
	case 0x02:  /* FCOM*/
		FPU_FCOM(TOP,STV(sub));
		break;
	case 0x03:  /* FCOMP*/
		FPU_FCOM(TOP,STV(sub));
		FPU_FPOP();
		break;
	case 0x04:  /* FSUBR STi,ST*/
		FPU_FSUBR(STV(sub),TOP);
		break;
	case 0x05:  /* FSUB  STi,ST*/
		FPU_FSUB(STV(sub),TOP);
		break;
	case 0x06:  /* FDIVR STi,ST*/
		FPU_FDIVR(STV(sub),TOP);
		break;
	case 0x07:  /* FDIV STi,ST*/
		FPU_FDIV(STV(sub),TOP);
		break;
	default:
		break;
	}
}

void FPU_ESC5_EA(Bitu rm,PhysPt addr, bool op16) {
	Bitu group=(rm >> 3) & 7;
	Bitu sub=(rm & 7);
	switch(group){
	case 0x00:  /* FLD double real*/
        FPU_FLD_F64(addr);
		break;
	case 0x01:  /* FISTTP longint*/
        if(CPU_ArchitectureType == CPU_ARCHTYPE_EXPERIMENTAL)
        {
            FPU_FSTT_I64(addr);
            FPU_FPOP();
        }
        else
            LOG(LOG_FPU, LOG_WARN)("ESC 5 EA:Unhandled group %d subfunction %d", (int)group, (int)sub);
		break;
	case 0x02:   /* FST double real*/
		FPU_FST_F64(addr);
		break;
	case 0x03:	/* FSTP double real*/
		FPU_FST_F64(addr);
		FPU_FPOP();
		break;
	case 0x04:	/* FRSTOR */
		FPU_FRSTOR(addr, op16);
		break;
	case 0x06:	/* FSAVE */
		FPU_FSAVE(addr, op16);
		break;
	case 0x07:   /*FNSTSW    NG DISAGREES ON THIS*/
		mem_writew(addr,fpu.sw);
		//seems to break all dos4gw games :)
		break;
	default:
		LOG(LOG_FPU,LOG_WARN)("ESC 5 EA:Unhandled group %d subfunction %d",(int)group,(int)sub);
	}
}

void FPU_ESC5_Normal(Bitu rm) {
	Bitu group=(rm >> 3) & 7;
	Bitu sub=(rm & 7);
	switch(group){
	case 0x00: /* FFREE STi */
        FPU_FFREE(STV(sub));
		break;
	case 0x01: /* FXCH STi*/
		FPU_FXCH(TOP,STV(sub));
		break;
	case 0x02: /* FST STi */
		FPU_FST(TOP,STV(sub));
		break;
	case 0x03:  /* FSTP STi*/
		FPU_FST(TOP,STV(sub));
		FPU_FPOP();
		break;
	case 0x04:	/* FUCOM STi */
		FPU_FUCOM(TOP,STV(sub));
		break;
	case 0x05:	/*FUCOMP STi */
		FPU_FUCOM(TOP,STV(sub));
		FPU_FPOP();
		break;
	default:
	LOG(LOG_FPU,LOG_WARN)("ESC 5:Unhandled group %d subfunction %d",(int)group,(int)sub);
	break;
	}
}

void FPU_ESC6_EA(Bitu rm,PhysPt addr) {
	/* 16 bit (word integer) operands */
	FPU_FLD_I16_EA(addr);
	EATREE(rm);
}

void FPU_ESC6_Normal(Bitu rm) {
	/* all P variants working only on registers */
	/* get top before switch and pop afterwards */
	Bitu group=(rm >> 3) & 7;
	Bitu sub=(rm & 7);
	switch(group){
	case 0x00:	/*FADDP STi,ST*/
		FPU_FADD(STV(sub),TOP);
		break;
	case 0x01:	/* FMULP STi,ST*/
		FPU_FMUL(STV(sub),TOP);
		break;
	case 0x02:  /* FCOMP5*/
		FPU_FCOM(TOP,STV(sub));
		break;	/* TODO IS THIS ALRIGHT ????????? */
	case 0x03:  /*FCOMPP*/
		if(sub != 1) {
			LOG(LOG_FPU,LOG_WARN)("ESC 6:Unhandled group %d subfunction %d",(int)group,(int)sub);
			return;
		}
		FPU_FCOM(TOP,STV(1));
		FPU_FPOP(); /* extra pop at the bottom*/
		break;
	case 0x04:  /* FSUBRP STi,ST*/
		FPU_FSUBR(STV(sub),TOP);
		break;
	case 0x05:  /* FSUBP  STi,ST*/
		FPU_FSUB(STV(sub),TOP);
		break;
	case 0x06:	/* FDIVRP STi,ST*/
		FPU_FDIVR(STV(sub),TOP);
		break;
	case 0x07:  /* FDIVP STi,ST*/
		FPU_FDIV(STV(sub),TOP);
		break;
	default:
		break;
	}
	FPU_FPOP();		
}


void FPU_ESC7_EA(Bitu rm,PhysPt addr) {
	Bitu group=(rm >> 3) & 7;
	Bitu sub=(rm & 7);
	switch(group){
	case 0x00:  /* FILD int16_t */
        FPU_FLD_I16(addr);
		break;
	case 0x01:  /* FISTTP int16_t */
        if(CPU_ArchitectureType == CPU_ARCHTYPE_EXPERIMENTAL)
        {
            FPU_FSTT_I16(addr);
            FPU_FPOP();
        }
        else
            LOG(LOG_FPU, LOG_WARN)("ESC 7 EA:Unhandled group %d subfunction %d", (int)group, (int)sub);
		break;
	case 0x02:   /* FIST int16_t */
		FPU_FST_I16(addr);
		break;
	case 0x03:	/* FISTP int16_t */
		FPU_FST_I16(addr);
		FPU_FPOP();
		break;
	case 0x04:   /* FBLD packed BCD */
        FPU_FBLD(addr);
		break;
	case 0x05:  /* FILD int64_t */
        FPU_FLD_I64(addr);
		break;
	case 0x06:	/* FBSTP packed BCD */
		FPU_FBST(addr);
		FPU_FPOP();
		break;
	case 0x07:  /* FISTP int64_t */
		FPU_FST_I64(addr);
		FPU_FPOP();
		break;
	default:
		LOG(LOG_FPU,LOG_WARN)("ESC 7 EA:Unhandled group %d subfunction %d",(int)group,(int)sub);
		break;
	}
}

void FPU_ESC7_Normal(Bitu rm) {
	Bitu group=(rm >> 3) & 7;
	Bitu sub=(rm & 7);
	switch (group){
	case 0x00: /* FFREEP STi*/
        FPU_FFREE(STV(sub));
		FPU_FPOP();
		break;
	case 0x01: /* FXCH STi*/
		FPU_FXCH(TOP,STV(sub));
		break;
	case 0x02:  /* FSTP STi*/
	case 0x03:  /* FSTP STi*/
		FPU_FST(TOP,STV(sub));
		FPU_FPOP();
		break;
	case 0x04:
		switch(sub){
			case 0x00:     /* FNSTSW AX*/
				reg_ax = fpu.sw;
				break;
			default:
				LOG(LOG_FPU,LOG_WARN)("ESC 7:Unhandled group %d subfunction %d",(int)group,(int)sub);
				break;
		}
		break;
	case 0x05:		/* FUCOMIP STi */
		FPU_FUCOMI(TOP,STV(sub));
		FPU_FPOP();
		break;
	case 0x06:		/* FCOMIP STi */
		FPU_FCOMI(TOP,STV(sub));
		FPU_FPOP();
		break;
	default:
		LOG(LOG_FPU,LOG_WARN)("ESC 7:Unhandled group %d subfunction %d",(int)group,(int)sub);
		break;
	}
}

// test routine at startup to make sure our typedef struct bitfields
// line up with the host's definition of a 32-bit single-precision
// floating point value.
void FPU_Selftest_32() {
	struct ftest {
		const char*	name;
		float		val;
		int		exponent:15;
		unsigned int	sign:1;
		uint32_t	mantissa;
	};
	static const struct ftest test[] = {
		// name			// val		// exponent (no bias)		// sign		// 23-bit mantissa without 23rd implied bit (max 2^23-1 = 0x7FFFFF)
		{"0.0f",		0.0f,		-FPU_Reg_32_exponent_bias,	0,		0x000000},	// IEEE standard way to encode zero
		{"1.0f",		1.0f,		0,				0,		0x000000},	// 1.0 x 2^0 = 1.0 x 1 = 1.0
		{"2.0f",		2.0f,		1,				0,		0x000000},	// 1.0 x 2^1 = 1.0 x 2 = 2.0
		{"3.0f",		3.0f,		1,				0,		0x400000},	// 1.5 x 2^1 = 1.5 x 2 = 3.0
		{"4.0f",		4.0f,		2,				0,		0x000000},	// 1.0 x 2^2 = 1.0 x 4 = 4.0
		{"-1.0f",		-1.0f,		0,				1,		0x000000},	// 1.0 x 2^0 = 1.0 x 1 = 1.0
		{"-2.0f",		-2.0f,		1,				1,		0x000000},	// 1.0 x 2^1 = 1.0 x 2 = 2.0
		{"-3.0f",		-3.0f,		1,				1,		0x400000},	// 1.5 x 2^1 = 1.5 x 2 = 3.0
		{"-4.0f",		-4.0f,		2,				1,		0x000000}	// 1.0 x 2^2 = 1.0 x 4 = 4.0
	};
	static const size_t tests = sizeof(test) / sizeof(test[0]);
	FPU_Reg_32 ft;

	if (sizeof(ft) < 4) {
		LOG(LOG_FPU,LOG_WARN)("FPU32 sizeof(reg32) < 4 bytes");
		return;
	}
	if (sizeof(float) != 4) {
		LOG(LOG_FPU,LOG_WARN)("FPU32 sizeof(float) != 4 bytes your host is weird");
		return;
	}

	// make sure bitfields line up
	ft.raw = 1UL << 31UL;
	if (ft.f.sign != 1 || ft.f.exponent != 0 || ft.f.mantissa != 0) {
		LOG(LOG_FPU,LOG_WARN)("FPU32 bitfield test #1 failed");
		return;
	}
	ft.raw = 1UL << 23UL;
	if (ft.f.sign != 0 || ft.f.exponent != 1 || ft.f.mantissa != 0) {
		LOG(LOG_FPU,LOG_WARN)("FPU32 bitfield test #2 failed");
		return;
	}
	ft.raw = 1UL << 0UL;
	if (ft.f.sign != 0 || ft.f.exponent != 0 || ft.f.mantissa != 1) {
		LOG(LOG_FPU,LOG_WARN)("FPU32 bitfield test #3 failed");
		return;
	}

	// carry out tests
	for (size_t t=0;t < tests;t++) {
		ft.v = test[t].val; FPU_Reg_m_barrier();
		if (((int)ft.f.exponent - FPU_Reg_32_exponent_bias) != test[t].exponent ||
			ft.f.sign != test[t].sign || ft.f.mantissa != test[t].mantissa) {
			LOG(LOG_FPU,LOG_WARN)("FPU32 selftest fail stage %s",test[t].name);
			LOG(LOG_FPU,LOG_WARN)("  expected t.v = %.10f t.s=%u t.exp=%d t.mantissa=%u",
				test[t].val,
				test[t].sign,
				(int)test[t].exponent,
				(unsigned int)test[t].mantissa);
			goto dump;
		}
	}

	LOG(LOG_FPU,LOG_DEBUG)("FPU32 selftest passed");
	return;
dump:
	LOG(LOG_FPU,LOG_WARN)("Result: t.v = %.10f t.s=%u t.exp=%d t.mantissa=%u",
		ft.v,
		ft.f.sign,
		(int)ft.f.exponent - FPU_Reg_32_exponent_bias,
		(unsigned int)ft.f.mantissa);
}

// test routine at startup to make sure our typedef struct bitfields
// line up with the host's definition of a 64-bit double-precision
// floating point value.
void FPU_Selftest_64() {
	struct ftest {
		const char*	name;
		double		val;
		int		exponent:15;
		unsigned int	sign:1;
		uint64_t	mantissa;
	};
	static const struct ftest test[] = {
		// name			// val		// exponent (no bias)		// sign		// 52-bit mantissa without 52rd implied bit (max 2^52-1 = 0x1FFFFFFFFFFFFF)
		{"0.0d",		0.0,		-FPU_Reg_64_exponent_bias,	0,		0x0000000000000ULL},	// IEEE standard way to encode zero
		{"1.0d",		1.0,		0,				0,		0x0000000000000ULL},	// 1.0 x 2^0 = 1.0 x 1 = 1.0
		{"2.0d",		2.0,		1,				0,		0x0000000000000ULL},	// 1.0 x 2^1 = 1.0 x 2 = 2.0
		{"3.0d",		3.0,		1,				0,		0x8000000000000ULL},	// 1.5 x 2^1 = 1.5 x 2 = 3.0
		{"4.0d",		4.0,		2,				0,		0x0000000000000ULL},	// 1.0 x 2^2 = 1.0 x 4 = 4.0
		{"-1.0d",		-1.0,		0,				1,		0x0000000000000ULL},	// 1.0 x 2^0 = 1.0 x 1 = 1.0
		{"-2.0d",		-2.0,		1,				1,		0x0000000000000ULL},	// 1.0 x 2^1 = 1.0 x 2 = 2.0
		{"-3.0d",		-3.0,		1,				1,		0x8000000000000ULL},	// 1.5 x 2^1 = 1.5 x 2 = 3.0
		{"-4.0d",		-4.0,		2,				1,		0x0000000000000ULL}	// 1.0 x 2^2 = 1.0 x 4 = 4.0
	};
	static const size_t tests = sizeof(test) / sizeof(test[0]);
	FPU_Reg_64 ft;

	if (sizeof(ft) < 8) {
		LOG(LOG_FPU,LOG_WARN)("FPU64 sizeof(reg64) < 8 bytes");
		return;
	}
	if (sizeof(double) != 8) {
		LOG(LOG_FPU,LOG_WARN)("FPU64 sizeof(float) != 8 bytes your host is weird");
		return;
	}

	// make sure bitfields line up
	ft.raw = 1ULL << 63ULL;
	if (ft.f.sign != 1 || ft.f.exponent != 0 || ft.f.mantissa != 0) {
		LOG(LOG_FPU,LOG_WARN)("FPU64 bitfield test #1 failed");
		return;
	}
	ft.raw = 1ULL << 52ULL;
	if (ft.f.sign != 0 || ft.f.exponent != 1 || ft.f.mantissa != 0) {
		LOG(LOG_FPU,LOG_WARN)("FPU64 bitfield test #2 failed");
		return;
	}
	ft.raw = 1ULL << 0ULL;
	if (ft.f.sign != 0 || ft.f.exponent != 0 || ft.f.mantissa != 1) {
		LOG(LOG_FPU,LOG_WARN)("FPU64 bitfield test #3 failed");
		return;
	}

	for (size_t t=0;t < tests;t++) {
		ft.v = test[t].val; FPU_Reg_m_barrier();
		if (((int)ft.f.exponent - FPU_Reg_64_exponent_bias) != test[t].exponent ||
			ft.f.sign != test[t].sign || ft.f.mantissa != test[t].mantissa) {
			LOG(LOG_FPU,LOG_WARN)("FPU64 selftest fail stage %s",test[t].name);
			LOG(LOG_FPU,LOG_WARN)("  expected t.v = %.10f t.s=%u t.exp=%d t.mantissa=%llu (0x%llx)",
				test[t].val,
				test[t].sign,
				(int)test[t].exponent,
				(unsigned long long)test[t].mantissa,
				(unsigned long long)test[t].mantissa);
			goto dump;
		}
	}

	LOG(LOG_FPU,LOG_DEBUG)("FPU64 selftest passed");
	return;
dump:
	LOG(LOG_FPU,LOG_WARN)("Result: t.v = %.10f t.s=%u t.exp=%d t.mantissa=%llu (0x%llx)",
		ft.v,
		(int)ft.f.sign,
		(int)ft.f.exponent - FPU_Reg_64_exponent_bias,
		(unsigned long long)ft.f.mantissa,
		(unsigned long long)ft.f.mantissa);
}

// test routine at startup to make sure our typedef struct bitfields
// line up with the host's definition of a 80-bit extended-precision
// floating point value (if the host is i686, x86_64, or any other
// host with the same definition of long double).
void FPU_Selftest_80() {
#if defined(HAS_LONG_DOUBLE)
	// we're assuming "long double" means the Intel 80x87 extended precision format, which is true when using
	// GCC on Linux i686 and x86_64 hosts.
	//
	// I understand that other platforms (PowerPC, Sparc, etc) might have other ideas on what makes "long double"
	// and I also understand Microsoft Visual C++ treats long double the same as double. We will disable this
	// test with #ifdefs when compiling for platforms where long double doesn't mean 80-bit extended precision.
	struct ftest {
		const char*	name;
		long double	val;
		int		exponent:15;
		unsigned int	sign:1;
		uint64_t	mantissa;
	};
	static const struct ftest test[] = {
		// name			// val		// exponent (no bias)		// sign		// 64-bit mantissa WITH whole integer bit #63
		{"0.0L",		0.0,		-FPU_Reg_80_exponent_bias,	0,		0x0000000000000000ULL},	// IEEE standard way to encode zero
		{"1.0L",		1.0,		0,				0,		0x8000000000000000ULL},	// 1.0 x 2^0 = 1.0 x 1 = 1.0
		{"2.0L",		2.0,		1,				0,		0x8000000000000000ULL},	// 1.0 x 2^1 = 1.0 x 2 = 2.0
		{"3.0L",		3.0,		1,				0,		0xC000000000000000ULL},	// 1.5 x 2^1 = 1.5 x 2 = 3.0
		{"4.0L",		4.0,		2,				0,		0x8000000000000000ULL},	// 1.0 x 2^2 = 1.0 x 4 = 4.0
		{"-1.0L",		-1.0,		0,				1,		0x8000000000000000ULL},	// 1.0 x 2^0 = 1.0 x 1 = 1.0
		{"-2.0L",		-2.0,		1,				1,		0x8000000000000000ULL},	// 1.0 x 2^1 = 1.0 x 2 = 2.0
		{"-3.0L",		-3.0,		1,				1,		0xC000000000000000ULL},	// 1.5 x 2^1 = 1.5 x 2 = 3.0
		{"-4.0L",		-4.0,		2,				1,		0x8000000000000000ULL}	// 1.0 x 2^2 = 1.0 x 4 = 4.0
	};
	static const size_t tests = sizeof(test) / sizeof(test[0]);
#endif
	FPU_Reg_80 ft;

	if (sizeof(ft) < 10) {
		LOG(LOG_FPU,LOG_WARN)("FPU80 sizeof(reg80) < 10 bytes");
		return;
	}
#if defined(HAS_LONG_DOUBLE)
	if (sizeof(long double) == sizeof(double)) {
		LOG(LOG_FPU,LOG_WARN)("FPU80 sizeof(long double) == sizeof(double) so your compiler just makes it an alias. skipping tests. please recompile with proper config.");
		return;
	}
	else if (sizeof(long double) < 10 || sizeof(long double) > 16) {
		// NTS: We can't assume 10 bytes. GCC on i686 makes long double 12 or 16 bytes long for alignment
		//      even though only 80 bits (10 bytes) are used.
		LOG(LOG_FPU,LOG_WARN)("FPU80 sizeof(float) < 10 bytes your host is weird");
		return;
	}
#endif

	// make sure bitfields line up
	ft.raw.l = 0;
	ft.raw.h = 1U << 15U;
	if (ft.f.sign != 1 || ft.f.exponent != 0 || ft.f.mantissa != 0) {
		LOG(LOG_FPU,LOG_WARN)("FPU80 bitfield test #1 failed. h=%04x l=%016llx",(unsigned int)ft.raw.h,(unsigned long long)ft.raw.l);
		return;
	}
	ft.raw.l = 0;
	ft.raw.h = 1U << 0U;
	if (ft.f.sign != 0 || ft.f.exponent != 1 || ft.f.mantissa != 0) {
		LOG(LOG_FPU,LOG_WARN)("FPU80 bitfield test #2 failed. h=%04x l=%016llx",(unsigned int)ft.raw.h,(unsigned long long)ft.raw.l);
		return;
	}
	ft.raw.l = 1ULL << 0ULL;
	ft.raw.h = 0;
	if (ft.f.sign != 0 || ft.f.exponent != 0 || ft.f.mantissa != 1) {
		LOG(LOG_FPU,LOG_WARN)("FPU80 bitfield test #3 failed. h=%04x l=%016llx",(unsigned int)ft.raw.h,(unsigned long long)ft.raw.l);
		return;
	}

#if defined(HAS_LONG_DOUBLE)
	for (size_t t=0;t < tests;t++) {
		ft.v = test[t].val; FPU_Reg_m_barrier();
		if (((int)ft.f.exponent - FPU_Reg_80_exponent_bias) != test[t].exponent ||
			ft.f.sign != test[t].sign || ft.f.mantissa != test[t].mantissa) {
			LOG(LOG_FPU,LOG_WARN)("FPU80 selftest fail stage %s",test[t].name);
			LOG(LOG_FPU,LOG_WARN)("  expected t.v = %.10Lf t.s=%u t.exp=%d t.mantissa=%llu (0x%llx)",
				test[t].val,
				test[t].sign,
				(int)test[t].exponent,
				(unsigned long long)test[t].mantissa,
				(unsigned long long)test[t].mantissa);
			goto dump;
		}
	}

	LOG(LOG_FPU,LOG_DEBUG)("FPU80 selftest passed");
	return;
dump:
	LOG(LOG_FPU,LOG_WARN)("Result: t.v = %.10Lf t.s=%u t.exp=%d t.mantissa=%llu (0x%llx)",
		ft.v,
		(int)ft.f.sign,
		(int)ft.f.exponent - FPU_Reg_64_exponent_bias,
		(unsigned long long)ft.f.mantissa,
		(unsigned long long)ft.f.mantissa);
#else
	LOG(LOG_FPU,LOG_DEBUG)("FPU80 selftest skipped, compiler does not have long double as 80-bit IEEE");
#endif
}

void FPU_Selftest() {
#if C_FPU_X86
    LOG(LOG_FPU,LOG_NORMAL)("FPU core: x86 FPU");
#elif defined(HAS_LONG_DOUBLE)
    LOG(LOG_FPU,LOG_NORMAL)("FPU core: long double FPU");
#else
    LOG(LOG_FPU,LOG_NORMAL)("FPU core: double FPU (caution: possible precision errors)");
#endif

	FPU_Selftest_32();
	FPU_Selftest_64();
	FPU_Selftest_80();
}

void FPU_Init() {
	LOG(LOG_MISC,LOG_DEBUG)("Initializing FPU");

	FPU_Selftest();
	FPU_FINIT();

    // Don't trigger any exceptions on the host
    fenv_t tmp;
    std::feholdexcept(&tmp);
}

static void FPU_SetAbridgedTag(uint8_t b)
{
    for (auto& regvalid: fpu.regvalid) {
        regvalid = !!(b & 1);
        b >>= 1;
    }
}

static uint8_t FPU_GetAbridgedTag()
{
    uint8_t b = 0;
    auto i = 0;
    for (auto regvalid: fpu.regvalid) {
        if (regvalid) b |= 1u << i++;
    }
    return b;
}

void CPU_FXSAVE(PhysPt eaa) {
	unsigned int i;

	/* Ref: [https://www.felixcloutier.com/x86/fxsave] */
	mem_writew(eaa+0x000,fpu.cw);					/* +0x000 FPU control word */
	mem_writew(eaa+0x002,fpu.sw);					/* +0x002 FPU status word */
	mem_writeb(eaa+0x004,FPU_GetAbridgedTag());			/* +0x004 FPU tag words, abridged to a bitfield of 1=not empty 0=empty, register order NOT from TOP */
	mem_writeb(eaa+0x005,0x00);					/* +0x005 reserved */
	mem_writew(eaa+0x006,0x0000);					/* +0x006 x87 FPU opcode (??) */
	mem_writed(eaa+0x008,reg_eip);					/* +0x008 x87 FPU instruction pointer (???) */
	mem_writew(eaa+0x00C,Segs.val[cs]);				/* +0x00C x87 FPU instruction pointer segment (???) */
	mem_writew(eaa+0x00E,0x0000);					/* +0x00E reserved */
	mem_writed(eaa+0x010,reg_eip);					/* +0x010 x87 FPU instruction operand (???) */
	mem_writew(eaa+0x014,Segs.val[ds]);				/* +0x014 x87 FPU instruction operand segment (???) */
	mem_writew(eaa+0x016,0x0000);					/* +0x016 reserved */
	mem_writed(eaa+0x018,fpu.mxcsr);				/* +0x018 MXCSR */
	mem_writed(eaa+0x01C,0xFFBF);					/* +0x01C MXCSR_MASK (DAZ not supported) */

	/* NTS: Remember that st(i) TOP pointer is in FPU status word */

	for (i=0;i < 8;i++) {
#if C_FPU_X86
		mem_writed(eaa+0x020+(i*16)+0,fpu.p_regs[STV(i)].m1);
		mem_writed(eaa+0x020+(i*16)+4,fpu.p_regs[STV(i)].m2);
		mem_writew(eaa+0x020+(i*16)+8,fpu.p_regs[STV(i)].m3);
#elif defined(HAS_LONG_DOUBLE)
		FPU_ST80(eaa+0x020+(i*16),STV(i));
#else
		FPU_ST80(eaa+0x020+(i*16),STV(i),/*&*/fpu.regs_80[STV(i)],fpu.use80[STV(i)]);
#endif
		mem_writed(eaa+0x020+(i*16)+0xA,0);
		mem_writew(eaa+0x020+(i*16)+0xE,0);
	}

	if (CPU_SSE()) {
		for (i=0;i < 8;i++) {
			XMM_Reg &xmm = fpu.xmmreg[i];
			mem_writed(eaa+0x0A0+(i*16)+0x0,xmm.u32[0]);
			mem_writed(eaa+0x0A0+(i*16)+0x4,xmm.u32[1]);
			mem_writed(eaa+0x0A0+(i*16)+0x8,xmm.u32[2]);
			mem_writed(eaa+0x0A0+(i*16)+0xC,xmm.u32[3]);
		}
	}
}

void CPU_FXRSTOR(PhysPt eaa) {
	/* Ref: [https://www.felixcloutier.com/x86/fxsave] */
	fpu.cw = mem_readw(eaa+0x000);					/* +0x000 FPU control word */
	fpu.sw = mem_readw(eaa+0x002);					/* +0x002 FPU status word */
	fpu.mxcsr = mem_readd(eaa+0x018);				/* +0x018 MXCSR */

	/* NTS: Remember that st(i) TOP pointer is in FPU status word */
	for (auto i = 0; i < 8; i++) {
#ifdef HAS_LONG_DOUBLE
		fpu.p_regs[STV(i)].m1 = mem_readd(eaa+0x020+(i*16)+0);
		fpu.p_regs[STV(i)].m2 = mem_readd(eaa+0x020+(i*16)+4);
		fpu.p_regs[STV(i)].m3 = mem_readw(eaa+0x020+(i*16)+8);
#else
        fpu.regs_80[STV(i)].raw.l = mem_readq(eaa+0x020+(i*16));
        fpu.regs_80[STV(i)].raw.h = mem_readw(eaa+0x020+(i*16)+8);
        fpu.regs[STV(i)].v = float80::convertToDouble(fpu.regs_80[STV(i)]);
		fpu.use80[STV(i)] = true;
#endif
	}

    FPU_SetAbridgedTag(mem_readb(eaa+0x004));	/* +0x004 FPU tag words, abridged to a bitfield of 1=not empty 0=empty, register order NOT from TOP */

	if (CPU_SSE()) {
		for (auto i = 0; i < 8; i++) {
			XMM_Reg &xmm = fpu.xmmreg[i];
			xmm.u32[0] = mem_readd(eaa+0x0A0+(i*16)+0x0);
			xmm.u32[1] = mem_readd(eaa+0x0A0+(i*16)+0x4);
			xmm.u32[2] = mem_readd(eaa+0x0A0+(i*16)+0x8);
			xmm.u32[3] = mem_readd(eaa+0x0A0+(i*16)+0xC);
		}
	}
}

#endif

//save state support
namespace
{
class SerializeFpu : public SerializeGlobalPOD
{
public:
    SerializeFpu() : SerializeGlobalPOD("FPU")
    {
        registerPOD(fpu);
    }
} dummy;
}

std::string FPUStatusWord::to_string() const
{
	return "B=" + std::to_string(B) + " C3-C0=" + std::to_string(C3) + std::to_string(C2) +
	       std::to_string(C1) + std::to_string(C0) + " ES=" + std::to_string(ES) +
	       " SF=" + std::to_string(SF) + " PE=" + std::to_string(PE) +
	       " UE=" + std::to_string(UE) + " OE=" + std::to_string(OE) +
	       " ZE=" + std::to_string(ZE) + " DE=" + std::to_string(DE) +
	       " IE=" + std::to_string(IE) + " TOP=" + std::to_string(top);
}
