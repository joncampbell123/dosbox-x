#include "mmx.h"

extern Bit32u * lookupRMEAregd[256];

#define LoadMd(off) mem_readd_inline(off)
#define LoadMq(off) ((Bit64u)((Bit64u)mem_readd_inline(off+4)<<32 | (Bit64u)mem_readd_inline(off)))
#define SaveMd(off,val)	mem_writed_inline(off,val)
#define SaveMq(off,val) {mem_writed_inline(off,val&0xffffffff);mem_writed_inline(off+4,(val>>32)&0xffffffff);}

#define CASE_0F_MMX(opcode) case(opcode):
#define GetRM
#define GetEAa
#define Fetchb() imm
#define GetEArd	Bit32u * eard=lookupRMEAregd[rm];

static void gen_mmx_op(Bitu op, Bitu rm, Bitu imm = 0, PhysPt eaa = 0) {
	switch (op)
	{
#include "..\core_normal\prefix_0f_mmx.h"
	default:
		break;
	}
illegal_opcode:
	return;
}

static void dyn_mmx_op(Bitu op) {
	Bitu imm = 0;
	dyn_get_modrm();

	if (decode.modrm.mod < 3) {
		dyn_fill_ea();
		gen_call_function((void*)&gen_mmx_op, "%I%I%I%Ddr", op, decode.modrm.val, imm, DREG(EA));
	}
	else {
		if ((op == 0x71) || (op == 0x72) || (op == 0x73))
			decode_fetchb_imm(imm);
		gen_call_function((void*)&gen_mmx_op, "%I%I%I", op, decode.modrm.val, imm);
	}
}

static void dyn_mmx_emms() {
	gen_call_function((void*)&setFPUTagEmpty, "");
}