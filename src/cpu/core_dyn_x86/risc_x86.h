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


static void gen_init(void);

/* End of needed */

#define X86_REGS		7
#define X86_REG_EAX		0x00
#define X86_REG_ECX		0x01
#define X86_REG_EDX		0x02
#define X86_REG_EBX		0x03
#define X86_REG_EBP		0x04
#define X86_REG_ESI		0x05
#define X86_REG_EDI		0x06

#define X86_REG_MASK(_REG_) (1 << X86_REG_ ## _REG_)

static struct {
	bool flagsactive;
	Bitu last_used;
	GenReg * regs[X86_REGS];
} x86gen;

class GenReg {
public:
	GenReg(uint8_t _index) {
		index=_index;
		notusable=false;dynreg=0;
	}
	DynReg  * dynreg;
	Bitu last_used;			//Keeps track of last assigned regs 
    uint8_t index;
	bool notusable;
	void Load(DynReg * _dynreg,bool stale=false) {
		if (!_dynreg) return;
		if (GCC_UNLIKELY((Bitu)dynreg)) Clear();
		dynreg=_dynreg;
		last_used=x86gen.last_used;
		dynreg->flags&=~DYNFLG_CHANGED;
		dynreg->genreg=this;
		if ((!stale) && (dynreg->flags & (DYNFLG_LOAD|DYNFLG_ACTIVE))) {
			cache_addw(0x058b+(index << (8+3)));		//Mov reg,[data]
			cache_addd((uint32_t)dynreg->data);
		}
		dynreg->flags|=DYNFLG_ACTIVE;
	}
	void Save(void) {
		if (GCC_UNLIKELY(!((Bitu)dynreg))) IllegalOption("GenReg->Save");
		dynreg->flags&=~DYNFLG_CHANGED;
		cache_addw(0x0589+(index << (8+3)));		//Mov [data],reg
		cache_addd((uint32_t)dynreg->data);
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

static BlockReturn gen_runcode(uint8_t * code) {
	BlockReturn retval;
#if defined (_MSC_VER)
	__asm {
/* Prepare the flags */
		mov		eax,[code]
		push	ebx
		push	ebp
		push	esi
		push	edi
		mov		ebx,[reg_flags]
		and		ebx,FMASK_TEST
		push	offset(return_address)
		push	ebx
		jmp		eax
/* Restore the flags */
return_address:
		/*	return here with flags in ecx */
		and		dword ptr [reg_flags],~FMASK_TEST
		and		ecx,FMASK_TEST
		or		[reg_flags],ecx
		pop		edi
		pop		esi
		pop		ebp
		pop		ebx
		mov		[retval],eax
	}
#elif defined (MACOSX)
	register uint32_t tempflags=reg_flags & FMASK_TEST;
	__asm__ volatile (
		"pushl %%ebx						\n"
		"pushl %%ebp						\n"
		"pushl $(run_return_adress)			\n"
		"pushl  %2							\n"
		"jmp  *%3							\n"
		"run_return_adress:					\n"
		"popl %%ebp							\n"
		"popl %%ebx							\n"
		:"=a" (retval), "=c" (tempflags)
		:"r" (tempflags),"r" (code)
		:"%edx","%edi","%esi","cc","memory"
	);
	reg_flags=(reg_flags & ~FMASK_TEST) | (tempflags & FMASK_TEST);
#else
	register uint32_t tempflags=reg_flags & FMASK_TEST;
	__asm__ volatile (
		"pushl %%ebp						\n"
		"pushl $(run_return_adress)			\n"
		"pushl  %2							\n"
		"jmp  *%3							\n"
		"run_return_adress:					\n"
		"popl %%ebp							\n"
		:"=a" (retval), "=c" (tempflags)
		:"r" (tempflags),"r" (code)
		:"%edx","%ebx","%edi","%esi","cc","memory"
	);
	reg_flags=(reg_flags & ~FMASK_TEST) | (tempflags & FMASK_TEST);
#endif
	return retval;
}

static GenReg * FindDynReg(DynReg * dynreg,bool stale=false) {
	x86gen.last_used++;
	if (dynreg->genreg) {
		dynreg->genreg->last_used=x86gen.last_used;
		return dynreg->genreg;
	}
	/* Find best match for selected global reg */
	Bits i;
	Bits first_used,first_index;
	first_used=-1;
	if (dynreg->flags & DYNFLG_HAS8) {
		/* Has to be eax,ebx,ecx,edx */
		for (i=first_index=0;i<=X86_REG_EBX;i++) {
			GenReg * genreg=x86gen.regs[i];
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
		for (i=first_index=X86_REGS-1;i>=0;i--) {
			GenReg * genreg=x86gen.regs[i];
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
	GenReg * newreg=x86gen.regs[first_index];
	newreg->Load(dynreg,stale);
	return newreg;
}

static GenReg * ForceDynReg(GenReg * genreg,DynReg * dynreg) {
	genreg->last_used=++x86gen.last_used;
	if (dynreg->genreg==genreg) return genreg;
	if (genreg->dynreg) genreg->Clear();
	if (dynreg->genreg) dynreg->genreg->Clear();
	genreg->Load(dynreg);
	return genreg;
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
	if (!x86gen.flagsactive) {
		x86gen.flagsactive=true;
		cache_addb(0x9d);		//POPFD
	}
}

static void gen_protectflags(void) {
	if (x86gen.flagsactive) {
		x86gen.flagsactive=false;
		cache_addb(0x9c);		//PUSHFD
	}
}

static void gen_discardflags(void) {
	if (!x86gen.flagsactive) {
		x86gen.flagsactive=true;
		cache_addw(0xc483);		//ADD ESP,4
		cache_addb(0x4);
	}
}

static void gen_needcarry(void) {
	if (!x86gen.flagsactive) {
		x86gen.flagsactive=true;
		cache_addw(0x2cd1);			//SHR DWORD [ESP],1
		cache_addb(0x24);
		cache_addd(0x0424648d);		//LEA ESP,[ESP+4]
	}
}

static void gen_setzeroflag(void) {
	if (x86gen.flagsactive) IllegalOption("gen_setzeroflag");
	cache_addw(0x0c83);			//OR DWORD [ESP],0x40
	cache_addw(0x4024);
}

static void gen_clearzeroflag(void) {
	if (x86gen.flagsactive) IllegalOption("gen_clearzeroflag");
	cache_addw(0x2483);			//AND DWORD [ESP],~0x40
	cache_addw(0xbf24);
}

static bool skip_flags=false;

static void set_skipflags(bool state) {
	if (!state) gen_discardflags();
	skip_flags=state;
}

static void gen_reinit(void) {
	x86gen.last_used=0;
	x86gen.flagsactive=false;
	for (Bitu i=0;i<X86_REGS;i++) {
		x86gen.regs[i]->dynreg=0;
	}
}


static void gen_load_host(void * data,DynReg * dr1,Bitu size) {
	GenReg * gr1=FindDynReg(dr1,true);
	switch (size) {
	case 1:cache_addw(0xb60f);break;	//movzx byte
	case 2:cache_addw(0xb70f);break;	//movzx word
	case 4:cache_addb(0x8b);break;	//mov
	default:
		IllegalOption("gen_load_host");
	}
	cache_addb(0x5+(gr1->index<<3));
	cache_addd((uint32_t)data);
	dr1->flags|=DYNFLG_CHANGED;
}

static void gen_mov_host(void * data,DynReg * dr1,Bitu size,Bitu di1=0) {
	GenReg * gr1=FindDynReg(dr1,(size==4));
	switch (size) {
	case 1:cache_addb(0x8a);break;	//mov byte
	case 2:cache_addb(0x66);		//mov word
	case 4:cache_addb(0x8b);break;	//mov
	default:
		IllegalOption("gen_mov_host");
	}
	cache_addb(0x5+((gr1->index+di1)<<3));
	cache_addd((uint32_t)data);
	dr1->flags|=DYNFLG_CHANGED;
}

static void gen_save_host(void * data,DynReg * dr1,Bitu size,Bitu di1=0) {
	GenReg * gr1=FindDynReg(dr1);
	switch (size) {
	case 1:cache_addb(0x88);break;	//mov byte
	case 2:cache_addb(0x66);		//mov word
	case 4:cache_addb(0x89);break;	//mov
	default:
		IllegalOption("gen_save_host");
	}
	cache_addb(0x5+((gr1->index+di1)<<3));
	cache_addd((uint32_t)data);
	dr1->flags|=DYNFLG_CHANGED;
}

static void gen_dop_byte(DualOps op,DynReg * dr1,uint8_t di1,DynReg * dr2,uint8_t di2) {
	GenReg * gr1=FindDynReg(dr1);GenReg * gr2=FindDynReg(dr2);
	uint8_t tmp;
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
	cache_addw(tmp|(0xc0+((gr1->index+di1)<<3)+gr2->index+di2)<<8);
}

static void gen_dop_byte_imm(DualOps op,DynReg * dr1,uint8_t di1,Bitu imm) {
	GenReg * gr1=FindDynReg(dr1);
	uint16_t tmp;
	switch (op) {
	case DOP_ADD:	tmp=0xc080; break;
	case DOP_ADC:	tmp=0xd080; break;
	case DOP_SUB:	tmp=0xe880; break;
	case DOP_SBB:	tmp=0xd880; break;
	case DOP_CMP:	tmp=0xf880; goto nochange;	//Doesn't change
	case DOP_XOR:	tmp=0xf080; break;
	case DOP_AND:	tmp=0xe080; break;
	case DOP_OR:	tmp=0xc880; break;
	case DOP_TEST:	tmp=0xc0f6; goto nochange;	//Doesn't change
	case DOP_MOV:	cache_addb(0xb0+gr1->index+di1);
					dr1->flags|=DYNFLG_CHANGED;
					goto finish;
	default:
		IllegalOption("gen_dop_byte_imm");
	}
	dr1->flags|=DYNFLG_CHANGED;
nochange:
	cache_addw(tmp+((gr1->index+di1)<<8));
finish:
	cache_addb(imm);
}

static void gen_dop_byte_imm_mem(DualOps op,DynReg * dr1,uint8_t di1,void* data) {
	GenReg * gr1=FindDynReg(dr1);
	uint16_t tmp;
	switch (op) {
	case DOP_ADD:	tmp=0x0502; break;
	case DOP_ADC:	tmp=0x0512; break;
	case DOP_SUB:	tmp=0x052a; break;
	case DOP_SBB:	tmp=0x051a; break;
	case DOP_CMP:	tmp=0x053a; goto nochange;	//Doesn't change
	case DOP_XOR:	tmp=0x0532; break;
	case DOP_AND:	tmp=0x0522; break;
	case DOP_OR:	tmp=0x050a; break;
	case DOP_TEST:	tmp=0x0584; goto nochange;	//Doesn't change
	case DOP_MOV:	tmp=0x058A; break;
	default:
		IllegalOption("gen_dop_byte_imm_mem");
	}
	dr1->flags|=DYNFLG_CHANGED;
nochange:
	cache_addw(tmp+((gr1->index+di1)<<11));
	cache_addd((uint32_t)data);
}

static void gen_sop_byte(SingleOps op,DynReg * dr1,uint8_t di1) {
	GenReg * gr1=FindDynReg(dr1);
	uint16_t tmp;
	switch (op) {
	case SOP_INC: tmp=0xc0FE; break;
	case SOP_DEC: tmp=0xc8FE; break;
	case SOP_NOT: tmp=0xd0f6; break;
	case SOP_NEG: tmp=0xd8f6; break;
	default:
		IllegalOption("gen_sop_byte");
	}
	cache_addw(tmp + ((gr1->index+di1)<<8));
	dr1->flags|=DYNFLG_CHANGED;
}


static void gen_extend_word(bool sign,DynReg * ddr,DynReg * dsr) {
	GenReg * gsr=FindDynReg(dsr);
	GenReg * gdr=FindDynReg(ddr,true);
	if (sign) cache_addw(0xbf0f);
	else cache_addw(0xb70f);
	cache_addb(0xc0+(gdr->index<<3)+(gsr->index));
	ddr->flags|=DYNFLG_CHANGED;
}

static void gen_extend_byte(bool sign,bool dword,DynReg * ddr,DynReg * dsr,uint8_t dsi) {
	GenReg * gsr=FindDynReg(dsr);
	GenReg * gdr=FindDynReg(ddr,dword);
	if (!dword) cache_addb(0x66);
	if (sign) cache_addw(0xbe0f);
	else cache_addw(0xb60f);
	cache_addb(0xc0+(gdr->index<<3)+(gsr->index+dsi));
	ddr->flags|=DYNFLG_CHANGED;
}

static void gen_lea(DynReg * ddr,DynReg * dsr1,DynReg * dsr2,Bitu scale,Bits imm) {
	GenReg * gdr=FindDynReg(ddr);
	Bitu imm_size;
	uint8_t rm_base=(gdr->index << 3);
	if (dsr1) {
		GenReg * gsr1=FindDynReg(dsr1);
		if (!imm && (gsr1->index!=0x5)) {
			imm_size=0;	rm_base+=0x0;			//no imm
		} else if ((imm>=-128 && imm<=127)) {
			imm_size=1;rm_base+=0x40;			//Signed byte imm
		} else {
			imm_size=4;rm_base+=0x80;			//Signed dword imm
		}
		if (dsr2) {
			GenReg * gsr2=FindDynReg(dsr2);
			cache_addb(0x8d);		//LEA
			cache_addb(rm_base+0x4);			//The sib indicator
			uint8_t sib=(gsr1->index)+(gsr2->index<<3)+(scale<<6);
			cache_addb(sib);
		} else {
			if ((ddr==dsr1) && !imm_size) return;
			cache_addb(0x8d);		//LEA
			cache_addb(rm_base+gsr1->index);
		}
	} else {
		if (dsr2) {
			GenReg * gsr2=FindDynReg(dsr2);
			cache_addb(0x8d);			//LEA
			cache_addb(rm_base+0x4);	//The sib indicator
			uint8_t sib=(5+(gsr2->index<<3)+(scale<<6));
			cache_addb(sib);
			imm_size=4;
		} else {
			cache_addb(0x8d);			//LEA
			cache_addb(rm_base+0x05);	//dword imm
			imm_size=4;
		}
	}
	switch (imm_size) {
	case 0:	break;
	case 1:cache_addb(imm);break;
	case 4:cache_addd(imm);break;
	}
	ddr->flags|=DYNFLG_CHANGED;
}

static void gen_lea_imm_mem(DynReg * ddr,DynReg * dsr,void* data) {
	GenReg * gdr=FindDynReg(ddr);
	uint8_t rm_base=(gdr->index << 3);
	cache_addw(0x058b+(rm_base<<8));
	cache_addd((uint32_t)data);
	GenReg * gsr=FindDynReg(dsr);
	cache_addb(0x8d);		//LEA
	cache_addb(rm_base+0x44);
	cache_addb(rm_base+gsr->index);
	cache_addb(0x00);
	ddr->flags|=DYNFLG_CHANGED;
}

static void gen_dop_word(DualOps op,bool dword,DynReg * dr1,DynReg * dr2) {
	GenReg * gr2=FindDynReg(dr2);
	GenReg * gr1=FindDynReg(dr1,dword && op==DOP_MOV);
	uint8_t tmp;
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
			dr1->genreg=gr2;dr1->genreg->dynreg=dr1;
			dr2->genreg=gr1;dr2->genreg->dynreg=dr2;
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
	if (!dword) cache_addb(0x66);
	cache_addw(tmp|(0xc0+(gr1->index<<3)+gr2->index)<<8);
}

static void gen_dop_word_imm(DualOps op,bool dword,DynReg * dr1,Bits imm) {
	GenReg * gr1=FindDynReg(dr1,dword && op==DOP_MOV);
	uint16_t tmp;
	if (!dword) cache_addb(0x66);
	switch (op) {
	case DOP_ADD:	tmp=0xc081; break; 
	case DOP_ADC:	tmp=0xd081; break;
	case DOP_SUB:	tmp=0xe881; break;
	case DOP_SBB:	tmp=0xd881; break;
	case DOP_CMP:	tmp=0xf881; goto nochange;	//Doesn't change
	case DOP_XOR:	tmp=0xf081; break;
	case DOP_AND:	tmp=0xe081; break;
	case DOP_OR:	tmp=0xc881; break;
	case DOP_TEST:	tmp=0xc0f7; goto nochange;	//Doesn't change
	case DOP_MOV:	cache_addb(0xb8+(gr1->index)); dr1->flags|=DYNFLG_CHANGED; goto finish;
	default:
		IllegalOption("gen_dop_word_imm");
	}
	dr1->flags|=DYNFLG_CHANGED;
nochange:
	cache_addw(tmp+(gr1->index<<8));
finish:
	if (dword) cache_addd(imm);
	else cache_addw(imm);
}

static void gen_dop_word_imm_mem(DualOps op,bool dword,DynReg * dr1,void* data) {
	GenReg * gr1=FindDynReg(dr1,dword && op==DOP_MOV);
	uint16_t tmp;
	switch (op) {
	case DOP_ADD:	tmp=0x0503; break; 
	case DOP_ADC:	tmp=0x0513; break;
	case DOP_SUB:	tmp=0x052b; break;
	case DOP_SBB:	tmp=0x051b; break;
	case DOP_CMP:	tmp=0x053b; goto nochange;	//Doesn't change
	case DOP_XOR:	tmp=0x0533; break;
	case DOP_AND:	tmp=0x0523; break;
	case DOP_OR:	tmp=0x050b; break;
	case DOP_TEST:	tmp=0x0585; goto nochange;	//Doesn't change
	case DOP_MOV:
		gen_mov_host(data,dr1,dword?4:2);
		dr1->flags|=DYNFLG_CHANGED;
		return;
	default:
		IllegalOption("gen_dop_word_imm_mem");
	}
	dr1->flags|=DYNFLG_CHANGED;
nochange:
	if (!dword) cache_addb(0x66);
	cache_addw(tmp+(gr1->index<<11));
	cache_addd((uint32_t)data);
}

static void gen_dop_word_var(DualOps op,bool dword,DynReg * dr1,void* drd) {
	GenReg * gr1=FindDynReg(dr1,dword && op==DOP_MOV);
	uint8_t tmp;
	switch (op) {
	case DOP_ADD:	tmp=0x03; break;
	case DOP_ADC:	tmp=0x13; break;
	case DOP_SUB:	tmp=0x2b; break;
	case DOP_SBB:	tmp=0x1b; break;
	case DOP_CMP:	tmp=0x3b; break;
	case DOP_XOR:	tmp=0x33; break;
	case DOP_AND:	tmp=0x23; break;
	case DOP_OR:	tmp=0x0b; break;
	case DOP_TEST:	tmp=0x85; break;
	case DOP_MOV:	tmp=0x8b; break;
	case DOP_XCHG:	tmp=0x87; break;
	default:
		IllegalOption("gen_dop_word_var");
	}
	if (!dword) cache_addb(0x66);
	cache_addw(tmp|(0x05+((gr1->index)<<3))<<8);
	cache_addd((uint32_t)drd);
}

static void gen_imul_word(bool dword,DynReg * dr1,DynReg * dr2) {
	GenReg * gr1=FindDynReg(dr1);GenReg * gr2=FindDynReg(dr2);
	dr1->flags|=DYNFLG_CHANGED;
	if (!dword) {
		cache_addd(0xaf0f66|(0xc0+(gr1->index<<3)+gr2->index)<<24);
	} else {
		cache_addw(0xaf0f);
		cache_addb(0xc0+(gr1->index<<3)+gr2->index);
	}
}

static void gen_imul_word_imm(bool dword,DynReg * dr1,DynReg * dr2,Bits imm) {
	GenReg * gr1=FindDynReg(dr1);GenReg * gr2=FindDynReg(dr2);
	if (!dword) cache_addb(0x66);
	if ((imm>=-128 && imm<=127)) {
		cache_addb(0x6b);
		cache_addb(0xc0+(gr1->index<<3)+gr2->index);
		cache_addb(imm);
	} else {
		cache_addb(0x69);
		cache_addb(0xc0+(gr1->index<<3)+gr2->index);
		if (dword) cache_addd(imm);
		else cache_addw(imm);
	}
	dr1->flags|=DYNFLG_CHANGED;
}


static void gen_sop_word(SingleOps op,bool dword,DynReg * dr1) {
	GenReg * gr1=FindDynReg(dr1);
	if (!dword) cache_addb(0x66);
	switch (op) {
	case SOP_INC:cache_addb(0x40+gr1->index);break;
	case SOP_DEC:cache_addb(0x48+gr1->index);break;
	case SOP_NOT:cache_addw(0xd0f7+(gr1->index<<8));break;
	case SOP_NEG:cache_addw(0xd8f7+(gr1->index<<8));break;
	default:
		IllegalOption("gen_sop_word");
	}
	dr1->flags|=DYNFLG_CHANGED;
}

static void gen_shift_byte_cl(Bitu op,DynReg * dr1,uint8_t di1,DynReg * drecx) {
	ForceDynReg(x86gen.regs[X86_REG_ECX],drecx);
	GenReg * gr1=FindDynReg(dr1);
	cache_addw(0xc0d2+(((uint16_t)op) << 11)+ ((gr1->index+di1)<<8));
	dr1->flags|=DYNFLG_CHANGED;
}

static void gen_shift_byte_imm(Bitu op,DynReg * dr1,uint8_t di1,uint8_t imm) {
	GenReg * gr1=FindDynReg(dr1);
	cache_addw(0xc0c0+(((uint16_t)op) << 11) + ((gr1->index+di1)<<8));
	cache_addb(imm);
	dr1->flags|=DYNFLG_CHANGED;
}

static void gen_shift_word_cl(Bitu op,bool dword,DynReg * dr1,DynReg * drecx) {
	ForceDynReg(x86gen.regs[X86_REG_ECX],drecx);
	GenReg * gr1=FindDynReg(dr1);
	if (!dword) cache_addb(0x66);
	cache_addw(0xc0d3+(((uint16_t)op) << 11) + ((gr1->index)<<8));
	dr1->flags|=DYNFLG_CHANGED;
}

static void gen_shift_word_imm(Bitu op,bool dword,DynReg * dr1,uint8_t imm) {
	GenReg * gr1=FindDynReg(dr1);
	dr1->flags|=DYNFLG_CHANGED;
	if (!dword) {
		cache_addd(0x66|((0xc0c1+((uint16_t)op << 11) + (gr1->index<<8))|imm<<16)<<8);
	} else { 
		cache_addw(0xc0c1+((uint16_t)op << 11) + (gr1->index<<8));
		cache_addb(imm);
	}
}

static void gen_cbw(bool dword,DynReg * dyn_ax) {
	ForceDynReg(x86gen.regs[X86_REG_EAX],dyn_ax);
	if (!dword) cache_addb(0x66);
	cache_addb(0x98);
	dyn_ax->flags|=DYNFLG_CHANGED;
}

static void gen_cwd(bool dword,DynReg * dyn_ax,DynReg * dyn_dx) {
	ForceDynReg(x86gen.regs[X86_REG_EAX],dyn_ax);
	ForceDynReg(x86gen.regs[X86_REG_EDX],dyn_dx);
	dyn_ax->flags|=DYNFLG_CHANGED;
	dyn_dx->flags|=DYNFLG_CHANGED;
	if (!dword) cache_addw(0x9966);
	else cache_addb(0x99);
}

static void gen_mul_byte(bool imul,DynReg * dyn_ax,DynReg * dr1,uint8_t di1) {
	ForceDynReg(x86gen.regs[X86_REG_EAX],dyn_ax);
	GenReg * gr1=FindDynReg(dr1);
	if (imul) cache_addw(0xe8f6+((gr1->index+di1)<<8));
	else cache_addw(0xe0f6+((gr1->index+di1)<<8));
	dyn_ax->flags|=DYNFLG_CHANGED;
}

static void gen_mul_word(bool imul,DynReg * dyn_ax,DynReg * dyn_dx,bool dword,DynReg * dr1) {
	ForceDynReg(x86gen.regs[X86_REG_EAX],dyn_ax);
	ForceDynReg(x86gen.regs[X86_REG_EDX],dyn_dx);
	GenReg * gr1=FindDynReg(dr1);
	if (!dword) cache_addb(0x66);
	if (imul) cache_addw(0xe8f7+(gr1->index<<8));
	else cache_addw(0xe0f7+(gr1->index<<8));
	dyn_ax->flags|=DYNFLG_CHANGED;
	dyn_dx->flags|=DYNFLG_CHANGED;
}

static void gen_dshift_imm(bool dword,bool left,DynReg * dr1,DynReg * dr2,Bitu imm) {
	GenReg * gr1=FindDynReg(dr1);
	GenReg * gr2=FindDynReg(dr2);
	if (!dword) cache_addb(0x66);
	if (left) cache_addw(0xa40f);		//SHLD IMM
	else  cache_addw(0xac0f);			//SHRD IMM
	cache_addb(0xc0+gr1->index+(gr2->index<<3));
	cache_addb(imm);
	dr1->flags|=DYNFLG_CHANGED;
}

static void gen_dshift_cl(bool dword,bool left,DynReg * dr1,DynReg * dr2,DynReg * drecx) {
	ForceDynReg(x86gen.regs[X86_REG_ECX],drecx);
	GenReg * gr1=FindDynReg(dr1);
	GenReg * gr2=FindDynReg(dr2);
	if (!dword) cache_addb(0x66);
	if (left) cache_addw(0xa50f);		//SHLD CL
	else  cache_addw(0xad0f);			//SHRD CL
	cache_addb(0xc0+gr1->index+(gr2->index<<3));
	dr1->flags|=DYNFLG_CHANGED;
}

static void gen_call_function(void * func,char const* ops,...) {
	Bits paramcount=0;
	bool release_flags=false;
	struct ParamInfo {
		const char * line;
		Bitu value;
	} pinfo[32];
	ParamInfo * retparam=0;
	/* Clear the EAX Genreg for usage */
	x86gen.regs[X86_REG_EAX]->Clear();
	x86gen.regs[X86_REG_EAX]->notusable=true;
	/* Save the flags */
	if (GCC_UNLIKELY(!skip_flags)) gen_protectflags();
	/* Scan for the amount of params */
	if (ops) {
		va_list params;
		va_start(params,ops);
#if defined (MACOSX)
		Bitu stack_used=0;
		bool free_flags=false;
#endif
		Bits pindex=0;
		while (*ops) {
			if (*ops=='%') {
				pinfo[pindex].line=ops+1;
				pinfo[pindex].value=va_arg(params,Bitu);
#if defined (MACOSX)
				const char * scan=pinfo[pindex].line;
				if ((*scan=='I') || (*scan=='D')) stack_used+=4;
				else if (*scan=='F') free_flags=true;
#endif
				pindex++;
			}
			ops++;
		}
		va_end(params);

#if defined (MACOSX)
		/* align stack */
		stack_used+=4;			// saving esp on stack as well

		cache_addw(0xc48b);		// mov eax,esp
		cache_addb(0x2d);		// sub eax,stack_used
		cache_addd(stack_used);
		cache_addw(0xe083);		// and eax,0xfffffff0
		cache_addb(0xf0);
		cache_addb(0x05);		// sub eax,stack_used
		cache_addd(stack_used);
		cache_addb(0x94);		// xchg eax,esp
		if (free_flags) {
			cache_addw(0xc083);	// add eax,4
			cache_addb(0x04);
		}
		cache_addb(0x50);		// push eax (==old esp)
#endif

		paramcount=0;
		while (pindex) {
			pindex--;
			const char * scan=pinfo[pindex].line;
			switch (*scan++) {
			case 'I':				/* immediate value */
				paramcount++;
				cache_addb(0x68);			//Push immediate
				cache_addd(pinfo[pindex].value);	//Push value
				break;
			case 'D':				/* Dynamic register */
				{
					bool release=false;
					paramcount++;
					DynReg * dynreg=(DynReg *)pinfo[pindex].value;
					GenReg * genreg=FindDynReg(dynreg);
					scanagain:
					switch (*scan++) {
					case 'd':
						cache_addb(0x50+genreg->index);		//Push reg
						break;
					case 'w':
						cache_addw(0xb70f);					//MOVZX EAX,reg
						cache_addb(0xc0+genreg->index);
						cache_addb(0x50);					//Push EAX
						break;
					case 'l':
						cache_addw(0xb60f);					//MOVZX EAX,reg[0]
						cache_addb(0xc0+genreg->index);
						cache_addb(0x50);					//Push EAX
						break;
					case 'h':
						cache_addw(0xb60f);					//MOVZX EAX,reg[1]
						cache_addb(0xc4+genreg->index);
						cache_addb(0x50);					//Push EAX
						break;
					case 'r':								/* release the reg afterwards */
						release=true;
						goto scanagain;
					default:
						IllegalOption("gen_call_function param:DREG");
					}
					if (release) gen_releasereg(dynreg);
				}
				break;
			case 'R':				/* Dynamic register to get the return value */
				retparam =&pinfo[pindex];
				pinfo[pindex].line=scan;
				break;
			case 'F':				/* Release flags from stack */
				release_flags=true;
				break;
			default:
				IllegalOption("gen_call_function unknown param");
			}
		}
#if defined (MACOSX)
		if (free_flags) release_flags=false;
	} else {
		/* align stack */
		uint32_t stack_used=8;	// saving esp and return address on the stack

		cache_addw(0xc48b);		// mov eax,esp
		cache_addb(0x2d);		// sub eax,stack_used
		cache_addd(stack_used);
		cache_addw(0xe083);		// and eax,0xfffffff0
		cache_addb(0xf0);
		cache_addb(0x05);		// sub eax,stack_used
		cache_addd(stack_used);
		cache_addb(0x94);		// xchg eax,esp
		cache_addb(0x50);		// push esp (==old esp)
#endif
	}

	/* Clear some unprotected registers */
	x86gen.regs[X86_REG_ECX]->Clear();
	x86gen.regs[X86_REG_EDX]->Clear();
	/* Make sure reg_esp is current */
	if (DynRegs[G_ESP].flags & DYNFLG_CHANGED)
		DynRegs[G_ESP].genreg->Save();
	/* Do the actual call to the procedure */
	cache_addb(0xe8);
	cache_addd((uint32_t)func - (uint32_t)cache_rwtox(cache.pos)-4);
	/* Restore the params of the stack */
	if (paramcount) {
		cache_addw(0xc483);				//add ESP,imm byte
		cache_addb(paramcount*4+(release_flags?4:0));
	} else if (release_flags) {
		cache_addw(0xc483);				//add ESP,imm byte
		cache_addb(4);
	}
	/* Save the return value in correct register */
	if (retparam) {
		DynReg * dynreg=(DynReg *)retparam->value;
		GenReg * genreg=FindDynReg(dynreg);
		if (genreg->index)		// test for (e)ax/al
		switch (*retparam->line) {
		case 'd':
			cache_addw(0xc08b+(genreg->index <<(8+3)));	//mov reg,eax
			break;
		case 'w':
			cache_addb(0x66);							
			cache_addw(0xc08b+(genreg->index <<(8+3)));	//mov reg,eax
			break;
		case 'l':
			cache_addw(0xc08a+(genreg->index <<(8+3)));	//mov reg,eax
			break;
		case 'h':
			cache_addw(0xc08a+((genreg->index+4) <<(8+3)));	//mov reg,eax
			break;
		}
		dynreg->flags|=DYNFLG_CHANGED;
	}
	/* Restore EAX registers to be used again */
	x86gen.regs[X86_REG_EAX]->notusable=false;

#if defined (MACOSX)
	/* restore stack */
	cache_addb(0x5c);	// pop esp
#endif
}

static void gen_call_write(DynReg * dr,uint32_t val,Bitu write_size) {
	/* Clear the EAX Genreg for usage */
	x86gen.regs[X86_REG_EAX]->Clear();
	x86gen.regs[X86_REG_EAX]->notusable=true;
	gen_protectflags();

#if defined (MACOSX)
	/* align stack */
	Bitu stack_used=12;

	cache_addw(0xc48b);		// mov eax,esp
	cache_addb(0x2d);		// sub eax,stack_used
	cache_addd(stack_used);
	cache_addw(0xe083);		// and eax,0xfffffff0
	cache_addb(0xf0);
	cache_addb(0x05);		// sub eax,stack_used
	cache_addd(stack_used);
	cache_addb(0x94);		// xchg eax,esp
	cache_addb(0x50);		// push eax (==old esp)
#endif

	cache_addb(0x68);	//PUSH val
	cache_addd(val);
	GenReg * genreg=FindDynReg(dr);
	cache_addb(0x50+genreg->index);		//PUSH reg

	/* Clear some unprotected registers */
	x86gen.regs[X86_REG_ECX]->Clear();
	x86gen.regs[X86_REG_EDX]->Clear();
	/* Make sure reg_esp is current */
	if (DynRegs[G_ESP].flags & DYNFLG_CHANGED)
		DynRegs[G_ESP].genreg->Save();
	/* Do the actual call to the procedure */
	cache_addb(0xe8);
	switch (write_size) {
		case 1: cache_addd((uint32_t)mem_writeb_checked_pagefault - (uint32_t)cache_rwtox(cache.pos)-4); break;
		case 2: cache_addd((uint32_t)mem_writew_checked_pagefault - (uint32_t)cache_rwtox(cache.pos)-4); break;
		case 4: cache_addd((uint32_t)mem_writed_checked_pagefault - (uint32_t)cache_rwtox(cache.pos)-4); break;
		default: IllegalOption("gen_call_write");
	}

	cache_addw(0xc483);		//ADD ESP,8
	cache_addb(2*4);
	x86gen.regs[X86_REG_EAX]->notusable=false;
	gen_releasereg(dr);

#if defined (MACOSX)
	/* restore stack */
	cache_addb(0x5c);	// pop esp
#endif
}

static uint8_t * gen_create_branch(BranchTypes type) {
	/* First free all registers */
	cache_addw(0x70+type);
	return (cache.pos-1);
}

static void gen_fill_branch(uint8_t * data,uint8_t * from=cache.pos) {
#if C_DEBUG
	Bits len=from-data;
	if (len<0) len=-len;
	if (len>126) LOG_MSG("Big jump %d",len);
#endif
	*data=(from-data-1);
}

static uint8_t * gen_create_branch_long(BranchTypes type) {
	cache_addw(0x800f+(type<<8));
	cache_addd(0);
	return (cache.pos-4);
}

static void gen_fill_branch_long(uint8_t * data,uint8_t * from=cache.pos) {
	*(uint32_t*)data=(from-data-4);
}

static uint8_t * gen_create_jump(uint8_t * to=0) {
	/* First free all registers */
	cache_addb(0xe9);
	cache_addd(to-(cache.pos+4));
	return (cache.pos-4);
}

static void gen_fill_jump(uint8_t * data,uint8_t * to=cache.pos) {
	*(uint32_t*)data=(to-data-4);
}


static void gen_jmp_ptr(void * ptr,Bits imm=0) {
	cache_addb(0xa1);
	cache_addd((uint32_t)ptr);
	cache_addb(0xff);		//JMP EA
	if (!imm) {			//NO EBP
		cache_addb(0x20);
    } else if ((imm>=-128 && imm<=127)) {
		cache_addb(0x60);
		cache_addb(imm);
	} else {
		cache_addb(0xa0);
		cache_addd(imm);
	}
}

static void gen_save_flags(DynReg * dynreg) {
	if (GCC_UNLIKELY(x86gen.flagsactive)) IllegalOption("gen_save_flags");
	GenReg * genreg=FindDynReg(dynreg);
	cache_addb(0x8b);					//MOV REG,[esp]
	cache_addw(0x2404+(genreg->index << 3));
	dynreg->flags|=DYNFLG_CHANGED;
}

static void gen_load_flags(DynReg * dynreg) {
	if (GCC_UNLIKELY(x86gen.flagsactive)) IllegalOption("gen_load_flags");
	cache_addw(0xc483);				//ADD ESP,4
	cache_addb(0x4);
	GenReg * genreg=FindDynReg(dynreg);
	cache_addb(0x50+genreg->index);		//PUSH 32
}

static void gen_save_host_direct(void * data,Bits imm) {
	cache_addw(0x05c7);		//MOV [],dword
	cache_addd((uint32_t)data);
	cache_addd(imm);
}

static void gen_test_host_byte(void * data) {
	cache_addb(0x50); // push eax
	cache_addw(0x058a);	//mov al, byte []
	cache_addd((uint32_t)data);
	cache_addw(0xc084); // test al, al
	cache_addb(0x58); // pop eax
}

static void gen_return(BlockReturn retcode) {
	gen_protectflags();
	cache_addb(0x59);			//POP ECX, the flags
	if (retcode==0) cache_addw(0xc033);		//MOV EAX, 0
	else {
		cache_addb(0xb8);		//MOV EAX, retcode
		cache_addd(retcode);
	}
	cache_addb(0xc3);			//RET
}

static void gen_return_fast(BlockReturn retcode,bool ret_exception=false) {
	if (GCC_UNLIKELY(x86gen.flagsactive)) IllegalOption("gen_return_fast");
	cache_addw(0x0d8b);			//MOV ECX, the flags
	cache_addd((uint32_t)&cpu_regs.flags);
	if (!ret_exception) {
		cache_addw(0xc483);			//ADD ESP,4
		cache_addb(0x4);
		if (retcode==0) cache_addw(0xc033);		//MOV EAX, 0
		else {
			cache_addb(0xb8);		//MOV EAX, retcode
			cache_addd(retcode);
		}
	}
	cache_addb(0xc3);			//RET
}

static void gen_init(void) {
	x86gen.regs[X86_REG_EAX]=new GenReg(0);
	x86gen.regs[X86_REG_ECX]=new GenReg(1);
	x86gen.regs[X86_REG_EDX]=new GenReg(2);
	x86gen.regs[X86_REG_EBX]=new GenReg(3);
	x86gen.regs[X86_REG_EBP]=new GenReg(5);
	x86gen.regs[X86_REG_ESI]=new GenReg(6);
	x86gen.regs[X86_REG_EDI]=new GenReg(7);
}

static void gen_free(void) {
	if (x86gen.regs[X86_REG_EAX]) {
		delete x86gen.regs[X86_REG_EAX];
		x86gen.regs[X86_REG_EAX] = NULL;
	}
	if (x86gen.regs[X86_REG_ECX]) {
		delete x86gen.regs[X86_REG_ECX];
		x86gen.regs[X86_REG_ECX] = NULL;
	}
	if (x86gen.regs[X86_REG_EDX]) {
		delete x86gen.regs[X86_REG_EDX];
		x86gen.regs[X86_REG_EDX] = NULL;
	}
	if (x86gen.regs[X86_REG_EBX]) {
		delete x86gen.regs[X86_REG_EBX];
		x86gen.regs[X86_REG_EBX] = NULL;
	}
	if (x86gen.regs[X86_REG_EBP]) {
		delete x86gen.regs[X86_REG_EBP];
		x86gen.regs[X86_REG_EBP] = NULL;
	}
	if (x86gen.regs[X86_REG_ESI]) {
		delete x86gen.regs[X86_REG_ESI];
		x86gen.regs[X86_REG_ESI] = NULL;
	}
	if (x86gen.regs[X86_REG_EDI]) {
		delete x86gen.regs[X86_REG_EDI];
		x86gen.regs[X86_REG_EDI] = NULL;
	}
}

#if defined(X86_DYNFPU_DH_ENABLED)
static void gen_dh_fpu_save(void)
#if defined (_MSC_VER)
{
	__asm {
	__asm	fnsave	dyn_dh_fpu.state
	__asm	fldcw	dyn_dh_fpu.host_cw
	}
	dyn_dh_fpu.state_used=false;
	dyn_dh_fpu.state.cw|=0x3f;
}
#else
{
	__asm__ volatile (
		"fnsave		%0			\n"
		"fldcw		%1			\n"
		:	"=m" (dyn_dh_fpu.state)
		:	"m" (dyn_dh_fpu.host_cw)
		:	"memory"
	);
	dyn_dh_fpu.state_used=false;
	dyn_dh_fpu.state.cw|=0x3f;
}
#endif
#endif
