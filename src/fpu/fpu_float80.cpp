#include "dosbox.h"
#if C_FPU

#include <limits>

#include "fpu.h"
#include "fpu_float80.h"

constexpr FPU_Reg_64 FPU_Reg_64::QNaN = {{
    0x0008'0000'0000'0000ULL,  // quiet-NaN bit
    0x7FFU,                    // all-one exponent
    1U                         // sign bit
}};

constexpr FPU_Reg_80 FPU_Reg_80::QNaN = {{
    0xC000'0000'0000'0000ULL,  // integer bit and quiet-NaN bit
    0x7FFFU,                   // all-one exponent
    1U                         // sign bit
}};

namespace float80 {

constexpr auto ExpBias = 16383U;
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
constexpr uint64_t QuietNaNBit = 0x4000'0000'0000'0000ULL;

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

void setQuietBit(FPU_Reg_80& val)
{
    val.f.mantissa |= QuietNaNBit;
}

namespace {

struct IEEEFormat {
    unsigned int fraction_bits;
    unsigned int exponent_bits;
    int exponent_bias;
    uint16_t max_exponent;
};

struct IEEEConversionResult {
    uint64_t raw = 0;
    uint16_t exceptions = 0;
    bool rounded_up = false;
};

IEEEConversionResult convertToIEEE(const FPU_Reg_80& val, const IEEEFormat& format)
{
    constexpr uint64_t extended_integer_bit = 0x8000'0000'0000'0000ULL;

    const auto fraction_mask = (1ULL << format.fraction_bits) - 1;
    const auto quiet_nan_bit = 1ULL << (format.fraction_bits - 1);
    const auto sign_shift = format.fraction_bits + format.exponent_bits;
    const auto sign = static_cast<bool>(val.f.sign);
    const auto exponent80 = val.f.exponent;
    auto significand = val.f.mantissa;
    IEEEConversionResult conversion = {};

    const auto set_result = [&conversion, fraction_mask, sign, sign_shift,
                             fraction_bits = format.fraction_bits](uint16_t exponent,
                                                                   uint64_t fraction) {
        conversion.raw = (static_cast<uint64_t>(sign) << sign_shift) |
                         (static_cast<uint64_t>(exponent) << fraction_bits) |
                         (fraction & fraction_mask);
    };

    if (exponent80 == 0x7FFFU) {
        const auto fraction = significand == extended_integer_bit
                                      ? 0
                                      : ((significand >> (63U - format.fraction_bits)) |
                                         quiet_nan_bit);
        set_result(format.max_exponent, fraction);
        return conversion;
    }

    if (significand == 0) {
        set_result(0, 0);
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
        conversion.rounded_up = round_up;
        return truncated + static_cast<uint64_t>(round_up);
    };

    const auto overflow = [&conversion, &set_result, fraction_mask, format, sign]() {
        const auto round_mode = static_cast<FPUControlWord::RoundMode>(
                static_cast<unsigned>(fpu.cw.RC));
        const auto to_infinity = round_mode == FPUControlWord::RoundMode::Nearest ||
                                 (round_mode == FPUControlWord::RoundMode::Up && !sign) ||
                                 (round_mode == FPUControlWord::RoundMode::Down && sign);
        set_result(to_infinity ? format.max_exponent
                               : static_cast<uint16_t>(format.max_exponent - 1U),
                   to_infinity ? 0 : fraction_mask);
        conversion.exceptions |= FPU_EX_OVERFLOW | FPU_EX_PRECISION;
        return conversion;
    };

    const auto max_normal_exponent = static_cast<int>(format.max_exponent - 1U) -
                                     format.exponent_bias;
    const auto min_normal_exponent = 1 - format.exponent_bias;
    if (exponent > max_normal_exponent)
        return overflow();

    if (exponent >= min_normal_exponent) {
        auto rounded = round_right(significand, 63U - format.fraction_bits);
        if (rounded == (1ULL << (format.fraction_bits + 1U))) {
            rounded >>= 1;
            if (++exponent > max_normal_exponent)
                return overflow();
        }
        set_result(static_cast<uint16_t>(exponent + format.exponent_bias), rounded);
        return conversion;
    }

    const auto fraction = round_right(
            significand,
            static_cast<unsigned int>(64 - format.exponent_bias -
                                      static_cast<int>(format.fraction_bits) - exponent));
    if (fraction == (1ULL << format.fraction_bits)) {
        set_result(1, 0);
    } else {
        set_result(0, fraction);
        if (conversion.exceptions & FPU_EX_PRECISION)
            conversion.exceptions |= FPU_EX_UNDERFLOW;
    }
    return conversion;
}

} // namespace

F32ConversionResult convertToF32(const FPU_Reg_80& val)
{
    const auto conversion = convertToIEEE(val, {23, 8, 127, 0xFFU});
    F32ConversionResult result = {};
    result.value.raw = static_cast<uint32_t>(conversion.raw);
    result.exceptions = conversion.exceptions;
    result.rounded_up = conversion.rounded_up;
    return result;
}

F64ConversionResult convertToF64(const FPU_Reg_80& val)
{
    const auto conversion = convertToIEEE(val, {52, 11, 1023, 0x7FFU});
    F64ConversionResult result = {};
    result.value.raw = conversion.raw;
    result.exceptions = conversion.exceptions;
    result.rounded_up = conversion.rounded_up;
    return result;
}

namespace {

IntegerConversionResult convertToInteger(const FPU_Reg_80& val,
                                         unsigned int target_bits)
{
    constexpr uint64_t extended_integer_bit = 0x8000'0000'0000'0000ULL;

    IntegerConversionResult conversion = {};
    const auto sign = static_cast<bool>(val.f.sign);
    const auto exponent80 = val.f.exponent;
    auto significand = val.f.mantissa;
    const auto target_min = target_bits == 63
                                    ? std::numeric_limits<int64_t>::min()
                                    : -(1LL << target_bits);

    const auto invalid = [&conversion, target_min]() {
        conversion.value = target_min;
        conversion.exceptions = FPU_EX_INVALID;
        conversion.rounded_up = false;
        return conversion;
    };

    if (exponent80 == 0x7FFFU)
        return invalid();

    if (significand == 0)
        return conversion;

    // An 80-bit subnormal uses an exponent of 1 - bias rather than -bias.
    int exponent = static_cast<int>(exponent80 ? exponent80 : 1) -
                   static_cast<int>(ExpBias);
    while (!(significand & extended_integer_bit)) {
        significand <<= 1;
        --exponent;
    }

    if (exponent > static_cast<int>(target_bits) ||
        (exponent == static_cast<int>(target_bits) &&
         (significand != extended_integer_bit || !sign))) {
        return invalid();
    }

    const auto shift = exponent < static_cast<int>(target_bits)
                               ? static_cast<unsigned int>(63 - exponent)
                               : 0U;
    auto magnitude = shift < 64 ? significand >> shift : 0;
    bool inexact = false;
    bool round_up = false;

    if (shift != 0) {
        if (shift < 64) {
            const auto half = 1ULL << (shift - 1);
            const auto remainder = significand & ((half << 1) - 1);
            inexact = remainder != 0;
            if (fpu.cw.RC == FPUControlWord::RoundMode::Nearest) {
                round_up = remainder > half ||
                           (remainder == half && (magnitude & 1));
            }
        } else {
            inexact = true;
            if (fpu.cw.RC == FPUControlWord::RoundMode::Nearest && shift == 64)
                round_up = significand > extended_integer_bit;
        }
    }

    if (inexact) {
        if (fpu.cw.RC == FPUControlWord::RoundMode::Down)
            round_up = sign;
        else if (fpu.cw.RC == FPUControlWord::RoundMode::Up)
            round_up = !sign;
        magnitude += static_cast<uint64_t>(round_up);
    }

    const auto target_limit = 1ULL << target_bits;
    if (magnitude > target_limit || (magnitude == target_limit && !sign)) {
        return invalid();
    }

    conversion.rounded_up = round_up;
    if (inexact)
        conversion.exceptions = FPU_EX_PRECISION;

    if (sign) {
        conversion.value = magnitude == target_limit
                                   ? target_min
                                   : -static_cast<int64_t>(magnitude);
    } else {
        conversion.value = static_cast<int64_t>(magnitude);
    }
    return conversion;
}

} // namespace

IntegerConversionResult convertToI16(const FPU_Reg_80& val)
{
    return convertToInteger(val, 15);
}

IntegerConversionResult convertToI32(const FPU_Reg_80& val)
{
    return convertToInteger(val, 31);
}

IntegerConversionResult convertToI64(const FPU_Reg_80& val)
{
    return convertToInteger(val, 63);
}

double convertToDouble(const FPU_Reg_80& val)
{
    return convertToF64(val).value.v;
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
