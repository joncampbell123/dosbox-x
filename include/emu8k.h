/*
 *  Copyright Notice:
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  EMU8000 wavetable. Register I/O, 32 voices, DRAM, envelopes, and
 *  cubic interpolation follow 86Box snd_emu8k.c.
 *
 *  86Box snd_emu8k.c: Sarah Walker.
 */

#ifndef DOSBOX_EMU8K_H
#define DOSBOX_EMU8K_H

#include "dosbox.h"

#define EMU8K_MIXBUF            (16 * 1024)
#define EMU8K_MEM_ADDRESS_MASK  0xFFFFFF
#define EMU8K_RAM_MEM_START     0x200000
#define EMU8K_FM_MEM_ADDRESS    0xFFFFE0
#define EMU8K_LFOCHORUS_SIZE    0x4000
#define EMU8K_MAX_REFL_SIZE     7744

typedef struct emu8k_mem_internal_t {
    union {
        uint64_t addr;
        struct {
            uint16_t fract_lw_address;
            uint16_t fract_address;
            uint32_t int_address;
        };
    };
} emu8k_mem_internal_t;

typedef struct emu8k_mem_pointers_t {
    union {
        uint32_t addr;
        struct {
            uint16_t lw_address;
            uint8_t  hb_address;
            uint8_t  unused_address;
        };
    };
} emu8k_mem_pointers_t;

typedef struct emu8k_envelope_t {
    int     state;
    int32_t delay_samples;
    int32_t hold_samples;
    int32_t attack_samples;
    int32_t value_amp_hz;
    int32_t value_db_oct;
    int32_t sustain_value_db_oct;
    int32_t attack_amount_amp_hz;
    int32_t ramp_amount_db_oct;
} emu8k_envelope_t;

typedef struct emu8k_chorus_eng_t {
    int32_t              write;
    int32_t              feedback;
    int32_t              delay_samples_central;
    double               lfodepth_multip;
    double               delay_offset_samples_right;
    emu8k_mem_internal_t lfo_inc;
    emu8k_mem_internal_t lfo_pos;
    int32_t              chorus_left_buffer[EMU8K_LFOCHORUS_SIZE];
    int32_t              chorus_right_buffer[EMU8K_LFOCHORUS_SIZE];
} emu8k_chorus_eng_t;

typedef struct emu8k_reverb_combfilter_t {
    int     read_pos;
    int32_t reflection[EMU8K_MAX_REFL_SIZE];
    float   output_gain;
    float   feedback;
    float   damp1;
    float   damp2;
    int     bufsize;
    int32_t filterstore;
} emu8k_reverb_combfilter_t;

typedef struct emu8k_reverb_eng_t {
    int16_t out_mix;
    int16_t link_return_amp;
    int8_t  link_return_type;
    uint8_t refl_in_amp;
    emu8k_reverb_combfilter_t reflections[6];
    emu8k_reverb_combfilter_t allpass[8];
    emu8k_reverb_combfilter_t tailL;
    emu8k_reverb_combfilter_t tailR;
    emu8k_reverb_combfilter_t damper;
} emu8k_reverb_eng_t;

typedef struct emu8k_slide_t {
    int32_t last;
} emu8k_slide_t;

typedef struct emu8k_voice_t {
    union {
        uint32_t cpf;
        struct {
            uint16_t cpf_curr_frac_addr;
            uint16_t cpf_curr_pitch;
        };
    };
    union {
        uint32_t ptrx;
        struct {
            uint8_t  ptrx_pan_aux;
            uint8_t  ptrx_revb_send;
            uint16_t ptrx_pit_target;
        };
    };
    union {
        uint32_t cvcf;
        struct {
            uint16_t cvcf_curr_filt_ctoff;
            uint16_t cvcf_curr_volume;
        };
    };
    emu8k_slide_t volumeslide;
    union {
        uint32_t vtft;
        struct {
            uint16_t vtft_filter_target;
            uint16_t vtft_vol_target;
        };
    };
    uint32_t z2;
    uint32_t z1;
    union {
        uint32_t psst;
        struct {
            uint16_t psst_lw_address;
            uint8_t  psst_hw_address;
            uint8_t  psst_pan;
        };
#define PSST_LOOP_START_MASK 0x00FFFFFF
    };
    union {
        uint32_t csl;
        struct {
            uint16_t csl_lw_address;
            uint8_t  csl_hw_address;
            uint8_t  csl_chor_send;
        };
#define CSL_LOOP_END_MASK 0x00FFFFFF
    };
    union {
        uint32_t ccca;
        struct {
            uint16_t ccca_lw_addr;
            uint8_t  ccca_hb_addr;
            uint8_t  ccca_qcontrol;
        };
    };
#define CCCA_FILTQ_GET(ccca)    ((ccca) >> 28)
#define CCCA_FILTQ_SET(ccca, q) (ccca) = ((ccca) & 0x0FFFFFFF) | ((q) << 28)
#define CCCA_DMA_ACTIVE(ccca)      ((ccca) & 0x04000000)
#define CCCA_DMA_WRITE_MODE(ccca)  ((ccca) & 0x02000000)
#define CCCA_DMA_WRITE_RIGHT(ccca) ((ccca) & 0x01000000)

    uint16_t envvol;
#define ENVVOL_NODELAY(envvol) ((envvol) & 0x8000)
#define ENVVOL_TO_EMU_SAMPLES(envvol) (((envvol) & 0x8000) ? 0 : ((0x8000 - ((envvol) & 0x7FFF)) << 5))

    uint16_t dcysusv;
#define DCYSUSV_IS_RELEASE(dcysusv)          ((dcysusv) & 0x8000)
#define DCYSUSV_GENERATOR_ENGINE_ON(dcysusv) (!((dcysusv) & 0x0080))
#define DCYSUSV_SUSVALUE_GET(dcysusv)        (((dcysusv) >> 8) & 0x7F)
#define DCYSUSV_SUS_TO_ENV_RANGE(susvalue) (((0x7F - (susvalue)) << 21) / 0x7F)
#define DCYSUSV_DECAYRELEASE_GET(dcysusv)  ((dcysusv) & 0x7F)

    uint16_t envval;
#define ENVVAL_NODELAY(envval) ((envval) & 0x8000)
#define ENVVAL_TO_EMU_SAMPLES(envval) (((envval) & 0x8000) ? 0 : ((0x8000 - ((envval) & 0x7FFF)) << 5))

    uint16_t dcysus;
#define DCYSUS_IS_RELEASE(dcysus)         ((dcysus) & 0x8000)
#define DCYSUS_SUSVALUE_GET(dcysus)       (((dcysus) >> 8) & 0x7F)
#define DCYSUS_SUS_TO_ENV_RANGE(susvalue) (((susvalue) << 21) / 0x7F)
#define DCYSUS_DECAYRELEASE_GET(dcysus)   ((dcysus) & 0x7F)

    uint16_t atkhldv;
#define ATKHLDV_TRIGGER(atkhldv)             (!((atkhldv) & 0x8000))
#define ATKHLDV_HOLD(atkhldv)                (((atkhldv) >> 8) & 0x7F)
#define ATKHLDV_HOLD_TO_EMU_SAMPLES(atkhldv) (4096 * (0x7F - (((atkhldv) >> 8) & 0x7F)))
#define ATKHLDV_ATTACK(atkhldv)              ((atkhldv) & 0x7F)

    uint16_t lfo1val, lfo2val;
#define LFOxVAL_NODELAY(lfoxval)        ((lfoxval) & 0x8000)
#define LFOxVAL_TO_EMU_SAMPLES(lfoxval) (((lfoxval) & 0x8000) ? 0 : ((0x8000 - ((lfoxval) & 0x7FFF)) << 5))

    uint16_t atkhld;
#define ATKHLD_TRIGGER(atkhld)             (!((atkhld) & 0x8000))
#define ATKHLD_HOLD(atkhld)                (((atkhld) >> 8) & 0x7F)
#define ATKHLD_HOLD_TO_EMU_SAMPLES(atkhld) (4096 * (0x7F - (((atkhld) >> 8) & 0x7F)))
#define ATKHLD_ATTACK(atkhld)              ((atkhld) & 0x7F)

    uint16_t ip;
#define INTIAL_PITCH_CENTER 0xE000
#define INTIAL_PITCH_OCTAVE 0x1000

    union {
        uint16_t ifatn;
        struct {
            uint8_t ifatn_attenuation;
            uint8_t ifatn_init_filter;
        };
    };
    union {
        uint16_t pefe;
        struct {
            int8_t pefe_modenv_filter_height;
            int8_t pefe_modenv_pitch_height;
        };
    };
    union {
        uint16_t fmmod;
        struct {
            int8_t fmmod_lfo1_filt_mod;
            int8_t fmmod_lfo1_vibrato;
        };
    };
    union {
        uint16_t tremfrq;
        struct {
            uint8_t tremfrq_lfo1_freq;
            int8_t  tremfrq_lfo1_tremolo;
        };
    };
    union {
        uint16_t fm2frq2;
        struct {
            uint8_t fm2frq2_lfo2_freq;
            int8_t  fm2frq2_lfo2_vibrato;
        };
    };

    int env_engine_on;

    emu8k_mem_internal_t addr;
    emu8k_mem_internal_t loop_start;
    emu8k_mem_internal_t loop_end;

    int32_t initial_att;
    int32_t initial_filter;

    emu8k_envelope_t vol_envelope;
    emu8k_envelope_t mod_envelope;

    int64_t              lfo1_speed;
    int64_t              lfo2_speed;
    emu8k_mem_internal_t lfo1_count;
    emu8k_mem_internal_t lfo2_count;
    int32_t              lfo1_delay_samples;
    int32_t              lfo2_delay_samples;
    int                  vol_l;
    int                  vol_r;

    int16_t fixed_modenv_filter_height;
    int16_t fixed_modenv_pitch_height;
    int16_t fixed_lfo1_filt_mod;
    int16_t fixed_lfo1_vibrato;
    int16_t fixed_lfo1_tremolo;
    int16_t fixed_lfo2_vibrato;

    int     filterq_idx;
    int32_t filt_att;
    int64_t filt_buffer[5];
} emu8k_voice_t;

typedef struct emu8k_t {
    emu8k_voice_t voice[32];

    uint16_t hwcf1;
    uint16_t hwcf2;
    uint16_t hwcf3;
    uint32_t hwcf4;
    uint32_t hwcf5;
    uint32_t hwcf6;
    uint32_t hwcf7;

    uint16_t init1[32];
    uint16_t init2[32];
    uint16_t init3[32];
    uint16_t init4[32];

    uint32_t smalr;
    uint32_t smarr;
    uint32_t smalw;
    uint32_t smarw;
    uint16_t smld_buffer;
    uint16_t smrd_buffer;

    uint16_t wc;
    uint16_t id;

    int16_t *ram;
    int16_t *rom;
    int16_t *empty;
    int16_t *ram_pointers[0x100];
    uint32_t ram_end_addr;

    int cur_reg;
    int cur_voice;

    int16_t out_l;
    int16_t out_r;

    emu8k_chorus_eng_t chorus_engine;
    int32_t            chorus_in_buffer[EMU8K_MIXBUF];
    emu8k_reverb_eng_t reverb_engine;
    int32_t            reverb_in_buffer[EMU8K_MIXBUF];

    int     pos;
    int32_t buffer[EMU8K_MIXBUF * 2];

    uint16_t addr;
    uint16_t dmareadbit;
    uint16_t dmawritebit;
} emu8k_t;

class EMU8K {
public:
    EMU8K();
    ~EMU8K();
    bool Init(const char *rom_path, int ram_kb);
    void Close();
    void ChangeAddr(Bitu emu_addr);
    uint16_t Inw(Bitu addr);
    void Outw(Bitu addr, uint16_t val);
    uint8_t Inb(Bitu addr);
    void Outb(Bitu addr, uint8_t val);
    void Generate(int16_t *stereo, Bitu frames);
    Bitu SampleRate() const { return 44100; }

private:
    emu8k_t chip;
};

#endif
