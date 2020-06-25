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


#ifndef DOSBOX_ADLIB_H
#define DOSBOX_ADLIB_H

#include "dosbox.h"
#include "mixer.h"
#include "inout.h"
#include "setup.h"
#include "pic.h"
#include "hardware.h"


namespace Adlib {

class Timer {
	//Rounded down start time
	double start;
	//Clock interval
	double interval;
	//Delay before you overflow
	double delay;
	Bit8u counter;
public:
	bool enabled;
	bool overflow;

	Timer( Bit16s micros ) {
		overflow = false;
		enabled = false;
		counter = 0;
		start = 0;
		delay = 0;
		//Interval in milliseconds
		interval = micros * 0.001;
	}

	//Update returns with true if overflow
	bool Update( double time ) {
		if ( !enabled ) 
			return false;
		const double deltaTime = time - start;
		//Only set the overflow flag when not masked
		if (deltaTime >= delay  ) {
			overflow = true;
			return true;
		}
		return false;
	}
	
	//On a reset make sure the start is in sync with the next cycle
	void Reset(const double time ) {
		overflow = false;
		if ( !enabled )
			return;
		//Sync start to the last delay interval
		const double deltaTime = time - start;
		const double rem = fmod(deltaTime, delay);
		start = time - rem;
	}

	void SetCounter(Bit8u val) {
		counter = val;
	}
	
	//Stopping always clears the overflow as well
	void Stop( ) {
		enabled = false;
		overflow = false;
	}
	
	//Starting clears overflow
	void Start( const double time ) {
		enabled = true;
		overflow = false;
		//The counter is basically copied on start so calculate delay now
		delay = (256 - counter) * interval;
		//Sync start to the last clock interval
		double rem = fmod(time, interval);
		start = time - rem;
	}
	
	//Does this clock need an update, you could save a pic_fullindex on read?
	bool NeedUpdate() const {
		return enabled && !overflow;
	}
};

struct Chip {
	//Last selected register
	Timer timer0, timer1;
	//Check for it being a write to the timer
	bool Write( Bit32u reg, Bit8u val );
	//Read the current timer state, will use current double
	Bit8u Read( );
	
	Chip();
	//poll counter
	double last_poll = 0;
	unsigned int poll_counter = 0;
};

//The type of handler this is
typedef enum {
	MODE_OPL2,
	MODE_DUALOPL2,
	MODE_OPL3,
	MODE_OPL3GOLD
} Mode;

class Handler {
public:
	//Write an address to a chip, returns the address the chip sets
	virtual Bit32u WriteAddr( Bit32u port, Bit8u val ) = 0;
	//Write to a specific register in the chip
	virtual void WriteReg( Bit32u addr, Bit8u val ) = 0;
	//Generate a certain amount of samples
	virtual void Generate( MixerChannel* chan, Bitu samples ) = 0;
	//Initialize at a specific sample rate and mode
	virtual void Init( Bitu rate ) = 0;
	virtual void SaveState( std::ostream& stream ) { (void)stream; }
	virtual void LoadState( std::istream& stream ) { (void)stream; }

	virtual ~Handler() {
	}
};

//The cache for 2 chips or an opl3
typedef Bit8u RegisterCache[512];

//Internal class used for dro capturing
class Capture;

class Module: public Module_base {
	IO_ReadHandleObject ReadHandler[12];
	IO_WriteHandleObject WriteHandler[12];
	MixerObject mixerObject;

	//Mode we're running in
	Mode mode;
	//Last selected address in the chip for the different modes
	union {
		Bit32u normal;
		Bit8u dual[2];
	} reg;
	struct {
		bool active;
		Bit8u index;
		Bit8u lvol;
		Bit8u rvol;
		bool mixer;
    } ctrl = {};
	void CacheWrite( Bit32u reg, Bit8u val );
	void DualWrite( Bit8u index, Bit8u reg, Bit8u val );
	void CtrlWrite( Bit8u val );
	Bitu CtrlRead( void );
public:
	static OPL_Mode oplmode;
	MixerChannel* mixerChan;
	Bit32u lastUsed;				//Ticks when adlib was last used to turn of mixing after a few second

	Handler* handler;				//Handler that will generate the sound
    RegisterCache cache = {};
	Capture* capture;
	Chip	chip[2];

	//Handle port writes
	void PortWrite( Bitu port, Bitu val, Bitu iolen );
	Bitu PortRead( Bitu port, Bitu iolen );
	void Init( Mode m );

	// savestate support
	virtual void SaveState( std::ostream& stream );
	virtual void LoadState( std::istream& stream );

	Module( Section* configuration); 
	~Module();
};


}		//Adlib namespace

#endif
