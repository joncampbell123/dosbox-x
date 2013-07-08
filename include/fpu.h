/*
 *  Copyright (C) 2002-2010  The DOSBox Team
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

#ifndef DOSBOX_FPU_H
#define DOSBOX_FPU_H

#ifndef DOSBOX_MEM_H
#include "mem.h"
#endif

void FPU_ESC0_Normal(Bitu rm);
void FPU_ESC0_EA(Bitu func,PhysPt ea);
void FPU_ESC1_Normal(Bitu rm);
void FPU_ESC1_EA(Bitu func,PhysPt ea);
void FPU_ESC2_Normal(Bitu rm);
void FPU_ESC2_EA(Bitu func,PhysPt ea);
void FPU_ESC3_Normal(Bitu rm);
void FPU_ESC3_EA(Bitu func,PhysPt ea);
void FPU_ESC4_Normal(Bitu rm);
void FPU_ESC4_EA(Bitu func,PhysPt ea);
void FPU_ESC5_Normal(Bitu rm);
void FPU_ESC5_EA(Bitu func,PhysPt ea);
void FPU_ESC6_Normal(Bitu rm);
void FPU_ESC6_EA(Bitu func,PhysPt ea);
void FPU_ESC7_Normal(Bitu rm);
void FPU_ESC7_EA(Bitu func,PhysPt ea);


typedef union {
    double d;
#ifndef WORDS_BIGENDIAN
    struct {
        Bit32u lower;
        Bit32s upper;
    } l;
#else
    struct {
        Bit32s upper;
        Bit32u lower;
    } l;
#endif
    Bit64s ll;
} FPU_Reg;

typedef struct {
    Bit32u m1;
    Bit32u m2;
    Bit16u m3;

    Bit16u d1;
    Bit32u d2;
} FPU_P_Reg;

enum FPU_Tag {
	TAG_Valid = 0,
	TAG_Zero  = 1,
	TAG_Weird = 2,
	TAG_Empty = 3
};

enum FPU_Round {
	ROUND_Nearest = 0,		
	ROUND_Down    = 1,
	ROUND_Up      = 2,	
	ROUND_Chop    = 3
};

typedef struct {
	FPU_Reg		regs[9];
	FPU_P_Reg	p_regs[9];
	FPU_Tag		tags[9];
	Bit16u		cw,cw_mask_all;
	Bit16u		sw;
	Bitu		top;
	FPU_Round	round;
} FPU_rec;


//get pi from a real library
#define PI		3.14159265358979323846
#define L2E		1.4426950408889634
#define L2T		3.3219280948873623
#define LN2		0.69314718055994531
#define LG2		0.3010299956639812


extern FPU_rec fpu;

#define TOP fpu.top
#define STV(i)  ( (fpu.top+ (i) ) & 7 )


Bit16u FPU_GetTag(void);
void FPU_FLDCW(PhysPt addr);

static INLINE void FPU_SetTag(Bit16u tag){
	for(Bitu i=0;i<8;i++)
		fpu.tags[i] = static_cast<FPU_Tag>((tag >>(2*i))&3);
}

static INLINE void FPU_SetCW(Bitu word){
	fpu.cw = (Bit16u)word;
	fpu.cw_mask_all = (Bit16u)(word | 0x3f);
	fpu.round = (FPU_Round)((word >> 10) & 3);
}


static INLINE Bitu FPU_GET_TOP(void) {
	return (fpu.sw & 0x3800)>>11;
}

static INLINE void FPU_SET_TOP(Bitu val){
	fpu.sw &= ~0x3800;
	fpu.sw |= (val&7)<<11;
}


static INLINE void FPU_SET_C0(Bitu C){
	fpu.sw &= ~0x0100;
	if(C) fpu.sw |=  0x0100;
}

static INLINE void FPU_SET_C1(Bitu C){
	fpu.sw &= ~0x0200;
	if(C) fpu.sw |=  0x0200;
}

static INLINE void FPU_SET_C2(Bitu C){
	fpu.sw &= ~0x0400;
	if(C) fpu.sw |=  0x0400;
}

static INLINE void FPU_SET_C3(Bitu C){
	fpu.sw &= ~0x4000;
	if(C) fpu.sw |= 0x4000;
}


#endif
