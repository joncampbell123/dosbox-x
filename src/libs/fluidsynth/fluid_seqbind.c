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



/*
 2002 : API design by Peter Hanappe and Antoine Schmitt
 August 2002 : Implementation by Antoine Schmitt as@gratin.org
               as part of the infiniteCD author project
               http://www.infiniteCD.org/
*/

#if !C_FLUIDSYNTH && defined(WIN32)
#include "fluidsynth_priv.h"
#include "fluid_synth.h"
#include "fluid_midi.h"
#include "fluid_event_priv.h"

 /***************************************************************
 *
 *                           SEQUENCER BINDING
 */

struct _fluid_seqbind_t {
	fluid_synth_t* synth;
	fluid_sequencer_t* seq;
	fluid_sample_timer_t* sample_timer;
	short client_id;
};
typedef struct _fluid_seqbind_t fluid_seqbind_t;


int fluid_seqbind_timer_callback(void* data, unsigned int msec);
void fluid_seq_fluidsynth_callback(unsigned int time, fluid_event_t* event, fluid_sequencer_t* seq, void* data);

/* Proper cleanup of the seqbind struct. */
void 
delete_fluid_seqbind(fluid_seqbind_t* seqbind) 
{
	if (seqbind == NULL) {
		return;
	}

	if ((seqbind->client_id != -1) && (seqbind->seq != NULL)) {
		fluid_sequencer_unregister_client(seqbind->seq, seqbind->client_id);
		seqbind->client_id = -1;
	}

	if ((seqbind->sample_timer != NULL) && (seqbind->synth != NULL)) {
		delete_fluid_sample_timer(seqbind->synth, seqbind->sample_timer);
		seqbind->sample_timer = NULL;
	}

	FLUID_FREE(seqbind);
}

/** 
 * Registers a synthesizer as a destination client of the given sequencer.
 * The \a synth is registered with the name "fluidsynth".
 * @param seq Sequencer instance
 * @param synth Synthesizer instance
 * @returns Sequencer client ID, or #FLUID_FAILED on error.
 */
short 
fluid_sequencer_register_fluidsynth (fluid_sequencer_t* seq, fluid_synth_t* synth)
{
	fluid_seqbind_t* seqbind;
	
	seqbind = FLUID_NEW(fluid_seqbind_t);
	if (seqbind == NULL) {
		fluid_log(FLUID_PANIC, "sequencer: Out of memory\n");
		return FLUID_FAILED;
	}

	seqbind->synth = synth;
	seqbind->seq = seq;
	seqbind->sample_timer = NULL;
	seqbind->client_id = -1;

	/* set up the sample timer */
	if (!fluid_sequencer_get_use_system_timer(seq)) {
		seqbind->sample_timer = 
			new_fluid_sample_timer(synth, fluid_seqbind_timer_callback, (void *) seqbind);
		if (seqbind->sample_timer == NULL) {
			fluid_log(FLUID_PANIC, "sequencer: Out of memory\n");
			delete_fluid_seqbind(seqbind);
			return FLUID_FAILED;
		}
	}

	/* register fluidsynth itself */
	seqbind->client_id = 
		fluid_sequencer_register_client(seq, "fluidsynth", fluid_seq_fluidsynth_callback, (void *)seqbind);
	if (seqbind->client_id == -1) {
		delete_fluid_seqbind(seqbind);
		return FLUID_FAILED;
	}

	return seqbind->client_id;
}

/* Callback for sample timer */
int
fluid_seqbind_timer_callback(void* data, unsigned int msec)
{
	fluid_seqbind_t* seqbind = (fluid_seqbind_t *) data;
	fluid_sequencer_process(seqbind->seq, msec);
	return 1;
}

/* Callback for midi events */
void 
fluid_seq_fluidsynth_callback(unsigned int time, fluid_event_t* evt, fluid_sequencer_t* seq, void* data)
{
	fluid_synth_t* synth;
	fluid_seqbind_t* seqbind = (fluid_seqbind_t *) data;
	synth = seqbind->synth;

  switch (fluid_event_get_type(evt)) {

  case FLUID_SEQ_NOTEON:
  	fluid_synth_noteon(synth, fluid_event_get_channel(evt), fluid_event_get_key(evt), fluid_event_get_velocity(evt));
  	break;

  case FLUID_SEQ_NOTEOFF:
  	fluid_synth_noteoff(synth, fluid_event_get_channel(evt), fluid_event_get_key(evt));
  	break;

  case FLUID_SEQ_NOTE:
	  {
	  	unsigned int dur;
	  	fluid_synth_noteon(synth, fluid_event_get_channel(evt), fluid_event_get_key(evt), fluid_event_get_velocity(evt));
	  	dur = fluid_event_get_duration(evt);
	  	fluid_event_noteoff(evt, fluid_event_get_channel(evt), fluid_event_get_key(evt));
	  	fluid_sequencer_send_at(seq, evt, dur, 0);
	  }
  	break;

	case FLUID_SEQ_ALLSOUNDSOFF:
		/* NYI */
  	break;

  case FLUID_SEQ_ALLNOTESOFF:
  	fluid_synth_cc(synth, fluid_event_get_channel(evt), 0x7B, 0);
  	break;

  case FLUID_SEQ_BANKSELECT:
  	fluid_synth_bank_select(synth, fluid_event_get_channel(evt), fluid_event_get_bank(evt));
  	break;

  case FLUID_SEQ_PROGRAMCHANGE:
  	fluid_synth_program_change(synth, fluid_event_get_channel(evt), fluid_event_get_program(evt));
  	break;

  case FLUID_SEQ_PROGRAMSELECT:
  	fluid_synth_program_select(synth, fluid_event_get_channel(evt), fluid_event_get_sfont_id(evt),
		fluid_event_get_bank(evt), fluid_event_get_program(evt));
  	break;

  case FLUID_SEQ_ANYCONTROLCHANGE:
  	/* nothing = only used by remove_events */
  	break;

  case FLUID_SEQ_PITCHBEND:
  	fluid_synth_pitch_bend(synth, fluid_event_get_channel(evt), fluid_event_get_pitch(evt));
  	break;

  case FLUID_SEQ_PITCHWHHELSENS:
  	fluid_synth_pitch_wheel_sens(synth, fluid_event_get_channel(evt), fluid_event_get_value(evt));
  	break;

  case FLUID_SEQ_CONTROLCHANGE:
	 fluid_synth_cc(synth, fluid_event_get_channel(evt), fluid_event_get_control(evt), fluid_event_get_value(evt));
  	break;

  case FLUID_SEQ_MODULATION:
	  {
	  	short ctrl = 0x01;	// MODULATION_MSB
	  	fluid_synth_cc(synth, fluid_event_get_channel(evt), ctrl, fluid_event_get_value(evt));
	  }
  	break;

  case FLUID_SEQ_SUSTAIN:
	  {
	  	short ctrl = 0x40;	// SUSTAIN_SWITCH
	  	fluid_synth_cc(synth, fluid_event_get_channel(evt), ctrl, fluid_event_get_value(evt));
	  }
  	break;

  case FLUID_SEQ_PAN:
	  {
	  	short ctrl = 0x0A;	// PAN_MSB
	  	fluid_synth_cc(synth, fluid_event_get_channel(evt), ctrl, fluid_event_get_value(evt));
	  }
  	break;

  case FLUID_SEQ_VOLUME:
	  {
	  	short ctrl = 0x07;	// VOLUME_MSB
	  	fluid_synth_cc(synth, fluid_event_get_channel(evt), ctrl, fluid_event_get_value(evt));
	  }
  	break;

  case FLUID_SEQ_REVERBSEND:
	  {
	  	short ctrl = 0x5B;	// EFFECTS_DEPTH1
	  	fluid_synth_cc(synth, fluid_event_get_channel(evt), ctrl, fluid_event_get_value(evt));
	  }
  	break;

  case FLUID_SEQ_CHORUSSEND:
	  {
	  	short ctrl = 0x5D;	// EFFECTS_DEPTH3
	  	fluid_synth_cc(synth, fluid_event_get_channel(evt), ctrl, fluid_event_get_value(evt));
	  }
  	break;

  case FLUID_SEQ_CHANNELPRESSURE: 
	  {
		fluid_synth_channel_pressure(synth, fluid_event_get_channel(evt), fluid_event_get_value(evt));
	  }
	break;

  case FLUID_SEQ_KEYPRESSURE:
	  /*fluid_synth_key_pressure(synth,
		  fluid_event_get_channel(evt),
		  fluid_event_get_key(evt),
		  fluid_event_get_value(evt));*/
	  break;

  case FLUID_SEQ_SYSTEMRESET: 
	  {
		fluid_synth_system_reset(synth);
	  }
	break;

  case FLUID_SEQ_UNREGISTERING: /* free ourselves */
	  {
		seqbind->client_id = -1; /* avoid recursive call to fluid_sequencer_unregister_client */
	        delete_fluid_seqbind(seqbind);
	  }
	break;

  case FLUID_SEQ_TIMER:
	  /* nothing in fluidsynth */
  	break;

	default:
  	break;
	}
}

static int get_fluidsynth_dest(fluid_sequencer_t* seq) 
{
	int i, id;
	char* name;
	int j = fluid_sequencer_count_clients(seq);
	for (i = 0; i < j; i++) {
		id = fluid_sequencer_get_client_id(seq, i);
		name = fluid_sequencer_get_client_name(seq, id);
		if (name && (strcmp(name, "fluidsynth") == 0)) {
			return id;
		}
	}
	return -1;
}

/**
 * Transforms an incoming midi event (from a midi driver or midi router) to a 
 * sequencer event and adds it to the sequencer queue for sending as soon as possible.
 * @param data The sequencer, must be a valid #fluid_sequencer_t
 * @param event MIDI event
 * @return #FLUID_OK or #FLUID_FAILED
 * @since 1.1.0
 */
int
fluid_sequencer_add_midi_event_to_buffer(void* data, fluid_midi_event_t* event)
{
	fluid_event_t evt;
	fluid_sequencer_t* seq = (fluid_sequencer_t*) data;
	int chan = fluid_midi_event_get_channel(event);

	fluid_event_clear(&evt);
	fluid_event_set_time(&evt, fluid_sequencer_get_tick(seq));
	fluid_event_set_dest(&evt, get_fluidsynth_dest(seq));

	switch (fluid_midi_event_get_type(event)) {
	case NOTE_OFF:
		fluid_event_noteoff(&evt, chan, fluid_midi_event_get_key(event));	
		break;  
	case NOTE_ON:
		fluid_event_noteon(&evt, fluid_midi_event_get_channel(event),
		                   fluid_midi_event_get_key(event), fluid_midi_event_get_velocity(event));	
		break;  
	case CONTROL_CHANGE:
		fluid_event_control_change(&evt, chan, fluid_midi_event_get_control(event),
		                           fluid_midi_event_get_value(event));
		break;
	case PROGRAM_CHANGE:
		fluid_event_program_change(&evt, chan, fluid_midi_event_get_program(event));
		break;
	case PITCH_BEND:
		fluid_event_pitch_bend(&evt, chan, fluid_midi_event_get_pitch(event));
		break;
	case CHANNEL_PRESSURE:
		fluid_event_channel_pressure(&evt, chan, fluid_midi_event_get_program(event));
		break;
	case KEY_PRESSURE:
		fluid_event_key_pressure(&evt, chan,
			(short)fluid_midi_event_get_key(event),
			(short)fluid_midi_event_get_value(event));
		break;
	case MIDI_SYSTEM_RESET:
		fluid_event_system_reset(&evt);
		break;
	default:  /* Not yet implemented */
		return FLUID_FAILED; 
	}

	/* Schedule for sending at next call to fluid_sequencer_process */
	return fluid_sequencer_send_at(seq, &evt, 0, 0);
}
#endif
