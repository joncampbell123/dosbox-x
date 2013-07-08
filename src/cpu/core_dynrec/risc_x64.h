/*
 *  Copyright (C) 2002-2010  The DOSBox Team
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

/* $Id: risc_x64.h,v 1.13 2009-06-25 19:31:43 c2woody Exp $ */


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


// register mapping
typedef Bit8u HostReg;

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

// register that holds the first parameter
#define FC_OP1 HOST_EDI

// register that holds the second parameter
#define FC_OP2 HOST_ESI

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


static INLINE void gen_memaddr(HostReg reg,void* data) {
	Bit64s diff = (Bit64s)data-((Bit64s)cache.pos+5);
	if ((diff<0x80000000LL) && (diff>-0x80000000LL)) {
		cache_addb(0x05+(reg<<3));
		// RIP-relative addressing is offset after the instruction 
		cache_addd((Bit32u)(((Bit64u)diff)&0xffffffffLL)); 
	} else if ((Bit64u)data<0x100000000LL) {
		cache_addw(0x2504+(reg<<3));
		cache_addd((Bit32u)(((Bit64u)data)&0xffffffffLL));
	} else {
		E_Exit("DRC64:Unhandled memory reference");
	}
}


// move a 32bit (dword==true) or 16bit (dword==false) value from memory into dest_reg
// 16bit moves may destroy the upper 16bit of the destination register
static void gen_mov_word_to_reg(HostReg dest_reg,void* data,bool dword) {
	if (!dword) cache_addb(0x66);
	cache_addb(0x8b); // mov reg,[data]
	gen_memaddr(dest_reg,data);
} 

// move a 16bit constant value into dest_reg
// the upper 16bit of the destination register may be destroyed
static void gen_mov_word_to_reg_imm(HostReg dest_reg,Bit16u imm) {
	cache_addb(0x66);
	cache_addb(0xb8+dest_reg);			// mov reg,imm
	cache_addw(imm);
}

// move a 32bit constant value into dest_reg
static void gen_mov_dword_to_reg_imm(HostReg dest_reg,Bit32u imm) {
	cache_addb(0xb8+dest_reg);			// mov reg,imm
	cache_addd(imm);
}

// move 32bit (dword==true) or 16bit (dword==false) of a register into memory
static void gen_mov_word_from_reg(HostReg src_reg,void* dest,bool dword) {
	if (!dword) cache_addb(0x66);
	cache_addb(0x89);	// mov [data],reg
	gen_memaddr(src_reg,dest);
}

// move an 8bit value from memory into dest_reg
// the upper 24bit of the destination register can be destroyed
// this function does not use FC_OP1/FC_OP2 as dest_reg as these
// registers might not be directly byte-accessible on some architectures
static void gen_mov_byte_to_reg_low(HostReg dest_reg,void* data) {
	cache_addb(0x8a);	// mov reg,[data]
	gen_memaddr(dest_reg,data);
}

// move an 8bit value from memory into dest_reg
// the upper 24bit of the destination register can be destroyed
// this function can use FC_OP1/FC_OP2 as dest_reg which are
// not directly byte-accessible on some architectures
static void gen_mov_byte_to_reg_low_canuseword(HostReg dest_reg,void* data) {
	cache_addb(0x66);
	cache_addb(0x8b);	// mov reg,[data]
	gen_memaddr(dest_reg,data);
}

// move an 8bit constant value into dest_reg
// the upper 24bit of the destination register can be destroyed
// this function does not use FC_OP1/FC_OP2 as dest_reg as these
// registers might not be directly byte-accessible on some architectures
static void gen_mov_byte_to_reg_low_imm(HostReg dest_reg,Bit8u imm) {
	cache_addb(0xb0+dest_reg);			// mov reg,imm
	cache_addb(imm);
}

// move an 8bit constant value into dest_reg
// the upper 24bit of the destination register can be destroyed
// this function can use FC_OP1/FC_OP2 as dest_reg which are
// not directly byte-accessible on some architectures
static void gen_mov_byte_to_reg_low_imm_canuseword(HostReg dest_reg,Bit8u imm) {
	cache_addb(0x66);
	cache_addb(0xb8+dest_reg);			// mov reg,imm
	cache_addw(imm);
}

// move the lowest 8bit of a register into memory
static void gen_mov_byte_from_reg_low(HostReg src_reg,void* dest) {
	cache_addb(0x88);	// mov [data],reg
	gen_memaddr(src_reg,dest);
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
	cache_addb(0x03);					// add reg,[data]
	gen_memaddr(reg,op);
}

// add a 32bit constant value to a full register
static void gen_add_imm(HostReg reg,Bit32u imm) {
	cache_addw(0xc081+(reg<<8));		// add reg,imm
	cache_addd(imm);
}

// and a 32bit constant value with a full register
static void gen_and_imm(HostReg reg,Bit32u imm) {
	cache_addw(0xe081+(reg<<8));		// and reg,imm
	cache_addd(imm);
}



// move a 32bit constant value into memory
static void gen_mov_direct_dword(void* dest,Bit32u imm) {
	cache_addw(0x04c7);					// mov [data],imm
	cache_addb(0x25);
	cache_addd((Bit32u)(((Bit64u)dest)&0xffffffffLL));
	cache_addd(imm);
}

// move a 64bit constant value into a full register
static void gen_mov_reg_qword(HostReg dest_reg,Bit64u imm) {
	cache_addb(0x48);
	cache_addb(0xb8+dest_reg);			// mov dest_reg,imm
	cache_addq(imm);
}

// move an address into memory
static void INLINE gen_mov_direct_ptr(void* dest,DRC_PTR_SIZE_IM imm) {
	gen_mov_reg_qword(HOST_EAX,imm);
	cache_addb(0x48);
	gen_mov_word_from_reg(HOST_EAX,dest,true);
}


// add an 8bit constant value to a memory value
static void gen_add_direct_byte(void* dest,Bit8s imm) {
	cache_addw(0x0483);					// add [data],imm
	cache_addb(0x25);
	cache_addd((Bit32u)(((Bit64u)dest)&0xffffffffLL));
	cache_addb(imm);
}

// add a 32bit (dword==true) or 16bit (dword==false) constant value to a memory value
static void gen_add_direct_word(void* dest,Bit32u imm,bool dword) {
	if ((imm<128) && dword) {
		gen_add_direct_byte(dest,(Bit8s)imm);
		return;
	}
	if (!dword) cache_addb(0x66);
	cache_addw(0x0481);					// add [data],imm
	cache_addb(0x25);
	cache_addd((Bit32u)(((Bit64u)dest)&0xffffffffLL));
	if (dword) cache_addd((Bit32u)imm);
	else cache_addw((Bit16u)imm);
}

// subtract an 8bit constant value from a memory value
static void gen_sub_direct_byte(void* dest,Bit8s imm) {
	cache_addw(0x2c83);					// sub [data],imm
	cache_addb(0x25);
	cache_addd((Bit32u)(((Bit64u)dest)&0xffffffffLL));
	cache_addb(imm);
}

// subtract a 32bit (dword==true) or 16bit (dword==false) constant value from a memory value
static void gen_sub_direct_word(void* dest,Bit32u imm,bool dword) {
	if ((imm<128) && dword) {
		gen_sub_direct_byte(dest,(Bit8s)imm);
		return;
	}
	if (!dword) cache_addb(0x66);
	cache_addw(0x2c81);					// sub [data],imm
	cache_addb(0x25);
	cache_addd((Bit32u)(((Bit64u)dest)&0xffffffffLL));
	if (dword) cache_addd((Bit32u)imm);
	else cache_addw((Bit16u)imm);
}



// effective address calculation, destination is dest_reg
// scale_reg is scaled by scale (scale_reg*(2^scale)) and
// added to dest_reg, then the immediate value is added
static INLINE void gen_lea(HostReg dest_reg,HostReg scale_reg,Bitu scale,Bits imm) {
	Bit8u rm_base;
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
	cache_addb(0x48);
	cache_addb(0x8d);			//LEA
	cache_addb(0x04+(dest_reg<<3));
	cache_addb(0x05+(dest_reg<<3)+(scale<<6));

	cache_addd(imm);		// always add dword immediate
}



// generate a call to a parameterless function
static void INLINE gen_call_function_raw(void * func) {
	cache_addb(0x48);
	cache_addb(0xb8);	// mov reg,imm64
	cache_addq((Bit64u)func);
	cache_addw(0xd0ff);
}

// generate a call to a function with paramcount parameters
// note: the parameters are loaded in the architecture specific way
// using the gen_load_param_ functions below
static Bit64u INLINE gen_call_function_setup(void * func,Bitu paramcount,bool fastcall=false) {
	// align the stack
	cache_addb(0x48);
	cache_addw(0xc48b);		// mov rax,rsp

	cache_addb(0x48);
	cache_addw(0xec83);		// sub rsp,0x08
	cache_addb(0x08);		// 0x08==return address pushed onto stack by call

	cache_addb(0x48);
	cache_addw(0xe483);		// and esp,0xfffffffffffffff0
	cache_addb(0xf0);

	cache_addb(0x48);
	cache_addw(0xc483);		// add rsp,0x08
	cache_addb(0x08);

	cache_addb(0x50);		// push rax (==old rsp)

	Bit64u proc_addr=(Bit64u)cache.pos;

	// Do the actual call to the procedure
	cache_addb(0x48);
	cache_addb(0xb8);		// mov reg,imm64
	cache_addq((Bit64u)func);

	cache_addw(0xd0ff);

	// restore stack
	cache_addb(0x5c);		// pop rsp

	return proc_addr;
}


// load an immediate value as param'th function parameter
static void INLINE gen_load_param_imm(Bitu imm,Bitu param) {
	// move an immediate 32bit value into a 64bit param reg
	switch (param) {
		case 0:			// mov param1,imm32
			gen_mov_dword_to_reg_imm(FC_OP1,(Bit32u)imm);
			break;
		case 1:			// mov param2,imm32
			gen_mov_dword_to_reg_imm(FC_OP2,(Bit32u)imm);
			break;
#if defined (_MSC_VER)
		case 2:			// mov r8,imm32
			cache_addw(0xb849);
			cache_addq((Bit32u)imm);
			break;
		case 3:			// mov r9,imm32
			cache_addw(0xb949);
			cache_addq((Bit32u)imm);
			break;
#else
		case 2:			// mov rdx,imm32
			gen_mov_dword_to_reg_imm(HOST_EDX,(Bit32u)imm);
			break;
		case 3:			// mov rcx,imm32
			gen_mov_dword_to_reg_imm(HOST_ECX,(Bit32u)imm);
			break;
#endif
		default:
			E_Exit("I(mm) >4 params unsupported");
			break;
	}
}

// load an address as param'th function parameter
static void INLINE gen_load_param_addr(DRC_PTR_SIZE_IM addr,Bitu param) {
	// move an immediate 64bit value into a 64bit param reg
	switch (param) {
		case 0:			// mov param1,addr64
			gen_mov_reg_qword(FC_OP1,addr);
			break;
		case 1:			// mov param2,addr64
			gen_mov_reg_qword(FC_OP2,addr);
			break;
#if defined (_MSC_VER)
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
#if defined (_MSC_VER)
		case 2:		// mov r8,reg&7
			cache_addb(0x49);
			gen_mov_regs(0,reg&7);
			break;
		case 3:		// mov r9,reg&7
			cache_addb(0x49);
			gen_mov_regs(1,reg&7);
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
#if defined (_MSC_VER)
		case 2:		// mov r8,[mem]
			cache_addb(0x49);
			gen_mov_word_to_reg(0,(void*)mem,true);
			break;
		case 3:		// mov r9,[mem]
			cache_addb(0x49);
			gen_mov_word_to_reg(1,(void*)mem,true);
			break;
#else
		case 2:		// mov rdx,[mem]
			gen_mov_word_to_reg(HOST_EDX,(void*)mem,true);
			break;
		case 3:		// mov rcx,[mem]
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
	cache_addq((Bit64u)ptr);

	cache_addb(0xff);		// jmp [rax+imm]
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
static Bit64u gen_create_branch_on_zero(HostReg reg,bool dword) {
	if (!dword) cache_addb(0x66);
	cache_addb(0x0b);					// or reg,reg
	cache_addb(0xc0+reg+(reg<<3));

	cache_addw(0x0074);					// jz addr
	return ((Bit64u)cache.pos-1);
}

// short conditional jump (+-127 bytes) if register is nonzero
// the destination is set by gen_fill_branch() later
static Bit64u gen_create_branch_on_nonzero(HostReg reg,bool dword) {
	if (!dword) cache_addb(0x66);
	cache_addb(0x0b);					// or reg,reg
	cache_addb(0xc0+reg+(reg<<3));

	cache_addw(0x0075);					// jnz addr
	return ((Bit64u)cache.pos-1);
}

// calculate relative offset and fill it into the location pointed to by data
static void gen_fill_branch(DRC_PTR_SIZE_IM data) {
#if C_DEBUG
	Bit64s len=(Bit64u)cache.pos-data;
	if (len<0) len=-len;
	if (len>126) LOG_MSG("Big jump %d",len);
#endif
	*(Bit8u*)data=(Bit8u)((Bit64u)cache.pos-data-1);
}

// conditional jump if register is nonzero
// for isdword==true the 32bit of the register are tested
// for isdword==false the lowest 8bit of the register are tested
static Bit64u gen_create_branch_long_nonzero(HostReg reg,bool isdword) {
	// isdword: cmp reg32,0
	// not isdword: cmp reg8,0
	cache_addb(0x0a+(isdword?1:0));				// or reg,reg
	cache_addb(0xc0+reg+(reg<<3));

	cache_addw(0x850f);		// jnz
	cache_addd(0);
	return ((Bit64u)cache.pos-4);
}

// compare 32bit-register against zero and jump if value less/equal than zero
static Bit64u gen_create_branch_long_leqzero(HostReg reg) {
	cache_addw(0xf883+(reg<<8));
	cache_addb(0x00);		// cmp reg,0

	cache_addw(0x8e0f);		// jle
	cache_addd(0);
	return ((Bit64u)cache.pos-4);
}

// calculate long relative offset and fill it into the location pointed to by data
static void gen_fill_branch_long(Bit64u data) {
	*(Bit32u*)data=(Bit32u)((Bit64u)cache.pos-data-4);
}


static void gen_run_code(void) {
	cache_addb(0x53);					// push rbx
	cache_addw(0xd0ff+(FC_OP1<<8));		// call rdi
	cache_addb(0x5b);					// pop  rbx
}

// return from a function
static void gen_return_function(void) {
	cache_addb(0xc3);		// ret
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
			*(Bit32u*)(pos+0)=0xf001f889;	// mov eax,edi; add eax,esi
			*(Bit32u*)(pos+4)=0x909006eb;	// skip
			*(Bit32u*)(pos+8)=0x90909090;
			break;
		case t_ORb:
		case t_ORw:
		case t_ORd:
			*(Bit32u*)(pos+0)=0xf009f889;	// mov eax,edi; or eax,esi
			*(Bit32u*)(pos+4)=0x909006eb;	// skip
			*(Bit32u*)(pos+8)=0x90909090;
			break;
		case t_ANDb:
		case t_ANDw:
		case t_ANDd:
			*(Bit32u*)(pos+0)=0xf021f889;	// mov eax,edi; and eax,esi
			*(Bit32u*)(pos+4)=0x909006eb;	// skip
			*(Bit32u*)(pos+8)=0x90909090;
			break;
		case t_SUBb:
		case t_SUBw:
		case t_SUBd:
			*(Bit32u*)(pos+0)=0xf029f889;	// mov eax,edi; sub eax,esi
			*(Bit32u*)(pos+4)=0x909006eb;	// skip
			*(Bit32u*)(pos+8)=0x90909090;
			break;
		case t_XORb:
		case t_XORw:
		case t_XORd:
			*(Bit32u*)(pos+0)=0xf031f889;	// mov eax,edi; xor eax,esi
			*(Bit32u*)(pos+4)=0x909006eb;	// skip
			*(Bit32u*)(pos+8)=0x90909090;
			break;
		case t_CMPb:
		case t_CMPw:
		case t_CMPd:
		case t_TESTb:
		case t_TESTw:
		case t_TESTd:
			*(Bit32u*)(pos+0)=0x90900aeb;	// skip
			*(Bit32u*)(pos+4)=0x90909090;
			*(Bit32u*)(pos+8)=0x90909090;
			break;
		case t_INCb:
		case t_INCw:
		case t_INCd:
			*(Bit32u*)(pos+0)=0xc0fff889;	// mov eax,edi; inc eax
			*(Bit32u*)(pos+4)=0x909006eb;	// skip
			*(Bit32u*)(pos+8)=0x90909090;
			break;
		case t_DECb:
		case t_DECw:
		case t_DECd:
			*(Bit32u*)(pos+0)=0xc8fff889;	// mov eax,edi; dec eax
			*(Bit32u*)(pos+4)=0x909006eb;	// skip
			*(Bit32u*)(pos+8)=0x90909090;
			break;
		case t_NEGb:
		case t_NEGw:
		case t_NEGd:
			*(Bit32u*)(pos+0)=0xd8f7f889;	// mov eax,edi; neg eax
			*(Bit32u*)(pos+4)=0x909006eb;	// skip
			*(Bit32u*)(pos+8)=0x90909090;
			break;
		default:
			*(Bit64u*)(pos+2)=(Bit64u)fct_ptr;		// fill function pointer
			break;
	}
#else
	*(Bit64u*)(pos+2)=(Bit64u)fct_ptr;
#endif
}
#endif

static void cache_block_closing(Bit8u* block_start,Bitu block_size) { }

static void cache_block_before_close(void) { }
