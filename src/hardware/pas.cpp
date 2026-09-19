/*
 *  Media Vision Pro AudioSpectrum, Pro AudioSpectrum Plus and Pro AudioSpectrum 16.
 *
 *  Ported from 86Box src/sound/snd_pas16.c by Sarah Walker, Miran Grca,
 *  TheCollector1995, Jasmine Iwanek and win2kgamer.
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

/* What this file owns, per board:
 *
 *   PAS (original)  base fixed at 388h. Two OPL2s (routed in adlib.cpp): with the FM split
 *                   on (B88h bit 7 clear) 388h is left and 38Ah right; with it off 388h
 *                   drives both. 788h always drives both. 8-bit DMA PCM clocked by the
 *                   8253 at 1388h.
 *                   National LMC1982/LMC835 serial mixer. YM3802 MIDI. No Sound Blaster.
 *   PAS Plus        base relocatable through 9A01h. OPL3 at 388h (adlib.cpp). Same PCM
 *                   and serial mixer, MIDI UART, plus a Sound Blaster 2.0 DSP whose
 *                   base/IRQ/DMA the driver sets through F789h/FB8Ah (sblaster.cpp).
 *   PAS 16          PAS Plus with 16-bit PCM, the MV508 mixer and a prescaled PCM clock.
 *
 * Everything below is addressed as an offset from the board base (388h), exactly as
 * in 86Box: B89h is 0x0801, F8Ah is 0x0C02, 1388h-138Bh (the 8253) is 0x1000-0x1003.
 *
 * Not emulated: the NCR 5380 SCSI port (reads 0FFh), MIDI input, bass/treble and the
 * CD/line/mic/speaker mixer inputs, and moving the OPL along with a relocated base.
 */

#include <math.h>
#include <string.h>
#include "dosbox.h"
#include "inout.h"
#include "logging.h"
#include "mixer.h"
#include "dma.h"
#include "pic.h"
#include "timer.h"

void MIDI_RawOutByte(uint8_t data);
void SB_PAS_SetCompat(bool enable, Bitu base, Bitu irq, Bitu dma);
bool SB_PAS_IRQPending(void);

enum { PAS_ORIGINAL = 1, PAS_PLUS = 2, PAS_16 = 3 };

enum { INT_SAMP = 0x04, INT_PCM = 0x08, INT_MIDI = 0x10 };                 /* B89h/B8Bh */
enum { PCM_MONO = 0x20, PCM_ENA = 0x40, PCM_DMA_ENA = 0x80 };             /* F8Ah */
enum { SC2_16BIT = 0x04, SC2_12BIT = 0x08, SC2_MSBINV = 0x10 };           /* 8389h */
enum { FILT_UNMUTE = 0x20, FILT_GATE0 = 0x40, FILT_GATE1 = 0x80 };        /* B8Ah */

/* B88h serial mixer lines (PAS/PAS Plus), and on the original PAS the FM split switch:
 * bit 7 set = split off, 388h writes both OPL2s (AdLib compatible) */
enum { SM_DATA = 0x01, SM_CLOCK = 0x02, SM_STROBE = 0x04, SM_IDENT = 0x10, SM_FM_MONO = 0x80 };

/* Serial mixer receive states. The low bits count the bit being shifted in. */
enum {
    ST_IDLE           = 0x00,
    ST_1982_ADDR      = 0x10, /* 10h-17h: address bits 0-7 */
    ST_1982_ADDR_OVER = 0x18,
    ST_1982_DATA      = 0x20, /* 20h-2Fh: data bits 0-15 */
    ST_1982_DATA_8    = 0x28,
    ST_1982_DATA_OVER = 0x30,
    ST_835_DATA       = 0x40, /* 40h-47h: data bits 0-7 */
    ST_835_DATA_OVER  = 0x48
};

enum { LMC1982_ISELECT = 0, LMC1982_BASS = 2, LMC1982_TREBLE = 3, LMC1982_VOL_L = 4, LMC1982_VOL_R = 5, LMC1982_MODE = 6 };
enum { LMC835_MODE = 0, LMC835_FM_L = 1, LMC835_PCM_L = 6, LMC835_FM_R = 8, LMC835_PCM_R = 0x0d };
enum { IM_1982_ADDR = 0, IM_1982_DATA = 1, IM_835_ADDR = 2, IM_835_DATA = 3 };

/* MV508 (PAS 16) register indices: bit 4 = mixer input, 20h = left, 40h = right */
enum { MV_MASTER_L = 0x21, MV_MASTER_R = 0x41, MV_FM_L = 0x30, MV_FM_R = 0x50,
       MV_PCM_L = 0x35, MV_PCM_R = 0x55, MV_SB_L = 0x37, MV_SB_R = 0x57 };

static const int pas_dmas[8]    = { 4, 1, 2, 3, 0, 5, 6, 7 };
static const int pas_sb_irqs[8] = { 0, 2, 3, 5, 7, 10, 11, 12 };

/* Attenuation tables, 32767 = 0 dB. */
static const uint16_t mv508_att_2db_5bit[32] = {
       25,    32,    41,    51,    65,    82,   103,   130,   164,   206,   260,   327,   412,   519,   653,   822,
     1036,  1304,  1641,  2067,  2602,  3276,  4125,  5192,  6537,  8230, 10362, 13044, 16422, 20674, 26027, 32767
};
static const uint16_t mv508_att_1db_6bit[64] = {
       18,    25,    29,    32,    36,    41,    46,    51,    58,    65,    73,    82,    92,   103,   116,   130,
      146,   164,   184,   206,   231,   260,   292,   327,   367,   412,   462,   519,   582,   653,   733,   822,
      923,  1036,  1162,  1304,  1463,  1641,  1842,  2067,  2319,  2602,  2920,  3276,  3676,  4125,  4628,  5192,
     5826,  6537,  7335,  8230,  9234, 10362, 11626, 13044, 14636, 16422, 18426, 20674, 23197, 26027, 29204, 32767
};
static const uint16_t lmc1982_att_2db_6bit[64] = {
    32767, 26027, 20674, 16422, 13044, 10362,  8230,  6537,  5192,  4125,  3276,  2602,  2067,  1641,  1304,  1036,
      822,   653,   519,   412,   327,   260,   206,   164,   130,   103,    82,    65,    51,    41,    32,    25,
       20,    16,    13,    10,     8,     6,     5,     4,     3,     3,     3,     3,     3,     3,     3,     3,
        3,     3,     3,     3,     3,     3,     3,     3,     3,     3,     3,     3,     3,     3,     3,     3
};
/* LMC835N: sparse, 7-bit code -> level for the 1 dB and the 0.5 dB step modes. The
 * 12 dB input cut is baked in, so the maximum boost is 0 dB (see 86Box). */
static uint16_t lmc835_att_1db[128], lmc835_att_05db[128];

static void PAS_BuildLMC835Tables(void) {
    static const struct { uint8_t code; uint16_t db1, db05; } t[] = {
        {0x40, 8230, 8230}, {0x60, 9234, 8718}, {0x50,10362, 9234}, {0x48,11626, 9782},
        {0x44,13044,10362}, {0x42,14636,10975}, {0x52,16422,11626}, {0x6a,18426,12315},
        {0x56,20674,13044}, {0x41,23197,13817}, {0x69,26027,14636}, {0x6d,29204,15503},
        {0x5d,29204,15503}, {0x6f,32767,16422}, {0x00, 2067, 4125}, {0x20, 2319, 4369},
        {0x10, 2602, 4628}, {0x08, 2920, 4920}, {0x04, 3276, 5192}, {0x02, 3676, 5500},
        {0x12, 4125, 5826}, {0x2a, 4628, 6172}, {0x16, 5192, 6537}, {0x01, 5826, 6925},
        {0x29, 6537, 7335}, {0x2d, 7335, 7770}, {0x1d, 7335, 7770}, {0x2f, 8230, 8230}
    };
    memset(lmc835_att_1db, 0, sizeof(lmc835_att_1db));
    memset(lmc835_att_05db, 0, sizeof(lmc835_att_05db));
    for (const auto &e : t) {
        lmc835_att_1db[e.code] = e.db1;
        lmc835_att_05db[e.code] = e.db05;
    }
}

struct PasCounter {
    uint32_t reload = 0x10000; /* 0 written means 65536 */
    uint32_t count = 0x10000;  /* counter 1 only: clocked once per DMA transfer */
    uint16_t latch = 0;
    uint8_t mode = 0, rw = 3, wlow = 0;
    bool write_hi = false, read_hi = false, latched = false;
};

struct PAS_State {
    unsigned int type = 0;
    Bitu base = 0x388, new_base = 0x388;
    uint8_t board_id = 0;
    bool master_ff = false;

    uint8_t audiofilt = 0, audio_mixer = 0, irq_stat = 0, irq_ena = 0, pcm_ctrl = 0;
    uint8_t prescale_div = 0, waitstates = 0, enhancedscsi = 0, timeout_count = 0, timeout_status = 0;
    uint8_t sys_conf[4] = {};
    uint8_t io_conf[4] = {};
    uint8_t compat = 0, compat_base = 0, sb_irqdma = 0;
    Bitu sb_base = 0x220, sb_irq = 7, sb_dma = 1;

    int irq = -1, dma = 4;
    DmaChannel *dma_chan = nullptr;
    bool stereo_lr = false, dma8_ff = false;
    uint16_t dma8_dat = 0;
    unsigned int ticks = 0;
    uint16_t pcm_l = 0, pcm_r = 0;

    PasCounter pit[3];
    pic_tickindex_t c0_start = 0;

    /* PAS Plus/16 MIDI UART */
    uint8_t midi_ctrl = 0, midi_stat = 0, midi_data = 0, fifo_stat = 0;
    bool midi_uart_out = false, midi_uart_in = false;

    /* original PAS YM3802 */
    uint8_t ym_ivr = 0, ym_rgr = 0, ym_isr = 0, ym_idx = 0;
    uint8_t ym_reg[4][16] = {};
    uint16_t ym_gen_timer = 0;

    /* LMC1982CIN + LMC835N (PAS, PAS Plus) */
    uint8_t lmc1982[8] = {};
    uint8_t lmc835[16] = {};
    uint8_t im_state = ST_IDLE;
    uint16_t im_data[4] = {};

    /* MV508 (PAS 16): bank 0 output mix, bank 1 input mix, bank 2 volumes */
    uint8_t mv_index = 0;
    uint8_t mv_regs[3][128] = {};

    bool use_mixer = true;
    float fm_scale = 1.0f;

    MixerObject mixobj;
    MixerChannel *chan = nullptr;
    IO_ReadHandleObject rd[64];
    IO_WriteHandleObject wr[64];
    IO_WriteHandleObject wr_9a01;
};

static PAS_State *pas = nullptr;

static void PAS_RaiseIRQ(void) {
    if (pas->irq >= 0) PIC_ActivateIRQ((Bitu)pas->irq);
}

static void PAS_LowerIRQ(void) {
    if (pas->irq >= 0) PIC_DeActivateIRQ((Bitu)pas->irq);
}

/* Render the PCM channel up to now, so a register change lands at the right sample. */
static void PAS_Sync(void) {
    if (pas->chan && pas->chan->enabled) pas->chan->FillUp();
}

/* ---- mixer ------------------------------------------------------------------ */

static void PAS_ApplyMixer(void) {
    if (!pas->use_mixer) return;

    float master_l, master_r, pcm_l, pcm_r, fm_l, fm_r, sb_l, sb_r;
    if (pas->type == PAS_16) {
        master_l = mv508_att_1db_6bit[pas->mv_regs[2][MV_MASTER_L] & 0x3f] / 32767.0f;
        master_r = mv508_att_1db_6bit[pas->mv_regs[2][MV_MASTER_R] & 0x3f] / 32767.0f;
        pcm_l = mv508_att_2db_5bit[pas->mv_regs[0][MV_PCM_L] & 0x1f] / 32767.0f;
        pcm_r = mv508_att_2db_5bit[pas->mv_regs[0][MV_PCM_R] & 0x1f] / 32767.0f;
        fm_l  = mv508_att_2db_5bit[pas->mv_regs[0][MV_FM_L] & 0x1f] / 32767.0f;
        fm_r  = mv508_att_2db_5bit[pas->mv_regs[0][MV_FM_R] & 0x1f] / 32767.0f;
        sb_l  = mv508_att_2db_5bit[pas->mv_regs[0][MV_SB_L] & 0x1f] / 32767.0f;
        sb_r  = mv508_att_2db_5bit[pas->mv_regs[0][MV_SB_R] & 0x1f] / 32767.0f;
    }
    else {
        /* LMC1982CIN: the Windows 95 driver says left and right are swapped in the wiring. */
        master_l = master_r = 0.0f;
        if ((pas->lmc1982[LMC1982_ISELECT] & 3) == 1) {
            const float vl = lmc1982_att_2db_6bit[pas->lmc1982[LMC1982_VOL_L] & 0x3f] / 32767.0f;
            const float vr = lmc1982_att_2db_6bit[pas->lmc1982[LMC1982_VOL_R] & 0x3f] / 32767.0f;
            switch (pas->lmc1982[LMC1982_MODE] & 7) {
                case 4: master_l = master_r = vr; break;          /* left mono */
                case 5: master_l = vr; master_r = vl; break;      /* stereo */
                case 6: case 7: master_l = master_r = vl; break;  /* right mono */
            }
        }
        const uint16_t *a = (pas->lmc835[LMC835_MODE] & 0x20) ? lmc835_att_05db : lmc835_att_1db;
        const uint16_t *b = (pas->lmc835[LMC835_MODE] & 0x08) ? lmc835_att_05db : lmc835_att_1db;
        pcm_l = a[pas->lmc835[LMC835_PCM_L] & 0x7f] / 32767.0f;
        fm_l  = a[pas->lmc835[LMC835_FM_L] & 0x7f] / 32767.0f;
        pcm_r = b[pas->lmc835[LMC835_PCM_R] & 0x7f] / 32767.0f;
        fm_r  = b[pas->lmc835[LMC835_FM_R] & 0x7f] / 32767.0f;
        sb_l = sb_r = 1.0f; /* the LMC835N has no Sound Blaster input; only master applies */
    }

    pas->chan->SetScale(pcm_l * master_l, pcm_r * master_r);
    if (MixerChannel *c = MIXER_FindChannel("FM")) c->SetScale(pas->fm_scale * fm_l * master_l, pas->fm_scale * fm_r * master_r);
    if (pas->type != PAS_ORIGINAL)
        if (MixerChannel *c = MIXER_FindChannel("SB")) c->SetScale(sb_l * master_l, sb_r * master_r);
}

/* The power-on mixer levels are unknown (86Box leaves them a TODO and its
 * MV508 guess puts master at -32 dB), so reset to 0 dB everywhere; SB and FM then play
 * at the usual DOSBox-X level until MVSOUND.SYS or a game programs the mixer. */
static void PAS_ResetMixer(void) {
    memset(pas->lmc1982, 0, sizeof(pas->lmc1982));
    pas->lmc1982[LMC1982_ISELECT] = 0x01;
    pas->lmc1982[LMC1982_BASS] = pas->lmc1982[LMC1982_TREBLE] = 0x06;
    pas->lmc1982[LMC1982_MODE] = 0x05;
    memset(pas->lmc835, 0x6f, sizeof(pas->lmc835));
    pas->lmc835[LMC835_MODE] = 0x00;
    pas->im_state = ST_IDLE;

    memset(pas->mv_regs, 0, sizeof(pas->mv_regs));
    for (unsigned int i = 0x30; i <= 0x37; i++) pas->mv_regs[0][i] = pas->mv_regs[0][i + 0x20] = 0x1f;
    pas->mv_regs[2][MV_MASTER_L] = pas->mv_regs[2][MV_MASTER_R] = 0x3f;

    PAS_ApplyMixer();
}

static void PAS_LMC1982Update(void) {
    if ((pas->im_data[IM_1982_ADDR] & 0xf8) == 0x40) {
        pas->lmc1982[pas->im_data[IM_1982_ADDR] & 7] = pas->im_data[IM_1982_DATA] & 0xff;
        PAS_ApplyMixer();
    }
    pas->im_state = ST_IDLE;
}

static void PAS_LMC835Update(void) {
    pas->lmc835[LMC835_MODE] = pas->im_data[IM_835_ADDR] & 0xf0;
    const unsigned int reg = pas->im_data[IM_835_ADDR] & 0x0f;
    if (reg >= 0x01 && reg <= 0x0e) pas->lmc835[reg] = (uint8_t)pas->im_data[IM_835_DATA];
    PAS_ApplyMixer();
}

/* B88h on PAS/PAS Plus: the driver bit-bangs the LMC1982CIN and LMC835N through
 * IDENT/CLOCK/DATA/STROBE. Straight port of 86Box's state machine. */
static void PAS_SerialMixerWrite(uint8_t val) {
    const uint8_t old = pas->audio_mixer;
    const bool clock_rise = (val & SM_CLOCK) && !(old & SM_CLOCK);
    const unsigned int st = pas->im_state;

    auto shift_in = [&](unsigned int which, unsigned int bit) {
        pas->im_data[which] |= (uint16_t)((val & SM_DATA) << bit);
        pas->im_state++;
    };

    if (st == ST_IDLE || st == ST_1982_ADDR) {
        if (val & SM_IDENT) {
            if (!(val & SM_CLOCK) && (old & SM_CLOCK)) {
                pas->im_data[IM_835_DATA] = 0;
                pas->im_state = ST_835_DATA;
            }
        }
        else {
            if (st == ST_IDLE && (old & SM_IDENT)) {
                pas->im_data[IM_1982_ADDR] = 0;
                pas->im_state = ST_1982_ADDR;
            }
            if (clock_rise && pas->im_state != ST_IDLE) shift_in(IM_1982_ADDR, pas->im_state & 7);
        }
    }
    else if (st > ST_1982_ADDR && st < ST_1982_ADDR_OVER) {
        if (clock_rise) shift_in(IM_1982_ADDR, st & 7);
    }
    else if (st == ST_1982_ADDR_OVER) {
        pas->im_data[IM_1982_DATA] = 0;
        pas->im_state = ST_1982_DATA;
    }
    else if (st == ST_1982_DATA_8) {
        if (val & SM_IDENT) {
            if (!(old & SM_IDENT)) PAS_LMC1982Update(); /* 8-bit transfer ended */
            else if (clock_rise) shift_in(IM_1982_DATA, st & 0x0f);
        }
    }
    else if (st >= ST_1982_DATA && st < ST_1982_DATA_OVER) {
        if (clock_rise) shift_in(IM_1982_DATA, st & 0x0f);
    }
    else if (st == ST_1982_DATA_OVER) {
        if ((val & SM_IDENT) && !(old & SM_IDENT)) PAS_LMC1982Update();
    }
    else if (st >= ST_835_DATA && st < ST_835_DATA_OVER) {
        if (clock_rise) shift_in(IM_835_DATA, st & 7);
    }
    else if (st == ST_835_DATA_OVER) {
        if ((val & SM_STROBE) && !(old & SM_STROBE)) {
            if (pas->im_data[IM_835_DATA] & 0x80) /* an address; bit 6 is don't care */
                pas->im_data[IM_835_ADDR] = pas->im_data[IM_835_DATA] & 0x7f;
            else
                PAS_LMC835Update();
            pas->im_state = ST_IDLE;
        }
    }

    pas->audio_mixer = val;
}

/* 78Bh on PAS 16 */
static void PAS_MV508Write(uint8_t val) {
    if (val & 0x80) {
        pas->mv_index = val & 0x7f;
        return;
    }

    unsigned int bank, mask;
    if (pas->mv_index & 0x10) { bank = (val & 0x20) ? 1 : 0; mask = 0x1f; }
    else                      { bank = 2; mask = 0x3f; }

    if (pas->mv_index & 0x60)
        pas->mv_regs[bank][pas->mv_index] = val & mask;
    else
        pas->mv_regs[bank][pas->mv_index | 0x20] = pas->mv_regs[bank][pas->mv_index | 0x40] = val & mask;

    PAS_ApplyMixer();
}

/* ---- PCM -------------------------------------------------------------------- */

static bool PAS_CompatStereo(void) {
    /* 8388h bit 1 clear: stereo alternates L/R one sample per timer 0 tick */
    return !(pas->pcm_ctrl & PCM_MONO) && !(pas->sys_conf[0] & 0x02);
}

static bool PAS_Running(void) {
    return (pas->audiofilt & FILT_GATE0) && (pas->pcm_ctrl & PCM_ENA) && (pas->pit[1].mode & 2);
}

static double PAS_ClockHz(void) {
    if (pas->type == PAS_16 && (pas->sys_conf[0] & 0x02) && pas->prescale_div)
        return ((pas->sys_conf[2] & 0x02) ? 1008000.0 : 441000.0) / pas->prescale_div;
    return (double)PIT_TICK_RATE_IBM;
}

/* The PCM channel runs at the timer 0 rate and its callback clocks the card, so the
 * channel only exists while the card is actually fetching samples. */
static void PAS_UpdateChannel(void) {
    if (!PAS_Running()) {
        pas->chan->Enable(false);
        return;
    }

    double rate = PAS_ClockHz() / pas->pit[0].reload;
    if (PAS_CompatStereo()) rate /= 2;
    /* The real card tops out at 88.2 kHz; the cap only guards the mixer
     * against a driver probing tiny divisors with PCM left running. */
    if (rate > 200000.0) rate = 200000.0;
    pas->chan->SetFreq((Bitu)(rate * 1000.0 + 0.5), 1000);
    pas->chan->Enable(true);
}

static void PAS_UpdateFilter(void) {
    Bitu cutoff;
    switch (pas->audiofilt & 0x1f) {
        case 0x01: cutoff = 17897; break;
        case 0x02: cutoff = 15909; break;
        case 0x09: cutoff = 11931; break;
        case 0x11: cutoff = 8948; break;
        case 0x19: cutoff = 5965; break;
        case 0x04: cutoff = 2982; break;
        default:   cutoff = 0; break;
    }
    pas->chan->SetLowpassFreq(cutoff, 2);
}

static uint16_t PAS_DmaRead(void) {
    if (!(pas->pcm_ctrl & PCM_DMA_ENA) || !pas->dma_chan || pas->dma_chan->masked) return 0;
    uint8_t buf[2] = { 0, 0 };
    if (pas->dma_chan->Read(1, buf) != 1) return 0;
    return (uint16_t)(buf[0] | (buf[1] << 8));
}

/* One 8-bit sample. On a 16-bit channel one DMA word carries two samples. */
static uint16_t PAS_ReadSample8(void) {
    if (pas->dma >= 5 && pas->dma8_ff) {
        pas->dma8_dat >>= 8;
    }
    else {
        pas->dma8_dat = PAS_DmaRead();
        pas->ticks++;
    }
    if (pas->dma >= 5) pas->dma8_ff = !pas->dma8_ff;
    return (uint16_t)(((pas->dma8_dat & 0xff) ^ 0x80) << 8);
}

static uint16_t PAS_ReadSample(void) {
    uint16_t r;
    if (pas->sys_conf[1] & SC2_16BIT) {
        if (pas->dma >= 5) {
            r = PAS_DmaRead();
            pas->ticks += 1;
        }
        else {
            r = PAS_DmaRead();
            r |= (uint16_t)(PAS_DmaRead() << 8);
            pas->ticks += 2;
        }
        if (pas->sys_conf[1] & SC2_12BIT) r &= 0xfff0;
    }
    else {
        r = PAS_ReadSample8();
    }
    if (pas->sys_conf[1] & SC2_MSBINV) r ^= 0x8000;
    return r;
}

/* Counter 1 counts DMA transfers; each time it runs out the buffer interrupt fires. */
static void PAS_ClockCounter1(void) {
    PasCounter &c = pas->pit[1];
    if (!(pas->audiofilt & FILT_GATE1)) return;
    if (--c.count != 0) return;
    c.count = c.reload;
    if ((pas->pcm_ctrl & PCM_ENA) && (pas->irq_ena & INT_PCM)) {
        pas->irq_stat |= INT_PCM;
        PAS_RaiseIRQ();
    }
}

static void PAS_UpdateMidiIRQ(void);

/* One timer 0 output edge. */
static void PAS_Tick(void) {
    const bool readable = pas->dma_chan && !pas->dma_chan->masked;
    if (!readable && pas->type != PAS_ORIGINAL) return;
    if (pas->type != PAS_ORIGINAL) PAS_UpdateMidiIRQ();

    pas->ticks = 0;
    if (pas->pcm_ctrl & PCM_MONO) {
        pas->pcm_l = pas->pcm_r = PAS_ReadSample();
    }
    else if (pas->sys_conf[0] & 0x02) {
        pas->pcm_l = PAS_ReadSample();
        pas->pcm_r = PAS_ReadSample();
    }
    else {
        const uint16_t s = PAS_ReadSample();
        if (pas->stereo_lr) pas->pcm_r = s;
        else pas->pcm_l = s;
        pas->stereo_lr = !pas->stereo_lr;
        pas->irq_stat = (uint8_t)((pas->irq_stat & 0xdf) | (pas->stereo_lr ? 0x20 : 0x00));
    }
    if (!readable) pas->pcm_l = pas->pcm_r = 0;

    for (unsigned int i = 0; i < pas->ticks; i++) PAS_ClockCounter1();

    pas->irq_stat |= INT_SAMP;
    if (pas->irq_ena & INT_SAMP) PAS_RaiseIRQ();
}

static void PAS_CallBack(Bitu len) {
    int16_t buf[512][2];
    const unsigned int ticks_per_frame = PAS_CompatStereo() ? 2 : 1;

    while (len > 0) {
        const Bitu n = len < 512 ? len : 512;
        for (Bitu i = 0; i < n; i++) {
            if (PAS_Running())
                for (unsigned int t = 0; t < ticks_per_frame; t++) PAS_Tick();
            const bool on = (pas->audiofilt & FILT_UNMUTE) != 0;
            buf[i][0] = on ? (int16_t)pas->pcm_l : 0;
            buf[i][1] = on ? (int16_t)pas->pcm_r : 0;
        }
        pas->chan->AddSamples_s16(n, &buf[0][0]);
        len -= n;
    }
}

static void PAS_DMA_CallBack(DmaChannel * /*chan*/, DMAEvent event) {
    /* a guest polling the DMA counter wants the transfer up to now */
    if (event == DMA_READ_COUNTER) PAS_Sync();
}

static void PAS_SetDMA(int dma) {
    if (pas->dma_chan && pas->dma_chan->callback == PAS_DMA_CallBack) pas->dma_chan->Register_Callback(nullptr);
    pas->dma = dma;
    pas->dma_chan = (dma == 4) ? nullptr : GetDMAChannel((uint8_t)dma);
    if (pas->dma_chan) pas->dma_chan->Register_Callback(PAS_DMA_CallBack);
}

static void PAS_ResetPCM(void) {
    pas->pcm_ctrl = 0;
    pas->stereo_lr = false;
    pas->irq_stat &= 0xd7;
    if (!pas->irq_stat) PAS_LowerIRQ();
}

/* ---- 8253 at base+1000h ------------------------------------------------------ */

static uint16_t PAS_CounterValue(unsigned int n) {
    const PasCounter &c = pas->pit[n];
    if (n == 1) return (uint16_t)c.count;
    if (n == 0 && (pas->audiofilt & FILT_GATE0)) {
        const double elapsed = (double)(PIC_FullIndex() - pas->c0_start) * PAS_ClockHz() / 1000.0;
        return (uint16_t)(c.reload - (uint32_t)fmod(elapsed, (double)c.reload));
    }
    return (uint16_t)c.reload;
}

static void PAS_CounterLoaded(unsigned int n) {
    PasCounter &c = pas->pit[n];
    if (n == 1) c.count = c.reload;
    if (n == 0) {
        pas->c0_start = PIC_FullIndex();
        PAS_UpdateChannel();
    }
}

static Bitu PAS_PITRead(unsigned int n) {
    PAS_Sync();
    PasCounter &c = pas->pit[n];
    if (!c.latched) c.latch = PAS_CounterValue(n);

    uint8_t ret;
    switch (c.rw) {
        case 1: ret = c.latch & 0xff; c.latched = false; break;
        case 2: ret = c.latch >> 8; c.latched = false; break;
        default:
            ret = c.read_hi ? (c.latch >> 8) : (c.latch & 0xff);
            if (c.read_hi) c.latched = false;
            c.read_hi = !c.read_hi;
            break;
    }
    return ret;
}

static void PAS_PITWrite(unsigned int n, uint8_t val) {
    PAS_Sync();
    PasCounter &c = pas->pit[n];
    switch (c.rw) {
        case 1: c.reload = val; break;
        case 2: c.reload = (uint32_t)val << 8; break;
        default:
            if (!c.write_hi) { c.wlow = val; c.write_hi = true; return; }
            c.reload = c.wlow | ((uint32_t)val << 8);
            c.write_hi = false;
            break;
    }
    if (c.reload == 0) c.reload = 0x10000;
    PAS_CounterLoaded(n);
}

static void PAS_PITControl(uint8_t val) {
    const unsigned int n = val >> 6;
    if (n == 3) return; /* 8254 read-back: not on the PAS's 8253 */
    PAS_Sync();
    PasCounter &c = pas->pit[n];
    const uint8_t rw = (val >> 4) & 3;
    if (rw == 0) { /* counter latch */
        c.latch = PAS_CounterValue(n);
        c.latched = true;
        c.read_hi = false;
        return;
    }
    c.rw = rw;
    c.mode = (val >> 1) & 7;
    if (c.mode > 5) c.mode -= 4;
    c.write_hi = c.read_hi = c.latched = false;
    PAS_UpdateChannel();
}

/* ---- MIDI --------------------------------------------------------------------- */

static void PAS_UpdateMidiIRQ(void) {
    if ((pas->midi_uart_out && (pas->midi_stat & 0x18)) || (pas->midi_uart_in && (pas->midi_stat & 0x04))) {
        pas->irq_stat |= INT_MIDI;
        if (pas->irq_ena & INT_MIDI) PAS_RaiseIRQ();
    }
}

static void PAS_YM3802TimerEvent(Bitu /*val*/) {
    if (!pas || !pas->ym_gen_timer) return;
    PIC_AddEvent(PAS_YM3802TimerEvent, pas->ym_gen_timer * 0.008);
    if (pas->ym_reg[2][0] & 0x80) {
        pas->irq_stat |= INT_MIDI;
        pas->ym_ivr |= 0x0e;
        pas->ym_isr |= 0x80;
        if (pas->irq_ena & INT_MIDI) PAS_RaiseIRQ();
    }
}

/* YM3802 registers 4-7 are banked by RGR bits 0-3; ym_reg[0..3] hold registers 4..7. */
static Bitu PAS_YM3802Read(unsigned int off) {
    switch (off) {
        case 0x1400: return pas->ym_ivr | (pas->ym_isr ? 0 : 0x10);
        case 0x1401: return pas->ym_rgr;
        case 0x1402: return pas->ym_isr;
        case 0x1800: return (pas->ym_idx == 5) ? 0xc0 : pas->ym_reg[0][pas->ym_idx]; /* TX FIFO empty */
        case 0x1801: return pas->ym_reg[1][pas->ym_idx];
        case 0x1802: return (pas->ym_idx == 3) ? 0x00 : pas->ym_reg[2][pas->ym_idx]; /* no MIDI input */
        case 0x1803: return pas->ym_reg[3][pas->ym_idx];
    }
    return 0xff;
}

static void PAS_YM3802Write(unsigned int off, uint8_t val) {
    const unsigned int i = pas->ym_idx;
    switch (off) {
        case 0x1401: {
            const uint8_t old = pas->ym_rgr;
            pas->ym_rgr = val;
            pas->ym_idx = val & 0x0f;
            if (old == 0x80 && val == 0x00) { /* reset */
                PIC_RemoveEvents(PAS_YM3802TimerEvent);
                pas->irq_stat &= 0xef;
                pas->ym_ivr = pas->ym_isr = 0;
                pas->ym_gen_timer = 0;
                memset(pas->ym_reg, 0, sizeof(pas->ym_reg));
            }
            break;
        }
        case 0x1403: /* ICR */
            pas->ym_ivr = pas->ym_reg[0][0] & 0xe0;
            pas->ym_isr &= (uint8_t)~val;
            if (!pas->ym_isr) pas->irq_stat &= 0x0f;
            if (!(pas->irq_stat & 0x0f) && !pas->ym_isr) PAS_LowerIRQ();
            break;
        case 0x1800:
            pas->ym_reg[0][i] = val;
            if (i == 0) pas->ym_ivr = (uint8_t)((val & 0xe0) | (pas->ym_ivr & 0x1f));
            if (i == 8) pas->ym_gen_timer = (uint16_t)((pas->ym_gen_timer & 0x3f00) | val);
            break;
        case 0x1801:
            pas->ym_reg[1][i] = val;
            if (i == 8) {
                pas->ym_gen_timer = (uint16_t)((pas->ym_gen_timer & 0x00ff) | ((val & 0x3f) << 8));
                PIC_RemoveEvents(PAS_YM3802TimerEvent);
                if ((val & 0x80) && pas->ym_gen_timer) PIC_AddEvent(PAS_YM3802TimerEvent, pas->ym_gen_timer * 0.008);
            }
            break;
        case 0x1802:
            pas->ym_reg[2][i] = val;
            if (i == 5 && (pas->ym_reg[1][5] & 0x01)) MIDI_RawOutByte(val);
            break;
        case 0x1803:
            pas->ym_reg[3][i] = val;
            break;
    }
}

/* ---- board registers ---------------------------------------------------------- */

static Bitu PAS_Read(Bitu port, Bitu iolen);
static void PAS_Write(Bitu port, Bitu val, Bitu iolen);

static void PAS_InstallPorts(bool on) {
    for (unsigned int k = 0; k < 64; k++) {
        pas->rd[k].Uninstall();
        pas->wr[k].Uninstall();
    }
    if (!on) return;
    /* base+0 is the OPL (adlib.cpp), and on the original PAS so is base+400h */
    for (unsigned int k = (pas->type == PAS_ORIGINAL) ? 2 : 1; k < 64; k++) {
        pas->rd[k].Install(pas->base + k * 0x400, PAS_Read, IO_MB, 4);
        pas->wr[k].Install(pas->base + k * 0x400, PAS_Write, IO_MB, 4);
    }
}

static void PAS_ApplySBCompat(void) {
    SB_PAS_SetCompat((pas->compat & 0x02) != 0, pas->sb_base, pas->sb_irq, pas->sb_dma);
}

/* 86Box pas16_reset_regs */
static void PAS_ResetRegs(void) {
    PAS_Sync();
    PAS_LowerIRQ();
    pas->sys_conf[0] &= 0xfd;
    pas->sys_conf[1] = pas->sys_conf[2] = 0;
    pas->prescale_div = 0;
    pas->audiofilt = 0;
    PAS_ResetPCM();
    pas->dma8_ff = false;
    pas->irq_ena = pas->irq_stat = 0;
    PAS_ResetMixer();
    PAS_UpdateFilter();
    PAS_UpdateChannel();
}

static int PAS_IRQConvert(unsigned int code) {
    if (code == 0) return -1;
    if (code <= 6) return (int)code + 1;
    if (code < 0x0b) return (int)code + 3;
    return (int)code + 4;
}

static Bitu PAS_Read(Bitu port, Bitu /*iolen*/) {
    const unsigned int off = (unsigned int)((port - pas->base) & 0xffff);
    const bool orig = pas->type == PAS_ORIGINAL;

    switch (off) {
        case 0x0800: return (pas->type == PAS_16) ? pas->audio_mixer : 0xff;
        case 0x0801: PAS_Sync(); return pas->irq_stat & 0xdf;
        case 0x0802: return pas->audiofilt;
        case 0x0803: return orig ? pas->irq_ena : (pas->irq_ena | 0x20); /* board revision bits read-only */
        case 0x0c02: return pas->pcm_ctrl;

        case 0x1000: case 0x1001: case 0x1002: return PAS_PITRead(off & 3);

        case 0x2401: return 0x00; /* board revision */
        case 0x4000: return pas->timeout_count;
        case 0x4001: return pas->timeout_status;
        case 0x7c01: return pas->enhancedscsi & ~0x01;
        case 0xbc00: return pas->waitstates;
        case 0xbc02: return pas->prescale_div;
        /* operation mode: bits 1-0 CD interface (3 = SCSI), bit 2 stereo FM, bit 3 16-bit */
        case 0xec03: return (pas->type == PAS_16) ? 0x0f : 0x07;
        case 0xfc00: return (pas->type == PAS_16) ? 0x0c : 0x01; /* board model */
        case 0xfc03: return (pas->type == PAS_16) ? 0x31 : 0x11; /* master mode: AT bus, XT/AT timing */
    }

    if (orig) {
        if ((off >= 0x1400 && off <= 0x1402) || (off >= 0x1800 && off <= 0x1803)) return PAS_YM3802Read(off);
        return 0xff;
    }

    switch (off) {
        case 0x1401: case 0x1403: return pas->midi_ctrl;
        case 0x1402: case 0x1802: {
            uint8_t ret = 0;
            if (pas->midi_uart_in) {
                /* loopback test: the driver writes AAh and reads it back */
                if (pas->midi_data == 0xaa && (pas->midi_ctrl & 0x04)) ret = pas->midi_data;
                pas->midi_stat &= ~0x04;
                PAS_UpdateMidiIRQ();
            }
            return ret;
        }
        case 0x1800: return pas->midi_stat;
        case 0x1801: return pas->fifo_stat;

        case 0x8000: case 0x8001: case 0x8002: case 0x8003: return pas->sys_conf[off & 3];
        case 0xf000: case 0xf001: case 0xf002: case 0xf003: return pas->io_conf[off & 3];

        case 0xf400: return (pas->compat & 0xf3) | (SB_PAS_IRQPending() ? 0x04 : 0x00);
        case 0xf401: return pas->compat_base;
        case 0xf802: return pas->sb_irqdma;
    }
    return 0xff;
}

static void PAS_Write(Bitu port, Bitu val_, Bitu /*iolen*/) {
    const unsigned int off = (unsigned int)((port - pas->base) & 0xffff);
    const uint8_t val = (uint8_t)val_;
    const bool orig = pas->type == PAS_ORIGINAL;

    switch (off) {
        case 0x0403:
            if (pas->type == PAS_16) PAS_MV508Write(val);
            return;
        case 0x0800:
            if (pas->type != PAS_16) {
                PAS_SerialMixerWrite(val);
            }
            else if (!(val & 0x01)) {
                PAS_Sync();
                pas->audio_mixer = val;
                PAS_ResetPCM();
                PAS_UpdateChannel();
            }
            return;
        case 0x0801:
            PAS_Sync();
            pas->irq_stat &= (uint8_t)~val;
            if (orig || !(pas->irq_stat & 0x1f)) PAS_LowerIRQ();
            return;
        case 0x0802:
            PAS_Sync();
            if (orig && (val & FILT_UNMUTE) && !(pas->audiofilt & FILT_UNMUTE)) pas->irq_ena = pas->irq_stat = 0;
            if ((val & FILT_GATE0) && !(pas->audiofilt & FILT_GATE0)) pas->c0_start = PIC_FullIndex();
            if ((val & FILT_GATE1) && !(pas->audiofilt & FILT_GATE1)) pas->pit[1].count = pas->pit[1].reload;
            pas->stereo_lr = false;
            pas->dma8_ff = false;
            pas->audiofilt = val;
            PAS_UpdateFilter();
            PAS_UpdateChannel();
            return;
        case 0x0803:
            PAS_Sync();
            pas->irq_ena = val & 0x1f;
            pas->irq_stat &= (uint8_t)((val & 0x1f) | 0xe0);
            if (!(pas->irq_stat & 0x1f)) PAS_LowerIRQ();
            return;
        case 0x0c00: case 0x0c01:
            PAS_Sync(); /* PIO sample data: not emulated (86Box neither) */
            return;
        case 0x0c02:
            PAS_Sync();
            if ((val & PCM_ENA) && !(pas->pcm_ctrl & PCM_ENA)) {
                pas->stereo_lr = false;
                pas->irq_stat &= 0xd7;
                pas->dma8_ff = false; /* 8-bit samples on a 16-bit channel */
            }
            pas->pcm_ctrl = val;
            PAS_UpdateChannel();
            return;

        case 0x1000: case 0x1001: case 0x1002: PAS_PITWrite(off & 3, val); return;
        case 0x1003: PAS_PITControl(val); return;

        case 0x4000: pas->timeout_count = val; return;
        case 0x4001: pas->timeout_status = val & 0x7f; return;
        case 0x7c01: pas->enhancedscsi = val; return;
        case 0xbc00: pas->waitstates = val; return;
        case 0xbc02:
            PAS_Sync();
            pas->prescale_div = val;
            PAS_UpdateChannel();
            return;
    }

    if (orig) {
        if ((off >= 0x1401 && off <= 0x1403) || (off >= 0x1800 && off <= 0x1803)) PAS_YM3802Write(off, val);
        return;
    }

    switch (off) {
        case 0x1401: case 0x1403:
            pas->midi_ctrl = val;
            if ((val & 0x60) == 0x60) pas->midi_uart_out = pas->midi_uart_in = false;
            else if ((val & 0x1c) == 0x04) pas->midi_uart_in = true;
            else pas->midi_uart_out = true;
            PAS_UpdateMidiIRQ();
            return;
        case 0x1402: case 0x1802:
            pas->midi_data = val;
            if (pas->midi_uart_out) MIDI_RawOutByte(val);
            return;
        case 0x1800:
            pas->midi_stat = val;
            PAS_UpdateMidiIRQ();
            return;
        case 0x1801:
            pas->fifo_stat = val;
            return;

        case 0x8000:
            PAS_Sync();
            if ((val & 0xc0) && !(pas->sys_conf[0] & 0xc0)) {
                /* board reset: also where a base written to 9A01h takes effect */
                PAS_ResetRegs();
                PAS_InstallPorts(false);
                pas->base = pas->new_base;
                PAS_InstallPorts(true);
                pas->sys_conf[0] = 0;
            }
            else {
                pas->sys_conf[0] = val;
            }
            PAS_UpdateChannel();
            return;
        case 0x8001: PAS_Sync(); pas->sys_conf[1] = val; return;
        case 0x8002: PAS_Sync(); pas->sys_conf[2] = val; PAS_UpdateChannel(); return;
        case 0x8003: pas->sys_conf[3] = val; return;

        case 0xf000: pas->io_conf[0] = val; return; /* bit 6 joystick enable: DOSBox-X's joystick stays put */
        case 0xf001:
            PAS_Sync();
            pas->io_conf[1] = val;
            PAS_SetDMA(pas_dmas[val & 7]);
            return;
        case 0xf002:
            pas->io_conf[2] = val;
            PAS_LowerIRQ();
            pas->irq = PAS_IRQConvert(val & 0x0f);
            return;
        case 0xf003: pas->io_conf[3] = val; return;

        case 0xf400: /* bit 1 SB emulation, bit 0 MPU-401 emulation (DOSBox-X's [midi] MPU stays put) */
            pas->compat = val & 0xf3;
            PAS_ApplySBCompat();
            return;
        case 0xf401:
            pas->compat_base = val;
            pas->sb_base = 0x200 | ((val & 0x0f) << 4);
            PAS_ApplySBCompat();
            return;
        case 0xf802: {
            pas->sb_irqdma = val;
            const int irq = pas_sb_irqs[(val >> 3) & 7];
            pas->sb_irq = irq ? (Bitu)irq : 0xff;
            pas->sb_dma = (val >> 6) & 3;
            PAS_ApplySBCompat();
            return;
        }
    }
}

/* 9A01h: master address pointer, shared by every PAS in the machine. Write the board
 * ID (BCh for the first board), then the new base >> 2. */
static void PAS_Write9A01(Bitu /*port*/, Bitu val, Bitu /*iolen*/) {
    if (pas->master_ff && pas->board_id == 0xbc)
        pas->new_base = (val & 0xff) << 2;
    else if (!pas->master_ff)
        pas->board_id = (uint8_t)val;
    pas->master_ff = !pas->master_ff;
}

/* ---- init --------------------------------------------------------------------- */

/* irq/dma are the [sblaster] values: the original PAS's jumpered native IRQ/DMA, or
 * the Sound Blaster emulation's starting IRQ/DMA on the PAS Plus/16. */
void PAS_Init(unsigned int type, Bitu sb_base, Bitu irq, Bitu dma, bool use_mixer) {
    PAS_BuildLMC835Tables();

    pas = new PAS_State();
    pas->type = type;
    pas->use_mixer = use_mixer;
    pas->chan = pas->mixobj.Install(PAS_CallBack, 22050, "PAS");
    pas->chan->Enable(false);
    if (MixerChannel *fm = MIXER_FindChannel("FM")) pas->fm_scale = fm->scale[0];

    if (type == PAS_ORIGINAL) {
        pas->irq = (irq == 0xff) ? -1 : (int)irq;
        PAS_SetDMA((dma == 0xff) ? 4 : (int)dma);
        /* Power up with the FM split off, so AdLib-only software is not
         * left-only without MVSOUND.SYS. The real board's reset state is unverified. */
        pas->audio_mixer = SM_FM_MONO;
    }
    else {
        /* 86Box defaults; MVSOUND.SYS reprograms them through F389h/F38Ah */
        pas->irq = (type == PAS_16) ? 10 : 5;
        pas->io_conf[2] = (type == PAS_16) ? 0x07 : 0x04;
        pas->io_conf[1] = 0x03;
        PAS_SetDMA(3);

        pas->sb_base = sb_base;
        pas->sb_irq = irq;
        pas->sb_dma = dma;
        pas->compat = 0x02; /* Sound Blaster emulation on */
        pas->compat_base = (uint8_t)(0x30 | ((sb_base >> 4) & 0x0f)); /* MPU-401 at 330h */
        uint8_t irq_code = 0;
        for (uint8_t i = 1; i < 8; i++) if ((Bitu)pas_sb_irqs[i] == irq) irq_code = i;
        pas->sb_irqdma = (uint8_t)((irq_code << 3) | ((dma & 3) << 6));

        pas->wr_9a01.Install(0x9a01, PAS_Write9A01, IO_MB);
    }

    PAS_ResetRegs();
    PAS_InstallPorts(true);

    LOG(LOG_SB, LOG_NORMAL)("Pro AudioSpectrum%s at %03Xh, IRQ %d, DMA %d",
        type == PAS_16 ? " 16" : (type == PAS_PLUS ? " Plus" : ""), (unsigned int)pas->base, pas->irq, pas->dma);
}

/* adlib.cpp: does 388h drive only the left OPL2 (FM split on, B88h bit 7 clear)? */
bool PAS_FMSplit(void) {
    return pas && pas->type == PAS_ORIGINAL && !(pas->audio_mixer & SM_FM_MONO);
}

void PAS_ShutDown(void) {
    if (!pas) return;
    PIC_RemoveEvents(PAS_YM3802TimerEvent);
    PAS_LowerIRQ();
    if (pas->dma_chan && pas->dma_chan->callback == PAS_DMA_CallBack) pas->dma_chan->Register_Callback(nullptr);
    if (MixerChannel *fm = MIXER_FindChannel("FM")) fm->SetScale(pas->fm_scale);
    delete pas;
    pas = nullptr;
}
