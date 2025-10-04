/*
 *  Copyright (C) 2002-2021  The DOSBox Team
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


#ifndef DOSBOX_PAGING_H
#define DOSBOX_PAGING_H

#include <exception>

#include "mem.h"
#include "bitop.h"

class PageHandler;
class MEM_CalloutObject;

class PageDirectory;

typedef PageHandler* (MEM_CalloutHandler)(MEM_CalloutObject &co,Bitu phys_page);

void MEM_RegisterHandler(Bitu phys_page,PageHandler *handler,Bitu page_range=1);

void MEM_FreeHandler(Bitu phys_page,Bitu page_range=1);

void MEM_InvalidateCachedHandler(Bitu phys_page,Bitu range=1);

static const Bitu MEMMASK_ISA_20BIT = 0x000000FFU; /* ISA 20-bit decode (20 - 12 = 8) */
static const Bitu MEMMASK_ISA_24BIT = 0x00000FFFU; /* ISA 24-bit decode (24 - 12 = 12) */
static const Bitu MEMMASK_FULL      = 0x000FFFFFU; /* full 32-bit decode (32 - 12 = 20) */

extern uint8_t enable_pse_extmask;

/* WARNING: Will only produce a correct result if 'x' is a nonzero power of two.
 * For use with MEMMASK_Combine. 'x' is in units of PAGES not BYTES.
 */
static inline Bitu MEMMASK_Range(const Bitu x) {
    return ~(x - 1);
}

/* combine range mask with MEMMASK value.
 */
static inline Bitu MEMMASK_Combine(const Bitu a,const Bitu b) {
    return a & b;
}

#define MEM_PAGE_SIZE		(4096)
#define XMS_START		(0x110)
#define TLB_SIZE		(1024*1024)

#define PFLAG_READABLE		0x1u
#define PFLAG_WRITEABLE		0x2u
#define PFLAG_HASROM		0x4u
#define PFLAG_HASCODE32		0x8u					//Page contains dynamic code
#define PFLAG_NOCODE		0x10u					//No dynamic code can be generated here
#define PFLAG_INIT		0x20u					//No dynamic code can be generated here
#define PFLAG_HASCODE16		0x40u					//Page contains 16-bit dynamic code
#define PFLAG_HASCODE		(PFLAG_HASCODE32|PFLAG_HASCODE16)

#define LINK_START		((1024+64)/4)				//Start right after the HMA

//Allow 128 mb of memory to be linked
#define PAGING_LINKS		(128*1024/4)

/* NTS: Despite that all devices respond to these page handlers, all addresses are still linear addresses.
 *      Devices that need physical memory addresses will need to call PAGING_GetPhysicalAddress() to translate
 *      linear to physical, or PAGING_GetPhysicalAddress64() if the device supports responding to addresses
 *      above 4GB. This sounds crazy, but this is one aspect of emulation inherited from DOSBox SVN and I
 *      don't think it's wise to break code to change it.  */
class PageHandler {
public:
	PageHandler(Bitu flg) : flags(flg) {}
	virtual ~PageHandler(void) { }
	PageHandler(void) : flags(0) { }

	virtual uint8_t readb(PhysPt addr);
	virtual uint16_t readw(PhysPt addr);
	virtual uint32_t readd(PhysPt addr);
	virtual void writeb(PhysPt addr,uint8_t val);
	virtual void writew(PhysPt addr,uint16_t val);
	virtual void writed(PhysPt addr,uint32_t val);
	virtual HostPt GetHostReadPt(PageNum phys_page);
	virtual HostPt GetHostWritePt(PageNum phys_page);
	virtual bool readb_checked(PhysPt addr,uint8_t * val);
	virtual bool readw_checked(PhysPt addr,uint16_t * val);
	virtual bool readd_checked(PhysPt addr,uint32_t * val);
	virtual bool writeb_checked(PhysPt addr,uint8_t val);
	virtual bool writew_checked(PhysPt addr,uint16_t val);
	virtual bool writed_checked(PhysPt addr,uint32_t val);

	Bitu getFlags() const {
		return flags;
	}
	void setFlags(const Bitu flagsNew) {
		flags = flagsNew;
	}

	Bitu flags;
private:
	PageHandler(const PageHandler&);

};

/* NTS: To explain the Install() method, the caller not only provides the IOMASK_.. value, but ANDs
 *      the least significant bits to define the range of I/O ports to respond to. An ISA Sound Blaster
 *      for example would set portmask = (IOMASK_ISA_10BIT & (~0xF)) in order to respond to 220h-22Fh,
 *      240h-24Fh, etc. At I/O callout time, the callout object is tested
 *      if (cpu_ioport & io_mask) == (m_port & io_mask)
 *
 *      This does not prevent emulation of devices that start on non-aligned ports or strange port ranges,
 *      because the callout handler is free to decline the I/O request, leading the callout process to
 *      move on to the next device or mark the I/O port as empty. */
class MEM_CalloutObject {
public:
	MEM_CalloutObject() {};
	void InvalidateCachedHandlers(void);
	void Install(Bitu page,Bitu pagemask/*MEMMASK_ISA_24BIT, etc.*/,MEM_CalloutHandler *handler);
	void Uninstall();
public:
	bool installed = false;
	Bitu mem_mask = 0xFFFFFFFF;
	Bitu range_mask = 0;
	Bitu alias_mask = 0xFFFFFFFF;
	unsigned int getcounter = 0;
	MEM_CalloutHandler *m_handler = NULL;
	Bitu m_base = 0;
	bool alloc = false;
public:
	inline bool MatchPage(const Bitu p) {
		/* (p & io_mask) == (m_port & io_mask) but this also works.
		 * apparently modern x86 processors are faster at addition/subtraction than bitmasking.
		 * for this to work, m_port must be a multiple of the I/O range. For example, if the I/O
		 * range is 16 ports, then m_port must be a multiple of 16. */
		return ((p - m_base) & mem_mask) == 0;
	}
	inline bool isInstalled(void) {
		return installed;
	}
};

enum MEM_Type_t {
	MEM_TYPE_NONE=0,
	MEM_TYPE_MIN=1,
	MEM_TYPE_ISA=1,
	MEM_TYPE_PCI,
	MEM_TYPE_MB,

	MEM_TYPE_MAX
};

void MEM_InitCallouts(void);

typedef uint32_t MEM_Callout_t;

static inline uint32_t MEM_Callout_t_comb(const enum MEM_Type_t t,const uint32_t idx) {
    return ((uint32_t)t << (uint32_t)28) + idx;
}

static inline enum MEM_Type_t MEM_Callout_t_type(const MEM_Callout_t t) {
    return (enum MEM_Type_t)(t >> 28);
}

static inline uint32_t MEM_Callout_t_index(const MEM_Callout_t t) {
    return t & (((uint32_t)1 << (uint32_t)28) - (uint32_t)1);
}

static const MEM_Callout_t MEM_Callout_t_none = (MEM_Callout_t)0;

MEM_Callout_t MEM_AllocateCallout(MEM_Type_t t);
void MEM_FreeCallout(MEM_Callout_t c);
MEM_CalloutObject *MEM_GetCallout(MEM_Callout_t c);
void MEM_PutCallout(MEM_CalloutObject *obj);

/* Some other functions */
void PAGING_Enable(bool enabled);
bool PAGING_Enabled(void);
void PAGING_SetWP(bool wp);
void PAGING_SwitchCPL(bool isUser);

Bitu PAGING_GetDirBase(void);
void PAGING_SetDirBase(Bitu cr3);
void PAGING_InitTLB(void);
void PAGING_ClearTLB(void);

void PAGING_LinkPage(PageNum lin_page,PageNum phys_page);
void PAGING_UnlinkPages(PageNum lin_page,PageNum pages);
/* This maps the page directly, only use when paging is disabled */
void PAGING_MapPage(PageNum lin_page,PageNum phys_page);
bool PAGING_MakePhysPage(PageNum &page);

void MEM_SetLFB(Bitu page, Bitu pages, PageHandler *handler, PageHandler *mmiohandler);
void MEM_SetPageHandler(Bitu phys_page, Bitu pages, PageHandler * handler);


#ifdef _MSC_VER
#pragma pack (1)
#endif

/* Page table TLB. At some point, 64-bit builds will have 64-bit TLB type */
typedef uint32_t tlbentry_t;

static constexpr size_t tlbentry_bits = bitop::type_bits<tlbentry_t>();
/* bits 31-30: ACCESS_* contants */
static constexpr tlbentry_t PHYSPAGE_ACCESS_BITS_SHIFT = tlbentry_bits - 2u;
static constexpr tlbentry_t PHYSPAGE_ACCESS_BITS = bitop::bitcount2masklsb<2u,PHYSPAGE_ACCESS_BITS_SHIFT,tlbentry_t>();
/* bit 29: dirty bit */
static constexpr tlbentry_t PHYSPAGE_DIRTY_SHIFT = tlbentry_bits - 3u;
static constexpr tlbentry_t PHYSPAGE_DIRTY = bitop::bit2mask<PHYSPAGE_DIRTY_SHIFT,tlbentry_t>();
/* bits 28: not defined */
/* bits 27-0: physical page. 40-bit addresses, that's as far as we can go, and the limits of PSE anyway */
static constexpr tlbentry_t PHYSPAGE_ADDR =
	build_memlimit_32bit() ?
		bitop::bitcount2masklsb<20,0,tlbentry_t>() : /* 32-bit builds limited to 1GB no point emulating past 4GB (20+12) = 32 */
		bitop::bitcount2masklsb<28,0,tlbentry_t>();  /* 64-bit builds allowed up to 1TB (28+12) = 40 */

struct X86_PageEntryBlock{ // Page Table Entry, though it keeps the PageEntryBlock name to avoid breaking all this code
#ifdef WORDS_BIGENDIAN
	uint32_t		base:20;	// [31:12]
	uint32_t		avl:3;		// [11: 8]
	uint32_t		g:1;		// [ 8: 8]
	uint32_t		pat:1;		// [ 7: 7]
	uint32_t		d:1;		// [ 6: 6]
	uint32_t		a:1;		// [ 5: 5]
	uint32_t		pcd:1;		// [ 4: 4]
	uint32_t		pwt:1;		// [ 3: 3]
	uint32_t		us:1;		// [ 2: 2]
	uint32_t		wr:1;		// [ 1: 1]
	uint32_t		p:1;		// [ 0: 0]
#else
	uint32_t		p:1;		// [ 0: 0]
	uint32_t		wr:1;		// [ 1: 1] R/W
	uint32_t		us:1;		// [ 2: 2] U/S
	uint32_t		pwt:1;		// [ 3: 3]
	uint32_t		pcd:1;		// [ 4: 4]
	uint32_t		a:1;		// [ 5: 5]
	uint32_t		d:1;		// [ 6: 6]
	uint32_t		pat:1;		// [ 7: 7]
	uint32_t		g:1;		// [ 8: 8]
	uint32_t		avl:3;		// [11: 9]
	uint32_t		base:20;	// [31:12]
#endif
} GCC_ATTRIBUTE(packed);

struct X86_PageDirEntryBlock{
#ifdef WORDS_BIGENDIAN
	uint32_t		base:20;	// [31:12]
	uint32_t		avl:3;		// [11: 9]
	uint32_t		g:1;		// [ 8: 8]
	uint32_t		ps:1;		// [ 7: 7]
	uint32_t		avl6:1;		// [ 6: 6]
	uint32_t		a:1;		// [ 5: 5]
	uint32_t		pcd:1;		// [ 4: 4]
	uint32_t		pwt:1;		// [ 3: 3]
	uint32_t		us:1;		// [ 2: 2]
	uint32_t		wr:1;		// [ 1: 1]
	uint32_t		p:1;		// [ 0: 0]
#else
	uint32_t		p:1;		// [ 0: 0]
	uint32_t		wr:1;		// [ 1: 1] R/W
	uint32_t		us:1;		// [ 2: 2] U/S
	uint32_t		pwt:1;		// [ 3: 3]
	uint32_t		pcd:1;		// [ 4: 4]
	uint32_t		a:1;		// [ 5: 5]
	uint32_t		avl6:1;		// [ 6: 6] See Note [*1], this bit changed meaning between the 486 and Pentium!
	uint32_t		ps:1;		// [ 7: 7] Page Size Extension (Pentium), even though Intel didn't document it until later
	uint32_t		g:1;		// [ 8: 8]
	uint32_t		avl:3;		// [11: 9]
	uint32_t		base:20;	// [31:12]
#endif
} GCC_ATTRIBUTE(packed);
/* Note [*1]: The i386 defined just the Page Table Entry to represent the PDE and PTE, because they had the same layout.
 *
 *            The i486 lists the PDE and PTE, even though they have the same bit layout.
 *
 *            The Pentium however, seems to have decided that bit 6 of the PDE is no longer the "dirty" bit and is redefined as AVL.
 *            Although, if PSE is enabled and the PDE is the 4MB format, bit 6 is once again a "dirty" but.
 *
 *            Ref: [http://hackipedia.org/browse.cgi/Computer/Platform/PC%2c%20IBM%20compatible/CPU/80386/Intel/386DX%20Microprocessor%20Programmer%27s%20Reference%20Manual%20%281990%29%2epdf]
 *            Ref: [http://hackipedia.org/browse.cgi/Computer/Platform/PC%2c%20IBM%20compatible/CPU/80486/Intel/i486%20Microprocessor%20%281989%2d04%29%2epdf]
 *            Ref: [http://hackipedia.org/browse.cgi/Computer/Platform/PC%2c%20IBM%20compatible/CPU/Pentium/Pentium%20Processor%20Family%20Developer%27s%20Manual%20%2d%20Volume%203%3a%20Architecture%20and%20Programming%20Manual%20%281995%2d07%29%2epdf] */

struct X86_PageDir4MBEntryBlock{ // PSE=1 and PageDirEntryBlock PS=1
#ifdef WORDS_BIGENDIAN
	uint32_t		base22:10;	// [31:22] bits 31:22
	uint32_t		reserved:1;	// [21:21]
	uint32_t		base32:8;	// [20:13] bits 39:32 if PSE36
	uint32_t		pat:1;		// [12:12]
	uint32_t		avl:3;		// [11: 9]
	uint32_t		g:1;		// [ 8: 8]
	uint32_t		ps:1;		// [ 7: 7] PS=1, or else this is just X86_PageDirEntryBlock
	uint32_t		d:1;		// [ 6: 6]
	uint32_t		a:1;		// [ 5: 5]
	uint32_t		pcd:1;		// [ 4: 4]
	uint32_t		pwt:1;		// [ 3: 3]
	uint32_t		us:1;		// [ 2: 2] U/S
	uint32_t		wr:1;		// [ 1: 1] R/W
	uint32_t		p:1;		// [ 0: 0]
#else
	uint32_t		p:1;		// [ 0: 0]
	uint32_t		wr:1;		// [ 1: 1] R/W
	uint32_t		us:1;		// [ 2: 2] U/S
	uint32_t		pwt:1;		// [ 3: 3]
	uint32_t		pcd:1;		// [ 4: 4]
	uint32_t		a:1;		// [ 5: 5]
	uint32_t		d:1;		// [ 6: 6]
	uint32_t		ps:1;		// [ 7: 7] PS=1, or else this is just X86_PageDirEntryBlock
	uint32_t		g:1;		// [ 8: 8]
	uint32_t		avl:3;		// [11: 9]
	uint32_t		pat:1;		// [12:12]
	uint32_t		base32:8;	// [20:13] bits 39:32 if PSE36
	uint32_t		reserved:1;	// [21:21]
	uint32_t		base22:10;	// [31:22] bits 31:22
#endif
	inline PageNum getBase(const LinearPt lin_page) const {
		return 	PageNum(base22 << PageNum(10u)) +
			PageNum((base32 & enable_pse_extmask) << PageNum(20u)) +
			PageNum(lin_page & 0x3FFu);
	}
} GCC_ATTRIBUTE(packed);

#ifdef _MSC_VER
#pragma pack ()
#endif

union X86PageEntry {
	uint32_t load;
	X86_PageEntryBlock block;
	X86_PageDirEntryBlock dirblock;
	X86_PageDir4MBEntryBlock dirblock4mb; // PSE=1 and PS=1

	template <const uint8_t bitpos/*for constant compile time optimization*/> inline uint8_t accbits(void) const {
		if (bitpos == 0)
			return (load >> 1u) & 3u; /* ex: (load >> 1u) & 0x3 -> U/S:R/W->[1:0]*/
		else /* usually 2 */
			return (load << (bitpos - 1u)) & (3u << bitpos); /* ex: (load << 1u) & 0xc -> U/S:R/W->[3:2]*/
	}
};

static_assert( sizeof(X86PageEntry) == 4, "oops" );

struct PagingBlock {
	uint32_t		cr3;
	uint32_t		cr2;
	struct {
		PageNum page;
		PhysPt addr;
	} base;
	struct {
		HostPt read[TLB_SIZE];
		HostPt write[TLB_SIZE];
		PageHandler *readhandler[TLB_SIZE];
		PageHandler *writehandler[TLB_SIZE];
		tlbentry_t phys_page[TLB_SIZE];
	} tlb;
	struct {
		uint32_t used;
		uint32_t entries[PAGING_LINKS]; /* does not require more than 32 bits */
	} links;
	struct {
		uint32_t used;
		uint32_t entries[PAGING_LINKS]; /* does not require more than 32 bits */
	} ur_links;
	struct {
		uint32_t used;
		uint32_t entries[PAGING_LINKS]; /* does not require more than 32 bits */
	} krw_links;
	struct {
		uint32_t used;
		uint32_t entries[PAGING_LINKS]; /* does not require more than 32 bits */
	} kr_links; // WP-only
	uint32_t	firstmb[LINK_START]; /* does not use flags and does not reach beyond 1MB, does not need more than 32 bits */
	bool		enabled;
	bool		wp;
};

extern PagingBlock paging; 

/* Some support functions */

PageHandler * MEM_GetPageHandler(const Bitu phys_page);


/* Unaligned address handlers */
uint16_t mem_unalignedreadw(const LinearPt address);
uint32_t mem_unalignedreadd(const LinearPt address);
void mem_unalignedwritew(const LinearPt address,const uint16_t val);
void mem_unalignedwrited(const LinearPt address,const uint32_t val);

bool mem_unalignedreadw_checked(const LinearPt address,uint16_t * const val);
bool mem_unalignedreadd_checked(const LinearPt address,uint32_t * const val);
bool mem_unalignedwritew_checked(const LinearPt address,uint16_t const val);
bool mem_unalignedwrited_checked(const LinearPt address,uint32_t const val);

static INLINE HostPt get_tlb_read(const LinearPt address) {
	return paging.tlb.read[address>>12];
}
static INLINE HostPt get_tlb_write(const LinearPt address) {
	return paging.tlb.write[address>>12];
}
static INLINE PageHandler* get_tlb_readhandler(const LinearPt address) {
	return paging.tlb.readhandler[address>>12];
}
static INLINE PageHandler* get_tlb_writehandler(const LinearPt address) {
	return paging.tlb.writehandler[address>>12];
}

/* Use these helper functions to access linear addresses in readX/writeX functions */
/* NTS: 12-bit shift 32-bit constant, upper bits get shifted out, therefore no need to bitmask */
static INLINE PhysPt PAGING_GetPhysicalPage(const LinearPt linePage) {
	return (paging.tlb.phys_page[linePage>>12]<<12);
}

static INLINE PhysPt PAGING_GetPhysicalPageNumber(const LinearPt linePage) {
	return paging.tlb.phys_page[linePage>>12]&PHYSPAGE_ADDR;
}

/* NTS: 12-bit shift 32-bit constant, upper bits get shifted out, therefore no need to bitmask */
static INLINE PhysPt PAGING_GetPhysicalAddress(const LinearPt linAddr) {
	return (paging.tlb.phys_page[linAddr>>12]<<12)|(linAddr&0xfff);
}

static INLINE PhysPt64 PAGING_GetPhysicalAddress64(const LinearPt linAddr) {
	return ((PhysPt64)(paging.tlb.phys_page[linAddr>>12]&PHYSPAGE_ADDR)<<(PhysPt64)12)|(linAddr&0xfff);
}

/* Special inlined memory reading/writing */

static INLINE uint8_t mem_readb_inline(const LinearPt address) {
	const HostPt tlb_addr=get_tlb_read(address);
	if (tlb_addr) return host_readb(tlb_addr+address);
	else return (uint8_t)(get_tlb_readhandler(address))->readb(address);
}

static INLINE uint16_t mem_readw_inline(const LinearPt address) {
	if ((address & 0xfff)<0xfff) {
		const HostPt tlb_addr=get_tlb_read(address);
		if (tlb_addr) return host_readw(tlb_addr+address);
		else return (uint16_t)(get_tlb_readhandler(address))->readw(address);
	} else return mem_unalignedreadw(address);
}

static INLINE uint32_t mem_readd_inline(const LinearPt address) {
	if ((address & 0xfff)<0xffd) {
		const HostPt tlb_addr=get_tlb_read(address);
		if (tlb_addr) return host_readd(tlb_addr+address);
		else return (uint32_t)(get_tlb_readhandler(address))->readd(address);
	} else return mem_unalignedreadd(address);
}

static INLINE void mem_writeb_inline(const LinearPt address,const uint8_t val) {
	const HostPt tlb_addr=get_tlb_write(address);
	if (tlb_addr) host_writeb(tlb_addr+address,val);
	else (get_tlb_writehandler(address))->writeb(address,val);
}

static INLINE void mem_writew_inline(const LinearPt address,const uint16_t val) {
	if ((address & 0xfffu)<0xfffu) {
		const HostPt tlb_addr=get_tlb_write(address);
		if (tlb_addr) host_writew(tlb_addr+address,val);
		else (get_tlb_writehandler(address))->writew(address,val);
	} else mem_unalignedwritew(address,val);
}

static INLINE void mem_writed_inline(const LinearPt address,const uint32_t val) {
	if ((address & 0xfffu)<0xffdu) {
		const HostPt tlb_addr=get_tlb_write(address);
		if (tlb_addr) host_writed(tlb_addr+address,val);
		else (get_tlb_writehandler(address))->writed(address,val);
	} else mem_unalignedwrited(address,val);
}


static INLINE bool mem_readb_checked(const LinearPt address, uint8_t * const val) {
	const HostPt tlb_addr=get_tlb_read(address);
	if (tlb_addr) {
		*val=host_readb(tlb_addr+address);
		return false;
	} else return (get_tlb_readhandler(address))->readb_checked(address, val);
}

static INLINE bool mem_readw_checked(const LinearPt address, uint16_t * const val) {
	if ((address & 0xfffu)<0xfffu) {
		const HostPt tlb_addr=get_tlb_read(address);
		if (tlb_addr) {
			*val=host_readw(tlb_addr+address);
			return false;
		} else return (get_tlb_readhandler(address))->readw_checked(address, val);
	} else return mem_unalignedreadw_checked(address, val);
}

static INLINE bool mem_readd_checked(const LinearPt address, uint32_t * const val) {
	if ((address & 0xfffu)<0xffdu) {
		const HostPt tlb_addr=get_tlb_read(address);
		if (tlb_addr) {
			*val=host_readd(tlb_addr+address);
			return false;
		} else return (get_tlb_readhandler(address))->readd_checked(address, val);
	} else return mem_unalignedreadd_checked(address, val);
}

static INLINE bool mem_writeb_checked(const LinearPt address,const uint8_t val) {
	const HostPt tlb_addr=get_tlb_write(address);
	if (tlb_addr) {
		host_writeb(tlb_addr+address,val);
		return false;
	} else return (get_tlb_writehandler(address))->writeb_checked(address,val);
}

static INLINE bool mem_writew_checked(const LinearPt address,const uint16_t val) {
	if ((address & 0xfffu)<0xfffu) {
		const HostPt tlb_addr=get_tlb_write(address);
		if (tlb_addr) {
			host_writew(tlb_addr+address,val);
			return false;
		} else return (get_tlb_writehandler(address))->writew_checked(address,val);
	} else return mem_unalignedwritew_checked(address,val);
}

static INLINE bool mem_writed_checked(const LinearPt address,const uint32_t val) {
	if ((address & 0xfffu)<0xffdu) {
		const HostPt tlb_addr=get_tlb_write(address);
		if (tlb_addr) {
			host_writed(tlb_addr+address,val);
			return false;
		} else return (get_tlb_writehandler(address))->writed_checked(address,val);
	} else return mem_unalignedwrited_checked(address,val);
}

extern bool dosbox_allow_nonrecursive_page_fault;	/* when set, do nonrecursive mode (when executing instruction) */

class GuestPageFaultException : public std::exception {
public:
	const char *what() const throw() override {
		return "Guest page fault exception";
	}
	GuestPageFaultException(LinearPt n_lin_addr, Bitu n_page_addr, Bitu n_faultcode) : lin_addr(n_lin_addr), page_addr(n_page_addr), faultcode(n_faultcode) {
	}
public:
	LinearPt lin_addr;
	Bitu page_addr;
	Bitu faultcode;
};

class GuestGenFaultException : public std::exception {
public:
	virtual const char *what() const throw() override {
		return "Guest general protection fault exception";
	}
	GuestGenFaultException() {
	}
};

uint8_t PageHandler_HostPtReadB(PageHandler *p,PhysPt addr);
uint16_t PageHandler_HostPtReadW(PageHandler *p,PhysPt addr);
uint32_t PageHandler_HostPtReadD(PageHandler *p,PhysPt addr);
void PageHandler_HostPtWriteB(PageHandler *p,PhysPt addr,uint8_t val);
void PageHandler_HostPtWriteW(PageHandler *p,PhysPt addr,uint16_t val);
void PageHandler_HostPtWriteD(PageHandler *p,PhysPt addr,uint32_t val);

#endif
