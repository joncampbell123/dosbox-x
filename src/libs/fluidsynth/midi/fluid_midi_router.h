/* FluidSynth - A Software Synthesizer
 *
 * Copyright (C) 2003  Peter Hanappe and others.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 */

/* Author: Markus Nentwig, nentwig@users.sourceforge.net
 */

#ifndef _FLUID_MIDIROUTER_H
#define _FLUID_MIDIROUTER_H

#include "../utils/fluidsynth_priv.h"
#include "fluid_midi.h"
#include "../utils/fluid_sys.h"

int fluid_midi_router_handle_clear(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out);
int fluid_midi_router_handle_default(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out);
int fluid_midi_router_handle_begin(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out);
int fluid_midi_router_handle_chan(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out);
int fluid_midi_router_handle_par1(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out);
int fluid_midi_router_handle_par2(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out);
int fluid_midi_router_handle_end(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out);

#endif
