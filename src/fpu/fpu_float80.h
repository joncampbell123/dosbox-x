#ifndef DOSBOX_FPU_FLOAT80_H
#define DOSBOX_FPU_FLOAT80_H

#include "fpu_state.h"

namespace float80 {

extern const FPU_Reg_80::raw_t QNaN;
extern const FPU_Reg_80::raw_t const1;
extern const FPU_Reg_80::raw_t PI;
extern const uint8_t PI_Extra2;
extern const FPU_Reg_80::raw_t L2T;
extern const uint8_t L2T_Extra2;
extern const FPU_Reg_80::raw_t L2E;
extern const uint8_t L2E_Extra2;
extern const FPU_Reg_80::raw_t LG2;
extern const uint8_t LG2_Extra2;
extern const FPU_Reg_80::raw_t LN2;
extern const uint8_t LN2_Extra2;

struct F32ConversionResult {
    FPU_Reg_32 value = {};
    uint16_t exceptions = 0;
    bool rounded_up = false;
};

struct F64ConversionResult {
    FPU_Reg_64 value = {};
    uint16_t exceptions = 0;
    bool rounded_up = false;
};

struct IntegerConversionResult {
    int64_t value = 0;
    uint16_t exceptions = 0;
    bool rounded_up = false;
};

F32ConversionResult convertToF32(const FPU_Reg_80& val);
F64ConversionResult convertToF64(const FPU_Reg_80& val);
IntegerConversionResult convertToI16(const FPU_Reg_80& val);
IntegerConversionResult convertToI32(const FPU_Reg_80& val);
IntegerConversionResult convertToI64(const FPU_Reg_80& val);
double convertToDouble(const FPU_Reg_80& val);
void convertFrom(FPU_Reg_80& result, int64_t value);
void convertFrom(FPU_Reg_80& result, const FPU_Reg_32& value);
void convertFrom(FPU_Reg_80& result, const FPU_Reg_64& value);
void round(FPU_Reg_80& val, uint8_t extra_two_bits);

} // namespace float80

#endif
