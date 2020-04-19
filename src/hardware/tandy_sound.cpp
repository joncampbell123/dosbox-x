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
#include "control.h"
#include "hardware.h"
#include <cstring>
#include <math.h>
#include "mame/emu.h"
#include "mame/sn76496.h"


#define SOUND_CLOCK (14318180 / 4)

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

static sn76496_device device_sn76496(machine_config(), 0, 0, SOUND_CLOCK );
static ncr8496_device device_ncr8496(machine_config(), 0, 0, SOUND_CLOCK);

static sn76496_base_device* activeDevice = &device_ncr8496;
#define device (*activeDevice)

static void SN76496Write(Bitu /*port*/,Bitu data,Bitu /*iolen*/) {
	tandy.last_write=PIC_Ticks;
	if (!tandy.enabled) {
		tandy.chan->Enable(true);
		tandy.enabled=true;
	}

	// assume state change, always.
	// this hack allows sample accurate rendering without enabling sample accurate mode in the mixer.
	tandy.chan->FillUp();

	device.write(data);

//	LOG_MSG("3voice write %X at time %7.3f",data,PIC_FullIndex());
}

static void SN76496Update(Bitu length) {
	//Disable the channel if it's been quiet for a while
	if ((tandy.last_write+5000)<PIC_Ticks) {
		tandy.enabled=false;
		tandy.chan->Enable(false);
		return;
	}
	const Bitu MAX_SAMPLES = 2048;
	if (length > MAX_SAMPLES)
		return;
	Bit16s buffer[MAX_SAMPLES];
	Bit16s* outputs = buffer;

	device_sound_interface::sound_stream stream;
	static_cast<device_sound_interface&>(device).sound_stream_update(stream, 0, &outputs, length);
	tandy.chan->AddSamples_m16(length, buffer);
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
		tandy.dac.frequency = (tandy.dac.frequency & 0xf00) | (Bit8u)(data & 0xff);
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
		tandy.dac.frequency = (tandy.dac.frequency & 0x00ff) | (((Bit8u)(data & 0xf)) << 8);
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
		/* even if the DAC is disabled hold the last sample and SLOWLY restore DC.
		 * jumping to silence quickly here causes loud annoying pops in Prince of Persia
		 * when sound effects play. Prince of Persia seems to emit some very non-zero DC
		 * value (like 0x08) when not playing sound effects, so this is needed to
		 * recover from that without pops. This behavior also seems to match DAC behavior
		 * with Prince of Persia according to real Tandy 1000 TL/3 hardware, complete
		 * with the 8-bit DAC stepping noise on return to zero. In any case, this
		 * modification makes the Tandy DAC sound effects in Prince of Persia much
		 * more enjoyable to listen to and match the apparent sound of real hardware. */
	} else if (tandy.dac.dma.last_sample != 128) {
		for (Bitu ct=0; ct < length; ct++) {
			tandy.dac.chan->AddSamples_m8(1,&tandy.dac.dma.last_sample);
			if (tandy.dac.dma.last_sample > 128)
				tandy.dac.dma.last_sample--;
			else if (tandy.dac.dma.last_sample < 128)
				tandy.dac.dma.last_sample++;
		}
	} else {
		tandy.dac.chan->AddSilence();
	}
}

Bit8u BIOS_tandy_D4_flag = 0;

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

		BIOS_tandy_D4_flag = 0;

		//Select the correct tandy chip implementation
		if (machine == MCH_PCJR) activeDevice = &device_sn76496;
		else activeDevice = &device_ncr8496;

		if (IS_TANDY_ARCH) {
			/* enable tandy sound if tandy=true/auto */
			if ((strcmp(section->Get_string("tandy"),"true")!=0) &&
				(strcmp(section->Get_string("tandy"),"on")!=0) &&
				(strcmp(section->Get_string("tandy"),"auto")!=0)) return;
		} else {
			/* only enable tandy sound if tandy=true */
			if ((strcmp(section->Get_string("tandy"),"true")!=0) &&
				(strcmp(section->Get_string("tandy"),"on")!=0)) return;

			if (enable_hw_tandy_dac) {
				WriteHandler[2].Install(0x1e0,SN76496Write,IO_MB,2);
				WriteHandler[3].Install(0x1e4,TandyDACWrite,IO_MB,4);
//				ReadHandler[3].Install(0x1e4,TandyDACRead,IO_MB,4);
			}
		}

		/* ports from second DMA controller conflict with tandy ports at 0xC0.
		 * Furthermore, the default I/O handlers after de-registration are needed
		 * to ensure the SN76496 is writeable at port 0xC0 whether you're doing
		 * normal 8-bit I/O or your a weirdo like Prince of Persia using 16-bit
		 * I/O to write frequency values. (bugfix for Tandy mode of Prince of
		 * Persia). */
		CloseSecondDMAController();

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
		tandy.dac.dma.last_sample=128;


		tandy.enabled=false;
		BIOS_tandy_D4_flag = 0xFF;

		((device_t&)device).device_start();
		device.convert_samplerate(sample_rate);

	}
	~TANDYSOUND(){ }
};



static TANDYSOUND* test = NULL;

void TANDYSOUND_ShutDown(Section* /*sec*/) {
    if (test) {
        delete test;
        test = NULL;
    }
}

void TANDYSOUND_OnReset(Section* sec) {
    (void)sec;//UNUSED
	if (test == NULL && !IS_PC98_ARCH) {
		LOG(LOG_MISC,LOG_DEBUG)("Allocating Tandy speaker emulation");
		test = new TANDYSOUND(control->GetSection("speaker"));
	}
}

void TANDYSOUND_Init() {
	LOG(LOG_MISC,LOG_DEBUG)("Initializing Tandy voice emulation");

	AddExitFunction(AddExitFunctionFuncPair(TANDYSOUND_ShutDown),true);
	AddVMEventFunction(VM_EVENT_RESET,AddVMEventFunctionFuncPair(TANDYSOUND_OnReset));
}

