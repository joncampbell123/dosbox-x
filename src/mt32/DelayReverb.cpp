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
#include <cstring>
#include "mt32emu.h"
#include "DelayReverb.h"
#include "FileStream.h"

namespace MT32Emu {

// CONFIRMED: The values below are found via analysis of digital samples and tracing reverb RAM address / data lines. Checked with all time and level combinations.
// Obviously:
// rightDelay = (leftDelay - 2) * 2 + 2
// echoDelay = rightDelay - 1
// Leaving these separate in case it's useful for work on other reverb modes...
static const Bit32u REVERB_TIMINGS[8][3]= {
	// {leftDelay, rightDelay, feedbackDelay}
	{402, 802, 801},
	{626, 1250, 1249},
	{962, 1922, 1921},
	{1490, 2978, 2977},
	{2258, 4514, 4513},
	{3474, 6946, 6945},
	{5282, 10562, 10561},
	{8002, 16002, 16001}
};

// Reverb amp is found as dryAmp * wetAmp
static const Bit32u REVERB_AMP[8] = {0x20*0x18, 0x50*0x18, 0x50*0x28, 0x50*0x40, 0x50*0x60, 0x50*0x80, 0x50*0xA8, 0x50*0xF8};
static const Bit32u REVERB_FEEDBACK67 = 0x60;
static const Bit32u REVERB_FEEDBACK = 0x68;
static const float LPF_VALUE = 0x68 / 256.0f;

static const Bit32u BUFFER_SIZE = 16384;

DelayReverb::DelayReverb() {
	buf = NULL;
	setParameters(0, 0);


	// loadstate init
	//bufSize = 0;
}

DelayReverb::~DelayReverb() {
	delete[] buf;
}

void DelayReverb::open() {
	if (buf == NULL) {
		delete[] buf;

		buf = new float[BUFFER_SIZE];

		recalcParameters();

		// mute buffer
		bufIx = 0;
		if (buf != NULL) {
			for (unsigned int i = 0; i < BUFFER_SIZE; i++) {
				buf[i] = 0.0f;
			}
		}
	}
}

void DelayReverb::close() {
	delete[] buf;
	buf = NULL;
}

// This method will always trigger a flush of the buffer
void DelayReverb::setParameters(Bit8u newTime, Bit8u newLevel) {
	time = newTime;
	level = newLevel;
	recalcParameters();
}

void DelayReverb::recalcParameters() {
	// Number of samples between impulse and eventual appearance on the left channel
	delayLeft = REVERB_TIMINGS[time][0];
	// Number of samples between impulse and eventual appearance on the right channel
	delayRight = REVERB_TIMINGS[time][1];
	// Number of samples between a response and that response feeding back/echoing
	delayFeedback = REVERB_TIMINGS[time][2];

	if (level < 3 || time < 6) {
		feedback = REVERB_FEEDBACK / 256.0f;
	} else {
		feedback = REVERB_FEEDBACK67 / 256.0f;
	}

	// Overall output amp
	amp = (level == 0 && time == 0) ? 0.0f : REVERB_AMP[level] / 65536.0f;
}

void DelayReverb::process(const float *inLeft, const float *inRight, float *outLeft, float *outRight, unsigned long numSamples) {
	if (buf == NULL) return;

	for (unsigned int sampleIx = 0; sampleIx < numSamples; sampleIx++) {
		// The ring buffer write index moves backwards; reads are all done with positive offsets.
		Bit32u bufIxPrev = (bufIx + 1) % BUFFER_SIZE;
		Bit32u bufIxLeft = (bufIx + delayLeft) % BUFFER_SIZE;
		Bit32u bufIxRight = (bufIx + delayRight) % BUFFER_SIZE;
		Bit32u bufIxFeedback = (bufIx + delayFeedback) % BUFFER_SIZE;

		// Attenuated input samples and feedback response are directly added to the current ring buffer location
		float lpfIn = amp * (inLeft[sampleIx] + inRight[sampleIx]) + feedback * buf[bufIxFeedback];

		// Single-pole IIR filter found on real devices
		buf[bufIx] = buf[bufIxPrev] * LPF_VALUE - lpfIn;

		outLeft[sampleIx] = buf[bufIxLeft];
		outRight[sampleIx] = buf[bufIxRight];

		bufIx = (BUFFER_SIZE + bufIx - 1) % BUFFER_SIZE;
	}
}

bool DelayReverb::isActive() const {
	if (buf == NULL) return false;

	float *b = buf;
	float max = 0.001f;
	for (Bit32u i = 0; i < BUFFER_SIZE; i++) {
		if ((*b < -max) || (*b > max)) return true;
		b++;
	}
	return false;
}


void DelayReverb::saveState( std::ostream &stream )
{
	stream.write(reinterpret_cast<const char*>(&time), sizeof(time) );
	stream.write(reinterpret_cast<const char*>(&level), sizeof(level) );
	//stream.write(reinterpret_cast<const char*>(&sampleRate), sizeof(sampleRate) );
	//stream.write(reinterpret_cast<const char*>(&bufSize), sizeof(bufSize) );
	stream.write(reinterpret_cast<const char*>(&bufIx), sizeof(bufIx) );

	if( buf )
		stream.write(reinterpret_cast<const char*>(buf), sizeof(buf) * sizeof(float) );

	stream.write(reinterpret_cast<const char*>(&delayLeft), sizeof(delayLeft) );
	stream.write(reinterpret_cast<const char*>(&delayRight), sizeof(delayRight) );
	stream.write(reinterpret_cast<const char*>(&delayFeedback), sizeof(delayFeedback) );
	//stream.write(reinterpret_cast<const char*>(&fade), sizeof(fade) );
	stream.write(reinterpret_cast<const char*>(&feedback), sizeof(feedback) );
}


void DelayReverb::loadState( std::istream &stream )
{
	stream.read(reinterpret_cast<char*>(&time), sizeof(time) );
	stream.read(reinterpret_cast<char*>(&level), sizeof(level) );
	//stream.read(reinterpret_cast<char*>(&sampleRate), sizeof(sampleRate) );
	//stream.read(reinterpret_cast<char*>(&bufSize), sizeof(bufSize) );
	stream.read(reinterpret_cast<char*>(&bufIx), sizeof(bufIx) );


	if( buf ) delete[] buf;
	if( sizeof(buf) == 0 ) {
		buf = NULL;
	}
	else {
		buf = new float[ sizeof(buf) ];
		stream.read(reinterpret_cast<char*>(buf), sizeof(buf) * sizeof(float) );
	}


	stream.read(reinterpret_cast<char*>(&delayLeft), sizeof(delayLeft) );
	stream.read(reinterpret_cast<char*>(&delayRight), sizeof(delayRight) );
	stream.read(reinterpret_cast<char*>(&delayFeedback), sizeof(delayFeedback) );
	//stream.read(reinterpret_cast<char*>(&fade), sizeof(fade) );
	stream.read(reinterpret_cast<char*>(&feedback), sizeof(feedback) );
}

}
