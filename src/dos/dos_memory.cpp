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


#include "dosbox.h"
#include "mem.h"
#include "bios.h"
#include "dos_inc.h"

// uncomment for alloc/free debug messages
#define DEBUG_ALLOC

Bitu UMB_START_SEG = 0x9FFF;
/* FIXME: This should be a variable that reflects the last RAM segment.
 *        That means 0x9FFF if 640KB or more, or a lesser value if less than 640KB */
//#define UMB_START_SEG 0x9fff

uint16_t first_umb_seg = 0xd000;
uint16_t first_umb_size = 0x2000;

static uint16_t memAllocStrategy = 0x00;

static void DOS_Mem_E_Exit(const char *msg) {
	uint16_t mcb_segment=dos.firstMCB;
	DOS_MCB mcb(mcb_segment);
	DOS_MCB mcb_next(0);
	Bitu counter=0;
	char name[10];
	char c;

	LOG_MSG("DOS MCB dump:\n");
	while ((c=(char)mcb.GetType()) != 'Z') {
		if (counter++ > 10000) break;
		if (c != 'M') break;

		mcb.GetFileName(name);
		LOG_MSG(" Type=0x%02x(%c) Seg=0x%04x size=0x%04x name='%s'\n",
			mcb.GetType(),c,
			mcb_segment+1,mcb.GetSize(),name);
		mcb_next.SetPt((uint16_t)(mcb_segment+mcb.GetSize()+1));
		mcb_segment+=mcb.GetSize()+1;
		mcb.SetPt(mcb_segment);
	}

	mcb.GetFileName(name);
	c = (char)mcb.GetType(); if (c < 32) c = '.';
	LOG_MSG("FINAL: Type=0x%02x(%c) Seg=0x%04x size=0x%04x name='%s'\n",
		mcb.GetType(),c,mcb_segment+1,mcb.GetSize(),name);
	LOG_MSG("End dump\n");

#if C_DEBUG
    LOG_MSG("DOS fatal memory error: %s",msg);
    throw int(7); // DOS non-fatal error (restart when debugger runs again)
#else
	E_Exit("%s",msg);
#endif
}

void DOS_CompressMemory(uint16_t first_segment=0/*default*/) {
	uint16_t mcb_segment=dos.firstMCB;
	DOS_MCB mcb(mcb_segment);
	DOS_MCB mcb_next(0);
	Bitu counter=0;

	while (mcb.GetType()!='Z') {
		if(counter++ > 10000000) DOS_Mem_E_Exit("DOS_CompressMemory: DOS MCB list corrupted.");
		mcb_next.SetPt((uint16_t)(mcb_segment+mcb.GetSize()+1));
		if (GCC_UNLIKELY((mcb_next.GetType()!=0x4d) && (mcb_next.GetType()!=0x5a))) DOS_Mem_E_Exit("Corrupt MCB chain");
		if (mcb_segment >= first_segment && (mcb.GetPSPSeg()==MCB_FREE) && (mcb_next.GetPSPSeg()==MCB_FREE)) {
			mcb.SetSize(mcb.GetSize()+mcb_next.GetSize()+1);
			mcb.SetType(mcb_next.GetType());
		} else {
			mcb_segment+=mcb.GetSize()+1;
			mcb.SetPt(mcb_segment);
		}
	}
}

void DOS_FreeProcessMemory(uint16_t pspseg) {
	uint16_t mcb_segment=dos.firstMCB;
	DOS_MCB mcb(mcb_segment);
	Bitu counter = 0;

	for (;;) {
		if(counter++ > 10000000) DOS_Mem_E_Exit("DOS_FreeProcessMemory: DOS MCB list corrupted.");
		if (mcb.GetPSPSeg()==pspseg) {
			mcb.SetPSPSeg(MCB_FREE);
		}
		if (mcb.GetType()==0x5a) break;
		if (GCC_UNLIKELY(mcb.GetType()!=0x4d)) DOS_Mem_E_Exit("Corrupt MCB chain");
		mcb_segment+=mcb.GetSize()+1;
		mcb.SetPt(mcb_segment);
	}

	uint16_t umb_start=dos_infoblock.GetStartOfUMBChain();
	if (umb_start==UMB_START_SEG) {
		DOS_MCB umb_mcb(umb_start);
		for (;;) {
			if (umb_mcb.GetPSPSeg()==pspseg) {
				umb_mcb.SetPSPSeg(MCB_FREE);
			}
			if (umb_mcb.GetType()!=0x4d) break;
			umb_start+=umb_mcb.GetSize()+1;
			umb_mcb.SetPt(umb_start);
		}
	} else if (umb_start!=0xffff) LOG(LOG_DOSMISC,LOG_ERROR)("Corrupt UMB chain: %x",umb_start);

	DOS_CompressMemory();
}

uint16_t DOS_GetMemAllocStrategy() {
	return memAllocStrategy;
}

bool DOS_SetMemAllocStrategy(uint16_t strat) {
	if ((strat&0x3f)<3) {
		memAllocStrategy = strat;
		return true;
	}
	/* otherwise an invalid allocation strategy was specified */
	return false;
}

extern bool dbg_zero_on_dos_allocmem;

void DOS_zeromem(uint16_t seg,uint16_t para) {
	uint32_t ofs,cnt;

	if (para == 0) return;

	ofs = ((uint32_t)seg << 4);
	cnt = ((uint32_t)para << 4);
	if ((ofs+cnt) > 0x100000) E_Exit("DOS_zeromem out of range");
	while (cnt != 0) {
		mem_writeb(ofs++,0);
		cnt--;
	}
}

bool DOS_AllocateMemory(uint16_t * segment,uint16_t * blocks) {
	DOS_CompressMemory();
	uint16_t bigsize=0;
	uint16_t mem_strat=memAllocStrategy;
	uint16_t mcb_segment=dos.firstMCB;

	uint16_t umb_start=dos_infoblock.GetStartOfUMBChain();
	if (umb_start==UMB_START_SEG) {
		/* start with UMBs if requested (bits 7 or 6 set) */
		if (mem_strat&0xc0) mcb_segment=umb_start;
	} else if (umb_start!=0xffff) LOG(LOG_DOSMISC,LOG_ERROR)("Corrupt UMB chain: %x",umb_start);

	DOS_MCB mcb(0);
	DOS_MCB mcb_next(0);
	DOS_MCB psp_mcb(dos.psp()-1);
	char psp_name[9];
	psp_mcb.GetFileName(psp_name);
	uint16_t found_seg=0,found_seg_size=0;
	for (;;) {
		mcb.SetPt(mcb_segment);
		if (mcb.GetPSPSeg()==MCB_FREE) {
			/* Check for enough free memory in current block */
			uint16_t block_size=mcb.GetSize();			
			if (block_size<(*blocks)) {
				if (bigsize<block_size) {
					/* current block is largest block that was found,
					   but still not as big as requested */
					bigsize=block_size;
				}
			} else if ((block_size==*blocks) && ((mem_strat & 0x3f)<2)) {
				/* MCB fits precisely, use it if search strategy is firstfit or bestfit */
				mcb.SetPSPSeg(dos.psp());
				*segment=mcb_segment+1;

				if (dbg_zero_on_dos_allocmem) DOS_zeromem(*segment,*blocks);
#ifdef DEBUG_ALLOC
				LOG(LOG_DOSMISC,LOG_DEBUG)("DOS_AllocateMemory(blocks=0x%04x) = 0x%04x-0x%04x",*blocks,*segment,*segment+*blocks-1);
#endif
				return true;
			} else {
				switch (mem_strat & 0x3f) {
					case 0: /* firstfit */
						mcb_next.SetPt((uint16_t)(mcb_segment+*blocks+1));
						mcb_next.SetPSPSeg(MCB_FREE);
						mcb_next.SetType(mcb.GetType());
						mcb_next.SetSize(block_size-*blocks-1);
						mcb.SetSize(*blocks);
						mcb.SetType(0x4d);		
						mcb.SetPSPSeg(dos.psp());
						mcb.SetFileName(psp_name);
						//TODO Filename
						*segment=mcb_segment+1;

						if (dbg_zero_on_dos_allocmem) DOS_zeromem(*segment,*blocks);
#ifdef DEBUG_ALLOC
						LOG(LOG_DOSMISC,LOG_DEBUG)("DOS_AllocateMemory(blocks=0x%04x) = 0x%04x-0x%04x",*blocks,*segment,*segment+*blocks-1);
#endif
						return true;
					case 1: /* bestfit */
						if ((found_seg_size==0) || (block_size<found_seg_size)) {
							/* first fitting MCB, or smaller than the last that was found */
							found_seg=mcb_segment;
							found_seg_size=block_size;
						}
						break;
					default: /* everything else is handled as lastfit by dos */
						/* MCB is large enough, note it down */
						found_seg=mcb_segment;
						found_seg_size=block_size;
						break;
				}
			}
		}
		/* Onward to the next MCB if there is one */
		if (mcb.GetType()==0x5a) {
			if ((mem_strat&0x80) && (umb_start==UMB_START_SEG)) {
				/* bit 7 set: try high memory first, then low */
				mcb_segment=dos.firstMCB;
				mem_strat&=(~0xc0);
			} else {
				/* finished searching all requested MCB chains */
				if (found_seg) {
					/* a matching MCB was found (cannot occur for firstfit) */
					if ((mem_strat & 0x3f)==0x01) {
						/* bestfit, allocate block at the beginning of the MCB */
						mcb.SetPt(found_seg);

						mcb_next.SetPt((uint16_t)(found_seg+*blocks+1));
						mcb_next.SetPSPSeg(MCB_FREE);
						mcb_next.SetType(mcb.GetType());
						mcb_next.SetSize(found_seg_size-*blocks-1);

						mcb.SetSize(*blocks);
						mcb.SetType(0x4d);		
						mcb.SetPSPSeg(dos.psp());
						mcb.SetFileName(psp_name);
						//TODO Filename
						*segment=found_seg+1;
					} else {
						/* lastfit, allocate block at the end of the MCB */
						mcb.SetPt(found_seg);
						if (found_seg_size==*blocks) {
							/* use the whole block */
							mcb.SetPSPSeg(dos.psp());
							//Not consistent with line 124. But how many application will use this information ?
							mcb.SetFileName(psp_name);
							*segment = found_seg+1;

							if (dbg_zero_on_dos_allocmem) DOS_zeromem(*segment,*blocks);
#ifdef DEBUG_ALLOC
							LOG(LOG_DOSMISC,LOG_DEBUG)("DOS_AllocateMemory(blocks=0x%04x) = 0x%04x-0x%04x",*blocks,*segment,*segment+*blocks-1);
#endif
							return true;
						}
						*segment = found_seg+1+found_seg_size - *blocks;
						mcb_next.SetPt((uint16_t)(*segment-1));
						mcb_next.SetSize(*blocks);
						mcb_next.SetType(mcb.GetType());
						mcb_next.SetPSPSeg(dos.psp());
						mcb_next.SetFileName(psp_name);
						// Old Block
						mcb.SetSize(found_seg_size-*blocks-1);
						mcb.SetPSPSeg(MCB_FREE);
						mcb.SetType(0x4D);
					}

					if (dbg_zero_on_dos_allocmem) DOS_zeromem(*segment,*blocks);
#ifdef DEBUG_ALLOC
					LOG(LOG_DOSMISC,LOG_DEBUG)("DOS_AllocateMemory(blocks=0x%04x) = 0x%04x-0x%04x",*blocks,*segment,*segment+*blocks-1);
#endif
					return true;
				}
				/* no fitting MCB found, return size of largest block */
				*blocks=bigsize;
				DOS_SetError(DOSERR_INSUFFICIENT_MEMORY);
				return false;
			}
		} else mcb_segment+=mcb.GetSize()+1;
	}

#ifdef DEBUG_ALLOC
	LOG(LOG_DOSMISC,LOG_DEBUG)("DOS_AllocateMemory(blocks=0x%04x) = 0x%04x-0x%04x",*blocks,*segment,*segment+*blocks-1);
#endif
	return false;
}


bool DOS_ResizeMemory(uint16_t segment,uint16_t * blocks) {
	if (segment < DOS_MEM_START+1) {
		LOG(LOG_DOSMISC,LOG_ERROR)("Program resizes %X, take care",segment);
	}

#ifdef DEBUG_ALLOC
	LOG(LOG_DOSMISC,LOG_DEBUG)("DOS_ResizeMemory(seg=0x%04x) blocks=0x%04x",segment,*blocks);
#endif

	DOS_MCB mcb(segment-1);
	if ((mcb.GetType()!=0x4d) && (mcb.GetType()!=0x5a)) {
		DOS_SetError(DOSERR_MCB_DESTROYED);
		return false;
	}

	uint16_t total=mcb.GetSize();
	DOS_MCB	mcb_next(segment+total);

	if (*blocks > total)
		DOS_CompressMemory(segment-1);
	else
		DOS_CompressMemory();

	if (*blocks<=total) {
		if (GCC_UNLIKELY(*blocks==total)) {
			/* Nothing to do */
			return true;
		}
		/* Shrinking MCB */
		DOS_MCB	mcb_new_next(segment+(*blocks));
		mcb.SetSize(*blocks);
		mcb_new_next.SetType(mcb.GetType());
		if (mcb.GetType()==0x5a) {
			/* Further blocks follow */
			mcb.SetType(0x4d);
		}

		mcb_new_next.SetSize(total-*blocks-1);
		mcb_new_next.SetPSPSeg(MCB_FREE);
		mcb.SetPSPSeg(dos.psp());
		DOS_CompressMemory();
		return true;
	}
	/* MCB will grow, try to join with following MCB */
	if (mcb.GetType()!=0x5a) {
		if (mcb_next.GetPSPSeg()==MCB_FREE) {
			total+=mcb_next.GetSize()+1;
		}
	}
	if (*blocks<total) {
		if (mcb.GetType()!=0x5a) {
			/* save type of following MCB */
			mcb.SetType(mcb_next.GetType());
		}
		mcb.SetSize(*blocks);
		mcb_next.SetPt((uint16_t)(segment+*blocks));
		mcb_next.SetSize(total-*blocks-1);
		mcb_next.SetType(mcb.GetType());
		mcb_next.SetPSPSeg(MCB_FREE);
		mcb.SetType(0x4d);
		mcb.SetPSPSeg(dos.psp());
		return true;
	}

	/* at this point: *blocks==total (fits) or *blocks>total,
	   in the second case resize block to maximum */

	if ((mcb_next.GetPSPSeg()==MCB_FREE) && (mcb.GetType()!=0x5a)) {
		/* adjust type of joined MCB */
		mcb.SetType(mcb_next.GetType());
	}
	mcb.SetSize(total);
	mcb.SetPSPSeg(dos.psp());
	if (*blocks==total) return true;	/* block fit exactly */

	*blocks=total;	/* return maximum */
	DOS_SetError(DOSERR_INSUFFICIENT_MEMORY);
	return false;
}


bool DOS_FreeMemory(uint16_t segment) {
//TODO Check if allowed to free this segment
	if (segment < DOS_MEM_START+1) {
		LOG(LOG_DOSMISC,LOG_ERROR)("Program tried to free %X ---ERROR",segment);
		DOS_SetError(DOSERR_MB_ADDRESS_INVALID);
		return false;
	}
      
	DOS_MCB mcb(segment-1);
	if ((mcb.GetType()!=0x4d) && (mcb.GetType()!=0x5a)) {
		DOS_SetError(DOSERR_MB_ADDRESS_INVALID);
		return false;
	}

#ifdef DEBUG_ALLOC
	LOG(LOG_DOSMISC,LOG_DEBUG)("DOS_FreeMemory(seg=0x%04x)",segment);
#endif

	mcb.SetPSPSeg(MCB_FREE);
//	DOS_CompressMemory();
	return true;
}

Bitu GetEMSPageFrameSegment(void);

void DOS_BuildUMBChain(bool umb_active,bool /*ems_active*/) {
	unsigned int seg_limit = (unsigned int)(MEM_TotalPages()*256);

	/* UMBs are only possible if the machine has 1MB+64KB of RAM */
	if (umb_active && (machine!=MCH_TANDY) && seg_limit >= (0x10000+0x1000-1) && first_umb_seg < GetEMSPageFrameSegment()) {
        /* XMS emulation sets UMB size now.
         * PCjr mode disables UMB emulation */
#if 0
		if (ems_active) {
			/* we can use UMBs up to the EMS page frame */
			/* FIXME: when we make the EMS page frame configurable this will need to be updated */
			first_umb_size = GetEMSPageFrameSegment() - first_umb_seg;
		}
		else if (machine == MCH_PCJR) {
			/* we can use UMBs up to where PCjr wants cartridge ROM */
			first_umb_size = 0xE000 - first_umb_seg;
		}
#endif

		dos_infoblock.SetStartOfUMBChain((uint16_t)UMB_START_SEG);
		dos_infoblock.SetUMBChainState(0);		// UMBs not linked yet

		DOS_MCB umb_mcb(first_umb_seg);
		umb_mcb.SetPSPSeg(0);		// currently free
		umb_mcb.SetSize(first_umb_size-1);
		umb_mcb.SetType(0x5a);

		/* Scan MCB-chain for last block */
		uint16_t mcb_segment=dos.firstMCB;
		DOS_MCB mcb(mcb_segment);
		while (mcb.GetType()!=0x5a) {
			mcb_segment+=mcb.GetSize()+1;
			mcb.SetPt(mcb_segment);
		}

		/* A system MCB has to cover the space between the
		   regular MCB-chain and the UMBs */
		uint16_t cover_mcb=(uint16_t)(mcb_segment+mcb.GetSize()+1);
		mcb.SetPt(cover_mcb);
		mcb.SetType(0x4d);
		mcb.SetPSPSeg(0x0008);
		mcb.SetSize(first_umb_seg-cover_mcb-1);
		mcb.SetFileName("SC      ");

	} else {
		dos_infoblock.SetStartOfUMBChain(0xffff);
		dos_infoblock.SetUMBChainState(0);
	}
}

bool DOS_LinkUMBsToMemChain(uint16_t linkstate) {
	/* Get start of UMB-chain */
	uint16_t umb_start=dos_infoblock.GetStartOfUMBChain();
	if (umb_start!=UMB_START_SEG) {
		if (umb_start!=0xffff) LOG(LOG_DOSMISC,LOG_ERROR)("Corrupt UMB chain: %x",umb_start);
		return false;
	}

	if ((linkstate&1)==(dos_infoblock.GetUMBChainState()&1)) return true;
	
	/* Scan MCB-chain for last block before UMB-chain */
	uint16_t mcb_segment=dos.firstMCB;
	uint16_t prev_mcb_segment=dos.firstMCB;
	DOS_MCB mcb(mcb_segment);
	while ((mcb_segment!=umb_start) && (mcb.GetType()!=0x5a)) {
		prev_mcb_segment=mcb_segment;
		mcb_segment+=mcb.GetSize()+1;
		mcb.SetPt(mcb_segment);
	}
	DOS_MCB prev_mcb(prev_mcb_segment);

	switch (linkstate) {
		case 0x0000:	// unlink
			if ((prev_mcb.GetType()==0x4d) && (mcb_segment==umb_start)) {
				prev_mcb.SetType(0x5a);
			}
			dos_infoblock.SetUMBChainState(0);
			break;
		case 0x0001:	// link
			if (mcb.GetType()==0x5a) {
				if ((mcb_segment+mcb.GetSize()+1) != umb_start) {
					LOG_MSG("MCB chain no longer goes to end of memory (corruption?), not linking in UMB!");
					return false;
				}
				mcb.SetType(0x4d);
				dos_infoblock.SetUMBChainState(1);
			}
			break;
		default:
			/* NTS: Some programs apparently call this function incorrectly.
			 *      The CauseWay extender in Open Watcom 1.9's installer for example
			 *      calls this function with AX=0x5803 BX=0x58 */
			return false;
	}

	return true;
}


#include <assert.h>

extern uint16_t DOS_IHSEG;

extern bool enable_dummy_device_mcb;

void DOS_SetupMemory(void) {
	unsigned int max_conv;
	unsigned int seg_limit;

	max_conv = (unsigned int)mem_readw(BIOS_MEMORY_SIZE) << (10u - 4u);
	seg_limit = (unsigned int)(MEM_TotalPages()*256);
	if (seg_limit > max_conv) seg_limit = max_conv;
	UMB_START_SEG = max_conv - 1;

	/* Let dos claim a few bios interrupts. Makes DOSBox more compatible with 
	 * buggy games, which compare against the interrupt table. (probably a 
	 * broken linked list implementation) */
	uint16_t ihseg;
	uint16_t ihofs;

	assert(DOS_IHSEG != 0);
	ihseg = DOS_IHSEG;
	ihofs = 0xF4;

	real_writeb(ihseg,ihofs,(uint8_t)0xCF);		//An IRET Instruction
	RealSetVec(0x01,RealMake(ihseg,ihofs));		//BioMenace (offset!=4)
	if (machine != MCH_PCJR) RealSetVec(0x02,RealMake(ihseg,ihofs)); //BioMenace (segment<0x8000). Else, taken by BIOS NMI interrupt
	RealSetVec(0x03,RealMake(ihseg,ihofs));		//Alien Incident (offset!=0)
	RealSetVec(0x04,RealMake(ihseg,ihofs));		//Shadow President (lower byte of segment!=0)
	RealSetVec(0x0f,RealMake(ihseg,ihofs));		//Always a tricky one (soundblaster irq)

	uint16_t mcb_sizes=0;

	if (enable_dummy_device_mcb) {
		// Create a dummy device MCB with PSPSeg=0x0008
        LOG_MSG("Dummy device MCB at segment 0x%x",DOS_MEM_START+mcb_sizes);
		DOS_MCB mcb_devicedummy((uint16_t)DOS_MEM_START+mcb_sizes);
		mcb_devicedummy.SetPSPSeg(MCB_DOS);	// Devices
		mcb_devicedummy.SetSize(16);
		mcb_devicedummy.SetType(0x4d);		// More blocks will follow
		mcb_sizes+=1+16;

// We DO need to mark this area as 'SD' but leaving it blank so far
// confuses MEM.EXE (shows ???????) which suggests other software
// might have a problem with it as well.
//		mcb_devicedummy.SetFileName("SD      ");
	}

	DOS_MCB mcb((uint16_t)DOS_MEM_START+mcb_sizes);
	mcb.SetPSPSeg(MCB_FREE);						//Free
	mcb.SetType(0x5a);								//Last Block
	if (machine==MCH_TANDY) {
		if (seg_limit < ((384*1024)/16))
			E_Exit("Tandy requires at least 384K");

		/* map memory as normal, the BIOS initialization is the code responsible
		 * for subtracting 32KB from top of system memory for video memory. */
		mcb.SetSize(/*normally 0x97FF*/(seg_limit-1) - DOS_MEM_START - mcb_sizes);
	} else if (machine==MCH_PCJR) {
		DOS_MCB mcb_devicedummy((uint16_t)0x2000);

        /* FIXME: The PCjr can have built-in either 64KB or 128KB of RAM.
         *        RAM beyond 128KB is made possible with expansion sidecars.
         *        DOSBox-X needs to support memsizekb=64 or memsizekb=128,
         *        and adjust video ram location appropriately. */

		if (seg_limit < ((256*1024)/16))
			E_Exit("PCjr requires at least 256K");
		/* memory from 128k to 640k is available */
		mcb_devicedummy.SetPt((uint16_t)0x2000);
		mcb_devicedummy.SetPSPSeg(MCB_FREE);
		mcb_devicedummy.SetSize(/*0x9FFF*/(seg_limit-1) - 0x2000);
		mcb_devicedummy.SetType(0x5a);

		/* exclude PCJr graphics region */
		mcb_devicedummy.SetPt((uint16_t)0x17ff);
		mcb_devicedummy.SetPSPSeg(MCB_DOS);
		mcb_devicedummy.SetSize(0x800);
		mcb_devicedummy.SetType(0x4d);

		/* memory below 96k */
		mcb.SetSize(0x1800 - DOS_MEM_START - (2+mcb_sizes));
		mcb.SetType(0x4d);
	} else {
#ifndef DEBUG_ALLOC
		/* NTS: Testing suggests we can push as low as 4KB. However, Wikipedia and
		 *      other sites suggest that the IBM PC only went as low as 16KB when
		 *      it first sold, and even that wasn't too typical. But what the hell,
		 *      we'll allow as little as 4KB if not for fun DOS hacking. */
		if (seg_limit < ((4*1024)/16))
			E_Exit("Standard PC requires at least 4K");
#endif

		/* complete memory up to 640k available */
		/* last paragraph used to add UMB chain to low-memory MCB chain */
		mcb.SetSize(/*0x9FFE*/(seg_limit-2) - DOS_MEM_START - mcb_sizes);
	}

	dos.firstMCB=DOS_MEM_START;
	dos_infoblock.SetFirstMCB(DOS_MEM_START);
}

// save state support
void POD_Save_DOS_Memory( std::ostream& stream )
{
	// - pure data
	WRITE_POD( &memAllocStrategy, memAllocStrategy );
}

void POD_Load_DOS_Memory( std::istream& stream )
{
	// - pure data
	READ_POD( &memAllocStrategy, memAllocStrategy );
}
