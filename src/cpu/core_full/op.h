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

/* Do the actual opcode */
switch (inst.code.op) {
	case t_ADDb:	case t_ADDw:	case t_ADDd:
		lf_var1d=inst_op1_d;
		lf_var2d=inst_op2_d;
		inst_op1_d=lf_resd=lf_var1d + lf_var2d;
		lflags.type=inst.code.op;
		break;
	case t_CMPb:	case t_CMPw:	case t_CMPd:
	case t_SUBb:	case t_SUBw:	case t_SUBd:
		lf_var1d=inst_op1_d;
		lf_var2d=inst_op2_d;
		inst_op1_d=lf_resd=lf_var1d - lf_var2d;
		lflags.type=inst.code.op;
		break;
	case t_ORb:		case t_ORw:		case t_ORd:
		lf_var1d=inst_op1_d;
		lf_var2d=inst_op2_d;
		inst_op1_d=lf_resd=lf_var1d | lf_var2d;
		lflags.type=inst.code.op;
		break;
	case t_XORb:	case t_XORw:	case t_XORd:
		lf_var1d=inst_op1_d;
		lf_var2d=inst_op2_d;
		inst_op1_d=lf_resd=lf_var1d ^ lf_var2d;
		lflags.type=inst.code.op;
		break;
	case t_TESTb:	case t_TESTw:	case t_TESTd:
	case t_ANDb:	case t_ANDw:	case t_ANDd:
		lf_var1d=inst_op1_d;
		lf_var2d=inst_op2_d;
		inst_op1_d=lf_resd=lf_var1d & lf_var2d;
		lflags.type=inst.code.op;
		break;
	case t_ADCb:	case t_ADCw:	case t_ADCd:
		lflags.oldcf=(get_CF()!=0);
		lf_var1d=inst_op1_d;
		lf_var2d=inst_op2_d;
		inst_op1_d=lf_resd=(Bit32u)(lf_var1d + lf_var2d + lflags.oldcf);
		lflags.type=inst.code.op;
		break;
	case t_SBBb:	case t_SBBw:	case t_SBBd:
		lflags.oldcf=(get_CF()!=0);
		lf_var1d=inst_op1_d;
		lf_var2d=inst_op2_d;
		inst_op1_d=lf_resd=(Bit32u)(lf_var1d - lf_var2d - lflags.oldcf);
		lflags.type=inst.code.op;
		break;
	case t_INCb:	case t_INCw:	case t_INCd:
		LoadCF;
		lf_var1d=inst_op1_d;
		inst_op1_d=lf_resd=inst_op1_d+1;
		lflags.type=inst.code.op;
		break;
	case t_DECb:	case t_DECw:	case t_DECd:
		LoadCF;
		lf_var1d=inst_op1_d;
		inst_op1_d=lf_resd=inst_op1_d-1;
		lflags.type=inst.code.op;
		break;
/* Using the instructions.h defines */
	case t_ROLb:
		ROLB(inst_op1_b,inst_op2_b,LoadD,SaveD);
		break;
	case t_ROLw:
		ROLW(inst_op1_w,inst_op2_b,LoadD,SaveD);
		break;
	case t_ROLd:
		ROLD(inst_op1_d,inst_op2_b,LoadD,SaveD);
		break;

	case t_RORb:
		RORB(inst_op1_b,inst_op2_b,LoadD,SaveD);
		break;
	case t_RORw:
		RORW(inst_op1_w,inst_op2_b,LoadD,SaveD);
		break;
	case t_RORd:
		RORD(inst_op1_d,inst_op2_b,LoadD,SaveD);
		break;

	case t_RCLb:
		RCLB(inst_op1_b,inst_op2_b,LoadD,SaveD);
		break;
	case t_RCLw:
		RCLW(inst_op1_w,inst_op2_b,LoadD,SaveD);
		break;
	case t_RCLd:
		RCLD(inst_op1_d,inst_op2_b,LoadD,SaveD);
		break;

	case t_RCRb:
		RCRB(inst_op1_b,inst_op2_b,LoadD,SaveD);
		break;
	case t_RCRw:
		RCRW(inst_op1_w,inst_op2_b,LoadD,SaveD);
		break;
	case t_RCRd:
		RCRD(inst_op1_d,inst_op2_b,LoadD,SaveD);
		break;

	case t_SHLb:
		SHLB(inst_op1_b,inst_op2_b,LoadD,SaveD);
		break;
	case t_SHLw:
		SHLW(inst_op1_w,inst_op2_b,LoadD,SaveD);
		break;
	case t_SHLd:
		SHLD(inst_op1_d,inst_op2_b,LoadD,SaveD);
		break;

	case t_SHRb:
		SHRB(inst_op1_b,inst_op2_b,LoadD,SaveD);
		break;
	case t_SHRw:
		SHRW(inst_op1_w,inst_op2_b,LoadD,SaveD);
		break;
	case t_SHRd:
		SHRD(inst_op1_d,inst_op2_b,LoadD,SaveD);
		break;

	case t_SARb:
		SARB(inst_op1_b,inst_op2_b,LoadD,SaveD);
		break;
	case t_SARw:
		SARW(inst_op1_w,inst_op2_b,LoadD,SaveD);
		break;
	case t_SARd:
		SARD(inst_op1_d,inst_op2_b,LoadD,SaveD);
		break;

	case O_DSHLw:
		{
			DSHLW(inst_op1_w,inst_op2_w,inst_imm_b,LoadD,SaveD);
			break;
		}
	case O_DSHRw:
		{
			DSHRW(inst_op1_w,inst_op2_w,inst_imm_b,LoadD,SaveD);
			break;
		}
	case O_DSHLd:
		{
			DSHLD(inst_op1_d,inst_op2_d,inst_imm_b,LoadD,SaveD);
			break;
		}
	case O_DSHRd:
		{
			DSHRD(inst_op1_d,inst_op2_d,inst_imm_b,LoadD,SaveD);
			break;
		}

	case t_NEGb:
		lf_var1b=inst_op1_b;
		inst_op1_b=lf_resb=0-inst_op1_b;
		lflags.type=t_NEGb;
		break;
	case t_NEGw:
		lf_var1w=inst_op1_w;
		inst_op1_w=lf_resw=0-inst_op1_w;
		lflags.type=t_NEGw;
		break;
	case t_NEGd:
		lf_var1d=inst_op1_d;
		inst_op1_d=lf_resd=0-inst_op1_d;
		lflags.type=t_NEGd;
		break;
	
	case O_NOT:
		inst_op1_d=~inst_op1_d;
		break;	
		
	/* Special instructions */
	case O_IMULRw:
		DIMULW(inst_op1_ws,inst_op1_ws,inst_op2_ws,LoadD,SaveD);
		break;
	case O_IMULRd:
		DIMULD(inst_op1_ds,inst_op1_ds,inst_op2_ds,LoadD,SaveD);
		break;
	case O_MULb:
		MULB(inst_op1_b,LoadD,0);
		goto nextopcode;
	case O_MULw:
		MULW(inst_op1_w,LoadD,0);
		goto nextopcode;
	case O_MULd:
		MULD(inst_op1_d,LoadD,0);
		goto nextopcode;
	case O_IMULb:
		IMULB(inst_op1_b,LoadD,0);
		goto nextopcode;
	case O_IMULw:
		IMULW(inst_op1_w,LoadD,0);
		goto nextopcode;
	case O_IMULd:
		IMULD(inst_op1_d,LoadD,0);
		goto nextopcode;
	case O_DIVb:
		DIVB(inst_op1_b,LoadD,0);
		goto nextopcode;
	case O_DIVw:
		DIVW(inst_op1_w,LoadD,0);
		goto nextopcode;
	case O_DIVd:
		DIVD(inst_op1_d,LoadD,0);
		goto nextopcode;
	case O_IDIVb:
		IDIVB(inst_op1_b,LoadD,0);
		goto nextopcode;
	case O_IDIVw:
		IDIVW(inst_op1_w,LoadD,0);
		goto nextopcode;
	case O_IDIVd:
		IDIVD(inst_op1_d,LoadD,0);
		goto nextopcode;
	case O_AAM:
		AAM(inst_op1_b);
		goto nextopcode;
	case O_AAD:
		AAD(inst_op1_b);
		goto nextopcode;

	case O_C_O:		inst.cond=TFLG_O;	break;
	case O_C_NO:	inst.cond=TFLG_NO;	break;
	case O_C_B:		inst.cond=TFLG_B;	break;
	case O_C_NB:	inst.cond=TFLG_NB;	break;
	case O_C_Z:		inst.cond=TFLG_Z;	break;
	case O_C_NZ:	inst.cond=TFLG_NZ;	break;
	case O_C_BE:	inst.cond=TFLG_BE;	break;
	case O_C_NBE:	inst.cond=TFLG_NBE;	break;
	case O_C_S:		inst.cond=TFLG_S;	break;
	case O_C_NS:	inst.cond=TFLG_NS;	break;
	case O_C_P:		inst.cond=TFLG_P;	break;
	case O_C_NP:	inst.cond=TFLG_NP;	break;
	case O_C_L:		inst.cond=TFLG_L;	break;
	case O_C_NL:	inst.cond=TFLG_NL;	break;
	case O_C_LE:	inst.cond=TFLG_LE;	break;
	case O_C_NLE:	inst.cond=TFLG_NLE;	break;

	case O_ALOP:
		reg_al=LoadMb(inst.rm_eaa);
		goto nextopcode;
	case O_AXOP:
		reg_ax=LoadMw(inst.rm_eaa);
		goto nextopcode;
	case O_EAXOP:
		reg_eax=LoadMd(inst.rm_eaa);
		goto nextopcode;
	case O_OPAL:
		SaveMb(inst.rm_eaa,reg_al);
		goto nextopcode;
	case O_OPAX:
		SaveMw(inst.rm_eaa,reg_ax);
		goto nextopcode;
	case O_OPEAX:
		SaveMd(inst.rm_eaa,reg_eax);
		goto nextopcode;
	case O_SEGDS:
		inst.code.extra=ds;
		break;
	case O_SEGES:
		inst.code.extra=es;
		break;
	case O_SEGFS:
		inst.code.extra=fs;
		break;
	case O_SEGGS:
		inst.code.extra=gs;
		break;
	case O_SEGSS:
		inst.code.extra=ss;
		break;
	
	case O_LOOP:
		if (inst.prefix & PREFIX_ADDR) {
			if (--reg_ecx) break;
		} else {
			if (--reg_cx) break;
		}
		goto nextopcode;
	case O_LOOPZ:
		if (inst.prefix & PREFIX_ADDR) {
			if (--reg_ecx && get_ZF()) break;
		} else {
			if (--reg_cx && get_ZF()) break;
		}
		goto nextopcode;
	case O_LOOPNZ:
		if (inst.prefix & PREFIX_ADDR) {
			if (--reg_ecx && !get_ZF()) break;
		} else {
			if (--reg_cx && !get_ZF()) break;
		}
		goto nextopcode;
	case O_JCXZ:
		if (inst.prefix & PREFIX_ADDR) {
			if (reg_ecx) goto nextopcode;
		} else {
			if (reg_cx) goto nextopcode;
		}
		break;
	case O_XCHG_AX:
		{
			Bit16u temp=reg_ax;
			reg_ax=inst_op1_w;
			inst_op1_w=temp;
			break;
		}
	case O_XCHG_EAX:
		{
			Bit32u temp=reg_eax;
			reg_eax=inst_op1_d;
			inst_op1_d=temp;
			break;
		}
	case O_CALLNw:
		SaveIP();
		Push_16(reg_ip);
		break;
	case O_CALLNd:
		SaveIP();
		Push_32(reg_eip);
		break;
	case O_CALLFw:
		FillFlags();
		CPU_CALL(false,inst_op2_d,inst_op1_d,GetIP());
		continue;
	case O_CALLFd:
		FillFlags();
		CPU_CALL(true,inst_op2_d,inst_op1_d,GetIP());
		continue;
	case O_JMPFw:
		FillFlags();
		CPU_JMP(false,inst_op2_d,inst_op1_d,GetIP());
		continue;
	case O_JMPFd:
		FillFlags();
		CPU_JMP(true,inst_op2_d,inst_op1_d,GetIP());
		continue;
	case O_INT:
#if C_DEBUG
		FillFlags();
		if (((inst.entry & 0xFF)==0xcc) && DEBUG_Breakpoint()) 
			return (Bits)debugCallback;
		else if (DEBUG_IntBreakpoint(inst_op1_b)) 
			return (Bits)debugCallback;
#endif
		CPU_SW_Interrupt(inst_op1_b,GetIP());
		continue;
	case O_INb:
		if (CPU_IO_Exception(inst_op1_d,1)) RunException();
		reg_al=IO_ReadB(inst_op1_d);
		goto nextopcode;
	case O_INw:
		if (CPU_IO_Exception(inst_op1_d,2)) RunException();
		reg_ax=IO_ReadW(inst_op1_d);
		goto nextopcode;
	case O_INd:
		if (CPU_IO_Exception(inst_op1_d,4)) RunException();
		reg_eax=IO_ReadD(inst_op1_d);
		goto nextopcode;
	case O_OUTb:
		if (CPU_IO_Exception(inst_op1_d,1)) RunException();
		IO_WriteB(inst_op1_d,reg_al);
		goto nextopcode;
	case O_OUTw:
		if (CPU_IO_Exception(inst_op1_d,2)) RunException();
		IO_WriteW(inst_op1_d,reg_ax);
		goto nextopcode;
	case O_OUTd:
		if (CPU_IO_Exception(inst_op1_d,4)) RunException();
		IO_WriteD(inst_op1_d,reg_eax);
		goto nextopcode;
	case O_CBACK:
		FillFlags();SaveIP();
		return (Bits)inst_op1_d;
	case O_GRP6w:
	case O_GRP6d:
		if ((reg_flags & FLAG_VM) || (!cpu.pmode)) goto illegalopcode;
		switch (inst.rm_index) {
		case 0x00:	/* SLDT */
			inst_op1_d=(Bit32u)CPU_SLDT();
			break;
		case 0x01:	/* STR */
			inst_op1_d=(Bit32u)CPU_STR();
			break;
		case 0x02:	/* LLDT */
			if (cpu.cpl) EXCEPTION(EXCEPTION_GP);
			if (CPU_LLDT(inst_op1_d)) RunException();
			goto nextopcode;		/* Else value will saved */
		case 0x03:	/* LTR */
			if (cpu.cpl) EXCEPTION(EXCEPTION_GP);
			if (CPU_LTR(inst_op1_d)) RunException();
			goto nextopcode;		/* Else value will saved */
		case 0x04:	/* VERR */
			CPU_VERR(inst_op1_d);
			goto nextopcode;		/* Else value will saved */
		case 0x05:	/* VERW */
			CPU_VERW(inst_op1_d);
			goto nextopcode;		/* Else value will saved */
		default:
			LOG(LOG_CPU,LOG_ERROR)("Group 6 Illegal subfunction %lx",(unsigned long)inst.rm_index);
			goto illegalopcode;
		}
		break;
	case O_GRP7w:
	case O_GRP7d:
		switch (inst.rm_index) {
		case 0:		/* SGDT */
			SaveMw(inst.rm_eaa,(Bit16u)CPU_SGDT_limit());
			SaveMd(inst.rm_eaa+2,(Bit32u)CPU_SGDT_base());
			goto nextopcode;
		case 1:		/* SIDT */
			SaveMw(inst.rm_eaa,(Bit16u)CPU_SIDT_limit());
			SaveMd(inst.rm_eaa+2,(Bit32u)CPU_SIDT_base());
			goto nextopcode;
		case 2:		/* LGDT */
			if (cpu.pmode && cpu.cpl) EXCEPTION(EXCEPTION_GP);
			CPU_LGDT(LoadMw(inst.rm_eaa),LoadMd(inst.rm_eaa+2)&((inst.code.op == O_GRP7w) ? 0xFFFFFF : 0xFFFFFFFF));
			goto nextopcode;
		case 3:		/* LIDT */
			if (cpu.pmode && cpu.cpl) EXCEPTION(EXCEPTION_GP);
			CPU_LIDT(LoadMw(inst.rm_eaa),LoadMd(inst.rm_eaa+2)&((inst.code.op == O_GRP7w) ? 0xFFFFFF : 0xFFFFFFFF));
			goto nextopcode;
		case 4:		/* SMSW */
			inst_op1_d=(Bit32u)CPU_SMSW();
			break;
		case 6:		/* LMSW */
			FillFlags();
			if (CPU_LMSW(inst_op1_w)) RunException();
			goto nextopcode;
		case 7:		/* INVLPG */
			if (cpu.pmode && cpu.cpl) EXCEPTION(EXCEPTION_GP);
			FillFlags();
			PAGING_ClearTLB();
			goto nextopcode;
		default:
			LOG(LOG_CPU,LOG_ERROR)("Group 7 Illegal subfunction %lx",(unsigned long)inst.rm_index);
			goto illegalopcode;
		}
		break;
	case O_M_CRx_Rd:
		if (CPU_WRITE_CRX(inst.rm_index,inst_op1_d)) RunException();
		break;
	case O_M_Rd_CRx:
		if (CPU_READ_CRX(inst.rm_index,inst_op1_d)) RunException();
		break;
	case O_M_DRx_Rd:
		if (CPU_WRITE_DRX(inst.rm_index,inst_op1_d)) RunException();
		break;
	case O_M_Rd_DRx:
		if (CPU_READ_DRX(inst.rm_index,inst_op1_d)) RunException();
		break;
	case O_M_TRx_Rd:
		if (CPU_WRITE_TRX(inst.rm_index,inst_op1_d)) RunException();
		break;
	case O_M_Rd_TRx:
		if (CPU_READ_TRX(inst.rm_index,inst_op1_d)) RunException();
		break;
	case O_LAR:
		{
			Bitu ar=inst_op2_d;
			CPU_LAR(inst_op1_w,ar);
			inst_op1_d=(Bit32u)ar;
		}
		break;
	case O_LSL:
		{
			Bitu limit=inst_op2_d;
			CPU_LSL(inst_op1_w,limit);
			inst_op1_d=(Bit32u)limit;
		}
		break;
	case O_ARPL:
		{
			Bitu new_sel=inst_op1_d;
			CPU_ARPL(new_sel,inst_op2_d);
			inst_op1_d=(Bit32u)new_sel;
		}
		break;
	case O_BSFw:
		{
			FillFlags();
			if (!inst_op1_w) {
				SETFLAGBIT(ZF,true);
				goto nextopcode;
			} else {
				uint8_t count=0;
				while (1) {
					if (inst_op1_w & 0x1) break;
					count++;inst_op1_w>>=1;
				}
				inst_op1_d=count;
				SETFLAGBIT(ZF,false);
			}
		}
		break;
	case O_BSFd:
		{
			FillFlags();
			if (!inst_op1_d) {
				SETFLAGBIT(ZF,true);
				goto nextopcode;
			} else {
				uint8_t count=0;
				while (1) {
					if (inst_op1_d & 0x1) break;
					count++;inst_op1_d>>=1;
				}
				inst_op1_d=count;
				SETFLAGBIT(ZF,false);
			}
		}
		break;
	case O_BSRw:
		{
			FillFlags();
			if (!inst_op1_w) {
				SETFLAGBIT(ZF,true);
				goto nextopcode;
			} else {
				uint8_t count=15;
				while (1) {
					if (inst_op1_w & 0x8000) break;
					count--;inst_op1_w<<=1;
				}
				inst_op1_d=count;
				SETFLAGBIT(ZF,false);
			}
		}
		break;
	case O_BSRd:
		{
			FillFlags();
			if (!inst_op1_d) {
				SETFLAGBIT(ZF,true);
				goto nextopcode;
			} else {
				uint8_t count=31;
				while (1) {
					if (inst_op1_d & 0x80000000) break;
					count--;inst_op1_d<<=1;
				}
				inst_op1_d=count;
				SETFLAGBIT(ZF,false);
			}
		}
		break;
	case O_BTw:
		FillFlags();
		SETFLAGBIT(CF,(inst_op1_d & (1u << (inst_op2_d & 15u))));
		break;
	case O_BTSw:
		FillFlags();
		SETFLAGBIT(CF,(inst_op1_d & (1u << (inst_op2_d & 15u))));
		inst_op1_d|=(1u << (inst_op2_d & 15u));
		break;
	case O_BTCw:
		FillFlags();
		SETFLAGBIT(CF,(inst_op1_d & (1u << (inst_op2_d & 15u))));
		inst_op1_d^=(1u << (inst_op2_d & 15u));
		break;
	case O_BTRw:
		FillFlags();
		SETFLAGBIT(CF,(inst_op1_d & (1u << (inst_op2_d & 15u))));
		inst_op1_d&=~(1u << (inst_op2_d & 15u));
		break;
	case O_BTd:
		FillFlags();
		SETFLAGBIT(CF,(inst_op1_d & (1u << (inst_op2_d & 31u))));
		break;
	case O_BTSd:
		FillFlags();
		SETFLAGBIT(CF,(inst_op1_d & (1u << (inst_op2_d & 31u))));
		inst_op1_d|=(1u << (inst_op2_d & 31u));
		break;
	case O_BTCd:
		FillFlags();
		SETFLAGBIT(CF,(inst_op1_d & (1u << (inst_op2_d & 31))));
		inst_op1_d^=(1u << (inst_op2_d & 31u));
		break;
	case O_BTRd:
		FillFlags();
		SETFLAGBIT(CF,(inst_op1_d & (1u << (inst_op2_d & 31u))));
		inst_op1_d&=~(1u << (inst_op2_d & 31u));
		break;
	case O_BSWAPw:
		if (CPU_ArchitectureType<CPU_ARCHTYPE_486OLD) goto illegalopcode;
		BSWAPW(inst_op1_w);
		break;
	case O_BSWAPd:
		if (CPU_ArchitectureType<CPU_ARCHTYPE_486OLD) goto illegalopcode;
		BSWAPD(inst_op1_d);
		break;
	case O_CMPXCHG:
		if (CPU_ArchitectureType<CPU_ARCHTYPE_486NEW) goto illegalopcode;
		FillFlags();
		if (inst_op1_d==reg_eax) {
			inst_op1_d=reg_32(inst.rm_index);
			if (inst.rm<0xc0) SaveMd(inst.rm_eaa,inst_op1_d);	// early write-pf
			SETFLAGBIT(ZF,1);
		} else {
			if (inst.rm<0xc0) SaveMd(inst.rm_eaa,inst_op1_d);	// early write-pf
			reg_eax=inst_op1_d;
			SETFLAGBIT(ZF,0);
		}
		break;
	case O_FPU:
#if C_FPU
		switch (((inst.rm>=0xc0) << 3) | inst.code.save) {
		case 0x00:	FPU_ESC0_EA(inst.rm,inst.rm_eaa);break;
		case 0x01:	FPU_ESC1_EA(inst.rm,inst.rm_eaa);break;
		case 0x02:	FPU_ESC2_EA(inst.rm,inst.rm_eaa);break;
		case 0x03:	FPU_ESC3_EA(inst.rm,inst.rm_eaa);break;
		case 0x04:	FPU_ESC4_EA(inst.rm,inst.rm_eaa);break;
		case 0x05:	FPU_ESC5_EA(inst.rm,inst.rm_eaa);break;
		case 0x06:	FPU_ESC6_EA(inst.rm,inst.rm_eaa);break;
		case 0x07:	FPU_ESC7_EA(inst.rm,inst.rm_eaa);break;

		case 0x08:	FPU_ESC0_Normal(inst.rm);break;
		case 0x09:	FPU_ESC1_Normal(inst.rm);break;
		case 0x0a:	FPU_ESC2_Normal(inst.rm);break;
		case 0x0b:	FPU_ESC3_Normal(inst.rm);break;
		case 0x0c:	FPU_ESC4_Normal(inst.rm);break;
		case 0x0d:	FPU_ESC5_Normal(inst.rm);break;
		case 0x0e:	FPU_ESC6_Normal(inst.rm);break;
		case 0x0f:	FPU_ESC7_Normal(inst.rm);break;
		}
		goto nextopcode;
#else
		LOG(LOG_CPU,LOG_ERROR)("Unhandled FPU ESCAPE %d",inst.code.save);
		goto nextopcode;
#endif
	case O_BOUNDw:
		{
			Bit16s bound_min, bound_max;
			bound_min=(Bit16s)LoadMw(inst.rm_eaa);
			bound_max=(Bit16s)LoadMw(inst.rm_eaa+2);
			if ( (((Bit16s)inst_op1_w) < bound_min) || (((Bit16s)inst_op1_w) > bound_max) ) {
				EXCEPTION(5);
			}
		}
		break;
	case 0:
		break;
	default:
		LOG(LOG_CPU,LOG_ERROR)("OP:Unhandled code %d entry %lx",inst.code.op,(unsigned long)inst.entry);
		
}
