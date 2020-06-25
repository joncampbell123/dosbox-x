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
#if C_FPU

#include <math.h>
#include <float.h>
#include "paging.h"
#include "cross.h"
#include "mem.h"
#include "fpu.h"
#include "cpu.h"
#include "../cpu/lazyflags.h"

FPU_rec fpu;

void FPU_FLDCW(PhysPt addr){
	Bit16u temp = mem_readw(addr);
	FPU_SetCW(temp);
}

Bit16u FPU_GetTag(void){
	Bit16u tag=0;

	for (Bitu i=0;i<8;i++)
		tag |= (fpu.tags[i]&3) << (2*i);

	return tag;
}

#if C_FPU_X86
#include "fpu_instructions_x86.h"
#elif defined(HAS_LONG_DOUBLE)
#include "fpu_instructions_longdouble.h"
#else
#include "fpu_instructions.h"
#endif

/* WATCHIT : ALWAYS UPDATE REGISTERS BEFORE AND AFTER USING THEM 
			STATUS WORD =>	FPU_SET_TOP(TOP) BEFORE a read
			TOP=FPU_GET_TOP() after a write;
			*/

static void EATREE(Bitu _rm){
	Bitu group=(_rm >> 3) & 7;
	switch(group){
		case 0x00:	/* FADD */
			FPU_FADD_EA(TOP);
			break;
		case 0x01:	/* FMUL  */
			FPU_FMUL_EA(TOP);
			break;
		case 0x02:	/* FCOM */
			FPU_FCOM_EA(TOP);
			break;
		case 0x03:	/* FCOMP */
			FPU_FCOM_EA(TOP);
			FPU_FPOP();
			break;
		case 0x04:	/* FSUB */
			FPU_FSUB_EA(TOP);
			break;
		case 0x05:	/* FSUBR */
			FPU_FSUBR_EA(TOP);
			break;
		case 0x06:	/* FDIV */
			FPU_FDIV_EA(TOP);
			break;
		case 0x07:	/* FDIVR */
			FPU_FDIVR_EA(TOP);
			break;
		default:
			break;
	}
}

void FPU_ESC0_EA(Bitu rm,PhysPt addr) {
	/* REGULAR TREE WITH 32 BITS REALS */
	FPU_FLD_F32_EA(addr);
	EATREE(rm);
}

void FPU_ESC0_Normal(Bitu rm) {
	Bitu group=(rm >> 3) & 7;
	Bitu sub=(rm & 7);
	switch (group){
	case 0x00:		/* FADD ST,STi */
		FPU_FADD(TOP,STV(sub));
		break;
	case 0x01:		/* FMUL  ST,STi */
		FPU_FMUL(TOP,STV(sub));
		break;
	case 0x02:		/* FCOM  STi */
		FPU_FCOM(TOP,STV(sub));
		break;
	case 0x03:		/* FCOMP STi */
		FPU_FCOM(TOP,STV(sub));
		FPU_FPOP();
		break;
	case 0x04:		/* FSUB  ST,STi */
		FPU_FSUB(TOP,STV(sub));
		break;	
	case 0x05:		/* FSUBR ST,STi */
		FPU_FSUBR(TOP,STV(sub));
		break;
	case 0x06:		/* FDIV  ST,STi */
		FPU_FDIV(TOP,STV(sub));
		break;
	case 0x07:		/* FDIVR ST,STi */
		FPU_FDIVR(TOP,STV(sub));
		break;
	default:
		break;
	}
}

void FPU_ESC1_EA(Bitu rm,PhysPt addr) {
// floats
	Bitu group=(rm >> 3) & 7;
	Bitu sub=(rm & 7);
	switch(group){
	case 0x00: /* FLD float*/
		{
			unsigned char old_TOP = TOP;

			try {
				FPU_PREP_PUSH();
				FPU_FLD_F32(addr,TOP);
			}
            catch (const GuestPageFaultException& pf) {
				(void)pf;
				TOP = old_TOP;
				throw;
			}
		}
		break;
	case 0x01: /* UNKNOWN */
		LOG(LOG_FPU,LOG_WARN)("ESC EA 1:Unhandled group %d subfunction %d",(int)group,(int)sub);
		break;
	case 0x02: /* FST float*/
		FPU_FST_F32(addr);
		break;
	case 0x03: /* FSTP float*/
		FPU_FST_F32(addr);
		FPU_FPOP();
		break;
	case 0x04: /* FLDENV */
		FPU_FLDENV(addr);
		break;
	case 0x05: /* FLDCW */
		FPU_FLDCW(addr);
		break;
	case 0x06: /* FSTENV */
		FPU_FSTENV(addr);
		break;
	case 0x07:  /* FNSTCW*/
		mem_writew(addr,fpu.cw);
		break;
	default:
		LOG(LOG_FPU,LOG_WARN)("ESC EA 1:Unhandled group %d subfunction %d",(int)group,(int)sub);
		break;
	}
}

void FPU_ESC1_Normal(Bitu rm) {
	Bitu group=(rm >> 3) & 7;
	Bitu sub=(rm & 7);
	switch (group){
	case 0x00: /* FLD STi */
		{
			Bitu reg_from=STV(sub);
			FPU_PREP_PUSH();
			FPU_FST(reg_from, TOP);
			break;
		}
	case 0x01: /* FXCH STi */
		FPU_FXCH(TOP,STV(sub));
		break;
	case 0x02: /* FNOP */
		FPU_FNOP();
		break;
	case 0x03: /* FSTP STi */
		FPU_FST(TOP,STV(sub));
		FPU_FPOP();
		break;   
	case 0x04:
		switch(sub){
		case 0x00:       /* FCHS */
			FPU_FCHS();
			break;
		case 0x01:       /* FABS */
			FPU_FABS();
			break;
		case 0x02:       /* UNKNOWN */
		case 0x03:       /* ILLEGAL */
			LOG(LOG_FPU,LOG_WARN)("ESC 1:Unhandled group %X subfunction %X",(int)group,(int)sub);
			break;
		case 0x04:       /* FTST */
			FPU_FTST();
			break;
		case 0x05:       /* FXAM */
			FPU_FXAM();
			break;
		case 0x06:       /* FTSTP (cyrix)*/
		case 0x07:       /* UNKNOWN */
			LOG(LOG_FPU,LOG_WARN)("ESC 1:Unhandled group %X subfunction %X",(int)group,(int)sub);
			break;
		}
		break;
	case 0x05:
		switch(sub){	
		case 0x00:       /* FLD1 */
			FPU_FLD1();
			break;
		case 0x01:       /* FLDL2T */
			FPU_FLDL2T();
			break;
		case 0x02:       /* FLDL2E */
			FPU_FLDL2E();
			break;
		case 0x03:       /* FLDPI */
			FPU_FLDPI();
			break;
		case 0x04:       /* FLDLG2 */
			FPU_FLDLG2();
			break;
		case 0x05:       /* FLDLN2 */
			FPU_FLDLN2();
			break;
		case 0x06:       /* FLDZ*/
			FPU_FLDZ();
			break;
		case 0x07:       /* ILLEGAL */
			LOG(LOG_FPU,LOG_WARN)("ESC 1:Unhandled group %X subfunction %X",(int)group,(int)sub);
			break;
		}
		break;
	case 0x06:
		switch(sub){
		case 0x00:	/* F2XM1 */
			FPU_F2XM1();
			break;
		case 0x01:	/* FYL2X */
			FPU_FYL2X();
			break;
		case 0x02:	/* FPTAN  */
			FPU_FPTAN();
			break;
		case 0x03:	/* FPATAN */
			FPU_FPATAN();
			break;
		case 0x04:	/* FXTRACT */
			FPU_FXTRACT();
			break;
		case 0x05:	/* FPREM1 */
			FPU_FPREM1();
			break;
		case 0x06:	/* FDECSTP */
			TOP = (TOP - 1) & 7;
			break;
		case 0x07:	/* FINCSTP */
			TOP = (TOP + 1) & 7;
			break;
		default:
			LOG(LOG_FPU,LOG_WARN)("ESC 1:Unhandled group %X subfunction %X",(int)group,(int)sub);
			break;
		}
		break;
	case 0x07:
		switch(sub){
		case 0x00:		/* FPREM */
			FPU_FPREM();
			break;
		case 0x01:		/* FYL2XP1 */
			FPU_FYL2XP1();
			break;
		case 0x02:		/* FSQRT */
			FPU_FSQRT();
			break;
		case 0x03:		/* FSINCOS */
			FPU_FSINCOS();
			break;
		case 0x04:		/* FRNDINT */
			FPU_FRNDINT();
			break;
		case 0x05:		/* FSCALE */
			FPU_FSCALE();
			break;
		case 0x06:		/* FSIN */
			FPU_FSIN();
			break;
		case 0x07:		/* FCOS */
			FPU_FCOS();
			break;
		default:
			LOG(LOG_FPU,LOG_WARN)("ESC 1:Unhandled group %X subfunction %X",(int)group,(int)sub);
			break;
		}
		break;
		default:
			LOG(LOG_FPU,LOG_WARN)("ESC 1:Unhandled group %X subfunction %X",(int)group,(int)sub);
	}
}


void FPU_ESC2_EA(Bitu rm,PhysPt addr) {
	/* 32 bits integer operants */
	FPU_FLD_I32_EA(addr);
	EATREE(rm);
}

void FPU_ESC2_Normal(Bitu rm) {
	Bitu group=(rm >> 3) & 7;
	Bitu sub=(rm & 7);
	switch(group){
	case 0x00: /* FCMOVB STi */
		if (TFLG_B) FPU_FCMOV(TOP,STV(sub));
		break;
	case 0x01: /* FCMOVE STi */
		if (TFLG_Z) FPU_FCMOV(TOP,STV(sub));
		break;
	case 0x02: /* FCMOVBE STi */
		if (TFLG_BE) FPU_FCMOV(TOP,STV(sub));
		break;
	case 0x03: /* FCMOVU STi */
		if (TFLG_P) FPU_FCMOV(TOP,STV(sub));
		break;
	case 0x05:
		switch(sub){
		case 0x01:		/* FUCOMPP */
			FPU_FUCOM(TOP,STV(1));
			FPU_FPOP();
			FPU_FPOP();
			break;
		default:
			LOG(LOG_FPU,LOG_WARN)("ESC 2:Unhandled group %d subfunction %d",(int)group,(int)sub); 
			break;
		}
		break;
	default:
	   	LOG(LOG_FPU,LOG_WARN)("ESC 2:Unhandled group %d subfunction %d",(int)group,(int)sub);
		break;
	}
}


void FPU_ESC3_EA(Bitu rm,PhysPt addr) {
	Bitu group=(rm >> 3) & 7;
	Bitu sub=(rm & 7);

	switch(group){
	case 0x00:	/* FILD */
		{
			unsigned char old_TOP = TOP;

			try {
				FPU_PREP_PUSH();
				FPU_FLD_I32(addr,TOP);
			}
            catch (const GuestPageFaultException& pf) {
				(void)pf;
				TOP = old_TOP;
				throw;
			}
		}
		break;
	case 0x01:	/* FISTTP */
		LOG(LOG_FPU,LOG_WARN)("ESC 3 EA:Unhandled group %d subfunction %d",(int)group,(int)sub);
		break;
	case 0x02:	/* FIST */
		FPU_FST_I32(addr);
		break;
	case 0x03:	/* FISTP */
		FPU_FST_I32(addr);
		FPU_FPOP();
		break;
	case 0x05:	/* FLD 80 Bits Real */
		{
			unsigned char old_TOP = TOP;

			try {
				FPU_PREP_PUSH();
				FPU_FLD_F80(addr);
			}
            catch (const GuestPageFaultException& pf) {
				(void)pf;
				TOP = old_TOP;
				throw;
			}
		}
		break;
	case 0x07:	/* FSTP 80 Bits Real */
		FPU_FST_F80(addr);
		FPU_FPOP();
		break;
	default:
		LOG(LOG_FPU,LOG_WARN)("ESC 3 EA:Unhandled group %d subfunction %d",(int)group,(int)sub);
	}
}

void FPU_ESC3_Normal(Bitu rm) {
	Bitu group=(rm >> 3) & 7;
	Bitu sub=(rm & 7);
	switch (group) {
	case 0x00: /* FCMOVNB STi */
		if (TFLG_NB) FPU_FCMOV(TOP,STV(sub));
		break;
	case 0x01: /* FCMOVNE STi */
		if (TFLG_NZ) FPU_FCMOV(TOP,STV(sub));
		break;
	case 0x02: /* FCMOVNBE STi */
		if (TFLG_NBE) FPU_FCMOV(TOP,STV(sub));
		break;
	case 0x03: /* FCMOVNU STi */
		if (TFLG_NP) FPU_FCMOV(TOP,STV(sub));
		break;
	case 0x04:
		switch (sub) {
		case 0x00:				//FNENI
		case 0x01:				//FNDIS
			LOG(LOG_FPU,LOG_ERROR)("8087 only fpu code used esc 3: group 4: subfuntion :%d",(int)sub);
			break;
		case 0x02:				//FNCLEX FCLEX
			FPU_FCLEX();
			break;
		case 0x03:				//FNINIT FINIT
			FPU_FINIT();
			break;
		case 0x04:				//FNSETPM
		case 0x05:				//FRSTPM
//			LOG(LOG_FPU,LOG_ERROR)("80267 protected mode (un)set. Nothing done");
			FPU_FNOP();
			break;
		default:
			E_Exit("ESC 3:ILLEGAL OPCODE group %d subfunction %d",(int)group,(int)sub);
		}
		break;
	case 0x05:		/* FUCOMI STi */
		FPU_FUCOMI(TOP,STV(sub));
		break;
	case 0x06:		/* FCOMI STi */
		FPU_FCOMI(TOP,STV(sub));
		break;
	default:
		LOG(LOG_FPU,LOG_WARN)("ESC 3:Unhandled group %d subfunction %d",(int)group,(int)sub);
		break;
	}
	return;
}


void FPU_ESC4_EA(Bitu rm,PhysPt addr) {
	/* REGULAR TREE WITH 64 BITS REALS */
	FPU_FLD_F64_EA(addr);
	EATREE(rm);
}

void FPU_ESC4_Normal(Bitu rm) {
	/* LOOKS LIKE number 6 without popping */
	Bitu group=(rm >> 3) & 7;
	Bitu sub=(rm & 7);
	switch(group){
	case 0x00:	/* FADD STi,ST*/
		FPU_FADD(STV(sub),TOP);
		break;
	case 0x01:	/* FMUL STi,ST*/
		FPU_FMUL(STV(sub),TOP);
		break;
	case 0x02:  /* FCOM*/
		FPU_FCOM(TOP,STV(sub));
		break;
	case 0x03:  /* FCOMP*/
		FPU_FCOM(TOP,STV(sub));
		FPU_FPOP();
		break;
	case 0x04:  /* FSUBR STi,ST*/
		FPU_FSUBR(STV(sub),TOP);
		break;
	case 0x05:  /* FSUB  STi,ST*/
		FPU_FSUB(STV(sub),TOP);
		break;
	case 0x06:  /* FDIVR STi,ST*/
		FPU_FDIVR(STV(sub),TOP);
		break;
	case 0x07:  /* FDIV STi,ST*/
		FPU_FDIV(STV(sub),TOP);
		break;
	default:
		break;
	}
}

void FPU_ESC5_EA(Bitu rm,PhysPt addr) {
	Bitu group=(rm >> 3) & 7;
	Bitu sub=(rm & 7);
	switch(group){
	case 0x00:  /* FLD double real*/
		{
			unsigned char old_TOP = TOP;

			try {
				FPU_PREP_PUSH();
				FPU_FLD_F64(addr,TOP);
			}
            catch (const GuestPageFaultException& pf) {
				(void)pf;
				TOP = old_TOP;
				throw;
			}
		}
		break;
	case 0x01:  /* FISTTP longint*/
		LOG(LOG_FPU,LOG_WARN)("ESC 5 EA:Unhandled group %d subfunction %d",(int)group,(int)sub);
		break;
	case 0x02:   /* FST double real*/
		FPU_FST_F64(addr);
		break;
	case 0x03:	/* FSTP double real*/
		FPU_FST_F64(addr);
		FPU_FPOP();
		break;
	case 0x04:	/* FRSTOR */
		FPU_FRSTOR(addr);
		break;
	case 0x06:	/* FSAVE */
		FPU_FSAVE(addr);
		break;
	case 0x07:   /*FNSTSW    NG DISAGREES ON THIS*/
		FPU_SET_TOP(TOP);
		mem_writew(addr,fpu.sw);
		//seems to break all dos4gw games :)
		break;
	default:
		LOG(LOG_FPU,LOG_WARN)("ESC 5 EA:Unhandled group %d subfunction %d",(int)group,(int)sub);
	}
}

void FPU_ESC5_Normal(Bitu rm) {
	Bitu group=(rm >> 3) & 7;
	Bitu sub=(rm & 7);
	switch(group){
	case 0x00: /* FFREE STi */
		fpu.tags[STV(sub)]=TAG_Empty;
		break;
	case 0x01: /* FXCH STi*/
		FPU_FXCH(TOP,STV(sub));
		break;
	case 0x02: /* FST STi */
		FPU_FST(TOP,STV(sub));
		break;
	case 0x03:  /* FSTP STi*/
		FPU_FST(TOP,STV(sub));
		FPU_FPOP();
		break;
	case 0x04:	/* FUCOM STi */
		FPU_FUCOM(TOP,STV(sub));
		break;
	case 0x05:	/*FUCOMP STi */
		FPU_FUCOM(TOP,STV(sub));
		FPU_FPOP();
		break;
	default:
	LOG(LOG_FPU,LOG_WARN)("ESC 5:Unhandled group %d subfunction %d",(int)group,(int)sub);
	break;
	}
}

void FPU_ESC6_EA(Bitu rm,PhysPt addr) {
	/* 16 bit (word integer) operants */
	FPU_FLD_I16_EA(addr);
	EATREE(rm);
}

void FPU_ESC6_Normal(Bitu rm) {
	/* all P variants working only on registers */
	/* get top before switch and pop afterwards */
	Bitu group=(rm >> 3) & 7;
	Bitu sub=(rm & 7);
	switch(group){
	case 0x00:	/*FADDP STi,ST*/
		FPU_FADD(STV(sub),TOP);
		break;
	case 0x01:	/* FMULP STi,ST*/
		FPU_FMUL(STV(sub),TOP);
		break;
	case 0x02:  /* FCOMP5*/
		FPU_FCOM(TOP,STV(sub));
		break;	/* TODO IS THIS ALLRIGHT ????????? */
	case 0x03:  /*FCOMPP*/
		if(sub != 1) {
			LOG(LOG_FPU,LOG_WARN)("ESC 6:Unhandled group %d subfunction %d",(int)group,(int)sub);
			return;
		}
		FPU_FCOM(TOP,STV(1));
		FPU_FPOP(); /* extra pop at the bottom*/
		break;
	case 0x04:  /* FSUBRP STi,ST*/
		FPU_FSUBR(STV(sub),TOP);
		break;
	case 0x05:  /* FSUBP  STi,ST*/
		FPU_FSUB(STV(sub),TOP);
		break;
	case 0x06:	/* FDIVRP STi,ST*/
		FPU_FDIVR(STV(sub),TOP);
		break;
	case 0x07:  /* FDIVP STi,ST*/
		FPU_FDIV(STV(sub),TOP);
		break;
	default:
		break;
	}
	FPU_FPOP();		
}


void FPU_ESC7_EA(Bitu rm,PhysPt addr) {
	Bitu group=(rm >> 3) & 7;
	Bitu sub=(rm & 7);
	switch(group){
	case 0x00:  /* FILD Bit16s */
		{
			unsigned char old_TOP = TOP;

			try {
				FPU_PREP_PUSH();
				FPU_FLD_I16(addr,TOP);
			}
            catch (const GuestPageFaultException& pf) {
				(void)pf;
				TOP = old_TOP;
				throw;
			}
		}
		break;
	case 0x01:
		LOG(LOG_FPU,LOG_WARN)("ESC 7 EA:Unhandled group %d subfunction %d",(int)group,(int)sub);
		break;
	case 0x02:   /* FIST Bit16s */
		FPU_FST_I16(addr);
		break;
	case 0x03:	/* FISTP Bit16s */
		FPU_FST_I16(addr);
		FPU_FPOP();
		break;
	case 0x04:   /* FBLD packed BCD */
		{
			unsigned char old_TOP = TOP;

			try {
				FPU_PREP_PUSH();
				FPU_FBLD(addr,TOP);
			}
            catch (const GuestPageFaultException& pf) {
				(void)pf;
				TOP = old_TOP;
				throw;
			}
		}
		break;
	case 0x05:  /* FILD Bit64s */
		{
			unsigned char old_TOP = TOP;

			try {
				FPU_PREP_PUSH();
				FPU_FLD_I64(addr,TOP);
			}
            catch (const GuestPageFaultException& pf) {
				(void)pf;
				TOP = old_TOP;
				throw;
			}
		}
		break;
	case 0x06:	/* FBSTP packed BCD */
		FPU_FBST(addr);
		FPU_FPOP();
		break;
	case 0x07:  /* FISTP Bit64s */
		FPU_FST_I64(addr);
		FPU_FPOP();
		break;
	default:
		LOG(LOG_FPU,LOG_WARN)("ESC 7 EA:Unhandled group %d subfunction %d",(int)group,(int)sub);
		break;
	}
}

void FPU_ESC7_Normal(Bitu rm) {
	Bitu group=(rm >> 3) & 7;
	Bitu sub=(rm & 7);
	switch (group){
	case 0x00: /* FFREEP STi*/
		fpu.tags[STV(sub)]=TAG_Empty;
		FPU_FPOP();
		break;
	case 0x01: /* FXCH STi*/
		FPU_FXCH(TOP,STV(sub));
		break;
	case 0x02:  /* FSTP STi*/
	case 0x03:  /* FSTP STi*/
		FPU_FST(TOP,STV(sub));
		FPU_FPOP();
		break;
	case 0x04:
		switch(sub){
			case 0x00:     /* FNSTSW AX*/
				FPU_SET_TOP(TOP);
				reg_ax = fpu.sw;
				break;
			default:
				LOG(LOG_FPU,LOG_WARN)("ESC 7:Unhandled group %d subfunction %d",(int)group,(int)sub);
				break;
		}
		break;
	case 0x05:		/* FUCOMIP STi */
		FPU_FUCOMI(TOP,STV(sub));
		FPU_FPOP();
		break;
	case 0x06:		/* FCOMIP STi */
		FPU_FCOMI(TOP,STV(sub));
		FPU_FPOP();
		break;
	default:
		LOG(LOG_FPU,LOG_WARN)("ESC 7:Unhandled group %d subfunction %d",(int)group,(int)sub);
		break;
	}
}

// test routine at startup to make sure our typedef struct bitfields
// line up with the host's definition of a 32-bit single-precision
// floating point value.
void FPU_Selftest_32() {
	struct ftest {
		const char*	name;
		float		val;
		int		exponent:15;
		unsigned int	sign:1;
		uint32_t	mantissa;
	};
	static const struct ftest test[] = {
		// name			// val		// exponent (no bias)		// sign		// 23-bit mantissa without 23rd implied bit (max 2^23-1 = 0x7FFFFF)
		{"0.0f",		0.0f,		-FPU_Reg_32_exponent_bias,	0,		0x000000},	// IEEE standard way to encode zero
		{"1.0f",		1.0f,		0,				0,		0x000000},	// 1.0 x 2^0 = 1.0 x 1 = 1.0
		{"2.0f",		2.0f,		1,				0,		0x000000},	// 1.0 x 2^1 = 1.0 x 2 = 2.0
		{"3.0f",		3.0f,		1,				0,		0x400000},	// 1.5 x 2^1 = 1.5 x 2 = 3.0
		{"4.0f",		4.0f,		2,				0,		0x000000},	// 1.0 x 2^2 = 1.0 x 4 = 4.0
		{"-1.0f",		-1.0f,		0,				1,		0x000000},	// 1.0 x 2^0 = 1.0 x 1 = 1.0
		{"-2.0f",		-2.0f,		1,				1,		0x000000},	// 1.0 x 2^1 = 1.0 x 2 = 2.0
		{"-3.0f",		-3.0f,		1,				1,		0x400000},	// 1.5 x 2^1 = 1.5 x 2 = 3.0
		{"-4.0f",		-4.0f,		2,				1,		0x000000}	// 1.0 x 2^2 = 1.0 x 4 = 4.0
	};
	static const size_t tests = sizeof(test) / sizeof(test[0]);
	FPU_Reg_32 ft;

	if (sizeof(ft) < 4) {
		LOG(LOG_FPU,LOG_WARN)("FPU32 sizeof(reg32) < 4 bytes");
		return;
	}
	if (sizeof(float) != 4) {
		LOG(LOG_FPU,LOG_WARN)("FPU32 sizeof(float) != 4 bytes your host is weird");
		return;
	}

	// make sure bitfields line up
	ft.raw = 1UL << 31UL;
	if (ft.f.sign != 1 || ft.f.exponent != 0 || ft.f.mantissa != 0) {
		LOG(LOG_FPU,LOG_WARN)("FPU32 bitfield test #1 failed");
		return;
	}
	ft.raw = 1UL << 23UL;
	if (ft.f.sign != 0 || ft.f.exponent != 1 || ft.f.mantissa != 0) {
		LOG(LOG_FPU,LOG_WARN)("FPU32 bitfield test #2 failed");
		return;
	}
	ft.raw = 1UL << 0UL;
	if (ft.f.sign != 0 || ft.f.exponent != 0 || ft.f.mantissa != 1) {
		LOG(LOG_FPU,LOG_WARN)("FPU32 bitfield test #3 failed");
		return;
	}

	// carry out tests
	for (size_t t=0;t < tests;t++) {
		ft.v = test[t].val; FPU_Reg_m_barrier();
		if (((int)ft.f.exponent - FPU_Reg_32_exponent_bias) != test[t].exponent ||
			ft.f.sign != test[t].sign || ft.f.mantissa != test[t].mantissa) {
			LOG(LOG_FPU,LOG_WARN)("FPU32 selftest fail stage %s",test[t].name);
			LOG(LOG_FPU,LOG_WARN)("  expected t.v = %.10f t.s=%u t.exp=%d t.mantissa=%u",
				test[t].val,
				test[t].sign,
				(int)test[t].exponent,
				(unsigned int)test[t].mantissa);
			goto dump;
		}
	}

	LOG(LOG_FPU,LOG_DEBUG)("FPU32 selftest passed");
	return;
dump:
	LOG(LOG_FPU,LOG_WARN)("Result: t.v = %.10f t.s=%u t.exp=%d t.mantissa=%u",
		ft.v,
		ft.f.sign,
		(int)ft.f.exponent - FPU_Reg_32_exponent_bias,
		(unsigned int)ft.f.mantissa);
}

// test routine at startup to make sure our typedef struct bitfields
// line up with the host's definition of a 64-bit double-precision
// floating point value.
void FPU_Selftest_64() {
	struct ftest {
		const char*	name;
		double		val;
		int		exponent:15;
		unsigned int	sign:1;
		uint64_t	mantissa;
	};
	static const struct ftest test[] = {
		// name			// val		// exponent (no bias)		// sign		// 52-bit mantissa without 52rd implied bit (max 2^52-1 = 0x1FFFFFFFFFFFFF)
		{"0.0d",		0.0,		-FPU_Reg_64_exponent_bias,	0,		0x0000000000000ULL},	// IEEE standard way to encode zero
		{"1.0d",		1.0,		0,				0,		0x0000000000000ULL},	// 1.0 x 2^0 = 1.0 x 1 = 1.0
		{"2.0d",		2.0,		1,				0,		0x0000000000000ULL},	// 1.0 x 2^1 = 1.0 x 2 = 2.0
		{"3.0d",		3.0,		1,				0,		0x8000000000000ULL},	// 1.5 x 2^1 = 1.5 x 2 = 3.0
		{"4.0d",		4.0,		2,				0,		0x0000000000000ULL},	// 1.0 x 2^2 = 1.0 x 4 = 4.0
		{"-1.0d",		-1.0,		0,				1,		0x0000000000000ULL},	// 1.0 x 2^0 = 1.0 x 1 = 1.0
		{"-2.0d",		-2.0,		1,				1,		0x0000000000000ULL},	// 1.0 x 2^1 = 1.0 x 2 = 2.0
		{"-3.0d",		-3.0,		1,				1,		0x8000000000000ULL},	// 1.5 x 2^1 = 1.5 x 2 = 3.0
		{"-4.0d",		-4.0,		2,				1,		0x0000000000000ULL}	// 1.0 x 2^2 = 1.0 x 4 = 4.0
	};
	static const size_t tests = sizeof(test) / sizeof(test[0]);
	FPU_Reg_64 ft;

	if (sizeof(ft) < 8) {
		LOG(LOG_FPU,LOG_WARN)("FPU64 sizeof(reg64) < 8 bytes");
		return;
	}
	if (sizeof(double) != 8) {
		LOG(LOG_FPU,LOG_WARN)("FPU64 sizeof(float) != 8 bytes your host is weird");
		return;
	}

	// make sure bitfields line up
	ft.raw = 1ULL << 63ULL;
	if (ft.f.sign != 1 || ft.f.exponent != 0 || ft.f.mantissa != 0) {
		LOG(LOG_FPU,LOG_WARN)("FPU64 bitfield test #1 failed");
		return;
	}
	ft.raw = 1ULL << 52ULL;
	if (ft.f.sign != 0 || ft.f.exponent != 1 || ft.f.mantissa != 0) {
		LOG(LOG_FPU,LOG_WARN)("FPU64 bitfield test #2 failed");
		return;
	}
	ft.raw = 1ULL << 0ULL;
	if (ft.f.sign != 0 || ft.f.exponent != 0 || ft.f.mantissa != 1) {
		LOG(LOG_FPU,LOG_WARN)("FPU64 bitfield test #3 failed");
		return;
	}

	for (size_t t=0;t < tests;t++) {
		ft.v = test[t].val; FPU_Reg_m_barrier();
		if (((int)ft.f.exponent - FPU_Reg_64_exponent_bias) != test[t].exponent ||
			ft.f.sign != test[t].sign || ft.f.mantissa != test[t].mantissa) {
			LOG(LOG_FPU,LOG_WARN)("FPU64 selftest fail stage %s",test[t].name);
			LOG(LOG_FPU,LOG_WARN)("  expected t.v = %.10f t.s=%u t.exp=%d t.mantissa=%llu (0x%llx)",
				test[t].val,
				test[t].sign,
				(int)test[t].exponent,
				(unsigned long long)test[t].mantissa,
				(unsigned long long)test[t].mantissa);
			goto dump;
		}
	}

	LOG(LOG_FPU,LOG_DEBUG)("FPU64 selftest passed");
	return;
dump:
	LOG(LOG_FPU,LOG_WARN)("Result: t.v = %.10f t.s=%u t.exp=%d t.mantissa=%llu (0x%llx)",
		ft.v,
		(int)ft.f.sign,
		(int)ft.f.exponent - FPU_Reg_64_exponent_bias,
		(unsigned long long)ft.f.mantissa,
		(unsigned long long)ft.f.mantissa);
}

// test routine at startup to make sure our typedef struct bitfields
// line up with the host's definition of a 80-bit extended-precision
// floating point value (if the host is i686, x86_64, or any other
// host with the same definition of long double).
void FPU_Selftest_80() {
#if defined(HAS_LONG_DOUBLE)
	// we're assuming "long double" means the Intel 80x87 extended precision format, which is true when using
	// GCC on Linux i686 and x86_64 hosts.
	//
	// I understand that other platforms (PowerPC, Sparc, etc) might have other ideas on what makes "long double"
	// and I also understand Microsoft Visual C++ treats long double the same as double. We will disable this
	// test with #ifdefs when compiling for platforms where long double doesn't mean 80-bit extended precision.
	struct ftest {
		const char*	name;
		long double	val;
		int		exponent:15;
		unsigned int	sign:1;
		uint64_t	mantissa;
	};
	static const struct ftest test[] = {
		// name			// val		// exponent (no bias)		// sign		// 64-bit mantissa WITH whole integer bit #63
		{"0.0L",		0.0,		-FPU_Reg_80_exponent_bias,	0,		0x0000000000000000ULL},	// IEEE standard way to encode zero
		{"1.0L",		1.0,		0,				0,		0x8000000000000000ULL},	// 1.0 x 2^0 = 1.0 x 1 = 1.0
		{"2.0L",		2.0,		1,				0,		0x8000000000000000ULL},	// 1.0 x 2^1 = 1.0 x 2 = 2.0
		{"3.0L",		3.0,		1,				0,		0xC000000000000000ULL},	// 1.5 x 2^1 = 1.5 x 2 = 3.0
		{"4.0L",		4.0,		2,				0,		0x8000000000000000ULL},	// 1.0 x 2^2 = 1.0 x 4 = 4.0
		{"-1.0L",		-1.0,		0,				1,		0x8000000000000000ULL},	// 1.0 x 2^0 = 1.0 x 1 = 1.0
		{"-2.0L",		-2.0,		1,				1,		0x8000000000000000ULL},	// 1.0 x 2^1 = 1.0 x 2 = 2.0
		{"-3.0L",		-3.0,		1,				1,		0xC000000000000000ULL},	// 1.5 x 2^1 = 1.5 x 2 = 3.0
		{"-4.0L",		-4.0,		2,				1,		0x8000000000000000ULL}	// 1.0 x 2^2 = 1.0 x 4 = 4.0
	};
	static const size_t tests = sizeof(test) / sizeof(test[0]);
#endif
	FPU_Reg_80 ft;

	if (sizeof(ft) < 10) {
		LOG(LOG_FPU,LOG_WARN)("FPU80 sizeof(reg80) < 10 bytes");
		return;
	}
#if defined(HAS_LONG_DOUBLE)
	if (sizeof(long double) == sizeof(double)) {
		LOG(LOG_FPU,LOG_WARN)("FPU80 sizeof(long double) == sizeof(double) so your compiler just makes it an alias. skipping tests. please recompile with proper config.");
		return;
	}
	else if (sizeof(long double) < 10 || sizeof(long double) > 16) {
		// NTS: We can't assume 10 bytes. GCC on i686 makes long double 12 or 16 bytes long for alignment
		//      even though only 80 bits (10 bytes) are used.
		LOG(LOG_FPU,LOG_WARN)("FPU80 sizeof(float) < 10 bytes your host is weird");
		return;
	}
#endif

	// make sure bitfields line up
	ft.raw.l = 0;
	ft.raw.h = 1U << 15U;
	if (ft.f.sign != 1 || ft.f.exponent != 0 || ft.f.mantissa != 0) {
		LOG(LOG_FPU,LOG_WARN)("FPU80 bitfield test #1 failed. h=%04x l=%016llx",(unsigned int)ft.raw.h,(unsigned long long)ft.raw.l);
		return;
	}
	ft.raw.l = 0;
	ft.raw.h = 1U << 0U;
	if (ft.f.sign != 0 || ft.f.exponent != 1 || ft.f.mantissa != 0) {
		LOG(LOG_FPU,LOG_WARN)("FPU80 bitfield test #2 failed. h=%04x l=%016llx",(unsigned int)ft.raw.h,(unsigned long long)ft.raw.l);
		return;
	}
	ft.raw.l = 1ULL << 0ULL;
	ft.raw.h = 0;
	if (ft.f.sign != 0 || ft.f.exponent != 0 || ft.f.mantissa != 1) {
		LOG(LOG_FPU,LOG_WARN)("FPU80 bitfield test #3 failed. h=%04x l=%016llx",(unsigned int)ft.raw.h,(unsigned long long)ft.raw.l);
		return;
	}

#if defined(HAS_LONG_DOUBLE)
	for (size_t t=0;t < tests;t++) {
		ft.v = test[t].val; FPU_Reg_m_barrier();
		if (((int)ft.f.exponent - FPU_Reg_80_exponent_bias) != test[t].exponent ||
			ft.f.sign != test[t].sign || ft.f.mantissa != test[t].mantissa) {
			LOG(LOG_FPU,LOG_WARN)("FPU80 selftest fail stage %s",test[t].name);
			LOG(LOG_FPU,LOG_WARN)("  expected t.v = %.10Lf t.s=%u t.exp=%d t.mantissa=%llu (0x%llx)",
				test[t].val,
				test[t].sign,
				(int)test[t].exponent,
				(unsigned long long)test[t].mantissa,
				(unsigned long long)test[t].mantissa);
			goto dump;
		}
	}

	LOG(LOG_FPU,LOG_DEBUG)("FPU80 selftest passed");
	return;
dump:
	LOG(LOG_FPU,LOG_WARN)("Result: t.v = %.10Lf t.s=%u t.exp=%d t.mantissa=%llu (0x%llx)",
		ft.v,
		(int)ft.f.sign,
		(int)ft.f.exponent - FPU_Reg_64_exponent_bias,
		(unsigned long long)ft.f.mantissa,
		(unsigned long long)ft.f.mantissa);
#else
	LOG(LOG_FPU,LOG_DEBUG)("FPU80 selftest skipped, compiler does not have long double as 80-bit IEEE");
#endif
}

void FPU_Selftest() {
	FPU_Reg freg;

	/* byte order test */
	freg.ll = 0x0123456789ABCDEFULL;
#ifndef WORDS_BIGENDIAN
	if (freg.l.lower != 0x89ABCDEFUL || freg.l.upper != 0x01234567UL) {
		LOG(LOG_FPU,LOG_WARN)("FPU_Reg field order is wrong. ll=0x%16llx l=0x%08lx h=0x%08lx",
			(unsigned long long)freg.ll,	(unsigned long)freg.l.lower,	(unsigned long)freg.l.upper);
	}
#else
	if (freg.l.upper != 0x89ABCDEFUL || freg.l.lower != 0x01234567UL) {
		LOG(LOG_FPU,LOG_WARN)("FPU_Reg field order is wrong. ll=0x%16llx l=0x%08lx h=0x%08lx",
			(unsigned long long)freg.ll,	(unsigned long)freg.l.lower,	(unsigned long)freg.l.upper);
	}
#endif

#if C_FPU_X86
    LOG(LOG_FPU,LOG_NORMAL)("FPU core: x86 FPU");
#elif defined(HAS_LONG_DOUBLE)
    LOG(LOG_FPU,LOG_NORMAL)("FPU core: long double FPU");
#else
    LOG(LOG_FPU,LOG_NORMAL)("FPU core: double FPU (caution: possible precision errors)");
#endif

	FPU_Selftest_32();
	FPU_Selftest_64();
	FPU_Selftest_80();
}

void FPU_Init() {
	LOG(LOG_MISC,LOG_DEBUG)("Initializing FPU");

	FPU_Selftest();
	FPU_FINIT();
}

#endif

//save state support
namespace
{
class SerializeFpu : public SerializeGlobalPOD
{
public:
    SerializeFpu() : SerializeGlobalPOD("FPU")
    {
        registerPOD(fpu);
    }
} dummy;
}
