/*
 *  Copyright (C) 2002-2010  The DOSBox Team
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


struct SN76496 {
	int SampleRate;
	unsigned int UpdateStep;
	int VolTable[16];	/* volume table         */
	int Register[8];	/* registers */
	int LastRegister;	/* last register written */
	int Volume[4];		/* volume of voice 0-2 and noise */
	unsigned int RNG;		/* noise generator      */
	int NoiseFB;		/* noise feedback mask */
	int Period[4];
	int Count[4];
	int Output[4];
};

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


static void SN76496Write(Bitu /*port*/,Bitu data,Bitu /*iolen*/) {
	struct SN76496 *R = &sn;

	tandy.last_write=PIC_Ticks;
	if (!tandy.enabled) {
		tandy.chan->Enable(true);
		tandy.enabled=true;
	}

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

static void SN76496Update(Bitu length) {
	if ((tandy.last_write+5000)<PIC_Ticks) {
		tandy.enabled=false;
		tandy.chan->Enable(false);
	}
	int i;
	struct SN76496 *R = &sn;
	Bit16s * buffer=(Bit16s *)MixTemp;

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
	tandy.chan->AddSamples_m16(length,(Bit16s *)MixTemp);
}



static void SN76496_set_clock(int clock) {
	struct SN76496 *R = &sn;

	/* the base clock for the tone generators is the chip clock divided by 16; */
	/* for the noise generator, it is clock / 256. */
	/* Here we calculate the number of steps which happen during one sample */
	/* at the given sample rate. No. of events = sample rate / (clock/16). */
	/* STEP is a multiplier used to turn the fraction into a fixed point */
	/* number. */
	R->UpdateStep = (unsigned int)(((double)STEP * R->SampleRate * 16) / clock);
}


static void SN76496_set_gain(int gain) {
	struct SN76496 *R = &sn;
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

static void TandyDACModeChanged(void) {
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

static void TandyDACDMAEnabled(void) {
	TandyDACModeChanged();
}

static void TandyDACDMADisabled(void) {
}

static void TandyDACWrite(Bitu port,Bitu data,Bitu /*iolen*/) {
	switch (port) {
	case 0xc4: {
		Bitu oldmode = tandy.dac.mode;
		tandy.dac.mode = (Bit8u)(data&0xff);
		if ((data&3)!=(oldmode&3)) {
			TandyDACModeChanged();
		}
		if (((data&0x0c)==0x0c) && ((oldmode&0x0c)!=0x0c)) {
			TandyDACDMAEnabled();
		} else if (((data&0x0c)!=0x0c) && ((oldmode&0x0c)==0x0c)) {
			TandyDACDMADisabled();
		}
		}
		break;
	case 0xc5:
		switch (tandy.dac.mode&3) {
		case 0:
			// joystick mode
			break;
		case 1:
			tandy.dac.control = (Bit8u)(data&0xff);
			break;
		case 2:
			break;
		case 3:
			// direct output
			break;
		}
		break;
	case 0xc6:
		tandy.dac.frequency = tandy.dac.frequency & 0xf00 | (Bit8u)(data&0xff);
		switch (tandy.dac.mode&3) {
		case 0:
			// joystick mode
			break;
		case 1:
		case 2:
		case 3:
			TandyDACModeChanged();
			break;
		}
		break;
	case 0xc7:
		tandy.dac.frequency = tandy.dac.frequency & 0x00ff | (((Bit8u)(data&0xf))<<8);
		tandy.dac.amplitude = (Bit8u)(data>>5);
		switch (tandy.dac.mode&3) {
		case 0:
			// joystick mode
			break;
		case 1:
		case 2:
		case 3:
			TandyDACModeChanged();
			break;
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
	LOG_MSG("Tandy DAC: Read from unknown %X",port);
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
				WriteHandler[2].Install(0x1e0,SN76496Write,IO_MB,2);
				WriteHandler[3].Install(0x1e4,TandyDACWrite,IO_MB,4);
//				ReadHandler[3].Install(0x1e4,TandyDACRead,IO_MB,4);
			}
		}


		Bit32u sample_rate = section->Get_int("tandyrate");
		tandy.chan=MixerChan.Install(&SN76496Update,sample_rate,"TANDY");

		WriteHandler[0].Install(0xc0,SN76496Write,IO_MB,2);

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

		Bitu i;
		struct SN76496 *R = &sn;
		R->SampleRate = sample_rate;
		SN76496_set_clock(3579545);
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
		SN76496_set_gain(0x1);
	}
	~TANDYSOUND(){ }
};



static TANDYSOUND* test;

void TANDYSOUND_ShutDown(Section* /*sec*/) {
	delete test;	
}

void TANDYSOUND_Init(Section* sec) {
	test = new TANDYSOUND(sec);
	sec->AddDestroyFunction(&TANDYSOUND_ShutDown,true);
}
