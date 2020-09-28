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

#include "adlib.h"
#include "dosbox.h"

//Use 8 handlers based on a small logatirmic wavetabe and an exponential table for volume
#define WAVE_HANDLER	10
//Use a logarithmic wavetable with an exponential table for volume
#define WAVE_TABLELOG	11
//Use a linear wavetable with a multiply table for volume
#define WAVE_TABLEMUL	12

//Select the type of wave generator routine
#define DBOPL_WAVE WAVE_TABLEMUL

namespace DBOPL {

struct Chip;
struct Operator;
struct Channel;

#if (DBOPL_WAVE == WAVE_HANDLER)
typedef Bits ( DB_FASTCALL *WaveHandler) ( Bitu i, Bitu volume );
#endif

typedef Bits ( DBOPL::Operator::*VolumeHandler) ( );
typedef Channel* ( DBOPL::Channel::*SynthHandler) ( Chip* chip, Bit32u samples, Bit32s* output );

//Different synth modes that can generate blocks of data
typedef enum {
	sm2AM,
	sm2FM,
	sm3AM,
	sm3FM,
	sm4Start,
	sm3FMFM,
	sm3AMFM,
	sm3FMAM,
	sm3AMAM,
	sm6Start,
	sm2Percussion,
	sm3Percussion,
} SynthMode;

//Shifts for the values contained in chandata variable
enum {
	SHIFT_KSLBASE = 16,
	SHIFT_KEYCODE = 24,
};

struct Operator {
public:
	//Masks for operator 20 values
	enum {
		MASK_KSR = 0x10,
		MASK_SUSTAIN = 0x20,
		MASK_VIBRATO = 0x40,
		MASK_TREMOLO = 0x80,
	};

	typedef enum {
		OFF,
		RELEASE,
		SUSTAIN,
		DECAY,
		ATTACK,
	} State;

	VolumeHandler volHandler;

#if (DBOPL_WAVE == WAVE_HANDLER)
	WaveHandler waveHandler;	//Routine that generate a wave 
#else
	Bit16s* waveBase;
	Bit32u waveMask;
	Bit32u waveStart;
#endif
	Bit32u waveIndex;			//WAVE_BITS shifted counter of the frequency index
	Bit32u waveAdd;				//The base frequency without vibrato
	Bit32u waveCurrent;			//waveAdd + vibratao

	Bit32u chanData;			//Frequency/octave and derived data coming from whatever channel controls this
	Bit32u freqMul;				//Scale channel frequency with this, TODO maybe remove?
	Bit32u vibrato;				//Scaled up vibrato strength
	Bit32s sustainLevel;		//When stopping at sustain level stop here
	Bit32s totalLevel;			//totalLevel is added to every generated volume
	Bit32u currentLevel;		//totalLevel + tremolo
	Bit32s volume;				//The currently active volume
	
	Bit32u attackAdd;			//Timers for the different states of the envelope
	Bit32u decayAdd;
	Bit32u releaseAdd;
	Bit32u rateIndex;			//Current position of the evenlope

	uint8_t rateZero;				//Bits for the different states of the envelope having no changes
	uint8_t keyOn;				//Bitmask of different values that can generate keyon
	//Registers, also used to check for changes
	uint8_t reg20, reg40, reg60, reg80, regE0;
	//Active part of the envelope we're in
	uint8_t state;
	//0xff when tremolo is enabled
	uint8_t tremoloMask;
	//Strength of the vibrato
	uint8_t vibStrength;
	//Keep track of the calculated KSR so we can check for changes
	uint8_t ksr;
private:
	void SetState( uint8_t s );
	void UpdateAttack( const Chip* chip );
	void UpdateRelease( const Chip* chip );
	void UpdateDecay( const Chip* chip );
public:
	void UpdateAttenuation();
	void UpdateRates( const Chip* chip );
	void UpdateFrequency( );

	void Write20( const Chip* chip, uint8_t val );
	void Write40( const Chip* chip, uint8_t val );
	void Write60( const Chip* chip, uint8_t val );
	void Write80( const Chip* chip, uint8_t val );
	void WriteE0( const Chip* chip, uint8_t val );

	bool Silent() const;
	void Prepare( const Chip* chip );

	void KeyOn( uint8_t mask);
	void KeyOff( uint8_t mask);

	template< State state>
	Bits TemplateVolume( );

	Bit32s RateForward( Bit32u add );
	Bitu ForwardWave();
	Bitu ForwardVolume();

	Bits GetSample( Bits modulation );
	Bits GetWave( Bitu index, Bitu vol );
public:
	Operator();
};

struct Channel {
	Operator op[2];
	inline Operator* Op( Bitu index ) {
		return &( ( this + (index >> 1) )->op[ index & 1 ]);
	}
	SynthHandler synthHandler;
	Bit32u chanData;		//Frequency/octave and derived values
	Bit32s old[2];			//Old data for feedback

	uint8_t feedback;			//Feedback shift
	uint8_t regB0;			//Register values to check for changes
	uint8_t regC0;
	//This should correspond with reg104, bit 6 indicates a Percussion channel, bit 7 indicates a silent channel
	uint8_t fourMask;
	int8_t maskLeft;		//Sign extended values for both channel's panning
	int8_t maskRight;

	//Forward the channel data to the operators of the channel
	void SetChanData( const Chip* chip, Bit32u data );
	//Change in the chandata, check for new values and if we have to forward to operators
	void UpdateFrequency( const Chip* chip, uint8_t fourOp );
	void UpdateSynth(const Chip* chip);
	void WriteA0( const Chip* chip, uint8_t val );
	void WriteB0( const Chip* chip, uint8_t val );
	void WriteC0( const Chip* chip, uint8_t val );

	//call this for the first channel
	template< bool opl3Mode >
	void GeneratePercussion( Chip* chip, Bit32s* output );

	//Generate blocks of data in specific modes
	template<SynthMode mode>
	Channel* BlockTemplate( Chip* chip, Bit32u samples, Bit32s* output );
	Channel();
};

struct Chip {
	//This is used as the base counter for vibrato and tremolo
	Bit32u lfoCounter = 0;
	Bit32u lfoAdd = 0;
	

	Bit32u noiseCounter = 0;
	Bit32u noiseAdd = 0;
	Bit32u noiseValue = 0;

	//Frequency scales for the different multiplications
    Bit32u freqMul[16] = {};
	//Rates for decay and release for rate of this chip
    Bit32u linearRates[76] = {};
	//Best match attack rates for the rate of this chip
    Bit32u attackRates[76] = {};

	//18 channels with 2 operators each
	Channel chan[18];

	uint8_t reg104;
	uint8_t reg08;
	uint8_t reg04;
	uint8_t regBD;
	uint8_t vibratoIndex = 0;
	uint8_t tremoloIndex = 0;
	int8_t vibratoSign = 0;
	uint8_t vibratoShift = 0;
	uint8_t tremoloValue = 0;
	uint8_t vibratoStrength = 0;
	uint8_t tremoloStrength = 0;
	//Mask for allowed wave forms
	uint8_t waveFormMask = 0;
	//0 or -1 when enabled
	int8_t opl3Active;

	//Return the maximum amount of samples before and LFO change
	Bit32u ForwardLFO( Bit32u samples );
	Bit32u ForwardNoise();

	void WriteBD( uint8_t val );
	void WriteReg(Bit32u reg, uint8_t val );

	Bit32u WriteAddr( Bit32u port, uint8_t val );

	void GenerateBlock2( Bitu total, Bit32s* output );
	void GenerateBlock3( Bitu total, Bit32s* output );

	//Update the synth handlers in all channels
	void UpdateSynths();
	void Generate( Bit32u samples );
	void Setup( Bit32u rate );

	Chip();
};

struct Handler : public Adlib::Handler {
	DBOPL::Chip chip;
	virtual Bit32u WriteAddr( Bit32u port, uint8_t val );
	virtual void WriteReg( Bit32u addr, uint8_t val );
	virtual void Generate( MixerChannel* chan, Bitu samples );
	virtual void Init( Bitu rate );
	virtual void SaveState( std::ostream& stream );
	virtual void LoadState( std::istream& stream );

};


}		//Namespace
