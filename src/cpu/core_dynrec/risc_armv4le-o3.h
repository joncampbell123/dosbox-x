/*
 *  Copyright (C) 2002-2018  The DOSBox Team
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */



/* ARMv4/ARMv7 (little endian) backend by M-HT (arm version) */


// temporary registers
#define temp1 HOST_ip
#define temp2 HOST_v3
#define temp3 HOST_v4

// register that holds function return values
#define FC_RETOP HOST_a1

// register used for address calculations,
#define FC_ADDR HOST_v1			// has to be saved across calls, see DRC_PROTECT_ADDR_REG

// register that holds the first parameter
#define FC_OP1 HOST_a1

// register that holds the second parameter
#define FC_OP2 HOST_a2

// special register that holds the third parameter for _R3 calls (byte accessible)
#define FC_OP3 HOST_v2

// register that holds byte-accessible temporary values
#define FC_TMP_BA1 HOST_a1

// register that holds byte-accessible temporary values
#define FC_TMP_BA2 HOST_a2

// temporary register for LEA
#define TEMP_REG_DRC HOST_v2

// used to hold the address of "cpu_regs" - preferably filled in function gen_run_code
#define FC_REGS_ADDR HOST_v7

// used to hold the address of "Segs" - preferably filled in function gen_run_code
#define FC_SEGS_ADDR HOST_v8

// used to hold the address of "core_dynrec.readdata" - filled in function gen_run_code
#define readdata_addr HOST_v5


// helper macro
#define ROTATE_SCALE(x) ( (x)?(32 - x):(0) )


// instruction encodings

// move
// mov dst, #(imm ror rimm)		@	0 <= imm <= 255	&	rimm mod 2 = 0
#define MOV_IMM(dst, imm, rimm) (0xe3a00000 + ((dst) << 12) + (imm) + ((rimm) << 7) )
// mov dst, src, lsl #imm
#define MOV_REG_LSL_IMM(dst, src, imm) (0xe1a00000 + ((dst) << 12) + (src) + ((imm) << 7) )
// movs dst, src, lsl #imm
#define MOVS_REG_LSL_IMM(dst, src, imm) (0xe1b00000 + ((dst) << 12) + (src) + ((imm) << 7) )
// mov dst, src, lsr #imm
#define MOV_REG_LSR_IMM(dst, src, imm) (0xe1a00020 + ((dst) << 12) + (src) + ((imm) << 7) )
// mov dst, src, asr #imm
#define MOV_REG_ASR_IMM(dst, src, imm) (0xe1a00040 + ((dst) << 12) + (src) + ((imm) << 7) )
// mov dst, src, lsl rreg
#define MOV_REG_LSL_REG(dst, src, rreg) (0xe1a00010 + ((dst) << 12) + (src) + ((rreg) << 8) )
// mov dst, src, lsr rreg
#define MOV_REG_LSR_REG(dst, src, rreg) (0xe1a00030 + ((dst) << 12) + (src) + ((rreg) << 8) )
// mov dst, src, asr rreg
#define MOV_REG_ASR_REG(dst, src, rreg) (0xe1a00050 + ((dst) << 12) + (src) + ((rreg) << 8) )
// mov dst, src, ror rreg
#define MOV_REG_ROR_REG(dst, src, rreg) (0xe1a00070 + ((dst) << 12) + (src) + ((rreg) << 8) )
// mvn dst, #(imm ror rimm)		@	0 <= imm <= 255	&	rimm mod 2 = 0
#define MVN_IMM(dst, imm, rimm) (0xe3e00000 + ((dst) << 12) + (imm) + ((rimm) << 7) )
#if C_TARGETCPU == ARMV7LE
// movw dst, #imm		@	0 <= imm <= 65535
#define MOVW(dst, imm) (0xe3000000 + ((dst) << 12) + (((imm) & 0xf000) << 4) + ((imm) & 0x0fff) )
// movt dst, #imm		@	0 <= imm <= 65535
#define MOVT(dst, imm) (0xe3400000 + ((dst) << 12) + (((imm) & 0xf000) << 4) + ((imm) & 0x0fff) )
#endif

// arithmetic
// add dst, src, #(imm ror rimm)		@	0 <= imm <= 255	&	rimm mod 2 = 0
#define ADD_IMM(dst, src, imm, rimm) (0xe2800000 + ((dst) << 12) + ((src) << 16) + (imm) + ((rimm) << 7) )
// add dst, src1, src2, lsl #imm
#define ADD_REG_LSL_IMM(dst, src1, src2, imm) (0xe0800000 + ((dst) << 12) + ((src1) << 16) + (src2) + ((imm) << 7) )
// sub dst, src, #(imm ror rimm)		@	0 <= imm <= 255	&	rimm mod 2 = 0
#define SUB_IMM(dst, src, imm, rimm) (0xe2400000 + ((dst) << 12) + ((src) << 16) + (imm) + ((rimm) << 7) )
// sub dst, src1, src2, lsl #imm
#define SUB_REG_LSL_IMM(dst, src1, src2, imm) (0xe0400000 + ((dst) << 12) + ((src1) << 16) + (src2) + ((imm) << 7) )
// rsb dst, src, #(imm ror rimm)		@	0 <= imm <= 255	&	rimm mod 2 = 0
#define RSB_IMM(dst, src, imm, rimm) (0xe2600000 + ((dst) << 12) + ((src) << 16) + (imm) + ((rimm) << 7) )
// cmp src, #(imm ror rimm)		@	0 <= imm <= 255	&	rimm mod 2 = 0
#define CMP_IMM(src, imm, rimm) (0xe3500000 + ((src) << 16) + (imm) + ((rimm) << 7) )
// nop
#if C_TARGETCPU == ARMV7LE
#define NOP (0xe320f000)
#else
#define NOP MOV_REG_LSL_IMM(HOST_r0, HOST_r0, 0)
#endif

// logical
// tst src, #(imm ror rimm)		@	0 <= imm <= 255	&	rimm mod 2 = 0
#define TST_IMM(src, imm, rimm) (0xe3100000 + ((src) << 16) + (imm) + ((rimm) << 7) )
// and dst, src, #(imm ror rimm)		@	0 <= imm <= 255	&	rimm mod 2 = 0
#define AND_IMM(dst, src, imm, rimm) (0xe2000000 + ((dst) << 12) + ((src) << 16) + (imm) + ((rimm) << 7) )
// and dst, src1, src2, lsl #imm
#define AND_REG_LSL_IMM(dst, src1, src2, imm) (0xe0000000 + ((dst) << 12) + ((src1) << 16) + (src2) + ((imm) << 7) )
// orr dst, src, #(imm ror rimm)		@	0 <= imm <= 255	&	rimm mod 2 = 0
#define ORR_IMM(dst, src, imm, rimm) (0xe3800000 + ((dst) << 12) + ((src) << 16) + (imm) + ((rimm) << 7) )
// orr dst, src1, src2, lsl #imm
#define ORR_REG_LSL_IMM(dst, src1, src2, imm) (0xe1800000 + ((dst) << 12) + ((src1) << 16) + (src2) + ((imm) << 7) )
// orr dst, src1, src2, lsr #imm
#define ORR_REG_LSR_IMM(dst, src1, src2, imm) (0xe1800020 + ((dst) << 12) + ((src1) << 16) + (src2) + ((imm) << 7) )
// eor dst, src1, src2, lsl #imm
#define EOR_REG_LSL_IMM(dst, src1, src2, imm) (0xe0200000 + ((dst) << 12) + ((src1) << 16) + (src2) + ((imm) << 7) )
// bic dst, src, #(imm ror rimm)		@	0 <= imm <= 255	&	rimm mod 2 = 0
#define BIC_IMM(dst, src, imm, rimm) (0xe3c00000 + ((dst) << 12) + ((src) << 16) + (imm) + ((rimm) << 7) )
// bic dst, src1, src2, lsl #imm		@	0 <= imm <= 31
#define BIC_REG_LSL_IMM(dst, src1, src2, imm) (0xe1c00000 + ((dst) << 12) + ((src1) << 16) + (src2) + ((imm) << 7) )

// load
// ldr reg, [addr, #imm]		@	0 <= imm < 4096
#define LDR_IMM(reg, addr, imm) (0xe5900000 + ((reg) << 12) + ((addr) << 16) + (imm) )
// ldr reg, [addr, #-(imm)]		@	0 <= imm < 4096
#define LDR_IMM_M(reg, addr, imm) (0xe5100000 + ((reg) << 12) + ((addr) << 16) + (imm) )
// ldrh reg, [addr, #imm]		@	0 <= imm < 256
#define LDRH_IMM(reg, addr, imm) (0xe1d000b0 + ((reg) << 12) + ((addr) << 16) + (((imm) & 0xf0) << 4) + ((imm) & 0x0f) )
// ldrh reg, [addr, #-(imm)]		@	0 <= imm < 256
#define LDRH_IMM_M(reg, addr, imm) (0xe15000b0 + ((reg) << 12) + ((addr) << 16) + (((imm) & 0xf0) << 4) + ((imm) & 0x0f) )
// ldrb reg, [addr, #imm]		@	0 <= imm < 4096
#define LDRB_IMM(reg, addr, imm) (0xe5d00000 + ((reg) << 12) + ((addr) << 16) + (imm) )
// ldrb reg, [addr, #-(imm)]		@	0 <= imm < 4096
#define LDRB_IMM_M(reg, addr, imm) (0xe5500000 + ((reg) << 12) + ((addr) << 16) + (imm) )
// ldr reg, [addr1, addr2, lsl #imm]		@	0 <= imm < 31
#define LDR_REG_LSL_IMM(reg, addr1, addr2, imm) (0xe7900000 + ((reg) << 12) + ((addr1) << 16) + (addr2) + ((imm) << 7) )

// store
// str reg, [addr, #imm]		@	0 <= imm < 4096
#define STR_IMM(reg, addr, imm) (0xe5800000 + ((reg) << 12) + ((addr) << 16) + (imm) )
// str reg, [addr, #-(imm)]		@	0 <= imm < 4096
#define STR_IMM_M(reg, addr, imm) (0xe5000000 + ((reg) << 12) + ((addr) << 16) + (imm) )
// strh reg, [addr, #imm]		@	0 <= imm < 256
#define STRH_IMM(reg, addr, imm) (0xe1c000b0 + ((reg) << 12) + ((addr) << 16) + (((imm) & 0xf0) << 4) + ((imm) & 0x0f) )
// strh reg, [addr, #-(imm)]		@	0 <= imm < 256
#define STRH_IMM_M(reg, addr, imm) (0xe14000b0 + ((reg) << 12) + ((addr) << 16) + (((imm) & 0xf0) << 4) + ((imm) & 0x0f) )
// strb reg, [addr, #imm]		@	0 <= imm < 4096
#define STRB_IMM(reg, addr, imm) (0xe5c00000 + ((reg) << 12) + ((addr) << 16) + (imm) )
// strb reg, [addr, #-(imm)]		@	0 <= imm < 4096
#define STRB_IMM_M(reg, addr, imm) (0xe5400000 + ((reg) << 12) + ((addr) << 16) + (imm) )

// branch
// beq pc+imm		@	0 <= imm < 32M	&	imm mod 4 = 0
#define BEQ_FWD(imm) (0x0a000000 + ((imm) >> 2) )
// bne pc+imm		@	0 <= imm < 32M	&	imm mod 4 = 0
#define BNE_FWD(imm) (0x1a000000 + ((imm) >> 2) )
// ble pc+imm		@	0 <= imm < 32M	&	imm mod 4 = 0
#define BLE_FWD(imm) (0xda000000 + ((imm) >> 2) )
// b pc+imm		@	0 <= imm < 32M	&	imm mod 4 = 0
#define B_FWD(imm) (0xea000000 + ((imm) >> 2) )
// bx reg
#define BX(reg) (0xe12fff10 + (reg) )
#if C_TARGETCPU == ARMV7LE
// blx reg
#define BLX_REG(reg) (0xe12fff30 + (reg) )

// extend
// sxth dst, src, ror #rimm		@	rimm = 0 | 8 | 16 | 24
#define SXTH(dst, src, rimm) (0xe6bf0070 + ((dst) << 12) + (src) + (((rimm) & 24) << 7) )
// sxtb dst, src, ror #rimm		@	rimm = 0 | 8 | 16 | 24
#define SXTB(dst, src, rimm) (0xe6af0070 + ((dst) << 12) + (src) + (((rimm) & 24) << 7) )
// uxth dst, src, ror #rimm		@	rimm = 0 | 8 | 16 | 24
#define UXTH(dst, src, rimm) (0xe6ff0070 + ((dst) << 12) + (src) + (((rimm) & 24) << 7) )
// uxtb dst, src, ror #rimm		@	rimm = 0 | 8 | 16 | 24
#define UXTB(dst, src, rimm) (0xe6ef0070 + ((dst) << 12) + (src) + (((rimm) & 24) << 7) )

// bit field
// bfi dst, src, #lsb, #width		@	lsb >= 0, width >= 1, lsb+width <= 32
#define BFI(dst, src, lsb, width) (0xe7c00010 + ((dst) << 12) + (src) + ((lsb) << 7) + (((lsb) + (width) - 1) << 16) )
// bfc dst, #lsb, #width		@	lsb >= 0, width >= 1, lsb+width <= 32
#define BFC(dst, lsb, width) (0xe7c0001f + ((dst) << 12) + ((lsb) << 7) + (((lsb) + (width) - 1) << 16) )
#endif


// move a full register from reg_src to reg_dst
static void gen_mov_regs(HostReg reg_dst,HostReg reg_src) {
	if(reg_src == reg_dst) return;
	cache_addd( MOV_REG_LSL_IMM(reg_dst, reg_src, 0) );      // mov reg_dst, reg_src
}

// helper function
static bool val_is_operand2(Bit32u value, Bit32u *val_shift) {
	Bit32u shift;

	if (GCC_UNLIKELY(value == 0)) {
		*val_shift = 0;
		return true;
	}

	shift = 0;
	while ((value & 3) == 0) {
		value>>=2;
		shift+=2;
	}

	if ((value >> 8) != 0) return false;

	*val_shift = shift;
	return true;
}

#if C_TARGETCPU != ARMV7LE
// helper function
static Bits get_imm_gen_len(Bit32u imm) {
	Bits ret;
	if (imm == 0) {
		return 1;
	} else {
		ret = 0;
		while (imm) {
			while ((imm & 3) == 0) {
				imm>>=2;
			}
			ret++;
			imm>>=8;
		}
		return ret;
	}
}

// helper function
static Bits get_min_imm_gen_len(Bit32u imm) {
	Bits num1, num2;

	num1 = get_imm_gen_len(imm);
	num2 = get_imm_gen_len(~imm);

	return (num1 <= num2)?num1:num2;
}
#endif

// move a 32bit constant value into dest_reg
static void gen_mov_dword_to_reg_imm(HostReg dest_reg,Bit32u imm) {
#if C_TARGETCPU == ARMV7LE
	Bit32u scale;

	if ( val_is_operand2(imm, &scale) ) {
		cache_addd( MOV_IMM(dest_reg, imm >> scale, ROTATE_SCALE(scale)) );      // mov dest_reg, #imm
	} else if ( val_is_operand2(~imm, &scale) ) {
		cache_addd( MVN_IMM(dest_reg, (~imm) >> scale, ROTATE_SCALE(scale)) );      // mvn dest_reg, #~imm
	} else {
		cache_addd( MOVW(dest_reg, imm & 0xffff) );      // movw dest_reg, #(imm & 0xffff)

		if (imm >= 0x10000)
		{
			cache_addd( MOVT(dest_reg, imm >> 16) );      // movt dest_reg, #(imm >> 16)
		}
	}
#else
	Bit32u imm2, first, scale;

	scale = 0;
	first = 1;
	imm2 = ~imm;

	if (get_imm_gen_len(imm) <= get_imm_gen_len(imm2)) {
		if (imm == 0) {
			cache_addd( MOV_IMM(dest_reg, 0, 0) );      // mov dest_reg, #0
		} else {
			while (imm) {
				while ((imm & 3) == 0) {
					imm>>=2;
					scale+=2;
				}
				if (first) {
					cache_addd( MOV_IMM(dest_reg, imm & 0xff, ROTATE_SCALE(scale)) );      // mov dest_reg, #((imm & 0xff) << scale)
					first = 0;
				} else {
					cache_addd( ORR_IMM(dest_reg, dest_reg, imm & 0xff, ROTATE_SCALE(scale)) );      // orr dest_reg, dest_reg, #((imm & 0xff) << scale)
				}
				imm>>=8;
				scale+=8;
			}
		}
	} else {
		if (imm2 == 0) {
			cache_addd( MVN_IMM(dest_reg, 0, 0) );      // mvn dest_reg, #0
		} else {
			while (imm2) {
				while ((imm2 & 3) == 0) {
					imm2>>=2;
					scale+=2;
				}
				if (first) {
					cache_addd( MVN_IMM(dest_reg, imm2 & 0xff, ROTATE_SCALE(scale)) );      // mvn dest_reg, #((imm2 & 0xff) << scale)
					first = 0;
				} else {
					cache_addd( BIC_IMM(dest_reg, dest_reg, imm2 & 0xff, ROTATE_SCALE(scale)) );      // bic dest_reg, dest_reg, #((imm2 & 0xff) << scale)
				}
				imm2>>=8;
				scale+=8;
			}
		}
	}
#endif
}

// helper function
static bool gen_mov_memval_to_reg_helper(HostReg dest_reg, Bit32u data, Bitu size, HostReg addr_reg, Bit32u addr_data) {
	switch (size) {
		case 4:
#if !(defined(C_UNALIGNED_MEMORY) || (C_TARGETCPU == ARMV7LE))
			if ((data & 3) == 0)
#endif
			{
				if ((data >= addr_data) && (data < addr_data + 4096)) {
					cache_addd( LDR_IMM(dest_reg, addr_reg, data - addr_data) );      // ldr dest_reg, [addr_reg, #(data - addr_data)]
					return true;
				} else if ((data < addr_data) && (data > addr_data - 4096)) {
					cache_addd( LDR_IMM_M(dest_reg, addr_reg, addr_data - data) );      // ldr dest_reg, [addr_reg, #-(addr_data - data)]
					return true;
				}
			}
			break;
		case 2:
#if !(defined(C_UNALIGNED_MEMORY) || (C_TARGETCPU == ARMV7LE))
			if ((data & 1) == 0)
#endif
			{
				if ((data >= addr_data) && (data < addr_data + 256)) {
					cache_addd( LDRH_IMM(dest_reg, addr_reg, data - addr_data) );      // ldrh dest_reg, [addr_reg, #(data - addr_data)]
					return true;
				} else if ((data < addr_data) && (data > addr_data - 256)) {
					cache_addd( LDRH_IMM_M(dest_reg, addr_reg, addr_data - data) );      // ldrh dest_reg, [addr_reg, #-(addr_data - data)]
					return true;
				}
			}
			break;
		case 1:
			if ((data >= addr_data) && (data < addr_data + 4096)) {
				cache_addd( LDRB_IMM(dest_reg, addr_reg, data - addr_data) );      // ldrb dest_reg, [addr_reg, #(data - addr_data)]
				return true;
			} else if ((data < addr_data) && (data > addr_data - 4096)) {
				cache_addd( LDRB_IMM_M(dest_reg, addr_reg, addr_data - data) );      // ldrb dest_reg, [addr_reg, #-(addr_data - data)]
				return true;
			}
		default:
			break;
	}
	return false;
}

// helper function
static bool gen_mov_memval_to_reg(HostReg dest_reg, void *data, Bitu size) {
	if (gen_mov_memval_to_reg_helper(dest_reg, (Bit32u)data, size, FC_REGS_ADDR, (Bit32u)&cpu_regs)) return true;
	if (gen_mov_memval_to_reg_helper(dest_reg, (Bit32u)data, size, readdata_addr, (Bit32u)&core_dynrec.readdata)) return true;
	if (gen_mov_memval_to_reg_helper(dest_reg, (Bit32u)data, size, FC_SEGS_ADDR, (Bit32u)&Segs)) return true;
	return false;
}

// helper function for gen_mov_word_to_reg
static void gen_mov_word_to_reg_helper(HostReg dest_reg,void* data,bool dword,HostReg data_reg) {
	// alignment....
	if (dword) {
#if !(defined(C_UNALIGNED_MEMORY) || (C_TARGETCPU == ARMV7LE))
		if ((Bit32u)data & 3) {
			if ( ((Bit32u)data & 3) == 2 ) {
				cache_addd( LDRH_IMM(dest_reg, data_reg, 0) );      // ldrh dest_reg, [data_reg]
				cache_addd( LDRH_IMM(temp2, data_reg, 2) );      // ldrh temp2, [data_reg, #2]
				cache_addd( ORR_REG_LSL_IMM(dest_reg, dest_reg, temp2, 16) );      // orr dest_reg, dest_reg, temp2, lsl #16
			} else {
				cache_addd( LDRB_IMM(dest_reg, data_reg, 0) );      // ldrb dest_reg, [data_reg]
				cache_addd( LDRH_IMM(temp2, data_reg, 1) );      // ldrh temp2, [data_reg, #1]
				cache_addd( ORR_REG_LSL_IMM(dest_reg, dest_reg, temp2, 8) );      // orr dest_reg, dest_reg, temp2, lsl #8
				cache_addd( LDRB_IMM(temp2, data_reg, 3) );      // ldrb temp2, [data_reg, #3]
				cache_addd( ORR_REG_LSL_IMM(dest_reg, dest_reg, temp2, 24) );      // orr dest_reg, dest_reg, temp2, lsl #24
			}
		} else
#endif
		{
			cache_addd( LDR_IMM(dest_reg, data_reg, 0) );      // ldr dest_reg, [data_reg]
		}
	} else {
#if !(defined(C_UNALIGNED_MEMORY) || (C_TARGETCPU == ARMV7LE))
		if ((Bit32u)data & 1) {
			cache_addd( LDRB_IMM(dest_reg, data_reg, 0) );      // ldrb dest_reg, [data_reg]
			cache_addd( LDRB_IMM(temp2, data_reg, 1) );      // ldrb temp2, [data_reg, #1]
			cache_addd( ORR_REG_LSL_IMM(dest_reg, dest_reg, temp2, 8) );      // orr dest_reg, dest_reg, temp2, lsl #8
		} else
#endif
		{
			cache_addd( LDRH_IMM(dest_reg, data_reg, 0) );      // ldrh dest_reg, [data_reg]
		}
	}
}

// move a 32bit (dword==true) or 16bit (dword==false) value from memory into dest_reg
// 16bit moves may destroy the upper 16bit of the destination register
static void gen_mov_word_to_reg(HostReg dest_reg,void* data,bool dword) {
	if (!gen_mov_memval_to_reg(dest_reg, data, (dword)?4:2)) {
		gen_mov_dword_to_reg_imm(temp1, (Bit32u)data);
		gen_mov_word_to_reg_helper(dest_reg, data, dword, temp1);
	}
}

// move a 16bit constant value into dest_reg
// the upper 16bit of the destination register may be destroyed
static void INLINE gen_mov_word_to_reg_imm(HostReg dest_reg,Bit16u imm) {
	gen_mov_dword_to_reg_imm(dest_reg, (Bit32u)imm);
}

// helper function
static bool gen_mov_memval_from_reg_helper(HostReg src_reg, Bit32u data, Bitu size, HostReg addr_reg, Bit32u addr_data) {
	switch (size) {
		case 4:
#if !(defined(C_UNALIGNED_MEMORY) || (C_TARGETCPU == ARMV7LE))
			if ((data & 3) == 0)
#endif
			{
				if ((data >= addr_data) && (data < addr_data + 4096)) {
					cache_addd( STR_IMM(src_reg, addr_reg, data - addr_data) );      // str src_reg, [addr_reg, #(data - addr_data)]
					return true;
				} else if ((data < addr_data) && (data > addr_data - 4096)) {
					cache_addd( STR_IMM_M(src_reg, addr_reg, addr_data - data) );      // str src_reg, [addr_reg, #-(addr_data - data)]
					return true;
				}
			}
			break;
		case 2:
#if !(defined(C_UNALIGNED_MEMORY) || (C_TARGETCPU == ARMV7LE))
			if ((data & 1) == 0)
#endif
			{
				if ((data >= addr_data) && (data < addr_data + 256)) {
					cache_addd( STRH_IMM(src_reg, addr_reg, data - addr_data) );      // strh src_reg, [addr_reg, #(data - addr_data)]
					return true;
				} else if ((data < addr_data) && (data > addr_data - 256)) {
					cache_addd( STRH_IMM_M(src_reg, addr_reg, addr_data - data) );      // strh src_reg, [addr_reg, #-(addr_data - data)]
					return true;
				}
			}
			break;
		case 1:
			if ((data >= addr_data) && (data < addr_data + 4096)) {
				cache_addd( STRB_IMM(src_reg, addr_reg, data - addr_data) );      // strb src_reg, [addr_reg, #(data - addr_data)]
				return true;
			} else if ((data < addr_data) && (data > addr_data - 4096)) {
				cache_addd( STRB_IMM_M(src_reg, addr_reg, addr_data - data) );      // strb src_reg, [addr_reg, #-(addr_data - data)]
				return true;
			}
		default:
			break;
	}
	return false;
}

// helper function
static bool gen_mov_memval_from_reg(HostReg src_reg, void *dest, Bitu size) {
	if (gen_mov_memval_from_reg_helper(src_reg, (Bit32u)dest, size, FC_REGS_ADDR, (Bit32u)&cpu_regs)) return true;
	if (gen_mov_memval_from_reg_helper(src_reg, (Bit32u)dest, size, readdata_addr, (Bit32u)&core_dynrec.readdata)) return true;
	if (gen_mov_memval_from_reg_helper(src_reg, (Bit32u)dest, size, FC_SEGS_ADDR, (Bit32u)&Segs)) return true;
	return false;
}

// helper function for gen_mov_word_from_reg
static void gen_mov_word_from_reg_helper(HostReg src_reg,void* dest,bool dword, HostReg data_reg) {
	// alignment....
	if (dword) {
#if !(defined(C_UNALIGNED_MEMORY) || (C_TARGETCPU == ARMV7LE))
		if ((Bit32u)dest & 3) {
			if ( ((Bit32u)dest & 3) == 2 ) {
				cache_addd( STRH_IMM(src_reg, data_reg, 0) );      // strh src_reg, [data_reg]
				cache_addd( MOV_REG_LSR_IMM(temp2, src_reg, 16) );      // mov temp2, src_reg, lsr #16
				cache_addd( STRH_IMM(temp2, data_reg, 2) );      // strh temp2, [data_reg, #2]
			} else {
				cache_addd( STRB_IMM(src_reg, data_reg, 0) );      // strb src_reg, [data_reg]
				cache_addd( MOV_REG_LSR_IMM(temp2, src_reg, 8) );      // mov temp2, src_reg, lsr #8
				cache_addd( STRH_IMM(temp2, data_reg, 1) );      // strh temp2, [data_reg, #1]
				cache_addd( MOV_REG_LSR_IMM(temp2, temp2, 16) );      // mov temp2, temp2, lsr #16
				cache_addd( STRB_IMM(temp2, data_reg, 3) );      // strb temp2, [data_reg, #3]
			}
		} else
#endif
		{
			cache_addd( STR_IMM(src_reg, data_reg, 0) );      // str src_reg, [data_reg]
		}
	} else {
#if !(defined(C_UNALIGNED_MEMORY) || (C_TARGETCPU == ARMV7LE))
		if ((Bit32u)dest & 1) {
			cache_addd( STRB_IMM(src_reg, data_reg, 0) );      // strb src_reg, [data_reg]
			cache_addd( MOV_REG_LSR_IMM(temp2, src_reg, 8) );      // mov temp2, src_reg, lsr #8
			cache_addd( STRB_IMM(temp2, data_reg, 1) );      // strb temp2, [data_reg, #1]
		} else
#endif
		{
			cache_addd( STRH_IMM(src_reg, data_reg, 0) );      // strh src_reg, [data_reg]
		}
	}
}

// move 32bit (dword==true) or 16bit (dword==false) of a register into memory
static void gen_mov_word_from_reg(HostReg src_reg,void* dest,bool dword) {
	if (!gen_mov_memval_from_reg(src_reg, dest, (dword)?4:2)) {
		gen_mov_dword_to_reg_imm(temp1, (Bit32u)dest);
		gen_mov_word_from_reg_helper(src_reg, dest, dword, temp1);
	}
}

// move an 8bit value from memory into dest_reg
// the upper 24bit of the destination register can be destroyed
// this function does not use FC_OP1/FC_OP2 as dest_reg as these
// registers might not be directly byte-accessible on some architectures
static void gen_mov_byte_to_reg_low(HostReg dest_reg,void* data) {
	if (!gen_mov_memval_to_reg(dest_reg, data, 1)) {
		gen_mov_dword_to_reg_imm(temp1, (Bit32u)data);
		cache_addd( LDRB_IMM(dest_reg, temp1, 0) );      // ldrb dest_reg, [temp1]
	}
}

// move an 8bit value from memory into dest_reg
// the upper 24bit of the destination register can be destroyed
// this function can use FC_OP1/FC_OP2 as dest_reg which are
// not directly byte-accessible on some architectures
static void INLINE gen_mov_byte_to_reg_low_canuseword(HostReg dest_reg,void* data) {
	gen_mov_byte_to_reg_low(dest_reg, data);
}

// move an 8bit constant value into dest_reg
// the upper 24bit of the destination register can be destroyed
// this function does not use FC_OP1/FC_OP2 as dest_reg as these
// registers might not be directly byte-accessible on some architectures
static void gen_mov_byte_to_reg_low_imm(HostReg dest_reg,Bit8u imm) {
	cache_addd( MOV_IMM(dest_reg, imm, 0) );      // mov dest_reg, #(imm)
}

// move an 8bit constant value into dest_reg
// the upper 24bit of the destination register can be destroyed
// this function can use FC_OP1/FC_OP2 as dest_reg which are
// not directly byte-accessible on some architectures
static void INLINE gen_mov_byte_to_reg_low_imm_canuseword(HostReg dest_reg,Bit8u imm) {
	gen_mov_byte_to_reg_low_imm(dest_reg, imm);
}

// move the lowest 8bit of a register into memory
static void gen_mov_byte_from_reg_low(HostReg src_reg,void* dest) {
	if (!gen_mov_memval_from_reg(src_reg, dest, 1)) {
		gen_mov_dword_to_reg_imm(temp1, (Bit32u)dest);
		cache_addd( STRB_IMM(src_reg, temp1, 0) );      // strb src_reg, [temp1]
	}
}



// convert an 8bit word to a 32bit dword
// the register is zero-extended (sign==false) or sign-extended (sign==true)
static void gen_extend_byte(bool sign,HostReg reg) {
	if (sign) {
#if C_TARGETCPU == ARMV7LE
		cache_addd( SXTB(reg, reg, 0) );      // sxtb reg, reg
#else
		cache_addd( MOV_REG_LSL_IMM(reg, reg, 24) );      // mov reg, reg, lsl #24
		cache_addd( MOV_REG_ASR_IMM(reg, reg, 24) );      // mov reg, reg, asr #24
#endif
	} else {
#if C_TARGETCPU == ARMV7LE
		cache_addd( UXTB(reg, reg, 0) );      // uxtb reg, reg
#else
		cache_addd( AND_IMM(reg, reg, 0xff, 0) );      // and reg, reg, #0xff
#endif
	}
}

// convert a 16bit word to a 32bit dword
// the register is zero-extended (sign==false) or sign-extended (sign==true)
static void gen_extend_word(bool sign,HostReg reg) {
	if (sign) {
#if C_TARGETCPU == ARMV7LE
		cache_addd( SXTH(reg, reg, 0) );      // sxth reg, reg
#else
		cache_addd( MOV_REG_LSL_IMM(reg, reg, 16) );      // mov reg, reg, lsl #16
		cache_addd( MOV_REG_ASR_IMM(reg, reg, 16) );      // mov reg, reg, asr #16
#endif
	} else {
#if C_TARGETCPU == ARMV7LE
		cache_addd( UXTH(reg, reg, 0) );      // uxth reg, reg
#else
		cache_addd( MOV_REG_LSL_IMM(reg, reg, 16) );      // mov reg, reg, lsl #16
		cache_addd( MOV_REG_LSR_IMM(reg, reg, 16) );      // mov reg, reg, lsr #16
#endif
	}
}

// add a 32bit value from memory to a full register
static void gen_add(HostReg reg,void* op) {
	gen_mov_word_to_reg(temp3, op, 1);
	cache_addd( ADD_REG_LSL_IMM(reg, reg, temp3, 0) );      // add reg, reg, temp3
}

// add a 32bit constant value to a full register
static void gen_add_imm(HostReg reg,Bit32u imm) {
	Bit32u imm2, scale;

	if(!imm) return;

	imm2 = (Bit32u) (-((Bit32s)imm));

	if ( val_is_operand2(imm, &scale) ) {
		cache_addd( ADD_IMM(reg, reg, imm >> scale, ROTATE_SCALE(scale)) );      // add reg, reg, #imm
	} else if ( val_is_operand2(imm2, &scale) ) {
		cache_addd( SUB_IMM(reg, reg, imm2 >> scale, ROTATE_SCALE(scale)) );      // sub reg, reg, #(-imm)
#if C_TARGETCPU == ARMV7LE
	} else if (imm2 < 0x10000) {
		cache_addd( MOVW(temp2, imm2) );      // movw temp2, #(-imm)
		cache_addd( SUB_REG_LSL_IMM(reg, reg, temp2, 0) );      // sub reg, reg, temp2
#endif
	} else {
#if C_TARGETCPU != ARMV7LE
		if (get_min_imm_gen_len(imm) <= get_min_imm_gen_len(imm2)) {
#endif
			gen_mov_dword_to_reg_imm(temp2, imm);
			cache_addd( ADD_REG_LSL_IMM(reg, reg, temp2, 0) );      // add reg, reg, temp2
#if C_TARGETCPU != ARMV7LE
		} else {
			gen_mov_dword_to_reg_imm(temp2, imm2);
			cache_addd( SUB_REG_LSL_IMM(reg, reg, temp2, 0) );      // sub reg, reg, temp2
		}
#endif
	}
}

// and a 32bit constant value with a full register
static void gen_and_imm(HostReg reg,Bit32u imm) {
	Bit32u imm2, scale;

	imm2 = ~imm;
	if(!imm2) return;

	if (!imm) {
		cache_addd( MOV_IMM(reg, 0, 0) );      // mov reg, #0
	} else if ( val_is_operand2(imm, &scale) ) {
		cache_addd( AND_IMM(reg, reg, imm >> scale, ROTATE_SCALE(scale)) );      // and reg, reg, #imm
	} else if ( val_is_operand2(imm2, &scale) ) {
		cache_addd( BIC_IMM(reg, reg, imm2 >> scale, ROTATE_SCALE(scale)) );      // bic reg, reg, #(~imm)
#if C_TARGETCPU == ARMV7LE
	} else if (imm2 < 0x10000) {
		cache_addd( MOVW(temp2, imm2) );      // movw temp2, #(~imm)
		cache_addd( BIC_REG_LSL_IMM(reg, reg, temp2, 0) );      // bic reg, reg, temp2
#endif
	} else {
		gen_mov_dword_to_reg_imm(temp2, imm);
		cache_addd( AND_REG_LSL_IMM(reg, reg, temp2, 0) );      // and reg, reg, temp2
	}
}


// move a 32bit constant value into memory
static void gen_mov_direct_dword(void* dest,Bit32u imm) {
	gen_mov_dword_to_reg_imm(temp3, imm);
	gen_mov_word_from_reg(temp3, dest, 1);
}

// move an address into memory
static void INLINE gen_mov_direct_ptr(void* dest,DRC_PTR_SIZE_IM imm) {
	gen_mov_direct_dword(dest,(Bit32u)imm);
}

// add a 32bit (dword==true) or 16bit (dword==false) constant value to a memory value
static void gen_add_direct_word(void* dest,Bit32u imm,bool dword) {
	if (!dword) imm &= 0xffff;
	if(!imm) return;

	if (!gen_mov_memval_to_reg(temp3, dest, (dword)?4:2)) {
		gen_mov_dword_to_reg_imm(temp1, (Bit32u)dest);
		gen_mov_word_to_reg_helper(temp3, dest, dword, temp1);
	}
	gen_add_imm(temp3, imm);
	if (!gen_mov_memval_from_reg(temp3, dest, (dword)?4:2)) {
		gen_mov_word_from_reg_helper(temp3, dest, dword, temp1);
	}
}

// add an 8bit constant value to a dword memory value
static void gen_add_direct_byte(void* dest,Bit8s imm) {
	gen_add_direct_word(dest, (Bit32s)imm, 1);
}

// subtract a 32bit (dword==true) or 16bit (dword==false) constant value from a memory value
static void gen_sub_direct_word(void* dest,Bit32u imm,bool dword) {
	Bit32u imm2, scale;

	if (!dword) imm &= 0xffff;
	if(!imm) return;

	if (!gen_mov_memval_to_reg(temp3, dest, (dword)?4:2)) {
		gen_mov_dword_to_reg_imm(temp1, (Bit32u)dest);
		gen_mov_word_to_reg_helper(temp3, dest, dword, temp1);
	}

	imm2 = (Bit32u) (-((Bit32s)imm));

	if ( val_is_operand2(imm, &scale) ) {
		cache_addd( SUB_IMM(temp3, temp3, imm >> scale, ROTATE_SCALE(scale)) );      // sub temp3, temp3, #imm
	} else if ( val_is_operand2(imm2, &scale) ) {
		cache_addd( ADD_IMM(temp3, temp3, imm2 >> scale, ROTATE_SCALE(scale)) );      // add temp3, temp3, #(-imm)
#if C_TARGETCPU == ARMV7LE
	} else if (imm2 < 0x10000) {
		cache_addd( MOVW(temp2, imm2) );      // movw temp2, #(-imm)
		cache_addd( ADD_REG_LSL_IMM(temp3, temp3, temp2, 0) );      // add temp3, temp3, temp2
#endif
	} else {
#if C_TARGETCPU != ARMV7LE
		if (get_min_imm_gen_len(imm) <= get_min_imm_gen_len(imm2)) {
#endif
			gen_mov_dword_to_reg_imm(temp2, imm);
			cache_addd( SUB_REG_LSL_IMM(temp3, temp3, temp2, 0) );      // sub temp3, temp3, temp2
#if C_TARGETCPU != ARMV7LE
		} else {
			gen_mov_dword_to_reg_imm(temp2, imm2);
			cache_addd( ADD_REG_LSL_IMM(temp3, temp3, temp2, 0) );      // add temp3, temp3, temp2
		}
#endif
	}

	if (!gen_mov_memval_from_reg(temp3, dest, (dword)?4:2)) {
		gen_mov_word_from_reg_helper(temp3, dest, dword, temp1);
	}
}

// subtract an 8bit constant value from a dword memory value
static void gen_sub_direct_byte(void* dest,Bit8s imm) {
	gen_sub_direct_word(dest, (Bit32s)imm, 1);
}

// effective address calculation, destination is dest_reg
// scale_reg is scaled by scale (scale_reg*(2^scale)) and
// added to dest_reg, then the immediate value is added
static INLINE void gen_lea(HostReg dest_reg,HostReg scale_reg,Bitu scale,Bits imm) {
	cache_addd( ADD_REG_LSL_IMM(dest_reg, dest_reg, scale_reg, scale) );      // add dest_reg, dest_reg, scale_reg, lsl #(scale)
	gen_add_imm(dest_reg, imm);
}

// effective address calculation, destination is dest_reg
// dest_reg is scaled by scale (dest_reg*(2^scale)),
// then the immediate value is added
static INLINE void gen_lea(HostReg dest_reg,Bitu scale,Bits imm) {
	if (scale) {
		cache_addd( MOV_REG_LSL_IMM(dest_reg, dest_reg, scale) );      // mov dest_reg, dest_reg, lsl #(scale)
	}
	gen_add_imm(dest_reg, imm);
}

// generate a call to a parameterless function
static void INLINE gen_call_function_raw(void * func) {
#if C_TARGETCPU == ARMV7LE
	cache_addd( MOVW(temp1, ((Bit32u)func) & 0xffff) );      // movw temp1, #(func & 0xffff)
	cache_addd( MOVT(temp1, ((Bit32u)func) >> 16) );      // movt temp1, #(func >> 16)
	cache_addd( BLX_REG(temp1) );      // blx temp1
#else
	cache_addd( LDR_IMM(temp1, HOST_pc, 4) );      // ldr temp1, [pc, #4]
	cache_addd( ADD_IMM(HOST_lr, HOST_pc, 4, 0) );      // add lr, pc, #4
	cache_addd( BX(temp1) );      // bx temp1
	cache_addd((Bit32u)func);      // .int func
#endif
}

// generate a call to a function with paramcount parameters
// note: the parameters are loaded in the architecture specific way
// using the gen_load_param_ functions below
static Bit32u INLINE gen_call_function_setup(void * func,Bitu paramcount,bool fastcall=false) {
	Bit32u proc_addr = (Bit32u)cache.pos;
	gen_call_function_raw(func);
	return proc_addr;
}

#if (1)
// max of 4 parameters in a1-a4

// load an immediate value as param'th function parameter
static void INLINE gen_load_param_imm(Bitu imm,Bitu param) {
	gen_mov_dword_to_reg_imm(param, imm);
}

// load an address as param'th function parameter
static void INLINE gen_load_param_addr(Bitu addr,Bitu param) {
	gen_mov_dword_to_reg_imm(param, addr);
}

// load a host-register as param'th function parameter
static void INLINE gen_load_param_reg(Bitu reg,Bitu param) {
	gen_mov_regs(param, reg);
}

// load a value from memory as param'th function parameter
static void INLINE gen_load_param_mem(Bitu mem,Bitu param) {
	gen_mov_word_to_reg(param, (void *)mem, 1);
}
#else
	other arm abis
#endif

// jump to an address pointed at by ptr, offset is in imm
static void gen_jmp_ptr(void * ptr,Bits imm=0) {
	Bit32u scale;

	gen_mov_word_to_reg(temp3, ptr, 1);

#if !(defined(C_UNALIGNED_MEMORY) || (C_TARGETCPU == ARMV7LE))
// (*ptr) should be word aligned
	if ((imm & 0x03) == 0) {
#endif
		if ((imm >= 0) && (imm < 4096)) {
			cache_addd( LDR_IMM(temp1, temp3, imm) );      // ldr temp1, [temp3, #imm]
		} else {
			gen_mov_dword_to_reg_imm(temp2, imm);
			cache_addd( LDR_REG_LSL_IMM(temp1, temp3, temp2, 0) );      // ldr temp1, [temp3, temp2]
		}
#if !(defined(C_UNALIGNED_MEMORY) || (C_TARGETCPU == ARMV7LE))
	} else {
		gen_add_imm(temp3, imm);

		cache_addd( LDRB_IMM(temp1, temp3, 0) );      // ldrb temp1, [temp3]
		cache_addd( LDRB_IMM(temp2, temp3, 1) );      // ldrb temp2, [temp3, #1]
		cache_addd( ORR_REG_LSL_IMM(temp1, temp1, temp2, 8) );      // orr temp1, temp1, temp2, lsl #8
		cache_addd( LDRB_IMM(temp2, temp3, 2) );      // ldrb temp2, [temp3, #2]
		cache_addd( ORR_REG_LSL_IMM(temp1, temp1, temp2, 16) );      // orr temp1, temp1, temp2, lsl #16
		cache_addd( LDRB_IMM(temp2, temp3, 3) );      // ldrb temp2, [temp3, #3]
		cache_addd( ORR_REG_LSL_IMM(temp1, temp1, temp2, 24) );      // orr temp1, temp1, temp2, lsl #24
	}
#endif

	cache_addd( BX(temp1) );      // bx temp1
}

// short conditional jump (+-127 bytes) if register is zero
// the destination is set by gen_fill_branch() later
static Bit32u gen_create_branch_on_zero(HostReg reg,bool dword) {
	if (dword) {
		cache_addd( CMP_IMM(reg, 0, 0) );      // cmp reg, #0
	} else {
		cache_addd( MOVS_REG_LSL_IMM(temp1, reg, 16) );      // movs temp1, reg, lsl #16
	}
	cache_addd( BEQ_FWD(0) );      // beq j
	return ((Bit32u)cache.pos-4);
}

// short conditional jump (+-127 bytes) if register is nonzero
// the destination is set by gen_fill_branch() later
static Bit32u gen_create_branch_on_nonzero(HostReg reg,bool dword) {
	if (dword) {
		cache_addd( CMP_IMM(reg, 0, 0) );      // cmp reg, #0
	} else {
		cache_addd( MOVS_REG_LSL_IMM(temp1, reg, 16) );      // movs temp1, reg, lsl #16
	}
	cache_addd( BNE_FWD(0) );      // bne j
	return ((Bit32u)cache.pos-4);
}

// calculate relative offset and fill it into the location pointed to by data
static void INLINE gen_fill_branch(DRC_PTR_SIZE_IM data) {
#if C_DEBUG
	Bits len=(Bit32u)cache.pos-(data+8);
	if (len<0) len=-len;
	if (len>0x02000000) LOG_MSG("Big jump %d",len);
#endif
	*(Bit32u*)data=( (*(Bit32u*)data) & 0xff000000 ) | ( ( ((Bit32u)cache.pos - (data+8)) >> 2 ) & 0x00ffffff );
}

// conditional jump if register is nonzero
// for isdword==true the 32bit of the register are tested
// for isdword==false the lowest 8bit of the register are tested
static Bit32u gen_create_branch_long_nonzero(HostReg reg,bool isdword) {
	if (isdword) {
		cache_addd( CMP_IMM(reg, 0, 0) );      // cmp reg, #0
	} else {
		cache_addd( TST_IMM(reg, 0xff, 0) );      // tst reg, #0xff
	}
	cache_addd( BNE_FWD(0) );      // bne j
	return ((Bit32u)cache.pos-4);
}

// compare 32bit-register against zero and jump if value less/equal than zero
static Bit32u gen_create_branch_long_leqzero(HostReg reg) {
	cache_addd( CMP_IMM(reg, 0, 0) );      // cmp reg, #0
	cache_addd( BLE_FWD(0) );      // ble j
	return ((Bit32u)cache.pos-4);
}

// calculate long relative offset and fill it into the location pointed to by data
static void INLINE gen_fill_branch_long(Bit32u data) {
	*(Bit32u*)data=( (*(Bit32u*)data) & 0xff000000 ) | ( ( ((Bit32u)cache.pos - (data+8)) >> 2 ) & 0x00ffffff );
}

static void gen_run_code(void) {
#if C_TARGETCPU == ARMV7LE
	cache_addd(0xe92d4df0);			// stmfd sp!, {v1-v5,v7,v8,lr}

	cache_addd( MOVW(FC_SEGS_ADDR, ((Bit32u)&Segs) & 0xffff) );      // movw FC_SEGS_ADDR, #(&Segs & 0xffff)
	cache_addd( MOVT(FC_SEGS_ADDR, ((Bit32u)&Segs) >> 16) );      // movt FC_SEGS_ADDR, #(&Segs >> 16)

	cache_addd( MOVW(FC_REGS_ADDR, ((Bit32u)&cpu_regs) & 0xffff) );      // movw FC_REGS_ADDR, #(&cpu_regs & 0xffff)
	cache_addd( MOVT(FC_REGS_ADDR, ((Bit32u)&cpu_regs) >> 16) );      // movt FC_REGS_ADDR, #(&cpu_regs >> 16)

	cache_addd( MOVW(readdata_addr, ((Bitu)&core_dynrec.readdata) & 0xffff) );      // movw readdata_addr, #(&core_dynrec.readdata & 0xffff)
	cache_addd( MOVT(readdata_addr, ((Bitu)&core_dynrec.readdata) >> 16) );      // movt readdata_addr, #(&core_dynrec.readdata >> 16)

	cache_addd( BX(HOST_r0) );			// bx r0
#else
	Bit8u *pos1, *pos2, *pos3;

	cache_addd(0xe92d4df0);			// stmfd sp!, {v1-v5,v7,v8,lr}

	pos1 = cache.pos;
	cache_addd( 0 );
	pos2 = cache.pos;
	cache_addd( 0 );
	pos3 = cache.pos;
	cache_addd( 0 );

	cache_addd( BX(HOST_r0) );			// bx r0

	// align cache.pos to 32 bytes
	if ((((Bitu)cache.pos) & 0x1f) != 0) {
		cache.pos = cache.pos + (32 - (((Bitu)cache.pos) & 0x1f));
	}

	*(Bit32u*)pos1 = LDR_IMM(FC_SEGS_ADDR, HOST_pc, cache.pos - (pos1 + 8));      // ldr FC_SEGS_ADDR, [pc, #(&Segs)]
	cache_addd((Bit32u)&Segs);      // address of "Segs"

	*(Bit32u*)pos2 = LDR_IMM(FC_REGS_ADDR, HOST_pc, cache.pos - (pos2 + 8));      // ldr FC_REGS_ADDR, [pc, #(&cpu_regs)]
	cache_addd((Bit32u)&cpu_regs);  // address of "cpu_regs"

	*(Bit32u*)pos3 = LDR_IMM(readdata_addr, HOST_pc, cache.pos - (pos3 + 8));      // ldr readdata_addr, [pc, #(&core_dynrec.readdata)]
	cache_addd((Bit32u)&core_dynrec.readdata);  // address of "core_dynrec.readdata"

	// align cache.pos to 32 bytes
	if ((((Bitu)cache.pos) & 0x1f) != 0) {
		cache.pos = cache.pos + (32 - (((Bitu)cache.pos) & 0x1f));
	}
#endif
}

// return from a function
static void gen_return_function(void) {
	cache_addd(0xe8bd8df0);			// ldmfd sp!, {v1-v5,v7,v8,pc}
}

#ifdef DRC_FLAGS_INVALIDATION

// called when a call to a function can be replaced by a
// call to a simpler function
static void gen_fill_function_ptr(Bit8u * pos,void* fct_ptr,Bitu flags_type) {
#ifdef DRC_FLAGS_INVALIDATION_DCODE
	// try to avoid function calls but rather directly fill in code
	switch (flags_type) {
		case t_ADDb:
		case t_ADDw:
		case t_ADDd:
			*(Bit32u*)pos=NOP;					// nop
			*(Bit32u*)(pos+4)=NOP;				// nop
			*(Bit32u*)(pos+8)=ADD_REG_LSL_IMM(FC_RETOP, HOST_a1, HOST_a2, 0);	// add FC_RETOP, a1, a2
#if C_TARGETCPU != ARMV7LE
			*(Bit32u*)(pos+12)=NOP;				// nop
#endif
			break;
		case t_ORb:
		case t_ORw:
		case t_ORd:
			*(Bit32u*)pos=NOP;					// nop
			*(Bit32u*)(pos+4)=NOP;				// nop
			*(Bit32u*)(pos+8)=ORR_REG_LSL_IMM(FC_RETOP, HOST_a1, HOST_a2, 0);	// orr FC_RETOP, a1, a2
#if C_TARGETCPU != ARMV7LE
			*(Bit32u*)(pos+12)=NOP;				// nop
#endif
			break;
		case t_ANDb:
		case t_ANDw:
		case t_ANDd:
			*(Bit32u*)pos=NOP;					// nop
			*(Bit32u*)(pos+4)=NOP;				// nop
			*(Bit32u*)(pos+8)=AND_REG_LSL_IMM(FC_RETOP, HOST_a1, HOST_a2, 0);	// and FC_RETOP, a1, a2
#if C_TARGETCPU != ARMV7LE
			*(Bit32u*)(pos+12)=NOP;				// nop
#endif
			break;
		case t_SUBb:
		case t_SUBw:
		case t_SUBd:
			*(Bit32u*)pos=NOP;					// nop
			*(Bit32u*)(pos+4)=NOP;				// nop
			*(Bit32u*)(pos+8)=SUB_REG_LSL_IMM(FC_RETOP, HOST_a1, HOST_a2, 0);	// sub FC_RETOP, a1, a2
#if C_TARGETCPU != ARMV7LE
			*(Bit32u*)(pos+12)=NOP;				// nop
#endif
			break;
		case t_XORb:
		case t_XORw:
		case t_XORd:
			*(Bit32u*)pos=NOP;					// nop
			*(Bit32u*)(pos+4)=NOP;				// nop
			*(Bit32u*)(pos+8)=EOR_REG_LSL_IMM(FC_RETOP, HOST_a1, HOST_a2, 0);	// eor FC_RETOP, a1, a2
#if C_TARGETCPU != ARMV7LE
			*(Bit32u*)(pos+12)=NOP;				// nop
#endif
			break;
		case t_CMPb:
		case t_CMPw:
		case t_CMPd:
		case t_TESTb:
		case t_TESTw:
		case t_TESTd:
			*(Bit32u*)pos=NOP;					// nop
			*(Bit32u*)(pos+4)=NOP;				// nop
			*(Bit32u*)(pos+8)=NOP;				// nop
#if C_TARGETCPU != ARMV7LE
			*(Bit32u*)(pos+12)=NOP;				// nop
#endif
			break;
		case t_INCb:
		case t_INCw:
		case t_INCd:
			*(Bit32u*)pos=NOP;					// nop
			*(Bit32u*)(pos+4)=NOP;				// nop
			*(Bit32u*)(pos+8)=ADD_IMM(FC_RETOP, HOST_a1, 1, 0);	// add FC_RETOP, a1, #1
#if C_TARGETCPU != ARMV7LE
			*(Bit32u*)(pos+12)=NOP;				// nop
#endif
			break;
		case t_DECb:
		case t_DECw:
		case t_DECd:
			*(Bit32u*)pos=NOP;					// nop
			*(Bit32u*)(pos+4)=NOP;				// nop
			*(Bit32u*)(pos+8)=SUB_IMM(FC_RETOP, HOST_a1, 1, 0);	// sub FC_RETOP, a1, #1
#if C_TARGETCPU != ARMV7LE
			*(Bit32u*)(pos+12)=NOP;				// nop
#endif
			break;
		case t_SHLb:
		case t_SHLw:
		case t_SHLd:
			*(Bit32u*)pos=NOP;					// nop
			*(Bit32u*)(pos+4)=NOP;				// nop
			*(Bit32u*)(pos+8)=MOV_REG_LSL_REG(FC_RETOP, HOST_a1, HOST_a2);	// mov FC_RETOP, a1, lsl a2
#if C_TARGETCPU != ARMV7LE
			*(Bit32u*)(pos+12)=NOP;				// nop
#endif
			break;
		case t_SHRb:
			*(Bit32u*)pos=NOP;					// nop
#if C_TARGETCPU == ARMV7LE
			*(Bit32u*)(pos+4)=BFC(HOST_a1, 8, 24);	// bfc a1, 8, 24
			*(Bit32u*)(pos+8)=MOV_REG_LSR_REG(FC_RETOP, HOST_a1, HOST_a2);	// mov FC_RETOP, a1, lsr a2
#else
			*(Bit32u*)(pos+4)=NOP;				// nop
			*(Bit32u*)(pos+8)=AND_IMM(FC_RETOP, HOST_a1, 0xff, 0);				// and FC_RETOP, a1, #0xff
			*(Bit32u*)(pos+12)=MOV_REG_LSR_REG(FC_RETOP, FC_RETOP, HOST_a2);	// mov FC_RETOP, FC_RETOP, lsr a2
#endif
			break;
		case t_SHRw:
			*(Bit32u*)pos=NOP;					// nop
#if C_TARGETCPU == ARMV7LE
			*(Bit32u*)(pos+4)=BFC(HOST_a1, 16, 16);	// bfc a1, 16, 16
			*(Bit32u*)(pos+8)=MOV_REG_LSR_REG(FC_RETOP, HOST_a1, HOST_a2);	// mov FC_RETOP, a1, lsr a2
#else
			*(Bit32u*)(pos+4)=MOV_REG_LSL_IMM(FC_RETOP, HOST_a1, 16);			// mov FC_RETOP, a1, lsl #16
			*(Bit32u*)(pos+8)=MOV_REG_LSR_IMM(FC_RETOP, FC_RETOP, 16);			// mov FC_RETOP, FC_RETOP, lsr #16
			*(Bit32u*)(pos+12)=MOV_REG_LSR_REG(FC_RETOP, FC_RETOP, HOST_a2);	// mov FC_RETOP, FC_RETOP, lsr a2
#endif
			break;
		case t_SHRd:
			*(Bit32u*)pos=NOP;					// nop
			*(Bit32u*)(pos+4)=NOP;				// nop
			*(Bit32u*)(pos+8)=MOV_REG_LSR_REG(FC_RETOP, HOST_a1, HOST_a2);	// mov FC_RETOP, a1, lsr a2
#if C_TARGETCPU != ARMV7LE
			*(Bit32u*)(pos+12)=NOP;				// nop
#endif
			break;
		case t_SARb:
			*(Bit32u*)pos=NOP;					// nop
#if C_TARGETCPU == ARMV7LE
			*(Bit32u*)(pos+4)=SXTB(FC_RETOP, HOST_a1, 0);					// sxtb FC_RETOP, a1
			*(Bit32u*)(pos+8)=MOV_REG_ASR_REG(FC_RETOP, FC_RETOP, HOST_a2);	// mov FC_RETOP, FC_RETOP, asr a2
#else
			*(Bit32u*)(pos+4)=MOV_REG_LSL_IMM(FC_RETOP, HOST_a1, 24);			// mov FC_RETOP, a1, lsl #24
			*(Bit32u*)(pos+8)=MOV_REG_ASR_IMM(FC_RETOP, FC_RETOP, 24);			// mov FC_RETOP, FC_RETOP, asr #24
			*(Bit32u*)(pos+12)=MOV_REG_ASR_REG(FC_RETOP, FC_RETOP, HOST_a2);	// mov FC_RETOP, FC_RETOP, asr a2
#endif
			break;
		case t_SARw:
			*(Bit32u*)pos=NOP;					// nop
#if C_TARGETCPU == ARMV7LE
			*(Bit32u*)(pos+4)=SXTH(FC_RETOP, HOST_a1, 0);					// sxth FC_RETOP, a1
			*(Bit32u*)(pos+8)=MOV_REG_ASR_REG(FC_RETOP, FC_RETOP, HOST_a2);	// mov FC_RETOP, FC_RETOP, asr a2
#else
			*(Bit32u*)(pos+4)=MOV_REG_LSL_IMM(FC_RETOP, HOST_a1, 16);			// mov FC_RETOP, a1, lsl #16
			*(Bit32u*)(pos+8)=MOV_REG_ASR_IMM(FC_RETOP, FC_RETOP, 16);			// mov FC_RETOP, FC_RETOP, asr #16
			*(Bit32u*)(pos+12)=MOV_REG_ASR_REG(FC_RETOP, FC_RETOP, HOST_a2);	// mov FC_RETOP, FC_RETOP, asr a2
#endif
			break;
		case t_SARd:
			*(Bit32u*)pos=NOP;					// nop
			*(Bit32u*)(pos+4)=NOP;				// nop
			*(Bit32u*)(pos+8)=MOV_REG_ASR_REG(FC_RETOP, HOST_a1, HOST_a2);	// mov FC_RETOP, a1, asr a2
#if C_TARGETCPU != ARMV7LE
			*(Bit32u*)(pos+12)=NOP;				// nop
#endif
			break;
		case t_RORb:
#if C_TARGETCPU == ARMV7LE
			*(Bit32u*)pos=BFI(HOST_a1, HOST_a1, 8, 8);						// bfi a1, a1, 8, 8
			*(Bit32u*)(pos+4)=BFI(HOST_a1, HOST_a1, 16, 16);				// bfi a1, a1, 16, 16
			*(Bit32u*)(pos+8)=MOV_REG_ROR_REG(FC_RETOP, HOST_a1, HOST_a2);	// mov FC_RETOP, a1, ror a2
#else
			*(Bit32u*)pos=MOV_REG_LSL_IMM(FC_RETOP, HOST_a1, 24);					// mov FC_RETOP, a1, lsl #24
			*(Bit32u*)(pos+4)=ORR_REG_LSR_IMM(FC_RETOP, FC_RETOP, FC_RETOP, 8);		// orr FC_RETOP, FC_RETOP, FC_RETOP, lsr #8
			*(Bit32u*)(pos+8)=ORR_REG_LSR_IMM(FC_RETOP, FC_RETOP, FC_RETOP, 16);	// orr FC_RETOP, FC_RETOP, FC_RETOP, lsr #16
			*(Bit32u*)(pos+12)=MOV_REG_ROR_REG(FC_RETOP, FC_RETOP, HOST_a2);		// mov FC_RETOP, FC_RETOP, ror a2
#endif
			break;
		case t_RORw:
			*(Bit32u*)pos=NOP;					// nop
#if C_TARGETCPU == ARMV7LE
			*(Bit32u*)(pos+4)=BFI(HOST_a1, HOST_a1, 16, 16);				// bfi a1, a1, 16, 16
			*(Bit32u*)(pos+8)=MOV_REG_ROR_REG(FC_RETOP, HOST_a1, HOST_a2);	// mov FC_RETOP, a1, ror a2
#else
			*(Bit32u*)(pos+4)=MOV_REG_LSL_IMM(FC_RETOP, HOST_a1, 16);				// mov FC_RETOP, a1, lsl #16
			*(Bit32u*)(pos+8)=ORR_REG_LSR_IMM(FC_RETOP, FC_RETOP, FC_RETOP, 16);	// orr FC_RETOP, FC_RETOP, FC_RETOP, lsr #16
			*(Bit32u*)(pos+12)=MOV_REG_ROR_REG(FC_RETOP, FC_RETOP, HOST_a2);		// mov FC_RETOP, FC_RETOP, ror a2
#endif
			break;
		case t_RORd:
			*(Bit32u*)pos=NOP;					// nop
			*(Bit32u*)(pos+4)=NOP;				// nop
			*(Bit32u*)(pos+8)=MOV_REG_ROR_REG(FC_RETOP, HOST_a1, HOST_a2);	// mov FC_RETOP, a1, ror a2
#if C_TARGETCPU != ARMV7LE
			*(Bit32u*)(pos+12)=NOP;				// nop
#endif
			break;
		case t_ROLw:
#if C_TARGETCPU == ARMV7LE
			*(Bit32u*)pos=BFI(HOST_a1, HOST_a1, 16, 16);					// bfi a1, a1, 16, 16
			*(Bit32u*)(pos+4)=RSB_IMM(HOST_a2, HOST_a2, 32, 0);				// rsb a2, a2, #32
			*(Bit32u*)(pos+8)=MOV_REG_ROR_REG(FC_RETOP, HOST_a1, HOST_a2);	// mov FC_RETOP, a1, ror a2
#else
			*(Bit32u*)pos=MOV_REG_LSL_IMM(FC_RETOP, HOST_a1, 16);					// mov FC_RETOP, a1, lsl #16
			*(Bit32u*)(pos+4)=RSB_IMM(HOST_a2, HOST_a2, 32, 0);						// rsb a2, a2, #32
			*(Bit32u*)(pos+8)=ORR_REG_LSR_IMM(FC_RETOP, FC_RETOP, FC_RETOP, 16);	// orr FC_RETOP, FC_RETOP, FC_RETOP, lsr #16
			*(Bit32u*)(pos+12)=MOV_REG_ROR_REG(FC_RETOP, FC_RETOP, HOST_a2);		// mov FC_RETOP, FC_RETOP, ror a2
#endif
			break;
		case t_ROLd:
			*(Bit32u*)pos=NOP;					// nop
#if C_TARGETCPU == ARMV7LE
			*(Bit32u*)(pos+4)=RSB_IMM(HOST_a2, HOST_a2, 32, 0);				// rsb a2, a2, #32
			*(Bit32u*)(pos+8)=MOV_REG_ROR_REG(FC_RETOP, HOST_a1, HOST_a2);	// mov FC_RETOP, a1, ror a2
#else
			*(Bit32u*)(pos+4)=NOP;				// nop
			*(Bit32u*)(pos+8)=RSB_IMM(HOST_a2, HOST_a2, 32, 0);				// rsb a2, a2, #32
			*(Bit32u*)(pos+12)=MOV_REG_ROR_REG(FC_RETOP, HOST_a1, HOST_a2);	// mov FC_RETOP, a1, ror a2
#endif
			break;
		case t_NEGb:
		case t_NEGw:
		case t_NEGd:
			*(Bit32u*)pos=NOP;					// nop
			*(Bit32u*)(pos+4)=NOP;				// nop
			*(Bit32u*)(pos+8)=RSB_IMM(FC_RETOP, HOST_a1, 0, 0);	// rsb FC_RETOP, a1, #0
#if C_TARGETCPU != ARMV7LE
			*(Bit32u*)(pos+12)=NOP;				// nop
#endif
			break;
		default:
#if C_TARGETCPU == ARMV7LE
			*(Bit32u*)pos=MOVW(temp1, ((Bit32u)fct_ptr) & 0xffff);      // movw temp1, #(fct_ptr & 0xffff)
			*(Bit32u*)(pos+4)=MOVT(temp1, ((Bit32u)fct_ptr) >> 16);      // movt temp1, #(fct_ptr >> 16)
#else
			*(Bit32u*)(pos+12)=(Bit32u)fct_ptr;		// simple_func
#endif
			break;

	}
#else
#if C_TARGETCPU == ARMV7LE
	*(Bit32u*)pos=MOVW(temp1, ((Bit32u)fct_ptr) & 0xffff);      // movw temp1, #(fct_ptr & 0xffff)
	*(Bit32u*)(pos+4)=MOVT(temp1, ((Bit32u)fct_ptr) >> 16);      // movt temp1, #(fct_ptr >> 16)
#else
	*(Bit32u*)(pos+12)=(Bit32u)fct_ptr;		// simple_func
#endif
#endif
}
#endif

static void cache_block_before_close(void) { }

#ifdef DRC_USE_SEGS_ADDR

// mov 16bit value from Segs[index] into dest_reg using FC_SEGS_ADDR (index modulo 2 must be zero)
// 16bit moves may destroy the upper 16bit of the destination register
static void gen_mov_seg16_to_reg(HostReg dest_reg,Bitu index) {
	cache_addd( LDRH_IMM(dest_reg, FC_SEGS_ADDR, index) );      // ldrh dest_reg, [FC_SEGS_ADDR, #index]
}

// mov 32bit value from Segs[index] into dest_reg using FC_SEGS_ADDR (index modulo 4 must be zero)
static void gen_mov_seg32_to_reg(HostReg dest_reg,Bitu index) {
	cache_addd( LDR_IMM(dest_reg, FC_SEGS_ADDR, index) );      // ldr dest_reg, [FC_SEGS_ADDR, #index]
}

// add a 32bit value from Segs[index] to a full register using FC_SEGS_ADDR (index modulo 4 must be zero)
static void gen_add_seg32_to_reg(HostReg reg,Bitu index) {
	cache_addd( LDR_IMM(temp1, FC_SEGS_ADDR, index) );      // ldr temp1, [FC_SEGS_ADDR, #index]
	cache_addd( ADD_REG_LSL_IMM(reg, reg, temp1, 0) );      // add reg, reg, temp1
}

#endif

#ifdef DRC_USE_REGS_ADDR

// mov 16bit value from cpu_regs[index] into dest_reg using FC_REGS_ADDR (index modulo 2 must be zero)
// 16bit moves may destroy the upper 16bit of the destination register
static void gen_mov_regval16_to_reg(HostReg dest_reg,Bitu index) {
	cache_addd( LDRH_IMM(dest_reg, FC_REGS_ADDR, index) );      // ldrh dest_reg, [FC_REGS_ADDR, #index]
}

// mov 32bit value from cpu_regs[index] into dest_reg using FC_REGS_ADDR (index modulo 4 must be zero)
static void gen_mov_regval32_to_reg(HostReg dest_reg,Bitu index) {
	cache_addd( LDR_IMM(dest_reg, FC_REGS_ADDR, index) );      // ldr dest_reg, [FC_REGS_ADDR, #index]
}

// move a 32bit (dword==true) or 16bit (dword==false) value from cpu_regs[index] into dest_reg using FC_REGS_ADDR (if dword==true index modulo 4 must be zero) (if dword==false index modulo 2 must be zero)
// 16bit moves may destroy the upper 16bit of the destination register
static void gen_mov_regword_to_reg(HostReg dest_reg,Bitu index,bool dword) {
	if (dword) {
		cache_addd( LDR_IMM(dest_reg, FC_REGS_ADDR, index) );      // ldr dest_reg, [FC_REGS_ADDR, #index]
	} else {
		cache_addd( LDRH_IMM(dest_reg, FC_REGS_ADDR, index) );      // ldrh dest_reg, [FC_REGS_ADDR, #index]
	}
}

// move an 8bit value from cpu_regs[index]  into dest_reg using FC_REGS_ADDR
// the upper 24bit of the destination register can be destroyed
// this function does not use FC_OP1/FC_OP2 as dest_reg as these
// registers might not be directly byte-accessible on some architectures
static void gen_mov_regbyte_to_reg_low(HostReg dest_reg,Bitu index) {
	cache_addd( LDRB_IMM(dest_reg, FC_REGS_ADDR, index) );      // ldrb dest_reg, [FC_REGS_ADDR, #index]
}

// move an 8bit value from cpu_regs[index]  into dest_reg using FC_REGS_ADDR
// the upper 24bit of the destination register can be destroyed
// this function can use FC_OP1/FC_OP2 as dest_reg which are
// not directly byte-accessible on some architectures
static void gen_mov_regbyte_to_reg_low_canuseword(HostReg dest_reg,Bitu index) {
	cache_addd( LDRB_IMM(dest_reg, FC_REGS_ADDR, index) );      // ldrb dest_reg, [FC_REGS_ADDR, #index]
}


// add a 32bit value from cpu_regs[index] to a full register using FC_REGS_ADDR (index modulo 4 must be zero)
static void gen_add_regval32_to_reg(HostReg reg,Bitu index) {
	cache_addd( LDR_IMM(temp2, FC_REGS_ADDR, index) );      // ldr temp2, [FC_REGS_ADDR, #index]
	cache_addd( ADD_REG_LSL_IMM(reg, reg, temp2, 0) );      // add reg, reg, temp2
}


// move 16bit of register into cpu_regs[index] using FC_REGS_ADDR (index modulo 2 must be zero)
static void gen_mov_regval16_from_reg(HostReg src_reg,Bitu index) {
	cache_addd( STRH_IMM(src_reg, FC_REGS_ADDR, index) );      // strh src_reg, [FC_REGS_ADDR, #index]
}

// move 32bit of register into cpu_regs[index] using FC_REGS_ADDR (index modulo 4 must be zero)
static void gen_mov_regval32_from_reg(HostReg src_reg,Bitu index) {
	cache_addd( STR_IMM(src_reg, FC_REGS_ADDR, index) );      // str src_reg, [FC_REGS_ADDR, #index]
}

// move 32bit (dword==true) or 16bit (dword==false) of a register into cpu_regs[index] using FC_REGS_ADDR (if dword==true index modulo 4 must be zero) (if dword==false index modulo 2 must be zero)
static void gen_mov_regword_from_reg(HostReg src_reg,Bitu index,bool dword) {
	if (dword) {
		cache_addd( STR_IMM(src_reg, FC_REGS_ADDR, index) );      // str src_reg, [FC_REGS_ADDR, #index]
	} else {
		cache_addd( STRH_IMM(src_reg, FC_REGS_ADDR, index) );      // strh src_reg, [FC_REGS_ADDR, #index]
	}
}

// move the lowest 8bit of a register into cpu_regs[index] using FC_REGS_ADDR
static void gen_mov_regbyte_from_reg_low(HostReg src_reg,Bitu index) {
	cache_addd( STRB_IMM(src_reg, FC_REGS_ADDR, index) );      // strb src_reg, [FC_REGS_ADDR, #index]
}

#endif
