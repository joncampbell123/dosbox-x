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

#define __EXTFILT_CC__
#include "extfilt.h"
#include "../../save_state.h"


// ----------------------------------------------------------------------------
// Constructor.
// ----------------------------------------------------------------------------
ExternalFilter::ExternalFilter()
{
  reset();
  enable_filter(true);
  set_chip_model(MOS6581);

  // Low-pass:  R = 10kOhm, C = 1000pF; w0l = 1/RC = 1/(1e4*1e-9) = 100000
  // High-pass: R =  1kOhm, C =   10uF; w0h = 1/RC = 1/(1e3*1e-5) =    100
  // Multiply with 1.048576 to facilitate division by 1 000 000 by right-
  // shifting 20 times (2 ^ 20 = 1048576).

  w0lp = 104858;
  w0hp = 105;
}


// ----------------------------------------------------------------------------
// Enable filter.
// ----------------------------------------------------------------------------
void ExternalFilter::enable_filter(bool enable)
{
  enabled = enable;
}


// ----------------------------------------------------------------------------
// Set chip model.
// ----------------------------------------------------------------------------
void ExternalFilter::set_chip_model(chip_model model)
{
  if (model == MOS6581) {
    // Maximum mixer DC output level; to be removed if the external
    // filter is turned off: ((wave DC + voice DC)*voices + mixer DC)*volume
    // See voice.cc and filter.cc for an explanation of the values.
    mixer_DC = ((((0x800 - 0x380) + 0x800)*0xff*3 - 0xfff*0xff/18) >> 7)*0x0f;
  }
  else {
    // No DC offsets in the MOS8580.
    mixer_DC = 0;
  }
}


// ----------------------------------------------------------------------------
// SID reset.
// ----------------------------------------------------------------------------
void ExternalFilter::reset()
{
  // State of filter.
  Vlp = 0;
  Vhp = 0;
  Vo = 0;
}



// save state support

void ExternalFilter::SaveState( std::ostream& stream )
{
	// - pure data
	WRITE_POD( &enabled, enabled );
	WRITE_POD( &mixer_DC, mixer_DC );

	WRITE_POD( &Vlp, Vlp );
	WRITE_POD( &Vhp, Vhp );
	WRITE_POD( &Vo, Vo );

	WRITE_POD( &w0lp, w0lp );
	WRITE_POD( &w0hp, w0hp );
}


void ExternalFilter::LoadState( std::istream& stream )
{
	// - pure data
	READ_POD( &enabled, enabled );
	READ_POD( &mixer_DC, mixer_DC );

	READ_POD( &Vlp, Vlp );
	READ_POD( &Vhp, Vhp );
	READ_POD( &Vo, Vo );

	READ_POD( &w0lp, w0lp );
	READ_POD( &w0hp, w0hp );
}



/*
ykhwong svn-daum 2012-05-21


class ExternalFilter

	// - pure data
  bool enabled;

  sound_sample mixer_DC;

  sound_sample Vlp;
  sound_sample Vhp;
  sound_sample Vo;

  sound_sample w0lp;
  sound_sample w0hp;
*/
