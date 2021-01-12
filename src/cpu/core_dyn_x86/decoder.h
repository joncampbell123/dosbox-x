/*
 *  Copyright (C) 2002-2020  The DOSBox Team
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

#define X86_DYNFPU_DH_ENABLED
#define X86_INLINED_MEMACCESS

#undef X86_DYNREC_MMX_ENABLED

#ifdef X86_DYNREC_MMX_ENABLED
void dyn_mmx_restore();
#endif

enum REP_Type {
	REP_NONE=0,REP_NZ,REP_Z
};

#ifdef DYN_NON_RECURSIVE_PAGEFAULT
union pagefault_restore {
	struct {
		uint32_t stack:1;
#ifdef CPU_FPU
		uint32_t fpu_top:1;
#ifdef X86_DYNFPU_DH_ENABLED
		uint32_t dh_fpu_inst:8;
		uint32_t dh_fpu_group:5;
#endif
#endif
	} data;
	uint32_t dword;
};
#endif

static struct DynDecode {
	PhysPt code;
	PhysPt code_start;
	PhysPt op_start;
	bool big_op;
	bool big_addr;
	REP_Type rep;
	Bitu cycles;
	CacheBlock * block;
	CacheBlock * active_block;
	struct {
		CodePageHandler * code;	
		Bitu index;
		uint8_t * wmap;
		uint8_t * invmap;
		Bitu first;
	} page;
	struct {
		Bitu val;
		Bitu mod;
		Bitu rm;
		Bitu reg;
	} modrm;
	DynReg * segprefix;
#ifdef DYN_NON_RECURSIVE_PAGEFAULT
	pagefault_restore pf_restore;
#endif
} decode;

bool PAGING_ForcePageInit(Bitu lin_addr);
static bool MakeCodePage(Bitu lin_addr,CodePageHandler * &cph) {
	uint8_t rdval;
	const Bitu cflag = cpu.code.big ? PFLAG_HASCODE32:PFLAG_HASCODE16;
	//Ensure page contains memory:
	if (GCC_UNLIKELY(mem_readb_checked(lin_addr,&rdval))) return true;
	PageHandler * handler=get_tlb_readhandler(lin_addr);
	if (handler->flags & PFLAG_HASCODE) {
		cph=( CodePageHandler *)handler;
		if (handler->flags & cflag) return false;
		cph->ClearRelease();
		cph=0;
		handler=get_tlb_readhandler(lin_addr);
	}
	if (handler->flags & PFLAG_NOCODE) {
		if (PAGING_ForcePageInit(lin_addr)) {
			handler=get_tlb_readhandler(lin_addr);
			if (handler->flags & PFLAG_HASCODE) {
				cph=( CodePageHandler *)handler;
				if (handler->flags & cflag) return false;
				cph->ClearRelease();
				cph=0;
				handler=get_tlb_readhandler(lin_addr);
			}
		}
		if (handler->flags & PFLAG_NOCODE) {
			LOG_MSG("DYNX86:Can't run code in this page!");
			cph=0;		return false;
		}
	} 
	Bitu lin_page=lin_addr >> 12;
	Bitu phys_page=lin_page;
	if (!PAGING_MakePhysPage(phys_page)) {
		LOG_MSG("DYNX86:Can't find physpage");
		cph=0;		return false;
	}
	/* Find a free CodePage */
	if (!cache.free_pages) {
		if (cache.used_pages!=decode.page.code) cache.used_pages->ClearRelease();
		else {
			if ((cache.used_pages->next) && (cache.used_pages->next!=decode.page.code))
				cache.used_pages->next->ClearRelease();
			else {
				LOG_MSG("DYNX86:Invalid cache links");
				cache.used_pages->ClearRelease();
			}
		}
	}
	CodePageHandler * cpagehandler=cache.free_pages;
	cache.free_pages=cache.free_pages->next;
	cpagehandler->prev=cache.last_page;
	cpagehandler->next=0;
	if (cache.last_page) cache.last_page->next=cpagehandler;
	cache.last_page=cpagehandler;
	if (!cache.used_pages) cache.used_pages=cpagehandler;
	cpagehandler->SetupAt(phys_page,handler);
	MEM_SetPageHandler(phys_page,1,cpagehandler);
	PAGING_UnlinkPages(lin_page,1);
	cph=cpagehandler;
	return false;
}

static uint8_t decode_fetchb(void) {
#ifdef DYN_NON_RECURSIVE_PAGEFAULT
	if (decoder_pagefault.had_pagefault) return 0;
	try {
#endif
		if (GCC_UNLIKELY(decode.page.index>=4096)) {
			/* Advance to the next page */
			decode.active_block->page.end=4095;
			/* trigger possible page fault here */
			decode.page.first++;
			Bitu fetchaddr=decode.page.first << 12;
			mem_readb(fetchaddr);
			MakeCodePage(fetchaddr,decode.page.code);
			CacheBlock * newblock=cache_getblock();
			decode.active_block->crossblock=newblock;
			newblock->crossblock=decode.active_block;
			decode.active_block=newblock;
			decode.active_block->page.start=0;
			decode.page.code->AddCrossBlock(decode.active_block);
			decode.page.wmap=decode.page.code->write_map;
			decode.page.invmap=decode.page.code->invalidation_map;
			decode.page.index=0;
		}
		decode.page.wmap[decode.page.index]+=0x01;
		decode.page.index++;
		decode.code+=1;
		return mem_readb(decode.code-1);
#ifdef DYN_NON_RECURSIVE_PAGEFAULT
	} catch (const GuestPageFaultException& pf) {
		decoder_pagefault = { true, pf.lin_addr, pf.page_addr, pf.faultcode };
		return 0;
	}
#endif
}
static uint16_t decode_fetchw(void) {
#ifdef DYN_NON_RECURSIVE_PAGEFAULT
	if (decoder_pagefault.had_pagefault) return 0;
	try {
#endif
		if (GCC_UNLIKELY(decode.page.index>=4095)) {
			uint16_t val=decode_fetchb();
			val|=decode_fetchb() << 8;
			return val;
		}
		*(uint16_t *)&decode.page.wmap[decode.page.index]+=0x0101;
		decode.code+=2;decode.page.index+=2;
		return mem_readw(decode.code-2);
#ifdef DYN_NON_RECURSIVE_PAGEFAULT
	} catch (const GuestPageFaultException& pf) {
		decoder_pagefault = { true, pf.lin_addr, pf.page_addr, pf.faultcode };
		return 0;
	}
#endif
}
static uint32_t decode_fetchd(void) {
#ifdef DYN_NON_RECURSIVE_PAGEFAULT
	if (decoder_pagefault.had_pagefault) return 0;
	try {
#endif
		if (GCC_UNLIKELY(decode.page.index>=4093)) {
			uint32_t val=decode_fetchb();
			val|=decode_fetchb() << 8;
			val|=decode_fetchb() << 16;
			val|=decode_fetchb() << 24;
			return val;
			/* Advance to the next page */
		}
		*(uint32_t *)&decode.page.wmap[decode.page.index]+=0x01010101;
		decode.code+=4;decode.page.index+=4;
		return mem_readd(decode.code-4);
#ifdef DYN_NON_RECURSIVE_PAGEFAULT
	} catch (const GuestPageFaultException& pf) {
		decoder_pagefault = { true, pf.lin_addr, pf.page_addr, pf.faultcode };
		return 0;
	}
#endif
}

#define START_WMMEM 64

static INLINE void decode_increase_wmapmask(Bitu size) {
	Bitu mapidx;
	CacheBlock* activecb=decode.active_block; 
	if (GCC_UNLIKELY(!activecb->cache.wmapmask)) {
		activecb->cache.wmapmask=(uint8_t*)malloc(START_WMMEM);
		memset(activecb->cache.wmapmask,0,START_WMMEM);
		activecb->cache.maskstart=decode.page.index;
		activecb->cache.masklen=START_WMMEM;
		mapidx=0;
	} else {
		mapidx=decode.page.index-activecb->cache.maskstart;
		if (GCC_UNLIKELY(mapidx+size>=activecb->cache.masklen)) {
			Bitu newmasklen=activecb->cache.masklen*4;
			if (newmasklen<mapidx+size) newmasklen=((mapidx+size)&~3)*2;
			uint8_t* tempmem=(uint8_t*)malloc(newmasklen);
			memset(tempmem,0,newmasklen);
			memcpy(tempmem,activecb->cache.wmapmask,activecb->cache.masklen);
			free(activecb->cache.wmapmask);
			activecb->cache.wmapmask=tempmem;
			activecb->cache.masklen=newmasklen;
		}
	}
	switch (size) {
		case 1 : activecb->cache.wmapmask[mapidx]+=0x01; break;
		case 2 : (*(uint16_t*)&activecb->cache.wmapmask[mapidx])+=0x0101; break;
		case 4 : (*(uint32_t*)&activecb->cache.wmapmask[mapidx])+=0x01010101; break;
	}
}

static bool decode_fetchb_imm(Bitu & val) {
	if (decode.page.index<4096) {
		if (decode.page.invmap != NULL) {
			if (decode.page.invmap[decode.page.index] == 0) {
				val=(uint32_t)decode_fetchb();
				return false;
			}
			HostPt tlb_addr=get_tlb_read(decode.code);
			if (tlb_addr) {
				val=(Bitu)(tlb_addr+decode.code);
				decode_increase_wmapmask(1);
				decode.code++;
				decode.page.index++;
				return true;
			}
		}
	}
	val=(uint32_t)decode_fetchb();
	return false;
}
static bool decode_fetchw_imm(Bitu & val) {
	if (decode.page.index<4095) {
        if (decode.page.invmap != NULL) {
            if ((decode.page.invmap[decode.page.index] == 0) &&
                (decode.page.invmap[decode.page.index + 1] == 0)
            ) {
				val=decode_fetchw();
				return false;
			}
			HostPt tlb_addr=get_tlb_read(decode.code);
			if (tlb_addr) {
				val=(Bitu)(tlb_addr+decode.code);
				decode_increase_wmapmask(2);
				decode.code+=2;
				decode.page.index+=2;
				return true;
			}
		}
	}
	val=decode_fetchw();
	return false;
}
static bool decode_fetchd_imm(Bitu & val) {
	if (decode.page.index<4093) {
        if (decode.page.invmap != NULL) {
            if ((decode.page.invmap[decode.page.index] == 0) &&
                (decode.page.invmap[decode.page.index + 1] == 0) &&
                (decode.page.invmap[decode.page.index + 2] == 0) &&
                (decode.page.invmap[decode.page.index + 3] == 0)
            ) {
				val=decode_fetchd();
				return false;
			}
			HostPt tlb_addr=get_tlb_read(decode.code);
			if (tlb_addr) {
				val=(Bitu)(tlb_addr+decode.code);
				decode_increase_wmapmask(4);
				decode.code+=4;
				decode.page.index+=4;
				return true;
			}
		}
	}
	val=decode_fetchd();
	return false;
}


static void dyn_reduce_cycles(void) {
	gen_protectflags();
	if (!decode.cycles) decode.cycles++;
	gen_dop_word_imm(DOP_SUB,true,DREG(CYCLES),decode.cycles);
}

static void dyn_save_noncritical_regs(void) {
	gen_releasereg(DREG(EAX));
	gen_releasereg(DREG(ECX));
	gen_releasereg(DREG(EDX));
	gen_releasereg(DREG(EBX));
	gen_releasereg(DREG(ESP));
	gen_releasereg(DREG(EBP));
	gen_releasereg(DREG(ESI));
	gen_releasereg(DREG(EDI));
}

static void dyn_save_critical_regs(void) {
	dyn_save_noncritical_regs();
	gen_releasereg(DREG(FLAGS));
	gen_releasereg(DREG(EIP));
	gen_releasereg(DREG(CYCLES));
}

static void dyn_set_eip_last_end(DynReg * endreg) {
	gen_protectflags();
	gen_lea(endreg,DREG(EIP),0,0,decode.code-decode.code_start);
	gen_dop_word_imm(DOP_ADD,decode.big_op,DREG(EIP),decode.op_start-decode.code_start);
}

static INLINE void dyn_set_eip_end(void) {
	gen_protectflags();
	gen_dop_word_imm(DOP_ADD,cpu.code.big,DREG(EIP),decode.code-decode.code_start);
}

static INLINE void dyn_set_eip_end(DynReg * endreg) {
	gen_protectflags();
	if (cpu.code.big) gen_dop_word(DOP_MOV,true,DREG(TMPW),DREG(EIP));
	else gen_extend_word(false,DREG(TMPW),DREG(EIP));
	gen_dop_word_imm(DOP_ADD,cpu.code.big,DREG(TMPW),decode.code-decode.code_start);
}

static INLINE void dyn_set_eip_last(void) {
	gen_protectflags();
	gen_dop_word_imm(DOP_ADD,cpu.code.big,DREG(EIP),decode.op_start-decode.code_start);
}


enum save_info_type {db_exception, cycle_check, normal, fpu_restore, trap, page_fault};


static struct {
	save_info_type type;
	DynState state;
	uint8_t * branch_pos;
	uint32_t eip_change;
	Bitu cycles;
	uint8_t * return_pos;
#ifdef DYN_NON_RECURSIVE_PAGEFAULT
	pagefault_restore pf_restore;
#endif
} save_info[512];

Bitu used_save_info=0;


static BlockReturn DynRunException(uint32_t eip_add,uint32_t cycle_sub,uint32_t dflags) {
	reg_flags=(dflags&FMASK_TEST) | (reg_flags&(~FMASK_TEST));
	reg_eip+=eip_add;
	CPU_Cycles-=cycle_sub;
	if (cpu.exception.which==SMC_CURRENT_BLOCK) return BR_SMCBlock;
	// TODO: Not sure how to handle it
	bool saved_allow = dosbox_allow_nonrecursive_page_fault;
	dosbox_allow_nonrecursive_page_fault = false;
	CPU_Exception(cpu.exception.which,cpu.exception.error);
	dosbox_allow_nonrecursive_page_fault = saved_allow;
	return BR_Normal;
}

#ifdef DYN_NON_RECURSIVE_PAGEFAULT
static BlockReturn DynRunPageFault(uint32_t eip_add,uint32_t cycle_sub,uint32_t pf_restore,uint32_t dflags) {
	pagefault_restore pf_restore_struct;
	pf_restore_struct.dword = pf_restore;
	reg_flags=(dflags&FMASK_TEST) | (reg_flags&(~FMASK_TEST));
	reg_eip+=eip_add;
	if (pf_restore_struct.data.stack)
		reg_esp = core_dyn.pagefault_old_stack;
#ifdef CPU_FPU
	if (pf_restore_struct.data.fpu_top)
		TOP = core_dyn.pagefault_old_fpu_top;
#endif
	CPU_Cycles-=cycle_sub;
	bool saved_allow = dosbox_allow_nonrecursive_page_fault;
	dosbox_allow_nonrecursive_page_fault = false;
	CPU_Exception(EXCEPTION_PF, core_dyn.pagefault_faultcode);
	dosbox_allow_nonrecursive_page_fault = saved_allow;
	return BR_Normal;
}
#endif

static void dyn_check_bool_exception(DynReg * check) {
	gen_dop_byte(DOP_TEST,check,0,check,0);
	save_info[used_save_info].branch_pos=gen_create_branch_long(BR_NZ);
	dyn_savestate(&save_info[used_save_info].state);
	if (!decode.cycles) decode.cycles++;
	save_info[used_save_info].cycles=decode.cycles;
	save_info[used_save_info].eip_change=decode.op_start-decode.code_start;
	if (!cpu.code.big) save_info[used_save_info].eip_change&=0xffff;
	save_info[used_save_info].type=db_exception;
	used_save_info++;
}

static void dyn_check_bool_exception_al(void) {
	cache_addw(0xC084);     // test al,al
	save_info[used_save_info].branch_pos=gen_create_branch_long(BR_NZ);
	dyn_savestate(&save_info[used_save_info].state);
	if (!decode.cycles) decode.cycles++;
	save_info[used_save_info].cycles=decode.cycles;
	save_info[used_save_info].eip_change=decode.op_start-decode.code_start;
	if (!cpu.code.big) save_info[used_save_info].eip_change&=0xffff;
	save_info[used_save_info].type=db_exception;
	used_save_info++;
}

#include "pic.h"

static void dyn_check_irqrequest(void) {
	gen_load_host(&PIC_IRQCheck,DREG(TMPB),4);
	gen_dop_word(DOP_TEST,true,DREG(TMPB),DREG(TMPB));
	save_info[used_save_info].branch_pos=gen_create_branch_long(BR_NZ);
	gen_releasereg(DREG(TMPB));
	dyn_savestate(&save_info[used_save_info].state);
	if (!decode.cycles) decode.cycles++;
	save_info[used_save_info].cycles=decode.cycles;
	save_info[used_save_info].eip_change=decode.code-decode.code_start;
	if (!cpu.code.big) save_info[used_save_info].eip_change&=0xffff;
	save_info[used_save_info].type=normal;
	used_save_info++;
}

static void dyn_check_trapflag(void) {
	gen_dop_word_imm(DOP_TEST,true,DREG(FLAGS),FLAG_TF);
	save_info[used_save_info].branch_pos=gen_create_branch_long(BR_NZ);
	gen_releasereg(DREG(FLAGS));
	dyn_savestate(&save_info[used_save_info].state);
	save_info[used_save_info].cycles=decode.cycles;
	save_info[used_save_info].eip_change=decode.code-decode.code_start;
	if (!cpu.code.big) save_info[used_save_info].eip_change&=0xffff;
	save_info[used_save_info].type=trap;
	used_save_info++;
}

#ifdef DYN_NON_RECURSIVE_PAGEFAULT
static void dyn_check_pagefault(void) {
	gen_test_host_byte(&core_dyn.pagefault);
	save_info[used_save_info].branch_pos=gen_create_branch_long(BR_NZ);
	dyn_savestate(&save_info[used_save_info].state);
	if (!decode.cycles) decode.cycles++;
	save_info[used_save_info].cycles=decode.cycles;
	save_info[used_save_info].eip_change=decode.op_start-decode.code_start;
	if (!cpu.code.big) save_info[used_save_info].eip_change&=0xffff;
	save_info[used_save_info].type=page_fault;
	save_info[used_save_info].pf_restore=decode.pf_restore;
	used_save_info++;
}

static void dyn_save_stack_for_pagefault(void) {
	gen_save_host(&core_dyn.pagefault_old_stack, DREG(ESP), 1);
	decode.pf_restore.data.stack = 1;
}

Bitu call_function_pagefault_safe_0() {
	DYN_PAGEFAULT_CHECK({
		return ((Bitu (*)())core_dyn.call_func)();
	});
}

Bitu call_function_pagefault_safe_1(Bitu arg1) {
	DYN_PAGEFAULT_CHECK({
		return ((Bitu (*)(Bitu))core_dyn.call_func)(arg1);
	});
}

Bitu call_function_pagefault_safe_2(Bitu arg1, Bitu arg2) {
	DYN_PAGEFAULT_CHECK({
		return ((Bitu (*)(Bitu,Bitu))core_dyn.call_func)(arg1, arg2);
	});
}

Bitu call_function_pagefault_safe_3(Bitu arg1, Bitu arg2, Bitu arg3) {
	DYN_PAGEFAULT_CHECK({
		return ((Bitu (*)(Bitu,Bitu,Bitu))core_dyn.call_func)(arg1, arg2, arg3);
	});
}

Bitu call_function_pagefault_safe_4(Bitu arg1, Bitu arg2, Bitu arg3, Bitu arg4) {
	DYN_PAGEFAULT_CHECK({
		return ((Bitu (*)(Bitu,Bitu,Bitu,Bitu))core_dyn.call_func)(arg1, arg2, arg3, arg4);
	});
}

static void * get_wrapped_call_function(const char* ops) {
	int num_args = 0; \
	while (*(ops++)) \
		if (*(ops-1)=='%' && (*ops == 'D' || *ops == 'I' || *ops == 'F')) \
			num_args++; \
	switch (num_args) {
		case 0: return (void*)&call_function_pagefault_safe_0;
		case 1: return (void*)&call_function_pagefault_safe_1;
		case 2: return (void*)&call_function_pagefault_safe_2;
		case 3: return (void*)&call_function_pagefault_safe_3;
		case 4: return (void*)&call_function_pagefault_safe_4;
		default:
			IllegalOption("dyn_call_function_pagefault_check unsupported number of arguments");
			return NULL;
	}
}

#define dyn_call_function_pagefault_check(func, ops, ...) {	\
	gen_save_host_direct(&core_dyn.call_func, (Bitu)(func)); \
	gen_call_function(get_wrapped_call_function(ops), ops, __VA_ARGS__); \
	dyn_check_pagefault(); \
}

#else
#define dyn_call_function_pagefault_check(...) { gen_call_function(__VA_ARGS__); }
#endif

#ifdef DYN_NON_RECURSIVE_PAGEFAULT
#ifdef X86_DYNFPU_DH_ENABLED
static void dh_fpu_mem_revert(uint8_t inst, uint8_t group);
#endif
#endif

static void dyn_fill_blocks(void) {
	for (Bitu sct=0; sct<used_save_info; sct++) {
		gen_fill_branch_long(save_info[sct].branch_pos);
		switch (save_info[sct].type) {
			case db_exception:
				dyn_loadstate(&save_info[sct].state);
				decode.cycles=save_info[sct].cycles;
				dyn_save_critical_regs();
				if (cpu.code.big) gen_call_function((void *)&DynRunException,"%Id%Id%F",save_info[sct].eip_change,save_info[sct].cycles);
				else gen_call_function((void *)&DynRunException,"%Iw%Id%F",save_info[sct].eip_change,save_info[sct].cycles);
				gen_return_fast(BR_Normal,true);
				break;
#ifdef DYN_NON_RECURSIVE_PAGEFAULT
			case page_fault:
				dyn_loadstate(&save_info[sct].state);
				decode.cycles=save_info[sct].cycles;
				dyn_save_critical_regs();
#ifdef X86_DYNFPU_DH_ENABLED
				if (save_info[sct].pf_restore.data.dh_fpu_inst)
					dh_fpu_mem_revert(save_info[sct].pf_restore.data.dh_fpu_inst, save_info[sct].pf_restore.data.dh_fpu_group);
#endif
				if (cpu.code.big) gen_call_function((void *)&DynRunPageFault,"%Id%Id%Id%F",save_info[sct].eip_change,save_info[sct].cycles,save_info[sct].pf_restore.dword);
				else gen_call_function((void *)&DynRunPageFault,"%Iw%Id%Id%F",save_info[sct].eip_change,save_info[sct].cycles,save_info[sct].pf_restore.dword);
				gen_return_fast(BR_Normal,true);
				break;
#endif
			case cycle_check:
				gen_return(BR_Cycles);
				break;
			case normal:
				dyn_loadstate(&save_info[sct].state);
				gen_dop_word_imm(DOP_ADD,decode.big_op,DREG(EIP),save_info[sct].eip_change);
				dyn_save_critical_regs();
				gen_return(BR_Cycles);
				break;
			case trap:
				dyn_loadstate(&save_info[sct].state);
				decode.cycles=save_info[sct].cycles;
				dyn_reduce_cycles();
				gen_dop_word_imm(DOP_ADD,decode.big_op,DREG(EIP),save_info[sct].eip_change);
				dyn_save_critical_regs();
				gen_return(BR_Trap);
				break;
#ifdef X86_DYNFPU_DH_ENABLED
			case fpu_restore:
				dyn_loadstate(&save_info[sct].state);
#if C_TARGETCPU == X86
				cache_addb(0xd9);   // FNSTCW fpu.host_cw
				cache_addb(0x3d);
				cache_addd((uint32_t)(&dyn_dh_fpu.host_cw));
				cache_addb(0xdd);	// FRSTOR fpu.state (fpu_restore)
				cache_addb(0x25);
				cache_addd((uint32_t)(&dyn_dh_fpu.state));
				cache_addb(0xC6);   // mov byte [fpu.state_used], 1
				cache_addb(0x05);
				cache_addd((uint32_t)(&dyn_dh_fpu.state_used));
				cache_addb(1);
#else // X86_64
				opcode(7).setabsaddr(&dyn_dh_fpu.host_cw).Emit8(0xD9); // FNSTCW [&fpu.host_cw]
				opcode(4).setabsaddr(&dyn_dh_fpu.state).Emit8(0xDD); // FRSTOR [&fpu.state]
				opcode(0).setimm(1,1).setabsaddr(&dyn_dh_fpu.state_used).Emit8(0xC6); // mov byte[], imm8
#endif
				dyn_synchstate(&save_info[sct].state);
				gen_create_jump(save_info[sct].return_pos);
				break;
#endif
		}
	}
	used_save_info=0;
}


#if !defined(X86_INLINED_MEMACCESS)
static void dyn_read_byte(DynReg * addr,DynReg * dst,bool high) {
	gen_protectflags();
	dyn_call_function_pagefault_check((void *)&mem_readb_checked,"%Dd%Ip",addr,&core_dyn.readdata);
	dyn_check_bool_exception_al();
	gen_mov_host(&core_dyn.readdata,dst,1,high?4:0);
}
static void dyn_write_byte(DynReg * addr,DynReg * val,bool high) {
	gen_protectflags();
	if (high) dyn_call_function_pagefault_check((void *)&mem_writeb_checked,"%Dd%Dh",addr,val)
	else dyn_call_function_pagefault_check((void *)&mem_writeb_checked,"%Dd%Dd",addr,val)
	dyn_check_bool_exception_al();
}
static void dyn_read_word(DynReg * addr,DynReg * dst,bool dword) {
	gen_protectflags();
	if (dword) dyn_call_function_pagefault_check((void *)&mem_readd_checked,"%Dd%Ip",addr,&core_dyn.readdata)
	else dyn_call_function_pagefault_check((void *)&mem_readw_checked,"%Dd%Ip",addr,&core_dyn.readdata)
	dyn_check_bool_exception_al();
	gen_mov_host(&core_dyn.readdata,dst,dword?4:2);
}
static void dyn_write_word(DynReg * addr,DynReg * val,bool dword) {
	gen_protectflags();
	if (dword) dyn_call_function_pagefault_check((void *)&mem_writed_checked_pf,"%Dd%Dd",addr,val)
	else dyn_call_function_pagefault_check((void *)&mem_writew_checked_pf,"%Dd%Dd",addr,val)
	dyn_check_bool_exception_al();
}
static void dyn_read_byte_release(DynReg * addr,DynReg * dst,bool high) {
	gen_protectflags();
	dyn_call_function_pagefault_check((void *)&mem_readb_checked,"%Drd%Ip",addr,&core_dyn.readdata);
	dyn_check_bool_exception_al();
	gen_mov_host(&core_dyn.readdata,dst,1,high?4:0);
}
static void dyn_write_byte_release(DynReg * addr,DynReg * val,bool high) {
	gen_protectflags();
	if (high) dyn_call_function_pagefault_check((void *)&mem_writeb_checked,"%Drd%Dh",addr,val)
	else dyn_call_function_pagefault_check((void *)&mem_writeb_checked,"%Drd%Dd",addr,val)
	dyn_check_bool_exception_al();
}
static void dyn_read_word_release(DynReg * addr,DynReg * dst,bool dword) {
	gen_protectflags();
	if (dword) dyn_call_function_pagefault_check((void *)&mem_readd_checked,"%Drd%Ip",addr,&core_dyn.readdata)
	else dyn_call_function_pagefault_check((void *)&mem_readw_checked,"%Drd%Ip",addr,&core_dyn.readdata)
	dyn_check_bool_exception_al();
	gen_mov_host(&core_dyn.readdata,dst,dword?4:2);
}
static void dyn_write_word_release(DynReg * addr,DynReg * val,bool dword) {
	gen_protectflags();
	if (dword) dyn_call_function_pagefault_check((void *)&mem_writed_checked,"%Drd%Dd",addr,val)
	else dyn_call_function_pagefault_check((void *)&mem_writew_checked,"%Drd%Dd",addr,val)
	dyn_check_bool_exception_al();
}

#else
#if C_TARGETCPU == X86

static void dyn_read_intro(DynReg * addr,bool release_addr=true) {
	gen_protectflags();

	if (addr->genreg) {
		// addr already in a register
		uint8_t reg_idx=(uint8_t)addr->genreg->index;
		x86gen.regs[X86_REG_ECX]->Clear();
		if (reg_idx!=1) {
			cache_addw(0xc88b+(reg_idx<<8));	//Mov ecx,reg
		}
		x86gen.regs[X86_REG_EAX]->Clear();
		if (release_addr) gen_releasereg(addr);
	} else {
		// addr still in memory, directly move into ecx
		x86gen.regs[X86_REG_EAX]->Clear();
		x86gen.regs[X86_REG_ECX]->Clear();
		cache_addw(0x0d8b);		//Mov ecx,[data]
		cache_addd((uint32_t)addr->data);
	}
	x86gen.regs[X86_REG_EDX]->Clear();

	cache_addw(0xc18b);		// mov eax,ecx
}

static bool mem_readb_checked_dcx86(PhysPt address) {
	DYN_PAGEFAULT_CHECK({
		return get_tlb_readhandler(address)->readb_checked(address, (uint8_t*)(&core_dyn.readdata));
	});
}

static void dyn_read_byte(DynReg * addr,DynReg * dst,bool high,bool release=false) {
	dyn_read_intro(addr,release);

	cache_addw(0xe8c1);		// shr eax,0x0c
	cache_addb(0x0c);
	cache_addw(0x048b);		// mov eax,paging.tlb.read[eax*TYPE uint32_t]
	cache_addb(0x85);
	cache_addd((uint32_t)(&paging.tlb.read[0]));
	cache_addw(0xc085);		// test eax,eax
	uint8_t* je_loc=gen_create_branch(BR_Z);


	cache_addw(0x048a);		// mov al,[eax+ecx]
	cache_addb(0x08);

	uint8_t* jmp_loc=gen_create_jump();
	gen_fill_branch(je_loc);
	cache_addb(0x51);		// push ecx
	cache_addb(0xe8);
	cache_addd(((uint32_t)&mem_readb_checked_dcx86) - (uint32_t)cache_rwtox(cache.pos)-4);
	cache_addw(0xc483);		// add esp,4
	cache_addb(0x04);

#ifdef DYN_NON_RECURSIVE_PAGEFAULT
	dyn_check_pagefault();
#endif
	dyn_check_bool_exception_al();

	cache_addw(0x058a);		//mov al,[]
	cache_addd((uint32_t)(&core_dyn.readdata));

	gen_fill_jump(jmp_loc);

	x86gen.regs[X86_REG_EAX]->notusable=true;
	GenReg * genreg=FindDynReg(dst);
	x86gen.regs[X86_REG_EAX]->notusable=false;
	cache_addw(0xc08a+(genreg->index<<11)+(high?0x2000:0));
	dst->flags|=DYNFLG_CHANGED;
}

static bool mem_readd_checked_dcx86(PhysPt address) {
	DYN_PAGEFAULT_CHECK({
		if ((address & 0xfff)<0xffd) {
			HostPt tlb_addr=get_tlb_read(address);
			if (tlb_addr) {
				core_dyn.readdata=host_readd(tlb_addr+address);
				return false;
			} else {
				return get_tlb_readhandler(address)->readd_checked(address, (uint32_t*)&core_dyn.readdata);
			}
		} else return mem_unalignedreadd_checked(address, (uint32_t*)&core_dyn.readdata);
	});
}

static bool mem_readw_checked_dcx86(PhysPt address) {
	DYN_PAGEFAULT_CHECK({
		if ((address & 0xfff)<0xfff) {
			HostPt tlb_addr=get_tlb_read(address);
			if (tlb_addr) {
				*(uint16_t*)&core_dyn.readdata=host_readw(tlb_addr+address);
				return false;
			} else {
				return get_tlb_readhandler(address)->readw_checked(address, (uint16_t*)&core_dyn.readdata);
			}
		} else return mem_unalignedreadw_checked(address, (uint16_t*)&core_dyn.readdata);
	});
}

static void dyn_read_word(DynReg * addr,DynReg * dst,bool dword,bool release=false) {
	dyn_read_intro(addr,release);

	if (!dword) {
		x86gen.regs[X86_REG_EAX]->notusable=true;
		x86gen.regs[X86_REG_ECX]->notusable=true;
	}
	GenReg * genreg=FindDynReg(dst,dword);
	if (!dword) {
		x86gen.regs[X86_REG_EAX]->notusable=false;
		x86gen.regs[X86_REG_ECX]->notusable=false;
	}

	cache_addw(0xc8c1);     // ror eax, 12
	cache_addb(0x0c);
	cache_addb(0x3d);       // cmp eax, 0xFFD00000/0xFFF00000
	cache_addd(dword ? 0xffd00000:0xfff00000);
	uint8_t* jb_loc1=gen_create_branch(BR_NB);
	cache_addb(0x25);       // and eax, 0x000FFFFF
	cache_addd(0x000fffff);
	cache_addw(0x048b);		// mov eax,paging.tlb.read[eax*TYPE uint32_t]
	cache_addb(0x85);
	cache_addd((uint32_t)(&paging.tlb.read[0]));
	cache_addw(0xc085);		// test eax,eax
	uint8_t* je_loc=gen_create_branch(BR_Z);

	if (!dword) cache_addb(0x66);
	cache_addw(0x048b+(genreg->index <<(8+3)));		// mov dest,[eax+ecx]
	cache_addb(0x08);

	uint8_t* jmp_loc=gen_create_jump();
	gen_fill_branch(jb_loc1);
	gen_fill_branch(je_loc);

	if (!dword) {
		cache_addw(0x0589+(genreg->index<<11));   // mov [core_dyn.readdata], dst
		cache_addd((uint32_t)&core_dyn.readdata);
	}
	cache_addb(0x51);		// push ecx
	cache_addb(0xe8);
	if (dword) cache_addd(((uint32_t)&mem_readd_checked_dcx86) - (uint32_t)cache_rwtox(cache.pos)-4);
	else cache_addd(((uint32_t)&mem_readw_checked_dcx86) - (uint32_t)cache_rwtox(cache.pos)-4);
	cache_addw(0xc483);		// add esp,4
	cache_addb(0x04);

#ifdef DYN_NON_RECURSIVE_PAGEFAULT
	dyn_check_pagefault();
#endif
	dyn_check_bool_exception_al();

	gen_mov_host(&core_dyn.readdata,dst,4);
	dst->flags|=DYNFLG_CHANGED;

	gen_fill_jump(jmp_loc);
}

static void dyn_write_intro(DynReg * addr,bool release_addr=true) {
	gen_protectflags();

	if (addr->genreg) {
		// addr in a register
		uint8_t reg_idx_addr=(uint8_t)addr->genreg->index;

		x86gen.regs[X86_REG_EAX]->Clear();
		x86gen.regs[X86_REG_EAX]->notusable=true;
		x86gen.regs[X86_REG_ECX]->Clear();
		x86gen.regs[X86_REG_ECX]->notusable=true;

		if (reg_idx_addr) {
			// addr!=eax
			cache_addb(0x8b);		//Mov eax,reg
			cache_addb(0xc0+reg_idx_addr);
		}
		if (release_addr) gen_releasereg(addr);
	} else {
		// addr still in memory, directly move into eax
		x86gen.regs[X86_REG_EAX]->Clear();
		x86gen.regs[X86_REG_EAX]->notusable=true;
		x86gen.regs[X86_REG_ECX]->Clear();
		x86gen.regs[X86_REG_ECX]->notusable=true;
		cache_addb(0xa1);		//Mov eax,[data]
		cache_addd((uint32_t)addr->data);
	}

	cache_addw(0xc88b);		// mov ecx,eax
}

static void dyn_write_byte(DynReg * addr,DynReg * val,bool high,bool release=false) {
	dyn_write_intro(addr,release);

	GenReg * genreg=FindDynReg(val);
	cache_addw(0xe9c1);		// shr ecx,0x0c
	cache_addb(0x0c);
	cache_addw(0x0c8b);		// mov ecx,paging.tlb.write[ecx*TYPE uint32_t]
	cache_addb(0x8d);
	cache_addd((uint32_t)(&paging.tlb.write[0]));
	cache_addw(0xc985);		// test ecx,ecx
	uint8_t* je_loc=gen_create_branch(BR_Z);

	cache_addw(0x0488+(genreg->index<<11)+(high?0x2000:0));		// mov [eax+ecx],reg
	cache_addb(0x08);

	uint8_t* jmp_loc=gen_create_jump();
	gen_fill_branch(je_loc);

	cache_addb(0x52);	// push edx
	if (GCC_UNLIKELY(high)) cache_addw(0xe086+((genreg->index+(genreg->index<<3))<<8));
	cache_addb(0x50+genreg->index);
	cache_addb(0x50);	// push eax
	if (GCC_UNLIKELY(high)) cache_addw(0xe086+((genreg->index+(genreg->index<<3))<<8));
	cache_addb(0xe8);
	cache_addd(((uint32_t)&mem_writeb_checked_pagefault) - (uint32_t)cache_rwtox(cache.pos)-4);
	cache_addw(0xc483);		// add esp,8
	cache_addb(0x08);
	cache_addb(0x5a);		// pop edx

	// Restore registers to be used again
	x86gen.regs[X86_REG_EAX]->notusable=false;
	x86gen.regs[X86_REG_ECX]->notusable=false;

#ifdef DYN_NON_RECURSIVE_PAGEFAULT
	dyn_check_pagefault();
#endif
	dyn_check_bool_exception_al();

	gen_fill_jump(jmp_loc);
}

static void dyn_write_word(DynReg * addr,DynReg * val,bool dword,bool release=false) {
	dyn_write_intro(addr,release);

	GenReg * genreg=FindDynReg(val);
	cache_addw(0xc9c1);     // ror ecx, 12
	cache_addb(0x0c);
	cache_addw(0xf981);     // cmp ecx, 0xFFD00000/0xFFF00000
	cache_addd(dword ? 0xffd00000:0xfff00000);
	uint8_t* jb_loc1=gen_create_branch(BR_NB);
	cache_addw(0xe181);     // and ecx, 0x000FFFFF
	cache_addd(0x000fffff);
	cache_addw(0x0c8b);		// mov ecx,paging.tlb.write[ecx*TYPE uint32_t]
	cache_addb(0x8d);
	cache_addd((uint32_t)(&paging.tlb.write[0]));
	cache_addw(0xc985);		// test ecx,ecx
	uint8_t* je_loc=gen_create_branch(BR_Z);

	if (!dword) cache_addb(0x66);
	cache_addw(0x0489+(genreg->index <<(8+3)));		// mov [eax+ecx],reg
	cache_addb(0x08);

	uint8_t* jmp_loc=gen_create_jump();
	gen_fill_branch(jb_loc1);
	gen_fill_branch(je_loc);

	cache_addb(0x52);	// push edx
	cache_addb(0x50+genreg->index);
	cache_addb(0x50);	// push eax
	cache_addb(0xe8);
	if (dword) cache_addd(((uint32_t)&mem_writed_checked_pagefault) - (uint32_t)cache_rwtox(cache.pos)-4);
	else cache_addd(((uint32_t)&mem_writew_checked_pagefault) - (uint32_t)cache_rwtox(cache.pos)-4);
	cache_addw(0xc483);		// add esp,8
	cache_addb(0x08);
	cache_addb(0x5a);		// pop edx

	// Restore registers to be used again
	x86gen.regs[X86_REG_EAX]->notusable=false;
	x86gen.regs[X86_REG_ECX]->notusable=false;

#ifdef DYN_NON_RECURSIVE_PAGEFAULT
	dyn_check_pagefault();
#endif
	dyn_check_bool_exception_al();

	gen_fill_jump(jmp_loc);
}

#else // X86_64
bool mem_unalignedreadw_checked_pagefault_check(const PhysPt address,uint16_t * const val) {
	DYN_PAGEFAULT_CHECK({
		return mem_unalignedreadw_checked(address, val);
	});
}
bool mem_unalignedreadd_checked_pagefault_check(const PhysPt address,uint32_t * const val) {
	DYN_PAGEFAULT_CHECK({
		return mem_unalignedreadd_checked(address, val);
	});
}
bool mem_unalignedwritew_checked_pagefault_check(const PhysPt address,uint16_t const val) {
	DYN_PAGEFAULT_CHECK({
		return mem_unalignedwritew_checked(address, val);
	});
}
bool mem_unalignedwrited_checked_pagefault_check(const PhysPt address,uint32_t const val) {
	DYN_PAGEFAULT_CHECK({
		return mem_unalignedwrited_checked(address, val);
	});
}

static bool mem_readd_checked_dcx64(PhysPt address, uint32_t* dst) {
	DYN_PAGEFAULT_CHECK({
		return get_tlb_readhandler(address)->readd_checked(address, dst);
	});
}
static bool mem_readw_checked_dcx64(PhysPt address, uint16_t* dst) {
	DYN_PAGEFAULT_CHECK({
		return get_tlb_readhandler(address)->readw_checked(address, dst);
	});
}
static bool mem_writed_checked_dcx64(PhysPt address, Bitu val) {
	DYN_PAGEFAULT_CHECK({
		return get_tlb_writehandler(address)->writed_checked(address, val);
	});
}
static bool mem_writew_checked_dcx64(PhysPt address, Bitu val) {
	DYN_PAGEFAULT_CHECK({
		return get_tlb_writehandler(address)->writew_checked(address, val);
	});
}
static bool mem_readb_checked_dcx64(PhysPt address, uint8_t* dst) {
	DYN_PAGEFAULT_CHECK({
		return get_tlb_readhandler(address)->readb_checked(address, dst);
	});
}
static bool mem_writeb_checked_dcx64(PhysPt address, Bitu val) {
	DYN_PAGEFAULT_CHECK({
		return get_tlb_writehandler(address)->writeb_checked(address, val);
	});
}

static void dyn_read_word(DynReg * addr,DynReg * dst,bool dword,bool release=false) {
	DynState callstate;
	uint8_t tmp;

	gen_protectflags();
	GenReg *gensrc = FindDynReg(addr);
	if (dword && release) gen_releasereg(addr);
	GenReg *gendst = FindDynReg(dst,dword);
	if (!dword && release) gen_releasereg(addr);
	gensrc->notusable = true;
	x64gen.regs[reg_args[0]]->notusable=true;
	x64gen.regs[reg_args[1]]->notusable=true;
	tmp = GetNextReg();
	gensrc->notusable = false;
	x64gen.regs[reg_args[0]]->notusable=false;
	x64gen.regs[reg_args[1]]->notusable=false;
	dyn_savestate(&callstate);

	uint8_t *page_brk;
	opcode(tmp).set64().setea(gensrc->index,-1,0,dword?3:1).Emit8(0x8D); // lea tmp, [dst+(dword?3:1)]
	if (dword) {
		opcode(4).set64().setimm(~0xFFF,4).setrm(tmp).Emit8(0x81); // and tmp, ~0xFFF
		opcode(gensrc->index).set64().setrm(tmp).Emit8(0x39); // cmp tmp,src
		page_brk=gen_create_branch(BR_NBE);
	} else {
		opcode(0,false).setimm(0xFFF,2).setrm(tmp).Emit8(0xF7); // test tmpw,0xFFF
		page_brk=gen_create_branch(BR_Z);
	}

	opcode(5).setrm(tmp).setimm(12,1).Emit8(0xC1); // shr tmpd,12
	// mov tmp, [8*tmp+paging.tlb.read(rbp)]
	opcode(tmp).set64().setea(5,tmp,3,(Bits)paging.tlb.read-(Bits)&cpu_regs).Emit8(0x8B);
	opcode(tmp).set64().setrm(tmp).Emit8(0x85); // test tmp,tmp
	uint8_t *nomap=gen_create_branch(BR_Z);
	//mov dst, [tmp+src]
	opcode(gendst->index,dword).setea(tmp,gensrc->index).Emit8(0x8B);
	uint8_t* jmp_loc = gen_create_jump();

	gen_fill_branch(page_brk);
	gen_load_imm(tmp, (Bitu)(dword?(void*)mem_unalignedreadd_checked:(void*)mem_unalignedreadw_checked));
	uint8_t* page_jmp = gen_create_short_jump();

	gen_fill_branch(nomap);
	gen_load_imm(tmp, (Bitu)(dword?(void*)mem_readd_checked_dcx64:(void*)mem_readw_checked_dcx64));
	gen_fill_short_jump(page_jmp);

	if (gensrc->index != ARG0_REG) {
		x64gen.regs[reg_args[0]]->Clear();
		opcode(ARG0_REG).setrm(gensrc->index).Emit8(0x8B);
	}
	gendst->Clear();
	x64gen.regs[reg_args[1]]->Clear();
	gen_load_imm(ARG1_REG, (Bitu)dst->data);
	gen_call_ptr(NULL, tmp);
#ifdef DYN_NON_RECURSIVE_PAGEFAULT
	dyn_check_pagefault();
#endif
	dyn_check_bool_exception_al();

	dyn_synchstate(&callstate);
	dst->flags |= DYNFLG_CHANGED;
	gen_fill_jump(jmp_loc);
}

static void dyn_read_byte(DynReg * addr,DynReg * dst,bool high,bool release=false) {
	DynState callstate;
	uint8_t tmp;

	gen_protectflags();
	GenReg *gensrc = FindDynReg(addr);
	GenReg *gendst = FindDynReg(dst);
	tmp = GetNextReg(high);
	if (release) gen_releasereg(addr);
	dyn_savestate(&callstate);

	if (gendst->index>3) IllegalOption("dyn_read_byte");

	opcode(tmp).setrm(gensrc->index).Emit8(0x8B); // mov tmp, src
	opcode(5).setrm(tmp).setimm(12,1).Emit8(0xC1); // shr tmp,12
	// mov tmp, [8*tmp+paging.tlb.read(rbp)]
	opcode(tmp).set64().setea(5,tmp,3,(Bits)paging.tlb.read-(Bits)&cpu_regs).Emit8(0x8B);
	opcode(tmp).set64().setrm(tmp).Emit8(0x85); // test tmp,tmp
	uint8_t *nomap=gen_create_branch(BR_Z);

	int src = gensrc->index;
	if (high && src>=8) { // can't use REX prefix with high-byte reg
		opcode(tmp).set64().setrm(src).Emit8(0x03); // add tmp, src
		src = -1;
	}
	// mov dst, byte [tmp+src]
	opcode(gendst->index,true,high?4:0).setea(tmp,src).Emit8(0x8A);
	uint8_t* jmp_loc=gen_create_jump();

	gen_fill_branch(nomap);
	if (gensrc->index != ARG0_REG) {
		x64gen.regs[reg_args[0]]->Clear();
		opcode(ARG0_REG).setrm(gensrc->index).Emit8(0x8B); // mov ARG0,src
	}
	x64gen.regs[reg_args[1]]->Clear();
	gen_load_imm(ARG1_REG, (Bitu)(high?((uint8_t*)dst->data)+1:dst->data));
	gendst->Clear();
	gen_call_ptr((void*)mem_readb_checked_dcx64);
#ifdef DYN_NON_RECURSIVE_PAGEFAULT
	dyn_check_pagefault();
#endif
	dyn_check_bool_exception_al();

	dyn_synchstate(&callstate);
	dst->flags |= DYNFLG_CHANGED;
	gen_fill_jump(jmp_loc);
}

static void dyn_write_word(DynReg * addr,DynReg * val,bool dword,bool release=false) {
	DynState callstate;
	uint8_t tmp;

	gen_protectflags();
	GenReg *gendst = FindDynReg(addr);
	GenReg *genval = FindDynReg(val);
	x64gen.regs[reg_args[0]]->notusable=true;
	x64gen.regs[reg_args[1]]->notusable=true;
	tmp = GetNextReg();
	x64gen.regs[reg_args[0]]->notusable=false;
	x64gen.regs[reg_args[1]]->notusable=false;
	if (release) gen_releasereg(addr);
	dyn_savestate(&callstate);

	uint8_t *page_brk;
	opcode(tmp).set64().setea(gendst->index,-1,0,dword?3:1).Emit8(0x8D); // lea tmp, [dst+(dword?3:1)]
	if (dword) {
		opcode(4).set64().setimm(~0xFFF,4).setrm(tmp).Emit8(0x81); // and tmp, ~0xFFF
		opcode(gendst->index).set64().setrm(tmp).Emit8(0x39); // cmp tmp,dst
		page_brk=gen_create_branch(BR_NBE);
	} else {
		opcode(0,false).setimm(0xFFF,2).setrm(tmp).Emit8(0xF7); // test tmpw,0xFFF
		page_brk=gen_create_branch(BR_Z);
	}

	opcode(5).setrm(tmp).setimm(12,1).Emit8(0xC1); // shr tmpd,12
	// mov tmp, [8*tmp+paging.tlb.write(rbp)]
	opcode(tmp).set64().setea(5,tmp,3,(Bits)paging.tlb.write-(Bits)&cpu_regs).Emit8(0x8B);
	opcode(tmp).set64().setrm(tmp).Emit8(0x85); // test tmp,tmp
	uint8_t *nomap=gen_create_branch(BR_Z);
	//mov [tmp+src], dst
	opcode(genval->index,dword).setea(tmp,gendst->index).Emit8(0x89);
	uint8_t* jmp_loc = gen_create_jump();

	gen_fill_branch(page_brk);
	gen_load_imm(tmp, (Bitu)(dword?(void*)mem_unalignedwrited_checked:(void*)mem_unalignedwritew_checked));
	uint8_t* page_jmp = gen_create_short_jump();
	gen_fill_branch(nomap);
	gen_load_imm(tmp, (Bitu)(dword?(void*)mem_writed_checked_dcx64:(void*)mem_writew_checked_dcx64));
	gen_fill_short_jump(page_jmp);

	if (gendst->index != ARG0_REG) {
		x64gen.regs[reg_args[0]]->Clear();
		opcode(ARG0_REG).setrm(gendst->index).Emit8(0x8B);
	}
	gen_load_arg_reg(1, val, dword ? "d":"w");
	gen_call_ptr(NULL, tmp);
#ifdef DYN_NON_RECURSIVE_PAGEFAULT
	dyn_check_pagefault();
#endif
	dyn_check_bool_exception_al();
	dyn_synchstate(&callstate);
	gen_fill_jump(jmp_loc);
}

static void dyn_write_byte(DynReg * addr,DynReg * val,bool high,bool release=false) {
	DynState callstate;
	uint8_t tmp;

	gen_protectflags();
	GenReg *gendst = FindDynReg(addr);
	GenReg *genval = FindDynReg(val);
	tmp = GetNextReg(high);
	if (release) gen_releasereg(addr);
	dyn_savestate(&callstate);

	if (genval->index>3) IllegalOption("dyn_write_byte");

	opcode(tmp).setrm(gendst->index).Emit8(0x8B); // mov tmpd, dst
	opcode(5).setrm(tmp).setimm(12,1).Emit8(0xC1); // shr tmpd,12
	// mov tmp, [8*tmp+paging.tlb.write(rbp)]
	opcode(tmp).set64().setea(5,tmp,3,(Bits)paging.tlb.write-(Bits)&cpu_regs).Emit8(0x8B);
	opcode(tmp).set64().setrm(tmp).Emit8(0x85); // test tmp,tmp
	uint8_t *nomap=gen_create_branch(BR_Z);

	int dst = gendst->index;
	if (high && dst>=8) { // can't use REX prefix with high-byte reg
		opcode(tmp).set64().setrm(dst).Emit8(0x03); // add tmp, dst
		dst = -1;
	}
	// mov byte [tmp+src], val
	opcode(genval->index,true,high?4:0).setea(tmp,dst).Emit8(0x88);

	uint8_t* jmp_loc=gen_create_short_jump();
	gen_fill_branch(nomap);

	if (gendst->index != ARG0_REG) {
		x64gen.regs[reg_args[0]]->Clear();
		opcode(ARG0_REG).setrm(gendst->index).Emit8(0x8B); // mov ARG0,dst
	}
	gen_load_arg_reg(1, val, high ? "h":"l");
	gen_call_ptr((void*)mem_writeb_checked_dcx64);
#ifdef DYN_NON_RECURSIVE_PAGEFAULT
	dyn_check_pagefault();
#endif
	dyn_check_bool_exception_al();

	dyn_synchstate(&callstate);
	gen_fill_short_jump(jmp_loc);
}

#endif
static void dyn_read_word_release(DynReg * addr,DynReg * dst,bool dword) {
	dyn_read_word(addr,dst,dword,addr!=dst);
}
static void dyn_read_byte_release(DynReg * addr,DynReg * dst,bool high) {
	dyn_read_byte(addr,dst,high,addr!=dst);
}
static void dyn_write_word_release(DynReg * addr,DynReg * val,bool dword) {
	dyn_write_word(addr,val,dword,true);
}
static void dyn_write_byte_release(DynReg * addr,DynReg * src,bool high) {
	dyn_write_byte(addr,src,high,true);
}
#endif


static void dyn_push_unchecked(DynReg * dynreg) {
	gen_protectflags();
	gen_lea(DREG(STACK),DREG(ESP),0,0,decode.big_op?(-4):(-2));
	gen_dop_word_var(DOP_AND,true,DREG(STACK),&cpu.stack.mask);
	gen_dop_word_var(DOP_AND,true,DREG(ESP),&cpu.stack.notmask);
	gen_dop_word(DOP_OR,true,DREG(ESP),DREG(STACK));
	gen_dop_word(DOP_ADD,true,DREG(STACK),DREG(SS));
	if (decode.big_op) {
		dyn_call_function_pagefault_check((void *)&mem_writed,"%Drd%Dd",DREG(STACK),dynreg);
	} else {
		//Can just push the whole 32-bit word as operand
		dyn_call_function_pagefault_check((void *)&mem_writew,"%Drd%Dd",DREG(STACK),dynreg);
	}
}

static void dyn_push(DynReg * dynreg) {
	gen_protectflags();
	gen_lea(DREG(STACK),DREG(ESP),0,0,decode.big_op?(-4):(-2));
	gen_dop_word(DOP_MOV,true,DREG(NEWESP),DREG(ESP));
	gen_dop_word_var(DOP_AND,true,DREG(STACK),&cpu.stack.mask);
	gen_dop_word_var(DOP_AND,true,DREG(NEWESP),&cpu.stack.notmask);
	gen_dop_word(DOP_OR,true,DREG(NEWESP),DREG(STACK));
	gen_dop_word(DOP_ADD,true,DREG(STACK),DREG(SS));
	if (decode.big_op) {
		dyn_call_function_pagefault_check((void *)&mem_writed_checked,"%Drd%Dd",DREG(STACK),dynreg);
	} else {
		//Can just push the whole 32-bit word as operand
		dyn_call_function_pagefault_check((void *)&mem_writew_checked,"%Drd%Dd",DREG(STACK),dynreg);
	}
	dyn_check_bool_exception_al();
	/* everything was ok, change register now */
	gen_dop_word(DOP_MOV,true,DREG(ESP),DREG(NEWESP));
	gen_releasereg(DREG(NEWESP));
}

static void dyn_pop(DynReg * dynreg,bool checked=true) {
	gen_protectflags();
	gen_dop_word(DOP_MOV,true,DREG(STACK),DREG(ESP));
	gen_dop_word_var(DOP_AND,true,DREG(STACK),&cpu.stack.mask);
	gen_dop_word(DOP_ADD,true,DREG(STACK),DREG(SS));
	if (checked) {
		if (decode.big_op) {
			dyn_call_function_pagefault_check((void *)&mem_readd_checked,"%Drd%Ip",DREG(STACK),&core_dyn.readdata);
		} else {
			dyn_call_function_pagefault_check((void *)&mem_readw_checked,"%Drd%Ip",DREG(STACK),&core_dyn.readdata);
		}
		dyn_check_bool_exception_al();
		gen_mov_host(&core_dyn.readdata,dynreg,decode.big_op?4:2);
	} else {
		if (decode.big_op) {
			dyn_call_function_pagefault_check((void *)&mem_readd,"%Rd%Drd",dynreg,DREG(STACK));
		} else {
			dyn_call_function_pagefault_check((void *)&mem_readw,"%Rw%Drd",dynreg,DREG(STACK));
		}
	}
	if (dynreg!=DREG(ESP)) {
		gen_lea(DREG(STACK),DREG(ESP),0,0,decode.big_op?4:2);
		gen_dop_word_var(DOP_AND,true,DREG(STACK),&cpu.stack.mask);
		gen_dop_word_var(DOP_AND,true,DREG(ESP),&cpu.stack.notmask);
		gen_dop_word(DOP_OR,true,DREG(ESP),DREG(STACK));
		gen_releasereg(DREG(STACK));
	}
}

static INLINE void dyn_get_modrm(void) {
	decode.modrm.val=decode_fetchb();
	decode.modrm.mod=(decode.modrm.val >> 6) & 3;
	decode.modrm.reg=(decode.modrm.val >> 3) & 7;
	decode.modrm.rm=(decode.modrm.val & 7);
}

static void dyn_fill_ea(bool addseg=true, DynReg * reg_ea=DREG(EA)) {
	DynReg * segbase;
	if (!decode.big_addr) {
		Bits imm;
		switch (decode.modrm.mod) {
		case 0:imm=0;break;
		case 1:imm=(int8_t)decode_fetchb();break;
		case 2:imm=(int16_t)decode_fetchw();break;
		}
		DynReg * extend_src=reg_ea;
		switch (decode.modrm.rm) {
		case 0:/* BX+SI */
			gen_lea(reg_ea,DREG(EBX),DREG(ESI),0,imm);
			segbase=DREG(DS);
			break;
		case 1:/* BX+DI */
			gen_lea(reg_ea,DREG(EBX),DREG(EDI),0,imm);
			segbase=DREG(DS);
			break;
		case 2:/* BP+SI */
			gen_lea(reg_ea,DREG(EBP),DREG(ESI),0,imm);
			segbase=DREG(SS);
			break;
		case 3:/* BP+DI */
			gen_lea(reg_ea,DREG(EBP),DREG(EDI),0,imm);
			segbase=DREG(SS);
			break;
		case 4:/* SI */
			if (imm) gen_lea(reg_ea,DREG(ESI),0,0,imm);
			else extend_src=DREG(ESI);
			segbase=DREG(DS);
			break;
		case 5:/* DI */
			if (imm) gen_lea(reg_ea,DREG(EDI),0,0,imm);
			else extend_src=DREG(EDI);
			segbase=DREG(DS);
			break;
		case 6:/* imm/BP */
			if (!decode.modrm.mod) {
				imm=decode_fetchw();
                gen_dop_word_imm(DOP_MOV,true,reg_ea,imm);
				segbase=DREG(DS);
				goto skip_extend_word;
			} else {
				gen_lea(reg_ea,DREG(EBP),0,0,imm);
				segbase=DREG(SS);
			}
			break;
		case 7: /* BX */
			if (imm) gen_lea(reg_ea,DREG(EBX),0,0,imm);
			else extend_src=DREG(EBX);
			segbase=DREG(DS);
			break;
		}
		gen_extend_word(false,reg_ea,extend_src);
skip_extend_word:
		if (addseg) {
			gen_lea(reg_ea,reg_ea,decode.segprefix ? decode.segprefix : segbase,0,0);
		}
	} else {
		Bits imm=0;
		DynReg * base=0;DynReg * scaled=0;Bitu scale=0;
		switch (decode.modrm.rm) {
		case 0:base=DREG(EAX);segbase=DREG(DS);break;
		case 1:base=DREG(ECX);segbase=DREG(DS);break;
		case 2:base=DREG(EDX);segbase=DREG(DS);break;
		case 3:base=DREG(EBX);segbase=DREG(DS);break;
		case 4:	/* SIB */
			{
				Bitu sib=decode_fetchb();
				static DynReg * scaledtable[8]={
					DREG(EAX),DREG(ECX),DREG(EDX),DREG(EBX),
							0,DREG(EBP),DREG(ESI),DREG(EDI),
				};
				scaled=scaledtable[(sib >> 3) &7];
				scale=(sib >> 6);
				switch (sib & 7) {
				case 0:base=DREG(EAX);segbase=DREG(DS);break;
				case 1:base=DREG(ECX);segbase=DREG(DS);break;
				case 2:base=DREG(EDX);segbase=DREG(DS);break;
				case 3:base=DREG(EBX);segbase=DREG(DS);break;
				case 4:base=DREG(ESP);segbase=DREG(SS);break;
				case 5:
					if (decode.modrm.mod) {
						base=DREG(EBP);segbase=DREG(SS);
					} else {
						segbase=DREG(DS);
						Bitu val;
						if (decode_fetchd_imm(val)) {
							gen_dop_word_imm_mem(DOP_MOV,true,DREG(EA),(void*)val);
							if (!addseg) {
								gen_lea(reg_ea,DREG(EA),scaled,scale,0);
							} else {
								DynReg** seg = decode.segprefix ? &decode.segprefix : &segbase;
								gen_lea(DREG(EA),DREG(EA),scaled,scale,0);
								gen_lea(reg_ea,DREG(EA),*seg,0,0);
							}
							if (reg_ea!=DREG(EA)) gen_releasereg(DREG(EA));
							return;
						}
						imm=(int32_t)val;
					}
					break;
				case 6:base=DREG(ESI);segbase=DREG(DS);break;
				case 7:base=DREG(EDI);segbase=DREG(DS);break;
				}
			}	
			break;	/* SIB Break */
		case 5:
			if (decode.modrm.mod) {
				base=DREG(EBP);segbase=DREG(SS);
			} else {
				imm=(int32_t)decode_fetchd();segbase=DREG(DS);
			}
			break;
		case 6:base=DREG(ESI);segbase=DREG(DS);break;
		case 7:base=DREG(EDI);segbase=DREG(DS);break;
		}
		switch (decode.modrm.mod) {
		case 1:imm=(int8_t)decode_fetchb();break;
		case 2: {
			Bitu val;
			if (decode_fetchd_imm(val)) {
				gen_dop_word_imm_mem(DOP_MOV,true,DREG(EA),(void*)val);
				if (!addseg) {
					gen_lea(DREG(EA),DREG(EA),scaled,scale,0);
					gen_lea(reg_ea,DREG(EA),base,0,0);
				} else {
					DynReg** seg = decode.segprefix ? &decode.segprefix : &segbase;
					if (!base) {
						gen_lea(DREG(EA),DREG(EA),scaled,scale,0);
						gen_lea(reg_ea,DREG(EA),*seg,0,0);
					} else if (!scaled) {
						gen_lea(DREG(EA),DREG(EA),*seg,0,0);
						gen_lea(reg_ea,DREG(EA),base,0,0);
					} else {
						gen_lea(DREG(EA),DREG(EA),scaled,scale,0);
						gen_lea(DREG(EA),DREG(EA),base,0,0);
						gen_lea(reg_ea,DREG(EA),decode.segprefix ? decode.segprefix : segbase,0,0);
					}
				}
				if (reg_ea!=DREG(EA)) gen_releasereg(DREG(EA));
				return;
			}
			
			imm=(int32_t)val;
			break;
			}
		}
		if (!addseg) {
			gen_lea(reg_ea,base,scaled,scale,imm);
		} else {
			DynReg** seg = decode.segprefix ? &decode.segprefix : &segbase;
			if (!base) gen_lea(reg_ea,*seg,scaled,scale,imm);
			else if (!scaled) gen_lea(reg_ea,base,*seg,0,imm);
			else {
				gen_lea(DREG(EA),base,scaled,scale,imm);
				gen_lea(reg_ea,DREG(EA),decode.segprefix ? decode.segprefix : segbase,0,0);
				if (reg_ea!=DREG(EA)) gen_releasereg(DREG(EA));
			}
		}
	}
}


static void dyn_dop_word_imm(DualOps op,bool dword,DynReg * dr1) {
	Bitu val;
	if (dword) {
		if (decode_fetchd_imm(val)) {
			gen_dop_word_imm_mem(op,true,dr1,(void*)val);
			return;
		}
	} else {
		if (decode_fetchw_imm(val)) {
			gen_dop_word_imm_mem(op,false,dr1,(void*)val);
			return;
		}
	}
	gen_dop_word_imm(op,dword,dr1,val);
}

static void dyn_dop_byte_imm(DualOps op,DynReg * dr1,uint8_t di1) {
	Bitu val;
	if (decode_fetchb_imm(val)) {
		gen_dop_byte_imm_mem(op,dr1,di1,(void*)val);
	} else {
		gen_dop_byte_imm(op,dr1,di1,(uint8_t)val);
	}
}


#include "helpers.h"
#include "string.h"


static void dyn_dop_ebgb(DualOps op) {
	dyn_get_modrm();DynReg * rm_reg=&DynRegs[decode.modrm.reg&3];
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		if ((op<=DOP_TEST) && (op!=DOP_ADC && op!=DOP_SBB)) set_skipflags(true);
		dyn_read_byte(DREG(EA),DREG(TMPB),false);
		if (op<=DOP_TEST) {
			if (op==DOP_ADC || op==DOP_SBB) gen_needcarry();
			else set_skipflags(false);
		}
		gen_dop_byte(op,DREG(TMPB),0,rm_reg,decode.modrm.reg&4);
		if (op!=DOP_CMP) dyn_write_byte_release(DREG(EA),DREG(TMPB),false);
		else gen_releasereg(DREG(EA));
		gen_releasereg(DREG(TMPB));
	} else {
		if (op<=DOP_TEST) {
			if (op==DOP_ADC || op==DOP_SBB) gen_needcarry();
			else gen_discardflags();
		}
		gen_dop_byte(op,&DynRegs[decode.modrm.rm&3],decode.modrm.rm&4,rm_reg,decode.modrm.reg&4);
	}
}


static void dyn_dop_gbeb(DualOps op) {
	dyn_get_modrm();DynReg * rm_reg=&DynRegs[decode.modrm.reg&3];
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		if ((op<=DOP_TEST) && (op!=DOP_ADC && op!=DOP_SBB)) set_skipflags(true);
		dyn_read_byte_release(DREG(EA),DREG(TMPB),false);
		if (op<=DOP_TEST) {
			if (op==DOP_ADC || op==DOP_SBB) gen_needcarry();
			else set_skipflags(false);
		}
		gen_dop_byte(op,rm_reg,decode.modrm.reg&4,DREG(TMPB),0);
		gen_releasereg(DREG(TMPB));
	} else {
		if (op<=DOP_TEST) {
			if (op==DOP_ADC || op==DOP_SBB) gen_needcarry();
			else gen_discardflags();
		}
		gen_dop_byte(op,rm_reg,decode.modrm.reg&4,&DynRegs[decode.modrm.rm&3],decode.modrm.rm&4);
	}
}

static void dyn_mov_ebib(void) {
	dyn_get_modrm();
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		gen_call_write(DREG(EA),decode_fetchb(),1);
#ifdef DYN_NON_RECURSIVE_PAGEFAULT
		dyn_check_pagefault();
#endif
		dyn_check_bool_exception_al();
	} else {
		gen_dop_byte_imm(DOP_MOV,&DynRegs[decode.modrm.rm&3],decode.modrm.rm&4,decode_fetchb());
	}
}

static void dyn_mov_ebgb(void) {
	dyn_get_modrm();
	DynReg * rm_reg=&DynRegs[decode.modrm.reg&3];Bitu rm_regi=decode.modrm.reg&4;
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		dyn_write_byte_release(DREG(EA),rm_reg,rm_regi!=0);
	} else {
		gen_dop_byte(DOP_MOV,&DynRegs[decode.modrm.rm&3],decode.modrm.rm&4,rm_reg,rm_regi);
	}
}

static void dyn_mov_gbeb(void) {
	dyn_get_modrm();
	DynReg * rm_reg=&DynRegs[decode.modrm.reg&3];Bitu rm_regi=decode.modrm.reg&4;
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		dyn_read_byte_release(DREG(EA),rm_reg,rm_regi!=0);
	} else {
		gen_dop_byte(DOP_MOV,rm_reg,rm_regi,&DynRegs[decode.modrm.rm&3],decode.modrm.rm&4);
	}
}

static void dyn_dop_evgv(DualOps op) {
	dyn_get_modrm();
	DynReg * rm_reg=&DynRegs[decode.modrm.reg];
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		if ((op<=DOP_TEST) && (op!=DOP_ADC && op!=DOP_SBB)) set_skipflags(true);
		dyn_read_word(DREG(EA),DREG(TMPW),decode.big_op);
		if (op<=DOP_TEST) {
			if (op==DOP_ADC || op==DOP_SBB) gen_needcarry();
			else set_skipflags(false);
		}
		gen_dop_word(op,decode.big_op,DREG(TMPW),rm_reg);
		if (op!=DOP_CMP) dyn_write_word_release(DREG(EA),DREG(TMPW),decode.big_op);
		else gen_releasereg(DREG(EA));
		gen_releasereg(DREG(TMPW));
	} else {
		if (op<=DOP_TEST) {
			if (op==DOP_ADC || op==DOP_SBB) gen_needcarry();
			else gen_discardflags();
		}
		gen_dop_word(op,decode.big_op,&DynRegs[decode.modrm.rm],rm_reg);
	}
}

static void dyn_imul_gvev(Bitu immsize) {
	dyn_get_modrm();DynReg * src;
	DynReg * rm_reg=&DynRegs[decode.modrm.reg];
	if (decode.modrm.mod<3) {
		dyn_fill_ea();dyn_read_word_release(DREG(EA),DREG(TMPW),decode.big_op);
		src=DREG(TMPW);
	} else {
		src=&DynRegs[decode.modrm.rm];
	}
	gen_needflags();
	switch (immsize) {
	case 0:gen_imul_word(decode.big_op,rm_reg,src);break;
	case 1:gen_imul_word_imm(decode.big_op,rm_reg,src,(int8_t)decode_fetchb());break;
	case 2:gen_imul_word_imm(decode.big_op,rm_reg,src,(int16_t)decode_fetchw());break;
	case 4:gen_imul_word_imm(decode.big_op,rm_reg,src,(int32_t)decode_fetchd());break;
	}
	gen_releasereg(DREG(TMPW));
}

static void dyn_dop_gvev(DualOps op) {
	dyn_get_modrm();
	DynReg * rm_reg=&DynRegs[decode.modrm.reg];
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		if ((op<=DOP_TEST) && (op!=DOP_ADC && op!=DOP_SBB)) set_skipflags(true);
		dyn_read_word_release(DREG(EA),DREG(TMPW),decode.big_op);
		if (op<=DOP_TEST) {
			if (op==DOP_ADC || op==DOP_SBB) gen_needcarry();
			else set_skipflags(false);
		}
		gen_dop_word(op,decode.big_op,rm_reg,DREG(TMPW));
		gen_releasereg(DREG(TMPW));
	} else {
		if (op<=DOP_TEST) {
			if (op==DOP_ADC || op==DOP_SBB) gen_needcarry();
			else gen_discardflags();
		}
		gen_dop_word(op,decode.big_op,rm_reg,&DynRegs[decode.modrm.rm]);
	}
}

static void dyn_mov_evgv(void) {
	dyn_get_modrm();
	DynReg * rm_reg=&DynRegs[decode.modrm.reg];
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		dyn_write_word_release(DREG(EA),rm_reg,decode.big_op);
	} else {
		gen_dop_word(DOP_MOV,decode.big_op,&DynRegs[decode.modrm.rm],rm_reg);
	}
}

static void dyn_mov_gvev(void) {
	dyn_get_modrm();
	DynReg * rm_reg=&DynRegs[decode.modrm.reg];
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		dyn_read_word_release(DREG(EA),rm_reg,decode.big_op);
	} else {
		gen_dop_word(DOP_MOV,decode.big_op,rm_reg,&DynRegs[decode.modrm.rm]);
	}
}
static void dyn_mov_eviv(void) {
	dyn_get_modrm();
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		gen_call_write(DREG(EA),decode.big_op ? decode_fetchd() : decode_fetchw(),decode.big_op?4:2);
#ifdef DYN_NON_RECURSIVE_PAGEFAULT
		dyn_check_pagefault();
#endif
		dyn_check_bool_exception_al();
	} else {
		gen_dop_word_imm(DOP_MOV,decode.big_op,&DynRegs[decode.modrm.rm],decode.big_op ? decode_fetchd() : decode_fetchw());
	}
}

static void dyn_mov_ev_gb(bool sign) {
	dyn_get_modrm();DynReg * rm_reg=&DynRegs[decode.modrm.reg];
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		dyn_read_byte_release(DREG(EA),DREG(TMPB),false);
		gen_extend_byte(sign,decode.big_op,rm_reg,DREG(TMPB),0);
		gen_releasereg(DREG(TMPB));
	} else {
		gen_extend_byte(sign,decode.big_op,rm_reg,&DynRegs[decode.modrm.rm&3],decode.modrm.rm&4);
	}
}

static void dyn_mov_ev_gw(bool sign) {
	if (!decode.big_op) {
		dyn_mov_gvev();
		return;
	}
	dyn_get_modrm();DynReg * rm_reg=&DynRegs[decode.modrm.reg];
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		dyn_read_word_release(DREG(EA),DREG(TMPW),false);
		gen_extend_word(sign,rm_reg,DREG(TMPW));
		gen_releasereg(DREG(TMPW));
	} else {
		gen_extend_word(sign,rm_reg,&DynRegs[decode.modrm.rm]);
	}
}

static void dyn_cmpxchg_evgv(void) {
	dyn_get_modrm();
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		dyn_read_word(DREG(EA),DREG(TMPW),decode.big_op);
		// update flags after write
		gen_protectflags();
	} else {
		gen_dop_word(DOP_MOV,decode.big_op,DREG(TMPW),&DynRegs[decode.modrm.rm]);
		// immediate dest can't fault, update flags from initial cmp
		gen_discardflags();
	}
	gen_dop_word(DOP_MOV,decode.big_op,DREG(TMPB),&DynRegs[decode.modrm.reg]);
	// TMPB=src,TMPW=temp
	gen_dop_word(DOP_CMP,decode.big_op,DREG(EAX),DREG(TMPW));
	uint8_t * branch=gen_create_branch(BR_Z);
	// if eax!=temp: TMPB=temp
	gen_dop_word(DOP_MOV,decode.big_op,DREG(TMPB),DREG(TMPW));
	gen_fill_branch(branch);
	// dest=TMPB,eax=TMPW
	if (decode.modrm.mod<3) {
		dyn_write_word_release(DREG(EA),DREG(TMPB),decode.big_op);
		gen_releasereg(DREG(TMPB));
		// safe to update flags now
		gen_discardflags();
		gen_dop_word(DOP_CMP,decode.big_op,DREG(EAX),DREG(TMPW));
		gen_dop_word(DOP_MOV,decode.big_op,DREG(EAX),DREG(TMPW));
		gen_releasereg(DREG(TMPW));
		return;
	}
	// if eax and dest are same register, skip eax output
	if (decode.modrm.rm != G_EAX)
		gen_dop_word(DOP_MOV,decode.big_op,DREG(EAX),DREG(TMPW));
	gen_releasereg(DREG(TMPW));
	gen_dop_word(DOP_MOV,decode.big_op,&DynRegs[decode.modrm.rm],DREG(TMPB));
	gen_releasereg(DREG(TMPB));
}

static void dyn_dshift_ev_gv(bool left,bool immediate) {
	dyn_get_modrm();
	DynReg * rm_reg=&DynRegs[decode.modrm.reg];
	DynReg * ea_reg;
	if (decode.modrm.mod<3) {
		dyn_fill_ea();ea_reg=DREG(TMPW);
		dyn_read_word(DREG(EA),DREG(TMPW),decode.big_op);
	} else ea_reg=&DynRegs[decode.modrm.rm];
	gen_needflags();
	if (immediate) gen_dshift_imm(decode.big_op,left,ea_reg,rm_reg,decode_fetchb());
	else gen_dshift_cl(decode.big_op,left,ea_reg,rm_reg,DREG(ECX));
	if (decode.modrm.mod<3) {
		dyn_write_word_release(DREG(EA),DREG(TMPW),decode.big_op);
		gen_releasereg(DREG(TMPW));
	}
}


static DualOps grp1_table[8]={DOP_ADD,DOP_OR,DOP_ADC,DOP_SBB,DOP_AND,DOP_SUB,DOP_XOR,DOP_CMP};
static void dyn_grp1_eb_ib(void) {
	dyn_get_modrm();
	DualOps op=grp1_table[decode.modrm.reg];
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		if ((op<=DOP_TEST) && (op!=DOP_ADC && op!=DOP_SBB)) set_skipflags(true);
		dyn_read_byte(DREG(EA),DREG(TMPB),false);
		if (op<=DOP_TEST) {
			if (op==DOP_ADC || op==DOP_SBB) gen_needcarry();
			else set_skipflags(false);
		}
		gen_dop_byte_imm(op,DREG(TMPB),0,decode_fetchb());
		if (op!=DOP_CMP) dyn_write_byte_release(DREG(EA),DREG(TMPB),false);
		else gen_releasereg(DREG(EA));
		gen_releasereg(DREG(TMPB));
	} else {
		if (op<=DOP_TEST) {
			if (op==DOP_ADC || op==DOP_SBB) gen_needcarry();
			else gen_discardflags();
		}
		dyn_dop_byte_imm(op,&DynRegs[decode.modrm.rm&3],decode.modrm.rm&4);
	}
}

static void dyn_grp1_ev_ivx(bool withbyte) {
	dyn_get_modrm();
	DualOps op=grp1_table[decode.modrm.reg];
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		if ((op<=DOP_TEST) && (op!=DOP_ADC && op!=DOP_SBB)) set_skipflags(true);
		dyn_read_word(DREG(EA),DREG(TMPW),decode.big_op);
		if (op<=DOP_TEST) {
			if (op==DOP_ADC || op==DOP_SBB) gen_needcarry();
			else set_skipflags(false);
		}
		if (!withbyte) {
			dyn_dop_word_imm(op,decode.big_op,DREG(TMPW));
		} else {
			gen_dop_word_imm(op,decode.big_op,DREG(TMPW),(int8_t)decode_fetchb());
		}
		if (op!=DOP_CMP) dyn_write_word_release(DREG(EA),DREG(TMPW),decode.big_op);
		else gen_releasereg(DREG(EA));
		gen_releasereg(DREG(TMPW));
	} else {
		if (op<=DOP_TEST) {
			if (op==DOP_ADC || op==DOP_SBB) gen_needcarry();
			else gen_discardflags();
		}
		if (!withbyte) {
			dyn_dop_word_imm(op,decode.big_op,&DynRegs[decode.modrm.rm]);
		} else {
			gen_dop_word_imm(op,decode.big_op,&DynRegs[decode.modrm.rm],(int8_t)decode_fetchb());
		}
	}
}

enum grp2_types {
	grp2_1,grp2_imm,grp2_cl,
};

static void dyn_grp2_eb(grp2_types type) {
	dyn_get_modrm();DynReg * src;uint8_t src_i;
	if (decode.modrm.mod<3) {
		dyn_fill_ea();dyn_read_byte(DREG(EA),DREG(TMPB),false);
		src=DREG(TMPB);
		src_i=0;
	} else {
		src=&DynRegs[decode.modrm.rm&3];
		src_i=decode.modrm.rm&4;
	}
	switch (type) {
	case grp2_1:
		/* rotates (first 4 ops) alter cf/of only; shifts (last 4 ops) alter all flags */
		if (decode.modrm.reg < 4) gen_needflags();
		else gen_discardflags();
		gen_shift_byte_imm(decode.modrm.reg,src,src_i,1);
		break;
	case grp2_imm: {
		uint8_t imm=decode_fetchb();
		if (imm) {
			/* rotates (first 4 ops) alter cf/of only; shifts (last 4 ops) alter all flags */
			if (decode.modrm.reg < 4) gen_needflags();
			else gen_discardflags();
			gen_shift_byte_imm(decode.modrm.reg,src,src_i,imm);
		} else return;
		}
		break;
	case grp2_cl:
		gen_needflags();	/* flags must not be changed on ecx==0 */
		gen_shift_byte_cl (decode.modrm.reg,src,src_i,DREG(ECX));
		break;
	}
	if (decode.modrm.mod<3) {
		dyn_write_byte_release(DREG(EA),src,false);
		gen_releasereg(src);
	}
}

static void dyn_grp2_ev(grp2_types type) {
	dyn_get_modrm();DynReg * src;
	if (decode.modrm.mod<3) {
		dyn_fill_ea();dyn_read_word(DREG(EA),DREG(TMPW),decode.big_op);
		src=DREG(TMPW);
	} else {
		src=&DynRegs[decode.modrm.rm];
	}
	switch (type) {
	case grp2_1:
		/* rotates (first 4 ops) alter cf/of only; shifts (last 4 ops) alter all flags */
		if (decode.modrm.reg < 4) gen_needflags();
		else gen_discardflags();
		gen_shift_word_imm(decode.modrm.reg,decode.big_op,src,1);
		break;
	case grp2_imm: {
		Bitu val;
		if (decode_fetchb_imm(val)) {
			if (decode.modrm.reg < 4) gen_needflags();
			else gen_discardflags();
			gen_dop_byte_imm_mem(DOP_MOV,DREG(TMPB),0,(void*)val);
			gen_shift_word_cl(decode.modrm.reg,decode.big_op,src,DREG(TMPB));
			gen_releasereg(DREG(TMPB));
			break;
		}
		uint8_t imm=(uint8_t)val;
		if (imm) {
			/* rotates (first 4 ops) alter cf/of only; shifts (last 4 ops) alter all flags */
			if (decode.modrm.reg < 4) gen_needflags();
			else gen_discardflags();
			gen_shift_word_imm(decode.modrm.reg,decode.big_op,src,imm);
		} else return;
		}
		break;
	case grp2_cl:
		gen_needflags();	/* flags must not be changed on ecx==0 */
		gen_shift_word_cl (decode.modrm.reg,decode.big_op,src,DREG(ECX));
		break;
	}
	if (decode.modrm.mod<3) {
		dyn_write_word_release(DREG(EA),src,decode.big_op);
		gen_releasereg(src);
	}
}

static void dyn_grp3_eb(void) {
	dyn_get_modrm();DynReg * src;uint8_t src_i;
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		if ((decode.modrm.reg==0) || (decode.modrm.reg==3)) set_skipflags(true);
		dyn_read_byte(DREG(EA),DREG(TMPB),false);
		src=DREG(TMPB);src_i=0;
	} else {
		src=&DynRegs[decode.modrm.rm&3];
		src_i=decode.modrm.rm&4;
	}
	switch (decode.modrm.reg) {
	case 0x0:	/* test eb,ib */
		set_skipflags(false);gen_dop_byte_imm(DOP_TEST,src,src_i,decode_fetchb());
		goto skipsave;
	case 0x2:	/* NOT Eb */
		gen_sop_byte(SOP_NOT,src,src_i);
		break;
	case 0x3:	/* NEG Eb */
		set_skipflags(false);gen_sop_byte(SOP_NEG,src,src_i);
		break;
	case 0x4:	/* mul Eb */
		gen_needflags();gen_mul_byte(false,DREG(EAX),src,src_i);
		goto skipsave;
	case 0x5:	/* imul Eb */
		gen_needflags();gen_mul_byte(true,DREG(EAX),src,src_i);
		goto skipsave;
	case 0x6:	/* div Eb */
	case 0x7:	/* idiv Eb */
		/* EAX could be used, so precache it */
		if (decode.modrm.mod==3)
			gen_dop_byte(DOP_MOV,DREG(TMPB),0,&DynRegs[decode.modrm.rm&3],decode.modrm.rm&4);
		gen_releasereg(DREG(EAX));
		gen_call_function((decode.modrm.reg==6) ? (void *)&dyn_helper_divb : (void *)&dyn_helper_idivb,
			"%Rd%Drd",DREG(TMPB),DREG(TMPB));
		dyn_check_bool_exception(DREG(TMPB));
		goto skipsave;
	}
	/* Save the result if memory op */
	if (decode.modrm.mod<3) dyn_write_byte_release(DREG(EA),src,false);
skipsave:
	gen_releasereg(DREG(TMPB));gen_releasereg(DREG(EA));
}

static void dyn_grp3_ev(void) {
	dyn_get_modrm();DynReg * src;
	if (decode.modrm.mod<3) {
		dyn_fill_ea();src=DREG(TMPW);
		if ((decode.modrm.reg==0) || (decode.modrm.reg==3)) set_skipflags(true);
		dyn_read_word(DREG(EA),DREG(TMPW),decode.big_op);
	} else src=&DynRegs[decode.modrm.rm];
	switch (decode.modrm.reg) {
	case 0x0:	/* test ev,iv */
		set_skipflags(false);gen_dop_word_imm(DOP_TEST,decode.big_op,src,decode.big_op ? decode_fetchd() : decode_fetchw());
		goto skipsave;
	case 0x2:	/* NOT Ev */
		gen_sop_word(SOP_NOT,decode.big_op,src);
		break;
	case 0x3:	/* NEG Eb */
		set_skipflags(false);gen_sop_word(SOP_NEG,decode.big_op,src);
		break;
	case 0x4:	/* mul Eb */
		gen_needflags();gen_mul_word(false,DREG(EAX),DREG(EDX),decode.big_op,src);
		goto skipsave;
	case 0x5:	/* imul Eb */
		gen_needflags();gen_mul_word(true,DREG(EAX),DREG(EDX),decode.big_op,src);
		goto skipsave;
	case 0x6:	/* div Eb */
	case 0x7:	/* idiv Eb */
		/* EAX could be used, so precache it */
		if (decode.modrm.mod==3)
			gen_dop_word(DOP_MOV,decode.big_op,DREG(TMPW),&DynRegs[decode.modrm.rm]);
		gen_releasereg(DREG(EAX));gen_releasereg(DREG(EDX));
		void * func=(decode.modrm.reg==6) ?
			(decode.big_op ? (void *)&dyn_helper_divd : (void *)&dyn_helper_divw) :
			(decode.big_op ? (void *)&dyn_helper_idivd : (void *)&dyn_helper_idivw);
		gen_call_function(func,"%Rd%Drd",DREG(TMPB),DREG(TMPW));
		dyn_check_bool_exception(DREG(TMPB));
		gen_releasereg(DREG(TMPB));
		goto skipsave;
	}
	/* Save the result if memory op */
	if (decode.modrm.mod<3) dyn_write_word_release(DREG(EA),src,decode.big_op);
skipsave:
	gen_releasereg(DREG(TMPW));gen_releasereg(DREG(EA));
}

static void dyn_mov_ev_seg(void) {
	dyn_get_modrm();
	gen_load_host(&Segs.val[decode.modrm.reg],DREG(TMPW),2);
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		dyn_write_word_release(DREG(EA),DREG(TMPW),false);
	} else {
		gen_dop_word(DOP_MOV,decode.big_op,&DynRegs[decode.modrm.rm],DREG(TMPW));
	}
	gen_releasereg(DREG(TMPW));
}

static void dyn_load_seg(SegNames seg,DynReg * src) {
	dyn_call_function_pagefault_check((void *)&CPU_SetSegGeneral,"%Rd%Id%Drw",DREG(TMPB),seg,src);
	dyn_check_bool_exception(DREG(TMPB));
	gen_releasereg(DREG(TMPB));
	gen_releasereg(&DynRegs[G_ES+seg]);
}

static void dyn_load_seg_off_ea(SegNames seg) {
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		gen_lea(DREG(TMPB),DREG(EA),0,0,decode.big_op ? 4:2);
		dyn_read_word(DREG(TMPB),DREG(TMPB),false);
		dyn_read_word_release(DREG(EA),DREG(TMPW),decode.big_op);
		dyn_load_seg(seg,DREG(TMPB));gen_releasereg(DREG(TMPB));
		gen_dop_word(DOP_MOV,decode.big_op,&DynRegs[decode.modrm.reg],DREG(TMPW));
		gen_releasereg(DREG(TMPW));
	} else {
		IllegalOption("dyn_load_seg_off_ea");
	}
}

static void dyn_mov_seg_ev(void) {
	dyn_get_modrm();
	SegNames seg=(SegNames)decode.modrm.reg;
	if (GCC_UNLIKELY(seg==cs)) IllegalOption("dyn_mov_seg_ev");
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		dyn_read_word(DREG(EA),DREG(EA),false);
		dyn_load_seg(seg,DREG(EA));
		gen_releasereg(DREG(EA));
	} else {
		dyn_load_seg(seg,&DynRegs[decode.modrm.rm]);
	}
}

static void dyn_push_seg(SegNames seg) {
	gen_load_host(&Segs.val[seg],DREG(TMPW),2);
	dyn_push(DREG(TMPW));
	gen_releasereg(DREG(TMPW));
}

static void dyn_pop_seg(SegNames seg) {
	gen_releasereg(DREG(ESP));
	dyn_call_function_pagefault_check((void *)&CPU_PopSeg,"%Rd%Id%Id",DREG(TMPB),seg,decode.big_op);
	dyn_check_bool_exception(DREG(TMPB));
	gen_releasereg(DREG(TMPB));
	gen_releasereg(&DynRegs[G_ES+seg]);
	gen_releasereg(DREG(ESP));
}

static void dyn_pop_ev(void) {
#ifdef DYN_NON_RECURSIVE_PAGEFAULT
	dyn_save_stack_for_pagefault();
#endif
	dyn_pop(DREG(TMPW));
	dyn_get_modrm();
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
//		dyn_write_word_release(DREG(EA),DREG(TMPW),decode.big_op);
		if (decode.big_op) dyn_call_function_pagefault_check((void *)&mem_writed_inline,"%Drd%Dd",DREG(EA),DREG(TMPW))
		else dyn_call_function_pagefault_check((void *)&mem_writew_inline,"%Drd%Dd",DREG(EA),DREG(TMPW))
	} else {
		gen_dop_word(DOP_MOV,decode.big_op,&DynRegs[decode.modrm.rm],DREG(TMPW));
	}
	gen_releasereg(DREG(TMPW));
}

static void dyn_enter(void) {
	gen_releasereg(DREG(ESP));
	gen_releasereg(DREG(EBP));
	Bitu bytes=decode_fetchw();
	Bitu level=decode_fetchb();
	dyn_call_function_pagefault_check((void *)&CPU_ENTER,"%Id%Id%Id",decode.big_op,bytes,level);
}

static void dyn_leave(void) {
	gen_protectflags();
#ifdef DYN_NON_RECURSIVE_PAGEFAULT
	dyn_save_stack_for_pagefault();
#endif
	gen_dop_word_var(DOP_MOV,true,DREG(TMPW),&cpu.stack.mask);
	gen_sop_word(SOP_NOT,true,DREG(TMPW));
	gen_dop_word(DOP_AND,true,DREG(ESP),DREG(TMPW));
	gen_dop_word(DOP_MOV,true,DREG(TMPW),DREG(EBP));
	gen_dop_word_var(DOP_AND,true,DREG(TMPW),&cpu.stack.mask);
	gen_dop_word(DOP_OR,true,DREG(ESP),DREG(TMPW));
	dyn_pop(DREG(EBP),false);
	gen_releasereg(DREG(TMPW));
}

static void dyn_segprefix(SegNames seg) {
//	if (GCC_UNLIKELY((Bitu)(decode.segprefix))) IllegalOption("dyn_segprefix");
	decode.segprefix=&DynRegs[G_ES+seg];
}

static void dyn_closeblock(void) {
	//Shouldn't create empty block normally but let's do it like this
	gen_protectflags();
	dyn_fill_blocks();
	cache_closeblock();
}

static void dyn_normal_exit(BlockReturn code) {
	gen_protectflags();
	dyn_reduce_cycles();
	dyn_set_eip_last();
	dyn_save_critical_regs();
	gen_return(code);
	dyn_closeblock();
}

static void dyn_exit_link(Bits eip_change) {
	gen_protectflags();
	gen_dop_word_imm(DOP_ADD,decode.big_op,DREG(EIP),(decode.code-decode.code_start)+eip_change);
	dyn_reduce_cycles();
	dyn_save_critical_regs();
	gen_jmp_ptr(&decode.block->link[0].to,offsetof(CacheBlock,cache.xstart));
	dyn_closeblock();
}

static void dyn_branched_exit(BranchTypes btype,int32_t eip_add) {
	Bitu eip_base=decode.code-decode.code_start;
 	gen_needflags();
 	gen_protectflags();
	dyn_save_noncritical_regs();
	gen_releasereg(DREG(FLAGS));
	gen_releasereg(DREG(EIP));

	gen_preloadreg(DREG(CYCLES));
	gen_preloadreg(DREG(EIP));
	DynReg save_cycles,save_eip;
	dyn_saveregister(DREG(CYCLES),&save_cycles);
	dyn_saveregister(DREG(EIP),&save_eip);
	uint8_t * data=gen_create_branch(btype);

 	/* Branch not taken */
	dyn_reduce_cycles();
 	gen_dop_word_imm(DOP_ADD,decode.big_op,DREG(EIP),eip_base);
	gen_releasereg(DREG(CYCLES));
 	gen_releasereg(DREG(EIP));
 	gen_jmp_ptr(&decode.block->link[0].to,offsetof(CacheBlock,cache.xstart));
 	gen_fill_branch(data);

 	/* Branch taken */
	dyn_restoreregister(&save_cycles,DREG(CYCLES));
	dyn_restoreregister(&save_eip,DREG(EIP));
	dyn_reduce_cycles();
 	gen_dop_word_imm(DOP_ADD,decode.big_op,DREG(EIP),eip_base+eip_add);
	gen_releasereg(DREG(CYCLES));
 	gen_releasereg(DREG(EIP));
 	gen_jmp_ptr(&decode.block->link[1].to,offsetof(CacheBlock,cache.xstart));
 	dyn_closeblock();
}

enum LoopTypes {
	LOOP_NONE,LOOP_NE,LOOP_E,LOOP_JCXZ
};

static void dyn_loop(LoopTypes type) {
	dyn_reduce_cycles();
	Bits eip_add=(int8_t)decode_fetchb();
	Bitu eip_base=decode.code-decode.code_start;
	uint8_t * branch1=0;uint8_t * branch2=0;
	dyn_save_critical_regs();
	switch (type) {
	case LOOP_E:
		gen_needflags();
		branch1=gen_create_branch(BR_NZ);
		break;
	case LOOP_NE:
		gen_needflags();
		branch1=gen_create_branch(BR_Z);
		break;
	}
	gen_protectflags();
	switch (type) {
	case LOOP_E:
	case LOOP_NE:
	case LOOP_NONE:
		gen_sop_word(SOP_DEC,decode.big_addr,DREG(ECX));
		gen_releasereg(DREG(ECX));
		branch2=gen_create_branch(BR_Z);
		break;
	case LOOP_JCXZ:
		gen_dop_word(DOP_TEST,decode.big_addr,DREG(ECX),DREG(ECX));
		gen_releasereg(DREG(ECX));
		branch2=gen_create_branch(BR_NZ);
		break;
	}
	gen_lea(DREG(EIP),DREG(EIP),0,0,eip_base+eip_add);
	gen_releasereg(DREG(EIP));
	gen_jmp_ptr(&decode.block->link[0].to,offsetof(CacheBlock,cache.xstart));
	if (branch1) {
		gen_fill_branch(branch1);
		gen_sop_word(SOP_DEC,decode.big_addr,DREG(ECX));
		gen_releasereg(DREG(ECX));
	}
	/* Branch taken */
	gen_fill_branch(branch2);
	gen_lea(DREG(EIP),DREG(EIP),0,0,eip_base);
	gen_releasereg(DREG(EIP));
	gen_jmp_ptr(&decode.block->link[1].to,offsetof(CacheBlock,cache.xstart));
	dyn_closeblock();
}

static void dyn_ret_near(Bitu bytes) {
	gen_protectflags();
	dyn_reduce_cycles();
#ifdef DYN_NON_RECURSIVE_PAGEFAULT
	dyn_save_stack_for_pagefault();
#endif
	dyn_pop(DREG(EIP));
	if (bytes) gen_dop_word_imm(DOP_ADD,true,DREG(ESP),bytes);
	dyn_save_critical_regs();
	gen_return(BR_Normal);
	dyn_closeblock();
}

static void dyn_call_near_imm(void) {
	Bits imm;
	if (decode.big_op) imm=(int32_t)decode_fetchd();
	else imm=(int16_t)decode_fetchw();
	dyn_set_eip_end(DREG(TMPW));
	dyn_push(DREG(TMPW));
	gen_dop_word_imm(DOP_ADD,decode.big_op,DREG(TMPW),imm);
	if (cpu.code.big) gen_dop_word(DOP_MOV,true,DREG(EIP),DREG(TMPW));
	else gen_extend_word(false,DREG(EIP),DREG(TMPW));
	dyn_reduce_cycles();
	dyn_save_critical_regs();
	gen_jmp_ptr(&decode.block->link[0].to,offsetof(CacheBlock,cache.xstart));
	dyn_closeblock();
}

static void dyn_ret_far(Bitu bytes) {
	gen_protectflags();
	dyn_reduce_cycles();
	dyn_set_eip_last_end(DREG(TMPW));
	dyn_flags_gen_to_host();
	dyn_save_critical_regs();
	dyn_call_function_pagefault_check((void*)&CPU_RET,"%Id%Id%Drd",decode.big_op,bytes,DREG(TMPW));
	gen_return_fast(BR_Normal);
	dyn_closeblock();
}

static void dyn_call_far_imm(void) {
	Bitu sel,off;
	off=decode.big_op ? decode_fetchd() : decode_fetchw();
	sel=decode_fetchw();
	dyn_reduce_cycles();
	dyn_set_eip_last_end(DREG(TMPW));
	dyn_flags_gen_to_host();
	dyn_save_critical_regs();
	dyn_call_function_pagefault_check((void*)&CPU_CALL,"%Id%Id%Id%Drd",decode.big_op,sel,off,DREG(TMPW));
	gen_return_fast(BR_Normal);
	dyn_closeblock();
}

static void dyn_jmp_far_imm(void) {
	Bitu sel,off;
	gen_protectflags();
	off=decode.big_op ? decode_fetchd() : decode_fetchw();
	sel=decode_fetchw();
	dyn_reduce_cycles();
	dyn_set_eip_last_end(DREG(TMPW));
	dyn_flags_gen_to_host();
	dyn_save_critical_regs();
	dyn_call_function_pagefault_check((void*)&CPU_JMP,"%Id%Id%Id%Drd",decode.big_op,sel,off,DREG(TMPW));
	gen_return_fast(BR_Normal);
	dyn_closeblock();
}

static void dyn_iret(void) {
	gen_protectflags();
	dyn_flags_gen_to_host();
	dyn_reduce_cycles();
	dyn_set_eip_last_end(DREG(TMPW));
	dyn_save_critical_regs();
	dyn_call_function_pagefault_check((void*)&CPU_IRET,"%Id%Drd",decode.big_op,DREG(TMPW));
	gen_return_fast(BR_Iret);
	dyn_closeblock();
}

static void dyn_interrupt(Bitu num) {
	gen_protectflags();
	dyn_flags_gen_to_host();
	dyn_reduce_cycles();
	dyn_set_eip_last_end(DREG(TMPW));
	dyn_save_critical_regs();
	dyn_call_function_pagefault_check((void*)&CPU_Interrupt,"%Id%Id%Drd",num,CPU_INT_SOFTWARE,DREG(TMPW));
	gen_return_fast(BR_Normal);
	dyn_closeblock();
}

static void dyn_add_iocheck(Bitu access_size) {
	dyn_call_function_pagefault_check((void *)&CPU_IO_Exception,"%Dw%Id",DREG(EDX),access_size);
	dyn_check_bool_exception_al();
}

static void dyn_add_iocheck_var(uint8_t accessed_port,Bitu access_size) {
	dyn_call_function_pagefault_check((void *)&CPU_IO_Exception,"%Id%Id",accessed_port,access_size);
	dyn_check_bool_exception_al();
}

static void dyn_xlat(void) {
	gen_extend_byte(false,true,DREG(TMPW),DREG(EAX),0);
	gen_lea(DREG(TMPW),DREG(TMPW),DREG(EBX),0,0);
	if (!decode.big_addr) gen_extend_word(false,DREG(TMPW),DREG(TMPW));
	gen_lea(DREG(TMPW),DREG(TMPW),decode.segprefix ? decode.segprefix : DREG(DS),0,0);
	dyn_read_byte_release(DREG(TMPW),DREG(EAX),false);
}

static void dyn_larlsl(bool islar) {
	dyn_get_modrm();
	gen_protectflags();
	// 32-bit code = protected mode, no need to check
	if (!cpu.code.big) {
		// LAR/LSL is undefined in real/v86 mode
		gen_load_host(&cpu.pmode,DREG(TMPW),1);
		gen_dop_word_imm(DOP_SUB,true,DREG(TMPW),1); // tmpw = cpu.pmode ? 0:0xffffffff
		gen_dop_word(DOP_OR,true,DREG(TMPW),DREG(FLAGS));
		gen_dop_word_imm(DOP_TEST,true,DREG(TMPW),FLAG_VM);
		gen_releasereg(DREG(TMPW));
		DynState s;
		dyn_savestate(&s);
		uint8_t *is_pmode = gen_create_branch(BR_Z);
		gen_call_function((void*)CPU_PrepareException,"%Id%Id",EXCEPTION_UD,0);
		dyn_check_bool_exception_al();
		gen_fill_branch(is_pmode);
		dyn_loadstate(&s);
	}
	void *func = islar ? (void*)CPU_LAR : (void*)CPU_LSL;
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		dyn_read_word_release(DREG(EA),DREG(TMPW),false);
		dyn_flags_gen_to_host();
		dyn_call_function_pagefault_check(func,"%Drw%Ip",DREG(TMPW),&core_dyn.readdata);
	} else {
		dyn_flags_gen_to_host();
		dyn_call_function_pagefault_check(func,"%Dw%Ip",&DynRegs[decode.modrm.rm],&core_dyn.readdata);
	}
	dyn_flags_host_to_gen();
	gen_needflags();
	gen_preloadreg(&DynRegs[decode.modrm.reg]);
	uint8_t *br = gen_create_branch(BR_NZ);
	gen_mov_host(&core_dyn.readdata, &DynRegs[decode.modrm.reg], decode.big_op?4:2);
	gen_fill_branch(br);
}

#ifdef X86_DYNFPU_DH_ENABLED
#include "dyn_fpu_dh.h"
#define dh_fpu_startup() {		\
	fpu_used=true;				\
	gen_protectflags();			\
	gen_load_host(&dyn_dh_fpu.state_used,DREG(TMPB),1);	\
	gen_dop_byte(DOP_TEST,DREG(TMPB),0,DREG(TMPB),0);		\
	gen_releasereg(DREG(TMPB));							\
	save_info[used_save_info].branch_pos=gen_create_branch_long(BR_Z);		\
	dyn_savestate(&save_info[used_save_info].state);	\
	save_info[used_save_info].return_pos=cache.pos;		\
	save_info[used_save_info].type=fpu_restore;			\
	used_save_info++;									\
}
#endif
#include "dyn_fpu.h"

#ifdef X86_DYNREC_MMX_ENABLED
#include "mmx_gen.h"
#define dyn_mmx_check() if ((dyn_dh_fpu.dh_fpu_enabled) && (!fpu_used)) {dh_fpu_startup();}
#endif

static CacheBlock * CreateCacheBlock(CodePageHandler * codepage,PhysPt start,Bitu max_opcodes) {
	Bits i;

	cache_remap_rw();

/* Init a load of variables */
	decode.code_start=start;
	decode.code=start;
	Bitu cycles=0;
	decode.page.code=codepage;
	decode.page.index=start&4095;
	decode.page.wmap=codepage->write_map;
	decode.page.invmap=codepage->invalidation_map;
	decode.page.first=start >> 12;
	decode.active_block=decode.block=cache_openblock();
	decode.block->page.start=decode.page.index;
	codepage->AddCacheBlock(decode.block);

	for (i=0;i<G_MAX;i++) {
		DynRegs[i].flags&=~(DYNFLG_ACTIVE|DYNFLG_CHANGED);
		DynRegs[i].genreg=0;
	}
	gen_reinit();
	gen_save_host_direct(&cache.block.running,(Bitu)decode.block);
	/* Start with the cycles check */
	gen_protectflags();
	gen_dop_word(DOP_TEST,true,DREG(CYCLES),DREG(CYCLES));
	save_info[used_save_info].branch_pos=gen_create_branch_long(BR_LE);
	save_info[used_save_info].type=cycle_check;
	used_save_info++;
	gen_releasereg(DREG(CYCLES));
	decode.cycles=0;
#ifdef X86_DYNFPU_DH_ENABLED
	bool fpu_used=false;
#endif
	while (max_opcodes--) {
#ifdef DYN_NON_RECURSIVE_PAGEFAULT
		if (decoder_pagefault.had_pagefault) goto illegalopcode;
#endif
/* Init prefixes */
		decode.big_addr=cpu.code.big;
		decode.big_op=cpu.code.big;
		decode.segprefix=0;
		decode.rep=REP_NONE;
		decode.cycles++;
		decode.op_start=decode.code;
#ifdef DYN_NON_RECURSIVE_PAGEFAULT
		decode.pf_restore.dword=0;
#endif
restart_prefix:
		Bitu opcode;
		if (!decode.page.invmap) opcode=decode_fetchb();
		else {
			if (decode.page.index<4096) {
				if (GCC_UNLIKELY(decode.page.invmap[decode.page.index]>=4)) goto illegalopcode;
				opcode=decode_fetchb();
			} else {
				opcode=decode_fetchb();
				if (GCC_UNLIKELY(decode.page.invmap && 
					(decode.page.invmap[decode.page.index-1]>=4))) goto illegalopcode;
			}
		}
		switch (opcode) {

		case 0x00:dyn_dop_ebgb(DOP_ADD);break;
		case 0x01:dyn_dop_evgv(DOP_ADD);break;
		case 0x02:dyn_dop_gbeb(DOP_ADD);break;
		case 0x03:dyn_dop_gvev(DOP_ADD);break;
		case 0x04:gen_discardflags();gen_dop_byte_imm(DOP_ADD,DREG(EAX),0,decode_fetchb());break;
		case 0x05:gen_discardflags();dyn_dop_word_imm(DOP_ADD,decode.big_op,DREG(EAX));break;
		case 0x06:dyn_push_seg(es);break;
		case 0x07:dyn_pop_seg(es);break;

		case 0x08:dyn_dop_ebgb(DOP_OR);break;
		case 0x09:dyn_dop_evgv(DOP_OR);break;
		case 0x0a:dyn_dop_gbeb(DOP_OR);break;
		case 0x0b:dyn_dop_gvev(DOP_OR);break;
		case 0x0c:gen_discardflags();gen_dop_byte_imm(DOP_OR,DREG(EAX),0,decode_fetchb());break;
		case 0x0d:gen_discardflags();gen_dop_word_imm(DOP_OR,decode.big_op,DREG(EAX),decode.big_op ? decode_fetchd() :  decode_fetchw());break;
		case 0x0e:dyn_push_seg(cs);break;
		case 0x0f:
		{
			Bitu dual_code=decode_fetchb();
			switch (dual_code) {
			/* LAR */
			case 0x02: dyn_larlsl(true);break;
			/* LSL */
			case 0x03: dyn_larlsl(false);break;
			/* Short conditional jumps */
			case 0x80:case 0x81:case 0x82:case 0x83:case 0x84:case 0x85:case 0x86:case 0x87:	
			case 0x88:case 0x89:case 0x8a:case 0x8b:case 0x8c:case 0x8d:case 0x8e:case 0x8f:	
				dyn_branched_exit((BranchTypes)(dual_code&0xf),
					decode.big_op ? (int32_t)decode_fetchd() : (int16_t)decode_fetchw());	
				goto finish_block;
			/* PUSH/POP FS */
			case 0xa0:dyn_push_seg(fs);break;
			case 0xa1:dyn_pop_seg(fs);break;
			/* SHLD Imm/cl*/
			case 0xa4:dyn_dshift_ev_gv(true,true);break;
			case 0xa5:dyn_dshift_ev_gv(true,false);break;
			/* PUSH/POP GS */
			case 0xa8:dyn_push_seg(gs);break;
			case 0xa9:dyn_pop_seg(gs);break;
			/* SHRD Imm/cl*/
			case 0xac:dyn_dshift_ev_gv(false,true);break;
			case 0xad:dyn_dshift_ev_gv(false,false);break;		
			/* Imul Ev,Gv */
			case 0xaf:dyn_imul_gvev(0);break;
			/* CMPXCHG */
			case 0xb1:
				if (CPU_ArchitectureType<CPU_ARCHTYPE_486OLD) goto illegalopcode;
				dyn_cmpxchg_evgv();break;
			/* LFS,LGS */
			case 0xb4:
				dyn_get_modrm();
				if (GCC_UNLIKELY(decode.modrm.mod==3)) goto illegalopcode;
				dyn_load_seg_off_ea(fs);
				break;
			case 0xb5:
				dyn_get_modrm();
				if (GCC_UNLIKELY(decode.modrm.mod==3)) goto illegalopcode;
				dyn_load_seg_off_ea(gs);
				break;
			/* MOVZX Gv,Eb/Ew */
			case 0xb6:dyn_mov_ev_gb(false);break;
			case 0xb7:dyn_mov_ev_gw(false);break;
			/* MOVSX Gv,Eb/Ew */
			case 0xbe:dyn_mov_ev_gb(true);break;
			case 0xbf:dyn_mov_ev_gw(true);break;

#if defined(X86_DYNREC_MMX_ENABLED) && defined(X86_DYNFPU_DH_ENABLED)
			
			/* OP mm, mm/m64 */
			/* pack/unpacks, compares */
			case 0x60:case 0x61:case 0x62:case 0x63:case 0x64:case 0x65:
			case 0x66:case 0x67:case 0x68:case 0x69:case 0x6a:case 0x6b:
			case 0x74:case 0x75:case 0x76:
			/* mm-directed shifts, add/sub, bitwise, multiplies */
			case 0xd1:case 0xd2:case 0xd3:case 0xd4:case 0xd5:case 0xd8:
			case 0xd9:case 0xdb:case 0xdc:case 0xdd:case 0xdf:case 0xe1:
			case 0xe2:case 0xe5:case 0xe8:case 0xe9:case 0xeb:case 0xec:
			case 0xed:case 0xef:case 0xf1:case 0xf2:case 0xf3:case 0xf5:
			case 0xf8:case 0xf9:case 0xfa:case 0xfc:case 0xfd:case 0xfe:
				dyn_mmx_check(); dyn_mmx_op(dual_code); break;

			/* SHIFT mm, imm8*/
			case 0x71:case 0x72:case 0x73:
				dyn_mmx_check(); dyn_mmx_shift_imm8(dual_code); break;

			/* MOVD mm, r/m32 */
			case 0x6e:dyn_mmx_check(); dyn_mmx_movd_pqed(); break;
			/* MOVQ mm, mm/m64 */
			case 0x6f:dyn_mmx_check(); dyn_mmx_movq_pqqq(); break;
			/* MOVD r/m32, mm */
			case 0x7e:dyn_mmx_check(); dyn_mmx_movd_edpq(); break;
			/* MOVQ mm/m64, mm */
			case 0x7f:dyn_mmx_check(); dyn_mmx_movq_qqpq(); break;
			/* EMMS */
			case 0x77:dyn_mmx_check(); dyn_mmx_emms(); break;
#endif

			default:

#if DYN_LOG
				LOG_MSG("Unhandled dual opcode 0F%02X",dual_code);
#endif
				goto illegalopcode;
			}
		}break;

		case 0x10:dyn_dop_ebgb(DOP_ADC);break;
		case 0x11:dyn_dop_evgv(DOP_ADC);break;
		case 0x12:dyn_dop_gbeb(DOP_ADC);break;
		case 0x13:dyn_dop_gvev(DOP_ADC);break;
		case 0x14:gen_needcarry();gen_dop_byte_imm(DOP_ADC,DREG(EAX),0,decode_fetchb());break;
		case 0x15:gen_needcarry();gen_dop_word_imm(DOP_ADC,decode.big_op,DREG(EAX),decode.big_op ? decode_fetchd() :  decode_fetchw());break;
		case 0x16:dyn_push_seg(ss);break;
		case 0x17:dyn_pop_seg(ss);break;

		case 0x18:dyn_dop_ebgb(DOP_SBB);break;
		case 0x19:dyn_dop_evgv(DOP_SBB);break;
		case 0x1a:dyn_dop_gbeb(DOP_SBB);break;
		case 0x1b:dyn_dop_gvev(DOP_SBB);break;
		case 0x1c:gen_needcarry();gen_dop_byte_imm(DOP_SBB,DREG(EAX),0,decode_fetchb());break;
		case 0x1d:gen_needcarry();gen_dop_word_imm(DOP_SBB,decode.big_op,DREG(EAX),decode.big_op ? decode_fetchd() :  decode_fetchw());break;
		case 0x1e:dyn_push_seg(ds);break;
		case 0x1f:dyn_pop_seg(ds);break;
		case 0x20:dyn_dop_ebgb(DOP_AND);break;
		case 0x21:dyn_dop_evgv(DOP_AND);break;
		case 0x22:dyn_dop_gbeb(DOP_AND);break;
		case 0x23:dyn_dop_gvev(DOP_AND);break;
		case 0x24:gen_discardflags();gen_dop_byte_imm(DOP_AND,DREG(EAX),0,decode_fetchb());break;
		case 0x25:gen_discardflags();dyn_dop_word_imm(DOP_AND,decode.big_op,DREG(EAX));break;
		case 0x26:dyn_segprefix(es);goto restart_prefix;

		case 0x28:dyn_dop_ebgb(DOP_SUB);break;
		case 0x29:dyn_dop_evgv(DOP_SUB);break;
		case 0x2a:dyn_dop_gbeb(DOP_SUB);break;
		case 0x2b:dyn_dop_gvev(DOP_SUB);break;
		case 0x2c:gen_discardflags();gen_dop_byte_imm(DOP_SUB,DREG(EAX),0,decode_fetchb());break;
		case 0x2d:gen_discardflags();gen_dop_word_imm(DOP_SUB,decode.big_op,DREG(EAX),decode.big_op ? decode_fetchd() :  decode_fetchw());break;
		case 0x2e:dyn_segprefix(cs);goto restart_prefix;

		case 0x30:dyn_dop_ebgb(DOP_XOR);break;
		case 0x31:dyn_dop_evgv(DOP_XOR);break;
		case 0x32:dyn_dop_gbeb(DOP_XOR);break;
		case 0x33:dyn_dop_gvev(DOP_XOR);break;
		case 0x34:gen_discardflags();gen_dop_byte_imm(DOP_XOR,DREG(EAX),0,decode_fetchb());break;
		case 0x35:gen_discardflags();gen_dop_word_imm(DOP_XOR,decode.big_op,DREG(EAX),decode.big_op ? decode_fetchd() :  decode_fetchw());break;
		case 0x36:dyn_segprefix(ss);goto restart_prefix;

		case 0x38:dyn_dop_ebgb(DOP_CMP);break;
		case 0x39:dyn_dop_evgv(DOP_CMP);break;
		case 0x3a:dyn_dop_gbeb(DOP_CMP);break;
		case 0x3b:dyn_dop_gvev(DOP_CMP);break;
		case 0x3c:gen_discardflags();gen_dop_byte_imm(DOP_CMP,DREG(EAX),0,decode_fetchb());break;
		case 0x3d:gen_discardflags();gen_dop_word_imm(DOP_CMP,decode.big_op,DREG(EAX),decode.big_op ? decode_fetchd() :  decode_fetchw());break;
		case 0x3e:dyn_segprefix(ds);goto restart_prefix;

		/* INC/DEC general register */
		case 0x40:case 0x41:case 0x42:case 0x43:case 0x44:case 0x45:case 0x46:case 0x47:	
			gen_needcarry();gen_sop_word(SOP_INC,decode.big_op,&DynRegs[opcode&7]);
			break;
		case 0x48:case 0x49:case 0x4a:case 0x4b:case 0x4c:case 0x4d:case 0x4e:case 0x4f:	
			gen_needcarry();gen_sop_word(SOP_DEC,decode.big_op,&DynRegs[opcode&7]);
			break;
		/* PUSH/POP General register */
		case 0x50:case 0x51:case 0x52:case 0x53:case 0x55:case 0x56:case 0x57:	
			dyn_push(&DynRegs[opcode&7]);
			break;
		case 0x54:		/* PUSH SP is special */
			gen_dop_word(DOP_MOV,true,DREG(TMPW),DREG(ESP));
			dyn_push(DREG(TMPW));
			gen_releasereg(DREG(TMPW));
			break;
		case 0x58:case 0x59:case 0x5a:case 0x5b:case 0x5c:case 0x5d:case 0x5e:case 0x5f:	
			dyn_pop(&DynRegs[opcode&7]);
			break;
		case 0x60:		/* PUSHA */
#ifdef DYN_NON_RECURSIVE_PAGEFAULT
			dyn_save_stack_for_pagefault();
#endif
			gen_dop_word(DOP_MOV,true,DREG(TMPW),DREG(ESP));
			for (i=G_EAX;i<=G_EDI;i++) {
				dyn_push_unchecked((i!=G_ESP) ? &DynRegs[i] : DREG(TMPW));
			}
			gen_releasereg(DREG(TMPW));
			break;
		case 0x61:		/* POPA */
#ifdef DYN_NON_RECURSIVE_PAGEFAULT
			dyn_save_stack_for_pagefault();
#endif
			for (i=G_EDI;i>=G_EAX;i--) {
				dyn_pop((i!=G_ESP) ? &DynRegs[i] : DREG(TMPW),false);
			}
			gen_releasereg(DREG(TMPW));
			break;
		//segprefix FS,GS
		case 0x64:dyn_segprefix(fs);goto restart_prefix;
		case 0x65:dyn_segprefix(gs);goto restart_prefix;
		//Push immediates
		//Operand size		
		case 0x66:decode.big_op=!cpu.code.big;goto restart_prefix;
		//Address size		
		case 0x67:decode.big_addr=!cpu.code.big;goto restart_prefix;
		case 0x68:		/* PUSH Iv */
			gen_dop_word_imm(DOP_MOV,decode.big_op,DREG(TMPW),decode.big_op ? decode_fetchd() : decode_fetchw());
			dyn_push(DREG(TMPW));
			gen_releasereg(DREG(TMPW));
			break;
		/* Imul Ivx */
		case 0x69:dyn_imul_gvev(decode.big_op ? 4 : 2);break;
		case 0x6a:		/* PUSH Ibx */
			gen_dop_word_imm(DOP_MOV,true,DREG(TMPW),(int8_t)decode_fetchb());
			dyn_push(DREG(TMPW));
			gen_releasereg(DREG(TMPW));
			break;
		/* Imul Ibx */
		case 0x6b:dyn_imul_gvev(1);break;
		/* Short conditional jumps */
		case 0x70:case 0x71:case 0x72:case 0x73:case 0x74:case 0x75:case 0x76:case 0x77:	
		case 0x78:case 0x79:case 0x7a:case 0x7b:case 0x7c:case 0x7d:case 0x7e:case 0x7f:	
			dyn_branched_exit((BranchTypes)(opcode&0xf),(int8_t)decode_fetchb());	
			goto finish_block;
		/* Group 1 */
		case 0x80:dyn_grp1_eb_ib();break;
		case 0x81:dyn_grp1_ev_ivx(false);break;
		case 0x82:dyn_grp1_eb_ib();break;
		case 0x83:dyn_grp1_ev_ivx(true);break;
		/* TEST Gb,Eb Gv,Ev */
		case 0x84:dyn_dop_gbeb(DOP_TEST);break;
		case 0x85:dyn_dop_gvev(DOP_TEST);break;
		/* XCHG Eb,Gb Ev,Gv */
		case 0x86:dyn_dop_ebgb(DOP_XCHG);break;
		case 0x87:dyn_dop_evgv(DOP_XCHG);break;
		/* MOV e,g and g,e */
		case 0x88:dyn_mov_ebgb();break;
		case 0x89:dyn_mov_evgv();break;
		case 0x8a:dyn_mov_gbeb();break;
		case 0x8b:dyn_mov_gvev();break;
		/* MOV ev,seg */
		case 0x8c:dyn_mov_ev_seg();break;
		/* LEA Gv */
		case 0x8d:
			dyn_get_modrm();
			if (decode.big_op) {
				dyn_fill_ea(false,&DynRegs[decode.modrm.reg]);
			} else {
				dyn_fill_ea(false);
				gen_dop_word(DOP_MOV,decode.big_op,&DynRegs[decode.modrm.reg],DREG(EA));
				gen_releasereg(DREG(EA));
			}
			break;
		/* Mov seg,ev */
		case 0x8e:dyn_mov_seg_ev();break;
		/* POP Ev */
		case 0x8f:dyn_pop_ev();break;
		case 0x90:	//NOP
		case 0x9b:	//WAIT/FWAIT
			break;
		//XCHG ax,reg
		case 0x91:case 0x92:case 0x93:case 0x94:case 0x95:case 0x96:case 0x97:	
			gen_dop_word(DOP_XCHG,decode.big_op,DREG(EAX),&DynRegs[opcode&07]);
			break;
		/* CBW/CWDE */
		case 0x98:
			gen_cbw(decode.big_op,DREG(EAX));
			break;
		/* CWD/CDQ */
		case 0x99:
			gen_cwd(decode.big_op,DREG(EAX),DREG(EDX));
			break;
		/* CALL FAR Ip */
		case 0x9a:dyn_call_far_imm();goto finish_block;
		case 0x9c:		//PUSHF
			gen_protectflags();
			gen_releasereg(DREG(ESP));
			dyn_flags_gen_to_host();
			dyn_call_function_pagefault_check((void *)&CPU_PUSHF,"%Rd%Id",DREG(TMPB),decode.big_op);
			dyn_check_bool_exception(DREG(TMPB));
			gen_releasereg(DREG(TMPB));
			break;
		case 0x9d:		//POPF
			gen_releasereg(DREG(ESP));
			gen_releasereg(DREG(FLAGS));
			dyn_call_function_pagefault_check((void *)&CPU_POPF,"%Rd%Id",DREG(TMPB),decode.big_op);
			dyn_check_bool_exception(DREG(TMPB));
			dyn_flags_host_to_gen();
			gen_releasereg(DREG(TMPB));
			dyn_check_trapflag();
			dyn_check_irqrequest();
			break;
		/* MOV AL,direct addresses */
		case 0xa0:
			gen_lea(DREG(EA),decode.segprefix ? decode.segprefix : DREG(DS),0,0,
				decode.big_addr ? decode_fetchd() : decode_fetchw());
			dyn_read_byte_release(DREG(EA),DREG(EAX),false);
			break;
		/* MOV AX,direct addresses */
		case 0xa1:
			gen_lea(DREG(EA),decode.segprefix ? decode.segprefix : DREG(DS),0,0,
				decode.big_addr ? decode_fetchd() : decode_fetchw());
			dyn_read_word_release(DREG(EA),DREG(EAX),decode.big_op);
			break;
		/* MOV direct address,AL */
		case 0xa2:
			if (decode.big_addr) {
				Bitu val;
				if (decode_fetchd_imm(val)) {
					gen_lea_imm_mem(DREG(EA),decode.segprefix ? decode.segprefix : DREG(DS),(void*)val);
				} else {
					gen_lea(DREG(EA),decode.segprefix ? decode.segprefix : DREG(DS),0,0,(Bits)val);
				}
				dyn_write_byte_release(DREG(EA),DREG(EAX),false);
			} else {
				gen_lea(DREG(EA),decode.segprefix ? decode.segprefix : DREG(DS),0,0,decode_fetchw());
				dyn_write_byte_release(DREG(EA),DREG(EAX),false);
			}
			break;
		/* MOV direct addresses,AX */
		case 0xa3:
			gen_lea(DREG(EA),decode.segprefix ? decode.segprefix : DREG(DS),0,0,
				decode.big_addr ? decode_fetchd() : decode_fetchw());
			dyn_write_word_release(DREG(EA),DREG(EAX),decode.big_op);
			break;
		/* MOVSB/W/D*/
		case 0xa4:dyn_string(STR_MOVSB);break;
		case 0xa5:dyn_string(decode.big_op ? STR_MOVSD : STR_MOVSW);break;
		/* TEST AL,AX Imm */
		case 0xa8:gen_discardflags();gen_dop_byte_imm(DOP_TEST,DREG(EAX),0,decode_fetchb());break;
		case 0xa9:gen_discardflags();gen_dop_word_imm(DOP_TEST,decode.big_op,DREG(EAX),decode.big_op ? decode_fetchd() :  decode_fetchw());break;
		/* STOSB/W/D*/
		case 0xaa:dyn_string(STR_STOSB);break;
		case 0xab:dyn_string(decode.big_op ? STR_STOSD : STR_STOSW);break;
		/* LODSB/W/D*/
		case 0xac:dyn_string(STR_LODSB);break;
		case 0xad:dyn_string(decode.big_op ? STR_LODSD : STR_LODSW);break;
		//Mov Byte reg,Imm byte
		case 0xb0:case 0xb1:case 0xb2:case 0xb3:case 0xb4:case 0xb5:case 0xb6:case 0xb7:	
			gen_dop_byte_imm(DOP_MOV,&DynRegs[opcode&3],opcode&4,decode_fetchb());
			break;
		//Mov word reg imm byte,word,
		case 0xb8:case 0xb9:case 0xba:case 0xbb:case 0xbc:case 0xbd:case 0xbe:case 0xbf:	
			if (decode.big_op) {
				dyn_dop_word_imm(DOP_MOV,decode.big_op,&DynRegs[opcode&7]);break;
			} else {
				gen_dop_word_imm(DOP_MOV,decode.big_op,&DynRegs[opcode&7],decode_fetchw());break;
			}
			break;
		//GRP2 Eb/Ev,Ib
		case 0xc0:dyn_grp2_eb(grp2_imm);break;
		case 0xc1:dyn_grp2_ev(grp2_imm);break;
		//RET near Iw / Ret
		case 0xc2:dyn_ret_near(decode_fetchw());goto finish_block;
		case 0xc3:dyn_ret_near(0);goto finish_block;
		//LES
		case 0xc4:
			dyn_get_modrm();
			if (GCC_UNLIKELY(decode.modrm.mod==3)) goto illegalopcode;
			dyn_load_seg_off_ea(es);
			break;
		//LDS
		case 0xc5:
			dyn_get_modrm();
			if (GCC_UNLIKELY(decode.modrm.mod==3)) goto illegalopcode;
			dyn_load_seg_off_ea(ds);
			break;
		// MOV Eb/Ev,Ib/Iv
		case 0xc6:dyn_mov_ebib();break;
		case 0xc7:dyn_mov_eviv();break;
		//ENTER and LEAVE
		case 0xc8:dyn_enter();break;
		case 0xc9:dyn_leave();break;
		//RET far Iw / Ret
		case 0xca:dyn_ret_far(decode_fetchw());goto finish_block;
		case 0xcb:dyn_ret_far(0);goto finish_block;
		/* Interrupt */
#if !(C_DEBUG)
		case 0xcd:dyn_interrupt(decode_fetchb());goto finish_block;
#endif
		/* IRET */
		case 0xcf:dyn_iret();goto finish_block;

		//GRP2 Eb/Ev,1
		case 0xd0:dyn_grp2_eb(grp2_1);break;
		case 0xd1:dyn_grp2_ev(grp2_1);break;
		//GRP2 Eb/Ev,CL
		case 0xd2:dyn_grp2_eb(grp2_cl);break;
		case 0xd3:dyn_grp2_ev(grp2_cl);break;
		// SALC
		case 0xd6:gen_needflags();gen_protectflags();gen_dop_byte(DOP_SBB,DREG(EAX),0,DREG(EAX),0);break;
		// XLAT
		case 0xd7:dyn_xlat();break;
		//FPU
#ifdef CPU_FPU
		case 0xd8:
#ifdef X86_DYNFPU_DH_ENABLED
			if (dyn_dh_fpu.dh_fpu_enabled) {
				if (fpu_used) dh_fpu_esc0();
				else {
					dh_fpu_startup();
					dh_fpu_esc0();
				}
			} else
#endif
			dyn_fpu_esc0();
			break;
		case 0xd9:
#ifdef X86_DYNFPU_DH_ENABLED
			if (dyn_dh_fpu.dh_fpu_enabled) {
				if (fpu_used) dh_fpu_esc1();
				else {
					dh_fpu_startup();
					dh_fpu_esc1();
				}
			} else
#endif
			dyn_fpu_esc1();
			break;
		case 0xda:
#ifdef X86_DYNFPU_DH_ENABLED
			if (dyn_dh_fpu.dh_fpu_enabled) {
				if (fpu_used) dh_fpu_esc2();
				else {
					dh_fpu_startup();
					dh_fpu_esc2();
				}
			} else
#endif
			dyn_fpu_esc2();
			break;
		case 0xdb:
#ifdef X86_DYNFPU_DH_ENABLED
			if (dyn_dh_fpu.dh_fpu_enabled) {
				if (fpu_used) dh_fpu_esc3();
				else {
					dh_fpu_startup();
					dh_fpu_esc3();
				}
			} else
#endif
			dyn_fpu_esc3();
			break;
		case 0xdc:
#ifdef X86_DYNFPU_DH_ENABLED
			if (dyn_dh_fpu.dh_fpu_enabled) {
				if (fpu_used) dh_fpu_esc4();
				else {
					dh_fpu_startup();
					dh_fpu_esc4();
				}
			} else
#endif
			dyn_fpu_esc4();
			break;
		case 0xdd:
#ifdef X86_DYNFPU_DH_ENABLED
			if (dyn_dh_fpu.dh_fpu_enabled) {
				if (fpu_used) dh_fpu_esc5();
				else {
					dh_fpu_startup();
					dh_fpu_esc5();
				}
			} else
#endif
			dyn_fpu_esc5();
			break;
		case 0xde:
#ifdef X86_DYNFPU_DH_ENABLED
			if (dyn_dh_fpu.dh_fpu_enabled) {
				if (fpu_used) dh_fpu_esc6();
				else {
					dh_fpu_startup();
					dh_fpu_esc6();
				}
			} else
#endif
			dyn_fpu_esc6();
			break;
		case 0xdf:
#ifdef X86_DYNFPU_DH_ENABLED
			if (dyn_dh_fpu.dh_fpu_enabled) {
				if (fpu_used) dh_fpu_esc7();
				else {
					dh_fpu_startup();
					dh_fpu_esc7();
				}
			} else
#endif
			dyn_fpu_esc7();
			break;
#endif
		//Loops 
		case 0xe2:dyn_loop(LOOP_NONE);goto finish_block;
		case 0xe3:dyn_loop(LOOP_JCXZ);goto finish_block;
		//IN AL/AX,imm
		case 0xe4: {
			Bitu port=decode_fetchb();
			dyn_add_iocheck_var(port,1);
			dyn_call_function_pagefault_check((void*)&IO_ReadB,"%Id%Rl",port,DREG(EAX));
			} break;
		case 0xe5: {
			Bitu port=decode_fetchb();
			dyn_add_iocheck_var(port,decode.big_op?4:2);
			if (decode.big_op) {
                dyn_call_function_pagefault_check((void*)&IO_ReadD,"%Id%Rd",port,DREG(EAX));
			} else {
				dyn_call_function_pagefault_check((void*)&IO_ReadW,"%Id%Rw",port,DREG(EAX));
			}
			} break;
		//OUT imm,AL
		case 0xe6: {
			Bitu port=decode_fetchb();
			dyn_add_iocheck_var(port,1);
			dyn_call_function_pagefault_check((void*)&IO_WriteB,"%Id%Dl",port,DREG(EAX));
			} break;
		case 0xe7: {
			Bitu port=decode_fetchb();
			dyn_add_iocheck_var(port,decode.big_op?4:2);
			if (decode.big_op) {
                dyn_call_function_pagefault_check((void*)&IO_WriteD,"%Id%Dd",port,DREG(EAX));
			} else {
				dyn_call_function_pagefault_check((void*)&IO_WriteW,"%Id%Dw",port,DREG(EAX));
			}
			} break;
		case 0xe8:		/* CALL Ivx */
			dyn_call_near_imm();
			goto finish_block;
		case 0xe9:		/* Jmp Ivx */
			dyn_exit_link(decode.big_op ? (int32_t)decode_fetchd() : (int16_t)decode_fetchw());
			goto finish_block;
		case 0xea:		/* JMP FAR Ip */
			dyn_jmp_far_imm();
			goto finish_block;
			/* Jmp Ibx */
		case 0xeb:dyn_exit_link((int8_t)decode_fetchb());goto finish_block;
		/* IN AL/AX,DX*/
		case 0xec:
			dyn_add_iocheck(1);
			dyn_call_function_pagefault_check((void*)&IO_ReadB,"%Dw%Rl",DREG(EDX),DREG(EAX));
			break;
		case 0xed:
			dyn_add_iocheck(decode.big_op?4:2);
			if (decode.big_op) {
                dyn_call_function_pagefault_check((void*)&IO_ReadD,"%Dw%Rd",DREG(EDX),DREG(EAX));
			} else {
				dyn_call_function_pagefault_check((void*)&IO_ReadW,"%Dw%Rw",DREG(EDX),DREG(EAX));
			}
			break;
		/* OUT DX,AL/AX */
		case 0xee:
			dyn_add_iocheck(1);
			dyn_call_function_pagefault_check((void*)&IO_WriteB,"%Dw%Dl",DREG(EDX),DREG(EAX));
			break;
		case 0xef:
			dyn_add_iocheck(decode.big_op?4:2);
			if (decode.big_op) {
                dyn_call_function_pagefault_check((void*)&IO_WriteD,"%Dw%Dd",DREG(EDX),DREG(EAX));
			} else {
				dyn_call_function_pagefault_check((void*)&IO_WriteW,"%Dw%Dw",DREG(EDX),DREG(EAX));
			}
			break;
		case 0xf0:		//LOCK
			goto restart_prefix;
		case 0xf2:		//REPNE/NZ
			decode.rep=REP_NZ;
			goto restart_prefix;
		case 0xf3:		//REPE/Z
			decode.rep=REP_Z;
			goto restart_prefix;
		/* Change carry flag */
		case 0xf5:		//CMC
		case 0xf8:		//CLC
		case 0xf9:		//STC
			gen_needflags();
			cache_addb(opcode);break;
		/* GRP 3 Eb/EV */
		case 0xf6:dyn_grp3_eb();break;
		case 0xf7:dyn_grp3_ev();break;
		/* Change interrupt flag */
		case 0xfa:		//CLI
			gen_releasereg(DREG(FLAGS));
			gen_call_function((void *)&CPU_CLI,"%Rd",DREG(TMPB));
			dyn_check_bool_exception(DREG(TMPB));
			gen_releasereg(DREG(TMPB));
			break;
		case 0xfb:		//STI
			gen_releasereg(DREG(FLAGS));
			gen_call_function((void *)&CPU_STI,"%Rd",DREG(TMPB));
			dyn_check_bool_exception(DREG(TMPB));
			gen_releasereg(DREG(TMPB));
			dyn_check_irqrequest();
			if (max_opcodes<=0) max_opcodes=1;		//Allow 1 extra opcode
			break;
		case 0xfc:		//CLD
			gen_protectflags();
			gen_dop_word_imm(DOP_AND,true,DREG(FLAGS),~FLAG_DF);
			gen_save_host_direct(&cpu.direction,1);
			break;
		case 0xfd:		//STD
			gen_protectflags();
			gen_dop_word_imm(DOP_OR,true,DREG(FLAGS),FLAG_DF);
			gen_save_host_direct(&cpu.direction,-1);
			break;
		/* GRP 4 Eb and callback's */
		case 0xfe:
            dyn_get_modrm();
			switch (decode.modrm.reg) {
			case 0x0://INC Eb
			case 0x1://DEC Eb
				if (decode.modrm.mod<3) {
					dyn_fill_ea();dyn_read_byte(DREG(EA),DREG(TMPB),false);
					gen_needcarry();
					gen_sop_byte(decode.modrm.reg==0 ? SOP_INC : SOP_DEC,DREG(TMPB),0);
					dyn_write_byte_release(DREG(EA),DREG(TMPB),false);
					gen_releasereg(DREG(TMPB));
				} else {
					gen_needcarry();
					gen_sop_byte(decode.modrm.reg==0 ? SOP_INC : SOP_DEC,
						&DynRegs[decode.modrm.rm&3],decode.modrm.rm&4);
				}
				break;
			case 0x7:		//CALBACK Iw
				gen_save_host_direct(&core_dyn.callback,decode_fetchw());
				dyn_set_eip_end();
				dyn_reduce_cycles();
				dyn_save_critical_regs();
				gen_return(BR_CallBack);
				dyn_closeblock();
				goto finish_block;
			}
			break;

		case 0xff: 
			{
            dyn_get_modrm();DynReg * src;
			if (decode.modrm.mod<3) {
				dyn_fill_ea();
				dyn_read_word(DREG(EA),DREG(TMPW),decode.big_op);
				src=DREG(TMPW);
			} else src=&DynRegs[decode.modrm.rm];
			switch (decode.modrm.reg) {
			case 0x0://INC Ev
			case 0x1://DEC Ev
				gen_needcarry();
				gen_sop_word(decode.modrm.reg==0 ? SOP_INC : SOP_DEC,decode.big_op,src);
				if (decode.modrm.mod<3){
					dyn_write_word_release(DREG(EA),DREG(TMPW),decode.big_op);
					gen_releasereg(DREG(TMPW));
				}
				break;
			case 0x2:	/* CALL Ev */
				gen_lea(DREG(TMPB),DREG(EIP),0,0,decode.code-decode.code_start);
				dyn_push(DREG(TMPB));
				gen_releasereg(DREG(TMPB));
				gen_dop_word(DOP_MOV,decode.big_op,DREG(EIP),src);
				goto core_close_block;
			case 0x4:	/* JMP Ev */
				gen_dop_word(DOP_MOV,decode.big_op,DREG(EIP),src);
				goto core_close_block;
			case 0x3:	/* CALL Ep */
			case 0x5:	/* JMP Ep */
				gen_protectflags();
				dyn_flags_gen_to_host();
				gen_lea(DREG(EA),DREG(EA),0,0,decode.big_op ? 4: 2);
				dyn_read_word(DREG(EA),DREG(EA),false);
				dyn_set_eip_last_end(DREG(TMPB));
				dyn_save_critical_regs();
				if (decode.big_op) {
					dyn_call_function_pagefault_check(
						decode.modrm.reg == 3 ? (void*)&CPU_CALL : (void*)&CPU_JMP,
						"%Id%Drw%Drd%Drd",
						decode.big_op,DREG(EA),DREG(TMPW),DREG(TMPB));
				} else {
					dyn_call_function_pagefault_check(
						decode.modrm.reg == 3 ? (void*)&CPU_CALL : (void*)&CPU_JMP,
						"%Id%Drw%Drw%Drd",
						decode.big_op,DREG(EA),DREG(TMPW),DREG(TMPB));
				}
				dyn_flags_host_to_gen();
				goto core_close_block;
			case 0x6:		/* PUSH Ev */
				gen_releasereg(DREG(EA));
				dyn_push(src);
				gen_releasereg(DREG(TMPW));
				break;
			default:
				LOG(LOG_CPU,LOG_ERROR)("CPU:GRP5:Illegal opcode 0xff");
				goto illegalopcode;
			}}
			break;
		default:
#if DYN_LOG
//			LOG_MSG("Dynamic unhandled opcode %X",opcode);
#endif
			goto illegalopcode;
		}
	}
	// link to next block because the maximum number of opcodes has been reached
	dyn_set_eip_end();
	dyn_reduce_cycles();
	dyn_save_critical_regs();
	gen_jmp_ptr(&decode.block->link[0].to,offsetof(CacheBlock,cache.xstart));
	dyn_closeblock();
	goto finish_block;
core_close_block:
	dyn_reduce_cycles();
	dyn_save_critical_regs();
	gen_return(BR_Normal);
	dyn_closeblock();
	goto finish_block;
illegalopcode:
	dyn_set_eip_last();
	dyn_reduce_cycles();
	dyn_save_critical_regs();
	gen_return(BR_Opcode);
	dyn_closeblock();
	goto finish_block;
#if (C_DEBUG)
	dyn_set_eip_last();
	dyn_reduce_cycles();
	dyn_save_critical_regs();
	gen_return(BR_OpcodeFull);
	dyn_closeblock();
	goto finish_block;
#endif
finish_block:
	/* Setup the correct end-address */
	decode.active_block->page.end=--decode.page.index;
//	LOG_MSG("Created block size %d start %d end %d",decode.block->cache.size,decode.block->page.start,decode.block->page.end);
	cache_remap_rx();
	return decode.block;
}
