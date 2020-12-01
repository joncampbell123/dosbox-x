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


//#define DYN_LOG 1 //Turn Logging on.


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
	BlockReturn (*runcode)(uint8_t*);		// points to code that can start a block
	Bitu callback;				// the occurred callback
	Bitu readdata;				// spare space used when reading from memory
	uint32_t protected_regs[8];	// space to save/restore register values
} core_dynrec;


#include "core_dynrec/cache.h"

#define X86			0x01
#define X86_64		0x02
#define MIPSEL		0x03
#define ARMV4LE		0x04
#define ARMV7LE		0x05
#define ARMV8LE		0x07

#if !defined(C_TARGETCPU)
# if defined(_MSC_VER) && defined(_M_AMD64)
#  define C_TARGETCPU X86_64
# elif defined(_MSC_VER) && defined(_M_ARM)
#  define C_TARGETCPU ARMV7LE
# elif defined(_MSC_VER) && defined(_M_ARM64)
#  define C_TARGETCPU ARMV8LE
# endif
#endif

#if C_TARGETCPU == X86_64
#include "core_dynrec/risc_x64.h"
#elif C_TARGETCPU == X86
#include "core_dynrec/risc_x86.h"
#elif C_TARGETCPU == MIPSEL
#include "core_dynrec/risc_mipsel32.h"
#elif (C_TARGETCPU == ARMV4LE) || (C_TARGETCPU == ARMV7LE)
#ifdef _MSC_VER
#pragma message("warning: Using ARMV4 or ARMV7")
#else
#warning Using ARMV4 or ARMV7
#endif
#include "core_dynrec/risc_armv4le.h"
#elif C_TARGETCPU == ARMV8LE
#include "core_dynrec/risc_armv8le.h"
#endif

#include "core_dynrec/decoder.h"

/* Dynarec on Windows ARM32 works, but only on Windows 10.
 * Windows RT 8.x only runs ARMv7 Thumb-2 code, which we don't have a backend for
 * Attempting to use dynarec on RT will result in an instant crash. 
 * So we need to detect the Windows version before enabling dynarec. */
#if defined(WIN32) && defined(_M_ARM)
bool IsWin10OrGreater() {
    HMODULE handle = GetModuleHandleW(L"ntdll.dll");
    if (handle) {
        typedef LONG(WINAPI* RtlGetVersionPtr)(RTL_OSVERSIONINFOW*);
        RtlGetVersionPtr getVersionPtr = (RtlGetVersionPtr)GetProcAddress(handle, "RtlGetVersion");
        if (getVersionPtr != NULL) {
            RTL_OSVERSIONINFOW info = { 0 };
            info.dwOSVersionInfoSize = sizeof(info);
            if (getVersionPtr(&info) == 0) { /* STATUS_SUCCESS == 0 */
                if (info.dwMajorVersion >= 10)
                    return true;
            }
        }
    }
    return false;
}

static bool is_win10 = false;
static bool winrt_warning = true;
#endif


CacheBlockDynRec * LinkBlocks(BlockReturn ret) {
	CacheBlockDynRec * block=NULL;
	// the last instruction was a control flow modifying instruction
	uint32_t temp_ip=SegPhys(cs)+reg_eip;
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

extern bool use_dynamic_core_with_paging;
extern int dynamic_core_cache_block_size;

static bool paging_warning = true;

Bits CPU_Core_Dynrec_Run(void) {
    if (CPU_Cycles <= 0)
	    return CBRET_NONE;

#if defined(WIN32) && defined(_M_ARM)
    if (!is_win10) {
        if (winrt_warning) {
            LOG_MSG("Dynamic core warning: Windows RT 8.x requires ARMv7 Thumb-2 dynarec core, which is not supported yet.");
            winrt_warning = false;
        }
        return CPU_Core_Normal_Run();
    }
#endif

    /* Dynamic core is NOT compatible with the way page faults
     * in the guest are handled in this emulator. Do not use
     * dynamic core if paging is enabled. Do not comment this
     * out, even if it happens to work for a minute, a half
     * hour, a day, because it will turn around and cause
     * Windows 95 to crash when you've become most comfortable
     * with the idea that it works. This code cannot handle
     * the sudden context switch of a page fault and it never
     * will. Don't do it. You have been warned. */
    if (paging.enabled && !use_dynamic_core_with_paging) {
        if (paging_warning) {
            LOG_MSG("Dynamic core warning: The guest OS/Application has just switched on 80386 paging, which is not supported by the dynamic core. The normal core will be used until paging is switched off again.");
            paging_warning = false;
        }

        return CPU_Core_Normal_Run();
    }

	for (;;) {
		// Determine the linear address of CS:EIP
		PhysPt ip_point=SegPhys(cs)+reg_eip;
		#if C_HEAVY_DEBUG
			if (DEBUG_HeavyIsBreakpoint()) return (Bits)debugCallback;
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
				cpu_cycles_count_t old_cycles=CPU_Cycles;
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

        if (sizeof(CPU_Cycles) > 4) {
            // HACK: All dynrec cores for each processor assume CPU_Cycles is 32-bit wide.
            //       The purpose of this hack is to sign-extend the lower 32 bits so that
            //       when CPU_Cycles goes negative it doesn't suddenly appear as a very
            //       large integer value.
            //
            //       This hack is needed for dynrec to work on x86_64 targets.
            CPU_Cycles = (Bits)((int32_t)CPU_Cycles);
        }

		switch (ret) {
		case BR_Iret:
#if C_DEBUG
#if C_HEAVY_DEBUG
			if (DEBUG_HeavyIsBreakpoint()) return (Bits)debugCallback;
#endif
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
			// or the maximum number of instructions to translate was reached
#if C_DEBUG
#if C_HEAVY_DEBUG
			if (DEBUG_HeavyIsBreakpoint()) return (Bits)debugCallback;
#endif
#endif
			break;

		case BR_Cycles:
			// cycles went negative, return from the core to handle
			// external events, schedule the pic...
#if C_DEBUG
#if C_HEAVY_DEBUG
			if (DEBUG_HeavyIsBreakpoint()) return (Bits)debugCallback;
#endif
#endif
			return CBRET_NONE;

		case BR_CallBack:
			// the callback code is executed in dosbox.conf, return the callback number
			FillFlags();
			return (Bits)core_dynrec.callback;

		case BR_SMCBlock:
//			LOG_MSG("selfmodification of running block at %x:%x",SegValue(cs),reg_eip);
			cpu.exception.which=0;
			// fallthrough, let the normal core handle the block-modifying instruction
		case BR_Opcode:
#if (C_DEBUG)
		case BR_OpcodeFull:
#endif
			// some instruction has been encountered that could not be translated
			// (thus it is not part of the code block), the normal core will
			// handle this instruction
			CPU_CycleLeft+=CPU_Cycles;
			CPU_Cycles=1;
			return CPU_Core_Normal_Run();

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
#if defined(WIN32) && defined(_M_ARM)
    is_win10 = IsWin10OrGreater();
#endif
}

void CPU_Core_Dynrec_Cache_Init(bool enable_cache) {
	// Initialize code cache and dynamic blocks
	cache_init(enable_cache);
}

void CPU_Core_Dynrec_Cache_Close(void) {
	cache_close();
}

void CPU_Core_Dynrec_Cache_Reset(void) {
	cache_reset();
}
#endif
