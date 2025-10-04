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
/* TODO: I noticed a "bug" on a SB16 ViBRA where if you use single-cycle DMA
 *       playback with DSP 4.xx commands with the FIFO enabled, the card seems
 *       to buffer through the FIFO, play the block, and then when the DSP
 *       block completes, doesn't play what remains in the FIFO and stops
 *       immediately. So, if you do very small DSP block single-cycle transfers
 *       using the SB16 0xB0-0xCF DSP commands, the audio will play fast because
 *       the last 16-32 samples are being skipped, effectively.
 *
 *       I also noticed (related to this) that Creative's documentation only
 *       lists using 0xB0/0xC0 for single-cycle playback, OR using 0xB6/0xC6
 *       for autoinit playback, in other words either single-cycle without FIFO
 *       or autoinit with FIFO.
 *
 *       Also noticed is that the DSP "nag" mode in my test program can interrupt
 *       the SB16's DSP chip at the right time (with auto-init DMA) that it can
 *       cause the DSP to drop a byte and effectively cause stereo left/right
 *       swapping. It can also cause 16-bit DMA to halt.
 *
 *       As usual, expect this to be a config option. --Jonathan C. */

/* FIXME: Sound Blaster 16 hardware has a FIFO between the ISA BUS and DSP.
 *        Could we update this code to read through a FIFO instead? How big is this
 *        FIFO anyway, and which cards have it? Would it also be possible to eliminate
 *        the need for sb.dma.min? */

/* Notes:
 *
 *   - Windows 95, when using the Creative SB16 drivers, seems to always use a 62ms
 *     DSP block size, DSP and DMA auto-init, within a 124ms buffer
 *     (DSP block size 1376 / DMA base count 2751 / sample rate 22050 for example).
 *
 *     The calculation seems to be Nhalf = ((sample_rate / 16) & (~3)) * bytes_per_sample
 *     and N = Nhalf * 2. It appears to round down to a multiple of 4 to correctly handle
 *     up to 16-bit stereo, of course.
 *
 *     What I'm trying to investigate are the cases where something, like CD-ROM IDE
 *     access, throws that off and causes audible popping and crackling.
 *
 *   - Windows 3.1 SB16: Holy Hell, do NOT attempt to play audio at 48KHz or higher!
 *     There seems to be a serious bug with the Creative SB16 driver where 48KHz playback
 *     results in garbled audio and soon a driver crash. 44.1KHz or lower is OK.
 *     Is this some kind of DMA vs timing bug? It also appears to take whatever sample
 *     rate is given by the Windows application and give it as-is to the DSP. The breaking
 *     point seems to be 45,100Hz.
 */

#if defined(_MSC_VER)
# pragma warning(disable:4244) /* const fmath::local::uint64_t to double possible loss of data */
# pragma warning(disable:4305) /* truncation from double to float */
# pragma warning(disable:4065) /* switch without case */
#define _USE_MATH_DEFINES      /* needed for M_PI definition */
//#define M_PI       3.14159265358979323846   /* pi */
#endif

#include <assert.h>
#include <iomanip>
#include <sstream>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "dosbox.h"
#include "inout.h"
#include "logging.h"
#include "mixer.h"
#include "dma.h"
#include "pic.h"
#include "bios.h"
#include "hardware.h"
#include "control.h"
#include "setup.h"
#include "support.h"
#include "shell.h"
#include "hardopl.h"
using namespace std;

#define MAX_CARDS 2

#define CARD_INDEX_BIT 28u

int MPU401_GetIRQ();
void MIDI_RawOutByte(uint8_t data);
bool MIDI_Available(void);
bool JOYSTICK_IsEnabled(Bitu which);
Bitu DEBUG_EnableDebugger(void);

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

#ifndef max
#define max(a,b) ((a)>(b)?(a):(b))
#endif
#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif

#define MIN_ADAPTIVE_STEP_SIZE 0
#define MAX_ADAPTIVE_STEP_SIZE 32767
#define DC_OFFSET_FADE 254

//Should be enough for sound generated in millisecond blocks
#define SB_SH   14
#define SB_SH_MASK  ((1 << SB_SH)-1)

enum {DSP_S_RESET,DSP_S_RESET_WAIT,DSP_S_NORMAL,DSP_S_HIGHSPEED};
enum SB_TYPES {SBT_NONE=0,SBT_GB=1,SBT_1=2,SBT_2=3,SBT_PRO1=4,SBT_PRO2=5,SBT_16=6};
enum SB_SUBTYPES {SBST_NONE,SBST_100,SBST_105,SBST_200,SBST_201};
enum REVEAL_SC_TYPES {RSC_NONE=0,RSC_SC400=1};
enum SB_IRQS {SB_IRQ_8,SB_IRQ_16,SB_IRQ_MPU};
enum ESS_TYPES {ESS_NONE=0,ESS_688=1,ESS_1688=2};

enum DSP_MODES {
	MODE_NONE,
	MODE_DAC,
	MODE_DMA,
	MODE_DMA_PAUSE,
	MODE_DMA_MASKED,
	MODE_DMA_REQUIRE_IRQ_ACK        /* Sound Blaster 16 style require IRQ ack to resume DMA */
};

enum DMA_MODES {
	DSP_DMA_NONE,
	DSP_DMA_2,DSP_DMA_3,DSP_DMA_4,DSP_DMA_8,
	DSP_DMA_16,DSP_DMA_16_ALIASED
};

enum {
	PLAY_MONO,PLAY_STEREO
};

enum {
	REC_SILENCE=0,
	REC_1KHZ_TONE,
	REC_HISS
};

static char const * const copyright_string="COPYRIGHT (C) CREATIVE TECHNOLOGY LTD, 1992.";

// number of bytes in input for commands (sb/sbpro)
static uint8_t const DSP_cmd_len_sb[256] = {
	0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x00
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

// number of bytes in input for commands (sb/sbpro compatible Reveal SC400)
static uint8_t const DSP_cmd_len_sc400[256] = {
	0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x00
	1,0,0,0, 2,2,2,2, 0,0,0,0, 0,0,0,0,  // 0x10 Wari hack
	0,0,0,0, 2,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x20
	0,0,0,0, 0,0,0,0, 1,0,0,0, 0,0,0,0,  // 0x30

	1,2,2,0, 0,0,0,0, 2,0,0,0, 0,0,0,0,  // 0x40
	1,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x50
	0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,2,0,  // 0x60
	0,0,0,0, 2,2,2,2, 0,0,0,0, 0,0,0,0,  // 0x70

	2,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x80
	0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x90
	0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0xa0
	3,3,3,3, 3,3,3,3, 3,3,3,3, 3,3,3,3,  // 0xb0

	3,3,3,3, 3,3,3,3, 3,3,3,3, 3,3,3,3,  // 0xc0
	0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0xd0
	1,0,1,0, 1,0,0,0, 0,0,0,0, 0,0,0,0,  // 0xe0
	0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0   // 0xf0
};

// number of bytes in input for commands (sbpro2 compatible ESS)
static uint8_t const DSP_cmd_len_ess[256] = {
	0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x00
	1,0,0,0, 2,2,2,2, 0,0,0,0, 0,0,0,0,  // 0x10 Wari hack
	0,0,0,0, 2,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x20
	0,0,0,0, 0,0,0,0, 1,0,0,0, 0,0,0,0,  // 0x30

	1,2,2,0, 0,0,0,0, 2,0,0,0, 0,0,0,0,  // 0x40
	0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x50
	0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x60
	0,0,0,0, 2,2,2,2, 0,0,0,0, 0,0,0,0,  // 0x70

	2,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x80
	0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x90
	1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1,  // 0xa0   ESS write register commands (0xA0-0xBF). Note this overlap with SB16 is
	1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1,  // 0xb0   why ESS chipsets cannot emulate SB16 playback/record commands.

	1,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0xc0   ESS additional commands.
	0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0xd0
	1,0,1,0, 1,0,0,0, 0,0,0,0, 0,0,0,0,  // 0xe0
	0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0   // 0xf0
};

// number of bytes in input for commands (sb16)
static uint8_t const DSP_cmd_len_sb16[256] = {
	0,0,0,0, 1,2,0,0, 1,0,0,0, 0,0,2,1,  // 0x00
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
	0,0,0,0, 0,0,0,0, 0,1,2,0, 0,0,0,0   // 0xf0
};

static const int8_t scaleMap_ADPCM4[64] = {
	0,  1,  2,  3,  4,  5,  6,  7,  0,  -1,  -2,  -3,  -4,  -5,  -6,  -7,
	1,  3,  5,  7,  9, 11, 13, 15, -1,  -3,  -5,  -7,  -9, -11, -13, -15,
	2,  6, 10, 14, 18, 22, 26, 30, -2,  -6, -10, -14, -18, -22, -26, -30,
	4, 12, 20, 28, 36, 44, 52, 60, -4, -12, -20, -28, -36, -44, -52, -60
};
static const uint8_t adjustMap_ADPCM4[64] = {
	  0, 0, 0, 0, 0, 16, 16, 16,
	  0, 0, 0, 0, 0, 16, 16, 16,
	240, 0, 0, 0, 0, 16, 16, 16,
	240, 0, 0, 0, 0, 16, 16, 16,
	240, 0, 0, 0, 0, 16, 16, 16,
	240, 0, 0, 0, 0, 16, 16, 16,
	240, 0, 0, 0, 0,  0,  0,  0,
	240, 0, 0, 0, 0,  0,  0,  0
};

static const int8_t scaleMap_ADPCM3[40] = {
	0,  1,  2,  3,  0,  -1,  -2,  -3,
	1,  3,  5,  7, -1,  -3,  -5,  -7,
	2,  6, 10, 14, -2,  -6, -10, -14,
	4, 12, 20, 28, -4, -12, -20, -28,
	5, 15, 25, 35, -5, -15, -25, -35
};
static const uint8_t adjustMap_ADPCM3[40] = {
	  0, 0, 0, 8,   0, 0, 0, 8,
	248, 0, 0, 8, 248, 0, 0, 8,
	248, 0, 0, 8, 248, 0, 0, 8,
	248, 0, 0, 8, 248, 0, 0, 8,
	248, 0, 0, 0, 248, 0, 0, 0
};

static const int8_t scaleMap_ADPCM2[24] = {
	0,  1,  0,  -1,  1,  3,  -1,  -3,
	2,  6, -2,  -6,  4, 12,  -4, -12,
	8, 24, -8, -24, 16, 48, -16, -48
};
static const uint8_t adjustMap_ADPCM2[24] = {
	  0, 4,   0, 4,
	252, 4, 252, 4, 252, 4, 252, 4,
	252, 4, 252, 4, 252, 4, 252, 4,
	252, 0, 252, 0
};

static void DMA_DAC_Event(Bitu);
static void END_DMA_Event(Bitu);
static void DMA_Silent_Event(Bitu val);
static void DSP_FinishReset(Bitu val);
static void DSP_BusyComplete(Bitu val);
static void DSP_RaiseIRQEvent(Bitu val);
static void DSP_DMA_CallBack(DmaChannel * chan, DMAEvent event);
static void DSP_E2_DMA_CallBack(DmaChannel *chan, DMAEvent event);
static void DSP_SC400_E6_DMA_CallBack(DmaChannel *chan, DMAEvent event);

static INLINE uint8_t expand16to32(const uint8_t t) {
	/* 4-bit -> 5-bit expansion.
	 *
	 * 0 -> 0
	 * 1 -> 2
	 * 2 -> 4
	 * 3 -> 6
	 * ....
	 * 7 -> 14
	 * 8 -> 17
	 * 9 -> 19
	 * 10 -> 21
	 * 11 -> 23
	 * ....
	 * 15 -> 31 */
	return (t << 1) | (t >> 3);
}

static INLINE uint8_t decode_ADPCM_4_sample(uint8_t sample,uint8_t & reference,Bits& scale) {
	Bits samp = sample + scale;

	if ((samp < 0) || (samp > 63)) {
		LOG(LOG_SB,LOG_ERROR)("Bad ADPCM-4 sample");
		if(samp < 0 ) samp =  0;
		if(samp > 63) samp = 63;
	}

	Bits ref = reference + scaleMap_ADPCM4[samp];
	if (ref > 0xff) reference = 0xff;
	else if (ref < 0x00) reference = 0x00;
	else reference = (uint8_t)(ref&0xff);
	scale = (scale + adjustMap_ADPCM4[samp]) & 0xff;

	return reference;
}

INLINE uint8_t decode_ADPCM_3_sample(uint8_t sample,uint8_t & reference,Bits& scale) {
	Bits samp = sample + scale;
	if ((samp < 0) || (samp > 39)) {
		LOG(LOG_SB,LOG_ERROR)("Bad ADPCM-3 sample");
		if(samp < 0 ) samp =  0;
		if(samp > 39) samp = 39;
	}

	Bits ref = reference + scaleMap_ADPCM3[samp];
	if (ref > 0xff) reference = 0xff;
	else if (ref < 0x00) reference = 0x00;
	else reference = (uint8_t)(ref&0xff);
	scale = (scale + adjustMap_ADPCM3[samp]) & 0xff;

	return reference;
}

static INLINE uint8_t decode_ADPCM_2_sample(uint8_t sample,uint8_t & reference,Bits& scale) {
	Bits samp = sample + scale;
	if ((samp < 0) || (samp > 23)) {
		LOG(LOG_SB,LOG_ERROR)("Bad ADPCM-2 sample");
		if(samp < 0 ) samp =  0;
		if(samp > 23) samp = 23;
	}

	Bits ref = reference + scaleMap_ADPCM2[samp];
	if (ref > 0xff) reference = 0xff;
	else if (ref < 0x00) reference = 0x00;
	else reference = (uint8_t)(ref&0xff);
	scale = (scale + adjustMap_ADPCM2[samp]) & 0xff;

	return reference;
}

#define DSP_SB16_ONLY if (type != SBT_16) { LOG(LOG_SB,LOG_ERROR)("DSP:Command %2X requires SB16",dsp.cmd); break; }
#define DSP_SB2_ABOVE if (type <= SBT_1) { LOG(LOG_SB,LOG_ERROR)("DSP:Command %2X requires SB2 or above",dsp.cmd); break; }
#define DSP_SB201_ABOVE if (type <= SBT_1 || (type == SBT_2 && subtype == SBST_200)) { LOG(LOG_SB,LOG_ERROR)("DSP:Command %2X requires SB2.01 or above",dsp.cmd); break; }

#define SETPROVOL(_WHICH_,_VAL_)                                        \
    _WHICH_[0]=   ((((_VAL_) & 0xf0) >> 3)|(type==SBT_16 ? 1:3));    \
    _WHICH_[1]=   ((((_VAL_) & 0x0f) << 1)|(type==SBT_16 ? 1:3));    \

#define MAKEPROVOL(_WHICH_)											\
	((((_WHICH_[0] & 0x1e) << 3) | ((_WHICH_[1] & 0x1e) >> 1)) |	\
		((type==SBT_PRO1 || type==SBT_PRO2) ? 0x11:0))

struct SB_INFO {
	Bitu freq;
	uint8_t timeconst;
	Bitu dma_dac_srcrate;
	struct {
		bool recording;
		bool stereo,sign,autoinit;
		bool force_autoinit;
		DMA_MODES mode_assigned;
		DMA_MODES mode;
		Bitu rate,mul;
		Bitu total,left,min;
		uint64_t start;
		union {
			uint8_t  b8[DMA_BUFSIZE];
			int16_t b16[DMA_BUFSIZE];
		} buf;
		Bitu bits;
		DmaChannel * chan;
		Bitu remain_size;
	} dma;
	bool freq_derived_from_tc;      // if set, sb.freq was derived from SB/SBpro time constant
	bool def_enable_speaker;        // start emulation with SB speaker enabled
	bool enable_asp;
	bool speaker;
	bool midi;
	bool vibra;
	bool emit_blaster_var;
	bool sbpro_stereo_bit_strict_mode; /* if set, stereo bit in mixer can only be set if emulating a Pro. if clear, SB16 can too */
	bool sample_rate_limits; /* real SB hardware has limits on the sample rate */
	bool single_sample_dma;
	bool directdac_warn_speaker_off; /* if set, warn if DSP command 0x10 is being used while the speaker is turned off */
	bool dma_dac_mode; /* some very old DOS demos "play" sound by setting the DMA terminal count to 0.
			      normally that would mean the DMA controller transmitting the same byte at the sample rate,
			      except that the program creates sound by overwriting that byte periodically.
			      on actual hardware this happens to work (though with kind of a gritty sound to it),
			      The DMA emulation here does not handle that well. */
	bool goldplay;
	bool goldplay_stereo;
	bool write_status_must_return_7f; // WRITE_STATUS (port base+0xC) must return 0x7F or 0xFF if set. Some very early demos rely on it.
	bool busy_cycle_always;
	bool ess_playback_mode;
	bool no_filtering;
	bool cms;
	uint8_t sc400_cfg;
	uint8_t time_constant;
	uint8_t sc400_dsp_major,sc400_dsp_minor;
	uint8_t sc400_jumper_status_1;
	uint8_t sc400_jumper_status_2;
	DSP_MODES mode;
	SB_TYPES type;
	SB_SUBTYPES subtype;
	REVEAL_SC_TYPES reveal_sc_type; // Reveal SC400 type
	ESS_TYPES ess_type; // ESS chipset emulation, to be set only if type == SBT_PRO2
	bool ess_extended_mode;
	int min_dma_user;
	int busy_cycle_hz;
	int busy_cycle_duty_percent;
	int busy_cycle_io_hack;
	double busy_cycle_last_check;
	double busy_cycle_last_poll;
	struct {
		bool pending_8bit;
		bool pending_16bit;
	} irq;
	struct {
		uint8_t state;
		uint8_t last_cmd;
		uint8_t cmd;
		uint8_t cmd_len;
		uint8_t cmd_in_pos;
		uint8_t cmd_in[DSP_BUFSIZE];
		struct {
			uint8_t lastval;
			uint8_t data[DSP_BUFSIZE];
			Bitu pos,used;
		} in,out;
		uint8_t test_register;
		Bitu write_busy;
		bool highspeed;
		bool require_irq_ack;
		bool instant_direct_dac;
		bool force_goldplay;
		bool midi_rwpoll_mode; // DSP command 0x34/0x35 MIDI Read Poll & Write Poll (0x35 with interrupt)
		bool midi_read_interrupt;
		bool midi_read_with_timestamps;
		bool command_aliases;
		unsigned int dsp_write_busy_time; /* when you write to the DSP, how long it signals "busy" */
	} dsp;
	struct {
		int16_t last;
		double dac_t,dac_pt;
	} dac;
	struct {
		uint8_t index;
		uint8_t dac[2],fm[2],cda[2],master[2],lin[2];
		uint8_t mic;
		bool stereo;
		bool enabled;
		bool filter_bypass;
		bool sbpro_stereo; /* Game or OS is attempting SB Pro type stereo */
		uint8_t unhandled[0x48];
		uint8_t ess_id_str[4];
		uint8_t ess_id_str_pos;
	} mixer;
	struct {
		uint8_t reference;
		Bits stepsize;
		bool haveref;
	} adpcm;
	struct {
		Bitu base;
		Bitu irq;
		uint8_t dma8,dma16;
		bool sb_io_alias;
	} hw;
	struct {
		uint8_t valadd;
		uint8_t valxor;
	} e2;
	double last_dma_callback = 0.0f;
	unsigned int recording_source = REC_SILENCE;
	bool listen_to_recording_source = false;
	uint8_t ASP_regs[256];
	uint8_t ASP_mode = 0x00;
	unsigned char ESSregs[0x20] = {0}; /* 0xA0-0xBF */
	MixerChannel * chan;
	unsigned int gen_input_ofs = 0;
	unsigned long long gen_tone_angle = 0;
	unsigned int gen_hiss_rand[2] = {0,0};
	size_t card_index = 0;
	int gen_last_hiss = 0;

	/* SB16 cards have a 256-byte block of 8051 internal RAM accessible through DSP commands 0xF9 (Read) and 0xFA (Write) */
	unsigned char sb16_8051_mem[256];

	/* The SB16 ASP appears to have a mystery 2KB RAM block that is accessible through register 0x83 of the ASP.
	 * This array represents the initial contents as seen on my SB16 non-PnP ASP chip (version ID 0x10). */
	unsigned int sb16asp_ram_contents_index = 0;
	unsigned char sb16asp_ram_contents[2048];
	pic_tickindex_t next_check_record_settings = 0;
	unsigned char pc98_mixctl_reg = 0x14;

	void gen_input_reset(void);
	int gen_hiss(unsigned int mask);
	int gen_noise(unsigned int mask);
	unsigned char &ESSreg(uint8_t reg);
	double gen_1khz_tone(const bool advance);
	void gen_input(Bitu dmabytes,unsigned char *buf);
	void gen_input_hiss(Bitu dmabytes,unsigned char *buf);
	void gen_input_silence(Bitu dmabytes,unsigned char *buf);
	void gen_input_1khz_tone(Bitu dmabytes,unsigned char *buf);
	void DSP_DoDMATransfer(DMA_MODES new_mode,Bitu freq,bool stereo,bool dontInitLeft=false);
	void DSP_PrepareDMA_New(DMA_MODES new_mode,Bitu length,bool autoinit,bool stereo);
	void DSP_PrepareDMA_Old(DMA_MODES mode,bool autoinit,bool sign,bool hispeed);
	unsigned int DSP_RateLimitedFinalSB16Freq_New(unsigned int freq);
	void sb16asp_write_current_RAM_byte(const uint8_t r);
	uint8_t sb16asp_read_current_RAM_byte(void);
	void ESS_DoWrite(uint8_t reg,uint8_t data);
	void sb_update_recording_source_settings();
	void updateSoundBlasterFilter(Bitu rate);
	void DSP_ChangeMode(DSP_MODES new_mode);
	uint8_t DSP_RateLimitedFinalTC_Old();
	unsigned int ESS_DMATransferCount();
	void DSP_ChangeStereo(bool stereo);
	void DSP_ChangeRate(Bitu new_freq);
	void ESSUpdateFilterFromSB(void);
	void CTMIXER_UpdateVolumes(void);
	void GenerateDMASound(Bitu size);
	void sb16asp_next_RAM_byte(void);
	void CTMIXER_Write(uint8_t val);
	uint8_t ESS_DoRead(uint8_t reg);
	void SB_RaiseIRQ(SB_IRQS type);
	float calc_vol(uint8_t amount);
	void DSP_AddData(uint8_t val);
	void DSP_DoReset(uint8_t val);
	void DSP_DoWrite(uint8_t val);
	void DSP_SetSpeaker(bool how);
	bool DSP_busy_cycle_active();
	uint8_t DSP_ReadData(void);
	uint8_t CTMIXER_Read(void);
	void ESS_CheckDMAEnable();
	void ESS_UpdateDMATotal();
	void DSP_DoCommand(void);
	void SB_OnEndOfDMA(void);
	void CTMIXER_Reset(void);
	void DSP_FlushData(void);
	std::string GetSBtype();
	void CheckDMAEnd(void);
	bool DSP_busy_cycle();
	void DSP_Reset(void);
	void ESS_StartDMA();
	void ESS_StopDMA();

	Bitu read_sb(Bitu port,Bitu /*iolen*/);
	void write_sb(Bitu port,Bitu val,Bitu /*iolen*/);
};

static const char *sb_section_names[MAX_CARDS] = {
	"sblaster",
	"sblaster2"
};

static const char *sbMixerChanNames[MAX_CARDS] = {
	"SB",
	"SB2"
};

const char *sbGetSectionName(const size_t ci) {
	if (ci < MAX_CARDS)
		return sb_section_names[ci];
	else
		return NULL;
}

static Section_prop* sbGetSection(const size_t ci) {
	assert(ci < MAX_CARDS);
	return static_cast<Section_prop *>(control->GetSection(sb_section_names[ci]));
}

unsigned char &SB_INFO::ESSreg(uint8_t reg) {
	assert(reg >= 0xA0 && reg <= 0xBF);
	return ESSregs[reg-0xA0];
}

void SB_INFO::gen_input_reset(void) {
	gen_input_ofs = 0;
}

double SB_INFO::gen_1khz_tone(const bool advance) {
	/* sin() is pretty fast on today's hardware, no lookup table necessary */
	if (advance) gen_tone_angle++;
	return sin((gen_tone_angle * M_PI * 1000.0) / dma_dac_srcrate);
}

int SB_INFO::gen_hiss(unsigned int mask) {
	if (gen_hiss_rand[0] == 0) gen_hiss_rand[0] = (unsigned int)rand();
	if (gen_hiss_rand[1] == 0) gen_hiss_rand[1] = (unsigned int)rand();
	/* ref: [https://stackoverflow.com/questions/167735/fast-pseudo-random-number-generator-for-procedural-content#167764] */
	gen_hiss_rand[0] = 36969*(gen_hiss_rand[0] & 0xFFFF) + (gen_hiss_rand[1] >> 16);
	gen_hiss_rand[1] = 18000*(gen_hiss_rand[1] & 0xFFFF) + (gen_hiss_rand[0] >> 16);
	const unsigned int v = (gen_hiss_rand[1] << 16) + (gen_hiss_rand[0] & 0xFFFF);
	const int r = (v - gen_last_hiss) & mask; /* we want a hiss not white noise */
	gen_last_hiss = v;
	return r;
}

int SB_INFO::gen_noise(unsigned int mask) {
	if (gen_hiss_rand[0] == 0) gen_hiss_rand[0] = (unsigned int)rand();
	if (gen_hiss_rand[1] == 0) gen_hiss_rand[1] = (unsigned int)rand();
	/* ref: [https://stackoverflow.com/questions/167735/fast-pseudo-random-number-generator-for-procedural-content#167764] */
	gen_hiss_rand[0] = 36969*(gen_hiss_rand[0] & 0xFFFF) + (gen_hiss_rand[1] >> 16);
	gen_hiss_rand[1] = 18000*(gen_hiss_rand[1] & 0xFFFF) + (gen_hiss_rand[0] >> 16);
	const unsigned int v = (gen_hiss_rand[1] << 16) + (gen_hiss_rand[0] & 0xFFFF);
	const int r = v & mask; /* white noise in this case */
	gen_last_hiss = v;
	return r;
}

void SB_INFO::gen_input_1khz_tone(Bitu dmabytes,unsigned char *buf) {
	const unsigned int ofsmax = dma.stereo ? 2 : 1;
	unsigned int fill;

	if (dma.mode == DSP_DMA_16 || dma.mode == DSP_DMA_16_ALIASED) {
		uint16_t *buf16 = (uint16_t*)buf;

		if (dma.mode == DSP_DMA_16_ALIASED) dmabytes >>= 1u;
		fill = ((unsigned int)(gen_1khz_tone(false) * 0x4000/*half range*/) & 0xFFFF) ^ (dma.sign ? 0x0000 : 0x8000);
		while (dmabytes-- > 0) {
			*buf16++ = fill;
			if ((++gen_input_ofs) >= ofsmax) {
				fill = ((unsigned int)(gen_1khz_tone(true) * 0x4000) & 0xFFFF) ^ (dma.sign ? 0x0000 : 0x8000);
				gen_input_ofs = 0;
			}
		}
	}
	else { /* 8-bit */
		fill = ((((unsigned int)(gen_1khz_tone(false) * 0x4000/*half range*/) + gen_noise(0x7f) - 0x40) & 0xFFFF) ^ (dma.sign ? 0x0000 : 0x8000)) >> 8u;
		while (dmabytes-- > 0) {
			*buf++ = fill;
			if ((++gen_input_ofs) >= ofsmax) {
				fill = ((((unsigned int)(gen_1khz_tone(true) * 0x4000/*half range*/) + gen_noise(0x7f) - 0x40) & 0xFFFF) ^ (dma.sign ? 0x0000 : 0x8000)) >> 8u;
				gen_input_ofs = 0;
			}
		}
	}
}

void SB_INFO::gen_input_hiss(Bitu dmabytes,unsigned char *buf) {
	if (dma.mode == DSP_DMA_16 || dma.mode == DSP_DMA_16_ALIASED) {
		uint16_t *buf16 = (uint16_t*)buf;

		if (dma.mode == DSP_DMA_16_ALIASED) dmabytes >>= 1u;
		while (dmabytes-- > 0) *buf16++ = ((gen_hiss(0x3ff) - 0x200) & 0xFFFF) ^ (dma.sign ? 0x0000 : 0x8000);
	}
	else { /* 8-bit */
		while (dmabytes-- > 0) *buf++ = ((gen_hiss(0x3) - 0x2) & 0xFF) ^ (dma.sign ? 0x00 : 0x80);
	}
}

void SB_INFO::gen_input_silence(Bitu dmabytes,unsigned char *buf) {
	unsigned int fill;

	if (dma.mode == DSP_DMA_16 || dma.mode == DSP_DMA_16_ALIASED) {
		uint16_t *buf16 = (uint16_t*)buf;

		if (dma.mode == DSP_DMA_16_ALIASED) dmabytes >>= 1u;
		fill = dma.sign ? 0x0000 : 0x8000;
		while (dmabytes-- > 0) *buf16++ = fill;
	}
	else { /* 8-bit */
		fill = dma.sign ? 0x00 : 0x80;
		while (dmabytes-- > 0) *buf++ = fill;
	}
}

void SB_INFO::gen_input(Bitu dmabytes,unsigned char *buf) {
	switch (recording_source) {
		case REC_SILENCE:
			gen_input_silence(dmabytes,buf);
			break;
		case REC_HISS:
			gen_input_hiss(dmabytes,buf);
			break;
		case REC_1KHZ_TONE:
			gen_input_1khz_tone(dmabytes,buf);
			break;
		default:
			abort();
	}
}

void SB_INFO::DSP_SetSpeaker(bool how) {
	if (speaker==how) return;
	speaker=how;
	if (type==SBT_16) return;
	if (ess_type!=ESS_NONE) return;
	chan->Enable(how);
	if (speaker) {
		PIC_RemoveEvents(DMA_Silent_Event);
		CheckDMAEnd();
	} else {
		;
	}
}

void SB_INFO::DSP_FlushData(void) {
	dsp.out.used=0;
	dsp.out.pos=0;
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
void SB_INFO::SB_RaiseIRQ(SB_IRQS type) {
	LOG(LOG_SB,LOG_NORMAL)("Raising IRQ");

	if (ess_playback_mode) {
		if (!(ESSreg(0xB1) & 0x40)) // if ESS playback, and IRQ disabled, do not fire
			return;
	}

	switch (type) {
		case SB_IRQ_8:
			if (irq.pending_8bit) {
				// LOG_MSG("SB: 8bit irq pending");
				return;
			}
			irq.pending_8bit=true;
			PIC_ActivateIRQ(hw.irq);
			break;
		case SB_IRQ_16:
			if (irq.pending_16bit) {
				// LOG_MSG("SB: 16bit irq pending");
				return;
			}
			irq.pending_16bit=true;
			PIC_ActivateIRQ(hw.irq);
			break;
		default:
			break;
	}
}

void SB_INFO::sb_update_recording_source_settings() {
	Section_prop* section = sbGetSection(card_index);

	listen_to_recording_source=section->Get_bool("listen to recording source");

	{
		const char *s = section->Get_string("recording source");

		if (!strcmp(s,"silence"))
			recording_source = REC_SILENCE;
		else if (!strcmp(s,"hiss"))
			recording_source = REC_HISS;
		else if (!strcmp(s,"1khz tone"))
			recording_source = REC_1KHZ_TONE;
		else
			recording_source = REC_SILENCE;
	}
}

void SB_INFO::SB_OnEndOfDMA(void) {
	bool was_irq=false;

	PIC_RemoveEvents(END_DMA_Event);
	if (ess_type == ESS_NONE && reveal_sc_type == RSC_NONE && dma.mode >= DSP_DMA_16) {
		was_irq = irq.pending_16bit;
		SB_RaiseIRQ(SB_IRQ_16);
	}
	else {
		was_irq = irq.pending_8bit;
		SB_RaiseIRQ(SB_IRQ_8);
	}

	if (!dma.autoinit) {
		dsp.highspeed = false;
		LOG(LOG_SB,LOG_NORMAL)("Single cycle transfer ended");
		mode=MODE_NONE;
		dma.mode=DSP_DMA_NONE;

		if (ess_playback_mode) {
			LOG(LOG_SB,LOG_NORMAL)("ESS DMA stop");
			ESSreg(0xB8) &= ~0x01; // automatically stop DMA (right?)
			if (dma.chan) dma.chan->Clear_Request();
		}
	} else {
		dma.left=dma.total;
		if (!dma.left) {
			LOG(LOG_SB,LOG_NORMAL)("Auto-init transfer with 0 size");
			dsp.highspeed = false;
			mode=MODE_NONE;
		}
		else if (dsp.require_irq_ack && was_irq) {
			/* Sound Blaster 16 behavior: If you do not acknowledge the IRQ, and the card goes to signal another IRQ, the DSP halts playback.
			 * This is different from previous cards (and clone hardware) that continue playing whether or not you acknowledge the IRQ. */
			LOG(LOG_SB,LOG_WARN)("DMA ended when previous IRQ had not yet been acked");
			mode=MODE_DMA_REQUIRE_IRQ_ACK;
		}
	}
}

void SB_INFO::GenerateDMASound(Bitu size) {
	Bitu read=0;Bitu done=0;Bitu i=0;

	// don't read if the DMA channel is masked
	if (dma.chan->masked) return;

	if (dma_dac_mode) return;

	last_dma_callback = PIC_FullIndex();

	if(dma.autoinit) {
		if (dma.left <= size) size = dma.left;
	} else {
		if (dma.left <= dma.min)
			size = dma.left;
	}

	if (size > DMA_BUFSIZE) {
		/* Maybe it's time to consider rendering intervals based on what the mixer wants rather than odd 1ms DMA packet calculations... */
		LOG(LOG_SB,LOG_WARN)("Whoah! GenerateDMASound asked to render too much audio (%u > %u). Read could have overrun the DMA buffer!",(unsigned int)size,DMA_BUFSIZE);
		size = DMA_BUFSIZE;
	}

	if (dma.recording) {
		/* How much can we do? assume not masked because we checked that at the start of this function.
		 * Then generate that much input data. After writing via DMA, mute the audio before it goes to
		 * the mixer.
		 *
		 * TODO: It would be kind of fun to tie the generated audio parameters to mixer controls such
		 *       as allowing the user to control how loud the 1KHz sine wave is with the line in volume
		 *       control, and perhaps we should allow the generated audio to go out to the mixer output
		 *       if the line in is unmuted, subject to the audio volume controls. Or perhaps this code
		 *       should just make another mixer channel for recording sources. */
		read=dma.chan->currcnt + 1; /* DMA channel current count remain */
		if (read > size) read = size;
		if (dma.mode == DSP_DMA_16 || dma.mode == DSP_DMA_16_ALIASED)
			gen_input(read,(unsigned char*)(&dma.buf.b16[dma.remain_size]));
		else
			gen_input(read,&dma.buf.b8[dma.remain_size]);

		switch (dma.mode) {
			case DSP_DMA_8:
				if (dma.stereo) {
					read=dma.chan->Write(read,&dma.buf.b8[dma.remain_size]);
					if (read > 0 && !listen_to_recording_source) gen_input_silence(read,&dma.buf.b8[dma.remain_size]); /* mute before going out to mixer */
					Bitu total=read+dma.remain_size;
					if (!dma.sign)  chan->AddSamples_s8(total>>1,dma.buf.b8);
					else chan->AddSamples_s8s(total>>1,(int8_t*)dma.buf.b8);
					if (total&1) {
						dma.remain_size=1;
						dma.buf.b8[0]=dma.buf.b8[total-1];
					} else dma.remain_size=0;
				} else {
					read=dma.chan->Write(read,dma.buf.b8);
					if (read > 0 && !listen_to_recording_source) gen_input_silence(read,dma.buf.b8); /* mute before going out to mixer */
					if (!dma.sign) chan->AddSamples_m8(read,dma.buf.b8);
					else chan->AddSamples_m8s(read,(int8_t *)dma.buf.b8);
				}
				break;
			case DSP_DMA_16:
			case DSP_DMA_16_ALIASED:
				if (dma.stereo) {
					/* In DSP_DMA_16_ALIASED mode temporarily divide by 2 to get number of 16-bit
					   samples, because 8-bit DMA Read returns byte size, while in DSP_DMA_16 mode
					   16-bit DMA Read returns word size */
					read=dma.chan->Write(read,(uint8_t *)&dma.buf.b16[dma.remain_size])
						>> (dma.mode==DSP_DMA_16_ALIASED ? 1:0);
					if (read > 0 && !listen_to_recording_source) gen_input_silence(read,(unsigned char*)(&dma.buf.b16[dma.remain_size])); /* mute before going out to mixer */
					Bitu total=read+dma.remain_size;
#if defined(WORDS_BIGENDIAN)
					if (dma.sign) chan->AddSamples_s16_nonnative(total>>1,dma.buf.b16);
					else chan->AddSamples_s16u_nonnative(total>>1,(uint16_t *)dma.buf.b16);
#else
					if (dma.sign) chan->AddSamples_s16(total>>1,dma.buf.b16);
					else chan->AddSamples_s16u(total>>1,(uint16_t *)dma.buf.b16);
#endif
					if (total&1) {
						dma.remain_size=1;
						dma.buf.b16[0]=dma.buf.b16[total-1];
					} else dma.remain_size=0;
				} else {
					read=dma.chan->Write(read,(uint8_t *)dma.buf.b16)
						>> (dma.mode==DSP_DMA_16_ALIASED ? 1:0);
					if (read > 0 && !listen_to_recording_source) gen_input_silence(read,(unsigned char*)dma.buf.b16); /* mute before going out to mixer */
#if defined(WORDS_BIGENDIAN)
					if (dma.sign) chan->AddSamples_m16_nonnative(read,dma.buf.b16);
					else chan->AddSamples_m16u_nonnative(read,(uint16_t *)dma.buf.b16);
#else
					if (dma.sign) chan->AddSamples_m16(read,dma.buf.b16);
					else chan->AddSamples_m16u(read,(uint16_t *)dma.buf.b16);
#endif
				}
				//restore buffer length value to byte size in aliased mode
				if (dma.mode==DSP_DMA_16_ALIASED) read=read<<1;
				break;
			default:
				LOG_MSG("Unhandled dma record mode %d",dma.mode);
				mode=MODE_NONE;
				return;
		}
	}
	else {
		switch (dma.mode) {
			case DSP_DMA_2:
				read=dma.chan->Read(size,dma.buf.b8);
				if (read && adpcm.haveref) {
					adpcm.haveref=false;
					adpcm.reference=dma.buf.b8[0];
					adpcm.stepsize=MIN_ADAPTIVE_STEP_SIZE;
					i++;
				}
				for (;i<read;i++) {
					MixTemp[done++]=decode_ADPCM_2_sample((dma.buf.b8[i] >> 6) & 0x3,adpcm.reference,adpcm.stepsize);
					MixTemp[done++]=decode_ADPCM_2_sample((dma.buf.b8[i] >> 4) & 0x3,adpcm.reference,adpcm.stepsize);
					MixTemp[done++]=decode_ADPCM_2_sample((dma.buf.b8[i] >> 2) & 0x3,adpcm.reference,adpcm.stepsize);
					MixTemp[done++]=decode_ADPCM_2_sample((dma.buf.b8[i] >> 0) & 0x3,adpcm.reference,adpcm.stepsize);
				}
				chan->AddSamples_m8(done,MixTemp);
				break;
			case DSP_DMA_3:
				read=dma.chan->Read(size,dma.buf.b8);
				if (read && adpcm.haveref) {
					adpcm.haveref=false;
					adpcm.reference=dma.buf.b8[0];
					adpcm.stepsize=MIN_ADAPTIVE_STEP_SIZE;
					i++;
				}
				for (;i<read;i++) {
					MixTemp[done++]=decode_ADPCM_3_sample((dma.buf.b8[i] >> 5) & 0x7,adpcm.reference,adpcm.stepsize);
					MixTemp[done++]=decode_ADPCM_3_sample((dma.buf.b8[i] >> 2) & 0x7,adpcm.reference,adpcm.stepsize);
					MixTemp[done++]=decode_ADPCM_3_sample((dma.buf.b8[i] & 0x3) << 1,adpcm.reference,adpcm.stepsize);
				}
				chan->AddSamples_m8(done,MixTemp);
				break;
			case DSP_DMA_4:
				read=dma.chan->Read(size,dma.buf.b8);
				if (read && adpcm.haveref) {
					adpcm.haveref=false;
					adpcm.reference=dma.buf.b8[0];
					adpcm.stepsize=MIN_ADAPTIVE_STEP_SIZE;
					i++;
				}
				for (;i<read;i++) {
					MixTemp[done++]=decode_ADPCM_4_sample(dma.buf.b8[i] >> 4,adpcm.reference,adpcm.stepsize);
					MixTemp[done++]=decode_ADPCM_4_sample(dma.buf.b8[i]& 0xf,adpcm.reference,adpcm.stepsize);
				}
				chan->AddSamples_m8(done,MixTemp);
				break;
			case DSP_DMA_8:
				if (dma.stereo) {
					read=dma.chan->Read(size,&dma.buf.b8[dma.remain_size]);
					Bitu total=read+dma.remain_size;
					if (!dma.sign)  chan->AddSamples_s8(total>>1,dma.buf.b8);
					else chan->AddSamples_s8s(total>>1,(int8_t*)dma.buf.b8);
					if (total&1) {
						dma.remain_size=1;
						dma.buf.b8[0]=dma.buf.b8[total-1];
					} else dma.remain_size=0;
				} else {
					read=dma.chan->Read(size,dma.buf.b8);
					if (!dma.sign) chan->AddSamples_m8(read,dma.buf.b8);
					else chan->AddSamples_m8s(read,(int8_t *)dma.buf.b8);
				}
				break;
			case DSP_DMA_16:
			case DSP_DMA_16_ALIASED:
				if (dma.stereo) {
					/* In DSP_DMA_16_ALIASED mode temporarily divide by 2 to get number of 16-bit
					   samples, because 8-bit DMA Read returns byte size, while in DSP_DMA_16 mode
					   16-bit DMA Read returns word size */
					read=dma.chan->Read(size,(uint8_t *)&dma.buf.b16[dma.remain_size])
						>> (dma.mode==DSP_DMA_16_ALIASED ? 1:0);
					Bitu total=read+dma.remain_size;
#if defined(WORDS_BIGENDIAN)
					if (dma.sign) chan->AddSamples_s16_nonnative(total>>1,dma.buf.b16);
					else chan->AddSamples_s16u_nonnative(total>>1,(uint16_t *)dma.buf.b16);
#else
					if (dma.sign) chan->AddSamples_s16(total>>1,dma.buf.b16);
					else chan->AddSamples_s16u(total>>1,(uint16_t *)dma.buf.b16);
#endif
					if (total&1) {
						dma.remain_size=1;
						dma.buf.b16[0]=dma.buf.b16[total-1];
					} else dma.remain_size=0;
				} else {
					read=dma.chan->Read(size,(uint8_t *)dma.buf.b16)
						>> (dma.mode==DSP_DMA_16_ALIASED ? 1:0);
#if defined(WORDS_BIGENDIAN)
					if (dma.sign) chan->AddSamples_m16_nonnative(read,dma.buf.b16);
					else chan->AddSamples_m16u_nonnative(read,(uint16_t *)dma.buf.b16);
#else
					if (dma.sign) chan->AddSamples_m16(read,dma.buf.b16);
					else chan->AddSamples_m16u(read,(uint16_t *)dma.buf.b16);
#endif
				}
				//restore buffer length value to byte size in aliased mode
				if (dma.mode==DSP_DMA_16_ALIASED) read=read<<1;
				break;
			default:
				LOG_MSG("Unhandled dma playback mode %d",dma.mode);
				mode=MODE_NONE;
				return;
		}
	}
	dma.left-=read;
	if (!dma.left) SB_OnEndOfDMA();
}

void SB_INFO::CheckDMAEnd(void) {
	if (!dma.left) return;
	if (!speaker && type!=SBT_16 && ess_type==ESS_NONE) {
		Bitu bigger=(dma.left > dma.min) ? dma.min : dma.left;
		float delay=(bigger*1000.0f)/dma.rate;
		PIC_AddEvent(DMA_Silent_Event,delay,bigger | (card_index << CARD_INDEX_BIT));
		LOG(LOG_SB,LOG_NORMAL)("Silent DMA Transfer scheduling IRQ in %.3f milliseconds",delay);
	} else if (dma.left<dma.min) {
		float delay=(dma.left*1000.0f)/dma.rate;
		LOG(LOG_SB,LOG_NORMAL)("Short transfer scheduling IRQ in %.3f milliseconds",delay);
		PIC_AddEvent(END_DMA_Event,delay,dma.left | (card_index << CARD_INDEX_BIT));
	}
}

void SB_INFO::DSP_ChangeMode(DSP_MODES new_mode) {
	if (mode == new_mode) return;
	else chan->FillUp();
	mode=new_mode;
}

void SB_INFO::DSP_DoDMATransfer(DMA_MODES new_mode,Bitu freq,bool stereo,bool dontInitLeft) {
	char const * type;

	mode=MODE_DMA_MASKED;

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
	if (dma.chan != NULL &&
			dma.chan->basecnt < ((new_mode==DSP_DMA_16_ALIASED?2:1)*((stereo || mixer.sbpro_stereo)?2:1))/*size of one sample in DMA counts*/)
		single_sample_dma = 1;
	else
		single_sample_dma = 0;

	dma_dac_srcrate=freq;
	if (dsp.force_goldplay || (goldplay && freq > 0 && single_sample_dma))
		dma_dac_mode=1;
	else
		dma_dac_mode=0;

	/* explanation: the purpose of Goldplay stereo mode is to compensate for the fact
	 * that demos using this method of playback know to set the SB Pro stereo bit, BUT,
	 * apparently did not know that they needed to double the sample rate when
	 * computing the DSP time constant. Such demos sound "OK" on Sound Blaster Pro but
	 * have audible aliasing artifacts because of this. The Goldplay Stereo hack
	 * detects this condition and doubles the sample rate to better capture what the
	 * demo is *trying* to do. NTS: freq is the raw sample rate given by the
	 * program, before it is divided by two for stereo.
	 *
	 * Of course, some demos like Crystal Dream take the approach of just setting the
	 * sample rate to the max supported by the card and then letting its timer interrupt
	 * define the sample rate. So of course anything below 44.1KHz sounds awful. */
	if (dma_dac_mode && goldplay_stereo && (stereo || mixer.sbpro_stereo) && single_sample_dma)
		dma_dac_srcrate = freq;

	chan->FillUp();

	if (!dontInitLeft)
		dma.left=dma.total;

	dma.mode=dma.mode_assigned=new_mode;
	dma.stereo=stereo;
	irq.pending_8bit=false;
	irq.pending_16bit=false;
	switch (new_mode) {
		case DSP_DMA_2:
			type="2-bits ADPCM";
			dma.mul=(1 << SB_SH)/4;
			break;
		case DSP_DMA_3:
			type="3-bits ADPCM";
			dma.mul=(1 << SB_SH)/3;
			break;
		case DSP_DMA_4:
			type="4-bits ADPCM";
			dma.mul=(1 << SB_SH)/2;
			break;
		case DSP_DMA_8:
			type="8-bits PCM";
			dma.mul=(1 << SB_SH);
			break;
		case DSP_DMA_16_ALIASED:
			type="16-bits(aliased) PCM";
			dma.mul=(1 << SB_SH)*2;
			break;
		case DSP_DMA_16:
			type="16-bits PCM";
			dma.mul=(1 << SB_SH);
			break;
		default:
			LOG(LOG_SB,LOG_ERROR)("DSP:Illegal transfer mode %d",new_mode);
			return;
	}
	if (dma.stereo) dma.mul*=2;
	dma.rate=(dma_dac_srcrate*dma.mul) >> SB_SH;
	dma.min=((Bitu)dma.rate*(Bitu)(min_dma_user >= 0 ? min_dma_user : /*default*/3))/1000u;
	if (dma_dac_mode && goldplay_stereo && (stereo || mixer.sbpro_stereo) && single_sample_dma) {
		//        LOG(LOG_SB,LOG_DEBUG)("Goldplay stereo hack. freq=%u rawfreq=%u dacrate=%u",(unsigned int)freq,(unsigned int)freq,(unsigned int)dma_dac_srcrate);
		chan->SetFreq(dma_dac_srcrate);
		updateSoundBlasterFilter(freq); /* BUT, you still filter like the actual sample rate */
	}
	else {
		chan->SetFreq(freq);
		updateSoundBlasterFilter(freq);
	}
	dma.mode=dma.mode_assigned=new_mode;
	PIC_RemoveEvents(DMA_DAC_Event);
	PIC_RemoveEvents(END_DMA_Event);

	if (dma_dac_mode)
		PIC_AddEvent(DMA_DAC_Event,1000.0 / dma_dac_srcrate,(card_index << CARD_INDEX_BIT));

	if (dma.chan != NULL) {
		dma.chan->Register_Callback(DSP_DMA_CallBack);
	}
	else {
		LOG(LOG_SB,LOG_WARN)("DMA transfer initiated with no channel assigned");
	}

#if (C_DEBUG)
	LOG(LOG_SB,LOG_NORMAL)("DMA Transfer:%s %s %s dsp %s dma %s freq %d rate %d dspsize %d dmasize %d gold %d",
		type,
		dma.recording ? "Recording" : "Playback",
		dma.stereo ? "Stereo" : "Mono",
		dma.autoinit ? "Auto-Init" : "Single-Cycle",
		dma.chan ? (dma.chan->autoinit ? "Auto-Init" : "Single-Cycle") : "n/a",
		(int)freq,(int)dma.rate,(int)dma.total,
		dma.chan ? (dma.chan->basecnt+1) : 0,
		(int)dma_dac_mode
		);
#else
	(void)type;
#endif
}

uint8_t SB_INFO::DSP_RateLimitedFinalTC_Old() {
	if (sample_rate_limits) { /* enforce speed limits documented by Creative */
		/* commands that invoke this call use the DSP time constant, so use the DSP
		 * time constant to constrain rate */
		unsigned int u_limit=212;/* 23KHz */

		/* NTS: We skip the SB16 commands because those are handled by another function */
		if ((dsp.cmd&0xFE) == 0x74 || dsp.cmd == 0x7D) { /* 4-bit ADPCM */
			u_limit = 172; /* 12KHz */
		}
		else if ((dsp.cmd&0xFE) == 0x76) { /* 2.6-bit ADPCM */
			if (type == SBT_2) u_limit = 172; /* 12KHz */
			else u_limit = 179; /* 13KHz */
		}
		else if ((dsp.cmd&0xFE) == 0x16) { /* 2-bit ADPCM */
			if (type == SBT_2) u_limit = 189; /* 15KHz */
			else u_limit = 165; /* 11KHz */
		}
		else if (type == SBT_16) /* Sound Blaster 16. Highspeed commands are treated like an alias to normal DSP commands */
			u_limit = 234/*45454Hz*/;
		else if (type == SBT_2) /* Sound Blaster 2.0 */
			u_limit = (dsp.highspeed ? 234/*45454Hz*/ : 210/*22.5KHz*/);
		else
			u_limit = (dsp.highspeed ? 234/*45454Hz*/ : 212/*22.5KHz*/);

		/* NTS: Don't forget: Sound Blaster Pro "stereo" is programmed with a time constant divided by
		 *      two times the sample rate, which is what we get back here. That's why here we don't need
		 *      to consider stereo vs mono. */
		if (timeconst > u_limit) return u_limit;
	}

	return timeconst;
}

unsigned int SB_INFO::DSP_RateLimitedFinalSB16Freq_New(unsigned int freq) {
	/* If sample rate was set by DSP command 0x41/0x42 */
	if (sample_rate_limits) { /* enforce speed limits documented by Creative... which are somewhat wrong. They are the same limits as high-speed playback modes on SB Pro and 2.0 */
		if (freq < 4900)
			freq = 5000; /* Apparent behavior is that SB16 commands only go down to 5KHz but that limit is imposed if slightly below 5KHz, see rate graphs on Hackipedia */
		if (freq > 45454)
			freq = 45454;
	}

	return freq;
}

void SB_INFO::DSP_AddData(uint8_t val) {
	if (dsp.out.used<DSP_BUFSIZE) {
		Bitu start=dsp.out.used+dsp.out.pos;
		if (start>=DSP_BUFSIZE) start-=DSP_BUFSIZE;
		dsp.out.data[start]=val;
		dsp.out.used++;
	} else {
		LOG(LOG_SB,LOG_ERROR)("DSP:Data Output buffer full");
	}
}

uint8_t SB_INFO::DSP_ReadData(void) {
	/* Static so it repeats the last value on successive reads (JANGLE DEMO) */
	if (dsp.out.used) {
		dsp.out.lastval=dsp.out.data[dsp.out.pos];
		dsp.out.pos++;
		if (dsp.out.pos>=DSP_BUFSIZE) dsp.out.pos-=DSP_BUFSIZE;
		dsp.out.used--;
	}
	return dsp.out.lastval;
}

void SB_INFO::DSP_PrepareDMA_Old(DMA_MODES mode,bool autoinit,bool sign,bool hispeed) {
	/* this must be processed BEFORE forcing auto-init because the non-autoinit case provides the DSP transfer block size (fix for "Jump" by Public NMI) */
	if (!autoinit) dma.total=1u+(unsigned int)dsp.in.data[0]+(unsigned int)(dsp.in.data[1] << 8u);

	if (dma.force_autoinit)
		autoinit = true;

	dma.autoinit=autoinit;
	dsp.highspeed=hispeed;
	dma.sign=sign;

	/* BUGFIX: There is code out there that uses SB16 sample rate commands mixed with SB/SBPro
	 *         playback commands. In order to handle these cases properly we need to use the
	 *         SB16 sample rate if that is what was given to us, else the sample rate will
	 *         come out wrong.
	 *
	 *         Test cases:
	 *
	 *         #1: Silpheed (vogons user newrisingsun amatorial patch)
	 */
	if (freq_derived_from_tc) {
		// sample rate was set by SB/SBpro command 0x40 Set Time Constant (very common case)
		/* BUGFIX: Instead of direct rate-limiting the DSP time constant, keep the original
		 *         value written intact and rate-limit a copy. Bugfix for Optic Nerve and
		 *         sbtype=sbpro2. On initialization the demo first sends DSP command 0x14
		 *         with a 2-byte playback interval, then sends command 0x91 to begin
		 *         playback. Rate-limiting the copy means the 45454Hz time constant written
		 *         by the demo stays intact despite being limited to 22050Hz during the first
		 *         DSP block (command 0x14). */
		const uint8_t final_tc = DSP_RateLimitedFinalTC_Old();
		freq = (256000000ul / (65536ul - ((unsigned long)final_tc << 8ul)));
	}
	else {
		LOG(LOG_SB,LOG_DEBUG)("Guest is using non-SB16 playback commands after using SB16 commands to set sample rate");
		freq = DSP_RateLimitedFinalSB16Freq_New(freq);
	}

	dma_dac_mode=0;
	ess_playback_mode = false;
	dma.chan=GetDMAChannel(hw.dma8);
	dma.chan->userData = card_index;
	DSP_DoDMATransfer(mode,freq / (mixer.stereo ? 2 : 1),mixer.stereo);
}

void SB_INFO::DSP_PrepareDMA_New(DMA_MODES new_mode,Bitu length,bool autoinit,bool stereo) {
	if (dma.force_autoinit)
		autoinit = true;

	/* apparently SB16 hardware allows 0xBx-0xCx 4.xx DSP commands to interrupt
	 * a previous SB16 playback command, DSP "nag" style. The difference is that
	 * if you do that you risk exploiting DMA and timing glitches in the chip that
	 * can cause funny things to happen, like causing 16-bit PCM to stop, or causing
	 * 8-bit stereo PCM to swap left/right channels because the host is using auto-init
	 * DMA and you interrupted the DSP chip when it fetched the L channel before it
	 * had a chance to latch it and begin loading the R channel. */
	if (mode == MODE_DMA) {
		if (!autoinit) dma.total=length;
		dma.left=dma.total;
		dma.autoinit=autoinit;
		return;
	}

	dsp.highspeed = false;
	freq = DSP_RateLimitedFinalSB16Freq_New(freq);
	timeconst = (65536 - (256000000 / freq)) >> 8;
	freq_derived_from_tc = false;

	const Bitu saved_freq=freq;
	//equal length if data format and dma channel are both 16-bit or 8-bit
	dma_dac_mode=0;
	dma.total=length;
	dma.autoinit=autoinit;
	ess_playback_mode = false;
	if (new_mode==DSP_DMA_16) {
		if (hw.dma16 == 0xff || hw.dma16 == hw.dma8) { /* 16-bit DMA not assigned or same as 8-bit channel */
			dma.chan=GetDMAChannel(hw.dma8);
			dma.chan->userData = card_index;
			new_mode=DSP_DMA_16_ALIASED;
			//UNDOCUMENTED:
			//In aliased mode sample length is written to DSP as number of
			//16-bit samples so we need double 8-bit DMA buffer length
			dma.total<<=1;
		}
		else if (hw.dma16 >= 4) { /* 16-bit DMA assigned to 16-bit DMA channel */
			dma.chan=GetDMAChannel(hw.dma16);
			dma.chan->userData = card_index;
		}
		else {
			/* Nope. According to one ViBRA PnP card I have on hand, asking the
			 * card to do 16-bit DMA over 8-bit DMA only works if they are the
			 * same channel, otherwise, the card doesn't seem to carry out any
			 * DMA fetching. */
			dma.chan=NULL;
			return;
		}
	} else {
		dma.chan=GetDMAChannel(hw.dma8);
		dma.chan->userData = card_index;
	}

	DSP_DoDMATransfer(new_mode,saved_freq,stereo);
}

void SB_INFO::DSP_Reset(void) {
	LOG(LOG_SB,LOG_NORMAL)("DSP:Reset");
	PIC_DeActivateIRQ(hw.irq);

	DSP_ChangeMode(MODE_NONE);
	DSP_FlushData();
	dsp.cmd=DSP_NO_COMMAND;
	dsp.cmd_len=0;
	dsp.in.pos=0;
	dsp.out.pos=0;
	dsp.write_busy=0;
	ess_extended_mode = false;
	ess_playback_mode = false;
	single_sample_dma = 0;
	dma_dac_mode = 0;
	directdac_warn_speaker_off = true;
	PIC_RemoveEvents(DSP_FinishReset);
	PIC_RemoveEvents(DSP_BusyComplete);

	dma.left=0;
	dma.total=0;
	dma.stereo=false;
	dma.recording=false;
	dma.sign=false;
	dma.autoinit=false;
	dma.mode=dma.mode_assigned=DSP_DMA_NONE;
	dma.remain_size=0;
	if (dma.chan) dma.chan->Clear_Request();

	/* reset must clear the DSP command E2h and E5h DMA handlers */
	{
		DmaChannel *chan = GetDMAChannel(hw.dma8);
		if (chan) {
			if (chan->callback == DSP_SC400_E6_DMA_CallBack || chan->callback == DSP_E2_DMA_CallBack) {
				LOG(LOG_SB,LOG_DEBUG)("DSP reset interrupting E2/E5h DMA commands");
				chan->Register_Callback(nullptr);
			}
		}
	}

	gen_input_reset();

	dsp.midi_rwpoll_mode = false;
	dsp.midi_read_interrupt = false;
	dsp.midi_read_with_timestamps = false;

	freq=22050;
	freq_derived_from_tc=true;
	time_constant=45;
	dac.last=0;
	e2.valadd=0xaa;
	e2.valxor=0x96;
	dsp.highspeed=0;
	irq.pending_8bit=false;
	irq.pending_16bit=false;
	chan->SetFreq(22050);
	updateSoundBlasterFilter(22050);
	//  DSP_SetSpeaker(false);
	PIC_RemoveEvents(END_DMA_Event);
	PIC_RemoveEvents(DMA_DAC_Event);
}

void SB_INFO::DSP_DoReset(uint8_t val) {
	if (((val&1)!=0) && (dsp.state!=DSP_S_RESET)) {
		//TODO Get out of highspeed mode
		DSP_Reset();
		dsp.state=DSP_S_RESET;
	} else if (((val&1)==0) && (dsp.state==DSP_S_RESET)) {   // reset off
		dsp.state=DSP_S_RESET_WAIT;
		PIC_RemoveEvents(DSP_FinishReset);
		PIC_AddEvent(DSP_FinishReset,20.0f/1000.0f,(card_index << CARD_INDEX_BIT));  // 20 microseconds
	}
	dsp.write_busy = 0;
}

void SB_INFO::DSP_ChangeRate(Bitu new_freq) {
	if (freq!=new_freq && dma.mode!=DSP_DMA_NONE) {
		chan->FillUp();
		chan->SetFreq(new_freq / (mixer.stereo ? 2 : 1));
		dma.rate=(new_freq*dma.mul) >> SB_SH;
		dma.min=(dma.rate*3)/1000;
	}
	freq = new_freq;
}

unsigned int SB_INFO::ESS_DMATransferCount() {
	unsigned int r;

	r = (unsigned int)ESSreg(0xA5) << 8U;
	r |= (unsigned int)ESSreg(0xA4);

	/* the 16-bit counter is a "two's complement" of the DMA count because it counts UP to 0 and triggers IRQ on overflow */
	return 0x10000U-r;
}

void SB_INFO::ESS_StartDMA() {
	LOG(LOG_SB,LOG_DEBUG)("ESS DMA start");
	dma_dac_mode = 0;
	dma.chan = GetDMAChannel(hw.dma8);
	dma.chan->userData = card_index;
	dma.recording = (ESSreg(0xB8) & 8/*ADC mode*/) > 0;
	if (dma.chan) dma.chan->Raise_Request();
	// FIXME: Which bit(s) are responsible for signalling stereo?
	//        Is it bit 3 of the Analog Control?
	//        Is it bit 3/6 of the Audio Control 1?
	//        Is it both?
	// NTS: ESS chipsets always use the 8-bit DMA channel, even for 16-bit PCM.
	// NTS: ESS chipsets also do not cap the sample rate, though if you drive them
	//      too fast the ISA bus will effectively cap the sample rate at some
	//      rate above 48KHz to 60KHz anyway.
	DSP_DoDMATransfer(
		(ESSreg(0xB7/*Audio Control 1*/)&4)?DSP_DMA_16_ALIASED:DSP_DMA_8,
		freq,(ESSreg(0xA8/*Analog control*/)&3)==1?1:0/*stereo*/,true/*don't change dma.left*/);
	mode = MODE_DMA;
	ess_playback_mode = true;
}

void SB_INFO::ESS_StopDMA() {
	// DMA stop
	DSP_ChangeMode(MODE_NONE);
	if (dma.chan) dma.chan->Clear_Request();
	PIC_RemoveEvents(END_DMA_Event);
	PIC_RemoveEvents(DMA_DAC_Event);
}

void SB_INFO::ESS_UpdateDMATotal() {
	dma.total = ESS_DMATransferCount();
}

void SB_INFO::ESS_CheckDMAEnable() {
	bool dma_en = (ESSreg(0xB8) & 1)?true:false;

	// if the DRQ is disabled, do not start
	if (!(ESSreg(0xB2) & 0x40))
		dma_en = false;

	if (ESSreg(0xB8) & 8) LOG(LOG_SB,LOG_WARN)("Guest recording audio using ESS commands");

	if (!!(ESSreg(0xB8) & 8/*ADC mode*/) != !!(ESSreg(0xB8) & 2/*DMA read*/)) LOG(LOG_SB,LOG_WARN)("ESS DMA direction vs ADC mismatch");

	if (dma_en) {
		if (mode != MODE_DMA) ESS_StartDMA();
	}
	else {
		if (mode == MODE_DMA) ESS_StopDMA();
	}
}

void SB_INFO::ESSUpdateFilterFromSB(void) {
	if (freq >= 22050)
		ESSreg(0xA1) = 256 - (795500UL / freq);
	else
		ESSreg(0xA1) = 128 - (397700UL / freq);

	const unsigned int new_freq = ((freq * 4) / (5 * 2)); /* 80% of 1/2 the sample rate */
	ESSreg(0xA2) = 256 - (7160000 / (new_freq * 82));
}

void SB_INFO::ESS_DoWrite(uint8_t reg,uint8_t data) {
	uint8_t chg;

	LOG(LOG_SB,LOG_DEBUG)("ESS register write reg=%02xh val=%02xh",reg,data);

	switch (reg) {
		case 0xA1: /* Extended Mode Sample Rate Generator */
			ESSreg(reg) = data;
			if (data & 0x80)
				freq = 795500UL / (256ul - data);
			else
				freq = 397700UL / (128ul - data);

			freq_derived_from_tc = false;
			if (mode == MODE_DMA) {
				ESS_StopDMA();
				ESS_StartDMA();
			}
			break;
		case 0xA2: /* Filter divider (effectively, a hardware lowpass filter under S/W control) */
			ESSreg(reg) = data;
			updateSoundBlasterFilter(freq);
			break;
		case 0xA4: /* DMA Transfer Count Reload (low) */
		case 0xA5: /* DMA Transfer Count Reload (high) */
			ESSreg(reg) = data;
			ESS_UpdateDMATotal();
			if (dma.left == 0) dma.left = dma.total;
			break;
		case 0xA8: /* Analog Control */
			/* bits 7:5   0                  Reserved. Always write 0
			 * bit  4     1                  Reserved. Always write 1
			 * bit  3     Record monitor     1=Enable record monitor
			 *            enable
			 * bit  2     0                  Reserved. Always write 0
			 * bits 1:0   Stereo/mono select 00=Reserved
			 *                               01=Stereo
			 *                               10=Mono
			 *                               11=Reserved */
			chg = ESSreg(reg) ^ data;
			ESSreg(reg) = data;
			if (chg & 0x3) {
				if (mode == MODE_DMA) {
					ESS_StopDMA();
					ESS_StartDMA();
				}
			}
			break;
		case 0xB1: /* Legacy Audio Interrupt Control */
		case 0xB2: /* DRQ Control */
			chg = ESSreg(reg) ^ data;
			ESSreg(reg) = (ESSreg(reg) & 0x0F) + (data & 0xF0); // lower 4 bits not writeable
			if (chg & 0x40) ESS_CheckDMAEnable();
			break;
		case 0xB5: /* DAC Direct Access Holding (low) */
		case 0xB6: /* DAC Direct Access Holding (high) */
			ESSreg(reg) = data;
			break;
		case 0xB7: /* Audio 1 Control 1 */
			/* bit  7     Enable FIFO to/from codec
			 * bit  6     Opposite from bit 3               Must be set opposite to bit 3
			 * bit  5     FIFO signed mode                  1=Data is signed twos-complement   0=Data is unsigned
			 * bit  4     Reserved                          Always write 1
			 * bit  3     FIFO stereo mode                  1=Data is stereo
			 * bit  2     FIFO 16-bit mode                  1=Data is 16-bit
			 * bit  1     Reserved                          Always write 0
			 * bit  0     Generate load signal */
			chg = ESSreg(reg) ^ data;
			ESSreg(reg) = data;
			dma.sign = (data&0x20)?1:0;
			if (chg & 0x04) ESS_UpdateDMATotal();
			if (chg & 0x0C) {
				if (mode == MODE_DMA) {
					ESS_StopDMA();
					ESS_StartDMA();
				}
			}
			break;
		case 0xB8: /* Audio 1 Control 2 */
			/* bits 7:4   reserved
			 * bit  3     CODEC mode         1=first DMA converter in ADC mode
			 *                               0=first DMA converter in DAC mode
			 * bit  2     DMA mode           1=auto-initialize mode
			 *                               0=normal DMA mode
			 * bit  1     DMA read enable    1=first DMA is read (for ADC)
			 *                               0=first DMA is write (for DAC)
			 * bit  0     DMA xfer enable    1=DMA is allowed to proceed */
			data &= 0xF;
			chg = ESSreg(reg) ^ data;
			ESSreg(reg) = data;

			/* FIXME: This is a guess */
			if (chg & 1) dma.left = dma.total;

			dma.autoinit = (data >> 2) & 1;
			if (chg & 0xB) {
				if (chg & 0xA) ESS_StopDMA(); /* changing capture/playback direction? stop DMA to reinit */
				ESS_CheckDMAEnable();
			}
			break;
		case 0xB9: /* Audio 1 Transfer Type */
		case 0xBA: /* Left Channel ADC Offset Adjust */
		case 0xBB: /* Right Channel ADC Offset Adjust */
			ESSreg(reg) = data;
			break;
	}
}

uint8_t SB_INFO::ESS_DoRead(uint8_t reg) {
	LOG(LOG_SB,LOG_DEBUG)("ESS register read reg=%02xh",reg);

	switch (reg) {
		default:
			return ESSreg(reg);
	}

	return 0xFF;
}

void SB_INFO::sb16asp_write_current_RAM_byte(const uint8_t r) {
	sb16asp_ram_contents[sb16asp_ram_contents_index] = r;
}

uint8_t SB_INFO::sb16asp_read_current_RAM_byte(void) {
	return sb16asp_ram_contents[sb16asp_ram_contents_index];
}

void SB_INFO::sb16asp_next_RAM_byte(void) {
	if ((++sb16asp_ram_contents_index) >= 2048)
		sb16asp_ram_contents_index = 0;
}

bool SB_INFO::DSP_busy_cycle_active() {
	/* NTS: Busy cycle happens on SB16 at all times, or on earlier cards, only when the DSP is
	 *      fetching/writing data via the ISA DMA channel. So a non-auto-init DSP block that's
	 *      just finished fetching ISA DMA and is playing from the FIFO doesn't count.
	 *
	 *      dma.left >= dma.min condition causes busy cycle to stop 3ms early (by default).
	 *      This helps realism.
	 *
	 *      This also helps Crystal Dream, which uses the busy cycle to detect when the Sound
	 *      Blaster is about to finish playing the DSP block and therefore needs the same 3ms
	 *      "dmamin" hack to reissue another playback command without any audible hiccups in
	 *      the audio. */
	return (mode == MODE_DMA && (dma.autoinit || dma.left >= dma.min)) || busy_cycle_always;
}

bool SB_INFO::DSP_busy_cycle() {
	double now;
	int t;

	if (!DSP_busy_cycle_active()) return false;
	if (busy_cycle_duty_percent <= 0 || busy_cycle_hz <= 0) return false;

	/* NTS: DOSBox's I/O emulation doesn't yet attempt to accurately match ISA bus speeds or
	 *      consider ISA bus cycles, but to emulate SB16 behavior we have to "time" it so
	 *      that 8 consecutive I/O reads eventually see a transition from busy to not busy
	 *      (or the other way around). So what this hack does is it uses accurate timing
	 *      to determine where in the cycle we are, but if this function is called repeatedly
	 *      through I/O access, we switch to incrementing a counter to ensure busy/not busy
	 *      transition happens in 8 I/O cycles.
	 *
	 *      Without this hack, the CPU cycles count becomes a major factor in how many I/O
	 *      reads are required for busy/not busy to happen. If you set cycles count high
	 *      enough, more than 8 is required, and the SNDSB test code will have issues with
	 *      direct DAC mode again.
	 *
	 *      This isn't 100% accurate, but it's the best DOSBox-X can do for now to mimic
	 *      SB16 DSP behavior. */

	now = PIC_FullIndex();
	if (now >= (busy_cycle_last_check+0.02/*ms*/))
		busy_cycle_io_hack = (int)(fmod((now / 1000) * busy_cycle_hz,1.0) * 16);

	busy_cycle_last_check = now;
	t = ((busy_cycle_io_hack % 16) * 100) / 16; /* HACK: DOSBox's I/O is not quite ISA bus speeds or related to it */
	if (t < busy_cycle_duty_percent) return true;
	return false;
}

void SB_INFO::DSP_DoWrite(uint8_t val) {
	if (dsp.write_busy || (dsp.highspeed && type != SBT_16 && ess_type == ESS_NONE && reveal_sc_type == RSC_NONE)) {
		LOG(LOG_SB,LOG_WARN)("DSP:Command write %2X ignored, DSP not ready. DOS game or OS is not polling status",val);
		return;
	}

	/* NTS: We allow the user to set busy wait time == 0 aka "instant gratification mode".
	 *      We also assume that if they do that, some DOS programs might be timing sensitive
	 *      enough to freak out when DSP commands and data are accepted immediately */
	{
		unsigned int delay = dsp.dsp_write_busy_time;

		if (dsp.instant_direct_dac) {
			delay = 0;
		}
		/* Part of enforcing sample rate limits is to make sure to emulate that the
		 * Direct DAC output command 0x10 is "busy" long enough to effectively rate
		 * limit output to 23KHz. */
		else if (sample_rate_limits) {
			unsigned int limit = 23000; /* documented max sample rate for SB16/SBPro and earlier */

			if (type == SBT_16 && vibra)
				limit = 23000; /* DSP maxes out at 46KHz not 44.1KHz on ViBRA cards */

			if (dsp.cmd == DSP_NO_COMMAND && val == 0x10/*DSP direct DAC, command*/)
				delay = (625000000UL / limit) - dsp.dsp_write_busy_time;
		}

		if (delay > 0) {
			dsp.write_busy = 1;
			PIC_RemoveEvents(DSP_BusyComplete);
			PIC_AddEvent(DSP_BusyComplete,(double)delay / 1000000,(card_index << CARD_INDEX_BIT));
		}

		//      LOG(LOG_SB,LOG_NORMAL)("DSP:Command %02x delay %u",val,delay);
	}

	if (dsp.midi_rwpoll_mode) {
		// DSP writes in this mode go to the MIDI port
		//      LOG(LOG_SB,LOG_DEBUG)("DSP MIDI read/write poll mode: sending 0x%02x",val);
		if (midi == true) MIDI_RawOutByte(val);
		return;
	}

	switch (dsp.cmd) {
		case DSP_NO_COMMAND:
			/* genuine SB Pro and lower: remap DSP command to emulate aliases. */
			if (dsp.command_aliases && type < SBT_16 && ess_type == ESS_NONE && reveal_sc_type == RSC_NONE) {
				/* 0x41...0x47 are aliases of 0x40.
				 * See also: [https://www.vogons.org/viewtopic.php?f=62&t=61098&start=280].
				 * This is required for ftp.scene.org/mirrors/hornet/demos/1994/y/yahxmas.zip which relies on the 0x41 alias of command 0x40
				 * to function (which means that it may happen to work on SB Pro but will fail on clones and will fail on SB16 cards). */
				if (val >= 0x41 && val <= 0x47) {
					LOG(LOG_SB,LOG_WARN)("DSP command %02x and SB Pro or lower, treating as alias of 40h. Either written for SB16 or using undocumented alias.",val);
					val = 0x40;
				}
			}

			dsp.cmd=val;
			if (type == SBT_16)
				dsp.cmd_len=DSP_cmd_len_sb16[val];
			else if (ess_type != ESS_NONE)
				dsp.cmd_len=DSP_cmd_len_ess[val];
			else if (reveal_sc_type != RSC_NONE)
				dsp.cmd_len=DSP_cmd_len_sc400[val];
			else
				dsp.cmd_len=DSP_cmd_len_sb[val];

			dsp.in.pos=0;
			if (!dsp.cmd_len) DSP_DoCommand();
			break;
		default:
			dsp.in.data[dsp.in.pos]=val;
			dsp.in.pos++;
			if (dsp.in.pos>=dsp.cmd_len) DSP_DoCommand();
	}
}

//The soundblaster manual says 2.0 Db steps but we'll go for a bit less
float SB_INFO::calc_vol(uint8_t amount) {
	const uint8_t count = 31 - amount;
	float db = static_cast<float>(count);
	if (type == SBT_PRO1 || type == SBT_PRO2) {
		if (count) {
			if (count < 16) db -= 1.0f;
			else if (count > 16) db += 1.0f;
			if (count == 24) db += 2.0f;
			if (count > 27) return 0.0f; //turn it off.
		}
	} else { //Give the rest, the SB16 scale, as we don't have data.
		db *= 2.0f;
		if (count > 20) db -= 1.0f;
	}
	return (float) pow (10.0f,-0.05f * db);
}

void SB_INFO::CTMIXER_UpdateVolumes(void) {
	if (!mixer.enabled) return;

	chan->FillUp();

	MixerChannel * chan;
	float m0 = calc_vol(mixer.master[0]);
	float m1 = calc_vol(mixer.master[1]);
	chan = MIXER_FindChannel(sbMixerChanNames[card_index]);
	if (chan) chan->SetVolume(m0 * calc_vol(mixer.dac[0]), m1 * calc_vol(mixer.dac[1]));
	chan = MIXER_FindChannel("FM");
	if (chan) chan->SetVolume(m0 * calc_vol(mixer.fm[0]) , m1 * calc_vol(mixer.fm[1]) );
	chan = MIXER_FindChannel("CDAUDIO");
	if (chan) chan->SetVolume(m0 * calc_vol(mixer.cda[0]), m1 * calc_vol(mixer.cda[1]));
}

void SB_INFO::CTMIXER_Reset(void) {
	mixer.filter_bypass=0; // Creative Documentation: filter_bypass bit is 0 by default
	mixer.fm[0]=
	mixer.fm[1]=
	mixer.cda[0]=
	mixer.cda[1]=
	mixer.dac[0]=
	mixer.dac[1]=31;
	mixer.master[0]=
	mixer.master[1]=31;
	CTMIXER_UpdateVolumes();
}

/* Demo notes for fixing:
 *
 *  - "Buttman"'s intro uses a timer and DSP command 0x10 to play the sound effects even in Pro mode.
 *    It doesn't use DMA + IRQ until the music starts.
 */

void SB_INFO::DSP_DoCommand(void) {
	if (ess_type != ESS_NONE && dsp.cmd >= 0xA0 && dsp.cmd <= 0xCF) {
		// ESS overlap with SB16 commands. Handle it here, not mucking up the switch statement.

		if (dsp.cmd < 0xC0) { // write ESS register   (cmd=register data[0]=value to write)
			if (ess_extended_mode)
				ESS_DoWrite(dsp.cmd,dsp.in.data[0]);
		}
		else if (dsp.cmd == 0xC0) { // read ESS register   (data[0]=register to read)
			DSP_FlushData();
			if (ess_extended_mode && dsp.in.data[0] >= 0xA0 && dsp.in.data[0] <= 0xBF)
				DSP_AddData(ESS_DoRead(dsp.in.data[0]));
		}
		else if (dsp.cmd == 0xC6 || dsp.cmd == 0xC7) { // set(0xC6) clear(0xC7) extended mode
			ess_extended_mode = (dsp.cmd == 0xC6);
		}
		else {
			LOG(LOG_SB,LOG_DEBUG)("ESS: Unknown command %02xh",dsp.cmd);
		}

		dsp.last_cmd=dsp.cmd;
		dsp.cmd=DSP_NO_COMMAND;
		dsp.cmd_len=0;
		dsp.in.pos=0;
		return;
	}

	if (type == SBT_16) {
		// FIXME: This is a guess! See also [https://github.com/joncampbell123/dosbox-x/issues/1044#issuecomment-480024957]
		sb16_8051_mem[0x20] = dsp.last_cmd; /* cur_cmd */
	}

	// TODO: There are more SD16 ASP commands we can implement, by name even, with microcode download,
	//       using as reference the Linux kernel driver code:
	//
	//       http://lxr.free-electrons.com/source/sound/isa/sb/sb16_csp.c

	//  LOG_MSG("DSP Command %X",dsp.cmd);
	switch (dsp.cmd) {
		case 0x04:
			if (type == SBT_16) {
				/* SB16 ASP set mode register */
				ASP_mode = dsp.in.data[0];

				// bit 7: if set, enables bit 3 and memory access.
				// bit 3: if set, and bit 7 is set, register 0x83 can be used to read/write ASP internal memory. if clear, register 0x83 contains chip version ID
				// bit 2: if set, memory index is reset to 0. doesn't matter if memory access or not.
				// bit 1: if set, writing register 0x83 increments memory index. doesn't matter if memory access or not.
				// bit 0: if set, reading register 0x83 increments memory index. doesn't matter if memory access or not.
				if (ASP_mode&4)
					sb16asp_ram_contents_index = 0;

				LOG(LOG_SB,LOG_DEBUG)("SB16ASP set mode register to %X",dsp.in.data[0]);
			} else {
				/* DSP Status SB 2.0/pro version. NOT SB16. */
				DSP_FlushData();
				if (type == SBT_2) DSP_AddData(0x88);
				else if ((type == SBT_PRO1) || (type == SBT_PRO2)) DSP_AddData(0x7b);
				else DSP_AddData(0xff);         //Everything enabled
			}
			break;
		case 0x05:  /* SB16 ASP set codec parameter */
			LOG(LOG_SB,LOG_NORMAL)("DSP Unhandled SB16ASP command %X (set codec parameter) value=0x%02x parameter=0x%02x",
					dsp.cmd,dsp.in.data[0],dsp.in.data[1]);
			break;
		case 0x08:  /* SB16 ASP get version */
			if (type == SBT_16) {
				switch (dsp.in.data[0]) {
					case 0x03:
						LOG(LOG_SB,LOG_DEBUG)("DSP SB16ASP command %X sub %X (get chip version)",dsp.cmd,dsp.in.data[0]);

						if (enable_asp)
							DSP_AddData(0x10);  // version ID
						else
							DSP_AddData(0xFF);  // NTS: This is what a SB16 ViBRA PnP card with no ASP returns when queried in this way
						break;
					default:
						LOG(LOG_SB,LOG_NORMAL)("DSP Unhandled SB16ASP command %X sub %X",dsp.cmd,dsp.in.data[0]);
						break;
				}
			} else {
				LOG(LOG_SB,LOG_NORMAL)("DSP Unhandled SB16ASP command %X sub %X",dsp.cmd,dsp.in.data[0]);
			}
			break;
		case 0x0e:  /* SB16 ASP set register */
			if (type == SBT_16) {
				if (enable_asp) {
					ASP_regs[dsp.in.data[0]] = dsp.in.data[1];

					if (dsp.in.data[0] == 0x83) {
						if ((ASP_mode&0x88) == 0x88) { // bit 3 and bit 7 must be set
							// memory access mode
							if (ASP_mode & 4) // NTS: As far as I can tell...
								sb16asp_ram_contents_index = 0;

							// log it, write it
							LOG(LOG_SB,LOG_DEBUG)("SB16 ASP write internal RAM byte index=0x%03x val=0x%02x",sb16asp_ram_contents_index,dsp.in.data[1]);
							sb16asp_write_current_RAM_byte(dsp.in.data[1]);

							if (ASP_mode & 2) // if bit 1 of the mode is set, memory index increment on write
								sb16asp_next_RAM_byte();
						}
						else {
							// unknown. nothing, I assume?
							LOG(LOG_SB,LOG_WARN)("SB16 ASP set register 0x83 not implemented for non-memory mode (unknown behavior)\n");
						}
					}
					else {
						LOG(LOG_SB,LOG_DEBUG)("SB16 ASP set register reg=0x%02x val=0x%02x",dsp.in.data[0],dsp.in.data[1]);
					}
				}
				else {
					LOG(LOG_SB,LOG_DEBUG)("SB16 ASP set register reg=0x%02x val=0x%02x ignored, ASP not enabled",dsp.in.data[0],dsp.in.data[1]);
				}
			} else {
				LOG(LOG_SB,LOG_NORMAL)("DSP Unhandled SB16ASP command %X (set register)",dsp.cmd);
			}
			break;
		case 0x0f:  /* SB16 ASP get register */
			if (type == SBT_16) {
				// FIXME: We have to emulate this whether or not ASP emulation is enabled. Windows 98 SB16 driver requires this.
				//        The question is: What does actual hardware do here exactly?
				if (enable_asp && dsp.in.data[0] == 0x83) {
					if ((ASP_mode&0x88) == 0x88) { // bit 3 and bit 7 must be set
						// memory access mode
						if (ASP_mode & 4) // NTS: As far as I can tell...
							sb16asp_ram_contents_index = 0;

						// log it, read it
						ASP_regs[0x83] = sb16asp_read_current_RAM_byte();
						LOG(LOG_SB,LOG_DEBUG)("SB16 ASP read internal RAM byte index=0x%03x => val=0x%02x",sb16asp_ram_contents_index,ASP_regs[0x83]);

						if (ASP_mode & 1) // if bit 0 of the mode is set, memory index increment on read
							sb16asp_next_RAM_byte();
					}
					else {
						// chip version ID
						ASP_regs[0x83] = 0x10;
					}
				}
				else {
					LOG(LOG_SB,LOG_DEBUG)("SB16 ASP get register reg=0x%02x, returning 0x%02x",dsp.in.data[0],ASP_regs[dsp.in.data[0]]);
				}

				DSP_AddData(ASP_regs[dsp.in.data[0]]);
			} else {
				LOG(LOG_SB,LOG_NORMAL)("DSP Unhandled SB16ASP command %X (get register)",dsp.cmd);
			}
			break;
		case 0x10:  /* Direct DAC */
			DSP_ChangeMode(MODE_DAC);

			/* just in case something is trying to play direct DAC audio while the speaker is turned off... */
			if (!speaker && directdac_warn_speaker_off) {
				LOG(LOG_SB,LOG_DEBUG)("DSP direct DAC sample written while speaker turned off. Program should use DSP command 0xD1 to turn it on.");
				directdac_warn_speaker_off = false;
			}

			freq = 22050;
			freq_derived_from_tc = true;
			dac.dac_pt = dac.dac_t;
			dac.dac_t = PIC_FullIndex();
			{
				double dt = dac.dac_t - dac.dac_pt; // time in milliseconds since last direct DAC output
				double rt = 1000 / dt; // estimated sample rate according to dt
				int s,sc = 1;

				// cap rate estimate to sanity. <= 1KHz means rendering once per timer tick in DOSBox,
				// so there's no point below that rate in additional rendering.
				if (rt < 1000) rt = 1000;

				// FIXME: What does the ESS AudioDrive do to its filter/sample rate divider registers when emulating this Sound Blaster command?
				ESSreg(0xA1) = 128 - (397700 / 22050);
				ESSreg(0xA2) = 256 - (7160000 / (82 * ((4 * 22050) / 10)));

				// Direct DAC playback could be thought of as application-driven 8-bit output up to 23KHz.
				// The sound card isn't given any hint what the actual sample rate is, only that it's given
				// instruction when to change the 8-bit value being output to the DAC which is why older DOS
				// games using this method tend to sound "grungy" compared to DMA playback. We recreate the
				// effect here by asking the mixer to do its linear interpolation as if at 23KHz while
				// rendering the audio at whatever rate the DOS game is giving it to us.
				chan->SetFreq((Bitu)(rt * 0x100),0x100);
				updateSoundBlasterFilter(freq);

				// avoid popping/crackling artifacts through the mixer by ensuring the render output is prefilled enough
				if (chan->msbuffer_o < 40/*FIXME: ask the mixer code!*/) sc += 2/*FIXME: use mixer rate / rate math*/;

				// do it
				for (s=0;s < sc;s++) chan->AddSamples_m8(1,(uint8_t*)(&dsp.in.data[0]));
			}
			break;
		case 0x99:  /* Single Cycle 8-Bit DMA High speed DAC */
			DSP_SB201_ABOVE;
			/* fall through */
		case 0x24:  /* Single Cycle 8-Bit DMA ADC */
			dma.recording=true;
			DSP_PrepareDMA_Old(DSP_DMA_8,false,false,/*hispeed*/(dsp.cmd&0x80)!=0);
			LOG(LOG_SB,LOG_WARN)("Guest recording audio using SB/SBPro commands");
			break;
		case 0x98:  /* Auto Init 8-bit DMA High Speed */
			DSP_SB201_ABOVE;
		case 0x2c:  /* Auto Init 8-bit DMA */
			DSP_SB2_ABOVE; /* Note: 0x98 is documented only for DSP ver.2.x and 3.x, not 4.x */
			dma.recording=true;
			DSP_PrepareDMA_Old(DSP_DMA_8,true,false,/*hispeed*/(dsp.cmd&0x80)!=0);
			break;
		case 0x91:  /* Single Cycle 8-Bit DMA High speed DAC */
			DSP_SB201_ABOVE;
			/* fall through */
		case 0x14:  /* Single Cycle 8-Bit DMA DAC */
		case 0x15:  /* Wari hack. Waru uses this one instead of 0x14, but some weird stuff going on there anyway */
			/* Note: 0x91 is documented only for DSP ver.2.x and 3.x, not 4.x */
			dma.recording=false;
			DSP_PrepareDMA_Old(DSP_DMA_8,false,false,/*hispeed*/(dsp.cmd&0x80)!=0);
			break;
		case 0x90:  /* Auto Init 8-bit DMA High Speed */
			DSP_SB201_ABOVE;
		case 0x1c:  /* Auto Init 8-bit DMA */
			DSP_SB2_ABOVE; /* Note: 0x90 is documented only for DSP ver.2.x and 3.x, not 4.x */
			dma.recording=false;
			DSP_PrepareDMA_Old(DSP_DMA_8,true,false,/*hispeed*/(dsp.cmd&0x80)!=0);
			break;
		case 0x38:  /* Write to SB MIDI Output */
			if (midi == true) MIDI_RawOutByte(dsp.in.data[0]);
			break;
		case 0x40:  /* Set Timeconstant */
			DSP_ChangeRate(256000000ul / (65536ul - ((unsigned int)dsp.in.data[0] << 8u)));
			timeconst=dsp.in.data[0];
			freq_derived_from_tc=true;

			if (ess_type != ESS_NONE) ESSUpdateFilterFromSB();
			break;
		case 0x41:  /* Set Output Samplerate */
		case 0x42:  /* Set Input Samplerate */
			if (reveal_sc_type == RSC_SC400) {
				/* Despite reporting itself as Sound Blaster Pro compatible, the Reveal SC400 supports some SB16 commands like this one */
			}
			else {
				DSP_SB16_ONLY;
			}

			DSP_ChangeRate(((unsigned int)dsp.in.data[0] << 8u) | (unsigned int)dsp.in.data[1]);
			freq_derived_from_tc=false;
			sb16_8051_mem[0x13] = freq & 0xffu;                  // rate low
			sb16_8051_mem[0x14] = (freq >> 8u) & 0xffu;          // rate high
			break;
		case 0x48:  /* Set DMA Block Size */
			DSP_SB2_ABOVE;
			//TODO Maybe check limit for new irq?
			dma.total=1u+(unsigned int)dsp.in.data[0]+((unsigned int)dsp.in.data[1] << 8u);
			// NTS: From Creative documentation: This is the number of BYTES to transfer per IRQ, not SAMPLES!
			//      dma.total is in SAMPLES (unless 16-bit PCM over 8-bit DMA) because this code inherits that
			//      design from DOSBox SVN. This check is needed for any DOS game or application that changes
			//      DSP block size during the game (such as when transitioning from general gameplay to spoken
			//      dialogue), and it is needed to stop Freddy Pharkas from stuttering when sbtype=sb16 ref
			//      [https://github.com/joncampbell123/dosbox-x/issues/2960]
			// NTS: Do NOT divide the byte count by 2 if 16-bit PCM audio but using an 8-bit DMA channel (DSP_DMA_16_ALIASED).
			//      dma.total in that cause really does contain the byte count of a DSP block. 16-bit PCM over 8-bit DMA
			//      is possible on real hardware too, likely as a fallback in case 16-bit DMA channels are just not available.
			//      Note that on one of my ViBRA PnP cards, 8-bit DMA is the only option because 16-bit DMA doesn't work for
			//      some odd reason. --J.C.
			if (dma.mode == DSP_DMA_16) {
				// NTS: dma.total is the number of individual samples, not paired samples, likely as a side effect of how
				//      this code was originally written over at DOSBox SVN regarding how block durations are handled with
				//      the Sound Blaster Pro in which the Pro treats stereo output as just mono that is alternately latched
				//      to left and right DACs. The SB16 handling here also follows that tradition because Creative's SB16
				//      playback commands 0xB0-0xCF follow the same tradition: Block size specified in the command is given
				//      in samples, and by samples, they mean individual samples, and therefore it is still doubled when
				//      asked to play stereo audio. I suppose this is why Linux ALSA chose to further clarify the terminology
				//      by defining audio "samples" vs audio "frames".
				// NTS: The dma.total as individual sample count has been confirmed with DOSLIB and real hardware, and by
				//      looking at snd_sb16_capture_prepare() in sound/isa/sb/sb16_main.c in the Linux kernel source.
				if (dma.total & 1) LOG(LOG_SB,LOG_WARN)("DSP command 0x48: 16-bit PCM and odd number of bytes given for block length");
				dma.total >>= 1u;
			}
			break;
		case 0x75:  /* 075h : Single Cycle 4-bit ADPCM Reference */
			adpcm.haveref=true;
			/* FALLTHROUGH */
		case 0x74:  /* 074h : Single Cycle 4-bit ADPCM */
			dma.recording=false;
			DSP_PrepareDMA_Old(DSP_DMA_4,false,false,false);
			break;
		case 0x77:  /* 077h : Single Cycle 3-bit(2.6bit) ADPCM Reference*/
			adpcm.haveref=true;
			/* FALLTHROUGH */
		case 0x76:  /* 074h : Single Cycle 3-bit(2.6bit) ADPCM */
			dma.recording=false;
			DSP_PrepareDMA_Old(DSP_DMA_3,false,false,false);
			break;
		case 0x7d:  /* Auto Init 4-bit ADPCM Reference */
			DSP_SB2_ABOVE;
			adpcm.haveref=true;
			dma.recording=false;
			DSP_PrepareDMA_Old(DSP_DMA_4,true,false,false);
			break;
		case 0x7f:  /* Auto Init 3-bit(2.6bit) ADPCM Reference */
			DSP_SB2_ABOVE;
			adpcm.haveref=true;
			dma.recording=false;
			DSP_PrepareDMA_Old(DSP_DMA_3,true,false,false);
			break;
		case 0x1f:  /* Auto Init 2-bit ADPCM Reference */
			DSP_SB2_ABOVE;
			adpcm.haveref=true;
			dma.recording=false;
			DSP_PrepareDMA_Old(DSP_DMA_2,true,false,false);
			break;
		case 0x17:  /* 017h : Single Cycle 2-bit ADPCM Reference*/
			adpcm.haveref=true;
			/* FALLTHROUGH */
		case 0x16:  /* 074h : Single Cycle 2-bit ADPCM */
			dma.recording=false;
			DSP_PrepareDMA_Old(DSP_DMA_2,false,false,false);
			break;
		case 0x80:  /* Silence DAC */
			PIC_AddEvent(&DSP_RaiseIRQEvent,
				(1000.0f*(1+dsp.in.data[0]+(dsp.in.data[1] << 8))/freq),
				(card_index << CARD_INDEX_BIT));
			break;
		case 0xb0:  case 0xb1:  case 0xb2:  case 0xb3:  case 0xb4:  case 0xb5:  case 0xb6:  case 0xb7:
		case 0xb8:  case 0xb9:  case 0xba:  case 0xbb:  case 0xbc:  case 0xbd:  case 0xbe:  case 0xbf:
		case 0xc0:  case 0xc1:  case 0xc2:  case 0xc3:  case 0xc4:  case 0xc5:  case 0xc6:  case 0xc7:
		case 0xc8:  case 0xc9:  case 0xca:  case 0xcb:  case 0xcc:  case 0xcd:  case 0xce:  case 0xcf:
			/* The low 5 bits DO have specific meanings:

			   Bx - Program 16-bit DMA mode digitized sound I/O
			   Command sequence:  Command, Mode, Lo(Length-1), Hi(Length-1)
Command:
╔════╤════╤════╤════╤═══════╤══════╤═══════╤════╗
║ D7 │ D6 │ D5 │ D4 │  D3   │  D2  │  D1   │ D0 ║
╠════╪════╪════╪════╪═══════╪══════╪═══════╪════╣
║  1 │  0 │  1 │  1 │  A/D  │  A/I │ FIFO  │  0 ║
╚════╧════╧════╧════┼───────┼──────┼───────┼════╝
                    │ 0=D/A │ 0=SC │ 0=off │
                    │ 1=A/D │ 1=AI │ 1=on  │
                    └───────┴──────┴───────┘
Common commands:
B8 - 16-bit single-cycle input
B0 - 16-bit single-cycle output
BE - 16-bit auto-initialized input
B6 - 16-bit auto-initialized output

Mode:
╔════╤════╤══════════╤════════════╤════╤════╤════╤════╗
║ D7 │ D6 │    D5    │     D4     │ D3 │ D2 │ D1 │ D0 ║
╠════╪════╪══════════╪════════════╪════╪════╪════╪════╣
║  0 │  0 │  Stereo  │   Signed   │  0 │  0 │  0 │  0 ║
╚════╧════┼──────────┼────────────┼════╧════╧════╧════╝
          │ 0=Mono   │ 0=unsigned │
          │ 1=Stereo │ 1=signed   │
          └──────────┴────────────┘

Cx - Program 8-bit DMA mode digitized sound I/O
Same procedure as 16-bit sound I/O using command Bx
Common commands:
C8 - 8-bit single-cycle input
C0 - 8-bit single-cycle output
CE - 8-bit auto-initialized input
C6 - 8-bit auto-initialized output

Note that this code makes NO attempt to distinguish recording vs playback commands, which
is responsible for some failures such as [https://github.com/joncampbell123/dosbox-x/issues/1589]
*/
			if (reveal_sc_type == RSC_SC400) {
				/* Despite reporting itself as Sound Blaster Pro, the Reveal SC400 cards do support *some* SB16 DSP commands! */
				/* BUT, it only recognizes a subset of this range. */
				if (dsp.cmd == 0xBE || dsp.cmd == 0xB6 ||
						dsp.cmd == 0xCE || dsp.cmd == 0xC6) {
					/* OK! */
				}
				else {
					LOG(LOG_SB,LOG_DEBUG)("SC400: SB16 playback command not recognized");
					break;
				}
			}
			else {
				DSP_SB16_ONLY;
			}

			if (dsp.cmd & 8) LOG(LOG_SB,LOG_WARN)("Guest recording audio using SB16 commands");

			/* Generic 8/16 bit DMA */
			//      DSP_SetSpeaker(true);       //SB16 always has speaker enabled
			dma.sign=(dsp.in.data[0] & 0x10) > 0;
			dma.recording=(dsp.cmd & 0x8) > 0;
			DSP_PrepareDMA_New((dsp.cmd & 0x10) ? DSP_DMA_16 : DSP_DMA_8,
					1u+(unsigned int)dsp.in.data[1]+((unsigned int)dsp.in.data[2] << 8u),
					(dsp.cmd & 0x4)>0,
					(dsp.in.data[0] & 0x20) > 0
					);
			break;
		case 0xd5:  /* Halt 16-bit DMA */
			DSP_SB16_ONLY;
		case 0xd0:  /* Halt 8-bit DMA */
			chan->FillUp();
			//      DSP_ChangeMode(MODE_NONE);
			//      Games sometimes already program a new dma before stopping, gives noise
			if (mode==MODE_NONE) {
				// possibly different code here that does not switch to MODE_DMA_PAUSE
			}
			mode=MODE_DMA_PAUSE;
			PIC_RemoveEvents(END_DMA_Event);
			PIC_RemoveEvents(DMA_DAC_Event);
			break;
		case 0xd1:  /* Enable Speaker */
			chan->FillUp();
			DSP_SetSpeaker(true);
			break;
		case 0xd3:  /* Disable Speaker */
			chan->FillUp();
			DSP_SetSpeaker(false);

			/* There are demoscene productions that reinitialize sound between parts.
			 * But instead of stopping playback, then starting it again, the demo leaves
			 * DMA running through RAM and expects the "DSP Disable Speaker" command to
			 * prevent the arbitrary contents of RAM from coming out the sound card as static
			 * while it loads data. The problem is, DSP enable/disable speaker commands don't
			 * do anything on Sound Blaster 16 cards. This is why such demos run fine when
			 * sbtype=sbpro2, but emit static/noise between demo parts when sbtype=sb16.
			 * The purpose of this warning is to clue the user on in this fact and suggest
			 * a fix.
			 *
			 * Demoscene productions known to have this bug/problem with sb16:
			 * - "Saga" by Dust (1993)                       noise/static between demo parts
			 * - "Facts of life" by Witan (1992)             noise/static during star wars scroller at the beginning
			 */
			if (type == SBT_16 && mode == MODE_DMA)
				LOG(LOG_MISC,LOG_WARN)("SB16 warning: DSP Disable Speaker command used while DMA is running, which has no effect on audio output on SB16 hardware. Audible noise/static may occur. You can eliminate the noise by setting sbtype=sbpro2");

			break;
		case 0xd8:  /* Speaker status */
			DSP_SB2_ABOVE;
			DSP_FlushData();
			if (speaker) DSP_AddData(0xff);
			else DSP_AddData(0x00);
			break;
		case 0xd6:  /* Continue DMA 16-bit */
			DSP_SB16_ONLY;
		case 0xd4:  /* Continue DMA 8-bit*/
			chan->FillUp();
			if (mode==MODE_DMA_PAUSE) {
				mode=MODE_DMA_MASKED;
				if (dma.chan!=NULL) dma.chan->Register_Callback(DSP_DMA_CallBack);
			}
			break;
		case 0x47:  /* Continue Autoinitialize 16-bit */
		case 0x45:  /* Continue Autoinitialize 8-bit */
			DSP_SB16_ONLY;
			chan->FillUp();
			dma.autoinit=true; // No. This DSP command does not resume DMA playback
			break;
		case 0xd9:  /* Exit Autoinitialize 16-bit */
			DSP_SB16_ONLY;
		case 0xda:  /* Exit Autoinitialize 8-bit */
			DSP_SB2_ABOVE;
			/* Set mode to single transfer so it ends with current block */
			dma.autoinit=false;      //Should stop itself
			chan->FillUp();
			break;
		case 0xe0:  /* DSP Identification - SB2.0+ */
			DSP_FlushData();
			DSP_AddData(~dsp.in.data[0]);
			break;
		case 0xe1:  /* Get DSP Version */
			DSP_FlushData();
			switch (type) {
				case SBT_1:
					if (subtype == SBST_100) { DSP_AddData(0x1); DSP_AddData(0x0); }
					else { DSP_AddData(0x1); DSP_AddData(0x5); }
					break;
				case SBT_2:
					if (subtype == SBST_200) { DSP_AddData(0x2); DSP_AddData(0x0); }
					else { DSP_AddData(0x2); DSP_AddData(0x1); }
					break;
				case SBT_PRO1:
					DSP_AddData(0x3); DSP_AddData(0x0);break;
				case SBT_PRO2:
					if (ess_type != ESS_NONE) {
						DSP_AddData(0x3); DSP_AddData(0x1);
					}
					else if (reveal_sc_type == RSC_SC400) { // SC400 cards report as v3.5 by default, but there is a DSP command to change the version!
						DSP_AddData(sc400_dsp_major); DSP_AddData(sc400_dsp_minor);
					}
					else {
						DSP_AddData(0x3); DSP_AddData(0x2);
					}
					break;
				case SBT_16:
					if (vibra) {
						DSP_AddData(4); /* SB16 ViBRA DSP 4.13 */
						DSP_AddData(13);
					}
					else {
						DSP_AddData(4); /* SB16 DSP 4.05 */
						DSP_AddData(5);
					}
					break;
				default:
					break;
			}
			break;
		case 0xe2:  /* Weird DMA identification write routine */
			{
				LOG(LOG_SB,LOG_NORMAL)("DSP Function 0xe2");
				e2.valadd += dsp.in.data[0] ^ e2.valxor;
				e2.valxor = (e2.valxor >> 2u) | (e2.valxor << 6u);
				GetDMAChannel(hw.dma8)->userData = card_index;
				GetDMAChannel(hw.dma8)->Register_Callback(DSP_E2_DMA_CallBack);
			}
			break;
		case 0xe3:  /* DSP Copyright */
			{
				DSP_FlushData();
				if (ess_type != ESS_NONE) {
					/* ESS chips do not return copyright string */
					DSP_AddData(0);
				}
				else if (reveal_sc_type == RSC_SC400) {
					static const char *gallant = "SC-6000";

					/* NTS: Yes, this writes the terminating NUL as well. Not a bug. */
					for (size_t i=0;i<=strlen(gallant);i++) {
						DSP_AddData((uint8_t)gallant[i]);
					}
				}
				else if (type <= SBT_PRO2) {
					/* Sound Blaster DSP 2.0: No copyright string observed. */
					/* Sound Blaster Pro DSP 3.1: No copyright string observed. */
					/* I have yet to observe if a Sound Blaster Pro DSP 3.2 (SBT_PRO2) returns a copyright string. */
					/* no response */
				}
				else {
					/* NTS: Yes, this writes the terminating NUL as well. Not a bug. */
					for (size_t i=0;i<=strlen(copyright_string);i++) {
						DSP_AddData((uint8_t)copyright_string[i]);
					}
				}
			}
			break;
		case 0xe4:  /* Write Test Register */
			dsp.test_register=dsp.in.data[0];
			break;
		case 0xe7:  /* ESS detect/read config */
			if (ess_type != ESS_NONE) {
				DSP_FlushData();
				switch (ess_type) {
					case ESS_NONE:
						break;
					case ESS_688:
						DSP_AddData(0x68);
						DSP_AddData(0x80 | 0x04);
						break;
					case ESS_1688:
						// Determined via Windows driver debugging.
						DSP_AddData(0x68);
						DSP_AddData(0x80 | 0x09);
						break;
				}
			}
			break;
		case 0xe8:  /* Read Test Register */
			DSP_FlushData();
			DSP_AddData(dsp.test_register);
			break;
		case 0xf2:  /* Trigger 8bit IRQ */
			//Small delay in order to emulate the slowness of the DSP, fixes Llamatron 2012 and Lemmings 3D
			PIC_AddEvent(&DSP_RaiseIRQEvent,0.01f,(card_index << CARD_INDEX_BIT));
			break;
		case 0xa0: case 0xa8: /* Documented only for DSP 3.x */
			if (type == SBT_PRO1 || type == SBT_PRO2)
				mixer.stereo = (dsp.cmd & 8) > 0; /* <- HACK! 0xA0 input mode to mono 0xA8 stereo */
			else
				LOG(LOG_SB,LOG_WARN)("DSP command A0h/A8h are only supported in Sound Blaster Pro emulation mode");
			break;
		case 0xf3:   /* Trigger 16bit IRQ */
			DSP_SB16_ONLY;
			SB_RaiseIRQ(SB_IRQ_16);
			break;
		case 0xf8:  /* Undocumented, pre-SB16 only */
			DSP_FlushData();
			DSP_AddData(0);
			break;
		case 0x30: case 0x31: case 0x32: case 0x33:
			LOG(LOG_SB,LOG_ERROR)("DSP:Unimplemented MIDI I/O command %2X",dsp.cmd);
			break;
		case 0x34: /* MIDI Read Poll & Write Poll */
			DSP_SB2_ABOVE;
			LOG(LOG_SB,LOG_DEBUG)("DSP:Entering MIDI Read/Write Poll mode");
			dsp.midi_rwpoll_mode = true;
			break;
		case 0x35: /* MIDI Read Interrupt & Write Poll */
			DSP_SB2_ABOVE;
			LOG(LOG_SB,LOG_DEBUG)("DSP:Entering MIDI Read Interrupt/Write Poll mode");
			dsp.midi_rwpoll_mode = true;
			dsp.midi_read_interrupt = true;
			break;
		case 0x37: /* MIDI Read Timestamp Interrupt & Write Poll */
			DSP_SB2_ABOVE;
			LOG(LOG_SB,LOG_DEBUG)("DSP:Entering MIDI Read Timestamp Interrupt/Write Poll mode");
			dsp.midi_rwpoll_mode = true;
			dsp.midi_read_interrupt = true;
			dsp.midi_read_with_timestamps = true;
			break;
		case 0x20:
			DSP_AddData(0x7f);   // fake silent input for Creative parrot
			break;
		case 0x88: /* Reveal SC400 ??? (used by TESTSC.EXE) */
			if (reveal_sc_type != RSC_SC400) break;
			/* ??? */
			break;
		case 0xE6: /* Reveal SC400 DMA test */
			if (reveal_sc_type != RSC_SC400) break;
			GetDMAChannel(hw.dma8)->userData = card_index;
			GetDMAChannel(hw.dma8)->Register_Callback(DSP_SC400_E6_DMA_CallBack);
			dsp.out.lastval = 0x80;
			dsp.out.used = 0;
			break;
		case 0x50: /* Reveal SC400 write configuration */
			if (reveal_sc_type != RSC_SC400) break;
			sc400_cfg = dsp.in.data[0];

			switch (dsp.in.data[0]&3) {
				case 0: hw.dma8 = (uint8_t)(-1); break;
				case 1: hw.dma8 =  0u; break;
				case 2: hw.dma8 =  1u; break;
				case 3: hw.dma8 =  3u; break;
			}
			hw.dma16 = hw.dma8;
			switch ((dsp.in.data[0]>>3)&7) {
				case 0: hw.irq =  (uint8_t)(-1); break;
				case 1: hw.irq =  7u; break;
				case 2: hw.irq =  9u; break;
				case 3: hw.irq = 10u; break;
				case 4: hw.irq = 11u; break;
				case 5: hw.irq =  5u; break;
				case 6: hw.irq =  (uint8_t)(-1); break;
				case 7: hw.irq =  (uint8_t)(-1); break;
			}
			{
				int irq;

				if (dsp.in.data[0]&0x04) /* MPU IRQ enable bit */
					irq = (dsp.in.data[0]&0x80) ? 9 : 5;
				else
					irq = -1;

				if (irq != MPU401_GetIRQ())
					LOG(LOG_SB,LOG_WARN)("SC400 warning: MPU401 emulation does not yet support changing the IRQ through configuration commands");
			}

			LOG(LOG_SB,LOG_DEBUG)("SC400: New configuration byte %02x irq=%d dma=%d",
					dsp.in.data[0],(int)hw.irq,(int)hw.dma8);

			break;
		case 0x58: /* Reveal SC400 read configuration */
			if (reveal_sc_type != RSC_SC400) break;
			DSP_AddData(sc400_jumper_status_1);
			DSP_AddData(sc400_jumper_status_2);
			DSP_AddData(sc400_cfg);
			break;
		case 0x6E: /* Reveal SC400 SBPRO.EXE set DSP version */
			if (reveal_sc_type != RSC_SC400) break;
			sc400_dsp_major = dsp.in.data[0];
			sc400_dsp_minor = dsp.in.data[1];
			LOG(LOG_SB,LOG_DEBUG)("SC400: DSP version changed to %u.%u",
					sc400_dsp_major,sc400_dsp_minor);
			break;
		case 0xfa:  /* SB16 8051 memory write */
			if (type == SBT_16) {
				sb16_8051_mem[dsp.in.data[0]] = dsp.in.data[1];
				LOG(LOG_SB,LOG_NORMAL)("SB16 8051 memory write %x byte %x",dsp.in.data[0],dsp.in.data[1]);
			} else {
				LOG(LOG_SB,LOG_ERROR)("DSP:Unhandled (undocumented) command %2X",dsp.cmd);
			}
			break;
		case 0xf9:  /* SB16 8051 memory read */
			if (type == SBT_16) {
				DSP_AddData(sb16_8051_mem[dsp.in.data[0]]);
				LOG(LOG_SB,LOG_NORMAL)("SB16 8051 memory read %x, got byte %x",dsp.in.data[0],sb16_8051_mem[dsp.in.data[0]]);
			} else {
				LOG(LOG_SB,LOG_ERROR)("DSP:Unhandled (undocumented) command %2X",dsp.cmd);
			}
			break;
		default:
			LOG(LOG_SB,LOG_ERROR)("DSP:Unhandled (undocumented) command %2X",dsp.cmd);
			break;
	}

	if (type == SBT_16) {
		// FIXME: This is a guess! See also [https://github.com/joncampbell123/dosbox-x/issues/1044#issuecomment-480024957]
		sb16_8051_mem[0x30] = dsp.last_cmd; /* last_cmd */
	}

	dsp.last_cmd=dsp.cmd;
	dsp.cmd=DSP_NO_COMMAND;
	dsp.cmd_len=0;
	dsp.in.pos=0;
}

// TODO: Put out the various hardware listed here, do some listening tests to confirm the emulation is accurate.
void SB_INFO::updateSoundBlasterFilter(Bitu rate) {
	/* "No filtering" option for those who don't want it, or are used to the way things sound in plain vanilla DOSBox */
	if (no_filtering) {
		chan->SetLowpassFreq(0/*off*/);
		chan->SetSlewFreq(0/*normal linear interpolation*/);
		return;
	}

	/* different sound cards filter their output differently */
	if (ess_type != ESS_NONE) { // ESS AudioDrive. Tested against real hardware (ESS 688) by Jonathan C.
		/* ESS AudioDrive lets the driver decide what the cutoff/rolloff to use */
		/* "The ratio of the roll-off frequency to the clock frequency is 1:82. In other words,
		 * first determine the desired roll off frequency by taking 80% of the sample rate
		 * divided by 2, the multiply by 82 to find the desired filter clock frequency"
		 *
		 * Try to approximate the ESS's filter by undoing the calculation then feeding our own lowpass filter with it.
		 *
		 * This implementation is matched against real hardware by ear, even though the reference hardware is a
		 * laptop with a cheap tinny amplifier */
		uint64_t filter_raw = (uint64_t)7160000ULL / (256u - ESSreg(0xA2));
		uint64_t filter_hz = (filter_raw * (uint64_t)11) / (uint64_t)(82 * 4); /* match lowpass by ear compared to real hardware */

		if ((filter_hz * 2) > freq)
			chan->SetSlewFreq(filter_hz * 2 * chan->freq_d_orig);
		else
			chan->SetSlewFreq(0);

		chan->SetLowpassFreq(filter_hz,/*order*/8);
	}
	else if (type == SBT_16 || // Sound Blaster 16 (DSP 4.xx). Tested against real hardware (CT4180 ViBRA 16C PnP) by Jonathan C.
			reveal_sc_type == RSC_SC400) { // Reveal SC400 (DSP 3.5). Tested against real hardware by Jonathan C.
		// My notes: The DSP automatically applies filtering at low sample rates. But the DSP has to know
		//           what the sample rate is to filter. If you use direct DAC output (DSP command 0x10)
		//           then no filtering is applied and the sound comes out grungy, just like older Sound
		//           Blaster cards.
		//
		//           I can also confirm the SB16's reputation for hiss and noise is true, it's noticeable
		//           with earbuds and the mixer volume at normal levels. --Jonathan C.
		if (mode == MODE_DAC) {
			chan->SetLowpassFreq(23000);
			chan->SetSlewFreq(23000 * chan->freq_d_orig);
		}
		else {
			chan->SetLowpassFreq(rate/2,1);
			chan->SetSlewFreq(0/*normal linear interpolation*/);
		}
	}
	else if (type == SBT_PRO1 || type == SBT_PRO2) { // Sound Blaster Pro (DSP 3.x). Tested against real hardware (CT1600) by Jonathan C.
		chan->SetSlewFreq(23000 * chan->freq_d_orig);
		if (mixer.filter_bypass)
			chan->SetLowpassFreq(23000); // max sample rate 46000Hz. slew rate filter does the rest of the filtering for us.
		else
			chan->SetLowpassFreq(3800); // NOT documented by Creative, guess based on listening tests with a CT1600, and documented Input filter freqs
	}
	else if (type == SBT_1 || type == SBT_2) { // Sound Blaster DSP 1.x and 2.x (not Pro). Tested against real hardware (CT1350B) by Jonathan C.
		/* As far as I can tell the DAC outputs sample-by-sample with no filtering whatsoever, aside from the limitations of analog audio */
		chan->SetSlewFreq(23000 * chan->freq_d_orig);
		chan->SetLowpassFreq(23000);
	}
}

void SB_INFO::DSP_ChangeStereo(bool stereo) {
	if (!dma.stereo && stereo) {
		chan->SetFreq(freq/2);
		updateSoundBlasterFilter(freq/2);
		dma.mul*=2;
		dma.rate=(freq*dma.mul) >> SB_SH;
		dma.min=((Bitu)dma.rate*(Bitu)(min_dma_user >= 0 ? min_dma_user : /*default*/3))/1000u;
	} else if (dma.stereo && !stereo) {
		chan->SetFreq(freq);
		updateSoundBlasterFilter(freq);
		dma.mul/=2;
		dma.rate=(freq*dma.mul) >> SB_SH;
		dma.min=((Bitu)dma.rate*(Bitu)(min_dma_user >= 0 ? min_dma_user : /*default*/3))/1000;
	}
	dma.stereo=stereo;
}

/* Sound Blaster Pro 2 (CT1600) notes:
 *
 * - Registers 0x40-0xFF do nothing written and read back 0xFF.
 * - Registers 0x00-0x3F are almost exact mirrors of registers 0x00-0x0F, but not quite
 * - Registers 0x00-0x1F are exact mirrors of 0x00-0x0F
 * - Registers 0x20-0x3F are exact mirrors of 0x20-0x2F which are.... non functional shadow copies of 0x00-0x0F (???)
 * - Register 0x0E is mirrored at 0x0F, 0x1E, 0x1F. Reading 0x00, 0x01, 0x10, 0x11 also reads register 0x0E.
 * - Writing 0x00, 0x01, 0x10, 0x11 resets the mixer as expected.
 * - Register 0x0E is 0x11 on reset, which defaults to mono and lowpass filter enabled.
 * - See screenshot for mixer registers on hardware or mixer reset, file 'CT1600 Sound Blaster Pro 2 mixer register dump hardware and mixer reset state.png' */
void SB_INFO::CTMIXER_Write(uint8_t val) {
	switch (mixer.index) {
		case 0x00:      /* Reset */
			CTMIXER_Reset();
			LOG(LOG_SB,LOG_WARN)("Mixer reset value %x",val);
			break;
		case 0x02:      /* Master Volume (SB2 Only) */
			SETPROVOL(mixer.master,(val&0xf)|(val<<4));
			CTMIXER_UpdateVolumes();
			break;
		case 0x04:      /* DAC Volume (SBPRO) */
			SETPROVOL(mixer.dac,val);
			CTMIXER_UpdateVolumes();
			break;
		case 0x06:      /* FM output selection, Somewhat obsolete with dual OPL SBpro + FM volume (SB2 Only) */
			//volume controls both channels
			SETPROVOL(mixer.fm,(val&0xf)|(val<<4));
			CTMIXER_UpdateVolumes();
			if(val&0x60) LOG(LOG_SB,LOG_WARN)("Turned FM one channel off. not implemented %X",val);
			//TODO Change FM Mode if only 1 fm channel is selected
			break;
		case 0x08:      /* CDA Volume (SB2 Only) */
			SETPROVOL(mixer.cda,(val&0xf)|(val<<4));
			CTMIXER_UpdateVolumes();
			break;
		case 0x0a:      /* Mic Level (SBPRO) or DAC Volume (SB2): 2-bit, 3-bit on SB16 */
			if (type==SBT_2) {
				mixer.dac[0]=mixer.dac[1]=((val & 0x6) << 2)|3;
				CTMIXER_UpdateVolumes();
			} else {
				mixer.mic=((val & 0x7) << 2)|(type==SBT_16?1:3);
			}
			break;
		case 0x0e:      /* Output/Stereo Select */
			/* Real CT1600 notes:
			 *
			 * This register only allows changing bits 1 and 5. Each nibble can be 1 or 3, therefore on readback it will always be 0x11, 0x13, 0x31, or 0x11.
			 * This register is also mirrored at 0x0F, 0x1E, 0x1F. On read this register also appears over 0x00, 0x01, 0x10, 0x11, though writing that register
			 * resets the mixer as expected. */
			/* only allow writing stereo bit if Sound Blaster Pro OR if a SB16 and user says to allow it */
			if ((type == SBT_PRO1 || type == SBT_PRO2) || (type == SBT_16 && !sbpro_stereo_bit_strict_mode))
				mixer.stereo=(val & 0x2) > 0;

			mixer.sbpro_stereo=(val & 0x2) > 0;
			mixer.filter_bypass=(val & 0x20) > 0; /* filter is ON by default, set the bit to turn OFF the filter */
			DSP_ChangeStereo(mixer.stereo);
			updateSoundBlasterFilter(freq);

			/* help the user out if they leave sbtype=sb16 and then wonder why their DOS game is producing scratchy monural audio. */
			if (type == SBT_16 && sbpro_stereo_bit_strict_mode && (val&0x2) != 0)
				LOG(LOG_SB,LOG_WARN)("Mixer stereo/mono bit is set. This is Sound Blaster Pro style Stereo which is not supported by sbtype=sb16, consider using sbtype=sbpro2 instead.");
			break;
		case 0x14:      /* Audio 1 Play Volume (ESS 688) */
			if (ess_type != ESS_NONE) {
				mixer.dac[0]=expand16to32((val>>4)&0xF);
				mixer.dac[1]=expand16to32(val&0xF);
				CTMIXER_UpdateVolumes();
			}
			break;
		case 0x22:      /* Master Volume (SBPRO) */
			SETPROVOL(mixer.master,val);
			CTMIXER_UpdateVolumes();
			break;
		case 0x26:      /* FM Volume (SBPRO) */
			SETPROVOL(mixer.fm,val);
			CTMIXER_UpdateVolumes();
			break;
		case 0x28:      /* CD Audio Volume (SBPRO) */
			SETPROVOL(mixer.cda,val);
			CTMIXER_UpdateVolumes();
			break;
		case 0x2e:      /* Line-in Volume (SBPRO) */
			SETPROVOL(mixer.lin,val);
			break;
			//case 0x20:        /* Master Volume Left (SBPRO) ? */
		case 0x30:      /* Master Volume Left (SB16) */
			if (type==SBT_16) {
				mixer.master[0]=val>>3;
				CTMIXER_UpdateVolumes();
			}
			break;
			//case 0x21:        /* Master Volume Right (SBPRO) ? */
		case 0x31:      /* Master Volume Right (SB16) */
			if (type==SBT_16) {
				mixer.master[1]=val>>3;
				CTMIXER_UpdateVolumes();
			}
			break;
		case 0x32:      /* DAC Volume Left (SB16) */
			/* Master Volume (ESS 688) */
			if (type==SBT_16) {
				mixer.dac[0]=val>>3;
				CTMIXER_UpdateVolumes();
			}
			else if (ess_type != ESS_NONE) {
				mixer.master[0]=expand16to32((val>>4)&0xF);
				mixer.master[1]=expand16to32(val&0xF);
				CTMIXER_UpdateVolumes();
			}
			break;
		case 0x33:      /* DAC Volume Right (SB16) */
			if (type==SBT_16) {
				mixer.dac[1]=val>>3;
				CTMIXER_UpdateVolumes();
			}
			break;
		case 0x34:      /* FM Volume Left (SB16) */
			if (type==SBT_16) {
				mixer.fm[0]=val>>3;
				CTMIXER_UpdateVolumes();
			}
			break;
		case 0x35:      /* FM Volume Right (SB16) */
			if (type==SBT_16) {
				mixer.fm[1]=val>>3;
				CTMIXER_UpdateVolumes();
			}
			break;
		case 0x36:      /* CD Volume Left (SB16) */
			/* FM Volume (ESS 688) */
			if (type==SBT_16) {
				mixer.cda[0]=val>>3;
				CTMIXER_UpdateVolumes();
			}
			else if (ess_type != ESS_NONE) {
				mixer.fm[0]=expand16to32((val>>4)&0xF);
				mixer.fm[1]=expand16to32(val&0xF);
				CTMIXER_UpdateVolumes();
			}
			break;
		case 0x37:      /* CD Volume Right (SB16) */
			if (type==SBT_16) {
				mixer.cda[1]=val>>3;
				CTMIXER_UpdateVolumes();
			}
			break;
		case 0x38:      /* Line-in Volume Left (SB16) */
			/* AuxA (CD) Volume Register (ESS 688) */
			if (type==SBT_16)
				mixer.lin[0]=val>>3;
			else if (ess_type != ESS_NONE) {
				mixer.cda[0]=expand16to32((val>>4)&0xF);
				mixer.cda[1]=expand16to32(val&0xF);
				CTMIXER_UpdateVolumes();
			}
			break;
		case 0x39:      /* Line-in Volume Right (SB16) */
			if (type==SBT_16) mixer.lin[1]=val>>3;
			break;
		case 0x3a:
			if (type==SBT_16) mixer.mic=val>>3;
			break;
		case 0x3e:      /* Line Volume (ESS 688) */
			if (ess_type != ESS_NONE) {
				mixer.lin[0]=expand16to32((val>>4)&0xF);
				mixer.lin[1]=expand16to32(val&0xF);
			}
			break;
		case 0x80:      /* IRQ Select */
			if (type==SBT_16 && !vibra) { /* ViBRA PnP cards do not allow reconfiguration by this byte */
				hw.irq=0xff;
				if (IS_PC98_ARCH) {
					if (val & 0x1) hw.irq=3;
					else if (val & 0x2) hw.irq=10;
					else if (val & 0x4) hw.irq=12;
					else if (val & 0x8) hw.irq=5;

					// NTS: Real hardware stores only the low 4 bits. The upper 4 bits will always read back 1111.
					//      The value read back will always be Fxh where x contains the 4 bits checked here.
				}
				else {
					if (val & 0x1) hw.irq=2;
					else if (val & 0x2) hw.irq=5;
					else if (val & 0x4) hw.irq=7;
					else if (val & 0x8) hw.irq=10;
				}
			}
			break;
		case 0x81:      /* DMA Select */
			if (type==SBT_16 && !vibra) { /* ViBRA PnP cards do not allow reconfiguration by this byte */
				hw.dma8=0xff;
				hw.dma16=0xff;
				if (IS_PC98_ARCH) {
					pc98_mixctl_reg = (unsigned char)val ^ 0x14;

					if (val & 0x1) hw.dma8=0;
					else if (val & 0x2) hw.dma8=3;

					// NTS: On real hardware, only bits 0 and 1 are writeable. bits 2 and 4 seem to act oddly in response to
					//      bytes written:
					//
					//      write 0x00          read 0x14
					//      write 0x01          read 0x15
					//      write 0x02          read 0x16
					//      write 0x03          read 0x17
					//      write 0x04          read 0x10
					//      write 0x05          read 0x11
					//      write 0x06          read 0x12
					//      write 0x07          read 0x13
					//      write 0x08          read 0x1C
					//      write 0x09          read 0x1D
					//      write 0x0A          read 0x1E
					//      write 0x0B          read 0x1F
					//      write 0x0C          read 0x18
					//      write 0x0D          read 0x19
					//      write 0x0E          read 0x1A
					//      write 0x0F          read 0x1B
					//      write 0x10          read 0x04
					//      write 0x11          read 0x05
					//      write 0x12          read 0x06
					//      write 0x13          read 0x07
					//      write 0x14          read 0x00
					//      write 0x15          read 0x01
					//      write 0x16          read 0x02
					//      write 0x17          read 0x03
					//      write 0x18          read 0x0C
					//      write 0x19          read 0x0D
					//      write 0x1A          read 0x0E
					//      write 0x1B          read 0x0F
					//      write 0x1C          read 0x08
					//      write 0x1D          read 0x09
					//      write 0x1E          read 0x0A
					//      write 0x1F          read 0x0B
					//
					//      This pattern repeats for any 5 bit value in bits [4:0] i.e. 0x20 will read back 0x34.
				}
				else {
					if (val & 0x1) hw.dma8=0;
					else if (val & 0x2) hw.dma8=1;
					else if (val & 0x8) hw.dma8=3;
					if (val & 0x20) hw.dma16=5;
					else if (val & 0x40) hw.dma16=6;
					else if (val & 0x80) hw.dma16=7;
				}
				LOG(LOG_SB,LOG_NORMAL)("Mixer select dma8:%x dma16:%x",hw.dma8,hw.dma16);
			}
			break;
		default:

			if( ((type == SBT_PRO1 || type == SBT_PRO2) && mixer.index==0x0c) || /* Input control on SBPro */
					(type == SBT_16 && mixer.index >= 0x3b && mixer.index <= 0x47)) /* New SB16 registers */
				mixer.unhandled[mixer.index] = val;
			LOG(LOG_SB,LOG_WARN)("MIXER:Write %X to unhandled index %X",val,mixer.index);
	}
}

uint8_t SB_INFO::CTMIXER_Read(void) {
	uint8_t ret = 0;

	//  if ( mixer.index< 0x80) LOG_MSG("Read mixer %x",mixer.index);
	switch (mixer.index) {
		case 0x00:      /* RESET */
			return 0x00;
		case 0x02:      /* Master Volume (SB2 Only) */
			return ((mixer.master[1]>>1) & 0xe);
		case 0x22:      /* Master Volume (SBPRO) */
			return  MAKEPROVOL(mixer.master);
		case 0x04:      /* DAC Volume (SBPRO) */
			return MAKEPROVOL(mixer.dac);
		case 0x06:      /* FM Volume (SB2 Only) + FM output selection */
			return ((mixer.fm[1]>>1) & 0xe);
		case 0x08:      /* CD Volume (SB2 Only) */
			return ((mixer.cda[1]>>1) & 0xe);
		case 0x0a:      /* Mic Level (SBPRO) or Voice (SB2 Only) */
			if (type==SBT_2) return (mixer.dac[0]>>2);
			else return ((mixer.mic >> 2) & (type==SBT_16 ? 7:6));
		case 0x0e:      /* Output/Stereo Select */
			return 0x11|(mixer.stereo ? 0x02 : 0x00)|(mixer.filter_bypass ? 0x20 : 0x00);
		case 0x14:      /* Audio 1 Play Volume (ESS 688) */
			if (ess_type != ESS_NONE) return ((mixer.dac[0] << 3) & 0xF0) + (mixer.dac[1] >> 1);
			break;
		case 0x26:      /* FM Volume (SBPRO) */
			return MAKEPROVOL(mixer.fm);
		case 0x28:      /* CD Audio Volume (SBPRO) */
			return MAKEPROVOL(mixer.cda);
		case 0x2e:      /* Line-IN Volume (SBPRO) */
			return MAKEPROVOL(mixer.lin);
		case 0x30:      /* Master Volume Left (SB16) */
			if (type==SBT_16) return mixer.master[0]<<3;
			ret=0xa;
			break;
		case 0x31:      /* Master Volume Right (S16) */
			if (type==SBT_16) return mixer.master[1]<<3;
			ret=0xa;
			break;
		case 0x32:      /* DAC Volume Left (SB16) */
			/* Master Volume (ESS 688) */
			if (type==SBT_16) return mixer.dac[0]<<3;
			if (ess_type != ESS_NONE) return ((mixer.master[0] << 3) & 0xF0) + (mixer.master[1] >> 1);
			ret=0xa;
			break;
		case 0x33:      /* DAC Volume Right (SB16) */
			if (type==SBT_16) return mixer.dac[1]<<3;
			ret=0xa;
			break;
		case 0x34:      /* FM Volume Left (SB16) */
			if (type==SBT_16) return mixer.fm[0]<<3;
			ret=0xa;
			break;
		case 0x35:      /* FM Volume Right (SB16) */
			if (type==SBT_16) return mixer.fm[1]<<3;
			ret=0xa;
			break;
		case 0x36:      /* CD Volume Left (SB16) */
			/* FM Volume (ESS 688) */
			if (type==SBT_16) return mixer.cda[0]<<3;
			if (ess_type != ESS_NONE) return ((mixer.fm[0] << 3) & 0xF0) + (mixer.fm[1] >> 1);
			ret=0xa;
			break;
		case 0x37:      /* CD Volume Right (SB16) */
			if (type==SBT_16) return mixer.cda[1]<<3;
			ret=0xa;
			break;
		case 0x38:      /* Line-in Volume Left (SB16) */
			/* AuxA (CD) Volume Register (ESS 688) */
			if (type==SBT_16) return mixer.lin[0]<<3;
			if (ess_type != ESS_NONE) return ((mixer.cda[0] << 3) & 0xF0) + (mixer.cda[1] >> 1);
			ret=0xa;
			break;
		case 0x39:      /* Line-in Volume Right (SB16) */
			if (type==SBT_16) return mixer.lin[1]<<3;
			ret=0xa;
			break;
		case 0x3a:      /* Mic Volume (SB16) */
			if (type==SBT_16) return mixer.mic<<3;
			ret=0xa;
			break;
		case 0x3e:      /* Line Volume (ESS 688) */
			if (ess_type != ESS_NONE) return ((mixer.lin[0] << 3) & 0xF0) + (mixer.lin[1] >> 1);
			break;
		case 0x40:      /* ESS identification value (ES1488 and later) */
			if (ess_type != ESS_NONE) {
				switch (ess_type) {
					case ESS_688:
						ret=0xa;
						break;
					case ESS_1688:
						ret=mixer.ess_id_str[mixer.ess_id_str_pos];
						mixer.ess_id_str_pos++;
						if (mixer.ess_id_str_pos >= 4) {
							mixer.ess_id_str_pos = 0;
						}
						break;
					default:
						ret=0xa;
						LOG(LOG_SB,LOG_WARN)("MIXER:FIXME:ESS identification function (0x%X) for selected card is not implemented",mixer.index);
				}
			} else {
				if (type == SBT_16) {
					ret=mixer.unhandled[mixer.index];
				} else {
					ret=0xa;
				}
				LOG(LOG_SB,LOG_WARN)("MIXER:Read from unhandled index %X",mixer.index);
			}
			break;
		case 0x80:      /* IRQ Select */
			ret=0;
			if (IS_PC98_ARCH) {
				switch (hw.irq) {
					case 3:  return 0xF1; // upper 4 bits always 1111
					case 10: return 0xF2;
					case 12: return 0xF4;
					case 5:  return 0xF8;
				}
			}
			else {
				switch (hw.irq) {
					case 2:  return 0x1;
					case 5:  return 0x2;
					case 7:  return 0x4;
					case 10: return 0x8;
				}
			}
			break;
		case 0x81:      /* DMA Select */
			ret=0;
			if (IS_PC98_ARCH) {
				switch (hw.dma8) {
					case 0:ret|=0x1;break;
					case 3:ret|=0x2;break;
				}

				// there's some strange behavior on the PC-98 version of the card
				ret |= (pc98_mixctl_reg & (~3u));
			}
			else {
				switch (hw.dma8) {
					case 0:ret|=0x1;break;
					case 1:ret|=0x2;break;
					case 3:ret|=0x8;break;
				}
				switch (hw.dma16) {
					case 5:ret|=0x20;break;
					case 6:ret|=0x40;break;
					case 7:ret|=0x80;break;
				}
			}
			return ret;
		case 0x82:      /* IRQ Status */
			return  (irq.pending_8bit ? 0x1 : 0) |
				(irq.pending_16bit ? 0x2 : 0) |
				((type == SBT_16) ? 0x20 : 0);
		default:
			if (    ((type == SBT_PRO1 || type == SBT_PRO2) && mixer.index==0x0c) || /* Input control on SBPro */
				(type == SBT_16 && mixer.index >= 0x3b && mixer.index <= 0x47)) /* New SB16 registers */
				ret = mixer.unhandled[mixer.index];
			else
				ret=0xa;
			LOG(LOG_SB,LOG_WARN)("MIXER:Read from unhandled index %X",mixer.index);
	}

	return ret;
}

std::string SB_INFO::GetSBtype() {
	switch (type) {
		case SBT_NONE:
			return "None";
		case SBT_1:
			return "SB1";
		case SBT_PRO1:
			return "SBPro";
		case SBT_2:
			return "SB2";
		case SBT_PRO2:
			return "SBPro 2";
		case SBT_16:
			return "SB16";
		case SBT_GB:
			return "GB";
		default:
			return "Unknown";
	}
}

Bitu SB_INFO::read_sb(Bitu port,Bitu /*iolen*/) {
	if (!IS_PC98_ARCH) {
		/* All Creative hardware prior to Sound Blaster 16 appear to alias most of the I/O ports.
		 * This has been confirmed on a Sound Blaster 2.0 and a Sound Blaster Pro (v3.1).
		 * DSP aliasing is also faithfully emulated by the ESS AudioDrive. */
		if (hw.sb_io_alias) {
			if ((port-hw.base) == DSP_ACK_16BIT && ess_type != ESS_NONE)
				{ } /* ESS AudioDrive does not alias DSP STATUS (0x22E) as seen on real hardware */
			else if ((port-hw.base) < MIXER_INDEX || (port-hw.base) > MIXER_DATA)
				port &= ~1u;
		}
	}

	switch (((port-hw.base) >> (IS_PC98_ARCH ? 8u : 0u)) & 0xFu) {
		case MIXER_INDEX:
			return mixer.index;
		case MIXER_DATA:
			return CTMIXER_Read();
		case DSP_READ_DATA:
			return DSP_ReadData();
		case DSP_READ_STATUS:
			//TODO See for high speed dma :)
			if (irq.pending_8bit)  {
				irq.pending_8bit=false;
				PIC_DeActivateIRQ(hw.irq);
			}

			if (mode == MODE_DMA_REQUIRE_IRQ_ACK) {
				chan->FillUp();
				mode = MODE_DMA;
			}

			extern const char* RunningProgram; // Wengier: Hack for Desert Strike & Jungle Strike
			if (!IS_PC98_ARCH && port>0x220 && port%0x10==0xE && !dsp.out.used && (!strcmp(RunningProgram, "DESERT") || !strcmp(RunningProgram, "JUNGLE"))) {
				LOG_MSG("Check status by game: %s\n", RunningProgram);
				dsp.out.used++;
			}
			if (ess_type == ESS_NONE && (type == SBT_1 || type == SBT_2 || type == SBT_PRO1 || type == SBT_PRO2))
				return dsp.out.used ? 0xAA : 0x2A; /* observed return values on SB 2.0---any significance? */
			else
				return dsp.out.used ? 0xFF : 0x7F; /* normal return values */
		case DSP_ACK_16BIT:
			if (ess_type == ESS_NONE && type == SBT_16) {
				if (irq.pending_16bit)  {
					irq.pending_16bit=false;
					PIC_DeActivateIRQ(hw.irq);
				}

				if (mode == MODE_DMA_REQUIRE_IRQ_ACK) {
					chan->FillUp();
					mode = MODE_DMA;
				}
			}
			break;
		case DSP_WRITE_STATUS:
			switch (dsp.state) {
				/* FIXME: On a SB 2.0 card I own, the port will usually read either 0x2A or 0xAA,
				 *        rather than 0x7F or 0xFF. Is there any significance to that? */
				case DSP_S_NORMAL: {
					bool busy = false;

					/* NTS: DSP "busy cycle" is independent of whether the DSP is actually
					 *      busy (executing a command) or highspeed mode. On SB16 hardware,
					 *      writing a DSP command during the busy cycle means that the command
					 *      is remembered, but not acted on until the DSP leaves its busy
					 *      cycle. */
					busy_cycle_io_hack++; /* NTS: busy cycle I/O timing hack! */
					if (DSP_busy_cycle())
						busy = true;
					else if (dsp.write_busy || (dsp.highspeed && type != SBT_16 && ess_type == ESS_NONE && reveal_sc_type == RSC_NONE))
						busy = true;

					if (!write_status_must_return_7f && ess_type == ESS_NONE && (type == SBT_2 || type == SBT_PRO1 || type == SBT_PRO2))
						return busy ? 0xAA : 0x2A; /* observed return values on SB 2.0---any significance? */
					else
						return busy ? 0xFF : 0x7F; /* normal return values */

				}
				case DSP_S_RESET:
				case DSP_S_RESET_WAIT:
					return 0xff;
			}
			return 0xff;
		case DSP_RESET:
			return 0xff;
		default:
			LOG(LOG_SB,LOG_NORMAL)("Unhandled read from SB Port %4X",(int)port);
			break;
	}
	return 0xff;
}

void SB_INFO::write_sb(Bitu port,Bitu val,Bitu /*iolen*/) {
	/* All Creative hardware prior to Sound Blaster 16 appear to alias most of the I/O ports.
	 * This has been confirmed on a Sound Blaster 2.0 and a Sound Blaster Pro (v3.1).
	 * DSP aliasing is also faithfully emulated by the ESS AudioDrive. */
	if (!IS_PC98_ARCH) {
		if (hw.sb_io_alias) {
			if ((port-hw.base) == DSP_ACK_16BIT && ess_type != ESS_NONE)
				{ } /* ESS AudioDrive does not alias DSP STATUS (0x22E) as seen on real hardware */
			else if ((port-hw.base) < MIXER_INDEX || (port-hw.base) > MIXER_DATA)
				port &= ~1u;
		}
	}

	uint8_t val8=(uint8_t)(val&0xff);
	switch (((port-hw.base) >> (IS_PC98_ARCH ? 8u : 0u)) & 0xFu) {
		case DSP_RESET:
			DSP_DoReset(val8);
			break;
		case DSP_WRITE_DATA:
			/* FIXME: We need to emulate behavior where either the DSP command is delayed (busy cycle)
			 *        and then acted on, or we need to emulate the DSP ignoring the byte because a
			 *        command is in progress */
			DSP_DoWrite(val8);
			break;
		case MIXER_INDEX:
			mixer.index=val8;
			if (mixer.index == 0x40 && ess_type != ESS_NONE) {
				mixer.ess_id_str_pos = 0;
			}
			break;
		case MIXER_DATA:
			CTMIXER_Write(val8);
			break;
		default:
			LOG(LOG_SB,LOG_NORMAL)("Unhandled write to SB Port %4X",(int)port);
			break;
	}
}

static SB_INFO sb[MAX_CARDS];

static void adlib_gusforward(Bitu /*port*/,Bitu val,Bitu /*iolen*/) {
	adlib_commandreg=(uint8_t)(val&0xff);
}

bool SB_Get_Address(Bitu& sbaddr, Bitu& sbirq, Bitu& sbdma) {
	const size_t ci = 0;
	sbaddr=0;
	sbirq =0;
	sbdma =0;
	if (sb[ci].type == SBT_NONE) return false;
	else {
		sbaddr=sb[ci].hw.base;
		sbirq =sb[ci].hw.irq;
		sbdma =sb[ci].hw.dma8;
		return true;
	}
}

static void SBLASTER_CallBack(const size_t ci,Bitu len) {
	pic_tickindex_t now = PIC_FullIndex();

	if (now >= sb[ci].next_check_record_settings) {
		sb[ci].next_check_record_settings = now + 100/*ms*/;
		sb[ci].sb_update_recording_source_settings();
	}

	sb[ci].directdac_warn_speaker_off = true;

	switch (sb[ci].mode) {
		case MODE_NONE:
		case MODE_DMA_PAUSE:
		case MODE_DMA_MASKED:
		case MODE_DMA_REQUIRE_IRQ_ACK:
			sb[ci].chan->AddSilence();
			break;
		case MODE_DAC:
			sb[ci].mode = MODE_NONE;
			break;
		case MODE_DMA:
			len*=sb[ci].dma.mul;
			if (len&SB_SH_MASK) len+=1 << SB_SH;
			len>>=SB_SH;
			if (len>sb[ci].dma.left) len=sb[ci].dma.left;
			sb[ci].GenerateDMASound(len);
			break;
	}
}

#define ISAPNP_COMPATIBLE(id) \
    ISAPNP_SMALL_TAG(3,4), \
    ( (id)        & 0xFF), \
    (((id) >>  8) & 0xFF), \
    (((id) >> 16) & 0xFF), \
    (((id) >> 24) & 0xFF)

#if 0
static const unsigned char ViBRA_sysdev[] = {
    ISAPNP_IO_RANGE(
            0x01,                   /* decodes 16-bit ISA addr */
            0x220,0x280,                /* min-max range I/O port */
            0x20,0x10),             /* align=0x20 length=0x10 */
    ISAPNP_IO_RANGE(
            0x01,                   /* decodes 16-bit ISA addr */
            0x300,0x330,                /* min-max range I/O port */
            0x10,0x4),              /* align=0x10 length=0x4 */
    ISAPNP_IO_RANGE(
            0x01,                   /* decodes 16-bit ISA addr */
            0x388,0x38C,                /* min-max range I/O port */
            0x4,0x4),               /* align=0x4 length=0x4 */
    ISAPNP_IRQ(
            (1 << 5) |
            (1 << 7) |
            (1 << 9) |
            (1 << 10) |
            (1 << 11),              /* IRQ 5,7,9,10,11 */
            0x09),                  /* HTE=1 LTL=1 */
    ISAPNP_DMA(
            (1 << 0) |
            (1 << 1) |
            (1 << 3),               /* DMA 0,1,3 */
            0x01),                  /* 8/16-bit */
    ISAPNP_DMA(
            (1 << 0) |
            (1 << 1) |
            (1 << 3) |
            (1 << 5) |
            (1 << 6) |
            (1 << 7),               /* DMA 0,1,3,5,6,7 */
            0x01),                  /* 8/16-bit */
    ISAPNP_COMPATIBLE(ISAPNP_ID('C','T','L',0x0,0x0,0x0,0x1)),  /* <- Windows 95 doesn't know CTL0070 but does know CTL0001, this hack causes Win95 to offer it's internal driver as an option */
    ISAPNP_END
};
#endif

class ViBRA_PnP : public ISAPnPDevice {
	public:
		size_t ci = 0;
		ViBRA_PnP(const size_t n_ci) : ISAPnPDevice() {
			resource_ident = 0;
			host_writed(ident+0,ISAPNP_ID('C','T','L',0x0,0x0,0x7,0x0)); /* CTL0070: ViBRA C */
			host_writed(ident+4,0xFFFFFFFFUL);
			checksum_ident();

			ci = n_ci;
			alloc(256 - 9/*ident*/); // Real ViBRA hardware acts as if PNP data is read from a 256-byte ROM

			// this template taken from a real Creative ViBRA16C card
			begin_write_res();
			write_ISAPnP_version(/*version*/1,0,/*vendor*/0x10);
			write_Identifier_String("Creative ViBRA16C PnP");

			write_Logical_Device_ID('C','T','L',0x0,0x0,0x0,0x1); // CTL0001
			write_Identifier_String("Audio");

			write_Dependent_Function_Start(ISAPnPDevice::DependentFunctionConfig::PreferredDependentConfiguration);
			write_IRQ_Format(
					ISAPnPDevice::irq2mask(5));
			write_DMA_Format(
					ISAPnPDevice::dma2mask(1),
					DMATransferType_8bitOnly,
					false,/*not a bus master*/
					true,/*byte mode */
					false,/*word mode*/
					DMASpeedSupported_Compat);
			write_DMA_Format(
					ISAPnPDevice::dma2mask(5),
					DMATransferType_16bitOnly,
					false,/*not a bus master*/
					false,/*byte mode */
					true,/*word mode*/
					DMASpeedSupported_Compat);
			write_IO_Port(/*min*/0x220,/*max*/0x220,/*count*/0x10,/*align*/0x01);
			if (ci == 0) {
				write_IO_Port(/*min*/0x330,/*max*/0x330,/*count*/0x02,/*align*/0x01);
				write_IO_Port(/*min*/0x388,/*max*/0x388,/*count*/0x04,/*align*/0x01);
			}

			write_Dependent_Function_Start(ISAPnPDevice::DependentFunctionConfig::AcceptableDependentConfiguration,true);
			write_IRQ_Format(
					ISAPnPDevice::irq2mask(5,7,9,10));
			write_DMA_Format(
					ISAPnPDevice::dma2mask(1,3),
					DMATransferType_8bitOnly,
					false,/*not a bus master*/
					true,/*byte mode */
					false,/*word mode*/
					DMASpeedSupported_Compat);
			write_DMA_Format(
					ISAPnPDevice::dma2mask(5,7),
					DMATransferType_16bitOnly,
					false,/*not a bus master*/
					false,/*byte mode */
					true,/*word mode*/
					DMASpeedSupported_Compat);
			write_IO_Port(/*min*/0x220,/*max*/0x280,/*count*/0x10,/*align*/0x20);
			if (ci == 0) {
				write_IO_Port(/*min*/0x300,/*max*/0x330,/*count*/0x02,/*align*/0x30);
				write_IO_Port(/*min*/0x388,/*max*/0x388,/*count*/0x04,/*align*/0x01);
			}

			write_Dependent_Function_Start(ISAPnPDevice::DependentFunctionConfig::AcceptableDependentConfiguration,true);
			write_IRQ_Format(
					ISAPnPDevice::irq2mask(5,7,9,10));
			write_DMA_Format(
					ISAPnPDevice::dma2mask(1,3),
					DMATransferType_8bitOnly,
					false,/*not a bus master*/
					true,/*byte mode */
					false,/*word mode*/
					DMASpeedSupported_Compat);
			write_DMA_Format(
					ISAPnPDevice::dma2mask(5,7),
					DMATransferType_16bitOnly,
					false,/*not a bus master*/
					false,/*byte mode */
					true,/*word mode*/
					DMASpeedSupported_Compat);
			write_IO_Port(/*min*/0x220,/*max*/0x280,/*count*/0x10,/*align*/0x20);
			if (ci == 0) {
				write_IO_Port(/*min*/0x300,/*max*/0x330,/*count*/0x02,/*align*/0x30);
			}

			write_Dependent_Function_Start(ISAPnPDevice::DependentFunctionConfig::SubOptimalDependentConfiguration);
			write_IRQ_Format(
					ISAPnPDevice::irq2mask(5,7,9,10));
			write_DMA_Format(
					ISAPnPDevice::dma2mask(1,3),
					DMATransferType_8bitOnly,
					false,/*not a bus master*/
					true,/*byte mode */
					false,/*word mode*/
					DMASpeedSupported_Compat);
			write_DMA_Format(
					ISAPnPDevice::dma2mask(5,7),
					DMATransferType_16bitOnly,
					false,/*not a bus master*/
					false,/*byte mode */
					true,/*word mode*/
					DMASpeedSupported_Compat);
			write_IO_Port(/*min*/0x220,/*max*/0x280,/*count*/0x10,/*align*/0x20);

			write_Dependent_Function_Start(ISAPnPDevice::DependentFunctionConfig::SubOptimalDependentConfiguration);
			write_IRQ_Format(
					ISAPnPDevice::irq2mask(5,7,9,10));
			write_DMA_Format(
					ISAPnPDevice::dma2mask(1,3),
					DMATransferType_8bitOnly,
					false,/*not a bus master*/
					true,/*byte mode */
					false,/*word mode*/
					DMASpeedSupported_Compat);
			write_IO_Port(/*min*/0x220,/*max*/0x280,/*count*/0x10,/*align*/0x20);
			if (ci == 0) {
				write_IO_Port(/*min*/0x300,/*max*/0x330,/*count*/0x02,/*align*/0x30);
				write_IO_Port(/*min*/0x388,/*max*/0x388,/*count*/0x04,/*align*/0x01);
			}

			write_Dependent_Function_Start(ISAPnPDevice::DependentFunctionConfig::SubOptimalDependentConfiguration);
			write_IRQ_Format(
					ISAPnPDevice::irq2mask(5,7,9,10));
			write_DMA_Format(
					ISAPnPDevice::dma2mask(1,3),
					DMATransferType_8bitOnly,
					false,/*not a bus master*/
					true,/*byte mode */
					false,/*word mode*/
					DMASpeedSupported_Compat);
			write_IO_Port(/*min*/0x220,/*max*/0x280,/*count*/0x10,/*align*/0x20);

			write_End_Dependent_Functions();

			if (ci == 0) {
				// NTS: DOSBox-X as coded now always has a joystick port at 0x201 even if no joystick
				write_Logical_Device_ID('C','T','L',0x7,0x0,0x0,0x1); // CTL7001
				write_Compatible_Device_ID('P','N','P',0xB,0x0,0x2,0xF); // PNPB02F
				write_Identifier_String("Game");
				write_IO_Port(/*min*/0x200,/*max*/0x200,/*count*/0x08);
			}

			end_write_res();        // END
		}
		void select_logical_device(Bitu val) override {
			logical_device = val;
		}
		uint8_t read(Bitu addr) override {
			uint8_t ret = 0xFF;
			if (logical_device == 0) {
				switch (addr) {
					case 0x60: case 0x61:   /* I/O [0] sound blaster */
						ret = sb[ci].hw.base >> ((addr & 1) ? 0 : 8);
						break;
					case 0x62: case 0x63:   /* I/O [1] MPU */
						ret = 0x330 >> ((addr & 1) ? 0 : 8); /* FIXME: What I/O port really IS the MPU on? */
						break;
					case 0x64: case 0x65:   /* I/O [1] OPL-3 */
						ret = 0x388 >> ((addr & 1) ? 0 : 8); /* FIXME */
						break;
					case 0x70: /* IRQ[0] */
						ret = sb[ci].hw.irq;
						break;
						/* TODO: 0x71 IRQ mode */
					case 0x74: /* DMA[0] */
						ret = sb[ci].hw.dma8;
						break;
					case 0x75: /* DMA[1] */
						ret = sb[ci].hw.dma16 == 0xFF ? sb[ci].hw.dma8 : sb[ci].hw.dma16;
						break;

				}
			}
			else if (logical_device == 1) {
				switch (addr) {
					case 0x60: case 0x61:   /* I/O [0] gameport */
						ret = 0x200 >> ((addr & 1) ? 0 : 8);
						break;
				}
			}

			return ret;
		}
		void write(Bitu addr,Bitu val) override {
			if (logical_device == 0) {
				switch (addr) {
					case 0x30:  /* activate range */
						/* TODO: set flag to ignore writes/return 0xFF when "deactivated". setting bit 0 activates, clearing deactivates */
						break;
					case 0x60: case 0x61:   /* I/O [0] sound blaster */
						/* TODO: on-the-fly changing */
						//LOG_MSG("ISA PnP Warning: Sound Blaster I/O port changing not implemented yet\n");
						break;
					case 0x62: case 0x63:   /* I/O [1] MPU */
						/* TODO: on-the-fly changing */
						//LOG_MSG("ISA PnP Warning: MPU I/O port changing not implemented yet\n");
						break;
					case 0x64: case 0x65:   /* I/O [1] OPL-3 */
						/* TODO: on-the-fly changing */
						//LOG_MSG("ISA PnP Warning: OPL-3 I/O port changing not implemented yet\n");
						break;
					case 0x70: /* IRQ[0] */
						if (val & 0xF)
							sb[ci].hw.irq = val;
						else
							sb[ci].hw.irq = 0xFF;
						break;
					case 0x74: /* DMA[0] */
						if ((val & 7) == 4)
							sb[ci].hw.dma8 = 0xFF;
						else
							sb[ci].hw.dma8 = val & 7;
						break;
					case 0x75: /* DMA[1] */
						if ((val & 7) == 4)
							sb[ci].hw.dma16 = 0xFF;
						else
							sb[ci].hw.dma16 = val & 7;
						break;

				}
			}
			else if (logical_device == 1) {
				switch (addr) {
					case 0x60: case 0x61:   /* I/O [0] gameport */
						/* TODO: on-the-fly changing */
						//LOG_MSG("ISA PnP Warning: Gameport I/O port changing not implemented yet\n");
						break;
				}
			}
		}
};

std::string GetSBtype() {
	const size_t ci = 0;
	return sb[ci].GetSBtype();
}

std::string GetSBbase() {
	const size_t ci = 0;
	std::stringstream ss;
	ss << std::hex << sb[ci].hw.base;
	return ss.str();
}

std::string GetSBirq() {
	const size_t ci = 0;
	return sb[ci].hw.irq==0xff?"None":std::to_string(sb[ci].hw.irq);
}

std::string GetSBldma() {
	const size_t ci = 0;
	return sb[ci].hw.dma8==0xff?"None":std::to_string((int)sb[ci].hw.dma8);
}

std::string GetSBhdma() {
	const size_t ci = 0;
	return sb[ci].hw.dma16==0xff?"None":std::to_string((int)sb[ci].hw.dma16);
}

static void DSP_DMA_CallBack(DmaChannel * chan, DMAEvent event) {
	const size_t ci = chan->userData;
	assert(ci < MAX_CARDS);
	if (chan!=sb[ci].dma.chan || event==DMA_REACHED_TC) return;
	else if (event==DMA_READ_COUNTER) {
		sb[ci].chan->FillUp();
	}
	else if (event==DMA_MASKED) {
		if (sb[ci].mode==MODE_DMA) {
			//Catch up to current time, but don't generate an IRQ!
			//Fixes problems with later sci games.
			double t = PIC_FullIndex() - sb[ci].last_dma_callback;
			Bitu s = static_cast<Bitu>(sb[ci].dma.rate * t / 1000.0f);
			if (s > sb[ci].dma.min) {
				LOG(LOG_SB,LOG_NORMAL)("limiting amount masked to sb[ci].dma.min");
				s = sb[ci].dma.min;
			}
			Bitu min_size = sb[ci].dma.mul >> SB_SH;
			if (!min_size) min_size = 1;
			min_size *= 2;
			if (sb[ci].dma.left > min_size) {
				if (s > (sb[ci].dma.left - min_size)) s = sb[ci].dma.left - min_size;
				//This will trigger an irq, see GenerateDMASound, so let's not do that
				if (!sb[ci].dma.autoinit && sb[ci].dma.left <= sb[ci].dma.min) s = 0;
				if (s) sb[ci].GenerateDMASound(s);
			}
			sb[ci].mode = MODE_DMA_MASKED;
			LOG(LOG_SB,LOG_NORMAL)("DMA masked, stopping %s, dsp left %d, dma left %d, by %s",sb[ci].dma.recording?"input":"output",(unsigned int)sb[ci].dma.left,chan->currcnt+1,DMAActorStr(chan->masked_by));
		}
	} else if (event==DMA_UNMASKED) {
		if (sb[ci].mode==MODE_DMA_MASKED && sb[ci].dma.mode!=DSP_DMA_NONE) {
			sb[ci].DSP_ChangeMode(MODE_DMA);
			sb[ci].CheckDMAEnd();
			LOG(LOG_SB,LOG_NORMAL)("DMA unmasked, starting %s, auto %d dma left %d, by %s dma init count %d",sb[ci].dma.recording?"input":"output",chan->autoinit,chan->currcnt+1,DMAActorStr(chan->masked_by),chan->basecnt+1);
		}
	}
}

static void DMA_Silent_Event(Bitu val) {
	const size_t ci = (size_t)(val >> (Bitu)CARD_INDEX_BIT); val &= (1u << CARD_INDEX_BIT) - 1u;
	assert(ci < MAX_CARDS);
	if (sb[ci].dma.left<val) val=sb[ci].dma.left;
	if (sb[ci].dma.recording) sb[ci].gen_input(val,sb[ci].dma.buf.b8);
	Bitu read =
		(sb[ci].dma.chan != NULL) ?
			(sb[ci].dma.recording ?
				sb[ci].dma.chan->Write(val,sb[ci].dma.buf.b8) :
				sb[ci].dma.chan->Read(val,sb[ci].dma.buf.b8)
			) :
			0;
	sb[ci].dma.left-=read;
	if (!sb[ci].dma.left) {
		if (sb[ci].dma.mode >= DSP_DMA_16) sb[ci].SB_RaiseIRQ(SB_IRQ_16);
		else sb[ci].SB_RaiseIRQ(SB_IRQ_8);
		if (sb[ci].dma.autoinit) sb[ci].dma.left=sb[ci].dma.total;
		else {
			sb[ci].mode=MODE_NONE;
			sb[ci].dma.mode=sb[ci].dma.mode_assigned=DSP_DMA_NONE;
		}
	}
	if (sb[ci].dma.left) {
		Bitu bigger=(sb[ci].dma.left > sb[ci].dma.min) ? sb[ci].dma.min : sb[ci].dma.left;
		float delay=(bigger*1000.0f)/sb[ci].dma.rate;
		PIC_AddEvent(DMA_Silent_Event,delay,bigger | (ci << CARD_INDEX_BIT));
	}
}

static void DMA_DAC_Event(Bitu val) {
	const size_t ci = (size_t)(val >> (Bitu)CARD_INDEX_BIT); val &= (1u << CARD_INDEX_BIT) - 1u;
	assert(ci < MAX_CARDS);
	unsigned char tmp[4];
	Bitu read,expct;
	signed int L,R;
	int16_t out[2];

	if (sb[ci].dma.chan->masked) {
		PIC_AddEvent(DMA_DAC_Event,1000.0 / sb[ci].dma_dac_srcrate,(ci << CARD_INDEX_BIT));
		return;
	}
	if (!sb[ci].dma.left)
		return;

	const bool psingle_sample = sb[ci].single_sample_dma;
	/* Fix for 1994 Demoscene entry myth_dw: The demo's Sound Blaster Pro initialization will start DMA with
	 * count == 1 or 2 (triggering Goldplay mode) but will change the DMA initial counter when it begins
	 * normal playback. If goldplay stereo hack is enabled and we do not catch this case, the first 0.5 seconds
	 * of music will play twice as fast. */
	if (sb[ci].dma.chan != NULL &&
		sb[ci].dma.chan->basecnt < ((sb[ci].dma.mode==DSP_DMA_16_ALIASED?2:1)*((sb[ci].dma.stereo || sb[ci].mixer.sbpro_stereo)?2:1))/*size of one sample in DMA counts*/)
		sb[ci].single_sample_dma = 1;
	else
		sb[ci].single_sample_dma = 0;

	if (psingle_sample && !sb[ci].single_sample_dma) {
		// WARNING: This assumes Sound Blaster Pro emulation!
		LOG(LOG_SB,LOG_NORMAL)("Goldplay mode unexpectedly switched off, normal DMA playback follows");
		sb[ci].dma_dac_mode = 0;
		sb[ci].dma_dac_srcrate = sb[ci].freq / (sb[ci].mixer.stereo ? 2 : 1);
		sb[ci].chan->SetFreq(sb[ci].dma_dac_srcrate);
		sb[ci].updateSoundBlasterFilter(sb[ci].dma_dac_srcrate);
		return;
	}

	/* NTS: chan->Read() deals with DMA unit transfers.
	 *      for 8-bit DMA, read/expct is in bytes, for 16-bit DMA, read/expct is in 16-bit words */
	expct = (sb[ci].dma.stereo ? 2u : 1u) * (sb[ci].dma.mode == DSP_DMA_16_ALIASED ? 2u : 1u);
	if (sb[ci].dma.recording) {
		sb[ci].gen_input(expct,tmp);
		read = sb[ci].dma.chan->Write(expct,tmp);
		L = R = 0;
	}
	else {
		read = sb[ci].dma.chan->Read(expct,tmp);
		//if (read != expct)
		//      LOG_MSG("DMA read was not sample aligned. Sound may swap channels or become static. On real hardware the same may happen unless audio is prepared specifically.\n");

		if (sb[ci].dma.mode == DSP_DMA_16 || sb[ci].dma.mode == DSP_DMA_16_ALIASED) {
			L = (int16_t)host_readw(&tmp[0]);
			if (!sb[ci].dma.sign) L ^= 0x8000;
			if (sb[ci].dma.stereo) {
				R = (int16_t)host_readw(&tmp[2]);
				if (!sb[ci].dma.sign) R ^= 0x8000;
			}
			else {
				R = L;
			}
		}
		else {
			L = tmp[0];
			if (!sb[ci].dma.sign) L ^= 0x80;
			L = (int16_t)(L << 8);
			if (sb[ci].dma.stereo) {
				R = tmp[1];
				if (!sb[ci].dma.sign) R ^= 0x80;
				R = (int16_t)(R << 8);
			}
			else {
				R = L;
			}
		}
	}

	if (sb[ci].dma.stereo) {
		out[0]=L;
		out[1]=R;
		sb[ci].chan->AddSamples_s16(1,out);
	}
	else {
		out[0]=L;
		sb[ci].chan->AddSamples_m16(1,out);
	}

	/* NTS: The reason we check this is that sometimes the various "checks" performed by
	   -        *      setup/configuration tools will setup impossible playback scenarios to test
	   -        *      the card that would result in read > sb[ci].dma.left. If read > sb[ci].dma.left then
	   -        *      the subtraction below would drive sb[ci].dma.left below zero and the IRQ would
	   -        *      never fire, and the test program would fail to detect SB16 emulation.
	   -        *
	   -        *      Bugfix for "Extreme Assault" that allows the game to detect Sound Blaster 16
	   -        *      hardware. "Extreme Assault"'s SB16 test appears to configure a DMA transfer
	   -        *      of 1 byte then attempt to play 16-bit signed stereo PCM (4 bytes) which prior
	   -        *      to this fix would falsely trigger Goldplay then cause sb[ci].dma.left to underrun
	   -        *      and fail to fire the IRQ. */
	if (sb[ci].dma.left >= read)
		sb[ci].dma.left -= read;
	else
		sb[ci].dma.left = 0;

	if (!sb[ci].dma.left) {
		sb[ci].SB_OnEndOfDMA();
		if (sb[ci].dma_dac_mode) PIC_AddEvent(DMA_DAC_Event,1000.0 / sb[ci].dma_dac_srcrate,(ci << CARD_INDEX_BIT));
	}
	else {
		PIC_AddEvent(DMA_DAC_Event,1000.0 / sb[ci].dma_dac_srcrate,(ci << CARD_INDEX_BIT));
	}
}

static void END_DMA_Event(Bitu val) {
	const size_t ci = (size_t)(val >> (Bitu)CARD_INDEX_BIT); val &= (1u << CARD_INDEX_BIT) - 1u;
	assert(ci < MAX_CARDS);
	sb[ci].GenerateDMASound(val);
}

static void DSP_RaiseIRQEvent(Bitu val) {
	const size_t ci = (size_t)(val >> (Bitu)CARD_INDEX_BIT); val &= (1u << CARD_INDEX_BIT) - 1u;
	assert(ci < MAX_CARDS);
	sb[ci].SB_RaiseIRQ(SB_IRQ_8);
}

static void DSP_BusyComplete(Bitu val) {
	const size_t ci = (size_t)(val >> (Bitu)CARD_INDEX_BIT); val &= (1u << CARD_INDEX_BIT) - 1u;
	assert(ci < MAX_CARDS);
	sb[ci].dsp.write_busy = 0;
}

static void DSP_FinishReset(Bitu val) {
	const size_t ci = (size_t)(val >> (Bitu)CARD_INDEX_BIT); val &= (1u << CARD_INDEX_BIT) - 1u;
	assert(ci < MAX_CARDS);
	sb[ci].DSP_FlushData();
	sb[ci].DSP_AddData(0xaa);
	sb[ci].dsp.state=DSP_S_NORMAL;
}

static void DSP_E2_DMA_CallBack(DmaChannel *chan, DMAEvent event) {
	const size_t ci = (size_t)chan->userData;
	assert(ci < MAX_CARDS);
	if (event==DMA_UNMASKED) {
		uint8_t val = sb[ci].e2.valadd;
		chan->Register_Callback(nullptr);
		chan->Write(1,&val);
	}
}

static void DSP_SC400_E6_DMA_CallBack(DmaChannel *chan, DMAEvent event) {
	const size_t ci = (size_t)chan->userData;
	assert(ci < MAX_CARDS);
	if (event==DMA_UNMASKED) {
		static const char *string = "\x01\x02\x04\x08\x10\x20\x40\x80"; /* Confirmed response via DMA from actual Reveal SC400 card */
		LOG(LOG_SB,LOG_DEBUG)("SC400 returning DMA test pattern on DMA channel=%u",sb[ci].hw.dma8);
		chan->Register_Callback(nullptr);
		chan->Write(8,(uint8_t*)string);
		chan->Clear_Request();
		if (!chan->tcount) LOG(LOG_SB,LOG_DEBUG)("SC400 warning: DMA did not reach terminal count");
		sb[ci].SB_RaiseIRQ(SB_IRQ_8);
	}
}

static void DSP_ADC_CallBack(DmaChannel *chan, DMAEvent event) {
	const size_t ci = (size_t)chan->userData;
	assert(ci < MAX_CARDS);
	if (event!=DMA_UNMASKED) return;
	uint8_t val=128;

	while (sb[ci].dma.left--)
		chan->Write(1,&val);

	sb[ci].SB_RaiseIRQ(SB_IRQ_8);
	chan->Register_Callback(nullptr);
}

template <const size_t ci> static void SBLASTER_CallBack(Bitu len) {
	SBLASTER_CallBack(ci,len);
}

template <const size_t ci> static Bitu read_sb(Bitu port,Bitu iolen) {
	return sb[ci].read_sb(port,iolen);
}

template <const size_t ci> static void write_sb(Bitu port,Bitu val,Bitu iolen) {
	sb[ci].write_sb(port,val,iolen);
}

static const MIXER_Handler SBLASTER_CallBacks[MAX_CARDS] = {
	SBLASTER_CallBack<0>,
	SBLASTER_CallBack<1>
};

static const IO_ReadHandler * const read_sbs[MAX_CARDS] = {
	read_sb<0>,
	read_sb<1>
};

static const IO_WriteHandler * const write_sbs[MAX_CARDS] = {
	write_sb<0>,
	write_sb<1>
};

class SBLASTER: public Module_base {
	private:
		/* Data */
		IO_ReadHandleObject ReadHandler[0x10];
		IO_WriteHandleObject WriteHandler[0x10];
		AutoexecObject autoexecline;
		MixerObject MixerChan;
		OPL_Mode oplmode;
		size_t ci = 0;

		/* Support Functions */
		void Find_Type_And_Opl(Section_prop* config,SB_TYPES& type, OPL_Mode& opl_mode) const {
			sb[ci].vibra = false;
			sb[ci].ess_type = ESS_NONE;
			sb[ci].reveal_sc_type = RSC_NONE;
			sb[ci].ess_extended_mode = false;
			sb[ci].subtype = SBST_NONE;
			const char * sbtype=config->Get_string("sbtype");
			if (control->opt_silent) type = SBT_NONE;
			else if (!strcasecmp(sbtype,"sb1.0")) { type=SBT_1; sb[ci].subtype=SBST_100; }
			else if (!strcasecmp(sbtype,"sb1.5")) { type=SBT_1; sb[ci].subtype=SBST_105; }
			else if (!strcasecmp(sbtype,"sb1")) { type=SBT_1; sb[ci].subtype=SBST_105; } /* DOSBox SVN compat same as sb1.5 */
			else if (!strcasecmp(sbtype,"sb2.0")) { type=SBT_2; sb[ci].subtype=SBST_200; }
			else if (!strcasecmp(sbtype,"sb2.01")) { type=SBT_2; sb[ci].subtype=SBST_201; }
			else if (!strcasecmp(sbtype,"sb2")) { type=SBT_2; sb[ci].subtype=SBST_201; } /* DOSBox SVN compat same as sb2.01 */
			else if (!strcasecmp(sbtype,"sbpro1")) type=SBT_PRO1;
			else if (!strcasecmp(sbtype,"sbpro2")) type=SBT_PRO2;
			else if (!strcasecmp(sbtype,"sb16vibra")) type=SBT_16;
			else if (!strcasecmp(sbtype,"sb16")) type=SBT_16;
			else if (!strcasecmp(sbtype,"gb")) type=SBT_GB;
			else if (!strcasecmp(sbtype,"none")) type=SBT_NONE;
			else if (!strcasecmp(sbtype,"ess688")) {
				type=SBT_PRO2;
				sb[ci].ess_type=ESS_688;
				LOG(LOG_SB,LOG_DEBUG)("ESS 688 emulation enabled.");
				LOG(LOG_SB,LOG_WARN)("ESS 688 emulation is EXPERIMENTAL at this time and should not yet be used for normal gaming");
			}
			else if (!strcasecmp(sbtype,"reveal_sc400")) {
				type=SBT_PRO2;
				sb[ci].reveal_sc_type=RSC_SC400;
				LOG(LOG_SB,LOG_DEBUG)("Reveal SC400 emulation enabled.");
				LOG(LOG_SB,LOG_WARN)("Reveal SC400 emulation is EXPERIMENTAL at this time and should not yet be used for normal gaming.");
				LOG(LOG_SB,LOG_WARN)("Additional WARNING: This code only emulates the Sound Blaster portion of the card. Attempting to use the Windows Sound System part of the card (i.e. the Voyetra/SC400 Windows drivers) will not work!");
			}
			else if (!strcasecmp(sbtype,"ess1688")) {
				type=SBT_PRO2;
				sb[ci].ess_type=ESS_1688;
				sb[ci].mixer.ess_id_str[0] = 0x16;
				sb[ci].mixer.ess_id_str[1] = 0x88;
				LOG(LOG_SB,LOG_DEBUG)("ESS ES1688 emulation enabled.");
				LOG(LOG_SB,LOG_WARN)("ESS ES1688 emulation is EXPERIMENTAL at this time and should not yet be used for normal gaming.");
			}
			else type=SBT_16;

			if (type == SBT_16) {
				/* NTS: mainline DOSBox forces the type to SBT_PRO2 if !IS_EGAVGA_ARCH or no secondary DMA controller.
				 *      I'd rather take the approach that, if the user wants to emulate a bizarre unusual setup like
				 *      sticking a Sound Blaster 16 in an 8-bit machine, they are free to do so on the understanding
				 *      it won't work properly (and emulation will show what happens). I also believe that tying
				 *      8-bit vs 16-bit system type to the *video card* was a really dumb move. */
				if (!SecondDMAControllerAvailable()) {
					LOG(LOG_SB,LOG_WARN)("Sound Blaster 16 enabled on a system without 16-bit DMA. Don't expect this setup to work properly! To improve compatibility please edit your dosbox-x.conf and change sbtype to sbpro2 instead, or else enable the secondary DMA controller.");
				}
			}

			/* SB16 Vibra cards are Plug & Play */
			if (!IS_PC98_ARCH) {
				if (!strcasecmp(sbtype,"sb16vibra")) {
					ISA_PNP_devreg(new ViBRA_PnP(ci));
					sb[ci].vibra = true;
				}
			}

			/* OPL/CMS Init */
			const char * omode=config->Get_string("oplmode");
			if (ci != 0) opl_mode=OPL_none; // only the first card can have an OPL chip
			else if (!strcasecmp(omode,"none")) opl_mode=OPL_none;
			else if (!strcasecmp(omode,"cms")); // Skip for backward compatibility with existing configurations
			else if (!strcasecmp(omode,"opl2")) opl_mode=OPL_opl2;
			else if (!strcasecmp(omode,"dualopl2")) opl_mode=OPL_dualopl2;
			else if (!strcasecmp(omode,"opl3")) opl_mode=OPL_opl3;
			else if (!strcasecmp(omode,"opl3gold")) opl_mode=OPL_opl3gold;
			else if (!strcasecmp(omode,"hardware")) opl_mode=OPL_hardware;
			else if (!strcasecmp(omode,"hardwaregb")) opl_mode=OPL_hardwareCMS;
			else if (!strcasecmp(omode,"esfm")) opl_mode=OPL_esfm;
			/* Else assume auto */
			else {
				switch (type) {
					case SBT_NONE:
					case SBT_GB:
						opl_mode=OPL_none;
						break;
					case SBT_1:
					case SBT_2:
						opl_mode=OPL_opl2;
						break;
					case SBT_PRO1:
						opl_mode=OPL_dualopl2;
						break;
					case SBT_PRO2: // NTS: ESS 688 cards also had an OPL3 (http://www.dosdays.co.uk/topics/Manufacturers/ess.php)
					case SBT_16:
						if (sb[ci].ess_type != ESS_NONE && sb[ci].ess_type != ESS_688) {
							opl_mode=OPL_esfm;
						} else {
							opl_mode=OPL_opl3;
						}
						break;
				}
			}

			if (IS_PC98_ARCH) {
				if (opl_mode != OPL_none) {
					if (opl_mode != OPL_opl3) {
						LOG(LOG_SB,LOG_WARN)("Only OPL3 is allowed in PC-98 mode");
						opl_mode = OPL_opl3;
					}
				}

				/* card type MUST be SB16.
				 * Creative did not release any other Sound Blaster for PC-98 as far as I know. */
				if (sb[ci].type != SBT_16) {
					LOG(LOG_SB,LOG_ERROR)("Only Sound Blaster 16 is allowed in PC-98 mode");
					sb[ci].type = SBT_NONE;
				}
			}
		}
	public:
		SBLASTER(const size_t n_ci,Section* configuration):Module_base(configuration) {
			bool bv;
			string s;
			Bitu i;
			int si;

			Section_prop * section=static_cast<Section_prop *>(configuration);

			ci = n_ci;
			sb[ci].card_index = ci;
			sb[ci].recording_source = REC_SILENCE;
			sb[ci].listen_to_recording_source = false;
			sb[ci].hw.base=(unsigned int)section->Get_hex("sbbase");

			if (sb[ci].ess_type != ESS_NONE && sb[ci].ess_type != ESS_688) {
				sb[ci].mixer.ess_id_str[2] = (sb[ci].hw.base >> 8) & 0xff;
				sb[ci].mixer.ess_id_str[3] = sb[ci].hw.base & 0xff;
			}

			if (IS_PC98_ARCH) {
				if (sb[ci].hw.base >= 0x220 && sb[ci].hw.base <= 0x2E0) /* translate IBM PC to PC-98 (220h -> D2h) */
					sb[ci].hw.base = 0xD0 + ((sb[ci].hw.base >> 4u) & 0xFu);
			}
			else {
				if (sb[ci].hw.base >= 0xD2 && sb[ci].hw.base <= 0xDE) /* translate PC-98 to IBM PC (D2h -> 220h) */
					sb[ci].hw.base = 0x200 + ((sb[ci].hw.base & 0xFu) << 4u);
			}

			sb[ci].sb_update_recording_source_settings();

			sb[ci].ASP_mode = 0x00;
			sb[ci].goldplay=section->Get_bool("goldplay");
			sb[ci].min_dma_user=section->Get_int("mindma");
			sb[ci].goldplay_stereo=section->Get_bool("goldplay stereo");
			sb[ci].emit_blaster_var=section->Get_bool("blaster environment variable");
			sb[ci].sample_rate_limits=section->Get_bool("sample rate limits");
			sb[ci].sbpro_stereo_bit_strict_mode=section->Get_bool("stereo control with sbpro only");
			sb[ci].hw.sb_io_alias=section->Get_bool("io port aliasing");
			sb[ci].busy_cycle_hz=section->Get_int("dsp busy cycle rate");
			sb[ci].busy_cycle_duty_percent=section->Get_int("dsp busy cycle duty");
			sb[ci].dsp.instant_direct_dac=section->Get_bool("instant direct dac");
			sb[ci].dsp.force_goldplay=section->Get_bool("force goldplay");
			sb[ci].dma.force_autoinit=section->Get_bool("force dsp auto-init");
			sb[ci].no_filtering=section->Get_bool("disable filtering");
			sb[ci].def_enable_speaker=section->Get_bool("enable speaker");
			sb[ci].enable_asp=section->Get_bool("enable asp");

			if (!sb[ci].goldplay && sb[ci].dsp.force_goldplay) {
				sb[ci].goldplay = true;
				LOG_MSG("force goldplay = true but goldplay = false, enabling Goldplay mode anyway");
			}

			/* Explanation: If the user set this option, the write status port must return 0x7F or 0xFF.
			 *              Else, we're free to return whatever with bit 7 to indicate busy.
			 *              The reason for this setting is that there exist some very early DOS demoscene productions
			 *              that assume the I/O port returns 0x7F or 0xFF, and will have various problems if that
			 *              is not the case.
			 *
			 *              Overload by Hysteria (1992):
			 *                 - This demo's Sound Blaster routines use the unusual space-saving assembly language
			 *                   trick of reading port 22Ch to wait for bit 7 to clear (which is normal), then
			 *                   using the value it read (left over from AL) as the byte to sum it's audio output
			 *                   to before sending out to the DAC. This is why, if the write status port returns
			 *                   a different value like 0x2A, the audio crackles with saturation errors.
			 *
			 *                   Overload's Sound Blaster output code:
			 *
			 *                   090D:5F50  EC                  in   al,dx
			 *                   090D:5F51  A880                test al,80
			 *                   090D:5F53  75FB                jne  00005F50 ($-5)         (no jmp)
			 *                   090D:5F55  FEC0                inc  al
			 *                   ; various code to render and sum audio samples to AL
			 *                   090D:5FC7  EE                  out  dx,al
			 *
			 *                   Notice it assumes AL is 0x7F and it assumes (++AL) == 0x80.
			 *
			 *                   It's also funny to note the demo's readme file mentions this problem with Sound
			 *                   Blaster Pro:
			 *
			 *                   "I have been told this works with the soundblaster however, this demo does
			 *                   not seem to work the soundblaster pro.
			 *
			 *                   If the music sounds bad and you want to know what it really sounds like
			 *                   let me know and I can send you the MOD format of the music.
			 *
			 *                   dmw@sioux.ee.ufl.edu"
			 *
			 */
			sb[ci].write_status_must_return_7f=section->Get_bool("dsp write buffer status must return 0x7f or 0xff");

			sb[ci].dsp.midi_rwpoll_mode = false;
			sb[ci].dsp.midi_read_interrupt = false;
			sb[ci].dsp.midi_read_with_timestamps = false;

			sb[ci].busy_cycle_last_check=0;
			sb[ci].busy_cycle_io_hack=0;

			si=section->Get_int("irq");
			sb[ci].hw.irq=(si >= 0) ? (unsigned int)si : 0xFF;

			if (IS_PC98_ARCH) {
				if (sb[ci].hw.irq == 7) /* IRQ 7 is not valid on PC-98 (that's cascade interrupt) */
					sb[ci].hw.irq = 5;
			}

			si=section->Get_int("dma");
			sb[ci].hw.dma8=(si >= 0) ? (unsigned int)si : 0xFF;

			si=section->Get_int("hdma");
			sb[ci].hw.dma16=(si >= 0) ? (unsigned int)si : 0xFF;

			if (IS_PC98_ARCH) {
				if (sb[ci].hw.dma8 > 3)
					sb[ci].hw.dma8 = 3;
				if (sb[ci].hw.dma8 == 1) /* DMA 1 is not usable for SB on PC-98? */
					sb[ci].hw.dma8 = 3;
				if (sb[ci].hw.dma16 > 3)
					sb[ci].hw.dma16 = sb[ci].hw.dma8;

				LOG_MSG("PC-98: Final SB16 resources are DMA8=%u DMA16=%u\n",sb[ci].hw.dma8,sb[ci].hw.dma16);

				sb[ci].dma.chan=GetDMAChannel(sb[ci].hw.dma8);
				if (sb[ci].dma.chan != NULL)
					sb[ci].dma.chan->userData = ci;
				else
					LOG_MSG("PC-98: SB16 is unable to obtain DMA channel");
			}

			sb[ci].dsp.command_aliases=section->Get_bool("dsp command aliases");

			Find_Type_And_Opl(section,sb[ci].type,oplmode);
			if (sb[ci].hw.irq == 0) {
				std::string sbtype=GetSBtype();
				sb[ci].hw.irq=sbtype=="SBPro 2"||sbtype=="SB16"||IS_PC98_ARCH?5:7;
			}

			/* some DOS games/demos support Sound Blaster, and expect the IRQ to fire, but
			 * make no attempt to unmask the IRQ (perhaps the programmer forgot). This option
			 * is needed for Public NMI "jump" demo in order for music to continue playing. */
			bv=section->Get_bool("pic unmask irq");
			if (bv && sb[ci].hw.irq != 0xFF) {
				LOG_MSG("Sound blaster: unmasking IRQ at startup as requested.");
				PIC_SetIRQMask(sb[ci].hw.irq,false);
			}

			if (sb[ci].hw.irq == 0xFF || sb[ci].hw.dma8 == 0xFF) {
				LOG(LOG_SB,LOG_WARN)("IRQ and 8-bit DMA not assigned, disabling BLASTER variable");
				sb[ci].emit_blaster_var = false;
			}

			sb[ci].mixer.enabled=section->Get_bool("sbmixer");
			sb[ci].mixer.stereo=false;

			bv=section->Get_bool("pre-set sbpro stereo");
			if (bv) {
				LOG_MSG("Sound blaster: setting SB Pro mixer 'stereo' bit as instructed.");
				sb[ci].mixer.stereo=true;
			}

#if HAS_HARDOPL
			bool isCMSpassthrough = false;
#endif
			if (ci == 0) {
				switch (oplmode) {
					case OPL_none:
						if (!IS_PC98_ARCH)
							WriteHandler[0].Install(0x388,adlib_gusforward,IO_MB);
						break;
					case OPL_opl2:
					case OPL_dualopl2:
						assert(!IS_PC98_ARCH);
						// fall-through
					case OPL_opl3:
					case OPL_opl3gold:
					case OPL_esfm:
						OPL_Init(section,oplmode);
						break;
					case OPL_hardwareCMS:
						assert(!IS_PC98_ARCH);
#if HAS_HARDOPL
						isCMSpassthrough = true;
#endif
						// fall-through
					case OPL_hardware:
						assert(!IS_PC98_ARCH);
#if HAS_HARDOPL
						Bitu base = (unsigned int)section->Get_hex("hardwarebase");
						HARDOPL_Init(base, sb[ci].hw.base, isCMSpassthrough);
#else
						LOG_MSG("OPL pass-through is disabled. It may not be supported on this operating system.");
#endif
						break;
				}
			}

			// Backward compatibility with existing configurations
			if(!strcmp(section->Get_string("oplmode"),"cms")) {
				LOG(LOG_SB, LOG_WARN)("The 'cms' setting for 'oplmode' is deprecated; use 'cms = on' instead.");
				sb[ci].cms = true;
			}
			else {
				const char *cms_str = section->Get_string("cms");
				if(!strcmp(cms_str,"on")) {
					sb[ci].cms = true;
				}
				else if(!strcmp(cms_str,"auto")) {
					sb[ci].cms = (sb[ci].type == SBT_1 || sb[ci].type == SBT_GB);
				}
				else
					sb[ci].cms = false;
			}

			/* only the first card can have CMS */
			if (sb[ci].cms && ci != 0)
				sb[ci].cms = false;

			switch(sb[ci].type) {
				case SBT_1: // CMS is optional for Sound Blaster 1 and 2
				case SBT_2:;
				case SBT_GB:
					   if(!sb[ci].cms) {
						   LOG(LOG_SB, LOG_WARN)("'cms' setting is 'off', but is forced to 'auto' on the Game Blaster.");
						   auto* sect_updater = sbGetSection(ci);
						   sect_updater->Get_prop("cms")->SetValue("auto");
					   }
					   sb[ci].cms = true; // Game Blaster is CMS
					   break;
				default:
					   if(sb[ci].cms) {
						   LOG(LOG_SB, LOG_WARN)("'cms' setting 'on' not supported on this card, forcing 'auto'.");
						   auto* sect_updater = sbGetSection(ci);
						   sect_updater->Get_prop("cms")->SetValue("auto");
					   }
					   sb[ci].cms = false;
			}

			if(sb[ci].cms && !IS_PC98_ARCH) {
				CMS_Init(section);
			}

			if (sb[ci].type==SBT_NONE || sb[ci].type==SBT_GB) return;

			sb[ci].chan=MixerChan.Install(SBLASTER_CallBacks[ci],22050,sbMixerChanNames[ci]);
			sb[ci].dac.dac_pt = sb[ci].dac.dac_t = 0;
			sb[ci].dsp.state=DSP_S_NORMAL;
			sb[ci].dsp.out.lastval=0xaa;
			sb[ci].dma.chan=NULL;

			for (i=4;i<=0xf;i++) {
				if (i==8 || i==9) continue;
				//Disable mixer ports for lower soundblaster
				if ((sb[ci].type==SBT_1 || sb[ci].type==SBT_2) && (i==4 || i==5)) continue;
				ReadHandler[i].Install(sb[ci].hw.base+(IS_PC98_ARCH ? ((i+0x20u) << 8u) : i),read_sbs[ci],IO_MB);
				WriteHandler[i].Install(sb[ci].hw.base+(IS_PC98_ARCH ? ((i+0x20u) << 8u) : i),write_sbs[ci],IO_MB);
			}

			// TODO: read/write handler for ESS AudioDrive ES1688 (and later) MPU-401 ports (3x0h/3x1h; prevents Windows drivers from working with default settings if missing)

			// NTS: Unknown/undefined registers appear to return the register index you requested rather than the actual contents,
			//      according to real SB16 CSP/ASP hardware (chip version id 0x10).
			//
			//      Registers 0x00-0x1F are defined. Registers 0x80-0x83 are defined.
			for (i=0;i<256;i++) sb[ci].ASP_regs[i] = i;
			for (i=0x00;i < 0x20;i++) sb[ci].ASP_regs[i] = 0;
			for (i=0x80;i < 0x84;i++) sb[ci].ASP_regs[i] = 0;
			sb[ci].ASP_regs[5] = 0x01;
			sb[ci].ASP_regs[9] = 0xf8;

			sb[ci].DSP_Reset();
			sb[ci].CTMIXER_Reset();

			// The documentation does not specify if SB gets initialized with the speaker enabled
			// or disabled. Real SBPro2 has it disabled.
			sb[ci].speaker=false;
			// On SB16 the speaker flag does not affect actual speaker state.
			if (sb[ci].type == SBT_16 || sb[ci].ess_type != ESS_NONE || sb[ci].reveal_sc_type != RSC_NONE) sb[ci].chan->Enable(true);
			else sb[ci].chan->Enable(false);

			if (sb[ci].def_enable_speaker)
				sb[ci].DSP_SetSpeaker(true);

			s=section->Get_string("dsp require interrupt acknowledge");
			if (s == "true" || s == "1" || s == "on")
				sb[ci].dsp.require_irq_ack = 1;
			else if (s == "false" || s == "0" || s == "off")
				sb[ci].dsp.require_irq_ack = 0;
			else /* auto */
				sb[ci].dsp.require_irq_ack = (sb[ci].type == SBT_16) ? 1 : 0; /* Yes if SB16, No otherwise */

			si=section->Get_int("dsp write busy delay"); /* in nanoseconds */
			if (si >= 0) sb[ci].dsp.dsp_write_busy_time = (unsigned int)si;
			else sb[ci].dsp.dsp_write_busy_time = 1000; /* FIXME: How long is the DSP busy on real hardware? */

			/* Sound Blaster (1.x and 2x) and Sound Blaster Pro (3.1) have the I/O port aliasing.
			 * The aliasing is also faithfully emulated by the ESS AudioDrive. */
			switch (sb[ci].type) {
				case SBT_1: /* guess */
				case SBT_2: /* verified on real hardware */
				case SBT_GB: /* FIXME: Right?? */
				case SBT_PRO1: /* verified on real hardware */
				case SBT_PRO2: /* guess */
					break;
				default:
					sb[ci].hw.sb_io_alias=false;
					break;
			}

			/* auto-pick busy cycle */
			/* NOTE: SB16 cards appear to run a DSP busy cycle at all times.
			 *       SB2 cards (and Pro?) appear to run a DSP busy cycle only when playing audio through DMA. */
			if (sb[ci].busy_cycle_hz < 0) {
				if (sb[ci].type == SBT_16) /* Guess: Pentium PCI-ISA SYSCLK=8.333MHz  /  (6 cycles per 8-bit I/O read  x  16 reads from DSP status) = about 86.805KHz? */
					sb[ci].busy_cycle_hz = 8333333 / 6 / 16;
				else /* Guess ???*/
					sb[ci].busy_cycle_hz = 8333333 / 6 / 16;
			}

			if (sb[ci].ess_type != ESS_NONE) {
				uint8_t t;

				/* legacy audio interrupt control */
				t = 0x80;/*game compatible IRQ*/
				switch (sb[ci].hw.irq) {
					case 5:     t |= 0x5; break;
					case 7:     t |= 0xA; break;
					case 10:    t |= 0xF; break;
				}
				sb[ci].ESSreg(0xB1) = t;

				/* DRQ control */
				t = 0x80;/*game compatible DRQ */
				switch (sb[ci].hw.dma8) {
					case 0:     t |= 0x5; break;
					case 1:     t |= 0xA; break;
					case 3:     t |= 0xF; break;
				}
				sb[ci].ESSreg(0xB2) = t;
			}

			/* first configuration byte returned by 0x58.
			 *
			 * bits 7-5: ???
			 * bit    4: Windows Sound System (Crystal Semiconductor CS4231A) I/O base port (1=0xE80  0=0x530)
			 * bits 3-2: ???
			 * bit    1: gameport disable (1=disabled 0=enabled)
			 * bit    0: jumper assigns Sound Blaster base port 240h (right??) */
			sb[ci].sc400_jumper_status_1 = 0x2C + ((sb[ci].hw.base == 0x240) ? 0x1 : 0x0);
			if (!JOYSTICK_IsEnabled(0) && !JOYSTICK_IsEnabled(1)) sb[ci].sc400_jumper_status_1 += 0x02; // set bit 1
			/* second configuration byte returned by 0x58.
			 *
			 * bits 7-5: 110b ???
			 * bits 4-0: bits 8-4 of the CD-ROM I/O port base. For example, to put the CD-ROM controller at 0x230 you would set this to 0x3.
			 *           TESTSC.EXE ignores the value unless it represents one of the supported base I/O ports for the CD-ROM, which are:
			 *
			 *           0x03 -> 0x230
			 *           0x05 -> 0x250
			 *           0x11 -> 0x310
			 *           0x12 -> 0x320
			 *           0x13 -> 0x330
			 *           0x14 -> 0x340
			 *           0x15 -> 0x350
			 *           0x16 -> 0x360
			 *
			 *           TODO: The CD-ROM interface is said to be designed for Panasonic or Goldstar CD-ROM drives. Can we emulate that someday,
			 *                 and allow the user to configure the base I/O port? */
			sb[ci].sc400_jumper_status_2 = 0xC0 | (((sb[ci].hw.base + 0x10) >> 4) & 0xF);

			sb[ci].sc400_dsp_major = 3;
			sb[ci].sc400_dsp_minor = 5;
			sb[ci].sc400_cfg = 0;
			/* bit 7:    MPU IRQ select  (1=IRQ 9  0=IRQ 5)
			 * bit 6:    ???
			 * bit 5-3:  IRQ select
			 * bit 2:    MPU IRQ enable
			 * bit 1-0:  DMA select */
			switch (sb[ci].hw.dma8) {
				case 0: sb[ci].sc400_cfg |= (1 << 0); break;
				case 1: sb[ci].sc400_cfg |= (2 << 0); break;
				case 3: sb[ci].sc400_cfg |= (3 << 0); break;
			}
			switch (sb[ci].hw.irq) {
				case 5: sb[ci].sc400_cfg |= (5 << 3); break;
				case 7: sb[ci].sc400_cfg |= (1 << 3); break;
				case 9: sb[ci].sc400_cfg |= (2 << 3); break;
				case 10:sb[ci].sc400_cfg |= (3 << 3); break;
				case 11:sb[ci].sc400_cfg |= (4 << 3); break;
			}
			switch (MPU401_GetIRQ()) { // SC400: bit 7 and bit 2 control MPU IRQ
				case 5: sb[ci].sc400_cfg |= (0 << 7) + (1 << 2); break; // bit 7=0 bit 2=1   MPU IRQ 5
				case 9: sb[ci].sc400_cfg |= (1 << 7) + (1 << 2); break; // bit 7=1 bit 2=1   MPU IRQ 9
				default: break;                     // bit 7=0 bit 2=0   MPU IRQ disabled
			}

			if (sb[ci].reveal_sc_type != RSC_NONE) {
				// SC400 cards always use 8-bit DMA even for 16-bit PCM
				if (sb[ci].hw.dma16 != sb[ci].hw.dma8) {
					LOG(LOG_SB,LOG_DEBUG)("SC400: DMA is always 8-bit, setting 16-bit == 8-bit");
					sb[ci].hw.dma16 = sb[ci].hw.dma8;
				}
			}

			si = section->Get_int("dsp busy cycle always");
			if (si >= 0) sb[ci].busy_cycle_always= (si > 0) ? true : false;
			else if (sb[ci].type == SBT_16) sb[ci].busy_cycle_always = true;
			else sb[ci].busy_cycle_always = false;

			/* auto-pick busy duty cycle */
			if (sb[ci].busy_cycle_duty_percent < 0 || sb[ci].busy_cycle_duty_percent > 100)
				sb[ci].busy_cycle_duty_percent = 50; /* seems about right */

			if (sb[ci].hw.irq != 0 && sb[ci].hw.irq != 0xFF) {
				s = section->Get_string("irq hack");
				if (!s.empty() && s != "none") {
					LOG(LOG_SB,LOG_NORMAL)("Sound Blaster emulation: Assigning IRQ hack '%s' as instructed",s.c_str());
					PIC_Set_IRQ_hack((int)sb[ci].hw.irq,PIC_parse_IRQ_hack_string(s.c_str()));
				}
			}

			// ASP emulation
			if (sb[ci].enable_asp && sb[ci].type != SBT_16) {
				LOG(LOG_SB,LOG_WARN)("ASP emulation requires you to select SB16 emulation");
				sb[ci].enable_asp = false;
			}

			// SB16 non-PNP ASP RAM content observation.
			// Contrary to my initial impression, it looks like the RAM is mostly 0xFF
			// with some random bits flipped because no initialization is done at hardware power-on.
			memset(sb[ci].sb16asp_ram_contents,0xFF,2048);
			for (i=0;i < (2048 * 8);i++) {
				if (((unsigned int)rand() & 31) == 0)
					sb[ci].sb16asp_ram_contents[i>>3] ^= 1 << (i & 7);
			}

			/* Reference: Command 0xF9 result map taken from Sound Blaster 16 with DSP 4.4 and ASP chip version ID 0x10:
			 *
			 * ASP> F9 result map:
00: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ff 07
10: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
20: f9 00 00 00 00 aa 96 00 00 00 00 00 00 00 00 00
30: f9 00 00 00 00 00 00 38 00 00 00 00 00 00 00 00
40: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
50: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
60: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
70: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
80: 00 19 0a 00 00 00 00 00 00 00 00 00 00 00 00 00
90: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
a0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
b0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
c0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
d0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
e0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
f0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
ASP>
			 * End reference */
			memset(sb[ci].sb16_8051_mem,0x00,256);
			sb[ci].sb16_8051_mem[0x0E] = 0xFF;
			sb[ci].sb16_8051_mem[0x0F] = 0x07;
			sb[ci].sb16_8051_mem[0x37] = 0x38;

			if (sb[ci].enable_asp)
				LOG(LOG_SB,LOG_WARN)("ASP emulation at this stage is extremely limited and only covers DSP command responses.");

			/* Soundblaster midi interface */
			if (!MIDI_Available()) sb[ci].midi = false;
			else sb[ci].midi = true;
		}

		void DOS_Shutdown() { /* very likely, we're booting into a guest OS where our environment variable has no meaning anymore */
			autoexecline.Uninstall();
		}

		void DOS_Startup() {
			if (sb[ci].type==SBT_NONE || sb[ci].type==SBT_GB) return;

			if (sb[ci].emit_blaster_var && ci == 0) {
				// Create set blaster line
				ostringstream temp;
				if (IS_PC98_ARCH)
					temp << "@SET BLASTER=A" << setw(2) << hex << sb[ci].hw.base;
				else
					temp << "@SET BLASTER=A" << setw(3) << hex << sb[ci].hw.base;

				if (sb[ci].hw.irq != 0xFF) temp << " I" << dec << (Bitu)sb[ci].hw.irq;
				if (sb[ci].hw.dma8 != 0xFF) temp << " D" << (Bitu)sb[ci].hw.dma8;
				if (sb[ci].type==SBT_16 && sb[ci].hw.dma16 != 0xFF) temp << " H" << (Bitu)sb[ci].hw.dma16;
				if (!IS_PC98_ARCH) {
					Section_prop * section=static_cast<Section_prop *>(control->GetSection("midi"));
					const char* s_mpu = section->Get_string("mpu401");
					if(strcasecmp(s_mpu,"none") && strcasecmp(s_mpu,"off") && strcasecmp(s_mpu,"false")) {
						Bitu baseio = (Bitu)section->Get_hex("mpubase");
						if (baseio == 0 || baseio < 0x300 || baseio > 0x360)
							baseio = 0x330;
						temp << " P" << hex << baseio;
					}
				}
				temp << " T" << static_cast<unsigned int>(sb[ci].type) << ends;

				autoexecline.Install(temp.str());
			}
		}

		~SBLASTER() {
			switch (oplmode) {
				case OPL_none:
					break;
				case OPL_opl2:
				case OPL_dualopl2:
				case OPL_opl3:
				case OPL_opl3gold:
				case OPL_esfm:
					OPL_ShutDown(m_configuration);
					break;
				default:
					break;
			}
			if(sb[ci].cms) {
				CMS_ShutDown(m_configuration);
			}
			if (sb[ci].type==SBT_NONE || sb[ci].type==SBT_GB) return;
			sb[ci].DSP_Reset(); // Stop everything
		}
}; //End of SBLASTER class

static SBLASTER* test[MAX_CARDS] = {NULL};

void SBLASTER_DOS_Shutdown() {
	for (size_t ci=0;ci < MAX_CARDS;ci++) {
		if (test[ci] != NULL)
			test[ci]->DOS_Shutdown();
	}
}

void SBLASTER_ShutDown(Section* /*sec*/) {
	for (size_t ci=0;ci < MAX_CARDS;ci++) {
		if (test[ci] != NULL) {
			delete test[ci];
			test[ci] = NULL;
		}
	}
#if HAS_HARDOPL
	HARDOPL_Cleanup();
#endif
}

void SBLASTER_OnReset(Section *sec) {
	(void)sec;//UNUSED
	SBLASTER_DOS_Shutdown();

	for (size_t ci=0;ci < MAX_CARDS;ci++) {
		if (test[ci] != NULL) {
			delete test[ci];
			test[ci] = NULL;
		}
	}
#if HAS_HARDOPL
	HARDOPL_Cleanup();
#endif

	for (size_t ci=0;ci < MAX_CARDS;ci++) {
		if (test[ci] == NULL) {
			LOG(LOG_MISC,LOG_DEBUG)("Allocating Sound Blaster emulation");
			test[ci] = new SBLASTER(ci,sbGetSection(ci));
		}
	}
}

void SBLASTER_DOS_Exit(Section *sec) {
	(void)sec;//UNUSED
	SBLASTER_DOS_Shutdown();
}

void SBLASTER_DOS_Boot(Section *sec) {
	(void)sec;//UNUSED
	for (size_t ci=0;ci < MAX_CARDS;ci++) {
		if (test[ci] != NULL)
			test[ci]->DOS_Startup();
	}
}

void SBLASTER_Init() {
	LOG(LOG_MISC,LOG_DEBUG)("Initializing Sound Blaster emulation");

	AddExitFunction(AddExitFunctionFuncPair(SBLASTER_ShutDown),true);
	AddVMEventFunction(VM_EVENT_RESET,AddVMEventFunctionFuncPair(SBLASTER_OnReset));
	AddVMEventFunction(VM_EVENT_DOS_EXIT_BEGIN,AddVMEventFunctionFuncPair(SBLASTER_DOS_Exit));
	AddVMEventFunction(VM_EVENT_DOS_SURPRISE_REBOOT,AddVMEventFunctionFuncPair(SBLASTER_DOS_Exit));
	AddVMEventFunction(VM_EVENT_DOS_EXIT_REBOOT_BEGIN,AddVMEventFunctionFuncPair(SBLASTER_DOS_Exit));
	AddVMEventFunction(VM_EVENT_DOS_INIT_SHELL_READY,AddVMEventFunctionFuncPair(SBLASTER_DOS_Boot));
}

// save state support
void *DMA_DAC_Event_PIC_Event = (void*)((uintptr_t)DMA_DAC_Event);
void *DMA_Silent_Event_PIC_Event = (void*)((uintptr_t)DMA_Silent_Event);
void *DSP_FinishReset_PIC_Event = (void*)((uintptr_t)DSP_FinishReset);
void *DSP_RaiseIRQEvent_PIC_Event = (void*)((uintptr_t)DSP_RaiseIRQEvent);
void *END_DMA_Event_PIC_Event = (void*)((uintptr_t)END_DMA_Event);

void *SB_DSP_DMA_CallBack_Func = (void*)((uintptr_t)DSP_DMA_CallBack);
void *SB_DSP_ADC_CallBack_Func = (void*)((uintptr_t)DSP_ADC_CallBack);
void *SB_DSP_E2_DMA_CallBack_Func = (void*)((uintptr_t)DSP_E2_DMA_CallBack);


void POD_Save_Sblaster( std::ostream& stream )
{
	const size_t ci=0;
	const char pod_name[32] = "SBlaster";

	if( stream.fail() ) return;
	if( !test ) return;
	if( !sb[ci].chan ) return;


	WRITE_POD( &pod_name, pod_name );


	//*******************************************
	//*******************************************
	//*******************************************

	uint8_t dma_idx;


	dma_idx = 0xff;
	for( int lcv=0; lcv<8; lcv++ ) {
		if( sb[ci].dma.chan == GetDMAChannel(lcv) ) { dma_idx = lcv; break; }
	}

	// *******************************************
	// *******************************************
	// *******************************************

	// - near-pure data
	WRITE_POD( &sb, sb );

	// - reloc ptr
	WRITE_POD( &dma_idx, dma_idx );

	// *******************************************
	// *******************************************
	// *******************************************

	sb[ci].chan->SaveState(stream);
}


void POD_Load_Sblaster( std::istream& stream )
{
	const size_t ci=0;
	char pod_name[32] = {0};

	if( stream.fail() ) return;
	if( !test ) return;
	if( !sb[ci].chan ) return;


	// error checking
	READ_POD( &pod_name, pod_name );
	if( strcmp( pod_name, "SBlaster" ) ) {
		stream.clear( std::istream::failbit | std::istream::badbit );
		return;
	}

	//************************************************
	//************************************************
	//************************************************

	uint8_t dma_idx;
	MixerChannel *mixer_old;


	// save static ptr
	mixer_old = sb[ci].chan;

	//*******************************************
	//*******************************************
	//*******************************************

	// - near-pure data
	READ_POD( &sb, sb );

	// - reloc ptr
	READ_POD( &dma_idx, dma_idx );

	//*******************************************
	//*******************************************
	//*******************************************

	sb[ci].dma.chan = NULL;
	if( dma_idx != 0xff ) sb[ci].dma.chan = GetDMAChannel(dma_idx);

	//*******************************************
	//*******************************************
	//*******************************************

	// restore static ptr
	sb[ci].chan = mixer_old;


	sb[ci].chan->LoadState(stream);
}
