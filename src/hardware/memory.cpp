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


#include <stdint.h>
#include <assert.h>
#include "dosbox.h"
#include "dos_inc.h"
#include "mem.h"
#include "inout.h"
#include "setup.h"
#include "paging.h"
#include "programs.h"
#include "regs.h"
#ifndef WIN32
# include <stdlib.h>
# include <unistd.h>
# include <stdio.h>
#endif

#include "voodoo.h"

#include <string.h>

bool a20_guest_changeable = true;
bool a20_full_masking = false;
bool a20_fake_changeable = false;

bool enable_port92 = true;
bool has_Init_RAM = false;
bool has_Init_MemHandles = false;
bool has_Init_MemoryAccessArray = false;

extern Bitu rombios_minimum_location;
extern bool VIDEO_BIOS_always_carry_14_high_font;
extern bool VIDEO_BIOS_always_carry_16_high_font;

#define PAGES_IN_BLOCK	((1024*1024)/MEM_PAGE_SIZE)
#define SAFE_MEMORY	32
#define MAX_MEMORY	512
#define MAX_PAGE_ENTRIES (MAX_MEMORY*1024*1024/4096)
#define LFB_PAGES	512
#define MAX_LINKS	((MAX_MEMORY*1024/4)+4096)		//Hopefully enough

/* if set: mainline DOSBox behavior where adapter ROM (0xA0000-0xFFFFF) except for
 * areas explicitly mapped to the ROM handler, are mapped the same as system RAM.
 *
 * if clear: associate any adapter ROM region not used by the BIOS, VGA BIOS, or
 * VGA, with the Illegal handler (not mapped). Actual RAM behind the storage does
 * not show up and reads return 0xFF, just like real hardware. */
bool adapter_rom_is_ram = false;

struct LinkBlock {
	Bitu used;
	Bit32u pages[MAX_LINKS];
};

static struct MemoryBlock {
	MemoryBlock() : pages(0), handler_pages(0), reported_pages(0), phandlers(NULL), mhandles(NULL), mem_alias_pagemask(0), address_bits(0) { }

	Bitu pages;
	Bitu handler_pages;
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
	Bit32u address_bits;
} memory;

HostPt MemBase = NULL;

class UnmappedPageHandler : public PageHandler {
public:
	UnmappedPageHandler() : PageHandler(PFLAG_INIT|PFLAG_NOCODE) {}
	Bitu readb(PhysPt addr) {
		return 0xFF; /* Real hardware returns 0xFF not 0x00 */
	} 
	void writeb(PhysPt addr,Bitu val) {
	}
};

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
		LOG(LOG_CPU,LOG_ERROR)("Write %x to rom at %x",(int)val,(int)addr);
	}
	void writew(PhysPt addr,Bitu val){
		LOG(LOG_CPU,LOG_ERROR)("Write %x to rom at %x",(int)val,(int)addr);
	}
	void writed(PhysPt addr,Bitu val){
		LOG(LOG_CPU,LOG_ERROR)("Write %x to rom at %x",(int)val,(int)addr);
	}
};



static UnmappedPageHandler unmapped_page_handler;
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

void MEM_ResetPageHandler_RAM(Bitu phys_page, Bitu pages) {
	PageHandler *ram_ptr =
		(memory.mem_alias_pagemask == (Bit32u)(~0UL) && !a20_full_masking)
		? (PageHandler*)(&ram_page_handler) /* no aliasing */
		: (PageHandler*)(&ram_alias_page_handler); /* aliasing */
	for (;pages>0;pages--) {
		memory.phandlers[phys_page]=ram_ptr;
		phys_page++;
	}
}

void MEM_ResetPageHandler_Unmapped(Bitu phys_page, Bitu pages) {
	for (;pages>0;pages--) {
		memory.phandlers[phys_page]=&unmapped_page_handler;
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
	if (a20_guest_changeable || a20_fake_changeable)
		memory.a20.enabled = enabled;

	LOG(LOG_MISC,LOG_DEBUG)("MEM_A20_Enable(%u)",enabled?1:0);

	if (!a20_full_masking) {
		Bitu phys_base=memory.a20.enabled ? (1024/4) : 0;
		for (Bitu i=0;i<16;i++) PAGING_MapPage((1024/4)+i,phys_base+i);
	}
	else if (!a20_fake_changeable) {
		if (memory.a20.enabled) memory.mem_alias_pagemask |= 0x100;
		else memory.mem_alias_pagemask &= ~0x100;
		PAGING_ClearTLB();
	}
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

#include "cpu.h"

extern bool warn_on_mem_write;
extern CPUBlock cpu;

void mem_writeb(PhysPt address,Bit8u val) {
//	if (warn_on_mem_write && cpu.pmode) LOG_MSG("WARNING: post-killswitch memory write to 0x%08x = 0x%02x\n",address,val);
	mem_writeb_inline(address,val);
}

void mem_writew(PhysPt address,Bit16u val) {
//	if (warn_on_mem_write && cpu.pmode) LOG_MSG("WARNING: post-killswitch memory write to 0x%08x = 0x%04x\n",address,val);
	mem_writew_inline(address,val);
}

void mem_writed(PhysPt address,Bit32u val) {
//	if (warn_on_mem_write && cpu.pmode) LOG_MSG("WARNING: post-killswitch memory write to 0x%08x = 0x%08x\n",address,val);
	mem_writed_inline(address,val);
}

void phys_writes(PhysPt addr, const char* string, Bitu length) {
	for(Bitu i = 0; i < length; i++) host_writeb(MemBase+addr+i,string[i]);
}

#include "control.h"

void restart_program(std::vector<std::string> & parameters);
unsigned char CMOS_GetShutdownByte();
void CPU_Snap_Back_To_Real_Mode();
void DEBUG_Enable(bool pressed);
void CPU_Snap_Back_Forget();
Bitu CPU_Pop16(void);

static bool cmos_reset_type_9_sarcastic_win31_comments=true;

void On_Software_286_int15_block_move_return(unsigned char code) {
	Bit16u vec_seg,vec_off;

	/* make CPU core stop immediately */
	CPU_Cycles = 0;

	/* force CPU back to real mode */
	CPU_Snap_Back_To_Real_Mode();
	CPU_Snap_Back_Forget();

	/* read the reset vector from the BIOS data area. this time, it's a stack pointer */
	vec_off = phys_readw(0x400 + 0x67);
	vec_seg = phys_readw(0x400 + 0x69);

	if (cmos_reset_type_9_sarcastic_win31_comments) {
		cmos_reset_type_9_sarcastic_win31_comments=false;
		LOG_MSG("CMOS Shutdown byte 0x%02x says to do INT 15 block move reset %04x:%04x. Only weirdos like Windows 3.1 use this... NOT WELL TESTED!",code,vec_seg,vec_off);
	}

#if C_DYNAMIC_X86
	/* FIXME: The way I will be carrying this out is incompatible with the Dynamic core! */
	if (cpudecoder == &CPU_Core_Dyn_X86_Run) E_Exit("Sorry, CMOS shutdown CPU reset method is not compatible with dynamic core");
#endif

	/* set stack pointer. prepare to emulate BIOS returning from INT 15h block move, 286 style */
	CPU_SetSegGeneral(cs,0xF000);
	CPU_SetSegGeneral(ss,vec_seg);
	reg_esp = vec_off;

	/* WARNING! WARNING! This is based on what Windows 3.1 standard mode (cputype=286) expects.
	 * We need more comprehensive documentation on what actual 286 BIOSes do.
	 * This code, order-wise, is a guess! No documentation exists on what actually happens!
	 * But so far, this allows Windows 3.1 to run in full Standard Mode when cputype=286 without crashing.
	 * Be warned that while it works DOSBox-X will spit a shitload of messages about the triple-fault reset trick it's using. */
	CPU_SetSegGeneral(es,CPU_Pop16());	/* FIXME: ES? or DS? */
	CPU_SetSegGeneral(ds,CPU_Pop16());	/* FIXME: ES? or DS? */
	/* probably the stack frame of POPA */
	reg_di=CPU_Pop16();reg_si=CPU_Pop16();reg_bp=CPU_Pop16();CPU_Pop16();//Don't save SP
	reg_bx=CPU_Pop16();reg_dx=CPU_Pop16();reg_cx=CPU_Pop16();reg_ax=CPU_Pop16();
	/* and then finally what looks like an IRET frame */
	CPU_IRET(false,0);

	/* force execution change (FIXME: Is there a better way to do this?) */
	throw int(4);
	/* does not return */
}

void On_Software_286_reset_vector(unsigned char code) {
	Bit16u vec_seg,vec_off;

	/* make CPU core stop immediately */
	CPU_Cycles = 0;

	/* force CPU back to real mode */
	CPU_Snap_Back_To_Real_Mode();
	CPU_Snap_Back_Forget();

	/* read the reset vector from the BIOS data area */
	vec_off = phys_readw(0x400 + 0x67);
	vec_seg = phys_readw(0x400 + 0x69);

	/* TODO: If cputype=386 or cputype=486, and A20 gate disabled, treat it as an intentional trick
	 * to trigger a reset + invalid opcode exception through which the program can then read the
	 * CPU stepping ID register */

	LOG_MSG("CMOS Shutdown byte 0x%02x says to jump to reset vector %04x:%04x",code,vec_seg,vec_off);

#if C_DYNAMIC_X86
	/* FIXME: The way I will be carrying this out is incompatible with the Dynamic core! */
	if (cpudecoder == &CPU_Core_Dyn_X86_Run) E_Exit("Sorry, CMOS shutdown CPU reset method is not compatible with dynamic core");
#endif

	/* following CPU reset, and coming from the BIOS, CPU registers are trashed */
	reg_eax = 0x2010000;
	reg_ebx = 0x2111;
	reg_ecx = 0;
	reg_edx = 0xABCD;
	reg_esi = 0;
	reg_edi = 0;
	reg_ebp = 0;
	reg_esp = 0x4F8;
	CPU_SetSegGeneral(ds,0x0040);
	CPU_SetSegGeneral(es,0x0000);
	CPU_SetSegGeneral(ss,0x0000);
	/* redirect CPU instruction pointer */
	CPU_SetSegGeneral(cs,vec_seg);
	reg_eip = vec_off;

	/* force execution change (FIXME: Is there a better way to do this?) */
	throw int(4);
	/* does not return */
}

void CPU_Exception_Level_Reset();

void On_Software_CPU_Reset() {
	unsigned char c;

	CPU_Exception_Level_Reset();

	/* software-initiated CPU reset. but the intent may not be to reset the system but merely
	 * the CPU. check the CMOS shutdown byte */
	switch (c=CMOS_GetShutdownByte()) {
		case 0x05:	/* JMP double word pointer with EOI */
		case 0x0A:	/* JMP double word pointer without EOI */
			On_Software_286_reset_vector(c);
			return;
		case 0x09:	/* INT 15h block move return to real mode (to appease Windows 3.1 KRNL286.EXE and cputype=286, yuck) */
			On_Software_286_int15_block_move_return(c);
			return;
	};

#if C_DYNAMIC_X86
	/* this technique is NOT reliable when running the dynamic core! */
	if (cpudecoder == &CPU_Core_Dyn_X86_Run) {
		LOG_MSG("Using traditional DOSBox re-exec, C++ exception method is not compatible with dynamic core\n");
		control->startup_params.insert(control->startup_params.begin(),control->cmdline->GetFileName());
		restart_program(control->startup_params);
		return;
	}
#endif

	throw int(3);
	/* does not return */
}

bool allow_port_92_reset = true;

static void write_p92(Bitu port,Bitu val,Bitu iolen) {
	memory.a20.controlport = val & ~2;
	MEM_A20_Enable((val & 2)>0);

	// Bit 0 = system reset (switch back to real mode)
	if (val & 1) {
		if (allow_port_92_reset) {
			LOG_MSG("Restart by port 92h requested\n");
			On_Software_CPU_Reset();
		}
		else {
			LOG_MSG("WARNING: port 92h written with bit 0 set. Is the guest OS or application attempting to reset the system?\n");
		}
	}
}

static Bitu read_p92(Bitu port,Bitu iolen) {
	return memory.a20.controlport | (memory.a20.enabled ? 0x02 : 0);
}

void RemoveEMSPageFrame(void) {
	LOG(LOG_MISC,LOG_DEBUG)("Removing EMS page frame");

	/* Setup rom at 0xe0000-0xf0000 */
	for (Bitu ct=0xe0;ct<0xf0;ct++) {
		memory.phandlers[ct] = &rom_page_handler;
	}
}

void PreparePCJRCartRom(void) {
	LOG(LOG_MISC,LOG_DEBUG)("Preparing mapping for PCjr cartridge ROM");

	/* Setup rom at 0xd0000-0xe0000 */
	for (Bitu ct=0xd0;ct<0xe0;ct++) {
		memory.phandlers[ct] = &rom_page_handler;
	}
}

/* how to use: unmap_physmem(0xA0000,0xBFFFF) to unmap 0xA0000 to 0xBFFFF */
bool MEM_unmap_physmem(Bitu start,Bitu end) {
	Bitu p;

	if (start & 0xFFF)
		LOG_MSG("WARNING: unmap_physmem() start not page aligned.\n");
	if ((end & 0xFFF) != 0xFFF)
		LOG_MSG("WARNING: unmap_physmem() end not page aligned.\n");
	start >>= 12; end >>= 12;

	for (p=start;p <= end;p++)
		memory.phandlers[p] = &unmapped_page_handler;

	PAGING_ClearTLB();
	return true;
}

bool MEM_map_RAM_physmem(Bitu start,Bitu end) {
	Bitu p;
	PageHandler *ram_ptr =
		(memory.mem_alias_pagemask == (Bit32u)(~0UL) && !a20_full_masking)
		? (PageHandler*)(&ram_page_handler) /* no aliasing */
		: (PageHandler*)(&ram_alias_page_handler); /* aliasing */

	if (start & 0xFFF)
		LOG_MSG("WARNING: unmap_physmem() start not page aligned.\n");
	if ((end & 0xFFF) != 0xFFF)
		LOG_MSG("WARNING: unmap_physmem() end not page aligned.\n");
	start >>= 12; end >>= 12;

	for (p=start;p <= end;p++) {
		if (memory.phandlers[p] != &illegal_page_handler && memory.phandlers[p] != &unmapped_page_handler)
			return false;
	}

	for (p=start;p <= end;p++)
		memory.phandlers[p] = ram_ptr;

	PAGING_ClearTLB();
	return true;
}

bool MEM_map_ROM_physmem(Bitu start,Bitu end) {
	Bitu p;

	if (start & 0xFFF)
		LOG_MSG("WARNING: unmap_physmem() start not page aligned.\n");
	if ((end & 0xFFF) != 0xFFF)
		LOG_MSG("WARNING: unmap_physmem() end not page aligned.\n");
	start >>= 12; end >>= 12;

	for (p=start;p <= end;p++) {
		if (memory.phandlers[p] != &illegal_page_handler && memory.phandlers[p] != &unmapped_page_handler)
			return false;
	}

	for (p=start;p <= end;p++)
		memory.phandlers[p] = &rom_page_handler;

	PAGING_ClearTLB();
	return true;
}

HostPt GetMemBase(void) { return MemBase; }

extern bool mainline_compatible_mapping;

static void RAM_remap_64KBat1MB_A20fast(bool enable/*if set, we're transitioning to fast remap, else to full mask*/) {
	PageHandler *oldp,*newp;
	Bitu c=0;

	/* undo the fast remap at 1MB */
	for (Bitu i=0;i<16;i++) PAGING_MapPage((1024/4)+i,(1024/4)+i);

	/* run through the page array, change ram_handler to ram_alias_handler
	 * (or the other way depending on enable) */
	newp =   enable  ? (PageHandler*)(&ram_page_handler) : (PageHandler*)(&ram_alias_page_handler);
	oldp = (!enable) ? (PageHandler*)(&ram_page_handler) : (PageHandler*)(&ram_alias_page_handler);
	for (Bitu i=0;i < memory.reported_pages;i++) {
		if (memory.phandlers[i] == oldp) {
			memory.phandlers[i] = newp;
			c++;
		}
	}

	LOG_MSG("A20gate mode change: %u pages modified (fast enable=%d)\n",(int)c,(int)enable);
}

class A20GATE : public Program {
public:
	void Run(void) {
		if (cmd->FindString("SET",temp_line,false)) {
			char *x = (char*)temp_line.c_str();

			a20_fake_changeable = false;
			a20_guest_changeable = true;
			MEM_A20_Enable(true);

			if (!strncasecmp(x,"off_fake",8)) {
				if (!a20_full_masking) RAM_remap_64KBat1MB_A20fast(false);
				a20_full_masking = true;
				MEM_A20_Enable(false);
				a20_guest_changeable = false;
				a20_fake_changeable = true;
				WriteOut("A20 gate now off_fake mode\n");
			}
			else if (!strncasecmp(x,"off",3)) {
				if (!a20_full_masking) RAM_remap_64KBat1MB_A20fast(false);
				a20_full_masking = true;
				MEM_A20_Enable(false);
				a20_guest_changeable = false;
				a20_fake_changeable = false;
				WriteOut("A20 gate now off mode\n");
			}
			else if (!strncasecmp(x,"on_fake",7)) {
				if (!a20_full_masking) RAM_remap_64KBat1MB_A20fast(false);
				a20_full_masking = true;
				MEM_A20_Enable(true);
				a20_guest_changeable = false;
				a20_fake_changeable = true;
				WriteOut("A20 gate now on_fake mode\n");
			}
			else if (!strncasecmp(x,"on",2)) {
				if (!a20_full_masking) RAM_remap_64KBat1MB_A20fast(false);
				a20_full_masking = true;
				MEM_A20_Enable(true);
				a20_guest_changeable = false;
				a20_fake_changeable = false;
				WriteOut("A20 gate now on mode\n");
			}
			else if (!strncasecmp(x,"mask",4)) {
				if (!a20_full_masking) RAM_remap_64KBat1MB_A20fast(false);
				a20_full_masking = true;
				MEM_A20_Enable(false);
				a20_guest_changeable = true;
				a20_fake_changeable = false;
				memory.a20.enabled = 0;
				WriteOut("A20 gate now mask mode\n");
			}
			else if (!strncasecmp(x,"fast",4)) {
				if (!a20_full_masking) RAM_remap_64KBat1MB_A20fast(true);
				a20_full_masking = false;
				MEM_A20_Enable(false);
				a20_guest_changeable = true;
				a20_fake_changeable = false;
				WriteOut("A20 gate now fast mode\n");
			}
			else {
				WriteOut("Unknown setting\n");
			}
		}
		else if (cmd->FindExist("ON")) {
			MEM_A20_Enable(true);
			WriteOut("Enabling A20 gate\n");
		}
		else if (cmd->FindExist("OFF")) {
			MEM_A20_Enable(false);
			WriteOut("Disabling A20 gate\n");
		}
		else {
			WriteOut("A20GATE SET [off | off_fake | on | on_fake | mask | fast]\n");
			WriteOut("A20GATE [ON | OFF]\n");
		}
	}
};

void A20GATE_ProgramStart(Program * * make) {
	*make=new A20GATE;
}

void Init_AddressLimitAndGateMask() {
	Section_prop * section=static_cast<Section_prop *>(control->GetSection("dosbox"));

	LOG(LOG_MISC,LOG_DEBUG)("Initializing address limit/gate system");

	// TODO: this option should be handled by the CPU init since it concerns emulation
	//       of older 386 and 486 boards with fewer than 32 address lines:
	//
	//         24-bit addressing on the 386SX vs full 32-bit addressing on the 386DX
	//         26-bit addressing on the 486SX vs full 32-bit addressing on the 486DX
	//
	//       Also this code should automatically cap itself at 24 for 286 emulation and
	//       20 for 8086 emulation.
	memory.address_bits=section->Get_int("memalias");

	if (memory.address_bits == 0)
		memory.address_bits = 32;
	else if (memory.address_bits < 20)
		memory.address_bits = 20;
	else if (memory.address_bits > 32)
		memory.address_bits = 32;

	// TODO: This should be ...? CPU init? Motherboard init?
	/* WARNING: Binary arithmetic done with 64-bit integers because under Microsoft C++
	   ((1UL << 32UL) - 1UL) == 0, which is WRONG.
	   But I'll never get back the 4 days I wasted chasing it down, trying to
	   figure out why DOSBox was getting stuck reopening it's own CON file handle. */
	memory.mem_alias_pagemask = (uint32_t)
		(((((uint64_t)1) << (uint64_t)memory.address_bits) - (uint64_t)1) >> (uint64_t)12);

	/* memory aliasing cannot go below 1MB or serious problems may result. */
	if ((memory.mem_alias_pagemask & 0xFF) != 0xFF) E_Exit("alias pagemask < 1MB");

	/* update alias pagemask according to A20 gate */
	if (a20_fake_changeable && a20_full_masking && !memory.a20.enabled)
		memory.mem_alias_pagemask &= ~0x100;

	/* log */
	LOG(LOG_MISC,LOG_DEBUG)("Memory: address_bits=%u alias_pagemask=%lx",(unsigned int)memory.address_bits,(unsigned long)memory.mem_alias_pagemask);
}

void ShutDownRAM(Section * sec) {
	if (MemBase != NULL) {
		delete [] MemBase;
		MemBase = NULL;
	}
}

void Init_RAM() {
	Section_prop * section=static_cast<Section_prop *>(control->GetSection("dosbox"));
	Bitu i;

	/* please let me know about shutdown! */
	if (!has_Init_RAM) {
		AddExitFunction(AddExitFunctionFuncPair(ShutDownRAM));
		has_Init_RAM = true;
	}

	// LOG
	LOG(LOG_MISC,LOG_DEBUG)("Initializing RAM emulation (system memory)");

	// CHECK: address mask init must have been called!
	assert(memory.mem_alias_pagemask > 0xFF);

	/* Setup the Physical Page Links */
	Bitu memsize=section->Get_int("memsize");	
	Bitu memsizekb=section->Get_int("memsizekb");

	/* we can't have more memory than the memory aliasing allows */
	if ((memory.mem_alias_pagemask+1) != 0/*32-bit integer overflow avoidance*/ &&
		((memsize*256)+(memsizekb/4)) > (memory.mem_alias_pagemask+1)) {
		LOG_MSG("%u-bit memory aliasing limits you to %uMB",
				(int)memory.address_bits,(int)((memory.mem_alias_pagemask+1)/256));
		memsize = (memory.mem_alias_pagemask+1)/256;
		memsizekb = 0;
	}

	/* FIXME: hardcoding the max memory size is BAD. Use MAX_MEMORY. */
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

	// FIXME: Hopefully our refactoring will remove the need for this hack
	/* if the config file asks for less than 1MB of memory
	 * then say so to the DOS program. but way too much code
	 * here assumes memsize >= 1MB */
	if (memory.pages < ((1024*1024)/4096))
		memory.pages = ((1024*1024)/4096);

	// DEBUG message (FIXME: later convert to LOG(LOG_MISC,LOG_DEBUG)
	LOG_MSG("Memory: %u pages of RAM, %u reported to OS, %u pages of memory handlers",
			(unsigned int)memory.pages,
			(unsigned int)memory.reported_pages,
			(unsigned int)memory.handler_pages);

	// sanity check!
	assert(memory.handler_pages >= memory.pages);
	assert(memory.reported_pages <= memory.pages);
	assert(memory.handler_pages >= memory.reported_pages);
	assert(memory.handler_pages >= 0x100); /* enough for at minimum 1MB of addressable memory */

	/* Allocate the RAM. We alloc as a large unsigned char array. new[] does not initialize the array,
	 * so we then must zero the buffer. */
	MemBase = new Bit8u[memory.pages*4096];
	if (!MemBase) E_Exit("Can't allocate main memory of %d MB",(int)memsize);
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

	// sanity check. if this condition is false the loops below will overrun the array!
	assert(memory.reported_pages <= memory.handler_pages);

	PageHandler *ram_ptr =
		(memory.mem_alias_pagemask == (Bit32u)(~0UL) && !a20_full_masking)
		? (PageHandler*)(&ram_page_handler) /* no aliasing */
		: (PageHandler*)(&ram_alias_page_handler); /* aliasing */

	for (i=0;i < memory.reported_pages;i++)
		memory.phandlers[i] = ram_ptr;
	for (;i < memory.handler_pages;i++)
		memory.phandlers[i] = &illegal_page_handler;

	if (!adapter_rom_is_ram) {
		/* FIXME: VGA emulation will selectively respond to 0xA0000-0xBFFFF according to the video mode,
		 *        what we want however is for the VGA emulation to assign illegal_page_handler for
		 *        address ranges it is not responding to when mapping changes. */
		for (i=0xa0;i<0x100;i++) /* we want to make sure adapter ROM is unmapped entirely! */
			memory.phandlers[i] = &unmapped_page_handler;
	}
}

static IO_ReadHandleObject PS2_Port_92h_ReadHandler;
static IO_WriteHandleObject PS2_Port_92h_WriteHandler;

void ShutDownMemoryAccessArray(Section * sec) {
	if (memory.phandlers != NULL) {
		delete [] memory.phandlers;
		memory.phandlers = NULL;
	}
}

void ShutDownMemHandles(Section * sec) {
	if (memory.mhandles != NULL) {
		delete [] memory.mhandles;
		memory.mhandles = NULL;
	}
}

void A20Gate_OnReset(Section *sec) {
	Section_prop * section=static_cast<Section_prop *>(control->GetSection("dosbox"));

	memory.a20.enabled = 0;
	a20_fake_changeable = false;

	// TODO: A20 gate control should also be handled by a motherboard init routine
	std::string ss = section->Get_string("a20");
	if (ss == "mask" || ss == "") {
		LOG(LOG_MISC,LOG_DEBUG)("A20: masking emulation");
		a20_guest_changeable = true;
		a20_full_masking = true;
	}
	else if (ss == "on") {
		LOG(LOG_MISC,LOG_DEBUG)("A20: locked on");
		a20_guest_changeable = false;
		a20_full_masking = true;
		memory.a20.enabled = 1;
	}
	else if (ss == "on_fake") {
		LOG(LOG_MISC,LOG_DEBUG)("A20: locked on (but will fake control bit)");
		a20_guest_changeable = false;
		a20_fake_changeable = true;
		a20_full_masking = true;
		memory.a20.enabled = 1;
	}
	else if (ss == "off") {
		LOG(LOG_MISC,LOG_DEBUG)("A20: locked off");
		a20_guest_changeable = false;
		a20_full_masking = true;
		memory.a20.enabled = 0;
	}
	else if (ss == "off_fake") {
		LOG(LOG_MISC,LOG_DEBUG)("A20: locked off (but will fake control bit)");
		a20_guest_changeable = false;
		a20_fake_changeable = true;
		a20_full_masking = true;
		memory.a20.enabled = 0;
	}
	else { /* "" or "fast" */
		LOG(LOG_MISC,LOG_DEBUG)("A20: fast remapping (64KB+1MB) DOSBox style");
		a20_guest_changeable = true;
		a20_full_masking = false;
	}
}

void Init_A20_Gate() {
	LOG(LOG_MISC,LOG_DEBUG)("Initializing A20 gate emulation");

	AddVMEventFunction(VM_EVENT_POWERON,AddVMEventFunctionFuncPair(A20Gate_OnReset));
	AddVMEventFunction(VM_EVENT_RESET,AddVMEventFunctionFuncPair(A20Gate_OnReset));
}

void PS2Port92_OnReset(Section *sec) {
	Section_prop * section=static_cast<Section_prop *>(control->GetSection("dosbox"));

	// TODO: this should be handled in a motherboard init routine
	enable_port92 = section->Get_bool("enable port 92");
	if (enable_port92) {
		// A20 Line - PS/2 system control port A
		// TODO: This should exist in the motherboard emulation code yet to come! The motherboard
		//       determines A20 gating, not the RAM!
		LOG(LOG_MISC,LOG_DEBUG)("Port 92h installed, emulating PS/2 system control port A");
		PS2_Port_92h_WriteHandler.Install(0x92,write_p92,IO_MB);
		PS2_Port_92h_ReadHandler.Install(0x92,read_p92,IO_MB);
	}
}

void Init_PS2_Port_92h() {
	LOG(LOG_MISC,LOG_DEBUG)("Initializing PS/2 port 92h emulation");

	AddVMEventFunction(VM_EVENT_POWERON,AddVMEventFunctionFuncPair(PS2Port92_OnReset));
	AddVMEventFunction(VM_EVENT_RESET,AddVMEventFunctionFuncPair(PS2Port92_OnReset));
}

void Init_MemHandles() {
	Bitu i;

	if (!has_Init_MemHandles) {
		AddExitFunction(AddExitFunctionFuncPair(ShutDownMemHandles));
		has_Init_MemHandles = true;
	}

	// LOG
	LOG(LOG_MISC,LOG_DEBUG)("Initializing memory handle array (EMS/XMS handle management). mem_pages=%lx",(unsigned long)memory.pages);

	if (memory.mhandles == NULL)
		memory.mhandles = new MemHandle[memory.pages];

	for (i = 0;i < memory.pages;i++)
		memory.mhandles[i] = 0;				//Set to 0 for memory allocation

	/* Reset some links */
	memory.links.used = 0;
}

void Init_MemoryAccessArray() {
	Bitu i;

	if (!has_Init_MemoryAccessArray) {
		has_Init_MemoryAccessArray = true;
		AddExitFunction(AddExitFunctionFuncPair(ShutDownMemoryAccessArray));
	}

	// LOG
	LOG(LOG_MISC,LOG_DEBUG)("Initializing memory access array (page handler callback system). mem_alias_pagemask=%lx",(unsigned long)memory.mem_alias_pagemask);

	// CHECK: address mask init must have been called!
	assert(memory.mem_alias_pagemask > 0xFF);

	// we maintain a different page count for page handlers because we want to maintain a
	// "cache" of sorts of what device responds to a given memory address. default to
	// MAX_PAGE_ENTRIES, but bring it down if the memory alising reduces the range of
	// addressable memory from the CPU's point of view.
	memory.handler_pages = MAX_PAGE_ENTRIES;
	if ((memory.mem_alias_pagemask+1) != 0/*integer overflow check*/ &&
		memory.handler_pages > (memory.mem_alias_pagemask+1))
		memory.handler_pages = (memory.mem_alias_pagemask+1);

	if (memory.phandlers == NULL)
		memory.phandlers = new PageHandler* [memory.handler_pages];

	for (i=0;i < memory.handler_pages;i++) // FIXME: This will eventually init all pages to the "slow path" for device lookup
		memory.phandlers[i] = &illegal_page_handler;
}

void Init_PCJR_CartridgeROM() {
	Bitu i;

	// log
	LOG(LOG_MISC,LOG_DEBUG)("Mapping ROM handler for PCjr cartridge emulation");

	/* Setup cartridge rom at 0xe0000-0xf0000.
	 * Don't call this function unless emulating PCjr! */
	for (i=0xe0;i<0xf0;i++)
		memory.phandlers[i] = &rom_page_handler;
}

