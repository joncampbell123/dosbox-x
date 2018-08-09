/*
 *  Copyright (C) 2002-2013  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <fluidsynth.h>
#include <math.h>

#define FLUID_FREE(_p)               free(_p)
#define FLUID_OK   (0) 
#define FLUID_NEW(_t)                (_t*)malloc(sizeof(_t))

/* Missing fluidsynth API follow: */
extern "C" {
  typedef struct _fluid_midi_parser_t fluid_midi_parser_t;
  typedef struct _fluid_midi_event_t fluid_midi_event_t;

  fluid_midi_parser_t * new_fluid_midi_parser(void);
  fluid_midi_event_t * fluid_midi_parser_parse(fluid_midi_parser_t *parser, unsigned char c);

  void fluid_log_config(void);
}

//static fluid_log_function_t fluid_log_function[LAST_LOG_LEVEL];
//static int fluid_log_initialized = 0;

struct _fluid_midi_event_t {
  fluid_midi_event_t* next; /* Link to next event */
  void *paramptr;           /* Pointer parameter (for SYSEX data), size is stored to param1, param2 indicates if pointer should be freed (dynamic if TRUE) */
  unsigned int dtime;       /* Delay (ticks) between this and previous event. midi tracks. */
  unsigned int param1;      /* First parameter */
  unsigned int param2;      /* Second parameter */
  unsigned char type;       /* MIDI event type */
  unsigned char channel;    /* MIDI channel */
};

struct _fluid_midi_parser_t {
  unsigned char status;           /* Identifies the type of event, that is currently received ('Noteon', 'Pitch Bend' etc). */
  unsigned char channel;          /* The channel of the event that is received (in case of a channel event) */
  unsigned int nr_bytes;          /* How many bytes have been read for the current event? */
  unsigned int nr_bytes_total;    /* How many bytes does the current event type include? */
  unsigned char data[1024]; /* The parameters or SYSEX data */
  fluid_midi_event_t event;        /* The event, that is returned to the MIDI driver. */
};

enum fluid_midi_event_type {
  /* channel messages */
  NOTE_OFF = 0x80,
  NOTE_ON = 0x90,
  KEY_PRESSURE = 0xa0,
  CONTROL_CHANGE = 0xb0,
  PROGRAM_CHANGE = 0xc0,
  CHANNEL_PRESSURE = 0xd0,
  PITCH_BEND = 0xe0,
  /* system exclusive */
  MIDI_SYSEX = 0xf0,
  /* system common - never in midi files */
  MIDI_TIME_CODE = 0xf1,
  MIDI_SONG_POSITION = 0xf2,
  MIDI_SONG_SELECT = 0xf3,
  MIDI_TUNE_REQUEST = 0xf6,
  MIDI_EOX = 0xf7,
  /* system real-time - never in midi files */
  MIDI_SYNC = 0xf8,
  MIDI_TICK = 0xf9,
  MIDI_START = 0xfa,
  MIDI_CONTINUE = 0xfb,
  MIDI_STOP = 0xfc,
  MIDI_ACTIVE_SENSING = 0xfe,
  MIDI_SYSTEM_RESET = 0xff,
  /* meta event - for midi files only */
  MIDI_META_EVENT = 0xff
};

#if 0 /* UNUSED CODE */
static int fluid_midi_event_length(unsigned char event) {
      switch (event & 0xF0) {
            case NOTE_OFF: 
            case NOTE_ON:
            case KEY_PRESSURE:
            case CONTROL_CHANGE:
            case PITCH_BEND: 
                  return 3;
            case PROGRAM_CHANGE:
            case CHANNEL_PRESSURE: 
                  return 2;
      }
      switch (event) {
            case MIDI_TIME_CODE:
            case MIDI_SONG_SELECT:
            case 0xF4:
            case 0xF5:
                  return 2;
            case MIDI_TUNE_REQUEST:
                  return 1;
            case MIDI_SONG_POSITION:
                  return 3;
      }
      return 1;
}
#endif

int delete_fluid_midi_parser(fluid_midi_parser_t* parser) {
      FLUID_FREE(parser);
      return FLUID_OK;
}

/* Protect against multiple inclusions */
#ifndef MIXER_BUFSIZE
#include "mixer.h"
#endif

static MixerChannel *synthchan = NULL;

static fluid_synth_t *synth_soft = NULL;
static fluid_midi_parser_t *synth_parser = NULL;

static int synthsamplerate = 0;


static void synth_log(int level, char *message, void *data) {
	switch (level) {
	case FLUID_PANIC:
	case FLUID_ERR:
		LOG(LOG_ALL,LOG_ERROR)(message);
		break;

	case FLUID_WARN:
		LOG(LOG_ALL,LOG_WARN)(message);
		break;

	default:
		LOG(LOG_ALL,LOG_NORMAL)(message);
		break;
	}
}

static void synth_CallBack(Bitu len) {
    if (synth_soft != NULL) {
        fluid_synth_write_s16(synth_soft, len, MixTemp, 0, 2, MixTemp, 1, 2);
        synthchan->AddSamples_s16(len,(Bit16s *)MixTemp);
    }
}

class MidiHandler_synth: public MidiHandler {
private:
	fluid_settings_t *settings;
	fluid_midi_router_t *router;
	int sfont_id;
	bool isOpen;

public:
	MidiHandler_synth() : MidiHandler(),isOpen(false) {};
	const char * GetName(void) { return "synth"; };
	bool Open(const char *conf) {

		/* Sound font file required */
		if (!conf || (conf[0] == '\0')) {
			LOG(LOG_MISC,LOG_DEBUG)("SYNTH: Specify .SF2 sound font file with config=");
			return false;
		}

		fluid_log_config();
		fluid_set_log_function(FLUID_PANIC, synth_log, NULL);
		fluid_set_log_function(FLUID_ERR, synth_log, NULL);
		fluid_set_log_function(FLUID_WARN, synth_log, NULL);
		fluid_set_log_function(FLUID_INFO, synth_log, NULL);
		fluid_set_log_function(FLUID_DBG, synth_log, NULL);

		/* Create the settings. */
		settings = new_fluid_settings();
		if (settings == NULL) {
			LOG(LOG_MISC,LOG_WARN)("SYNTH: Error allocating MIDI soft synth settings");
			return false;
		}

		fluid_settings_setstr(settings, "audio.sample-format", "16bits");

		if (synthsamplerate == 0) {
			synthsamplerate = 44100;
		}

		fluid_settings_setnum(settings,
			"synth.sample-rate", (double)synthsamplerate);

		//fluid_settings_setnum(settings,
		//	"synth.gain", 0.5);

		/* Create the synthesizer. */
		synth_soft = new_fluid_synth(settings);
		if (synth_soft == NULL) {
			LOG(LOG_MISC,LOG_WARN)("SYNTH: Error initialising MIDI soft synth");
			delete_fluid_settings(settings);
			return false;
		}

		/* Load a SoundFont */
		extern std::string capturedir;
		char str[260];
		strcpy(str,capturedir.c_str());
		#if defined (WIN32) || defined (OS2)
		strcat(str,"\\");
		#else
		strcat(str,"/");
		#endif
		strcat(str,conf);
		sfont_id = fluid_synth_sfload(synth_soft, conf, 0);
		if (sfont_id == -1) {
			sfont_id = fluid_synth_sfload(synth_soft, str, 0);
			if (sfont_id == -1) {
				LOG(LOG_MISC,LOG_WARN)("SYNTH: Failed to load MIDI sound font file \"%s\"",
				   conf);
				delete_fluid_synth(synth_soft);
				delete_fluid_settings(settings);
				return false;
			}
		}

		/* Allocate one event to store the input data */
		synth_parser = new_fluid_midi_parser();
		if (synth_parser == NULL) {
			LOG(LOG_MISC,LOG_WARN)("SYNTH: Failed to allocate MIDI parser");
			delete_fluid_synth(synth_soft);
			delete_fluid_settings(settings);
			return false;
		}

		router = new_fluid_midi_router(settings,
					       fluid_synth_handle_midi_event,
					       (void*)synth_soft);
		if (router == NULL) {
			LOG(LOG_MISC,LOG_WARN)("SYNTH: Failed to initialise MIDI router");
			delete_fluid_midi_parser(synth_parser);
			delete_fluid_synth(synth_soft);
			delete_fluid_settings(settings);
			return false;
		}

		synthchan=MIXER_AddChannel(synth_CallBack, synthsamplerate,
					   "SYNTH");
		synthchan->Enable(false);
		isOpen = true;
		return true;
	};
	void Close(void) {
		if (!isOpen) return;

        synthchan->Enable(false);
        MIXER_DelChannel(synthchan);
        synthchan=NULL;

		delete_fluid_midi_router(router);
		delete_fluid_midi_parser(synth_parser);
		delete_fluid_synth(synth_soft);
		delete_fluid_settings(settings);
        synth_parser=NULL;
        synth_soft=NULL;
        settings=NULL;
		isOpen=false;
        router=NULL;
	};
	void PlayMsg(Bit8u *msg) {
		fluid_midi_event_t *evt;
		Bitu len;
		Bitu i;

		len=MIDI_evt_len[*msg];
		synthchan->Enable(true);

		/* let the parser convert the data into events */
		for (i = 0; i < len; i++) {
			evt = fluid_midi_parser_parse(synth_parser, msg[i]);
			if (evt != NULL) {
			  /* send the event to the next link in the chain */
			  fluid_midi_router_handle_midi_event(router, evt);
			}
		}
	};
	void PlaySysex(Bit8u *sysex, Bitu len) {
		fluid_midi_event_t *evt;
		Bitu i;

		/* let the parser convert the data into events */
		for (i = 0; i < len; i++) {
			evt = fluid_midi_parser_parse(synth_parser, sysex[i]);
			if (evt != NULL) {
			  /* send the event to the next link in the chain */
			  fluid_midi_router_handle_midi_event(router, evt);
			}
		}
	};
};

MidiHandler_synth Midi_synth;
