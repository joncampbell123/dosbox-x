#include "dosbox.h"
#if C_FPU

#include <cfenv>
#include <cstdint>
#include <cmath>

#include "cpu.h"
#include "cpu/lazyflags.h"
#include "fpu.h"
#include "fpu_float80.h"
#include "fpu_helpers.h"
#include "fpu_state.h"
#include "fpu_types.h"
#include "mem.h"
#include "pic.h"
#if C_FPU_X86
#include "fpu_x86_assembly.h"
#endif

namespace fpu_detail {

namespace {

constexpr Bitu fpu_irq = 13;

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
    if (op == 8 && fpu.ea_denormal) return true;
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

template <typename T>
void CalculatePartialRemainder(T& dividend, T divisor, RemainderMode mode)
{
    constexpr auto partial_reduction_width = 63;
    const auto exponent_difference = std::ilogb(dividend) - std::ilogb(divisor);

    if (exponent_difference >= 64) {
        const auto shift = exponent_difference - partial_reduction_width;
        const auto quotient = std::trunc(std::scalbn(dividend, -shift) / divisor);
        dividend -= std::scalbn(quotient * divisor, shift);
        fpu.sw.C2 = 1;
        return;
    }

    const auto division = dividend / divisor;
    auto quotient = std::trunc(division);
    if (mode == RemainderMode::Nearest) {
        const auto fraction = std::fabs(division - quotient);
        if (fraction > 0.5 ||
            (fraction == 0.5 && std::fmod(std::fabs(quotient), 2) != 0)) {
            quotient += std::copysign(static_cast<T>(1), division);
        }
    }
    dividend -= quotient * divisor;

    auto quotient_bits = static_cast<uint8_t>(std::fmod(std::fabs(quotient), 8));
    if (std::signbit(quotient) && quotient_bits)
        quotient_bits = 8 - quotient_bits;

    fpu.sw.C0 = (quotient_bits & 4) != 0;
    fpu.sw.C3 = (quotient_bits & 2) != 0;
    fpu.sw.C1 = (quotient_bits & 1) != 0;
    fpu.sw.C2 = 0;
}

} // namespace

bool CheckException()
{
    const auto raised = fpu.sw.reg & FPUStatusWord::exceptionMask;
    const auto masked = fpu.cw.reg & FPUStatusWord::exceptionMask;

    if ((raised & ~masked) == 0) return false;

    fpu.sw.ES = true;
    fpu.sw.B = true;

    if (!(cpu.cr0 & CR0_NUMERICERROR)) PIC_ActivateIRQ(fpu_irq);

    return true;
}

bool IsRealMode()
{
    return !cpu.pmode || (reg_flags & FLAG_VM);
}

bool UsesPacked16BitRealEnvironment()
{
    // The 8087 and 387-compatible real/v86 layouts pack two 20-bit linear
    // pointers and the 11-bit FOP into the final four words. The 287 instead
    // uses its segmented 16:16 layout in both modes.
    return IsRealMode() && FPU_ArchitectureType != FPU_ARCHTYPE_287;
}

bool UsesLinearRealModePointers()
{
    return IsRealMode() && FPU_ArchitectureType != FPU_ARCHTYPE_287;
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

uint16_t GetTag()
{
    uint16_t tags = 0;
    for (auto i = 0; i < 8; ++i) {
        FPUTag tag;
        if (!fpu.regvalid[i]) {
            tag = FPUTag::Empty;
#if C_FPU_X86 || defined(HAS_LONG_DOUBLE)
        } else if (IsZero(fpu.regs_80[i])) {
            tag = FPUTag::Zero;
        } else if (IsSpecial(fpu.regs_80[i])) {
            tag = FPUTag::Special;
#else
        } else if (fpu.use80[i] ? IsZero(fpu.regs_80[i]) : IsZero(fpu.regs[i])) {
            tag = FPUTag::Zero;
        } else if (fpu.use80[i] ? IsSpecial(fpu.regs_80[i]) : IsSpecial(fpu.regs[i])) {
            tag = FPUTag::Special;
#endif
        } else {
            tag = FPUTag::Valid;
        }
        tags |= static_cast<uint8_t>(tag) << (2 * i);
    }
    return tags;
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
    (void)CheckException();
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

bool CheckInputDenormals(int op)
{
    if (!InputIsSubnormal(op)) return false;

    fpu.sw.DE = 1;
    return CheckException();
}

bool CheckInputDenormals(int op1, int op2)
{
    if (!InputIsSubnormal(op1) && !InputIsSubnormal(op2)) return false;

    fpu.sw.DE = 1;
    return CheckException();
}

void PartialRemainder(double& dividend, double divisor, RemainderMode mode)
{
    CalculatePartialRemainder(dividend, divisor, mode);
}

#ifdef HAS_LONG_DOUBLE
void PartialRemainder(long double& dividend, long double divisor, RemainderMode mode)
{
    CalculatePartialRemainder(dividend, divisor, mode);
}
#endif

bool CheckInputs(int op)
{
    if (!StackValid(op)) return true;
    const auto is_nan = InputIsNaN(op);
    const auto is_signaling_nan = InputIsSignalingNaN(op);

    if (is_signaling_nan) {
        fpu.sw.IE = 1;
        if (CheckException()) return true;
        QuietInputNaN(op);
    }
    return is_nan;
}

bool CheckInputs(int op1, int op2, bool propagate_nan)
{
    const auto op2_valid = StackValid(op2);
    const auto op1_valid = StackValid(op1);
    if (!op1_valid || !op2_valid) return true;
    const auto op1_is_nan = InputIsNaN(op1);
    const auto op2_is_nan = InputIsNaN(op2);
    const auto op1_is_signaling_nan = InputIsSignalingNaN(op1);
    const auto op2_is_signaling_nan = InputIsSignalingNaN(op2);

    if (op1_is_signaling_nan || op2_is_signaling_nan) {
        fpu.sw.IE = 1;
        if (CheckException()) return true;
    }
    if (op1_is_signaling_nan && propagate_nan)
        QuietInputNaN(op1);
    if (op2_is_signaling_nan && propagate_nan)
        QuietInputNaN(op2);
    if (op2_is_nan && propagate_nan) {
        SetQNaN(op1);
    }
    return op1_is_nan || op2_is_nan;
}

void RaiseLoadExceptions(bool denormal, bool signaling_nan)
{
    if (denormal) {
        fpu.sw.DE = 1;
        (void)CheckException();
    } else if (signaling_nan) {
        fpu.sw.IE = 1;
        (void)CheckException();
    }
}

bool Compare(int op1, int op2, bool ordered)
{
    const auto has_nan = CheckInputs(op1, op2, false);
    if (has_nan && CheckException()) return true;
    if (!has_nan && CheckInputDenormals(op1, op2)) return true;

    // An 8087/287 compares infinities as equal regardless of their signs.
    if (FPU_ArchitectureType < FPU_ARCHTYPE_387 &&
        InputIsInfinity(op1) && InputIsInfinity(op2)) {
        SetComparisonFlags(false, true, false);
        return false;
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

    return CheckException();
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

    if (Compare(op1, op2, ordered)) {
        fpu.sw.C0 = old_c0;
        fpu.sw.C1 = 0;
        fpu.sw.C2 = old_c2;
        fpu.sw.C3 = old_c3;
        return;
    }

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


namespace {

static PhysPt FPU_FindEscape(PhysPt code)
{
    for (auto i = 0; i < 15; ++i, ++code) {
        const auto byte = mem_readb(code);
        if (byte >= 0xd8 && byte <= 0xdf)
            return code;
    }
    return code;
}

static SegNames FPU_GetOperandSegment(uint32_t instruction_pointer, Bitu rm)
{
    auto code = SegPhys(cs) + instruction_pointer;
    auto address_32bit = cpu.code.big;
    auto segment = ds;
    auto segment_override = false;

    for (auto i = 0; i < 15; ++i, ++code) {
        switch (mem_readb(code)) {
        case 0x26: segment = es; segment_override = true; continue;
        case 0x2e: segment = cs; segment_override = true; continue;
        case 0x36: segment = ss; segment_override = true; continue;
        case 0x3e: segment = ds; segment_override = true; continue;
        case 0x64: segment = fs; segment_override = true; continue;
        case 0x65: segment = gs; segment_override = true; continue;
        case 0x66:
        case 0xf0:
        case 0xf2:
        case 0xf3: continue;
        case 0x67: address_32bit = !address_32bit; continue;
        default: break;
        }
        break;
    }

    if (segment_override)
        return segment;

    const auto mod = (rm >> 6u) & 3u;
    const auto base = rm & 7u;
    auto uses_ss = false;
    if (address_32bit) {
        uses_ss = (base == 5 && mod != 0);
        if (base == 4) {
            const auto escape = FPU_FindEscape(SegPhys(cs) + instruction_pointer);
            const auto sib_base = mem_readb(escape + 2) & 7u;
            uses_ss = sib_base == 4 || (sib_base == 5 && mod != 0);
        }
    } else {
        uses_ss = base == 2 || base == 3 || (base == 6 && mod != 0);
    }
    return uses_ss ? ss : ds;
}

static bool FPU_Uses32BitAddressing(uint32_t instruction_pointer)
{
    auto address_32bit = cpu.code.big;
    auto code = SegPhys(cs) + instruction_pointer;
    for (auto i = 0; i < 15; ++i, ++code) {
        const auto byte = mem_readb(code);
        if (byte == 0x67) {
            address_32bit = !address_32bit;
        } else if (byte != 0x26 && byte != 0x2e && byte != 0x36 &&
                   byte != 0x3e && byte != 0x64 && byte != 0x65 &&
                   byte != 0x66 && byte != 0xf0 && byte != 0xf2 &&
                   byte != 0xf3) {
            break;
        }
    }
    return address_32bit;
}

static uint32_t FPU_GetReg32(Bitu reg)
{
    switch (reg) {
    case 0: return reg_eax;
    case 1: return reg_ecx;
    case 2: return reg_edx;
    case 3: return reg_ebx;
    case 4: return reg_esp;
    case 5: return reg_ebp;
    case 6: return reg_esi;
    default: return reg_edi;
    }
}

static uint32_t FPU_GetDynOperandOffset(uint32_t instruction_pointer, Bitu rm)
{
    const auto escape = FPU_FindEscape(SegPhys(cs) + instruction_pointer);
    auto code = escape + 2; // Skip the ESC and ModR/M bytes.
    const auto mod = (rm >> 6u) & 3u;
    const auto base = rm & 7u;

    if (FPU_Uses32BitAddressing(instruction_pointer)) {
        uint32_t offset = 0;
        auto displacement = 0;
        if (base == 4) {
            const auto sib = mem_readb(code++);
            const auto sib_base = sib & 7u;
            const auto sib_index = (sib >> 3u) & 7u;
            if (sib_index != 4)
                offset = FPU_GetReg32(sib_index) << (sib >> 6u);
            if (mod != 0 || sib_base != 5)
                offset += FPU_GetReg32(sib_base);
            else
                displacement = 4;
        } else if (mod != 0 || base != 5) {
            offset = FPU_GetReg32(base);
        } else {
            displacement = 4;
        }

        if (mod == 1) {
            offset += static_cast<int8_t>(mem_readb(code));
        } else if (mod == 2 || displacement == 4) {
            offset += static_cast<int32_t>(mem_readd(code));
        }
        return offset;
    }

    uint16_t offset = 0;
    switch (base) {
    case 0: offset = reg_bx + reg_si; break;
    case 1: offset = reg_bx + reg_di; break;
    case 2: offset = reg_bp + reg_si; break;
    case 3: offset = reg_bp + reg_di; break;
    case 4: offset = reg_si; break;
    case 5: offset = reg_di; break;
    case 6:
        if (mod != 0) offset = reg_bp;
        break;
    case 7: offset = reg_bx; break;
    }
    if (mod == 0 && base == 6)
        offset = mem_readw(code);
    else if (mod == 1)
        offset = static_cast<uint16_t>(offset + static_cast<int8_t>(mem_readb(code)));
    else if (mod == 2)
        offset = static_cast<uint16_t>(offset + static_cast<int16_t>(mem_readw(code)));
    return offset;
}

} // namespace

void FPU_RecordExceptionContext(Bitu escape, Bitu rm, bool has_data_pointer,
                                PhysPt data_pointer)
{
    auto opcode_pointer = reg_eip;
    // The 8087 records the ESC byte itself, whereas later x87s record the
    // complete instruction including prefixes.
    if (FPU_ArchitectureType <= FPU_ARCHTYPE_8087) {
        opcode_pointer = static_cast<uint32_t>(
                FPU_FindEscape(SegPhys(cs) + reg_eip) - SegPhys(cs));
    }

    const auto data_segment = FPU_GetOperandSegment(reg_eip, rm);
    const auto data_offset = has_data_pointer
                                     ? static_cast<uint32_t>(data_pointer - SegPhys(data_segment))
                                     : 0;
    const auto instruction_offset = FPU_ArchitectureType <= FPU_ARCHTYPE_8087
                                            ? opcode_pointer
                                            : reg_eip;
    fpu.fip = fpu_detail::UsesLinearRealModePointers()
                      ? SegPhys(cs) + instruction_offset
                      : instruction_offset;
    fpu.fcs = SegValue(cs);
    fpu.fop = static_cast<uint16_t>((escape << 8u) | rm);
    fpu.fdp = has_data_pointer && fpu_detail::UsesLinearRealModePointers()
                      ? data_pointer
                      : data_offset;
    fpu.fds = has_data_pointer ? SegValue(data_segment) : 0;
}

void FPU_RecordExceptionContextFromDynCPU(uint32_t instruction_pointer_offset,
                                          uint16_t opcode)
{
    const auto instruction_pointer = reg_eip + instruction_pointer_offset;
    auto opcode_pointer = instruction_pointer;
    if (FPU_ArchitectureType <= FPU_ARCHTYPE_8087) {
        opcode_pointer = static_cast<uint32_t>(
                FPU_FindEscape(SegPhys(cs) + instruction_pointer) - SegPhys(cs));
    }
    const auto instruction_offset = FPU_ArchitectureType <= FPU_ARCHTYPE_8087
                                            ? opcode_pointer
                                            : instruction_pointer;
    fpu.fip = fpu_detail::UsesLinearRealModePointers()
                      ? SegPhys(cs) + instruction_offset
                      : instruction_offset;
    fpu.fcs = SegValue(cs);
    fpu.fop = opcode & 0x7ffu;
    const auto rm = opcode & 0xffu;
    if (rm < 0xc0) {
        const auto segment = FPU_GetOperandSegment(instruction_pointer, rm);
        const auto offset = FPU_GetDynOperandOffset(instruction_pointer, rm);
        fpu.fdp = fpu_detail::UsesLinearRealModePointers()
                          ? SegPhys(segment) + offset
                          : offset;
        fpu.fds = SegValue(segment);
    } else {
        fpu.fdp = 0;
        fpu.fds = 0;
    }
}

bool FPU_UnmaskedExceptionPending()
{
    const auto raised = fpu.sw.reg & FPUStatusWord::exceptionMask;
    const auto masked = fpu.cw.reg & FPUStatusWord::exceptionMask;
    return (raised & ~masked) != 0;
}

bool FPU_NumericExceptionPending()
{
    if (!(cpu.cr0 & CR0_NUMERICERROR)) return false;

    return FPU_UnmaskedExceptionPending();
}

#endif // C_FPU
