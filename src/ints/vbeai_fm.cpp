/*
 *  MIDI-to-OPL synthesiser for the DOSBox-X VBE/AI provider.
 *
 *  VBE/AI's MIDI device class covers two quite different kinds of driver:
 *  a transmitter that forwards the stream to an external synthesiser, and one
 *  that *interprets* it -- "for other devices, such as the OPL3 FM synthesizer,
 *  the driver will be interpreting the MIDI stream to play the individual
 *  notes" (spec 5.1). int10_vesa_ai.cpp implements the former; this file is
 *  the latter.
 *
 *  The chip is private to this provider rather than the one [sblaster] drives.
 *  Sharing would have been more faithful to a real 1994 machine, which had
 *  exactly one OPL, but it would also make midimode=opl2 silently produce
 *  nothing whenever oplmode=none, and would let a game's Adlib writes and the
 *  VBE/AI stream fight over the same registers. See docs/vbeai.md.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#include <math.h>           /* adlib.h, via dbopl.h, uses fmod() without including it */

#include "dosbox.h"
#include "logging.h"
#include "mem.h"
#include "mixer.h"
#include "setup.h"
#include "vbeai_fm.h"
#include "hardware/dbopl.h"

/* ---------------------------------------------------------------------------
 * Patch representation
 *
 * A two-operator voice is five registers per operator plus one shared. That is
 * exactly what the spec's OPL2 patch carries, field for field, so the SDK patch
 * format converts into this without loss.
 * ------------------------------------------------------------------------- */

struct FMOp {
    uint8_t am_vib_eg_ksr_mult;     /* 0x20: AM VIB EGT KSR MULT   */
    uint8_t ksl_tl;                 /* 0x40: KSL TL (0 = loudest)  */
    uint8_t ar_dr;                  /* 0x60: attack, decay         */
    uint8_t sl_rr;                  /* 0x80: sustain level, release*/
    uint8_t waveform;               /* 0xE0: waveform select       */
};

struct FMPatch {
    FMOp        op[2];              /* [0] modulator, [1] carrier  */
    uint8_t     fb_cnt;             /* 0xC0: feedback<<1 | connection */
    int8_t      transpose;          /* semitones                   */
    uint8_t     fixed_note;         /* non-zero: ignore the key, play this */
};

/* ---------------------------------------------------------------------------
 * The built-in bank
 *
 * These timbres are original and deliberately plain. The obvious candidate for
 * a real bank -- The Fat Man's FATV10.BNK, which ships with the VESA SDK -- is
 * not redistributable: it carries a per-product licence fee and an on-screen
 * credit requirement, so it cannot be bundled with DOSBox-X. Rather than ship
 * nothing and force every application down the patch-library path, there is one
 * rough voice per General MIDI family here, and msPreLoadPatch lets anyone who
 * does hold a proper bank override them per program.
 *
 * They are meant to be recognisable, not good.
 * ------------------------------------------------------------------------- */

#define FAMILY_COUNT    16

static const FMPatch fm_family[FAMILY_COUNT] = {
    /* 0  Piano: struck, bright attack, decays away                         */
    { { {0x01,0x4F,0xF2,0x53,0x00}, {0x01,0x00,0xF2,0x74,0x00} }, 0x06, 0, 0 },
    /* 1  Chromatic percussion: bell-like, fast attack, long ring           */
    { { {0x07,0x4C,0xF8,0xF5,0x00}, {0x01,0x00,0xF8,0xF6,0x00} }, 0x08, 0, 0 },
    /* 2  Organ: sustaining, no decay                                       */
    { { {0x21,0x1B,0xF0,0xFF,0x00}, {0x21,0x00,0xF0,0xFF,0x00} }, 0x0A, 0, 0 },
    /* 3  Guitar: plucked                                                   */
    { { {0x01,0x2C,0xF6,0x86,0x00}, {0x11,0x00,0xF5,0x57,0x00} }, 0x08, 0, 0 },
    /* 4  Bass: short, punchy, low multiplier                               */
    { { {0x01,0x1D,0xF8,0x87,0x00}, {0x01,0x00,0xF8,0x68,0x00} }, 0x08, 0, 0 },
    /* 5  Strings: slow attack, sustaining                                  */
    { { {0x21,0x1E,0x41,0x1F,0x00}, {0x21,0x00,0x51,0x1F,0x00} }, 0x0A, 0, 0 },
    /* 6  Ensemble: sustaining, brighter                                    */
    { { {0x21,0x18,0x61,0x2F,0x00}, {0x21,0x00,0x61,0x2F,0x00} }, 0x0C, 0, 0 },
    /* 7  Brass: medium attack, sustaining, some bite                       */
    { { {0x21,0x16,0x91,0x1F,0x00}, {0x21,0x00,0x81,0x1F,0x00} }, 0x0C, 0, 0 },
    /* 8  Reed: sustaining, nasal                                           */
    { { {0x31,0x1C,0x71,0x2F,0x01}, {0x21,0x00,0x71,0x2F,0x00} }, 0x0A, 0, 0 },
    /* 9  Pipe: soft, flute-like, nearly pure                               */
    { { {0x21,0x2B,0x61,0x3F,0x00}, {0x21,0x00,0x61,0x3F,0x00} }, 0x0E, 0, 0 },
    /* 10 Synth lead: bright sustaining saw-ish                             */
    { { {0x21,0x14,0xA1,0x1F,0x02}, {0x21,0x00,0xA1,0x1F,0x01} }, 0x0A, 0, 0 },
    /* 11 Synth pad: very slow attack                                       */
    { { {0x61,0x1E,0x21,0x1F,0x00}, {0x21,0x00,0x31,0x1F,0x00} }, 0x0A, 0, 0 },
    /* 12 Synth effects: detuned, slow                                      */
    { { {0xA1,0x1A,0x31,0x2F,0x03}, {0x61,0x00,0x41,0x2F,0x02} }, 0x0C, 0, 0 },
    /* 13 Ethnic: plucked, short                                            */
    { { {0x01,0x26,0xF7,0x76,0x00}, {0x11,0x00,0xF6,0x56,0x00} }, 0x08, 0, 0 },
    /* 14 Percussive: very short                                            */
    { { {0x01,0x2A,0xF9,0xA7,0x00}, {0x01,0x00,0xF9,0x98,0x00} }, 0x06, 0, 0 },
    /* 15 Sound effects: noisy, unpitched-ish                               */
    { { {0x0E,0x1F,0xF4,0x54,0x03}, {0x0E,0x00,0xF4,0x54,0x03} }, 0x00, 0, 0 }
};

/* Percussion voices for MIDI channel 10, each at a fixed pitch. */
enum { PERC_BASS = 0, PERC_SNARE, PERC_HIHAT, PERC_OPENHAT, PERC_TOM, PERC_CYMBAL, PERC_COUNT };

static const FMPatch fm_perc[PERC_COUNT] = {
    /* bass drum   */ { { {0x01,0x14,0xF8,0xF7,0x00}, {0x01,0x00,0xF8,0xB8,0x00} }, 0x06, 0, 28 },
    /* snare       */ { { {0x0C,0x18,0xF9,0xF8,0x03}, {0x0E,0x00,0xF9,0xC8,0x03} }, 0x00, 0, 50 },
    /* closed hat  */ { { {0x0E,0x2A,0xFA,0xFA,0x03}, {0x0E,0x00,0xFA,0xDA,0x03} }, 0x00, 0, 72 },
    /* open hat    */ { { {0x0E,0x2A,0xF6,0x86,0x03}, {0x0E,0x00,0xF6,0x56,0x03} }, 0x00, 0, 72 },
    /* tom         */ { { {0x01,0x18,0xF8,0xB7,0x00}, {0x01,0x00,0xF8,0x98,0x00} }, 0x06, 0, 41 },
    /* cymbal      */ { { {0x0E,0x26,0xF4,0x54,0x03}, {0x0E,0x00,0xF4,0x35,0x03} }, 0x00, 0, 76 }
};

/* GM drum note (35..81) -> percussion voice. Anything outside falls back to
 * the tom, which is inoffensive. */
static uint8_t PercVoiceFor(uint8_t note) {
    switch (note) {
    case 35: case 36:                      return PERC_BASS;
    case 37: case 38: case 39: case 40:    return PERC_SNARE;
    case 42: case 44:                      return PERC_HIHAT;
    case 46:                               return PERC_OPENHAT;
    case 49: case 52: case 55: case 57:
    case 51: case 53: case 59:             return PERC_CYMBAL;
    default:                               return PERC_TOM;
    }
}

/* ---------------------------------------------------------------------------
 * Chip state
 * ------------------------------------------------------------------------- */

#define MAX_VOICES      18
#define MIDI_CHANNELS   16
#define GM_PROGRAMS     128

/* Operator offsets for the nine voices of one OPL bank. */
static const uint8_t op_base[9] = { 0x00,0x01,0x02, 0x08,0x09,0x0A, 0x10,0x11,0x12 };

/* One octave of F-numbers; block supplies the octave.  Computed as
 *     fnum = freq * 2^(20-block) / 49716,  block 4, A4 = 440 Hz
 * and rounded, which lands every semitone within 0.03% of equal temperament.
 * The table many period drivers used (343, 363, 385 ...) is a systematic 9
 * cents flat; there is no reason to reproduce that. */
static const uint16_t fnum_table[12] = {
    345, 365, 387, 410, 434, 460, 488, 517, 547, 580, 614, 651
};

struct FMVoice {
    bool        active;
    bool        sustained;      /* released but held by the sustain pedal */
    uint8_t     channel;
    uint8_t     note;
    uint8_t     velocity;
    uint32_t    age;            /* allocation order, for stealing */
    uint16_t    fnum;
    uint8_t     block;
};

struct FMChannel {
    uint8_t     program;
    uint8_t     volume;         /* CC7  */
    uint8_t     expression;     /* CC11 */
    uint8_t     pan;            /* CC10 */
    bool        sustain;        /* CC64 */
    int16_t     bend;           /* -8192..8191 */
};

static struct {
    bool            active = false;
    bool            opl3 = false;
    unsigned        voices = 9;

    DBOPL::Handler* chip = NULL;
    MixerChannel*   chan = NULL;

    FMVoice         voice[MAX_VOICES];
    FMChannel       ch[MIDI_CHANNELS];
    FMPatch         patch[GM_PROGRAMS];
    bool            patch_custom[GM_PROGRAMS];

    uint16_t        steal_mask = 0xffff;
    uint32_t        clock = 0;

    /* MIDI byte parser */
    uint8_t         status = 0;
    uint8_t         data[2];
    unsigned        datalen = 0;
    bool            in_sysex = false;
} fm;

/* ---------------------------------------------------------------------------
 * Register plumbing
 * ------------------------------------------------------------------------- */

static void FM_Write(uint16_t reg, uint8_t val) {
    if (fm.chip == NULL) return;
    fm.chip->WriteReg(reg, val);
}

/* Voices 9..17 live in the OPL3 second register bank at +0x100. */
static inline uint16_t VoiceBank(unsigned v) { return (v >= 9) ? 0x100 : 0x000; }
static inline uint8_t  VoiceIndex(unsigned v) { return (uint8_t)(v % 9); }

static void FM_MixerCallback(Bitu len) {
    if (fm.chip == NULL || fm.chan == NULL) return;
    fm.chip->Generate(fm.chan, len);
}

/* ---------------------------------------------------------------------------
 * Voice programming
 * ------------------------------------------------------------------------- */

static const FMPatch *PatchFor(uint8_t channel, uint8_t note) {
    if (channel == 9) return &fm_perc[PercVoiceFor(note)];
    return &fm.patch[fm.ch[channel].program & 0x7f];
}

/* Carrier attenuation from note velocity, channel volume and expression.
 * TL runs 0 (loudest) to 63 (silent), so this is an attenuation, not a gain. */
static uint8_t CarrierTL(const FMPatch *p, uint8_t channel, uint8_t velocity) {
    const FMChannel &c = fm.ch[channel];
    uint32_t scale = (uint32_t)velocity * c.volume * c.expression;   /* 0..127^3 */
    uint32_t base  = p->op[1].ksl_tl & 0x3f;

    /* 0..127^3 mapped onto 0..47 of extra attenuation, quietest when scale=0 */
    uint32_t atten = 47;
    if (scale > 0) {
        /* cheap log-ish curve: four halvings of the range */
        uint32_t s = scale >> 12;           /* 0..511 */
        if      (s >= 256) atten = 0;
        else if (s >= 128) atten = 6;
        else if (s >= 64)  atten = 12;
        else if (s >= 32)  atten = 18;
        else if (s >= 16)  atten = 24;
        else if (s >= 8)   atten = 30;
        else if (s >= 4)   atten = 36;
        else if (s >= 2)   atten = 42;
        else               atten = 46;
    }

    uint32_t tl = base + atten;
    if (tl > 63) tl = 63;
    return (uint8_t)tl;
}

static void ProgramVoice(unsigned v, const FMPatch *p, uint8_t channel, uint8_t velocity) {
    const uint16_t bank = VoiceBank(v);
    const uint8_t  idx  = VoiceIndex(v);
    const uint8_t  o0   = op_base[idx];
    const uint8_t  o1   = (uint8_t)(op_base[idx] + 3);

    FM_Write((uint16_t)(bank + 0x20 + o0), p->op[0].am_vib_eg_ksr_mult);
    FM_Write((uint16_t)(bank + 0x40 + o0), p->op[0].ksl_tl);
    FM_Write((uint16_t)(bank + 0x60 + o0), p->op[0].ar_dr);
    FM_Write((uint16_t)(bank + 0x80 + o0), p->op[0].sl_rr);
    FM_Write((uint16_t)(bank + 0xE0 + o0), p->op[0].waveform);

    FM_Write((uint16_t)(bank + 0x20 + o1), p->op[1].am_vib_eg_ksr_mult);
    FM_Write((uint16_t)(bank + 0x40 + o1),
             (uint8_t)((p->op[1].ksl_tl & 0xc0) | CarrierTL(p, channel, velocity)));
    FM_Write((uint16_t)(bank + 0x60 + o1), p->op[1].ar_dr);
    FM_Write((uint16_t)(bank + 0x80 + o1), p->op[1].sl_rr);
    FM_Write((uint16_t)(bank + 0xE0 + o1), p->op[1].waveform);

    /* On OPL3 the two stereo enable bits must be set or the voice is silent.
     * Pan hard left/right at the extremes, both otherwise. */
    uint8_t cd = 0x30;
    if (fm.opl3) {
        const uint8_t pan = fm.ch[channel].pan;
        if      (pan < 32)  cd = 0x10;      /* left  */
        else if (pan > 95)  cd = 0x20;      /* right */
    }
    FM_Write((uint16_t)(bank + 0xC0 + idx), (uint8_t)(p->fb_cnt | (fm.opl3 ? cd : 0)));
}

static void NoteFrequency(uint8_t channel, uint8_t note, uint16_t *fnum, uint8_t *block) {
    int n = (int)note;
    int oct;

    if (n < 0)   n = 0;
    if (n > 127) n = 127;

    oct = (n / 12) - 1;
    if (oct < 0) oct = 0;
    if (oct > 7) oct = 7;

    uint32_t f = fnum_table[n % 12];

    /* Pitch bend, +/- 2 semitones. A semitone is about a 5.95% frequency step;
     * scaling the F-number directly is accurate enough over that range. */
    const int16_t bend = fm.ch[channel].bend;
    if (bend != 0) {
        /* +/-8192 maps to +/-2 semitones => at most about 12.2% */
        int32_t adj = ((int32_t)f * (int32_t)bend * 1225) / (8192 * 10000);
        int32_t nf  = (int32_t)f + adj;
        if (nf < 1)    nf = 1;
        if (nf > 1023) nf = 1023;
        f = (uint32_t)nf;
    }

    *fnum  = (uint16_t)f;
    *block = (uint8_t)oct;
}

static void KeyOn(unsigned v) {
    const uint16_t bank = VoiceBank(v);
    const uint8_t  idx  = VoiceIndex(v);
    FMVoice &vo = fm.voice[v];

    FM_Write((uint16_t)(bank + 0xA0 + idx), (uint8_t)(vo.fnum & 0xff));
    FM_Write((uint16_t)(bank + 0xB0 + idx),
             (uint8_t)(0x20 | ((vo.block & 7) << 2) | ((vo.fnum >> 8) & 3)));
}

static void KeyOff(unsigned v) {
    const uint16_t bank = VoiceBank(v);
    const uint8_t  idx  = VoiceIndex(v);
    FMVoice &vo = fm.voice[v];

    FM_Write((uint16_t)(bank + 0xB0 + idx),
             (uint8_t)(((vo.block & 7) << 2) | ((vo.fnum >> 8) & 3)));
}

/* ---------------------------------------------------------------------------
 * Voice allocation
 * ------------------------------------------------------------------------- */

static unsigned AllocVoice(uint8_t channel) {
    unsigned i;
    unsigned oldest = MAX_VOICES;
    uint32_t oldest_age = 0xffffffffUL;

    for (i = 0; i < fm.voices; i++)
        if (!fm.voice[i].active && !fm.voice[i].sustained) return i;

    /* Nothing free. Prefer a voice only being held by the sustain pedal. */
    for (i = 0; i < fm.voices; i++) {
        if (fm.voice[i].sustained && !fm.voice[i].active) {
            if (fm.voice[i].age < oldest_age) { oldest_age = fm.voice[i].age; oldest = i; }
        }
    }
    if (oldest < MAX_VOICES) return oldest;

    /* Steal, unless this channel has stealing disabled. MIDIVOICESTEAL sets a
     * bit per channel; the spec's default is stealing enabled everywhere. */
    if (!(fm.steal_mask & (1u << channel))) return MAX_VOICES;

    oldest_age = 0xffffffffUL;
    for (i = 0; i < fm.voices; i++) {
        if (fm.voice[i].age < oldest_age) { oldest_age = fm.voice[i].age; oldest = i; }
    }
    return oldest;
}

static void DoNoteOff(uint8_t channel, uint8_t note) {
    for (unsigned i = 0; i < fm.voices; i++) {
        FMVoice &v = fm.voice[i];
        if (!v.active || v.channel != channel || v.note != note) continue;

        if (fm.ch[channel].sustain) {
            v.active = false;
            v.sustained = true;         /* keep sounding until the pedal lifts */
        }
        else {
            KeyOff(i);
            v.active = false;
            v.sustained = false;
        }
    }
}

/* Hard-release every voice playing this note, ignoring the sustain pedal.
 * Used when the same key is struck again: the old voice must be reclaimed, not
 * left pedal-sustained, or repeated notes under the pedal pile up one voice per
 * strike and starve the chip. */
static void ReleaseNote(uint8_t channel, uint8_t note) {
    for (unsigned i = 0; i < fm.voices; i++) {
        FMVoice &v = fm.voice[i];
        if ((!v.active && !v.sustained) || v.channel != channel || v.note != note) continue;
        KeyOff(i);
        v.active = false;
        v.sustained = false;
    }
}

static void DoNoteOn(uint8_t channel, uint8_t note, uint8_t velocity) {
    if (velocity == 0) { DoNoteOff(channel, note); return; }

    /* Re-trigger rather than stack a duplicate of the same note. */
    ReleaseNote(channel, note);

    const unsigned v = AllocVoice(channel);
    if (v >= MAX_VOICES) return;        /* stealing disabled and nothing free */

    if (fm.voice[v].active || fm.voice[v].sustained) KeyOff(v);

    const FMPatch *p = PatchFor(channel, note);
    uint8_t play = note;

    if (p->fixed_note) play = p->fixed_note;
    else if (p->transpose) {
        int n = (int)note + p->transpose;
        play = (uint8_t)((n < 0) ? 0 : (n > 127 ? 127 : n));
    }

    FMVoice &vo = fm.voice[v];
    vo.active = true;
    vo.sustained = false;
    vo.channel = channel;
    vo.note = note;
    vo.velocity = velocity;
    vo.age = fm.clock++;
    NoteFrequency(channel, play, &vo.fnum, &vo.block);

    ProgramVoice(v, p, channel, velocity);
    KeyOn(v);
}

static void AllNotesOff(uint8_t channel) {
    for (unsigned i = 0; i < fm.voices; i++) {
        if ((fm.voice[i].active || fm.voice[i].sustained) && fm.voice[i].channel == channel) {
            KeyOff(i);
            fm.voice[i].active = false;
            fm.voice[i].sustained = false;
        }
    }
}

static void SustainOff(uint8_t channel) {
    for (unsigned i = 0; i < fm.voices; i++) {
        if (fm.voice[i].sustained && fm.voice[i].channel == channel) {
            KeyOff(i);
            fm.voice[i].sustained = false;
        }
    }
}

/* Re-pitch the sounding voices of a channel, after a pitch bend. */
static void RepitchChannel(uint8_t channel) {
    for (unsigned i = 0; i < fm.voices; i++) {
        FMVoice &v = fm.voice[i];
        if (!v.active || v.channel != channel) continue;

        const FMPatch *p = PatchFor(channel, v.note);
        uint8_t play = v.note;
        if (p->fixed_note) play = p->fixed_note;
        else if (p->transpose) {
            int n = (int)v.note + p->transpose;
            play = (uint8_t)((n < 0) ? 0 : (n > 127 ? 127 : n));
        }
        NoteFrequency(channel, play, &v.fnum, &v.block);
        KeyOn(i);                        /* rewrites A0/B0 with key still on */
    }
}

/* Re-apply carrier level to sounding voices, after a volume/expression change. */
static void RelevelChannel(uint8_t channel) {
    for (unsigned i = 0; i < fm.voices; i++) {
        FMVoice &v = fm.voice[i];
        if (!v.active || v.channel != channel) continue;

        const FMPatch *p = PatchFor(channel, v.note);
        const uint16_t bank = VoiceBank(i);
        const uint8_t  o1 = (uint8_t)(op_base[VoiceIndex(i)] + 3);
        FM_Write((uint16_t)(bank + 0x40 + o1),
                 (uint8_t)((p->op[1].ksl_tl & 0xc0) | CarrierTL(p, channel, v.velocity)));
    }
}

/* ---------------------------------------------------------------------------
 * MIDI message handling
 * ------------------------------------------------------------------------- */

static void Controller(uint8_t channel, uint8_t cc, uint8_t value) {
    switch (cc) {
    case 7:                                     /* Main Volume  */
        fm.ch[channel].volume = value;
        RelevelChannel(channel);
        break;
    case 10:                                    /* Pan          */
        fm.ch[channel].pan = value;
        break;
    case 11:                                    /* Expression   */
        fm.ch[channel].expression = value;
        RelevelChannel(channel);
        break;
    case 64:                                    /* Sustain      */
        fm.ch[channel].sustain = (value >= 64);
        if (!fm.ch[channel].sustain) SustainOff(channel);
        break;
    case 120:                                   /* All Sound Off */
    case 123:                                   /* All Notes Off */
        AllNotesOff(channel);
        break;
    case 121:                                   /* Reset All Controllers */
        fm.ch[channel].volume = 100;
        fm.ch[channel].expression = 127;
        fm.ch[channel].pan = 64;
        fm.ch[channel].sustain = false;
        fm.ch[channel].bend = 0;
        SustainOff(channel);
        RepitchChannel(channel);
        RelevelChannel(channel);
        break;
    default:
        /* Modulation, reverb and chorus are in the spec's list but have no
         * meaningful OPL equivalent; ignoring them is better than faking it. */
        break;
    }
}

static void Dispatch(uint8_t status, const uint8_t *d, unsigned n) {
    const uint8_t channel = (uint8_t)(status & 0x0f);
    const uint8_t kind    = (uint8_t)(status & 0xf0);

    switch (kind) {
    case 0x80: if (n >= 2) DoNoteOff(channel, d[0]);              break;
    case 0x90: if (n >= 2) DoNoteOn(channel, d[0], d[1]);         break;
    case 0xA0:                                                     break;  /* poly key pressure */
    case 0xB0: if (n >= 2) Controller(channel, d[0], d[1]);       break;
    case 0xC0:
        if (n >= 1) fm.ch[channel].program = (uint8_t)(d[0] & 0x7f);
        break;
    case 0xD0:                                                     break;  /* channel pressure */
    case 0xE0:
        if (n >= 2) {
            fm.ch[channel].bend = (int16_t)(((int)d[1] << 7 | d[0]) - 8192);
            RepitchChannel(channel);
        }
        break;
    default:                                                       break;
    }
}

static unsigned MessageLength(uint8_t status) {
    switch (status & 0xf0) {
    case 0xC0: case 0xD0:   return 1;
    case 0xF0:              return 0;
    default:                return 2;
    }
}

void VBEAI_FM_Byte(uint8_t b) {
    if (!fm.active) return;

    if (b >= 0xf8) return;                      /* realtime: nothing to do */

    if (fm.in_sysex) {
        if (b == 0xf7) fm.in_sysex = false;
        else if (b & 0x80) { fm.in_sysex = false; /* fall through to handle it */ }
        else return;
    }

    if (b & 0x80) {
        if (b == 0xf0) { fm.in_sysex = true; fm.status = 0; return; }
        if (b >= 0xf0) { fm.status = 0; fm.datalen = 0; return; }   /* other system msgs */
        fm.status = b;
        fm.datalen = 0;
        return;
    }

    if (fm.status == 0) return;                 /* data with no running status */

    fm.data[fm.datalen++] = b;
    if (fm.datalen >= MessageLength(fm.status)) {
        Dispatch(fm.status, fm.data, fm.datalen);
        fm.datalen = 0;                         /* running status stays armed */
    }
}

/* ---------------------------------------------------------------------------
 * Patch loading (msPreLoadPatch)
 * ------------------------------------------------------------------------- */

/* Spec 7.2.1. Each operator is 13 signed chars, one field per OPL parameter,
 * which we pack back into the five registers the chip actually wants. */
#define OPL2OPR_LEN     13

static void PackOp(FMOp *out, PhysPt src, uint8_t waveform) {
    const uint8_t ksl       = mem_readb(src + 0)  & 0x03;
    const uint8_t freqMult  = mem_readb(src + 1)  & 0x0f;
    const uint8_t attack    = mem_readb(src + 3)  & 0x0f;
    const uint8_t sustLevel = mem_readb(src + 4)  & 0x0f;
    const uint8_t sustain   = mem_readb(src + 5)  & 0x01;
    const uint8_t decay     = mem_readb(src + 6)  & 0x0f;
    const uint8_t release   = mem_readb(src + 7)  & 0x0f;
    const uint8_t output    = mem_readb(src + 8)  & 0x3f;
    const uint8_t am        = mem_readb(src + 9)  & 0x01;
    const uint8_t vib       = mem_readb(src + 10) & 0x01;
    const uint8_t ksr       = mem_readb(src + 11) & 0x01;

    out->am_vib_eg_ksr_mult = (uint8_t)((am << 7) | (vib << 6) | (sustain << 5) |
                                        (ksr << 4) | freqMult);
    out->ksl_tl   = (uint8_t)((ksl << 6) | output);
    out->ar_dr    = (uint8_t)((attack << 4) | decay);
    out->sl_rr    = (uint8_t)((sustLevel << 4) | release);
    out->waveform = (uint8_t)(waveform & 0x07);
}

bool VBEAI_FM_LoadPatch(uint16_t type, uint16_t program, PhysPt data, uint32_t len) {
    if (!fm.active || program >= GM_PROGRAMS) return false;

    if (type == VBEAI_PATCH_OPL2) {
        /* patchtype(2) mode(1) percVoice(1) op0(13) op1(13) wave0(1) wave1(1) */
        if (len < 32) return false;

        FMPatch p;
        const uint8_t feedback = mem_readb(data + 4 + 2) & 0x07;   /* op0 feedBack */
        const uint8_t connect  = mem_readb(data + 4 + 12) & 0x01;  /* op0 fm flag  */

        PackOp(&p.op[0], data + 4,                 mem_readb(data + 4 + OPL2OPR_LEN * 2));
        PackOp(&p.op[1], data + 4 + OPL2OPR_LEN,   mem_readb(data + 4 + OPL2OPR_LEN * 2 + 1));

        /* opl2fm is 1 for frequency modulation, and the chip's connection bit
         * is 1 for *additive*, so it inverts. */
        p.fb_cnt = (uint8_t)((feedback << 1) | (connect ? 0 : 1));
        p.transpose = 0;
        p.fixed_note = 0;

        fm.patch[program] = p;
        fm.patch_custom[program] = true;
        return true;
    }

    if (type == VBEAI_PATCH_OPL3 && fm.opl3) {
        /* patchtype(2) reg20h[4] reg40h[4] reg60h[4] reg80h[4] regE0h[2] regC0h[2]
         * Raw four-operator register images. We run two-operator voices, so
         * take the first operator pair, which is the 2-op subset of the same
         * layout, and keep its channel register. */
        if (len < 22) return false;

        FMPatch p;
        p.op[0].am_vib_eg_ksr_mult = mem_readb(data + 2 + 0);
        p.op[1].am_vib_eg_ksr_mult = mem_readb(data + 2 + 1);
        p.op[0].ksl_tl             = mem_readb(data + 2 + 4);
        p.op[1].ksl_tl             = mem_readb(data + 2 + 5);
        p.op[0].ar_dr              = mem_readb(data + 2 + 8);
        p.op[1].ar_dr              = mem_readb(data + 2 + 9);
        p.op[0].sl_rr              = mem_readb(data + 2 + 12);
        p.op[1].sl_rr              = mem_readb(data + 2 + 13);
        p.op[0].waveform           = mem_readb(data + 2 + 16) & 0x07;
        p.op[1].waveform           = mem_readb(data + 2 + 17) & 0x07;
        p.fb_cnt                   = mem_readb(data + 2 + 18) & 0x0f;
        p.transpose = 0;
        p.fixed_note = 0;

        fm.patch[program] = p;
        fm.patch_custom[program] = true;
        return true;
    }

    return false;
}

void VBEAI_FM_UnloadPatch(uint16_t program) {
    if (program >= GM_PROGRAMS) return;
    fm.patch[program] = fm_family[program / 8];
    fm.patch_custom[program] = false;
}

/* ---------------------------------------------------------------------------
 * Lifecycle
 * ------------------------------------------------------------------------- */

static void ResetChannels(void) {
    for (unsigned c = 0; c < MIDI_CHANNELS; c++) {
        fm.ch[c].program = 0;
        fm.ch[c].volume = 100;
        fm.ch[c].expression = 127;
        fm.ch[c].pan = 64;
        fm.ch[c].sustain = false;
        fm.ch[c].bend = 0;
    }
    for (unsigned v = 0; v < MAX_VOICES; v++) {
        fm.voice[v].active = false;
        fm.voice[v].sustained = false;
        fm.voice[v].channel = 0;
        fm.voice[v].note = 0;
        fm.voice[v].velocity = 0;
        fm.voice[v].age = 0;
        fm.voice[v].fnum = 0;
        fm.voice[v].block = 0;
    }
    fm.clock = 0;
    fm.status = 0;
    fm.datalen = 0;
    fm.in_sysex = false;
}

static void SilenceChip(void) {
    if (fm.chip == NULL) return;

    for (unsigned v = 0; v < fm.voices; v++) {
        const uint16_t bank = VoiceBank(v);
        FM_Write((uint16_t)(bank + 0xB0 + VoiceIndex(v)), 0x00);
    }
    /* percussion mode off, vibrato/tremolo depth default */
    FM_Write(0xBD, 0x00);
}

void VBEAI_FM_Reset(void) {
    if (!fm.active) return;

    SilenceChip();
    ResetChannels();
    for (unsigned i = 0; i < GM_PROGRAMS; i++) VBEAI_FM_UnloadPatch((uint16_t)i);
}

bool VBEAI_FM_Init(bool opl3) {
    if (fm.active && fm.opl3 == opl3) { VBEAI_FM_Reset(); return true; }

    VBEAI_FM_ShutDown();

    fm.opl3 = opl3;
    fm.voices = opl3 ? 18 : 9;

    fm.chip = new DBOPL::Handler(opl3);

    if (fm.chan == NULL) {
        /* Created once and kept, like the WAVE channel, to avoid any static
         * destruction ordering question against the mixer. */
        fm.chan = MIXER_AddChannel(&FM_MixerCallback, 49716, "VBEAIFM");
        if (fm.chan == NULL) {
            LOG(LOG_MISC, LOG_WARN)("VBE/AI: could not create the FM mixer channel");
            delete fm.chip;
            fm.chip = NULL;
            return false;
        }
    }

    fm.chip->Init(49716);
    fm.active = true;

    /* OPL3 needs the NEW bit before the second bank or the wider waveforms
     * respond to anything. */
    if (opl3) FM_Write(0x105, 0x01);
    FM_Write(0x01, 0x20);               /* enable waveform select on OPL2 */

    VBEAI_FM_Reset();
    if (fm.chan) fm.chan->Enable(true);

    LOG(LOG_MISC, LOG_DEBUG)("VBE/AI: FM synthesiser ready, %s, %u voices",
        opl3 ? "OPL3" : "OPL2", fm.voices);
    return true;
}

void VBEAI_FM_ShutDown(void) {
    if (fm.chan) fm.chan->Enable(false);
    if (fm.chip) { SilenceChip(); delete fm.chip; fm.chip = NULL; }
    fm.active = false;
}

/* ---------------------------------------------------------------------------
 * Queries
 * ------------------------------------------------------------------------- */

unsigned VBEAI_FM_FreeVoices(void) {
    if (!fm.active) return 0;

    unsigned n = 0;
    for (unsigned i = 0; i < fm.voices; i++)
        if (!fm.voice[i].active && !fm.voice[i].sustained) n++;
    return n;
}

unsigned VBEAI_FM_TotalVoices(void) { return fm.active ? fm.voices : 0; }
bool     VBEAI_FM_Active(void)      { return fm.active; }

void     VBEAI_FM_SetVoiceSteal(uint16_t mask) { fm.steal_mask = mask; }
uint16_t VBEAI_FM_GetVoiceSteal(void)          { return fm.steal_mask; }
