
#include "mmx.h"

MMX_reg mmxtmp;

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
void dyn_mmx_simple(Bit8u op, Bit8u modrm) {
	cache_addw(0x0F | (op << 8)); cache_addb(modrm);
}

// same but with imm8 also
void dyn_mmx_simple_imm8(Bit8u op, Bit8u modrm, Bit8u imm) {
	cache_addw(0x0F | (op << 8)); cache_addb(modrm); cache_addb(imm);
}

// mmx OP mm, mm/m64 template
void dyn_mmx_op(Bit8u op) {
	dyn_get_modrm();

	if (decode.modrm.mod < 3) {
		dyn_fill_ea();
		gen_call_function((void*)&MMX_LOAD_64, "%Ddr", DREG(EA));
		cache_addw(0x0F | (op << 8));
		cache_addb(0x05 | (decode.modrm.val & 0x38));
		cache_addd((uintptr_t)&mmxtmp);
	} else dyn_mmx_simple(op, decode.modrm.val);
}

// mmx SHIFT mm, imm8 template 
void dyn_mmx_shift_imm8(Bit8u op) {
	dyn_get_modrm();
	Bitu imm; decode_fetchb_imm(imm);
	
	dyn_mmx_simple_imm8(op, decode.modrm.val, imm);
}

// 0x6E - MOVD mm, r/m32
void dyn_mmx_movd_pqed() {
	dyn_get_modrm();

	if (decode.modrm.mod < 3) {
		// movd mm, m32 - resolve EA and load data
		dyn_fill_ea();
		// generate call to mmxtmp load
		gen_call_function((void*)&MMX_LOAD_32, "%Ddr", DREG(EA));
		// mmxtmp contains loaded value - finish by loading it to mm
		cache_addw(0x6E0F);	// movd  mm, m32
		cache_addb(0x05 | (decode.modrm.val & 0x38));
		cache_addd((uintptr_t)&mmxtmp);
	}
	else {
		// movd mm, r32 - r32->mmxtmp->mm
		// resolve dynreg
		DynReg *reg = &DynRegs[decode.modrm.rm];
		// resolve genreg
		GenReg *gr = FindDynReg(reg);
		// move from genreg to mmxtmp
		cache_addb(0x89);	// mov m32, r32
		cache_addb(0x05 | (gr->index << 3));
		cache_addd((uintptr_t)&mmxtmp);
		// mmxtmp contains loaded value - finish by loading it to mm
		cache_addw(0x6E0F);	// movd  mm, m32
		cache_addb(0x05 | (decode.modrm.val & 0x38));
		cache_addd((uintptr_t)&mmxtmp);
	}
}


// 0x6F - MOVQ mm, mm/m64
void dyn_mmx_movq_pqqq() {
	dyn_get_modrm(); 

	if (decode.modrm.mod < 3) {
		// movq mm, m64 - resolve EA and load data
		dyn_fill_ea();
		// generate call to mmxtmp load
		gen_call_function((void*)&MMX_LOAD_64, "%Ddr", DREG(EA));
		// mmxtmp contains loaded value - finish by loading it to mm
		cache_addw(0x6F0F);	// movq  mm, m64
		cache_addb(0x05 | (decode.modrm.val & 0x38));
		cache_addd((uintptr_t)&mmxtmp);
	}
	else {
		// movq mm, mm
		dyn_mmx_simple(0x6F, decode.modrm.val);
	}
}

// 0x7E - MOVD r/m32, mm 
void dyn_mmx_movd_edpq() {
	dyn_get_modrm();

	if (decode.modrm.mod < 3) {
		// movd m32, mm - resolve EA and load data
		dyn_fill_ea();
		// fill mmxtmp
		cache_addw(0x7E0F);	// movd  mm, m32
		cache_addb(0x05 | (decode.modrm.val & 0x38));
		cache_addd((uintptr_t)&mmxtmp);
		// generate call to mmxtmp store
		gen_call_function((void*)&MMX_STORE_32, "%Ddr", DREG(EA));
	}
	else {
		// movd r32, mm - mm->mmxtmp->r32
		// resolve dynreg
		DynReg *reg = &DynRegs[decode.modrm.rm];
		// resolve genreg
		GenReg *gr = FindDynReg(reg);
		// fill mmxtmp
		cache_addw(0x7E0F);	// movd  mm, m32
		cache_addb(0x05 | (decode.modrm.val & 0x38));
		cache_addd((uintptr_t)&mmxtmp);
		// move from mmxtmp to genreg
		cache_addb(0x8b);	// mov r32, m32
		cache_addb(0x05 | (gr->index << 3));
		cache_addd((uintptr_t)&mmxtmp);
		// mark dynreg as changed
		reg->flags |= DYNFLG_CHANGED;
	}
}

// 0x7F - MOVQ mm/m64, mm
void dyn_mmx_movq_qqpq() {
	dyn_get_modrm();

	if (decode.modrm.mod < 3) {
		// movq m64, mm - resolve EA and load data
		dyn_fill_ea();
		// fill mmxtmp
		cache_addw(0x7F0F);	// movq  mm, m64
		cache_addb(0x05 | (decode.modrm.val & 0x38));
		cache_addd((uintptr_t)&mmxtmp);
		// generate call to mmxtmp store
		gen_call_function((void*)&MMX_STORE_64, "%Ddr", DREG(EA));
	}
	else {
		// movq mm, mm
		dyn_mmx_simple(0x7F, decode.modrm.val);
	}
}

// 0x77 - EMMS
void dyn_mmx_emms() {
	cache_addw(0x770F);
}