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

/* Jumps */

extern bool enable_fpu;

/* All Byte general instructions */
#define ADDB(op1,op2,load,save)								\
	lf_var1b=load(op1);lf_var2b=op2;					\
	lf_resb=lf_var1b+lf_var2b;					\
	save(op1,lf_resb);								\
	lflags.type=t_ADDb;

#define ADCB(op1,op2,load,save)								\
	lflags.oldcf=get_CF()!=0;								\
	lf_var1b=load(op1);lf_var2b=op2;					\
	lf_resb=(unsigned int)lf_var1b+(unsigned int)lf_var2b+(unsigned int)lflags.oldcf;		\
	save(op1,lf_resb);								\
	lflags.type=t_ADCb;

#define SBBB(op1,op2,load,save)								\
	lflags.oldcf=get_CF()!=0;									\
	lf_var1b=load(op1);lf_var2b=op2;					\
	lf_resb=lf_var1b-(lf_var2b+lflags.oldcf);	\
	save(op1,lf_resb);								\
	lflags.type=t_SBBb;

#define SUBB(op1,op2,load,save)								\
	lf_var1b=load(op1);lf_var2b=op2;					\
	lf_resb=lf_var1b-lf_var2b;					\
	save(op1,lf_resb);								\
	lflags.type=t_SUBb;

#define ORB(op1,op2,load,save)								\
	lf_var1b=load(op1);lf_var2b=op2;					\
	lf_resb=lf_var1b | lf_var2b;				\
	save(op1,lf_resb);								\
	lflags.type=t_ORb;

#define XORB(op1,op2,load,save)								\
	lf_var1b=load(op1);lf_var2b=op2;					\
	lf_resb=lf_var1b ^ lf_var2b;				\
	save(op1,lf_resb);								\
	lflags.type=t_XORb;

#define ANDB(op1,op2,load,save)								\
	lf_var1b=load(op1);lf_var2b=op2;					\
	lf_resb=lf_var1b & lf_var2b;				\
	save(op1,lf_resb);								\
	lflags.type=t_ANDb;

#define CMPB(op1,op2,load,save)								\
	lf_var1b=load(op1);lf_var2b=op2;					\
	lf_resb=lf_var1b-lf_var2b;					\
	lflags.type=t_CMPb;

#define TESTB(op1,op2,load,save)							\
	lf_var1b=load(op1);lf_var2b=op2;					\
	lf_resb=lf_var1b & lf_var2b;				\
	lflags.type=t_TESTb;

/* All Word General instructions */

#define ADDW(op1,op2,load,save)								\
	lf_var1w=load(op1);lf_var2w=op2;					\
	lf_resw=lf_var1w+lf_var2w;					\
	save(op1,lf_resw);								\
	lflags.type=t_ADDw;

#define ADCW(op1,op2,load,save)								\
	lflags.oldcf=get_CF()!=0;									\
	lf_var1w=load(op1);lf_var2w=op2;					\
	lf_resw=(unsigned int)lf_var1w+(unsigned int)lf_var2w+(unsigned int)lflags.oldcf;		\
	save(op1,lf_resw);								\
	lflags.type=t_ADCw;

#define SBBW(op1,op2,load,save)								\
	lflags.oldcf=get_CF()!=0;									\
	lf_var1w=load(op1);lf_var2w=op2;					\
	lf_resw=lf_var1w-(lf_var2w+lflags.oldcf);	\
	save(op1,lf_resw);								\
	lflags.type=t_SBBw;

#define SUBW(op1,op2,load,save)								\
	lf_var1w=load(op1);lf_var2w=op2;					\
	lf_resw=lf_var1w-lf_var2w;					\
	save(op1,lf_resw);								\
	lflags.type=t_SUBw;

#define ORW(op1,op2,load,save)								\
	lf_var1w=load(op1);lf_var2w=op2;					\
	lf_resw=lf_var1w | lf_var2w;				\
	save(op1,lf_resw);								\
	lflags.type=t_ORw;

#define XORW(op1,op2,load,save)								\
	lf_var1w=load(op1);lf_var2w=op2;					\
	lf_resw=lf_var1w ^ lf_var2w;				\
	save(op1,lf_resw);								\
	lflags.type=t_XORw;

#define ANDW(op1,op2,load,save)								\
	lf_var1w=load(op1);lf_var2w=op2;					\
	lf_resw=lf_var1w & lf_var2w;				\
	save(op1,lf_resw);								\
	lflags.type=t_ANDw;

#define CMPW(op1,op2,load,save)								\
	lf_var1w=load(op1);lf_var2w=op2;					\
	lf_resw=lf_var1w-lf_var2w;					\
	lflags.type=t_CMPw;

#define TESTW(op1,op2,load,save)							\
	lf_var1w=load(op1);lf_var2w=op2;					\
	lf_resw=lf_var1w & lf_var2w;				\
	lflags.type=t_TESTw;

/* All DWORD General Instructions */

#define ADDD(op1,op2,load,save)								\
	lf_var1d=load(op1);lf_var2d=op2;					\
	lf_resd=lf_var1d+lf_var2d;					\
	save(op1,lf_resd);								\
	lflags.type=t_ADDd;

#define ADCD(op1,op2,load,save)								\
	lflags.oldcf=get_CF()!=0;									\
	lf_var1d=load(op1);lf_var2d=op2;					\
	lf_resd=lf_var1d+lf_var2d+lflags.oldcf;		\
	save(op1,lf_resd);								\
	lflags.type=t_ADCd;

#define SBBD(op1,op2,load,save)								\
	lflags.oldcf=get_CF()!=0;									\
	lf_var1d=load(op1);lf_var2d=op2;					\
	lf_resd=lf_var1d-(lf_var2d+lflags.oldcf);	\
	save(op1,lf_resd);								\
	lflags.type=t_SBBd;

#define SUBD(op1,op2,load,save)								\
	lf_var1d=load(op1);lf_var2d=op2;					\
	lf_resd=lf_var1d-lf_var2d;					\
	save(op1,lf_resd);								\
	lflags.type=t_SUBd;

#define ORD(op1,op2,load,save)								\
	lf_var1d=load(op1);lf_var2d=op2;					\
	lf_resd=lf_var1d | lf_var2d;				\
	save(op1,lf_resd);								\
	lflags.type=t_ORd;

#define XORD(op1,op2,load,save)								\
	lf_var1d=load(op1);lf_var2d=op2;					\
	lf_resd=lf_var1d ^ lf_var2d;				\
	save(op1,lf_resd);								\
	lflags.type=t_XORd;

#define ANDD(op1,op2,load,save)								\
	lf_var1d=load(op1);lf_var2d=op2;					\
	lf_resd=lf_var1d & lf_var2d;				\
	save(op1,lf_resd);								\
	lflags.type=t_ANDd;

#define CMPD(op1,op2,load,save)								\
	lf_var1d=load(op1);lf_var2d=op2;					\
	lf_resd=lf_var1d-lf_var2d;					\
	lflags.type=t_CMPd;


#define TESTD(op1,op2,load,save)							\
	lf_var1d=load(op1);lf_var2d=op2;					\
	lf_resd=lf_var1d & lf_var2d;				\
	lflags.type=t_TESTd;




#define INCB(op1,load,save)									\
	LoadCF;lf_var1b=load(op1);								\
	lf_resb=lf_var1b+1;										\
	save(op1,lf_resb);										\
	lflags.type=t_INCb;										\

#define INCW(op1,load,save)									\
	LoadCF;lf_var1w=load(op1);								\
	lf_resw=lf_var1w+1;										\
	save(op1,lf_resw);										\
	lflags.type=t_INCw;

#define INCD(op1,load,save)									\
	LoadCF;lf_var1d=load(op1);								\
	lf_resd=lf_var1d+1;										\
	save(op1,lf_resd);										\
	lflags.type=t_INCd;

#define DECB(op1,load,save)									\
	LoadCF;lf_var1b=load(op1);								\
	lf_resb=lf_var1b-1;										\
	save(op1,lf_resb);										\
	lflags.type=t_DECb;

#define DECW(op1,load,save)									\
	LoadCF;lf_var1w=load(op1);								\
	lf_resw=lf_var1w-1;										\
	save(op1,lf_resw);										\
	lflags.type=t_DECw;

#define DECD(op1,load,save)									\
	LoadCF;lf_var1d=load(op1);								\
	lf_resd=lf_var1d-1;										\
	save(op1,lf_resd);										\
	lflags.type=t_DECd;



#define ROLB(op1,op2,load,save)						\
	FillFlagsNoCFOF();								\
	lf_var1b=load(op1);								\
	lf_var2b=op2&0x07;								\
	lf_resb=(lf_var1b << lf_var2b) |				\
			(lf_var1b >> (8-lf_var2b));				\
	save(op1,lf_resb);								\
	SETFLAGBIT(CF,lf_resb & 1);						\
	SETFLAGBIT(OF,(lf_resb & 1) ^ (lf_resb >> 7));

#define ROLW(op1,op2,load,save)						\
	FillFlagsNoCFOF();								\
	lf_var1w=load(op1);								\
	lf_var2b=op2&0xf;								\
	lf_resw=(lf_var1w << lf_var2b) |				\
			(lf_var1w >> (16-lf_var2b));			\
	save(op1,lf_resw);								\
	SETFLAGBIT(CF,lf_resw & 1);						\
	SETFLAGBIT(OF,(lf_resw & 1) ^ (lf_resw >> 15));

#define ROLD(op1,op2,load,save)						\
	FillFlagsNoCFOF();								\
	lf_var1d=load(op1);								\
	lf_var2b=op2;									\
	lf_resd=(lf_var1d << lf_var2b) |				\
			(lf_var1d >> (32-lf_var2b));			\
	save(op1,lf_resd);								\
	SETFLAGBIT(CF,lf_resd & 1);						\
	SETFLAGBIT(OF,(lf_resd & 1) ^ (lf_resd >> 31));


#define RORB(op1,op2,load,save)						\
	FillFlagsNoCFOF();								\
	lf_var1b=load(op1);								\
	lf_var2b=op2&0x07;								\
	lf_resb=(lf_var1b >> lf_var2b) |				\
			(lf_var1b << (8-lf_var2b));				\
	save(op1,lf_resb);								\
	SETFLAGBIT(CF,lf_resb & 0x80);					\
	SETFLAGBIT(OF,(lf_resb ^ (lf_resb<<1)) & 0x80);

#define RORW(op1,op2,load,save)					\
	FillFlagsNoCFOF();							\
	lf_var1w=load(op1);							\
	lf_var2b=op2&0xf;							\
	lf_resw=(lf_var1w >> lf_var2b) |			\
			(lf_var1w << (16-lf_var2b));		\
	save(op1,lf_resw);							\
	SETFLAGBIT(CF,lf_resw & 0x8000);			\
	SETFLAGBIT(OF,(lf_resw ^ (lf_resw<<1)) & 0x8000);

#define RORD(op1,op2,load,save)					\
	FillFlagsNoCFOF();							\
	lf_var1d=load(op1);							\
	lf_var2b=op2;								\
	lf_resd=(lf_var1d >> lf_var2b) |			\
			(lf_var1d << (32-lf_var2b));		\
	save(op1,lf_resd);							\
	SETFLAGBIT(CF,lf_resd & 0x80000000);		\
	SETFLAGBIT(OF,(lf_resd ^ (lf_resd<<1)) & 0x80000000);


#define RCLB(op1,op2,load,save)							\
	if (!(op2%9)) break;								\
{	uint8_t cf=(uint8_t)FillFlags()&0x1;					\
	lf_var1b=load(op1);									\
	lf_var2b=op2%9;										\
	lf_resb=(lf_var1b << lf_var2b) |					\
			(cf << (lf_var2b-1)) |						\
			(lf_var1b >> (9-lf_var2b));					\
 	save(op1,lf_resb);									\
	SETFLAGBIT(CF,(((unsigned int)lf_var1b >> (8u-lf_var2b)) & 1u));	\
	SETFLAGBIT(OF,(reg_flags & 1u) ^ ((unsigned int)lf_resb >> 7u));	\
}

#define RCLW(op1,op2,load,save)							\
	if (!(op2%17)) break;								\
{	uint16_t cf=(uint16_t)FillFlags()&0x1;					\
	lf_var1w=load(op1);									\
	lf_var2b=op2%17;									\
	lf_resw=(lf_var1w << lf_var2b) |					\
			(cf << (lf_var2b-1)) |						\
			(lf_var1w >> (17-lf_var2b));				\
	save(op1,lf_resw);									\
	SETFLAGBIT(CF,(((unsigned int)lf_var1w >> (16u-lf_var2b)) & 1u));	\
	SETFLAGBIT(OF,(reg_flags & 1u) ^ ((unsigned int)lf_resw >> 15u));	\
}

#define RCLD(op1,op2,load,save)							\
	if (!op2) break;									\
{	Bit32u cf=(Bit32u)FillFlags()&0x1;					\
	lf_var1d=load(op1);									\
	lf_var2b=op2;										\
	if (lf_var2b==1)	{								\
		lf_resd=(lf_var1d << 1) | cf;					\
	} else 	{											\
		lf_resd=(lf_var1d << lf_var2b) |				\
		(cf << (lf_var2b-1)) |							\
		(lf_var1d >> (33-lf_var2b));					\
	}													\
	save(op1,lf_resd);									\
	SETFLAGBIT(CF,(((unsigned int)lf_var1d >> (32u-lf_var2b)) & 1u));	\
	SETFLAGBIT(OF,(reg_flags & 1u) ^ ((unsigned int)lf_resd >> 31u));	\
}

#define RCRB(op1,op2,load,save)								\
	if (op2%9) {											\
		uint8_t cf=(uint8_t)FillFlags()&0x1;					\
		lf_var1b=load(op1);									\
		lf_var2b=op2%9;										\
	 	lf_resb=(lf_var1b >> lf_var2b) |					\
				(cf << (8-lf_var2b)) |						\
				(lf_var1b << (9-lf_var2b));					\
		save(op1,lf_resb);									\
		SETFLAGBIT(CF,((unsigned int)lf_var1b >> (lf_var2b - 1u)) & 1u);	\
		SETFLAGBIT(OF,(lf_resb ^ ((unsigned int)lf_resb<<1u)) & 0x80u);		\
	}

#define RCRW(op1,op2,load,save)								\
	if (op2%17) {											\
		uint16_t cf=(uint16_t)FillFlags()&0x1;					\
		lf_var1w=load(op1);									\
		lf_var2b=op2%17;									\
	 	lf_resw=(lf_var1w >> lf_var2b) |					\
				(cf << (16-lf_var2b)) |						\
				(lf_var1w << (17-lf_var2b));				\
		save(op1,lf_resw);									\
		SETFLAGBIT(CF,((unsigned int)lf_var1w >> (lf_var2b - 1u)) & 1u);	\
		SETFLAGBIT(OF,(lf_resw ^ ((unsigned int)lf_resw<<1u)) & 0x8000u);	\
	}

#define RCRD(op1,op2,load,save)								\
	if (op2) {												\
		Bit32u cf=(Bit32u)FillFlags()&0x1;					\
		lf_var1d=load(op1);									\
		lf_var2b=op2;										\
		if (lf_var2b==1) {									\
			lf_resd=lf_var1d >> 1 | cf << 31;				\
		} else {											\
 			lf_resd=(lf_var1d >> lf_var2b) |				\
				(cf << (32-lf_var2b)) |						\
				(lf_var1d << (33-lf_var2b));				\
		}													\
		save(op1,lf_resd);									\
		SETFLAGBIT(CF,((unsigned int)lf_var1d >> (lf_var2b - 1u)) & 1u);	\
		SETFLAGBIT(OF,(lf_resd ^ ((unsigned int)lf_resd<<1u)) & 0x80000000u);	\
	}


#define SHLB(op1,op2,load,save)								\
	lf_var1b=load(op1);lf_var2b=op2;				\
	lf_resb=(lf_var2b < 8u) ? ((unsigned int)lf_var1b << lf_var2b) : 0;			\
	save(op1,lf_resb);								\
	lflags.type=t_SHLb;

#define SHLW(op1,op2,load,save)								\
	lf_var1w=load(op1);lf_var2b=op2 ;				\
	lf_resw=(lf_var2b < 16u) ? ((unsigned int)lf_var1w << lf_var2b) : 0;			\
	save(op1,lf_resw);								\
	lflags.type=t_SHLw;

#define SHLD(op1,op2,load,save)								\
	lf_var1d=load(op1);lf_var2b=op2;				\
	lf_resd=(lf_var2b < 32u) ? ((unsigned int)lf_var1d << lf_var2b) : 0;			\
	save(op1,lf_resd);								\
	lflags.type=t_SHLd;


#define SHRB(op1,op2,load,save)								\
	lf_var1b=load(op1);lf_var2b=op2;				\
	lf_resb=(lf_var2b < 8u) ? ((unsigned int)lf_var1b >> lf_var2b) : 0;			\
	save(op1,lf_resb);								\
	lflags.type=t_SHRb;

#define SHRW(op1,op2,load,save)								\
	lf_var1w=load(op1);lf_var2b=op2;				\
	lf_resw=(lf_var2b < 16u) ? ((unsigned int)lf_var1w >> lf_var2b) : 0;			\
	save(op1,lf_resw);								\
	lflags.type=t_SHRw;

#define SHRD(op1,op2,load,save)								\
	lf_var1d=load(op1);lf_var2b=op2;				\
	lf_resd=(lf_var2b < 32u) ? ((unsigned int)lf_var1d >> lf_var2b) : 0;			\
	save(op1,lf_resd);								\
	lflags.type=t_SHRd;


#define SARB(op1,op2,load,save)								\
	lf_var1b=load(op1);lf_var2b=op2;				\
	if (lf_var2b>8) lf_var2b=8;						\
    if (lf_var1b & 0x80) {								\
		lf_resb=(lf_var1b >> lf_var2b)|		\
		(0xff << (8 - lf_var2b));						\
	} else {												\
		lf_resb=lf_var1b >> lf_var2b;		\
    }														\
	save(op1,lf_resb);								\
	lflags.type=t_SARb;

#define SARW(op1,op2,load,save)								\
	lf_var1w=load(op1);lf_var2b=op2;			\
	if (lf_var2b>16) lf_var2b=16;					\
	if (lf_var1w & 0x8000) {							\
		lf_resw=(lf_var1w >> lf_var2b)|		\
		(0xffff << (16 - lf_var2b));					\
	} else {												\
		lf_resw=lf_var1w >> lf_var2b;		\
    }														\
	save(op1,lf_resw);								\
	lflags.type=t_SARw;

#define SARD(op1,op2,load,save)								\
	lf_var2b=op2;lf_var1d=load(op1);			\
	if (lf_var1d & 0x80000000) {						\
		lf_resd=(lf_var1d >> lf_var2b)|		\
		(0xffffffff << (32 - lf_var2b));				\
	} else {												\
		lf_resd=lf_var1d >> lf_var2b;		\
    }														\
	save(op1,lf_resd);								\
	lflags.type=t_SARd;



#define DAA()												\
	if (((reg_al & 0x0F)>0x09) || get_AF()) {				\
		if ((reg_al > 0x99) || get_CF()) {					\
			reg_al+=0x60;									\
			SETFLAGBIT(CF,true);							\
		} else {											\
			SETFLAGBIT(CF,false);							\
		}													\
		reg_al+=0x06;										\
		SETFLAGBIT(AF,true);								\
	} else {												\
		if ((reg_al > 0x99) || get_CF()) {					\
			reg_al+=0x60;									\
			SETFLAGBIT(CF,true);							\
		} else {											\
			SETFLAGBIT(CF,false);							\
		}													\
		SETFLAGBIT(AF,false);								\
	}														\
	SETFLAGBIT(SF,(reg_al&0x80));							\
	SETFLAGBIT(ZF,(reg_al==0));								\
	SETFLAGBIT(PF,parity_lookup[reg_al]);					\
	lflags.type=t_UNKNOWN;


#define DAS()												\
{															\
	uint8_t osigned=reg_al & 0x80;							\
	if (((reg_al & 0x0f) > 9) || get_AF()) {				\
		if ((reg_al>0x99) || get_CF()) {					\
			reg_al-=0x60;									\
			SETFLAGBIT(CF,true);							\
		} else {											\
			SETFLAGBIT(CF,(reg_al<=0x05));					\
		}													\
		reg_al-=6;											\
		SETFLAGBIT(AF,true);								\
	} else {												\
		if ((reg_al>0x99) || get_CF()) {					\
			reg_al-=0x60;									\
			SETFLAGBIT(CF,true);							\
		} else {											\
			SETFLAGBIT(CF,false);							\
		}													\
		SETFLAGBIT(AF,false);								\
	}														\
	SETFLAGBIT(OF,osigned && ((reg_al&0x80)==0));			\
	SETFLAGBIT(SF,(reg_al&0x80));							\
	SETFLAGBIT(ZF,(reg_al==0));								\
	SETFLAGBIT(PF,parity_lookup[reg_al]);					\
	lflags.type=t_UNKNOWN;									\
}


#define AAA()												\
	SETFLAGBIT(SF,((reg_al>=0x7a) && (reg_al<=0xf9)));		\
	if ((reg_al & 0xf) > 9) {								\
		SETFLAGBIT(OF,(reg_al&0xf0)==0x70);					\
		reg_ax += 0x106;									\
		SETFLAGBIT(CF,true);								\
		SETFLAGBIT(ZF,(reg_al == 0));						\
		SETFLAGBIT(AF,true);								\
	} else if (get_AF()) {									\
		reg_ax += 0x106;									\
		SETFLAGBIT(OF,false);								\
		SETFLAGBIT(CF,true);								\
		SETFLAGBIT(ZF,false);								\
		SETFLAGBIT(AF,true);								\
	} else {												\
		SETFLAGBIT(OF,false);								\
		SETFLAGBIT(CF,false);								\
		SETFLAGBIT(ZF,(reg_al == 0));						\
		SETFLAGBIT(AF,false);								\
	}														\
	SETFLAGBIT(PF,parity_lookup[reg_al]);					\
	reg_al &= 0x0F;											\
	lflags.type=t_UNKNOWN;

#define AAS()												\
	if ((reg_al & 0x0f)>9) {								\
		SETFLAGBIT(SF,(reg_al>0x85));						\
		reg_ax -= 0x106;									\
		SETFLAGBIT(OF,false);								\
		SETFLAGBIT(CF,true);								\
		SETFLAGBIT(AF,true);								\
	} else if (get_AF()) {									\
		SETFLAGBIT(OF,((reg_al>=0x80) && (reg_al<=0x85)));	\
		SETFLAGBIT(SF,(reg_al<0x06) || (reg_al>0x85));		\
		reg_ax -= 0x106;									\
		SETFLAGBIT(CF,true);								\
		SETFLAGBIT(AF,true);								\
	} else {												\
		SETFLAGBIT(SF,(reg_al>=0x80));						\
		SETFLAGBIT(OF,false);								\
		SETFLAGBIT(CF,false);								\
		SETFLAGBIT(AF,false);								\
	}														\
	SETFLAGBIT(ZF,(reg_al == 0));							\
	SETFLAGBIT(PF,parity_lookup[reg_al]);					\
	reg_al &= 0x0F;											\
	lflags.type=t_UNKNOWN;

#define AAM(op1)											\
{															\
	uint8_t dv=op1;											\
	if (dv!=0) {											\
		reg_ah=reg_al / dv;									\
		reg_al=reg_al % dv;									\
		SETFLAGBIT(SF,(reg_al & 0x80));						\
		SETFLAGBIT(ZF,(reg_al == 0));						\
		SETFLAGBIT(PF,parity_lookup[reg_al]);				\
		SETFLAGBIT(CF,false);								\
		SETFLAGBIT(OF,false);								\
		SETFLAGBIT(AF,false);								\
		lflags.type=t_UNKNOWN;								\
	} else EXCEPTION(0);									\
}


//Took this from bochs, i seriously hate these weird bcd opcodes
#define AAD(op1)											\
	{														\
		uint16_t ax1 = reg_ah * op1;							\
		uint16_t ax2 = ax1 + reg_al;							\
		reg_al = (uint8_t) ax2;								\
		reg_ah = 0;											\
		SETFLAGBIT(CF,false);								\
		SETFLAGBIT(OF,false);								\
		SETFLAGBIT(AF,false);								\
		SETFLAGBIT(SF,reg_al >= 0x80);						\
		SETFLAGBIT(ZF,reg_al == 0);							\
		SETFLAGBIT(PF,parity_lookup[reg_al]);				\
		lflags.type=t_UNKNOWN;								\
	}

#define PARITY16(x)  (parity_lookup[((unsigned int)((uint16_t)(x))>>8u)&0xffu]^parity_lookup[((uint8_t)(x))&0xffu]^FLAG_PF)
#define PARITY32(x)  (PARITY16(((uint16_t)(x))&0xffffu)^PARITY16(((unsigned int)((Bit32u)(x))>>16u)&0xffffu)^FLAG_PF)

#define MULB(op1,load,save)									\
	reg_ax=reg_al*load(op1);								\
	FillFlagsNoCFOF();										\
	SETFLAGBIT(ZF,reg_al == 0 && CPU_CORE >= CPU_ARCHTYPE_286);								\
	SETFLAGBIT(PF,PARITY16(reg_ax));								\
	if (reg_ax & 0xff00) {									\
		SETFLAGBIT(CF,true);SETFLAGBIT(OF,true);			\
	} else {												\
		SETFLAGBIT(CF,false);SETFLAGBIT(OF,false);			\
	}

#define MULW(op1,load,save)									\
{															\
	Bitu tempu=(Bitu)reg_ax*(Bitu)(load(op1));				\
	reg_ax=(uint16_t)(tempu);									\
	reg_dx=(uint16_t)(tempu >> 16);							\
	FillFlagsNoCFOF();										\
	SETFLAGBIT(ZF,reg_ax == 0 && CPU_CORE >= CPU_ARCHTYPE_286);								\
	SETFLAGBIT(PF,PARITY16(reg_ax)^PARITY16(reg_dx)^FLAG_PF);						\
	if (reg_dx) {											\
		SETFLAGBIT(CF,true);SETFLAGBIT(OF,true);			\
	} else {												\
		SETFLAGBIT(CF,false);SETFLAGBIT(OF,false);			\
	}														\
}

#define MULD(op1,load,save)									\
{															\
	Bit64u tempu=(Bit64u)reg_eax*(Bit64u)(load(op1));		\
	reg_eax=(Bit32u)(tempu);								\
	reg_edx=(Bit32u)(tempu >> 32);							\
	FillFlagsNoCFOF();										\
	SETFLAGBIT(ZF,reg_eax == 0 && CPU_CORE >= CPU_ARCHTYPE_286);							\
	SETFLAGBIT(PF,PARITY32(reg_eax)^PARITY32(reg_edx)^FLAG_PF);						\
	if (reg_edx) {											\
		SETFLAGBIT(CF,true);SETFLAGBIT(OF,true);			\
	} else {												\
		SETFLAGBIT(CF,false);SETFLAGBIT(OF,false);			\
	}														\
}

#define DIVB(op1,load,save)									\
{															\
	Bitu val=load(op1);										\
	if (val==0)	EXCEPTION(0);								\
	Bitu quo=reg_ax / val;									\
	uint8_t rem=(uint8_t)(reg_ax % val);						\
	uint8_t quo8=(uint8_t)(quo&0xff);							\
	if (quo>0xff) EXCEPTION(0);								\
	reg_ah=rem;												\
	reg_al=quo8;											\
	FillFlags();											\
	SETFLAGBIT(AF,0);/*FIXME*/									\
	SETFLAGBIT(SF,0);/*FIXME*/									\
	SETFLAGBIT(OF,0);/*FIXME*/									\
	SETFLAGBIT(ZF,(rem==0)&&((quo8&1)!=0));								\
	SETFLAGBIT(CF,((rem&3) >= 1 && (rem&3) <= 2)); \
	SETFLAGBIT(PF,parity_lookup[rem&0xff]^parity_lookup[quo8&0xff]^FLAG_PF);					\
}


#define DIVW(op1,load,save)									\
{															\
	Bitu val=load(op1);										\
	if (val==0)	EXCEPTION(0);								\
	Bitu num=((Bit32u)reg_dx<<16)|reg_ax;							\
	Bitu quo=num/val;										\
	uint16_t rem=(uint16_t)(num % val);							\
	uint16_t quo16=(uint16_t)(quo&0xffff);						\
	if (quo!=(Bit32u)quo16) EXCEPTION(0);					\
	reg_dx=rem;												\
	reg_ax=quo16;											\
	FillFlags();											\
	SETFLAGBIT(AF,0);/*FIXME*/									\
	SETFLAGBIT(SF,0);/*FIXME*/									\
	SETFLAGBIT(OF,0);/*FIXME*/									\
	SETFLAGBIT(ZF,(rem==0)&&((quo16&1)!=0));								\
	SETFLAGBIT(CF,((rem&3) >= 1 && (rem&3) <= 2)); \
	SETFLAGBIT(PF,PARITY16(rem&0xffff)^PARITY16(quo16&0xffff)^FLAG_PF);					\
}

#define DIVD(op1,load,save)									\
{															\
	Bitu val=load(op1);										\
	if (val==0) EXCEPTION(0);									\
	Bit64u num=(((Bit64u)reg_edx)<<32)|reg_eax;				\
	Bit64u quo=num/val;										\
	Bit32u rem=(Bit32u)(num % val);							\
	Bit32u quo32=(Bit32u)(quo&0xffffffff);					\
	if (quo!=(Bit64u)quo32) EXCEPTION(0);					\
	reg_edx=rem;											\
	reg_eax=quo32;											\
	FillFlags();											\
	SETFLAGBIT(AF,0);/*FIXME*/									\
	SETFLAGBIT(SF,0);/*FIXME*/									\
	SETFLAGBIT(OF,0);/*FIXME*/									\
	SETFLAGBIT(ZF,(rem==0)&&((quo32&1)!=0));								\
	SETFLAGBIT(CF,((rem&3) >= 1 && (rem&3) <= 2)); \
	SETFLAGBIT(PF,PARITY32(rem&0xffffffff)^PARITY32(quo32&0xffffffff)^FLAG_PF);					\
}


#define IDIVB(op1,load,save)								\
{															\
	Bits val=(int8_t)(load(op1));							\
	if (val==0)	EXCEPTION(0);								\
	Bits quo=((Bit16s)reg_ax) / val;						\
	int8_t rem=(int8_t)((Bit16s)reg_ax % val);				\
	int8_t quo8s=(int8_t)(quo&0xff);							\
	if (quo!=(Bit16s)quo8s) EXCEPTION(0);					\
	reg_ah=(uint8_t)rem;												\
	reg_al=(uint8_t)quo8s;											\
	FillFlags();											\
	SETFLAGBIT(AF,0);/*FIXME*/									\
	SETFLAGBIT(SF,0);/*FIXME*/									\
	SETFLAGBIT(OF,0);/*FIXME*/									\
	SETFLAGBIT(ZF,(rem==0)&&((quo8s&1)!=0));								\
	SETFLAGBIT(CF,((rem&3) >= 1 && (rem&3) <= 2)); \
	SETFLAGBIT(PF,parity_lookup[rem&0xff]^parity_lookup[quo8s&0xff]^FLAG_PF);					\
}


#define IDIVW(op1,load,save)								\
{															\
	Bits val=(Bit16s)(load(op1));							\
	if (val==0) EXCEPTION(0);									\
	Bits num=(Bit32s)(((unsigned int)reg_dx<<16u)|(unsigned int)reg_ax);					\
	Bits quo=num/val;										\
	Bit16s rem=(Bit16s)(num % val);							\
	Bit16s quo16s=(Bit16s)quo;								\
	if (quo!=(Bit32s)quo16s) EXCEPTION(0);					\
	reg_dx=(uint16_t)rem;												\
	reg_ax=(uint16_t)quo16s;											\
	FillFlags();											\
	SETFLAGBIT(AF,0);/*FIXME*/									\
	SETFLAGBIT(SF,0);/*FIXME*/									\
	SETFLAGBIT(OF,0);/*FIXME*/									\
	SETFLAGBIT(ZF,(rem==0)&&((quo16s&1)!=0));								\
	SETFLAGBIT(CF,((rem&3) >= 1 && (rem&3) <= 2)); \
	SETFLAGBIT(PF,PARITY16(rem&0xffff)^PARITY16(quo16s&0xffff)^FLAG_PF);					\
}

#define IDIVD(op1,load,save)								\
{															\
	Bits val=(Bit32s)(load(op1));							\
	if (val==0) EXCEPTION(0);									\
	Bit64s num=(Bit64s)((((Bit64u)reg_edx)<<(Bit64u)32)|(Bit64u)reg_eax);				\
	Bit64s quo=num/val;										\
	Bit32s rem=(Bit32s)(num % val);							\
	Bit32s quo32s=(Bit32s)(quo&0xffffffff);					\
	if (quo!=(Bit64s)quo32s) EXCEPTION(0);					\
	reg_edx=(Bit32u)rem;											\
	reg_eax=(Bit32u)quo32s;											\
	FillFlags();											\
	SETFLAGBIT(AF,0);/*FIXME*/									\
	SETFLAGBIT(SF,0);/*FIXME*/									\
	SETFLAGBIT(OF,0);/*FIXME*/									\
	SETFLAGBIT(ZF,(rem==0)&&((quo32s&1)!=0));								\
	SETFLAGBIT(CF,((rem&3) >= 1 && (rem&3) <= 2)); \
	SETFLAGBIT(PF,PARITY32((Bit32u)rem&0xffffffffu)^PARITY32((Bit32u)quo32s&0xffffffffu)^FLAG_PF);					\
}

#define IMULB(op1,load,save)								\
{															\
	reg_ax=((int8_t)reg_al) * ((int8_t)(load(op1)));			\
	FillFlagsNoCFOF();										\
	SETFLAGBIT(ZF,reg_al == 0);								\
	SETFLAGBIT(SF,reg_al & 0x80);							\
	if ((reg_ax & 0xff80)==0xff80 ||						\
		(reg_ax & 0xff80)==0x0000) {						\
		SETFLAGBIT(CF,false);SETFLAGBIT(OF,false);			\
	} else {												\
		SETFLAGBIT(CF,true);SETFLAGBIT(OF,true);			\
	}														\
}
	

#define IMULW(op1,load,save)								\
{															\
	Bits temps=((Bit16s)reg_ax)*((Bit16s)(load(op1)));		\
	reg_ax=(uint16_t)(temps);									\
	reg_dx=(uint16_t)(temps >> 16);							\
	FillFlagsNoCFOF();										\
	SETFLAGBIT(ZF,reg_ax == 0);								\
	SETFLAGBIT(SF,reg_ax & 0x8000);							\
	if ((((unsigned int)temps & 0xffff8000)==0xffff8000 ||				\
		((unsigned int)temps & 0xffff8000)==0x0000)) {					\
		SETFLAGBIT(CF,false);SETFLAGBIT(OF,false);			\
	} else {												\
		SETFLAGBIT(CF,true);SETFLAGBIT(OF,true);			\
	}														\
}

#define IMULD(op1,load,save)								\
{															\
	Bit64s temps=((Bit64s)((Bit32s)reg_eax))*				\
				 ((Bit64s)((Bit32s)(load(op1))));			\
	reg_eax=(Bit32u)(temps);								\
	reg_edx=(Bit32u)(temps >> 32);							\
	FillFlagsNoCFOF();										\
	SETFLAGBIT(ZF,reg_eax == 0);							\
	SETFLAGBIT(SF,reg_eax & 0x80000000);					\
	if ((reg_edx==0xffffffff) &&							\
		(reg_eax & 0x80000000) ) {							\
		SETFLAGBIT(CF,false);SETFLAGBIT(OF,false);			\
	} else if ( (reg_edx==0x00000000) &&					\
				(reg_eax< 0x80000000) ) {					\
		SETFLAGBIT(CF,false);SETFLAGBIT(OF,false);			\
	} else {												\
		SETFLAGBIT(CF,true);SETFLAGBIT(OF,true);			\
	}														\
}

#define DIMULW(op1,op2,op3,load,save)						\
{															\
	Bits res=((Bit16s)op2) * ((Bit16s)op3);					\
	save(op1,res & 0xffff);									\
	FillFlagsNoCFOF();										\
	if ((res>= -32768)  && (res<=32767)) {					\
		SETFLAGBIT(CF,false);SETFLAGBIT(OF,false);			\
	} else {												\
		SETFLAGBIT(CF,true);SETFLAGBIT(OF,true);			\
	}														\
}

#define DIMULD(op1,op2,op3,load,save)						\
{															\
	Bit64s res=((Bit64s)((Bit32s)op2))*((Bit64s)((Bit32s)op3));	\
	save(op1,(Bit32s)res);									\
	FillFlagsNoCFOF();										\
	if ((res>=-((Bit64s)(2147483647)+1)) &&					\
		(res<=(Bit64s)2147483647)) {						\
		SETFLAGBIT(CF,false);SETFLAGBIT(OF,false);			\
	} else {												\
		SETFLAGBIT(CF,true);SETFLAGBIT(OF,true);			\
	}														\
}

#if CPU_CORE == CPU_ARCHTYPE_8086
#  define CPU_SHIFTOP_MASK(x,m) ((x) & 0xff)
#else
#  define CPU_SHIFTOP_MASK(x,m) ((x) & 0x1f)
#endif

/* FIXME: For 8086 core we care mostly about whether or not SHL/SHR emulate the non-masked shift count.
 *        Note that running this code compiled for Intel processors naturally imposes the shift count,
 *        compilers generate a shift instruction and do not consider the CPU masking the bit count.
 *
 *        unsigned int a = 0x12345678,b = 0x20,c;
 *        c = a << b;      <-- on Intel x86 builds, c == a because GCC will compile to shl eax,cl and Intel processors will act like c == a << (b&0x1F).
 *
 *        What we care about is that shift counts greater than or equal to the width of the target register come out to zero,
 *        or for SAR, that all bits become copies of the sign bit. When emulating the 8086 we want DOS programs to be able to
 *        test that we are an 8086 by executing shl ax,cl with cl == 32 and to get AX == 0 as a result instead of AX unchanged.
 *        the shift count mask test is one of the several tests DOS programs may do to differentiate 8086 from 80186. */
#define GRP2B(blah)											\
{															\
	GetRM;Bitu which=(rm>>3)&7;								\
	if (rm >= 0xc0) {										\
		GetEArb;											\
		uint8_t val=CPU_SHIFTOP_MASK(blah,7);								\
		if (!val) break;									\
		switch (which)	{									\
		case 0x00:ROLB(*earb,val,LoadRb,SaveRb);break;		\
		case 0x01:RORB(*earb,val,LoadRb,SaveRb);break;		\
		case 0x02:RCLB(*earb,val,LoadRb,SaveRb);break;		\
		case 0x03:RCRB(*earb,val,LoadRb,SaveRb);break;		\
		case 0x04:/* SHL and SAL are the same */			\
		case 0x06:SHLB(*earb,val,LoadRb,SaveRb);break;		\
		case 0x05:SHRB(*earb,val,LoadRb,SaveRb);break;		\
		case 0x07:SARB(*earb,val,LoadRb,SaveRb);break;		\
		}													\
	} else {												\
		GetEAa;												\
		uint8_t val=CPU_SHIFTOP_MASK(blah,7);								\
		if (!val) break;									\
		switch (which) {									\
		case 0x00:ROLB(eaa,val,LoadMb,SaveMb);break;		\
		case 0x01:RORB(eaa,val,LoadMb,SaveMb);break;		\
		case 0x02:RCLB(eaa,val,LoadMb,SaveMb);break;		\
		case 0x03:RCRB(eaa,val,LoadMb,SaveMb);break;		\
		case 0x04:/* SHL and SAL are the same */			\
		case 0x06:SHLB(eaa,val,LoadMb,SaveMb);break;		\
		case 0x05:SHRB(eaa,val,LoadMb,SaveMb);break;		\
		case 0x07:SARB(eaa,val,LoadMb,SaveMb);break;		\
		}													\
	}														\
}



#define GRP2W(blah)											\
{															\
	GetRM;Bitu which=(rm>>3)&7;								\
	if (rm >= 0xc0) {										\
		GetEArw;											\
		uint8_t val=CPU_SHIFTOP_MASK(blah,15);								\
		if (!val) break;									\
		switch (which)	{									\
		case 0x00:ROLW(*earw,val,LoadRw,SaveRw);break;		\
		case 0x01:RORW(*earw,val,LoadRw,SaveRw);break;		\
		case 0x02:RCLW(*earw,val,LoadRw,SaveRw);break;		\
		case 0x03:RCRW(*earw,val,LoadRw,SaveRw);break;		\
		case 0x04:/* SHL and SAL are the same */			\
		case 0x06:SHLW(*earw,val,LoadRw,SaveRw);break;		\
		case 0x05:SHRW(*earw,val,LoadRw,SaveRw);break;		\
		case 0x07:SARW(*earw,val,LoadRw,SaveRw);break;		\
		}													\
	} else {												\
		GetEAa;												\
		uint8_t val=CPU_SHIFTOP_MASK(blah,15);								\
		if (!val) break;									\
		switch (which) {									\
		case 0x00:ROLW(eaa,val,LoadMw,SaveMw);break;		\
		case 0x01:RORW(eaa,val,LoadMw,SaveMw);break;		\
		case 0x02:RCLW(eaa,val,LoadMw,SaveMw);break;		\
		case 0x03:RCRW(eaa,val,LoadMw,SaveMw);break;		\
		case 0x04:/* SHL and SAL are the same */			\
		case 0x06:SHLW(eaa,val,LoadMw,SaveMw);break;		\
		case 0x05:SHRW(eaa,val,LoadMw,SaveMw);break;		\
		case 0x07:SARW(eaa,val,LoadMw,SaveMw);break;		\
		}													\
	}														\
}


#define GRP2D(blah)											\
{															\
	GetRM;Bitu which=(rm>>3)&7;								\
	if (rm >= 0xc0) {										\
		GetEArd;											\
		uint8_t val=CPU_SHIFTOP_MASK(blah,31);								\
		if (!val) break;									\
		switch (which)	{									\
		case 0x00:ROLD(*eard,val,LoadRd,SaveRd);break;		\
		case 0x01:RORD(*eard,val,LoadRd,SaveRd);break;		\
		case 0x02:RCLD(*eard,val,LoadRd,SaveRd);break;		\
		case 0x03:RCRD(*eard,val,LoadRd,SaveRd);break;		\
		case 0x04:/* SHL and SAL are the same */			\
		case 0x06:SHLD(*eard,val,LoadRd,SaveRd);break;		\
		case 0x05:SHRD(*eard,val,LoadRd,SaveRd);break;		\
		case 0x07:SARD(*eard,val,LoadRd,SaveRd);break;		\
		}													\
	} else {												\
		GetEAa;												\
		uint8_t val=CPU_SHIFTOP_MASK(blah,31);								\
		if (!val) break;									\
		switch (which) {									\
		case 0x00:ROLD(eaa,val,LoadMd,SaveMd);break;		\
		case 0x01:RORD(eaa,val,LoadMd,SaveMd);break;		\
		case 0x02:RCLD(eaa,val,LoadMd,SaveMd);break;		\
		case 0x03:RCRD(eaa,val,LoadMd,SaveMd);break;		\
		case 0x04:/* SHL and SAL are the same */			\
		case 0x06:SHLD(eaa,val,LoadMd,SaveMd);break;		\
		case 0x05:SHRD(eaa,val,LoadMd,SaveMd);break;		\
		case 0x07:SARD(eaa,val,LoadMd,SaveMd);break;		\
		}													\
	}														\
}

/* let's hope bochs has it correct with the higher than 16 shifts */
/* double-precision shift left has low bits in second argument */
#define DSHLW(op1,op2,op3,load,save)									\
	uint8_t val=op3 & 0x1Fu;												\
	if (!val) break;													\
	lf_var2b=val;lf_var1d=((unsigned int)load(op1)<<16u)|op2;					\
	Bit32u tempd=lf_var1d << lf_var2b;							\
  	if (lf_var2b>16u) tempd |= ((unsigned int)op2 << (lf_var2b - 16u));			\
	lf_resw=(uint16_t)((unsigned int)tempd >> 16u);								\
	save(op1,lf_resw);											\
	lflags.type=t_DSHLw;

#define DSHLD(op1,op2,op3,load,save)									\
	uint8_t val=op3 & 0x1Fu;												\
	if (!val) break;													\
	lf_var2b=val;lf_var1d=load(op1);							\
	lf_resd=((unsigned int)lf_var1d << lf_var2b) | ((unsigned int)op2 >> (32u-lf_var2b));	\
	save(op1,lf_resd);											\
	lflags.type=t_DSHLd;

/* double-precision shift right has high bits in second argument */
#define DSHRW(op1,op2,op3,load,save)									\
	uint8_t val=op3 & 0x1Fu;												\
	if (!val) break;													\
	lf_var2b=val;lf_var1d=((unsigned int)op2<<16u)|load(op1);					\
	Bit32u tempd=(unsigned int)lf_var1d >> lf_var2b;							\
  	if (lf_var2b>16u) tempd |= ((unsigned int)op2 << (32u-lf_var2b));			\
	lf_resw=(uint16_t)(tempd);										\
	save(op1,lf_resw);											\
	lflags.type=t_DSHRw;

#define DSHRD(op1,op2,op3,load,save)									\
	uint8_t val=op3 & 0x1Fu;												\
	if (!val) break;													\
	lf_var2b=val;lf_var1d=load(op1);							\
	lf_resd=((unsigned int)lf_var1d >> lf_var2b) | ((unsigned int)op2 << (32u-lf_var2b));	\
	save(op1,lf_resd);											\
	lflags.type=t_DSHRd;

#define BSWAPW(op1)														\
	op1 = 0;

#define BSWAPD(op1)														\
	op1 = (op1>>24)|((op1>>8)&0xFF00)|((op1<<8)&0xFF0000)|((op1<<24)&0xFF000000);
