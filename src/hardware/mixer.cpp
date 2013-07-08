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
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/* $Id: mixer.cpp,v 1.54 2009-09-05 11:10:04 qbix79 Exp $ */

/* 
	Remove the sdl code from here and have it handeld in the sdlmain.
	That should call the mixer start from there or something.
*/

#include <string.h>
#include <sys/types.h>
#include <math.h>

#if defined (WIN32)
//Midi listing
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmsystem.h>
#endif

#include "SDL.h"
#include "mem.h"
#include "pic.h"
#include "dosbox.h"
#include "mixer.h"
#include "timer.h"
#include "setup.h"
#include "cross.h"
#include "support.h"
#include "mapper.h"
#include "hardware.h"
#include "programs.h"

#define MIXER_SSIZE 4
#define MIXER_SHIFT 14
#define MIXER_REMAIN ((1<<MIXER_SHIFT)-1)
#define MIXER_VOLSHIFT 13

static INLINE Bit16s MIXER_CLIP(Bits SAMP) {
	if (SAMP < MAX_AUDIO) {
		if (SAMP > MIN_AUDIO)
			return SAMP;
		else return MIN_AUDIO;
	} else return MAX_AUDIO;
}

static struct {
	Bit32s work[MIXER_BUFSIZE][2];
	Bitu pos,done;
	Bitu needed, min_needed, max_needed;
	Bit32u tick_add,tick_remain;
	float mastervol[2];
	MixerChannel * channels;
	bool nosound;
	Bit32u freq;
	Bit32u blocksize;
} mixer;

Bit8u MixTemp[MIXER_BUFSIZE];

MixerChannel * MIXER_AddChannel(MIXER_Handler handler,Bitu freq,const char * name) {
	MixerChannel * chan=new MixerChannel();
	chan->scale = 1.0;
	chan->handler=handler;
	chan->name=name;
	chan->SetFreq(freq);
	chan->next=mixer.channels;
	chan->SetVolume(1,1);
	chan->enabled=false;
	mixer.channels=chan;
	return chan;
}

MixerChannel * MIXER_FindChannel(const char * name) {
	MixerChannel * chan=mixer.channels;
	while (chan) {
		if (!strcasecmp(chan->name,name)) break;
		chan=chan->next;
	}
	return chan;
}

void MIXER_DelChannel(MixerChannel* delchan) {
	MixerChannel * chan=mixer.channels;
	MixerChannel * * where=&mixer.channels;
	while (chan) {
		if (chan==delchan) {
			*where=chan->next;
			delete delchan;
			return;
		}
		where=&chan->next;
		chan=chan->next;
	}
}

void MixerChannel::UpdateVolume(void) {
	volmul[0]=(Bits)((1 << MIXER_VOLSHIFT)*scale*volmain[0]*mixer.mastervol[0]);
	volmul[1]=(Bits)((1 << MIXER_VOLSHIFT)*scale*volmain[1]*mixer.mastervol[1]);
}

void MixerChannel::SetVolume(float _left,float _right) {
	volmain[0]=_left;
	volmain[1]=_right;
	UpdateVolume();
}

void MixerChannel::SetScale( float f ) {
	scale = f;
	UpdateVolume();
}

void MixerChannel::Enable(bool _yesno) {
	if (_yesno==enabled) return;
	enabled=_yesno;
	if (enabled) {
		freq_index=MIXER_REMAIN;
		SDL_LockAudio();
		if (done<mixer.done) done=mixer.done;
		SDL_UnlockAudio();
	}
}

void MixerChannel::SetFreq(Bitu _freq) {
	freq_add=(_freq<<MIXER_SHIFT)/mixer.freq;
}

void MixerChannel::Mix(Bitu _needed) {
	needed=_needed;
	while (enabled && needed>done) {
		Bitu todo=needed-done;
		todo *= freq_add;
		todo  = (todo >> MIXER_SHIFT) + ((todo & MIXER_REMAIN)!=0);
		handler(todo);
	}
}

void MixerChannel::AddSilence(void) {
	if (done<needed) {
		done=needed;
		last[0]=last[1]=0;
		freq_index=MIXER_REMAIN;
	}
}

template<class Type,bool stereo,bool signeddata,bool nativeorder>
inline void MixerChannel::AddSamples(Bitu len, const Type* data) {
	Bits diff[2];
	Bitu mixpos=mixer.pos+done;
	freq_index&=MIXER_REMAIN;
	Bitu pos=0;Bitu new_pos;

	goto thestart;
	for (;;) {
		new_pos=freq_index >> MIXER_SHIFT;
		if (pos<new_pos) {
			last[0]+=diff[0];
			if (stereo) last[1]+=diff[1];
			pos=new_pos;
thestart:
			if (pos>=len) return;
			if ( sizeof( Type) == 1) {
				if (!signeddata) {
					if (stereo) {
						diff[0]=(((Bit8s)(data[pos*2+0] ^ 0x80)) << 8)-last[0];
						diff[1]=(((Bit8s)(data[pos*2+1] ^ 0x80)) << 8)-last[1];
					} else {
						diff[0]=(((Bit8s)(data[pos] ^ 0x80)) << 8)-last[0];
					}
				} else {
					if (stereo) {
						diff[0]=(data[pos*2+0] << 8)-last[0];
						diff[1]=(data[pos*2+1] << 8)-last[1];
					} else {
						diff[0]=(data[pos] << 8)-last[0];
					}
				}
			//16bit and 32bit both contain 16bit data internally
			} else  {
				if (signeddata) {
					if (stereo) {
						if (nativeorder) {
							diff[0]=data[pos*2+0]-last[0];
							diff[1]=data[pos*2+1]-last[1];
						} else {
							if ( sizeof( Type) == 2) {
								diff[0]=(Bit16s)host_readw((HostPt)&data[pos*2+0])-last[0];
								diff[1]=(Bit16s)host_readw((HostPt)&data[pos*2+1])-last[1];
							} else {
								diff[0]=(Bit32s)host_readd((HostPt)&data[pos*2+0])-last[0];
								diff[1]=(Bit32s)host_readd((HostPt)&data[pos*2+1])-last[1];
							}
						}
					} else {
						if (nativeorder) {
							diff[0]=data[pos]-last[0];
						} else {
							if ( sizeof( Type) == 2) {
								diff[0]=(Bit16s)host_readw((HostPt)&data[pos])-last[0];
							} else {
								diff[0]=(Bit32s)host_readd((HostPt)&data[pos])-last[0];
							}
						}
					}
				} else {
					if (stereo) {
						if (nativeorder) {
							diff[0]=(Bits)data[pos*2+0]-32768-last[0];
							diff[1]=(Bits)data[pos*2+1]-32768-last[1];
						} else {
							if ( sizeof( Type) == 2) {
								diff[0]=(Bits)host_readw((HostPt)&data[pos*2+0])-32768-last[0];
								diff[1]=(Bits)host_readw((HostPt)&data[pos*2+1])-32768-last[1];
							} else {
								diff[0]=(Bits)host_readd((HostPt)&data[pos*2+0])-32768-last[0];
								diff[1]=(Bits)host_readd((HostPt)&data[pos*2+1])-32768-last[1];
							}
						}
					} else {
						if (nativeorder) {
							diff[0]=(Bits)data[pos]-32768-last[0];
						} else {
							if ( sizeof( Type) == 2) {
								diff[0]=(Bits)host_readw((HostPt)&data[pos])-32768-last[0];
							} else {
								diff[0]=(Bits)host_readd((HostPt)&data[pos])-32768-last[0];
							}
						}
					}
				}
			}
		}
		Bits diff_mul=freq_index & MIXER_REMAIN;
		freq_index+=freq_add;
		mixpos&=MIXER_BUFMASK;
		Bits sample=last[0]+((diff[0]*diff_mul) >> MIXER_SHIFT);
		mixer.work[mixpos][0]+=sample*volmul[0];
		if (stereo) sample=last[1]+((diff[1]*diff_mul) >> MIXER_SHIFT);
		mixer.work[mixpos][1]+=sample*volmul[1];
		mixpos++;done++;
	}
}

void MixerChannel::AddStretched(Bitu len,Bit16s * data) {
	if (done>=needed) {
		LOG_MSG("Can't add, buffer full");	
		return;
	}
	Bitu outlen=needed-done;Bits diff;
	freq_index=0;
	Bitu temp_add=(len << MIXER_SHIFT)/outlen;
	Bitu mixpos=mixer.pos+done;done=needed;
	Bitu pos=0;
	diff=data[0]-last[0];
	while (outlen--) {
		Bitu new_pos=freq_index >> MIXER_SHIFT;
		if (pos<new_pos) {
			pos=new_pos;
			last[0]+=diff;
			diff=data[pos]-last[0];
		}
		Bits diff_mul=freq_index & MIXER_REMAIN;
		freq_index+=temp_add;
		mixpos&=MIXER_BUFMASK;
		Bits sample=last[0]+((diff*diff_mul) >> MIXER_SHIFT);
		mixer.work[mixpos][0]+=sample*volmul[0];
		mixer.work[mixpos][1]+=sample*volmul[1];
		mixpos++;
	}
}

void MixerChannel::AddStretchedStereo(Bitu len/*combined L+R samples*/,Bit16s * data) {
	if (done>=needed) {
		LOG_MSG("Can't add, buffer full");	
		return;
	}
	Bitu outlen=needed-done;Bits diff,diff2;
	freq_index=0;
	Bitu temp_add=(len << MIXER_SHIFT)/outlen;
	Bitu mixpos=mixer.pos+done;done=needed;
	Bitu pos=0;
	diff=data[0]-last[0];
	diff2=data[1]-last[1];
	while (outlen--) {
		Bitu new_pos=freq_index >> MIXER_SHIFT;
		if (pos<new_pos) {
			pos=new_pos;
			last[0]+=diff;
			last[1]+=diff2;
			diff=data[pos*2]-last[0];
			diff2=data[pos*2+1]-last[1];
		}
		Bits diff_mul=freq_index & MIXER_REMAIN;
		freq_index+=temp_add;
		mixpos&=MIXER_BUFMASK;
		Bits sample;
		sample=last[0]+((diff*diff_mul) >> MIXER_SHIFT);
		mixer.work[mixpos][0]+=sample*volmul[0];
		sample=last[1]+((diff2*diff_mul) >> MIXER_SHIFT);
		mixer.work[mixpos][1]+=sample*volmul[1];
		mixpos++;
	}
}

void MixerChannel::AddSamples_m8(Bitu len, const Bit8u * data) {
	AddSamples<Bit8u,false,false,true>(len,data);
}
void MixerChannel::AddSamples_s8(Bitu len,const Bit8u * data) {
	AddSamples<Bit8u,true,false,true>(len,data);
}
void MixerChannel::AddSamples_m8s(Bitu len,const Bit8s * data) {
	AddSamples<Bit8s,false,true,true>(len,data);
}
void MixerChannel::AddSamples_s8s(Bitu len,const Bit8s * data) {
	AddSamples<Bit8s,true,true,true>(len,data);
}
void MixerChannel::AddSamples_m16(Bitu len,const Bit16s * data) {
	AddSamples<Bit16s,false,true,true>(len,data);
}
void MixerChannel::AddSamples_s16(Bitu len,const Bit16s * data) {
	AddSamples<Bit16s,true,true,true>(len,data);
}
void MixerChannel::AddSamples_m16u(Bitu len,const Bit16u * data) {
	AddSamples<Bit16u,false,false,true>(len,data);
}
void MixerChannel::AddSamples_s16u(Bitu len,const Bit16u * data) {
	AddSamples<Bit16u,true,false,true>(len,data);
}
void MixerChannel::AddSamples_m32(Bitu len,const Bit32s * data) {
	AddSamples<Bit32s,false,true,true>(len,data);
}
void MixerChannel::AddSamples_s32(Bitu len,const Bit32s * data) {
	AddSamples<Bit32s,true,true,true>(len,data);
}
void MixerChannel::AddSamples_m16_nonnative(Bitu len,const Bit16s * data) {
	AddSamples<Bit16s,false,true,false>(len,data);
}
void MixerChannel::AddSamples_s16_nonnative(Bitu len,const Bit16s * data) {
	AddSamples<Bit16s,true,true,false>(len,data);
}
void MixerChannel::AddSamples_m16u_nonnative(Bitu len,const Bit16u * data) {
	AddSamples<Bit16u,false,false,false>(len,data);
}
void MixerChannel::AddSamples_s16u_nonnative(Bitu len,const Bit16u * data) {
	AddSamples<Bit16u,true,false,false>(len,data);
}
void MixerChannel::AddSamples_m32_nonnative(Bitu len,const Bit32s * data) {
	AddSamples<Bit32s,false,true,false>(len,data);
}
void MixerChannel::AddSamples_s32_nonnative(Bitu len,const Bit32s * data) {
	AddSamples<Bit32s,true,true,false>(len,data);
}

void MixerChannel::FillUp(void) {
	SDL_LockAudio();
	if (!enabled || done<mixer.done) {
		SDL_UnlockAudio();
		return;
	}
	float index=PIC_TickIndex();
	Mix((Bitu)(index*mixer.needed));
	SDL_UnlockAudio();
}

extern bool ticksLocked;
static inline bool Mixer_irq_important(void) {
	/* In some states correct timing of the irqs is more important then 
	 * non stuttering audo */
	return (ticksLocked || (CaptureState & (CAPTURE_WAVE|CAPTURE_VIDEO)));
}

/* Mix a certain amount of new samples */
static void MIXER_MixData(Bitu needed) {
	MixerChannel * chan=mixer.channels;
	while (chan) {
		chan->Mix(needed);
		chan=chan->next;
	}
	if (CaptureState & (CAPTURE_WAVE|CAPTURE_VIDEO)) {
		Bit16s convert[1024][2];
		Bitu added=needed-mixer.done;
		if (added>1024) 
			added=1024;
		Bitu readpos=(mixer.pos+mixer.done)&MIXER_BUFMASK;
		for (Bitu i=0;i<added;i++) {
			Bits sample=mixer.work[readpos][0] >> MIXER_VOLSHIFT;
			convert[i][0]=MIXER_CLIP(sample);
			sample=mixer.work[readpos][1] >> MIXER_VOLSHIFT;
			convert[i][1]=MIXER_CLIP(sample);
			readpos=(readpos+1)&MIXER_BUFMASK;
		}
		CAPTURE_AddWave( mixer.freq, added, (Bit16s*)convert );
	}
	//Reset the the tick_add for constant speed
	if( Mixer_irq_important() )
		mixer.tick_add = ((mixer.freq) << MIXER_SHIFT)/1000;
	mixer.done = needed;
}

static void MIXER_Mix(void) {
	SDL_LockAudio();
	MIXER_MixData(mixer.needed);
	mixer.tick_remain+=mixer.tick_add;
	mixer.needed+=(mixer.tick_remain>>MIXER_SHIFT);
	mixer.tick_remain&=MIXER_REMAIN;
	SDL_UnlockAudio();
}

static void MIXER_Mix_NoSound(void) {
	MIXER_MixData(mixer.needed);
	/* Clear piece we've just generated */
	for (Bitu i=0;i<mixer.needed;i++) {
		mixer.work[mixer.pos][0]=0;
		mixer.work[mixer.pos][1]=0;
		mixer.pos=(mixer.pos+1)&MIXER_BUFMASK;
	}
	/* Reduce count in channels */
	for (MixerChannel * chan=mixer.channels;chan;chan=chan->next) {
		if (chan->done>mixer.needed) chan->done-=mixer.needed;
		else chan->done=0;
	}
	/* Set values for next tick */
	mixer.tick_remain+=mixer.tick_add;
	mixer.needed=mixer.tick_remain>>MIXER_SHIFT;
	mixer.tick_remain&=MIXER_REMAIN;
	mixer.done=0;
}

static void MIXER_CallBack(void * userdata, Uint8 *stream, int len) {
	Bitu need=(Bitu)len/MIXER_SSIZE;
	Bit16s * output=(Bit16s *)stream;
	Bitu reduce;
	Bitu pos, index, index_add;
	Bits sample;
	/* Enough room in the buffer ? */
	if (mixer.done < need) {
//		LOG_MSG("Full underrun need %d, have %d, min %d", need, mixer.done, mixer.min_needed);
		if((need - mixer.done) > (need >>7) ) //Max 1 procent stretch.
			return;
		reduce = mixer.done;
		index_add = (reduce << MIXER_SHIFT) / need;
		mixer.tick_add = ((mixer.freq+mixer.min_needed) << MIXER_SHIFT)/1000;
	} else if (mixer.done < mixer.max_needed) {
		Bitu left = mixer.done - need;
		if (left < mixer.min_needed) {
			if( !Mixer_irq_important() ) {
				Bitu needed = mixer.needed - need;
				Bitu diff = (mixer.min_needed>needed?mixer.min_needed:needed) - left;
				mixer.tick_add = ((mixer.freq+(diff*3)) << MIXER_SHIFT)/1000;
				left = 0; //No stretching as we compensate with the tick_add value
			} else {
				left = (mixer.min_needed - left);
				left = 1 + (2*left) / mixer.min_needed; //left=1,2,3
			}
//			LOG_MSG("needed underrun need %d, have %d, min %d, left %d", need, mixer.done, mixer.min_needed, left);
			reduce = need - left;
			index_add = (reduce << MIXER_SHIFT) / need;
		} else {
			reduce = need;
			index_add = (1 << MIXER_SHIFT);
//			LOG_MSG("regular run need %d, have %d, min %d, left %d", need, mixer.done, mixer.min_needed, left);

			/* Mixer tick value being updated:
			 * 3 cases:
			 * 1) A lot too high. >division by 5. but maxed by 2* min to prevent too fast drops.
			 * 2) A little too high > division by 8
			 * 3) A little to nothing above the min_needed buffer > go to default value
			 */
			Bitu diff = left - mixer.min_needed;
			if(diff > (mixer.min_needed<<1)) diff = mixer.min_needed<<1;
			if(diff > (mixer.min_needed>>1))
				mixer.tick_add = ((mixer.freq-(diff/5)) << MIXER_SHIFT)/1000;
			else if (diff > (mixer.min_needed>>4))
				mixer.tick_add = ((mixer.freq-(diff>>3)) << MIXER_SHIFT)/1000;
			else
				mixer.tick_add = (mixer.freq<< MIXER_SHIFT)/1000;
		}
	} else {
		/* There is way too much data in the buffer */
//		LOG_MSG("overflow run need %d, have %d, min %d", need, mixer.done, mixer.min_needed);
		if (mixer.done > MIXER_BUFSIZE)
			index_add = MIXER_BUFSIZE - 2*mixer.min_needed;
		else 
			index_add = mixer.done - 2*mixer.min_needed;
		index_add = (index_add << MIXER_SHIFT) / need;
		reduce = mixer.done - 2* mixer.min_needed;
		mixer.tick_add = ((mixer.freq-(mixer.min_needed/5)) << MIXER_SHIFT)/1000;
	}
	/* Reduce done count in all channels */
	for (MixerChannel * chan=mixer.channels;chan;chan=chan->next) {
		if (chan->done>reduce) chan->done-=reduce;
		else chan->done=0;
	}
   
	// Reset mixer.tick_add when irqs are important
	if( Mixer_irq_important() )
		mixer.tick_add=(mixer.freq<< MIXER_SHIFT)/1000;

	mixer.done -= reduce;
	mixer.needed -= reduce;
	pos = mixer.pos;
	mixer.pos = (mixer.pos + reduce) & MIXER_BUFMASK;
	index = 0;
	if(need != reduce) {
		while (need--) {
			Bitu i = (pos + (index >> MIXER_SHIFT )) & MIXER_BUFMASK;
			index += index_add;
			sample=mixer.work[i][0]>>MIXER_VOLSHIFT;
			*output++=MIXER_CLIP(sample);
			sample=mixer.work[i][1]>>MIXER_VOLSHIFT;
			*output++=MIXER_CLIP(sample);
		}
		/* Clean the used buffer */
		while (reduce--) {
			pos &= MIXER_BUFMASK;
			mixer.work[pos][0]=0;
			mixer.work[pos][1]=0;
			pos++;
		}
	} else {
		while (reduce--) {
			pos &= MIXER_BUFMASK;
			sample=mixer.work[pos][0]>>MIXER_VOLSHIFT;
			*output++=MIXER_CLIP(sample);
			sample=mixer.work[pos][1]>>MIXER_VOLSHIFT;
			*output++=MIXER_CLIP(sample);
			mixer.work[pos][0]=0;
			mixer.work[pos][1]=0;
			pos++;
		}
	}
}

static void MIXER_Stop(Section* sec) {
}

class MIXER : public Program {
public:
	void MakeVolume(char * scan,float & vol0,float & vol1) {
		Bitu w=0;
		bool db=(toupper(*scan)=='D');
		if (db) scan++;
		while (*scan) {
			if (*scan==':') {
				++scan;w=1;
			}
			char * before=scan;
			float val=(float)strtod(scan,&scan);
			if (before==scan) {
				++scan;continue;
			}
			if (!db) val/=100;
			else val=powf(10.0f,(float)val/20.0f);
			if (val<0) val=1.0f;
			if (!w) {
				vol0=val;
			} else {
				vol1=val;
			}
		}
		if (!w) vol1=vol0;
	}

	void Run(void) {
		if(cmd->FindExist("/LISTMIDI")) {
			ListMidi();
			return;
		}
		if (cmd->FindString("MASTER",temp_line,false)) {
			MakeVolume((char *)temp_line.c_str(),mixer.mastervol[0],mixer.mastervol[1]);
		}
		MixerChannel * chan=mixer.channels;
		while (chan) {
			if (cmd->FindString(chan->name,temp_line,false)) {
				MakeVolume((char *)temp_line.c_str(),chan->volmain[0],chan->volmain[1]);
			}
			chan->UpdateVolume();
			chan=chan->next;
		}
		if (cmd->FindExist("/NOSHOW")) return;
		chan=mixer.channels;
		WriteOut("Channel  Main    Main(dB)\n");
		ShowVolume("MASTER",mixer.mastervol[0],mixer.mastervol[1]);
		for (chan=mixer.channels;chan;chan=chan->next) 
			ShowVolume(chan->name,chan->volmain[0],chan->volmain[1]);
	}
private:
	void ShowVolume(const char * name,float vol0,float vol1) {
		WriteOut("%-8s %3.0f:%-3.0f  %+3.2f:%-+3.2f \n",name,
			vol0*100,vol1*100,
			20*log(vol0)/log(10.0f),20*log(vol1)/log(10.0f)
		);
	}

	void ListMidi(){
#if defined (WIN32)
		unsigned int total = midiOutGetNumDevs();	
		for(unsigned int i=0;i<total;i++) {
			MIDIOUTCAPS mididev;
			midiOutGetDevCaps(i, &mididev, sizeof(MIDIOUTCAPS));
			WriteOut("%2d\t \"%s\"\n",i,mididev.szPname);
		}
#endif
	return;
	};

};

static void MIXER_ProgramStart(Program * * make) {
	*make=new MIXER;
}

MixerChannel* MixerObject::Install(MIXER_Handler handler,Bitu freq,const char * name){
	if(!installed) {
		if(strlen(name) > 31) E_Exit("Too long mixer channel name");
		safe_strncpy(m_name,name,32);
		installed = true;
		return MIXER_AddChannel(handler,freq,name);
	} else {
		E_Exit("allready added mixer channel.");
		return 0; //Compiler happy
	}
}

MixerObject::~MixerObject(){
	if(!installed) return;
	MIXER_DelChannel(MIXER_FindChannel(m_name));
}


void MIXER_Init(Section* sec) {
	sec->AddDestroyFunction(&MIXER_Stop);

	Section_prop * section=static_cast<Section_prop *>(sec);
	/* Read out config section */
	mixer.freq=section->Get_int("rate");
	mixer.nosound=section->Get_bool("nosound");
	mixer.blocksize=section->Get_int("blocksize");

	/* Initialize the internal stuff */
	mixer.channels=0;
	mixer.pos=0;
	mixer.done=0;
	memset(mixer.work,0,sizeof(mixer.work));
	mixer.mastervol[0]=1.0f;
	mixer.mastervol[1]=1.0f;

	/* Start the Mixer using SDL Sound at 22 khz */
	SDL_AudioSpec spec;
	SDL_AudioSpec obtained;

	spec.freq=mixer.freq;
	spec.format=AUDIO_S16SYS;
	spec.channels=2;
	spec.callback=MIXER_CallBack;
	spec.userdata=NULL;
	spec.samples=(Uint16)mixer.blocksize;

	mixer.tick_remain=0;
	if (mixer.nosound) {
		LOG_MSG("MIXER:No Sound Mode Selected.");
		mixer.tick_add=((mixer.freq) << MIXER_SHIFT)/1000;
		TIMER_AddTickHandler(MIXER_Mix_NoSound);
	} else if (SDL_OpenAudio(&spec, &obtained) <0 ) {
		mixer.nosound = true;
		LOG_MSG("MIXER:Can't open audio: %s , running in nosound mode.",SDL_GetError());
		mixer.tick_add=((mixer.freq) << MIXER_SHIFT)/1000;
		TIMER_AddTickHandler(MIXER_Mix_NoSound);
	} else {
		if((mixer.freq != obtained.freq) || (mixer.blocksize != obtained.samples))
			LOG_MSG("MIXER:Got different values from SDL: freq %d, blocksize %d",obtained.freq,obtained.samples);
		mixer.freq=obtained.freq;
		mixer.blocksize=obtained.samples;
		mixer.tick_add=(mixer.freq << MIXER_SHIFT)/1000;
		TIMER_AddTickHandler(MIXER_Mix);
		SDL_PauseAudio(0);
	}
	mixer.min_needed=section->Get_int("prebuffer");
	if (mixer.min_needed>100) mixer.min_needed=100;
	mixer.min_needed=(mixer.freq*mixer.min_needed)/1000;
	mixer.max_needed=mixer.blocksize * 2 + 2*mixer.min_needed;
	mixer.needed=mixer.min_needed+1;
	PROGRAMS_MakeFile("MIXER.COM",MIXER_ProgramStart);
}
