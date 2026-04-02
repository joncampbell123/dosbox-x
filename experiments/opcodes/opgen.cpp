
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "opgen.h"

#include <vector>

using namespace std;

struct opcode_pat_t {
	vector<uint8_t>		pat;
	unsigned int		mrm:1; //mod/reg/rm
	unsigned int		mrm_chk_mod:1; //does mod matter?
	unsigned int		mrm_mod3:1; //mod==3
	unsigned int		mrm_chk_reg:1; //does reg matter?
	unsigned int		mrm_reg:3; //value of reg
	const struct opcode_t*	op;
};

vector<struct opcode_pat_t> opflst;

void addlist_pattmrm(opcode_pat_t &opt,size_t parmi) {
	if (parmi < MAX_PATPARAM && opt.op->patparam[parmi] == CPUPPM_MODREGRM) {
		opcode_pat_t c_opt = opt;
		c_opt.mrm = 1;
		parmi++;

		for (unsigned int mod3=0;mod3<2;mod3++) {
			for (unsigned int reg=0;reg<8;reg++) {
				if (c_opt.mrm_chk_mod) c_opt.mrm_mod3=mod3;
				if (c_opt.mrm_chk_reg) c_opt.mrm_reg=reg;
				if ((c_opt.mrm_chk_mod || mod3 == 0) && (c_opt.mrm_chk_reg || reg == 0)) opflst.push_back(c_opt);
			}
		}
	}
	else {
		opflst.push_back(opt);
	}
}

void addlist_pattlb(opcode_pat_t &opt,size_t parmi) {
	if (opt.op->pattern_lb_mask.mask != 0) {
		assert(opt.op->pattern_sz != 0);
		assert(opt.pat.size() == opt.op->pattern_sz);
		assert((opt.pat[opt.pat.size()-1u] & opt.op->pattern_lb_mask.mask) == 0);

		opcode_pat_t c_opt = opt;
		for (unsigned int c=0;c <= c_opt.op->pattern_lb_mask.mask;c++) {
			c_opt.pat[c_opt.pat.size()-1u] = (c_opt.pat[c_opt.pat.size()-1u] & ~c_opt.op->pattern_lb_mask.mask) + c;
			if (c_opt.op->pattern_lb_mask.match & (1u << c)) addlist_pattmrm(c_opt,parmi);
		}
	}
	else {
		addlist_pattmrm(opt,parmi);
	}
}

void addlist(const struct opcode_t** oplist) {
	while (*oplist) {
		const struct opcode_t* op = *oplist++;
		opcode_pat_t opt = { {}, 0, 0, 0, 0, 0, NULL };

		if (op->pattern_sz == 0) continue;
		opt.pat.resize(op->pattern_sz);
		memcpy(opt.pat.data(),op->pattern,op->pattern_sz);
		opt.op = op;

		addlist_pattlb(opt,0);
	}
}

const char *param2str(const uint8_t p,const uint8_t s) {
	switch (p) {
		case CPUP_NONE: return "none";
		case CPUP_ZERO: return "0";
		case CPUP_ONE: return "1";
		case CPUP_IMMEDIATE: return "imm";
		case CPUP_IMMEDIATE_SX: return "immsx";
		case CPUP_MEM_RM: return "mem(rm)";
		case CPUP_MEM_IMM: return "mem(imm)";

		case CPUP_GREG_EAX:
			switch (s) {
				case CPUPS_BYTE: return "al";
				case CPUPS_WORD: return "ax";
				case CPUPS_DWORD: return "eax";
				case CPUPS_QWORD: return "rax";
				case CPUPS_NATIVEWORD: return "ax/al";
				default: return "a?";
			};
			break;

		case CPUP_GREG_ECX:
			switch (s) {
				case CPUPS_BYTE: return "cl";
				case CPUPS_WORD: return "cx";
				case CPUPS_DWORD: return "ecx";
				case CPUPS_QWORD: return "rcx";
				case CPUPS_NATIVEWORD: return "cx/cl";
				default: return "c?";
			};
			break;

		case CPUP_GREG_EDX:
			switch (s) {
				case CPUPS_BYTE: return "dl";
				case CPUPS_WORD: return "dx";
				case CPUPS_DWORD: return "edx";
				case CPUPS_QWORD: return "rdx";
				case CPUPS_NATIVEWORD: return "dx/dl";
				default: return "d?";
			};
			break;

		case CPUP_GREG_EBX:
			switch (s) {
				case CPUPS_BYTE: return "bl";
				case CPUPS_WORD: return "bx";
				case CPUPS_DWORD: return "ebx";
				case CPUPS_QWORD: return "rbx";
				case CPUPS_NATIVEWORD: return "bx/bl";
				default: return "b?";
			};
			break;

		case CPUP_GREG_ESP:
			switch (s) {
				case CPUPS_BYTE: return "ah";
				case CPUPS_WORD: return "sp";
				case CPUPS_DWORD: return "esp";
				case CPUPS_QWORD: return "rsp";
				case CPUPS_NATIVEWORD: return "sp/ah";
				default: return "sp?";
			};
			break;

		case CPUP_GREG_EBP:
			switch (s) {
				case CPUPS_BYTE: return "ch";
				case CPUPS_WORD: return "bp";
				case CPUPS_DWORD: return "ebp";
				case CPUPS_QWORD: return "rbp";
				case CPUPS_NATIVEWORD: return "bp/ch";
				default: return "bp?";
			};
			break;

		case CPUP_GREG_ESI:
			switch (s) {
				case CPUPS_BYTE: return "dh";
				case CPUPS_WORD: return "si";
				case CPUPS_DWORD: return "esi";
				case CPUPS_QWORD: return "rsi";
				case CPUPS_NATIVEWORD: return "si/cl";
				default: return "si?";
			};
			break;

		case CPUP_GREG_EDI:
			switch (s) {
				case CPUPS_BYTE: return "bh";
				case CPUPS_WORD: return "di";
				case CPUPS_DWORD: return "edi";
				case CPUPS_QWORD: return "rdi";
				case CPUPS_NATIVEWORD: return "di/bl";
				default: return "di?";
			};
			break;

		case CPUP_GREG_DXAX:
			switch (s) {
				case CPUPS_BYTE: return "ax";
				case CPUPS_WORD: return "dx:ax";
				case CPUPS_DWORD: return "edx:eax";
				case CPUPS_QWORD: return "rdx:rax";
				case CPUPS_NATIVEWORD: return "d:a/ax";
				default: return "d:a?";
			};
			break;

		case CPUP_GREG_REG: return "genreg(reg)";
		case CPUP_GREG_RM: return "genreg(r/m)";

		case CPUP_SREG_ES: return "es";
		case CPUP_SREG_CS: return "cs";
		case CPUP_SREG_SS: return "ss";
		case CPUP_SREG_DS: return "ds";
		case CPUP_SREG_FS: return "fs";
		case CPUP_SREG_GS: return "gs";
		case CPUP_SREG_REG: return "sreg(reg)";
		case CPUP_SREG_RM: return "sreg(r/m)";
	};

	return "?";
}

const char *paramsz2str(const uint8_t s) {
	switch (s) {
		case CPUPS_BYTE: return "b";
		case CPUPS_WORD: return "w";
		case CPUPS_DWORD: return "d";
		case CPUPS_QWORD: return "q";
		case CPUPS_NATIVEWORD: return "n/w";
		default: break;
	};

	return "?";
}

int main(int argc,char **argv) {
	addlist(oplist_gen_8086);

	printf("Opcodes:\n");
	for (const auto &o : opflst) {
		printf("  ");
		for (const auto &p : o.pat)
			printf("%02x ",p);

		for (size_t i=0;i < MAX_PATPARAM;i++) {
			if (o.op->patparam[i] == CPUPPM_NONE) break;
			switch (o.op->patparam[i]) {
				case CPUPPM_MODREGRM:
					printf("mod/reg/rm ");
					break;
				case CPUPPM_IMMEDIATEB:
					printf("imm8 ");
					break;
				case CPUPPM_IMMEDIATEW:
					printf("imm16 ");
					break;
				case CPUPPM_IMMEDIATED:
					printf("imm32 ");
					break;
				case CPUPPM_IMMEDIATENW:
					printf("immnw ");
					break;
				default:
					break;
			}
		}

		if (o.mrm && o.mrm_chk_mod)
			printf("(mod%s3) ",o.mrm_mod3?"==":"!=");
		if (o.mrm && o.mrm_chk_reg)
			printf("(reg=%u) ",o.mrm_reg);

		printf("-> '%s' ",o.op->opcode_name);

		if (o.op->p_dst.p != CPUP_NONE)
			printf("d:%s(%s) ",param2str(o.op->p_dst.p,o.op->p_dst.s),paramsz2str(o.op->p_dst.s));

		for (size_t i=0;i < MAX_SPARAM;i++) {
			if (o.op->p_src[i].p == CPUP_NONE) break;
			printf("s:%s(%s) ",param2str(o.op->p_src[i].p,o.op->p_src[i].s),paramsz2str(o.op->p_src[i].s));
		}

		if (o.op->flags & CPUFL_PREFIX_SEGMENT)
			printf("(prefix:segov) ");
		if (o.op->flags & CPUFL_REG_FROM_OPCODE)
			printf("(reg:opcode) ");

		printf("\n");
	}

	return 0;
}

