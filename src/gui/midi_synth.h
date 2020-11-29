/*
 *  Copyright (C) 2002-2020  The DOSBox Team
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
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#if C_FLUIDSYNTH
#include <fluidsynth.h>
#else
#include "fluidsynth.h"
#endif
#include <math.h>
#include <string.h>
#include "control.h"

/* Protect against multiple inclusions */
#ifndef MIXER_BUFSIZE
#include "mixer.h"
#endif

static MixerChannel *synthchan = NULL;
static fluid_synth_t *synth_soft = NULL;
static int synthsamplerate = 0;

static void synth_log(int level,
#if !defined (FLUIDSYNTH_VERSION_MAJOR) || FLUIDSYNTH_VERSION_MAJOR >= 2 // Let version 2.x be the default
                      const
#endif
                      char *message,
                      void *data) {
    (void)data;//UNUSED

	switch (level) {
	case FLUID_PANIC:
	case FLUID_ERR:
		LOG(LOG_ALL,LOG_ERROR)("%s", message);
		break;

	case FLUID_WARN:
		LOG(LOG_ALL,LOG_WARN)("%s", message);
		break;

	default:
		LOG(LOG_ALL,LOG_NORMAL)("%s", message);
		break;
	}
}

static void synth_CallBack(Bitu len) {
	if (synth_soft != NULL) {
		fluid_synth_write_s16(synth_soft, (int)len, MixTemp, 0, 2, MixTemp, 1, 2);
		synthchan->AddSamples_s16(len,(int16_t *)MixTemp);
	}
}

#if defined (WIN32) || defined (OS2)
#	define PATH_SEP "\\"
#else
#	define PATH_SEP "/"
#endif

class MidiHandler_synth: public MidiHandler {
private:
	fluid_settings_t *settings;
	int sfont_id;
	bool isOpen;

	void PlayEvent(uint8_t *msg, Bitu len) {
		uint8_t event = msg[0], channel, p1, p2;

		switch (event) {
		case 0xf0:
		case 0xf7:
			LOG(LOG_MISC,LOG_DEBUG)("SYNTH: sysex 0x%02x len %lu", (int)event, (long unsigned)len);
			fluid_synth_sysex(synth_soft, (char *)(msg + 1), (int)(len - 1), NULL, NULL, NULL, 0);
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

		std::string sf = "";
		/* Sound font file required */
		if (!conf || (conf[0] == '\0')) {
#if defined (WIN32)
			// default for windows according to fluidsynth docs
			if (FILE *file = fopen("C:\\soundfonts\\default.sf2", "r")) {
				fclose(file);
				sf = "C:\\soundfonts\\default.sf2";
			} else if (FILE *file = fopen("C:\\DOSBox-X\\FluidR3_GM.sf2", "r")) {
				fclose(file);
				sf = "C:\\DOSBox-X\\FluidR3_GM.sf2";
			} else if (FILE *file = fopen("C:\\DOSBox-X\\GeneralUser_GS.sf2", "r")) {
				fclose(file);
				sf = "C:\\DOSBox-X\\GeneralUser_GS.sf2";
			} else {
				LOG_MSG("MIDI:synth: Specify .SF2 sound font file with midiconfig=");
				return false;
			}
#else
			// Default on "other" platforms according to fluidsynth docs
			// This works on RH and Fedora, if a soundfont is installed
			if (FILE *file = fopen("/usr/share/soundfonts/default.sf2", "r")) {
				fclose(file);
				sf = "/usr/share/soundfonts/default.sf2";
			// Ubuntu and Debian don't have a default.sf2...
			} else if (FILE *file = fopen("/usr/share/sounds/sf2/FluidR3_GM.sf2", "r")) {
				fclose(file);
				sf = "/usr/share/sounds/sf2/FluidR3_GM.sf2";
			} else if (FILE *file = fopen("/usr/share/sounds/sf2/GeneralUser_GS.sf2", "r")) {
				fclose(file);
				sf = "/usr/share/sounds/sf2/GeneralUser_GS.sf2";
			} else {
				LOG_MSG("MIDI:synth: Specify .SF2 sound font file with midiconfig=");
				return false;
			}
#endif
		} else
            sf = std::string(conf);

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
		sfont_id = fluid_synth_sfload(synth_soft, sf.c_str(), 0);
		if (sfont_id == -1) {
			extern std::string capturedir;
			std::string str = capturedir + std::string(PATH_SEP) + sf;
			sfont_id = fluid_synth_sfload(synth_soft, str.c_str(), 0);
		}

		if (sfont_id == -1) {
			LOG(LOG_MISC,LOG_WARN)("SYNTH: Failed to load MIDI sound font file \"%s\"", sf.c_str());
			delete_fluid_synth(synth_soft);
			delete_fluid_settings(settings);
			return false;
		}
		sffile=sf;

		synthchan = MIXER_AddChannel(synth_CallBack, (unsigned int)synthsamplerate, "SYNTH");
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

	void PlayMsg(uint8_t *msg) {
		synthchan->Enable(true);
		PlayEvent(msg, MIDI_evt_len[*msg]);
	};

	void PlaySysex(uint8_t *sysex, Bitu len) {
		PlayEvent(sysex, len);
	};
};

MidiHandler_synth Midi_synth;

class MidiHandler_fluidsynth : public MidiHandler {
private:
	std::string soundfont;
	int soundfont_id;
	fluid_settings_t *settings;
	fluid_synth_t *synth;
	fluid_audio_driver_t* adriver;
public:
	MidiHandler_fluidsynth() : MidiHandler() {};
	const char* GetName(void) { return "fluidsynth"; }
	void PlaySysex(uint8_t * sysex, Bitu len) {
		fluid_synth_sysex(synth, (char*)sysex, (int)len, NULL, NULL, NULL, 0);
	}

	void PlayMsg(uint8_t * msg) {
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
		case 0xE0: {
			long theBend = ((long)msg[1] + (long)(msg[2] << 7));
			fluid_synth_pitch_bend(synth, chanID, theBend);
		}
				   break;
		default:
			LOG(LOG_MISC, LOG_WARN)("MIDI:fluidsynth: Unknown Command: %08lx", (long)msg[0]);
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
		(void)conf;

		Section_prop *section = static_cast<Section_prop *>(control->GetSection("midi"));
		std::string sf = section->Get_string("fluid.soundfont");
		if (!sf.size()) { // Let's try to find a soundfont before bailing
#if defined (WIN32)
			// default for windows according to fluidsynth docs
			if (FILE *file = fopen("C:\\soundfonts\\default.sf2", "r")) {
				fclose(file);
				sf = "C:\\soundfonts\\default.sf2";
			} else if (FILE *file = fopen("C:\\DOSBox-X\\FluidR3_GM.sf2", "r")) {
				fclose(file);
				sf = "C:\\DOSBox-X\\FluidR3_GM.sf2";
			} else if (FILE *file = fopen("C:\\DOSBox-X\\GeneralUser_GS.sf2", "r")) {
				fclose(file);
				sf = "C:\\DOSBox-X\\GeneralUser_GS.sf2";
			} else {
				LOG_MSG("MIDI:fluidsynth: SoundFont not specified");
				return false;
			}
#else
			// Default on "other" platforms according to fluidsynth docs
			// This works on RH and Fedora, if a soundfont is installed
			if (FILE *file = fopen("/usr/share/soundfonts/default.sf2", "r")) {
				fclose(file);
				sf = "/usr/share/soundfonts/default.sf2";
			// Ubuntu and Debian don't have a default.sf2...
			} else if (FILE *file = fopen("/usr/share/sounds/sf2/FluidR3_GM.sf2", "r")) {
				fclose(file);
				sf = "/usr/share/sounds/sf2/FluidR3_GM.sf2";
			} else if (FILE *file = fopen("/usr/share/sounds/sf2/GeneralUser_GS.sf2", "r")) {
				fclose(file);
				sf = "/usr/share/sounds/sf2/GeneralUser_GS.sf2";
			} else {
				LOG_MSG("MIDI:fluidsynth: SoundFont not specified, and no system SoundFont found");
				return false;
			}
#endif
		}
		soundfont.assign(sf);
		settings = new_fluid_settings();
		if (strcmp(section->Get_string("fluid.driver"), "default") != 0) {
			fluid_settings_setstr(settings, "audio.driver", section->Get_string("fluid.driver"));
		}
#if defined (__linux__) // Let's use pulseaudio as default on Linux, and not the FluidSynth default of Jack
		else
		    fluid_settings_setstr(settings, "audio.driver", "pulseaudio");
#endif
		fluid_settings_setnum(settings, "synth.sample-rate", atof(section->Get_string("fluid.samplerate")));
		fluid_settings_setnum(settings, "synth.gain", atof(section->Get_string("fluid.gain")));
		fluid_settings_setint(settings, "synth.polyphony", section->Get_int("fluid.polyphony"));
		if (strcmp(section->Get_string("fluid.cores"), "default") != 0) {
			fluid_settings_setnum(settings, "synth.cpu-cores", atof(section->Get_string("fluid.cores")));
		}
		
		std::string period=section->Get_string("fluid.periods"), periodsize=section->Get_string("fluid.periodsize");
		if (period=="default") {
#if defined (WIN32)
			period="8";
#else
			period="16";
#endif
		}
		if (periodsize=="default") {
#if defined (WIN32)
			periodsize="512";
#else
			periodsize="64";
#endif
		}
#if !defined (FLUIDSYNTH_VERSION_MAJOR) || FLUIDSYNTH_VERSION_MAJOR >= 2
		fluid_settings_setint(settings, "audio.periods", atoi(period.c_str()));
		fluid_settings_setint(settings, "audio.period-size", atoi(periodsize.c_str()));
		fluid_settings_setint(settings, "synth.reverb.active", !strcmp(section->Get_string("fluid.reverb"), "yes")?1:0);
		fluid_settings_setint(settings, "synth.chorus.active", !strcmp(section->Get_string("fluid.chorus"), "yes")?1:0);
#else
		fluid_settings_setnum(settings, "audio.periods", atof(period.c_str()));
		fluid_settings_setnum(settings, "audio.period-size", atof(periodsize.c_str()));
		fluid_settings_setstr(settings, "synth.reverb.active", section->Get_string("fluid.reverb"));
		fluid_settings_setstr(settings, "synth.chorus.active", section->Get_string("fluid.chorus"));
#endif

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

		fluid_synth_set_reverb(synth, atof(section->Get_string("fluid.reverb.roomsize")), atof(section->Get_string("fluid.reverb.damping")), atof(section->Get_string("fluid.reverb.width")), atof(section->Get_string("fluid.reverb.level")));

		fluid_synth_set_chorus(synth, section->Get_int("fluid.chorus.number"), atof(section->Get_string("fluid.chorus.level")), atof(section->Get_string("fluid.chorus.speed")), atof(section->Get_string("fluid.chorus.depth")), section->Get_int("fluid.chorus.type"));

		/* Optionally load a soundfont */
		if (!soundfont.empty()) {
			soundfont_id = fluid_synth_sfload(synth, soundfont.c_str(), 1);
			if (soundfont_id == FLUID_FAILED) {
				/* Just consider this a warning (fluidsynth already prints) */
				soundfont.clear();
				soundfont_id = -1;
			}
			else {
				sffile=soundfont;
				LOG_MSG("MIDI:fluidsynth: Loaded SoundFont: %s", soundfont.c_str());
			}
		}
		else {
			soundfont_id = -1;
			LOG_MSG("MIDI:fluidsynth: No SoundFont loaded");
		}
		return true;
	}
};

MidiHandler_fluidsynth Midi_fluidsynth;
