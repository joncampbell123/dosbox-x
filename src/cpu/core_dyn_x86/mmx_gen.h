
#include "mmx.h"

static MMX_reg mmxtmp;

static void MMX_LOAD_32(PhysPt addr) {
	mmxtmp.ud.d0 = mem_readd(addr);
}

static void MMX_STORE_32(PhysPt addr) {
	mem_writed(addr, mmxtmp.ud.d0);
}

static void MMX_LOAD_64(PhysPt addr) {
	mmxtmp.ud.d0 = mem_readd(addr);
	mmxtmp.ud.d1 = mem_readd(addr + 4);
}

static void MMX_STORE_64(PhysPt addr) {
	mem_writed(addr, mmxtmp.ud.d0);
	mem_writed(addr + 4, mmxtmp.ud.d1);
}

// add simple instruction (that operates only with mm regs)
static void dyn_mmx_simple(uint8_t op, uint8_t modrm) {
	cache_addw(0x0F | (op << 8)); cache_addb(modrm);
}

// same but with imm8 also
static void dyn_mmx_simple_imm8(uint8_t op, uint8_t modrm, uint8_t imm) {
	cache_addw(0x0F | (op << 8)); cache_addb(modrm); cache_addb(imm);
}

static void dyn_mmx_mem(uint8_t op, Bitu reg=decode.modrm.reg, void* mem=&mmxtmp) {
#if C_TARGETCPU == X86
	cache_addw(0x0F | (op << 8));
	cache_addb(0x05|(reg<<3));
	cache_addd((uint32_t)(mem));
#else // X86_64
	opcode(reg).setabsaddr(mem).Emit16(0x0F | (op << 8));
#endif
}

// mmx OP mm, mm/m64 template
static void dyn_mmx_op(uint8_t op) {
	dyn_get_modrm();

	if (decode.modrm.mod < 3) {
		dyn_fill_ea();
		dyn_call_function_pagefault_check((void*)&MMX_LOAD_64, "%Ddr", DREG(EA));
		dyn_mmx_mem(op);
	} else dyn_mmx_simple(op, decode.modrm.val);
}

// mmx SHIFT mm, imm8 template 
static void dyn_mmx_shift_imm8(uint8_t op) {
	dyn_get_modrm();
	Bitu imm; decode_fetchb_imm(imm);
	
	dyn_mmx_simple_imm8(op, decode.modrm.val, imm);
}

// 0x6E - MOVD mm, r/m32
static void dyn_mmx_movd_pqed() {
	dyn_get_modrm();

	if (decode.modrm.mod < 3) {
		// movd mm, m32 - resolve EA and load data
		dyn_fill_ea();
		// generate call to mmxtmp load
		dyn_call_function_pagefault_check((void*)&MMX_LOAD_32, "%Ddr", DREG(EA));
		// mmxtmp contains loaded value - finish by loading it to mm
		dyn_mmx_mem(0x6E); // movd  mm, m32
	}
	else {
		// movd mm, r32 - r32->mmxtmp->mm
		gen_save_host((void*)&mmxtmp, &DynRegs[decode.modrm.rm], 4);
		// mmxtmp contains loaded value - finish by loading it to mm
		dyn_mmx_mem(0x6E); // movd  mm, m32
	}
}


// 0x6F - MOVQ mm, mm/m64
static void dyn_mmx_movq_pqqq() {
	dyn_get_modrm(); 

	if (decode.modrm.mod < 3) {
		// movq mm, m64 - resolve EA and load data
		dyn_fill_ea();
		// generate call to mmxtmp load
		dyn_call_function_pagefault_check((void*)&MMX_LOAD_64, "%Ddr", DREG(EA));
		// mmxtmp contains loaded value - finish by loading it to mm
		dyn_mmx_mem(0x6F); // movq  mm, m64
	}
	else {
		// movq mm, mm
		dyn_mmx_simple(0x6F, decode.modrm.val);
	}
}

// 0x7E - MOVD r/m32, mm 
static void dyn_mmx_movd_edpq() {
	dyn_get_modrm();

	if (decode.modrm.mod < 3) {
		// movd m32, mm - resolve EA and load data
		dyn_fill_ea();
		// fill mmxtmp
		dyn_mmx_mem(0x7E); // movd  mm, m32
		// generate call to mmxtmp store
		dyn_call_function_pagefault_check((void*)&MMX_STORE_32, "%Ddr", DREG(EA));
	}
	else {
		// movd r32, mm - mm->mmxtmp->r32
		// fill mmxtmp
		dyn_mmx_mem(0x7E); // movd  mm, m32
		// move from mmxtmp to genreg
		gen_load_host((void*)&mmxtmp, &DynRegs[decode.modrm.rm], 4);
	}
}

// 0x7F - MOVQ mm/m64, mm
static void dyn_mmx_movq_qqpq() {
	dyn_get_modrm();

	if (decode.modrm.mod < 3) {
		// movq m64, mm - resolve EA and load data
		dyn_fill_ea();
		// fill mmxtmp
		dyn_mmx_mem(0x7F); // movq  mm, m64
		// generate call to mmxtmp store
		dyn_call_function_pagefault_check((void*)&MMX_STORE_64, "%Ddr", DREG(EA));
	}
	else {
		// movq mm, mm
		dyn_mmx_simple(0x7F, decode.modrm.val);
	}
}

// 0x77 - EMMS
static void dyn_mmx_emms() {
	cache_addw(0x770F);
}