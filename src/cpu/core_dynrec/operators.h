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



static uint8_t DRC_CALL_CONV dynrec_add_byte(uint8_t op1,uint8_t op2) DRC_FC;
static uint8_t DRC_CALL_CONV dynrec_add_byte(uint8_t op1,uint8_t op2) {
	lf_var1b=op1;
	lf_var2b=op2;
	lf_resb=(uint8_t)(lf_var1b+lf_var2b);
	lflags.type=t_ADDb;
	return lf_resb;
}

static uint8_t DRC_CALL_CONV dynrec_add_byte_simple(uint8_t op1,uint8_t op2) DRC_FC;
static uint8_t DRC_CALL_CONV dynrec_add_byte_simple(uint8_t op1,uint8_t op2) {
	return op1+op2;
}

static uint8_t DRC_CALL_CONV dynrec_adc_byte(uint8_t op1,uint8_t op2) DRC_FC;
static uint8_t DRC_CALL_CONV dynrec_adc_byte(uint8_t op1,uint8_t op2) {
	lflags.oldcf=get_CF()!=0;
	lf_var1b=op1;
	lf_var2b=op2;
	lf_resb=(uint8_t)(lf_var1b+lf_var2b+lflags.oldcf);
	lflags.type=t_ADCb;
	return lf_resb;
}

static uint8_t DRC_CALL_CONV dynrec_adc_byte_simple(uint8_t op1,uint8_t op2) DRC_FC;
static uint8_t DRC_CALL_CONV dynrec_adc_byte_simple(uint8_t op1,uint8_t op2) {
	return (uint8_t)(op1+op2+(Bitu)(get_CF()!=0));
}

static uint8_t DRC_CALL_CONV dynrec_sub_byte(uint8_t op1,uint8_t op2) DRC_FC;
static uint8_t DRC_CALL_CONV dynrec_sub_byte(uint8_t op1,uint8_t op2) {
	lf_var1b=op1;
	lf_var2b=op2;
	lf_resb=(uint8_t)(lf_var1b-lf_var2b);
	lflags.type=t_SUBb;
	return lf_resb;
}

static uint8_t DRC_CALL_CONV dynrec_sub_byte_simple(uint8_t op1,uint8_t op2) DRC_FC;
static uint8_t DRC_CALL_CONV dynrec_sub_byte_simple(uint8_t op1,uint8_t op2) {
	return op1-op2;
}

static uint8_t DRC_CALL_CONV dynrec_sbb_byte(uint8_t op1,uint8_t op2) DRC_FC;
static uint8_t DRC_CALL_CONV dynrec_sbb_byte(uint8_t op1,uint8_t op2) {
	lflags.oldcf=get_CF()!=0;
	lf_var1b=op1;
	lf_var2b=op2;
	lf_resb=(uint8_t)(lf_var1b-(lf_var2b+lflags.oldcf));
	lflags.type=t_SBBb;
	return lf_resb;
}

static uint8_t DRC_CALL_CONV dynrec_sbb_byte_simple(uint8_t op1,uint8_t op2) DRC_FC;
static uint8_t DRC_CALL_CONV dynrec_sbb_byte_simple(uint8_t op1,uint8_t op2) {
	return (uint8_t)(op1-(op2+(Bitu)(get_CF()!=0)));
}

static void DRC_CALL_CONV dynrec_cmp_byte(uint8_t op1,uint8_t op2) DRC_FC;
static void DRC_CALL_CONV dynrec_cmp_byte(uint8_t op1,uint8_t op2) {
	lf_var1b=op1;
	lf_var2b=op2;
	lf_resb=(uint8_t)(lf_var1b-lf_var2b);
	lflags.type=t_CMPb;
}

static void DRC_CALL_CONV dynrec_cmp_byte_simple(uint8_t op1,uint8_t op2) DRC_FC;
static void DRC_CALL_CONV dynrec_cmp_byte_simple(uint8_t op1,uint8_t op2) {
	(void)op1;
	(void)op2;
}

static uint8_t DRC_CALL_CONV dynrec_xor_byte(uint8_t op1,uint8_t op2) DRC_FC;
static uint8_t DRC_CALL_CONV dynrec_xor_byte(uint8_t op1,uint8_t op2) {
	lf_var1b=op1;
	lf_var2b=op2;
	lf_resb=lf_var1b ^ lf_var2b;
	lflags.type=t_XORb;
	return lf_resb;
}

static uint8_t DRC_CALL_CONV dynrec_xor_byte_simple(uint8_t op1,uint8_t op2) DRC_FC;
static uint8_t DRC_CALL_CONV dynrec_xor_byte_simple(uint8_t op1,uint8_t op2) {
	return op1 ^ op2;
}

static uint8_t DRC_CALL_CONV dynrec_and_byte(uint8_t op1,uint8_t op2) DRC_FC;
static uint8_t DRC_CALL_CONV dynrec_and_byte(uint8_t op1,uint8_t op2) {
	lf_var1b=op1;
	lf_var2b=op2;
	lf_resb=lf_var1b & lf_var2b;
	lflags.type=t_ANDb;
	return lf_resb;
}

static uint8_t DRC_CALL_CONV dynrec_and_byte_simple(uint8_t op1,uint8_t op2) DRC_FC;
static uint8_t DRC_CALL_CONV dynrec_and_byte_simple(uint8_t op1,uint8_t op2) {
	return op1 & op2;
}

static uint8_t DRC_CALL_CONV dynrec_or_byte(uint8_t op1,uint8_t op2) DRC_FC;
static uint8_t DRC_CALL_CONV dynrec_or_byte(uint8_t op1,uint8_t op2) {
	lf_var1b=op1;
	lf_var2b=op2;
	lf_resb=lf_var1b | lf_var2b;
	lflags.type=t_ORb;
	return lf_resb;
}

static uint8_t DRC_CALL_CONV dynrec_or_byte_simple(uint8_t op1,uint8_t op2) DRC_FC;
static uint8_t DRC_CALL_CONV dynrec_or_byte_simple(uint8_t op1,uint8_t op2) {
	return op1 | op2;
}

static void DRC_CALL_CONV dynrec_test_byte(uint8_t op1,uint8_t op2) DRC_FC;
static void DRC_CALL_CONV dynrec_test_byte(uint8_t op1,uint8_t op2) {
	lf_var1b=op1;
	lf_var2b=op2;
	lf_resb=lf_var1b & lf_var2b;
	lflags.type=t_TESTb;
}

static void DRC_CALL_CONV dynrec_test_byte_simple(uint8_t op1,uint8_t op2) DRC_FC;
static void DRC_CALL_CONV dynrec_test_byte_simple(uint8_t op1,uint8_t op2) {
	(void)op1;
	(void)op2;
}

static Bit16u DRC_CALL_CONV dynrec_add_word(Bit16u op1,Bit16u op2) DRC_FC;
static Bit16u DRC_CALL_CONV dynrec_add_word(Bit16u op1,Bit16u op2) {
	lf_var1w=op1;
	lf_var2w=op2;
	lf_resw=(Bit16u)(lf_var1w+lf_var2w);
	lflags.type=t_ADDw;
	return lf_resw;
}

static Bit16u DRC_CALL_CONV dynrec_add_word_simple(Bit16u op1,Bit16u op2) DRC_FC;
static Bit16u DRC_CALL_CONV dynrec_add_word_simple(Bit16u op1,Bit16u op2) {
	return op1+op2;
}

static Bit16u DRC_CALL_CONV dynrec_adc_word(Bit16u op1,Bit16u op2) DRC_FC;
static Bit16u DRC_CALL_CONV dynrec_adc_word(Bit16u op1,Bit16u op2) {
	lflags.oldcf=get_CF()!=0;
	lf_var1w=op1;
	lf_var2w=op2;
	lf_resw=(Bit16u)(lf_var1w+lf_var2w+lflags.oldcf);
	lflags.type=t_ADCw;
	return lf_resw;
}

static Bit16u DRC_CALL_CONV dynrec_adc_word_simple(Bit16u op1,Bit16u op2) DRC_FC;
static Bit16u DRC_CALL_CONV dynrec_adc_word_simple(Bit16u op1,Bit16u op2) {
	return (Bit16u)(op1+op2+(Bitu)(get_CF()!=0));
}

static Bit16u DRC_CALL_CONV dynrec_sub_word(Bit16u op1,Bit16u op2) DRC_FC;
static Bit16u DRC_CALL_CONV dynrec_sub_word(Bit16u op1,Bit16u op2) {
	lf_var1w=op1;
	lf_var2w=op2;
	lf_resw=(Bit16u)(lf_var1w-lf_var2w);
	lflags.type=t_SUBw;
	return lf_resw;
}

static Bit16u DRC_CALL_CONV dynrec_sub_word_simple(Bit16u op1,Bit16u op2) DRC_FC;
static Bit16u DRC_CALL_CONV dynrec_sub_word_simple(Bit16u op1,Bit16u op2) {
	return op1-op2;
}

static Bit16u DRC_CALL_CONV dynrec_sbb_word(Bit16u op1,Bit16u op2) DRC_FC;
static Bit16u DRC_CALL_CONV dynrec_sbb_word(Bit16u op1,Bit16u op2) {
	lflags.oldcf=get_CF()!=0;
	lf_var1w=op1;
	lf_var2w=op2;
	lf_resw=(Bit16u)(lf_var1w-(lf_var2w+lflags.oldcf));
	lflags.type=t_SBBw;
	return lf_resw;
}

static Bit16u DRC_CALL_CONV dynrec_sbb_word_simple(Bit16u op1,Bit16u op2) DRC_FC;
static Bit16u DRC_CALL_CONV dynrec_sbb_word_simple(Bit16u op1,Bit16u op2) {
	return (Bit16u)(op1-(op2+(Bitu)(get_CF()!=0)));
}

static void DRC_CALL_CONV dynrec_cmp_word(Bit16u op1,Bit16u op2) DRC_FC;
static void DRC_CALL_CONV dynrec_cmp_word(Bit16u op1,Bit16u op2) {
	lf_var1w=op1;
	lf_var2w=op2;
	lf_resw=(Bit16u)(lf_var1w-lf_var2w);
	lflags.type=t_CMPw;
}

static void DRC_CALL_CONV dynrec_cmp_word_simple(Bit16u op1,Bit16u op2) DRC_FC;
static void DRC_CALL_CONV dynrec_cmp_word_simple(Bit16u op1,Bit16u op2) {
	(void)op1;
	(void)op2;
}

static Bit16u DRC_CALL_CONV dynrec_xor_word(Bit16u op1,Bit16u op2) DRC_FC;
static Bit16u DRC_CALL_CONV dynrec_xor_word(Bit16u op1,Bit16u op2) {
	lf_var1w=op1;
	lf_var2w=op2;
	lf_resw=lf_var1w ^ lf_var2w;
	lflags.type=t_XORw;
	return lf_resw;
}

static Bit16u DRC_CALL_CONV dynrec_xor_word_simple(Bit16u op1,Bit16u op2) DRC_FC;
static Bit16u DRC_CALL_CONV dynrec_xor_word_simple(Bit16u op1,Bit16u op2) {
	return op1 ^ op2;
}

static Bit16u DRC_CALL_CONV dynrec_and_word(Bit16u op1,Bit16u op2) DRC_FC;
static Bit16u DRC_CALL_CONV dynrec_and_word(Bit16u op1,Bit16u op2) {
	lf_var1w=op1;
	lf_var2w=op2;
	lf_resw=lf_var1w & lf_var2w;
	lflags.type=t_ANDw;
	return lf_resw;
}

static Bit16u DRC_CALL_CONV dynrec_and_word_simple(Bit16u op1,Bit16u op2) DRC_FC;
static Bit16u DRC_CALL_CONV dynrec_and_word_simple(Bit16u op1,Bit16u op2) {
	return op1 & op2;
}

static Bit16u DRC_CALL_CONV dynrec_or_word(Bit16u op1,Bit16u op2) DRC_FC;
static Bit16u DRC_CALL_CONV dynrec_or_word(Bit16u op1,Bit16u op2) {
	lf_var1w=op1;
	lf_var2w=op2;
	lf_resw=lf_var1w | lf_var2w;
	lflags.type=t_ORw;
	return lf_resw;
}

static Bit16u DRC_CALL_CONV dynrec_or_word_simple(Bit16u op1,Bit16u op2) DRC_FC;
static Bit16u DRC_CALL_CONV dynrec_or_word_simple(Bit16u op1,Bit16u op2) {
	return op1 | op2;
}

static void DRC_CALL_CONV dynrec_test_word(Bit16u op1,Bit16u op2) DRC_FC;
static void DRC_CALL_CONV dynrec_test_word(Bit16u op1,Bit16u op2) {
	lf_var1w=op1;
	lf_var2w=op2;
	lf_resw=lf_var1w & lf_var2w;
	lflags.type=t_TESTw;
}

static void DRC_CALL_CONV dynrec_test_word_simple(Bit16u op1,Bit16u op2) DRC_FC;
static void DRC_CALL_CONV dynrec_test_word_simple(Bit16u op1,Bit16u op2) {
	(void)op1;
	(void)op2;
}

static Bit32u DRC_CALL_CONV dynrec_add_dword(Bit32u op1,Bit32u op2) DRC_FC;
static Bit32u DRC_CALL_CONV dynrec_add_dword(Bit32u op1,Bit32u op2) {
	lf_var1d=op1;
	lf_var2d=op2;
	lf_resd=lf_var1d+lf_var2d;
	lflags.type=t_ADDd;
	return lf_resd;
}

static Bit32u DRC_CALL_CONV dynrec_add_dword_simple(Bit32u op1,Bit32u op2) DRC_FC;
static Bit32u DRC_CALL_CONV dynrec_add_dword_simple(Bit32u op1,Bit32u op2) {
	return op1 + op2;
}

static Bit32u DRC_CALL_CONV dynrec_adc_dword(Bit32u op1,Bit32u op2) DRC_FC;
static Bit32u DRC_CALL_CONV dynrec_adc_dword(Bit32u op1,Bit32u op2) {
	lflags.oldcf=get_CF()!=0;
	lf_var1d=op1;
	lf_var2d=op2;
	lf_resd=lf_var1d+lf_var2d+lflags.oldcf;
	lflags.type=t_ADCd;
	return lf_resd;
}

static Bit32u DRC_CALL_CONV dynrec_adc_dword_simple(Bit32u op1,Bit32u op2) DRC_FC;
static Bit32u DRC_CALL_CONV dynrec_adc_dword_simple(Bit32u op1,Bit32u op2) {
	return op1+op2+(Bitu)(get_CF()!=0);
}

static Bit32u DRC_CALL_CONV dynrec_sub_dword(Bit32u op1,Bit32u op2) DRC_FC;
static Bit32u DRC_CALL_CONV dynrec_sub_dword(Bit32u op1,Bit32u op2) {
	lf_var1d=op1;
	lf_var2d=op2;
	lf_resd=lf_var1d-lf_var2d;
	lflags.type=t_SUBd;
	return lf_resd;
}

static Bit32u DRC_CALL_CONV dynrec_sub_dword_simple(Bit32u op1,Bit32u op2) DRC_FC;
static Bit32u DRC_CALL_CONV dynrec_sub_dword_simple(Bit32u op1,Bit32u op2) {
	return op1-op2;
}

static Bit32u DRC_CALL_CONV dynrec_sbb_dword(Bit32u op1,Bit32u op2) DRC_FC;
static Bit32u DRC_CALL_CONV dynrec_sbb_dword(Bit32u op1,Bit32u op2) {
	lflags.oldcf=get_CF()!=0;
	lf_var1d=op1;
	lf_var2d=op2;
	lf_resd=lf_var1d-(lf_var2d+lflags.oldcf);
	lflags.type=t_SBBd;
	return lf_resd;
}

static Bit32u DRC_CALL_CONV dynrec_sbb_dword_simple(Bit32u op1,Bit32u op2) DRC_FC;
static Bit32u DRC_CALL_CONV dynrec_sbb_dword_simple(Bit32u op1,Bit32u op2) {
	return op1-(op2+(Bitu)(get_CF()!=0));
}

static void DRC_CALL_CONV dynrec_cmp_dword(Bit32u op1,Bit32u op2) DRC_FC;
static void DRC_CALL_CONV dynrec_cmp_dword(Bit32u op1,Bit32u op2) {
	lf_var1d=op1;
	lf_var2d=op2;
	lf_resd=lf_var1d-lf_var2d;
	lflags.type=t_CMPd;
}

static void DRC_CALL_CONV dynrec_cmp_dword_simple(Bit32u op1,Bit32u op2) DRC_FC;
static void DRC_CALL_CONV dynrec_cmp_dword_simple(Bit32u op1,Bit32u op2) {
	(void)op1;
	(void)op2;
}

static Bit32u DRC_CALL_CONV dynrec_xor_dword(Bit32u op1,Bit32u op2) DRC_FC;
static Bit32u DRC_CALL_CONV dynrec_xor_dword(Bit32u op1,Bit32u op2) {
	lf_var1d=op1;
	lf_var2d=op2;
	lf_resd=lf_var1d ^ lf_var2d;
	lflags.type=t_XORd;
	return lf_resd;
}

static Bit32u DRC_CALL_CONV dynrec_xor_dword_simple(Bit32u op1,Bit32u op2) DRC_FC;
static Bit32u DRC_CALL_CONV dynrec_xor_dword_simple(Bit32u op1,Bit32u op2) {
	return op1 ^ op2;
}

static Bit32u DRC_CALL_CONV dynrec_and_dword(Bit32u op1,Bit32u op2) DRC_FC;
static Bit32u DRC_CALL_CONV dynrec_and_dword(Bit32u op1,Bit32u op2) {
	lf_var1d=op1;
	lf_var2d=op2;
	lf_resd=lf_var1d & lf_var2d;
	lflags.type=t_ANDd;
	return lf_resd;
}

static Bit32u DRC_CALL_CONV dynrec_and_dword_simple(Bit32u op1,Bit32u op2) DRC_FC;
static Bit32u DRC_CALL_CONV dynrec_and_dword_simple(Bit32u op1,Bit32u op2) {
	return op1 & op2;
}

static Bit32u DRC_CALL_CONV dynrec_or_dword(Bit32u op1,Bit32u op2) DRC_FC;
static Bit32u DRC_CALL_CONV dynrec_or_dword(Bit32u op1,Bit32u op2) {
	lf_var1d=op1;
	lf_var2d=op2;
	lf_resd=lf_var1d | lf_var2d;
	lflags.type=t_ORd;
	return lf_resd;
}

static Bit32u DRC_CALL_CONV dynrec_or_dword_simple(Bit32u op1,Bit32u op2) DRC_FC;
static Bit32u DRC_CALL_CONV dynrec_or_dword_simple(Bit32u op1,Bit32u op2) {
	return op1 | op2;
}

static void DRC_CALL_CONV dynrec_test_dword(Bit32u op1,Bit32u op2) DRC_FC;
static void DRC_CALL_CONV dynrec_test_dword(Bit32u op1,Bit32u op2) {
	lf_var1d=op1;
	lf_var2d=op2;
	lf_resd=lf_var1d & lf_var2d;
	lflags.type=t_TESTd;
}

static void DRC_CALL_CONV dynrec_test_dword_simple(Bit32u op1,Bit32u op2) DRC_FC;
static void DRC_CALL_CONV dynrec_test_dword_simple(Bit32u op1,Bit32u op2) {
	(void)op1;
	(void)op2;
}


static void dyn_dop_byte_gencall(DualOps op) {
	switch (op) {
		case DOP_ADD:
			InvalidateFlags(dynrec_add_byte_simple,t_ADDb);
			gen_call_function_raw(dynrec_add_byte);
			break;
		case DOP_ADC:
			AcquireFlags(FLAG_CF);
			InvalidateFlagsPartially(dynrec_adc_byte_simple,t_ADCb);
			gen_call_function_raw(dynrec_adc_byte);
			break;
		case DOP_SUB:
			InvalidateFlags(dynrec_sub_byte_simple,t_SUBb);
			gen_call_function_raw(dynrec_sub_byte);
			break;
		case DOP_SBB:
			AcquireFlags(FLAG_CF);
			InvalidateFlagsPartially(dynrec_sbb_byte_simple,t_SBBb);
			gen_call_function_raw(dynrec_sbb_byte);
			break;
		case DOP_CMP:
			InvalidateFlags(dynrec_cmp_byte_simple,t_CMPb);
			gen_call_function_raw(dynrec_cmp_byte);
			break;
		case DOP_XOR:
			InvalidateFlags(dynrec_xor_byte_simple,t_XORb);
			gen_call_function_raw(dynrec_xor_byte);
			break;
		case DOP_AND:
			InvalidateFlags(dynrec_and_byte_simple,t_ANDb);
			gen_call_function_raw(dynrec_and_byte);
			break;
		case DOP_OR:
			InvalidateFlags(dynrec_or_byte_simple,t_ORb);
			gen_call_function_raw(dynrec_or_byte);
			break;
		case DOP_TEST:
			InvalidateFlags(dynrec_test_byte_simple,t_TESTb);
			gen_call_function_raw(dynrec_test_byte);
			break;
		default: IllegalOptionDynrec("dyn_dop_byte_gencall");
	}
}

static void dyn_dop_word_gencall(DualOps op,bool dword) {
	if (dword) {
		switch (op) {
			case DOP_ADD:
				InvalidateFlags(dynrec_add_dword_simple,t_ADDd);
				gen_call_function_raw(dynrec_add_dword);
				break;
			case DOP_ADC:
				AcquireFlags(FLAG_CF);
				InvalidateFlagsPartially(dynrec_adc_dword_simple,t_ADCd);
				gen_call_function_raw(dynrec_adc_dword);
				break;
			case DOP_SUB:
				InvalidateFlags(dynrec_sub_dword_simple,t_SUBd);
				gen_call_function_raw(dynrec_sub_dword);
				break;
			case DOP_SBB:
				AcquireFlags(FLAG_CF);
				InvalidateFlagsPartially(dynrec_sbb_dword_simple,t_SBBd);
				gen_call_function_raw(dynrec_sbb_dword);
				break;
			case DOP_CMP:
				InvalidateFlags(dynrec_cmp_dword_simple,t_CMPd);
				gen_call_function_raw(dynrec_cmp_dword);
				break;
			case DOP_XOR:
				InvalidateFlags(dynrec_xor_dword_simple,t_XORd);
				gen_call_function_raw(dynrec_xor_dword);
				break;
			case DOP_AND:
				InvalidateFlags(dynrec_and_dword_simple,t_ANDd);
				gen_call_function_raw(dynrec_and_dword);
				break;
			case DOP_OR:
				InvalidateFlags(dynrec_or_dword_simple,t_ORd);
				gen_call_function_raw(dynrec_or_dword);
				break;
			case DOP_TEST:
				InvalidateFlags(dynrec_test_dword_simple,t_TESTd);
				gen_call_function_raw(dynrec_test_dword);
				break;
			default: IllegalOptionDynrec("dyn_dop_dword_gencall");
		}
	} else {
		switch (op) {
			case DOP_ADD:
				InvalidateFlags(dynrec_add_word_simple,t_ADDw);
				gen_call_function_raw(dynrec_add_word);
				break;
			case DOP_ADC:
				AcquireFlags(FLAG_CF);
				InvalidateFlagsPartially(dynrec_adc_word_simple,t_ADCw);
				gen_call_function_raw(dynrec_adc_word);
				break;
			case DOP_SUB:
				InvalidateFlags(dynrec_sub_word_simple,t_SUBw);
				gen_call_function_raw(dynrec_sub_word);
				break;
			case DOP_SBB:
				AcquireFlags(FLAG_CF);
				InvalidateFlagsPartially(dynrec_sbb_word_simple,t_SBBw);
				gen_call_function_raw(dynrec_sbb_word);
				break;
			case DOP_CMP:
				InvalidateFlags(dynrec_cmp_word_simple,t_CMPw);
				gen_call_function_raw(dynrec_cmp_word);
				break;
			case DOP_XOR:
				InvalidateFlags(dynrec_xor_word_simple,t_XORw);
				gen_call_function_raw(dynrec_xor_word);
				break;
			case DOP_AND:
				InvalidateFlags(dynrec_and_word_simple,t_ANDw);
				gen_call_function_raw(dynrec_and_word);
				break;
			case DOP_OR:
				InvalidateFlags(dynrec_or_word_simple,t_ORw);
				gen_call_function_raw(dynrec_or_word);
				break;
			case DOP_TEST:
				InvalidateFlags(dynrec_test_word_simple,t_TESTw);
				gen_call_function_raw(dynrec_test_word);
				break;
			default: IllegalOptionDynrec("dyn_dop_word_gencall");
		}
	}
}


static uint8_t DRC_CALL_CONV dynrec_inc_byte(uint8_t op) DRC_FC;
static uint8_t DRC_CALL_CONV dynrec_inc_byte(uint8_t op) {
	LoadCF;
	lf_var1b=op;
	lf_resb=lf_var1b+1;
	lflags.type=t_INCb;
	return lf_resb;
}

static uint8_t DRC_CALL_CONV dynrec_inc_byte_simple(uint8_t op) DRC_FC;
static uint8_t DRC_CALL_CONV dynrec_inc_byte_simple(uint8_t op) {
	return op+1;
}

static uint8_t DRC_CALL_CONV dynrec_dec_byte(uint8_t op) DRC_FC;
static uint8_t DRC_CALL_CONV dynrec_dec_byte(uint8_t op) {
	LoadCF;
	lf_var1b=op;
	lf_resb=lf_var1b-1;
	lflags.type=t_DECb;
	return lf_resb;
}

static uint8_t DRC_CALL_CONV dynrec_dec_byte_simple(uint8_t op) DRC_FC;
static uint8_t DRC_CALL_CONV dynrec_dec_byte_simple(uint8_t op) {
	return op-1;
}

static uint8_t DRC_CALL_CONV dynrec_not_byte(uint8_t op) DRC_FC;
static uint8_t DRC_CALL_CONV dynrec_not_byte(uint8_t op) {
	return ~op;
}

static uint8_t DRC_CALL_CONV dynrec_neg_byte(uint8_t op) DRC_FC;
static uint8_t DRC_CALL_CONV dynrec_neg_byte(uint8_t op) {
	lf_var1b=op;
	lf_resb=0-lf_var1b;
	lflags.type=t_NEGb;
	return lf_resb;
}

static uint8_t DRC_CALL_CONV dynrec_neg_byte_simple(uint8_t op) DRC_FC;
static uint8_t DRC_CALL_CONV dynrec_neg_byte_simple(uint8_t op) {
	return 0-op;
}

static Bit16u DRC_CALL_CONV dynrec_inc_word(Bit16u op) DRC_FC;
static Bit16u DRC_CALL_CONV dynrec_inc_word(Bit16u op) {
	LoadCF;
	lf_var1w=op;
	lf_resw=lf_var1w+1;
	lflags.type=t_INCw;
	return lf_resw;
}

static Bit16u DRC_CALL_CONV dynrec_inc_word_simple(Bit16u op) DRC_FC;
static Bit16u DRC_CALL_CONV dynrec_inc_word_simple(Bit16u op) {
	return op+1;
}

static Bit16u DRC_CALL_CONV dynrec_dec_word(Bit16u op) DRC_FC;
static Bit16u DRC_CALL_CONV dynrec_dec_word(Bit16u op) {
	LoadCF;
	lf_var1w=op;
	lf_resw=lf_var1w-1;
	lflags.type=t_DECw;
	return lf_resw;
}

static Bit16u DRC_CALL_CONV dynrec_dec_word_simple(Bit16u op) DRC_FC;
static Bit16u DRC_CALL_CONV dynrec_dec_word_simple(Bit16u op) {
	return op-1;
}

static Bit16u DRC_CALL_CONV dynrec_not_word(Bit16u op) DRC_FC;
static Bit16u DRC_CALL_CONV dynrec_not_word(Bit16u op) {
	return ~op;
}

static Bit16u DRC_CALL_CONV dynrec_neg_word(Bit16u op) DRC_FC;
static Bit16u DRC_CALL_CONV dynrec_neg_word(Bit16u op) {
	lf_var1w=op;
	lf_resw=0-lf_var1w;
	lflags.type=t_NEGw;
	return lf_resw;
}

static Bit16u DRC_CALL_CONV dynrec_neg_word_simple(Bit16u op) DRC_FC;
static Bit16u DRC_CALL_CONV dynrec_neg_word_simple(Bit16u op) {
	return 0-op;
}

static Bit32u DRC_CALL_CONV dynrec_inc_dword(Bit32u op) DRC_FC;
static Bit32u DRC_CALL_CONV dynrec_inc_dword(Bit32u op) {
	LoadCF;
	lf_var1d=op;
	lf_resd=lf_var1d+1;
	lflags.type=t_INCd;
	return lf_resd;
}

static Bit32u DRC_CALL_CONV dynrec_inc_dword_simple(Bit32u op) DRC_FC;
static Bit32u DRC_CALL_CONV dynrec_inc_dword_simple(Bit32u op) {
	return op+1;
}

static Bit32u DRC_CALL_CONV dynrec_dec_dword(Bit32u op) DRC_FC;
static Bit32u DRC_CALL_CONV dynrec_dec_dword(Bit32u op) {
	LoadCF;
	lf_var1d=op;
	lf_resd=lf_var1d-1;
	lflags.type=t_DECd;
	return lf_resd;
}

static Bit32u DRC_CALL_CONV dynrec_dec_dword_simple(Bit32u op) DRC_FC;
static Bit32u DRC_CALL_CONV dynrec_dec_dword_simple(Bit32u op) {
	return op-1;
}

static Bit32u DRC_CALL_CONV dynrec_not_dword(Bit32u op) DRC_FC;
static Bit32u DRC_CALL_CONV dynrec_not_dword(Bit32u op) {
	return ~op;
}

static Bit32u DRC_CALL_CONV dynrec_neg_dword(Bit32u op) DRC_FC;
static Bit32u DRC_CALL_CONV dynrec_neg_dword(Bit32u op) {
	lf_var1d=op;
	lf_resd=0-lf_var1d;
	lflags.type=t_NEGd;
	return lf_resd;
}

static Bit32u DRC_CALL_CONV dynrec_neg_dword_simple(Bit32u op) DRC_FC;
static Bit32u DRC_CALL_CONV dynrec_neg_dword_simple(Bit32u op) {
	return 0-op;
}


static void dyn_sop_byte_gencall(SingleOps op) {
	switch (op) {
		case SOP_INC:
			InvalidateFlagsPartially(dynrec_inc_byte_simple,t_INCb);
			gen_call_function_raw(dynrec_inc_byte);
			break;
		case SOP_DEC:
			InvalidateFlagsPartially(dynrec_dec_byte_simple,t_DECb);
			gen_call_function_raw(dynrec_dec_byte);
			break;
		case SOP_NOT:
			gen_call_function_raw(dynrec_not_byte);
			break;
		case SOP_NEG:
			InvalidateFlags(dynrec_neg_byte_simple,t_NEGb);
			gen_call_function_raw(dynrec_neg_byte);
			break;
		default: IllegalOptionDynrec("dyn_sop_byte_gencall");
	}
}

static void dyn_sop_word_gencall(SingleOps op,bool dword) {
	if (dword) {
		switch (op) {
			case SOP_INC:
				InvalidateFlagsPartially(dynrec_inc_dword_simple,t_INCd);
				gen_call_function_raw(dynrec_inc_dword);
				break;
			case SOP_DEC:
				InvalidateFlagsPartially(dynrec_dec_dword_simple,t_DECd);
				gen_call_function_raw(dynrec_dec_dword);
				break;
			case SOP_NOT:
				gen_call_function_raw(dynrec_not_dword);
				break;
			case SOP_NEG:
				InvalidateFlags(dynrec_neg_dword_simple,t_NEGd);
				gen_call_function_raw(dynrec_neg_dword);
				break;
			default: IllegalOptionDynrec("dyn_sop_dword_gencall");
		}
	} else {
		switch (op) {
			case SOP_INC:
				InvalidateFlagsPartially(dynrec_inc_word_simple,t_INCw);
				gen_call_function_raw(dynrec_inc_word);
				break;
			case SOP_DEC:
				InvalidateFlagsPartially(dynrec_dec_word_simple,t_DECw);
				gen_call_function_raw(dynrec_dec_word);
				break;
			case SOP_NOT:
				gen_call_function_raw(dynrec_not_word);
				break;
			case SOP_NEG:
				InvalidateFlags(dynrec_neg_word_simple,t_NEGw);
				gen_call_function_raw(dynrec_neg_word);
				break;
			default: IllegalOptionDynrec("dyn_sop_word_gencall");
		}
	}
}


static uint8_t DRC_CALL_CONV dynrec_rol_byte(uint8_t op1,uint8_t op2) DRC_FC;
static uint8_t DRC_CALL_CONV dynrec_rol_byte(uint8_t op1,uint8_t op2) {
	if (!(op2&0x7)) {
		if (op2&0x18) {
			FillFlagsNoCFOF();
			SETFLAGBIT(CF,op1 & 1);
			SETFLAGBIT(OF,(op1 & 1) ^ (op1 >> 7));
		}
		return op1;
	}
	FillFlagsNoCFOF();
	lf_var1b=op1;
	lf_var2b=op2&0x07;
	lf_resb=(lf_var1b << lf_var2b) | (lf_var1b >> (8-lf_var2b));
	SETFLAGBIT(CF,lf_resb & 1);
	SETFLAGBIT(OF,(lf_resb & 1) ^ (lf_resb >> 7));
	return lf_resb;
}

static uint8_t DRC_CALL_CONV dynrec_rol_byte_simple(uint8_t op1,uint8_t op2) DRC_FC;
static uint8_t DRC_CALL_CONV dynrec_rol_byte_simple(uint8_t op1,uint8_t op2) {
	if (!(op2&0x7)) return op1;
	return (op1 << (op2&0x07)) | (op1 >> (8-(op2&0x07)));
}

static uint8_t DRC_CALL_CONV dynrec_ror_byte(uint8_t op1,uint8_t op2) DRC_FC;
static uint8_t DRC_CALL_CONV dynrec_ror_byte(uint8_t op1,uint8_t op2) {
	if (!(op2&0x7)) {
		if (op2&0x18) {
			FillFlagsNoCFOF();
			SETFLAGBIT(CF,op1>>7);
			SETFLAGBIT(OF,(op1>>7) ^ ((op1>>6) & 1));
		}
		return op1;
	}
	FillFlagsNoCFOF();
	lf_var1b=op1;
	lf_var2b=op2&0x07;
	lf_resb=(lf_var1b >> lf_var2b) | (lf_var1b << (8-lf_var2b));
	SETFLAGBIT(CF,lf_resb & 0x80);
	SETFLAGBIT(OF,(lf_resb ^ (lf_resb<<1)) & 0x80);
	return lf_resb;
}

static uint8_t DRC_CALL_CONV dynrec_ror_byte_simple(uint8_t op1,uint8_t op2) DRC_FC;
static uint8_t DRC_CALL_CONV dynrec_ror_byte_simple(uint8_t op1,uint8_t op2) {
	if (!(op2&0x7)) return op1;
	return (op1 >> (op2&0x07)) | (op1 << (8-(op2&0x07)));
}

static uint8_t DRC_CALL_CONV dynrec_rcl_byte(uint8_t op1,uint8_t op2) DRC_FC;
static uint8_t DRC_CALL_CONV dynrec_rcl_byte(uint8_t op1,uint8_t op2) {
	if (op2%9) {
		uint8_t cf=(uint8_t)FillFlags()&0x1;
		lf_var1b=op1;
		lf_var2b=op2%9;
		lf_resb=(lf_var1b << lf_var2b) | (cf << (lf_var2b-1)) | (lf_var1b >> (9-lf_var2b));
		SETFLAGBIT(CF,((lf_var1b >> (8-lf_var2b)) & 1));
		SETFLAGBIT(OF,(reg_flags & 1) ^ (lf_resb >> 7));
		return lf_resb;
	} else return op1;
}

static uint8_t DRC_CALL_CONV dynrec_rcr_byte(uint8_t op1,uint8_t op2) DRC_FC;
static uint8_t DRC_CALL_CONV dynrec_rcr_byte(uint8_t op1,uint8_t op2) {
	if (op2%9) {
		uint8_t cf=(uint8_t)FillFlags()&0x1;
		lf_var1b=op1;
		lf_var2b=op2%9;
	 	lf_resb=(lf_var1b >> lf_var2b) | (cf << (8-lf_var2b)) | (lf_var1b << (9-lf_var2b));					\
		SETFLAGBIT(CF,(lf_var1b >> (lf_var2b - 1)) & 1);
		SETFLAGBIT(OF,(lf_resb ^ (lf_resb<<1)) & 0x80);
		return lf_resb;
	} else return op1;
}

static uint8_t DRC_CALL_CONV dynrec_shl_byte(uint8_t op1,uint8_t op2) DRC_FC;
static uint8_t DRC_CALL_CONV dynrec_shl_byte(uint8_t op1,uint8_t op2) {
	if (!op2) return op1;
	lf_var1b=op1;
	lf_var2b=op2;
	lf_resb=lf_var1b << lf_var2b;
	lflags.type=t_SHLb;
	return lf_resb;
}

static uint8_t DRC_CALL_CONV dynrec_shl_byte_simple(uint8_t op1,uint8_t op2) DRC_FC;
static uint8_t DRC_CALL_CONV dynrec_shl_byte_simple(uint8_t op1,uint8_t op2) {
	if (!op2) return op1;
	return op1 << op2;
}

static uint8_t DRC_CALL_CONV dynrec_shr_byte(uint8_t op1,uint8_t op2) DRC_FC;
static uint8_t DRC_CALL_CONV dynrec_shr_byte(uint8_t op1,uint8_t op2) {
	if (!op2) return op1;
	lf_var1b=op1;
	lf_var2b=op2;
	lf_resb=lf_var1b >> lf_var2b;
	lflags.type=t_SHRb;
	return lf_resb;
}

static uint8_t DRC_CALL_CONV dynrec_shr_byte_simple(uint8_t op1,uint8_t op2) DRC_FC;
static uint8_t DRC_CALL_CONV dynrec_shr_byte_simple(uint8_t op1,uint8_t op2) {
	if (!op2) return op1;
	return op1 >> op2;
}

static uint8_t DRC_CALL_CONV dynrec_sar_byte(uint8_t op1,uint8_t op2) DRC_FC;
static uint8_t DRC_CALL_CONV dynrec_sar_byte(uint8_t op1,uint8_t op2) {
	if (!op2) return op1;
	lf_var1b=op1;
	lf_var2b=op2;
	if (lf_var2b>8) lf_var2b=8;
    if (lf_var1b & 0x80) {
		lf_resb=(lf_var1b >> lf_var2b)| (0xff << (8 - lf_var2b));
	} else {
		lf_resb=lf_var1b >> lf_var2b;
    }
	lflags.type=t_SARb;
	return lf_resb;
}

static uint8_t DRC_CALL_CONV dynrec_sar_byte_simple(uint8_t op1,uint8_t op2) DRC_FC;
static uint8_t DRC_CALL_CONV dynrec_sar_byte_simple(uint8_t op1,uint8_t op2) {
	if (!op2) return op1;
	if (op2>8) op2=8;
    if (op1 & 0x80) return (op1 >> op2) | (0xff << (8 - op2));
	else return op1 >> op2;
}

static Bit16u DRC_CALL_CONV dynrec_rol_word(Bit16u op1,uint8_t op2) DRC_FC;
static Bit16u DRC_CALL_CONV dynrec_rol_word(Bit16u op1,uint8_t op2) {
	if (!(op2&0xf)) {
		if (op2&0x10) {
			FillFlagsNoCFOF();
			SETFLAGBIT(CF,op1 & 1);
			SETFLAGBIT(OF,(op1 & 1) ^ (op1 >> 15));
		}
		return op1;
	}
	FillFlagsNoCFOF();
	lf_var1w=op1;
	lf_var2b=op2&0xf;
	lf_resw=(lf_var1w << lf_var2b) | (lf_var1w >> (16-lf_var2b));
	SETFLAGBIT(CF,lf_resw & 1);
	SETFLAGBIT(OF,(lf_resw & 1) ^ (lf_resw >> 15));
	return lf_resw;
}

static Bit16u DRC_CALL_CONV dynrec_rol_word_simple(Bit16u op1,uint8_t op2) DRC_FC;
static Bit16u DRC_CALL_CONV dynrec_rol_word_simple(Bit16u op1,uint8_t op2) {
	if (!(op2&0xf)) return op1;
	return (op1 << (op2&0xf)) | (op1 >> (16-(op2&0xf)));
}

static Bit16u DRC_CALL_CONV dynrec_ror_word(Bit16u op1,uint8_t op2) DRC_FC;
static Bit16u DRC_CALL_CONV dynrec_ror_word(Bit16u op1,uint8_t op2) {
	if (!(op2&0xf)) {
		if (op2&0x10) {
			FillFlagsNoCFOF();
			SETFLAGBIT(CF,op1>>15);
			SETFLAGBIT(OF,(op1>>15) ^ ((op1>>14) & 1));
		}
		return op1;
	}
	FillFlagsNoCFOF();
	lf_var1w=op1;
	lf_var2b=op2&0xf;
	lf_resw=(lf_var1w >> lf_var2b) | (lf_var1w << (16-lf_var2b));
	SETFLAGBIT(CF,lf_resw & 0x8000);
	SETFLAGBIT(OF,(lf_resw ^ (lf_resw<<1)) & 0x8000);
	return lf_resw;
}

static Bit16u DRC_CALL_CONV dynrec_ror_word_simple(Bit16u op1,uint8_t op2) DRC_FC;
static Bit16u DRC_CALL_CONV dynrec_ror_word_simple(Bit16u op1,uint8_t op2) {
	if (!(op2&0xf)) return op1;
	return (op1 >> (op2&0xf)) | (op1 << (16-(op2&0xf)));
}

static Bit16u DRC_CALL_CONV dynrec_rcl_word(Bit16u op1,uint8_t op2) DRC_FC;
static Bit16u DRC_CALL_CONV dynrec_rcl_word(Bit16u op1,uint8_t op2) {
	if (op2%17) {
		Bit16u cf=(Bit16u)FillFlags()&0x1;
		lf_var1w=op1;
		lf_var2b=op2%17;
		lf_resw=(lf_var1w << lf_var2b) | (cf << (lf_var2b-1)) | (lf_var1w >> (17-lf_var2b));
		SETFLAGBIT(CF,((lf_var1w >> (16-lf_var2b)) & 1));
		SETFLAGBIT(OF,(reg_flags & 1) ^ (lf_resw >> 15));
		return lf_resw;
	} else return op1;
}

static Bit16u DRC_CALL_CONV dynrec_rcr_word(Bit16u op1,uint8_t op2) DRC_FC;
static Bit16u DRC_CALL_CONV dynrec_rcr_word(Bit16u op1,uint8_t op2) {
	if (op2%17) {
		Bit16u cf=(Bit16u)FillFlags()&0x1;
		lf_var1w=op1;
		lf_var2b=op2%17;
	 	lf_resw=(lf_var1w >> lf_var2b) | (cf << (16-lf_var2b)) | (lf_var1w << (17-lf_var2b));
		SETFLAGBIT(CF,(lf_var1w >> (lf_var2b - 1)) & 1);
		SETFLAGBIT(OF,(lf_resw ^ (lf_resw<<1)) & 0x8000);
		return lf_resw;
	} else return op1;
}

static Bit16u DRC_CALL_CONV dynrec_shl_word(Bit16u op1,uint8_t op2) DRC_FC;
static Bit16u DRC_CALL_CONV dynrec_shl_word(Bit16u op1,uint8_t op2) {
	if (!op2) return op1;
	lf_var1w=op1;
	lf_var2b=op2;
	lf_resw=lf_var1w << lf_var2b;
	lflags.type=t_SHLw;
	return lf_resw;
}

static Bit16u DRC_CALL_CONV dynrec_shl_word_simple(Bit16u op1,uint8_t op2) DRC_FC;
static Bit16u DRC_CALL_CONV dynrec_shl_word_simple(Bit16u op1,uint8_t op2) {
	if (!op2) return op1;
	return op1 << op2;
}

static Bit16u DRC_CALL_CONV dynrec_shr_word(Bit16u op1,uint8_t op2) DRC_FC;
static Bit16u DRC_CALL_CONV dynrec_shr_word(Bit16u op1,uint8_t op2) {
	if (!op2) return op1;
	lf_var1w=op1;
	lf_var2b=op2;
	lf_resw=lf_var1w >> lf_var2b;
	lflags.type=t_SHRw;
	return lf_resw;
}

static Bit16u DRC_CALL_CONV dynrec_shr_word_simple(Bit16u op1,uint8_t op2) DRC_FC;
static Bit16u DRC_CALL_CONV dynrec_shr_word_simple(Bit16u op1,uint8_t op2) {
	if (!op2) return op1;
	return op1 >> op2;
}

static Bit16u DRC_CALL_CONV dynrec_sar_word(Bit16u op1,uint8_t op2) DRC_FC;
static Bit16u DRC_CALL_CONV dynrec_sar_word(Bit16u op1,uint8_t op2) {
	if (!op2) return op1;
	lf_var1w=op1;
	lf_var2b=op2;
	if (lf_var2b>16) lf_var2b=16;
	if (lf_var1w & 0x8000) {
		lf_resw=(lf_var1w >> lf_var2b) | (0xffff << (16 - lf_var2b));
	} else {
		lf_resw=lf_var1w >> lf_var2b;
    }
	lflags.type=t_SARw;
	return lf_resw;
}

static Bit16u DRC_CALL_CONV dynrec_sar_word_simple(Bit16u op1,uint8_t op2) DRC_FC;
static Bit16u DRC_CALL_CONV dynrec_sar_word_simple(Bit16u op1,uint8_t op2) {
	if (!op2) return op1;
	if (op2>16) op2=16;
	if (op1 & 0x8000) return (op1 >> op2) | (0xffff << (16 - op2));
	else return op1 >> op2;
}

static Bit32u DRC_CALL_CONV dynrec_rol_dword(Bit32u op1,uint8_t op2) DRC_FC;
static Bit32u DRC_CALL_CONV dynrec_rol_dword(Bit32u op1,uint8_t op2) {
	if (!op2) return op1;
	FillFlagsNoCFOF();
	lf_var1d=op1;
	lf_var2b=op2;
	lf_resd=(lf_var1d << lf_var2b) | (lf_var1d >> (32-lf_var2b));
	SETFLAGBIT(CF,lf_resd & 1);
	SETFLAGBIT(OF,(lf_resd & 1) ^ (lf_resd >> 31));
	return lf_resd;
}

static Bit32u DRC_CALL_CONV dynrec_rol_dword_simple(Bit32u op1,uint8_t op2) DRC_FC;
static Bit32u DRC_CALL_CONV dynrec_rol_dword_simple(Bit32u op1,uint8_t op2) {
	if (!op2) return op1;
	return (op1 << op2) | (op1 >> (32-op2));
}

static Bit32u DRC_CALL_CONV dynrec_ror_dword(Bit32u op1,uint8_t op2) DRC_FC;
static Bit32u DRC_CALL_CONV dynrec_ror_dword(Bit32u op1,uint8_t op2) {
	if (!op2) return op1;
	FillFlagsNoCFOF();
	lf_var1d=op1;
	lf_var2b=op2;
	lf_resd=(lf_var1d >> lf_var2b) | (lf_var1d << (32-lf_var2b));
	SETFLAGBIT(CF,lf_resd & 0x80000000);
	SETFLAGBIT(OF,(lf_resd ^ (lf_resd<<1)) & 0x80000000);
	return lf_resd;
}

static Bit32u DRC_CALL_CONV dynrec_ror_dword_simple(Bit32u op1,uint8_t op2) DRC_FC;
static Bit32u DRC_CALL_CONV dynrec_ror_dword_simple(Bit32u op1,uint8_t op2) {
	if (!op2) return op1;
	return (op1 >> op2) | (op1 << (32-op2));
}

static Bit32u DRC_CALL_CONV dynrec_rcl_dword(Bit32u op1,uint8_t op2) DRC_FC;
static Bit32u DRC_CALL_CONV dynrec_rcl_dword(Bit32u op1,uint8_t op2) {
	if (!op2) return op1;
	Bit32u cf=(Bit32u)FillFlags()&0x1;
	lf_var1d=op1;
	lf_var2b=op2;
	if (lf_var2b==1) {
		lf_resd=(lf_var1d << 1) | cf;
	} else 	{
		lf_resd=(lf_var1d << lf_var2b) | (cf << (lf_var2b-1)) | (lf_var1d >> (33-lf_var2b));
	}
	SETFLAGBIT(CF,((lf_var1d >> (32-lf_var2b)) & 1));
	SETFLAGBIT(OF,(reg_flags & 1) ^ (lf_resd >> 31));
	return lf_resd;
}

static Bit32u DRC_CALL_CONV dynrec_rcr_dword(Bit32u op1,uint8_t op2) DRC_FC;
static Bit32u DRC_CALL_CONV dynrec_rcr_dword(Bit32u op1,uint8_t op2) {
	if (op2) {
		Bit32u cf=(Bit32u)FillFlags()&0x1;
		lf_var1d=op1;
		lf_var2b=op2;
		if (lf_var2b==1) {
			lf_resd=lf_var1d >> 1 | cf << 31;
		} else {
 			lf_resd=(lf_var1d >> lf_var2b) | (cf << (32-lf_var2b)) | (lf_var1d << (33-lf_var2b));
		}
		SETFLAGBIT(CF,(lf_var1d >> (lf_var2b - 1)) & 1);
		SETFLAGBIT(OF,(lf_resd ^ (lf_resd<<1)) & 0x80000000);
		return lf_resd;
	} else return op1;
}

static Bit32u DRC_CALL_CONV dynrec_shl_dword(Bit32u op1,uint8_t op2) DRC_FC;
static Bit32u DRC_CALL_CONV dynrec_shl_dword(Bit32u op1,uint8_t op2) {
	if (!op2) return op1;
	lf_var1d=op1;
	lf_var2b=op2;
	lf_resd=lf_var1d << lf_var2b;
	lflags.type=t_SHLd;
	return lf_resd;
}

static Bit32u DRC_CALL_CONV dynrec_shl_dword_simple(Bit32u op1,uint8_t op2) DRC_FC;
static Bit32u DRC_CALL_CONV dynrec_shl_dword_simple(Bit32u op1,uint8_t op2) {
	if (!op2) return op1;
	return op1 << op2;
}

static Bit32u DRC_CALL_CONV dynrec_shr_dword(Bit32u op1,uint8_t op2) DRC_FC;
static Bit32u DRC_CALL_CONV dynrec_shr_dword(Bit32u op1,uint8_t op2) {
	if (!op2) return op1;
	lf_var1d=op1;
	lf_var2b=op2;
	lf_resd=lf_var1d >> lf_var2b;
	lflags.type=t_SHRd;
	return lf_resd;
}

static Bit32u DRC_CALL_CONV dynrec_shr_dword_simple(Bit32u op1,uint8_t op2) DRC_FC;
static Bit32u DRC_CALL_CONV dynrec_shr_dword_simple(Bit32u op1,uint8_t op2) {
	if (!op2) return op1;
	return op1 >> op2;
}

static Bit32u DRC_CALL_CONV dynrec_sar_dword(Bit32u op1,uint8_t op2) DRC_FC;
static Bit32u DRC_CALL_CONV dynrec_sar_dword(Bit32u op1,uint8_t op2) {
	if (!op2) return op1;
	lf_var2b=op2;
	lf_var1d=op1;
	if (lf_var1d & 0x80000000) {
		lf_resd=(lf_var1d >> lf_var2b) | (0xffffffff << (32 - lf_var2b));
	} else {
		lf_resd=lf_var1d >> lf_var2b;
    }
	lflags.type=t_SARd;
	return lf_resd;
}

static Bit32u DRC_CALL_CONV dynrec_sar_dword_simple(Bit32u op1,uint8_t op2) DRC_FC;
static Bit32u DRC_CALL_CONV dynrec_sar_dword_simple(Bit32u op1,uint8_t op2) {
	if (!op2) return op1;
	if (op1 & 0x80000000) return (op1 >> op2) | (0xffffffff << (32 - op2));
	else return op1 >> op2;
}

static void dyn_shift_byte_gencall(ShiftOps op) {
	switch (op) {
		case SHIFT_ROL:
			InvalidateFlagsPartially(dynrec_rol_byte_simple,t_ROLb);
			gen_call_function_raw(dynrec_rol_byte);
			break;
		case SHIFT_ROR:
			InvalidateFlagsPartially(dynrec_ror_byte_simple,t_RORb);
			gen_call_function_raw(dynrec_ror_byte);
			break;
		case SHIFT_RCL:
			AcquireFlags(FLAG_CF);
			gen_call_function_raw(dynrec_rcl_byte);
			break;
		case SHIFT_RCR:
			AcquireFlags(FLAG_CF);
			gen_call_function_raw(dynrec_rcr_byte);
			break;
		case SHIFT_SHL:
		case SHIFT_SAL:
			InvalidateFlagsPartially(dynrec_shl_byte_simple,t_SHLb);
			gen_call_function_raw(dynrec_shl_byte);
			break;
		case SHIFT_SHR:
			InvalidateFlagsPartially(dynrec_shr_byte_simple,t_SHRb);
			gen_call_function_raw(dynrec_shr_byte);
			break;
		case SHIFT_SAR:
			InvalidateFlagsPartially(dynrec_sar_byte_simple,t_SARb);
			gen_call_function_raw(dynrec_sar_byte);
			break;
		default: IllegalOptionDynrec("dyn_shift_byte_gencall");
	}
}

static void dyn_shift_word_gencall(ShiftOps op,bool dword) {
	if (dword) {
		switch (op) {
			case SHIFT_ROL:
				InvalidateFlagsPartially(dynrec_rol_dword_simple,t_ROLd);
				gen_call_function_raw(dynrec_rol_dword);
				break;
			case SHIFT_ROR:
				InvalidateFlagsPartially(dynrec_ror_dword_simple,t_RORd);
				gen_call_function_raw(dynrec_ror_dword);
				break;
			case SHIFT_RCL:
				AcquireFlags(FLAG_CF);
				gen_call_function_raw(dynrec_rcl_dword);
				break;
			case SHIFT_RCR:
				AcquireFlags(FLAG_CF);
				gen_call_function_raw(dynrec_rcr_dword);
				break;
			case SHIFT_SHL:
			case SHIFT_SAL:
				InvalidateFlagsPartially(dynrec_shl_dword_simple,t_SHLd);
				gen_call_function_raw(dynrec_shl_dword);
				break;
			case SHIFT_SHR:
				InvalidateFlagsPartially(dynrec_shr_dword_simple,t_SHRd);
				gen_call_function_raw(dynrec_shr_dword);
				break;
			case SHIFT_SAR:
				InvalidateFlagsPartially(dynrec_sar_dword_simple,t_SARd);
				gen_call_function_raw(dynrec_sar_dword);
				break;
			default: IllegalOptionDynrec("dyn_shift_dword_gencall");
		}
	} else {
		switch (op) {
			case SHIFT_ROL:
				InvalidateFlagsPartially(dynrec_rol_word_simple,t_ROLw);
				gen_call_function_raw(dynrec_rol_word);
				break;
			case SHIFT_ROR:
				InvalidateFlagsPartially(dynrec_ror_word_simple,t_RORw);
				gen_call_function_raw(dynrec_ror_word);
				break;
			case SHIFT_RCL:
				AcquireFlags(FLAG_CF);
				gen_call_function_raw(dynrec_rcl_word);
				break;
			case SHIFT_RCR:
				AcquireFlags(FLAG_CF);
				gen_call_function_raw(dynrec_rcr_word);
				break;
			case SHIFT_SHL:
			case SHIFT_SAL:
				InvalidateFlagsPartially(dynrec_shl_word_simple,t_SHLw);
				gen_call_function_raw(dynrec_shl_word);
				break;
			case SHIFT_SHR:
				InvalidateFlagsPartially(dynrec_shr_word_simple,t_SHRw);
				gen_call_function_raw(dynrec_shr_word);
				break;
			case SHIFT_SAR:
				InvalidateFlagsPartially(dynrec_sar_word_simple,t_SARw);
				gen_call_function_raw(dynrec_sar_word);
				break;
			default: IllegalOptionDynrec("dyn_shift_word_gencall");
		}
	}
}

static Bit16u DRC_CALL_CONV dynrec_dshl_word(Bit16u op1,Bit16u op2,uint8_t op3) DRC_FC;
static Bit16u DRC_CALL_CONV dynrec_dshl_word(Bit16u op1,Bit16u op2,uint8_t op3) {
	uint8_t val=op3 & 0x1f;
	if (!val) return op1;
	lf_var2b=val;
	lf_var1d=(op1<<16)|op2;
	Bit32u tempd=lf_var1d << lf_var2b;
  	if (lf_var2b>16) tempd |= (op2 << (lf_var2b - 16));
	lf_resw=(Bit16u)(tempd >> 16);
	lflags.type=t_DSHLw;
	return lf_resw;
}

static Bit16u DRC_CALL_CONV dynrec_dshl_word_simple(Bit16u op1,Bit16u op2,uint8_t op3) DRC_FC;
static Bit16u DRC_CALL_CONV dynrec_dshl_word_simple(Bit16u op1,Bit16u op2,uint8_t op3) {
	uint8_t val=op3 & 0x1f;
	if (!val) return op1;
	Bit32u tempd=(Bit32u)((((Bit32u)op1)<<16)|op2) << val;
  	if (val>16) tempd |= (op2 << (val - 16));
	return (Bit16u)(tempd >> 16);
}

static Bit32u DRC_CALL_CONV dynrec_dshl_dword(Bit32u op1,Bit32u op2,uint8_t op3) DRC_FC;
static Bit32u DRC_CALL_CONV dynrec_dshl_dword(Bit32u op1,Bit32u op2,uint8_t op3) {
	uint8_t val=op3 & 0x1f;
	if (!val) return op1;
	lf_var2b=val;
	lf_var1d=op1;
	lf_resd=(lf_var1d << lf_var2b) | (op2 >> (32-lf_var2b));
	lflags.type=t_DSHLd;
	return lf_resd;
}

static Bit32u DRC_CALL_CONV dynrec_dshl_dword_simple(Bit32u op1,Bit32u op2,uint8_t op3) DRC_FC;
static Bit32u DRC_CALL_CONV dynrec_dshl_dword_simple(Bit32u op1,Bit32u op2,uint8_t op3) {
	uint8_t val=op3 & 0x1f;
	if (!val) return op1;
	return (op1 << val) | (op2 >> (32-val));
}

static Bit16u DRC_CALL_CONV dynrec_dshr_word(Bit16u op1,Bit16u op2,uint8_t op3) DRC_FC;
static Bit16u DRC_CALL_CONV dynrec_dshr_word(Bit16u op1,Bit16u op2,uint8_t op3) {
	uint8_t val=op3 & 0x1f;
	if (!val) return op1;
	lf_var2b=val;
	lf_var1d=(op2<<16)|op1;
	Bit32u tempd=lf_var1d >> lf_var2b;
  	if (lf_var2b>16) tempd |= (op2 << (32-lf_var2b ));
	lf_resw=(Bit16u)(tempd);
	lflags.type=t_DSHRw;
	return lf_resw;
}

static Bit16u DRC_CALL_CONV dynrec_dshr_word_simple(Bit16u op1,Bit16u op2,uint8_t op3) DRC_FC;
static Bit16u DRC_CALL_CONV dynrec_dshr_word_simple(Bit16u op1,Bit16u op2,uint8_t op3) {
	uint8_t val=op3 & 0x1f;
	if (!val) return op1;
	Bit32u tempd=(Bit32u)((((Bit32u)op2)<<16)|op1) >> val;
  	if (val>16) tempd |= (op2 << (32-val));
	return (Bit16u)(tempd);
}

static Bit32u DRC_CALL_CONV dynrec_dshr_dword(Bit32u op1,Bit32u op2,uint8_t op3) DRC_FC;
static Bit32u DRC_CALL_CONV dynrec_dshr_dword(Bit32u op1,Bit32u op2,uint8_t op3) {
	uint8_t val=op3 & 0x1f;
	if (!val) return op1;
	lf_var2b=val;
	lf_var1d=op1;
	lf_resd=(lf_var1d >> lf_var2b) | (op2 << (32-lf_var2b));
	lflags.type=t_DSHRd;
	return lf_resd;
}

static Bit32u DRC_CALL_CONV dynrec_dshr_dword_simple(Bit32u op1,Bit32u op2,uint8_t op3) DRC_FC;
static Bit32u DRC_CALL_CONV dynrec_dshr_dword_simple(Bit32u op1,Bit32u op2,uint8_t op3) {
	uint8_t val=op3 & 0x1f;
	if (!val) return op1;
	return (op1 >> val) | (op2 << (32-val));
}

static void dyn_dpshift_word_gencall(bool left) {
	if (left) {
		DRC_PTR_SIZE_IM proc_addr=gen_call_function_R3(dynrec_dshl_word,FC_OP3);
		InvalidateFlagsPartially(dynrec_dshl_word_simple,proc_addr,t_DSHLw);
	} else {
		DRC_PTR_SIZE_IM proc_addr=gen_call_function_R3(dynrec_dshr_word,FC_OP3);
		InvalidateFlagsPartially(dynrec_dshr_word_simple,proc_addr,t_DSHRw);
	}
}

static void dyn_dpshift_dword_gencall(bool left) {
	if (left) {
		DRC_PTR_SIZE_IM proc_addr=gen_call_function_R3(dynrec_dshl_dword,FC_OP3);
		InvalidateFlagsPartially(dynrec_dshl_dword_simple,proc_addr,t_DSHLd);
	} else {
		DRC_PTR_SIZE_IM proc_addr=gen_call_function_R3(dynrec_dshr_dword,FC_OP3);
		InvalidateFlagsPartially(dynrec_dshr_dword_simple,proc_addr,t_DSHRd);
	}
}



static Bit32u DRC_CALL_CONV dynrec_get_of(void)		DRC_FC;
static Bit32u DRC_CALL_CONV dynrec_get_of(void)		{ return TFLG_O; }
static Bit32u DRC_CALL_CONV dynrec_get_nof(void)	DRC_FC;
static Bit32u DRC_CALL_CONV dynrec_get_nof(void)	{ return TFLG_NO; }
static Bit32u DRC_CALL_CONV dynrec_get_cf(void)		DRC_FC;
static Bit32u DRC_CALL_CONV dynrec_get_cf(void)		{ return TFLG_B; }
static Bit32u DRC_CALL_CONV dynrec_get_ncf(void)	DRC_FC;
static Bit32u DRC_CALL_CONV dynrec_get_ncf(void)	{ return TFLG_NB; }
static Bit32u DRC_CALL_CONV dynrec_get_zf(void)		DRC_FC;
static Bit32u DRC_CALL_CONV dynrec_get_zf(void)		{ return TFLG_Z; }
static Bit32u DRC_CALL_CONV dynrec_get_nzf(void)	DRC_FC;
static Bit32u DRC_CALL_CONV dynrec_get_nzf(void)	{ return TFLG_NZ; }
static Bit32u DRC_CALL_CONV dynrec_get_sf(void)		DRC_FC;
static Bit32u DRC_CALL_CONV dynrec_get_sf(void)		{ return TFLG_S; }
static Bit32u DRC_CALL_CONV dynrec_get_nsf(void)	DRC_FC;
static Bit32u DRC_CALL_CONV dynrec_get_nsf(void)	{ return TFLG_NS; }
static Bit32u DRC_CALL_CONV dynrec_get_pf(void)		DRC_FC;
static Bit32u DRC_CALL_CONV dynrec_get_pf(void)		{ return TFLG_P; }
static Bit32u DRC_CALL_CONV dynrec_get_npf(void)	DRC_FC;
static Bit32u DRC_CALL_CONV dynrec_get_npf(void)	{ return TFLG_NP; }

static Bit32u DRC_CALL_CONV dynrec_get_cf_or_zf(void)			DRC_FC;
static Bit32u DRC_CALL_CONV dynrec_get_cf_or_zf(void)			{ return TFLG_BE; }
static Bit32u DRC_CALL_CONV dynrec_get_ncf_and_nzf(void)		DRC_FC;
static Bit32u DRC_CALL_CONV dynrec_get_ncf_and_nzf(void)		{ return TFLG_NBE; }
static Bit32u DRC_CALL_CONV dynrec_get_sf_neq_of(void)			DRC_FC;
static Bit32u DRC_CALL_CONV dynrec_get_sf_neq_of(void)			{ return TFLG_L; }
static Bit32u DRC_CALL_CONV dynrec_get_sf_eq_of(void)			DRC_FC;
static Bit32u DRC_CALL_CONV dynrec_get_sf_eq_of(void)			{ return TFLG_NL; }
static Bit32u DRC_CALL_CONV dynrec_get_zf_or_sf_neq_of(void)	DRC_FC;
static Bit32u DRC_CALL_CONV dynrec_get_zf_or_sf_neq_of(void)	{ return TFLG_LE; }
static Bit32u DRC_CALL_CONV dynrec_get_nzf_and_sf_eq_of(void)	DRC_FC;
static Bit32u DRC_CALL_CONV dynrec_get_nzf_and_sf_eq_of(void)	{ return TFLG_NLE; }


static void dyn_branchflag_to_reg(BranchTypes btype) {
	switch (btype) {
		case BR_O:gen_call_function_raw(dynrec_get_of);break;
		case BR_NO:gen_call_function_raw(dynrec_get_nof);break;
		case BR_B:gen_call_function_raw(dynrec_get_cf);break;
		case BR_NB:gen_call_function_raw(dynrec_get_ncf);break;
		case BR_Z:gen_call_function_raw(dynrec_get_zf);break;
		case BR_NZ:gen_call_function_raw(dynrec_get_nzf);break;
		case BR_BE:gen_call_function_raw(dynrec_get_cf_or_zf);break;
		case BR_NBE:gen_call_function_raw(dynrec_get_ncf_and_nzf);break;

		case BR_S:gen_call_function_raw(dynrec_get_sf);break;
		case BR_NS:gen_call_function_raw(dynrec_get_nsf);break;
		case BR_P:gen_call_function_raw(dynrec_get_pf);break;
		case BR_NP:gen_call_function_raw(dynrec_get_npf);break;
		case BR_L:gen_call_function_raw(dynrec_get_sf_neq_of);break;
		case BR_NL:gen_call_function_raw(dynrec_get_sf_eq_of);break;
		case BR_LE:gen_call_function_raw(dynrec_get_zf_or_sf_neq_of);break;
		case BR_NLE:gen_call_function_raw(dynrec_get_nzf_and_sf_eq_of);break;
	}
}


static void DRC_CALL_CONV dynrec_mul_byte(uint8_t op) DRC_FC;
static void DRC_CALL_CONV dynrec_mul_byte(uint8_t op) {
	FillFlagsNoCFOF();
	reg_ax=reg_al*op;
	SETFLAGBIT(ZF,reg_al == 0);
	if (reg_ax & 0xff00) {
		SETFLAGBIT(CF,true);
		SETFLAGBIT(OF,true);
	} else {
		SETFLAGBIT(CF,false);
		SETFLAGBIT(OF,false);
	}
}

static void DRC_CALL_CONV dynrec_imul_byte(uint8_t op) DRC_FC;
static void DRC_CALL_CONV dynrec_imul_byte(uint8_t op) {
	FillFlagsNoCFOF();
	reg_ax=((int8_t)reg_al) * ((int8_t)op);
	if ((reg_ax & 0xff80)==0xff80 || (reg_ax & 0xff80)==0x0000) {
		SETFLAGBIT(CF,false);
		SETFLAGBIT(OF,false);
	} else {
		SETFLAGBIT(CF,true);
		SETFLAGBIT(OF,true);
	}
}

static void DRC_CALL_CONV dynrec_mul_word(Bit16u op) DRC_FC;
static void DRC_CALL_CONV dynrec_mul_word(Bit16u op) {
	FillFlagsNoCFOF();
	Bitu tempu=(Bitu)reg_ax*(Bitu)op;
	reg_ax=(Bit16u)(tempu);
	reg_dx=(Bit16u)(tempu >> 16);
	SETFLAGBIT(ZF,reg_ax == 0);
	if (reg_dx) {
		SETFLAGBIT(CF,true);
		SETFLAGBIT(OF,true);
	} else {
		SETFLAGBIT(CF,false);
		SETFLAGBIT(OF,false);
	}
}

static void DRC_CALL_CONV dynrec_imul_word(Bit16u op) DRC_FC;
static void DRC_CALL_CONV dynrec_imul_word(Bit16u op) {
	FillFlagsNoCFOF();
	Bits temps=Bits((Bit16s)reg_ax)*Bits((Bit16s)op);
	reg_ax=(Bit16s)(temps);
	reg_dx=(Bit16s)(temps >> 16);
	if (((temps & 0xffff8000)==0xffff8000 || (temps & 0xffff8000)==0x0000)) {
		SETFLAGBIT(CF,false);
		SETFLAGBIT(OF,false);
	} else {
		SETFLAGBIT(CF,true);
		SETFLAGBIT(OF,true);
	}
}

static void DRC_CALL_CONV dynrec_mul_dword(Bit32u op) DRC_FC;
static void DRC_CALL_CONV dynrec_mul_dword(Bit32u op) {
	FillFlagsNoCFOF();
	Bit64u tempu=(Bit64u)reg_eax*(Bit64u)op;
	reg_eax=(Bit32u)(tempu);
	reg_edx=(Bit32u)(tempu >> 32);
	SETFLAGBIT(ZF,reg_eax == 0);
	if (reg_edx) {
		SETFLAGBIT(CF,true);
		SETFLAGBIT(OF,true);
	} else {
		SETFLAGBIT(CF,false);
		SETFLAGBIT(OF,false);
	}
}

static void DRC_CALL_CONV dynrec_imul_dword(Bit32u op) DRC_FC;
static void DRC_CALL_CONV dynrec_imul_dword(Bit32u op) {
	FillFlagsNoCFOF();
	Bit64s temps=((Bit64s)((Bit32s)reg_eax))*((Bit64s)((Bit32s)op));
	reg_eax=(Bit32u)(temps);
	reg_edx=(Bit32u)(temps >> 32);
	if ((reg_edx==0xffffffff) && (reg_eax & 0x80000000) ) {
		SETFLAGBIT(CF,false);
		SETFLAGBIT(OF,false);
	} else if ( (reg_edx==0x00000000) && (reg_eax< 0x80000000) ) {
		SETFLAGBIT(CF,false);
		SETFLAGBIT(OF,false);
	} else {
		SETFLAGBIT(CF,true);
		SETFLAGBIT(OF,true);
	}
}


static bool DRC_CALL_CONV dynrec_div_byte(uint8_t op) DRC_FC;
static bool DRC_CALL_CONV dynrec_div_byte(uint8_t op) {
	Bitu val=op;
	if (val==0) return CPU_PrepareException(0,0);
	Bitu quo=reg_ax / val;
	uint8_t rem=(uint8_t)(reg_ax % val);
	uint8_t quo8=(uint8_t)(quo&0xff);
	if (quo>0xff) return CPU_PrepareException(0,0);
	reg_ah=rem;
	reg_al=quo8;
	return false;
}

static bool DRC_CALL_CONV dynrec_idiv_byte(uint8_t op) DRC_FC;
static bool DRC_CALL_CONV dynrec_idiv_byte(uint8_t op) {
	Bits val=(int8_t)op;
	if (val==0) return CPU_PrepareException(0,0);
	Bits quo=((Bit16s)reg_ax) / val;
	int8_t rem=(int8_t)((Bit16s)reg_ax % val);
	int8_t quo8s=(int8_t)(quo&0xff);
	if (quo!=(Bit16s)quo8s) return CPU_PrepareException(0,0);
	reg_ah=rem;
	reg_al=quo8s;
	return false;
}

static bool DRC_CALL_CONV dynrec_div_word(Bit16u op) DRC_FC;
static bool DRC_CALL_CONV dynrec_div_word(Bit16u op) {
	Bitu val=op;
	if (val==0)	return CPU_PrepareException(0,0);
	Bitu num=((Bit32u)reg_dx<<16)|reg_ax;
	Bitu quo=num/val;
	Bit16u rem=(Bit16u)(num % val);
	Bit16u quo16=(Bit16u)(quo&0xffff);
	if (quo!=(Bit32u)quo16) return CPU_PrepareException(0,0);
	reg_dx=rem;
	reg_ax=quo16;
	return false;
}

static bool DRC_CALL_CONV dynrec_idiv_word(Bit16u op) DRC_FC;
static bool DRC_CALL_CONV dynrec_idiv_word(Bit16u op) {
	Bits val=(Bit16s)op;
	if (val==0) return CPU_PrepareException(0,0);
	Bits num=(Bit32s)((reg_dx<<16)|reg_ax);
	Bits quo=num/val;
	Bit16s rem=(Bit16s)(num % val);
	Bit16s quo16s=(Bit16s)quo;
	if (quo!=(Bit32s)quo16s) return CPU_PrepareException(0,0);
	reg_dx=rem;
	reg_ax=quo16s;
	return false;
}

static bool DRC_CALL_CONV dynrec_div_dword(Bit32u op) DRC_FC;
static bool DRC_CALL_CONV dynrec_div_dword(Bit32u op) {
	Bitu val=op;
	if (val==0) return CPU_PrepareException(0,0);
	Bit64u num=(((Bit64u)reg_edx)<<32)|reg_eax;
	Bit64u quo=num/val;
	Bit32u rem=(Bit32u)(num % val);
	Bit32u quo32=(Bit32u)(quo&0xffffffff);
	if (quo!=(Bit64u)quo32) return CPU_PrepareException(0,0);
	reg_edx=rem;
	reg_eax=quo32;
	return false;
}

static bool DRC_CALL_CONV dynrec_idiv_dword(Bit32u op) DRC_FC;
static bool DRC_CALL_CONV dynrec_idiv_dword(Bit32u op) {
	Bits val=(Bit32s)op;
	if (val==0) return CPU_PrepareException(0,0);
	Bit64s num=(((Bit64u)reg_edx)<<32)|reg_eax;	
	Bit64s quo=num/val;
	Bit32s rem=(Bit32s)(num % val);
	Bit32s quo32s=(Bit32s)(quo&0xffffffff);
	if (quo!=(Bit64s)quo32s) return CPU_PrepareException(0,0);
	reg_edx=rem;
	reg_eax=quo32s;
	return false;
}


static Bit16u DRC_CALL_CONV dynrec_dimul_word(Bit16u op1,Bit16u op2) DRC_FC;
static Bit16u DRC_CALL_CONV dynrec_dimul_word(Bit16u op1,Bit16u op2) {
	FillFlagsNoCFOF();
	Bits res=Bits((Bit16s)op1) * Bits((Bit16s)op2);
	if ((res>-32768)  && (res<32767)) {
		SETFLAGBIT(CF,false);
		SETFLAGBIT(OF,false);
	} else {
		SETFLAGBIT(CF,true);
		SETFLAGBIT(OF,true);
	}
	return (Bit16u)(res & 0xffff);
}

static Bit32u DRC_CALL_CONV dynrec_dimul_dword(Bit32u op1,Bit32u op2) DRC_FC;
static Bit32u DRC_CALL_CONV dynrec_dimul_dword(Bit32u op1,Bit32u op2) {
	FillFlagsNoCFOF();
	Bit64s res=((Bit64s)((Bit32s)op1))*((Bit64s)((Bit32s)op2));
	if ((res>-((Bit64s)(2147483647)+1)) && (res<(Bit64s)2147483647)) {
		SETFLAGBIT(CF,false);
		SETFLAGBIT(OF,false);
	} else {
		SETFLAGBIT(CF,true);
		SETFLAGBIT(OF,true);
	}
	return (Bit32s)res;
}



static Bit16u DRC_CALL_CONV dynrec_cbw(uint8_t op) DRC_FC;
static Bit16u DRC_CALL_CONV dynrec_cbw(uint8_t op) {
	return (int8_t)op;
}

static Bit32u DRC_CALL_CONV dynrec_cwde(Bit16u op) DRC_FC;
static Bit32u DRC_CALL_CONV dynrec_cwde(Bit16u op) {
	return (Bit16s)op;
}

static Bit16u DRC_CALL_CONV dynrec_cwd(Bit16u op) DRC_FC;
static Bit16u DRC_CALL_CONV dynrec_cwd(Bit16u op) {
	if (op & 0x8000) return 0xffff;
	else return 0;
}

static Bit32u DRC_CALL_CONV dynrec_cdq(Bit32u op) DRC_FC;
static Bit32u DRC_CALL_CONV dynrec_cdq(Bit32u op) {
	if (op & 0x80000000) return 0xffffffff;
	else return 0;
}


static void DRC_CALL_CONV dynrec_sahf(Bit16u op) DRC_FC;
static void DRC_CALL_CONV dynrec_sahf(Bit16u op) {
	SETFLAGBIT(OF,get_OF());
	lflags.type=t_UNKNOWN;
	CPU_SetFlags(op>>8,FMASK_NORMAL & 0xff);
}


static void DRC_CALL_CONV dynrec_cmc(void) DRC_FC;
static void DRC_CALL_CONV dynrec_cmc(void) {
	FillFlags();
	SETFLAGBIT(CF,!(reg_flags & FLAG_CF));
}
static void DRC_CALL_CONV dynrec_clc(void) DRC_FC;
static void DRC_CALL_CONV dynrec_clc(void) {
	FillFlags();
	SETFLAGBIT(CF,false);
}
static void DRC_CALL_CONV dynrec_stc(void) DRC_FC;
static void DRC_CALL_CONV dynrec_stc(void) {
	FillFlags();
	SETFLAGBIT(CF,true);
}
static void DRC_CALL_CONV dynrec_cld(void) DRC_FC;
static void DRC_CALL_CONV dynrec_cld(void) {
	SETFLAGBIT(DF,false);
	cpu.direction=1;
}
static void DRC_CALL_CONV dynrec_std(void) DRC_FC;
static void DRC_CALL_CONV dynrec_std(void) {
	SETFLAGBIT(DF,true);
	cpu.direction=-1;
}


static Bit16u DRC_CALL_CONV dynrec_movsb_word(Bit16u count,Bit16s add_index,PhysPt si_base,PhysPt di_base) DRC_FC;
static Bit16u DRC_CALL_CONV dynrec_movsb_word(Bit16u count,Bit16s add_index,PhysPt si_base,PhysPt di_base) {
	Bit16u count_left;
	if (count<(Bitu)CPU_Cycles) {
		count_left=0;
	} else {
		count_left=(Bit16u)(count-CPU_Cycles);
		count=(Bit16u)CPU_Cycles;
		CPU_Cycles=0;
	}
	for (;count>0;count--) {
		mem_writeb(di_base+reg_di,mem_readb(si_base+reg_si));
		reg_si+=add_index;
		reg_di+=add_index;
	}
	return count_left;
}

static Bit32u DRC_CALL_CONV dynrec_movsb_dword(Bit32u count,Bit32s add_index,PhysPt si_base,PhysPt di_base) DRC_FC;
static Bit32u DRC_CALL_CONV dynrec_movsb_dword(Bit32u count,Bit32s add_index,PhysPt si_base,PhysPt di_base) {
	Bit32u count_left;
	if (count<(Bitu)CPU_Cycles) {
		count_left=0;
	} else {
		count_left= (Bit32u)(count-CPU_Cycles);
		count= (Bit32u)CPU_Cycles;
		CPU_Cycles=0;
	}
	for (;count>0;count--) {
		mem_writeb(di_base+reg_edi,mem_readb(si_base+reg_esi));
		reg_esi+=add_index;
		reg_edi+=add_index;
	}
	return count_left;
}

static Bit16u DRC_CALL_CONV dynrec_movsw_word(Bit16u count,Bit16s add_index,PhysPt si_base,PhysPt di_base) DRC_FC;
static Bit16u DRC_CALL_CONV dynrec_movsw_word(Bit16u count,Bit16s add_index,PhysPt si_base,PhysPt di_base) {
	Bit16u count_left;
	if (count<(Bitu)CPU_Cycles) {
		count_left=0;
	} else {
		count_left=(Bit16u)(count-CPU_Cycles);
		count=(Bit16u)CPU_Cycles;
		CPU_Cycles=0;
	}
	add_index<<=1;
	for (;count>0;count--) {
		mem_writew(di_base+reg_di,mem_readw(si_base+reg_si));
		reg_si+=add_index;
		reg_di+=add_index;
	}
	return count_left;
}

static Bit32u DRC_CALL_CONV dynrec_movsw_dword(Bit32u count,Bit32s add_index,PhysPt si_base,PhysPt di_base) DRC_FC;
static Bit32u DRC_CALL_CONV dynrec_movsw_dword(Bit32u count,Bit32s add_index,PhysPt si_base,PhysPt di_base) {
	Bit32u count_left;
	if (count<(Bitu)CPU_Cycles) {
		count_left=0;
	} else {
		count_left= (Bit32u)(count-CPU_Cycles);
		count= (Bit32u)CPU_Cycles;
		CPU_Cycles=0;
	}
	add_index<<=1;
	for (;count>0;count--) {
		mem_writew(di_base+reg_edi,mem_readw(si_base+reg_esi));
		reg_esi+=add_index;
		reg_edi+=add_index;
	}
	return count_left;
}

static Bit16u DRC_CALL_CONV dynrec_movsd_word(Bit16u count,Bit16s add_index,PhysPt si_base,PhysPt di_base) DRC_FC;
static Bit16u DRC_CALL_CONV dynrec_movsd_word(Bit16u count,Bit16s add_index,PhysPt si_base,PhysPt di_base) {
	Bit16u count_left;
	if (count<(Bitu)CPU_Cycles) {
		count_left=0;
	} else {
		count_left=(Bit16u)(count-CPU_Cycles);
		count=(Bit16u)CPU_Cycles;
		CPU_Cycles=0;
	}
	add_index<<=2;
	for (;count>0;count--) {
		mem_writed(di_base+reg_di,mem_readd(si_base+reg_si));
		reg_si+=add_index;
		reg_di+=add_index;
	}
	return count_left;
}

static Bit32u DRC_CALL_CONV dynrec_movsd_dword(Bit32u count,Bit32s add_index,PhysPt si_base,PhysPt di_base) DRC_FC;
static Bit32u DRC_CALL_CONV dynrec_movsd_dword(Bit32u count,Bit32s add_index,PhysPt si_base,PhysPt di_base) {
	Bit32u count_left;
	if (count<(Bitu)CPU_Cycles) {
		count_left=0;
	} else {
		count_left= (Bit32u)(count - CPU_Cycles);
		count= (Bit32u)CPU_Cycles;
		CPU_Cycles=0;
	}
	add_index<<=2;
	for (;count>0;count--) {
		mem_writed(di_base+reg_edi,mem_readd(si_base+reg_esi));
		reg_esi+=add_index;
		reg_edi+=add_index;
	}
	return count_left;
}


static Bit16u DRC_CALL_CONV dynrec_lodsb_word(Bit16u count,Bit16s add_index,PhysPt si_base) DRC_FC;
static Bit16u DRC_CALL_CONV dynrec_lodsb_word(Bit16u count,Bit16s add_index,PhysPt si_base) {
	Bit16u count_left;
	if (count<(Bitu)CPU_Cycles) {
		count_left=0;
	} else {
		count_left=(Bit16u)(count-CPU_Cycles);
		count=(Bit16u)CPU_Cycles;
		CPU_Cycles=0;
	}
	for (;count>0;count--) {
		reg_al=mem_readb(si_base+reg_si);
		reg_si+=add_index;
	}
	return count_left;
}

static Bit32u DRC_CALL_CONV dynrec_lodsb_dword(Bit32u count,Bit32s add_index,PhysPt si_base) DRC_FC;
static Bit32u DRC_CALL_CONV dynrec_lodsb_dword(Bit32u count,Bit32s add_index,PhysPt si_base) {
	Bit32u count_left;
	if (count<(Bitu)CPU_Cycles) {
		count_left=0;
	} else {
		count_left= (Bit32u)(count - CPU_Cycles);
		count= (Bit32u)CPU_Cycles;
		CPU_Cycles=0;
	}
	for (;count>0;count--) {
		reg_al=mem_readb(si_base+reg_esi);
		reg_esi+=add_index;
	}
	return count_left;
}

static Bit16u DRC_CALL_CONV dynrec_lodsw_word(Bit16u count,Bit16s add_index,PhysPt si_base) DRC_FC;
static Bit16u DRC_CALL_CONV dynrec_lodsw_word(Bit16u count,Bit16s add_index,PhysPt si_base) {
	Bit16u count_left;
	if (count<(Bitu)CPU_Cycles) {
		count_left=0;
	} else {
		count_left=(Bit16u)(count-CPU_Cycles);
		count=(Bit16u)CPU_Cycles;
		CPU_Cycles=0;
	}
	add_index<<=1;
	for (;count>0;count--) {
		reg_ax=mem_readw(si_base+reg_si);
		reg_si+=add_index;
	}
	return count_left;
}

static Bit32u DRC_CALL_CONV dynrec_lodsw_dword(Bit32u count,Bit32s add_index,PhysPt si_base) DRC_FC;
static Bit32u DRC_CALL_CONV dynrec_lodsw_dword(Bit32u count,Bit32s add_index,PhysPt si_base) {
	Bit32u count_left;
	if (count<(Bitu)CPU_Cycles) {
		count_left=0;
	} else {
		count_left= (Bit32u)(count - CPU_Cycles);
		count= (Bit32u)CPU_Cycles;
		CPU_Cycles=0;
	}
	add_index<<=1;
	for (;count>0;count--) {
		reg_ax=mem_readw(si_base+reg_esi);
		reg_esi+=add_index;
	}
	return count_left;
}

static Bit16u DRC_CALL_CONV dynrec_lodsd_word(Bit16u count,Bit16s add_index,PhysPt si_base) DRC_FC;
static Bit16u DRC_CALL_CONV dynrec_lodsd_word(Bit16u count,Bit16s add_index,PhysPt si_base) {
	Bit16u count_left;
	if (count<(Bitu)CPU_Cycles) {
		count_left=0;
	} else {
		count_left=(Bit16u)(count-CPU_Cycles);
		count=(Bit16u)CPU_Cycles;
		CPU_Cycles=0;
	}
	add_index<<=2;
	for (;count>0;count--) {
		reg_eax=mem_readd(si_base+reg_si);
		reg_si+=add_index;
	}
	return count_left;
}

static Bit32u DRC_CALL_CONV dynrec_lodsd_dword(Bit32u count,Bit32s add_index,PhysPt si_base) DRC_FC;
static Bit32u DRC_CALL_CONV dynrec_lodsd_dword(Bit32u count,Bit32s add_index,PhysPt si_base) {
	Bit32u count_left;
	if (count<(Bitu)CPU_Cycles) {
		count_left=0;
	} else {
		count_left= (Bit32u)(count - CPU_Cycles);
		count= (Bit32u)CPU_Cycles;
		CPU_Cycles=0;
	}
	add_index<<=2;
	for (;count>0;count--) {
		reg_eax=mem_readd(si_base+reg_esi);
		reg_esi+=add_index;
	}
	return count_left;
}


static Bit16u DRC_CALL_CONV dynrec_stosb_word(Bit16u count,Bit16s add_index,PhysPt di_base) DRC_FC;
static Bit16u DRC_CALL_CONV dynrec_stosb_word(Bit16u count,Bit16s add_index,PhysPt di_base) {
	Bit16u count_left;
	if (count<(Bitu)CPU_Cycles) {
		count_left=0;
	} else {
		count_left=(Bit16u)(count-CPU_Cycles);
		count=(Bit16u)CPU_Cycles;
		CPU_Cycles=0;
	}
	for (;count>0;count--) {
		mem_writeb(di_base+reg_di,reg_al);
		reg_di+=add_index;
	}
	return count_left;
}

static Bit32u DRC_CALL_CONV dynrec_stosb_dword(Bit32u count,Bit32s add_index,PhysPt di_base) DRC_FC;
static Bit32u DRC_CALL_CONV dynrec_stosb_dword(Bit32u count,Bit32s add_index,PhysPt di_base) {
	Bit32u count_left;
	if (count<(Bitu)CPU_Cycles) {
		count_left=0;
	} else {
		count_left= (Bit32u)(count - CPU_Cycles);
		count= (Bit32u)CPU_Cycles;
		CPU_Cycles=0;
	}
	for (;count>0;count--) {
		mem_writeb(di_base+reg_edi,reg_al);
		reg_edi+=add_index;
	}
	return count_left;
}

static Bit16u DRC_CALL_CONV dynrec_stosw_word(Bit16u count,Bit16s add_index,PhysPt di_base) DRC_FC;
static Bit16u DRC_CALL_CONV dynrec_stosw_word(Bit16u count,Bit16s add_index,PhysPt di_base) {
	Bit16u count_left;
	if (count<(Bitu)CPU_Cycles) {
		count_left=0;
	} else {
		count_left=(Bit16u)(count-CPU_Cycles);
		count=(Bit16u)CPU_Cycles;
		CPU_Cycles=0;
	}
	add_index<<=1;
	for (;count>0;count--) {
		mem_writew(di_base+reg_di,reg_ax);
		reg_di+=add_index;
	}
	return count_left;
}

static Bit32u DRC_CALL_CONV dynrec_stosw_dword(Bit32u count,Bit32s add_index,PhysPt di_base) DRC_FC;
static Bit32u DRC_CALL_CONV dynrec_stosw_dword(Bit32u count,Bit32s add_index,PhysPt di_base) {
	Bit32u count_left;
	if (count<(Bitu)CPU_Cycles) {
		count_left=0;
	} else {
		count_left= (Bit32u)(count - CPU_Cycles);
		count=(Bit32u) CPU_Cycles;
		CPU_Cycles=0;
	}
	add_index<<=1;
	for (;count>0;count--) {
		mem_writew(di_base+reg_edi,reg_ax);
		reg_edi+=add_index;
	}
	return count_left;
}

static Bit16u DRC_CALL_CONV dynrec_stosd_word(Bit16u count,Bit16s add_index,PhysPt di_base) DRC_FC;
static Bit16u DRC_CALL_CONV dynrec_stosd_word(Bit16u count,Bit16s add_index,PhysPt di_base) {
	Bit16u count_left;
	if (count<(Bitu)CPU_Cycles) {
		count_left=0;
	} else {
		count_left=(Bit16u)(count-CPU_Cycles);
		count=(Bit16u)CPU_Cycles;
		CPU_Cycles=0;
	}
	add_index<<=2;
	for (;count>0;count--) {
		mem_writed(di_base+reg_di,reg_eax);
		reg_di+=add_index;
	}
	return count_left;
}

static Bit32u DRC_CALL_CONV dynrec_stosd_dword(Bit32u count,Bit32s add_index,PhysPt di_base) DRC_FC;
static Bit32u DRC_CALL_CONV dynrec_stosd_dword(Bit32u count,Bit32s add_index,PhysPt di_base) {
	Bit32u count_left;
	if (count<(Bitu)CPU_Cycles) {
		count_left=0;
	} else {
		count_left= (Bit32u)(count - CPU_Cycles);
		count=(Bit32u) CPU_Cycles;
		CPU_Cycles=0;
	}
	add_index<<=2;
	for (;count>0;count--) {
		mem_writed(di_base+reg_edi,reg_eax);
		reg_edi+=add_index;
	}
	return count_left;
}


static void DRC_CALL_CONV dynrec_push_word(Bit16u value) DRC_FC;
static void DRC_CALL_CONV dynrec_push_word(Bit16u value) {
	Bit32u new_esp=(reg_esp&cpu.stack.notmask)|((reg_esp-2)&cpu.stack.mask);
	mem_writew(SegPhys(ss) + (new_esp & cpu.stack.mask),value);
	reg_esp=new_esp;
}

static void DRC_CALL_CONV dynrec_push_dword(Bit32u value) DRC_FC;
static void DRC_CALL_CONV dynrec_push_dword(Bit32u value) {
	Bit32u new_esp=(reg_esp&cpu.stack.notmask)|((reg_esp-4)&cpu.stack.mask);
	mem_writed(SegPhys(ss) + (new_esp & cpu.stack.mask) ,value);
	reg_esp=new_esp;
}

static Bit16u DRC_CALL_CONV dynrec_pop_word(void) DRC_FC;
static Bit16u DRC_CALL_CONV dynrec_pop_word(void) {
	Bit16u val=mem_readw(SegPhys(ss) + (reg_esp & cpu.stack.mask));
	reg_esp=(reg_esp&cpu.stack.notmask)|((reg_esp+2)&cpu.stack.mask);
	return val;
}

static Bit32u DRC_CALL_CONV dynrec_pop_dword(void) DRC_FC;
static Bit32u DRC_CALL_CONV dynrec_pop_dword(void) {
	Bit32u val=mem_readd(SegPhys(ss) + (reg_esp & cpu.stack.mask));
	reg_esp=(reg_esp&cpu.stack.notmask)|((reg_esp+4)&cpu.stack.mask);
	return val;
}
