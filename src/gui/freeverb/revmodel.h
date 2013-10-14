// Reverb model declaration
//
// Written by Jezar at Dreampoint, June 2000
// Modifications by Jerome Fisher, 2009
// http://www.dreampoint.co.uk
// This code is public domain

#ifndef _revmodel_
#define _revmodel_

#include "comb.h"
#include "allpass.h"
#include "tuning.h"
#include "../FileStream.h"

class revmodel
{
public:
			       revmodel(float scaletuning);
						 ~revmodel();
			void   mute();
			void   process(const float *inputL, const float *inputR, float *outputL, float *outputR, long numsamples);
			void   setroomsize(float value);
			float  getroomsize();
			void   setdamp(float value);
			float  getdamp();
			void   setwet(float value);
			float  getwet();
			void   setdry(float value);
			float  getdry();
			void   setwidth(float value);
			float  getwidth();
			void   setmode(float value);
			float  getmode();
			void   setfiltval(float value);

			void saveState( std::ostream &stream );
			void loadState( std::istream &stream );
private:
			void   update();
private:
	float  gain;
	float  roomsize,roomsize1;
	float  damp,damp1;
	float  wet,wet1,wet2;
	float  dry;
	float  width;
	float  mode;

	// LPF stuff
	float filtval;
	float filtprev1;
	float filtprev2;

	// Comb filters
	comb   combL[numcombs];
	comb   combR[numcombs];

	// Allpass filters
	allpass	allpassL[numallpasses];
	allpass	allpassR[numallpasses];
};

#endif//_revmodel_
