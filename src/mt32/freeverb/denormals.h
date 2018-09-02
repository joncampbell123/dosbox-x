// Macro for killing denormalled numbers
//
// Written by Jezar at Dreampoint, June 2000
// http://www.dreampoint.co.uk
// Based on IS_DENORMAL macro by Jon Watte
// This code is public domain

#ifndef _denormals_
#define _denormals_

static inline float undenormalise(float x) {
	union {
		float f;
		unsigned int i;
	} u;
	u.f = x;
	if ((u.i & 0x7f800000) == 0) {
		return 0.0f;
	}
	return x;
}

#endif//_denormals_
