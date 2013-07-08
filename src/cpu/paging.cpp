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

/* $Id: paging.cpp,v 1.36 2009-05-27 09:15:41 qbix79 Exp $ */

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

#define LINK_TOTAL		(64*1024)

#define USERWRITE_PROHIBITED			((cpu.cpl&cpu.mpl)==3)


PagingBlock paging;


Bitu PageHandler::readb(PhysPt addr) {
	E_Exit("No byte handler for read from %d",addr);	
	return 0;
}
Bitu PageHandler::readw(PhysPt addr) {
	return 
		(readb(addr+0) << 0) |
		(readb(addr+1) << 8);
}
Bitu PageHandler::readd(PhysPt addr) {
	return 
		(readb(addr+0) << 0)  |
		(readb(addr+1) << 8)  |
		(readb(addr+2) << 16) |
		(readb(addr+3) << 24);
}

void PageHandler::writeb(PhysPt addr,Bitu /*val*/) {
	E_Exit("No byte handler for write to %d",addr);	
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

#define PF_QUEUESIZE 16
static struct {
	Bitu used;
	PF_Entry entries[PF_QUEUESIZE];
} pf_queue;

static Bits PageFaultCore(void) {
	CPU_CycleLeft+=CPU_Cycles;
	CPU_Cycles=1;
	Bits ret=CPU_Core_Full_Run();
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

bool first=false;

void PAGING_PageFault(PhysPt lin_addr,Bitu page_addr,Bitu faultcode) {
	/* Save the state of the cpu cores */
	LazyFlags old_lflags;
	memcpy(&old_lflags,&lflags,sizeof(LazyFlags));
	CPU_Decoder * old_cpudecoder;
	old_cpudecoder=cpudecoder;
	cpudecoder=&PageFaultCore;
	paging.cr2=lin_addr;
	PF_Entry * entry=&pf_queue.entries[pf_queue.used++];
	LOG(LOG_PAGING,LOG_NORMAL)("PageFault at %X type [%x] queue %d",lin_addr,faultcode,pf_queue.used);
//	LOG_MSG("EAX:%04X ECX:%04X EDX:%04X EBX:%04X",reg_eax,reg_ecx,reg_edx,reg_ebx);
//	LOG_MSG("CS:%04X EIP:%08X SS:%04x SP:%08X",SegValue(cs),reg_eip,SegValue(ss),reg_esp);
	entry->cs=SegValue(cs);
	entry->eip=reg_eip;
	entry->page_addr=page_addr;
	entry->mpl=cpu.mpl;
	cpu.mpl=3;

	CPU_Exception(EXCEPTION_PF,faultcode);
#if C_DEBUG
//	DEBUG_EnableDebugger();
#endif
	DOSBOX_RunMachine();
	pf_queue.used--;
	LOG(LOG_PAGING,LOG_NORMAL)("Left PageFault for %x queue %d",lin_addr,pf_queue.used);
	memcpy(&lflags,&old_lflags,sizeof(LazyFlags));
	cpudecoder=old_cpudecoder;
//	LOG_MSG("SS:%04x SP:%08X",SegValue(ss),reg_esp);
}

static INLINE void InitPageUpdateLink(Bitu relink,PhysPt addr) {
	if (relink==0) return;
	if (paging.links.used) {
		if (paging.links.entries[paging.links.used-1]==(addr>>12)) {
			paging.links.used--;
			PAGING_UnlinkPages(addr>>12,1);
		}
	}
	if (relink>1) PAGING_LinkPage_ReadOnly(addr>>12,relink);
}

static INLINE void InitPageCheckPresence(PhysPt lin_addr,bool writing,X86PageEntry& table,X86PageEntry& entry) {
	Bitu lin_page=lin_addr >> 12;
	Bitu d_index=lin_page >> 10;
	Bitu t_index=lin_page & 0x3ff;
	Bitu table_addr=(paging.base.page<<12)+d_index*4;
	table.load=phys_readd(table_addr);
	if (!table.block.p) {
		LOG(LOG_PAGING,LOG_NORMAL)("NP Table");
		PAGING_PageFault(lin_addr,table_addr,
			(writing?0x02:0x00) | (((cpu.cpl&cpu.mpl)==0)?0x00:0x04));
		table.load=phys_readd(table_addr);
		if (GCC_UNLIKELY(!table.block.p))
			E_Exit("Pagefault didn't correct table");
	}
	Bitu entry_addr=(table.block.base<<12)+t_index*4;
	entry.load=phys_readd(entry_addr);
	if (!entry.block.p) {
//		LOG(LOG_PAGING,LOG_NORMAL)("NP Page");
		PAGING_PageFault(lin_addr,entry_addr,
			(writing?0x02:0x00) | (((cpu.cpl&cpu.mpl)==0)?0x00:0x04));
		entry.load=phys_readd(entry_addr);
		if (GCC_UNLIKELY(!entry.block.p))
			E_Exit("Pagefault didn't correct page");
	}
}
			
static INLINE bool InitPageCheckPresence_CheckOnly(PhysPt lin_addr,bool writing,X86PageEntry& table,X86PageEntry& entry) {
	Bitu lin_page=lin_addr >> 12;
	Bitu d_index=lin_page >> 10;
	Bitu t_index=lin_page & 0x3ff;
	Bitu table_addr=(paging.base.page<<12)+d_index*4;
	table.load=phys_readd(table_addr);
	if (!table.block.p) {
		paging.cr2=lin_addr;
		cpu.exception.which=EXCEPTION_PF;
		cpu.exception.error=(writing?0x02:0x00) | (((cpu.cpl&cpu.mpl)==0)?0x00:0x04);
		return false;
	}
	Bitu entry_addr=(table.block.base<<12)+t_index*4;
	entry.load=phys_readd(entry_addr);
	if (!entry.block.p) {
		paging.cr2=lin_addr;
		cpu.exception.which=EXCEPTION_PF;
		cpu.exception.error=(writing?0x02:0x00) | (((cpu.cpl&cpu.mpl)==0)?0x00:0x04);
		return false;
	}
	return true;
}

// check if a user-level memory access would trigger a privilege page fault
static INLINE bool InitPage_CheckUseraccess(Bitu u1,Bitu u2) {
	switch (CPU_ArchitectureType) {
	case CPU_ARCHTYPE_MIXED:
	case CPU_ARCHTYPE_386SLOW:
	case CPU_ARCHTYPE_386FAST:
	default:
		return ((u1)==0) && ((u2)==0);
	case CPU_ARCHTYPE_486OLDSLOW:
	case CPU_ARCHTYPE_486NEWSLOW:
	case CPU_ARCHTYPE_PENTIUMSLOW:
		return ((u1)==0) || ((u2)==0);
	}
}


class InitPageHandler : public PageHandler {
public:
	InitPageHandler() {
		flags=PFLAG_INIT|PFLAG_NOCODE;
	}
	Bitu readb(PhysPt addr) {
		Bitu needs_reset=InitPage(addr,false);
		Bit8u val=mem_readb(addr);
		InitPageUpdateLink(needs_reset,addr);
		return val;
	}
	Bitu readw(PhysPt addr) {
		Bitu needs_reset=InitPage(addr,false);
		Bit16u val=mem_readw(addr);
		InitPageUpdateLink(needs_reset,addr);
		return val;
	}
	Bitu readd(PhysPt addr) {
		Bitu needs_reset=InitPage(addr,false);
		Bit32u val=mem_readd(addr);
		InitPageUpdateLink(needs_reset,addr);
		return val;
	}
	void writeb(PhysPt addr,Bitu val) {
		Bitu needs_reset=InitPage(addr,true);
		mem_writeb(addr,val);
		InitPageUpdateLink(needs_reset,addr);
	}
	void writew(PhysPt addr,Bitu val) {
		Bitu needs_reset=InitPage(addr,true);
		mem_writew(addr,val);
		InitPageUpdateLink(needs_reset,addr);
	}
	void writed(PhysPt addr,Bitu val) {
		Bitu needs_reset=InitPage(addr,true);
		mem_writed(addr,val);
		InitPageUpdateLink(needs_reset,addr);
	}
	bool readb_checked(PhysPt addr, Bit8u * val) {
		if (InitPageCheckOnly(addr,false)) {
			*val=mem_readb(addr);
			return false;
		} else return true;
	}
	bool readw_checked(PhysPt addr, Bit16u * val) {
		if (InitPageCheckOnly(addr,false)){
			*val=mem_readw(addr);
			return false;
		} else return true;
	}
	bool readd_checked(PhysPt addr, Bit32u * val) {
		if (InitPageCheckOnly(addr,false)) {
			*val=mem_readd(addr);
			return false;
		} else return true;
	}
	bool writeb_checked(PhysPt addr,Bitu val) {
		if (InitPageCheckOnly(addr,true)) {
			mem_writeb(addr,val);
			return false;
		} else return true;
	}
	bool writew_checked(PhysPt addr,Bitu val) {
		if (InitPageCheckOnly(addr,true)) {
			mem_writew(addr,val);
			return false;
		} else return true;
	}
	bool writed_checked(PhysPt addr,Bitu val) {
		if (InitPageCheckOnly(addr,true)) {
			mem_writed(addr,val);
			return false;
		} else return true;
	}
	Bitu InitPage(Bitu lin_addr,bool writing) {
		Bitu lin_page=lin_addr >> 12;
		Bitu phys_page;
		if (paging.enabled) {
			X86PageEntry table;
			X86PageEntry entry;
			InitPageCheckPresence(lin_addr,writing,table,entry);

			// 0: no action
			// 1: can (but currently does not) fail a user-level access privilege check
			// 2: can (but currently does not) fail a write privilege check
			// 3: fails a privilege check
			Bitu priv_check=0;
			if (InitPage_CheckUseraccess(entry.block.us,table.block.us)) {
				if ((cpu.cpl&cpu.mpl)==3) priv_check=3;
				else {
					switch (CPU_ArchitectureType) {
					case CPU_ARCHTYPE_MIXED:
					case CPU_ARCHTYPE_386FAST:
					default:
//						priv_check=0;	// default
						break;
					case CPU_ARCHTYPE_386SLOW:
					case CPU_ARCHTYPE_486OLDSLOW:
					case CPU_ARCHTYPE_486NEWSLOW:
					case CPU_ARCHTYPE_PENTIUMSLOW:
						priv_check=1;
						break;
					}
				}
			}
			if ((entry.block.wr==0) || (table.block.wr==0)) {
				// page is write-protected for user mode
				if (priv_check==0) {
					switch (CPU_ArchitectureType) {
					case CPU_ARCHTYPE_MIXED:
					case CPU_ARCHTYPE_386FAST:
					default:
//						priv_check=0;	// default
						break;
					case CPU_ARCHTYPE_386SLOW:
					case CPU_ARCHTYPE_486OLDSLOW:
					case CPU_ARCHTYPE_486NEWSLOW:
					case CPU_ARCHTYPE_PENTIUMSLOW:
						priv_check=2;
						break;
					}
				}
				// check if actually failing the write-protected check
				if (writing && USERWRITE_PROHIBITED) priv_check=3;
			}
			if (priv_check==3) {
				LOG(LOG_PAGING,LOG_NORMAL)("Page access denied: cpl=%i, %x:%x:%x:%x",
					cpu.cpl,entry.block.us,table.block.us,entry.block.wr,table.block.wr);
				PAGING_PageFault(lin_addr,(table.block.base<<12)+(lin_page & 0x3ff)*4,0x05 | (writing?0x02:0x00));
				priv_check=0;
			}

			if (!table.block.a) {
				table.block.a=1;		// set page table accessed
				phys_writed((paging.base.page<<12)+(lin_page >> 10)*4,table.load);
			}
			if ((!entry.block.a) || (!entry.block.d)) {
				entry.block.a=1;		// set page accessed

				// page is dirty if we're writing to it, or if we're reading but the
				// page will be fully linked so we can't track later writes
				if (writing || (priv_check==0)) entry.block.d=1;		// mark page as dirty

				phys_writed((table.block.base<<12)+(lin_page & 0x3ff)*4,entry.load);
			}

			phys_page=entry.block.base;
			
			// now see how the page should be linked best, if we need to catch privilege
			// checks later on it should be linked as read-only page
			if (priv_check==0) {
				// if reading we could link the page as read-only to later cacth writes,
				// will slow down pretty much but allows catching all dirty events
				PAGING_LinkPage(lin_page,phys_page);
			} else {
				if (priv_check==1) {
					PAGING_LinkPage(lin_page,phys_page);
					return 1;
				} else if (writing) {
					PageHandler * handler=MEM_GetPageHandler(phys_page);
					PAGING_LinkPage(lin_page,phys_page);
					if (!(handler->flags & PFLAG_READABLE)) return 1;
					if (!(handler->flags & PFLAG_WRITEABLE)) return 1;
					if (get_tlb_read(lin_addr)!=get_tlb_write(lin_addr)) return 1;
					if (phys_page>1) return phys_page;
					else return 1;
				} else {
					PAGING_LinkPage_ReadOnly(lin_page,phys_page);
				}
			}
		} else {
			if (lin_page<LINK_START) phys_page=paging.firstmb[lin_page];
			else phys_page=lin_page;
			PAGING_LinkPage(lin_page,phys_page);
		}
		return 0;
	}
	bool InitPageCheckOnly(Bitu lin_addr,bool writing) {
		Bitu lin_page=lin_addr >> 12;
		if (paging.enabled) {
			X86PageEntry table;
			X86PageEntry entry;
			if (!InitPageCheckPresence_CheckOnly(lin_addr,writing,table,entry)) return false;

			if (!USERWRITE_PROHIBITED) return true;

			if (InitPage_CheckUseraccess(entry.block.us,table.block.us) ||
					(((entry.block.wr==0) || (table.block.wr==0)) && writing)) {
				LOG(LOG_PAGING,LOG_NORMAL)("Page access denied: cpl=%i, %x:%x:%x:%x",
					cpu.cpl,entry.block.us,table.block.us,entry.block.wr,table.block.wr);
				paging.cr2=lin_addr;
				cpu.exception.which=EXCEPTION_PF;
				cpu.exception.error=0x05 | (writing?0x02:0x00);
				return false;
			}
		} else {
			Bitu phys_page;
			if (lin_page<LINK_START) phys_page=paging.firstmb[lin_page];
			else phys_page=lin_page;
			PAGING_LinkPage(lin_page,phys_page);
		}
		return true;
	}
	void InitPageForced(Bitu lin_addr) {
		Bitu lin_page=lin_addr >> 12;
		Bitu phys_page;
		if (paging.enabled) {
			X86PageEntry table;
			X86PageEntry entry;
			InitPageCheckPresence(lin_addr,false,table,entry);

			if (!table.block.a) {
				table.block.a=1;		//Set access
				phys_writed((paging.base.page<<12)+(lin_page >> 10)*4,table.load);
			}
			if (!entry.block.a) {
				entry.block.a=1;					//Set access
				phys_writed((table.block.base<<12)+(lin_page & 0x3ff)*4,entry.load);
			}
			phys_page=entry.block.base;
			// maybe use read-only page here if possible
		} else {
			if (lin_page<LINK_START) phys_page=paging.firstmb[lin_page];
			else phys_page=lin_page;
		}
		PAGING_LinkPage(lin_page,phys_page);
	}
};

class InitPageUserROHandler : public PageHandler {
public:
	InitPageUserROHandler() {
		flags=PFLAG_INIT|PFLAG_NOCODE;
	}
	void writeb(PhysPt addr,Bitu val) {
		InitPage(addr,(Bit8u)(val&0xff));
		host_writeb(get_tlb_read(addr)+addr,(Bit8u)(val&0xff));
	}
	void writew(PhysPt addr,Bitu val) {
		InitPage(addr,(Bit16u)(val&0xffff));
		host_writew(get_tlb_read(addr)+addr,(Bit16u)(val&0xffff));
	}
	void writed(PhysPt addr,Bitu val) {
		InitPage(addr,(Bit32u)val);
		host_writed(get_tlb_read(addr)+addr,(Bit32u)val);
	}
	bool writeb_checked(PhysPt addr,Bitu val) {
		Bitu writecode=InitPageCheckOnly(addr,(Bit8u)(val&0xff));
		if (writecode) {
			HostPt tlb_addr;
			if (writecode>1) tlb_addr=get_tlb_read(addr);
			else tlb_addr=get_tlb_write(addr);
			host_writeb(tlb_addr+addr,(Bit8u)(val&0xff));
			return false;
		}
		return true;
	}
	bool writew_checked(PhysPt addr,Bitu val) {
		Bitu writecode=InitPageCheckOnly(addr,(Bit16u)(val&0xffff));
		if (writecode) {
			HostPt tlb_addr;
			if (writecode>1) tlb_addr=get_tlb_read(addr);
			else tlb_addr=get_tlb_write(addr);
			host_writew(tlb_addr+addr,(Bit16u)(val&0xffff));
			return false;
		}
		return true;
	}
	bool writed_checked(PhysPt addr,Bitu val) {
		Bitu writecode=InitPageCheckOnly(addr,(Bit32u)val);
		if (writecode) {
			HostPt tlb_addr;
			if (writecode>1) tlb_addr=get_tlb_read(addr);
			else tlb_addr=get_tlb_write(addr);
			host_writed(tlb_addr+addr,(Bit32u)val);
			return false;
		}
		return true;
	}
	void InitPage(Bitu lin_addr,Bitu val) {
		Bitu lin_page=lin_addr >> 12;
		Bitu phys_page;
		if (paging.enabled) {
			if (!USERWRITE_PROHIBITED) return;

			X86PageEntry table;
			X86PageEntry entry;
			InitPageCheckPresence(lin_addr,true,table,entry);

			LOG(LOG_PAGING,LOG_NORMAL)("Page access denied: cpl=%i, %x:%x:%x:%x",
				cpu.cpl,entry.block.us,table.block.us,entry.block.wr,table.block.wr);
			PAGING_PageFault(lin_addr,(table.block.base<<12)+(lin_page & 0x3ff)*4,0x07);

			if (!table.block.a) {
				table.block.a=1;		//Set access
				phys_writed((paging.base.page<<12)+(lin_page >> 10)*4,table.load);
			}
			if ((!entry.block.a) || (!entry.block.d)) {
				entry.block.a=1;	//Set access
				entry.block.d=1;	//Set dirty
				phys_writed((table.block.base<<12)+(lin_page & 0x3ff)*4,entry.load);
			}
			phys_page=entry.block.base;
			PAGING_LinkPage(lin_page,phys_page);
		} else {
			if (lin_page<LINK_START) phys_page=paging.firstmb[lin_page];
			else phys_page=lin_page;
			PAGING_LinkPage(lin_page,phys_page);
		}
	}
	Bitu InitPageCheckOnly(Bitu lin_addr,Bitu val) {
		Bitu lin_page=lin_addr >> 12;
		if (paging.enabled) {
			if (!USERWRITE_PROHIBITED) return 2;

			X86PageEntry table;
			X86PageEntry entry;
			if (!InitPageCheckPresence_CheckOnly(lin_addr,true,table,entry)) return 0;

			if (InitPage_CheckUseraccess(entry.block.us,table.block.us) || (((entry.block.wr==0) || (table.block.wr==0)))) {
				LOG(LOG_PAGING,LOG_NORMAL)("Page access denied: cpl=%i, %x:%x:%x:%x",
					cpu.cpl,entry.block.us,table.block.us,entry.block.wr,table.block.wr);
				paging.cr2=lin_addr;
				cpu.exception.which=EXCEPTION_PF;
				cpu.exception.error=0x07;
				return 0;
			}
			PAGING_LinkPage(lin_page,entry.block.base);
		} else {
			Bitu phys_page;
			if (lin_page<LINK_START) phys_page=paging.firstmb[lin_page];
			else phys_page=lin_page;
			PAGING_LinkPage(lin_page,phys_page);
		}
		return 1;
	}
	void InitPageForced(Bitu lin_addr) {
		Bitu lin_page=lin_addr >> 12;
		Bitu phys_page;
		if (paging.enabled) {
			X86PageEntry table;
			X86PageEntry entry;
			InitPageCheckPresence(lin_addr,true,table,entry);

			if (!table.block.a) {
				table.block.a=1;		//Set access
				phys_writed((paging.base.page<<12)+(lin_page >> 10)*4,table.load);
			}
			if (!entry.block.a) {
				entry.block.a=1;	//Set access
				phys_writed((table.block.base<<12)+(lin_page & 0x3ff)*4,entry.load);
			}
			phys_page=entry.block.base;
		} else {
			if (lin_page<LINK_START) phys_page=paging.firstmb[lin_page];
			else phys_page=lin_page;
		}
		PAGING_LinkPage(lin_page,phys_page);
	}
};


bool PAGING_MakePhysPage(Bitu & page) {
	if (paging.enabled) {
		Bitu d_index=page >> 10;
		Bitu t_index=page & 0x3ff;
		X86PageEntry table;
		table.load=phys_readd((paging.base.page<<12)+d_index*4);
		if (!table.block.p) return false;
		X86PageEntry entry;
		entry.load=phys_readd((table.block.base<<12)+t_index*4);
		if (!entry.block.p) return false;
		page=entry.block.base;
	} else {
		if (page<LINK_START) page=paging.firstmb[page];
		//Else keep it the same
	}
	return true;
}

static InitPageHandler init_page_handler;
static InitPageUserROHandler init_page_handler_userro;


Bitu PAGING_GetDirBase(void) {
	return paging.cr3;
}

bool PAGING_ForcePageInit(Bitu lin_addr) {
	PageHandler * handler=get_tlb_readhandler(lin_addr);
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

#if defined(USE_FULL_TLB)
void PAGING_InitTLB(void) {
	for (Bitu i=0;i<TLB_SIZE;i++) {
		paging.tlb.read[i]=0;
		paging.tlb.write[i]=0;
		paging.tlb.readhandler[i]=&init_page_handler;
		paging.tlb.writehandler[i]=&init_page_handler;
	}
	paging.links.used=0;
}

void PAGING_ClearTLB(void) {
	Bit32u * entries=&paging.links.entries[0];
	for (;paging.links.used>0;paging.links.used--) {
		Bitu page=*entries++;
		paging.tlb.read[page]=0;
		paging.tlb.write[page]=0;
		paging.tlb.readhandler[page]=&init_page_handler;
		paging.tlb.writehandler[page]=&init_page_handler;
	}
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
	if (handler->flags & PFLAG_READABLE) paging.tlb.read[lin_page]=handler->GetHostReadPt(phys_page)-lin_base;
	else paging.tlb.read[lin_page]=0;
	if (handler->flags & PFLAG_WRITEABLE) paging.tlb.write[lin_page]=handler->GetHostWritePt(phys_page)-lin_base;
	else paging.tlb.write[lin_page]=0;

	paging.links.entries[paging.links.used++]=lin_page;
	paging.tlb.readhandler[lin_page]=handler;
	paging.tlb.writehandler[lin_page]=handler;
}

void PAGING_LinkPage_ReadOnly(Bitu lin_page,Bitu phys_page) {
	PageHandler * handler=MEM_GetPageHandler(phys_page);
	Bitu lin_base=lin_page << 12;
	if (lin_page>=TLB_SIZE || phys_page>=TLB_SIZE) 
		E_Exit("Illegal page");

	if (paging.links.used>=PAGING_LINKS) {
		LOG(LOG_PAGING,LOG_NORMAL)("Not enough paging links, resetting cache");
		PAGING_ClearTLB();
	}

	paging.tlb.phys_page[lin_page]=phys_page;
	if (handler->flags & PFLAG_READABLE) paging.tlb.read[lin_page]=handler->GetHostReadPt(phys_page)-lin_base;
	else paging.tlb.read[lin_page]=0;
	paging.tlb.write[lin_page]=0;

	paging.links.entries[paging.links.used++]=lin_page;
	paging.tlb.readhandler[lin_page]=handler;
	paging.tlb.writehandler[lin_page]=&init_page_handler_userro;
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
	if (handler->flags & PFLAG_READABLE) entry->read=handler->GetHostReadPt(phys_page)-lin_base;
	else entry->read=0;
	if (handler->flags & PFLAG_WRITEABLE) entry->write=handler->GetHostWritePt(phys_page)-lin_base;
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
	if (handler->flags & PFLAG_READABLE) entry->read=handler->GetHostReadPt(phys_page)-lin_base;
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
	paging.base.addr=cr3 & ~4095;
//	LOG(LOG_PAGING,LOG_NORMAL)("CR3:%X Base %X",cr3,paging.base.page);
	if (paging.enabled) {
		PAGING_ClearTLB();
	}
}

void PAGING_Enable(bool enabled) {
	/* If paging is disable we work from a default paging table */
	if (paging.enabled==enabled) return;
	paging.enabled=enabled;
	if (enabled) {
		if (GCC_UNLIKELY(cpudecoder==CPU_Core_Simple_Run)) {
//			LOG_MSG("CPU core simple won't run this game,switching to normal");
			cpudecoder=CPU_Core_Normal_Run;
			CPU_CycleLeft+=CPU_Cycles;
			CPU_Cycles=0;
		}
//		LOG(LOG_PAGING,LOG_NORMAL)("Enabled");
		PAGING_SetDirBase(paging.cr3);
	}
	PAGING_ClearTLB();
}

bool PAGING_Enabled(void) {
	return paging.enabled;
}

class PAGING:public Module_base{
public:
	PAGING(Section* configuration):Module_base(configuration){
		/* Setup default Page Directory, force it to update */
		paging.enabled=false;
		PAGING_InitTLB();
		Bitu i;
		for (i=0;i<LINK_START;i++) {
			paging.firstmb[i]=i;
		}
		pf_queue.used=0;
	}
	~PAGING(){}
};

static PAGING* test;
void PAGING_Init(Section * sec) {
	test = new PAGING(sec);
}
