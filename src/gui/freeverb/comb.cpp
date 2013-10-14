// Comb filter implementation
//
// Written by Jezar at Dreampoint, June 2000
// http://www.dreampoint.co.uk
// This code is public domain

#include "comb.h"

comb::comb()
{
	filterstore = 0;
	bufidx = 0;
}

void comb::setbuffer(float *buf, int size)
{
	buffer = buf;
	bufsize = size;
}

void comb::mute()
{
	for (int i=0; i<bufsize; i++)
		buffer[i]=0;
}

void comb::setdamp(float val)
{
	damp1 = val;
	damp2 = 1-val;
}

float comb::getdamp()
{
	return damp1;
}

void comb::setfeedback(float val)
{
	feedback = val;
}

float comb::getfeedback()
{
	return feedback;
}

void comb::deletebuffer()
{
	delete[] buffer;
	buffer = 0;
}


void comb::saveState( std::ostream &stream)
{
	stream.write(reinterpret_cast<const char*>(&feedback), sizeof(feedback) );
	stream.write(reinterpret_cast<const char*>(&filterstore), sizeof(filterstore) );
	stream.write(reinterpret_cast<const char*>(&damp1), sizeof(damp1) );
	stream.write(reinterpret_cast<const char*>(&damp2), sizeof(damp2) );
	stream.write(reinterpret_cast<const char*>(buffer), bufsize * sizeof(float) );
	stream.write(reinterpret_cast<const char*>(&bufsize), sizeof(bufsize) );
	stream.write(reinterpret_cast<const char*>(&bufidx), sizeof(bufidx) );
}


void comb::loadState( std::istream &stream)
{
	stream.read(reinterpret_cast<char*>(&feedback), sizeof(feedback) );
	stream.read(reinterpret_cast<char*>(&filterstore), sizeof(filterstore) );
	stream.read(reinterpret_cast<char*>(&damp1), sizeof(damp1) );
	stream.read(reinterpret_cast<char*>(&damp2), sizeof(damp2) );
	stream.read(reinterpret_cast<char*>(buffer), bufsize * sizeof(float) );
	stream.read(reinterpret_cast<char*>(&bufsize), sizeof(bufsize) );
	stream.read(reinterpret_cast<char*>(&bufidx), sizeof(bufidx) );
}
