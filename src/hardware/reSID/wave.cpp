//  ---------------------------------------------------------------------------
//  This file is part of reSID, a MOS6581 SID emulator engine.
//  Copyright (C) 2004  Dag Lem <resid@nimrod.no>
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//  ---------------------------------------------------------------------------

#define __WAVE_CC__
#include "wave.h"
#include "../../save_state.h"

// ----------------------------------------------------------------------------
// Constructor.
// ----------------------------------------------------------------------------
WaveformGenerator::WaveformGenerator()
{
  sync_source = this;

  set_chip_model(MOS6581);

  reset();
}


// ----------------------------------------------------------------------------
// Set sync source.
// ----------------------------------------------------------------------------
void WaveformGenerator::set_sync_source(WaveformGenerator* source)
{
  sync_source = source;
  source->sync_dest = this;
}


// ----------------------------------------------------------------------------
// Set chip model.
// ----------------------------------------------------------------------------
void WaveformGenerator::set_chip_model(chip_model model)
{
  if (model == MOS6581) {
    wave__ST = wave6581__ST;
    wave_P_T = wave6581_P_T;
    wave_PS_ = wave6581_PS_;
    wave_PST = wave6581_PST;
  }
  else {
    wave__ST = wave8580__ST;
    wave_P_T = wave8580_P_T;
    wave_PS_ = wave8580_PS_;
    wave_PST = wave8580_PST;
  }
}


// ----------------------------------------------------------------------------
// Register functions.
// ----------------------------------------------------------------------------
void WaveformGenerator::writeFREQ_LO(reg8 freq_lo)
{
  freq = freq & 0xff00 | freq_lo & 0x00ff;
}

void WaveformGenerator::writeFREQ_HI(reg8 freq_hi)
{
  freq = (freq_hi << 8) & 0xff00 | freq & 0x00ff;
}

void WaveformGenerator::writePW_LO(reg8 pw_lo)
{
  pw = pw & 0xf00 | pw_lo & 0x0ff;
}

void WaveformGenerator::writePW_HI(reg8 pw_hi)
{
  pw = (pw_hi << 8) & 0xf00 | pw & 0x0ff;
}

void WaveformGenerator::writeCONTROL_REG(reg8 control)
{
  waveform = (control >> 4) & 0x0f;
  ring_mod = control & 0x04;
  sync = control & 0x02;

  reg8 test_next = control & 0x08;

  // Test bit set.
  // The accumulator and the shift register are both cleared.
  // NB! The shift register is not really cleared immediately. It seems like
  // the individual bits in the shift register start to fade down towards
  // zero when test is set. All bits reach zero within approximately
  // $2000 - $4000 cycles.
  // This is not modeled. There should fortunately be little audible output
  // from this peculiar behavior.
  if (test_next) {
    accumulator = 0;
    shift_register = 0;
  }
  // Test bit cleared.
  // The accumulator starts counting, and the shift register is reset to
  // the value 0x7ffff8.
  // NB! The shift register will not actually be set to this exact value if the
  // shift register bits have not had time to fade to zero.
  // This is not modeled.
  else if (test) {
    shift_register = 0x7ffff8;
  }

  test = test_next;

  // The gate bit is handled by the EnvelopeGenerator.
}

reg8 WaveformGenerator::readOSC()
{
  return output() >> 4;
}

// ----------------------------------------------------------------------------
// SID reset.
// ----------------------------------------------------------------------------
void WaveformGenerator::reset()
{
  accumulator = 0;
  shift_register = 0x7ffff8;
  freq = 0;
  pw = 0;

  test = 0;
  ring_mod = 0;
  sync = 0;

  msb_rising = false;
}



// save state support

void WaveformGenerator::SaveState( std::ostream& stream )
{
	unsigned char wave_st_idx, wave_pt_idx, wave_ps_idx, wave_pst_idx;


	wave_st_idx = 0xff;
	wave_pt_idx = 0xff;
	wave_ps_idx = 0xff;
	wave_pst_idx = 0xff;


	if(0) {}
	else if( wave__ST == wave6581__ST ) wave_st_idx = 0;
	else if( wave__ST == wave8580__ST ) wave_st_idx = 1;


	if(0) {}
	else if( wave_P_T == wave6581_P_T ) wave_pt_idx = 0;
	else if( wave_P_T == wave8580_P_T ) wave_pt_idx = 1;


	if(0) {}
	else if( wave_PS_ == wave6581_PS_ ) wave_ps_idx = 0;
	else if( wave_PS_ == wave8580_PS_ ) wave_ps_idx = 1;


	if(0) {}
	else if( wave_PST == wave6581_PST ) wave_pst_idx = 0;
	else if( wave_PST == wave8580_PST ) wave_pst_idx = 1;

	//**********************************************
	//**********************************************
	//**********************************************

	// - pure data
	WRITE_POD( &msb_rising, msb_rising );

	WRITE_POD( &accumulator, accumulator );
	WRITE_POD( &shift_register, shift_register );
	WRITE_POD( &freq, freq );
	WRITE_POD( &pw, pw );

	WRITE_POD( &waveform, waveform );
	WRITE_POD( &test, test );
	WRITE_POD( &ring_mod, ring_mod );
	WRITE_POD( &sync, sync );



	// - reloc ptr
	WRITE_POD( &wave_st_idx, wave_st_idx );
	WRITE_POD( &wave_pt_idx, wave_pt_idx );
	WRITE_POD( &wave_ps_idx, wave_ps_idx );
	WRITE_POD( &wave_pst_idx, wave_pst_idx );
}


void WaveformGenerator::LoadState( std::istream& stream )
{
	unsigned char wave_st_idx, wave_pt_idx, wave_ps_idx, wave_pst_idx;

	//**********************************************
	//**********************************************
	//**********************************************

	// - pure data
	READ_POD( &msb_rising, msb_rising );

	READ_POD( &accumulator, accumulator );
	READ_POD( &shift_register, shift_register );
	READ_POD( &freq, freq );
	READ_POD( &pw, pw );

	READ_POD( &waveform, waveform );
	READ_POD( &test, test );
	READ_POD( &ring_mod, ring_mod );
	READ_POD( &sync, sync );



	// - reloc ptr
	READ_POD( &wave_st_idx, wave_st_idx );
	READ_POD( &wave_pt_idx, wave_pt_idx );
	READ_POD( &wave_ps_idx, wave_ps_idx );
	READ_POD( &wave_pst_idx, wave_pst_idx );

	//**********************************************
	//**********************************************
	//**********************************************

	switch( wave_st_idx ) {
		case 0: wave__ST = wave6581__ST; break;
		case 1: wave__ST = wave8580__ST; break;
	}

	switch( wave_pt_idx ) {
		case 0: wave_P_T = wave6581_P_T; break;
		case 1: wave_P_T = wave8580_P_T; break;
	}

	switch( wave_ps_idx ) {
		case 0: wave_PS_ = wave6581_PS_; break;
		case 1: wave_PS_ = wave8580_PS_; break;
	}

	switch( wave_pt_idx ) {
		case 0: wave_PST = wave6581_PST; break;
		case 1: wave_PST = wave8580_PST; break;
	}
}



/*
ykhwong svn-daum 2012-05-21


class WaveformGenerator

	// - static class ptr
  const WaveformGenerator* sync_source;
  WaveformGenerator* sync_dest;


	// - pure data
  bool msb_rising;

  reg24 accumulator;
  reg24 shift_register;

  reg16 freq;
  reg12 pw;

  reg8 waveform;

  reg8 test;
  reg8 ring_mod;
  reg8 sync;


  // - static data
  static reg8 wave6581__ST[];
  static reg8 wave6581_P_T[];
  static reg8 wave6581_PS_[];
  static reg8 wave6581_PST[];

  static reg8 wave8580__ST[];
  static reg8 wave8580_P_T[];
  static reg8 wave8580_PS_[];
  static reg8 wave8580_PST[];


	// - reloc ptr
  reg8* wave__ST;
  reg8* wave_P_T;
  reg8* wave_PS_;
  reg8* wave_PST;
*/
