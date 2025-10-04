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

#ifndef _FLUIDSYNTH_SHELL_H
#define _FLUIDSYNTH_SHELL_H


#ifdef __cplusplus
extern "C" {
#endif


/**
 * @file shell.h
 * @brief Command shell interface
 *
 * The shell interface allows you to send simple textual commands to
 * the synthesizer, to parse a command file, or to read commands
 * from the stdin or other input streams.
 */

FLUIDSYNTH_API fluid_istream_t fluid_get_stdin(void);
FLUIDSYNTH_API fluid_ostream_t fluid_get_stdout(void);

FLUIDSYNTH_API char* fluid_get_userconf(char* buf, int len);
FLUIDSYNTH_API char* fluid_get_sysconf(char* buf, int len);

/**
 * Command handler function prototype.
 * @param data User defined data
 * @param ac Argument count
 * @param av Array of string arguments
 * @param out Output stream to send response to
 * @return Should return #FLUID_OK on success, #FLUID_FAILED otherwise
 */
typedef int (*fluid_cmd_func_t)(void* data, int ac, char** av, fluid_ostream_t out);  

/**
 * Shell command information structure.
 */
typedef struct {
  char* name;                           /**< The name of the command, as typed in the shell */
  char* topic;                          /**< The help topic group of this command */ 
  fluid_cmd_func_t handler;             /**< Pointer to the handler for this command */
  void* data;                           /**< User data passed to the handler */
  char* help;                           /**< A help string */
} fluid_cmd_t;


/* The command handler */

FLUIDSYNTH_API 
fluid_cmd_handler_t* new_fluid_cmd_handler(fluid_synth_t* synth);

FLUIDSYNTH_API 
void delete_fluid_cmd_handler(fluid_cmd_handler_t* handler);

FLUIDSYNTH_API 
void fluid_cmd_handler_set_synth(fluid_cmd_handler_t* handler, fluid_synth_t* synth);

FLUIDSYNTH_API 
int fluid_cmd_handler_register(fluid_cmd_handler_t* handler, fluid_cmd_t* cmd);

FLUIDSYNTH_API 
int fluid_cmd_handler_unregister(fluid_cmd_handler_t* handler, const char *cmd);


/* Command function */

FLUIDSYNTH_API 
int fluid_command(fluid_cmd_handler_t* handler, const char *cmd, fluid_ostream_t out);

FLUIDSYNTH_API 
int fluid_source(fluid_cmd_handler_t* handler, const char *filename);

FLUIDSYNTH_API 
void fluid_usershell(fluid_settings_t* settings, fluid_cmd_handler_t* handler);


/* Shell */

FLUIDSYNTH_API 
fluid_shell_t* new_fluid_shell(fluid_settings_t* settings, fluid_cmd_handler_t* handler,
			     fluid_istream_t in, fluid_ostream_t out, int thread);

FLUIDSYNTH_API void delete_fluid_shell(fluid_shell_t* shell);



/* TCP/IP server */

/**
 * Callback function which is executed for new server connections.
 * @param data User defined data supplied in call to new_fluid_server()
 * @param addr The IP address of the client (can be NULL)
 * @return Should return a new command handler for the connection (new_fluid_cmd_handler()).
 */
typedef fluid_cmd_handler_t* (*fluid_server_newclient_func_t)(void* data, char* addr);

FLUIDSYNTH_API 
fluid_server_t* new_fluid_server(fluid_settings_t* settings, 
			       fluid_server_newclient_func_t func,
			       void* data);

FLUIDSYNTH_API void delete_fluid_server(fluid_server_t* server);

FLUIDSYNTH_API int fluid_server_join(fluid_server_t* server);


#ifdef __cplusplus
}
#endif

#endif /* _FLUIDSYNTH_SHELL_H */
