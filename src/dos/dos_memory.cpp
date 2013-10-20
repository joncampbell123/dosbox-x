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


#include "dosbox.h"
#include "mem.h"
#include "dos_inc.h"
#include "callback.h"

#define UMB_START_SEG 0x9fff

static Bit16u memAllocStrategy = 0x00;

static void DOS_CompressMemory(void) {
	Bit16u mcb_segment=dos.firstMCB;
	DOS_MCB mcb(mcb_segment);
	DOS_MCB mcb_next(0);
	Bitu counter=0;

	while (mcb.GetType()!='Z') {
		if(counter++ > 10000000) E_Exit("DOS MCB list corrupted.");
		mcb_next.SetPt((Bit16u)(mcb_segment+mcb.GetSize()+1));
		if ((mcb.GetPSPSeg()==0) && (mcb_next.GetPSPSeg()==0)) {
			mcb.SetSize(mcb.GetSize()+mcb_next.GetSize()+1);
			mcb.SetType(mcb_next.GetType());
		} else {
			mcb_segment+=mcb.GetSize()+1;
			mcb.SetPt(mcb_segment);
		}
	}
}

void DOS_FreeProcessMemory(Bit16u pspseg) {
	Bit16u mcb_segment=dos.firstMCB;
	DOS_MCB mcb(mcb_segment);
	Bitu counter = 0;
	for (;;) {
		if(counter++ > 10000000) E_Exit("DOS MCB list corrupted.");
		if (mcb.GetPSPSeg()==pspseg) {
			mcb.SetPSPSeg(MCB_FREE);
		}
		if (mcb.GetType()==0x5a) {
			/* check if currently last block reaches up to the PCJr graphics memory */
			if ((machine==MCH_PCJR) && (mcb_segment+mcb.GetSize()==0x17fe) &&
			   (real_readb(0x17ff,0)==0x4d) && (real_readw(0x17ff,1)==8)) {
				/* re-enable the memory past segment 0x2000 */
				mcb.SetType(0x4d);
			} else break;
		}
		mcb_segment+=mcb.GetSize()+1;
		mcb.SetPt(mcb_segment);
	}

	Bit16u umb_start=dos_infoblock.GetStartOfUMBChain();
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

Bit16u DOS_GetMemAllocStrategy() {
	return memAllocStrategy;
}

bool DOS_SetMemAllocStrategy(Bit16u strat) {
	if ((strat&0x3f)<3) {
		memAllocStrategy = strat;
		return true;
	}
	/* otherwise an invalid allocation strategy was specified */
	return false;
}

bool DOS_AllocateMemory(Bit16u * segment,Bit16u * blocks) {
	DOS_CompressMemory();
	Bit16u bigsize=0;
	Bit16u mem_strat=memAllocStrategy;
	Bit16u mcb_segment=dos.firstMCB;

	Bit16u umb_start=dos_infoblock.GetStartOfUMBChain();
	if (umb_start==UMB_START_SEG) {
		/* start with UMBs if requested (bits 7 or 6 set) */
		if (mem_strat&0xc0) mcb_segment=umb_start;
	} else if (umb_start!=0xffff) LOG(LOG_DOSMISC,LOG_ERROR)("Corrupt UMB chain: %x",umb_start);

	DOS_MCB mcb(0);
	DOS_MCB mcb_next(0);
	DOS_MCB psp_mcb(dos.psp()-1);
	char psp_name[9];
	psp_mcb.GetFileName(psp_name);
	Bit16u found_seg=0,found_seg_size=0;
	for (;;) {
		mcb.SetPt(mcb_segment);
		if (mcb.GetPSPSeg()==0) {
			/* Check for enough free memory in current block */
			Bit16u block_size=mcb.GetSize();			
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
				return true;
			} else {
				switch (mem_strat & 0x3f) {
					case 0: /* firstfit */
						mcb_next.SetPt((Bit16u)(mcb_segment+*blocks+1));
						mcb_next.SetPSPSeg(MCB_FREE);
						mcb_next.SetType(mcb.GetType());
						mcb_next.SetSize(block_size-*blocks-1);
						mcb.SetSize(*blocks);
						mcb.SetType(0x4d);		
						mcb.SetPSPSeg(dos.psp());
						mcb.SetFileName(psp_name);
						//TODO Filename
						*segment=mcb_segment+1;
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

						mcb_next.SetPt((Bit16u)(found_seg+*blocks+1));
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
							return true;
						}
						*segment = found_seg+1+found_seg_size - *blocks;
						mcb_next.SetPt((Bit16u)(*segment-1));
						mcb_next.SetSize(*blocks);
						mcb_next.SetType(mcb.GetType());
						mcb_next.SetPSPSeg(dos.psp());
						mcb_next.SetFileName(psp_name);
						// Old Block
						mcb.SetSize(found_seg_size-*blocks-1);
						mcb.SetPSPSeg(MCB_FREE);
						mcb.SetType(0x4D);
					}
					return true;
				}
				/* no fitting MCB found, return size of largest block */
				*blocks=bigsize;
				DOS_SetError(DOSERR_INSUFFICIENT_MEMORY);
				return false;
			}
		} else mcb_segment+=mcb.GetSize()+1;
	}
	return false;
}


bool DOS_ResizeMemory(Bit16u segment,Bit16u * blocks) {
	if (segment < DOS_MEM_START+1) {
		LOG(LOG_DOSMISC,LOG_ERROR)("Program resizes %X, take care",segment);
	}
      
	DOS_MCB mcb(segment-1);
	if ((mcb.GetType()!=0x4d) && (mcb.GetType()!=0x5a)) {
		DOS_SetError(DOSERR_MCB_DESTROYED);
		return false;
	}

	DOS_CompressMemory();
	Bit16u total=mcb.GetSize();
	DOS_MCB	mcb_next(segment+total);
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
		mcb_next.SetPt((Bit16u)(segment+*blocks));
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


bool DOS_FreeMemory(Bit16u segment) {
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
	mcb.SetPSPSeg(MCB_FREE);
//	DOS_CompressMemory();
	return true;
}


void DOS_BuildUMBChain(bool umb_active,bool ems_active) {
	unsigned int seg_limit = MEM_TotalPages()*256;

	/* UMBs are only possible if the machine has 1MB+64KB of RAM */
	if (umb_active && (machine!=MCH_TANDY) && seg_limit >= (0x10000+0x1000-1)) {
		Bit16u first_umb_seg = 0xd000;
		Bit16u first_umb_size = 0x2000;
		if(ems_active || (machine == MCH_PCJR)) first_umb_size = 0x1000;

		dos_infoblock.SetStartOfUMBChain(UMB_START_SEG);
		dos_infoblock.SetUMBChainState(0);		// UMBs not linked yet

		DOS_MCB umb_mcb(first_umb_seg);
		umb_mcb.SetPSPSeg(0);		// currently free
		umb_mcb.SetSize(first_umb_size-1);
		umb_mcb.SetType(0x5a);

		/* Scan MCB-chain for last block */
		Bit16u mcb_segment=dos.firstMCB;
		DOS_MCB mcb(mcb_segment);
		while (mcb.GetType()!=0x5a) {
			mcb_segment+=mcb.GetSize()+1;
			mcb.SetPt(mcb_segment);
		}

		/* A system MCB has to cover the space between the
		   regular MCB-chain and the UMBs */
		Bit16u cover_mcb=(Bit16u)(mcb_segment+mcb.GetSize()+1);
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

bool DOS_LinkUMBsToMemChain(Bit16u linkstate) {
	/* Get start of UMB-chain */
	Bit16u umb_start=dos_infoblock.GetStartOfUMBChain();
	if (umb_start!=UMB_START_SEG) {
		if (umb_start!=0xffff) LOG(LOG_DOSMISC,LOG_ERROR)("Corrupt UMB chain: %x",umb_start);
		return false;
	}

	if ((linkstate&1)==(dos_infoblock.GetUMBChainState()&1)) return true;
	
	/* Scan MCB-chain for last block before UMB-chain */
	Bit16u mcb_segment=dos.firstMCB;
	Bit16u prev_mcb_segment=dos.firstMCB;
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
			LOG_MSG("Invalid link state %x when reconfiguring MCB chain",linkstate);
			return false;
	}

	return true;
}


static Bitu DOS_default_handler(void) {
	LOG(LOG_CPU,LOG_ERROR)("DOS rerouted Interrupt Called %X",lastint);
	return CBRET_NONE;
}

static	CALLBACK_HandlerObject callbackhandler;
void DOS_SetupMemory(void) {
	unsigned int seg_limit = MEM_TotalPages()*256;
	if (seg_limit > 0xA000) seg_limit = 0xA000;

	/* Let dos claim a few bios interrupts. Makes DOSBox more compatible with 
	 * buggy games, which compare against the interrupt table. (probably a 
	 * broken linked list implementation) */
	callbackhandler.Allocate(&DOS_default_handler,"DOS default int");
	Bit16u ihseg = 0x70;
	Bit16u ihofs = 0x08;
	real_writeb(ihseg,ihofs+0x00,(Bit8u)0xFE);	//GRP 4
	real_writeb(ihseg,ihofs+0x01,(Bit8u)0x38);	//Extra Callback instruction
	real_writew(ihseg,ihofs+0x02,callbackhandler.Get_callback());  //The immediate word
	real_writeb(ihseg,ihofs+0x04,(Bit8u)0xCF);	//An IRET Instruction
	RealSetVec(0x01,RealMake(ihseg,ihofs));		//BioMenace (offset!=4)
	RealSetVec(0x02,RealMake(ihseg,ihofs));		//BioMenace (segment<0x8000)
	RealSetVec(0x03,RealMake(ihseg,ihofs));		//Alien Incident (offset!=0)
	RealSetVec(0x04,RealMake(ihseg,ihofs));		//Shadow President (lower byte of segment!=0)
//	RealSetVec(0x0f,RealMake(ihseg,ihofs));		//Always a tricky one (soundblaster irq)

	// Create a dummy device MCB with PSPSeg=0x0008
	DOS_MCB mcb_devicedummy((Bit16u)DOS_MEM_START);
	mcb_devicedummy.SetPSPSeg(MCB_DOS);	// Devices
	mcb_devicedummy.SetSize(1);
	mcb_devicedummy.SetType(0x4d);		// More blocks will follow
//	mcb_devicedummy.SetFileName("SD      ");

	Bit16u mcb_sizes=2;
	// Create a small empty MCB (result from a growing environment block)
	DOS_MCB tempmcb((Bit16u)DOS_MEM_START+mcb_sizes);
	tempmcb.SetPSPSeg(MCB_FREE);
	tempmcb.SetSize(4);
	mcb_sizes+=5;
	tempmcb.SetType(0x4d);

	// Lock the previous empty MCB
	DOS_MCB tempmcb2((Bit16u)DOS_MEM_START+mcb_sizes);
	tempmcb2.SetPSPSeg(0x40);	// can be removed by loadfix
	tempmcb2.SetSize(16);
	mcb_sizes+=17;
	tempmcb2.SetType(0x4d);

	DOS_MCB mcb((Bit16u)DOS_MEM_START+mcb_sizes);
	mcb.SetPSPSeg(MCB_FREE);						//Free
	mcb.SetType(0x5a);								//Last Block
	if (machine==MCH_TANDY) {
		if (seg_limit < ((384*1024)/16))
			E_Exit("Tandy requires at least 384K");
		/* memory up to 608k available, the rest (to 640k) is used by
			the tandy graphics system's variable mapping of 0xb800 */
/*
		mcb.SetSize(0x9BFF - DOS_MEM_START - mcb_sizes);
*/		mcb.SetSize(/*0x9BFF*/(seg_limit-0x801) - DOS_MEM_START - mcb_sizes);
	} else if (machine==MCH_PCJR) {
		if (seg_limit < ((256*1024)/16))
			E_Exit("PCjr requires at least 256K");
		/* memory from 128k to 640k is available */
		mcb_devicedummy.SetPt((Bit16u)0x2000);
		mcb_devicedummy.SetPSPSeg(MCB_FREE);
		mcb_devicedummy.SetSize(/*0x9FFF*/(seg_limit-1) - 0x2000);
		mcb_devicedummy.SetType(0x5a);

		/* exclude PCJr graphics region */
		mcb_devicedummy.SetPt((Bit16u)0x17ff);
		mcb_devicedummy.SetPSPSeg(MCB_DOS);
		mcb_devicedummy.SetSize(0x800);
		mcb_devicedummy.SetType(0x4d);

		/* memory below 96k */
		mcb.SetSize(0x1800 - DOS_MEM_START - (2+mcb_sizes));
		mcb.SetType(0x4d);
	} else {
		if (seg_limit < ((192*1024)/16))
			E_Exit("DOS requires at least 192K");

		/* complete memory up to 640k available */
		/* last paragraph used to add UMB chain to low-memory MCB chain */
		mcb.SetSize(/*0x9FFE*/(seg_limit-2) - DOS_MEM_START - mcb_sizes);
	}

	dos.firstMCB=DOS_MEM_START;
	dos_infoblock.SetFirstMCB(DOS_MEM_START);
}

