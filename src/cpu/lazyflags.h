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

#ifndef DOSBOX_LAZYFLAGS_H
#define DOSBOX_LAZYFLAGS_H

//Flag Handling
uint32_t get_CF(void);
uint32_t get_AF(void);
uint32_t get_ZF(void);
uint32_t get_SF(void);
uint32_t get_OF(void);
uint32_t get_PF(void);

Bitu FillFlags(void);
void FillFlagsNoCFOF(void);
void DestroyConditionFlags(void);

#ifndef DOSBOX_REGS_H
#include "regs.h"
#endif

struct LazyFlags {
    GenReg32 var1,var2,res;
	Bitu type;
	Bitu prev_type;
	uint8_t oldcf;
};

extern LazyFlags lfags;

#define lf_var1b lflags.var1.byte[BL_INDEX]
#define lf_var2b lflags.var2.byte[BL_INDEX]
#define lf_resb lflags.res.byte[BL_INDEX]

#define lf_var1w lflags.var1.word[W_INDEX]
#define lf_var2w lflags.var2.word[W_INDEX]
#define lf_resw lflags.res.word[W_INDEX]

#define lf_var1d lflags.var1.dword[DW_INDEX]
#define lf_var2d lflags.var2.dword[DW_INDEX]
#define lf_resd lflags.res.dword[DW_INDEX]


extern LazyFlags lflags;

#define SETFLAGSb(FLAGB)													\
{																			\
	SETFLAGBIT(OF,get_OF());												\
	lflags.type=t_UNKNOWN;													\
	CPU_SetFlags(FLAGB,FMASK_NORMAL & 0xff);								\
}

#define SETFLAGSw(FLAGW)													\
{																			\
	lflags.type=t_UNKNOWN;													\
	CPU_SetFlagsw(FLAGW);													\
}

#define SETFLAGSd(FLAGD)													\
{																			\
	lflags.type=t_UNKNOWN;													\
	CPU_SetFlagsd(FLAGD);													\
}

#define LoadCF SETFLAGBIT(CF,get_CF());
#define LoadZF SETFLAGBIT(ZF,get_ZF());
#define LoadSF SETFLAGBIT(SF,get_SF());
#define LoadOF SETFLAGBIT(OF,get_OF());
#define LoadAF SETFLAGBIT(AF,get_AF());

#define TFLG_O		(get_OF())
#define TFLG_NO		(!get_OF())
#define TFLG_B		(get_CF())
#define TFLG_NB		(!get_CF())
#define TFLG_Z		(get_ZF())
#define TFLG_NZ		(!get_ZF())
#define TFLG_BE		(get_CF() || get_ZF())
#define TFLG_NBE	(!get_CF() && !get_ZF())
#define TFLG_S		(get_SF())
#define TFLG_NS		(!get_SF())
#define TFLG_P		(get_PF())
#define TFLG_NP		(!get_PF())
#define TFLG_L		((get_SF()!=0) != (get_OF()!=0))
#define TFLG_NL		((get_SF()!=0) == (get_OF()!=0))
#define TFLG_LE		(get_ZF()  || ((get_SF()!=0) != (get_OF()!=0)))
#define TFLG_NLE	(!get_ZF() && ((get_SF()!=0) == (get_OF()!=0)))

//Types of Flag changing instructions
enum {
	t_UNKNOWN=0,
	t_ADDb,t_ADDw,t_ADDd, 
	t_ORb,t_ORw,t_ORd, 
	t_ADCb,t_ADCw,t_ADCd,
	t_SBBb,t_SBBw,t_SBBd,
	t_ANDb,t_ANDw,t_ANDd,
	t_SUBb,t_SUBw,t_SUBd,
	t_XORb,t_XORw,t_XORd,
	t_CMPb,t_CMPw,t_CMPd,
	t_INCb,t_INCw,t_INCd,
	t_DECb,t_DECw,t_DECd,
	t_TESTb,t_TESTw,t_TESTd,
	t_SHLb,t_SHLw,t_SHLd,
	t_SHRb,t_SHRw,t_SHRd,
	t_SARb,t_SARw,t_SARd,
	t_ROLb,t_ROLw,t_ROLd,
	t_RORb,t_RORw,t_RORd,
	t_RCLb,t_RCLw,t_RCLd,
	t_RCRb,t_RCRw,t_RCRd,
	t_NEGb,t_NEGw,t_NEGd,
	
	t_DSHLw,t_DSHLd,
	t_DSHRw,t_DSHRd,
	t_MUL,t_DIV,
	t_NOTDONE,
	t_LASTFLAG
};

#endif
