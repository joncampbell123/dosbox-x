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



/* ARMv8 (little endian, 64-bit) backend by M-HT */


// some configuring defines that specify the capabilities of this architecture
// or aspects of the recompiling

// protect FC_ADDR over function calls if necessaray
// #define DRC_PROTECT_ADDR_REG

// try to use non-flags generating functions if possible
#define DRC_FLAGS_INVALIDATION
// try to replace _simple functions by code
#define DRC_FLAGS_INVALIDATION_DCODE

// type with the same size as a pointer
#define DRC_PTR_SIZE_IM Bit64u

// calling convention modifier
#define DRC_CALL_CONV	/* nothing */
#define DRC_FC			/* nothing */

// use FC_REGS_ADDR to hold the address of "cpu_regs" and to access it using FC_REGS_ADDR
#define DRC_USE_REGS_ADDR
// use FC_SEGS_ADDR to hold the address of "Segs" and to access it using FC_SEGS_ADDR
#define DRC_USE_SEGS_ADDR

// register mapping
typedef Bit8u HostReg;

// registers
#define HOST_r0		 0
#define HOST_r1		 1
#define HOST_r2		 2
#define HOST_r3		 3
#define HOST_r4		 4
#define HOST_r5		 5
#define HOST_r6		 6
#define HOST_r7		 7
#define HOST_r8		 8
#define HOST_r9		 9
#define HOST_r10	10
#define HOST_r11	11
#define HOST_r12	12
#define HOST_r13	13
#define HOST_r14	14
#define HOST_r15	15
#define HOST_r16	16
#define HOST_r17	17
#define HOST_r18	18
#define HOST_r19	19
#define HOST_r20	20
#define HOST_r21	21
#define HOST_r22	22
#define HOST_r23	23
#define HOST_r24	24
#define HOST_r25	25
#define HOST_r26	26
#define HOST_r27	27
#define HOST_r28	28
#define HOST_r29	29
#define HOST_r30	30
// special registers
#define HOST_sp		31
#define HOST_zr		31

// register aliases
// 32-bit registers
#define HOST_w0		HOST_r0
#define HOST_w1		HOST_r1
#define HOST_w2		HOST_r2
#define HOST_w3		HOST_r3
#define HOST_w4		HOST_r4
#define HOST_w5		HOST_r5
#define HOST_w6		HOST_r6
#define HOST_w7		HOST_r7
#define HOST_w8		HOST_r8
#define HOST_w9		HOST_r9
#define HOST_w10	HOST_r10
#define HOST_w11	HOST_r11
#define HOST_w12	HOST_r12
#define HOST_w13	HOST_r13
#define HOST_w14	HOST_r14
#define HOST_w15	HOST_r15
#define HOST_w16	HOST_r16
#define HOST_w17	HOST_r17
#define HOST_w18	HOST_r18
#define HOST_w19	HOST_r19
#define HOST_w20	HOST_r20
#define HOST_w21	HOST_r21
#define HOST_w22	HOST_r22
#define HOST_w23	HOST_r23
#define HOST_w24	HOST_r24
#define HOST_w25	HOST_r25
#define HOST_w26	HOST_r26
#define HOST_w27	HOST_r27
#define HOST_w28	HOST_r28
#define HOST_w29	HOST_r29
#define HOST_w30	HOST_r30
#define HOST_wsp	HOST_sp
#define HOST_wzr	HOST_zr
// 64-bit registers
#define HOST_x0		HOST_r0
#define HOST_x1		HOST_r1
#define HOST_x2		HOST_r2
#define HOST_x3		HOST_r3
#define HOST_x4		HOST_r4
#define HOST_x5		HOST_r5
#define HOST_x6		HOST_r6
#define HOST_x7		HOST_r7
#define HOST_x8		HOST_r8
#define HOST_x9		HOST_r9
#define HOST_x10	HOST_r10
#define HOST_x11	HOST_r11
#define HOST_x12	HOST_r12
#define HOST_x13	HOST_r13
#define HOST_x14	HOST_r14
#define HOST_x15	HOST_r15
#define HOST_x16	HOST_r16
#define HOST_x17	HOST_r17
#define HOST_x18	HOST_r18
#define HOST_x19	HOST_r19
#define HOST_x20	HOST_r20
#define HOST_x21	HOST_r21
#define HOST_x22	HOST_r22
#define HOST_x23	HOST_r23
#define HOST_x24	HOST_r24
#define HOST_x25	HOST_r25
#define HOST_x26	HOST_r26
#define HOST_x27	HOST_r27
#define HOST_x28	HOST_r28
#define HOST_x29	HOST_r29
#define HOST_x30	HOST_r30
#define HOST_xzr	HOST_zr
#define HOST_ip0	HOST_r16
#define HOST_ip1	HOST_r17
#define HOST_fp		HOST_r29
#define HOST_lr		HOST_r30


// temporary registers
#define temp1 HOST_r10
#define temp2 HOST_r11
#define temp3 HOST_r12

// register that holds function return values
#define FC_RETOP HOST_r0

// register used for address calculations,
#define FC_ADDR HOST_r19			// has to be saved across calls, see DRC_PROTECT_ADDR_REG

// register that holds the first parameter
#define FC_OP1 HOST_r0

// register that holds the second parameter
#define FC_OP2 HOST_r1

// special register that holds the third parameter for _R3 calls (byte accessible)
#define FC_OP3 HOST_r2

// register that holds byte-accessible temporary values
#define FC_TMP_BA1 HOST_r0

// register that holds byte-accessible temporary values
#define FC_TMP_BA2 HOST_r1

// temporary register for LEA
#define TEMP_REG_DRC HOST_r9

// used to hold the address of "cpu_regs" - preferably filled in function gen_run_code
#define FC_REGS_ADDR HOST_r20

// used to hold the address of "Segs" - preferably filled in function gen_run_code
#define FC_SEGS_ADDR HOST_r21

// used to hold the address of "core_dynrec.readdata" - filled in function gen_run_code
#define readdata_addr HOST_r22


// instruction encodings

// move
// mov dst, src, lsl #imm
#define MOV_REG_LSL_IMM(dst, src, imm) ORR_REG_LSL_IMM(dst, HOST_wzr, src, imm)
// movz dst, #(imm lsl simm)		@	0 <= imm <= 65535	&	simm = 0/16
#define MOVZ(dst, imm, simm) (0x52800000 + (dst) + ((imm) << 5) + ((simm)?0x00200000:0) )
// movn dst, #(imm lsl simm)		@	0 <= imm <= 65535	&	simm = 0/16
#define MOVN(dst, imm, simm) (0x12800000 + (dst) + ((imm) << 5) + ((simm)?0x00200000:0) )
// movk dst, #(imm lsl simm)		@	0 <= imm <= 65535	&	simm = 0/16
#define MOVK(dst, imm, simm) (0x72800000 + (dst) + ((imm) << 5) + ((simm)?0x00200000:0) )
// movz dst, #(imm lsl simm)		@	0 <= imm <= 65535	&	simm = 0/16/32/48
#define MOVZ64(dst, imm, simm) (0xd2800000 + (dst) + ((imm) << 5) + (((simm) >> 4) << 21) )
// movk dst, #(imm lsl simm)		@	0 <= imm <= 65535	&	simm = 0/16/32/48
#define MOVK64(dst, imm, simm) (0xf2800000 + (dst) + ((imm) << 5) + (((simm) >> 4) << 21) )
// lslv dst, src, rreg
#define LSLV(dst, src, rreg) (0x1ac02000 + (dst) + ((src) << 5) + ((rreg) << 16) )
// lsrv dst, src, rreg
#define LSRV(dst, src, rreg) (0x1ac02400 + (dst) + ((src) << 5) + ((rreg) << 16) )
// asrv dst, src, rreg
#define ASRV(dst, src, rreg) (0x1ac02800 + (dst) + ((src) << 5) + ((rreg) << 16) )
// rorv dst, src, rreg
#define RORV(dst, src, rreg) (0x1ac02c00 + (dst) + ((src) << 5) + ((rreg) << 16) )
// lslv dst, src, rreg
#define LSLV64(dst, src, rreg) (0x9ac02000 + (dst) + ((src) << 5) + ((rreg) << 16) )
// lsrv dst, src, rreg
#define LSRV64(dst, src, rreg) (0x9ac02400 + (dst) + ((src) << 5) + ((rreg) << 16) )
// lsr dst, src, #imm
#define LSR64_IMM(dst, src, imm) UBFM64(dst, src, imm, 63)

// arithmetic
// add dst, src, #(imm lsl simm)		@	0 <= imm <= 4095	&	simm = 0/12
#define ADD_IMM(dst, src, imm, simm) (0x11000000 + (dst) + ((src) << 5) + ((imm) << 10) + ((simm)?0x00400000:0) )
// add dst, src1, src2, lsl #imm
#define ADD_REG_LSL_IMM(dst, src1, src2, imm) (0x0b000000 + (dst) + ((src1) << 5) + ((src2) << 16) + ((imm) << 10) )
// sub dst, src, #(imm lsl simm)		@	0 <= imm <= 4095	&	simm = 0/12
#define SUB_IMM(dst, src, imm, simm) (0x51000000 + (dst) + ((src) << 5) + ((imm) << 10) + ((simm)?0x00400000:0) )
// sub dst, src1, src2, lsl #imm
#define SUB_REG_LSL_IMM(dst, src1, src2, imm) (0x4b000000 + (dst) + ((src1) << 5) + ((src2) << 16) + ((imm) << 10) )
// cmp src, #(imm lsl simm)		@	0 <= imm <= 4095	&	simm = 0/12
#define CMP_IMM(src, imm, simm) (0x7100001f + ((src) << 5) + ((imm) << 10) + ((simm)?0x00400000:0) )
// nop
#define NOP (0xd503201f)

// logical
// and dst, src1, src2, lsl #imm		@	0 <= imm <= 31
#define AND_REG_LSL_IMM(dst, src1, src2, imm) (0x0a000000 + (dst) + ((src1) << 5) + ((src2) << 16) + ((imm) << 10) )
// orr dst, src1, src2, lsl #imm		@	0 <= imm <= 31
#define ORR_REG_LSL_IMM(dst, src1, src2, imm) (0x2a000000 + (dst) + ((src1) << 5) + ((src2) << 16) + ((imm) << 10) )
// eor dst, src1, src2, lsl #imm		@	0 <= imm <= 31
#define EOR_REG_LSL_IMM(dst, src1, src2, imm) (0x4a000000 + (dst) + ((src1) << 5) + ((src2) << 16) + ((imm) << 10) )
// bic dst, src1, src2, lsl #imm		@	0 <= imm <= 31
#define BIC_REG_LSL_IMM(dst, src1, src2, imm) (0x0a200000 + (dst) + ((src1) << 5) + ((src2) << 16) + ((imm) << 10) )
// and dst, src1, src2, lsl #imm		@	0 <= imm <= 63
#define AND64_REG_LSL_IMM(dst, src1, src2, imm) (0x8a000000 + (dst) + ((src1) << 5) + ((src2) << 16) + ((imm) << 10) )

// load
// ldr reg, [pc, #imm]		@	-1M <= imm < 1M	&	imm mod 4 = 0
#define LDR64_PC(reg, imm) (0x58000000 + (reg) + (((imm) << 3) & 0x00ffffe0) )
// ldp reg1, reg2 [addr, #imm]		@	-512 <= imm < 512	&	imm mod 8 = 0
#define LDP64_IMM(reg1, reg2, addr, imm) (0xa9400000 + (reg1) + ((reg2) << 10) + ((addr) << 5) + ((imm) << 12) )
// ldr reg, [addr, #imm]		@	0 <= imm < 32768	&	imm mod 8 = 0
#define LDR64_IMM(reg, addr, imm) (0xf9400000 + (reg) + ((addr) << 5) + ((imm) << 7) )
// ldr reg, [addr, #imm]		@	0 <= imm < 16384	&	imm mod 4 = 0
#define LDR_IMM(reg, addr, imm) (0xb9400000 + (reg) + ((addr) << 5) + ((imm) << 8) )
// ldrh reg, [addr, #imm]		@	0 <= imm < 8192	&	imm mod 2 = 0
#define LDRH_IMM(reg, addr, imm) (0x79400000 + (reg) + ((addr) << 5) + ((imm) << 9) )
// ldrb reg, [addr, #imm]		@	0 <= imm < 4096
#define LDRB_IMM(reg, addr, imm) (0x39400000 + (reg) + ((addr) << 5) + ((imm) << 10) )
// ldr reg, [addr1, addr2, lsl #imm]		@	imm = 0/2
#define LDR64_REG_LSL_IMM(reg, addr1, addr2, imm) (0xf8606800 + (reg) + ((addr1) << 5) + ((addr2) << 16) + ((imm)?0x00001000:0) )
// ldur reg, [addr, #imm]		@	-256 <= imm < 256
#define LDUR64_IMM(reg, addr, imm) (0xf8400000 + (reg) + ((addr) << 5) + (((imm) << 12) & 0x001ff000) )
// ldur reg, [addr, #imm]		@	-256 <= imm < 256
#define LDUR_IMM(reg, addr, imm) (0xb8400000 + (reg) + ((addr) << 5) + (((imm) << 12) & 0x001ff000) )
// ldurh reg, [addr, #imm]		@	-256 <= imm < 256
#define LDURH_IMM(reg, addr, imm) (0x78400000 + (reg) + ((addr) << 5) + (((imm) << 12) & 0x001ff000) )
// ldurb reg, [addr, #imm]		@	-256 <= imm < 256
#define LDURB_IMM(reg, addr, imm) (0x38400000 + (reg) + ((addr) << 5) + (((imm) << 12) & 0x001ff000) )

// store
// stp reg1, reg2 [addr, #imm]		@	-512 <= imm < 512	&	imm mod 8 = 0
#define STP64_IMM(reg1, reg2, addr, imm) (0xa9000000 + (reg1) + ((reg2) << 10) + ((addr) << 5) + ((imm) << 12) )
// str reg, [addr, #imm]		@	0 <= imm < 32768	&	imm mod 8 = 0
#define STR64_IMM(reg, addr, imm) (0xf9000000 + (reg) + ((addr) << 5) + ((imm) << 7) )
// str reg, [addr, #imm]		@	0 <= imm < 16384	&	imm mod 4 = 0
#define STR_IMM(reg, addr, imm) (0xb9000000 + (reg) + ((addr) << 5) + ((imm) << 8) )
// strh reg, [addr, #imm]		@	0 <= imm < 8192	&	imm mod 2 = 0
#define STRH_IMM(reg, addr, imm) (0x79000000 + (reg) + ((addr) << 5) + ((imm) << 9) )
// strb reg, [addr, #imm]		@	0 <= imm < 4096
#define STRB_IMM(reg, addr, imm) (0x39000000 + (reg) + ((addr) << 5) + ((imm) << 10) )
// stur reg, [addr, #imm]		@	-256 <= imm < 256
#define STUR64_IMM(reg, addr, imm) (0xf8000000 + (reg) + ((addr) << 5) + (((imm) << 12) & 0x001ff000) )
// stur reg, [addr, #imm]		@	-256 <= imm < 256
#define STUR_IMM(reg, addr, imm) (0xb8000000 + (reg) + ((addr) << 5) + (((imm) << 12) & 0x001ff000) )
// sturh reg, [addr, #imm]		@	-256 <= imm < 256
#define STURH_IMM(reg, addr, imm) (0x78000000 + (reg) + ((addr) << 5) + (((imm) << 12) & 0x001ff000) )
// sturb reg, [addr, #imm]		@	-256 <= imm < 256
#define STURB_IMM(reg, addr, imm) (0x38000000 + (reg) + ((addr) << 5) + (((imm) << 12) & 0x001ff000) )

// branch
// bgt pc+imm		@	0 <= imm < 1M	&	imm mod 4 = 0
#define BGT_FWD(imm) (0x5400000c + ((imm) << 3) )
// b pc+imm		@	0 <= imm < 128M	&	imm mod 4 = 0
#define B_FWD(imm) (0x14000000 + ((imm) >> 2) )
// br reg
#define BR(reg) (0xd61f0000 + ((reg) << 5) )
// blr reg
#define BLR_REG(reg) (0xd63f0000 + ((reg) << 5) )
// cbz reg, pc+imm		@	0 <= imm < 1M	&	imm mod 4 = 0
#define CBZ_FWD(reg, imm) (0x34000000 + (reg) + ((imm) << 3) )
// cbnz reg, pc+imm		@	0 <= imm < 1M	&	imm mod 4 = 0
#define CBNZ_FWD(reg, imm) (0x35000000 + (reg) + ((imm) << 3) )
// ret reg
#define RET_REG(reg) (0xd65f0000 + ((reg) << 5) )
// ret
#define RET RET_REG(HOST_x30)

// extend
// sxth dst, src
#define SXTH(dst, src) SBFM(dst, src, 0, 15)
// sxtb dst, src
#define SXTB(dst, src) SBFM(dst, src, 0, 7)
// uxth dst, src
#define UXTH(dst, src) UBFM(dst, src, 0, 15)
// uxtb dst, src
#define UXTB(dst, src) UBFM(dst, src, 0, 7)

// bit field
// bfi dst, src, #lsb, #width		@	lsb >= 0, width >= 1, lsb+width <= 32
#define BFI(dst, src, lsb, width) BFM(dst, src, (32 - (lsb)) & 0x1f, (width) - 1)
// bfm dst, src, #rimm, #simm		@	0 <= rimm < 32, 0 <= simm < 32
#define BFM(dst, src, rimm, simm) (0x33000000 + (dst) + ((src) << 5) + ((rimm) << 16) + ((simm) << 10) )
// sbfm dst, src, #rimm, #simm		@	0 <= rimm < 32, 0 <= simm < 32
#define SBFM(dst, src, rimm, simm) (0x13000000 + (dst) + ((src) << 5) + ((rimm) << 16) + ((simm) << 10) )
// ubfm dst, src, #rimm, #simm		@	0 <= rimm < 32, 0 <= simm < 32
#define UBFM(dst, src, rimm, simm) (0x53000000 + (dst) + ((src) << 5) + ((rimm) << 16) + ((simm) << 10) )
// bfi dst, src, #lsb, #width		@	lsb >= 0, width >= 1, lsb+width <= 64
#define BFI64(dst, src, lsb, width) BFM64(dst, src, (64 - (lsb)) & 0x3f, (width) - 1)
// bfm dst, src, #rimm, #simm		@	0 <= rimm < 64, 0 <= simm < 64
#define BFM64(dst, src, rimm, simm) (0xb3400000 + (dst) + ((src) << 5) + ((rimm) << 16) + ((simm) << 10) )
// ubfm dst, src, #rimm, #simm		@	0 <= rimm < 64, 0 <= simm < 64
#define UBFM64(dst, src, rimm, simm) (0xd3400000 + (dst) + ((src) << 5) + ((rimm) << 16) + ((simm) << 10) )


// move a full register from reg_src to reg_dst
static void gen_mov_regs(HostReg reg_dst,HostReg reg_src) {
	if(reg_src == reg_dst) return;
	cache_addd( MOV_REG_LSL_IMM(reg_dst, reg_src, 0) );      // mov reg_dst, reg_src
}

// move a 32bit constant value into dest_reg
static void gen_mov_dword_to_reg_imm(HostReg dest_reg,Bit32u imm) {
	if ( (imm & 0xffff0000) == 0 ) {
		cache_addd( MOVZ(dest_reg, imm, 0) );               // movz dest_reg, #imm
	} else if ( (imm & 0x0000ffff) == 0 ) {
		cache_addd( MOVZ(dest_reg, imm >> 16, 16) );        // movz dest_reg, #(imm >> 16), lsl #16
	} else if ( ((~imm) & 0xffff0000) == 0 ) {
		cache_addd( MOVN(dest_reg, ~imm, 0) );              // movn dest_reg, #(~imm)
	} else if ( ((~imm) & 0x0000ffff) == 0 ) {
		cache_addd( MOVN(dest_reg, (~imm) >> 16, 16) );     // movn dest_reg, #(~imm >> 16), lsl #16
	} else {
		cache_addd( MOVZ(dest_reg, imm & 0xffff, 0) );      // movz dest_reg, #(imm & 0xffff)
		cache_addd( MOVK(dest_reg, imm >> 16, 16) );        // movk dest_reg, #(imm >> 16), lsl #16
	}
}

// helper function
static bool gen_mov_memval_to_reg_helper(HostReg dest_reg, Bit64u data, Bitu size, HostReg addr_reg, Bit64u addr_data) {
	switch (size) {
		case 8:
			if (((data & 7) == 0) && (data >= addr_data) && (data < addr_data + 32768)) {
				cache_addd( LDR64_IMM(dest_reg, addr_reg, data - addr_data) );      // ldr dest_reg, [addr_reg, #(data - addr_data)]
				return true;
			} else if ((data < addr_data + 256) && (data >= addr_data - 256)) {
				cache_addd( LDUR64_IMM(dest_reg, addr_reg, data - addr_data) );     // ldur dest_reg, [addr_reg, #(data - addr_data)]
				return true;
			}
			break;
		case 4:
			if (((data & 3) == 0) && (data >= addr_data) && (data < addr_data + 16384)) {
				cache_addd( LDR_IMM(dest_reg, addr_reg, data - addr_data) );        // ldr dest_reg, [addr_reg, #(data - addr_data)]
				return true;
			} else if ((data < addr_data + 256) && (data >= addr_data - 256)) {
				cache_addd( LDUR_IMM(dest_reg, addr_reg, data - addr_data) );       // ldur dest_reg, [addr_reg, #(data - addr_data)]
				return true;
			}
			break;
		case 2:
			if (((data & 1) == 0) && (data >= addr_data) && (data < addr_data + 8192)) {
				cache_addd( LDRH_IMM(dest_reg, addr_reg, data - addr_data) );       // ldrh dest_reg, [addr_reg, #(data - addr_data)]
				return true;
			} else if ((data < addr_data + 256) && (data >= addr_data - 256)) {
				cache_addd( LDURH_IMM(dest_reg, addr_reg, data - addr_data) );      // ldurh dest_reg, [addr_reg, #(data - addr_data)]
				return true;
			}
			break;
		case 1:
			if ((data >= addr_data) && (data < addr_data + 4096)) {
				cache_addd( LDRB_IMM(dest_reg, addr_reg, data - addr_data) );       // ldrb dest_reg, [addr_reg, #(data - addr_data)]
				return true;
			} else if ((data < addr_data) && (data >= addr_data - 256)) {
				cache_addd( LDURB_IMM(dest_reg, addr_reg, data - addr_data) );      // ldurb dest_reg, [addr_reg, #(data - addr_data)]
				return true;
			}
		default:
			break;
	}
	return false;
}

// helper function
static bool gen_mov_memval_to_reg(HostReg dest_reg, void *data, Bitu size) {
	if (gen_mov_memval_to_reg_helper(dest_reg, (Bit64u)data, size, FC_REGS_ADDR, (Bit64u)&cpu_regs)) return true;
	if (gen_mov_memval_to_reg_helper(dest_reg, (Bit64u)data, size, readdata_addr, (Bit64u)&core_dynrec.readdata)) return true;
	if (gen_mov_memval_to_reg_helper(dest_reg, (Bit64u)data, size, FC_SEGS_ADDR, (Bit64u)&Segs)) return true;
	return false;
}

// helper function - move a 64bit constant value into dest_reg
static void gen_mov_qword_to_reg_imm(HostReg dest_reg,Bit64u imm) {
	bool isfirst = true;

	if ( (imm & 0xffff) != 0 ) {
		cache_addd( MOVZ64(dest_reg, imm & 0xffff, 0) );                // movz dest_reg, #(imm & 0xffff)
		isfirst = false;
	}
	if ( ((imm >> 16) & 0xffff) != 0 ) {
		if (isfirst) {
			isfirst = false;
			cache_addd( MOVZ64(dest_reg, (imm >> 16) & 0xffff, 16) );   // movz dest_reg, #((imm >> 16) & 0xffff), lsl #16
		} else {
			cache_addd( MOVK64(dest_reg, (imm >> 16) & 0xffff, 16) );   // movk dest_reg, #((imm >> 16) & 0xffff), lsl #16
		}
	}
	if ( ((imm >> 32) & 0xffff) != 0 ) {
		if (isfirst) {
			isfirst = false;
			cache_addd( MOVZ64(dest_reg, (imm >> 32) & 0xffff, 32) );   // movz dest_reg, #((imm >> 32) & 0xffff), lsl #32
		} else {
			cache_addd( MOVK64(dest_reg, (imm >> 32) & 0xffff, 32) );   // movk dest_reg, #((imm >> 32) & 0xffff), lsl #32
		}
	}
	if ( ((imm >> 48) & 0xffff) != 0 ) {
		if (isfirst) {
			isfirst = false;
			cache_addd( MOVZ64(dest_reg, (imm >> 48) & 0xffff, 48) );   // movz dest_reg, #((imm >> 48) & 0xffff), lsl #48
		} else {
			cache_addd( MOVK64(dest_reg, (imm >> 48) & 0xffff, 48) );   // movk dest_reg, #((imm >> 48) & 0xffff), lsl #48
		}
	}
	if (isfirst) {
		cache_addd( MOVZ64(dest_reg, 0, 0) );                           // movz dest_reg, #0
	}
}

// helper function for gen_mov_word_to_reg
static void gen_mov_word_to_reg_helper(HostReg dest_reg,void* data,bool dword,HostReg data_reg) {
	if (dword) {
		cache_addd( LDR_IMM(dest_reg, data_reg, 0) );       // ldr dest_reg, [data_reg]
	} else {
		cache_addd( LDRH_IMM(dest_reg, data_reg, 0) );      // ldrh dest_reg, [data_reg]
	}
}

// move a 32bit (dword==true) or 16bit (dword==false) value from memory into dest_reg
// 16bit moves may destroy the upper 16bit of the destination register
static void gen_mov_word_to_reg(HostReg dest_reg,void* data,bool dword) {
	if (!gen_mov_memval_to_reg(dest_reg, data, (dword)?4:2)) {
		gen_mov_qword_to_reg_imm(temp1, (Bit64u)data);
		gen_mov_word_to_reg_helper(dest_reg, data, dword, temp1);
	}
}

// move a 16bit constant value into dest_reg
// the upper 16bit of the destination register may be destroyed
static void INLINE gen_mov_word_to_reg_imm(HostReg dest_reg,Bit16u imm) {
	cache_addd( MOVZ(dest_reg, imm, 0) );   // movz dest_reg, #imm
}

// helper function
static bool gen_mov_memval_from_reg_helper(HostReg src_reg, Bit64u data, Bitu size, HostReg addr_reg, Bit64u addr_data) {
	switch (size) {
		case 8:
			if (((data & 7) == 0) && (data >= addr_data) && (data < addr_data + 32768)) {
				cache_addd( STR64_IMM(src_reg, addr_reg, data - addr_data) );       // str src_reg, [addr_reg, #(data - addr_data)]
				return true;
			} else if ((data < addr_data + 256) && (data >= addr_data - 256)) {
				cache_addd( STUR64_IMM(src_reg, addr_reg, data - addr_data) );      // stur src_reg, [addr_reg, #(data - addr_data)]
				return true;
			}
			break;
		case 4:
			if (((data & 3) == 0) && (data >= addr_data) && (data < addr_data + 16384)) {
				cache_addd( STR_IMM(src_reg, addr_reg, data - addr_data) );         // str src_reg, [addr_reg, #(data - addr_data)]
				return true;
			} else if ((data < addr_data + 256) && (data >= addr_data - 256)) {
				cache_addd( STUR_IMM(src_reg, addr_reg, data - addr_data) );        // stur src_reg, [addr_reg, #(data - addr_data)]
				return true;
			}
			break;
		case 2:
			if (((data & 1) == 0) && (data >= addr_data) && (data < addr_data + 8192)) {
				cache_addd( STRH_IMM(src_reg, addr_reg, data - addr_data) );        // strh src_reg, [addr_reg, #(data - addr_data)]
				return true;
			} else if ((data < addr_data + 256) && (data >= addr_data - 256)) {
				cache_addd( STURH_IMM(src_reg, addr_reg, data - addr_data) );       // sturh src_reg, [addr_reg, #(data - addr_data)]
				return true;
			}
			break;
		case 1:
			if ((data >= addr_data) && (data < addr_data + 4096)) {
				cache_addd( STRB_IMM(src_reg, addr_reg, data - addr_data) );        // strb src_reg, [addr_reg, #(data - addr_data)]
				return true;
			} else if ((data < addr_data) && (data >= addr_data - 256)) {
				cache_addd( STURB_IMM(src_reg, addr_reg, data - addr_data) );       // sturb src_reg, [addr_reg, #(data - addr_data)]
				return true;
			}
		default:
			break;
	}
	return false;
}

// helper function
static bool gen_mov_memval_from_reg(HostReg src_reg, void *dest, Bitu size) {
	if (gen_mov_memval_from_reg_helper(src_reg, (Bit64u)dest, size, FC_REGS_ADDR, (Bit64u)&cpu_regs)) return true;
	if (gen_mov_memval_from_reg_helper(src_reg, (Bit64u)dest, size, readdata_addr, (Bit64u)&core_dynrec.readdata)) return true;
	if (gen_mov_memval_from_reg_helper(src_reg, (Bit64u)dest, size, FC_SEGS_ADDR, (Bit64u)&Segs)) return true;
	return false;
}

// helper function for gen_mov_word_from_reg
static void gen_mov_word_from_reg_helper(HostReg src_reg,void* dest,bool dword, HostReg data_reg) {
	if (dword) {
		cache_addd( STR_IMM(src_reg, data_reg, 0) );        // str src_reg, [data_reg]
	} else {
		cache_addd( STRH_IMM(src_reg, data_reg, 0) );       // strh src_reg, [data_reg]
	}
}

// move 32bit (dword==true) or 16bit (dword==false) of a register into memory
static void gen_mov_word_from_reg(HostReg src_reg,void* dest,bool dword) {
	if (!gen_mov_memval_from_reg(src_reg, dest, (dword)?4:2)) {
		gen_mov_qword_to_reg_imm(temp1, (Bit64u)dest);
		gen_mov_word_from_reg_helper(src_reg, dest, dword, temp1);
	}
}

// move an 8bit value from memory into dest_reg
// the upper 24bit of the destination register can be destroyed
// this function does not use FC_OP1/FC_OP2 as dest_reg as these
// registers might not be directly byte-accessible on some architectures
static void gen_mov_byte_to_reg_low(HostReg dest_reg,void* data) {
	if (!gen_mov_memval_to_reg(dest_reg, data, 1)) {
		gen_mov_qword_to_reg_imm(temp1, (Bit64u)data);
		cache_addd( LDRB_IMM(dest_reg, temp1, 0) );     // ldrb dest_reg, [temp1]
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
	cache_addd( MOVZ(dest_reg, imm, 0) );   // movz dest_reg, #imm
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
		gen_mov_qword_to_reg_imm(temp1, (Bit64u)dest);
		cache_addd( STRB_IMM(src_reg, temp1, 0) );      // strb src_reg, [temp1]
	}
}



// convert an 8bit word to a 32bit dword
// the register is zero-extended (sign==false) or sign-extended (sign==true)
static void gen_extend_byte(bool sign,HostReg reg) {
	if (sign) {
		cache_addd( SXTB(reg, reg) );      // sxtb reg, reg
	} else {
		cache_addd( UXTB(reg, reg) );      // uxtb reg, reg
	}
}

// convert a 16bit word to a 32bit dword
// the register is zero-extended (sign==false) or sign-extended (sign==true)
static void gen_extend_word(bool sign,HostReg reg) {
	if (sign) {
		cache_addd( SXTH(reg, reg) );      // sxth reg, reg
	} else {
		cache_addd( UXTH(reg, reg) );      // uxth reg, reg
	}
}

// add a 32bit value from memory to a full register
static void gen_add(HostReg reg,void* op) {
	gen_mov_word_to_reg(temp3, op, 1);
	cache_addd( ADD_REG_LSL_IMM(reg, reg, temp3, 0) );      // add reg, reg, temp3
}

// add a 32bit constant value to a full register
static void gen_add_imm(HostReg reg,Bit32u imm) {
	Bit32u imm2;

	if(!imm) return;

	imm2 = (Bit32u) (-((Bit32s)imm));

	if (imm < 4096) {
		cache_addd( ADD_IMM(reg, reg, imm, 0) );            // add reg, reg, #imm
	} else if ((imm & 0xff000fff) == 0) {
		cache_addd( ADD_IMM(reg, reg, imm >> 12, 12) );     // add reg, reg, #(imm >> 12), lsl #12
	} else if (imm2 < 4096) {
		cache_addd( SUB_IMM(reg, reg, imm2, 0) );           // sub reg, reg, #(-imm)
	} else if ((imm2 & 0xff000fff) == 0) {
		cache_addd( SUB_IMM(reg, reg, imm2 >> 12, 12) );    // sub reg, reg, #(-imm >> 12), lsl #12
	} else if (imm2 < 0x10000) {
		cache_addd( MOVZ(temp2, imm2, 0) );                 // movz temp2, #(-imm)
		cache_addd( SUB_REG_LSL_IMM(reg, reg, temp2, 0) );  // sub reg, reg, temp2
	} else {
		gen_mov_dword_to_reg_imm(temp2, imm);
		cache_addd( ADD_REG_LSL_IMM(reg, reg, temp2, 0) );  // add reg, reg, temp2
	}
}

// and a 32bit constant value with a full register
static void gen_and_imm(HostReg reg,Bit32u imm) {
	Bit32u imm2, scale;

	imm2 = ~imm;
	if(!imm2) return;

	if (!imm) {
		cache_addd( MOVZ(reg, 0, 0) );                          // movz reg, #0
	} else if (imm2 < 0x10000) {
		cache_addd( MOVZ(temp2, imm2, 0) );                     // movz temp2, #(~imm)
		cache_addd( BIC_REG_LSL_IMM(reg, reg, temp2, 0) );      // bic reg, reg, temp2
	} else if ((imm2 & 0xffff) == 0) {
		cache_addd( MOVZ(temp2, imm2 >> 16, 16) );              // movz temp2, #(~imm >> 16), lsl #16
		cache_addd( BIC_REG_LSL_IMM(reg, reg, temp2, 0) );      // bic reg, reg, temp2
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
	gen_mov_qword_to_reg_imm(temp3, imm);
	if (!gen_mov_memval_from_reg(temp3, dest, 8)) {
		gen_mov_qword_to_reg_imm(temp1, (Bit64u)dest);
		cache_addd( STR64_IMM(temp3, temp1, 0) );       // str temp3, [temp1]
	}
}

// add a 32bit (dword==true) or 16bit (dword==false) constant value to a memory value
static void gen_add_direct_word(void* dest,Bit32u imm,bool dword) {
	if (!dword) imm &= 0xffff;
	if(!imm) return;

	if (!gen_mov_memval_to_reg(temp3, dest, (dword)?4:2)) {
		gen_mov_qword_to_reg_imm(temp1, (Bit64u)dest);
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
	Bit32u imm2;

	if (!dword) imm &= 0xffff;
	if(!imm) return;

	if (!gen_mov_memval_to_reg(temp3, dest, (dword)?4:2)) {
		gen_mov_qword_to_reg_imm(temp1, (Bit64u)dest);
		gen_mov_word_to_reg_helper(temp3, dest, dword, temp1);
	}

	imm2 = (Bit32u) (-((Bit32s)imm));

	if (imm < 4096) {
		cache_addd( SUB_IMM(temp3, temp3, imm, 0) );            // sub temp3, temp3, #imm
	} else if ((imm & 0xff000fff) == 0) {
		cache_addd( SUB_IMM(temp3, temp3, imm >> 12, 12) );     // sub temp3, temp3, #(imm >> 12), lsl #12
	} else if (imm2 < 4096) {
		cache_addd( ADD_IMM(temp3, temp3, imm2, 0) );           // add temp3, temp3, #(-imm)
	} else if ((imm2 & 0xff000fff) == 0) {
		cache_addd( ADD_IMM(temp3, temp3, imm2 >> 12, 12) );    // add temp3, temp3, #(-imm >> 12), lsl #12
	} else if (imm2 < 0x10000) {
		cache_addd( MOVZ(temp2, imm2, 0) );                     // movz temp2, #(-imm)
		cache_addd( ADD_REG_LSL_IMM(temp3, temp3, temp2, 0) );  // add temp3, temp3, temp2
	} else {
		gen_mov_dword_to_reg_imm(temp2, imm);
		cache_addd( SUB_REG_LSL_IMM(temp3, temp3, temp2, 0) );  // sub temp3, temp3, temp2
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
	cache_addd( ADD_REG_LSL_IMM(dest_reg, dest_reg, scale_reg, scale) );      // add dest_reg, dest_reg, scale_reg, lsl #scale
	gen_add_imm(dest_reg, imm);
}

// effective address calculation, destination is dest_reg
// dest_reg is scaled by scale (dest_reg*(2^scale)),
// then the immediate value is added
static INLINE void gen_lea(HostReg dest_reg,Bitu scale,Bits imm) {
	if (scale) {
		cache_addd( MOV_REG_LSL_IMM(dest_reg, dest_reg, scale) );      // mov dest_reg, dest_reg, lsl #scale
	}
	gen_add_imm(dest_reg, imm);
}

// generate a call to a parameterless function
static void INLINE gen_call_function_raw(void * func) {
	cache_addd( MOVZ64(temp1, ((Bit64u)func) & 0xffff, 0) );            // movz dest_reg, #(func & 0xffff)
	cache_addd( MOVK64(temp1, (((Bit64u)func) >> 16) & 0xffff, 16) );   // movk dest_reg, #((func >> 16) & 0xffff), lsl #16
	cache_addd( MOVK64(temp1, (((Bit64u)func) >> 32) & 0xffff, 32) );   // movk dest_reg, #((func >> 32) & 0xffff), lsl #32
	cache_addd( MOVK64(temp1, (((Bit64u)func) >> 48) & 0xffff, 48) );   // movk dest_reg, #((func >> 48) & 0xffff), lsl #48
	cache_addd( BLR_REG(temp1) );      // blr temp1
}

// generate a call to a function with paramcount parameters
// note: the parameters are loaded in the architecture specific way
// using the gen_load_param_ functions below
static DRC_PTR_SIZE_IM INLINE gen_call_function_setup(void * func,Bitu paramcount,bool fastcall=false) {
	DRC_PTR_SIZE_IM proc_addr = (DRC_PTR_SIZE_IM)cache.pos;
	gen_call_function_raw(func);
	return proc_addr;
}

// load an immediate value as param'th function parameter
static void INLINE gen_load_param_imm(Bitu imm,Bitu param) {
	gen_mov_qword_to_reg_imm(param, imm);
}

// load an address as param'th function parameter
static void INLINE gen_load_param_addr(DRC_PTR_SIZE_IM addr,Bitu param) {
	gen_mov_qword_to_reg_imm(param, addr);
}

// load a host-register as param'th function parameter
static void INLINE gen_load_param_reg(Bitu reg,Bitu param) {
	gen_mov_regs(param, reg);
}

// load a value from memory as param'th function parameter
static void INLINE gen_load_param_mem(Bitu mem,Bitu param) {
	gen_mov_word_to_reg(param, (void *)mem, 1);
}

// jump to an address pointed at by ptr, offset is in imm
static void gen_jmp_ptr(void * ptr,Bits imm=0) {
	if (!gen_mov_memval_to_reg(temp3, ptr, 8)) {
		gen_mov_qword_to_reg_imm(temp1, (Bit64u)ptr);
		cache_addd( LDR64_IMM(temp3, temp1, 0) );     // ldr temp3, [temp1]
	}

	if (((imm & 7) == 0) && (imm >= 0) && (imm < 32768)) {
		cache_addd( LDR64_IMM(temp1, temp3, imm) );                 // ldr temp1, [temp3, #imm]
	} else if ((imm < 256) && (imm >= -256)) {
		cache_addd( LDUR64_IMM(temp1, temp3, imm) );                // ldur temp1, [temp3, #imm]
	} else {
		gen_mov_qword_to_reg_imm(temp2, imm);
		cache_addd( LDR64_REG_LSL_IMM(temp1, temp3, temp2, 0) );    // ldr temp1, [temp3, temp2]
	}

	cache_addd( BR(temp1) );      // br temp1
}

// short conditional jump (+-127 bytes) if register is zero
// the destination is set by gen_fill_branch() later
static DRC_PTR_SIZE_IM gen_create_branch_on_zero(HostReg reg,bool dword) {
	if (dword) {
		cache_addd( CBZ_FWD(reg, 0) );      // cbz reg, j
	} else {
		cache_addd( UXTH(temp1, reg) );     // uxth temp1, reg
		cache_addd( CBZ_FWD(temp1, 0) );    // cbz temp1, j
	}
	return ((DRC_PTR_SIZE_IM)cache.pos-4);
}

// short conditional jump (+-127 bytes) if register is nonzero
// the destination is set by gen_fill_branch() later
static DRC_PTR_SIZE_IM gen_create_branch_on_nonzero(HostReg reg,bool dword) {
	if (dword) {
		cache_addd( CBNZ_FWD(reg, 0) );     // cbnz reg, j
	} else {
		cache_addd( UXTH(temp1, reg) );     // uxth temp1, reg
		cache_addd( CBNZ_FWD(temp1, 0) );   // cbnz temp1, j
	}
	return ((DRC_PTR_SIZE_IM)cache.pos-4);
}

// calculate relative offset and fill it into the location pointed to by data
static void INLINE gen_fill_branch(DRC_PTR_SIZE_IM data) {
#if C_DEBUG
	Bits len=(Bit64u)cache.pos-data;
	if (len<0) len=-len;
	if (len>=0x00100000) LOG_MSG("Big jump %d",len);
#endif
	*(Bit32u*)data=( (*(Bit32u*)data) & 0xff00001f ) | ( ( ((Bit64u)cache.pos - data) << 3 ) & 0x00ffffe0 );
}

// conditional jump if register is nonzero
// for isdword==true the 32bit of the register are tested
// for isdword==false the lowest 8bit of the register are tested
static DRC_PTR_SIZE_IM gen_create_branch_long_nonzero(HostReg reg,bool isdword) {
	if (isdword) {
		cache_addd( CBZ_FWD(reg, 8) );      // cbz reg, pc+8    // skip next instruction
	} else {
		cache_addd( UXTB(temp1, reg) );     // uxtb temp1, reg
		cache_addd( CBZ_FWD(temp1, 8) );    // cbz temp1, pc+8  // skip next instruction
	}
	cache_addd( B_FWD(0) );         // b j
	return ((DRC_PTR_SIZE_IM)cache.pos-4);
}

// compare 32bit-register against zero and jump if value less/equal than zero
static DRC_PTR_SIZE_IM gen_create_branch_long_leqzero(HostReg reg) {
	cache_addd( CMP_IMM(reg, 0, 0) );       // cmp reg, #0
	cache_addd( BGT_FWD(8) );               // bgt pc+8 // skip next instruction
	cache_addd( B_FWD(0) );                 // b j
	return ((DRC_PTR_SIZE_IM)cache.pos-4);
}

// calculate long relative offset and fill it into the location pointed to by data
static void INLINE gen_fill_branch_long(DRC_PTR_SIZE_IM data) {
	// optimize for shorter branches ?
	*(Bit32u*)data=( (*(Bit32u*)data) & 0xfc000000 ) | ( ( ((Bit64u)cache.pos - data) >> 2 ) & 0x03ffffff );
}

static void gen_run_code(void) {
	Bit8u *pos1, *pos2, *pos3;

	cache_addd( 0xa9bd7bfd );                                           // stp fp, lr, [sp, #-48]!
	cache_addd( 0x910003fd );                                           // mov fp, sp
	cache_addd( STP64_IMM(FC_ADDR, FC_REGS_ADDR, HOST_sp, 16) );        // stp FC_ADDR, FC_REGS_ADDR, [sp, #16]
	cache_addd( STP64_IMM(FC_SEGS_ADDR, readdata_addr, HOST_sp, 32) );  // stp FC_SEGS_ADDR, readdata_addr, [sp, #32]

	pos1 = cache.pos;
	cache_addd( 0 );
	pos2 = cache.pos;
	cache_addd( 0 );
	pos3 = cache.pos;
	cache_addd( 0 );

	cache_addd( BR(HOST_x0) );			// br x0

	// align cache.pos to 32 bytes
	if ((((Bitu)cache.pos) & 0x1f) != 0) {
		cache.pos = cache.pos + (32 - (((Bitu)cache.pos) & 0x1f));
	}

	*(Bit32u *)pos1 = LDR64_PC(FC_SEGS_ADDR, cache.pos - pos1);   // ldr FC_SEGS_ADDR, [pc, #(&Segs)]
	cache_addq((Bit64u)&Segs);                      // address of "Segs"

	*(Bit32u *)pos2 = LDR64_PC(FC_REGS_ADDR, cache.pos - pos2);   // ldr FC_REGS_ADDR, [pc, #(&cpu_regs)]
	cache_addq((Bit64u)&cpu_regs);                  // address of "cpu_regs"

	*(Bit32u *)pos3 = LDR64_PC(readdata_addr, cache.pos - pos3);  // ldr readdata_addr, [pc, #(&core_dynrec.readdata)]
	cache_addq((Bit64u)&core_dynrec.readdata);      // address of "core_dynrec.readdata"

	// align cache.pos to 32 bytes
	if ((((Bitu)cache.pos) & 0x1f) != 0) {
		cache.pos = cache.pos + (32 - (((Bitu)cache.pos) & 0x1f));
	}
}

// return from a function
static void gen_return_function(void) {
	cache_addd( LDP64_IMM(FC_ADDR, FC_REGS_ADDR, HOST_sp, 16) );        // ldp FC_ADDR, FC_REGS_ADDR, [sp, #16]
	cache_addd( LDP64_IMM(FC_SEGS_ADDR, readdata_addr, HOST_sp, 32) );  // ldp FC_SEGS_ADDR, readdata_addr, [sp, #32]
	cache_addd( 0xa8c37bfd );                                           // ldp fp, lr, [sp], #48
	cache_addd( RET );                                                  // ret
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
			*(Bit32u*)(pos+8)=ADD_REG_LSL_IMM(FC_RETOP, HOST_w0, HOST_w1, 0);	// add FC_RETOP, w0, w1
			*(Bit32u*)(pos+12)=NOP;				// nop
			*(Bit32u*)(pos+16)=NOP;				// nop
			break;
		case t_ORb:
		case t_ORw:
		case t_ORd:
			*(Bit32u*)pos=NOP;					// nop
			*(Bit32u*)(pos+4)=NOP;				// nop
			*(Bit32u*)(pos+8)=ORR_REG_LSL_IMM(FC_RETOP, HOST_w0, HOST_w1, 0);	// orr FC_RETOP, w0, w1
			*(Bit32u*)(pos+12)=NOP;				// nop
			*(Bit32u*)(pos+16)=NOP;				// nop
			break;
		case t_ANDb:
		case t_ANDw:
		case t_ANDd:
			*(Bit32u*)pos=NOP;					// nop
			*(Bit32u*)(pos+4)=NOP;				// nop
			*(Bit32u*)(pos+8)=AND_REG_LSL_IMM(FC_RETOP, HOST_w0, HOST_w1, 0);	// and FC_RETOP, w0, w1
			*(Bit32u*)(pos+12)=NOP;				// nop
			*(Bit32u*)(pos+16)=NOP;				// nop
			break;
		case t_SUBb:
		case t_SUBw:
		case t_SUBd:
			*(Bit32u*)pos=NOP;					// nop
			*(Bit32u*)(pos+4)=NOP;				// nop
			*(Bit32u*)(pos+8)=SUB_REG_LSL_IMM(FC_RETOP, HOST_w0, HOST_w1, 0);	// sub FC_RETOP, w0, w1
			*(Bit32u*)(pos+12)=NOP;				// nop
			*(Bit32u*)(pos+16)=NOP;				// nop
			break;
		case t_XORb:
		case t_XORw:
		case t_XORd:
			*(Bit32u*)pos=NOP;					// nop
			*(Bit32u*)(pos+4)=NOP;				// nop
			*(Bit32u*)(pos+8)=EOR_REG_LSL_IMM(FC_RETOP, HOST_w0, HOST_w1, 0);	// eor FC_RETOP, w0, w1
			*(Bit32u*)(pos+12)=NOP;				// nop
			*(Bit32u*)(pos+16)=NOP;				// nop
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
			*(Bit32u*)(pos+12)=NOP;				// nop
			*(Bit32u*)(pos+16)=NOP;				// nop
			break;
		case t_INCb:
		case t_INCw:
		case t_INCd:
			*(Bit32u*)pos=NOP;					// nop
			*(Bit32u*)(pos+4)=NOP;				// nop
			*(Bit32u*)(pos+8)=ADD_IMM(FC_RETOP, HOST_w0, 1, 0);	// add FC_RETOP, w0, #1
			*(Bit32u*)(pos+12)=NOP;				// nop
			*(Bit32u*)(pos+16)=NOP;				// nop
			break;
		case t_DECb:
		case t_DECw:
		case t_DECd:
			*(Bit32u*)pos=NOP;					// nop
			*(Bit32u*)(pos+4)=NOP;				// nop
			*(Bit32u*)(pos+8)=SUB_IMM(FC_RETOP, HOST_w0, 1, 0);	// sub FC_RETOP, w0, #1
			*(Bit32u*)(pos+12)=NOP;				// nop
			*(Bit32u*)(pos+16)=NOP;				// nop
			break;
		case t_SHLb:
		case t_SHLw:
		case t_SHLd:
			*(Bit32u*)pos=NOP;					// nop
			*(Bit32u*)(pos+4)=NOP;				// nop
			*(Bit32u*)(pos+8)=LSLV(FC_RETOP, HOST_w0, HOST_w1);	// lslv FC_RETOP, w0, w1
			*(Bit32u*)(pos+12)=NOP;				// nop
			*(Bit32u*)(pos+16)=NOP;				// nop
			break;
		case t_SHRb:
			*(Bit32u*)pos=NOP;					// nop
			*(Bit32u*)(pos+4)=UXTB(FC_RETOP, HOST_w0);				// uxtb FC_RETOP, w0
			*(Bit32u*)(pos+8)=NOP;				// nop
			*(Bit32u*)(pos+12)=LSRV(FC_RETOP, FC_RETOP, HOST_w1);	// lsrv FC_RETOP, FC_RETOP, w1
			*(Bit32u*)(pos+16)=NOP;				// nop
			break;
		case t_SHRw:
			*(Bit32u*)pos=NOP;					// nop
			*(Bit32u*)(pos+4)=UXTH(FC_RETOP, HOST_w0);				// uxth FC_RETOP, w0
			*(Bit32u*)(pos+8)=NOP;				// nop
			*(Bit32u*)(pos+12)=LSRV(FC_RETOP, FC_RETOP, HOST_w1);	// lsrv FC_RETOP, FC_RETOP, w1
			*(Bit32u*)(pos+16)=NOP;				// nop
			break;
		case t_SHRd:
			*(Bit32u*)pos=NOP;					// nop
			*(Bit32u*)(pos+4)=NOP;				// nop
			*(Bit32u*)(pos+8)=LSRV(FC_RETOP, HOST_w0, HOST_w1);	// lsrv FC_RETOP, w0, w1
			*(Bit32u*)(pos+12)=NOP;				// nop
			*(Bit32u*)(pos+16)=NOP;				// nop
			break;
		case t_SARb:
			*(Bit32u*)pos=NOP;					// nop
			*(Bit32u*)(pos+4)=SXTB(FC_RETOP, HOST_w0);				// sxtb FC_RETOP, w0
			*(Bit32u*)(pos+8)=NOP;				// nop
			*(Bit32u*)(pos+12)=ASRV(FC_RETOP, FC_RETOP, HOST_w1);	// asrv FC_RETOP, FC_RETOP, w1
			*(Bit32u*)(pos+16)=NOP;				// nop
			break;
		case t_SARw:
			*(Bit32u*)pos=NOP;					// nop
			*(Bit32u*)(pos+4)=SXTH(FC_RETOP, HOST_w0);				// sxth FC_RETOP, w0
			*(Bit32u*)(pos+8)=NOP;				// nop
			*(Bit32u*)(pos+12)=ASRV(FC_RETOP, FC_RETOP, HOST_w1);	// asrv FC_RETOP, FC_RETOP, w1
			*(Bit32u*)(pos+16)=NOP;				// nop
			break;
		case t_SARd:
			*(Bit32u*)pos=NOP;					// nop
			*(Bit32u*)(pos+4)=NOP;				// nop
			*(Bit32u*)(pos+8)=ASRV(FC_RETOP, HOST_w0, HOST_w1);	// asrv FC_RETOP, w0, w1
			*(Bit32u*)(pos+12)=NOP;				// nop
			*(Bit32u*)(pos+16)=NOP;				// nop
			break;
		case t_RORb:
			*(Bit32u*)pos=NOP;					// nop
			*(Bit32u*)(pos+4)=BFI(HOST_w0, HOST_w0, 8, 8);			// bfi w0, w0, 8, 8
			*(Bit32u*)(pos+8)=BFI(HOST_w0, HOST_w0, 16, 16);		// bfi w0, w0, 16, 16
			*(Bit32u*)(pos+12)=RORV(FC_RETOP, HOST_w0, HOST_w1);	// rorv FC_RETOP, w0, w1
			*(Bit32u*)(pos+16)=NOP;				// nop
			break;
		case t_RORw:
			*(Bit32u*)pos=NOP;					// nop
			*(Bit32u*)(pos+4)=BFI(HOST_w0, HOST_w0, 16, 16);		// bfi w0, w0, 16, 16
			*(Bit32u*)(pos+8)=NOP;				// nop
			*(Bit32u*)(pos+12)=RORV(FC_RETOP, HOST_w0, HOST_w1);	// rorv FC_RETOP, w0, w1
			*(Bit32u*)(pos+16)=NOP;				// nop
			break;
		case t_RORd:
			*(Bit32u*)pos=NOP;					// nop
			*(Bit32u*)(pos+4)=NOP;				// nop
			*(Bit32u*)(pos+8)=RORV(FC_RETOP, HOST_w0, HOST_w1);	// rorv FC_RETOP, w0, w1
			*(Bit32u*)(pos+12)=NOP;				// nop
			*(Bit32u*)(pos+16)=NOP;				// nop
			break;
		case t_ROLb:
			*(Bit32u*)pos=MOVZ(HOST_w2, 32, 0);									// movz w2, #32
			*(Bit32u*)(pos+4)=BFI(HOST_w0, HOST_w0, 8, 8);						// bfi w0, w0, 8, 8
			*(Bit32u*)(pos+8)=SUB_REG_LSL_IMM(HOST_w2, HOST_w2, HOST_w1, 0);	// sub w2, w2, w1
			*(Bit32u*)(pos+12)=BFI(HOST_w0, HOST_w0, 16, 16);					// bfi w0, w0, 16, 16
			*(Bit32u*)(pos+16)=RORV(FC_RETOP, HOST_w0, HOST_w2);				// rorv FC_RETOP, w0, w2
			break;
		case t_ROLw:
			*(Bit32u*)pos=MOVZ(HOST_w2, 32, 0);									// movz w2, #32
			*(Bit32u*)(pos+4)=BFI(HOST_w0, HOST_w0, 16, 16);					// bfi w0, w0, 16, 16
			*(Bit32u*)(pos+8)=SUB_REG_LSL_IMM(HOST_w2, HOST_w2, HOST_w1, 0);	// sub w2, w2, w1
			*(Bit32u*)(pos+12)=NOP;				// nop
			*(Bit32u*)(pos+16)=RORV(FC_RETOP, HOST_w0, HOST_w2);				// rorv FC_RETOP, w0, w2
			break;
		case t_ROLd:
			*(Bit32u*)pos=NOP;					// nop
			*(Bit32u*)(pos+4)=MOVZ(HOST_w2, 32, 0);								// movz w2, #32
			*(Bit32u*)(pos+8)=SUB_REG_LSL_IMM(HOST_w2, HOST_w2, HOST_w1, 0);	// sub w2, w2, w1
			*(Bit32u*)(pos+12)=RORV(FC_RETOP, HOST_w0, HOST_w2);				// rorv FC_RETOP, w0, w2
			*(Bit32u*)(pos+16)=NOP;				// nop
			break;
		case t_NEGb:
		case t_NEGw:
		case t_NEGd:
			*(Bit32u*)pos=NOP;					// nop
			*(Bit32u*)(pos+4)=NOP;				// nop
			*(Bit32u*)(pos+8)=SUB_REG_LSL_IMM(FC_RETOP, HOST_wzr, HOST_w0, 0);	// sub FC_RETOP, wzr, w0
			*(Bit32u*)(pos+12)=NOP;				// nop
			*(Bit32u*)(pos+16)=NOP;				// nop
			break;
		case t_DSHLd:
			*(Bit32u*)pos=MOVZ64(HOST_x3, 0x1f, 0);								// movz x3, #0x1f
			*(Bit32u*)(pos+4)=BFI64(HOST_x1, HOST_x0, 32, 32);					// bfi x1, x0, 32, 32
			*(Bit32u*)(pos+8)=AND64_REG_LSL_IMM(HOST_x2, HOST_x2, HOST_x3, 0);	// and x2, x2, x3
			*(Bit32u*)(pos+12)=LSLV64(FC_RETOP, HOST_x1, HOST_x2);				// lslv FC_RETOP, x1, x2
			*(Bit32u*)(pos+16)=LSR64_IMM(FC_RETOP, FC_RETOP, 32);				// lsr FC_RETOP, FC_RETOP, #32
			break;
		case t_DSHRd:
			*(Bit32u*)pos=MOVZ64(HOST_x3, 0x1f, 0);								// movz x3, #0x1f
			*(Bit32u*)(pos+4)=BFI64(HOST_x0, HOST_x1, 32, 32);					// bfi x0, x1, 32, 32
			*(Bit32u*)(pos+8)=AND64_REG_LSL_IMM(HOST_x2, HOST_x2, HOST_x3, 0);	// and x2, x2, x3
			*(Bit32u*)(pos+12)=NOP;												// nop
			*(Bit32u*)(pos+16)=LSRV64(FC_RETOP, HOST_x0, HOST_x2);				// lsrv FC_RETOP, x0, x2
			break;
		default:
			*(Bit32u*)pos=MOVZ64(temp1, ((Bit64u)fct_ptr) & 0xffff, 0);                 // movz temp1, #(fct_ptr & 0xffff)
			*(Bit32u*)(pos+4)=MOVK64(temp1, (((Bit64u)fct_ptr) >> 16) & 0xffff, 16);    // movk temp1, #((fct_ptr >> 16) & 0xffff), lsl #16
			*(Bit32u*)(pos+8)=MOVK64(temp1, (((Bit64u)fct_ptr) >> 32) & 0xffff, 32);    // movk temp1, #((fct_ptr >> 32) & 0xffff), lsl #32
			*(Bit32u*)(pos+12)=MOVK64(temp1, (((Bit64u)fct_ptr) >> 48) & 0xffff, 48);   // movk temp1, #((fct_ptr >> 48) & 0xffff), lsl #48
			break;

	}
#else
	*(Bit32u*)pos=MOVZ64(temp1, ((Bit64u)fct_ptr) & 0xffff, 0);                 // movz temp1, #(fct_ptr & 0xffff)
	*(Bit32u*)(pos+4)=MOVK64(temp1, (((Bit64u)fct_ptr) >> 16) & 0xffff, 16);    // movk temp1, #((fct_ptr >> 16) & 0xffff), lsl #16
	*(Bit32u*)(pos+8)=MOVK64(temp1, (((Bit64u)fct_ptr) >> 32) & 0xffff, 32);    // movk temp1, #((fct_ptr >> 32) & 0xffff), lsl #32
	*(Bit32u*)(pos+12)=MOVK64(temp1, (((Bit64u)fct_ptr) >> 48) & 0xffff, 48);   // movk temp1, #((fct_ptr >> 48) & 0xffff), lsl #48
#endif
}
#endif

static void cache_block_closing(Bit8u* block_start,Bitu block_size) {
	//flush cache - GCC/LLVM builtin
	__builtin___clear_cache((char *)block_start, (char *)(block_start+block_size));
}

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
