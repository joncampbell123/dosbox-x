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

/* Protect against multiple inclusions */
#ifndef MIXER_BUFSIZE
#include "mixer.h"
#endif

static MixerChannel *synthchan = NULL;
static fluid_synth_t *synth_soft = NULL;
static int synthsamplerate = 0;

static void synth_log(int level, char *message, void *data) {
    (void)data;//UNUSED

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

#if defined (WIN32) || defined (OS2)
#	define PATH_SEP "\\";
#else
#	define PATH_SEP "/";
#endif

class MidiHandler_synth: public MidiHandler {
private:
	fluid_settings_t *settings;
	int sfont_id;
	bool isOpen;

	void PlayEvent(Bit8u *msg, Bitu len) {
		Bit8u event = msg[0], channel, p1, p2;

		switch (event) {
		case 0xf0:
		case 0xf7:
			LOG(LOG_MISC,LOG_DEBUG)("SYNTH: sysex 0x%02x len %lu", (int)event, (long unsigned)len);
			fluid_synth_sysex(synth_soft, (char *)(msg + 1), len - 1, NULL, NULL, NULL, 0);
			return;
		case 0xf9:
			LOG(LOG_MISC,LOG_DEBUG)("SYNTH: midi tick");
			return;
		case 0xff:
			LOG(LOG_MISC,LOG_DEBUG)("SYNTH: system reset");
			fluid_synth_system_reset(synth_soft);
			return;
		case 0xf1: case 0xf2: case 0xf3: case 0xf4:
		case 0xf5: case 0xf6: case 0xf8: case 0xfa:
		case 0xfb: case 0xfc: case 0xfd: case 0xfe:
			LOG(LOG_MISC,LOG_WARN)("SYNTH: unhandled event 0x%02x", (int)event);
			return;
		}

		channel = event & 0xf;
		p1 = len > 1 ? msg[1] : 0;
		p2 = len > 2 ? msg[2] : 0;

		LOG(LOG_MISC,LOG_DEBUG)("SYNTH: event 0x%02x channel %d, 0x%02x 0x%02x",
			(int)event, (int)channel, (int)p1, (int)p2);

		switch (event & 0xf0) {
		case 0x80:
			fluid_synth_noteoff(synth_soft, channel, p1);
			break;
		case 0x90:
			fluid_synth_noteon(synth_soft, channel, p1, p2);
			break;
		case 0xb0:
			fluid_synth_cc(synth_soft, channel, p1, p2);
			break;
		case 0xc0:
			fluid_synth_program_change(synth_soft, channel, p1);
			break;
		case 0xd0:
			fluid_synth_channel_pressure(synth_soft, channel, p1);
			break;
		case 0xe0:
			fluid_synth_pitch_bend(synth_soft, channel, (p2 << 7) | p1);
			break;
		}
	};

public:
	MidiHandler_synth() : MidiHandler(),isOpen(false) {};

	const char * GetName(void) {
		return "synth";
	};

	bool Open(const char *conf) {
		if (isOpen) return false;

		/* Sound font file required */
		if (!conf || (conf[0] == '\0')) {
			LOG(LOG_MISC,LOG_DEBUG)("SYNTH: Specify .SF2 sound font file with midiconfig=");
			return false;
		}

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
		fluid_settings_setnum(settings, "synth.sample-rate", (double)synthsamplerate);
		//fluid_settings_setnum(settings, "synth.gain", 0.5);

		/* Create the synthesizer. */
		synth_soft = new_fluid_synth(settings);
		if (synth_soft == NULL) {
			LOG(LOG_MISC,LOG_WARN)("SYNTH: Error initialising MIDI soft synth");
			delete_fluid_settings(settings);
			return false;
		}

		/* Load a SoundFont */
		sfont_id = fluid_synth_sfload(synth_soft, conf, 0);
		if (sfont_id == -1) {
			extern std::string capturedir;
			std::string str = capturedir + PATH_SEP + conf;
			sfont_id = fluid_synth_sfload(synth_soft, str.c_str(), 0);
		}

		if (sfont_id == -1) {
			LOG(LOG_MISC,LOG_WARN)("SYNTH: Failed to load MIDI sound font file \"%s\"",
			   conf);
			delete_fluid_synth(synth_soft);
			delete_fluid_settings(settings);
			return false;
		}

		synthchan = MIXER_AddChannel(synth_CallBack, synthsamplerate, "SYNTH");
		synthchan->Enable(false);
		isOpen = true;
		return true;
	};

	void Close(void) {
		if (!isOpen) return;

		synthchan->Enable(false);
		MIXER_DelChannel(synthchan);
		delete_fluid_synth(synth_soft);
		delete_fluid_settings(settings);

		synthchan = NULL;
		synth_soft = NULL;
		settings = NULL;
		isOpen = false;
	};

	void PlayMsg(Bit8u *msg) {
		synthchan->Enable(true);
		PlayEvent(msg, MIDI_evt_len[*msg]);
	};

	void PlaySysex(Bit8u *sysex, Bitu len) {
		PlayEvent(sysex, len);
	};
};

MidiHandler_synth Midi_synth;
