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


#ifndef DOSBOX_DMA_H
#define DOSBOX_DMA_H
#include <fstream>

enum DMAEvent {
	DMA_REACHED_TC,
	DMA_MASKED,
	DMA_UNMASKED,
	DMA_TRANSFEREND
};

class DmaChannel;
typedef void (* DMA_CallBack)(DmaChannel * chan,DMAEvent event);

class DmaChannel {
public:
	Bit32u pagebase;
	Bit16u baseaddr;
	Bit32u curraddr;
	Bit16u basecnt;
	Bit16u currcnt;
	Bit8u channum;
	Bit8u pagenum;
    Bit8u DMA16_PAGESHIFT;
    Bit32u DMA16_ADDRMASK;
	Bit8u DMA16;
	bool increment;
	bool autoinit;
	Bit8u trantype;
	bool masked;
	bool tcount;
	bool request;
	DMA_CallBack callback;

	DmaChannel(Bit8u num, bool dma16);
	void DoCallBack(DMAEvent event) {
		if (callback)	(*callback)(this,event);
	}
	void SetMask(bool _mask) {
		masked=_mask;
		DoCallBack(masked ? DMA_MASKED : DMA_UNMASKED);
	}
    void Set128KMode(bool en) {
        // 128KB mode (legacy ISA) (en=true):
        //    page shift = 1        (discard bit 0 of page register)
        //    addr mask = 0x1FFFF   (all bits 0-15 become bits 1-16, bit 15 of addr takes the place of page register bit 0)
        // 64KB mode (modern PCI including Intel chipsets) (en=false):
        //    page shift = 0        (all 8 bits of page register are used)
        //    addr mask = 0xFFFF    (discard bit 15, bits 0-14 become bits 1-15 on ISA bus)
        DMA16_PAGESHIFT = (en && DMA16) ? 0x1 : 0x0; // nonzero if we're to discard bit 0 of page register
        DMA16_ADDRMASK = (1UL << ((en && DMA16) ? 17UL : 16UL)) - 1UL; // nonzero if (addrreg << 1) to cover 128KB, zero if (addrreg << 1) to discard MSB, limit to 64KB
    }
	void Register_Callback(DMA_CallBack _cb) { 
		callback = _cb; 
		SetMask(masked);
		if (callback) Raise_Request();
		else Clear_Request();
	}
	void ReachedTC(void) {
		tcount=true;
		DoCallBack(DMA_REACHED_TC);
	}
	void SetPage(Bit8u val) {
		pagenum=val;
		pagebase=(pagenum >> DMA16_PAGESHIFT) << (16+DMA16_PAGESHIFT);
	}
	void Raise_Request(void) {
		request=true;
	}
	void Clear_Request(void) {
		request=false;
	}
	Bitu Read(Bitu size, Bit8u * buffer);
	Bitu Write(Bitu size, Bit8u * buffer);

	void SaveState( std::ostream& stream );
	void LoadState( std::istream& stream );
};

class DmaController {
private:
	Bit8u ctrlnum;
	bool flipflop;
	DmaChannel *DmaChannels[4];
public:
	IO_ReadHandleObject DMA_ReadHandler[0x11];
	IO_WriteHandleObject DMA_WriteHandler[0x11];
	DmaController(Bit8u num) {
		flipflop = false;
		ctrlnum = num;		/* first or second DMA controller */
		for(Bit8u i=0;i<4;i++) {
			DmaChannels[i] = new DmaChannel(i+ctrlnum*4,ctrlnum==1);
		}
	}
	~DmaController(void) {
		for(Bit8u i=0;i<4;i++) {
			delete DmaChannels[i];
		}
	}
	DmaChannel * GetChannel(Bit8u chan) {
		if (chan<4) return DmaChannels[chan];
		else return NULL;
	}
	void WriteControllerReg(Bitu reg,Bitu val,Bitu len);
	Bitu ReadControllerReg(Bitu reg,Bitu len);

	void SaveState( std::ostream& stream );
	void LoadState( std::istream& stream );
};

DmaChannel * GetDMAChannel(Bit8u chan);

void CloseSecondDMAController(void);
bool SecondDMAControllerAvailable(void);

void DMA_SetWrapping(Bitu wrap);

#endif
