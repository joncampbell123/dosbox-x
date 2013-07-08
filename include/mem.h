/*
 *  Copyright (C) 2002-2010  The DOSBox Team
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

#ifndef DOSBOX_MEM_H
#define DOSBOX_MEM_H

#ifndef DOSBOX_DOSBOX_H
#include "dosbox.h"
#endif

typedef Bit32u PhysPt;
typedef Bit8u * HostPt;
typedef Bit32u RealPt;

typedef Bit32s MemHandle;

#define MEM_PAGESIZE 4096

extern HostPt MemBase;
HostPt GetMemBase(void);

bool MEM_A20_Enabled(void);
void MEM_A20_Enable(bool enable);

/* Memory management / EMS mapping */
HostPt MEM_GetBlockPage(void);
Bitu MEM_FreeTotal(void);			//Free 4 kb pages
Bitu MEM_FreeLargest(void);			//Largest free 4 kb pages block
Bitu MEM_TotalPages(void);			//Total amount of 4 kb pages
Bitu MEM_AllocatedPages(MemHandle handle); // amount of allocated pages of handle
MemHandle MEM_AllocatePages(Bitu pages,bool sequence);
MemHandle MEM_GetNextFreePage(void);
PhysPt MEM_AllocatePage(void);
void MEM_ReleasePages(MemHandle handle);
bool MEM_ReAllocatePages(MemHandle & handle,Bitu pages,bool sequence);

MemHandle MEM_NextHandle(MemHandle handle);
MemHandle MEM_NextHandleAt(MemHandle handle,Bitu where);

/* 
	The folowing six functions are used everywhere in the end so these should be changed for
	Working on big or little endian machines 
*/

#if defined(WORDS_BIGENDIAN) || !defined(C_UNALIGNED_MEMORY)

static INLINE Bit8u host_readb(HostPt off) {
	return off[0];
}
static INLINE Bit16u host_readw(HostPt off) {
	return off[0] | (off[1] << 8);
}
static INLINE Bit32u host_readd(HostPt off) {
	return off[0] | (off[1] << 8) | (off[2] << 16) | (off[3] << 24);
}
static INLINE void host_writeb(HostPt off,Bit8u val) {
	off[0]=val;
}
static INLINE void host_writew(HostPt off,Bit16u val) {
	off[0]=(Bit8u)(val);
	off[1]=(Bit8u)(val >> 8);
}
static INLINE void host_writed(HostPt off,Bit32u val) {
	off[0]=(Bit8u)(val);
	off[1]=(Bit8u)(val >> 8);
	off[2]=(Bit8u)(val >> 16);
	off[3]=(Bit8u)(val >> 24);
}

#else

static INLINE Bit8u host_readb(HostPt off) {
	return *(Bit8u *)off;
}
static INLINE Bit16u host_readw(HostPt off) {
	return *(Bit16u *)off;
}
static INLINE Bit32u host_readd(HostPt off) {
	return *(Bit32u *)off;
}
static INLINE void host_writeb(HostPt off,Bit8u val) {
	*(Bit8u *)(off)=val;
}
static INLINE void host_writew(HostPt off,Bit16u val) {
	*(Bit16u *)(off)=val;
}
static INLINE void host_writed(HostPt off,Bit32u val) {
	*(Bit32u *)(off)=val;
}

#endif


static INLINE void var_write(Bit8u * var, Bit8u val) {
	host_writeb((HostPt)var, val);
}

static INLINE void var_write(Bit16u * var, Bit16u val) {
	host_writew((HostPt)var, val);
}

static INLINE void var_write(Bit32u * var, Bit32u val) {
	host_writed((HostPt)var, val);
}

/* The Folowing six functions are slower but they recognize the paged memory system */

Bit8u  mem_readb(PhysPt pt);
Bit16u mem_readw(PhysPt pt);
Bit32u mem_readd(PhysPt pt);

void mem_writeb(PhysPt pt,Bit8u val);
void mem_writew(PhysPt pt,Bit16u val);
void mem_writed(PhysPt pt,Bit32u val);

static INLINE void phys_writeb(PhysPt addr,Bit8u val) {
	host_writeb(MemBase+addr,val);
}
static INLINE void phys_writew(PhysPt addr,Bit16u val){
	host_writew(MemBase+addr,val);
}
static INLINE void phys_writed(PhysPt addr,Bit32u val){
	host_writed(MemBase+addr,val);
}

static INLINE Bit8u phys_readb(PhysPt addr) {
	return host_readb(MemBase+addr);
}
static INLINE Bit16u phys_readw(PhysPt addr){
	return host_readw(MemBase+addr);
}
static INLINE Bit32u phys_readd(PhysPt addr){
	return host_readd(MemBase+addr);
}

/* These don't check for alignment, better be sure it's correct */

void MEM_BlockWrite(PhysPt pt,void const * const data,Bitu size);
void MEM_BlockRead(PhysPt pt,void * data,Bitu size);
void MEM_BlockCopy(PhysPt dest,PhysPt src,Bitu size);
void MEM_StrCopy(PhysPt pt,char * data,Bitu size);

void mem_memcpy(PhysPt dest,PhysPt src,Bitu size);
Bitu mem_strlen(PhysPt pt);
void mem_strcpy(PhysPt dest,PhysPt src);

/* The folowing functions are all shortcuts to the above functions using physical addressing */

static INLINE Bit8u real_readb(Bit16u seg,Bit16u off) {
	return mem_readb((seg<<4)+off);
}
static INLINE Bit16u real_readw(Bit16u seg,Bit16u off) {
	return mem_readw((seg<<4)+off);
}
static INLINE Bit32u real_readd(Bit16u seg,Bit16u off) {
	return mem_readd((seg<<4)+off);
}

static INLINE void real_writeb(Bit16u seg,Bit16u off,Bit8u val) {
	mem_writeb(((seg<<4)+off),val);
}
static INLINE void real_writew(Bit16u seg,Bit16u off,Bit16u val) {
	mem_writew(((seg<<4)+off),val);
}
static INLINE void real_writed(Bit16u seg,Bit16u off,Bit32u val) {
	mem_writed(((seg<<4)+off),val);
}


static INLINE Bit16u RealSeg(RealPt pt) {
	return (Bit16u)(pt>>16);
}

static INLINE Bit16u RealOff(RealPt pt) {
	return (Bit16u)(pt&0xffff);
}

static INLINE PhysPt Real2Phys(RealPt pt) {
	return (RealSeg(pt)<<4) +RealOff(pt);
}

static INLINE PhysPt PhysMake(Bit16u seg,Bit16u off) {
	return (seg<<4)+off;
}

static INLINE RealPt RealMake(Bit16u seg,Bit16u off) {
	return (seg<<16)+off;
}

static INLINE void RealSetVec(Bit8u vec,RealPt pt) {
	mem_writed(vec<<2,pt);
}

static INLINE void RealSetVec(Bit8u vec,RealPt pt,RealPt &old) {
	old = mem_readd(vec<<2);
	mem_writed(vec<<2,pt);
}

static INLINE RealPt RealGetVec(Bit8u vec) {
	return mem_readd(vec<<2);
}	

#endif

