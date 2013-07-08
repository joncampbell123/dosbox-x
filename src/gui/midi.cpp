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

#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "dosbox.h"
#include "cross.h"
#include "support.h"
#include "setup.h"
#include "mapper.h"
#include "pic.h"
#include "hardware.h"

#define SYSEX_SIZE 1024
#define RAWBUF	1024

Bit8u MIDI_evt_len[256] = {
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x00
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x10
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x20
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x30
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x40
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x50
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x60
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x70

  3,3,3,3, 3,3,3,3, 3,3,3,3, 3,3,3,3,  // 0x80
  3,3,3,3, 3,3,3,3, 3,3,3,3, 3,3,3,3,  // 0x90
  3,3,3,3, 3,3,3,3, 3,3,3,3, 3,3,3,3,  // 0xa0
  3,3,3,3, 3,3,3,3, 3,3,3,3, 3,3,3,3,  // 0xb0

  2,2,2,2, 2,2,2,2, 2,2,2,2, 2,2,2,2,  // 0xc0
  2,2,2,2, 2,2,2,2, 2,2,2,2, 2,2,2,2,  // 0xd0

  3,3,3,3, 3,3,3,3, 3,3,3,3, 3,3,3,3,  // 0xe0

  0,2,3,2, 0,0,1,0, 1,0,1,1, 1,0,1,0   // 0xf0
};

class MidiHandler;

MidiHandler * handler_list=0;

class MidiHandler {
public:
	MidiHandler() {
		next=handler_list;
		handler_list=this;
	};
	virtual bool Open(const char * /*conf*/) { return true; };
	virtual void Close(void) {};
	virtual void PlayMsg(Bit8u * /*msg*/) {};
	virtual void PlaySysex(Bit8u * /*sysex*/,Bitu /*len*/) {};
	virtual const char * GetName(void) { return "none"; };
	virtual ~MidiHandler() { };
	MidiHandler * next;
};

MidiHandler Midi_none;

/* Include different midi drivers, lowest ones get checked first for default */

#if defined (HAVE_FLUID)

#include "midi_fluidsynth.h"

#endif

#if defined(MACOSX)

#include "midi_coremidi.h"
#include "midi_coreaudio.h"

#elif defined (WIN32)

#include "midi_win32.h"

#else

#include "midi_oss.h"

#endif

#if defined (HAVE_ALSA)

#include "midi_alsa.h"

#endif

static struct {
	Bitu status;
	Bitu cmd_len;
	Bitu cmd_pos;
	Bit8u cmd_buf[8];
	Bit8u rt_buf[8];
	struct {
		Bit8u buf[SYSEX_SIZE];
		Bitu used;
	} sysex;
	bool available;
	MidiHandler * handler;
} midi;

void MIDI_RawOutByte(Bit8u data) {
	/* Test for a realtime MIDI message */
	if (data>=0xf8) {
		midi.rt_buf[0]=data;
		midi.handler->PlayMsg(midi.rt_buf);
		return;
	}	 
	/* Test for a active sysex tranfer */
	if (midi.status==0xf0) {
		if (!(data&0x80)) { 
			if (midi.sysex.used<(SYSEX_SIZE-1)) midi.sysex.buf[midi.sysex.used++]=data;
			return;
		} else {
			midi.sysex.buf[midi.sysex.used++]=0xf7;
			midi.handler->PlaySysex(midi.sysex.buf,midi.sysex.used);
			LOG(LOG_ALL,LOG_NORMAL)("Sysex message size %d",midi.sysex.used);
			if (CaptureState & CAPTURE_MIDI) {
				CAPTURE_AddMidi( true, midi.sysex.used-1, &midi.sysex.buf[1]);
			}
		}
	}
	if (data&0x80) {
		midi.status=data;
		midi.cmd_pos=0;
		midi.cmd_len=MIDI_evt_len[data];
		if (midi.status==0xf0) {
			midi.sysex.buf[0]=0xf0;
			midi.sysex.used=1;
		}
	}
	if (midi.cmd_len) {
		midi.cmd_buf[midi.cmd_pos++]=data;
		if (midi.cmd_pos >= midi.cmd_len) {
			if (CaptureState & CAPTURE_MIDI) {
				CAPTURE_AddMidi(false, midi.cmd_len, midi.cmd_buf);
			}
			midi.handler->PlayMsg(midi.cmd_buf);
			midi.cmd_pos=1;		//Use Running status
		}
	}
}

bool MIDI_Available(void)  {
	return midi.available;
}

class MIDI:public Module_base{
public:
	MIDI(Section* configuration):Module_base(configuration){
		Section_prop * section=static_cast<Section_prop *>(configuration);
		const char * dev=section->Get_string("mididevice");
		const char * conf=section->Get_string("midiconfig");
		/* If device = "default" go for first handler that works */
		MidiHandler * handler;
//		MAPPER_AddHandler(MIDI_SaveRawEvent,MK_f8,MMOD1|MMOD2,"caprawmidi","Cap MIDI");
		midi.status=0x00;
		midi.cmd_pos=0;
		midi.cmd_len=0;
		if (!strcasecmp(dev,"default")) goto getdefault;
		handler=handler_list;
		while (handler) {
			if (!strcasecmp(dev,handler->GetName())) {
				if (!handler->Open(conf)) {
					LOG_MSG("MIDI:Can't open device:%s with config:%s.",dev,conf);	
					goto getdefault;
				}
				midi.handler=handler;
				midi.available=true;	
				LOG_MSG("MIDI:Opened device:%s",handler->GetName());
				return;
			}
			handler=handler->next;
		}
		LOG_MSG("MIDI:Can't find device:%s, finding default handler.",dev);	
getdefault:	
		handler=handler_list;
		while (handler) {
			if (handler->Open(conf)) {
				midi.available=true;	
				midi.handler=handler;
				LOG_MSG("MIDI:Opened device:%s",handler->GetName());
				return;
			}
			handler=handler->next;
		}
		/* This shouldn't be possible */
	}
	~MIDI(){
		if(midi.available) midi.handler->Close();
		midi.available = false;
		midi.handler = 0;
	}
};


static MIDI* test;
void MIDI_Destroy(Section* /*sec*/){
	delete test;
}
void MIDI_Init(Section * sec) {
	test = new MIDI(sec);
	sec->AddDestroyFunction(&MIDI_Destroy,true);
}
