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
#if HAVE_CONFIG_H
#include "config.h"
#endif

#if defined(HAVE_LASH) || defined(HAVE_LADCCA)

#include "fluid_synth.h"

#define LASH_ENABLED 1

#ifdef HAVE_LASH

#include <lash/lash.h>
extern lash_client_t * fluid_lash_client;
#define fluid_lash_args_t  lash_args_t
#define fluid_lash_alsa_client_id  lash_alsa_client_id
#define fluid_lash_jack_client_name  lash_jack_client_name

#else		/* old deprecated LADCCA support which will be removed someday */

#include <ladcca/ladcca.h>
extern cca_client_t * fluid_lash_client;
#define fluid_lash_args_t  cca_args_t
#define fluid_lash_alsa_client_id  cca_alsa_client_id
#define fluid_lash_jack_client_name  cca_jack_client_name

#endif


fluid_lash_args_t *fluid_lash_extract_args (int * pargc, char *** pargv);
int fluid_lash_connect (fluid_lash_args_t * args);
void fluid_lash_create_thread (fluid_synth_t * synth);

#endif 		/* defined(HAVE_LASH) || defined(HAVE_LADCCA) */
