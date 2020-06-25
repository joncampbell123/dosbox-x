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

/* Write the data from the opcode */
switch (inst.code.save) {
/* Byte */
	case S_C_Eb:
		inst_op1_b=inst.cond ? 1 : 0;
	case S_Eb:
		if (inst.rm<0xc0) SaveMb(inst.rm_eaa,inst_op1_b);
		else reg_8(inst.rm_eai)=inst_op1_b;
		break;	
	case S_Gb:
		reg_8(inst.rm_index)=inst_op1_b;
		break;	
	case S_EbGb:
		if (inst.rm<0xc0) SaveMb(inst.rm_eaa,inst_op1_b);
		else reg_8(inst.rm_eai)=inst_op1_b;
		reg_8(inst.rm_index)=inst_op2_b;
		break;	
/* Word */
	case S_Ew:
		if (inst.rm<0xc0) SaveMw(inst.rm_eaa,inst_op1_w);
		else reg_16(inst.rm_eai)=inst_op1_w;
		break;	
	case S_Gw:
		reg_16(inst.rm_index)=inst_op1_w;
		break;	
	case S_EwGw:
		if (inst.rm<0xc0) SaveMw(inst.rm_eaa,inst_op1_w);
		else reg_16(inst.rm_eai)=inst_op1_w;
		reg_16(inst.rm_index)=inst_op2_w;
		break;	
/* Dword */
	case S_Ed:
		if (inst.rm<0xc0) SaveMd(inst.rm_eaa,inst_op1_d);
		else reg_32(inst.rm_eai)=inst_op1_d;
		break;	
	case S_EdMw:		/* Special one 16 to memory, 32 zero extend to reg */
		if (inst.rm<0xc0) SaveMw(inst.rm_eaa,inst_op1_w);
		else reg_32(inst.rm_eai)=inst_op1_d;
		break;
	case S_Gd:
		reg_32(inst.rm_index)=inst_op1_d;
		break;	
	case S_EdGd:
		if (inst.rm<0xc0) SaveMd(inst.rm_eaa,inst_op1_d);
		else reg_32(inst.rm_eai)=inst_op1_d;
		reg_32(inst.rm_index)=inst_op2_d;
		break;	

	case S_REGb:
		reg_8(inst.code.extra)=inst_op1_b;
		break;
	case S_REGw:
		reg_16(inst.code.extra)=inst_op1_w;
		break;
	case S_REGd:
		reg_32(inst.code.extra)=inst_op1_d;
		break;	
	case S_SEGm:
		if (CPU_SetSegGeneral((SegNames)inst.rm_index,inst_op1_w)) RunException();
		break;
	case S_SEGGw:
		if (CPU_SetSegGeneral((SegNames)inst.code.extra,inst_op2_w)) RunException();
		reg_16(inst.rm_index)=inst_op1_w;
		break;
	case S_SEGGd:
		if (CPU_SetSegGeneral((SegNames)inst.code.extra,inst_op2_w)) RunException();
		reg_32(inst.rm_index)=inst_op1_d;
		break;
	case S_PUSHw:
		Push_16(inst_op1_w);
		break;
	case S_PUSHd:
		Push_32(inst_op1_d);
		break;

	case S_C_AIPw:
		if (!inst.cond) goto nextopcode;
	case S_AIPw:
		SaveIP();
		reg_eip+=inst_op1_d;
		reg_eip&=0xffff;
		continue;
	case S_C_AIPd:
		if (!inst.cond) goto nextopcode;
	case S_AIPd:
		SaveIP();
		reg_eip+=inst_op1_d;
		continue;
	case S_IPIw:
		reg_esp+=Fetchw();
	case S_IP:
		SaveIP();
		reg_eip=inst_op1_d;
		continue;
	case 0:
		break;
	default:
		LOG(LOG_CPU,LOG_ERROR)("SAVE:Unhandled code %d entry %lx",inst.code.save,(unsigned long)inst.entry);
}
