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

#define CPU_CORE CPU_ARCHTYPE_8086
#define CPU_Core_Prefetch_Trap_Run CPU_Core8086_Prefetch_Trap_Run

#define DoString DoString_Prefetch8086

extern bool ignore_opcode_63;

#if C_DEBUG
#include "debug.h"
#endif

static uint16_t last_ea86_offset;

/* NTS: we special case writes to seg:ffff to emulate 8086 behavior where word read/write wraps around the 64KB segment */
#if (!C_CORE_INLINE)

#define LoadMb(off) mem_readb(off)

static inline uint16_t LoadMw(Bitu off) {
	if (last_ea86_offset == 0xffff)
		return (mem_readb(off) | (mem_readb(off-0xffff) << 8));

	return mem_readw(off);	
}

#define LoadMd(off) mem_readd(off)

#define SaveMb(off,val)	mem_writeb(off,val)

static void SaveMw(Bitu off,Bitu val) {
	if (last_ea86_offset == 0xffff) {
		mem_writeb(off,val);
		mem_writeb(off-0xffff,val>>8);
	}
	else {
		mem_writew(off,val);
	}
}

#define SaveMd(off,val)	mem_writed(off,val)

#else 

#include "paging.h"

#define LoadMb(off) mem_readb_inline(off)

static inline uint16_t LoadMw(Bitu off) {
	if (last_ea86_offset == 0xffff)
		return (mem_readb_inline((PhysPt)off) | (mem_readb_inline((PhysPt)(off-0xffff)) << 8));

	return mem_readw_inline((PhysPt)off);
}

#define LoadMd(off) mem_readd_inline(off)

#define SaveMb(off,val)	mem_writeb_inline(off,val)

static void SaveMw(Bitu off,Bitu val) {
	if (last_ea86_offset == 0xffff) {
		mem_writeb_inline((PhysPt)off,(uint8_t)val);
		mem_writeb_inline((PhysPt)(off-0xffff),(uint8_t)(val>>8));
	}
	else {
		mem_writew_inline((PhysPt)off,(uint16_t)val);
	}
}

#define SaveMd(off,val)	mem_writed_inline(off,val)

#endif

extern Bitu cycle_count;

#if C_FPU
#define CPU_FPU	1u						//Enable FPU escape instructions
#endif

#define CPU_PIC_CHECK 1u
#define CPU_TRAP_CHECK 1u

Bits CPU_Core_Prefetch_Trap_Run(void);
#define CPU_TRAP_DECODER	CPU_Core_Prefetch_Trap_Run

#define OPCODE_NONE			0x000u
#define OPCODE_0F			0x100u

#define OPCODE_SIZE			0u          //DISABLED

#define PREFIX_ADDR			0u			//DISABLED

#define PREFIX_REP			0x2u

#define TEST_PREFIX_ADDR	(0u)
#define TEST_PREFIX_REP		(core.prefixes & PREFIX_REP)

#define DO_PREFIX_SEG(_SEG)					\
    if (GETFLAG(IF) && CPU_Cycles <= 0) goto prefix_out; \
	BaseDS=SegBase(_SEG);					\
	BaseSS=SegBase(_SEG);					\
	core.base_val_ds=_SEG;					\
	goto restart_opcode;

// it's the core's job not to decode 0x66-0x67 when compiled for 8086
#define DO_PREFIX_ADDR()								\
    abort();

#define DO_PREFIX_REP(_ZERO)				\
    if (GETFLAG(IF) && CPU_Cycles <= 0) goto prefix_out; \
	core.prefixes|=PREFIX_REP;				\
	core.rep_zero=_ZERO;					\
	goto restart_opcode;

typedef PhysPt (*GetEAHandler)(void);

static const uint32_t AddrMaskTable[2]={0x0000ffffu,0x0000ffffu};

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

#define SAVEIP_PREFIX		reg_eip=GETIP-1;

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
#define prefetch_unit       (2ul)

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

static uint16_t Fetchw() {
	return Fetch<uint16_t>();
}

static uint32_t Fetchd() {
	return Fetch<uint32_t>();
}

#define Push_16 CPU_Push16
#define Pop_16 CPU_Pop16

#include "instructions.h"
#include "core_normal/support.h"
#include "core_normal/string.h"


#define EALookupTable (core.ea_table)

void CPU_Core8086_Prefetch_reset(void) {
    pq_valid=false;
    prefetch_init(0);
#ifdef PREFETCH_DEBUG
    pq_next_dbg=0;
#endif
}

Bits CPU_Core8086_Prefetch_Run(void) {
	bool invalidate_pq=false;

    if (CPU_Cycles <= 0)
	    return CBRET_NONE;

    pq_limit = (max(CPU_PrefetchQueueSize,(unsigned int)(4ul + prefetch_unit)) + prefetch_unit - 1ul) & (~(prefetch_unit-1ul));
    pq_reload = min(pq_limit,(Bitu)8u);

	while (CPU_Cycles-->0) {
		if (invalidate_pq) {
			pq_valid=false;
			invalidate_pq=false;
		}
		LOADIP;
		core.prefixes=0;
		core.opcode_index=0;
		last_ea86_offset=0;
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
			/* there is no invalid opcode instruction. */
			reg_eip += 2;
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

Bits CPU_Core_Prefetch_Trap_Run(void) {
	Bits oldCycles = CPU_Cycles;
	CPU_Cycles = 1;
	cpu.trap_skip = false;

	Bits ret=CPU_Core_Prefetch_Run();
	if (!cpu.trap_skip) CPU_DebugException(DBINT_STEP,reg_eip);
	CPU_Cycles = oldCycles-1;
	cpudecoder = &CPU_Core_Prefetch_Run;

	return ret;
}

