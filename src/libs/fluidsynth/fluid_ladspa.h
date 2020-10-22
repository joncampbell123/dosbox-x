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


#ifndef _FLUID_LADSPA_H
#define _FLUID_LADSPA_H

/***************************************************************
 *
 *                         INCLUDES
 */

#include "fluidsynth_priv.h"

#ifdef LADSPA
#include "fluid_list.h"
#include <pthread.h>
#include <ladspa.h>

/***************************************************************
 *
 *                         DEFINES
 */

/* How many different plugin libraries may be used at the same time? */
#define FLUID_LADSPA_MaxLibs 100
/* How many plugin instances may be used at the same time? */
#define FLUID_LADSPA_MaxPlugins 100
/* How many nodes are allowed? */
#define FLUID_LADSPA_MaxNodes 100
/* How many tokens are allowed in one command line? (for example 152 => max. 50 port plugin allowed) */
#define FLUID_LADSPA_MaxTokens 152
/* What is the maximum path length? */
#define FLUID_LADSPA_MaxPathLength 512
/***************************************************************
 *
 *                         ENUM
 */

typedef enum {
  fluid_LADSPA_NoMatch,
  fluid_LADSPA_PartialMatch,
  fluid_LADSPA_FullMatch
} fluid_LADSPA_Stringmatch_t;

/* Bypass state of the Fx unit */
typedef enum {
  fluid_LADSPA_Active,
  fluid_LADSPA_Bypassed,
  fluid_LADSPA_BypassRequest
} fluid_LADSPA_BypassState;

typedef enum {
  fluid_LADSPA_node_is_source=1,
  fluid_LADSPA_node_is_sink=2,
  fluid_LADSPA_node_is_audio=4,
  fluid_LADSPA_node_is_control=8,
  fluid_LADSPA_node_is_dummy=16,
  fluid_LADSPA_node_is_user_ctrl=32
} fluid_LADSPA_nodeflags;

/* fluid_LADSPA_Node_t
 * An internal node of the Fx unit.
 * A 'node' is the 'glue' that connects several LADSPA plugins.
 * Basically it's a real-valued variable (control node) or a real-valued buffer (audio node).
 */
typedef struct {
  LADSPA_Data * buf;      /*Either the buffer (Audio node) or a single control value (Control node)*/
  char * Name;            /* Unique identifier*/
  int InCount;            /* How many sources feed into this node? (0 or 1) */
  int OutCount;           /* How many other elements take data out of this node? */
  int flags;
} fluid_LADSPA_Node_t;

/*
 * fluid_LADSPA_Fx_t
 * Fx unit using LADSPA.
 * This includes a number of LADSPA plugins, their libraries, nodes etc.
 * The Fx unit connects its input to Fluidsynth and its output to the soundcard.
 */
typedef struct {
  /* LADSPA-plugins are in shared libraries (for example aw.so).
   * Pointers to them are stored here. A library is uniquely identified through
   * its filename (full path).*/
  fluid_synth_t* synth;

  int NumberLibs;
  void * ppvPluginLibs[FLUID_LADSPA_MaxLibs];
  char * ppvPluginLibNames[FLUID_LADSPA_MaxLibs];

  /*List of plugins (descriptor and instance)
   * A LADSPA plugin descriptor points to the code, which is executed, when a plugin is run.
   * The plugin instance is given as a parameter, when calling.
   */
  int NumberPlugins;
  const LADSPA_Descriptor * PluginDescriptorTable[FLUID_LADSPA_MaxPlugins];
  LADSPA_Handle * PluginInstanceTable[FLUID_LADSPA_MaxPlugins];

  /* List of nodes */
  int NumberNodes;
  fluid_LADSPA_Node_t * Nodelist[FLUID_LADSPA_MaxNodes];

  /* List of Command lines
   * During the setup phase, each ladspa_add command creates one command sequence. For example:
   * ./aw.so alienwah_stereo Input <- Master_L_Synth Output -> Master_R_Synth Parameter <- #42.0
   * Those lists are stored in LADSPA_Command_Sequence.
   * One command line results in one plugin => size MaxPlugins.
   */
  int NumberCommands;
  char ** LADSPA_Command_Sequence[FLUID_LADSPA_MaxPlugins];

  /* User control nodes
   * A user control node is declared at any time before the ladspa_start command.
   * It acts as a constant node, but it has a name and can be changed with the ladspa_nodeset command. */
  int NumberUserControlNodes;
  char * UserControlNodeNames[FLUID_LADSPA_MaxNodes];
  fluid_real_t UserControlNodeValues[FLUID_LADSPA_MaxNodes];

  /* Bypass switch
   * If set, the LADSPA Fx unit does not touch the signal.*/
  fluid_LADSPA_BypassState Bypass;

  /* Communication between the 'command line' process and the synthesis process.
   * A possible conflict situation arises, when fluid_clear is called, and starts to destroy
   * the plugins. But the synthesis thread still processes plugins at the same time. The consequences are ugly.
   * Therefore ladspa_clear waits for acknowledgement from the synthesis thread, that the Fx unit is bypassed.
   * 'cond' is used for the communication, the mutex is required for changing the condition.
   */
  pthread_cond_t cond;
  pthread_mutex_t mutex;
} fluid_LADSPA_FxUnit_t;

/*
 * misc
 */

/* Purpose:
 * Creates a new Fx unit in bypass mode with default settings.
 * It is ready for further calls (add, clear, start).
 */
fluid_LADSPA_FxUnit_t* new_fluid_LADSPA_FxUnit(fluid_synth_t* synth);

/* Purpose:
 * Applies the master gain (from command line option --gain or gain command).
 * Processes one block of sound data (generated from the synthesizer) through
 * the LADSPA Fx unit.
 * Acknowledges a bypass request.
 */
void fluid_LADSPA_run(fluid_LADSPA_FxUnit_t* Fx_unit, fluid_real_t* left_buf[], fluid_real_t* right_buf[], fluid_real_t* fx_left_buf[], fluid_real_t* fx_right_buf[]);

/* Purpose:
 * Returns the node belonging to Name or NULL, if not found
 */
fluid_LADSPA_Node_t* fluid_LADSPA_RetrieveNode(fluid_LADSPA_FxUnit_t* FxUnit, char * Name);

/* Purpose:
 * Creates a new node with the given characteristics.
 */
fluid_LADSPA_Node_t* fluid_LADSPA_CreateNode(fluid_LADSPA_FxUnit_t* FxUnit, char * Name, int flags);

/* Purpose:
 * - Resets LADSPA Fx unit to bypass.
 * - Removes all plugins from the reverb unit.
 * - Releases all libraries.
 * Note: It would be more efficient to keep the libraries. But then the user would have to restart fluidsynth each time
 * a plugin is recompiled.
 */
void fluid_LADSPA_clear(fluid_LADSPA_FxUnit_t* FxUnit);

/* Purpose:
 * Frees all memory and shuts down the Fx block.
 * The synthesis thread must be stopped, when calling.
 */
void fluid_LADSPA_shutdown(fluid_LADSPA_FxUnit_t* FxUnit);



/*
 * fluid_handle_LADSPA_XXX
 * Those functions are called from fluid_cmd, when a command is entered on the command line.
 */

/* Purpose:
 * - Resets LADSPA Fx unit to bypass.
 * - Removes all plugins from the reverb unit.
 * - Releases all libraries.
 * Note: It would be more efficient to keep the libraries. But then the user would have to restart fluidsynth each time
 * a plugin is recompiled.
 */
int fluid_LADSPA_handle_clear(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out);

/* Purpose:
 * Loads the plugins added with 'ladspa_add' and then start the Fx unit.
 * Internal processes:
 * - load the LADSPA plugin libraries
 * - instantiate the plugins
 * - connect the plugins
 * - set the bypass switch to 'not bypassed'
 */
int fluid_LADSPA_handle_start(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out);

/* Purpose:
 * Adds one plugin into the list of the LADSPA Fx unit.
 * This is only allowed, while the Fx block is in 'bypass' state (after clear).
 */
int fluid_LADSPA_handle_add(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out);

/* Purpose:
 * Declares a user control node and a value; for further processing in ladspa_start.
 */
int fluid_LADSPA_handle_declnode(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out);

/* Purpose:
 * Assigns a value to the a user control node
 */
int fluid_LADSPA_handle_setnode(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out);

#endif /* LADSPA */

#endif  /* _FLUID_LADSPA_H */
