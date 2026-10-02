#include "dosbox.h"
#if C_FPU

#include <cfenv>
#include <cstdint>

#include "cpu.h"
#include "cpu/lazyflags.h"
#include "fpu_float80.h"
#include "fpu_helpers.h"
#include "fpu_state.h"
#include "fpu_types.h"
#if C_FPU_X86
#include "fpu_x86_assembly.h"
#endif

namespace fpu_detail {

namespace {

template <typename T>
InputClass Classify(const T& input)
{
    if (IsZero(input))      return InputClass::Zero;
    if (IsSubnormal(input)) return InputClass::Denormal;
    if (IsInfinity(input))  return InputClass::Infinity;
    if (IsNaN(input))       return InputClass::NaN;
    if (IsNormal(input))    return InputClass::Normal;
    return InputClass::Unsupported;
}

bool InputIsSignalingNaN(int op)
{
#if C_FPU_X86 || defined(HAS_LONG_DOUBLE)
    return IsSNaN(fpu.regs_80[op]);
#else
    return fpu.use80[op] ? IsSNaN(fpu.regs_80[op]) : IsSNaN(fpu.regs[op]);
#endif
}

bool InputIsSubnormal(int op)
{
#if C_FPU_X86 || defined(HAS_LONG_DOUBLE)
    return IsSubnormal(fpu.regs_80[op]);
#else
    return fpu.use80[op] ? IsSubnormal(fpu.regs_80[op]) :
                           IsSubnormal(fpu.regs[op]);
#endif
}

void QuietInputNaN(int op)
{
#if C_FPU_X86 || defined(HAS_LONG_DOUBLE)
    float80::setQuietBit(fpu.regs_80[op]);
#else
    if (fpu.use80[op]) {
        float80::setQuietBit(fpu.regs_80[op]);
    } else {
        constexpr uint64_t quiet_nan_bit = 0x0008'0000'0000'0000ULL;
        fpu.regs[op].f.mantissa |= quiet_nan_bit;
    }
#endif
}

void SetComparisonFlags(bool unordered, bool equal, bool less)
{
    fpu.sw.C1 = 0;
    fpu.sw.C3 = unordered || equal;
    fpu.sw.C2 = unordered;
    fpu.sw.C0 = unordered || less;
}

} // namespace

void CheckException()
{
    // TODO
}

void SetStatusFromHostExceptions()
{
    const auto exceptions = std::fetestexcept(FE_ALL_EXCEPT);

    if (exceptions & FE_INVALID)   fpu.sw.IE = 1;
    if (exceptions & FE_DIVBYZERO) fpu.sw.ZE = 1;
    if (exceptions & FE_OVERFLOW)  fpu.sw.OE = 1;
    if (exceptions & FE_UNDERFLOW) fpu.sw.UE = 1;
    if (exceptions & FE_INEXACT)   fpu.sw.PE = 1;
}

InputClass ClassifyInput(int op)
{
#if C_FPU_X86 || defined(HAS_LONG_DOUBLE)
    return Classify(fpu.regs_80[op]);
#else
    return fpu.use80[op] ? Classify(fpu.regs_80[op]) :
                           Classify(fpu.regs[op]);
#endif
}

void SetQNaN(int pos)
{
    fpu.regs_80[pos] = FPU_Reg_80::QNaN;
#ifndef HAS_LONG_DOUBLE
    fpu.regs[pos] = FPU_Reg_64::QNaN;
    fpu.use80[pos] = true;
#endif
}

void SetInfinity(int pos, bool negative)
{
    fpu.regs_80[pos] = {};
    fpu.regs_80[pos].f.mantissa = 0x8000'0000'0000'0000ULL;
    fpu.regs_80[pos].f.exponent = 0x7FFFU;
    fpu.regs_80[pos].f.sign = negative;
#ifndef HAS_LONG_DOUBLE
    fpu.regs[pos] = {};
    fpu.regs[pos].f.exponent = 0x7FFU;
    fpu.regs[pos].f.sign = negative;
    fpu.use80[pos] = true;
#endif
}

bool StackValid(int pos)
{
    if (fpu.regvalid[pos]) return true;
    fpu.sw.IE = 1;
    fpu.sw.SF = 1;
    fpu.sw.C1 = 0;
    CheckException();
    fpu.regvalid[pos] = true;
    SetQNaN(pos);
    return false;
}

bool InputIsInfinity(int op)
{
#if C_FPU_X86 || defined(HAS_LONG_DOUBLE)
    return IsInfinity(fpu.regs_80[op]);
#else
    return fpu.use80[op] ? IsInfinity(fpu.regs_80[op]) :
                           IsInfinity(fpu.regs[op]);
#endif
}

bool InputIsNegative(int op)
{
#if C_FPU_X86 || defined(HAS_LONG_DOUBLE)
    return fpu.regs_80[op].f.sign;
#else
    return fpu.use80[op] ? fpu.regs_80[op].f.sign : fpu.regs[op].f.sign;
#endif
}

bool InputIsNaN(int op)
{
#if C_FPU_X86 || defined(HAS_LONG_DOUBLE)
    return IsNaN(fpu.regs_80[op]);
#else
    return fpu.use80[op] ? IsNaN(fpu.regs_80[op]) : IsNaN(fpu.regs[op]);
#endif
}

bool InputIsZero(int op)
{
#if C_FPU_X86 || defined(HAS_LONG_DOUBLE)
    return IsZero(fpu.regs_80[op]);
#else
    return fpu.use80[op] ? IsZero(fpu.regs_80[op]) : IsZero(fpu.regs[op]);
#endif
}

void CheckInputDenormals(int op)
{
    if (!InputIsSubnormal(op)) return;

    fpu.sw.DE = 1;
    CheckException();
}

void CheckInputDenormals(int op1, int op2)
{
    if (!InputIsSubnormal(op1) && !InputIsSubnormal(op2)) return;

    fpu.sw.DE = 1;
    CheckException();
}

bool CheckInputs(int op)
{
    StackValid(op);
    const auto is_nan = InputIsNaN(op);
    const auto is_signaling_nan = InputIsSignalingNaN(op);

    if (is_signaling_nan) {
        QuietInputNaN(op);
        fpu.sw.IE = 1;
        CheckException();
    }
    return is_nan;
}

bool CheckInputs(int op1, int op2, bool propagate_nan)
{
    StackValid(op2);
    StackValid(op1);
    const auto op1_is_nan = InputIsNaN(op1);
    const auto op2_is_nan = InputIsNaN(op2);
    const auto op1_is_signaling_nan = InputIsSignalingNaN(op1);
    const auto op2_is_signaling_nan = InputIsSignalingNaN(op2);

    if (op1_is_signaling_nan && propagate_nan)
        QuietInputNaN(op1);
    if (op2_is_signaling_nan && propagate_nan)
        QuietInputNaN(op2);
    if (op1_is_signaling_nan || op2_is_signaling_nan) {
        fpu.sw.IE = 1;
        CheckException();
    }
    if (op2_is_nan && propagate_nan) {
        SetQNaN(op1);
    }
    return op1_is_nan || op2_is_nan;
}

void RaiseLoadExceptions(bool denormal, bool signaling_nan)
{
    if (denormal) {
        fpu.sw.DE = 1;
        CheckException();
    } else if (signaling_nan) {
        fpu.sw.IE = 1;
        CheckException();
    }
}

void Compare(int op1, int op2, bool ordered)
{
    CheckInputs(op1, op2, false);

    // An 8087/287 compares infinities as equal regardless of their signs.
    if (FPU_ArchitectureType < FPU_ARCHTYPE_387 &&
        InputIsInfinity(op1) && InputIsInfinity(op2)) {
        SetComparisonFlags(false, true, false);
        return;
    }

#if C_FPU_X86
    if (ordered) {
        FPUD_COMPARE(fcompp);
    } else {
        FPUD_COMPARE(fucompp);
    }
#else
 #ifdef HAS_LONG_DOUBLE
    const auto a = fpu.regs_80[op1].v;
    const auto b = fpu.regs_80[op2].v;
 #else
    const auto a = fpu.regs[op1].v;
    const auto b = fpu.regs[op2].v;
 #endif
    if (std::isnan(a) || std::isnan(b)) {
        if (ordered)
            fpu.sw.IE = 1;
        SetComparisonFlags(true, false, false);
    } else {
        SetComparisonFlags(false, a == b, a < b);
    }
#endif

    CheckException();
}

void CompareToCpuFlags(int op1, int op2, bool ordered)
{
    FillFlags();
    SETFLAGBIT(OF, false);
    SETFLAGBIT(SF, false);
    SETFLAGBIT(AF, false);

    const auto old_c0 = fpu.sw.C0;
    const auto old_c2 = fpu.sw.C2;
    const auto old_c3 = fpu.sw.C3;

    Compare(op1, op2, ordered);

    const auto compare_c0 = fpu.sw.C0;
    const auto compare_c2 = fpu.sw.C2;
    const auto compare_c3 = fpu.sw.C3;

    // FCOMI and FUCOMI leave C0, C2, and C3 unchanged and clear C1.
    fpu.sw.C0 = old_c0;
    fpu.sw.C1 = 0;
    fpu.sw.C2 = old_c2;
    fpu.sw.C3 = old_c3;

    const auto unordered = compare_c0 && compare_c2 && compare_c3;
    SETFLAGBIT(ZF, unordered || compare_c3);
    SETFLAGBIT(PF, unordered);
    SETFLAGBIT(CF, unordered || compare_c0);
}

} // namespace fpu_detail

#endif // C_FPU
