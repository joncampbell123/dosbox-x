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


#include <assert.h>
#include <sstream>
#include <stddef.h>
#include "dosbox.h"
#include "cpu.h"
#include "memory.h"
#include "debug.h"
#include "mapper.h"
#include "setup.h"
#include "programs.h"
#include "paging.h"
#include "callback.h"
#include "lazyflags.h"
#include "support.h"
#include "control.h"

/* caution: do not uncomment unless you want a lot of spew */
//#define CPU_DEBUG_SPEW

#if defined(CPU_DEBUG_SPEW)
# define _LOG LOG
#else
class _LOG : public LOG { // HACK
public:
	_LOG(LOG_TYPES type , LOG_SEVERITIES severity) : LOG(type,severity) { }
};
# undef LOG
# if defined (_MSC_VER)
#  define LOG(X,Y)
# else
#  define LOG(X,Y) CPU_LOG
# define CPU_LOG(...)
# endif
#endif

bool enable_weitek = false;

bool CPU_NMI_gate = true;
bool CPU_NMI_active = false;
bool CPU_NMI_pending = false;
bool do_seg_limits = false;

bool enable_fpu = true;
bool enable_msr = true;
bool enable_cmpxchg8b = true;
bool ignore_undefined_msr = true;

extern bool ignore_opcode_63;

bool cpu_double_fault_enable;
bool cpu_triple_fault_reset;

int cpu_rep_max = 0;

Bitu DEBUG_EnableDebugger(void);
extern void GFX_SetTitle(Bit32s cycles, Bits frameskip, Bits timing, bool paused);

CPU_Regs cpu_regs;
CPUBlock cpu;
Segments Segs;

/* [cpu] setting realbig16.
 * If set, allow code to switch back to real mode with the B (big) set in the
 * code selector, and retain the state of the B bit while running in 16-bit
 * real mode. Needed for demos like Project Angel.
 *
 * Modifications are a derivative of this patch:
 *
 * cpu.diff from http://www.vogons.org/viewtopic.php?f=33&t=28226&start=5
 *
 * The main difference between that patch and the modifications derived is that
 * I modified additional points to keep the big bit set (the original cpu.diff
 * missed the CALL and JMP emulation that also reset the flag)
 *
 * It's well known that DOS programs can access all 4GB of addressable memory by
 * jumping into protected mode, loading segment registers with a 4GB limit, then
 * jumping back to real mode without reloading the segment registers, knowing
 * that Intel processors will not update the shadow part of the segment register
 * in real mode. I'm guessing that what Project Angel is doing, is using the same
 * abuse of protected mode to also set the B (big) bit in the code segment so that
 * it's code segment can extend past 64KB (huge unreal mode), which works until
 * something like an interrupt chops off the top 16 bits of the instruction pointer.
 *
 * I want to clarify that realbig16 is an OPTION that is off by default, because
 * I am uncertain at this time whether or not the patch breaks any DOS games or
 * OS emulation. It is rare for a DOS game or demo to actually abuse the CPU in
 * that way, so it is set up that you have to enable it if you need it. --J.C.
 *
 * J.C. TODO: Write a program that abuses the B (big) bit in real mode in the same
 *            way that Project Angel supposedly does, see if it works, then test it
 *            and Project Angel on some old 386/486/Pentium systems lying around to
 *            see how compatible such abuse is with PC hardware. That would make a
 *            good Hackipedia.org episode as well. --J.C.
 *
 * 2014/01/19: I can attest that this patch does indeed allow Project Angel to
 *             run when realbig16=true. And if GUS emulation is active, there is
 *             music as well. Now as for reliability... testing shows that one of
 *             three things can happen when you run the demo:
 *
 *             1) the demo hangs on startup, either right away or after it starts
 *                the ominous music (if you sit for 30 seconds waiting for the
 *                music to build up and nothing happens, consider closing the
 *                emulator and trying again).
 *
 *             2) the demo runs perfectly fine, but timing is slightly screwed up,
 *                and parts of the music sound badly out of sync with each other,
 *                or randomly slows to about 1/2 speed in some sections, animations
 *                run slow sometimes. If this happens, make sure you didn't set
 *                forcerate=ntsc.
 *
 *             3) the demo runs perfectly fine, with no timing issues, except that
 *                DOSBox's S3 emulation is not quite on-time and the bottom 1/4th
 *                of the screen flickers with the contents of the next frame that
 *                the demo is still drawing :(
 *
 *             --J.C. */
bool cpu_allow_big16 = false;

Bit32s CPU_Cycles = 0;
Bit32s CPU_CycleLeft = 3000;
Bit32s CPU_CycleMax = 3000;
Bit32s CPU_OldCycleMax = 3000;
Bit32s CPU_CyclePercUsed = 100;
Bit32s CPU_CycleLimit = -1;
Bit32s CPU_CycleUp = 0;
Bit32s CPU_CycleDown = 0;
Bit32s CPU_CyclesSet = 3000;
Bit64s CPU_IODelayRemoved = 0;
char core_mode[16];
CPU_Decoder * cpudecoder;
bool CPU_CycleAutoAdjust = false;
bool CPU_SkipCycleAutoAdjust = false;
Bitu CPU_AutoDetermineMode = 0;

Bitu CPU_ArchitectureType = CPU_ARCHTYPE_MIXED;

Bitu CPU_extflags_toggle=0;	// ID and AC flags may be toggled depending on emulated CPU architecture

Bitu CPU_PrefetchQueueSize=0;

void CPU_Core_Full_Init(void);
void CPU_Core_Normal_Init(void);
void CPU_Core_Simple_Init(void);
#if (C_DYNAMIC_X86)
void CPU_Core_Dyn_X86_Init(void);
void CPU_Core_Dyn_X86_Cache_Init(bool enable_cache);
void CPU_Core_Dyn_X86_Cache_Close(void);
void CPU_Core_Dyn_X86_SetFPUMode(bool dh_fpu);
void CPU_Core_Dyn_X86_Cache_Reset(void);
#endif

/* called to signal an NMI. */

/* NTS: From the Intel 80386 programmer's reference manual:
 *
 * "
 *   9.2.1 NMI Masks Further NMIs
 *
 *   While an NMI handler is executing, the processor ignores further interrupt
 *   signals at the NMI pin until the next IRET instruction is executed.
 * "
 *
 * This is why, further down, CPU_IRET() clears the CPU_NMI_active flag.
 *
 *
 * And, in response to my father's incredulous response regarding the fact that
 * NMI is edge-triggered (from the Intel 386SX Microprocessor datasheet):
 *
 * "
 *   Non-Maskable Interrupt Request (NMI))
 *
 *   This input indicates a request for interrupt service
 *   which cannot be masked by software. The non-
 *   maskable interrupt request is always processed ac-
 *   cording to the pointer or gate in slot 2 of the interrupt
 *   table. Because of the fixed NMI slot assignment, no
 *   interrupt acknowledge cycles are performed when
 *   processing NMI.
 *
 *   NMI is an active HIGH, rising edge-sensitive asyn-
 *   chronous signal. Setup and hold times, t27 and and t28,
 *   relative to the CLK2 signal must be met to guarantee
 *   recognition at a particular clock edge. To assure rec-
 *   ognition of NMI, it must be inactive for at least eight
 *   CLK2 periods, and then be active for at least eight
 *   CLK2 periods before the beginning of the instruction
 *   boundary in the Intel386 SX Microprocessor's Exe-
 *   cution Unit.
 *
 *   Once NMI processing has begun, no additional
 *   NMI's are processed until after the next IRET in-
 *   struction, which is typically the end of the NMI serv-
 *   ice routine. If NMI is re-asserted prior to that time,
 *   however, one rising edge on NMI will be remem-
 *   bered for processing after executing the next IRET
 *   instruction
 * "
 *
 * From the Pentium Pro Processor datasheet:
 *
 * "
 *   A.38 NMI (I)
 *
 *   The NMI signal is the Non-maskable Interrupt signal.
 *   It is the state of the LINT1 signal when APIC is
 *   disabled. Asserting NMI causes an interrupt with an
 *   internally supplied vector value of 2. An external
 *   interrupt-acknowledge transaction is not generated. If
 *   NMI is asserted during the execution of an NMI
 *   service routine, it remains pending and is recognized
 *   after the IRET is executed by the NMI service
 *   routine. At most, one assertion of NMI is held
 *   pending.
 *
 *   NMI is rising-edge sensitive. Recognition of NMI is
 *   guaranteed in a specific clock if it is asserted
 *   synchronously and meets the setup and hold times. If
 *   asserted asynchronously, active and inactive pulse
 *   widths must be a minimum of two clocks. In FRC
 *   mode, NMI must be synchronous to BCLK.
 * "
 *
 * Similar references exist in the Pentium III and Pentium 4
 * datasheets, while later on in the Core 2 datasheets there
 * is no mention whatsoever to the NMI that I can find.
 */
void CPU_NMI_Interrupt() {
	if (CPU_NMI_active) E_Exit("CPU_NMI_Interrupt() called while NMI already active");
	CPU_NMI_active = true;
	CPU_NMI_pending = false;
	CPU_Interrupt(2/*INT 2 = NMI*/,0,reg_eip);
}

void CPU_Raise_NMI() {
	CPU_NMI_pending = true;
	if (!CPU_NMI_active && CPU_NMI_gate) CPU_NMI_Interrupt();
}

void CPU_Check_NMI() {
	if (!CPU_NMI_active && CPU_NMI_gate && CPU_NMI_pending) CPU_NMI_Interrupt();
}

/* In debug mode exceptions are tested and dosbox exits when 
 * a unhandled exception state is detected. 
 * USE CHECK_EXCEPT to raise an exception in that case to see if that exception
 * solves the problem.
 * 
 * In non-debug mode dosbox doesn't do detection (and hence doesn't crash at
 * that point). (game might crash later due to the unhandled exception) */

#define CPU_CHECK_EXCEPT 1
// #define CPU_CHECK_IGNORE 1

#if C_DEBUG
// #define CPU_CHECK_EXCEPT 1
// #define CPU_CHECK_IGNORE 1
 /* Use CHECK_EXCEPT when something doesn't work to see if a exception is 
 * needed that isn't enabled by default.*/
#else
/* NORMAL NO CHECKING => More Speed */
//#define CPU_CHECK_IGNORE 1
#endif /* C_DEBUG */

#if defined(CPU_CHECK_IGNORE)
#define CPU_CHECK_COND(cond,msg,exc,sel) {	\
	if (cond) do {} while (0);				\
}
#elif defined(CPU_CHECK_EXCEPT)
#define CPU_CHECK_COND(cond,msg,exc,sel) {	\
	if (cond) {					\
		CPU_Exception(exc,sel);		\
		return;				\
	}					\
}
#else
#define CPU_CHECK_COND(cond,msg,exc,sel) {	\
	if (cond) E_Exit(msg);			\
}
#endif


void Descriptor::Load(PhysPt address) {
	cpu.mpl=0;
	Bit32u* data = (Bit32u*)&saved;
	*data	  = mem_readd(address);
	*(data+1) = mem_readd(address+4);
	cpu.mpl=3;
}
void Descriptor:: Save(PhysPt address) {
	cpu.mpl=0;
	Bit32u* data = (Bit32u*)&saved;
	mem_writed(address,*data);
	mem_writed(address+4,*(data+1));
	cpu.mpl=3;
}


void CPU_Push16(Bitu value) {
	Bit32u new_esp=(reg_esp&cpu.stack.notmask)|((reg_esp-2)&cpu.stack.mask);
	mem_writew(SegPhys(ss) + (new_esp & cpu.stack.mask) ,value);
	reg_esp=new_esp;
}

void CPU_Push32(Bitu value) {
	Bit32u new_esp=(reg_esp&cpu.stack.notmask)|((reg_esp-4)&cpu.stack.mask);
	mem_writed(SegPhys(ss) + (new_esp & cpu.stack.mask) ,value);
	reg_esp=new_esp;
}

Bitu CPU_Pop16(void) {
	Bitu val=mem_readw(SegPhys(ss) + (reg_esp & cpu.stack.mask));
	reg_esp=(reg_esp&cpu.stack.notmask)|((reg_esp+2)&cpu.stack.mask);
	return val;
}

Bitu CPU_Pop32(void) {
	Bitu val=mem_readd(SegPhys(ss) + (reg_esp & cpu.stack.mask));
	reg_esp=(reg_esp&cpu.stack.notmask)|((reg_esp+4)&cpu.stack.mask);
	return val;
}

PhysPt SelBase(Bitu sel) {
	if (cpu.cr0 & CR0_PROTECTION) {
		Descriptor desc;
		cpu.gdt.GetDescriptor(sel,desc);
		return desc.GetBase();
	} else {
		return sel<<4;
	}
}

void CPU_SetCPL(Bitu newcpl) {
	if (newcpl != cpu.cpl) {
		if (paging.enabled) {
			if ( ((cpu.cpl < 3) && (newcpl == 3)) || ((cpu.cpl == 3) && (newcpl < 3)) )
			PAGING_SwitchCPL(newcpl == 3);
		}
		cpu.cpl = newcpl;
	}
}

void CPU_SetFlags(Bitu word,Bitu mask) {
	/* 8086/286 flags manipulation.
	 * For more information read about the Intel CPU detection algorithm and other bits of info:
	 * [http://www.rcollins.org/ddj/Sep96/Sep96.html] */

	/* 8086: bits 12-15 cannot be zeroed */
	if (CPU_ArchitectureType <= CPU_ARCHTYPE_80186) {
		/* update mask and word to ensure bits 12-15 are set */
		word |= 0xF000;
		mask |= 0xF000;
	}
	/* 286 real mode: bits 12-15 bits cannot be set, always zero */
	else if (CPU_ArchitectureType <= CPU_ARCHTYPE_286) {
		if (!(cpu.cr0 & CR0_PROTECTION)) {
			/* update mask and word to ensure bits 12-15 are zero */
			word &= ~0xF000;
			mask |= 0xF000;
		}
	}
	else {
		mask |= CPU_extflags_toggle;	// ID-flag and AC-flag can be toggled on CPUID-supporting CPUs
	}

	reg_flags=(reg_flags & ~mask)|(word & mask)|2;
	cpu.direction=1-((reg_flags & FLAG_DF) >> 9);
	// ^ NTS: Notice the DF flag is bit 10. This code computes (reg_flags & FLAG_DF) >> 9 on purpose.
	//        It's not a typo (9 vs 10), it's done to set cpu.direction to either 1 or -1.
}

bool CPU_PrepareException(Bitu which,Bitu error) {
	cpu.exception.which=which;
	cpu.exception.error=error;
	return true;
}

bool CPU_CLI(void) {
	if (cpu.pmode && ((!GETFLAG(VM) && (GETFLAG_IOPL<cpu.cpl)) || (GETFLAG(VM) && (GETFLAG_IOPL<3)))) {
		return CPU_PrepareException(EXCEPTION_GP,0);
	} else {
		SETFLAGBIT(IF,false);
		return false;
	}
}

bool CPU_STI(void) {
	if (cpu.pmode && ((!GETFLAG(VM) && (GETFLAG_IOPL<cpu.cpl)) || (GETFLAG(VM) && (GETFLAG_IOPL<3)))) {
		return CPU_PrepareException(EXCEPTION_GP,0);
	} else {
 		SETFLAGBIT(IF,true);
		return false;
	}
}

bool CPU_POPF(Bitu use32) {
	if (cpu.pmode && GETFLAG(VM) && (GETFLAG(IOPL)!=FLAG_IOPL)) {
		/* Not enough privileges to execute POPF */
		return CPU_PrepareException(EXCEPTION_GP,0);
	}
	Bitu mask=FMASK_ALL;
	/* IOPL field can only be modified when CPL=0 or in real mode: */
	if (cpu.pmode && (cpu.cpl>0)) mask &= (~FLAG_IOPL);
	if (cpu.pmode && !GETFLAG(VM) && (GETFLAG_IOPL<cpu.cpl)) mask &= (~FLAG_IF);
	if (use32)
		CPU_SetFlags(CPU_Pop32(),mask);
	else CPU_SetFlags(CPU_Pop16(),mask & 0xffff);
	DestroyConditionFlags();
	return false;
}

bool CPU_PUSHF(Bitu use32) {
	if (cpu.pmode && GETFLAG(VM) && (GETFLAG(IOPL)!=FLAG_IOPL)) {
		/* Not enough privileges to execute PUSHF */
		return CPU_PrepareException(EXCEPTION_GP,0);
	}
	FillFlags();
	if (use32) 
		CPU_Push32(reg_flags & 0xfcffff);
	else CPU_Push16(reg_flags);
	return false;
}

void CPU_CheckSegments(void) {
	bool needs_invalidation=false;
	Descriptor desc;
	if (!cpu.gdt.GetDescriptor(SegValue(es),desc)) needs_invalidation=true;
	else switch (desc.Type()) {
		case DESC_DATA_EU_RO_NA:	case DESC_DATA_EU_RO_A:	case DESC_DATA_EU_RW_NA:	case DESC_DATA_EU_RW_A:
		case DESC_DATA_ED_RO_NA:	case DESC_DATA_ED_RO_A:	case DESC_DATA_ED_RW_NA:	case DESC_DATA_ED_RW_A:
		case DESC_CODE_N_NC_A:	case DESC_CODE_N_NC_NA:	case DESC_CODE_R_NC_A:	case DESC_CODE_R_NC_NA:
			if (cpu.cpl>desc.DPL()) needs_invalidation=true; break;
		default: break;	}
	if (needs_invalidation) CPU_SetSegGeneral(es,0);

	needs_invalidation=false;
	if (!cpu.gdt.GetDescriptor(SegValue(ds),desc)) needs_invalidation=true;
	else switch (desc.Type()) {
		case DESC_DATA_EU_RO_NA:	case DESC_DATA_EU_RO_A:	case DESC_DATA_EU_RW_NA:	case DESC_DATA_EU_RW_A:
		case DESC_DATA_ED_RO_NA:	case DESC_DATA_ED_RO_A:	case DESC_DATA_ED_RW_NA:	case DESC_DATA_ED_RW_A:
		case DESC_CODE_N_NC_A:	case DESC_CODE_N_NC_NA:	case DESC_CODE_R_NC_A:	case DESC_CODE_R_NC_NA:
			if (cpu.cpl>desc.DPL()) needs_invalidation=true; break;
		default: break;	}
	if (needs_invalidation) CPU_SetSegGeneral(ds,0);

	needs_invalidation=false;
	if (!cpu.gdt.GetDescriptor(SegValue(fs),desc)) needs_invalidation=true;
	else switch (desc.Type()) {
		case DESC_DATA_EU_RO_NA:	case DESC_DATA_EU_RO_A:	case DESC_DATA_EU_RW_NA:	case DESC_DATA_EU_RW_A:
		case DESC_DATA_ED_RO_NA:	case DESC_DATA_ED_RO_A:	case DESC_DATA_ED_RW_NA:	case DESC_DATA_ED_RW_A:
		case DESC_CODE_N_NC_A:	case DESC_CODE_N_NC_NA:	case DESC_CODE_R_NC_A:	case DESC_CODE_R_NC_NA:
			if (cpu.cpl>desc.DPL()) needs_invalidation=true; break;
		default: break;	}
	if (needs_invalidation) CPU_SetSegGeneral(fs,0);

	needs_invalidation=false;
	if (!cpu.gdt.GetDescriptor(SegValue(gs),desc)) needs_invalidation=true;
	else switch (desc.Type()) {
		case DESC_DATA_EU_RO_NA:	case DESC_DATA_EU_RO_A:	case DESC_DATA_EU_RW_NA:	case DESC_DATA_EU_RW_A:
		case DESC_DATA_ED_RO_NA:	case DESC_DATA_ED_RO_A:	case DESC_DATA_ED_RW_NA:	case DESC_DATA_ED_RW_A:
		case DESC_CODE_N_NC_A:	case DESC_CODE_N_NC_NA:	case DESC_CODE_R_NC_A:	case DESC_CODE_R_NC_NA:
			if (cpu.cpl>desc.DPL()) needs_invalidation=true; break;
		default: break;	}
	if (needs_invalidation) CPU_SetSegGeneral(gs,0);
}


class TaskStateSegment {
public:
	TaskStateSegment() {
		valid=false;
	}
	bool IsValid(void) {
		return valid;
	}
	Bitu Get_back(void) {
		cpu.mpl=0;
		Bit16u backlink=mem_readw(base);
		cpu.mpl=3;
		return backlink;
	}
	void SaveSelector(void) {
		cpu.gdt.SetDescriptor(selector,desc);
	}
	void Get_SSx_ESPx(Bitu level,Bitu & _ss,Bitu & _esp) {
		cpu.mpl=0;
		if (is386) {
			PhysPt where=base+offsetof(TSS_32,esp0)+level*8;
			_esp=mem_readd(where);
			_ss=mem_readw(where+4);
		} else {
			PhysPt where=base+offsetof(TSS_16,sp0)+level*4;
			_esp=mem_readw(where);
			_ss=mem_readw(where+2);
		}
		cpu.mpl=3;
	}
	bool SetSelector(Bitu new_sel) {
		valid=false;
		if ((new_sel & 0xfffc)==0) {
			selector=0;
			base=0;
			limit=0;
			is386=1;
			return true;
		}
		if (new_sel&4) return false;
		if (!cpu.gdt.GetDescriptor(new_sel,desc)) return false;
		switch (desc.Type()) {
			case DESC_286_TSS_A:		case DESC_286_TSS_B:
			case DESC_386_TSS_A:		case DESC_386_TSS_B:
				break;
			default:
				return false;
		}
		if (!desc.saved.seg.p) return false;
		selector=new_sel;
		valid=true;
		base=desc.GetBase();
		limit=desc.GetLimit();
		is386=desc.Is386();
		return true;
	}

	void SaveState( std::ostream& stream );
	void LoadState( std::istream& stream );

	TSS_Descriptor desc;
	Bitu selector;
	PhysPt base;
	Bitu limit;
	Bitu is386;
	bool valid;
};

TaskStateSegment cpu_tss;

enum TSwitchType {
	TSwitch_JMP,TSwitch_CALL_INT,TSwitch_IRET
};

bool CPU_SwitchTask(Bitu new_tss_selector,TSwitchType tstype,Bitu old_eip) {
	bool old_allow = dosbox_allow_nonrecursive_page_fault;

	/* this code isn't very easy to make interruptible. so temporarily revert to recursive PF handling method */
	dosbox_allow_nonrecursive_page_fault = false;

	FillFlags();
	TaskStateSegment new_tss;
	if (!new_tss.SetSelector(new_tss_selector)) 
		E_Exit("Illegal TSS for switch, selector=%x, switchtype=%lx",(int)new_tss_selector,(unsigned long)tstype);
	if (tstype==TSwitch_IRET) {
		if (!new_tss.desc.IsBusy())
			E_Exit("TSS not busy for IRET");
	} else {
		if (new_tss.desc.IsBusy())
			E_Exit("TSS busy for JMP/CALL/INT");
	}
	Bitu new_cr3=0;
	Bitu new_eax,new_ebx,new_ecx,new_edx,new_esp,new_ebp,new_esi,new_edi;
	Bitu new_es,new_cs,new_ss,new_ds,new_fs,new_gs;
	Bitu new_ldt,new_eip,new_eflags;
	/* Read new context from new TSS */
	if (new_tss.is386) {
		new_cr3=mem_readd(new_tss.base+offsetof(TSS_32,cr3));
		new_eip=mem_readd(new_tss.base+offsetof(TSS_32,eip));
		new_eflags=mem_readd(new_tss.base+offsetof(TSS_32,eflags));
		new_eax=mem_readd(new_tss.base+offsetof(TSS_32,eax));
		new_ecx=mem_readd(new_tss.base+offsetof(TSS_32,ecx));
		new_edx=mem_readd(new_tss.base+offsetof(TSS_32,edx));
		new_ebx=mem_readd(new_tss.base+offsetof(TSS_32,ebx));
		new_esp=mem_readd(new_tss.base+offsetof(TSS_32,esp));
		new_ebp=mem_readd(new_tss.base+offsetof(TSS_32,ebp));
		new_edi=mem_readd(new_tss.base+offsetof(TSS_32,edi));
		new_esi=mem_readd(new_tss.base+offsetof(TSS_32,esi));

		new_es=mem_readw(new_tss.base+offsetof(TSS_32,es));
		new_cs=mem_readw(new_tss.base+offsetof(TSS_32,cs));
		new_ss=mem_readw(new_tss.base+offsetof(TSS_32,ss));
		new_ds=mem_readw(new_tss.base+offsetof(TSS_32,ds));
		new_fs=mem_readw(new_tss.base+offsetof(TSS_32,fs));
		new_gs=mem_readw(new_tss.base+offsetof(TSS_32,gs));
		new_ldt=mem_readw(new_tss.base+offsetof(TSS_32,ldt));
	} else {
		E_Exit("286 task switch");
		new_cr3=0;
		new_eip=0;
		new_eflags=0;
		new_eax=0;	new_ecx=0;	new_edx=0;	new_ebx=0;
		new_esp=0;	new_ebp=0;	new_edi=0;	new_esi=0;

		new_es=0;	new_cs=0;	new_ss=0;	new_ds=0;	new_fs=0;	new_gs=0;
		new_ldt=0;
	}

	/* Check if we need to clear busy bit of old TASK */
	if (tstype==TSwitch_JMP || tstype==TSwitch_IRET) {
		cpu_tss.desc.SetBusy(false);
		cpu_tss.SaveSelector();
	}
	Bit32u old_flags = reg_flags;
	if (tstype==TSwitch_IRET) old_flags &= (~FLAG_NT);

	/* Save current context in current TSS */
	if (cpu_tss.is386) {
		mem_writed(cpu_tss.base+offsetof(TSS_32,eflags),old_flags);
		mem_writed(cpu_tss.base+offsetof(TSS_32,eip),old_eip);

		mem_writed(cpu_tss.base+offsetof(TSS_32,eax),reg_eax);
		mem_writed(cpu_tss.base+offsetof(TSS_32,ecx),reg_ecx);
		mem_writed(cpu_tss.base+offsetof(TSS_32,edx),reg_edx);
		mem_writed(cpu_tss.base+offsetof(TSS_32,ebx),reg_ebx);
		mem_writed(cpu_tss.base+offsetof(TSS_32,esp),reg_esp);
		mem_writed(cpu_tss.base+offsetof(TSS_32,ebp),reg_ebp);
		mem_writed(cpu_tss.base+offsetof(TSS_32,esi),reg_esi);
		mem_writed(cpu_tss.base+offsetof(TSS_32,edi),reg_edi);

		mem_writed(cpu_tss.base+offsetof(TSS_32,es),SegValue(es));
		mem_writed(cpu_tss.base+offsetof(TSS_32,cs),SegValue(cs));
		mem_writed(cpu_tss.base+offsetof(TSS_32,ss),SegValue(ss));
		mem_writed(cpu_tss.base+offsetof(TSS_32,ds),SegValue(ds));
		mem_writed(cpu_tss.base+offsetof(TSS_32,fs),SegValue(fs));
		mem_writed(cpu_tss.base+offsetof(TSS_32,gs),SegValue(gs));
	} else {
		E_Exit("286 task switch");
	}

	/* Setup a back link to the old TSS in new TSS */
	if (tstype==TSwitch_CALL_INT) {
		if (new_tss.is386) {
			mem_writed(new_tss.base+offsetof(TSS_32,back),cpu_tss.selector);
		} else {
			mem_writew(new_tss.base+offsetof(TSS_16,back),cpu_tss.selector);
		}
		/* And make the new task's eflag have the nested task bit */
		new_eflags|=FLAG_NT;
	}
	/* Set the busy bit in the new task */
	if (tstype==TSwitch_JMP || tstype==TSwitch_CALL_INT) {
		new_tss.desc.SetBusy(true);
		new_tss.SaveSelector();
	}

//	cpu.cr0|=CR0_TASKSWITCHED;
	if (new_tss_selector == cpu_tss.selector) {
		reg_eip = old_eip;
		new_cs = SegValue(cs);
		new_ss = SegValue(ss);
		new_ds = SegValue(ds);
		new_es = SegValue(es);
		new_fs = SegValue(fs);
		new_gs = SegValue(gs);
	} else {
	
		/* Setup the new cr3 */
		if (paging.cr3 != new_cr3)
			// if they are the same it is not flushed
			// according to the 386 manual
		PAGING_SetDirBase(new_cr3);

		/* Load new context */
		if (new_tss.is386) {
			reg_eip=new_eip;
			CPU_SetFlags(new_eflags,FMASK_ALL | FLAG_VM);
			reg_eax=new_eax;
			reg_ecx=new_ecx;
			reg_edx=new_edx;
			reg_ebx=new_ebx;
			reg_esp=new_esp;
			reg_ebp=new_ebp;
			reg_edi=new_edi;
			reg_esi=new_esi;

//			new_cs=mem_readw(new_tss.base+offsetof(TSS_32,cs));
		} else {
			E_Exit("286 task switch");
		}
	}
	/* Load the new selectors */
	if (reg_flags & FLAG_VM) {
		SegSet16(cs,new_cs);
		cpu.code.big=false;
		CPU_SetCPL(3);			//We don't have segment caches so this will do
	} else {
		/* Protected mode task */
		if (new_ldt!=0) CPU_LLDT(new_ldt);
		/* Load the new CS*/
		Descriptor cs_desc;
		CPU_SetCPL(new_cs & 3);
		if (!cpu.gdt.GetDescriptor(new_cs,cs_desc))
			E_Exit("Task switch with CS beyond limits");
		if (!cs_desc.saved.seg.p)
			E_Exit("Task switch with non present code-segment");
		switch (cs_desc.Type()) {
		case DESC_CODE_N_NC_A:		case DESC_CODE_N_NC_NA:
		case DESC_CODE_R_NC_A:		case DESC_CODE_R_NC_NA:
			if (cpu.cpl != cs_desc.DPL()) E_Exit("Task CS RPL != DPL");
			goto doconforming;
		case DESC_CODE_N_C_A:		case DESC_CODE_N_C_NA:
		case DESC_CODE_R_C_A:		case DESC_CODE_R_C_NA:
			if (cpu.cpl < cs_desc.DPL()) E_Exit("Task CS RPL < DPL");
doconforming:
			Segs.expanddown[cs]=cs_desc.GetExpandDown();
			Segs.limit[cs]=do_seg_limits?cs_desc.GetLimit():(~0UL);
			Segs.phys[cs]=cs_desc.GetBase();
			cpu.code.big=cs_desc.Big()>0;
			Segs.val[cs]=new_cs;
			break;
		default:
			E_Exit("Task switch CS Type %d",(int)cs_desc.Type());
		}
	}
	CPU_SetSegGeneral(es,new_es);
	CPU_SetSegGeneral(ss,new_ss);
	CPU_SetSegGeneral(ds,new_ds);
	CPU_SetSegGeneral(fs,new_fs);
	CPU_SetSegGeneral(gs,new_gs);
	if (!cpu_tss.SetSelector(new_tss_selector)) {
		LOG(LOG_CPU,LOG_NORMAL)("TaskSwitch: set tss selector %X failed",new_tss_selector);
	}
//	cpu_tss.desc.SetBusy(true);
//	cpu_tss.SaveSelector();
//	LOG_MSG("Task CPL %X CS:%X IP:%X SS:%X SP:%X eflags %x",cpu.cpl,SegValue(cs),reg_eip,SegValue(ss),reg_esp,reg_flags);

	dosbox_allow_nonrecursive_page_fault = old_allow;
	return true;
}

bool CPU_IO_Exception(Bitu port,Bitu size) {
	if (cpu.pmode && ((GETFLAG_IOPL<cpu.cpl) || GETFLAG(VM))) {
		cpu.mpl=0;
		if (!cpu_tss.is386) goto doexception;
		PhysPt bwhere=cpu_tss.base+0x66;
		Bitu ofs=mem_readw(bwhere);
		if (ofs>cpu_tss.limit) goto doexception;
		bwhere=cpu_tss.base+ofs+(port/8);
		Bitu map=mem_readw(bwhere);
		Bitu mask=(0xffff>>(16-size)) << (port&7);
		if (map & mask) goto doexception;
		cpu.mpl=3;
	}
	return false;
doexception:
	cpu.mpl=3;
	LOG(LOG_CPU,LOG_NORMAL)("IO Exception port %X",port);
	return CPU_PrepareException(EXCEPTION_GP,0);
}

#include <stack>

int CPU_Exception_Level[0x20] = {0};
std::stack<int> CPU_Exception_In_Progress;

void CPU_Exception_Level_Reset() {
	int i;

	for (i=0;i < 0x20;i++)
		CPU_Exception_Level[i] = 0;
	while (!CPU_Exception_In_Progress.empty())
		CPU_Exception_In_Progress.pop();
}

bool has_printed_double_fault = false;
bool has_printed_triple_fault = false;
bool always_report_double_fault = false;
bool always_report_triple_fault = false;

void On_Software_CPU_Reset();

void CPU_Exception(Bitu which,Bitu error ) {
	assert(which < 0x20);
//	LOG_MSG("Exception %d error %x",which,error);
	if (CPU_Exception_Level[which] != 0) {
		if (CPU_Exception_Level[EXCEPTION_DF] != 0 && cpu_triple_fault_reset) {
			if (always_report_triple_fault || !has_printed_triple_fault) {
				LOG_MSG("CPU_Exception: Double fault already in progress == Triple Fault. Resetting CPU.");
				has_printed_triple_fault = true;
			}

			// Triple fault -> special shutdown cycle -> reset signal -> reset.
			// Sickening, I know, but that's how IBM wired things a long long time ago.
			On_Software_CPU_Reset();
			E_Exit("Triple fault reset call unexpectedly returned");
		}

		if (always_report_double_fault || !has_printed_double_fault) {
			LOG_MSG("CPU_Exception: Exception %d already in progress, triggering double fault instead",(int)which);
			has_printed_double_fault = true;
		}
		which = EXCEPTION_DF;
		error = 0;
	}

	if (cpu_double_fault_enable) {
		/* CPU_Interrupt() could cause another fault during memory access. This needs to happen here */
		CPU_Exception_Level[which]++;
		CPU_Exception_In_Progress.push(which);
	}

	cpu.exception.error=error;
	CPU_Interrupt(which,CPU_INT_EXCEPTION | ((which>=8) ? CPU_INT_HAS_ERROR : 0),reg_eip);

	/* allow recursive page faults. required for multitasking OSes like Windows 95.
	 * we set this AFTER CPU_Interrupt so that if CPU_Interrupt faults while starting
	 * a page fault we still trigger double fault. */
	if (which == EXCEPTION_PF || which == EXCEPTION_GP) {
		if (CPU_Exception_Level[which] > 0)
			CPU_Exception_Level[which]--;

		if (!CPU_Exception_In_Progress.empty()) {
			if ((Bitu)CPU_Exception_In_Progress.top() == which)
				CPU_Exception_In_Progress.pop();
			else
				LOG_MSG("Top of fault stack not the same as what I'm handling");
		}
	}
}

Bit8u lastint;
void CPU_Interrupt(Bitu num,Bitu type,Bitu oldeip) {
	lastint=num;
	FillFlags();
#if C_DEBUG
	switch (num) {
	case 0xcd:
#if C_HEAVY_DEBUG
 		LOG(LOG_CPU,LOG_ERROR)("Call to interrupt 0xCD this is BAD");
		DEBUG_HeavyWriteLogInstruction();
		E_Exit("Call to interrupt 0xCD this is BAD");
#endif
		break;
	case 0x03:
		if (DEBUG_Breakpoint()) {
			CPU_Cycles=0;
			return;
		}
	};
#endif
	if (!cpu.pmode) {
		/* Save everything on a 16-bit stack */
		CPU_Push16(reg_flags & 0xffff);
		CPU_Push16(SegValue(cs));
		CPU_Push16(oldeip);
		SETFLAGBIT(IF,false);
		SETFLAGBIT(TF,false);
		/* Get the new CS:IP from vector table */
		PhysPt base=cpu.idt.GetBase();
		reg_eip=mem_readw(base+(num << 2));
		Segs.val[cs]=mem_readw(base+(num << 2)+2);
		Segs.phys[cs]=Segs.val[cs]<<4;
		if (!cpu_allow_big16) cpu.code.big=false;
		return;
	} else {
		/* Protected Mode Interrupt */
		if ((reg_flags & FLAG_VM) && (type&CPU_INT_SOFTWARE) && !(type&CPU_INT_NOIOPLCHECK)) {
//			LOG_MSG("Software int in v86, AH %X IOPL %x",reg_ah,(reg_flags & FLAG_IOPL) >>12);
			if ((reg_flags & FLAG_IOPL)!=FLAG_IOPL) {
				CPU_Exception(EXCEPTION_GP,0);
				return;
			}
		} 

		Descriptor gate;
		if (!cpu.idt.GetDescriptor(num<<3,gate)) {
			// zone66
			CPU_Exception(EXCEPTION_GP,num*8+2+(type&CPU_INT_SOFTWARE)?0:1);
			return;
		}

		if ((type&CPU_INT_SOFTWARE) && (gate.DPL()<cpu.cpl)) {
			// zone66, win3.x e
			CPU_Exception(EXCEPTION_GP,num*8+2);
			return;
		}

		Bitu old_esp,old_ss,old_cpl;

		old_esp = reg_esp;
		old_ss = SegValue(ss);
		old_cpl = cpu.cpl;

		try {
		switch (gate.Type()) {
		case DESC_286_INT_GATE:		case DESC_386_INT_GATE:
		case DESC_286_TRAP_GATE:	case DESC_386_TRAP_GATE:
			{
				CPU_CHECK_COND(!gate.saved.seg.p,
					"INT:Gate segment not present",
					EXCEPTION_NP,num*8+2+(type&CPU_INT_SOFTWARE)?0:1)

				Descriptor cs_desc;
				Bitu gate_sel=gate.GetSelector();
				Bitu gate_off=gate.GetOffset();
				CPU_CHECK_COND((gate_sel & 0xfffc)==0,
					"INT:Gate with CS zero selector",
					EXCEPTION_GP,(type&CPU_INT_SOFTWARE)?0:1)
				CPU_CHECK_COND(!cpu.gdt.GetDescriptor(gate_sel,cs_desc),
					"INT:Gate with CS beyond limit",
					EXCEPTION_GP,(gate_sel & 0xfffc)+(type&CPU_INT_SOFTWARE)?0:1)

				Bitu cs_dpl=cs_desc.DPL();
				CPU_CHECK_COND(cs_dpl>cpu.cpl,
					"Interrupt to higher privilege",
					EXCEPTION_GP,(gate_sel & 0xfffc)+(type&CPU_INT_SOFTWARE)?0:1)
				switch (cs_desc.Type()) {
				case DESC_CODE_N_NC_A:	case DESC_CODE_N_NC_NA:
				case DESC_CODE_R_NC_A:	case DESC_CODE_R_NC_NA:
					if (cs_dpl<cpu.cpl) {
						/* Prepare for gate to inner level */
						CPU_CHECK_COND(!cs_desc.saved.seg.p,
							"INT:Inner level:CS segment not present",
							EXCEPTION_NP,(gate_sel & 0xfffc)+(type&CPU_INT_SOFTWARE)?0:1)
						CPU_CHECK_COND((reg_flags & FLAG_VM) && (cs_dpl!=0),
							"V86 interrupt calling codesegment with DPL>0",
							EXCEPTION_GP,gate_sel & 0xfffc)

						Bitu n_ss,n_esp;
						Bitu o_ss,o_esp;
						o_ss=SegValue(ss);
						o_esp=reg_esp;
						cpu_tss.Get_SSx_ESPx(cs_dpl,n_ss,n_esp);
						CPU_CHECK_COND((n_ss & 0xfffc)==0,
							"INT:Gate with SS zero selector",
							EXCEPTION_TS,(type&CPU_INT_SOFTWARE)?0:1)
						Descriptor n_ss_desc;
						CPU_CHECK_COND(!cpu.gdt.GetDescriptor(n_ss,n_ss_desc),
							"INT:Gate with SS beyond limit",
							EXCEPTION_TS,(n_ss & 0xfffc)+(type&CPU_INT_SOFTWARE)?0:1)
						CPU_CHECK_COND(((n_ss & 3)!=cs_dpl) || (n_ss_desc.DPL()!=cs_dpl),
							"INT:Inner level with CS_DPL!=SS_DPL and SS_RPL",
							EXCEPTION_TS,(n_ss & 0xfffc)+(type&CPU_INT_SOFTWARE)?0:1)

						// check if stack segment is a writable data segment
						switch (n_ss_desc.Type()) {
						case DESC_DATA_EU_RW_NA:		case DESC_DATA_EU_RW_A:
						case DESC_DATA_ED_RW_NA:		case DESC_DATA_ED_RW_A:
							break;
						default:
							E_Exit("INT:Inner level:Stack segment not writable.");		// or #TS(ss_sel+EXT)
						}
						CPU_CHECK_COND(!n_ss_desc.saved.seg.p,
							"INT:Inner level with nonpresent SS",
							EXCEPTION_SS,(n_ss & 0xfffc)+(type&CPU_INT_SOFTWARE)?0:1)

						// commit point
						Segs.expanddown[ss]=n_ss_desc.GetExpandDown();
						Segs.limit[ss]=do_seg_limits?n_ss_desc.GetLimit():(~0UL);
						Segs.phys[ss]=n_ss_desc.GetBase();
						Segs.val[ss]=n_ss;
						if (n_ss_desc.Big()) {
							cpu.stack.big=true;
							cpu.stack.mask=0xffffffff;
							cpu.stack.notmask=0;
							reg_esp=n_esp;
						} else {
							cpu.stack.big=false;
							cpu.stack.mask=0xffff;
							cpu.stack.notmask=0xffff0000;
							reg_sp=n_esp & 0xffff;
						}

						CPU_SetCPL(cs_dpl);
						if (gate.Type() & 0x8) {	/* 32-bit Gate */
							if (reg_flags & FLAG_VM) {
								CPU_Push32(SegValue(gs));SegSet16(gs,0x0);
								CPU_Push32(SegValue(fs));SegSet16(fs,0x0);
								CPU_Push32(SegValue(ds));SegSet16(ds,0x0);
								CPU_Push32(SegValue(es));SegSet16(es,0x0);
							}
							CPU_Push32(o_ss);
							CPU_Push32(o_esp);
						} else {					/* 16-bit Gate */
							if (reg_flags & FLAG_VM) E_Exit("V86 to 16-bit gate");
							CPU_Push16(o_ss);
							CPU_Push16(o_esp);
						}
//						LOG_MSG("INT:Gate to inner level SS:%X SP:%X",n_ss,n_esp);
						goto do_interrupt;
					} 
					if (cs_dpl!=cpu.cpl)
						E_Exit("Non-conforming intra privilege INT with DPL!=CPL");
				case DESC_CODE_N_C_A:	case DESC_CODE_N_C_NA:
				case DESC_CODE_R_C_A:	case DESC_CODE_R_C_NA:
					/* Prepare stack for gate to same priviledge */
					CPU_CHECK_COND(!cs_desc.saved.seg.p,
							"INT:Same level:CS segment not present",
						EXCEPTION_NP,(gate_sel & 0xfffc)+(type&CPU_INT_SOFTWARE)?0:1)
					if ((reg_flags & FLAG_VM) && (cs_dpl<cpu.cpl))
						E_Exit("V86 interrupt doesn't change to pl0");	// or #GP(cs_sel)

					// commit point
do_interrupt:
					if (gate.Type() & 0x8) {	/* 32-bit Gate */
						CPU_Push32(reg_flags);
						CPU_Push32(SegValue(cs));
						CPU_Push32(oldeip);
						if (type & CPU_INT_HAS_ERROR) CPU_Push32(cpu.exception.error);
					} else {					/* 16-bit gate */
						CPU_Push16(reg_flags & 0xffff);
						CPU_Push16(SegValue(cs));
						CPU_Push16(oldeip);
						if (type & CPU_INT_HAS_ERROR) CPU_Push16(cpu.exception.error);
					}
					break;		
				default:
					E_Exit("INT:Gate Selector points to illegal descriptor with type %x",(int)cs_desc.Type());
				}

				Segs.val[cs]=(gate_sel&0xfffc) | cpu.cpl;
				Segs.phys[cs]=cs_desc.GetBase();
				Segs.limit[cs]=do_seg_limits?cs_desc.GetLimit():(~0UL);
				Segs.expanddown[cs]=cs_desc.GetExpandDown();
				cpu.code.big=cs_desc.Big()>0;
				reg_eip=gate_off;

				if (!(gate.Type()&1)) {
					SETFLAGBIT(IF,false);
				}
				SETFLAGBIT(TF,false);
				SETFLAGBIT(NT,false);
				SETFLAGBIT(VM,false);
				LOG(LOG_CPU,LOG_NORMAL)("INT:Gate to %X:%X big %d %s",gate_sel,gate_off,cs_desc.Big(),gate.Type() & 0x8 ? "386" : "286");
				return;
			}
		case DESC_TASK_GATE:
			CPU_CHECK_COND(!gate.saved.seg.p,
				"INT:Gate segment not present",
				EXCEPTION_NP,num*8+2+(type&CPU_INT_SOFTWARE)?0:1)

			CPU_SwitchTask(gate.GetSelector(),TSwitch_CALL_INT,oldeip);
			if (type & CPU_INT_HAS_ERROR) {
				//TODO Be sure about this, seems somewhat unclear
				if (cpu_tss.is386) CPU_Push32(cpu.exception.error);
				else CPU_Push16(cpu.exception.error);
			}
			return;
		default:
			E_Exit("Illegal descriptor type %X for int %X",(int)gate.Type(),(int)num);
		}
		}
		catch (GuestPageFaultException &pf) {
			LOG_MSG("CPU_Interrupt() interrupted");
			CPU_SetSegGeneral(ss,old_ss);
			reg_esp = old_esp;
			CPU_SetCPL(old_cpl);
			throw;
		}
	}
	assert(1);
	return ; // make compiler happy
}


void CPU_IRET(bool use32,Bitu oldeip) {
	Bitu orig_esp = reg_esp;

	/* x86 CPUs consider IRET the completion of an NMI, no matter where it happens */
	/* FIXME: If the IRET causes an exception, is it still considered the end of the NMI? */
	CPU_NMI_active = false;

	/* Fault emulation */
	if (!CPU_Exception_In_Progress.empty()) {
		int which = CPU_Exception_In_Progress.top();
		CPU_Exception_In_Progress.pop();
		assert(which < 0x20);

		if (CPU_Exception_Level[which] > 0)
			CPU_Exception_Level[which]--;

//		LOG_MSG("Leaving CPU exception %d",which);
	}

	if (!cpu.pmode) {					/* RealMode IRET */
		if (use32) {
			reg_eip=CPU_Pop32();
			SegSet16(cs,CPU_Pop32());
			CPU_SetFlags(CPU_Pop32(),FMASK_ALL);
		} else {
			reg_eip=CPU_Pop16();
			SegSet16(cs,CPU_Pop16());
			CPU_SetFlags(CPU_Pop16(),FMASK_ALL & 0xffff);
		}
		if (!cpu_allow_big16) cpu.code.big=false;
		DestroyConditionFlags();
		return;
	} else {	/* Protected mode IRET */
		if (reg_flags & FLAG_VM) {
			if ((reg_flags & FLAG_IOPL)!=FLAG_IOPL) {
				// win3.x e
				CPU_Exception(EXCEPTION_GP,0);
				return;
			} else {
				try {
				if (use32) {
					Bit32u new_eip=mem_readd(SegPhys(ss) + (reg_esp & cpu.stack.mask));
					Bit32u tempesp=(reg_esp&cpu.stack.notmask)|((reg_esp+4)&cpu.stack.mask);
					Bit32u new_cs=mem_readd(SegPhys(ss) + (tempesp & cpu.stack.mask));
					tempesp=(tempesp&cpu.stack.notmask)|((tempesp+4)&cpu.stack.mask);
					Bit32u new_flags=mem_readd(SegPhys(ss) + (tempesp & cpu.stack.mask));
					reg_esp=(tempesp&cpu.stack.notmask)|((tempesp+4)&cpu.stack.mask);

					reg_eip=new_eip;
					SegSet16(cs,(Bit16u)(new_cs&0xffff));
					/* IOPL can not be modified in v86 mode by IRET */
					CPU_SetFlags(new_flags,FMASK_NORMAL|FLAG_NT);
				} else {
					Bit16u new_eip=mem_readw(SegPhys(ss) + (reg_esp & cpu.stack.mask));
					Bit32u tempesp=(reg_esp&cpu.stack.notmask)|((reg_esp+2)&cpu.stack.mask);
					Bit16u new_cs=mem_readw(SegPhys(ss) + (tempesp & cpu.stack.mask));
					tempesp=(tempesp&cpu.stack.notmask)|((tempesp+2)&cpu.stack.mask);
					Bit16u new_flags=mem_readw(SegPhys(ss) + (tempesp & cpu.stack.mask));
					reg_esp=(tempesp&cpu.stack.notmask)|((tempesp+2)&cpu.stack.mask);

					reg_eip=(Bit32u)new_eip;
					SegSet16(cs,new_cs);
					/* IOPL can not be modified in v86 mode by IRET */
					CPU_SetFlags(new_flags,FMASK_NORMAL|FLAG_NT);
				}
				}
				catch (GuestPageFaultException &pf) {
					LOG_MSG("CPU_IRET() interrupted prot vm86");
					reg_esp = orig_esp;
					throw;
				}
				cpu.code.big=false;
				DestroyConditionFlags();
				return;
			}
		}
		/* Check if this is task IRET */	
		if (GETFLAG(NT)) {
			if (GETFLAG(VM)) E_Exit("Pmode IRET with VM bit set");
			CPU_CHECK_COND(!cpu_tss.IsValid(),
				"TASK Iret without valid TSS",
				EXCEPTION_TS,cpu_tss.selector & 0xfffc)
			if (!cpu_tss.desc.IsBusy()) {
				LOG(LOG_CPU,LOG_ERROR)("TASK Iret:TSS not busy");
			}
			Bitu back_link=cpu_tss.Get_back();
			CPU_SwitchTask(back_link,TSwitch_IRET,oldeip);
			return;
		}
		Bitu n_cs_sel,n_eip,n_flags;
		Bit32u tempesp;
		if (use32) {
			n_eip=mem_readd(SegPhys(ss) + (reg_esp & cpu.stack.mask));
			tempesp=(reg_esp&cpu.stack.notmask)|((reg_esp+4)&cpu.stack.mask);
			n_cs_sel=mem_readd(SegPhys(ss) + (tempesp & cpu.stack.mask)) & 0xffff;
			tempesp=(tempesp&cpu.stack.notmask)|((tempesp+4)&cpu.stack.mask);
			n_flags=mem_readd(SegPhys(ss) + (tempesp & cpu.stack.mask));
			tempesp=(tempesp&cpu.stack.notmask)|((tempesp+4)&cpu.stack.mask);

			if ((n_flags & FLAG_VM) && (cpu.cpl==0)) {
				// commit point
				try {
				reg_esp=tempesp;
				reg_eip=n_eip & 0xffff;
				Bitu n_ss,n_esp,n_es,n_ds,n_fs,n_gs;
				n_esp=CPU_Pop32();
				n_ss=CPU_Pop32() & 0xffff;
				n_es=CPU_Pop32() & 0xffff;
				n_ds=CPU_Pop32() & 0xffff;
				n_fs=CPU_Pop32() & 0xffff;
				n_gs=CPU_Pop32() & 0xffff;

				CPU_SetFlags(n_flags,FMASK_ALL | FLAG_VM);
				DestroyConditionFlags();
				CPU_SetCPL(3);

				CPU_SetSegGeneral(ss,n_ss);
				CPU_SetSegGeneral(es,n_es);
				CPU_SetSegGeneral(ds,n_ds);
				CPU_SetSegGeneral(fs,n_fs);
				CPU_SetSegGeneral(gs,n_gs);
				reg_esp=n_esp;
				cpu.code.big=false;
				SegSet16(cs,n_cs_sel);
				LOG(LOG_CPU,LOG_NORMAL)("IRET:Back to V86: CS:%X IP %X SS:%X SP %X FLAGS:%X",SegValue(cs),reg_eip,SegValue(ss),reg_esp,reg_flags);	
				return;
				}
				catch (GuestPageFaultException &pf) {
					LOG_MSG("CPU_IRET() interrupted prot use32");
					reg_esp = orig_esp;
					throw;
				}
			}
			if (n_flags & FLAG_VM) E_Exit("IRET from pmode to v86 with CPL!=0");
		} else {
			n_eip=mem_readw(SegPhys(ss) + (reg_esp & cpu.stack.mask));
			tempesp=(reg_esp&cpu.stack.notmask)|((reg_esp+2)&cpu.stack.mask);
			n_cs_sel=mem_readw(SegPhys(ss) + (tempesp & cpu.stack.mask));
			tempesp=(tempesp&cpu.stack.notmask)|((tempesp+2)&cpu.stack.mask);
			n_flags=mem_readw(SegPhys(ss) + (tempesp & cpu.stack.mask));
			n_flags|=(reg_flags & 0xffff0000);
			tempesp=(tempesp&cpu.stack.notmask)|((tempesp+2)&cpu.stack.mask);

			if (n_flags & FLAG_VM) E_Exit("VM Flag in 16-bit iret");
		}
		CPU_CHECK_COND((n_cs_sel & 0xfffc)==0,
			"IRET:CS selector zero",
			EXCEPTION_GP,0)
		Bitu n_cs_rpl=n_cs_sel & 3;
		Descriptor n_cs_desc;
		CPU_CHECK_COND(!cpu.gdt.GetDescriptor(n_cs_sel,n_cs_desc),
			"IRET:CS selector beyond limits",
			EXCEPTION_GP,n_cs_sel & 0xfffc)
		CPU_CHECK_COND(n_cs_rpl<cpu.cpl,
			"IRET to lower privilege",
			EXCEPTION_GP,n_cs_sel & 0xfffc)

		switch (n_cs_desc.Type()) {
		case DESC_CODE_N_NC_A:	case DESC_CODE_N_NC_NA:
		case DESC_CODE_R_NC_A:	case DESC_CODE_R_NC_NA:
			CPU_CHECK_COND(n_cs_rpl!=n_cs_desc.DPL(),
				"IRET:NC:DPL!=RPL",
				EXCEPTION_GP,n_cs_sel & 0xfffc)
			break;
		case DESC_CODE_N_C_A:	case DESC_CODE_N_C_NA:
		case DESC_CODE_R_C_A:	case DESC_CODE_R_C_NA:
			CPU_CHECK_COND(n_cs_desc.DPL()>n_cs_rpl,
				"IRET:C:DPL>RPL",
				EXCEPTION_GP,n_cs_sel & 0xfffc)
			break;
		default:
			E_Exit("IRET:Illegal descriptor type %X",(int)n_cs_desc.Type());
		}
		CPU_CHECK_COND(!n_cs_desc.saved.seg.p,
			"IRET with nonpresent code segment",
			EXCEPTION_NP,n_cs_sel & 0xfffc)

		if (n_cs_rpl==cpu.cpl) {	
			/* Return to same level */

			// commit point
			reg_esp=tempesp;
			Segs.expanddown[cs]=n_cs_desc.GetExpandDown();
			Segs.limit[cs]=do_seg_limits?n_cs_desc.GetLimit():(~0UL);
			Segs.phys[cs]=n_cs_desc.GetBase();
			cpu.code.big=n_cs_desc.Big()>0;
			Segs.val[cs]=n_cs_sel;
			reg_eip=n_eip;

			Bitu mask=cpu.cpl ? (FMASK_NORMAL | FLAG_NT) : FMASK_ALL;
			if (GETFLAG_IOPL<cpu.cpl) mask &= (~FLAG_IF);
			CPU_SetFlags(n_flags,mask);
			DestroyConditionFlags();
			LOG(LOG_CPU,LOG_NORMAL)("IRET:Same level:%X:%X big %d",n_cs_sel,n_eip,cpu.code.big);
		} else {
			/* Return to outer level */
			Bitu n_ss,n_esp;
			if (use32) {
				n_esp=mem_readd(SegPhys(ss) + (tempesp & cpu.stack.mask));
				tempesp=(tempesp&cpu.stack.notmask)|((tempesp+4)&cpu.stack.mask);
				n_ss=mem_readd(SegPhys(ss) + (tempesp & cpu.stack.mask)) & 0xffff;
			} else {
				n_esp=mem_readw(SegPhys(ss) + (tempesp & cpu.stack.mask));
				tempesp=(tempesp&cpu.stack.notmask)|((tempesp+2)&cpu.stack.mask);
				n_ss=mem_readw(SegPhys(ss) + (tempesp & cpu.stack.mask));
			}
			CPU_CHECK_COND((n_ss & 0xfffc)==0,
				"IRET:Outer level:SS selector zero",
				EXCEPTION_GP,0)
			CPU_CHECK_COND((n_ss & 3)!=n_cs_rpl,
				"IRET:Outer level:SS rpl!=CS rpl",
				EXCEPTION_GP,n_ss & 0xfffc)
			Descriptor n_ss_desc;
			CPU_CHECK_COND(!cpu.gdt.GetDescriptor(n_ss,n_ss_desc),
				"IRET:Outer level:SS beyond limit",
				EXCEPTION_GP,n_ss & 0xfffc)
			CPU_CHECK_COND(n_ss_desc.DPL()!=n_cs_rpl,
				"IRET:Outer level:SS dpl!=CS rpl",
				EXCEPTION_GP,n_ss & 0xfffc)

			// check if stack segment is a writable data segment
			switch (n_ss_desc.Type()) {
			case DESC_DATA_EU_RW_NA:		case DESC_DATA_EU_RW_A:
			case DESC_DATA_ED_RW_NA:		case DESC_DATA_ED_RW_A:
				break;
			default:
				E_Exit("IRET:Outer level:Stack segment not writable");		// or #GP(ss_sel)
			}
			CPU_CHECK_COND(!n_ss_desc.saved.seg.p,
				"IRET:Outer level:Stack segment not present",
				EXCEPTION_NP,n_ss & 0xfffc)

			// commit point

			Segs.expanddown[cs]=n_cs_desc.GetExpandDown();
			Segs.limit[cs]=do_seg_limits?n_cs_desc.GetLimit():(~0UL);
			Segs.phys[cs]=n_cs_desc.GetBase();
			cpu.code.big=n_cs_desc.Big()>0;
			Segs.val[cs]=n_cs_sel;

			Bitu mask=cpu.cpl ? (FMASK_NORMAL | FLAG_NT) : FMASK_ALL;
			if (GETFLAG_IOPL<cpu.cpl) mask &= (~FLAG_IF);
			CPU_SetFlags(n_flags,mask);
			DestroyConditionFlags();

			CPU_SetCPL(n_cs_rpl);
			reg_eip=n_eip;

			Segs.val[ss]=n_ss;
			Segs.phys[ss]=n_ss_desc.GetBase();
			Segs.limit[ss]=do_seg_limits?n_ss_desc.GetLimit():(~0UL);
			Segs.expanddown[ss]=n_ss_desc.GetExpandDown();
			if (n_ss_desc.Big()) {
				cpu.stack.big=true;
				cpu.stack.mask=0xffffffff;
				cpu.stack.notmask=0;
				reg_esp=n_esp;
			} else {
				cpu.stack.big=false;
				cpu.stack.mask=0xffff;
				cpu.stack.notmask=0xffff0000;
				reg_sp=n_esp & 0xffff;
			}

			// borland extender, zrdx
			CPU_CheckSegments();

			LOG(LOG_CPU,LOG_NORMAL)("IRET:Outer level:%X:%X big %d",n_cs_sel,n_eip,cpu.code.big);
		}
		return;
	}
}


void CPU_JMP(bool use32,Bitu selector,Bitu offset,Bitu oldeip) {
	if (!cpu.pmode || (reg_flags & FLAG_VM)) {
		if (!use32) {
			reg_eip=offset&0xffff;
		} else {
			reg_eip=offset;
		}
		SegSet16(cs,selector);
		if (!cpu_allow_big16) cpu.code.big=false;
		return;
	} else {
		CPU_CHECK_COND((selector & 0xfffc)==0,
			"JMP:CS selector zero",
			EXCEPTION_GP,0)
		Bitu rpl=selector & 3;
		Descriptor desc;
		CPU_CHECK_COND(!cpu.gdt.GetDescriptor(selector,desc),
			"JMP:CS beyond limits",
			EXCEPTION_GP,selector & 0xfffc)
		switch (desc.Type()) {
		case DESC_CODE_N_NC_A:		case DESC_CODE_N_NC_NA:
		case DESC_CODE_R_NC_A:		case DESC_CODE_R_NC_NA:
			CPU_CHECK_COND(rpl>cpu.cpl,
				"JMP:NC:RPL>CPL",
				EXCEPTION_GP,selector & 0xfffc)
			CPU_CHECK_COND(cpu.cpl!=desc.DPL(),
				"JMP:NC:RPL != DPL",
				EXCEPTION_GP,selector & 0xfffc)
			LOG(LOG_CPU,LOG_NORMAL)("JMP:Code:NC to %X:%X big %d",selector,offset,desc.Big());
			goto CODE_jmp;
		case DESC_CODE_N_C_A:		case DESC_CODE_N_C_NA:
		case DESC_CODE_R_C_A:		case DESC_CODE_R_C_NA:
			LOG(LOG_CPU,LOG_NORMAL)("JMP:Code:C to %X:%X big %d",selector,offset,desc.Big());
			CPU_CHECK_COND(cpu.cpl<desc.DPL(),
				"JMP:C:CPL < DPL",
				EXCEPTION_GP,selector & 0xfffc)
CODE_jmp:
			if (!desc.saved.seg.p) {
				// win
				CPU_Exception(EXCEPTION_NP,selector & 0xfffc);
				return;
			}

			/* Normal jump to another selector:offset */
			Segs.expanddown[cs]=desc.GetExpandDown();
			Segs.limit[cs]=do_seg_limits?desc.GetLimit():(~0UL);
			Segs.phys[cs]=desc.GetBase();
			cpu.code.big=desc.Big()>0;
			Segs.val[cs]=(selector & 0xfffc) | cpu.cpl;
			reg_eip=offset;
			return;
		case DESC_386_TSS_A:
			CPU_CHECK_COND(desc.DPL()<cpu.cpl,
				"JMP:TSS:dpl<cpl",
				EXCEPTION_GP,selector & 0xfffc)
			CPU_CHECK_COND(desc.DPL()<rpl,
				"JMP:TSS:dpl<rpl",
				EXCEPTION_GP,selector & 0xfffc)
			LOG(LOG_CPU,LOG_NORMAL)("JMP:TSS to %X",selector);
			CPU_SwitchTask(selector,TSwitch_JMP,oldeip);
			break;
		default:
			E_Exit("JMP Illegal descriptor type %X",(int)desc.Type());
		}
	}
	assert(1);
}


void CPU_CALL(bool use32,Bitu selector,Bitu offset,Bitu oldeip) {
	Bit32u old_esp = reg_esp;
	Bit32u old_eip = reg_eip;

	if (!cpu.pmode || (reg_flags & FLAG_VM)) {
		try {
		if (!use32) {
			CPU_Push16(SegValue(cs));
			CPU_Push16(oldeip);
			reg_eip=offset&0xffff;
		} else {
			CPU_Push32(SegValue(cs));
			CPU_Push32(oldeip);
			reg_eip=offset;
		}
		}
		catch (GuestPageFaultException &pf) {
			reg_esp = old_esp;
			reg_eip = old_eip;
			throw;
		}
		if (!cpu_allow_big16) cpu.code.big=false;
		SegSet16(cs,selector);
		return;
	} else {
		CPU_CHECK_COND((selector & 0xfffc)==0,
			"CALL:CS selector zero",
			EXCEPTION_GP,0)
		Bitu rpl=selector & 3;
		Descriptor call;
		CPU_CHECK_COND(!cpu.gdt.GetDescriptor(selector,call),
			"CALL:CS beyond limits",
			EXCEPTION_GP,selector & 0xfffc)
		/* Check for type of far call */
		switch (call.Type()) {
		case DESC_CODE_N_NC_A:case DESC_CODE_N_NC_NA:
		case DESC_CODE_R_NC_A:case DESC_CODE_R_NC_NA:
			CPU_CHECK_COND(rpl>cpu.cpl,
				"CALL:CODE:NC:RPL>CPL",
				EXCEPTION_GP,selector & 0xfffc)
			CPU_CHECK_COND(call.DPL()!=cpu.cpl,
				"CALL:CODE:NC:DPL!=CPL",
				EXCEPTION_GP,selector & 0xfffc)
			LOG(LOG_CPU,LOG_NORMAL)("CALL:CODE:NC to %X:%X",selector,offset);
			goto call_code;	
		case DESC_CODE_N_C_A:case DESC_CODE_N_C_NA:
		case DESC_CODE_R_C_A:case DESC_CODE_R_C_NA:
			CPU_CHECK_COND(call.DPL()>cpu.cpl,
				"CALL:CODE:C:DPL>CPL",
				EXCEPTION_GP,selector & 0xfffc)
			LOG(LOG_CPU,LOG_NORMAL)("CALL:CODE:C to %X:%X",selector,offset);
call_code:
			if (!call.saved.seg.p) {
				// borland extender (RTM)
				CPU_Exception(EXCEPTION_NP,selector & 0xfffc);
				return;
			}
			// commit point
			try {
			if (!use32) {
				CPU_Push16(SegValue(cs));
				CPU_Push16(oldeip);
				reg_eip=offset & 0xffff;
			} else {
				CPU_Push32(SegValue(cs));
				CPU_Push32(oldeip);
				reg_eip=offset;
			}
			}
			catch (GuestPageFaultException &pf) {
				reg_esp = old_esp;
				reg_eip = old_eip;
				throw;
			}

			Segs.expanddown[cs]=call.GetExpandDown();
			Segs.limit[cs]=do_seg_limits?call.GetLimit():(~0UL);
			Segs.phys[cs]=call.GetBase();
			cpu.code.big=call.Big()>0;
			Segs.val[cs]=(selector & 0xfffc) | cpu.cpl;
			return;
		case DESC_386_CALL_GATE: 
		case DESC_286_CALL_GATE:
			{
				CPU_CHECK_COND(call.DPL()<cpu.cpl,
					"CALL:Gate:Gate DPL<CPL",
					EXCEPTION_GP,selector & 0xfffc)
				CPU_CHECK_COND(call.DPL()<rpl,
					"CALL:Gate:Gate DPL<RPL",
					EXCEPTION_GP,selector & 0xfffc)
				CPU_CHECK_COND(!call.saved.seg.p,
					"CALL:Gate:Segment not present",
					EXCEPTION_NP,selector & 0xfffc)
				Descriptor n_cs_desc;
				Bitu n_cs_sel=call.GetSelector();

				CPU_CHECK_COND((n_cs_sel & 0xfffc)==0,
					"CALL:Gate:CS selector zero",
					EXCEPTION_GP,0)
				CPU_CHECK_COND(!cpu.gdt.GetDescriptor(n_cs_sel,n_cs_desc),
					"CALL:Gate:CS beyond limits",
					EXCEPTION_GP,n_cs_sel & 0xfffc)
				Bitu n_cs_dpl	= n_cs_desc.DPL();
				CPU_CHECK_COND(n_cs_dpl>cpu.cpl,
					"CALL:Gate:CS DPL>CPL",
					EXCEPTION_GP,n_cs_sel & 0xfffc)

				CPU_CHECK_COND(!n_cs_desc.saved.seg.p,
					"CALL:Gate:CS not present",
					EXCEPTION_NP,n_cs_sel & 0xfffc)

				Bitu n_eip		= call.GetOffset();
				switch (n_cs_desc.Type()) {
				case DESC_CODE_N_NC_A:case DESC_CODE_N_NC_NA:
				case DESC_CODE_R_NC_A:case DESC_CODE_R_NC_NA:
					/* Check if we goto inner priviledge */
					if (n_cs_dpl < cpu.cpl) {
						/* Get new SS:ESP out of TSS */
						Bitu n_ss_sel,n_esp;
						Descriptor n_ss_desc;
						cpu_tss.Get_SSx_ESPx(n_cs_dpl,n_ss_sel,n_esp);
						CPU_CHECK_COND((n_ss_sel & 0xfffc)==0,
							"CALL:Gate:NC:SS selector zero",
							EXCEPTION_TS,0)
						CPU_CHECK_COND(!cpu.gdt.GetDescriptor(n_ss_sel,n_ss_desc),
							"CALL:Gate:Invalid SS selector",
							EXCEPTION_TS,n_ss_sel & 0xfffc)
						CPU_CHECK_COND(((n_ss_sel & 3)!=n_cs_desc.DPL()) || (n_ss_desc.DPL()!=n_cs_desc.DPL()),
							"CALL:Gate:Invalid SS selector privileges",
							EXCEPTION_TS,n_ss_sel & 0xfffc)

						switch (n_ss_desc.Type()) {
						case DESC_DATA_EU_RW_NA:		case DESC_DATA_EU_RW_A:
						case DESC_DATA_ED_RW_NA:		case DESC_DATA_ED_RW_A:
							// writable data segment
							break;
						default:
							E_Exit("Call:Gate:SS no writable data segment");	// or #TS(ss_sel)
						}
						CPU_CHECK_COND(!n_ss_desc.saved.seg.p,
							"CALL:Gate:Stack segment not present",
							EXCEPTION_SS,n_ss_sel & 0xfffc)

						/* Load the new SS:ESP and save data on it */
						Bitu o_esp		= reg_esp;
						Bitu o_ss		= SegValue(ss);
						PhysPt o_stack  = SegPhys(ss)+(reg_esp & cpu.stack.mask);


						// catch pagefaults
						if (call.saved.gate.paramcount&31) {
							if (call.Type()==DESC_386_CALL_GATE) {
								for (Bits i=(call.saved.gate.paramcount&31)-1;i>=0;i--) 
									mem_readd(o_stack+i*4);
							} else {
								for (Bits i=(call.saved.gate.paramcount&31)-1;i>=0;i--)
									mem_readw(o_stack+i*2);
							}
						}

						bool old_allow = dosbox_allow_nonrecursive_page_fault;

						/* this code isn't very easy to make interruptible. so temporarily revert to recursive PF handling method */
						dosbox_allow_nonrecursive_page_fault = false;

						// commit point
						Segs.val[ss]=n_ss_sel;
						Segs.phys[ss]=n_ss_desc.GetBase();
						Segs.limit[ss]=do_seg_limits?n_ss_desc.GetLimit():(~0UL);
						Segs.expanddown[ss]=n_ss_desc.GetExpandDown();
						if (n_ss_desc.Big()) {
							cpu.stack.big=true;
							cpu.stack.mask=0xffffffff;
							cpu.stack.notmask=0;
							reg_esp=n_esp;
						} else {
							cpu.stack.big=false;
							cpu.stack.mask=0xffff;
							cpu.stack.notmask=0xffff0000;
							reg_sp=n_esp & 0xffff;
						}

						CPU_SetCPL(n_cs_desc.DPL());
						Bit16u oldcs    = SegValue(cs);
						/* Switch to new CS:EIP */
						Segs.expanddown[cs]=n_cs_desc.GetExpandDown();
						Segs.limit[cs]  = do_seg_limits?n_cs_desc.GetLimit():(~0UL);
						Segs.phys[cs]	= n_cs_desc.GetBase();
						Segs.val[cs]	= (n_cs_sel & 0xfffc) | cpu.cpl;
						cpu.code.big	= n_cs_desc.Big()>0;
						reg_eip			= n_eip;
						if (!use32)	reg_eip&=0xffff;

						if (call.Type()==DESC_386_CALL_GATE) {
							CPU_Push32(o_ss);		//save old stack
							CPU_Push32(o_esp);
							if (call.saved.gate.paramcount&31)
								for (Bits i=(call.saved.gate.paramcount&31)-1;i>=0;i--) 
									CPU_Push32(mem_readd(o_stack+i*4));
							CPU_Push32(oldcs);
							CPU_Push32(oldeip);
						} else {
							CPU_Push16(o_ss);		//save old stack
							CPU_Push16(o_esp);
							if (call.saved.gate.paramcount&31)
								for (Bits i=(call.saved.gate.paramcount&31)-1;i>=0;i--)
									CPU_Push16(mem_readw(o_stack+i*2));
							CPU_Push16(oldcs);
							CPU_Push16(oldeip);
						}

						dosbox_allow_nonrecursive_page_fault = old_allow;
						break;		
					} else if (n_cs_dpl > cpu.cpl)
						E_Exit("CALL:GATE:CS DPL>CPL");		// or #GP(sel)
				case DESC_CODE_N_C_A:case DESC_CODE_N_C_NA:
				case DESC_CODE_R_C_A:case DESC_CODE_R_C_NA:
					// zrdx extender

					try {
					if (call.Type()==DESC_386_CALL_GATE) {
						CPU_Push32(SegValue(cs));
						CPU_Push32(oldeip);
					} else {
						CPU_Push16(SegValue(cs));
						CPU_Push16(oldeip);
					}
					}
					catch (GuestPageFaultException &pf) {
						reg_esp = old_esp;
						reg_eip = old_eip;
						throw;
					}

					/* Switch to new CS:EIP */
					Segs.expanddown[cs]=n_cs_desc.GetExpandDown();
					Segs.limit[cs]  = do_seg_limits?n_cs_desc.GetLimit():(~0UL);
					Segs.phys[cs]	= n_cs_desc.GetBase();
					Segs.val[cs]	= (n_cs_sel & 0xfffc) | cpu.cpl;
					cpu.code.big	= n_cs_desc.Big()>0;
					reg_eip			= n_eip;
					if (!use32)	reg_eip&=0xffff;
					break;
				default:
					E_Exit("CALL:GATE:CS no executable segment");
				}
			}			/* Call Gates */
			break;
		case DESC_386_TSS_A:
			CPU_CHECK_COND(call.DPL()<cpu.cpl,
				"CALL:TSS:dpl<cpl",
				EXCEPTION_GP,selector & 0xfffc)
			CPU_CHECK_COND(call.DPL()<rpl,
				"CALL:TSS:dpl<rpl",
				EXCEPTION_GP,selector & 0xfffc)

			CPU_CHECK_COND(!call.saved.seg.p,
				"CALL:TSS:Segment not present",
				EXCEPTION_NP,selector & 0xfffc)

			LOG(LOG_CPU,LOG_NORMAL)("CALL:TSS to %X",selector);
			CPU_SwitchTask(selector,TSwitch_CALL_INT,oldeip);
			break;
		case DESC_DATA_EU_RW_NA:	// vbdos
		case DESC_INVALID:			// used by some installers
			CPU_Exception(EXCEPTION_GP,selector & 0xfffc);
			return;
		default:
			E_Exit("CALL:Descriptor type %x unsupported",(int)call.Type());
		}
	}
	assert(1);
}


void CPU_RET(bool use32,Bitu bytes,Bitu oldeip) {
	Bitu orig_esp = reg_esp;

	if (!cpu.pmode || (reg_flags & FLAG_VM)) {
		try {
		Bitu new_ip,new_cs;
		if (!use32) {
			new_ip=CPU_Pop16();
			new_cs=CPU_Pop16();
		} else {
			new_ip=CPU_Pop32();
			new_cs=CPU_Pop32() & 0xffff;
		}
		reg_esp+=bytes;
		SegSet16(cs,new_cs);
		reg_eip=new_ip;
		if (!cpu_allow_big16) cpu.code.big=false;
		return;
		}
		catch (GuestPageFaultException &pf) {
			LOG_MSG("CPU_RET() interrupted real/vm86");
			reg_esp = orig_esp;
			throw;
		}
	} else {
		Bitu offset,selector;
		if (!use32) selector	= mem_readw(SegPhys(ss) + (reg_esp & cpu.stack.mask) + 2);
		else 		selector	= mem_readd(SegPhys(ss) + (reg_esp & cpu.stack.mask) + 4) & 0xffff;

		Descriptor desc;
		Bitu rpl=selector & 3;
		if(rpl < cpu.cpl) {
			// win setup
			CPU_Exception(EXCEPTION_GP,selector & 0xfffc);
			return;
		}

		CPU_CHECK_COND((selector & 0xfffc)==0,
			"RET:CS selector zero",
			EXCEPTION_GP,0)
		CPU_CHECK_COND(!cpu.gdt.GetDescriptor(selector,desc),
			"RET:CS beyond limits",
			EXCEPTION_GP,selector & 0xfffc)

		if (cpu.cpl==rpl) {	
			/* Return to same level */
			switch (desc.Type()) {
			case DESC_CODE_N_NC_A:case DESC_CODE_N_NC_NA:
			case DESC_CODE_R_NC_A:case DESC_CODE_R_NC_NA:
				CPU_CHECK_COND(cpu.cpl!=desc.DPL(),
					"RET to NC segment of other privilege",
					EXCEPTION_GP,selector & 0xfffc)
				goto RET_same_level;
			case DESC_CODE_N_C_A:case DESC_CODE_N_C_NA:
			case DESC_CODE_R_C_A:case DESC_CODE_R_C_NA:
				CPU_CHECK_COND(desc.DPL()>cpu.cpl,
					"RET to C segment of higher privilege",
					EXCEPTION_GP,selector & 0xfffc)
				break;
			default:
				E_Exit("RET from illegal descriptor type %X",(int)desc.Type());
			}
RET_same_level:
			if (!desc.saved.seg.p) {
				// borland extender (RTM)
				CPU_Exception(EXCEPTION_NP,selector & 0xfffc);
				return;
			}

			// commit point
			try {
			if (!use32) {
				offset=CPU_Pop16();
				selector=CPU_Pop16();
			} else {
				offset=CPU_Pop32();
				selector=CPU_Pop32() & 0xffff;
			}
			}
			catch (GuestPageFaultException &pf) {
				LOG_MSG("CPU_RET() interrupted prot rpl==cpl");
				reg_esp = orig_esp;
				throw;
			}

			Segs.expanddown[cs]=desc.GetExpandDown();
			Segs.limit[cs]=do_seg_limits?desc.GetLimit():(~0UL);
			Segs.phys[cs]=desc.GetBase();
			cpu.code.big=desc.Big()>0;
			Segs.val[cs]=selector;
			reg_eip=offset;
			if (cpu.stack.big) {
				reg_esp+=bytes;
			} else {
				reg_sp+=bytes;
			}
			LOG(LOG_CPU,LOG_NORMAL)("RET - Same level to %X:%X RPL %X DPL %X",selector,offset,rpl,desc.DPL());
			return;
		} else {
			/* Return to outer level */
			switch (desc.Type()) {
			case DESC_CODE_N_NC_A:case DESC_CODE_N_NC_NA:
			case DESC_CODE_R_NC_A:case DESC_CODE_R_NC_NA:
				CPU_CHECK_COND(desc.DPL()!=rpl,
					"RET to outer NC segment with DPL!=RPL",
					EXCEPTION_GP,selector & 0xfffc)
				break;
			case DESC_CODE_N_C_A:case DESC_CODE_N_C_NA:
			case DESC_CODE_R_C_A:case DESC_CODE_R_C_NA:
				CPU_CHECK_COND(desc.DPL()>rpl,
					"RET to outer C segment with DPL>RPL",
					EXCEPTION_GP,selector & 0xfffc)
				break;
			default:
				E_Exit("RET from illegal descriptor type %X",(int)desc.Type());		// or #GP(selector)
			}

			CPU_CHECK_COND(!desc.saved.seg.p,
				"RET:Outer level:CS not present",
				EXCEPTION_NP,selector & 0xfffc)

			// commit point
			Bitu n_esp,n_ss;
			try {
			if (use32) {
				offset=CPU_Pop32();
				selector=CPU_Pop32() & 0xffff;
				reg_esp+=bytes;
				n_esp = CPU_Pop32();
				n_ss = CPU_Pop32() & 0xffff;
			} else {
				offset=CPU_Pop16();
				selector=CPU_Pop16();
				reg_esp+=bytes;
				n_esp = CPU_Pop16();
				n_ss = CPU_Pop16();
			}
			}
			catch (GuestPageFaultException &pf) {
				LOG_MSG("CPU_RET() interrupted prot #2");
				reg_esp = orig_esp;
				throw;
			}

			CPU_CHECK_COND((n_ss & 0xfffc)==0,
				"RET to outer level with SS selector zero",
				EXCEPTION_GP,0)

			Descriptor n_ss_desc;
			CPU_CHECK_COND(!cpu.gdt.GetDescriptor(n_ss,n_ss_desc),
				"RET:SS beyond limits",
				EXCEPTION_GP,n_ss & 0xfffc)

			CPU_CHECK_COND(((n_ss & 3)!=rpl) || (n_ss_desc.DPL()!=rpl),
				"RET to outer segment with invalid SS privileges",
				EXCEPTION_GP,n_ss & 0xfffc)
			switch (n_ss_desc.Type()) {
			case DESC_DATA_EU_RW_NA:		case DESC_DATA_EU_RW_A:
			case DESC_DATA_ED_RW_NA:		case DESC_DATA_ED_RW_A:
				break;
			default:
				E_Exit("RET:SS selector type no writable data segment");	// or #GP(selector)
			}
			CPU_CHECK_COND(!n_ss_desc.saved.seg.p,
				"RET:Stack segment not present",
				EXCEPTION_SS,n_ss & 0xfffc)

			CPU_SetCPL(rpl);
			Segs.expanddown[cs]=desc.GetExpandDown();
			Segs.limit[cs]=do_seg_limits?desc.GetLimit():(~0UL);
			Segs.phys[cs]=desc.GetBase();
			cpu.code.big=desc.Big()>0;
			Segs.val[cs]=(selector&0xfffc) | cpu.cpl;
			reg_eip=offset;

			Segs.val[ss]=n_ss;
			Segs.phys[ss]=n_ss_desc.GetBase();
			Segs.limit[ss]=do_seg_limits?n_ss_desc.GetLimit():(~0UL);
			Segs.expanddown[ss]=n_ss_desc.GetExpandDown();
			if (n_ss_desc.Big()) {
				cpu.stack.big=true;
				cpu.stack.mask=0xffffffff;
				cpu.stack.notmask=0;
				reg_esp=n_esp+bytes;
			} else {
				cpu.stack.big=false;
				cpu.stack.mask=0xffff;
				cpu.stack.notmask=0xffff0000;
				reg_sp=(n_esp & 0xffff)+bytes;
			}

			CPU_CheckSegments();

//			LOG(LOG_MISC,LOG_ERROR)("RET - Higher level to %X:%X RPL %X DPL %X",selector,offset,rpl,desc.DPL());
			return;
		}
		LOG(LOG_CPU,LOG_NORMAL)("Prot ret %lX:%lX",(unsigned long)selector,(unsigned long)offset);
		return;
	}
	assert(1);
}


Bitu CPU_SLDT(void) {
	return cpu.gdt.SLDT();
}

bool CPU_LLDT(Bitu selector) {
	if (!cpu.gdt.LLDT(selector)) {
		LOG(LOG_CPU,LOG_ERROR)("LLDT failed, selector=%X",selector);
		return true;
	}
	LOG(LOG_CPU,LOG_NORMAL)("LDT Set to %X",selector);
	return false;
}

Bitu CPU_STR(void) {
	return cpu_tss.selector;
}

bool CPU_LTR(Bitu selector) {
	if ((selector & 0xfffc)==0) {
		cpu_tss.SetSelector(selector);
		return false;
	}
	TSS_Descriptor desc;
	if ((selector & 4) || (!cpu.gdt.GetDescriptor(selector,desc))) {
		LOG(LOG_CPU,LOG_ERROR)("LTR failed, selector=%X",selector);
		return CPU_PrepareException(EXCEPTION_GP,selector);
	}

	if ((desc.Type()==DESC_286_TSS_A) || (desc.Type()==DESC_386_TSS_A)) {
		if (!desc.saved.seg.p) {
			LOG(LOG_CPU,LOG_ERROR)("LTR failed, selector=%X (not present)",selector);
			return CPU_PrepareException(EXCEPTION_NP,selector);
		}
		if (!cpu_tss.SetSelector(selector)) E_Exit("LTR failed, selector=%X",(int)selector);
		cpu_tss.desc.SetBusy(true);
		cpu_tss.SaveSelector();
	} else {
		/* Descriptor was no available TSS descriptor */ 
		LOG(LOG_CPU,LOG_NORMAL)("LTR failed, selector=%X (type=%X)",selector,desc.Type());
		return CPU_PrepareException(EXCEPTION_GP,selector);
	}
	return false;
}

void CPU_LGDT(Bitu limit,Bitu base) {
	LOG(LOG_CPU,LOG_NORMAL)("GDT Set to base:%X limit:%X",base,limit);
	cpu.gdt.SetLimit(limit);
	cpu.gdt.SetBase(base);
}

void CPU_LIDT(Bitu limit,Bitu base) {
	LOG(LOG_CPU,LOG_NORMAL)("IDT Set to base:%X limit:%X",base,limit);
	cpu.idt.SetLimit(limit);
	cpu.idt.SetBase(base);
}

Bitu CPU_SGDT_base(void) {
	return cpu.gdt.GetBase();
}
Bitu CPU_SGDT_limit(void) {
	return cpu.gdt.GetLimit();
}

Bitu CPU_SIDT_base(void) {
	return cpu.idt.GetBase();
}
Bitu CPU_SIDT_limit(void) {
	return cpu.idt.GetLimit();
}

static bool snap_cpu_snapped=false;
static Bit32u snap_cpu_saved_cr0;
static Bit32u snap_cpu_saved_cr2;
static Bit32u snap_cpu_saved_cr3;

/* On shutdown, DOSBox needs to snap back to real mode
 * so that it's shutdown code doesn't cause page faults
 * trying to clean up DOS structures when we've booted
 * a 32-bit OS. It shouldn't be cleaning up DOS structures
 * anyway in that case considering they're likely obliterated
 * by the guest OS, but that's something we'll clean up
 * later. */
void CPU_Snap_Back_To_Real_Mode() {
    if (snap_cpu_snapped) return;

    SETFLAGBIT(IF,false);	/* forcibly clear interrupt flag */

    cpu.code.big = false;   /* force back to 16-bit */
    cpu.stack.big = false;
    cpu.stack.mask = 0xffff;
    cpu.stack.notmask = 0xffff0000;

    snap_cpu_saved_cr0 = cpu.cr0;
    snap_cpu_saved_cr2 = paging.cr2;
    snap_cpu_saved_cr3 = paging.cr3;

    CPU_SET_CRX(0,0);	/* force CPU to real mode */
    CPU_SET_CRX(2,0);	/* disable paging */
    CPU_SET_CRX(3,0);	/* clear the page table dir */

    cpu.idt.SetBase(0);         /* or ELSE weird things will happen when INTerrupts are run */
    cpu.idt.SetLimit(1023);

    snap_cpu_snapped = true;
}

void CPU_Snap_Back_Restore() {
	if (!snap_cpu_snapped) return;

	CPU_SET_CRX(0,snap_cpu_saved_cr0);
	CPU_SET_CRX(2,snap_cpu_saved_cr2);
	CPU_SET_CRX(3,snap_cpu_saved_cr3);

	snap_cpu_snapped = false;
}

void CPU_Snap_Back_Forget() {
	snap_cpu_snapped = false;
}

static bool printed_cycles_auto_info = false;
void CPU_SET_CRX(Bitu cr,Bitu value) {
	switch (cr) {
	case 0:
		{
			value|=CR0_FPUPRESENT;
			Bitu changed=cpu.cr0 ^ value;
			if (!changed) return;
			if (GCC_UNLIKELY(changed & CR0_WRITEPROTECT)) {
				if (CPU_ArchitectureType >= CPU_ARCHTYPE_486OLD)
					PAGING_SetWP((value&CR0_WRITEPROTECT)? true:false);
			}
			cpu.cr0=value;
			if (value & CR0_PROTECTION) {
				cpu.pmode=true;
				LOG(LOG_CPU,LOG_NORMAL)("Protected mode");
				PAGING_Enable((value&CR0_PAGING)? true:false);

				if (!(CPU_AutoDetermineMode&CPU_AUTODETERMINE_MASK)) break;

				if (CPU_AutoDetermineMode&CPU_AUTODETERMINE_CYCLES) {
					CPU_CycleAutoAdjust=true;
					CPU_CycleLeft=0;
					CPU_Cycles=0;
					CPU_OldCycleMax=CPU_CycleMax;
					GFX_SetTitle(CPU_CyclePercUsed,-1,-1,false);
					if(!printed_cycles_auto_info) {
						printed_cycles_auto_info = true;
						LOG_MSG("DOSBox switched to max cycles, because of the setting: cycles=auto. If the game runs too fast try a fixed cycles amount in DOSBox's options.");
					}
				} else {
					GFX_SetTitle(-1,-1,-1,false);
				}
#if (C_DYNAMIC_X86)
				if (CPU_AutoDetermineMode&CPU_AUTODETERMINE_CORE) {
					CPU_Core_Dyn_X86_Cache_Init(true);
					cpudecoder=&CPU_Core_Dyn_X86_Run;
					strcpy(core_mode, "dynamic");
				}
#endif
				CPU_AutoDetermineMode<<=CPU_AUTODETERMINE_SHIFT;
			} else {
				cpu.pmode=false;
				if (value & CR0_PAGING) LOG_MSG("Paging requested without PE=1");
				PAGING_Enable(false);
				LOG(LOG_CPU,LOG_NORMAL)("Real mode");
			}
			break;
		}
	case 2:
		paging.cr2=value;
		break;
	case 3:
		PAGING_SetDirBase(value);
		break;
	default:
		LOG(LOG_CPU,LOG_ERROR)("Unhandled MOV CR%d,%X",cr,value);
		break;
	}
}

bool CPU_WRITE_CRX(Bitu cr,Bitu value) {
	/* Check if privileged to access control registers */
	if (cpu.pmode && (cpu.cpl>0)) return CPU_PrepareException(EXCEPTION_GP,0);
	if ((cr==1) || (cr>4)) return CPU_PrepareException(EXCEPTION_UD,0);
	if (CPU_ArchitectureType<CPU_ARCHTYPE_486OLD) {
		if (cr==4) return CPU_PrepareException(EXCEPTION_UD,0);
	}
	CPU_SET_CRX(cr,value);
	return false;
}

Bitu CPU_GET_CRX(Bitu cr) {
	switch (cr) {
	case 0:
		if (CPU_ArchitectureType>=CPU_ARCHTYPE_PENTIUM) return cpu.cr0;
		else if (CPU_ArchitectureType>=CPU_ARCHTYPE_486OLD) return (cpu.cr0 & 0xe005003f);
		else return (cpu.cr0 | 0x7ffffff0);
	case 2:
		return paging.cr2;
	case 3:
		return PAGING_GetDirBase() & 0xfffff000;
	default:
		LOG(LOG_CPU,LOG_ERROR)("Unhandled MOV XXX, CR%d",cr);
		break;
	}
	return 0;
}

bool CPU_READ_CRX(Bitu cr,Bit32u & retvalue) {
	/* Check if privileged to access control registers */
	if (cpu.pmode && (cpu.cpl>0)) return CPU_PrepareException(EXCEPTION_GP,0);
	if ((cr==1) || (cr>4)) return CPU_PrepareException(EXCEPTION_UD,0);
	if (CPU_ArchitectureType<CPU_ARCHTYPE_486OLD) {
		if (cr==4) return CPU_PrepareException(EXCEPTION_UD,0);
	}
	retvalue=CPU_GET_CRX(cr);
	return false;
}


bool CPU_WRITE_DRX(Bitu dr,Bitu value) {
	/* Check if privileged to access control registers */
	if (cpu.pmode && (cpu.cpl>0)) return CPU_PrepareException(EXCEPTION_GP,0);
	switch (dr) {
	case 0:
	case 1:
	case 2:
	case 3:
		cpu.drx[dr]=value;
		break;
	case 4:
	case 6:
		cpu.drx[6]=(value|0xffff0ff0) & 0xffffefff;
		break;
	case 5:
	case 7:
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUM) {
			cpu.drx[7]=(value|0x400) & 0xffff2fff;
		} else {
			cpu.drx[7]=(value|0x400);
		}
		break;
	default:
		LOG(LOG_CPU,LOG_ERROR)("Unhandled MOV DR%d,%X",dr,value);
		break;
	}
	return false;
}

bool CPU_READ_DRX(Bitu dr,Bit32u & retvalue) {
	/* Check if privileged to access control registers */
	if (cpu.pmode && (cpu.cpl>0)) return CPU_PrepareException(EXCEPTION_GP,0);
	switch (dr) {
	case 0:
	case 1:
	case 2:
	case 3:
	case 6:
	case 7:
		retvalue=cpu.drx[dr];
		break;
	case 4:
		retvalue=cpu.drx[6];
		break;
	case 5:
		retvalue=cpu.drx[7];
		break;
	default:
		LOG(LOG_CPU,LOG_ERROR)("Unhandled MOV XXX, DR%d",dr);
		retvalue=0;
		break;
	}
	return false;
}

bool CPU_WRITE_TRX(Bitu tr,Bitu value) {
	/* Check if privileged to access control registers */
	if (cpu.pmode && (cpu.cpl>0)) return CPU_PrepareException(EXCEPTION_GP,0);
	switch (tr) {
//	case 3:
	case 6:
	case 7:
		cpu.trx[tr]=value;
		return false;
	default:
		LOG(LOG_CPU,LOG_ERROR)("Unhandled MOV TR%d,%X",tr,value);
		break;
	}
	return CPU_PrepareException(EXCEPTION_UD,0);
}

bool CPU_READ_TRX(Bitu tr,Bit32u & retvalue) {
	/* Check if privileged to access control registers */
	if (cpu.pmode && (cpu.cpl>0)) return CPU_PrepareException(EXCEPTION_GP,0);
	switch (tr) {
//	case 3:
	case 6:
	case 7:
		retvalue=cpu.trx[tr];
		return false;
	default:
		LOG(LOG_CPU,LOG_ERROR)("Unhandled MOV XXX, TR%d",tr);
		break;
	}
	return CPU_PrepareException(EXCEPTION_UD,0);
}


Bitu CPU_SMSW(void) {
	return cpu.cr0;
}

bool CPU_LMSW(Bitu word) {
	if (cpu.pmode && (cpu.cpl>0)) return CPU_PrepareException(EXCEPTION_GP,0);
	word&=0xf;
	if (cpu.cr0 & 1) word|=1; 
	word|=(cpu.cr0&0xfffffff0);
	CPU_SET_CRX(0,word);
	return false;
}

void CPU_ARPL(Bitu & dest_sel,Bitu src_sel) {
	FillFlags();
	if ((dest_sel & 3) < (src_sel & 3)) {
		dest_sel=(dest_sel & 0xfffc) + (src_sel & 3);
//		dest_sel|=0xff3f0000;
		SETFLAGBIT(ZF,true);
	} else {
		SETFLAGBIT(ZF,false);
	} 
}
	
void CPU_LAR(Bitu selector,Bitu & ar) {
	FillFlags();
	if (selector == 0) {
		SETFLAGBIT(ZF,false);
		return;
	}
	Descriptor desc;Bitu rpl=selector & 3;
	if (!cpu.gdt.GetDescriptor(selector,desc)){
		SETFLAGBIT(ZF,false);
		return;
	}
	switch (desc.Type()){
	case DESC_CODE_N_C_A:	case DESC_CODE_N_C_NA:
	case DESC_CODE_R_C_A:	case DESC_CODE_R_C_NA:
		break;

	case DESC_286_INT_GATE:		case DESC_286_TRAP_GATE:	{
	case DESC_386_INT_GATE:		case DESC_386_TRAP_GATE:
		SETFLAGBIT(ZF,false);
		return;
	}

	case DESC_LDT:
	case DESC_TASK_GATE:

	case DESC_286_TSS_A:		case DESC_286_TSS_B:
	case DESC_286_CALL_GATE:

	case DESC_386_TSS_A:		case DESC_386_TSS_B:
	case DESC_386_CALL_GATE:
	

	case DESC_DATA_EU_RO_NA:	case DESC_DATA_EU_RO_A:
	case DESC_DATA_EU_RW_NA:	case DESC_DATA_EU_RW_A:
	case DESC_DATA_ED_RO_NA:	case DESC_DATA_ED_RO_A:
	case DESC_DATA_ED_RW_NA:	case DESC_DATA_ED_RW_A:
	case DESC_CODE_N_NC_A:		case DESC_CODE_N_NC_NA:
	case DESC_CODE_R_NC_A:		case DESC_CODE_R_NC_NA:
		if (desc.DPL()<cpu.cpl || desc.DPL() < rpl) {
			SETFLAGBIT(ZF,false);
			return;
		}
		break;
	default:
		SETFLAGBIT(ZF,false);
		return;
	}
	/* Valid descriptor */
	ar=desc.saved.fill[1] & 0x00ffff00;
	SETFLAGBIT(ZF,true);
}

void CPU_LSL(Bitu selector,Bitu & limit) {
	FillFlags();
	if (selector == 0) {
		SETFLAGBIT(ZF,false);
		return;
	}
	Descriptor desc;Bitu rpl=selector & 3;
	if (!cpu.gdt.GetDescriptor(selector,desc)){
		SETFLAGBIT(ZF,false);
		return;
	}
	switch (desc.Type()){
	case DESC_CODE_N_C_A:	case DESC_CODE_N_C_NA:
	case DESC_CODE_R_C_A:	case DESC_CODE_R_C_NA:
		break;

	case DESC_LDT:
	case DESC_286_TSS_A:
	case DESC_286_TSS_B:
	
	case DESC_386_TSS_A:
	case DESC_386_TSS_B:

	case DESC_DATA_EU_RO_NA:	case DESC_DATA_EU_RO_A:
	case DESC_DATA_EU_RW_NA:	case DESC_DATA_EU_RW_A:
	case DESC_DATA_ED_RO_NA:	case DESC_DATA_ED_RO_A:
	case DESC_DATA_ED_RW_NA:	case DESC_DATA_ED_RW_A:
	
	case DESC_CODE_N_NC_A:		case DESC_CODE_N_NC_NA:
	case DESC_CODE_R_NC_A:		case DESC_CODE_R_NC_NA:
		if (desc.DPL()<cpu.cpl || desc.DPL() < rpl) {
			SETFLAGBIT(ZF,false);
			return;
		}
		break;
	default:
		SETFLAGBIT(ZF,false);
		return;
	}
	limit=desc.GetLimit();
	SETFLAGBIT(ZF,true);
}

void CPU_VERR(Bitu selector) {
	FillFlags();
	if (selector == 0) {
		SETFLAGBIT(ZF,false);
		return;
	}
	Descriptor desc;Bitu rpl=selector & 3;
	if (!cpu.gdt.GetDescriptor(selector,desc)){
		SETFLAGBIT(ZF,false);
		return;
	}
	switch (desc.Type()){
	case DESC_CODE_R_C_A:		case DESC_CODE_R_C_NA:	
		//Conforming readable code segments can be always read 
		break;
	case DESC_DATA_EU_RO_NA:	case DESC_DATA_EU_RO_A:
	case DESC_DATA_EU_RW_NA:	case DESC_DATA_EU_RW_A:
	case DESC_DATA_ED_RO_NA:	case DESC_DATA_ED_RO_A:
	case DESC_DATA_ED_RW_NA:	case DESC_DATA_ED_RW_A:

	case DESC_CODE_R_NC_A:		case DESC_CODE_R_NC_NA:
		if (desc.DPL()<cpu.cpl || desc.DPL() < rpl) {
			SETFLAGBIT(ZF,false);
			return;
		}
		break;
	default:
		SETFLAGBIT(ZF,false);
		return;
	}
	SETFLAGBIT(ZF,true);
}

void CPU_VERW(Bitu selector) {
	FillFlags();
	if (selector == 0) {
		SETFLAGBIT(ZF,false);
		return;
	}
	Descriptor desc;Bitu rpl=selector & 3;
	if (!cpu.gdt.GetDescriptor(selector,desc)){
		SETFLAGBIT(ZF,false);
		return;
	}
	switch (desc.Type()){
	case DESC_DATA_EU_RW_NA:	case DESC_DATA_EU_RW_A:
	case DESC_DATA_ED_RW_NA:	case DESC_DATA_ED_RW_A:
		if (desc.DPL()<cpu.cpl || desc.DPL() < rpl) {
			SETFLAGBIT(ZF,false);
			return;
		}
		break;
	default:
		SETFLAGBIT(ZF,false);
		return;
	}
	SETFLAGBIT(ZF,true);
}

bool CPU_SetSegGeneral(SegNames seg,Bitu value) {
	value &= 0xffff;
	if (!cpu.pmode || (reg_flags & FLAG_VM)) {
		Segs.val[seg]=value;
		Segs.phys[seg]=value << 4;
		if (seg==ss) {
			cpu.stack.big=false;
			cpu.stack.mask=0xffff;
			cpu.stack.notmask=0xffff0000;
		}

		/* real mode: loads do not change the limit. "Flat real mode" would not be possible otherwise.
		 * vm86: loads are fixed at 64KB (right?) */
		if (reg_flags & FLAG_VM)
			Segs.limit[seg] = 0xFFFF;

		return false;
	} else {
		if (seg==ss) {
			// Stack needs to be non-zero
			if ((value & 0xfffc)==0) {
				E_Exit("CPU_SetSegGeneral: Stack segment zero");
//				return CPU_PrepareException(EXCEPTION_GP,0);
			}
			Descriptor desc;
			if (!cpu.gdt.GetDescriptor(value,desc)) {
				E_Exit("CPU_SetSegGeneral: Stack segment beyond limits");
//				return CPU_PrepareException(EXCEPTION_GP,value & 0xfffc);
			}
			if (((value & 3)!=cpu.cpl) || (desc.DPL()!=cpu.cpl)) {
				E_Exit("CPU_SetSegGeneral: Stack segment with invalid privileges");
//				return CPU_PrepareException(EXCEPTION_GP,value & 0xfffc);
			}

			switch (desc.Type()) {
			case DESC_DATA_EU_RW_NA:		case DESC_DATA_EU_RW_A:
			case DESC_DATA_ED_RW_NA:		case DESC_DATA_ED_RW_A:
				break;
			default:
				//Earth Siege 1
				return CPU_PrepareException(EXCEPTION_GP,value & 0xfffc);
			}

			if (!desc.saved.seg.p) {
//				E_Exit("CPU_SetSegGeneral: Stack segment not present");	// or #SS(sel)
				return CPU_PrepareException(EXCEPTION_SS,value & 0xfffc);
			}

			Segs.val[seg]=value;
			Segs.phys[seg]=desc.GetBase();
			Segs.limit[seg]=do_seg_limits?desc.GetLimit():(~0UL);
			Segs.expanddown[seg]=desc.GetExpandDown();
			if (desc.Big()) {
				cpu.stack.big=true;
				cpu.stack.mask=0xffffffff;
				cpu.stack.notmask=0;
			} else {
				cpu.stack.big=false;
				cpu.stack.mask=0xffff;
				cpu.stack.notmask=0xffff0000;
			}
		} else {
			if ((value & 0xfffc)==0) {
				Segs.val[seg]=value;
				Segs.phys[seg]=0;	// ??
				return false;
			}
			Descriptor desc;
			if (!cpu.gdt.GetDescriptor(value,desc)) {
				return CPU_PrepareException(EXCEPTION_GP,value & 0xfffc);
			}
			switch (desc.Type()) {
			case DESC_DATA_EU_RO_NA:		case DESC_DATA_EU_RO_A:
			case DESC_DATA_EU_RW_NA:		case DESC_DATA_EU_RW_A:
			case DESC_DATA_ED_RO_NA:		case DESC_DATA_ED_RO_A:
			case DESC_DATA_ED_RW_NA:		case DESC_DATA_ED_RW_A:
			case DESC_CODE_R_NC_A:			case DESC_CODE_R_NC_NA:
				if (((value & 3)>desc.DPL()) || (cpu.cpl>desc.DPL())) {
					// extreme pinball
					return CPU_PrepareException(EXCEPTION_GP,value & 0xfffc);
				}
				break;
			case DESC_CODE_R_C_A:			case DESC_CODE_R_C_NA:
				break;
			default:
				// gabriel knight
				return CPU_PrepareException(EXCEPTION_GP,value & 0xfffc);

			}
			if (!desc.saved.seg.p) {
				// win
				return CPU_PrepareException(EXCEPTION_NP,value & 0xfffc);
			}

			Segs.val[seg]=value;
			Segs.phys[seg]=desc.GetBase();
			Segs.limit[seg]=do_seg_limits?desc.GetLimit():(~0UL);
			Segs.expanddown[seg]=desc.GetExpandDown();
		}

		return false;
	}
}

bool CPU_PopSeg(SegNames seg,bool use32) {
	Bitu val=mem_readw(SegPhys(ss) + (reg_esp & cpu.stack.mask));
	if (CPU_SetSegGeneral(seg,val)) return true;
	Bitu addsp=use32?0x04:0x02;
	reg_esp=(reg_esp&cpu.stack.notmask)|((reg_esp+addsp)&cpu.stack.mask);
	return false;
}

extern bool enable_fpu;

bool CPU_CPUID(void) {
	if (CPU_ArchitectureType < CPU_ARCHTYPE_486NEW) return false;
	switch (reg_eax) {
	case 0:	/* Vendor ID String and maximum level? */
		reg_eax=1;  /* Maximum level */ 
		reg_ebx='G' | ('e' << 8) | ('n' << 16) | ('u'<< 24); 
		reg_edx='i' | ('n' << 8) | ('e' << 16) | ('I'<< 24); 
		reg_ecx='n' | ('t' << 8) | ('e' << 16) | ('l'<< 24); 
		break;
	case 1:	/* get processor type/family/model/stepping and feature flags */
		if ((CPU_ArchitectureType == CPU_ARCHTYPE_486NEW) ||
			(CPU_ArchitectureType == CPU_ARCHTYPE_MIXED)) {
			reg_eax=0x402;		/* intel 486dx */
			reg_ebx=0;			/* Not Supported */
			reg_ecx=0;			/* No features */
			reg_edx=enable_fpu?1:0;	/* FPU */
		} else if (CPU_ArchitectureType == CPU_ARCHTYPE_PENTIUM) {
			reg_eax=0x513;		/* intel pentium */
			reg_ebx=0;			/* Not Supported */
			reg_ecx=0;			/* No features */
			reg_edx=0x00000010|(enable_fpu?1:0);	/* FPU+TimeStamp/RDTSC */
			if (enable_msr) reg_edx |= 0x20; /* ModelSpecific/MSR */
            if (enable_cmpxchg8b) reg_edx |= 0x100; /* CMPXCHG8B */
		} else if (CPU_ArchitectureType == CPU_ARCHTYPE_P55CSLOW) {
			reg_eax=0x543;		/* intel pentium mmx (P55C) */
			reg_ebx=0;			/* Not Supported */
			reg_ecx=0;			/* No features */
			reg_edx=0x00800010|(enable_fpu?1:0);	/* FPU+TimeStamp/RDTSC+MMX+ModelSpecific/MSR */
			if (enable_msr) reg_edx |= 0x20; /* ModelSpecific/MSR */
            if (enable_cmpxchg8b) reg_edx |= 0x100; /* CMPXCHG8B */
		} else if (CPU_ArchitectureType == CPU_ARCHTYPE_PPROSLOW) {
			reg_eax=0x612;		/* intel pentium pro */
			reg_ebx=0;			/* Not Supported */
			reg_ecx=0;			/* No features */
			reg_edx=0x00008011;	/* FPU+TimeStamp/RDTSC */
		} else {
			return false;
		}
		break;
	default:
		LOG(LOG_CPU,LOG_ERROR)("Unhandled CPUID Function %x",reg_eax);
		reg_eax=0;
		reg_ebx=0;
		reg_ecx=0;
		reg_edx=0;
		break;
	}
	return true;
}

Bits HLT_Decode(void) {
	/* Once an interrupt occurs, it should change cpu core */
	if (reg_eip!=cpu.hlt.eip || SegValue(cs) != cpu.hlt.cs) {
		cpudecoder=cpu.hlt.old_decoder;
	} else {
		CPU_IODelayRemoved += CPU_Cycles;
		CPU_Cycles=0;
	}
	return 0;
}

void CPU_HLT(Bitu oldeip) {
	/* Since cpu.hlt.old_decoder assigns the current decoder to old, and relies on restoring
	 * it back when finished, setting cpudecoder to HLT_Decode while already HLT_Decode effectively
	 * hangs DOSBox and makes it complete unresponsive. Don't want that! */
	if (cpudecoder == &HLT_Decode) E_Exit("CPU_HLT attempted to set HLT_Decode while CPU decoder already HLT_Decode");

	reg_eip=oldeip;
	CPU_IODelayRemoved += CPU_Cycles;
	CPU_Cycles=0;
	cpu.hlt.cs=SegValue(cs);
	cpu.hlt.eip=reg_eip;
	cpu.hlt.old_decoder=cpudecoder;
	cpudecoder=&HLT_Decode;
}

void CPU_ENTER(bool use32,Bitu bytes,Bitu level) {
	level&=0x1f;
	Bitu sp_index=reg_esp&cpu.stack.mask;
	Bitu bp_index=reg_ebp&cpu.stack.mask;
	if (!use32) {
		sp_index-=2;
		mem_writew(SegPhys(ss)+sp_index,reg_bp);
		reg_bp=(Bit16u)(reg_esp-2);
		if (level) {
			for (Bitu i=1;i<level;i++) {	
				sp_index-=2;bp_index-=2;
				mem_writew(SegPhys(ss)+sp_index,mem_readw(SegPhys(ss)+bp_index));
			}
			sp_index-=2;
			mem_writew(SegPhys(ss)+sp_index,reg_bp);
		}
	} else {
		sp_index-=4;
        mem_writed(SegPhys(ss)+sp_index,reg_ebp);
		reg_ebp=(reg_esp-4);
		if (level) {
			for (Bitu i=1;i<level;i++) {	
				sp_index-=4;bp_index-=4;
				mem_writed(SegPhys(ss)+sp_index,mem_readd(SegPhys(ss)+bp_index));
			}
			sp_index-=4;
			mem_writed(SegPhys(ss)+sp_index,reg_ebp);
		}
	}
	sp_index-=bytes;
	reg_esp=(reg_esp&cpu.stack.notmask)|((sp_index)&cpu.stack.mask);
}

void CPU_CycleIncrease(bool pressed) {
	if (!pressed) return;
	if (CPU_CycleAutoAdjust) {
		CPU_CyclePercUsed+=5;
		if (CPU_CyclePercUsed>105) CPU_CyclePercUsed=105;
		LOG_MSG("CPU speed: max %d percent.",CPU_CyclePercUsed);
		GFX_SetTitle(CPU_CyclePercUsed,-1,-1,false);
	} else {
		Bit32s old_cycles=CPU_CycleMax;
		if (CPU_CycleUp < 100) {
			CPU_CycleMax = (Bit32s)(CPU_CycleMax * (1 + (float)CPU_CycleUp / 100.0));
		} else {
			CPU_CycleMax = (Bit32s)(CPU_CycleMax + CPU_CycleUp);
		}
	    
		CPU_CycleLeft=0;CPU_Cycles=0;
		if (CPU_CycleMax==old_cycles) CPU_CycleMax++;
		if (CPU_AutoDetermineMode&CPU_AUTODETERMINE_CYCLES) {
		    LOG_MSG("CPU:%d cycles (auto)",CPU_CycleMax);
		} else {
		    CPU_CyclesSet=CPU_CycleMax;
#if (C_DYNAMIC_X86)
            if (CPU_CycleMax > 15000 && cpudecoder != &CPU_Core_Dyn_X86_Run)
                LOG_MSG("CPU speed: fixed %d cycles. If you need more than 20000, try core=dynamic in DOSBox's options.",CPU_CycleMax);
            else
#endif
                LOG_MSG("CPU speed: fixed %d cycles.",CPU_CycleMax);
        }
		GFX_SetTitle(CPU_CycleMax,-1,-1,false);
	}
}

void CPU_CycleDecrease(bool pressed) {
	if (!pressed) return;
	if (CPU_CycleAutoAdjust) {
		CPU_CyclePercUsed-=5;
		if (CPU_CyclePercUsed<=0) CPU_CyclePercUsed=1;
		if(CPU_CyclePercUsed <=70)
			LOG_MSG("CPU speed: max %d percent. If the game runs too fast, try a fixed cycles amount in DOSBox's options.",CPU_CyclePercUsed);
		else
			LOG_MSG("CPU speed: max %d percent.",CPU_CyclePercUsed);
		GFX_SetTitle(CPU_CyclePercUsed,-1,-1,false);
	} else {
		if (CPU_CycleDown < 100) {
			CPU_CycleMax = (Bit32s)(CPU_CycleMax / (1 + (float)CPU_CycleDown / 100.0));
		} else {
			CPU_CycleMax = (Bit32s)(CPU_CycleMax - CPU_CycleDown);
		}
		CPU_CycleLeft=0;CPU_Cycles=0;
		if (CPU_CycleMax <= 0) CPU_CycleMax=1;
		if (CPU_AutoDetermineMode&CPU_AUTODETERMINE_CYCLES) {
		    LOG_MSG("CPU:%d cycles (auto)",CPU_CycleMax);
		} else {
		    CPU_CyclesSet=CPU_CycleMax;
		    LOG_MSG("CPU speed: fixed %d cycles.",CPU_CycleMax);
		}
		GFX_SetTitle(CPU_CycleMax,-1,-1,false);
	}
}

static void CPU_ToggleAutoCycles(bool pressed) {
    if (!pressed)
	return;
    Section* sec=control->GetSection("cpu");
    if(sec) {
	std::string tmp("cycles=");
	if(CPU_CycleAutoAdjust) {
	    std::ostringstream str;
	    str << "fixed " << CPU_CyclesSet;
	    tmp.append(str.str());
	} else if(CPU_AutoDetermineMode&CPU_AUTODETERMINE_CYCLES) {
	    tmp.append("max");
	} else {
	    tmp.append("auto");
	}
	sec->HandleInputline(tmp);
    }
}

static void CPU_ToggleFullCore(bool pressed) {
    if (!pressed)
	return;
    Section* sec=control->GetSection("cpu");
    if(sec) {
	std::string tmp="core=full";
	sec->HandleInputline(tmp);
    }
}

static void CPU_ToggleNormalCore(bool pressed) {
    if (!pressed)
	return;
    Section* sec=control->GetSection("cpu");
    if(sec) {
	std::string tmp="core=normal";
	sec->HandleInputline(tmp);
    }
}

#if (C_DYNAMIC_X86)
static void CPU_ToggleDynamicCore(bool pressed) {
    if (!pressed)
	return;
    Section* sec=control->GetSection("cpu");
    if(sec) {
	std::string tmp="core=dynamic";
	sec->HandleInputline(tmp);
    }
}
#endif

static void CPU_ToggleSimpleCore(bool pressed) {
    if (!pressed)
	return;
    Section* sec=control->GetSection("cpu");
    std::string tmp="core=simple";
    if(sec) {
	sec->HandleInputline(tmp);
    }
}

void CPU_Enable_SkipAutoAdjust(void) {
	if (CPU_CycleAutoAdjust) {
		CPU_CycleMax /= 2;
		if (CPU_CycleMax < CPU_CYCLES_LOWER_LIMIT)
			CPU_CycleMax = CPU_CYCLES_LOWER_LIMIT;
	}
	CPU_SkipCycleAutoAdjust=true;
}

void CPU_Disable_SkipAutoAdjust(void) {
	CPU_SkipCycleAutoAdjust=false;
}


extern Bit32s ticksDone;
extern Bit32u ticksScheduled;
extern int dynamic_core_cache_block_size;

void CPU_Reset_AutoAdjust(void) {
	CPU_IODelayRemoved = 0;
	ticksDone = 0;
	ticksScheduled = 0;
}

class Weitek_PageHandler : public PageHandler {
public:
	Weitek_PageHandler(HostPt /*addr*/){
		flags=PFLAG_NOCODE;
	}

	~Weitek_PageHandler() {
	}

	Bitu readb(PhysPt addr);
	void writeb(PhysPt addr,Bitu val);
	Bitu readw(PhysPt addr);
	void writew(PhysPt addr,Bitu val);
	Bitu readd(PhysPt addr);
	void writed(PhysPt addr,Bitu val);
};

Bitu Weitek_PageHandler::readb(PhysPt addr) {
    LOG_MSG("Weitek stub: readb at 0x%lx",(unsigned long)addr);
	return (Bitu)-1;
}
void Weitek_PageHandler::writeb(PhysPt addr,Bitu val) {
    LOG_MSG("Weitek stub: writeb at 0x%lx val=0x%lx",(unsigned long)addr,(unsigned long)val);
}

Bitu Weitek_PageHandler::readw(PhysPt addr) {
    LOG_MSG("Weitek stub: readw at 0x%lx",(unsigned long)addr);
	return (Bitu)-1;
}

void Weitek_PageHandler::writew(PhysPt addr,Bitu val) {
    LOG_MSG("Weitek stub: writew at 0x%lx val=0x%lx",(unsigned long)addr,(unsigned long)val);
}

Bitu Weitek_PageHandler::readd(PhysPt addr) {
    LOG_MSG("Weitek stub: readd at 0x%lx",(unsigned long)addr);
	return (Bitu)-1;
}

void Weitek_PageHandler::writed(PhysPt addr,Bitu val) {
    LOG_MSG("Weitek stub: writed at 0x%lx val=0x%lx",(unsigned long)addr,(unsigned long)val);
}

Weitek_PageHandler weitek_pagehandler(0);

PageHandler* weitek_memio_cb(MEM_CalloutObject &co,Bitu phys_page) {
    return &weitek_pagehandler;
}

class CPU: public Module_base {
private:
	static bool inited;
public:
	CPU(Section* configuration):Module_base(configuration) {
		Section_prop * section=static_cast<Section_prop *>(configuration);

		if(inited) {
			Change_Config(configuration);
			return;
		}
//		Section_prop * section=static_cast<Section_prop *>(configuration);
		inited=true;
		reg_eax=0;
		reg_ebx=0;
		reg_ecx=0;
		reg_edx=0;
		reg_edi=0;
		reg_esi=0;
		reg_ebp=0;
		reg_esp=0;

		do_seg_limits = section->Get_bool("segment limits");
	
		SegSet16(cs,0); Segs.limit[cs] = do_seg_limits ? 0xFFFF : ~0UL; Segs.expanddown[cs] = false;
		SegSet16(ds,0); Segs.limit[ds] = do_seg_limits ? 0xFFFF : ~0UL; Segs.expanddown[ds] = false;
		SegSet16(es,0); Segs.limit[es] = do_seg_limits ? 0xFFFF : ~0UL; Segs.expanddown[es] = false;
		SegSet16(fs,0); Segs.limit[fs] = do_seg_limits ? 0xFFFF : ~0UL; Segs.expanddown[fs] = false;
		SegSet16(gs,0); Segs.limit[gs] = do_seg_limits ? 0xFFFF : ~0UL; Segs.expanddown[gs] = false;
		SegSet16(ss,0); Segs.limit[ss] = do_seg_limits ? 0xFFFF : ~0UL; Segs.expanddown[ss] = false;
	
		CPU_SetFlags(FLAG_IF,FMASK_ALL);		//Enable interrupts
		cpu.cr0=0xffffffff;
		CPU_SET_CRX(0,0);						//Initialize
		cpu.code.big=false;
		cpu.stack.mask=0xffff;
		cpu.stack.notmask=0xffff0000;
		cpu.stack.big=false;
		cpu.trap_skip=false;
		cpu.idt.SetBase(0);
		cpu.idt.SetLimit(1023);

		for (Bitu i=0; i<7; i++) {
			cpu.drx[i]=0;
			cpu.trx[i]=0;
		}
		if (CPU_ArchitectureType>=CPU_ARCHTYPE_PENTIUM) {
			cpu.drx[6]=0xffff0ff0;
		} else {
			cpu.drx[6]=0xffff1ff0;
		}
		cpu.drx[7]=0x00000400;

		/* Init the cpu cores */
		CPU_Core_Normal_Init();
		CPU_Core_Simple_Init();
		CPU_Core_Full_Init();
#if (C_DYNAMIC_X86)
		CPU_Core_Dyn_X86_Init();
#endif
		MAPPER_AddHandler(CPU_CycleDecrease,MK_f11,MMOD1,"cycledown","Dec Cycles");
		MAPPER_AddHandler(CPU_CycleIncrease,MK_f12,MMOD1,"cycleup"  ,"Inc Cycles");
		MAPPER_AddHandler(CPU_ToggleAutoCycles,MK_equals,MMOD1,"cycauto","AutoCycles");
		MAPPER_AddHandler(CPU_ToggleNormalCore,MK_1,MMOD1,"normal"  ,"NormalCore");
		MAPPER_AddHandler(CPU_ToggleFullCore,MK_2,MMOD1,"full","Full Core");
		MAPPER_AddHandler(CPU_ToggleSimpleCore,MK_4,MMOD1,"simple","SimpleCore");
#if (C_DYNAMIC_X86)
		MAPPER_AddHandler(CPU_ToggleDynamicCore,MK_3,MMOD1,"dynamic","DynCore");
#endif
		Change_Config(configuration);	
		CPU_JMP(false,0,0,0);					//Setup the first cpu core
	}
	bool Change_Config(Section* newconfig){
		Section_prop * section=static_cast<Section_prop *>(newconfig);
		CPU_AutoDetermineMode=CPU_AUTODETERMINE_NONE;
		//CPU_CycleLeft=0;//needed ?
		CPU_Cycles=0;
		CPU_SkipCycleAutoAdjust=false;

		ignore_opcode_63 = section->Get_bool("ignore opcode 63");
		cpu_double_fault_enable = section->Get_bool("double fault");
		cpu_triple_fault_reset = section->Get_bool("reset on triple fault");
		cpu_allow_big16 = section->Get_bool("realbig16");
		if (cpu_allow_big16) LOG(LOG_CPU,LOG_DEBUG)("Emulation of the B (big) bit in real mode enabled\n");

		always_report_double_fault = section->Get_bool("always report double fault");
		always_report_triple_fault = section->Get_bool("always report triple fault");

		dynamic_core_cache_block_size = section->Get_int("dynamic core cache block size");
		if (dynamic_core_cache_block_size < 1 || dynamic_core_cache_block_size > 65536) dynamic_core_cache_block_size = 32;

		Prop_multival* p = section->Get_multival("cycles");
		std::string type = p->GetSection()->Get_string("type");
		std::string str ;
		CommandLine cmd(0,p->GetSection()->Get_string("parameters"));
		if (type=="max") {
			CPU_CycleMax=0;
			CPU_CyclePercUsed=100;
			CPU_CycleAutoAdjust=true;
			CPU_CycleLimit=-1;
			for (Bitu cmdnum=1; cmdnum<=cmd.GetCount(); cmdnum++) {
				if (cmd.FindCommand(cmdnum,str)) {
					if (str.find('%')==str.length()-1) {
						str.erase(str.find('%'));
						int percval=0;
						std::istringstream stream(str);
						stream >> percval;
						if ((percval>0) && (percval<=105)) CPU_CyclePercUsed=(Bit32s)percval;
					} else if (str=="limit") {
						cmdnum++;
						if (cmd.FindCommand(cmdnum,str)) {
							int cyclimit=0;
							std::istringstream stream(str);
							stream >> cyclimit;
							if (cyclimit>0) CPU_CycleLimit=cyclimit;
						}
					}
				}
			}
		} else {
			if (type=="auto") {
				CPU_AutoDetermineMode|=CPU_AUTODETERMINE_CYCLES;
				CPU_CycleMax=3000;
				CPU_OldCycleMax=3000;
				CPU_CyclePercUsed=100;
				for (Bitu cmdnum=0; cmdnum<=cmd.GetCount(); cmdnum++) {
					if (cmd.FindCommand(cmdnum,str)) {
						if (str.find('%')==str.length()-1) {
							str.erase(str.find('%'));
							int percval=0;
							std::istringstream stream(str);
							stream >> percval;
							if ((percval>0) && (percval<=105)) CPU_CyclePercUsed=(Bit32s)percval;
						} else if (str=="limit") {
							cmdnum++;
							if (cmd.FindCommand(cmdnum,str)) {
								int cyclimit=0;
								std::istringstream stream(str);
								stream >> cyclimit;
								if (cyclimit>0) CPU_CycleLimit=cyclimit;
							}
						} else {
							int rmdval=0;
							std::istringstream stream(str);
							stream >> rmdval;
							if (rmdval>0) {
								CPU_CycleMax=(Bit32s)rmdval;
								CPU_OldCycleMax=(Bit32s)rmdval;
							}
						}
					}
				}
			} else if(type =="fixed") {
				cmd.FindCommand(1,str);
				int rmdval=0;
				std::istringstream stream(str);
				stream >> rmdval;
				CPU_CycleMax=(Bit32s)rmdval;
			} else {
				std::istringstream stream(type);
				int rmdval=0;
				stream >> rmdval;
				if(rmdval) CPU_CycleMax=(Bit32s)rmdval;
				if(rmdval) CPU_CyclesSet=(Bit32s)rmdval;
			}
			CPU_CycleAutoAdjust=false;
		}

		enable_fpu=section->Get_bool("fpu");
		cpu_rep_max=section->Get_int("interruptible rep string op");
		ignore_undefined_msr=section->Get_bool("ignore undefined msr");
		enable_msr=section->Get_bool("enable msr");
        enable_cmpxchg8b=section->Get_bool("enable cmpxchg8b");
		CPU_CycleUp=section->Get_int("cycleup");
		CPU_CycleDown=section->Get_int("cycledown");
		std::string core(section->Get_string("core"));
		cpudecoder=&CPU_Core_Normal_Run;
		safe_strncpy(core_mode,core.c_str(),15);
		core_mode[15] = '\0';
		if (core == "normal") {
			cpudecoder=&CPU_Core_Normal_Run;
		} else if (core =="simple") {
			cpudecoder=&CPU_Core_Simple_Run;
		} else if (core == "full") {
			cpudecoder=&CPU_Core_Full_Run;
		} else if (core == "auto") {
			cpudecoder=&CPU_Core_Normal_Run;
			CPU_AutoDetermineMode|=CPU_AUTODETERMINE_CORE;
#if (C_DYNAMIC_X86)
		} else if (core == "dynamic") {
			cpudecoder=&CPU_Core_Dyn_X86_Run;
			CPU_Core_Dyn_X86_SetFPUMode(true);
		} else if (core == "dynamic_nodhfpu") {
			cpudecoder=&CPU_Core_Dyn_X86_Run;
			CPU_Core_Dyn_X86_SetFPUMode(false);
#endif
		} else {
			strcpy(core_mode,"normal");
			LOG_MSG("CPU:Unknown core type %s, switching back to normal.",core.c_str());
		}
  

#if (C_DYNAMIC_X86)
		CPU_Core_Dyn_X86_Cache_Init((core == "dynamic") || (core == "dynamic_nodhfpu"));
#endif

		CPU_ArchitectureType = CPU_ARCHTYPE_MIXED;
		std::string cputype(section->Get_string("cputype"));
		if (cputype == "auto") {
			CPU_ArchitectureType = CPU_ARCHTYPE_MIXED;
		} else if (cputype == "8086") {
			CPU_ArchitectureType = CPU_ARCHTYPE_8086;
			cpudecoder=&CPU_Core8086_Normal_Run;
		} else if (cputype == "8086_prefetch") { /* 6-byte prefetch queue ref [http://www.phatcode.net/res/224/files/html/ch11/11-02.html] */
			CPU_ArchitectureType = CPU_ARCHTYPE_8086;
			if (core == "normal") {
				cpudecoder=&CPU_Core_Prefetch_Run; /* TODO: Alternate 16-bit only decoder for 286 that does NOT include 386+ instructions */
				CPU_PrefetchQueueSize = 4; /* Emulate the 8088, which was more common in home PCs than having an 8086 */
			} else if (core == "auto") {
				cpudecoder=&CPU_Core_Prefetch_Run; /* TODO: Alternate 16-bit only decoder for 286 that does NOT include 386+ instructions */
				CPU_PrefetchQueueSize = 4; /* Emulate the 8088, which was more common in home PCs than having an 8086 */
				CPU_AutoDetermineMode&=(~CPU_AUTODETERMINE_CORE);
			} else {
				E_Exit("prefetch queue emulation requires the normal core setting.");
			}
		} else if (cputype == "80186") {
			CPU_ArchitectureType = CPU_ARCHTYPE_80186;
			cpudecoder=&CPU_Core286_Normal_Run;
		} else if (cputype == "80186_prefetch") { /* 6-byte prefetch queue ref [http://www.phatcode.net/res/224/files/html/ch11/11-02.html] */
			CPU_ArchitectureType = CPU_ARCHTYPE_80186;
			if (core == "normal") {
				cpudecoder=&CPU_Core_Prefetch_Run; /* TODO: Alternate 16-bit only decoder for 286 that does NOT include 386+ instructions */
				CPU_PrefetchQueueSize = 6;
			} else if (core == "auto") {
				cpudecoder=&CPU_Core_Prefetch_Run; /* TODO: Alternate 16-bit only decoder for 286 that does NOT include 386+ instructions */
				CPU_PrefetchQueueSize = 6;
				CPU_AutoDetermineMode&=(~CPU_AUTODETERMINE_CORE);
			} else {
				E_Exit("prefetch queue emulation requires the normal core setting.");
			}
		} else if (cputype == "286") {
			CPU_ArchitectureType = CPU_ARCHTYPE_286;
			cpudecoder=&CPU_Core286_Normal_Run;
		} else if (cputype == "286_prefetch") { /* 6-byte prefetch queue ref [http://www.phatcode.net/res/224/files/html/ch11/11-02.html] */
			CPU_ArchitectureType = CPU_ARCHTYPE_286;
			if (core == "normal") {
				cpudecoder=&CPU_Core_Prefetch_Run; /* TODO: Alternate 16-bit only decoder for 286 that does NOT include 386+ instructions */
				CPU_PrefetchQueueSize = 6;
			} else if (core == "auto") {
				cpudecoder=&CPU_Core_Prefetch_Run; /* TODO: Alternate 16-bit only decoder for 286 that does NOT include 386+ instructions */
				CPU_PrefetchQueueSize = 6;
				CPU_AutoDetermineMode&=(~CPU_AUTODETERMINE_CORE);
			} else {
				E_Exit("prefetch queue emulation requires the normal core setting.");
			}
		} else if (cputype == "386") {
			CPU_ArchitectureType = CPU_ARCHTYPE_386;
		} else if (cputype == "386_prefetch") {
			CPU_ArchitectureType = CPU_ARCHTYPE_386;
			if (core == "normal") {
				cpudecoder=&CPU_Core_Prefetch_Run;
				CPU_PrefetchQueueSize = 16;
			} else if (core == "auto") {
				cpudecoder=&CPU_Core_Prefetch_Run;
				CPU_PrefetchQueueSize = 16;
				CPU_AutoDetermineMode&=(~CPU_AUTODETERMINE_CORE);
			} else {
				E_Exit("prefetch queue emulation requires the normal core setting.");
			}
		} else if (cputype == "486") {
			CPU_ArchitectureType = CPU_ARCHTYPE_486NEW;
		} else if (cputype == "486_prefetch") {
			CPU_ArchitectureType = CPU_ARCHTYPE_486NEW;
			if (core == "normal") {
				cpudecoder=&CPU_Core_Prefetch_Run;
				CPU_PrefetchQueueSize = 32;
			} else if (core == "auto") {
				cpudecoder=&CPU_Core_Prefetch_Run;
				CPU_PrefetchQueueSize = 32;
				CPU_AutoDetermineMode&=(~CPU_AUTODETERMINE_CORE);
			} else {
				E_Exit("prefetch queue emulation requires the normal core setting.");
			}
		} else if (cputype == "pentium") {
			CPU_ArchitectureType = CPU_ARCHTYPE_PENTIUM;
		} else if (cputype == "pentium_mmx") {
			CPU_ArchitectureType = CPU_ARCHTYPE_P55CSLOW;
		} else if (cputype == "ppro_slow") {
			CPU_ArchitectureType = CPU_ARCHTYPE_PPROSLOW;
 		}

		/* WARNING */
		if (CPU_ArchitectureType == CPU_ARCHTYPE_8086) {
			LOG_MSG("CPU warning: 8086 cpu type is experimental at this time");
		}
		else if (CPU_ArchitectureType == CPU_ARCHTYPE_80186) {
			LOG_MSG("CPU warning: 80186 cpu type is experimental at this time");
		}

		if (CPU_ArchitectureType>=CPU_ARCHTYPE_486NEW) CPU_extflags_toggle=(FLAG_ID|FLAG_AC);
		else if (CPU_ArchitectureType>=CPU_ARCHTYPE_486OLD) CPU_extflags_toggle=(FLAG_AC);
		else CPU_extflags_toggle=0;

    // weitek coprocessor emulation?
        if (CPU_ArchitectureType == CPU_ARCHTYPE_386 || CPU_ArchitectureType == CPU_ARCHTYPE_486OLD || CPU_ArchitectureType == CPU_ARCHTYPE_486NEW) {
	        Section_prop *dsection = static_cast<Section_prop *>(control->GetSection("dosbox"));

            enable_weitek = dsection->Get_bool("weitek");
            if (enable_weitek) {
                LOG_MSG("Weitek coprocessor emulation enabled");

                static Bitu weitek_lfb = 0xC0000000UL;
                static Bitu weitek_lfb_pages = 0x2000000UL >> 12UL; /* "The coprocessor will respond to memory addresses 0xC0000000-0xC1FFFFFF" */
                static MEM_Callout_t weitek_lfb_cb = MEM_Callout_t_none;

                if (weitek_lfb_cb == MEM_Callout_t_none) {
                    weitek_lfb_cb = MEM_AllocateCallout(MEM_TYPE_MB);
                    if (weitek_lfb_cb == MEM_Callout_t_none) E_Exit("Unable to allocate weitek cb for LFB");
                }

                {
                    MEM_CalloutObject *cb = MEM_GetCallout(weitek_lfb_cb);

                    assert(cb != NULL);

                    cb->Uninstall();

                    cb->Install(weitek_lfb>>12UL,MEMMASK_Combine(MEMMASK_FULL,MEMMASK_Range(weitek_lfb_pages)),weitek_memio_cb);

                    MEM_PutCallout(cb);
                }
            }
        }
        else {
            enable_weitek = false;
        }

		if (cpu_rep_max < 0) cpu_rep_max = 4;	/* compromise to help emulation speed without too much loss of accuracy */

		if(CPU_CycleMax <= 0) CPU_CycleMax = 3000;
		if(CPU_CycleUp <= 0)   CPU_CycleUp = 500;
		if(CPU_CycleDown <= 0) CPU_CycleDown = 20;

        if (enable_cmpxchg8b && CPU_ArchitectureType >= CPU_ARCHTYPE_PENTIUM) LOG_MSG("Pentium CMPXCHG8B emulation is enabled");

		if (CPU_CycleAutoAdjust) GFX_SetTitle(CPU_CyclePercUsed,-1,-1,false);
		else GFX_SetTitle(CPU_CycleMax,-1,-1,false);
		return true;
	}
	~CPU(){ /* empty */};
};
	
static CPU * test;

void CPU_ShutDown(Section* sec) {
#if (C_DYNAMIC_X86)
	CPU_Core_Dyn_X86_Cache_Close();
#endif
	delete test;
}

void CPU_OnReset(Section* sec) {
	LOG(LOG_CPU,LOG_DEBUG)("CPU reset");

	CPU_Snap_Back_To_Real_Mode();
	CPU_Snap_Back_Forget();
	CPU_SetFlags(0,~0);

	Segs.limit[cs]=0xFFFF;
	Segs.expanddown[cs]=false;
	if (CPU_ArchitectureType >= CPU_ARCHTYPE_386) {
		/* 386 and later start at F000:FFF0 with CS base set to FFFF0000 (really?) */
		SegSet16(cs,0xF000);
		reg_eip=0xFFF0;
		Segs.phys[cs]=0xFFFF0000;
	}
	else if (CPU_ArchitectureType >= CPU_ARCHTYPE_286) {
		/* 286 start at F000:FFF0 (FFFF0) */
		SegSet16(cs,0xF000);
		reg_eip=0xFFF0;
	}
	else {
		/* 8086 start at FFFF:0000 (FFFF0) */
		SegSet16(cs,0xFFFF);
		reg_eip=0x0000;
	}
}

void CPU_OnSectionPropChange(Section *x) {
	if (test != NULL) test->Change_Config(x);
}

void CPU_Init() {
	LOG(LOG_MISC,LOG_DEBUG)("Initializing CPU");

	control->GetSection("cpu")->onpropchange.push_back(&CPU_OnSectionPropChange);

	test = new CPU(control->GetSection("cpu"));
	AddExitFunction(AddExitFunctionFuncPair(CPU_ShutDown),true);
	AddVMEventFunction(VM_EVENT_RESET,AddVMEventFunctionFuncPair(CPU_OnReset));
}
//initialize static members
bool CPU::inited=false;


Bit16u CPU_FindDecoderType( CPU_Decoder *decoder )
{
	Bit16u decoder_idx;

	decoder_idx = 0xffff;


	if(0) {}
	else if( cpudecoder == &CPU_Core_Normal_Run ) decoder_idx = 0;
	else if( cpudecoder == &CPU_Core_Prefetch_Run ) decoder_idx = 1;
	else if( cpudecoder == &CPU_Core_Simple_Run ) decoder_idx = 2;
	else if( cpudecoder == &CPU_Core_Full_Run ) decoder_idx = 3;
#if C_DYNAMIC_X86
	else if( cpudecoder == &CPU_Core_Dyn_X86_Run ) decoder_idx = 4;
#endif
	else if( cpudecoder == &CPU_Core_Normal_Trap_Run ) decoder_idx = 100;
#if C_DYNAMIC_X86
	else if( cpudecoder == &CPU_Core_Dyn_X86_Trap_Run ) decoder_idx = 101;
#endif
	else if( cpudecoder == &HLT_Decode ) decoder_idx = 200;


	return decoder_idx;
}


CPU_Decoder *CPU_IndexDecoderType( Bit16u decoder_idx )
{
	CPU_Decoder *cpudecoder;


	cpudecoder = 0;
	switch( decoder_idx ) {
		case 0: cpudecoder = &CPU_Core_Normal_Run; break;
		case 1: cpudecoder = &CPU_Core_Prefetch_Run; break;
		case 2: cpudecoder = &CPU_Core_Simple_Run; break;
		case 3: cpudecoder = &CPU_Core_Full_Run; break;
#if C_DYNAMIC_X86
		case 4: cpudecoder = &CPU_Core_Dyn_X86_Run; break;
#endif
		case 100: cpudecoder = &CPU_Core_Normal_Trap_Run; break;
#if C_DYNAMIC_X86
		case 101: cpudecoder = &CPU_Core_Dyn_X86_Trap_Run; break;
#endif
		case 200: cpudecoder = &HLT_Decode; break;
	}


	return cpudecoder;
}

Bitu vm86_fake_io_seg = 0xF000;	/* unused area in BIOS for IO instruction */
Bitu vm86_fake_io_off = 0x0700;
Bitu vm86_fake_io_offs[3*2]={0};	/* offsets from base off because of dynamic core cache */

void init_vm86_fake_io() {
	Bitu phys = (vm86_fake_io_seg << 4) + vm86_fake_io_off;
	Bitu wo = 0;

	if (vm86_fake_io_offs[0] != 0)
		return;

	/* read */
	vm86_fake_io_offs[0] = vm86_fake_io_off + wo;
	phys_writeb(phys+wo+0x00,(Bit8u)0xEC);	/* IN AL,DX */
	phys_writeb(phys+wo+0x01,(Bit8u)0xCB);	/* RETF */
	wo += 2;

	vm86_fake_io_offs[1] = vm86_fake_io_off + wo;
	phys_writeb(phys+wo+0x00,(Bit8u)0xED);	/* IN AX,DX */
	phys_writeb(phys+wo+0x01,(Bit8u)0xCB);	/* RETF */
	wo += 2;

	vm86_fake_io_offs[2] = vm86_fake_io_off + wo;
	phys_writeb(phys+wo+0x00,(Bit8u)0x66);	/* IN EAX,DX */
	phys_writeb(phys+wo+0x01,(Bit8u)0xED);
	phys_writeb(phys+wo+0x02,(Bit8u)0xCB);	/* RETF */
	wo += 3;

	/* write */
	vm86_fake_io_offs[3] = vm86_fake_io_off + wo;
	phys_writeb(phys+wo+0x00,(Bit8u)0xEE);	/* OUT DX,AL */
	phys_writeb(phys+wo+0x01,(Bit8u)0xCB);	/* RETF */
	wo += 2;

	vm86_fake_io_offs[4] = vm86_fake_io_off + wo;
	phys_writeb(phys+wo+0x00,(Bit8u)0xEF);	/* OUT DX,AX */
	phys_writeb(phys+wo+0x01,(Bit8u)0xCB);	/* RETF */
	wo += 2;

	vm86_fake_io_offs[5] = vm86_fake_io_off + wo;
	phys_writeb(phys+wo+0x00,(Bit8u)0x66);	/* OUT DX,EAX */
	phys_writeb(phys+wo+0x01,(Bit8u)0xEF);
	phys_writeb(phys+wo+0x02,(Bit8u)0xCB);	/* RETF */
	wo += 3;
}

Bitu CPU_ForceV86FakeIO_In(Bitu port,Bitu len) {
	Bitu old_ax,old_dx,ret;

	/* save EAX:EDX and setup DX for IN instruction */
	old_ax = reg_eax;
	old_dx = reg_edx;

	reg_edx = port;

	/* make the CPU execute that instruction */
	CALLBACK_RunRealFar(vm86_fake_io_seg,vm86_fake_io_offs[(len==4?2:(len-1))+0]);

	/* take whatever the CPU or OS v86 trap left in EAX and return it */
	ret = reg_eax;
	if (len == 1) ret &= 0xFF;
	else if (len == 2) ret &= 0xFFFF;

	/* then restore EAX:EDX */
	reg_eax = old_ax;
	reg_edx = old_dx;

	return ret;
}

void CPU_ForceV86FakeIO_Out(Bitu port,Bitu val,Bitu len) {
	Bitu old_ax,old_dx;

	/* save EAX:EDX and setup DX/AX for OUT instruction */
	old_ax = reg_eax;
	old_dx = reg_edx;

	reg_edx = port;
	reg_eax = val;

	/* make the CPU execute that instruction */
	CALLBACK_RunRealFar(vm86_fake_io_seg,vm86_fake_io_offs[(len==4?2:(len-1))+3]);

	/* then restore EAX:EDX */
	reg_eax = old_ax;
	reg_edx = old_dx;
}

/* pentium machine-specific registers */
bool CPU_RDMSR() {
	if (!enable_msr) return false;

	switch (reg_ecx) {
		default:
			LOG(LOG_CPU,LOG_NORMAL)("RDMSR: Unknown register 0x%08lx",(unsigned long)reg_ecx);
			break;
	}

	if (ignore_undefined_msr) {
		/* wing it and hope nobody notices */
		reg_edx = reg_eax = 0;
		return true;
	}

	return false; /* unknown reg, signal illegal opcode */
}

bool CPU_WRMSR() {
	if (!enable_msr) return false;

	switch (reg_ecx) {
		default:
			LOG(LOG_CPU,LOG_NORMAL)("WRMSR: Unknown register 0x%08lx (write 0x%08lx:0x%08lx)",(unsigned long)reg_ecx,(unsigned long)reg_edx,(unsigned long)reg_eax);
			break;
	}

	if (ignore_undefined_msr) return true; /* ignore */
	return false; /* unknown reg, signal illegal opcode */
}

/* NTS: Hopefully by implementing this Windows ME can stop randomly crashing when cputype=pentium */
void CPU_CMPXCHG8B(PhysPt eaa) {
    uint32_t hi,lo;

    /* NTS: We assume that, if reading doesn't cause a page fault, writing won't either */
    hi = (uint32_t)mem_readd(eaa+(PhysPt)4);
    lo = (uint32_t)mem_readd(eaa);

    LOG_MSG("Experimental CMPXCHG8B implementation executed. EDX:EAX=0x%08lx%08lx ECX:EBX=0x%08lx%08lx EA=0x%08lx MEM64=0x%08lx%08lx",
        (unsigned long)reg_edx,
        (unsigned long)reg_eax,
        (unsigned long)reg_ecx,
        (unsigned long)reg_ebx,
        (unsigned long)eaa,
        (unsigned long)hi,
        (unsigned long)lo);

    /* Compare EDX:EAX with 64-bit DWORD at memaddr 'eaa'.
     * if they match, ZF=1 and write ECX:EBX to memaddr 'eaa'.
     * else, ZF=0 and load memaddr 'eaa' into EDX:EAX */
    if (reg_edx == hi && reg_eax == lo) {
        mem_writed(eaa+(PhysPt)4,reg_ecx);
        mem_writed(eaa,          reg_ebx);
		SETFLAGBIT(ZF,true);
    }
    else {
		SETFLAGBIT(ZF,false);
        reg_edx = hi;
        reg_eax = lo;
    }
}

