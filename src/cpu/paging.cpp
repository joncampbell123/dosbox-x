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


#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "paging.h"
#include "lazyflags.h"
#include "cpu.h"
#include "logging.h"

extern bool do_pse;
extern bool enable_pse;
extern uint8_t enable_pse_extbits;
extern uint8_t enable_pse_extmask;

extern bool dos_kernel_disabled;
PagingBlock paging;

// Pagehandler implementation
uint8_t PageHandler::readb(PhysPt addr) {
	E_Exit("No byte handler for read from %x",addr);	
	return 0;
}
uint16_t PageHandler::readw(PhysPt addr) {
	uint16_t ret = (readb(addr+0) << 0);
	ret     |= (readb(addr+1) << 8);
	return ret;
}
uint32_t PageHandler::readd(PhysPt addr) {
    uint32_t ret       = ((uint32_t)readb(addr+0) << 0u);
    ret             |= ((uint32_t)readb(addr+1) << 8u);
    ret             |= ((uint32_t)readb(addr+2) << 16u);
    ret             |= ((uint32_t)readb(addr+3) << 24u);
    return ret;
}

void PageHandler::writeb(PhysPt addr,uint8_t /*val*/) {
	E_Exit("No byte handler for write to %x",addr);	
}

void PageHandler::writew(PhysPt addr,uint16_t val) {
	writeb(addr+0,(uint8_t) (val >> 0));
	writeb(addr+1,(uint8_t) (val >> 8));
}
void PageHandler::writed(PhysPt addr,uint32_t val) {
	writeb(addr+0,(uint8_t) (val >> 0));
	writeb(addr+1,(uint8_t) (val >> 8));
	writeb(addr+2,(uint8_t) (val >> 16));
	writeb(addr+3,(uint8_t) (val >> 24));
}

HostPt PageHandler::GetHostReadPt(PageNum /*phys_page*/) {
	return nullptr;
}

HostPt PageHandler::GetHostWritePt(PageNum /*phys_page*/) {
	return nullptr;
}

bool PageHandler::readb_checked(PhysPt addr, uint8_t * val) {
	*val=readb(addr);	return false;
}
bool PageHandler::readw_checked(PhysPt addr, uint16_t * val) {
	*val=readw(addr);	return false;
}
bool PageHandler::readd_checked(PhysPt addr, uint32_t * val) {
	*val=readd(addr);	return false;
}
bool PageHandler::writeb_checked(PhysPt addr,uint8_t val) {
	writeb(addr,val);	return false;
}
bool PageHandler::writew_checked(PhysPt addr,uint16_t val) {
	writew(addr,val);	return false;
}
bool PageHandler::writed_checked(PhysPt addr,uint32_t val) {
	writed(addr,val);	return false;
}



struct PF_Entry {
	Bitu cs;
	Bitu eip;
	Bitu page_addr;
	Bitu mpl;
};

#define PF_QUEUESIZE 80
static struct {
	Bitu used;
	PF_Entry entries[PF_QUEUESIZE];
} pf_queue;

Bits PageFaultCore(void) {
	CPU_CycleLeft+=CPU_Cycles;
	CPU_Cycles=1;
	Bits ret=CPU_Core_Full_Run(); // FIXME: What is the Full core doing right that Normal core is doing wrong here?
	CPU_CycleLeft+=CPU_Cycles;
	if (ret<0) E_Exit("Got a dosbox close machine in pagefault core?");
	if (ret) 
		return ret;
	if (!pf_queue.used) E_Exit("PF Core without PF");
	const PF_Entry* entry = &pf_queue.entries[pf_queue.used - 1];
	X86PageEntry pentry;
	pentry.load=phys_readd((PhysPt)entry->page_addr);
	if (pentry.block.p && entry->cs == SegValue(cs) && entry->eip==reg_eip) {
		cpu.mpl=entry->mpl;
		return -1;
	}
	return 0;
}
#if C_DEBUG
Bitu DEBUG_EnableDebugger(void);
#endif

#define ACCESS_KR  0
#define ACCESS_KRW 1
#define ACCESS_UR  2
#define ACCESS_URW 3
#define ACCESS_TABLEFAULT 4
//const char* const mtr[] = {"KR ","KRW","UR ","URW","PFL"};

// bit0 entry write
// bit1 entry access
// bit2 table write
// bit3 table access
// These arrays define how the access bits in the page table and entry
// result in access rights.
// The used array is switched depending on the CPU under emulation.

// Intel says the lowest numeric value wins for both 386 and 486+
// There's something strange about KR with WP=1 though
static const uint8_t translate_array[] = {
	ACCESS_KR,		// 00 00
	ACCESS_KR,		// 00 01
	ACCESS_KR,		// 00 10
	ACCESS_KR,		// 00 11
	ACCESS_KR,		// 01 00
	ACCESS_KRW,		// 01 01
	ACCESS_KR, //	ACCESS_KRW,		// 01 10
	ACCESS_KRW,		// 01 11
	ACCESS_KR,		// 10 00
	ACCESS_KR, //	ACCESS_KRW,		// 10 01
	ACCESS_UR,		// 10 10
	ACCESS_UR,		// 10 11
	ACCESS_KR,		// 11 00
	ACCESS_KRW,		// 11 01
	ACCESS_UR,		// 11 10
	ACCESS_URW		// 11 11
};

static inline uint8_t page_access_bits(const X86PageEntry &dir_entry, const X86PageEntry &table_entry) {
	return translate_array[dir_entry.accbits<2>()+table_entry.accbits<0>()];
}

static inline uint8_t page_access_bits(const X86PageEntry &dir_entry) {
	return translate_array[dir_entry.accbits<2>()+dir_entry.accbits<0>()];
}

// This array defines how a page is mapped depending on 
// page access right, cpl==3, and WP.
// R = map handler as read, W = map handler as write, E = map exception handler
#define ACMAP_RW 0
#define ACMAP_RE 1
#define ACMAP_EE 2

//static const char* const lnm[] = {"RW ","RE ","EE "}; // debug stuff

// bit0-1 ACCESS_ type
// bit2   1=user mode
// bit3   WP on

static const uint8_t xlat_mapping[] = {
//  KR        KRW       UR        URW
	// index 0-3   kernel, wp 0
	ACMAP_RW, ACMAP_RW, ACMAP_RW, ACMAP_RW,
	// index 4-7   user,   wp 0
	ACMAP_EE, ACMAP_EE, ACMAP_RE, ACMAP_RW,
	// index 8-11  kernel, wp 1
	ACMAP_RE, ACMAP_RW, ACMAP_RE, ACMAP_RW,
	// index 11-15 user,   wp 1 (same as user, wp 0)
	ACMAP_EE, ACMAP_EE, ACMAP_RE, ACMAP_RW,
};

// This table can figure out if we are going to fault right now in the init handler
// (1=fault) 
// bit0-1 ACCESS_ type
// bit2   1=user mode
// bit3   1=writing
// bit4   wp

static const uint8_t fault_table[] = {
//	KR	KRW	UR	URW
	// WP 0
	// index 0-3   kernel, reading
	0,	0,	0,	0,
	// index 4-7   user,   reading
	1,	1,	0,	0,
	// index 8-11  kernel, writing
	0,	0,	0,	0,
	// index 11-15 user,   writing
	1,	1,	1,	0,
	// WP 1
	// index 0-3   kernel, reading
	0,	0,	0,	0,
	// index 4-7   user,   reading
	1,	1,	0,	0,
	// index 8-11  kernel, writing
	1,	0,	1,	0,
	// index 11-15 user,   writing
	1,	1,	1,	0,
};

// helper functions for calculating table entry addresses
//
// Linear addr: 33222222222211111111110000000000
//              10987654321098765432109876543210
//
// didx         DDDDDDDDDD......................
// dirent_addr  (didx * 4) + (CR3[31:12] << 12)
// dirent       *((uint32_t*)dirent_addr)
//
// tidx         ..........TTTTTTTTTT............
// tblent_addr  (dirent[31:12] << 12) + (tidx * 4)
// tblent       *((uint32_t*)tblent_addr)
//
// pageoff      ....................OOOOOOOOOOOO
// physoff      (tblent[31:12] << 12) + pageoff      <- final step
//
static inline PhysPt GetPageDirectoryEntryAddr(LinearPt lin_addr) {
	return PhysPt(paging.base.addr + ((lin_addr >> 20u) & 0xffcu)); /* equiv: ((lin_addr >> 22) & 0x3ff) * 4 */
}
static inline PhysPt GetPageTableEntryAddr(LinearPt lin_addr, const X86PageEntry& dir_entry) {
	return PhysPt(dir_entry.dirblock.base << PhysPt(12u)) + PhysPt((lin_addr >> LinearPt(10u)) & 0xffcu); /* equiv: ((lin_addr >> 12) & 0x3ff) * 4 */
}

bool use_dynamic_core_with_paging = false; /* allow dynamic core even with paging (AT YOUR OWN RISK!!!!) */
bool auto_determine_dynamic_core_paging = false; /* enable use_dynamic_core_with_paging when paging is enabled */
bool dosbox_allow_nonrecursive_page_fault = false;	/* when set, do nonrecursive mode (when executing instruction) */

void PAGING_PageFault(PhysPt lin_addr,Bitu page_addr,Bitu faultcode) {
	/* Save the state of the cpu cores */
	LazyFlags old_lflags;
	memcpy(&old_lflags,&lflags,sizeof(LazyFlags));
	CPU_Decoder * old_cpudecoder;
	old_cpudecoder=cpudecoder;
	cpudecoder=&PageFaultCore;
	paging.cr2=lin_addr;
	PF_Entry * entry=&pf_queue.entries[pf_queue.used++];
	LOG(LOG_PAGING,LOG_NORMAL)("PageFault at %lX type [%lx] queue %d",(unsigned long)lin_addr,(unsigned long)faultcode,(int)pf_queue.used);
//	LOG_MSG("EAX:%04X ECX:%04X EDX:%04X EBX:%04X",reg_eax,reg_ecx,reg_edx,reg_ebx);
//	LOG_MSG("CS:%04X EIP:%08X SS:%04x SP:%08X",SegValue(cs),reg_eip,SegValue(ss),reg_esp);
	entry->cs=SegValue(cs);
	entry->eip=reg_eip;
	entry->page_addr=page_addr;/*misnamed: this is the physical memory address of the page table entry to update/check */
	entry->mpl=cpu.mpl;
	cpu.mpl=3;

	CPU_Exception(EXCEPTION_PF,faultcode);
#if C_DEBUG
//	DEBUG_EnableDebugger();
#endif
	DOSBOX_RunMachine();
	pf_queue.used--;
	LOG(LOG_PAGING,LOG_NORMAL)("Left PageFault for %lx queue %d",(unsigned long)lin_addr,(int)pf_queue.used);
	memcpy(&lflags,&old_lflags,sizeof(LazyFlags));
	cpudecoder=old_cpudecoder;
//	LOG_MSG("SS:%04x SP:%08X",SegValue(ss),reg_esp);
}

// PAGING_NewPageFault
// lin_addr, page_addr: the linear and page address the fault happened at
// prepare_only: true in case the calling core handles the fault, else the PageFaultCore does
// NTS: page_addr is misnamed, it is the physical memory address of the page table entry to update/check.
//      furthermore for 4MB pages it is the page directory entry representing the 4MB page.
static void PAGING_NewPageFault(PhysPt lin_addr, Bitu page_addr, bool prepare_only, Bitu faultcode) {
	paging.cr2=lin_addr;
	//LOG_MSG("FAULT q%d, code %x",  pf_queue.used, faultcode);
	//PrintPageInfo("FA+",lin_addr,faultcode, prepare_only);

	if (prepare_only) {
		cpu.exception.which = EXCEPTION_PF;
		cpu.exception.error = faultcode;
	} else if (dosbox_allow_nonrecursive_page_fault) {
		throw GuestPageFaultException(lin_addr,page_addr,faultcode);
	} else {
		// Save the state of the cpu cores
		LazyFlags old_lflags;
		memcpy(&old_lflags,&lflags,sizeof(LazyFlags));
		CPU_Decoder * old_cpudecoder;
		old_cpudecoder=cpudecoder;
		cpudecoder=&PageFaultCore;
		LOG(LOG_PAGING,LOG_NORMAL)("Recursive PageFault for %lx used=%d",(unsigned long)lin_addr,(int)pf_queue.used);
		if (pf_queue.used >= PF_QUEUESIZE) E_Exit("PF queue overrun.");
		if (pf_queue.used != 0) LOG_MSG("Warning: PAGING_NewPageFault() more than one level, now using level %d\n",(int)pf_queue.used+1);
		PF_Entry * entry=&pf_queue.entries[pf_queue.used++];
		entry->cs=SegValue(cs);
		entry->eip=reg_eip;
		entry->page_addr=page_addr;
		entry->mpl=cpu.mpl;
		cpu.mpl=3;
		CPU_Exception(EXCEPTION_PF,faultcode);
		DOSBOX_RunMachine();
		pf_queue.used--;
		memcpy(&lflags,&old_lflags,sizeof(LazyFlags));
		cpudecoder=old_cpudecoder;
	}
}

class PageFoilHandler : public PageHandler {
private:
	void work(PhysPt addr) {
		const PageNum lin_page = PageNum(addr >> 12u);
		const PageNum phys_page = PageNum(paging.tlb.phys_page[lin_page] & PHYSPAGE_ADDR);
		X86PageEntry dir_entry, table_entry;
			
		// set the page dirty in the tlb
		paging.tlb.phys_page[lin_page] |= PHYSPAGE_DIRTY;

		// mark the page table entry dirty
		const PhysPt dirEntryAddr = GetPageDirectoryEntryAddr(addr);
		dir_entry.load = phys_readd(dirEntryAddr);
		if (!dir_entry.dirblock.p) E_Exit("PageFoilHandler unexpected directory entry not present (line %d)",__LINE__);

		if (do_pse && dir_entry.dirblock.ps) {
			// for debugging...
			if (dir_entry.dirblock4mb.getBase(lin_page) != phys_page)
				E_Exit("PageFoilHandler unexpected page address changed from TLB copy (line %d)",__LINE__);

			// set the dirty bit
			if (!dir_entry.block.d) {
				dir_entry.block.d = 1;
				phys_writed(dirEntryAddr,dir_entry.load);
			}
		}
		else {
			const PhysPt tableEntryAddr = GetPageTableEntryAddr(addr, dir_entry);
			table_entry.load = phys_readd(tableEntryAddr);
			if (!table_entry.block.p) E_Exit("PageFoilHandler unexpecteed table entry not present (line %d)",__LINE__);

			// for debugging...
			if (table_entry.block.base != phys_page)
				E_Exit("PageFoilHandler unexpected page address changed from TLB copy (line %d)",__LINE__);

			// this can happen when the same page table is used at two different
			// page directory entries / linear locations (WfW311)
			// if (table_entry.block.d) E_Exit("% 8x Already dirty!!",table_entry.load);

			// set the dirty bit
			if (!table_entry.block.d) {
				table_entry.block.d = 1;
				phys_writed(tableEntryAddr,table_entry.load);
			}
		}

		// replace this handler with the real thing
		PageHandler* const handler = MEM_GetPageHandler(phys_page);
		if (handler->getFlags() & PFLAG_WRITEABLE)
			paging.tlb.write[lin_page] = handler->GetHostWritePt(phys_page) - (lin_page << 12);
		else
			paging.tlb.write[lin_page] = nullptr;

		paging.tlb.writehandler[lin_page] = handler;
	}

	void read() {
		E_Exit("The page foiler shouldn't be read.");
	}
public:
	PageFoilHandler() : PageHandler(PFLAG_INIT|PFLAG_NOCODE) {}
	uint8_t readb(PhysPt addr) override {(void)addr;read();return 0;}
	uint16_t readw(PhysPt addr) override {(void)addr;read();return 0;}
	uint32_t readd(PhysPt addr) override {(void)addr;read();return 0;}

	void writeb(PhysPt addr,uint8_t val) override {
		work(addr);
		// execute the write:
		// no need to care about mpl because we won't be entered
		// if write isn't allowed
		mem_writeb(addr,val);
	}
	void writew(PhysPt addr,uint16_t val) override {
		work(addr);
		mem_writew(addr,val);
	}
	void writed(PhysPt addr,uint32_t val) override {
		work(addr);
		mem_writed(addr,val);
	}

	bool readb_checked(PhysPt addr, uint8_t * val) override {(void)addr;(void)val;read();return true;}
	bool readw_checked(PhysPt addr, uint16_t * val) override {(void)addr;(void)val;read();return true;}
	bool readd_checked(PhysPt addr, uint32_t * val) override {(void)addr;(void)val;read();return true;}

	bool writeb_checked(PhysPt addr,uint8_t val) override {
		work(addr);
		mem_writeb(addr,val);
		return false;
	}
	bool writew_checked(PhysPt addr,uint16_t val) override {
		work(addr);
		mem_writew(addr,val);
		return false;
	}
	bool writed_checked(PhysPt addr,uint32_t val) override {
		work(addr);
		mem_writed(addr,val);
		return false;
	}
};

class ExceptionPageHandler : public PageHandler {
private:
	PageHandler* getHandler(PhysPt addr) {
		const PageNum lin_page = PageNum(addr >> 12u);
		const PageNum phys_page = paging.tlb.phys_page[lin_page] & PHYSPAGE_ADDR;
		PageHandler* const handler = MEM_GetPageHandler(phys_page);
		return handler;
	}

	bool hack_check(PhysPt addr) {
		// First Encounters
		//
		// They change the page attributes without clearing the TLB.
		// On a real 486 they get away with it because its TLB has only 32 or so 
		// elements. The changed page attribs get overwritten and re-read before
		// the exception happens. Here we have gazillions of TLB entries so the
		// exception occurs if we don't check for it.

		// NTS 2024/12/15: Checking with 7-cpu.com, the Pentium also has 32,
		// the Pentium II and III has 64. You would need to go to a pretty late
		// level of CPU to find something that would probably cause this game to
		// fail from page table laziness. However, to simplify this code, this
		// check is skipped over if PSE or PAE are enabled because DOS games are
		// unlikely to use those page table features. --J.C.

		if (!do_pse) {
			const uint8_t old_attirbs = uint8_t(paging.tlb.phys_page[addr>>12] >> PHYSPAGE_ACCESS_BITS_SHIFT);
			X86PageEntry dir_entry, table_entry;

			dir_entry.load = phys_readd(GetPageDirectoryEntryAddr(addr));
			if (!dir_entry.dirblock.p) return false;
			table_entry.load = phys_readd(GetPageTableEntryAddr(addr, dir_entry));
			if (!table_entry.block.p) return false;

			const uint8_t result = page_access_bits(dir_entry, table_entry);
			if (result != old_attirbs) return true;
		}

		return false;
	}

	void Exception(PhysPt addr, bool writing, bool checked) {
		//PrintPageInfo("XCEPT",addr,writing, checked);
		//LOG_MSG("XCEPT LIN% 8x wr%d, ch%d, cpl%d, mpl%d",addr, writing, checked, cpu.cpl, cpu.mpl);
		PhysPt tableaddr = 0;
		if (!checked) {
			X86PageEntry dir_entry;
			dir_entry.load = phys_readd(GetPageDirectoryEntryAddr(addr));
			if (!dir_entry.dirblock.p) E_Exit("Undesired situation 1 in exception handler.");

			if (do_pse && dir_entry.dirblock.ps) E_Exit("PSE and Exception not yet supported");//TODO

			// page table entry
			tableaddr = GetPageTableEntryAddr(addr, dir_entry);
			//Bitu d_index=(addr >> 12) >> 10;
			//tableaddr=(paging.base.page<<12) | (d_index<<2);
		} 
		PAGING_NewPageFault(addr, tableaddr, checked,
			1u | (writing ? 2u : 0u) | (((cpu.cpl&cpu.mpl) == 3u) ? 4u : 0u));
		
		PAGING_ClearTLB(); // TODO got a better idea?
	}

	uint8_t readb_through(PhysPt addr) {
		Bitu lin_page = addr >> 12;
		uint32_t phys_page = paging.tlb.phys_page[lin_page] & PHYSPAGE_ADDR;
		PageHandler* handler = MEM_GetPageHandler(phys_page);
		if (handler->getFlags() & PFLAG_READABLE) {
			return host_readb(handler->GetHostReadPt(phys_page) + (addr&0xfff));
		}
		else return handler->readb(addr);
	}
	uint16_t readw_through(PhysPt addr) {
		Bitu lin_page = addr >> 12;
		uint32_t phys_page = paging.tlb.phys_page[lin_page] & PHYSPAGE_ADDR;
		PageHandler* handler = MEM_GetPageHandler(phys_page);
		if (handler->getFlags() & PFLAG_READABLE) {
			return host_readw(handler->GetHostReadPt(phys_page) + (addr&0xfff));
		}
		else return handler->readw(addr);
	}
	uint32_t readd_through(PhysPt addr) {
		Bitu lin_page = addr >> 12;
		uint32_t phys_page = paging.tlb.phys_page[lin_page] & PHYSPAGE_ADDR;
		PageHandler* handler = MEM_GetPageHandler(phys_page);
		if (handler->getFlags() & PFLAG_READABLE) {
			return host_readd(handler->GetHostReadPt(phys_page) + (addr&0xfff));
		}
		else return handler->readd(addr);
	}

	void writeb_through(PhysPt addr, uint8_t val) {
		Bitu lin_page = addr >> 12;
		uint32_t phys_page = paging.tlb.phys_page[lin_page] & PHYSPAGE_ADDR;
		PageHandler* handler = MEM_GetPageHandler(phys_page);
		if (handler->getFlags() & PFLAG_WRITEABLE) {
			return host_writeb(handler->GetHostWritePt(phys_page) + (addr&0xfff), val);
		}
		else return handler->writeb(addr, val);
	}

	void writew_through(PhysPt addr, uint16_t val) {
		Bitu lin_page = addr >> 12;
		uint32_t phys_page = paging.tlb.phys_page[lin_page] & PHYSPAGE_ADDR;
		PageHandler* handler = MEM_GetPageHandler(phys_page);
		if (handler->getFlags() & PFLAG_WRITEABLE) {
			return host_writew(handler->GetHostWritePt(phys_page) + (addr&0xfff), val);
		}
		else return handler->writew(addr, val);
	}

	void writed_through(PhysPt addr, uint32_t val) {
		Bitu lin_page = addr >> 12;
		uint32_t phys_page = paging.tlb.phys_page[lin_page] & PHYSPAGE_ADDR;
		PageHandler* handler = MEM_GetPageHandler(phys_page);
		if (handler->getFlags() & PFLAG_WRITEABLE) {
			return host_writed(handler->GetHostWritePt(phys_page) + (addr&0xfff), val);
		}
		else return handler->writed(addr, val);
	}

public:
	ExceptionPageHandler() : PageHandler(PFLAG_INIT|PFLAG_NOCODE) {}
	uint8_t readb(PhysPt addr) override {
		if (!cpu.mpl) return readb_through(addr);
			
		Exception(addr, false, false);
		return mem_readb(addr); // read the updated page (unlikely to happen?)
				}
	uint16_t readw(PhysPt addr) override {
		// access type is supervisor mode (temporary)
		// we are always allowed to read in superuser mode
		// so get the handler and address and read it
		if (!cpu.mpl) return readw_through(addr);

		Exception(addr, false, false);
		return mem_readw(addr);
			}
	uint32_t readd(PhysPt addr) override {
		if (!cpu.mpl) return readd_through(addr);

		Exception(addr, false, false);
		return mem_readd(addr);
		}
	void writeb(PhysPt addr,uint8_t val) override {
		if (!cpu.mpl) {
			writeb_through(addr, val);
			return;
	}
		Exception(addr, true, false);
		mem_writeb(addr, val);
			}
	void writew(PhysPt addr,uint16_t val) override {
		if (!cpu.mpl) {
			// TODO Exception on a KR-page?
			writew_through(addr, val);
			return;
		}
		if (hack_check(addr)) {
			LOG_MSG("Page attributes modified without clear");
			PAGING_ClearTLB();
			mem_writew(addr,val);
			return;
		}
		// firstenc here
		Exception(addr, true, false);
		mem_writew(addr, val);
	}
	void writed(PhysPt addr,uint32_t val) override {
		if (!cpu.mpl) {
			writed_through(addr, val);
			return;
		}
		Exception(addr, true, false);
		mem_writed(addr, val);
	}
	// returning true means an exception was triggered for these _checked functions
	bool readb_checked(PhysPt addr, uint8_t * val) override {
        (void)val;//UNUSED
		Exception(addr, false, true);
		return true;
	}
	bool readw_checked(PhysPt addr, uint16_t * val) override {
        (void)val;//UNUSED
		Exception(addr, false, true);
		return true;
			}
	bool readd_checked(PhysPt addr, uint32_t * val) override {
        (void)val;//UNUSED
		Exception(addr, false, true);
		return true;
		}
	bool writeb_checked(PhysPt addr,uint8_t val) override {
        (void)val;//UNUSED
		Exception(addr, true, true);
		return true;
	}
	bool writew_checked(PhysPt addr,uint16_t val) override {
		if (hack_check(addr)) {
			LOG_MSG("Page attributes modified without clear");
			PAGING_ClearTLB();
			mem_writew(addr,val); // TODO this makes us recursive again?
			return false;
		}
		Exception(addr, true, true);
		return true;
	}
	bool writed_checked(PhysPt addr,uint32_t val) override {
        (void)val;//UNUSED
		Exception(addr, true, true);
		return true;
	}
};

static void PAGING_LinkPageNew(PageNum lin_page, PageNum phys_page, const uint8_t linkmode, bool dirty);

static INLINE void InitPageCheckPresence(PhysPt lin_addr,bool writing,X86PageEntry& table,X86PageEntry& entry) {
	Bitu lin_page=lin_addr >> 12;
	Bitu d_index=lin_page >> 10;
	Bitu t_index=lin_page & 0x3ff;
	Bitu table_addr=(paging.base.page<<12)+d_index*4;
	table.load=phys_readd((PhysPt)table_addr);
	if (!table.block.p) {
		LOG(LOG_PAGING,LOG_NORMAL)("NP Table");
		PAGING_PageFault(lin_addr,table_addr,
			(writing?0x02:0x00) | (((cpu.cpl&cpu.mpl)==0)?0x00:0x04));
		table.load=phys_readd((PhysPt)table_addr);
		if (GCC_UNLIKELY(!table.block.p))
			E_Exit("Pagefault didn't correct table");
	}

	if (do_pse && table.dirblock.ps) { // 4MB PSE page
		entry.load=0;
	}
	else {
		Bitu entry_addr=(table.block.base<<12)+t_index*4;
		entry.load=phys_readd((PhysPt)entry_addr);
		if (!entry.block.p) {
//			LOG(LOG_PAGING,LOG_NORMAL)("NP Page");
			PAGING_PageFault(lin_addr,entry_addr,
				(writing?0x02:0x00) | (((cpu.cpl&cpu.mpl)==0)?0x00:0x04));
			entry.load=phys_readd((PhysPt)entry_addr);
			if (GCC_UNLIKELY(!entry.block.p))
				E_Exit("Pagefault didn't correct page");
		}
	}
}

static INLINE bool InitPageCheckPresence_CheckOnly(PhysPt lin_addr,bool writing,X86PageEntry& table,X86PageEntry& entry) {
	Bitu lin_page=lin_addr >> 12;
	Bitu d_index=lin_page >> 10;
	Bitu t_index=lin_page & 0x3ff;
	Bitu table_addr=(paging.base.page<<12)+d_index*4;
	table.load=phys_readd((PhysPt)table_addr);
	if (!table.block.p) {
		paging.cr2=lin_addr;
		cpu.exception.which=EXCEPTION_PF;
		cpu.exception.error=(writing?0x02:0x00) | (((cpu.cpl&cpu.mpl)==0)?0x00:0x04);
		return false;
	}

	if (do_pse && table.dirblock.ps) { // 4MB PSE page
	}
	else {
		Bitu entry_addr=(table.block.base<<12)+t_index*4;
		entry.load=phys_readd((PhysPt)entry_addr);
		if (!entry.block.p) {
			paging.cr2=lin_addr;
			cpu.exception.which=EXCEPTION_PF;
			cpu.exception.error=(writing?0x02:0x00) | (((cpu.cpl&cpu.mpl)==0)?0x00:0x04);
			return false;
		}
	}

	return true;
}

// check if a user-level memory access would trigger a privilege page fault
static INLINE bool InitPage_CheckUseraccess(Bitu u1,Bitu u2) {
	switch (CPU_ArchitectureType) {
	case CPU_ARCHTYPE_MIXED:
	case CPU_ARCHTYPE_386:
	default:
		return ((u1)==0) && ((u2)==0);
	case CPU_ARCHTYPE_486OLD:
	case CPU_ARCHTYPE_486NEW:
	case CPU_ARCHTYPE_PENTIUM:
		return ((u1)==0) || ((u2)==0);
	}
}

class NewInitPageHandler : public PageHandler {
public:
	NewInitPageHandler() : PageHandler(PFLAG_INIT|PFLAG_NOCODE) {}
	uint8_t readb(PhysPt addr) override {
		InitPage(addr, false, false);
		return mem_readb(addr);
	}
	uint16_t readw(PhysPt addr) override {
		InitPage(addr, false, false);
		return mem_readw(addr);
	}
	uint32_t readd(PhysPt addr) override {
		InitPage(addr, false, false);
		return mem_readd(addr);
	}
	void writeb(PhysPt addr,uint8_t val) override {
		InitPage(addr, true, false);
		mem_writeb(addr,val);
	}
	void writew(PhysPt addr,uint16_t val) override {
		InitPage(addr, true, false);
		mem_writew(addr,val);
	}
	void writed(PhysPt addr,uint32_t val) override {
		InitPage(addr, true, false);
		mem_writed(addr,val);
	}

	bool readb_checked(PhysPt addr, uint8_t * val) override {
		if (InitPage(addr, false, true)) return true;
		*val=mem_readb(addr);
			return false;
		}
	bool readw_checked(PhysPt addr, uint16_t * val) override {
		if (InitPage(addr, false, true)) return true;
		*val=mem_readw(addr);
		return false;
	}
	bool readd_checked(PhysPt addr, uint32_t * val) override {
		if (InitPage(addr, false, true)) return true;
		*val=mem_readd(addr);
			return false;
		}
	bool writeb_checked(PhysPt addr,uint8_t val) override {
		if (InitPage(addr, true, true)) return true;
		mem_writeb(addr,val);
		return false;
	}
	bool writew_checked(PhysPt addr,uint16_t val) override {
		if (InitPage(addr, true, true)) return true;
		mem_writew(addr,val);
			return false;
		}
	bool writed_checked(PhysPt addr,uint32_t val) override {
		if (InitPage(addr, true, true)) return true;
		mem_writed(addr,val);
		return false;
	}
	bool InitPage(PhysPt lin_addr, bool writing, bool prepare_only) {
		const PageNum lin_page = PageNum(lin_addr >> 12);
		PageNum phys_page;

		if (paging.enabled) {
initpage_retry:
			X86PageEntry dir_entry, table_entry;
			const bool isUser = (((cpu.cpl & cpu.mpl)==3)? true:false);

			// Read the paging stuff, throw not present exceptions if needed
			// and find out how the page should be mapped
			const PhysPt dirEntryAddr = GetPageDirectoryEntryAddr(lin_addr);
			// Range check to avoid emulator segfault: phys_readd() reads from MemBase+addr and does NOT range check.
			// Needed to avoid segfault when running 1999 demo "Void Main" in a bootable Windows 95 image in pure DOS mode.
			// 2024/12/22: phys_readx() does range checking now
			dir_entry.load = phys_readd(dirEntryAddr);

			if (!dir_entry.dirblock.p) {
				// table pointer is not present, do a page fault
				PAGING_NewPageFault(lin_addr, dirEntryAddr, prepare_only,
					(writing ? 2u : 0u) | (isUser ? 4u : 0u));

				if (prepare_only) return true;
				else goto initpage_retry; // TODO maybe E_Exit after a few loops
			}

			if (do_pse && dir_entry.dirblock.ps) { // 4MB PSE page
				const uint8_t result = page_access_bits(dir_entry);

				// save load to see if it changed later
				const uint32_t dir_load = dir_entry.load;

				// if we are writing we can set it right here to save some CPU
				if (writing) dir_entry.dirblock4mb.d = 1;

				// set page accessed
				dir_entry.dirblock4mb.a = 1;

				// update if needed
				if (dir_load != dir_entry.load)
					phys_writed(dirEntryAddr, dir_entry.load);

				// if the page isn't dirty and we are reading we need to map the foiler
				// (dirty = false)
				const bool dirty = dir_entry.dirblock4mb.d ? true : false;

				/* LOG_MSG("INITPSE lin=0x%x phys=0x%lx base22=0x%x base32=0x%x",
					(unsigned int)lin_addr,
					(unsigned long)(((dir_entry.dirblock4mb.base22<<10ul)|((dir_entry.dirblock4mb.base32&enable_pse_extmask)<<20ul)|(lin_page&0x3FFul))<<12ul),
					(unsigned int)dir_entry.dirblock4mb.base22,
					(unsigned int)dir_entry.dirblock4mb.base32); */
				// finally install the new page
				PAGING_LinkPageNew(lin_page, dir_entry.dirblock4mb.getBase(lin_page), result, dirty);
			}
			else {
				const PhysPt tableEntryAddr = GetPageTableEntryAddr(lin_addr, dir_entry);
				// Range check to avoid emulator segfault: phys_readd() reads from MemBase+addr and does NOT range check.
				// 2024/12/22: phys_readx() does range checking now
				table_entry.load = phys_readd(tableEntryAddr);

				// set page table accessed (IA manual: A is set whenever the entry is 
				// used in a page translation)
				if (!dir_entry.dirblock.a) {
					dir_entry.dirblock.a = 1;
					phys_writed(dirEntryAddr, dir_entry.load);
				}

				if (!table_entry.block.p) {
					// physpage pointer is not present, do a page fault
					PAGING_NewPageFault(lin_addr, tableEntryAddr, prepare_only,
						(writing ? 2u : 0u) | (isUser ? 4u : 0u));

					if (prepare_only) return true;
					else goto initpage_retry;
				}
				//PrintPageInfo("INI",lin_addr,writing,prepare_only);

				const uint8_t result = page_access_bits(dir_entry, table_entry);

				// If a page access right exception occurs we shouldn't change a or d
				// I'd prefer running into the prepared exception handler but we'd need
				// an additional handler that sets the 'a' bit - idea - foiler read?
				const uint8_t ft_index = result | (writing ? 8u : 0u) | (isUser ? 4u : 0u) | (paging.wp ? 16u : 0u);

				if (GCC_UNLIKELY(fault_table[ft_index])) {
					// exception error code format: 
					// bit0 - protection violation, bit1 - writing, bit2 - user mode
					PAGING_NewPageFault(lin_addr, tableEntryAddr, prepare_only,
						1u | (writing ? 2u : 0u) | (isUser ? 4u : 0u));

					if (prepare_only) return true;
					else goto initpage_retry; // unlikely to happen?
				}

				// save load to see if it changed later
				const uint32_t table_load = table_entry.load;

				// if we are writing we can set it right here to save some CPU
				if (writing) table_entry.block.d = 1;

				// set page accessed
				table_entry.block.a = 1;

				// update if needed
				if (table_load != table_entry.load)
					phys_writed(tableEntryAddr, table_entry.load);

				// if the page isn't dirty and we are reading we need to map the foiler
				// (dirty = false)
				const bool dirty = table_entry.block.d? true:false;
				/*
				   LOG_MSG("INIT  %s LIN% 8x PHYS% 5x wr%x ch%x wp%x d%x c%x m%x a%x [%x/%x/%x]",
				   mtr[result], lin_addr, table_entry.block.base,
				   writing, prepare_only, paging.wp,
				   dirty, cpu.cpl, cpu.mpl,
				   ((dir_entry.load<<1)&0xc) | ((table_entry.load>>1)&0x3),
				   dirEntryAddr, tableEntryAddr, table_entry.load);
				   */
				// finally install the new page
				PAGING_LinkPageNew(lin_page, table_entry.block.base, result, dirty);
			}

		} else { // paging off
			if (lin_page < LINK_START) phys_page = paging.firstmb[lin_page];
			else phys_page = lin_page;
			PAGING_LinkPage(lin_page,phys_page);
		}
		return false;
	}
	void InitPageForced(LinearPt lin_addr) {
		const PageNum lin_page = PageNum(lin_addr >> 12);
		PageNum phys_page;

		if (paging.enabled) {
			X86PageEntry table, entry;

			InitPageCheckPresence((PhysPt)lin_addr,false,table,entry);

			if (!table.block.a) {
				table.block.a=1;		//Set access
				phys_writed((PhysPt)((paging.base.page<<12)+(lin_page >> 10)*4),table.load);
			}

			if (do_pse && table.dirblock.ps) { // 4MB PSE page
				phys_page = table.dirblock4mb.getBase(lin_page);
			}
			else {
				if (!entry.block.a) {
					entry.block.a=1;					//Set access
					phys_writed((table.block.base<<12)+(lin_page & 0x3ff)*4,entry.load);
				}
				phys_page = entry.block.base;
			}
			// maybe use read-only page here if possible
		} else {
			if (lin_page < LINK_START) phys_page = paging.firstmb[lin_page];
			else phys_page = lin_page;
		}
		PAGING_LinkPage(lin_page,phys_page);
	}
};

bool PAGING_MakePhysPage(PageNum &page) {
	// page is the linear address on entry
	if (paging.enabled) {
		// check the page directory entry for this address
		X86PageEntry dir_entry;
		dir_entry.load = phys_readd(GetPageDirectoryEntryAddr((PhysPt)(page<<12)));
		if (!dir_entry.dirblock.p) return false;

		if (do_pse && dir_entry.dirblock.ps) {
			// return it
			page = dir_entry.dirblock4mb.getBase(page);
		}
		else {
			// check the page table entry
			X86PageEntry tbl_entry;
			tbl_entry.load = phys_readd(GetPageTableEntryAddr((PhysPt)(page<<12), dir_entry));
			if (!tbl_entry.block.p) return false;

			// return it
			page = tbl_entry.block.base;
		}
	} else {
		if (page < LINK_START) page = paging.firstmb[page];
		//Else keep it the same
	}
	return true;
}

static NewInitPageHandler init_page_handler;
static ExceptionPageHandler exception_handler;
static PageFoilHandler foiling_handler;

Bitu PAGING_GetDirBase(void) {
	return paging.cr3;
}

void PAGING_InitTLB(void) {
	for (Bitu i=0;i<TLB_SIZE;i++) {
		paging.tlb.read[i]=nullptr;
		paging.tlb.write[i]=nullptr;
		paging.tlb.readhandler[i]=&init_page_handler;
		paging.tlb.writehandler[i]=&init_page_handler;
	}
	paging.ur_links.used=0;
	paging.krw_links.used=0;
	paging.kr_links.used=0;
	paging.links.used=0;
}

void PAGING_ClearTLB(void) {
//	LOG_MSG("CLEAR                          m% 4u, kr% 4u, krw% 4u, ur% 4u",
//		paging.links.used, paging.kro_links.used, paging.krw_links.used, paging.ure_links.used);

	uint32_t * entries=&paging.links.entries[0];
	for (;paging.links.used>0;paging.links.used--) {
		Bitu page=*entries++;
		paging.tlb.read[page]=nullptr;
		paging.tlb.write[page]=nullptr;
		paging.tlb.readhandler[page]=&init_page_handler;
		paging.tlb.writehandler[page]=&init_page_handler;
	}
	paging.ur_links.used=0;
	paging.krw_links.used=0;
	paging.kr_links.used=0;
	paging.links.used=0;
}

void PAGING_UnlinkPages(PageNum lin_page,PageNum pages) {
	for (;pages>0;pages--) {
		paging.tlb.read[lin_page]=nullptr;
		paging.tlb.write[lin_page]=nullptr;
		paging.tlb.readhandler[lin_page]=&init_page_handler;
		paging.tlb.writehandler[lin_page]=&init_page_handler;
		lin_page++;
	}
}

void PAGING_MapPage(PageNum lin_page,PageNum phys_page) {
	if (lin_page<LINK_START) {
		paging.firstmb[lin_page]=(uint32_t)phys_page;
		paging.tlb.read[lin_page]=nullptr;
		paging.tlb.write[lin_page]=nullptr;
		paging.tlb.readhandler[lin_page]=&init_page_handler;
		paging.tlb.writehandler[lin_page]=&init_page_handler;
	} else {
		PAGING_LinkPage(lin_page,phys_page);
	}
}

static void PAGING_LinkPageNew(PageNum lin_page, PageNum phys_page, const uint8_t linkmode, bool dirty) {
	const uint8_t xlat_index = linkmode | (paging.wp? 8:0) | ((cpu.cpl==3)? 4:0);
	const uint8_t outcome = xlat_mapping[xlat_index];

	// get the physpage handler we are going to map 
	PageHandler * const handler = MEM_GetPageHandler(phys_page);
	const LinearPt lin_base = LinearPt(lin_page << 12);

	//NTS: phys_page is not used to index anything, however it must not use bits 31-30 used for other info. Stay within PHYSPAGE_ADDR.
	phys_page &= PHYSPAGE_ADDR;

//	LOG_MSG("MAPPG %s",lnm[outcome]);
	
	if (GCC_UNLIKELY(lin_page>=TLB_SIZE))
		E_Exit("Illegal page");
	if (GCC_UNLIKELY(paging.links.used>=PAGING_LINKS)) {
		LOG(LOG_PAGING,LOG_NORMAL)("Not enough paging links, resetting cache");
		PAGING_ClearTLB();
	}
	// re-use some of the unused bits in the phys_page variable
	// needed in the exception handler and foiler so they can replace themselves appropriately
	// bit31-30 ACMAP_
	// bit29	dirty
	// these bits are shifted off at the places paging.tlb.phys_page is read
	paging.tlb.phys_page[lin_page]= (uint32_t)(phys_page | (linkmode << 30u) | (dirty ? PHYSPAGE_DIRTY : 0));
	switch(outcome) {
	case ACMAP_RW:
		// read
		if (handler->getFlags() & PFLAG_READABLE) paging.tlb.read[lin_page] = 
			handler->GetHostReadPt(phys_page)-lin_base;
		else
			paging.tlb.read[lin_page]=nullptr;
		paging.tlb.readhandler[lin_page]=handler;

		// write
		if (dirty) { // in case it is already dirty we don't need to check
			if (handler->getFlags() & PFLAG_WRITEABLE) paging.tlb.write[lin_page] = 
				handler->GetHostWritePt(phys_page)-lin_base;
			else
				paging.tlb.write[lin_page]=nullptr;
			paging.tlb.writehandler[lin_page]=handler;
		} else {
			paging.tlb.writehandler[lin_page]= &foiling_handler;
			paging.tlb.write[lin_page]=nullptr;
		}
		break;
	case ACMAP_RE:
		// read
		if (handler->getFlags() & PFLAG_READABLE) paging.tlb.read[lin_page] = 
			handler->GetHostReadPt(phys_page)-lin_base;
		else
			paging.tlb.read[lin_page]=nullptr;
		paging.tlb.readhandler[lin_page]=handler;
		// exception
		paging.tlb.writehandler[lin_page]= &exception_handler;
		paging.tlb.write[lin_page]=nullptr;
		break;
	case ACMAP_EE:
		paging.tlb.readhandler[lin_page]= &exception_handler;
		paging.tlb.writehandler[lin_page]= &exception_handler;
		paging.tlb.read[lin_page]=nullptr;
		paging.tlb.write[lin_page]=nullptr;
		break;
	}

	switch(linkmode) {
		case ACCESS_KR:
			paging.kr_links.entries[paging.kr_links.used++]=(uint32_t)lin_page;
			break;
		case ACCESS_KRW:
			paging.krw_links.entries[paging.krw_links.used++]= (uint32_t)lin_page;
			break;
		case ACCESS_UR:
			paging.ur_links.entries[paging.ur_links.used++]= (uint32_t)lin_page;
			break;
		case ACCESS_URW:	// with this access right everything is possible
			// thus no need to modify it on a us <-> sv switch
			break;
	}
	paging.links.entries[paging.links.used++]= (uint32_t)lin_page; // "master table"
}

void PAGING_LinkPage(PageNum lin_page,PageNum phys_page) {
	PageHandler * const handler = MEM_GetPageHandler(phys_page);
	const LinearPt lin_base = LinearPt(lin_page << 12);

	//NTS: phys_page is not used to index anything, however it must not use bits 31-30 used for other info. Stay within PHYSPAGE_ADDR.
	phys_page &= PHYSPAGE_ADDR;

	if (lin_page>=TLB_SIZE)
		return E_Exit("Illegal page");

	if (paging.links.used>=PAGING_LINKS) {
		LOG(LOG_PAGING,LOG_NORMAL)("Not enough paging links, resetting cache");
		PAGING_ClearTLB();
	}

	paging.tlb.phys_page[lin_page]= (uint32_t)phys_page;
	if (handler->getFlags() & PFLAG_READABLE) paging.tlb.read[lin_page]=handler->GetHostReadPt(phys_page)-lin_base;
	else paging.tlb.read[lin_page]=nullptr;
	if (handler->getFlags() & PFLAG_WRITEABLE) paging.tlb.write[lin_page]=handler->GetHostWritePt(phys_page)-lin_base;
	else paging.tlb.write[lin_page]=nullptr;

	paging.links.entries[paging.links.used++]= (uint32_t)lin_page;
	paging.tlb.readhandler[lin_page]=handler;
	paging.tlb.writehandler[lin_page]=handler;
}

// parameter is the new cpl mode
void PAGING_SwitchCPL(bool isUser) {
//	LOG_MSG("SWCPL u%d kr%d, krw%d, ur%d",
//		isUser, paging.kro_links.used, paging.krw_links.used, paging.ure_links.used);
	
	// this function is worth optimizing
	// some of this cold be pre-stored?

	// krw - same for WP1 and WP0
	if (isUser) {
		// sv -> us: rw -> ee 
		for(Bitu i = 0; i < paging.krw_links.used; i++) {
			const tlbentry_t tlb_index = paging.krw_links.entries[i];
			paging.tlb.readhandler[tlb_index] = &exception_handler;
			paging.tlb.writehandler[tlb_index] = &exception_handler;
			paging.tlb.read[tlb_index] = nullptr;
			paging.tlb.write[tlb_index] = nullptr;
		}
	} else {
		// us -> sv: ee -> rw
		for(Bitu i = 0; i < paging.krw_links.used; i++) {
			const tlbentry_t tlb_index = paging.krw_links.entries[i];
			const PageNum phys_page = paging.tlb.phys_page[tlb_index] & PHYSPAGE_ADDR;
			const LinearPt lin_base = LinearPt(tlb_index << 12u);
			const bool dirty = (phys_page & PHYSPAGE_DIRTY) ? true : false;
			PageHandler* const handler = MEM_GetPageHandler(phys_page);
			
			// map read handler
			paging.tlb.readhandler[tlb_index] = handler;
			if (handler->getFlags()&PFLAG_READABLE)
				paging.tlb.read[tlb_index] = handler->GetHostReadPt(phys_page)-lin_base;
			else
				paging.tlb.read[tlb_index] = nullptr;
			
			// map write handler
			if (dirty) {
				paging.tlb.writehandler[tlb_index] = handler;
				if (handler->getFlags()&PFLAG_WRITEABLE)
					paging.tlb.write[tlb_index] = handler->GetHostWritePt(phys_page)-lin_base;
				else
					paging.tlb.write[tlb_index] = nullptr;
			} else {
				paging.tlb.writehandler[tlb_index] = &foiling_handler;
				paging.tlb.write[tlb_index] = nullptr;
			}
		}
	}
	
	if (GCC_UNLIKELY(paging.wp)) {
		// ur: no change with WP=1
		// kr
		if (isUser) {
			// sv -> us: re -> ee 
			for(Bitu i = 0; i < paging.kr_links.used; i++) {
				const tlbentry_t tlb_index = paging.kr_links.entries[i];
				paging.tlb.readhandler[tlb_index] = &exception_handler;
				paging.tlb.read[tlb_index] = nullptr;
			}
		} else {
			// us -> sv: ee -> re
			for(Bitu i = 0; i < paging.kr_links.used; i++) {
				const tlbentry_t tlb_index = paging.kr_links.entries[i];
				const LinearPt lin_base = LinearPt(tlb_index << 12);
				const PageNum phys_page = paging.tlb.phys_page[tlb_index] & PHYSPAGE_ADDR;
				PageHandler* const handler = MEM_GetPageHandler(phys_page);

				paging.tlb.readhandler[tlb_index] = handler;
				if (handler->getFlags()&PFLAG_READABLE)
					paging.tlb.read[tlb_index] = handler->GetHostReadPt(phys_page)-lin_base;
				else
					paging.tlb.read[tlb_index] = nullptr;
			}
		}
	} else { // WP=0
		// ur
		if (isUser) {
			// sv -> us: rw -> re 
			for(Bitu i = 0; i < paging.ur_links.used; i++) {
				const tlbentry_t tlb_index = paging.ur_links.entries[i];
				paging.tlb.writehandler[tlb_index] = &exception_handler;
				paging.tlb.write[tlb_index] = nullptr;
			}
		} else {
			// us -> sv: re -> rw
			for(Bitu i = 0; i < paging.ur_links.used; i++) {
				const tlbentry_t tlb_index = paging.ur_links.entries[i];
				const PageNum phys_page = paging.tlb.phys_page[tlb_index] & PHYSPAGE_ADDR;
				const bool dirty = (phys_page & PHYSPAGE_DIRTY) ? true : false;
				PageHandler* const handler = MEM_GetPageHandler(phys_page);

				if (dirty) {
					const LinearPt lin_base = LinearPt(tlb_index << 12);
					paging.tlb.writehandler[tlb_index] = handler;
					if (handler->getFlags()&PFLAG_WRITEABLE)
						paging.tlb.write[tlb_index] = handler->GetHostWritePt(phys_page)-lin_base;
					else
						paging.tlb.write[tlb_index] = nullptr;
				} else {
					paging.tlb.writehandler[tlb_index] = &foiling_handler;
					paging.tlb.write[tlb_index] = nullptr;
				}
			}
		}
	}
}

#define USERWRITE_PROHIBITED			((cpu.cpl&cpu.mpl)==3)
class InitPageUserROHandler : public PageHandler {
public:
	InitPageUserROHandler() {
		flags=PFLAG_INIT|PFLAG_NOCODE;
	}
	void writeb(PhysPt addr,uint8_t val) override {
		InitPage(addr,true,false);
		host_writeb(get_tlb_read(addr)+addr,(uint8_t)(val&0xff));
	}
	void writew(PhysPt addr,uint16_t val) override {
		InitPage(addr,true,false);
		host_writew(get_tlb_read(addr)+addr,(uint16_t)(val&0xffff));
	}
	void writed(PhysPt addr,uint32_t val) override {
		InitPage(addr,true,false);
		host_writed(get_tlb_read(addr)+addr,(uint32_t)val);
	}
	bool writeb_checked(PhysPt addr,uint8_t val) override {
		const uint8_t writecode = InitPageCheckOnly(addr);
		if (writecode) {
			HostPt tlb_addr;
			if (writecode>1) tlb_addr=get_tlb_read(addr);
			else tlb_addr=get_tlb_write(addr);
			host_writeb(tlb_addr+addr,(uint8_t)(val&0xff));
			return false;
		}
		return true;
	}
	bool writew_checked(PhysPt addr,uint16_t val) override {
		const uint8_t writecode = InitPageCheckOnly(addr);
		if (writecode) {
			HostPt tlb_addr;
			if (writecode>1) tlb_addr=get_tlb_read(addr);
			else tlb_addr=get_tlb_write(addr);
			host_writew(tlb_addr+addr,(uint16_t)(val&0xffff));
			return false;
		}
		return true;
	}
	bool writed_checked(PhysPt addr,uint32_t val) override {
		const uint8_t writecode = InitPageCheckOnly(addr);
		if (writecode) {
			HostPt tlb_addr;
			if (writecode>1) tlb_addr=get_tlb_read(addr);
			else tlb_addr=get_tlb_write(addr);
			host_writed(tlb_addr+addr,(uint32_t)val);
			return false;
		}
		return true;
	}
	bool InitPage(PhysPt lin_addr, bool writing, bool prepare_only) {
		(void)prepare_only;
		(void)writing;
		const PageNum lin_page = PageNum(lin_addr >> 12);
		PageNum phys_page;
		if (paging.enabled) {
			if (!USERWRITE_PROHIBITED) return true;

			X86PageEntry table;
			X86PageEntry entry;
			InitPageCheckPresence(lin_addr,true,table,entry);

			LOG(LOG_PAGING,LOG_NORMAL)("Page access denied: cpl=%i, %x:%x:%x:%x",
				(int)cpu.cpl,entry.block.us,table.block.us,entry.block.wr,table.block.wr);
			PAGING_PageFault(lin_addr,(table.block.base<<12)+(lin_page & 0x3ff)*4,0x07);

			if (!table.block.a) {
				table.block.a=1;		//Set access
				phys_writed((PhysPt)((paging.base.page<<12)+(lin_page >> 10)*4),table.load);
			}
			if (do_pse && table.dirblock.ps) { // 4MB PSE page
				phys_page = table.dirblock4mb.getBase(lin_page);
			}
			else {
				if ((!entry.block.a) || (!entry.block.d)) {
					entry.block.a=1;	//Set access
					entry.block.d=1;	//Set dirty
					phys_writed((table.block.base<<12)+(lin_page & 0x3ff)*4,entry.load);
				}
				phys_page = entry.block.base;
			}
			PAGING_LinkPage(lin_page,phys_page);
		} else {
			if (lin_page<LINK_START) phys_page = paging.firstmb[lin_page];
			else phys_page = lin_page;
			PAGING_LinkPage(lin_page,phys_page);
		}
		return false;
	}
	uint8_t InitPageCheckOnly(const PhysPt lin_addr) {
		const PageNum lin_page = PageNum(lin_addr >> 12);
		if (paging.enabled) {
			if (!USERWRITE_PROHIBITED) return 2;

			X86PageEntry table, entry;
			if (!InitPageCheckPresence_CheckOnly(lin_addr,true,table,entry)) return 0;

			if (InitPage_CheckUseraccess(entry.block.us,table.block.us) || (((entry.block.wr==0) || (table.block.wr==0)))) {
				LOG(LOG_PAGING,LOG_NORMAL)("Page access denied: cpl=%i, %x:%x:%x:%x",
					(int)cpu.cpl,entry.block.us,table.block.us,entry.block.wr,table.block.wr);
				paging.cr2=lin_addr;
				cpu.exception.which=EXCEPTION_PF;
				cpu.exception.error=0x07;
				return 0;
			}
			PAGING_LinkPage(lin_page,entry.block.base);
		} else {
			PageNum phys_page;
			if (lin_page<LINK_START) phys_page = paging.firstmb[lin_page];
			else phys_page = lin_page;
			PAGING_LinkPage(lin_page,phys_page);
		}
		return 1;
	}
	void InitPageForced(LinearPt lin_addr) {
		const PageNum lin_page = PageNum(lin_addr >> 12);
		PageNum phys_page;

		if (paging.enabled) {
			X86PageEntry table, entry;

			InitPageCheckPresence((PhysPt)lin_addr,true,table,entry);

			if (!table.block.a) {
				table.block.a=1;		//Set access
				phys_writed((PhysPt)((paging.base.page<<12)+(lin_page >> 10)*4),table.load);
			}

			if (do_pse && table.dirblock.ps) { // 4MB PSE page
				phys_page = table.dirblock4mb.getBase(lin_page);
			}
			else {
				if (!entry.block.a) {
					entry.block.a=1;	//Set access
					phys_writed((table.block.base<<12)+(lin_page & 0x3ff)*4,entry.load);
				}
				phys_page = entry.block.base;
			}
		} else {
			if (lin_page < LINK_START) phys_page = paging.firstmb[lin_page];
			else phys_page = lin_page;
		}
		PAGING_LinkPage(lin_page,phys_page);
	}
};

static InitPageUserROHandler init_page_handler_userro;

bool PAGING_ForcePageInit(LinearPt lin_addr) {
	PageHandler * const handler = get_tlb_readhandler((PhysPt)lin_addr);
	if (handler==&init_page_handler) {
		init_page_handler.InitPageForced(lin_addr);
		return true;
	} else if (handler==&init_page_handler_userro) {
		PAGING_UnlinkPages(lin_addr>>12,1);
		init_page_handler_userro.InitPageForced(lin_addr);
		return true;
	}
	return false;
}

void PAGING_SetDirBase(Bitu cr3) {
	paging.cr3=cr3;
	
	paging.base.page=cr3 >> 12U;
	paging.base.addr=cr3 & ~0xFFFU;
//	LOG(LOG_PAGING,LOG_NORMAL)("CR3:%X Base %X",cr3,paging.base.page);
	if (paging.enabled) {
		PAGING_ClearTLB();
	}
}

void PAGING_SetWP(bool wp) {
	paging.wp = wp;
	if (paging.enabled)
		PAGING_ClearTLB();
}

int CPU_IsDynamicCore(void);

void PAGING_Enable(bool enabled) {
	/* If paging is disabled, we work from a default paging table */
	if (paging.enabled==enabled) return;
	paging.enabled=enabled;
	if (auto_determine_dynamic_core_paging) {
		int coretype=CPU_IsDynamicCore();
		if (coretype) use_dynamic_core_with_paging = coretype==1?enabled&&dos_kernel_disabled:enabled&&!dos_kernel_disabled;
	}
	if (enabled) {
//		LOG(LOG_PAGING,LOG_NORMAL)("Enabled");
		PAGING_SetDirBase(paging.cr3);
	}
	PAGING_ClearTLB();
}

bool PAGING_Enabled(void) {
	return paging.enabled;
}

void PAGING_Init() {
	uint16_t i;

	// log
	LOG(LOG_MISC,LOG_DEBUG)("Initializing paging system (CPU linear -> physical mapping system)");

	/* Setup default Page Directory, force it to update */
	paging.enabled=false;
	paging.wp=false;
	PAGING_InitTLB();
	for (i=0;i<LINK_START;i++) paging.firstmb[i]=i;
	pf_queue.used=0;
}

// save state support
void POD_Save_CPU_Paging( std::ostream& stream )
{
	WRITE_POD( &paging.cr3, paging.cr3 );
	WRITE_POD( &paging.cr2, paging.cr2 );
//	WRITE_POD( &paging.wp, paging.wp );
	WRITE_POD( &paging.base, paging.base );

	WRITE_POD( &paging.tlb.read, paging.tlb.read );
	WRITE_POD( &paging.tlb.write, paging.tlb.write );
	WRITE_POD( &paging.tlb.phys_page, paging.tlb.phys_page );

	WRITE_POD( &paging.links, paging.links );
//	WRITE_POD( &paging.ur_links, paging.ur_links );
//	WRITE_POD( &paging.krw_links, paging.krw_links );
//	WRITE_POD( &paging.kr_links, paging.kr_links );

	WRITE_POD( &paging.firstmb, paging.firstmb );
	WRITE_POD( &paging.enabled, paging.enabled );

	WRITE_POD( &pf_queue, pf_queue );
}

void POD_Load_CPU_Paging( std::istream& stream )
{
	READ_POD( &paging.cr3, paging.cr3 );
	READ_POD( &paging.cr2, paging.cr2 );
//	READ_POD( &paging.wp, paging.wp );
	READ_POD( &paging.base, paging.base );

	READ_POD( &paging.tlb.read, paging.tlb.read );
	READ_POD( &paging.tlb.write, paging.tlb.write );
	READ_POD( &paging.tlb.phys_page, paging.tlb.phys_page );

	READ_POD( &paging.links, paging.links );
//	READ_POD( &paging.ur_links, paging.ur_links );
//	READ_POD( &paging.krw_links, paging.krw_links );
//	READ_POD( &paging.kr_links, paging.kr_links );

	READ_POD( &paging.firstmb, paging.firstmb );
	READ_POD( &paging.enabled, paging.enabled );

	READ_POD( &pf_queue, pf_queue );

	// reset all information
	paging.links.used = PAGING_LINKS;
	PAGING_ClearTLB();

	for( int lcv=0; lcv<TLB_SIZE; lcv++ ) {
		paging.tlb.read[lcv] = nullptr;
		paging.tlb.write[lcv] = nullptr;
		paging.tlb.readhandler[lcv] = &init_page_handler;
		paging.tlb.writehandler[lcv] = &init_page_handler;
	}
}

uint8_t PageHandler_HostPtReadB(PageHandler *p,PhysPt addr) {
	return p->GetHostReadPt(PAGING_GetPhysicalPageNumber(addr))[addr&0xFFF];
}

uint16_t PageHandler_HostPtReadW(PageHandler *p,PhysPt addr) {
	if ((addr&0xFFF) < 0xFFF) {
		return host_readw(&(p->GetHostReadPt(PAGING_GetPhysicalPageNumber(addr))[addr&0xFFF]));
	}
	else {
		return 	 PageHandler_HostPtReadB(p,addr) +
			(PageHandler_HostPtReadB(p,addr+PhysPt(1u)) << 8u);
	}
}

uint32_t PageHandler_HostPtReadD(PageHandler *p,PhysPt addr) {
	if ((addr&0xFFF) < 0xFFD) {
		return host_readd(&(p->GetHostReadPt(PAGING_GetPhysicalPageNumber(addr))[addr&0xFFF]));
	}
	else {
		return 	 PageHandler_HostPtReadB(p,addr) +
			(PageHandler_HostPtReadB(p,addr+PhysPt(1u)) << 8u) +
			(PageHandler_HostPtReadB(p,addr+PhysPt(2u)) << 16u) +
			(PageHandler_HostPtReadB(p,addr+PhysPt(3u)) << 24u);
	}
}

void PageHandler_HostPtWriteB(PageHandler *p,PhysPt addr,uint8_t val) {
	p->GetHostWritePt(PAGING_GetPhysicalPageNumber(addr))[addr&0xFFF] = val;
}

void PageHandler_HostPtWriteW(PageHandler *p,PhysPt addr,uint16_t val) {
	if ((addr&0xFFF) < 0xFFF) {
		host_writew(&(p->GetHostWritePt(PAGING_GetPhysicalPageNumber(addr))[addr&0xFFF]),val);
	}
	else {
		PageHandler_HostPtWriteB(p,addr,            val     &0xFFu);
		PageHandler_HostPtWriteB(p,addr+PhysPt(1u),(val>>8u)&0xFFu);
	}
}

void PageHandler_HostPtWriteD(PageHandler *p,PhysPt addr,uint32_t val) {
	if ((addr&0xFFF) < 0xFFD) {
		host_writed(&(p->GetHostWritePt(PAGING_GetPhysicalPageNumber(addr))[addr&0xFFF]),val);
	}
	else {
		PageHandler_HostPtWriteB(p,addr,            val      &0xFFu);
		PageHandler_HostPtWriteB(p,addr+PhysPt(1u),(val>> 8u)&0xFFu);
		PageHandler_HostPtWriteB(p,addr+PhysPt(2u),(val>>16u)&0xFFu);
		PageHandler_HostPtWriteB(p,addr+PhysPt(3u),(val>>24u)&0xFFu);
	}
}

