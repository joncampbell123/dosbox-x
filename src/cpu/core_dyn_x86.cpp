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


#include "dosbox.h"

#if (C_DYNAMIC_X86)

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>

#if defined (WIN32)
#include <windows.h>
#include <winbase.h>
#endif

#if (C_HAVE_MPROTECT)
#include <sys/mman.h>

#include <limits.h>
#ifndef PAGESIZE
#define PAGESIZE 4096
#endif
#endif /* C_HAVE_MPROTECT */

#include "callback.h"
#include "regs.h"
#include "mem.h"
#include "cpu.h"
#include "debug.h"
#include "paging.h"
#include "inout.h"
#include "fpu.h"

#define CACHE_MAXSIZE	(4096*4)
#define CACHE_TOTAL		(1024*1024*8)
#define CACHE_PAGES		(512)
#define CACHE_BLOCKS	(64*1024)
#define CACHE_ALIGN		(16)
#define DYN_HASH_SHIFT	(4)
#define DYN_PAGE_HASH	(4096>>DYN_HASH_SHIFT)
#define DYN_LINKS		(16)

//#define DYN_LOG 1 //Turn logging on

#define DYN_NON_RECURSIVE_PAGEFAULT

#if C_FPU
#define CPU_FPU 1                                               //Enable FPU escape instructions
#define X86_DYNFPU_DH_ENABLED
#endif

enum {
	G_EAX,G_ECX,G_EDX,G_EBX,
	G_ESP,G_EBP,G_ESI,G_EDI,
	G_ES,G_CS,G_SS,G_DS,G_FS,G_GS,
	G_FLAGS,G_NEWESP,G_EIP,
	G_EA,G_STACK,G_CYCLES,
	G_TMPB,G_TMPW,G_TMPD,G_SHIFT,
	G_EXIT,
	G_MAX,
};

enum SingleOps {
	SOP_INC,SOP_DEC,
	SOP_NOT,SOP_NEG,
};

enum DualOps {
	DOP_ADD,DOP_ADC,
	DOP_SUB,DOP_SBB,
	DOP_CMP,DOP_XOR,
	DOP_AND,DOP_OR,
	DOP_TEST,
	DOP_MOV,
	DOP_XCHG,
};

enum ShiftOps {
	SHIFT_ROL,SHIFT_ROR,
	SHIFT_RCL,SHIFT_RCR,
	SHIFT_SHL,SHIFT_SHR,
	SHIFT_SAL,SHIFT_SAR,
};

enum BranchTypes {
	BR_O,BR_NO,BR_B,BR_NB,
	BR_Z,BR_NZ,BR_BE,BR_NBE,
	BR_S,BR_NS,BR_P,BR_NP,
	BR_L,BR_NL,BR_LE,BR_NLE
};


enum BlockReturn {
	BR_Normal=0,
	BR_Cycles,
	BR_Link1,BR_Link2,
	BR_Opcode,
#if (C_DEBUG)
	BR_OpcodeFull,
#endif
	BR_Iret,
	BR_CallBack,
	BR_SMCBlock,
	BR_Trap
};

#define SMC_CURRENT_BLOCK	0xffff


#define DYNFLG_HAS16		0x1		//Would like 8-bit host reg support
#define DYNFLG_HAS8			0x2		//Would like 16-bit host reg support
#define DYNFLG_LOAD			0x4		//Load value when accessed
#define DYNFLG_SAVE			0x8		//Needs to be saved back at the end of block
#define DYNFLG_CHANGED		0x10	//Value is in a register and changed from load
#define DYNFLG_ACTIVE		0x20	//Register has an active value

class GenReg;
class CodePageHandler;

struct DynReg {
	Bitu flags;
	GenReg * genreg;
	void * data;
}; 

enum DynAccess {
	DA_d,DA_w,
	DA_bh,DA_bl
};

enum ByteCombo {
	BC_ll,BC_lh,
	BC_hl,BC_hh,
};

static DynReg DynRegs[G_MAX];
#define DREG(_WHICH_) &DynRegs[G_ ## _WHICH_ ]

static struct {
	uint32_t ea,tmpb,tmpw,tmpd,stack,shift,newesp;
} extra_regs;

#define IllegalOption(msg) E_Exit("DYNX86: illegal option in " msg)

#include "core_dyn_x86/cache.h" 

static struct {
	Bitu callback;
	Bitu readdata;
#ifdef DYN_NON_RECURSIVE_PAGEFAULT
	void *call_func;
	Bitu pagefault_old_stack;
#ifdef CPU_FPU
	Bitu pagefault_old_fpu_top;
#endif
	Bitu pagefault_faultcode;
	bool pagefault;
#endif
} core_dyn;

#ifdef DYN_NON_RECURSIVE_PAGEFAULT
static struct {
	bool had_pagefault;
	PhysPt lin_addr;
	Bitu page_addr;
	Bitu faultcode;
} decoder_pagefault;
#endif

#if defined(X86_DYNFPU_DH_ENABLED)
static struct dyn_dh_fpu {
	uint16_t	cw,host_cw;
	bool		state_used;
	// some fields expanded here for alignment purposes
	struct {
		uint32_t cw;
		uint32_t sw;
		uint32_t tag;
		uint32_t ip;
		uint32_t cs;
		uint32_t ea;
		uint32_t ds;
		uint8_t st_reg[8][10];
	} state;
	FPU_P_Reg	temp,temp2;
	uint32_t	dh_fpu_enabled;
	uint8_t		temp_state[128];
} dyn_dh_fpu;
#endif

#ifdef DYN_NON_RECURSIVE_PAGEFAULT
#define DYN_DEBUG_PAGEFAULT

#ifdef DYN_DEBUG_PAGEFAULT
#define DYN_PF_LOG_MSG LOG_MSG
#else
#define DYN_PF_LOG_MSG(...)
#endif

#define DYN_PAGEFAULT_CHECK(x) { \
	try { \
		x; \
	} catch (const GuestPageFaultException &pf) { \
		core_dyn.pagefault_faultcode = pf.faultcode; \
		core_dyn.pagefault = true; \
		DYN_PF_LOG_MSG("Caught pagefault at %s:%d (%s)", __FILE__, __LINE__, __FUNCTION__); \
	} \
	return 0; \
}

static INLINE bool mem_writeb_checked_pagefault(const PhysPt address,const uint8_t val) {
	DYN_PAGEFAULT_CHECK({
		return mem_writeb_checked(address, val);
	});
}

static INLINE bool mem_writew_checked_pagefault(const PhysPt address,const uint16_t val) {
	DYN_PAGEFAULT_CHECK({
		return mem_writew_checked(address, val);
	});
}

static INLINE bool mem_writed_checked_pagefault(const PhysPt address,const uint32_t val) {
	DYN_PAGEFAULT_CHECK({
		return mem_writed_checked(address, val);
	});
}

#else

#define DYN_PAGEFAULT_CHECK(x) x
#define mem_writeb_checked_pagefault mem_writeb_checked
#define mem_writew_checked_pagefault mem_writew_checked
#define mem_writed_checked_pagefault mem_writed_checked

#endif

#define X86         0x01
#define X86_64      0x02

#if C_TARGETCPU == X86_64
#include "core_dyn_x86/risc_x64.h"
#elif C_TARGETCPU == X86
#include "core_dyn_x86/risc_x86.h"
#else
#error DYN_X86 core not supported for this CPU target.
#endif

struct DynState {
	DynReg regs[G_MAX];
};

static void dyn_flags_host_to_gen(void) {
	gen_dop_word(DOP_MOV,true,DREG(EXIT),DREG(FLAGS));
	gen_dop_word_imm(DOP_AND,true,DREG(EXIT),FMASK_TEST);
	gen_load_flags(DREG(EXIT));
	gen_releasereg(DREG(EXIT));
	gen_releasereg(DREG(FLAGS));
}

static void dyn_flags_gen_to_host(void) {
	gen_save_flags(DREG(EXIT));
	gen_dop_word_imm(DOP_AND,true,DREG(EXIT),FMASK_TEST);
	gen_dop_word_imm(DOP_AND,true,DREG(FLAGS),~FMASK_TEST);
	gen_dop_word(DOP_OR,true,DREG(FLAGS),DREG(EXIT)); //flags are marked for save
	gen_releasereg(DREG(EXIT));
	gen_releasereg(DREG(FLAGS));
}

static void dyn_savestate(DynState * state) {
	for (Bitu i=0;i<G_MAX;i++) {
		state->regs[i].flags=DynRegs[i].flags;
		state->regs[i].genreg=DynRegs[i].genreg;
	}
}

static void dyn_loadstate(DynState * state) {
	for (Bitu i=0;i<G_MAX;i++) {
		gen_setupreg(&DynRegs[i],&state->regs[i]);
	}
}

static void dyn_synchstate(DynState * state) {
	for (Bitu i=0;i<G_MAX;i++) {
		gen_synchreg(&DynRegs[i],&state->regs[i]);
	}
}

static void dyn_saveregister(DynReg * src_reg, DynReg * dst_reg) {
	dst_reg->flags=src_reg->flags;
	dst_reg->genreg=src_reg->genreg;
}

static void dyn_restoreregister(DynReg * src_reg, DynReg * dst_reg) {
	dst_reg->flags=src_reg->flags;
	dst_reg->genreg=src_reg->genreg;
	dst_reg->genreg->dynreg=dst_reg;	// necessary when register has been released
}

#include "core_dyn_x86/decoder.h"

extern int dynamic_core_cache_block_size;

Bits CPU_Core_Dyn_X86_Run(void) {
	// helper class to auto-save DH_FPU state on function exit
	class auto_dh_fpu {
	public:
		~auto_dh_fpu(void) {
#if defined(X86_DYNFPU_DH_ENABLED)
			if (dyn_dh_fpu.state_used)
				gen_dh_fpu_save();
#endif
		};
	};
	auto_dh_fpu fpu_saver;

	/* Determine the linear address of CS:EIP */
restart_core:
#ifndef DYN_NON_RECURSIVE_PAGEFAULT
	dosbox_allow_nonrecursive_page_fault = false;
#endif
	PhysPt ip_point=SegPhys(cs)+reg_eip;
#if C_DEBUG
#if C_HEAVY_DEBUG
		if (DEBUG_HeavyIsBreakpoint()) return debugCallback;
#endif
#endif
	CodePageHandler * chandler=0;
	if (GCC_UNLIKELY(MakeCodePage(ip_point,chandler))) {
		CPU_Exception(cpu.exception.which,cpu.exception.error);
		goto restart_core;
	}
	if (!chandler) {
#ifndef DYN_NON_RECURSIVE_PAGEFAULT
		dosbox_allow_nonrecursive_page_fault = true;
#endif
		return CPU_Core_Normal_Run();
	}
	/* Find correct Dynamic Block to run */
	CacheBlock * block=chandler->FindCacheBlock(ip_point&4095);
	if (!block) {
		if (!chandler->invalidation_map || (chandler->invalidation_map[ip_point&4095]<4)) {
#ifdef DYN_NON_RECURSIVE_PAGEFAULT
			decoder_pagefault.had_pagefault = false;
			// We can't throw exception during the creation of the block, as it will corrupt things
			// If a page fault occoured, invalidated the block and throw exception from here
#endif
			int cache_size = dynamic_core_cache_block_size;
			block = CreateCacheBlock(chandler,ip_point,cache_size);
#ifdef DYN_NON_RECURSIVE_PAGEFAULT
			while (decoder_pagefault.had_pagefault) {
				block->Clear();
				if (cache_size == 1)
					throw GuestPageFaultException(decoder_pagefault.lin_addr, decoder_pagefault.page_addr, decoder_pagefault.faultcode);
				cache_size /= 2;
				decoder_pagefault.had_pagefault = false;
				block = CreateCacheBlock(chandler,ip_point,cache_size);
			}
#endif
		} else {
			int32_t old_cycles=CPU_Cycles;
			CPU_Cycles=1;
			CPU_CycleLeft+=old_cycles;
			// manually save
			fpu_saver = auto_dh_fpu();
#ifndef DYN_NON_RECURSIVE_PAGEFAULT
			dosbox_allow_nonrecursive_page_fault = true;
#endif
			Bits nc_retcode=CPU_Core_Normal_Run();
			if (!nc_retcode) {
				CPU_Cycles=old_cycles-1;
				CPU_CycleLeft-=old_cycles;
				goto restart_core;
			}
			return nc_retcode; 
		}
	}
run_block:
	cache.block.running=0;
#ifdef DYN_NON_RECURSIVE_PAGEFAULT
	core_dyn.pagefault = false;
#endif
	BlockReturn ret=gen_runcode((uint8_t*)cache_rwtox(block->cache.start));

	if (sizeof(CPU_Cycles) > 4) {
		// HACK: All dynrec cores for each processor assume CPU_Cycles is 32-bit wide.
		//       The purpose of this hack is to sign-extend the lower 32 bits so that
		//       when CPU_Cycles goes negative it doesn't suddenly appear as a very
		//       large integer value.
		//
		//       This hack is needed for dynrec to work on x86_64 targets.
		CPU_Cycles = (Bits)((int32_t)CPU_Cycles);
	}

#if C_DEBUG
	cycle_count += 32;
#endif
	switch (ret) {
	case BR_Iret:
#if C_DEBUG
#if C_HEAVY_DEBUG
		if (DEBUG_HeavyIsBreakpoint()) {
			return debugCallback;
		}
#endif
#endif
		if (!GETFLAG(TF)) {
			if (GETFLAG(IF) && PIC_IRQCheck) {
				return CBRET_NONE;
			}
			goto restart_core;
		}
		cpudecoder=CPU_Core_Dyn_X86_Trap_Run;
		return CBRET_NONE;
	case BR_Normal:
		/* Maybe check if we staying in the same page? */
#if C_DEBUG
#if C_HEAVY_DEBUG
		if (DEBUG_HeavyIsBreakpoint()) return debugCallback;
#endif
#endif
		goto restart_core;
	case BR_Cycles:
#if C_DEBUG
#if C_HEAVY_DEBUG			
		if (DEBUG_HeavyIsBreakpoint()) return debugCallback;
#endif
#endif
		return CBRET_NONE;
	case BR_CallBack:
		return core_dyn.callback;
	case BR_SMCBlock:
//		LOG_MSG("selfmodification of running block at %x:%x",SegValue(cs),reg_eip);
		cpu.exception.which=0;
		// fallthrough, let the normal core handle the block-modifying instruction
	case BR_Opcode:
		CPU_CycleLeft+=CPU_Cycles;
		CPU_Cycles=1;
#ifndef DYN_NON_RECURSIVE_PAGEFAULT
		dosbox_allow_nonrecursive_page_fault = true;
#endif
		return CPU_Core_Normal_Run();
#if (C_DEBUG)
	case BR_OpcodeFull:
		CPU_CycleLeft+=CPU_Cycles;
		CPU_Cycles=1;
#ifndef DYN_NON_RECURSIVE_PAGEFAULT
		dosbox_allow_nonrecursive_page_fault = true;
#endif
		return CPU_Core_Full_Run();
#endif
	case BR_Link1:
	case BR_Link2:
		{
			uint32_t temp_ip=SegPhys(cs)+reg_eip;
			CodePageHandler * temp_handler=(CodePageHandler *)get_tlb_readhandler(temp_ip);
			if (temp_handler->flags & (cpu.code.big ? PFLAG_HASCODE32:PFLAG_HASCODE16)) {
				block=temp_handler->FindCacheBlock(temp_ip & 4095);
				if (!block) goto restart_core;
				cache.block.running->LinkTo(ret==BR_Link2,block);
				goto run_block;
			}
		}
		goto restart_core;
	case BR_Trap:
			// trapflag is set, switch to the trap-aware decoder
	#if C_DEBUG
	#if C_HEAVY_DEBUG
			if (DEBUG_HeavyIsBreakpoint()) {
				return debugCallback;
			}
	#endif
	#endif
			cpudecoder=CPU_Core_Dyn_X86_Trap_Run;
			return CBRET_NONE;
	}
	return CBRET_NONE;
}

Bits CPU_Core_Dyn_X86_Trap_Run(void) {
	int32_t oldCycles = CPU_Cycles;
	CPU_Cycles = 1;
	cpu.trap_skip = false;

	Bits ret=CPU_Core_Normal_Run();
	if (!cpu.trap_skip) CPU_HW_Interrupt(1);
	CPU_Cycles = oldCycles-1;
	cpudecoder = &CPU_Core_Dyn_X86_Run;

	return ret;
}

void CPU_Core_Dyn_X86_Shutdown(void) {
	gen_free();
}

void CPU_Core_Dyn_X86_Init(void) {
	Bits i;
	/* Setup the global registers and their flags */
	for (i=0;i<G_MAX;i++) DynRegs[i].genreg=0;
	DynRegs[G_EAX].data=&reg_eax;
	DynRegs[G_EAX].flags=DYNFLG_HAS8|DYNFLG_HAS16|DYNFLG_LOAD|DYNFLG_SAVE;
	DynRegs[G_ECX].data=&reg_ecx;
	DynRegs[G_ECX].flags=DYNFLG_HAS8|DYNFLG_HAS16|DYNFLG_LOAD|DYNFLG_SAVE;
	DynRegs[G_EDX].data=&reg_edx;
	DynRegs[G_EDX].flags=DYNFLG_HAS8|DYNFLG_HAS16|DYNFLG_LOAD|DYNFLG_SAVE;
	DynRegs[G_EBX].data=&reg_ebx;
	DynRegs[G_EBX].flags=DYNFLG_HAS8|DYNFLG_HAS16|DYNFLG_LOAD|DYNFLG_SAVE;

	DynRegs[G_EBP].data=&reg_ebp;
	DynRegs[G_EBP].flags=DYNFLG_HAS16|DYNFLG_LOAD|DYNFLG_SAVE;
	DynRegs[G_ESP].data=&reg_esp;
	DynRegs[G_ESP].flags=DYNFLG_HAS16|DYNFLG_LOAD|DYNFLG_SAVE;
	DynRegs[G_EDI].data=&reg_edi;
	DynRegs[G_EDI].flags=DYNFLG_HAS16|DYNFLG_LOAD|DYNFLG_SAVE;
	DynRegs[G_ESI].data=&reg_esi;
	DynRegs[G_ESI].flags=DYNFLG_HAS16|DYNFLG_LOAD|DYNFLG_SAVE;

	DynRegs[G_ES].data=&Segs.phys[es];
	DynRegs[G_ES].flags=DYNFLG_LOAD|DYNFLG_SAVE;
	DynRegs[G_CS].data=&Segs.phys[cs];
	DynRegs[G_CS].flags=DYNFLG_LOAD|DYNFLG_SAVE;
	DynRegs[G_SS].data=&Segs.phys[ss];
	DynRegs[G_SS].flags=DYNFLG_LOAD|DYNFLG_SAVE;
	DynRegs[G_DS].data=&Segs.phys[ds];
	DynRegs[G_DS].flags=DYNFLG_LOAD|DYNFLG_SAVE;
	DynRegs[G_FS].data=&Segs.phys[fs];
	DynRegs[G_FS].flags=DYNFLG_LOAD|DYNFLG_SAVE;
	DynRegs[G_GS].data=&Segs.phys[gs];
	DynRegs[G_GS].flags=DYNFLG_LOAD|DYNFLG_SAVE;

	DynRegs[G_FLAGS].data=&reg_flags;
	DynRegs[G_FLAGS].flags=DYNFLG_LOAD|DYNFLG_SAVE;

	DynRegs[G_NEWESP].data=&extra_regs.newesp;
	DynRegs[G_NEWESP].flags=0;

	DynRegs[G_EIP].data=&reg_eip;
	DynRegs[G_EIP].flags=DYNFLG_LOAD|DYNFLG_SAVE;

	DynRegs[G_EA].data=&extra_regs.ea;
	DynRegs[G_EA].flags=0;
	DynRegs[G_STACK].data=&extra_regs.stack;
	DynRegs[G_STACK].flags=0;
	DynRegs[G_CYCLES].data=&CPU_Cycles;
	DynRegs[G_CYCLES].flags=DYNFLG_LOAD|DYNFLG_SAVE;
	DynRegs[G_TMPB].data=&extra_regs.tmpb;
	DynRegs[G_TMPB].flags=DYNFLG_HAS8|DYNFLG_HAS16;
	DynRegs[G_TMPW].data=&extra_regs.tmpw;
	DynRegs[G_TMPW].flags=DYNFLG_HAS16;
	DynRegs[G_TMPD].data=&extra_regs.tmpd;
	DynRegs[G_TMPD].flags=DYNFLG_HAS16;
	DynRegs[G_SHIFT].data=&extra_regs.shift;
	DynRegs[G_SHIFT].flags=DYNFLG_HAS8|DYNFLG_HAS16;
	DynRegs[G_EXIT].data=0;
	DynRegs[G_EXIT].flags=DYNFLG_HAS16;
	/* Init the generator */
	gen_init();

#if defined(X86_DYNFPU_DH_ENABLED)
	/* Init the fpu state */
	dyn_dh_fpu.dh_fpu_enabled=true;
	dyn_dh_fpu.state_used=false;
	dyn_dh_fpu.cw=0x37f;
	// FINIT
	memset(&dyn_dh_fpu.state, 0, sizeof(dyn_dh_fpu.state));
	dyn_dh_fpu.state.cw = 0x37F;
	dyn_dh_fpu.state.tag = 0xFFFF;
#endif

	return;
}

void CPU_Core_Dyn_X86_Cache_Init(bool enable_cache) {
	/* Initialize code cache and dynamic blocks */
	cache_init(enable_cache);
}

void CPU_Core_Dyn_X86_Cache_Close(void) {
	cache_close();
}

void CPU_Core_Dyn_X86_Cache_Reset(void) {
	cache_reset();
}

void CPU_Core_Dyn_X86_SetFPUMode(bool dh_fpu) {
#if defined(X86_DYNFPU_DH_ENABLED)
	dyn_dh_fpu.dh_fpu_enabled=dh_fpu;
#endif
}

uint32_t fpu_state[32];

void CPU_Core_Dyn_X86_SaveDHFPUState(void) {
#if C_TARGETCPU != X86_64
	if (dyn_dh_fpu.dh_fpu_enabled) {
		if (dyn_dh_fpu.state_used!=0) {
#if defined (_MSC_VER)
			__asm {
			__asm	fsave	fpu_state[0]
			__asm	finit
			}
#else
			__asm__ volatile (
				"fsave		%0			\n"
				"finit					\n"
				:	"=m" (fpu_state[0])
				:
				:	"memory"
			);
#endif
		}
	}
#endif
}

void CPU_Core_Dyn_X86_RestoreDHFPUState(void) {
#if C_TARGETCPU != X86_64
	if (dyn_dh_fpu.dh_fpu_enabled) {
		if (dyn_dh_fpu.state_used!=0) {
#if defined (_MSC_VER)
			__asm {
			__asm	frstor	fpu_state[0]
			}
#else
			__asm__ volatile (
				"frstor		%0			\n"
				:
				:	"m" (fpu_state[0])
				:
			);
#endif
		}
	}
#endif
}

#else

void CPU_Core_Dyn_X86_SaveDHFPUState(void) {
}

void CPU_Core_Dyn_X86_RestoreDHFPUState(void) {
}

#endif
