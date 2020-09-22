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

/* RAM SoundFonts: October 2002 - Antoine Schmitt */

#ifndef _FLUIDSYNTH_RAMSFONT_H
#define _FLUIDSYNTH_RAMSFONT_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file ramsfont.h
 * @brief API for creating and managing SoundFont instruments in RAM.
 *
 * RAM SoundFonts live in ram. The samples are loaded from files
 * or from RAM.  A minimal API manages a soundFont structure,
 * with presets, each preset having only one preset-zone, which
 * instrument has potentially many instrument-zones.  No global
 * zones, and nor generator nor modulator other than the default
 * ones are permitted.  This may be extensible in the future.
 */

FLUIDSYNTH_API fluid_sfont_t* fluid_ramsfont_create_sfont(void);
FLUIDSYNTH_API int fluid_ramsfont_set_name(fluid_ramsfont_t* sfont, const char *name);
FLUIDSYNTH_API 
int fluid_ramsfont_add_izone(fluid_ramsfont_t* sfont,
				unsigned int bank, unsigned int num, fluid_sample_t* sample,
				int lokey, int hikey);
FLUIDSYNTH_API
int fluid_ramsfont_remove_izone(fluid_ramsfont_t* sfont,
				unsigned int bank, unsigned int num, fluid_sample_t* sample);
FLUIDSYNTH_API
int fluid_ramsfont_izone_set_gen(fluid_ramsfont_t* sfont,
				unsigned int bank, unsigned int num, fluid_sample_t* sample,
				int gen_type, float value);
FLUIDSYNTH_API
int fluid_ramsfont_izone_set_loop(fluid_ramsfont_t* sfont,
				unsigned int bank, unsigned int num, fluid_sample_t* sample,
				int on, float loopstart, float loopend);

FLUIDSYNTH_API fluid_sample_t* new_fluid_ramsample(void);
FLUIDSYNTH_API int delete_fluid_ramsample(fluid_sample_t* sample);
FLUIDSYNTH_API int fluid_sample_set_name(fluid_sample_t* sample, const char *name);
FLUIDSYNTH_API 
int fluid_sample_set_sound_data(fluid_sample_t* sample, short *data, 
			       unsigned int nbframes, short copy_data, int rootkey);


#ifdef __cplusplus
}
#endif

#endif /* _FLUIDSYNTH_RAMSFONT_H */
