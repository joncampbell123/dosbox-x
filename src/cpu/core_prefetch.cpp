/*
 *  Copyright (C) 2002-2013  The DOSBox Team
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


#include <stdio.h>

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


#define MAX_PQ_SIZE 32
static Bit8u prefetch_buffer[MAX_PQ_SIZE];
static bool pq_valid=false;
static Bitu pq_start;

static Bit8u Fetchb() {
	Bit8u temp;
	if (pq_valid && (core.cseip>=pq_start) && (core.cseip<pq_start+CPU_PrefetchQueueSize)) {
		temp=prefetch_buffer[core.cseip-pq_start];
		if ((core.cseip+1>=pq_start+CPU_PrefetchQueueSize-4) &&
			(core.cseip+1<pq_start+CPU_PrefetchQueueSize)) {
			Bitu remaining_bytes=pq_start+CPU_PrefetchQueueSize-(core.cseip+1);
			for (Bitu i=0; i<remaining_bytes; i++) prefetch_buffer[i]=prefetch_buffer[core.cseip+1-pq_start+i];
			for (Bitu i=remaining_bytes; i<CPU_PrefetchQueueSize; i++) prefetch_buffer[i]=LoadMb(core.cseip+1+i);
			pq_start=core.cseip+1;
			pq_valid=true;
		}
	} else {
		for (Bitu i=0; i<CPU_PrefetchQueueSize; i++) prefetch_buffer[i]=LoadMb(core.cseip+i);
		pq_start=core.cseip;
		pq_valid=true;
		temp=prefetch_buffer[0];
	}
/*	if (temp!=LoadMb(core.cseip)) {
		LOG_MSG("prefetch queue content!=memory at %x:%x",SegValue(cs),reg_eip);
	} */
	core.cseip+=1;
	return temp;
}

static Bit16u Fetchw() {
	Bit16u temp;
	if (pq_valid && (core.cseip>=pq_start) && (core.cseip+2<pq_start+CPU_PrefetchQueueSize)) {
		temp=prefetch_buffer[core.cseip-pq_start]|
			(prefetch_buffer[core.cseip-pq_start+1]<<8);
		if ((core.cseip+2>=pq_start+CPU_PrefetchQueueSize-4) &&
			(core.cseip+2<pq_start+CPU_PrefetchQueueSize)) {
			Bitu remaining_bytes=pq_start+CPU_PrefetchQueueSize-(core.cseip+2);
			for (Bitu i=0; i<remaining_bytes; i++) prefetch_buffer[i]=prefetch_buffer[core.cseip+2-pq_start+i];
			for (Bitu i=remaining_bytes; i<CPU_PrefetchQueueSize; i++) prefetch_buffer[i]=LoadMb(core.cseip+2+i);
			pq_start=core.cseip+2;
			pq_valid=true;
		}
	} else {
		for (Bitu i=0; i<CPU_PrefetchQueueSize; i++) prefetch_buffer[i]=LoadMb(core.cseip+i);
		pq_start=core.cseip;
		pq_valid=true;
		temp=prefetch_buffer[0] | (prefetch_buffer[1]<<8);
	}
/*	if (temp!=LoadMw(core.cseip)) {
		LOG_MSG("prefetch queue content!=memory at %x:%x",SegValue(cs),reg_eip);
	} */
	core.cseip+=2;
	return temp;
}

static Bit32u Fetchd() {
	Bit32u temp;
	if (pq_valid && (core.cseip>=pq_start) && (core.cseip+4<pq_start+CPU_PrefetchQueueSize)) {
		temp=prefetch_buffer[core.cseip-pq_start]|
			(prefetch_buffer[core.cseip-pq_start+1]<<8)|
			(prefetch_buffer[core.cseip-pq_start+2]<<16)|
			(prefetch_buffer[core.cseip-pq_start+3]<<24);
		if ((core.cseip+4>=pq_start+CPU_PrefetchQueueSize-4) &&
			(core.cseip+4<pq_start+CPU_PrefetchQueueSize)) {
			Bitu remaining_bytes=pq_start+CPU_PrefetchQueueSize-(core.cseip+4);
			for (Bitu i=0; i<remaining_bytes; i++) prefetch_buffer[i]=prefetch_buffer[core.cseip+4-pq_start+i];
			for (Bitu i=remaining_bytes; i<CPU_PrefetchQueueSize; i++) prefetch_buffer[i]=LoadMb(core.cseip+4+i);
			pq_start=core.cseip+4;
			pq_valid=true;
		}
	} else {
		for (Bitu i=0; i<CPU_PrefetchQueueSize; i++) prefetch_buffer[i]=LoadMb(core.cseip+i);
		pq_start=core.cseip;
		pq_valid=true;
		temp=prefetch_buffer[0] | (prefetch_buffer[1]<<8) |
			(prefetch_buffer[2]<<16) | (prefetch_buffer[3]<<24);
	}
/*	if (temp!=LoadMd(core.cseip)) {
		LOG_MSG("prefetch queue content!=memory at %x:%x",SegValue(cs),reg_eip);
	} */
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

Bits CPU_Core_Prefetch_Run(void) {
	bool invalidate_pq=false;
	while (CPU_Cycles-->0) {
		if (invalidate_pq) {
			pq_valid=false;
			invalidate_pq=false;
		}
		LOADIP;
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
		cycle_count++;
#endif
restart_opcode:
		Bit8u next_opcode=Fetchb();
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
				Bitu len=(GETIP-reg_eip);
				LOADIP;
				if (len>16) len=16;
				char tempcode[16*2+1];char * writecode=tempcode;
				for (;len>0;len--) {
					sprintf(writecode,"%02X",mem_readb(core.cseip++));
					writecode+=2;
				}
				LOG(LOG_CPU,LOG_NORMAL)("Illegal/Unhandled opcode %s",tempcode);
			}
#endif
			CPU_Exception(6,0);
			invalidate_pq=true;
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

