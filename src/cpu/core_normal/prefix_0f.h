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

	CASE_0F_W(0x00)												/* GRP 6 Exxx */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_286) goto illegal_opcode;
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
					if (rm >= 0xc0) {GetEArw;*earw=(uint16_t)saveval;}
					else {GetEAa;SaveMw(eaa,(uint16_t)saveval);}
				}
				break;
			case 0x02:case 0x03:case 0x04:case 0x05:
				{
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
				goto illegal_opcode;
			}
		}
		break;
	CASE_0F_W(0x01)												/* Group 7 Ew */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_286) goto illegal_opcode;
		{
			GetRM;Bitu which=(rm>>3)&7;
			if (rm < 0xc0)	{ //First ones all use EA
				GetEAa;Bitu limit;
				switch (which) {
					/* NTS: The 286 is documented to write 0xFF in the most significant byte of the 6-byte field,
					 *      while 386 and later is documented to write 0x00 (or the MSB of the base if 32-bit form).
					 *      Some programs, including Microsoft Windows 3.0, execute SGDT and then compare the most
					 *      significant byte against 0xFF. It does NOT use the standard Intel detection process. */
				case 0x00:										/* SGDT */
					SaveMw(eaa,(uint16_t)CPU_SGDT_limit());
					SaveMd(eaa+2,(uint32_t)(CPU_SGDT_base()|(CPU_ArchitectureType<CPU_ARCHTYPE_386?0xFF000000UL:0)));
					break;
					/* NTS: Same comments for SIDT as SGDT */
				case 0x01:										/* SIDT */
					SaveMw(eaa,(uint16_t)CPU_SIDT_limit());
					SaveMd(eaa+2,(uint32_t)(CPU_SIDT_base()|(CPU_ArchitectureType<CPU_ARCHTYPE_386?0xFF000000UL:0)));
					break;
				case 0x02:										/* LGDT */
					if (cpu.pmode && cpu.cpl) EXCEPTION(EXCEPTION_GP);
					CPU_LGDT(LoadMw(eaa),LoadMd(eaa+2) & 0xFFFFFF);
					break;
				case 0x03:										/* LIDT */
					if (cpu.pmode && cpu.cpl) EXCEPTION(EXCEPTION_GP);
					CPU_LIDT(LoadMw(eaa),LoadMd(eaa+2) & 0xFFFFFF);
					break;
				case 0x04:										/* SMSW */
					SaveMw(eaa,(uint16_t)CPU_SMSW());
					break;
				case 0x06:										/* LMSW */
					limit=LoadMw(eaa);
					if (CPU_LMSW(limit)) RUNEXCEPTION();
					break;
				case 0x07:										/* INVLPG */
					if (CPU_ArchitectureType<CPU_ARCHTYPE_486OLD) goto illegal_opcode;
					if (cpu.pmode && cpu.cpl) EXCEPTION(EXCEPTION_GP);
					PAGING_ClearTLB();
					break;
				}
			} else {
				GetEArw;
				switch (which) {
				case 0x02:										/* LGDT */
					if (cpu.pmode && cpu.cpl) EXCEPTION(EXCEPTION_GP);
					goto illegal_opcode;
				case 0x03:										/* LIDT */
					if (cpu.pmode && cpu.cpl) EXCEPTION(EXCEPTION_GP);
					goto illegal_opcode;
				case 0x04:										/* SMSW */
					*earw=(uint16_t)CPU_SMSW();
					break;
				case 0x06:										/* LMSW */
					if (CPU_LMSW(*earw)) RUNEXCEPTION();
					break;
				default:
					goto illegal_opcode;
				}
			}
		}
		break;
	CASE_0F_W(0x02)												/* LAR Gw,Ew */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_286) goto illegal_opcode;
		{
			if ((reg_flags & FLAG_VM) || (!cpu.pmode)) goto illegal_opcode;
			GetRMrw;Bitu ar=*rmrw;
			if (rm >= 0xc0) {
				GetEArw;CPU_LAR(*earw,ar);
			} else {
				GetEAa;CPU_LAR(LoadMw(eaa),ar);
			}
			*rmrw=(uint16_t)ar;
		}
		break;
	CASE_0F_W(0x03)												/* LSL Gw,Ew */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_286) goto illegal_opcode;
		{
			if ((reg_flags & FLAG_VM) || (!cpu.pmode)) goto illegal_opcode;
			GetRMrw;Bitu limit=*rmrw;
			if (rm >= 0xc0) {
				GetEArw;CPU_LSL(*earw,limit);
			} else {
				GetEAa;CPU_LSL(LoadMw(eaa),limit);
			}
			*rmrw=(uint16_t)limit;
		}
		break;
	CASE_0F_B(0x06)												/* CLTS */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_286) goto illegal_opcode;
		if (cpu.pmode && cpu.cpl) EXCEPTION(EXCEPTION_GP);
		cpu.cr0&=(~CR0_TASKSWITCH);
		break;
	CASE_0F_B(0x08)												/* INVD */
	CASE_0F_B(0x09)												/* WBINVD */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_486OLD) goto illegal_opcode;
		if (cpu.pmode && cpu.cpl) EXCEPTION(EXCEPTION_GP);
		break;
	CASE_0F_B(0x20)												/* MOV Rd.CRx */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_386) goto illegal_opcode;
		{
			GetRM;
			Bitu which=(rm >> 3) & 7;
			if (rm < 0xc0 ) {
				rm |= 0xc0;
				LOG(LOG_CPU,LOG_ERROR)("MOV XXX,CR%u with non-register",(int)which);
			}
			GetEArd;
			uint32_t crx_value;
			if (CPU_READ_CRX(which,crx_value)) RUNEXCEPTION();
			*eard=crx_value;
		}
		break;
	CASE_0F_B(0x21)												/* MOV Rd,DRx */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_386) goto illegal_opcode;
		{
			GetRM;
			Bitu which=(rm >> 3) & 7;
			if (rm < 0xc0 ) {
				rm |= 0xc0;
				LOG(LOG_CPU,LOG_ERROR)("MOV XXX,DR%u with non-register",(int)which);
			}
			GetEArd;
			uint32_t drx_value;
			if (CPU_READ_DRX(which,drx_value)) RUNEXCEPTION();
			*eard=drx_value;
		}
		break;
	CASE_0F_B(0x22)												/* MOV CRx,Rd */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_386) goto illegal_opcode;
		{
			GetRM;
			Bitu which=(rm >> 3) & 7;
			if (rm < 0xc0 ) {
				rm |= 0xc0;
				LOG(LOG_CPU,LOG_ERROR)("MOV XXX,CR%u with non-register",(int)which);
			}
			GetEArd;
			if (CPU_WRITE_CRX(which,*eard)) RUNEXCEPTION();
		}
		break;
	CASE_0F_B(0x23)												/* MOV DRx,Rd */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_386) goto illegal_opcode;
		{
			GetRM;
			Bitu which=(rm >> 3) & 7;
			if (rm < 0xc0 ) {
				rm |= 0xc0;
				LOG(LOG_CPU,LOG_ERROR)("MOV DR%u,XXX with non-register",(int)which);
			}
			GetEArd;
			if (CPU_WRITE_DRX(which,*eard)) RUNEXCEPTION();
		}
		break;
	CASE_0F_B(0x24)												/* MOV Rd,TRx */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_386) goto illegal_opcode;
		{
			GetRM;
			Bitu which=(rm >> 3) & 7;
			if (rm < 0xc0 ) {
				rm |= 0xc0;
				LOG(LOG_CPU,LOG_ERROR)("MOV XXX,TR%u with non-register",(int)which);
			}
			GetEArd;
			uint32_t trx_value;
			if (CPU_READ_TRX(which,trx_value)) RUNEXCEPTION();
			*eard=trx_value;
		}
		break;
	CASE_0F_B(0x26)												/* MOV TRx,Rd */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_386) goto illegal_opcode;
		{
			GetRM;
			Bitu which=(rm >> 3) & 7;
			if (rm < 0xc0 ) {
				rm |= 0xc0;
				LOG(LOG_CPU,LOG_ERROR)("MOV TR%u,XXX with non-register",(int)which);
			}
			GetEArd;
			if (CPU_WRITE_TRX(which,*eard)) RUNEXCEPTION();
		}
		break;
	CASE_0F_B(0x30)												/* WRMSR */
		{
			if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUM) goto illegal_opcode;
			if (!CPU_WRMSR()) goto illegal_opcode;
		}
		break;
	CASE_0F_B(0x31)												/* RDTSC */
		{
			if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUM) goto illegal_opcode;
			/* Use a fixed number when in auto cycles mode as else the reported value changes constantly */
			int64_t tsc=(int64_t)(PIC_FullIndex()*(double) (CPU_CycleAutoAdjust?70000:CPU_CycleMax));
			reg_edx=(uint32_t)(tsc>>32);
			reg_eax=(uint32_t)(tsc&0xffffffff);
		}
		break;

	// Pentium Pro Conditional Moves
	CASE_0F_W(0x40)												/* CMOVO */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PPROSLOW) goto illegal_opcode;
		MoveCond16(TFLG_O); break;
	CASE_0F_W(0x41)												/* CMOVNO */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PPROSLOW) goto illegal_opcode;
		MoveCond16(TFLG_NO); break;
	CASE_0F_W(0x42)												/* CMOVB */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PPROSLOW) goto illegal_opcode;
		MoveCond16(TFLG_B); break;
	CASE_0F_W(0x43)												/* CMOVNB */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PPROSLOW) goto illegal_opcode;
		MoveCond16(TFLG_NB); break;
	CASE_0F_W(0x44)												/* CMOVZ */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PPROSLOW) goto illegal_opcode;
		MoveCond16(TFLG_Z); break;
	CASE_0F_W(0x45)												/* CMOVNZ */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PPROSLOW) goto illegal_opcode;
		MoveCond16(TFLG_NZ); break;
	CASE_0F_W(0x46)												/* CMOVBE */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PPROSLOW) goto illegal_opcode;
		MoveCond16(TFLG_BE); break;
	CASE_0F_W(0x47)												/* CMOVNBE */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PPROSLOW) goto illegal_opcode;
		MoveCond16(TFLG_NBE); break;
	CASE_0F_W(0x48)												/* CMOVS */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PPROSLOW) goto illegal_opcode;
		MoveCond16(TFLG_S); break;
	CASE_0F_W(0x49)												/* CMOVNS */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PPROSLOW) goto illegal_opcode;
		MoveCond16(TFLG_NS); break;
	CASE_0F_W(0x4A)												/* CMOVP */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PPROSLOW) goto illegal_opcode;
		MoveCond16(TFLG_P); break;
	CASE_0F_W(0x4B)												/* CMOVNP */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PPROSLOW) goto illegal_opcode;
		MoveCond16(TFLG_NP); break;
	CASE_0F_W(0x4C)												/* CMOVL */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PPROSLOW) goto illegal_opcode;
		MoveCond16(TFLG_L); break;
	CASE_0F_W(0x4D)												/* CMOVNL */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PPROSLOW) goto illegal_opcode;
		MoveCond16(TFLG_NL); break;
	CASE_0F_W(0x4E)												/* CMOVLE */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PPROSLOW) goto illegal_opcode;
		MoveCond16(TFLG_LE); break;
	CASE_0F_W(0x4F)												/* CMOVNLE */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PPROSLOW) goto illegal_opcode;
		MoveCond16(TFLG_NLE); break;

	CASE_0F_B(0x32)												/* RDMSR */
		{
			if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUM) goto illegal_opcode;
			if (!CPU_RDMSR()) goto illegal_opcode;
		}
		break;
#if CPU_CORE >= CPU_ARCHTYPE_386
	CASE_0F_W(0x80)												/* JO */
		JumpCond16_w(TFLG_O);break;
	CASE_0F_W(0x81)												/* JNO */
		JumpCond16_w(TFLG_NO);break;
	CASE_0F_W(0x82)												/* JB */
		JumpCond16_w(TFLG_B);break;
	CASE_0F_W(0x83)												/* JNB */
		JumpCond16_w(TFLG_NB);break;
	CASE_0F_W(0x84)												/* JZ */
		JumpCond16_w(TFLG_Z);break;
	CASE_0F_W(0x85)												/* JNZ */
		JumpCond16_w(TFLG_NZ);break;
	CASE_0F_W(0x86)												/* JBE */
		JumpCond16_w(TFLG_BE);break;
	CASE_0F_W(0x87)												/* JNBE */
		JumpCond16_w(TFLG_NBE);break;
	CASE_0F_W(0x88)												/* JS */
		JumpCond16_w(TFLG_S);break;
	CASE_0F_W(0x89)												/* JNS */
		JumpCond16_w(TFLG_NS);break;
	CASE_0F_W(0x8a)												/* JP */
		JumpCond16_w(TFLG_P);break;
	CASE_0F_W(0x8b)												/* JNP */
		JumpCond16_w(TFLG_NP);break;
	CASE_0F_W(0x8c)												/* JL */
		JumpCond16_w(TFLG_L);break;
	CASE_0F_W(0x8d)												/* JNL */
		JumpCond16_w(TFLG_NL);break;
	CASE_0F_W(0x8e)												/* JLE */
		JumpCond16_w(TFLG_LE);break;
	CASE_0F_W(0x8f)												/* JNLE */
		JumpCond16_w(TFLG_NLE);break;
	CASE_0F_B(0x90)												/* SETO */
		SETcc(TFLG_O);break;
	CASE_0F_B(0x91)												/* SETNO */
		SETcc(TFLG_NO);break;
	CASE_0F_B(0x92)												/* SETB */
		SETcc(TFLG_B);break;
	CASE_0F_B(0x93)												/* SETNB */
		SETcc(TFLG_NB);break;
	CASE_0F_B(0x94)												/* SETZ */
		SETcc(TFLG_Z);break;
	CASE_0F_B(0x95)												/* SETNZ */
		SETcc(TFLG_NZ);	break;
	CASE_0F_B(0x96)												/* SETBE */
		SETcc(TFLG_BE);break;
	CASE_0F_B(0x97)												/* SETNBE */
		SETcc(TFLG_NBE);break;
	CASE_0F_B(0x98)												/* SETS */
		SETcc(TFLG_S);break;
	CASE_0F_B(0x99)												/* SETNS */
		SETcc(TFLG_NS);break;
	CASE_0F_B(0x9a)												/* SETP */
		SETcc(TFLG_P);break;
	CASE_0F_B(0x9b)												/* SETNP */
		SETcc(TFLG_NP);break;
	CASE_0F_B(0x9c)												/* SETL */
		SETcc(TFLG_L);break;
	CASE_0F_B(0x9d)												/* SETNL */
		SETcc(TFLG_NL);break;
	CASE_0F_B(0x9e)												/* SETLE */
		SETcc(TFLG_LE);break;
	CASE_0F_B(0x9f)												/* SETNLE */
		SETcc(TFLG_NLE);break;
#endif
	CASE_0F_W(0xa0)												/* PUSH FS */		
		if (CPU_ArchitectureType<CPU_ARCHTYPE_386) goto illegal_opcode;
		Push_16(SegValue(fs));break;
	CASE_0F_W(0xa1)												/* POP FS */	
		if (CPU_ArchitectureType<CPU_ARCHTYPE_386) goto illegal_opcode;
		if (CPU_PopSeg(fs,false)) RUNEXCEPTION();
		break;
	CASE_0F_B(0xa2)												/* CPUID */
		if (!CPU_CPUID()) goto illegal_opcode;
		break;
	CASE_0F_W(0xa3)												/* BT Ew,Gw */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_386) goto illegal_opcode;
		{
			FillFlags();GetRMrw;
			uint16_t mask=1u << (*rmrw & 15u);
			if (rm >= 0xc0 ) {
				GetEArw;
				SETFLAGBIT(CF,(*earw & mask));
			} else {
				GetEAa;eaa+=(PhysPt)((((int16_t)*rmrw)>>4)*2);
				if (!TEST_PREFIX_ADDR) FixEA16;
				uint16_t old=LoadMw(eaa);
				SETFLAGBIT(CF,(old & mask));
			}
			break;
		}
	CASE_0F_W(0xa4)												/* SHLD Ew,Gw,Ib */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_386) goto illegal_opcode;
		RMEwGwOp3(DSHLW,Fetchb());
		break;
	CASE_0F_W(0xa5)												/* SHLD Ew,Gw,CL */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_386) goto illegal_opcode;
		RMEwGwOp3(DSHLW,reg_cl);
		break;
	CASE_0F_W(0xa8)												/* PUSH GS */		
		if (CPU_ArchitectureType<CPU_ARCHTYPE_386) goto illegal_opcode;
		Push_16(SegValue(gs));break;
	CASE_0F_W(0xa9)												/* POP GS */		
		if (CPU_ArchitectureType<CPU_ARCHTYPE_386) goto illegal_opcode;
		if (CPU_PopSeg(gs,false)) RUNEXCEPTION();
		break;
	CASE_0F_W(0xab)												/* BTS Ew,Gw */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_386) goto illegal_opcode;
		{
			FillFlags();GetRMrw;
			uint16_t mask=1u << (*rmrw & 15u);
			if (rm >= 0xc0 ) {
				GetEArw;
				SETFLAGBIT(CF,(*earw & mask));
				*earw|=mask;
			} else {
				GetEAa;eaa+=(PhysPt)((((int16_t)*rmrw)>>4)*2);
				if (!TEST_PREFIX_ADDR) FixEA16;
				uint16_t old=LoadMw(eaa);
				SETFLAGBIT(CF,(old & mask));
				SaveMw(eaa,old | mask);
			}
			break;
		}
	CASE_0F_W(0xac)												/* SHRD Ew,Gw,Ib */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_386) goto illegal_opcode;
		RMEwGwOp3(DSHRW,Fetchb());
		break;
	CASE_0F_W(0xad)												/* SHRD Ew,Gw,CL */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_386) goto illegal_opcode;
		RMEwGwOp3(DSHRW,reg_cl);
		break;
	CASE_0F_W(0xaf)												/* IMUL Gw,Ew */
		RMGwEwOp3(DIMULW,*rmrw);
		break;
	CASE_0F_B(0xb0) 										/* cmpxchg Eb,Gb */
		{
			if (CPU_ArchitectureType<CPU_ARCHTYPE_486OLD) goto illegal_opcode;
			FillFlags();
			GetRMrb;
			if (rm >= 0xc0 ) {
				GetEArb;
				if (reg_al == *earb) {
					*earb=*rmrb;
					SETFLAGBIT(ZF,1);
				} else {
					reg_al = *earb;
					SETFLAGBIT(ZF,0);
				}
			} else {
   				GetEAa;
				uint8_t val = LoadMb(eaa);
				if (reg_al == val) { 
					SaveMb(eaa,*rmrb);
					SETFLAGBIT(ZF,1);
				} else {
					SaveMb(eaa,val);	// cmpxchg always issues a write
					reg_al = val;
					SETFLAGBIT(ZF,0);
				}
			}
			break;
		}
	CASE_0F_W(0xb1) 									/* cmpxchg Ew,Gw */
		{
			if (CPU_ArchitectureType<CPU_ARCHTYPE_486OLD) goto illegal_opcode;
			FillFlags();
			GetRMrw;
			if (rm >= 0xc0 ) {
				GetEArw;
				if(reg_ax == *earw) { 
					*earw = *rmrw;
					SETFLAGBIT(ZF,1);
				} else {
					reg_ax = *earw;
					SETFLAGBIT(ZF,0);
				}
			} else {
   				GetEAa;
				uint16_t val = LoadMw(eaa);
				if(reg_ax == val) { 
					SaveMw(eaa,*rmrw);
					SETFLAGBIT(ZF,1);
				} else {
					SaveMw(eaa,val);	// cmpxchg always issues a write
					reg_ax = val;
					SETFLAGBIT(ZF,0);
				}
			}
			break;
		}

	CASE_0F_W(0xb2)												/* LSS Ew */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_386) goto illegal_opcode;
		{	
			GetRMrw;
			if (rm >= 0xc0) goto illegal_opcode;
			GetEAa;
			if (CPU_SetSegGeneral(ss,LoadMw(eaa+2))) RUNEXCEPTION();
			*rmrw=LoadMw(eaa);
			break;
		}
	CASE_0F_W(0xb3)												/* BTR Ew,Gw */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_386) goto illegal_opcode;
		{
			FillFlags();GetRMrw;
			uint16_t mask=1u << (*rmrw & 15u);
			if (rm >= 0xc0 ) {
				GetEArw;
				SETFLAGBIT(CF,(*earw & mask));
				*earw&= ~mask;
			} else {
				GetEAa;eaa+=(PhysPt)((((int16_t)*rmrw)>>4)*2);
				if (!TEST_PREFIX_ADDR) FixEA16;
				uint16_t old=LoadMw(eaa);
				SETFLAGBIT(CF,(old & mask));
				SaveMw(eaa,old & ~mask);
			}
			break;
		}
	CASE_0F_W(0xb4)												/* LFS Ew */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_386) goto illegal_opcode;
		{	
			GetRMrw;
			if (rm >= 0xc0) goto illegal_opcode;
			GetEAa;
			if (CPU_SetSegGeneral(fs,LoadMw(eaa+2))) RUNEXCEPTION();
			*rmrw=LoadMw(eaa);
			break;
		}
	CASE_0F_W(0xb5)												/* LGS Ew */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_386) goto illegal_opcode;
		{	
			GetRMrw;
			if (rm >= 0xc0) goto illegal_opcode;
			GetEAa;
			if (CPU_SetSegGeneral(gs,LoadMw(eaa+2))) RUNEXCEPTION();
			*rmrw=LoadMw(eaa);
			break;
		}
	CASE_0F_W(0xb6)												/* MOVZX Gw,Eb */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_386) goto illegal_opcode;
		{
			GetRMrw;															
			if (rm >= 0xc0 ) {GetEArb;*rmrw=*earb;}
			else {GetEAa;*rmrw=LoadMb(eaa);}
			break;
		}
	CASE_0F_W(0xb7)												/* MOVZX Gw,Ew */
	CASE_0F_W(0xbf)												/* MOVSX Gw,Ew */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_386) goto illegal_opcode;
		{
			GetRMrw;															
			if (rm >= 0xc0 ) {GetEArw;*rmrw=*earw;}
			else {GetEAa;*rmrw=LoadMw(eaa);}
			break;
		}
	CASE_0F_W(0xba)												/* GRP8 Ew,Ib */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_386) goto illegal_opcode;
		{
			FillFlags();GetRM;
			if (rm >= 0xc0 ) {
				GetEArw;
				uint16_t mask=1 << (Fetchb() & 15);
				SETFLAGBIT(CF,(*earw & mask));
				switch (rm & 0x38) {
				case 0x20:										/* BT */
					break;
				case 0x28:										/* BTS */
					*earw|=mask;
					break;
				case 0x30:										/* BTR */
					*earw&= ~mask;
					break;
				case 0x38:										/* BTC */
					*earw^=mask;
					break;
				default:
					E_Exit("CPU:0F:BA:Illegal subfunction %X",rm & 0x38);
				}
			} else {
				GetEAa;uint16_t old=LoadMw(eaa);
				uint16_t mask=1 << (Fetchb() & 15);
				SETFLAGBIT(CF,(old & mask));
				switch (rm & 0x38) {
				case 0x20:										/* BT */
					break;
				case 0x28:										/* BTS */
					SaveMw(eaa,old|mask);
					break;
				case 0x30:										/* BTR */
					SaveMw(eaa,old & ~mask);
					break;
				case 0x38:										/* BTC */
					SaveMw(eaa,old ^ mask);
					break;
				default:
					E_Exit("CPU:0F:BA:Illegal subfunction %X",rm & 0x38);
				}
			}
			break;
		}
	CASE_0F_W(0xbb)												/* BTC Ew,Gw */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_386) goto illegal_opcode;
		{
			FillFlags();GetRMrw;
			uint16_t mask=1u << (*rmrw & 15u);
			if (rm >= 0xc0 ) {
				GetEArw;
				SETFLAGBIT(CF,(*earw & mask));
				*earw^=mask;
			} else {
				GetEAa;eaa+=(PhysPt)((((int16_t)*rmrw)>>4)*2);
				if (!TEST_PREFIX_ADDR) FixEA16;
				uint16_t old=LoadMw(eaa);
				SETFLAGBIT(CF,(old & mask));
				SaveMw(eaa,old ^ mask);
			}
			break;
		}
	CASE_0F_W(0xbc)												/* BSF Gw,Ew */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_386) goto illegal_opcode;
		{
			GetRMrw;
			uint16_t value;
			if (rm >= 0xc0) { GetEArw; value=*earw; } 
			else			{ GetEAa; value=LoadMw(eaa); }
			if (value==0) {
				SETFLAGBIT(ZF,true);
			} else {
				uint16_t result = 0;
				while ((value & 0x01)==0) { result++; value>>=1; }
				SETFLAGBIT(ZF,false);
				*rmrw = result;
			}
			lflags.type=t_UNKNOWN;
			break;
		}
	CASE_0F_W(0xbd)												/* BSR Gw,Ew */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_386) goto illegal_opcode;
		{
			GetRMrw;
			uint16_t value;
			if (rm >= 0xc0) { GetEArw; value=*earw; } 
			else			{ GetEAa; value=LoadMw(eaa); }
			if (value==0) {
				SETFLAGBIT(ZF,true);
			} else {
				uint16_t result = 15;	// Operandsize-1
				while ((value & 0x8000)==0) { result--; value<<=1; }
				SETFLAGBIT(ZF,false);
				*rmrw = result;
			}
			lflags.type=t_UNKNOWN;
			break;
		}
	CASE_0F_W(0xbe)												/* MOVSX Gw,Eb */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_386) goto illegal_opcode;
		{
			GetRMrw;															
			if (rm >= 0xc0 ) {GetEArb;*rmrw=(uint16_t)(*(int8_t *)earb);}
			else {GetEAa;*rmrw=(uint16_t)LoadMbs(eaa);}
			break;
		}
	CASE_0F_B(0xc0)												/* XADD Gb,Eb */
		{
			if (CPU_ArchitectureType<CPU_ARCHTYPE_486OLD) goto illegal_opcode;
			GetRMrb;uint8_t oldrmrb=*rmrb;
			if (rm >= 0xc0 ) {GetEArb;*rmrb=*earb;*earb+=oldrmrb;}
			else {GetEAa;*rmrb=LoadMb(eaa);SaveMb(eaa,LoadMb(eaa)+oldrmrb);}
			break;
		}
	CASE_0F_W(0xc1)												/* XADD Gw,Ew */
		{
			if (CPU_ArchitectureType<CPU_ARCHTYPE_486OLD) goto illegal_opcode;
			GetRMrw;uint16_t oldrmrw=*rmrw;
			if (rm >= 0xc0 ) {GetEArw;*rmrw=*earw;*earw+=oldrmrw;}
			else {GetEAa;*rmrw=LoadMw(eaa);SaveMw(eaa,LoadMw(eaa)+oldrmrw);}
			break;
		}
    CASE_0F_W(0xc7)
        {
            extern bool enable_cmpxchg8b;
            void CPU_CMPXCHG8B(PhysPt eaa);

            if (!enable_cmpxchg8b || CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUM) goto illegal_opcode;
            GetRM;
            if (((rm >> 3) & 7) == 1) { // CMPXCHG8B /1 r/m
                if (rm >= 0xc0 ) goto illegal_opcode;
                GetEAa;
                CPU_CMPXCHG8B(eaa);
            }
            else {
                goto illegal_opcode;
            }
            break;
        }
	CASE_0F_W(0xc8)												/* BSWAP AX */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_486OLD) goto illegal_opcode;
		BSWAPW(reg_ax);break;
	CASE_0F_W(0xc9)												/* BSWAP CX */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_486OLD) goto illegal_opcode;
		BSWAPW(reg_cx);break;
	CASE_0F_W(0xca)												/* BSWAP DX */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_486OLD) goto illegal_opcode;
		BSWAPW(reg_dx);break;
	CASE_0F_W(0xcb)												/* BSWAP BX */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_486OLD) goto illegal_opcode;
		BSWAPW(reg_bx);break;
	CASE_0F_W(0xcc)												/* BSWAP SP */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_486OLD) goto illegal_opcode;
		BSWAPW(reg_sp);break;
	CASE_0F_W(0xcd)												/* BSWAP BP */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_486OLD) goto illegal_opcode;
		BSWAPW(reg_bp);break;
	CASE_0F_W(0xce)												/* BSWAP SI */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_486OLD) goto illegal_opcode;
		BSWAPW(reg_si);break;
	CASE_0F_W(0xcf)												/* BSWAP DI */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_486OLD) goto illegal_opcode;
		BSWAPW(reg_di);break;
#if C_FPU
#define CASE_0F_MMX(x) CASE_0F_W(x)
#include "prefix_0f_mmx.h"
#undef CASE_0F_MMX
#endif
