/*
 *  Copyright (C) 2002-2020  The DOSBox Team
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
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */


#include "dosbox.h"

#include "mem.h"
#include "mmx.h"
#include "cpu.h"
#include "fpu.h"

#if C_FPU

MMX_reg *reg_mmx[8] = {
#if defined(HAS_LONG_DOUBLE)
	&fpu._do_not_use__regs[0].reg_mmx,
	&fpu._do_not_use__regs[1].reg_mmx,
	&fpu._do_not_use__regs[2].reg_mmx,
	&fpu._do_not_use__regs[3].reg_mmx,
	&fpu._do_not_use__regs[4].reg_mmx,
	&fpu._do_not_use__regs[5].reg_mmx,
	&fpu._do_not_use__regs[6].reg_mmx,
	&fpu._do_not_use__regs[7].reg_mmx,
#else
	&fpu.regs[0].reg_mmx,
	&fpu.regs[1].reg_mmx,
	&fpu.regs[2].reg_mmx,
	&fpu.regs[3].reg_mmx,
	&fpu.regs[4].reg_mmx,
	&fpu.regs[5].reg_mmx,
	&fpu.regs[6].reg_mmx,
	&fpu.regs[7].reg_mmx,
#endif
};

MMX_reg * lookupRMregMM[256]={
	reg_mmx[0],reg_mmx[0],reg_mmx[0],reg_mmx[0],reg_mmx[0],reg_mmx[0],reg_mmx[0],reg_mmx[0],
	reg_mmx[1],reg_mmx[1],reg_mmx[1],reg_mmx[1],reg_mmx[1],reg_mmx[1],reg_mmx[1],reg_mmx[1],
	reg_mmx[2],reg_mmx[2],reg_mmx[2],reg_mmx[2],reg_mmx[2],reg_mmx[2],reg_mmx[2],reg_mmx[2],
	reg_mmx[3],reg_mmx[3],reg_mmx[3],reg_mmx[3],reg_mmx[3],reg_mmx[3],reg_mmx[3],reg_mmx[3],
	reg_mmx[4],reg_mmx[4],reg_mmx[4],reg_mmx[4],reg_mmx[4],reg_mmx[4],reg_mmx[4],reg_mmx[4],
	reg_mmx[5],reg_mmx[5],reg_mmx[5],reg_mmx[5],reg_mmx[5],reg_mmx[5],reg_mmx[5],reg_mmx[5],
	reg_mmx[6],reg_mmx[6],reg_mmx[6],reg_mmx[6],reg_mmx[6],reg_mmx[6],reg_mmx[6],reg_mmx[6],
	reg_mmx[7],reg_mmx[7],reg_mmx[7],reg_mmx[7],reg_mmx[7],reg_mmx[7],reg_mmx[7],reg_mmx[7],

	reg_mmx[0],reg_mmx[0],reg_mmx[0],reg_mmx[0],reg_mmx[0],reg_mmx[0],reg_mmx[0],reg_mmx[0],
	reg_mmx[1],reg_mmx[1],reg_mmx[1],reg_mmx[1],reg_mmx[1],reg_mmx[1],reg_mmx[1],reg_mmx[1],
	reg_mmx[2],reg_mmx[2],reg_mmx[2],reg_mmx[2],reg_mmx[2],reg_mmx[2],reg_mmx[2],reg_mmx[2],
	reg_mmx[3],reg_mmx[3],reg_mmx[3],reg_mmx[3],reg_mmx[3],reg_mmx[3],reg_mmx[3],reg_mmx[3],
	reg_mmx[4],reg_mmx[4],reg_mmx[4],reg_mmx[4],reg_mmx[4],reg_mmx[4],reg_mmx[4],reg_mmx[4],
	reg_mmx[5],reg_mmx[5],reg_mmx[5],reg_mmx[5],reg_mmx[5],reg_mmx[5],reg_mmx[5],reg_mmx[5],
	reg_mmx[6],reg_mmx[6],reg_mmx[6],reg_mmx[6],reg_mmx[6],reg_mmx[6],reg_mmx[6],reg_mmx[6],
	reg_mmx[7],reg_mmx[7],reg_mmx[7],reg_mmx[7],reg_mmx[7],reg_mmx[7],reg_mmx[7],reg_mmx[7],

	reg_mmx[0],reg_mmx[0],reg_mmx[0],reg_mmx[0],reg_mmx[0],reg_mmx[0],reg_mmx[0],reg_mmx[0],
	reg_mmx[1],reg_mmx[1],reg_mmx[1],reg_mmx[1],reg_mmx[1],reg_mmx[1],reg_mmx[1],reg_mmx[1],
	reg_mmx[2],reg_mmx[2],reg_mmx[2],reg_mmx[2],reg_mmx[2],reg_mmx[2],reg_mmx[2],reg_mmx[2],
	reg_mmx[3],reg_mmx[3],reg_mmx[3],reg_mmx[3],reg_mmx[3],reg_mmx[3],reg_mmx[3],reg_mmx[3],
	reg_mmx[4],reg_mmx[4],reg_mmx[4],reg_mmx[4],reg_mmx[4],reg_mmx[4],reg_mmx[4],reg_mmx[4],
	reg_mmx[5],reg_mmx[5],reg_mmx[5],reg_mmx[5],reg_mmx[5],reg_mmx[5],reg_mmx[5],reg_mmx[5],
	reg_mmx[6],reg_mmx[6],reg_mmx[6],reg_mmx[6],reg_mmx[6],reg_mmx[6],reg_mmx[6],reg_mmx[6],
	reg_mmx[7],reg_mmx[7],reg_mmx[7],reg_mmx[7],reg_mmx[7],reg_mmx[7],reg_mmx[7],reg_mmx[7],

	reg_mmx[0],reg_mmx[0],reg_mmx[0],reg_mmx[0],reg_mmx[0],reg_mmx[0],reg_mmx[0],reg_mmx[0],
	reg_mmx[1],reg_mmx[1],reg_mmx[1],reg_mmx[1],reg_mmx[1],reg_mmx[1],reg_mmx[1],reg_mmx[1],
	reg_mmx[2],reg_mmx[2],reg_mmx[2],reg_mmx[2],reg_mmx[2],reg_mmx[2],reg_mmx[2],reg_mmx[2],
	reg_mmx[3],reg_mmx[3],reg_mmx[3],reg_mmx[3],reg_mmx[3],reg_mmx[3],reg_mmx[3],reg_mmx[3],
	reg_mmx[4],reg_mmx[4],reg_mmx[4],reg_mmx[4],reg_mmx[4],reg_mmx[4],reg_mmx[4],reg_mmx[4],
	reg_mmx[5],reg_mmx[5],reg_mmx[5],reg_mmx[5],reg_mmx[5],reg_mmx[5],reg_mmx[5],reg_mmx[5],
	reg_mmx[6],reg_mmx[6],reg_mmx[6],reg_mmx[6],reg_mmx[6],reg_mmx[6],reg_mmx[6],reg_mmx[6],
	reg_mmx[7],reg_mmx[7],reg_mmx[7],reg_mmx[7],reg_mmx[7],reg_mmx[7],reg_mmx[7],reg_mmx[7],
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

uint8_t SaturateWordSToByteU(Bit16s value)
{
  if(value < 0) return 0;
  if(value > 255) return 255;
  return (uint8_t) value;
}

Bit16u SaturateDwordSToWordU(Bit32s value)
{
  if(value < 0) return 0;
  if(value > 65535) return 65535;
  return (Bit16u) value;
}

void setFPUTagEmpty() {
	FPU_SetCW(0x37F);
	fpu.sw = 0;
	TOP = FPU_GET_TOP();
	fpu.tags[0] = TAG_Empty;
	fpu.tags[1] = TAG_Empty;
	fpu.tags[2] = TAG_Empty;
	fpu.tags[3] = TAG_Empty;
	fpu.tags[4] = TAG_Empty;
	fpu.tags[5] = TAG_Empty;
	fpu.tags[6] = TAG_Empty;
	fpu.tags[7] = TAG_Empty;
	fpu.tags[8] = TAG_Valid; // is only used by us
}

#endif

