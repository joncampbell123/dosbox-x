/*
 *  Copyright (C) 2002-2013  The DOSBox Team
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

	CASE_0F_D(0x00)												/* GRP 6 Exxx */
		{
			if ((reg_flags & FLAG_VM) || (!cpu.pmode)) goto illegal_opcode;
			GetRM;Bitu which=(rm>>3)&7;
			switch (which) {
			case 0x00:	/* SLDT */
			case 0x01:	/* STR */
				{
					Bitu saveval;
					if (!which) saveval=CPU_SLDT();
					else saveval=CPU_STR();
					if (rm >= 0xc0) {GetEArw;*earw=(Bit16u)saveval;}
					else {GetEAa;SaveMw(eaa,saveval);}
				}
				break;
			case 0x02:case 0x03:case 0x04:case 0x05:
				{
					/* Just use 16-bit loads since were only using selectors */
					Bitu loadval;
					if (rm >= 0xc0 ) {GetEArw;loadval=*earw;}
					else {GetEAa;loadval=LoadMw(eaa);}
					switch (which) {
					case 0x02:
						if (cpu.cpl) EXCEPTION(EXCEPTION_GP);
						if (CPU_LLDT(loadval)) RUNEXCEPTION();
						break;
					case 0x03:
						if (cpu.cpl) EXCEPTION(EXCEPTION_GP);
						if (CPU_LTR(loadval)) RUNEXCEPTION();
						break;
					case 0x04:
						CPU_VERR(loadval);
						break;
					case 0x05:
						CPU_VERW(loadval);
						break;
					}
				}
				break;
			default:
				LOG(LOG_CPU,LOG_ERROR)("GRP6:Illegal call %2X",which);
				goto illegal_opcode;
			}
		}
		break;
	CASE_0F_D(0x01)												/* Group 7 Ed */
		{
			GetRM;Bitu which=(rm>>3)&7;
			if (rm < 0xc0)	{ //First ones all use EA
				GetEAa;Bitu limit;
				switch (which) {
				case 0x00:										/* SGDT */
					SaveMw(eaa,(Bit16u)CPU_SGDT_limit());
					SaveMd(eaa+2,(Bit32u)CPU_SGDT_base());
					break;
				case 0x01:										/* SIDT */
					SaveMw(eaa,(Bit16u)CPU_SIDT_limit());
					SaveMd(eaa+2,(Bit32u)CPU_SIDT_base());
					break;
				case 0x02:										/* LGDT */
					if (cpu.pmode && cpu.cpl) EXCEPTION(EXCEPTION_GP);
					CPU_LGDT(LoadMw(eaa),LoadMd(eaa+2));
					break;
				case 0x03:										/* LIDT */
					if (cpu.pmode && cpu.cpl) EXCEPTION(EXCEPTION_GP);
					CPU_LIDT(LoadMw(eaa),LoadMd(eaa+2));
					break;
				case 0x04:										/* SMSW */
					SaveMw(eaa,(Bit16u)CPU_SMSW());
					break;
				case 0x06:										/* LMSW */
					limit=LoadMw(eaa);
					if (CPU_LMSW((Bit16u)limit)) RUNEXCEPTION();
					break;
				case 0x07:										/* INVLPG */
					if (cpu.pmode && cpu.cpl) EXCEPTION(EXCEPTION_GP);
					PAGING_ClearTLB();
					break;
				}
			} else {
				GetEArd;
				switch (which) {
				case 0x02:										/* LGDT */
					if (cpu.pmode && cpu.cpl) EXCEPTION(EXCEPTION_GP);
					goto illegal_opcode;
				case 0x03:										/* LIDT */
					if (cpu.pmode && cpu.cpl) EXCEPTION(EXCEPTION_GP);
					goto illegal_opcode;
				case 0x04:										/* SMSW */
					*eard=(Bit32u)CPU_SMSW();
					break;
				case 0x06:										/* LMSW */
					if (CPU_LMSW(*eard)) RUNEXCEPTION();
					break;
				default:
					LOG(LOG_CPU,LOG_ERROR)("Illegal group 7 RM subfunction %d",which);
					goto illegal_opcode;
					break;
				}

			}
		}
		break;
	CASE_0F_D(0x02)												/* LAR Gd,Ed */
		{
			if ((reg_flags & FLAG_VM) || (!cpu.pmode)) goto illegal_opcode;
			GetRMrd;Bitu ar=*rmrd;
			if (rm >= 0xc0) {
				GetEArw;CPU_LAR(*earw,ar);
			} else {
				GetEAa;CPU_LAR(LoadMw(eaa),ar);
			}
			*rmrd=(Bit32u)ar;
		}
		break;
	CASE_0F_D(0x03)												/* LSL Gd,Ew */
		{
			if ((reg_flags & FLAG_VM) || (!cpu.pmode)) goto illegal_opcode;
			GetRMrd;Bitu limit=*rmrd;
			/* Just load 16-bit values for selectors */
			if (rm >= 0xc0) {
				GetEArw;CPU_LSL(*earw,limit);
			} else {
				GetEAa;CPU_LSL(LoadMw(eaa),limit);
			}
			*rmrd=(Bit32u)limit;
		}
		break;
	CASE_0F_D(0x80)												/* JO */
		JumpCond32_d(TFLG_O);break;
	CASE_0F_D(0x81)												/* JNO */
		JumpCond32_d(TFLG_NO);break;
	CASE_0F_D(0x82)												/* JB */
		JumpCond32_d(TFLG_B);break;
	CASE_0F_D(0x83)												/* JNB */
		JumpCond32_d(TFLG_NB);break;
	CASE_0F_D(0x84)												/* JZ */
		JumpCond32_d(TFLG_Z);break;
	CASE_0F_D(0x85)												/* JNZ */
		JumpCond32_d(TFLG_NZ);break;
	CASE_0F_D(0x86)												/* JBE */
		JumpCond32_d(TFLG_BE);break;
	CASE_0F_D(0x87)												/* JNBE */
		JumpCond32_d(TFLG_NBE);break;
	CASE_0F_D(0x88)												/* JS */
		JumpCond32_d(TFLG_S);break;
	CASE_0F_D(0x89)												/* JNS */
		JumpCond32_d(TFLG_NS);break;
	CASE_0F_D(0x8a)												/* JP */
		JumpCond32_d(TFLG_P);break;
	CASE_0F_D(0x8b)												/* JNP */
		JumpCond32_d(TFLG_NP);break;
	CASE_0F_D(0x8c)												/* JL */
		JumpCond32_d(TFLG_L);break;
	CASE_0F_D(0x8d)												/* JNL */
		JumpCond32_d(TFLG_NL);break;
	CASE_0F_D(0x8e)												/* JLE */
		JumpCond32_d(TFLG_LE);break;
	CASE_0F_D(0x8f)												/* JNLE */
		JumpCond32_d(TFLG_NLE);break;
	
	CASE_0F_D(0xa0)												/* PUSH FS */		
		Push_32(SegValue(fs));break;
	CASE_0F_D(0xa1)												/* POP FS */		
		if (CPU_PopSeg(fs,true)) RUNEXCEPTION();
		break;
	CASE_0F_D(0xa3)												/* BT Ed,Gd */
		{
			FillFlags();GetRMrd;
			Bit32u mask=1 << (*rmrd & 31);
			if (rm >= 0xc0 ) {
				GetEArd;
				SETFLAGBIT(CF,(*eard & mask));
			} else {
				GetEAa;eaa+=(((Bit32s)*rmrd)>>5)*4;
				Bit32u old=LoadMd(eaa);
				SETFLAGBIT(CF,(old & mask));
			}
			break;
		}
	CASE_0F_D(0xa4)												/* SHLD Ed,Gd,Ib */
		RMEdGdOp3(DSHLD,Fetchb());
		break;
	CASE_0F_D(0xa5)												/* SHLD Ed,Gd,CL */
		RMEdGdOp3(DSHLD,reg_cl);
		break;
	CASE_0F_D(0xa8)												/* PUSH GS */		
		Push_32(SegValue(gs));break;
	CASE_0F_D(0xa9)												/* POP GS */		
		if (CPU_PopSeg(gs,true)) RUNEXCEPTION();
		break;
	CASE_0F_D(0xab)												/* BTS Ed,Gd */
		{
			FillFlags();GetRMrd;
			Bit32u mask=1 << (*rmrd & 31);
			if (rm >= 0xc0 ) {
				GetEArd;
				SETFLAGBIT(CF,(*eard & mask));
				*eard|=mask;
			} else {
				GetEAa;eaa+=(((Bit32s)*rmrd)>>5)*4;
				Bit32u old=LoadMd(eaa);
				SETFLAGBIT(CF,(old & mask));
				SaveMd(eaa,old | mask);
			}
			break;
		}
	
	CASE_0F_D(0xac)												/* SHRD Ed,Gd,Ib */
		RMEdGdOp3(DSHRD,Fetchb());
		break;
	CASE_0F_D(0xad)												/* SHRD Ed,Gd,CL */
		RMEdGdOp3(DSHRD,reg_cl);
		break;
	CASE_0F_D(0xaf)												/* IMUL Gd,Ed */
		{
			RMGdEdOp3(DIMULD,*rmrd);
			break;
		}
	CASE_0F_D(0xb1)												/* CMPXCHG Ed,Gd */
		{	
			if (CPU_ArchitectureType<CPU_ARCHTYPE_486NEW) goto illegal_opcode;
			FillFlags();
			GetRMrd;
			if (rm >= 0xc0) {
				GetEArd;
				if (*eard==reg_eax) {
					*eard=*rmrd;
					SETFLAGBIT(ZF,1);
				} else {
					reg_eax=*eard;
					SETFLAGBIT(ZF,0);
				}
			} else {
				GetEAa;
				Bit32u val=LoadMd(eaa);
				if (val==reg_eax) {
					SaveMd(eaa,*rmrd);
					SETFLAGBIT(ZF,1);
				} else {
					SaveMd(eaa,val);	// cmpxchg always issues a write
					reg_eax=val;
					SETFLAGBIT(ZF,0);
				}
			}
			break;
		}
	CASE_0F_D(0xb2)												/* LSS Ed */
		{	
			GetRMrd;
			if (rm >= 0xc0) goto illegal_opcode;
			GetEAa;
			if (CPU_SetSegGeneral(ss,LoadMw(eaa+4))) RUNEXCEPTION();
			*rmrd=LoadMd(eaa);
			break;
		}
	CASE_0F_D(0xb3)												/* BTR Ed,Gd */
		{
			FillFlags();GetRMrd;
			Bit32u mask=1 << (*rmrd & 31);
			if (rm >= 0xc0 ) {
				GetEArd;
				SETFLAGBIT(CF,(*eard & mask));
				*eard&= ~mask;
			} else {
				GetEAa;eaa+=(((Bit32s)*rmrd)>>5)*4;
				Bit32u old=LoadMd(eaa);
				SETFLAGBIT(CF,(old & mask));
				SaveMd(eaa,old & ~mask);
			}
			break;
		}
	CASE_0F_D(0xb4)												/* LFS Ed */
		{	
			GetRMrd;
			if (rm >= 0xc0) goto illegal_opcode;
			GetEAa;
			if (CPU_SetSegGeneral(fs,LoadMw(eaa+4))) RUNEXCEPTION();
			*rmrd=LoadMd(eaa);
			break;
		}
	CASE_0F_D(0xb5)												/* LGS Ed */
		{	
			GetRMrd;
			if (rm >= 0xc0) goto illegal_opcode;
			GetEAa;
			if (CPU_SetSegGeneral(gs,LoadMw(eaa+4))) RUNEXCEPTION();
			*rmrd=LoadMd(eaa);
			break;
		}
	CASE_0F_D(0xb6)												/* MOVZX Gd,Eb */
		{
			GetRMrd;															
			if (rm >= 0xc0 ) {GetEArb;*rmrd=*earb;}
			else {GetEAa;*rmrd=LoadMb(eaa);}
			break;
		}
	CASE_0F_D(0xb7)												/* MOVXZ Gd,Ew */
		{
			GetRMrd;
			if (rm >= 0xc0 ) {GetEArw;*rmrd=*earw;}
			else {GetEAa;*rmrd=LoadMw(eaa);}
			break;
		}
	CASE_0F_D(0xba)												/* GRP8 Ed,Ib */
		{
			FillFlags();GetRM;
			if (rm >= 0xc0 ) {
				GetEArd;
				Bit32u mask=1 << (Fetchb() & 31);
				SETFLAGBIT(CF,(*eard & mask));
				switch (rm & 0x38) {
				case 0x20:											/* BT */
					break;
				case 0x28:											/* BTS */
					*eard|=mask;
					break;
				case 0x30:											/* BTR */
					*eard&=~mask;
					break;
				case 0x38:											/* BTC */
					if (GETFLAG(CF)) *eard&=~mask;
					else *eard|=mask;
					break;
				default:
					E_Exit("CPU:66:0F:BA:Illegal subfunction %X",rm & 0x38);
				}
			} else {
				GetEAa;Bit32u old=LoadMd(eaa);
				Bit32u mask=1 << (Fetchb() & 31);
				SETFLAGBIT(CF,(old & mask));
				switch (rm & 0x38) {
				case 0x20:											/* BT */
					break;
				case 0x28:											/* BTS */
					SaveMd(eaa,old|mask);
					break;
				case 0x30:											/* BTR */
					SaveMd(eaa,old & ~mask);
					break;
				case 0x38:											/* BTC */
					if (GETFLAG(CF)) old&=~mask;
					else old|=mask;
					SaveMd(eaa,old);
					break;
				default:
					E_Exit("CPU:66:0F:BA:Illegal subfunction %X",rm & 0x38);
				}
			}
			break;
		}
	CASE_0F_D(0xbb)												/* BTC Ed,Gd */
		{
			FillFlags();GetRMrd;
			Bit32u mask=1 << (*rmrd & 31);
			if (rm >= 0xc0 ) {
				GetEArd;
				SETFLAGBIT(CF,(*eard & mask));
				*eard^=mask;
			} else {
				GetEAa;eaa+=(((Bit32s)*rmrd)>>5)*4;
				Bit32u old=LoadMd(eaa);
				SETFLAGBIT(CF,(old & mask));
				SaveMd(eaa,old ^ mask);
			}
			break;
		}
	CASE_0F_D(0xbc)												/* BSF Gd,Ed */
		{
			GetRMrd;
			Bit32u result,value;
			if (rm >= 0xc0) { GetEArd; value=*eard; } 
			else			{ GetEAa; value=LoadMd(eaa); }
			if (value==0) {
				SETFLAGBIT(ZF,true);
			} else {
				result = 0;
				while ((value & 0x01)==0) { result++; value>>=1; }
				SETFLAGBIT(ZF,false);
				*rmrd = result;
			}
			lflags.type=t_UNKNOWN;
			break;
		}
	CASE_0F_D(0xbd)												/*  BSR Gd,Ed */
		{
			GetRMrd;
			Bit32u result,value;
			if (rm >= 0xc0) { GetEArd; value=*eard; } 
			else			{ GetEAa; value=LoadMd(eaa); }
			if (value==0) {
				SETFLAGBIT(ZF,true);
			} else {
				result = 31;	// Operandsize-1
				while ((value & 0x80000000)==0) { result--; value<<=1; }
				SETFLAGBIT(ZF,false);
				*rmrd = result;
			}
			lflags.type=t_UNKNOWN;
			break;
		}
	CASE_0F_D(0xbe)												/* MOVSX Gd,Eb */
		{
			GetRMrd;															
			if (rm >= 0xc0 ) {GetEArb;*rmrd=*(Bit8s *)earb;}
			else {GetEAa;*rmrd=LoadMbs(eaa);}
			break;
		}
	CASE_0F_D(0xbf)												/* MOVSX Gd,Ew */
		{
			GetRMrd;															
			if (rm >= 0xc0 ) {GetEArw;*rmrd=*(Bit16s *)earw;}
			else {GetEAa;*rmrd=LoadMws(eaa);}
			break;
		}
	CASE_0F_D(0xc1)												/* XADD Gd,Ed */
		{
			if (CPU_ArchitectureType<CPU_ARCHTYPE_486OLD) goto illegal_opcode;
			GetRMrd;Bit32u oldrmrd=*rmrd;
			if (rm >= 0xc0 ) {GetEArd;*rmrd=*eard;*eard+=oldrmrd;}
			else {GetEAa;*rmrd=LoadMd(eaa);SaveMd(eaa,LoadMd(eaa)+oldrmrd);}
			break;
		}
	CASE_0F_D(0xc8)												/* BSWAP EAX */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_486OLD) goto illegal_opcode;
		BSWAPD(reg_eax);break;
	CASE_0F_D(0xc9)												/* BSWAP ECX */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_486OLD) goto illegal_opcode;
		BSWAPD(reg_ecx);break;
	CASE_0F_D(0xca)												/* BSWAP EDX */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_486OLD) goto illegal_opcode;
		BSWAPD(reg_edx);break;
	CASE_0F_D(0xcb)												/* BSWAP EBX */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_486OLD) goto illegal_opcode;
		BSWAPD(reg_ebx);break;
	CASE_0F_D(0xcc)												/* BSWAP ESP */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_486OLD) goto illegal_opcode;
		BSWAPD(reg_esp);break;
	CASE_0F_D(0xcd)												/* BSWAP EBP */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_486OLD) goto illegal_opcode;
		BSWAPD(reg_ebp);break;
	CASE_0F_D(0xce)												/* BSWAP ESI */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_486OLD) goto illegal_opcode;
		BSWAPD(reg_esi);break;
	CASE_0F_D(0xcf)												/* BSWAP EDI */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_486OLD) goto illegal_opcode;
		BSWAPD(reg_edi);break;
#include "prefix_0f_mmx.h"