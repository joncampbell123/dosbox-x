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
#include "mmx.h"

#define CPU_CORE CPU_ARCHTYPE_386

extern bool ignore_opcode_63;

#if C_DEBUG
#include "debug.h"
#endif

#include "paging.h"
#define SegBase(c)  SegPhys(c)
#define LoadMb(off) mem_readb(off)
#define LoadMw(off) mem_readw(off)
#define LoadMd(off) mem_readd(off)
#define LoadMq(off) ((Bit64u)((Bit64u)mem_readd(off+4)<<32 | (Bit64u)mem_readd(off)))

#define SaveMb(off,val) mem_writeb(off,val)
#define SaveMw(off,val) mem_writew(off,val)
#define SaveMd(off,val) mem_writed(off,val)
#define SaveMq(off,val) {mem_writed(off,val&0xffffffff);mem_writed(off+4,(val>>32)&0xffffffff);}

extern Bitu cycle_count;

#if C_FPU
#define CPU_FPU 1u                       //Enable FPU escape instructions
#endif

#define CPU_PIC_CHECK 1u
#define CPU_TRAP_CHECK 1u

#define CPU_TRAP_DECODER	CPU_Core_Simple_Trap_Run

#define OPCODE_NONE         0x000u
#define OPCODE_0F           0x100u
#define OPCODE_SIZE         0x200u

#define PREFIX_ADDR         0x1u
#define PREFIX_REP          0x2u

#define TEST_PREFIX_ADDR    (core.prefixes & PREFIX_ADDR)
#define TEST_PREFIX_REP     (core.prefixes & PREFIX_REP)

#define DO_PREFIX_SEG(_SEG)                 \
    BaseDS=SegBase(_SEG);                   \
    BaseSS=SegBase(_SEG);                   \
    core.base_val_ds=_SEG;                  \
    goto restart_opcode;

#define DO_PREFIX_ADDR()                                \
    core.prefixes=(core.prefixes & ~PREFIX_ADDR) |      \
    (cpu.code.big ^ PREFIX_ADDR);                       \
    core.ea_table=&EATable[(core.prefixes&1u) * 256u];    \
    goto restart_opcode;

#define DO_PREFIX_REP(_ZERO)                \
    core.prefixes|=PREFIX_REP;              \
    core.rep_zero=_ZERO;                    \
    goto restart_opcode;

typedef PhysPt (*GetEAHandler)(void);

static const Bit32u AddrMaskTable[2]={0x0000ffffu,0xffffffffu};

static struct {
    Bitu                    opcode_index;
#if defined (_MSC_VER)
    volatile HostPt         cseip;
#else
    HostPt                  cseip;
#endif
    PhysPt                  base_ds,base_ss;
    SegNames                base_val_ds;
    bool                    rep_zero;
    Bitu                    prefixes;
    GetEAHandler*           ea_table;
} core;

#define GETIP       ((Bit32u) ((uintptr_t)core.cseip - (uintptr_t)SegBase(cs) - (uintptr_t)MemBase))
#define SAVEIP      reg_eip=GETIP;
#define LOADIP      core.cseip=((HostPt) ((uintptr_t)MemBase + (uintptr_t)SegBase(cs) + (uintptr_t)reg_eip));

#define SegBase(c)  SegPhys(c)
#define BaseDS      core.base_ds
#define BaseSS      core.base_ss

static INLINE void FetchDiscardb() {
	core.cseip+=1;
}

static INLINE Bit8u FetchPeekb() {
    Bit8u temp=host_readb(core.cseip);
    return temp;
}

static INLINE Bit8u Fetchb() {
    Bit8u temp=host_readb(core.cseip);
    core.cseip+=1;
    return temp;
}

static INLINE Bit16u Fetchw() {
    Bit16u temp=host_readw(core.cseip);
    core.cseip+=2;
    return temp;
}
static INLINE Bit32u Fetchd() {
    Bit32u temp=host_readd(core.cseip);
    core.cseip+=4;
    return temp;
}

#define Push_16 CPU_Push16
#define Push_32 CPU_Push32
#define Pop_16 CPU_Pop16
#define Pop_32 CPU_Pop32

bool CPU_RDMSR();
bool CPU_WRMSR();

#include "instructions.h"
#include "core_normal/support.h"
#include "core_normal/string.h"


#define EALookupTable (core.ea_table)

/* NTS: This code fetches opcodes directly from MemBase plus an offset.
 *      Because of this, the simple core cannot be used to execute directly
 *      from ROM provided by an adapter since that requires memory I/O callbacks. */
Bits CPU_Core_Simple_Run(void) {
    HostPt safety_limit;

    /* simple core is incompatible with paging */
    if (paging.enabled)
        return CPU_Core_Normal_Run();

    safety_limit = (HostPt)((size_t)MemBase + ((size_t)MEM_TotalPages() * (size_t)4096) - (size_t)16384); /* safety margin */

    LOADIP;
    if (core.cseip >= safety_limit) /* beyond the safety limit, use the normal core */
        return CPU_Core_Normal_Run();

    while (CPU_Cycles-->0) {
        LOADIP;

        /* Simple core optimizes for non-paged linear memory access and can break (segfault) if beyond end of memory */
        if (core.cseip >= safety_limit) break;

        core.opcode_index=cpu.code.big*0x200u;
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
decode_end:
    SAVEIP;
    FillFlags();
    return CBRET_NONE;
}

Bits CPU_Core_Simple_Trap_Run(void) {
    Bits oldCycles = CPU_Cycles;
    CPU_Cycles = 1;
    cpu.trap_skip = false;

    Bits ret=CPU_Core_Simple_Run();
    if (!cpu.trap_skip) CPU_HW_Interrupt(1);
    CPU_Cycles = oldCycles-1;
    cpudecoder = &CPU_Core_Simple_Run;

    return ret;
}

void CPU_Core_Simple_Init(void) {

}
