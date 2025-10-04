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

#ifndef _FLUID_MDRIVER_H
#define _FLUID_MDRIVER_H

#include "fluidsynth_priv.h"

void fluid_midi_driver_settings(fluid_settings_t* settings);


/*
 * fluid_midi_driver_t
 */

struct _fluid_midi_driver_t
{
  char* name;
  handle_midi_event_func_t handler;
  void* data;
};



#endif  /* _FLUID_AUDRIVER_H */
