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



#include "decoder_basic.h"
#include "operators.h"
#include "decoder_opcodes.h"

#include "dyn_fpu.h"

/*
	The function CreateCacheBlock translates the instruction stream
	until either an unhandled instruction is found, the maximum
	number of translated instructions is reached or some critical
	instruction is encountered.
*/

static CacheBlockDynRec * CreateCacheBlock(CodePageHandlerDynRec * codepage,PhysPt start,Bitu max_opcodes) {
#if (C_HAVE_MPROTECT)
	if (w_xor_x) {
		if (mprotect(cache_code_link_blocks,CACHE_TOTAL+CACHE_MAXSIZE+PAGESIZE_TEMP,PROT_READ|PROT_WRITE))
			LOG_MSG("Setting execute permission on the code cache has failed! err=%s",strerror(errno));
	}
#endif

	// initialize a load of variables
	decode.code_start=start;
	decode.code=start;
	decode.page.code=codepage;
	decode.page.index=start&4095;
	decode.page.wmap=codepage->write_map;
	decode.page.invmap=codepage->invalidation_map;
	decode.page.first=start >> 12;
	decode.active_block=decode.block=cache_openblock();
	decode.block->page.start=(uint16_t)decode.page.index;
	codepage->AddCacheBlock(decode.block);

	InitFlagsOptimization();

	// every codeblock that is run sets cache.block.running to itself
	// so the block linking knows the last executed block
	gen_mov_direct_ptr(&cache.block.running,(DRC_PTR_SIZE_IM)decode.block);

	// start with the cycles check
	gen_mov_word_to_reg(FC_RETOP,&CPU_Cycles,true);
	save_info_dynrec[used_save_info_dynrec].branch_pos=gen_create_branch_long_leqzero(FC_RETOP);
	save_info_dynrec[used_save_info_dynrec].type=cycle_check;
	used_save_info_dynrec++;

	decode.cycles=0;
	while (max_opcodes--) {
		// Init prefixes
		decode.big_addr=cpu.code.big;
		decode.big_op=cpu.code.big;
		decode.seg_prefix=0;
		decode.seg_prefix_used=false;
		decode.rep=REP_NONE;
		decode.cycles++;
		decode.op_start=decode.code;
restart_prefix:
		Bitu opcode;
		if (!decode.page.invmap) opcode=decode_fetchb();
		else {
			// some entries in the invalidation map, see if the next
			// instruction is known to be modified a lot
			if (decode.page.index<4096) {
				if (GCC_UNLIKELY(decode.page.invmap[decode.page.index]>=4)) goto illegalopcode;
				opcode=decode_fetchb();
			} else {
				// switch to the next page
				opcode=decode_fetchb();
				if (GCC_UNLIKELY(decode.page.invmap && 
					(decode.page.invmap[decode.page.index-1]>=4))) goto illegalopcode;
			}
		}
		switch (opcode) {
		// instructions 'op reg8,reg8' and 'op [],reg8'
		case 0x00:dyn_dop_ebgb(DOP_ADD);break;
		case 0x08:dyn_dop_ebgb(DOP_OR);break;
		case 0x10:dyn_dop_ebgb(DOP_ADC);break;
		case 0x18:dyn_dop_ebgb(DOP_SBB);break;
		case 0x20:dyn_dop_ebgb(DOP_AND);break;
		case 0x28:dyn_dop_ebgb(DOP_SUB);break;
		case 0x30:dyn_dop_ebgb(DOP_XOR);break;
		case 0x38:dyn_dop_ebgb(DOP_CMP);break;

		// instructions 'op reg8,reg8' and 'op reg8,[]'
		case 0x02:dyn_dop_gbeb(DOP_ADD);break;
		case 0x0a:dyn_dop_gbeb(DOP_OR);break;
		case 0x12:dyn_dop_gbeb(DOP_ADC);break;
		case 0x1a:dyn_dop_gbeb(DOP_SBB);break;
		case 0x22:dyn_dop_gbeb(DOP_AND);break;
		case 0x2a:dyn_dop_gbeb(DOP_SUB);break;
		case 0x32:dyn_dop_gbeb(DOP_XOR);break;
		case 0x3a:dyn_dop_gbeb(DOP_CMP);break;

		// instructions 'op reg16/32,reg16/32' and 'op [],reg16/32'
		case 0x01:dyn_dop_evgv(DOP_ADD);break;
		case 0x09:dyn_dop_evgv(DOP_OR);break;
		case 0x11:dyn_dop_evgv(DOP_ADC);break;
		case 0x19:dyn_dop_evgv(DOP_SBB);break;
		case 0x21:dyn_dop_evgv(DOP_AND);break;
		case 0x29:dyn_dop_evgv(DOP_SUB);break;
		case 0x31:dyn_dop_evgv(DOP_XOR);break;
		case 0x39:dyn_dop_evgv(DOP_CMP);break;

		// instructions 'op reg16/32,reg16/32' and 'op reg16/32,[]'
		case 0x03:dyn_dop_gvev(DOP_ADD);break;
		case 0x0b:dyn_dop_gvev(DOP_OR);break;
		case 0x13:dyn_dop_gvev(DOP_ADC);break;
		case 0x1b:dyn_dop_gvev(DOP_SBB);break;
		case 0x23:dyn_dop_gvev(DOP_AND);break;
		case 0x2b:dyn_dop_gvev(DOP_SUB);break;
		case 0x33:dyn_dop_gvev(DOP_XOR);break;
		case 0x3b:dyn_dop_gvev(DOP_CMP);break;

		// instructions 'op reg8,imm8'
		case 0x04:dyn_dop_byte_imm(DOP_ADD,DRC_REG_EAX,0);break;
		case 0x0c:dyn_dop_byte_imm(DOP_OR,DRC_REG_EAX,0);break;
		case 0x14:dyn_dop_byte_imm(DOP_ADC,DRC_REG_EAX,0);break;
		case 0x1c:dyn_dop_byte_imm(DOP_SBB,DRC_REG_EAX,0);break;
		case 0x24:dyn_dop_byte_imm(DOP_AND,DRC_REG_EAX,0);break;
		case 0x2c:dyn_dop_byte_imm(DOP_SUB,DRC_REG_EAX,0);break;
		case 0x34:dyn_dop_byte_imm(DOP_XOR,DRC_REG_EAX,0);break;
		case 0x3c:dyn_dop_byte_imm(DOP_CMP,DRC_REG_EAX,0);break;

		// instructions 'op reg16/32,imm16/32'
		case 0x05:dyn_dop_word_imm(DOP_ADD,DRC_REG_EAX);break;
		case 0x0d:dyn_dop_word_imm_old(DOP_OR,DRC_REG_EAX,decode.big_op ? decode_fetchd() :  decode_fetchw());break;
		case 0x15:dyn_dop_word_imm_old(DOP_ADC,DRC_REG_EAX,decode.big_op ? decode_fetchd() :  decode_fetchw());break;
		case 0x1d:dyn_dop_word_imm_old(DOP_SBB,DRC_REG_EAX,decode.big_op ? decode_fetchd() :  decode_fetchw());break;
		case 0x25:dyn_dop_word_imm(DOP_AND,DRC_REG_EAX);break;
		case 0x2d:dyn_dop_word_imm_old(DOP_SUB,DRC_REG_EAX,decode.big_op ? decode_fetchd() :  decode_fetchw());break;
		case 0x35:dyn_dop_word_imm_old(DOP_XOR,DRC_REG_EAX,decode.big_op ? decode_fetchd() :  decode_fetchw());break;
		case 0x3d:dyn_dop_word_imm_old(DOP_CMP,DRC_REG_EAX,decode.big_op ? decode_fetchd() :  decode_fetchw());break;

		// push segment register onto stack
		case 0x06:dyn_push_seg(DRC_SEG_ES);break;
		case 0x0e:dyn_push_seg(DRC_SEG_CS);break;
		case 0x16:dyn_push_seg(DRC_SEG_SS);break;
		case 0x1e:dyn_push_seg(DRC_SEG_DS);break;

		// pop segment register from stack
		case 0x07:dyn_pop_seg(DRC_SEG_ES);break;
		case 0x17:dyn_pop_seg(DRC_SEG_SS);break;
		case 0x1f:dyn_pop_seg(DRC_SEG_DS);break;

		// segment prefixes
		case 0x26:dyn_segprefix(DRC_SEG_ES);goto restart_prefix;
		case 0x2e:dyn_segprefix(DRC_SEG_CS);goto restart_prefix;
		case 0x36:dyn_segprefix(DRC_SEG_SS);goto restart_prefix;
		case 0x3e:dyn_segprefix(DRC_SEG_DS);goto restart_prefix;
		case 0x64:dyn_segprefix(DRC_SEG_FS);goto restart_prefix;
		case 0x65:dyn_segprefix(DRC_SEG_GS);goto restart_prefix;

//		case 0x27: DAA missing
//		case 0x2f: DAS missing
//		case 0x37: AAA missing
//		case 0x3f: AAS missing

		// dual opcodes
		case 0x0f:
		{
			Bitu dual_code=decode_fetchb();
			switch (dual_code) {
				case 0x00:
					if ((reg_flags & FLAG_VM) || (!cpu.pmode)) goto illegalopcode;
					dyn_grp6();
					break;
				case 0x01:
					if (dyn_grp7()) goto finish_block;
					break;
/*				case 0x02:
					if ((reg_flags & FLAG_VM) || (!cpu.pmode)) goto illegalopcode;
					dyn_larlsl(true);
					break;
				case 0x03:
					if ((reg_flags & FLAG_VM) || (!cpu.pmode)) goto illegalopcode;
					dyn_larlsl(false);
					break; */

				case 0x20:dyn_mov_from_crx();break;
				case 0x22:dyn_mov_to_crx();goto finish_block;

				// short conditional jumps
				case 0x80:case 0x81:case 0x82:case 0x83:case 0x84:case 0x85:case 0x86:case 0x87:	
				case 0x88:case 0x89:case 0x8a:case 0x8b:case 0x8c:case 0x8d:case 0x8e:case 0x8f:	
					dyn_branched_exit((BranchTypes)(dual_code&0xf),
						decode.big_op ? (int32_t)decode_fetchd() : (int16_t)decode_fetchw());
					goto finish_block;

				// conditional byte set instructions
/*				case 0x90:case 0x91:case 0x92:case 0x93:case 0x94:case 0x95:case 0x96:case 0x97:	
				case 0x98:case 0x99:case 0x9a:case 0x9b:case 0x9c:case 0x9d:case 0x9e:case 0x9f:	
					dyn_set_byte_on_condition((BranchTypes)(dual_code&0xf));
					AcquireFlags(FMASK_TEST);
					break; */

				// push/pop segment registers
				case 0xa0:dyn_push_seg(DRC_SEG_FS);break;
				case 0xa1:dyn_pop_seg(DRC_SEG_FS);break;
				case 0xa8:dyn_push_seg(DRC_SEG_GS);break;
				case 0xa9:dyn_pop_seg(DRC_SEG_GS);break;

				// double shift instructions
				case 0xa4:dyn_dshift_ev_gv(true,true);break;
				case 0xa5:dyn_dshift_ev_gv(true,false);break;
				case 0xac:dyn_dshift_ev_gv(false,true);break;
				case 0xad:dyn_dshift_ev_gv(false,false);break;

				case 0xaf:dyn_imul_gvev(0);break;

				// lfs
				case 0xb4:
					dyn_get_modrm();
					if (GCC_UNLIKELY(decode.modrm.mod==3)) goto illegalopcode;
					dyn_load_seg_off_ea(DRC_SEG_FS);
					break;
				// lgs
				case 0xb5:
					dyn_get_modrm();
					if (GCC_UNLIKELY(decode.modrm.mod==3)) goto illegalopcode;
					dyn_load_seg_off_ea(DRC_SEG_GS);
					break;

				// zero-extending moves
				case 0xb6:dyn_movx_ev_gb(false);break;
				case 0xb7:dyn_movx_ev_gw(false);break;

				// sign-extending moves
				case 0xbe:dyn_movx_ev_gb(true);break;
				case 0xbf:dyn_movx_ev_gw(true);break;

				default:
#if DYN_LOG
//					LOG_MSG("Unhandled dual opcode 0F%02X",dual_code);
#endif
					goto illegalopcode;
			}
			break;
		}

		// 'inc/dec reg16/32'
		case 0x40:case 0x41:case 0x42:case 0x43:case 0x44:case 0x45:case 0x46:case 0x47:	
			dyn_sop_word(SOP_INC,opcode&7);
			break;
		case 0x48:case 0x49:case 0x4a:case 0x4b:case 0x4c:case 0x4d:case 0x4e:case 0x4f:	
			dyn_sop_word(SOP_DEC,opcode&7);
			break;

		// 'push/pop reg16/32'
		case 0x50:case 0x51:case 0x52:case 0x53:case 0x54:case 0x55:case 0x56:case 0x57:	
			dyn_push_reg(opcode&7);
			break;
		case 0x58:case 0x59:case 0x5a:case 0x5b:case 0x5c:case 0x5d:case 0x5e:case 0x5f:	
			dyn_pop_reg(opcode&7);
			break;

		case 0x60:
			if (decode.big_op) gen_call_function_raw(dynrec_pusha_dword);
			else gen_call_function_raw(dynrec_pusha_word);
			break;
		case 0x61:
			if (decode.big_op) gen_call_function_raw(dynrec_popa_dword);
			else gen_call_function_raw(dynrec_popa_word);
			break;

//		case 0x62: BOUND missing
//		case 0x61: ARPL missing

		case 0x66:decode.big_op=!cpu.code.big;goto restart_prefix;
		case 0x67:decode.big_addr=!cpu.code.big;goto restart_prefix;

		// 'push imm8/16/32'
		case 0x68:
			dyn_push_word_imm(decode.big_op ? decode_fetchd() :  decode_fetchw());
			break;
		case 0x6a:
			dyn_push_byte_imm((int8_t)decode_fetchb());
			break;

		// signed multiplication
		case 0x69:dyn_imul_gvev(decode.big_op ? 4 : 2);break;
		case 0x6b:dyn_imul_gvev(1);break;

//		case 0x6c to 0x6f missing (ins/outs)

		// short conditional jumps
		case 0x70:case 0x71:case 0x72:case 0x73:case 0x74:case 0x75:case 0x76:case 0x77:	
		case 0x78:case 0x79:case 0x7a:case 0x7b:case 0x7c:case 0x7d:case 0x7e:case 0x7f:	
			dyn_branched_exit((BranchTypes)(opcode&0xf),(int8_t)decode_fetchb());	
			goto finish_block;

		// 'op []/reg8,imm8'
		case 0x80:
		case 0x82:dyn_grp1_eb_ib();break;

		// 'op []/reg16/32,imm16/32'
		case 0x81:dyn_grp1_ev_iv(false);break;
		case 0x83:dyn_grp1_ev_iv(true);break;

		// 'test []/reg8/16/32,reg8/16/32'
		case 0x84:dyn_dop_gbeb(DOP_TEST);break;
		case 0x85:dyn_dop_gvev(DOP_TEST);break;

		// 'xchg reg8/16/32,[]/reg8/16/32'
		case 0x86:dyn_dop_ebgb_xchg();break;
		case 0x87:dyn_dop_evgv_xchg();break;

		// 'mov []/reg8/16/32,reg8/16/32'
		case 0x88:dyn_dop_ebgb_mov();break;
		case 0x89:dyn_dop_evgv_mov();break;
		// 'mov reg8/16/32,[]/reg8/16/32'
		case 0x8a:dyn_dop_gbeb_mov();break;
		case 0x8b:dyn_dop_gvev_mov();break;

		// move segment register into memory or a 16bit register
		case 0x8c:dyn_mov_ev_seg();break;

		// load effective address
		case 0x8d:dyn_lea();break;

		// move a value from memory or a 16bit register into a segment register
		case 0x8e:dyn_mov_seg_ev();break;

		// 'pop []'
		case 0x8f:dyn_pop_ev();break;

		case 0x90:	// nop
		case 0x9b:	// wait
		case 0xf0:	// lock
			break;

		case 0x91:case 0x92:case 0x93:case 0x94:case 0x95:case 0x96:case 0x97:
			dyn_xchg_ax(opcode&7);
			break;

		// sign-extend al into ax/sign-extend ax into eax
		case 0x98:dyn_cbw();break;
		// sign-extend ax into dx:ax/sign-extend eax into edx:eax
		case 0x99:dyn_cwd();break;

		case 0x9a:dyn_call_far_imm();goto finish_block;

		case 0x9c:	// pushf
			AcquireFlags(FMASK_TEST);
			gen_call_function_I(CPU_PUSHF,decode.big_op);
			dyn_check_exception(FC_RETOP);
			break;
		case 0x9d:	// popf
			gen_call_function_I(CPU_POPF,decode.big_op);
			dyn_check_exception(FC_RETOP);
			InvalidateFlags();
			break;

		case 0x9e:dyn_sahf();break;
//		case 0x9f: LAHF missing

		// 'mov al/ax,[]'
		case 0xa0:
			dyn_mov_byte_al_direct(decode.big_addr ? decode_fetchd() : decode_fetchw());
			break;
		case 0xa1:
			dyn_mov_byte_ax_direct(decode.big_addr ? decode_fetchd() : decode_fetchw());
			break;
		// 'mov [],al/ax'
		case 0xa2:
			dyn_mov_byte_direct_al();
			break;
		case 0xa3:
			dyn_mov_byte_direct_ax(decode.big_addr ? decode_fetchd() : decode_fetchw());
			break;


//		case 0xa6 to 0xaf string operations, some missing

		// movsb/w/d
		case 0xa4:dyn_string(STR_MOVSB);break;
		case 0xa5:dyn_string(decode.big_op ? STR_MOVSD : STR_MOVSW);break;

		// stosb/w/d
		case 0xaa:dyn_string(STR_STOSB);break;
		case 0xab:dyn_string(decode.big_op ? STR_STOSD : STR_STOSW);break;

		// lodsb/w/d
		case 0xac:dyn_string(STR_LODSB);break;
		case 0xad:dyn_string(decode.big_op ? STR_LODSD : STR_LODSW);break;


		// 'test reg8/16/32,imm8/16/32'
		case 0xa8:dyn_dop_byte_imm(DOP_TEST,DRC_REG_EAX,0);break;
		case 0xa9:dyn_dop_word_imm_old(DOP_TEST,DRC_REG_EAX,decode.big_op ? decode_fetchd() :  decode_fetchw());break;

		// 'mov reg8/16/32,imm8/16/32'
		case 0xb0:case 0xb1:case 0xb2:case 0xb3:case 0xb4:case 0xb5:case 0xb6:case 0xb7:	
			dyn_mov_byte_imm(opcode&3,(opcode>>2)&1,decode_fetchb());
			break;
		case 0xb8:case 0xb9:case 0xba:case 0xbb:case 0xbc:case 0xbd:case 0xbe:case 0xbf:	
			dyn_mov_word_imm(opcode&7);
			break;

		// 'shiftop []/reg8,imm8/1/cl'
		case 0xc0:dyn_grp2_eb(grp2_imm);break;
		case 0xd0:dyn_grp2_eb(grp2_1);break;
		case 0xd2:dyn_grp2_eb(grp2_cl);break;

		// 'shiftop []/reg16/32,imm8/1/cl'
		case 0xc1:dyn_grp2_ev(grp2_imm);break;
		case 0xd1:dyn_grp2_ev(grp2_1);break;
		case 0xd3:dyn_grp2_ev(grp2_cl);break;

		// retn [param]
		case 0xc2:dyn_ret_near(decode_fetchw());goto finish_block;
		case 0xc3:dyn_ret_near(0);goto finish_block;

		// les
		case 0xc4:
			dyn_get_modrm();
			if (GCC_UNLIKELY(decode.modrm.mod==3)) goto illegalopcode;
			dyn_load_seg_off_ea(DRC_SEG_ES);
			break;
		// lds
		case 0xc5:
			dyn_get_modrm();
			if (GCC_UNLIKELY(decode.modrm.mod==3)) goto illegalopcode;
			dyn_load_seg_off_ea(DRC_SEG_DS);break;

		// 'mov []/reg8/16/32,imm8/16/32'
		case 0xc6:dyn_dop_ebib_mov();break;
		case 0xc7:dyn_dop_eviv_mov();break;

		case 0xc8:dyn_enter();break;
		case 0xc9:dyn_leave();break;

		// retf [param]
		case 0xca:dyn_ret_far(decode_fetchw());goto finish_block;
		case 0xcb:dyn_ret_far(0);goto finish_block;

		// int/iret
#if !(C_DEBUG)
		case 0xcd:dyn_interrupt(decode_fetchb());goto finish_block;
#endif
		case 0xcf:dyn_iret();goto finish_block;

//		case 0xd4: AAM missing
//		case 0xd5: AAD missing

//		case 0xd6: SALC missing
//		case 0xd7: XLAT missing


#ifdef CPU_FPU
		// floating point instructions
		case 0xd8:
			dyn_fpu_esc0();
			break;
		case 0xd9:
			dyn_fpu_esc1();
			break;
		case 0xda:
			dyn_fpu_esc2();
			break;
		case 0xdb:
			dyn_fpu_esc3();
			break;
		case 0xdc:
			dyn_fpu_esc4();
			break;
		case 0xdd:
			dyn_fpu_esc5();
			break;
		case 0xde:
			dyn_fpu_esc6();
			break;
		case 0xdf:
			dyn_fpu_esc7();
			break;
#endif


		// loop instructions
		case 0xe0:dyn_loop(LOOP_NE);goto finish_block;
		case 0xe1:dyn_loop(LOOP_E);goto finish_block;
		case 0xe2:dyn_loop(LOOP_NONE);goto finish_block;
		case 0xe3:dyn_loop(LOOP_JCXZ);goto finish_block;


		// 'in al/ax/eax,port_imm'
		case 0xe4:dyn_read_port_byte_direct(decode_fetchb());break;
		case 0xe5:dyn_read_port_word_direct(decode_fetchb());break;
		// 'out port_imm,al/ax/eax'
		case 0xe6:dyn_write_port_byte_direct(decode_fetchb());break;
		case 0xe7:dyn_write_port_word_direct(decode_fetchb());break;

		// 'in al/ax/eax,port_dx'
		case 0xec:dyn_read_port_byte();break;
		case 0xed:dyn_read_port_word();break;
		// 'out port_dx,al/ax/eax'
		case 0xee:dyn_write_port_byte();break;
		case 0xef:dyn_write_port_word();break;


		// 'call near imm16/32'
		case 0xe8:
			dyn_call_near_imm();
			goto finish_block;
		// 'jmp near imm16/32'
		case 0xe9:
			dyn_exit_link(decode.big_op ? (int32_t)decode_fetchd() : (int16_t)decode_fetchw());
			goto finish_block;
		// 'jmp far'
		case 0xea:
			dyn_jmp_far_imm();
			goto finish_block;
		// 'jmp short imm8'
		case 0xeb:
			dyn_exit_link((int8_t)decode_fetchb());
			goto finish_block;


		// repeat prefixes
		case 0xf2:
			decode.rep=REP_NZ;
			goto restart_prefix;
		case 0xf3:
			decode.rep=REP_Z;
			goto restart_prefix;

		case 0xf5:		//CMC
			gen_call_function_raw(dynrec_cmc);
			break;
		case 0xf8:		//CLC
			gen_call_function_raw(dynrec_clc);
			break;
		case 0xf9:		//STC
			gen_call_function_raw(dynrec_stc);
			break;

		case 0xf6:dyn_grp3_eb();break;
		case 0xf7:dyn_grp3_ev();break;

		case 0xfa:		//CLI
			gen_call_function_raw(CPU_CLI);
			dyn_check_exception(FC_RETOP);
			break;
		case 0xfb:		//STI
			gen_call_function_raw(CPU_STI);
			dyn_check_exception(FC_RETOP);
			max_opcodes=1;		//Allow 1 extra opcode
			break;

		case 0xfc:		//CLD
			gen_call_function_raw(dynrec_cld);
			break;
		case 0xfd:		//STD
			gen_call_function_raw(dynrec_std);
			break;

		case 0xfe:
			if (dyn_grp4_eb()) goto finish_block;
			break;
		case 0xff:
			switch (dyn_grp4_ev()) {
			case 0:
				break;
			case 1:
				goto core_close_block;
			case 2:
				goto illegalopcode;
			default:
				break;
			}
			break;

		default:
#if DYN_LOG
//			LOG_MSG("Dynrec unhandled opcode %X",opcode);
#endif
			goto illegalopcode;
		}
	}
	// link to next block because the maximum number of opcodes has been reached
	dyn_set_eip_end();
	dyn_reduce_cycles();
	gen_jmp_ptr(&decode.block->link[0].to,offsetof(CacheBlockDynRec,cache.start));
	dyn_closeblock();
    goto finish_block;
core_close_block:
	dyn_reduce_cycles();
	dyn_return(BR_Normal);
	dyn_closeblock();
	goto finish_block;
illegalopcode:
	// some unhandled opcode has been encountered
	dyn_set_eip_last();
	dyn_reduce_cycles();
	dyn_return(BR_Opcode);	// tell the core what happened
	dyn_closeblock();
	goto finish_block;
finish_block:

	// setup the correct end-address
	decode.page.index--;
	decode.active_block->page.end=(uint16_t)decode.page.index;
//	LOG_MSG("Created block size %d start %d end %d",decode.block->cache.size,decode.block->page.start,decode.block->page.end);

#if (C_HAVE_MPROTECT)
	if (w_xor_x) {
		if (mprotect(cache_code_link_blocks,CACHE_TOTAL+CACHE_MAXSIZE+PAGESIZE_TEMP,PROT_READ|PROT_EXEC))
			LOG_MSG("Setting execute permission on the code cache has failed! err=%s",strerror(errno));
	}
#endif

	return decode.block;
}
