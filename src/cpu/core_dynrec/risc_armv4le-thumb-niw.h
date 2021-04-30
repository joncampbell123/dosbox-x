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



/* ARMv4 (little endian) backend by M-HT (thumb version with data pool) */


// temporary "lo" registers
#define templo1 HOST_v3
#define templo2 HOST_v4
#define templo3 HOST_v2

// register that holds function return values
#define FC_RETOP HOST_a1

// register used for address calculations,
#define FC_ADDR HOST_v1			// has to be saved across calls, see DRC_PROTECT_ADDR_REG

// register that holds the first parameter
#define FC_OP1 HOST_a1

// register that holds the second parameter
#define FC_OP2 HOST_a2

// special register that holds the third parameter for _R3 calls (byte accessible)
#define FC_OP3 HOST_a4

// register that holds byte-accessible temporary values
#define FC_TMP_BA1 HOST_a1

// register that holds byte-accessible temporary values
#define FC_TMP_BA2 HOST_a2

// temporary register for LEA
#define TEMP_REG_DRC HOST_a4

// used to hold the address of "cpu_regs" - preferably filled in function gen_run_code
#define FC_REGS_ADDR HOST_v7

// used to hold the address of "Segs" - preferably filled in function gen_run_code
#define FC_SEGS_ADDR HOST_v8

// used to hold the address of "core_dynrec.readdata" - filled in function gen_run_code
#define readdata_addr HOST_v5


// instruction encodings

// move
// mov dst, #imm		@	0 <= imm <= 255
#define MOV_IMM(dst, imm) (0x2000 + ((dst) << 8) + (imm) )
// mov dst, src
#define MOV_REG(dst, src) ADD_IMM3(dst, src, 0)
// mov dst, src
#define MOV_LO_HI(dst, src) (0x4640 + (dst) + (((src) - HOST_r8) << 3) )
// mov dst, src
#define MOV_HI_LO(dst, src) (0x4680 + ((dst) - HOST_r8) + ((src) << 3) )

// arithmetic
// add dst, src, #imm		@	0 <= imm <= 7
#define ADD_IMM3(dst, src, imm) (0x1c00 + (dst) + ((src) << 3) + ((imm) << 6) )
// add dst, #imm		@	0 <= imm <= 255
#define ADD_IMM8(dst, imm) (0x3000 + ((dst) << 8) + (imm) )
// add dst, src1, src2
#define ADD_REG(dst, src1, src2) (0x1800 + (dst) + ((src1) << 3) + ((src2) << 6) )
// add dst, pc, #imm		@	0 <= imm < 1024	&	imm mod 4 = 0
#define ADD_LO_PC_IMM(dst, imm) (0xa000 + ((dst) << 8) + ((imm) >> 2) )
// sub dst, src1, src2
#define SUB_REG(dst, src1, src2) (0x1a00 + (dst) + ((src1) << 3) + ((src2) << 6) )
// sub dst, src, #imm		@	0 <= imm <= 7
#define SUB_IMM3(dst, src, imm) (0x1e00 + (dst) + ((src) << 3) + ((imm) << 6) )
// sub dst, #imm		@	0 <= imm <= 255
#define SUB_IMM8(dst, imm) (0x3800 + ((dst) << 8) + (imm) )
// neg dst, src
#define NEG(dst, src) (0x4240 + (dst) + ((src) << 3) )
// cmp dst, #imm		@	0 <= imm <= 255
#define CMP_IMM(dst, imm) (0x2800 + ((dst) << 8) + (imm) )
// nop
#define NOP (0x46c0)

// logical
// and dst, src
#define AND(dst, src) (0x4000 + (dst) + ((src) << 3) )
// bic dst, src
#define BIC(dst, src) (0x4380 + (dst) + ((src) << 3) )
// eor dst, src
#define EOR(dst, src) (0x4040 + (dst) + ((src) << 3) )
// orr dst, src
#define ORR(dst, src) (0x4300 + (dst) + ((src) << 3) )
// mvn dst, src
#define MVN(dst, src) (0x43c0 + (dst) + ((src) << 3) )

// shift/rotate
// lsl dst, src, #imm
#define LSL_IMM(dst, src, imm) (0x0000 + (dst) + ((src) << 3) + ((imm) << 6) )
// lsl dst, reg
#define LSL_REG(dst, reg) (0x4080 + (dst) + ((reg) << 3) )
// lsr dst, src, #imm
#define LSR_IMM(dst, src, imm) (0x0800 + (dst) + ((src) << 3) + ((imm) << 6) )
// lsr dst, reg
#define LSR_REG(dst, reg) (0x40c0 + (dst) + ((reg) << 3) )
// asr dst, src, #imm
#define ASR_IMM(dst, src, imm) (0x1000 + (dst) + ((src) << 3) + ((imm) << 6) )
// asr dst, reg
#define ASR_REG(dst, reg) (0x4100 + (dst) + ((reg) << 3) )
// ror dst, reg
#define ROR_REG(dst, reg) (0x41c0 + (dst) + ((reg) << 3) )

// load
// ldr reg, [addr, #imm]		@	0 <= imm < 128	&	imm mod 4 = 0
#define LDR_IMM(reg, addr, imm) (0x6800 + (reg) + ((addr) << 3) + ((imm) << 4) )
// ldrh reg, [addr, #imm]		@	0 <= imm < 64	&	imm mod 2 = 0
#define LDRH_IMM(reg, addr, imm) (0x8800 + (reg) + ((addr) << 3) + ((imm) << 5) )
// ldrb reg, [addr, #imm]		@	0 <= imm < 32
#define LDRB_IMM(reg, addr, imm) (0x7800 + (reg) + ((addr) << 3) + ((imm) << 6) )
// ldr reg, [pc, #imm]		@	0 <= imm < 1024	&	imm mod 4 = 0
#define LDR_PC_IMM(reg, imm) (0x4800 + ((reg) << 8) + ((imm) >> 2) )
// ldr reg, [addr1, addr2]
#define LDR_REG(reg, addr1, addr2) (0x5800 + (reg) + ((addr1) << 3) + ((addr2) << 6) )

// store
// str reg, [addr, #imm]		@	0 <= imm < 128	&	imm mod 4 = 0
#define STR_IMM(reg, addr, imm) (0x6000 + (reg) + ((addr) << 3) + ((imm) << 4) )
// strh reg, [addr, #imm]		@	0 <= imm < 64	&	imm mod 2 = 0
#define STRH_IMM(reg, addr, imm) (0x8000 + (reg) + ((addr) << 3) + ((imm) << 5) )
// strb reg, [addr, #imm]		@	0 <= imm < 32
#define STRB_IMM(reg, addr, imm) (0x7000 + (reg) + ((addr) << 3) + ((imm) << 6) )

// branch
// beq pc+imm		@	0 <= imm < 256	&	imm mod 2 = 0
#define BEQ_FWD(imm) (0xd000 + ((imm) >> 1) )
// bne pc+imm		@	0 <= imm < 256	&	imm mod 2 = 0
#define BNE_FWD(imm) (0xd100 + ((imm) >> 1) )
// bgt pc+imm		@	0 <= imm < 256	&	imm mod 2 = 0
#define BGT_FWD(imm) (0xdc00 + ((imm) >> 1) )
// b pc+imm		@	0 <= imm < 2048	&	imm mod 2 = 0
#define B_FWD(imm) (0xe000 + ((imm) >> 1) )
// bx reg
#define BX(reg) (0x4700 + ((reg) << 3) )


// arm instructions

// arithmetic
// add dst, src, #(imm ror rimm)		@	0 <= imm <= 255	&	rimm mod 2 = 0
#define ARM_ADD_IMM(dst, src, imm, rimm) (0xe2800000 + ((dst) << 12) + ((src) << 16) + (imm) + ((rimm) << 7) )

// load
// ldr reg, [addr, #imm]		@	0 <= imm < 4096
#define ARM_LDR_IMM(reg, addr, imm) (0xe5900000 + ((reg) << 12) + ((addr) << 16) + (imm) )

// store
// str reg, [addr, #-(imm)]!		@	0 <= imm < 4096
#define ARM_STR_IMM_M_W(reg, addr, imm) (0xe5200000 + ((reg) << 12) + ((addr) << 16) + (imm) )

// branch
// bx reg
#define ARM_BX(reg) (0xe12fff10 + (reg) )


// data pool defines
#define CACHE_DATA_JUMP	 (2)
#define CACHE_DATA_ALIGN (32)
#define CACHE_DATA_MIN	 (32)
#define CACHE_DATA_MAX	 (288)

// data pool variables
static uint8_t * cache_datapos = NULL;	// position of data pool in the cache block
static uint32_t cache_datasize = 0;		// total size of data pool
static uint32_t cache_dataindex = 0;		// used size of data pool = index of free data item (in bytes) in data pool


// forwarded function
static void INLINE gen_create_branch_short(void * func);

// function to check distance to data pool
// if too close, then generate jump after data pool
static void cache_checkinstr(uint32_t size) {
	if (cache_datasize == 0) {
		if (cache_datapos != NULL) {
			if (cache.pos + size + CACHE_DATA_JUMP >= cache_datapos) {
				cache_datapos = NULL;
			}
		}
		return;
	}

	if (cache.pos + size + CACHE_DATA_JUMP <= cache_datapos) return;

	{
		register uint8_t * newcachepos;

		newcachepos = cache_datapos + cache_datasize;
		gen_create_branch_short(newcachepos);
		cache.pos = newcachepos;
	}

	if (cache.pos + CACHE_DATA_MAX + CACHE_DATA_ALIGN >= cache.block.active->cache.start + cache.block.active->cache.size &&
		cache.pos + CACHE_DATA_MIN + CACHE_DATA_ALIGN + (CACHE_DATA_ALIGN - CACHE_ALIGN) < cache.block.active->cache.start + cache.block.active->cache.size)
	{
		cache_datapos = (uint8_t *) (((Bitu)cache.block.active->cache.start + cache.block.active->cache.size - CACHE_DATA_ALIGN) & ~(CACHE_DATA_ALIGN - 1));
	} else {
		register uint32_t cachemodsize;

		cachemodsize = (cache.pos - cache.block.active->cache.start) & (CACHE_MAXSIZE - 1);

		if (cachemodsize + CACHE_DATA_MAX + CACHE_DATA_ALIGN <= CACHE_MAXSIZE ||
			cachemodsize + CACHE_DATA_MIN + CACHE_DATA_ALIGN + (CACHE_DATA_ALIGN - CACHE_ALIGN) > CACHE_MAXSIZE)
		{
			cache_datapos = (uint8_t *) (((Bitu)cache.pos + CACHE_DATA_MAX) & ~(CACHE_DATA_ALIGN - 1));
		} else {
			cache_datapos = (uint8_t *) (((Bitu)cache.pos + (CACHE_MAXSIZE - CACHE_DATA_ALIGN) - cachemodsize) & ~(CACHE_DATA_ALIGN - 1));
		}
	}

	cache_datasize = 0;
	cache_dataindex = 0;
}

// function to reserve item in data pool
// returns address of item
static uint8_t * cache_reservedata(void) {
	// if data pool not yet initialized, then initialize data pool
	if (GCC_UNLIKELY(cache_datapos == NULL)) {
		if (cache.pos + CACHE_DATA_MIN + CACHE_DATA_ALIGN < cache.block.active->cache.start + CACHE_DATA_MAX) {
			cache_datapos = (uint8_t *) (((Bitu)cache.block.active->cache.start + CACHE_DATA_MAX) & ~(CACHE_DATA_ALIGN - 1));
		}
	}

	// if data pool not yet used, then set data pool
	if (cache_datasize == 0) {
		// set data pool address is too close (or behind)  cache.pos then set new data pool size
		if (cache.pos + CACHE_DATA_MIN + CACHE_DATA_JUMP /*+ CACHE_DATA_ALIGN*/ > cache_datapos) {
			if (cache.pos + CACHE_DATA_MAX + CACHE_DATA_ALIGN >= cache.block.active->cache.start + cache.block.active->cache.size &&
				cache.pos + CACHE_DATA_MIN + CACHE_DATA_ALIGN + (CACHE_DATA_ALIGN - CACHE_ALIGN) < cache.block.active->cache.start + cache.block.active->cache.size)
			{
				cache_datapos = (uint8_t *) (((Bitu)cache.block.active->cache.start + cache.block.active->cache.size - CACHE_DATA_ALIGN) & ~(CACHE_DATA_ALIGN - 1));
			} else {
				register uint32_t cachemodsize;

				cachemodsize = (cache.pos - cache.block.active->cache.start) & (CACHE_MAXSIZE - 1);

				if (cachemodsize + CACHE_DATA_MAX + CACHE_DATA_ALIGN <= CACHE_MAXSIZE ||
					cachemodsize + CACHE_DATA_MIN + CACHE_DATA_ALIGN + (CACHE_DATA_ALIGN - CACHE_ALIGN) > CACHE_MAXSIZE)
				{
					cache_datapos = (uint8_t *) (((Bitu)cache.pos + CACHE_DATA_MAX) & ~(CACHE_DATA_ALIGN - 1));
				} else {
					cache_datapos = (uint8_t *) (((Bitu)cache.pos + (CACHE_MAXSIZE - CACHE_DATA_ALIGN) - cachemodsize) & ~(CACHE_DATA_ALIGN - 1));
				}
			}
		}
		// set initial data pool size
		cache_datasize = CACHE_DATA_ALIGN;
	}

	// if data pool is full, then enlarge data pool
	if (cache_dataindex == cache_datasize) {
		cache_datasize += CACHE_DATA_ALIGN;
	}

	cache_dataindex += 4;
	return (cache_datapos + (cache_dataindex - 4));
}

static void cache_block_before_close(void) {
	// if data pool in use, then resize cache block to include the data pool
	if (cache_datasize != 0)
	{
		cache.pos = cache_datapos + cache_dataindex;
	}

	// clear the values before next use
	cache_datapos = NULL;
	cache_datasize = 0;
	cache_dataindex = 0;
}


// move a full register from reg_src to reg_dst
static void gen_mov_regs(HostReg reg_dst,HostReg reg_src) {
	if(reg_src == reg_dst) return;
	cache_checkinstr(2);
	cache_addw( MOV_REG(reg_dst, reg_src) );      // mov reg_dst, reg_src
}

// helper function
static bool val_single_shift(uint32_t value, uint32_t *val_shift) {
	uint32_t shift;

	if (GCC_UNLIKELY(value == 0)) {
		*val_shift = 0;
		return true;
	}

	shift = 0;
	while ((value & 1) == 0) {
		value>>=1;
		shift+=1;
	}

	if ((value >> 8) != 0) return false;

	*val_shift = shift;
	return true;
}

// move a 32bit constant value into dest_reg
static void gen_mov_dword_to_reg_imm(HostReg dest_reg,uint32_t imm) {
	uint32_t scale;

	if (imm < 256) {
		cache_checkinstr(2);
		cache_addw( MOV_IMM(dest_reg, imm) );      // mov dest_reg, #(imm)
	} else if ((~imm) < 256) {
		cache_checkinstr(4);
		cache_addw( MOV_IMM(dest_reg, ~imm) );      // mov dest_reg, #(~imm)
		cache_addw( MVN(dest_reg, dest_reg) );      // mvn dest_reg, dest_reg
	} else if (val_single_shift(imm, &scale)) {
		cache_checkinstr(4);
		cache_addw( MOV_IMM(dest_reg, imm >> scale) );      // mov dest_reg, #(imm >> scale)
		cache_addw( LSL_IMM(dest_reg, dest_reg, scale) );      // lsl dest_reg, dest_reg, #scale
	} else {
		uint32_t diff;

		cache_checkinstr(4);

		diff = imm - ((uint32_t)cache.pos+4);

		if ((diff < 1024) && ((imm & 0x03) == 0)) {
			if (((uint32_t)cache.pos & 0x03) == 0) {
				cache_addw( ADD_LO_PC_IMM(dest_reg, diff >> 2) );      // add dest_reg, pc, #(diff >> 2)
			} else {
				cache_addw( NOP );      // nop
				cache_addw( ADD_LO_PC_IMM(dest_reg, (diff - 2) >> 2) );      // add dest_reg, pc, #((diff - 2) >> 2)
			}
		} else {
			uint8_t *datapos;

			datapos = cache_reservedata();
			*(uint32_t*)datapos=imm;

			if (((uint32_t)cache.pos & 0x03) == 0) {
				cache_addw( LDR_PC_IMM(dest_reg, datapos - (cache.pos + 4)) );      // ldr dest_reg, [pc, datapos]
			} else {
				cache_addw( LDR_PC_IMM(dest_reg, datapos - (cache.pos + 2)) );      // ldr dest_reg, [pc, datapos]
			}
		}
	}
}

// helper function
static bool gen_mov_memval_to_reg_helper(HostReg dest_reg, uint32_t data, Bitu size, HostReg addr_reg, uint32_t addr_data) {
	switch (size) {
		case 4:
#if !defined(C_UNALIGNED_MEMORY)
			if ((data & 3) == 0)
#endif
			{
				if ((data >= addr_data) && (data < addr_data + 128) && (((data - addr_data) & 3) == 0)) {
					cache_checkinstr(4);
					cache_addw( MOV_LO_HI(templo2, addr_reg) );      // mov templo2, addr_reg
					cache_addw( LDR_IMM(dest_reg, templo2, data - addr_data) );      // ldr dest_reg, [templo2, #(data - addr_data)]
					return true;
				}
			}
			break;
		case 2:
#if !defined(C_UNALIGNED_MEMORY)
			if ((data & 1) == 0)
#endif
			{
				if ((data >= addr_data) && (data < addr_data + 64) && (((data - addr_data) & 1) == 0)) {
					cache_checkinstr(4);
					cache_addw( MOV_LO_HI(templo2, addr_reg) );      // mov templo2, addr_reg
					cache_addw( LDRH_IMM(dest_reg, templo2, data - addr_data) );      // ldrh dest_reg, [templo2, #(data - addr_data)]
					return true;
				}
			}
			break;
		case 1:
			if ((data >= addr_data) && (data < addr_data + 32)) {
				cache_checkinstr(4);
				cache_addw( MOV_LO_HI(templo2, addr_reg) );      // mov templo2, addr_reg
				cache_addw( LDRB_IMM(dest_reg, templo2, data - addr_data) );      // ldrb dest_reg, [templo2, #(data - addr_data)]
				return true;
			}
		default:
			break;
	}
	return false;
}

// helper function
static bool gen_mov_memval_to_reg(HostReg dest_reg, void *data, Bitu size) {
	if (gen_mov_memval_to_reg_helper(dest_reg, (uint32_t)data, size, FC_REGS_ADDR, (uint32_t)&cpu_regs)) return true;
	if (gen_mov_memval_to_reg_helper(dest_reg, (uint32_t)data, size, readdata_addr, (uint32_t)&core_dynrec.readdata)) return true;
	if (gen_mov_memval_to_reg_helper(dest_reg, (uint32_t)data, size, FC_SEGS_ADDR, (uint32_t)&Segs)) return true;
	return false;
}

// helper function for gen_mov_word_to_reg
static void gen_mov_word_to_reg_helper(HostReg dest_reg,void* data,bool dword,HostReg data_reg) {
	// alignment....
	if (dword) {
#if !defined(C_UNALIGNED_MEMORY)
		if ((uint32_t)data & 3) {
			if ( ((uint32_t)data & 3) == 2 ) {
				cache_checkinstr(8);
				cache_addw( LDRH_IMM(dest_reg, data_reg, 0) );      // ldrh dest_reg, [data_reg]
				cache_addw( LDRH_IMM(templo1, data_reg, 2) );      // ldrh templo1, [data_reg, #2]
				cache_addw( LSL_IMM(templo1, templo1, 16) );      // lsl templo1, templo1, #16
				cache_addw( ORR(dest_reg, templo1) );      // orr dest_reg, templo1
			} else {
				cache_checkinstr(16);
				cache_addw( LDRB_IMM(dest_reg, data_reg, 0) );      // ldrb dest_reg, [data_reg]
				cache_addw( ADD_IMM3(templo1, data_reg, 1) );      // add templo1, data_reg, #1
				cache_addw( LDRH_IMM(templo1, templo1, 0) );      // ldrh templo1, [templo1]
				cache_addw( LSL_IMM(templo1, templo1, 8) );      // lsl templo1, templo1, #8
				cache_addw( ORR(dest_reg, templo1) );      // orr dest_reg, templo1
				cache_addw( LDRB_IMM(templo1, data_reg, 3) );      // ldrb templo1, [data_reg, #3]
				cache_addw( LSL_IMM(templo1, templo1, 24) );      // lsl templo1, templo1, #24
				cache_addw( ORR(dest_reg, templo1) );      // orr dest_reg, templo1
			}
		} else
#endif
		{
			cache_checkinstr(2);
			cache_addw( LDR_IMM(dest_reg, data_reg, 0) );      // ldr dest_reg, [data_reg]
		}
	} else {
#if !defined(C_UNALIGNED_MEMORY)
		if ((uint32_t)data & 1) {
			cache_checkinstr(8);
			cache_addw( LDRB_IMM(dest_reg, data_reg, 0) );      // ldrb dest_reg, [data_reg]
			cache_addw( LDRB_IMM(templo1, data_reg, 1) );      // ldrb templo1, [data_reg, #1]
			cache_addw( LSL_IMM(templo1, templo1, 8) );      // lsl templo1, templo1, #8
			cache_addw( ORR(dest_reg, templo1) );      // orr dest_reg, templo1
		} else
#endif
		{
			cache_checkinstr(2);
			cache_addw( LDRH_IMM(dest_reg, data_reg, 0) );      // ldrh dest_reg, [data_reg]
		}
	}
}

// move a 32bit (dword==true) or 16bit (dword==false) value from memory into dest_reg
// 16bit moves may destroy the upper 16bit of the destination register
static void gen_mov_word_to_reg(HostReg dest_reg,void* data,bool dword) {
	if (!gen_mov_memval_to_reg(dest_reg, data, (dword)?4:2)) {
		gen_mov_dword_to_reg_imm(templo2, (uint32_t)data);
		gen_mov_word_to_reg_helper(dest_reg, data, dword, templo2);
	}
}

// move a 16bit constant value into dest_reg
// the upper 16bit of the destination register may be destroyed
static void INLINE gen_mov_word_to_reg_imm(HostReg dest_reg,uint16_t imm) {
	gen_mov_dword_to_reg_imm(dest_reg, (uint32_t)imm);
}

// helper function
static bool gen_mov_memval_from_reg_helper(HostReg src_reg, uint32_t data, Bitu size, HostReg addr_reg, uint32_t addr_data) {
	switch (size) {
		case 4:
#if !defined(C_UNALIGNED_MEMORY)
			if ((data & 3) == 0)
#endif
			{
				if ((data >= addr_data) && (data < addr_data + 128) && (((data - addr_data) & 3) == 0)) {
					cache_checkinstr(4);
					cache_addw( MOV_LO_HI(templo2, addr_reg) );      // mov templo2, addr_reg
					cache_addw( STR_IMM(src_reg, templo2, data - addr_data) );      // str src_reg, [templo2, #(data - addr_data)]
					return true;
				}
			}
			break;
		case 2:
#if !defined(C_UNALIGNED_MEMORY)
			if ((data & 1) == 0)
#endif
			{
				if ((data >= addr_data) && (data < addr_data + 64) && (((data - addr_data) & 1) == 0)) {
					cache_checkinstr(4);
					cache_addw( MOV_LO_HI(templo2, addr_reg) );      // mov templo2, addr_reg
					cache_addw( STRH_IMM(src_reg, templo2, data - addr_data) );      // strh src_reg, [templo2, #(data - addr_data)]
					return true;
				}
			}
			break;
		case 1:
			if ((data >= addr_data) && (data < addr_data + 32)) {
				cache_checkinstr(4);
				cache_addw( MOV_LO_HI(templo2, addr_reg) );      // mov templo2, addr_reg
				cache_addw( STRB_IMM(src_reg, templo2, data - addr_data) );      // strb src_reg, [templo2, #(data - addr_data)]
				return true;
			}
		default:
			break;
	}
	return false;
}

// helper function
static bool gen_mov_memval_from_reg(HostReg src_reg, void *dest, Bitu size) {
	if (gen_mov_memval_from_reg_helper(src_reg, (uint32_t)dest, size, FC_REGS_ADDR, (uint32_t)&cpu_regs)) return true;
	if (gen_mov_memval_from_reg_helper(src_reg, (uint32_t)dest, size, readdata_addr, (uint32_t)&core_dynrec.readdata)) return true;
	if (gen_mov_memval_from_reg_helper(src_reg, (uint32_t)dest, size, FC_SEGS_ADDR, (uint32_t)&Segs)) return true;
	return false;
}

// helper function for gen_mov_word_from_reg
static void gen_mov_word_from_reg_helper(HostReg src_reg,void* dest,bool dword, HostReg data_reg) {
	// alignment....
	if (dword) {
#if !defined(C_UNALIGNED_MEMORY)
		if ((uint32_t)dest & 3) {
			if ( ((uint32_t)dest & 3) == 2 ) {
				cache_checkinstr(8);
				cache_addw( STRH_IMM(src_reg, data_reg, 0) );      // strh src_reg, [data_reg]
				cache_addw( MOV_REG(templo1, src_reg) );      // mov templo1, src_reg
				cache_addw( LSR_IMM(templo1, templo1, 16) );      // lsr templo1, templo1, #16
				cache_addw( STRH_IMM(templo1, data_reg, 2) );      // strh templo1, [data_reg, #2]
			} else {
				cache_checkinstr(20);
				cache_addw( STRB_IMM(src_reg, data_reg, 0) );      // strb src_reg, [data_reg]
				cache_addw( MOV_REG(templo1, src_reg) );      // mov templo1, src_reg
				cache_addw( LSR_IMM(templo1, templo1, 8) );      // lsr templo1, templo1, #8
				cache_addw( STRB_IMM(templo1, data_reg, 1) );      // strb templo1, [data_reg, #1]
				cache_addw( MOV_REG(templo1, src_reg) );      // mov templo1, src_reg
				cache_addw( LSR_IMM(templo1, templo1, 16) );      // lsr templo1, templo1, #16
				cache_addw( STRB_IMM(templo1, data_reg, 2) );      // strb templo1, [data_reg, #2]
				cache_addw( MOV_REG(templo1, src_reg) );      // mov templo1, src_reg
				cache_addw( LSR_IMM(templo1, templo1, 24) );      // lsr templo1, templo1, #24
				cache_addw( STRB_IMM(templo1, data_reg, 3) );      // strb templo1, [data_reg, #3]
			}
		} else
#endif
		{
			cache_checkinstr(2);
			cache_addw( STR_IMM(src_reg, data_reg, 0) );      // str src_reg, [data_reg]
		}
	} else {
#if !defined(C_UNALIGNED_MEMORY)
		if ((uint32_t)dest & 1) {
			cache_checkinstr(8);
			cache_addw( STRB_IMM(src_reg, data_reg, 0) );      // strb src_reg, [data_reg]
			cache_addw( MOV_REG(templo1, src_reg) );      // mov templo1, src_reg
			cache_addw( LSR_IMM(templo1, templo1, 8) );      // lsr templo1, templo1, #8
			cache_addw( STRB_IMM(templo1, data_reg, 1) );      // strb templo1, [data_reg, #1]
		} else
#endif
		{
			cache_checkinstr(2);
			cache_addw( STRH_IMM(src_reg, data_reg, 0) );      // strh src_reg, [data_reg]
		}
	}
}

// move 32bit (dword==true) or 16bit (dword==false) of a register into memory
static void gen_mov_word_from_reg(HostReg src_reg,void* dest,bool dword) {
	if (!gen_mov_memval_from_reg(src_reg, dest, (dword)?4:2)) {
		gen_mov_dword_to_reg_imm(templo2, (uint32_t)dest);
		gen_mov_word_from_reg_helper(src_reg, dest, dword, templo2);
	}
}

// move an 8bit value from memory into dest_reg
// the upper 24bit of the destination register can be destroyed
// this function does not use FC_OP1/FC_OP2 as dest_reg as these
// registers might not be directly byte-accessible on some architectures
static void gen_mov_byte_to_reg_low(HostReg dest_reg,void* data) {
	if (!gen_mov_memval_to_reg(dest_reg, data, 1)) {
		gen_mov_dword_to_reg_imm(templo1, (uint32_t)data);
		cache_checkinstr(2);
		cache_addw( LDRB_IMM(dest_reg, templo1, 0) );      // ldrb dest_reg, [templo1]
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
static void gen_mov_byte_to_reg_low_imm(HostReg dest_reg,uint8_t imm) {
	cache_checkinstr(2);
	cache_addw( MOV_IMM(dest_reg, imm) );      // mov dest_reg, #(imm)
}

// move an 8bit constant value into dest_reg
// the upper 24bit of the destination register can be destroyed
// this function can use FC_OP1/FC_OP2 as dest_reg which are
// not directly byte-accessible on some architectures
static void INLINE gen_mov_byte_to_reg_low_imm_canuseword(HostReg dest_reg,uint8_t imm) {
	gen_mov_byte_to_reg_low_imm(dest_reg, imm);
}

// move the lowest 8bit of a register into memory
static void gen_mov_byte_from_reg_low(HostReg src_reg,void* dest) {
	if (!gen_mov_memval_from_reg(src_reg, dest, 1)) {
		gen_mov_dword_to_reg_imm(templo1, (uint32_t)dest);
		cache_checkinstr(2);
		cache_addw( STRB_IMM(src_reg, templo1, 0) );      // strb src_reg, [templo1]
	}
}



// convert an 8bit word to a 32bit dword
// the register is zero-extended (sign==false) or sign-extended (sign==true)
static void gen_extend_byte(bool sign,HostReg reg) {
	cache_checkinstr(4);
	cache_addw( LSL_IMM(reg, reg, 24) );      // lsl reg, reg, #24

	if (sign) {
		cache_addw( ASR_IMM(reg, reg, 24) );      // asr reg, reg, #24
	} else {
		cache_addw( LSR_IMM(reg, reg, 24) );      // lsr reg, reg, #24
	}
}

// convert a 16bit word to a 32bit dword
// the register is zero-extended (sign==false) or sign-extended (sign==true)
static void gen_extend_word(bool sign,HostReg reg) {
	cache_checkinstr(4);
	cache_addw( LSL_IMM(reg, reg, 16) );      // lsl reg, reg, #16

	if (sign) {
		cache_addw( ASR_IMM(reg, reg, 16) );      // asr reg, reg, #16
	} else {
		cache_addw( LSR_IMM(reg, reg, 16) );      // lsr reg, reg, #16
	}
}

// add a 32bit value from memory to a full register
static void gen_add(HostReg reg,void* op) {
	gen_mov_word_to_reg(templo3, op, 1);
	cache_checkinstr(2);
	cache_addw( ADD_REG(reg, reg, templo3) );      // add reg, reg, templo3
}

// add a 32bit constant value to a full register
static void gen_add_imm(HostReg reg,uint32_t imm) {
	uint32_t imm2, scale;

	if(!imm) return;

	imm2 = (uint32_t) (-((int32_t)imm));

	if (imm <= 255) {
		cache_checkinstr(2);
		cache_addw( ADD_IMM8(reg, imm) );      // add reg, #imm
	} else if (imm2 <= 255) {
		cache_checkinstr(2);
		cache_addw( SUB_IMM8(reg, imm2) );      // sub reg, #(-imm)
	} else {
		if (val_single_shift(imm2, &scale)) {
			cache_checkinstr((scale)?6:4);
			cache_addw( MOV_IMM(templo1, imm2 >> scale) );      // mov templo1, #(~imm >> scale)
			if (scale) {
				cache_addw( LSL_IMM(templo1, templo1, scale) );      // lsl templo1, templo1, #scale
			}
			cache_addw( SUB_REG(reg, reg, templo1) );      // sub reg, reg, templo1
		} else {
			gen_mov_dword_to_reg_imm(templo1, imm);
			cache_checkinstr(2);
			cache_addw( ADD_REG(reg, reg, templo1) );      // add reg, reg, templo1
		}
	}
}

// and a 32bit constant value with a full register
static void gen_and_imm(HostReg reg,uint32_t imm) {
	uint32_t imm2, scale;

	imm2 = ~imm;
	if(!imm2) return;

	if (!imm) {
		cache_checkinstr(2);
		cache_addw( MOV_IMM(reg, 0) );      // mov reg, #0
	} else {
		if (val_single_shift(imm2, &scale)) {
			cache_checkinstr((scale)?6:4);
			cache_addw( MOV_IMM(templo1, imm2 >> scale) );      // mov templo1, #(~imm >> scale)
			if (scale) {
				cache_addw( LSL_IMM(templo1, templo1, scale) );      // lsl templo1, templo1, #scale
			}
			cache_addw( BIC(reg, templo1) );      // bic reg, templo1
		} else {
			gen_mov_dword_to_reg_imm(templo1, imm);
			cache_checkinstr(2);
			cache_addw( AND(reg, templo1) );      // and reg, templo1
		}
	}
}


// move a 32bit constant value into memory
static void gen_mov_direct_dword(void* dest,uint32_t imm) {
	gen_mov_dword_to_reg_imm(templo3, imm);
	gen_mov_word_from_reg(templo3, dest, 1);
}

// move an address into memory
static void INLINE gen_mov_direct_ptr(void* dest,DRC_PTR_SIZE_IM imm) {
	gen_mov_direct_dword(dest,(uint32_t)imm);
}

// add a 32bit (dword==true) or 16bit (dword==false) constant value to a memory value
static void gen_add_direct_word(void* dest,uint32_t imm,bool dword) {
	if (!dword) imm &= 0xffff;
	if(!imm) return;

	if (!gen_mov_memval_to_reg(templo3, dest, (dword)?4:2)) {
		gen_mov_dword_to_reg_imm(templo2, (uint32_t)dest);
		gen_mov_word_to_reg_helper(templo3, dest, dword, templo2);
	}
	gen_add_imm(templo3, imm);
	if (!gen_mov_memval_from_reg(templo3, dest, (dword)?4:2)) {
		gen_mov_word_from_reg_helper(templo3, dest, dword, templo2);
	}
}

// add an 8bit constant value to a dword memory value
static void gen_add_direct_byte(void* dest,int8_t imm) {
	gen_add_direct_word(dest, (int32_t)imm, 1);
}

// subtract a 32bit (dword==true) or 16bit (dword==false) constant value from a memory value
static void gen_sub_direct_word(void* dest,uint32_t imm,bool dword) {
	uint32_t imm2, scale;

	if (!dword) imm &= 0xffff;
	if(!imm) return;

	if (!gen_mov_memval_to_reg(templo3, dest, (dword)?4:2)) {
		gen_mov_dword_to_reg_imm(templo2, (uint32_t)dest);
		gen_mov_word_to_reg_helper(templo3, dest, dword, templo2);
	}

	imm2 = (uint32_t) (-((int32_t)imm));

	if (imm <= 255) {
		cache_checkinstr(2);
		cache_addw( SUB_IMM8(templo3, imm) );      // sub templo3, #imm
	} else if (imm2 <= 255) {
		cache_checkinstr(2);
		cache_addw( ADD_IMM8(templo3, imm2) );      // add templo3, #(-imm)
	} else {
		if (val_single_shift(imm2, &scale)) {
			cache_checkinstr((scale)?6:4);
			cache_addw( MOV_IMM(templo1, imm2 >> scale) );      // mov templo1, #(~imm >> scale)
			if (scale) {
				cache_addw( LSL_IMM(templo1, templo1, scale) );      // lsl templo1, templo1, #scale
			}
			cache_addw( ADD_REG(templo3, templo3, templo1) );      // add templo3, templo3, templo1
		} else {
			gen_mov_dword_to_reg_imm(templo1, imm);
			cache_checkinstr(2);
			cache_addw( SUB_REG(templo3, templo3, templo1) );      // sub templo3, templo3, templo1
		}
	}

	if (!gen_mov_memval_from_reg(templo3, dest, (dword)?4:2)) {
		gen_mov_word_from_reg_helper(templo3, dest, dword, templo2);
	}
}

// subtract an 8bit constant value from a dword memory value
static void gen_sub_direct_byte(void* dest,int8_t imm) {
	gen_sub_direct_word(dest, (int32_t)imm, 1);
}

// effective address calculation, destination is dest_reg
// scale_reg is scaled by scale (scale_reg*(2^scale)) and
// added to dest_reg, then the immediate value is added
static INLINE void gen_lea(HostReg dest_reg,HostReg scale_reg,Bitu scale,Bits imm) {
	if (scale) {
		cache_checkinstr(4);
		cache_addw( LSL_IMM(templo1, scale_reg, scale) );      // lsl templo1, scale_reg, #(scale)
		cache_addw( ADD_REG(dest_reg, dest_reg, templo1) );      // add dest_reg, dest_reg, templo1
	} else {
		cache_checkinstr(2);
		cache_addw( ADD_REG(dest_reg, dest_reg, scale_reg) );      // add dest_reg, dest_reg, scale_reg
	}
	gen_add_imm(dest_reg, imm);
}

// effective address calculation, destination is dest_reg
// dest_reg is scaled by scale (dest_reg*(2^scale)),
// then the immediate value is added
static INLINE void gen_lea(HostReg dest_reg,Bitu scale,Bits imm) {
	if (scale) {
		cache_checkinstr(2);
		cache_addw( LSL_IMM(dest_reg, dest_reg, scale) );      // lsl dest_reg, dest_reg, #(scale)
	}
	gen_add_imm(dest_reg, imm);
}

// helper function for gen_call_function_raw and gen_call_function_setup
template <typename T> static void gen_call_function_helper(const T func) {
    uint8_t *datapos;

    datapos = cache_reservedata();
    *(uint32_t*)datapos=(uint32_t)func;

    if (((uint32_t)cache.pos & 0x03) == 0) {
        cache_addw( LDR_PC_IMM(templo1, datapos - (cache.pos + 4)) );      // ldr templo1, [pc, datapos]
        cache_addw( ADD_LO_PC_IMM(templo2, 4) );      // adr templo2, after_call (add templo2, pc, #4)
        cache_addw( MOV_HI_LO(HOST_lr, templo2) );      // mov lr, templo2
        cache_addw( BX(templo1) );      // bx templo1     --- switch to arm state
    } else {
        cache_addw( LDR_PC_IMM(templo1, datapos - (cache.pos + 2)) );      // ldr templo1, [pc, datapos]
        cache_addw( ADD_LO_PC_IMM(templo2, 4) );      // adr templo2, after_call (add templo2, pc, #4)
        cache_addw( MOV_HI_LO(HOST_lr, templo2) );      // mov lr, templo2
        cache_addw( BX(templo1) );      // bx templo1     --- switch to arm state
        cache_addw( NOP );      // nop
    }
    // after_call:

    // switch from arm to thumb state
    cache_addd(0xe2800000 + (templo1 << 12) + (HOST_pc << 16) + (1));      // add templo1, pc, #1
    cache_addd(0xe12fff10 + (templo1));      // bx templo1

    // thumb state from now on
}

// generate a call to a parameterless function
template <typename T> static void INLINE gen_call_function_raw(const T func) {
    cache_checkinstr(18);
    gen_call_function_helper(func);
}

// generate a call to a function with paramcount parameters
// note: the parameters are loaded in the architecture specific way
// using the gen_load_param_ functions below
template <typename T> static uint32_t INLINE gen_call_function_setup(const T func,Bitu paramcount,bool fastcall=false) {
	cache_checkinstr(18);
	uint32_t proc_addr = (uint32_t)cache.pos;
	gen_call_function_helper(func);
	return proc_addr;
	// if proc_addr is on word  boundary ((proc_addr & 0x03) == 0)
	//   then length of generated code is 16 bytes
	//   otherwise length of generated code is 18 bytes
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
	gen_mov_word_to_reg(templo3, ptr, 1);

#if !defined(C_UNALIGNED_MEMORY)
// (*ptr) should be word aligned
	if ((imm & 0x03) == 0) {
#endif
		if ((imm >= 0) && (imm < 128) && ((imm & 3) == 0)) {
			cache_checkinstr(6);
			cache_addw( LDR_IMM(templo2, templo3, imm) );      // ldr templo2, [templo3, #imm]
		} else {
			gen_mov_dword_to_reg_imm(templo2, imm);
			cache_checkinstr(6);
			cache_addw( LDR_REG(templo2, templo3, templo2) );      // ldr templo2, [templo3, templo2]
		}
#if !defined(C_UNALIGNED_MEMORY)
	} else {
		gen_add_imm(templo3, imm);

		cache_checkinstr(24);
		cache_addw( LDRB_IMM(templo2, templo3, 0) );      // ldrb templo2, [templo3]
		cache_addw( LDRB_IMM(templo1, templo3, 1) );      // ldrb templo1, [templo3, #1]
		cache_addw( LSL_IMM(templo1, templo1, 8) );      // lsl templo1, templo1, #8
		cache_addw( ORR(templo2, templo1) );      // orr templo2, templo1
		cache_addw( LDRB_IMM(templo1, templo3, 2) );      // ldrb templo1, [templo3, #2]
		cache_addw( LSL_IMM(templo1, templo1, 16) );      // lsl templo1, templo1, #16
		cache_addw( ORR(templo2, templo1) );      // orr templo2, templo1
		cache_addw( LDRB_IMM(templo1, templo3, 3) );      // ldrb templo1, [templo3, #3]
		cache_addw( LSL_IMM(templo1, templo1, 24) );      // lsl templo1, templo1, #24
		cache_addw( ORR(templo2, templo1) );      // orr templo2, templo1
	}
#endif

	// increase jmp address to keep thumb state
	cache_addw( ADD_IMM3(templo2, templo2, 1) );      // add templo2, templo2, #1

	cache_addw( BX(templo2) );      // bx templo2
}

// short conditional jump (+-127 bytes) if register is zero
// the destination is set by gen_fill_branch() later
static uint32_t gen_create_branch_on_zero(HostReg reg,bool dword) {
	cache_checkinstr(4);
	if (dword) {
		cache_addw( CMP_IMM(reg, 0) );      // cmp reg, #0
	} else {
		cache_addw( LSL_IMM(templo1, reg, 16) );      // lsl templo1, reg, #16
	}
	cache_addw( BEQ_FWD(0) );      // beq j
	return ((uint32_t)cache.pos-2);
}

// short conditional jump (+-127 bytes) if register is nonzero
// the destination is set by gen_fill_branch() later
static uint32_t gen_create_branch_on_nonzero(HostReg reg,bool dword) {
	cache_checkinstr(4);
	if (dword) {
		cache_addw( CMP_IMM(reg, 0) );      // cmp reg, #0
	} else {
		cache_addw( LSL_IMM(templo1, reg, 16) );      // lsl templo1, reg, #16
	}
	cache_addw( BNE_FWD(0) );      // bne j
	return ((uint32_t)cache.pos-2);
}

// calculate relative offset and fill it into the location pointed to by data
static void INLINE gen_fill_branch(DRC_PTR_SIZE_IM data) {
#if C_DEBUG
	Bits len=(uint32_t)cache.pos-(data+4);
	if (len<0) len=-len;
	if (len>252) LOG_MSG("Big jump %d",len);
#endif
	*(uint8_t*)data=(uint8_t)( ((uint32_t)cache.pos-(data+4)) >> 1 );
}


// conditional jump if register is nonzero
// for isdword==true the 32bit of the register are tested
// for isdword==false the lowest 8bit of the register are tested
static uint32_t gen_create_branch_long_nonzero(HostReg reg,bool isdword) {
	uint8_t *datapos;

	cache_checkinstr(8);
	datapos = cache_reservedata();

	if (isdword) {
		cache_addw( CMP_IMM(reg, 0) );      // cmp reg, #0
	} else {
		cache_addw( LSL_IMM(templo2, reg, 24) );      // lsl templo2, reg, #24
	}
	cache_addw( BEQ_FWD(2) );      // beq nobranch (pc+2)
	if (((uint32_t)cache.pos & 0x03) == 0) {
		cache_addw( LDR_PC_IMM(templo1, datapos - (cache.pos + 4)) );      // ldr templo1, [pc, datapos]
	} else {
		cache_addw( LDR_PC_IMM(templo1, datapos - (cache.pos + 2)) );      // ldr templo1, [pc, datapos]
	}
	cache_addw( BX(templo1) );      // bx templo1
	// nobranch:
	return ((uint32_t)datapos);
}

// compare 32bit-register against zero and jump if value less/equal than zero
static uint32_t gen_create_branch_long_leqzero(HostReg reg) {
	uint8_t *datapos;

	cache_checkinstr(8);
	datapos = cache_reservedata();

	cache_addw( CMP_IMM(reg, 0) );      // cmp reg, #0
	cache_addw( BGT_FWD(2) );      // bgt nobranch (pc+2)
	if (((uint32_t)cache.pos & 0x03) == 0) {
		cache_addw( LDR_PC_IMM(templo1, datapos - (cache.pos + 4)) );      // ldr templo1, [pc, datapos]
	} else {
		cache_addw( LDR_PC_IMM(templo1, datapos - (cache.pos + 2)) );      // ldr templo1, [pc, datapos]
	}
	cache_addw( BX(templo1) );      // bx templo1
	// nobranch:
	return ((uint32_t)datapos);
}

// calculate long relative offset and fill it into the location pointed to by data
static void INLINE gen_fill_branch_long(uint32_t data) {
	// this is an absolute branch
	*(uint32_t*)data=((uint32_t)cache.pos) + 1; // add 1 to keep processor in thumb state
}

static void gen_run_code(void) {
	uint8_t *pos1, *pos2, *pos3;

#if (__ARM_EABI__)
	// 8-byte stack alignment
	cache_addd(0xe92d4ff0);			// stmfd sp!, {v1-v8,lr}
#else
	cache_addd(0xe92d4df0);			// stmfd sp!, {v1-v5,v7,v8,lr}
#endif

	cache_addd( ARM_ADD_IMM(HOST_r0, HOST_r0, 1, 0) );      // add r0, r0, #1

	pos1 = cache.pos;
	cache_addd( 0 );
	pos2 = cache.pos;
	cache_addd( 0 );
	pos3 = cache.pos;
	cache_addd( 0 );

	cache_addd( ARM_ADD_IMM(HOST_lr, HOST_pc, 4, 0) );			// add lr, pc, #4
	cache_addd( ARM_STR_IMM_M_W(HOST_lr, HOST_sp, 4) );      // str lr, [sp, #-4]!
	cache_addd( ARM_BX(HOST_r0) );			// bx r0

#if (__ARM_EABI__)
	cache_addd(0xe8bd4ff0);			// ldmfd sp!, {v1-v8,lr}
#else
	cache_addd(0xe8bd4df0);			// ldmfd sp!, {v1-v5,v7,v8,lr}
#endif
	cache_addd( ARM_BX(HOST_lr) );			// bx lr

	// align cache.pos to 32 bytes
	if ((((Bitu)cache.pos) & 0x1f) != 0) {
		cache.pos = cache.pos + (32 - (((Bitu)cache.pos) & 0x1f));
	}

	*(uint32_t*)pos1 = ARM_LDR_IMM(FC_SEGS_ADDR, HOST_pc, cache.pos - (pos1 + 8));      // ldr FC_SEGS_ADDR, [pc, #(&Segs)]
	cache_addd((uint32_t)&Segs);      // address of "Segs"

	*(uint32_t*)pos2 = ARM_LDR_IMM(FC_REGS_ADDR, HOST_pc, cache.pos - (pos2 + 8));      // ldr FC_REGS_ADDR, [pc, #(&cpu_regs)]
	cache_addd((uint32_t)&cpu_regs);  // address of "cpu_regs"

	*(uint32_t*)pos3 = ARM_LDR_IMM(readdata_addr, HOST_pc, cache.pos - (pos3 + 8));      // ldr readdata_addr, [pc, #(&core_dynrec.readdata)]
	cache_addd((uint32_t)&core_dynrec.readdata);  // address of "core_dynrec.readdata"

	// align cache.pos to 32 bytes
	if ((((Bitu)cache.pos) & 0x1f) != 0) {
		cache.pos = cache.pos + (32 - (((Bitu)cache.pos) & 0x1f));
	}
}

// return from a function
static void gen_return_function(void) {
	cache_checkinstr(4);
	cache_addw(0xbc08);      // pop {r3}
	cache_addw( BX(HOST_r3) );      // bx r3
}


// short unconditional jump (over data pool)
// must emit at most CACHE_DATA_JUMP bytes
static void INLINE gen_create_branch_short(void * func) {
	cache_addw( B_FWD((uint32_t)func - ((uint32_t)cache.pos + 4)) );      // b func
}


#ifdef DRC_FLAGS_INVALIDATION

// called when a call to a function can be replaced by a
// call to a simpler function
static void gen_fill_function_ptr(uint8_t * pos,void* fct_ptr,Bitu flags_type) {
	if ((*(uint16_t*)pos & 0xf000) == 0xe000) {
		if ((*(uint16_t*)pos & 0x0fff) >= ((CACHE_DATA_ALIGN / 2) - 1) &&
			(*(uint16_t*)pos & 0x0fff) < 0x0800)
		{
			pos = (uint8_t *) ( ( ( (uint32_t)(*(uint16_t*)pos & 0x0fff) ) << 1 ) + ((uint32_t)pos + 4) );
		}
	}

#ifdef DRC_FLAGS_INVALIDATION_DCODE
	if (((uint32_t)pos & 0x03) == 0)
	{
		// try to avoid function calls but rather directly fill in code
		switch (flags_type) {
			case t_ADDb:
			case t_ADDw:
			case t_ADDd:
				*(uint16_t*)pos=ADD_REG(HOST_a1, HOST_a1, HOST_a2);	// add a1, a1, a2
				*(uint16_t*)(pos+2)=B_FWD(10);						// b after_call (pc+10)
				break;
			case t_ORb:
			case t_ORw:
			case t_ORd:
				*(uint16_t*)pos=ORR(HOST_a1, HOST_a2);				// orr a1, a2
				*(uint16_t*)(pos+2)=B_FWD(10);						// b after_call (pc+10)
				break;
			case t_ANDb:
			case t_ANDw:
			case t_ANDd:
				*(uint16_t*)pos=AND(HOST_a1, HOST_a2);				// and a1, a2
				*(uint16_t*)(pos+2)=B_FWD(10);						// b after_call (pc+10)
				break;
			case t_SUBb:
			case t_SUBw:
			case t_SUBd:
				*(uint16_t*)pos=SUB_REG(HOST_a1, HOST_a1, HOST_a2);	// sub a1, a1, a2
				*(uint16_t*)(pos+2)=B_FWD(10);						// b after_call (pc+10)
				break;
			case t_XORb:
			case t_XORw:
			case t_XORd:
				*(uint16_t*)pos=EOR(HOST_a1, HOST_a2);				// eor a1, a2
				*(uint16_t*)(pos+2)=B_FWD(10);						// b after_call (pc+10)
				break;
			case t_CMPb:
			case t_CMPw:
			case t_CMPd:
			case t_TESTb:
			case t_TESTw:
			case t_TESTd:
				*(uint16_t*)pos=B_FWD(12);							// b after_call (pc+12)
				break;
			case t_INCb:
			case t_INCw:
			case t_INCd:
				*(uint16_t*)pos=ADD_IMM3(HOST_a1, HOST_a1, 1);		// add a1, a1, #1
				*(uint16_t*)(pos+2)=B_FWD(10);						// b after_call (pc+10)
				break;
			case t_DECb:
			case t_DECw:
			case t_DECd:
				*(uint16_t*)pos=SUB_IMM3(HOST_a1, HOST_a1, 1);		// sub a1, a1, #1
				*(uint16_t*)(pos+2)=B_FWD(10);						// b after_call (pc+10)
				break;
			case t_SHLb:
			case t_SHLw:
			case t_SHLd:
				*(uint16_t*)pos=LSL_REG(HOST_a1, HOST_a2);			// lsl a1, a2
				*(uint16_t*)(pos+2)=B_FWD(10);						// b after_call (pc+10)
				break;
			case t_SHRb:
				*(uint16_t*)pos=LSL_IMM(HOST_a1, HOST_a1, 24);		// lsl a1, a1, #24
				*(uint16_t*)(pos+2)=LSR_IMM(HOST_a1, HOST_a1, 24);	// lsr a1, a1, #24
				*(uint16_t*)(pos+4)=LSR_REG(HOST_a1, HOST_a2);		// lsr a1, a2
				*(uint16_t*)(pos+6)=B_FWD(6);							// b after_call (pc+6)
				break;
			case t_SHRw:
				*(uint16_t*)pos=LSL_IMM(HOST_a1, HOST_a1, 16);		// lsl a1, a1, #16
				*(uint16_t*)(pos+2)=LSR_IMM(HOST_a1, HOST_a1, 16);	// lsr a1, a1, #16
				*(uint16_t*)(pos+4)=LSR_REG(HOST_a1, HOST_a2);		// lsr a1, a2
				*(uint16_t*)(pos+6)=B_FWD(6);							// b after_call (pc+6)
				break;
			case t_SHRd:
				*(uint16_t*)pos=LSR_REG(HOST_a1, HOST_a2);			// lsr a1, a2
				*(uint16_t*)(pos+2)=B_FWD(10);						// b after_call (pc+10)
				break;
			case t_SARb:
				*(uint16_t*)pos=LSL_IMM(HOST_a1, HOST_a1, 24);		// lsl a1, a1, #24
				*(uint16_t*)(pos+2)=ASR_IMM(HOST_a1, HOST_a1, 24);	// asr a1, a1, #24
				*(uint16_t*)(pos+4)=ASR_REG(HOST_a1, HOST_a2);		// asr a1, a2
				*(uint16_t*)(pos+6)=B_FWD(6);							// b after_call (pc+6)
				break;
			case t_SARw:
				*(uint16_t*)pos=LSL_IMM(HOST_a1, HOST_a1, 16);		// lsl a1, a1, #16
				*(uint16_t*)(pos+2)=ASR_IMM(HOST_a1, HOST_a1, 16);	// asr a1, a1, #16
				*(uint16_t*)(pos+4)=ASR_REG(HOST_a1, HOST_a2);		// asr a1, a2
				*(uint16_t*)(pos+6)=B_FWD(6);							// b after_call (pc+6)
				break;
			case t_SARd:
				*(uint16_t*)pos=ASR_REG(HOST_a1, HOST_a2);			// asr a1, a2
				*(uint16_t*)(pos+2)=B_FWD(10);						// b after_call (pc+10)
				break;
			case t_RORb:
				*(uint16_t*)pos=LSL_IMM(HOST_a1, HOST_a1, 24);		// lsl a1, a1, #24
				*(uint16_t*)(pos+2)=LSR_IMM(templo1, HOST_a1, 8);		// lsr templo1, a1, #8
				*(uint16_t*)(pos+4)=ORR(HOST_a1, templo1);			// orr a1, templo1
				*(uint16_t*)(pos+6)=NOP;								// nop
				*(uint16_t*)(pos+8)=LSR_IMM(templo1, HOST_a1, 16);	// lsr templo1, a1, #16
				*(uint16_t*)(pos+10)=NOP;								// nop
				*(uint16_t*)(pos+12)=ORR(HOST_a1, templo1);			// orr a1, templo1
				*(uint16_t*)(pos+14)=ROR_REG(HOST_a1, HOST_a2);		// ror a1, a2
				break;
			case t_RORw:
				*(uint16_t*)pos=LSL_IMM(HOST_a1, HOST_a1, 16);		// lsl a1, a1, #16
				*(uint16_t*)(pos+2)=LSR_IMM(templo1, HOST_a1, 16);	// lsr templo1, a1, #16
				*(uint16_t*)(pos+4)=ORR(HOST_a1, templo1);			// orr a1, templo1
				*(uint16_t*)(pos+6)=ROR_REG(HOST_a1, HOST_a2);		// ror a1, a2
				*(uint16_t*)(pos+8)=B_FWD(4);							// b after_call (pc+4)
				break;
			case t_RORd:
				*(uint16_t*)pos=ROR_REG(HOST_a1, HOST_a2);			// ror a1, a2
				*(uint16_t*)(pos+2)=B_FWD(10);						// b after_call (pc+10)
				break;
			case t_ROLb:
				*(uint16_t*)pos=LSL_IMM(HOST_a1, HOST_a1, 24);		// lsl a1, a1, #24
				*(uint16_t*)(pos+2)=NEG(HOST_a2, HOST_a2);			// neg a2, a2
				*(uint16_t*)(pos+4)=LSR_IMM(templo1, HOST_a1, 8);		// lsr templo1, a1, #8
				*(uint16_t*)(pos+6)=ADD_IMM8(HOST_a2, 32);			// add a2, #32
				*(uint16_t*)(pos+8)=ORR(HOST_a1, templo1);			// orr a1, templo1
				*(uint16_t*)(pos+10)=LSR_IMM(templo1, HOST_a1, 16);	// lsr templo1, a1, #16
				*(uint16_t*)(pos+12)=ORR(HOST_a1, templo1);			// orr a1, templo1
				*(uint16_t*)(pos+14)=ROR_REG(HOST_a1, HOST_a2);		// ror a1, a2
				break;
			case t_ROLw:
				*(uint16_t*)pos=LSL_IMM(HOST_a1, HOST_a1, 16);		// lsl a1, a1, #16
				*(uint16_t*)(pos+2)=NEG(HOST_a2, HOST_a2);			// neg a2, a2
				*(uint16_t*)(pos+4)=LSR_IMM(templo1, HOST_a1, 16);	// lsr templo1, a1, #16
				*(uint16_t*)(pos+6)=ADD_IMM8(HOST_a2, 32);			// add a2, #32
				*(uint16_t*)(pos+8)=ORR(HOST_a1, templo1);			// orr a1, templo1
				*(uint16_t*)(pos+10)=NOP;								// nop
				*(uint16_t*)(pos+12)=ROR_REG(HOST_a1, HOST_a2);		// ror a1, a2
				*(uint16_t*)(pos+14)=NOP;								// nop
				break;
			case t_ROLd:
				*(uint16_t*)pos=NEG(HOST_a2, HOST_a2);				// neg a2, a2
				*(uint16_t*)(pos+2)=ADD_IMM8(HOST_a2, 32);			// add a2, #32
				*(uint16_t*)(pos+4)=ROR_REG(HOST_a1, HOST_a2);		// ror a1, a2
				*(uint16_t*)(pos+6)=B_FWD(6);							// b after_call (pc+6)
				break;
			case t_NEGb:
			case t_NEGw:
			case t_NEGd:
				*(uint16_t*)pos=NEG(HOST_a1, HOST_a1);				// neg a1, a1
				*(uint16_t*)(pos+2)=B_FWD(10);						// b after_call (pc+10)
				break;
			default:
				*(uint32_t*)( ( ((uint32_t) (*pos)) << 2 ) + ((uint32_t)pos + 4) ) = (uint32_t)fct_ptr;		// simple_func
				break;
		}
	}
	else
	{
		// try to avoid function calls but rather directly fill in code
		switch (flags_type) {
			case t_ADDb:
			case t_ADDw:
			case t_ADDd:
				*(uint16_t*)pos=ADD_REG(HOST_a1, HOST_a1, HOST_a2);	// add a1, a1, a2
				*(uint16_t*)(pos+2)=B_FWD(12);						// b after_call (pc+12)
				break;
			case t_ORb:
			case t_ORw:
			case t_ORd:
				*(uint16_t*)pos=ORR(HOST_a1, HOST_a2);				// orr a1, a2
				*(uint16_t*)(pos+2)=B_FWD(12);						// b after_call (pc+12)
				break;
			case t_ANDb:
			case t_ANDw:
			case t_ANDd:
				*(uint16_t*)pos=AND(HOST_a1, HOST_a2);				// and a1, a2
				*(uint16_t*)(pos+2)=B_FWD(12);						// b after_call (pc+12)
				break;
			case t_SUBb:
			case t_SUBw:
			case t_SUBd:
				*(uint16_t*)pos=SUB_REG(HOST_a1, HOST_a1, HOST_a2);	// sub a1, a1, a2
				*(uint16_t*)(pos+2)=B_FWD(12);						// b after_call (pc+12)
				break;
			case t_XORb:
			case t_XORw:
			case t_XORd:
				*(uint16_t*)pos=EOR(HOST_a1, HOST_a2);				// eor a1, a2
				*(uint16_t*)(pos+2)=B_FWD(12);						// b after_call (pc+12)
				break;
			case t_CMPb:
			case t_CMPw:
			case t_CMPd:
			case t_TESTb:
			case t_TESTw:
			case t_TESTd:
				*(uint16_t*)pos=B_FWD(14);							// b after_call (pc+14)
				break;
			case t_INCb:
			case t_INCw:
			case t_INCd:
				*(uint16_t*)pos=ADD_IMM3(HOST_a1, HOST_a1, 1);		// add a1, a1, #1
				*(uint16_t*)(pos+2)=B_FWD(12);						// b after_call (pc+12)
				break;
			case t_DECb:
			case t_DECw:
			case t_DECd:
				*(uint16_t*)pos=SUB_IMM3(HOST_a1, HOST_a1, 1);		// sub a1, a1, #1
				*(uint16_t*)(pos+2)=B_FWD(12);						// b after_call (pc+12)
				break;
			case t_SHLb:
			case t_SHLw:
			case t_SHLd:
				*(uint16_t*)pos=LSL_REG(HOST_a1, HOST_a2);			// lsl a1, a2
				*(uint16_t*)(pos+2)=B_FWD(12);						// b after_call (pc+12)
				break;
			case t_SHRb:
				*(uint16_t*)pos=LSL_IMM(HOST_a1, HOST_a1, 24);		// lsl a1, a1, #24
				*(uint16_t*)(pos+2)=LSR_IMM(HOST_a1, HOST_a1, 24);	// lsr a1, a1, #24
				*(uint16_t*)(pos+4)=LSR_REG(HOST_a1, HOST_a2);		// lsr a1, a2
				*(uint16_t*)(pos+6)=B_FWD(8);							// b after_call (pc+8)
				break;
			case t_SHRw:
				*(uint16_t*)pos=LSL_IMM(HOST_a1, HOST_a1, 16);		// lsl a1, a1, #16
				*(uint16_t*)(pos+2)=LSR_IMM(HOST_a1, HOST_a1, 16);	// lsr a1, a1, #16
				*(uint16_t*)(pos+4)=LSR_REG(HOST_a1, HOST_a2);		// lsr a1, a2
				*(uint16_t*)(pos+6)=B_FWD(8);							// b after_call (pc+8)
				break;
			case t_SHRd:
				*(uint16_t*)pos=LSR_REG(HOST_a1, HOST_a2);			// lsr a1, a2
				*(uint16_t*)(pos+2)=B_FWD(12);						// b after_call (pc+12)
				break;
			case t_SARb:
				*(uint16_t*)pos=LSL_IMM(HOST_a1, HOST_a1, 24);		// lsl a1, a1, #24
				*(uint16_t*)(pos+2)=ASR_IMM(HOST_a1, HOST_a1, 24);	// asr a1, a1, #24
				*(uint16_t*)(pos+4)=ASR_REG(HOST_a1, HOST_a2);		// asr a1, a2
				*(uint16_t*)(pos+6)=B_FWD(8);							// b after_call (pc+8)
				break;
			case t_SARw:
				*(uint16_t*)pos=LSL_IMM(HOST_a1, HOST_a1, 16);		// lsl a1, a1, #16
				*(uint16_t*)(pos+2)=ASR_IMM(HOST_a1, HOST_a1, 16);	// asr a1, a1, #16
				*(uint16_t*)(pos+4)=ASR_REG(HOST_a1, HOST_a2);		// asr a1, a2
				*(uint16_t*)(pos+6)=B_FWD(8);							// b after_call (pc+8)
				break;
			case t_SARd:
				*(uint16_t*)pos=ASR_REG(HOST_a1, HOST_a2);			// asr a1, a2
				*(uint16_t*)(pos+2)=B_FWD(12);						// b after_call (pc+12)
				break;
			case t_RORb:
				*(uint16_t*)pos=LSL_IMM(HOST_a1, HOST_a1, 24);		// lsl a1, a1, #24
				*(uint16_t*)(pos+2)=LSR_IMM(templo1, HOST_a1, 8);		// lsr templo1, a1, #8
				*(uint16_t*)(pos+4)=ORR(HOST_a1, templo1);			// orr a1, templo1
				*(uint16_t*)(pos+6)=NOP;								// nop
				*(uint16_t*)(pos+8)=LSR_IMM(templo1, HOST_a1, 16);	// lsr templo1, a1, #16
				*(uint16_t*)(pos+10)=NOP;								// nop
				*(uint16_t*)(pos+12)=ORR(HOST_a1, templo1);			// orr a1, templo1
				*(uint16_t*)(pos+14)=NOP;								// nop
				*(uint16_t*)(pos+16)=ROR_REG(HOST_a1, HOST_a2);		// ror a1, a2
				break;
			case t_RORw:
				*(uint16_t*)pos=LSL_IMM(HOST_a1, HOST_a1, 16);		// lsl a1, a1, #16
				*(uint16_t*)(pos+2)=LSR_IMM(templo1, HOST_a1, 16);	// lsr templo1, a1, #16
				*(uint16_t*)(pos+4)=ORR(HOST_a1, templo1);			// orr a1, templo1
				*(uint16_t*)(pos+6)=ROR_REG(HOST_a1, HOST_a2);		// ror a1, a2
				*(uint16_t*)(pos+8)=B_FWD(6);							// b after_call (pc+6)
				break;
			case t_RORd:
				*(uint16_t*)pos=ROR_REG(HOST_a1, HOST_a2);			// ror a1, a2
				*(uint16_t*)(pos+2)=B_FWD(12);						// b after_call (pc+12)
				break;
			case t_ROLb:
				*(uint16_t*)pos=LSL_IMM(HOST_a1, HOST_a1, 24);		// lsl a1, a1, #24
				*(uint16_t*)(pos+2)=NEG(HOST_a2, HOST_a2);			// neg a2, a2
				*(uint16_t*)(pos+4)=LSR_IMM(templo1, HOST_a1, 8);		// lsr templo1, a1, #8
				*(uint16_t*)(pos+6)=ADD_IMM8(HOST_a2, 32);			// add a2, #32
				*(uint16_t*)(pos+8)=ORR(HOST_a1, templo1);			// orr a1, templo1
				*(uint16_t*)(pos+10)=LSR_IMM(templo1, HOST_a1, 16);	// lsr templo1, a1, #16
				*(uint16_t*)(pos+12)=ORR(HOST_a1, templo1);			// orr a1, templo1
				*(uint16_t*)(pos+14)=NOP;								// nop
				*(uint16_t*)(pos+16)=ROR_REG(HOST_a1, HOST_a2);		// ror a1, a2
				break;
			case t_ROLw:
				*(uint16_t*)pos=LSL_IMM(HOST_a1, HOST_a1, 16);		// lsl a1, a1, #16
				*(uint16_t*)(pos+2)=NEG(HOST_a2, HOST_a2);			// neg a2, a2
				*(uint16_t*)(pos+4)=LSR_IMM(templo1, HOST_a1, 16);	// lsr templo1, a1, #16
				*(uint16_t*)(pos+6)=ADD_IMM8(HOST_a2, 32);			// add a2, #32
				*(uint16_t*)(pos+8)=ORR(HOST_a1, templo1);			// orr a1, templo1
				*(uint16_t*)(pos+10)=NOP;								// nop
				*(uint16_t*)(pos+12)=ROR_REG(HOST_a1, HOST_a2);		// ror a1, a2
				*(uint16_t*)(pos+14)=NOP;								// nop
				*(uint16_t*)(pos+16)=NOP;								// nop
				break;
			case t_ROLd:
				*(uint16_t*)pos=NEG(HOST_a2, HOST_a2);				// neg a2, a2
				*(uint16_t*)(pos+2)=ADD_IMM8(HOST_a2, 32);			// add a2, #32
				*(uint16_t*)(pos+4)=ROR_REG(HOST_a1, HOST_a2);		// ror a1, a2
				*(uint16_t*)(pos+6)=B_FWD(8);							// b after_call (pc+8)
				break;
			case t_NEGb:
			case t_NEGw:
			case t_NEGd:
				*(uint16_t*)pos=NEG(HOST_a1, HOST_a1);				// neg a1, a1
				*(uint16_t*)(pos+2)=B_FWD(12);						// b after_call (pc+12)
				break;
			default:
				*(uint32_t*)( ( ((uint32_t) (*pos)) << 2 ) + ((uint32_t)pos + 2) ) = (uint32_t)fct_ptr;		// simple_func
				break;
		}

	}
#else
	if (((uint32_t)pos & 0x03) == 0)
	{
		*(uint32_t*)( ( ((uint32_t) (*pos)) << 2 ) + ((uint32_t)pos + 4) ) = (uint32_t)fct_ptr;		// simple_func
	}
	else
	{
		*(uint32_t*)( ( ((uint32_t) (*pos)) << 2 ) + ((uint32_t)pos + 2) ) = (uint32_t)fct_ptr;		// simple_func
	}
#endif
}
#endif

#ifdef DRC_USE_SEGS_ADDR

// mov 16bit value from Segs[index] into dest_reg using FC_SEGS_ADDR (index modulo 2 must be zero)
// 16bit moves may destroy the upper 16bit of the destination register
static void gen_mov_seg16_to_reg(HostReg dest_reg,Bitu index) {
	cache_checkinstr(4);
	cache_addw( MOV_LO_HI(templo1, FC_SEGS_ADDR) );      // mov templo1, FC_SEGS_ADDR
	cache_addw( LDRH_IMM(dest_reg, templo1, index) );      // ldrh dest_reg, [templo1, #index]
}

// mov 32bit value from Segs[index] into dest_reg using FC_SEGS_ADDR (index modulo 4 must be zero)
static void gen_mov_seg32_to_reg(HostReg dest_reg,Bitu index) {
	cache_checkinstr(4);
	cache_addw( MOV_LO_HI(templo1, FC_SEGS_ADDR) );      // mov templo1, FC_SEGS_ADDR
	cache_addw( LDR_IMM(dest_reg, templo1, index) );      // ldr dest_reg, [templo1, #index]
}

// add a 32bit value from Segs[index] to a full register using FC_SEGS_ADDR (index modulo 4 must be zero)
static void gen_add_seg32_to_reg(HostReg reg,Bitu index) {
	cache_checkinstr(6);
	cache_addw( MOV_LO_HI(templo1, FC_SEGS_ADDR) );      // mov templo1, FC_SEGS_ADDR
	cache_addw( LDR_IMM(templo2, templo1, index) );      // ldr templo2, [templo1, #index]
	cache_addw( ADD_REG(reg, reg, templo2) );      // add reg, reg, templo2
}

#endif

#ifdef DRC_USE_REGS_ADDR

// mov 16bit value from cpu_regs[index] into dest_reg using FC_REGS_ADDR (index modulo 2 must be zero)
// 16bit moves may destroy the upper 16bit of the destination register
static void gen_mov_regval16_to_reg(HostReg dest_reg,Bitu index) {
	cache_checkinstr(4);
	cache_addw( MOV_LO_HI(templo2, FC_REGS_ADDR) );      // mov templo2, FC_REGS_ADDR
	cache_addw( LDRH_IMM(dest_reg, templo2, index) );      // ldrh dest_reg, [templo2, #index]
}

// mov 32bit value from cpu_regs[index] into dest_reg using FC_REGS_ADDR (index modulo 4 must be zero)
static void gen_mov_regval32_to_reg(HostReg dest_reg,Bitu index) {
	cache_checkinstr(4);
	cache_addw( MOV_LO_HI(templo2, FC_REGS_ADDR) );      // mov templo2, FC_REGS_ADDR
	cache_addw( LDR_IMM(dest_reg, templo2, index) );      // ldr dest_reg, [templo2, #index]
}

// move a 32bit (dword==true) or 16bit (dword==false) value from cpu_regs[index] into dest_reg using FC_REGS_ADDR (if dword==true index modulo 4 must be zero) (if dword==false index modulo 2 must be zero)
// 16bit moves may destroy the upper 16bit of the destination register
static void gen_mov_regword_to_reg(HostReg dest_reg,Bitu index,bool dword) {
	cache_checkinstr(4);
	cache_addw( MOV_LO_HI(templo2, FC_REGS_ADDR) );      // mov templo2, FC_REGS_ADDR
	if (dword) {
		cache_addw( LDR_IMM(dest_reg, templo2, index) );      // ldr dest_reg, [templo2, #index]
	} else {
		cache_addw( LDRH_IMM(dest_reg, templo2, index) );      // ldrh dest_reg, [templo2, #index]
	}
}

// move an 8bit value from cpu_regs[index]  into dest_reg using FC_REGS_ADDR
// the upper 24bit of the destination register can be destroyed
// this function does not use FC_OP1/FC_OP2 as dest_reg as these
// registers might not be directly byte-accessible on some architectures
static void gen_mov_regbyte_to_reg_low(HostReg dest_reg,Bitu index) {
	cache_checkinstr(4);
	cache_addw( MOV_LO_HI(templo2, FC_REGS_ADDR) );      // mov templo2, FC_REGS_ADDR
	cache_addw( LDRB_IMM(dest_reg, templo2, index) );      // ldrb dest_reg, [templo2, #index]
}

// move an 8bit value from cpu_regs[index]  into dest_reg using FC_REGS_ADDR
// the upper 24bit of the destination register can be destroyed
// this function can use FC_OP1/FC_OP2 as dest_reg which are
// not directly byte-accessible on some architectures
static void INLINE gen_mov_regbyte_to_reg_low_canuseword(HostReg dest_reg,Bitu index) {
	cache_checkinstr(4);
	cache_addw( MOV_LO_HI(templo2, FC_REGS_ADDR) );      // mov templo2, FC_REGS_ADDR
	cache_addw( LDRB_IMM(dest_reg, templo2, index) );      // ldrb dest_reg, [templo2, #index]
}


// add a 32bit value from cpu_regs[index] to a full register using FC_REGS_ADDR (index modulo 4 must be zero)
static void gen_add_regval32_to_reg(HostReg reg,Bitu index) {
	cache_checkinstr(6);
	cache_addw( MOV_LO_HI(templo2, FC_REGS_ADDR) );      // mov templo2, FC_REGS_ADDR
	cache_addw( LDR_IMM(templo1, templo2, index) );      // ldr templo1, [templo2, #index]
	cache_addw( ADD_REG(reg, reg, templo1) );      // add reg, reg, templo1
}


// move 16bit of register into cpu_regs[index] using FC_REGS_ADDR (index modulo 2 must be zero)
static void gen_mov_regval16_from_reg(HostReg src_reg,Bitu index) {
	cache_checkinstr(4);
	cache_addw( MOV_LO_HI(templo1, FC_REGS_ADDR) );      // mov templo1, FC_REGS_ADDR
	cache_addw( STRH_IMM(src_reg, templo1, index) );      // strh src_reg, [templo1, #index]
}

// move 32bit of register into cpu_regs[index] using FC_REGS_ADDR (index modulo 4 must be zero)
static void gen_mov_regval32_from_reg(HostReg src_reg,Bitu index) {
	cache_checkinstr(4);
	cache_addw( MOV_LO_HI(templo1, FC_REGS_ADDR) );      // mov templo1, FC_REGS_ADDR
	cache_addw( STR_IMM(src_reg, templo1, index) );      // str src_reg, [templo1, #index]
}

// move 32bit (dword==true) or 16bit (dword==false) of a register into cpu_regs[index] using FC_REGS_ADDR (if dword==true index modulo 4 must be zero) (if dword==false index modulo 2 must be zero)
static void gen_mov_regword_from_reg(HostReg src_reg,Bitu index,bool dword) {
	cache_checkinstr(4);
	cache_addw( MOV_LO_HI(templo1, FC_REGS_ADDR) );      // mov templo1, FC_REGS_ADDR
	if (dword) {
		cache_addw( STR_IMM(src_reg, templo1, index) );      // str src_reg, [templo1, #index]
	} else {
		cache_addw( STRH_IMM(src_reg, templo1, index) );      // strh src_reg, [templo1, #index]
	}
}

// move the lowest 8bit of a register into cpu_regs[index] using FC_REGS_ADDR
static void gen_mov_regbyte_from_reg_low(HostReg src_reg,Bitu index) {
	cache_checkinstr(4);
	cache_addw( MOV_LO_HI(templo1, FC_REGS_ADDR) );      // mov templo1, FC_REGS_ADDR
	cache_addw( STRB_IMM(src_reg, templo1, index) );      // strb src_reg, [templo1, #index]
}

#endif
