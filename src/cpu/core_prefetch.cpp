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
#include <string.h>

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

using namespace std;

#include <algorithm>

#define CPU_CORE CPU_ARCHTYPE_386

#define DoString DoString_Prefetch

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

extern Bitu cycle_count;

#if C_FPU
#define CPU_FPU	1u						//Enable FPU escape instructions
#endif

#define CPU_PIC_CHECK 1u
#define CPU_TRAP_CHECK 1u

#define CPU_TRAP_DECODER	CPU_Core_Prefetch_Trap_Run

#define OPCODE_NONE			0x000u
#define OPCODE_0F			0x100u
#define OPCODE_SIZE			0x200u

#define PREFIX_ADDR			0x1u
#define PREFIX_REP			0x2u

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
	core.ea_table=&EATable[(core.prefixes&1u) * 256u];	\
	goto restart_opcode;

#define DO_PREFIX_REP(_ZERO)				\
	core.prefixes|=PREFIX_REP;				\
	core.rep_zero=_ZERO;					\
	goto restart_opcode;

typedef PhysPt (*GetEAHandler)(void);

static const Bit32u AddrMaskTable[2]={0x0000ffffu,0xffffffffu};

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

//#define PREFETCH_DEBUG

#define MAX_PQ_SIZE 32
static uint8_t prefetch_buffer[MAX_PQ_SIZE];
static bool pq_valid=false;
static Bitu pq_start;
static Bitu pq_fill;
static Bitu pq_limit;
static Bitu pq_reload;
#ifdef PREFETCH_DEBUG
static double pq_next_dbg=0;
static unsigned int pq_hit=0,pq_miss=0;
#endif

/* MUST BE POWER OF 2 */
#define prefetch_unit       (4ul)

#include "core_prefetch_buf.h"

static INLINE void FetchDiscardb() {
	FetchDiscard<uint8_t>();
}

static INLINE uint8_t FetchPeekb() {
	return FetchPeek<uint8_t>();
}

static uint8_t Fetchb() {
	return Fetch<uint8_t>();
}

static Bit16u Fetchw() {
	return Fetch<uint16_t>();
}

static Bit32u Fetchd() {
	return Fetch<uint32_t>();
}

bool CPU_RDMSR();
bool CPU_WRMSR();

#define Push_16 CPU_Push16
#define Push_32 CPU_Push32
#define Pop_16 CPU_Pop16
#define Pop_32 CPU_Pop32

#include "instructions.h"
#include "core_normal/support.h"
#include "core_normal/string.h"


#define EALookupTable (core.ea_table)

void CPU_Core_Prefetch_reset(void) {
    pq_valid=false;
    prefetch_init(0);
#ifdef PREFETCH_DEBUG
    pq_next_dbg=0;
#endif
}

Bits CPU_Core_Prefetch_Run(void) {
	bool invalidate_pq=false;

    if (CPU_Cycles <= 0)
	    return CBRET_NONE;

    // FIXME: This makes 8086 4-byte prefetch queue impossible to emulate.
    //        The best way to accomplish this is to have an alternate version
    //        of this prefetch queue for 286 or lower that fetches in 16-bit
    //        WORDs instead of 32-bit WORDs.
    pq_limit = (max(CPU_PrefetchQueueSize,(unsigned int)(4ul + prefetch_unit)) + prefetch_unit - 1ul) & (~(prefetch_unit-1ul));
    pq_reload = min(pq_limit,(Bitu)8u);

	while (CPU_Cycles-->0) {
		if (invalidate_pq) {
			pq_valid=false;
			invalidate_pq=false;
		}
		LOADIP;
		core.opcode_index=cpu.code.big*(Bitu)0x200u;
		core.prefixes=cpu.code.big;
		core.ea_table=&EATable[cpu.code.big*256u];
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
		cycle_count++;
#endif
restart_opcode:
		uint8_t next_opcode=Fetchb();
		invalidate_pq=false;
		if (core.opcode_index&OPCODE_0F) invalidate_pq=true;
		else switch (next_opcode) {
			case 0x70:	case 0x71:	case 0x72:	case 0x73:
			case 0x74:	case 0x75:	case 0x76:	case 0x77:
			case 0x78:	case 0x79:	case 0x7a:	case 0x7b:
			case 0x7c:	case 0x7d:	case 0x7e:	case 0x7f:	// jcc
			case 0x9a:	// call
			case 0xc2:	case 0xc3:	// retn
			case 0xc8:	// enter
			case 0xc9:	// leave
			case 0xca:	case 0xcb:	// retf
			case 0xcc:	// int3
			case 0xcd:	// int
			case 0xce:	// into
			case 0xcf:	// iret
			case 0xe0:	// loopnz
			case 0xe1:	// loopz
			case 0xe2:	// loop
			case 0xe3:	// jcxz
			case 0xe8:	// call
			case 0xe9:	case 0xea:	case 0xeb:	// jmp
			case 0xff:
				invalidate_pq=true;
				break;
			default:
				break;
		}
		switch (core.opcode_index+next_opcode) {
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
			invalidate_pq=true;
			continue;
		gp_fault:
			CPU_Exception(EXCEPTION_GP,0);
			continue;
		}
		SAVEIP;
	}

#ifdef PREFETCH_DEBUG
    if (PIC_FullIndex() > pq_next_dbg) {
        LOG_MSG("Prefetch core debug: prefetch cache hit=%u miss=%u",pq_hit,pq_miss);
        pq_next_dbg += 500.0;
    }
#endif

	FillFlags();
	return CBRET_NONE;
decode_end:
	SAVEIP;
	FillFlags();
	return CBRET_NONE;
}

Bits CPU_Core_Prefetch_Trap_Run(void) {
	Bits oldCycles = CPU_Cycles;
	CPU_Cycles = 1;
	cpu.trap_skip = false;

	Bits ret=CPU_Core_Prefetch_Run();
	if (!cpu.trap_skip) CPU_HW_Interrupt(1);
	CPU_Cycles = oldCycles-1;
	cpudecoder = &CPU_Core_Prefetch_Run;

	return ret;
}



void CPU_Core_Prefetch_Init(void) {

}

