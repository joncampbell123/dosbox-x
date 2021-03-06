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



// some configuring defines that specify the capabilities of this architecture
// or aspects of the recompiling

// protect FC_ADDR over function calls if necessaray
// #define DRC_PROTECT_ADDR_REG

// try to use non-flags generating functions if possible
#define DRC_FLAGS_INVALIDATION
// try to replace _simple functions by code
#define DRC_FLAGS_INVALIDATION_DCODE

// type with the same size as a pointer
#define DRC_PTR_SIZE_IM uint32_t

// calling convention modifier
#if defined (WIN32)
#define DRC_CALL_CONV _fastcall
#define DRC_FC /* nothing */
#else
#define DRC_CALL_CONV /* nothing */
#define DRC_FC GCC_ATTRIBUTE(fastcall)
#endif


// register mapping
enum HostReg {
	HOST_EAX=0,
	HOST_ECX,
	HOST_EDX,
	HOST_EBX,
	HOST_ESP,
	HOST_EBP,
	HOST_ESI,
	HOST_EDI
};


// register that holds function return values
#define FC_RETOP HOST_EAX

// register used for address calculations, if the ABI does not
// state that this register is preserved across function calls
// then define DRC_PROTECT_ADDR_REG above
#define FC_ADDR HOST_EBX

// register that holds the first parameter
#define FC_OP1 HOST_ECX

// register that holds the second parameter
#define FC_OP2 HOST_EDX

// special register that holds the third parameter for _R3 calls (byte accessible)
#define FC_OP3 HOST_EAX

// register that holds byte-accessible temporary values
#define FC_TMP_BA1 HOST_ECX

// register that holds byte-accessible temporary values
#define FC_TMP_BA2 HOST_EDX


// temporary register for LEA
#define TEMP_REG_DRC HOST_ESI


// move a full register from reg_src to reg_dst
static void gen_mov_regs(HostReg reg_dst,HostReg reg_src) {
	cache_addb(0x8b);					// mov reg_dst,reg_src
	cache_addb(0xc0+(reg_dst<<3)+reg_src);
}

// move a 32bit (dword==true) or 16bit (dword==false) value from memory into dest_reg
// 16bit moves may destroy the upper 16bit of the destination register
static void gen_mov_word_to_reg(HostReg dest_reg,void* data,bool dword) {
	if (!dword) cache_addb(0x66);
	cache_addw(0x058b+(dest_reg<<11));	// mov reg,[data]
	cache_addd((uint32_t)data);
}

// move a 16bit constant value into dest_reg
// the upper 16bit of the destination register may be destroyed
static void gen_mov_word_to_reg_imm(HostReg dest_reg,uint16_t imm) {
	cache_addb(0x66);
	cache_addb(0xb8+dest_reg);			// mov reg,imm
	cache_addw(imm);
}

// move a 32bit constant value into dest_reg
static void gen_mov_dword_to_reg_imm(HostReg dest_reg,uint32_t imm) {
	cache_addb(0xb8+dest_reg);			// mov reg,imm
	cache_addd(imm);
}

// move 32bit (dword==true) or 16bit (dword==false) of a register into memory
static void gen_mov_word_from_reg(HostReg src_reg,void* dest,bool dword) {
	if (!dword) cache_addb(0x66);
	cache_addw(0x0589+(src_reg<<11));	// mov [data],reg
	cache_addd((uint32_t)dest);
}

// move an 8bit value from memory into dest_reg
// the upper 24bit of the destination register can be destroyed
// this function does not use FC_OP1/FC_OP2 as dest_reg as these
// registers might not be directly byte-accessible on some architectures
static void gen_mov_byte_to_reg_low(HostReg dest_reg,void* data) {
	cache_addw(0x058a+(dest_reg<<11));	// mov reg,[data]
	cache_addd((uint32_t)data);
}

// move an 8bit value from memory into dest_reg
// the upper 24bit of the destination register can be destroyed
// this function can use FC_OP1/FC_OP2 as dest_reg which are
// not directly byte-accessible on some architectures
static void gen_mov_byte_to_reg_low_canuseword(HostReg dest_reg,void* data) {
	cache_addb(0x66);
	cache_addw(0x058b+(dest_reg<<11));	// mov reg,[data]
	cache_addd((uint32_t)data);
}

// move an 8bit constant value into dest_reg
// the upper 24bit of the destination register can be destroyed
// this function does not use FC_OP1/FC_OP2 as dest_reg as these
// registers might not be directly byte-accessible on some architectures
static void gen_mov_byte_to_reg_low_imm(HostReg dest_reg,uint8_t imm) {
	cache_addb(0xb0+dest_reg);			// mov reg,imm
	cache_addb(imm);
}

// move an 8bit constant value into dest_reg
// the upper 24bit of the destination register can be destroyed
// this function can use FC_OP1/FC_OP2 as dest_reg which are
// not directly byte-accessible on some architectures
static void gen_mov_byte_to_reg_low_imm_canuseword(HostReg dest_reg,uint8_t imm) {
	cache_addb(0x66);
	cache_addb(0xb8+dest_reg);			// mov reg,imm
	cache_addw(imm);
}

// move the lowest 8bit of a register into memory
static void gen_mov_byte_from_reg_low(HostReg src_reg,void* dest) {
	cache_addw(0x0588+(src_reg<<11));	// mov [data],reg
	cache_addd((uint32_t)dest);
}



// convert an 8bit word to a 32bit dword
// the register is zero-extended (sign==false) or sign-extended (sign==true)
static void gen_extend_byte(bool sign,HostReg reg) {
	cache_addw(0xb60f+(sign?0x800:0));		// movsx/movzx
	cache_addb(0xc0+(reg<<3)+reg);
}

// convert a 16bit word to a 32bit dword
// the register is zero-extended (sign==false) or sign-extended (sign==true)
static void gen_extend_word(bool sign,HostReg reg) {
	cache_addw(0xb70f+(sign?0x800:0));		// movsx/movzx
	cache_addb(0xc0+(reg<<3)+reg);
}



// add a 32bit value from memory to a full register
static void gen_add(HostReg reg,void* op) {
	cache_addw(0x0503+(reg<<11));		// add reg,[data]
	cache_addd((uint32_t)op);
}

// add a 32bit constant value to a full register
static void gen_add_imm(HostReg reg,uint32_t imm) {
	cache_addw(0xc081+(reg<<8));		// add reg,imm
	cache_addd(imm);
}

// and a 32bit constant value with a full register
static void gen_and_imm(HostReg reg,uint32_t imm) {
	cache_addw(0xe081+(reg<<8));		// and reg,imm
	cache_addd(imm);
}



// move a 32bit constant value into memory
static void gen_mov_direct_dword(void* dest,uint32_t imm) {
	cache_addw(0x05c7);					// mov [data],imm
	cache_addd((uint32_t)dest);
	cache_addd(imm);
}

// move an address into memory
static void INLINE gen_mov_direct_ptr(void* dest,DRC_PTR_SIZE_IM imm) {
	gen_mov_direct_dword(dest,(uint32_t)imm);
}


// add an 8bit constant value to a memory value
static void gen_add_direct_byte(void* dest,int8_t imm) {
	cache_addw(0x0583);					// add [data],imm
	cache_addd((uint32_t)dest);
	cache_addb(imm);
}

// add a 32bit (dword==true) or 16bit (dword==false) constant value to a memory value
static void gen_add_direct_word(void* dest,uint32_t imm,bool dword) {
	if ((imm<128) && dword) {
		gen_add_direct_byte(dest,(int8_t)imm);
		return;
	}
	if (!dword) cache_addb(0x66);
	cache_addw(0x0581);					// add [data],imm
	cache_addd((uint32_t)dest);
	if (dword) cache_addd((uint32_t)imm);
	else cache_addw((uint16_t)imm);
}

// subtract an 8bit constant value from a memory value
static void gen_sub_direct_byte(void* dest,int8_t imm) {
	cache_addw(0x2d83);					// sub [data],imm
	cache_addd((uint32_t)dest);
	cache_addb(imm);
}

// subtract a 32bit (dword==true) or 16bit (dword==false) constant value from a memory value
static void gen_sub_direct_word(void* dest,uint32_t imm,bool dword) {
	if ((imm<128) && dword) {
		gen_sub_direct_byte(dest,(int8_t)imm);
		return;
	}
	if (!dword) cache_addb(0x66);
	cache_addw(0x2d81);					// sub [data],imm
	cache_addd((uint32_t)dest);
	if (dword) cache_addd((uint32_t)imm);
	else cache_addw((uint16_t)imm);
}



// effective address calculation, destination is dest_reg
// scale_reg is scaled by scale (scale_reg*(2^scale)) and
// added to dest_reg, then the immediate value is added
static INLINE void gen_lea(HostReg dest_reg,HostReg scale_reg,Bitu scale,Bits imm) {
	uint8_t rm_base;
	Bitu imm_size;
	if (!imm) {
		imm_size=0;	rm_base=0x0;			//no imm
	} else if ((imm>=-128 && imm<=127)) {
		imm_size=1;	rm_base=0x40;			//Signed byte imm
	} else {
		imm_size=4;	rm_base=0x80;			//Signed dword imm
	}

	// ea_reg := ea_reg+scale_reg*(2^scale)+imm
	cache_addb(0x8d);			//LEA
	cache_addb(0x04+(dest_reg << 3)+rm_base);	//The sib indicator
	cache_addb(dest_reg+(scale_reg<<3)+(scale<<6));

	switch (imm_size) {
	case 0:	break;
	case 1:cache_addb(imm);break;
	case 4:cache_addd(imm);break;
	}
}

// effective address calculation, destination is dest_reg
// dest_reg is scaled by scale (dest_reg*(2^scale)),
// then the immediate value is added
static INLINE void gen_lea(HostReg dest_reg,Bitu scale,Bits imm) {
	// ea_reg := ea_reg*(2^scale)+imm
	// ea_reg :=   op2 *(2^scale)+imm
	cache_addb(0x8d);			//LEA
	cache_addb(0x04+(dest_reg<<3));
	cache_addb(0x05+(dest_reg<<3)+(scale<<6));

	cache_addd(imm);		// always add dword immediate
}



// generate a call to a parameterless function
template <typename T> static void INLINE gen_call_function_raw(const T func) {
    cache_addb(0xe8);
	cache_addd((uint32_t)func - (uint32_t)cache_rwtox(cache.pos)-4);
}

// generate a call to a function with paramcount parameters
// note: the parameters are loaded in the architecture specific way
// using the gen_load_param_ functions below
template <typename T> static uint32_t INLINE gen_call_function_setup(const T func,Bitu paramcount,bool fastcall=false) {
    uint32_t proc_addr=(uint32_t)cache.pos;
	// Do the actual call to the procedure
	cache_addb(0xe8);
	cache_addd((uint32_t)func - (uint32_t)cache_rwtox(cache.pos)-4);

	// Restore the params of the stack
	if (paramcount) {
		cache_addw(0xc483);				//add ESP,imm byte
		cache_addb((!fastcall)?paramcount*4:0);
	}
	return proc_addr;
}


// load an immediate value as param'th function parameter
static void INLINE gen_load_param_imm(Bitu imm,Bitu param) {
    (void)param;
	cache_addb(0x68);			// push immediate
	cache_addd(imm);
}

// load an address as param'th function parameter
static void INLINE gen_load_param_addr(Bitu addr,Bitu param) {
    (void)param;
	cache_addb(0x68);			// push immediate (address)
	cache_addd(addr);
}

// load a host-register as param'th function parameter
static void INLINE gen_load_param_reg(Bitu reg,Bitu param) {
    (void)param;
	cache_addb(0x50+(reg&7));	// push reg
}

// load a value from memory as param'th function parameter
static void INLINE gen_load_param_mem(Bitu mem,Bitu param) {
    (void)param;
	cache_addw(0x35ff);			// push []
	cache_addd(mem);
}



// jump to an address pointed at by ptr, offset is in imm
static void gen_jmp_ptr(void * ptr,Bits imm=0) {
	gen_mov_word_to_reg(HOST_EAX,ptr,true);
	cache_addb(0xff);		// jmp [eax+imm]
	if (!imm) {
		cache_addb(0x20);
    } else if ((imm>=-128 && imm<=127)) {
		cache_addb(0x60);
		cache_addb(imm);
	} else {
		cache_addb(0xa0);
		cache_addd(imm);
	}
}


// short conditional jump (+-127 bytes) if register is zero
// the destination is set by gen_fill_branch() later
static uint32_t gen_create_branch_on_zero(HostReg reg,bool dword) {
	if (!dword) cache_addb(0x66);
	cache_addb(0x0b);					// or reg,reg
	cache_addb(0xc0+reg+(reg<<3));

	cache_addw(0x0074);					// jz addr
	return ((uint32_t)cache.pos-1);
}

// short conditional jump (+-127 bytes) if register is nonzero
// the destination is set by gen_fill_branch() later
static uint32_t gen_create_branch_on_nonzero(HostReg reg,bool dword) {
	if (!dword) cache_addb(0x66);
	cache_addb(0x0b);					// or reg,reg
	cache_addb(0xc0+reg+(reg<<3));

	cache_addw(0x0075);					// jnz addr
	return ((uint32_t)cache.pos-1);
}

// calculate relative offset and fill it into the location pointed to by data
static void gen_fill_branch(DRC_PTR_SIZE_IM data) {
#if C_DEBUG
	Bits len=(uint32_t)cache.pos-data;
	if (len<0) len=-len;
	if (len>126) LOG_MSG("Big jump %d",len);
#endif
	*(uint8_t*)data=(uint8_t)((uint32_t)cache.pos-data-1);
}

// conditional jump if register is nonzero
// for isdword==true the 32bit of the register are tested
// for isdword==false the lowest 8bit of the register are tested
static uint32_t gen_create_branch_long_nonzero(HostReg reg,bool isdword) {
	// isdword: cmp reg32,0
	// not isdword: cmp reg8,0
	cache_addb(0x0a+(isdword?1:0));				// or reg,reg
	cache_addb(0xc0+reg+(reg<<3));

	cache_addw(0x850f);		// jnz
	cache_addd(0);
	return ((uint32_t)cache.pos-4);
}

// compare 32bit-register against zero and jump if value less/equal than zero
static uint32_t gen_create_branch_long_leqzero(HostReg reg) {
	cache_addw(0xf883+(reg<<8));
	cache_addb(0x00);		// cmp reg,0

	cache_addw(0x8e0f);		// jle
	cache_addd(0);
	return ((uint32_t)cache.pos-4);
}

// calculate long relative offset and fill it into the location pointed to by data
static void gen_fill_branch_long(uint32_t data) {
	*(uint32_t*)data=((uint32_t)cache.pos-data-4);
}


static void gen_run_code(void) {
	cache_addd(0x0424448b);		// mov eax,[esp+4]
	cache_addb(0x53);			// push ebx
	cache_addb(0x56);			// push esi
	cache_addw(0xd0ff);			// call eax
	cache_addb(0x5e);			// pop  esi
	cache_addb(0x5b);			// pop  ebx
}

// return from a function
static void gen_return_function(void) {
	cache_addb(0xc3);		// ret
}

#ifdef DRC_FLAGS_INVALIDATION
// called when a call to a function can be replaced by a
// call to a simpler function
static void gen_fill_function_ptr(uint8_t * pos,void* fct_ptr,Bitu flags_type) {
#ifdef DRC_FLAGS_INVALIDATION_DCODE
	// try to avoid function calls but rather directly fill in code
	switch (flags_type) {
		case t_ADDb:
		case t_ADDw:
		case t_ADDd:
			*(uint32_t*)pos=0xc203c18b;	// mov eax,ecx; add eax,edx
			*(pos+4)=0x90;
			break;
		case t_ORb:
		case t_ORw:
		case t_ORd:
			*(uint32_t*)pos=0xc20bc18b;	// mov eax,ecx; or eax,edx
			*(pos+4)=0x90;
			break;
		case t_ANDb:
		case t_ANDw:
		case t_ANDd:
			*(uint32_t*)pos=0xc223c18b;	// mov eax,ecx; and eax,edx
			*(pos+4)=0x90;
			break;
		case t_SUBb:
		case t_SUBw:
		case t_SUBd:
			*(uint32_t*)pos=0xc22bc18b;	// mov eax,ecx; sub eax,edx
			*(pos+4)=0x90;
			break;
		case t_XORb:
		case t_XORw:
		case t_XORd:
			*(uint32_t*)pos=0xc233c18b;	// mov eax,ecx; xor eax,edx
			*(pos+4)=0x90;
			break;
		case t_CMPb:
		case t_CMPw:
		case t_CMPd:
		case t_TESTb:
		case t_TESTw:
		case t_TESTd:
			*(uint32_t*)pos=0x909003eb;	// skip
			*(pos+4)=0x90;
			break;
		case t_INCb:
		case t_INCw:
		case t_INCd:
			*(uint32_t*)pos=0x9040c18b;	// mov eax,ecx; inc eax
			*(pos+4)=0x90;
			break;
		case t_DECb:
		case t_DECw:
		case t_DECd:
			*(uint32_t*)pos=0x9048c18b;	// mov eax,ecx; dec eax
			*(pos+4)=0x90;
			break;
		case t_NEGb:
		case t_NEGw:
		case t_NEGd:
			*(uint32_t*)pos=0xd8f7c18b;	// mov eax,ecx; neg eax
			*(pos+4)=0x90;
			break;
		default:
			*(uint32_t*)(pos+1)=(uint32_t)((uint8_t*)fct_ptr - ((uint8_t*)cache_rwtox(pos)+1+4));	// fill function pointer
			break;
	}
#else
	*(uint32_t*)(pos+1)=(uint32_t)((uint8_t*)fct_ptr - ((uint8_t*)cache_rwtox(pos)+1+4));	// fill function pointer
#endif
}
#endif

static void cache_block_closing(uint8_t* block_start,Bitu block_size) { (void)block_start; (void)block_size; }

static void cache_block_before_close(void) { }
