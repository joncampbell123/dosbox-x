/*
 *  Copyright Notice:
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  ISA Windows Sound System card wrapping CS4231.
 *  Config register from 86Box snd_wss.c (Sarah Walker, TheCollector1995).
 */

#include "dosbox.h"
#include "inout.h"
#include "logging.h"
#include "mixer.h"
#include "setup.h"
#include "control.h"
#include "cs4231.h"

static const int wss_dma[4] = { 0, 0, 1, 3 };
static const int wss_irq[8] = { 5, 7, 9, 10, 11, 12, 14, 15 };

class WSS;

static WSS *wss_inst = NULL;

class WSS : public Module_base {
public:
	IO_ReadHandleObject ReadHandler;
	IO_WriteHandleObject WriteHandler;
	MixerObject MixerChan;
	CS4231 codec;
	MixerChannel *chan;
	Bitu base;
	uint8_t config;
	Bitu last_rate;

	WSS(Section *configuration);
	~WSS() {
		if (wss_inst == this)
			wss_inst = NULL;
	}
};

static void wss_write(Bitu port, Bitu val, Bitu iolen) {
	(void)iolen;
	if (!wss_inst) return;
	Bitu off = port - wss_inst->base;
	if (off < 4) {
		wss_inst->config = (uint8_t)val;
		wss_inst->codec.SetPlaybackDMA((uint8_t)wss_dma[val & 3]);
		wss_inst->codec.SetIRQ((uint8_t)wss_irq[(val >> 3) & 7]);
		LOG(LOG_MISC,LOG_NORMAL)("WSS: %03Xh in DMA=%u IRQ=%u", (unsigned)port,
			(unsigned)wss_dma[val & 3], (unsigned)wss_irq[(val >> 3) & 7]);
	} else {
		wss_inst->codec.WritePort((uint8_t)(off & 3), (uint8_t)val);
		Bitu r = wss_inst->codec.SampleRate();
		if (r != wss_inst->last_rate && r != 0 && wss_inst->chan) {
			wss_inst->last_rate = r;
			wss_inst->chan->FillUp();
			wss_inst->chan->SetFreq(r);
		}
	}
}

static Bitu wss_read(Bitu port, Bitu iolen) {
	(void)iolen;
	if (!wss_inst) return 0xff;
	Bitu off = port - wss_inst->base;
	if (off < 4)
		return (Bitu)(0x04 | (wss_inst->config & 0x40));
	return wss_inst->codec.ReadPort((uint8_t)(off & 3));
}

static void WSS_CallBack(Bitu len) {
	if (!wss_inst || !wss_inst->chan) return;
	if (!len) return;
	if (!wss_inst->codec.PlaybackEnabled()) {
		wss_inst->chan->AddSilence();
		return;
	}
	if (len * 4 > MIXER_BUFSIZE)
		len = MIXER_BUFSIZE / 4;
	int16_t *buf = (int16_t *)MixTemp;
	wss_inst->codec.Generate(buf, len);
	wss_inst->chan->AddSamples_s16(len, buf);
}

WSS::WSS(Section *configuration):Module_base(configuration) {
	chan = NULL;
	base = 0;
	config = 0;
	last_rate = 0;

	Section_prop *section = static_cast<Section_prop *>(configuration);
	if (!section->Get_bool("wss") || control->opt_silent || IS_PC98_ARCH)
		return;

	int irq = section->Get_int("irq");
	int dma = section->Get_int("dma");

	base = (Bitu)section->Get_hex("wssbase");
	codec.Reset();
	codec.SetIRQ(irq);
	codec.SetPlaybackDMA(dma);
	codec.SetMixerShim(section->Get_bool("wssmixer"));
	codec.SetAux(0, CS4231_AUX_CD);
	config = 0;
	last_rate = codec.SampleRate();

	wss_inst = this;
	WriteHandler.Install(base, wss_write, IO_MB, 8);
	ReadHandler.Install(base, wss_read, IO_MB, 8);
	chan = MixerChan.Install(&WSS_CallBack, (unsigned int)codec.SampleRate(), "WSS");
	chan->Enable(true);
	codec.SetDacChannel(chan);

	LOG_MSG("WSS: Initialized on port %03Xh", (unsigned int)base);
}

static WSS *test = NULL;

static void WSS_ShutDown(Section *sec) {
	(void)sec;
	if (test != NULL) {
		delete test;
		test = NULL;
	}
}

void WSS_OnReset(Section *sec) {
	(void)sec;
	if (test != NULL) {
		delete test;
		test = NULL;
	}
	test = new WSS(control->GetSection("wss"));
}

void WSS_Init() {
	AddExitFunction(AddExitFunctionFuncPair(WSS_ShutDown), true);
	AddVMEventFunction(VM_EVENT_RESET, AddVMEventFunctionFuncPair(WSS_OnReset));
}
