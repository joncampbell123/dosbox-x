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


#include <string.h>
#include <algorithm>
#include "dosbox.h"
#include "inout.h"
#include "mixer.h"
#include "pic.h"
#include "setup.h"
#include "bios.h"
#include "mem.h"
#include "control.h"

using namespace std;

unsigned int DISNEY_BASE = 0x0378;

#define DISNEY_SIZE 128

typedef struct _dac_channel {
	Bit8u buffer[DISNEY_SIZE];	// data buffer
	Bitu used;					// current data buffer level
	double speedcheck_sum;
	double speedcheck_last;
	bool speedcheck_failed;
	bool speedcheck_init;
} dac_channel;

static struct {
	// parallel port stuff
	Bit8u data;
	Bit8u status;
	Bit8u control;
	// the D/A channels
	dac_channel da[2];

	Bitu last_used;
	MixerObject * mo;
	MixerChannel * chan;
	bool stereo;
	// which channel do we use for mono output?
	// and the channel used for stereo
	dac_channel* leader;
	
	Bitu state;
	Bitu interface_det;
	Bitu interface_det_ext;
} disney;

#define DS_IDLE 0
#define DS_RUNNING 1
#define DS_FINISH 2
#define DS_ANALYZING 3

static void DISNEY_CallBack(Bitu len);

static void DISNEY_disable(Bitu) {
	if(disney.mo) {
		disney.chan->AddSilence();
		disney.chan->Enable(false);
	}
	disney.leader = 0;
	disney.last_used = 0;
	disney.state = DS_IDLE;
	disney.interface_det = 0;
	disney.interface_det_ext = 0;
	disney.stereo = false;
}

static void DISNEY_enable(Bitu freq) {
	if(freq < 500 || freq > 100000) {
		// try again..
		disney.state = DS_IDLE;
#if 0
		LOG(LOG_MISC,LOG_NORMAL)("disney enable rejected, %ld Hz, stereo=%u",(long)freq,(int)disney.stereo);
#endif
		return;	
	} else {
#if 0
		if(disney.stereo) LOG(LOG_MISC,LOG_NORMAL)("disney enable %d Hz, stereo",freq);
		else LOG(LOG_MISC,LOG_NORMAL)("disney enable %d Hz, mono",freq);
#endif
		disney.chan->SetFreq(freq);
		disney.chan->Enable(true);
		disney.state = DS_RUNNING;
	}
}

static void DISNEY_analyze(Bitu channel){
	switch(disney.state) {
		case DS_RUNNING: // should not get here
			break;
		case DS_IDLE:
			// initialize channel data
			for(int i = 0; i < 2; i++) {
				disney.da[i].used = 0;
				disney.da[i].speedcheck_sum = 0;
				disney.da[i].speedcheck_failed = false;
				disney.da[i].speedcheck_init = false;
			}
			disney.da[channel].speedcheck_last = PIC_FullIndex();
			disney.da[channel].speedcheck_init = true;
			
			disney.state = DS_ANALYZING;
			break;

		case DS_FINISH: 
		{
			// detect stereo: if we have about the same data amount in both channels
			Bits st_diff = (Bits)disney.da[0].used - (Bits)disney.da[1].used;
			
			// find leader channel (the one with higher rate) [this good for the stereo case?]
			if(disney.da[0].used > disney.da[1].used) {
				//disney.monochannel=0;
				disney.leader = &disney.da[0];
			} else {
				//disney.monochannel=1;
				disney.leader = &disney.da[1];
			}
			
			if((st_diff < 5) && (st_diff > -5)) disney.stereo = true;
			else disney.stereo = false;

			// calculate rate for both channels
			Bitu ch_speed[2];

			for(Bitu i = 0; i < 2; i++) {
				if (disney.da[i].used > 1/*avoid divide-by-zero!*/) {
					ch_speed[i] = (Bitu)(1.0/((disney.da[i].speedcheck_sum/1000.0) /
						(float)(((float)disney.da[i].used)-1.0))); // -1.75
				}
				else {
					ch_speed[i] = 0;
				}
			}

			// choose the larger value
			DISNEY_enable(max(ch_speed[0],ch_speed[1]));
			break;
		}
		case DS_ANALYZING:
		{
			double current = PIC_FullIndex();
			dac_channel* cch = &disney.da[channel];

			if(!cch->speedcheck_init) {
				cch->speedcheck_init = true;
				cch->speedcheck_last = current;
				break;
			}
			cch->speedcheck_sum += current - cch->speedcheck_last;
			//LOG_MSG("t=%f",current - cch->speedcheck_last);
			
			// sanity checks (printer...)
			if((current - cch-> speedcheck_last) < 0.01 ||
				(current - cch-> speedcheck_last) > 2)
				cch->speedcheck_failed = true;
			
			// if both are failed we are back at start
			if(disney.da[0].speedcheck_failed && disney.da[1].speedcheck_failed) {
				disney.state=DS_IDLE;
				break;
			}

			cch->speedcheck_last = current;
			
			// analyze finish condition
			if(disney.da[0].used > 30 || disney.da[1].used > 30)
				disney.state = DS_FINISH;
			break;
		}
	}
}

static void disney_write(Bitu port,Bitu val,Bitu iolen) {
    (void)iolen;//UNUSED
	//LOG_MSG("write disney time %f addr%x val %x",PIC_FullIndex(),port,val);
	disney.last_used=PIC_Ticks;
	switch (port-DISNEY_BASE) {
	case 0:		/* Data Port */
	{
		disney.data=(Bit8u)val;
		// if data is written here too often without using the stereo
		// mechanism we use the simple DAC machanism. 
        if(disney.state != DS_RUNNING) {
			disney.interface_det++;
			if(disney.interface_det > 5)
				DISNEY_analyze(0);
		}
		if(disney.interface_det > 5) {
			if(disney.da[0].used < DISNEY_SIZE) {
				disney.da[0].buffer[disney.da[0].used] = disney.data;
				disney.da[0].used++;
			} //else LOG_MSG("disney overflow 0");
		}
		break;
	}
	case 1:		/* Status Port */		
		LOG(LOG_MISC,LOG_NORMAL)("DISNEY:Status write %x",(int)val);
		break;
	case 2:		/* Control Port */
		if((disney.control & 0x2) && !(val & 0x2)) {
			if(disney.state != DS_RUNNING) {
				disney.interface_det = 0;
				disney.interface_det_ext = 0;
				DISNEY_analyze(1);
			}

			// stereo channel latch
			if(disney.da[1].used < DISNEY_SIZE) {
				disney.da[1].buffer[disney.da[1].used] = disney.data;
				disney.da[1].used++;
			} //else LOG_MSG("disney overflow 1");
		}

		if((disney.control & 0x1) && !(val & 0x1)) {
			if(disney.state != DS_RUNNING) {
				disney.interface_det = 0;
				disney.interface_det_ext = 0;
				DISNEY_analyze(0);
			}
			// stereo channel latch
			if(disney.da[0].used < DISNEY_SIZE) {
				disney.da[0].buffer[disney.da[0].used] = disney.data;
				disney.da[0].used++;
			} //else LOG_MSG("disney overflow 0");
		}

		if((disney.control & 0x8) && !(val & 0x8)) {
			// emulate a device with 16-byte sound FIFO
			if(disney.state != DS_RUNNING) {
				disney.interface_det_ext++;
				disney.interface_det = 0;
				if(disney.interface_det_ext > 5) {
					disney.leader = &disney.da[0];
					DISNEY_enable(7000);
				}
			}
			if(disney.interface_det_ext > 5) {
				if(disney.da[0].used < DISNEY_SIZE) {
					disney.da[0].buffer[disney.da[0].used] = disney.data;
					disney.da[0].used++;
				}
			}
		}

//		LOG_WARN("DISNEY:Control write %x",val);
		if (val&0x10) LOG(LOG_MISC,LOG_ERROR)("DISNEY:Parallel IRQ Enabled");
		disney.control=(Bit8u)val;
		break;
	}
}

static Bitu disney_read(Bitu port,Bitu iolen) {
    (void)iolen;//UNUSED
	Bitu retval;
	switch (port-DISNEY_BASE) {
	case 0:		/* Data Port */
//		LOG(LOG_MISC,LOG_NORMAL)("DISNEY:Read from data port");
		return disney.data;
		break;
	case 1:		/* Status Port */	
//		LOG(LOG_MISC,"DISNEY:Read from status port %X",disney.status);
		retval = 0x07;//0x40; // Stereo-on-1 and (or) New-Stereo DACs present
		if(disney.interface_det_ext > 5) {
			if (disney.leader && disney.leader->used >= 16){
				retval |= 0x40u; // ack
				retval &= ~0x4u; // interrupt
			}
		}
		if(!(disney.data&0x80)) retval |= 0x80; // pin 9 is wired to pin 11
		return retval;
		break;
	case 2:		/* Control Port */
		LOG(LOG_MISC,LOG_NORMAL)("DISNEY:Read from control port");
		return disney.control;
		break;
	}
	return 0xff;
}

static void DISNEY_PlayStereo(Bitu len, Bit8u* l, Bit8u* r) {
	static Bit8u stereodata[DISNEY_SIZE*2];
	for(Bitu i = 0; i < len; i++) {
		stereodata[i*2] = l[i];
		stereodata[i*2+1] = r[i];
	}
	disney.chan->AddSamples_s8(len,stereodata);
}

static void DISNEY_CallBack(Bitu len) {
	if (!len) return;
	if (disney.leader == NULL) return;

	// get the smaller used
	Bitu real_used;
	if(disney.stereo) {
		real_used = disney.da[0].used;
		if(disney.da[1].used < real_used) real_used = disney.da[1].used;
	} else
		real_used = disney.leader->used;

	if (real_used >= len) { // enough data for now
		if(disney.stereo) DISNEY_PlayStereo(len, disney.da[0].buffer, disney.da[1].buffer);
		else disney.chan->AddSamples_m8(len,disney.leader->buffer);

		// put the rest back to start
		for(int i = 0; i < 2; i++) {
			// TODO for mono only one 
			memmove(disney.da[i].buffer,&disney.da[i].buffer[len],DISNEY_SIZE/*real_used*/-len);
			disney.da[i].used -= len;
		}
	// TODO: len > DISNEY
	} else { // not enough data
		if(disney.stereo) {
			Bit8u gapfiller0 = 128;
			Bit8u gapfiller1 = 128;
			if(real_used) {
				gapfiller0 = disney.da[0].buffer[real_used-1];
				gapfiller1 = disney.da[1].buffer[real_used-1];
			}

			memset(disney.da[0].buffer+real_used,
				gapfiller0,len-real_used);
			memset(disney.da[1].buffer+real_used,
				gapfiller1,len-real_used);

			DISNEY_PlayStereo(len, disney.da[0].buffer, disney.da[1].buffer);
			len -= real_used;

		} else { // mono
			Bit8u gapfiller = 128; //Keep the middle
			if(real_used) {
				// fix for some stupid game; it outputs 0 at the end of the stream
				// causing a click. So if we have at least two bytes availible in the
				// buffer and the last one is a 0 then ignore that.
				if(disney.leader->buffer[real_used-1]==0)
					real_used--;
			}
			// do it this way because AddSilence sounds like a gnawing mouse
			if(real_used)
				gapfiller = disney.leader->buffer[real_used-1];
			//LOG_MSG("gapfiller %x, fill len %d, realused %d",gapfiller,len-real_used,real_used);
			memset(disney.leader->buffer+real_used,	gapfiller, len-real_used);
			disney.chan->AddSamples_m8(len, disney.leader->buffer);
		}
		disney.da[0].used =0;
		disney.da[1].used =0;

		//LOG_MSG("disney underflow %d",len - real_used);
	}
	if (disney.last_used+100<PIC_Ticks) {
		// disable sound output
		PIC_AddEvent(DISNEY_disable,0.0001f);	// I think we shouldn't delete the 
												// mixer while we are inside it
	}
}

void CPU_Snap_Back_To_Real_Mode();
void CPU_Snap_Back_Restore();

class DISNEY: public Module_base {
private:
	IO_ReadHandleObject ReadHandler;
	IO_WriteHandleObject WriteHandler;
	//FIXME: this conflicts with MPU-401, is this really needed? //IO_WriteHandleObject WriteHandler_cvm;
	//MixerObject MixerChan;
public:
	DISNEY(Section* configuration):Module_base(configuration) {
		Section_prop * section=static_cast<Section_prop *>(configuration);
		if(!section->Get_bool("disney")) return;

		for(int i = 0; i < 2; i++) {
			disney.da[i].used = 0;
			disney.da[i].speedcheck_sum = 0;
			disney.da[i].speedcheck_failed = false;
			disney.da[i].speedcheck_init = false;
		}

		WriteHandler.Install(DISNEY_BASE,disney_write,IO_MB,3);
		ReadHandler.Install(DISNEY_BASE,disney_read,IO_MB,3);
		// see above //WriteHandler_cvm.Install(0x330,disney_write,IO_MB,1);
	
		disney.status=0x84;
		disney.control=0;
		disney.last_used=0;

		disney.mo = new MixerObject();
		disney.chan=disney.mo->Install(&DISNEY_CallBack,10000,"DISNEY");
		DISNEY_disable(0);
	}
	~DISNEY(){
		CPU_Snap_Back_To_Real_Mode();

		BIOS_SetLPTPort(0,0);
		DISNEY_disable(0);
		if (disney.mo)
			delete disney.mo;

		CPU_Snap_Back_Restore();
	}
};

static DISNEY* test = NULL;

static void DISNEY_ShutDown(Section* sec){
    (void)sec;//UNUSED
    if (test) {
        delete test;
        test = NULL;
    }
}

Bitu DISNEY_BasePort() {
	return DISNEY_BASE;
}

bool DISNEY_ShouldInit() {
	Section_prop *sec = (Section_prop*)control->GetSection("speaker");
	return sec->Get_bool("disney");
}

bool DISNEY_HasInit() {
	return (test != NULL);
}

// parallel port init code will call this depending on port allocation and config.
void DISNEY_Init(unsigned int base_port) {
	if (test == NULL) {
		DISNEY_BASE = base_port;
		LOG(LOG_MISC,LOG_DEBUG)("Allocating Disney Sound emulation on port %xh",DISNEY_BASE);
		test = new DISNEY(control->GetSection("speaker"));
	}
}

void DISNEY_Init() {
	LOG(LOG_MISC,LOG_DEBUG)("Initializing Disney Sound Source emulation");

	AddExitFunction(AddExitFunctionFuncPair(DISNEY_ShutDown),true);
}

// save state support
void *DISNEY_disable_PIC_Event = (void*)DISNEY_disable;

void POD_Save_Disney( std::ostream& stream )
{
	const char pod_name[32] = "Disney";

	if( stream.fail() ) return;
	if( !test ) return;
	if( !disney.chan ) return;

	WRITE_POD( &pod_name, pod_name );

	//************************************************
	//************************************************
	//************************************************

	Bit8u dac_leader_idx;


	dac_leader_idx = 0xff;
	for( int lcv=0; lcv<2; lcv++ ) {
		if( disney.leader == &disney.da[lcv] ) { dac_leader_idx = lcv; break; }
	}

	// *******************************************
	// *******************************************
	// *******************************************

	// - near-pure struct data
	WRITE_POD( &disney, disney );




	// - reloc ptr
	WRITE_POD( &dac_leader_idx, dac_leader_idx );

	//*******************************************
	//*******************************************
	//*******************************************

	disney.chan->SaveState(stream);
}

void POD_Load_Disney( std::istream& stream )
{
	char pod_name[32] = {0};

	if( stream.fail() ) return;
	if( !test ) return;
	if( !disney.chan ) return;

	// error checking
	READ_POD( &pod_name, pod_name );
	if( strcmp( pod_name, "Disney" ) ) {
		stream.clear( std::istream::failbit | std::istream::badbit );
		return;
	}

	//************************************************
	//************************************************
	//************************************************

	Bit8u dac_leader_idx;
	MixerObject *mo_old;
	MixerChannel *chan_old;


	// save old ptrs
	mo_old = disney.mo;
	chan_old = disney.chan;

	//*******************************************
	//*******************************************
	//*******************************************

	// - near-pure struct data
	READ_POD( &disney, disney );



	// - reloc ptr
	READ_POD( &dac_leader_idx, dac_leader_idx );

	//*******************************************
	//*******************************************
	//*******************************************

	disney.leader = NULL;
	if( dac_leader_idx != 0xff ) disney.leader = &disney.da[dac_leader_idx];

	//*******************************************
	//*******************************************
	//*******************************************

	// restore old ptrs
	disney.mo = mo_old;
	disney.chan = chan_old;


	disney.chan->LoadState(stream);
}
