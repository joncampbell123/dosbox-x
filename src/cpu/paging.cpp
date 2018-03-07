/*
 *  Copyright (C) 2002-2015  The DOSBox Team
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


#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "dosbox.h"
#include "mem.h"
#include "paging.h"
#include "regs.h"
#include "lazyflags.h"
#include "cpu.h"
#include "debug.h"
#include "setup.h"

PagingBlock paging;

// Pagehandler implementation
Bitu PageHandler::readb(PhysPt addr) {
	E_Exit("No byte handler for read from %x",addr);	
	return 0;
}
Bitu PageHandler::readw(PhysPt addr) {
	Bitu ret = (readb(addr+0) << 0);
	ret     |= (readb(addr+1) << 8);
	return ret;
}
Bitu PageHandler::readd(PhysPt addr) {
	Bitu ret = (readb(addr+0) << 0);
	ret     |= (readb(addr+1) << 8);
	ret     |= (readb(addr+2) << 16);
	ret     |= (readb(addr+3) << 24);
	return ret;
}

void PageHandler::writeb(PhysPt addr,Bitu /*val*/) {
	E_Exit("No byte handler for write to %x",addr);	
}

void PageHandler::writew(PhysPt addr,Bitu val) {
	writeb(addr+0,(Bit8u) (val >> 0));
	writeb(addr+1,(Bit8u) (val >> 8));
}
void PageHandler::writed(PhysPt addr,Bitu val) {
	writeb(addr+0,(Bit8u) (val >> 0));
	writeb(addr+1,(Bit8u) (val >> 8));
	writeb(addr+2,(Bit8u) (val >> 16));
	writeb(addr+3,(Bit8u) (val >> 24));
}

HostPt PageHandler::GetHostReadPt(Bitu /*phys_page*/) {
	return 0;
}

HostPt PageHandler::GetHostWritePt(Bitu /*phys_page*/) {
	return 0;
}

bool PageHandler::readb_checked(PhysPt addr, Bit8u * val) {
	*val=(Bit8u)readb(addr);	return false;
}
bool PageHandler::readw_checked(PhysPt addr, Bit16u * val) {
	*val=(Bit16u)readw(addr);	return false;
}
bool PageHandler::readd_checked(PhysPt addr, Bit32u * val) {
	*val=(Bit32u)readd(addr);	return false;
}
bool PageHandler::writeb_checked(PhysPt addr,Bitu val) {
	writeb(addr,val);	return false;
}
bool PageHandler::writew_checked(PhysPt addr,Bitu val) {
	writew(addr,val);	return false;
}
bool PageHandler::writed_checked(PhysPt addr,Bitu val) {
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
//	Bits ret=CPU_Core_Full_Run();
	Bits ret=CPU_Core_Normal_Run();
	CPU_CycleLeft+=CPU_Cycles;
	if (ret<0) E_Exit("Got a dosbox close machine in pagefault core?");
	if (ret) 
		return ret;
	if (!pf_queue.used) E_Exit("PF Core without PF");
	PF_Entry * entry=&pf_queue.entries[pf_queue.used-1];
	X86PageEntry pentry;
	pentry.load=phys_readd(entry->page_addr);
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
const char* const mtr[] = {"KR ","KRW","UR ","URW","PFL"};

// bit0 entry write
// bit1 entry access
// bit2 table write
// bit3 table access
// These arrays define how the access bits in the page table and entry
// result in access rights.
// The used array is switched depending on the CPU under emulation.

// Intel says the lowest numeric value wins for both 386 and 486+
// There's something strange about KR with WP=1 though
static const Bit8u translate_array[] = {
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

// This array defines how a page is mapped depending on 
// page access right, cpl==3, and WP.
// R = map handler as read, W = map handler as write, E = map exception handler
#define ACMAP_RW 0
#define ACMAP_RE 1
#define ACMAP_EE 2

static const char* const lnm[] = {"RW ","RE ","EE "}; // debug stuff

// bit0-1 ACCESS_ type
// bit2   1=user mode
// bit3   WP on

static const Bit8u xlat_mapping[] = {
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

static const Bit8u fault_table[] = {
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

#define PHYSPAGE_DITRY 0x10000000
#define PHYSPAGE_ADDR  0x000FFFFF

// helper functions for calculating table entry addresses
static inline PhysPt GetPageDirectoryEntryAddr(PhysPt lin_addr) {
	return paging.base.addr | ((lin_addr >> 22) << 2);
}
static inline PhysPt GetPageTableEntryAddr(PhysPt lin_addr, X86PageEntry& dir_entry) {
	return (dir_entry.block.base<<12) | ((lin_addr >> 10) & 0xffc);
}
/*
void PrintPageInfo(const char* string, PhysPt lin_addr, bool writing, bool prepare_only) {

	Bitu lin_page=lin_addr >> 12;

	X86PageEntry dir_entry, table_entry;
	bool isUser = (((cpu.cpl & cpu.mpl)==3)? true:false);

	PhysPt dirEntryAddr = GetPageDirectoryEntryAddr(lin_addr);
	PhysPt tableEntryAddr = 0;
	dir_entry.load=phys_readd(dirEntryAddr);
	Bitu result = 4;
	bool dirty = false;
	Bitu ft_index = 0;

	if (dir_entry.block.p) {
		tableEntryAddr = GetPageTableEntryAddr(lin_addr, dir_entry);
		table_entry.load=phys_readd(tableEntryAddr);
		if (table_entry.block.p) {
			result =
				translate_array[((dir_entry.load<<1)&0xc) | ((table_entry.load>>1)&0x3)];

			ft_index = result | (writing? 8:0) | (isUser? 4:0) |
				(paging.wp? 16:0);

			dirty = table_entry.block.d? true:false;
		}
	}
	LOG_MSG("%s %s LIN% 8x PHYS% 5x wr%x ch%x wp%x d%x c%x m%x f%x a%x [%x/%x/%x]",
		string, mtr[result], lin_addr, table_entry.block.base,
		writing, prepare_only, paging.wp,
		dirty, cpu.cpl, cpu.mpl, fault_table[ft_index],
		((dir_entry.load<<1)&0xc) | ((table_entry.load>>1)&0x3),
		dirEntryAddr, tableEntryAddr, table_entry.load);
}
*/

bool dosbox_allow_nonrecursive_page_fault = false;	/* when set, do nonrecursive mode (when executing instruction) */

// PAGING_NewPageFault
// lin_addr, page_addr: the linear and page address the fault happened at
// prepare_only: true in case the calling core handles the fault, else the PageFaultCore does
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
		Bitu lin_page = addr >> 12;
		Bit32u phys_page = paging.tlb.phys_page[lin_page] & PHYSPAGE_ADDR;
			
		// set the page dirty in the tlb
		paging.tlb.phys_page[lin_page] |= PHYSPAGE_DITRY;

		// mark the page table entry dirty
		X86PageEntry dir_entry, table_entry;

		PhysPt dirEntryAddr = GetPageDirectoryEntryAddr(addr);
		dir_entry.load=phys_readd(dirEntryAddr);
		if (!dir_entry.block.p) E_Exit("Undesired situation 1 in page foiler.");

		PhysPt tableEntryAddr = GetPageTableEntryAddr(addr, dir_entry);
		table_entry.load=phys_readd(tableEntryAddr);
		if (!table_entry.block.p) E_Exit("Undesired situation 2 in page foiler.");

		// for debugging...
		if (table_entry.block.base != phys_page)
			E_Exit("Undesired situation 3 in page foiler.");

		// map the real write handler in our place
		PageHandler* handler = MEM_GetPageHandler(phys_page);

		// debug
//		LOG_MSG("FOIL            LIN% 8x PHYS% 8x [%x/%x/%x] WRP % 8x", addr, phys_page,
//			dirEntryAddr, tableEntryAddr, table_entry.load, wtest);

		// this can happen when the same page table is used at two different
		// page directory entries / linear locations (WfW311)
		// if (table_entry.block.d) E_Exit("% 8x Already dirty!!",table_entry.load);
		
		
		// set the dirty bit
		table_entry.block.d=1;
		phys_writed(tableEntryAddr,table_entry.load);
		
		// replace this handler with the real thing
		if (handler->getFlags() & PFLAG_WRITEABLE)
			paging.tlb.write[lin_page] = handler->GetHostWritePt(phys_page) - (lin_page << 12);
		else paging.tlb.write[lin_page]=0;
		paging.tlb.writehandler[lin_page]=handler;

		return;
	}

	void read() {
		E_Exit("The page foiler shouldn't be read.");
	}
public:
	PageFoilHandler() : PageHandler(PFLAG_INIT|PFLAG_NOCODE) {}
	Bitu readb(PhysPt addr) {read();return 0;}
	Bitu readw(PhysPt addr) {read();return 0;}
	Bitu readd(PhysPt addr) {read();return 0;}

	void writeb(PhysPt addr,Bitu val) {
		work(addr);
		// execute the write:
		// no need to care about mpl because we won't be entered
		// if write isn't allowed
		mem_writeb(addr,val);
	}
	void writew(PhysPt addr,Bitu val) {
		work(addr);
		mem_writew(addr,val);
	}
	void writed(PhysPt addr,Bitu val) {
		work(addr);
		mem_writed(addr,val);
	}

	bool readb_checked(PhysPt addr, Bit8u * val) {read();return true;}
	bool readw_checked(PhysPt addr, Bit16u * val) {read();return true;}
	bool readd_checked(PhysPt addr, Bit32u * val) {read();return true;}

	bool writeb_checked(PhysPt addr,Bitu val) {
		work(addr);
			mem_writeb(addr,val);
			return false;
	}
	bool writew_checked(PhysPt addr,Bitu val) {
		work(addr);
			mem_writew(addr,val);
			return false;
	}
	bool writed_checked(PhysPt addr,Bitu val) {
		work(addr);
			mem_writed(addr,val);
			return false;
	}
};

class ExceptionPageHandler : public PageHandler {
private:
	PageHandler* getHandler(PhysPt addr) {
		Bitu lin_page = addr >> 12;
		Bit32u phys_page = paging.tlb.phys_page[lin_page] & PHYSPAGE_ADDR;
		PageHandler* handler = MEM_GetPageHandler(phys_page);
		return handler;
					}

	bool hack_check(PhysPt addr) {
		// First Encounters
		// They change the page attributes without clearing the TLB.
		// On a real 486 they get away with it because its TLB has only 32 or so 
		// elements. The changed page attribs get overwritten and re-read before
		// the exception happens. Here we have gazillions of TLB entries so the
		// exception occurs if we don't check for it.

		Bitu old_attirbs = paging.tlb.phys_page[addr>>12] >> 30;
		X86PageEntry dir_entry, table_entry;
		
		dir_entry.load = phys_readd(GetPageDirectoryEntryAddr(addr));
		if (!dir_entry.block.p) return false;
		table_entry.load = phys_readd(GetPageTableEntryAddr(addr, dir_entry));
		if (!table_entry.block.p) return false;
		Bitu result =
		translate_array[((dir_entry.load<<1)&0xc) | ((table_entry.load>>1)&0x3)];
		if (result != old_attirbs) return true;
		return false;
				}

	void Exception(PhysPt addr, bool writing, bool checked) {
		//PrintPageInfo("XCEPT",addr,writing, checked);
		//LOG_MSG("XCEPT LIN% 8x wr%d, ch%d, cpl%d, mpl%d",addr, writing, checked, cpu.cpl, cpu.mpl);
		PhysPt tableaddr = 0;
		if (!checked) {
			X86PageEntry dir_entry;
			dir_entry.load = phys_readd(GetPageDirectoryEntryAddr(addr));
			if (!dir_entry.block.p) E_Exit("Undesired situation 1 in exception handler.");
			
			// page table entry
			tableaddr = GetPageTableEntryAddr(addr, dir_entry);
			//Bitu d_index=(addr >> 12) >> 10;
			//tableaddr=(paging.base.page<<12) | (d_index<<2);
		} 
		PAGING_NewPageFault(addr, tableaddr, checked,
			1 | (writing? 2:0) | (((cpu.cpl&cpu.mpl)==3)? 4:0));
		
		PAGING_ClearTLB(); // TODO got a better idea?
	}

	Bitu readb_through(PhysPt addr) {
		Bitu lin_page = addr >> 12;
		Bit32u phys_page = paging.tlb.phys_page[lin_page] & PHYSPAGE_ADDR;
		PageHandler* handler = MEM_GetPageHandler(phys_page);
		if (handler->getFlags() & PFLAG_READABLE) {
			return host_readb(handler->GetHostReadPt(phys_page) + (addr&0xfff));
			}
		else return handler->readb(addr);
					}
	Bitu readw_through(PhysPt addr) {
		Bitu lin_page = addr >> 12;
		Bit32u phys_page = paging.tlb.phys_page[lin_page] & PHYSPAGE_ADDR;
		PageHandler* handler = MEM_GetPageHandler(phys_page);
		if (handler->getFlags() & PFLAG_READABLE) {
			return host_readw(handler->GetHostReadPt(phys_page) + (addr&0xfff));
				}
		else return handler->readw(addr);
			}
	Bitu readd_through(PhysPt addr) {
		Bitu lin_page = addr >> 12;
		Bit32u phys_page = paging.tlb.phys_page[lin_page] & PHYSPAGE_ADDR;
		PageHandler* handler = MEM_GetPageHandler(phys_page);
		if (handler->getFlags() & PFLAG_READABLE) {
			return host_readd(handler->GetHostReadPt(phys_page) + (addr&0xfff));
		}
		else return handler->readd(addr);
			}

	void writeb_through(PhysPt addr, Bitu val) {
		Bitu lin_page = addr >> 12;
		Bit32u phys_page = paging.tlb.phys_page[lin_page] & PHYSPAGE_ADDR;
		PageHandler* handler = MEM_GetPageHandler(phys_page);
		if (handler->getFlags() & PFLAG_WRITEABLE) {
			return host_writeb(handler->GetHostWritePt(phys_page) + (addr&0xfff), (Bit8u)val);
		}
		else return handler->writeb(addr, val);
			}

	void writew_through(PhysPt addr, Bitu val) {
		Bitu lin_page = addr >> 12;
		Bit32u phys_page = paging.tlb.phys_page[lin_page] & PHYSPAGE_ADDR;
		PageHandler* handler = MEM_GetPageHandler(phys_page);
		if (handler->getFlags() & PFLAG_WRITEABLE) {
			return host_writew(handler->GetHostWritePt(phys_page) + (addr&0xfff), (Bit16u)val);
		}
		else return handler->writew(addr, val);
	}

	void writed_through(PhysPt addr, Bitu val) {
		Bitu lin_page = addr >> 12;
		Bit32u phys_page = paging.tlb.phys_page[lin_page] & PHYSPAGE_ADDR;
		PageHandler* handler = MEM_GetPageHandler(phys_page);
		if (handler->getFlags() & PFLAG_WRITEABLE) {
			return host_writed(handler->GetHostWritePt(phys_page) + (addr&0xfff), val);
		}
		else return handler->writed(addr, val);
			}

public:
	ExceptionPageHandler() : PageHandler(PFLAG_INIT|PFLAG_NOCODE) {}
	Bitu readb(PhysPt addr) {
		if (!cpu.mpl) return readb_through(addr);
			
		Exception(addr, false, false);
		return mem_readb(addr); // read the updated page (unlikely to happen?)
				}
	Bitu readw(PhysPt addr) {
		// access type is supervisor mode (temporary)
		// we are always allowed to read in superuser mode
		// so get the handler and address and read it
		if (!cpu.mpl) return readw_through(addr);

		Exception(addr, false, false);
		return mem_readw(addr);
			}
	Bitu readd(PhysPt addr) {
		if (!cpu.mpl) return readd_through(addr);

		Exception(addr, false, false);
		return mem_readd(addr);
		}
	void writeb(PhysPt addr,Bitu val) {
		if (!cpu.mpl) {
			writeb_through(addr, val);
			return;
	}
		Exception(addr, true, false);
		mem_writeb(addr, val);
			}
	void writew(PhysPt addr,Bitu val) {
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
	void writed(PhysPt addr,Bitu val) {
		if (!cpu.mpl) {
			writed_through(addr, val);
			return;
		}
		Exception(addr, true, false);
		mem_writed(addr, val);
	}
	// returning true means an exception was triggered for these _checked functions
	bool readb_checked(PhysPt addr, Bit8u * val) {
		Exception(addr, false, true);
		return true;
	}
	bool readw_checked(PhysPt addr, Bit16u * val) {
		Exception(addr, false, true);
		return true;
			}
	bool readd_checked(PhysPt addr, Bit32u * val) {
		Exception(addr, false, true);
		return true;
		}
	bool writeb_checked(PhysPt addr,Bitu val) {
		Exception(addr, true, true);
		return true;
	}
	bool writew_checked(PhysPt addr,Bitu val) {
		if (hack_check(addr)) {
			LOG_MSG("Page attributes modified without clear");
			PAGING_ClearTLB();
			mem_writew(addr,val); // TODO this makes us recursive again?
			return false;
		}
		Exception(addr, true, true);
		return true;
	}
	bool writed_checked(PhysPt addr,Bitu val) {
		Exception(addr, true, true);
		return true;
	}
};

static void PAGING_LinkPageNew(Bitu lin_page, Bitu phys_page, Bitu linkmode, bool dirty);

class NewInitPageHandler : public PageHandler {
public:
	NewInitPageHandler() : PageHandler(PFLAG_INIT|PFLAG_NOCODE) {}
	Bitu readb(PhysPt addr) {
		InitPage(addr, false, false);
		return mem_readb(addr);
	}
	Bitu readw(PhysPt addr) {
		InitPage(addr, false, false);
		return mem_readw(addr);
	}
	Bitu readd(PhysPt addr) {
		InitPage(addr, false, false);
		return mem_readd(addr);
	}
	void writeb(PhysPt addr,Bitu val) {
		InitPage(addr, true, false);
		mem_writeb(addr,val);
	}
	void writew(PhysPt addr,Bitu val) {
		InitPage(addr, true, false);
		mem_writew(addr,val);
	}
	void writed(PhysPt addr,Bitu val) {
		InitPage(addr, true, false);
		mem_writed(addr,val);
	}

	bool readb_checked(PhysPt addr, Bit8u * val) {
		if (InitPage(addr, false, true)) return true;
		*val=mem_readb(addr);
			return false;
		}
	bool readw_checked(PhysPt addr, Bit16u * val) {
		if (InitPage(addr, false, true)) return true;
		*val=mem_readw(addr);
		return false;
	}
	bool readd_checked(PhysPt addr, Bit32u * val) {
		if (InitPage(addr, false, true)) return true;
		*val=mem_readd(addr);
			return false;
		}
	bool writeb_checked(PhysPt addr,Bitu val) {
		if (InitPage(addr, true, true)) return true;
		mem_writeb(addr,val);
		return false;
	}
	bool writew_checked(PhysPt addr,Bitu val) {
		if (InitPage(addr, true, true)) return true;
		mem_writew(addr,val);
			return false;
		}
	bool writed_checked(PhysPt addr,Bitu val) {
		if (InitPage(addr, true, true)) return true;
		mem_writed(addr,val);
		return false;
	}
	bool InitPage(PhysPt lin_addr, bool writing, bool prepare_only) {
		Bitu lin_page=lin_addr >> 12;
		Bitu phys_page;
		if (paging.enabled) {
initpage_retry:
			X86PageEntry dir_entry, table_entry;
			bool isUser = (((cpu.cpl & cpu.mpl)==3)? true:false);

			// Read the paging stuff, throw not present exceptions if needed
			// and find out how the page should be mapped
			PhysPt dirEntryAddr = GetPageDirectoryEntryAddr(lin_addr);
			dir_entry.load=phys_readd(dirEntryAddr);

			if (!dir_entry.block.p) {
				// table pointer is not present, do a page fault
				PAGING_NewPageFault(lin_addr, dirEntryAddr, prepare_only,
					(writing? 2:0) | (isUser? 4:0));
				
				if (prepare_only) return true;
				else goto initpage_retry; // TODO maybe E_Exit after a few loops
			}
			PhysPt tableEntryAddr = GetPageTableEntryAddr(lin_addr, dir_entry);
			table_entry.load=phys_readd(tableEntryAddr);

			// set page table accessed (IA manual: A is set whenever the entry is 
			// used in a page translation)
			if (!dir_entry.block.a) {
				dir_entry.block.a = 1;		
				phys_writed(dirEntryAddr, dir_entry.load);
		}

			if (!table_entry.block.p) {
				// physpage pointer is not present, do a page fault
				PAGING_NewPageFault(lin_addr, tableEntryAddr, prepare_only,
					 (writing? 2:0) | (isUser? 4:0));
				
				if (prepare_only) return true;
				else goto initpage_retry;
	}
			//PrintPageInfo("INI",lin_addr,writing,prepare_only);

			Bitu result =
				translate_array[((dir_entry.load<<1)&0xc) | ((table_entry.load>>1)&0x3)];
			
			// If a page access right exception occurs we shouldn't change a or d
			// I'd prefer running into the prepared exception handler but we'd need
			// an additional handler that sets the 'a' bit - idea - foiler read?
			Bitu ft_index = result | (writing? 8:0) | (isUser? 4:0) |
				(paging.wp? 16:0);
			
			if (GCC_UNLIKELY(fault_table[ft_index])) {
				// exception error code format: 
				// bit0 - protection violation, bit1 - writing, bit2 - user mode
				PAGING_NewPageFault(lin_addr, tableEntryAddr, prepare_only,
					1 | (writing? 2:0) | (isUser? 4:0));
				
				if (prepare_only) return true;
				else goto initpage_retry; // unlikely to happen?
			}
			// save load to see if it changed later
			Bit32u table_load = table_entry.load;

			// if we are writing we can set it right here to save some CPU
			if (writing) table_entry.block.d = 1;

			// set page accessed
			table_entry.block.a = 1;
			
			// update if needed
			if (table_load != table_entry.load)
				phys_writed(tableEntryAddr, table_entry.load);

			// if the page isn't dirty and we are reading we need to map the foiler
			// (dirty = false)
			bool dirty = table_entry.block.d? true:false;
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

		} else { // paging off
			if (lin_page<LINK_START) phys_page=paging.firstmb[lin_page];
			else phys_page=lin_page;
			PAGING_LinkPage(lin_page,phys_page);
		}
		return false;
	}
};

bool PAGING_MakePhysPage(Bitu & page) {
	// page is the linear address on entry
	if (paging.enabled) {
		// check the page directory entry for this address
		X86PageEntry dir_entry;
		dir_entry.load = phys_readd(GetPageDirectoryEntryAddr(page<<12));
		if (!dir_entry.block.p) return false;
		
		// check the page table entry
		X86PageEntry tbl_entry;
		tbl_entry.load = phys_readd(GetPageTableEntryAddr(page<<12, dir_entry));
		if (!tbl_entry.block.p) return false;

		// return it
		page = tbl_entry.block.base;
	} else {
		if (page<LINK_START) page=paging.firstmb[page];
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

#if defined(USE_FULL_TLB)
void PAGING_InitTLB(void) {
	for (Bitu i=0;i<TLB_SIZE;i++) {
		paging.tlb.read[i]=0;
		paging.tlb.write[i]=0;
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

	Bit32u * entries=&paging.links.entries[0];
	for (;paging.links.used>0;paging.links.used--) {
		Bitu page=*entries++;
		paging.tlb.read[page]=0;
		paging.tlb.write[page]=0;
		paging.tlb.readhandler[page]=&init_page_handler;
		paging.tlb.writehandler[page]=&init_page_handler;
	}
	paging.ur_links.used=0;
	paging.krw_links.used=0;
	paging.kr_links.used=0;
	paging.links.used=0;
}

void PAGING_UnlinkPages(Bitu lin_page,Bitu pages) {
	for (;pages>0;pages--) {
		paging.tlb.read[lin_page]=0;
		paging.tlb.write[lin_page]=0;
		paging.tlb.readhandler[lin_page]=&init_page_handler;
		paging.tlb.writehandler[lin_page]=&init_page_handler;
		lin_page++;
	}
}

void PAGING_MapPage(Bitu lin_page,Bitu phys_page) {
	if (lin_page<LINK_START) {
		paging.firstmb[lin_page]=phys_page;
		paging.tlb.read[lin_page]=0;
		paging.tlb.write[lin_page]=0;
		paging.tlb.readhandler[lin_page]=&init_page_handler;
		paging.tlb.writehandler[lin_page]=&init_page_handler;
	} else {
		PAGING_LinkPage(lin_page,phys_page);
	}
}

static void PAGING_LinkPageNew(Bitu lin_page, Bitu phys_page, Bitu linkmode, bool dirty) {
	Bitu xlat_index = linkmode | (paging.wp? 8:0) | ((cpu.cpl==3)? 4:0);
	Bit8u outcome = xlat_mapping[xlat_index];

	// get the physpage handler we are going to map 
	PageHandler * handler=MEM_GetPageHandler(phys_page);
	Bitu lin_base=lin_page << 12;

//	LOG_MSG("MAPPG %s",lnm[outcome]);
	
	if (GCC_UNLIKELY(lin_page>=TLB_SIZE || phys_page>=TLB_SIZE)) 
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
	paging.tlb.phys_page[lin_page]= phys_page | (linkmode<< 30) | (dirty? PHYSPAGE_DITRY:0);
	switch(outcome) {
	case ACMAP_RW:
		// read
		if (handler->getFlags() & PFLAG_READABLE) paging.tlb.read[lin_page] = 
			handler->GetHostReadPt(phys_page)-lin_base;
	else paging.tlb.read[lin_page]=0;
	paging.tlb.readhandler[lin_page]=handler;
		
		// write
		if (dirty) { // in case it is already dirty we don't need to check
			if (handler->getFlags() & PFLAG_WRITEABLE) paging.tlb.write[lin_page] = 
				handler->GetHostWritePt(phys_page)-lin_base;
			else paging.tlb.write[lin_page]=0;
	paging.tlb.writehandler[lin_page]=handler;
		} else {
			paging.tlb.writehandler[lin_page]= &foiling_handler;
			paging.tlb.write[lin_page]=0;
		}
		break;
	case ACMAP_RE:
		// read
		if (handler->getFlags() & PFLAG_READABLE) paging.tlb.read[lin_page] = 
			handler->GetHostReadPt(phys_page)-lin_base;
		else paging.tlb.read[lin_page]=0;
		paging.tlb.readhandler[lin_page]=handler;
		// exception
		paging.tlb.writehandler[lin_page]= &exception_handler;
		paging.tlb.write[lin_page]=0;
		break;
	case ACMAP_EE:
		paging.tlb.readhandler[lin_page]= &exception_handler;
		paging.tlb.writehandler[lin_page]= &exception_handler;
		paging.tlb.read[lin_page]=0;
		paging.tlb.write[lin_page]=0;
		break;
}

	switch(linkmode) {
	case ACCESS_KR:
		paging.kr_links.entries[paging.kr_links.used++]=lin_page;
		break;
	case ACCESS_KRW:
		paging.krw_links.entries[paging.krw_links.used++]=lin_page;
		break;
	case ACCESS_UR:
		paging.ur_links.entries[paging.ur_links.used++]=lin_page;
		break;
	case ACCESS_URW:	// with this access right everything is possible
						// thus no need to modify it on a us <-> sv switch
		break;
	}
	paging.links.entries[paging.links.used++]=lin_page; // "master table"
}

void PAGING_LinkPage(Bitu lin_page,Bitu phys_page) {
	PageHandler * handler=MEM_GetPageHandler(phys_page);
	Bitu lin_base=lin_page << 12;
	if (lin_page>=TLB_SIZE || phys_page>=TLB_SIZE) 
		E_Exit("Illegal page");

	if (paging.links.used>=PAGING_LINKS) {
		LOG(LOG_PAGING,LOG_NORMAL)("Not enough paging links, resetting cache");
		PAGING_ClearTLB();
	}

	paging.tlb.phys_page[lin_page]=phys_page;
	if (handler->getFlags() & PFLAG_READABLE) paging.tlb.read[lin_page]=handler->GetHostReadPt(phys_page)-lin_base;
	else paging.tlb.read[lin_page]=0;
	if (handler->getFlags() & PFLAG_WRITEABLE) paging.tlb.write[lin_page]=handler->GetHostWritePt(phys_page)-lin_base;
	else paging.tlb.write[lin_page]=0;

	paging.links.entries[paging.links.used++]=lin_page;
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
			Bitu tlb_index = paging.krw_links.entries[i];
			paging.tlb.readhandler[tlb_index] = &exception_handler;
			paging.tlb.writehandler[tlb_index] = &exception_handler;
			paging.tlb.read[tlb_index] = 0;
			paging.tlb.write[tlb_index] = 0;
		}
	} else {
		// us -> sv: ee -> rw
		for(Bitu i = 0; i < paging.krw_links.used; i++) {
			Bitu tlb_index = paging.krw_links.entries[i];
			Bitu phys_page = paging.tlb.phys_page[tlb_index];
			Bitu lin_base = tlb_index << 12;
			bool dirty = (phys_page & PHYSPAGE_DITRY)? true:false;
			phys_page &= PHYSPAGE_ADDR;
			PageHandler* handler = MEM_GetPageHandler(phys_page);
			
			// map read handler
			paging.tlb.readhandler[tlb_index] = handler;
			if (handler->getFlags()&PFLAG_READABLE)
				paging.tlb.read[tlb_index] = handler->GetHostReadPt(phys_page)-lin_base;
			else paging.tlb.read[tlb_index] = 0;
			
			// map write handler
			if (dirty) {
				paging.tlb.writehandler[tlb_index] = handler;
				if (handler->getFlags()&PFLAG_WRITEABLE)
					paging.tlb.write[tlb_index] = handler->GetHostWritePt(phys_page)-lin_base;
				else paging.tlb.write[tlb_index] = 0;
			} else {
				paging.tlb.writehandler[tlb_index] = &foiling_handler;
				paging.tlb.write[tlb_index] = 0;
			}
		}
	}
	
	if (GCC_UNLIKELY(paging.wp)) {
		// ur: no change with WP=1
		// kr
		if (isUser) {
			// sv -> us: re -> ee 
			for(Bitu i = 0; i < paging.kr_links.used; i++) {
				Bitu tlb_index = paging.kr_links.entries[i];
				paging.tlb.readhandler[tlb_index] = &exception_handler;
				paging.tlb.read[tlb_index] = 0;
			}
		} else {
			// us -> sv: ee -> re
			for(Bitu i = 0; i < paging.kr_links.used; i++) {
				Bitu tlb_index = paging.kr_links.entries[i];
				Bitu lin_base = tlb_index << 12;
				Bitu phys_page = paging.tlb.phys_page[tlb_index] & PHYSPAGE_ADDR;
				PageHandler* handler = MEM_GetPageHandler(phys_page);

				paging.tlb.readhandler[tlb_index] = handler;
				if (handler->getFlags()&PFLAG_READABLE)
					paging.tlb.read[tlb_index] = handler->GetHostReadPt(phys_page)-lin_base;
				else paging.tlb.read[tlb_index] = 0;
			}
		}
	} else { // WP=0
		// ur
		if (isUser) {
			// sv -> us: rw -> re 
			for(Bitu i = 0; i < paging.ur_links.used; i++) {
				Bitu tlb_index = paging.ur_links.entries[i];
				paging.tlb.writehandler[tlb_index] = &exception_handler;
				paging.tlb.write[tlb_index] = 0;
			}
		} else {
			// us -> sv: re -> rw
			for(Bitu i = 0; i < paging.ur_links.used; i++) {
				Bitu tlb_index = paging.ur_links.entries[i];
				Bitu phys_page = paging.tlb.phys_page[tlb_index];
				bool dirty = (phys_page & PHYSPAGE_DITRY)? true:false;
				phys_page &= PHYSPAGE_ADDR;
				PageHandler* handler = MEM_GetPageHandler(phys_page);

				if (dirty) {
					Bitu lin_base = tlb_index << 12;
					paging.tlb.writehandler[tlb_index] = handler;
					if (handler->getFlags()&PFLAG_WRITEABLE)
						paging.tlb.write[tlb_index] = handler->GetHostWritePt(phys_page)-lin_base;
					else paging.tlb.write[tlb_index] = 0;
				} else {
					paging.tlb.writehandler[tlb_index] = &foiling_handler;
					paging.tlb.write[tlb_index] = 0;
				}
			}
		}
	}
}

#else

static INLINE void InitTLBInt(tlb_entry *bank) {
 	for (Bitu i=0;i<TLB_SIZE;i++) {
		bank[i].read=0;
		bank[i].write=0;
		bank[i].readhandler=&init_page_handler;
		bank[i].writehandler=&init_page_handler;
 	}
}

void PAGING_InitTLBBank(tlb_entry **bank) {
	*bank = (tlb_entry *)malloc(sizeof(tlb_entry)*TLB_SIZE);
	if(!*bank) E_Exit("Out of Memory");
	InitTLBInt(*bank);
}

void PAGING_InitTLB(void) {
	InitTLBInt(paging.tlbh);
 	paging.links.used=0;
}

void PAGING_ClearTLB(void) {
	Bit32u * entries=&paging.links.entries[0];
	for (;paging.links.used>0;paging.links.used--) {
		Bitu page=*entries++;
		tlb_entry *entry = get_tlb_entry(page<<12);
		entry->read=0;
		entry->write=0;
		entry->readhandler=&init_page_handler;
		entry->writehandler=&init_page_handler;
	}
	paging.links.used=0;
}

void PAGING_UnlinkPages(Bitu lin_page,Bitu pages) {
	for (;pages>0;pages--) {
		tlb_entry *entry = get_tlb_entry(lin_page<<12);
		entry->read=0;
		entry->write=0;
		entry->readhandler=&init_page_handler;
		entry->writehandler=&init_page_handler;
		lin_page++;
	}
}

void PAGING_MapPage(Bitu lin_page,Bitu phys_page) {
	if (lin_page<LINK_START) {
		paging.firstmb[lin_page]=phys_page;
		paging.tlbh[lin_page].read=0;
		paging.tlbh[lin_page].write=0;
		paging.tlbh[lin_page].readhandler=&init_page_handler;
		paging.tlbh[lin_page].writehandler=&init_page_handler;
	} else {
		PAGING_LinkPage(lin_page,phys_page);
	}
}

void PAGING_LinkPage(Bitu lin_page,Bitu phys_page) {
	PageHandler * handler=MEM_GetPageHandler(phys_page);
	Bitu lin_base=lin_page << 12;
	if (lin_page>=(TLB_SIZE*(TLB_BANKS+1)) || phys_page>=(TLB_SIZE*(TLB_BANKS+1))) 
		E_Exit("Illegal page");

	if (paging.links.used>=PAGING_LINKS) {
		LOG(LOG_PAGING,LOG_NORMAL)("Not enough paging links, resetting cache");
		PAGING_ClearTLB();
	}

	tlb_entry *entry = get_tlb_entry(lin_base);
	entry->phys_page=phys_page;
	if (handler->getFlags() & PFLAG_READABLE) entry->read=handler->GetHostReadPt(phys_page)-lin_base;
	else entry->read=0;
	if (handler->getFlags() & PFLAG_WRITEABLE) entry->write=handler->GetHostWritePt(phys_page)-lin_base;
	else entry->write=0;

 	paging.links.entries[paging.links.used++]=lin_page;
	entry->readhandler=handler;
	entry->writehandler=handler;
}

void PAGING_LinkPage_ReadOnly(Bitu lin_page,Bitu phys_page) {
	PageHandler * handler=MEM_GetPageHandler(phys_page);
	Bitu lin_base=lin_page << 12;
	if (lin_page>=(TLB_SIZE*(TLB_BANKS+1)) || phys_page>=(TLB_SIZE*(TLB_BANKS+1))) 
		E_Exit("Illegal page");

	if (paging.links.used>=PAGING_LINKS) {
		LOG(LOG_PAGING,LOG_NORMAL)("Not enough paging links, resetting cache");
		PAGING_ClearTLB();
	}

	tlb_entry *entry = get_tlb_entry(lin_base);
	entry->phys_page=phys_page;
	if (handler->getFlags() & PFLAG_READABLE) entry->read=handler->GetHostReadPt(phys_page)-lin_base;
	else entry->read=0;
	entry->write=0;

 	paging.links.entries[paging.links.used++]=lin_page;
	entry->readhandler=handler;
	entry->writehandler=&init_page_handler_userro;
}

#endif


void PAGING_SetDirBase(Bitu cr3) {
	paging.cr3=cr3;
	
	paging.base.page=cr3 >> 12;
	paging.base.addr=cr3 & ~0xFFF;
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

void PAGING_Enable(bool enabled) {
	/* If paging is disabled, we work from a default paging table */
	if (paging.enabled==enabled) return;
	paging.enabled=enabled;
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
	Bitu i;

	// log
	LOG(LOG_MISC,LOG_DEBUG)("Initializing paging system (CPU linear -> physical mapping system)");

	/* Setup default Page Directory, force it to update */
	paging.enabled=false;
	paging.wp=false;
	PAGING_InitTLB();
	for (i=0;i<LINK_START;i++) paging.firstmb[i]=i;
	pf_queue.used=0;
}

