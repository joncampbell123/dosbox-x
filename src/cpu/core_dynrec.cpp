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

/* $Id: core_dynrec.cpp,v 1.15 2009-08-02 16:52:33 c2woody Exp $ */

#include "dosbox.h"

#if (C_DYNREC)

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
#include "lazyflags.h"
#include "pic.h"

#define CACHE_MAXSIZE	(4096*2)
#define CACHE_TOTAL		(1024*1024*8)
#define CACHE_PAGES		(512)
#define CACHE_BLOCKS	(128*1024)
#define CACHE_ALIGN		(16)
#define DYN_HASH_SHIFT	(4)
#define DYN_PAGE_HASH	(4096>>DYN_HASH_SHIFT)
#define DYN_LINKS		(16)

#if 0
#define DYN_LOG	LOG_MSG
#else 
#define DYN_LOG
#endif

#if C_FPU
#define CPU_FPU 1                                               //Enable FPU escape instructions
#endif


// the emulated x86 registers
#define DRC_REG_EAX 0
#define DRC_REG_ECX 1
#define DRC_REG_EDX 2
#define DRC_REG_EBX 3
#define DRC_REG_ESP 4
#define DRC_REG_EBP 5
#define DRC_REG_ESI 6
#define DRC_REG_EDI 7

// the emulated x86 segment registers
#define DRC_SEG_ES 0
#define DRC_SEG_CS 1
#define DRC_SEG_SS 2
#define DRC_SEG_DS 3
#define DRC_SEG_FS 4
#define DRC_SEG_GS 5


// access to a general register
#define DRCD_REG_VAL(reg) (&cpu_regs.regs[reg].dword)
// access to a segment register
#define DRCD_SEG_VAL(seg) (&Segs.val[seg])
// access to the physical value of a segment register/selector
#define DRCD_SEG_PHYS(seg) (&Segs.phys[seg])

// access to an 8bit general register
#define DRCD_REG_BYTE(reg,idx) (&cpu_regs.regs[reg].byte[idx?BH_INDEX:BL_INDEX])
// access to  16/32bit general registers
#define DRCD_REG_WORD(reg,dwrd) ((dwrd)?((void*)(&cpu_regs.regs[reg].dword[DW_INDEX])):((void*)(&cpu_regs.regs[reg].word[W_INDEX])))


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
	BR_SMCBlock
};

// identificator to signal self-modification of the currently executed block
#define SMC_CURRENT_BLOCK	0xffff


static void IllegalOptionDynrec(const char* msg) {
	E_Exit("DynrecCore: illegal option in %s",msg);
}

static struct {
	BlockReturn (*runcode)(Bit8u*);		// points to code that can start a block
	Bitu callback;				// the occurred callback
	Bitu readdata;				// spare space used when reading from memory
	Bit32u protected_regs[8];	// space to save/restore register values
} core_dynrec;


#include "core_dynrec/cache.h"

#define X86			0x01
#define X86_64		0x02
#define MIPSEL		0x03
#define ARMV4LE		0x04
#define POWERPC		0x04

#if C_TARGETCPU == X86_64
#include "core_dynrec/risc_x64.h"
#elif C_TARGETCPU == X86
#include "core_dynrec/risc_x86.h"
#elif C_TARGETCPU == MIPSEL
#include "core_dynrec/risc_mipsel32.h"
#elif C_TARGETCPU == ARMV4LE
#include "core_dynrec/risc_armv4le.h"
#elif C_TARGETCPU == POWERPC
#include "core_dynrec/risc_ppc.h"
#endif

#include "core_dynrec/decoder.h"

CacheBlockDynRec * LinkBlocks(BlockReturn ret) {
	CacheBlockDynRec * block=NULL;
	// the last instruction was a control flow modifying instruction
	Bitu temp_ip=SegPhys(cs)+reg_eip;
	CodePageHandlerDynRec * temp_handler=(CodePageHandlerDynRec *)get_tlb_readhandler(temp_ip);
	if (temp_handler->flags & PFLAG_HASCODE) {
		// see if the target is an already translated block
		block=temp_handler->FindCacheBlock(temp_ip & 4095);
		if (!block) return NULL;

		// found it, link the current block to 
		cache.block.running->LinkTo(ret==BR_Link2,block);
		return block;
	}
	return NULL;
}

/*
	The core tries to find the block that should be executed next.
	If such a block is found, it is run, otherwise the instruction
	stream starting at ip_point is translated (see decoder.h) and
	makes up a new code block that will be run.
	When control is returned to CPU_Core_Dynrec_Run (which might
	be right after the block is run, or somewhen long after that
	due to the direct cacheblock linking) the returncode decides
	the next action. This might be continuing the translation and
	execution process, or returning from the core etc.
*/

Bits CPU_Core_Dynrec_Run(void) {
	for (;;) {
		// Determine the linear address of CS:EIP
		PhysPt ip_point=SegPhys(cs)+reg_eip;
		#if C_HEAVY_DEBUG
			if (DEBUG_HeavyIsBreakpoint()) return debugCallback;
		#endif

		CodePageHandlerDynRec * chandler=0;
		// see if the current page is present and contains code
		if (GCC_UNLIKELY(MakeCodePage(ip_point,chandler))) {
			// page not present, throw the exception
			CPU_Exception(cpu.exception.which,cpu.exception.error);
			continue;
		}

		// page doesn't contain code or is special
		if (GCC_UNLIKELY(!chandler)) return CPU_Core_Normal_Run();

		// find correct Dynamic Block to run
		CacheBlockDynRec * block=chandler->FindCacheBlock(ip_point&4095);
		if (!block) {
			// no block found, thus translate the instruction stream
			// unless the instruction is known to be modified
			if (!chandler->invalidation_map || (chandler->invalidation_map[ip_point&4095]<4)) {
				// translate up to 32 instructions
				block=CreateCacheBlock(chandler,ip_point,32);
			} else {
				// let the normal core handle this instruction to avoid zero-sized blocks
				Bitu old_cycles=CPU_Cycles;
				CPU_Cycles=1;
				Bits nc_retcode=CPU_Core_Normal_Run();
				if (!nc_retcode) {
					CPU_Cycles=old_cycles-1;
					continue;
				}
				CPU_CycleLeft+=old_cycles;
				return nc_retcode; 
			}
		}

run_block:
		cache.block.running=0;
		// now we're ready to run the dynamic code block
//		BlockReturn ret=((BlockReturn (*)(void))(block->cache.start))();
		BlockReturn ret=core_dynrec.runcode(block->cache.start);

		switch (ret) {
		case BR_Iret:
#if C_HEAVY_DEBUG
			if (DEBUG_HeavyIsBreakpoint()) return debugCallback;
#endif
			if (!GETFLAG(TF)) {
				if (GETFLAG(IF) && PIC_IRQCheck) return CBRET_NONE;
				break;
			}
			// trapflag is set, switch to the trap-aware decoder
			cpudecoder=CPU_Core_Dynrec_Trap_Run;
			return CBRET_NONE;

		case BR_Normal:
			// the block was exited due to a non-predictable control flow
			// modifying instruction (like ret) or some nontrivial cpu state
			// changing instruction (for example switch to/from pmode),
			// or the maximal number of instructions to translate was reached
#if C_HEAVY_DEBUG
			if (DEBUG_HeavyIsBreakpoint()) return debugCallback;
#endif
			break;

		case BR_Cycles:
			// cycles went negative, return from the core to handle
			// external events, schedule the pic...
#if C_HEAVY_DEBUG			
			if (DEBUG_HeavyIsBreakpoint()) return debugCallback;
#endif
			return CBRET_NONE;

		case BR_CallBack:
			// the callback code is executed in dosbox.conf, return the callback number
			FillFlags();
			return core_dynrec.callback;

		case BR_SMCBlock:
//			LOG_MSG("selfmodification of running block at %x:%x",SegValue(cs),reg_eip);
			cpu.exception.which=0;
			// fallthrough, let the normal core handle the block-modifying instruction
		case BR_Opcode:
			// some instruction has been encountered that could not be translated
			// (thus it is not part of the code block), the normal core will
			// handle this instruction
			CPU_CycleLeft+=CPU_Cycles;
			CPU_Cycles=1;
			return CPU_Core_Normal_Run();

#if (C_DEBUG)
		case BR_OpcodeFull:
			CPU_CycleLeft+=CPU_Cycles;
			CPU_Cycles=1;
			return CPU_Core_Full_Run();
#endif

		case BR_Link1:
		case BR_Link2:
			block=LinkBlocks(ret);
			if (block) goto run_block;
			break;

		default:
			E_Exit("Invalid return code %d", ret);
		}
	}
	return CBRET_NONE;
}

Bits CPU_Core_Dynrec_Trap_Run(void) {
	Bits oldCycles = CPU_Cycles;
	CPU_Cycles = 1;
	cpu.trap_skip = false;

	// let the normal core execute the next (only one!) instruction
	Bits ret=CPU_Core_Normal_Run();

	// trap to int1 unless the last instruction deferred this
	// (allows hardware interrupts to be served without interaction)
	if (!cpu.trap_skip) CPU_HW_Interrupt(1);

	CPU_Cycles = oldCycles-1;
	// continue (either the trapflag was clear anyways, or the int1 cleared it)
	cpudecoder = &CPU_Core_Dynrec_Run;

	return ret;
}

void CPU_Core_Dynrec_Init(void) {
}

void CPU_Core_Dynrec_Cache_Init(bool enable_cache) {
	// Initialize code cache and dynamic blocks
	cache_init(enable_cache);
}

void CPU_Core_Dynrec_Cache_Close(void) {
	cache_close();
}

#endif
