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

#ifndef MT32EMU_LA32_FLOAT_MATH_H
#define MT32EMU_LA32_FLOAT_MATH_H

#include "globals.h"
#include "mmath.h"
#include "Types.h"

namespace MT32Emu {

static const float MIDDLE_CUTOFF_VALUE = 128.0f;

/*
 * The LA32FloatWaveGenerator heavily uses some math functions, e.g. exp, sin and cos. The time it takes to compute these functions
 * affects the performance rather considerably. Unfortunately, not every implementation of the standard math library provides for
 * the best performance even in 2026. In particular, there is no difference in speed between the single and double precision in
 * certain environments. Additionally, we cut a few corners specific to our use case.
 */
namespace LA32FloatMath {

struct VanillaMath {
	static void init() {
		// No-op
	}

	static float fsin(float x) {
		return sin(FLOAT_PI * x);
	}

	static float fcos(float x) {
		return cos(FLOAT_PI * x);
	}

	static float cosineLengthFactor(float cutoff) {
		// found from sample analysis
		return EXP2F((cutoff - MIDDLE_CUTOFF_VALUE) / -16.0f);
	}

	static float unlogPCMSample(Bit32u pcmSample) {
		return EXP2F((pcmSample - 32787.0f) / 2048.0f);
	}

	static float resonanceAmpBase(Bit8u resonance) {
		return EXP2F(1.0f - (32 - resonance) / 4.0f);
	}

	static float pulseLengthFactor(Bit8u pulseWidth) {
		return EXP2F((64 - pulseWidth) / 64.0f);
	}

	static float linearAmpFactor(Bit32u amp) {
		// See Partial::getAmpValue for more details
		return EXP2F(amp / -1024.0f / 4096.0f);
	}

	static float frequency(Bit16u pitch) {
		return EXP2F(pitch / 4096.0f - 16.0f) * SAMPLE_RATE;
	}

	static float attenuationFactorBelowMiddleCutoff(float cutoff) {
		return EXP2F(-0.125f * (MIDDLE_CUTOFF_VALUE - cutoff));
	}

	static float unlogResonanceAmpFade(float resAmpFadeLog2) {
		return EXP2F(resAmpFadeLog2);
	}
}; // struct VanillaMath

struct LinearInterpolator {
	static float exp2Taylor(float approxExp2, float delta) {
		return approxExp2 + approxExp2 * delta;
	}

	static float sinTaylor(float approxSin, float approxCos, float delta) {
		return approxSin + approxCos * delta;
	}
};

struct QuadraticInterpolator {
	static float exp2Taylor(float approxExp2, float delta) {
		return approxExp2 + (approxExp2 + 0.5f * approxExp2 * delta) * delta;
	}

	static float sinTaylor(float approxSin, float approxCos, float delta) {
		return approxSin + (approxCos - 0.5f * approxSin * delta) * delta;
	}
};

template<class Interpolator, int INDEX_BITS>
class Exp2Table {
	// The table includes one more entry for 2 ^ 1 = 2 to simplify the rounding logic.
	static const Bit32u SIZE_M1 = 1 << INDEX_BITS;
	static const Bit32u SIZE = SIZE_M1 + 1;

	// These support an additional optimisation for the LA32 native logarithmic format which has a 12-bit fractional part.
	// See fexp2i(Bit32u).
	static const int FRAC_BITS = 12;
	static const Bit32u FRAC_MASK = (1 << FRAC_BITS) - 1;

	static float scaleFactor;
	static float table[SIZE];

	// The ANSI C++ does not support inline member specialisation. The workaround is to use a helper type and overloading.
	template<bool> class InterpolationNeededFlag {};

	static float interpolate(float scaledFracX) {
		Bit32u tableIx = Bit32u(scaledFracX + 0.5f);
		float delta = (scaledFracX - tableIx) * scaleFactor;
		return Interpolator::exp2Taylor(table[tableIx], delta);
	}

	// For the table resolutions lesser than the LA32 fraction, we have to interpolate.
	static float mantissa(Bit32u fracX, InterpolationNeededFlag<true>) {
		static const float FRAC_SCALE_FACTOR = SIZE_M1 / float(1 << FRAC_BITS);

		return interpolate(fracX * FRAC_SCALE_FACTOR);
	}

	// For the table resolutions that match or exceed the LA32 fraction, the interpolation is unnecessary, and we can perform a direct
	// table lookup (just maybe scale the argument).
	static float mantissa(Bit32u fracX, InterpolationNeededFlag<false>) {
		static const Bit32u FRAC_SHIFT_BITS = INDEX_BITS - FRAC_BITS;

		return table[fracX << FRAC_SHIFT_BITS];
	}

public:
	static void init() {
		scaleFactor = FLOAT_LN_2 / SIZE_M1;

		float tableSizeRcp = 1.0f / SIZE_M1;
		for (Bit32u i = 0; i < SIZE_M1; i++) {
			table[i] = EXP2F(tableSizeRcp * i);
		}
		table[SIZE_M1] = 2.0f;
	}

	// This function takes the parameter in the LA32 native fixed-point logarithmic format with 12-bit fractional part.
	// Splitting the integer argument is more efficient, yet the 12-bit fractions map 1:1 to the "big" LUT, so we can look up directly.
	// The maximum value of the argument must never exceed 16 * 4096, so the integer overflow when scaling the result doesn't happen.
	static float fexp2i(Bit32u x) {
		Bit32u upScale = x >> FRAC_BITS;
		Bit32u fracX = x & FRAC_MASK;

		return mantissa(fracX, InterpolationNeededFlag<INDEX_BITS < FRAC_BITS>()) * (1 << upScale);
	}

	// Here we cut some corners to avoid standard math library calls each of which significantly degrades the performance in most cases.
	// First limitation: the parameter must never be negative. To ensure this, we shake the related formulae.
	// Next thing: the maximum value of the argument is also limited by 30.0 to avoid the integer overflow when scaling the result.
	static float fexp2(float x) {
		Bit32u upScale = Bit32u(x);
		float fracX = x - upScale;
		return interpolate(fracX * SIZE_M1) * (1 << upScale);
	}
}; // class Exp2Table

// The table covers the entire sine period to simplify the computational logic.
// Even an attempt to halve the table by taking advantage of the sine's antisymmetry well degrades the performance (either using
// simple conditions or various dirty tricks for setting the sign bit directly). The tests showed numbers close to those achieved
// with the quadratic interpolation, but the table is already 8 times shorter in the latter case.
template<class Interpolator, int INDEX_BITS>
class SineTable {
	static const Bit32u SIZE = 1 << INDEX_BITS;
	static const Bit32u INDEX_MASK = SIZE - 1;
	static const Bit32u PI_IX = SIZE >> 1;
	static const Bit32u HALF_PI_IX = SIZE >> 2;

	static float scaleFactor;
	static float table[SIZE];

public:
	static void init() {
		scaleFactor = FLOAT_2PI / SIZE;
		for (Bit32u i = 0; i < SIZE; i++) {
			table[i] = sin(scaleFactor * i);
		}
	}

	// The first cut corner: the argument is scaled by PI, meaning a frequency relative to the Nyquist frequency.
	// In 100% of our call sites we had to multiply the argument by PI just to revert that here. Additionally, this also _reduces_
	// the computation error for bigger arguments since the irrational factor PI isn't involved.
	//
	// The second cut corner: the argument must never be negative nor exceed 2^20. That's always the case with our usages. This makes
	// the argument reduction unnecessary as a separate step.
	static float fsin(float x) {
		float scaledX = x * PI_IX;
		Bit32u sinIx = Bit32u(scaledX + 0.5f);
		float approxSin = table[sinIx & INDEX_MASK];
		Bit32u cosIx = sinIx + HALF_PI_IX;
		float approxCos = table[cosIx & INDEX_MASK];
		float delta = (scaledX - sinIx) * scaleFactor;
		float res = Interpolator::sinTaylor(approxSin, approxCos, delta);
		return res;
	}
}; // class SineTable

template<class Interpolator, int EXP2_INDEX_BITS, int SINE_INDEX_BITS>
struct FasterMath {
	typedef LA32FloatMath::Exp2Table<Interpolator, EXP2_INDEX_BITS> Exp2Table;
	typedef LA32FloatMath::SineTable<Interpolator, SINE_INDEX_BITS> SineTable;

	static void init() {
		Exp2Table::init();
		SineTable::init();
	}

	static float cosineLengthFactor(float cutoff) {
		static const float mult = 1.0f / 256.0f; // EXP2F(-MIDDLE_CUTOFF_VALUE / 16.0f)
		return Exp2Table::fexp2((2.0f * MIDDLE_CUTOFF_VALUE - cutoff) / 16.0f) * mult;
	}

	static float unlogPCMSample(Bit32u pcmSample) {
		static const float mult = 1.51609812e-5f; // EXP2F(-32787.0f / 2048.0f)
		return Exp2Table::fexp2i(pcmSample << 1) * mult;
	}

	static float resonanceAmpBase(Bit8u resonance) {
		static const float mult = 1.0f / 128.0f; // EXP2F(1.0f - 32.0f / 4.0f)
		return Exp2Table::fexp2i(resonance << 10) * mult;
	}

	static float pulseLengthFactor(Bit8u pulseWidth) {
		static const float mult = 0.125f; // EXP2F(-192.0f / 64.0f)
		return Exp2Table::fexp2i((256 - pulseWidth) << 6) * mult;
	}

	static float linearAmpFactor(Bit32u amp) {
		static const float logAmpScaleFactor = 1.0f / 1024.0f / 4096.0f;
		static const float mult = 1.5238139e-5f; // EXP2F(-67117056.0f * logAmpScaleFactor)
		// TODO: The amp value subtraction actually reverts what we did in Partial::getAmpValue, this should be simplified.
		return Exp2Table::fexp2((67117056 - amp) * logAmpScaleFactor) * mult;
	}

	static float frequency(Bit16u pitch) {
		static const float mult = SAMPLE_RATE / 65536.0f; // SAMPLE_RATE * EXP2F(-16.0f)
		return Exp2Table::fexp2i(pitch) * mult;
	}

	static float attenuationFactorBelowMiddleCutoff(float cutoff) {
		static const float mult = 1.0f / 65536.0f; // EXP2F(-0.125f * MIDDLE_CUTOFF_VALUE)
		return Exp2Table::fexp2(0.125f * cutoff) * mult;
	}

	static float unlogResonanceAmpFade(float resAmpFadeLog2) {
		static const float mult = 1.0f / (1 << 30);
		// Basically, the resonance wave barely affects the resulting wave as soon as the log2 factor falls below -23 while working with
		// the IEEE floats. Picking -30 as the minimum log2 gives a bit smoother transition of the addition to a no-op in most cases yet
		// ensures that the argument fits the fexp2 requirements.
		return (resAmpFadeLog2 < -30.0f) ? 0.0f : Exp2Table::fexp2(30.0f + resAmpFadeLog2) * mult;
	}

	static float fsin(float x) {
		return SineTable::fsin(x);
	}

	static float fcos(float x) {
		return fsin(x + 0.5f);
	}
}; // struct FasterMath

template<class Interpolator, int INDEX_BITS>
float Exp2Table<Interpolator, INDEX_BITS>::scaleFactor = 0;
template<class Interpolator, int INDEX_BITS>
float Exp2Table<Interpolator, INDEX_BITS>::table[] = {};
template<class Interpolator, int INDEX_BITS>
float SineTable<Interpolator, INDEX_BITS>::scaleFactor = 0;
template<class Interpolator, int INDEX_BITS>
float SineTable<Interpolator, INDEX_BITS>::table[] = {};

} // namespace LA32FloatMath

} // namespace MT32Emu

#endif // #ifndef MT32EMU_LA32_FLOAT_MATH_H
