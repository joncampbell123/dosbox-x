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

/* $Id: dma.cpp,v 1.41 2009-07-24 09:56:14 c2woody Exp $ */

#include <string.h>
#include "dosbox.h"
#include "mem.h"
#include "inout.h"
#include "dma.h"
#include "pic.h"
#include "paging.h"
#include "setup.h"

DmaController *DmaControllers[2];

#define EMM_PAGEFRAME4K	((0xE000*16)/4096)
Bit32u ems_board_mapping[LINK_START];

static void UpdateEMSMapping(void) {
	/* if EMS is not present, this will result in a 1:1 mapping */
	Bitu i;
	for (i=0;i<0x10;i++) {
		ems_board_mapping[EMM_PAGEFRAME4K+i]=paging.firstmb[EMM_PAGEFRAME4K+i];
	}
}

/* read a block from physical memory */
static void DMA_BlockRead(PhysPt spage,PhysPt offset,void * data,Bitu size,Bit8u dma16) {
	Bit8u * write=(Bit8u *) data;
	Bitu highpart_addr_page = spage>>12;
	size <<= dma16;
	offset <<= dma16;
	Bit32u dma_wrap = ((0xffff<<dma16)+dma16) | dma_wrapping;
	for ( ; size ; size--, offset++) {
		if (offset>(dma_wrapping<<dma16)) E_Exit("DMA segbound wrapping (read)");
		offset &= dma_wrap;
		Bitu page = highpart_addr_page+(offset >> 12);
		/* care for EMS pageframe etc. */
		if (page < EMM_PAGEFRAME4K) page = paging.firstmb[page];
		else if (page < EMM_PAGEFRAME4K+0x10) page = ems_board_mapping[page];
		else if (page < LINK_START) page = paging.firstmb[page];
		*write++=phys_readb(page*4096 + (offset & 4095));
	}
}

/* write a block into physical memory */
static void DMA_BlockWrite(PhysPt spage,PhysPt offset,void * data,Bitu size,Bit8u dma16) {
	Bit8u * read=(Bit8u *) data;
	Bitu highpart_addr_page = spage>>12;
	size <<= dma16;
	offset <<= dma16;
	Bit32u dma_wrap = ((0xffff<<dma16)+dma16) | dma_wrapping;
	for ( ; size ; size--, offset++) {
		if (offset>(dma_wrapping<<dma16)) E_Exit("DMA segbound wrapping (write)");
		offset &= dma_wrap;
		Bitu page = highpart_addr_page+(offset >> 12);
		/* care for EMS pageframe etc. */
		if (page < EMM_PAGEFRAME4K) page = paging.firstmb[page];
		else if (page < EMM_PAGEFRAME4K+0x10) page = ems_board_mapping[page];
		else if (page < LINK_START) page = paging.firstmb[page];
		phys_writeb(page*4096 + (offset & 4095), *read++);
	}
}

DmaChannel * GetDMAChannel(Bit8u chan) {
	if (chan<4) {
		/* channel on first DMA controller */
		if (DmaControllers[0]) return DmaControllers[0]->GetChannel(chan);
	} else if (chan<8) {
		/* channel on second DMA controller */
		if (DmaControllers[1]) return DmaControllers[1]->GetChannel(chan-4);
	}
	return NULL;
}

/* remove the second DMA controller (ports are removed automatically) */
void CloseSecondDMAController(void) {
	if (DmaControllers[1]) {
		delete DmaControllers[1];
		DmaControllers[1]=NULL;
	}
}

/* check availability of second DMA controller, needed for SB16 */
bool SecondDMAControllerAvailable(void) {
	if (DmaControllers[1]) return true;
	else return false;
}

static void DMA_Write_Port(Bitu port,Bitu val,Bitu /*iolen*/) {
	if (port<0x10) {
		/* write to the first DMA controller (channels 0-3) */
		DmaControllers[0]->WriteControllerReg(port,val,1);
	} else if (port>=0xc0 && port <=0xdf) {
		/* write to the second DMA controller (channels 4-7) */
		DmaControllers[1]->WriteControllerReg((port-0xc0) >> 1,val,1);
	} else {
		UpdateEMSMapping();
		switch (port) {
			/* write DMA page register */
			case 0x81:GetDMAChannel(2)->SetPage((Bit8u)val);break;
			case 0x82:GetDMAChannel(3)->SetPage((Bit8u)val);break;
			case 0x83:GetDMAChannel(1)->SetPage((Bit8u)val);break;
			case 0x89:GetDMAChannel(6)->SetPage((Bit8u)val);break;
			case 0x8a:GetDMAChannel(7)->SetPage((Bit8u)val);break;
			case 0x8b:GetDMAChannel(5)->SetPage((Bit8u)val);break;
		}
	}
}

static Bitu DMA_Read_Port(Bitu port,Bitu iolen) {
	if (port<0x10) {
		/* read from the first DMA controller (channels 0-3) */
		return DmaControllers[0]->ReadControllerReg(port,iolen);
	} else if (port>=0xc0 && port <=0xdf) {
		/* read from the second DMA controller (channels 4-7) */
		return DmaControllers[1]->ReadControllerReg((port-0xc0) >> 1,iolen);
	} else switch (port) {
		/* read DMA page register */
		case 0x81:return GetDMAChannel(2)->pagenum;
		case 0x82:return GetDMAChannel(3)->pagenum;
		case 0x83:return GetDMAChannel(1)->pagenum;
		case 0x89:return GetDMAChannel(6)->pagenum;
		case 0x8a:return GetDMAChannel(7)->pagenum;
		case 0x8b:return GetDMAChannel(5)->pagenum;
	}
	return 0;
}

void DmaController::WriteControllerReg(Bitu reg,Bitu val,Bitu /*len*/) {
	DmaChannel * chan;
	switch (reg) {
	/* set base address of DMA transfer (1st byte low part, 2nd byte high part) */
	case 0x0:case 0x2:case 0x4:case 0x6:
		UpdateEMSMapping();
		chan=GetChannel((Bit8u)(reg >> 1));
		flipflop=!flipflop;
		if (flipflop) {
			chan->baseaddr=(chan->baseaddr&0xff00)|val;
			chan->curraddr=(chan->curraddr&0xff00)|val;
		} else {
			chan->baseaddr=(chan->baseaddr&0x00ff)|(val << 8);
			chan->curraddr=(chan->curraddr&0x00ff)|(val << 8);
		}
		break;
	/* set DMA transfer count (1st byte low part, 2nd byte high part) */
	case 0x1:case 0x3:case 0x5:case 0x7:
		UpdateEMSMapping();
		chan=GetChannel((Bit8u)(reg >> 1));
		flipflop=!flipflop;
		if (flipflop) {
			chan->basecnt=(chan->basecnt&0xff00)|val;
			chan->currcnt=(chan->currcnt&0xff00)|val;
		} else {
			chan->basecnt=(chan->basecnt&0x00ff)|(val << 8);
			chan->currcnt=(chan->currcnt&0x00ff)|(val << 8);
		}
		break;
	case 0x8:		/* Comand reg not used */
		break;
	case 0x9:		/* Request registers, memory to memory */
		//TODO Warning?
		break;
	case 0xa:		/* Mask Register */
		if ((val & 0x4)==0) UpdateEMSMapping();
		chan=GetChannel(val & 3);
		chan->SetMask((val & 0x4)>0);
		break;
	case 0xb:		/* Mode Register */
		UpdateEMSMapping();
		chan=GetChannel(val & 3);
		chan->autoinit=(val & 0x10) > 0;
		chan->increment=(val & 0x20) > 0;
		//TODO Maybe other bits?
		break;
	case 0xc:		/* Clear Flip/Flip */
		flipflop=false;
		break;
	case 0xd:		/* Master Clear/Reset */
		for (Bit8u ct=0;ct<4;ct++) {
			chan=GetChannel(ct);
			chan->SetMask(true);
			chan->tcount=false;
		}
		flipflop=false;
		break;
	case 0xe:		/* Clear Mask register */		
		UpdateEMSMapping();
		for (Bit8u ct=0;ct<4;ct++) {
			chan=GetChannel(ct);
			chan->SetMask(false);
		}
		break;
	case 0xf:		/* Multiple Mask register */
		UpdateEMSMapping();
		for (Bit8u ct=0;ct<4;ct++) {
			chan=GetChannel(ct);
			chan->SetMask(val & 1);
			val>>=1;
		}
		break;
	}
}

Bitu DmaController::ReadControllerReg(Bitu reg,Bitu /*len*/) {
	DmaChannel * chan;Bitu ret;
	switch (reg) {
	/* read base address of DMA transfer (1st byte low part, 2nd byte high part) */
	case 0x0:case 0x2:case 0x4:case 0x6:
		chan=GetChannel((Bit8u)(reg >> 1));
		flipflop=!flipflop;
		if (flipflop) {
			return chan->curraddr & 0xff;
		} else {
			return (chan->curraddr >> 8) & 0xff;
		}
	/* read DMA transfer count (1st byte low part, 2nd byte high part) */
	case 0x1:case 0x3:case 0x5:case 0x7:
		chan=GetChannel((Bit8u)(reg >> 1));
		flipflop=!flipflop;
		if (flipflop) {
			return chan->currcnt & 0xff;
		} else {
			return (chan->currcnt >> 8) & 0xff;
		}
	case 0x8:		/* Status Register */
		ret=0;
		for (Bit8u ct=0;ct<4;ct++) {
			chan=GetChannel(ct);
			if (chan->tcount) ret|=1 << ct;
			chan->tcount=false;
			if (chan->request) ret|=1 << (4+ct);
		}
		return ret;
	default:
		LOG(LOG_DMACONTROL,LOG_NORMAL)("Trying to read undefined DMA port %x",reg);
		break;
	}
	return 0xffffffff;
}

DmaChannel::DmaChannel(Bit8u num, bool dma16) {
	masked = true;
	callback = NULL;
	if(num == 4) return;
	channum = num;
	DMA16 = dma16 ? 0x1 : 0x0;
	pagenum = 0;
	pagebase = 0;
	baseaddr = 0;
	curraddr = 0;
	basecnt = 0;
	currcnt = 0;
	increment = true;
	autoinit = false;
	tcount = false;
	request = false;
}

Bitu DmaChannel::Read(Bitu want, Bit8u * buffer) {
	Bitu done=0;
	curraddr &= dma_wrapping;
again:
	Bitu left=(currcnt+1);
	if (want<left) {
		DMA_BlockRead(pagebase,curraddr,buffer,want,DMA16);
		done+=want;
		curraddr+=want;
		currcnt-=want;
	} else {
		DMA_BlockRead(pagebase,curraddr,buffer,want,DMA16);
		buffer+=left << DMA16;
		want-=left;
		done+=left;
		ReachedTC();
		if (autoinit) {
			currcnt=basecnt;
			curraddr=baseaddr;
			if (want) goto again;
			UpdateEMSMapping();
		} else {
			curraddr+=left;
			currcnt=0xffff;
			masked=true;
			UpdateEMSMapping();
			DoCallBack(DMA_TRANSFEREND);
		}
	}
	return done;
}

Bitu DmaChannel::Write(Bitu want, Bit8u * buffer) {
	Bitu done=0;
	curraddr &= dma_wrapping;
again:
	Bitu left=(currcnt+1);
	if (want<left) {
		DMA_BlockWrite(pagebase,curraddr,buffer,want,DMA16);
		done+=want;
		curraddr+=want;
		currcnt-=want;
	} else {
		DMA_BlockWrite(pagebase,curraddr,buffer,left,DMA16);
		buffer+=left << DMA16;
		want-=left;
		done+=left;
		ReachedTC();
		if (autoinit) {
			currcnt=basecnt;
			curraddr=baseaddr;
			if (want) goto again;
			UpdateEMSMapping();
		} else {
			curraddr+=left;
			currcnt=0xffff;
			masked=true;
			UpdateEMSMapping();
			DoCallBack(DMA_TRANSFEREND);
		}
	}
	return done;
}

class DMA:public Module_base{
public:
	DMA(Section* configuration):Module_base(configuration){
		Bitu i;
		DmaControllers[0] = new DmaController(0);
		if (IS_EGAVGA_ARCH) DmaControllers[1] = new DmaController(1);
		else DmaControllers[1] = NULL;
	
		for (i=0;i<0x10;i++) {
			Bitu mask=IO_MB;
			if (i<8) mask|=IO_MW;
			/* install handler for first DMA controller ports */
			DmaControllers[0]->DMA_WriteHandler[i].Install(i,DMA_Write_Port,mask);
			DmaControllers[0]->DMA_ReadHandler[i].Install(i,DMA_Read_Port,mask);
			if (IS_EGAVGA_ARCH) {
				/* install handler for second DMA controller ports */
				DmaControllers[1]->DMA_WriteHandler[i].Install(0xc0+i*2,DMA_Write_Port,mask);
				DmaControllers[1]->DMA_ReadHandler[i].Install(0xc0+i*2,DMA_Read_Port,mask);
			}
		}
		/* install handlers for ports 0x81-0x83 (on the first DMA controller) */
		DmaControllers[0]->DMA_WriteHandler[0x10].Install(0x81,DMA_Write_Port,IO_MB,3);
		DmaControllers[0]->DMA_ReadHandler[0x10].Install(0x81,DMA_Read_Port,IO_MB,3);

		if (IS_EGAVGA_ARCH) {
			/* install handlers for ports 0x81-0x83 (on the second DMA controller) */
			DmaControllers[1]->DMA_WriteHandler[0x10].Install(0x89,DMA_Write_Port,IO_MB,3);
			DmaControllers[1]->DMA_ReadHandler[0x10].Install(0x89,DMA_Read_Port,IO_MB,3);
		}
	}
	~DMA(){
		if (DmaControllers[0]) {
			delete DmaControllers[0];
			DmaControllers[0]=NULL;
		}
		if (DmaControllers[1]) {
			delete DmaControllers[1];
			DmaControllers[1]=NULL;
		}
	}
};

void DMA_SetWrapping(Bitu wrap) {
	dma_wrapping = wrap;
}

static DMA* test;

void DMA_Destroy(Section* /*sec*/){
	delete test;
}
void DMA_Init(Section* sec) {
	DMA_SetWrapping(0xffff);
	test = new DMA(sec);
	sec->AddDestroyFunction(&DMA_Destroy);
	Bitu i;
	for (i=0;i<LINK_START;i++) {
		ems_board_mapping[i]=i;
	}
}
