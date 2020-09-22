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


#ifndef _FLUID_RAMSFONT_H
#define _FLUID_RAMSFONT_H


#include "fluidsynth.h"
#include "../utils/fluidsynth_priv.h"

#include "fluid_defsfont.h"


#ifdef __cplusplus
extern "C" {
#endif


  /*
 * fluid_ramsfont_t
 */
struct _fluid_ramsfont_t
{
  char name[21];                        /* the name of the soundfont */
  fluid_list_t* sample;    /* the samples in this soundfont */
  fluid_rampreset_t* preset;    /* the presets of this soundfont */

  fluid_preset_t iter_preset;        /* preset interface used in the iteration */
  fluid_rampreset_t* iter_cur;       /* the current preset in the iteration */
};

/*
 * fluid_preset_t
 */
struct _fluid_rampreset_t
{
  fluid_rampreset_t* next;
  fluid_ramsfont_t* sfont;                  /* the soundfont this preset belongs to */
  char name[21];                        /* the name of the preset */
  unsigned int bank;                    /* the bank number */
  unsigned int num;                     /* the preset number */
  fluid_preset_zone_t* global_zone;        /* the global zone of the preset */
  fluid_preset_zone_t* zone;               /* the chained list of preset zones */
  fluid_list_t *presetvoices;									/* chained list of used voices */
};


#ifdef __cplusplus
}
#endif

#endif  /* _FLUID_SFONT_H */
