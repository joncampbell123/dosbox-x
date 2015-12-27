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
#include <string.h>
#include <stddef.h>
#include "dosbox.h"
#include "callback.h"
#include "mem.h"
#include "regs.h"
#include "dos_inc.h"
#include "setup.h"
#include "inout.h"
#include "xms.h"
#include "bios.h"
#include "cpu.h"
#include "control.h"

#include <algorithm>

#define XMS_HANDLES							50		/* 50 XMS Memory Blocks */ 
#define XMS_VERSION    						0x0300	/* version 3.00 */
#define XMS_DRIVER_VERSION					0x0301	/* my driver version 3.01 */

#define	XMS_GET_VERSION						0x00
#define	XMS_ALLOCATE_HIGH_MEMORY			0x01
#define	XMS_FREE_HIGH_MEMORY				0x02
#define	XMS_GLOBAL_ENABLE_A20				0x03
#define	XMS_GLOBAL_DISABLE_A20				0x04
#define	XMS_LOCAL_ENABLE_A20				0x05
#define	XMS_LOCAL_DISABLE_A20				0x06
#define	XMS_QUERY_A20						0x07
#define	XMS_QUERY_FREE_EXTENDED_MEMORY		0x08
#define	XMS_ALLOCATE_EXTENDED_MEMORY		0x09
#define	XMS_FREE_EXTENDED_MEMORY			0x0a
#define	XMS_MOVE_EXTENDED_MEMORY_BLOCK		0x0b
#define	XMS_LOCK_EXTENDED_MEMORY_BLOCK		0x0c
#define	XMS_UNLOCK_EXTENDED_MEMORY_BLOCK	0x0d
#define	XMS_GET_EMB_HANDLE_INFORMATION		0x0e
#define	XMS_RESIZE_EXTENDED_MEMORY_BLOCK	0x0f
#define	XMS_ALLOCATE_UMB					0x10
#define	XMS_DEALLOCATE_UMB					0x11
#define XMS_QUERY_ANY_FREE_MEMORY			0x88
#define XMS_ALLOCATE_ANY_MEMORY				0x89
#define	XMS_GET_EMB_HANDLE_INFORMATION_EXT	0x8e
#define XMS_RESIZE_ANY_EXTENDED_MEMORY_BLOCK 0x8f

#define	XMS_FUNCTION_NOT_IMPLEMENTED		0x80
#define	HIGH_MEMORY_NOT_EXIST				0x90
#define	HIGH_MEMORY_IN_USE					0x91
#define HIGH_MEMORY_NOT_BIG_ENOUGH			0x92
#define	HIGH_MEMORY_NOT_ALLOCATED			0x93
#define XMS_OUT_OF_SPACE					0xa0
#define XMS_OUT_OF_HANDLES					0xa1
#define XMS_INVALID_HANDLE					0xa2
#define XMS_INVALID_SOURCE_HANDLE			0xa3
#define XMS_INVALID_SOURCE_OFFSET			0xa4
#define XMS_INVALID_DEST_HANDLE				0xa5
#define XMS_INVALID_DEST_OFFSET				0xa6
#define XMS_INVALID_LENGTH					0xa7
#define XMS_BLOCK_NOT_LOCKED				0xaa
#define XMS_BLOCK_LOCKED					0xab
#define	UMB_ONLY_SMALLER_BLOCK				0xb0
#define	UMB_NO_BLOCKS_AVAILABLE				0xb1

bool DOS_IS_IN_HMA();

extern Bitu rombios_minimum_location;

Bitu xms_hma_minimum_alloc = 0;
bool xms_hma_exists = true;
bool xms_hma_application_has_control = false;
bool xms_hma_alloc_non_dos_kernel_control = true;

struct XMS_Block {
	Bitu	size;
	MemHandle mem;
	Bit8u	locked;
	bool	free;
};

#ifdef _MSC_VER
#pragma pack (1)
#endif
struct XMS_MemMove{
	Bit32u length;
	Bit16u src_handle;
	union {
		RealPt realpt;
		Bit32u offset;
	} src;
	Bit16u dest_handle;
	union {
		RealPt realpt;
		Bit32u offset;
	} dest;

} GCC_ATTRIBUTE(packed);
#ifdef _MSC_VER
#pragma pack ()
#endif

static int xms_local_enable_count = 0;

Bitu XMS_EnableA20(bool enable) {
	Bit8u val = IO_Read	(0x92);
	if (enable) IO_Write(0x92,val | 2);
	else		IO_Write(0x92,val & ~2);
//	LOG_MSG("XMS A20 enable=%u lock=%u",enable?1:0,xms_local_enable_count);
	return 0;
}

Bitu XMS_GetEnabledA20(void) {
	return (IO_Read(0x92)&2)>0;
}

static RealPt xms_callback;
static bool umb_available = false;
static bool umb_init = false;

static XMS_Block xms_handles[XMS_HANDLES];

static INLINE bool InvalidHandle(Bitu handle) {
	return (!handle || (handle>=XMS_HANDLES) || xms_handles[handle].free);
}

Bitu XMS_QueryFreeMemory(Bit32u& largestFree, Bit32u& totalFree) {
	/* Scan the tree for free memory and find largest free block */
	totalFree=(Bit32u)(MEM_FreeTotal()*4);
	largestFree=(Bit32u)(MEM_FreeLargest()*4);
	if (!totalFree) return XMS_OUT_OF_SPACE;
	return 0;
}

void XMS_ZeroAllocation(MemHandle mem,unsigned int pages) {
	PhysPt address;

	if (pages == 0) return;
	address = mem*4096;
	pages *= 4096;

	if ((address+pages) > 0xC0000000) E_Exit("XMS_ZeroAllocation out of range");
	while (pages != 0) {
		mem_writeb(address++,0);
		pages--;
	}
}

extern bool enable_a20_on_windows_init;
extern bool dbg_zero_on_xms_allocmem;

Bitu XMS_AllocateMemory(Bitu size, Bit16u& handle) {	// size = kb
	/* Find free handle */
	Bit16u index=1;
	while (!xms_handles[index].free) {
		if (++index>=XMS_HANDLES) return XMS_OUT_OF_HANDLES;
	}
	MemHandle mem;
	if (size!=0) {
		Bitu pages=(size/4) + ((size & 3) ? 1 : 0);
		mem=MEM_AllocatePages(pages,true);
		if (!mem) return XMS_OUT_OF_SPACE;
		if (dbg_zero_on_xms_allocmem) XMS_ZeroAllocation(mem,pages);
	} else {
		mem=MEM_GetNextFreePage();
		if (mem==0) LOG(LOG_MISC,LOG_DEBUG)("XMS:Allocate zero pages with no memory left"); // Windows 3.1 triggers this surprisingly often!
		if (mem != 0 && dbg_zero_on_xms_allocmem) XMS_ZeroAllocation(mem,1);
	}
	xms_handles[index].free=false;
	xms_handles[index].mem=mem;
	xms_handles[index].locked=0;
	xms_handles[index].size=size;
	handle=index;
	return 0;
}

Bitu XMS_FreeMemory(Bitu handle) {
	if (InvalidHandle(handle)) return XMS_INVALID_HANDLE;
	MEM_ReleasePages(xms_handles[handle].mem);
	xms_handles[handle].mem=-1;
	xms_handles[handle].size=0;
	xms_handles[handle].free=true;
	return 0;
}

Bitu XMS_MoveMemory(PhysPt bpt) {
	/* Read the block with mem_read's */
	Bitu length=mem_readd(bpt+offsetof(XMS_MemMove,length));
	Bitu src_handle=mem_readw(bpt+offsetof(XMS_MemMove,src_handle));
	union {
		RealPt realpt;
		Bit32u offset;
	} src,dest;
	src.offset=mem_readd(bpt+offsetof(XMS_MemMove,src.offset));
	Bitu dest_handle=mem_readw(bpt+offsetof(XMS_MemMove,dest_handle));
	dest.offset=mem_readd(bpt+offsetof(XMS_MemMove,dest.offset));
	PhysPt srcpt,destpt;
	if (src_handle) {
		if (InvalidHandle(src_handle)) {
			return XMS_INVALID_SOURCE_HANDLE;
		}
		if (src.offset>=(xms_handles[src_handle].size*1024U)) {
			return XMS_INVALID_SOURCE_OFFSET;
		}
		if (length>xms_handles[src_handle].size*1024U-src.offset) {
			return XMS_INVALID_LENGTH;
		}
		srcpt=(xms_handles[src_handle].mem*4096)+src.offset;
	} else {
		srcpt=Real2Phys(src.realpt);
	}
	if (dest_handle) {
		if (InvalidHandle(dest_handle)) {
			return XMS_INVALID_DEST_HANDLE;
		}
		if (dest.offset>=(xms_handles[dest_handle].size*1024U)) {
			return XMS_INVALID_DEST_OFFSET;
		}
		if (length>xms_handles[dest_handle].size*1024U-dest.offset) {
			return XMS_INVALID_LENGTH;
		}
		destpt=(xms_handles[dest_handle].mem*4096)+dest.offset;
	} else {
		destpt=Real2Phys(dest.realpt);
	}
//	LOG_MSG("XMS move src %X dest %X length %X",srcpt,destpt,length);
	mem_memcpy(destpt,srcpt,length);
	return 0;
}

Bitu XMS_LockMemory(Bitu handle, Bit32u& address) {
	if (InvalidHandle(handle)) return XMS_INVALID_HANDLE;
	if (xms_handles[handle].locked<255) xms_handles[handle].locked++;
	address = xms_handles[handle].mem*4096;
	return 0;
}

Bitu XMS_UnlockMemory(Bitu handle) {
 	if (InvalidHandle(handle)) return XMS_INVALID_HANDLE;
	if (xms_handles[handle].locked) {
		xms_handles[handle].locked--;
		return 0;
	}
	return XMS_BLOCK_NOT_LOCKED;
}

Bitu XMS_GetHandleInformation(Bitu handle, Bit8u& lockCount, Bit8u& numFree, Bit32u& size) {
	if (InvalidHandle(handle)) return XMS_INVALID_HANDLE;
	lockCount = xms_handles[handle].locked;
	/* Find available blocks */
	numFree=0;
	for (Bitu i=1;i<XMS_HANDLES;i++) {
		if (xms_handles[i].free) numFree++;
	}
	size=(Bit32u)(xms_handles[handle].size);
	return 0;
}

Bitu XMS_ResizeMemory(Bitu handle, Bitu newSize) {
	if (InvalidHandle(handle)) return XMS_INVALID_HANDLE;	
	// Block has to be unlocked
	if (xms_handles[handle].locked>0) return XMS_BLOCK_LOCKED;
	Bitu pages=newSize/4 + ((newSize & 3) ? 1 : 0);
	if (MEM_ReAllocatePages(xms_handles[handle].mem,pages,true)) {
		xms_handles[handle].size = newSize;
		return 0;
	} else return XMS_OUT_OF_SPACE;
}

static bool multiplex_xms(void) {
	switch (reg_ax) {
	case 0x4300:					/* XMS installed check */
			reg_al=0x80;
			return true;
	case 0x4310:					/* XMS handler seg:offset */
			SegSet16(es,RealSeg(xms_callback));
			reg_bx=RealOff(xms_callback);
			return true;			
	}
	return false;

}

INLINE void SET_RESULT(Bitu res,bool touch_bl_on_succes=true) {
	if(touch_bl_on_succes || res) reg_bl = (Bit8u)res;
	reg_ax = (res==0)?1:0;
}

Bitu XMS_Handler(void) {
//	LOG(LOG_MISC,LOG_ERROR)("XMS: CALL %02X",reg_ah);
	switch (reg_ah) {
	case XMS_GET_VERSION:										/* 00 */
		reg_ax=XMS_VERSION;
		reg_bx=XMS_DRIVER_VERSION;
		reg_dx=xms_hma_exists?1:0;
		break;
	case XMS_ALLOCATE_HIGH_MEMORY:								/* 01 */
		if (xms_hma_exists) {
			if (xms_hma_application_has_control || DOS_IS_IN_HMA()) {
				/* hma already controlled by application or DOS kernel */
				reg_ax=0;
				reg_bl=HIGH_MEMORY_IN_USE;
			}
			else if (reg_dx < xms_hma_minimum_alloc) {
				/* not big enough */
				reg_ax=0;
				reg_bl=HIGH_MEMORY_NOT_BIG_ENOUGH;
			}
			else { /* program allocation */
				LOG(LOG_MISC,LOG_DEBUG)("XMS: HMA allocated by application/TSR");
				xms_hma_application_has_control = true;
				reg_ax=1;
			}
		}
		else {
			reg_ax=0;
			reg_bl=HIGH_MEMORY_NOT_EXIST;
		}
		break;
	case XMS_FREE_HIGH_MEMORY:									/* 02 */
		if (xms_hma_exists) {
			if (DOS_IS_IN_HMA()) LOG(LOG_MISC,LOG_WARN)("DOS application attempted to free HMA while DOS kernel occupies it!");

			if (xms_hma_application_has_control) {
				LOG(LOG_MISC,LOG_DEBUG)("XMS: HMA freed by application/TSR");
				xms_hma_application_has_control = false;
				reg_ax=1;
			}
			else {
				reg_ax=0;
				reg_bl=HIGH_MEMORY_NOT_ALLOCATED;
			}
		}
		else {
			reg_ax=0;
			reg_bl=HIGH_MEMORY_NOT_EXIST;
		}
		break;
	case XMS_GLOBAL_ENABLE_A20:									/* 03 */
		SET_RESULT(XMS_EnableA20(true));
		break;
	case XMS_GLOBAL_DISABLE_A20:								/* 04 */
		SET_RESULT(XMS_EnableA20(false));
		break;
	case XMS_LOCAL_ENABLE_A20:									/* 05 */
		if (xms_local_enable_count == 0) {
			SET_RESULT(XMS_EnableA20(true));
		}
		else {
			SET_RESULT(0);
		}
		xms_local_enable_count++;
		break;
	case XMS_LOCAL_DISABLE_A20:									/* 06 */
		if (xms_local_enable_count > 0) {
			xms_local_enable_count--;
			if (xms_local_enable_count == 0) {
				SET_RESULT(XMS_EnableA20(false));
			}
			else {
				SET_RESULT(0x94/*still enabled*/);
			}
		}
		else {
			/* NTS: The XMS spec says that A20 disable should only happen IF the counter == 1 and decrements to zero.
			 *      Windows 3.1 treats the XMS driver differently on startup (big surprise), and will call LOCAL DISABLE
			 *      and QUERY A20 until we indicate the A20 line is off (at one point during startup). */
			SET_RESULT(XMS_EnableA20(false));
		}
		break;
	case XMS_QUERY_A20:											/* 07 */
		reg_ax = XMS_GetEnabledA20();
		reg_bl = 0;
		break;
	case XMS_QUERY_FREE_EXTENDED_MEMORY:						/* 08 */
		reg_bl = XMS_QueryFreeMemory(reg_eax,reg_edx);
		if (reg_eax > 65535) reg_eax = 65535; /* cap sizes for older DOS programs. newer ones use function 0x88 */
		if (reg_edx > 65535) reg_edx = 65535;
		break;
	case XMS_ALLOCATE_ANY_MEMORY:								/* 89 */
		{ /* Chopping off bits 16-31 to fall through to ALLOCATE_EXTENDED_MEMORY is inaccurate.
		     The Extended Memory Specification states you use all of EDX, so programs can request
		     64MB or more. Even if DOSBox does not (yet) support >= 64MB of RAM. */
		Bit16u handle = 0;
		SET_RESULT(XMS_AllocateMemory(reg_edx,handle));
		reg_dx = handle;
		}; break;
	case XMS_ALLOCATE_EXTENDED_MEMORY:							/* 09 */
		{
		Bit16u handle = 0;
		SET_RESULT(XMS_AllocateMemory(reg_dx,handle));
		reg_dx = handle;
		}; break;
	case XMS_FREE_EXTENDED_MEMORY:								/* 0a */
		SET_RESULT(XMS_FreeMemory(reg_dx));
		break;
	case XMS_MOVE_EXTENDED_MEMORY_BLOCK:						/* 0b */
		SET_RESULT(XMS_MoveMemory(SegPhys(ds)+reg_si),false);
		break;
	case XMS_LOCK_EXTENDED_MEMORY_BLOCK: {						/* 0c */
		Bit32u address;
		Bitu res = XMS_LockMemory(reg_dx, address);
		if(res) reg_bl = (Bit8u)res;
		reg_ax = (res==0);
		if (res==0) { // success
			reg_bx=(Bit16u)(address & 0xFFFF);
			reg_dx=(Bit16u)(address >> 16);
		};
		}; break;
	case XMS_UNLOCK_EXTENDED_MEMORY_BLOCK:						/* 0d */
		SET_RESULT(XMS_UnlockMemory(reg_dx));
		break;
	case XMS_GET_EMB_HANDLE_INFORMATION:  						/* 0e */
		SET_RESULT(XMS_GetHandleInformation(reg_dx,reg_bh,reg_bl,reg_edx),false);
		reg_edx &= 0xFFFF;
		break;
	case XMS_RESIZE_ANY_EXTENDED_MEMORY_BLOCK:					/* 0x8f */
		SET_RESULT(XMS_ResizeMemory(reg_dx, reg_ebx));
		break;
	case XMS_RESIZE_EXTENDED_MEMORY_BLOCK:						/* 0f */
		SET_RESULT(XMS_ResizeMemory(reg_dx, reg_bx));
		break;
	case XMS_ALLOCATE_UMB: {									/* 10 */
		if (!umb_available) {
			reg_ax=0;
			reg_bl=XMS_FUNCTION_NOT_IMPLEMENTED;
			break;
		}
		Bit16u umb_start=dos_infoblock.GetStartOfUMBChain();
		if (umb_start==0xffff) {
			reg_ax=0;
			reg_bl=UMB_NO_BLOCKS_AVAILABLE;
			reg_dx=0;	// no upper memory available
			break;
		}
		/* Save status and linkage of upper UMB chain and link upper
		   memory to the regular MCB chain */
		Bit8u umb_flag=dos_infoblock.GetUMBChainState();
		if ((umb_flag&1)==0) DOS_LinkUMBsToMemChain(1);
		Bit8u old_memstrat=DOS_GetMemAllocStrategy()&0xff;
		DOS_SetMemAllocStrategy(0x40);	// search in UMBs only

		Bit16u size=reg_dx;Bit16u seg;
		if (DOS_AllocateMemory(&seg,&size)) {
			reg_ax=1;
			reg_bx=seg;
		} else {
			reg_ax=0;
			if (size==0) reg_bl=UMB_NO_BLOCKS_AVAILABLE;
			else reg_bl=UMB_ONLY_SMALLER_BLOCK;
			reg_dx=size;	// size of largest available UMB
		}

		/* Restore status and linkage of upper UMB chain */
		Bit8u current_umb_flag=dos_infoblock.GetUMBChainState();
		if ((current_umb_flag&1)!=(umb_flag&1)) DOS_LinkUMBsToMemChain(umb_flag);
		DOS_SetMemAllocStrategy(old_memstrat);
		}
		break;
	case XMS_DEALLOCATE_UMB:									/* 11 */
		if (!umb_available) {
			reg_ax=0;
			reg_bl=XMS_FUNCTION_NOT_IMPLEMENTED;
			break;
		}
		if (dos_infoblock.GetStartOfUMBChain()!=0xffff) {
			if (DOS_FreeMemory(reg_dx)) {
				reg_ax=0x0001;
				break;
			}
		}
		reg_ax=0x0000;
		reg_bl=UMB_NO_BLOCKS_AVAILABLE;
		break;
	case XMS_QUERY_ANY_FREE_MEMORY:								/* 88 */
		reg_bl = XMS_QueryFreeMemory(reg_eax,reg_edx);
		reg_ecx = (MEM_TotalPages()*MEM_PAGESIZE)-1;			// highest known physical memory address
		break;
	case XMS_GET_EMB_HANDLE_INFORMATION_EXT: {					/* 8e */
		Bit8u free_handles;
		Bitu result = XMS_GetHandleInformation(reg_dx,reg_bh,free_handles,reg_edx);
		if (result != 0) reg_bl = result;
		else reg_cx = free_handles;
		reg_ax = (result==0);
		} break;
	default:
		LOG(LOG_MISC,LOG_ERROR)("XMS: unknown function %02X",reg_ah);
		reg_ax=0;
		reg_bl=XMS_FUNCTION_NOT_IMPLEMENTED;
	}
//	LOG(LOG_MISC,LOG_ERROR)("XMS: CALL Result: %02X",reg_bl);
	return CBRET_NONE;
}

bool xms_init = false;

bool keep_umb_on_boot;

extern Bitu VGA_BIOS_SEG;
extern Bitu VGA_BIOS_SEG_END;
extern Bitu VGA_BIOS_Size;

extern bool mainline_compatible_mapping;

bool XMS_IS_ACTIVE() {
	return (xms_callback != 0);
}

bool XMS_HMA_EXISTS() {
	return XMS_IS_ACTIVE() && xms_hma_exists;
}

Bitu GetEMSType(Section_prop * section);
void DOS_GetMemory_Choose();

void ROMBIOS_FreeUnusedMinToLoc(Bitu phys);
bool MEM_unmap_physmem(Bitu start,Bitu end);
Bitu ROMBIOS_MinAllocatedLoc();

void RemoveUMBBlock() {
	/* FIXME: Um... why is umb_available == false even when set to true below? */
	if (umb_init) {
		LOG_MSG("Removing UMB block 0x%04x-0x%04x\n",first_umb_seg,first_umb_seg+first_umb_size-1);
		MEM_unmap_physmem(first_umb_seg<<4,((first_umb_seg+first_umb_size)<<4)-1);
		umb_init = false;
	}
}

class XMS: public Module_base {
private:
	CALLBACK_HandlerObject callbackhandler;
public:
	XMS(Section* configuration):Module_base(configuration){
		Section_prop * section=static_cast<Section_prop *>(configuration);
		umb_available=false;

		if (!section->Get_bool("xms")) return;

		/* NTS: Disable XMS emulation if CPU type is less than a 286, because extended memory did not
		 *      exist until the CPU had enough address lines to read past the 1MB mark.
		 *
		 *      The other reason we do this is that there is plenty of software that assumes 286+ instructions
		 *      if they detect XMS services, including but not limited to:
		 *
		 *      MSD.EXE Microsoft Diagnostics
		 *      Microsoft Windows 3.0
		 *
		 *      Not emulating XMS for 8086/80186 emulation prevents the software from crashing. */

		/* TODO: Add option to allow users to *force* XMS emulation, overriding this lockout, if they're
		 *       crazy enough to see what happens or they want to witness the mis-detection mentioned above. */
		if (CPU_ArchitectureType < CPU_ARCHTYPE_286) {
			LOG_MSG("CPU is 80186 or lower model that lacks the address lines needed for 'extended memory' to exist, disabling XMS");
			return;
		}

		xms_init = true;

		xms_hma_exists = section->Get_bool("hma");
		xms_hma_minimum_alloc = section->Get_int("hma minimum allocation");
		xms_hma_alloc_non_dos_kernel_control = section->Get_bool("hma allow reservation");
		if (xms_hma_minimum_alloc > 0xFFF0) xms_hma_minimum_alloc = 0xFFF0;

		Bitu i;
		BIOS_ZeroExtendedSize(true);
		DOS_AddMultiplexHandler(multiplex_xms);

		enable_a20_on_windows_init = section->Get_bool("enable a20 on windows init");
		dbg_zero_on_xms_allocmem = section->Get_bool("zero memory on xms memory allocation");

		if (dbg_zero_on_xms_allocmem) {
			LOG_MSG("Debug option enabled: XMS memory allocation will always clear memory block before returning\n");
		}

		/* place hookable callback in writable memory area */
		xms_callback=RealMake(DOS_GetMemory(0x1,"xms_callback")-1,0x10);
		callbackhandler.Install(&XMS_Handler,CB_HOOKABLE,Real2Phys(xms_callback),"XMS Handler");
		// pseudocode for CB_HOOKABLE:
		//	jump near skip
		//	nop,nop,nop
		//	label skip:
		//	callback XMS_Handler
		//	retf
	   
		for (i=0;i<XMS_HANDLES;i++) {
			xms_handles[i].free=true;
			xms_handles[i].mem=-1;
			xms_handles[i].size=0;
			xms_handles[i].locked=0;
		}
		/* Disable the 0 handle */
		xms_handles[0].free	= false;

		/* Set up UMB chain */
		keep_umb_on_boot=section->Get_bool("keep umb on boot");
		umb_available=section->Get_bool("umb");
		first_umb_seg=section->Get_hex("umb start");
		first_umb_size=section->Get_hex("umb end");

		DOS_GetMemory_Choose();

		// Sanity check
		if (rombios_minimum_location == 0) E_Exit("Uninitialized ROM BIOS base");

		if (first_umb_seg == 0) {
			first_umb_seg = DOS_PRIVATE_SEGMENT_END;
			if (mainline_compatible_mapping && first_umb_seg < 0xD000)
				first_umb_seg = 0xD000; /* Mainline DOSBox assumes a 128KB UMB region starting at 0xD000 */
			else if (first_umb_seg < VGA_BIOS_SEG_END)
				first_umb_seg = VGA_BIOS_SEG_END;
		}
		if (first_umb_size == 0) first_umb_size = ROMBIOS_MinAllocatedLoc()>>4;

		if (first_umb_seg < 0xC000 || first_umb_seg < DOS_PRIVATE_SEGMENT_END) {
			LOG(LOG_MISC,LOG_WARN)("UMB blocks before 0xD000 conflict with VGA (0xA000-0xBFFF), VGA BIOS (0xC000-0xC7FF) and DOSBox private area (0x%04x-0x%04x)",
				DOS_PRIVATE_SEGMENT,DOS_PRIVATE_SEGMENT_END-1);
			first_umb_seg = 0xC000;
			if (first_umb_seg < (Bitu)DOS_PRIVATE_SEGMENT_END) first_umb_seg = (Bitu)DOS_PRIVATE_SEGMENT_END;
		}
		if (first_umb_seg >= (rombios_minimum_location>>4)) {
			LOG(LOG_MISC,LOG_NORMAL)("UMB starting segment 0x%04x conflict with BIOS at 0x%04x. Disabling UMBs",(int)first_umb_seg,(int)(rombios_minimum_location>>4));
			umb_available = false;
		}
		if (first_umb_size >= (rombios_minimum_location>>4)) {
			/* we can ask the BIOS code to trim back the region, assuming it hasn't allocated anything there yet */
			LOG(LOG_MISC,LOG_DEBUG)("UMB ending segment 0x%04x conflicts with BIOS at 0x%04x, asking BIOS to move aside",(int)first_umb_size,(int)(rombios_minimum_location>>4));
			ROMBIOS_FreeUnusedMinToLoc(first_umb_size<<4);
		}
		if (first_umb_size >= (rombios_minimum_location>>4)) {
			LOG(LOG_MISC,LOG_DEBUG)("UMB ending segment 0x%04x conflicts with BIOS at 0x%04x, truncating region",(int)first_umb_size,(int)(rombios_minimum_location>>4));
			first_umb_size = (rombios_minimum_location>>4)-1;
		}
		if (first_umb_size < first_umb_seg) {
			LOG(LOG_MISC,LOG_NORMAL)("UMB end segment below UMB start. I'll just assume you mean to disable UMBs then.");
			first_umb_size = first_umb_seg - 1;
			umb_available = false;
		}
		first_umb_size = (first_umb_size + 1 - first_umb_seg);
		if (umb_available) {
			LOG(LOG_MISC,LOG_NORMAL)("UMB assigned region is 0x%04x-0x%04x",(int)first_umb_seg,(int)(first_umb_seg+first_umb_size-1));
			if (MEM_map_RAM_physmem(first_umb_seg<<4,((first_umb_seg+first_umb_size)<<4)-1)) {
				memset(GetMemBase()+(first_umb_seg<<4),0x00,first_umb_size<<4);
			}
			else {
				LOG(LOG_MISC,LOG_WARN)("Unable to claim UMB region (perhaps adapter ROM is in the way). Disabling UMB");
				umb_available = false;
			}
		}

		bool ems_available = GetEMSType(section)>0;
		DOS_BuildUMBChain(umb_available,ems_available);
		umb_init = true;
	}

	~XMS(){
		/* Remove upper memory information */
		dos_infoblock.SetStartOfUMBChain(0xffff);
		if (umb_available) {
			dos_infoblock.SetUMBChainState(0);
			umb_available=false;
		}

		if (!xms_init) return;

		/* Undo biosclearing */
		BIOS_ZeroExtendedSize(false);

		/* Remove Multiplex */
		DOS_DelMultiplexHandler(multiplex_xms);

		/* Free used memory while skipping the 0 handle */
		for (Bitu i = 1;i<XMS_HANDLES;i++) 
			if(!xms_handles[i].free) XMS_FreeMemory(i);

		xms_init = false;
	}

};

static XMS* test = NULL;

void XMS_DoShutDown() {
	if (test != NULL) {
		delete test;	
		test = NULL;
	}
}

void XMS_ShutDown(Section* /*sec*/) {
	XMS_DoShutDown();
}

void XMS_OnReset(Section *sec) {
	if (test == NULL) {
		LOG(LOG_MISC,LOG_DEBUG)("Allocating XMS emulation");
		test = new XMS(control->GetSection("dos"));
	}
}

void XMS_Init() {
	LOG(LOG_MISC,LOG_DEBUG)("Initializing XMS extended memory services");

	AddExitFunction(AddExitFunctionFuncPair(XMS_ShutDown),true);
	AddVMEventFunction(VM_EVENT_RESET,AddVMEventFunctionFuncPair(XMS_OnReset));
}

