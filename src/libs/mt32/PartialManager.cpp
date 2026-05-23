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
#include <cstring>

#include "internals.h"

#include "PartialManager.h"
#include "Part.h"
#include "Partial.h"
#include "Poly.h"
#include "Synth.h"

namespace MT32Emu {

PartialManager *PartialManager::getPartialManager(Synth &synth) {
	return synth.partialManager;
}

PartialManager::PartialManager(Synth *useSynth) {
	synth = useSynth;
	parts = useSynth->parts;
	inactivePartialCount = synth->getPartialCount();
	partialTable = new Partial *[inactivePartialCount];
	inactivePartials = new int[inactivePartialCount];
	freePolys = new Poly *[synth->getPartialCount()];
	firstFreePolyIndex = 0;
	for (unsigned int i = 0; i < synth->getPartialCount(); i++) {
		partialTable[i] = new Partial(synth, i);
		inactivePartials[i] = inactivePartialCount - i - 1;
		freePolys[i] = new Poly();
	}
}

PartialManager::~PartialManager(void) {
	for (unsigned int i = 0; i < synth->getPartialCount(); i++) {
		delete partialTable[i];
		if (freePolys[i] != NULL) delete freePolys[i];
	}
	delete[] partialTable;
	delete[] inactivePartials;
	delete[] freePolys;
}

void PartialManager::clearAlreadyOutputed() {
	for (unsigned int i = 0; i < synth->getPartialCount(); i++) {
		partialTable[i]->alreadyOutputed = false;
	}
}

bool PartialManager::shouldReverb(int i) {
	return partialTable[i]->shouldReverb();
}

bool PartialManager::produceOutput(int i, IntSample *leftBuf, IntSample *rightBuf, Bit32u bufferLength) {
	return partialTable[i]->produceOutput(leftBuf, rightBuf, bufferLength);
}

bool PartialManager::produceOutput(int i, FloatSample *leftBuf, FloatSample *rightBuf, Bit32u bufferLength) {
	return partialTable[i]->produceOutput(leftBuf, rightBuf, bufferLength);
}

void PartialManager::deactivateAll() {
	for (unsigned int i = 0; i < synth->getPartialCount(); i++) {
		partialTable[i]->deactivate();
	}
}

unsigned int PartialManager::setReserve(Bit8u *rset) {
	unsigned int pr = 0;
	for (int x = 0; x <= 8; x++) {
		numReservedPartialsForPart[x] = rset[x];
		pr += rset[x];
	}
	return pr;
}

Partial *PartialManager::allocPartial(int partNum) {
	if (inactivePartialCount > 0) {
		Partial *partial = partialTable[inactivePartials[--inactivePartialCount]];
		partial->activate(partNum);
		return partial;
	}
	synth->printDebug("PartialManager Error: No inactive partials to allocate for part %d, current partial state:\n", partNum);
	for (Bit32u i = 0; i < synth->getPartialCount(); i++) {
		const Partial *partial = partialTable[i];
		synth->printDebug("[Partial %d]: activation=%d, owner part=%d\n", i, partial->isActive(), partial->getOwnerPart());
	}
	return NULL;
}

unsigned int PartialManager::getFreePartialCount() {
	return inactivePartialCount;
}

// This function is solely used to gather data for debug output at the moment.
void PartialManager::getPerPartPartialUsage(unsigned int perPartPartialUsage[9]) {
	memset(perPartPartialUsage, 0, 9 * sizeof(unsigned int));
	for (unsigned int i = 0; i < synth->getPartialCount(); i++) {
		if (partialTable[i]->isActive()) {
			perPartPartialUsage[partialTable[i]->getOwnerPart()]++;
		}
	}
}

const Poly *PartialManager::getAbortingPoly() {
	return synth->abortingPoly;
}

// Finds the lowest-priority part that is exceeding its reserved partial allocation and has a poly
// in POLY_Releasing, then kills its first releasing poly.
// Parts with higher priority than minPart are not checked.
// Assumes that getFreePartials() has been called to make numReservedPartialsForPart up-to-date.
bool PartialManager::abortFirstReleasingPolyWhereReserveExceeded(int minPart) {
	if (minPart == 8) {
		// Rhythm is highest priority
		minPart = -1;
	}
	for (int partNum = 7; partNum >= minPart; partNum--) {
		int usePartNum = partNum == -1 ? 8 : partNum;
		if (parts[usePartNum]->getActivePartialCount() > numReservedPartialsForPart[usePartNum]) {
			// This part has exceeded its reserved partial count.
			// If it has any releasing polys, kill its first one and we're done.
			if (parts[usePartNum]->abortFirstPoly(POLY_Releasing)) {
				return true;
			}
		}
	}
	return false;
}

// Finds the lowest-priority part that is exceeding its reserved partial allocation and has a poly, then kills
// its first poly in POLY_Held - or failing that, its first poly in any state.
// Parts with higher priority than minPart are not checked.
// Assumes that getFreePartials() has been called to make numReservedPartialsForPart up-to-date.
bool PartialManager::abortFirstPolyPreferHeldWhereReserveExceeded(int minPart) {
	if (minPart == 8) {
		// Rhythm is highest priority
		minPart = -1;
	}
	for (int partNum = 7; partNum >= minPart; partNum--) {
		int usePartNum = partNum == -1 ? 8 : partNum;
		if (parts[usePartNum]->getActivePartialCount() > numReservedPartialsForPart[usePartNum]) {
			// This part has exceeded its reserved partial count.
			// If it has any polys, kill its first (preferably held) one and we're done.
			if (parts[usePartNum]->abortFirstPolyPreferHeld()) {
				return true;
			}
		}
	}
	return false;
}

bool PartialManager::abortFirstPolyPreferReleasingThenHeldWhereReserveExceeded(int minPartNum) {
	for (int candidatePartNum = 7; candidatePartNum >= minPartNum; candidatePartNum--) {
		if (parts[candidatePartNum]->getActivePartialCount() > numReservedPartialsForPart[candidatePartNum]) {
			return abortFirstPolyOnPartPreferReleasingThenHeld(candidatePartNum);
		}
	}
	return false;
}

bool PartialManager::abortFirstPolyOnPartPreferReleasingThenHeld(int partNum) {
	if (parts[partNum]->abortFirstPoly(POLY_Releasing)) {
		return true;
	}
	return parts[partNum]->abortFirstPolyPreferHeld();
}

bool PartialManager::freePartials(unsigned int needed, int partNum) {
	// NOTE: Currently, we don't consider peculiarities of partial allocation when a timbre involves structures with ring modulation.

	if (synth->controlROMFeatures->newGenNoteCancellation) {
		return freePartialsNewGen(needed, partNum);
	}

	while (!synth->isAbortingPoly() && getFreePartialCount() < needed) {
		if (parts[partNum]->getActiveNonReleasingPartialCount() + needed > numReservedPartialsForPart[partNum]) {
			// If priority is given to earlier polys, there's nothing we can do.
			if (synth->getPart(partNum)->getPatchTemp()->patch.assignMode & 1) return false;

			if (needed <= numReservedPartialsForPart[partNum]) {
				// No more partials needed than reserved for this part, only attempt to free partials on the same part.
				abortFirstPolyOnPartPreferReleasingThenHeld(partNum);
				continue;
			}

			// More partials desired than reserved, try to borrow some from parts with lesser priority.
			// NOTE: there's a bug here in the old-gen program, so that the device exhibits undefined behaviour when trying to play
			// a note on the rhythm part beyond the reserve while none of the voice parts has exceeded their reserve.
			// We don't emulate this here, assuming that the intention was to traverse all voice parts, then check the rhythm part.
			if (abortFirstPolyPreferReleasingThenHeldWhereReserveExceeded(partNum < 8 ? partNum : 0)) continue;

			// At this point, old-gen devices try to borrow partials from the rhythm part if it's exceeding the reservation.
			if (parts[8]->getActivePartialCount() > numReservedPartialsForPart[8]
				&& abortFirstPolyOnPartPreferReleasingThenHeld(8)) continue;

			// Alas, this one will be muted.
			return false;
		}

		// OK, we're not going to exceed the reserve. Reclaim our partials from other parts starting from 7 to 0.
		if (abortFirstPolyPreferReleasingThenHeldWhereReserveExceeded(0)) continue;

		// Now try the rhythm part.
		if (parts[8]->getActivePartialCount() > numReservedPartialsForPart[8]
			&& abortFirstPolyOnPartPreferReleasingThenHeld(8)) continue;

		// Lastly, try to silence a poly on this part.
		if (abortFirstPolyOnPartPreferReleasingThenHeld(partNum)) continue;

		// Fair enough, there's no room for it.
		return false;
	}
	return true;
}

bool PartialManager::freePartialsNewGen(unsigned int needed, int partNum) {
	// CONFIRMED: Barring bugs, this matches the real LAPC-I according to information from Mok.

	// BUG: There's a bug in the LAPC-I implementation:
	// When allocating for rhythm part, or when allocating for a part that is using fewer partials than it has reserved,
	// held and playing polys on the rhythm part can potentially be aborted before releasing polys on the rhythm part.
	// This bug isn't present on MT-32.
	// I consider this to be a bug because I think that playing polys should always have priority over held polys,
	// and held polys should always have priority over releasing polys.

	// NOTE: This code generally aborts polys in parts (according to certain conditions) in the following order:
	// 7, 6, 5, 4, 3, 2, 1, 0, 8 (rhythm)
	// (from lowest priority, meaning most likely to have polys aborted, to highest priority, meaning least likely)

	if (needed == 0) {
		return true;
	}

	if (getFreePartialCount() >= needed) {
		return true;
	}

	for (;;) {
		// Abort releasing polys in non-rhythm parts that have exceeded their partial reservation (working backwards from part 7)
		if (!abortFirstReleasingPolyWhereReserveExceeded(0)) {
			break;
		}
		if (synth->isAbortingPoly() || getFreePartialCount() >= needed) {
			return true;
		}
	}

	if (parts[partNum]->getActiveNonReleasingPartialCount() + needed > numReservedPartialsForPart[partNum]) {
		// With the new partials we're freeing for, we would end up using more partials than we have reserved.
		if (synth->getPart(partNum)->getPatchTemp()->patch.assignMode & 1) {
			// Priority is given to earlier polys, so just give up
			return false;
		}
		// Only abort held polys in the target part and parts that have a lower priority
		// (higher part number = lower priority, except for rhythm, which has the highest priority).
		for (;;) {
			if (!abortFirstPolyPreferHeldWhereReserveExceeded(partNum)) {
				break;
			}
			if (synth->isAbortingPoly() || getFreePartialCount() >= needed) {
				return true;
			}
		}
		if (needed > numReservedPartialsForPart[partNum]) {
			return false;
		}
	} else {
		// At this point, we're certain that we've reserved enough partials to play our poly.
		// Check all parts from lowest to highest priority to see whether they've exceeded their
		// reserve, and abort their polys until we have enough free partials or they're within
		// their reserve allocation.
		for (;;) {
			if (!abortFirstPolyPreferHeldWhereReserveExceeded(-1)) {
				break;
			}
			if (synth->isAbortingPoly() || getFreePartialCount() >= needed) {
				return true;
			}
		}
	}

	// Abort polys in the target part until there are enough free partials for the new one
	for (;;) {
		if (!parts[partNum]->abortFirstPolyPreferHeld()) {
			break;
		}
		if (synth->isAbortingPoly() || getFreePartialCount() >= needed) {
			return true;
		}
	}

	// Aww, not enough partials for you.
	return false;
}

const Partial *PartialManager::getPartial(unsigned int partialNum) const {
	if (partialNum > synth->getPartialCount() - 1) {
		return NULL;
	}
	return partialTable[partialNum];
}

Poly *PartialManager::assignPolyToPart(Part *part) {
	if (firstFreePolyIndex < synth->getPartialCount()) {
		Poly *poly = freePolys[firstFreePolyIndex];
		freePolys[firstFreePolyIndex] = NULL;
		firstFreePolyIndex++;
		poly->setPart(part);
		return poly;
	}
	return NULL;
}

void PartialManager::polyFreed(Poly *poly) {
	if (0 == firstFreePolyIndex) {
		synth->printDebug("PartialManager Error: Cannot return freed poly, currently active polys:\n");
		for (Bit32u partNum = 0; partNum < 9; partNum++) {
			const Poly *activePoly = synth->getPart(partNum)->getFirstActivePoly();
			Bit32u polyCount = 0;
			while (activePoly != NULL) {
				activePoly = activePoly->getNext();
				polyCount++;
			}
			synth->printDebug("Part: %i, active poly count: %i\n", partNum, polyCount);
		}
	} else {
		firstFreePolyIndex--;
		freePolys[firstFreePolyIndex] = poly;
	}
	poly->setPart(NULL);
}

void PartialManager::partialDeactivated(int partialIndex) {
	if (inactivePartialCount < synth->getPartialCount()) {
		inactivePartials[inactivePartialCount++] = partialIndex;
		return;
	}
	synth->printDebug("PartialManager Error: Cannot return deactivated partial %d, current partial state:\n", partialIndex);
	for (Bit32u i = 0; i < synth->getPartialCount(); i++) {
		const Partial *partial = partialTable[i];
		synth->printDebug("[Partial %d]: activation=%d, owner part=%d\n", i, partial->isActive(), partial->getOwnerPart());
	}
}

} // namespace MT32Emu
