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

#ifndef MT32EMU_B_REVERB_MODEL_H
#define MT32EMU_B_REVERB_MODEL_H

namespace MT32Emu {

struct BReverbSettings {
	const Bit32u numberOfAllpasses;
	const Bit32u * const allpassSizes;
	const Bit32u numberOfCombs;
	const Bit32u * const combSizes;
	const Bit32u * const outLPositions;
	const Bit32u * const outRPositions;
	const Bit32u * const filterFactors;
	const Bit32u * const feedbackFactors;
	const Bit32u * const dryAmps;
	const Bit32u * const wetLevels;
	const Bit32u lpfAmp;
};

class RingBuffer {
protected:
	Bit16s *buffer;
	const Bit32u size;
	Bit32u index;

public:
	RingBuffer(const Bit32u size);
	virtual ~RingBuffer();
	Bit32s next();
	bool isEmpty() const;
	void mute();
};

class AllpassFilter : public RingBuffer {
public:
	AllpassFilter(const Bit32u size);
	Bit32s process(const Bit32s in);
};

class CombFilter : public RingBuffer {
protected:
	const Bit32u filterFactor;
	Bit32u feedbackFactor;

public:
	CombFilter(const Bit32u size, const Bit32u useFilterFactor);
	virtual void process(const Bit32s in); // Actually, no need to make it virtual, but for sure
	Bit32s getOutputAt(const Bit32u outIndex) const;
	void setFeedbackFactor(const Bit32u useFeedbackFactor);
};

class DelayWithLowPassFilter : public CombFilter {
	Bit32u amp;

public:
	DelayWithLowPassFilter(const Bit32u useSize, const Bit32u useFilterFactor, const Bit32u useAmp);
	void process(const Bit32s in);
	void setFeedbackFactor(const Bit32u) {}
};

class TapDelayCombFilter : public CombFilter {
	Bit32u outL;
	Bit32u outR;

public:
	TapDelayCombFilter(const Bit32u useSize, const Bit32u useFilterFactor);
	void process(const Bit32s in);
	Bit32s getLeftOutput() const;
	Bit32s getRightOutput() const;
	void setOutputPositions(const Bit32u useOutL, const Bit32u useOutR);
};

class BReverbModel : public ReverbModel {
	AllpassFilter **allpasses;
	CombFilter **combs;

	const BReverbSettings &currentSettings;
	const bool tapDelayMode;
	Bit32u dryAmp;
	Bit32u wetLevel;
	void mute();

public:
	BReverbModel(const ReverbMode mode);
	~BReverbModel();
	void open();
	void close();
	void setParameters(Bit8u time, Bit8u level);
	void process(const float *inLeft, const float *inRight, float *outLeft, float *outRight, unsigned long numSamples);
	bool isActive() const;
};

}

#endif
