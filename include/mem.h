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

#ifndef DOSBOX_MEM_H
#define DOSBOX_MEM_H

#ifndef DOSBOX_DOSBOX_H
#include "dosbox.h"
#endif

#include "byteorder.h"

#define MEM_PAGESIZE        (4096U)

typedef Bit8u const *       ConstHostPt;        /* host (virtual) memory address aka ptr */

typedef Bit8u *             HostPt;             /* host (virtual) memory address aka ptr */

typedef Bit32u              PhysPt;      /* guest physical memory pointer */
typedef Bit32u              LinearPt;    /* guest linear memory address */
typedef Bit32u              RealPt;      /* guest real-mode memory address (16:16 -> seg:offset) */
typedef Bit16u              SegmentVal;  /* guest segment value */

typedef Bit32s              MemHandle;

extern HostPt               MemBase;

HostPt                      GetMemBase(void);
bool                        MEM_A20_Enabled(void);
void                        MEM_A20_Enable(bool enabled);

/* Memory management / EMS mapping */
Bitu                        MEM_FreeTotal(void);           //Free 4 kb pages
Bitu                        MEM_FreeLargest(void);         //Largest free 4 kb pages block
Bitu                        MEM_TotalPages(void);          //Total amount of 4 kb pages
Bitu                        MEM_AllocatedPages(MemHandle handle); // amount of allocated pages of handle
MemHandle                   MEM_AllocatePages(Bitu pages,bool sequence);
MemHandle                   MEM_AllocatePages_A20_friendly(Bitu pages,bool sequence);
MemHandle                   MEM_GetNextFreePage(void);
void                        MEM_ReleasePages(MemHandle handle);
bool                        MEM_ReAllocatePages(MemHandle & handle,Bitu pages,bool sequence);

MemHandle                   MEM_NextHandle(MemHandle handle);
MemHandle                   MEM_NextHandleAt(MemHandle handle,Bitu where);

/* 
    The folowing six functions are used everywhere in the end so these should be changed for
    Working on big or little endian machines 
*/

#if !defined(C_UNALIGNED_MEMORY)
/* meaning: we're probably being compiled for a processor that doesn't like unaligned WORD access,
            on such processors typecasting memory as uint16_t and higher can cause a fault if the
        address is not aligned to that datatype when we read/write through it. */

static INLINE Bit8u host_readb(ConstHostPt const off) {
    return *off;
}
static INLINE Bit16u host_readw(ConstHostPt const off) {
    return (Bit16u)host_readb(off) + ((Bit16u)host_readb(off+(uintptr_t)1U) << (Bit16u)8U);
}
static INLINE Bit32u host_readd(ConstHostPt const off) {
    return (Bit32u)host_readw(off) + ((Bit32u)host_readw(off+(uintptr_t)2U) << (Bit32u)16U);
}
static INLINE Bit64u host_readq(ConstHostPt const off) {
    return (Bit64u)host_readd(off) + ((Bit64u)host_readd(off+(uintptr_t)4U) << (Bit64u)32U);
}

static INLINE void host_writeb(HostPt const off,const Bit8u val) {
    *off = val;
}
static INLINE void host_writew(HostPt const off,const Bit16u val) {
    host_writeb(off,   (Bit8u)(val));
    host_writeb(off+1U,(Bit8u)(val >> (Bit16u)8U));
}
static INLINE void host_writed(HostPt const off,const Bit32u val) {
    host_writew(off,   (Bit16u)(val));
    host_writew(off+2U,(Bit16u)(val >> (Bit32u)16U));
}
static INLINE void host_writeq(HostPt const off,const Bit64u val) {
    host_writed(off,   (Bit32u)(val));
    host_writed(off+4U,(Bit32u)(val >> (Bit64u)32U));
}

#else

static INLINE Bit8u host_readb(ConstHostPt const off) {
    return *(const Bit8u *)off;
}
static INLINE Bit16u host_readw(ConstHostPt const off) {
    return le16toh((*(const Bit16u *)off)); // BSD endian.h
}
static INLINE Bit32u host_readd(ConstHostPt const off) {
    return le32toh((*(const Bit32u *)off)); // BSD endian.h
}
static INLINE Bit64u host_readq(ConstHostPt const off) {
    return le64toh((*(const Bit64u *)off)); // BSD endian.h
}

static INLINE void host_writeb(HostPt const off,const Bit8u val) {
    *(Bit8u *)(off) = val;
}
static INLINE void host_writew(HostPt const off,const Bit16u val) {
    *(Bit16u *)(off) = htole16(val);
}
static INLINE void host_writed(HostPt const off,const Bit32u val) {
    *(Bit32u *)(off) = htole32(val);
}
static INLINE void host_writeq(HostPt const off,const Bit64u val) {
    *(Bit64u *)(off) = htole64(val);
}

#endif


static INLINE void var_write(Bit8u * const var, const Bit8u val) {
    host_writeb((HostPt)var, val);
}

static INLINE void var_write(Bit16u * const var, const Bit16u val) {
    host_writew((HostPt)var, val);
}

static INLINE void var_write(Bit32u * const var, const Bit32u val) {
    host_writed((HostPt)var, val);
}

static INLINE void var_write(Bit64u * const var, const Bit64u val) {
    host_writeq((HostPt)var, val);
}

/* The Folowing six functions are slower but they recognize the paged memory system */

Bit8u  mem_readb(const PhysPt address);
Bit16u mem_readw(const PhysPt address);
Bit32u mem_readd(const PhysPt address);

void mem_writeb(const PhysPt address,const Bit8u val);
void mem_writew(const PhysPt address,const Bit16u val);
void mem_writed(const PhysPt address,const Bit32u val);

void phys_writes(PhysPt addr, const char* string, Bitu length);

/* WARNING: These will cause a segfault or out of bounds access IF
 *          addr is beyond the end of memory */

static INLINE void phys_writeb(const PhysPt addr,const Bit8u val) {
    host_writeb(MemBase+addr,val);
}
static INLINE void phys_writew(const PhysPt addr,const Bit16u val) {
    host_writew(MemBase+addr,val);
}
static INLINE void phys_writed(const PhysPt addr,const Bit32u val) {
    host_writed(MemBase+addr,val);
}

static INLINE Bit8u phys_readb(const PhysPt addr) {
    return host_readb(MemBase+addr);
}
static INLINE Bit16u phys_readw(const PhysPt addr) {
    return host_readw(MemBase+addr);
}
static INLINE Bit32u phys_readd(const PhysPt addr) {
    return host_readd(MemBase+addr);
}

/* These don't check for alignment, better be sure it's correct */

void MEM_BlockWrite(PhysPt pt,void const * const data,Bitu size);
void MEM_BlockRead(PhysPt pt,void * data,Bitu size);
void MEM_BlockWrite32(PhysPt pt,void * data,Bitu size);
void MEM_BlockRead32(PhysPt pt,void * data,Bitu size);
void MEM_BlockCopy(PhysPt dest,PhysPt src,Bitu size);
void MEM_StrCopy(PhysPt pt,char * data,Bitu size);

void mem_memcpy(PhysPt dest,PhysPt src,Bitu size);
Bitu mem_strlen(PhysPt pt);
void mem_strcpy(PhysPt dest,PhysPt src);

/* The folowing functions are all shortcuts to the above functions using physical addressing */

static inline constexpr PhysPt PhysMake(const Bit16u seg,const Bit16u off) {
    return ((PhysPt)seg << (PhysPt)4U) + (PhysPt)off;
}

static inline constexpr Bit16u RealSeg(const RealPt pt) {
    return (Bit16u)((RealPt)pt >> (RealPt)16U);
}

static inline constexpr Bit16u RealOff(const RealPt pt) {
    return (Bit16u)((RealPt)pt & (RealPt)0xffffu);
}

static inline constexpr PhysPt Real2Phys(const RealPt pt) {
    return (PhysPt)(((PhysPt)RealSeg(pt) << (PhysPt)4U) + (PhysPt)RealOff(pt));
}

static inline constexpr RealPt RealMake(const Bit16u seg,const Bit16u off) {
    return (RealPt)(((RealPt)seg << (RealPt)16U) + (RealPt)off);
}

/* convert physical address to 4:16 real pointer (example: 0xABCDE -> 0xA000:0xBCDE) */
static inline constexpr RealPt PhysToReal416(const PhysPt phys) {
    return RealMake((Bit16u)(((PhysPt)phys >> (PhysPt)4U) & (PhysPt)0xF000U),(Bit16u)((PhysPt)phys & (PhysPt)0xFFFFU));
}

static inline constexpr PhysPt RealVecAddress(const Bit8u vec) {
    return (PhysPt)((unsigned int)vec << 2U);
}


static INLINE Bit8u real_readb(const Bit16u seg,const Bit16u off) {
    return mem_readb(PhysMake(seg,off));
}
static INLINE Bit16u real_readw(const Bit16u seg,const Bit16u off) {
    return mem_readw(PhysMake(seg,off));
}
static INLINE Bit32u real_readd(const Bit16u seg,const Bit16u off) {
    return mem_readd(PhysMake(seg,off));
}

static INLINE void real_writeb(const Bit16u seg,const Bit16u off,const Bit8u val) {
    mem_writeb(PhysMake(seg,off),val);
}
static INLINE void real_writew(const Bit16u seg,const Bit16u off,const Bit16u val) {
    mem_writew(PhysMake(seg,off),val);
}
static INLINE void real_writed(const Bit16u seg,const Bit16u off,const Bit32u val) {
    mem_writed(PhysMake(seg,off),val);
}


static INLINE RealPt RealGetVec(const Bit8u vec) {
    return mem_readd(RealVecAddress(vec));
}

static INLINE void RealSetVec(const Bit8u vec,const RealPt pt) {
    mem_writed(RealVecAddress(vec),(Bit32u)pt);
}

static INLINE void RealSetVec(const Bit8u vec,const RealPt pt,RealPt &old) {
    const PhysPt addr = RealVecAddress(vec);
    old = mem_readd(addr);
    mem_writed(addr,pt);
}

#endif

