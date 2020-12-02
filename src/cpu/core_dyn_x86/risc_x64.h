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

#if defined(_WIN64)
enum {
	X64_REG_RBX,
	X64_REG_RDX,
	X64_REG_RCX,
	X64_REG_RAX,
	// volatiles
	X64_REG_R8,
	X64_REG_R9,
	X64_REG_R10,
	X64_REG_R11,
	// non-volatiles
	X64_REG_R12,
	X64_REG_R13,
	X64_REG_R14,
	X64_REG_R15,
	X64_REG_RSI,
	X64_REG_RDI,
	X64_REGS
};
static const int reg_args[4] = {X64_REG_RCX, X64_REG_RDX, X64_REG_R8, X64_REG_R9};
#define ARG0_REG 1
#define ARG1_REG 2
#define CALLSTACK 40
#else
enum {
	// (high)byte-accessible
	X64_REG_RBX,
	X64_REG_RCX,
	X64_REG_RDX,
	X64_REG_RAX,
	// volatiles
	X64_REG_RSI,
	X64_REG_RDI,
	X64_REG_R8,
	X64_REG_R9,
	X64_REG_R10,
	X64_REG_R11,
	// non-volatiles
	X64_REG_R12,
	X64_REG_R13,
	X64_REG_R14,
	X64_REG_R15,
	// delimiter
	X64_REGS
};
static const int reg_args[4] = {X64_REG_RDI, X64_REG_RSI, X64_REG_RDX, X64_REG_RCX};
#define ARG0_REG 7
#define ARG1_REG 6
#define CALLSTACK 8
#endif

static struct {
	bool flagsactive;
	Bitu last_used;
	GenReg * regs[X64_REGS];
} x64gen;

class opcode {
public:
	opcode(void) : is_word(false), imm_size(0), rex(0) {}
	opcode(int reg,bool dword=true,Bitu acc=1) : is_word(!dword), imm_size(0), rex(0) {
		setreg(reg, acc);
	}

	opcode& setword() {is_word=true; return *this;}
	opcode& set64(void) {rex|=0x48;return *this;}
	opcode& setimm(uint64_t _imm, int size) {imm=_imm;imm_size=size;return *this;}

	opcode& setreg(int r, Bitu acc=1); // acc: 0=low byte, 1=word/dword, 4=high byte
	opcode& setrm(int r, Bitu acc=1);
	opcode& setabsaddr(void* addr);
	opcode& setea(int rbase, int rscale=-1, Bitu scale=0, Bits off=0);

	void Emit8Reg(uint8_t op);
	void Emit8(uint8_t op);
	void Emit16(uint16_t op);

private:
	bool is_word;
	int reg;
	uint64_t imm;
	int imm_size;

	uint8_t rex, modrm, sib;
	Bits offset;

	void EmitImm(void) {
		switch(imm_size) {
		case 1: cache_addb((uint8_t)imm);break;
		case 2: cache_addw((uint16_t)imm);break;
		case 4: cache_addd((uint32_t)imm);break;
		case 8: cache_addq(imm);break;
		}
	}

	void EmitSibOffImm(void) {
		if (modrm<0xC0) {
			if ((modrm&7)==4) cache_addb(sib);
			switch (modrm>>6) {
			case 0:
				if ((modrm&7)==5) {
					// update offset to be RIP relative
					Bits diff = offset - (Bits)cache.pos - 4 - imm_size;
					if ((int32_t)diff == diff) offset = diff;
					else { // try 32-bit absolute address
						if ((int32_t)offset != offset) IllegalOption("opcode::Emit: bad RIP address");
						// change emitted modrm base from 5 to 4 (use sib)
						cache.pos[-1] -= 1; 
						cache_addb(0x25); // sib: [none+1*none+simm32]
					}
				} else if ((modrm&7)!=4 || (sib&7)!=5)
					break;
			case 2:	cache_addd((uint32_t)offset); break;
			case 1: cache_addb((uint8_t)offset); break;
			}
		}
		EmitImm();
	}
};

void opcode::Emit8Reg(uint8_t op) {
	if (is_word) cache_addb(0x66);
	if (reg>=8) rex |= 0x41;
	if (rex) cache_addb(rex);
	cache_addb(op|(reg&7));
	EmitImm();
}

void opcode::Emit8(uint8_t op) {
	if (is_word) cache_addb(0x66);
	if (rex) cache_addb(rex);
	cache_addw(op+(modrm<<8));
	EmitSibOffImm();
}

void opcode::Emit16(uint16_t op) {
	if (is_word) cache_addb(0x66);
	if (rex) cache_addb(rex);
	cache_addw(op);
	cache_addb(modrm);
	EmitSibOffImm();
}

opcode& opcode::setreg(int r, Bitu acc) {
	if (acc==4) {
		if (r>3 || rex) IllegalOption("opcode::setreg: cannot encode high byte");
		r += 4;
	}
	else if (acc==0 && r>3) rex |= 0x40;
	reg = r;
	return *this;
}

opcode& opcode::setrm(int r, Bitu acc) {
	if (reg>=8) rex |= 0x44;
	if (r>=8) rex |= 0x41;
	if (acc==4) {
		if (r>3 || rex) IllegalOption("opcode::setrm: cannot encode high byte");
		r += 4;
	}
	else if (acc==0 && r>3) rex |= 0x40;
	modrm = 0xC0+((reg&7)<<3)+(r&7);
	return *this;
}

opcode& opcode::setabsaddr(void* addr) {
	/* address must be in one of three ranges (in order of preference:
	 * &cpu_regs +/- 2GB (RBP relative) enc: modrm+1 or 4 bytes
	 * cache.pos +/- 2GB (RIP relative) enc: modrm+4 bytes
	 * < 0x80000000 or >= 0xFFFFFFFF80000000 (signed 32-bit absolute) enc: modrm+sib+4 bytes
	 */
	if (reg>=8) rex |= 0x44;
	modrm = (reg&7)<<3;
	offset = (Bits)addr - (Bits)&cpu_regs;
	if ((int32_t)offset == offset) { // [RBP+(int8_t/int32_t)]
		if ((int8_t)offset == offset) modrm += 0x45;
		else modrm += 0x85;
	} else {
		offset = (Bits)addr;
		modrm += 5; // [RIP+int32_t] or [abs int32_t]
	}

	return *this;
}

opcode& opcode::setea(int rbase, int rscale, Bitu scale, Bits off) {
	if (reg>=8) rex |= 0x44;
	if (rbase>=8) rex |= 0x41, rbase &= 7;
	if (rscale>=8) rex |= 0x42, rscale &= 7;
	modrm = (reg&7)<<3;
	offset = off;

	if (rbase<0 || rscale>=0 || rbase==4) { // sib required
		modrm += 4;
		if (rscale>=0) sib = (uint8_t)((scale<<6)+(rscale<<3));
		else sib = 4<<3;
		if (rbase>=0) sib += rbase;
		else sib += 5;
	} else modrm += rbase;

	if (rbase==5 || (off && rbase>=0)) {
		if ((int8_t)off == off) modrm += 0x40;
		else modrm += 0x80;
	}

	return *this;
}


class GenReg {
public:
	GenReg(uint8_t _index) : index(_index) {
		notusable=false;dynreg=0;
	}
	DynReg  * dynreg;
	Bitu last_used;			//Keeps track of last assigned regs 
	const uint8_t index;
	bool notusable;
	void Load(DynReg * _dynreg,bool stale=false) {
		if (!_dynreg) return;
		if (GCC_UNLIKELY((Bitu)dynreg)) Clear();
		dynreg=_dynreg;
		last_used=x64gen.last_used;
		dynreg->flags&=~DYNFLG_CHANGED;
		dynreg->genreg=this;
		if ((!stale) && (dynreg->flags & (DYNFLG_LOAD|DYNFLG_ACTIVE))) {
			opcode(index).setabsaddr(dynreg->data).Emit8(0x8B); // mov r32, []
		}
		dynreg->flags|=DYNFLG_ACTIVE;
	}
	void Save(void) {
		if (GCC_UNLIKELY(!((Bitu)dynreg))) IllegalOption("GenReg->Save");
		dynreg->flags&=~DYNFLG_CHANGED;
		opcode(index).setabsaddr(dynreg->data).Emit8(0x89); // mov [], r32
	}
	void Release(void) {
		if (GCC_UNLIKELY(!((Bitu)dynreg))) return;
		if (dynreg->flags&DYNFLG_CHANGED && dynreg->flags&DYNFLG_SAVE) {
			Save();
		}
		dynreg->flags&=~(DYNFLG_CHANGED|DYNFLG_ACTIVE);
		dynreg->genreg=0;dynreg=0;
	}
	void Clear(void) {
		if (!dynreg) return;
		if (dynreg->flags&DYNFLG_CHANGED) {
			Save();
		}
		dynreg->genreg=0;dynreg=0;
	}
};

static BlockReturn gen_runcodeInit(uint8_t *code);
static BlockReturn (*gen_runcode)(uint8_t *code) = gen_runcodeInit;

static BlockReturn gen_runcodeInit(uint8_t *code) {
	uint8_t* oldpos = cache.pos;
	cache.pos = &cache_code_link_blocks[128];
	gen_runcode = (BlockReturn(*)(uint8_t*))cache_rwtox(cache.pos);

	opcode(5).Emit8Reg(0x50);  // push rbp
	opcode(15).Emit8Reg(0x50); // push r15
	opcode(14).Emit8Reg(0x50); // push r14
	// mov rbp, &cpu_regs
    /* NTS: For VS2019, mere typecasting won't correctly detect whether the pointer is small enough to fit in 32 bits.
            We have to mask and compare. If the pointer is above 4GB and the wrong choice is made, dynamic core will crash. --J.C. */
	if ((((Bitu)&cpu_regs) && (Bitu)0xFFFFFFFFul) == (Bitu)&cpu_regs) opcode(5).setimm((Bitu)&cpu_regs,4).Emit8Reg(0xB8);
	else opcode(5).set64().setimm((Bitu)&cpu_regs,8).Emit8Reg(0xB8);
	opcode(13).Emit8Reg(0x50); // push r13
	opcode(12).Emit8Reg(0x50); // push r12
	opcode(3).Emit8Reg(0x50);  // push rbx
	opcode(0).setea(5,-1,0,offsetof(CPU_Regs,flags)).Emit8(0x8B); // mov eax, [reg_flags(rbp)]
#if defined(_WIN64)
	opcode(7).Emit8Reg(0x50);  // push rdi
	opcode(6).Emit8Reg(0x50);  // push rsi
#endif
	opcode(15).set64().setrm(4).Emit8(0x8B);  // mov r15, rsp
	opcode(0).setimm(FMASK_TEST,4).Emit8Reg(0x25); // and eax, FMASK_TEST
	cache_addb(0x48);cache_addw(0x158D); // lea rdx, [rip+simm32]
	uint8_t *diff = cache.pos;
	cache_addd(0);
	opcode(4).set64().setrm(4).setimm(~15,1).Emit8(0x83); // and rsp, ~15
	opcode(15).Emit8Reg(0x50);  // push r15
	opcode(2).Emit8Reg(0x50);   // push rdx
	opcode(5).set64().setrm(4).setimm(CALLSTACK*2,1).Emit8(0x83); // sub rsp, 16/80
	opcode(0).setea(4,-1,0,CALLSTACK).Emit8(0x89);  // mov [rsp+8/40], eax
	opcode(4).setrm(ARG0_REG).Emit8(0xFF);   // jmp ARG0

	*(uint32_t*)diff = (uint32_t)(cache.pos - diff - 4);
	// eax = return value, ecx = flags
	opcode(1).setea(5,-1,0,offsetof(CPU_Regs,flags)).Emit8(0x33); // xor ecx, reg_flags
	opcode(4).setrm(1).setimm(FMASK_TEST,4).Emit8(0x81);          // and ecx,FMASK_TEST
	opcode(1).setea(5,-1,0,offsetof(CPU_Regs,flags)).Emit8(0x31); // xor reg_flags, ecx

	opcode(4).set64().setea(4,-1,0,CALLSTACK).Emit8(0x8B); // mov rsp, [rsp+8/40]
#if defined(_WIN64)
	opcode(6).Emit8Reg(0x58);  // pop rsi
	opcode(7).Emit8Reg(0x58);  // pop rdi
#endif
	opcode(3).Emit8Reg(0x58);  // pop rbx
	opcode(12).Emit8Reg(0x58); // pop r12
	opcode(13).Emit8Reg(0x58); // pop r13
	opcode(14).Emit8Reg(0x58); // pop r14
	opcode(15).Emit8Reg(0x58); // pop r15
	opcode(5).Emit8Reg(0x58);  // pop rbp
	cache_addb(0xc3);          // ret

	cache.pos = oldpos;
	return gen_runcode(code);
}

static GenReg * FindDynReg(DynReg * dynreg,bool stale=false) {
	x64gen.last_used++;
	if (dynreg->genreg) {
		dynreg->genreg->last_used=x64gen.last_used;
		return dynreg->genreg;
	}
	/* Find best match for selected global reg */
	Bits i;
	Bits first_used,first_index;
	first_used=-1;
	if (dynreg->flags & DYNFLG_HAS8) {
		/* Has to be rax,rbx,rcx,rdx */
		for (i=first_index=0;i<=3;i++) {
			GenReg * genreg=x64gen.regs[i];
			if (genreg->notusable) continue;
			if (!(genreg->dynreg)) {
				genreg->Load(dynreg,stale);
				return genreg;
			}
			if (genreg->last_used<(Bitu)first_used) {
				first_used=genreg->last_used;
				first_index=i;
			}
		}
	} else {
		for (i=first_index=X64_REGS-1;i>=0;i--) {
			GenReg * genreg=x64gen.regs[i];
			if (genreg->notusable) continue;
			if (!(genreg->dynreg)) {
				genreg->Load(dynreg,stale);
				return genreg;
			}
			if (genreg->last_used<(Bitu)first_used) {
				first_used=genreg->last_used;
				first_index=i;
			}
		}
	}
	/* No free register found use earliest assigned one */
	GenReg * newreg=x64gen.regs[first_index];
	newreg->Load(dynreg,stale);
	return newreg;
}

static uint8_t GetNextReg(bool low=false) {
	Bitu i;
	Bitu first_used,first_index;
	first_used=x64gen.last_used+1;
	for (i=first_index=0;i<X64_REGS;i++) {
		GenReg* genreg=x64gen.regs[i];
		if (genreg->notusable) continue;
		if (low && genreg->index>=8) continue;
		if (!(genreg->dynreg)) {
			first_index=i;
			break;
		}
		if (genreg->last_used<first_used) {
			first_used = genreg->last_used;
			first_index = i;
		}
	}
	x64gen.regs[first_index]->Clear();
	return x64gen.regs[first_index]->index;
}

static void ForceDynReg(GenReg * genreg,DynReg * dynreg) {
	genreg->last_used = ++x64gen.last_used;
	if (dynreg->genreg) {
		if (dynreg->genreg==genreg) return;
		if (genreg->dynreg) genreg->Clear();
		// mov dst32, src32
		opcode(genreg->index).setrm(dynreg->genreg->index).Emit8(0x8B);
		dynreg->genreg->dynreg=0;
		dynreg->genreg=genreg;
		genreg->dynreg=dynreg;
	} else genreg->Load(dynreg);
}

static void gen_preloadreg(DynReg * dynreg) {
	FindDynReg(dynreg);
}

static void gen_releasereg(DynReg * dynreg) {
	GenReg * genreg=dynreg->genreg;
	if (genreg) genreg->Release();
	else dynreg->flags&=~(DYNFLG_ACTIVE|DYNFLG_CHANGED);
}

static void gen_setupreg(DynReg * dnew,DynReg * dsetup) {
	dnew->flags=dsetup->flags;
	if (dnew->genreg==dsetup->genreg) return;
	/* Not the same genreg must be wrong */
	if (dnew->genreg) {
		/* Check if the genreg i'm changing is actually linked to me */
		if (dnew->genreg->dynreg==dnew) dnew->genreg->dynreg=0;
	}
	dnew->genreg=dsetup->genreg;
	if (dnew->genreg) dnew->genreg->dynreg=dnew;
}

static void gen_synchreg(DynReg * dnew,DynReg * dsynch) {
	/* First make sure the registers match */
	if (dnew->genreg!=dsynch->genreg) {
		if (dnew->genreg) dnew->genreg->Clear();
		if (dsynch->genreg) {
			dsynch->genreg->Load(dnew);
		}
	}
	/* Always use the loadonce flag from either state */
	dnew->flags|=(dsynch->flags & dnew->flags&DYNFLG_ACTIVE);
	if ((dnew->flags ^ dsynch->flags) & DYNFLG_CHANGED) {
		/* Ensure the changed value gets saved */	
		if (dnew->flags & DYNFLG_CHANGED) {
			dnew->genreg->Save();
		} else dnew->flags|=DYNFLG_CHANGED;
	}
}

static void gen_needflags(void) {
	if (!x64gen.flagsactive) {
		x64gen.flagsactive=true;
		opcode(0).set64().setrm(4).setimm(CALLSTACK,1).Emit8(0x83); // add rsp,8/40
		cache_addb(0x9d);		//POPFQ
	}
}

static void gen_protectflags(void) {
	if (x64gen.flagsactive) {
		x64gen.flagsactive=false;
		cache_addb(0x9c);		//PUSHFQ
		opcode(4).set64().setea(4,-1,0,-(CALLSTACK)).Emit8(0x8D); // lea rsp, [rsp-8/40]
	}
}

static void gen_discardflags(void) {
	if (!x64gen.flagsactive) {
		x64gen.flagsactive=true;
		opcode(0).set64().setrm(4).setimm(CALLSTACK+8,1).Emit8(0x83); // add rsp,16/48
	}
}

static void gen_needcarry(void) {
	if (!x64gen.flagsactive) {
		x64gen.flagsactive=true;
		opcode(4).setea(4,-1,0,CALLSTACK).setimm(0,1).Emit16(0xBA0F);  // bt [rsp+8/40], 0
		opcode(4).set64().setea(4,-1,0,CALLSTACK+8).Emit8(0x8D);       // lea rsp, [rsp+16/48]
	}
}

static void gen_setzeroflag(void) {
	if (x64gen.flagsactive) IllegalOption("gen_setzeroflag");
	opcode(1).setea(4,-1,0,CALLSTACK).setimm(0x40,1).Emit8(0x83); // or dword [rsp+8/40],0x40
}

static void gen_clearzeroflag(void) {
	if (x64gen.flagsactive) IllegalOption("gen_clearzeroflag");
	opcode(4).setea(4,-1,0,CALLSTACK).setimm(~0x40,1).Emit8(0x83); // and dword [rsp+8/40],~0x40
}

static bool skip_flags=false;

static void set_skipflags(bool state) {
	if (!state) gen_discardflags();
	skip_flags=state;
}

static void gen_reinit(void) {
	x64gen.last_used=0;
	x64gen.flagsactive=false;
	for (Bitu i=0;i<X64_REGS;i++) {
		x64gen.regs[i]->dynreg=0;
	}
}

static void gen_load_host(void * data,DynReg * dr1,Bitu size) {
	opcode op = opcode(FindDynReg(dr1,true)->index).setabsaddr(data);
	switch (size) {
	case 1: // movzx r32, byte[]
		op.Emit16(0xB60F);
		break;
	case 2: // movzx r32, word[]
		op.Emit16(0xB70F);
		break;
	case 4: // mov r32, []
		op.Emit8(0x8B);
		break;
	default:
		IllegalOption("gen_load_host");
	}
	dr1->flags|=DYNFLG_CHANGED;
}

static void gen_mov_host(void * data,DynReg * dr1,Bitu size,Bitu di1=0) {
	int idx = FindDynReg(dr1,size==4)->index;
	opcode op;
	uint8_t tmp;
	switch (size) {
	case 1:
		op.setreg(idx,di1);
		tmp = 0x8A; // mov r8, []
		break;
	case 2: op.setword(); // mov r16, []
	case 4: op.setreg(idx);
		tmp = 0x8B; // mov r32, []
		break;
	default:
		IllegalOption("gen_mov_host");
	}
	op.setabsaddr(data).Emit8(tmp);
	dr1->flags|=DYNFLG_CHANGED;
}

static void gen_load_arg_reg(int argno,DynReg *dr,const char *s) {
	GenReg *gen = x64gen.regs[reg_args[argno]];
	GenReg *src = dr->genreg;
	opcode op(gen->index);

	if (*s=='r') {
		s++;
		gen_releasereg(dr);
	}

	gen->Clear();

	switch (*s) {
		case 'h':
			if (src) {
				if (src->index>3 || gen->index>3) {
					// shld r32,r32,24
					opcode(src->index).setimm(24,1).setrm(gen->index).Emit16(0xA40F);
					op.setrm(gen->index,0);
				} else op.setrm(src->index,4);
			} else op.setabsaddr(((uint8_t*)dr->data)+1);
			op.Emit16(0xB60F); // movzx r32, r/m8
			break;
		case 'l':
			if (src) op.setrm(src->index,0);
			else op.setabsaddr(dr->data);
			op.Emit16(0xB60F); // movzx r32, r/m8
			break;
		case 'w':
			if (src) op.setrm(src->index);
			else op.setabsaddr(dr->data);
			op.Emit16(0xB70F); // movzx r32, r/m16
			break;
		case 'd':
			if (src) {
				if (src != gen) op.setrm(src->index).Emit8(0x8B);
			} else op.setabsaddr(dr->data).Emit8(0x8B);
			break;
		default: 
			IllegalOption("gen_load_arg_reg param:DREG");
	}
}

static void gen_load_imm(int index,Bitu imm) {
	if (imm==0)
		opcode(index).setrm(index).Emit8(0x33); // xor r32,r32
	else if ((uint32_t)imm==imm)
		opcode(index).setimm(imm,4).Emit8Reg(0xB8); // MOV r32, imm32
	else if ((int32_t)imm==(Bits)imm)
		opcode(0).set64().setimm(imm,4).setrm(index).Emit8(0xC7); // mov r64, simm32
	else
		opcode(index).set64().setabsaddr((void*)imm).Emit8(0x8D); // lea r64, [imm]
}

static void gen_dop_byte(DualOps op,DynReg * dr1,Bitu di1,DynReg * dr2,Bitu di2) {
	uint8_t tmp;
	opcode i(FindDynReg(dr1)->index,true,di1);
	i.setrm(FindDynReg(dr2)->index,di2);

	switch (op) {
	case DOP_ADD:	tmp=0x02; break;
	case DOP_ADC:	tmp=0x12; break;
	case DOP_SUB:	tmp=0x2a; break;
	case DOP_SBB:	tmp=0x1a; break;
	case DOP_CMP:	tmp=0x3a; goto nochange;
	case DOP_XOR:	tmp=0x32; break;
	case DOP_AND:	tmp=0x22; if ((dr1==dr2) && (di1==di2)) goto nochange; break;
	case DOP_OR:	tmp=0x0a; if ((dr1==dr2) && (di1==di2)) goto nochange; break;
	case DOP_TEST:	tmp=0x84; goto nochange;
	case DOP_MOV:	if ((dr1==dr2) && (di1==di2)) return; tmp=0x8a; break;
	case DOP_XCHG:	if ((dr1==dr2) && (di1==di2)) return;
		tmp=0x86; dr2->flags|=DYNFLG_CHANGED; break;
	default:
		IllegalOption("gen_dop_byte");
	}
	dr1->flags|=DYNFLG_CHANGED;
nochange:
	i.Emit8(tmp);
}

static void gen_dop_byte_imm(DualOps op,DynReg * dr1,Bitu di1,Bitu imm) {
	uint8_t tmp=0x80;
	int dst = FindDynReg(dr1)->index;
	opcode i;
	i.setimm(imm,1);
	imm &= 0xff;

	switch (op) {
	case DOP_ADD:	i.setreg(0); if (!imm) goto nochange; break;
	case DOP_ADC:	i.setreg(2); break;
	case DOP_SUB:	i.setreg(5); if (!imm) goto nochange; break;
	case DOP_SBB:	i.setreg(3); break;
	case DOP_CMP:	i.setreg(7); goto nochange;	//Doesn't change
	case DOP_XOR:	i.setreg(6); if (!imm) goto nochange; break;
	case DOP_AND:	i.setreg(4); if (imm==255) goto nochange; break;
	case DOP_OR:	i.setreg(1); if (!imm) goto nochange; break;
	case DOP_TEST:	i.setreg(0);tmp=0xF6;goto nochange;
	case DOP_MOV:	i.setreg(dst,di1).Emit8Reg(0xB0);
					dr1->flags|=DYNFLG_CHANGED;
					return;
	default:
		IllegalOption("gen_dop_byte_imm");
	}
	dr1->flags|=DYNFLG_CHANGED;
nochange:
	i.setrm(dst,di1).Emit8(tmp);
}

static void gen_dop_byte_imm_mem(DualOps op,DynReg * dr1,Bitu di1,void* data) {
	opcode i;
	Bits addr = (Bits)data;
	Bits rbpdiff = addr - (Bits)&cpu_regs;
	Bits ripdiff = addr - (Bits)cache.pos;
	if (ripdiff<0) ripdiff = ~ripdiff+32;
	if ((int32_t)addr==addr || (int32_t)rbpdiff==rbpdiff || ripdiff < 0x7FFFFFE0ll)
		i = opcode(FindDynReg(dr1)->index,true,di1).setabsaddr(data);
	else {
		GenReg* dst = FindDynReg(dr1);
		dst->notusable=true;
		int src = GetNextReg(di1!=0);
		dst->notusable=false;
		if ((uint32_t)addr == (Bitu)addr) opcode(src).setimm(addr,4).Emit8Reg(0xB8);
		else opcode(src).setimm(addr,8).set64().Emit8Reg(0xB8);
		i = opcode(dst->index,true,di1).setea(src);
	}

	uint8_t tmp;
	switch (op) {
	case DOP_ADD:	tmp=0x02; break;
	case DOP_ADC:	tmp=0x12; break;
	case DOP_SUB:	tmp=0x2a; break;
	case DOP_SBB:	tmp=0x1a; break;
	case DOP_CMP:	tmp=0x3a; goto nochange;	//Doesn't change
	case DOP_XOR:	tmp=0x32; break;
	case DOP_AND:	tmp=0x22; break;
	case DOP_OR:	tmp=0x0a; break;
	case DOP_TEST:	tmp=0x84; goto nochange;	//Doesn't change
	case DOP_MOV:	tmp=0x8A; break;
	default:
		IllegalOption("gen_dop_byte_imm_mem");
	}
	dr1->flags|=DYNFLG_CHANGED;
nochange:
	i.Emit8(tmp);
}

static void gen_sop_byte(SingleOps op,DynReg * dr1,Bitu di1) {
	uint8_t tmp;
	int dst = FindDynReg(dr1)->index;
	opcode i;

	switch (op) {
	case SOP_INC: i.setreg(0);tmp=0xFE; break;
	case SOP_DEC: i.setreg(1);tmp=0xFE; break;
	case SOP_NOT: i.setreg(2);tmp=0xF6; break;
	case SOP_NEG: i.setreg(3);tmp=0xF6; break;
	default:
		IllegalOption("gen_sop_byte");
	}
	i.setrm(dst,di1).Emit8(tmp);
	dr1->flags|=DYNFLG_CHANGED;
}

static void gen_extend_word(bool sign,DynReg * ddr,DynReg * dsr) {
	if (ddr==dsr && dsr->genreg==NULL)
		opcode(FindDynReg(ddr,true)->index).setabsaddr(dsr->data).Emit16(sign ? 0xBF0F:0xB70F);
	else {
		int src = FindDynReg(dsr)->index;
		int dst = FindDynReg(ddr,true)->index;
		if (sign && (src|dst)==0) cache_addb(0x98); // cwde
		else opcode(dst).setrm(src).Emit16(sign ? 0xBF0F:0xB70F); // movsx/movzx dst32, src16
	}

	ddr->flags|=DYNFLG_CHANGED;
}

static void gen_extend_byte(bool sign,bool dword,DynReg * ddr,DynReg * dsr,Bitu dsi) {
	if (ddr==dsr && dword && dsr->genreg==NULL) {
		opcode op = opcode(FindDynReg(ddr,true)->index);
		if (dsi) op.setabsaddr((void*)(((uint8_t*)dsr->data)+1));
		else op.setabsaddr(dsr->data);
		op.Emit16(sign ? 0xBE0F:0xB60F); // movsx/movzx r32,m8
	} else {
		int src = FindDynReg(dsr)->index;
		int dst = FindDynReg(ddr,dword)->index;
		if (dsi && (src>3 || dst>=8)) { // high-byte + REX = extra work required
			// high-byte + REX prefix = extra work required:
			// move source high-byte to dest low-byte then extend dest
			gen_protectflags(); // shld changes flags, movzx/movsx does not

			// shld r16, r16, 8
			opcode(src,false).setimm(8,1).setrm(dst).Emit16(0xA40F);
			src = dst;
			dsi = 0;
		}
		if (sign && !dword && (src|dst|dsi)==0) cache_addw(0x9866); // cbw
		else opcode(dst,dword).setrm(src,dsi).Emit16(sign ? 0xBE0F:0xB60F);
	}
	ddr->flags|=DYNFLG_CHANGED;
}

static void gen_lea(DynReg * ddr,DynReg * dsr1,DynReg * dsr2,Bitu scale,Bits imm) {
	if (ddr==dsr1 && dsr2==NULL && !imm)
		return;
	if (ddr==dsr2 && dsr1==NULL) {
		if (!scale && !imm) 
			return;
		else if (scale<2) {
			// change [2*reg] to [reg+reg]
			// or [0+1*reg] to [reg+0*reg]
			// (index with no base requires 32-bit offset)
			dsr1 = dsr2;
			if (!scale) dsr2 = NULL;
			else scale = 0;
		}
	}

	GenReg * gdr=FindDynReg(ddr,ddr!=dsr1 && ddr!=dsr2);

	int idx1 = dsr1 ? FindDynReg(dsr1)->index : -1;
	int idx2 = dsr2 ? FindDynReg(dsr2)->index : -1;

	if (idx1==13 && dsr2 && idx2!=13 && !scale && !imm) {
		// use r13 as index instead of base to avoid mandatory offset
		int s = idx1;
		idx1 = idx2;
		idx2 = s;
	}

	opcode(gdr->index).setea(idx1, idx2, scale, imm).Emit8(0x8D);
	ddr->flags|=DYNFLG_CHANGED;
}

static void gen_dop_word(DualOps op,bool dword,DynReg * dr1,DynReg * dr2) {
	uint8_t tmp;
	GenReg *gr2 = FindDynReg(dr2);
	GenReg *gr1 = FindDynReg(dr1,dword && op==DOP_MOV);

	switch (op) {
	case DOP_ADD:	tmp=0x03; break;
	case DOP_ADC:	tmp=0x13; break;
	case DOP_SUB:	tmp=0x2b; break;
	case DOP_SBB:	tmp=0x1b; break;
	case DOP_CMP:	tmp=0x3b; goto nochange;
	case DOP_XOR:	tmp=0x33; break;
	case DOP_AND:	tmp=0x23; if (dr1==dr2) goto nochange; break;
	case DOP_OR:	tmp=0x0b; if (dr1==dr2) goto nochange; break;
	case DOP_TEST:	tmp=0x85; goto nochange;
	case DOP_MOV:	if (dr1==dr2) return; tmp=0x8b; break;
	case DOP_XCHG:	if (dr1==dr2) return;
		dr2->flags|=DYNFLG_CHANGED;
		if (dword && !((dr1->flags&DYNFLG_HAS8) ^ (dr2->flags&DYNFLG_HAS8))) {
			dr1->genreg=gr2;gr2->dynreg=dr1;
			dr2->genreg=gr1;gr1->dynreg=dr2;
			dr1->flags|=DYNFLG_CHANGED;
			return;
		}
		tmp=0x87;
		break;
	default:
		IllegalOption("gen_dop_word");
	}
	dr1->flags|=DYNFLG_CHANGED;
nochange:
	opcode(gr1->index,dword).setrm(gr2->index).Emit8(tmp);
}

static void gen_dop_word_imm(DualOps op,bool dword,DynReg * dr1,Bits imm) {
	uint8_t tmp=0x81;
	int dst = FindDynReg(dr1,dword && op==DOP_MOV)->index;
	opcode i;
	if (!dword) {
		i.setword();
		imm = (int16_t)imm;
	} else imm = (int32_t)imm;
	if (op <= DOP_OR && (int8_t)imm==imm) {
		i.setimm(imm, 1);
		tmp = 0x83;
	} else i.setimm(imm, dword?4:2);

	switch (op) {
	case DOP_ADD:	i.setreg(0); if (!imm) goto nochange; break; 
	case DOP_ADC:	i.setreg(2); break;
	case DOP_SUB:	i.setreg(5); if (!imm) goto nochange; break;
	case DOP_SBB:	i.setreg(3); break;
	case DOP_CMP:	i.setreg(7); goto nochange;	//Doesn't change
	case DOP_XOR:	i.setreg(6); if (!imm) goto nochange; break;
	case DOP_AND:	i.setreg(4); if (imm==-1) goto nochange; break;
	case DOP_OR:	i.setreg(1); if (!imm) goto nochange; break;
	case DOP_TEST:	i.setreg(0);tmp=0xF7; goto nochange;	//Doesn't change
	case DOP_MOV:	i.setreg(dst).Emit8Reg(0xB8); dr1->flags|=DYNFLG_CHANGED; return;
	default:
		IllegalOption("gen_dop_word_imm");
	}
	dr1->flags|=DYNFLG_CHANGED;
nochange:
	i.setrm(dst).Emit8(tmp);
}

static void gen_dop_word(DualOps op,DynReg *dr1,opcode &i) {
	uint8_t tmp;
	switch (op) {
	case DOP_ADD:	tmp=0x03; break; 
	case DOP_ADC:	tmp=0x13; break;
	case DOP_SUB:	tmp=0x2b; break;
	case DOP_SBB:	tmp=0x1b; break;
	case DOP_CMP:	tmp=0x3b; goto nochange;	//Doesn't change
	case DOP_XOR:	tmp=0x33; break;
	case DOP_AND:	tmp=0x23; break;
	case DOP_OR:	tmp=0x0b; break;
	case DOP_TEST:	tmp=0x85; goto nochange;	//Doesn't change
	case DOP_MOV:   tmp=0x8b; break;
	case DOP_XCHG:  tmp=0x87; break;
	default:
		IllegalOption("gen_dop_word0");
	}
	dr1->flags|=DYNFLG_CHANGED;
nochange:
	i.Emit8(tmp);
}

static void gen_dop_word_var(DualOps op,bool dword,DynReg * dr1,void* drd) {
	opcode i = opcode(FindDynReg(dr1,dword && op==DOP_MOV)->index,dword).setabsaddr(drd);
	gen_dop_word(op,dr1,i);
}

static void gen_dop_word_imm_mem(DualOps op,bool dword,DynReg * dr1,void* data) {
	opcode i;
	Bits addr = (Bits)data;
	Bits rbpdiff = addr - (Bits)&cpu_regs;
	Bits ripdiff = addr - (Bits)cache.pos;
	if (ripdiff<0) ripdiff = ~ripdiff+32;
	if ((int32_t)addr==addr || (int32_t)rbpdiff==rbpdiff || ripdiff < 0x7FFFFFE0ll)
		i = opcode(FindDynReg(dr1,dword && op==DOP_MOV)->index,dword).setabsaddr(data);
	else if (dword && op==DOP_MOV) {
		if (dr1->genreg) dr1->genreg->dynreg=0;
		x64gen.regs[X64_REG_RAX]->Load(dr1,true);
		if ((uint32_t)addr == (Bitu)addr) {
			cache_addb(0x67);
			opcode(0).setimm(addr,4).Emit8Reg(0xA1);
		} else opcode(0).setimm(addr,8).Emit8Reg(0xA1);
		dr1->flags|=DYNFLG_CHANGED;
		return;
	} else {
		GenReg* dst = FindDynReg(dr1,false);
		dst->notusable=true;
		int src = GetNextReg();
		dst->notusable=false;
		if ((uint32_t)addr == (Bitu)addr) opcode(src).setimm(addr,4).Emit8Reg(0xB8);
		else opcode(src).setimm(addr,8).set64().Emit8Reg(0xB8);
		i = opcode(dst->index,dword).setea(src);
	}
	gen_dop_word(op,dr1,i);
}

static void gen_lea_imm_mem(DynReg * ddr,DynReg * dsr,void* data) {
	gen_dop_word_imm_mem(DOP_MOV,true,ddr,data);
	gen_lea(ddr, ddr, dsr, 0, 0);
}

static void gen_imul_word(bool dword,DynReg * dr1,DynReg * dr2) {
	// dr1 = dr1*dr2
	opcode(FindDynReg(dr1)->index,dword).setrm(FindDynReg(dr2)->index).Emit16(0xAF0F);
	dr1->flags|=DYNFLG_CHANGED;
}

static void gen_imul_word_imm(bool dword,DynReg * dr1,DynReg * dr2,Bits imm) {
	// dr1 = dr2*imm
	opcode op;
	if (dr1==dr2 && dword && dr1->genreg==NULL)
		op = opcode(FindDynReg(dr1,true)->index).setabsaddr(dr2->data);
	else
		op = opcode(FindDynReg(dr1,dword&&dr1!=dr2)->index,dword).setrm(FindDynReg(dr2)->index);

	if ((int8_t)imm==imm) op.setimm(imm,1).Emit8(0x6B);
	else op.setimm(imm,dword?4:2).Emit8(0x69);
	dr1->flags|=DYNFLG_CHANGED;
}

static void gen_sop_word(SingleOps op,bool dword,DynReg * dr1) {
	opcode i;
	uint8_t tmp;
	if (!dword) i.setword();
	switch (op) {
	case SOP_INC: i.setreg(0);tmp=0xFF;break;
	case SOP_DEC: i.setreg(1);tmp=0xFF;break;
	case SOP_NOT: i.setreg(2);tmp=0xF7;break;
	case SOP_NEG: i.setreg(3);tmp=0xF7;break;
	default:
		IllegalOption("gen_sop_word");
	}
	i.setrm(FindDynReg(dr1)->index).Emit8(tmp);
	dr1->flags|=DYNFLG_CHANGED;
}

static void gen_shift_byte_cl(Bitu op,DynReg * dr1,Bitu di1,DynReg * drecx) {
	ForceDynReg(x64gen.regs[X64_REG_RCX],drecx);
	opcode((int)op).setrm(FindDynReg(dr1)->index,di1).Emit8(0xD2);
	dr1->flags|=DYNFLG_CHANGED;
}

static void gen_shift_byte_imm(Bitu op,DynReg * dr1,Bitu di1,uint8_t imm) {
	opcode inst = opcode((int)op).setrm(FindDynReg(dr1)->index,di1);
	if (imm==1) inst.Emit8(0xD0);
	else inst.setimm(imm,1).Emit8(0xC0);
	dr1->flags|=DYNFLG_CHANGED;
}

static void gen_shift_word_cl(Bitu op,bool dword,DynReg * dr1,DynReg * drecx) {
	ForceDynReg(x64gen.regs[X64_REG_RCX],drecx);
	opcode((int)op,dword).setrm(FindDynReg(dr1)->index).Emit8(0xD3);
	dr1->flags|=DYNFLG_CHANGED;
}

static void gen_shift_word_imm(Bitu op,bool dword,DynReg * dr1,uint8_t imm) {
	opcode inst = opcode((int)op,dword).setrm(FindDynReg(dr1)->index);
	if (imm==1) inst.Emit8(0xD1);
	else inst.setimm(imm,1).Emit8(0xC1);
	dr1->flags|=DYNFLG_CHANGED;
}

static void gen_cbw(bool dword,DynReg * dyn_ax) {
	if (dword) gen_extend_word(true,dyn_ax,dyn_ax);
	else gen_extend_byte(true,false,dyn_ax,dyn_ax,0);
}

static void gen_cwd(bool dword,DynReg * dyn_ax,DynReg * dyn_dx) {
	if (dyn_dx->genreg != x64gen.regs[X64_REG_RDX]) {
		if (dword) {
			if (dyn_dx->genreg) dyn_dx->genreg->dynreg = NULL;
			x64gen.regs[X64_REG_RDX]->Load(dyn_dx,true);
		} else ForceDynReg(x64gen.regs[X64_REG_RDX],dyn_dx);
	}
	ForceDynReg(x64gen.regs[X64_REG_RAX],dyn_ax);
	dyn_dx->flags|=DYNFLG_CHANGED;
	if (!dword) cache_addw(0x9966);
	else cache_addb(0x99);
}

static void gen_mul_byte(bool imul,DynReg * dyn_ax,DynReg * dr1,Bitu di1) {
	ForceDynReg(x64gen.regs[X64_REG_RAX],dyn_ax);
	opcode(imul?5:4).setrm(FindDynReg(dr1)->index,di1).Emit8(0xF6);
	dyn_ax->flags|=DYNFLG_CHANGED;
}

static void gen_mul_word(bool imul,DynReg * dyn_ax,DynReg * dyn_dx,bool dword,DynReg * dr1) {
	ForceDynReg(x64gen.regs[X64_REG_RAX],dyn_ax);
	if (dword && dyn_dx!=dr1) {
		// release current genreg
		if (dyn_dx->genreg) dyn_dx->genreg->dynreg = NULL;
		x64gen.regs[X64_REG_RDX]->Load(dyn_dx,true);
	} else ForceDynReg(x64gen.regs[X64_REG_RDX],dyn_dx);
	opcode(imul?5:4,dword).setrm(FindDynReg(dr1)->index).Emit8(0xF7);
	dyn_ax->flags|=DYNFLG_CHANGED;
	dyn_dx->flags|=DYNFLG_CHANGED;
}

static void gen_dshift_imm(bool dword,bool left,DynReg * dr1,DynReg * dr2,Bitu imm) {
	// shld/shrd imm
	opcode(FindDynReg(dr2)->index,dword).setimm(imm,1).setrm(FindDynReg(dr1)->index).Emit16(left ? 0xA40F:0xAC0F);
	dr1->flags|=DYNFLG_CHANGED;
}

static void gen_dshift_cl(bool dword,bool left,DynReg * dr1,DynReg * dr2,DynReg * drecx) {
	ForceDynReg(x64gen.regs[X64_REG_RCX],drecx);
	// shld/shrd cl
	opcode(FindDynReg(dr2)->index,dword).setrm(FindDynReg(dr1)->index).Emit16(left ? 0xA50F:0xAD0F);
	dr1->flags|=DYNFLG_CHANGED;
}

static void gen_call_ptr(void *func=NULL, uint8_t ptr=0) {
	x64gen.regs[X64_REG_RAX]->Clear();
	x64gen.regs[X64_REG_RCX]->Clear();
	x64gen.regs[X64_REG_RDX]->Clear();
#if !defined(_WIN64)
	x64gen.regs[X64_REG_RSI]->Clear();
	x64gen.regs[X64_REG_RDI]->Clear();
#endif
	x64gen.regs[X64_REG_R8]->Clear();
	x64gen.regs[X64_REG_R9]->Clear();
	x64gen.regs[X64_REG_R10]->Clear();
	x64gen.regs[X64_REG_R11]->Clear();
	/* Make sure reg_esp is current */
	if (DynRegs[G_ESP].flags & DYNFLG_CHANGED)
		DynRegs[G_ESP].genreg->Save();

	/* Do the actual call to the procedure */
	if (func!=NULL) {
		Bits diff = (Bits)func - (Bits)cache.pos - 5;
		if ((int32_t)diff == diff) {
			opcode(0).setimm(diff,4).Emit8Reg(0xE8); // call rel32
			return;
		}
		gen_load_imm(ptr, (Bitu)func);
	}
	opcode(2).setrm(ptr).Emit8(0xFF); // call ptr
}

static void gen_call_function(void * func,const char* ops,...) {
	Bitu paramcount=0;
	va_list params;
	DynReg *dynret=NULL;
	char rettype;

	/* Save the flags */
	if (GCC_LIKELY(!skip_flags)) gen_protectflags();
	if (ops==NULL) IllegalOption("gen_call_function NULL format");
	va_start(params,ops);
	while (*ops) {
		if (*ops++=='%') {
			GenReg *gen;
			switch (*ops++) {
			case 'I':       /* immediate value */
				gen = x64gen.regs[reg_args[paramcount++]];
				gen->Clear();
				if (*ops++!='p') gen_load_imm(gen->index,va_arg(params,uint32_t));
				else gen_load_imm(gen->index,va_arg(params,Bitu));
				break;
			case 'D':       /* Dynamic register */
				gen_load_arg_reg((int)paramcount++, va_arg(params,DynReg*), ops++);
				break;
			case 'R':       /* Dynamic register for returned value */
				dynret = va_arg(params,DynReg*);
				rettype = *ops++;
				break;
			case 'F':       /* arg is flags, release */
				gen = x64gen.regs[reg_args[paramcount++]];
				gen->Clear();
				gen_protectflags();
				opcode(gen->index).setea(4,-1,0,CALLSTACK).Emit8(0x8B); // mov reg, [rsp+8/40]
				opcode(0).set64().setimm(CALLSTACK+8,1).setrm(4).Emit8(0x83); // add rsp,16/48
				break;
			default:
				IllegalOption("gen_call_function unknown param");
			}
		}
	}
	va_end(params);

	gen_call_ptr(func);

	/* Save the return value in correct register */
	if (dynret) {
		GenReg * genret;
		if (rettype == 'd') {
			genret=x64gen.regs[X64_REG_RAX];
			if (dynret->genreg) dynret->genreg->dynreg=0;
			genret->Load(dynret,true);
		} else {
			opcode op(0); // src=eax/ax/al/ah
			x64gen.regs[X64_REG_RAX]->notusable = true;
			genret = FindDynReg(dynret);
			x64gen.regs[X64_REG_RAX]->notusable = false;
			switch (rettype) {
			case 'w':
				// mov r16, ax
				op.setword().setrm(genret->index).Emit8(0x89);
				break;
			case 'h':
				// mov reg8h, al
				op.setrm(genret->index,4).Emit8(0x88);
				break;
			case 'l':
				// mov r/m8, al
				op.setrm(genret->index,0).Emit8(0x88);
				break;
			}
		}
		dynret->flags|=DYNFLG_CHANGED;
	}
}

static void gen_call_write(DynReg * dr,uint32_t val,Bitu write_size) {
	void *func;
	gen_protectflags();
	gen_load_arg_reg(0,dr,"rd");

	switch (write_size) {
		case 1: func = (void*)mem_writeb_checked; break;
		case 2: func = (void*)mem_writew_checked; break;
		case 4: func = (void*)mem_writed_checked; break;
		default: IllegalOption("gen_call_write");
	}

	x64gen.regs[reg_args[1]]->Clear();
	opcode(ARG1_REG).setimm(val,4).Emit8Reg(0xB8); // mov ARG2, imm32
	gen_call_ptr(func);
}

static uint8_t * gen_create_branch(BranchTypes type) {
	/* First free all registers */
	cache_addw(0x70+type);
	return (cache.pos-1);
}

static void gen_fill_branch(uint8_t * data,uint8_t * from=cache.pos) {
#if C_DEBUG
	Bits len=from-data-1;
	if (len<0) len=~len;
	if (len>127)
		LOG_MSG("Big jump %d",len);
#endif
	*data=(uint8_t)(from-data-1);
}

static uint8_t * gen_create_branch_long(BranchTypes type) {
	cache_addw(0x800f+(type<<8));
	cache_addd(0);
	return (cache.pos-4);
}

static void gen_fill_branch_long(uint8_t * data,uint8_t * from=cache.pos) {
	*(uint32_t*)data=(uint32_t)(from-data-4);
}

static uint8_t * gen_create_jump(uint8_t * to=0) {
	/* First free all registers */
	cache_addb(0xe9);
	cache_addd((uint32_t)(to-(cache.pos+4)));
	return (cache.pos-4);
}

static void gen_fill_jump(uint8_t * data,uint8_t * to=cache.pos) {
	*(uint32_t*)data=(uint32_t)(to-data-4);
}

static uint8_t * gen_create_short_jump(void) {
	cache_addw(0x00EB);
	return cache.pos-1;
}

static void gen_fill_short_jump(uint8_t * data, uint8_t * to=cache.pos) {
#if C_DEBUG
	Bits len=to-data-1;
	if (len<0) len=~len;
	if (len>127)
		LOG_MSG("Big jump %d",len);
#endif
	data[0] = (uint8_t)(to-data-1);
}

static void gen_jmp_ptr(void * _ptr,int32_t imm=0) {
	Bitu ptr = (Bitu)_ptr;
	if ((uint32_t)ptr == ptr) {
		cache_addb(0x67); // 32-bit abs address
		opcode(0).set64().setimm(ptr,4).Emit8Reg(0xA1);
	} else opcode(0).set64().setimm(ptr,8).Emit8Reg(0xA1);
	opcode(4).setea(0,-1,0,imm).Emit8(0xFF); // jmp [rax+imm]
}

static void gen_save_flags(DynReg * dynreg) {
	if (GCC_UNLIKELY(x64gen.flagsactive)) IllegalOption("gen_save_flags");
	opcode(FindDynReg(dynreg)->index).setea(4,-1,0,CALLSTACK).Emit8(0x8B); // mov reg32, [rsp+8/40]
	dynreg->flags|=DYNFLG_CHANGED;
}

static void gen_load_flags(DynReg * dynreg) {
	if (GCC_UNLIKELY(x64gen.flagsactive)) IllegalOption("gen_load_flags");
	opcode(FindDynReg(dynreg)->index).setea(4,-1,0,CALLSTACK).Emit8(0x89); // mov [rsp+8/40],reg32
}

static void gen_save_host_direct(void *data,Bitu imm) {
	if ((int32_t)imm != (Bits)imm) {
		opcode(0).setimm(imm,4).setabsaddr(data).Emit8(0xC7); // mov dword[], imm32 (low dword)
		opcode(0).setimm(imm>>32,4).setabsaddr((uint8_t*)data+4).Emit8(0xC7); // high dword
	} else
		opcode(0).set64().setimm(imm,4).setabsaddr(data).Emit8(0xC7); // mov qword[], int32_t
}

static void gen_return(BlockReturn retcode) {
	gen_protectflags();
	opcode(1).setea(4,-1,0,CALLSTACK).Emit8(0x8B); // mov ecx, [rsp+8/40]
	opcode(0).set64().setrm(4).setimm(CALLSTACK+8,1).Emit8(0x83); // add rsp,16/48
	if (retcode==0) cache_addw(0xc033);		// xor eax,eax
	else {
		cache_addb(0xb8);		//MOV EAX, retcode
		cache_addd(retcode);
	}
	opcode(4).setea(4,-1,0,CALLSTACK-8).Emit8(0xFF); // jmp [rsp+CALLSTACK-8]
}

static void gen_return_fast(BlockReturn retcode,bool ret_exception=false) {
	if (GCC_UNLIKELY(x64gen.flagsactive)) IllegalOption("gen_return_fast");
	opcode(1).setabsaddr(&reg_flags).Emit8(0x8B); // mov ECX, [cpu_regs.flags]
	if (!ret_exception) {
		opcode(0).set64().setrm(4).setimm(CALLSTACK+8,1).Emit8(0x83); // add rsp,16/48
		if (retcode==0) cache_addw(0xc033);		// xor eax,eax
		else {
			cache_addb(0xb8);		//MOV EAX, retcode
			cache_addd(retcode);
		}
	}
	opcode(4).setea(4,-1,0,CALLSTACK-8).Emit8(0xFF); // jmp [rsp+CALLSTACK]
}

static void gen_init(void) {
	x64gen.regs[X64_REG_RAX]=new GenReg(0);
	x64gen.regs[X64_REG_RCX]=new GenReg(1);
	x64gen.regs[X64_REG_RDX]=new GenReg(2);
	x64gen.regs[X64_REG_RBX]=new GenReg(3);
	x64gen.regs[X64_REG_RSI]=new GenReg(6);
	x64gen.regs[X64_REG_RDI]=new GenReg(7);
	x64gen.regs[X64_REG_R8]=new GenReg(8);
	x64gen.regs[X64_REG_R9]=new GenReg(9);
	x64gen.regs[X64_REG_R10]=new GenReg(10);
	x64gen.regs[X64_REG_R11]=new GenReg(11);
	x64gen.regs[X64_REG_R12]=new GenReg(12);
	x64gen.regs[X64_REG_R13]=new GenReg(13);
	x64gen.regs[X64_REG_R14]=new GenReg(14);
	x64gen.regs[X64_REG_R15]=new GenReg(15);
}

static void gen_free(void) {
	if (x64gen.regs[X64_REG_RAX]) {
		delete x64gen.regs[X64_REG_RAX];
		x64gen.regs[X64_REG_RAX] = NULL;
	}
	if (x64gen.regs[X64_REG_RCX]) {
		delete x64gen.regs[X64_REG_RCX];
		x64gen.regs[X64_REG_RCX] = NULL;
	}
	if (x64gen.regs[X64_REG_RDX]) {
		delete x64gen.regs[X64_REG_RDX];
		x64gen.regs[X64_REG_RDX] = NULL;
	}
	if (x64gen.regs[X64_REG_RBX]) {
		delete x64gen.regs[X64_REG_RBX];
		x64gen.regs[X64_REG_RBX] = NULL;
	}
	if (x64gen.regs[X64_REG_RSI]) {
		delete x64gen.regs[X64_REG_RSI];
		x64gen.regs[X64_REG_RSI] = NULL;
	}
	if (x64gen.regs[X64_REG_RDI]) {
		delete x64gen.regs[X64_REG_RDI];
		x64gen.regs[X64_REG_RDI] = NULL;
	}
	if (x64gen.regs[X64_REG_R8]) {
		delete x64gen.regs[X64_REG_R8];
		x64gen.regs[X64_REG_R8] = NULL;
	}
	if (x64gen.regs[X64_REG_R9]) {
		delete x64gen.regs[X64_REG_R9];
		x64gen.regs[X64_REG_R9] = NULL;
	}
	if (x64gen.regs[X64_REG_R10]) {
		delete x64gen.regs[X64_REG_R10];
		x64gen.regs[X64_REG_R10] = NULL;
	}
	if (x64gen.regs[X64_REG_R11]) {
		delete x64gen.regs[X64_REG_R11];
		x64gen.regs[X64_REG_R11] = NULL;
	}
	if (x64gen.regs[X64_REG_R12]) {
		delete x64gen.regs[X64_REG_R12];
		x64gen.regs[X64_REG_R12] = NULL;
	}
	if (x64gen.regs[X64_REG_R13]) {
		delete x64gen.regs[X64_REG_R13];
		x64gen.regs[X64_REG_R13] = NULL;
	}
	if (x64gen.regs[X64_REG_R14]) {
		delete x64gen.regs[X64_REG_R14];
		x64gen.regs[X64_REG_R14] = NULL;
	}
	if (x64gen.regs[X64_REG_R15]) {
		delete x64gen.regs[X64_REG_R15];
		x64gen.regs[X64_REG_R15] = NULL;
	}
}

#if defined(X86_DYNFPU_DH_ENABLED)
static void gen_dh_fpu_saveInit(void);
static void (*gen_dh_fpu_save)(void)  = gen_dh_fpu_saveInit;

// DO NOT USE opcode::setabsaddr IN THIS FUNCTION (RBP unavailable at execution time)
static void gen_dh_fpu_saveInit(void) {
	uint8_t* oldpos = cache.pos;
	cache.pos = &cache_code_link_blocks[64];
	gen_dh_fpu_save = (void(*)(void))cache_rwtox(cache.pos);

	Bitu addr = (Bitu)&dyn_dh_fpu;
	// mov RAX, &dyn_dh_fpu
	if ((uint32_t)addr == addr) opcode(0).setimm(addr,4).Emit8Reg(0xB8);
	else opcode(0).set64().setimm(addr,8).Emit8Reg(0xB8);

	// fnsave [dyn_dh_fpu.state]
	opcode(6).setea(0,-1,0,offsetof(struct dyn_dh_fpu,state)).Emit8(0xdd);
	// fldcw [dyn_dh_fpu.host_cw]
	opcode(5).setea(0,-1,0,offsetof(struct dyn_dh_fpu,host_cw)).Emit8(0xd9);
	// mov byte [dyn_dh_fpu.state_used], 0
	opcode(0).setimm(0,1).setea(0,-1,0,offsetof(struct dyn_dh_fpu,state_used)).Emit8(0xc6);
	// or byte [dyn_dh_fpu.state.cw], 0x3F
	opcode(1).setimm(0x3F,1).setea(0,-1,0,offsetof(struct dyn_dh_fpu,state.cw)).Emit8(0x80);
	cache_addb(0xC3); // RET

	cache.pos = oldpos;
	gen_dh_fpu_save();
}
#endif

