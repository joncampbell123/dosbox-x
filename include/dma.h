/*
 *  Copyright (C) 2002-2020  The DOSBox Team
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


#ifndef DOSBOX_DMA_H
#define DOSBOX_DMA_H
#include <fstream>

enum DMAEvent {
	DMA_REACHED_TC,
	DMA_MASKED,
	DMA_UNMASKED,
	DMA_TRANSFEREND
};

enum DMATransfer {
    DMAT_VERIFY=0,
    DMAT_WRITE=1,
    DMAT_READ=2
};

class DmaChannel;
typedef void (* DMA_CallBack)(DmaChannel * chan,DMAEvent event);

class DmaChannel {
public:
	Bit32u pagebase;
	uint16_t baseaddr;
	Bit32u curraddr;
	uint16_t basecnt;
	uint16_t currcnt;
	uint8_t channum;
	uint8_t pagenum;
    uint8_t DMA16_PAGESHIFT;
    Bit32u DMA16_ADDRMASK;
	uint8_t DMA16;
    uint8_t transfer_mode;
	bool increment;
	bool autoinit;
	bool masked;
	bool tcount;
	bool request;
	DMA_CallBack callback;

    // additional PC-98 proprietary feature:
    //  auto "bank" increment on DMA wraparound.
    //
    //  I/O port 29h:
    //    bits [7:4] = 0
    //    bits [3:2] = increment mode   0=64KB wraparound (no incr)  1=1MB boundary wrap   2=invalid   3=16MB boundary wrap
    //    bits [1:0] = which DMA channel to set
    //
    //  This value is set by:
    //    0 = 0x00
    //    1 = 0x0F
    //    2 = 0xF0 (probably why it's invalid)
    //    3 = 0xFF
    //
    // TODO: Does this setting stick or does it reset after normal legacy programming?
    // TODO: When the bank auto increments does it increment the actual register or just
    //       an internal copy?
    uint8_t page_bank_increment_wraparound = 0u;

    void page_bank_increment(void) { // to be called on DMA wraparound
        if (page_bank_increment_wraparound != 0u) {
            // FIXME: Improve this.
            // Currently this code assumes that the auto increment in PC-98 modifies the
            // register value (and therefore visible to the guest). Change this code if
            // that model is wrong.
            const uint8_t add =
                increment ? 0x01u : 0xFFu;
            const uint8_t nv =
                ( pagenum        & (~page_bank_increment_wraparound)) +
                ((pagenum + add) & ( page_bank_increment_wraparound));
            SetPage(nv);
        }
    }

	DmaChannel(uint8_t num, bool dma16);
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
	void SetPage(uint8_t val) {
		pagenum=val;
		pagebase=(Bit32u)(pagenum >> DMA16_PAGESHIFT) << (Bit32u)((uint8_t)16u + DMA16_PAGESHIFT);
	}
	void Raise_Request(void) {
		request=true;
	}
	void Clear_Request(void) {
		request=false;
	}
	Bitu Read(Bitu want, uint8_t * buffer);
	Bitu Write(Bitu want, uint8_t * buffer);

	void SaveState( std::ostream& stream );
	void LoadState( std::istream& stream );
};

class DmaController {
private:
	uint8_t ctrlnum;
	bool flipflop;
    DmaChannel* DmaChannels[4] = {};
public:
	IO_ReadHandleObject DMA_ReadHandler[0x15];
	IO_WriteHandleObject DMA_WriteHandler[0x15];
	DmaController(uint8_t num) {
		flipflop = false;
		ctrlnum = num;		/* first or second DMA controller */
		for(uint8_t i=0;i<4;i++) {
			DmaChannels[i] = new DmaChannel(i+ctrlnum*4,ctrlnum==1);
		}
	}
	~DmaController(void) {
		for(uint8_t i=0;i<4;i++) {
			delete DmaChannels[i];
		}
	}
	DmaChannel * GetChannel(uint8_t chan) {
		if (chan<4) return DmaChannels[chan];
		else return NULL;
	}
	void WriteControllerReg(Bitu reg,Bitu val,Bitu len);
	Bitu ReadControllerReg(Bitu reg,Bitu len);

	void SaveState( std::ostream& stream );
	void LoadState( std::istream& stream );
};

DmaChannel * GetDMAChannel(uint8_t chan);

void CloseSecondDMAController(void);
bool SecondDMAControllerAvailable(void);

void DMA_SetWrapping(Bit32u wrap);

#endif
