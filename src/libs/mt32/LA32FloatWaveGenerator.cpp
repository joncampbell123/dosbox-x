/* Copyright (C) 2003, 2004, 2005, 2006, 2008, 2009 Dean Beeler, Jerome Fisher
 * Copyright (C) 2011-2026 Dean Beeler, Jerome Fisher, Sergey V. Mikayev
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

#include <cstddef>

#include "internals.h"

#include "LA32FloatWaveGenerator.h"

#include "LA32FloatMath.h"
#include "mmath.h"
#include "Tables.h"

namespace MT32Emu {

static const float RESONANCE_DECAY_THRESHOLD_CUTOFF_VALUE = 144.0f;

static const Bit8u *resAmpDecayFactors;

#ifndef MT32EMU_FASTER_MATH
#define MT32EMU_FASTER_MATH 1
#endif

#if MT32EMU_FASTER_MATH == 0

typedef LA32FloatMath::VanillaMath LA32FloatMathImpl;

#elif MT32EMU_FASTER_MATH == 1

// In this mode we strive to minimise the number of operations on the critical path at the expense of increased memory usage,
// hence it may well not be the fastest option under memory pressure. The LUT sizes are picked so to achieve a good accuracy with
// the SNR about 126dB. Albeit, we don't need the exponent table that big with the linear interpolation: 10 bits would be enough.
// We pick the table size this big to simplify fexp2i a lot, leading to a considerable speedup.
typedef LA32FloatMath::FasterMath<LA32FloatMath::LinearInterpolator, 12, 12> LA32FloatMathImpl;

#elif MT32EMU_FASTER_MATH == 2

// The quadratic interpolation allows to reduce the memory needed for the LUTs significantly, but also slightly increases the code
// complexity. The same level of accuracy is expected as with the option above.
typedef LA32FloatMath::FasterMath<LA32FloatMath::QuadraticInterpolator, 6, 9> LA32FloatMathImpl;

#else

#error "Wrong MT32EMU_FASTER_MATH"

#endif

void LA32FloatWaveGenerator::updateWaveFormParameters() {
	cosineLen = 0.5f * waveLen;
	if (cutoff > MIDDLE_CUTOFF_VALUE) {
		cosineLen *= LA32FloatMathImpl::cosineLengthFactor(cutoff);
	}
}

float LA32FloatWaveGenerator::getPCMSample(unsigned int position) {
	if (position >= pcmWaveLength) {
		if (!pcmWaveLooped) {
			return 0;
		}
		position = position % pcmWaveLength;
	}
	Bit16s pcmSample = pcmWaveAddress[position];
	float sampleValue = LA32FloatMathImpl::unlogPCMSample(pcmSample & 32767);
	return ((pcmSample & 32768) == 0) ? sampleValue : -sampleValue;
}

void LA32FloatWaveGenerator::initSynth(const bool useSawtoothWaveform, const Bit8u usePulseWidth, const Bit8u useResonance) {
	sawtoothWaveform = useSawtoothWaveform;
	pulseWidth = usePulseWidth;
	resonance = useResonance;

	resAmpBase = LA32FloatMathImpl::resonanceAmpBase(resonance);
	resAmp = resAmpBase;

	pulseLenFactor = (pulseWidth > 128) ? LA32FloatMathImpl::pulseLengthFactor(pulseWidth) : 0.5f;

	cutoff = 0.0f;
	freq = 0.0f;
	waveLen = 0.0f;
	wavePos = 0.0f;

	pcmWaveAddress = NULL;
	active = true;
}

void LA32FloatWaveGenerator::initPCM(const Bit16s * const usePCMWaveAddress, const Bit32u usePCMWaveLength, const bool usePCMWaveLooped, const bool usePCMWaveInterpolated) {
	pcmWaveAddress = usePCMWaveAddress;
	pcmWaveLength = usePCMWaveLength;
	pcmWaveLooped = usePCMWaveLooped;
	pcmWaveInterpolated = usePCMWaveInterpolated;

	pcmPosition = 0.0f;
	active = true;
}

void LA32FloatWaveGenerator::setAmp(const Bit32u amp) {
	ampFactor = LA32FloatMathImpl::linearAmpFactor(amp);
}

void LA32FloatWaveGenerator::setPitch(const Bit16u pitch) {
	float lastFreq = freq;
	freq = LA32FloatMathImpl::frequency(pitch);

	if (isPCMWave()) {
		pcmPositionDelta = freq * 2048.0f / SAMPLE_RATE;
	} else {
		waveLen = SAMPLE_RATE / freq;
		wavePos *= lastFreq / freq;
		updateWaveFormParameters();
	}
}

void LA32FloatWaveGenerator::setCutoff(const Bit32u cutoffVal) {
	cutoff = cutoffVal / 262144.0f;

	// Correct resAmp for cutoff in range 50..66
	if ((cutoff >= MIDDLE_CUTOFF_VALUE) && (cutoff < RESONANCE_DECAY_THRESHOLD_CUTOFF_VALUE)) {
		resAmp = resAmpBase * LA32FloatMathImpl::fsin((cutoff - MIDDLE_CUTOFF_VALUE) / 32.0f);
	} else {
		resAmp = resAmpBase;
	}
	lowCutoffAttenuationFactor = cutoff < MIDDLE_CUTOFF_VALUE ? LA32FloatMathImpl::attenuationFactorBelowMiddleCutoff(cutoff) : 1;
	updateWaveFormParameters();
}

float LA32FloatWaveGenerator::generateNextSample() {
	if (!active) {
		return 0.0f;
	}

	float sample = 0.0f;

	if (isPCMWave()) {
		// Render PCM waveform
		int len = pcmWaveLength;
		int intPCMPosition = int(pcmPosition);
		if (intPCMPosition >= len && !pcmWaveLooped) {
			// We're now past the end of a non-looping PCM waveform so it's time to die.
			deactivate();
			return 0.0f;
		}

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

		float newPCMPosition = pcmPosition + pcmPositionDelta;
		if (pcmWaveLooped) {
			newPCMPosition = fmod(newPCMPosition, float(pcmWaveLength));
		}
		pcmPosition = newPCMPosition;
	} else {
		// Render synthesised waveform

		// Start playing in center of first cosine segment
		// relWavePos is shifted by a half of cosineLen
		float relWavePos = wavePos + 0.5f * cosineLen;
		if (relWavePos > waveLen) {
			relWavePos -= waveLen;
		}

		float pulseLen = pulseLenFactor * waveLen;
		float hLen = pulseLen - cosineLen;

		// Ignore pulsewidths too high for given freq
		if (hLen < 0.0f) {
			hLen = 0.0f;
		}

		// Produce filtered square wave with 2 cosine waves on slopes
		float squareWaveSample;

		// 1st cosine segment
		if (relWavePos < cosineLen) {
			squareWaveSample = -LA32FloatMathImpl::fcos(relWavePos / cosineLen);
		} else

		// high linear segment
		if (relWavePos < (cosineLen + hLen)) {
			squareWaveSample = 1.f;
		} else

		// 2nd cosine segment
		if (relWavePos < (2 * cosineLen + hLen)) {
			squareWaveSample = LA32FloatMathImpl::fcos((relWavePos - (cosineLen + hLen)) / cosineLen);
		} else {

		// low linear segment
			squareWaveSample = -1.f;
		}

		sample = squareWaveSample;

		if (cutoff < MIDDLE_CUTOFF_VALUE) {
			sample *= lowCutoffAttenuationFactor;
		} else {

			// Add resonance sine. Effective for cutoff > 50 only
			float resSample = 1.0f;

			// Resonance decay speed factor
			float resAmpDecayFactor = resAmpDecayFactors[resonance >> 2];

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
			resSample *= LA32FloatMathImpl::fsin(relWavePos / cosineLen);

			// Resonance sine amp
			float resAmpFadeLog2 = -0.125f * resAmpDecayFactor * (relWavePos / cosineLen); // seems to be exact
			float resAmpFade = LA32FloatMathImpl::unlogResonanceAmpFade(resAmpFadeLog2);

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

			// To ensure the output wave has no breaks, two different windows are applied to the beginning and the ending of the resonance sine segment
			if (relWavePos < 0.5f * cosineLen) {
				if (relWavePos < 0.0f) {
					// The window is synchronous square sine here
					resAmpFade *= squareWaveSample * squareWaveSample;
				} else {
					// The window is synchronous sine here
					resAmpFade *= squareWaveSample < 0 ? -squareWaveSample : squareWaveSample;
				}
			}

			sample += resSample * resAmp * resAmpFade;
		}

		// sawtooth waves
		if (sawtoothWaveform) {
			sample *= LA32FloatMathImpl::fcos(2 * wavePos / waveLen);
		}

		wavePos++;

		// wavePos isn't supposed to be > waveLen
		if (wavePos > waveLen) {
			wavePos -= waveLen;
		}
	}

	// Multiply sample with current TVA value
	sample *= ampFactor;
	return sample;
}

void LA32FloatWaveGenerator::deactivate() {
	active = false;
}

bool LA32FloatWaveGenerator::isActive() const {
	return active;
}

bool LA32FloatWaveGenerator::isPCMWave() const {
	return pcmWaveAddress != NULL;
}

void LA32FloatPartialPair::initTables(const Tables &tables) {
	resAmpDecayFactors = tables.resAmpDecayFactors;
	LA32FloatMathImpl::init();
}

void LA32FloatPartialPair::init(const bool useRingModulated, const bool useMixed) {
	ringModulated = useRingModulated;
	mixed = useMixed;
	masterOutputSample = 0.0f;
	slaveOutputSample = 0.0f;
}

void LA32FloatPartialPair::initSynth(const PairType useMaster, const bool sawtoothWaveform, const Bit8u pulseWidth, const Bit8u resonance) {
	if (useMaster == MASTER) {
		master.initSynth(sawtoothWaveform, pulseWidth, resonance);
	} else {
		slave.initSynth(sawtoothWaveform, pulseWidth, resonance);
	}
}

void LA32FloatPartialPair::initPCM(const PairType useMaster, const Bit16s *pcmWaveAddress, const Bit32u pcmWaveLength, const bool pcmWaveLooped) {
	if (useMaster == MASTER) {
		master.initPCM(pcmWaveAddress, pcmWaveLength, pcmWaveLooped, true);
	} else {
		slave.initPCM(pcmWaveAddress, pcmWaveLength, pcmWaveLooped, !ringModulated);
	}
}

void LA32FloatPartialPair::setAmp(const PairType useMaster, const Bit32u amp) {
	if (useMaster == MASTER) {
		master.setAmp(amp);
	} else {
		slave.setAmp(amp);
	}
}

void LA32FloatPartialPair::setPitch(const PairType useMaster, const Bit16u pitch) {
	if (useMaster == MASTER) {
		master.setPitch(pitch);
	} else {
		slave.setPitch(pitch);
	}
}

void LA32FloatPartialPair::setCutoff(const PairType useMaster, const Bit32u cutoff) {
	if (useMaster == MASTER) {
		master.setCutoff(cutoff);
	} else {
		slave.setCutoff(cutoff);
	}
}

void LA32FloatPartialPair::generateNextSample(const PairType useMaster) {
	if (useMaster == MASTER) {
		masterOutputSample = master.generateNextSample();
	} else {
		slaveOutputSample = slave.generateNextSample();
	}
}

static inline float produceDistortedSample(float sample) {
	if (sample < -1.0f) {
		return sample + 2.0f;
	} else if (1.0f < sample) {
		return sample - 2.0f;
	}
	return sample;
}

float LA32FloatPartialPair::nextOutSample() {
	// Note, LA32FloatWaveGenerator produces each sample normalised in terms of a single playing partial,
	// so the unity sample corresponds to the internal LA32 logarithmic fixed-point unity sample.
	// However, each logarithmic sample is then unlogged to a 14-bit signed integer value, i.e. the max absolute value is 8192.
	// Thus, considering that samples are further mapped to a 16-bit signed integer,
	// we apply a conversion factor 0.25 to produce properly normalised float samples.
	if (!ringModulated) {
		return 0.25f * (masterOutputSample + slaveOutputSample);
	}
	/*
	 * SEMI-CONFIRMED: Ring modulation model derived from sample analysis of specially constructed patches which exploit distortion.
	 * LA32 ring modulator found to produce distorted output in case if the absolute value of maximal amplitude of one of the input partials exceeds 8191.
	 * This is easy to reproduce using synth partials with resonance values close to the maximum. It looks like an integer overflow happens in this case.
	 * As the distortion is strictly bound to the amplitude of the complete mixed square + resonance wave in the linear space,
	 * it is reasonable to assume the ring modulation is performed also in the linear space by sample multiplication.
	 * Most probably the overflow is caused by limited precision of the multiplication circuit as the very similar distortion occurs with panning.
	 */
	float ringModulatedSample = produceDistortedSample(masterOutputSample) * produceDistortedSample(slaveOutputSample);
	return 0.25f * (mixed ? masterOutputSample + ringModulatedSample : ringModulatedSample);
}

void LA32FloatPartialPair::deactivate(const PairType useMaster) {
	if (useMaster == MASTER) {
		master.deactivate();
		masterOutputSample = 0.0f;
	} else {
		slave.deactivate();
		slaveOutputSample = 0.0f;
	}
}

bool LA32FloatPartialPair::isActive(const PairType useMaster) const {
	return useMaster == MASTER ? master.isActive() : slave.isActive();
}

} // namespace MT32Emu
