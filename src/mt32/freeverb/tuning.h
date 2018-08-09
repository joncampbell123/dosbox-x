// Reverb model tuning values
//
// Written by Jezar at Dreampoint, June 2000
// http://www.dreampoint.co.uk
// This code is public domain

#ifndef _tuning_
#define _tuning_

const int   numcombs        = 8;
const int   numallpasses    = 4;
const float muted           = 0;
const float fixedgain       = 0.015f;
const float scalewet        = 3;
const float scaledry        = 2;
const float scaledamp       = 0.4f;
const float scaleroom       = 0.28f;
const float offsetroom      = 0.7f;
const float initialroom     = 0.5f;
const float initialdamp     = 0.5f;
const float initialwet      = 1/scalewet;
const float initialdry      = 0;
const float initialwidth    = 1;
const float initialmode     = 0;
const float freezemode      = 0.5f;
const int   stereospread    = 23;

const int combtuning[]      = {1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617};
const int allpasstuning[]   = {556, 441, 341, 225};

#endif//_tuning_
