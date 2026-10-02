/*
 *  Copyright Notice:
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  CS4231 codec. Register I/O, 16-bit LE stereo PCM, volume, and
 *  playback-count IRQ follow 86Box AD1848_TYPE_CS4231 (snd_ad1848.c).
 *
 *  Authors of the 86Box codec: Sarah Walker, TheCollector1995, RichardG.
 */

#ifndef DOSBOX_CS4231_H
#define DOSBOX_CS4231_H

#include "dosbox.h"

class MixerChannel;

enum CS4231AuxDest {
	CS4231_AUX_NONE = 0,
	CS4231_AUX_CD,
	CS4231_AUX_OPL,
	CS4231_AUX_GUS
};

#define CS4231_BUFFERS (1u << 11)
#define CS4231_BUFMASK (CS4231_BUFFERS - 1u)
#define CS4231_MAXDMAREADBYTES (1u << 9)
#define CS4231_PIOBUFFERS (CS4231_BUFFERS / 2u)

#define CS4231_LINPUT 0x00
#define CS4231_RINPUT 0x01
#define CS4231_AUX1L 0x02
#define CS4231_AUX1R 0x03
#define CS4231_AUX2L 0x04
#define CS4231_AUX2R 0x05
#define CS4231_LOUTPUT 0x06
#define CS4231_ROUTPUT 0x07
#define CS4231_PLAYFMT 0x08
#define CS4231_INTERFACE 0x09
#define CS4231_PINCTRL 0x0a
#define CS4231_TESTINIT 0x0b
#define CS4231_MISCINFO 0x0c
#define CS4231_LOOPBACK 0x0d
#define CS4231_PLAYCNTM 0x0e
#define CS4231_PLAYCNTL 0x0f
#define CS4231_FEATURE1 0x10
#define CS4231_FEATURE2 0x11
#define CS4231_LLINEIN 0x12
#define CS4231_RLINEIN 0x13
#define CS4231_TIMERL 0x14
#define CS4231_TIMERH 0x15
#define CS4231_RESERVED1 0x16
#define CS4231_RESERVED2 0x17
#define CS4231_IRQSTAT 0x18
#define CS4231_VERSION 0x19
#define CS4231_MONOCTRL 0x1a
#define CS4231_RESERVED3 0x1b
#define CS4231_RECFMT 0x1c
#define CS4231_PLAYFREQ 0x1d
#define CS4231_RECCNTM 0x1e
#define CS4231_RECCNTL 0x1f

#define CS4231_IA 0x1f
#define CS4231_IA_MODE1 0x0f
#define CS4231_TRD (1 << 5)
#define CS4231_MCE (1 << 6)
#define CS4231_INIT (1 << 7)

#define CS4231_INT (1 << 0)
#define CS4231_PRDY (1 << 1)
#define CS4231_PLR (1 << 2)
#define CS4231_PULR (1 << 3)
#define CS4231_SER (1 << 4)
#define CS4231_CRDY (1 << 5)
#define CS4231_CLR (1 << 6)
#define CS4231_CUL (1 << 7)

#define CS4231_GAIN 0x0f
#define CS4231_MGE (1 << 5)
#define CS4231_SS 0xc0

#define CS4231_ATTEN 0x1f
#define CS4231_MUTE (1 << 7)

#define CS4231_VOL 0x3f

#define CS4231_CSL (1 << 0)
#define CS4231_CFS 0x0e
#define CS4231_STEREO 0x10
#define CS4231_FMT_U8 0x00
#define CS4231_FMT_U8_STEREO 0x10
#define CS4231_FMT_ULAW 0x20
#define CS4231_FMT_ULAW_STEREO 0x30
#define CS4231_FMT_S16LE 0x40
#define CS4231_FMT_S16LE_STEREO 0x50
#define CS4231_FMT_ALAW 0x60
#define CS4231_FMT_ALAW_STEREO 0x70
#define CS4231_FMT_ADPCM 0xa0
#define CS4231_FMT_ADPCM_STEREO 0xb0
#define CS4231_FMT_S16BE 0xc0
#define CS4231_FMT_S16BE_STEREO 0xd0
#define CS4231_FMT 0xe0
#define CS4231_MODE1_FMT 0x70
#define CS4231_MODE2_FMT 0xf0

#define CS4231_PEN (1 << 0)
#define CS4231_CEN (1 << 1)
#define CS4231_SDC (1 << 2)
#define CS4231_CAL0 (1 << 3)
#define CS4231_CAL1 (1 << 4)
#define CS4231_PPIO (1 << 6)
#define CS4231_CPIO (1 << 7)

#define CS4231_IEN (1 << 1)
#define CS4231_DEN (1 << 3)
#define CS4231_XCTL0 (1 << 6)
#define CS4231_XCTL1 (1 << 7)

#define CS4231_ACI (1 << 5)

#define CS4231_ID 0x0f
#define CS4231_C8CME (1 << 5)
#define CS4231_MODE2 (1 << 6)
#define CS4231_RES (1 << 7)

#define CS4231_LBE (1 << 0)
#define CS4231_LOOPATTEN 0xfc

#define CS4231_TE (1 << 6)

#define CS4231_APAR (1 << 3)

#define CS4231_PI (1 << 4)
#define CS4231_CI (1 << 5)
#define CS4231_TI (1 << 6)
#define CS4231_IRQ (CS4231_PI | CS4231_CI | CS4231_TI)

#define CS4231_MIA 0x0f
#define CS4231_MOM (1 << 6)
#define CS4231_MIM (1 << 7)

class CS4231 {
public:
	CS4231();
	void Reset();
	void SetIRQ(uint8_t irq);
	void SetPlaybackDMA(uint8_t ch);
	void SetMixerShim(bool on);
	void SetAux(unsigned slot, CS4231AuxDest dest);
	void SetDacChannel(MixerChannel *ch);
	uint8_t ReadPort(uint8_t offset);
	void WritePort(uint8_t offset, uint8_t val);
	void Generate(int16_t *stereo, Bitu frames);
	Bitu SampleRate() const { return (Bitu)freq; }
	bool PlaybackEnabled() const { return enable; }
	~CS4231();
private:
	bool ReadDmaByte(uint8_t &b);
	unsigned DmaBytesLeft() const;
	void AlignDmaFrame(unsigned need);
	void UpdateFreq();
	void RaiseIRQ();
	void LowerIRQ();
	void StartCalibrate();
	void EndCalibrate();
	void ArmTimer();
	void TimerTick();
	int16_t ProcessMulaw(uint8_t byte);
	int16_t ProcessAlaw(uint8_t byte);
	int16_t ProcessAdpcm(int channel);
	static void LogPlay(int freq, uint8_t i8);
	void UpdateMixerShim(unsigned index);
	static float AuxVol(uint8_t r);

	friend void CS4231_CalibrateEnd(Bitu val);
	friend void CS4231_TimerTick(Bitu val);

	uint8_t index;
	uint8_t trd, mce;
	uint8_t regs[32];
	uint8_t status;
	uint8_t irq, dma;
	uint8_t dma_ff;
	uint32_t dma_data;
	uint8_t fmt_mask;
	uint8_t wave_vol_mask;
	bool enable;
	bool init;
	int aci_count;
	int count;
	int freq;
	int16_t out_l, out_r;
	int adpcm_predictor[2];
	int16_t adpcm_step_index[2];
	uint8_t adpcm_data;
	int adpcm_pos;
	bool mixer_shim;
	CS4231AuxDest aux_dest[2];
	MixerChannel *dac_chan;
	int pic_slot;
	unsigned BytesPerPeriod() const;
};

#endif
