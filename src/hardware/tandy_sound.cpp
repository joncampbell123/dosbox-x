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

/* 
	Based of sn76496.c of the M.A.M.E. project
*/

#include "dosbox.h"
#include "inout.h"
#include "mixer.h"
#include "mem.h"
#include "setup.h"
#include "pic.h"
#include "dma.h"
#include "hardware.h"
#include "control.h"
#include "sn76496.h"
#include <cstring>
#include <math.h>

#define MAX_OUTPUT 0x7fff
#define STEP 0x10000

/* Formulas for noise generator */
/* bit0 = output */

/* noise feedback for white noise mode (verified on real SN76489 by John Kortink) */
#define FB_WNOISE 0x14002	/* (16bits) bit16 = bit0(out) ^ bit2 ^ bit15 */

/* noise feedback for periodic noise mode */
//#define FB_PNOISE 0x10000 /* 16bit rorate */
#define FB_PNOISE 0x08000   /* JH 981127 - fixes Do Run Run */

/*
0x08000 is definitely wrong. The Master System conversion of Marble Madness
uses periodic noise as a baseline. With a 15-bit rotate, the bassline is
out of tune.
The 16-bit rotate has been confirmed against a real PAL Sega Master System 2.
Hope that helps the System E stuff, more news on the PSG as and when!
*/

/* noise generator start preset (for periodic noise) */
#define NG_PRESET 0x0f35

static struct SN76496 sn;

#define TDAC_DMA_BUFSIZE 1024

static struct {
	MixerChannel * chan;
	bool enabled;
	Bitu last_write;
	struct {
		MixerChannel * chan;
		bool enabled;
		struct {
			Bitu base;
			Bit8u irq,dma;
		} hw;
		struct {
			Bitu rate;
			Bit8u buf[TDAC_DMA_BUFSIZE];
			Bit8u last_sample;
			DmaChannel * chan;
			bool transfer_done;
		} dma;
		Bit8u mode,control;
		Bit16u frequency;
		Bit8u amplitude;
		bool irq_activated;
	} dac;
} tandy;

void SN76496Write(struct SN76496 *R,Bitu port,Bitu data) {
	/* update the output buffer before changing the registers */

	if (data & 0x80)
	{
		int r = (data & 0x70) >> 4;
		int c = r/2;

		R->LastRegister = r;
		R->Register[r] = (R->Register[r] & 0x3f0) | (data & 0x0f);
		switch (r)
		{
			case 0:	/* tone 0 : frequency */
			case 2:	/* tone 1 : frequency */
			case 4:	/* tone 2 : frequency */
				R->Period[c] = R->UpdateStep * R->Register[r];
				if (R->Period[c] == 0) R->Period[c] = 0x3fe;
				if (r == 4)
				{
					/* update noise shift frequency */
					if ((R->Register[6] & 0x03) == 0x03)
						R->Period[3] = 2 * R->Period[2];
				}
				break;
			case 1:	/* tone 0 : volume */
			case 3:	/* tone 1 : volume */
			case 5:	/* tone 2 : volume */
			case 7:	/* noise  : volume */
				R->Volume[c] = R->VolTable[data & 0x0f];
				break;
			case 6:	/* noise  : frequency, mode */
				{
					int n = R->Register[6];
					R->NoiseFB = (n & 4) ? FB_WNOISE : FB_PNOISE;
					n &= 3;
					/* N/512,N/1024,N/2048,Tone #3 output */
					R->Period[3] = (n == 3) ? 2 * R->Period[2] : (R->UpdateStep << (5+n));

					/* reset noise shifter */
//					R->RNG = NG_PRESET;
//					R->Output[3] = R->RNG & 1;
				}
				break;
		}
	}
	else
	{
		int r = R->LastRegister;
		int c = r/2;

		switch (r)
		{
			case 0:	/* tone 0 : frequency */
			case 2:	/* tone 1 : frequency */
			case 4:	/* tone 2 : frequency */
				R->Register[r] = (R->Register[r] & 0x0f) | ((data & 0x3f) << 4);
				R->Period[c] = R->UpdateStep * R->Register[r];
				if (R->Period[c] == 0) R->Period[c] = 0x3fe;
				if (r == 4)
				{
					/* update noise shift frequency */
					if ((R->Register[6] & 0x03) == 0x03)
						R->Period[3] = 2 * R->Period[2];
				}
				break;
		}
	}
}

void SN76496Update(struct SN76496 *R, Bit16s *buffer, Bitu length) {
	int i;

	/* If the volume is 0, increase the counter */
	for (i = 0;i < 4;i++)
	{
		if (R->Volume[i] == 0)
		{
			/* note that I do count += length, NOT count = length + 1. You might think */
			/* it's the same since the volume is 0, but doing the latter could cause */
			/* interferencies when the program is rapidly modulating the volume. */
			if (R->Count[i] <= (int)length*STEP) R->Count[i] += length*STEP;
		}
	}

	Bitu count=length;
	while (count)
	{
		int vol[4];
		unsigned int out;
		int left;


		/* vol[] keeps track of how long each square wave stays */
		/* in the 1 position during the sample period. */
		vol[0] = vol[1] = vol[2] = vol[3] = 0;

		for (i = 0;i < 3;i++)
		{
			if (R->Output[i]) vol[i] += R->Count[i];
			R->Count[i] -= STEP;
			/* Period[i] is the half period of the square wave. Here, in each */
			/* loop I add Period[i] twice, so that at the end of the loop the */
			/* square wave is in the same status (0 or 1) it was at the start. */
			/* vol[i] is also incremented by Period[i], since the wave has been 1 */
			/* exactly half of the time, regardless of the initial position. */
			/* If we exit the loop in the middle, Output[i] has to be inverted */
			/* and vol[i] incremented only if the exit status of the square */
			/* wave is 1. */
			while (R->Count[i] <= 0)
			{
				R->Count[i] += R->Period[i];
				if (R->Count[i] > 0)
				{
					R->Output[i] ^= 1;
					if (R->Output[i]) vol[i] += R->Period[i];
					break;
				}
				R->Count[i] += R->Period[i];
				vol[i] += R->Period[i];
			}
			if (R->Output[i]) vol[i] -= R->Count[i];
		}

		left = STEP;
		do
		{
			int nextevent;


			if (R->Count[3] < left) nextevent = R->Count[3];
			else nextevent = left;

			if (R->Output[3]) vol[3] += R->Count[3];
			R->Count[3] -= nextevent;
			if (R->Count[3] <= 0)
			{
				if (R->RNG & 1) R->RNG ^= R->NoiseFB;
				R->RNG >>= 1;
				R->Output[3] = R->RNG & 1;
				R->Count[3] += R->Period[3];
				if (R->Output[3]) vol[3] += R->Period[3];
			}
			if (R->Output[3]) vol[3] -= R->Count[3];

			left -= nextevent;
		} while (left > 0);

		out = vol[0] * R->Volume[0] + vol[1] * R->Volume[1] +
				vol[2] * R->Volume[2] + vol[3] * R->Volume[3];

		if (out > MAX_OUTPUT * STEP) out = MAX_OUTPUT * STEP;

		*(buffer++) = (Bit16s)(out / STEP);

		count--;
	}
}

static void TandySN76496Write(Bitu port,Bitu data,Bitu iolen) {
	struct SN76496 *R = &sn;
 
	tandy.last_write=PIC_Ticks;
	if (!tandy.enabled) {
		tandy.chan->Enable(true);
		tandy.enabled=true;
	}

	SN76496Write(R,port,data);
}

static void TandySN76496Update(Bitu length) {
	struct SN76496 *R = &sn;

	if ((tandy.last_write+5000)<PIC_Ticks) {
		tandy.enabled=false;
		tandy.chan->Enable(false);
	}
 
	Bit16s * buffer=(Bit16s *)MixTemp;
	SN76496Update(R,buffer,length);
	tandy.chan->AddSamples_m16(length,(Bit16s *)MixTemp);
}

static void TandyDACWrite(Bitu port,Bitu data,Bitu iolen) {
	LOG_MSG("Write tandy dac %X val %X",(int)port,(int)data);
}

static void SN76496_set_clock(struct SN76496 *R, int clock) {
	/* the base clock for the tone generators is the chip clock divided by 16; */
	/* for the noise generator, it is clock / 256. */
	/* Here we calculate the number of steps which happen during one sample */
	/* at the given sample rate. No. of events = sample rate / (clock/16). */
	/* STEP is a multiplier used to turn the fraction into a fixed point */
	/* number. */
	R->UpdateStep = (unsigned int)(((double)STEP * R->SampleRate * 16) / clock);
}


static void SN76496_set_gain(struct SN76496 *R, int gain) {
	int i;
	double out;

	gain &= 0xff;

	/* increase max output basing on gain (0.2 dB per step) */
	out = MAX_OUTPUT / 3;
	while (gain-- > 0)
		out *= 1.023292992;	/* = (10 ^ (0.2/20)) */

	/* build volume table (2dB per step) */
	for (i = 0;i < 15;i++)
	{
		/* limit volume to avoid clipping */
		if (out > MAX_OUTPUT / 3) R->VolTable[i] = MAX_OUTPUT / 3;
		else R->VolTable[i] = (int)out;

		out /= 1.258925412;	/* = 10 ^ (2/20) = 2dB */
	}
	R->VolTable[15] = 0;
}

void SN76496Reset(struct SN76496 *R, Bitu Clock, Bitu sample_rate) {
	Bitu i;
	R->SampleRate = sample_rate;
	SN76496_set_clock(R,Clock);
	for (i = 0;i < 4;i++) R->Volume[i] = 0;
	R->LastRegister = 0;
	for (i = 0;i < 8;i+=2)
	{
		R->Register[i] = 0;
		R->Register[i + 1] = 0x0f;	/* volume = 0 */
	}
	
	for (i = 0;i < 4;i++)
	{
		R->Output[i] = 0;
		R->Period[i] = R->Count[i] = R->UpdateStep;
	}
	R->RNG = NG_PRESET;
	R->Output[3] = R->RNG & 1;
	SN76496_set_gain(R,0x1);
}

bool TS_Get_Address(Bitu& tsaddr, Bitu& tsirq, Bitu& tsdma) {
	tsaddr=0;
	tsirq =0;
	tsdma =0;
	if (tandy.dac.enabled) {
		tsaddr=tandy.dac.hw.base;
		tsirq =tandy.dac.hw.irq;
		tsdma =tandy.dac.hw.dma;
		return true;
	}
	return false;
}


static void TandyDAC_DMA_CallBack(DmaChannel * /*chan*/, DMAEvent event) {
	if (event == DMA_REACHED_TC) {
		tandy.dac.dma.transfer_done=true;
		PIC_ActivateIRQ(tandy.dac.hw.irq);
	}
}

/* NTS: Formerly defined static, but nobody uses the function. But, this
 * function contains too much technical documentation within that is worth
 * keeping. --J.C. */
void TandyDACModeChanged(void) {
	switch (tandy.dac.mode&3) {
	case 0:
		// joystick mode
		break;
	case 1:
		break;
	case 2:
		// recording
		break;
	case 3:
		// playback
		tandy.dac.chan->FillUp();
		if (tandy.dac.frequency!=0) {
			float freq=3579545.0f/((float)tandy.dac.frequency);
			tandy.dac.chan->SetFreq((Bitu)freq);
			float vol=((float)tandy.dac.amplitude)/7.0f;
			tandy.dac.chan->SetVolume(vol,vol);
			if ((tandy.dac.mode&0x0c)==0x0c) {
				tandy.dac.dma.transfer_done=false;
				tandy.dac.dma.chan=GetDMAChannel(tandy.dac.hw.dma);
				if (tandy.dac.dma.chan) {
					tandy.dac.dma.chan->Register_Callback(TandyDAC_DMA_CallBack);
					tandy.dac.chan->Enable(true);
//					LOG_MSG("Tandy DAC: playback started with freqency %f, volume %f",freq,vol);
				}
			}
		}
		break;
	}
}

static Bitu TandyDACRead(Bitu port,Bitu /*iolen*/) {
	switch (port) {
	case 0xc4:
		return (tandy.dac.mode&0x77) | (tandy.dac.irq_activated ? 0x08 : 0x00);
	case 0xc6:
		return (Bit8u)(tandy.dac.frequency&0xff);
	case 0xc7:
		return (Bit8u)(((tandy.dac.frequency>>8)&0xf) | (tandy.dac.amplitude<<5));
	}
	LOG_MSG("Tandy DAC: Read from unknown %X",(int)port);
	return 0xff;
}

static void TandyDACGenerateDMASound(Bitu length) {
	if (length) {
		Bitu read=tandy.dac.dma.chan->Read(length,tandy.dac.dma.buf);
		tandy.dac.chan->AddSamples_m8(read,tandy.dac.dma.buf);
		if (read < length) {
			if (read>0) tandy.dac.dma.last_sample=tandy.dac.dma.buf[read-1];
			for (Bitu ct=read; ct < length; ct++) {
				tandy.dac.chan->AddSamples_m8(1,&tandy.dac.dma.last_sample);
			}
		}
	}
}

static void TandyDACUpdate(Bitu length) {
	if (tandy.dac.enabled && ((tandy.dac.mode&0x0c)==0x0c)) {
		if (!tandy.dac.dma.transfer_done) {
			Bitu len = length;
			TandyDACGenerateDMASound(len);
		} else {
			for (Bitu ct=0; ct < length; ct++) {
				tandy.dac.chan->AddSamples_m8(1,&tandy.dac.dma.last_sample);
			}
		}
	} else {
		tandy.dac.chan->AddSilence();
	}
}


class TANDYSOUND: public Module_base {
private:
	IO_WriteHandleObject WriteHandler[4];
	IO_ReadHandleObject ReadHandler[4];
	MixerObject MixerChan;
	MixerObject MixerChanDAC;
public:
	TANDYSOUND(Section* configuration):Module_base(configuration){
		Section_prop * section=static_cast<Section_prop *>(configuration);

		bool enable_hw_tandy_dac=true;
		Bitu sbport, sbirq, sbdma;
		if (SB_Get_Address(sbport, sbirq, sbdma)) {
			enable_hw_tandy_dac=false;
		}

		real_writeb(0x40,0xd4,0x00);
		if (IS_TANDY_ARCH) {
			/* enable tandy sound if tandy=true/auto */
			if ((strcmp(section->Get_string("tandy"),"true")!=0) &&
				(strcmp(section->Get_string("tandy"),"on")!=0) &&
				(strcmp(section->Get_string("tandy"),"auto")!=0)) return;
		} else {
			/* only enable tandy sound if tandy=true */
			if ((strcmp(section->Get_string("tandy"),"true")!=0) &&
				(strcmp(section->Get_string("tandy"),"on")!=0)) return;

			/* ports from second DMA controller conflict with tandy ports */
			CloseSecondDMAController();

			if (enable_hw_tandy_dac) {
				WriteHandler[2].Install(0x1e0,TandySN76496Write,IO_MB,2);
				WriteHandler[3].Install(0x1e4,TandyDACWrite,IO_MB,4);
//				ReadHandler[3].Install(0x1e4,TandyDACRead,IO_MB,4);
			}
		}


		Bit32u sample_rate = section->Get_int("tandyrate");
		tandy.chan=MixerChan.Install(&TandySN76496Update,sample_rate,"TANDY");

		WriteHandler[0].Install(0xc0,TandySN76496Write,IO_MB,2);

		if (enable_hw_tandy_dac) {
			// enable low-level Tandy DAC emulation
			WriteHandler[1].Install(0xc4,TandyDACWrite,IO_MB,4);
			ReadHandler[1].Install(0xc4,TandyDACRead,IO_MB,4);

			tandy.dac.enabled=true;
			tandy.dac.chan=MixerChanDAC.Install(&TandyDACUpdate,sample_rate,"TANDYDAC");

			tandy.dac.hw.base=0xc4;
			tandy.dac.hw.irq =7;
			tandy.dac.hw.dma =1;
		} else {
			tandy.dac.enabled=false;
			tandy.dac.hw.base=0;
			tandy.dac.hw.irq =0;
			tandy.dac.hw.dma =0;
		}

		tandy.dac.control=0;
		tandy.dac.mode   =0;
		tandy.dac.irq_activated=false;
		tandy.dac.frequency=0;
		tandy.dac.amplitude=0;
		tandy.dac.dma.last_sample=0;


		tandy.enabled=false;
		real_writeb(0x40,0xd4,0xff);	/* BIOS Tandy DAC initialization value */

		SN76496Reset( &sn, 3579545, sample_rate );
	}
	~TANDYSOUND(){ }
};



static TANDYSOUND* test;

void TANDYSOUND_ShutDown(Section* /*sec*/) {
	delete test;	
}

void TANDYSOUND_OnReset(Section* sec) {
	if (test == NULL) {
		LOG(LOG_MISC,LOG_DEBUG)("Allocating Tandy speaker emulation");
		test = new TANDYSOUND(control->GetSection("speaker"));
	}
}

void TANDYSOUND_Init() {
	LOG(LOG_MISC,LOG_DEBUG)("Initializing Tandy voice emulation");

	AddExitFunction(AddExitFunctionFuncPair(TANDYSOUND_ShutDown),true);
	AddVMEventFunction(VM_EVENT_RESET,AddVMEventFunctionFuncPair(TANDYSOUND_OnReset));
}

