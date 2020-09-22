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

	CASE_B(0x00)												/* ADD Eb,Gb */
		RMEbGb(ADDB);break;
	CASE_W(0x01)												/* ADD Ew,Gw */
		RMEwGw(ADDW);break;	
	CASE_B(0x02)												/* ADD Gb,Eb */
		RMGbEb(ADDB);break;
	CASE_W(0x03)												/* ADD Gw,Ew */
		RMGwEw(ADDW);break;
	CASE_B(0x04)												/* ADD AL,Ib */
		ALIb(ADDB);break;
	CASE_W(0x05)												/* ADD AX,Iw */
		AXIw(ADDW);break;
	CASE_W(0x06)												/* PUSH ES */		
		Push_16(SegValue(es));break;
	CASE_W(0x07)												/* POP ES */
		if (CPU_PopSeg(es,false)) RUNEXCEPTION();
		break;
	CASE_B(0x08)												/* OR Eb,Gb */
		RMEbGb(ORB);break;
	CASE_W(0x09)												/* OR Ew,Gw */
		RMEwGw(ORW);break;
	CASE_B(0x0a)												/* OR Gb,Eb */
		RMGbEb(ORB);break;
	CASE_W(0x0b)												/* OR Gw,Ew */
		RMGwEw(ORW);break;
	CASE_B(0x0c)												/* OR AL,Ib */
		ALIb(ORB);break;
	CASE_W(0x0d)												/* OR AX,Iw */
		AXIw(ORW);break;
	CASE_W(0x0e)												/* PUSH CS */		
		Push_16(SegValue(cs));break;
	CASE_B(0x0f)												/* 2 byte opcodes*/
#if CPU_CORE < CPU_ARCHTYPE_286
		if (CPU_ArchitectureType < CPU_ARCHTYPE_286) {
			/* 8086 emulation: treat as "POP CS" */
			if (CPU_PopSeg(cs,false)) RUNEXCEPTION();
			break;
		}
		else
#endif
		{
			core.opcode_index|=OPCODE_0F;
			goto restart_opcode;
		} break;
	CASE_B(0x10)												/* ADC Eb,Gb */
		RMEbGb(ADCB);break;
	CASE_W(0x11)												/* ADC Ew,Gw */
		RMEwGw(ADCW);break;	
	CASE_B(0x12)												/* ADC Gb,Eb */
		RMGbEb(ADCB);break;
	CASE_W(0x13)												/* ADC Gw,Ew */
		RMGwEw(ADCW);break;
	CASE_B(0x14)												/* ADC AL,Ib */
		ALIb(ADCB);break;
	CASE_W(0x15)												/* ADC AX,Iw */
		AXIw(ADCW);break;
	CASE_W(0x16)												/* PUSH SS */		
		Push_16(SegValue(ss));break;
	CASE_W(0x17)												/* POP SS */
		if (CPU_PopSeg(ss,false)) RUNEXCEPTION();
		CPU_Cycles++; //Always do another instruction
		break;
	CASE_B(0x18)												/* SBB Eb,Gb */
		RMEbGb(SBBB);break;
	CASE_W(0x19)												/* SBB Ew,Gw */
		RMEwGw(SBBW);break;
	CASE_B(0x1a)												/* SBB Gb,Eb */
		RMGbEb(SBBB);break;
	CASE_W(0x1b)												/* SBB Gw,Ew */
		RMGwEw(SBBW);break;
	CASE_B(0x1c)												/* SBB AL,Ib */
		ALIb(SBBB);break;
	CASE_W(0x1d)												/* SBB AX,Iw */
		AXIw(SBBW);break;
	CASE_W(0x1e)												/* PUSH DS */		
		Push_16(SegValue(ds));break;
	CASE_W(0x1f)												/* POP DS */
		if (CPU_PopSeg(ds,false)) RUNEXCEPTION();
		break;
	CASE_B(0x20)												/* AND Eb,Gb */
		RMEbGb(ANDB);break;
	CASE_W(0x21)												/* AND Ew,Gw */
		RMEwGw(ANDW);break;	
	CASE_B(0x22)												/* AND Gb,Eb */
		RMGbEb(ANDB);break;
	CASE_W(0x23)												/* AND Gw,Ew */
		RMGwEw(ANDW);break;
	CASE_B(0x24)												/* AND AL,Ib */
		ALIb(ANDB);break;
	CASE_W(0x25)												/* AND AX,Iw */
		AXIw(ANDW);break;
	CASE_B(0x26)												/* SEG ES: */
		DO_PREFIX_SEG(es);break;
	CASE_B(0x27)												/* DAA */
		DAA();break;
	CASE_B(0x28)												/* SUB Eb,Gb */
		RMEbGb(SUBB);break;
	CASE_W(0x29)												/* SUB Ew,Gw */
		RMEwGw(SUBW);break;
	CASE_B(0x2a)												/* SUB Gb,Eb */
		RMGbEb(SUBB);break;
	CASE_W(0x2b)												/* SUB Gw,Ew */
		RMGwEw(SUBW);break;
	CASE_B(0x2c)												/* SUB AL,Ib */
		ALIb(SUBB);break;
	CASE_W(0x2d)												/* SUB AX,Iw */
		AXIw(SUBW);break;
	CASE_B(0x2e)												/* SEG CS: */
		DO_PREFIX_SEG(cs);break;
	CASE_B(0x2f)												/* DAS */
		DAS();break;  
	CASE_B(0x30)												/* XOR Eb,Gb */
		RMEbGb(XORB);break;
	CASE_W(0x31)												/* XOR Ew,Gw */
		RMEwGw(XORW);break;	
	CASE_B(0x32)												/* XOR Gb,Eb */
		RMGbEb(XORB);break;
	CASE_W(0x33)												/* XOR Gw,Ew */
		RMGwEw(XORW);break;
	CASE_B(0x34)												/* XOR AL,Ib */
		ALIb(XORB);break;
	CASE_W(0x35)												/* XOR AX,Iw */
		AXIw(XORW);break;
	CASE_B(0x36)												/* SEG SS: */
		DO_PREFIX_SEG(ss);break;
	CASE_B(0x37)												/* AAA */
		AAA();break;  
	CASE_B(0x38)												/* CMP Eb,Gb */
		RMEbGb(CMPB);break;
	CASE_W(0x39)												/* CMP Ew,Gw */
		RMEwGw(CMPW);break;
	CASE_B(0x3a)												/* CMP Gb,Eb */
		RMGbEb(CMPB);break;
	CASE_W(0x3b)												/* CMP Gw,Ew */
		RMGwEw(CMPW);break;
	CASE_B(0x3c)												/* CMP AL,Ib */
		ALIb(CMPB);break;
	CASE_W(0x3d)												/* CMP AX,Iw */
		AXIw(CMPW);break;
	CASE_B(0x3e)												/* SEG DS: */
		DO_PREFIX_SEG(ds);break;
	CASE_B(0x3f)												/* AAS */
		AAS();break;
	CASE_W(0x40)												/* INC AX */
		INCW(reg_ax,LoadRw,SaveRw);break;
	CASE_W(0x41)												/* INC CX */
		INCW(reg_cx,LoadRw,SaveRw);break;
	CASE_W(0x42)												/* INC DX */
		INCW(reg_dx,LoadRw,SaveRw);break;
	CASE_W(0x43)												/* INC BX */
		INCW(reg_bx,LoadRw,SaveRw);break;
	CASE_W(0x44)												/* INC SP */
		INCW(reg_sp,LoadRw,SaveRw);break;
	CASE_W(0x45)												/* INC BP */
		INCW(reg_bp,LoadRw,SaveRw);break;
	CASE_W(0x46)												/* INC SI */
		INCW(reg_si,LoadRw,SaveRw);break;
	CASE_W(0x47)												/* INC DI */
		INCW(reg_di,LoadRw,SaveRw);break;
	CASE_W(0x48)												/* DEC AX */
		DECW(reg_ax,LoadRw,SaveRw);break;
	CASE_W(0x49)												/* DEC CX */
  		DECW(reg_cx,LoadRw,SaveRw);break;
	CASE_W(0x4a)												/* DEC DX */
		DECW(reg_dx,LoadRw,SaveRw);break;
	CASE_W(0x4b)												/* DEC BX */
		DECW(reg_bx,LoadRw,SaveRw);break;
	CASE_W(0x4c)												/* DEC SP */
		DECW(reg_sp,LoadRw,SaveRw);break;
	CASE_W(0x4d)												/* DEC BP */
		DECW(reg_bp,LoadRw,SaveRw);break;
	CASE_W(0x4e)												/* DEC SI */
		DECW(reg_si,LoadRw,SaveRw);break;
	CASE_W(0x4f)												/* DEC DI */
		DECW(reg_di,LoadRw,SaveRw);break;
	CASE_W(0x50)												/* PUSH AX */
		Push_16(reg_ax);break;
	CASE_W(0x51)												/* PUSH CX */
		Push_16(reg_cx);break;
	CASE_W(0x52)												/* PUSH DX */
		Push_16(reg_dx);break;
	CASE_W(0x53)												/* PUSH BX */
		Push_16(reg_bx);break;
	CASE_W(0x54)												/* PUSH SP */
		if (CPU_ArchitectureType >= CPU_ARCHTYPE_286)
			Push_16(reg_sp);
		else /* 8086 decrements SP then pushes it */
			Push_16(reg_sp-2u);
		break;
	CASE_W(0x55)												/* PUSH BP */
		Push_16(reg_bp);break;
	CASE_W(0x56)												/* PUSH SI */
		Push_16(reg_si);break;
	CASE_W(0x57)												/* PUSH DI */
		Push_16(reg_di);break;
	CASE_W(0x58)												/* POP AX */
		reg_ax=Pop_16();break;
	CASE_W(0x59)												/* POP CX */
		reg_cx=Pop_16();break;
	CASE_W(0x5a)												/* POP DX */
		reg_dx=Pop_16();break;
	CASE_W(0x5b)												/* POP BX */
		reg_bx=Pop_16();break;
	CASE_W(0x5c)												/* POP SP */
		reg_sp=Pop_16();break;
	CASE_W(0x5d)												/* POP BP */
		reg_bp=Pop_16();break;
	CASE_W(0x5e)												/* POP SI */
		reg_si=Pop_16();break;
	CASE_W(0x5f)												/* POP DI */
		reg_di=Pop_16();break;
	CASE_W(0x60)												/* PUSHA */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_80186) goto illegal_opcode;
		{
			Bit32u old_esp = reg_esp;
			try {
				Bit16u old_sp = (CPU_ArchitectureType >= CPU_ARCHTYPE_286 ? reg_sp : (reg_sp-10));
				Push_16(reg_ax);Push_16(reg_cx);Push_16(reg_dx);Push_16(reg_bx);
				Push_16(old_sp);Push_16(reg_bp);Push_16(reg_si);Push_16(reg_di);
			}
			catch (GuestPageFaultException &pf) {
				(void)pf;
				LOG_MSG("PUSHA interrupted by page fault");
				reg_esp = old_esp;
				throw;
			}
		} break;
	CASE_W(0x61)												/* POPA */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_80186) goto illegal_opcode;
		{
			Bit32u old_esp = reg_esp;
			try {
				reg_di=Pop_16();reg_si=Pop_16();reg_bp=Pop_16();Pop_16();//Don't save SP
				reg_bx=Pop_16();reg_dx=Pop_16();reg_cx=Pop_16();reg_ax=Pop_16();
			}
			catch (GuestPageFaultException &pf) {
				(void)pf;
				LOG_MSG("POPA interrupted by page fault");
				reg_esp = old_esp;
				throw;
			}
		} break;
	CASE_W(0x62)												/* BOUND */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_80186) goto illegal_opcode;
		{
			Bit16s bound_min, bound_max;
			GetRMrw;GetEAa;
			bound_min=(Bit16s)LoadMw(eaa);
			bound_max=(Bit16s)LoadMw(eaa+2u);
			if ( (((Bit16s)*rmrw) < bound_min) || (((Bit16s)*rmrw) > bound_max) ) {
				EXCEPTION(5);
			}
		}
		break;
	CASE_W(0x63)												/* ARPL Ew,Rw */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_286) goto illegal_opcode;
		{
			if ((reg_flags & FLAG_VM) || (!cpu.pmode)) goto illegal_opcode;
			GetRMrw;
			if (rm >= 0xc0 ) {
				GetEArw;Bitu new_sel=*earw;
				CPU_ARPL(new_sel,*rmrw);
				*earw=(Bit16u)new_sel;
			} else {
				GetEAa;Bitu new_sel=LoadMw(eaa);
				CPU_ARPL(new_sel,*rmrw);
				SaveMw(eaa,(Bit16u)new_sel);
			}
		}
		break;
	CASE_B(0x64)												/* SEG FS: */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_386) goto illegal_opcode;
		DO_PREFIX_SEG(fs);break;
	CASE_B(0x65)												/* SEG GS: */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_386) goto illegal_opcode;
		DO_PREFIX_SEG(gs);break;
#if CPU_CORE >= CPU_ARCHTYPE_386
	CASE_B(0x66)												/* Operand Size Prefix (386+) */
		core.opcode_index=(cpu.code.big^0x1u)*0x200u;
		goto restart_opcode;
#endif
#if CPU_CORE >= CPU_ARCHTYPE_386
	CASE_B(0x67)												/* Address Size Prefix (386+) */
		DO_PREFIX_ADDR();
#endif
	CASE_W(0x68)												/* PUSH Iw */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_80186) goto illegal_opcode;
		Push_16(Fetchw());break;
	CASE_W(0x69)												/* IMUL Gw,Ew,Iw */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_80186) goto illegal_opcode;
		RMGwEwOp3(DIMULW,Fetchws());
		break;
	CASE_W(0x6a)												/* PUSH Ib */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_80186) goto illegal_opcode;
		Push_16((Bit16u)Fetchbs());
		break;
	CASE_W(0x6b)												/* IMUL Gw,Ew,Ib */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_80186) goto illegal_opcode;
		RMGwEwOp3(DIMULW,Fetchbs());
		break;
	CASE_B(0x6c)												/* INSB */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_80186) goto illegal_opcode;
		if (CPU_IO_Exception(reg_dx,1)) RUNEXCEPTION();
		DoString(R_INSB);break;
	CASE_W(0x6d)												/* INSW */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_80186) goto illegal_opcode;
		if (CPU_IO_Exception(reg_dx,2)) RUNEXCEPTION();
		DoString(R_INSW);break;
	CASE_B(0x6e)												/* OUTSB */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_80186) goto illegal_opcode;
		if (CPU_IO_Exception(reg_dx,1)) RUNEXCEPTION();
		DoString(R_OUTSB);break;
	CASE_W(0x6f)												/* OUTSW */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_80186) goto illegal_opcode;
		if (CPU_IO_Exception(reg_dx,2)) RUNEXCEPTION();
		DoString(R_OUTSW);break;
	CASE_W(0x70)												/* JO */
		JumpCond16_b(TFLG_O);break;
	CASE_W(0x71)												/* JNO */
		JumpCond16_b(TFLG_NO);break;
	CASE_W(0x72)												/* JB */
		JumpCond16_b(TFLG_B);break;
	CASE_W(0x73)												/* JNB */
		JumpCond16_b(TFLG_NB);break;
	CASE_W(0x74)												/* JZ */
  		JumpCond16_b(TFLG_Z);break;
	CASE_W(0x75)												/* JNZ */
		JumpCond16_b(TFLG_NZ);break;
	CASE_W(0x76)												/* JBE */
		JumpCond16_b(TFLG_BE);break;
	CASE_W(0x77)												/* JNBE */
		JumpCond16_b(TFLG_NBE);break;
	CASE_W(0x78)												/* JS */
		JumpCond16_b(TFLG_S);break;
	CASE_W(0x79)												/* JNS */
		JumpCond16_b(TFLG_NS);break;
	CASE_W(0x7a)												/* JP */
		JumpCond16_b(TFLG_P);break;
	CASE_W(0x7b)												/* JNP */
		JumpCond16_b(TFLG_NP);break;
	CASE_W(0x7c)												/* JL */
		JumpCond16_b(TFLG_L);break;
	CASE_W(0x7d)												/* JNL */
		JumpCond16_b(TFLG_NL);break;
	CASE_W(0x7e)												/* JLE */
		JumpCond16_b(TFLG_LE);break;
	CASE_W(0x7f)												/* JNLE */
		JumpCond16_b(TFLG_NLE);break;
	CASE_B(0x80)												/* Grpl Eb,Ib */
	CASE_B(0x82)												/* Grpl Eb,Ib Mirror instruction*/
		{
			GetRM;Bitu which=(rm>>3)&7;
			if (rm>= 0xc0) {
				GetEArb;Bit8u ib=Fetchb();
				switch (which) {
				case 0x00:ADDB(*earb,ib,LoadRb,SaveRb);break;
				case 0x01: ORB(*earb,ib,LoadRb,SaveRb);break;
				case 0x02:ADCB(*earb,ib,LoadRb,SaveRb);break;
				case 0x03:SBBB(*earb,ib,LoadRb,SaveRb);break;
				case 0x04:ANDB(*earb,ib,LoadRb,SaveRb);break;
				case 0x05:SUBB(*earb,ib,LoadRb,SaveRb);break;
				case 0x06:XORB(*earb,ib,LoadRb,SaveRb);break;
				case 0x07:CMPB(*earb,ib,LoadRb,SaveRb);break;
				}
			} else {
				GetEAa;Bit8u ib=Fetchb();
				switch (which) {
				case 0x00:ADDB(eaa,ib,LoadMb,SaveMb);break;
				case 0x01: ORB(eaa,ib,LoadMb,SaveMb);break;
				case 0x02:ADCB(eaa,ib,LoadMb,SaveMb);break;
				case 0x03:SBBB(eaa,ib,LoadMb,SaveMb);break;
				case 0x04:ANDB(eaa,ib,LoadMb,SaveMb);break;
				case 0x05:SUBB(eaa,ib,LoadMb,SaveMb);break;
				case 0x06:XORB(eaa,ib,LoadMb,SaveMb);break;
				case 0x07:CMPB(eaa,ib,LoadMb,SaveMb);break;
				}
			}
			break;
		}
	CASE_W(0x81)												/* Grpl Ew,Iw */
		{
			GetRM;Bitu which=(rm>>3)&7;
			if (rm>= 0xc0) {
				GetEArw;Bit16u iw=Fetchw();
				switch (which) {
				case 0x00:ADDW(*earw,iw,LoadRw,SaveRw);break;
				case 0x01: ORW(*earw,iw,LoadRw,SaveRw);break;
				case 0x02:ADCW(*earw,iw,LoadRw,SaveRw);break;
				case 0x03:SBBW(*earw,iw,LoadRw,SaveRw);break;
				case 0x04:ANDW(*earw,iw,LoadRw,SaveRw);break;
				case 0x05:SUBW(*earw,iw,LoadRw,SaveRw);break;
				case 0x06:XORW(*earw,iw,LoadRw,SaveRw);break;
				case 0x07:CMPW(*earw,iw,LoadRw,SaveRw);break;
				}
			} else {
				GetEAa;Bit16u iw=Fetchw();
				switch (which) {
				case 0x00:ADDW(eaa,iw,LoadMw,SaveMw);break;
				case 0x01: ORW(eaa,iw,LoadMw,SaveMw);break;
				case 0x02:ADCW(eaa,iw,LoadMw,SaveMw);break;
				case 0x03:SBBW(eaa,iw,LoadMw,SaveMw);break;
				case 0x04:ANDW(eaa,iw,LoadMw,SaveMw);break;
				case 0x05:SUBW(eaa,iw,LoadMw,SaveMw);break;
				case 0x06:XORW(eaa,iw,LoadMw,SaveMw);break;
				case 0x07:CMPW(eaa,iw,LoadMw,SaveMw);break;
				}
			}
			break;
		}
	CASE_W(0x83)												/* Grpl Ew,Ix */
		{
			GetRM;Bitu which=(rm>>3)&7;
			if (rm>= 0xc0) {
				GetEArw;Bit16u iw=(Bit16u)Fetchbs();
				switch (which) {
				case 0x00:ADDW(*earw,iw,LoadRw,SaveRw);break;
				case 0x01: ORW(*earw,iw,LoadRw,SaveRw);break;
				case 0x02:ADCW(*earw,iw,LoadRw,SaveRw);break;
				case 0x03:SBBW(*earw,iw,LoadRw,SaveRw);break;
				case 0x04:ANDW(*earw,iw,LoadRw,SaveRw);break;
				case 0x05:SUBW(*earw,iw,LoadRw,SaveRw);break;
				case 0x06:XORW(*earw,iw,LoadRw,SaveRw);break;
				case 0x07:CMPW(*earw,iw,LoadRw,SaveRw);break;
				}
			} else {
				GetEAa;Bit16u iw=(Bit16u)Fetchbs();
				switch (which) {
				case 0x00:ADDW(eaa,iw,LoadMw,SaveMw);break;
				case 0x01: ORW(eaa,iw,LoadMw,SaveMw);break;
				case 0x02:ADCW(eaa,iw,LoadMw,SaveMw);break;
				case 0x03:SBBW(eaa,iw,LoadMw,SaveMw);break;
				case 0x04:ANDW(eaa,iw,LoadMw,SaveMw);break;
				case 0x05:SUBW(eaa,iw,LoadMw,SaveMw);break;
				case 0x06:XORW(eaa,iw,LoadMw,SaveMw);break;
				case 0x07:CMPW(eaa,iw,LoadMw,SaveMw);break;
				}
			}
			break;
		}
	CASE_B(0x84)												/* TEST Eb,Gb */
		RMEbGb(TESTB);
		break;
	CASE_W(0x85)												/* TEST Ew,Gw */
		RMEwGw(TESTW);
		break;
	CASE_B(0x86)												/* XCHG Eb,Gb */
		{	
			GetRMrb;Bit8u oldrmrb=*rmrb;
			if (rm >= 0xc0 ) {GetEArb;*rmrb=*earb;*earb=oldrmrb;}
			else {GetEAa;*rmrb=LoadMb(eaa);SaveMb(eaa,oldrmrb);}
			break;
		}
	CASE_W(0x87)												/* XCHG Ew,Gw */
		{	
			GetRMrw;Bit16u oldrmrw=*rmrw;
			if (rm >= 0xc0 ) {GetEArw;*rmrw=*earw;*earw=oldrmrw;}
			else {GetEAa;*rmrw=LoadMw(eaa);SaveMw(eaa,oldrmrw);}
			break;
		}
	CASE_B(0x88)												/* MOV Eb,Gb */
		{	
			GetRMrb;
			if (rm >= 0xc0 ) {GetEArb;*earb=*rmrb;}
			else {
				if (cpu.pmode) {
					if (GCC_UNLIKELY((rm==0x05) && (!cpu.code.big))) {
						Descriptor desc;
						cpu.gdt.GetDescriptor(SegValue(core.base_val_ds),desc);
						if ((desc.Type()==DESC_CODE_R_NC_A) || (desc.Type()==DESC_CODE_R_NC_NA)) {
							CPU_Exception(EXCEPTION_GP,SegValue(core.base_val_ds) & 0xfffc);
							continue;
						}
					}
				}
				GetEAa;SaveMb(eaa,*rmrb);
			}
			break;
		}
	CASE_W(0x89)												/* MOV Ew,Gw */
		{	
			GetRMrw;
			if (rm >= 0xc0 ) {GetEArw;*earw=*rmrw;}
			else {GetEAa;SaveMw(eaa,*rmrw);}
			break;
		}
	CASE_B(0x8a)												/* MOV Gb,Eb */
		{	
			GetRMrb;
			if (rm >= 0xc0 ) {GetEArb;*rmrb=*earb;}
			else {GetEAa;*rmrb=LoadMb(eaa);}
			break;
		}
	CASE_W(0x8b)												/* MOV Gw,Ew */
		{	
			GetRMrw;
			if (rm >= 0xc0 ) {GetEArw;*rmrw=*earw;}
			else {GetEAa;*rmrw=LoadMw(eaa);}
			break;
		}
	CASE_W(0x8c)												/* Mov Ew,Sw */
		{
			GetRM;Bit16u val;Bitu which=(rm>>3)&7;
			switch (which) {
			case 0x00:					/* MOV Ew,ES */
				val=SegValue(es);break;
			case 0x01:					/* MOV Ew,CS */
				val=SegValue(cs);break;
			case 0x02:					/* MOV Ew,SS */
				val=SegValue(ss);break;
			case 0x03:					/* MOV Ew,DS */
				val=SegValue(ds);break;
			case 0x04:					/* MOV Ew,FS */
				val=SegValue(fs);break;
			case 0x05:					/* MOV Ew,GS */
				val=SegValue(gs);break;
			default:
				LOG(LOG_CPU,LOG_ERROR)("CPU:8c:Illegal RM Byte");
				goto illegal_opcode;
			}
			if (rm >= 0xc0 ) {GetEArw;*earw=val;}
			else {GetEAa;SaveMw(eaa,val);}
			break;
		}
	CASE_W(0x8d)												/* LEA Gw */
		{
			//Little hack to always use segprefixed version
			BaseDS=BaseSS=0;
			GetRMrw;
			if (TEST_PREFIX_ADDR) {
				*rmrw=(Bit16u)(*EATable[256+rm])();
			} else {
				*rmrw=(Bit16u)(*EATable[rm])();
			}
			break;
		}
	CASE_B(0x8e)												/* MOV Sw,Ew */
		{
			GetRM;Bit16u val;Bitu which=(rm>>3)&7;
			if (rm >= 0xc0 ) {GetEArw;val=*earw;}
			else {GetEAa;val=LoadMw(eaa);}
			switch (which) {
#if CPU_CORE <= CPU_ARCHTYPE_8086
			case 0x01:					/* MOV CS,Ew (8086 only) */
#endif
			case 0x02:					/* MOV SS,Ew */
				CPU_Cycles++; //Always do another instruction
			case 0x00:					/* MOV ES,Ew */
			case 0x03:					/* MOV DS,Ew */
			case 0x05:					/* MOV GS,Ew */
			case 0x04:					/* MOV FS,Ew */
				if (CPU_SetSegGeneral((SegNames)which,val)) RUNEXCEPTION();
				break;
			default:
				goto illegal_opcode;
			}
			break;
		}							
	CASE_W(0x8f)												/* POP Ew */
		{
			Bit32u old_esp = reg_esp;

			try {
				Bit16u val=Pop_16();
				GetRM;
				if (rm >= 0xc0 ) {GetEArw;*earw=val;}
				else {GetEAa;SaveMw(eaa,val);}
			}
			catch (GuestPageFaultException &pf) {
				(void)pf;
				reg_esp = old_esp;
				throw;
			}
		} break;
	CASE_B(0x90)												/* NOP */
		break;
	CASE_W(0x91)												/* XCHG CX,AX */
		{ Bit16u temp=reg_ax;reg_ax=reg_cx;reg_cx=temp; }
		break;
	CASE_W(0x92)												/* XCHG DX,AX */
		{ Bit16u temp=reg_ax;reg_ax=reg_dx;reg_dx=temp; }
		break;
	CASE_W(0x93)												/* XCHG BX,AX */
		{ Bit16u temp=reg_ax;reg_ax=reg_bx;reg_bx=temp; }
		break;
	CASE_W(0x94)												/* XCHG SP,AX */
		{ Bit16u temp=reg_ax;reg_ax=reg_sp;reg_sp=temp; }
		break;
	CASE_W(0x95)												/* XCHG BP,AX */
		{ Bit16u temp=reg_ax;reg_ax=reg_bp;reg_bp=temp; }
		break;
	CASE_W(0x96)												/* XCHG SI,AX */
		{ Bit16u temp=reg_ax;reg_ax=reg_si;reg_si=temp; }
		break;
	CASE_W(0x97)												/* XCHG DI,AX */
		{ Bit16u temp=reg_ax;reg_ax=reg_di;reg_di=temp; }
		break;
	CASE_W(0x98)												/* CBW */
		reg_ax=(Bit16u)((Bit8s)reg_al);break;
	CASE_W(0x99)												/* CWD */
		if (reg_ax & 0x8000) reg_dx=0xffff;else reg_dx=0;
		break;
	CASE_W(0x9a)												/* CALL Ap */
		{ 
			FillFlags();
			Bit16u newip=Fetchw();Bit16u newcs=Fetchw();
			CPU_CALL(false,newcs,newip,GETIP);
#if CPU_TRAP_CHECK
			if (GETFLAG(TF)) {	
				cpudecoder=CPU_TRAP_DECODER;
				return CBRET_NONE;
			}
#endif
			continue;
		}
	CASE_B(0x9b)												/* WAIT */
		break; /* No waiting here */
	CASE_W(0x9c)												/* PUSHF */
		if (CPU_PUSHF(false)) RUNEXCEPTION();
		break;
	CASE_W(0x9d)												/* POPF */
		if (CPU_POPF(false)) RUNEXCEPTION();
#if CPU_TRAP_CHECK
		if (GETFLAG(TF)) {	
			cpudecoder=CPU_TRAP_DECODER;
			goto decode_end;
		}
#endif
#if	CPU_PIC_CHECK
		if (GETFLAG(IF) && PIC_IRQCheck) goto decode_end;
#endif
		break;
	CASE_B(0x9e)												/* SAHF */
		SETFLAGSb(reg_ah);
		break;
	CASE_B(0x9f)												/* LAHF */
		FillFlags();
		reg_ah=reg_flags&0xff;
		break;
	CASE_B(0xa0)												/* MOV AL,Ob */
		{ /* NTS: GetEADirect may jump instead to the GP# trigger code if the offset exceeds the segment limit.
		          For whatever reason, NOT signalling GP# in that condition prevents Windows 95 OSR2 from starting a DOS VM. Weird. */
			GetEADirect(1);
			reg_al=LoadMb(eaa);
		}
		break;
	CASE_W(0xa1)												/* MOV AX,Ow */
		{ /* NTS: GetEADirect may jump instead to the GP# trigger code if the offset exceeds the segment limit.
		          For whatever reason, NOT signalling GP# in that condition prevents Windows 95 OSR2 from starting a DOS VM. Weird. */
			GetEADirect(2);
			reg_ax=LoadMw(eaa);
		}
		break;
	CASE_B(0xa2)												/* MOV Ob,AL */
		{ /* NTS: GetEADirect may jump instead to the GP# trigger code if the offset exceeds the segment limit.
		          For whatever reason, NOT signalling GP# in that condition prevents Windows 95 OSR2 from starting a DOS VM. Weird. */
			GetEADirect(1);
			SaveMb(eaa,reg_al);
		}
		break;
	CASE_W(0xa3)												/* MOV Ow,AX */
		{ /* NTS: GetEADirect may jump instead to the GP# trigger code if the offset exceeds the segment limit.
		          For whatever reason, NOT signalling GP# in that condition prevents Windows 95 OSR2 from starting a DOS VM. Weird. */
			GetEADirect(2);
			SaveMw(eaa,reg_ax);
		}
		break;
	CASE_B(0xa4)												/* MOVSB */
		DoString(R_MOVSB);break;
	CASE_W(0xa5)												/* MOVSW */
		DoString(R_MOVSW);break;
	CASE_B(0xa6)												/* CMPSB */
		DoString(R_CMPSB);break;
	CASE_W(0xa7)												/* CMPSW */
		DoString(R_CMPSW);break;
	CASE_B(0xa8)												/* TEST AL,Ib */
		ALIb(TESTB);break;
	CASE_W(0xa9)												/* TEST AX,Iw */
		AXIw(TESTW);break;
	CASE_B(0xaa)												/* STOSB */
		DoString(R_STOSB);break;
	CASE_W(0xab)												/* STOSW */
		DoString(R_STOSW);break;
	CASE_B(0xac)												/* LODSB */
		DoString(R_LODSB);break;
	CASE_W(0xad)												/* LODSW */
		DoString(R_LODSW);break;
	CASE_B(0xae)												/* SCASB */
		DoString(R_SCASB);break;
	CASE_W(0xaf)												/* SCASW */
		DoString(R_SCASW);break;
	CASE_B(0xb0)												/* MOV AL,Ib */
		reg_al=Fetchb();break;
	CASE_B(0xb1)												/* MOV CL,Ib */
		reg_cl=Fetchb();break;
	CASE_B(0xb2)												/* MOV DL,Ib */
		reg_dl=Fetchb();break;
	CASE_B(0xb3)												/* MOV BL,Ib */
		reg_bl=Fetchb();break;
	CASE_B(0xb4)												/* MOV AH,Ib */
		reg_ah=Fetchb();break;
	CASE_B(0xb5)												/* MOV CH,Ib */
		reg_ch=Fetchb();break;
	CASE_B(0xb6)												/* MOV DH,Ib */
		reg_dh=Fetchb();break;
	CASE_B(0xb7)												/* MOV BH,Ib */
		reg_bh=Fetchb();break;
	CASE_W(0xb8)												/* MOV AX,Iw */
		reg_ax=Fetchw();break;
	CASE_W(0xb9)												/* MOV CX,Iw */
		reg_cx=Fetchw();break;
	CASE_W(0xba)												/* MOV DX,Iw */
		reg_dx=Fetchw();break;
	CASE_W(0xbb)												/* MOV BX,Iw */
		reg_bx=Fetchw();break;
	CASE_W(0xbc)												/* MOV SP,Iw */
		reg_sp=Fetchw();break;
	CASE_W(0xbd)												/* MOV BP.Iw */
		reg_bp=Fetchw();break;
	CASE_W(0xbe)												/* MOV SI,Iw */
		reg_si=Fetchw();break;
	CASE_W(0xbf)												/* MOV DI,Iw */
		reg_di=Fetchw();break;
#if CPU_CORE >= CPU_ARCHTYPE_80186
	CASE_B(0xc0)												/* GRP2 Eb,Ib */
		if (CPU_ArchitectureType < CPU_ARCHTYPE_80186) abort();
		GRP2B(Fetchb());break;
	CASE_W(0xc1)												/* GRP2 Ew,Ib */
		if (CPU_ArchitectureType < CPU_ARCHTYPE_80186) abort();
		GRP2W(Fetchb());break;
#endif
	CASE_W(0xc2)												/* RETN Iw */
		{
			Bit32u old_esp = reg_esp;

			try {
				/* this is structured either to complete RET or leave registers unmodified if interrupted by page fault */
				Bit32u new_eip = Pop_16();
				reg_esp += Fetchw();
				reg_eip = new_eip;
			}
			catch (GuestPageFaultException &pf) {
				(void)pf;
				reg_esp = old_esp; /* restore stack pointer */
				throw;
			}
		} continue;
	CASE_W(0xc3)												/* RETN */
		reg_eip=Pop_16();
		continue;
	CASE_W(0xc4)												/* LES */
		{	
			GetRMrw;
			if (rm >= 0xc0) goto illegal_opcode;
			GetEAa;
			if (CPU_SetSegGeneral(es,LoadMw(eaa+2))) RUNEXCEPTION();
			*rmrw=LoadMw(eaa);
			break;
		}
	CASE_W(0xc5)												/* LDS */
		{	
			GetRMrw;
			if (rm >= 0xc0) goto illegal_opcode;
			GetEAa;
			if (CPU_SetSegGeneral(ds,LoadMw(eaa+2))) RUNEXCEPTION();
			*rmrw=LoadMw(eaa);
			break;
		}
	CASE_B(0xc6)												/* MOV Eb,Ib */
		{
			GetRM;
			if (rm >= 0xc0) {GetEArb;*earb=Fetchb();}
			else {GetEAa;SaveMb(eaa,Fetchb());}
			break;
		}
	CASE_W(0xc7)												/* MOV EW,Iw */
		{
			GetRM;
			if (rm >= 0xc0) {GetEArw;*earw=Fetchw();}
			else {GetEAa;SaveMw(eaa,Fetchw());}
			break;
		}
	CASE_W(0xc8)												/* ENTER Iw,Ib */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_80186) goto illegal_opcode;
		{
			Bitu bytes=Fetchw();
			Bitu level=Fetchb();
			CPU_ENTER(false,bytes,level);
		}
		break;
	CASE_W(0xc9)												/* LEAVE */
		if (CPU_ArchitectureType<CPU_ARCHTYPE_80186) goto illegal_opcode;
		{
			Bit32u old_esp = reg_esp;

			reg_esp &= cpu.stack.notmask;
			reg_esp |= reg_ebp&cpu.stack.mask;
			try {
				reg_bp = Pop_16();
			}
			catch (GuestPageFaultException &pf) {
				(void)pf;
				reg_esp = old_esp;
				throw;
			}
		} break;
	CASE_W(0xca)												/* RETF Iw */
		{
			Bitu words=Fetchw();
			FillFlags();
			CPU_RET(false,words,GETIP);
			continue;
		}
	CASE_W(0xcb)												/* RETF */			
		FillFlags();
		CPU_RET(false,0,GETIP);
		continue;
	CASE_B(0xcc)												/* INT3 */
#if C_DEBUG	
		FillFlags();
		if (DEBUG_Breakpoint())
			return (Bits)debugCallback;
		if (DEBUG_IntBreakpoint(3))
			return (Bits)debugCallback;
#endif			
		CPU_SW_Interrupt_NoIOPLCheck(3,GETIP);
#if CPU_TRAP_CHECK
		cpu.trap_skip=true;
#endif
		continue;
	CASE_B(0xcd)												/* INT Ib */	
		{
			Bit8u num=Fetchb();
#if C_DEBUG
			FillFlags();
			if (DEBUG_IntBreakpoint(num)) {
				return (Bits)debugCallback;
			}
#endif
			CPU_SW_Interrupt(num,GETIP);
#if CPU_TRAP_CHECK
			cpu.trap_skip=true;
#endif
			continue;
		}
	CASE_B(0xce)												/* INTO */
		if (get_OF()) {
			CPU_SW_Interrupt(4,GETIP);
#if CPU_TRAP_CHECK
			cpu.trap_skip=true;
#endif
			continue;
		}
		break;
	CASE_W(0xcf)												/* IRET */
		{
			CPU_IRET(false,GETIP);
#if CPU_TRAP_CHECK
			if (GETFLAG(TF)) {	
				cpudecoder=CPU_TRAP_DECODER;
				return CBRET_NONE;
			}
#endif
#if CPU_PIC_CHECK
			if (GETFLAG(IF) && PIC_IRQCheck) return CBRET_NONE;
#endif
			continue;
		}
	CASE_B(0xd0)												/* GRP2 Eb,1 */
		GRP2B(1);break;
	CASE_W(0xd1)												/* GRP2 Ew,1 */
		GRP2W(1);break;
	CASE_B(0xd2)												/* GRP2 Eb,CL */
		GRP2B(reg_cl);break;
	CASE_W(0xd3)												/* GRP2 Ew,CL */
		GRP2W(reg_cl);break;
	CASE_B(0xd4)												/* AAM Ib */
		AAM(Fetchb());break;
	CASE_B(0xd5)												/* AAD Ib */
		AAD(Fetchb());break;
	CASE_B(0xd6)												/* SALC */
		reg_al = get_CF() ? 0xFF : 0;
		break;
	CASE_B(0xd7)												/* XLAT */
		if (TEST_PREFIX_ADDR) {
			reg_al=LoadMb(BaseDS+(Bit32u)(reg_ebx+reg_al));
		} else {
			reg_al=LoadMb(BaseDS+(Bit16u)(reg_bx+reg_al));
		}
		break;
#ifdef CPU_FPU
	CASE_B(0xd8)												/* FPU ESC 0 */
		if (enable_fpu) {
			FPU_ESC(0);
		}
		else {
			Bit8u rm=Fetchb();
			if (rm<0xc0) { GetEAa; (void)eaa; }
		}
		break;
	CASE_B(0xd9)												/* FPU ESC 1 */
		if (enable_fpu) {
			FPU_ESC(1);
		}
		else {
			Bit8u rm=Fetchb();
			if (rm<0xc0) { GetEAa; (void)eaa; }
		}
		break;
	CASE_B(0xda)												/* FPU ESC 2 */
		if (enable_fpu) {
			FPU_ESC(2);
		}
		else {
			Bit8u rm=Fetchb();
			if (rm<0xc0) { GetEAa; (void)eaa; }
		}
		break;
	CASE_B(0xdb)												/* FPU ESC 3 */
		if (enable_fpu) {
			FPU_ESC(3);
		}
		else {
			Bit8u rm=Fetchb();
			if (rm<0xc0) { GetEAa; (void)eaa; }
		}
		break;
	CASE_B(0xdc)												/* FPU ESC 4 */
		if (enable_fpu) {
			FPU_ESC(4);
		}
		else {
			Bit8u rm=Fetchb();
			if (rm<0xc0) { GetEAa; (void)eaa; }
		}
		break;
	CASE_B(0xdd)												/* FPU ESC 5 */
		if (enable_fpu) {
			FPU_ESC(5);
		}
		else {
			Bit8u rm=Fetchb();
			if (rm<0xc0) { GetEAa; (void)eaa; }
		}
		break;
	CASE_B(0xde)												/* FPU ESC 6 */
		if (enable_fpu) {
			FPU_ESC(6);
		}
		else {
			Bit8u rm=Fetchb();
			if (rm<0xc0) { GetEAa; (void)eaa; }
		}
		break;
	CASE_B(0xdf)												/* FPU ESC 7 */
		if (enable_fpu) {
			FPU_ESC(7);
		}
		else {
			Bit8u rm=Fetchb();
			if (rm<0xc0) { GetEAa; (void)eaa; }
		}
		break;
#else 
	CASE_B(0xd8)												/* FPU ESC 0 */
	CASE_B(0xd9)												/* FPU ESC 1 */
	CASE_B(0xda)												/* FPU ESC 2 */
	CASE_B(0xdb)												/* FPU ESC 3 */
	CASE_B(0xdc)												/* FPU ESC 4 */
	CASE_B(0xdd)												/* FPU ESC 5 */
	CASE_B(0xde)												/* FPU ESC 6 */
	CASE_B(0xdf)												/* FPU ESC 7 */
		{
			LOG(LOG_CPU,LOG_NORMAL)("FPU used");
			Bit8u rm=Fetchb();
			if (rm<0xc0) GetEAa;
		}
		break;
#endif
	CASE_W(0xe0)												/* LOOPNZ */
		if (TEST_PREFIX_ADDR) {
			JumpCond16_b(--reg_ecx && !get_ZF());
		} else {
			JumpCond16_b(--reg_cx && !get_ZF());
		}
		break;
	CASE_W(0xe1)												/* LOOPZ */
		if (TEST_PREFIX_ADDR) {
			JumpCond16_b(--reg_ecx && get_ZF());
		} else {
			JumpCond16_b(--reg_cx && get_ZF());
		}
		break;
	CASE_W(0xe2)												/* LOOP */
		if (TEST_PREFIX_ADDR) {	
			JumpCond16_b(--reg_ecx);
		} else {
			JumpCond16_b(--reg_cx);
		}
		break;
	CASE_W(0xe3)												/* JCXZ */
		JumpCond16_b(!(reg_ecx & AddrMaskTable[core.prefixes& PREFIX_ADDR]));
		break;
	CASE_B(0xe4)												/* IN AL,Ib */
		{	
			Bitu port=Fetchb();
			if (CPU_IO_Exception(port,1)) RUNEXCEPTION();
			reg_al=IO_ReadB(port);
			break;
		}
	CASE_W(0xe5)												/* IN AX,Ib */
		{	
			Bitu port=Fetchb();
			if (CPU_IO_Exception(port,2)) RUNEXCEPTION();
			reg_ax=IO_ReadW(port);
			break;
		}
	CASE_B(0xe6)												/* OUT Ib,AL */
		{
			Bitu port=Fetchb();
			if (CPU_IO_Exception(port,1)) RUNEXCEPTION();
			IO_WriteB(port,reg_al);
			break;
		}		
	CASE_W(0xe7)												/* OUT Ib,AX */
		{
			Bitu port=Fetchb();
			if (CPU_IO_Exception(port,2)) RUNEXCEPTION();
			IO_WriteW(port,reg_ax);
			break;
		}
	CASE_W(0xe8)												/* CALL Jw */
		{ 
			/* must not adjust (E)IP until we have completed the instruction.
			 * if interrupted by a page fault, EIP must be unmodified. */
			Bit16u addip=(Bit16u)Fetchws();
			Bit16u here=GETIP;
			Push_16(here);
			reg_eip=(Bit16u)(addip+here);
			continue;
		}
	CASE_W(0xe9)												/* JMP Jw */
		{ 
			Bit16u addip=(Bit16u)Fetchws();
			SAVEIP;
			reg_eip=(Bit16u)(reg_eip+addip);
			continue;
		}
	CASE_W(0xea)												/* JMP Ap */
		{ 
			Bit16u newip=Fetchw();
			Bit16u newcs=Fetchw();
			FillFlags();
			CPU_JMP(false,newcs,newip,GETIP);
#if CPU_TRAP_CHECK
			if (GETFLAG(TF)) {	
				cpudecoder=CPU_TRAP_DECODER;
				return CBRET_NONE;
			}
#endif
			continue;
		}
	CASE_W(0xeb)												/* JMP Jb */
		{ 
			Bit16s addip=Fetchbs();
			SAVEIP;
			reg_eip=(Bit16u)(reg_eip+(Bit32u)addip);
			continue;
		}
	CASE_B(0xec)												/* IN AL,DX */
		if (CPU_IO_Exception(reg_dx,1)) RUNEXCEPTION();
		reg_al=IO_ReadB(reg_dx);
		break;
	CASE_W(0xed)												/* IN AX,DX */
		if (CPU_IO_Exception(reg_dx,2)) RUNEXCEPTION();
		reg_ax=IO_ReadW(reg_dx);
		break;
	CASE_B(0xee)												/* OUT DX,AL */
		if (CPU_IO_Exception(reg_dx,1)) RUNEXCEPTION();
		IO_WriteB(reg_dx,reg_al);
		break;
	CASE_W(0xef)												/* OUT DX,AX */
		if (CPU_IO_Exception(reg_dx,2)) RUNEXCEPTION();
		IO_WriteW(reg_dx,reg_ax);
		break;
	CASE_B(0xf0)												/* LOCK */
// todo: make an option to show this
//		LOG(LOG_CPU,LOG_NORMAL)("CPU:LOCK"); /* FIXME: see case D_LOCK in core_full/load.h */
		break;
	CASE_B(0xf1)												/* ICEBP */
		CPU_SW_Interrupt_NoIOPLCheck(1,GETIP);
#if CPU_TRAP_CHECK
		cpu.trap_skip=true;
#endif
		continue;
	CASE_B(0xf2)												/* REPNZ */
		DO_PREFIX_REP(false);	
		break;		
	CASE_B(0xf3)												/* REPZ */
		DO_PREFIX_REP(true);	
		break;		
	CASE_B(0xf4)												/* HLT */
		if (cpu.pmode && cpu.cpl) EXCEPTION(EXCEPTION_GP);
		FillFlags();
		CPU_HLT(GETIP);
		return CBRET_NONE;		//Needs to return for hlt cpu core
	CASE_B(0xf5)												/* CMC */
		FillFlags();
		SETFLAGBIT(CF,!(reg_flags & FLAG_CF));
		break;
	CASE_B(0xf6)												/* GRP3 Eb(,Ib) */
		{	
			GetRM;Bitu which=(rm>>3)&7;
			switch (which) {
			case 0x00:											/* TEST Eb,Ib */
			case 0x01:											/* TEST Eb,Ib Undocumented*/
				{
					if (rm >= 0xc0 ) {GetEArb;TESTB(*earb,Fetchb(),LoadRb,0)}
					else {GetEAa;TESTB(eaa,Fetchb(),LoadMb,0);}
					break;
				}
			case 0x02:											/* NOT Eb */
				{
					if (rm >= 0xc0 ) {GetEArb;*earb=~*earb;}
					else {GetEAa;SaveMb(eaa,~LoadMb(eaa));}
					break;
				}
			case 0x03:											/* NEG Eb */
				{
					lflags.type=t_NEGb;
					if (rm >= 0xc0 ) {
						GetEArb;lf_var1b=*earb;lf_resb=0-lf_var1b;
						*earb=lf_resb;
					} else {
						GetEAa;lf_var1b=LoadMb(eaa);lf_resb=0-lf_var1b;
 						SaveMb(eaa,lf_resb);
					}
					break;
				}
			case 0x04:											/* MUL AL,Eb */
				RMEb(MULB);
				break;
			case 0x05:											/* IMUL AL,Eb */
				RMEb(IMULB);
				break;
			case 0x06:											/* DIV Eb */
				RMEb(DIVB);
				break;
			case 0x07:											/* IDIV Eb */
				RMEb(IDIVB);
				break;
			}
			break;
		}
	CASE_W(0xf7)												/* GRP3 Ew(,Iw) */
		{ 
			GetRM;Bitu which=(rm>>3)&7;
			switch (which) {
			case 0x00:											/* TEST Ew,Iw */
			case 0x01:											/* TEST Ew,Iw Undocumented*/
				{
					if (rm >= 0xc0 ) {GetEArw;TESTW(*earw,Fetchw(),LoadRw,SaveRw);}
					else {GetEAa;TESTW(eaa,Fetchw(),LoadMw,SaveMw);}
					break;
				}
			case 0x02:											/* NOT Ew */
				{
					if (rm >= 0xc0 ) {GetEArw;*earw=~*earw;}
					else {GetEAa;SaveMw(eaa,(Bitu)(~LoadMw(eaa)));}
					break;
				}
			case 0x03:											/* NEG Ew */
				{
					lflags.type=t_NEGw;
					if (rm >= 0xc0 ) {
						GetEArw;lf_var1w=*earw;lf_resw=0-lf_var1w;
						*earw=lf_resw;
					} else {
						GetEAa;lf_var1w=LoadMw(eaa);lf_resw=0-lf_var1w;
 						SaveMw(eaa,lf_resw);
					}
					break;
				}
			case 0x04:											/* MUL AX,Ew */
				RMEw(MULW);
				break;
			case 0x05:											/* IMUL AX,Ew */
				RMEw(IMULW)
				break;
			case 0x06:											/* DIV Ew */
				RMEw(DIVW)
				break;
			case 0x07:											/* IDIV Ew */
				RMEw(IDIVW)
				break;
			}
			break;
		}
	CASE_B(0xf8)												/* CLC */
		FillFlags();
		SETFLAGBIT(CF,false);
		break;
	CASE_B(0xf9)												/* STC */
		FillFlags();
		SETFLAGBIT(CF,true);
		break;
	CASE_B(0xfa)												/* CLI */
do_cli:	if (CPU_CLI()) RUNEXCEPTION();
		break;
	CASE_B(0xfb)												/* STI */
		if (CPU_STI()) RUNEXCEPTION();
#if CPU_PIC_CHECK
		if (GETFLAG(IF) && PIC_IRQCheck) {
            // NTS: Do not immmediately break execution, but set the cycle count to a minimal
            //      value so that if a CLI follows immediately the interrupt will be ignored.
            //
            //      It turns out on a 486 that STI+CLI (right next to each other) does not
            //      trigger the CPU to process interrupts. Like this:
            //
            //      STI
            //      CLI
            //
            //      The FM music driver for the PC-98 version of Peret em Heru appears to have
            //      STI+CLI sequences for some reason in certain subroutines within the FM
            //      music interrupt handler, which should not be trigger points to process
            //      interrupts because the FM interrupt is non-reentrant and Peret is also
            //      calling another entry point to the FM driver from IRQ 2 (vsync). If
            //      IRQ 2 is processed while the FM interrupt is processing at the STI, the
            //      stack switch will overwrite the first and the FM interrupt will return
            //      by stale data and crash.
            //
            //      [https://github.com/joncampbell123/dosbox-x/issues/1162]
            //
            // NTS: The prior fix that set CPU_Cycles = 4 causes the normal core to get stuck
            //      and never break out (hanging DOSBox-X) when running PC-98 game Night Slave,
            //      so that isn't a long-term option.
            //
            //      Capping CPU_Cycles = 2 seems to break Commander Keen games because of the
            //      way the video vsync and wait loop works. It waits for retrace/vsync with
            //      interrupts disabled, then on vertical retrace, briefly enables interrupts
            //      to allow them to run by jumping to a STI + JMP short $+2 + CLI sequence.
            //      Note the code deliberately uses a JMP short delay to avoid STI + CLI and
            //      make sure interrupts process.
            {
                Bit8u b = FetchPeekb();
                if (b == 0xFAu) {
                    /* if the next opcode is CLI, then do CLI right here before the normal core
                     * has any chance to break and handle interrupts */
                    FetchDiscardb(); // discard opcode we peeked, and then go execute it
                    CPU_Cycles--; // we're executing another instruction, which should eat one CPU cycle
                    goto do_cli;
                }
            }
            // otherwise, break for interrupt handling as normal
            if (CPU_Cycles > 1) {
                CPU_CycleLeft += CPU_Cycles - 1;
                CPU_Cycles = 1;
            }
            else {
                goto decode_end;
            }
        }
#endif
		break;
	CASE_B(0xfc)												/* CLD */
		SETFLAGBIT(DF,false);
		cpu.direction=1;
		break;
	CASE_B(0xfd)												/* STD */
		SETFLAGBIT(DF,true);
		cpu.direction=-1;
		break;
	CASE_B(0xfe)												/* GRP4 Eb */
		{
			GetRM;Bitu which=(rm>>3)&7;
			switch (which) {
			case 0x00:										/* INC Eb */
				RMEb(INCB);
				break;		
			case 0x01:										/* DEC Eb */
				RMEb(DECB);
				break;
			case 0x07:										/* CallBack */
				{
					Bitu cb=Fetchw();
					FillFlags();SAVEIP;
					return (Bits)cb;
				}
			default:
				LOG(LOG_CPU,LOG_DEBUG)("Illegal GRP4 Call %d",(rm>>3) & 7);
				goto illegal_opcode;
			}
			break;
		}
	CASE_W(0xff)												/* GRP5 Ew */
		{
			GetRM;Bitu which=(rm>>3)&7;
			switch (which) {
			case 0x00:										/* INC Ew */
				RMEw(INCW);
				break;		
			case 0x01:										/* DEC Ew */
				RMEw(DECW);
				break;		
			case 0x02:										/* CALL Ev */
				{ /* either EIP is set to the call address or EIP does not change if interrupted by PF */
					Bit16u new_eip;
					if (rm >= 0xc0 ) {GetEArw;new_eip=*earw;}
					else {GetEAa;new_eip=LoadMw(eaa);}
					Push_16(GETIP); /* <- PF may happen here */
					reg_eip = new_eip;
				}
				continue;
			case 0x03:										/* CALL Ep */
				{
					if (rm >= 0xc0) goto illegal_opcode;
					GetEAa;
					Bit16u newip=LoadMw(eaa);
					Bit16u newcs=LoadMw(eaa+2);
					FillFlags();
					CPU_CALL(false,newcs,newip,GETIP);
#if CPU_TRAP_CHECK
					if (GETFLAG(TF)) {	
						cpudecoder=CPU_TRAP_DECODER;
						return CBRET_NONE;
					}
#endif
					continue;
				}
				break;
			case 0x04:										/* JMP Ev */	
				if (rm >= 0xc0 ) {GetEArw;reg_eip=*earw;}
				else {GetEAa;reg_eip=LoadMw(eaa);}
				continue;
			case 0x05:										/* JMP Ep */	
				{
					if (rm >= 0xc0) goto illegal_opcode;
					GetEAa;
					Bit16u newip=LoadMw(eaa);
					Bit16u newcs=LoadMw(eaa+2);
					FillFlags();
					CPU_JMP(false,newcs,newip,GETIP);
#if CPU_TRAP_CHECK
					if (GETFLAG(TF)) {	
						cpudecoder=CPU_TRAP_DECODER;
						return CBRET_NONE;
					}
#endif
					continue;
				}
				break;
			case 0x06:										/* PUSH Ev */
				if (rm >= 0xc0 ) {GetEArw;Push_16(*earw);}
				else {GetEAa;Push_16(LoadMw(eaa));}
				break;
			default:
				goto illegal_opcode;
			}
			break;
		}
			



