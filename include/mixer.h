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


#ifndef DOSBOX_MIXER_H
#define DOSBOX_MIXER_H

#include <sstream>

#ifndef DOSBOX_DOSBOX_H
#include "dosbox.h"
#endif

typedef void (*MIXER_MixHandler)(Bit8u * sampdate,Bit32u len);
typedef void (*MIXER_Handler)(Bitu len);

enum BlahModes {
	MIXER_8MONO,MIXER_8STEREO,
	MIXER_16MONO,MIXER_16STEREO
};

enum MixerModes {
	M_8M,M_8S,
	M_16M,M_16S
};

#define MIXER_BUFSIZE (16*1024)
#define MIXER_BUFMASK (MIXER_BUFSIZE-1)
extern Bit8u MixTemp[MIXER_BUFSIZE];

#define MAX_AUDIO ((1<<(16-1))-1)
#define MIN_AUDIO -(1<<(16-1))

class MixerChannel {
public:
	void SetVolume(float _left,float _right);
	void SetScale( float f );
	void UpdateVolume(void);
	void SetFreq(Bitu _freq);
	void Mix(Bitu _needed);
	void AddSilence(void);			//Fill up until needed

	template<class Type,bool stereo,bool signeddata,bool nativeorder>
	void AddSamples(Bitu len, const Type* data);

	void AddSamples_m8(Bitu len, const Bit8u * data);
	void AddSamples_s8(Bitu len, const Bit8u * data);
	void AddSamples_m8s(Bitu len, const Bit8s * data);
	void AddSamples_s8s(Bitu len, const Bit8s * data);
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

	void AddStretched(Bitu len,Bit16s * data);		//Strech block up into needed data
	void AddStretchedStereo(Bitu len,Bit16s * data);		//Strech block up into needed data
	void FillUp(void);
	void Enable(bool _yesno);

	void SaveState( std::ostream& stream );
	void LoadState( std::istream& stream );

	MIXER_Handler handler;
	float volmain[2];
	float scale;
	Bit32s volmul[2];
	Bitu freq_add,freq_index;
	Bitu done,needed;
	Bits last[2];
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
	char m_name[32];
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
