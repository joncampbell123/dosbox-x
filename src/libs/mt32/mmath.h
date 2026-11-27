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

#ifndef MT32EMU_MMATH_H
#define MT32EMU_MMATH_H

#include <cmath>

namespace MT32Emu {

using ::std::fmod;
using ::std::exp;
using ::std::log;
using ::std::log10;
using ::std::sin;
using ::std::cos;

// Mathematical constants
const float FLOAT_PI = 3.1415927f;
const float FLOAT_2PI = 6.2831853f;
const float FLOAT_LN_2 = 0.6931472f;

static inline float EXP2F(float x) {
#ifdef MT32EMU_WITH_EXP2F
	return exp2f(x);
#else
	// This is an inaccurate approximation.
	// We prefer it as a faster alternative to a combination of modf, exp and ldexp which would improve the accuracy.
	return exp(FLOAT_LN_2 * x);
#endif
}

static inline float LOG2F(float x) {
	return log(x) / FLOAT_LN_2;
}

static inline float LOG10F(float x) {
	return log10(x);
}

} // namespace MT32Emu

#endif // #ifndef MT32EMU_MMATH_H
