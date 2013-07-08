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
 
 /* $Id: pcspeaker.cpp,v 1.26 2009-05-27 09:15:41 qbix79 Exp $ */

#include <math.h>
#include "dosbox.h"
#include "mixer.h"
#include "timer.h"
#include "setup.h"
#include "pic.h"


#ifndef PI
#define PI 3.14159265358979323846
#endif

#define SPKR_ENTRIES 1024
#define SPKR_VOLUME 5000
//#define SPKR_SHIFT 8
#define SPKR_SPEED (float)((SPKR_VOLUME*2)/0.070f)

enum SPKR_MODES {
	SPKR_OFF,SPKR_ON,SPKR_PIT_OFF,SPKR_PIT_ON
};

struct DelayEntry {
	float index;
	float vol;
};

static struct {
	MixerChannel * chan;
	SPKR_MODES mode;
	Bitu pit_mode;
	Bitu rate;

	float pit_last;
	float pit_new_max,pit_new_half;
	float pit_max,pit_half;
	float pit_index;
	float volwant,volcur;
	Bitu last_ticks;
	float last_index;
	Bitu min_tr;
	DelayEntry entries[SPKR_ENTRIES];
	Bitu used;
} spkr;

static void AddDelayEntry(float index,float vol) {
	if (spkr.used==SPKR_ENTRIES) {
		return;
	}
	spkr.entries[spkr.used].index=index;
	spkr.entries[spkr.used].vol=vol;
	spkr.used++;
}


static void ForwardPIT(float newindex) {
	float passed=(newindex-spkr.last_index);
	float delay_base=spkr.last_index;
	spkr.last_index=newindex;
	switch (spkr.pit_mode) {
	case 0:
		return;
	case 1:
		return;
	case 2:
		while (passed>0) {
			/* passed the initial low cycle? */
			if (spkr.pit_index>=spkr.pit_half) {
				/* Start a new low cycle */
				if ((spkr.pit_index+passed)>=spkr.pit_max) {
					float delay=spkr.pit_max-spkr.pit_index;
					delay_base+=delay;passed-=delay;
					spkr.pit_last=-SPKR_VOLUME;
					if (spkr.mode==SPKR_PIT_ON) AddDelayEntry(delay_base,spkr.pit_last);
					spkr.pit_index=0;
				} else {
					spkr.pit_index+=passed;
					return;
				}
			} else {
				if ((spkr.pit_index+passed)>=spkr.pit_half) {
					float delay=spkr.pit_half-spkr.pit_index;
					delay_base+=delay;passed-=delay;
					spkr.pit_last=SPKR_VOLUME;
					if (spkr.mode==SPKR_PIT_ON) AddDelayEntry(delay_base,spkr.pit_last);
					spkr.pit_index=spkr.pit_half;
				} else {
					spkr.pit_index+=passed;
					return;
				}
			}
		}
		break;
		//END CASE 2
	case 3:
		while (passed>0) {
			/* Determine where in the wave we're located */
			if (spkr.pit_index>=spkr.pit_half) {
				if ((spkr.pit_index+passed)>=spkr.pit_max) {
					float delay=spkr.pit_max-spkr.pit_index;
					delay_base+=delay;passed-=delay;
					spkr.pit_last=SPKR_VOLUME;
					if (spkr.mode==SPKR_PIT_ON) AddDelayEntry(delay_base,spkr.pit_last);
					spkr.pit_index=0;
					/* Load the new count */
					spkr.pit_half=spkr.pit_new_half;
					spkr.pit_max=spkr.pit_new_max;
				} else {
					spkr.pit_index+=passed;
					return;
				}
			} else {
				if ((spkr.pit_index+passed)>=spkr.pit_half) {
					float delay=spkr.pit_half-spkr.pit_index;
					delay_base+=delay;passed-=delay;
					spkr.pit_last=-SPKR_VOLUME;
					if (spkr.mode==SPKR_PIT_ON) AddDelayEntry(delay_base,spkr.pit_last);
					spkr.pit_index=spkr.pit_half;
					/* Load the new count */
					spkr.pit_half=spkr.pit_new_half;
					spkr.pit_max=spkr.pit_new_max;
				} else {
					spkr.pit_index+=passed;
					return;
				}
			}
		}
		break;
		//END CASE 3
	case 4:
		if (spkr.pit_index<spkr.pit_max) {
			/* Check if we're gonna pass the end this block */
			if (spkr.pit_index+passed>=spkr.pit_max) {
				float delay=spkr.pit_max-spkr.pit_index;
				delay_base+=delay;passed-=delay;
				spkr.pit_last=-SPKR_VOLUME;
				if (spkr.mode==SPKR_PIT_ON) AddDelayEntry(delay_base,spkr.pit_last);				//No new events unless reprogrammed
				spkr.pit_index=spkr.pit_max;
			} else spkr.pit_index+=passed;
		}
		break;
		//END CASE 4
	}
}

void PCSPEAKER_SetCounter(Bitu cntr,Bitu mode) {
	if (!spkr.last_ticks) {
		if(spkr.chan) spkr.chan->Enable(true);
		spkr.last_index=0;
	}
	spkr.last_ticks=PIC_Ticks;
	float newindex=PIC_TickIndex();
	ForwardPIT(newindex);
	switch (mode) {
	case 0:		/* Mode 0 one shot, used with realsound */
		if (spkr.mode!=SPKR_PIT_ON) return;
		if (cntr>80) { 
			cntr=80;
		}
		spkr.pit_last=((float)cntr-40)*(SPKR_VOLUME/40.0f);
		AddDelayEntry(newindex,spkr.pit_last);
		spkr.pit_index=0;
		break;
	case 1:
		if (spkr.mode!=SPKR_PIT_ON) return;
		spkr.pit_last=SPKR_VOLUME;
		AddDelayEntry(newindex,spkr.pit_last);
		break;
	case 2:			/* Single cycle low, rest low high generator */
		spkr.pit_index=0;
		spkr.pit_last=-SPKR_VOLUME;
		AddDelayEntry(newindex,spkr.pit_last);
		spkr.pit_half=(1000.0f/PIT_TICK_RATE)*1;
		spkr.pit_max=(1000.0f/PIT_TICK_RATE)*cntr;
		break;
	case 3:		/* Square wave generator */
		if (cntr<spkr.min_tr) {
			/* skip frequencies that can't be represented */
			spkr.pit_last=0;
			spkr.pit_mode=0;
			return;
		}
		spkr.pit_new_max=(1000.0f/PIT_TICK_RATE)*cntr;
		spkr.pit_new_half=spkr.pit_new_max/2;
		break;
	case 4:		/* Software triggered strobe */
		spkr.pit_last=SPKR_VOLUME;
		AddDelayEntry(newindex,spkr.pit_last);
		spkr.pit_index=0;
		spkr.pit_max=(1000.0f/PIT_TICK_RATE)*cntr;
		break;
	default:
#if C_DEBUG
		LOG_MSG("Unhandled speaker mode %d",mode);
#endif
		return;
	}
	spkr.pit_mode=mode;
}

void PCSPEAKER_SetType(Bitu mode) {
	if (!spkr.last_ticks) {
		if(spkr.chan) spkr.chan->Enable(true);
		spkr.last_index=0;
	}
	spkr.last_ticks=PIC_Ticks;
	float newindex=PIC_TickIndex();
	ForwardPIT(newindex);
	switch (mode) {
	case 0:
		spkr.mode=SPKR_OFF;
		AddDelayEntry(newindex,-SPKR_VOLUME);
		break;
	case 1:
		spkr.mode=SPKR_PIT_OFF;
		AddDelayEntry(newindex,-SPKR_VOLUME);
		break;
	case 2:
		spkr.mode=SPKR_ON;
		AddDelayEntry(newindex,SPKR_VOLUME);
		break;
	case 3:
		if (spkr.mode!=SPKR_PIT_ON) {
			AddDelayEntry(newindex,spkr.pit_last);
		}
		spkr.mode=SPKR_PIT_ON;
		break;
	};
}

static void PCSPEAKER_CallBack(Bitu len) {
	Bit16s * stream=(Bit16s*)MixTemp;
	ForwardPIT(1);
	spkr.last_index=0;
	Bitu count=len;
	Bitu pos=0;
	float sample_base=0;
	float sample_add=(1.0001f)/len;
	while (count--) {
		float index=sample_base;
		sample_base+=sample_add;
		float end=sample_base;
		double value=0;
		while(index<end) {
			/* Check if there is an upcoming event */
			if (spkr.used && spkr.entries[pos].index<=index) {
				spkr.volwant=spkr.entries[pos].vol;
				pos++;spkr.used--;
				continue;
			}
			float vol_end;
			if (spkr.used && spkr.entries[pos].index<end) {
				vol_end=spkr.entries[pos].index;
			} else vol_end=end;
			float vol_len=vol_end-index;
            /* Check if we have to slide the volume */
			float vol_diff=spkr.volwant-spkr.volcur;
			if (vol_diff == 0) {
				value+=spkr.volcur*vol_len;
				index+=vol_len;
			} else {
				/* Check how long it will take to goto new level */
				float vol_time=fabs(vol_diff)/SPKR_SPEED;
				if (vol_time<=vol_len) {
					/* Volume reaches endpoint in this block, calc until that point */
					value+=vol_time*spkr.volcur;
					value+=vol_time*vol_diff/2;
					index+=vol_time;
					spkr.volcur=spkr.volwant;
				} else {
					/* Volume still not reached in this block */
					value+=spkr.volcur*vol_len;
					if (vol_diff<0) {
						value-=(SPKR_SPEED*vol_len*vol_len)/2;
						spkr.volcur-=SPKR_SPEED*vol_len;
					} else {
						value+=(SPKR_SPEED*vol_len*vol_len)/2;
						spkr.volcur+=SPKR_SPEED*vol_len;
					}
					index+=vol_len;
				}
			}
		}
		*stream++=(Bit16s)(value/sample_add);
	}
	if(spkr.chan) spkr.chan->AddSamples_m16(len,(Bit16s*)MixTemp);

	//Turn off speaker after 10 seconds of idle or one second idle when in off mode
	bool turnoff = false;
	Bitu test_ticks = PIC_Ticks;
	if ((spkr.last_ticks + 10000) < test_ticks) turnoff = true;
	if((spkr.mode == SPKR_OFF) && ((spkr.last_ticks + 1000) < test_ticks)) turnoff = true;

	if(turnoff){
		if(spkr.volwant == 0) { 
			spkr.last_ticks = 0;
			if(spkr.chan) spkr.chan->Enable(false);
		} else {
			if(spkr.volwant > 0) spkr.volwant--; else spkr.volwant++;
		
		}
	} 

}
class PCSPEAKER:public Module_base {
private:
	MixerObject MixerChan;
public:
	PCSPEAKER(Section* configuration):Module_base(configuration){
		spkr.chan=0;
		Section_prop * section=static_cast<Section_prop *>(configuration);
		if(!section->Get_bool("pcspeaker")) return;
		spkr.mode=SPKR_OFF;
		spkr.last_ticks=0;
		spkr.last_index=0;
		spkr.rate=section->Get_int("pcrate");
		spkr.pit_max=(1000.0f/PIT_TICK_RATE)*65535;
		spkr.pit_half=spkr.pit_max/2;
		spkr.pit_new_max=spkr.pit_max;
		spkr.pit_new_half=spkr.pit_half;
		spkr.pit_index=0;
		spkr.min_tr=(PIT_TICK_RATE+spkr.rate/2-1)/(spkr.rate/2);
		spkr.used=0;
		/* Register the sound channel */
		spkr.chan=MixerChan.Install(&PCSPEAKER_CallBack,spkr.rate,"SPKR");
	}
	~PCSPEAKER(){
		Section_prop * section=static_cast<Section_prop *>(m_configuration);
		if(!section->Get_bool("pcspeaker")) return;
	}
};
static PCSPEAKER* test;

void PCSPEAKER_ShutDown(Section* sec){
	delete test;
}

void PCSPEAKER_Init(Section* sec) {
	test = new PCSPEAKER(sec);
	sec->AddDestroyFunction(&PCSPEAKER_ShutDown,true);
}
