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

#include <iomanip>
#include <sstream>
#include <string.h>
#include <math.h> 
#include "dosbox.h"
#include "inout.h"
#include "mixer.h"
#include "dma.h"
#include "pic.h"
#include "bios.h"
#include "hardware.h"
#include "setup.h"
#include "support.h"
#include "shell.h"
using namespace std;

void MIDI_RawOutByte(Bit8u data);
bool MIDI_Available(void);

#define SB_PIC_EVENTS 0

#define DSP_MAJOR 3
#define DSP_MINOR 1

#define MIXER_INDEX 0x04
#define MIXER_DATA 0x05

#define DSP_RESET 0x06
#define DSP_READ_DATA 0x0A
#define DSP_WRITE_DATA 0x0C
#define DSP_WRITE_STATUS 0x0C
#define DSP_READ_STATUS 0x0E
#define DSP_ACK_16BIT 0x0f

#define DSP_NO_COMMAND 0

#define DMA_BUFSIZE 1024
#define DSP_BUFSIZE 64
#define DSP_DACSIZE 512

//Should be enough for sound generated in millisecond blocks
#define SB_SH	14
#define SB_SH_MASK	((1 << SB_SH)-1)

enum {DSP_S_RESET,DSP_S_RESET_WAIT,DSP_S_NORMAL,DSP_S_HIGHSPEED};
enum SB_TYPES {SBT_NONE=0,SBT_1=1,SBT_PRO1=2,SBT_2=3,SBT_PRO2=4,SBT_16=6,SBT_GB=7}; /* TODO: Need SB 2.0 vs SB 2.01 */
enum SB_IRQS {SB_IRQ_8,SB_IRQ_16,SB_IRQ_MPU};

enum DSP_MODES {
	MODE_NONE,
	MODE_DAC,
	MODE_DMA,
	MODE_DMA_PAUSE,
	MODE_DMA_MASKED,
	MODE_DMA_REQUIRE_IRQ_ACK		/* Sound Blaster 16 style require IRQ ack to resume DMA */
};

enum DMA_MODES {
	DSP_DMA_NONE,
	DSP_DMA_2,DSP_DMA_3,DSP_DMA_4,DSP_DMA_8,
	DSP_DMA_16,DSP_DMA_16_ALIASED
};

enum {
	PLAY_MONO,PLAY_STEREO
};

struct SB_INFO {
	Bitu freq;
	Bitu dma_dac_srcrate;
	struct {
		bool stereo,sign,autoinit;
		DMA_MODES mode;
		Bitu rate,mul;
		Bitu total,left,min;
		Bit64u start;
		union {
			Bit8u  b8[DMA_BUFSIZE];
			Bit16s b16[DMA_BUFSIZE];
		} buf;
		Bitu bits;
		DmaChannel * chan;
		Bitu remain_size;
	} dma;
	bool speaker;
	bool midi;
	bool vibra;
	bool emit_blaster_var;
	bool sbpro_stereo_bit_strict_mode; /* if set, stereo bit in mixer can only be set if emulating a Pro. if clear, SB16 can too */
	bool sample_rate_limits; /* real SB hardware has limits on the sample rate */
	bool dma_dac_mode; /* some very old DOS demos "play" sound by setting the DMA terminal count to 0.
			      normally that would mean the DMA controller transmitting the same byte at the sample rate,
			      except that the program creates sound by overwriting that byte periodically.
			      on actual hardware this happens to work (though with kind of a gritty sound to it),
			      The DMA emulation here does not handle that well. */
	bool goldplay;
	bool goldplay_stereo;
	Bit8u time_constant;
	DSP_MODES mode;
	SB_TYPES type;
	struct {
		bool pending_8bit;
		bool pending_16bit;
	} irq;
	struct {
		Bit8u state;
		Bit8u cmd;
		Bit8u cmd_len;
		Bit8u cmd_in_pos;
		Bit8u cmd_in[DSP_BUFSIZE];
		struct {
			Bit8u lastval;
			Bit8u data[DSP_BUFSIZE];
			Bitu pos,used;
		} in,out;
		Bit8u test_register;
		Bitu write_busy;
		bool highspeed;
		bool require_irq_ack;
		unsigned int dsp_write_busy_time; /* when you write to the DSP, how long it signals "busy" */
	} dsp;
	struct {
		Bit16s data[DSP_DACSIZE+1];
		Bitu used;
		Bit16s last;
	} dac;
	struct {
		Bit8u index;
		Bit8u dac[2],fm[2],cda[2],master[2],lin[2];
		Bit8u mic;
		bool stereo;
		bool enabled;
		bool filtered;
		Bit8u unhandled[0x48];
	} mixer;
	struct {
		Bit8u reference;
		Bits stepsize;
		bool haveref;
	} adpcm;
	struct {
		Bitu base;
		Bitu irq;
		Bit8u dma8,dma16;
	} hw;
	struct {
		Bits value;
		Bitu count;
	} e2;
	MixerChannel * chan;
};



static SB_INFO sb;

static char const * const copyright_string="COPYRIGHT (C) CREATIVE TECHNOLOGY LTD, 1992.";

// number of bytes in input for commands (sb/sbpro)
static Bit8u const DSP_cmd_len_sb[256] = {
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x00
//  1,0,0,0, 2,0,2,2, 0,0,0,0, 0,0,0,0,  // 0x10
  1,0,0,0, 2,2,2,2, 0,0,0,0, 0,0,0,0,  // 0x10 Wari hack
  0,0,0,0, 2,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x20
  0,0,0,0, 0,0,0,0, 1,0,0,0, 0,0,0,0,  // 0x30

  1,2,2,0, 0,0,0,0, 2,0,0,0, 0,0,0,0,  // 0x40
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x50
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x60
  0,0,0,0, 2,2,2,2, 0,0,0,0, 0,0,0,0,  // 0x70

  2,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x80
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x90
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0xa0
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0xb0

  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0xc0
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0xd0
  1,0,1,0, 1,0,0,0, 0,0,0,0, 0,0,0,0,  // 0xe0
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0   // 0xf0
};

// number of bytes in input for commands (sb16)
static Bit8u const DSP_cmd_len_sb16[256] = {
  0,0,0,0, 1,2,0,0, 1,0,0,0, 0,0,2,1,  // 0x00
//  1,0,0,0, 2,0,2,2, 0,0,0,0, 0,0,0,0,  // 0x10
  1,0,0,0, 2,2,2,2, 0,0,0,0, 0,0,0,0,  // 0x10 Wari hack
  0,0,0,0, 2,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x20
  0,0,0,0, 0,0,0,0, 1,0,0,0, 0,0,0,0,  // 0x30

  1,2,2,0, 0,0,0,0, 2,0,0,0, 0,0,0,0,  // 0x40
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x50
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x60
  0,0,0,0, 2,2,2,2, 0,0,0,0, 0,0,0,0,  // 0x70

  2,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x80
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x90
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0xa0
  3,3,3,3, 3,3,3,3, 3,3,3,3, 3,3,3,3,  // 0xb0

  3,3,3,3, 3,3,3,3, 3,3,3,3, 3,3,3,3,  // 0xc0
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0xd0
  1,0,1,0, 1,0,0,0, 0,0,0,0, 0,0,0,0,  // 0xe0
  0,0,0,0, 0,0,0,0, 0,1,0,0, 0,0,0,0   // 0xf0
};

static Bit8u ASP_regs[256];
static bool ASP_init_in_progress = false;

static int const E2_incr_table[4][9] = {
  {  0x01, -0x02, -0x04,  0x08, -0x10,  0x20,  0x40, -0x80, -106 },
  { -0x01,  0x02, -0x04,  0x08,  0x10, -0x20,  0x40, -0x80,  165 },
  { -0x01,  0x02,  0x04, -0x08,  0x10, -0x20, -0x40,  0x80, -151 },
  {  0x01, -0x02,  0x04, -0x08, -0x10,  0x20, -0x40,  0x80,   90 }
};

#ifndef max
#define max(a,b) ((a)>(b)?(a):(b))
#endif
#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif

static void DSP_ChangeMode(DSP_MODES mode);
static void CheckDMAEnd();
static void DMA_DAC_Event(Bitu);
static void END_DMA_Event(Bitu);
static void DMA_Silent_Event(Bitu val);
static void GenerateDMASound(Bitu size);

static void DSP_SetSpeaker(bool how) {
	if (sb.speaker==how) return;
	sb.speaker=how;
	if (sb.type==SBT_16) return;
	sb.chan->Enable(how);
	if (sb.speaker) {
		PIC_RemoveEvents(DMA_Silent_Event);
		CheckDMAEnd();
	} else {
		
	}
}

/* NTS: Using some old Creative Sound Blaster 16 ViBRA PnP cards as reference,
 *      the card will send IRQ 9 if the IRQ is configured to either "2" or "9".
 *      Whichever value is written will be read back. The reason for this has
 *      to do with the pin on the ISA bus connector that used to signal IRQ 2
 *      on PC/XT hardware, but was wired to fire IRQ 9 instead on PC/AT hardware
 *      because of the IRQ 8-15 -> IRQ 2 cascade on AT hardware.
 *
 *      There's not much to change here, because PIC_ActivateIRQ was modified
 *      to remap IRQ 2 -> IRQ 9 for us *if* emulating AT hardware.
 *
 *      --Jonathan C. */
static INLINE void SB_RaiseIRQ(SB_IRQS type) {
	LOG(LOG_SB,LOG_NORMAL)("Raising IRQ");

	switch (type) {
	case SB_IRQ_8:
		if (sb.irq.pending_8bit) {
//			LOG_MSG("SB: 8bit irq pending");
			return;
		}
		sb.irq.pending_8bit=true;
		PIC_ActivateIRQ(sb.hw.irq);
		break;
	case SB_IRQ_16:
		if (sb.irq.pending_16bit) {
//			LOG_MSG("SB: 16bit irq pending");
			return;
		}
		sb.irq.pending_16bit=true;
		PIC_ActivateIRQ(sb.hw.irq);
		break;
	default:
		break;
	}
}

static INLINE void DSP_FlushData(void) {
	sb.dsp.out.used=0;
	sb.dsp.out.pos=0;
}

static void DSP_DMA_CallBack(DmaChannel * chan, DMAEvent event) {
	if (event==DMA_REACHED_TC) return;
	else if (event==DMA_MASKED) {
		if (sb.mode==MODE_DMA) {
			if (!sb.dma_dac_mode) GenerateDMASound(sb.dma.min);
			sb.mode=MODE_DMA_MASKED;
//			DSP_ChangeMode(MODE_DMA_MASKED);
			LOG(LOG_SB,LOG_NORMAL)("DMA masked,stopping output, left %d",chan->currcnt);
		}
	} else if (event==DMA_UNMASKED) {
		if (sb.mode==MODE_DMA_MASKED && sb.dma.mode!=DSP_DMA_NONE) {
			DSP_ChangeMode(MODE_DMA);
//			sb.mode=MODE_DMA;
			if (!sb.dma_dac_mode) CheckDMAEnd();
			LOG(LOG_SB,LOG_NORMAL)("DMA unmasked,starting output, auto %d block %d",chan->autoinit,chan->basecnt);
		}
	}
}

#define MIN_ADAPTIVE_STEP_SIZE 0
#define MAX_ADAPTIVE_STEP_SIZE 32767
#define DC_OFFSET_FADE 254

static INLINE Bit8u decode_ADPCM_4_sample(Bit8u sample,Bit8u & reference,Bits& scale) {
	static const Bit8s scaleMap[64] = {
		0,  1,  2,  3,  4,  5,  6,  7,  0,  -1,  -2,  -3,  -4,  -5,  -6,  -7,
		1,  3,  5,  7,  9, 11, 13, 15, -1,  -3,  -5,  -7,  -9, -11, -13, -15,
		2,  6, 10, 14, 18, 22, 26, 30, -2,  -6, -10, -14, -18, -22, -26, -30,
		4, 12, 20, 28, 36, 44, 52, 60, -4, -12, -20, -28, -36, -44, -52, -60
	};
	static const Bit8u adjustMap[64] = {
		  0, 0, 0, 0, 0, 16, 16, 16,
		  0, 0, 0, 0, 0, 16, 16, 16,
		240, 0, 0, 0, 0, 16, 16, 16,
		240, 0, 0, 0, 0, 16, 16, 16,
		240, 0, 0, 0, 0, 16, 16, 16,
		240, 0, 0, 0, 0, 16, 16, 16,
		240, 0, 0, 0, 0,  0,  0,  0,
		240, 0, 0, 0, 0,  0,  0,  0
	};

	Bits samp = sample + scale;

	if ((samp < 0) || (samp > 63)) { 
		LOG(LOG_SB,LOG_ERROR)("Bad ADPCM-4 sample");
		if(samp < 0 ) samp =  0;
		if(samp > 63) samp = 63;
	}

	Bits ref = reference + scaleMap[samp];
	if (ref > 0xff) reference = 0xff;
	else if (ref < 0x00) reference = 0x00;
	else reference = (Bit8u)(ref&0xff);
	scale = (scale + adjustMap[samp]) & 0xff;
	
	return reference;
}

static INLINE Bit8u decode_ADPCM_2_sample(Bit8u sample,Bit8u & reference,Bits& scale) {
	static const Bit8s scaleMap[24] = {
		0,  1,  0,  -1,  1,  3,  -1,  -3,
		2,  6, -2,  -6,  4, 12,  -4, -12,
		8, 24, -8, -24, 16, 48, -16, -48
	};
	static const Bit8u adjustMap[24] = {
		  0, 4,   0, 4,
		252, 4, 252, 4, 252, 4, 252, 4,
		252, 4, 252, 4, 252, 4, 252, 4,
		252, 0, 252, 0
	};

	Bits samp = sample + scale;
	if ((samp < 0) || (samp > 23)) {
		LOG(LOG_SB,LOG_ERROR)("Bad ADPCM-2 sample");
		if(samp < 0 ) samp =  0;
		if(samp > 23) samp = 23;
	}

	Bits ref = reference + scaleMap[samp];
	if (ref > 0xff) reference = 0xff;
	else if (ref < 0x00) reference = 0x00;
	else reference = (Bit8u)(ref&0xff);
	scale = (scale + adjustMap[samp]) & 0xff;

	return reference;
}

INLINE Bit8u decode_ADPCM_3_sample(Bit8u sample,Bit8u & reference,Bits& scale) {
	static const Bit8s scaleMap[40] = { 
		0,  1,  2,  3,  0,  -1,  -2,  -3,
		1,  3,  5,  7, -1,  -3,  -5,  -7,
		2,  6, 10, 14, -2,  -6, -10, -14,
		4, 12, 20, 28, -4, -12, -20, -28,
		5, 15, 25, 35, -5, -15, -25, -35
	};
	static const Bit8u adjustMap[40] = {
		  0, 0, 0, 8,   0, 0, 0, 8,
		248, 0, 0, 8, 248, 0, 0, 8,
		248, 0, 0, 8, 248, 0, 0, 8,
		248, 0, 0, 8, 248, 0, 0, 8,
		248, 0, 0, 0, 248, 0, 0, 0
	};

	Bits samp = sample + scale;
	if ((samp < 0) || (samp > 39)) {
		LOG(LOG_SB,LOG_ERROR)("Bad ADPCM-3 sample");
		if(samp < 0 ) samp =  0;
		if(samp > 39) samp = 39;
	}

	Bits ref = reference + scaleMap[samp];
	if (ref > 0xff) reference = 0xff;
	else if (ref < 0x00) reference = 0x00;
	else reference = (Bit8u)(ref&0xff);
	scale = (scale + adjustMap[samp]) & 0xff;

	return reference;
}

void SB_OnEndOfDMA(void) {
	bool was_irq=false;

	PIC_RemoveEvents(END_DMA_Event);
	if (sb.dma.mode >= DSP_DMA_16) {
		was_irq = sb.irq.pending_16bit;
		SB_RaiseIRQ(SB_IRQ_16);
	}
	else {
		was_irq = sb.irq.pending_8bit;
		SB_RaiseIRQ(SB_IRQ_8);
	}

	if (!sb.dma.autoinit) {
		sb.dsp.highspeed = false;
		LOG(LOG_SB,LOG_NORMAL)("Single cycle transfer ended");
		sb.mode=MODE_NONE;
		sb.dma.mode=DSP_DMA_NONE;
	} else {
		sb.dma.left=sb.dma.total;
		if (!sb.dma.left) {
			LOG(LOG_SB,LOG_NORMAL)("Auto-init transfer with 0 size");
			sb.dsp.highspeed = false;
			sb.mode=MODE_NONE;
		}
		else if (sb.dsp.require_irq_ack && was_irq) { /* Sound Blaster 16-style require IRQ ack before proceeding even with auto-init */
			/* FIXME: ^ Is this accurate? Because this makes sense to me.
			 *          Or, does the SB16 have a timeout from the IRQ instead? */
			LOG(LOG_SB,LOG_WARN)("DMA ended when previous IRQ had not yet been acked");
			sb.mode=MODE_DMA_REQUIRE_IRQ_ACK;
		}
	}
}

static void GenerateDMASound(Bitu size) {
	Bitu read=0;Bitu done=0;Bitu i=0;

	if(sb.dma.autoinit) {
		if (sb.dma.left <= size) size = sb.dma.left;
	} else if (sb.dma.left <= sb.dma.min) size = sb.dma.left;

	switch (sb.dma.mode) {
	case DSP_DMA_2:
		read=sb.dma.chan->Read(size,sb.dma.buf.b8);
		if (read && sb.adpcm.haveref) {
			sb.adpcm.haveref=false;
			sb.adpcm.reference=sb.dma.buf.b8[0];
			sb.adpcm.stepsize=MIN_ADAPTIVE_STEP_SIZE;
			i++;
		}
		for (;i<read;i++) {
			MixTemp[done++]=decode_ADPCM_2_sample((sb.dma.buf.b8[i] >> 6) & 0x3,sb.adpcm.reference,sb.adpcm.stepsize);
			MixTemp[done++]=decode_ADPCM_2_sample((sb.dma.buf.b8[i] >> 4) & 0x3,sb.adpcm.reference,sb.adpcm.stepsize);
			MixTemp[done++]=decode_ADPCM_2_sample((sb.dma.buf.b8[i] >> 2) & 0x3,sb.adpcm.reference,sb.adpcm.stepsize);
			MixTemp[done++]=decode_ADPCM_2_sample((sb.dma.buf.b8[i] >> 0) & 0x3,sb.adpcm.reference,sb.adpcm.stepsize);
		}
		sb.chan->AddSamples_m8(done,MixTemp);
		break;
	case DSP_DMA_3:
		read=sb.dma.chan->Read(size,sb.dma.buf.b8);
		if (read && sb.adpcm.haveref) {
			sb.adpcm.haveref=false;
			sb.adpcm.reference=sb.dma.buf.b8[0];
			sb.adpcm.stepsize=MIN_ADAPTIVE_STEP_SIZE;
			i++;
		}
		for (;i<read;i++) {
			MixTemp[done++]=decode_ADPCM_3_sample((sb.dma.buf.b8[i] >> 5) & 0x7,sb.adpcm.reference,sb.adpcm.stepsize);
			MixTemp[done++]=decode_ADPCM_3_sample((sb.dma.buf.b8[i] >> 2) & 0x7,sb.adpcm.reference,sb.adpcm.stepsize);
			MixTemp[done++]=decode_ADPCM_3_sample((sb.dma.buf.b8[i] & 0x3) << 1,sb.adpcm.reference,sb.adpcm.stepsize);
		}
		sb.chan->AddSamples_m8(done,MixTemp);
		break;
	case DSP_DMA_4:
		read=sb.dma.chan->Read(size,sb.dma.buf.b8);
		if (read && sb.adpcm.haveref) {
			sb.adpcm.haveref=false;
			sb.adpcm.reference=sb.dma.buf.b8[0];
			sb.adpcm.stepsize=MIN_ADAPTIVE_STEP_SIZE;
			i++;
		}
		for (;i<read;i++) {
			MixTemp[done++]=decode_ADPCM_4_sample(sb.dma.buf.b8[i] >> 4,sb.adpcm.reference,sb.adpcm.stepsize);
			MixTemp[done++]=decode_ADPCM_4_sample(sb.dma.buf.b8[i]& 0xf,sb.adpcm.reference,sb.adpcm.stepsize);
		}
		sb.chan->AddSamples_m8(done,MixTemp);
		break;
	case DSP_DMA_8:
		if (sb.dma.stereo) {
			read=sb.dma.chan->Read(size,&sb.dma.buf.b8[sb.dma.remain_size]);
			Bitu total=read+sb.dma.remain_size;
            if (!sb.dma.sign)  sb.chan->AddSamples_s8(total>>1,sb.dma.buf.b8);
            else sb.chan->AddSamples_s8s(total>>1,(Bit8s*)sb.dma.buf.b8); 
			if (total&1) {
				sb.dma.remain_size=1;
				sb.dma.buf.b8[0]=sb.dma.buf.b8[total-1];
			} else sb.dma.remain_size=0;
		} else {
			read=sb.dma.chan->Read(size,sb.dma.buf.b8);
			if (!sb.dma.sign) sb.chan->AddSamples_m8(read,sb.dma.buf.b8);
			else sb.chan->AddSamples_m8s(read,(Bit8s *)sb.dma.buf.b8);
		}
		break;
	case DSP_DMA_16:
	case DSP_DMA_16_ALIASED:
		if (sb.dma.stereo) {
			/* In DSP_DMA_16_ALIASED mode temporarily divide by 2 to get number of 16-bit
			   samples, because 8-bit DMA Read returns byte size, while in DSP_DMA_16 mode
			   16-bit DMA Read returns word size */
			read=sb.dma.chan->Read(size,(Bit8u *)&sb.dma.buf.b16[sb.dma.remain_size]) 
				>> (sb.dma.mode==DSP_DMA_16_ALIASED ? 1:0);
			Bitu total=read+sb.dma.remain_size;
#if defined(WORDS_BIGENDIAN)
			if (sb.dma.sign) sb.chan->AddSamples_s16_nonnative(total>>1,sb.dma.buf.b16);
			else sb.chan->AddSamples_s16u_nonnative(total>>1,(Bit16u *)sb.dma.buf.b16);
#else
			if (sb.dma.sign) sb.chan->AddSamples_s16(total>>1,sb.dma.buf.b16);
			else sb.chan->AddSamples_s16u(total>>1,(Bit16u *)sb.dma.buf.b16);
#endif
			if (total&1) {
				sb.dma.remain_size=1;
				sb.dma.buf.b16[0]=sb.dma.buf.b16[total-1];
			} else sb.dma.remain_size=0;
		} else {
			read=sb.dma.chan->Read(size,(Bit8u *)sb.dma.buf.b16) 
				>> (sb.dma.mode==DSP_DMA_16_ALIASED ? 1:0);
#if defined(WORDS_BIGENDIAN)
			if (sb.dma.sign) sb.chan->AddSamples_m16_nonnative(read,sb.dma.buf.b16);
			else sb.chan->AddSamples_m16u_nonnative(read,(Bit16u *)sb.dma.buf.b16);
#else
			if (sb.dma.sign) sb.chan->AddSamples_m16(read,sb.dma.buf.b16);
			else sb.chan->AddSamples_m16u(read,(Bit16u *)sb.dma.buf.b16);
#endif
		}
		//restore buffer length value to byte size in aliased mode
		if (sb.dma.mode==DSP_DMA_16_ALIASED) read=read<<1;
		break;
	default:
		LOG_MSG("Unhandled dma mode %d",sb.dma.mode);
		sb.mode=MODE_NONE;
		return;
	}
	sb.dma.left-=read;
	if (!sb.dma.left) SB_OnEndOfDMA();
}

/* old version...
static void GenerateDACSound(Bitu len) {
	if (!sb.dac.used) {
		sb.mode=MODE_NONE;
		return;
	}
	Bitu dac_add=(sb.dac.used<<16)/len;
	Bitu dac_pos=0;
	Bit16s * out=(Bit16s *)MixTemp;
	for (Bitu i=len;i;i--) {
		*out++=sb.dac.data[0+(dac_pos>>16)];
		dac_pos+=dac_add;
	}
	sb.dac.used=0;
	sb.chan->AddSamples_m16(len,(Bit16s *)MixTemp);
}
*/

static void DMA_Silent_Event(Bitu val) {
	if (sb.dma.left<val) val=sb.dma.left;
	Bitu read=sb.dma.chan->Read(val,sb.dma.buf.b8);
	sb.dma.left-=read;
	if (!sb.dma.left) {
		if (sb.dma.mode >= DSP_DMA_16) SB_RaiseIRQ(SB_IRQ_16);
		else SB_RaiseIRQ(SB_IRQ_8);
		if (sb.dma.autoinit) sb.dma.left=sb.dma.total;
		else {
			sb.mode=MODE_NONE;
			sb.dma.mode=DSP_DMA_NONE;
		}
	}
	if (sb.dma.left) {
		Bitu bigger=(sb.dma.left > sb.dma.min) ? sb.dma.min : sb.dma.left;
		float delay=(bigger*1000.0f)/sb.dma.rate;
		PIC_AddEvent(DMA_Silent_Event,delay,bigger);
	}

}

#include <assert.h>

static void DMA_DAC_Event(Bitu val) {
	unsigned char tmp[4];
	Bitu read,expct;
	signed int L,R;

	if (sb.dma.chan->masked) {
		PIC_AddEvent(DMA_DAC_Event,1000.0 / sb.dma_dac_srcrate);
		return;
	}

	/* NTS: chan->Read() deals with DMA unit transfers.
	 *      for 8-bit DMA, read/expct is in bytes, for 16-bit DMA, read/expct is in 16-bit words */
	expct = (sb.dma.stereo ? 2 : 1);// * ((sb.dma.mode == DSP_DMA_16 || sb.dma.mode == DSP_DMA_16_ALIASED) ? 2 : 1);
	read = sb.dma.chan->Read(expct,tmp);
	//if (read != expct)
	//	LOG_MSG("DMA read was not sample aligned. Sound may swap channels or become static. On real hardware the same may happen unless audio is prepared specifically.\n");

	if (sb.dma.mode == DSP_DMA_16 || sb.dma.mode == DSP_DMA_16_ALIASED) {
		L = (int16_t)host_readw(&tmp[0]);
		if (!sb.dma.sign) L ^= 0x8000;
		if (sb.dma.stereo) {
			R = (int16_t)host_readw(&tmp[2]);
			if (!sb.dma.sign) R ^= 0x8000;
		}
		else {
			R = L;
		}
	}
	else {
		L = tmp[0];
		if (!sb.dma.sign) L ^= 0x80;
		L = (int16_t)(L << 8);
		if (sb.dma.stereo) {
			R = tmp[1];
			if (!sb.dma.sign) R ^= 0x80;
			R = (int16_t)(R << 8);
		}
		else {
			R = L;
		}
	}

	if (sb.dma.stereo) {
		if ((sb.dac.used+2) <= DSP_DACSIZE) {
			sb.dac.data[sb.dac.used++]=L;
			sb.dac.data[sb.dac.used++]=R;
		}
	}
	else {
		if (sb.dac.used < DSP_DACSIZE)
			sb.dac.data[sb.dac.used++]=L;
	}

	sb.dma.left -= read;
	if (!sb.dma.left) {
		SB_OnEndOfDMA();
		if (sb.dma_dac_mode) PIC_AddEvent(DMA_DAC_Event,1000.0 / sb.dma_dac_srcrate);
	}
	else {
		PIC_AddEvent(DMA_DAC_Event,1000.0 / sb.dma_dac_srcrate);
	}
}

static void END_DMA_Event(Bitu val) {
	GenerateDMASound(val);
}

static void CheckDMAEnd(void) {
	if (!sb.dma.left) return;
	if (!sb.speaker && sb.type!=SBT_16) {
		Bitu bigger=(sb.dma.left > sb.dma.min) ? sb.dma.min : sb.dma.left;
		float delay=(bigger*1000.0f)/sb.dma.rate;
		PIC_AddEvent(DMA_Silent_Event,delay,bigger);
		LOG(LOG_SB,LOG_NORMAL)("Silent DMA Transfer scheduling IRQ in %.3f milliseconds",delay);
	} else if (sb.dma.left<sb.dma.min) {
		float delay=(sb.dma.left*1000.0f)/sb.dma.rate;
		LOG(LOG_SB,LOG_NORMAL)("Short transfer scheduling IRQ in %.3f milliseconds",delay);	
		PIC_AddEvent(END_DMA_Event,delay,sb.dma.left);
	}
}

static void DSP_ChangeMode(DSP_MODES mode) {
	if (sb.mode==mode) return;
	else sb.chan->FillUp();
	sb.mode=mode;
}

static void DSP_RaiseIRQEvent(Bitu /*val*/) {
	SB_RaiseIRQ(SB_IRQ_8);
}

static void DSP_DoDMATransfer(DMA_MODES mode,Bitu freq,bool stereo) {
	char const * type;

	sb.mode=MODE_DMA_MASKED;

	/* Explanation: A handful of ancient DOS demos (in the 1990-1992 timeframe) were written to output
	 *              sound using the timer interrupt (IRQ 0) at a fixed rate to a device, usually the
	 *              PC speaker or LPT1 DAC. When SoundBlaster came around, the programmers decided
	 *              apparently to treat the Sound Blaster in the same way, so many of these early
	 *              demos (especially those using GoldPlay) used either Direct DAC output or a hacked
	 *              form of DMA single-cycle 8-bit output.
	 *
	 *              The way the hacked DMA mode works, is that the Sound Blaster is told the transfer
	 *              length is 65536 or some other large value. Then, the DMA controller is programmed
	 *              to point at a specific byte (or two bytes for stereo) and the counter value for
	 *              that DMA channel is set to 0 (or 1 for stereo). This means that as the Sound Blaster 
	 *              fetches bytes to play, the DMA controller ends up sending the same byte value
	 *              over and over again. However, the demo has the timer running at the desired sample
	 *              rate (IRQ 0) and the interrupt routine is modifying the byte to reflect the latest
	 *              sample output. In this way, the demo renders audio whenever it feels like it and
	 *              the Sound Blaster gets audio at the rate it works best with.
	 *
	 *              It's worth noting the programmers may have done this because DMA playback is the
	 *              only way to get SB Pro stereo output working.
	 *
	 *              The problem here in DOSBox is that the DMA block-transfer code here is not precise
	 *              enough to handle that properly. When you run such a program in DOSBox 0.74 and
	 *              earlier, you get a low-frequency digital "rumble" that kinda-sorta sounds like
	 *              what the demo is playing (the same byte value repeated over and over again, 
	 *              remember?). The only way to properly render such output, is to read the memory
	 *              value at the sample rate and buffer it for output.
	 *
	 * This fixes Sound Blaster output in:
	 *    Twilight Zone - Buttman (1992) [SB and SB Pro modes]
	 *    Triton - Crystal Dream (1992) [SB and SB Pro modes]
	 *    The Jungly (1992) [SB and SB Pro modes]
	 */
	if (sb.goldplay && sb.freq > 0 && sb.dma.chan->basecnt < 4)
		sb.dma_dac_mode=1;
	else
		sb.dma_dac_mode=0;

	sb.chan->FillUp();
	sb.dma_dac_srcrate=sb.freq;
	sb.dma.left=sb.dma.total;
	sb.dma.mode=mode;
	sb.dma.stereo=stereo;
	sb.irq.pending_8bit=false;
	sb.irq.pending_16bit=false;
	switch (mode) {
	case DSP_DMA_2:
		type="2-bits ADPCM";
		sb.dma.mul=(1 << SB_SH)/4;
		break;
	case DSP_DMA_3:
		type="3-bits ADPCM";
		sb.dma.mul=(1 << SB_SH)/3;
		break;
	case DSP_DMA_4:
		type="4-bits ADPCM";
		sb.dma.mul=(1 << SB_SH)/2;
		break;
	case DSP_DMA_8:
		type="8-bits PCM";
		sb.dma.mul=(1 << SB_SH);
		break;
	case DSP_DMA_16_ALIASED:
		type="16-bits(aliased) PCM";
		sb.dma.mul=(1 << SB_SH)*2;
		break;
	case DSP_DMA_16:
		type="16-bits PCM";
		sb.dma.mul=(1 << SB_SH);
		break;
	default:
		LOG(LOG_SB,LOG_ERROR)("DSP:Illegal transfer mode %d",mode);
		return;
	}
	if (sb.dma.stereo) sb.dma.mul*=2;
	sb.dma.rate=(sb.dma_dac_srcrate*sb.dma.mul) >> SB_SH;
	sb.dma.min=(sb.dma.rate*3)/1000;
	if (sb.dma_dac_mode && sb.goldplay_stereo)
		sb.chan->SetFreq(sb.dma_dac_srcrate);
	else
		sb.chan->SetFreq(freq);
	sb.dma.mode=mode;
	PIC_RemoveEvents(DMA_DAC_Event);
	PIC_RemoveEvents(END_DMA_Event);

	if (sb.dma_dac_mode)
		PIC_AddEvent(DMA_DAC_Event,1000.0 / sb.dma_dac_srcrate);

	if (sb.dma.chan != NULL) {
		sb.dma.chan->Register_Callback(DSP_DMA_CallBack);
	}
	else {
		LOG(LOG_SB,LOG_WARN)("DMA transfer initiated with no channel assigned");
	}

#if (C_DEBUG)
	LOG(LOG_SB,LOG_NORMAL)("DMA Transfer:%s %s %s freq %d rate %d size %d",
		type,
		sb.dma.stereo ? "Stereo" : "Mono",
		sb.dma.autoinit ? "Auto-Init" : "Single-Cycle",
		freq,sb.dma.rate,sb.dma.total
	);
#endif
}

static void DSP_PrepareDMA_Old(DMA_MODES mode,bool autoinit,bool sign) {
	/* Hack for Crystal Dream and any other bozo implementation that spams the DSP
	 * with command 0x14: if we're already playing audio, don't setup audio playback
	 * again. the reason this is important is that this setup process sets the mode
	 * to DMA_MASKED, which gives the mixer callback an opportunity to insert silence
	 * and thus cause popping and crackling. */
	if (sb.mode == MODE_DMA) {
		/* FIXME: So how does real hardware handle DSP command 0x14 while another
		 * DSP command 0x14 is in progress? */
		if (!autoinit) sb.dma.total=1+sb.dsp.in.data[0]+(sb.dsp.in.data[1] << 8);
		sb.dma.left=sb.dma.total;
		sb.dma.autoinit=autoinit;
		return;
	}

	/* FIXME: DSP 2.xx and 3.xx are said to require a reset to exit highspeed mode.
	 *        And the DSP does not accept writes. Is that true for SB16 hardware? */
	if (sb.dsp.cmd == 0x90 || sb.dsp.cmd == 0x91) { /* highspeed modes */
		/* Ignore high-speed DAC commands if Sound Blaster 1.xx */
		if (sb.type == SBT_1) return;
		sb.dsp.highspeed = true;
	}
	else {
		sb.dsp.highspeed = false;
	}

	if (sb.sample_rate_limits) { /* enforce speed limits documented by Creative */
		unsigned int u_limit=23000,l_limit=4000; /* NTS: Recording vs playback is not considered because DOSBox only emulates playback */

		if ((sb.dsp.cmd&0xFE) == 0x74 || sb.dsp.cmd == 0x7D) /* 4-bit ADPCM */
			u_limit = 12000;
		else if ((sb.dsp.cmd&0xFE) == 0x76) /* 2.6-bit ADPCM */
			u_limit = 13000;
		else if ((sb.dsp.cmd&0xFE) == 0x16) /* 2-bit ADPCM */
			u_limit = 11000;
		else if ((sb.dsp.cmd&0xFE) == 0x90) /* high-speed DAC */ {
			if (sb.type == SBT_1) /* 1.xx does not support */
				return;
			else
				u_limit = 44100; /* which is 22050Hz stereo when programmed DSP 3.x style */
		}
		/* NTS: We skip the SB16 commands because those are handled by another function */
		else {
			/* NTS: experience tells me that even on SB16 the traditional (pre 4.xx) DSP commands
			 *      are limited to 23KHz. TODO: What are the highspeed DAC commands limited to? 44.1KHz? 48KHz? */
			u_limit = 23000; /* non-highspeed limit is 23KHz */
		}

		/* NTS: Don't forget: Sound Blaster Pro "stereo" is programmed with a time constant divided by
		 *      two times the sample rate, which is what we get back here. That's why here we don't need
		 *      to consider stereo vs mono. */
		if (sb.freq < l_limit)
			sb.freq = l_limit;
		if (sb.freq > u_limit)
			sb.freq = u_limit;
	}

	sb.dma_dac_mode=0;
	sb.dma.autoinit=autoinit;
	sb.dma.sign=sign;
	if (!autoinit) sb.dma.total=1+sb.dsp.in.data[0]+(sb.dsp.in.data[1] << 8);
	sb.dma.chan=GetDMAChannel(sb.hw.dma8);
	DSP_DoDMATransfer(mode,sb.freq / (sb.mixer.stereo ? 2 : 1),sb.mixer.stereo);
}

static void DSP_PrepareDMA_New(DMA_MODES mode,Bitu length,bool autoinit,bool stereo) {
	sb.dsp.highspeed = false;
	if (sb.sample_rate_limits) { /* enforce speed limits documented by Creative */
		unsigned int u_limit=23000,l_limit=4000; /* NTS: Recording vs playback is not considered because DOSBox only emulates playback */

		if (sb.vibra) u_limit = 48000;
		else u_limit = 44100;

		if (sb.freq < l_limit)
			sb.freq = l_limit;
		if (sb.freq > u_limit)
			sb.freq = u_limit;
	}

	Bitu freq=sb.freq;
	//equal length if data format and dma channel are both 16-bit or 8-bit
	sb.dma_dac_mode=0;
	sb.dma.total=length;
	sb.dma.autoinit=autoinit;
	if (mode==DSP_DMA_16) {
		if (sb.hw.dma16 == 0xff || sb.hw.dma16 == sb.hw.dma8) { /* 16-bit DMA not assigned or same as 8-bit channel */
			sb.dma.chan=GetDMAChannel(sb.hw.dma8);
			mode=DSP_DMA_16_ALIASED;
			//UNDOCUMENTED:
			//In aliased mode sample length is written to DSP as number of
			//16-bit samples so we need double 8-bit DMA buffer length
			sb.dma.total<<=1;
		}
		else if (sb.hw.dma16 >= 4) { /* 16-bit DMA assigned to 16-bit DMA channel */
			sb.dma.chan=GetDMAChannel(sb.hw.dma16);
		}
		else {
			/* Nope. According to one ViBRA PnP card I have on hand, asking the
			 * card to do 16-bit DMA over 8-bit DMA only works if they are the
			 * same channel, otherwise, the card doesn't seem to carry out any
			 * DMA fetching. */
			sb.dma.chan=NULL;
			return;
		}
	} else {
		sb.dma.chan=GetDMAChannel(sb.hw.dma8);
	}

	DSP_DoDMATransfer(mode,freq,stereo);
}


static void DSP_AddData(Bit8u val) {
	if (sb.dsp.out.used<DSP_BUFSIZE) {
		Bitu start=sb.dsp.out.used+sb.dsp.out.pos;
		if (start>=DSP_BUFSIZE) start-=DSP_BUFSIZE;
		sb.dsp.out.data[start]=val;
		sb.dsp.out.used++;
	} else {
		LOG(LOG_SB,LOG_ERROR)("DSP:Data Output buffer full");
	}
}

static void DSP_BusyComplete(Bitu /*val*/) {
	sb.dsp.write_busy = 0;
}

static void DSP_FinishReset(Bitu /*val*/) {
	DSP_FlushData();
	DSP_AddData(0xaa);
	sb.dsp.state=DSP_S_NORMAL;
}

static void DSP_Reset(void) {
	LOG(LOG_SB,LOG_NORMAL)("DSP:Reset");
	PIC_DeActivateIRQ(sb.hw.irq);

	DSP_ChangeMode(MODE_NONE);
	DSP_FlushData();
	sb.dsp.cmd_len=0;
	sb.dsp.in.pos=0;
	sb.dsp.out.pos=0;
	sb.dsp.write_busy=0;
	PIC_RemoveEvents(DSP_FinishReset);
	PIC_RemoveEvents(DSP_BusyComplete);

	sb.dma.left=0;
	sb.dma.total=0;
	sb.dma.stereo=false;
	sb.dma.sign=false;
	sb.dma.autoinit=false;
	sb.dma.mode=DSP_DMA_NONE;
	sb.dma.remain_size=0;
	if (sb.dma.chan) sb.dma.chan->Clear_Request();

	sb.freq=22050;
	sb.time_constant=45;
	sb.dac.used=0;
	sb.dac.last=0;
	sb.e2.value=0xaa;
	sb.e2.count=0;
	sb.dsp.highspeed=0;
	sb.irq.pending_8bit=false;
	sb.irq.pending_16bit=false;
	sb.chan->SetFreq(22050);
//	DSP_SetSpeaker(false);
	PIC_RemoveEvents(END_DMA_Event);
	PIC_RemoveEvents(DMA_DAC_Event);
}

static void DSP_DoReset(Bit8u val) {
	if (((val&1)!=0) && (sb.dsp.state!=DSP_S_RESET)) {
//TODO Get out of highspeed mode
		DSP_Reset();
		sb.dsp.state=DSP_S_RESET;
	} else if (((val&1)==0) && (sb.dsp.state==DSP_S_RESET)) {	// reset off
		sb.dsp.state=DSP_S_RESET_WAIT;
		PIC_RemoveEvents(DSP_FinishReset);
		PIC_AddEvent(DSP_FinishReset,20.0f/1000.0f,0);	// 20 microseconds
	}
	sb.dsp.write_busy = 0;
}

static void DSP_E2_DMA_CallBack(DmaChannel * /*chan*/, DMAEvent event) {
	if (event==DMA_UNMASKED) {
		Bit8u val=(Bit8u)(sb.e2.value&0xff);
		DmaChannel * chan=GetDMAChannel(sb.hw.dma8);
		chan->Register_Callback(0);
		chan->Write(1,&val);
	}
}

static void DSP_ADC_CallBack(DmaChannel * /*chan*/, DMAEvent event) {
	if (event!=DMA_UNMASKED) return;
	Bit8u val=128;
	DmaChannel * ch=GetDMAChannel(sb.hw.dma8);
	while (sb.dma.left--) {
		ch->Write(1,&val);
	}
	SB_RaiseIRQ(SB_IRQ_8);
	ch->Register_Callback(0);
}

Bitu DEBUG_EnableDebugger(void);

#define DSP_SB16_ONLY if (sb.type != SBT_16) { LOG(LOG_SB,LOG_ERROR)("DSP:Command %2X requires SB16",sb.dsp.cmd); break; }
#define DSP_SB2_ABOVE if (sb.type <= SBT_1) { LOG(LOG_SB,LOG_ERROR)("DSP:Command %2X requires SB2 or above",sb.dsp.cmd); break; } 

/* Demo notes for fixing:
 *
 *  - "Buttman"'s intro uses a timer and DSP command 0x10 to play the sound effects even in Pro mode.
 *    It doesn't use DMA + IRQ until the music starts.
 */

static void DSP_DoCommand(void) {
//	LOG_MSG("DSP Command %X",sb.dsp.cmd);
	switch (sb.dsp.cmd) {
	case 0x04:
		if (sb.type == SBT_16) {
			/* SB16 ASP set mode register */
			if ((sb.dsp.in.data[0]&0xf1)==0xf1) ASP_init_in_progress=true;
			else ASP_init_in_progress=false;
			LOG(LOG_SB,LOG_NORMAL)("DSP Unhandled SB16ASP command %X (set mode register to %X)",sb.dsp.cmd,sb.dsp.in.data[0]);
		} else {
			/* DSP Status SB 2.0/pro version. NOT SB16. */
			DSP_FlushData();
			if (sb.type == SBT_2) DSP_AddData(0x88);
			else if ((sb.type == SBT_PRO1) || (sb.type == SBT_PRO2)) DSP_AddData(0x7b);
			else DSP_AddData(0xff);			//Everything enabled
		}
		break;
	case 0x05:	/* SB16 ASP set codec parameter */
		LOG(LOG_SB,LOG_NORMAL)("DSP Unhandled SB16ASP command %X (set codec parameter)",sb.dsp.cmd);
		break;
	case 0x08:	/* SB16 ASP get version */
		LOG(LOG_SB,LOG_NORMAL)("DSP Unhandled SB16ASP command %X sub %X",sb.dsp.cmd,sb.dsp.in.data[0]);
		if (sb.type == SBT_16) {
			switch (sb.dsp.in.data[0]) {
				case 0x03:
					DSP_AddData(0x18);	// version ID (??)
					break;
				default:
					LOG(LOG_SB,LOG_NORMAL)("DSP Unhandled SB16ASP command %X sub %X",sb.dsp.cmd,sb.dsp.in.data[0]);
					break;
			}
		} else {
			LOG(LOG_SB,LOG_NORMAL)("DSP Unhandled SB16ASP command %X sub %X",sb.dsp.cmd,sb.dsp.in.data[0]);
		}
		break;
	case 0x0e:	/* SB16 ASP set register */
		if (sb.type == SBT_16) {
//			LOG(LOG_SB,LOG_NORMAL)("SB16 ASP set register %X := %X",sb.dsp.in.data[0],sb.dsp.in.data[1]);
			ASP_regs[sb.dsp.in.data[0]] = sb.dsp.in.data[1];
		} else {
			LOG(LOG_SB,LOG_NORMAL)("DSP Unhandled SB16ASP command %X (set register)",sb.dsp.cmd);
		}
		break;
	case 0x0f:	/* SB16 ASP get register */
		if (sb.type == SBT_16) {
			if ((ASP_init_in_progress) && (sb.dsp.in.data[0]==0x83)) {
				ASP_regs[0x83] = ~ASP_regs[0x83];
			}
//			LOG(LOG_SB,LOG_NORMAL)("SB16 ASP get register %X == %X",sb.dsp.in.data[0],ASP_regs[sb.dsp.in.data[0]]);
			DSP_AddData(ASP_regs[sb.dsp.in.data[0]]);
		} else {
			LOG(LOG_SB,LOG_NORMAL)("DSP Unhandled SB16ASP command %X (get register)",sb.dsp.cmd);
		}
		break;
	case 0x10:	/* Direct DAC */
		DSP_ChangeMode(MODE_DAC);
		if (sb.dac.used<DSP_DACSIZE) {
			sb.dac.data[sb.dac.used++]=(Bit8s(sb.dsp.in.data[0] ^ 0x80)) << 8;
			sb.dac.data[sb.dac.used++]=(Bit8s(sb.dsp.in.data[0] ^ 0x80)) << 8;
		}
		break;
	case 0x24:	/* Singe Cycle 8-Bit DMA ADC */
		sb.dma.left=sb.dma.total=1+sb.dsp.in.data[0]+(sb.dsp.in.data[1] << 8);
		sb.dma.sign=false;
		LOG(LOG_SB,LOG_ERROR)("DSP:Faked ADC for %d bytes",sb.dma.total);
		GetDMAChannel(sb.hw.dma8)->Register_Callback(DSP_ADC_CallBack);
		break;
	case 0x14:	/* Singe Cycle 8-Bit DMA DAC */
	case 0x15:	/* Wari hack. Waru uses this one instead of 0x14, but some weird stuff going on there anyway */
	case 0x91:	/* Singe Cycle 8-Bit DMA High speed DAC */
		/* Note: 0x91 is documented only for DSP ver.2.x and 3.x, not 4.x */
		DSP_PrepareDMA_Old(DSP_DMA_8,false,false);
		break;
	case 0x90:	/* Auto Init 8-bit DMA High Speed */
	case 0x1c:	/* Auto Init 8-bit DMA */
		DSP_SB2_ABOVE; /* Note: 0x90 is documented only for DSP ver.2.x and 3.x, not 4.x */
		DSP_PrepareDMA_Old(DSP_DMA_8,true,false);
		break;
	case 0x38:  /* Write to SB MIDI Output */
		if (sb.midi == true) MIDI_RawOutByte(sb.dsp.in.data[0]);
		break;
	case 0x40:	/* Set Timeconstant */
		sb.freq=(256000000 / (65536 - (sb.dsp.in.data[0] << 8)));
		/* Nasty kind of hack to allow runtime changing of frequency */
		if (sb.dma.mode != DSP_DMA_NONE && sb.dma.autoinit) {
			DSP_PrepareDMA_Old(sb.dma.mode,sb.dma.autoinit,sb.dma.sign);
		}
		break;
	case 0x41:	/* Set Output Samplerate */
	case 0x42:	/* Set Input Samplerate */
		DSP_SB16_ONLY;
		sb.freq=(sb.dsp.in.data[0] << 8)  | sb.dsp.in.data[1];
		break;
	case 0x48:	/* Set DMA Block Size */
		DSP_SB2_ABOVE;
		//TODO Maybe check limit for new irq?
		sb.dma.total=1+sb.dsp.in.data[0]+(sb.dsp.in.data[1] << 8);
		break;
	case 0x75:	/* 075h : Single Cycle 4-bit ADPCM Reference */
		sb.adpcm.haveref=true;
	case 0x74:	/* 074h : Single Cycle 4-bit ADPCM */	
		DSP_PrepareDMA_Old(DSP_DMA_4,false,false);
		break;
	case 0x77:	/* 077h : Single Cycle 3-bit(2.6bit) ADPCM Reference*/
		sb.adpcm.haveref=true;
	case 0x76:  /* 074h : Single Cycle 3-bit(2.6bit) ADPCM */
		DSP_PrepareDMA_Old(DSP_DMA_3,false,false);
		break;
	case 0x7d:	/* Auto Init 4-bit ADPCM Reference */
		DSP_SB2_ABOVE;
		sb.adpcm.haveref=true;
		DSP_PrepareDMA_Old(DSP_DMA_4,true,false);
		break;
	case 0x7f:	/* Auto Init 3-bit(2.6bit) ADPCM Reference */
		DSP_SB2_ABOVE;
		sb.adpcm.haveref=true;
		DSP_PrepareDMA_Old(DSP_DMA_3,true,false);
		break;
	case 0x1f:	/* Auto Init 2-bit ADPCM Reference */
		DSP_SB2_ABOVE;
		sb.adpcm.haveref=true;
		DSP_PrepareDMA_Old(DSP_DMA_2,true,false);
		break;
	case 0x17:	/* 017h : Single Cycle 2-bit ADPCM Reference*/
		sb.adpcm.haveref=true;
	case 0x16:  /* 074h : Single Cycle 2-bit ADPCM */
		DSP_PrepareDMA_Old(DSP_DMA_2,false,false);
		break;
	case 0x80:	/* Silence DAC */
		PIC_AddEvent(&DSP_RaiseIRQEvent,
			(1000.0f*(1+sb.dsp.in.data[0]+(sb.dsp.in.data[1] << 8))/sb.freq));
		break;
	case 0xb0:	case 0xb1:	case 0xb2:	case 0xb3:  case 0xb4:	case 0xb5:	case 0xb6:	case 0xb7:
	case 0xb8:	case 0xb9:	case 0xba:	case 0xbb:  case 0xbc:	case 0xbd:	case 0xbe:	case 0xbf:
	case 0xc0:	case 0xc1:	case 0xc2:	case 0xc3:  case 0xc4:	case 0xc5:	case 0xc6:	case 0xc7:
	case 0xc8:	case 0xc9:	case 0xca:	case 0xcb:  case 0xcc:	case 0xcd:	case 0xce:	case 0xcf:
		DSP_SB16_ONLY;
		/* Generic 8/16 bit DMA */
//		DSP_SetSpeaker(true);		//SB16 always has speaker enabled
		sb.dma.sign=(sb.dsp.in.data[0] & 0x10) > 0;
		DSP_PrepareDMA_New((sb.dsp.cmd & 0x10) ? DSP_DMA_16 : DSP_DMA_8,
			1+sb.dsp.in.data[1]+(sb.dsp.in.data[2] << 8),
			(sb.dsp.cmd & 0x4)>0,
			(sb.dsp.in.data[0] & 0x20) > 0
		);
		break;
	case 0xd5:	/* Halt 16-bit DMA */
		DSP_SB16_ONLY;
	case 0xd0:	/* Halt 8-bit DMA */
//		DSP_ChangeMode(MODE_NONE);
//		Games sometimes already program a new dma before stopping, gives noise
		if (sb.mode==MODE_NONE) {
			// possibly different code here that does not switch to MODE_DMA_PAUSE
		}
		sb.mode=MODE_DMA_PAUSE;
		PIC_RemoveEvents(END_DMA_Event);
		PIC_RemoveEvents(DMA_DAC_Event);
		break;
	case 0xd1:	/* Enable Speaker */
		DSP_SetSpeaker(true);
		break;
	case 0xd3:	/* Disable Speaker */
		DSP_SetSpeaker(false);
		break;
	case 0xd8:  /* Speaker status */
		DSP_SB2_ABOVE;
		DSP_FlushData();
		if (sb.speaker) DSP_AddData(0xff);
		else DSP_AddData(0x00);
		break;
	case 0xd6:	/* Continue DMA 16-bit */
		DSP_SB16_ONLY;
	case 0xd4:	/* Continue DMA 8-bit*/
		if (sb.mode==MODE_DMA_PAUSE) {
			sb.mode=MODE_DMA_MASKED;
			if (sb.dma.chan!=NULL) sb.dma.chan->Register_Callback(DSP_DMA_CallBack);
		}
		break;
	case 0xd9:  /* Exit Autoinitialize 16-bit */
		DSP_SB16_ONLY;
	case 0xda:	/* Exit Autoinitialize 8-bit */
		DSP_SB2_ABOVE;
		/* Set mode to single transfer so it ends with current block */
		sb.dma.autoinit=false;		//Should stop itself
		break;
	case 0xe0:	/* DSP Identification - SB2.0+ */
		DSP_FlushData();
		DSP_AddData(~sb.dsp.in.data[0]);
		break;
	case 0xe1:	/* Get DSP Version */
		DSP_FlushData();
		switch (sb.type) {
		case SBT_1:
			DSP_AddData(0x1);DSP_AddData(0x05);break;
		case SBT_2:
			DSP_AddData(0x2);DSP_AddData(0x1);break;
		case SBT_PRO1:
			DSP_AddData(0x3);DSP_AddData(0x0);break;
		case SBT_PRO2:
			DSP_AddData(0x3);DSP_AddData(0x2);break;
		case SBT_16:
			DSP_AddData(0x4);DSP_AddData(0x5);break;
		default:
			break;
		}
		break;
	case 0xe2:	/* Weird DMA identification write routine */
		{
			LOG(LOG_SB,LOG_NORMAL)("DSP Function 0xe2");
			for (Bitu i = 0; i < 8; i++)
				if ((sb.dsp.in.data[0] >> i) & 0x01) sb.e2.value += E2_incr_table[sb.e2.count % 4][i];
			 sb.e2.value += E2_incr_table[sb.e2.count % 4][8];
			 sb.e2.count++;
			 GetDMAChannel(sb.hw.dma8)->Register_Callback(DSP_E2_DMA_CallBack);
		}
		break;
	case 0xe3:	/* DSP Copyright */
		{
			DSP_FlushData();
			for (size_t i=0;i<=strlen(copyright_string);i++) {
				DSP_AddData(copyright_string[i]);
			}
		}
		break;
	case 0xe4:	/* Write Test Register */
		sb.dsp.test_register=sb.dsp.in.data[0];
		break;
	case 0xe8:	/* Read Test Register */
		DSP_FlushData();
		DSP_AddData(sb.dsp.test_register);;
		break;
	case 0xf2:	/* Trigger 8bit IRQ */
		SB_RaiseIRQ(SB_IRQ_8);
		break;
	case 0xf3:   /* Trigger 16bit IRQ */
		DSP_SB16_ONLY; 
		SB_RaiseIRQ(SB_IRQ_16);
		break;
	case 0xf8:  /* Undocumented, pre-SB16 only */
		DSP_FlushData();
		DSP_AddData(0);
		break;
	case 0x30: case 0x31:
		LOG(LOG_SB,LOG_ERROR)("DSP:Unimplemented MIDI I/O command %2X",sb.dsp.cmd);
		break;
	case 0x34: case 0x35: case 0x36: case 0x37:
		DSP_SB2_ABOVE;
		LOG(LOG_SB,LOG_ERROR)("DSP:Unimplemented MIDI UART command %2X",sb.dsp.cmd);
		break;
	case 0x20:
		DSP_AddData(0x7f);   // fake silent input for Creative parrot
		break;
	case 0x2c:
	case 0x98: case 0x99: /* Documented only for DSP 2.x and 3.x */
	case 0xa0: case 0xa8: /* Documented only for DSP 3.x */
		LOG(LOG_SB,LOG_ERROR)("DSP:Unimplemented input command %2X",sb.dsp.cmd);
		break;
	case 0xf9:	/* SB16 ASP ??? */
		if (sb.type == SBT_16) {
			LOG(LOG_SB,LOG_NORMAL)("SB16 ASP unknown function %x",sb.dsp.in.data[0]);
			// just feed it what it expects
			switch (sb.dsp.in.data[0]) {
			case 0x0b:
				DSP_AddData(0x00);
				break;
			case 0x0e:
				DSP_AddData(0xff);
				break;
			case 0x0f:
				DSP_AddData(0x07);
				break;
			case 0x23:
				DSP_AddData(0x00);
				break;
			case 0x24:
				DSP_AddData(0x00);
				break;
			case 0x2b:
				DSP_AddData(0x00);
				break;
			case 0x2c:
				DSP_AddData(0x00);
				break;
			case 0x2d:
				DSP_AddData(0x00);
				break;
			case 0x37:
				DSP_AddData(0x38);
				break;
			default:
				DSP_AddData(0x00);
				break;
			}
		} else {
			LOG(LOG_SB,LOG_NORMAL)("SB16 ASP unknown function %X",sb.dsp.cmd);
		}
		break;
	default:
		LOG(LOG_SB,LOG_ERROR)("DSP:Unhandled (undocumented) command %2X",sb.dsp.cmd);
		break;
	}
	sb.dsp.cmd=DSP_NO_COMMAND;
	sb.dsp.cmd_len=0;
	sb.dsp.in.pos=0;
}

static void DSP_DoWrite(Bit8u val) {
	if (sb.dsp.write_busy || sb.dsp.highspeed) {
		LOG(LOG_SB,LOG_WARN)("DSP:Command write %2X ignored, DSP not ready. DOS game or OS is not polling status",val);
		return;
	}

	/* NTS: We allow the user to set busy wait time == 0 aka "instant gratification mode".
	 *      We also assume that if they do that, some DOS programs might be timing sensitive
	 *      enough to freak out when DSP commands and data are accepted immediately */
	{
		unsigned int delay = sb.dsp.dsp_write_busy_time;

		/* Part of enforcing sample rate limits is to make sure to emulate that the
		 * Direct DAC output command 0x10 is "busy" long enough to effectively rate
		 * limit output to 23KHz. */
		if (sb.sample_rate_limits) {
			if (sb.dsp.cmd == 0x10/*DSP direct DAC, this write is the data byte*/) {
				delay = (1000000000 / 25500/*Hz FIXME*/) - sb.dsp.dsp_write_busy_time;
			}
		}

		if (delay > 0) {
			sb.dsp.write_busy = 1;
			PIC_RemoveEvents(DSP_BusyComplete);
			PIC_AddEvent(DSP_BusyComplete,(double)delay / 1000000);
		}
	}

	switch (sb.dsp.cmd) {
		case DSP_NO_COMMAND:
			sb.dsp.cmd=val;
			if (sb.type == SBT_16) sb.dsp.cmd_len=DSP_cmd_len_sb16[val];
			else sb.dsp.cmd_len=DSP_cmd_len_sb[val];
			sb.dsp.in.pos=0;
			if (!sb.dsp.cmd_len) DSP_DoCommand();
			break;
		default:
			sb.dsp.in.data[sb.dsp.in.pos]=val;
			sb.dsp.in.pos++;
			if (sb.dsp.in.pos>=sb.dsp.cmd_len) DSP_DoCommand();
	}
}

static Bit8u DSP_ReadData(void) {
/* Static so it repeats the last value on succesive reads (JANGLE DEMO) */
	if (sb.dsp.out.used) {
		sb.dsp.out.lastval=sb.dsp.out.data[sb.dsp.out.pos];
		sb.dsp.out.pos++;
		if (sb.dsp.out.pos>=DSP_BUFSIZE) sb.dsp.out.pos-=DSP_BUFSIZE;
		sb.dsp.out.used--;
	}
	return sb.dsp.out.lastval;
}

//The soundblaster manual says 2.0 Db steps but we'll go for a bit less
#define CALCVOL(_VAL) (float)pow(10.0f,((float)(31-_VAL)*-1.3f)/20)
static void CTMIXER_UpdateVolumes(void) {
	if (!sb.mixer.enabled) return;
	MixerChannel * chan;
	//adjust to get linear master volume slider in trackers
	chan=MIXER_FindChannel("SB");
	if (chan) chan->SetVolume(float(sb.mixer.master[0])/31.0f*CALCVOL(sb.mixer.dac[0]),
							  float(sb.mixer.master[1])/31.0f*CALCVOL(sb.mixer.dac[1]));
	chan=MIXER_FindChannel("FM");
	if (chan) chan->SetVolume(float(sb.mixer.master[0])/31.0f*CALCVOL(sb.mixer.fm[0]),
							  float(sb.mixer.master[1])/31.0f*CALCVOL(sb.mixer.fm[1]));
	chan=MIXER_FindChannel("CDAUDIO");
	if (chan) chan->SetVolume(float(sb.mixer.master[0])/31.0f*CALCVOL(sb.mixer.cda[0]),
							  float(sb.mixer.master[1])/31.0f*CALCVOL(sb.mixer.cda[1]));
}

static void CTMIXER_Reset(void) {
	sb.mixer.fm[0]=
	sb.mixer.fm[1]=
	sb.mixer.cda[0]=
	sb.mixer.cda[1]=
	sb.mixer.dac[0]=
	sb.mixer.dac[1]=31;
	sb.mixer.master[0]=
	sb.mixer.master[1]=31;
	CTMIXER_UpdateVolumes();
}

#define SETPROVOL(_WHICH_,_VAL_)										\
	_WHICH_[0]=   ((((_VAL_) & 0xf0) >> 3)|(sb.type==SBT_16 ? 1:3));	\
	_WHICH_[1]=   ((((_VAL_) & 0x0f) << 1)|(sb.type==SBT_16 ? 1:3));	\

#define MAKEPROVOL(_WHICH_)			\
	((((_WHICH_[0] & 0x1e) << 3) | ((_WHICH_[1] & 0x1e) >> 1)) & (sb.type==SBT_16 ? 0xff:0xee))

static void DSP_ChangeStereo(bool stereo) {
	if (!sb.dma.stereo && stereo) {
		sb.chan->SetFreq(sb.freq/2);
		sb.dma.mul*=2;
		sb.dma.rate=(sb.freq*sb.dma.mul) >> SB_SH;
		sb.dma.min=(sb.dma.rate*3)/1000;
	} else if (sb.dma.stereo && !stereo) {
		sb.chan->SetFreq(sb.freq);
		sb.dma.mul/=2;
		sb.dma.rate=(sb.freq*sb.dma.mul) >> SB_SH;
		sb.dma.min=(sb.dma.rate*3)/1000;
	}
	sb.dma.stereo=stereo;
}

static void CTMIXER_Write(Bit8u val) {
	switch (sb.mixer.index) {
	case 0x00:		/* Reset */
		CTMIXER_Reset();
		LOG(LOG_SB,LOG_WARN)("Mixer reset value %x",val);
		break;
	case 0x02:		/* Master Volume (SB2 Only) */
		SETPROVOL(sb.mixer.master,(val&0xf)|(val<<4));
		CTMIXER_UpdateVolumes();
		break;
	case 0x04:		/* DAC Volume (SBPRO) */
		SETPROVOL(sb.mixer.dac,val);
		CTMIXER_UpdateVolumes();
		break;
	case 0x06:		/* FM output selection, Somewhat obsolete with dual OPL SBpro + FM volume (SB2 Only) */
		//volume controls both channels
		SETPROVOL(sb.mixer.fm,(val&0xf)|(val<<4));
		CTMIXER_UpdateVolumes();
		if(val&0x60) LOG(LOG_SB,LOG_WARN)("Turned FM one channel off. not implemented %X",val);
		//TODO Change FM Mode if only 1 fm channel is selected
		break;
	case 0x08:		/* CDA Volume (SB2 Only) */
		SETPROVOL(sb.mixer.cda,(val&0xf)|(val<<4));
		CTMIXER_UpdateVolumes();
		break;
	case 0x0a:		/* Mic Level (SBPRO) or DAC Volume (SB2): 2-bit, 3-bit on SB16 */
		if (sb.type==SBT_2) {
			sb.mixer.dac[0]=sb.mixer.dac[1]=((val & 0x6) << 2)|3;
			CTMIXER_UpdateVolumes();
		} else {
			sb.mixer.mic=((val & 0x7) << 2)|(sb.type==SBT_16?1:3);
		}
		break;
	case 0x0e:		/* Output/Stereo Select */
		/* only allow writing stereo bit if Sound Blaster Pro OR if a SB16 and user says to allow it */
		if ((sb.type == SBT_PRO1 || sb.type == SBT_PRO2) || (sb.type == SBT_16 && !sb.sbpro_stereo_bit_strict_mode))
			sb.mixer.stereo=(val & 0x2) > 0;

		sb.mixer.filtered=(val & 0x20) > 0;
		DSP_ChangeStereo(sb.mixer.stereo);
		LOG(LOG_SB,LOG_WARN)("Mixer set to %s",sb.dma.stereo ? "STEREO" : "MONO");
		break;
	case 0x22:		/* Master Volume (SBPRO) */
		SETPROVOL(sb.mixer.master,val);
		CTMIXER_UpdateVolumes();
		break;
	case 0x26:		/* FM Volume (SBPRO) */
		SETPROVOL(sb.mixer.fm,val);
		CTMIXER_UpdateVolumes();
		break;
	case 0x28:		/* CD Audio Volume (SBPRO) */
		SETPROVOL(sb.mixer.cda,val);
		CTMIXER_UpdateVolumes();
		break;
	case 0x2e:		/* Line-in Volume (SBPRO) */
		SETPROVOL(sb.mixer.lin,val);
		break;
	//case 0x20:		/* Master Volume Left (SBPRO) ? */
	case 0x30:		/* Master Volume Left (SB16) */
		if (sb.type==SBT_16) {
			sb.mixer.master[0]=val>>3;
			CTMIXER_UpdateVolumes();
		}
		break;
	//case 0x21:		/* Master Volume Right (SBPRO) ? */
	case 0x31:		/* Master Volume Right (SB16) */
		if (sb.type==SBT_16) {
			sb.mixer.master[1]=val>>3;
			CTMIXER_UpdateVolumes();
		}
		break;
	case 0x32:		/* DAC Volume Left (SB16) */
		if (sb.type==SBT_16) {
			sb.mixer.dac[0]=val>>3;
			CTMIXER_UpdateVolumes();
		}
		break;
	case 0x33:		/* DAC Volume Right (SB16) */
		if (sb.type==SBT_16) {
			sb.mixer.dac[1]=val>>3;
			CTMIXER_UpdateVolumes();
		}
		break;
	case 0x34:		/* FM Volume Left (SB16) */
		if (sb.type==SBT_16) {
			sb.mixer.fm[0]=val>>3;
			CTMIXER_UpdateVolumes();
		}
                break;
	case 0x35:		/* FM Volume Right (SB16) */
		if (sb.type==SBT_16) {
			sb.mixer.fm[1]=val>>3;
			CTMIXER_UpdateVolumes();
		}
		break;
	case 0x36:		/* CD Volume Left (SB16) */
		if (sb.type==SBT_16) {
			sb.mixer.cda[0]=val>>3;
			CTMIXER_UpdateVolumes();
		}
		break;
	case 0x37:		/* CD Volume Right (SB16) */
		if (sb.type==SBT_16) {
			sb.mixer.cda[1]=val>>3;
			CTMIXER_UpdateVolumes();
		}
		break;
	case 0x38:		/* Line-in Volume Left (SB16) */
		if (sb.type==SBT_16) sb.mixer.lin[0]=val>>3;
		break;
	case 0x39:		/* Line-in Volume Right (SB16) */
		if (sb.type==SBT_16) sb.mixer.lin[1]=val>>3;
		break;
	case 0x3a:
		if (sb.type==SBT_16) sb.mixer.mic=val>>3;
		break;
	case 0x80:		/* IRQ Select */
		if (sb.type==SBT_16 && !sb.vibra) { /* ViBRA PnP cards do not allow reconfiguration by this byte */
			sb.hw.irq=0xff;
			if (val & 0x1) sb.hw.irq=2;
			else if (val & 0x2) sb.hw.irq=5;
			else if (val & 0x4) sb.hw.irq=7;
			else if (val & 0x8) sb.hw.irq=10;
		}
		break;
	case 0x81:		/* DMA Select */
		if (sb.type==SBT_16 && !sb.vibra) { /* ViBRA PnP cards do not allow reconfiguration by this byte */
			sb.hw.dma8=0xff;
			sb.hw.dma16=0xff;
			if (val & 0x1) sb.hw.dma8=0;
			else if (val & 0x2) sb.hw.dma8=1;
			else if (val & 0x8) sb.hw.dma8=3;
			if (val & 0x20) sb.hw.dma16=5;
			else if (val & 0x40) sb.hw.dma16=6;
			else if (val & 0x80) sb.hw.dma16=7;
			LOG(LOG_SB,LOG_NORMAL)("Mixer select dma8:%x dma16:%x",sb.hw.dma8,sb.hw.dma16);
		}
		break;
	default:

		if(	((sb.type == SBT_PRO1 || sb.type == SBT_PRO2) && sb.mixer.index==0x0c) || /* Input control on SBPro */
			 (sb.type == SBT_16 && sb.mixer.index >= 0x3b && sb.mixer.index <= 0x47)) /* New SB16 registers */
			sb.mixer.unhandled[sb.mixer.index] = val;
		LOG(LOG_SB,LOG_WARN)("MIXER:Write %X to unhandled index %X",val,sb.mixer.index);
	}
}

static Bit8u CTMIXER_Read(void) {
	Bit8u ret;
//	if ( sb.mixer.index< 0x80) LOG_MSG("Read mixer %x",sb.mixer.index);
	switch (sb.mixer.index) {
	case 0x00:		/* RESET */
		return 0x00;
	case 0x02:		/* Master Volume (SB2 Only) */
		return ((sb.mixer.master[1]>>1) & 0xe);
	case 0x22:		/* Master Volume (SBPRO) */
		return	MAKEPROVOL(sb.mixer.master);
	case 0x04:		/* DAC Volume (SBPRO) */
		return MAKEPROVOL(sb.mixer.dac);
	case 0x06:		/* FM Volume (SB2 Only) + FM output selection */
		return ((sb.mixer.fm[1]>>1) & 0xe);
	case 0x08:		/* CD Volume (SB2 Only) */
		return ((sb.mixer.cda[1]>>1) & 0xe);
	case 0x0a:		/* Mic Level (SBPRO) or Voice (SB2 Only) */
		if (sb.type==SBT_2) return (sb.mixer.dac[0]>>2);
		else return ((sb.mixer.mic >> 2) & (sb.type==SBT_16 ? 7:6));
	case 0x0e:		/* Output/Stereo Select */
		return 0x11|(sb.mixer.stereo ? 0x02 : 0x00)|(sb.mixer.filtered ? 0x20 : 0x00);
	case 0x26:		/* FM Volume (SBPRO) */
		return MAKEPROVOL(sb.mixer.fm);
	case 0x28:		/* CD Audio Volume (SBPRO) */
		return MAKEPROVOL(sb.mixer.cda);
	case 0x2e:		/* Line-IN Volume (SBPRO) */
		return MAKEPROVOL(sb.mixer.lin);
	case 0x30:		/* Master Volume Left (SB16) */
		if (sb.type==SBT_16) return sb.mixer.master[0]<<3;
		ret=0xa;
		break;
	case 0x31:		/* Master Volume Right (S16) */
		if (sb.type==SBT_16) return sb.mixer.master[1]<<3;
		ret=0xa;
		break;
	case 0x32:		/* DAC Volume Left (SB16) */
		if (sb.type==SBT_16) return sb.mixer.dac[0]<<3;
		ret=0xa;
		break;
	case 0x33:		/* DAC Volume Right (SB16) */
		if (sb.type==SBT_16) return sb.mixer.dac[1]<<3;
		ret=0xa;
		break;
	case 0x34:		/* FM Volume Left (SB16) */
		if (sb.type==SBT_16) return sb.mixer.fm[0]<<3;
		ret=0xa;
		break;
	case 0x35:		/* FM Volume Right (SB16) */
		if (sb.type==SBT_16) return sb.mixer.fm[1]<<3;
		ret=0xa;
		break;
	case 0x36:		/* CD Volume Left (SB16) */
		if (sb.type==SBT_16) return sb.mixer.cda[0]<<3;
		ret=0xa;
		break;
	case 0x37:		/* CD Volume Right (SB16) */
		if (sb.type==SBT_16) return sb.mixer.cda[1]<<3;
		ret=0xa;
		break;
	case 0x38:		/* Line-in Volume Left (SB16) */
		if (sb.type==SBT_16) return sb.mixer.lin[0]<<3;
		ret=0xa;
		break;
	case 0x39:		/* Line-in Volume Right (SB16) */
		if (sb.type==SBT_16) return sb.mixer.lin[1]<<3;
		ret=0xa;
		break;
	case 0x3a:		/* Mic Volume (SB16) */
		if (sb.type==SBT_16) return sb.mixer.mic<<3;
		ret=0xa;
		break;
	case 0x80:		/* IRQ Select */
		ret=0;
		switch (sb.hw.irq) {
		case 2:  return 0x1;
		case 5:  return 0x2;
		case 7:  return 0x4;
		case 10: return 0x8;
		}
	case 0x81:		/* DMA Select */
		ret=0;
		switch (sb.hw.dma8) {
		case 0:ret|=0x1;break;
		case 1:ret|=0x2;break;
		case 3:ret|=0x8;break;
		}
		switch (sb.hw.dma16) {
		case 5:ret|=0x20;break;
		case 6:ret|=0x40;break;
		case 7:ret|=0x80;break;
		}
		return ret;
	case 0x82:		/* IRQ Status */
		return	(sb.irq.pending_8bit ? 0x1 : 0) |
				(sb.irq.pending_16bit ? 0x2 : 0) | 
				((sb.type == SBT_16) ? 0x20 : 0);
	default:
		if (	((sb.type == SBT_PRO1 || sb.type == SBT_PRO2) && sb.mixer.index==0x0c) || /* Input control on SBPro */
			(sb.type == SBT_16 && sb.mixer.index >= 0x3b && sb.mixer.index <= 0x47)) /* New SB16 registers */
			ret = sb.mixer.unhandled[sb.mixer.index];
		else
			ret=0xa;
		LOG(LOG_SB,LOG_WARN)("MIXER:Read from unhandled index %X",sb.mixer.index);
	}
	return ret;
}


static Bitu read_sb(Bitu port,Bitu /*iolen*/) {
	switch (port-sb.hw.base) {
	case MIXER_INDEX:
		return sb.mixer.index;
	case MIXER_DATA:
		return CTMIXER_Read();
	case DSP_READ_DATA:
		return DSP_ReadData();
	case DSP_READ_STATUS:
		//TODO See for high speed dma :)
		if (sb.irq.pending_8bit)  {
			sb.irq.pending_8bit=false;
			PIC_DeActivateIRQ(sb.hw.irq);
		}

		if (sb.mode == MODE_DMA_REQUIRE_IRQ_ACK)
			sb.mode = MODE_DMA;

		if (sb.dsp.out.used) return 0xff;
		else return 0x7f;
	case DSP_ACK_16BIT:
		if (sb.mode == MODE_DMA_REQUIRE_IRQ_ACK)
			sb.mode = MODE_DMA;

		sb.irq.pending_16bit=false;
		break;
	case DSP_WRITE_STATUS:
		switch (sb.dsp.state) {
		case DSP_S_NORMAL:
			if (sb.dsp.write_busy || sb.dsp.highspeed) return 0xff;
			return 0x7f;
		case DSP_S_RESET:
		case DSP_S_RESET_WAIT:
			return 0xff;
		}
		return 0xff;
	case DSP_RESET:
		return 0xff;
	default:
		LOG(LOG_SB,LOG_NORMAL)("Unhandled read from SB Port %4X",port);
		break;
	}
	return 0xff;
}

static void write_sb(Bitu port,Bitu val,Bitu /*iolen*/) {
	Bit8u val8=(Bit8u)(val&0xff);
	switch (port-sb.hw.base) {
	case DSP_RESET:
		DSP_DoReset(val8);
		break;
	case DSP_WRITE_DATA:
		DSP_DoWrite(val8);
		break;
	case MIXER_INDEX:
		sb.mixer.index=val8;
		break;
	case MIXER_DATA:
		CTMIXER_Write(val8);
		break;
	default:
		LOG(LOG_SB,LOG_NORMAL)("Unhandled write to SB Port %4X",port);
		break;
	}
}

static void adlib_gusforward(Bitu /*port*/,Bitu val,Bitu /*iolen*/) {
	adlib_commandreg=(Bit8u)(val&0xff);
}

bool SB_Get_Address(Bitu& sbaddr, Bitu& sbirq, Bitu& sbdma) {
	sbaddr=0;
	sbirq =0;
	sbdma =0;
	if (sb.type == SBT_NONE) return false;
	else {
		sbaddr=sb.hw.base;
		sbirq =sb.hw.irq;
		sbdma = sb.hw.dma8;
		return true;
	}
}

static void SBLASTER_CallBack(Bitu len) {
	switch (sb.mode) {
	case MODE_NONE:
	case MODE_DMA_PAUSE:
	case MODE_DMA_MASKED:
	case MODE_DMA_REQUIRE_IRQ_ACK:
		sb.chan->AddSilence();
		break;
	case MODE_DAC:
//		GenerateDACSound(len);
//		break;
		if (!sb.dac.used) {
			sb.mode=MODE_NONE;
			return;
		}
		sb.chan->AddStretched(sb.dac.used,sb.dac.data);
		sb.dac.used=0;
		break;
	case MODE_DMA:
		if (sb.dma_dac_mode) { /* might as well treat the micro-DMA transfers the same way we treat direct DAC output */
			if (sb.dac.used != 0) {
				if (sb.dma.stereo) sb.chan->AddStretchedStereo(sb.dac.used/2,sb.dac.data);
				else sb.chan->AddStretched(sb.dac.used,sb.dac.data);
				sb.dac.used=0;
			}
			else {
				sb.chan->AddSilence();
			}
		}
		else {
			len*=sb.dma.mul;
			if (len&SB_SH_MASK) len+=1 << SB_SH;
			len>>=SB_SH;
			if (len>sb.dma.left) len=sb.dma.left;
			GenerateDMASound(len);
		}
		break;
	}
}

#define ISAPNP_COMPATIBLE(id) \
	ISAPNP_SMALL_TAG(3,4), \
	( (id)        & 0xFF), \
	(((id) >>  8) & 0xFF), \
	(((id) >> 16) & 0xFF), \
	(((id) >> 24) & 0xFF)

static const unsigned char ViBRA_sysdev[] = {
	ISAPNP_IO_RANGE(
			0x01,					/* decodes 16-bit ISA addr */
			0x220,0x280,				/* min-max range I/O port */
			0x20,0x10),				/* align=0x20 length=0x10 */
	ISAPNP_IO_RANGE(
			0x01,					/* decodes 16-bit ISA addr */
			0x300,0x330,				/* min-max range I/O port */
			0x10,0x4),				/* align=0x10 length=0x4 */
	ISAPNP_IO_RANGE(
			0x01,					/* decodes 16-bit ISA addr */
			0x388,0x38C,				/* min-max range I/O port */
			0x4,0x4),				/* align=0x4 length=0x4 */
	ISAPNP_IRQ(
			(1 << 5) |
			(1 << 7) |
			(1 << 9) |
			(1 << 10) |
			(1 << 11),				/* IRQ 5,7,9,10,11 */
			0x09),					/* HTE=1 LTL=1 */
	ISAPNP_DMA(
			(1 << 0) |
			(1 << 1) |
			(1 << 3),				/* DMA 0,1,3 */
			0x01),					/* 8/16-bit */
	ISAPNP_DMA(
			(1 << 0) |
			(1 << 1) |
			(1 << 3) |
			(1 << 5) |
			(1 << 6) |
			(1 << 7),				/* DMA 0,1,3,5,6,7 */
			0x01),					/* 8/16-bit */
	ISAPNP_COMPATIBLE(ISAPNP_ID('C','T','L',0x0,0x0,0x0,0x1)),	/* <- Windows 95 doesn't know CTL0070 but does know CTL0001, this hack causes Win95 to offer it's internal driver as an option */
	ISAPNP_END
};

class ViBRA_PnP : public ISAPnPDevice {
	public:
		ViBRA_PnP() : ISAPnPDevice() {
			resource_ident = 0;
			resource_data = (unsigned char*)ViBRA_sysdev;
			resource_data_len = sizeof(ViBRA_sysdev);
			*((uint32_t*)(ident+0)) = ISAPNP_ID('C','T','L',0x0,0x0,0x7,0x0); /* CTL0070: ViBRA C */
			*((uint32_t*)(ident+4)) = 0xFFFFFFFFUL;
			checksum_ident();
		}
		void select_logical_device(Bitu val) {
			logical_device = val;
		}
		uint8_t read(Bitu addr) {
			uint8_t ret = 0xFF;
			if (logical_device == 0) {
				switch (addr) {
					case 0x60: case 0x61:	/* I/O [0] sound blaster */
						ret = sb.hw.base >> ((addr & 1) ? 0 : 8);
						break;
					case 0x62: case 0x63:	/* I/O [1] MPU */
						ret = 0x330 >> ((addr & 1) ? 0 : 8); /* FIXME: What I/O port really IS the MPU on? */
						break;
					case 0x64: case 0x65:	/* I/O [1] OPL-3 */
						ret = 0x388 >> ((addr & 1) ? 0 : 8); /* FIXME */
						break;
					case 0x70: /* IRQ[0] */
						ret = sb.hw.irq;
						break;
						/* TODO: 0x71 IRQ mode */
					case 0x74: /* DMA[0] */
						ret = sb.hw.dma8;
						break;
					case 0x75: /* DMA[1] */
						ret = sb.hw.dma16 == 0xFF ? sb.hw.dma8 : sb.hw.dma16;
						break;

				};
			}
			else if (logical_device == 1) {
				switch (addr) {
					case 0x60: case 0x61:	/* I/O [0] gameport */
						ret = 0x200 >> ((addr & 1) ? 0 : 8);
						break;
				};
			}

			return ret;
		}
		void write(Bitu addr,Bitu val) {
			if (logical_device == 0) {
				switch (addr) {
					case 0x30:	/* activate range */
						/* TODO: set flag to ignore writes/return 0xFF when "deactivated". setting bit 0 activates, clearing deactivates */
						break;
					case 0x60: case 0x61:	/* I/O [0] sound blaster */
						/* TODO: on-the-fly changing */
						//LOG_MSG("ISA PnP Warning: Sound Blaster I/O port changing not implemented yet\n");
						break;
					case 0x62: case 0x63:	/* I/O [1] MPU */
						/* TODO: on-the-fly changing */
						//LOG_MSG("ISA PnP Warning: MPU I/O port changing not implemented yet\n");
						break;
					case 0x64: case 0x65:	/* I/O [1] OPL-3 */
						/* TODO: on-the-fly changing */
						//LOG_MSG("ISA PnP Warning: OPL-3 I/O port changing not implemented yet\n");
						break;
					case 0x70: /* IRQ[0] */
						if (val & 0xF)
							sb.hw.irq = val;
						else
							sb.hw.irq = 0xFF;
						break;
					case 0x74: /* DMA[0] */
						if ((val & 7) == 4)
							sb.hw.dma8 = 0xFF;
						else
							sb.hw.dma8 = val & 7;
						break;
					case 0x75: /* DMA[1] */
						if ((val & 7) == 4)
							sb.hw.dma16 = 0xFF;
						else
							sb.hw.dma16 = val & 7;
						break;

				};
			}
			else if (logical_device == 1) {
				switch (addr) {
					case 0x60: case 0x61:	/* I/O [0] gameport */
						/* TODO: on-the-fly changing */
						//LOG_MSG("ISA PnP Warning: Gameport I/O port changing not implemented yet\n");
						break;
				};
			}
		}
};

extern void HARDOPL_Init(Bitu hardwareaddr, Bitu sbbase, bool isCMS);

class SBLASTER: public Module_base {
private:
	/* Data */
	IO_ReadHandleObject ReadHandler[0x10];
	IO_WriteHandleObject WriteHandler[0x10];
	AutoexecObject autoexecline;
	MixerObject MixerChan;
	OPL_Mode oplmode;

	/* Support Functions */
	void Find_Type_And_Opl(Section_prop* config,SB_TYPES& type, OPL_Mode& opl_mode){
		sb.vibra = false;
		const char * sbtype=config->Get_string("sbtype");
		if (!strcasecmp(sbtype,"sb1")) type=SBT_1;
		else if (!strcasecmp(sbtype,"sb2")) type=SBT_2;
		else if (!strcasecmp(sbtype,"sbpro1")) type=SBT_PRO1;
		else if (!strcasecmp(sbtype,"sbpro2")) type=SBT_PRO2;
		else if (!strcasecmp(sbtype,"sb16vibra")) type=SBT_16;
		else if (!strcasecmp(sbtype,"sb16")) type=SBT_16;
		else if (!strcasecmp(sbtype,"gb")) type=SBT_GB;
		else if (!strcasecmp(sbtype,"none")) type=SBT_NONE;
		else type=SBT_16;

		if (type == SBT_16) {
			/* NTS: mainline DOSBox forces the type to SBT_PRO2 if !IS_EGAVGA_ARCH or no secondary DMA controller.
			 *      I'd rather take the approach that, if the user wants to emulate a bizarre unusual setup like
			 *      sticking a Sound Blaster 16 in an 8-bit machine, they are free to do so on the understanding
			 *      it won't work properly (and emulation will show what happens). I also believe that tying
			 *      8-bit vs 16-bit system type to the *video card* was a really dumb move. */
			if (!SecondDMAControllerAvailable()) {
				LOG(LOG_SB,LOG_WARN)("Sound Blaster 16 enabled on a system without 16-bit DMA. Don't expect this setup to work properly! To improve compatability please edit your dosbox.conf and change sbtype to sbpro2 instead, or else enable the secondary DMA controller.");
			}
		}

		/* SB16 Vibra cards are Plug & Play */
		if (!strcasecmp(sbtype,"sb16vibra")) {
			ISA_PNP_devreg(new ViBRA_PnP());
			sb.vibra = true;
		}

		/* OPL/CMS Init */
		const char * omode=config->Get_string("oplmode");
		if (!strcasecmp(omode,"none")) opl_mode=OPL_none;	
		else if (!strcasecmp(omode,"cms")) opl_mode=OPL_cms;
		else if (!strcasecmp(omode,"opl2")) opl_mode=OPL_opl2;
		else if (!strcasecmp(omode,"dualopl2")) opl_mode=OPL_dualopl2;
		else if (!strcasecmp(omode,"opl3")) opl_mode=OPL_opl3;
		else if (!strcasecmp(omode,"hardware")) opl_mode=OPL_hardware;
		else if (!strcasecmp(omode,"hardwaregb")) opl_mode=OPL_hardwareCMS;
		/* Else assume auto */
		else {
			switch (type) {
			case SBT_NONE:
				opl_mode=OPL_none;
				break;
			case SBT_GB:
				opl_mode=OPL_cms;
				break;
			case SBT_1:
			case SBT_2:
				opl_mode=OPL_opl2;
				break;
			case SBT_PRO1:
				opl_mode=OPL_dualopl2;
				break;
			case SBT_PRO2:
			case SBT_16:
				opl_mode=OPL_opl3;
				break;
			}
		}	
	}
public:
	SBLASTER(Section* configuration):Module_base(configuration) {
		string s;
		Bitu i;
		int si;

		Section_prop * section=static_cast<Section_prop *>(configuration);

		sb.hw.base=section->Get_hex("sbbase");
		sb.goldplay=section->Get_bool("goldplay");
		sb.goldplay_stereo=section->Get_bool("goldplay stereo");
		sb.emit_blaster_var=section->Get_bool("blaster environment variable");
		sb.sample_rate_limits=section->Get_bool("sample rate limits");
		sb.sbpro_stereo_bit_strict_mode=section->Get_bool("stereo control with sbpro only");

		si=section->Get_int("irq");
		sb.hw.irq=(si >= 0) ? si : 0xFF;

		si=section->Get_int("dma");
		sb.hw.dma8=(si >= 0) ? si : 0xFF;

		si=section->Get_int("hdma");
		sb.hw.dma16=(si >= 0) ? si : 0xFF;

		if (sb.hw.irq == 0xFF || sb.hw.dma8 == 0xFF) {
			LOG(LOG_SB,LOG_WARN)("IRQ and 8-bit DMA not assigned, disabling BLASTER variable");
			sb.emit_blaster_var = false;
		}

		sb.mixer.enabled=section->Get_bool("sbmixer");
		sb.mixer.stereo=false;
		OPL_Mode opl_mode = OPL_none;
		Find_Type_And_Opl(section,sb.type,oplmode);
		bool isCMSpassthrough = false;
	
		switch (oplmode) {
		case OPL_none:
			WriteHandler[0].Install(0x388,adlib_gusforward,IO_MB);
			break;
		case OPL_cms:
			WriteHandler[0].Install(0x388,adlib_gusforward,IO_MB);
			CMS_Init(section);
			break;
		case OPL_opl2:
			CMS_Init(section);
			// fall-through
		case OPL_dualopl2:
		case OPL_opl3:
			OPL_Init(section,oplmode);
			break;
		case OPL_hardwareCMS:
			isCMSpassthrough = true;
		case OPL_hardware:
			Bitu base = section->Get_hex("hardwarebase");
			HARDOPL_Init(base, sb.hw.base, isCMSpassthrough);
			break;
		}
		if (sb.type==SBT_NONE || sb.type==SBT_GB) return;

		sb.chan=MixerChan.Install(&SBLASTER_CallBack,22050,"SB");
		sb.dsp.state=DSP_S_NORMAL;
		sb.dsp.out.lastval=0xaa;
		sb.dma.chan=NULL;

		for (i=4;i<=0xf;i++) {
			if (i==8 || i==9) continue;
			//Disable mixer ports for lower soundblaster
			if ((sb.type==SBT_1 || sb.type==SBT_2) && (i==4 || i==5)) continue; 
			ReadHandler[i].Install(sb.hw.base+i,read_sb,IO_MB);
			WriteHandler[i].Install(sb.hw.base+i,write_sb,IO_MB);
		}
		for (i=0;i<256;i++) ASP_regs[i] = 0;
		ASP_regs[5] = 0x01;
		ASP_regs[9] = 0xf8;

		DSP_Reset();
		CTMIXER_Reset();

		// The documentation does not specify if SB gets initialized with the speaker enabled
		// or disabled. Real SBPro2 has it disabled. 
		sb.speaker=false;
		// On SB16 the speaker flag does not affect actual speaker state.
		if (sb.type == SBT_16) sb.chan->Enable(true);
		else sb.chan->Enable(false);

		if (sb.emit_blaster_var) {
			// Create set blaster line
			ostringstream temp;
			temp << "SET BLASTER=A" << setw(3) << hex << sb.hw.base;
			if (sb.hw.irq != 0xFF) temp << " I" << dec << (Bitu)sb.hw.irq;
			if (sb.hw.dma8 != 0xFF) temp << " D" << (Bitu)sb.hw.dma8;
			if (sb.type==SBT_16 && sb.hw.dma16 != 0xFF) temp << " H" << (Bitu)sb.hw.dma16;
			temp << " T" << static_cast<unsigned int>(sb.type) << ends;

			autoexecline.Install(temp.str());
		}

		s=section->Get_string("dsp require interrupt acknowledge");
		if (s == "true" || s == "1" || s == "on")
			sb.dsp.require_irq_ack = 1;
		else if (s == "false" || s == "0" || s == "off")
			sb.dsp.require_irq_ack = 0;
		else /* auto */
			sb.dsp.require_irq_ack = (sb.type == SBT_16) ? 1 : 0; /* Yes if SB16, No otherwise */

		si=section->Get_int("dsp write busy delay"); /* in nanoseconds */
		if (si >= 0) sb.dsp.dsp_write_busy_time = si;
		else sb.dsp.dsp_write_busy_time = 15000; /* FIXME: How long is the DSP busy on real hardware? */

		/* Soundblaster midi interface */
		if (!MIDI_Available()) sb.midi = false;
		else sb.midi = true;
	}	

	void DOS_Shutdown() { /* very likely, we're booting into a guest OS where our environment variable has no meaning anymore */
		autoexecline.Uninstall();
	}
	
	~SBLASTER() {
		switch (oplmode) {
		case OPL_none:
			break;
		case OPL_cms:
			CMS_ShutDown(m_configuration);
			break;
		case OPL_opl2:
			CMS_ShutDown(m_configuration);
			// fall-through
		case OPL_dualopl2:
		case OPL_opl3:
			OPL_ShutDown(m_configuration);
			break;
		}
		if (sb.type==SBT_NONE || sb.type==SBT_GB) return;
		DSP_Reset(); // Stop everything	
	}	
}; //End of SBLASTER class

extern void HWOPL_Cleanup();

static SBLASTER* test = NULL;

void SBLASTER_DOS_Shutdown() {
	if (test != NULL) test->DOS_Shutdown();
}

void SBLASTER_ShutDown(Section* /*sec*/) {
	if (test != NULL) {
		delete test;	
		test = NULL;
	}
	HWOPL_Cleanup();
}

void SBLASTER_Init(Section* sec) {
	test = new SBLASTER(sec);
	sec->AddDestroyFunction(&SBLASTER_ShutDown,true);
}

