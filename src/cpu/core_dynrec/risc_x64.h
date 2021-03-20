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

// calling convention modifier
#define DRC_CALL_CONV	/* nothing */
#define DRC_FC			/* nothing */


// register mapping
typedef uint8_t HostReg;

#define HOST_EAX 0
#define HOST_ECX 1
#define HOST_EDX 2
#define HOST_EBX 3
#define HOST_ESI 6
#define HOST_EDI 7


// register that holds function return values
#define FC_RETOP HOST_EAX

// register used for address calculations, if the ABI does not
// state that this register is preserved across function calls
// then define DRC_PROTECT_ADDR_REG above
#define FC_ADDR HOST_EBX

#if defined (_WIN64)
#define FC_OP1 HOST_ECX
#define FC_OP2 HOST_EDX
#else
// register that holds the first parameter
#define FC_OP1 HOST_EDI

// register that holds the second parameter
#define FC_OP2 HOST_ESI
#endif

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
	if (reg_dst==reg_src) return;
	cache_addb(0x8b);					// mov reg_dst,reg_src
	cache_addb(0xc0+(reg_dst<<3)+reg_src);
}

static void gen_mov_reg_qword(HostReg dest_reg,uint64_t imm);

// This function generates an instruction with register addressing and a memory location
static INLINE void gen_reg_memaddr(HostReg reg,void* data,uint8_t op,uint8_t prefix=0) {
	int64_t diff = (int64_t)data-((int64_t)cache_rwtox(cache.pos+(prefix?7:6)));
//	if ((diff<0x80000000LL) && (diff>-0x80000000LL)) { //clang messes itself up on this...
	if ( (diff>>63) == (diff>>31) ) { //signed bit extend, test to see if value fits in a int32_t
		// mov reg,[rip+diff] (or similar, depending on the op) to fetch *data
		if(prefix) cache_addb(prefix);
		cache_addb(op);
		cache_addb(0x05+(reg<<3));
		// RIP-relative addressing is offset after the instruction 
		cache_addd((uint32_t)(((uint64_t)diff)&0xffffffffLL)); 
	} else if ((uint64_t)data<0x100000000LL) {
		// mov reg,[data] (or similar, depending on the op) when absolute address of data is <4GB
		if(prefix) cache_addb(prefix);
		cache_addb(op);
		cache_addw(0x2504+(reg<<3));
		cache_addd((uint32_t)(((uint64_t)data)&0xffffffffLL));
	} else {
		// load 64-bit data into tmp_reg and do mov reg,[tmp_reg] (or similar, depending on the op)
		HostReg tmp_reg = HOST_EAX;
		if(reg == HOST_EAX) tmp_reg = HOST_ECX;

		cache_addb(0x50+tmp_reg);	// push rax/rcx
		gen_mov_reg_qword(tmp_reg,(uint64_t)data);

		if(prefix) cache_addb(prefix);
		cache_addb(op);
		cache_addb(tmp_reg+(reg<<3));

		cache_addb(0x58+tmp_reg);	// pop rax/rcx
	}
}

// Same as above, but with immediate addressing and a memory location
static INLINE void gen_memaddr(uint8_t modreg,void* data,Bitu off,Bitu imm,uint8_t op,uint8_t prefix=0) {
	int64_t diff = (int64_t)data-((int64_t)cache.pos+off+(prefix?7:6));
//	if ((diff<0x80000000LL) && (diff>-0x80000000LL)) {
	if ( (diff>>63) == (diff>>31) ) {
		// RIP-relative addressing is offset after the instruction 
		if(prefix) cache_addb(prefix);
		cache_addw(op+((modreg+1)<<8));
		cache_addd((uint32_t)(((uint64_t)diff)&0xffffffffLL));

		switch(off) {
			case 1: cache_addb(((uint8_t)imm&0xff)); break;
			case 2: cache_addw(((uint16_t)imm&0xffff)); break;
			case 4: cache_addd(((uint32_t)imm&0xffffffff)); break;
		}

	} else if ((uint64_t)data<0x100000000LL) {
		if(prefix) cache_addb(prefix);
		cache_addw(op+(modreg<<8));
		cache_addb(0x25);
		cache_addd((uint32_t)(((uint64_t)data)&0xffffffffLL));

		switch(off) {
			case 1: cache_addb(((uint8_t)imm&0xff)); break;
			case 2: cache_addw(((uint16_t)imm&0xffff)); break;
			case 4: cache_addd(((uint32_t)imm&0xffffffff)); break;
		}

	} else {
		HostReg tmp_reg = HOST_EAX;

		cache_addb(0x50+tmp_reg);	// push rax
		gen_mov_reg_qword(tmp_reg,(uint64_t)data);

		if(prefix) cache_addb(prefix);
		cache_addw(op+((modreg-4+tmp_reg)<<8));

		switch(off) {
			case 1: cache_addb(((uint8_t)imm&0xff)); break;
			case 2: cache_addw(((uint16_t)imm&0xffff)); break;
			case 4: cache_addd(((uint32_t)imm&0xffffffff)); break;
		}

		cache_addb(0x58+tmp_reg);	// pop rax
	}
}

// move a 32bit (dword==true) or 16bit (dword==false) value from memory into dest_reg
// 16bit moves may destroy the upper 16bit of the destination register
static void gen_mov_word_to_reg(HostReg dest_reg,void* data,bool dword,uint8_t prefix=0) {
	if (!dword) gen_reg_memaddr(dest_reg,data,0xb7,0x0f);	// movzx reg,[data] - zero extend data, fixes LLVM compile where the called function does not extend the parameters
	else gen_reg_memaddr(dest_reg,data,0x8b,prefix);	// mov reg,[data]
} 

// move a 16bit constant value into dest_reg
// the upper 16bit of the destination register may be destroyed
static void gen_mov_word_to_reg_imm(HostReg dest_reg,uint16_t imm) {
	cache_addb(0xb8+dest_reg);			// mov reg,imm
	cache_addd((uint32_t)imm);
}

// move a 32bit constant value into dest_reg
static void gen_mov_dword_to_reg_imm(HostReg dest_reg,uint32_t imm) {
	cache_addb(0xb8+dest_reg);			// mov reg,imm
	cache_addd(imm);
}

// move a 64bit constant value into a full register
static void gen_mov_reg_qword(HostReg dest_reg,uint64_t imm) {
	if (imm==(uint32_t)imm) {
		gen_mov_dword_to_reg_imm(dest_reg, (uint32_t)imm);
		return;
	}
	cache_addb(0x48);
	cache_addb(0xb8+dest_reg);			// mov dest_reg,imm
	cache_addq(imm);
}

// move 32bit (dword==true) or 16bit (dword==false) of a register into memory
static void gen_mov_word_from_reg(HostReg src_reg,void* dest,bool dword,uint8_t prefix=0) {
	gen_reg_memaddr(src_reg,dest,0x89,(dword?prefix:0x66));		// mov [data],reg
}

// move an 8bit value from memory into dest_reg
// the upper 24bit of the destination register can be destroyed
// this function does not use FC_OP1/FC_OP2 as dest_reg as these
// registers might not be directly byte-accessible on some architectures
static void gen_mov_byte_to_reg_low(HostReg dest_reg,void* data) {
	gen_reg_memaddr(dest_reg,data,0xb6,0x0f);	// movzx reg,[data]
}

// move an 8bit value from memory into dest_reg
// the upper 24bit of the destination register can be destroyed
// this function can use FC_OP1/FC_OP2 as dest_reg which are
// not directly byte-accessible on some architectures
static void gen_mov_byte_to_reg_low_canuseword(HostReg dest_reg,void* data) {
	gen_reg_memaddr(dest_reg,data,0xb6,0x0f);	// movzx reg,[data]
}

// move an 8bit constant value into dest_reg
// the upper 24bit of the destination register can be destroyed
// this function does not use FC_OP1/FC_OP2 as dest_reg as these
// registers might not be directly byte-accessible on some architectures
static void gen_mov_byte_to_reg_low_imm(HostReg dest_reg,uint8_t imm) {
	cache_addb(0xb8+dest_reg);			// mov reg,imm
	cache_addd((uint32_t)imm);
}

// move an 8bit constant value into dest_reg
// the upper 24bit of the destination register can be destroyed
// this function can use FC_OP1/FC_OP2 as dest_reg which are
// not directly byte-accessible on some architectures
static void gen_mov_byte_to_reg_low_imm_canuseword(HostReg dest_reg,uint8_t imm) {
	cache_addb(0xb8+dest_reg);			// mov reg,imm
	cache_addd((uint32_t)imm);
}

// move the lowest 8bit of a register into memory
static void gen_mov_byte_from_reg_low(HostReg src_reg,void* dest) {
	gen_reg_memaddr(src_reg,dest,0x88);	// mov byte [data],reg
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
	gen_reg_memaddr(reg,op,0x03);		// add reg,[data]
}

// add a 32bit constant value to a full register
static void gen_add_imm(HostReg reg,uint32_t imm) {
	if (!imm) return;
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
	gen_memaddr(0x4,dest,4,imm,0xc7);	// mov [data],imm
}


// move an address into memory
static void INLINE gen_mov_direct_ptr(void* dest,Bitu imm) {
	gen_mov_reg_qword(HOST_EAX,imm);
	gen_mov_word_from_reg(HOST_EAX,dest,true,0x48);		// 0x48 prefixes full 64-bit mov
}


// add an 8bit constant value to a memory value
static void gen_add_direct_byte(void* dest,int8_t imm) {
	if (!imm) return;
	gen_memaddr(0x4,dest,1,imm,0x83);	// add [data],imm
}

// add a 32bit (dword==true) or 16bit (dword==false) constant value to a memory value
static void gen_add_direct_word(void* dest,uint32_t imm,bool dword) {
	if (!imm) return;
	if ((imm<128) && dword) {
		gen_add_direct_byte(dest,(int8_t)imm);
		return;
	}
	gen_memaddr(0x4,dest,(dword?4:2),imm,0x81,(dword?0:0x66));	// add [data],imm
}

// subtract an 8bit constant value from a memory value
static void gen_sub_direct_byte(void* dest,int8_t imm) {
	if (!imm) return;
	gen_memaddr(0x2c,dest,1,imm,0x83);
}

// subtract a 32bit (dword==true) or 16bit (dword==false) constant value from a memory value
static void gen_sub_direct_word(void* dest,uint32_t imm,bool dword) {
	if (!imm) return;
	if ((imm<128) && dword) {
		gen_sub_direct_byte(dest,(int8_t)imm);
		return;
	}
	gen_memaddr(0x2c,dest,(dword?4:2),imm,0x81,(dword?0:0x66));	// sub [data],imm
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
	cache_addb(0x48);
	cache_addb(0x8d);			//LEA
	cache_addb(0x04+(dest_reg << 3)+rm_base);	//The sib indicator
	cache_addb((uint8_t)(dest_reg+(scale_reg<<3)+(scale<<6)));

	switch (imm_size) {
	case 0:	break;
	case 1:cache_addb((uint8_t)imm);break;
	case 4:cache_addd((uint32_t)imm);break;
	}
}

// effective address calculation, destination is dest_reg
// dest_reg is scaled by scale (dest_reg*(2^scale)),
// then the immediate value is added
static INLINE void gen_lea(HostReg dest_reg,Bitu scale,Bits imm) {
	// ea_reg := ea_reg*(2^scale)+imm
	// ea_reg :=   op2 *(2^scale)+imm
	cache_addb(0x48);
	cache_addb(0x8d);			//LEA
	cache_addb(0x04+(dest_reg<<3));
	cache_addb((uint8_t)(0x05+(dest_reg<<3)+(scale<<6)));

	cache_addd((uint32_t)imm);		// always add dword immediate
}



// generate a call to a parameterless function
template <typename T> static void INLINE gen_call_function_raw(const T func) {
	cache_addw(0xb848);
	cache_addq((uint64_t)func);
	cache_addw(0xd0ff);
}

// generate a call to a function with paramcount parameters
// note: the parameters are loaded in the architecture specific way
// using the gen_load_param_ functions below
template <typename T> static INLINE const uint8_t* gen_call_function_setup(const T func,Bitu paramcount,bool fastcall=false) {
	(void)paramcount;
	(void)fastcall;

	const uint8_t* proc_addr = cache.pos;
	gen_call_function_raw(func);
	return proc_addr;
}


// load an immediate value as param'th function parameter
static void INLINE gen_load_param_imm(Bitu imm,Bitu param) {
	// move an immediate 32bit value into a 64bit param reg
	switch (param) {
		case 0:			// mov param1,imm32
			gen_mov_dword_to_reg_imm(FC_OP1,(uint32_t)imm);
			break;
		case 1:			// mov param2,imm32
			gen_mov_dword_to_reg_imm(FC_OP2,(uint32_t)imm);
			break;
#if defined (_WIN64)
		case 2:			// mov r8d,imm32
			cache_addw(0xb841);
			cache_addd((uint32_t)imm);
			break;
		case 3:			// mov r9d,imm32
			cache_addw(0xb941);
			cache_addd((uint32_t)imm);
			break;
#else
		case 2:			// mov rdx,imm32
			gen_mov_dword_to_reg_imm(HOST_EDX,(uint32_t)imm);
			break;
		case 3:			// mov rcx,imm32
			gen_mov_dword_to_reg_imm(HOST_ECX,(uint32_t)imm);
			break;
#endif
		default:
			E_Exit("I(mm) >4 params unsupported");
			break;
	}
}

// load an address as param'th function parameter
static void INLINE gen_load_param_addr(Bitu addr,Bitu param) {
	// move an immediate 64bit value into a 64bit param reg
	switch (param) {
		case 0:			// mov param1,addr64
			gen_mov_reg_qword(FC_OP1,addr);
			break;
		case 1:			// mov param2,addr64
			gen_mov_reg_qword(FC_OP2,addr);
			break;
#if defined (_WIN64)
		case 2:			// mov r8,addr64
			cache_addw(0xb849);
			cache_addq(addr);
			break;
		case 3:			// mov r9,addr64
			cache_addw(0xb949);
			cache_addq(addr);
			break;
#else
		case 2:			// mov rdx,addr64
			gen_mov_reg_qword(HOST_EDX,addr);
			break;
		case 3:			// mov rcx,addr64
			gen_mov_reg_qword(HOST_ECX,addr);
			break;
#endif
		default:
			E_Exit("A(ddr) >4 params unsupported");
			break;
	}
}

// load a host-register as param'th function parameter
static void INLINE gen_load_param_reg(Bitu reg,Bitu param) {
	// move a register into a 64bit param reg, {inputregs}!={outputregs}
	switch (param) {
		case 0:		// mov param1,reg&7
			gen_mov_regs(FC_OP1,reg&7);
			break;
		case 1:		// mov param2,reg&7
			gen_mov_regs(FC_OP2,reg&7);
			break;
#if defined (_WIN64)
		case 2:		// mov r8,reg&7
			cache_addw(0x8949);
			cache_addb(0xc0 + ((reg & 7) << 3));
			break;
		case 3:		// mov r9,reg&7
			cache_addw(0x8949);
			cache_addb(0xc1 + ((reg & 7) << 3));
			break;
#else
		case 2:		// mov rdx,reg&7
			gen_mov_regs(HOST_EDX,reg&7);
			break;
		case 3:		// mov rcx,reg&7
			gen_mov_regs(HOST_ECX,reg&7);
			break;
#endif
		default:
			E_Exit("R(eg) >4 params unsupported");
			break;
	}
}

// load a value from memory as param'th function parameter
static void INLINE gen_load_param_mem(Bitu mem,Bitu param) {
	// move memory content into a 64bit param reg
	switch (param) {
		case 0:		// mov param1,[mem]
			gen_mov_word_to_reg(FC_OP1,(void*)mem,true);
			break;
		case 1:		// mov param2,[mem]
			gen_mov_word_to_reg(FC_OP2,(void*)mem,true);
			break;
#if defined (_WIN64)
		case 2:		// mov r8d,[mem]
			gen_mov_word_to_reg(0,(void*)mem,true,0x44);	// 0x44, use x64 rXd regs
			break;
		case 3:		// mov r9d,[mem]
			gen_mov_word_to_reg(1,(void*)mem,true,0x44);	// 0x44, use x64 rXd regs
			break;
#else
		case 2:		// mov edx,[mem]
			gen_mov_word_to_reg(HOST_EDX,(void*)mem,true);
			break;
		case 3:		// mov ecx,[mem]
			gen_mov_word_to_reg(HOST_ECX,(void*)mem,true);
			break;
#endif
		default:
			E_Exit("R(eg) >4 params unsupported");
			break;
	}
}



// jump to an address pointed at by ptr, offset is in imm
static void gen_jmp_ptr(void * ptr,Bits imm=0) {
	cache_addw(0xa148);		// mov rax,[data]
	cache_addq((uint64_t)ptr);

	cache_addb(0xff);		// jmp [rax+imm]
	if (!imm) {
		cache_addb(0x20);
    } else if ((imm>=-128 && imm<=127)) {
		cache_addb(0x60);
		cache_addb((uint8_t)imm);
	} else {
		cache_addb(0xa0);
		cache_addd((uint32_t)imm);
	}
}


// short conditional jump (+-127 bytes) if register is zero
// the destination is set by gen_fill_branch() later
static const uint8_t* gen_create_branch_on_zero(HostReg reg,bool dword) {
	if (!dword) cache_addb(0x66);
	cache_addb(0x0b);					// or reg,reg
	cache_addb(0xc0+reg+(reg<<3));

	cache_addw(0x0074);					// jz addr
	return (cache.pos-1);
}

// short conditional jump (+-127 bytes) if register is nonzero
// the destination is set by gen_fill_branch() later
static const uint8_t* gen_create_branch_on_nonzero(HostReg reg,bool dword) {
	if (!dword) cache_addb(0x66);
	cache_addb(0x0b);					// or reg,reg
	cache_addb(0xc0+reg+(reg<<3));

	cache_addw(0x0075);					// jnz addr
	return (cache.pos-1);
}

// calculate relative offset and fill it into the location pointed to by data
static void gen_fill_branch(const uint8_t* data) {
#if C_DEBUG
	int64_t len=cache.pos-data;
	if (len<0) len=-len;
	if (len>126) LOG_MSG("Big jump %d",(int)len);
#endif
	cache_addb((uint8_t)(cache.pos-data-1),data);
}

// conditional jump if register is nonzero
// for isdword==true the 32bit of the register are tested
// for isdword==false the lowest 8bit of the register are tested
static const uint8_t* gen_create_branch_long_nonzero(HostReg reg,bool isdword) {
	// isdword: cmp reg32,0
	// not isdword: cmp reg8,0
	cache_addb(0x0a+(isdword?1:0));				// or reg,reg
	cache_addb(0xc0+reg+(reg<<3));

	cache_addw(0x850f);		// jnz
	cache_addd(0);
	return (cache.pos-4);
}

// compare 32bit-register against zero and jump if value less/equal than zero
static const uint8_t* gen_create_branch_long_leqzero(HostReg reg) {
	cache_addw(0xf883+(reg<<8));
	cache_addb(0x00);		// cmp reg,0

	cache_addw(0x8e0f);		// jle
	cache_addd(0);
	return (cache.pos-4);
}

// calculate long relative offset and fill it into the location pointed to by data
static void gen_fill_branch_long(const uint8_t* data) {
	cache_addd((uint32_t)(cache.pos-data-4),data);
}

static void gen_run_code(void) {
	cache_addw(0x5355);     // push rbp,rbx
	cache_addb(0x56);       // push rsi
	cache_addd(0x20EC8348); // sub rsp, 32
	cache_addb(0x48);cache_addw(0x2D8D);cache_addd(2); // lea rbp, [rip+2]
	cache_addw(0xE0FF+(FC_OP1<<8)); // jmp FC_OP1
	cache_addd(0x20C48348); // add rsp, 32
	cache_addd(0xC35D5B5E); // pop rsi,rbx,rbp;ret
}

// return from a function
static void gen_return_function(void) {
	cache_addw(0xE5FF); // jmp rbp
}

#ifdef DRC_FLAGS_INVALIDATION
// called when a call to a function can be replaced by a
// call to a simpler function
// check gen_call_function_raw and gen_call_function_setup
// for the targeted code
static void gen_fill_function_ptr(const uint8_t * pos,void* fct_ptr,Bitu flags_type) {
#ifdef DRC_FLAGS_INVALIDATION_DCODE
	// try to avoid function calls but rather directly fill in code
	switch (flags_type) {
		case t_ADDb:
		case t_ADDw:
		case t_ADDd:
			// mov eax,FC_OP1; add eax,FC_OP2
			cache_addd(0xC001c089+(FC_OP1<<11)+(FC_OP2<<27),pos);
			cache_addw(0x06eb,pos+4);       // skip
			return;
		case t_ORb:
		case t_ORw:
		case t_ORd:
			// mov eax,FC_OP1; or eax,FC_OP2
			cache_addd(0xc009c089+(FC_OP1<<11)+(FC_OP2<<27),pos);
			cache_addw(0x06eb,pos+4);       // skip
			return;
		case t_ANDb:
		case t_ANDw:
		case t_ANDd:
			// mov eax,FC_OP1; and eax,FC_OP2
			cache_addd(0xc021c089+(FC_OP1<<11)+(FC_OP2<<27),pos);
			cache_addw(0x06eb,pos+4);       // skip
			return;
		case t_SUBb:
		case t_SUBw:
		case t_SUBd:
			// mov eax,FC_OP1; sub eax,FC_OP2
			cache_addd(0xc029c089+(FC_OP1<<11)+(FC_OP2<<27),pos);
			cache_addw(0x06eb,pos+4);       // skip
			return;
		case t_XORb:
		case t_XORw:
		case t_XORd:
			// mov eax,FC_OP1; xor eax,FC_OP2
			cache_addd(0xc031c089+(FC_OP1<<11)+(FC_OP2<<27),pos);
			cache_addw(0x06eb,pos+4);       // skip
			return;
		case t_CMPb:
		case t_CMPw:
		case t_CMPd:
		case t_TESTb:
		case t_TESTw:
		case t_TESTd:
			cache_addw(0x0aeb,pos);         // skip
			return;
		case t_INCb:
		case t_INCw:
		case t_INCd:
			// mov eax,FC_OP1; inc eax
			cache_addd(0xc0ffc089+(FC_OP1<<11),pos);
			cache_addw(0x06eb,pos+4);       // skip
			return;
		case t_DECb:
		case t_DECw:
		case t_DECd:
			// mov eax,FC_OP1; dec eax
			cache_addd(0xc8ffc089+(FC_OP1<<11),pos);
			cache_addw(0x06eb,pos+4);       // skip
			return;
		case t_NEGb:
		case t_NEGw:
		case t_NEGd:
			// mov eax,FC_OP1; neg eax
			cache_addd(0xd8f7c089+(FC_OP1<<11),pos);
			cache_addw(0x06eb,pos+4);       // skip
			return;
	}
#endif
	cache_addq((uint64_t)fct_ptr,pos+2);      // fill function pointer
}
#endif

static void cache_block_closing(const uint8_t* block_start,Bitu block_size) {
	(void)block_start;
	(void)block_size;
}

static void cache_block_before_close(void) { }
