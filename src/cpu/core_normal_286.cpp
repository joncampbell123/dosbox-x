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

#include <stdio.h>
#include <stdlib.h>

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

bool CPU_RDMSR();
bool CPU_WRMSR();

#define CPU_CORE CPU_ARCHTYPE_286
#define CPU_Core_Normal_Trap_Run CPU_Core286_Normal_Trap_Run

#define DoString DoString_Normal286

extern bool ignore_opcode_63;

#if C_DEBUG
extern bool mustCompleteInstruction;
# include "debug.h"
#else
# define mustCompleteInstruction (0)
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

extern Bitu cycle_count;

#if C_FPU
#define CPU_FPU	1u						//Enable FPU escape instructions
#endif

#define CPU_PIC_CHECK 1u
#define CPU_TRAP_CHECK 1u

#define OPCODE_NONE			0x000u
#define OPCODE_0F			0x100u

#define OPCODE_SIZE			0u			//DISABLED

#define PREFIX_ADDR			0u			//DISABLED

#define PREFIX_REP			0x2u

#define TEST_PREFIX_ADDR	(0u)				//DISABLED
#define TEST_PREFIX_REP		(core.prefixes & PREFIX_REP)

#define DO_PREFIX_SEG(_SEG)					\
    if (GETFLAG(IF) && CPU_Cycles <= 0 && !mustCompleteInstruction) goto prefix_out; \
	BaseDS=SegBase(_SEG);					\
	BaseSS=SegBase(_SEG);					\
	core.base_val_ds=_SEG;					\
	goto restart_opcode;

// it's the core's job not to decode 0x66-0x67 when compiled for 286
#define DO_PREFIX_ADDR()								\
	abort();									

#define DO_PREFIX_REP(_ZERO)				\
    if (GETFLAG(IF) && CPU_Cycles <= 0 && !mustCompleteInstruction) goto prefix_out; \
	core.prefixes|=PREFIX_REP;				\
	core.rep_zero=_ZERO;					\
	goto restart_opcode;

typedef PhysPt (*GetEAHandler)(void);

static const Bit32u AddrMaskTable[2]={0x0000ffffu,0x0000ffffu};

static struct {
	Bitu opcode_index;
	PhysPt cseip;
	PhysPt base_ds,base_ss;
	SegNames base_val_ds;
	bool rep_zero;
	Bitu prefixes;
	GetEAHandler * ea_table;
} core;

/* FIXME: Someone at Microsoft tell how subtracting PhysPt - PhysPt = __int64, or PhysPt + PhysPt = __int64 */
#define GETIP		((PhysPt)(core.cseip-SegBase(cs)))
#define SAVEIP		reg_eip=GETIP;
#define LOADIP		core.cseip=((PhysPt)(SegBase(cs)+reg_eip));

#define SAVEIP_PREFIX		reg_eip=GETIP-1;

#define SegBase(c)	SegPhys(c)
#define BaseDS		core.base_ds
#define BaseSS		core.base_ss

static INLINE void FetchDiscardb() {
	core.cseip+=1;
}

static INLINE Bit8u FetchPeekb() {
	Bit8u temp=LoadMb(core.cseip);
	return temp;
}

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
#define Pop_16 CPU_Pop16

#include "instructions.h"
#include "core_normal/support.h"
#include "core_normal/string.h"


#define EALookupTable (core.ea_table)

Bits CPU_Core286_Normal_Run(void) {
    if (CPU_Cycles <= 0)
	    return CBRET_NONE;

	while (CPU_Cycles-->0) {
		LOADIP;
		core.prefixes=0;
		core.opcode_index=0;
		core.ea_table=&EATable[0];
		BaseDS=SegBase(ds);
		BaseSS=SegBase(ss);
		core.base_val_ds=ds;
#if C_DEBUG
#if C_HEAVY_DEBUG
		if (DEBUG_HeavyIsBreakpoint()) {
			FillFlags();
			return (Bits)debugCallback;
		}
#endif
#endif
		cycle_count++;
restart_opcode:
		switch (core.opcode_index+Fetchb()) {
		#include "core_normal/prefix_none.h"
		#include "core_normal/prefix_0f.h"
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
			CPU_Exception(EXCEPTION_GP,0);
			continue;
		}
		SAVEIP;
	}
	FillFlags();
	return CBRET_NONE;
/* 8086/286 multiple prefix interrupt bug emulation.
 * If an instruction is interrupted, only the last prefix is restarted.
 * See also [https://www.pcjs.org/pubs/pc/reference/intel/8086/] and [https://www.youtube.com/watch?v=6FC-tcwMBnU] */ 
prefix_out:
	SAVEIP_PREFIX;
	FillFlags();
	return CBRET_NONE;
decode_end:
	SAVEIP;
	FillFlags();
	return CBRET_NONE;
}

Bits CPU_Core286_Normal_Trap_Run(void) {
	Bits oldCycles = CPU_Cycles;
	CPU_Cycles = 1;
	cpu.trap_skip = false;

	Bits ret=CPU_Core286_Normal_Run();
	if (!cpu.trap_skip) CPU_HW_Interrupt(1);
	CPU_Cycles = oldCycles-1;
	cpudecoder = &CPU_Core286_Normal_Run;

	return ret;
}



void CPU_Core286_Normal_Init(void) {

}

