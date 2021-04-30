/* Copyright (C) 2003, 2004, 2005, 2006, 2008, 2009 Dean Beeler, Jerome Fisher
 * Copyright (C) 2011-2021 Dean Beeler, Jerome Fisher, Sergey V. Mikayev
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

#ifndef MT32EMU_LA32_FLOAT_WAVE_GENERATOR_H
#define MT32EMU_LA32_FLOAT_WAVE_GENERATOR_H

#include "globals.h"
#include "internals.h"
#include "Types.h"
#include "LA32WaveGenerator.h"

namespace MT32Emu {

/**
 * LA32WaveGenerator is aimed to represent the exact model of LA32 wave generator.
 * The output square wave is created by adding high / low linear segments in-between
 * the rising and falling cosine segments. Basically, it's very similar to the phase distortion synthesis.
 * Behaviour of a true resonance filter is emulated by adding decaying sine wave.
 * The beginning and the ending of the resonant sine is multiplied by a cosine window.
 * To synthesise sawtooth waves, the resulting square wave is multiplied by synchronous cosine wave.
 */
class LA32FloatWaveGenerator {
	//***************************************************************************
	//  The local copy of partial parameters below
	//***************************************************************************

	bool active;

	// True means the resulting square wave is to be multiplied by the synchronous cosine
	bool sawtoothWaveform;

	// Values in range [1..31]
	// Value 1 correspong to the minimum resonance
	uint8_t resonance;

	// Processed value in range [0..255]
	// Values in range [0..128] have no effect and the resulting wave remains symmetrical
	// Value 255 corresponds to the maximum possible asymmetric of the resulting wave
	uint8_t pulseWidth;

	// Logarithmic PCM sample start address
	const int16_t *pcmWaveAddress;

	// Logarithmic PCM sample length
	uint32_t pcmWaveLength;

	// true for looped logarithmic PCM samples
	bool pcmWaveLooped;

	// false for slave PCM partials in the structures with the ring modulation
	bool pcmWaveInterpolated;

	//***************************************************************************
	// Internal variables below
	//***************************************************************************

	float wavePos;
	float lastFreq;
	float pcmPosition;

	float getPCMSample(unsigned int position);

public:
	// Initialise the WG engine for generation of synth partial samples and set up the invariant parameters
	void initSynth(const bool sawtoothWaveform, const uint8_t pulseWidth, const uint8_t resonance);

	// Initialise the WG engine for generation of PCM partial samples and set up the invariant parameters
	void initPCM(const int16_t * const pcmWaveAddress, const uint32_t pcmWaveLength, const bool pcmWaveLooped, const bool pcmWaveInterpolated);

	// Update parameters with respect to TVP, TVA and TVF, and generate next sample
	float generateNextSample(const uint32_t amp, const uint16_t pitch, const uint32_t cutoff);

	// Deactivate the WG engine
	void deactivate();

	// Return active state of the WG engine
	bool isActive() const;

	// Return true if the WG engine generates PCM wave samples
	bool isPCMWave() const;
}; // class LA32FloatWaveGenerator

class LA32FloatPartialPair : public LA32PartialPair {
	LA32FloatWaveGenerator master;
	LA32FloatWaveGenerator slave;
	bool ringModulated;
	bool mixed;
	float masterOutputSample;
	float slaveOutputSample;

public:
	// ringModulated should be set to false for the structures with mixing or stereo output
	// ringModulated should be set to true for the structures with ring modulation
	// mixed is used for the structures with ring modulation and indicates whether the master partial output is mixed to the ring modulator output
	void init(const bool ringModulated, const bool mixed);

	// Initialise the WG engine for generation of synth partial samples and set up the invariant parameters
	void initSynth(const PairType master, const bool sawtoothWaveform, const uint8_t pulseWidth, const uint8_t resonance);

	// Initialise the WG engine for generation of PCM partial samples and set up the invariant parameters
	void initPCM(const PairType master, const int16_t * const pcmWaveAddress, const uint32_t pcmWaveLength, const bool pcmWaveLooped);

	// Update parameters with respect to TVP, TVA and TVF, and generate next sample
	void generateNextSample(const PairType master, const uint32_t amp, const uint16_t pitch, const uint32_t cutoff);

	// Perform mixing / ring modulation and return the result
	float nextOutSample();

	// Deactivate the WG engine
	void deactivate(const PairType master);

	// Return active state of the WG engine
	bool isActive(const PairType master) const;
}; // class LA32FloatPartialPair

} // namespace MT32Emu

#endif // #ifndef MT32EMU_LA32_FLOAT_WAVE_GENERATOR_H
