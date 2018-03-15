#include <SDL_thread.h>
#include <SDL_endian.h>
#include "control.h"

#ifndef DOSBOX_MIDI_H
#include "midi.h"
#endif

#include "midi_mt32.h"

static const Bitu MILLIS_PER_SECOND = 1000;

MidiHandler_mt32 &MidiHandler_mt32::GetInstance() {
	static MidiHandler_mt32 midiHandler_mt32;
	return midiHandler_mt32;
}

const char *MidiHandler_mt32::GetName(void) {
	return "mt32";
}

bool MidiHandler_mt32::Open(const char *conf) {
	service = new MT32Emu::Service();
	Bit32u version = service->getLibraryVersionInt();
	if (version < 0x020100) {
		delete service;
		service = NULL;
		LOG(LOG_MISC,LOG_WARN)("MT32: libmt32emu version is too old: %s", service->getLibraryVersionString());
		return false;
	}
	service->createContext(getReportHandlerInterface(), this);
	mt32emu_return_code rc;

	Section_prop *section = static_cast<Section_prop *>(control->GetSection("midi"));
	const char *romDir = section->Get_string("mt32.romdir");
	if (romDir == NULL) romDir = "./"; // Paranoid NULL-check, should never happen
	size_t romDirLen = strlen(romDir);
	bool addPathSeparator = false;
	if (romDirLen < 1) {
		romDir = "./";
	} else if (4080 < romDirLen) {
		LOG(LOG_MISC,LOG_WARN)("MT32: mt32.romdir is too long, using the current dir.");
		romDir = "./";
	} else {
		char lastChar = romDir[strlen(romDir) - 1];
		addPathSeparator = lastChar != '/' && lastChar != '\\';
	}

	char pathName[4096];

	makeROMPathName(pathName, romDir, "CM32L_CONTROL.ROM", addPathSeparator);
	if (MT32EMU_RC_ADDED_CONTROL_ROM != service->addROMFile(pathName)) {
		makeROMPathName(pathName, romDir, "MT32_CONTROL.ROM", addPathSeparator);
		if (MT32EMU_RC_ADDED_CONTROL_ROM != service->addROMFile(pathName)) {
			delete service;
			service = NULL;
			LOG(LOG_MISC,LOG_WARN)("MT32: Control ROM file not found");
			return false;
		}
	}
	makeROMPathName(pathName, romDir, "CM32L_PCM.ROM", addPathSeparator);
	if (MT32EMU_RC_ADDED_PCM_ROM != service->addROMFile(pathName)) {
		makeROMPathName(pathName, romDir, "MT32_PCM.ROM", addPathSeparator);
		if (MT32EMU_RC_ADDED_PCM_ROM != service->addROMFile(pathName)) {
			delete service;
			service = NULL;
			LOG(LOG_MISC,LOG_WARN)("MT32: PCM ROM file not found");
			return false;
		}
	}

	service->setPartialCount(Bit32u(section->Get_int("mt32.partials")));
	service->setAnalogOutputMode((MT32Emu::AnalogOutputMode)section->Get_int("mt32.analog"));
	int sampleRate = section->Get_int("mt32.rate");
	service->setStereoOutputSampleRate(sampleRate);
	service->setSamplerateConversionQuality((MT32Emu::SamplerateConversionQuality)section->Get_int("mt32.src.quality"));

	if (MT32EMU_RC_OK != (rc = service->openSynth())) {
		delete service;
		service = NULL;
		LOG(LOG_MISC,LOG_WARN)("MT32: Error initialising emulation: %i", rc);
		return false;
	}

	if (strcmp(section->Get_string("mt32.reverb.mode"), "auto") != 0) {
		Bit8u reverbsysex[] = {0x10, 0x00, 0x01, 0x00, 0x05, 0x03};
		reverbsysex[3] = (Bit8u)atoi(section->Get_string("mt32.reverb.mode"));
		reverbsysex[4] = (Bit8u)section->Get_int("mt32.reverb.time");
		reverbsysex[5] = (Bit8u)section->Get_int("mt32.reverb.level");
		service->writeSysex(16, reverbsysex, 6);
		service->setReverbOverridden(true);
	}

	service->setDACInputMode((MT32Emu::DACInputMode)section->Get_int("mt32.dac"));

	service->setReversedStereoEnabled(section->Get_bool("mt32.reverse.stereo"));
	service->setNiceAmpRampEnabled(section->Get_bool("mt32.niceampramp"));
	noise = section->Get_bool("mt32.verbose");
	renderInThread = section->Get_bool("mt32.thread");

	if (noise) LOG(LOG_MISC,LOG_WARN)("MT32: Set maximum number of partials %d", service->getPartialCount());

	if (noise) LOG(LOG_MISC,LOG_WARN)("MT32: Adding mixer channel at sample rate %d", sampleRate);
	chan = MIXER_AddChannel(mixerCallBack, sampleRate, "MT32");

	if (renderInThread) {
		stopProcessing = false;
		playPos = 0;
		int chunkSize = section->Get_int("mt32.chunk");
		minimumRenderFrames = (chunkSize * sampleRate) / MILLIS_PER_SECOND;
		int latency = section->Get_int("mt32.prebuffer");
		if (latency <= chunkSize) {
			latency = 2 * chunkSize;
			LOG(LOG_MISC,LOG_WARN)("MT32: chunk length must be less than prebuffer length, prebuffer length reset to %i ms.", latency);
		}
		framesPerAudioBuffer = (latency * sampleRate) / MILLIS_PER_SECOND;
		audioBufferSize = framesPerAudioBuffer << 1;
		audioBuffer = new Bit16s[audioBufferSize];
		service->renderBit16s(audioBuffer, framesPerAudioBuffer - 1);
		renderPos = (framesPerAudioBuffer - 1) << 1;
		playedBuffers = 1;
		lock = SDL_CreateMutex();
		framesInBufferChanged = SDL_CreateCond();
		thread = SDL_CreateThread(processingThread, NULL);
	}
	chan->Enable(true);

	open = true;
	return true;
}

void MidiHandler_mt32::Close(void) {
	if (!open) return;
	chan->Enable(false);
	if (renderInThread) {
		stopProcessing = true;
		SDL_LockMutex(lock);
		SDL_CondSignal(framesInBufferChanged);
		SDL_UnlockMutex(lock);
		SDL_WaitThread(thread, NULL);
		thread = NULL;
		SDL_DestroyMutex(lock);
		lock = NULL;
		SDL_DestroyCond(framesInBufferChanged);
		framesInBufferChanged = NULL;
		delete[] audioBuffer;
		audioBuffer = NULL;
	}
	MIXER_DelChannel(chan);
	chan = NULL;
	service->closeSynth();
	delete service;
	service = NULL;
	open = false;
}

void MidiHandler_mt32::PlayMsg(Bit8u *msg) {
	if (renderInThread) {
		service->playMsgAt(SDL_SwapLE32(*(Bit32u *)msg), getMidiEventTimestamp());
	} else {
		service->playMsg(SDL_SwapLE32(*(Bit32u *)msg));
	}
}

void MidiHandler_mt32::PlaySysex(Bit8u *sysex, Bitu len) {
	if (renderInThread) {
		service->playSysexAt(sysex, len, getMidiEventTimestamp());
	} else {
		service->playSysex(sysex, len);
	}
}

void MidiHandler_mt32::mixerCallBack(Bitu len) {
	MidiHandler_mt32::GetInstance().handleMixerCallBack(len);
}

int MidiHandler_mt32::processingThread(void *) {
	MidiHandler_mt32::GetInstance().renderingLoop();
	return 0;
}

void MidiHandler_mt32::makeROMPathName(char pathName[], const char romDir[], const char fileName[], bool addPathSeparator) {
	strcpy(pathName, romDir);
	if (addPathSeparator) {
		strcat(pathName, "/");
	}
	strcat(pathName, fileName);
}

mt32emu_report_handler_i MidiHandler_mt32::getReportHandlerInterface() {
	class ReportHandler {
	public:
		static mt32emu_report_handler_version getReportHandlerVersionID(mt32emu_report_handler_i) {
			return MT32EMU_REPORT_HANDLER_VERSION_0;
		}

		static void printDebug(void *instance_data, const char *fmt, va_list list) {
			MidiHandler_mt32 &midiHandler_mt32 = *(MidiHandler_mt32 *)instance_data;
			if (midiHandler_mt32.noise) {
				char s[1024];
				vsnprintf(s, 1023, fmt, list);
				LOG(LOG_MISC,LOG_WARN)("MT32: %s", s);
			}
		}

		static void onErrorControlROM(void *) {
			LOG(LOG_MISC,LOG_WARN)("MT32: Couldn't open Control ROM file");
			LOG(LOG_MISC,LOG_WARN)("MT32 emulation cannot work without the CONTROL ROM files.");
			LOG(LOG_MISC,LOG_WARN)("To eliminate this error message, either change mididevice= to something else, or");
			LOG(LOG_MISC,LOG_WARN)("point the emulator to the directory containing");
			LOG(LOG_MISC,LOG_WARN)("CM32L_CONTROL.ROM and MT32_CONTROL.ROM using the romdir= configuration option");
		}

		static void onErrorPCMROM(void *) {
			LOG(LOG_MISC,LOG_WARN)("MT32: Couldn't open PCM ROM file");
			LOG(LOG_MISC,LOG_WARN)("MT32 emulation cannot work without the PCM ROM files.");
			LOG(LOG_MISC,LOG_WARN)("To eliminate this error message, either change mididevice= to something else, or");
			LOG(LOG_MISC,LOG_WARN)("point the emulator to the directory containing");
			LOG(LOG_MISC,LOG_WARN)("CM32L_PCM.ROM and MT32_PCM.ROM using the romdir= configuration option");
		}

		static void showLCDMessage(void *, const char *message) {
			LOG(LOG_MISC,LOG_DEBUG)("MT32: LCD-Message: %s", message);
		}
	};

	static const mt32emu_report_handler_i_v0 REPORT_HANDLER_V0_IMPL = {
		ReportHandler::getReportHandlerVersionID,
		ReportHandler::printDebug,
		ReportHandler::onErrorControlROM,
		ReportHandler::onErrorPCMROM,
		ReportHandler::showLCDMessage
	};

	static const mt32emu_report_handler_i REPORT_HANDLER_I = { &REPORT_HANDLER_V0_IMPL };

	return REPORT_HANDLER_I;
}

MidiHandler_mt32::MidiHandler_mt32() : chan(NULL), service(NULL), thread(NULL), open(false) {
}

MidiHandler_mt32::~MidiHandler_mt32() {
	Close();
}

void MidiHandler_mt32::handleMixerCallBack(Bitu len) {
	if (renderInThread) {
		while (renderPos == playPos) {
			SDL_LockMutex(lock);
			SDL_CondWait(framesInBufferChanged, lock);
			SDL_UnlockMutex(lock);
			if (stopProcessing) return;
		}
		Bitu renderPosSnap = renderPos;
		Bitu playPosSnap = playPos;
		Bitu samplesReady = (renderPosSnap < playPosSnap) ? audioBufferSize - playPosSnap : renderPosSnap - playPosSnap;
		if (len > (samplesReady >> 1)) {
			len = samplesReady >> 1;
		}
		chan->AddSamples_s16(len, audioBuffer + playPosSnap);
		playPosSnap += (len << 1);
		while (audioBufferSize <= playPosSnap) {
			playPosSnap -= audioBufferSize;
			playedBuffers++;
		}
		playPos = playPosSnap;
		renderPosSnap = renderPos;
		const Bitu samplesFree = (renderPosSnap < playPosSnap) ? playPosSnap - renderPosSnap : audioBufferSize + playPosSnap - renderPosSnap;
		if (minimumRenderFrames <= (samplesFree >> 1)) {
			SDL_LockMutex(lock);
			SDL_CondSignal(framesInBufferChanged);
			SDL_UnlockMutex(lock);
		}
	} else {
		service->renderBit16s((Bit16s *)MixTemp, len);
		chan->AddSamples_s16(len, (Bit16s *)MixTemp);
	}
}

void MidiHandler_mt32::renderingLoop() {
	while (!stopProcessing) {
		const Bitu renderPosSnap = renderPos;
		const Bitu playPosSnap = playPos;
		Bitu samplesToRender;
		if (renderPosSnap < playPosSnap) {
			samplesToRender = playPosSnap - renderPosSnap - 2;
		} else {
			samplesToRender = audioBufferSize - renderPosSnap;
			if (playPosSnap == 0) samplesToRender -= 2;
		}
		Bitu framesToRender = samplesToRender >> 1;
		if ((framesToRender == 0) || ((framesToRender < minimumRenderFrames) && (renderPosSnap < playPosSnap))) {
			SDL_LockMutex(lock);
			SDL_CondWait(framesInBufferChanged, lock);
			SDL_UnlockMutex(lock);
		} else {
			service->renderBit16s(audioBuffer + renderPosSnap, framesToRender);
			renderPos = (renderPosSnap + samplesToRender) % audioBufferSize;
			if (renderPosSnap == playPos) {
				SDL_LockMutex(lock);
				SDL_CondSignal(framesInBufferChanged);
				SDL_UnlockMutex(lock);
			}
		}
	}
}