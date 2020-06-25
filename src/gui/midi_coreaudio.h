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
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */


#include <AudioToolbox/AUGraph.h>
#include <CoreServices/CoreServices.h>

// A macro to simplify error handling a bit.
#define RequireNoErr(error)                                         \
do {                                                                \
	err = error;                                                    \
	if (err != noErr)                                               \
		goto bail;                                                  \
} while (false)

// With the release of Mac OS X 10.5 in October 2007, Apple deprecated the
// AUGraphNewNode & AUGraphGetNodeInfo APIs in favor of the new AUGraphAddNode &
// AUGraphNodeInfo APIs. While it is easy to switch to those, it breaks
// compatibility with all pre-10.5 systems.
//
// Since 10.5 was the last system to support PowerPC, we use the old, deprecated
// APIs on PowerPC based systems by default. On all other systems (such as Mac
// OS X running on Intel hardware, or iOS running on ARM), we use the new API by
// default.
//
// This leaves Mac OS X 10.4 running on x86 processors as the only system
// combination that this code will not support by default. It seems quite
// reasonable to assume that anybody with an Intel system has since then moved
// on to a newer Mac OS X release. But if for some reason you absolutely need to
// build an x86 version of this code using the old, deprecated API, you can
// simply do so by manually enable the USE_DEPRECATED_COREAUDIO_API switch (e.g.
// by adding setting it suitably in CPPFLAGS).
#if !defined(USE_DEPRECATED_COREAUDIO_API)
#   if TARGET_CPU_PPC || TARGET_CPU_PPC64
#      define USE_DEPRECATED_COREAUDIO_API 1
#   else
#      define USE_DEPRECATED_COREAUDIO_API 0
#   endif
#endif

class MidiHandler_coreaudio : public MidiHandler {
private:
	AUGraph m_auGraph;
	AudioUnit m_synth;
        const char *soundfont;
public:
	MidiHandler_coreaudio() : m_auGraph(0), m_synth(0) {}
	const char * GetName(void) { return "coreaudio"; }
	bool Open(const char * conf) {
		OSStatus err = 0;

		if (m_auGraph)
			return false;

		// Open the Music Device.
		RequireNoErr(NewAUGraph(&m_auGraph));

		AUNode outputNode, synthNode;
		// OS X 10.5 SDK doesn't know AudioComponentDescription desc;
#if USE_DEPRECATED_COREAUDIO_API || (MAC_OS_X_VERSION_MAX_ALLOWED <= 1050)
		ComponentDescription desc;
#else
		AudioComponentDescription desc;
#endif

		// The default output device
		desc.componentType = kAudioUnitType_Output;
		desc.componentSubType = kAudioUnitSubType_DefaultOutput;
		desc.componentManufacturer = kAudioUnitManufacturer_Apple;
		desc.componentFlags = 0;
		desc.componentFlagsMask = 0;
#if USE_DEPRECATED_COREAUDIO_API
		RequireNoErr(AUGraphNewNode(m_auGraph, &desc, 0, NULL, &outputNode));
#else
		RequireNoErr(AUGraphAddNode(m_auGraph, &desc, &outputNode));
#endif

		// The built-in default (softsynth) music device
		desc.componentType = kAudioUnitType_MusicDevice;
		desc.componentSubType = kAudioUnitSubType_DLSSynth;
		desc.componentManufacturer = kAudioUnitManufacturer_Apple;
#if USE_DEPRECATED_COREAUDIO_API
		RequireNoErr(AUGraphNewNode(m_auGraph, &desc, 0, NULL, &synthNode));
#else
		RequireNoErr(AUGraphAddNode(m_auGraph, &desc, &synthNode));
#endif

		// Connect the softsynth to the default output
		RequireNoErr(AUGraphConnectNodeInput(m_auGraph, synthNode, 0, outputNode, 0));

		// Open and initialize the whole graph
		RequireNoErr(AUGraphOpen(m_auGraph));
		RequireNoErr(AUGraphInitialize(m_auGraph));

		// Get the music device from the graph.
#if USE_DEPRECATED_COREAUDIO_API
		RequireNoErr(AUGraphGetNodeInfo(m_auGraph, synthNode, NULL, NULL, NULL, &m_synth));
#else
		RequireNoErr(AUGraphNodeInfo(m_auGraph, synthNode, NULL, &m_synth));
#endif

		// Optionally load a soundfont 
		if (conf && conf[0]) {
			soundfont = conf;
			OSErr err = 0;
#if USE_DEPRECATED_COREAUDIO_API
			FSRef soundfontRef;
			err = FSPathMakeRef((const UInt8*)soundfont, &soundfontRef, NULL);
			if (!err) {
				err = AudioUnitSetProperty(
				                           m_synth,
				                           kMusicDeviceProperty_SoundBankFSRef,
				                           kAudioUnitScope_Global,
				                           0,
				                           &soundfontRef,
				                           sizeof(soundfontRef)
				                          );
					}
#else
					// kMusicDeviceProperty_SoundBankFSRef is present on 10.6+, but
					// kMusicDeviceProperty_SoundBankURL was added in 10.5 as a future prooof replacement
					CFURLRef url = CFURLCreateFromFileSystemRepresentation(
					                                                       kCFAllocatorDefault,
					                                                       (const UInt8*)soundfont,
					                                                       strlen(soundfont), false
					                                                      );
					if (url) {
						err = AudioUnitSetProperty(
						                           m_synth, kMusicDeviceProperty_SoundBankURL,
						                           kAudioUnitScope_Global, 0, &url, sizeof(url)
						                          );
						CFRelease(url);
					} else {
						LOG_MSG("Failed to allocate CFURLRef from  %s",soundfont);
					}
#endif
					if (!err) {
						LOG_MSG("MIDI:coreaudio: loaded soundfont: %s",soundfont);
					} else {
						LOG_MSG("Error loading CoreAudio SoundFont %s",soundfont);
						// after trying and failing to load a soundfont it's better
						// to fail initializing the CoreAudio driver or it might crash
						return false;
					}
				}		

		// Finally: Start the graph!
		RequireNoErr(AUGraphStart(m_auGraph));

		return true;

	bail:
		if (m_auGraph) {
			AUGraphStop(m_auGraph);
			DisposeAUGraph(m_auGraph);
			m_auGraph = 0;
		}
		return false;
	}

	void Close(void) {
		if (m_auGraph) {
			AUGraphStop(m_auGraph);
			DisposeAUGraph(m_auGraph);
			m_auGraph = 0;
		}
	}

	void PlayMsg(Bit8u * msg) {
		MusicDeviceMIDIEvent(m_synth, msg[0], msg[1], msg[2], 0);
	}	

	void PlaySysex(Bit8u * sysex, Bitu len) {
		MusicDeviceSysEx(m_synth, sysex, len);
	}
};

#undef RequireNoErr

MidiHandler_coreaudio Midi_coreaudio;
