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

#if MT32EMU_USE_REVERBMODEL == 2

#include "BReverbModel.h"

// Analysing of state of reverb RAM address lines gives exact sizes of the buffers of filters used. This also indicates that
// the reverb model implemented in the real devices consists of three series allpass filters preceded by a non-feedback comb (or a delay with a LPF)
// and followed by three parallel comb filters

namespace MT32Emu {

// Because LA-32 chip makes it's output available to process by the Boss chip with a significant delay,
// the Boss chip puts to the buffer the LA32 dry output when it is ready and performs processing of the _previously_ latched data.
// Of course, the right way would be to use a dedicated variable for this, but our reverb model is way higher level,
// so we can simply increase the input buffer size.
static const Bit32u PROCESS_DELAY = 1;

static const Bit32u MODE_3_ADDITIONAL_DELAY = 1;
static const Bit32u MODE_3_FEEDBACK_DELAY = 1;

// Default reverb settings for modes 0-2. These correspond to CM-32L / LAPC-I "new" reverb settings. MT-32 reverb is a bit different.
// Found by tracing reverb RAM data lines (thanks go to Lord_Nightmare & balrog).

static const Bit32u MODE_0_NUMBER_OF_ALLPASSES = 3;
static const Bit32u MODE_0_ALLPASSES[] = {994, 729, 78};
static const Bit32u MODE_0_NUMBER_OF_COMBS = 4; // Well, actually there are 3 comb filters, but the entrance LPF + delay can be processed via a hacked comb.
static const Bit32u MODE_0_COMBS[] = {705 + PROCESS_DELAY, 2349, 2839, 3632};
static const Bit32u MODE_0_OUTL[] = {2349, 141, 1960};
static const Bit32u MODE_0_OUTR[] = {1174, 1570, 145};
static const Bit32u MODE_0_COMB_FACTOR[] = {0xA0, 0x60, 0x60, 0x60};
static const Bit32u MODE_0_COMB_FEEDBACK[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                              0x28, 0x48, 0x60, 0x78, 0x80, 0x88, 0x90, 0x98,
                                              0x28, 0x48, 0x60, 0x78, 0x80, 0x88, 0x90, 0x98,
                                              0x28, 0x48, 0x60, 0x78, 0x80, 0x88, 0x90, 0x98};
static const Bit32u MODE_0_DRY_AMP[] = {0xA0, 0xA0, 0xA0, 0xA0, 0xB0, 0xB0, 0xB0, 0xD0};
static const Bit32u MODE_0_WET_AMP[] = {0x10, 0x30, 0x50, 0x70, 0x90, 0xC0, 0xF0, 0xF0};
static const Bit32u MODE_0_LPF_AMP = 0x60;

static const Bit32u MODE_1_NUMBER_OF_ALLPASSES = 3;
static const Bit32u MODE_1_ALLPASSES[] = {1324, 809, 176};
static const Bit32u MODE_1_NUMBER_OF_COMBS = 4; // Same as for mode 0 above
static const Bit32u MODE_1_COMBS[] = {961 + PROCESS_DELAY, 2619, 3545, 4519};
static const Bit32u MODE_1_OUTL[] = {2618, 1760, 4518};
static const Bit32u MODE_1_OUTR[] = {1300, 3532, 2274};
static const Bit32u MODE_1_COMB_FACTOR[] = {0x80, 0x60, 0x60, 0x60};
static const Bit32u MODE_1_COMB_FEEDBACK[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
											  0x28, 0x48, 0x60, 0x70, 0x78, 0x80, 0x90, 0x98,
											  0x28, 0x48, 0x60, 0x78, 0x80, 0x88, 0x90, 0x98,
											  0x28, 0x48, 0x60, 0x78, 0x80, 0x88, 0x90, 0x98};
static const Bit32u MODE_1_DRY_AMP[] = {0xA0, 0xA0, 0xB0, 0xB0, 0xB0, 0xB0, 0xB0, 0xE0};
static const Bit32u MODE_1_WET_AMP[] = {0x10, 0x30, 0x50, 0x70, 0x90, 0xC0, 0xF0, 0xF0};
static const Bit32u MODE_1_LPF_AMP = 0x60;

static const Bit32u MODE_2_NUMBER_OF_ALLPASSES = 3;
static const Bit32u MODE_2_ALLPASSES[] = {969, 644, 157};
static const Bit32u MODE_2_NUMBER_OF_COMBS = 4; // Same as for mode 0 above
static const Bit32u MODE_2_COMBS[] = {116 + PROCESS_DELAY, 2259, 2839, 3539};
static const Bit32u MODE_2_OUTL[] = {2259, 718, 1769};
static const Bit32u MODE_2_OUTR[] = {1136, 2128, 1};
static const Bit32u MODE_2_COMB_FACTOR[] = {0, 0x20, 0x20, 0x20};
static const Bit32u MODE_2_COMB_FEEDBACK[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                              0x30, 0x58, 0x78, 0x88, 0xA0, 0xB8, 0xC0, 0xD0,
                                              0x30, 0x58, 0x78, 0x88, 0xA0, 0xB8, 0xC0, 0xD0,
                                              0x30, 0x58, 0x78, 0x88, 0xA0, 0xB8, 0xC0, 0xD0};
static const Bit32u MODE_2_DRY_AMP[] = {0xA0, 0xA0, 0xB0, 0xB0, 0xB0, 0xB0, 0xC0, 0xE0};
static const Bit32u MODE_2_WET_AMP[] = {0x10, 0x30, 0x50, 0x70, 0x90, 0xC0, 0xF0, 0xF0};
static const Bit32u MODE_2_LPF_AMP = 0x80;

static const Bit32u MODE_3_NUMBER_OF_ALLPASSES = 0;
static const Bit32u MODE_3_NUMBER_OF_COMBS = 1;
static const Bit32u MODE_3_DELAY[] = {16000 + MODE_3_FEEDBACK_DELAY + PROCESS_DELAY + MODE_3_ADDITIONAL_DELAY};
static const Bit32u MODE_3_OUTL[] = {400, 624, 960, 1488, 2256, 3472, 5280, 8000};
static const Bit32u MODE_3_OUTR[] = {800, 1248, 1920, 2976, 4512, 6944, 10560, 16000};
static const Bit32u MODE_3_COMB_FACTOR[] = {0x68};
static const Bit32u MODE_3_COMB_FEEDBACK[] = {0x68, 0x60};
static const Bit32u MODE_3_DRY_AMP[] = {0x20, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50};
static const Bit32u MODE_3_WET_AMP[] = {0x18, 0x18, 0x28, 0x40, 0x60, 0x80, 0xA8, 0xF8};

static const BReverbSettings REVERB_MODE_0_SETTINGS = {MODE_0_NUMBER_OF_ALLPASSES, MODE_0_ALLPASSES, MODE_0_NUMBER_OF_COMBS, MODE_0_COMBS, MODE_0_OUTL, MODE_0_OUTR, MODE_0_COMB_FACTOR, MODE_0_COMB_FEEDBACK, MODE_0_DRY_AMP, MODE_0_WET_AMP, MODE_0_LPF_AMP};
static const BReverbSettings REVERB_MODE_1_SETTINGS = {MODE_1_NUMBER_OF_ALLPASSES, MODE_1_ALLPASSES, MODE_1_NUMBER_OF_COMBS, MODE_1_COMBS, MODE_1_OUTL, MODE_1_OUTR, MODE_1_COMB_FACTOR, MODE_1_COMB_FEEDBACK, MODE_1_DRY_AMP, MODE_1_WET_AMP, MODE_1_LPF_AMP};
static const BReverbSettings REVERB_MODE_2_SETTINGS = {MODE_2_NUMBER_OF_ALLPASSES, MODE_2_ALLPASSES, MODE_2_NUMBER_OF_COMBS, MODE_2_COMBS, MODE_2_OUTL, MODE_2_OUTR, MODE_2_COMB_FACTOR, MODE_2_COMB_FEEDBACK, MODE_2_DRY_AMP, MODE_2_WET_AMP, MODE_2_LPF_AMP};
static const BReverbSettings REVERB_MODE_3_SETTINGS = {MODE_3_NUMBER_OF_ALLPASSES, NULL, MODE_3_NUMBER_OF_COMBS, MODE_3_DELAY, MODE_3_OUTL, MODE_3_OUTR, MODE_3_COMB_FACTOR, MODE_3_COMB_FEEDBACK, MODE_3_DRY_AMP, MODE_3_WET_AMP, 0};

static const BReverbSettings * const REVERB_SETTINGS[] = {&REVERB_MODE_0_SETTINGS, &REVERB_MODE_1_SETTINGS, &REVERB_MODE_2_SETTINGS, &REVERB_MODE_3_SETTINGS};

// This algorithm tries to emulate exactly Boss multiplication operation (at least this is what we see on reverb RAM data lines).
// Also LA32 is suspected to use the similar one to perform PCM interpolation and ring modulation.
static Bit32s weirdMul(Bit32s a, Bit8u addMask, Bit8u carryMask) {
	Bit8u mask = 0x80;
	Bit32s res = 0;
	for (int i = 0; i < 8; i++) {
		Bit32s carry = (a < 0) && (mask & carryMask) > 0 ? a & 1 : 0;
		a >>= 1;
		res += (mask & addMask) > 0 ? a + carry : 0;
		mask >>= 1;
	}
	return res;
}

RingBuffer::RingBuffer(Bit32u newsize) : size(newsize), index(0) {
	buffer = new Bit16s[size];
}

RingBuffer::~RingBuffer() {
	delete[] buffer;
	buffer = NULL;
}

Bit32s RingBuffer::next() {
	if (++index >= size) {
		index = 0;
	}
	return buffer[index];
}

bool RingBuffer::isEmpty() const {
	if (buffer == NULL) return true;

	Bit16s *buf = buffer;
	for (Bit32u i = 0; i < size; i++) {
		if (*buf < -8 || *buf > 8) return false;
		buf++;
	}
	return true;
}

void RingBuffer::mute() {
	Bit16s *buf = buffer;
	for (Bit32u i = 0; i < size; i++) {
		*buf++ = 0;
	}
}

AllpassFilter::AllpassFilter(const Bit32u useSize) : RingBuffer(useSize) {}

Bit32s AllpassFilter::process(const Bit32s in) {
	// This model corresponds to the allpass filter implementation of the real CM-32L device
	// found from sample analysis

	Bit16s bufferOut = next();

	// store input - feedback / 2
	buffer[index] = in - (bufferOut >> 1);

	// return buffer output + feedforward / 2
	return bufferOut + (buffer[index] >> 1);
}

CombFilter::CombFilter(const Bit32u useSize, const Bit32u useFilterFactor) : RingBuffer(useSize), filterFactor(useFilterFactor) {}

void CombFilter::process(const Bit32s in) {
	// This model corresponds to the comb filter implementation of the real CM-32L device

	// the previously stored value
	Bit32s last = buffer[index];

	// prepare input + feedback
	Bit32s filterIn = in + weirdMul(next(), feedbackFactor, 0xF0 /* Maybe 0x80 ? */);

	// store input + feedback processed by a low-pass filter
	buffer[index] = weirdMul(last, filterFactor, 0x40) - filterIn;
}

Bit32s CombFilter::getOutputAt(const Bit32u outIndex) const {
	return buffer[(size + index - outIndex) % size];
}

void CombFilter::setFeedbackFactor(const Bit32u useFeedbackFactor) {
	feedbackFactor = useFeedbackFactor;
}

DelayWithLowPassFilter::DelayWithLowPassFilter(const Bit32u useSize, const Bit32u useFilterFactor, const Bit32u useAmp)
	: CombFilter(useSize, useFilterFactor), amp(useAmp) {}

void DelayWithLowPassFilter::process(const Bit32s in) {
	// the previously stored value
	Bit32s last = buffer[index];

	// move to the next index
	next();

	// low-pass filter process
	Bit32s lpfOut = weirdMul(last, filterFactor, 0xFF) + in;

	// store lpfOut multiplied by LPF amp factor
	buffer[index] = weirdMul(lpfOut, amp, 0xFF);
}

TapDelayCombFilter::TapDelayCombFilter(const Bit32u useSize, const Bit32u useFilterFactor) : CombFilter(useSize, useFilterFactor) {}

void TapDelayCombFilter::process(const Bit32s in) {
	// the previously stored value
	Bit32s last = buffer[index];

	// move to the next index
	next();

	// prepare input + feedback
	// Actually, the size of the filter varies with the TIME parameter, the feedback sample is taken from the position just below the right output
	Bit32s filterIn = in + weirdMul(getOutputAt(outR + MODE_3_FEEDBACK_DELAY), feedbackFactor, 0xF0);

	// store input + feedback processed by a low-pass filter
	buffer[index] = weirdMul(last, filterFactor, 0xF0) - filterIn;
}

Bit32s TapDelayCombFilter::getLeftOutput() const {
	return getOutputAt(outL + PROCESS_DELAY + MODE_3_ADDITIONAL_DELAY);
}

Bit32s TapDelayCombFilter::getRightOutput() const {
	return getOutputAt(outR + PROCESS_DELAY + MODE_3_ADDITIONAL_DELAY);
}

void TapDelayCombFilter::setOutputPositions(const Bit32u useOutL, const Bit32u useOutR) {
	outL = useOutL;
	outR = useOutR;
}

BReverbModel::BReverbModel(const ReverbMode mode)
	: allpasses(NULL), combs(NULL), currentSettings(*REVERB_SETTINGS[mode]), tapDelayMode(mode == REVERB_MODE_TAP_DELAY) {}

BReverbModel::~BReverbModel() {
	close();
}

void BReverbModel::open() {
	if (currentSettings.numberOfAllpasses > 0) {
		allpasses = new AllpassFilter*[currentSettings.numberOfAllpasses];
		for (Bit32u i = 0; i < currentSettings.numberOfAllpasses; i++) {
			allpasses[i] = new AllpassFilter(currentSettings.allpassSizes[i]);
		}
	}
	combs = new CombFilter*[currentSettings.numberOfCombs];
	if (tapDelayMode) {
		*combs = new TapDelayCombFilter(*currentSettings.combSizes, *currentSettings.filterFactors);
	} else {
		combs[0] = new DelayWithLowPassFilter(currentSettings.combSizes[0], currentSettings.filterFactors[0], currentSettings.lpfAmp);
		for (Bit32u i = 1; i < currentSettings.numberOfCombs; i++) {
			combs[i] = new CombFilter(currentSettings.combSizes[i], currentSettings.filterFactors[i]);
		}
	}
	mute();
}

void BReverbModel::close() {
	if (allpasses != NULL) {
		for (Bit32u i = 0; i < currentSettings.numberOfAllpasses; i++) {
			if (allpasses[i] != NULL) {
				delete allpasses[i];
				allpasses[i] = NULL;
			}
		}
		delete[] allpasses;
		allpasses = NULL;
	}
	if (combs != NULL) {
		for (Bit32u i = 0; i < currentSettings.numberOfCombs; i++) {
			if (combs[i] != NULL) {
				delete combs[i];
				combs[i] = NULL;
			}
		}
		delete[] combs;
		combs = NULL;
	}
}

void BReverbModel::mute() {
	if (allpasses != NULL) {
		for (Bit32u i = 0; i < currentSettings.numberOfAllpasses; i++) {
			allpasses[i]->mute();
		}
	}
	if (combs != NULL) {
		for (Bit32u i = 0; i < currentSettings.numberOfCombs; i++) {
			combs[i]->mute();
		}
	}
}

void BReverbModel::setParameters(Bit8u time, Bit8u level) {
	if (combs == NULL) return;
	level &= 7;
	time &= 7;
	if (tapDelayMode) {
		TapDelayCombFilter *comb = static_cast<TapDelayCombFilter *> (*combs);
		comb->setOutputPositions(currentSettings.outLPositions[time], currentSettings.outRPositions[time & 7]);
		comb->setFeedbackFactor(currentSettings.feedbackFactors[((level < 3) || (time < 6)) ? 0 : 1]);
	} else {
		for (Bit32u i = 0; i < currentSettings.numberOfCombs; i++) {
			combs[i]->setFeedbackFactor(currentSettings.feedbackFactors[(i << 3) + time]);
		}
	}
	if (time == 0 && level == 0) {
		dryAmp = wetLevel = 0;
	} else {
		dryAmp = currentSettings.dryAmps[level];
		wetLevel = currentSettings.wetLevels[level];
	}
}

bool BReverbModel::isActive() const {
	for (Bit32u i = 0; i < currentSettings.numberOfAllpasses; i++) {
		if (!allpasses[i]->isEmpty()) return true;
	}
	for (Bit32u i = 0; i < currentSettings.numberOfCombs; i++) {
		if (!combs[i]->isEmpty()) return true;
	}
	return false;
}

void BReverbModel::process(const float *inLeft, const float *inRight, float *outLeft, float *outRight, unsigned long numSamples) {
	Bit32s dry, link, outL1, outR1;

	for (unsigned long i = 0; i < numSamples; i++) {
		if (tapDelayMode) {
			dry = Bit32s(*inLeft * 8192.0f) + Bit32s(*inRight * 8192.0f);
		} else {
			dry = Bit32s(*inLeft * 8192.0f) / 2 + Bit32s(*inRight * 8192.0f) / 2;
		}

		// Looks like dryAmp doesn't change in MT-32 but it does in CM-32L / LAPC-I
		dry = weirdMul(dry, dryAmp, 0xFF);

		if (tapDelayMode) {
			TapDelayCombFilter *comb = static_cast<TapDelayCombFilter *> (*combs);
			comb->process(dry);
			*outLeft = weirdMul(comb->getLeftOutput(), wetLevel, 0xFF) / 8192.0f;
			*outRight = weirdMul(comb->getRightOutput(), wetLevel, 0xFF) / 8192.0f;
		} else {
			// Get the last stored sample before processing in order not to loose it
			link = combs[0]->getOutputAt(currentSettings.combSizes[0] - 1);

			// Entrance LPF. Note, comb.process() differs a bit here.
			combs[0]->process(dry);

			// This introduces reverb noise which actually makes output from the real Boss chip nondeterministic
			link = link - 1;
			link = allpasses[0]->process(link);
			link = allpasses[1]->process(link);
			link = allpasses[2]->process(link);

			// If the output position is equal to the comb size, get it now in order not to loose it
			outL1 = combs[1]->getOutputAt(currentSettings.outLPositions[0] - 1);
			outL1 += outL1 >> 1;

			combs[1]->process(link);
			combs[2]->process(link);
			combs[3]->process(link);

			link = combs[2]->getOutputAt(currentSettings.outLPositions[1]);
			link += link >> 1;
			link += outL1;
			link += combs[3]->getOutputAt(currentSettings.outLPositions[2]);
			*outLeft = weirdMul(link, wetLevel, 0xFF) / 8192.0f;

			outR1 = combs[1]->getOutputAt(currentSettings.outRPositions[0]);
			outR1 += outR1 >> 1;
			link = combs[2]->getOutputAt(currentSettings.outRPositions[1]);
			link += link >> 1;
			link += outR1;
			link += combs[3]->getOutputAt(currentSettings.outRPositions[2]);
			*outRight = weirdMul(link, wetLevel, 0xFF) / 8192.0f;
		}

		inLeft++;
		inRight++;
		outLeft++;
		outRight++;
	}
}

}

#endif
