#include "dosbox.h"
#if C_FPU

#include "fpu.h"
#include "fpu_float80.h"

namespace float80 {

constexpr auto ExpBias = 16383U;
const FPU_Reg_80::raw_t QNaN = {
    0xC000'0000'0000'0000ULL, // integer bit and quiet-NaN bit
    0xFFFFU                   // sign bit and all-one exponent
};
const FPU_Reg_80::raw_t const1 = {
    0x8000'0000'0000'0000ULL,
    ExpBias
};
const FPU_Reg_80::raw_t PI = {
    0xC90F'DAA2'2168'C234ULL,
    0x4000U
};
const uint8_t PI_Extra2 = 3;
const FPU_Reg_80::raw_t L2T = {
    0xD49A'784B'CD1B'8AFEULL,
    0x4000U
};
const uint8_t L2T_Extra2 = 1;
const FPU_Reg_80::raw_t L2E = {
    0xB8AA'3B29'5C17'F0BBULL,
    0x3FFFU
};
const uint8_t L2E_Extra2 = 2;
const FPU_Reg_80::raw_t LG2 = {
    0x9A20'9A84'FBCF'F798ULL,
    0x3FFDU
};
const uint8_t LG2_Extra2 = 3;
const FPU_Reg_80::raw_t LN2 = {
    0xB172'17F7'D1CF'79ABULL,
    0x3FFEU
};
const uint8_t LN2_Extra2 = 3;

namespace {

constexpr uint64_t IntegerBit = 0x8000'0000'0000'0000ULL;

void convertFromIEEE(FPU_Reg_80& result,
                     uint64_t fraction,
                     uint16_t source_exponent,
                     bool sign,
                     unsigned int fraction_bits,
                     int source_bias,
                     uint16_t max_exponent)
{
    const auto shift = 63U - fraction_bits;
    result.f.sign = sign;

    if (source_exponent == max_exponent) {
        // Map the source quiet-NaN bit to bit 62 of the extended significand.
        result.f.exponent = 0x7FFFU;
        result.f.mantissa = IntegerBit | (fraction << shift);
        return;
    }

    if (source_exponent != 0) {
        result.f.exponent = static_cast<uint16_t>(
                static_cast<int>(ExpBias) + static_cast<int>(source_exponent) - source_bias);
        result.f.mantissa = IntegerBit | (fraction << shift);
        return;
    }

    if (fraction == 0) {
        result.f.exponent = 0;
        result.f.mantissa = 0;
        return;
    }

    // A binary32 or binary64 subnormal is normal in the 80-bit format.
    auto mantissa = fraction << shift;
    auto exponent = 1 - source_bias;
    while (!(mantissa & IntegerBit)) {
        mantissa <<= 1;
        --exponent;
    }

    result.f.exponent = static_cast<uint16_t>(static_cast<int>(ExpBias) + exponent);
    result.f.mantissa = mantissa;
}

} // namespace

DoubleConversionResult convertToDouble(const FPU_Reg_80& val)
{
    constexpr auto double_exponent_bias = 1023;
    constexpr auto double_fraction_mask = 0x000F'FFFF'FFFF'FFFFULL;
    constexpr auto double_quiet_nan_bit = 0x0008'0000'0000'0000ULL;
    constexpr auto extended_integer_bit = 0x8000'0000'0000'0000ULL;

    DoubleConversionResult conversion = {};
    FPU_Reg result = {};
    const auto sign = static_cast<bool>(val.f.sign);
    const auto exponent80 = val.f.exponent;
    auto significand = val.f.mantissa;
    result.f.sign = sign;

    if (exponent80 == 0x7FFFU) {
        result.f.exponent = 0x7FFU;
        if (significand != extended_integer_bit) {
            // Preserve the payload where possible, and always return a quiet NaN.
            result.f.mantissa = ((significand >> 11) & double_fraction_mask) |
                                double_quiet_nan_bit;
        }
        conversion.value = result.d;
        return conversion;
    }

    if (significand == 0) {
        conversion.value = result.d;
        return conversion;
    }

    // An 80-bit subnormal uses an exponent of 1 - bias rather than -bias.
    int exponent = static_cast<int>(exponent80 ? exponent80 : 1) -
                   static_cast<int>(ExpBias);
    while (!(significand & extended_integer_bit)) {
        significand <<= 1;
        --exponent;
    }

    const auto round_right = [&conversion, sign, extended_integer_bit](uint64_t value,
                                                                         unsigned int shift) {
        const auto truncated = shift < 64 ? value >> shift : 0;
        bool inexact = false;
        bool round_up = false;

        if (shift < 64) {
            const auto half = 1ULL << (shift - 1);
            const auto remainder = value & ((half << 1) - 1);
            inexact = remainder != 0;
            if (fpu.cw.RC == FPUControlWord::RoundMode::Nearest)
                round_up = remainder > half || (remainder == half && (truncated & 1));
        } else {
            inexact = value != 0;
            if (fpu.cw.RC == FPUControlWord::RoundMode::Nearest && shift == 64)
                round_up = value > extended_integer_bit;
        }

        if (inexact) {
            conversion.exceptions |= FPU_EX_PRECISION;
            if (fpu.cw.RC == FPUControlWord::RoundMode::Down)
                round_up = sign;
            else if (fpu.cw.RC == FPUControlWord::RoundMode::Up)
                round_up = !sign;
        }
        return truncated + static_cast<uint64_t>(round_up);
    };

    const auto overflow = [&conversion, &result, sign, double_fraction_mask]() {
        const auto round_mode = static_cast<FPUControlWord::RoundMode>(
                static_cast<unsigned>(fpu.cw.RC));
        const auto to_infinity = round_mode == FPUControlWord::RoundMode::Nearest ||
                                 (round_mode == FPUControlWord::RoundMode::Up && !sign) ||
                                 (round_mode == FPUControlWord::RoundMode::Down && sign);
        result.f.exponent = to_infinity ? 0x7FFU : 0x7FEU;
        result.f.mantissa = to_infinity ? 0 : double_fraction_mask;
        conversion.exceptions |= FPU_EX_OVERFLOW | FPU_EX_PRECISION;
        conversion.value = result.d;
        return conversion;
    };

    if (exponent > 1023)
        return overflow();

    if (exponent >= -1022) {
        auto rounded = round_right(significand, 11);
        if (rounded == (1ULL << 53)) {
            rounded >>= 1;
            if (++exponent > 1023)
                return overflow();
        }
        result.f.exponent = exponent + double_exponent_bias;
        result.f.mantissa = rounded & double_fraction_mask;
        conversion.value = result.d;
        return conversion;
    }

    // A subnormal double is an integer multiple of 2^-1074.
    const auto fraction = round_right(significand,
                                      static_cast<unsigned int>(-exponent - 1011));
    if (fraction == (1ULL << 52)) {
        result.f.exponent = 1;
    } else {
        result.f.mantissa = fraction;
        if (conversion.exceptions & FPU_EX_PRECISION)
            conversion.exceptions |= FPU_EX_UNDERFLOW;
    }
    conversion.value = result.d;
    return conversion;
}

void convertFrom(FPU_Reg_80& result, int64_t value)
{
    result.f.mantissa = 0;
    result.f.exponent = 0;
    result.f.sign = 0;

    if (value == 0)
        return;

    const auto sign = value < 0;
    auto magnitude = static_cast<uint64_t>(value);
    if (sign)
        magnitude = 0ULL - magnitude;

    unsigned int shift = 0;
    while (!(magnitude & 0x8000'0000'0000'0000ULL)) {
        magnitude <<= 1;
        ++shift;
    }

    result.f.mantissa = magnitude;
    result.f.exponent = static_cast<uint16_t>(ExpBias + 63U - shift);
    result.f.sign = sign;
}

void convertFrom(FPU_Reg_80& result, const FPU_Reg_32& value)
{
    convertFromIEEE(result,
                    value.f.mantissa,
                    static_cast<uint16_t>(value.f.exponent),
                    static_cast<bool>(value.f.sign),
                    23,
                    127,
                    0xFFU);
}

void convertFrom(FPU_Reg_80& result, const FPU_Reg_64& value)
{
    convertFromIEEE(result,
                    value.f.mantissa,
                    static_cast<uint16_t>(value.f.exponent),
                    static_cast<bool>(value.f.sign),
                    52,
                    1023,
                    0x7FFU);
}

void round(FPU_Reg_80& val, uint8_t extra_two_bits)
{
    const auto round_mode = FPU_ArchitectureType >= FPU_ARCHTYPE_387
                                    ? static_cast<FPUControlWord::RoundMode>(
                                            static_cast<unsigned>(fpu.cw.RC))
                                    : FPUControlWord::RoundMode::Nearest;
    const auto remainder = extra_two_bits & 0x3u;
    bool round_up = false;

    switch (round_mode) {
    case FPUControlWord::RoundMode::Nearest:
        round_up = remainder > 2 || (remainder == 2 && (val.f.mantissa & 1));
        break;
    case FPUControlWord::RoundMode::Down:
        round_up = remainder != 0 && val.f.sign;
        break;
    case FPUControlWord::RoundMode::Up:
        round_up = remainder != 0 && !val.f.sign;
        break;
    case FPUControlWord::RoundMode::Chop:
        break;
    }

    if (round_up && ++val.f.mantissa == 0) {
        val.f.mantissa = 0x8000'0000'0000'0000ULL;
        ++val.f.exponent;
    }
}

} // namespace float80

#endif
