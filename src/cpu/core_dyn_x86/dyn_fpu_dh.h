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

/* $Id: dyn_fpu_dh.h,v 1.7 2009-09-23 20:55:19 c2woody Exp $ */

#include "dosbox.h"
#if C_FPU

static void FPU_FLD_16(PhysPt addr) {
	dyn_dh_fpu.temp.m1 = (Bit32u)mem_readw(addr);
}

static void FPU_FST_16(PhysPt addr) {
	mem_writew(addr,(Bit16u)dyn_dh_fpu.temp.m1);
}

static void FPU_FLD_32(PhysPt addr) {
	dyn_dh_fpu.temp.m1 = mem_readd(addr);
}

static void FPU_FST_32(PhysPt addr) {
	mem_writed(addr,dyn_dh_fpu.temp.m1);
}

static void FPU_FLD_64(PhysPt addr) {
	dyn_dh_fpu.temp.m1 = mem_readd(addr);
	dyn_dh_fpu.temp.m2 = mem_readd(addr+4);
}

static void FPU_FST_64(PhysPt addr) {
	mem_writed(addr,dyn_dh_fpu.temp.m1);
	mem_writed(addr+4,dyn_dh_fpu.temp.m2);
}

static void FPU_FLD_80(PhysPt addr) {
	dyn_dh_fpu.temp.m1 = mem_readd(addr);
	dyn_dh_fpu.temp.m2 = mem_readd(addr+4);
	dyn_dh_fpu.temp.m3 = mem_readw(addr+8);
}

static void FPU_FST_80(PhysPt addr) {
	mem_writed(addr,dyn_dh_fpu.temp.m1);
	mem_writed(addr+4,dyn_dh_fpu.temp.m2);
	mem_writew(addr+8,dyn_dh_fpu.temp.m3);
}

static void FPU_FLDCW_DH(PhysPt addr){
	dyn_dh_fpu.cw = mem_readw(addr);
	dyn_dh_fpu.temp.m1 = (Bit32u)(dyn_dh_fpu.cw|0x3f);
}

static void FPU_FNSTCW_DH(PhysPt addr){
	mem_writew(addr,(Bit16u)(dyn_dh_fpu.cw&0xffff));
}

static void FPU_FNINIT_DH(void){
	dyn_dh_fpu.cw = 0x37f;
}

static void FPU_FSTENV_DH(PhysPt addr){
	if(!cpu.code.big) {
		mem_writew(addr+0,(Bit16u)dyn_dh_fpu.cw);
		mem_writew(addr+2,(Bit16u)dyn_dh_fpu.temp.m2);
		mem_writew(addr+4,dyn_dh_fpu.temp.m3);
	} else { 
		mem_writed(addr+0,dyn_dh_fpu.temp.m1);
		mem_writew(addr+0,(Bit16u)dyn_dh_fpu.cw);
		mem_writed(addr+4,dyn_dh_fpu.temp.m2);
		mem_writed(addr+8,dyn_dh_fpu.temp.m3);
	}
}

static void FPU_FLDENV_DH(PhysPt addr){
	if(!cpu.code.big) {
		dyn_dh_fpu.cw = (Bit32u)mem_readw(addr);
		dyn_dh_fpu.temp.m1 = dyn_dh_fpu.cw|0x3f;
		dyn_dh_fpu.temp.m2 = (Bit32u)mem_readw(addr+2);
		dyn_dh_fpu.temp.m3 = mem_readw(addr+4);
	} else { 
		dyn_dh_fpu.cw = (Bit32u)mem_readw(addr);
		dyn_dh_fpu.temp.m1 = mem_readd(addr)|0x3f;
		dyn_dh_fpu.temp.m2 = mem_readd(addr+4);
		dyn_dh_fpu.temp.m3 = mem_readw(addr+8);
		dyn_dh_fpu.temp.d1 = mem_readw(addr+10);
	}
}

static void FPU_FSAVE_DH(PhysPt addr){
	if (!cpu.code.big) {
		mem_writew(addr,(Bit16u)dyn_dh_fpu.cw);
		addr+=2;
		mem_writeb(addr++,dyn_dh_fpu.temp_state[0x04]);
		mem_writeb(addr++,dyn_dh_fpu.temp_state[0x05]);
		mem_writeb(addr++,dyn_dh_fpu.temp_state[0x08]);
		mem_writeb(addr++,dyn_dh_fpu.temp_state[0x09]);
		mem_writeb(addr++,dyn_dh_fpu.temp_state[0x0c]);
		mem_writeb(addr++,dyn_dh_fpu.temp_state[0x0d]);
		mem_writeb(addr++,dyn_dh_fpu.temp_state[0x10]);
		mem_writeb(addr++,dyn_dh_fpu.temp_state[0x11]);
		mem_writeb(addr++,dyn_dh_fpu.temp_state[0x14]);
		mem_writeb(addr++,dyn_dh_fpu.temp_state[0x15]);
		mem_writeb(addr++,dyn_dh_fpu.temp_state[0x18]);
		mem_writeb(addr++,dyn_dh_fpu.temp_state[0x19]);
		for(Bitu i=28;i<108;i++) mem_writeb(addr++,dyn_dh_fpu.temp_state[i]);
	} else {
		mem_writew(addr,(Bit16u)dyn_dh_fpu.cw);
		addr+=2;
		for(Bitu i=2;i<108;i++) mem_writeb(addr++,dyn_dh_fpu.temp_state[i]);
	}
}

static void FPU_FRSTOR_DH(PhysPt addr){
	if (!cpu.code.big) {
		dyn_dh_fpu.cw = (Bit32u)mem_readw(addr);
		dyn_dh_fpu.temp_state[0x00] = mem_readb(addr++)|0x3f;
		dyn_dh_fpu.temp_state[0x01] = mem_readb(addr++);
		dyn_dh_fpu.temp_state[0x04] = mem_readb(addr++);
		dyn_dh_fpu.temp_state[0x05] = mem_readb(addr++);
		dyn_dh_fpu.temp_state[0x08] = mem_readb(addr++);
		dyn_dh_fpu.temp_state[0x09] = mem_readb(addr++);
		dyn_dh_fpu.temp_state[0x0c] = mem_readb(addr++);
		dyn_dh_fpu.temp_state[0x0d] = mem_readb(addr++);
		dyn_dh_fpu.temp_state[0x10] = mem_readb(addr++);
		dyn_dh_fpu.temp_state[0x11] = mem_readb(addr++);
		dyn_dh_fpu.temp_state[0x14] = mem_readb(addr++);
		dyn_dh_fpu.temp_state[0x15] = mem_readb(addr++);
		dyn_dh_fpu.temp_state[0x18] = mem_readb(addr++);
		dyn_dh_fpu.temp_state[0x19] = mem_readb(addr++);
		for(Bitu i=28;i<108;i++) dyn_dh_fpu.temp_state[i] = mem_readb(addr++);
	} else {
		dyn_dh_fpu.cw = (Bit32u)mem_readw(addr);
		for(Bitu i=0;i<108;i++) dyn_dh_fpu.temp_state[i] = mem_readb(addr++);
		dyn_dh_fpu.temp_state[0]|=0x3f;
	}
}

static void dh_fpu_esc0(){
	dyn_get_modrm(); 
	if (decode.modrm.val >= 0xc0) {
		cache_addb(0xd8);
		cache_addb(decode.modrm.val);
	} else { 
		dyn_fill_ea();
		gen_call_function((void*)&FPU_FLD_32,"%Ddr",DREG(EA)); 
		cache_addb(0xd8);
		cache_addb(0x05|(decode.modrm.reg<<3));
		cache_addd((Bit32u)(&(dyn_dh_fpu.temp.m1)));
	}
}

static void dh_fpu_esc1(){
	dyn_get_modrm();  
	if (decode.modrm.val >= 0xc0) {
		cache_addb(0xd9);
		cache_addb(decode.modrm.val);
	} else {
		Bitu group=(decode.modrm.val >> 3) & 7;
		Bitu sub=(decode.modrm.val & 7);
		dyn_fill_ea(); 
		switch(group){
		case 0x00: /* FLD float*/
			gen_call_function((void*)&FPU_FLD_32,"%Ddr",DREG(EA));
			cache_addb(0xd9);
			cache_addb(0x05|(decode.modrm.reg<<3));
			cache_addd((Bit32u)(&(dyn_dh_fpu.temp.m1)));
			break;
		case 0x01: /* UNKNOWN */
			LOG(LOG_FPU,LOG_WARN)("ESC EA 1:Unhandled group %d subfunction %d",group,sub);
			break;
		case 0x02: /* FST float*/
			cache_addb(0xd9);
			cache_addb(0x05|(decode.modrm.reg<<3));
			cache_addd((Bit32u)(&(dyn_dh_fpu.temp.m1)));
			gen_call_function((void*)&FPU_FST_32,"%Ddr",DREG(EA));
			break;
		case 0x03: /* FSTP float*/
			cache_addb(0xd9);
			cache_addb(0x05|(decode.modrm.reg<<3));
			cache_addd((Bit32u)(&(dyn_dh_fpu.temp.m1)));
			gen_call_function((void*)&FPU_FST_32,"%Ddr",DREG(EA));
			break;
		case 0x04: /* FLDENV */
			gen_call_function((void*)&FPU_FLDENV_DH,"%Ddr",DREG(EA));
			cache_addb(0xd9);
			cache_addb(0x05|(decode.modrm.reg<<3));
			cache_addd((Bit32u)(&(dyn_dh_fpu.temp.m1)));
			break;
		case 0x05: /* FLDCW */
			gen_call_function((void *)&FPU_FLDCW_DH,"%Ddr",DREG(EA));
			cache_addb(0xd9);
			cache_addb(0x05|(decode.modrm.reg<<3));
			cache_addd((Bit32u)(&(dyn_dh_fpu.temp.m1)));
			break;
		case 0x06: /* FSTENV */
			cache_addb(0xd9);
			cache_addb(0x05|(decode.modrm.reg<<3));
			cache_addd((Bit32u)(&(dyn_dh_fpu.temp.m1)));
			gen_call_function((void*)&FPU_FSTENV_DH,"%Ddr",DREG(EA));
			break;
		case 0x07:  /* FNSTCW*/
			gen_call_function((void*)&FPU_FNSTCW_DH,"%Ddr",DREG(EA));
			break;
		default:
			LOG(LOG_FPU,LOG_WARN)("ESC EA 1:Unhandled group %d subfunction %d",group,sub);
			break;
		}
	}
}

static void dh_fpu_esc2(){
	dyn_get_modrm();  
	if (decode.modrm.val >= 0xc0) { 
		cache_addb(0xda);
		cache_addb(decode.modrm.val);
	} else {
		dyn_fill_ea(); 
		gen_call_function((void*)&FPU_FLD_32,"%Ddr",DREG(EA)); 
		cache_addb(0xda);
		cache_addb(0x05|(decode.modrm.reg<<3));
		cache_addd((Bit32u)(&(dyn_dh_fpu.temp.m1)));
	}
}

static void dh_fpu_esc3(){
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
				cache_addb(0xdb);
				cache_addb(decode.modrm.val);
				break;
			case 0x03:				//FNINIT FINIT
				gen_call_function((void*)&FPU_FNINIT_DH,""); 
				cache_addb(0xdb);
				cache_addb(decode.modrm.val);
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
			LOG(LOG_FPU,LOG_WARN)("ESC 3:Unhandled group %d subfunction %d",group,sub);
			break;
		}
	} else {
		Bitu group=(decode.modrm.val >> 3) & 7;
		Bitu sub=(decode.modrm.val & 7);
		dyn_fill_ea(); 
		switch(group){
		case 0x00:	/* FILD */
			gen_call_function((void*)&FPU_FLD_32,"%Ddr",DREG(EA));
			cache_addb(0xdb);
			cache_addb(0x05|(decode.modrm.reg<<3));
			cache_addd((Bit32u)(&(dyn_dh_fpu.temp.m1)));
			break;
		case 0x01:	/* FISTTP */
			LOG(LOG_FPU,LOG_WARN)("ESC 3 EA:Unhandled group %d subfunction %d",group,sub);
			break;
		case 0x02:	/* FIST */
			cache_addb(0xdb);
			cache_addb(0x05|(decode.modrm.reg<<3));
			cache_addd((Bit32u)(&(dyn_dh_fpu.temp.m1)));
			gen_call_function((void*)&FPU_FST_32,"%Ddr",DREG(EA));
			break;
		case 0x03:	/* FISTP */
			cache_addb(0xdb);
			cache_addb(0x05|(decode.modrm.reg<<3));
			cache_addd((Bit32u)(&(dyn_dh_fpu.temp.m1)));
			gen_call_function((void*)&FPU_FST_32,"%Ddr",DREG(EA));
			break;
		case 0x05:	/* FLD 80 Bits Real */
			gen_call_function((void*)&FPU_FLD_80,"%Ddr",DREG(EA));
			cache_addb(0xdb);
			cache_addb(0x05|(decode.modrm.reg<<3));
			cache_addd((Bit32u)(&(dyn_dh_fpu.temp.m1)));
			break;
		case 0x07:	/* FSTP 80 Bits Real */
			cache_addb(0xdb);
			cache_addb(0x05|(decode.modrm.reg<<3));
			cache_addd((Bit32u)(&(dyn_dh_fpu.temp.m1)));
			gen_call_function((void*)&FPU_FST_80,"%Ddr",DREG(EA));
			break;
		default:
			LOG(LOG_FPU,LOG_WARN)("ESC 3 EA:Unhandled group %d subfunction %d",group,sub);
		}
	}
}

static void dh_fpu_esc4(){
	dyn_get_modrm();  
	Bitu group=(decode.modrm.val >> 3) & 7;
	Bitu sub=(decode.modrm.val & 7);
	if (decode.modrm.val >= 0xc0) { 
		cache_addb(0xdc);
		cache_addb(decode.modrm.val);
	} else { 
		dyn_fill_ea(); 
		gen_call_function((void*)&FPU_FLD_64,"%Ddr",DREG(EA)); 
		cache_addb(0xdc);
		cache_addb(0x05|(decode.modrm.reg<<3));
		cache_addd((Bit32u)(&(dyn_dh_fpu.temp.m1)));
	}
}

static void dh_fpu_esc5(){
	dyn_get_modrm();  
	if (decode.modrm.val >= 0xc0) { 
		cache_addb(0xdd);
		cache_addb(decode.modrm.val);
	} else {
		dyn_fill_ea(); 
		Bitu group=(decode.modrm.val >> 3) & 7;
		Bitu sub=(decode.modrm.val & 7);
		switch(group){
		case 0x00:  /* FLD double real*/
			gen_call_function((void*)&FPU_FLD_64,"%Ddr",DREG(EA));
			cache_addb(0xdd);
			cache_addb(0x05|(decode.modrm.reg<<3));
			cache_addd((Bit32u)(&(dyn_dh_fpu.temp.m1)));
			break;
		case 0x01:  /* FISTTP longint*/
			LOG(LOG_FPU,LOG_WARN)("ESC 5 EA:Unhandled group %d subfunction %d",group,sub);
			break;
		case 0x02:   /* FST double real*/
			cache_addb(0xdd);
			cache_addb(0x05|(decode.modrm.reg<<3));
			cache_addd((Bit32u)(&(dyn_dh_fpu.temp.m1)));
			gen_call_function((void*)&FPU_FST_64,"%Ddr",DREG(EA));
			break;
		case 0x03:	/* FSTP double real*/
			cache_addb(0xdd);
			cache_addb(0x05|(decode.modrm.reg<<3));
			cache_addd((Bit32u)(&(dyn_dh_fpu.temp.m1)));
			gen_call_function((void*)&FPU_FST_64,"%Ddr",DREG(EA));
			break;
		case 0x04:	/* FRSTOR */
			gen_call_function((void*)&FPU_FRSTOR_DH,"%Ddr",DREG(EA));
			cache_addb(0xdd);
			cache_addb(0x05|(decode.modrm.reg<<3));
			cache_addd((Bit32u)(&(dyn_dh_fpu.temp_state[0])));
			break;
		case 0x06:	/* FSAVE */
			cache_addb(0xdd);
			cache_addb(0x05|(decode.modrm.reg<<3));
			cache_addd((Bit32u)(&(dyn_dh_fpu.temp_state[0])));
			gen_call_function((void*)&FPU_FSAVE_DH,"%Ddr",DREG(EA));
			cache_addb(0xdb);
			cache_addb(0xe3);
			break;
		case 0x07:   /* FNSTSW */
			cache_addb(0xdd);
			cache_addb(0x05|(decode.modrm.reg<<3));
			cache_addd((Bit32u)(&(dyn_dh_fpu.temp.m1)));
			gen_call_function((void*)&FPU_FST_16,"%Ddr",DREG(EA));
			break;
		default:
			LOG(LOG_FPU,LOG_WARN)("ESC 5 EA:Unhandled group %d subfunction %d",group,sub);
		}
	}
}

static void dh_fpu_esc6(){
	dyn_get_modrm();  
	Bitu group=(decode.modrm.val >> 3) & 7;
	Bitu sub=(decode.modrm.val & 7);
	if (decode.modrm.val >= 0xc0) { 
		cache_addb(0xde);
		cache_addb(decode.modrm.val);
	} else {
		dyn_fill_ea(); 
		gen_call_function((void*)&FPU_FLD_16,"%Ddr",DREG(EA)); 
		cache_addb(0xde);
		cache_addb(0x05|(decode.modrm.reg<<3));
		cache_addd((Bit32u)(&(dyn_dh_fpu.temp.m1)));
	}
}

static void dh_fpu_esc7(){
	dyn_get_modrm();  
	Bitu group=(decode.modrm.val >> 3) & 7;
	Bitu sub=(decode.modrm.val & 7);
	if (decode.modrm.val >= 0xc0) { 
		switch (group){
		case 0x00: /* FFREEP STi*/
			cache_addb(0xdf);
			cache_addb(decode.modrm.val);
			break;
		case 0x01: /* FXCH STi*/
			cache_addb(0xdf);
			cache_addb(decode.modrm.val);
			break;
		case 0x02:  /* FSTP STi*/
		case 0x03:  /* FSTP STi*/
			cache_addb(0xdf);
			cache_addb(decode.modrm.val);
			break;
		case 0x04:
			switch(sub){
				case 0x00:     /* FNSTSW AX*/
					cache_addb(0xdd);
					cache_addb(0x05|(0x07<<3));
					cache_addd((Bit32u)(&(dyn_dh_fpu.temp.m1)));
					gen_load_host(&(dyn_dh_fpu.temp.m1),DREG(TMPB),4);
					gen_dop_word(DOP_MOV,false,DREG(EAX),DREG(TMPB));
					gen_releasereg(DREG(TMPB));
					break;
				default:
					LOG(LOG_FPU,LOG_WARN)("ESC 7:Unhandled group %d subfunction %d",group,sub);
					break;
			}
			break;
		default:
			LOG(LOG_FPU,LOG_WARN)("ESC 7:Unhandled group %d subfunction %d",group,sub);
			break;
		}
	} else {
		dyn_fill_ea(); 
		switch(group){
		case 0x00:  /* FILD Bit16s */
			gen_call_function((void*)&FPU_FLD_16,"%Ddr",DREG(EA));
			cache_addb(0xdf);
			cache_addb(0x05|(decode.modrm.reg<<3));
			cache_addd((Bit32u)(&(dyn_dh_fpu.temp.m1)));
			break;
		case 0x01:
			LOG(LOG_FPU,LOG_WARN)("ESC 7 EA:Unhandled group %d subfunction %d",group,sub);
			break;
		case 0x02:   /* FIST Bit16s */
			cache_addb(0xdf);
			cache_addb(0x05|(decode.modrm.reg<<3));
			cache_addd((Bit32u)(&(dyn_dh_fpu.temp.m1)));
			gen_call_function((void*)&FPU_FST_16,"%Ddr",DREG(EA));
			break;
		case 0x03:	/* FISTP Bit16s */
			cache_addb(0xdf);
			cache_addb(0x05|(decode.modrm.reg<<3));
			cache_addd((Bit32u)(&(dyn_dh_fpu.temp.m1)));
			gen_call_function((void*)&FPU_FST_16,"%Ddr",DREG(EA));
			break;
		case 0x04:   /* FBLD packed BCD */
			gen_call_function((void*)&FPU_FLD_80,"%Ddr",DREG(EA));
			cache_addb(0xdf);
			cache_addb(0x05|(decode.modrm.reg<<3));
			cache_addd((Bit32u)(&(dyn_dh_fpu.temp.m1)));
			break;
		case 0x05:  /* FILD Bit64s */
			gen_call_function((void*)&FPU_FLD_64,"%Ddr",DREG(EA));
			cache_addb(0xdf);
			cache_addb(0x05|(decode.modrm.reg<<3));
			cache_addd((Bit32u)(&(dyn_dh_fpu.temp.m1)));
			break;
		case 0x06:	/* FBSTP packed BCD */
			cache_addb(0xdf);
			cache_addb(0x05|(decode.modrm.reg<<3));
			cache_addd((Bit32u)(&(dyn_dh_fpu.temp.m1)));
			gen_call_function((void*)&FPU_FST_80,"%Ddr",DREG(EA));
			break;
		case 0x07:  /* FISTP Bit64s */
			cache_addb(0xdf);
			cache_addb(0x05|(decode.modrm.reg<<3));
			cache_addd((Bit32u)(&(dyn_dh_fpu.temp.m1)));
			gen_call_function((void*)&FPU_FST_64,"%Ddr",DREG(EA));
			break;
		default:
			LOG(LOG_FPU,LOG_WARN)("ESC 7 EA:Unhandled group %d subfunction %d",group,sub);
			break;
		}
	}
}

#endif
