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

#ifndef _FLUID_CMD_H
#define _FLUID_CMD_H

#include "../utils/fluidsynth_priv.h"

void fluid_shell_settings(fluid_settings_t* settings);


/** some help functions */
int fluid_is_number(char* a);
int fluid_is_empty(char* a);
char* fluid_expand_path(char* path, char* new_path, int len);

/** the handlers for the command lines */
int fluid_handle_help(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out);
int fluid_handle_quit(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out);
int fluid_handle_noteon(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out);
int fluid_handle_noteoff(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out);
int fluid_handle_pitch_bend(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out);
int fluid_handle_pitch_bend_range(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out);
int fluid_handle_cc(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out);
int fluid_handle_prog(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out);
int fluid_handle_select(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out);
int fluid_handle_inst(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out);
int fluid_handle_channels(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out);
int fluid_handle_load(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out);
int fluid_handle_unload(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out);
int fluid_handle_reload(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out);
int fluid_handle_fonts(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out);
int fluid_handle_mstat(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out);
int fluid_handle_reverbpreset(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out);
int fluid_handle_reverbsetroomsize(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out);
int fluid_handle_reverbsetdamp(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out);
int fluid_handle_reverbsetwidth(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out);
int fluid_handle_reverbsetlevel(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out);
int fluid_handle_chorusnr(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out);
int fluid_handle_choruslevel(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out);
int fluid_handle_chorusspeed(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out);
int fluid_handle_chorusdepth(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out);
int fluid_handle_chorus(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out);
int fluid_handle_reverb(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out);
int fluid_handle_gain(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out);
int fluid_handle_interp(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out);
int fluid_handle_interpc(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out);
int fluid_handle_tuning(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out);
int fluid_handle_tune(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out);
int fluid_handle_settuning(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out);
int fluid_handle_resettuning(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out);
int fluid_handle_tunings(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out);
int fluid_handle_dumptuning(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out);
int fluid_handle_reset(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out);
int fluid_handle_source(fluid_cmd_handler_t* handler, int ac, char** av, fluid_ostream_t out);
int fluid_handle_echo(fluid_cmd_handler_t* handler, int ac, char** av, fluid_ostream_t out);

int fluid_handle_set(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out);
int fluid_handle_get(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out);
int fluid_handle_info(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out);
int fluid_handle_settings(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out);


fluid_cmd_t* fluid_cmd_copy(fluid_cmd_t* cmd);
void delete_fluid_cmd(fluid_cmd_t* cmd);

int fluid_cmd_handler_handle(fluid_cmd_handler_t* handler,
			    int ac, char** av,
			    fluid_ostream_t out);



void fluid_server_remove_client(fluid_server_t* server, fluid_client_t* client);
void fluid_server_add_client(fluid_server_t* server, fluid_client_t* client);


fluid_client_t* new_fluid_client(fluid_server_t* server,
			       fluid_settings_t* settings,
			       fluid_cmd_handler_t* handler,
			       fluid_socket_t sock);

void delete_fluid_client(fluid_client_t* client);
void fluid_client_quit(fluid_client_t* client);


#endif /* _FLUID_CMD_H */
