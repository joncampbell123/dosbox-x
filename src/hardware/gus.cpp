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


#include <string.h>
#include <iomanip>
#include <sstream>
#include "dosbox.h"
#include "inout.h"
#include "mixer.h"
#include "dma.h"
#include "pic.h"
#include "control.h"
#include "setup.h"
#include "shell.h"
#include "math.h"
#include "regs.h"
using namespace std;

enum GUSType {
	GUS_CLASSIC=0,
	GUS_MAX,
	GUS_INTERWAVE
};

//Extra bits of precision over normal gus
#define WAVE_BITS 2
#define WAVE_FRACT (9+WAVE_BITS)
#define WAVE_FRACT_MASK ((1 << WAVE_FRACT)-1)
#define WAVE_MSWMASK ((1 << (16+WAVE_BITS))-1)
#define WAVE_LSWMASK (0xffffffff ^ WAVE_MSWMASK)

//Amount of precision the volume has
#define RAMP_FRACT (10)
#define RAMP_FRACT_MASK ((1 << RAMP_FRACT)-1)

#define GUS_BASE myGUS.portbase
#define GUS_RATE myGUS.rate
#define LOG_GUS 0

// fixed panning table (avx)
static Bit16u const pantablePDF[16] = { 0, 13, 26, 41, 57, 72, 94, 116, 141, 169, 203, 244, 297, 372, 500, 4095 };
static bool gus_fixed_table = false;

Bit8u adlib_commandreg;
static MixerChannel * gus_chan;
static Bit8u const irqtable[8] = { 0/*invalid*/, 2, 5, 3, 7, 11, 12, 15 };
static Bit8u const dmatable[8] = { 0/*NO DMA*/, 1, 3, 5, 6, 7, 0/*invalid*/, 0/*invalid*/ };
static Bit8u GUSRam[1024*1024 + 16/*safety margin*/]; // 1024K of GUS Ram
static Bit32s AutoAmp = 512;
static Bit16u vol16bit[4096];
static Bit32u pantable[16];
static enum GUSType gus_type = GUS_CLASSIC;
static bool gus_ics_mixer = false;

class GUSChannels;
static void CheckVoiceIrq(void);

struct GFGus {
	Bit8u gRegSelectData;		// what is read back from 3X3. not necessarily the index selected, but
					// apparently the last byte read OR written to ports 3X3-3X5 as seen
					// on actual GUS hardware.
	Bit8u gRegSelect;
	Bit16u gRegData;
	Bit32u gDramAddr;
	Bit16u gCurChannel;

	Bit8u gUltraMAXControl;
	Bit8u DMAControl;
	Bit16u dmaAddr;
	Bit8u dmaAddrOffset; /* bits 0-3 of the addr */
	Bit8u TimerControl;
	Bit8u SampControl;
	Bit8u mixControl;
	Bit8u ActiveChannels;
	Bit8u ActiveChannelsUser; /* what the guest wrote */
	Bit8u gRegControl;
	Bit32u basefreq;

	struct GusTimer {
		float delay;
		Bit8u value;
		bool reached;
		bool raiseirq;
		bool masked;
		bool running;
	} timers[2];
	Bit32u rate;
	Bitu portbase;
	Bit32u memsize;
	Bit8u dma1;
	Bit8u dma2;

	Bit8u irq1;		// GF1 IRQ
	Bit8u irq2;		// MIDI IRQ

	bool irqenabled;
	bool ChangeIRQDMA;
	bool initUnmaskDMA;
	bool force_master_irq_enable;
	bool fixed_sample_rate_output;
	bool clearTCIfPollingIRQStatus;
	double lastIRQStatusPollAt;
	int lastIRQStatusPollRapidCount;
	// IRQ status register values
	Bit8u IRQStatus;
	Bit32u ActiveMask;
	Bit8u IRQChan;
	Bit32u RampIRQ;
	Bit32u WaveIRQ;
} myGUS;

Bitu DEBUG_EnableDebugger(void);

// Returns a single 16-bit sample from the Gravis's RAM
static INLINE Bit32s GetSample(Bit32u Delta, Bit32u CurAddr, bool eightbit) {
	Bit32u useAddr;
	Bit32u holdAddr;
	useAddr = CurAddr >> WAVE_FRACT;
	if (eightbit) {
		if (Delta >= (1 << WAVE_FRACT)) {
			Bit32s tmpsmall = (Bit8s)GUSRam[useAddr];
			return tmpsmall << 8;
		} else {
			// Interpolate
			Bit32s w1 = ((Bit8s)GUSRam[useAddr+0]) << 8;
			Bit32s w2 = ((Bit8s)GUSRam[useAddr+1]) << 8;
			Bit32s diff = w2 - w1;
			return (w1+((diff*(Bit32s)(CurAddr&WAVE_FRACT_MASK ))>>WAVE_FRACT));
		}
	} else {
		// Formula used to convert addresses for use with 16-bit samples
		holdAddr = useAddr & 0xc0000L;
		useAddr = useAddr & 0x1ffffL;
		useAddr = useAddr << 1;
		useAddr = (holdAddr | useAddr);

		if(Delta >= (1 << WAVE_FRACT)) {
			return (GUSRam[useAddr+0] | (((Bit8s)GUSRam[useAddr+1]) << 8));
		} else {
			// Interpolate
			Bit32s w1 = (GUSRam[useAddr+0] | (((Bit8s)GUSRam[useAddr+1]) << 8));
			Bit32s w2 = (GUSRam[useAddr+2] | (((Bit8s)GUSRam[useAddr+3]) << 8));
			Bit32s diff = w2 - w1;
			return (w1+((diff*(Bit32s)(CurAddr&WAVE_FRACT_MASK ))>>WAVE_FRACT));
		}
	}
}

static uint8_t GUS_reset_reg = 0;

class GUSChannels {
public:
	Bit32u WaveStart;
	Bit32u WaveEnd;
	Bit32u WaveAddr;
	Bit32u WaveAdd;
	Bit8u  WaveCtrl;
	Bit16u WaveFreq;

	Bit32u RampStart;
	Bit32u RampEnd;
	Bit32u RampVol;
	Bit32u RampAdd;
	Bit32u RampAddReal;

	Bit8u RampRate;
	Bit8u RampCtrl;

	Bit8u PanPot;
	Bit8u channum;
	Bit32u irqmask;
	Bit32u PanLeft;
	Bit32u PanRight;
	Bit32s VolLeft;
	Bit32s VolRight;

	GUSChannels(Bit8u num) { 
		channum = num;
		irqmask = 1 << num;
		WaveStart = 0;
		WaveEnd = 0;
		WaveAddr = 0;
		WaveAdd = 0;
		WaveFreq = 0;
		WaveCtrl = 3;
		RampRate = 0;
		RampStart = 0;
		RampEnd = 0;
		RampCtrl = 3;
		RampAdd = 0;
		RampAddReal = 0;
		RampVol = 0;
		VolLeft = 0;
		VolRight = 0;
		PanLeft = 0;
		PanRight = 0;
		PanPot = 0x7;
	};
	void WriteWaveFreq(Bit16u val) {
		WaveFreq = val;
		if (myGUS.fixed_sample_rate_output) {
			double frameadd = double(val >> 1)/512.0;		//Samples / original gus frame
			double realadd = (frameadd*(double)myGUS.basefreq/(double)GUS_RATE) * (double)(1 << WAVE_FRACT);
			WaveAdd = (Bit32u)realadd;
		}
		else {
			WaveAdd = ((Bit32u)(val >> 1)) << ((Bit32u)(WAVE_FRACT-9));
		}
	}
	void WriteWaveCtrl(Bit8u val) {
		Bit32u oldirq=myGUS.WaveIRQ;
		WaveCtrl = val & 0x7f;

		if ((val & 0xa0)==0xa0) myGUS.WaveIRQ|=irqmask;
		else myGUS.WaveIRQ&=~irqmask;

		if (oldirq != myGUS.WaveIRQ) 
			CheckVoiceIrq();
	}
	INLINE Bit8u ReadWaveCtrl(void) {
		Bit8u ret=WaveCtrl;
		if (myGUS.WaveIRQ & irqmask) ret|=0x80;
		return ret;
	}
	void UpdateWaveRamp(void) { 
		WriteWaveFreq(WaveFreq);
		WriteRampRate(RampRate);
	}
	void WritePanPot(Bit8u val) {
		PanPot = val;
		PanLeft = pantable[(val & 0xf)];
		PanRight = pantable[0x0f-(val & 0xf)];
		UpdateVolumes();
	}
	Bit8u ReadPanPot(void) {
		return PanPot;
	}
	void WriteRampCtrl(Bit8u val) {
		Bit32u old=myGUS.RampIRQ;
		RampCtrl = val & 0x7f;
		if ((val & 0xa0)==0xa0) myGUS.RampIRQ|=irqmask;
		else myGUS.RampIRQ&=~irqmask;
		if (old != myGUS.RampIRQ) CheckVoiceIrq();
	}
	INLINE Bit8u ReadRampCtrl(void) {
		Bit8u ret=RampCtrl;
		if (myGUS.RampIRQ & irqmask) ret|=0x80;
		return ret;
	}
	void WriteRampRate(Bit8u val) {
		RampRate = val;
		if (myGUS.fixed_sample_rate_output) {
			double frameadd = (double)(RampRate & 63)/(double)(1 << (3*(val >> 6)));
			double realadd = (frameadd*(double)myGUS.basefreq/(double)GUS_RATE) * (double)(1 << RAMP_FRACT);
			RampAdd = (Bit32u)realadd;
		}
		else {
			/* NTS: Note RAMP_FRACT == 10, shift = 10 - (3*(val>>6)).
			 * From the upper two bits, the possible shift values for 0, 1, 2, 3 are: 10, 7, 4, 1 */
			RampAdd = ((Bit32u)(RampRate & 63)) << ((Bit32u)(RAMP_FRACT - (3*(val >> 6))));
#if 0//SET TO 1 TO CHECK YOUR MATH!
			double frameadd = (double)(RampRate & 63)/(double)(1 << (3*(val >> 6)));
			double realadd = frameadd * (double)(1 << RAMP_FRACT);
			Bit32u checkadd = (Bit32u)realadd;
			signed long error = (signed long)checkadd - (signed long)RampAdd;

			if (error < -1L || error > 1L)
				LOG_MSG("RampAdd nonfixed error %ld (%lu != %lu)",error,(unsigned long)checkadd,(unsigned long)RampAdd);
#endif
		}
	}
	INLINE void WaveUpdate(void) {
		Bit32u WaveExtra = 0;
		bool endcondition;

		if ((WaveCtrl & 0x3) == 0/*voice is running*/) {
			/* NTS: WaveAddr and WaveAdd are unsigned.
			 *      If WaveAddr <= WaveAdd going backwards, WaveAddr becomes negative, which as an unsigned integer,
			 *      means carrying down from the highest possible value of the integer type. Which means that if the
			 *      start position is less than WaveAdd the WaveAddr will skip over the start pointer and continue
			 *      playing downward from the top of the GUS memory, without stopping/looping as expected.
			 *
			 *      This "bug" was implemented on purpose because real Gravis Ultrasound hardware acts this way. */
			if (WaveCtrl & 0x40/*backwards (direction)*/) {
				/* unsigned int subtract, mask, compare. will miss start pointer if WaveStart <= WaveAdd.
				 * This bug is deliberate, accurate to real GUS hardware, do not fix. */
				WaveAddr -= WaveAdd;
				WaveAddr &= ((Bitu)1 << ((Bitu)WAVE_FRACT + (Bitu)20/*1MB*/)) - 1;
				endcondition = (WaveAddr < WaveStart)?true:false;
				if (endcondition) WaveExtra = WaveStart - WaveAddr;
			}
			else {
				WaveAddr += WaveAdd;
				endcondition = (WaveAddr > WaveEnd)?true:false;
				WaveAddr &= ((Bitu)1 << ((Bitu)WAVE_FRACT + (Bitu)20/*1MB*/)) - 1;
				if (endcondition) WaveExtra = WaveAddr - WaveEnd;
			}

			if (endcondition) {
				if (WaveCtrl & 0x20) /* generate an IRQ if requested */
					myGUS.WaveIRQ |= irqmask;

				if ((RampCtrl & 0x04/*roll over*/) && !(WaveCtrl & 0x08/*looping*/)) {
					/* "3.11. Rollover feature
					 * 
					 * Each voice has a 'rollover' feature that allows an application to be notified when a voice's playback position passes
					 * over a particular place in DRAM.  This is very useful for getting seamless digital audio playback.  Basically, the GF1
					 * will generate an IRQ when a voice's current position is  equal to the end position.  However, instead of stopping or
					 * looping back to the start position, the voice will continue playing in the same direction.  This means that there will be
					 * no pause (or gap) in the playback.  Note that this feature is enabled/disabled through the voice's VOLUME control
					 * register (since there are no more bits available in the voice control registers).   A voice's loop enable bit takes
					 * precedence over the rollover.  This means that if a voice's loop enable is on, it will loop when it hits the end position,
					 * regardless of the state of the rollover enable."
					 *
					 * Despite the confusing description above, that means that looping takes precedence over rollover. If not looping, then
					 * rollover means to fire the IRQ but keep moving. If looping, then fire IRQ and carry out loop behavior. Gravis Ultrasound
					 * Windows 3.1 drivers expect this behavior, else Windows WAVE output will not work correctly. */
				}
				else {
					if (WaveCtrl & 0x08/*loop*/) {
						if (WaveCtrl & 0x10/*bi-directional*/) WaveCtrl ^= 0x40/*change direction*/;
						WaveAddr = (WaveCtrl & 0x40) ? (WaveEnd - WaveExtra) : (WaveStart + WaveExtra);
					} else {
						WaveCtrl |= 1; /* stop the channel */
						WaveAddr = (WaveCtrl & 0x40) ? WaveStart : WaveEnd;
					}
				}
			}
		}
		else if ((WaveCtrl & 0x20)/*IRQ enabled*/) {
			/* Undocumented behavior observed on real GUS hardware: A stopped voice will still rapid-fire IRQs
			 * if IRQ enabled and current position <= start position OR current position >= end position */
			if (WaveCtrl & 0x40/*backwards (direction)*/)
				endcondition = (WaveAddr <= WaveStart)?true:false;
			else
				endcondition = (WaveAddr >= WaveEnd)?true:false;

			if (endcondition)
				myGUS.WaveIRQ |= irqmask;
		}
	}
	INLINE void UpdateVolumes(void) {
		Bit32s templeft=RampVol - PanLeft;
		templeft&=~(templeft >> 31);
		Bit32s tempright=RampVol - PanRight;
		tempright&=~(tempright >> 31);
		VolLeft=vol16bit[templeft >> RAMP_FRACT];
		VolRight=vol16bit[tempright >> RAMP_FRACT];
	}
	INLINE void RampUpdate(void) {
		if (RampCtrl & 0x3) return; /* if the ramping is turned off, then don't change the ramp */

		Bit32s RampLeft;
		if (RampCtrl & 0x40) {
			RampVol-=RampAdd;
			if ((Bit32s)RampVol < (Bit32s)0) RampVol=0;
			RampLeft=RampStart-RampVol;
		} else {
			RampVol+=RampAdd;
			if (RampVol > ((4096 << RAMP_FRACT)-1)) RampVol=((4096 << RAMP_FRACT)-1);
			RampLeft=RampVol-RampEnd;
		}
		if (RampLeft<0) {
			UpdateVolumes();
			return;
		}
		/* Generate an IRQ if needed */
		if (RampCtrl & 0x20) {
			myGUS.RampIRQ|=irqmask;
		}
		/* Check for looping */
		if (RampCtrl & 0x08) {
			/* Bi-directional looping */
			if (RampCtrl & 0x10) RampCtrl^=0x40;
			RampVol = (RampCtrl & 0x40) ? (RampEnd-RampLeft) : (RampStart+RampLeft);
		} else {
			RampCtrl|=1;	//Stop the channel
			RampVol = (RampCtrl & 0x40) ? RampStart : RampEnd;
		}
		if ((Bit32s)RampVol < (Bit32s)0) RampVol=0;
		if (RampVol > ((4096 << RAMP_FRACT)-1)) RampVol=((4096 << RAMP_FRACT)-1);
		UpdateVolumes();
	}
	void generateSamples(Bit32s * stream,Bit32u len) {
		bool eightbit = ((WaveCtrl & 0x4) == 0);
		Bit32s tmpsamp;
		int i;

		/* NTS: The GUS is *always* rendering the audio sample at the current position,
		 *      even if the voice is stopped. This can be confirmed using DOSLIB, loading
		 *      the Ultrasound test program, loading a WAV file into memory, then using
		 *      the Ultrasound test program's voice control dialog to single-step the
		 *      voice through RAM (abruptly change the current position) while the voice
		 *      is stopped. You will hear "popping" noises come out the GUS audio output
		 *      as the current position changes and the piece of the sample rendered
		 *      abruptly changes as well. */
		for(i=0;i<(int)len;i++) {
			// Get sample
			tmpsamp = GetSample(WaveAdd, WaveAddr, eightbit);

			// Output stereo sample if DAC enable on
			if ((GUS_reset_reg & 0x02/*DAC enable*/) == 0x02) {
				stream[i<<1]+= tmpsamp * VolLeft;
				stream[(i<<1)+1]+= tmpsamp * VolRight;
			}

			WaveUpdate();
			RampUpdate();
		}
	}
};

static GUSChannels *guschan[32] = {NULL};
static GUSChannels *curchan = NULL;

static INLINE void GUS_CheckIRQ(void);

static void GUS_TimerEvent(Bitu val);

static void GUS_DMA_Callback(DmaChannel * chan,DMAEvent event);

void GUS_StopDMA();
void GUS_StartDMA();
void GUS_Update_DMA_Event_transfer();

static void GUSReset(void) {
	unsigned char p_GUS_reset_reg = GUS_reset_reg;

	/* NTS: From the Ultrasound SDK:
	 *
	 *      Global Data Low (3X4) is either a 16-bit transfer, or the low half of a 16-bit transfer with 8-bit I/O.
	 *
	 *      Global Data High (3X5) is either an 8-bit transfer for one of the GF registers or the high part of a 16-bit wide register with 8-bit I/O.
	 *
	 *      Prior to 2015/12/29 DOSBox and DOSBox-X contained a programming error here where reset and master IRQ enable were handled from the
	 *      LOWER 8 bits, when the code should have been checking the UPPER 8 bits. Programming error #2 was the mis-interpetation of bit 0 (bit 8 of
	 *      the gRegData). According to the SDK, clearing bit 0 triggers RESET, setting bit 0 starts the card running again. The original code had
	 *      it backwards. */
	GUS_reset_reg = (myGUS.gRegData >> 8) & 7;

	if ((myGUS.gRegData & 0x400) != 0x000 || myGUS.force_master_irq_enable)
		myGUS.irqenabled = true;
	else
		myGUS.irqenabled = false;

	LOG(LOG_MISC,LOG_DEBUG)("GUS reset with 0x%04X",myGUS.gRegData);
	if ((myGUS.gRegData & 0x100) == 0x000) {
		// Stop all channels
		int i;
		for(i=0;i<32;i++) {
			guschan[i]->RampVol=0;
			guschan[i]->WriteWaveCtrl(0x1);
			guschan[i]->WriteRampCtrl(0x1);
			guschan[i]->WritePanPot(0x7);
		}

		// Stop DMA
		GUS_StopDMA();

		// Reset
		adlib_commandreg = 85;
		myGUS.IRQStatus = 0;
		myGUS.RampIRQ = 0;
		myGUS.WaveIRQ = 0;
		myGUS.IRQChan = 0;

		myGUS.timers[0].delay = 0.080f;
		myGUS.timers[1].delay = 0.320f;
		myGUS.timers[0].value = 0xff;
		myGUS.timers[1].value = 0xff;
		myGUS.timers[0].masked = false;
		myGUS.timers[1].masked = false;
		myGUS.timers[0].raiseirq = false;
		myGUS.timers[1].raiseirq = false;
		myGUS.timers[0].reached = true;
		myGUS.timers[1].reached = true;
		myGUS.timers[0].running = false;
		myGUS.timers[1].running = false;

		PIC_RemoveEvents(GUS_TimerEvent);

		myGUS.ChangeIRQDMA = false;
		myGUS.DMAControl = 0x00;
		myGUS.mixControl = 0x0b;	// latches enabled by default LINEs disabled
		myGUS.TimerControl = 0x00;
		myGUS.SampControl = 0x00;
		myGUS.ActiveChannels = 14;
		myGUS.ActiveChannelsUser = 14;
		myGUS.ActiveMask=0xffffffffU >> (32-myGUS.ActiveChannels);
		myGUS.basefreq = (Bit32u)((float)1000000/(1.619695497*(float)(myGUS.ActiveChannels)));

		gus_chan->FillUp();
		if (!myGUS.fixed_sample_rate_output)	gus_chan->SetFreq(myGUS.basefreq);
		else					gus_chan->SetFreq(GUS_RATE);

		myGUS.gCurChannel = 0;
		curchan = guschan[myGUS.gCurChannel];

		myGUS.dmaAddr = 0;
		myGUS.irqenabled = 0;
		myGUS.gRegControl = 0;
		myGUS.dmaAddrOffset = 0;
		myGUS.gDramAddr = 0;
		myGUS.gRegData = 0;

		GUS_Update_DMA_Event_transfer();
	}

	/* if the card was just put into reset, or the card WAS in reset, bits 1-2 are cleared */
	if ((GUS_reset_reg & 1) == 0 || (p_GUS_reset_reg & 1) == 0) {
		/* GUS classic observed behavior: resetting the card, or even coming out of reset, clears bits 1-2.
		 * That means, if you write any value to GUS RESET with bit 0 == 0, bits 1-2 become zero as well.
		 * And if you take the card out of reset, bits 1-2 are zeroed.
		 *
		 * test 1:
		 * outb(0x3X3,0x4C); outb(0x3X5,0x00);
		 * outb(0x3X3,0x4C); c = inb(0x3X5);      <- you'll get 0x00 as expected
		 * outb(0x3X3,0x4C); outb(0x3X5,0x07);
		 * outb(0x3X3,0x4C); c = inb(0x3X5);      <- you'll get 0x01, not 0x07
		 *
		 * test 2:
		 * outb(0x3X3,0x4C); outb(0x3X5,0x00);
		 * outb(0x3X3,0x4C); c = inb(0x3X5);      <- you'll get 0x00 as expected
		 * outb(0x3X3,0x4C); outb(0x3X5,0x01);
		 * outb(0x3X3,0x4C); c = inb(0x3X5);      <- you'll get 0x01 as expected, card taken out of reset
		 * outb(0x3X3,0x4C); outb(0x3X5,0x07);
		 * outb(0x3X3,0x4C); c = inb(0x3X5);      <- you'll get 0x07 as expected
		 * outb(0x3X3,0x4C); outb(0x3X5,0x06);    <- bit 0 == 0, we're trying to set bits 1-2
		 * outb(0x3X3,0x4C); c = inb(0x3X5);      <- you'll get 0x00, not 0x06, card is in reset state */
		myGUS.irqenabled = myGUS.force_master_irq_enable; // IRQ enable resets, unless user specified we force it on
		GUS_reset_reg &= 1;
	}

	GUS_CheckIRQ();
}

static uint8_t GUS_EffectiveIRQStatus(void) {
	uint8_t ret = 0;

	/* Behavior observed on real GUS hardware: Master IRQ enable bit 2 of the reset register affects only voice/wave
	 * IRQ signals from the GF1. It does not affect the DMA terminal count interrupt nor does it affect the Adlib timers.
	 * This is how "Juice" by Psychic Link is able to play music by GUS timer even though the demo never enables the
	 * Master IRQ Enable bit. */

	/* DMA */
	if (myGUS.DMAControl & 0x20/*DMA IRQ Enable*/)
		ret |= (myGUS.IRQStatus & 0x80/*DMA TC IRQ*/);

	/* Timer 1 & 2 */
	ret |= (myGUS.IRQStatus/*Timer 1&2 IRQ*/ & myGUS.TimerControl/*Timer 1&2 IRQ Enable*/ & 0x0C);

	/* Voice IRQ */
	if (myGUS.irqenabled)
		ret |= (myGUS.IRQStatus & 0x60/*Wave/Ramp IRQ*/);

	/* TODO: MIDI IRQ? */

	return ret;
}

static uint8_t gus_prev_effective_irqstat = 0;

static INLINE void GUS_CheckIRQ(void) {
	if (myGUS.mixControl & 0x08/*Enable latches*/) {
		uint8_t irqstat = GUS_EffectiveIRQStatus();

		if (irqstat != 0) {
			/* The GUS fires an IRQ, then waits for the interrupt service routine to
			 * clear all pending interrupt events before firing another one. if you
			 * don't service all events, then you don't get another interrupt. */
			if (gus_prev_effective_irqstat == 0)
				PIC_ActivateIRQ(myGUS.irq1);
		}
		else {
			PIC_DeActivateIRQ(myGUS.irq1);
		}

		gus_prev_effective_irqstat = irqstat;
	}
	else {/*Enable latch disabled. Technically this would cause random interrupts by leaving the IRQ lines floating, but...*/
		PIC_DeActivateIRQ(myGUS.irq1);
	}
}

static void CheckVoiceIrq(void) {
	Bitu totalmask=(myGUS.RampIRQ|myGUS.WaveIRQ) & myGUS.ActiveMask;
	if (!totalmask) {
		GUS_CheckIRQ();
		return;
	}

	if (myGUS.RampIRQ) myGUS.IRQStatus|=0x40;
	if (myGUS.WaveIRQ) myGUS.IRQStatus|=0x20;
	GUS_CheckIRQ();
	for (;;) {
		Bit32u check=(1 << myGUS.IRQChan);
		if (totalmask & check) return;
		myGUS.IRQChan++;
		if (myGUS.IRQChan>=myGUS.ActiveChannels) myGUS.IRQChan=0;
	}
}

static Bit16u ExecuteReadRegister(void) {
	Bit8u tmpreg;
//	LOG_MSG("Read global reg %x",myGUS.gRegSelect);
	switch (myGUS.gRegSelect) {
	case 0x8E:  // read active channel register
		// NTS: The GUS SDK documents the active channel count as bits 5-0, which is wrong. it's bits 4-0. bits 7-5 are always 1 on real hardware.
		return ((Bit16u)(0xE0 | (myGUS.ActiveChannelsUser - 1))) << 8;
	case 0x41: // Dma control register - read acknowledges DMA IRQ
		tmpreg = myGUS.DMAControl & 0xbf;
		tmpreg |= (myGUS.IRQStatus & 0x80) >> 1;
		myGUS.IRQStatus&=0x7f;
		GUS_CheckIRQ();
		return (Bit16u)(tmpreg << 8);
	case 0x42:  // Dma address register
		return myGUS.dmaAddr;
	case 0x45:  // Timer control register.  Identical in operation to Adlib's timer
		return (Bit16u)(myGUS.TimerControl << 8);
		break;
	case 0x49:  // Dma sample register
		tmpreg = myGUS.DMAControl & 0xbf;
		tmpreg |= (myGUS.IRQStatus & 0x80) >> 1;
		return (Bit16u)(tmpreg << 8);
	case 0x4c:  // GUS reset register
		tmpreg = (GUS_reset_reg & ~0x4) | (myGUS.irqenabled ? 0x4 : 0x0);
		/* GUS Classic observed behavior: You can read Register 4Ch from both 3X4 and 3X5 and get the same 8-bit contents */
		return ((Bit16u)(tmpreg << 8) | (Bit16u)tmpreg);
	case 0x80: // Channel voice control read register
		if (curchan) return curchan->ReadWaveCtrl() << 8;
		else return 0x0300;
	case 0x81:  // Channel frequency control register
		if(curchan) return (Bit16u)(curchan->WaveFreq);
		else return 0x0000;;
	case 0x82: // Channel MSB start address register
		if (curchan) return (Bit16u)(curchan->WaveStart >> (WAVE_BITS+16));
		else return 0x0000;
	case 0x83: // Channel LSW start address register
		if (curchan) return (Bit16u)(curchan->WaveStart >> WAVE_BITS);
		else return 0x0000;
	case 0x84: // Channel MSB end address register
		if (curchan) return (Bit16u)(curchan->WaveEnd >> (WAVE_BITS+16));
		else return 0x0000;
	case 0x85: // Channel LSW end address register
		if (curchan) return (Bit16u)(curchan->WaveEnd >> WAVE_BITS);
		else return 0x0000;

	case 0x89: // Channel volume register
		if (curchan) return (Bit16u)((curchan->RampVol >> RAMP_FRACT) << 4);
		else return 0x0000;
	case 0x8a: // Channel MSB current address register
		if (curchan) return (Bit16u)(curchan->WaveAddr >> (WAVE_BITS+16));
		else return 0x0000;
	case 0x8b: // Channel LSW current address register
		if (curchan) return (Bit16u)(curchan->WaveAddr >> WAVE_BITS);
		else return 0x0000;

	case 0x8d: // Channel volume control register
		if (curchan) return curchan->ReadRampCtrl() << 8;
		else return 0x0300;
	case 0x8f: // General channel IRQ status register
		tmpreg=myGUS.IRQChan|0x20;
		Bit32u mask;
		mask=1 << myGUS.IRQChan;
		if (!(myGUS.RampIRQ & mask)) tmpreg|=0x40;
		if (!(myGUS.WaveIRQ & mask)) tmpreg|=0x80;
		myGUS.RampIRQ&=~mask;
		myGUS.WaveIRQ&=~mask;
		myGUS.IRQStatus&=0x9f;
		CheckVoiceIrq();
		return (Bit16u)(tmpreg << 8);
	default:
#if LOG_GUS
		LOG_MSG("Read Register num 0x%x", myGUS.gRegSelect);
#endif
		return myGUS.gRegData;
	}
}

static void GUS_TimerEvent(Bitu val) {
	if (!myGUS.timers[val].masked) myGUS.timers[val].reached=true;
	if (myGUS.timers[val].raiseirq) {
		myGUS.IRQStatus|=0x4 << val;
		GUS_CheckIRQ();
	}
	if (myGUS.timers[val].running) 
		PIC_AddEvent(GUS_TimerEvent,myGUS.timers[val].delay,val);
}

 
static void ExecuteGlobRegister(void) {
	int i;
//	if (myGUS.gRegSelect|1!=0x44) LOG_MSG("write global register %x with %x", myGUS.gRegSelect, myGUS.gRegData);
	switch(myGUS.gRegSelect) {
	case 0x0:  // Channel voice control register
		if(curchan) curchan->WriteWaveCtrl((Bit16u)myGUS.gRegData>>8);
		break;
	case 0x1:  // Channel frequency control register
		if(curchan) curchan->WriteWaveFreq(myGUS.gRegData);
		break;
	case 0x2:  // Channel MSW start address register
		if (curchan) {
			Bit32u tmpaddr = (Bit32u)(myGUS.gRegData & 0x1fff) << (16+WAVE_BITS); /* upper 13 bits of integer portion */
			curchan->WaveStart = (curchan->WaveStart & WAVE_MSWMASK) | tmpaddr;
		}
		break;
	case 0x3:  // Channel LSW start address register
		if(curchan != NULL) {
			Bit32u tmpaddr = (Bit32u)(myGUS.gRegData & 0xffe0) << WAVE_BITS; /* lower 7 bits of integer portion, and all 4 bits of fractional portion. bits 4-0 of the incoming 16-bit WORD are not used */
			curchan->WaveStart = (curchan->WaveStart & WAVE_LSWMASK) | tmpaddr;
		}
		break;
	case 0x4:  // Channel MSW end address register
		if(curchan != NULL) {
			Bit32u tmpaddr = (Bit32u)(myGUS.gRegData & 0x1fff) << (16+WAVE_BITS); /* upper 13 bits of integer portion */
			curchan->WaveEnd = (curchan->WaveEnd & WAVE_MSWMASK) | tmpaddr;
		}
		break;
	case 0x5:  // Channel MSW end address register
		if(curchan != NULL) {
			Bit32u tmpaddr = (Bit32u)(myGUS.gRegData & 0xffe0) << WAVE_BITS; /* lower 7 bits of integer portion, and all 4 bits of fractional portion. bits 4-0 of the incoming 16-bit WORD are not used */
			curchan->WaveEnd = (curchan->WaveEnd & WAVE_LSWMASK) | tmpaddr;
		}
		break;
	case 0x6:  // Channel volume ramp rate register
		if(curchan != NULL) {
			Bit8u tmpdata = (Bit16u)myGUS.gRegData>>8;
			curchan->WriteRampRate(tmpdata);
		}
		break;
	case 0x7:  // Channel volume ramp start register  EEEEMMMM
		if(curchan != NULL) {
			Bit8u tmpdata = (Bit16u)myGUS.gRegData >> 8;
			curchan->RampStart = tmpdata << (4+RAMP_FRACT);
		}
		break;
	case 0x8:  // Channel volume ramp end register  EEEEMMMM
		if(curchan != NULL) {
			Bit8u tmpdata = (Bit16u)myGUS.gRegData >> 8;
			curchan->RampEnd = tmpdata << (4+RAMP_FRACT);
		}
		break;
	case 0x9:  // Channel current volume register
		if(curchan != NULL) {
			Bit16u tmpdata = (Bit16u)myGUS.gRegData >> 4;
			curchan->RampVol = tmpdata << RAMP_FRACT;
			curchan->UpdateVolumes();
		}
		break;
	case 0xA:  // Channel MSW current address register
		if(curchan != NULL) {
			Bit32u tmpaddr = (Bit32u)(myGUS.gRegData & 0x1fff) << (16+WAVE_BITS); /* upper 13 bits of integer portion */
			curchan->WaveAddr = (curchan->WaveAddr & WAVE_MSWMASK) | tmpaddr;
		}
		break;
	case 0xB:  // Channel LSW current address register
		if(curchan != NULL) {
			Bit32u tmpaddr = (Bit32u)(myGUS.gRegData & 0xffff) << (WAVE_BITS); /* lower 7 bits of integer portion, and all 9 bits of fractional portion */
			curchan->WaveAddr = (curchan->WaveAddr & WAVE_LSWMASK) | tmpaddr;
		}
		break;
	case 0xC:  // Channel pan pot register
		if(curchan) curchan->WritePanPot((Bit16u)myGUS.gRegData>>8);
		break;
	case 0xD:  // Channel volume control register
		if(curchan) curchan->WriteRampCtrl((Bit16u)myGUS.gRegData>>8);
		break;
	case 0xE:  // Set active channel register
		myGUS.gRegSelect = myGUS.gRegData>>8;		//JAZZ Jackrabbit seems to assume this?
		myGUS.ActiveChannelsUser = 1+((myGUS.gRegData>>8) & 31); // NTS: The GUS SDK documents this field as bits 5-0, which is wrong, it's bits 4-0. 5-0 would imply 64 channels.

		/* The GUS SDK claims that if a channel count less than 14 is written, then it caps to 14.
		 * That's not true. Perhaps what the SDK is doing, but the actual hardware acts differently.
		 * This implementation is based on what the Gravis Ultrasound MAX actually does with this
		 * register. You can apparently achieve higher than 44.1KHz sample rates by programming less
		 * than 14 channels, and the sample rate scale ramps up appropriately, except that values
		 * 0 and 1 have the same effect as writing 2 and 3. Very useful undocumented behavior!
		 * If Gravis were smart, they would have been able to claim 48KHz sample rates by allowing
		 * less than 14 channels in their SDK! Not sure why they would cap it like that, unless
		 * there are undocumented chipset instabilities with running at higher rates.
		 *
		 * So far only verified on a Gravis Ultrasound MAX.
		 *
		 * Does anyone out there have a Gravis Ultrasound Classic (original 1992 version) they can
		 * test for this behavior?
		 *
		 * NOTED: Gravis Ultrasound Plug & Play (interwave) cards *do* enforce the 14-channel minimum.
		 *        You can write less than 14 channels to this register, but unlike the Classic and Max
		 *        cards they will not run faster than 44.1KHz. */
		myGUS.ActiveChannels = myGUS.ActiveChannelsUser;

		if (gus_type < GUS_INTERWAVE) {
			// GUS MAX behavior seen on real hardware
			if(myGUS.ActiveChannels < 3) myGUS.ActiveChannels += 2;
			if(myGUS.ActiveChannels > 32) myGUS.ActiveChannels = 32;
		}
		else {
			// Interwave PnP behavior seen on real hardware
			if(myGUS.ActiveChannels < 14) myGUS.ActiveChannels = 14;
			if(myGUS.ActiveChannels > 32) myGUS.ActiveChannels = 32;
		}

		myGUS.ActiveMask=0xffffffffU >> (32-myGUS.ActiveChannels);
		myGUS.basefreq = (Bit32u)((float)1000000/(1.619695497*(float)(myGUS.ActiveChannels)));

		gus_chan->FillUp();
		if (!myGUS.fixed_sample_rate_output)	gus_chan->SetFreq(myGUS.basefreq);
		else					gus_chan->SetFreq(GUS_RATE);

#if LOG_GUS
		LOG_MSG("GUS set to %d channels fixed=%u freq=%luHz", myGUS.ActiveChannels,myGUS.fixed_sample_rate_output,(unsigned long)myGUS.basefreq);
#endif
		for (i=0;i<myGUS.ActiveChannels;i++) guschan[i]->UpdateWaveRamp();
		break;
	case 0x10:  // Undocumented register used in Fast Tracker 2
		break;
	case 0x41:  // Dma control register
		myGUS.DMAControl = (Bit8u)(myGUS.gRegData>>8);
		GUS_Update_DMA_Event_transfer();
		if (myGUS.DMAControl & 1) GUS_StartDMA();
		else GUS_StopDMA();
		break;
	case 0x42:  // Gravis DRAM DMA address register
		myGUS.dmaAddr = myGUS.gRegData;
		myGUS.dmaAddrOffset = 0;
		break;
	case 0x43:  // LSB Peek/poke DRAM position
		myGUS.gDramAddr = (0xff0000 & myGUS.gDramAddr) | ((Bit32u)myGUS.gRegData);
		break;
	case 0x44:  // MSW Peek/poke DRAM position
		myGUS.gDramAddr = (0xffff & myGUS.gDramAddr) | ((Bit32u)myGUS.gRegData>>8) << 16;
		break;
	case 0x45:  // Timer control register.  Identical in operation to Adlib's timer
		myGUS.TimerControl = (Bit8u)(myGUS.gRegData>>8);
		myGUS.timers[0].raiseirq=(myGUS.TimerControl & 0x04)>0;
		if (!myGUS.timers[0].raiseirq) myGUS.IRQStatus&=~0x04;
		myGUS.timers[1].raiseirq=(myGUS.TimerControl & 0x08)>0;
		if (!myGUS.timers[1].raiseirq) myGUS.IRQStatus&=~0x08;
		GUS_CheckIRQ();
		break;
	case 0x46:  // Timer 1 control
		myGUS.timers[0].value = (Bit8u)(myGUS.gRegData>>8);
		myGUS.timers[0].delay = (0x100 - myGUS.timers[0].value) * 0.080f;
		break;
	case 0x47:  // Timer 2 control
		myGUS.timers[1].value = (Bit8u)(myGUS.gRegData>>8);
		myGUS.timers[1].delay = (0x100 - myGUS.timers[1].value) * 0.320f;
		break;
	case 0x49:  // DMA sampling control register
		myGUS.SampControl = (Bit8u)(myGUS.gRegData>>8);
		if (myGUS.DMAControl & 1) GUS_StartDMA();
		else GUS_StopDMA();
		break;
	case 0x4c:  // GUS reset register
		GUSReset();
		break;
	default:
#if LOG_GUS
		LOG_MSG("Unimplemented global register %x -- %x", myGUS.gRegSelect, myGUS.gRegData);
#endif
		break;
	}
	return;
}

/* Gravis Ultrasound ICS-2101 Digitally Controlled Audio Mixer emulation */
/* NTS: This was written and tested only through Ultrasound software and emulation.
 *      I do not have a Gravis Ultrasound card with this type of mixer to test against. --J.C. */
struct gus_ICS2101 {
public:
	// ICS2101 and how Gravis wired up the input pairs when using it, according to some BSD and Linux kernel sources
	//
	// From the header file:
	//
	//   Register defs for Integrated Circuit Systems, Inc. ICS-2101 mixer
	//   chip, used on Gravis UltraSound cards.
	//    
	//   Block diagram:
	//                                    port #
	//                                         0 +----+
	//    Mic in (Right/Left)        -->--->---|    |
	//                                       1 |    |          amp --->---- amp out
	//    Line in (Right/Left)       -->--->---|    |           |
	//                                       2 |    |           |
	//    CD in (Right/Left)         -->--->---|    |--->---+---+----->---- line out
	//                                       3 |    |       |
	//    GF1 Out (Right/Left)       -->--->---|    |       |
	//                                       4 |    |       |
	//    Unused (Right/Left)        -->--->---|    |       |
	//                                         +----+       v
	//                                       ICS 2101       |
	//                                                      |
	//               To GF1 Sample Input ---<---------------+
	//                     
	//    Master output volume: mixer channel #5
	enum {
		MIC_IN_PORT=0,
		LINE_IN_PORT=1,
		CD_IN_PORT=2,
		GF1_OUT_PORT=3,
		UNUSED_PORT=4,
		MASTER_OUTPUT_PORT=5
	};
public:
	gus_ICS2101() {
	}
public:
	void addressWrite(uint8_t addr) {
		addr_attenuator = (addr >> 3) & 7;
		addr_control = addr & 7;
	}
	void dataWrite(uint8_t val) {
		LOG(LOG_MISC,LOG_DEBUG)("GUS ICS-2101 Mixer Data Write val=%02xh to attenuator=%u(%s) control=%u(%s)",
			(int)val,
			addr_attenuator,attenuatorName(addr_attenuator),
			addr_control,controlName(addr_control));
	}
	const char *attenuatorName(const uint8_t c) const {
		switch (c) {
			case 0:	return "Mic in";
			case 1:	return "Line in";
			case 2:	return "CD in";
			case 3:	return "GF1 out";
			case 4:	return "Pair 5, unused";
			case 5:	return "Master output";
		};

		return "?";
	}
	const char *controlName(const uint8_t c) const {
		switch (c) {
			case 0:	return "Control Left";		// 000
			case 1:	return "Control Right";		// 001
			case 2:	return "Attenuator Left";	// 010
			case 3:	return "Attenuator Right";	// 011
			case 4: case 5:				// 10x
				return "Pan/Balance";
		};

		return "?";
	}
public:
	uint8_t		addr_attenuator;	// which attenuator is selected
	uint8_t		addr_control;		// which control is selected
} GUS_ICS2101;

/* Gravis Ultrasound MAX Crystal Semiconductor CS4231A emulation */
/* NOTES:
 *
 *    This 4-port I/O interface, implemented by Crystal Semiconductors, Analog Devices, etc. and used
 *    by sound cards in the early 1990s, is the said to be the "standardized" hardware interface of
 *    the "Windows Sound System" standard at the time.
 *
 *    According to an AD1848 datasheet, and a CS4231A datasheet, all I/O ports and indirect registers
 *    appear to be the same, with the exception that Crystal Semiconductor adds 16 registers with the
 *    "MODE 2" bit.
 *
 *    Perhaps at some point, we can untie this from GUS emulation and let it exist as it's own C++
 *    class that covers CS4231A, AD1848, and other "WSS" chipset emulation on behalf of GUS and SB
 *    emulation, much like the OPL3 emulation already present in this source tree for Sound Blaster.
 *
 */
struct gus_cs4231 {
public:
	gus_cs4231() : address(0), mode2(false), ia4(false), trd(false), mce(true), init(false) {
	}
public:
	void data_write(uint8_t addr,uint8_t val) {
//		LOG(LOG_MISC,LOG_DEBUG)("GUS CS4231 write data addr=%02xh val=%02xh",addr,val);

		switch (addr) {
			case 0x00: /* Left ADC Input Control (I0) */
				ADCInputControl[0] = val; break;
			case 0x01: /* Right ADC Input Control (I1) */
				ADCInputControl[1] = val; break;
			case 0x02: /* Left Auxiliary #1 Input Control (I2) */
				Aux1InputControl[0] = val; break;
			case 0x03: /* Right Auxiliary #1 Input Control (I3) */
				Aux1InputControl[1] = val; break;
			case 0x06: /* Left DAC Output Control (I6) */
				DACOutputControl[0] = val; break;
			case 0x07: /* Left DAC Output Control (I7) */
				DACOutputControl[1] = val; break;
			case 0x0C: /* MODE and ID (I12) */
				mode2 = (val & 0x40)?1:0; break;
			default:
				LOG(LOG_MISC,LOG_DEBUG)("GUS CS4231 unhandled data write addr=%02xh val=%02xh",addr,val);
				break;
		}
	}
	uint8_t data_read(uint8_t addr) {
//		LOG(LOG_MISC,LOG_DEBUG)("GUS CS4231 read data addr=%02xh",addr);

		switch (addr) {
			case 0x00: /* Left ADC Input Control (I0) */
				return ADCInputControl[0];
			case 0x01: /* Right ADC Input Control (I1) */
				return ADCInputControl[1];
			case 0x02: /* Left Auxiliary #1 Input Control (I2) */
				return Aux1InputControl[0];
			case 0x03: /* Right Auxiliary #1 Input Control (I3) */
				return Aux1InputControl[1];
			case 0x06: /* Left DAC Output Control (I6) */
				return DACOutputControl[0];
			case 0x07: /* Left DAC Output Control (I7) */
				return DACOutputControl[1];
			case 0x0C: /* MODE and ID (I12) */
				return 0x80 | (mode2 ? 0x40 : 0x00) | 0xA/*1010 codec ID*/;
			default:
				LOG(LOG_MISC,LOG_DEBUG)("GUS CS4231 unhandled data read addr=%02xh",addr);
				break;
		}

		return 0;
	}
	void playio_data_write(uint8_t val) {
		LOG(LOG_MISC,LOG_DEBUG)("GUS CS4231 Playback I/O write %02xh",val);
	}
	uint8_t capio_data_read(void) {
		LOG(LOG_MISC,LOG_DEBUG)("GUS CS4231 Capture I/O read");
		return 0;
	}
	uint8_t status_read(void) {
		LOG(LOG_MISC,LOG_DEBUG)("GUS CS4231 Status read");
		return 0;
	}
	void iowrite(uint8_t reg,uint8_t val) {
//		LOG(LOG_MISC,LOG_DEBUG)("GUS CS4231 write reg=%u val=%02xh",reg,val);

		if (init) return;

		switch (reg) {
			case 0x0: /* Index Address Register (R0) */
				address = val & (mode2 ? 0x1F : 0x0F);
				trd = (val & 0x20)?1:0;
				mce = (val & 0x40)?1:0;
				break;
			case 0x1: /* Index Data Register (R1) */
				data_write(address,val);
				break;
			case 0x2: /* Status Register (R2) */
				LOG(LOG_MISC,LOG_DEBUG)("GUS CS4231 attempted write to status register val=%02xh",val);
				break;
			case 0x3: /* Playback I/O Data Register (R3) */
				playio_data_write(val);
				break;
		}
	}
	uint8_t ioread(uint8_t reg) {
//		LOG(LOG_MISC,LOG_DEBUG)("GUS CS4231 write read=%u",reg);

		if (init) return 0x80;

		switch (reg) {
			case 0x0: /* Index Address Register (R0) */
				return address | (trd?0x20:0x00) | (mce?0x40:0x00) | (init?0x80:0x00);
			case 0x1: /* Index Data Register (R1) */
				return data_read(address);
			case 0x2: /* Status Register (R2) */
				return status_read();
			case 0x3: /* Capture I/O Data Register (R3) */
				return capio_data_read();
		}

		return 0;
	}
public:
	uint8_t		address;
	bool		mode2; // read CS4231A datasheet for more information
	bool		ia4;
	bool		trd;
	bool		mce;
	bool		init;

	uint8_t		ADCInputControl[2];	/* left (I0) and right (I1) ADC Input control. bits 7-6 select source. bit 5 is mic gain. bits 3-0 controls gain. */
	uint8_t		Aux1InputControl[2];	/* left (I2) and right (I3) aux. input control. bits 5-0 control gain in 1.5dB steps. bit 7 is mute */
	uint8_t		DACOutputControl[2];	/* left (I6) and right (I7) output control attenuation. bits 5-0 control in -1.5dB steps, bit 7 is mute */
} GUS_CS4231;

static Bitu read_gus_cs4231(Bitu port,Bitu iolen) {
	if (myGUS.gUltraMAXControl & 0x40/*codec enable*/)
		return GUS_CS4231.ioread((port - GUS_BASE) & 3); // FIXME: UltraMAX allows this to be relocatable

	return 0xFF;
}

static void write_gus_cs4231(Bitu port,Bitu val,Bitu iolen) {
	if (myGUS.gUltraMAXControl & 0x40/*codec enable*/)
		GUS_CS4231.iowrite((port - GUS_BASE) & 3,val&0xFF);
}

static Bitu read_gus(Bitu port,Bitu iolen) {
	Bit16u reg16;

//	LOG_MSG("read from gus port %x",port);
	switch(port - GUS_BASE) {
	case 0x206:
		if (myGUS.clearTCIfPollingIRQStatus) {
			double t = PIC_FullIndex();

			/* Hack: "Warcraft II" by Blizzard entertainment.
			 *
			 * If you configure the game to use the Gravis Ultrasound for both music and sound, the GUS support code for digital
			 * sound will cause the game to hang if music is playing at the same time within the main menu. The bug is that there
			 * is code (in real mode no less) that polls the IRQ status register (2X6) and handles each IRQ event to clear it,
			 * however there is no condition to handle clearing the DMA Terminal Count IRQ flag. The routine will not terminate
			 * until all bits are cleared, hence, the game hangs after starting a sound effect. Most often, this is visible to
			 * the user as causing the game to hang when you click on a button on the main menu.
			 *
			 * This hack attempts to detect the bug by looking for rapid polling of this register in a short period of time.
			 * If detected, we clear the DMA IRQ flag to help the loop terminate so the game continues to run. */
			if (t < (myGUS.lastIRQStatusPollAt + 0.1/*ms*/)) {
				myGUS.lastIRQStatusPollAt = t;
				myGUS.lastIRQStatusPollRapidCount++;
				if (myGUS.clearTCIfPollingIRQStatus && (myGUS.IRQStatus & 0x80) && myGUS.lastIRQStatusPollRapidCount >= 500) {
					LOG(LOG_MISC,LOG_DEBUG)("GUS: Clearing DMA TC IRQ status, DOS application appears to be stuck");
					myGUS.lastIRQStatusPollRapidCount = 0;
					myGUS.lastIRQStatusPollAt = t;
					myGUS.IRQStatus &= 0x7F;
					GUS_CheckIRQ();
				}
			}
			else {
				myGUS.lastIRQStatusPollAt = t;
				myGUS.lastIRQStatusPollRapidCount = 0;
			}
		}

		/* NTS: Contrary to some false impressions, GUS hardware does not report "one at a time", it really is a bitmask.
		 *      I had the funny idea you read this register "one at a time" just like reading the IRQ reason bits of the RS-232 port --J.C. */
		return GUS_EffectiveIRQStatus();
	case 0x208:
		Bit8u tmptime;
		tmptime = 0;
		if (myGUS.timers[0].reached) tmptime |= (1 << 6);
		if (myGUS.timers[1].reached) tmptime |= (1 << 5);
		if (tmptime & 0x60) tmptime |= (1 << 7);
		if (myGUS.IRQStatus & 0x04) tmptime|=(1 << 2);
		if (myGUS.IRQStatus & 0x08) tmptime|=(1 << 1);
		return tmptime;
	case 0x20a:
		return adlib_commandreg;
	case 0x20f:
		if (gus_type >= GUS_MAX || gus_ics_mixer)
			return 0x02; /* <- FIXME: What my GUS MAX returns. What does this mean? */
		return ~0; // should not happen
	case 0x302:
		return myGUS.gRegSelectData;
	case 0x303:
		return myGUS.gRegSelectData;
	case 0x304:
		if (iolen==2) reg16 = ExecuteReadRegister() & 0xffff;
		else reg16 = ExecuteReadRegister() & 0xff;

		if (gus_type < GUS_INTERWAVE) // Versions prior to the Interwave will reflect last I/O to 3X2-3X5 when read back from 3X3
			myGUS.gRegSelectData = reg16 & 0xFF;

		return reg16;
	case 0x305:
		reg16 = ExecuteReadRegister() >> 8;

		if (gus_type < GUS_INTERWAVE) // Versions prior to the Interwave will reflect last I/O to 3X2-3X5 when read back from 3X3
			myGUS.gRegSelectData = reg16 & 0xFF;

		return reg16;
	case 0x307:
		if(myGUS.gDramAddr < myGUS.memsize) {
			return GUSRam[myGUS.gDramAddr];
		} else {
			return 0;
		}
	case 0x306:
	case 0x706:
		if (gus_type >= GUS_MAX)
			return 0x0B; /* UltraMax with CS4231 codec */
		else if (gus_ics_mixer)
			return 0x06; /* revision 3.7+ with ICS-2101 mixer */
		else
			return 0xFF;
		break;
	default:
#if LOG_GUS
		LOG_MSG("Read GUS at port 0x%x", port);
#endif
		break;
	}

	return 0xff;
}


static void write_gus(Bitu port,Bitu val,Bitu iolen) {
//	LOG_MSG("Write gus port %x val %x",port,val);
	switch(port - GUS_BASE) {
	case 0x200:
		myGUS.gRegControl = 0;
		myGUS.mixControl = (Bit8u)val;
		myGUS.ChangeIRQDMA = true;
		return;
	case 0x208:
		adlib_commandreg = (Bit8u)val;
		break;
	case 0x209:
//TODO adlib_commandreg should be 4 for this to work else it should just latch the value
		if (val & 0x80) {
			myGUS.timers[0].reached=false;
			myGUS.timers[1].reached=false;
			return;
		}
		myGUS.timers[0].masked=(val & 0x40)>0;
		myGUS.timers[1].masked=(val & 0x20)>0;
		if (val & 0x1) {
			if (!myGUS.timers[0].running) {
				PIC_AddEvent(GUS_TimerEvent,myGUS.timers[0].delay,0);
				myGUS.timers[0].running=true;
			}
		} else myGUS.timers[0].running=false;
		if (val & 0x2) {
			if (!myGUS.timers[1].running) {
				PIC_AddEvent(GUS_TimerEvent,myGUS.timers[1].delay,1);
				myGUS.timers[1].running=true;
			}
		} else myGUS.timers[1].running=false;
		break;
//TODO Check if 0x20a register is also available on the gus like on the interwave
	case 0x20b:
		if (!myGUS.ChangeIRQDMA) break;
		myGUS.ChangeIRQDMA=false;

		if (myGUS.gRegControl == 6) {
			/* Jumper register:
			 *
			 * 7: reserved
			 * 6: reserved
			 * 5: reserved
			 * 4: reserved
			 * 3: reserved
			 * 2: enable joystick port decode
			 * 1: enable midi port address decode
			 * 0: reserved */
			/* TODO: */
			LOG(LOG_MISC,LOG_DEBUG)("GUS: DOS application wrote to 2XB register control number 0x06 (Jumper) val=%02xh",(int)val);
		}
		else if (myGUS.gRegControl == 5) {
			/* write a 0 to clear IRQs on power up (?) */
			LOG(LOG_MISC,LOG_DEBUG)("GUS: DOS application wrote to 2XB register control number 0x05 (Clear IRQs) val=%02xh",(int)val);
		}
		else if (myGUS.gRegControl == 0) {
			if (myGUS.mixControl & 0x40) {
				// GUS SDK: IRQ Control Register
				//     Channel 1 GF1 IRQ selector (bits 2-0)
				//       0=reserved, do not use
				//       1=IRQ2
				//       2=IRQ5
				//       3=IRQ3
				//       4=IRQ7
				//       5=IRQ11
				//       6=IRQ12
				//       7=IRQ15
				//     Channel 2 MIDI IRQ selector (bits 5-3)
				//       0=no interrupt
				//       1=IRQ2
				//       2=IRQ5
				//       3=IRQ3
				//       4=IRQ7
				//       5=IRQ11
				//       6=IRQ12
				//       7=IRQ15
				//     Combine both IRQs using channel 1 (bit 6)
				//     Reserved (bit 7)
				//
				//     "If both channels are sharing an IRQ, channel 2's IRQ must be set to 0 and turn on bit 6. A
				//      bus conflict will occur if both latches are programmed with the same IRQ #."
				if (irqtable[val & 0x7] != 0)
					myGUS.irq1 = irqtable[val & 0x7];

				if (val & 0x40) // "Combine both IRQs"
					myGUS.irq2 = myGUS.irq1;
				else if (((val >> 3) & 7) != 0)
					myGUS.irq2 = irqtable[(val >> 3) & 0x7];

				LOG(LOG_MISC,LOG_DEBUG)("GUS IRQ reprogrammed: GF1 IRQ %d, MIDI IRQ %d",(int)myGUS.irq1,(int)myGUS.irq2);

				if (!(val & 0x40) && (val & 7) == ((val >> 3) & 7))
					LOG(LOG_MISC,LOG_WARN)("GUS warning: Both IRQs set to the same signal line WITHOUT combining! This is documented to cause bus conflicts on real hardware");
			} else {
				// GUS SDK: DMA Control Register
				//     Channel 1 (bits 2-0)
				//       0=NO DMA
				//       1=DMA1
				//       2=DMA3
				//       3=DMA5
				//       4=DMA6
				//       5=DMA7
				//       6=?
				//       7=?
				//     Channel 2 (bits 5-3)
				//       0=NO DMA
				//       1=DMA1
				//       2=DMA3
				//       3=DMA5
				//       4=DMA6
				//       5=DMA7
				//       6=?
				//       7=?
				//     Combine both DMA channels using channel 1 (bit 6)
				//     Reserved (bit 7)
				//
				//     "If both channels are sharing an DMA, channel 2's DMA must be set to 0 and turn on bit 6. A
				//      bus conflict will occur if both latches are programmed with the same DMA #."

				// NTS: This emulation does not use the second DMA channel... yet... which is why we do not bother
				//      unregistering and reregistering the second DMA channel.

				GetDMAChannel(myGUS.dma1)->Register_Callback(0); // FIXME: On DMA conflict this could mean kicking other devices off!

				// NTS: Contrary to an earlier commit, DMA channel 0 is not a valid choice
				if (dmatable[val & 0x7] != 0)
					myGUS.dma1 = dmatable[val & 0x7];

				if (val & 0x40) // "Combine both DMA channels"
					myGUS.dma2 = myGUS.dma1;
				else if (dmatable[(val >> 3) & 0x7] != 0)
					myGUS.dma2 = dmatable[(val >> 3) & 0x7];

				GetDMAChannel(myGUS.dma1)->Register_Callback(GUS_DMA_Callback);

				LOG(LOG_MISC,LOG_DEBUG)("GUS DMA reprogrammed: DMA1 %d, DMA2 %d",(int)myGUS.dma1,(int)myGUS.dma2);

				// NTS: The Windows 3.1 Gravis Ultrasound drivers will program the same DMA channel into both without setting the "combining" bit,
				//      even though their own SDK says not to, when Windows starts up. But it then immediately reprograms it normally, so no bus
				//      conflicts actually occur. Strange.
				if (!(val & 0x40) && (val & 7) == ((val >> 3) & 7))
					LOG(LOG_MISC,LOG_WARN)("GUS warning: Both DMA channels set to the same channel WITHOUT combining! This is documented to cause bus conflicts on real hardware");
			}
		}
		else {
			LOG(LOG_MISC,LOG_DEBUG)("GUS warning: Port 2XB register control %02xh written (unknown control reg) val=%02xh",(int)myGUS.gRegControl,(int)val);
		}
		break;
	case 0x20f:
		if (gus_type >= GUS_MAX || gus_ics_mixer) {
			myGUS.gRegControl = val;
			myGUS.ChangeIRQDMA = true;
		}
		break;
	case 0x302:
		myGUS.gCurChannel = val & 31;
		if (gus_type < GUS_INTERWAVE) // Versions prior to the Interwave will reflect last I/O to 3X2-3X5 when read back from 3X3
			myGUS.gRegSelectData = (Bit8u)val;

		curchan = guschan[myGUS.gCurChannel];
		break;
	case 0x303:
		myGUS.gRegSelect = myGUS.gRegSelectData = (Bit8u)val;
		myGUS.gRegData = 0;
		break;
	case 0x304:
		if (iolen==2) {
			if (gus_type < GUS_INTERWAVE) // Versions prior to the Interwave will reflect last I/O to 3X2-3X5 when read back from 3X3
				myGUS.gRegSelectData = val & 0xFF;

			myGUS.gRegData=(Bit16u)val;
			ExecuteGlobRegister();
		} else {
			if (gus_type < GUS_INTERWAVE) // Versions prior to the Interwave will reflect last I/O to 3X2-3X5 when read back from 3X3
				myGUS.gRegSelectData = val;

			myGUS.gRegData = (Bit16u)val;
		}
		break;
	case 0x305:
		if (gus_type < GUS_INTERWAVE) // Versions prior to the Interwave will reflect last I/O to 3X2-3X5 when read back from 3X3
			myGUS.gRegSelectData = val;

		myGUS.gRegData = (Bit16u)((0x00ff & myGUS.gRegData) | val << 8);
		ExecuteGlobRegister();
		break;
	case 0x307:
		if(myGUS.gDramAddr < myGUS.memsize) GUSRam[myGUS.gDramAddr] = (Bit8u)val;
		break;
	case 0x306:
	case 0x706:
		if (gus_type >= GUS_MAX) {
			/* Ultramax control register:
			 *
			 * bit 7: reserved
			 * bit 6: codec enable
			 * bit 5: playback channel type (1=16-bit 0=8-bit)
			 * bit 4: capture channel type (1=16-bit 0=8-bit)
			 * bits 3-0: Codec I/O port address decode bits 7-4.
			 *
			 * For example, to put the CS4231 codec at port 0x34C, and enable the codec, write 0x44 to this register.
			 * If you want to move the codec to base I/O port 0x32C, write 0x42 here. */
			myGUS.gUltraMAXControl = val;

			if (val & 0x40) {
				if ((val & 0xF) != ((port >> 4) & 0xF))
					LOG(LOG_MISC,LOG_WARN)("GUS WARNING: DOS application is attempting to relocate the CS4231 codec, which is not supported");
			}
		}
		else if (gus_ics_mixer) {
			if ((port - GUS_BASE) == 0x306)
				GUS_ICS2101.dataWrite(val&0xFF);
			else if ((port - GUS_BASE) == 0x706)
				GUS_ICS2101.addressWrite(val&0xFF);
		}
		break;
	default:
#if LOG_GUS
		LOG_MSG("Write GUS at port 0x%x with %x", port, val);
#endif
		break;
	}
}

static Bitu GUS_Master_Clock = 617400; /* NOTE: This is 1000000Hz / 1.619695497. Seems to be a common base rate within the hardware. */
static Bitu GUS_DMA_Event_transfer = 16; /* DMA words (8 or 16-bit) per interval */
static Bitu GUS_DMA_Events_per_sec = 44100 / 4; /* cheat a little, to improve emulation performance */
static double GUS_DMA_Event_interval = 1000.0 / GUS_DMA_Events_per_sec;
static double GUS_DMA_Event_interval_init = 1000.0 / 44100;
static bool GUS_DMA_Active = false;

void GUS_Update_DMA_Event_transfer() {
	/* NTS: From the GUS SDK, bits 3-4 of DMA Control divide the ISA DMA transfer rate down from "approx 650KHz".
	 *      Bits 3-4 are documented as "DMA Rate divisor" */
	GUS_DMA_Event_transfer = GUS_Master_Clock / GUS_DMA_Events_per_sec / (((myGUS.DMAControl >> 3) & 3) + 1);
	GUS_DMA_Event_transfer &= ~1; /* make sure it's word aligned in case of 16-bit PCM */
	if (GUS_DMA_Event_transfer == 0) GUS_DMA_Event_transfer = 2;
}

void GUS_DMA_Event_Transfer(DmaChannel *chan,Bitu dmawords) {
	Bitu dmaaddr = (myGUS.dmaAddr << 4) + myGUS.dmaAddrOffset;
	Bitu dmalimit = myGUS.memsize;
	int step = 0,docount = 0;
	bool dma16xlate;
	Bitu holdAddr;

	// FIXME: What does the GUS do if the DMA address goes beyond the end of memory?

	/* is this a DMA transfer where the GUS memory address is to be translated for 16-bit PCM?
	 * Note that there is plenty of code out there that transfers 16-bit PCM with 8-bit DMA,
	 * and such transfers DO NOT involve the memory address translation.
	 *
	 * MODE    XLATE?    NOTE
	 * 0x00    NO        8-bit PCM, 8-bit DMA (Most DOS programs)
	 * 0x04    DEPENDS   8-bit PCM, 16-bit DMA (Star Control II, see comments below)
	 * 0x40    NO        16-bit PCM, 8-bit DMA (Windows 3.1, Quake, for 16-bit PCM over 8-bit DMA)
	 * 0x44    YES       16-bit PCM, 16-bit DMA (Windows 3.1, Quake, for 16-bit PCM over 16-bit DMA)
	 *
	 * Mode 0x04 is marked DEPENDS. It DEPENDS on whether the assigned DMA channel is 8-bit (NO) or 16-bit (YES).
	 * Mode 0x04 does not appear to be used by any other DOS application or Windows drivers, only by an erratic
	 * bug in Star Control II. FIXME: But, what does REAL hardware do? Drag out the old Pentium 100MHz with the
	 * GUS classic and test! --J.C.
	 *
	 * Star Control II has a bug where, if the GUS DMA channel is 8-bit (DMA channel 0-3), it will upload
	 * it's samples to GUS RAM, one DMA transfer per sample, and sometimes set bit 2. Setting bit 2 incorrectly
	 * tells the GUS it's a 16-bit wide DMA transfer when the DMA channel is 8-bit. But, if the DMA channel is
	 * 16-bit (DMA channel 5-7), Star Control II will correctly set bit 2 in all cases and samples will
	 * transfer correctly to GUS RAM.
	 * */

	/* FIXME: So, if the GUS DMA channel is 8-bit and Star Control II writes mode 0x04 (8-bit PCM, 16-bit DMA), what does the GF1 do?
	 *        I'm guessing so far that something happens within the GF1 to transfer as 8-bit anyway, clearly the developers of Star Control II
	 *        did not hear any audible sign that an invalid DMA control was being used. Perhaps the hardware engineers of the GF1 figured
	 *        out that case and put something in the silicon to ignore the invalid DMA control state. I won't have any answers until
	 *        I pull out an old Pentium box with a GUS classic and check. --J.C.
	 *
	 *        DMA transfers noted by Star Control II that are the reason for this hack (gusdma=1):
	 *
	 *        LOG:  157098507 DEBUG MISC:GUS DMA: terminal count reached. DMAControl=0x21
	 *        LOG:  157098507 DEBUG MISC:GUS DMA transfer 1981 bytes, GUS RAM address 0x0 8-bit DMA 8-bit PCM (ctrl=0x21)
	 *        LOG:  157098507 DEBUG MISC:GUS DMA: terminal count reached. DMAControl=0x21
	 *        LOG:  157100331 DEBUG MISC:GUS DMA: terminal count reached. DMAControl=0x21
	 *        LOG:  157100331 DEBUG MISC:GUS DMA transfer 912 bytes, GUS RAM address 0x7c0 8-bit DMA 8-bit PCM (ctrl=0x21)
	 *        LOG:  157100331 DEBUG MISC:GUS DMA: terminal count reached. DMAControl=0x21
	 *        LOG:  157100470 DEBUG MISC:GUS DMA: terminal count reached. DMAControl=0x25
	 *        LOG:  157100470 DEBUG MISC:GUS DMA transfer 1053 bytes, GUS RAM address 0xb50 16-bit DMA 8-bit PCM (ctrl=0x25)    <-- What?
	 *        LOG:  157100470 DEBUG MISC:GUS DMA: terminal count reached. DMAControl=0x25
	 *        LOG:  157102251 DEBUG MISC:GUS DMA: terminal count reached. DMAControl=0x21
	 *        LOG:  157102251 DEBUG MISC:GUS DMA transfer 1597 bytes, GUS RAM address 0xf80 8-bit DMA 8-bit PCM (ctrl=0x21)
	 *        LOG:  157102251 DEBUG MISC:GUS DMA: terminal count reached. DMAControl=0x21
	 *        LOG:  157104064 DEBUG MISC:GUS DMA: terminal count reached. DMAControl=0x21
	 *        LOG:  157104064 DEBUG MISC:GUS DMA transfer 2413 bytes, GUS RAM address 0x15c0 8-bit DMA 8-bit PCM (ctrl=0x21)
	 *        LOG:  157104064 DEBUG MISC:GUS DMA: terminal count reached. DMAControl=0x21
	 *
	 *        (end list)
	 *
	 *        Noted: Prior to this hack, the samples played by Star Control II sounded more random and often involved leftover
	 *               sample data in GUS RAM, where with this fix, the music now sounds identical to what is played when using
	 *               it's Sound Blaster support. */
	if (myGUS.dma1 < 4/*8-bit DMA channel*/ && (myGUS.DMAControl & 0x44) == 0x04/*8-bit PCM, 16-bit DMA*/)
		dma16xlate = false; /* Star Control II hack: 8-bit PCM, 8-bit DMA, ignore the bit that says it's 16-bit wide */
	else
		dma16xlate = (myGUS.DMAControl & 0x4) ? true : false;

	if (dma16xlate) {
		// 16-bit wide DMA. The GUS SDK specifically mentions that 16-bit DMA is translated
		// to GUS RAM the same way you translate the play pointer. Eugh. But this allows
		// older demos to work properly even if you set the GUS DMA to a 16-bit channel (5)
		// instead of the usual 8-bit channel (1).
		holdAddr = dmaaddr & 0xc0000L;
		dmaaddr = dmaaddr & 0x1ffffL;
		dmaaddr = dmaaddr << 1;
		dmaaddr = (holdAddr | dmaaddr);
		dmalimit = ((dmaaddr & 0xc0000L) | 0x3FFFFL) + 1;
	}

	if (dmaaddr < dmalimit)
		docount = dmalimit - dmaaddr;

	docount /= (chan->DMA16+1);
	if (docount > (chan->currcnt+1)) docount = chan->currcnt+1;
	if ((Bitu)docount > dmawords) docount = dmawords;

	// hack: some programs especially Gravis Ultrasound MIDI playback like to upload by DMA but never clear the DMA TC flag on the DMA controller.
	bool saved_tcount = chan->tcount;
	chan->tcount = false;

	if (docount > 0) {
		if ((myGUS.DMAControl & 0x2) == 0) {
			Bitu read=chan->Read(docount,&GUSRam[dmaaddr]);
			//Check for 16 or 8bit channel
			read*=(chan->DMA16+1);
			if((myGUS.DMAControl & 0x80) != 0) {
				//Invert the MSB to convert twos compliment form
				Bitu i;
				if((myGUS.DMAControl & 0x40) == 0) {
					// 8-bit data
					for(i=dmaaddr;i<(dmaaddr+read);i++) GUSRam[i] ^= 0x80;
				} else {
					// 16-bit data
					for(i=dmaaddr+1;i<(dmaaddr+read);i+=2) GUSRam[i] ^= 0x80;
				}
			}

			step = read;
		} else {
			//Read data out of UltraSound
			int wd = chan->Write(docount,&GUSRam[dmaaddr]);
			//Check for 16 or 8bit channel
			wd*=(chan->DMA16+1);

			step = wd;
		}
	}

	LOG(LOG_MISC,LOG_DEBUG)("GUS DMA transfer %lu bytes, GUS RAM address 0x%lx %u-bit DMA %u-bit PCM (ctrl=0x%02x) tcount=%u",
		(unsigned long)step,(unsigned long)dmaaddr,myGUS.DMAControl & 0x4 ? 16 : 8,myGUS.DMAControl & 0x40 ? 16 : 8,myGUS.DMAControl,chan->tcount);

	if (step > 0) {
		dmaaddr += (unsigned int)step;

		if (dma16xlate) {
			holdAddr = dmaaddr & 0xc0000L;
			dmaaddr = dmaaddr & 0x3ffffL;
			dmaaddr = dmaaddr >> 1;
			dmaaddr = (holdAddr | dmaaddr);
		}

		myGUS.dmaAddr = dmaaddr >> 4;
		myGUS.dmaAddrOffset = dmaaddr & 0xF;
	}

	if (chan->tcount) {
		LOG(LOG_MISC,LOG_DEBUG)("GUS DMA transfer hit Terminal Count, setting DMA TC IRQ pending");

		/* Raise the TC irq, and stop DMA */
		myGUS.IRQStatus |= 0x80;
		saved_tcount = true;
		GUS_CheckIRQ();
		GUS_StopDMA();
	}

	chan->tcount = saved_tcount;
}

void GUS_DMA_Event(Bitu val) {
	DmaChannel *chan = GetDMAChannel(myGUS.dma1);
	if (chan == NULL) {
		LOG(LOG_MISC,LOG_DEBUG)("GUS DMA event: DMA channel no longer exists, stopping DMA transfer events");
		GUS_DMA_Active = false;
		return;
	}

	if (chan->masked) {
		LOG(LOG_MISC,LOG_DEBUG)("GUS: Stopping DMA transfer interval, DMA masked=%u",chan->masked?1:0);
		GUS_DMA_Active = false;
		return;
	}

	if (!(myGUS.DMAControl & 0x01/*DMA enable*/)) {
		LOG(LOG_MISC,LOG_DEBUG)("GUS DMA event: DMA control 'enable DMA' bit was reset, stopping DMA transfer events");
		GUS_DMA_Active = false;
		return;
	}

	LOG(LOG_MISC,LOG_DEBUG)("GUS DMA event: max %u DMA words. DMA: tc=%u mask=%u cnt=%u",
		(unsigned int)GUS_DMA_Event_transfer,
		chan->tcount?1:0,
		chan->masked?1:0,
		chan->currcnt+1);
	GUS_DMA_Event_Transfer(chan,GUS_DMA_Event_transfer);

	if (GUS_DMA_Active) {
		/* keep going */
		PIC_AddEvent(GUS_DMA_Event,GUS_DMA_Event_interval);
	}
}

void GUS_StopDMA() {
	if (GUS_DMA_Active)
		LOG(LOG_MISC,LOG_DEBUG)("GUS: Stopping DMA transfer interval");

	PIC_RemoveEvents(GUS_DMA_Event);
	GUS_DMA_Active = false;
}

void GUS_StartDMA() {
	if (!GUS_DMA_Active) {
		GUS_DMA_Active = true;
		LOG(LOG_MISC,LOG_DEBUG)("GUS: Starting DMA transfer interval");
		PIC_AddEvent(GUS_DMA_Event,GUS_DMA_Event_interval_init);
	}
}

static void GUS_DMA_Callback(DmaChannel * chan,DMAEvent event) {
	if (event == DMA_UNMASKED) {
		LOG(LOG_MISC,LOG_DEBUG)("GUS: DMA unmasked");
		if (myGUS.DMAControl & 0x01/*DMA enable*/) GUS_StartDMA();
	}
	else if (event == DMA_MASKED) {
		LOG(LOG_MISC,LOG_DEBUG)("GUS: DMA masked. Perhaps it will stop the DMA transfer event.");
	}
}

static void GUS_CallBack(Bitu len) {
	memset(&MixTemp,0,len*8);
	Bitu i;
	Bit16s * buf16 = (Bit16s *)MixTemp;
	Bit32s * buf32 = (Bit32s *)MixTemp;

	if ((GUS_reset_reg & 0x01/*!master reset*/) == 0x01) {
		for(i=0;i<myGUS.ActiveChannels;i++)
			guschan[i]->generateSamples(buf32,len);
	}

	for(i=0;i<len*2;i++) {
		Bit32s sample=((buf32[i] >> 13)*AutoAmp)>>9;
		if (sample>32767) {
			sample=32767;                       
			AutoAmp--;
		} else if (sample<-32768) {
			sample=-32768;
			AutoAmp--;
		}
		buf16[i] = (Bit16s)(sample);
	}
	gus_chan->AddSamples_s16(len,buf16);
	CheckVoiceIrq();
}

// Generate logarithmic to linear volume conversion tables
static void MakeTables(void) {
	int i;
	double out = (double)(1 << 13);
	for (i=4095;i>=0;i--) {
		vol16bit[i]=(Bit16s)out;
		out/=1.002709201;		/* 0.0235 dB Steps */
	}
	/* FIX: DOSBox 0.74 had code here that produced a pantable which
	 *      had nothing to do with actual panning control variables.
	 *      Instead it seemed to generate a 16-element map that started
	 *      at 0, jumped sharply to unity and decayed to 0.
	 *      The unfortunate result was that stock builds of DOSBox
	 *      effectively locked Gravis Ultrasound capable programs
	 *      to monural audio.
	 *
	 *      This fix generates the table properly so that they correspond
	 *      to how much we attenuate the LEFT channel for any given
	 *      4-bit value of the Panning register (you attenuate the
	 *      RIGHT channel by looking at element 0xF - (val&0xF)).
	 *
	 *      Having made this fix I can finally enjoy old DOS demos
	 *      in GUS stereo instead of having everything mushed into
	 *      mono. */
	if (gus_fixed_table) {
		for (i=0;i < 16;i++)
			pantable[i] = pantablePDF[i] * 2048;

		LOG(LOG_MISC,LOG_DEBUG)("GUS: using accurate (fixed) pantable");
	}
	else {
		for (i=0;i < 8;i++)
			pantable[i] = 0;
		for (i=8;i < 15;i++)
			pantable[i]=(Bit32u)(-128.0*(log((double)(15-i)/7.0)/log(2.0))*(double)(1 << RAMP_FRACT));

		/* if the program cranks the pan register all the way, ensure the
		 * opposite channel is crushed to silence */
		pantable[15] = 1UL << 30UL;

		LOG(LOG_MISC,LOG_DEBUG)("GUS: using old (naive) pantable");
	}

	LOG(LOG_MISC,LOG_DEBUG)("GUS pantable (attenuation, left to right in dB): hard left -%.3f, -%.3f, -%.3f, -%.3f, -%.3f, -%.3f, -%.3f, center(7) -%.3f, center(8) -%.3f, -%.3f, -%.3f, -%.3f, -%.3f, -%.3f, -%.3f, hard right -%.3f",
		((double)pantable[0]) / (1 << RAMP_FRACT),
		((double)pantable[1]) / (1 << RAMP_FRACT),
		((double)pantable[2]) / (1 << RAMP_FRACT),
		((double)pantable[3]) / (1 << RAMP_FRACT),
		((double)pantable[4]) / (1 << RAMP_FRACT),
		((double)pantable[5]) / (1 << RAMP_FRACT),
		((double)pantable[6]) / (1 << RAMP_FRACT),
		((double)pantable[7]) / (1 << RAMP_FRACT),
		((double)pantable[8]) / (1 << RAMP_FRACT),
		((double)pantable[9]) / (1 << RAMP_FRACT),
		((double)pantable[10]) / (1 << RAMP_FRACT),
		((double)pantable[11]) / (1 << RAMP_FRACT),
		((double)pantable[12]) / (1 << RAMP_FRACT),
		((double)pantable[13]) / (1 << RAMP_FRACT),
		((double)pantable[14]) / (1 << RAMP_FRACT),
		((double)pantable[15]) / (1 << RAMP_FRACT));
}

class GUS:public Module_base{
private:
	IO_ReadHandleObject ReadHandler[12];
	IO_WriteHandleObject WriteHandler[12];
	IO_ReadHandleObject ReadCS4231Handler[4];
	IO_WriteHandleObject WriteCS4231Handler[4];
	AutoexecObject autoexecline[3];
	MixerObject MixerChan;
public:
	GUS(Section* configuration):Module_base(configuration){
		int x;

		if(!IS_EGAVGA_ARCH) return;
		Section_prop * section=static_cast<Section_prop *>(configuration);
		if(!section->Get_bool("gus")) return;
	
		memset(&myGUS,0,sizeof(myGUS));
		memset(GUSRam,0,1024*1024);

		string s_pantable = section->Get_string("gus panning table");
		if (s_pantable == "default" || s_pantable == "" || s_pantable == "accurate")
			gus_fixed_table = true;
		else if (s_pantable == "old")
			gus_fixed_table = false;
		else
			gus_fixed_table = true;

		gus_ics_mixer = false;
		string s_gustype = section->Get_string("gustype");
		if (s_gustype == "classic") {
			LOG(LOG_MISC,LOG_DEBUG)("GUS: Classic emulation");
			gus_type = GUS_CLASSIC;
		}
		else if (s_gustype == "classic37") {
			LOG(LOG_MISC,LOG_DEBUG)("GUS: Classic emulation");
			gus_type = GUS_CLASSIC;
			gus_ics_mixer = true;
		}
		else if (s_gustype == "max") {
			// UltraMAX cards do not have the ICS mixer
			LOG(LOG_MISC,LOG_DEBUG)("GUS: MAX emulation");
			gus_type = GUS_MAX;
		}
		else if (s_gustype == "interwave") {
			// Neither do Interwave cards
			LOG(LOG_MISC,LOG_DEBUG)("GUS: Interwave PnP emulation");
			gus_type = GUS_INTERWAVE;
		}
		else {
			LOG(LOG_MISC,LOG_DEBUG)("GUS: Classic emulation by default");
			gus_type = GUS_CLASSIC;
		}

		myGUS.clearTCIfPollingIRQStatus = section->Get_bool("clear dma tc irq if excess polling");
		if (myGUS.clearTCIfPollingIRQStatus)
			LOG(LOG_MISC,LOG_DEBUG)("GUS: Will clear DMA TC IRQ if excess polling, as instructed");

		myGUS.gUltraMAXControl = 0;
		myGUS.lastIRQStatusPollRapidCount = 0;
		myGUS.lastIRQStatusPollAt = 0;

		myGUS.initUnmaskDMA = section->Get_bool("unmask dma");
		if (myGUS.initUnmaskDMA)
			LOG(LOG_MISC,LOG_DEBUG)("GUS: Unmasking DMA at boot time as requested");

		myGUS.fixed_sample_rate_output = section->Get_bool("gus fixed render rate");
		LOG(LOG_MISC,LOG_DEBUG)("GUS: using %s sample rate output",myGUS.fixed_sample_rate_output?"fixed":"realistic");

		myGUS.force_master_irq_enable=section->Get_bool("force master irq enable");
		if (myGUS.force_master_irq_enable)
			LOG(LOG_MISC,LOG_DEBUG)("GUS: Master IRQ enable will be forced on as instructed");

		myGUS.rate=section->Get_int("gusrate");

		x = section->Get_int("gusmemsize");
		if (x >= 0) myGUS.memsize = x*1024;
		else myGUS.memsize = 1024*1024;

		if (myGUS.memsize > (1024*1024))
			myGUS.memsize = (1024*1024);

		if ((myGUS.memsize&((256 << 10) - 1)) != 0)
			LOG(LOG_MISC,LOG_WARN)("GUS emulation warning: %uKB onboard is an unusual value. Usually GUS cards have some multiple of 256KB RAM onboard",myGUS.memsize>>10);

		LOG(LOG_MISC,LOG_DEBUG)("GUS emulation: %uKB onboard",myGUS.memsize>>10);

		// FIXME: HUH?? Read the port number and subtract 0x200, then use GUS_BASE
		// in other parts of the code to compare against 0x200 and 0x300? That's confusing. Fix!
		myGUS.portbase = section->Get_hex("gusbase") - 0x200;

		// TODO: so, if the GUS ULTRASND variable actually mentions two DMA and two IRQ channels,
		//       shouldn't we offer the ability to specify them independently? especially when
		//       GUS NMI is completed to the extent that SBOS and MEGA-EM can run within DOSBox?
		int dma_val = section->Get_int("gusdma");
		if ((dma_val<0) || (dma_val>255)) dma_val = 3;	// sensible default

		int irq_val = section->Get_int("gusirq");
		if ((irq_val<0) || (irq_val>255)) irq_val = 5;	// sensible default

		myGUS.dma1 = (Bit8u)dma_val;
		myGUS.dma2 = (Bit8u)dma_val;
		myGUS.irq1 = (Bit8u)irq_val;
		myGUS.irq2 = (Bit8u)irq_val;

		// We'll leave the MIDI interface to the MPU-401 
		// Ditto for the Joystick 
		// GF1 Synthesizer 
		ReadHandler[0].Install(0x302 + GUS_BASE,read_gus,IO_MB);
		WriteHandler[0].Install(0x302 + GUS_BASE,write_gus,IO_MB);
	
		WriteHandler[1].Install(0x303 + GUS_BASE,write_gus,IO_MB);
		ReadHandler[1].Install(0x303 + GUS_BASE,read_gus,IO_MB);
	
		WriteHandler[2].Install(0x304 + GUS_BASE,write_gus,IO_MB|IO_MW);
		ReadHandler[2].Install(0x304 + GUS_BASE,read_gus,IO_MB|IO_MW);
	
		WriteHandler[3].Install(0x305 + GUS_BASE,write_gus,IO_MB);
		ReadHandler[3].Install(0x305 + GUS_BASE,read_gus,IO_MB);
	
		ReadHandler[4].Install(0x206 + GUS_BASE,read_gus,IO_MB);
	
		WriteHandler[4].Install(0x208 + GUS_BASE,write_gus,IO_MB);
		ReadHandler[5].Install(0x208 + GUS_BASE,read_gus,IO_MB);
	
		WriteHandler[5].Install(0x209 + GUS_BASE,write_gus,IO_MB);
	
		WriteHandler[6].Install(0x307 + GUS_BASE,write_gus,IO_MB);
		ReadHandler[6].Install(0x307 + GUS_BASE,read_gus,IO_MB);
	
		// Board Only 
	
		WriteHandler[7].Install(0x200 + GUS_BASE,write_gus,IO_MB);
		ReadHandler[7].Install(0x20A + GUS_BASE,read_gus,IO_MB);
		WriteHandler[8].Install(0x20B + GUS_BASE,write_gus,IO_MB);

		if (gus_type >= GUS_MAX || gus_ics_mixer/*classic with 3.7 mixer*/) {
			/* "This register is only valid for UltraSound boards that are at or above revision 3.4. It is not valid for boards which
			 * have a prior revision number.
			 * On 3.4 and above boards, there is a bank of 6 registers that exist at location 2XB. Register 2XF is used to select
			 * which one is being talked to." */
			ReadHandler[9].Install(0x20F + GUS_BASE,read_gus,IO_MB);
			WriteHandler[9].Install(0x20F + GUS_BASE,write_gus,IO_MB);

			/* FIXME: I'm not so sure Interwave PnP cards have this */
			ReadHandler[10].Install(0x306 + GUS_BASE,read_gus,IO_MB); // Revision level
			ReadHandler[11].Install(0x706 + GUS_BASE,read_gus,IO_MB); // Revision level
			WriteHandler[10].Install(0x306 + GUS_BASE,write_gus,IO_MB); // Mixer control
			WriteHandler[11].Install(0x706 + GUS_BASE,write_gus,IO_MB); // Mixer data / GUS UltraMAX Control register
		}
		if (gus_type >= GUS_MAX) {
			LOG(LOG_MISC,LOG_WARN)("GUS caution: CS4231 UltraMax emulation is new and experimental at this time and it is not guaranteed to work.");
			LOG(LOG_MISC,LOG_WARN)("GUS caution: CS4231 UltraMax emulation as it exists now may cause applications to hang or malfunction attempting to play through it.");

			/* UltraMax has a CS4231 codec at 3XC-3XF */
			/* FIXME: Does the Interwave have a CS4231? */
			for (unsigned int i=0;i < 4;i++) {
				ReadCS4231Handler[i].Install(0x30C + i + GUS_BASE,read_gus_cs4231,IO_MB);
				WriteCS4231Handler[i].Install(0x30C + i + GUS_BASE,write_gus_cs4231,IO_MB);
			}
		}
	
	//	DmaChannels[myGUS.dma1]->Register_TC_Callback(GUS_DMA_TC_Callback);
	
		MakeTables();
	
		for (Bit8u chan_ct=0; chan_ct<32; chan_ct++) {
			guschan[chan_ct] = new GUSChannels(chan_ct);
		}
		// Register the Mixer CallBack 
		gus_chan=MixerChan.Install(GUS_CallBack,GUS_RATE,"GUS");

		// FIXME: Could we leave the card in reset state until a fake ULTRINIT runs?
		myGUS.gRegData=0x000/*reset*/;
		GUSReset();

		if (myGUS.initUnmaskDMA)
			GetDMAChannel(myGUS.dma1)->SetMask(false);

		gus_chan->Enable(true);

		GetDMAChannel(myGUS.dma1)->Register_Callback(GUS_DMA_Callback);

		int portat = 0x200+GUS_BASE;

		// ULTRASND=Port,DMA1,DMA2,IRQ1,IRQ2
		// [GUS port], [GUS DMA (recording)], [GUS DMA (playback)], [GUS IRQ (playback)], [GUS IRQ (MIDI)]
		ostringstream temp;
		temp << "SET ULTRASND=" << hex << setw(3) << portat << ","
		     << dec << (Bitu)myGUS.dma1 << "," << (Bitu)myGUS.dma2 << ","
		     << (Bitu)myGUS.irq1 << "," << (Bitu)myGUS.irq2 << ends;
		// Create autoexec.bat lines
		autoexecline[0].Install(temp.str());
		autoexecline[1].Install(std::string("SET ULTRADIR=") + section->Get_string("ultradir"));

		if (gus_type >= GUS_MAX) {
			/* FIXME: Does the Interwave have a CS4231? */
			ostringstream temp2;
			temp2 << "SET ULTRA16=" << hex << setw(3) << (0x30C+GUS_BASE) << ","
				<< "0,0,1,0" << ends; // FIXME What do these numbers mean?
			autoexecline[2].Install(temp2.str());
		}
	}

	void DOS_Shutdown() { /* very likely, we're booting into a guest OS where our environment variable has no meaning anymore */
		autoexecline[0].Uninstall();
		autoexecline[1].Uninstall();
		autoexecline[2].Uninstall();
	}

	~GUS() {
#if 0 // FIXME
		if(!IS_EGAVGA_ARCH) return;
	
		myGUS.gRegData=0x1;
		GUSReset();
		myGUS.gRegData=0x0;
	
		for(Bitu i=0;i<32;i++) {
			delete guschan[i];
		}

		memset(&myGUS,0,sizeof(myGUS));
		memset(GUSRam,0,1024*1024);
#endif
	}
};

static GUS* test = NULL;

void GUS_DOS_Shutdown() {
	if (test != NULL) test->DOS_Shutdown();
}

void GUS_ShutDown(Section* /*sec*/) {
	if (test != NULL) {
		delete test;	
		test = NULL;
	}
}

void GUS_OnReset(Section *sec) {
	if (test == NULL) {
		LOG(LOG_MISC,LOG_DEBUG)("Allocating GUS emulation");
		test = new GUS(control->GetSection("gus"));
	}
}

void GUS_Init() {
	LOG(LOG_MISC,LOG_DEBUG)("Initializing Gravis Ultrasound emulation");

	AddExitFunction(AddExitFunctionFuncPair(GUS_ShutDown),true);
	AddVMEventFunction(VM_EVENT_RESET,AddVMEventFunctionFuncPair(GUS_OnReset));
}

