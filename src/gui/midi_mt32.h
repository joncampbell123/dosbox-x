#include <SDL_thread.h>
#include <SDL_timer.h>
#include "mixer.h"
#include "control.h"

#define MT32EMU_API_TYPE 3
#define MT32EMU_EXPORTS_TYPE 1
#include <mt32emu.h>

static const Bitu MILLIS_PER_SECOND = 1000;

class RingBuffer {
private:
	static const unsigned int bufferSize = 1024;
	volatile unsigned int startpos;
	volatile unsigned int endpos;
    uint64_t ringBuffer[bufferSize] = {};

public:
	RingBuffer() {
		startpos = 0;
		endpos = 0;
	}

	bool put(uint32_t data) {
		unsigned int newEndpos = endpos;
		newEndpos++;
		if (newEndpos == bufferSize) newEndpos = 0;
		if (startpos == newEndpos) return false;
		ringBuffer[endpos] = data;
		endpos = newEndpos;
		return true;
	}

	uint32_t get() {
		if (startpos == endpos) return 0;
		uint32_t data = (uint32_t)ringBuffer[startpos]; /* <- FIXME: Um.... really? */
		startpos++;
		if (startpos == bufferSize) startpos = 0;
		return data;
	}
	void reset() {
		startpos = 0;
		endpos = 0;
	}
};

class MidiHandler_mt32 : public MidiHandler {
private:
	MixerChannel *chan;
	MT32Emu::Service *service;
	SDL_Thread *thread;
	SDL_mutex *lock;
	SDL_cond *framesInBufferChanged;
	int16_t *audioBuffer;
	Bitu audioBufferSize;
	Bitu framesPerAudioBuffer;
	Bitu minimumRenderFrames;
	volatile Bitu renderPos, playPos, playedBuffers;
	volatile bool stopProcessing;
	bool open, noise, renderInThread;

	static void mixerCallBack(Bitu len) {
        MidiHandler_mt32::GetInstance().handleMixerCallBack(len);
    }
	static int processingThread(void *) {
        MidiHandler_mt32::GetInstance().renderingLoop();
        return 0;
    }
	static void makeROMPathName(char pathName[], const char romDir[], const char fileName[], bool addPathSeparator) {
        strcpy(pathName, romDir);
        if (addPathSeparator) {
            strcat(pathName, "/");
        }
        strcat(pathName, fileName);
    }
	static mt32emu_report_handler_i getReportHandlerInterface() {
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
                    LOG_MSG("MT32: %s", s);
                }
            }

            static void onErrorControlROM(void *) {
                LOG_MSG("MT32: Couldn't open Control ROM file");
            }

            static void onErrorPCMROM(void *) {
                LOG_MSG("MT32: Couldn't open PCM ROM file");
            }

            static void showLCDMessage(void *, const char *message) {
                LOG_MSG("MT32: LCD-Message: %s", message);
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
	void handleMixerCallBack(Bitu len) {
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
            service->renderint16_t((int16_t *)MixTemp, (MT32Emu::uint32_t)len);
            chan->AddSamples_s16(len, (int16_t *)MixTemp);
        }
    }
	void renderingLoop() {
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
                service->renderint16_t(audioBuffer + renderPosSnap, (MT32Emu::uint32_t)framesToRender);
                renderPos = (renderPosSnap + samplesToRender) % audioBufferSize;
                if (renderPosSnap == playPos) {
                    SDL_LockMutex(lock);
                    SDL_CondSignal(framesInBufferChanged);
                    SDL_UnlockMutex(lock);
                }
            }
        }
    }
	uint32_t inline getMidiEventTimestamp() {
		return service->convertOutputToSynthTimestamp(uint32_t(playedBuffers * framesPerAudioBuffer + (playPos >> 1)));
	}

public:
    MidiHandler_mt32() : open(false), chan(NULL), service(NULL), thread(NULL) {
    }

	~MidiHandler_mt32() {
		MidiHandler_mt32::Close();
	}

    static MidiHandler_mt32 & GetInstance() {
        static MidiHandler_mt32 midiHandler_mt32;
        return midiHandler_mt32;
    }

	const char *GetName(void) {
		return "mt32";
	}

    void user_romhelp(void) {
        /* don't just grunt and say "ROM file not found", help the user get the ROMs in place so we can work! */
        /* we're now 25 years past the era of limited RAM and "error code -25" we have more than enough disk and RAM
         * to contain explanatory text where the user went wrong! */
        LOG_MSG("MT32 emulation cannot work without the PCM and CONTROL ROM files.");
        LOG_MSG("To eliminate this error message, either change mididevice= to something else, or");
        LOG_MSG("place the ROM files in what will be the \"current working directory\" for DOSBox-X");
        LOG_MSG("when it starts up and initializes MIDI emulation.");
        LOG_MSG("The ROM files are: CM32L_CONTROL.ROM, MT32_CONTROL.ROM, CM32L_PCM.ROM, MT32_PCM.ROM");
    }

	bool Open(const char *conf) {
        service = new MT32Emu::Service();
        uint32_t version = service->getLibraryVersionInt();
        if (version < 0x020100) {
            delete service;
            service = NULL;
            LOG_MSG("MT32: libmt32emu version is too old: %s", service->getLibraryVersionString());
            return false;
        }
        service->createContext(getReportHandlerInterface(), this);
        mt32emu_return_code rc;

        Section_prop *section = static_cast<Section_prop *>(control->GetSection("midi"));
        std::string romDir = section->Get_string("mt32.romdir");
        void ResolvePath(std::string& in);
        ResolvePath(romDir);
        //if (romDir == NULL) romDir = "./"; // Paranoid NULL-check, should never happen
        size_t romDirLen = romDir.size();
        bool addPathSeparator = false;
        if (romDirLen < 1) {
            romDir = "./";
        } else if (4080 < romDirLen) {
            LOG_MSG("MT32: mt32.romdir is too long, using the current dir.");
            romDir = "./";
        } else {
            char lastChar = romDir.back();
            addPathSeparator = lastChar != '/' && lastChar != '\\';
        }

        char pathName[4096];
        std::string roms = "";
        makeROMPathName(pathName, romDir.c_str(), "CM32L_CONTROL.ROM", addPathSeparator);
        if (MT32EMU_RC_ADDED_CONTROL_ROM != service->addROMFile(pathName)) {
            makeROMPathName(pathName, romDir.c_str(), "MT32_CONTROL.ROM", addPathSeparator);
            if (MT32EMU_RC_ADDED_CONTROL_ROM != service->addROMFile(pathName)) {
                delete service;
                service = NULL;
                LOG_MSG("MT32: Control ROM file not found");
                user_romhelp();
                return false;
            } else
                roms = "MT32_CONTROL.ROM";
        } else
            roms = "CM32L_CONTROL.ROM";
        if (roms.size()) roms += " and ";
        makeROMPathName(pathName, romDir.c_str(), "CM32L_PCM.ROM", addPathSeparator);
        if (MT32EMU_RC_ADDED_PCM_ROM != service->addROMFile(pathName)) {
            makeROMPathName(pathName, romDir.c_str(), "MT32_PCM.ROM", addPathSeparator);
            if (MT32EMU_RC_ADDED_PCM_ROM != service->addROMFile(pathName)) {
                delete service;
                service = NULL;
                LOG_MSG("MT32: PCM ROM file not found");
                user_romhelp();
                return false;
            } else
                roms += "MT32_PCM.ROM";
        } else
            roms += "CM32L_PCM.ROM";
        LOG_MSG("MT32: Found ROM pair in %s: %s", romDir.c_str(), roms.c_str());
        sffile=romDir;

        service->setPartialCount(uint32_t(section->Get_int("mt32.partials")));
        service->setAnalogOutputMode((MT32Emu::AnalogOutputMode)section->Get_int("mt32.analog"));
        int sampleRate = section->Get_int("mt32.rate");
        service->setStereoOutputSampleRate(sampleRate);
        service->setSamplerateConversionQuality((MT32Emu::SamplerateConversionQuality)section->Get_int("mt32.src.quality"));

        if (MT32EMU_RC_OK != (rc = service->openSynth())) {
            delete service;
            service = NULL;
            LOG_MSG("MT32: Error initialising emulation: %i", rc);
            return false;
        }

        if (strcmp(section->Get_string("mt32.reverb.mode"), "auto") != 0) {
            uint8_t reverbsysex[] = {0x10, 0x00, 0x01, 0x00, 0x05, 0x03};
            reverbsysex[3] = (uint8_t)atoi(section->Get_string("mt32.reverb.mode"));
            reverbsysex[4] = (uint8_t)section->Get_int("mt32.reverb.time");
            reverbsysex[5] = (uint8_t)section->Get_int("mt32.reverb.level");
            service->writeSysex(16, reverbsysex, 6);
            service->setReverbOverridden(true);
        }

        service->setOutputGain(0.01f * section->Get_int("mt32.output.gain"));
        service->setReverbOutputGain(0.01f * section->Get_int("mt32.reverb.output.gain"));
        service->setDACInputMode((MT32Emu::DACInputMode)section->Get_int("mt32.dac"));

        service->setReversedStereoEnabled(section->Get_bool("mt32.reverse.stereo"));
        service->setNiceAmpRampEnabled(section->Get_bool("mt32.niceampramp"));
        noise = section->Get_bool("mt32.verbose");
        renderInThread = section->Get_bool("mt32.thread");

        if (noise) LOG_MSG("MT32: Set maximum number of partials %d", service->getPartialCount());

        if (noise) LOG_MSG("MT32: Adding mixer channel at sample rate %d", sampleRate);
        chan = MIXER_AddChannel(mixerCallBack, sampleRate, "MT32");

        if (renderInThread) {
            stopProcessing = false;
            playPos = 0;
            int chunkSize = section->Get_int("mt32.chunk");
            minimumRenderFrames = (chunkSize * sampleRate) / MILLIS_PER_SECOND;
            int latency = section->Get_int("mt32.prebuffer");
            if (latency <= chunkSize) {
                latency = 2 * chunkSize;
                LOG_MSG("MT32: chunk length must be less than prebuffer length, prebuffer length reset to %i ms.", latency);
            }
            framesPerAudioBuffer = (latency * sampleRate) / MILLIS_PER_SECOND;
            audioBufferSize = framesPerAudioBuffer << 1;
            audioBuffer = new int16_t[audioBufferSize];
            service->renderint16_t(audioBuffer, (MT32Emu::uint32_t)(framesPerAudioBuffer - 1));
            renderPos = (framesPerAudioBuffer - 1) << 1;
            playedBuffers = 1;
            lock = SDL_CreateMutex();
            framesInBufferChanged = SDL_CreateCond();
#if defined(C_SDL2)
			thread = SDL_CreateThread(processingThread, "MT32", NULL);
#else
			thread = SDL_CreateThread(processingThread, NULL);
#endif
        }
        chan->Enable(true);

        open = true;
        return true;
	}

	void Close(void) {
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

	void PlayMsg(uint8_t *msg) {
        if (renderInThread) {
            service->playMsgAt(SDL_SwapLE32(*(uint32_t *)msg), getMidiEventTimestamp());
        } else {
            service->playMsg(SDL_SwapLE32(*(uint32_t *)msg));
        }
	}

	void PlaySysex(uint8_t *sysex, Bitu len) {
        if (renderInThread) {
            service->playSysexAt(sysex, (MT32Emu::uint32_t)len, getMidiEventTimestamp());
        } else {
            service->playSysex(sysex, (MT32Emu::uint32_t)len);
        }
	}
};

static MidiHandler_mt32 &Midi_mt32 = MidiHandler_mt32::GetInstance();
