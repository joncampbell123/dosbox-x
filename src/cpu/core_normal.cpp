/*
 *  Copyright (C) 2002-2015  The DOSBox Team
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

#include "config.h"

#include <stdio.h>

#if defined(LINUX)
# include <sys/types.h>
# include <sys/ptrace.h>
# include <sys/mman.h>
# include <sys/wait.h>
# include <signal.h>
# include <errno.h>
# include <string.h>
# include <unistd.h>
# include <fcntl.h>
#endif

#include "dosbox.h"
#include "mem.h"
#include "cpu.h"
#include "lazyflags.h"
#include "inout.h"
#include "callback.h"
#include "pic.h"
#include "fpu.h"
#include "paging.h"
#include "mmx.h"

#define CPU_CORE CPU_ARCHTYPE_386

#define DoString DoString_Normal

extern bool ignore_opcode_63;

#if C_DEBUG
#include "debug.h"
#endif

#if (!C_CORE_INLINE)
#define LoadMb(off) mem_readb(off)
#define LoadMw(off) mem_readw(off)
#define LoadMd(off) mem_readd(off)
#define LoadMq(off) ((Bit64u)((Bit64u)mem_readd(off+4)<<32 | (Bit64u)mem_readd(off)))
#define SaveMb(off,val)	mem_writeb(off,val)
#define SaveMw(off,val)	mem_writew(off,val)
#define SaveMd(off,val)	mem_writed(off,val)
#define SaveMq(off,val) {mem_writed(off,val&0xffffffff);mem_writed(off+4,(val>>32)&0xffffffff);}
#else 
#include "paging.h"
#define LoadMb(off) mem_readb_inline(off)
#define LoadMw(off) mem_readw_inline(off)
#define LoadMd(off) mem_readd_inline(off)
#define LoadMq(off) ((Bit64u)((Bit64u)mem_readd_inline(off+4)<<32 | (Bit64u)mem_readd_inline(off)))
#define SaveMb(off,val)	mem_writeb_inline(off,val)
#define SaveMw(off,val)	mem_writew_inline(off,val)
#define SaveMd(off,val)	mem_writed_inline(off,val)
#define SaveMq(off,val) {mem_writed_inline(off,val&0xffffffff);mem_writed_inline(off+4,(val>>32)&0xffffffff);}
#endif

Bitu cycle_count;

#if C_FPU
#define CPU_FPU	1						//Enable FPU escape instructions
#endif

#define CPU_PIC_CHECK 1
#define CPU_TRAP_CHECK 1

#define OPCODE_NONE			0x000
#define OPCODE_0F			0x100
#define OPCODE_SIZE			0x200

#define PREFIX_ADDR			0x1
#define PREFIX_REP			0x2

#define TEST_PREFIX_ADDR	(core.prefixes & PREFIX_ADDR)
#define TEST_PREFIX_REP		(core.prefixes & PREFIX_REP)

#define DO_PREFIX_SEG(_SEG)					\
	BaseDS=SegBase(_SEG);					\
	BaseSS=SegBase(_SEG);					\
	core.base_val_ds=_SEG;					\
	goto restart_opcode;

#define DO_PREFIX_ADDR()								\
	core.prefixes=(core.prefixes & ~PREFIX_ADDR) |		\
	(cpu.code.big ^ PREFIX_ADDR);						\
	core.ea_table=&EATable[(core.prefixes&1) * 256];	\
	goto restart_opcode;

#define DO_PREFIX_REP(_ZERO)				\
	core.prefixes|=PREFIX_REP;				\
	core.rep_zero=_ZERO;					\
	goto restart_opcode;

typedef PhysPt (*GetEAHandler)(void);

static const Bit32u AddrMaskTable[2]={0x0000ffff,0xffffffff};

static struct {
	Bitu opcode_index;
	PhysPt cseip;
	PhysPt base_ds,base_ss;
	SegNames base_val_ds;
	bool rep_zero;
	Bitu prefixes;
	GetEAHandler * ea_table;
} core;

#define GETIP		(core.cseip-SegBase(cs))
#define SAVEIP		reg_eip=GETIP;
#define LOADIP		core.cseip=(SegBase(cs)+reg_eip);

#define SegBase(c)	SegPhys(c)
#define BaseDS		core.base_ds
#define BaseSS		core.base_ss

static INLINE Bit8u Fetchb() {
	Bit8u temp=LoadMb(core.cseip);
	core.cseip+=1;
	return temp;
}

static INLINE Bit16u Fetchw() {
	Bit16u temp=LoadMw(core.cseip);
	core.cseip+=2;
	return temp;
}
static INLINE Bit32u Fetchd() {
	Bit32u temp=LoadMd(core.cseip);
	core.cseip+=4;
	return temp;
}

#define Push_16 CPU_Push16
#define Push_32 CPU_Push32
#define Pop_16 CPU_Pop16
#define Pop_32 CPU_Pop32

#include "instructions.h"
#include "core_normal/support.h"
#include "core_normal/string.h"


#define EALookupTable (core.ea_table)

extern Bitu dosbox_check_nonrecursive_pf_cs;
extern Bitu dosbox_check_nonrecursive_pf_eip;

Bits CPU_Core_Normal_Run(void) {
	while (CPU_Cycles-->0) {
		LOADIP;
		dosbox_check_nonrecursive_pf_cs = SegValue(cs);
		dosbox_check_nonrecursive_pf_eip = reg_eip;
		core.opcode_index=cpu.code.big*0x200;
		core.prefixes=cpu.code.big;
		core.ea_table=&EATable[cpu.code.big*256];
		BaseDS=SegBase(ds);
		BaseSS=SegBase(ss);
		core.base_val_ds=ds;
#if C_DEBUG
#if C_HEAVY_DEBUG
		if (DEBUG_HeavyIsBreakpoint()) {
			FillFlags();
			return debugCallback;
		};
#endif
#endif
		cycle_count++;
restart_opcode:
		switch (core.opcode_index+Fetchb()) {
		#include "core_normal/prefix_none.h"
		#include "core_normal/prefix_0f.h"
		#include "core_normal/prefix_66.h"
		#include "core_normal/prefix_66_0f.h"
		default:
		illegal_opcode:
#if C_DEBUG	
			{
				bool ignore=false;
				Bitu len=(GETIP-reg_eip);
				LOADIP;
				if (len>16) len=16;
				char tempcode[16*2+1];char * writecode=tempcode;
				if (ignore_opcode_63 && mem_readb(core.cseip) == 0x63)
					ignore = true;
				for (;len>0;len--) {
					sprintf(writecode,"%02X",mem_readb(core.cseip++));
					writecode+=2;
				}
				if (!ignore)
					LOG(LOG_CPU,LOG_NORMAL)("Illegal/Unhandled opcode %s",tempcode);
			}
#endif
			CPU_Exception(6,0);
			continue;
		gp_fault:
			LOG_MSG("Segment limit violation");
			CPU_Exception(EXCEPTION_GP,0);
			continue;
		}
		SAVEIP;
	}
	FillFlags();
	return CBRET_NONE;
decode_end:
	SAVEIP;
	FillFlags();
	return CBRET_NONE;
}

Bits CPU_Core_Normal_Trap_Run(void) {
	Bits oldCycles = CPU_Cycles;
	CPU_Cycles = 1;
	cpu.trap_skip = false;

	Bits ret=CPU_Core_Normal_Run();
	if (!cpu.trap_skip) CPU_HW_Interrupt(1);
	CPU_Cycles = oldCycles-1;
	cpudecoder = &CPU_Core_Normal_Run;

	return ret;
}



void CPU_Core_Normal_Init(void) {

}

#if defined(LINUX)
/* Next development step:
 *
 * Instead of actually copying the entire LDT, we generate LDTs only for
 * the segment registers of the CPU that match their values. After execution,
 * we clear the LDT entries. 32-bit programs generally do not reload registers.
 * 16-bit programs generally do, but, when they load a different register we'll
 * catch it as an exception and update the LDT to match. */

/* Possible problem:
 *
 * We're going to modify the LDT in THIS process,
 * expecting it to take effect in the ptrace process.
 * Does that work?
 *
 * Remember that when we called clone() we asked Linux to keep the memory map the
 * same between us and the ptrace process.
 *
 * A cursory scan of the Linux kernel source suggests that the "mm" member of the
 * process is the memory map/state.
 *
 * On context switch, the Linux kernel will call switch_mm if changing memory maps.
 *
 * modify_ldt allocates/updates the LDT table which is kept in the mm structure.
 *
 * Therefore, though nobody has confirmed this yet, having the same memory map
 * between us and ptrace_pid *probably* means that both of us will have the
 * same LDT table and therefore modifications from THIS process will take effect
 * in the ptrace process. */

bool ptrace_compatible_segment(const uint16_t sv) {
    return (sv == 0/*NULL descriptor*/ || (sv & 7) == 7/*LDT ring-3 descriptor*/);
}

/* TODO: Move to it's own source file */
bool cpu_state_ptrace_compatible(void) {
    /* The CPU is in a ptrace-compatible state IF:
     * - The CPU is in protected mode
     * - The CPU is NOT running in virtual 8086 mode
     * - All segment registers contain either NULL or a ring-3 LDT descriptor */
    if (!cpu.pmode) return false;   // Protected mode or bust
    if (GETFLAG(VM)) return false;  // Virtual 8086 is not supported

    if (!ptrace_compatible_segment(Segs.val[cs])) return false;
    if (!ptrace_compatible_segment(Segs.val[ds])) return false;
    if (!ptrace_compatible_segment(Segs.val[es])) return false;
    if (!ptrace_compatible_segment(Segs.val[ss])) return false;

    if (CPU_ArchitectureType >= CPU_ARCHTYPE_386) {
        if (!ptrace_compatible_segment(Segs.val[fs])) return false;
        if (!ptrace_compatible_segment(Segs.val[gs])) return false;
    }

    return true;
}

size_t ptrace_user_size = 0;
unsigned char *ptrace_user_base = NULL;
std::vector<bool> ptrace_user_mapped;

unsigned char ptrace_process_stack[4096];/*enough to get going*/
bool ptrace_failed = false;
pid_t ptrace_pid = -1;

volatile unsigned int ptrace_process_wait = 0;
volatile unsigned int ptrace_process_waiting = 0;

size_t ptrace_user_determine_size(void) {
    if (ptrace_user_size == 0) {
#if defined(__x86_64__)
        ptrace_user_size = (4096UL * 1024UL * 1024UL); // the full 4GB is available
#elif defined(__i386__)
        ptrace_user_size = (512UL * 1024UL * 1024UL); // take a conservative 512MB
#endif
    }

    return ptrace_user_size;
}

bool ptrace_user_ptr_mapped(const void * const p) {
    if (p < ptrace_user_base || p >= (const void*)((const unsigned char*)ptrace_user_base+ptrace_user_size))
        return false;

    size_t elem = (size_t)(((uintptr_t)p - (uintptr_t)ptrace_user_base) >> (uintptr_t)12);

    if (elem >= ptrace_user_mapped.size())
        return false;

    return ptrace_user_mapped[elem];
}

void ptrace_user_ptr_mapset(const void * const p,const bool flag) {
    if (p < ptrace_user_base || p >= (const void*)((const unsigned char*)ptrace_user_base+ptrace_user_size))
        return;

    size_t elem = (size_t)(((uintptr_t)p - (uintptr_t)ptrace_user_base) >> (uintptr_t)12);

    if (elem >= ptrace_user_mapped.size()) {
        ptrace_user_mapped.resize(elem+1);
        assert(elem < ptrace_user_mapped.size());
    }

    ptrace_user_mapped[elem] = flag;
}

// i.e. clearing the cache
bool ptrace_user_map_reset(void) {
    if (ptrace_user_base == NULL)
        return true;

    ptrace_user_mapped.clear();
    if (mprotect(ptrace_user_base,ptrace_user_size,PROT_NONE)) {
        LOG_MSG("Ptrace core: mprotect failed (clearing user map)");
        ptrace_user_base = NULL;
        ptrace_failed = true;
        return false;
    }

    // we expect PROT_NONE to cause msync to fail. does it?
    if (ptrace_user_ptr_mapped(ptrace_user_base)) {
        LOG_MSG("Ptrace core: mmap PROT_NONE is still valid according to ptr test");
        ptrace_failed = true;
        return false;
    }

    return true;
}

bool ptrace_user_map(void) {
    if (ptrace_user_determine_size() == 0)
        return false;

    if (ptrace_user_base != NULL)
        return true;

    ptrace_user_mapped.clear();
    ptrace_user_base = (unsigned char*)mmap(NULL,ptrace_user_size,PROT_NONE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if (ptrace_user_base == (unsigned char*)MAP_FAILED) {
        LOG_MSG("Ptrace core: mmap failed, cannot determine user base");
        ptrace_user_base = NULL;
        return false;
    }

    LOG_MSG("Ptrace core: user area mapped at %p-%p (%luMB)",
        (void*)ptrace_user_base,(void*)(ptrace_user_base+ptrace_user_size-1UL),(ptrace_user_size + 0xFFFFFUL) >> 20UL);

    /* testing */
    if (!ptrace_user_map_reset())
        return false;

    return true;
}

void ptrace_user_unmap(void) {
    if (ptrace_user_base != NULL) {
        LOG_MSG("Ptrace core: user area unmapped");
        munmap(ptrace_user_base,ptrace_user_size);
        ptrace_user_base = NULL;
    }
}

bool ptrace_process_stopped(void) {
    if (ptrace_pid >= 0) {
        if (waitpid(ptrace_pid,NULL,WNOHANG) == ptrace_pid) {
            /* we just reaped it */
            ptrace_process_waiting = 0;
            ptrace_process_wait = 0;
            ptrace_pid = -1;
            return true;
        }
    }

    return false;
}

void ptrace_process_halt(void) {
    if (ptrace_pid >= 0) {
        ptrace_process_waiting = 0;
        ptrace_process_wait = 0;

        LOG_MSG("Ptrace core: Stopping process");
        kill(ptrace_pid,SIGKILL);

        LOG_MSG("Ptrace core: Waiting for process to stop");
        while (waitpid(ptrace_pid,NULL,0) != ptrace_pid) {
            if (errno == ECHILD) break;
            usleep(1000);
        }

        ptrace_user_unmap();

        ptrace_pid = -1;
    }
}

int ptrace_process(void *x) {
    while (ptrace_process_wait) {
        ptrace_process_waiting = 1;
        usleep(100000); /* try not to burn CPU while waiting */
    }
    ptrace_process_waiting = 0;
    return 0;
}

bool ptrace_pid_start(void) {
    if (ptrace_pid >= 0)
        return true;

    ptrace_process_waiting = 0;
    ptrace_process_wait = 1;

    ptrace_pid = clone(ptrace_process,ptrace_process_stack+sizeof(ptrace_process_stack)-16,SIGCHLD | CLONE_VM | CLONE_UNTRACED,NULL);
    if (ptrace_pid < 0) {
        LOG_MSG("Ptrace core: failed to clone, %s",strerror(errno));
        return false;
    }
    LOG_MSG("Ptrace core: Ptrace process pid %lu",(unsigned long)ptrace_pid);

    return true;
}

bool ptrace_sync(void) {
    if (ptrace_pid < 0) {
        if (!ptrace_user_map()) {
            LOG_MSG("Ptrace core: cannot determine user map\n");
            ptrace_failed = true;
            return false;
        }

        if (!ptrace_pid_start()) {
            LOG_MSG("Ptrace core: failed to start PID\n");
            ptrace_failed = true;
            return false;
        }
    }
    else {
        if (ptrace_process_stopped()) {
            LOG_MSG("Ptrace core: WARNING, process stopped\n");
            return false;
        }

        if (ptrace_process_waiting) {
            if (ptrace_process_wait) {
                LOG_MSG("Ptrace core: PID is ready, waiting.\n");
                ptrace_process_wait = false;
            }

            return false;
        }
    }

    return true;
}

/* TODO: Move to it's own source file */
Bits CPU_Core_Ptrace_Run(void) {
    if (!ptrace_failed) {
        if (cpu_state_ptrace_compatible()) {
            if (!ptrace_sync())
                return CPU_Core_Normal_Run();

            return CBRET_NONE;
        }
    }

    return CPU_Core_Normal_Run();
}
#endif

