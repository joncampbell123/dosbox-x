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
#include "cross.h"
#include "mem.h"
#include "fpu.h"
#include "cpu.h"


static void FPU_FDECSTP(){
	TOP = (TOP - 1) & 7;
}

static void FPU_FINCSTP(){
	TOP = (TOP + 1) & 7;
}

static void FPU_FNSTCW(PhysPt addr){
	mem_writew(addr,fpu.cw);
}

static void FPU_FFREE(Bitu st) {
	fpu.tags[st]=TAG_Empty;
}


#if C_FPU_X86
#include "../../fpu/fpu_instructions_x86.h"
#else
#include "../../fpu/fpu_instructions.h"
#endif


#define dyn_fpu_top() {				\
	gen_protectflags();				\
	gen_load_host(&TOP,DREG(EA),4); \
	gen_dop_word_imm(DOP_ADD,true,DREG(EA),decode.modrm.rm);	\
	gen_dop_word_imm(DOP_AND,true,DREG(EA),7);	\
	gen_load_host(&TOP,DREG(TMPB),4);			\
}

static void dyn_save_fpu_top_for_pagefault() {
	gen_load_host(&TOP,DREG(TMPB),4); 
	gen_save_host(&core_dyn.pagefault_old_fpu_top, DREG(TMPB), 4);
	decode.pf_restore.data.fpu_top = 1;
}

static void dyn_eatree() {
	Bitu group=(decode.modrm.val >> 3) & 7;
	switch (group){
	case 0x00:		/* FADD ST,STi */
		gen_call_function((void*)&FPU_FADD_EA,"%Drd",DREG(TMPB));
		break;
	case 0x01:		/* FMUL  ST,STi */
		gen_call_function((void*)&FPU_FMUL_EA,"%Drd",DREG(TMPB));
		break;
	case 0x02:		/* FCOM  STi */
		gen_call_function((void*)&FPU_FCOM_EA,"%Drd",DREG(TMPB));
		break;
	case 0x03:		/* FCOMP STi */
		gen_call_function((void*)&FPU_FCOM_EA,"%Drd",DREG(TMPB));
		gen_call_function((void*)&FPU_FPOP,"");
		break;
	case 0x04:		/* FSUB  ST,STi */
		gen_call_function((void*)&FPU_FSUB_EA,"%Drd",DREG(TMPB));
		break;	
	case 0x05:		/* FSUBR ST,STi */
		gen_call_function((void*)&FPU_FSUBR_EA,"%Drd",DREG(TMPB));
		break;
	case 0x06:		/* FDIV  ST,STi */
		gen_call_function((void*)&FPU_FDIV_EA,"%Drd",DREG(TMPB));
		break;
	case 0x07:		/* FDIVR ST,STi */
		gen_call_function((void*)&FPU_FDIVR_EA,"%Drd",DREG(TMPB));
		break;
	default:
		break;
	}
}

static void dyn_fpu_esc0(){
	dyn_get_modrm(); 
	if (decode.modrm.val >= 0xc0) { 
		dyn_fpu_top();
		Bitu group=(decode.modrm.val >> 3) & 7;
		//Bitu sub=(decode.modrm.val & 7);
		switch (group){
		case 0x00:		//FADD ST,STi /
			gen_call_function((void*)&FPU_FADD,"%Drd%Drd",DREG(TMPB),DREG(EA));
			break;
		case 0x01:		// FMUL  ST,STi /
			gen_call_function((void*)&FPU_FMUL,"%Drd%Drd",DREG(TMPB),DREG(EA));
			break;
		case 0x02:		// FCOM  STi /
			gen_call_function((void*)&FPU_FCOM,"%Drd%Drd",DREG(TMPB),DREG(EA));
			break;
		case 0x03:		// FCOMP STi /
			gen_call_function((void*)&FPU_FCOM,"%Drd%Drd",DREG(TMPB),DREG(EA));
			gen_call_function((void*)&FPU_FPOP,"");
			break;
		case 0x04:		// FSUB  ST,STi /
			gen_call_function((void*)&FPU_FSUB,"%Drd%Drd",DREG(TMPB),DREG(EA));
			break;	
		case 0x05:		// FSUBR ST,STi /
			gen_call_function((void*)&FPU_FSUBR,"%Drd%Drd",DREG(TMPB),DREG(EA));
			break;
		case 0x06:		// FDIV  ST,STi /
			gen_call_function((void*)&FPU_FDIV,"%Drd%Drd",DREG(TMPB),DREG(EA));
			break;
		case 0x07:		// FDIVR ST,STi /
			gen_call_function((void*)&FPU_FDIVR,"%Drd%Drd",DREG(TMPB),DREG(EA));
			break;
		default:
			break;
		}
	} else { 
		dyn_fill_ea();
		dyn_call_function_pagefault_check((void*)&FPU_FLD_F32_EA,"%Drd",DREG(EA)); 
		gen_load_host(&TOP,DREG(TMPB),4);
		dyn_eatree();
	}
}

static void dyn_fpu_esc1(){
	dyn_get_modrm();  
	if (decode.modrm.val >= 0xc0) { 
		Bitu group=(decode.modrm.val >> 3) & 7;
		Bitu sub=(decode.modrm.val & 7);
		switch (group){
		case 0x00: /* FLD STi */
			gen_protectflags(); 
			gen_load_host(&TOP,DREG(EA),4); 
			gen_dop_word_imm(DOP_ADD,true,DREG(EA),decode.modrm.rm); 
			gen_dop_word_imm(DOP_AND,true,DREG(EA),7); 
			gen_call_function((void*)&FPU_PREP_PUSH,""); 
			gen_load_host(&TOP,DREG(TMPB),4); 
			gen_call_function((void*)&FPU_FST,"%Drd%Drd",DREG(EA),DREG(TMPB));
			break;
		case 0x01: /* FXCH STi */
			dyn_fpu_top();
			gen_call_function((void*)&FPU_FXCH,"%Drd%Drd",DREG(TMPB),DREG(EA));
			break;
		case 0x02: /* FNOP */
			gen_call_function((void*)&FPU_FNOP,"");
			break;
		case 0x03: /* FSTP STi */
			dyn_fpu_top();
			gen_call_function((void*)&FPU_FST,"%Drd%Drd",DREG(TMPB),DREG(EA));
			gen_call_function((void*)&FPU_FPOP,"");
			break;   
		case 0x04:
			switch(sub){
			case 0x00:       /* FCHS */
				gen_call_function((void*)&FPU_FCHS,"");
				break;
			case 0x01:       /* FABS */
				gen_call_function((void*)&FPU_FABS,"");
				break;
			case 0x02:       /* UNKNOWN */
			case 0x03:       /* ILLEGAL */
				FPU_LOG_WARN(1,false,group,sub);
				break;
			case 0x04:       /* FTST */
				gen_call_function((void*)&FPU_FTST,"");
				break;
			case 0x05:       /* FXAM */
				gen_call_function((void*)&FPU_FXAM,"");
				break;
			case 0x06:       /* FTSTP (cyrix)*/
			case 0x07:       /* UNKNOWN */
				FPU_LOG_WARN(1,false,group,sub);
				break;
			}
			break;
		case 0x05:
			switch(sub){	
			case 0x00:       /* FLD1 */
				gen_call_function((void*)&FPU_FLD1,"");
				break;
			case 0x01:       /* FLDL2T */
				gen_call_function((void*)&FPU_FLDL2T,"");
				break;
			case 0x02:       /* FLDL2E */
				gen_call_function((void*)&FPU_FLDL2E,"");
				break;
			case 0x03:       /* FLDPI */
				gen_call_function((void*)&FPU_FLDPI,"");
				break;
			case 0x04:       /* FLDLG2 */
				gen_call_function((void*)&FPU_FLDLG2,"");
				break;
			case 0x05:       /* FLDLN2 */
				gen_call_function((void*)&FPU_FLDLN2,"");
				break;
			case 0x06:       /* FLDZ*/
				gen_call_function((void*)&FPU_FLDZ,"");
				break;
			case 0x07:       /* ILLEGAL */
				FPU_LOG_WARN(1,false,group,sub);
				break;
			}
			break;
		case 0x06:
			switch(sub){
			case 0x00:	/* F2XM1 */
				gen_call_function((void*)&FPU_F2XM1,"");
				break;
			case 0x01:	/* FYL2X */
				gen_call_function((void*)&FPU_FYL2X,"");
				break;
			case 0x02:	/* FPTAN  */
				gen_call_function((void*)&FPU_FPTAN,"");
				break;
			case 0x03:	/* FPATAN */
				gen_call_function((void*)&FPU_FPATAN,"");
				break;
			case 0x04:	/* FXTRACT */
				gen_call_function((void*)&FPU_FXTRACT,"");
				break;
			case 0x05:	/* FPREM1 */
				gen_call_function((void*)&FPU_FPREM1,"");
				break;
			case 0x06:	/* FDECSTP */
				gen_call_function((void*)&FPU_FDECSTP,"");
				break;
			case 0x07:	/* FINCSTP */
				gen_call_function((void*)&FPU_FINCSTP,"");
				break;
			default:
				FPU_LOG_WARN(1,false,group,sub);
				break;
			}
			break;
		case 0x07:
			switch(sub){
			case 0x00:		/* FPREM */
				gen_call_function((void*)&FPU_FPREM,"");
				break;
			case 0x01:		/* FYL2XP1 */
				gen_call_function((void*)&FPU_FYL2XP1,"");
				break;
			case 0x02:		/* FSQRT */
				gen_call_function((void*)&FPU_FSQRT,"");
				break;
			case 0x03:		/* FSINCOS */
				gen_call_function((void*)&FPU_FSINCOS,"");
				break;
			case 0x04:		/* FRNDINT */
				gen_call_function((void*)&FPU_FRNDINT,"");
				break;
			case 0x05:		/* FSCALE */
				gen_call_function((void*)&FPU_FSCALE,"");
				break;
			case 0x06:		/* FSIN */
				gen_call_function((void*)&FPU_FSIN,"");
				break;
			case 0x07:		/* FCOS */
				gen_call_function((void*)&FPU_FCOS,"");
				break;
			default:
				FPU_LOG_WARN(1,false,group,sub);
				break;
			}
			break;
		default:
			FPU_LOG_WARN(1,false,group,sub);
			break;
		}
	} else {
		Bitu group=(decode.modrm.val >> 3) & 7;
		Bitu sub=(decode.modrm.val & 7);
		dyn_fill_ea(); 
		switch(group){
		case 0x00: /* FLD float*/
			gen_protectflags(); 
			if (use_dynamic_core_with_paging) dyn_save_fpu_top_for_pagefault();
			gen_call_function((void*)&FPU_PREP_PUSH,"");
			gen_load_host(&TOP,DREG(TMPB),4); 
			dyn_call_function_pagefault_check((void*)&FPU_FLD_F32,"%Drd%Drd",DREG(EA),DREG(TMPB));
			break;
		case 0x01: /* UNKNOWN */
			FPU_LOG_WARN(1,true,group,sub);
			break;
		case 0x02: /* FST float*/
			dyn_call_function_pagefault_check((void*)&FPU_FST_F32,"%Drd",DREG(EA));
			break;
		case 0x03: /* FSTP float*/
			dyn_call_function_pagefault_check((void*)&FPU_FST_F32,"%Drd",DREG(EA));
			gen_call_function((void*)&FPU_FPOP,"");
			break;
		case 0x04: /* FLDENV */
			dyn_call_function_pagefault_check((void*)&FPU_FLDENV,"%Drd",DREG(EA));
			break;
		case 0x05: /* FLDCW */
			dyn_call_function_pagefault_check((void *)&FPU_FLDCW,"%Drd",DREG(EA));
			break;
		case 0x06: /* FSTENV */
			dyn_call_function_pagefault_check((void *)&FPU_FSTENV,"%Drd",DREG(EA));
			break;
		case 0x07:  /* FNSTCW*/
			dyn_call_function_pagefault_check((void *)&FPU_FNSTCW,"%Drd",DREG(EA));
			break;
		default:
			FPU_LOG_WARN(1,true,group,sub);
			break;
		}
	}
}

static void dyn_fpu_esc2(){
	dyn_get_modrm();  
	if (decode.modrm.val >= 0xc0) { 
		Bitu group=(decode.modrm.val >> 3) & 7;
		Bitu sub=(decode.modrm.val & 7);
		switch(group){
		case 0x05:
			switch(sub){
			case 0x01:		/* FUCOMPP */
				gen_protectflags(); 
				gen_load_host(&TOP,DREG(EA),4); 
				gen_dop_word_imm(DOP_ADD,true,DREG(EA),1); 
				gen_dop_word_imm(DOP_AND,true,DREG(EA),7); 
				gen_load_host(&TOP,DREG(TMPB),4); 
				gen_call_function((void *)&FPU_FUCOM,"%Drd%Drd",DREG(TMPB),DREG(EA));
				gen_call_function((void *)&FPU_FPOP,"");
				gen_call_function((void *)&FPU_FPOP,"");
				break;
			default:
				FPU_LOG_WARN(2,false,5,sub);
				break;
			}
			break;
		default:
			FPU_LOG_WARN(2,false,group,sub);
			break;
		}
	} else {
		dyn_fill_ea(); 
		dyn_call_function_pagefault_check((void*)&FPU_FLD_I32_EA,"%Drd",DREG(EA)); 
		gen_load_host(&TOP,DREG(TMPB),4); 
		dyn_eatree();
	}
}

static void dyn_fpu_esc3(){
	dyn_get_modrm();  
	if (decode.modrm.val >= 0xc0) { 
		Bitu group=(decode.modrm.val >> 3) & 7;
		Bitu sub=(decode.modrm.val & 7);
		switch (group) {
		case 0x04:
			switch (sub) {
			case 0x00:				//FNENI
			case 0x01:				//FNDIS
				LOG(LOG_FPU,LOG_ERROR)("8087 only fpu code used esc 3: group 4: subfuntion :%d",sub);
				break;
			case 0x02:				//FNCLEX FCLEX
				gen_call_function((void*)&FPU_FCLEX,"");
				break;
			case 0x03:				//FNINIT FINIT
				gen_call_function((void*)&FPU_FINIT,"");
				break;
			case 0x04:				//FNSETPM
			case 0x05:				//FRSTPM
//				LOG(LOG_FPU,LOG_ERROR)("80267 protected mode (un)set. Nothing done");
				break;
			default:
				E_Exit("ESC 3:ILLEGAL OPCODE group %d subfunction %d",group,sub);
			}
			break;
		default:
			FPU_LOG_WARN(3,false,group,sub);
			break;
		}
	} else {
		Bitu group=(decode.modrm.val >> 3) & 7;
		Bitu sub=(decode.modrm.val & 7);
		dyn_fill_ea(); 
		switch(group){
		case 0x00:	/* FILD */
			if (use_dynamic_core_with_paging) dyn_save_fpu_top_for_pagefault();
			gen_call_function((void*)&FPU_PREP_PUSH,"");
			gen_protectflags(); 
			gen_load_host(&TOP,DREG(TMPB),4); 
			dyn_call_function_pagefault_check((void*)&FPU_FLD_I32,"%Drd%Drd",DREG(EA),DREG(TMPB));
			break;
		case 0x01:	/* FISTTP */
			FPU_LOG_WARN(3,false,1,sub);
			break;
		case 0x02:	/* FIST */
			dyn_call_function_pagefault_check((void*)&FPU_FST_I32,"%Drd",DREG(EA));
			break;
		case 0x03:	/* FISTP */
			dyn_call_function_pagefault_check((void*)&FPU_FST_I32,"%Drd",DREG(EA));
			gen_call_function((void*)&FPU_FPOP,"");
			break;
		case 0x05:	/* FLD 80 Bits Real */
			if (use_dynamic_core_with_paging) dyn_save_fpu_top_for_pagefault();
			gen_call_function((void*)&FPU_PREP_PUSH,"");
			dyn_call_function_pagefault_check((void*)&FPU_FLD_F80,"%Drd",DREG(EA));
			break;
		case 0x07:	/* FSTP 80 Bits Real */
			dyn_call_function_pagefault_check((void*)&FPU_FST_F80,"%Drd",DREG(EA));
			gen_call_function((void*)&FPU_FPOP,"");
			break;
		default:
			FPU_LOG_WARN(3,true,group,sub);
			break;
		}
	}
}

static void dyn_fpu_esc4(){
	dyn_get_modrm();  
	Bitu group=(decode.modrm.val >> 3) & 7;
	//Bitu sub=(decode.modrm.val & 7);
	if (decode.modrm.val >= 0xc0) { 
		dyn_fpu_top();
		switch(group){
		case 0x00:	/* FADD STi,ST*/
			gen_call_function((void*)&FPU_FADD,"%Drd%Drd",DREG(EA),DREG(TMPB));
			break;
		case 0x01:	/* FMUL STi,ST*/
			gen_call_function((void*)&FPU_FMUL,"%Drd%Drd",DREG(EA),DREG(TMPB));
			break;
		case 0x02:  /* FCOM*/
			gen_call_function((void*)&FPU_FCOM,"%Drd%Drd",DREG(TMPB),DREG(EA));
			break;
		case 0x03:  /* FCOMP*/
			gen_call_function((void*)&FPU_FCOM,"%Drd%Drd",DREG(TMPB),DREG(EA));
			gen_call_function((void*)&FPU_FPOP,"");
			break;
		case 0x04:  /* FSUBR STi,ST*/
			gen_call_function((void*)&FPU_FSUBR,"%Drd%Drd",DREG(EA),DREG(TMPB));
			break;
		case 0x05:  /* FSUB  STi,ST*/
			gen_call_function((void*)&FPU_FSUB,"%Drd%Drd",DREG(EA),DREG(TMPB));
			break;
		case 0x06:  /* FDIVR STi,ST*/
			gen_call_function((void*)&FPU_FDIVR,"%Drd%Drd",DREG(EA),DREG(TMPB));
			break;
		case 0x07:  /* FDIV STi,ST*/
			gen_call_function((void*)&FPU_FDIV,"%Drd%Drd",DREG(EA),DREG(TMPB));
			break;
		default:
			break;
		}
	} else { 
		dyn_fill_ea(); 
		dyn_call_function_pagefault_check((void*)&FPU_FLD_F64_EA,"%Drd",DREG(EA)); 
		gen_load_host(&TOP,DREG(TMPB),4); 
		dyn_eatree();
	}
}

static void dyn_fpu_esc5(){
	dyn_get_modrm();  
	Bitu group=(decode.modrm.val >> 3) & 7;
	Bitu sub=(decode.modrm.val & 7);
	if (decode.modrm.val >= 0xc0) { 
		dyn_fpu_top();
		switch(group){
		case 0x00: /* FFREE STi */
			gen_call_function((void*)&FPU_FFREE,"%Drd",DREG(EA));
			break;
		case 0x01: /* FXCH STi*/
			gen_call_function((void*)&FPU_FXCH,"%Drd%Drd",DREG(TMPB),DREG(EA));
			break;
		case 0x02: /* FST STi */
			gen_call_function((void*)&FPU_FST,"%Drd%Drd",DREG(TMPB),DREG(EA));
			break;
		case 0x03:  /* FSTP STi*/
			gen_call_function((void*)&FPU_FST,"%Drd%Drd",DREG(TMPB),DREG(EA));
			gen_call_function((void*)&FPU_FPOP,"");
			break;
		case 0x04:	/* FUCOM STi */
			gen_call_function((void*)&FPU_FUCOM,"%Drd%Drd",DREG(TMPB),DREG(EA));
			break;
		case 0x05:	/*FUCOMP STi */
			gen_call_function((void*)&FPU_FUCOM,"%Drd%Drd",DREG(TMPB),DREG(EA));
			gen_call_function((void*)&FPU_FPOP,"");
			break;
		default:
			FPU_LOG_WARN(5,false,group,sub);
			break;
        }
		gen_releasereg(DREG(EA));
		gen_releasereg(DREG(TMPB));
	} else {
		dyn_fill_ea(); 
		switch(group){
		case 0x00:  /* FLD double real*/
			if (use_dynamic_core_with_paging) dyn_save_fpu_top_for_pagefault();
			gen_call_function((void*)&FPU_PREP_PUSH,"");
			gen_protectflags(); 
			gen_load_host(&TOP,DREG(TMPB),4); 
			dyn_call_function_pagefault_check((void*)&FPU_FLD_F64,"%Drd%Drd",DREG(EA),DREG(TMPB));
			break;
		case 0x01:  /* FISTTP longint*/
			FPU_LOG_WARN(5,true,1,sub);
			break;
		case 0x02:   /* FST double real*/
			dyn_call_function_pagefault_check((void*)&FPU_FST_F64,"%Drd",DREG(EA));
			break;
		case 0x03:	/* FSTP double real*/
			dyn_call_function_pagefault_check((void*)&FPU_FST_F64,"%Drd",DREG(EA));
			gen_call_function((void*)&FPU_FPOP,"");
			break;
		case 0x04:	/* FRSTOR */
			dyn_call_function_pagefault_check((void*)&FPU_FRSTOR,"%Drd",DREG(EA));
			break;
		case 0x06:	/* FSAVE */
			dyn_call_function_pagefault_check((void*)&FPU_FSAVE,"%Drd",DREG(EA));
			break;
		case 0x07:   /*FNSTSW */
			gen_protectflags(); 
			gen_load_host(&TOP,DREG(TMPB),4); 
			gen_call_function((void*)&FPU_SET_TOP,"%Dd",DREG(TMPB));
			gen_load_host(&fpu.sw,DREG(TMPB),4); 
			dyn_call_function_pagefault_check((void*)&mem_writew,"%Drd%Drd",DREG(EA),DREG(TMPB));
			break;
		default:
			FPU_LOG_WARN(5,true,group,sub);
			break;
		}
	}
}

static void dyn_fpu_esc6(){
	dyn_get_modrm();  
	Bitu group=(decode.modrm.val >> 3) & 7;
	Bitu sub=(decode.modrm.val & 7);
	if (decode.modrm.val >= 0xc0) { 
		dyn_fpu_top();
		switch(group){
		case 0x00:	/*FADDP STi,ST*/
			gen_call_function((void*)&FPU_FADD,"%Drd%Drd",DREG(EA),DREG(TMPB));
			break;
		case 0x01:	/* FMULP STi,ST*/
			gen_call_function((void*)&FPU_FMUL,"%Drd%Drd",DREG(EA),DREG(TMPB));
			break;
		case 0x02:  /* FCOMP5*/
			gen_call_function((void*)&FPU_FCOM,"%Drd%Drd",DREG(TMPB),DREG(EA));
			break;	/* TODO IS THIS ALLRIGHT ????????? */
		case 0x03:  /*FCOMPP*/
			if(sub != 1) {
				FPU_LOG_WARN(6,false,3,sub);
				return;
			}
			gen_load_host(&TOP,DREG(EA),4); 
			gen_dop_word_imm(DOP_ADD,true,DREG(EA),1);
			gen_dop_word_imm(DOP_AND,true,DREG(EA),7);
			gen_call_function((void*)&FPU_FCOM,"%Drd%Drd",DREG(TMPB),DREG(EA));
			gen_call_function((void*)&FPU_FPOP,""); /* extra pop at the bottom*/
			break;
		case 0x04:  /* FSUBRP STi,ST*/
			gen_call_function((void*)&FPU_FSUBR,"%Drd%Drd",DREG(EA),DREG(TMPB));
			break;
		case 0x05:  /* FSUBP  STi,ST*/
			gen_call_function((void*)&FPU_FSUB,"%Drd%Drd",DREG(EA),DREG(TMPB));
			break;
		case 0x06:	/* FDIVRP STi,ST*/
			gen_call_function((void*)&FPU_FDIVR,"%Drd%Drd",DREG(EA),DREG(TMPB));
			break;
		case 0x07:  /* FDIVP STi,ST*/
			gen_call_function((void*)&FPU_FDIV,"%Drd%Drd",DREG(EA),DREG(TMPB));
			break;
		default:
			break;
		}
		gen_call_function((void*)&FPU_FPOP,"");		
	} else {
		dyn_fill_ea(); 
		dyn_call_function_pagefault_check((void*)&FPU_FLD_I16_EA,"%Drd",DREG(EA)); 
		gen_load_host(&TOP,DREG(TMPB),4); 
		dyn_eatree();
	}
}

static void dyn_fpu_esc7(){
	dyn_get_modrm();  
	Bitu group=(decode.modrm.val >> 3) & 7;
	Bitu sub=(decode.modrm.val & 7);
	if (decode.modrm.val >= 0xc0) { 
		switch (group){
		case 0x00: /* FFREEP STi*/
			dyn_fpu_top();
			gen_call_function((void*)&FPU_FFREE,"%Drd",DREG(EA));
			gen_call_function((void*)&FPU_FPOP,"");
			break;
		case 0x01: /* FXCH STi*/
			dyn_fpu_top();
			gen_call_function((void*)&FPU_FXCH,"%Drd%Drd",DREG(TMPB),DREG(EA));
			break;
		case 0x02:  /* FSTP STi*/
		case 0x03:  /* FSTP STi*/
			dyn_fpu_top();
			gen_call_function((void*)&FPU_FST,"%Drd%Drd",DREG(TMPB),DREG(EA));
			gen_call_function((void*)&FPU_FPOP,"");
			break;
		case 0x04:
			switch(sub){
				case 0x00:     /* FNSTSW AX*/
					gen_load_host(&TOP,DREG(TMPB),4);
					gen_call_function((void*)&FPU_SET_TOP,"%Drd",DREG(TMPB)); 
					gen_mov_host(&fpu.sw,DREG(EAX),2);
					break;
				default:
					FPU_LOG_WARN(7,false,4,sub);
					break;
			}
			break;
		default:
			FPU_LOG_WARN(7,false,group,sub);
			break;
		}
	} else {
		dyn_fill_ea(); 
		switch(group){
		case 0x00:  /* FILD Bit16s */
			if (use_dynamic_core_with_paging) dyn_save_fpu_top_for_pagefault();
			gen_call_function((void*)&FPU_PREP_PUSH,"");
			gen_load_host(&TOP,DREG(TMPB),4); 
			dyn_call_function_pagefault_check((void*)&FPU_FLD_I16,"%Drd%Drd",DREG(EA),DREG(TMPB));
			break;
		case 0x01:
			FPU_LOG_WARN(7,true,group,sub);
			break;
		case 0x02:   /* FIST Bit16s */
			dyn_call_function_pagefault_check((void*)&FPU_FST_I16,"%Drd",DREG(EA));
			break;
		case 0x03:	/* FISTP Bit16s */
			dyn_call_function_pagefault_check((void*)&FPU_FST_I16,"%Drd",DREG(EA));
			gen_call_function((void*)&FPU_FPOP,"");
			break;
		case 0x04:   /* FBLD packed BCD */
			if (use_dynamic_core_with_paging) dyn_save_fpu_top_for_pagefault();
			gen_call_function((void*)&FPU_PREP_PUSH,"");
			gen_load_host(&TOP,DREG(TMPB),4);
			dyn_call_function_pagefault_check((void*)&FPU_FBLD,"%Drd%Drd",DREG(EA),DREG(TMPB));
			break;
		case 0x05:  /* FILD Bit64s */
			if (use_dynamic_core_with_paging) dyn_save_fpu_top_for_pagefault();
			gen_call_function((void*)&FPU_PREP_PUSH,"");
			gen_load_host(&TOP,DREG(TMPB),4);
			dyn_call_function_pagefault_check((void*)&FPU_FLD_I64,"%Drd%Drd",DREG(EA),DREG(TMPB));
			break;
		case 0x06:	/* FBSTP packed BCD */
			dyn_call_function_pagefault_check((void*)&FPU_FBST,"%Drd",DREG(EA));
			gen_call_function((void*)&FPU_FPOP,"");
			break;
		case 0x07:  /* FISTP Bit64s */
			dyn_call_function_pagefault_check((void*)&FPU_FST_I64,"%Drd",DREG(EA));
			gen_call_function((void*)&FPU_FPOP,"");
			break;
		default:
			FPU_LOG_WARN(7,true,group,sub);
			break;
		}
	}
}

#endif
