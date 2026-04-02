
#include <stdio.h>
#include <stdint.h>

#include "opgen.h"

const struct opcode_t op_00_add = { // ADD r/m(b), reg(b)        0x00 mod/reg/rm
	.opcode_name = "add",
	.pattern_sz = 1,
	.pattern = {0x00},
	.patparam = { CPUPPM_MODREGRM },
	.p_dst = { .p = CPUP_GREG_RM, .s = CPUPS_BYTE },
	.p_src = { { .p = CPUP_GREG_REG, .s = CPUPS_BYTE } }
};
const struct opcode_t op_01_add = { // ADD r/m(w), reg(w)        0x01 mod/reg/rm
	.opcode_name = "add",
	.pattern_sz = 1,
	.pattern = {0x01},
	.patparam = { CPUPPM_MODREGRM },
	.p_dst = { .p = CPUP_GREG_RM, .s = CPUPS_NATIVEWORD },
	.p_src = { { .p = CPUP_GREG_REG, .s = CPUPS_NATIVEWORD } }
};
const struct opcode_t op_02_add = { // ADD reg(b), r/m(b)        0x02 mod/reg/rm
	.opcode_name = "add",
	.pattern_sz = 1,
	.pattern = {0x02},
	.patparam = { CPUPPM_MODREGRM },
	.p_dst = { .p = CPUP_GREG_REG, .s = CPUPS_BYTE },
	.p_src = { { .p = CPUP_GREG_RM, .s = CPUPS_BYTE } }
};
const struct opcode_t op_03_add = { // ADD reg(w), r/m(w)        0x03 mod/reg/rm
	.opcode_name = "add",
	.pattern_sz = 1,
	.pattern = {0x03},
	.patparam = { CPUPPM_MODREGRM },
	.p_dst = { .p = CPUP_GREG_REG, .s = CPUPS_NATIVEWORD },
	.p_src = { { .p = CPUP_GREG_RM, .s = CPUPS_NATIVEWORD } }
};
const struct opcode_t op_04_add = { // ADD a(b), imm(b)          0x04 mod/reg/rm
	.opcode_name = "add",
	.pattern_sz = 1,
	.pattern = {0x04},
	.patparam = { CPUPPM_IMMEDIATEB },
	.p_dst = { .p = CPUP_GREG_A, .s = CPUPS_BYTE },
	.p_src = { { .p = CPUP_IMMEDIATE, .s = CPUPS_BYTE } }
};
const struct opcode_t op_05_add = { // ADD a(w), imm(w)          0x05 mod/reg/rm
	.opcode_name = "add",
	.pattern_sz = 1,
	.pattern = {0x05},
	.patparam = { CPUPPM_IMMEDIATENW },
	.p_dst = { .p = CPUP_GREG_A, .s = CPUPS_NATIVEWORD },
	.p_src = { { .p = CPUP_IMMEDIATE, .s = CPUPS_NATIVEWORD } }
};
const struct opcode_t op_06_push = { // PUSH ES                   0x06
	.opcode_name = "push",
	.pattern_sz = 1,
	.pattern = {0x06},
	.p_src = { { .p = CPUP_SREG_ES, .s = CPUPS_NATIVEWORD } }
};
const struct opcode_t op_07_pop = { // POP ES                    0x07
	.opcode_name = "pop",
	.pattern_sz = 1,
	.pattern = {0x07},
	.p_dst = { .p = CPUP_SREG_ES, .s = CPUPS_NATIVEWORD }
};
const struct opcode_t op_08_or = { // OR r/m(b), reg(b)        0x08 mod/reg/rm
	.opcode_name = "or",
	.pattern_sz = 1,
	.pattern = {0x08},
	.patparam = { CPUPPM_MODREGRM },
	.p_dst = { .p = CPUP_GREG_RM, .s = CPUPS_BYTE },
	.p_src = { { .p = CPUP_GREG_REG, .s = CPUPS_BYTE } }
};
const struct opcode_t op_09_or = { // OR r/m(w), reg(w)        0x09 mod/reg/rm
	.opcode_name = "or",
	.pattern_sz = 1,
	.pattern = {0x09},
	.patparam = { CPUPPM_MODREGRM },
	.p_dst = { .p = CPUP_GREG_RM, .s = CPUPS_NATIVEWORD },
	.p_src = { { .p = CPUP_GREG_REG, .s = CPUPS_NATIVEWORD } }
};
const struct opcode_t op_0a_or = { // OR reg(b), r/m(b)        0x0A mod/reg/rm
	.opcode_name = "or",
	.pattern_sz = 1,
	.pattern = {0x0a},
	.patparam = { CPUPPM_MODREGRM },
	.p_dst = { .p = CPUP_GREG_REG, .s = CPUPS_BYTE },
	.p_src = { { .p = CPUP_GREG_RM, .s = CPUPS_BYTE } }
};
const struct opcode_t op_0b_or = { // OR reg(w), r/m(w)        0x0B mod/reg/rm
	.opcode_name = "or",
	.pattern_sz = 1,
	.pattern = {0x0b},
	.patparam = { CPUPPM_MODREGRM },
	.p_dst = { .p = CPUP_GREG_REG, .s = CPUPS_NATIVEWORD },
	.p_src = { { .p = CPUP_GREG_RM, .s = CPUPS_NATIVEWORD } }
};
const struct opcode_t op_0c_or = { // OR a(b), imm(b)          0x0C mod/reg/rm
	.opcode_name = "or",
	.pattern_sz = 1,
	.pattern = {0x0c},
	.patparam = { CPUPPM_IMMEDIATEB },
	.p_dst = { .p = CPUP_GREG_A, .s = CPUPS_BYTE },
	.p_src = { { .p = CPUP_IMMEDIATE, .s = CPUPS_BYTE } }
};
const struct opcode_t op_0d_or = { // OR a(w), imm(w)          0x0D mod/reg/rm
	.opcode_name = "or",
	.pattern_sz = 1,
	.pattern = {0x0d},
	.patparam = { CPUPPM_IMMEDIATENW },
	.p_dst = { .p = CPUP_GREG_A, .s = CPUPS_NATIVEWORD },
	.p_src = { { .p = CPUP_IMMEDIATE, .s = CPUPS_NATIVEWORD } }
};
const struct opcode_t op_0e_push = { // PUSH CS                   0x0E
	.opcode_name = "push",
	.pattern_sz = 1,
	.pattern = {0x0e},
	.p_src = { { .p = CPUP_SREG_CS, .s = CPUPS_NATIVEWORD } }
};
const struct opcode_t op_0f_pop = { // POP CS                    0x0F
	.opcode_name = "pop",
	.pattern_sz = 1,
	.pattern = {0x0f},
	.p_dst = { .p = CPUP_SREG_CS, .s = CPUPS_NATIVEWORD }
};
const struct opcode_t op_10_adc = { // ADC r/m(b), reg(b)        0x10 mod/reg/rm
	.opcode_name = "adc",
	.pattern_sz = 1,
	.pattern = {0x10},
	.patparam = { CPUPPM_MODREGRM },
	.p_dst = { .p = CPUP_GREG_RM, .s = CPUPS_BYTE },
	.p_src = { { .p = CPUP_GREG_REG, .s = CPUPS_BYTE } }
};
const struct opcode_t op_11_adc = { // ADC r/m(w), reg(w)        0x11 mod/reg/rm
	.opcode_name = "adc",
	.pattern_sz = 1,
	.pattern = {0x11},
	.patparam = { CPUPPM_MODREGRM },
	.p_dst = { .p = CPUP_GREG_RM, .s = CPUPS_NATIVEWORD },
	.p_src = { { .p = CPUP_GREG_REG, .s = CPUPS_NATIVEWORD } }
};
const struct opcode_t op_12_adc = { // ADC reg(b), r/m(b)        0x12 mod/reg/rm
	.opcode_name = "adc",
	.pattern_sz = 1,
	.pattern = {0x12},
	.patparam = { CPUPPM_MODREGRM },
	.p_dst = { .p = CPUP_GREG_REG, .s = CPUPS_BYTE },
	.p_src = { { .p = CPUP_GREG_RM, .s = CPUPS_BYTE } }
};
const struct opcode_t op_13_adc = { // ADC reg(w), r/m(w)        0x13 mod/reg/rm
	.opcode_name = "adc",
	.pattern_sz = 1,
	.pattern = {0x13},
	.patparam = { CPUPPM_MODREGRM },
	.p_dst = { .p = CPUP_GREG_REG, .s = CPUPS_NATIVEWORD },
	.p_src = { { .p = CPUP_GREG_RM, .s = CPUPS_NATIVEWORD } }
};
const struct opcode_t op_14_adc = { // ADC a(b), imm(b)          0x14 mod/reg/rm
	.opcode_name = "adc",
	.pattern_sz = 1,
	.pattern = {0x14},
	.patparam = { CPUPPM_IMMEDIATEB },
	.p_dst = { .p = CPUP_GREG_A, .s = CPUPS_BYTE },
	.p_src = { { .p = CPUP_IMMEDIATE, .s = CPUPS_BYTE } }
};
const struct opcode_t op_15_adc = { // ADC a(w), imm(w)          0x15 mod/reg/rm
	.opcode_name = "adc",
	.pattern_sz = 1,
	.pattern = {0x15},
	.patparam = { CPUPPM_IMMEDIATENW },
	.p_dst = { .p = CPUP_GREG_A, .s = CPUPS_NATIVEWORD },
	.p_src = { { .p = CPUP_IMMEDIATE, .s = CPUPS_NATIVEWORD } }
};
const struct opcode_t op_16_push = { // PUSH SS                   0x16
	.opcode_name = "push",
	.pattern_sz = 1,
	.pattern = {0x16},
	.p_src = { { .p = CPUP_SREG_SS, .s = CPUPS_NATIVEWORD } }
};
const struct opcode_t op_17_pop = { // POP SS                    0x17
	.opcode_name = "pop",
	.pattern_sz = 1,
	.pattern = {0x17},
	.p_dst = { .p = CPUP_SREG_SS, .s = CPUPS_NATIVEWORD }
};
const struct opcode_t op_18_sbb = { // SBB r/m(b), reg(b)        0x18 mod/reg/rm
	.opcode_name = "sbb",
	.pattern_sz = 1,
	.pattern = {0x18},
	.patparam = { CPUPPM_MODREGRM },
	.p_dst = { .p = CPUP_GREG_RM, .s = CPUPS_BYTE },
	.p_src = { { .p = CPUP_GREG_REG, .s = CPUPS_BYTE } }
};
const struct opcode_t op_19_sbb = { // SBB r/m(w), reg(w)        0x19 mod/reg/rm
	.opcode_name = "sbb",
	.pattern_sz = 1,
	.pattern = {0x19},
	.patparam = { CPUPPM_MODREGRM },
	.p_dst = { .p = CPUP_GREG_RM, .s = CPUPS_NATIVEWORD },
	.p_src = { { .p = CPUP_GREG_REG, .s = CPUPS_NATIVEWORD } }
};
const struct opcode_t op_1a_sbb = { // SBB reg(b), r/m(b)        0x1A mod/reg/rm
	.opcode_name = "sbb",
	.pattern_sz = 1,
	.pattern = {0x1a},
	.patparam = { CPUPPM_MODREGRM },
	.p_dst = { .p = CPUP_GREG_REG, .s = CPUPS_BYTE },
	.p_src = { { .p = CPUP_GREG_RM, .s = CPUPS_BYTE } }
};
const struct opcode_t op_1b_sbb = { // SBB reg(w), r/m(w)        0x1B mod/reg/rm
	.opcode_name = "sbb",
	.pattern_sz = 1,
	.pattern = {0x1b},
	.patparam = { CPUPPM_MODREGRM },
	.p_dst = { .p = CPUP_GREG_REG, .s = CPUPS_NATIVEWORD },
	.p_src = { { .p = CPUP_GREG_RM, .s = CPUPS_NATIVEWORD } }
};
const struct opcode_t op_1c_sbb = { // SBB a(b), imm(b)          0x1C mod/reg/rm
	.opcode_name = "sbb",
	.pattern_sz = 1,
	.pattern = {0x1c},
	.patparam = { CPUPPM_IMMEDIATEB },
	.p_dst = { .p = CPUP_GREG_A, .s = CPUPS_BYTE },
	.p_src = { { .p = CPUP_IMMEDIATE, .s = CPUPS_BYTE } }
};
const struct opcode_t op_1d_sbb = { // SBB a(w), imm(w)          0x1D mod/reg/rm
	.opcode_name = "sbb",
	.pattern_sz = 1,
	.pattern = {0x1d},
	.patparam = { CPUPPM_IMMEDIATENW },
	.p_dst = { .p = CPUP_GREG_A, .s = CPUPS_NATIVEWORD },
	.p_src = { { .p = CPUP_IMMEDIATE, .s = CPUPS_NATIVEWORD } }
};
const struct opcode_t op_1e_push = { // PUSH DS                   0x1E
	.opcode_name = "push",
	.pattern_sz = 1,
	.pattern = {0x1e},
	.p_src = { { .p = CPUP_SREG_DS, .s = CPUPS_NATIVEWORD } }
};
const struct opcode_t op_1f_pop = { // POP DS                    0x1F
	.opcode_name = "pop",
	.pattern_sz = 1,
	.pattern = {0x1f},
	.p_dst = { .p = CPUP_SREG_DS, .s = CPUPS_NATIVEWORD }
};
const struct opcode_t op_20_and = { // AND r/m(b), reg(b)        0x20 mod/reg/rm
	.opcode_name = "and",
	.pattern_sz = 1,
	.pattern = {0x20},
	.patparam = { CPUPPM_MODREGRM },
	.p_dst = { .p = CPUP_GREG_RM, .s = CPUPS_BYTE },
	.p_src = { { .p = CPUP_GREG_REG, .s = CPUPS_BYTE } }
};
const struct opcode_t op_21_and = { // AND r/m(w), reg(w)        0x21 mod/reg/rm
	.opcode_name = "and",
	.pattern_sz = 1,
	.pattern = {0x21},
	.patparam = { CPUPPM_MODREGRM },
	.p_dst = { .p = CPUP_GREG_RM, .s = CPUPS_NATIVEWORD },
	.p_src = { { .p = CPUP_GREG_REG, .s = CPUPS_NATIVEWORD } }
};
const struct opcode_t op_22_and = { // AND reg(b), r/m(b)        0x22 mod/reg/rm
	.opcode_name = "and",
	.pattern_sz = 1,
	.pattern = {0x22},
	.patparam = { CPUPPM_MODREGRM },
	.p_dst = { .p = CPUP_GREG_REG, .s = CPUPS_BYTE },
	.p_src = { { .p = CPUP_GREG_RM, .s = CPUPS_BYTE } }
};
const struct opcode_t op_23_and = { // AND reg(w), r/m(w)        0x23 mod/reg/rm
	.opcode_name = "and",
	.pattern_sz = 1,
	.pattern = {0x23},
	.patparam = { CPUPPM_MODREGRM },
	.p_dst = { .p = CPUP_GREG_REG, .s = CPUPS_NATIVEWORD },
	.p_src = { { .p = CPUP_GREG_RM, .s = CPUPS_NATIVEWORD } }
};
const struct opcode_t op_24_and = { // AND a(b), imm(b)          0x24 mod/reg/rm
	.opcode_name = "and",
	.pattern_sz = 1,
	.pattern = {0x24},
	.patparam = { CPUPPM_IMMEDIATEB },
	.p_dst = { .p = CPUP_GREG_A, .s = CPUPS_BYTE },
	.p_src = { { .p = CPUP_IMMEDIATE, .s = CPUPS_BYTE } }
};
const struct opcode_t op_25_and = { // AND a(w), imm(w)          0x25 mod/reg/rm
	.opcode_name = "and",
	.pattern_sz = 1,
	.pattern = {0x25},
	.patparam = { CPUPPM_IMMEDIATENW },
	.p_dst = { .p = CPUP_GREG_A, .s = CPUPS_NATIVEWORD },
	.p_src = { { .p = CPUP_IMMEDIATE, .s = CPUPS_NATIVEWORD } }
};
const struct opcode_t op_26_es_segov = { // ES:                       0x26
	.opcode_name = "es:",
	.pattern_sz = 1,
	.pattern = {0x26},
	.flags = CPUFL_PREFIX_SEGMENT
};
const struct opcode_t op_27_daa = { // DAA                       0x27
	.opcode_name = "daa",
	.pattern_sz = 1,
	.pattern = {0x27},
	.p_dst = { .p = CPUP_GREG_AL, .s = CPUPS_BYTE },
	.p_src = { { .p = CPUP_GREG_AL, .s = CPUPS_BYTE } }
};
const struct opcode_t op_28_sub = { // SUB r/m(b), reg(b)        0x28 mod/reg/rm
	.opcode_name = "sub",
	.pattern_sz = 1,
	.pattern = {0x28},
	.patparam = { CPUPPM_MODREGRM },
	.p_dst = { .p = CPUP_GREG_RM, .s = CPUPS_BYTE },
	.p_src = { { .p = CPUP_GREG_REG, .s = CPUPS_BYTE } }
};
const struct opcode_t op_29_sub = { // SUB r/m(w), reg(w)        0x29 mod/reg/rm
	.opcode_name = "sub",
	.pattern_sz = 1,
	.pattern = {0x29},
	.patparam = { CPUPPM_MODREGRM },
	.p_dst = { .p = CPUP_GREG_RM, .s = CPUPS_NATIVEWORD },
	.p_src = { { .p = CPUP_GREG_REG, .s = CPUPS_NATIVEWORD } }
};
const struct opcode_t op_2a_sub = { // SUB reg(b), r/m(b)        0x2A mod/reg/rm
	.opcode_name = "sub",
	.pattern_sz = 1,
	.pattern = {0x2A},
	.patparam = { CPUPPM_MODREGRM },
	.p_dst = { .p = CPUP_GREG_REG, .s = CPUPS_BYTE },
	.p_src = { { .p = CPUP_GREG_RM, .s = CPUPS_BYTE } }
};
const struct opcode_t op_2b_sub = { // SUB reg(w), r/m(w)        0x2B mod/reg/rm
	.opcode_name = "sub",
	.pattern_sz = 1,
	.pattern = {0x2B},
	.patparam = { CPUPPM_MODREGRM },
	.p_dst = { .p = CPUP_GREG_REG, .s = CPUPS_NATIVEWORD },
	.p_src = { { .p = CPUP_GREG_RM, .s = CPUPS_NATIVEWORD } }
};
const struct opcode_t op_2c_sub = { // SUB a(b), imm(b)          0x2C mod/reg/rm
	.opcode_name = "sub",
	.pattern_sz = 1,
	.pattern = {0x2C},
	.patparam = { CPUPPM_IMMEDIATEB },
	.p_dst = { .p = CPUP_GREG_A, .s = CPUPS_BYTE },
	.p_src = { { .p = CPUP_IMMEDIATE, .s = CPUPS_BYTE } }
};
const struct opcode_t op_2d_sub = { // SUB a(w), imm(w)          0x2D mod/reg/rm
	.opcode_name = "sub",
	.pattern_sz = 1,
	.pattern = {0x2D},
	.patparam = { CPUPPM_IMMEDIATENW },
	.p_dst = { .p = CPUP_GREG_A, .s = CPUPS_NATIVEWORD },
	.p_src = { { .p = CPUP_IMMEDIATE, .s = CPUPS_NATIVEWORD } }
};
const struct opcode_t op_2e_cs_segov = { // CS:                       0x2E
	.opcode_name = "cs:",
	.pattern_sz = 1,
	.pattern = {0x2E},
	.flags = CPUFL_PREFIX_SEGMENT
};
const struct opcode_t op_2f_das = { // DAS                       0x2F
	.opcode_name = "das",
	.pattern_sz = 1,
	.pattern = {0x2F},
	.p_dst = { .p = CPUP_GREG_AL, .s = CPUPS_BYTE },
	.p_src = { { .p = CPUP_GREG_AL, .s = CPUPS_BYTE } }
};
const struct opcode_t op_30_xor = { // XOR r/m(b), reg(b)        0x30 mod/reg/rm
	.opcode_name = "xor",
	.pattern_sz = 1,
	.pattern = {0x30},
	.patparam = { CPUPPM_MODREGRM },
	.p_dst = { .p = CPUP_GREG_RM, .s = CPUPS_BYTE },
	.p_src = { { .p = CPUP_GREG_REG, .s = CPUPS_BYTE } }
};
const struct opcode_t op_31_xor = { // XOR r/m(w), reg(w)        0x31 mod/reg/rm
	.opcode_name = "xor",
	.pattern_sz = 1,
	.pattern = {0x31},
	.patparam = { CPUPPM_MODREGRM },
	.p_dst = { .p = CPUP_GREG_RM, .s = CPUPS_NATIVEWORD },
	.p_src = { { .p = CPUP_GREG_REG, .s = CPUPS_NATIVEWORD } }
};
const struct opcode_t op_32_xor = { // XOR reg(b), r/m(b)        0x32 mod/reg/rm
	.opcode_name = "xor",
	.pattern_sz = 1,
	.pattern = {0x32},
	.patparam = { CPUPPM_MODREGRM },
	.p_dst = { .p = CPUP_GREG_REG, .s = CPUPS_BYTE },
	.p_src = { { .p = CPUP_GREG_RM, .s = CPUPS_BYTE } }
};
const struct opcode_t op_33_xor = { // XOR reg(w), r/m(w)        0x33 mod/reg/rm
	.opcode_name = "xor",
	.pattern_sz = 1,
	.pattern = {0x33},
	.patparam = { CPUPPM_MODREGRM },
	.p_dst = { .p = CPUP_GREG_REG, .s = CPUPS_NATIVEWORD },
	.p_src = { { .p = CPUP_GREG_RM, .s = CPUPS_NATIVEWORD } }
};
const struct opcode_t op_34_xor = { // XOR a(b), imm(b)          0x34 mod/reg/rm
	.opcode_name = "xor",
	.pattern_sz = 1,
	.pattern = {0x34},
	.patparam = { CPUPPM_IMMEDIATEB },
	.p_dst = { .p = CPUP_GREG_A, .s = CPUPS_BYTE },
	.p_src = { { .p = CPUP_IMMEDIATE, .s = CPUPS_BYTE } }
};
const struct opcode_t op_35_xor = { // XOR a(w), imm(w)          0x35 mod/reg/rm
	.opcode_name = "xor",
	.pattern_sz = 1,
	.pattern = {0x35},
	.patparam = { CPUPPM_IMMEDIATENW },
	.p_dst = { .p = CPUP_GREG_A, .s = CPUPS_NATIVEWORD },
	.p_src = { { .p = CPUP_IMMEDIATE, .s = CPUPS_NATIVEWORD } }
};
const struct opcode_t op_36_ss_segov = { // SS:                       0x36
	.opcode_name = "ss:",
	.pattern_sz = 1,
	.pattern = {0x36},
	.flags = CPUFL_PREFIX_SEGMENT
};
const struct opcode_t op_37_aaa = { // AAA                       0x37
	.opcode_name = "aaa",
	.pattern_sz = 1,
	.pattern = {0x37},
	.p_dst = { .p = CPUP_GREG_AL, .s = CPUPS_BYTE },
	.p_src = { { .p = CPUP_GREG_AL, .s = CPUPS_BYTE } }
};

const struct opcode_t op_38_cmp = { // CMP r/m(b), reg(b)        0x38 mod/reg/rm
	.opcode_name = "cmp",
	.pattern_sz = 1,
	.pattern = {0x38},
	.patparam = { CPUPPM_MODREGRM },
	.p_dst = { .p = CPUP_GREG_RM, .s = CPUPS_BYTE },
	.p_src = { { .p = CPUP_GREG_REG, .s = CPUPS_BYTE } }
};
const struct opcode_t op_39_cmp = { // CMP r/m(w), reg(w)        0x39 mod/reg/rm
	.opcode_name = "cmp",
	.pattern_sz = 1,
	.pattern = {0x39},
	.patparam = { CPUPPM_MODREGRM },
	.p_dst = { .p = CPUP_GREG_RM, .s = CPUPS_NATIVEWORD },
	.p_src = { { .p = CPUP_GREG_REG, .s = CPUPS_NATIVEWORD } }
};
const struct opcode_t op_3a_cmp = { // CMP reg(b), r/m(b)        0x3A mod/reg/rm
	.opcode_name = "cmp",
	.pattern_sz = 1,
	.pattern = {0x3A},
	.patparam = { CPUPPM_MODREGRM },
	.p_dst = { .p = CPUP_GREG_REG, .s = CPUPS_BYTE },
	.p_src = { { .p = CPUP_GREG_RM, .s = CPUPS_BYTE } }
};
const struct opcode_t op_3b_cmp = { // CMP reg(w), r/m(w)        0x3B mod/reg/rm
	.opcode_name = "cmp",
	.pattern_sz = 1,
	.pattern = {0x3B},
	.patparam = { CPUPPM_MODREGRM },
	.p_dst = { .p = CPUP_GREG_REG, .s = CPUPS_NATIVEWORD },
	.p_src = { { .p = CPUP_GREG_RM, .s = CPUPS_NATIVEWORD } }
};
const struct opcode_t op_3c_cmp = { // CMP a(b), imm(b)          0x3C mod/reg/rm
	.opcode_name = "cmp",
	.pattern_sz = 1,
	.pattern = {0x3C},
	.patparam = { CPUPPM_IMMEDIATEB },
	.p_dst = { .p = CPUP_GREG_A, .s = CPUPS_BYTE },
	.p_src = { { .p = CPUP_IMMEDIATE, .s = CPUPS_BYTE } }
};
const struct opcode_t op_3d_cmp = { // CMP a(w), imm(w)          0x3D mod/reg/rm
	.opcode_name = "cmp",
	.pattern_sz = 1,
	.pattern = {0x3D},
	.patparam = { CPUPPM_IMMEDIATENW },
	.p_dst = { .p = CPUP_GREG_A, .s = CPUPS_NATIVEWORD },
	.p_src = { { .p = CPUP_IMMEDIATE, .s = CPUPS_NATIVEWORD } }
};
const struct opcode_t op_3e_ds_segov = { // DS:                       0x3E
	.opcode_name = "ds:",
	.pattern_sz = 1,
	.pattern = {0x3E},
	.flags = CPUFL_PREFIX_SEGMENT
};
const struct opcode_t op_3f_aas = { // AAS                       0x3F
	.opcode_name = "aas",
	.pattern_sz = 1,
	.pattern = {0x3F},
	.p_dst = { .p = CPUP_GREG_AL, .s = CPUPS_BYTE },
	.p_src = { { .p = CPUP_GREG_AL, .s = CPUPS_BYTE } }
};

const struct opcode_t op_40_inc = { // INC <w>                    0x40-0x47
	.opcode_name = "inc",
	.pattern_sz = 1,
	.pattern = {0x40},
	.pattern_lb_mask = { .mask=0x07, .match=0xFF }, // match 0x40-0x47
	.flags = CPUFL_REG_FROM_OPCODE,
	.p_dst = { .p = CPUP_GREG_REG, .s = CPUPS_NATIVEWORD }
};

// general instruction decoding (8086 level)
const struct opcode_t* oplist_gen_8086[] = {
	&op_00_add,
	&op_01_add,
	&op_02_add,
	&op_03_add,
	&op_04_add,
	&op_05_add,
	&op_06_push,
	&op_07_pop,
	&op_08_or,
	&op_09_or,
	&op_0a_or,
	&op_0b_or,
	&op_0c_or,
	&op_0d_or,
	&op_0e_push,
	&op_0f_pop,
	&op_10_adc,
	&op_11_adc,
	&op_12_adc,
	&op_13_adc,
	&op_14_adc,
	&op_15_adc,
	&op_16_push,
	&op_17_pop,
	&op_18_sbb,
	&op_19_sbb,
	&op_1a_sbb,
	&op_1b_sbb,
	&op_1c_sbb,
	&op_1d_sbb,
	&op_1e_push,
	&op_1f_pop,
	&op_20_and,
	&op_21_and,
	&op_22_and,
	&op_23_and,
	&op_24_and,
	&op_25_and,
	&op_26_es_segov,
	&op_27_daa,
	&op_28_sub,
	&op_29_sub,
	&op_2a_sub,
	&op_2b_sub,
	&op_2c_sub,
	&op_2d_sub,
	&op_2e_cs_segov,
	&op_2f_das,
	&op_30_xor,
	&op_31_xor,
	&op_32_xor,
	&op_33_xor,
	&op_34_xor,
	&op_35_xor,
	&op_36_ss_segov,
	&op_37_aaa,
	&op_38_cmp,
	&op_39_cmp,
	&op_3a_cmp,
	&op_3b_cmp,
	&op_3c_cmp,
	&op_3d_cmp,
	&op_3e_ds_segov,
	&op_3f_aas,
	&op_40_inc,

	NULL
};

