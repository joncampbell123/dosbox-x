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

#include <cmath>
#include "mt32emu.h"
#include "mmath.h"
#include "LegacyWaveGenerator.h"

#if MT32EMU_ACCURATE_WG == 1

namespace MT32Emu {

static const float MIDDLE_CUTOFF_VALUE = 128.0f;
static const float RESONANCE_DECAY_THRESHOLD_CUTOFF_VALUE = 144.0f;
static const float MAX_CUTOFF_VALUE = 240.0f;

float LA32WaveGenerator::getPCMSample(unsigned int position) {
	if (position >= pcmWaveLength) {
		if (!pcmWaveLooped) {
			return 0;
		}
		position = position % pcmWaveLength;
	}
	Bit16s pcmSample = pcmWaveAddress[position];
	float sampleValue = EXP2F(((pcmSample & 32767) - 32787.0f) / 2048.0f);
	return ((pcmSample & 32768) == 0) ? sampleValue : -sampleValue;
}

void LA32WaveGenerator::initSynth(const bool sawtoothWaveform, const Bit8u pulseWidth, const Bit8u resonance) {
	this->sawtoothWaveform = sawtoothWaveform;
	this->pulseWidth = pulseWidth;
	this->resonance = resonance;

	wavePos = 0.0f;
	lastFreq = 0.0f;

	pcmWaveAddress = NULL;
	active = true;
}

void LA32WaveGenerator::initPCM(const Bit16s * const pcmWaveAddress, const Bit32u pcmWaveLength, const bool pcmWaveLooped, const bool pcmWaveInterpolated) {
	this->pcmWaveAddress = pcmWaveAddress;
	this->pcmWaveLength = pcmWaveLength;
	this->pcmWaveLooped = pcmWaveLooped;
	this->pcmWaveInterpolated = pcmWaveInterpolated;

	pcmPosition = 0.0f;
	active = true;
}

float LA32WaveGenerator::generateNextSample(const Bit32u ampVal, const Bit16u pitch, const Bit32u cutoffRampVal) {
	if (!active) {
		return 0.0f;
	}

	this->amp = amp;
	this->pitch = pitch;

	float sample = 0.0f;

	// SEMI-CONFIRMED: From sample analysis:
	// (1) Tested with a single partial playing PCM wave 77 with pitchCoarse 36 and no keyfollow, velocity follow, etc.
	// This gives results within +/- 2 at the output (before any DAC bitshifting)
	// when sustaining at levels 156 - 255 with no modifiers.
	// (2) Tested with a special square wave partial (internal capture ID tva5) at TVA envelope levels 155-255.
	// This gives deltas between -1 and 0 compared to the real output. Note that this special partial only produces
	// positive amps, so negative still needs to be explored, as well as lower levels.
	//
	// Also still partially unconfirmed is the behaviour when ramping between levels, as well as the timing.

	float amp = EXP2F(ampVal / -1024.0f / 4096.0f);
	float freq = EXP2F(pitch / 4096.0f - 16.0f) * SAMPLE_RATE;

	if (isPCMWave()) {
		// Render PCM waveform
		int len = pcmWaveLength;
		int intPCMPosition = (int)pcmPosition;
		if (intPCMPosition >= len && !pcmWaveLooped) {
			// We're now past the end of a non-looping PCM waveform so it's time to die.
			deactivate();
			return 0.0f;
		}
		float positionDelta = freq * 2048.0f / SAMPLE_RATE;

		// Linear interpolation
		float firstSample = getPCMSample(intPCMPosition);
		// We observe that for partial structures with ring modulation the interpolation is not applied to the slave PCM partial.
		// It's assumed that the multiplication circuitry intended to perform the interpolation on the slave PCM partial
		// is borrowed by the ring modulation circuit (or the LA32 chip has a similar lack of resources assigned to each partial pair).
		if (pcmWaveInterpolated) {
			sample = firstSample + (getPCMSample(intPCMPosition + 1) - firstSample) * (pcmPosition - intPCMPosition);
		} else {
			sample = firstSample;
		}

		float newPCMPosition = pcmPosition + positionDelta;
		if (pcmWaveLooped) {
			newPCMPosition = fmod(newPCMPosition, (float)pcmWaveLength);
		}
		pcmPosition = newPCMPosition;
	} else {
		// Render synthesised waveform
		wavePos *= lastFreq / freq;
		lastFreq = freq;

		float resAmp = EXP2F(1.0f - (32 - resonance) / 4.0f);
		{
			//static const float resAmpFactor = EXP2F(-7);
			//resAmp = EXP2I(resonance << 10) * resAmpFactor;
		}

		// The cutoffModifier may not be supposed to be directly added to the cutoff -
		// it may for example need to be multiplied in some way.
		// The 240 cutoffVal limit was determined via sample analysis (internal Munt capture IDs: glop3, glop4).
		// More research is needed to be sure that this is correct, however.
		float cutoffVal = cutoffRampVal / 262144.0f;
		if (cutoffVal > MAX_CUTOFF_VALUE) {
			cutoffVal = MAX_CUTOFF_VALUE;
		}

		// Wave length in samples
		float waveLen = SAMPLE_RATE / freq;

		// Init cosineLen
		float cosineLen = 0.5f * waveLen;
		if (cutoffVal > MIDDLE_CUTOFF_VALUE) {
			cosineLen *= EXP2F((cutoffVal - MIDDLE_CUTOFF_VALUE) / -16.0f); // found from sample analysis
		}

		// Start playing in center of first cosine segment
		// relWavePos is shifted by a half of cosineLen
		float relWavePos = wavePos + 0.5f * cosineLen;
		if (relWavePos > waveLen) {
			relWavePos -= waveLen;
		}

		// Ratio of positive segment to wave length
		float pulseLen = 0.5f;
		if (pulseWidth > 128) {
			pulseLen = EXP2F((64 - pulseWidth) / 64.0f);
			//static const float pulseLenFactor = EXP2F(-192 / 64);
			//pulseLen = EXP2I((256 - pulseWidthVal) << 6) * pulseLenFactor;
		}
		pulseLen *= waveLen;

		float hLen = pulseLen - cosineLen;

		// Ignore pulsewidths too high for given freq
		if (hLen < 0.0f) {
			hLen = 0.0f;
		}

		// Ignore pulsewidths too high for given freq and cutoff
		float lLen = waveLen - hLen - 2 * cosineLen;
		if (lLen < 0.0f) {
			lLen = 0.0f;
		}

		// Correct resAmp for cutoff in range 50..66
		if ((cutoffVal >= 128.0f) && (cutoffVal < 144.0f)) {
			resAmp *= sin(FLOAT_PI * (cutoffVal - 128.0f) / 32.0f);
		}

		// Produce filtered square wave with 2 cosine waves on slopes

		// 1st cosine segment
		if (relWavePos < cosineLen) {
			sample = -cos(FLOAT_PI * relWavePos / cosineLen);
		} else

		// high linear segment
		if (relWavePos < (cosineLen + hLen)) {
			sample = 1.f;
		} else

		// 2nd cosine segment
		if (relWavePos < (2 * cosineLen + hLen)) {
			sample = cos(FLOAT_PI * (relWavePos - (cosineLen + hLen)) / cosineLen);
		} else {

		// low linear segment
			sample = -1.f;
		}

		if (cutoffVal < 128.0f) {

			// Attenuate samples below cutoff 50
			// Found by sample analysis
			sample *= EXP2F(-0.125f * (128.0f - cutoffVal));
		} else {

			// Add resonance sine. Effective for cutoff > 50 only
			float resSample = 1.0f;

			// Resonance decay speed factor
			float resAmpDecayFactor = Tables::getInstance().resAmpDecayFactor[resonance >> 2];

			// Now relWavePos counts from the middle of first cosine
			relWavePos = wavePos;

			// negative segments
			if (!(relWavePos < (cosineLen + hLen))) {
				resSample = -resSample;
				relWavePos -= cosineLen + hLen;

				// From the digital captures, the decaying speed of the resonance sine is found a bit different for the positive and the negative segments
				resAmpDecayFactor += 0.25f;
			}

			// Resonance sine WG
			resSample *= sin(FLOAT_PI * relWavePos / cosineLen);

			// Resonance sine amp
			float resAmpFadeLog2 = -0.125f * resAmpDecayFactor * (relWavePos / cosineLen); // seems to be exact
			float resAmpFade = EXP2F(resAmpFadeLog2);

			// Now relWavePos set negative to the left from center of any cosine
			relWavePos = wavePos;

			// negative segment
			if (!(wavePos < (waveLen - 0.5f * cosineLen))) {
				relWavePos -= waveLen;
			} else

			// positive segment
			if (!(wavePos < (hLen + 0.5f * cosineLen))) {
				relWavePos -= cosineLen + hLen;
			}

			// To ensure the output wave has no breaks, two different windows are appied to the beginning and the ending of the resonance sine segment
			if (relWavePos < 0.5f * cosineLen) {
				float syncSine = sin(FLOAT_PI * relWavePos / cosineLen);
				if (relWavePos < 0.0f) {
					// The window is synchronous square sine here
					resAmpFade *= syncSine * syncSine;
				} else {
					// The window is synchronous sine here
					resAmpFade *= syncSine;
				}
			}

			sample += resSample * resAmp * resAmpFade;
		}

		// sawtooth waves
		if (sawtoothWaveform) {
			sample *= cos(FLOAT_2PI * wavePos / waveLen);
		}

		wavePos++;

		// wavePos isn't supposed to be > waveLen
		if (wavePos > waveLen) {
			wavePos -= waveLen;
		}
	}

	// Multiply sample with current TVA value
	sample *= amp;
	return sample;
}

void LA32WaveGenerator::deactivate() {
	active = false;
}

bool LA32WaveGenerator::isActive() const {
	return active;
}

bool LA32WaveGenerator::isPCMWave() const {
	return pcmWaveAddress != NULL;
}

void LA32PartialPair::init(const bool ringModulated, const bool mixed) {
	this->ringModulated = ringModulated;
	this->mixed = mixed;
	masterOutputSample = 0.0f;
	slaveOutputSample = 0.0f;
}

void LA32PartialPair::initSynth(const PairType useMaster, const bool sawtoothWaveform, const Bit8u pulseWidth, const Bit8u resonance) {
	if (useMaster == MASTER) {
		master.initSynth(sawtoothWaveform, pulseWidth, resonance);
	} else {
		slave.initSynth(sawtoothWaveform, pulseWidth, resonance);
	}
}

void LA32PartialPair::initPCM(const PairType useMaster, const Bit16s *pcmWaveAddress, const Bit32u pcmWaveLength, const bool pcmWaveLooped) {
	if (useMaster == MASTER) {
		master.initPCM(pcmWaveAddress, pcmWaveLength, pcmWaveLooped, true);
	} else {
		slave.initPCM(pcmWaveAddress, pcmWaveLength, pcmWaveLooped, !ringModulated);
	}
}

void LA32PartialPair::generateNextSample(const PairType useMaster, const Bit32u amp, const Bit16u pitch, const Bit32u cutoff) {
	if (useMaster == MASTER) {
		masterOutputSample = master.generateNextSample(amp, pitch, cutoff);
	} else {
		slaveOutputSample = slave.generateNextSample(amp, pitch, cutoff);
	}
}

Bit16s LA32PartialPair::nextOutSample() {
	float outputSample;
	if (ringModulated) {
		float ringModulatedSample = masterOutputSample * slaveOutputSample;
		outputSample = mixed ? masterOutputSample + ringModulatedSample : ringModulatedSample;
	} else {
		outputSample = masterOutputSample + slaveOutputSample;
	}
	return Bit16s(outputSample * 8192.0f);
}

void LA32PartialPair::deactivate(const PairType useMaster) {
	if (useMaster == MASTER) {
		master.deactivate();
		masterOutputSample = 0.0f;
	} else {
		slave.deactivate();
		slaveOutputSample = 0.0f;
	}
}

bool LA32PartialPair::isActive(const PairType useMaster) const {
	return useMaster == MASTER ? master.isActive() : slave.isActive();
}

}

#endif // #if MT32EMU_ACCURATE_WG == 1
