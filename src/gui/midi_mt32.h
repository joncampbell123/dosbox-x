#include "mt32emu.h"
#include <SDL_thread.h>
#include <SDL_timer.h>
#include "mixer.h"
#include "control.h"

class RingBuffer {
private:
	static const unsigned int bufferSize = 1024;
	volatile unsigned int startpos;
	volatile unsigned int endpos;
    Bit64u ringBuffer[bufferSize] = {};

public:
	RingBuffer() {
		startpos = 0;
		endpos = 0;
	}

	bool put(Bit32u data) {
		unsigned int newEndpos = endpos;
		newEndpos++;
		if (newEndpos == bufferSize) newEndpos = 0;
		if (startpos == newEndpos) return false;
		ringBuffer[endpos] = data;
		endpos = newEndpos;
		return true;
	}

	Bit32u get() {
		if (startpos == endpos) return 0;
		Bit32u data = (Bit32u)ringBuffer[startpos]; /* <- FIXME: Um.... really? */
		startpos++;
		if (startpos == bufferSize) startpos = 0;
		return data;
	}
	void reset() {
		startpos = 0;
		endpos = 0;
	}
};

static class MidiHandler_mt32 : public MidiHandler {
private:
	static const Bitu MIXER_BUFFER_SIZE = MIXER_BUFSIZE >> 2;
	MixerChannel *chan;
	MT32Emu::Synth *synth;
	RingBuffer midiBuffer;
	SDL_Thread *thread;
	SDL_mutex *synthMutex;
	SDL_semaphore *procIdleSem, *mixerReqSem;
    Bit16s mixerBuffer[2 * MIXER_BUFFER_SIZE] = {};
	volatile Bitu mixerBufferSize = 0;
	volatile bool stopProcessing = false;
	bool open, noise = false, reverseStereo = false, renderInThread = false;
	Bit16s numPartials = 0;

	class MT32ReportHandler : public MT32Emu::ReportHandler {
	protected:
		virtual void onErrorControlROM() {
			LOG(LOG_MISC,LOG_WARN)("MT32: Couldn't open Control ROM file");
		}

		virtual void onErrorPCMROM() {
			LOG(LOG_MISC,LOG_WARN)("MT32: Couldn't open PCM ROM file");
		}

		virtual void showLCDMessage(const char *message) {
			LOG(LOG_MISC,LOG_DEBUG)("MT32: LCD-Message: %s", message);
		}

		virtual void printDebug(const char *fmt, va_list list);
	} reportHandler;

	static void mixerCallBack(Bitu len);
	static int processingThread(void *);

public:
	MidiHandler_mt32() : chan(NULL), synth(NULL), thread(NULL), synthMutex(NULL), procIdleSem(NULL), mixerReqSem(NULL), open(false) {}

	~MidiHandler_mt32() {
		MidiHandler_mt32::Close();
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
        (void)conf;//UNUSED

		MT32Emu::FileStream controlROMFile;
		MT32Emu::FileStream pcmROMFile;

        /* TODO: Add an option through midiconfig for the user to tell us where to look for ROMs.
         *       That way they don't have to sit there in the root directory of the DOS game alongside
         *       the game files. Much like Fluidsynth MIDI support where you can use midiconfig to
         *       tell it where the soundfonts are. */

		if (!controlROMFile.open("CM32L_CONTROL.ROM")) {
			if (!controlROMFile.open("MT32_CONTROL.ROM")) {
				LOG(LOG_MISC,LOG_WARN)("MT32: Control ROM file not found");
                user_romhelp();
				return false;
			}
		}
		if (!pcmROMFile.open("CM32L_PCM.ROM")) {
			if (!pcmROMFile.open("MT32_PCM.ROM")) {
				LOG(LOG_MISC,LOG_WARN)("MT32: PCM ROM file not found");
                user_romhelp();
				return false;
			}
		}

		Section_prop *section = static_cast<Section_prop *>(control->GetSection("midi"));

		const MT32Emu::ROMImage *controlROMImage = MT32Emu::ROMImage::makeROMImage(&controlROMFile);
		const MT32Emu::ROMImage *pcmROMImage = MT32Emu::ROMImage::makeROMImage(&pcmROMFile);

		synth = new MT32Emu::Synth(&reportHandler);

		/* NTS: Read and apply mt32.partials BEFORE opening the synth, because the
			synth assumes an initial number of partials, and allocates PartialManager()
			instances which in turn initialize THAT many partials. If we later
			call setPartialLimit() with the (often lower) number we wanted, then
			a memory leak will occur because the PartialManager() will only free
			the new lower limit and leave the rest in memory. */
		numPartials = section->Get_int("mt32.partials");
		if(numPartials>MT32EMU_MAX_PARTIALS) numPartials=MT32EMU_MAX_PARTIALS;
		synth->setPartialLimit((unsigned int)numPartials);

		if (!synth->open(*controlROMImage, *pcmROMImage)) {
			LOG(LOG_MISC,LOG_WARN)("MT32: Error initialising emulation");
			return false;
		}

		if (strcmp(section->Get_string("mt32.reverb.mode"), "auto") != 0) {
			Bit8u reverbsysex[] = {0x10, 0x00, 0x01, 0x00, 0x05, 0x03};
			reverbsysex[3] = (Bit8u)atoi(section->Get_string("mt32.reverb.mode"));
			reverbsysex[4] = (Bit8u)section->Get_int("mt32.reverb.time");
			reverbsysex[5] = (Bit8u)section->Get_int("mt32.reverb.level");
			synth->writeSysex(16, reverbsysex, 6);
			synth->setReverbOverridden(true);
		} else {
			LOG(LOG_MISC,LOG_DEBUG)("MT32: Using default reverb");
		}

		if (strcmp(section->Get_string("mt32.dac"), "auto") != 0) {
			synth->setDACInputMode((MT32Emu::DACInputMode)atoi(section->Get_string("mt32.dac")));
			// PURE mode = 1/2 reverb output
			if( atoi(section->Get_string("mt32.dac")) == 1 ) {
				synth->setReverbOutputGain( 0.68f / 2 );
 			}
		}

		reverseStereo = strcmp(section->Get_string("mt32.reverse.stereo"), "on") == 0;
		noise = strcmp(section->Get_string("mt32.verbose"), "on") == 0;
		renderInThread = strcmp(section->Get_string("mt32.thread"), "on") == 0;

		chan = MIXER_AddChannel(mixerCallBack, MT32Emu::SAMPLE_RATE, "MT32");
		if (renderInThread) {
			mixerBufferSize = 0;
			stopProcessing = false;
			synthMutex = SDL_CreateMutex();
			procIdleSem = SDL_CreateSemaphore(0);
			mixerReqSem = SDL_CreateSemaphore(0);
#if defined(C_SDL2)
			thread = SDL_CreateThread(processingThread, "MT32", NULL);
#else
			thread = SDL_CreateThread(processingThread, NULL);
#endif
			//if (thread == NULL || synthMutex == NULL || sleepMutex == NULL) renderInThread = false;
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
			SDL_SemPost(mixerReqSem);
			SDL_WaitThread(thread, NULL);
			thread = NULL;
			SDL_DestroyMutex(synthMutex);
			synthMutex = NULL;
			SDL_DestroySemaphore(procIdleSem);
			procIdleSem = NULL;
			SDL_DestroySemaphore(mixerReqSem);
			mixerReqSem = NULL;
		}
		MIXER_DelChannel(chan);
		chan = NULL;
		synth->close();
		delete synth;
		synth = NULL;
		open = false;
	}

	void PlayMsg(Bit8u *msg) {
		//if (!midiBuffer.put(*(Bit32u *)msg | (Bit64u(playPos + AUDIO_BUFFER_SIZE) << 32))) LOG_MSG("MT32: Playback buffer full!");
		if (!midiBuffer.put(*(Bit32u *)msg)) { } //LOG_MSG("MT32: Playback buffer full!");
	}

	void PlaySysex(Bit8u *sysex, Bitu len) {
		if (renderInThread) SDL_LockMutex(synthMutex);
		synth->playSysex(sysex, (Bit32u)len);
		if (renderInThread) SDL_UnlockMutex(synthMutex);
	}

	MT32Emu::Synth* GetSynth() { return synth; }

	void Reset() {
		midiBuffer.reset();

		if (renderInThread) SDL_LockMutex(synthMutex);
		mixerBufferSize = 0;
		if (renderInThread) SDL_UnlockMutex(synthMutex);
	}

private:
	void render(Bitu len, Bit16s *buf) {
		Bit32u msg = midiBuffer.get();
		if (msg != 0) synth->playMsg(msg);
		synth->render(buf, (Bit32u)len);
		if (reverseStereo) {
			Bit16s *revBuf = buf;
			for(Bitu i = 0; i < len; i++) {
				Bit16s left = revBuf[0];
				Bit16s right = revBuf[1];
				*revBuf++ = right;
				*revBuf++ = left;
			}
		}
		chan->AddSamples_s16(len, buf);
	}
} midiHandler_mt32;

void MidiHandler_mt32::MT32ReportHandler::printDebug(const char *fmt, va_list list) {
	if (midiHandler_mt32.noise) {
		char s[1024];
		strcpy(s, "MT32: ");
		vsnprintf(s + 6, 1017, fmt, list);
		LOG_MSG("%s", s);
	}
}

void MidiHandler_mt32::mixerCallBack(Bitu len) {
	if (midiHandler_mt32.renderInThread) {
		SDL_SemWait(midiHandler_mt32.procIdleSem);
		midiHandler_mt32.mixerBufferSize += len;
		SDL_SemPost(midiHandler_mt32.mixerReqSem);
	} else {
		midiHandler_mt32.render(len, (Bit16s *)MixTemp);
	}
}

int MidiHandler_mt32::processingThread(void *) {
	while (!midiHandler_mt32.stopProcessing) {
		SDL_SemPost(midiHandler_mt32.procIdleSem);
		SDL_SemWait(midiHandler_mt32.mixerReqSem);
		for (;;) {
			Bitu samplesToRender = midiHandler_mt32.mixerBufferSize;
			if (samplesToRender == 0) break;
			SDL_LockMutex(midiHandler_mt32.synthMutex);
			midiHandler_mt32.render(samplesToRender, midiHandler_mt32.mixerBuffer);
			SDL_UnlockMutex(midiHandler_mt32.synthMutex);
			midiHandler_mt32.mixerBufferSize -= samplesToRender;
		}
	}
	return 0;
}
