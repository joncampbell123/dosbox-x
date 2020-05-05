/*
 *  Copyright (C) 2002-2019  The DOSBox Team
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335, USA.
 */

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "dosbox.h"
#include "callback.h"
#include "mem.h"
#include "paging.h"
#include "bios.h"
#include "keyboard.h"
#include "regs.h"
#include "inout.h"
#include "dos_inc.h"
#include "setup.h"
#include "support.h"
#include "cpu.h"
#include "dma.h"
#include "control.h"

/* TODO: Make EMS page frame address (and size) user configurable.
 *       With auto setting to fit in automatically with BIOS and UMBs.
 *       With limit variable to emulate EMM386.EXE limits (32MB limit in MS-DOS/Win9x).
 *       With command .COM program in the Z:\ directly to allow enabling/disabling EMS
 *           and turning on/off the virtual 8086 mode, just like what EMM386.EXE offers in DOS.
 *       With code to create and maintain vm86 page tables when emulating EMM386.EXE,
 *           while retaining the 'hardware' based mapping it does now if "mixed" or "board".
 *       With configuration parameter to control the number of EMS handles.
 *       With configuration parameter to specify what version EMS to emulate.
 *       With code to change DOS device EMMXXXX0 <-> EMMQXXX0 depending on when you enable/disable EMS at runtime.
 *       With runtime enable/disable of VCPI (if nobody is using VCPI).
 *       With DOSBox option to force EMM OS handle to be allocated from an within an even megabyte if the user
 *           would rather be able to manage the A20 line without crashing, and another option to specify if
 *           A20 should be turned on or left off in this case.
 */

Bitu XMS_EnableA20(bool enable);

unsigned short EMM_PAGEFRAME =      0xE000;
unsigned short EMM_PAGEFRAME4K =    ((EMM_PAGEFRAME*16)/4096);

Bitu GetEMSPageFrameSegment(void) {
    return EMM_PAGEFRAME;
}

#define	EMM_MAX_HANDLES	200U			/* 255 Max */
#define EMM_PAGE_SIZE	(16U*1024U)
#define EMM_MAX_PHYS	4U				/* 4 16kb pages in pageframe */

Bitu GetEMSPageFrameSize(void) {
    return EMM_MAX_PHYS * EMM_PAGE_SIZE;
}

#define EMM_VERSION			0x40
#define EMM_MINOR_VERSION		0x00
//#define EMM_MINOR_VERSION		0x30	// emm386 4.48
#define GEMMIS_VERSION			0x0001	// Version 1.0

#define EMM_SYSTEM_HANDLE		0x0000
#define NULL_HANDLE			0xffff
#define	NULL_PAGE			0xffff

bool ENABLE_VCPI=true;
bool ENABLE_V86_STARTUP=false;
bool zero_int67_if_no_ems=true;
bool ems_syshandle_on_even_mb=false;

/* EMM errors */
#define EMM_NO_ERROR			0x00
#define EMM_SOFT_MAL			0x80
#define EMM_HARD_MAL			0x81
#define EMM_INVALID_HANDLE		0x83
#define EMM_FUNC_NOSUP			0x84
#define EMM_OUT_OF_HANDLES		0x85
#define EMM_SAVEMAP_ERROR		0x86
#define EMM_OUT_OF_PHYS			0x87
#define EMM_OUT_OF_LOG			0x88
#define EMM_ZERO_PAGES			0x89
#define EMM_LOG_OUT_RANGE		0x8a
#define EMM_ILL_PHYS			0x8b
#define EMM_PAGE_MAP_SAVED		0x8d
#define EMM_NO_SAVED_PAGE_MAP	0x8e
#define EMM_INVALID_SUB			0x8f
#define EMM_FEAT_NOSUP			0x91
#define EMM_MOVE_OVLAP			0x92
#define EMM_MOVE_OVLAPI			0x97
#define EMM_NOT_FOUND			0xa0

enum {
	EMS_NONE=0,			/* no emulation */
	EMS_MIXED,			/* "mixed mode", default if "true", attempts to please everybody */
	EMS_BOARD,			/* act like pre-386 expanded memory, provided by an expansion card */
	EMS_EMM386			/* act like 386+ expanded memory, faked using virtual 8086 mode (EMM386.EXE style) */
};

struct EMM_Mapping {
	Bit16u handle;
	Bit16u page;
};

struct EMM_Handle {
	Bit16u pages;
	MemHandle mem;
	char name[8];
	bool saved_page_map;
	EMM_Mapping page_map[EMM_MAX_PHYS];
};

static Bitu ems_type = EMS_NONE;

const char *EMS_Type_String(void) {
    switch (ems_type) {
        case EMS_NONE:  return "None";
        case EMS_MIXED: return "Mixed";
        case EMS_BOARD: return "Board";
        case EMS_EMM386:return "EMM386";
        default:        break;
    }

    return NULL;
}

static EMM_Handle emm_handles[EMM_MAX_HANDLES];
static EMM_Mapping emm_mappings[EMM_MAX_PHYS];
static EMM_Mapping emm_segmentmappings[0x40];

bool EMS_GetMapping(Bitu &handle,Bit16u &log_page,Bitu ems_page) {
    if (ems_page < EMM_MAX_PHYS) {
        auto &x = emm_mappings[ems_page];

        if (x.handle != NULL_HANDLE && x.page != NULL_PAGE) {
            handle = x.handle;
            log_page = x.page;
            return true;
        }
    }

    return false;
}

bool EMS_GetHandle(Bitu &size,PhysPt &addr,std::string &name,Bitu handle) {
    if (handle < EMM_MAX_HANDLES) {
        auto &x = emm_handles[handle];

        if (x.pages != NULL_HANDLE) {
            {
                unsigned int i=0;

                while (i < sizeof(x.name) && x.name[i] != 0) i++;
                name = std::string(x.name,i);
            }
            size = (Bitu)x.pages << 14UL; // 16KB pages
            addr = (PhysPt)x.mem << 12UL;
            return true;
        }
    }

    return false;
}

Bitu EMS_Max_Handles(void) {
    return EMM_MAX_HANDLES;
}

bool EMS_Active(void) {
    return ems_type != EMS_NONE;
}

static Bit16u GEMMIS_seg; 

class device_EMM : public DOS_Device {
public:
    device_EMM(bool is_emm386_avail) : is_emm386(is_emm386_avail) {
		SetName("EMMXXXX0");
		GEMMIS_seg=0;
	}
	bool Read(Bit8u * /*data*/,Bit16u * /*size*/) { return false;}
	bool Write(const Bit8u * /*data*/,Bit16u * /*size*/){ 
		LOG(LOG_IOCTL,LOG_NORMAL)("EMS:Write to device");	
		return false;
	}
	bool Seek(Bit32u * /*pos*/,Bit32u /*type*/){return false;}
	bool Close(){return false;}
	Bit16u GetInformation(void){return 0xc0c0;}
	bool ReadFromControlChannel(PhysPt bufptr,Bit16u size,Bit16u * retcode);
	bool WriteToControlChannel(PhysPt /*bufptr*/,Bit16u /*size*/,Bit16u * /*retcode*/){return true;}
private:
//	Bit8u cache;
	bool is_emm386;
};

bool device_EMM::ReadFromControlChannel(PhysPt bufptr,Bit16u size,Bit16u * retcode) { 
	Bitu subfct=mem_readb(bufptr);
	switch (subfct) {
		case 0x00:
			if (size!=6) return false;
			mem_writew(bufptr+0x00,0x0023);		// ID
			mem_writed(bufptr+0x02,0);			// private API entry point
			*retcode=6;
			return true;
		case 0x01: {
			if (!is_emm386) return false;
			if (size!=6) return false;
			if (GEMMIS_seg==0) GEMMIS_seg=DOS_GetMemory(0x20,"GEMMIS_seg");
			PhysPt GEMMIS_addr=PhysMake(GEMMIS_seg,0);

			mem_writew(GEMMIS_addr+0x00,0x0004);			// flags
			mem_writew(GEMMIS_addr+0x02,0x019d);			// size of this structure
			mem_writew(GEMMIS_addr+0x04,GEMMIS_VERSION);	// version 1.0 (provide ems information only)
			mem_writed(GEMMIS_addr+0x06,0);					// reserved

			/* build non-EMS frames (0-0xe000) */
			for (PhysPt frct=0; frct<(unsigned int)EMM_PAGEFRAME4K/4U; frct++) {
				mem_writeb(GEMMIS_addr+0x0a+frct*6,0x00);	// frame type: NONE
				mem_writeb(GEMMIS_addr+0x0b+frct*6,0xff);	// owner: NONE
				mem_writew(GEMMIS_addr+0x0c+frct*6,0xffff);	// non-EMS frame
				mem_writeb(GEMMIS_addr+0x0e + frct*6,0xff);	// EMS page number (NONE)
				mem_writeb(GEMMIS_addr+0x0f+frct*6,0xaa);	// flags: direct mapping
			}
			/* build EMS page frame (0xe000-0xf000) */
			for (PhysPt frct=0; frct<0x10U/4U; frct++) {
				PhysPt frnr=(frct+EMM_PAGEFRAME4K/4u)*6u;
				mem_writeb(GEMMIS_addr+0x0a+frnr,0x03);		// frame type: EMS frame in 64k page
				mem_writeb(GEMMIS_addr+0x0b+frnr,0xff);		// owner: NONE
				mem_writew(GEMMIS_addr+0x0c+frnr,0x7fff);	// no logical page number
				mem_writeb(GEMMIS_addr+0x0e + frnr,(Bit8u)(frct&0xff));		// physical EMS page number
				mem_writeb(GEMMIS_addr+0x0f+frnr,0x00);		// EMS frame
			}
			/* build non-EMS ROM frames (0xf000-0x10000) */
			for (PhysPt frct=(EMM_PAGEFRAME4K+0x10u)/4u; frct<0xf0u/4u; frct++) {
				mem_writeb(GEMMIS_addr+0x0a+frct*6u,0x00);	// frame type: NONE
				mem_writeb(GEMMIS_addr+0x0b+frct*6u,0xff);	// owner: NONE
				mem_writew(GEMMIS_addr+0x0c+frct*6u,0xffff);	// non-EMS frame
				mem_writeb(GEMMIS_addr+0x0e + frct*6u,0xff);	// EMS page number (NONE)
				mem_writeb(GEMMIS_addr+0x0f+frct*6u,0xaa);	// flags: direct mapping
			}

			mem_writeb(GEMMIS_addr+0x18a,0x74);			// ???
			mem_writeb(GEMMIS_addr+0x18b,0x00);			// no UMB descriptors following
			mem_writeb(GEMMIS_addr+0x18c,0x01);			// 1 EMS handle info recort
			mem_writew(GEMMIS_addr+0x18d,0x0000);		// system handle
			mem_writed(GEMMIS_addr+0x18f,0);			// handle name
			mem_writed(GEMMIS_addr+0x193,0);			// handle name
			if (emm_handles[EMM_SYSTEM_HANDLE].pages != NULL_HANDLE) {
				mem_writew(GEMMIS_addr+0x197,(emm_handles[EMM_SYSTEM_HANDLE].pages+3u)/4u);
				mem_writed(GEMMIS_addr+0x199,(unsigned int)emm_handles[EMM_SYSTEM_HANDLE].mem<<12u);	// physical address
			} else {
				mem_writew(GEMMIS_addr+0x197,0x0001);		// system handle
				mem_writed(GEMMIS_addr+0x199,0x00110000);	// physical address
			}

			/* fill buffer with import structure */
			mem_writed(bufptr+0x00,(unsigned int)GEMMIS_seg<<4u);
			mem_writew(bufptr+0x04,GEMMIS_VERSION);
			*retcode=6;
			return true;
			}
		case 0x02:
			if (!is_emm386) return false;
			if (size!=2) return false;
			mem_writeb(bufptr+0x00,(unsigned int)EMM_VERSION>>4u);		// version 4
			mem_writeb(bufptr+0x01,EMM_MINOR_VERSION);
			*retcode=2;
			return true;
		case 0x03:
			if (!is_emm386) return false;
			if (EMM_MINOR_VERSION < 0x2d) return false;
			if (size!=4) return false;
			mem_writew(bufptr+0x00,(Bit16u)(MEM_TotalPages()*4ul));	// max size (kb)
			mem_writew(bufptr+0x02,0x80);							// min size (kb)
			*retcode=2;
			return true;
	}
	return false;
}

static struct {
	bool enabled;
	Bit16u ems_handle;
	Bitu pm_interface;
	MemHandle private_area;
	Bit8u pic1_remapping,pic2_remapping;
} vcpi ;

struct MoveRegion {
	Bit32u bytes;
	Bit8u src_type;
	Bit16u src_handle;
	Bit16u src_offset;
	Bit16u src_page_seg;
	Bit8u dest_type;
	Bit16u dest_handle;
	Bit16u dest_offset;
	Bit16u dest_page_seg;
};

static Bit16u EMM_GetTotalPages(void) {
	Bitu count=MEM_FreeLargest()/4;
	if (count>0x7fff) count=0x7fff;
	return (Bit16u)count;
}

static Bit16u EMM_GetFreePages(void) {
	Bitu count=MEM_FreeTotal()/4;
	if (count>0x7fff) count=0x7fff;
	return (Bit16u)count;
}

static bool INLINE ValidHandle(Bit16u handle) {
	if (handle>=EMM_MAX_HANDLES) return false;
	if (emm_handles[handle].pages==NULL_HANDLE) return false;
	return true;
}

void EMS_ZeroAllocation(MemHandle mem,unsigned int pages) {
	PhysPt address;

	if (pages == 0) return;
	address = (PhysPt)mem * 4096ul;
	pages *= 4096u;

	if ((address+pages) > 0xC0000000ul) E_Exit("EMS_ZeroAllocation out of range");
	while (pages != 0) {
		mem_writeb(address++,0);
		pages--;
	}
}

extern bool dbg_zero_on_ems_allocmem;

/* NTS: "page" in EMS refers to 16KB regions, not the 4KB memory pages we normally work with */
static Bit8u EMM_AllocateMemory(Bit16u pages,Bit16u & dhandle,bool can_allocate_zpages) {
	/* Check for 0 page allocation */
	if (!pages) {
		if (!can_allocate_zpages) return EMM_ZERO_PAGES;
	}
	/* Check for enough free pages */
	if ((MEM_FreeTotal()/ 4) < pages) { return EMM_OUT_OF_LOG;}
	Bit16u handle = 1;
	/* Check for a free handle */
	while (emm_handles[handle].pages != NULL_HANDLE) {
		if (++handle >= EMM_MAX_HANDLES) {return EMM_OUT_OF_HANDLES;}
	}
	MemHandle mem = 0;
	if (pages) {
		mem = MEM_AllocatePages(pages*4u,false);
		if (!mem) E_Exit("EMS:Memory allocation failure");
		else if (dbg_zero_on_ems_allocmem) EMS_ZeroAllocation(mem,pages*4u);
	}
	emm_handles[handle].pages = pages;
	emm_handles[handle].mem = mem;
	/* Change handle only if there is no error. */
	dhandle = handle;
	return EMM_NO_ERROR;
}

static Bit8u EMM_AllocateSystemHandle(Bit16u pages/*NTS: EMS pages are 16KB, this does not refer to 4KB CPU pages*/) {
	MemHandle mem = 0;

	/* Check for enough free pages */
	if ((MEM_FreeTotal()/ 4) < pages) { return EMM_OUT_OF_LOG;}
	Bit16u handle = EMM_SYSTEM_HANDLE;	// emm system handle (reserved for OS usage)
	/* Release memory if already allocated */
	if (emm_handles[handle].pages != NULL_HANDLE) {
		MEM_ReleasePages(emm_handles[handle].mem);
	}

	/* NTS: DOSBox 0.74 and older versions of DOSBox-X allocated EMS memory with the "sequential" param == false */
	/* We now offer the option, if specified in the configuration, to allocate our system handle on an even megabyte.
	 * Why? Well, if the system handle exists on an even megabyte, then it no longer matters what state the A20
	 * gate is in. If the DOS game relies on EMM386.EXE but fiddles with the A20 gate while running in virtual 8086
	 * mode, setting this option can help avoid crashes, where normally clearing the A20 gate prevents EMM386.EXE
	 * from using it's protected mode structures (GDT, page tables) and the sudden aliasing turns them into junk
	 * and the system crashes (random contents in DOS conventional memory interpreted as protected mode structures
	 * doesn't work very well). */
	mem = 0;
	if (ems_syshandle_on_even_mb) {
		mem = MEM_AllocatePages_A20_friendly(pages*4u,/*sequential=*/true);
		if (!mem) LOG(LOG_MISC,LOG_WARN)("EMS: Despite configuration setting, I was unable to allocate EMS system handle on even megabyte");
	}
	if (!mem) mem = MEM_AllocatePages(pages*4u,/*sequential=*/true);
	if (!mem) E_Exit("EMS:System handle memory allocation failure");
	emm_handles[handle].pages = pages;
	emm_handles[handle].mem = mem;
	LOG(LOG_MISC,LOG_DEBUG)("EMS: OS handle allocated %u 16KB pages 0x%08lx-0x%08lx",
		(unsigned int)pages,
		(unsigned long)mem * 4096UL,
		((unsigned long)mem * 4096UL) + (pages * 16384UL) - 1ul);
	return EMM_NO_ERROR;
}

static Bit8u EMM_ReallocatePages(Bit16u handle,Bit16u & pages) {
	/* Check for valid handle */
	if (!ValidHandle(handle)) return EMM_INVALID_HANDLE;
	if (emm_handles[handle].pages != 0) {
		/* Check for enough pages */
		if (!MEM_ReAllocatePages(emm_handles[handle].mem,pages*4u,false)) return EMM_OUT_OF_LOG;
	} else {
		MemHandle mem = MEM_AllocatePages(pages*4u,false);
		if (!mem) E_Exit("EMS:Memory allocation failure during reallocation");
		emm_handles[handle].mem = mem;
	}
	/* Update size */
	emm_handles[handle].pages=pages;
	return EMM_NO_ERROR;
}

Bitu XMS_EnableA20(bool enable);
Bitu XMS_GetEnabledA20(void);

static Bit8u EMM_MapPage(Bitu phys_page,Bit16u handle,Bit16u log_page) {
//	LOG_MSG("EMS MapPage handle %d phys %d log %d",handle,phys_page,log_page);
	/* Check for too high physical page */
	if (phys_page>=EMM_MAX_PHYS) return EMM_ILL_PHYS;

    /* Make sure the A20 gate is on, to avoid crashes.
     * This code maps pages into the page frame like EMM386.EXE
     * does, from extended memory. */
    /* TODO: We should NOT do this when emulating an EMM board
     *       because those cards have their own memory and do not
     *       use the motherboard's extended memory. */
    if (!XMS_GetEnabledA20()) XMS_EnableA20(true);

	/* unmapping doesn't need valid handle (as handle isn't used) */
	if (log_page==NULL_PAGE) {
		/* Unmapping */
		emm_mappings[phys_page].handle=NULL_HANDLE;
		emm_mappings[phys_page].page=NULL_PAGE;
		for (Bitu i=0;i<4;i++) 
			PAGING_MapPage(EMM_PAGEFRAME4K+phys_page*4u+i,EMM_PAGEFRAME4K+phys_page*4u+i);
		PAGING_ClearTLB();
		return EMM_NO_ERROR;
	}
	/* Check for valid handle */
	if (!ValidHandle(handle)) return EMM_INVALID_HANDLE;
	
	if (log_page<emm_handles[handle].pages) {
		/* Mapping it is */
		emm_mappings[phys_page].handle=handle;
		emm_mappings[phys_page].page=log_page;
		
		MemHandle memh=MEM_NextHandleAt(emm_handles[handle].mem,log_page*4u);
		for (Bitu i=0;i<4;i++) {
			PAGING_MapPage(EMM_PAGEFRAME4K+(unsigned int)phys_page*4u+i,(Bitu)memh);
			memh=MEM_NextHandle(memh);
		}
		PAGING_ClearTLB();
		return EMM_NO_ERROR;
	} else  {
		/* Illegal logical page it is */
		return EMM_LOG_OUT_RANGE;
	}
}

static Bit8u EMM_MapSegment(Bitu segment,Bit16u handle,Bit16u log_page) {
//	LOG_MSG("EMS MapSegment handle %d segment %d log %d",handle,segment,log_page);

	bool valid_segment=false;

	if ((ems_type == EMS_MIXED) || (ems_type == EMS_EMM386)) {
		if (segment<0xf000U+0x1000U) valid_segment=true;
	} else {
		if ((segment>=0xa000U) && (segment<0xb000U)) {
			valid_segment=true;		// allow mapping of graphics memory
		}
		if ((segment>=EMM_PAGEFRAME) && (segment<EMM_PAGEFRAME+0x1000U)) {
			valid_segment=true;		// allow mapping of EMS page frame
		}
/*		if ((segment>=EMM_PAGEFRAME-0x1000) && (segment<EMM_PAGEFRAME)) {
			valid_segment=true;
		} */
	}

	if (valid_segment) {
		Bit32s tphysPage = ((Bit32s)segment-(Bit32s)EMM_PAGEFRAME)/(Bit32s)(0x1000/EMM_MAX_PHYS);

		/* unmapping doesn't need valid handle (as handle isn't used) */
		if (log_page==NULL_PAGE) {
			/* Unmapping */
			if ((tphysPage>=0) && ((Bit32u)tphysPage<EMM_MAX_PHYS)) {
				emm_mappings[tphysPage].handle=NULL_HANDLE;
				emm_mappings[tphysPage].page=NULL_PAGE;
			} else {
				emm_segmentmappings[segment>>10].handle=NULL_HANDLE;
				emm_segmentmappings[segment>>10].page=NULL_PAGE;
			}
			for (Bitu i=0;i<4;i++) 
				PAGING_MapPage(segment*16u/4096u+i,segment*16u/4096u+i);
			PAGING_ClearTLB();
			return EMM_NO_ERROR;
		}
		/* Check for valid handle */
		if (!ValidHandle(handle)) return EMM_INVALID_HANDLE;
		
		if (log_page<emm_handles[handle].pages) {
			/* Mapping it is */
			if ((tphysPage>=0) && ((Bit32u)tphysPage<EMM_MAX_PHYS)) {
				emm_mappings[tphysPage].handle=handle;
				emm_mappings[tphysPage].page=log_page;
			} else {
				emm_segmentmappings[segment>>10u].handle=handle;
				emm_segmentmappings[segment>>10u].page=log_page;
			}
			
			MemHandle memh=MEM_NextHandleAt(emm_handles[handle].mem,log_page*4u);
			for (Bitu i=0;i<4;i++) {
				PAGING_MapPage(segment*16u/4096u+i,(Bitu)memh);
				memh=MEM_NextHandle(memh);
			}
			PAGING_ClearTLB();
			return EMM_NO_ERROR;
		} else  {
			/* Illegal logical page it is */
			return EMM_LOG_OUT_RANGE;
		}
	}

	return EMM_ILL_PHYS;
}

static Bit8u EMM_ReleaseMemory(Bit16u handle) {
	/* Check for valid handle */
	if (!ValidHandle(handle)) return EMM_INVALID_HANDLE;

	// should check for saved_page_map flag here, returning an error if it's true
	// as apps are required to restore the pagemap beforehand; to be checked
//	if (emm_handles[handle].saved_page_map) return EMM_SAVEMAP_ERROR;

	if (emm_handles[handle].pages != 0) {
		MEM_ReleasePages(emm_handles[handle].mem);
	}
	/* Reset handle */
	emm_handles[handle].mem=0;
	if (handle==0) {
		emm_handles[handle].pages=0;	// OS handle is NEVER deallocated
	} else {
		emm_handles[handle].pages=NULL_HANDLE;
	}
	emm_handles[handle].saved_page_map=false;
	memset(&emm_handles[handle].name,0,8);
	return EMM_NO_ERROR;
}

static Bit8u EMM_SavePageMap(Bit16u handle) {
	/* Check for valid handle */
	if (handle>=EMM_MAX_HANDLES || (handle != 0 && emm_handles[handle].pages==NULL_HANDLE)) {
		return EMM_INVALID_HANDLE;
	}
	/* Check for previous save */
	if (emm_handles[handle].saved_page_map) return EMM_PAGE_MAP_SAVED;
	/* Copy the mappings over */
	for (Bitu i=0;i<EMM_MAX_PHYS;i++) {
		emm_handles[handle].page_map[i].page=emm_mappings[i].page;
		emm_handles[handle].page_map[i].handle=emm_mappings[i].handle;
	}
	emm_handles[handle].saved_page_map=true;
	return EMM_NO_ERROR;
}

static Bit8u EMM_RestoreMappingTable(void) {
	/* Move through the mappings table and setup mapping accordingly */
	for (Bitu i=0;i<0x40;i++) {
		/* Skip the pageframe */
		if ((i>=(unsigned int)EMM_PAGEFRAME/0x400U) && (i<((unsigned int)EMM_PAGEFRAME/0x400U)+(unsigned int)EMM_MAX_PHYS)) continue;
		EMM_MapSegment(i<<10,emm_segmentmappings[i].handle,emm_segmentmappings[i].page);
	}
	for (Bitu i=0;i<EMM_MAX_PHYS;i++) {
		EMM_MapPage(i,emm_mappings[i].handle,emm_mappings[i].page);
	}

	return EMM_NO_ERROR;
}
static Bit8u EMM_RestorePageMap(Bit16u handle) {
	/* Check for valid handle */
	if (handle>=EMM_MAX_HANDLES || (handle != 0 && emm_handles[handle].pages==NULL_HANDLE)) {
		return EMM_INVALID_HANDLE;
	}
	/* Check for previous save */
	if (!emm_handles[handle].saved_page_map) return EMM_NO_SAVED_PAGE_MAP;
	/* Restore the mappings */
	emm_handles[handle].saved_page_map=false;
	for (Bitu i=0;i<EMM_MAX_PHYS;i++) {
		emm_mappings[i].page=emm_handles[handle].page_map[i].page;
		emm_mappings[i].handle=emm_handles[handle].page_map[i].handle;
	}
	return EMM_RestoreMappingTable();
}

static Bit8u EMM_GetPagesForAllHandles(PhysPt table,Bit16u & handles) {
	handles=0;
	for (Bit16u i=0;i<EMM_MAX_HANDLES;i++) {
		if (emm_handles[i].pages!=NULL_HANDLE) {
			handles++;
			mem_writew(table,i);
			mem_writew(table+2,emm_handles[i].pages);
			table+=4;
		}
	}
	return EMM_NO_ERROR;
}

static Bit8u EMM_PartialPageMapping(void) {
	PhysPt list,data;Bit16u count;
	switch (reg_al) {
	case 0x00:	/* Save Partial Page Map */
		list = SegPhys(ds)+reg_si;
		data = SegPhys(es)+reg_di;
		count=mem_readw(list);list+=2;
		mem_writew(data,count);data+=2;
		for (;count>0;count--) {
			Bit16u segment=mem_readw(list);list+=2;
			if ((segment>=EMM_PAGEFRAME) && (segment<EMM_PAGEFRAME+0x1000u)) {
				Bit16u page = (unsigned int)(segment-EMM_PAGEFRAME) / (unsigned int)(EMM_PAGE_SIZE>>4u);
				mem_writew(data,segment);data+=2;
				MEM_BlockWrite(data,&emm_mappings[page],sizeof(EMM_Mapping));
				data+=sizeof(EMM_Mapping);
			} else if ((ems_type == EMS_MIXED) || (ems_type == EMS_EMM386) || ((segment>=EMM_PAGEFRAME-0x1000) && (segment<EMM_PAGEFRAME)) || ((segment>=0xa000) && (segment<0xb000))) {
				mem_writew(data,segment);data+=2;
				MEM_BlockWrite(data,&emm_segmentmappings[segment>>10u],sizeof(EMM_Mapping));
				data+=sizeof(EMM_Mapping);
			} else {
				return EMM_ILL_PHYS;
			}
		}
		break;
	case 0x01:	/* Restore Partial Page Map */
		data = SegPhys(ds)+reg_si;
		count= mem_readw(data);data+=2;
		for (;count>0;count--) {
			Bit16u segment=mem_readw(data);data+=2;
			if ((segment>=EMM_PAGEFRAME) && (segment<EMM_PAGEFRAME+0x1000)) {
				Bit16u page = (unsigned int)(segment-EMM_PAGEFRAME) / (unsigned int)(EMM_PAGE_SIZE>>4);
				MEM_BlockRead(data,&emm_mappings[page],sizeof(EMM_Mapping));
			} else if ((ems_type == EMS_MIXED) || (ems_type == EMS_EMM386) || ((segment>=EMM_PAGEFRAME-0x1000) && (segment<EMM_PAGEFRAME)) || ((segment>=0xa000) && (segment<0xb000))) {
				MEM_BlockRead(data,&emm_segmentmappings[segment>>10u],sizeof(EMM_Mapping));
			} else {
				return EMM_ILL_PHYS;
			}
			data+=sizeof(EMM_Mapping);
		}
		return EMM_RestoreMappingTable();
		break;
	case 0x02:	/* Get Partial Page Map Array Size */
		reg_al=(Bit8u)(2u+reg_bx*(2u+sizeof(EMM_Mapping)));
		break;
	default:
		LOG(LOG_MISC,LOG_ERROR)("EMS:Call %2X Subfunction %2X not supported",reg_ah,reg_al);
		return EMM_FUNC_NOSUP;
	}
	return EMM_NO_ERROR;
}

static Bit8u HandleNameSearch(void) {
	char name[9];
	Bit16u handle=0;PhysPt data;
	switch (reg_al) {
	case 0x00:	/* Get all handle names */
		reg_al=0;data=SegPhys(es)+reg_di;
		for (handle=0;handle<EMM_MAX_HANDLES;handle++) {
			if (emm_handles[handle].pages!=NULL_HANDLE) {
				reg_al++;
				mem_writew(data,handle);
				MEM_BlockWrite(data+2,emm_handles[handle].name,8);
				data+=10;
			}
		}
		break;
	case 0x01: /* Search for a handle name */
		MEM_StrCopy(SegPhys(ds)+reg_si,name,8);name[8]=0;
		for (handle=0;handle<EMM_MAX_HANDLES;handle++) {
			if (emm_handles[handle].pages!=NULL_HANDLE) {
				if (!strncmp(name,emm_handles[handle].name,8)) {
					reg_dx=handle;
					return EMM_NO_ERROR;
				}
			}
		}
		return EMM_NOT_FOUND;
		break;
	case 0x02: /* Get Total number of handles */
		reg_bx=EMM_MAX_HANDLES;
		break;
	default:
		LOG(LOG_MISC,LOG_ERROR)("EMS:Call %2X Subfunction %2X not supported",reg_ah,reg_al);
		return EMM_INVALID_SUB;
	}
	return EMM_NO_ERROR;
}

static Bit8u GetSetHandleName(void) {
	Bit16u handle=reg_dx;
	switch (reg_al) {
	case 0x00:	/* Get Handle Name */
		if (handle>=EMM_MAX_HANDLES || emm_handles[handle].pages==NULL_HANDLE) return EMM_INVALID_HANDLE;
		MEM_BlockWrite(SegPhys(es)+reg_di,emm_handles[handle].name,8);
		break;
	case 0x01:	/* Set Handle Name */
		if (handle>=EMM_MAX_HANDLES || emm_handles[handle].pages==NULL_HANDLE) return EMM_INVALID_HANDLE;
		MEM_BlockRead(SegPhys(es)+reg_di,emm_handles[handle].name,8);
		break;
	default:
		LOG(LOG_MISC,LOG_ERROR)("EMS:Call %2X Subfunction %2X not supported",reg_ah,reg_al);
		return EMM_INVALID_SUB;
	}
	return EMM_NO_ERROR;

}


static void LoadMoveRegion(PhysPt data,MoveRegion & region) {
	region.bytes=mem_readd(data+0x0);

	region.src_type=mem_readb(data+0x4);
	region.src_handle=mem_readw(data+0x5);
	region.src_offset=mem_readw(data+0x7);
	region.src_page_seg=mem_readw(data+0x9);

	region.dest_type=mem_readb(data+0xb);
	region.dest_handle=mem_readw(data+0xc);
	region.dest_offset=mem_readw(data+0xe);
	region.dest_page_seg=mem_readw(data+0x10);
}

static Bit8u MemoryRegion(void) {
	MoveRegion region;
	Bit8u buf_src[MEM_PAGE_SIZE];
	Bit8u buf_dest[MEM_PAGE_SIZE];
	if (reg_al>1) {
		LOG(LOG_MISC,LOG_ERROR)("EMS:Call %2X Subfunction %2X not supported",reg_ah,reg_al);
		return EMM_FUNC_NOSUP;
	}
	LoadMoveRegion(SegPhys(ds)+reg_si,region);
	/* Parse the region for information */
	PhysPt src_mem = 0,dest_mem = 0;
	MemHandle src_handle = 0,dest_handle = 0;
	PhysPt src_off = 0,dest_off = 0;
    PhysPt src_remain = 0,dest_remain = 0;
	if (!region.src_type) {
		src_mem=region.src_page_seg*16u+region.src_offset;
	} else {
		if (!ValidHandle(region.src_handle)) return EMM_INVALID_HANDLE;
		if ((emm_handles[region.src_handle].pages*EMM_PAGE_SIZE) < ((region.src_page_seg*EMM_PAGE_SIZE)+region.src_offset+region.bytes)) return EMM_LOG_OUT_RANGE;
		src_handle=emm_handles[region.src_handle].mem;
        PhysPt pages=region.src_page_seg*4u+(region.src_offset/MEM_PAGE_SIZE);
		for (;pages>0;pages--) src_handle=MEM_NextHandle(src_handle);
		src_off=region.src_offset&(MEM_PAGE_SIZE-1);
		src_remain=MEM_PAGE_SIZE-src_off;
	}
	if (!region.dest_type) {
		dest_mem=region.dest_page_seg*16u+region.dest_offset;
	} else {
		if (!ValidHandle(region.dest_handle)) return EMM_INVALID_HANDLE;
		if (emm_handles[region.dest_handle].pages*EMM_PAGE_SIZE < (region.dest_page_seg*EMM_PAGE_SIZE)+region.dest_offset+region.bytes) return EMM_LOG_OUT_RANGE;
		dest_handle=emm_handles[region.dest_handle].mem;
        PhysPt pages=region.dest_page_seg*4u+(region.dest_offset/MEM_PAGE_SIZE);
		for (;pages>0;pages--) dest_handle=MEM_NextHandle(dest_handle);
		dest_off=region.dest_offset&(MEM_PAGE_SIZE-1);
		dest_remain=MEM_PAGE_SIZE-dest_off;
	}
    PhysPt toread;
    bool a20_was_enabled = XMS_GetEnabledA20();

    XMS_EnableA20(true);
	while (region.bytes>0) {
		if (region.bytes>MEM_PAGE_SIZE) toread=MEM_PAGE_SIZE;
		else toread=region.bytes;
		/* Read from the source */
		if (!region.src_type) {
			MEM_BlockRead(src_mem,buf_src,toread);
		} else {
			if (toread<src_remain) {
				MEM_BlockRead(((unsigned long)src_handle*(unsigned long)MEM_PAGE_SIZE)+src_off,buf_src,toread);
			} else {
				MEM_BlockRead(((unsigned long)src_handle*(unsigned long)MEM_PAGE_SIZE)+src_off,buf_src,src_remain);
				MEM_BlockRead(((unsigned long)MEM_NextHandle(src_handle)*(unsigned long)MEM_PAGE_SIZE),&buf_src[src_remain],toread-src_remain);
			}
		}
		/* Check for a move */
		if (reg_al==1) {
			/* Read from the destination */
			if (!region.dest_type) {
				MEM_BlockRead(dest_mem,buf_dest,toread);
			} else {
				if (toread<dest_remain) {
					MEM_BlockRead(((unsigned long)dest_handle*(unsigned long)MEM_PAGE_SIZE)+dest_off,buf_dest,toread);
				} else {
					MEM_BlockRead(((unsigned long)dest_handle*(unsigned long)MEM_PAGE_SIZE)+dest_off,buf_dest,dest_remain);
					MEM_BlockRead(((unsigned long)MEM_NextHandle(dest_handle)*(unsigned long)MEM_PAGE_SIZE),&buf_dest[dest_remain],toread-dest_remain);
				}
			}
			/* Write to the source */
			if (!region.src_type) {
				MEM_BlockWrite(src_mem,buf_dest,toread);
			} else {
				if (toread<src_remain) {
					MEM_BlockWrite(((unsigned long)src_handle*(unsigned long)MEM_PAGE_SIZE)+src_off,buf_dest,toread);
				} else {
					MEM_BlockWrite(((unsigned long)src_handle*(unsigned long)MEM_PAGE_SIZE)+src_off,buf_dest,src_remain);
					MEM_BlockWrite(((unsigned long)MEM_NextHandle(src_handle)*(unsigned long)MEM_PAGE_SIZE),&buf_dest[src_remain],toread-src_remain);
				}
			}
		}
		/* Write to the destination */
		if (!region.dest_type) {
			MEM_BlockWrite(dest_mem,buf_src,toread);
		} else {
			if (toread<dest_remain) {
				MEM_BlockWrite(((unsigned long)dest_handle*(unsigned long)MEM_PAGE_SIZE)+dest_off,buf_src,toread);
			} else {
				MEM_BlockWrite(((unsigned long)dest_handle*(unsigned long)MEM_PAGE_SIZE)+dest_off,buf_src,dest_remain);
				MEM_BlockWrite(((unsigned long)MEM_NextHandle(dest_handle)*(unsigned long)MEM_PAGE_SIZE),&buf_src[dest_remain],toread-dest_remain);
			}
		}
		/* Advance the pointers */
		if (!region.src_type) src_mem+=(PhysPt)toread;
		else src_handle=MEM_NextHandle(src_handle);
		if (!region.dest_type) dest_mem+=(PhysPt)toread;
		else dest_handle=MEM_NextHandle(dest_handle);
		region.bytes-=(Bit32u)toread;
	}

    if (!a20_was_enabled) XMS_EnableA20(false);
    return EMM_NO_ERROR;
}


static Bitu INT67_Handler(void) {
	switch (reg_ah) {
	case 0x40:		/* Get Status */
		reg_ah=EMM_NO_ERROR;	
		break;
	case 0x41:		/* Get PageFrame Segment */
		reg_bx=EMM_PAGEFRAME;
		reg_ah=EMM_NO_ERROR;
		break;
	case 0x42:		/* Get number of pages */
		reg_dx=EMM_GetTotalPages();
		reg_bx=EMM_GetFreePages();
		reg_ah=EMM_NO_ERROR;
		break;
	case 0x43:		/* Get Handle and Allocate Pages */
		reg_ah=EMM_AllocateMemory(reg_bx,reg_dx,false);
		break;
	case 0x44:		/* Map Expanded Memory Page */
		reg_ah=EMM_MapPage(reg_al,reg_dx,reg_bx);
		break;
	case 0x45:		/* Release handle and free pages */
		reg_ah=EMM_ReleaseMemory(reg_dx);
		break;
	case 0x46:		/* Get EMM Version */
		reg_ah=EMM_NO_ERROR;
		reg_al=EMM_VERSION;
		break;
	case 0x47:		/* Save Page Map */
		reg_ah=EMM_SavePageMap(reg_dx);
		break;
	case 0x48:		/* Restore Page Map */
		reg_ah=EMM_RestorePageMap(reg_dx);
		break;
	case 0x4b:		/* Get Handle Count */
		reg_bx=0;
		for (unsigned int i=0;i<EMM_MAX_HANDLES;i++) if (emm_handles[i].pages!=NULL_HANDLE) reg_bx++;
		reg_ah=EMM_NO_ERROR;
		break;
	case 0x4c:		/* Get Pages for one Handle */
		if (!ValidHandle(reg_dx)) {reg_ah=EMM_INVALID_HANDLE;break;}
		reg_bx=emm_handles[reg_dx].pages;
		reg_ah=EMM_NO_ERROR;
		break;
	case 0x4d:		/* Get Pages for all Handles */
		reg_ah=EMM_GetPagesForAllHandles(SegPhys(es)+reg_di,reg_bx);
		break;
	case 0x4e:		/*Save/Restore Page Map */
		switch (reg_al) {
		case 0x00:	/* Save Page Map */
			MEM_BlockWrite(SegPhys(es)+reg_di,emm_mappings,sizeof(emm_mappings));
			reg_ah=EMM_NO_ERROR;
			break;
		case 0x01:	/* Restore Page Map */
			MEM_BlockRead(SegPhys(ds)+reg_si,emm_mappings,sizeof(emm_mappings));
			reg_ah=EMM_RestoreMappingTable();
			break;
		case 0x02:	/* Save and Restore Page Map */
			MEM_BlockWrite(SegPhys(es)+reg_di,emm_mappings,sizeof(emm_mappings));
			MEM_BlockRead(SegPhys(ds)+reg_si,emm_mappings,sizeof(emm_mappings));
			reg_ah=EMM_RestoreMappingTable();
			break;	
		case 0x03:	/* Get Page Map Array Size */
			reg_al=sizeof(emm_mappings);
			reg_ah=EMM_NO_ERROR;
			break;
		default:
			LOG(LOG_MISC,LOG_ERROR)("EMS:Call %2X Subfunction %2X not supported",reg_ah,reg_al);
			reg_ah=EMM_INVALID_SUB;
			break;
		}
		break;
	case 0x4f:	/* Save/Restore Partial Page Map */
		reg_ah=EMM_PartialPageMapping();
		break;
	case 0x50:	/* Map/Unmap multiple handle pages */
		reg_ah = EMM_NO_ERROR;
		switch (reg_al) {
			case 0x00: // use physical page numbers
				{	PhysPt data = SegPhys(ds)+reg_si;
					for (int i=0; i<reg_cx; i++) {
						Bit16u logPage	= mem_readw(data); data+=2;
						Bit16u physPage = mem_readw(data); data+=2;
						reg_ah = EMM_MapPage(physPage,reg_dx,logPage);
						if (reg_ah!=EMM_NO_ERROR) break;
					}
				} break;
			case 0x01: // use segment address 
				{	PhysPt data = SegPhys(ds)+reg_si;
					for (int i=0; i<reg_cx; i++) {
						Bit16u logPage	= mem_readw(data); data+=2;
						reg_ah = EMM_MapSegment(mem_readw(data),reg_dx,logPage); data+=2;
						if (reg_ah!=EMM_NO_ERROR) break;
					}
				}
				break;
			default:
				LOG(LOG_MISC,LOG_ERROR)("EMS:Call %2X Subfunction %2X not supported",reg_ah,reg_al);
				reg_ah=EMM_INVALID_SUB;
				break;
		}
		break;
	case 0x51:	/* Reallocate Pages */
		reg_ah=EMM_ReallocatePages(reg_dx,reg_bx);
		break;
	case 0x53: // Set/Get Handlename
		reg_ah=GetSetHandleName();
		break;
	case 0x54:	/* Handle Functions */
		reg_ah=HandleNameSearch();
		break;
	case 0x57:	/* Memory region */
		reg_ah=MemoryRegion();
		if (reg_ah) LOG(LOG_MISC,LOG_ERROR)("EMS:Function 57 move failed");
		break;
	case 0x58: // Get mappable physical array address array
		if (reg_al==0x00) {
			PhysPt data = SegPhys(es)+reg_di;
			Bit16u step = 0x1000 / EMM_MAX_PHYS;
			for (Bit16u i=0; i<EMM_MAX_PHYS; i++) {
				mem_writew(data,EMM_PAGEFRAME+step*i);	data+=2;
				mem_writew(data,i);						data+=2;
			}
		}
		// Set number of pages
		reg_cx = EMM_MAX_PHYS;
		reg_ah = EMM_NO_ERROR;
		break;
	case 0x5A:              /* Allocate standard/raw Pages */
		if (reg_al<=0x01) {
			reg_ah=EMM_AllocateMemory(reg_bx,reg_dx,true);	// can allocate 0 pages
		} else {
			LOG(LOG_MISC,LOG_ERROR)("EMS:Call 5A subfct %2X not supported",reg_al);
			reg_ah=EMM_INVALID_SUB;
		}
		break;
    case 0x70:              /* NEC PC-98 specific function group? */
        if (reg_al == 1) {
            if (IS_PC98_ARCH) {
                /* NTS: EMM386.EXE for PC-98 will return an error code if BX > 1 */
                /* NTS: VEMM486.EXE sets a flag but doesn't seem to care what BX is */
                /* NTS: Neither one seems to remap segment B0000h as far as I can tell, so implementing this will be tricky and full of guesswork. */
                /* See also [http://www.koizuka.jp/~koizuka/master.lib/MASTER.MAN.txt], section "ems_enablepageframe - NEC EMSページフレームバンクの操作" */
                if (reg_bx <= 1) {
                    LOG(LOG_MISC,LOG_DEBUG)("EMS:Call 70 subfct %2X remapping EMS page frame at B000h to %s not yet implemented. Hope your DOS application does not rely on that.",
                        reg_al,reg_bx == 1 ? "system memory" : "video memory");
                }
                else {
                    LOG(LOG_MISC,LOG_DEBUG)("EMS:Call 70 subfct %2X given invalid value BX=%04x which will likely do nothing (master library EMS_ENABLEPAGEFRAME bug)",
                        reg_al,reg_bx);
                }

                reg_ah=EMM_INVALID_SUB;
            }
            else {
                LOG(LOG_MISC,LOG_DEBUG)("EMS:Call 70 subfct %2X not supported outside PC-98 mode",reg_al);
                reg_ah=EMM_INVALID_SUB;
            }
        }
        else {
            LOG(LOG_MISC,LOG_ERROR)("EMS:Call 70 subfct %2X not supported",reg_al);
            reg_ah=EMM_INVALID_SUB;
        }
        break;
	case 0xDE:		/* VCPI Functions */
		if (!vcpi.enabled) {
			LOG(LOG_MISC,LOG_ERROR)("EMS:VCPI Call %2X not supported",reg_al);
			reg_ah=EMM_FUNC_NOSUP;
		} else {
			switch (reg_al) {
			case 0x00:		/* VCPI Installation Check */
				if (((reg_cx==0) && (reg_di==0x0012)) || (cpu.pmode && (reg_flags & FLAG_VM))) {
					/* JEMM detected or already in v86 mode */
					reg_ah=EMM_NO_ERROR;
					reg_bx=0x100;
				} else {
					reg_ah=EMM_FUNC_NOSUP;
				}
				break;
			case 0x01: {	/* VCPI Get Protected Mode Interface */
				Bit16u ct;
				/* Set up page table buffer */
				for (ct=0; ct<0xff; ct++) {
					real_writeb(SegValue(es),reg_di+ct*4+0x00,0x67);		// access bits
					real_writew(SegValue(es),reg_di+ct*4+0x01,ct*0x10);		// mapping
					real_writeb(SegValue(es),reg_di+ct*4+0x03,0x00);
				}
				for (ct=0xff; ct<0x100; ct++) {
					real_writeb(SegValue(es),reg_di+ct*4+0x00,0x67);		// access bits
					real_writew(SegValue(es),reg_di+ct*4+0x01,(ct-0xff)*0x10+0x1100);	// mapping
					real_writeb(SegValue(es),reg_di+ct*4+0x03,0x00);
				}
				/* adjust paging entries for page frame (if mapped) */
				for (ct=0; ct<4; ct++) { 
					Bit16u handle=emm_mappings[ct].handle;
					if (handle!=0xffff) {
						Bit16u memh=(Bit16u)MEM_NextHandleAt(emm_handles[handle].mem,(unsigned int)emm_mappings[ct].page*4u);
						Bit16u entry_addr=(unsigned int)reg_di+(unsigned int)(EMM_PAGEFRAME>>6u)+(unsigned int)(ct*0x10u);
						real_writew(SegValue(es),entry_addr+0x00u+0x01u,(memh+0u)*0x10u);		// mapping of 1/4 of page
						real_writew(SegValue(es),entry_addr+0x04u+0x01u,(memh+1u)*0x10u);		// mapping of 2/4 of page
						real_writew(SegValue(es),entry_addr+0x08u+0x01u,(memh+2u)*0x10u);		// mapping of 3/4 of page
						real_writew(SegValue(es),entry_addr+0x0cu+0x01u,(memh+3u)*0x10u);		// mapping of 4/4 of page
					}
				}
				reg_di+=0x400;		// advance pointer by 0x100*4
				
				/* Set up three descriptor table entries */
				Bit32u cbseg_low=(CALLBACK_GetBase()&0xffff)<<16;
				Bit32u cbseg_high=(CALLBACK_GetBase()&0x1f0000)>>16;
				/* Descriptor 1 (code segment, callback segment) */
				real_writed(SegValue(ds),reg_si+0x00,0x0000ffff|cbseg_low);
				real_writed(SegValue(ds),reg_si+0x04,0x00009a00|cbseg_high);
				/* Descriptor 2 (data segment, full access) */
				real_writed(SegValue(ds),reg_si+0x08,0x0000ffff);
				real_writed(SegValue(ds),reg_si+0x0c,0x00009200);
				/* Descriptor 3 (full access) */
				real_writed(SegValue(ds),reg_si+0x10,0x0000ffff);
				real_writed(SegValue(ds),reg_si+0x14,0x00009200);

				reg_ebx=(vcpi.pm_interface&0xffff);
				reg_ah=EMM_NO_ERROR;
				break;
				}
			case 0x02:		/* VCPI Maximum Physical Address */
				reg_edx=((MEM_TotalPages()*MEM_PAGESIZE)-1)&0xfffff000;
				reg_ah=EMM_NO_ERROR;
				break;
			case 0x03:		/* VCPI Get Number of Free Pages */
				reg_edx=(Bit32u)MEM_FreeTotal();
				reg_ah=EMM_NO_ERROR;
				break;
			case 0x04: {	/* VCPI Allocate one Page */
				MemHandle mem = MEM_AllocatePages(1,false);
				if (mem) {
					reg_edx=(unsigned int)mem<<12u;
					reg_ah=EMM_NO_ERROR;
				} else {
					reg_ah=EMM_OUT_OF_LOG;
				}
				break;
				}
			case 0x05:		/* VCPI Free Page */
				MEM_ReleasePages((MemHandle)((unsigned int)reg_edx>>12u));
				reg_ah=EMM_NO_ERROR;
				break;
			case 0x06: {	/* VCPI Get Physical Address of Page in 1st MB */
				if (((unsigned int)(reg_cx<<8u)>=(unsigned int)EMM_PAGEFRAME) && ((unsigned int)(reg_cx<<8u)<(unsigned int)EMM_PAGEFRAME+0x1000u)) {
					/* Page is in Pageframe, so check what EMS-page it is
					   and return the physical address */
					Bit8u phys_page;
					Bit16u mem_seg=(unsigned int)reg_cx<<8u;
					if (mem_seg<EMM_PAGEFRAME+0x400) phys_page=0;
					else if (mem_seg<EMM_PAGEFRAME+0x800) phys_page=1;
					else if (mem_seg<EMM_PAGEFRAME+0xc00) phys_page=2;
					else phys_page=3;
					Bit16u handle=emm_mappings[phys_page].handle;
					if (handle==0xffff) {
						reg_ah=EMM_ILL_PHYS;
						break;
					} else {
						MemHandle memh=MEM_NextHandleAt(
							emm_handles[handle].mem,
							emm_mappings[phys_page].page*4u);
						reg_edx=((unsigned int)memh+((unsigned int)reg_cx&3u))<<12u;
					}
				} else {
					/* Page not in Pageframe, so just translate into physical address */
					reg_edx=(unsigned int)reg_cx<<12u;
				}

				reg_ah=EMM_NO_ERROR;
				}
				break;
			case 0x07:		/* VCPI Read CR0 */
				reg_ah=EMM_NO_ERROR;
				reg_ebx=(Bit32u)CPU_GET_CRX(0);
				break;
			case 0x0a:		/* VCPI Get PIC Vector Mappings */
				reg_bx=vcpi.pic1_remapping;		// master PIC
				reg_cx=vcpi.pic2_remapping;		// slave PIC
				reg_ah=EMM_NO_ERROR;
				break;
			case 0x0b:		/* VCPI Set PIC Vector Mappings */
				reg_flags&=(~FLAG_IF);
				vcpi.pic1_remapping=reg_bx&0xff;
				vcpi.pic2_remapping=reg_cx&0xff;
				reg_ah=EMM_NO_ERROR;
				break;
			case 0x0c: {	/* VCPI Switch from V86 to Protected Mode */
				reg_flags&=(~FLAG_IF);
				CPU_SetCPL(0);

				/* Read data from ESI (linear address) */
				Bit32u new_cr3=mem_readd(reg_esi);
				Bit32u new_gdt_addr=mem_readd(reg_esi+4);
				Bit32u new_idt_addr=mem_readd(reg_esi+8);
				Bit16u new_ldt=mem_readw(reg_esi+0x0c);
				Bit16u new_tr=mem_readw(reg_esi+0x0e);
				Bit32u new_eip=mem_readd(reg_esi+0x10);
				Bit16u new_cs=mem_readw(reg_esi+0x14);

				/* Get GDT and IDT entries */
				Bit16u new_gdt_limit=mem_readw(new_gdt_addr);
				Bit32u new_gdt_base=mem_readd(new_gdt_addr+2);
				Bit16u new_idt_limit=mem_readw(new_idt_addr);
				Bit32u new_idt_base=mem_readd(new_idt_addr+2);

				/* Switch to protected mode, paging enabled if necessary */
				Bit32u new_cr0=(Bit32u)(CPU_GET_CRX(0)|1u);
				if (new_cr3!=0) new_cr0|=0x80000000;
				CPU_SET_CRX(0, new_cr0);
				CPU_SET_CRX(3, new_cr3);

				/* Load tables and initialize segment registers */
				CPU_LGDT(new_gdt_limit, new_gdt_base);
				CPU_LIDT(new_idt_limit, new_idt_base);
				if (CPU_LLDT(new_ldt)) LOG_MSG("VCPI:Could not load LDT with %x",new_ldt);
				if (CPU_LTR(new_tr)) LOG_MSG("VCPI:Could not load TR with %x",new_tr);

				CPU_SetSegGeneral(ds,0);
				CPU_SetSegGeneral(es,0);
				CPU_SetSegGeneral(fs,0);
				CPU_SetSegGeneral(gs,0);

//				MEM_A20_Enable(true);

				/* Switch to protected mode */
				reg_flags&=(~(FLAG_VM|FLAG_NT));
				reg_flags|=0x3000;
				CPU_JMP(true, new_cs, new_eip, 0);
				}
				break;
			default:
				LOG(LOG_MISC,LOG_ERROR)("EMS:VCPI Call %x not supported",reg_ax);
				reg_ah=EMM_FUNC_NOSUP;
				break;
			}
		}
		break;
	default:
		LOG(LOG_MISC,LOG_ERROR)("EMS:Call %2X not supported",reg_ah);
		reg_ah=EMM_FUNC_NOSUP;
		break;
	}
	return CBRET_NONE;
}

static Bitu VCPI_PM_Handler() {
//	LOG_MSG("VCPI PMODE handler, function %x",reg_ax);
	switch (reg_ax) {
	case 0xDE03:		/* VCPI Get Number of Free Pages */
		reg_edx=(Bit32u)MEM_FreeTotal();
		reg_ah=EMM_NO_ERROR;
		break;
	case 0xDE04: {		/* VCPI Allocate one Page */
		MemHandle mem = MEM_AllocatePages(1,false);
		if (mem) {
			reg_edx=(unsigned int)((unsigned int)mem<<12);
			reg_ah=EMM_NO_ERROR;
		} else {
			reg_ah=EMM_OUT_OF_LOG;
		}
		break;
		}
	case 0xDE05:		/* VCPI Free Page */
		MEM_ReleasePages((MemHandle)(reg_edx>>12u));
		reg_ah=EMM_NO_ERROR;
		break;
	case 0xDE0C: {		/* VCPI Switch from Protected Mode to V86 */
		reg_flags&=(~FLAG_IF);

		/* Flags need to be filled in, VM=true, IOPL=3 */
		mem_writed(SegPhys(ss) + (reg_esp & cpu.stack.mask)+0x10, 0x23002);

		/* Disable Paging */
		CPU_SET_CRX(0, CPU_GET_CRX(0)&0x7ffffff7);
		CPU_SET_CRX(3, 0);

		PhysPt tbaddr=(PhysPt)vcpi.private_area+0x0000u+(0x10u&0xfff8u)+5u;
		Bit8u tb=mem_readb(tbaddr);
		mem_writeb(tbaddr, tb&0xfd);

		/* Load descriptor table registers */
		CPU_LGDT(0xff, (unsigned int)vcpi.private_area+0x0000u);
		CPU_LIDT(0x7ff, (unsigned int)vcpi.private_area+0x2000u);
		if (CPU_LLDT(0x08)) LOG_MSG("VCPI:Could not load LDT");
		if (CPU_LTR(0x10)) LOG_MSG("VCPI:Could not load TR");

		reg_flags&=(~FLAG_NT);
		reg_esp+=8;		// skip interrupt return information
//		MEM_A20_Enable(false);

		/* Switch to v86-task */
		CPU_IRET(true,0);
		}
		break;
	default:
		LOG(LOG_MISC,LOG_WARN)("Unhandled VCPI-function %x in protected mode",reg_al);
		break;
	}
	return CBRET_NONE;
}

bool vcpi_virtual_a20 = true;

/* if we handle the read, we're expected to write over AL/AX */
bool VCPI_trapio_r(uint16_t port,unsigned int sz) {
    (void)sz;//UNUSED
	switch (port) {
		case 0x92:
			reg_al = vcpi_virtual_a20?0x02:0x00;
			return true;
	}

	return false;
}

bool VCPI_trapio_w(uint16_t port,uint32_t data,unsigned int sz) {
    (void)sz;//UNUSED
	switch (port) {
		case 0x92:
			vcpi_virtual_a20 = (data & 2) ? true : false;
			return true;
	}

	return false;
}

static Bitu V86_Monitor() {
	/* Calculate which interrupt did occur */
	Bitu int_num=((unsigned int)mem_readw(SegPhys(ss)+((unsigned int)reg_esp & (unsigned int)cpu.stack.mask)) - 0x2803u);

	/* See if Exception 0x0d and not Interrupt 0x0d */
	if ((int_num==(0x0d*4)) && ((reg_sp&0xffff)!=0x1fda)) {
		/* Protection violation during V86-execution,
		   needs intervention by monitor (depends on faulting opcode) */

		reg_esp+=6;		// skip ip of CALL and error code of EXCEPTION 0x0d

		/* Get address of faulting instruction */
		Bit16u v86_cs=mem_readw(SegPhys(ss)+((reg_esp+4) & cpu.stack.mask));
		Bit16u v86_ip=mem_readw(SegPhys(ss)+((reg_esp+0) & cpu.stack.mask));
		Bit8u v86_opcode=mem_readb(((unsigned int)v86_cs<<4u)+(unsigned int)v86_ip);
//		LOG_MSG("v86 monitor caught protection violation at %x:%x, opcode=%x",v86_cs,v86_ip,v86_opcode);
		switch (v86_opcode) {
			case 0x0f:		// double byte opcode
				v86_opcode=mem_readb((unsigned int)(v86_cs<<4u)+(unsigned int)v86_ip+1u);
				switch (v86_opcode) {
					case 0x20: {	// mov reg,CRx
						Bitu rm_val=mem_readb((unsigned int)(v86_cs<<4u)+(unsigned int)v86_ip+2u);
						Bitu which=(unsigned int)(rm_val >> 3u) & 7u;
						if ((rm_val<0xc0) || (rm_val>=0xe8))
							E_Exit("Invalid opcode 0x0f 0x20 %x caused a protection fault!",static_cast<unsigned int>(rm_val));
						Bit32u crx=(Bit32u)CPU_GET_CRX(which);
						switch (rm_val&7) {
							case 0:	reg_eax=crx;	break;
							case 1:	reg_ecx=crx;	break;
							case 2:	reg_edx=crx;	break;
							case 3:	reg_ebx=crx;	break;
							case 4:	reg_esp=crx;	break;
							case 5:	reg_ebp=crx;	break;
							case 6:	reg_esi=crx;	break;
							case 7:	reg_edi=crx;	break;
						}
						mem_writew(SegPhys(ss)+((reg_esp+0u) & cpu.stack.mask),(unsigned int)v86_ip+3u);
						}
						break;
					case 0x22: {	// mov CRx,reg
						Bitu rm_val=mem_readb((unsigned int)(v86_cs<<4u)+(unsigned int)v86_ip+2u);
						Bitu which=(rm_val >> 3u) & 7u;
						if ((rm_val<0xc0) || (rm_val>=0xe8))
							E_Exit("Invalid opcode 0x0f 0x22 %x caused a protection fault!",static_cast<unsigned int>(rm_val));
						Bit32u crx=0;
						switch (rm_val&7) {
							case 0:	crx=reg_eax;	break;
							case 1:	crx=reg_ecx;	break;
							case 2:	crx=reg_edx;	break;
							case 3:	crx=reg_ebx;	break;
							case 4:	crx=reg_esp;	break;
							case 5:	crx=reg_ebp;	break;
							case 6:	crx=reg_esi;	break;
							case 7:	crx=reg_edi;	break;
						}
						if (which==0) crx|=1;	// protection bit always on
						CPU_SET_CRX(which,crx);
						mem_writew(SegPhys(ss)+((reg_esp+0) & cpu.stack.mask),v86_ip+3);
						}
						break;
					default:
						E_Exit("Unhandled opcode 0x0f %x caused a protection fault!",v86_opcode);
				}
				break;
			case 0xe4:		// IN AL,Ib
				if (!VCPI_trapio_r(mem_readb((unsigned int)(v86_cs<<4u)+(unsigned int)v86_ip+1u),1))
					reg_al=IO_ReadB(mem_readb((unsigned int)(v86_cs<<4)+(unsigned int)v86_ip+1))&0xff;
				mem_writew(SegPhys(ss)+((reg_esp+0) & cpu.stack.mask),v86_ip+2u);
				break;
			case 0xe5:		// IN AX,Ib
				if (!VCPI_trapio_r(mem_readb((unsigned int)(v86_cs<<4u)+(unsigned int)v86_ip+1u),2))
					reg_ax=IO_ReadW(mem_readb((unsigned int)(v86_cs<<4)+(unsigned int)v86_ip+1))&0xffff;
				mem_writew(SegPhys(ss)+((reg_esp+0) & cpu.stack.mask),v86_ip+2u);
				break;
			case 0xe6:		// OUT Ib,AL
				if (!VCPI_trapio_w(mem_readb((unsigned int)(v86_cs<<4u)+(unsigned int)v86_ip+1u),reg_al,1))
					IO_WriteB(mem_readb((unsigned int)(v86_cs<<4)+(unsigned int)v86_ip+1),reg_al);
				mem_writew(SegPhys(ss)+((reg_esp+0) & cpu.stack.mask),v86_ip+2u);
				break;
			case 0xe7:		// OUT Ib,AX
				if (!VCPI_trapio_w(mem_readb((unsigned int)(v86_cs<<4u)+(unsigned int)v86_ip+1u),reg_ax,2u))
					IO_WriteW(mem_readb((unsigned int)(v86_cs<<4)+(unsigned int)v86_ip+1),reg_ax);
				mem_writew(SegPhys(ss)+((reg_esp+0) & cpu.stack.mask),v86_ip+2u);
				break;
			case 0xec:		// IN AL,DX
				if (!VCPI_trapio_r(reg_dx,1))
					reg_al=IO_ReadB(reg_dx)&0xff;
				mem_writew(SegPhys(ss)+((reg_esp+0) & (unsigned int)cpu.stack.mask),(unsigned int)v86_ip+1u);
				break;
			case 0xed:		// IN AX,DX
				if (!VCPI_trapio_r(reg_dx,2))
					reg_ax=IO_ReadW(reg_dx)&0xffff;
				mem_writew(SegPhys(ss)+((reg_esp+0) & (unsigned int)cpu.stack.mask),(unsigned int)v86_ip+1u);
				break;
			case 0xee:		// OUT DX,AL
				if (!VCPI_trapio_w(reg_dx,reg_al,1))
					IO_WriteB(reg_dx,reg_al);
				mem_writew(SegPhys(ss)+((reg_esp+0) & (unsigned int)cpu.stack.mask),(unsigned int)v86_ip+1u);
				break;
			case 0xef:		// OUT DX,AX
				if (!VCPI_trapio_w(reg_dx,reg_ax,2))
					IO_WriteW(reg_dx,reg_ax);
				mem_writew(SegPhys(ss)+((reg_esp+0) & (unsigned int)cpu.stack.mask),(unsigned int)v86_ip+1u);
				break;
			case 0xf0:		// LOCK prefix
				mem_writew(SegPhys(ss)+((reg_esp+0) & (unsigned int)cpu.stack.mask),(unsigned int)v86_ip+1u);
				break;
			case 0xf4:		// HLT
				reg_flags|=FLAG_IF;
				CPU_HLT(reg_eip);
				mem_writew(SegPhys(ss)+((reg_esp+0) & (unsigned int)cpu.stack.mask),(unsigned int)v86_ip+1u);
				break;
			default:
				E_Exit("Unhandled opcode %x caused a protection fault!",v86_opcode);
		}
		return CBRET_NONE;
	}

	/* Get address to interrupt handler */
	Bit16u vint_vector_seg=mem_readw((PhysPt)(SegValue(ds)+int_num+2u));
	Bit16u vint_vector_ofs=mem_readw((PhysPt)int_num);
	if (reg_sp!=0x1fdau) reg_esp+=(2u+3u*4u);	// Interrupt from within protected mode
	else reg_esp+=2;

	/* Read entries that were pushed onto the stack by the interrupt */
	Bit16u return_ip=mem_readw(SegPhys(ss)+(reg_esp & cpu.stack.mask));
	Bit16u return_cs=mem_readw(SegPhys(ss)+((reg_esp+4u) & cpu.stack.mask));
	Bit32u return_eflags=mem_readd(SegPhys(ss)+((reg_esp+8u) & cpu.stack.mask));

	/* Modify stack to call v86-interrupt handler */
	mem_writed(SegPhys(ss)+(reg_esp & cpu.stack.mask),vint_vector_ofs);
	mem_writed(SegPhys(ss)+((reg_esp+4u) & cpu.stack.mask),vint_vector_seg);
	mem_writed(SegPhys(ss)+((reg_esp+8u) & cpu.stack.mask),return_eflags&(~(FLAG_IF|FLAG_TF)));

	/* Adjust SP of v86-stack */
	Bit16u v86_ss=mem_readw(SegPhys(ss)+((reg_esp+0x10u) & cpu.stack.mask));
	Bit16u v86_sp=mem_readw(SegPhys(ss)+((reg_esp+0x0cu) & cpu.stack.mask))-6u;
	mem_writew(SegPhys(ss)+((reg_esp+0x0cu) & cpu.stack.mask),v86_sp);

	/* Return to original code after v86-interrupt handler */
	mem_writew((unsigned int)(v86_ss<<4u)+(unsigned int)v86_sp+0u,return_ip);
	mem_writew((unsigned int)(v86_ss<<4u)+(unsigned int)v86_sp+2u,return_cs);
	mem_writew((unsigned int)(v86_ss<<4u)+(unsigned int)v86_sp+4u,(Bit16u)(return_eflags&0xffffu));
	return CBRET_NONE;
}

inline void VCPI_iopermw(uint16_t port,bool set) {
	unsigned char b;

	b = mem_readb((unsigned int)vcpi.private_area+0x3000u+0x68u+((unsigned int)port>>3u));
	if (set) b |= 1u<<(port&7u);
	else b &= ~(1u<<(port&7u));
	mem_writeb((unsigned int)vcpi.private_area+0x3000u+0x68u+((unsigned int)port>>3u),b);
}

static void SetupVCPI() {
    Bitu old_a20 = XMS_GetEnabledA20();

	/* The EMM OS handle is often located just above the 1MB boundary.
	 * And we're about to write that area directly. So for obvious
	 * reasons we should enable the A20 gate now. This fixes random
	 * crashes in v86 mode when a20=mask as opposed to a20=fast. */
	if (((unsigned int)emm_handles[vcpi.ems_handle].mem<<12u) & (1u<<20u)) {
		LOG(LOG_MISC,LOG_DEBUG)("EMS:EMM OS handle is associated with memory on an odd megabyte. Enabling A20 gate to avoid corrupting DOS state, will restore A20 state after this setup phase.");
		XMS_EnableA20(true);
	}

	vcpi.ems_handle=0;	// use EMM system handle for VCPI data

	vcpi.enabled=true;

	vcpi.pic1_remapping=0x08;	// master PIC base
	vcpi.pic2_remapping=0x70;	// slave PIC base

	vcpi.private_area=(MemHandle)((unsigned int)emm_handles[vcpi.ems_handle].mem<<12u);

	/* GDT */
	mem_writed((unsigned int)vcpi.private_area+0x0000,0x00000000);	// descriptor 0
	mem_writed((unsigned int)vcpi.private_area+0x0004,0x00000000);	// descriptor 0

	Bit32u ldt_address=((unsigned int)vcpi.private_area+0x1000);
	Bit16u ldt_limit=0xff;
	Bit32u ldt_desc_part=(((unsigned int)ldt_address&0xffff)<<16u)|(unsigned int)ldt_limit;
	mem_writed((unsigned int)vcpi.private_area+0x0008,(unsigned int)ldt_desc_part);	// descriptor 1 (LDT)
	ldt_desc_part=(((unsigned int)ldt_address&0xff0000)>>16)|((unsigned int)ldt_address&0xff000000)|0x8200;
	mem_writed((unsigned int)vcpi.private_area+0x000c,(unsigned int)ldt_desc_part);	// descriptor 1

	Bit32u tss_address=((unsigned int)vcpi.private_area+0x3000);
	Bit32u tss_desc_part=(((unsigned int)tss_address&0xffff)<<16u)|(0x0068+0x200);
	mem_writed((unsigned int)vcpi.private_area+0x0010,(unsigned int)tss_desc_part);	// descriptor 2 (TSS)
	tss_desc_part=(((unsigned int)tss_address&0xff0000)>>16)|((unsigned int)tss_address&0xff000000)|0x8900;
	mem_writed((unsigned int)vcpi.private_area+0x0014,(unsigned int)tss_desc_part);	// descriptor 2

	/* LDT */
	mem_writed((unsigned int)vcpi.private_area+0x1000,0x00000000);	// descriptor 0
	mem_writed((unsigned int)vcpi.private_area+0x1004,0x00000000);	// descriptor 0
	Bit32u cs_desc_part=(((unsigned int)vcpi.private_area&0xffff)<<16u)|0xffff;
	mem_writed((unsigned int)vcpi.private_area+0x1008,(unsigned int)cs_desc_part);	// descriptor 1 (code)
	cs_desc_part=(((unsigned int)vcpi.private_area&0xff0000)>>16u)|((unsigned int)vcpi.private_area&0xff000000)|0x9a00;
	mem_writed((unsigned int)vcpi.private_area+0x100c,(unsigned int)cs_desc_part);	// descriptor 1
	Bit32u ds_desc_part=(((unsigned int)vcpi.private_area&0xffff)<<16u)|0xffff;
	mem_writed((unsigned int)vcpi.private_area+0x1010,(unsigned int)ds_desc_part);	// descriptor 2 (data)
	ds_desc_part=(((unsigned int)vcpi.private_area&0xff0000)>>16u)|((unsigned int)vcpi.private_area&0xff000000)|0x9200;
	mem_writed((unsigned int)vcpi.private_area+0x1014,(unsigned int)ds_desc_part);	// descriptor 2

	/* IDT setup */
	for (Bit16u int_ct=0; int_ct<0x100; int_ct++) {
		/* build a CALL NEAR V86MON, the value of IP pushed by the
			CALL is used to identify the interrupt number */
		mem_writeb((unsigned int)vcpi.private_area+0x2800+(unsigned int)int_ct*4u+0,0xe8);	// call
		mem_writew((unsigned int)vcpi.private_area+0x2800+(unsigned int)int_ct*4u+1,0x05fd-((unsigned int)int_ct*4u));
		mem_writeb((unsigned int)vcpi.private_area+0x2800+(unsigned int)int_ct*4u+3,0xcf);	// iret (dummy)

		/* put a Gate-Descriptor into the IDT */
		mem_writed((unsigned int)vcpi.private_area+0x2000+(unsigned int)int_ct*8u+0,0x000c0000|(0x2800+(unsigned int)int_ct*4u));
		mem_writed((unsigned int)vcpi.private_area+0x2000+(unsigned int)int_ct*8u+4,0x0000ee00);
	}

	/* TSS */
	for (Bitu tse_ct=0; tse_ct<0x68+0x2000/*all 65536 I/O ports*/; tse_ct++) {
		/* clear the TSS as most entries are not used here */
		mem_writeb((unsigned int)vcpi.private_area+0x3000+(unsigned int)tse_ct,0);
	}

	/* trap some ports */
	VCPI_iopermw(0x92,true);

	/* Set up the ring0-stack */
	mem_writed((unsigned int)vcpi.private_area+0x3004,0x00002000);	// esp
	mem_writed((unsigned int)vcpi.private_area+0x3008,0x00000014);	// ss

	mem_writed((unsigned int)vcpi.private_area+0x3066,0x0068);		// io-map base (map follows, all zero)

    XMS_EnableA20(old_a20 != 0);
}

static Bitu INT4B_Handler() {
	switch (reg_ah) {
	case 0x81:
		CALLBACK_SCF(true);
		reg_ax=0x1;
		break;
	default:
		LOG(LOG_MISC,LOG_WARN)("Unhandled interrupt 4B function %x",reg_ah);
		break;
	}
	return CBRET_NONE;
}

Bitu GetEMSType(Section_prop * section) {
	std::string emstypestr = section->Get_string("ems");
	Bitu rtype = 0;

	if (emstypestr == "true")
		rtype = EMS_MIXED;
	else if (emstypestr == "emsboard")
		rtype = EMS_BOARD;
	else if (emstypestr == "emm386")
		rtype = EMS_EMM386;
	else
		rtype = EMS_NONE;

	return rtype;
}

void setup_EMS_none() {
	if (zero_int67_if_no_ems) {
		/* zero INT 67h */
		phys_writed(0x67*4,0);
	}
}

Bitu call_int67 = 0;

class EMS: public Module_base {
private:
    Bit16u ems_baseseg = 0;
    DOS_Device* emm_device = NULL;
    unsigned int oshandle_memsize_16kb = 0;
    RealPt /*old4b_pointer,*/old67_pointer = 0/*NULL*/;
	CALLBACK_HandlerObject call_vdma,call_vcpi,call_v86mon;

public:
	EMS(Section* configuration):Module_base(configuration) {

		/* Virtual DMA interrupt callback */
		call_vdma.Install(&INT4B_Handler,CB_IRET,"Int 4b vdma");
		call_vdma.Set_RealVec(0x4b);

		vcpi.enabled=false;
		GEMMIS_seg=0;

		Section_prop * section=static_cast<Section_prop *>(configuration);
		ems_syshandle_on_even_mb = section->Get_bool("ems system handle on even megabyte");
		zero_int67_if_no_ems = section->Get_bool("zero int 67h if no ems");
		ems_type = GetEMSType(section);
		if (ems_type == EMS_NONE) {
			setup_EMS_none();
			return;
		}

		if (machine == MCH_PCJR) {
			setup_EMS_none();
			ems_type = EMS_NONE;
			LOG_MSG("EMS disabled for PCJr machine");
			return;
		}

        LOG_MSG("EMS page frame at 0x%04x-0x%04x",EMM_PAGEFRAME,EMM_PAGEFRAME+0xFFF);

		ENABLE_VCPI = section->Get_bool("vcpi");
		ENABLE_V86_STARTUP = section->Get_bool("emm386 startup active");

		/* if 286 or lower, EMM386 emulation is impossible */
		if (CPU_ArchitectureType < CPU_ARCHTYPE_386 && ems_type != EMS_BOARD) {
			LOG_MSG("CPU is 286 or lower, setting EMS emulation to ems=emsboard and disabling VCPI and v86 startup");
			ENABLE_V86_STARTUP = false;
			ems_type = EMS_BOARD;
			ENABLE_VCPI = false;
		}

        bool XMS_Active(void);

        /* if XMS is not enabled, EMM386 emulation is impossible.
         * Real MS-DOS EMM386.EXE will flat out refuse to load if HIMEM.SYS is not loaded.
         * that also prevents VCPI from working. */
        if (!XMS_Active() && ems_type != EMS_BOARD) {
            if (ems_type == EMS_MIXED) {
                LOG_MSG("EMS changed to board mode and VCPI disabled, because XMS is not enabled.");
                ems_type = EMS_BOARD;
            }
            else if (ems_type == EMS_EMM386) {
                /* do as MS-DOS does */
                setup_EMS_none();
                ems_type = EMS_NONE;
                LOG_MSG("EMS disabled, EMM386 emulation is impossible when XMS is not enabled");
                return;
            }

			ENABLE_V86_STARTUP = false;
            ENABLE_VCPI = false;
        }

        if (ems_type != EMS_BOARD)
            BIOS_ZeroExtendedSize(true);

		dbg_zero_on_ems_allocmem = section->Get_bool("zero memory on ems memory allocation");
		if (dbg_zero_on_ems_allocmem) {
			LOG(LOG_MISC,LOG_DEBUG)("Debug option enabled: EMS memory allocation will always clear memory block before returning\n");
		}

		/* FIXME: VM86 monitor is not stable! */
		if (ENABLE_V86_STARTUP) {
			LOG(LOG_MISC,LOG_WARN)("EMM386 virtual 8086 monitor is not stable! Use with caution!");
		}

		if (ems_type == EMS_BOARD && ENABLE_VCPI) {
			LOG_MSG("VCPI emulation is incompatible with ems=board. Turning off VCPI emulation");
			ENABLE_VCPI=false;
		}
		if (ems_type != EMS_EMM386 && ENABLE_V86_STARTUP) {
			/* starting up in virtual 8086 mode makes no sense unless emulating EMM386.EXE */
			if (ems_type != EMS_MIXED) {
				/* v86 startup is default. to avoid yelling at the user do not print anything unless
				 * the user set it to any value other than the default ems=true */
				LOG_MSG("EMS EMM386.EXE v86 mode is incompatible with ems= setting. Starting up in real mode.");
			}
			ENABLE_V86_STARTUP=false;
		}
		if (ENABLE_V86_STARTUP && !ENABLE_VCPI) {
			LOG_MSG("EMS: DOSBox does not support enabling virtual 8086 mode without VCPI.");
			ENABLE_V86_STARTUP=false;
		}

		oshandle_memsize_16kb = (unsigned int)section->Get_int("ems system handle memory size");
		/* convert KB to 16KB pages */
		oshandle_memsize_16kb = (oshandle_memsize_16kb+15u)/16u;
		if (oshandle_memsize_16kb == 0) oshandle_memsize_16kb = 1;

		ems_baseseg=DOS_GetMemory(2,"ems_baseseg");	//We have 32 bytes

		/* Add a little hack so it appears that there is an actual ems device installed */
		char const* emsname="EMMXXXX0";
		MEM_BlockWrite(PhysMake(ems_baseseg,0xa),emsname,(Bitu)(strlen(emsname)+1));

		call_int67=CALLBACK_Allocate();
		CALLBACK_Setup(call_int67,&INT67_Handler,CB_IRET,PhysMake(ems_baseseg,4),"Int 67 ems");
		RealSetVec(0x67,RealMake(ems_baseseg,4),old67_pointer);

		/* Register the ems device */
		//TODO MAYBE put it in the class.
		emm_device = new device_EMM(ems_type != EMS_BOARD);
		DOS_AddDevice(emm_device);

		/* Clear handle and page tables */
		Bitu i;
		for (i=0;i<EMM_MAX_HANDLES;i++) {
			emm_handles[i].mem=0;
			emm_handles[i].pages=NULL_HANDLE;
			memset(&emm_handles[i].name,0,8);
		}
		for (i=0;i<EMM_MAX_PHYS;i++) {
			emm_mappings[i].page=NULL_PAGE;
			emm_mappings[i].handle=NULL_HANDLE;
		}
		for (i=0;i<0x40;i++) {
			emm_segmentmappings[i].page=NULL_PAGE;
			emm_segmentmappings[i].handle=NULL_HANDLE;
		}

		if (EMM_AllocateSystemHandle(oshandle_memsize_16kb) != EMM_NO_ERROR) { // allocate OS-dedicated handle (ems handle zero, 384kb)
			LOG_MSG("EMS:Unable to allocate EMS system handle. disabling VCPI");
			ENABLE_VCPI = false;
		}

		if (ems_type == EMS_EMM386) {
			DMA_SetWrapping(0xffffffff);	// emm386-bug that disables dma wrapping
		}

		/* the VCPI emulation requires a large enough OS handle memory region. */
		if (ENABLE_VCPI && oshandle_memsize_16kb < (0x4000/16384)) { /* at least 16KB */
			LOG_MSG("EMS:System handle memory size too small (<16KB), disabling VCPI");
			ENABLE_VCPI = false;
		}

		if (ENABLE_VCPI) {
			assert(ems_type != EMS_BOARD);
			LOG(LOG_MISC,LOG_DEBUG)("Enabling VCPI emulation");

			/* Install a callback that handles VCPI-requests in protected mode requests */
			call_vcpi.Install(&VCPI_PM_Handler,CB_IRETD,"VCPI PM");
			vcpi.pm_interface=(call_vcpi.Get_callback())*CB_SIZE;

			/* Initialize private data area and set up descriptor tables */
			SetupVCPI();

			if (!vcpi.enabled) return;

			/* Install v86-callback that handles interrupts occuring
			   in v86 mode, including protection fault exceptions */
			call_v86mon.Install(&V86_Monitor,CB_IRET,"V86 Monitor");

            {
                Bitu old_a20 = XMS_GetEnabledA20();

                XMS_EnableA20(true);

                mem_writeb((unsigned int)vcpi.private_area+0x2e00,(Bit8u)0xFE);       //GRP 4
                mem_writeb((unsigned int)vcpi.private_area+0x2e01,(Bit8u)0x38);       //Extra Callback instruction
                mem_writew((unsigned int)vcpi.private_area+0x2e02,call_v86mon.Get_callback());		//The immediate word
                mem_writeb((unsigned int)vcpi.private_area+0x2e04,(Bit8u)0x66);
                mem_writeb((unsigned int)vcpi.private_area+0x2e05,(Bit8u)0xCF);       //A IRETD Instruction

                XMS_EnableA20(old_a20 != 0);
            }

			/* DOSBox's default EMS emulation provides the EMS memory mapping but without the virtual 8086
			 * mode. But there are DOS games and demos that assume EMM386.EXE == virtual 8086 mode and will
			 * fail to detect EMS if the v86 bit in EFLAGS is not set.
			 *
			 * Examples:
			 *   ftp://ftp.scene.org/pub/parties/1994/3s94/demo/friends.zip
			 *     - Refuses to run unless it detects Expanded memory
			 *     - Does not detect Expanded memory because it doesn't detect virtual 8086 mode
			 *     - Therefore, without this option, it is impossible to run the demo. */
			if (ENABLE_V86_STARTUP) {
				LOG(LOG_MISC,LOG_DEBUG)("EMS: Now setting up the DOS environment to run in EMM386.EXE virtual 8086 mode");

				/* our V86 state may require A20 enabled to run. the EMM OS handle
				 * often resides on an odd megabyte */
				if (((unsigned int)emm_handles[vcpi.ems_handle].mem<<12u) & (1u<<20u)) {
					LOG(LOG_MISC,LOG_DEBUG)("EMS:EMM OS handle is associated with memory on an odd megabyte. Enabling A20 gate to safely enter V86 mode.");
					XMS_EnableA20(true);
				}
				vcpi_virtual_a20 = true;

				/* Prepare V86-task */
				CPU_SET_CRX(0, 1);
				CPU_LGDT(0xff, (unsigned int)vcpi.private_area+0x0000);
				CPU_LIDT(0x7ff, (unsigned int)vcpi.private_area+0x2000);
				if (CPU_LLDT(0x08)) LOG_MSG("VCPI:Could not load LDT");
				if (CPU_LTR(0x10)) LOG_MSG("VCPI:Could not load TR");

				/* TODO: Page tables are usually involved as well. That is the "magic"
				 * behind EMM386.EXE page frames. */

				/* TODO: Also setup (here or in SetupVCPI) the I/O permission bitmap
				 * so that the V86 monitor can virtualize certain I/O ports. EMM386.EXE
				 * emulation would demand that we at least virtualize the DMA controller
				 * I/O ports as MS-DOS does. */

				/* register setup */
				CPU_Push32(SegValue(gs));
				CPU_Push32(SegValue(fs));
				CPU_Push32(SegValue(ds));
				CPU_Push32(SegValue(es));
				CPU_Push32(SegValue(ss));
				CPU_Push32(0x23002); /* FIXME: Confirm: this is EFLAGS? */
				CPU_Push32(SegValue(cs));
				CPU_Push32(reg_eip&0xffff);
				/* Switch to V86-mode */
				CPU_SetCPL(0);
				CPU_IRET(true,0);
			}
		}
	}
	
	~EMS() {
		if (ems_type==0) return;

		/* Undo Biosclearing */
		BIOS_ZeroExtendedSize(false);
 
		/* Remove ems device */
		if (emm_device!=NULL) {
// FIXME: This is being called now after DOS shutdown. Causes a crash!
//			DOS_DelDevice(emm_device);
			emm_device=NULL;
		}
		GEMMIS_seg=0;

		/* Remove the emsname and callback hack */
		if (ems_baseseg != 0) {
			char buf[32]= { 0 };
			MEM_BlockWrite(PhysMake(ems_baseseg,0),buf,32);
		}
		RealSetVec(0x67,zero_int67_if_no_ems ? 0 : old67_pointer);

#if 0 // FIXME
		/* Release memory allocated to system handle */
		if (emm_handles[EMM_SYSTEM_HANDLE].pages != NULL_HANDLE) {
			MEM_ReleasePages(emm_handles[EMM_SYSTEM_HANDLE].mem);
		}
#endif

		/* Clear handle and page tables */
		//TODO

		if (ENABLE_VCPI && vcpi.enabled) {
			if (cpu.pmode && GETFLAG(VM)) {
				/* Switch back to real mode if in v86-mode */
				CPU_SET_CRX(0, 0);
				CPU_SET_CRX(3, 0);
				reg_flags&=(~(FLAG_IOPL|FLAG_VM));
				CPU_LIDT(0x3ff, 0);
				CPU_SetCPL(0);
			}
		}
	}
};
		
static EMS* test = NULL;

void CALLBACK_DeAllocate(Bitu in);

void EMS_DoShutDown() {
	if (test != NULL) {
		delete test;
		test = NULL;
	}
    if (call_int67 != 0) {
        CALLBACK_DeAllocate(call_int67);
        call_int67 = 0;
    }
}

void EMS_PickPageFrame(void) {
    /* the EMS page frame needs to move depending on IBM PC or PC-98 emulation.
     * IBM PC emulation can put the page frame at 0xE000 (as DOSBox has always done).
     * PC-98 emulation needs to move the page frame down because 0xE000 is taken by the 4th EGC bitplane. */
    EMM_PAGEFRAME =      IS_PC98_ARCH ? 0xD000 : 0xE000;
    EMM_PAGEFRAME4K =    ((EMM_PAGEFRAME*16)/4096);
}

void EMS_DOSBoot(Section* /*sec*/) {
    EMS_PickPageFrame();
}

void EMS_ShutDown(Section* /*sec*/) {
	EMS_DoShutDown();
}

void EMS_Startup(Section* sec) {
    (void)sec;//UNUSED
	if (test == NULL) {
		LOG(LOG_MISC,LOG_DEBUG)("Allocating EMS emulation");
		test = new EMS(control->GetSection("dos"));
	}
}

void EMS_Init() {
	LOG(LOG_MISC,LOG_DEBUG)("Initializing EMS expanded memory services");

	AddExitFunction(AddExitFunctionFuncPair(EMS_ShutDown),true);
	AddVMEventFunction(VM_EVENT_RESET,AddVMEventFunctionFuncPair(EMS_ShutDown));
    AddVMEventFunction(VM_EVENT_DOS_BOOT,AddVMEventFunctionFuncPair(EMS_DOSBoot));
	AddVMEventFunction(VM_EVENT_DOS_EXIT_BEGIN,AddVMEventFunctionFuncPair(EMS_ShutDown));
}

