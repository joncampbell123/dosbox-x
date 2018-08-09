// Reverb model implementation
//
// Written by Jezar at Dreampoint, June 2000
// Modifications by Jerome Fisher, 2009, 2011
// http://www.dreampoint.co.uk
// This code is public domain

#include "revmodel.h"

revmodel::revmodel(float scaletuning)
{
	int i;
	int bufsize;

	// Allocate buffers for the components
	for (i = 0; i < numcombs; i++) {
		bufsize = int(scaletuning * combtuning[i]);
		combL[i].setbuffer(new float[bufsize], bufsize);
		bufsize += int(scaletuning * stereospread);
		combR[i].setbuffer(new float[bufsize], bufsize);
	}
	for (i = 0; i < numallpasses; i++) {
		bufsize = int(scaletuning * allpasstuning[i]);
		allpassL[i].setbuffer(new float[bufsize], bufsize);
		allpassL[i].setfeedback(0.5f);
		bufsize += int(scaletuning * stereospread);
		allpassR[i].setbuffer(new float[bufsize], bufsize);
		allpassR[i].setfeedback(0.5f);
	}

	// Set default values
	dry = initialdry;
	wet = initialwet*scalewet;
	damp = initialdamp*scaledamp;
	roomsize = (initialroom*scaleroom) + offsetroom;
	width = initialwidth;
	mode = initialmode;
	update();

	// Buffer will be full of rubbish - so we MUST mute them
	mute();
}

revmodel::~revmodel()
{
	int i;

	for (i = 0; i < numcombs; i++) {
		combL[i].deletebuffer();
		combR[i].deletebuffer();
	}
	for (i = 0; i < numallpasses; i++) {
		allpassL[i].deletebuffer();
		allpassR[i].deletebuffer();
	}
}

void revmodel::mute()
{
	int i;

	if (getmode() >= freezemode)
		return;

	for (i=0;i<numcombs;i++)
	{
		combL[i].mute();
		combR[i].mute();
	}
	for (i=0;i<numallpasses;i++)
	{
		allpassL[i].mute();
		allpassR[i].mute();
	}

	// Init LPF history
	filtprev1 = 0;
	filtprev2 = 0;
}

void revmodel::process(const float *inputL, const float *inputR, float *outputL, float *outputR, long numsamples)
{
	float outL,outR,input;

	while (numsamples-- > 0)
	{
		int i;

		outL = outR = 0;
		input = (*inputL + *inputR) * gain;

		// Implementation of 2-stage IIR single-pole low-pass filter
		// found at the entrance of reverb processing on real devices
		filtprev1 += (input - filtprev1) * filtval;
		filtprev2 += (filtprev1 - filtprev2) * filtval;
		input = filtprev2;

		int s = -1;
		// Accumulate comb filters in parallel
		for (i=0; i<numcombs; i++)
		{
			outL += s * combL[i].process(input);
			outR += s * combR[i].process(input);
			s = -s;
		}

		// Feed through allpasses in series
		for (i=0; i<numallpasses; i++)
		{
			outL = allpassL[i].process(outL);
			outR = allpassR[i].process(outR);
		}

		// Calculate output REPLACING anything already there
		*outputL = outL*wet1 + outR*wet2;
		*outputR = outR*wet1 + outL*wet2;
		
		inputL++;
		inputR++;
		outputL++;
		outputR++;
	}
}

void revmodel::update()
{
// Recalculate internal values after parameter change

	int i;

	wet1 = wet*(width/2 + 0.5f);
	wet2 = wet*((1-width)/2);

	if (mode >= freezemode)
	{
		roomsize1 = 1;
		damp1 = 0;
		gain = muted;
	}
	else
	{
		roomsize1 = roomsize;
		damp1 = damp;
		gain = fixedgain;
	}

	for (i=0; i<numcombs; i++)
	{
		combL[i].setfeedback(roomsize1);
		combR[i].setfeedback(roomsize1);
	}

	for (i=0; i<numcombs; i++)
	{
		combL[i].setdamp(damp1);
		combR[i].setdamp(damp1);
	}
}

// The following get/set functions are not inlined, because
// speed is never an issue when calling them, and also
// because as you develop the reverb model, you may
// wish to take dynamic action when they are called.

void revmodel::setroomsize(float value)
{
	roomsize = (value*scaleroom) + offsetroom;
	update();
}

float revmodel::getroomsize()
{
	return (roomsize-offsetroom)/scaleroom;
}

void revmodel::setdamp(float value)
{
	damp = value*scaledamp;
	update();
}

float revmodel::getdamp()
{
	return damp/scaledamp;
}

void revmodel::setwet(float value)
{
	wet = value*scalewet;
	update();
}

float revmodel::getwet()
{
	return wet/scalewet;
}

void revmodel::setdry(float value)
{
	dry = value*scaledry;
}

float revmodel::getdry()
{
	return dry/scaledry;
}

void revmodel::setwidth(float value)
{
	width = value;
	update();
}

float revmodel::getwidth()
{
	return width;
}

void revmodel::setmode(float value)
{
	mode = value;
	update();
}

float revmodel::getmode()
{
	if (mode >= freezemode)
		return 1;
	else
		return 0;
}

void revmodel::setfiltval(float value)
{
	filtval = value;
}


void revmodel::saveState( std::ostream &stream )
{
	stream.write(reinterpret_cast<const char*>(&gain), sizeof(gain) );
	stream.write(reinterpret_cast<const char*>(&roomsize), sizeof(roomsize) );
	stream.write(reinterpret_cast<const char*>(&roomsize1), sizeof(roomsize1) );
	stream.write(reinterpret_cast<const char*>(&damp), sizeof(damp) );
	stream.write(reinterpret_cast<const char*>(&damp1), sizeof(damp1) );
	stream.write(reinterpret_cast<const char*>(&wet), sizeof(wet) );
	stream.write(reinterpret_cast<const char*>(&wet1), sizeof(wet1) );
	stream.write(reinterpret_cast<const char*>(&wet2), sizeof(wet2) );
	stream.write(reinterpret_cast<const char*>(&dry), sizeof(dry) );
	stream.write(reinterpret_cast<const char*>(&width), sizeof(width) );
	stream.write(reinterpret_cast<const char*>(&mode), sizeof(mode) );
	stream.write(reinterpret_cast<const char*>(&filtval), sizeof(filtval) );
	stream.write(reinterpret_cast<const char*>(&filtprev1), sizeof(filtprev1) );
	stream.write(reinterpret_cast<const char*>(&filtprev2), sizeof(filtprev2) );


	for( int lcv=0; lcv<numcombs; lcv++ ) {
		combL[lcv].saveState(stream);
		combR[lcv].saveState(stream);
	}

	for( int lcv=0; lcv<numallpasses; lcv++ ) {
		allpassL[lcv].saveState(stream);
		allpassR[lcv].saveState(stream);
	}
}


void revmodel::loadState( std::istream &stream )
{
	stream.read(reinterpret_cast<char*>(&gain), sizeof(gain) );
	stream.read(reinterpret_cast<char*>(&roomsize), sizeof(roomsize) );
	stream.read(reinterpret_cast<char*>(&roomsize1), sizeof(roomsize1) );
	stream.read(reinterpret_cast<char*>(&damp), sizeof(damp) );
	stream.read(reinterpret_cast<char*>(&damp1), sizeof(damp1) );
	stream.read(reinterpret_cast<char*>(&wet), sizeof(wet) );
	stream.read(reinterpret_cast<char*>(&wet1), sizeof(wet1) );
	stream.read(reinterpret_cast<char*>(&wet2), sizeof(wet2) );
	stream.read(reinterpret_cast<char*>(&dry), sizeof(dry) );
	stream.read(reinterpret_cast<char*>(&width), sizeof(width) );
	stream.read(reinterpret_cast<char*>(&mode), sizeof(mode) );
	stream.read(reinterpret_cast<char*>(&filtval), sizeof(filtval) );
	stream.read(reinterpret_cast<char*>(&filtprev1), sizeof(filtprev1) );
	stream.read(reinterpret_cast<char*>(&filtprev2), sizeof(filtprev2) );


	for( int lcv=0; lcv<numcombs; lcv++ ) {
		combL[lcv].loadState(stream);
		combR[lcv].loadState(stream);
	}

	for( int lcv=0; lcv<numallpasses; lcv++ ) {
		allpassL[lcv].loadState(stream);
		allpassR[lcv].loadState(stream);
	}
}
