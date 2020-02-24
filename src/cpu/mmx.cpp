/*
 *  Copyright (C) 2002-2019  The DOSBox Team
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335, USA.
 */


#include "dosbox.h"

#include "mem.h"
#include "mmx.h"
#include "cpu.h"
#include "fpu.h"

#if C_FPU

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

#endif

