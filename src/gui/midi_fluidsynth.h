/*
 *  Copyright (C) 2002-2011  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <fluidsynth.h>
#include <ctype.h>
#include <string>
#include <sstream>

class MidiHandler_fluidsynth : public MidiHandler {
private:
	std::string audio_driver;
	std::string soundfont;
	int soundfont_id;
	fluid_settings_t *settings;
	fluid_synth_t *synth;
	fluid_audio_driver_t* adriver;
public:
	MidiHandler_fluidsynth() : MidiHandler() {};
	const char* GetName(void) { return "fluidsynth"; }
	void PlaySysex(Bit8u * sysex,Bitu len) {
		fluid_synth_sysex(synth, (char*) sysex, len, NULL, NULL, NULL, 0);
	}

	void PlayMsg(Bit8u * msg) {
		unsigned char chanID = msg[0] & 0x0F;
		switch (msg[0] & 0xF0) {
		case 0x80:
			fluid_synth_noteoff(synth, chanID, msg[1]);
			break;
		case 0x90:
			fluid_synth_noteon(synth, chanID, msg[1], msg[2]);
			break;
		case 0xB0:
			fluid_synth_cc(synth, chanID, msg[1], msg[2]);
			break;
		case 0xC0:
			fluid_synth_program_change(synth, chanID, msg[1]);
			break;
		case 0xD0:
			fluid_synth_channel_pressure(synth, chanID, msg[1]);
			break;
		case 0xE0:{
				long theBend = ((long)msg[1] + (long)(msg[2] << 7));
				fluid_synth_pitch_bend(synth, chanID, theBend);
			}
			break;
		default:
			LOG(LOG_MISC,LOG_WARN)("MIDI:fluidsynth: Unknown Command: %08lx", (long)msg);
			break;
		}
	}	

	void Close(void) {
		if (soundfont_id >= 0) {
			fluid_synth_sfunload(synth, soundfont_id, 0);
		}
		delete_fluid_audio_driver(adriver);
		delete_fluid_synth(synth);
		delete_fluid_settings(settings);
	}

	bool Open(const char * conf) {
		/* The 'midiconfig' for the fluidsynth driver is a string of the
		 * following format:
		 * audio_driver:soundfont
		 * The audio_driver is the name of the FluidSynth audio driver (e.g.,
		 * "jack" or "alsa"). The soundfont is the filename to the soundfont
		 * to load.
		 * If there is no colon, the whole string is the driver, and no
		 * soundfont is loaded. If midiconfig is missing or empty, the driver
		 * is the default.
		 */
		if (conf && conf[0]) {
			std::string conf_str(conf);
			size_t delim_pos = conf_str.find(':');
			if (delim_pos == std::string::npos) {
				/* No colon; whole string is driver */
				audio_driver = conf_str;
			} else {
				/* Split string based on position of colon */
				audio_driver.assign(conf_str, 0, delim_pos);
				soundfont.assign(conf_str.c_str() + delim_pos + 1);
			}
		}
		settings = new_fluid_settings();
		if (!audio_driver.empty()) {
			fluid_settings_setstr(settings, "audio.driver", audio_driver.c_str());
		}
		synth = new_fluid_synth(settings);
		if (!synth) {
			LOG_MSG("MIDI:fluidsynth: Can't open synthesiser");
			delete_fluid_settings(settings);
			return false;
		}
		adriver = new_fluid_audio_driver(settings, synth);
		if (!adriver) {
			LOG_MSG("MIDI:fluidsynth: Can't create audio driver");
			delete_fluid_synth(synth);
			delete_fluid_settings(settings);
			return false;
		}

		/* Optionally load a soundfont */
		if (!soundfont.empty()) {
			soundfont_id = fluid_synth_sfload(synth, soundfont.c_str(), 1);
			if (soundfont_id == FLUID_FAILED) {
				/* Just consider this a warning (fluidsynth already prints) */
				soundfont.clear();
				soundfont_id = -1;
			} else {
				LOG_MSG("MIDI:fluidsynth: loaded soundfont: %s",
					soundfont.c_str());
			}
		} else {
			soundfont_id = -1;
			LOG_MSG("MIDI:fluidsynth: no soundfont loaded");
		}
		return true;
	}
};

MidiHandler_fluidsynth Midi_fluidsynth;
