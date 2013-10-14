/*
 *  Copyright (C) 2002-2013  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */


#include "dosbox.h"

#include "mem.h"
#include "mmx.h"
#include "cpu.h"
#include "fpu.h"
#include "../save_state.h"

MMX_reg reg_mmx[8];


MMX_reg * lookupRMregMM[256]={
	&reg_mmx[0],&reg_mmx[0],&reg_mmx[0],&reg_mmx[0],&reg_mmx[0],&reg_mmx[0],&reg_mmx[0],&reg_mmx[0],
	&reg_mmx[1],&reg_mmx[1],&reg_mmx[1],&reg_mmx[1],&reg_mmx[1],&reg_mmx[1],&reg_mmx[1],&reg_mmx[1],
	&reg_mmx[2],&reg_mmx[2],&reg_mmx[2],&reg_mmx[2],&reg_mmx[2],&reg_mmx[2],&reg_mmx[2],&reg_mmx[2],
	&reg_mmx[3],&reg_mmx[3],&reg_mmx[3],&reg_mmx[3],&reg_mmx[3],&reg_mmx[3],&reg_mmx[3],&reg_mmx[3],
	&reg_mmx[4],&reg_mmx[4],&reg_mmx[4],&reg_mmx[4],&reg_mmx[4],&reg_mmx[4],&reg_mmx[4],&reg_mmx[4],
	&reg_mmx[5],&reg_mmx[5],&reg_mmx[5],&reg_mmx[5],&reg_mmx[5],&reg_mmx[5],&reg_mmx[5],&reg_mmx[5],
	&reg_mmx[6],&reg_mmx[6],&reg_mmx[6],&reg_mmx[6],&reg_mmx[6],&reg_mmx[6],&reg_mmx[6],&reg_mmx[6],
	&reg_mmx[7],&reg_mmx[7],&reg_mmx[7],&reg_mmx[7],&reg_mmx[7],&reg_mmx[7],&reg_mmx[7],&reg_mmx[7],

	&reg_mmx[0],&reg_mmx[0],&reg_mmx[0],&reg_mmx[0],&reg_mmx[0],&reg_mmx[0],&reg_mmx[0],&reg_mmx[0],
	&reg_mmx[1],&reg_mmx[1],&reg_mmx[1],&reg_mmx[1],&reg_mmx[1],&reg_mmx[1],&reg_mmx[1],&reg_mmx[1],
	&reg_mmx[2],&reg_mmx[2],&reg_mmx[2],&reg_mmx[2],&reg_mmx[2],&reg_mmx[2],&reg_mmx[2],&reg_mmx[2],
	&reg_mmx[3],&reg_mmx[3],&reg_mmx[3],&reg_mmx[3],&reg_mmx[3],&reg_mmx[3],&reg_mmx[3],&reg_mmx[3],
	&reg_mmx[4],&reg_mmx[4],&reg_mmx[4],&reg_mmx[4],&reg_mmx[4],&reg_mmx[4],&reg_mmx[4],&reg_mmx[4],
	&reg_mmx[5],&reg_mmx[5],&reg_mmx[5],&reg_mmx[5],&reg_mmx[5],&reg_mmx[5],&reg_mmx[5],&reg_mmx[5],
	&reg_mmx[6],&reg_mmx[6],&reg_mmx[6],&reg_mmx[6],&reg_mmx[6],&reg_mmx[6],&reg_mmx[6],&reg_mmx[6],
	&reg_mmx[7],&reg_mmx[7],&reg_mmx[7],&reg_mmx[7],&reg_mmx[7],&reg_mmx[7],&reg_mmx[7],&reg_mmx[7],

	&reg_mmx[0],&reg_mmx[0],&reg_mmx[0],&reg_mmx[0],&reg_mmx[0],&reg_mmx[0],&reg_mmx[0],&reg_mmx[0],
	&reg_mmx[1],&reg_mmx[1],&reg_mmx[1],&reg_mmx[1],&reg_mmx[1],&reg_mmx[1],&reg_mmx[1],&reg_mmx[1],
	&reg_mmx[2],&reg_mmx[2],&reg_mmx[2],&reg_mmx[2],&reg_mmx[2],&reg_mmx[2],&reg_mmx[2],&reg_mmx[2],
	&reg_mmx[3],&reg_mmx[3],&reg_mmx[3],&reg_mmx[3],&reg_mmx[3],&reg_mmx[3],&reg_mmx[3],&reg_mmx[3],
	&reg_mmx[4],&reg_mmx[4],&reg_mmx[4],&reg_mmx[4],&reg_mmx[4],&reg_mmx[4],&reg_mmx[4],&reg_mmx[4],
	&reg_mmx[5],&reg_mmx[5],&reg_mmx[5],&reg_mmx[5],&reg_mmx[5],&reg_mmx[5],&reg_mmx[5],&reg_mmx[5],
	&reg_mmx[6],&reg_mmx[6],&reg_mmx[6],&reg_mmx[6],&reg_mmx[6],&reg_mmx[6],&reg_mmx[6],&reg_mmx[6],
	&reg_mmx[7],&reg_mmx[7],&reg_mmx[7],&reg_mmx[7],&reg_mmx[7],&reg_mmx[7],&reg_mmx[7],&reg_mmx[7],

	&reg_mmx[0],&reg_mmx[0],&reg_mmx[0],&reg_mmx[0],&reg_mmx[0],&reg_mmx[0],&reg_mmx[0],&reg_mmx[0],
	&reg_mmx[1],&reg_mmx[1],&reg_mmx[1],&reg_mmx[1],&reg_mmx[1],&reg_mmx[1],&reg_mmx[1],&reg_mmx[1],
	&reg_mmx[2],&reg_mmx[2],&reg_mmx[2],&reg_mmx[2],&reg_mmx[2],&reg_mmx[2],&reg_mmx[2],&reg_mmx[2],
	&reg_mmx[3],&reg_mmx[3],&reg_mmx[3],&reg_mmx[3],&reg_mmx[3],&reg_mmx[3],&reg_mmx[3],&reg_mmx[3],
	&reg_mmx[4],&reg_mmx[4],&reg_mmx[4],&reg_mmx[4],&reg_mmx[4],&reg_mmx[4],&reg_mmx[4],&reg_mmx[4],
	&reg_mmx[5],&reg_mmx[5],&reg_mmx[5],&reg_mmx[5],&reg_mmx[5],&reg_mmx[5],&reg_mmx[5],&reg_mmx[5],
	&reg_mmx[6],&reg_mmx[6],&reg_mmx[6],&reg_mmx[6],&reg_mmx[6],&reg_mmx[6],&reg_mmx[6],&reg_mmx[6],
	&reg_mmx[7],&reg_mmx[7],&reg_mmx[7],&reg_mmx[7],&reg_mmx[7],&reg_mmx[7],&reg_mmx[7],&reg_mmx[7],
};


Bit8s SaturateWordSToByteS(Bit16s value)
{
  if(value < -128) return -128;
  if(value >  127) return  127;
  return (Bit8s) value;
}

Bit16s SaturateDwordSToWordS(Bit32s value)
{
  if(value < -32768) return -32768;
  if(value >  32767) return  32767;
  return (Bit16s) value;
}

Bit8u SaturateWordSToByteU(Bit16s value)
{
  if(value < 0) return 0;
  if(value > 255) return 255;
  return (Bit8u) value;
}

Bit16u SaturateDwordSToWordU(Bit32s value)
{
  if(value < 0) return 0;
  if(value > 65535) return 65535;
  return (Bit16u) value;
}

void setFPU(Bit16u tag) {
	FPU_SET_TOP(0);
	TOP=FPU_GET_TOP();
	FPU_SetTag(tag);
}



// save state support
void POD_Save_CPU_MMX( std::ostream& stream )
{
	// - pure data
	WRITE_POD( &reg_mmx, reg_mmx );
}


void POD_Load_CPU_MMX( std::istream& stream )
{
	// - pure data
	READ_POD( &reg_mmx, reg_mmx );
}


/*
ykhwong svn-daum 2012-02-20


static globals:


struct MMX_reg reg_mmx[8];
	// - pure data
	typedef union {

		Bit64u q;

	#ifndef WORDS_BIGENDIAN
		struct {
			Bit32u d0,d1;
		} ud;

		struct {
			Bit32s d0,d1;
		} sd;

		struct {
			Bit16u w0,w1,w2,w3;
		} uw;

		struct {
			Bit16s w0,w1,w2,w3;
		} sw;

		struct {
			Bit8u b0,b1,b2,b3,b4,b5,b6,b7;
		} ub;

		struct {
			Bit8s b0,b1,b2,b3,b4,b5,b6,b7;
		} sb;
*/
