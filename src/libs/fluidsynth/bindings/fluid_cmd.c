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

//#include <glib.h>
#include <fcntl.h>

#include "../utils/fluidsynth_priv.h"
#include "fluid_cmd.h"
#include "../synth/fluid_synth.h"
#include "../utils/fluid_settings.h"
#include "../utils/fluid_hash.h"
#include "../utils/fluid_sys.h"
#include "../midi/fluid_midi_router.h"
#include "../sfloader/fluid_sfont.h"
#include "../synth/fluid_chan.h"

#if WITH_READLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif

#define MAX_TOKENS 100 /* LADSPA plugins need lots of parameters */
#define MAX_COMMAND_LEN 1024	/* max command length accepted by fluid_command() */
#define FLUID_WORKLINELENGTH 1024 /* LADSPA plugins use long command lines */

struct _fluid_shell_t {
  fluid_settings_t* settings;
  fluid_cmd_handler_t* handler;
  fluid_thread_t* thread;
  fluid_istream_t in;
  fluid_ostream_t out;
};

static int fluid_shell_run(fluid_shell_t* shell);
static void fluid_shell_init(fluid_shell_t* shell,
                             fluid_settings_t* settings, fluid_cmd_handler_t* handler,
                             fluid_istream_t in, fluid_ostream_t out);
static int fluid_handle_voice_count (fluid_synth_t *synth, int ac, char **av,
                                     fluid_ostream_t out);

void fluid_shell_settings(fluid_settings_t* settings)
{
  fluid_settings_register_str(settings, "shell.prompt", "", 0, NULL, NULL);
  fluid_settings_register_int(settings, "shell.port", 9800, 1, 65535, 0, NULL, NULL);
}


/** the table of all handled commands */

fluid_cmd_t fluid_commands[] = {
  { "help", "general", (fluid_cmd_func_t) fluid_handle_help, NULL,
    "help                       Show help topics ('help TOPIC' for more info)" },
  { "quit", "general", (fluid_cmd_func_t) fluid_handle_quit, NULL,
    "quit                       Quit the synthesizer" },
  { "noteon", "event", (fluid_cmd_func_t) fluid_handle_noteon, NULL,
    "noteon chan key vel        Send noteon" },
  { "noteoff", "event", (fluid_cmd_func_t) fluid_handle_noteoff, NULL,
    "noteoff chan key           Send noteoff"  },
  { "pitch_bend", "event", (fluid_cmd_func_t) fluid_handle_pitch_bend, NULL,
    "pitch_bend chan offset           Bend pitch"  },
  { "pitch_bend_range", "event", (fluid_cmd_func_t) fluid_handle_pitch_bend_range, NULL,
    "pitch_bend chan range           Set bend pitch range"  },
  { "cc", "event", (fluid_cmd_func_t) fluid_handle_cc, NULL,
    "cc chan ctrl value         Send control-change message" },
  { "prog", "event", (fluid_cmd_func_t) fluid_handle_prog, NULL,
    "prog chan num              Send program-change message" },
  { "select", "event", (fluid_cmd_func_t) fluid_handle_select, NULL,
    "select chan sfont bank prog  Combination of bank-select and program-change" },
  { "load", "general", (fluid_cmd_func_t) fluid_handle_load, NULL,
    "load file [reset] [bankofs] Load SoundFont (reset=0|1, def 1; bankofs=n, def 0)" },
  { "unload", "general", (fluid_cmd_func_t) fluid_handle_unload, NULL,
    "unload id [reset]          Unload SoundFont by ID (reset=0|1, default 1)"},
  { "reload", "general", (fluid_cmd_func_t) fluid_handle_reload, NULL,
    "reload id                  Reload the SoundFont with the specified ID" },
  { "fonts", "general", (fluid_cmd_func_t) fluid_handle_fonts, NULL,
    "fonts                      Display the list of loaded SoundFonts" },
  { "inst", "general", (fluid_cmd_func_t) fluid_handle_inst, NULL,
    "inst font                  Print out the available instruments for the font" },
  { "channels", "general", (fluid_cmd_func_t) fluid_handle_channels, NULL,
    "channels [-verbose]        Print out preset of all channels" },
  { "interp", "general", (fluid_cmd_func_t) fluid_handle_interp, NULL,
    "interp num                 Choose interpolation method for all channels" },
  { "interpc", "general", (fluid_cmd_func_t) fluid_handle_interpc, NULL,
    "interpc chan num           Choose interpolation method for one channel" },
  { "rev_preset", "reverb", (fluid_cmd_func_t) fluid_handle_reverbpreset, NULL,
    "rev_preset num             Load preset num into the reverb unit" },
  { "rev_setroomsize", "reverb", (fluid_cmd_func_t) fluid_handle_reverbsetroomsize, NULL,
    "rev_setroomsize num        Change reverb room size" },
  { "rev_setdamp", "reverb", (fluid_cmd_func_t) fluid_handle_reverbsetdamp, NULL,
    "rev_setdamp num            Change reverb damping" },
  { "rev_setwidth", "reverb", (fluid_cmd_func_t) fluid_handle_reverbsetwidth, NULL,
    "rev_setwidth num           Change reverb width" },
  { "rev_setlevel", "reverb", (fluid_cmd_func_t) fluid_handle_reverbsetlevel, NULL,
    "rev_setlevel num           Change reverb level" },
  { "reverb", "reverb", (fluid_cmd_func_t) fluid_handle_reverb, NULL,
    "reverb [0|1|on|off]        Turn the reverb on or off" },
  { "cho_set_nr", "chorus", (fluid_cmd_func_t) fluid_handle_chorusnr, NULL,
    "cho_set_nr n               Use n delay lines (default 3)" },
  { "cho_set_level", "chorus", (fluid_cmd_func_t) fluid_handle_choruslevel, NULL,
    "cho_set_level num          Set output level of each chorus line to num" },
  { "cho_set_speed", "chorus", (fluid_cmd_func_t) fluid_handle_chorusspeed, NULL,
    "cho_set_speed num          Set mod speed of chorus to num (Hz)" },
  { "cho_set_depth", "chorus", (fluid_cmd_func_t) fluid_handle_chorusdepth, NULL,
    "cho_set_depth num          Set chorus modulation depth to num (ms)" },
  { "chorus", "chorus", (fluid_cmd_func_t) fluid_handle_chorus, NULL,
    "chorus [0|1|on|off]        Turn the chorus on or off" },
  { "gain", "general", (fluid_cmd_func_t) fluid_handle_gain, NULL,
    "gain value                 Set the master gain (0 < gain < 5)" },
  { "voice_count", "general", (fluid_cmd_func_t) fluid_handle_voice_count, NULL,
    "voice_count                Get number of active synthesis voices" },
  { "tuning", "tuning", (fluid_cmd_func_t) fluid_handle_tuning, NULL,
    "tuning name bank prog      Create a tuning with name, bank number, \n"
    "                           and program number (0 <= bank,prog <= 127)" },
  { "tune", "tuning", (fluid_cmd_func_t) fluid_handle_tune, NULL,
    "tune bank prog key pitch   Tune a key" },
  { "settuning", "tuning", (fluid_cmd_func_t) fluid_handle_settuning, NULL,
    "settuning chan bank prog   Set the tuning for a MIDI channel" },
  { "resettuning", "tuning", (fluid_cmd_func_t) fluid_handle_resettuning, NULL,
    "resettuning chan           Restore the default tuning of a MIDI channel" },
  { "tunings", "tuning", (fluid_cmd_func_t) fluid_handle_tunings, NULL,
    "tunings                    Print the list of available tunings" },
  { "dumptuning", "tuning", (fluid_cmd_func_t) fluid_handle_dumptuning, NULL,
    "dumptuning bank prog       Print the pitch details of the tuning" },
  { "reset", "general", (fluid_cmd_func_t) fluid_handle_reset, NULL,
    "reset                      System reset (all notes off, reset controllers)" },
  { "set", "settings", (fluid_cmd_func_t) fluid_handle_set, NULL,
    "set name value             Set the value of a controller or settings" },
  { "get", "settings", (fluid_cmd_func_t) fluid_handle_get, NULL,
    "get name                   Get the value of a controller or settings" },
  { "info", "settings", (fluid_cmd_func_t) fluid_handle_info, NULL,
    "info name                  Get information about a controller or settings" },
  { "settings", "settings", (fluid_cmd_func_t) fluid_handle_settings, NULL,
    "settings                   Print out all settings" },
  { "echo", "general", (fluid_cmd_func_t) fluid_handle_echo, NULL,
    "echo arg                   Print arg" },
  /* LADSPA-related commands */
#ifdef LADSPA
  { "ladspa_clear", "ladspa", (fluid_cmd_func_t) fluid_LADSPA_handle_clear, NULL,
    "ladspa_clear               Resets LADSPA effect unit to bypass state"},
  { "ladspa_add", "ladspa", (fluid_cmd_func_t) fluid_LADSPA_handle_add, NULL,
    "ladspa_add lib plugin n1 <- p1 n2 -> p2 ... Loads and connects LADSPA plugin"},
  { "ladspa_start", "ladspa", (fluid_cmd_func_t) fluid_LADSPA_handle_start, NULL,
    "ladspa_start               Starts LADSPA effect unit"},
  { "ladspa_declnode", "ladspa", (fluid_cmd_func_t) fluid_LADSPA_handle_declnode, NULL,
    "ladspa_declnode node value Declares control node `node' with value `value'"},
  { "ladspa_setnode", "ladspa", (fluid_cmd_func_t) fluid_LADSPA_handle_setnode, NULL,
    "ladspa_setnode node value  Assigns `value' to `node'"},
#endif
  { "router_clear", "router", (fluid_cmd_func_t) fluid_midi_router_handle_clear, NULL,
    "router_clear               Clears all routing rules from the midi router"},
  { "router_default", "router", (fluid_cmd_func_t) fluid_midi_router_handle_default, NULL,
    "router_default             Resets the midi router to default state"},
  { "router_begin", "router", (fluid_cmd_func_t) fluid_midi_router_handle_begin, NULL,
    "router_begin [note|cc|prog|pbend|cpress|kpress]: Starts a new routing rule"},
  { "router_chan", "router", (fluid_cmd_func_t) fluid_midi_router_handle_chan, NULL,
    "router_chan min max mul add      filters and maps midi channels on current rule"},
  { "router_par1", "router", (fluid_cmd_func_t) fluid_midi_router_handle_par1, NULL,
    "router_par1 min max mul add      filters and maps parameter 1 (key/ctrl nr)"},
  { "router_par2", "router", (fluid_cmd_func_t) fluid_midi_router_handle_par2, NULL,
    "router_par2 min max mul add      filters and maps parameter 2 (vel/cc val)"},
  { "router_end", "router", (fluid_cmd_func_t) fluid_midi_router_handle_end, NULL,
    "router_end                 closes and commits the current routing rule"},
  { NULL, NULL, NULL, NULL, NULL }
};

/**
 * Process a string command.
 * NOTE: FluidSynth 1.0.8 and above no longer modifies the 'cmd' string.
 * @param handler FluidSynth command handler
 * @param cmd Command string (NOTE: Gets modified by FluidSynth prior to 1.0.8)
 * @param out Output stream to display command response to
 * @return Integer value corresponding to: -1 on command error, 0 on success,
 *   1 if 'cmd' is a comment or is empty and -2 if quit was issued
 */
int
fluid_command(fluid_cmd_handler_t* handler, const char *cmd, fluid_ostream_t out)
{
  int result, num_tokens = 0;
  char** tokens = NULL;

  if (cmd[0] == '#' || cmd[0] == '\0') {
    return 1;
  }

/*  if (!g_shell_parse_argv(cmd, &num_tokens, &tokens, NULL)) {
    fluid_ostream_printf(out, "Error parsing command\n");
    return -1;
  }
  */
  result = fluid_cmd_handler_handle(handler, num_tokens, &tokens[0], out);
//  g_strfreev(tokens);

  return result;
}

/**
 * Create a new FluidSynth command shell.
 * @param settings Setting parameters to use with the shell
 * @param handler Command handler
 * @param in Input stream
 * @param out Output stream
 * @param thread TRUE if shell should be run in a separate thread, FALSE to run
 *   it in the current thread (function blocks until "quit")
 * @return New shell instance or NULL on error
 */
fluid_shell_t *
new_fluid_shell(fluid_settings_t* settings, fluid_cmd_handler_t* handler,
                fluid_istream_t in, fluid_ostream_t out, int thread)
{
  fluid_shell_t* shell = FLUID_NEW(fluid_shell_t);
  if (shell == NULL) {
    FLUID_LOG (FLUID_PANIC, "Out of memory");
    return NULL;
  }


  fluid_shell_init(shell, settings, handler, in, out);

/*  if (thread) {
    shell->thread = new_fluid_thread((fluid_thread_func_t) fluid_shell_run, shell, 0, TRUE);
    if (shell->thread == NULL) {
      delete_fluid_shell(shell);
      return NULL;
    }
  } else {
  */
    shell->thread = NULL;
    fluid_shell_run(shell);
  /*}*/

  return shell;
}

static void
fluid_shell_init(fluid_shell_t* shell,
		 fluid_settings_t* settings, fluid_cmd_handler_t* handler,
		 fluid_istream_t in, fluid_ostream_t out)
{
  shell->settings = settings;
  shell->handler = handler;
  shell->in = in;
  shell->out = out;
}

/**
 * Delete a FluidSynth command shell.
 * @param shell Command shell instance
 */
void
delete_fluid_shell(fluid_shell_t* shell)
{
  if (shell->thread != NULL) {
    delete_fluid_thread(shell->thread);
  }

  FLUID_FREE(shell);
}

static int
fluid_shell_run(fluid_shell_t* shell)
{
  char workline[FLUID_WORKLINELENGTH];
  char* prompt = NULL;
  int cont = 1;
  int errors = 0;
  int n;

  if (shell->settings)
    fluid_settings_dupstr(shell->settings, "shell.prompt", &prompt);    /* ++ alloc prompt */

  /* handle user input */
  while (cont) {

    n = fluid_istream_readline(shell->in, shell->out, prompt ? prompt : "", workline, FLUID_WORKLINELENGTH);

    if (n < 0) {
      break;
    }

#if WITH_READLINE
    if (shell->in == fluid_get_stdin()) {
      add_history(workline);
    }
#endif

    /* handle the command */
    switch (fluid_command(shell->handler, workline, shell->out)) {

    case 1: /* empty line or comment */
      break;

    case -1: /* erronous command */
      errors++;
    case 0: /* valid command */
      break;

    case -2: /* quit */
      cont = 0;
      break;
    }

    if (n == 0) {
       break;
    }
  }

  if (prompt) FLUID_FREE (prompt);      /* -- free prompt */

  return errors;
}

/**
 * A convenience function to create a shell interfacing to standard input/output
 * console streams.
 * @param settings Settings instance for the shell
 * @param handler Command handler callback
 */
void
fluid_usershell(fluid_settings_t* settings, fluid_cmd_handler_t* handler)
{
  fluid_shell_t shell;
  fluid_shell_init(&shell, settings, handler, fluid_get_stdin(), fluid_get_stdout());
  fluid_shell_run(&shell);
}

/**
 * Execute shell commands in a file.
 * @param handler Command handler callback
 * @param filename File name
 * @return 0 on success, a value >1 on error
 */
int
fluid_source(fluid_cmd_handler_t* handler, const char *filename)
{
  int file;
  fluid_shell_t shell;

#ifdef WIN32
  file = _open(filename, _O_RDONLY);
#else
  file = open(filename, O_RDONLY);
#endif
  if (file < 0) {
    return file;
  }
  fluid_shell_init(&shell, NULL, handler, file, fluid_get_stdout());
  return fluid_shell_run(&shell);
}

/**
 * Get the user specific FluidSynth command file name.
 * @param buf Caller supplied string buffer to store file name to.
 * @param len Length of \a buf
 * @return Returns \a buf pointer or NULL if no user command file for this system type.
 */
char*
fluid_get_userconf(char* buf, int len)
{
#if defined(WIN32) || defined(MACOS9)
  return NULL;
#else
  char* home = getenv("HOME");
  if (home == NULL) {
    return NULL;
  } else {
    snprintf(buf, len, "%s/.fluidsynth", home);
    return buf;
  }
#endif
}

/**
 * Get the system FluidSynth command file name.
 * @param buf Caller supplied string buffer to store file name to.
 * @param len Length of \a buf
 * @return Returns \a buf pointer or NULL if no system command file for this system type.
 */
char*
fluid_get_sysconf(char* buf, int len)
{
#if defined(WIN32) || defined(MACOS9)
  return NULL;
#else
  snprintf(buf, len, "/etc/fluidsynth.conf");
  return buf;
#endif
}


/*
 *  handlers
 */
int
fluid_handle_noteon(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out)
{
  if (ac < 3) {
    fluid_ostream_printf(out, "noteon: too few arguments\n");
    return -1;
  }
  if (!fluid_is_number(av[0]) || !fluid_is_number(av[1]) || !fluid_is_number(av[2])) {
    fluid_ostream_printf(out, "noteon: invalid argument\n");
    return -1;
  }
  return fluid_synth_noteon(synth, atoi(av[0]), atoi(av[1]), atoi(av[2]));
}

int
fluid_handle_noteoff(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out)
{
  if (ac < 2) {
    fluid_ostream_printf(out, "noteoff: too few arguments\n");
    return -1;
  }
  if (!fluid_is_number(av[0]) || !fluid_is_number(av[1])) {
    fluid_ostream_printf(out, "noteon: invalid argument\n");
    return -1;
  }
  return fluid_synth_noteoff(synth, atoi(av[0]), atoi(av[1]));
}

int
fluid_handle_pitch_bend(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out)
{
  if (ac < 2) {
    fluid_ostream_printf(out, "pitch_bend: too few arguments\n");
    return -1;
  }
  if (!fluid_is_number(av[0]) || !fluid_is_number(av[1])) {
    fluid_ostream_printf(out, "pitch_bend: invalid argument\n");
    return -1;
  }
  return fluid_synth_pitch_bend(synth, atoi(av[0]), atoi(av[1]));
}

int
fluid_handle_pitch_bend_range(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out)
{
  int channum;
  int value;
  if (ac < 2) {
    fluid_ostream_printf(out, "pitch_bend_range: too few arguments\n");
    return -1;
  }
  if (!fluid_is_number(av[0]) || !fluid_is_number(av[1])) {
    fluid_ostream_printf(out, "pitch_bend_range: invalid argument\n");
    return -1;
  }
  channum = atoi(av[0]);
  value = atoi(av[1]);
  fluid_channel_set_pitch_wheel_sensitivity(synth->channel[channum], value);
  return 0;
}

int
fluid_handle_cc(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out)
{
  if (ac < 3) {
    fluid_ostream_printf(out, "cc: too few arguments\n");
    return -1;
  }
  if (!fluid_is_number(av[0]) || !fluid_is_number(av[1]) || !fluid_is_number(av[2])) {
    fluid_ostream_printf(out, "cc: invalid argument\n");
    return -1;
  }
  return fluid_synth_cc(synth, atoi(av[0]), atoi(av[1]), atoi(av[2]));
}

int
fluid_handle_prog(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out)
{
  if (ac < 2) {
    fluid_ostream_printf(out, "prog: too few arguments\n");
    return -1;
  }
  if (!fluid_is_number(av[0]) || !fluid_is_number(av[1])) {
    fluid_ostream_printf(out, "prog: invalid argument\n");
    return -1;
  }
  return fluid_synth_program_change(synth, atoi(av[0]), atoi(av[1]));
}

int
fluid_handle_select(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out)
{
  int sfont_id;
  int chan;
  int bank;
  int prog;

  if (ac < 4) {
    fluid_ostream_printf(out, "preset: too few arguments\n");
    return -1;
  }
  if (!fluid_is_number(av[0]) || !fluid_is_number(av[1])
      || !fluid_is_number(av[2]) || !fluid_is_number(av[3])) {
    fluid_ostream_printf(out, "preset: invalid argument\n");
    return -1;
  }

  chan = atoi(av[0]);
  sfont_id = atoi(av[1]);
  bank = atoi(av[2]);
  prog = atoi(av[3]);

  if (sfont_id != 0) {
    return fluid_synth_program_select(synth, chan, sfont_id, bank, prog);
  } else {
    if (fluid_synth_bank_select(synth, chan, bank) == FLUID_OK) {
      return fluid_synth_program_change(synth, chan, prog);
    }
    return FLUID_FAILED;
  }
}

int
fluid_handle_inst(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out)
{
  int font;
  fluid_sfont_t* sfont;
  fluid_preset_t preset;
  int offset;

  if (ac < 1) {
    fluid_ostream_printf(out, "inst: too few arguments\n");
    return -1;
  }

  if (!fluid_is_number(av[0])) {
    fluid_ostream_printf(out, "inst: invalid argument\n");
    return -1;
  }

  font = atoi(av[0]);

  sfont = fluid_synth_get_sfont_by_id(synth, font);
  offset = fluid_synth_get_bank_offset(synth, font);

  if (sfont == NULL) {
    fluid_ostream_printf(out, "inst: invalid font number\n");
    return -1;
  }

  fluid_sfont_iteration_start(sfont);

  while (fluid_sfont_iteration_next(sfont, &preset)) {
    fluid_ostream_printf(out, "%03d-%03d %s\n",
			fluid_preset_get_banknum(&preset) + offset,
			fluid_preset_get_num(&preset),
			fluid_preset_get_name(&preset));
  }

  return 0;
}


int
fluid_handle_channels(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out)
{
  fluid_synth_channel_info_t info;
  int verbose = 0;
  int i;

  if (ac > 0 && strcmp( av[0], "-verbose") == 0) verbose = 1;

  for (i = 0; i < fluid_synth_count_midi_channels (synth); i++)
  {
    fluid_synth_get_channel_info (synth, i, &info);

    if (!verbose)
      fluid_ostream_printf (out, "chan %d, %s\n", i,
                            info.assigned ? info.name : "no preset");
    else
      fluid_ostream_printf (out, "chan %d, sfont %d, bank %d, preset %d, %s\n", i,
                            info.sfont_id, info.bank, info.program,
                            info.assigned ? info.name : "no preset");
  }

  return 0;
}

int
fluid_handle_load(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out)
{
  char buf[1024];
  int id;
  int reset = 1;
  int offset = 0;

  if (ac < 1) {
    fluid_ostream_printf(out, "load: too few arguments\n");
    return -1;
  }
  if (ac == 2) {
    reset = atoi(av[1]);
  }
  if (ac == 3) {
    offset = atoi(av[2]);
  }

  /* Load the SoundFont without resetting the programs. The reset will
   * be done later (if requested). */
  id = fluid_synth_sfload(synth, fluid_expand_path(av[0], buf, 1024), 0);

  if (id == -1) {
    fluid_ostream_printf(out, "failed to load the SoundFont\n");
    return -1;
  } else {
    fluid_ostream_printf(out, "loaded SoundFont has ID %d\n", id);
  }

  if (offset) {
    fluid_synth_set_bank_offset(synth, id, offset);
  }

  /* The reset should be done after the offset is set. */
  if (reset) {
    fluid_synth_program_reset(synth);
  }

  return 0;
}

int
fluid_handle_unload(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out)
{
  int reset = 1;
  if (ac < 1) {
    fluid_ostream_printf(out, "unload: too few arguments\n");
    return -1;
  }
  if (!fluid_is_number(av[0])) {
    fluid_ostream_printf(out, "unload: expected a number as argument\n");
    return -1;
  }
  if (ac == 2) {
    reset = atoi(av[1]);
  }
  if (fluid_synth_sfunload(synth, atoi(av[0]), reset) != 0) {
    fluid_ostream_printf(out, "failed to unload the SoundFont\n");
    return -1;
  }
  return 0;
}

int
fluid_handle_reload(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out)
{
  if (ac < 1) {
    fluid_ostream_printf(out, "reload: too few arguments\n");
    return -1;
  }
  if (!fluid_is_number(av[0])) {
    fluid_ostream_printf(out, "reload: expected a number as argument\n");
    return -1;
  }
  if (fluid_synth_sfreload(synth, atoi(av[0])) == -1) {
    fluid_ostream_printf(out, "failed to reload the SoundFont\n");
    return -1;
  }
  return 0;
}


int
fluid_handle_fonts(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out)
{
  int i;
  fluid_sfont_t* sfont;
  int num;

  num = fluid_synth_sfcount(synth);

  if (num == 0) {
    fluid_ostream_printf(out, "no SoundFont loaded (try load)\n");
    return 0;
  }

  fluid_ostream_printf(out, "ID  Name\n");

  for (i = 0; i < num; i++) {
    sfont = fluid_synth_get_sfont(synth, i);
    fluid_ostream_printf(out, "%2d  %s\n",
			fluid_sfont_get_id(sfont),
			fluid_sfont_get_name(sfont));
  }

  return 0;
}

int
fluid_handle_mstat(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out)
{
/*    fluid_ostream_printf(out, "Dvr=%s, Dev=%s\n",  */
/*  	 fluid_midi_handler_get_driver_name(midi), */
/*  	 fluid_midi_handler_get_device_name(midi)); */
/*    fluid_ostream_printf(out, "Stat=%s, On=%d, Off=%d, Prog=%d, Pbend=%d, Err=%d\n",  */
/*  	 fluid_midi_handler_get_status(midi), */
/*  	 fluid_midi_handler_get_event_count(midi, 0x90), */
/*  	 fluid_midi_handler_get_event_count(midi, 0x80), */
/*  	 fluid_midi_handler_get_event_count(midi, 0xc0), */
/*  	 fluid_midi_handler_get_event_count(midi, 0xe0), */
/*  	 fluid_midi_handler_get_event_count(midi, 0)); */
  fluid_ostream_printf(out, "not yet implemented\n");
  return -1;
}

/* Purpose:
 * Response to 'rev_preset' command.
 * Load the values from a reverb preset into the reverb unit. */
int
fluid_handle_reverbpreset(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out)
{
  int reverb_preset_number;
  if (ac < 1) {
    fluid_ostream_printf(out, "rev_preset: too few arguments\n");
    return -1;
  }
  reverb_preset_number = atoi(av[0]);
  if (fluid_synth_set_reverb_preset(synth, reverb_preset_number)!=FLUID_OK){
    fluid_ostream_printf(out, "rev_preset: Failed. Parameter out of range?\n");
    return -1;
  };
  return 0;
}

/* Purpose:
 * Response to 'rev_setroomsize' command.
 * Load the new room size into the reverb unit. */
int
fluid_handle_reverbsetroomsize(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out)
{
  fluid_real_t room_size;
  if (ac < 1) {
    fluid_ostream_printf(out, "rev_setroomsize: too few arguments.\n");
    return -1;
  }
  room_size = atof(av[0]);
  if (room_size < 0){
    fluid_ostream_printf(out, "rev_setroomsize: Room size must be positive!\n");
    return -1;
  }
  if (room_size > 1.2){
    fluid_ostream_printf(out, "rev_setroomsize: Room size too big!\n");
    return -1;
  }
  fluid_synth_set_reverb_full (synth, FLUID_REVMODEL_SET_ROOMSIZE,
                               room_size, 0.0, 0.0, 0.0);
  return 0;
}

/* Purpose:
 * Response to 'rev_setdamp' command.
 * Load the new damp factor into the reverb unit. */
int
fluid_handle_reverbsetdamp(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out)
{
  fluid_real_t damp;
  if (ac < 1) {
    fluid_ostream_printf(out, "rev_setdamp: too few arguments.\n");
    return -1;
  }
  damp = atof(av[0]);
  if ((damp < 0.0f) || (damp > 1)){
    fluid_ostream_printf(out, "rev_setdamp: damp must be between 0 and 1!\n");
    return -1;
  }
  fluid_synth_set_reverb_full (synth, FLUID_REVMODEL_SET_DAMPING,
                               0.0, damp, 0.0, 0.0);
  return 0;
}

/* Purpose:
 * Response to 'rev_setwidth' command.
 * Load the new width into the reverb unit. */
int
fluid_handle_reverbsetwidth(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out)
{
  fluid_real_t width;
  if (ac < 1) {
    fluid_ostream_printf(out, "rev_setwidth: too few arguments.\n");
    return -1;
  }
  width = atof(av[0]);
  if ((width < 0) || (width > 100)){
    fluid_ostream_printf(out, "rev_setroomsize: Too wide! (0..100)\n");
    return 0;
  }
  fluid_synth_set_reverb_full (synth, FLUID_REVMODEL_SET_WIDTH,
                               0.0, 0.0, width, 0.0);
  return 0;
}

/* Purpose:
 * Response to 'rev_setlevel' command.
 * Load the new level into the reverb unit. */
int
fluid_handle_reverbsetlevel(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out)
{
  fluid_real_t level;
  if (ac < 1) {
    fluid_ostream_printf(out, "rev_setlevel: too few arguments.\n");
    return -1;
  }
  level = atof(av[0]);
  if (abs(level) > 30){
    fluid_ostream_printf(out, "rev_setlevel: Value too high! (Value of 10 =+20 dB)\n");
    return 0;
  }
  fluid_synth_set_reverb_full (synth, FLUID_REVMODEL_SET_LEVEL,
                               0.0, 0.0, 0.0, level);
  return 0;
}

/* Purpose:
 * Response to 'reverb' command.
 * Change the FLUID_REVERB flag in the synth */
int
fluid_handle_reverb(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out)
{
  if (ac < 1) {
    fluid_ostream_printf(out, "reverb: too few arguments.\n");
    return -1;
  }

  if ((strcmp(av[0], "0") == 0) || (strcmp(av[0], "off") == 0)) {
    fluid_synth_set_reverb_on(synth,0);
  } else if ((strcmp(av[0], "1") == 0) || (strcmp(av[0], "on") == 0)) {
    fluid_synth_set_reverb_on(synth,1);
  } else {
    fluid_ostream_printf(out, "reverb: invalid arguments %s [0|1|on|off]", av[0]);
    return -1;
  }

  return 0;
}


/* Purpose:
 * Response to 'chorus_setnr' command */
int
fluid_handle_chorusnr(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out)
{
  int nr;
  if (ac < 1) {
    fluid_ostream_printf(out, "cho_set_nr: too few arguments.\n");
    return -1;
  }
  nr = atoi(av[0]);
  return fluid_synth_set_chorus_full (synth, FLUID_CHORUS_SET_NR, nr, 0.0, 0.0, 0.0, 0);
}

/* Purpose:
 * Response to 'chorus_setlevel' command */
int
fluid_handle_choruslevel(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out)
{
  fluid_real_t level;
  if (ac < 1) {
    fluid_ostream_printf(out, "cho_set_level: too few arguments.\n");
    return -1;
  }
  level = atof(av[0]);
  return fluid_synth_set_chorus_full (synth, FLUID_CHORUS_SET_LEVEL, 0, level, 0.0, 0.0, 0);
}

/* Purpose:
 * Response to 'chorus_setspeed' command */
int
fluid_handle_chorusspeed(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out)
{
  fluid_real_t speed;
  if (ac < 1) {
    fluid_ostream_printf(out, "cho_set_speed: too few arguments.\n");
    return -1;
  }
  speed = atof(av[0]);
  return fluid_synth_set_chorus_full (synth, FLUID_CHORUS_SET_SPEED, 0, 0.0, speed, 0.0, 0);
}

/* Purpose:
 * Response to 'chorus_setdepth' command */
int
fluid_handle_chorusdepth(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out)
{
  fluid_real_t depth;
  if (ac < 1) {
    fluid_ostream_printf(out, "cho_set_depth: too few arguments.\n");
    return -1;
  }
  depth = atof(av[0]);
  return fluid_synth_set_chorus_full (synth, FLUID_CHORUS_SET_DEPTH, 0, 0.0, 0.0, depth, 0);
}

int
fluid_handle_chorus(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out)
{
  if (ac < 1) {
    fluid_ostream_printf(out, "chorus: too few arguments\n");
    return -1;
  }

  if ((strcmp(av[0], "0") == 0) || (strcmp(av[0], "off") == 0)) {
    fluid_synth_set_chorus_on(synth,0);
  } else if ((strcmp(av[0], "1") == 0) || (strcmp(av[0], "on") == 0)) {
    fluid_synth_set_chorus_on(synth,1);
  } else {
    fluid_ostream_printf(out, "chorus: invalid arguments %s [0|1|on|off]", av[0]);
    return -1;
  }

  return 0;
}

/* Purpose:
 * Response to the 'echo' command.
 * The command itself is useful, when the synth is used via TCP/IP.
 * It can signal for example, that a list of commands has been processed.
 */
int
fluid_handle_echo(fluid_cmd_handler_t* handler, int ac, char** av, fluid_ostream_t out)
{
  if (ac < 1) {
    fluid_ostream_printf(out, "echo: too few arguments.\n");
    return -1;
  }

  fluid_ostream_printf(out, "%s\n",av[0]);

  return 0;
}

int
fluid_handle_source(fluid_cmd_handler_t* handler, int ac, char** av, fluid_ostream_t out)
{
  if (ac < 1) {
    fluid_ostream_printf(out, "source: too few arguments.\n");
    return -1;
  }

  fluid_source(handler, av[0]);

  return 0;
}

/* Purpose:
 * Response to 'gain' command. */
int
fluid_handle_gain(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out)
{
  float gain;

  if (ac < 1) {
    fluid_ostream_printf(out, "gain: too few arguments.\n");
    return -1;
  }

  gain = atof(av[0]);

  if ((gain < 0.0f) || (gain > 5.0f)) {
    fluid_ostream_printf(out, "gain: value should be between '0' and '5'.\n");
    return -1;
  };

  fluid_synth_set_gain(synth, gain);

  return 0;
}

/* Response to voice_count command */
static int
fluid_handle_voice_count (fluid_synth_t *synth, int ac, char **av,
                          fluid_ostream_t out)
{
  fluid_ostream_printf (out, "voice_count: %d\n",
                        fluid_synth_get_active_voice_count (synth));
  return FLUID_OK;
}

/* Purpose:
 * Response to 'interp' command. */
int
fluid_handle_interp(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out)
{
  int interp;
  int chan=-1; /* -1: Set all channels */

  if (ac < 1) {
    fluid_ostream_printf(out, "interp: too few arguments.\n");
    return -1;
  }

  interp = atoi(av[0]);

  if ((interp < 0) || (interp > FLUID_INTERP_HIGHEST)) {
    fluid_ostream_printf(out, "interp: Bad value\n");
    return -1;
  };

  fluid_synth_set_interp_method(synth, chan, interp);

  return 0;
}

/* Purpose:
 * Response to 'interp' command. */
int
fluid_handle_interpc(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out)
{
  int interp;
  int chan;

  if (ac < 2) {
    fluid_ostream_printf(out, "interpc: too few arguments.\n");
    return -1;
  }

  chan = atoi(av[0]);
  interp = atoi(av[1]);

  if ((chan < 0) || (chan >= fluid_synth_count_midi_channels(synth))){
    fluid_ostream_printf(out, "interp: Bad value for channel number.\n");
    return -1;
  };
  if ((interp < 0) || (interp > FLUID_INTERP_HIGHEST)) {
    fluid_ostream_printf(out, "interp: Bad value for interpolation method.\n");
    return -1;
  };

  fluid_synth_set_interp_method(synth, chan, interp);

  return 0;
}

int
fluid_handle_tuning(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out)
{
  char *name;
  int bank, prog;

  if (ac < 3) {
    fluid_ostream_printf(out, "tuning: too few arguments.\n");
    return -1;
  }

  name = av[0];

  if (!fluid_is_number(av[1])) {
    fluid_ostream_printf(out, "tuning: 2nd argument should be a number.\n");
    return -1;
  }
  bank = atoi(av[1]);
  if ((bank < 0) || (bank >= 128)){
    fluid_ostream_printf(out, "tuning: invalid bank number.\n");
    return -1;
  };

  if (!fluid_is_number(av[2])) {
    fluid_ostream_printf(out, "tuning: 3rd argument should be a number.\n");
    return -1;
  }
  prog = atoi(av[2]);
  if ((prog < 0) || (prog >= 128)){
    fluid_ostream_printf(out, "tuning: invalid program number.\n");
    return -1;
  };

  fluid_synth_create_key_tuning(synth, bank, prog, name, NULL);

  return 0;
}

int
fluid_handle_tune(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out)
{
  int bank, prog, key;
  double pitch;

  if (ac < 4) {
    fluid_ostream_printf(out, "tune: too few arguments.\n");
    return -1;
  }

  if (!fluid_is_number(av[0])) {
    fluid_ostream_printf(out, "tune: 1st argument should be a number.\n");
    return -1;
  }
  bank = atoi(av[0]);
  if ((bank < 0) || (bank >= 128)){
    fluid_ostream_printf(out, "tune: invalid bank number.\n");
    return -1;
  };

  if (!fluid_is_number(av[1])) {
    fluid_ostream_printf(out, "tune: 2nd argument should be a number.\n");
    return -1;
  }
  prog = atoi(av[1]);
  if ((prog < 0) || (prog >= 128)){
    fluid_ostream_printf(out, "tune: invalid program number.\n");
    return -1;
  };

  if (!fluid_is_number(av[2])) {
    fluid_ostream_printf(out, "tune: 3rd argument should be a number.\n");
    return -1;
  }
  key = atoi(av[2]);
  if ((key < 0) || (key >= 128)){
    fluid_ostream_printf(out, "tune: invalid key number.\n");
    return -1;
  };

  pitch = atof(av[3]);
  if (pitch < 0.0f) {
    fluid_ostream_printf(out, "tune: invalid pitch.\n");
    return -1;
  };

  fluid_synth_tune_notes(synth, bank, prog, 1, &key, &pitch, 0);

  return 0;
}

int
fluid_handle_settuning(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out)
{
  int chan, bank, prog;

  if (ac < 3) {
    fluid_ostream_printf(out, "settuning: too few arguments.\n");
    return -1;
  }

  if (!fluid_is_number(av[0])) {
    fluid_ostream_printf(out, "tune: 1st argument should be a number.\n");
    return -1;
  }
  chan = atoi(av[0]);
  if ((chan < 0) || (chan >= fluid_synth_count_midi_channels(synth))){
    fluid_ostream_printf(out, "tune: invalid channel number.\n");
    return -1;
  };

  if (!fluid_is_number(av[1])) {
    fluid_ostream_printf(out, "tuning: 2nd argument should be a number.\n");
    return -1;
  }
  bank = atoi(av[1]);
  if ((bank < 0) || (bank >= 128)){
    fluid_ostream_printf(out, "tuning: invalid bank number.\n");
    return -1;
  };

  if (!fluid_is_number(av[2])) {
    fluid_ostream_printf(out, "tuning: 3rd argument should be a number.\n");
    return -1;
  }
  prog = atoi(av[2]);
  if ((prog < 0) || (prog >= 128)){
    fluid_ostream_printf(out, "tuning: invalid program number.\n");
    return -1;
  };

  fluid_synth_select_tuning(synth, chan, bank, prog);

  return 0;
}

int
fluid_handle_resettuning(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out)
{
  int chan;

  if (ac < 1) {
    fluid_ostream_printf(out, "resettuning: too few arguments.\n");
    return -1;
  }

  if (!fluid_is_number(av[0])) {
    fluid_ostream_printf(out, "tune: 1st argument should be a number.\n");
    return -1;
  }
  chan = atoi(av[0]);
  if ((chan < 0) || (chan >= fluid_synth_count_midi_channels(synth))){
    fluid_ostream_printf(out, "tune: invalid channel number.\n");
    return -1;
  };

  fluid_synth_reset_tuning(synth, chan);

  return 0;
}

int
fluid_handle_tunings(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out)
{
  int bank, prog;
  char name[256];
  int count = 0;

  fluid_synth_tuning_iteration_start(synth);

  while (fluid_synth_tuning_iteration_next(synth, &bank, &prog)) {
    fluid_synth_tuning_dump(synth, bank, prog, name, 256, NULL);
    fluid_ostream_printf(out, "%03d-%03d %s\n", bank, prog, name);
    count++;
  }

  if (count == 0) {
    fluid_ostream_printf(out, "No tunings available\n");
  }

  return 0;
}

int
fluid_handle_dumptuning(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out)
{
  int bank, prog, i;
  double pitch[128];
  char name[256];

  if (ac < 2) {
    fluid_ostream_printf(out, "dumptuning: too few arguments.\n");
    return -1;
  }

  if (!fluid_is_number(av[0])) {
    fluid_ostream_printf(out, "dumptuning: 1st argument should be a number.\n");
    return -1;
  }
  bank = atoi(av[0]);
  if ((bank < 0) || (bank >= 128)){
    fluid_ostream_printf(out, "dumptuning: invalid bank number.\n");
    return -1;
  };

  if (!fluid_is_number(av[1])) {
    fluid_ostream_printf(out, "dumptuning: 2nd argument should be a number.\n");
    return -1;
  }
  prog = atoi(av[1]);
  if ((prog < 0) || (prog >= 128)){
    fluid_ostream_printf(out, "dumptuning: invalid program number.\n");
    return -1;
  };

  fluid_synth_tuning_dump(synth, bank, prog, name, 256, pitch);

  fluid_ostream_printf(out, "%03d-%03d %s:\n", bank, prog, name);

  for (i = 0; i < 128; i++) {
    fluid_ostream_printf(out, "key %03d, pitch %5.2f\n", i, pitch[i]);
  }

  return 0;
}

int
fluid_handle_set(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out)
{
  int hints;
  int ival;

  if (ac < 2) {
    fluid_ostream_printf(out, "set: Too few arguments.\n");
    return -1;
  }

  switch (fluid_settings_get_type (synth->settings, av[0]))
  {
    case FLUID_NO_TYPE:
      fluid_ostream_printf (out, "set: Parameter '%s' not found.\n", av[0]);
      break;
    case FLUID_INT_TYPE:
      hints = fluid_settings_get_hints (synth->settings, av[0]);

      if (hints & FLUID_HINT_TOGGLED)
      {
        if (FLUID_STRCMP (av[1], "yes") == 0 || FLUID_STRCMP (av[1], "True") == 0
            || FLUID_STRCMP (av[1], "TRUE") == 0 || FLUID_STRCMP (av[1], "true") == 0
            || FLUID_STRCMP (av[1], "T") == 0)
          ival = 1;
        else ival = atoi (av[1]);
      }
      else ival = atoi (av[1]);

      fluid_synth_setint (synth, av[0], ival);
      break;
    case FLUID_NUM_TYPE:
      fluid_synth_setnum (synth, av[0], atof (av[1]));
      break;
    case FLUID_STR_TYPE:
      fluid_synth_setstr(synth, av[0], av[1]);
      break;
    case FLUID_SET_TYPE:
      fluid_ostream_printf (out, "set: Parameter '%s' is a node.\n", av[0]);
      break;
  }

  return 0;
}

int
fluid_handle_get(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out)
{
  if (ac < 1) {
    fluid_ostream_printf(out, "get: too few arguments.\n");
    return -1;
  }

  switch (fluid_settings_get_type(fluid_synth_get_settings(synth), av[0])) {
  case FLUID_NO_TYPE:
    fluid_ostream_printf(out, "get: no such setting '%s'.\n", av[0]);
    return -1;

  case FLUID_NUM_TYPE: {
    double value;
    fluid_synth_getnum(synth, av[0], &value);
    fluid_ostream_printf(out, "%.3f", value);
    break;
  }

  case FLUID_INT_TYPE: {
    int value;
    fluid_synth_getint(synth, av[0], &value);
    fluid_ostream_printf(out, "%d", value);
    break;
  }

  case FLUID_STR_TYPE: {
    char* s;
    fluid_synth_dupstr(synth, av[0], &s);       /* ++ alloc string */
    fluid_ostream_printf(out, "%s", s ? s : "NULL");
    if (s) FLUID_FREE (s);      /* -- free string */
    break;
  }

  case FLUID_SET_TYPE:
    fluid_ostream_printf(out, "%s is a node", av[0]);
    break;
  }

  return 0;
}

struct _fluid_handle_settings_data_t {
  int len;
  fluid_synth_t* synth;
  fluid_ostream_t out;
};

static void fluid_handle_settings_iter1(void* data, char* name, int type)
{
  struct _fluid_handle_settings_data_t* d = (struct _fluid_handle_settings_data_t*) data;

  int len = FLUID_STRLEN(name);
  if (len > d->len) {
    d->len = len;
  }
}

static void fluid_handle_settings_iter2(void* data, char* name, int type)
{
  struct _fluid_handle_settings_data_t* d = (struct _fluid_handle_settings_data_t*) data;

  int len = FLUID_STRLEN(name);
  fluid_ostream_printf(d->out, "%s", name);
  while (len++ < d->len) {
    fluid_ostream_printf(d->out, " ");
  }
  fluid_ostream_printf(d->out, "   ");

  switch (fluid_settings_get_type(fluid_synth_get_settings(d->synth), name)) {
  case FLUID_NUM_TYPE: {
    double value;
    fluid_synth_getnum(d->synth, name, &value);
    fluid_ostream_printf(d->out, "%.3f\n", value);
    break;
  }

  case FLUID_INT_TYPE: {
    int value, hints;
    fluid_synth_getint(d->synth, name, &value);
    hints = fluid_settings_get_hints (d->synth->settings, name);

    if (!(hints & FLUID_HINT_TOGGLED))
      fluid_ostream_printf(d->out, "%d\n", value);
    else fluid_ostream_printf(d->out, "%s\n", value ? "True" : "False");
    break;
  }

  case FLUID_STR_TYPE: {
    char* s;
    fluid_synth_dupstr(d->synth, name, &s);     /* ++ alloc string */
    fluid_ostream_printf(d->out, "%s\n", s ? s : "NULL");
    if (s) FLUID_FREE (s);      /* -- free string */
    break;
  }
  }
}

int
fluid_handle_settings(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out)
{
  struct _fluid_handle_settings_data_t data;

  data.len = 0;
  data.synth = synth;
  data.out = out;

  fluid_settings_foreach(fluid_synth_get_settings(synth), &data, fluid_handle_settings_iter1);
  fluid_settings_foreach(fluid_synth_get_settings(synth), &data, fluid_handle_settings_iter2);
  return 0;
}


struct _fluid_handle_option_data_t {
  int first;
  fluid_ostream_t out;
};

void fluid_handle_print_option(void* data, char* name, char* option)
{
  struct _fluid_handle_option_data_t* d = (struct _fluid_handle_option_data_t*) data;

  if (d->first) {
    fluid_ostream_printf(d->out, "%s", option);
    d->first = 0;
  } else {
    fluid_ostream_printf(d->out, ", %s", option);
  }
}

int
fluid_handle_info(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out)
{
  fluid_settings_t* settings = fluid_synth_get_settings(synth);
  struct _fluid_handle_option_data_t data;

  if (ac < 1) {
    fluid_ostream_printf(out, "info: too few arguments.\n");
    return -1;
  }

  switch (fluid_settings_get_type(settings, av[0])) {
  case FLUID_NO_TYPE:
    fluid_ostream_printf(out, "info: no such setting '%s'.\n", av[0]);
    return -1;

  case FLUID_NUM_TYPE: {
    double value, min, max;
    fluid_settings_getnum_range(settings, av[0], &min, &max);
    fluid_settings_getnum(settings, av[0], &value);
    fluid_ostream_printf(out, "%s:\n", av[0]);
    fluid_ostream_printf(out, "Type:          number\n");
    fluid_ostream_printf(out, "Value:         %.3f\n", value);
    fluid_ostream_printf(out, "Minimum value: %.3f\n", min);
    fluid_ostream_printf(out, "Maximum value: %.3f\n", max);
    fluid_ostream_printf(out, "Default value: %.3f\n",
			fluid_settings_getnum_default(settings, av[0]));
    fluid_ostream_printf(out, "Real-time:     %s\n",
			fluid_settings_is_realtime(settings, av[0])? "yes" : "no");
    break;
  }

  case FLUID_INT_TYPE: {
    int value, min, max, def, hints;

    fluid_settings_getint_range(settings, av[0], &min, &max);
    fluid_settings_getint(settings, av[0], &value);
    hints = fluid_settings_get_hints(settings, av[0]);
    def = fluid_settings_getint_default (settings, av[0]);

    fluid_ostream_printf(out, "%s:\n", av[0]);

    if (!(hints & FLUID_HINT_TOGGLED))
    {
      fluid_ostream_printf(out, "Type:          integer\n");
      fluid_ostream_printf(out, "Value:         %d\n", value);
      fluid_ostream_printf(out, "Minimum value: %d\n", min);
      fluid_ostream_printf(out, "Maximum value: %d\n", max);
      fluid_ostream_printf(out, "Default value: %d\n", def);
    }
    else
    {
      fluid_ostream_printf(out, "Type:          boolean\n");
      fluid_ostream_printf(out, "Value:         %s\n", value ? "True" : "False");
      fluid_ostream_printf(out, "Default value: %s\n", def ? "True" : "False");
    }

    fluid_ostream_printf(out, "Real-time:     %s\n",
			fluid_settings_is_realtime(settings, av[0])? "yes" : "no");
    break;
  }

  case FLUID_STR_TYPE: {
    char *s;
    fluid_settings_dupstr(settings, av[0], &s);         /* ++ alloc string */
    fluid_ostream_printf(out, "%s:\n", av[0]);
    fluid_ostream_printf(out, "Type:          string\n");
    fluid_ostream_printf(out, "Value:         %s\n", s ? s : "NULL");
    fluid_ostream_printf(out, "Default value: %s\n",
			fluid_settings_getstr_default(settings, av[0]));

    if (s) FLUID_FREE (s);

    data.out = out;
    data.first = 1;
    fluid_ostream_printf(out, "Options:       ");
    fluid_settings_foreach_option (settings, av[0], &data, fluid_handle_print_option);
    fluid_ostream_printf(out, "\n");

    fluid_ostream_printf(out, "Real-time:     %s\n",
			fluid_settings_is_realtime(settings, av[0])? "yes" : "no");
    break;
  }

  case FLUID_SET_TYPE:
    fluid_ostream_printf(out, "%s:\n", av[0]);
    fluid_ostream_printf(out, "Type:          node\n");
    break;
  }

  return 0;
}

int
fluid_handle_reset(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out)
{
  fluid_synth_system_reset(synth);
  return 0;
}

int
fluid_handle_quit(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out)
{
  fluid_ostream_printf(out, "cheers!\n");
  return -2;
}

int
fluid_handle_help(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out)
{
  /* Purpose:
   * Prints the help text for the command line commands.
   * Can be used as follows:
   * - help
   * - help (topic), where (topic) is 'general', 'chorus', etc.
   * - help all
   */

  char* topic = "help"; /* default, if no topic is given */
  int count = 0;
  int i;

  fluid_ostream_printf(out, "\n");
  /* 1st argument (optional): help topic */
  if (ac >= 1) {
    topic = av[0];
  }
  if (strcmp(topic,"help") == 0){
    /* "help help": Print a list of all topics */
    fluid_ostream_printf(out,
			"*** Help topics:***\n"
			"help all (prints all topics)\n");
    for (i = 0; fluid_commands[i].name != NULL; i++) {
      int listed_first_time = 1;
      int ii;
      for (ii = 0; ii < i; ii++){
	if (strcmp(fluid_commands[i].topic, fluid_commands[ii].topic) == 0){
	  listed_first_time = 0;
	}; /* if topic has already been listed */
      }; /* for all topics (inner loop) */
      if (listed_first_time){
	fluid_ostream_printf(out, "help %s\n",fluid_commands[i].topic);
      };
    }; /* for all topics (outer loop) */
  } else {
    /* help (arbitrary topic or "all") */
    for (i = 0; fluid_commands[i].name != NULL; i++) {
      fluid_cmd_t cmd = fluid_commands[i];
      if (cmd.help != NULL) {
	if (strcmp(topic,"all") == 0 || strcmp(topic,cmd.topic) == 0){
	  fluid_ostream_printf(out, "%s\n", fluid_commands[i].help);
	  count++;
	}; /* if it matches the topic */
      }; /* if help text exists */
    }; /* foreach command */
    if (count == 0){
      fluid_ostream_printf(out, "Unknown help topic. Try 'help help'.\n");
    };
  };
  return 0;
}

int
fluid_is_number(char* a)
{
  while (*a != 0) {
    if (((*a < '0') || (*a > '9')) && (*a != '-') && (*a != '+') && (*a != '.')) {
      return 0;
    }
    a++;
  }
  return 1;
}

int
fluid_is_empty(char* a)
{
  while (*a != 0) {
    if ((*a != ' ') && (*a != '\t') && (*a != '\n') && (*a != '\r')) {
      return 0;
    }
    a++;
  }
  return 1;
}

char*
fluid_expand_path(char* path, char* new_path, int len)
{
#if defined(WIN32) || defined(MACOS9)
  snprintf(new_path, len - 1, "%s", path);
#else
  if ((path[0] == '~') && (path[1] == '/')) {
    char* home = getenv("HOME");
    if (home == NULL) {
      snprintf(new_path, len - 1, "%s", path);
    } else {
      snprintf(new_path, len - 1, "%s%s", home, &path[1]);
    }
  } else {
    snprintf(new_path, len - 1, "%s", path);
  }
#endif

  new_path[len - 1] = 0;
  return new_path;
}



/*
 * Command
 */

fluid_cmd_t* fluid_cmd_copy(fluid_cmd_t* cmd)
{
  fluid_cmd_t* copy = FLUID_NEW(fluid_cmd_t);
  if (copy == NULL) {
    FLUID_LOG (FLUID_PANIC, "Out of memory");
    return NULL;
  }

  copy->name = FLUID_STRDUP(cmd->name);
  copy->topic = FLUID_STRDUP(cmd->topic);
  copy->help = FLUID_STRDUP(cmd->help);
  copy->handler = cmd->handler;
  copy->data = cmd->data;
  return copy;
}

void delete_fluid_cmd(fluid_cmd_t* cmd)
{
  if (cmd->name) {
    FLUID_FREE(cmd->name);
  }
  if (cmd->topic) {
    FLUID_FREE(cmd->topic);
  }
  if (cmd->help) {
    FLUID_FREE(cmd->help);
  }
  FLUID_FREE(cmd);
}

/*
 * Command handler
 */

static void
fluid_cmd_handler_destroy_hash_value (void *value)
{
  delete_fluid_cmd ((fluid_cmd_t *)value);
}

/**
 * Create a new command handler.
 * @param synth If not NULL, all the default synthesizer commands will be
 *   added to the new handler.
 * @return New command handler
 */
fluid_cmd_handler_t *
new_fluid_cmd_handler(fluid_synth_t* synth)
{
  int i;
  fluid_cmd_handler_t* handler;

  fluid_cmd_t source = {
    "source", "general", (fluid_cmd_func_t) fluid_handle_source, NULL,
    "source filename            Load a file and parse every line as a command"
  };

  handler = new_fluid_hashtable_full (fluid_str_hash, fluid_str_equal,
                                      NULL, fluid_cmd_handler_destroy_hash_value);
  if (handler == NULL) {
    return NULL;
  }

  if (synth != NULL) {
    for (i = 0; fluid_commands[i].name != NULL; i++) {
      fluid_commands[i].data = synth;
      fluid_cmd_handler_register(handler, &fluid_commands[i]);
      fluid_commands[i].data = NULL;
    }
  }

  source.data = handler;
  fluid_cmd_handler_register(handler, &source);

  return handler;
}

/**
 * Delete a command handler.
 * @param handler Command handler to delete
 */
void
delete_fluid_cmd_handler(fluid_cmd_handler_t* handler)
{
  delete_fluid_hashtable (handler);
}

/**
 * Register a new command to the handler.
 * @param handler Command handler instance
 * @param cmd Command info (gets copied)
 * @return #FLUID_OK if command was inserted, #FLUID_FAILED otherwise
 */
int
fluid_cmd_handler_register(fluid_cmd_handler_t* handler, fluid_cmd_t* cmd)
{
  fluid_cmd_t* copy = fluid_cmd_copy(cmd);
  fluid_hashtable_insert(handler, copy->name, copy);
  return FLUID_OK;
}

/**
 * Unregister a command from a command handler.
 * @param handler Command handler instance
 * @param cmd Name of the command
 * @return TRUE if command was found and unregistered, FALSE otherwise
 */
int
fluid_cmd_handler_unregister(fluid_cmd_handler_t* handler, const char *cmd)
{
  return fluid_hashtable_remove(handler, cmd);
}

int
fluid_cmd_handler_handle(fluid_cmd_handler_t* handler, int ac, char** av, fluid_ostream_t out)
{
  fluid_cmd_t* cmd;

  cmd = fluid_hashtable_lookup(handler, av[0]);

  if (cmd && cmd->handler)
    return (*cmd->handler)(cmd->data, ac - 1, av + 1, out);

  fluid_ostream_printf(out, "unknown command: %s (try help)\n", av[0]);
  return -1;
}


#if !defined(WITHOUT_SERVER)



struct _fluid_server_t {
  fluid_server_socket_t* socket;
  fluid_settings_t* settings;
  fluid_server_newclient_func_t newclient;
  void* data;
  fluid_list_t* clients;
  fluid_mutex_t mutex;
};

static void fluid_server_handle_connection(fluid_server_t* server,
					  fluid_socket_t client_socket,
					  char* addr);
static void fluid_server_close(fluid_server_t* server);

/**
 * Create a new TCP/IP command shell server.
 * @param settings Settings instance to use for the shell
 * @param newclient Callback function to call for each new client connection
 * @param data User defined data to pass to \a newclient callback
 * @return New shell server instance or NULL on error
 */
fluid_server_t*
new_fluid_server(fluid_settings_t* settings,
		fluid_server_newclient_func_t newclient,
		void* data)
{
  fluid_server_t* server;
  int port;

  server = FLUID_NEW(fluid_server_t);
  if (server == NULL) {
    FLUID_LOG(FLUID_ERR, "Out of memory");
    return NULL;
  }

  server->settings = settings;
  server->clients = NULL;
  server->newclient = newclient;
  server->data = data;

  fluid_mutex_init(server->mutex);

  fluid_settings_getint(settings, "shell.port", &port);

  server->socket = new_fluid_server_socket(port,
					  (fluid_server_func_t) fluid_server_handle_connection,
					  server);
  if (server->socket == NULL) {
    FLUID_FREE(server);
    return NULL;
  }

  return server;
}

/**
 * Delete a TCP/IP shell server.
 * @param server Shell server instance
 */
void
delete_fluid_server(fluid_server_t* server)
{
  if (server == NULL) {
    return;
  }

  fluid_server_close(server);

  FLUID_FREE(server);
}

static void fluid_server_close(fluid_server_t* server)
{
  fluid_list_t* list;
  fluid_list_t* clients;
  fluid_client_t* client;

  if (server == NULL) {
    return;
  }

  fluid_mutex_lock(server->mutex);
  clients = server->clients;
  server->clients = NULL;
  fluid_mutex_unlock(server->mutex);

  list = clients;

  while (list) {
    client = fluid_list_get(list);
    fluid_client_quit(client);
    list = fluid_list_next(list);
  }

  delete_fluid_list(clients);

  if (server->socket) {
    delete_fluid_server_socket(server->socket);
    server->socket = NULL;
  }
}

static void
fluid_server_handle_connection(fluid_server_t* server, fluid_socket_t client_socket, char* addr)
{
  fluid_client_t* client;
  fluid_cmd_handler_t* handler;

  handler = server->newclient(server->data, addr);
  if (handler == NULL) {
    return;
  }

  client = new_fluid_client(server, server->settings, handler, client_socket);
  if (client == NULL) {
    return;
  }
  fluid_server_add_client(server, client);
}

void fluid_server_add_client(fluid_server_t* server, fluid_client_t* client)
{
  fluid_mutex_lock(server->mutex);
  server->clients = fluid_list_append(server->clients, client);
  fluid_mutex_unlock(server->mutex);
}

void fluid_server_remove_client(fluid_server_t* server, fluid_client_t* client)
{
  fluid_mutex_lock(server->mutex);
  server->clients = fluid_list_remove(server->clients, client);
  fluid_mutex_unlock(server->mutex);
}

/**
 * Join a shell server thread (wait until it quits).
 * @param server Shell server instance
 * @return #FLUID_OK on success, #FLUID_FAILED otherwise
 */
int fluid_server_join(fluid_server_t* server)
{
  return fluid_server_socket_join(server->socket);
}




struct _fluid_client_t {
  fluid_server_t* server;
  fluid_settings_t* settings;
  fluid_cmd_handler_t* handler;
  fluid_socket_t socket;
  fluid_thread_t* thread;
};



static void fluid_client_run(fluid_client_t* client)
{
  fluid_shell_t shell;
  fluid_shell_init(&shell,
		  client->settings,
		  client->handler,
		  fluid_socket_get_istream(client->socket),
		  fluid_socket_get_ostream(client->socket));
  fluid_shell_run(&shell);
  fluid_server_remove_client(client->server, client);
  delete_fluid_client(client);
}


fluid_client_t*
new_fluid_client(fluid_server_t* server, fluid_settings_t* settings,
		fluid_cmd_handler_t* handler, fluid_socket_t sock)
{
  fluid_client_t* client;

  client = FLUID_NEW(fluid_client_t);
  if (client == NULL) {
    FLUID_LOG(FLUID_ERR, "Out of memory");
    return NULL;
  }

  client->server = server;
  client->socket = sock;
  client->settings = settings;
  client->handler = handler;

  /*client->thread = new_fluid_thread((fluid_thread_func_t) fluid_client_run, client,
                                    0, FALSE);

  if (client->thread == NULL) {
    fluid_socket_close(sock);
    FLUID_FREE(client);
    return NULL;
  }
  */
  return client;
}

void fluid_client_quit(fluid_client_t* client)
{
  if (client->socket != INVALID_SOCKET) {
    fluid_socket_close(client->socket);
    client->socket = INVALID_SOCKET;
  }
  FLUID_LOG(FLUID_DBG, "fluid_client_quit: joining");
  fluid_thread_join(client->thread);
  FLUID_LOG(FLUID_DBG, "fluid_client_quit: done");
}

void delete_fluid_client(fluid_client_t* client)
{
  if (client->socket != INVALID_SOCKET) {
    fluid_socket_close(client->socket);
    client->socket = INVALID_SOCKET;
  }
  if (client->thread != NULL) {
    delete_fluid_thread(client->thread);
    client->thread = NULL;
  }
  FLUID_FREE(client);
}


#endif /* WITHOUT_SERVER */
