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



/*
	The functions in this file are called almost exclusively by	decoder.h,
	they translate an (or a set of) instruction(s) into hostspecific code
	and use the lower-level functions from decoder_basic.h and generating
	functions from risc_?.h
	Some complex instructions as well as most instructions that can modify
	the condition flags are translated by generating a call to a C-function
	(see operators.h). Parameter loading and result writeback is done
	according to the instruction.
*/

static void dyn_dop_ebgb(DualOps op) {
	dyn_get_modrm();
	if (decode.modrm.mod<3) {
		dyn_fill_ea(FC_ADDR);
		gen_protect_addr_reg();
		dyn_read_byte_canuseword(FC_ADDR,FC_OP1);
		MOV_REG_BYTE_TO_HOST_REG_LOW_CANUSEWORD(FC_OP2,decode.modrm.reg&3,((decode.modrm.reg>>2)&1));
		dyn_dop_byte_gencall(op);

		if ((op!=DOP_CMP) && (op!=DOP_TEST)) {
			gen_restore_addr_reg();
			dyn_write_byte(FC_ADDR,FC_RETOP);
		}
	} else {
		MOV_REG_BYTE_TO_HOST_REG_LOW_CANUSEWORD(FC_OP1,decode.modrm.rm&3,((decode.modrm.rm>>2)&1));
		MOV_REG_BYTE_TO_HOST_REG_LOW_CANUSEWORD(FC_OP2,decode.modrm.reg&3,((decode.modrm.reg>>2)&1));
		dyn_dop_byte_gencall(op);
		if ((op!=DOP_CMP) && (op!=DOP_TEST)) MOV_REG_BYTE_FROM_HOST_REG_LOW(FC_RETOP,decode.modrm.rm&3,((decode.modrm.rm>>2)&1));
	}
}

static void dyn_dop_ebgb_mov(void) {
	dyn_get_modrm();
	if (decode.modrm.mod<3) {
		dyn_fill_ea(FC_ADDR);
		MOV_REG_BYTE_TO_HOST_REG_LOW(FC_TMP_BA1,decode.modrm.reg&3,((decode.modrm.reg>>2)&1));
		dyn_write_byte(FC_ADDR,FC_TMP_BA1);
	} else {
		MOV_REG_BYTE_TO_HOST_REG_LOW(FC_TMP_BA1,decode.modrm.reg&3,((decode.modrm.reg>>2)&1));
		MOV_REG_BYTE_FROM_HOST_REG_LOW(FC_TMP_BA1,decode.modrm.rm&3,((decode.modrm.rm>>2)&1));
	}
}

static void dyn_dop_ebib_mov(void) {
	dyn_get_modrm();
	if (decode.modrm.mod<3) {
		dyn_fill_ea(FC_ADDR);
		gen_mov_byte_to_reg_low_imm(FC_TMP_BA1,decode_fetchb());
		dyn_write_byte(FC_ADDR,FC_TMP_BA1);
	} else {
		gen_mov_byte_to_reg_low_imm(FC_TMP_BA1,decode_fetchb());
		MOV_REG_BYTE_FROM_HOST_REG_LOW(FC_TMP_BA1,decode.modrm.rm&3,((decode.modrm.rm>>2)&1));
	}
}

static void dyn_dop_ebgb_xchg(void) {
	dyn_get_modrm();
	if (decode.modrm.mod<3) {
		dyn_fill_ea(FC_ADDR);
		gen_protect_addr_reg();
		dyn_read_byte(FC_ADDR,FC_TMP_BA1);
		MOV_REG_BYTE_TO_HOST_REG_LOW(FC_TMP_BA2,decode.modrm.reg&3,((decode.modrm.reg>>2)&1));

		gen_protect_reg(FC_TMP_BA1);
		gen_restore_addr_reg();
		dyn_write_byte(FC_ADDR,FC_TMP_BA2);
		gen_restore_reg(FC_TMP_BA1);
		MOV_REG_BYTE_FROM_HOST_REG_LOW(FC_TMP_BA1,decode.modrm.reg&3,((decode.modrm.reg>>2)&1));
	} else {
		MOV_REG_BYTE_TO_HOST_REG_LOW(FC_TMP_BA1,decode.modrm.rm&3,((decode.modrm.rm>>2)&1));
		MOV_REG_BYTE_TO_HOST_REG_LOW(FC_TMP_BA2,decode.modrm.reg&3,((decode.modrm.reg>>2)&1));
		MOV_REG_BYTE_FROM_HOST_REG_LOW(FC_TMP_BA1,decode.modrm.reg&3,((decode.modrm.reg>>2)&1));
		MOV_REG_BYTE_FROM_HOST_REG_LOW(FC_TMP_BA2,decode.modrm.rm&3,((decode.modrm.rm>>2)&1));
	}
}

static void dyn_dop_gbeb(DualOps op) {
	dyn_get_modrm();
	if (decode.modrm.mod<3) {
		dyn_fill_ea(FC_ADDR);
		dyn_read_byte_canuseword(FC_ADDR,FC_OP2);
		MOV_REG_BYTE_TO_HOST_REG_LOW_CANUSEWORD(FC_OP1,decode.modrm.reg&3,((decode.modrm.reg>>2)&1));
		dyn_dop_byte_gencall(op);
		if ((op!=DOP_CMP) && (op!=DOP_TEST)) MOV_REG_BYTE_FROM_HOST_REG_LOW(FC_RETOP,decode.modrm.reg&3,((decode.modrm.reg>>2)&1));
	} else {
		MOV_REG_BYTE_TO_HOST_REG_LOW_CANUSEWORD(FC_OP2,decode.modrm.rm&3,((decode.modrm.rm>>2)&1));
		MOV_REG_BYTE_TO_HOST_REG_LOW_CANUSEWORD(FC_OP1,decode.modrm.reg&3,((decode.modrm.reg>>2)&1));
		dyn_dop_byte_gencall(op);
		if ((op!=DOP_CMP) && (op!=DOP_TEST)) MOV_REG_BYTE_FROM_HOST_REG_LOW(FC_RETOP,decode.modrm.reg&3,((decode.modrm.reg>>2)&1));
	}
}

static void dyn_dop_gbeb_mov(void) {
	dyn_get_modrm();
	if (decode.modrm.mod<3) {
		dyn_fill_ea(FC_ADDR);
		dyn_read_byte(FC_ADDR,FC_TMP_BA1);
		MOV_REG_BYTE_FROM_HOST_REG_LOW(FC_TMP_BA1,decode.modrm.reg&3,((decode.modrm.reg>>2)&1));
	} else {
		MOV_REG_BYTE_TO_HOST_REG_LOW(FC_TMP_BA1,decode.modrm.rm&3,((decode.modrm.rm>>2)&1));
		MOV_REG_BYTE_FROM_HOST_REG_LOW(FC_TMP_BA1,decode.modrm.reg&3,((decode.modrm.reg>>2)&1));
	}
}

static void dyn_dop_evgv(DualOps op) {
	dyn_get_modrm();
	if (decode.modrm.mod<3) {
		dyn_fill_ea(FC_ADDR);
		dyn_read_word(FC_ADDR,FC_OP1,decode.big_op);
		gen_protect_addr_reg();
		MOV_REG_WORD_TO_HOST_REG(FC_OP2,decode.modrm.reg,decode.big_op);
		dyn_dop_word_gencall(op,decode.big_op);
		if ((op!=DOP_CMP) && (op!=DOP_TEST)) {
			gen_restore_addr_reg();
			dyn_write_word(FC_ADDR,FC_RETOP,decode.big_op);
		}
	} else {
		MOV_REG_WORD_TO_HOST_REG(FC_OP1,decode.modrm.rm,decode.big_op);
		MOV_REG_WORD_TO_HOST_REG(FC_OP2,decode.modrm.reg,decode.big_op);
		dyn_dop_word_gencall(op,decode.big_op);
		if ((op!=DOP_CMP) && (op!=DOP_TEST)) MOV_REG_WORD_FROM_HOST_REG(FC_RETOP,decode.modrm.rm,decode.big_op);
	}
}

static void dyn_dop_evgv_mov(void) {
	dyn_get_modrm();
	if (decode.modrm.mod<3) {
		dyn_fill_ea(FC_ADDR);
		MOV_REG_WORD_TO_HOST_REG(FC_OP1,decode.modrm.reg,decode.big_op);
		dyn_write_word(FC_ADDR,FC_OP1,decode.big_op);
	} else {
		MOV_REG_WORD_TO_HOST_REG(FC_OP1,decode.modrm.reg,decode.big_op);
		MOV_REG_WORD_FROM_HOST_REG(FC_OP1,decode.modrm.rm,decode.big_op);
	}
}

static void dyn_dop_eviv_mov(void) {
	dyn_get_modrm();
	if (decode.modrm.mod<3) {
		dyn_fill_ea(FC_ADDR);
		if (decode.big_op) gen_mov_dword_to_reg_imm(FC_OP1,decode_fetchd());
		else gen_mov_word_to_reg_imm(FC_OP1,decode_fetchw());
		dyn_write_word(FC_ADDR,FC_OP1,decode.big_op);
	} else {
		if (decode.big_op) gen_mov_dword_to_reg_imm(FC_OP1,decode_fetchd());
		else gen_mov_word_to_reg_imm(FC_OP1,decode_fetchw());
		MOV_REG_WORD_FROM_HOST_REG(FC_OP1,decode.modrm.rm,decode.big_op);
	}
}

static void dyn_dop_evgv_xchg(void) {
	dyn_get_modrm();
	if (decode.modrm.mod<3) {
		dyn_fill_ea(FC_ADDR);
		gen_protect_addr_reg();
		dyn_read_word(FC_ADDR,FC_OP1,decode.big_op);
		MOV_REG_WORD_TO_HOST_REG(FC_OP2,decode.modrm.reg,decode.big_op);

		gen_protect_reg(FC_OP1);
		gen_restore_addr_reg();
		dyn_write_word(FC_ADDR,FC_OP2,decode.big_op);
		gen_restore_reg(FC_OP1);
		MOV_REG_WORD_FROM_HOST_REG(FC_OP1,decode.modrm.reg,decode.big_op);
	} else {
		MOV_REG_WORD_TO_HOST_REG(FC_OP1,decode.modrm.rm,decode.big_op);
		MOV_REG_WORD_TO_HOST_REG(FC_OP2,decode.modrm.reg,decode.big_op);
		MOV_REG_WORD_FROM_HOST_REG(FC_OP1,decode.modrm.reg,decode.big_op);
		MOV_REG_WORD_FROM_HOST_REG(FC_OP2,decode.modrm.rm,decode.big_op);
	}
}

static void dyn_xchg_ax(uint8_t reg) {
	MOV_REG_WORD_TO_HOST_REG(FC_OP1,DRC_REG_EAX,decode.big_op);
	MOV_REG_WORD_TO_HOST_REG(FC_OP2,reg,decode.big_op);
	MOV_REG_WORD_FROM_HOST_REG(FC_OP1,reg,decode.big_op);
	MOV_REG_WORD_FROM_HOST_REG(FC_OP2,DRC_REG_EAX,decode.big_op);
}

static void dyn_dop_gvev(DualOps op) {
	dyn_get_modrm();
	if (decode.modrm.mod<3) {
		dyn_fill_ea(FC_ADDR);
		gen_protect_addr_reg();
		dyn_read_word(FC_ADDR,FC_OP2,decode.big_op);
		MOV_REG_WORD_TO_HOST_REG(FC_OP1,decode.modrm.reg,decode.big_op);
		dyn_dop_word_gencall(op,decode.big_op);
		if ((op!=DOP_CMP) && (op!=DOP_TEST)) {
			gen_restore_addr_reg();
			MOV_REG_WORD_FROM_HOST_REG(FC_RETOP,decode.modrm.reg,decode.big_op);
		}
	} else {
		MOV_REG_WORD_TO_HOST_REG(FC_OP2,decode.modrm.rm,decode.big_op);
		MOV_REG_WORD_TO_HOST_REG(FC_OP1,decode.modrm.reg,decode.big_op);
		dyn_dop_word_gencall(op,decode.big_op);
		if ((op!=DOP_CMP) && (op!=DOP_TEST)) MOV_REG_WORD_FROM_HOST_REG(FC_RETOP,decode.modrm.reg,decode.big_op);
	}
}

static void dyn_dop_gvev_mov(void) {
	dyn_get_modrm();
	if (decode.modrm.mod<3) {
		dyn_fill_ea(FC_ADDR);
		dyn_read_word(FC_ADDR,FC_OP1,decode.big_op);
		MOV_REG_WORD_FROM_HOST_REG(FC_OP1,decode.modrm.reg,decode.big_op);
	} else {
		MOV_REG_WORD_TO_HOST_REG(FC_OP1,decode.modrm.rm,decode.big_op);
		MOV_REG_WORD_FROM_HOST_REG(FC_OP1,decode.modrm.reg,decode.big_op);
	}
}

static void dyn_dop_byte_imm(DualOps op,uint8_t reg,uint8_t idx) {
	MOV_REG_BYTE_TO_HOST_REG_LOW_CANUSEWORD(FC_OP1,reg,idx);
	gen_mov_byte_to_reg_low_imm_canuseword(FC_OP2,decode_fetchb());
	dyn_dop_byte_gencall(op);
	if ((op!=DOP_CMP) && (op!=DOP_TEST)) MOV_REG_BYTE_FROM_HOST_REG_LOW(FC_RETOP,reg,idx);
}

static void dyn_dop_byte_imm_mem(DualOps op,uint8_t reg,uint8_t idx) {
	MOV_REG_BYTE_TO_HOST_REG_LOW_CANUSEWORD(FC_OP1,reg,idx);
	Bitu val;
	if (decode_fetchb_imm(val)) {
		gen_mov_byte_to_reg_low_canuseword(FC_OP2,(void*)val);
	} else {
		gen_mov_byte_to_reg_low_imm_canuseword(FC_OP2,(uint8_t)val);
	}
	dyn_dop_byte_gencall(op);
	if ((op!=DOP_CMP) && (op!=DOP_TEST)) MOV_REG_BYTE_FROM_HOST_REG_LOW(FC_RETOP,reg,idx);
}

static void dyn_prep_word_imm(uint8_t reg) {
	(void)reg;

	Bitu val;
	if (decode.big_op) {
		if (decode_fetchd_imm(val)) {
			gen_mov_word_to_reg(FC_OP2,(void*)val,true);
			return;
		}
	} else {
		if (decode_fetchw_imm(val)) {
			gen_mov_word_to_reg(FC_OP2,(void*)val,false);
			return;
		}
	}
	if (decode.big_op) gen_mov_dword_to_reg_imm(FC_OP2,(uint32_t)val);
	else gen_mov_word_to_reg_imm(FC_OP2,(uint16_t)val);
}

static void dyn_dop_word_imm(DualOps op,uint8_t reg) {
	MOV_REG_WORD_TO_HOST_REG(FC_OP1,reg,decode.big_op);
	dyn_prep_word_imm(reg);
	dyn_dop_word_gencall(op,decode.big_op);
	if ((op!=DOP_CMP) && (op!=DOP_TEST)) MOV_REG_WORD_FROM_HOST_REG(FC_RETOP,reg,decode.big_op);
}

static void dyn_dop_word_imm_old(DualOps op,uint8_t reg,Bitu imm) {
	MOV_REG_WORD_TO_HOST_REG(FC_OP1,reg,decode.big_op);
	if (decode.big_op) gen_mov_dword_to_reg_imm(FC_OP2,(uint32_t)imm);
	else gen_mov_word_to_reg_imm(FC_OP2,(uint16_t)imm);
	dyn_dop_word_gencall(op,decode.big_op);
	if ((op!=DOP_CMP) && (op!=DOP_TEST)) MOV_REG_WORD_FROM_HOST_REG(FC_RETOP,reg,decode.big_op);
}

static void dyn_mov_byte_imm(uint8_t reg,uint8_t idx,uint8_t imm) {
	gen_mov_byte_to_reg_low_imm(FC_TMP_BA1,imm);
	MOV_REG_BYTE_FROM_HOST_REG_LOW(FC_TMP_BA1,reg,idx);
}

static void dyn_mov_word_imm(uint8_t reg) {
	Bitu val;
	if (decode.big_op) {
		if (decode_fetchd_imm(val)) {
			gen_mov_word_to_reg(FC_OP1,(void*)val,true);
			MOV_REG_WORD32_FROM_HOST_REG(FC_OP1,reg);
			return;
		}
	} else {
		if (decode_fetchw_imm(val)) {
			gen_mov_word_to_reg(FC_OP1,(void*)val,false);
			MOV_REG_WORD16_FROM_HOST_REG(FC_OP1,reg);
			return;
		}
	}
	if (decode.big_op) gen_mov_dword_to_reg_imm(FC_OP1,(uint32_t)val);
	else gen_mov_word_to_reg_imm(FC_OP1,(uint16_t)val);
	MOV_REG_WORD_FROM_HOST_REG(FC_OP1,reg,decode.big_op);
}


static void dyn_sop_word(SingleOps op,uint8_t reg) {
	MOV_REG_WORD_TO_HOST_REG(FC_OP1,reg,decode.big_op);
	dyn_sop_word_gencall(op,decode.big_op);
	MOV_REG_WORD_FROM_HOST_REG(FC_RETOP,reg,decode.big_op);
}


static void dyn_mov_byte_al_direct(uint32_t imm) {
	MOV_SEG_PHYS_TO_HOST_REG(FC_ADDR,(decode.seg_prefix_used ? decode.seg_prefix : DRC_SEG_DS));
	gen_add_imm(FC_ADDR,imm);
	dyn_read_byte(FC_ADDR,FC_TMP_BA1);
	MOV_REG_BYTE_FROM_HOST_REG_LOW(FC_TMP_BA1,DRC_REG_EAX,0);
}

static void dyn_mov_byte_ax_direct(uint32_t imm) {
	MOV_SEG_PHYS_TO_HOST_REG(FC_ADDR,(decode.seg_prefix_used ? decode.seg_prefix : DRC_SEG_DS));
	gen_add_imm(FC_ADDR,imm);
	dyn_read_word(FC_ADDR,FC_OP1,decode.big_op);
	MOV_REG_WORD_FROM_HOST_REG(FC_OP1,DRC_REG_EAX,decode.big_op);
}

static void dyn_mov_byte_direct_al() {
	MOV_SEG_PHYS_TO_HOST_REG(FC_ADDR,(decode.seg_prefix_used ? decode.seg_prefix : DRC_SEG_DS));
	if (decode.big_addr) {
		Bitu val;
		if (decode_fetchd_imm(val)) {
			gen_add(FC_ADDR,(void*)val);
		} else {
			gen_add_imm(FC_ADDR,(uint32_t)val);
		}
	} else {
		gen_add_imm(FC_ADDR,decode_fetchw());
	}
	MOV_REG_BYTE_TO_HOST_REG_LOW(FC_TMP_BA1,DRC_REG_EAX,0);
	dyn_write_byte(FC_ADDR,FC_TMP_BA1);
}

static void dyn_mov_byte_direct_ax(uint32_t imm) {
	MOV_SEG_PHYS_TO_HOST_REG(FC_ADDR,(decode.seg_prefix_used ? decode.seg_prefix : DRC_SEG_DS));
	gen_add_imm(FC_ADDR,imm);
	MOV_REG_WORD_TO_HOST_REG(FC_OP1,DRC_REG_EAX,decode.big_op);
	dyn_write_word(FC_ADDR,FC_OP1,decode.big_op);
}


static void dyn_movx_ev_gb(bool sign) {
	dyn_get_modrm();
	if (decode.modrm.mod<3) {
		dyn_fill_ea(FC_ADDR);
		dyn_read_byte(FC_ADDR,FC_TMP_BA1);
		gen_extend_byte(sign,FC_TMP_BA1);
		MOV_REG_WORD_FROM_HOST_REG(FC_TMP_BA1,decode.modrm.reg,decode.big_op);
	} else {
		MOV_REG_BYTE_TO_HOST_REG_LOW(FC_TMP_BA1,decode.modrm.rm&3,((decode.modrm.rm>>2)&1));
		gen_extend_byte(sign,FC_TMP_BA1);
		MOV_REG_WORD_FROM_HOST_REG(FC_TMP_BA1,decode.modrm.reg,decode.big_op);
	}
}

static void dyn_movx_ev_gw(bool sign) {
	dyn_get_modrm();
	if (decode.modrm.mod<3) {
		dyn_fill_ea(FC_ADDR);
		dyn_read_word(FC_ADDR,FC_OP1,false);
		gen_extend_word(sign,FC_OP1);
		MOV_REG_WORD_FROM_HOST_REG(FC_OP1,decode.modrm.reg,decode.big_op);
	} else {
		MOV_REG_WORD16_TO_HOST_REG(FC_OP1,decode.modrm.rm);
		gen_extend_word(sign,FC_OP1);
		MOV_REG_WORD_FROM_HOST_REG(FC_OP1,decode.modrm.reg,decode.big_op);
	}
}


static void dyn_mov_ev_seg(void) {
	dyn_get_modrm();
	MOV_SEG_VAL_TO_HOST_REG(FC_OP1,decode.modrm.reg);
	if (decode.modrm.mod<3) {
		dyn_fill_ea(FC_ADDR);
		dyn_write_word(FC_ADDR,FC_OP1,false);
	} else {
		if (decode.big_op) gen_extend_word(false,FC_OP1);
		MOV_REG_WORD_FROM_HOST_REG(FC_OP1,decode.modrm.rm,decode.big_op);
	}
}


static void dyn_lea(void) {
	dyn_get_modrm();
	dyn_fill_ea(FC_ADDR,false);
	MOV_REG_WORD_FROM_HOST_REG(FC_ADDR,decode.modrm.reg,decode.big_op);
}


static void dyn_push_seg(uint8_t seg) {
	MOV_SEG_VAL_TO_HOST_REG(FC_OP1,seg);
	if (decode.big_op) {
		gen_extend_word(false,FC_OP1);
		gen_call_function_raw(dynrec_push_dword);
	} else {
		gen_call_function_raw(dynrec_push_word);
	}
}

static void dyn_pop_seg(uint8_t seg) {
	gen_call_function_II(CPU_PopSeg,seg,decode.big_op);
	dyn_check_exception(FC_RETOP);
}

static void dyn_push_reg(uint8_t reg) {
	MOV_REG_WORD_TO_HOST_REG(FC_OP1,reg,decode.big_op);
	if (decode.big_op) gen_call_function_raw(dynrec_push_dword);
	else gen_call_function_raw(dynrec_push_word);
}

static void dyn_pop_reg(uint8_t reg) {
	if (decode.big_op) gen_call_function_raw(dynrec_pop_dword);
	else gen_call_function_raw(dynrec_pop_word);
	MOV_REG_WORD_FROM_HOST_REG(FC_RETOP,reg,decode.big_op);
}

static void dyn_push_byte_imm(int8_t imm) {
	gen_mov_dword_to_reg_imm(FC_OP1,(uint32_t)imm);
	if (decode.big_op) gen_call_function_raw(dynrec_push_dword);
	else gen_call_function_raw(dynrec_push_word);
}

static void dyn_push_word_imm(uint32_t imm) {
	if (decode.big_op) {
		gen_mov_dword_to_reg_imm(FC_OP1,imm);
		gen_call_function_raw(dynrec_push_dword);
	} else {
		gen_mov_word_to_reg_imm(FC_OP1,(uint16_t)imm);
		gen_call_function_raw(dynrec_push_word);
	}
}

static void dyn_pop_ev(void) {
	dyn_get_modrm();
	if (decode.modrm.mod<3) {
		// save original ESP
		MOV_REG_WORD32_TO_HOST_REG(FC_OP2,DRC_REG_ESP);
		gen_protect_reg(FC_OP2);
		if (decode.big_op) gen_call_function_raw(dynrec_pop_dword);
		else gen_call_function_raw(dynrec_pop_word);
		dyn_fill_ea(FC_ADDR);
		gen_mov_regs(FC_OP2,FC_RETOP);
		gen_mov_regs(FC_OP1,FC_ADDR);
		if (decode.big_op) gen_call_function_raw(mem_writed_checked_drc);
		else gen_call_function_raw(mem_writew_checked_drc);
		gen_extend_byte(false,FC_RETOP); // bool -> dword
		DRC_PTR_SIZE_IM no_fault = gen_create_branch_on_zero(FC_RETOP, true);
		// restore original ESP
		gen_restore_reg(FC_OP2);
		MOV_REG_WORD32_FROM_HOST_REG(FC_OP2,DRC_REG_ESP);
		dyn_check_exception(FC_RETOP);
		gen_fill_branch(no_fault);
	} else {
		if (decode.big_op) gen_call_function_raw(dynrec_pop_dword);
		else gen_call_function_raw(dynrec_pop_word);
		MOV_REG_WORD_FROM_HOST_REG(FC_RETOP,decode.modrm.rm,decode.big_op);
	}
}


static void dyn_segprefix(uint8_t seg) {
//	if (GCC_UNLIKELY(decode.seg_prefix_used)) IllegalOptionDynrec("dyn_segprefix");
	decode.seg_prefix=seg;
	decode.seg_prefix_used=true;
}

 static void dyn_mov_seg_ev(void) {
	dyn_get_modrm();
	if (GCC_UNLIKELY(decode.modrm.reg==DRC_SEG_CS)) IllegalOptionDynrec("dyn_mov_seg_ev");
	if (decode.modrm.mod<3) {
		dyn_fill_ea(FC_ADDR);
		dyn_read_word(FC_ADDR,FC_RETOP,false);
	} else {
		MOV_REG_WORD16_TO_HOST_REG(FC_RETOP,decode.modrm.rm);
	}
	gen_call_function_IR(CPU_SetSegGeneral,decode.modrm.reg,FC_RETOP);
	dyn_check_exception(FC_RETOP);
}

static void dyn_load_seg_off_ea(uint8_t seg) {
	if (decode.modrm.mod<3) {
		dyn_fill_ea(FC_ADDR);
		gen_protect_addr_reg();
		dyn_read_word(FC_ADDR,FC_OP1,decode.big_op);
		gen_protect_reg(FC_OP1);

		gen_restore_addr_reg();
		gen_add_imm(FC_ADDR,decode.big_op ? 4:2);
		dyn_read_word(FC_ADDR,FC_RETOP,false);

		gen_call_function_IR(CPU_SetSegGeneral,seg,FC_RETOP);
		dyn_check_exception(FC_RETOP);

		gen_restore_reg(FC_OP1);
		MOV_REG_WORD_FROM_HOST_REG(FC_OP1,decode.modrm.reg,decode.big_op);
	} else {
		IllegalOptionDynrec("dyn_load_seg_off_ea");
	}
}



static void dyn_imul_gvev(Bitu immsize) {
	dyn_get_modrm();
	if (decode.modrm.mod<3) {
		dyn_fill_ea(FC_ADDR);
		dyn_read_word(FC_ADDR,FC_OP1,decode.big_op);
	} else {
		MOV_REG_WORD_TO_HOST_REG(FC_OP1,decode.modrm.rm,decode.big_op);
	}

	switch (immsize) {
		case 0:
			MOV_REG_WORD_TO_HOST_REG(FC_OP2,decode.modrm.reg,decode.big_op);
			break;
		case 1:
			if (decode.big_op) gen_mov_dword_to_reg_imm(FC_OP2,(int8_t)decode_fetchb());
			else  gen_mov_word_to_reg_imm(FC_OP2,(int8_t)decode_fetchb());
			break;
		case 2:
			if (decode.big_op) gen_mov_dword_to_reg_imm(FC_OP2,(int16_t)decode_fetchw());
			else gen_mov_word_to_reg_imm(FC_OP2,(int16_t)decode_fetchw());
			break;
		case 4:
			if (decode.big_op) gen_mov_dword_to_reg_imm(FC_OP2,(int32_t)decode_fetchd());
			else gen_mov_word_to_reg_imm(FC_OP2,(uint16_t)((int32_t)decode_fetchd()));
			break;
	}

	if (decode.big_op) gen_call_function_raw(dynrec_dimul_dword);
	else gen_call_function_raw(dynrec_dimul_word);

	MOV_REG_WORD_FROM_HOST_REG(FC_RETOP,decode.modrm.reg,decode.big_op);
}

static void dyn_dshift_ev_gv(bool left,bool immediate) {
	dyn_get_modrm();
	if (decode.modrm.mod<3) {
		dyn_fill_ea(FC_ADDR);
		gen_protect_addr_reg();
		dyn_read_word(FC_ADDR,FC_OP1,decode.big_op);
	} else {
		MOV_REG_WORD_TO_HOST_REG(FC_OP1,decode.modrm.rm,decode.big_op);
	}
	MOV_REG_WORD_TO_HOST_REG(FC_OP2,decode.modrm.reg,decode.big_op);
	if (immediate) gen_mov_byte_to_reg_low_imm(FC_OP3,decode_fetchb());
	else MOV_REG_BYTE_TO_HOST_REG_LOW(FC_OP3,DRC_REG_ECX,0);
	if (decode.big_op) dyn_dpshift_dword_gencall(left);
	else dyn_dpshift_word_gencall(left);

	if (decode.modrm.mod<3) {
		gen_restore_addr_reg();
		dyn_write_word(FC_ADDR,FC_RETOP,decode.big_op);
	} else {
		MOV_REG_WORD_FROM_HOST_REG(FC_RETOP,decode.modrm.rm,decode.big_op);
	}
}


static void dyn_grp1_eb_ib(void) {
	dyn_get_modrm();
	DualOps op=grp1_table[decode.modrm.reg];
	if (decode.modrm.mod<3) {
		dyn_fill_ea(FC_ADDR);
		gen_protect_addr_reg();
		dyn_read_byte_canuseword(FC_ADDR,FC_OP1);
		gen_mov_byte_to_reg_low_imm_canuseword(FC_OP2,decode_fetchb());
		dyn_dop_byte_gencall(op);

		if ((op!=DOP_CMP) && (op!=DOP_TEST)) {
			gen_restore_addr_reg();
			dyn_write_byte(FC_ADDR,FC_RETOP);
		}
	} else {
		dyn_dop_byte_imm_mem(op,decode.modrm.rm&3,(decode.modrm.rm>>2)&1);
	}
}

static void dyn_grp1_ev_iv(bool withbyte) {
	dyn_get_modrm();
	DualOps op=grp1_table[decode.modrm.reg];
	if (decode.modrm.mod<3) {
		dyn_fill_ea(FC_ADDR);
		gen_protect_addr_reg();
		dyn_read_word(FC_ADDR,FC_OP1,decode.big_op);

		if (!withbyte) {
			dyn_prep_word_imm(FC_OP2);
		} else {
			Bits imm=(int8_t)decode_fetchb(); 
			if (decode.big_op) gen_mov_dword_to_reg_imm(FC_OP2,(uint32_t)imm);
			else gen_mov_word_to_reg_imm(FC_OP2,(uint16_t)imm);
		}

		dyn_dop_word_gencall(op,decode.big_op);

		if ((op!=DOP_CMP) && (op!=DOP_TEST)) {
			gen_restore_addr_reg();
			dyn_write_word(FC_ADDR,FC_RETOP,decode.big_op);
		}
	} else {
		if (!withbyte) {
			dyn_dop_word_imm(op,decode.modrm.rm);
		} else {
			Bits imm=withbyte ? (int8_t)decode_fetchb() : (decode.big_op ? decode_fetchd(): decode_fetchw()); 
			dyn_dop_word_imm_old(op,decode.modrm.rm,imm);
		}
	}
}


static void dyn_grp2_eb(grp2_types type) {
	dyn_get_modrm();
	if (decode.modrm.mod<3) {
		dyn_fill_ea(FC_ADDR);
		gen_protect_addr_reg();
		dyn_read_byte_canuseword(FC_ADDR,FC_OP1);
	} else {
		MOV_REG_BYTE_TO_HOST_REG_LOW_CANUSEWORD(FC_OP1,decode.modrm.rm&3,((decode.modrm.rm>>2)&1));
	}
	switch (type) {
	case grp2_1:
		gen_mov_byte_to_reg_low_imm_canuseword(FC_OP2,1);
		dyn_shift_byte_gencall((ShiftOps)decode.modrm.reg);
		break;
	case grp2_imm: {
		uint8_t imm=decode_fetchb();
		if (imm) {
			gen_mov_byte_to_reg_low_imm_canuseword(FC_OP2,imm&0x1f);
			dyn_shift_byte_gencall((ShiftOps)decode.modrm.reg);
		} else return;
		}
		break;
	case grp2_cl:
		MOV_REG_BYTE_TO_HOST_REG_LOW_CANUSEWORD(FC_OP2,DRC_REG_ECX,0);
		gen_and_imm(FC_OP2,0x1f);
		dyn_shift_byte_gencall((ShiftOps)decode.modrm.reg);
		break;
	}
	if (decode.modrm.mod<3) {
		gen_restore_addr_reg();
		dyn_write_byte(FC_ADDR,FC_RETOP);
	} else {
		MOV_REG_BYTE_FROM_HOST_REG_LOW(FC_RETOP,decode.modrm.rm&3,((decode.modrm.rm>>2)&1));
	}
}

static void dyn_grp2_ev(grp2_types type) {
	dyn_get_modrm();
	if (decode.modrm.mod<3) {
		dyn_fill_ea(FC_ADDR);
		gen_protect_addr_reg();
		dyn_read_word(FC_ADDR,FC_OP1,decode.big_op);
	} else {
		MOV_REG_WORD_TO_HOST_REG(FC_OP1,decode.modrm.rm,decode.big_op);
	}
	switch (type) {
	case grp2_1:
		gen_mov_byte_to_reg_low_imm_canuseword(FC_OP2,1);
		dyn_shift_word_gencall((ShiftOps)decode.modrm.reg,decode.big_op);
		break;
	case grp2_imm: {
		Bitu val;
		if (decode_fetchb_imm(val)) {
			gen_mov_byte_to_reg_low_canuseword(FC_OP2,(void*)val);
			gen_and_imm(FC_OP2,0x1f);
			dyn_shift_word_gencall((ShiftOps)decode.modrm.reg,decode.big_op);
			break;
		}
		uint8_t imm=(uint8_t)val;
		if (imm) {
			gen_mov_byte_to_reg_low_imm_canuseword(FC_OP2,imm&0x1f);
			dyn_shift_word_gencall((ShiftOps)decode.modrm.reg,decode.big_op);
		} else return;
		}
		break;
	case grp2_cl:
		MOV_REG_BYTE_TO_HOST_REG_LOW_CANUSEWORD(FC_OP2,DRC_REG_ECX,0);
		gen_and_imm(FC_OP2,0x1f);
		dyn_shift_word_gencall((ShiftOps)decode.modrm.reg,decode.big_op);
		break;
	}
	if (decode.modrm.mod<3) {
		gen_restore_addr_reg();
		dyn_write_word(FC_ADDR,FC_RETOP,decode.big_op);
	} else {
		MOV_REG_WORD_FROM_HOST_REG(FC_RETOP,decode.modrm.rm,decode.big_op);
	}
}


static void dyn_grp3_eb(void) {
	dyn_get_modrm();
	if (decode.modrm.mod<3) {
		dyn_fill_ea(FC_ADDR);
		if ((decode.modrm.reg==2) || (decode.modrm.reg==3)) gen_protect_addr_reg();
		dyn_read_byte_canuseword(FC_ADDR,FC_OP1);
	} else {
		MOV_REG_BYTE_TO_HOST_REG_LOW_CANUSEWORD(FC_OP1,decode.modrm.rm&3,((decode.modrm.rm>>2)&1));
	}
	switch (decode.modrm.reg) {
	case 0x0:	// test eb,ib
		gen_mov_byte_to_reg_low_imm_canuseword(FC_OP2,decode_fetchb());
		dyn_dop_byte_gencall(DOP_TEST);
		return;
	case 0x2:	// NOT Eb
		dyn_sop_byte_gencall(SOP_NOT);
		break;
	case 0x3:	// NEG Eb
		dyn_sop_byte_gencall(SOP_NEG);
		break;
	case 0x4:	// mul Eb
		gen_call_function_raw(dynrec_mul_byte);
		return;
	case 0x5:	// imul Eb
		gen_call_function_raw(dynrec_imul_byte);
		return;
	case 0x6:	// div Eb
		gen_call_function_raw(dynrec_div_byte);
		dyn_check_exception(FC_RETOP);
		return;
	case 0x7:	// idiv Eb
		gen_call_function_raw(dynrec_idiv_byte);
		dyn_check_exception(FC_RETOP);
		return;
	}
	// Save the result if memory op
	if (decode.modrm.mod<3) {
		gen_restore_addr_reg();
		dyn_write_byte(FC_ADDR,FC_RETOP);
	} else {
		MOV_REG_BYTE_FROM_HOST_REG_LOW(FC_RETOP,decode.modrm.rm&3,((decode.modrm.rm>>2)&1));
	}
}

static void dyn_grp3_ev(void) {
	dyn_get_modrm();
	if (decode.modrm.mod<3) {
		dyn_fill_ea(FC_ADDR);
		if ((decode.modrm.reg==2) || (decode.modrm.reg==3)) gen_protect_addr_reg();
		dyn_read_word(FC_ADDR,FC_OP1,decode.big_op);
	} else {
		MOV_REG_WORD_TO_HOST_REG(FC_OP1,decode.modrm.rm,decode.big_op);
	}
	switch (decode.modrm.reg) {
	case 0x0:	// test ev,iv
		if (decode.big_op) gen_mov_dword_to_reg_imm(FC_OP2,decode_fetchd());
		else gen_mov_word_to_reg_imm(FC_OP2,decode_fetchw());
		dyn_dop_word_gencall(DOP_TEST,decode.big_op);
		return;
	case 0x2:	// NOT Ev
		dyn_sop_word_gencall(SOP_NOT,decode.big_op);
		break;
	case 0x3:	// NEG Eb
		dyn_sop_word_gencall(SOP_NEG,decode.big_op);
		break;
	case 0x4:	// mul Eb
		if (decode.big_op) gen_call_function_raw(dynrec_mul_dword);
		else gen_call_function_raw(dynrec_mul_word);
		return;
	case 0x5:	// imul Eb
		if (decode.big_op) gen_call_function_raw(dynrec_imul_dword);
		else gen_call_function_raw(dynrec_imul_word);
		return;
	case 0x6:	// div Eb
		if (decode.big_op) gen_call_function_raw(dynrec_div_dword);
		else gen_call_function_raw(dynrec_div_word);
		dyn_check_exception(FC_RETOP);
		return;
	case 0x7:	// idiv Eb
		if (decode.big_op) gen_call_function_raw(dynrec_idiv_dword);
		else gen_call_function_raw(dynrec_idiv_word);
		dyn_check_exception(FC_RETOP);
		return;
	}
	// Save the result if memory op
	if (decode.modrm.mod<3) {
		gen_restore_addr_reg();
		dyn_write_word(FC_ADDR,FC_RETOP,decode.big_op);
	} else {
		MOV_REG_WORD_FROM_HOST_REG(FC_RETOP,decode.modrm.rm,decode.big_op);
	}
}


static bool dyn_grp4_eb(void) {
	dyn_get_modrm();
	switch (decode.modrm.reg) {
	case 0x0://INC Eb
	case 0x1://DEC Eb
		if (decode.modrm.mod<3) {
			dyn_fill_ea(FC_ADDR);
			gen_protect_addr_reg();
			dyn_read_byte_canuseword(FC_ADDR,FC_OP1);
			dyn_sop_byte_gencall(decode.modrm.reg==0 ? SOP_INC : SOP_DEC);
			gen_restore_addr_reg();
			dyn_write_byte(FC_ADDR,FC_RETOP);
		} else {
			MOV_REG_BYTE_TO_HOST_REG_LOW_CANUSEWORD(FC_OP1,decode.modrm.rm&3,((decode.modrm.rm>>2)&1));
			dyn_sop_byte_gencall(decode.modrm.reg==0 ? SOP_INC : SOP_DEC);
			MOV_REG_BYTE_FROM_HOST_REG_LOW(FC_RETOP,decode.modrm.rm&3,((decode.modrm.rm>>2)&1));
		}
		break;
	case 0x7:		//CALBACK Iw
		gen_mov_direct_dword(&core_dynrec.callback,decode_fetchw());
		dyn_set_eip_end();
		dyn_reduce_cycles();
		dyn_return(BR_CallBack);
		dyn_closeblock();
		return true;
	default:
		IllegalOptionDynrec("dyn_grp4_eb");
		break;
	}
	return false;
}

static Bitu dyn_grp4_ev(void) {
	dyn_get_modrm();
	if (decode.modrm.mod<3) {
		dyn_fill_ea(FC_ADDR);
		if ((decode.modrm.reg<2) || (decode.modrm.reg==3) || (decode.modrm.reg==5))  gen_protect_addr_reg();
		dyn_read_word(FC_ADDR,FC_OP1,decode.big_op);
	} else {
		MOV_REG_WORD_TO_HOST_REG(FC_OP1,decode.modrm.rm,decode.big_op);
	}
	switch (decode.modrm.reg) {
	case 0x0://INC Ev
	case 0x1://DEC Ev
		dyn_sop_word_gencall(decode.modrm.reg==0 ? SOP_INC : SOP_DEC,decode.big_op);
		if (decode.modrm.mod<3) {
			gen_restore_addr_reg();
			dyn_write_word(FC_ADDR,FC_RETOP,decode.big_op);
		} else {
			MOV_REG_WORD_FROM_HOST_REG(FC_RETOP,decode.modrm.rm,decode.big_op);
		}
		break;
	case 0x2:	// CALL Ev
		gen_mov_regs(FC_ADDR,FC_OP1);
		gen_protect_addr_reg();
		gen_mov_word_to_reg(FC_OP1,decode.big_op?(void*)(&reg_eip):(void*)(&reg_ip),decode.big_op);
		gen_add_imm(FC_OP1,(uint32_t)(decode.code-decode.code_start));
		if (decode.big_op) gen_call_function_raw(dynrec_push_dword);
		else gen_call_function_raw(dynrec_push_word);

		gen_restore_addr_reg();
		gen_mov_word_from_reg(FC_ADDR,decode.big_op?(void*)(&reg_eip):(void*)(&reg_ip),decode.big_op);
		return 1;
	case 0x4:	// JMP Ev
		gen_mov_word_from_reg(FC_OP1,decode.big_op?(void*)(&reg_eip):(void*)(&reg_ip),decode.big_op);
		return 1;
	case 0x3:	// CALL Ep
	case 0x5:	// JMP Ep
		if (!decode.big_op) gen_extend_word(false,FC_OP1);
		if (decode.modrm.mod<3) gen_restore_addr_reg();
		gen_protect_reg(FC_OP1);
		gen_add_imm(FC_ADDR,decode.big_op?4:2);
		dyn_read_word(FC_ADDR,FC_OP2,decode.big_op);
		gen_extend_word(false,FC_OP2);

		dyn_set_eip_last_end(FC_RETOP);
		gen_restore_reg(FC_OP1,FC_ADDR);
		gen_call_function_IRRR(decode.modrm.reg == 3 ? CPU_CALL : CPU_JMP,
			decode.big_op,FC_OP2,FC_ADDR,FC_RETOP);
		return 1;
	case 0x6:		// PUSH Ev
		if (decode.big_op) gen_call_function_raw(dynrec_push_dword);
		else gen_call_function_raw(dynrec_push_word);
		break;
	default:
//		IllegalOptionDynrec("dyn_grp4_ev");
		return 2;
	}
	return 0;
}


static bool dyn_grp6(void) {
	dyn_get_modrm();
	switch (decode.modrm.reg) {
		case 0x00:	// SLDT
		case 0x01:	// STR
			if (decode.modrm.reg==0) gen_call_function_raw(CPU_SLDT);
			else gen_call_function_raw(CPU_STR);
			if (decode.modrm.mod<3) {
				dyn_fill_ea(FC_ADDR);
				dyn_write_word(FC_ADDR,FC_RETOP,false);
			} else {
				MOV_REG_WORD16_FROM_HOST_REG(FC_RETOP,decode.modrm.rm);
			}
			break;
		case 0x02:	// LLDT
		case 0x03:	// LTR
		case 0x04:	// VERR
		case 0x05:	// VERW
			if (decode.modrm.mod<3) {
				dyn_fill_ea(FC_ADDR);
				dyn_read_word(FC_ADDR,FC_RETOP,false);
			} else {
				MOV_REG_WORD16_TO_HOST_REG(FC_RETOP,decode.modrm.rm);
			}
			gen_extend_word(false,FC_RETOP);
			switch (decode.modrm.reg) {
				case 0x02:	// LLDT
//					if (cpu.cpl) return CPU_PrepareException(EXCEPTION_GP,0);
					if (cpu.cpl) E_Exit("lldt cpl>0");
					gen_call_function_R(CPU_LLDT,FC_RETOP);
					dyn_check_exception(FC_RETOP);
					break;
				case 0x03:	// LTR
//					if (cpu.cpl) return CPU_PrepareException(EXCEPTION_GP,0);
					if (cpu.cpl) E_Exit("ltr cpl>0");
					gen_call_function_R(CPU_LTR,FC_RETOP);
					dyn_check_exception(FC_RETOP);
					break;
				case 0x04:	// VERR
					gen_call_function_R(CPU_VERR,FC_RETOP);
					break;
				case 0x05:	// VERW
					gen_call_function_R(CPU_VERW,FC_RETOP);
					break;
			}
			break;
		default: IllegalOptionDynrec("dyn_grp6");
	}
	return false;
}

static bool dyn_grp7(void) {
	dyn_get_modrm();
	if (decode.modrm.mod<3) {
		switch (decode.modrm.reg) {
			case 0x00:	// SGDT
				gen_call_function_raw(CPU_SGDT_limit);
				dyn_fill_ea(FC_ADDR);
				gen_protect_addr_reg();
				dyn_write_word(FC_ADDR,FC_RETOP,false);
				gen_call_function_raw(CPU_SGDT_base);
				gen_restore_addr_reg();
				gen_add_imm(FC_ADDR,2);
				dyn_write_word(FC_ADDR,FC_RETOP,true);
				break;
			case 0x01:	// SIDT
				gen_call_function_raw(CPU_SIDT_limit);
				dyn_fill_ea(FC_ADDR);
				gen_protect_addr_reg();
				dyn_write_word(FC_ADDR,FC_RETOP,false);
				gen_call_function_raw(CPU_SIDT_base);
				gen_restore_addr_reg();
				gen_add_imm(FC_ADDR,2);
				dyn_write_word(FC_ADDR,FC_RETOP,true);
				break;
			case 0x02:	// LGDT
			case 0x03:	// LIDT
//				if (cpu.pmode && cpu.cpl) EXCEPTION(EXCEPTION_GP);
				if (cpu.pmode && cpu.cpl) IllegalOptionDynrec("lgdt nonpriviledged");
				dyn_fill_ea(FC_ADDR);
				gen_protect_addr_reg();
				dyn_read_word(FC_ADDR,FC_OP1,false);
				gen_extend_word(false,FC_OP1);
				gen_protect_reg(FC_OP1);

				gen_restore_addr_reg();
				gen_add_imm(FC_ADDR,2);
				dyn_read_word(FC_ADDR,FC_OP2,true);
				if (!decode.big_op) gen_and_imm(FC_OP2,0xffffff);

				gen_restore_reg(FC_OP1);
				if (decode.modrm.reg==2) gen_call_function_RR(CPU_LGDT,FC_OP1,FC_OP2);
				else gen_call_function_RR(CPU_LIDT,FC_OP1,FC_OP2);
				break;
			case 0x04:	// SMSW
				gen_call_function_raw(CPU_SMSW);
				dyn_fill_ea(FC_ADDR);
				dyn_write_word(FC_ADDR,FC_RETOP,false);
				break;
			case 0x06:	// LMSW
				dyn_fill_ea(FC_ADDR);
				dyn_read_word(FC_ADDR,FC_RETOP,false);
				gen_call_function_R(CPU_LMSW,FC_RETOP);
				dyn_check_exception(FC_RETOP);
				dyn_set_eip_end();
				dyn_reduce_cycles();
				dyn_return(BR_Normal);
				dyn_closeblock();
				return true;
			case 0x07:	// INVLPG
//				if (cpu.pmode && cpu.cpl) EXCEPTION(EXCEPTION_GP);
				if (cpu.pmode && cpu.cpl) IllegalOptionDynrec("invlpg nonpriviledged");
				gen_call_function_raw(PAGING_ClearTLB);
				break;
			default: IllegalOptionDynrec("dyn_grp7_1");
		}
	} else {
		switch (decode.modrm.reg) {
			case 0x04:	// SMSW
				gen_call_function_raw(CPU_SMSW);
				MOV_REG_WORD16_FROM_HOST_REG(FC_RETOP,decode.modrm.rm);
				break;
			case 0x06:	// LMSW
				MOV_REG_WORD16_TO_HOST_REG(FC_RETOP,decode.modrm.rm);
				gen_call_function_R(CPU_LMSW,FC_RETOP);
				dyn_check_exception(FC_RETOP);
				dyn_set_eip_end();
				dyn_reduce_cycles();
				dyn_return(BR_Normal);
				dyn_closeblock();
				return true;
			default: IllegalOptionDynrec("dyn_grp7_2");
		}
	}
	return false;
}


/*
static void dyn_larlsl(bool is_lar) {
	dyn_get_modrm();
	if (decode.modrm.mod<3) {
		dyn_fill_ea(FC_ADDR);
		dyn_read_word(FC_ADDR,FC_RETOP,false);
	} else {
		MOV_REG_WORD16_TO_HOST_REG(FC_RETOP,decode.modrm.rm);
	}
	gen_extend_word(false,FC_RETOP);
	if (is_lar) gen_call_function((void*)CPU_LAR,"%R%A",FC_RETOP,(DRC_PTR_SIZE_IM)&core_dynrec.readdata);
	else gen_call_function((void*)CPU_LSL,"%R%A",FC_RETOP,(DRC_PTR_SIZE_IM)&core_dynrec.readdata);
	DRC_PTR_SIZE_IM brnz=gen_create_branch_on_nonzero(FC_RETOP,true);
	gen_mov_word_to_reg(FC_OP2,&core_dynrec.readdata,true);
	MOV_REG_WORD_FROM_HOST_REG(FC_OP2,decode.modrm.reg,decode.big_op);
	gen_fill_branch(brnz);
}
*/


static void dyn_mov_from_crx(void) {
	dyn_get_modrm();
	gen_call_function_IA(CPU_READ_CRX,decode.modrm.reg,(DRC_PTR_SIZE_IM)&core_dynrec.readdata);
	dyn_check_exception(FC_RETOP);
	gen_mov_word_to_reg(FC_OP2,&core_dynrec.readdata,true);
	MOV_REG_WORD32_FROM_HOST_REG(FC_OP2,decode.modrm.rm);
}

static void dyn_mov_to_crx(void) {
	dyn_get_modrm();
	MOV_REG_WORD32_TO_HOST_REG(FC_RETOP,decode.modrm.rm);
	gen_call_function_IR(CPU_WRITE_CRX,decode.modrm.reg,FC_RETOP);
	dyn_check_exception(FC_RETOP);
	dyn_set_eip_end();
	dyn_reduce_cycles();
	dyn_return(BR_Normal);
	dyn_closeblock();
}


static void dyn_cbw(void) {
	if (decode.big_op) {
		MOV_REG_WORD16_TO_HOST_REG(FC_OP1,DRC_REG_EAX);
		gen_call_function_raw(dynrec_cwde);
		MOV_REG_WORD32_FROM_HOST_REG(FC_RETOP,DRC_REG_EAX);
	} else {
		MOV_REG_BYTE_TO_HOST_REG_LOW_CANUSEWORD(FC_OP1,DRC_REG_EAX,0);
		gen_call_function_raw(dynrec_cbw);
		MOV_REG_WORD16_FROM_HOST_REG(FC_RETOP,DRC_REG_EAX);
	}
}

static void dyn_cwd(void) {
	MOV_REG_WORD_TO_HOST_REG(FC_OP1,DRC_REG_EAX,decode.big_op);
	if (decode.big_op) {
		gen_call_function_raw(dynrec_cdq);
		MOV_REG_WORD32_FROM_HOST_REG(FC_RETOP,DRC_REG_EDX);
	} else {
		gen_call_function_raw(dynrec_cwd);
		MOV_REG_WORD16_FROM_HOST_REG(FC_RETOP,DRC_REG_EDX);
	}
}

static void dyn_sahf(void) {
	MOV_REG_WORD16_TO_HOST_REG(FC_OP1,DRC_REG_EAX);
	gen_call_function_raw(dynrec_sahf);
	InvalidateFlags();
}


static void dyn_exit_link(int32_t eip_change) {
	gen_add_direct_word(&reg_eip,(decode.code-decode.code_start)+eip_change,decode.big_op);
	dyn_reduce_cycles();
	gen_jmp_ptr(&decode.block->link[0].to,offsetof(CacheBlockDynRec,cache.xstart));
	dyn_closeblock();
}


static void dyn_branched_exit(BranchTypes btype,int32_t eip_add) {
	uint32_t eip_base=decode.code-decode.code_start;
	dyn_reduce_cycles();

	dyn_branchflag_to_reg(btype);
	DRC_PTR_SIZE_IM data=gen_create_branch_on_nonzero(FC_RETOP,true);

 	// Branch not taken
	gen_add_direct_word(&reg_eip,eip_base,decode.big_op);
 	gen_jmp_ptr(&decode.block->link[0].to,offsetof(CacheBlockDynRec,cache.xstart));
 	gen_fill_branch(data);

 	// Branch taken
	gen_add_direct_word(&reg_eip,eip_base+eip_add,decode.big_op);
 	gen_jmp_ptr(&decode.block->link[1].to,offsetof(CacheBlockDynRec,cache.xstart));
 	dyn_closeblock();
}

/*
static void dyn_set_byte_on_condition(BranchTypes btype) {
	dyn_get_modrm();
	dyn_branchflag_to_reg(btype);
	gen_and_imm(FC_RETOP,1);
	if (decode.modrm.mod<3) {
		dyn_fill_ea(FC_ADDR);
		dyn_write_byte(FC_ADDR,FC_RETOP);
	} else {
		MOV_REG_BYTE_FROM_HOST_REG_LOW(FC_RETOP,decode.modrm.rm&3,(decode.modrm.rm>>2)&1);
	}
}
*/

static void dyn_loop(LoopTypes type) {
	dyn_reduce_cycles();
	int8_t eip_add=(int8_t)decode_fetchb();
	uint32_t eip_base=decode.code-decode.code_start;
	DRC_PTR_SIZE_IM branch1=0;
	DRC_PTR_SIZE_IM branch2=0;
	switch (type) {
	case LOOP_E:
		dyn_branchflag_to_reg(BR_NZ);
		branch1=gen_create_branch_on_nonzero(FC_RETOP,true);
		break;
	case LOOP_NE:
		dyn_branchflag_to_reg(BR_Z);
		branch1=gen_create_branch_on_nonzero(FC_RETOP,true);
		break;
	default:
		break;
	}
	switch (type) {
	case LOOP_E:
	case LOOP_NE:
	case LOOP_NONE:
		MOV_REG_WORD_TO_HOST_REG(FC_OP1,DRC_REG_ECX,decode.big_addr);
		gen_add_imm(FC_OP1,(uint32_t)(-1));
		MOV_REG_WORD_FROM_HOST_REG(FC_OP1,DRC_REG_ECX,decode.big_addr);
		branch2=gen_create_branch_on_zero(FC_OP1,decode.big_addr);
		break;
	case LOOP_JCXZ:
		MOV_REG_WORD_TO_HOST_REG(FC_OP1,DRC_REG_ECX,decode.big_addr);
		branch2=gen_create_branch_on_nonzero(FC_OP1,decode.big_addr);
		break;
	default:
		break;
	}
	gen_add_direct_word(&reg_eip,eip_base+eip_add,true);
	gen_jmp_ptr(&decode.block->link[0].to,offsetof(CacheBlockDynRec,cache.xstart));
	if (branch1) {
		gen_fill_branch(branch1);
		MOV_REG_WORD_TO_HOST_REG(FC_OP1,DRC_REG_ECX,decode.big_addr);
		gen_add_imm(FC_OP1,(uint32_t)(-1));
		MOV_REG_WORD_FROM_HOST_REG(FC_OP1,DRC_REG_ECX,decode.big_addr);
	}
	// Branch taken
	gen_fill_branch(branch2);
	gen_add_direct_word(&reg_eip,eip_base,decode.big_op);
	gen_jmp_ptr(&decode.block->link[1].to,offsetof(CacheBlockDynRec,cache.xstart));
	dyn_closeblock();
}


static void dyn_ret_near(uint16_t bytes) {
	dyn_reduce_cycles();

	if (decode.big_op) gen_call_function_raw(dynrec_pop_dword);
	else {
		gen_call_function_raw(dynrec_pop_word);
		gen_extend_word(false,FC_RETOP);
	}
	gen_mov_word_from_reg(FC_RETOP,decode.big_op?(void*)(&reg_eip):(void*)(&reg_ip),true);

	if (bytes) gen_add_direct_word(&reg_esp,bytes,true);
	dyn_return(BR_Normal);
	dyn_closeblock();
}

static void dyn_call_near_imm(void) {
	int32_t imm;
	if (decode.big_op) imm=(int32_t)decode_fetchd();
	else imm=(int16_t)decode_fetchw();
	dyn_set_eip_end(FC_OP1);
	if (decode.big_op) gen_call_function_raw(dynrec_push_dword);
	else gen_call_function_raw(dynrec_push_word);

	dyn_set_eip_end(FC_OP1,imm);
	gen_mov_word_from_reg(FC_OP1,decode.big_op?(void*)(&reg_eip):(void*)(&reg_ip),decode.big_op);

	dyn_reduce_cycles();
	gen_jmp_ptr(&decode.block->link[0].to,offsetof(CacheBlockDynRec,cache.xstart));
	dyn_closeblock();
}

static void dyn_ret_far(Bitu bytes) {
	dyn_reduce_cycles();
	dyn_set_eip_last_end(FC_RETOP);
	gen_call_function_IIR(CPU_RET,decode.big_op,bytes,FC_RETOP);
	dyn_return(BR_Normal);
	dyn_closeblock();
}

static void dyn_call_far_imm(void) {
	Bitu sel,off;
	off=decode.big_op ? decode_fetchd() : decode_fetchw();
	sel=decode_fetchw();
	dyn_reduce_cycles();
	dyn_set_eip_last_end(FC_RETOP);
	gen_call_function_IIIR(CPU_CALL,decode.big_op,sel,off,FC_RETOP);
	dyn_return(BR_Normal);
	dyn_closeblock();
}

static void dyn_jmp_far_imm(void) {
    uint16_t sel;
    uint32_t off;
	off=decode.big_op ? decode_fetchd() : decode_fetchw();
	sel=decode_fetchw();
	dyn_reduce_cycles();
	dyn_set_eip_last_end(FC_RETOP);
	gen_call_function_IIIR(CPU_JMP,decode.big_op,sel,off,FC_RETOP);
	dyn_return(BR_Normal);
	dyn_closeblock();
}

static void dyn_iret(void) {
	dyn_reduce_cycles();
	dyn_set_eip_last_end(FC_RETOP);
	gen_call_function_IR(CPU_IRET,decode.big_op,FC_RETOP);
	dyn_return(BR_Iret);
	dyn_closeblock();
}

#if !(C_DEBUG)
static void dyn_interrupt(uint8_t num) {
	dyn_reduce_cycles();
	dyn_set_eip_last_end(FC_RETOP);
	gen_call_function_IIR(CPU_Interrupt,num,CPU_INT_SOFTWARE,FC_RETOP);
	dyn_return(BR_Normal);
	dyn_closeblock();
}
#endif


static void dyn_string(StringOps op) {
	if (decode.rep) MOV_REG_WORD_TO_HOST_REG(FC_OP1,DRC_REG_ECX,decode.big_addr);
	else gen_mov_dword_to_reg_imm(FC_OP1,1);
	gen_mov_word_to_reg(FC_OP2,&cpu.direction,true);
	uint8_t di_base_addr=decode.seg_prefix_used ? decode.seg_prefix : DRC_SEG_DS;
	switch (op) {
		case STR_MOVSB:
			if (decode.big_addr) gen_call_function_mm(dynrec_movsb_dword,(Bitu)DRCD_SEG_PHYS(di_base_addr),(Bitu)DRCD_SEG_PHYS(DRC_SEG_ES));
			else gen_call_function_mm(dynrec_movsb_word,(Bitu)DRCD_SEG_PHYS(di_base_addr),(Bitu)DRCD_SEG_PHYS(DRC_SEG_ES));
			break;
		case STR_MOVSW:
			if (decode.big_addr) gen_call_function_mm(dynrec_movsw_dword,(Bitu)DRCD_SEG_PHYS(di_base_addr),(Bitu)DRCD_SEG_PHYS(DRC_SEG_ES));
			else gen_call_function_mm(dynrec_movsw_word,(Bitu)DRCD_SEG_PHYS(di_base_addr),(Bitu)DRCD_SEG_PHYS(DRC_SEG_ES));
			break;
		case STR_MOVSD:
			if (decode.big_addr) gen_call_function_mm(dynrec_movsd_dword,(Bitu)DRCD_SEG_PHYS(di_base_addr),(Bitu)DRCD_SEG_PHYS(DRC_SEG_ES));
			else gen_call_function_mm(dynrec_movsd_word,(Bitu)DRCD_SEG_PHYS(di_base_addr),(Bitu)DRCD_SEG_PHYS(DRC_SEG_ES));
			break;

		case STR_LODSB:
			if (decode.big_addr) gen_call_function_m(dynrec_lodsb_dword,(Bitu)DRCD_SEG_PHYS(di_base_addr));
			else gen_call_function_m(dynrec_lodsb_word,(Bitu)DRCD_SEG_PHYS(di_base_addr));
			break;
		case STR_LODSW:
			if (decode.big_addr) gen_call_function_m(dynrec_lodsw_dword,(Bitu)DRCD_SEG_PHYS(di_base_addr));
			else gen_call_function_m(dynrec_lodsw_word,(Bitu)DRCD_SEG_PHYS(di_base_addr));
			break;
		case STR_LODSD:
			if (decode.big_addr) gen_call_function_m(dynrec_lodsd_dword,(Bitu)DRCD_SEG_PHYS(di_base_addr));
			else gen_call_function_m(dynrec_lodsd_word,(Bitu)DRCD_SEG_PHYS(di_base_addr));
			break;

		case STR_STOSB:
			if (decode.big_addr) gen_call_function_m(dynrec_stosb_dword,(Bitu)DRCD_SEG_PHYS(DRC_SEG_ES));
			else gen_call_function_m(dynrec_stosb_word,(Bitu)DRCD_SEG_PHYS(DRC_SEG_ES));
			break;
		case STR_STOSW:
			if (decode.big_addr) gen_call_function_m(dynrec_stosw_dword,(Bitu)DRCD_SEG_PHYS(DRC_SEG_ES));
			else gen_call_function_m(dynrec_stosw_word,(Bitu)DRCD_SEG_PHYS(DRC_SEG_ES));
			break;
		case STR_STOSD:
			if (decode.big_addr) gen_call_function_m(dynrec_stosd_dword,(Bitu)DRCD_SEG_PHYS(DRC_SEG_ES));
			else gen_call_function_m(dynrec_stosd_word,(Bitu)DRCD_SEG_PHYS(DRC_SEG_ES));
			break;
		default: IllegalOptionDynrec("dyn_string");
	}
	if (decode.rep) MOV_REG_WORD_FROM_HOST_REG(FC_RETOP,DRC_REG_ECX,decode.big_addr);

	if (op<STR_SCASB) {
		// those string operations are allowed for premature termination
		// when not enough cycles left
		if (!decode.big_addr) gen_extend_word(false,FC_RETOP);
		save_info_dynrec[used_save_info_dynrec].branch_pos=gen_create_branch_long_nonzero(FC_RETOP,true);
		save_info_dynrec[used_save_info_dynrec].eip_change=decode.op_start-decode.code_start;
		save_info_dynrec[used_save_info_dynrec].type=string_break;
		used_save_info_dynrec++;
	}
}


static void dyn_read_port_byte_direct(uint8_t port) {
	gen_mov_dword_to_reg_imm(FC_OP1,port);
	gen_call_function_raw(dynrec_io_readB);
	dyn_check_exception(FC_RETOP);
}

static void dyn_read_port_word_direct(uint8_t port) {
	gen_mov_dword_to_reg_imm(FC_OP1,port);
    if(decode.big_op)
        gen_call_function_raw(dynrec_io_readD);
    else
        gen_call_function_raw(dynrec_io_readW);
	dyn_check_exception(FC_RETOP);
}

static void dyn_write_port_byte_direct(uint8_t port) {
	gen_mov_dword_to_reg_imm(FC_OP1,port);
	gen_call_function_raw(dynrec_io_writeB);
	dyn_check_exception(FC_RETOP);
}

static void dyn_write_port_word_direct(uint8_t port) {
	gen_mov_dword_to_reg_imm(FC_OP1,port);
    if(decode.big_op)
        gen_call_function_raw(dynrec_io_writeD);
    else
        gen_call_function_raw(dynrec_io_writeW);
	dyn_check_exception(FC_RETOP);
}


static void dyn_read_port_byte(void) {
	MOV_REG_WORD16_TO_HOST_REG(FC_OP1,DRC_REG_EDX);
	gen_extend_word(false,FC_OP1);
	gen_call_function_raw(dynrec_io_readB);
	dyn_check_exception(FC_RETOP);
}

static void dyn_read_port_word(void) {
	MOV_REG_WORD16_TO_HOST_REG(FC_OP1,DRC_REG_EDX);
	gen_extend_word(false,FC_OP1);
    if(decode.big_op)
        gen_call_function_raw(dynrec_io_readD);
    else
        gen_call_function_raw(dynrec_io_readW);
	dyn_check_exception(FC_RETOP);
}

static void dyn_write_port_byte(void) {
	MOV_REG_WORD16_TO_HOST_REG(FC_OP1,DRC_REG_EDX);
	gen_extend_word(false,FC_OP1);
	gen_call_function_raw(dynrec_io_writeB);
	dyn_check_exception(FC_RETOP);
}

static void dyn_write_port_word(void) {
	MOV_REG_WORD16_TO_HOST_REG(FC_OP1,DRC_REG_EDX);
	gen_extend_word(false,FC_OP1);
    if(decode.big_op)
        gen_call_function_raw(dynrec_io_writeD);
    else
        gen_call_function_raw(dynrec_io_writeW);
	dyn_check_exception(FC_RETOP);
}


static void dyn_enter(void) {
	Bitu bytes=decode_fetchw();
	Bitu level=decode_fetchb();
	gen_call_function_III(CPU_ENTER,decode.big_op,bytes,level);
}

static void dynrec_leave_word(void) {
	reg_esp&=cpu.stack.notmask;
	reg_esp|=(reg_ebp&cpu.stack.mask);
	reg_bp=CPU_Pop16();
}

static void dynrec_leave_dword(void) {
	reg_esp&=cpu.stack.notmask;
	reg_esp|=(reg_ebp&cpu.stack.mask);
	reg_ebp=CPU_Pop32();
}

static void dyn_leave(void) {
	if (decode.big_op) gen_call_function_raw(dynrec_leave_dword);
	else gen_call_function_raw(dynrec_leave_word);
}


static void dynrec_pusha_word(void) {
	uint16_t old_sp=reg_sp;
	CPU_Push16(reg_ax);CPU_Push16(reg_cx);CPU_Push16(reg_dx);CPU_Push16(reg_bx);
	CPU_Push16(old_sp);CPU_Push16(reg_bp);CPU_Push16(reg_si);CPU_Push16(reg_di);
}

static void dynrec_pusha_dword(void) {
	uint32_t tmpesp = reg_esp;
	CPU_Push32(reg_eax);CPU_Push32(reg_ecx);CPU_Push32(reg_edx);CPU_Push32(reg_ebx);
	CPU_Push32(tmpesp);CPU_Push32(reg_ebp);CPU_Push32(reg_esi);CPU_Push32(reg_edi);
}

static void dynrec_popa_word(void) {
	reg_di=CPU_Pop16();reg_si=CPU_Pop16();
	reg_bp=CPU_Pop16();CPU_Pop16();		//Don't save SP
	reg_bx=CPU_Pop16();reg_dx=CPU_Pop16();
	reg_cx=CPU_Pop16();reg_ax=CPU_Pop16();
}

static void dynrec_popa_dword(void) {
	reg_edi=CPU_Pop32();reg_esi=CPU_Pop32();reg_ebp=CPU_Pop32();CPU_Pop32();	//Don't save ESP
	reg_ebx=CPU_Pop32();reg_edx=CPU_Pop32();reg_ecx=CPU_Pop32();reg_eax=CPU_Pop32();
}
