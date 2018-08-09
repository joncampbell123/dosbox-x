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
#include <cstdlib>
#include <cstring>

#include "mt32emu.h"
#include "mmath.h"

namespace MT32Emu {

#ifdef INACCURATE_SMOOTH_PAN
// Mok wanted an option for smoother panning, and we love Mok.
static const float PAN_NUMERATOR_NORMAL[] = {0.0f, 0.5f, 1.0f, 1.5f, 2.0f, 2.5f, 3.0f, 3.5f, 4.0f, 4.5f, 5.0f, 5.5f, 6.0f, 6.5f, 7.0f};
#else
// CONFIRMED by Mok: These NUMERATOR values (as bytes, not floats, obviously) are sent exactly like this to the LA32.
static const float PAN_NUMERATOR_NORMAL[] = {0.0f, 0.0f, 1.0f, 1.0f, 2.0f, 2.0f, 3.0f, 3.0f, 4.0f, 4.0f, 5.0f, 5.0f, 6.0f, 6.0f, 7.0f};
#endif
static const float PAN_NUMERATOR_MASTER[] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f};
static const float PAN_NUMERATOR_SLAVE[]  = {0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 7.0f, 7.0f, 7.0f, 7.0f, 7.0f, 7.0f, 7.0f};

Partial::Partial(Synth *useSynth, int useDebugPartialNum) :
	synth(useSynth), debugPartialNum(useDebugPartialNum), sampleNum(0) {
	// Initialisation of tva, tvp and tvf uses 'this' pointer
	// and thus should not be in the initializer list to avoid a compiler warning
	tva = new TVA(this, &ampRamp);
	tvp = new TVP(this);
	tvf = new TVF(this, &cutoffModifierRamp);
	ownerPart = -1;
	poly = NULL;
	pair = NULL;


	// init ptr warnings (load state crashes)
	pcmWave = NULL;
	patchCache = NULL;
	cachebackup.partialParam = NULL;
}

Partial::~Partial() {
	delete tva;
	delete tvp;
	delete tvf;
}

// Only used for debugging purposes
int Partial::debugGetPartialNum() const {
	return debugPartialNum;
}

// Only used for debugging purposes
unsigned long Partial::debugGetSampleNum() const {
	return sampleNum;
}

int Partial::getOwnerPart() const {
	return ownerPart;
}

bool Partial::isActive() const {
	return ownerPart > -1;
}

const Poly *Partial::getPoly() const {
	return poly;
}

void Partial::activate(int part) {
	// This just marks the partial as being assigned to a part
	ownerPart = part;
}

void Partial::deactivate() {
	if (!isActive()) {
		return;
	}
	ownerPart = -1;
	if (poly != NULL) {
		poly->partialDeactivated(this);
	}
	synth->partialStateChanged(this, tva->getPhase(), TVA_PHASE_DEAD);
#if MT32EMU_MONITOR_PARTIALS > 2
	synth->printDebug("[+%lu] [Partial %d] Deactivated", sampleNum, debugPartialNum);
	synth->printPartialUsage(sampleNum);
#endif
	if (isRingModulatingSlave()) {
		pair->la32Pair.deactivate(LA32PartialPair::SLAVE);
	} else {
		la32Pair.deactivate(LA32PartialPair::MASTER);
		if (hasRingModulatingSlave()) {
			pair->deactivate();
			pair = NULL;
		}
	}

	// loadstate ptr warnings (sometimes points to freePolys)
	poly = NULL;

	if (pair != NULL) {
		pair->pair = NULL;
	}
}

void Partial::startPartial(const Part *part, Poly *usePoly, const PatchCache *usePatchCache, const MemParams::RhythmTemp *rhythmTemp, Partial *pairPartial) {
	if (usePoly == NULL || usePatchCache == NULL) {
		synth->printDebug("[Partial %d] *** Error: Starting partial for owner %d, usePoly=%s, usePatchCache=%s", debugPartialNum, ownerPart, usePoly == NULL ? "*** NULL ***" : "OK", usePatchCache == NULL ? "*** NULL ***" : "OK");
		return;
	}
	patchCache = usePatchCache;
	poly = usePoly;
	mixType = patchCache->structureMix;
	structurePosition = patchCache->structurePosition;

	Bit8u panSetting = rhythmTemp != NULL ? rhythmTemp->panpot : part->getPatchTemp()->panpot;
	float panVal;
	if (mixType == 3) {
		if (structurePosition == 0) {
			panVal = PAN_NUMERATOR_MASTER[panSetting];
		} else {
			panVal = PAN_NUMERATOR_SLAVE[panSetting];
		}
		// Do a normal mix independent of any pair partial.
		mixType = 0;
		pairPartial = NULL;
	} else {
		panVal = PAN_NUMERATOR_NORMAL[panSetting];
	}

	// FIXME: Sample analysis suggests that the use of panVal is linear, but there are some some quirks that still need to be resolved.
	stereoVolume.leftVol = panVal / 7.0f;
	stereoVolume.rightVol = 1.0f - stereoVolume.leftVol;

	// SEMI-CONFIRMED: From sample analysis:
	// Found that timbres with 3 or 4 partials (i.e. one using two partial pairs) are mixed in two different ways.
	// Either partial pairs are added or subtracted, it depends on how the partial pairs are allocated.
	// It seems that partials are grouped into quarters and if the partial pairs are allocated in different quarters the subtraction happens.
	// Though, this matters little for the majority of timbres, it becomes crucial for timbres which contain several partials that sound very close.
	// In this case that timbre can sound totally different depending of the way it is mixed up.
	// Most easily this effect can be displayed with the help of a special timbre consisting of several identical square wave partials (3 or 4).
	// Say, it is 3-partial timbre. Just play any two notes simultaneously and the polys very probably are mixed differently.
	// Moreover, the partial allocator retains the last partial assignment it did and all the subsequent notes will sound the same as the last released one.
	// The situation is better with 4-partial timbres since then a whole quarter is assigned for each poly. However, if a 3-partial timbre broke the normal
	// whole-quarter assignment or after some partials got aborted, even 4-partial timbres can be found sounding differently.
	// This behaviour is also confirmed with two more special timbres: one with identical sawtooth partials, and one with PCM wave 02.
	// For my personal taste, this behaviour rather enriches the sounding and should be emulated.
	// Also, the current partial allocator model probably needs to be refined.
	if (debugPartialNum & 8) {
		stereoVolume.leftVol = -stereoVolume.leftVol;
		stereoVolume.rightVol = -stereoVolume.rightVol;
	}

	if (patchCache->PCMPartial) {
		pcmNum = patchCache->pcm;
		if (synth->controlROMMap->pcmCount > 128) {
			// CM-32L, etc. support two "banks" of PCMs, selectable by waveform type parameter.
			if (patchCache->waveform > 1) {
				pcmNum += 128;
			}
		}
		pcmWave = &synth->pcmWaves[pcmNum];
	} else {
		pcmWave = NULL;
	}

	// CONFIRMED: pulseWidthVal calculation is based on information from Mok
	pulseWidthVal = (poly->getVelocity() - 64) * (patchCache->srcPartial.wg.pulseWidthVeloSensitivity - 7) + Tables::getInstance().pulseWidth100To255[patchCache->srcPartial.wg.pulseWidth];
	if (pulseWidthVal < 0) {
		pulseWidthVal = 0;
	} else if (pulseWidthVal > 255) {
		pulseWidthVal = 255;
	}

	pair = pairPartial;
	alreadyOutputed = false;
	tva->reset(part, patchCache->partialParam, rhythmTemp);
	tvp->reset(part, patchCache->partialParam);
	tvf->reset(patchCache->partialParam, tvp->getBasePitch());

	LA32PartialPair::PairType pairType;
	LA32PartialPair *useLA32Pair;
	if (isRingModulatingSlave()) {
		pairType = LA32PartialPair::SLAVE;
		useLA32Pair = &pair->la32Pair;
	} else {
		pairType = LA32PartialPair::MASTER;
		la32Pair.init(hasRingModulatingSlave(), mixType == 1);
		useLA32Pair = &la32Pair;
	}
	if (isPCM()) {
		useLA32Pair->initPCM(pairType, &synth->pcmROMData[pcmWave->addr], pcmWave->len, pcmWave->loop);
	} else {
		useLA32Pair->initSynth(pairType, (patchCache->waveform & 1) != 0, pulseWidthVal, patchCache->srcPartial.tvf.resonance + 1);
	}
	if (!hasRingModulatingSlave()) {
		la32Pair.deactivate(LA32PartialPair::SLAVE);
	}
	// Temporary integration hack
	stereoVolume.leftVol /= 8192.0f;
	stereoVolume.rightVol /= 8192.0f;
}

Bit32u Partial::getAmpValue() {
	// SEMI-CONFIRMED: From sample analysis:
	// (1) Tested with a single partial playing PCM wave 77 with pitchCoarse 36 and no keyfollow, velocity follow, etc.
	// This gives results within +/- 2 at the output (before any DAC bitshifting)
	// when sustaining at levels 156 - 255 with no modifiers.
	// (2) Tested with a special square wave partial (internal capture ID tva5) at TVA envelope levels 155-255.
	// This gives deltas between -1 and 0 compared to the real output. Note that this special partial only produces
	// positive amps, so negative still needs to be explored, as well as lower levels.
	//
	// Also still partially unconfirmed is the behaviour when ramping between levels, as well as the timing.
	// TODO: The tests above were performed using the float model, to be refined
	Bit32u ampRampVal = 67117056 - ampRamp.nextValue();
	if (ampRamp.checkInterrupt()) {
		tva->handleInterrupt();
	}
	return ampRampVal;
}

Bit32u Partial::getCutoffValue() {
	if (isPCM()) {
		return 0;
	}
	Bit32u cutoffModifierRampVal = cutoffModifierRamp.nextValue();
	if (cutoffModifierRamp.checkInterrupt()) {
		tvf->handleInterrupt();
	}
	return (tvf->getBaseCutoff() << 18) + cutoffModifierRampVal;
}

unsigned long Partial::generateSamples(Bit16s *partialBuf, unsigned long length) {
	if (!isActive() || alreadyOutputed) {
		return 0;
	}
	if (poly == NULL) {
		synth->printDebug("[Partial %d] *** ERROR: poly is NULL at Partial::generateSamples()!", debugPartialNum);
		return 0;
	}
	alreadyOutputed = true;

	for (sampleNum = 0; sampleNum < length; sampleNum++) {
		if (!tva->isPlaying() || !la32Pair.isActive(LA32PartialPair::MASTER)) {
			deactivate();
			break;
		}
		la32Pair.generateNextSample(LA32PartialPair::MASTER, getAmpValue(), tvp->nextPitch(), getCutoffValue());
		if (hasRingModulatingSlave()) {
			la32Pair.generateNextSample(LA32PartialPair::SLAVE, pair->getAmpValue(), pair->tvp->nextPitch(), pair->getCutoffValue());
			if (!pair->tva->isPlaying() || !la32Pair.isActive(LA32PartialPair::SLAVE)) {
				pair->deactivate();
				if (mixType == 2) {
					deactivate();
					break;
				}
			}
		}
		*partialBuf++ = la32Pair.nextOutSample();
	}
	unsigned long renderedSamples = sampleNum;
	sampleNum = 0;
	return renderedSamples;
}

bool Partial::hasRingModulatingSlave() const {
	return pair != NULL && structurePosition == 0 && (mixType == 1 || mixType == 2);
}

bool Partial::isRingModulatingSlave() const {
	return pair != NULL && structurePosition == 1 && (mixType == 1 || mixType == 2);
}

bool Partial::isPCM() const {
	return pcmWave != NULL;
}

const ControlROMPCMStruct *Partial::getControlROMPCMStruct() const {
	if (pcmWave != NULL) {
		return pcmWave->controlROMPCMStruct;
	}
	return NULL;
}

Synth *Partial::getSynth() const {
	return synth;
}

bool Partial::produceOutput(float *leftBuf, float *rightBuf, unsigned long length) {
	if (!isActive() || alreadyOutputed || isRingModulatingSlave()) {
		return false;
	}
	if (poly == NULL) {
		synth->printDebug("[Partial %d] *** ERROR: poly is NULL at Partial::produceOutput()!", debugPartialNum);
		return false;
	}
	unsigned long numGenerated = generateSamples(myBuffer, length);
	for (unsigned int i = 0; i < numGenerated; i++) {
		*leftBuf++ = myBuffer[i] * stereoVolume.leftVol;
		*rightBuf++ = myBuffer[i] * stereoVolume.rightVol;
	}
	for (; numGenerated < length; numGenerated++) {
		*leftBuf++ = 0.0f;
		*rightBuf++ = 0.0f;
	}
	return true;
}

bool Partial::shouldReverb() {
	if (!isActive()) {
		return false;
	}
	return patchCache->reverb;
}

void Partial::startAbort() {
	// This is called when the partial manager needs to terminate partials for re-use by a new Poly.
	tva->startAbort();
}

void Partial::startDecayAll() {
	tva->startDecay();
	tvp->startDecay();
	tvf->startDecay();
}

}
