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

#ifndef MT32EMU_A_REVERB_MODEL_H
#define MT32EMU_A_REVERB_MODEL_H

namespace MT32Emu {

struct AReverbSettings {
	const Bit32u * const allpassSizes;
	const Bit32u * const combSizes;
	const Bit32u * const outLPositions;
	const Bit32u * const outRPositions;
	const Bit32u * const filterFactor;
	const Bit32u * const decayTimes;
	const Bit32u * const wetLevels;
	const Bit32u lpfAmp;
};

class RingBuffer {
protected:
	float *buffer;
	const Bit32u size;
	Bit32u index;

public:
	RingBuffer(const Bit32u newsize);
	virtual ~RingBuffer();
	float next();
	bool isEmpty() const;
	void mute();
};

class AllpassFilter : public RingBuffer {
public:
	AllpassFilter(const Bit32u useSize);
	float process(const float in);
};

class CombFilter : public RingBuffer {
	float feedbackFactor;
	float filterFactor;

public:
	CombFilter(const Bit32u useSize);
	void process(const float in);
	float getOutputAt(const Bit32u outIndex) const;
	void setFeedbackFactor(const float useFeedbackFactor);
	void setFilterFactor(const float useFilterFactor);
};

class AReverbModel : public ReverbModel {
	AllpassFilter **allpasses;
	CombFilter **combs;

	const AReverbSettings &currentSettings;
	float lpfAmp;
	float wetLevel;
	void mute();

public:
	AReverbModel(const ReverbMode mode);
	~AReverbModel();
	void open();
	void close();
	void setParameters(Bit8u time, Bit8u level);
	void process(const float *inLeft, const float *inRight, float *outLeft, float *outRight, unsigned long numSamples);
	bool isActive() const;
};

}

#endif
