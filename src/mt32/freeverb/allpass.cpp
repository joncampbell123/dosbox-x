// Allpass filter implementation
//
// Written by Jezar at Dreampoint, June 2000
// http://www.dreampoint.co.uk
// This code is public domain

#include "allpass.h"

allpass::allpass()
{
    bufidx = 0;
    buffer = NULL;
    bufsize = 0;
    feedback = 0.0F;
}

void allpass::setbuffer(float *buf, int size)
{
	buffer = buf;
	bufsize = size;
}

void allpass::mute()
{
	for (int i=0; i<bufsize; i++)
		buffer[i]=0;
}

void allpass::setfeedback(float val)
{
	feedback = val;
}

float allpass::getfeedback()
{
	return feedback;
}

void allpass::deletebuffer()
{
	delete[] buffer;
	buffer = 0;
}


void allpass::saveState( std::ostream &stream )
{
	stream.write(reinterpret_cast<const char*>(&feedback), sizeof(feedback) );
	stream.write(reinterpret_cast<const char*>(buffer), (std::streamsize)((size_t)bufsize * sizeof(float)) );
	stream.write(reinterpret_cast<const char*>(&bufsize), sizeof(bufsize) );
	stream.write(reinterpret_cast<const char*>(&bufidx), sizeof(bufidx) );
}


void allpass::loadState( std::istream &stream )
{
	stream.read(reinterpret_cast<char*>(&feedback), sizeof(feedback) );
	stream.read(reinterpret_cast<char*>(buffer), (std::streamsize)((size_t)bufsize * sizeof(float)) );
	stream.read(reinterpret_cast<char*>(&bufsize), sizeof(bufsize) );
	stream.read(reinterpret_cast<char*>(&bufidx), sizeof(bufidx) );
}
