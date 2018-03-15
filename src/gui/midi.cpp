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

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include <algorithm>

#include "SDL.h"

#include "dosbox.h"
#include "control.h"
#include "cross.h"
#include "support.h"
#include "setup.h"
#include "mapper.h"
#include "pic.h"
#include "hardware.h"
#include "timer.h"

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
	virtual void Reset() {};
	virtual ~MidiHandler() { };
	MidiHandler * next;
};

MidiHandler Midi_none;


static struct midi_state_t {
	Bitu status;
	Bitu cmd_len;
	Bitu cmd_pos;
	Bit8u cmd_buf[8];
	Bit8u rt_buf[8];
	struct midi_state_sysex_t {
		Bit8u buf[SYSEX_SIZE];
		Bitu used;
		Bitu delay;
		Bit32u start;

		midi_state_sysex_t() : used(0), delay(0), start(0) { }
	} sysex;
	bool available;
	MidiHandler * handler;

	midi_state_t() : status(0x00), cmd_len(0), cmd_pos(0), available(false), handler(NULL) { }
} midi;


static struct {
	bool init;
	bool ignore;

	// NOTE: 16-bit ($ffff = not used, $00-ff = data value)
	Bit16u code_80[0x80];			// note off (w/ velocity)
	Bit16u code_90[0x80];			// note on (w/ velocity)
	Bit16u code_a0[0x80];			// aftertouch (polyphonic key pressure)
	Bit16u code_b0[0x80];			// Continuous controller (GM 1.0 + GS)
	Bit16u code_c0[1];				// patch change
	Bit16u code_d0[1];				// channel pressure (after-touch)
	Bit16u code_e0[2];				// pitch bend
	//Bit16u code_f0-ff				// system messages

	Bit16u code_rpn_coarse[3];		// registered parameter numbers (GM 1.0)
	Bit16u code_rpn_fine[3];			// registered parameter numbers (GM 1.0)
} midi_state[16];



/* Include different midi drivers, lowest ones get checked first for default */

#if C_MT32
#include "midi_mt32.h"
static MidiHandler_mt32 &Midi_mt32 = MidiHandler_mt32::GetInstance();
#endif

#if C_FLUIDSYNTH
#include "midi_synth.h"
#endif

#if !defined(WIN32)
# include "midi_timidity.h"
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


//#define WIN32_MIDI_STATE_DEBUG

void MIDI_State_Reset()
{
	memset( &midi_state, 0xff, sizeof(midi_state) );

	midi_state[0].init = true;
	midi_state[0].ignore = false;
}


void MIDI_State_SaveMessage()
{
	Bit8u channel, command, arg1, arg2;

	if( midi_state[0].init == false ) {
		MIDI_State_Reset();
	}

	if( midi_state[0].ignore == true ) return;

	// skip MUNT
	//if( strcmp( midi.handler->GetName(), "mt32" ) == 0 ) {
	//	return;
	//}

	channel = midi.cmd_buf[0] & 0xf;
	command = midi.cmd_buf[0] >> 4;
	arg1 = midi.cmd_buf[1];
	arg2 = midi.cmd_buf[2];

	switch( command ) {
		case 0x8:		// Note off
			// - arg1 = note, arg2 = velocity off
			midi_state[channel].code_80[arg1] = arg2;

			memset( &midi_state[channel].code_90[arg1], 0xff, sizeof(midi_state[channel].code_90[arg1]) );
			memset( &midi_state[channel].code_a0[arg1], 0xff, sizeof(midi_state[channel].code_a0[arg1]) );
			memset( midi_state[channel].code_d0, 0xff, sizeof(midi_state[channel].code_d0) );
			break;


		case 0x9:		// Note on
			if( arg2 > 0 ) {
				// - arg1 = note, arg2 = velocity on
				midi_state[channel].code_90[arg1] = arg2;

				memset( &midi_state[channel].code_80[arg1], 0xff, sizeof(midi_state[channel].code_80[arg1]) );
			}
			else {
				// velocity = 0 (note off)
				midi_state[channel].code_80[arg1] = arg2;

				memset( &midi_state[channel].code_90[arg1], 0xff, sizeof(midi_state[channel].code_90[arg1]) );
				memset( &midi_state[channel].code_a0[arg1], 0xff, sizeof(midi_state[channel].code_a0[arg1]) );
				memset( midi_state[channel].code_d0, 0xff, sizeof(midi_state[channel].code_d0) );
			}
			break;


		case 0xA:		// Aftertouch (polyphonic pressure)
			midi_state[channel].code_a0[arg1] = arg2;
			break;


		case 0xB:		// Controller #s
			midi_state[channel].code_b0[arg1] = arg2;

			switch( arg1 ) {
				// General Midi 1.0
				case 0x01: break;		// Modulation

				case 0x07: break;		// Volume
				case 0x0A: break;		// Pan
				case 0x0B: break;		// Expression

				case 0x40: break;		// Sustain pedal

				case 0x64: break;		// Registered Parameter Number (RPN) LSB
				case 0x65: break;		// Registered Parameter Number (RPN) MSB

				case 0x79:					// All controllers off
					/*
					(likely GM1+GM2)
					Set Expression (#11) to 127
					Set Modulation (#1) to 0
					Set Pedals (#64, #65, #66, #67) to 0
					Set Registered and Non-registered parameter number LSB and MSB (#98-#101) to null value (127)
					Set pitch bender to center (64/0)
					Reset channel pressure to 0 
					Reset polyphonic pressure for all notes to 0.
					*/
					memset( midi_state[channel].code_a0, 0xff, sizeof(midi_state[channel].code_a0) );
					memset( midi_state[channel].code_c0, 0xff, sizeof(midi_state[channel].code_c0) );
					memset( midi_state[channel].code_d0, 0xff, sizeof(midi_state[channel].code_d0) );
					memset( midi_state[channel].code_e0, 0xff, sizeof(midi_state[channel].code_e0) );

					memset( midi_state[channel].code_b0+0x01, 0xff, sizeof(midi_state[channel].code_b0[0x01]) );
					memset( midi_state[channel].code_b0+0x0b, 0xff, sizeof(midi_state[channel].code_b0[0x0b]) );
					memset( midi_state[channel].code_b0+0x40, 0xff, sizeof(midi_state[channel].code_b0[0x40]) );

					memset( midi_state[channel].code_rpn_coarse, 0xff, sizeof(midi_state[channel].code_rpn_coarse) );
					memset( midi_state[channel].code_rpn_fine, 0xff, sizeof(midi_state[channel].code_rpn_fine) );

					/*
					(likely GM1+GM2)
					Do NOT reset Bank Select (#0/#32)
					Do NOT reset Volume (#7)
					Do NOT reset Pan (#10)
					Do NOT reset Program Change
					Do NOT reset Effect Controllers (#91-#95) (5B-5F)
					Do NOT reset Sound Controllers (#70-#79) (46-4F)
					Do NOT reset other channel mode messages (#120-#127) (78-7F)
					Do NOT reset registered or non-registered parameters.
					*/
					//Intentional fall-through

				case 0x7b:					// All notes off
					memset( midi_state[channel].code_80, 0xff, sizeof(midi_state[channel].code_80) );
					memset( midi_state[channel].code_90, 0xff, sizeof(midi_state[channel].code_90) );
					break;

				//******************************************
				//******************************************
				//******************************************

				// Roland GS
				case 0x00: break;		// Bank select MSB
				case 0x05: break;		// Portamento time
				case 0x20: break;		// Bank select LSB
				case 0x41: break;		// Portamento
				case 0x42: break;		// Sostenuto
				case 0x43: break;		// Soft pedal
				case 0x54: break;		// Portamento control
				case 0x5B: break;		// Effect 1 (Reverb) send level
				case 0x5D: break;		// Effect 3 (Chorus) send level
				case 0x5E: break;		// Effect 4 (Variation) send level

				// TODO: Add more as needed
				case 0x62: break;		// NRPN LSB
				case 0x63: break;		// NRPN MSB
				case 0x78:					// All sounds off
					memset( midi_state[channel].code_80, 0xff, sizeof(midi_state[channel].code_80) );
					memset( midi_state[channel].code_90, 0xff, sizeof(midi_state[channel].code_90) );
					break;

				/*
				// Roland MT-32
				case 0x64:
					parts[part]->setRPNLSB(velocity);
					break;
				case 0x65:
					parts[part]->setRPNMSB(velocity);
					break;
				case 0x7B: // All notes off
					//printDebug("All notes off");
					parts[part]->allNotesOff();
					break;
				case 0x7C:
				case 0x7D:
				case 0x7E:
				case 0x7F:
					// CONFIRMED:Mok: A real LAPC-I responds to these controllers as follows:
					parts[part]->setHoldPedal(false);
					parts[part]->allNotesOff();
					break;
				*/

				//******************************************
				//******************************************
				//******************************************

				/*
				(LSB,MSB) RPN

				GM 1.0
				00,00 = Pitch bend range (fine + coarse)
				01,00 = Channel Fine tuning
				02,00 = Channel Coarse tuning
				7F,7F = None
				*/

				case 0x06:					// Data entry (coarse)
					{
						int rpn;

						rpn = midi_state[channel].code_b0[0x64];
						rpn |= (midi_state[channel].code_b0[0x65]) << 8;


						// GM 1.0 = 0-2
						if( rpn >= 3 ) break;
						if( rpn == 0x7f7f ) break;

						midi_state[channel].code_rpn_coarse[rpn] = arg2;
					}
					break;


				case 0x26:					// Data entry (fine)
					{
						int rpn;

						rpn = midi_state[channel].code_b0[0x64];
						rpn |= (midi_state[channel].code_b0[0x65]) << 8;


						// GM 1.0 = 0-2
						if( rpn >= 3 ) break;
						if( rpn == 0x7f7f ) break;

						midi_state[channel].code_rpn_fine[rpn] = arg2;
					}
					break;


				default:			// unsupported
					break;
			}
			break;


		case 0xC:		// Patch change
			midi_state[channel].code_c0[0] = arg1;
			break;

		case 0xD:		// Channel pressure (aftertouch)
			midi_state[channel].code_d0[0] = arg1;
			break;

		case 0xE:		// Pitch bend
			midi_state[channel].code_e0[0] = arg1;
			midi_state[channel].code_e0[1] = arg2;
			break;

		case 0xF:		// System messages
			// TODO: General Midi 1.0 says 'Master Volume' SysEx
			break;

		// unhandled case
		default:
			break;
	}
}


void MIDI_RawOutByte(Bit8u);
void MIDI_State_LoadMessage()
{
	if( midi_state[0].init == false ) {
		MIDI_State_Reset();
	}


	midi_state[0].ignore = true;

	// flush data buffer - throw invalid midi message
	MIDI_RawOutByte(0xf7);

	// shutdown sound
	for( int lcv=0; lcv<16; lcv++ ) {
		MIDI_RawOutByte( 0xb0+lcv );
		MIDI_RawOutByte( 0x78 );
		MIDI_RawOutByte( 0x00 );
		MIDI_RawOutByte( 0x79 );
		MIDI_RawOutByte( 0x00 );

		MIDI_RawOutByte( 0x7b );
		MIDI_RawOutByte( 0x00 );
	}


	for( int lcv=0; lcv<16; lcv++ ) {
		// control states (bx - 00-5F)
		MIDI_RawOutByte( 0xb0+lcv );


		for( int lcv2=0; lcv2<0x80; lcv2++ ) {
			if( lcv2 >= 0x60 ) break;
			if( midi_state[lcv].code_b0[lcv2] == 0xffff ) continue;

			// RPN data - coarse / fine
			if( lcv2 == 0x06 ) continue;
			if( lcv2 == 0x26 ) continue;


			//MIDI_RawOutByte( 0xb0+lcv );
			MIDI_RawOutByte( lcv2 );
			MIDI_RawOutByte( midi_state[lcv].code_b0[lcv2] & 0xff );
		}


		// control states - RPN data (GM 1.0)
		for( int lcv2=0; lcv2<3; lcv2++ ) {
			if( midi_state[lcv].code_rpn_coarse[lcv2] == 0xffff &&
					midi_state[lcv].code_rpn_fine[lcv2] == 0xffff ) continue;


			//MIDI_RawOutByte( 0xb0+lcv );
			MIDI_RawOutByte( 0x64 );
			MIDI_RawOutByte( lcv2 );
			MIDI_RawOutByte( 0x65 );
			MIDI_RawOutByte( 0x00 );


			//MIDI_RawOutByte( 0xb0+lcv );
			if( midi_state[lcv].code_rpn_coarse[lcv2] != 0xffff ) {
				MIDI_RawOutByte( 0x06 );
				MIDI_RawOutByte( midi_state[lcv].code_rpn_coarse[lcv2] & 0xff );
			}

			if( midi_state[lcv].code_rpn_fine[lcv2] != 0xffff ) {
				MIDI_RawOutByte( 0x26 );
				MIDI_RawOutByte( midi_state[lcv].code_rpn_fine[lcv2] & 0xff );
			}


			//MIDI_RawOutByte( 0xb0+lcv );
			MIDI_RawOutByte( 0x64 );
			MIDI_RawOutByte( 0x7f );
			MIDI_RawOutByte( 0x65 );
			MIDI_RawOutByte( 0x7f );
		}


		// program change
		if( midi_state[lcv].code_c0[0] != 0xffff ) {
			MIDI_RawOutByte( 0xc0+lcv );
			MIDI_RawOutByte( midi_state[lcv].code_c0[0]&0xff );
		}


		// pitch wheel change
		if( midi_state[lcv].code_e0[0] != 0xffff ) {
			MIDI_RawOutByte( 0xe0+lcv );
			MIDI_RawOutByte( midi_state[lcv].code_e0[0]&0xff );
			MIDI_RawOutByte( midi_state[lcv].code_e0[1]&0xff );
		}

		//******************************************
		//******************************************
		//******************************************

		// note on
		MIDI_RawOutByte( 0x90+lcv );

		for( int lcv2=0; lcv2<0x80; lcv2++ ) {
			if( midi_state[lcv].code_90[lcv2] == 0xffff ) continue;


			//MIDI_RawOutByte( 0x90+lcv );
			MIDI_RawOutByte( lcv2 );
			MIDI_RawOutByte( midi_state[lcv].code_90[lcv2]&0xff );
		}


		// polyphonic aftertouch
		MIDI_RawOutByte( 0xa0+lcv );

		for( int lcv2=0; lcv2<0x80; lcv2++ ) {
			if( midi_state[lcv].code_a0[lcv2] == 0xffff ) continue;


			//MIDI_RawOutByte( 0xa0+lcv );
			MIDI_RawOutByte( lcv2 );
			MIDI_RawOutByte( midi_state[lcv].code_a0[lcv2]&0xff );
		}


		// channel aftertouch
		if( midi_state[lcv].code_d0[0] != 0xffff ) {
			MIDI_RawOutByte( 0xd0+lcv );
			MIDI_RawOutByte( midi_state[lcv].code_d0[0]&0xff );
		}
	}

	midi_state[0].ignore = false;
}


void MIDI_RawOutByte(Bit8u data) {
	if (midi.sysex.start) {
		Bit32u passed_ticks = GetTicks() - midi.sysex.start;
		if (passed_ticks < midi.sysex.delay) SDL_Delay(midi.sysex.delay - passed_ticks);
	}

	/* Test for a realtime MIDI message */
	if (data>=0xf8) {
		midi.rt_buf[0]=data;
		midi.handler->PlayMsg(midi.rt_buf);
		return;
	}	 
	/* Test for a active sysex tranfer */
	if (midi.status==0xf0) {
		if (!(data&0x80)) { 
			if (midi.sysex.used<(SYSEX_SIZE-1)) midi.sysex.buf[midi.sysex.used++] = data;
			return;
		} else {
			midi.sysex.buf[midi.sysex.used++] = 0xf7;

			if ((midi.sysex.start) && (midi.sysex.used >= 4) && (midi.sysex.used <= 9) && (midi.sysex.buf[1] == 0x41) && (midi.sysex.buf[3] == 0x16)) {
				LOG(LOG_ALL,LOG_ERROR)("MIDI:Skipping invalid MT-32 SysEx midi message (too short to contain a checksum)");
			} else {
//				LOG(LOG_ALL,LOG_NORMAL)("Play sysex; address:%02X %02X %02X, length:%4d, delay:%3d", midi.sysex.buf[5], midi.sysex.buf[6], midi.sysex.buf[7], midi.sysex.used, midi.sysex.delay);
				midi.handler->PlaySysex(midi.sysex.buf, midi.sysex.used);
				if (midi.sysex.start) {
					if (midi.sysex.buf[5] == 0x7F) {
						midi.sysex.delay = 290; // All Parameters reset
					} else if (midi.sysex.buf[5] == 0x10 && midi.sysex.buf[6] == 0x00 && midi.sysex.buf[7] == 0x04) {
						midi.sysex.delay = 145; // Viking Child
					} else if (midi.sysex.buf[5] == 0x10 && midi.sysex.buf[6] == 0x00 && midi.sysex.buf[7] == 0x01) {
						midi.sysex.delay = 30; // Dark Sun 1
					} else midi.sysex.delay = (Bitu)(((float)(midi.sysex.used) * 1.25f) * 1000.0f / 3125.0f) + 2;
					midi.sysex.start = GetTicks();
				}
			}

			LOG(LOG_ALL,LOG_NORMAL)("Sysex message size %d",(int)midi.sysex.used);
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

			MIDI_State_SaveMessage();
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
		std::string fullconf=section->Get_string("midiconfig");
		/* If device = "default" go for first handler that works */
		MidiHandler * handler;
//		MAPPER_AddHandler(MIDI_SaveRawEvent,MK_f8,MMOD1|MMOD2,"caprawmidi","Cap MIDI");
		midi.sysex.delay = 0;
		midi.sysex.start = 0;
		if (fullconf.find("delaysysex") != std::string::npos) {
			midi.sysex.start = GetTicks();
			fullconf.erase(fullconf.find("delaysysex"));
			LOG(LOG_MISC,LOG_DEBUG)("MIDI:Using delayed SysEx processing");
		}
		std::remove(fullconf.begin(), fullconf.end(), ' ');
		const char * conf = fullconf.c_str();
		midi.status=0x00;
		midi.cmd_pos=0;
		midi.cmd_len=0;
		if (!strcasecmp(dev,"default")) goto getdefault;
		handler=handler_list;
		while (handler) {
			if (!strcasecmp(dev,handler->GetName())) {
#if C_FLUIDSYNTH
                       if(!strcasecmp(dev,"synth"))    // synth device, get sample rate from config
                           synthsamplerate=section->Get_int("samplerate");
#endif
				if (!handler->Open(conf)) {
					LOG(LOG_MISC,LOG_WARN)("MIDI:Can't open device:%s with config:%s.",dev,conf);	
					goto getdefault;
				}
				midi.handler=handler;
				midi.available=true;	
				LOG(LOG_MISC,LOG_DEBUG)("MIDI:Opened device:%s",handler->GetName());

				// force reset to prevent crashes (when not properly shutdown)
				// ex. Roland VSC = unexpected hard system crash
				midi_state[0].init = false;
				MIDI_State_LoadMessage();
				return;
			}
			handler=handler->next;
		}
		LOG(LOG_MISC,LOG_DEBUG)("MIDI:Can't find device:%s, finding default handler.",dev);	
getdefault:	
		handler=handler_list;
		while (handler) {
			if (handler->Open(conf)) {
				midi.available=true;	
				midi.handler=handler;
				LOG(LOG_MISC,LOG_DEBUG)("MIDI:Opened device:%s",handler->GetName());
				return;
			}
			handler=handler->next;
		}
		/* This shouldn't be possible */
	}
	~MIDI(){
		if( midi.status < 0xf0 ) {
			// throw invalid midi message - start new cmd
			MIDI_RawOutByte(0x80);
		}
		else if( midi.status == 0xf0 ) {
			// SysEx - throw invalid midi message
			MIDI_RawOutByte(0xf7);
		}
		if(midi.available) midi.handler->Close();
		midi.available = false;
		midi.handler = 0;
	}
};


static MIDI* test = NULL;
void MIDI_Destroy(Section* /*sec*/){
	if (test != NULL) {
		delete test;
		test = NULL;
	}
}

void MIDI_Init() {
	LOG(LOG_MISC,LOG_DEBUG)("Initializing MIDI emulation");

	test = new MIDI(control->GetSection("midi"));
	AddExitFunction(AddExitFunctionFuncPair(MIDI_Destroy),true);
}

void MIDI_GUI_OnSectionPropChange(Section *x) {
	if (test != NULL) {
		delete test;
		test = NULL;
	}

	test = new MIDI(control->GetSection("midi"));
}

