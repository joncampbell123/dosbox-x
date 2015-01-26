/*
 *  Copyright (C) 2002-2015  The DOSBox Team
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

switch (inst.code.load) {
/* General loading */
	case L_POPwRM:
		inst_op1_w = Pop_16();
		goto case_L_MODRM;
	case L_POPdRM:
		inst_op1_d = Pop_32();
		goto case_L_MODRM;
	case L_MODRM_NVM:
		if ((reg_flags & FLAG_VM) || !cpu.pmode) goto illegalopcode;
		goto case_L_MODRM;
case_L_MODRM:
	case L_MODRM:
		inst.rm=Fetchb();
		inst.rm_index=(inst.rm >> 3) & 7;
		inst.rm_eai=inst.rm&07;
		inst.rm_mod=inst.rm>>6;
		/* Decode address of mod/rm if needed */
		if (inst.rm<0xc0) {
			if (!(inst.prefix & PREFIX_ADDR))
			#include "ea_lookup.h"
		}
l_MODRMswitch:
		switch (inst.code.extra) {	
/* Byte */
		case M_Ib:
			inst_op1_d=Fetchb();
			break;
		case M_Ebx:
			if (inst.rm<0xc0) inst_op1_ds=(Bit8s)LoadMb(inst.rm_eaa);
			else inst_op1_ds=(Bit8s)reg_8(inst.rm_eai);
			break;
		case M_EbIb:
			inst_op2_d=Fetchb();
		case M_Eb:
			if (inst.rm<0xc0) inst_op1_d=LoadMb(inst.rm_eaa);
			else inst_op1_d=reg_8(inst.rm_eai);
			break;
		case M_EbGb:
			if (inst.rm<0xc0) inst_op1_d=LoadMb(inst.rm_eaa);
			else inst_op1_d=reg_8(inst.rm_eai);
			inst_op2_d=reg_8(inst.rm_index);
			break;
		case M_GbEb:
			if (inst.rm<0xc0) inst_op2_d=LoadMb(inst.rm_eaa);
			else inst_op2_d=reg_8(inst.rm_eai);
		case M_Gb:
			inst_op1_d=reg_8(inst.rm_index);;
			break;
/* Word */
		case M_Iw:
			inst_op1_d=Fetchw();
			break;
		case M_EwxGwx:
			inst_op2_ds=(Bit16s)reg_16(inst.rm_index);
			goto l_M_Ewx;
		case M_EwxIbx:
			inst_op2_ds=Fetchbs();
			goto l_M_Ewx;
		case M_EwxIwx:
			inst_op2_ds=Fetchws();
l_M_Ewx:		
		case M_Ewx:
			if (inst.rm<0xc0) inst_op1_ds=(Bit16s)LoadMw(inst.rm_eaa);
			else inst_op1_ds=(Bit16s)reg_16(inst.rm_eai);
			break;
		case M_EwIb:
			inst_op2_d=Fetchb();
			goto l_M_Ew;
		case M_EwIbx:
			inst_op2_ds=Fetchbs();
			goto l_M_Ew;		
		case M_EwIw:
			inst_op2_d=Fetchw();
			goto l_M_Ew;
		case M_EwGwCL:
			inst_imm_d=reg_cl;
			goto l_M_EwGw;
		case M_EwGwIb:
			inst_imm_d=Fetchb();
			goto l_M_EwGw;
		case M_EwGwt:
			inst_op2_d=reg_16(inst.rm_index);
			inst.rm_eaa+=((Bit16s)inst_op2_d >> 4) * 2;
			goto l_M_Ew;
l_M_EwGw:			
		case M_EwGw:
			inst_op2_d=reg_16(inst.rm_index);
l_M_Ew:
		case M_Ew:
			if (inst.rm<0xc0) inst_op1_d=LoadMw(inst.rm_eaa);
			else inst_op1_d=reg_16(inst.rm_eai);
			break;
		case M_GwEw:
			if (inst.rm<0xc0) inst_op2_d=LoadMw(inst.rm_eaa);
			else inst_op2_d=reg_16(inst.rm_eai);
		case M_Gw:
			inst_op1_d=reg_16(inst.rm_index);;
			break;
/* DWord */
		case M_Id:
			inst_op1_d=Fetchd();
			break;
		case M_EdxGdx:
			inst_op2_ds=(Bit32s)reg_32(inst.rm_index);
		case M_Edx:
			if (inst.rm<0xc0) inst_op1_d=(Bit32s)LoadMd(inst.rm_eaa);
			else inst_op1_d=(Bit32s)reg_32(inst.rm_eai);
			break;
		case M_EdIb:
			inst_op2_d=Fetchb();
			goto l_M_Ed;
		case M_EdIbx:
			inst_op2_ds=Fetchbs();
			goto l_M_Ed;
		case M_EdId:
			inst_op2_d=Fetchd();
			goto l_M_Ed;			
		case M_EdGdCL:
			inst_imm_d=reg_cl;
			goto l_M_EdGd;
		case M_EdGdt:
			inst_op2_d=reg_32(inst.rm_index);
			inst.rm_eaa+=((Bit32s)inst_op2_d >> 5) * 4;
			goto l_M_Ed;
		case M_EdGdIb:
			inst_imm_d=Fetchb();
			goto l_M_EdGd;
l_M_EdGd:
		case M_EdGd:
			inst_op2_d=reg_32(inst.rm_index);
l_M_Ed:
		case M_Ed:
			if (inst.rm<0xc0) inst_op1_d=LoadMd(inst.rm_eaa);
			else inst_op1_d=reg_32(inst.rm_eai);
			break;
		case M_GdEd:
			if (inst.rm<0xc0) inst_op2_d=LoadMd(inst.rm_eaa);
			else inst_op2_d=reg_32(inst.rm_eai);
		case M_Gd:
			inst_op1_d=reg_32(inst.rm_index);
			break;
/* Others */		

		case M_SEG:
			//TODO Check for limit
			inst_op1_d=SegValue((SegNames)inst.rm_index);
			break;
		case M_Efw:
			if (inst.rm>=0xc0) goto illegalopcode;
			inst_op1_d=LoadMw(inst.rm_eaa);
			inst_op2_d=LoadMw(inst.rm_eaa+2);
			break;
		case M_Efd:
			if (inst.rm>=0xc0) goto illegalopcode;
			inst_op1_d=LoadMd(inst.rm_eaa);
			inst_op2_d=LoadMw(inst.rm_eaa+4);
			break;
		case M_EA:
			inst_op1_d=inst.rm_off;
			break;
		case M_POPw:
			inst_op1_d = Pop_16();
			break;
		case M_POPd:
			inst_op1_d = Pop_32();
			break;
		case M_GRP:
			inst.code=Groups[inst.code.op][inst.rm_index];
			goto l_MODRMswitch;
		case M_GRP_Ib:
			inst_op2_d=Fetchb();
			inst.code=Groups[inst.code.op][inst.rm_index];
			goto l_MODRMswitch;
		case M_GRP_CL:
			inst_op2_d=reg_cl;
			inst.code=Groups[inst.code.op][inst.rm_index];
			goto l_MODRMswitch;
		case M_GRP_1:
			inst_op2_d=1;
			inst.code=Groups[inst.code.op][inst.rm_index];
			goto l_MODRMswitch;
		case 0:
			break;
		default:
			LOG(LOG_CPU,LOG_ERROR)("MODRM:Unhandled load %d entry %x",inst.code.extra,inst.entry);
			break;
		}
		break;
	case L_POPw:
		inst_op1_d = Pop_16();
		break;
	case L_POPd:
		inst_op1_d = Pop_32();
		break;
	case L_POPfw:
		inst_op1_d = Pop_16();
		inst_op2_d = Pop_16();
		break;
	case L_POPfd:
		inst_op1_d = Pop_32();
		inst_op2_d = Pop_16();
		break;
	case L_Ib:
		inst_op1_d=Fetchb();
		break;
	case L_Ibx:
		inst_op1_ds=Fetchbs();
		break;
	case L_Iw:
		inst_op1_d=Fetchw();
		break;
	case L_Iwx:
		inst_op1_ds=Fetchws();
		break;
	case L_Idx:
	case L_Id:
		inst_op1_d=Fetchd();
		break;
	case L_Ifw:
		inst_op1_d=Fetchw();
		inst_op2_d=Fetchw();
		break;
	case L_Ifd:
		inst_op1_d=Fetchd();
		inst_op2_d=Fetchw();
		break;
/* Direct load of registers */
	case L_REGbIb:
		inst_op2_d=Fetchb();
	case L_REGb:
		inst_op1_d=reg_8(inst.code.extra);
		break;
	case L_REGwIw:
		inst_op2_d=Fetchw();
	case L_REGw:
		inst_op1_d=reg_16(inst.code.extra);
		break;
	case L_REGdId:
		inst_op2_d=Fetchd();
	case L_REGd:
		inst_op1_d=reg_32(inst.code.extra);
		break;
	case L_SEG:
		inst_op1_d=SegValue((SegNames)inst.code.extra);
		break;
/* Depending on addressize */
	case L_OP:
		if (inst.prefix & PREFIX_ADDR) {
			inst.rm_eaa=Fetchd();
		} else {
			inst.rm_eaa=Fetchw();
		}
		if (inst.prefix & PREFIX_SEG) {
			inst.rm_eaa+=inst.seg.base;
		} else {
			inst.rm_eaa+=SegBase(ds);
		}
		break;
	/* Special cases */
	case L_DOUBLE:
		inst.entry|=0x100;
		goto restartopcode;
	case L_PRESEG:
		inst.prefix|=PREFIX_SEG;
		inst.seg.base=SegBase((SegNames)inst.code.extra);
		goto restartopcode;
	case L_PREREPNE:
		inst.prefix|=PREFIX_REP;
		inst.repz=false;
		goto restartopcode;
	case L_PREREP:
		inst.prefix|=PREFIX_REP;
		inst.repz=true;
		goto restartopcode;
	case L_PREOP:
		inst.entry=(cpu.code.big ^1) * 0x200;
		goto restartopcode;
	case L_PREADD:
		inst.prefix=(inst.prefix & ~1) | (cpu.code.big ^ 1);
		goto restartopcode;
	case L_VAL:
		inst_op1_d=inst.code.extra;
		break;
	case L_INTO:
		if (!get_OF()) goto nextopcode;
		inst_op1_d=4;
		break;
	case D_IRETw:
		CPU_IRET(false,GetIP());
		if (GETFLAG(IF) && PIC_IRQCheck) {
			return CBRET_NONE;
		}
		continue;
	case D_IRETd:
		CPU_IRET(true,GetIP());
		if (GETFLAG(IF) && PIC_IRQCheck) 
			return CBRET_NONE;
		continue;
	case D_RETFwIw:
		{
			Bitu words=Fetchw();
			FillFlags();	
			CPU_RET(false,words,GetIP());
			continue;
		}
	case D_RETFw:
		FillFlags();
		CPU_RET(false,0,GetIP());
		continue;
	case D_RETFdIw:
		{
			Bitu words=Fetchw();
			FillFlags();	
			CPU_RET(true,words,GetIP());
			continue;
		}
	case D_RETFd:
		FillFlags();
		CPU_RET(true,0,GetIP());
		continue;
/* Direct operations */
	case L_STRING:
		#include "string.h"
		goto nextopcode;
	case D_PUSHAw:
		{
			Bit16u old_sp=reg_sp;
			Push_16(reg_ax);Push_16(reg_cx);Push_16(reg_dx);Push_16(reg_bx);
			Push_16(old_sp);Push_16(reg_bp);Push_16(reg_si);Push_16(reg_di);
		}
		goto nextopcode;
	case D_PUSHAd:
		{
			Bit32u old_esp=reg_esp;
			Push_32(reg_eax);Push_32(reg_ecx);Push_32(reg_edx);Push_32(reg_ebx);
			Push_32(old_esp);Push_32(reg_ebp);Push_32(reg_esi);Push_32(reg_edi);
		}
		goto nextopcode;
	case D_POPAw:
		reg_di=Pop_16();reg_si=Pop_16();reg_bp=Pop_16();Pop_16();//Don't save SP
		reg_bx=Pop_16();reg_dx=Pop_16();reg_cx=Pop_16();reg_ax=Pop_16();
		goto nextopcode;
	case D_POPAd:
		reg_edi=Pop_32();reg_esi=Pop_32();reg_ebp=Pop_32();Pop_32();//Don't save ESP
		reg_ebx=Pop_32();reg_edx=Pop_32();reg_ecx=Pop_32();reg_eax=Pop_32();
		goto nextopcode;
	case D_POPSEGw:
		if (CPU_PopSeg((SegNames)inst.code.extra,false)) RunException();
		goto nextopcode;
	case D_POPSEGd:
		if (CPU_PopSeg((SegNames)inst.code.extra,true)) RunException();
		goto nextopcode;
	case D_SETALC:
		reg_al = get_CF() ? 0xFF : 0;
		goto nextopcode;
	case D_XLAT:
		if (inst.prefix & PREFIX_SEG) {
			if (inst.prefix & PREFIX_ADDR) {
				reg_al=LoadMb(inst.seg.base+(Bit32u)(reg_ebx+reg_al));
			} else {
				reg_al=LoadMb(inst.seg.base+(Bit16u)(reg_bx+reg_al));
			}
		} else {
			if (inst.prefix & PREFIX_ADDR) {
				reg_al=LoadMb(SegBase(ds)+(Bit32u)(reg_ebx+reg_al));
			} else {
				reg_al=LoadMb(SegBase(ds)+(Bit16u)(reg_bx+reg_al));
			}
		}
		goto nextopcode;
	case D_CBW:
		reg_ax=(Bit8s)reg_al;
		goto nextopcode;
	case D_CWDE:
		reg_eax=(Bit16s)reg_ax;
		goto nextopcode;
	case D_CWD:
		if (reg_ax & 0x8000) reg_dx=0xffff;
		else reg_dx=0;
		goto nextopcode;
	case D_CDQ:
		if (reg_eax & 0x80000000) reg_edx=0xffffffff;
		else reg_edx=0;
		goto nextopcode;
	case D_CLI:
		if (CPU_CLI()) RunException();
		goto nextopcode;
	case D_STI:
		if (CPU_STI()) RunException();
		goto nextopcode;
	case D_STC:
		FillFlags();SETFLAGBIT(CF,true);
		goto nextopcode;
	case D_CLC:
		FillFlags();SETFLAGBIT(CF,false);
		goto nextopcode;
	case D_CMC:
		FillFlags();
		SETFLAGBIT(CF,!(reg_flags & FLAG_CF));
		goto nextopcode;
	case D_CLD:
		SETFLAGBIT(DF,false);
		cpu.direction=1;
		goto nextopcode;
	case D_STD:
		SETFLAGBIT(DF,true);
		cpu.direction=-1;
		goto nextopcode;
	case D_PUSHF:
		if (CPU_PUSHF(inst.code.extra)) RunException();
		goto nextopcode;
	case D_POPF:
		if (CPU_POPF(inst.code.extra)) RunException();
		if (GETFLAG(IF) && PIC_IRQCheck) {
			SaveIP();
			return CBRET_NONE;
		}
		goto nextopcode;
	case D_SAHF:
		SETFLAGSb(reg_ah);
		goto nextopcode;
	case D_LAHF:
		FillFlags();
		reg_ah=reg_flags&0xff;
		goto nextopcode;
	case D_WAIT:
	case D_NOP:
		goto nextopcode;
	case D_LOCK: /* FIXME: according to intel, LOCK should raise an exception if it's not followed by one of a small set of instructions;
			probably doesn't matter for our purposes as it is a pentium prefix anyhow */
		LOG(LOG_CPU,LOG_NORMAL)("CPU:LOCK");
		goto nextopcode;
	case D_ENTERw:
		{
			Bitu bytes=Fetchw();
			Bitu level=Fetchb();
			CPU_ENTER(false,bytes,level);
			goto nextopcode;
		}
	case D_ENTERd:
		{
			Bitu bytes=Fetchw();
			Bitu level=Fetchb();
			CPU_ENTER(true,bytes,level);
			goto nextopcode;
		}
	case D_LEAVEw:
		reg_esp&=cpu.stack.notmask;
		reg_esp|=(reg_ebp&cpu.stack.mask);
		reg_bp=Pop_16();
		goto nextopcode;
	case D_LEAVEd:
		reg_esp&=cpu.stack.notmask;
		reg_esp|=(reg_ebp&cpu.stack.mask);
		reg_ebp=Pop_32();
		goto nextopcode;
	case D_DAA:
		DAA();
		goto nextopcode;
	case D_DAS:
		DAS();
		goto nextopcode;
	case D_AAA:
		AAA();
		goto nextopcode;
	case D_AAS:
		AAS();
		goto nextopcode;
	case D_CPUID:
		if (!CPU_CPUID()) goto illegalopcode;
		goto nextopcode;
	case D_HLT:
		if (cpu.pmode && cpu.cpl) EXCEPTION(EXCEPTION_GP);
		FillFlags();
		CPU_HLT(GetIP());
		return CBRET_NONE;
	case D_CLTS:
		if (cpu.pmode && cpu.cpl) goto illegalopcode;
		cpu.cr0&=(~CR0_TASKSWITCH);
		goto nextopcode;
	case D_ICEBP:
		CPU_SW_Interrupt_NoIOPLCheck(1,GetIP());
		continue;
	case D_RDTSC: {
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUM) goto illegalopcode;
		Bit64s tsc=(Bit64s)(PIC_FullIndex()*(double)(CPU_CycleAutoAdjust?70000:CPU_CycleMax));
		reg_edx=(Bit32u)(tsc>>32);
		reg_eax=(Bit32u)(tsc&0xffffffff);
		break;
		}
	default:
		LOG(LOG_CPU,LOG_ERROR)("LOAD:Unhandled code %d opcode %X",inst.code.load,inst.entry);
		goto illegalopcode;
}

