/* Copyright (C) 2003, 2004, 2005, 2006, 2008, 2009 Dean Beeler, Jerome Fisher
 * Copyright (C) 2011, 2012, 2013 Dean Beeler, Jerome Fisher, Sergey V. Mikayev
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation, either version 2.1 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "mt32emu.h"

#if MT32EMU_USE_REVERBMODEL == 1

#include "AReverbModel.h"

// Analysing of state of reverb RAM address lines gives exact sizes of the buffers of filters used. This also indicates that
// the reverb model implemented in the real devices consists of three series allpass filters preceded by a non-feedback comb (or a delay with a LPF)
// and followed by three parallel comb filters

namespace MT32Emu {

// Because LA-32 chip makes it's output available to process by the Boss chip with a significant delay,
// the Boss chip puts to the buffer the LA32 dry output when it is ready and performs processing of the _previously_ latched data.
// Of course, the right way would be to use a dedicated variable for this, but our reverb model is way higher level,
// so we can simply increase the input buffer size.
static const Bit32u PROCESS_DELAY = 1;

// Default reverb settings for modes 0-2. These correspond to CM-32L / LAPC-I "new" reverb settings. MT-32 reverb is a bit different.
// Found by tracing reverb RAM data lines (thanks go to Lord_Nightmare & balrog).

static const Bit32u NUM_ALLPASSES = 3;
static const Bit32u NUM_COMBS = 4; // Well, actually there are 3 comb filters, but the entrance LPF + delay can be perfectly processed via a comb here.

static const Bit32u MODE_0_ALLPASSES[] = {994, 729, 78};
static const Bit32u MODE_0_COMBS[] = {705 + PROCESS_DELAY, 2349, 2839, 3632};
static const Bit32u MODE_0_OUTL[] = {2349, 141, 1960};
static const Bit32u MODE_0_OUTR[] = {1174, 1570, 145};
static const Bit32u MODE_0_COMB_FACTOR[] = {0x3C, 0x60, 0x60, 0x60};
static const Bit32u MODE_0_COMB_FEEDBACK[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                              0x28, 0x48, 0x60, 0x78, 0x80, 0x88, 0x90, 0x98,
                                              0x28, 0x48, 0x60, 0x78, 0x80, 0x88, 0x90, 0x98,
                                              0x28, 0x48, 0x60, 0x78, 0x80, 0x88, 0x90, 0x98};
static const Bit32u MODE_0_LEVELS[] = {10*1, 10*3, 10*5, 10*7, 11*9, 11*12, 11*15, 13*15};
static const Bit32u MODE_0_LPF_AMP = 6;

static const Bit32u MODE_1_ALLPASSES[] = {1324, 809, 176};
static const Bit32u MODE_1_COMBS[] = {961 + PROCESS_DELAY, 2619, 3545, 4519};
static const Bit32u MODE_1_OUTL[] = {2618, 1760, 4518};
static const Bit32u MODE_1_OUTR[] = {1300, 3532, 2274};
static const Bit32u MODE_1_COMB_FACTOR[] = {0x30, 0x60, 0x60, 0x60};
static const Bit32u MODE_1_COMB_FEEDBACK[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
											  0x28, 0x48, 0x60, 0x70, 0x78, 0x80, 0x90, 0x98,
											  0x28, 0x48, 0x60, 0x78, 0x80, 0x88, 0x90, 0x98,
											  0x28, 0x48, 0x60, 0x78, 0x80, 0x88, 0x90, 0x98};
static const Bit32u MODE_1_LEVELS[] = {10*1, 10*3, 11*5, 11*7, 11*9, 11*12, 11*15, 14*15};
static const Bit32u MODE_1_LPF_AMP = 6;

static const Bit32u MODE_2_ALLPASSES[] = {969, 644, 157};
static const Bit32u MODE_2_COMBS[] = {116 + PROCESS_DELAY, 2259, 2839, 3539};
static const Bit32u MODE_2_OUTL[] = {2259, 718, 1769};
static const Bit32u MODE_2_OUTR[] = {1136, 2128, 1};
static const Bit32u MODE_2_COMB_FACTOR[] = {0, 0x20, 0x20, 0x20};
static const Bit32u MODE_2_COMB_FEEDBACK[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                              0x30, 0x58, 0x78, 0x88, 0xA0, 0xB8, 0xC0, 0xD0,
                                              0x30, 0x58, 0x78, 0x88, 0xA0, 0xB8, 0xC0, 0xD0,
                                              0x30, 0x58, 0x78, 0x88, 0xA0, 0xB8, 0xC0, 0xD0};
static const Bit32u MODE_2_LEVELS[] = {10*1, 10*3, 11*5, 11*7, 11*9, 11*12, 12*15, 14*15};
static const Bit32u MODE_2_LPF_AMP = 8;

static const AReverbSettings REVERB_MODE_0_SETTINGS = {MODE_0_ALLPASSES, MODE_0_COMBS, MODE_0_OUTL, MODE_0_OUTR, MODE_0_COMB_FACTOR, MODE_0_COMB_FEEDBACK, MODE_0_LEVELS, MODE_0_LPF_AMP};
static const AReverbSettings REVERB_MODE_1_SETTINGS = {MODE_1_ALLPASSES, MODE_1_COMBS, MODE_1_OUTL, MODE_1_OUTR, MODE_1_COMB_FACTOR, MODE_1_COMB_FEEDBACK, MODE_1_LEVELS, MODE_1_LPF_AMP};
static const AReverbSettings REVERB_MODE_2_SETTINGS = {MODE_2_ALLPASSES, MODE_2_COMBS, MODE_2_OUTL, MODE_2_OUTR, MODE_2_COMB_FACTOR, MODE_2_COMB_FEEDBACK, MODE_2_LEVELS, MODE_2_LPF_AMP};

static const AReverbSettings * const REVERB_SETTINGS[] = {&REVERB_MODE_0_SETTINGS, &REVERB_MODE_1_SETTINGS, &REVERB_MODE_2_SETTINGS, &REVERB_MODE_0_SETTINGS};

RingBuffer::RingBuffer(const Bit32u newsize) : size(newsize), index(0) {
	buffer = new float[size];
}

RingBuffer::~RingBuffer() {
	delete[] buffer;
	buffer = NULL;
}

float RingBuffer::next() {
	if (++index >= size) {
		index = 0;
	}
	return buffer[index];
}

bool RingBuffer::isEmpty() const {
	if (buffer == NULL) return true;

	float *buf = buffer;
	float max = 0.001f;
	for (Bit32u i = 0; i < size; i++) {
		if ((*buf < -max) || (*buf > max)) return false;
		buf++;
	}
	return true;
}

void RingBuffer::mute() {
	float *buf = buffer;
	for (Bit32u i = 0; i < size; i++) {
		*buf++ = 0;
	}
}

AllpassFilter::AllpassFilter(const Bit32u useSize) : RingBuffer(useSize) {}

float AllpassFilter::process(const float in) {
	// This model corresponds to the allpass filter implementation of the real CM-32L device
	// found from sample analysis

	const float bufferOut = next();

	// store input - feedback / 2
	buffer[index] = in - 0.5f * bufferOut;

	// return buffer output + feedforward / 2
	return bufferOut + 0.5f * buffer[index];
}

CombFilter::CombFilter(const Bit32u useSize) : RingBuffer(useSize), feedbackFactor(0.0F), filterFactor(0.0F) {}

void CombFilter::process(const float in) {
	// This model corresponds to the comb filter implementation of the real CM-32L device
	// found from sample analysis

	// the previously stored value
	float last = buffer[index];

	// prepare input + feedback
	float filterIn = in + next() * feedbackFactor;

	// store input + feedback processed by a low-pass filter
	buffer[index] = filterFactor * last - filterIn;
}

float CombFilter::getOutputAt(const Bit32u outIndex) const {
	return buffer[(size + index - outIndex) % size];
}

void CombFilter::setFeedbackFactor(const float useFeedbackFactor) {
	feedbackFactor = useFeedbackFactor;
}

void CombFilter::setFilterFactor(const float useFilterFactor) {
	filterFactor = useFilterFactor;
}

AReverbModel::AReverbModel(const ReverbMode mode) : allpasses(NULL), combs(NULL), currentSettings(*REVERB_SETTINGS[mode]), lpfAmp(0.0F), wetLevel(0.0F) {}

AReverbModel::~AReverbModel() {
	AReverbModel::close();
}

void AReverbModel::open() {
	allpasses = new AllpassFilter*[NUM_ALLPASSES];
	for (Bit32u i = 0; i < NUM_ALLPASSES; i++) {
		allpasses[i] = new AllpassFilter(currentSettings.allpassSizes[i]);
	}
	combs = new CombFilter*[NUM_COMBS];
	for (Bit32u i = 0; i < NUM_COMBS; i++) {
		combs[i] = new CombFilter(currentSettings.combSizes[i]);
		combs[i]->setFilterFactor(currentSettings.filterFactor[i] / 256.0f);
	}
	lpfAmp = currentSettings.lpfAmp / 16.0f;
	mute();
}

void AReverbModel::close() {
	if (allpasses != NULL) {
		for (Bit32u i = 0; i < NUM_ALLPASSES; i++) {
			if (allpasses[i] != NULL) {
				delete allpasses[i];
				allpasses[i] = NULL;
			}
		}
		delete[] allpasses;
		allpasses = NULL;
	}
	if (combs != NULL) {
		for (Bit32u i = 0; i < NUM_COMBS; i++) {
			if (combs[i] != NULL) {
				delete combs[i];
				combs[i] = NULL;
			}
		}
		delete[] combs;
		combs = NULL;
	}
}

void AReverbModel::mute() {
	if (allpasses == NULL || combs == NULL) return;
	for (Bit32u i = 0; i < NUM_ALLPASSES; i++) {
		allpasses[i]->mute();
	}
	for (Bit32u i = 0; i < NUM_COMBS; i++) {
		combs[i]->mute();
	}
}

void AReverbModel::setParameters(Bit8u time, Bit8u level) {
// FIXME: wetLevel definitely needs ramping when changed
// Although, most games don't set reverb level during MIDI playback
	if (combs == NULL) return;
	level &= 7;
	time &= 7;
	for (Bit32u i = 0; i < NUM_COMBS; i++) {
		combs[i]->setFeedbackFactor(currentSettings.decayTimes[(i << 3) + time] / 256.0f);
	}
	wetLevel = (level == 0 && time == 0) ? 0.0f : 0.5f * lpfAmp * currentSettings.wetLevels[level] / 256.0f;
}

bool AReverbModel::isActive() const {
	for (Bit32u i = 0; i < NUM_ALLPASSES; i++) {
		if (!allpasses[i]->isEmpty()) return true;
	}
	for (Bit32u i = 0; i < NUM_COMBS; i++) {
		if (!combs[i]->isEmpty()) return true;
	}
	return false;
}

void AReverbModel::process(const float *inLeft, const float *inRight, float *outLeft, float *outRight, unsigned long numSamples) {
	for (unsigned long i = 0; i < numSamples; i++) {
		float dry = wetLevel * (*inLeft + *inRight);

		// Get the last stored sample before processing in order not to loose it
		float link = combs[0]->getOutputAt(currentSettings.combSizes[0] - 1);

		combs[0]->process(-dry);

		link = allpasses[0]->process(link);
		link = allpasses[1]->process(link);
		link = allpasses[2]->process(link);

		// If the output position is equal to the comb size, get it now in order not to loose it
		float outL1 = 1.5f * combs[1]->getOutputAt(currentSettings.outLPositions[0] - 1);

		combs[1]->process(link);
		combs[2]->process(link);
		combs[3]->process(link);

		link = outL1 + 1.5f * combs[2]->getOutputAt(currentSettings.outLPositions[1]);
		link += combs[3]->getOutputAt(currentSettings.outLPositions[2]);
		*outLeft = link;

		link = 1.5f * combs[1]->getOutputAt(currentSettings.outRPositions[0]);
		link += 1.5f * combs[2]->getOutputAt(currentSettings.outRPositions[1]);
		link += combs[3]->getOutputAt(currentSettings.outRPositions[2]);
		*outRight = link;

		inLeft++;
		inRight++;
		outLeft++;
		outRight++;
	}
}

}

#endif
