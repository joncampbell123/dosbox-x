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
# include <sys/prctl.h>
# include <sys/mman.h>
# include <sys/wait.h>
# include <sys/user.h>
# include <asm/ldt.h> /* user_desc */
# include <syscall.h>
# include <signal.h>
# include <errno.h>
# include <string.h>
# include <unistd.h>
# include <fcntl.h>

# include "timer.h"
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
/* Update: sys_clone is handled by do_fork() in the Linux kernel. do_fork() if
 *         CLONE_VM is set will addref the current task's mm struct and assign
 *         directly to the new task, instead of calling dup_mm(). Therefore
 *         when CLONE_VM is set, both parent and child have the same memory
 *         map and therefore the same LDT table. */

bool ptrace_compatible_segment(const uint16_t sv) {
    return (sv == 0/*NULL descriptor*/ || (sv & 7) == 7/*LDT ring-3 descriptor*/);
}

/* TODO: Move to it's own source file */
bool cpu_state_ptrace_compatible(void) {
    /* The CPU is in a ptrace-compatible state IF:
     * - A20 gate must be enabled
     * - The CPU is in protected mode
     * - The CPU is NOT running in virtual 8086 mode
     * - All segment registers contain either NULL or a ring-3 LDT descriptor */
    if (!MEM_A20_Enabled()) return false; // A20 must be enabled
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

uint16_t ldt_current_sreg[gs+1]; /* NTS: include/regs.h es=0 gs=last */

int modify_ldt(int func,void *ptr,unsigned long bytecount) {
    return (int)syscall(__NR_modify_ldt,func,ptr,bytecount);
}

bool ptrace_clear_ldt_segment(uint16_t &seg) {
    if (seg == 0 || (seg&7) != 7)
        return true;

    struct user_desc ud;

    memset(&ud,0,sizeof(ud));
    ud.entry_number = seg >> 3;
    ud.seg_not_present = 1;
    ud.read_exec_only = 1;

    if (modify_ldt(1,&ud,sizeof(ud)) == 0) {
        seg = 0;
    }
    else {
        LOG_MSG("Ptrace core: warning, clear_ldt_segment failed for segment 0x%x",seg);
        return false;
    }

    return true;
}

bool ptrace_ldt_load(uint16_t &seg,unsigned int sreg) {
    if (Segs.val[sreg] == 0) {
        if (seg != 0)
            return ptrace_clear_ldt_segment(seg);

        return true;
    }

    /* the base MUST fit within 32 bits and within user area */
    uintptr_t newbase = (uintptr_t)ptrace_user_base + (uintptr_t)Segs.phys[sreg];
    if (newbase > (uintptr_t)0xFFFFFFFFUL)
        return false;
    if (newbase > ((uintptr_t)ptrace_user_base+(uintptr_t)ptrace_user_size))
        return false;

    /* even if the segment value is the same, the guest COULD change the LDT descriptor's state */
    struct user_desc ud;

    memset(&ud,0,sizeof(ud));
    ud.entry_number = Segs.val[sreg] >> 3;
    ud.seg_not_present = 0;     // FIXME: How do we detect if the guest has marked the segment not present?
    ud.read_exec_only = 0;      // FIXME: How do we detect if the guest marked the segment read/execute only?
    ud.contents = 0;            // bits [1:0] of contents become bits [3:2] of type field
    if (sreg == cs)
        ud.contents |= 2;       // code segment (else, data)
    // FIXME: How do we detect if the guest marked the segment as conforming/expand-down?
    ud.base_addr = newbase;
    ud.limit = Segs.limit[sreg];
    // must cap the limit to stay within our user mapping
    if (((uintptr_t)ud.base_addr+(uintptr_t)ud.limit) >= ((uintptr_t)ptrace_user_base+(uintptr_t)ptrace_user_size))
        ud.limit = ((uintptr_t)ptrace_user_base+(uintptr_t)ptrace_user_size-(uintptr_t)ud.base_addr) - 1;

    if (ud.limit > 0xFFFFUL) {
        unsigned long nl = ((unsigned long)ud.limit + 0xFFFUL) >> 12UL;
        if (nl > 0xFFFFFUL) nl = 0xFFFFFUL;
        ud.limit_in_pages = 1;
    }

    if (sreg == cs)
        ud.seg_32bit = cpu.code.big?1:0;
    else
        ud.seg_32bit = cpu.stack.big?1:0;

    // we're having issues with 16-bit code?
    if (!ud.seg_32bit)
        return false;

    ud.useable = 1;

    if (modify_ldt(1,&ud,sizeof(ud)) == 0) {
        LOG_MSG("Ptrace core: sreg=%u seg=0x%x base=0x%lx limit=0x%lx in_pages=%u",(unsigned int)sreg,(unsigned int)seg,(unsigned long)ud.base_addr,(unsigned long)ud.limit,(unsigned int)ud.limit_in_pages);
        seg = Segs.val[sreg];
    }
    else {
        LOG_MSG("Ptrace core: warning, clear_ldt_segment failed for segment 0x%x",seg);
        return false;
    }

    return true;
}

size_t ptrace_user_determine_size(void) {
    if (ptrace_user_size == 0) {
#if defined(__x86_64__)
        ptrace_user_size = (512UL * 1024UL * 1024UL); // take a conservative 512MB, must exist below 4GB
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

    /* we MUST make sure the mapping is below 4GB. modify_ldt() only supports 32-bit wide base addresses for the LDT selector */
    ptrace_user_mapped.clear();
    ptrace_user_base = (unsigned char*)mmap(NULL,ptrace_user_size,PROT_NONE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_32BIT,-1,0);
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

bool ptrace_process_check(int *status,bool wait=false) {
    *status = -1;
    if (ptrace_pid >= 0) {
        if (waitpid(ptrace_pid,status,WUNTRACED | (wait ? 0 : WNOHANG)) == ptrace_pid) {
            if (WIFEXITED(*status)) {
                /* we just reaped it */
                ptrace_process_waiting = 0;
                ptrace_process_wait = 0;
                ptrace_pid = -1;
                return true;
            }
            else if (WIFSTOPPED(*status)) {
                LOG_MSG("Ptrace core: ptrace process stopped by signal %u",WSTOPSIG(*status));
            }
            else if (WIFSIGNALED(*status)) {
                LOG_MSG("Ptrace core: ptrace process stopped/signalled by signal %u",WTERMSIG(*status));
            }
            else {
                LOG_MSG("Ptrace core: ptrace event status 0x%08lx",(unsigned long)(*status));
            }
        }
    }

    return false;
}

bool ptrace_process_stopped(void) {
    if (ptrace_pid >= 0) {
        int status;
        if (waitpid(ptrace_pid,&status,WNOHANG) == ptrace_pid) {
            if (WIFEXITED(status)) {
                /* we just reaped it */
                ptrace_process_waiting = 0;
                ptrace_process_wait = 0;
                ptrace_pid = -1;
                return true;
            }
            else if (WIFSTOPPED(status)) {
                LOG_MSG("Ptrace core: ptrace process stopped by signal %u",WSTOPSIG(status));
            }
            else if (WIFSIGNALED(status)) {
                LOG_MSG("Ptrace core: ptrace process stopped by signal %u",WTERMSIG(status));
            }
            else {
                LOG_MSG("Ptrace core: ptrace event status 0x%08lx",(unsigned long)status);
            }
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

        for (unsigned int sr=0;sr <= gs;sr++) { /* NTS: include/regs. es=0 gs=last */
            ptrace_clear_ldt_segment(/*&*/ldt_current_sreg[sr]);
        }

        ptrace_pid = -1;
    }
}

int ptrace_process(void *x) {
    prctl(PR_SET_TSC,PR_TSC_SIGSEGV); /* make sure guest cannot read TSC directly */
    while (ptrace_process_wait) {
        ptrace_process_waiting = 1;
        usleep(100000); /* try not to burn CPU while waiting */
    }
    /* main process will use ptrace() to redirect our execution */
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

/* WARNING: this code ASSUMES SIGSTOP is pending! */
bool ptrace_wait_SIGSTOP(void) {
    int sign=0;

    while (waitpid(ptrace_pid,&sign,WUNTRACED) != ptrace_pid) {
        if (WIFSTOPPED(sign)) {
            if (WSTOPSIG(sign) == SIGSTOP) {
                LOG_MSG("PTrace core: ptrace process SIGSTOP'd");
            }
            else {
                LOG_MSG("PTrace core: ptrace process stopped by another signal. Giving up.");
                ptrace_failed = true;
                return false;
            }
        }
        else if (WIFEXITED(sign)) {
            ptrace_process_stopped();
            LOG_MSG("PTrace core: ptrace process stopped normally, unexpectedly");
            return false;
        }
        else {
            LOG_MSG("PTrace core: ptrace process stopped by reasons other than SIGSTOP. Giving up.");
            ptrace_failed = true;
            return false;
        }
    }

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

                /* make sure the process is ready for our control.
                 * assuming it's stopped, properly, waitpid() will not return SIGSTOP */
                if (kill(ptrace_pid,SIGSTOP)) {
                    LOG_MSG("Ptrace core: failed to SIGSTOP ptrace process\n");
                    ptrace_failed = true;
                    return false;
                }

                // the child process is sent SIGSTOP, but not right away.
                if (!ptrace_wait_SIGSTOP())
                    return false;

                if (ptrace(PTRACE_ATTACH,ptrace_pid,NULL,NULL)) {
                    LOG_MSG("PTrace core: failed to attach to process via ptrace");
                    ptrace_failed = true;
                    return false;
                }

                // the child process is sent SIGSTOP, but not right away.
                if (!ptrace_wait_SIGSTOP())
                    return false;

                LOG_MSG("PTrace core: successfully attached to process via ptrace");
                ptrace_process_waiting = false;
                ptrace_process_wait = false;
            }
        }
    }

    if (ptrace_pid >= 0) {
        if (!ptrace_user_map_reset())
            return false;
    }

    if (ptrace_pid >= 0 && !ptrace_process_wait && !ptrace_process_waiting) {
        for (unsigned int sr=0;sr <= gs;sr++) { /* NTS: include/regs. es=0 gs=last */
            if (!ptrace_ldt_load(/*&*/ldt_current_sreg[sr],sr)) {
//                LOG_MSG("Ptrace core: failed to load segreg register from guest into LDT\n");
                /* non-fatal */
                return false;
            }
        }

        if (ptrace_process_stopped()) {
            LOG_MSG("Ptrace core: WARNING, process stopped\n");
            return false;
        }

        {
            int status;
            ptrace_process_check(&status,false/*wait*/);
        }

        /* copy our CPU state into the process state */
#if defined(__x86_64__)
        {
            struct user_regs_struct r;

            if (ptrace(PTRACE_GETREGS,ptrace_pid,&r,&r) < 0) {
                LOG_MSG("Ptrace core: failed to read CPU state from ptrace process. err=%s",strerror(errno));
                ptrace_failed = true;
                return false;
            }

            r.cs = ldt_current_sreg[cs];
            r.ds = ldt_current_sreg[ds];
            r.es = ldt_current_sreg[es];
            r.fs = ldt_current_sreg[fs];
            r.gs = ldt_current_sreg[gs];
            r.ss = ldt_current_sreg[ss];
            r.eflags = reg_flags;
            r.fs_base = 0;
            r.gs_base = 0;
            r.rsp = reg_esp;
            r.rip = reg_eip;
            r.rax = r.orig_rax = reg_eax;
            r.rbx = reg_ebx;
            r.rcx = reg_ecx;
            r.rdx = reg_edx;
            r.rsi = reg_esi;
            r.rdi = reg_edi;
            r.rbp = reg_ebp;

            if (ptrace(PTRACE_SETREGS,ptrace_pid,&r,&r) < 0) {
                LOG_MSG("Ptrace core: failed to copy CPU state to ptrace process. err=%s",strerror(errno));
                ptrace_failed = true;
                return false;
            }

            if (ptrace(PTRACE_GETREGS,ptrace_pid,&r,&r) < 0) {
                LOG_MSG("Ptrace core: failed to read CPU state from ptrace process. err=%s",strerror(errno));
                ptrace_failed = true;
                return false;
            }

            LOG_MSG("cs:ip %04X:%04lX before",(unsigned int)r.cs,(unsigned long)r.rip);
        }
#else
# error unsupported target
#endif

try_again:
        LOG_MSG("ptrace core");

        /* GO! */
        if (ptrace(PTRACE_CONT,ptrace_pid,NULL,NULL)) {
            LOG_MSG("Ptrace core: failed to start ptrace process");
            ptrace_failed = true;
            return false;
        }

		PAGING_ClearTLB();

        // NTS: Obviously the only thing this subprocess is going to do is page fault (SIGSEGV) a
        //      lot because I haven't yet gotten the code to map pages in!

        /* run for one millsecond or until fault */
        Bitu bt = GetTicks();
        int status = 0;

        while (GetTicks() == bt) {
            usleep(100);
            if (ptrace_process_check(&status,false/*wait*/)) {
                LOG_MSG("Ptrace core: WARNING, process stopped\n");
                return false;
            }
            else if (status != -1) {
                break;
            }
        }

        if (status != -1 && WIFSTOPPED(status)) {
            struct user_regs_struct r;

            /* flush back into ours */
            if (msync(ptrace_user_base,ptrace_user_size,MS_INVALIDATE | MS_SYNC))
                LOG_MSG("msync error %s",strerror(errno));

            /* copy CPU state back into ours */
            if (ptrace(PTRACE_GETREGS,ptrace_pid,&r,&r) < 0) {
                LOG_MSG("Ptrace core: failed to read CPU state from ptrace process. err=%s",strerror(errno));
                ptrace_failed = true;
                return false;
            }

            CPU_SetSegGeneral(cs,r.cs);
            CPU_SetSegGeneral(ds,r.ds);
            CPU_SetSegGeneral(es,r.es);
            CPU_SetSegGeneral(fs,r.fs);
            CPU_SetSegGeneral(gs,r.gs);
            CPU_SetSegGeneral(ss,r.ss);
            reg_flags = r.eflags;
            reg_esp = r.rsp;
            reg_eip = r.rip;
            reg_eax = r.rax;
            reg_ebx = r.rbx;
            reg_ecx = r.rcx;
            reg_edx = r.rdx;
            reg_esi = r.rsi;
            reg_edi = r.rdi;
            reg_ebp = r.rbp;

            LOG_MSG("cs:ip %04X:%04lX after",(unsigned int)r.cs,(unsigned long)r.rip);

            /* most common case: it stopped because of a page fault (SIGSEGV) which is probably
             * our fault since we don't map pages in by default. */
            if (WSTOPSIG(status) == SIGSEGV) {
                siginfo_t si;

                if (ptrace(PTRACE_GETSIGINFO,ptrace_pid,&si,&si) == 0) {
                    void *fixed_si_addr = si.si_addr;

                    if (si.si_signo == SIGSEGV && si.si_code == SEGV_ACCERR &&
                        (unsigned char*)fixed_si_addr >= ptrace_user_base &&
                        (unsigned char*)fixed_si_addr < (ptrace_user_base+ptrace_user_size)) {
                        uintptr_t guest_vma = (uintptr_t)fixed_si_addr - (uintptr_t)ptrace_user_base;
                        uintptr_t guest_vma_page = guest_vma & ~((uintptr_t)0xFFFUL);
                        LOG_MSG("Guest SIGSEGV at %p (page %p) (flat %p)",(void*)guest_vma,(void*)guest_vma_page,fixed_si_addr);

                        /* if we already took care of it, then it's something the normal core needs to run */
                        if (ptrace_user_ptr_mapped(fixed_si_addr)) {
                            DEBUG_EnableDebugger();

                            LOG_MSG("normal core needs to handle this");
                            return false;
                        }

                        bool HandlerIsMem(PageHandler *ph);

                        extern HostPt MemBase;
                        extern int MemBaseFd;

                        /* okay, let's try to handle it.
                         * what memory page does that correspond to? */
                        uintptr_t guest_pma_page = PAGING_GetPhysicalPage(guest_vma_page);
                        PageHandler *ph = MEM_GetPageHandler(guest_pma_page >> 12UL);
                        if (ph == NULL) {
                            LOG_MSG("unable to get page handler. normal core needs to handle this");
                            return false;
                        }
                        /* currently we can only allow system memory, because system memory is known to hold the
                         * file handle we need for mapping */
                        else if (HandlerIsMem(ph) && (guest_vma>>12UL) < MEM_TotalPages() && MemBase != NULL && MemBaseFd >= 0) {
                            LOG_MSG("page handler is system memory! we can do this. vma=%p pma=%p",(void*)guest_vma_page,(void*)guest_pma_page);

                            /* make sure contents make it */
                            msync(ptrace_user_base + guest_vma_page,4096,MS_INVALIDATE | MS_SYNC);

                            /* unmap the affected region */
                            munmap(ptrace_user_base + guest_vma_page,4096);
                            /* map in the system memory */
                            /* FIXME: We need to read guest page tables, enforce read-only, read-write, executable bits */
                            if (mmap(ptrace_user_base + guest_vma_page,4096,PROT_READ|PROT_WRITE|PROT_EXEC,MAP_SHARED|MAP_FIXED,MemBaseFd,guest_pma_page) == MAP_FAILED) {
                                LOG_MSG("failed to map in system memory page, error=%p",strerror(errno));
                                return false;
                            }

                            /* go back and try again! */
                            ptrace_user_ptr_mapset(fixed_si_addr,true);
                            goto try_again;
                        }
                        else {
                            LOG_MSG("page handler not recognized. normal core needs to handle this");
                            return false;
                        }
                    }
                    else if (si.si_code == 0x80) {
                        /* Linux doesn't give the address of the INT instruction, just si.si_addr == 0, so... */
                        uintptr_t fault_addr = (uintptr_t)Segs.phys[cs] + (uintptr_t)reg_eip + (uintptr_t)ptrace_user_base;
                        /* undocumented behavior: SIGSEGV with code == 0 or 0x80 can mean the guest attempted the INT instruction */
                        unsigned int opcode = ptrace(PTRACE_PEEKDATA,ptrace_pid,(void*)fault_addr,NULL);

                        /* is it an INT instruction? */
                        if ((opcode & 0xFF) == 0xCD) {
                            DEBUG_EnableDebugger();
                            LOG_MSG("Ptrace core: Guest attempted INT %02x AX=%x at %p",
                                (opcode >> 8) & 0xFF,
                                reg_ax,
                                (void*)(fault_addr - (uintptr_t)ptrace_user_base));
                            CPU_Cycles = 1;
                            return false; // let the normal core handle it!
                        }
                        /* CLI/STI? */
                        else if ((opcode & 0xFF) == 0xFB || (opcode && 0xFF) == 0xFA) {
                        DEBUG_EnableDebugger();
                            LOG_MSG("Ptrace core: Guest attempted CLI/STI at %p",(void*)(fault_addr - (uintptr_t)ptrace_user_base));
                            CPU_Cycles = 1;
                            return false; // let the normal core handle it!
                        }

                        DEBUG_EnableDebugger();
                        LOG_MSG("SIGSEGV fault other than page, fault=%p opcode=0x%x",(void*)fault_addr,opcode);
                    }
                    else {
                        DEBUG_EnableDebugger();
                        LOG_MSG("Unexpected SIGSEGV outside of guest area at %p (guest %p-%p) code=%u",
                            fixed_si_addr,
                            ptrace_user_base,
                            ptrace_user_base+ptrace_user_size-1,
                            si.si_code);
                    }
                }
                else {
                    LOG_MSG("Ptrace core: SIGSEGV but unable to get siginfo");
                }
            }
            else if (WSTOPSIG(status) != SIGSTOP) {
                LOG_MSG("Ptrace core: Stopped by some other signal %u",WSTOPSIG(status));
            }
        }

        if (status == -1) {
            /* nothing happened, therefore loop ran for 1ms */
            CPU_Cycles = 1; // FIXME we can be more precise than this!

            if (!WIFSTOPPED(status)) {
                kill(ptrace_pid,SIGSTOP);

                /* and then we have to pick it up */
                ptrace_process_check(&status,true/*wait*/);
            }

            {
                struct user_regs_struct r;

                /* flush back into ours */
                if (msync(ptrace_user_base,ptrace_user_size,MS_INVALIDATE | MS_SYNC))
                    LOG_MSG("msync error %s",strerror(errno));

                /* copy CPU state back into ours */
                if (ptrace(PTRACE_GETREGS,ptrace_pid,&r,&r) < 0) {
                    LOG_MSG("Ptrace core: failed to read CPU state from ptrace process. err=%s",strerror(errno));
                    ptrace_failed = true;
                    return false;
                }

                CPU_SetSegGeneral(cs,r.cs);
                CPU_SetSegGeneral(ds,r.ds);
                CPU_SetSegGeneral(es,r.es);
                CPU_SetSegGeneral(fs,r.fs);
                CPU_SetSegGeneral(gs,r.gs);
                CPU_SetSegGeneral(ss,r.ss);
                reg_flags = r.eflags;
                reg_esp = r.rsp;
                reg_eip = r.rip;
                reg_eax = r.rax;
                reg_ebx = r.rbx;
                reg_ecx = r.rcx;
                reg_edx = r.rdx;
                reg_esi = r.rsi;
                reg_edi = r.rdi;
                reg_ebp = r.rbp;
            }
        }

        return true;
    }

    return false;
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

