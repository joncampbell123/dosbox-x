/*
 *  Copyright (C) 2002-2020  The DOSBox Team
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
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */


#ifndef DOSBOX_MIXER_H
#define DOSBOX_MIXER_H

#include <sstream>

#ifndef DOSBOX_DOSBOX_H
#include "dosbox.h"
#endif

typedef void (*MIXER_MixHandler)(uint8_t * sampdate,Bit32u len);
typedef void (*MIXER_Handler)(Bitu len);

template <class T> T clamp(const T& n, const T& lower, const T& upper) {
	return std::max<T>(lower, std::min<T>(n, upper));
}

#define MIXER_BUFSIZE (16*1024)
#define MIXER_BUFMASK (MIXER_BUFSIZE-1)
extern uint8_t MixTemp[MIXER_BUFSIZE];

#define MAX_AUDIO ((1<<(16-1))-1)
#define MIN_AUDIO -(1<<(16-1))

#define LOWPASS_ORDER 8

class MixerChannel {
public:
	void SetVolume(float _left,float _right);
	void SetScale( float f );
	void SetScale(float _left, float _right);
	void UpdateVolume(void);
	void SetLowpassFreq(Bitu _freq,unsigned int order=2); // _freq / 1 Hz. call with _freq == 0 to disable
	void SetSlewFreq(Bitu _freq); // denominator provided by call to SetFreq. call with _freq == 0 to disable
	void SetFreq(Bitu _freq,Bitu _den=1U);
	void Mix(Bitu whole,Bitu frac);
	void AddSilence(void);			//Fill up until needed
	void EndFrame(Bitu samples);

	void lowpassUpdate();
	Bit32s lowpassStep(Bit32s in,const unsigned int iteration,const unsigned int channel);
	void lowpassProc(Bit32s ch[2]);

	template<class Type,bool stereo,bool signeddata,bool nativeorder,bool lowpass>
	void loadCurrentSample(Bitu &len, const Type* &data);

	template<class Type,bool stereo,bool signeddata,bool nativeorder>
	void AddSamples(Bitu len, const Type* data);
	double timeSinceLastSample(void);

	bool runSampleInterpolation(const Bitu upto);

	void updateSlew(void);
	void padFillSampleInterpolation(const Bitu upto);
	void finishSampleInterpolation(const Bitu upto);
	void AddSamples_m8(Bitu len, const uint8_t * data);
	void AddSamples_s8(Bitu len, const uint8_t * data);
	void AddSamples_m8s(Bitu len, const int8_t * data);
	void AddSamples_s8s(Bitu len, const int8_t * data);
	void AddSamples_m16(Bitu len, const Bit16s * data);
	void AddSamples_s16(Bitu len, const Bit16s * data);
	void AddSamples_m16u(Bitu len, const Bit16u * data);
	void AddSamples_s16u(Bitu len, const Bit16u * data);
	void AddSamples_m32(Bitu len, const Bit32s * data);
	void AddSamples_s32(Bitu len, const Bit32s * data);
	void AddSamples_m16_nonnative(Bitu len, const Bit16s * data);
	void AddSamples_s16_nonnative(Bitu len, const Bit16s * data);
	void AddSamples_m16u_nonnative(Bitu len, const Bit16u * data);
	void AddSamples_s16u_nonnative(Bitu len, const Bit16u * data);
	void AddSamples_m32_nonnative(Bitu len, const Bit32s * data);
	void AddSamples_s32_nonnative(Bitu len, const Bit32s * data);

	void FillUp(void);
	void Enable(bool _yesno);
	void SaveState( std::ostream& stream );
	void LoadState( std::istream& stream );

	MIXER_Handler handler;
	float volmain[2];
	float scale[2];
	Bit32s volmul[2];
	Bit32s lowpass[LOWPASS_ORDER][2];	// lowpass filter
	Bit32s lowpass_alpha;			// "alpha" multiplier for lowpass (16.16 fixed point)
	Bitu lowpass_freq;
	unsigned int lowpass_order;
	bool lowpass_on_load;			// apply lowpass on sample load (if source rate > mixer rate)
	bool lowpass_on_out;			// apply lowpass on rendered output (if source rate <= mixer rate)
	unsigned int freq_f,freq_fslew;
	unsigned int freq_nslew,freq_nslew_want;
	unsigned int rendering_to_n,rendering_to_d;
	unsigned int rend_n,rend_d;
	unsigned int freq_n,freq_d,freq_d_orig;
	bool current_loaded;
	Bit32s current[2],last[2],delta[2],max_change;
	Bit32s msbuffer[2048][2];		// more than enough for 1ms of audio, at mixer sample rate
	Bits last_sample_write;
	Bitu msbuffer_o;
	Bitu msbuffer_i;
	const char * name;
	bool enabled;
	MixerChannel * next;
};

MixerChannel * MIXER_AddChannel(MIXER_Handler handler,Bitu freq,const char * name);
MixerChannel * MIXER_FindChannel(const char * name);
/* Find the device you want to delete with findchannel "delchan gets deleted" */
void MIXER_DelChannel(MixerChannel* delchan); 

/* Object to maintain a mixerchannel; As all objects it registers itself with create
 * and removes itself when destroyed. */
class MixerObject{
private:
	bool installed;
    char m_name[32] = {};
public:
	MixerObject():installed(false){};
	MixerChannel* Install(MIXER_Handler handler,Bitu freq,const char * name);
	~MixerObject();
};


/* PC Speakers functions, tightly related to the timer functions */
void PCSPEAKER_SetCounter(Bitu cntr,Bitu mode);
void PCSPEAKER_SetType(bool pit_clock_gate_enabled, bool pit_output_enabled);
void PCSPEAKER_SetPITControl(Bitu mode);

#endif
