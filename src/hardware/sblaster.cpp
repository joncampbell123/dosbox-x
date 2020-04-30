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
 *       As usual, expect this to be a dosbox.conf option --Jonathan C. */

/* FIXME: Sound Blaster 16 hardware has a FIFO between the ISA BUS and DSP.
 *        Could we update this code to read through a FIFO instead? How big is this
 *        FIFO anyway, and which cards have it? Would it also be possible to eliminate
 *        the need for sb.dma.min? */

#if defined(_MSC_VER)
# pragma warning(disable:4244) /* const fmath::local::uint64_t to double possible loss of data */
# pragma warning(disable:4305) /* truncation from double to float */
# pragma warning(disable:4065) /* switch without case */
#endif

#include <iomanip>
#include <sstream>
#include <stdlib.h>
#include <string.h>
#include <math.h> 
#include "dosbox.h"
#include "inout.h"
#include "mixer.h"
#include "dma.h"
#include "pic.h"
#include "bios.h"
#include "hardware.h"
#include "control.h"
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
#define SB_SH   14
#define SB_SH_MASK  ((1 << SB_SH)-1)

enum {DSP_S_RESET,DSP_S_RESET_WAIT,DSP_S_NORMAL,DSP_S_HIGHSPEED};
enum SB_TYPES {SBT_NONE=0,SBT_1=1,SBT_PRO1=2,SBT_2=3,SBT_PRO2=4,SBT_16=6,SBT_GB=7}; /* TODO: Need SB 2.0 vs SB 2.01 */
enum REVEAL_SC_TYPES {RSC_NONE=0,RSC_SC400=1};
enum SB_IRQS {SB_IRQ_8,SB_IRQ_16,SB_IRQ_MPU};
enum ESS_TYPES {ESS_NONE=0,ESS_688=1};

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

struct SB_INFO {
    Bitu freq;
    Bit8u timeconst;
    Bitu dma_dac_srcrate;
    struct {
        bool stereo,sign,autoinit;
        bool force_autoinit;
        DMA_MODES mode_assigned;
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
    Bit8u sc400_cfg;
    Bit8u time_constant;
    Bit8u sc400_dsp_major,sc400_dsp_minor;
    Bit8u sc400_jumper_status_1;
    Bit8u sc400_jumper_status_2;
    DSP_MODES mode;
    SB_TYPES type;
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
        Bit8u state;
        Bit8u last_cmd;
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
        bool instant_direct_dac;
        bool force_goldplay;
        bool midi_rwpoll_mode; // DSP command 0x34/0x35 MIDI Read Poll & Write Poll (0x35 with interrupt)
        bool midi_read_interrupt;
        bool midi_read_with_timestamps;
        unsigned int dsp_write_busy_time; /* when you write to the DSP, how long it signals "busy" */
    } dsp;
    struct {
        Bit16s last;
        double dac_t,dac_pt;
    } dac;
    struct {
        Bit8u index;
        Bit8u dac[2],fm[2],cda[2],master[2],lin[2];
        Bit8u mic;
        bool stereo;
        bool enabled;
        bool filtered;
        bool sbpro_stereo; /* Game or OS is attempting SB Pro type stereo */
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
        bool sb_io_alias;
    } hw;
    struct {
        Bit8u valadd;
        Bit8u valxor;
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

// number of bytes in input for commands (sb/sbpro compatible Reveal SC400)
static Bit8u const DSP_cmd_len_sc400[256] = {
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x00
//  1,0,0,0, 2,0,2,2, 0,0,0,0, 0,0,0,0,  // 0x10
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
static Bit8u const DSP_cmd_len_ess[256] = {
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
  1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1,  // 0xa0   ESS write register commands (0xA0-0xBF). Note this overlap with SB16 is
  1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1,  // 0xb0   why ESS chipsets cannot emulate SB16 playback/record commands.

  1,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0xc0   ESS additional commands.
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
  0,0,0,0, 0,0,0,0, 0,1,2,0, 0,0,0,0   // 0xf0
};

static unsigned char ESSregs[0x20] = {0}; /* 0xA0-0xBF */

static unsigned char &ESSreg(uint8_t reg) {
    assert(reg >= 0xA0 && reg <= 0xBF);
    return ESSregs[reg-0xA0];
}

static Bit8u ASP_regs[256];
static Bit8u ASP_mode = 0x00;

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

    if (sb.ess_playback_mode) {
        if (!(ESSreg(0xB1) & 0x40)) // if ESS playback, and IRQ disabled, do not fire
            return;
    }

    switch (type) {
    case SB_IRQ_8:
        if (sb.irq.pending_8bit) {
//          LOG_MSG("SB: 8bit irq pending");
            return;
        }
        sb.irq.pending_8bit=true;
        PIC_ActivateIRQ(sb.hw.irq);
        break;
    case SB_IRQ_16:
        if (sb.irq.pending_16bit) {
//          LOG_MSG("SB: 16bit irq pending");
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

static double last_dma_callback = 0.0f;

static void DSP_DMA_CallBack(DmaChannel * chan, DMAEvent event) {
    if (chan!=sb.dma.chan || event==DMA_REACHED_TC) return;
    else if (event==DMA_MASKED) {
        if (sb.mode==MODE_DMA) {
            //Catch up to current time, but don't generate an IRQ!
            //Fixes problems with later sci games.
            double t = PIC_FullIndex() - last_dma_callback;
            Bitu s = static_cast<Bitu>(sb.dma.rate * t / 1000.0f);
            if (s > sb.dma.min) {
                LOG(LOG_SB,LOG_NORMAL)("limiting amount masked to sb.dma.min");
                s = sb.dma.min;
            }
            Bitu min_size = sb.dma.mul >> SB_SH;
            if (!min_size) min_size = 1;
            min_size *= 2;
            if (sb.dma.left > min_size) {
                if (s > (sb.dma.left-min_size)) s = sb.dma.left - min_size;
                GenerateDMASound(s);
            }
            sb.mode=MODE_DMA_MASKED;
            LOG(LOG_SB,LOG_NORMAL)("DMA masked,stopping output, left %d",chan->currcnt);
        }
    } else if (event==DMA_TRANSFEREND) {
        if (sb.mode==MODE_DMA) sb.mode=MODE_DMA_MASKED;
    } else if (event==DMA_UNMASKED) {
        if (sb.mode==MODE_DMA_MASKED && sb.dma.mode!=DSP_DMA_NONE) {
            DSP_ChangeMode(MODE_DMA);
            CheckDMAEnd();
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
    if (sb.ess_type == ESS_NONE && sb.reveal_sc_type == RSC_NONE && sb.dma.mode >= DSP_DMA_16) {
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

        if (sb.ess_playback_mode) {
            LOG(LOG_SB,LOG_NORMAL)("ESS DMA stop");
            ESSreg(0xB8) &= ~0x01; // automatically stop DMA (right?)
            if (sb.dma.chan) sb.dma.chan->Clear_Request();
        }
    } else {
        sb.dma.left=sb.dma.total;
        if (!sb.dma.left) {
            LOG(LOG_SB,LOG_NORMAL)("Auto-init transfer with 0 size");
            sb.dsp.highspeed = false;
            sb.mode=MODE_NONE;
        }
        else if (sb.dsp.require_irq_ack && was_irq) {
            /* Sound Blaster 16 behavior: If you do not acknowledge the IRQ, and the card goes to signal another IRQ, the DSP halts playback.
             * This is different from previous cards (and clone hardware) that continue playing whether or not you acknowledge the IRQ. */
            LOG(LOG_SB,LOG_WARN)("DMA ended when previous IRQ had not yet been acked");
            sb.mode=MODE_DMA_REQUIRE_IRQ_ACK;
        }
    }
}

static void GenerateDMASound(Bitu size) {
    Bitu read=0;Bitu done=0;Bitu i=0;

    // don't read if the DMA channel is masked
    if (sb.dma.chan->masked) return;

    if (sb.dma_dac_mode) return;

    if(sb.dma.autoinit) {
        if (sb.dma.left <= size) size = sb.dma.left;
    } else {
        if (sb.dma.left <= sb.dma.min) 
            size = sb.dma.left;
    }

    if (size > DMA_BUFSIZE) {
        /* Maybe it's time to consider rendering intervals based on what the mixer wants rather than odd 1ms DMA packet calculations... */
        LOG(LOG_SB,LOG_WARN)("Whoah! GenerateDMASound asked to render too much audio (%u > %u). Read could have overrun the DMA buffer!",(unsigned int)size,DMA_BUFSIZE);
        size = DMA_BUFSIZE;
    }

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
            sb.dma.mode=sb.dma.mode_assigned=DSP_DMA_NONE;
        }
    }
    if (sb.dma.left) {
        Bitu bigger=(sb.dma.left > sb.dma.min) ? sb.dma.min : sb.dma.left;
        float delay=(bigger*1000.0f)/sb.dma.rate;
        PIC_AddEvent(DMA_Silent_Event,delay,bigger);
    }

}

#include <assert.h>

void updateSoundBlasterFilter(Bitu rate);

static void DMA_DAC_Event(Bitu val) {
    (void)val;//UNUSED
    unsigned char tmp[4];
    Bitu read,expct;
    signed int L,R;
    Bit16s out[2];

    if (sb.dma.chan->masked) {
        PIC_AddEvent(DMA_DAC_Event,1000.0 / sb.dma_dac_srcrate);
        return;
    }
    if (!sb.dma.left)
        return;

    /* Fix for 1994 Demoscene entry myth_dw: The demo's Sound Blaster Pro initialization will start DMA with
     * count == 1 or 2 (triggering Goldplay mode) but will change the DMA initial counter when it begins
     * normal playback. If goldplay stereo hack is enabled and we do not catch this case, the first 0.5 seconds
     * of music will play twice as fast. */
    if (sb.dma.chan != NULL &&
        sb.dma.chan->basecnt < ((sb.dma.mode==DSP_DMA_16_ALIASED?2:1)*((sb.dma.stereo || sb.mixer.sbpro_stereo)?2:1))/*size of one sample in DMA counts*/)
        sb.single_sample_dma = 1;
    else
        sb.single_sample_dma = 0;

    if (!sb.single_sample_dma) {
        // WARNING: This assumes Sound Blaster Pro emulation!
        LOG(LOG_SB,LOG_NORMAL)("Goldplay mode unexpectedly switched off, normal DMA playback follows"); 
        sb.dma_dac_mode = 0;
        sb.dma_dac_srcrate = sb.freq / (sb.mixer.stereo ? 2 : 1);
        sb.chan->SetFreq(sb.dma_dac_srcrate);
        updateSoundBlasterFilter(sb.dma_dac_srcrate);
        return;
    }

    /* NTS: chan->Read() deals with DMA unit transfers.
     *      for 8-bit DMA, read/expct is in bytes, for 16-bit DMA, read/expct is in 16-bit words */
    expct = (sb.dma.stereo ? 2u : 1u) * (sb.dma.mode == DSP_DMA_16_ALIASED ? 2u : 1u);
    read = sb.dma.chan->Read(expct,tmp);
    //if (read != expct)
    //      LOG_MSG("DMA read was not sample aligned. Sound may swap channels or become static. On real hardware the same may happen unless audio is prepared specifically.\n");

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
        out[0]=L;
        out[1]=R;
        sb.chan->AddSamples_s16(1,out);
    }
    else {
        out[0]=L;
        sb.chan->AddSamples_m16(1,out);
    }

    /* NTS: The reason we check this is that sometimes the various "checks" performed by
       -        *      setup/configuration tools will setup impossible playback scenarios to test
       -        *      the card that would result in read > sb.dma.left. If read > sb.dma.left then
       -        *      the subtraction below would drive sb.dma.left below zero and the IRQ would
       -        *      never fire, and the test program would fail to detect SB16 emulation.
       -        *
       -        *      Bugfix for "Extreme Assault" that allows the game to detect Sound Blaster 16
       -        *      hardware. "Extreme Assault"'s SB16 test appears to configure a DMA transfer
       -        *      of 1 byte then attempt to play 16-bit signed stereo PCM (4 bytes) which prior
       -        *      to this fix would falsely trigger Goldplay then cause sb.dma.left to underrun
       -        *      and fail to fire the IRQ. */
    if (sb.dma.left >= read)
        sb.dma.left -= read;
    else
        sb.dma.left = 0;

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

static void DSP_DoDMATransfer(DMA_MODES mode,Bitu freq,bool stereo,bool dontInitLeft=false) {
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
    if (sb.dma.chan != NULL &&
        sb.dma.chan->basecnt < ((mode==DSP_DMA_16_ALIASED?2:1)*((stereo || sb.mixer.sbpro_stereo)?2:1))/*size of one sample in DMA counts*/)
        sb.single_sample_dma = 1;
    else
        sb.single_sample_dma = 0;

    sb.dma_dac_srcrate=freq;
    if (sb.dsp.force_goldplay || (sb.goldplay && sb.freq > 0 && sb.single_sample_dma))
        sb.dma_dac_mode=1;
    else
        sb.dma_dac_mode=0;

    /* explanation: the purpose of Goldplay stereo mode is to compensate for the fact
     * that demos using this method of playback know to set the SB Pro stereo bit, BUT,
     * apparently did not know that they needed to double the sample rate when
     * computing the DSP time constant. Such demos sound "OK" on Sound Blaster Pro but
     * have audible aliasing artifacts because of this. The Goldplay Stereo hack
     * detects this condition and doubles the sample rate to better capture what the
     * demo is *trying* to do. NTS: sb.freq is the raw sample rate given by the
     * program, before it is divided by two for stereo.
     *
     * Of course, some demos like Crystal Dream take the approach of just setting the
     * sample rate to the max supported by the card and then letting it's timer interrupt
     * define the sample rate. So of course anything below 44.1KHz sounds awful. */
    if (sb.dma_dac_mode && sb.goldplay_stereo && (stereo || sb.mixer.sbpro_stereo) && sb.single_sample_dma)
        sb.dma_dac_srcrate = sb.freq;

    sb.chan->FillUp();

    if (!dontInitLeft)
        sb.dma.left=sb.dma.total;

    sb.dma.mode=sb.dma.mode_assigned=mode;
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
    sb.dma.min=((Bitu)sb.dma.rate*(Bitu)(sb.min_dma_user >= 0 ? sb.min_dma_user : /*default*/3))/1000u;
    if (sb.dma_dac_mode && sb.goldplay_stereo && (stereo || sb.mixer.sbpro_stereo) && sb.single_sample_dma) {
//        LOG(LOG_SB,LOG_DEBUG)("Goldplay stereo hack. freq=%u rawfreq=%u dacrate=%u",(unsigned int)freq,(unsigned int)sb.freq,(unsigned int)sb.dma_dac_srcrate);
        sb.chan->SetFreq(sb.dma_dac_srcrate);
        updateSoundBlasterFilter(freq); /* BUT, you still filter like the actual sample rate */
    }
    else {
        sb.chan->SetFreq(freq);
        updateSoundBlasterFilter(freq);
    }
    sb.dma.mode=sb.dma.mode_assigned=mode;
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
    LOG(LOG_SB,LOG_NORMAL)("DMA Transfer:%s %s %s freq %d rate %d size %d gold %d",
        type,
        sb.dma.stereo ? "Stereo" : "Mono",
        sb.dma.autoinit ? "Auto-Init" : "Single-Cycle",
        (int)freq,(int)sb.dma.rate,(int)sb.dma.total,
        (int)sb.dma_dac_mode
    );
#else
    (void)type;
#endif
}

static Bit8u DSP_RateLimitedFinalTC_Old() {
    if (sb.sample_rate_limits) { /* enforce speed limits documented by Creative */
        /* commands that invoke this call use the DSP time constant, so use the DSP
         * time constant to constrain rate */
        unsigned int u_limit=212;/* 23KHz */

        /* NTS: We skip the SB16 commands because those are handled by another function */
        if ((sb.dsp.cmd&0xFE) == 0x74 || sb.dsp.cmd == 0x7D) { /* 4-bit ADPCM */
            u_limit = 172; /* 12KHz */
        }
        else if ((sb.dsp.cmd&0xFE) == 0x76) { /* 2.6-bit ADPCM */
            if (sb.type == SBT_2) u_limit = 172; /* 12KHz */
            else u_limit = 179; /* 13KHz */
        }
        else if ((sb.dsp.cmd&0xFE) == 0x16) { /* 2-bit ADPCM */
            if (sb.type == SBT_2) u_limit = 189; /* 15KHz */
            else u_limit = 165; /* 11KHz */
        }
        else if (sb.type == SBT_16) /* Sound Blaster 16. Highspeed commands are treated like an alias to normal DSP commands */
            u_limit = sb.vibra ? 234/*46KHz*/ : 233/*44.1KHz*/;
        else if (sb.type == SBT_2) /* Sound Blaster 2.0 */
            u_limit = (sb.dsp.highspeed ? 233/*44.1KHz*/ : 210/*22.5KHz*/);
        else
            u_limit = (sb.dsp.highspeed ? 233/*44.1KHz*/ : 212/*22.5KHz*/);

        /* NTS: Don't forget: Sound Blaster Pro "stereo" is programmed with a time constant divided by
         *      two times the sample rate, which is what we get back here. That's why here we don't need
         *      to consider stereo vs mono. */
        if (sb.timeconst > u_limit) return u_limit;
    }

    return sb.timeconst;
}

static unsigned int DSP_RateLimitedFinalSB16Freq_New(unsigned int freq) {
    // sample rate was set by SB16 DSP command 0x41/0x42, not SB/SBpro command 0x40 (unusual case)
    if (sb.sample_rate_limits) { /* enforce speed limits documented by Creative */
        unsigned int u_limit,l_limit=4000; /* NTS: Recording vs playback is not considered because DOSBox only emulates playback */

        if (sb.vibra) u_limit = 46000;
        else u_limit = 44100;

        if (freq < l_limit)
            freq = l_limit;
        if (freq > u_limit)
            freq = u_limit;
    }

    return freq;
}

static void DSP_PrepareDMA_Old(DMA_MODES mode,bool autoinit,bool sign,bool hispeed) {
    if (sb.dma.force_autoinit)
        autoinit = true;

    sb.dma.total=1u+(unsigned int)sb.dsp.in.data[0]+(unsigned int)(sb.dsp.in.data[1] << 8u);
    sb.dma.autoinit=autoinit;
    sb.dsp.highspeed=hispeed;
    sb.dma.sign=sign;

    /* BUGFIX: There is code out there that uses SB16 sample rate commands mixed with SB/SBPro
     *         playback commands. In order to handle these cases properly we need to use the
     *         SB16 sample rate if that is what was given to us, else the sample rate will
     *         come out wrong.
     *
     *         Test cases:
     *
     *         #1: Silpheed (vogons user newrisingsun amatorial patch)
     */
    if (sb.freq_derived_from_tc) {
        // sample rate was set by SB/SBpro command 0x40 Set Time Constant (very common case)
        /* BUGFIX: Instead of direct rate-limiting the DSP time constant, keep the original
         *         value written intact and rate-limit a copy. Bugfix for Optic Nerve and
         *         sbtype=sbpro2. On initialization the demo first sends DSP command 0x14
         *         with a 2-byte playback interval, then sends command 0x91 to begin
         *         playback. Rate-limiting the copy means the 45454Hz time constant written
         *         by the demo stays intact despite being limited to 22050Hz during the first
         *         DSP block (command 0x14). */
        Bit8u final_tc = DSP_RateLimitedFinalTC_Old();
        sb.freq = (256000000ul / (65536ul - ((unsigned long)final_tc << 8ul)));
    }
    else {
        LOG(LOG_SB,LOG_DEBUG)("Guest is using non-SB16 playback commands after using SB16 commands to set sample rate");
        sb.freq = DSP_RateLimitedFinalSB16Freq_New(sb.freq);
    }

    sb.dma_dac_mode=0;
    sb.ess_playback_mode = false;
    sb.dma.chan=GetDMAChannel(sb.hw.dma8);
    DSP_DoDMATransfer(mode,sb.freq / (sb.mixer.stereo ? 2 : 1),sb.mixer.stereo);
}

static void DSP_PrepareDMA_New(DMA_MODES mode,Bitu length,bool autoinit,bool stereo) {
    if (sb.dma.force_autoinit)
        autoinit = true;

    /* apparently SB16 hardware allows 0xBx-0xCx 4.xx DSP commands to interrupt
     * a previous SB16 playback command, DSP "nag" style. The difference is that
     * if you do that you risk exploiting DMA and timing glitches in the chip that
     * can cause funny things to happen, like causing 16-bit PCM to stop, or causing
     * 8-bit stereo PCM to swap left/right channels because the host is using auto-init
     * DMA and you interrupted the DSP chip when it fetched the L channel before it
     * had a chance to latch it and begin loading the R channel. */
    if (sb.mode == MODE_DMA) {
        if (!autoinit) sb.dma.total=length;
        sb.dma.left=sb.dma.total;
        sb.dma.autoinit=autoinit;
        return;
    }

    sb.dsp.highspeed = false;
    sb.freq = DSP_RateLimitedFinalSB16Freq_New(sb.freq);
    sb.timeconst = (65536 - (256000000 / sb.freq)) >> 8;
    sb.freq_derived_from_tc = false;

    Bitu freq=sb.freq;
    //equal length if data format and dma channel are both 16-bit or 8-bit
    sb.dma_dac_mode=0;
    sb.dma.total=length;
    sb.dma.autoinit=autoinit;
    sb.ess_playback_mode = false;
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
    sb.dsp.cmd=DSP_NO_COMMAND;
    sb.dsp.cmd_len=0;
    sb.dsp.in.pos=0;
    sb.dsp.out.pos=0;
    sb.dsp.write_busy=0;
    sb.ess_extended_mode = false;
    sb.ess_playback_mode = false;
    sb.single_sample_dma = 0;
    sb.dma_dac_mode = 0;
    sb.directdac_warn_speaker_off = true;
    PIC_RemoveEvents(DSP_FinishReset);
    PIC_RemoveEvents(DSP_BusyComplete);

    sb.dma.left=0;
    sb.dma.total=0;
    sb.dma.stereo=false;
    sb.dma.sign=false;
    sb.dma.autoinit=false;
    sb.dma.mode=sb.dma.mode_assigned=DSP_DMA_NONE;
    sb.dma.remain_size=0;
    if (sb.dma.chan) sb.dma.chan->Clear_Request();

    sb.dsp.midi_rwpoll_mode = false;
    sb.dsp.midi_read_interrupt = false;
    sb.dsp.midi_read_with_timestamps = false;

    sb.freq=22050;
    sb.freq_derived_from_tc=true;
    sb.time_constant=45;
    sb.dac.last=0;
    sb.e2.valadd=0xaa;
    sb.e2.valxor=0x96;
    sb.dsp.highspeed=0;
    sb.irq.pending_8bit=false;
    sb.irq.pending_16bit=false;
    sb.chan->SetFreq(22050);
    updateSoundBlasterFilter(22050);
//  DSP_SetSpeaker(false);
    PIC_RemoveEvents(END_DMA_Event);
    PIC_RemoveEvents(DMA_DAC_Event);
}

static void DSP_DoReset(Bit8u val) {
    if (((val&1)!=0) && (sb.dsp.state!=DSP_S_RESET)) {
//TODO Get out of highspeed mode
        DSP_Reset();
        sb.dsp.state=DSP_S_RESET;
    } else if (((val&1)==0) && (sb.dsp.state==DSP_S_RESET)) {   // reset off
        sb.dsp.state=DSP_S_RESET_WAIT;
        PIC_RemoveEvents(DSP_FinishReset);
        PIC_AddEvent(DSP_FinishReset,20.0f/1000.0f,0);  // 20 microseconds
    }
    sb.dsp.write_busy = 0;
}

static void DSP_E2_DMA_CallBack(DmaChannel * /*chan*/, DMAEvent event) {
    if (event==DMA_UNMASKED) {
        Bit8u val = sb.e2.valadd;
        DmaChannel * chan=GetDMAChannel(sb.hw.dma8);
        chan->Register_Callback(0);
        chan->Write(1,&val);
    }
}

Bitu DEBUG_EnableDebugger(void);

static void DSP_SC400_E6_DMA_CallBack(DmaChannel * /*chan*/, DMAEvent event) {
    if (event==DMA_UNMASKED) {
        static const char *string = "\x01\x02\x04\x08\x10\x20\x40\x80"; /* Confirmed response via DMA from actual Reveal SC400 card */
        DmaChannel * chan=GetDMAChannel(sb.hw.dma8);
        LOG(LOG_SB,LOG_DEBUG)("SC400 returning DMA test pattern on DMA channel=%u",sb.hw.dma8);
        chan->Register_Callback(0);
        chan->Write(8,(Bit8u*)string);
        chan->Clear_Request();
        if (!chan->tcount) LOG(LOG_SB,LOG_DEBUG)("SC400 warning: DMA did not reach terminal count");
        SB_RaiseIRQ(SB_IRQ_8);
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

static void DSP_ChangeRate(Bitu freq) {
	if (sb.freq!=freq && sb.dma.mode!=DSP_DMA_NONE) {
		sb.chan->FillUp();
		sb.chan->SetFreq(freq / (sb.mixer.stereo ? 2 : 1));
		sb.dma.rate=(freq*sb.dma.mul) >> SB_SH;
		sb.dma.min=(sb.dma.rate*3)/1000;
	}
	sb.freq=freq;
}

Bitu DEBUG_EnableDebugger(void);

#define DSP_SB16_ONLY if (sb.type != SBT_16) { LOG(LOG_SB,LOG_ERROR)("DSP:Command %2X requires SB16",sb.dsp.cmd); break; }
#define DSP_SB2_ABOVE if (sb.type <= SBT_1) { LOG(LOG_SB,LOG_ERROR)("DSP:Command %2X requires SB2 or above",sb.dsp.cmd); break; } 

static unsigned int ESS_DMATransferCount() {
    unsigned int r;

    r = (unsigned int)ESSreg(0xA5) << 8U;
    r |= (unsigned int)ESSreg(0xA4);

    /* the 16-bit counter is a "two's complement" of the DMA count because it counts UP to 0 and triggers IRQ on overflow */
    return 0x10000U-r;
}

static void ESS_StartDMA() {
    LOG(LOG_SB,LOG_DEBUG)("ESS DMA start");
    sb.dma_dac_mode = 0;
    sb.dma.chan = GetDMAChannel(sb.hw.dma8);
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
        sb.freq,(ESSreg(0xA8/*Analog control*/)&3)==1?1:0/*stereo*/,true/*don't change dma.left*/);
    sb.ess_playback_mode = true;
}

static void ESS_StopDMA() {
    // DMA stop
    DSP_ChangeMode(MODE_NONE);
    if (sb.dma.chan) sb.dma.chan->Clear_Request();
    PIC_RemoveEvents(END_DMA_Event);
    PIC_RemoveEvents(DMA_DAC_Event);
}

static void ESS_UpdateDMATotal() {
    sb.dma.total = ESS_DMATransferCount();
}

static void ESS_CheckDMAEnable() {
    bool dma_en = (ESSreg(0xB8) & 1)?true:false;

    // if the DRQ is disabled, do not start
    if (!(ESSreg(0xB2) & 0x40))
        dma_en = false;
    // HACK: DOSBox does not yet support recording
    if (ESSreg(0xB8) & 8/*ADC mode*/)
        dma_en = false;
    if (ESSreg(0xB8) & 2/*DMA read*/)
        dma_en = false;

    if (dma_en) {
        if (sb.mode != MODE_DMA) ESS_StartDMA();
    }
    else {
        if (sb.mode == MODE_DMA) ESS_StopDMA();
    }
}

static void ESSUpdateFilterFromSB(void) {
    if (sb.freq >= 22050)
        ESSreg(0xA1) = 256 - (795500UL / sb.freq);
    else
        ESSreg(0xA1) = 128 - (397700UL / sb.freq);

    unsigned int freq = ((sb.freq * 4) / (5 * 2)); /* 80% of 1/2 the sample rate */
    ESSreg(0xA2) = 256 - (7160000 / (freq * 82));
}

static void ESS_DoWrite(uint8_t reg,uint8_t data) {
    uint8_t chg;

    LOG(LOG_SB,LOG_DEBUG)("ESS register write reg=%02xh val=%02xh",reg,data);

    switch (reg) {
        case 0xA1: /* Extended Mode Sample Rate Generator */
            ESSreg(reg) = data;
            if (data & 0x80)
                sb.freq = 795500UL / (256ul - data);
            else
                sb.freq = 397700UL / (128ul - data);

            sb.freq_derived_from_tc = false;
            if (sb.mode == MODE_DMA) {
                ESS_StopDMA();
                ESS_StartDMA();
            }
            break;
        case 0xA2: /* Filter divider (effectively, a hardware lowpass filter under S/W control) */
            ESSreg(reg) = data;
            updateSoundBlasterFilter(sb.freq);
            break;
        case 0xA4: /* DMA Transfer Count Reload (low) */
        case 0xA5: /* DMA Transfer Count Reload (high) */
            ESSreg(reg) = data;
            ESS_UpdateDMATotal();
            if (sb.dma.left == 0) sb.dma.left = sb.dma.total;
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
                if (sb.mode == MODE_DMA) {
                    ESS_StopDMA();
                    ESS_StartDMA();
                }
            }
            break;
        case 0xB1: /* Legacy Audio Interrupt Control */
            chg = ESSreg(reg) ^ data;
            ESSreg(reg) = (ESSreg(reg) & 0x0F) + (data & 0xF0); // lower 4 bits not writeable
            if (chg & 0x40) ESS_CheckDMAEnable();
            break;
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
            sb.dma.sign = (data&0x20)?1:0;
            if (chg & 0x04) ESS_UpdateDMATotal();
            if (chg & 0x0C) {
                if (sb.mode == MODE_DMA) {
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
            if (chg & 1) sb.dma.left = sb.dma.total;

            sb.dma.autoinit = (data >> 2) & 1;
            if (chg & 0xB) ESS_CheckDMAEnable();
            break;
        case 0xB9: /* Audio 1 Transfer Type */
        case 0xBA: /* Left Channel ADC Offset Adjust */
        case 0xBB: /* Right Channel ADC Offset Adjust */
            ESSreg(reg) = data;
            break;
    }
}

static uint8_t ESS_DoRead(uint8_t reg) {
    LOG(LOG_SB,LOG_DEBUG)("ESS register read reg=%02xh",reg);

    switch (reg) {
        default:
            return ESSreg(reg);
    }

    return 0xFF;
}

int MPU401_GetIRQ();

/* SB16 cards have a 256-byte block of 8051 internal RAM accessible through DSP commands 0xF9 (Read) and 0xFA (Write) */
static unsigned char sb16_8051_mem[256];

/* The SB16 ASP appears to have a mystery 2KB RAM block that is accessible through register 0x83 of the ASP.
 * This array represents the initial contents as seen on my SB16 non-PnP ASP chip (version ID 0x10). */
static unsigned int sb16asp_ram_contents_index = 0;
static unsigned char sb16asp_ram_contents[2048];

static void sb16asp_write_current_RAM_byte(const uint8_t r) {
    sb16asp_ram_contents[sb16asp_ram_contents_index] = r;
}

static uint8_t sb16asp_read_current_RAM_byte(void) {
    return sb16asp_ram_contents[sb16asp_ram_contents_index];
}

static void sb16asp_next_RAM_byte(void) {
    if ((++sb16asp_ram_contents_index) >= 2048)
        sb16asp_ram_contents_index = 0;
}

/* Demo notes for fixing:
 *
 *  - "Buttman"'s intro uses a timer and DSP command 0x10 to play the sound effects even in Pro mode.
 *    It doesn't use DMA + IRQ until the music starts.
 */

static void DSP_DoCommand(void) {
    if (sb.ess_type != ESS_NONE && sb.dsp.cmd >= 0xA0 && sb.dsp.cmd <= 0xCF) {
        // ESS overlap with SB16 commands. Handle it here, not mucking up the switch statement.

        if (sb.dsp.cmd < 0xC0) { // write ESS register   (cmd=register data[0]=value to write)
            if (sb.ess_extended_mode)
                ESS_DoWrite(sb.dsp.cmd,sb.dsp.in.data[0]);
        }
        else if (sb.dsp.cmd == 0xC0) { // read ESS register   (data[0]=register to read)
            DSP_FlushData();
            if (sb.ess_extended_mode && sb.dsp.in.data[0] >= 0xA0 && sb.dsp.in.data[0] <= 0xBF)
                DSP_AddData(ESS_DoRead(sb.dsp.in.data[0]));
        }
        else if (sb.dsp.cmd == 0xC6 || sb.dsp.cmd == 0xC7) { // set(0xC6) clear(0xC7) extended mode
            sb.ess_extended_mode = (sb.dsp.cmd == 0xC6);
        }
        else {
            LOG(LOG_SB,LOG_DEBUG)("ESS: Unknown command %02xh",sb.dsp.cmd);
        }

        sb.dsp.last_cmd=sb.dsp.cmd;
        sb.dsp.cmd=DSP_NO_COMMAND;
        sb.dsp.cmd_len=0;
        sb.dsp.in.pos=0;
        return;
    }

    if (sb.type == SBT_16) {
        // FIXME: This is a guess! See also [https://github.com/joncampbell123/dosbox-x/issues/1044#issuecomment-480024957]
        sb16_8051_mem[0x20] = sb.dsp.last_cmd; /* cur_cmd */
    }

    // TODO: There are more SD16 ASP commands we can implement, by name even, with microcode download,
    //       using as reference the Linux kernel driver code:
    //
    //       http://lxr.free-electrons.com/source/sound/isa/sb/sb16_csp.c

//  LOG_MSG("DSP Command %X",sb.dsp.cmd);
    switch (sb.dsp.cmd) {
    case 0x04:
        if (sb.type == SBT_16) {
            /* SB16 ASP set mode register */
            ASP_mode = sb.dsp.in.data[0];

            // bit 7: if set, enables bit 3 and memory access.
            // bit 3: if set, and bit 7 is set, register 0x83 can be used to read/write ASP internal memory. if clear, register 0x83 contains chip version ID
            // bit 2: if set, memory index is reset to 0. doesn't matter if memory access or not.
            // bit 1: if set, writing register 0x83 increments memory index. doesn't matter if memory access or not.
            // bit 0: if set, reading register 0x83 increments memory index. doesn't matter if memory access or not.
            if (ASP_mode&4)
                sb16asp_ram_contents_index = 0;

            LOG(LOG_SB,LOG_DEBUG)("SB16ASP set mode register to %X",sb.dsp.in.data[0]);
        } else {
            /* DSP Status SB 2.0/pro version. NOT SB16. */
            DSP_FlushData();
            if (sb.type == SBT_2) DSP_AddData(0x88);
            else if ((sb.type == SBT_PRO1) || (sb.type == SBT_PRO2)) DSP_AddData(0x7b);
            else DSP_AddData(0xff);         //Everything enabled
        }
        break;
    case 0x05:  /* SB16 ASP set codec parameter */
        LOG(LOG_SB,LOG_NORMAL)("DSP Unhandled SB16ASP command %X (set codec parameter) value=0x%02x parameter=0x%02x",
            sb.dsp.cmd,sb.dsp.in.data[0],sb.dsp.in.data[1]);
        break;
    case 0x08:  /* SB16 ASP get version */
        if (sb.type == SBT_16) {
            switch (sb.dsp.in.data[0]) {
                case 0x03:
                    LOG(LOG_SB,LOG_DEBUG)("DSP SB16ASP command %X sub %X (get chip version)",sb.dsp.cmd,sb.dsp.in.data[0]);

                    if (sb.enable_asp)
                        DSP_AddData(0x10);  // version ID
                    else
                        DSP_AddData(0xFF);  // NTS: This is what a SB16 ViBRA PnP card with no ASP returns when queried in this way
                    break;
                default:
                    LOG(LOG_SB,LOG_NORMAL)("DSP Unhandled SB16ASP command %X sub %X",sb.dsp.cmd,sb.dsp.in.data[0]);
                    break;
            }
        } else {
            LOG(LOG_SB,LOG_NORMAL)("DSP Unhandled SB16ASP command %X sub %X",sb.dsp.cmd,sb.dsp.in.data[0]);
        }
        break;
    case 0x0e:  /* SB16 ASP set register */
        if (sb.type == SBT_16) {
            if (sb.enable_asp) {
                ASP_regs[sb.dsp.in.data[0]] = sb.dsp.in.data[1];

                if (sb.dsp.in.data[0] == 0x83) {
                    if ((ASP_mode&0x88) == 0x88) { // bit 3 and bit 7 must be set
                        // memory access mode
                        if (ASP_mode & 4) // NTS: As far as I can tell...
                            sb16asp_ram_contents_index = 0;

                        // log it, write it
                        LOG(LOG_SB,LOG_DEBUG)("SB16 ASP write internal RAM byte index=0x%03x val=0x%02x",sb16asp_ram_contents_index,sb.dsp.in.data[1]);
                        sb16asp_write_current_RAM_byte(sb.dsp.in.data[1]);

                        if (ASP_mode & 2) // if bit 1 of the mode is set, memory index increment on write
                            sb16asp_next_RAM_byte();
                    }
                    else {
                        // unknown. nothing, I assume?
                        LOG(LOG_SB,LOG_WARN)("SB16 ASP set register 0x83 not implemented for non-memory mode (unknown behavior)\n");
                    }
                }
                else {
                    LOG(LOG_SB,LOG_DEBUG)("SB16 ASP set register reg=0x%02x val=0x%02x",sb.dsp.in.data[0],sb.dsp.in.data[1]);
                }
            }
            else {
                LOG(LOG_SB,LOG_DEBUG)("SB16 ASP set register reg=0x%02x val=0x%02x ignored, ASP not enabled",sb.dsp.in.data[0],sb.dsp.in.data[1]);
            }
        } else {
            LOG(LOG_SB,LOG_NORMAL)("DSP Unhandled SB16ASP command %X (set register)",sb.dsp.cmd);
        }
        break;
    case 0x0f:  /* SB16 ASP get register */
        if (sb.type == SBT_16) {
            // FIXME: We have to emulate this whether or not ASP emulation is enabled. Windows 98 SB16 driver requires this.
            //        The question is: What does actual hardware do here exactly?
            if (sb.enable_asp && sb.dsp.in.data[0] == 0x83) {
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
                LOG(LOG_SB,LOG_DEBUG)("SB16 ASP get register reg=0x%02x, returning 0x%02x",sb.dsp.in.data[0],ASP_regs[sb.dsp.in.data[0]]);
            }

            DSP_AddData(ASP_regs[sb.dsp.in.data[0]]);
        } else {
            LOG(LOG_SB,LOG_NORMAL)("DSP Unhandled SB16ASP command %X (get register)",sb.dsp.cmd);
        }
        break;
    case 0x10:  /* Direct DAC */
        DSP_ChangeMode(MODE_DAC);

        /* just in case something is trying to play direct DAC audio while the speaker is turned off... */
        if (!sb.speaker && sb.directdac_warn_speaker_off) {
            LOG(LOG_SB,LOG_DEBUG)("DSP direct DAC sample written while speaker turned off. Program should use DSP command 0xD1 to turn it on.");
            sb.directdac_warn_speaker_off = false;
        }

        sb.freq = 22050;
        sb.freq_derived_from_tc = true;
        sb.dac.dac_pt = sb.dac.dac_t;
        sb.dac.dac_t = PIC_FullIndex();
        {
            double dt = sb.dac.dac_t - sb.dac.dac_pt; // time in milliseconds since last direct DAC output
            double rt = 1000 / dt; // estimated sample rate according to dt
            int s,sc = 1;

            // cap rate estimate to sanity. <= 1KHz means rendering once per timer tick in DOSBox,
            // so there's no point below that rate in additional rendering.
            if (rt < 1000) rt = 1000;

            // FIXME: What does the ESS AudioDrive do to it's filter/sample rate divider registers when emulating this Sound Blaster command?
            ESSreg(0xA1) = 128 - (397700 / 22050);
            ESSreg(0xA2) = 256 - (7160000 / (82 * ((4 * 22050) / 10)));

            // Direct DAC playback could be thought of as application-driven 8-bit output up to 23KHz.
            // The sound card isn't given any hint what the actual sample rate is, only that it's given
            // instruction when to change the 8-bit value being output to the DAC which is why older DOS
            // games using this method tend to sound "grungy" compared to DMA playback. We recreate the
            // effect here by asking the mixer to do it's linear interpolation as if at 23KHz while
            // rendering the audio at whatever rate the DOS game is giving it to us.
            sb.chan->SetFreq((Bitu)(rt * 0x100),0x100);
            updateSoundBlasterFilter(sb.freq);

            // avoid popping/crackling artifacts through the mixer by ensuring the render output is prefilled enough
            if (sb.chan->msbuffer_o < 40/*FIXME: ask the mixer code!*/) sc += 2/*FIXME: use mixer rate / rate math*/;

            // do it
            for (s=0;s < sc;s++) sb.chan->AddSamples_m8(1,(Bit8u*)(&sb.dsp.in.data[0]));
        }
        break;
    case 0x24:  /* Singe Cycle 8-Bit DMA ADC */
        sb.dma.left=sb.dma.total=1u+(unsigned int)sb.dsp.in.data[0]+((unsigned int)sb.dsp.in.data[1] << 8u);
        sb.dma.sign=false;
        LOG(LOG_SB,LOG_ERROR)("DSP:Faked ADC for %d bytes",(int)sb.dma.total);
        GetDMAChannel(sb.hw.dma8)->Register_Callback(DSP_ADC_CallBack);
        break;
    case 0x91:  /* Singe Cycle 8-Bit DMA High speed DAC */
        DSP_SB2_ABOVE;
        /* fall through */
    case 0x14:  /* Singe Cycle 8-Bit DMA DAC */
    case 0x15:  /* Wari hack. Waru uses this one instead of 0x14, but some weird stuff going on there anyway */
        /* Note: 0x91 is documented only for DSP ver.2.x and 3.x, not 4.x */
        DSP_PrepareDMA_Old(DSP_DMA_8,false,false,/*hispeed*/(sb.dsp.cmd&0x80)!=0);
        break;
    case 0x90:  /* Auto Init 8-bit DMA High Speed */
    case 0x1c:  /* Auto Init 8-bit DMA */
        DSP_SB2_ABOVE; /* Note: 0x90 is documented only for DSP ver.2.x and 3.x, not 4.x */
        DSP_PrepareDMA_Old(DSP_DMA_8,true,false,/*hispeed*/(sb.dsp.cmd&0x80)!=0);
        break;
    case 0x38:  /* Write to SB MIDI Output */
        if (sb.midi == true) MIDI_RawOutByte(sb.dsp.in.data[0]);
        break;
    case 0x40:  /* Set Timeconstant */
        DSP_ChangeRate(256000000ul / (65536ul - ((unsigned int)sb.dsp.in.data[0] << 8u)));
        sb.timeconst=sb.dsp.in.data[0];
        sb.freq_derived_from_tc=true;

        if (sb.ess_type != ESS_NONE) ESSUpdateFilterFromSB();
        break;
    case 0x41:  /* Set Output Samplerate */
    case 0x42:  /* Set Input Samplerate */
        if (sb.reveal_sc_type == RSC_SC400) {
            /* Despite reporting itself as Sound Blaster Pro compatible, the Reveal SC400 supports some SB16 commands like this one */
        }
        else {
            DSP_SB16_ONLY;
        }

        DSP_ChangeRate(((unsigned int)sb.dsp.in.data[0] << 8u) | (unsigned int)sb.dsp.in.data[1]);
        sb.freq_derived_from_tc=false;
        sb16_8051_mem[0x13] = sb.freq & 0xffu;                  // rate low
        sb16_8051_mem[0x14] = (sb.freq >> 8u) & 0xffu;          // rate high
        break;
    case 0x48:  /* Set DMA Block Size */
        DSP_SB2_ABOVE;
        //TODO Maybe check limit for new irq?
        sb.dma.total=1u+(unsigned int)sb.dsp.in.data[0]+((unsigned int)sb.dsp.in.data[1] << 8u);
        break;
    case 0x75:  /* 075h : Single Cycle 4-bit ADPCM Reference */
        sb.adpcm.haveref=true;
    case 0x74:  /* 074h : Single Cycle 4-bit ADPCM */   
        DSP_PrepareDMA_Old(DSP_DMA_4,false,false,false);
        break;
    case 0x77:  /* 077h : Single Cycle 3-bit(2.6bit) ADPCM Reference*/
        sb.adpcm.haveref=true;
    case 0x76:  /* 074h : Single Cycle 3-bit(2.6bit) ADPCM */
        DSP_PrepareDMA_Old(DSP_DMA_3,false,false,false);
        break;
    case 0x7d:  /* Auto Init 4-bit ADPCM Reference */
        DSP_SB2_ABOVE;
        sb.adpcm.haveref=true;
        DSP_PrepareDMA_Old(DSP_DMA_4,true,false,false);
        break;
    case 0x7f:  /* Auto Init 3-bit(2.6bit) ADPCM Reference */
        DSP_SB2_ABOVE;
        sb.adpcm.haveref=true;
        DSP_PrepareDMA_Old(DSP_DMA_3,true,false,false);
        break;
    case 0x1f:  /* Auto Init 2-bit ADPCM Reference */
        DSP_SB2_ABOVE;
        sb.adpcm.haveref=true;
        DSP_PrepareDMA_Old(DSP_DMA_2,true,false,false);
        break;
    case 0x17:  /* 017h : Single Cycle 2-bit ADPCM Reference*/
        sb.adpcm.haveref=true;
    case 0x16:  /* 074h : Single Cycle 2-bit ADPCM */
        DSP_PrepareDMA_Old(DSP_DMA_2,false,false,false);
        break;
    case 0x80:  /* Silence DAC */
        PIC_AddEvent(&DSP_RaiseIRQEvent,
            (1000.0f*(1+sb.dsp.in.data[0]+(sb.dsp.in.data[1] << 8))/sb.freq));
        break;
    case 0xb0:  case 0xb1:  case 0xb2:  case 0xb3:  case 0xb4:  case 0xb5:  case 0xb6:  case 0xb7:
    case 0xb8:  case 0xb9:  case 0xba:  case 0xbb:  case 0xbc:  case 0xbd:  case 0xbe:  case 0xbf:
    case 0xc0:  case 0xc1:  case 0xc2:  case 0xc3:  case 0xc4:  case 0xc5:  case 0xc6:  case 0xc7:
    case 0xc8:  case 0xc9:  case 0xca:  case 0xcb:  case 0xcc:  case 0xcd:  case 0xce:  case 0xcf:
        if (sb.reveal_sc_type == RSC_SC400) {
            /* Despite reporting itself as Sound Blaster Pro, the Reveal SC400 cards do support *some* SB16 DSP commands! */
            /* BUT, it only recognizes a subset of this range. */
            if (sb.dsp.cmd == 0xBE || sb.dsp.cmd == 0xB6 ||
                sb.dsp.cmd == 0xCE || sb.dsp.cmd == 0xC6) {
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

        /* Generic 8/16 bit DMA */
//      DSP_SetSpeaker(true);       //SB16 always has speaker enabled
        sb.dma.sign=(sb.dsp.in.data[0] & 0x10) > 0;
        DSP_PrepareDMA_New((sb.dsp.cmd & 0x10) ? DSP_DMA_16 : DSP_DMA_8,
            1u+(unsigned int)sb.dsp.in.data[1]+((unsigned int)sb.dsp.in.data[2] << 8u),
            (sb.dsp.cmd & 0x4)>0,
            (sb.dsp.in.data[0] & 0x20) > 0
        );
        break;
    case 0xd5:  /* Halt 16-bit DMA */
        DSP_SB16_ONLY;
    case 0xd0:  /* Halt 8-bit DMA */
        sb.chan->FillUp();
//      DSP_ChangeMode(MODE_NONE);
//      Games sometimes already program a new dma before stopping, gives noise
        if (sb.mode==MODE_NONE) {
            // possibly different code here that does not switch to MODE_DMA_PAUSE
        }
        sb.mode=MODE_DMA_PAUSE;
        PIC_RemoveEvents(END_DMA_Event);
        PIC_RemoveEvents(DMA_DAC_Event);
        break;
    case 0xd1:  /* Enable Speaker */
        sb.chan->FillUp();
        DSP_SetSpeaker(true);
        break;
    case 0xd3:  /* Disable Speaker */
        sb.chan->FillUp();
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
        if (sb.type == SBT_16 && sb.mode == MODE_DMA)
            LOG(LOG_MISC,LOG_WARN)("SB16 warning: DSP Disable Speaker command used while DMA is running, which has no effect on audio output on SB16 hardware. Audible noise/static may occur. You can eliminate the noise by setting sbtype=sbpro2");

        break;
    case 0xd8:  /* Speaker status */
        DSP_SB2_ABOVE;
        DSP_FlushData();
        if (sb.speaker) DSP_AddData(0xff);
        else DSP_AddData(0x00);
        break;
    case 0xd6:  /* Continue DMA 16-bit */
        DSP_SB16_ONLY;
    case 0xd4:  /* Continue DMA 8-bit*/
        sb.chan->FillUp();
        if (sb.mode==MODE_DMA_PAUSE) {
            sb.mode=MODE_DMA_MASKED;
            if (sb.dma.chan!=NULL) sb.dma.chan->Register_Callback(DSP_DMA_CallBack);
        }
        break;
    case 0x47:  /* Continue Autoinitialize 16-bit */
    case 0x45:  /* Continue Autoinitialize 8-bit */
        DSP_SB16_ONLY;
        sb.chan->FillUp();
        sb.dma.autoinit=true; // No. This DSP command does not resume DMA playback
        break;
    case 0xd9:  /* Exit Autoinitialize 16-bit */
        DSP_SB16_ONLY;
    case 0xda:  /* Exit Autoinitialize 8-bit */
        DSP_SB2_ABOVE;
        /* Set mode to single transfer so it ends with current block */
        sb.dma.autoinit=false;      //Should stop itself
        sb.chan->FillUp();
        break;
    case 0xe0:  /* DSP Identification - SB2.0+ */
        DSP_FlushData();
        DSP_AddData(~sb.dsp.in.data[0]);
        break;
    case 0xe1:  /* Get DSP Version */
        DSP_FlushData();
        switch (sb.type) {
        case SBT_1:
            DSP_AddData(0x1);DSP_AddData(0x05);break;
        case SBT_2:
            DSP_AddData(0x2);DSP_AddData(0x1);break;
        case SBT_PRO1:
            DSP_AddData(0x3);DSP_AddData(0x0);break;
        case SBT_PRO2:
            if (sb.ess_type != ESS_NONE) {
                DSP_AddData(0x3);DSP_AddData(0x1);
            }
            else if (sb.reveal_sc_type == RSC_SC400) { // SC400 cards report as v3.5 by default, but there is a DSP command to change the version!
                DSP_AddData(sb.sc400_dsp_major);DSP_AddData(sb.sc400_dsp_minor);
            }
            else {
                DSP_AddData(0x3);DSP_AddData(0x2);
            }
            break;
        case SBT_16:
            if (sb.vibra) {
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
            sb.e2.valadd += sb.dsp.in.data[0] ^ sb.e2.valxor;
            sb.e2.valxor = (sb.e2.valxor >> 2u) | (sb.e2.valxor << 6u);
            GetDMAChannel(sb.hw.dma8)->Register_Callback(DSP_E2_DMA_CallBack);
        }
        break;
    case 0xe3:  /* DSP Copyright */
        {
            DSP_FlushData();
            if (sb.ess_type != ESS_NONE) {
                /* ESS chips do not return copyright string */
                DSP_AddData(0);
            }
            else if (sb.reveal_sc_type == RSC_SC400) {
                static const char *gallant = "SC-6000";

                /* NTS: Yes, this writes the terminating NUL as well. Not a bug. */
                for (size_t i=0;i<=strlen(gallant);i++) {
                    DSP_AddData((Bit8u)gallant[i]);
                }
            }
            else if (sb.type <= SBT_PRO2) {
                /* Sound Blaster DSP 2.0: No copyright string observed. */
                /* Sound Blaster Pro DSP 3.1: No copyright string observed. */
                /* I have yet to observe if a Sound Blaster Pro DSP 3.2 (SBT_PRO2) returns a copyright string. */
                /* no response */
            }
            else {
                /* NTS: Yes, this writes the terminating NUL as well. Not a bug. */
                for (size_t i=0;i<=strlen(copyright_string);i++) {
                    DSP_AddData((Bit8u)copyright_string[i]);
                }
            }
        }
        break;
    case 0xe4:  /* Write Test Register */
        sb.dsp.test_register=sb.dsp.in.data[0];
        break;
    case 0xe7:  /* ESS detect/read config */
        if (sb.ess_type != ESS_NONE) {
            DSP_FlushData();
            DSP_AddData(0x68);
            DSP_AddData(0x80 | 0x04/*ESS 688 version*/);
        }
        break;
    case 0xe8:  /* Read Test Register */
        DSP_FlushData();
        DSP_AddData(sb.dsp.test_register);
        break;
    case 0xf2:  /* Trigger 8bit IRQ */
        //Small delay in order to emulate the slowness of the DSP, fixes Llamatron 2012 and Lemmings 3D
        PIC_AddEvent(&DSP_RaiseIRQEvent,0.01f); 
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
        LOG(LOG_SB,LOG_ERROR)("DSP:Unimplemented MIDI I/O command %2X",sb.dsp.cmd);
        break;
    case 0x34: /* MIDI Read Poll & Write Poll */
        DSP_SB2_ABOVE;
        LOG(LOG_SB,LOG_DEBUG)("DSP:Entering MIDI Read/Write Poll mode");
        sb.dsp.midi_rwpoll_mode = true;
        break;
    case 0x35: /* MIDI Read Interrupt & Write Poll */
        DSP_SB2_ABOVE;
        LOG(LOG_SB,LOG_DEBUG)("DSP:Entering MIDI Read Interrupt/Write Poll mode");
        sb.dsp.midi_rwpoll_mode = true;
        sb.dsp.midi_read_interrupt = true;
        break;
    case 0x37: /* MIDI Read Timestamp Interrupt & Write Poll */
        DSP_SB2_ABOVE;
        LOG(LOG_SB,LOG_DEBUG)("DSP:Entering MIDI Read Timstamp Interrupt/Write Poll mode");
        sb.dsp.midi_rwpoll_mode = true;
        sb.dsp.midi_read_interrupt = true;
        sb.dsp.midi_read_with_timestamps = true;
        break;
    case 0x20:
        DSP_AddData(0x7f);   // fake silent input for Creative parrot
        break;
    case 0x2c:
    case 0x98: case 0x99: /* Documented only for DSP 2.x and 3.x */
    case 0xa0: case 0xa8: /* Documented only for DSP 3.x */
        LOG(LOG_SB,LOG_ERROR)("DSP:Unimplemented input command %2X",sb.dsp.cmd);
        break;
    case 0x88: /* Reveal SC400 ??? (used by TESTSC.EXE) */
        if (sb.reveal_sc_type != RSC_SC400) break;
        /* ??? */
        break;
    case 0xE6: /* Reveal SC400 DMA test */
        if (sb.reveal_sc_type != RSC_SC400) break;
        GetDMAChannel(sb.hw.dma8)->Register_Callback(DSP_SC400_E6_DMA_CallBack);
        sb.dsp.out.lastval = 0x80;
        sb.dsp.out.used = 0;
        break;
    case 0x50: /* Reveal SC400 write configuration */
        if (sb.reveal_sc_type != RSC_SC400) break;
        sb.sc400_cfg = sb.dsp.in.data[0];

        switch (sb.dsp.in.data[0]&3) {
            case 0: sb.hw.dma8 = (Bit8u)(-1); break;
            case 1: sb.hw.dma8 =  0u; break;
            case 2: sb.hw.dma8 =  1u; break;
            case 3: sb.hw.dma8 =  3u; break;
        }
        sb.hw.dma16 = sb.hw.dma8;
        switch ((sb.dsp.in.data[0]>>3)&7) {
            case 0: sb.hw.irq =  (Bit8u)(-1); break;
            case 1: sb.hw.irq =  7u; break;
            case 2: sb.hw.irq =  9u; break;
            case 3: sb.hw.irq = 10u; break;
            case 4: sb.hw.irq = 11u; break;
            case 5: sb.hw.irq =  5u; break;
            case 6: sb.hw.irq =  (Bit8u)(-1); break;
            case 7: sb.hw.irq =  (Bit8u)(-1); break;
        }
        {
            int irq;

            if (sb.dsp.in.data[0]&0x04) /* MPU IRQ enable bit */
                irq = (sb.dsp.in.data[0]&0x80) ? 9 : 5;
            else
                irq = -1;

            if (irq != MPU401_GetIRQ())
                LOG(LOG_SB,LOG_WARN)("SC400 warning: MPU401 emulation does not yet support changing the IRQ through configuration commands");
        }

        LOG(LOG_SB,LOG_DEBUG)("SC400: New configuration byte %02x irq=%d dma=%d",
            sb.dsp.in.data[0],(int)sb.hw.irq,(int)sb.hw.dma8);

        break;
    case 0x58: /* Reveal SC400 read configuration */
        if (sb.reveal_sc_type != RSC_SC400) break;
        DSP_AddData(sb.sc400_jumper_status_1);
        DSP_AddData(sb.sc400_jumper_status_2);
        DSP_AddData(sb.sc400_cfg);
        break;
    case 0x6E: /* Reveal SC400 SBPRO.EXE set DSP version */
        if (sb.reveal_sc_type != RSC_SC400) break;
        sb.sc400_dsp_major = sb.dsp.in.data[0];
        sb.sc400_dsp_minor = sb.dsp.in.data[1];
        LOG(LOG_SB,LOG_DEBUG)("SC400: DSP version changed to %u.%u",
            sb.sc400_dsp_major,sb.sc400_dsp_minor);
        break;
    case 0xfa:  /* SB16 8051 memory write */
        if (sb.type == SBT_16) {
            sb16_8051_mem[sb.dsp.in.data[0]] = sb.dsp.in.data[1];
            LOG(LOG_SB,LOG_NORMAL)("SB16 8051 memory write %x byte %x",sb.dsp.in.data[0],sb.dsp.in.data[1]);
        } else {
            LOG(LOG_SB,LOG_ERROR)("DSP:Unhandled (undocumented) command %2X",sb.dsp.cmd);
        }
        break;
    case 0xf9:  /* SB16 8051 memory read */
        if (sb.type == SBT_16) {
            DSP_AddData(sb16_8051_mem[sb.dsp.in.data[0]]);
            LOG(LOG_SB,LOG_NORMAL)("SB16 8051 memory read %x, got byte %x",sb.dsp.in.data[0],sb16_8051_mem[sb.dsp.in.data[0]]);
        } else {
            LOG(LOG_SB,LOG_ERROR)("DSP:Unhandled (undocumented) command %2X",sb.dsp.cmd);
        }
        break;
    default:
        LOG(LOG_SB,LOG_ERROR)("DSP:Unhandled (undocumented) command %2X",sb.dsp.cmd);
        break;
    }

    if (sb.type == SBT_16) {
        // FIXME: This is a guess! See also [https://github.com/joncampbell123/dosbox-x/issues/1044#issuecomment-480024957]
        sb16_8051_mem[0x30] = sb.dsp.last_cmd; /* last_cmd */
    }

    sb.dsp.last_cmd=sb.dsp.cmd;
    sb.dsp.cmd=DSP_NO_COMMAND;
    sb.dsp.cmd_len=0;
    sb.dsp.in.pos=0;
}

static bool DSP_busy_cycle_active() {
    /* NTS: Busy cycle happens on SB16 at all times, or on earlier cards, only when the DSP is
     *      fetching/writing data via the ISA DMA channel. So a non-auto-init DSP block that's
     *      just finished fetching ISA DMA and is playing from the FIFO doesn't count.
     *
     *      sb.dma.left >= sb.dma.min condition causes busy cycle to stop 3ms early (by default).
     *      This helps realism.
     *
     *      This also helps Crystal Dream, which uses the busy cycle to detect when the Sound
     *      Blaster is about to finish playing the DSP block and therefore needs the same 3ms
     *      "dmamin" hack to reissue another playback command without any audible hiccups in
     *      the audio. */
    return (sb.mode == MODE_DMA && (sb.dma.autoinit || sb.dma.left >= sb.dma.min)) || sb.busy_cycle_always;
}

static bool DSP_busy_cycle() {
    double now;
    int t;

    if (!DSP_busy_cycle_active()) return false;
    if (sb.busy_cycle_duty_percent <= 0 || sb.busy_cycle_hz <= 0) return false;

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
     *      This isn't 100% accurate, but it's the best DOSBox-X can do for now to mimick
     *      SB16 DSP behavior. */

    now = PIC_FullIndex();
    if (now >= (sb.busy_cycle_last_check+0.02/*ms*/))
        sb.busy_cycle_io_hack = (int)(fmod((now / 1000) * sb.busy_cycle_hz,1.0) * 16);

    sb.busy_cycle_last_check = now;
    t = ((sb.busy_cycle_io_hack % 16) * 100) / 16; /* HACK: DOSBox's I/O is not quite ISA bus speeds or related to it */
    if (t < sb.busy_cycle_duty_percent) return true;
    return false;
}

static void DSP_DoWrite(Bit8u val) {
    if (sb.dsp.write_busy || (sb.dsp.highspeed && sb.type != SBT_16 && sb.ess_type == ESS_NONE && sb.reveal_sc_type == RSC_NONE)) {
        LOG(LOG_SB,LOG_WARN)("DSP:Command write %2X ignored, DSP not ready. DOS game or OS is not polling status",val);
        return;
    }

    /* NTS: We allow the user to set busy wait time == 0 aka "instant gratification mode".
     *      We also assume that if they do that, some DOS programs might be timing sensitive
     *      enough to freak out when DSP commands and data are accepted immediately */
    {
        unsigned int delay = sb.dsp.dsp_write_busy_time;

        if (sb.dsp.instant_direct_dac) {
            delay = 0;
        }
        /* Part of enforcing sample rate limits is to make sure to emulate that the
         * Direct DAC output command 0x10 is "busy" long enough to effectively rate
         * limit output to 23KHz. */
        else if (sb.sample_rate_limits) {
            unsigned int limit = 23000; /* documented max sample rate for SB16/SBPro and earlier */

            if (sb.type == SBT_16 && sb.vibra)
                limit = 23000; /* DSP maxes out at 46KHz not 44.1KHz on ViBRA cards */

            if (sb.dsp.cmd == DSP_NO_COMMAND && val == 0x10/*DSP direct DAC, command*/)
                delay = (625000000UL / limit) - sb.dsp.dsp_write_busy_time;
        }

        if (delay > 0) {
            sb.dsp.write_busy = 1;
            PIC_RemoveEvents(DSP_BusyComplete);
            PIC_AddEvent(DSP_BusyComplete,(double)delay / 1000000);
        }

//      LOG(LOG_SB,LOG_NORMAL)("DSP:Command %02x delay %u",val,delay);
    }

    if (sb.dsp.midi_rwpoll_mode) {
        // DSP writes in this mode go to the MIDI port
//      LOG(LOG_SB,LOG_DEBUG)("DSP MIDI read/write poll mode: sending 0x%02x",val);
        if (sb.midi == true) MIDI_RawOutByte(val);
        return;
    }

    switch (sb.dsp.cmd) {
        case DSP_NO_COMMAND:
            sb.dsp.cmd=val;
            if (sb.type == SBT_16)
                sb.dsp.cmd_len=DSP_cmd_len_sb16[val];
            else if (sb.ess_type != ESS_NONE) 
                sb.dsp.cmd_len=DSP_cmd_len_ess[val];
            else if (sb.reveal_sc_type != RSC_NONE)
                sb.dsp.cmd_len=DSP_cmd_len_sc400[val];
            else
                sb.dsp.cmd_len=DSP_cmd_len_sb[val];

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
static float calc_vol(Bit8u amount) {
    Bit8u count = 31 - amount;
    float db = static_cast<float>(count);
    if (sb.type == SBT_PRO1 || sb.type == SBT_PRO2) {
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
static void CTMIXER_UpdateVolumes(void) {
    if (!sb.mixer.enabled) return;

    sb.chan->FillUp();

    MixerChannel * chan;
    float m0 = calc_vol(sb.mixer.master[0]);
    float m1 = calc_vol(sb.mixer.master[1]);
    chan = MIXER_FindChannel("SB");
    if (chan) chan->SetVolume(m0 * calc_vol(sb.mixer.dac[0]), m1 * calc_vol(sb.mixer.dac[1]));
    chan = MIXER_FindChannel("FM");
    if (chan) chan->SetVolume(m0 * calc_vol(sb.mixer.fm[0]) , m1 * calc_vol(sb.mixer.fm[1]) );
    chan = MIXER_FindChannel("CDAUDIO");
    if (chan) chan->SetVolume(m0 * calc_vol(sb.mixer.cda[0]), m1 * calc_vol(sb.mixer.cda[1]));
}

static void CTMIXER_Reset(void) {
    sb.mixer.filtered=0; // Creative Documentation: filtered bit 0 by default
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

#define SETPROVOL(_WHICH_,_VAL_)                                        \
    _WHICH_[0]=   ((((_VAL_) & 0xf0) >> 3)|(sb.type==SBT_16 ? 1:3));    \
    _WHICH_[1]=   ((((_VAL_) & 0x0f) << 1)|(sb.type==SBT_16 ? 1:3));    \

#define MAKEPROVOL(_WHICH_)											\
	((((_WHICH_[0] & 0x1e) << 3) | ((_WHICH_[1] & 0x1e) >> 1)) |	\
		((sb.type==SBT_PRO1 || sb.type==SBT_PRO2) ? 0x11:0))

// TODO: Put out the various hardware listed here, do some listening tests to confirm the emulation is accurate.
void updateSoundBlasterFilter(Bitu rate) {
    /* "No filtering" option for those who don't want it, or are used to the way things sound in plain vanilla DOSBox */
    if (sb.no_filtering) {
        sb.chan->SetLowpassFreq(0/*off*/);
        sb.chan->SetSlewFreq(0/*normal linear interpolation*/);
        return;
    }

    /* different sound cards filter their output differently */
    if (sb.ess_type != ESS_NONE) { // ESS AudioDrive. Tested against real hardware (ESS 688) by Jonathan C.
        /* ESS AudioDrive lets the driver decide what the cutoff/rolloff to use */
        /* "The ratio of the roll-off frequency to the clock frequency is 1:82. In other words,
         * first determine the desired roll off frequency by taking 80% of the sample rate
         * divided by 2, the multiply by 82 to find the desired filter clock frequency"
         *
         * Try to approximate the ESS's filter by undoing the calculation then feeding our own lowpass filter with it.
         *
         * This implementation is matched aginst real hardware by ear, even though the reference hardware is a
         * laptop with a cheap tinny amplifier */
        Bit64u filter_raw = (Bit64u)7160000ULL / (256u - ESSreg(0xA2));
        Bit64u filter_hz = (filter_raw * (Bit64u)11) / (Bit64u)(82 * 4); /* match lowpass by ear compared to real hardware */

        if ((filter_hz * 2) > sb.freq)
            sb.chan->SetSlewFreq(filter_hz * 2 * sb.chan->freq_d_orig);
        else
            sb.chan->SetSlewFreq(0);

        sb.chan->SetLowpassFreq(filter_hz,/*order*/8);
    }
    else if (sb.type == SBT_16 || // Sound Blaster 16 (DSP 4.xx). Tested against real hardware (CT4180 ViBRA 16C PnP) by Jonathan C.
        sb.reveal_sc_type == RSC_SC400) { // Reveal SC400 (DSP 3.5). Tested against real hardware by Jonathan C.
        // My notes: The DSP automatically applies filtering at low sample rates. But the DSP has to know
        //           what the sample rate is to filter. If you use direct DAC output (DSP command 0x10)
        //           then no filtering is applied and the sound comes out grungy, just like older Sound
        //           Blaster cards.
        //
        //           I can also confirm the SB16's reputation for hiss and noise is true, it's noticeable
        //           with earbuds and the mixer volume at normal levels. --Jonathan C.
        if (sb.mode == MODE_DAC) {
            sb.chan->SetLowpassFreq(23000);
            sb.chan->SetSlewFreq(23000 * sb.chan->freq_d_orig);
        }
        else {
            sb.chan->SetLowpassFreq(rate/2,1);
            sb.chan->SetSlewFreq(0/*normal linear interpolation*/);
        }
    }
    else if (sb.type == SBT_PRO1 || sb.type == SBT_PRO2) { // Sound Blaster Pro (DSP 3.x). Tested against real hardware (CT1600) by Jonathan C.
        sb.chan->SetSlewFreq(23000 * sb.chan->freq_d_orig);
        if (sb.mixer.filtered/*setting the bit means to bypass the lowpass filter*/)
            sb.chan->SetLowpassFreq(23000); // max sample rate 46000Hz. slew rate filter does the rest of the filtering for us.
        else
            sb.chan->SetLowpassFreq(3800); // NOT documented by Creative, guess based on listening tests with a CT1600, and documented Input filter freqs
    }
    else if (sb.type == SBT_1 || sb.type == SBT_2) { // Sound Blaster DSP 1.x and 2.x (not Pro). Tested against real hardware (CT1350B) by Jonathan C.
        /* As far as I can tell the DAC outputs sample-by-sample with no filtering whatsoever, aside from the limitations of analog audio */
        sb.chan->SetSlewFreq(23000 * sb.chan->freq_d_orig);
        sb.chan->SetLowpassFreq(23000);
    }
}

static void DSP_ChangeStereo(bool stereo) {
    if (!sb.dma.stereo && stereo) {
        sb.chan->SetFreq(sb.freq/2);
        updateSoundBlasterFilter(sb.freq/2);
        sb.dma.mul*=2;
        sb.dma.rate=(sb.freq*sb.dma.mul) >> SB_SH;
        sb.dma.min=((Bitu)sb.dma.rate*(Bitu)(sb.min_dma_user >= 0 ? sb.min_dma_user : /*default*/3))/1000u;
    } else if (sb.dma.stereo && !stereo) {
        sb.chan->SetFreq(sb.freq);
        updateSoundBlasterFilter(sb.freq);
        sb.dma.mul/=2;
        sb.dma.rate=(sb.freq*sb.dma.mul) >> SB_SH;
        sb.dma.min=((Bitu)sb.dma.rate*(Bitu)(sb.min_dma_user >= 0 ? sb.min_dma_user : /*default*/3))/1000;
    }
    sb.dma.stereo=stereo;
}

static inline uint8_t expand16to32(const uint8_t t) {
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

static unsigned char pc98_mixctl_reg = 0x14;

static void CTMIXER_Write(Bit8u val) {
    switch (sb.mixer.index) {
    case 0x00:      /* Reset */
        CTMIXER_Reset();
        LOG(LOG_SB,LOG_WARN)("Mixer reset value %x",val);
        break;
    case 0x02:      /* Master Volume (SB2 Only) */
        SETPROVOL(sb.mixer.master,(val&0xf)|(val<<4));
        CTMIXER_UpdateVolumes();
        break;
    case 0x04:      /* DAC Volume (SBPRO) */
        SETPROVOL(sb.mixer.dac,val);
        CTMIXER_UpdateVolumes();
        break;
    case 0x06:      /* FM output selection, Somewhat obsolete with dual OPL SBpro + FM volume (SB2 Only) */
        //volume controls both channels
        SETPROVOL(sb.mixer.fm,(val&0xf)|(val<<4));
        CTMIXER_UpdateVolumes();
        if(val&0x60) LOG(LOG_SB,LOG_WARN)("Turned FM one channel off. not implemented %X",val);
        //TODO Change FM Mode if only 1 fm channel is selected
        break;
    case 0x08:      /* CDA Volume (SB2 Only) */
        SETPROVOL(sb.mixer.cda,(val&0xf)|(val<<4));
        CTMIXER_UpdateVolumes();
        break;
    case 0x0a:      /* Mic Level (SBPRO) or DAC Volume (SB2): 2-bit, 3-bit on SB16 */
        if (sb.type==SBT_2) {
            sb.mixer.dac[0]=sb.mixer.dac[1]=((val & 0x6) << 2)|3;
            CTMIXER_UpdateVolumes();
        } else {
            sb.mixer.mic=((val & 0x7) << 2)|(sb.type==SBT_16?1:3);
        }
        break;
    case 0x0e:      /* Output/Stereo Select */
        /* only allow writing stereo bit if Sound Blaster Pro OR if a SB16 and user says to allow it */
        if ((sb.type == SBT_PRO1 || sb.type == SBT_PRO2) || (sb.type == SBT_16 && !sb.sbpro_stereo_bit_strict_mode))
            sb.mixer.stereo=(val & 0x2) > 0;

        sb.mixer.sbpro_stereo=(val & 0x2) > 0;
        sb.mixer.filtered=(val & 0x20) > 0;
        DSP_ChangeStereo(sb.mixer.stereo);
        updateSoundBlasterFilter(sb.freq);

        /* help the user out if they leave sbtype=sb16 and then wonder why their DOS game is producing scratchy monural audio. */
        if (sb.type == SBT_16 && sb.sbpro_stereo_bit_strict_mode && (val&0x2) != 0)
            LOG(LOG_SB,LOG_WARN)("Mixer stereo/mono bit is set. This is Sound Blaster Pro style Stereo which is not supported by sbtype=sb16, consider using sbtype=sbpro2 instead.");
        break;
    case 0x14:      /* Audio 1 Play Volume (ESS 688) */
        if (sb.ess_type != ESS_NONE) {
            sb.mixer.dac[0]=expand16to32((val>>4)&0xF);
            sb.mixer.dac[1]=expand16to32(val&0xF);
            CTMIXER_UpdateVolumes();
        }
        break;
    case 0x22:      /* Master Volume (SBPRO) */
        SETPROVOL(sb.mixer.master,val);
        CTMIXER_UpdateVolumes();
        break;
    case 0x26:      /* FM Volume (SBPRO) */
        SETPROVOL(sb.mixer.fm,val);
        CTMIXER_UpdateVolumes();
        break;
    case 0x28:      /* CD Audio Volume (SBPRO) */
        SETPROVOL(sb.mixer.cda,val);
        CTMIXER_UpdateVolumes();
        break;
    case 0x2e:      /* Line-in Volume (SBPRO) */
        SETPROVOL(sb.mixer.lin,val);
        break;
    //case 0x20:        /* Master Volume Left (SBPRO) ? */
    case 0x30:      /* Master Volume Left (SB16) */
        if (sb.type==SBT_16) {
            sb.mixer.master[0]=val>>3;
            CTMIXER_UpdateVolumes();
        }
        break;
    //case 0x21:        /* Master Volume Right (SBPRO) ? */
    case 0x31:      /* Master Volume Right (SB16) */
        if (sb.type==SBT_16) {
            sb.mixer.master[1]=val>>3;
            CTMIXER_UpdateVolumes();
        }
        break;
    case 0x32:      /* DAC Volume Left (SB16) */
                /* Master Volume (ESS 688) */
        if (sb.type==SBT_16) {
            sb.mixer.dac[0]=val>>3;
            CTMIXER_UpdateVolumes();
        }
        else if (sb.ess_type != ESS_NONE) {
            sb.mixer.master[0]=expand16to32((val>>4)&0xF);
            sb.mixer.master[1]=expand16to32(val&0xF);
            CTMIXER_UpdateVolumes();
        }
        break;
    case 0x33:      /* DAC Volume Right (SB16) */
        if (sb.type==SBT_16) {
            sb.mixer.dac[1]=val>>3;
            CTMIXER_UpdateVolumes();
        }
        break;
    case 0x34:      /* FM Volume Left (SB16) */
        if (sb.type==SBT_16) {
            sb.mixer.fm[0]=val>>3;
            CTMIXER_UpdateVolumes();
        }
                break;
    case 0x35:      /* FM Volume Right (SB16) */
        if (sb.type==SBT_16) {
            sb.mixer.fm[1]=val>>3;
            CTMIXER_UpdateVolumes();
        }
        break;
    case 0x36:      /* CD Volume Left (SB16) */
                /* FM Volume (ESS 688) */
        if (sb.type==SBT_16) {
            sb.mixer.cda[0]=val>>3;
            CTMIXER_UpdateVolumes();
        }
        else if (sb.ess_type != ESS_NONE) {
            sb.mixer.fm[0]=expand16to32((val>>4)&0xF);
            sb.mixer.fm[1]=expand16to32(val&0xF);
            CTMIXER_UpdateVolumes();
        }
        break;
    case 0x37:      /* CD Volume Right (SB16) */
        if (sb.type==SBT_16) {
            sb.mixer.cda[1]=val>>3;
            CTMIXER_UpdateVolumes();
        }
        break;
    case 0x38:      /* Line-in Volume Left (SB16) */
                /* AuxA (CD) Volume Register (ESS 688) */
        if (sb.type==SBT_16)
            sb.mixer.lin[0]=val>>3;
        else if (sb.ess_type != ESS_NONE) {
            sb.mixer.cda[0]=expand16to32((val>>4)&0xF);
            sb.mixer.cda[1]=expand16to32(val&0xF);
            CTMIXER_UpdateVolumes();
        }
        break;
    case 0x39:      /* Line-in Volume Right (SB16) */
        if (sb.type==SBT_16) sb.mixer.lin[1]=val>>3;
        break;
    case 0x3a:
        if (sb.type==SBT_16) sb.mixer.mic=val>>3;
        break;
    case 0x3e:      /* Line Volume (ESS 688) */
        if (sb.ess_type != ESS_NONE) {
            sb.mixer.lin[0]=expand16to32((val>>4)&0xF);
            sb.mixer.lin[1]=expand16to32(val&0xF);
        }
        break;
    case 0x80:      /* IRQ Select */
        if (sb.type==SBT_16 && !sb.vibra) { /* ViBRA PnP cards do not allow reconfiguration by this byte */
            sb.hw.irq=0xff;
            if (IS_PC98_ARCH) {
                if (val & 0x1) sb.hw.irq=3;
                else if (val & 0x2) sb.hw.irq=10;
                else if (val & 0x4) sb.hw.irq=12;
                else if (val & 0x8) sb.hw.irq=5;

                // NTS: Real hardware stores only the low 4 bits. The upper 4 bits will always read back 1111.
                //      The value read back will always be Fxh where x contains the 4 bits checked here.
            }
            else {
                if (val & 0x1) sb.hw.irq=2;
                else if (val & 0x2) sb.hw.irq=5;
                else if (val & 0x4) sb.hw.irq=7;
                else if (val & 0x8) sb.hw.irq=10;
            }
        }
        break;
    case 0x81:      /* DMA Select */
        if (sb.type==SBT_16 && !sb.vibra) { /* ViBRA PnP cards do not allow reconfiguration by this byte */
            sb.hw.dma8=0xff;
            sb.hw.dma16=0xff;
            if (IS_PC98_ARCH) {
                pc98_mixctl_reg = (unsigned char)val ^ 0x14;

                if (val & 0x1) sb.hw.dma8=0;
                else if (val & 0x2) sb.hw.dma8=3;

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
                if (val & 0x1) sb.hw.dma8=0;
                else if (val & 0x2) sb.hw.dma8=1;
                else if (val & 0x8) sb.hw.dma8=3;
                if (val & 0x20) sb.hw.dma16=5;
                else if (val & 0x40) sb.hw.dma16=6;
                else if (val & 0x80) sb.hw.dma16=7;
            }
            LOG(LOG_SB,LOG_NORMAL)("Mixer select dma8:%x dma16:%x",sb.hw.dma8,sb.hw.dma16);
        }
        break;
    default:

        if( ((sb.type == SBT_PRO1 || sb.type == SBT_PRO2) && sb.mixer.index==0x0c) || /* Input control on SBPro */
             (sb.type == SBT_16 && sb.mixer.index >= 0x3b && sb.mixer.index <= 0x47)) /* New SB16 registers */
            sb.mixer.unhandled[sb.mixer.index] = val;
        LOG(LOG_SB,LOG_WARN)("MIXER:Write %X to unhandled index %X",val,sb.mixer.index);
    }
}

static Bit8u CTMIXER_Read(void) {
    Bit8u ret = 0;

//  if ( sb.mixer.index< 0x80) LOG_MSG("Read mixer %x",sb.mixer.index);
    switch (sb.mixer.index) {
    case 0x00:      /* RESET */
        return 0x00;
    case 0x02:      /* Master Volume (SB2 Only) */
        return ((sb.mixer.master[1]>>1) & 0xe);
    case 0x22:      /* Master Volume (SBPRO) */
        return  MAKEPROVOL(sb.mixer.master);
    case 0x04:      /* DAC Volume (SBPRO) */
        return MAKEPROVOL(sb.mixer.dac);
    case 0x06:      /* FM Volume (SB2 Only) + FM output selection */
        return ((sb.mixer.fm[1]>>1) & 0xe);
    case 0x08:      /* CD Volume (SB2 Only) */
        return ((sb.mixer.cda[1]>>1) & 0xe);
    case 0x0a:      /* Mic Level (SBPRO) or Voice (SB2 Only) */
        if (sb.type==SBT_2) return (sb.mixer.dac[0]>>2);
        else return ((sb.mixer.mic >> 2) & (sb.type==SBT_16 ? 7:6));
    case 0x0e:      /* Output/Stereo Select */
        return 0x11|(sb.mixer.stereo ? 0x02 : 0x00)|(sb.mixer.filtered ? 0x20 : 0x00);
    case 0x14:      /* Audio 1 Play Volume (ESS 688) */
        if (sb.ess_type != ESS_NONE) return ((sb.mixer.dac[0] << 3) & 0xF0) + (sb.mixer.dac[1] >> 1);
        break;
    case 0x26:      /* FM Volume (SBPRO) */
        return MAKEPROVOL(sb.mixer.fm);
    case 0x28:      /* CD Audio Volume (SBPRO) */
        return MAKEPROVOL(sb.mixer.cda);
    case 0x2e:      /* Line-IN Volume (SBPRO) */
        return MAKEPROVOL(sb.mixer.lin);
    case 0x30:      /* Master Volume Left (SB16) */
        if (sb.type==SBT_16) return sb.mixer.master[0]<<3;
        ret=0xa;
        break;
    case 0x31:      /* Master Volume Right (S16) */
        if (sb.type==SBT_16) return sb.mixer.master[1]<<3;
        ret=0xa;
        break;
    case 0x32:      /* DAC Volume Left (SB16) */
                /* Master Volume (ESS 688) */
        if (sb.type==SBT_16) return sb.mixer.dac[0]<<3;
        if (sb.ess_type != ESS_NONE) return ((sb.mixer.master[0] << 3) & 0xF0) + (sb.mixer.master[1] >> 1);
        ret=0xa;
        break;
    case 0x33:      /* DAC Volume Right (SB16) */
        if (sb.type==SBT_16) return sb.mixer.dac[1]<<3;
        ret=0xa;
        break;
    case 0x34:      /* FM Volume Left (SB16) */
        if (sb.type==SBT_16) return sb.mixer.fm[0]<<3;
        ret=0xa;
        break;
    case 0x35:      /* FM Volume Right (SB16) */
        if (sb.type==SBT_16) return sb.mixer.fm[1]<<3;
        ret=0xa;
        break;
    case 0x36:      /* CD Volume Left (SB16) */
                /* FM Volume (ESS 688) */
        if (sb.type==SBT_16) return sb.mixer.cda[0]<<3;
        if (sb.ess_type != ESS_NONE) return ((sb.mixer.fm[0] << 3) & 0xF0) + (sb.mixer.fm[1] >> 1);
        ret=0xa;
        break;
    case 0x37:      /* CD Volume Right (SB16) */
        if (sb.type==SBT_16) return sb.mixer.cda[1]<<3;
        ret=0xa;
        break;
    case 0x38:      /* Line-in Volume Left (SB16) */
                /* AuxA (CD) Volume Register (ESS 688) */
        if (sb.type==SBT_16) return sb.mixer.lin[0]<<3;
        if (sb.ess_type != ESS_NONE) return ((sb.mixer.cda[0] << 3) & 0xF0) + (sb.mixer.cda[1] >> 1);
        ret=0xa;
        break;
    case 0x39:      /* Line-in Volume Right (SB16) */
        if (sb.type==SBT_16) return sb.mixer.lin[1]<<3;
        ret=0xa;
        break;
    case 0x3a:      /* Mic Volume (SB16) */
        if (sb.type==SBT_16) return sb.mixer.mic<<3;
        ret=0xa;
        break;
    case 0x3e:      /* Line Volume (ESS 688) */
        if (sb.ess_type != ESS_NONE) return ((sb.mixer.lin[0] << 3) & 0xF0) + (sb.mixer.lin[1] >> 1);
        break;
    case 0x80:      /* IRQ Select */
        ret=0;
        if (IS_PC98_ARCH) {
            switch (sb.hw.irq) {
                case 3:  return 0xF1; // upper 4 bits always 1111
                case 10: return 0xF2;
                case 12: return 0xF4;
                case 5:  return 0xF8;
            }
        }
        else {
            switch (sb.hw.irq) {
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
            switch (sb.hw.dma8) {
                case 0:ret|=0x1;break;
                case 3:ret|=0x2;break;
            }

            // there's some strange behavior on the PC-98 version of the card
            ret |= (pc98_mixctl_reg & (~3u));
        }
        else {
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
        }
        return ret;
    case 0x82:      /* IRQ Status */
        return  (sb.irq.pending_8bit ? 0x1 : 0) |
                (sb.irq.pending_16bit ? 0x2 : 0) | 
                ((sb.type == SBT_16) ? 0x20 : 0);
    default:
        if (    ((sb.type == SBT_PRO1 || sb.type == SBT_PRO2) && sb.mixer.index==0x0c) || /* Input control on SBPro */
            (sb.type == SBT_16 && sb.mixer.index >= 0x3b && sb.mixer.index <= 0x47)) /* New SB16 registers */
            ret = sb.mixer.unhandled[sb.mixer.index];
        else
            ret=0xa;
        LOG(LOG_SB,LOG_WARN)("MIXER:Read from unhandled index %X",sb.mixer.index);
    }

    return ret;
}


static Bitu read_sb(Bitu port,Bitu /*iolen*/) {
    if (!IS_PC98_ARCH) {
        /* All Creative hardware prior to Sound Blaster 16 appear to alias most of the I/O ports.
         * This has been confirmed on a Sound Blaster 2.0 and a Sound Blaster Pro (v3.1).
         * DSP aliasing is also faithfully emulated by the ESS AudioDrive. */
        if (sb.hw.sb_io_alias) {
            if ((port-sb.hw.base) == DSP_ACK_16BIT && sb.ess_type != ESS_NONE)
                { } /* ESS AudioDrive does not alias DSP STATUS (0x22E) as seen on real hardware */
            else if ((port-sb.hw.base) < MIXER_INDEX || (port-sb.hw.base) > MIXER_DATA)
                port &= ~1u;
        }
    }

    switch (((port-sb.hw.base) >> (IS_PC98_ARCH ? 8u : 0u)) & 0xFu) {
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

        if (sb.ess_type == ESS_NONE && (sb.type == SBT_1 || sb.type == SBT_2 || sb.type == SBT_PRO1 || sb.type == SBT_PRO2))
            return sb.dsp.out.used ? 0xAA : 0x2A; /* observed return values on SB 2.0---any significance? */
        else
            return sb.dsp.out.used ? 0xFF : 0x7F; /* normal return values */
        break;
    case DSP_ACK_16BIT:
        if (sb.ess_type == ESS_NONE && sb.type == SBT_16) {
            if (sb.irq.pending_16bit)  {
                sb.irq.pending_16bit=false;
                PIC_DeActivateIRQ(sb.hw.irq);
            }

            if (sb.mode == MODE_DMA_REQUIRE_IRQ_ACK)
                sb.mode = MODE_DMA;
        }
        break;
    case DSP_WRITE_STATUS:
        switch (sb.dsp.state) {
            /* FIXME: On a SB 2.0 card I own, the port will usually read either 0x2A or 0xAA,
             *        rather than 0x7F or 0xFF. Is there any significance to that? */
        case DSP_S_NORMAL: {
            bool busy = false;

            /* NTS: DSP "busy cycle" is independent of whether the DSP is actually
             *      busy (executing a command) or highspeed mode. On SB16 hardware,
             *      writing a DSP command during the busy cycle means that the command
             *      is remembered, but not acted on until the DSP leaves it's busy
             *      cycle. */
            sb.busy_cycle_io_hack++; /* NTS: busy cycle I/O timing hack! */
            if (DSP_busy_cycle())
                busy = true;
            else if (sb.dsp.write_busy || (sb.dsp.highspeed && sb.type != SBT_16 && sb.ess_type == ESS_NONE && sb.reveal_sc_type == RSC_NONE))
                busy = true;

            if (!sb.write_status_must_return_7f && sb.ess_type == ESS_NONE && (sb.type == SBT_2 || sb.type == SBT_PRO1 || sb.type == SBT_PRO2))
                return busy ? 0xAA : 0x2A; /* observed return values on SB 2.0---any significance? */
            else
                return busy ? 0xFF : 0x7F; /* normal return values */
            
            } break;
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

static void write_sb(Bitu port,Bitu val,Bitu /*iolen*/) {
    /* All Creative hardware prior to Sound Blaster 16 appear to alias most of the I/O ports.
     * This has been confirmed on a Sound Blaster 2.0 and a Sound Blaster Pro (v3.1).
     * DSP aliasing is also faithfully emulated by the ESS AudioDrive. */
    if (!IS_PC98_ARCH) {
        if (sb.hw.sb_io_alias) {
            if ((port-sb.hw.base) == DSP_ACK_16BIT && sb.ess_type != ESS_NONE)
                { } /* ESS AudioDrive does not alias DSP STATUS (0x22E) as seen on real hardware */
            else if ((port-sb.hw.base) < MIXER_INDEX || (port-sb.hw.base) > MIXER_DATA)
                port &= ~1u;
        }
    }

    Bit8u val8=(Bit8u)(val&0xff);
    switch (((port-sb.hw.base) >> (IS_PC98_ARCH ? 8u : 0u)) & 0xFu) {
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
        sb.mixer.index=val8;
        break;
    case MIXER_DATA:
        CTMIXER_Write(val8);
        break;
    default:
        LOG(LOG_SB,LOG_NORMAL)("Unhandled write to SB Port %4X",(int)port);
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
    sb.directdac_warn_speaker_off = true;

    switch (sb.mode) {
    case MODE_NONE:
    case MODE_DMA_PAUSE:
    case MODE_DMA_MASKED:
    case MODE_DMA_REQUIRE_IRQ_ACK:
        sb.chan->AddSilence();
        break;
    case MODE_DAC:
        sb.mode = MODE_NONE;
        break;
    case MODE_DMA:
        len*=sb.dma.mul;
        if (len&SB_SH_MASK) len+=1 << SB_SH;
        len>>=SB_SH;
        if (len>sb.dma.left) len=sb.dma.left;
        GenerateDMASound(len);
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
        ViBRA_PnP() : ISAPnPDevice() {
            resource_ident = 0;
            host_writed(ident+0,ISAPNP_ID('C','T','L',0x0,0x0,0x7,0x0)); /* CTL0070: ViBRA C */
            host_writed(ident+4,0xFFFFFFFFUL);
            checksum_ident();

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
            write_IO_Port(/*min*/0x330,/*max*/0x330,/*count*/0x02,/*align*/0x01);
            write_IO_Port(/*min*/0x388,/*max*/0x388,/*count*/0x04,/*align*/0x01);

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
            write_IO_Port(/*min*/0x300,/*max*/0x330,/*count*/0x02,/*align*/0x30);
            write_IO_Port(/*min*/0x388,/*max*/0x388,/*count*/0x04,/*align*/0x01);

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
            write_IO_Port(/*min*/0x300,/*max*/0x330,/*count*/0x02,/*align*/0x30);

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
            write_IO_Port(/*min*/0x300,/*max*/0x330,/*count*/0x02,/*align*/0x30);
            write_IO_Port(/*min*/0x388,/*max*/0x388,/*count*/0x04,/*align*/0x01);

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

            // NTS: DOSBox-X as coded now always has a joystick port at 0x201 even if no joystick
            write_Logical_Device_ID('C','T','L',0x7,0x0,0x0,0x1); // CTL7001
            write_Compatible_Device_ID('P','N','P',0xB,0x0,0x2,0xF); // PNPB02F
            write_Identifier_String("Game");
            write_IO_Port(/*min*/0x200,/*max*/0x200,/*count*/0x08);

            end_write_res();        // END
        }
        void select_logical_device(Bitu val) {
            logical_device = val;
        }
        uint8_t read(Bitu addr) {
            uint8_t ret = 0xFF;
            if (logical_device == 0) {
                switch (addr) {
                    case 0x60: case 0x61:   /* I/O [0] sound blaster */
                        ret = sb.hw.base >> ((addr & 1) ? 0 : 8);
                        break;
                    case 0x62: case 0x63:   /* I/O [1] MPU */
                        ret = 0x330 >> ((addr & 1) ? 0 : 8); /* FIXME: What I/O port really IS the MPU on? */
                        break;
                    case 0x64: case 0x65:   /* I/O [1] OPL-3 */
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
        void write(Bitu addr,Bitu val) {
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

bool JOYSTICK_IsEnabled(Bitu which);
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
        sb.ess_type = ESS_NONE;
        sb.reveal_sc_type = RSC_NONE;
        sb.ess_extended_mode = false;
        const char * sbtype=config->Get_string("sbtype");
        if (!strcasecmp(sbtype,"sb1")) type=SBT_1;
        else if (!strcasecmp(sbtype,"sb2")) type=SBT_2;
        else if (!strcasecmp(sbtype,"sbpro1")) type=SBT_PRO1;
        else if (!strcasecmp(sbtype,"sbpro2")) type=SBT_PRO2;
        else if (!strcasecmp(sbtype,"sb16vibra")) type=SBT_16;
        else if (!strcasecmp(sbtype,"sb16")) type=SBT_16;
        else if (!strcasecmp(sbtype,"gb")) type=SBT_GB;
        else if (!strcasecmp(sbtype,"none")) type=SBT_NONE;
        else if (!strcasecmp(sbtype,"ess688")) {
            type=SBT_PRO2;
            sb.ess_type=ESS_688;
            LOG(LOG_SB,LOG_DEBUG)("ESS 688 emulation enabled.");
            LOG(LOG_SB,LOG_WARN)("ESS 688 emulation is EXPERIMENTAL at this time and should not yet be used for normal gaming");
        }
        else if (!strcasecmp(sbtype,"reveal_sc400")) {
            type=SBT_PRO2;
            sb.reveal_sc_type=RSC_SC400;
            LOG(LOG_SB,LOG_DEBUG)("Reveal SC400 emulation enabled.");
            LOG(LOG_SB,LOG_WARN)("Reveal SC400 emulation is EXPERIMENTAL at this time and should not yet be used for normal gaming.");
            LOG(LOG_SB,LOG_WARN)("Additional WARNING: This code only emulates the Sound Blaster portion of the card. Attempting to use the Windows Sound System part of the card (i.e. the Voyetra/SC400 Windows drivers) will not work!");
        }
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
        if (!IS_PC98_ARCH) {
            if (!strcasecmp(sbtype,"sb16vibra")) {
                ISA_PNP_devreg(new ViBRA_PnP());
                sb.vibra = true;
            }
        }

        /* OPL/CMS Init */
        const char * omode=config->Get_string("oplmode");
        if (!strcasecmp(omode,"none")) opl_mode=OPL_none;   
        else if (!strcasecmp(omode,"cms")) opl_mode=OPL_cms;
        else if (!strcasecmp(omode,"opl2")) opl_mode=OPL_opl2;
        else if (!strcasecmp(omode,"dualopl2")) opl_mode=OPL_dualopl2;
        else if (!strcasecmp(omode,"opl3")) opl_mode=OPL_opl3;
        else if (!strcasecmp(omode,"opl3gold")) opl_mode=OPL_opl3gold;
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

        if (IS_PC98_ARCH) {
            if (opl_mode != OPL_none) {
                if (opl_mode != OPL_opl3) {
                    LOG(LOG_SB,LOG_WARN)("Only OPL3 is allowed in PC-98 mode");
                    opl_mode = OPL_opl3;
                }
            }

            /* card type MUST be SB16.
             * Creative did not release any other Sound Blaster for PC-98 as far as I know. */
            if (sb.type != SBT_16) {
                LOG(LOG_SB,LOG_ERROR)("Only Sound Blaster 16 is allowed in PC-98 mode");
                sb.type = SBT_NONE;
            }
        }
    }
public:
    SBLASTER(Section* configuration):Module_base(configuration) {
        bool bv;
        string s;
        Bitu i;
        int si;

        Section_prop * section=static_cast<Section_prop *>(configuration);

        sb.hw.base=(unsigned int)section->Get_hex("sbbase");

        if (IS_PC98_ARCH) {
            if (sb.hw.base >= 0x220 && sb.hw.base <= 0x2E0) /* translate IBM PC to PC-98 (220h -> D2h) */
                sb.hw.base = 0xD0 + ((sb.hw.base >> 4u) & 0xFu);
        }
        else {
            if (sb.hw.base >= 0xD2 && sb.hw.base <= 0xDE) /* translate PC-98 to IBM PC (D2h -> 220h) */
                sb.hw.base = 0x200 + ((sb.hw.base & 0xFu) << 4u);
        }

        sb.goldplay=section->Get_bool("goldplay");
        sb.min_dma_user=section->Get_int("mindma");
        sb.goldplay_stereo=section->Get_bool("goldplay stereo");
        sb.emit_blaster_var=section->Get_bool("blaster environment variable");
        sb.sample_rate_limits=section->Get_bool("sample rate limits");
        sb.sbpro_stereo_bit_strict_mode=section->Get_bool("stereo control with sbpro only");
        sb.hw.sb_io_alias=section->Get_bool("io port aliasing");
        sb.busy_cycle_hz=section->Get_int("dsp busy cycle rate");
        sb.busy_cycle_duty_percent=section->Get_int("dsp busy cycle duty");
        sb.dsp.instant_direct_dac=section->Get_bool("instant direct dac");
        sb.dsp.force_goldplay=section->Get_bool("force goldplay");
        sb.dma.force_autoinit=section->Get_bool("force dsp auto-init");
        sb.no_filtering=section->Get_bool("disable filtering");
        sb.def_enable_speaker=section->Get_bool("enable speaker");
        sb.enable_asp=section->Get_bool("enable asp");

        if (!sb.goldplay && sb.dsp.force_goldplay) {
            sb.goldplay = true;
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
         *                   a different value like 0x2A, the audio crackes with saturation errors.
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
        sb.write_status_must_return_7f=section->Get_bool("dsp write buffer status must return 0x7f or 0xff");

        sb.dsp.midi_rwpoll_mode = false;
        sb.dsp.midi_read_interrupt = false;
        sb.dsp.midi_read_with_timestamps = false;

        sb.busy_cycle_last_check=0;
        sb.busy_cycle_io_hack=0;

        si=section->Get_int("irq");
        sb.hw.irq=(si >= 0) ? (unsigned int)si : 0xFF;

        if (IS_PC98_ARCH) {
            if (sb.hw.irq == 7) /* IRQ 7 is not valid on PC-98 (that's cascade interrupt) */
                sb.hw.irq = 5;
        }

        si=section->Get_int("dma");
        sb.hw.dma8=(si >= 0) ? (unsigned int)si : 0xFF;

        si=section->Get_int("hdma");
        sb.hw.dma16=(si >= 0) ? (unsigned int)si : 0xFF;

        if (IS_PC98_ARCH) {
            if (sb.hw.dma8 > 3)
                sb.hw.dma8 = 3;
            if (sb.hw.dma8 == 1) /* DMA 1 is not usable for SB on PC-98? */
                sb.hw.dma8 = 3;
            if (sb.hw.dma16 > 3)
                sb.hw.dma16 = sb.hw.dma8;

            LOG_MSG("PC-98: Final SB16 resources are DMA8=%u DMA16=%u\n",sb.hw.dma8,sb.hw.dma16);

            sb.dma.chan=GetDMAChannel(sb.hw.dma8);
            if (sb.dma.chan == NULL) LOG_MSG("PC-98: SB16 is unable to obtain DMA channel");
        }

        /* some DOS games/demos support Sound Blaster, and expect the IRQ to fire, but
         * make no attempt to unmask the IRQ (perhaps the programmer forgot). This option
         * is needed for Public NMI "jump" demo in order for music to continue playing. */
        bv=section->Get_bool("pic unmask irq");
        if (bv && sb.hw.irq != 0xFF) {
            LOG_MSG("Sound blaster: unmasking IRQ at startup as requested.");
            PIC_SetIRQMask(sb.hw.irq,false);
        }

        if (sb.hw.irq == 0xFF || sb.hw.dma8 == 0xFF) {
            LOG(LOG_SB,LOG_WARN)("IRQ and 8-bit DMA not assigned, disabling BLASTER variable");
            sb.emit_blaster_var = false;
        }

        sb.mixer.enabled=section->Get_bool("sbmixer");
        sb.mixer.stereo=false;

        bv=section->Get_bool("pre-set sbpro stereo");
        if (bv) {
            LOG_MSG("Sound blaster: setting SB Pro mixer 'stereo' bit as instructed.");
            sb.mixer.stereo=true;
        }

        Find_Type_And_Opl(section,sb.type,oplmode);
        bool isCMSpassthrough = false;
    
        switch (oplmode) {
        case OPL_none:
            if (!IS_PC98_ARCH)
                WriteHandler[0].Install(0x388,adlib_gusforward,IO_MB);
            break;
        case OPL_cms:
            assert(!IS_PC98_ARCH);
            WriteHandler[0].Install(0x388,adlib_gusforward,IO_MB);
            CMS_Init(section);
            break;
        case OPL_opl2:
            assert(!IS_PC98_ARCH);
            CMS_Init(section);
            // fall-through
        case OPL_dualopl2:
            assert(!IS_PC98_ARCH);
        case OPL_opl3:
        case OPL_opl3gold:
            OPL_Init(section,oplmode);
            break;
        case OPL_hardwareCMS:
            assert(!IS_PC98_ARCH);
            isCMSpassthrough = true;
        case OPL_hardware:
            assert(!IS_PC98_ARCH);
            Bitu base = (unsigned int)section->Get_hex("hardwarebase");
            HARDOPL_Init(base, sb.hw.base, isCMSpassthrough);
            break;
        }
        if (sb.type==SBT_NONE || sb.type==SBT_GB) return;

        sb.chan=MixerChan.Install(&SBLASTER_CallBack,22050,"SB");
        sb.dac.dac_pt = sb.dac.dac_t = 0;
        sb.dsp.state=DSP_S_NORMAL;
        sb.dsp.out.lastval=0xaa;
        sb.dma.chan=NULL;

        for (i=4;i<=0xf;i++) {
            if (i==8 || i==9) continue;
            //Disable mixer ports for lower soundblaster
            if ((sb.type==SBT_1 || sb.type==SBT_2) && (i==4 || i==5)) continue; 
            ReadHandler[i].Install(sb.hw.base+(IS_PC98_ARCH ? ((i+0x20u) << 8u) : i),read_sb,IO_MB);
            WriteHandler[i].Install(sb.hw.base+(IS_PC98_ARCH ? ((i+0x20u) << 8u) : i),write_sb,IO_MB);
        }

        // NTS: Unknown/undefined registers appear to return the register index you requested rather than the actual contents,
        //      according to real SB16 CSP/ASP hardware (chip version id 0x10).
        //
        //      Registers 0x00-0x1F are defined. Registers 0x80-0x83 are defined.
        for (i=0;i<256;i++) ASP_regs[i] = i;
        for (i=0x00;i < 0x20;i++) ASP_regs[i] = 0;
        for (i=0x80;i < 0x84;i++) ASP_regs[i] = 0;
        ASP_regs[5] = 0x01;
        ASP_regs[9] = 0xf8;

        DSP_Reset();
        CTMIXER_Reset();

        // The documentation does not specify if SB gets initialized with the speaker enabled
        // or disabled. Real SBPro2 has it disabled. 
        sb.speaker=false;
        // On SB16 the speaker flag does not affect actual speaker state.
        if (sb.type == SBT_16 || sb.ess_type != ESS_NONE || sb.reveal_sc_type != RSC_NONE) sb.chan->Enable(true);
        else sb.chan->Enable(false);

        if (sb.def_enable_speaker)
            DSP_SetSpeaker(true);

        s=section->Get_string("dsp require interrupt acknowledge");
        if (s == "true" || s == "1" || s == "on")
            sb.dsp.require_irq_ack = 1;
        else if (s == "false" || s == "0" || s == "off")
            sb.dsp.require_irq_ack = 0;
        else /* auto */
            sb.dsp.require_irq_ack = (sb.type == SBT_16) ? 1 : 0; /* Yes if SB16, No otherwise */

        si=section->Get_int("dsp write busy delay"); /* in nanoseconds */
        if (si >= 0) sb.dsp.dsp_write_busy_time = (unsigned int)si;
        else sb.dsp.dsp_write_busy_time = 1000; /* FIXME: How long is the DSP busy on real hardware? */

        /* Sound Blaster (1.x and 2x) and Sound Blaster Pro (3.1) have the I/O port aliasing.
         * The aliasing is also faithfully emulated by the ESS AudioDrive. */
        switch (sb.type) {
            case SBT_1: /* guess */
            case SBT_2: /* verified on real hardware */
            case SBT_GB: /* FIXME: Right?? */
            case SBT_PRO1: /* verified on real hardware */
            case SBT_PRO2: /* guess */
                break;
            default:
                sb.hw.sb_io_alias=false;
                break;
        }

        /* auto-pick busy cycle */
        /* NOTE: SB16 cards appear to run a DSP busy cycle at all times.
         *       SB2 cards (and Pro?) appear to run a DSP busy cycle only when playing audio through DMA. */
        if (sb.busy_cycle_hz < 0) {
            if (sb.type == SBT_16) /* Guess: Pentium PCI-ISA SYSCLK=8.333MHz  /  (6 cycles per 8-bit I/O read  x  16 reads from DSP status) = about 86.805KHz? */
                sb.busy_cycle_hz = 8333333 / 6 / 16;
            else /* Guess ???*/
                sb.busy_cycle_hz = 8333333 / 6 / 16;
        }

        if (sb.ess_type != ESS_NONE) {
            uint8_t t;

            /* legacy audio interrupt control */
            t = 0x80;/*game compatible IRQ*/
            switch (sb.hw.irq) {
                case 5:     t |= 0x5; break;
                case 7:     t |= 0xA; break;
                case 10:    t |= 0xF; break;
            }
            ESSreg(0xB1) = t;

            /* DRQ control */
            t = 0x80;/*game compatible DRQ */
            switch (sb.hw.dma8) {
                case 0:     t |= 0x5; break;
                case 1:     t |= 0xA; break;
                case 3:     t |= 0xF; break;
            }
            ESSreg(0xB2) = t;
        }

        /* first configuration byte returned by 0x58.
         *
         * bits 7-5: ???
         * bit    4: Windows Sound System (Crystal Semiconductor CS4231A) I/O base port (1=0xE80  0=0x530)
         * bits 3-2: ???
         * bit    1: gameport disable (1=disabled 0=enabled)
         * bit    0: jumper assigns Sound Blaster base port 240h (right??) */
        sb.sc400_jumper_status_1 = 0x2C + ((sb.hw.base == 0x240) ? 0x1 : 0x0);
        if (!JOYSTICK_IsEnabled(0) && !JOYSTICK_IsEnabled(1)) sb.sc400_jumper_status_1 += 0x02; // set bit 1
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
        sb.sc400_jumper_status_2 = 0xC0 | (((sb.hw.base + 0x10) >> 4) & 0xF);

        sb.sc400_dsp_major = 3;
        sb.sc400_dsp_minor = 5;
        sb.sc400_cfg = 0;
        /* bit 7:    MPU IRQ select  (1=IRQ 9  0=IRQ 5)
         * bit 6:    ???
         * bit 5-3:  IRQ select
         * bit 2:    MPU IRQ enable
         * bit 1-0:  DMA select */
        switch (sb.hw.dma8) {
            case 0: sb.sc400_cfg |= (1 << 0); break;
            case 1: sb.sc400_cfg |= (2 << 0); break;
            case 3: sb.sc400_cfg |= (3 << 0); break;
        }
        switch (sb.hw.irq) {
            case 5: sb.sc400_cfg |= (5 << 3); break;
            case 7: sb.sc400_cfg |= (1 << 3); break;
            case 9: sb.sc400_cfg |= (2 << 3); break;
            case 10:sb.sc400_cfg |= (3 << 3); break;
            case 11:sb.sc400_cfg |= (4 << 3); break;
        }
        switch (MPU401_GetIRQ()) { // SC400: bit 7 and bit 2 control MPU IRQ
            case 5: sb.sc400_cfg |= (0 << 7) + (1 << 2); break; // bit 7=0 bit 2=1   MPU IRQ 5
            case 9: sb.sc400_cfg |= (1 << 7) + (1 << 2); break; // bit 7=1 bit 2=1   MPU IRQ 9
            default: break;                     // bit 7=0 bit 2=0   MPU IRQ disabled
        }

        if (sb.reveal_sc_type != RSC_NONE) {
            // SC400 cards always use 8-bit DMA even for 16-bit PCM
            if (sb.hw.dma16 != sb.hw.dma8) {
                LOG(LOG_SB,LOG_DEBUG)("SC400: DMA is always 8-bit, setting 16-bit == 8-bit");
                sb.hw.dma16 = sb.hw.dma8;
            }
        }

        si = section->Get_int("dsp busy cycle always");
        if (si >= 0) sb.busy_cycle_always= (si > 0) ? true : false;
        else if (sb.type == SBT_16) sb.busy_cycle_always = true;
        else sb.busy_cycle_always = false;

        /* auto-pick busy duty cycle */
        if (sb.busy_cycle_duty_percent < 0 || sb.busy_cycle_duty_percent > 100)
            sb.busy_cycle_duty_percent = 50; /* seems about right */

        if (sb.hw.irq != 0 && sb.hw.irq != 0xFF) {
            s = section->Get_string("irq hack");
            if (!s.empty() && s != "none") {
                LOG(LOG_SB,LOG_NORMAL)("Sound Blaster emulation: Assigning IRQ hack '%s' as instruced",s.c_str());
                PIC_Set_IRQ_hack((int)sb.hw.irq,PIC_parse_IRQ_hack_string(s.c_str()));
            }
        }

        // ASP emulation
        if (sb.enable_asp && sb.type != SBT_16) {
            LOG(LOG_SB,LOG_WARN)("ASP emulation requires you to select SB16 emulation");
            sb.enable_asp = false;
        }

        // SB16 non-PNP ASP RAM content observation.
        // Contrary to my initial impression, it looks like the RAM is mostly 0xFF
        // with some random bits flipped because no initialization is done at hardware power-on.
        memset(sb16asp_ram_contents,0xFF,2048);
        for (i=0;i < (2048 * 8);i++) {
            if (((unsigned int)rand() & 31) == 0)
                sb16asp_ram_contents[i>>3] ^= 1 << (i & 7);
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
        memset(sb16_8051_mem,0x00,256);
        sb16_8051_mem[0x0E] = 0xFF;
        sb16_8051_mem[0x0F] = 0x07;
        sb16_8051_mem[0x37] = 0x38;

        if (sb.enable_asp)
            LOG(LOG_SB,LOG_WARN)("ASP emulation at this stage is extremely limited and only covers DSP command responses.");

        /* Soundblaster midi interface */
        if (!MIDI_Available()) sb.midi = false;
        else sb.midi = true;
    }   

    void DOS_Shutdown() { /* very likely, we're booting into a guest OS where our environment variable has no meaning anymore */
        autoexecline.Uninstall();
    }

    void DOS_Startup() {
        if (sb.type==SBT_NONE || sb.type==SBT_GB) return;

        if (sb.emit_blaster_var) {
            // Create set blaster line
            ostringstream temp;
            if (IS_PC98_ARCH)
                temp << "@SET BLASTER=A" << setw(2) << hex << sb.hw.base;
            else
                temp << "@SET BLASTER=A" << setw(3) << hex << sb.hw.base;

            if (sb.hw.irq != 0xFF) temp << " I" << dec << (Bitu)sb.hw.irq;
            if (sb.hw.dma8 != 0xFF) temp << " D" << (Bitu)sb.hw.dma8;
            if (sb.type==SBT_16 && sb.hw.dma16 != 0xFF) temp << " H" << (Bitu)sb.hw.dma16;
            temp << " T" << static_cast<unsigned int>(sb.type) << ends;

            autoexecline.Install(temp.str());
        }
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
        case OPL_opl3gold:
            OPL_ShutDown(m_configuration);
            break;
        default:
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

void SBLASTER_OnReset(Section *sec) {
    (void)sec;//UNUSED
    SBLASTER_DOS_Shutdown();

    if (test != NULL) {
        delete test;    
        test = NULL;
    }
    HWOPL_Cleanup();

    if (test == NULL) {
        LOG(LOG_MISC,LOG_DEBUG)("Allocating Sound Blaster emulation");
        test = new SBLASTER(control->GetSection("sblaster"));
    }
}

void SBLASTER_DOS_Exit(Section *sec) {
    (void)sec;//UNUSED
    SBLASTER_DOS_Shutdown();
}

void SBLASTER_DOS_Boot(Section *sec) {
    (void)sec;//UNUSED
    if (test != NULL) test->DOS_Startup();
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

