/*
 *  Copyright (C) 2002-2021  The DOSBox Team
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


#define GetEAa												\
	PhysPt eaa=EALookupTable[rm]();					

#define GetRMEAa											\
	GetRM;													\
	GetEAa;											


#define RMEbGb(inst)														\
	{																		\
		GetRMrb;															\
		if (rm >= 0xc0 ) {GetEArb;inst(*earb,*rmrb,LoadRb,SaveRb);}			\
		else {GetEAa;inst(eaa,*rmrb,LoadMb,SaveMb);}						\
	}

#define RMGbEb(inst)														\
	{																		\
		GetRMrb;															\
		if (rm >= 0xc0 ) {GetEArb;inst(*rmrb,*earb,LoadRb,SaveRb);}			\
		else {GetEAa;inst(*rmrb,LoadMb(eaa),LoadRb,SaveRb);}				\
	}

#define RMEb(inst)															\
	{																		\
		if (rm >= 0xc0 ) {GetEArb;inst(*earb,LoadRb,SaveRb);}				\
		else {GetEAa;inst(eaa,LoadMb,SaveMb);}								\
	}

#define RMEwGw(inst)														\
	{																		\
		GetRMrw;															\
		if (rm >= 0xc0 ) {GetEArw;inst(*earw,*rmrw,LoadRw,SaveRw);}			\
		else {GetEAa;inst(eaa,*rmrw,LoadMw,SaveMw);}						\
	}

#define RMEwGwOp3(inst,op3)													\
	{																		\
		GetRMrw;															\
		if (rm >= 0xc0 ) {GetEArw;inst(*earw,*rmrw,op3,LoadRw,SaveRw);}		\
		else {GetEAa;inst(eaa,*rmrw,op3,LoadMw,SaveMw);}					\
	}

#define RMGwEw(inst)														\
	{																		\
		GetRMrw;															\
		if (rm >= 0xc0 ) {GetEArw;inst(*rmrw,*earw,LoadRw,SaveRw);}			\
		else {GetEAa;inst(*rmrw,LoadMw(eaa),LoadRw,SaveRw);}				\
	}																

#define RMGwEwOp3(inst,op3)													\
	{																		\
		GetRMrw;															\
		if (rm >= 0xc0 ) {GetEArw;inst(*rmrw,*earw,op3,LoadRw,SaveRw);}		\
		else {GetEAa;inst(*rmrw,LoadMw(eaa),op3,LoadRw,SaveRw);}			\
	}																

#define RMEw(inst)															\
	{																		\
		if (rm >= 0xc0 ) {GetEArw;inst(*earw,LoadRw,SaveRw);}				\
		else {GetEAa;inst(eaa,LoadMw,SaveMw);}								\
	}

#define RMEdGd(inst)														\
	{																		\
		GetRMrd;															\
		if (rm >= 0xc0 ) {GetEArd;inst(*eard,*rmrd,LoadRd,SaveRd);}			\
		else {GetEAa;inst(eaa,*rmrd,LoadMd,SaveMd);}						\
	}

#define RMEdGdOp3(inst,op3)													\
	{																		\
		GetRMrd;															\
		if (rm >= 0xc0 ) {GetEArd;inst(*eard,*rmrd,op3,LoadRd,SaveRd);}		\
		else {GetEAa;inst(eaa,*rmrd,op3,LoadMd,SaveMd);}					\
	}


#define RMGdEd(inst)														\
	{																		\
		GetRMrd;															\
		if (rm >= 0xc0 ) {GetEArd;inst(*rmrd,*eard,LoadRd,SaveRd);}			\
		else {GetEAa;inst(*rmrd,LoadMd(eaa),LoadRd,SaveRd);}				\
	}																

#define RMGdEdOp3(inst,op3)													\
	{																		\
		GetRMrd;															\
		if (rm >= 0xc0 ) {GetEArd;inst(*rmrd,*eard,op3,LoadRd,SaveRd);}		\
		else {GetEAa;inst(*rmrd,LoadMd(eaa),op3,LoadRd,SaveRd);}			\
	}																




#define RMEw(inst)															\
	{																		\
		if (rm >= 0xc0 ) {GetEArw;inst(*earw,LoadRw,SaveRw);}				\
		else {GetEAa;inst(eaa,LoadMw,SaveMw);}								\
	}

#define RMEd(inst)															\
	{																		\
		if (rm >= 0xc0 ) {GetEArd;inst(*eard,LoadRd,SaveRd);}				\
		else {GetEAa;inst(eaa,LoadMd,SaveMd);}								\
	}

#define ALIb(inst)															\
	{ inst(reg_al,Fetchb(),LoadRb,SaveRb)}

#define AXIw(inst)															\
	{ inst(reg_ax,Fetchw(),LoadRw,SaveRw);}

#define EAXId(inst)															\
	{ inst(reg_eax,Fetchd(),LoadRd,SaveRd);}

#define FPU_ESC(code) {														\
	uint8_t rm=Fetchb();														\
	if (rm >= 0xc0) {															\
		FPU_ESC ## code ## _Normal(rm);										\
	} else {																\
		GetEAa;FPU_ESC ## code ## _EA(rm,eaa);								\
	}																		\
}

#define CASE_W(_WHICH)							\
	case (OPCODE_NONE+_WHICH):

#if CPU_CORE >= CPU_ARCHTYPE_386
# define CASE_D(_WHICH)							\
	case (OPCODE_SIZE+_WHICH):
#else
# define CASE_D(_WHICH)
#endif

#define CASE_B(_WHICH)							\
	CASE_W(_WHICH)								\
	CASE_D(_WHICH)

#define CASE_0F_W(_WHICH)						\
	case ((OPCODE_0F|OPCODE_NONE)+_WHICH):

#if CPU_CORE >= CPU_ARCHTYPE_386
# define CASE_0F_D(_WHICH)						\
	case ((OPCODE_0F|OPCODE_SIZE)+_WHICH):
#else
# define CASE_0F_D(_WHICH)
#endif

#define CASE_0F_B(_WHICH)						\
	CASE_0F_W(_WHICH)							\
	CASE_0F_D(_WHICH)

#define FixEA16 do {							\
		switch (rm & 7) {						\
			case 6:	if (rm < 0x40) break;		\
			case 2:								\
			case 3:								\
				BaseDS=BaseSS;					\
		}										\
		eaa=BaseDS+(uint16_t)(eaa-BaseDS);        \
	} while(0)
