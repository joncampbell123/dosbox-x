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

#ifndef MT32EMU_TVP_H
#define MT32EMU_TVP_H

#include "globals.h"
#include "Types.h"
#include "Structures.h"

namespace MT32Emu {

class Part;
class Partial;

class TVP {
private:
	const Partial * const partial;
	const MemParams::System * const system; // FIXME: Only necessary because masterTune calculation is done in the wrong place atm.

	const Part *part;
	const TimbreParam::PartialParam *partialParam;
	const MemParams::PatchTemp *patchTemp;

	int processTimerIncrement;
	int counter;
	uint32_t timeElapsed;

	int phase;
	uint32_t basePitch;
	int32_t targetPitchOffsetWithoutLFO;
	int32_t currentPitchOffset;

	int16_t lfoPitchOffset;
	// In range -12 - 36
	int8_t timeKeyfollowSubtraction;

	int16_t pitchOffsetChangePerBigTick;
	uint16_t targetPitchOffsetReachedBigTick;
	unsigned int shifts;

	uint16_t pitch;

	void updatePitch();
	void setupPitchChange(int targetPitchOffset, uint8_t changeDuration);
	void targetPitchOffsetReached();
	void nextPhase();
	void process();
public:
	TVP(const Partial *partial);
	void reset(const Part *part, const TimbreParam::PartialParam *partialParam);
	uint32_t getBasePitch() const;
	uint16_t nextPitch();
	void startDecay();
}; // class TVP

} // namespace MT32Emu

#endif // #ifndef MT32EMU_TVP_H
