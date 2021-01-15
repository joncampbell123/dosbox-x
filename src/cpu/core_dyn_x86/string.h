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

enum STRING_OP {
	STR_OUTSB=0,STR_OUTSW,STR_OUTSD,
	STR_INSB=4,STR_INSW,STR_INSD,
	STR_MOVSB=8,STR_MOVSW,STR_MOVSD,
	STR_LODSB=12,STR_LODSW,STR_LODSD,
	STR_STOSB=16,STR_STOSW,STR_STOSD,
	STR_SCASB=20,STR_SCASW,STR_SCASD,
	STR_CMPSB=24,STR_CMPSW,STR_CMPSD
};

static void dyn_string(STRING_OP op) {
	DynReg * si_base=decode.segprefix ? decode.segprefix : DREG(DS);
	DynReg * di_base=DREG(ES);
	DynReg * tmp_reg;bool usesi;bool usedi;bool cmp=false;
	gen_protectflags();
	if (decode.rep) {
		gen_dop_word_imm(DOP_SUB,true,DREG(CYCLES),decode.cycles);
		gen_releasereg(DREG(CYCLES));
		decode.cycles=0;
	}
	/* Check what each string operation will be using */
	switch (op) {
	case STR_CMPSB:	case STR_CMPSW:	case STR_CMPSD:
		cmp=true;
	case STR_MOVSB:	case STR_MOVSW:	case STR_MOVSD:
		tmp_reg=DREG(TMPB);usesi=true;usedi=true;break;
	case STR_LODSB:	case STR_LODSW:	case STR_LODSD:
		tmp_reg=DREG(EAX);usesi=true;usedi=false;break;
	case STR_OUTSB:	case STR_OUTSW:	case STR_OUTSD:
		tmp_reg=DREG(TMPB);usesi=true;usedi=false;break;
	case STR_SCASB:	case STR_SCASW:	case STR_SCASD:
		cmp=true;
	case STR_STOSB:	case STR_STOSW:	case STR_STOSD:
		tmp_reg=DREG(EAX);usesi=false;usedi=true;break;
	case STR_INSB:	case STR_INSW:	case STR_INSD:
		tmp_reg=DREG(TMPB);usesi=false;usedi=true;break;
	default:
		IllegalOption("dyn_string op");
	}
	gen_load_host(&cpu.direction,DREG(TMPW),4);
	switch (op & 3) {
	case 0:break;
	case 1:gen_shift_word_imm(SHIFT_SHL,true,DREG(TMPW),1);break;
	case 2:gen_shift_word_imm(SHIFT_SHL,true,DREG(TMPW),2);break;
	default:
		IllegalOption("dyn_string shift");

	}
	if (usesi) {
		gen_preloadreg(DREG(ESI));
		DynRegs[G_ESI].flags|=DYNFLG_CHANGED;
		gen_preloadreg(si_base);
	}
	if (usedi) {
		gen_preloadreg(DREG(EDI));
		DynRegs[G_EDI].flags|=DYNFLG_CHANGED;
		gen_preloadreg(di_base);
	}
	if (decode.rep) {
		gen_preloadreg(DREG(ECX));
		DynRegs[G_ECX].flags|=DYNFLG_CHANGED;
	}
	DynState rep_state, cmp_state;
	dyn_savestate(&rep_state);
	uint8_t * rep_start=cache.pos;
	uint8_t * rep_ecx_jmp, * rep_cmp_jmp;
	/* Check if ECX!=zero */
	if (decode.rep) {
		gen_dop_word(DOP_TEST,decode.big_addr,DREG(ECX),DREG(ECX));
		rep_ecx_jmp=gen_create_branch_long(BR_Z);
	}
	if (usesi) {
		if (!decode.big_addr) {
			gen_extend_word(false,DREG(EA),DREG(ESI));
			gen_lea(DREG(EA),si_base,DREG(EA),0,0);
		} else {
			gen_lea(DREG(EA),si_base,DREG(ESI),0,0);
		}
		switch (op&3) {
		case 0:dyn_read_byte(DREG(EA),tmp_reg,false);break;
		case 1:dyn_read_word(DREG(EA),tmp_reg,false);break;
		case 2:dyn_read_word(DREG(EA),tmp_reg,true);break;
		}
		switch (op) {
		case STR_OUTSB:
			gen_call_function((void*)&IO_WriteB,"%Dw%Dl",DREG(EDX),tmp_reg);break;
		case STR_OUTSW:
			gen_call_function((void*)&IO_WriteW,"%Dw%Dw",DREG(EDX),tmp_reg);break;
		case STR_OUTSD:
			gen_call_function((void*)&IO_WriteD,"%Dw%Dd",DREG(EDX),tmp_reg);break;
		}
	}
	if (usedi) {
		if (!decode.big_addr) {
			gen_extend_word(false,DREG(EA),DREG(EDI));
			gen_lea(DREG(EA),di_base,DREG(EA),0,0);
		} else {
			gen_lea(DREG(EA),di_base,DREG(EDI),0,0);
		}
		if (cmp) {
			switch (op&3) {
			case 0:dyn_read_byte(DREG(EA),DREG(TMPD),false);break;
			case 1:dyn_read_word(DREG(EA),DREG(TMPD),false);break;
			case 2:dyn_read_word(DREG(EA),DREG(TMPD),true);break;
			}
			gen_needflags();
		}
		/* Maybe something special to be done to fill the value */
		switch (op) {
		case STR_INSB:
			gen_call_function((void*)&IO_ReadB,"%Dw%Rl",DREG(EDX),tmp_reg);
		case STR_MOVSB:
		case STR_STOSB:
			dyn_write_byte(DREG(EA),tmp_reg,false);
			break;
		case STR_CMPSB:
		case STR_SCASB:
			gen_dop_byte(DOP_CMP,tmp_reg,0,DREG(TMPD),0);
			break;
		case STR_INSW:
			gen_call_function((void*)&IO_ReadW,"%Dw%Rw",DREG(EDX),tmp_reg);
		case STR_MOVSW:
		case STR_STOSW:
			dyn_write_word(DREG(EA),tmp_reg,false);
			break;
		case STR_CMPSW:
		case STR_SCASW:
			gen_dop_word(DOP_CMP,false,tmp_reg,DREG(TMPD));
			break;
		case STR_INSD:
			gen_call_function((void*)&IO_ReadD,"%Dw%Rd",DREG(EDX),tmp_reg);
		case STR_MOVSD:
		case STR_STOSD:
			dyn_write_word(DREG(EA),tmp_reg,true);
			break;
		case STR_CMPSD:
		case STR_SCASD:
			gen_dop_word(DOP_CMP,true,tmp_reg,DREG(TMPD));
			break;
		default:
			IllegalOption("dyn_string op");
		}
		if (cmp) {
			gen_protectflags();
			
			if (decode.rep) {
				dyn_savestate(&cmp_state);
				rep_cmp_jmp=gen_create_branch_long(decode.rep == REP_NZ ? BR_Z : BR_NZ);
			}
		}
	}
	gen_releasereg(DREG(EA));gen_releasereg(DREG(TMPB));

	/* update registers */
	if (usesi) gen_dop_word(DOP_ADD,decode.big_addr,DREG(ESI),DREG(TMPW));
	if (usedi) gen_dop_word(DOP_ADD,decode.big_addr,DREG(EDI),DREG(TMPW));

	if (decode.rep) {
		gen_sop_word(SOP_DEC,decode.big_addr,DREG(ECX));
		gen_sop_word(SOP_DEC,true,DREG(CYCLES));
		gen_releasereg(DREG(CYCLES));
		dyn_savestate(&save_info[used_save_info].state);
		save_info[used_save_info].branch_pos=gen_create_branch_long(BR_LE);
		save_info[used_save_info].eip_change=decode.op_start-decode.code_start;
		save_info[used_save_info].type=normal;
		used_save_info++;

		/* Jump back to start of ECX check */
		dyn_synchstate(&rep_state);
		gen_create_jump(rep_start);

		if (cmp) {
			dyn_loadstate(&cmp_state);
			gen_fill_branch_long(rep_cmp_jmp);

			/* update registers */
			if (usesi) gen_dop_word(DOP_ADD,decode.big_addr,DREG(ESI),DREG(TMPW));
			if (usedi) gen_dop_word(DOP_ADD,decode.big_addr,DREG(EDI),DREG(TMPW));

			gen_sop_word(SOP_DEC,decode.big_addr,DREG(ECX));

			dyn_synchstate(&rep_state);
		}

		dyn_loadstate(&rep_state);
		gen_fill_branch_long(rep_ecx_jmp);
	}
	gen_releasereg(DREG(TMPW));
}
