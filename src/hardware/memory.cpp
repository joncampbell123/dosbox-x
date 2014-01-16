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


#include <stdint.h>
#include "dosbox.h"
#include "mem.h"
#include "inout.h"
#include "setup.h"
#include "paging.h"
#include "regs.h"
#ifndef WIN32
# include <stdlib.h>
# include <unistd.h>
# include <stdio.h>
#endif

#include "voodoo.h"

#include <string.h>

#define PAGES_IN_BLOCK	((1024*1024)/MEM_PAGE_SIZE)
#define SAFE_MEMORY	32
#define MAX_MEMORY	512
#define MAX_PAGE_ENTRIES (MAX_MEMORY*1024*1024/4096)
#define LFB_PAGES	512
#define MAX_LINKS	((MAX_MEMORY*1024/4)+4096)		//Hopefully enough

struct LinkBlock {
	Bitu used;
	Bit32u pages[MAX_LINKS];
};

static struct MemoryBlock {
	Bitu pages;
	Bitu reported_pages;
	PageHandler * * phandlers;
	MemHandle * mhandles;
	LinkBlock links;
	struct	{
		Bitu		start_page;
		Bitu		end_page;
		Bitu		pages;
		PageHandler *handler;
		PageHandler *mmiohandler;
	} lfb;
	struct {
		bool enabled;
		Bit8u controlport;
	} a20;
	Bit32u mem_alias_pagemask;
} memory;

HostPt MemBase;

namespace
{
size_t memorySize;
}


class IllegalPageHandler : public PageHandler {
public:
	IllegalPageHandler() : PageHandler(PFLAG_INIT|PFLAG_NOCODE) {}
	Bitu readb(PhysPt addr) {
#if C_DEBUG
		LOG_MSG("Warning: Illegal read from %x, CS:IP %8x:%8x",addr,SegValue(cs),reg_eip);
#else
		static Bits lcount=0;
		if (lcount<1000) {
			lcount++;
			//LOG_MSG("Warning: Illegal read from %x, CS:IP %8x:%8x",addr,SegValue(cs),reg_eip);
		}
#endif
		return 0xFF; /* Real hardware returns 0xFF not 0x00 */
	} 
	void writeb(PhysPt addr,Bitu val) {
#if C_DEBUG
		LOG_MSG("Warning: Illegal write to %x, CS:IP %8x:%8x",addr,SegValue(cs),reg_eip);
#else
		static Bits lcount=0;
		if (lcount<1000) {
			lcount++;
			//LOG_MSG("Warning: Illegal write to %x, CS:IP %8x:%8x",addr,SegValue(cs),reg_eip);
		}
#endif
	}
};

class RAMPageHandler : public PageHandler {
public:
	RAMPageHandler() : PageHandler(PFLAG_READABLE|PFLAG_WRITEABLE) {}
	RAMPageHandler(Bitu flags) : PageHandler(flags) {}
	HostPt GetHostReadPt(Bitu phys_page) {
		return MemBase+phys_page*MEM_PAGESIZE;
	}
	HostPt GetHostWritePt(Bitu phys_page) {
		return MemBase+phys_page*MEM_PAGESIZE;
	}
};

class RAMAliasPageHandler : public PageHandler {
public:
	RAMAliasPageHandler() {
		flags=PFLAG_READABLE|PFLAG_WRITEABLE;
	}
	HostPt GetHostReadPt(Bitu phys_page) {
		return MemBase+(phys_page&memory.mem_alias_pagemask)*MEM_PAGESIZE;
	}
	HostPt GetHostWritePt(Bitu phys_page) {
		return MemBase+(phys_page&memory.mem_alias_pagemask)*MEM_PAGESIZE;
	}
};

class ROMPageHandler : public RAMAliasPageHandler {
public:
	ROMPageHandler() {
		flags=PFLAG_READABLE|PFLAG_HASROM;
	}
	void writeb(PhysPt addr,Bitu val){
		LOG(LOG_CPU,LOG_ERROR)("Write %x to rom at %x",val,addr);
	}
	void writew(PhysPt addr,Bitu val){
		LOG(LOG_CPU,LOG_ERROR)("Write %x to rom at %x",val,addr);
	}
	void writed(PhysPt addr,Bitu val){
		LOG(LOG_CPU,LOG_ERROR)("Write %x to rom at %x",val,addr);
	}
};



static IllegalPageHandler illegal_page_handler;
static RAMAliasPageHandler ram_alias_page_handler;
static RAMPageHandler ram_page_handler;
static ROMPageHandler rom_page_handler;

void MEM_SetLFB(Bitu page, Bitu pages, PageHandler *handler, PageHandler *mmiohandler) {
	memory.lfb.handler=handler;
	memory.lfb.mmiohandler=mmiohandler;
	memory.lfb.start_page=page;
	memory.lfb.end_page=page+pages;
	memory.lfb.pages=pages;
	PAGING_ClearTLB();
}

PageHandler * MEM_GetPageHandler(Bitu phys_page) {
	phys_page &= memory.mem_alias_pagemask;
	if (phys_page<memory.pages) {
		return memory.phandlers[phys_page];
	} else if ((phys_page>=memory.lfb.start_page) && (phys_page<memory.lfb.end_page)) {
		return memory.lfb.handler;
	} else if ((phys_page>=memory.lfb.start_page+0x01000000/4096) &&
				(phys_page<memory.lfb.start_page+0x01000000/4096+16)) {
		return memory.lfb.mmiohandler;
	} else if (VOODOO_PCI_CheckLFBPage(phys_page)) {
		return VOODOO_GetPageHandler();
	}
	return &illegal_page_handler;
}

void MEM_SetPageHandler(Bitu phys_page,Bitu pages,PageHandler * handler) {
	for (;pages>0;pages--) {
		memory.phandlers[phys_page]=handler;
		phys_page++;
	}
}

void MEM_ResetPageHandler(Bitu phys_page, Bitu pages) {
	PageHandler *ram_ptr =
		memory.mem_alias_pagemask == (Bit32u)(~0UL)
		? (PageHandler*)(&ram_page_handler) /* no aliasing */
		: (PageHandler*)(&ram_alias_page_handler); /* aliasing */
	for (;pages>0;pages--) {
		memory.phandlers[phys_page]=ram_ptr;
		phys_page++;
	}
}

Bitu mem_strlen(PhysPt pt) {
	Bitu x=0;
	while (x<1024) {
		if (!mem_readb_inline(pt+x)) return x;
		x++;
	}
	return 0;		//Hope this doesn't happen
}

void mem_strcpy(PhysPt dest,PhysPt src) {
	Bit8u r;
	while ( (r = mem_readb(src++)) ) mem_writeb_inline(dest++,r);
	mem_writeb_inline(dest,0);
}

void mem_memcpy(PhysPt dest,PhysPt src,Bitu size) {
	while (size--) mem_writeb_inline(dest++,mem_readb_inline(src++));
}

void MEM_BlockRead(PhysPt pt,void * data,Bitu size) {
	Bit8u * write=reinterpret_cast<Bit8u *>(data);
	while (size--) {
		*write++=mem_readb_inline(pt++);
	}
}

void MEM_BlockWrite(PhysPt pt,void const * const data,Bitu size) {
	Bit8u const * read = reinterpret_cast<Bit8u const * const>(data);
	if (size==0)
		return;

	if ((pt >> 12) == ((pt+size-1)>>12)) { // Always same TLB entry
		HostPt tlb_addr=get_tlb_write(pt);
		if (!tlb_addr) {
			Bit8u val = *read++;
			get_tlb_writehandler(pt)->writeb(pt,val);
			tlb_addr=get_tlb_write(pt);
			pt++; size--;
			if (!tlb_addr) {
				// Slow path
				while (size--) {
					mem_writeb_inline(pt++,*read++);
				}
				return;
			}
		}
		// Fast path
		memcpy(tlb_addr+pt, read, size);
	}
	else {
		const Bitu current = (((pt>>12)+1)<<12) - pt;
		Bitu remainder = size - current;
		MEM_BlockWrite(pt, data, current);
		MEM_BlockWrite(pt+current, reinterpret_cast<Bit8u const * const>(data)+current, remainder);
	}
}

void MEM_BlockRead32(PhysPt pt,void * data,Bitu size) {
	Bit32u * write=(Bit32u *) data;
	size>>=2;
	while (size--) {
		*write++=mem_readd_inline(pt);
		pt+=4;
	}
}

void MEM_BlockWrite32(PhysPt pt,void * data,Bitu size) {
	Bit32u * read=(Bit32u *) data;
	size>>=2;
	while (size--) {
		mem_writed_inline(pt,*read++);
		pt+=4;
	}
}

void MEM_BlockCopy(PhysPt dest,PhysPt src,Bitu size) {
	mem_memcpy(dest,src,size);
}

void MEM_StrCopy(PhysPt pt,char * data,Bitu size) {
	while (size--) {
		Bit8u r=mem_readb_inline(pt++);
		if (!r) break;
		*data++=r;
	}
	*data=0;
}

Bitu MEM_TotalPages(void) {
	return memory.reported_pages;
}

Bitu MEM_FreeLargest(void) {
	Bitu size=0;Bitu largest=0;
	Bitu index=XMS_START;	
	while (index<memory.reported_pages) {
		if (!memory.mhandles[index]) {
			size++;
		} else {
			if (size>largest) largest=size;
			size=0;
		}
		index++;
	}
	if (size>largest) largest=size;
	return largest;
}

Bitu MEM_FreeTotal(void) {
	Bitu free=0;
	Bitu index=XMS_START;	
	while (index<memory.reported_pages) {
		if (!memory.mhandles[index]) free++;
		index++;
	}
	return free;
}

Bitu MEM_AllocatedPages(MemHandle handle) 
{
	Bitu pages = 0;
	while (handle>0) {
		pages++;
		handle=memory.mhandles[handle];
	}
	return pages;
}

//TODO Maybe some protection for this whole allocation scheme

INLINE Bitu BestMatch(Bitu size) {
	Bitu index=XMS_START;	
	Bitu first=0;
	Bitu best=0xfffffff;
	Bitu best_first=0;
	while (index<memory.reported_pages) {
		/* Check if we are searching for first free page */
		if (!first) {
			/* Check if this is a free page */
			if (!memory.mhandles[index]) {
				first=index;	
			}
		} else {
			/* Check if this still is used page */
			if (memory.mhandles[index]) {
				Bitu pages=index-first;
				if (pages==size) {
					return first;
				} else if (pages>size) {
					if (pages<best) {
						best=pages;
						best_first=first;
					}
				}
				first=0;			//Always reset for new search
			}
		}
		index++;
	}
	/* Check for the final block if we can */
	if (first && (index-first>=size) && (index-first<best)) {
		return first;
	}
	return best_first;
}

MemHandle MEM_AllocatePages(Bitu pages,bool sequence) {
	MemHandle ret;
	if (!pages) return 0;
	if (sequence) {
		Bitu index=BestMatch(pages);
		if (!index) return 0;
		MemHandle * next=&ret;
		while (pages) {
			*next=index;
			next=&memory.mhandles[index];
			index++;pages--;
		}
		*next=-1;
	} else {
		if (MEM_FreeTotal()<pages) return 0;
		MemHandle * next=&ret;
		while (pages) {
			Bitu index=BestMatch(1);
			if (!index) E_Exit("MEM:corruption during allocate");
			while (pages && (!memory.mhandles[index])) {
				*next=index;
				next=&memory.mhandles[index];
				index++;pages--;
			}
			*next=-1;		//Invalidate it in case we need another match
		}
	}
	return ret;
}

MemHandle MEM_GetNextFreePage(void) {
	return (MemHandle)BestMatch(1);
}

void MEM_ReleasePages(MemHandle handle) {
	while (handle>0) {
		MemHandle next=memory.mhandles[handle];
		memory.mhandles[handle]=0;
		handle=next;
	}
}

bool MEM_ReAllocatePages(MemHandle & handle,Bitu pages,bool sequence) {
	if (handle<=0) {
		if (!pages) return true;
		handle=MEM_AllocatePages(pages,sequence);
		return (handle>0);
	}
	if (!pages) {
		MEM_ReleasePages(handle);
		handle=-1;
		return true;
	}
	MemHandle index=handle;
	MemHandle last;Bitu old_pages=0;
	while (index>0) {
		old_pages++;
		last=index;
		index=memory.mhandles[index];
	}
	if (old_pages == pages) return true;
	if (old_pages > pages) {
		/* Decrease size */
		pages--;index=handle;old_pages--;
		while (pages) {
			index=memory.mhandles[index];
			pages--;old_pages--;
		}
		MemHandle next=memory.mhandles[index];
		memory.mhandles[index]=-1;
		index=next;
		while (old_pages) {
			next=memory.mhandles[index];
			memory.mhandles[index]=0;
			index=next;
			old_pages--;
		}
		return true;
	} else {
		/* Increase size, check for enough free space */
		Bitu need=pages-old_pages;
		if (sequence) {
			index=last+1;
			Bitu free=0;
			while ((index<(MemHandle)memory.reported_pages) && !memory.mhandles[index]) {
				index++;free++;
			}
			if (free>=need) {
				/* Enough space allocate more pages */
				index=last;
				while (need) {
					memory.mhandles[index]=index+1;
					need--;index++;
				}
				memory.mhandles[index]=-1;
				return true;
			} else {
				/* Not Enough space allocate new block and copy */
				MemHandle newhandle=MEM_AllocatePages(pages,true);
				if (!newhandle) return false;
				MEM_BlockCopy(newhandle*4096,handle*4096,old_pages*4096);
				MEM_ReleasePages(handle);
				handle=newhandle;
				return true;
			}
		} else {
			MemHandle rem=MEM_AllocatePages(need,false);
			if (!rem) return false;
			memory.mhandles[last]=rem;
			return true;
		}
	}
	return 0;
}

MemHandle MEM_NextHandle(MemHandle handle) {
	return memory.mhandles[handle];
}

MemHandle MEM_NextHandleAt(MemHandle handle,Bitu where) {
	while (where) {
		where--;	
		handle=memory.mhandles[handle];
	}
	return handle;
}


/* 
	A20 line handling, 
	Basically maps the 4 pages at the 1mb to 0mb in the default page directory
*/
bool MEM_A20_Enabled(void) {
	return memory.a20.enabled;
}

void MEM_A20_Enable(bool enabled) {
	Bitu phys_base=enabled ? (1024/4) : 0;
	for (Bitu i=0;i<16;i++) PAGING_MapPage((1024/4)+i,phys_base+i);
	memory.a20.enabled=enabled;
}


/* Memory access functions */
Bit16u mem_unalignedreadw(PhysPt address) {
	Bit16u ret = mem_readb_inline(address);
	ret       |= mem_readb_inline(address+1) << 8;
	return ret;
}

Bit32u mem_unalignedreadd(PhysPt address) {
	Bit32u ret = mem_readb_inline(address);
	ret       |= mem_readb_inline(address+1) << 8;
	ret       |= mem_readb_inline(address+2) << 16;
	ret       |= mem_readb_inline(address+3) << 24;
	return ret;
}


void mem_unalignedwritew(PhysPt address,Bit16u val) {
	mem_writeb_inline(address,(Bit8u)val);val>>=8;
	mem_writeb_inline(address+1,(Bit8u)val);
}

void mem_unalignedwrited(PhysPt address,Bit32u val) {
	mem_writeb_inline(address,(Bit8u)val);val>>=8;
	mem_writeb_inline(address+1,(Bit8u)val);val>>=8;
	mem_writeb_inline(address+2,(Bit8u)val);val>>=8;
	mem_writeb_inline(address+3,(Bit8u)val);
}


bool mem_unalignedreadw_checked(PhysPt address, Bit16u * val) {
	Bit8u rval1,rval2;
	if (mem_readb_checked(address+0, &rval1)) return true;
	if (mem_readb_checked(address+1, &rval2)) return true;
	*val=(Bit16u)(((Bit8u)rval1) | (((Bit8u)rval2) << 8));
	return false;
}

bool mem_unalignedreadd_checked(PhysPt address, Bit32u * val) {
	Bit8u rval1,rval2,rval3,rval4;
	if (mem_readb_checked(address+0, &rval1)) return true;
	if (mem_readb_checked(address+1, &rval2)) return true;
	if (mem_readb_checked(address+2, &rval3)) return true;
	if (mem_readb_checked(address+3, &rval4)) return true;
	*val=(Bit32u)(((Bit8u)rval1) | (((Bit8u)rval2) << 8) | (((Bit8u)rval3) << 16) | (((Bit8u)rval4) << 24));
	return false;
}

bool mem_unalignedwritew_checked(PhysPt address,Bit16u val) {
	if (mem_writeb_checked(address,(Bit8u)(val & 0xff))) return true;val>>=8;
	if (mem_writeb_checked(address+1,(Bit8u)(val & 0xff))) return true;
	return false;
}

bool mem_unalignedwrited_checked(PhysPt address,Bit32u val) {
	if (mem_writeb_checked(address,(Bit8u)(val & 0xff))) return true;val>>=8;
	if (mem_writeb_checked(address+1,(Bit8u)(val & 0xff))) return true;val>>=8;
	if (mem_writeb_checked(address+2,(Bit8u)(val & 0xff))) return true;val>>=8;
	if (mem_writeb_checked(address+3,(Bit8u)(val & 0xff))) return true;
	return false;
}

Bit8u mem_readb(PhysPt address) {
	return mem_readb_inline(address);
}

Bit16u mem_readw(PhysPt address) {
	return mem_readw_inline(address);
}

Bit32u mem_readd(PhysPt address) {
	return mem_readd_inline(address);
}

void mem_writeb(PhysPt address,Bit8u val) {
	mem_writeb_inline(address,val);
}

void mem_writew(PhysPt address,Bit16u val) {
	mem_writew_inline(address,val);
}

void mem_writed(PhysPt address,Bit32u val) {
	mem_writed_inline(address,val);
}

void phys_writes(PhysPt addr, const char* string, Bitu length) {
	for(Bitu i = 0; i < length; i++) host_writeb(MemBase+addr+i,string[i]);
}

static void write_p92(Bitu port,Bitu val,Bitu iolen) {	
	// Bit 0 = system reset (switch back to real mode)
	if (val&1) E_Exit("XMS: CPU reset via port 0x92 not supported.");
	memory.a20.controlport = val & ~2;
	MEM_A20_Enable((val & 2)>0);
}

static Bitu read_p92(Bitu port,Bitu iolen) {
	return memory.a20.controlport | (memory.a20.enabled ? 0x02 : 0);
}

void RemoveEMSPageFrame(void) {
	/* Setup rom at 0xe0000-0xf0000 */
	for (Bitu ct=0xe0;ct<0xf0;ct++) {
		memory.phandlers[ct] = &rom_page_handler;
	}
}

void PreparePCJRCartRom(void) {
	/* Setup rom at 0xd0000-0xe0000 */
	for (Bitu ct=0xd0;ct<0xe0;ct++) {
		memory.phandlers[ct] = &rom_page_handler;
	}
}

HostPt GetMemBase(void) { return MemBase; }

class MEMORY:public Module_base{
private:
	IO_ReadHandleObject ReadHandler;
	IO_WriteHandleObject WriteHandler;
public:	
	MEMORY(Section* configuration):Module_base(configuration){
		Bitu i;
		Section_prop * section=static_cast<Section_prop *>(configuration);
	
		/* Setup the Physical Page Links */
		Bitu memsize=section->Get_int("memsize");	
		Bitu memsizekb=section->Get_int("memsizekb");
		Bitu address_bits=section->Get_int("memalias");

		if (address_bits == 0)
			address_bits = 32;
		else if (address_bits < 20)
			address_bits = 20;
		else if (address_bits > 32)
			address_bits = 32;

		/* WARNING: Binary arithmetic done with 64-bit integers because under Microsoft C++
		            ((1UL << 32UL) - 1UL) == 0, which is WRONG.
					But I'll never get back the 4 days I wasted chasing it down, trying to
					figure out why DOSBox was getting stuck reopening it's own CON file handle. */
		memory.mem_alias_pagemask = (uint32_t)
			(((((uint64_t)1) << (uint64_t)address_bits) - (uint64_t)1) >> (uint64_t)12);

		/* If the page mask affects below the 1MB boundary then abort. There is a LOT in
		   DOSBox that relies on reading and maintaining DOS structures and wrapping in
		   that way is a good way to cause a crash. Note 0xFF << 12 == 0xFFFFF */
		if ((memory.mem_alias_pagemask & 0xFF) != 0xFF) {
			//fprintf(stderr,"BUG: invalid alias pagemask 0x%08lX\n",
			//	(unsigned long)memory.mem_alias_pagemask);
			abort();
		}

		/* we can't have more memory than the memory aliasing allows */
		if (address_bits < 32 && ((memsize*256)+(memsizekb/4)) > (memory.mem_alias_pagemask+1)) {
			LOG_MSG("%u-bit memory aliasing limits you to %uMB",
				address_bits,(memory.mem_alias_pagemask+1)/256);
			memsize = (memory.mem_alias_pagemask+1)/256;
			memsizekb = 0;
		}

		if (memsizekb > 524288) memsizekb = 524288;
		if (memsizekb == 0 && memsize < 1) memsize = 1;
		else if (memsizekb != 0 && memsize < 0) memsize = 0;
		/* max 63 to solve problems with certain xms handlers */
		if ((memsize+(memsizekb/1024)) > MAX_MEMORY-1) {
			LOG_MSG("Maximum memory size is %d MB",MAX_MEMORY - 1);
			memsize = MAX_MEMORY-1;
			memsizekb = 0;
		}
		if ((memsize+(memsizekb/1024)) > SAFE_MEMORY-1) {
			LOG_MSG("Memory sizes above %d MB are NOT recommended.",SAFE_MEMORY - 1);
			if ((memsize+(memsizekb/1024)) > 200) LOG_MSG("Memory sizes above 200 MB are too big for saving/loading states.");
			LOG_MSG("Stick with the default values unless you are absolutely certain.");
		}
		memory.reported_pages = memory.pages =
			((memsize*1024*1024) + (memsizekb*1024))/4096;

		/* if the config file asks for less than 1MB of memory
		 * then say so to the DOS program. but way too much code
		 * here assumes memsize >= 1MB */
		if (memory.pages < ((1024*1024)/4096))
			memory.pages = ((1024*1024)/4096);

		MemBase = new Bit8u[memory.pages*4096];
		memorySize = sizeof(Bit8u) * memsize*1024*1024;
		if (!MemBase) E_Exit("Can't allocate main memory of %d MB",memsize);
		/* Clear the memory, as new doesn't always give zeroed memory
		 * (Visual C debug mode). We want zeroed memory though. */
		memset((void*)MemBase,0,memory.reported_pages*4096);
		/* the rest of "ROM" is for unmapped devices so we need to fill it appropriately */
		if (memory.reported_pages < memory.pages)
			memset((char*)MemBase+(memory.reported_pages*4096),0xFF,
				(memory.pages - memory.reported_pages)*4096);
		/* adapter ROM */
		memset((char*)MemBase+0xA0000,0xFF,0x60000);
		/* except for 0xF0000-0xFFFFF */
		memset((char*)MemBase+0xF0000,0x00,0x10000);
		/* and 0xC0000-0xC7FFF for VGA BIOS */
		memset((char*)MemBase+0xC0000,0x00,0x8000);
		/* and 0xC8000-0xCFFFF for DOSBox private data area */
		memset((char*)MemBase+0xC8000,0x00,0x8000);
		/* FIXME: Wait----why is DOSBox-X loading DOS programs into 0xD000:0x0000?? */
		/* Allocate the data for the different page information blocks */

		PageHandler *ram_ptr =
			memory.mem_alias_pagemask == (Bit32u)(~0UL)
			? (PageHandler*)(&ram_page_handler) /* no aliasing */
			: (PageHandler*)(&ram_alias_page_handler); /* aliasing */

		memory.phandlers=new  PageHandler * [memory.pages];
		memory.mhandles=new MemHandle [memory.pages];
		for (i = 0;i < memory.reported_pages;i++) {
			memory.phandlers[i] = ram_ptr;
			memory.mhandles[i] = 0;				//Set to 0 for memory allocation
		}
		for (;i < memory.pages;i++) {
			memory.phandlers[i] = &illegal_page_handler;
			memory.mhandles[i] = 0;				//Set to 0 for memory allocation
		}
		/* apparently vital areas for DOS */
		for (i=0xc8;i<0xd0;i++) {
			memory.phandlers[i] = ram_ptr;
			memory.mhandles[i] = 0;				//Set to 0 for memory allocation
		}
#if 0
		/* FIXME: Same with the 0x9fff segment */
		for (i=0x9f;i<0xa0;i++) {
			memory.phandlers[i] = ram_ptr;
			memory.mhandles[i] = 0;				//Set to 0 for memory allocation
		}
#endif
		/* Setup rom at 0xc0000-0xc8000 */
		for (i=0xc0;i<0xc8;i++) {
			memory.phandlers[i] = &rom_page_handler;
		}
		/* Setup rom at 0xf0000-0x100000 */
		for (i=0xf0;i<0x100;i++) {
			memory.phandlers[i] = &rom_page_handler;
		}
		if (machine==MCH_PCJR) {
			/* Setup cartridge rom at 0xe0000-0xf0000 */
			for (i=0xe0;i<0xf0;i++) {
				memory.phandlers[i] = &rom_page_handler;
			}
		}
		/* Reset some links */
		memory.links.used = 0;
		// A20 Line - PS/2 system control port A
		WriteHandler.Install(0x92,write_p92,IO_MB);
		ReadHandler.Install(0x92,read_p92,IO_MB);
		MEM_A20_Enable(false);
	}
	~MEMORY(){
		delete [] MemBase;
		delete [] memory.phandlers;
		delete [] memory.mhandles;
	}
};	

	
static MEMORY* test;	
	
static void MEM_ShutDown(Section * sec) {
	delete test;
}

void MEM_Init(Section * sec) {
	/* shutdown function */
	test = new MEMORY(sec);
	sec->AddDestroyFunction(&MEM_ShutDown);
}



//save state support
extern void* VGA_PageHandler_Func[16];

Bit32u Memory_PageHandler_table[] = 
{
	(Bit32u) NULL,
	(Bit32u) &ram_page_handler,
	(Bit32u) &rom_page_handler,

	(Bit32u) VGA_PageHandler_Func[0],
	(Bit32u) VGA_PageHandler_Func[1],
	(Bit32u) VGA_PageHandler_Func[2],
	(Bit32u) VGA_PageHandler_Func[3],
	(Bit32u) VGA_PageHandler_Func[4],
	(Bit32u) VGA_PageHandler_Func[5],
	(Bit32u) VGA_PageHandler_Func[6],
	(Bit32u) VGA_PageHandler_Func[7],
	(Bit32u) VGA_PageHandler_Func[8],
	(Bit32u) VGA_PageHandler_Func[9],
	(Bit32u) VGA_PageHandler_Func[10],
	(Bit32u) VGA_PageHandler_Func[11],
	(Bit32u) VGA_PageHandler_Func[12],
	(Bit32u) VGA_PageHandler_Func[13],
	(Bit32u) VGA_PageHandler_Func[14],
	(Bit32u) VGA_PageHandler_Func[15],
};

