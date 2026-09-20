/*
 *  Copyright Notice:
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  CS4231 codec extracted from 86Box snd_ad1848.c (AD1848_TYPE_CS4231).
 *
 *  Authors of the 86Box codec:
 *      Sarah Walker, <https://pcem-emulator.co.uk/>
 *      TheCollector1995, <mariogplayer@gmail.com>
 *      RichardG, <richardg867@gmail.com>
 *
 *  Copyright 2008-2020 Sarah Walker.
 *  Copyright 2018-2020 TheCollector1995.
 *  Copyright 2021-2025 RichardG.
 */

#include <math.h>
#include <string.h>
#include "dosbox.h"
#include "cs4231.h"
#include "inout.h"
#include "dma.h"
#include "pic.h"
#include "logging.h"
#include "mixer.h"

static int cs4231_vols_7bits[128];
static bool cs4231_vols_ready = false;
static CS4231 *cs4231_slots[4] = { NULL, NULL, NULL, NULL };

static const int8_t adpcm_index_table[16] = {
	-1, -1, -1, -1, 2, 4, 6, 8,
	-1, -1, -1, -1, 2, 4, 6, 8
};
static const int16_t adpcm_step_table[89] = {
	7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
	19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
	50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
	130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
	337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
	876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
	2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
	5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
	15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
};

void CS4231_CalibrateEnd(Bitu val) {
	if (val < 4 && cs4231_slots[val])
		cs4231_slots[val]->EndCalibrate();
}

void CS4231_TimerTick(Bitu val) {
	if (val < 4 && cs4231_slots[val])
		cs4231_slots[val]->TimerTick();
}

static void cs4231_init_vols(void) {
	uint8_t c;
	double attenuation;

	if (cs4231_vols_ready) return;
	for (c = 0; c < 128; c++) {
		attenuation = 0.0;
		if (c & 0x40) {
			if (c < 72)
				attenuation = (c - 72) * -1.5;
		} else {
			if (c & 0x01)
				attenuation -= 1.5;
			if (c & 0x02)
				attenuation -= 3.0;
			if (c & 0x04)
				attenuation -= 6.0;
			if (c & 0x08)
				attenuation -= 12.0;
			if (c & 0x10)
				attenuation -= 24.0;
			if (c & 0x20)
				attenuation -= 48.0;
		}
		attenuation = pow(10, attenuation / 10);
		cs4231_vols_7bits[c] = (int)(attenuation * 65536);
	}
	cs4231_vols_ready = true;
}

CS4231::CS4231() {
	mixer_shim = true;
	aux_dest[0] = aux_dest[1] = CS4231_AUX_NONE;
	dac_chan = NULL;
	pic_slot = -1;
	Reset();
}

void CS4231::SetDacChannel(MixerChannel *ch) {
	dac_chan = ch;
}

void CS4231::SetMixerShim(bool on) {
	mixer_shim = on;
}

void CS4231::SetAux(unsigned slot, CS4231AuxDest dest) {
	if (slot > 1) return;
	aux_dest[slot] = dest;
}

float CS4231::AuxVol(uint8_t r) {
	if (r & CS4231_MUTE)
		return 0.0f;
	double att = 0.0;
	if (r & 0x01) att -= 1.5;
	if (r & 0x02) att -= 3.0;
	if (r & 0x04) att -= 6.0;
	if (r & 0x08) att -= 12.0;
	if (r & 0x10) att -= 24.0;
	return (float)pow(10.0, att / 10.0);
}

void CS4231::UpdateMixerShim(unsigned index) {
	if (!mixer_shim) return;
	if (index == 0xff || index == CS4231_LOUTPUT || index == CS4231_ROUTPUT) {
		if (dac_chan) {
			float l = 0.0f, r = 0.0f;
			if (!(regs[CS4231_LOUTPUT] & CS4231_MUTE))
				l = (float)cs4231_vols_7bits[regs[CS4231_LOUTPUT] & wave_vol_mask] / 65536.0f;
			if (!(regs[CS4231_ROUTPUT] & CS4231_MUTE))
				r = (float)cs4231_vols_7bits[regs[CS4231_ROUTPUT] & wave_vol_mask] / 65536.0f;
			dac_chan->FillUp();
			dac_chan->SetVolume(l, r);
		}
	}
	static const uint8_t regbase[2] = { CS4231_AUX1L, CS4231_AUX2L };
	static const char *chan_name[] = { NULL, "CDAUDIO", "FM", "GUS" };
	static const char *log_name[] = { NULL, "CDA", "OPL", "GUS" };
	unsigned i;
	for (i = 0; i < 2; i++) {
		if (index != 0xff && index != regbase[i] && index != (unsigned)(regbase[i] + 1))
			continue;
		CS4231AuxDest d = aux_dest[i];
		if (d == CS4231_AUX_NONE || d > CS4231_AUX_GUS) continue;
		MixerChannel *mch = MIXER_FindChannel(chan_name[d]);
		if (!mch) continue;
		mch->FillUp();
		mch->SetVolume(AuxVol(regs[regbase[i]]), AuxVol(regs[regbase[i] + 1]));
	}
	if (index > 31) return;
	const char *ch = NULL;
	unsigned volmask = 0x1f;
	if (index == CS4231_LOUTPUT || index == CS4231_ROUTPUT) {
		ch = (dac_chan && dac_chan->name) ? dac_chan->name : "WSS";
		volmask = wave_vol_mask;
	} else if (index == CS4231_AUX1L || index == CS4231_AUX1R) {
		if (aux_dest[0] != CS4231_AUX_NONE && aux_dest[0] <= CS4231_AUX_GUS)
			ch = log_name[aux_dest[0]];
	} else if (index == CS4231_AUX2L || index == CS4231_AUX2R) {
		if (aux_dest[1] != CS4231_AUX_NONE && aux_dest[1] <= CS4231_AUX_GUS)
			ch = log_name[aux_dest[1]];
	}
	if (!ch) return;
	const char *side = (index & 1) ? "RIGHT" : "LEFT";
	if (regs[index] & CS4231_MUTE)
		LOG(LOG_MISC,LOG_NORMAL)("CS4231: I%u in %s %s mute=on", index, ch, side);
	else {
		unsigned v;
		if (index == CS4231_LOUTPUT || index == CS4231_ROUTPUT)
			v = (unsigned)((cs4231_vols_7bits[regs[index] & volmask] * 100 + 32768) / 65536);
		else
			v = (unsigned)(AuxVol(regs[index]) * 100.0f + 0.5f);
		LOG(LOG_MISC,LOG_NORMAL)("CS4231: I%u in %s %s vol=%u", index, ch, side, v);
	}
}

CS4231::~CS4231() {
	if (pic_slot >= 0) {
		PIC_RemoveSpecificEvents(CS4231_CalibrateEnd, (Bitu)pic_slot);
		PIC_RemoveSpecificEvents(CS4231_TimerTick, (Bitu)pic_slot);
		cs4231_slots[pic_slot] = NULL;
		pic_slot = -1;
	}
}

void CS4231::SetIRQ(uint8_t newirq) {
	irq = newirq;
}

void CS4231::SetPlaybackDMA(uint8_t ch) {
	dma = ch;
}

void CS4231::UpdateFreq() {
	double f = (regs[CS4231_PLAYFMT] & CS4231_CSL) ? 16934400.0 : 24576000.0;
	switch ((regs[CS4231_PLAYFMT] & CS4231_CFS) >> 1) {
		case 0: // CFS 000: CSL=0 → 8 kHz, CSL=1 → 5.5125 kHz
			f /= 3072.0; break;
		case 1: // CFS 001: CSL=0 → 16 kHz, CSL=1 → 11.025 kHz
			f /= 1536.0; break;
		case 2: // CFS 010: CSL=0 → 27.428 kHz, CSL=1 → 18.9 kHz
			f /= 896.0; break;
		case 3: // CFS 011: CSL=0 → 32 kHz, CSL=1 → 22.05 kHz
			f /= 768.0; break;
		case 4: // CFS 100: CSL=0 → 54.857 kHz, CSL=1 → 37.8 kHz
			f /= 448.0; break;
		case 5: // CFS 101: CSL=0 → 64 kHz, CSL=1 → 44.1 kHz
			f /= 384.0; break;
		case 6: // CFS 110: CSL=0 → 48 kHz, CSL=1 → 33.075 kHz
			f /= 512.0; break;
		case 7: // CFS 111: CSL=0 → 9.6 kHz, CSL=1 → 6.615 kHz
			f /= 2560.0; break;
		default: break;
	}
	freq = (int)f;
	if (freq < 1) freq = 1;
}

void CS4231::Reset() {
	cs4231_init_vols();
	memset(regs, 0, sizeof(regs));
	status = CS4231_CUL | CS4231_CLR | CS4231_PULR | CS4231_PLR;
	index = 0;
	trd = 0;
	mce = CS4231_MCE;
	enable = false;
	count = 0;
	out_l = 0;
	out_r = 0;
	irq = 7;
	dma = 3;
	dma_ff = 0;
	dma_data = 0;
	regs[CS4231_AUX1L] = regs[CS4231_AUX1R] = CS4231_MUTE;
	regs[CS4231_AUX2L] = regs[CS4231_AUX2R] = CS4231_MUTE;
	regs[CS4231_LOUTPUT] = regs[CS4231_ROUTPUT] = CS4231_MUTE;
	regs[CS4231_INTERFACE] = CS4231_CAL0;
	regs[CS4231_MISCINFO] = CS4231_RES | 0x0a;
	regs[CS4231_LLINEIN] = regs[CS4231_RLINEIN] = CS4231_MUTE | 0x08;
	regs[CS4231_RESERVED1] = CS4231_MUTE;
	regs[CS4231_VERSION] = 0x80;
	regs[CS4231_MONOCTRL] = CS4231_MIM;
	regs[CS4231_PLAYFREQ] = CS4231_MUTE;
	fmt_mask = CS4231_MODE1_FMT;
	wave_vol_mask = CS4231_VOL;
	init = false;
	aci_count = 0;
	adpcm_predictor[0] = adpcm_predictor[1] = 0;
	adpcm_step_index[0] = adpcm_step_index[1] = 0;
	adpcm_data = 0;
	adpcm_pos = 0;
	if (pic_slot < 0) {
		unsigned i;
		for (i = 0; i < 4; i++) {
			if (!cs4231_slots[i]) {
				cs4231_slots[i] = this;
				pic_slot = (int)i;
				break;
			}
		}
	}
	if (pic_slot >= 0) {
		PIC_RemoveSpecificEvents(CS4231_CalibrateEnd, (Bitu)pic_slot);
		PIC_RemoveSpecificEvents(CS4231_TimerTick, (Bitu)pic_slot);
	}
	UpdateFreq();
}

void CS4231::StartCalibrate() {
	init = true;
	aci_count = 16;
	regs[CS4231_TESTINIT] |= CS4231_ACI;
	if (pic_slot >= 0) {
		PIC_RemoveSpecificEvents(CS4231_CalibrateEnd, (Bitu)pic_slot);
		PIC_AddEvent(CS4231_CalibrateEnd, 0.0, (Bitu)pic_slot);
	}
}

void CS4231::EndCalibrate() {
	init = false;
}

void CS4231::ArmTimer() {
	unsigned t = ((unsigned)regs[CS4231_TIMERH] << 8) | regs[CS4231_TIMERL];
	if (pic_slot >= 0)
		PIC_RemoveSpecificEvents(CS4231_TimerTick, (Bitu)pic_slot);
	if (t == 0) return;
	if (pic_slot < 0) return;
	double usec = (double)t * ((regs[CS4231_PLAYFMT] & CS4231_CSL) ? 9.92 : 9.969);
	PIC_AddEvent(CS4231_TimerTick, (pic_tickindex_t)(usec / 1000.0), (Bitu)pic_slot);
}

void CS4231::TimerTick() {
	regs[CS4231_IRQSTAT] |= CS4231_TI;
	status |= CS4231_INT;
	if (regs[CS4231_PINCTRL] & CS4231_IEN)
		PIC_ActivateIRQ(irq);
	else
		PIC_DeActivateIRQ(irq);
	if (regs[CS4231_FEATURE1] & CS4231_TE)
		ArmTimer();
}

void CS4231::LogPlay(int freq, uint8_t i8) {
	const char *ch = (i8 & CS4231_STEREO) ? "stereo" : "mono";
	const char *bits = "8bit";
	const char *enc = "PCM";
	switch (i8 & CS4231_FMT) {
		case CS4231_FMT_ULAW:
			enc = "uLaw";
			break;
		case CS4231_FMT_S16LE:
			bits = "16bit";
			break;
		case CS4231_FMT_ALAW:
			enc = "A-law";
			break;
		case CS4231_FMT_ADPCM:
			bits = "16bit";
			enc = "ADPCM";
			break;
		case CS4231_FMT_S16BE:
			bits = "16bit BE";
			break;
		default:
			break;
	}
	if (freq % 1000 == 0)
		LOG(LOG_MISC,LOG_NORMAL)("CS4231: playing %d kHz %s %s %s", freq / 1000, bits, ch, enc);
	else
		LOG(LOG_MISC,LOG_NORMAL)("CS4231: playing %g kHz %s %s %s", freq / 1000.0, bits, ch, enc);
}

unsigned CS4231::BytesPerPeriod() const {
	switch (regs[CS4231_PLAYFMT] & fmt_mask) {
		case CS4231_FMT_U8_STEREO:
		case CS4231_FMT_ULAW_STEREO:
		case CS4231_FMT_S16LE:
		case CS4231_FMT_ALAW_STEREO:
		case CS4231_FMT_S16BE:
			return 2;
		case CS4231_FMT_S16LE_STEREO:
		case CS4231_FMT_S16BE_STEREO:
			return 4;
		default:
			return 1;
	}
}

int16_t CS4231::ProcessMulaw(uint8_t byte) {
	byte = (uint8_t)~byte;
	int temp = (((byte & 0x0f) << 3) + 0x84);
	temp <<= ((byte & 0x70) >> 4);
	temp = (byte & 0x80) ? (0x84 - temp) : (temp - 0x84);
	if (temp > 32767)
		return 32767;
	else if (temp < -32768)
		return -32768;
	return (int16_t)temp;
}

int16_t CS4231::ProcessAlaw(uint8_t byte) {
	byte ^= 0x55;
	int dec = ((byte & 0x0f) << 4);
	const int seg = (int)((byte & 0x70) >> 4);
	switch (seg) {
		default:
			dec |= 0x108;
			dec <<= seg - 1;
			break;
		case 0:
			dec |= 0x8;
			break;
		case 1:
			dec |= 0x108;
			break;
	}
	return (int16_t)((byte & 0x80) ? dec : -dec);
}

int16_t CS4231::ProcessAdpcm(int channel) {
	int temp;
	if (adpcm_pos & 1) {
		temp = adpcm_data >> 4;
	} else {
		uint8_t b;
		if (!ReadDmaByte(b))
			return (int16_t)adpcm_predictor[channel];
		adpcm_data = b;
		temp = b & 0x0f;
	}
	adpcm_pos++;

	int step = adpcm_step_table[adpcm_step_index[channel]];
	int step_index = adpcm_step_index[channel] + adpcm_index_table[temp];
	if (step_index < 0)
		step_index = 0;
	else if (step_index > 88)
		step_index = 88;

	int diff = ((2 * (temp & 7) + 1) * step) >> 3;
	int predictor = adpcm_predictor[channel] + ((temp & 8) ? -diff : diff);
	if (predictor < -32768)
		predictor = -32768;
	else if (predictor > 32767)
		predictor = 32767;
	adpcm_predictor[channel] = predictor;
	adpcm_step_index[channel] = (int16_t)step_index;

	return (int16_t)predictor;
}

void CS4231::RaiseIRQ() {
	if (!(status & CS4231_INT)) {
		status |= CS4231_INT;
		regs[CS4231_IRQSTAT] |= CS4231_PI;
	}
	if (regs[CS4231_PINCTRL] & CS4231_IEN)
		PIC_ActivateIRQ(irq);
	else
		PIC_DeActivateIRQ(irq);
}

void CS4231::LowerIRQ() {
	PIC_DeActivateIRQ(irq);
}

unsigned CS4231::DmaBytesLeft() const {
	DmaChannel *ch = GetDMAChannel(dma);
	if (!ch || ch->masked) return 0;
	unsigned n = (unsigned)ch->currcnt + 1u;
	if (dma >= 4) n *= 2;
	if (dma_ff) n += 1;
	return n;
}

void CS4231::AlignDmaFrame(unsigned need) {
	if (need <= 1) return;
	unsigned left = DmaBytesLeft();
	while (left > 0 && left < need) {
		uint8_t d;
		if (!ReadDmaByte(d)) break;
		left = DmaBytesLeft();
	}
}

bool CS4231::ReadDmaByte(uint8_t &b) {
	DmaChannel *ch = GetDMAChannel(dma);
	if (!ch) return false;
	if (dma >= 4) {
		if (dma_ff) {
			b = (uint8_t)((dma_data >> 8) & 0xff);
		} else {
			uint8_t word[2];
			if (ch->Read(1, word) == 0) return false;
			dma_data = (uint32_t)word[0] | ((uint32_t)word[1] << 8);
			b = (uint8_t)(dma_data & 0xff);
		}
		dma_ff = (uint8_t)(dma_ff ^ 1);
	} else {
		if (ch->Read(1, &b) == 0) return false;
	}
	return true;
}

uint8_t CS4231::ReadPort(uint8_t offset) {
	uint8_t ret = 0xff;
	switch (offset & 3) {
		case 0: // index port: which indirect register the data port uses; bit 7 set means still calibrating
			if (init)
				ret = CS4231_INIT;
			else
				ret = (uint8_t)(index | trd | mce);
			break;
		case 1: // data port: the indirect register currently selected by the index port
			if (index == CS4231_TESTINIT) {
				ret = (uint8_t)(regs[CS4231_TESTINIT] & ~CS4231_ACI);
				if (aci_count > 0) {
					aci_count--;
					ret |= CS4231_ACI;
				}
				regs[CS4231_TESTINIT] = ret;
			} else {
				ret = regs[index];
			}
			break;
		case 2: // status port: bit 0 is the pending interrupt
			ret = status;
			break;
		case 3: // programmed-I/O sample port; PIO is not implemented, so this reads as 0x80
			ret = 0x80;
			break;
	}
	return ret;
}

void CS4231::WritePort(uint8_t offset, uint8_t val) {
	if (init && ((offset & 3) < 2))
		return;
	switch (offset & 3) {
		case 0: { // index port: pick the indirect register; clearing bit 6 starts autocalibrate
			uint8_t new_mce = val & CS4231_MCE;
			if (mce && !new_mce && (regs[CS4231_INTERFACE] & (CS4231_CAL0 | CS4231_CAL1)))
				StartCalibrate();
			if (regs[CS4231_MISCINFO] & CS4231_MODE2)
				index = val & CS4231_IA;
			else
				index = val & CS4231_IA_MODE1;
			trd = val & CS4231_TRD;
			mce = new_mce;
			break;
		}
		case 1: // data port: write the indirect register currently selected by the index port
			switch (index) {
				case CS4231_PLAYFMT:
					regs[CS4231_PLAYFMT] = val;
					UpdateFreq();
					LogPlay(freq, (uint8_t)(val & fmt_mask));
					break;
				case CS4231_INTERFACE:
					if (!enable && (val & (CS4231_PEN | CS4231_PPIO)) == CS4231_PEN) {
						adpcm_pos = 0;
						adpcm_predictor[0] = adpcm_predictor[1] = 0;
						adpcm_step_index[0] = adpcm_step_index[1] = 0;
						dma_ff = 0;
					}
					enable = ((val & (CS4231_PEN | CS4231_PPIO)) == CS4231_PEN);
					if (!enable) {
						out_l = 0;
						out_r = 0;
					}
					regs[CS4231_INTERFACE] = val;
					break;
				case CS4231_PINCTRL:
					regs[CS4231_PINCTRL] = val;
					if ((status & CS4231_INT) && (val & CS4231_IEN))
						PIC_ActivateIRQ(irq);
					else
						PIC_DeActivateIRQ(irq);
					break;
				case CS4231_TESTINIT:
				case CS4231_RESERVED2:
				case CS4231_VERSION:
					break;
				case CS4231_MISCINFO:
					regs[CS4231_MISCINFO] = (uint8_t)(CS4231_RES | (val & (CS4231_MODE2 | CS4231_C8CME)) | (regs[CS4231_MISCINFO] & CS4231_ID));
					if (val & CS4231_MODE2)
						fmt_mask = CS4231_MODE2_FMT;
					else
						fmt_mask = CS4231_MODE1_FMT;
					break;
				case CS4231_PLAYCNTM:
					regs[CS4231_PLAYCNTM] = val;
					count = regs[CS4231_PLAYCNTL] | (val << 8);
					break;
				case CS4231_FEATURE1:
					regs[CS4231_FEATURE1] = val;
					if (val & CS4231_TE)
						ArmTimer();
					else if (pic_slot >= 0)
						PIC_RemoveSpecificEvents(CS4231_TimerTick, (Bitu)pic_slot);
					break;
				case CS4231_FEATURE2:
					if (val & CS4231_APAR)
						adpcm_predictor[0] = adpcm_predictor[1] = 0;
					regs[CS4231_FEATURE2] = val;
					break;
				case CS4231_TIMERL:
					regs[CS4231_TIMERL] = val;
					if (regs[CS4231_FEATURE1] & CS4231_TE)
						ArmTimer();
					break;
				case CS4231_TIMERH:
					regs[CS4231_TIMERH] = val;
					if (regs[CS4231_FEATURE1] & CS4231_TE)
						ArmTimer();
					break;
				case CS4231_IRQSTAT:
					val = (uint8_t)(regs[CS4231_IRQSTAT] & ((val & CS4231_IRQ) | 0x0f));
					regs[CS4231_IRQSTAT] = val;
					if (!(val & CS4231_IRQ)) {
						status &= ~CS4231_INT;
						LowerIRQ();
					}
					break;
				default: // leftover mixer/volume/count registers stored as-is
					regs[index] = val;
					break;
			}
			if (index == CS4231_AUX1L || index == CS4231_AUX1R ||
			    index == CS4231_AUX2L || index == CS4231_AUX2R ||
			    index == CS4231_LOUTPUT || index == CS4231_ROUTPUT)
				UpdateMixerShim(index);
			break;
		case 2: // writing status acknowledges the interrupt
			status &= ~CS4231_INT;
			regs[CS4231_IRQSTAT] &= 0x0f;
			LowerIRQ();
			break;
		default: // programmed-I/O sample port; not implemented
			break;
	}
}

void CS4231::Generate(int16_t *stereo, Bitu frames) {
	Bitu i;
	for (i = 0; i < frames; i++) {
		if (!enable) {
			stereo[i * 2] = 0;
			stereo[i * 2 + 1] = 0;
			continue;
		}
		AlignDmaFrame(BytesPerPeriod());
		int pos0 = adpcm_pos;
		bool dma_tick = false;

		switch (regs[CS4231_PLAYFMT] & fmt_mask) {
			case CS4231_FMT_U8: {
				uint8_t b;
				if (ReadDmaByte(b)) {
					out_l = out_r = (int16_t)(((int)b - 0x80) << 8);
					dma_tick = true;
				}
				break;
			}
			case CS4231_FMT_U8_STEREO: {
				uint8_t bl, br;
				if (ReadDmaByte(bl) && ReadDmaByte(br)) {
					out_l = (int16_t)(((int)bl - 0x80) << 8);
					out_r = (int16_t)(((int)br - 0x80) << 8);
					dma_tick = true;
				}
				break;
			}
			case CS4231_FMT_S16LE: {
				uint8_t lo, hi;
				if (ReadDmaByte(lo) && ReadDmaByte(hi)) {
					out_l = out_r = (int16_t)((hi << 8) | lo);
					dma_tick = true;
				}
				break;
			}
			case CS4231_FMT_S16LE_STEREO: {
				uint8_t b0, b1, b2, b3;
				if (ReadDmaByte(b0) && ReadDmaByte(b1) && ReadDmaByte(b2) && ReadDmaByte(b3)) {
					out_l = (int16_t)((b1 << 8) | b0);
					out_r = (int16_t)((b3 << 8) | b2);
					dma_tick = true;
				}
				break;
			}
			case CS4231_FMT_ULAW: {
				uint8_t b;
				if (ReadDmaByte(b)) {
					out_l = out_r = ProcessMulaw(b);
					dma_tick = true;
				}
				break;
			}
			case CS4231_FMT_ULAW_STEREO: {
				uint8_t bl, br;
				if (ReadDmaByte(bl) && ReadDmaByte(br)) {
					out_l = ProcessMulaw(bl);
					out_r = ProcessMulaw(br);
					dma_tick = true;
				}
				break;
			}
			case CS4231_FMT_ALAW: {
				uint8_t b;
				if (ReadDmaByte(b)) {
					out_l = out_r = ProcessAlaw(b);
					dma_tick = true;
				}
				break;
			}
			case CS4231_FMT_ALAW_STEREO: {
				uint8_t bl, br;
				if (ReadDmaByte(bl) && ReadDmaByte(br)) {
					out_l = ProcessAlaw(bl);
					out_r = ProcessAlaw(br);
					dma_tick = true;
				}
				break;
			}
			case CS4231_FMT_ADPCM:
				out_l = out_r = ProcessAdpcm(0);
				break;
			case CS4231_FMT_ADPCM_STEREO:
				out_l = ProcessAdpcm(0);
				out_r = ProcessAdpcm(1);
				break;
			case CS4231_FMT_S16BE: {
				uint8_t hi, lo;
				if (ReadDmaByte(hi) && ReadDmaByte(lo)) {
					out_l = out_r = (int16_t)((hi << 8) | lo);
					dma_tick = true;
				}
				break;
			}
			case CS4231_FMT_S16BE_STEREO: {
				uint8_t h0, l0, h1, l1;
				if (ReadDmaByte(h0) && ReadDmaByte(l0) && ReadDmaByte(h1) && ReadDmaByte(l1)) {
					out_l = (int16_t)((h0 << 8) | l0);
					out_r = (int16_t)((h1 << 8) | l1);
					dma_tick = true;
				}
				break;
			}
			default:
				break;
		}

		if (count < 0) {
			count = regs[CS4231_PLAYCNTL] | (regs[CS4231_PLAYCNTM] << 8);
			RaiseIRQ();
		}
		if ((regs[CS4231_PLAYFMT] & CS4231_FMT) == CS4231_FMT_ADPCM) {
			if (adpcm_pos != pos0 && !(adpcm_pos & 7))
				count--;
		} else if (dma_tick) {
			count--;
		}

		if (mce) {
			stereo[i * 2] = 0;
			stereo[i * 2 + 1] = 0;
		} else {
			stereo[i * 2] = out_l;
			stereo[i * 2 + 1] = out_r;
		}
	}
}
