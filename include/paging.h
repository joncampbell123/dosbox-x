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

/* $Id: paging.h,v 1.33 2009-05-27 09:15:41 qbix79 Exp $ */

#ifndef DOSBOX_PAGING_H
#define DOSBOX_PAGING_H

#ifndef DOSBOX_DOSBOX_H
#include "dosbox.h"
#endif
#ifndef DOSBOX_MEM_H
#include "mem.h"
#endif

// disable this to reduce the size of the TLB
// NOTE: does not work with the dynamic core (dynrec is fine)
#define USE_FULL_TLB

class PageDirectory;

#define MEM_PAGE_SIZE	(4096)
#define XMS_START		(0x110)

#if defined(USE_FULL_TLB)
#define TLB_SIZE		(1024*1024)
#else
#define TLB_SIZE		65536	// This must a power of 2 and greater then LINK_START
#define BANK_SHIFT		28
#define BANK_MASK		0xffff // always the same as TLB_SIZE-1?
#define TLB_BANKS		((1024*1024/TLB_SIZE)-1)
#endif

#define PFLAG_READABLE		0x1
#define PFLAG_WRITEABLE		0x2
#define PFLAG_HASROM		0x4
#define PFLAG_HASCODE		0x8				//Page contains dynamic code
#define PFLAG_NOCODE		0x10			//No dynamic code can be generated here
#define PFLAG_INIT			0x20			//No dynamic code can be generated here

#define LINK_START	((1024+64)/4)			//Start right after the HMA

//Allow 128 mb of memory to be linked
#define PAGING_LINKS (128*1024/4)

class PageHandler {
public:
	virtual ~PageHandler(void) { }
	virtual Bitu readb(PhysPt addr);
	virtual Bitu readw(PhysPt addr);
	virtual Bitu readd(PhysPt addr);
	virtual void writeb(PhysPt addr,Bitu val);
	virtual void writew(PhysPt addr,Bitu val);
	virtual void writed(PhysPt addr,Bitu val);
	virtual HostPt GetHostReadPt(Bitu phys_page);
	virtual HostPt GetHostWritePt(Bitu phys_page);
	virtual bool readb_checked(PhysPt addr,Bit8u * val);
	virtual bool readw_checked(PhysPt addr,Bit16u * val);
	virtual bool readd_checked(PhysPt addr,Bit32u * val);
	virtual bool writeb_checked(PhysPt addr,Bitu val);
	virtual bool writew_checked(PhysPt addr,Bitu val);
	virtual bool writed_checked(PhysPt addr,Bitu val);
	Bitu flags;
};

/* Some other functions */
void PAGING_Enable(bool enabled);
bool PAGING_Enabled(void);

Bitu PAGING_GetDirBase(void);
void PAGING_SetDirBase(Bitu cr3);
void PAGING_InitTLB(void);
void PAGING_ClearTLB(void);

void PAGING_LinkPage(Bitu lin_page,Bitu phys_page);
void PAGING_LinkPage_ReadOnly(Bitu lin_page,Bitu phys_page);
void PAGING_UnlinkPages(Bitu lin_page,Bitu pages);
/* This maps the page directly, only use when paging is disabled */
void PAGING_MapPage(Bitu lin_page,Bitu phys_page);
bool PAGING_MakePhysPage(Bitu & page);
bool PAGING_ForcePageInit(Bitu lin_addr);

void MEM_SetLFB(Bitu page, Bitu pages, PageHandler *handler, PageHandler *mmiohandler);
void MEM_SetPageHandler(Bitu phys_page, Bitu pages, PageHandler * handler);
void MEM_ResetPageHandler(Bitu phys_page, Bitu pages);


#ifdef _MSC_VER
#pragma pack (1)
#endif
struct X86_PageEntryBlock{
#ifdef WORDS_BIGENDIAN
	Bit32u		base:20;
	Bit32u		avl:3;
	Bit32u		g:1;
	Bit32u		pat:1;
	Bit32u		d:1;
	Bit32u		a:1;
	Bit32u		pcd:1;
	Bit32u		pwt:1;
	Bit32u		us:1;
	Bit32u		wr:1;
	Bit32u		p:1;
#else
	Bit32u		p:1;
	Bit32u		wr:1;
	Bit32u		us:1;
	Bit32u		pwt:1;
	Bit32u		pcd:1;
	Bit32u		a:1;
	Bit32u		d:1;
	Bit32u		pat:1;
	Bit32u		g:1;
	Bit32u		avl:3;
	Bit32u		base:20;
#endif
} GCC_ATTRIBUTE(packed);
#ifdef _MSC_VER
#pragma pack ()
#endif


union X86PageEntry {
	Bit32u load;
	X86_PageEntryBlock block;
};

#if !defined(USE_FULL_TLB)
typedef struct {
	HostPt read;
	HostPt write;
	PageHandler * readhandler;
	PageHandler * writehandler;
	Bit32u phys_page;
} tlb_entry;
#endif

struct PagingBlock {
	Bitu			cr3;
	Bitu			cr2;
	struct {
		Bitu page;
		PhysPt addr;
	} base;
#if defined(USE_FULL_TLB)
	struct {
		HostPt read[TLB_SIZE];
		HostPt write[TLB_SIZE];
		PageHandler * readhandler[TLB_SIZE];
		PageHandler * writehandler[TLB_SIZE];
		Bit32u	phys_page[TLB_SIZE];
	} tlb;
#else
	tlb_entry tlbh[TLB_SIZE];
	tlb_entry *tlbh_banks[TLB_BANKS];
#endif
	struct {
		Bitu used;
		Bit32u entries[PAGING_LINKS];
	} links;
	Bit32u		firstmb[LINK_START];
	bool		enabled;
};

extern PagingBlock paging; 

/* Some support functions */

PageHandler * MEM_GetPageHandler(Bitu phys_page);


/* Unaligned address handlers */
Bit16u mem_unalignedreadw(PhysPt address);
Bit32u mem_unalignedreadd(PhysPt address);
void mem_unalignedwritew(PhysPt address,Bit16u val);
void mem_unalignedwrited(PhysPt address,Bit32u val);

bool mem_unalignedreadw_checked(PhysPt address,Bit16u * val);
bool mem_unalignedreadd_checked(PhysPt address,Bit32u * val);
bool mem_unalignedwritew_checked(PhysPt address,Bit16u val);
bool mem_unalignedwrited_checked(PhysPt address,Bit32u val);

#if defined(USE_FULL_TLB)

static INLINE HostPt get_tlb_read(PhysPt address) {
	return paging.tlb.read[address>>12];
}
static INLINE HostPt get_tlb_write(PhysPt address) {
	return paging.tlb.write[address>>12];
}
static INLINE PageHandler* get_tlb_readhandler(PhysPt address) {
	return paging.tlb.readhandler[address>>12];
}
static INLINE PageHandler* get_tlb_writehandler(PhysPt address) {
	return paging.tlb.writehandler[address>>12];
}

/* Use these helper functions to access linear addresses in readX/writeX functions */
static INLINE PhysPt PAGING_GetPhysicalPage(PhysPt linePage) {
	return (paging.tlb.phys_page[linePage>>12]<<12);
}

static INLINE PhysPt PAGING_GetPhysicalAddress(PhysPt linAddr) {
	return (paging.tlb.phys_page[linAddr>>12]<<12)|(linAddr&0xfff);
}

#else

void PAGING_InitTLBBank(tlb_entry **bank);

static INLINE tlb_entry *get_tlb_entry(PhysPt address) {
	Bitu index=(address>>12);
	if (TLB_BANKS && (index > TLB_SIZE)) {
		Bitu bank=(address>>BANK_SHIFT) - 1;
		if (!paging.tlbh_banks[bank])
			PAGING_InitTLBBank(&paging.tlbh_banks[bank]);
		return &paging.tlbh_banks[bank][index & BANK_MASK];
	}
	return &paging.tlbh[index];
}

static INLINE HostPt get_tlb_read(PhysPt address) {
	return get_tlb_entry(address)->read;
}
static INLINE HostPt get_tlb_write(PhysPt address) {
	return get_tlb_entry(address)->write;
}
static INLINE PageHandler* get_tlb_readhandler(PhysPt address) {
	return get_tlb_entry(address)->readhandler;
}
static INLINE PageHandler* get_tlb_writehandler(PhysPt address) {
	return get_tlb_entry(address)->writehandler;
}

/* Use these helper functions to access linear addresses in readX/writeX functions */
static INLINE PhysPt PAGING_GetPhysicalPage(PhysPt linePage) {
	tlb_entry *entry = get_tlb_entry(linePage);
	return (entry->phys_page<<12);
}

static INLINE PhysPt PAGING_GetPhysicalAddress(PhysPt linAddr) {
	tlb_entry *entry = get_tlb_entry(linAddr);
	return (entry->phys_page<<12)|(linAddr&0xfff);
}
#endif

/* Special inlined memory reading/writing */

static INLINE Bit8u mem_readb_inline(PhysPt address) {
	HostPt tlb_addr=get_tlb_read(address);
	if (tlb_addr) return host_readb(tlb_addr+address);
	else return (Bit8u)(get_tlb_readhandler(address))->readb(address);
}

static INLINE Bit16u mem_readw_inline(PhysPt address) {
	if ((address & 0xfff)<0xfff) {
		HostPt tlb_addr=get_tlb_read(address);
		if (tlb_addr) return host_readw(tlb_addr+address);
		else return (Bit16u)(get_tlb_readhandler(address))->readw(address);
	} else return mem_unalignedreadw(address);
}

static INLINE Bit32u mem_readd_inline(PhysPt address) {
	if ((address & 0xfff)<0xffd) {
		HostPt tlb_addr=get_tlb_read(address);
		if (tlb_addr) return host_readd(tlb_addr+address);
		else return (get_tlb_readhandler(address))->readd(address);
	} else return mem_unalignedreadd(address);
}

static INLINE void mem_writeb_inline(PhysPt address,Bit8u val) {
	HostPt tlb_addr=get_tlb_write(address);
	if (tlb_addr) host_writeb(tlb_addr+address,val);
	else (get_tlb_writehandler(address))->writeb(address,val);
}

static INLINE void mem_writew_inline(PhysPt address,Bit16u val) {
	if ((address & 0xfff)<0xfff) {
		HostPt tlb_addr=get_tlb_write(address);
		if (tlb_addr) host_writew(tlb_addr+address,val);
		else (get_tlb_writehandler(address))->writew(address,val);
	} else mem_unalignedwritew(address,val);
}

static INLINE void mem_writed_inline(PhysPt address,Bit32u val) {
	if ((address & 0xfff)<0xffd) {
		HostPt tlb_addr=get_tlb_write(address);
		if (tlb_addr) host_writed(tlb_addr+address,val);
		else (get_tlb_writehandler(address))->writed(address,val);
	} else mem_unalignedwrited(address,val);
}


static INLINE bool mem_readb_checked(PhysPt address, Bit8u * val) {
	HostPt tlb_addr=get_tlb_read(address);
	if (tlb_addr) {
		*val=host_readb(tlb_addr+address);
		return false;
	} else return (get_tlb_readhandler(address))->readb_checked(address, val);
}

static INLINE bool mem_readw_checked(PhysPt address, Bit16u * val) {
	if ((address & 0xfff)<0xfff) {
		HostPt tlb_addr=get_tlb_read(address);
		if (tlb_addr) {
			*val=host_readw(tlb_addr+address);
			return false;
		} else return (get_tlb_readhandler(address))->readw_checked(address, val);
	} else return mem_unalignedreadw_checked(address, val);
}

static INLINE bool mem_readd_checked(PhysPt address, Bit32u * val) {
	if ((address & 0xfff)<0xffd) {
		HostPt tlb_addr=get_tlb_read(address);
		if (tlb_addr) {
			*val=host_readd(tlb_addr+address);
			return false;
		} else return (get_tlb_readhandler(address))->readd_checked(address, val);
	} else return mem_unalignedreadd_checked(address, val);
}

static INLINE bool mem_writeb_checked(PhysPt address,Bit8u val) {
	HostPt tlb_addr=get_tlb_write(address);
	if (tlb_addr) {
		host_writeb(tlb_addr+address,val);
		return false;
	} else return (get_tlb_writehandler(address))->writeb_checked(address,val);
}

static INLINE bool mem_writew_checked(PhysPt address,Bit16u val) {
	if ((address & 0xfff)<0xfff) {
		HostPt tlb_addr=get_tlb_write(address);
		if (tlb_addr) {
			host_writew(tlb_addr+address,val);
			return false;
		} else return (get_tlb_writehandler(address))->writew_checked(address,val);
	} else return mem_unalignedwritew_checked(address,val);
}

static INLINE bool mem_writed_checked(PhysPt address,Bit32u val) {
	if ((address & 0xfff)<0xffd) {
		HostPt tlb_addr=get_tlb_write(address);
		if (tlb_addr) {
			host_writed(tlb_addr+address,val);
			return false;
		} else return (get_tlb_writehandler(address))->writed_checked(address,val);
	} else return mem_unalignedwrited_checked(address,val);
}


#endif
