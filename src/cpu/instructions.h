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

/* Jumps */

/* All Byte general instructions */
#define ADDB(op1,op2,load,save)								\
	lf_var1b=load(op1);lf_var2b=op2;					\
	lf_resb=lf_var1b+lf_var2b;					\
	save(op1,lf_resb);								\
	lflags.type=t_ADDb;

#define ADCB(op1,op2,load,save)								\
	lflags.oldcf=get_CF()!=0;								\
	lf_var1b=load(op1);lf_var2b=op2;					\
	lf_resb=lf_var1b+lf_var2b+lflags.oldcf;		\
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
	lf_resw=lf_var1w+lf_var2w+lflags.oldcf;		\
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
	if (!(op2&0x7)) {								\
		if (op2&0x18) {								\
			FillFlagsNoCFOF();						\
			SETFLAGBIT(CF,op1 & 1);					\
			SETFLAGBIT(OF,(op1 & 1) ^ (op1 >> 7));	\
		}											\
		break;										\
	}												\
	FillFlagsNoCFOF();								\
	lf_var1b=load(op1);								\
	lf_var2b=op2&0x07;								\
	lf_resb=(lf_var1b << lf_var2b) |				\
			(lf_var1b >> (8-lf_var2b));				\
	save(op1,lf_resb);								\
	SETFLAGBIT(CF,lf_resb & 1);						\
	SETFLAGBIT(OF,(lf_resb & 1) ^ (lf_resb >> 7));

#define ROLW(op1,op2,load,save)						\
	if (!(op2&0xf)) {								\
		if (op2&0x10) {								\
			FillFlagsNoCFOF();						\
			SETFLAGBIT(CF,op1 & 1);					\
			SETFLAGBIT(OF,(op1 & 1) ^ (op1 >> 15));	\
		}											\
		break;										\
	}												\
	FillFlagsNoCFOF();								\
	lf_var1w=load(op1);								\
	lf_var2b=op2&0xf;								\
	lf_resw=(lf_var1w << lf_var2b) |				\
			(lf_var1w >> (16-lf_var2b));			\
	save(op1,lf_resw);								\
	SETFLAGBIT(CF,lf_resw & 1);						\
	SETFLAGBIT(OF,(lf_resw & 1) ^ (lf_resw >> 15));

#define ROLD(op1,op2,load,save)						\
	if (!op2) break;								\
	FillFlagsNoCFOF();								\
	lf_var1d=load(op1);								\
	lf_var2b=op2;									\
	lf_resd=(lf_var1d << lf_var2b) |				\
			(lf_var1d >> (32-lf_var2b));			\
	save(op1,lf_resd);								\
	SETFLAGBIT(CF,lf_resd & 1);						\
	SETFLAGBIT(OF,(lf_resd & 1) ^ (lf_resd >> 31));


#define RORB(op1,op2,load,save)						\
	if (!(op2&0x7)) {								\
		if (op2&0x18) {								\
			FillFlagsNoCFOF();						\
			SETFLAGBIT(CF,op1>>7);					\
			SETFLAGBIT(OF,(op1>>7) ^ ((op1>>6) & 1));			\
		}											\
		break;										\
	}												\
	FillFlagsNoCFOF();								\
	lf_var1b=load(op1);								\
	lf_var2b=op2&0x07;								\
	lf_resb=(lf_var1b >> lf_var2b) |				\
			(lf_var1b << (8-lf_var2b));				\
	save(op1,lf_resb);								\
	SETFLAGBIT(CF,lf_resb & 0x80);					\
	SETFLAGBIT(OF,(lf_resb ^ (lf_resb<<1)) & 0x80);

#define RORW(op1,op2,load,save)					\
	if (!(op2&0xf)) {							\
		if (op2&0x10) {							\
			FillFlagsNoCFOF();					\
			SETFLAGBIT(CF,op1>>15);				\
			SETFLAGBIT(OF,(op1>>15) ^ ((op1>>14) & 1));			\
		}										\
		break;									\
	}											\
	FillFlagsNoCFOF();							\
	lf_var1w=load(op1);							\
	lf_var2b=op2&0xf;							\
	lf_resw=(lf_var1w >> lf_var2b) |			\
			(lf_var1w << (16-lf_var2b));		\
	save(op1,lf_resw);							\
	SETFLAGBIT(CF,lf_resw & 0x8000);			\
	SETFLAGBIT(OF,(lf_resw ^ (lf_resw<<1)) & 0x8000);

#define RORD(op1,op2,load,save)					\
	if (!op2) break;							\
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
{	Bit8u cf=(Bit8u)FillFlags()&0x1;					\
	lf_var1b=load(op1);									\
	lf_var2b=op2%9;										\
	lf_resb=(lf_var1b << lf_var2b) |					\
			(cf << (lf_var2b-1)) |						\
			(lf_var1b >> (9-lf_var2b));					\
 	save(op1,lf_resb);									\
	SETFLAGBIT(CF,((lf_var1b >> (8-lf_var2b)) & 1));	\
	SETFLAGBIT(OF,(reg_flags & 1) ^ (lf_resb >> 7));	\
}

#define RCLW(op1,op2,load,save)							\
	if (!(op2%17)) break;								\
{	Bit16u cf=(Bit16u)FillFlags()&0x1;					\
	lf_var1w=load(op1);									\
	lf_var2b=op2%17;									\
	lf_resw=(lf_var1w << lf_var2b) |					\
			(cf << (lf_var2b-1)) |						\
			(lf_var1w >> (17-lf_var2b));				\
	save(op1,lf_resw);									\
	SETFLAGBIT(CF,((lf_var1w >> (16-lf_var2b)) & 1));	\
	SETFLAGBIT(OF,(reg_flags & 1) ^ (lf_resw >> 15));	\
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
	SETFLAGBIT(CF,((lf_var1d >> (32-lf_var2b)) & 1));	\
	SETFLAGBIT(OF,(reg_flags & 1) ^ (lf_resd >> 31));	\
}



#define RCRB(op1,op2,load,save)								\
	if (op2%9) {											\
		Bit8u cf=(Bit8u)FillFlags()&0x1;					\
		lf_var1b=load(op1);									\
		lf_var2b=op2%9;										\
	 	lf_resb=(lf_var1b >> lf_var2b) |					\
				(cf << (8-lf_var2b)) |						\
				(lf_var1b << (9-lf_var2b));					\
		save(op1,lf_resb);									\
		SETFLAGBIT(CF,(lf_var1b >> (lf_var2b - 1)) & 1);	\
		SETFLAGBIT(OF,(lf_resb ^ (lf_resb<<1)) & 0x80);		\
	}

#define RCRW(op1,op2,load,save)								\
	if (op2%17) {											\
		Bit16u cf=(Bit16u)FillFlags()&0x1;					\
		lf_var1w=load(op1);									\
		lf_var2b=op2%17;									\
	 	lf_resw=(lf_var1w >> lf_var2b) |					\
				(cf << (16-lf_var2b)) |						\
				(lf_var1w << (17-lf_var2b));				\
		save(op1,lf_resw);									\
		SETFLAGBIT(CF,(lf_var1w >> (lf_var2b - 1)) & 1);	\
		SETFLAGBIT(OF,(lf_resw ^ (lf_resw<<1)) & 0x8000);	\
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
		SETFLAGBIT(CF,(lf_var1d >> (lf_var2b - 1)) & 1);	\
		SETFLAGBIT(OF,(lf_resd ^ (lf_resd<<1)) & 0x80000000);	\
	}


#define SHLB(op1,op2,load,save)								\
	if (!op2) break;										\
	lf_var1b=load(op1);lf_var2b=op2;				\
	lf_resb=lf_var1b << lf_var2b;			\
	save(op1,lf_resb);								\
	lflags.type=t_SHLb;

#define SHLW(op1,op2,load,save)								\
	if (!op2) break;										\
	lf_var1w=load(op1);lf_var2b=op2 ;				\
	lf_resw=lf_var1w << lf_var2b;			\
	save(op1,lf_resw);								\
	lflags.type=t_SHLw;

#define SHLD(op1,op2,load,save)								\
	if (!op2) break;										\
	lf_var1d=load(op1);lf_var2b=op2;				\
	lf_resd=lf_var1d << lf_var2b;			\
	save(op1,lf_resd);								\
	lflags.type=t_SHLd;


#define SHRB(op1,op2,load,save)								\
	if (!op2) break;										\
	lf_var1b=load(op1);lf_var2b=op2;				\
	lf_resb=lf_var1b >> lf_var2b;			\
	save(op1,lf_resb);								\
	lflags.type=t_SHRb;

#define SHRW(op1,op2,load,save)								\
	if (!op2) break;										\
	lf_var1w=load(op1);lf_var2b=op2;				\
	lf_resw=lf_var1w >> lf_var2b;			\
	save(op1,lf_resw);								\
	lflags.type=t_SHRw;

#define SHRD(op1,op2,load,save)								\
	if (!op2) break;										\
	lf_var1d=load(op1);lf_var2b=op2;				\
	lf_resd=lf_var1d >> lf_var2b;			\
	save(op1,lf_resd);								\
	lflags.type=t_SHRd;


#define SARB(op1,op2,load,save)								\
	if (!op2) break;										\
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
	if (!op2) break;								\
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
	if (!op2) break;								\
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
	Bit8u osigned=reg_al & 0x80;							\
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
	Bit8u dv=op1;											\
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
		Bit16u ax1 = reg_ah * op1;							\
		Bit16u ax2 = ax1 + reg_al;							\
		reg_al = (Bit8u) ax2;								\
		reg_ah = 0;											\
		SETFLAGBIT(CF,false);								\
		SETFLAGBIT(OF,false);								\
		SETFLAGBIT(AF,false);								\
		SETFLAGBIT(SF,reg_al >= 0x80);						\
		SETFLAGBIT(ZF,reg_al == 0);							\
		SETFLAGBIT(PF,parity_lookup[reg_al]);				\
		lflags.type=t_UNKNOWN;								\
	}

#define MULB(op1,load,save)									\
	reg_ax=reg_al*load(op1);								\
	FillFlagsNoCFOF();										\
	SETFLAGBIT(ZF,reg_al == 0);								\
	if (reg_ax & 0xff00) {									\
		SETFLAGBIT(CF,true);SETFLAGBIT(OF,true);			\
	} else {												\
		SETFLAGBIT(CF,false);SETFLAGBIT(OF,false);			\
	}

#define MULW(op1,load,save)									\
{															\
	Bitu tempu=(Bitu)reg_ax*(Bitu)(load(op1));				\
	reg_ax=(Bit16u)(tempu);									\
	reg_dx=(Bit16u)(tempu >> 16);							\
	FillFlagsNoCFOF();										\
	SETFLAGBIT(ZF,reg_ax == 0);								\
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
	SETFLAGBIT(ZF,reg_eax == 0);							\
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
	Bit8u rem=(Bit8u)(reg_ax % val);						\
	Bit8u quo8=(Bit8u)(quo&0xff);							\
	if (quo>0xff) EXCEPTION(0);								\
	reg_ah=rem;												\
	reg_al=quo8;											\
}


#define DIVW(op1,load,save)									\
{															\
	Bitu val=load(op1);										\
	if (val==0)	EXCEPTION(0);								\
	Bitu num=((Bit32u)reg_dx<<16)|reg_ax;							\
	Bitu quo=num/val;										\
	Bit16u rem=(Bit16u)(num % val);							\
	Bit16u quo16=(Bit16u)(quo&0xffff);						\
	if (quo!=(Bit32u)quo16) EXCEPTION(0);					\
	reg_dx=rem;												\
	reg_ax=quo16;											\
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
}


#define IDIVB(op1,load,save)								\
{															\
	Bits val=(Bit8s)(load(op1));							\
	if (val==0)	EXCEPTION(0);								\
	Bits quo=((Bit16s)reg_ax) / val;						\
	Bit8s rem=(Bit8s)((Bit16s)reg_ax % val);				\
	Bit8s quo8s=(Bit8s)(quo&0xff);							\
	if (quo!=(Bit16s)quo8s) EXCEPTION(0);					\
	reg_ah=rem;												\
	reg_al=quo8s;											\
}


#define IDIVW(op1,load,save)								\
{															\
	Bits val=(Bit16s)(load(op1));							\
	if (val==0) EXCEPTION(0);									\
	Bits num=(Bit32s)((reg_dx<<16)|reg_ax);					\
	Bits quo=num/val;										\
	Bit16s rem=(Bit16s)(num % val);							\
	Bit16s quo16s=(Bit16s)quo;								\
	if (quo!=(Bit32s)quo16s) EXCEPTION(0);					\
	reg_dx=rem;												\
	reg_ax=quo16s;											\
}

#define IDIVD(op1,load,save)								\
{															\
	Bits val=(Bit32s)(load(op1));							\
	if (val==0) EXCEPTION(0);									\
	Bit64s num=(((Bit64u)reg_edx)<<32)|reg_eax;				\
	Bit64s quo=num/val;										\
	Bit32s rem=(Bit32s)(num % val);							\
	Bit32s quo32s=(Bit32s)(quo&0xffffffff);					\
	if (quo!=(Bit64s)quo32s) EXCEPTION(0);					\
	reg_edx=rem;											\
	reg_eax=quo32s;											\
}

#define IMULB(op1,load,save)								\
{															\
	reg_ax=((Bit8s)reg_al) * ((Bit8s)(load(op1)));			\
	FillFlagsNoCFOF();										\
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
	reg_ax=(Bit16s)(temps);									\
	reg_dx=(Bit16s)(temps >> 16);							\
	FillFlagsNoCFOF();										\
	if (((temps & 0xffff8000)==0xffff8000 ||				\
		(temps & 0xffff8000)==0x0000)) {					\
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
	if ((res> -32768)  && (res<32767)) {					\
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
	if ((res>-((Bit64s)(2147483647)+1)) &&					\
		(res<(Bit64s)2147483647)) {							\
		SETFLAGBIT(CF,false);SETFLAGBIT(OF,false);			\
	} else {												\
		SETFLAGBIT(CF,true);SETFLAGBIT(OF,true);			\
	}														\
}

#define GRP2B(blah)											\
{															\
	GetRM;Bitu which=(rm>>3)&7;								\
	if (rm >= 0xc0) {										\
		GetEArb;											\
		Bit8u val=blah & 0x1f;								\
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
		Bit8u val=blah & 0x1f;								\
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
		Bit8u val=blah & 0x1f;								\
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
		Bit8u val=blah & 0x1f;								\
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
		Bit8u val=blah & 0x1f;								\
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
		Bit8u val=blah & 0x1f;								\
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
	Bit8u val=op3 & 0x1F;												\
	if (!val) break;													\
	lf_var2b=val;lf_var1d=(load(op1)<<16)|op2;					\
	Bit32u tempd=lf_var1d << lf_var2b;							\
  	if (lf_var2b>16) tempd |= (op2 << (lf_var2b - 16));			\
	lf_resw=(Bit16u)(tempd >> 16);								\
	save(op1,lf_resw);											\
	lflags.type=t_DSHLw;

#define DSHLD(op1,op2,op3,load,save)									\
	Bit8u val=op3 & 0x1F;												\
	if (!val) break;													\
	lf_var2b=val;lf_var1d=load(op1);							\
	lf_resd=(lf_var1d << lf_var2b) | (op2 >> (32-lf_var2b));	\
	save(op1,lf_resd);											\
	lflags.type=t_DSHLd;

/* double-precision shift right has high bits in second argument */
#define DSHRW(op1,op2,op3,load,save)									\
	Bit8u val=op3 & 0x1F;												\
	if (!val) break;													\
	lf_var2b=val;lf_var1d=(op2<<16)|load(op1);					\
	Bit32u tempd=lf_var1d >> lf_var2b;							\
  	if (lf_var2b>16) tempd |= (op2 << (32-lf_var2b ));			\
	lf_resw=(Bit16u)(tempd);										\
	save(op1,lf_resw);											\
	lflags.type=t_DSHRw;

#define DSHRD(op1,op2,op3,load,save)									\
	Bit8u val=op3 & 0x1F;												\
	if (!val) break;													\
	lf_var2b=val;lf_var1d=load(op1);							\
	lf_resd=(lf_var1d >> lf_var2b) | (op2 << (32-lf_var2b));	\
	save(op1,lf_resd);											\
	lflags.type=t_DSHRd;

#define BSWAPW(op1)														\
	op1 = 0;

#define BSWAPD(op1)														\
	op1 = (op1>>24)|((op1>>8)&0xFF00)|((op1<<8)&0xFF0000)|((op1<<24)&0xFF000000);
