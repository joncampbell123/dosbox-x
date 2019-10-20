// Comb filter class declaration
//
// Written by Jezar at Dreampoint, June 2000
// http://www.dreampoint.co.uk
// This code is public domain

#ifndef _comb_
#define _comb_

#include "denormals.h"
#include "../FileStream.h"

class comb
{
public:
	                comb();
	        void    setbuffer(float *buf, int size);
	        void    deletebuffer();
	inline  float   process(float input);
	        void    mute();
	        void    setdamp(float val);
	        float   getdamp();
	        void    setfeedback(float val);
	        float   getfeedback();

					void saveState( std::ostream &stream );
					void loadState( std::istream &stream );
private:
	float   feedback = 0;
	float   filterstore;
	float   damp1 = 0;
	float   damp2 = 0;
	float   *buffer = NULL;
	int     bufsize = 0;
	int     bufidx;
};


// Big to inline - but crucial for speed

inline float comb::process(float input)
{
	float output;

	output = undenormalise(buffer[bufidx]);

	filterstore = undenormalise((output*damp2) + (filterstore*damp1));

	buffer[bufidx] = input + (filterstore*feedback);

	if (++bufidx>=bufsize) bufidx = 0;

	return output;
}

#endif //_comb_
