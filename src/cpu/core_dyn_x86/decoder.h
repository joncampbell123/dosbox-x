/*
 *  Copyright (C) 2002-2013  The DOSBox Team
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */


#define X86_DYNFPU_DH_ENABLED
#define X86_INLINED_MEMACCESS


enum REP_Type {
	REP_NONE=0,REP_NZ,REP_Z
};

static struct DynDecode {
	PhysPt code;
	PhysPt code_start;
	PhysPt op_start;
	bool big_op;
	bool big_addr;
	REP_Type rep;
	Bitu cycles;
	CacheBlock * block;
	CacheBlock * active_block;
	struct {
		CodePageHandler * code;	
		Bitu index;
		Bit8u * wmap;
		Bit8u * invmap;
		Bitu first;
	} page;
	struct {
		Bitu val;
		Bitu mod;
		Bitu rm;
		Bitu reg;
	} modrm;
	DynReg * segprefix;
} decode;

static bool MakeCodePage(Bitu lin_addr,CodePageHandler * &cph) {
	Bit8u rdval;
	//Ensure page contains memory:
	if (GCC_UNLIKELY(mem_readb_checked(lin_addr,&rdval))) return true;
	PageHandler * handler=get_tlb_readhandler(lin_addr);
	if (handler->getFlags() & PFLAG_HASCODE) {
		cph=( CodePageHandler *)handler;
		return false;
	}
	if (handler->getFlags() & PFLAG_NOCODE) {
			LOG_MSG("DYNX86:Can't run code in this page!");
			cph=0;		return false;
		}
	Bitu lin_page=lin_addr >> 12;
	Bitu phys_page=lin_page;
	if (!PAGING_MakePhysPage(phys_page)) {
		LOG_MSG("DYNX86:Can't find physpage for lin addr %x", lin_addr);
		cph=0;		return false;
	}
	/* Find a free CodePage */
	if (!cache.free_pages) {
		if (cache.used_pages!=decode.page.code) cache.used_pages->ClearRelease();
		else {
			if ((cache.used_pages->next) && (cache.used_pages->next!=decode.page.code))
				cache.used_pages->next->ClearRelease();
			else {
				LOG_MSG("DYNX86:Invalid cache links");
				cache.used_pages->ClearRelease();
			}
		}
	}
	CodePageHandler * cpagehandler=cache.free_pages;
	cache.free_pages=cache.free_pages->next;
	cpagehandler->prev=cache.last_page;
	cpagehandler->next=0;
	if (cache.last_page) cache.last_page->next=cpagehandler;
	cache.last_page=cpagehandler;
	if (!cache.used_pages) cache.used_pages=cpagehandler;
	cpagehandler->SetupAt(phys_page,handler);
	MEM_SetPageHandler(phys_page,1,cpagehandler);
	PAGING_UnlinkPages(lin_page,1);
	cph=cpagehandler;
	return false;
}

static Bit8u decode_fetchb(void) {
	if (GCC_UNLIKELY(decode.page.index>=4096)) {
        /* Advance to the next page */
		decode.active_block->page.end=4095;
		/* trigger possible page fault here */
		decode.page.first++;
		Bitu fetchaddr=decode.page.first << 12;
		mem_readb(fetchaddr);
		MakeCodePage(fetchaddr,decode.page.code);
		CacheBlock * newblock=cache_getblock();
		decode.active_block->crossblock=newblock;
		newblock->crossblock=decode.active_block;
		decode.active_block=newblock;
		decode.active_block->page.start=0;
		decode.page.code->AddCrossBlock(decode.active_block);
		decode.page.wmap=decode.page.code->write_map;
		decode.page.invmap=decode.page.code->invalidation_map;
		decode.page.index=0;
	}
	decode.page.wmap[decode.page.index]+=0x01;
	decode.page.index++;
	decode.code+=1;
	return mem_readb(decode.code-1);
}
static Bit16u decode_fetchw(void) {
	if (GCC_UNLIKELY(decode.page.index>=4095)) {
   		Bit16u val=decode_fetchb();
		val|=decode_fetchb() << 8;
		return val;
	}
	*(Bit16u *)&decode.page.wmap[decode.page.index]+=0x0101;
	decode.code+=2;decode.page.index+=2;
	return mem_readw(decode.code-2);
}
static Bit32u decode_fetchd(void) {
	if (GCC_UNLIKELY(decode.page.index>=4093)) {
   		Bit32u val=decode_fetchb();
		val|=decode_fetchb() << 8;
		val|=decode_fetchb() << 16;
		val|=decode_fetchb() << 24;
		return val;
        /* Advance to the next page */
	}
	*(Bit32u *)&decode.page.wmap[decode.page.index]+=0x01010101;
	decode.code+=4;decode.page.index+=4;
	return mem_readd(decode.code-4);
}

#define START_WMMEM 64

static INLINE void decode_increase_wmapmask(Bitu size) {
	Bitu mapidx;
	CacheBlock* activecb=decode.active_block; 
	if (GCC_UNLIKELY(!activecb->cache.wmapmask)) {
		activecb->cache.wmapmask=(Bit8u*)malloc(START_WMMEM);
		memset(activecb->cache.wmapmask,0,START_WMMEM);
		activecb->cache.maskstart=decode.page.index;
		activecb->cache.masklen=START_WMMEM;
		mapidx=0;
	} else {
		mapidx=decode.page.index-activecb->cache.maskstart;
		if (GCC_UNLIKELY(mapidx+size>=activecb->cache.masklen)) {
			Bitu newmasklen=activecb->cache.masklen*4;
			if (newmasklen<mapidx+size) newmasklen=((mapidx+size)&~3)*2;
			Bit8u* tempmem=(Bit8u*)malloc(newmasklen);
			memset(tempmem,0,newmasklen);
			memcpy(tempmem,activecb->cache.wmapmask,activecb->cache.masklen);
			free(activecb->cache.wmapmask);
			activecb->cache.wmapmask=tempmem;
			activecb->cache.masklen=newmasklen;
		}
	}
	switch (size) {
		case 1 : activecb->cache.wmapmask[mapidx]+=0x01; break;
		case 2 : (*(Bit16u*)&activecb->cache.wmapmask[mapidx])+=0x0101; break;
		case 4 : (*(Bit32u*)&activecb->cache.wmapmask[mapidx])+=0x01010101; break;
	}
}

static bool decode_fetchb_imm(Bitu & val) {
	if (decode.page.index<4096) {
		if (decode.page.invmap != NULL) {
			if (decode.page.invmap[decode.page.index] == 0) {
				val=(Bit32u)decode_fetchb();
				return false;
			}
			HostPt tlb_addr=get_tlb_read(decode.code);
			if (tlb_addr) {
				val=(Bitu)(tlb_addr+decode.code);
				decode_increase_wmapmask(1);
				decode.code++;
				decode.page.index++;
				return true;
			}
		}
	}
	val=(Bit32u)decode_fetchb();
	return false;
}
static bool decode_fetchw_imm(Bitu & val) {
	if (decode.page.index<4095) {
        if (decode.page.invmap != NULL) {
            if ((decode.page.invmap[decode.page.index] == 0) &&
                (decode.page.invmap[decode.page.index + 1] == 0)
            ) {
				val=decode_fetchw();
				return false;
			}
			HostPt tlb_addr=get_tlb_read(decode.code);
			if (tlb_addr) {
				val=(Bitu)(tlb_addr+decode.code);
				decode_increase_wmapmask(2);
				decode.code+=2;
				decode.page.index+=2;
				return true;
			}
		}
	}
	val=decode_fetchw();
	return false;
}
static bool decode_fetchd_imm(Bitu & val) {
	if (decode.page.index<4093) {
        if (decode.page.invmap != NULL) {
            if ((decode.page.invmap[decode.page.index] == 0) &&
                (decode.page.invmap[decode.page.index + 1] == 0) &&
                (decode.page.invmap[decode.page.index + 2] == 0) &&
                (decode.page.invmap[decode.page.index + 3] == 0)
            ) {
				val=decode_fetchd();
				return false;
			}
			HostPt tlb_addr=get_tlb_read(decode.code);
			if (tlb_addr) {
				val=(Bitu)(tlb_addr+decode.code);
				decode_increase_wmapmask(4);
				decode.code+=4;
				decode.page.index+=4;
				return true;
			}
		}
	}
	val=decode_fetchd();
	return false;
}


static void dyn_reduce_cycles(void) {
	gen_protectflags();
	if (!decode.cycles) decode.cycles++;
	gen_dop_word_imm(DOP_SUB,true,DREG(CYCLES),decode.cycles);
}

static void dyn_save_noncritical_regs(void) {
	gen_releasereg(DREG(EAX));
	gen_releasereg(DREG(ECX));
	gen_releasereg(DREG(EDX));
	gen_releasereg(DREG(EBX));
	gen_releasereg(DREG(ESP));
	gen_releasereg(DREG(EBP));
	gen_releasereg(DREG(ESI));
	gen_releasereg(DREG(EDI));
}

static void dyn_save_critical_regs(void) {
	dyn_save_noncritical_regs();
	gen_releasereg(DREG(FLAGS));
	gen_releasereg(DREG(EIP));
	gen_releasereg(DREG(CYCLES));
}

static void dyn_set_eip_last_end(DynReg * endreg) {
	gen_protectflags();
	gen_lea(endreg,DREG(EIP),0,0,decode.code-decode.code_start);
	gen_dop_word_imm(DOP_ADD,decode.big_op,DREG(EIP),decode.op_start-decode.code_start);
}

static INLINE void dyn_set_eip_end(void) {
	gen_protectflags();
	gen_dop_word_imm(DOP_ADD,cpu.code.big,DREG(EIP),decode.code-decode.code_start);
}

static INLINE void dyn_set_eip_end(DynReg * endreg) {
	gen_protectflags();
	if (cpu.code.big) gen_dop_word(DOP_MOV,true,DREG(TMPW),DREG(EIP));
	else gen_extend_word(false,DREG(TMPW),DREG(EIP));
	gen_dop_word_imm(DOP_ADD,cpu.code.big,DREG(TMPW),decode.code-decode.code_start);
}

static INLINE void dyn_set_eip_last(void) {
	gen_protectflags();
	gen_dop_word_imm(DOP_ADD,cpu.code.big,DREG(EIP),decode.op_start-decode.code_start);
}


enum save_info_type {db_exception, cycle_check, normal, fpu_restore};


static struct {
	save_info_type type;
	DynState state;
	Bit8u * branch_pos;
	Bit32u eip_change;
	Bitu cycles;
	Bit8u * return_pos;
} save_info[512];

Bitu used_save_info=0;


static BlockReturn DynRunException(Bit32u eip_add,Bit32u cycle_sub,Bit32u dflags) {
	reg_flags=(dflags&FMASK_TEST) | (reg_flags&(~FMASK_TEST));
	reg_eip+=eip_add;
	CPU_Cycles-=cycle_sub;
	if (cpu.exception.which==SMC_CURRENT_BLOCK) return BR_SMCBlock;
	CPU_Exception(cpu.exception.which,cpu.exception.error);
	return BR_Normal;
}

static void dyn_check_bool_exception(DynReg * check) {
	gen_dop_byte(DOP_OR,check,0,check,0);
	save_info[used_save_info].branch_pos=gen_create_branch_long(BR_NZ);
	dyn_savestate(&save_info[used_save_info].state);
	if (!decode.cycles) decode.cycles++;
	save_info[used_save_info].cycles=decode.cycles;
	save_info[used_save_info].eip_change=decode.op_start-decode.code_start;
	if (!cpu.code.big) save_info[used_save_info].eip_change&=0xffff;
	save_info[used_save_info].type=db_exception;
	used_save_info++;
}

static void dyn_check_bool_exception_al(void) {
	cache_addw(0xc00a);		// or al, al
	save_info[used_save_info].branch_pos=gen_create_branch_long(BR_NZ);
	dyn_savestate(&save_info[used_save_info].state);
	if (!decode.cycles) decode.cycles++;
	save_info[used_save_info].cycles=decode.cycles;
	save_info[used_save_info].eip_change=decode.op_start-decode.code_start;
	if (!cpu.code.big) save_info[used_save_info].eip_change&=0xffff;
	save_info[used_save_info].type=db_exception;
	used_save_info++;
}

#include "pic.h"

static void dyn_check_irqrequest(void) {
	gen_load_host(&PIC_IRQCheck,DREG(TMPB),4);
	gen_dop_word(DOP_OR,true,DREG(TMPB),DREG(TMPB));
	save_info[used_save_info].branch_pos=gen_create_branch_long(BR_NZ);
	gen_releasereg(DREG(TMPB));
	dyn_savestate(&save_info[used_save_info].state);
	if (!decode.cycles) decode.cycles++;
	save_info[used_save_info].cycles=decode.cycles;
	save_info[used_save_info].eip_change=decode.code-decode.code_start;
	if (!cpu.code.big) save_info[used_save_info].eip_change&=0xffff;
	save_info[used_save_info].type=normal;
	used_save_info++;
}

static void dyn_check_bool_exception_ne(void) {
	save_info[used_save_info].branch_pos=gen_create_branch_long(BR_Z);
	dyn_savestate(&save_info[used_save_info].state);
	if (!decode.cycles) decode.cycles++;
	save_info[used_save_info].cycles=decode.cycles;
	save_info[used_save_info].eip_change=decode.op_start-decode.code_start;
	if (!cpu.code.big) save_info[used_save_info].eip_change&=0xffff;
	save_info[used_save_info].type=db_exception;
	used_save_info++;
}

static void dyn_fill_blocks(void) {
	for (Bitu sct=0; sct<used_save_info; sct++) {
		gen_fill_branch_long(save_info[sct].branch_pos);
		switch (save_info[sct].type) {
			case db_exception:
				dyn_loadstate(&save_info[sct].state);
				decode.cycles=save_info[sct].cycles;
				dyn_save_critical_regs();
				if (cpu.code.big) gen_call_function((void *)&DynRunException,"%Id%Id%F",save_info[sct].eip_change,save_info[sct].cycles);
				else gen_call_function((void *)&DynRunException,"%Iw%Id%F",save_info[sct].eip_change,save_info[sct].cycles);
				gen_return_fast(BR_Normal,true);
				break;
			case cycle_check:
				gen_return(BR_Cycles);
				break;
			case normal:
				dyn_loadstate(&save_info[sct].state);
				gen_dop_word_imm(DOP_ADD,decode.big_op,DREG(EIP),save_info[sct].eip_change);
				dyn_save_critical_regs();
				gen_return(BR_Cycles);
				break;
			case fpu_restore:
				dyn_loadstate(&save_info[sct].state);
				gen_load_host(&dyn_dh_fpu.state_used,DREG(TMPB),4);
				gen_sop_word(SOP_INC,true,DREG(TMPB));
				GenReg * gr1=FindDynReg(DREG(TMPB));
				cache_addb(0xdd);	// FRSTOR fpu.state (fpu_restore)
				cache_addb(0x25);
				cache_addd((Bit32u)(&(dyn_dh_fpu.state[0])));
				cache_addb(0x89);	// mov fpu.state_used,1
				cache_addb(0x05|(gr1->index<<3));
				cache_addd((Bit32u)(&(dyn_dh_fpu.state_used)));
				gen_releasereg(DREG(TMPB));
				dyn_synchstate(&save_info[sct].state);
				gen_create_jump(save_info[sct].return_pos);
				break;
		}
	}
	used_save_info=0;
}


#if !defined(X86_INLINED_MEMACCESS)
static void dyn_read_byte(DynReg * addr,DynReg * dst,Bitu high) {
	gen_protectflags();
	gen_call_function((void *)&mem_readb_checked,"%Dd%Id",addr,&core_dyn.readdata);
	dyn_check_bool_exception_al();
	gen_mov_host(&core_dyn.readdata,dst,1,high);
}
static void dyn_write_byte(DynReg * addr,DynReg * val,Bitu high) {
	gen_protectflags();
	if (high) gen_call_function((void *)&mem_writeb_checked,"%Dd%Dh",addr,val);
	else gen_call_function((void *)&mem_writeb_checked,"%Dd%Dd",addr,val);
	dyn_check_bool_exception_al();
}
static void dyn_read_word(DynReg * addr,DynReg * dst,bool dword) {
	gen_protectflags();
	if (dword) gen_call_function((void *)&mem_readd_checked,"%Dd%Id",addr,&core_dyn.readdata);
	else gen_call_function((void *)&mem_readw_checked,"%Dd%Id",addr,&core_dyn.readdata);
	dyn_check_bool_exception_al();
	gen_mov_host(&core_dyn.readdata,dst,dword?4:2);
}
static void dyn_write_word(DynReg * addr,DynReg * val,bool dword) {
	gen_protectflags();
	if (dword) gen_call_function((void *)&mem_writed_checked,"%Dd%Dd",addr,val);
	else gen_call_function((void *)&mem_writew_checked,"%Dd%Dd",addr,val);
	dyn_check_bool_exception_al();
}
static void dyn_read_byte_release(DynReg * addr,DynReg * dst,Bitu high) {
	gen_protectflags();
	gen_call_function((void *)&mem_readb_checked,"%Ddr%Id",addr,&core_dyn.readdata);
	dyn_check_bool_exception_al();
	gen_mov_host(&core_dyn.readdata,dst,1,high);
}
static void dyn_write_byte_release(DynReg * addr,DynReg * val,Bitu high) {
	gen_protectflags();
	if (high) gen_call_function((void *)&mem_writeb_checked,"%Ddr%Dh",addr,val);
	else gen_call_function((void *)&mem_writeb_checked,"%Ddr%Dd",addr,val);
	dyn_check_bool_exception_al();
}
static void dyn_read_word_release(DynReg * addr,DynReg * dst,bool dword) {
	gen_protectflags();
	if (dword) gen_call_function((void *)&mem_readd_checked,"%Ddr%Id",addr,&core_dyn.readdata);
	else gen_call_function((void *)&mem_readw_checked,"%Ddr%Id",addr,&core_dyn.readdata);
	dyn_check_bool_exception_al();
	gen_mov_host(&core_dyn.readdata,dst,dword?4:2);
}
static void dyn_write_word_release(DynReg * addr,DynReg * val,bool dword) {
	gen_protectflags();
	if (dword) gen_call_function((void *)&mem_writed_checked,"%Ddr%Dd",addr,val);
	else gen_call_function((void *)&mem_writew_checked,"%Ddr%Dd",addr,val);
	dyn_check_bool_exception_al();
}

#else

static void dyn_read_intro(DynReg * addr,bool release_addr=true) {
	gen_protectflags();

	if (addr->genreg) {
		// addr already in a register
		Bit8u reg_idx=(Bit8u)addr->genreg->index;
		x86gen.regs[X86_REG_ECX]->Clear();
		if (reg_idx!=1) {
			cache_addw(0xc88b+(reg_idx<<8));	//Mov ecx,reg
		}
		x86gen.regs[X86_REG_EAX]->Clear();
		if (release_addr) gen_releasereg(addr);
	} else {
		// addr still in memory, directly move into ecx
		x86gen.regs[X86_REG_EAX]->Clear();
		x86gen.regs[X86_REG_ECX]->Clear();
		cache_addw(0x0d8b);		//Mov ecx,[data]
		cache_addd((Bit32u)addr->data);
	}
	x86gen.regs[X86_REG_EDX]->Clear();

	cache_addw(0xc18b);		// mov eax,ecx
}

bool mem_readb_checked_dcx86(PhysPt address) {
	return get_tlb_readhandler(address)->readb_checked(address, (Bit8u*)(&core_dyn.readdata));
}

static void dyn_read_byte(DynReg * addr,DynReg * dst,Bitu high) {
	dyn_read_intro(addr,false);

	cache_addw(0xe8c1);		// shr eax,0x0c
	cache_addb(0x0c);
	cache_addw(0x048b);		// mov eax,paging.tlb.read[eax*TYPE Bit32u]
	cache_addb(0x85);
	cache_addd((Bit32u)(&paging.tlb.read[0]));
	cache_addw(0xc085);		// test eax,eax
	Bit8u* je_loc=gen_create_branch(BR_Z);


	cache_addw(0x048a);		// mov al,[eax+ecx]
	cache_addb(0x08);

	Bit8u* jmp_loc=gen_create_jump();
	gen_fill_branch(je_loc);
	cache_addb(0x51);		// push ecx
	cache_addb(0xe8);
	cache_addd(((Bit32u)&mem_readb_checked_dcx86) - (Bit32u)cache.pos-4);
	cache_addw(0xc483);		// add esp,4
	cache_addb(0x04);
	cache_addw(0x012c);		// sub al,1

	dyn_check_bool_exception_ne();

	cache_addw(0x058a);		//mov al,[]
	cache_addd((Bit32u)(&core_dyn.readdata));

	gen_fill_jump(jmp_loc);

	x86gen.regs[X86_REG_EAX]->notusable=true;
	GenReg * genreg=FindDynReg(dst);
	x86gen.regs[X86_REG_EAX]->notusable=false;
	cache_addw(0xc08a+(genreg->index<<11)+(high?0x2000:0));
	dst->flags|=DYNFLG_CHANGED;
}

static void dyn_read_byte_release(DynReg * addr,DynReg * dst,Bitu high) {
	dyn_read_intro(addr);

	cache_addw(0xe8c1);		// shr eax,0x0c
	cache_addb(0x0c);
	cache_addw(0x048b);		// mov eax,paging.tlb.read[eax*TYPE Bit32u]
	cache_addb(0x85);
	cache_addd((Bit32u)(&paging.tlb.read[0]));
	cache_addw(0xc085);		// test eax,eax
	Bit8u* je_loc=gen_create_branch(BR_Z);


	cache_addw(0x048a);		// mov al,[eax+ecx]
	cache_addb(0x08);

	Bit8u* jmp_loc=gen_create_jump();
	gen_fill_branch(je_loc);
	cache_addb(0x51);		// push ecx
	cache_addb(0xe8);
	cache_addd(((Bit32u)&mem_readb_checked_dcx86) - (Bit32u)cache.pos-4);
	cache_addw(0xc483);		// add esp,4
	cache_addb(0x04);
	cache_addw(0x012c);		// sub al,1

	dyn_check_bool_exception_ne();

	cache_addw(0x058a);		//mov al,[]
	cache_addd((Bit32u)(&core_dyn.readdata));

	gen_fill_jump(jmp_loc);

	x86gen.regs[X86_REG_EAX]->notusable=true;
	GenReg * genreg=FindDynReg(dst);
	x86gen.regs[X86_REG_EAX]->notusable=false;
	cache_addw(0xc08a+(genreg->index<<11)+(high?0x2000:0));
	dst->flags|=DYNFLG_CHANGED;
}

bool mem_readd_checked_dcx86(PhysPt address) {
	if ((address & 0xfff)<0xffd) {
		HostPt tlb_addr=get_tlb_read(address);
		if (tlb_addr) {
			core_dyn.readdata=host_readd(tlb_addr+address);
			return false;
		} else {
			return get_tlb_readhandler(address)->readd_checked(address, &core_dyn.readdata);
		}
	} else return mem_unalignedreadd_checked(address, &core_dyn.readdata);
}

static void dyn_read_word(DynReg * addr,DynReg * dst,bool dword) {
	if (dword) {
		dyn_read_intro(addr,false);

		cache_addw(0xe8d1);		// shr eax,0x1
		Bit8u* jb_loc1=gen_create_branch(BR_B);
		cache_addw(0xe8d1);		// shr eax,0x1
		Bit8u* jb_loc2=gen_create_branch(BR_B);
		cache_addw(0xe8c1);		// shr eax,0x0a
		cache_addb(0x0a);
		cache_addw(0x048b);		// mov eax,paging.tlb.read[eax*TYPE Bit32u]
		cache_addb(0x85);
		cache_addd((Bit32u)(&paging.tlb.read[0]));
		cache_addw(0xc085);		// test eax,eax
		Bit8u* je_loc=gen_create_branch(BR_Z);

		GenReg * genreg=FindDynReg(dst,true);

		cache_addw(0x048b+(genreg->index <<(8+3)));		// mov dest,[eax+ecx]
		cache_addb(0x08);

		Bit8u* jmp_loc=gen_create_jump();
		gen_fill_branch(jb_loc1);
		gen_fill_branch(jb_loc2);
		gen_fill_branch(je_loc);
		cache_addb(0x51);		// push ecx
		cache_addb(0xe8);
		cache_addd(((Bit32u)&mem_readd_checked_dcx86) - (Bit32u)cache.pos-4);
		cache_addw(0xc483);		// add esp,4
		cache_addb(0x04);
		cache_addw(0x012c);		// sub al,1

		dyn_check_bool_exception_ne();

		gen_mov_host(&core_dyn.readdata,dst,4);
		dst->flags|=DYNFLG_CHANGED;

		gen_fill_jump(jmp_loc);
	} else {
		gen_protectflags();
		gen_call_function((void *)&mem_readw_checked,"%Dd%Id",addr,&core_dyn.readdata);
		dyn_check_bool_exception_al();
		gen_mov_host(&core_dyn.readdata,dst,2);
	}
}

static void dyn_read_word_release(DynReg * addr,DynReg * dst,bool dword) {
	if (dword) {
		dyn_read_intro(addr);

		cache_addw(0xe8d1);		// shr eax,0x1
		Bit8u* jb_loc1=gen_create_branch(BR_B);
		cache_addw(0xe8d1);		// shr eax,0x1
		Bit8u* jb_loc2=gen_create_branch(BR_B);
		cache_addw(0xe8c1);		// shr eax,0x0a
		cache_addb(0x0a);
		cache_addw(0x048b);		// mov eax,paging.tlb.read[eax*TYPE Bit32u]
		cache_addb(0x85);
		cache_addd((Bit32u)(&paging.tlb.read[0]));
		cache_addw(0xc085);		// test eax,eax
		Bit8u* je_loc=gen_create_branch(BR_Z);

		GenReg * genreg=FindDynReg(dst,true);

		cache_addw(0x048b+(genreg->index <<(8+3)));		// mov dest,[eax+ecx]
		cache_addb(0x08);

		Bit8u* jmp_loc=gen_create_jump();
		gen_fill_branch(jb_loc1);
		gen_fill_branch(jb_loc2);
		gen_fill_branch(je_loc);
		cache_addb(0x51);		// push ecx
		cache_addb(0xe8);
		cache_addd(((Bit32u)&mem_readd_checked_dcx86) - (Bit32u)cache.pos-4);
		cache_addw(0xc483);		// add esp,4
		cache_addb(0x04);
		cache_addw(0x012c);		// sub al,1

		dyn_check_bool_exception_ne();

		gen_mov_host(&core_dyn.readdata,dst,4);
		dst->flags|=DYNFLG_CHANGED;

		gen_fill_jump(jmp_loc);
	} else {
		gen_protectflags();
		gen_call_function((void *)&mem_readw_checked,"%Ddr%Id",addr,&core_dyn.readdata);
		dyn_check_bool_exception_al();
		gen_mov_host(&core_dyn.readdata,dst,2);
	}
}

static void dyn_write_intro(DynReg * addr,bool release_addr=true) {
	gen_protectflags();

	if (addr->genreg) {
		// addr in a register
		Bit8u reg_idx_addr=(Bit8u)addr->genreg->index;

		x86gen.regs[X86_REG_EAX]->Clear();
		x86gen.regs[X86_REG_EAX]->notusable=true;
		x86gen.regs[X86_REG_ECX]->Clear();
		x86gen.regs[X86_REG_ECX]->notusable=true;

		if (reg_idx_addr) {
			// addr!=eax
			cache_addb(0x8b);		//Mov eax,reg
			cache_addb(0xc0+reg_idx_addr);
		}
		if (release_addr) gen_releasereg(addr);
	} else {
		// addr still in memory, directly move into eax
		x86gen.regs[X86_REG_EAX]->Clear();
		x86gen.regs[X86_REG_EAX]->notusable=true;
		x86gen.regs[X86_REG_ECX]->Clear();
		x86gen.regs[X86_REG_ECX]->notusable=true;
		cache_addb(0xa1);		//Mov eax,[data]
		cache_addd((Bit32u)addr->data);
	}

	cache_addw(0xc88b);		// mov ecx,eax
}

static void dyn_write_byte(DynReg * addr,DynReg * val,bool high) {
	dyn_write_intro(addr,false);

	GenReg * genreg=FindDynReg(val);
	cache_addw(0xe9c1);		// shr ecx,0x0c
	cache_addb(0x0c);
	cache_addw(0x0c8b);		// mov ecx,paging.tlb.read[ecx*TYPE Bit32u]
	cache_addb(0x8d);
	cache_addd((Bit32u)(&paging.tlb.write[0]));
	cache_addw(0xc985);		// test ecx,ecx
	Bit8u* je_loc=gen_create_branch(BR_Z);

	cache_addw(0x0488+(genreg->index<<11)+(high?0x2000:0));		// mov [eax+ecx],reg
	cache_addb(0x08);

	Bit8u* jmp_loc=gen_create_jump();
	gen_fill_branch(je_loc);

	if (GCC_UNLIKELY(high)) cache_addw(0xe086+((genreg->index+(genreg->index<<3))<<8));
	cache_addb(0x52);	// push edx
	cache_addb(0x50+genreg->index);
	cache_addb(0x50);	// push eax
	if (GCC_UNLIKELY(high)) cache_addw(0xe086+((genreg->index+(genreg->index<<3))<<8));
	cache_addb(0xe8);
	cache_addd(((Bit32u)&mem_writeb_checked) - (Bit32u)cache.pos-4);
	cache_addw(0xc483);		// add esp,8
	cache_addb(0x08);
	cache_addw(0x012c);		// sub al,1
	cache_addb(0x5a);		// pop edx

	// Restore registers to be used again
	x86gen.regs[X86_REG_EAX]->notusable=false;
	x86gen.regs[X86_REG_ECX]->notusable=false;

	dyn_check_bool_exception_ne();

	gen_fill_jump(jmp_loc);
}

static void dyn_write_byte_release(DynReg * addr,DynReg * val,bool high) {
	dyn_write_intro(addr);

	GenReg * genreg=FindDynReg(val);
	cache_addw(0xe9c1);		// shr ecx,0x0c
	cache_addb(0x0c);
	cache_addw(0x0c8b);		// mov ecx,paging.tlb.read[ecx*TYPE Bit32u]
	cache_addb(0x8d);
	cache_addd((Bit32u)(&paging.tlb.write[0]));
	cache_addw(0xc985);		// test ecx,ecx
	Bit8u* je_loc=gen_create_branch(BR_Z);

	cache_addw(0x0488+(genreg->index<<11)+(high?0x2000:0));		// mov [eax+ecx],reg
	cache_addb(0x08);

	Bit8u* jmp_loc=gen_create_jump();
	gen_fill_branch(je_loc);

	cache_addb(0x52);	// push edx
	if (GCC_UNLIKELY(high)) cache_addw(0xe086+((genreg->index+(genreg->index<<3))<<8));
	cache_addb(0x50+genreg->index);
	cache_addb(0x50);	// push eax
	if (GCC_UNLIKELY(high)) cache_addw(0xe086+((genreg->index+(genreg->index<<3))<<8));
	cache_addb(0xe8);
	cache_addd(((Bit32u)&mem_writeb_checked) - (Bit32u)cache.pos-4);
	cache_addw(0xc483);		// add esp,8
	cache_addb(0x08);
	cache_addw(0x012c);		// sub al,1
	cache_addb(0x5a);		// pop edx

	// Restore registers to be used again
	x86gen.regs[X86_REG_EAX]->notusable=false;
	x86gen.regs[X86_REG_ECX]->notusable=false;

	dyn_check_bool_exception_ne();

	gen_fill_jump(jmp_loc);
}

static void dyn_write_word(DynReg * addr,DynReg * val,bool dword) {
	if (dword) {
		dyn_write_intro(addr,false);

		GenReg * genreg=FindDynReg(val);
		cache_addw(0xe9d1);		// shr ecx,0x1
		Bit8u* jb_loc1=gen_create_branch(BR_B);
		cache_addw(0xe9d1);		// shr ecx,0x1
		Bit8u* jb_loc2=gen_create_branch(BR_B);
		cache_addw(0xe9c1);		// shr ecx,0x0a
		cache_addb(0x0a);
		cache_addw(0x0c8b);		// mov ecx,paging.tlb.read[ecx*TYPE Bit32u]
		cache_addb(0x8d);
		cache_addd((Bit32u)(&paging.tlb.write[0]));
		cache_addw(0xc985);		// test ecx,ecx
		Bit8u* je_loc=gen_create_branch(BR_Z);

		cache_addw(0x0489+(genreg->index <<(8+3)));		// mov [eax+ecx],reg
		cache_addb(0x08);

		Bit8u* jmp_loc=gen_create_jump();
		gen_fill_branch(jb_loc1);
		gen_fill_branch(jb_loc2);
		gen_fill_branch(je_loc);

		cache_addb(0x52);	// push edx
		cache_addb(0x50+genreg->index);
		cache_addb(0x50);	// push eax
		cache_addb(0xe8);
		cache_addd(((Bit32u)&mem_writed_checked) - (Bit32u)cache.pos-4);
		cache_addw(0xc483);		// add esp,8
		cache_addb(0x08);
		cache_addw(0x012c);		// sub al,1
		cache_addb(0x5a);		// pop edx

		// Restore registers to be used again
		x86gen.regs[X86_REG_EAX]->notusable=false;
		x86gen.regs[X86_REG_ECX]->notusable=false;

		dyn_check_bool_exception_ne();

		gen_fill_jump(jmp_loc);
	} else {
		gen_protectflags();
		gen_call_function((void *)&mem_writew_checked,"%Dd%Dd",addr,val);
		dyn_check_bool_exception_al();
	}
}

static void dyn_write_word_release(DynReg * addr,DynReg * val,bool dword) {
	if (dword) {
		dyn_write_intro(addr);

		GenReg * genreg=FindDynReg(val);
		cache_addw(0xe9d1);		// shr ecx,0x1
		Bit8u* jb_loc1=gen_create_branch(BR_B);
		cache_addw(0xe9d1);		// shr ecx,0x1
		Bit8u* jb_loc2=gen_create_branch(BR_B);
		cache_addw(0xe9c1);		// shr ecx,0x0a
		cache_addb(0x0a);
		cache_addw(0x0c8b);		// mov ecx,paging.tlb.read[ecx*TYPE Bit32u]
		cache_addb(0x8d);
		cache_addd((Bit32u)(&paging.tlb.write[0]));
		cache_addw(0xc985);		// test ecx,ecx
		Bit8u* je_loc=gen_create_branch(BR_Z);

		cache_addw(0x0489+(genreg->index <<(8+3)));		// mov [eax+ecx],reg
		cache_addb(0x08);

		Bit8u* jmp_loc=gen_create_jump();
		gen_fill_branch(jb_loc1);
		gen_fill_branch(jb_loc2);
		gen_fill_branch(je_loc);

		cache_addb(0x52);	// push edx
		cache_addb(0x50+genreg->index);
		cache_addb(0x50);	// push eax
		cache_addb(0xe8);
		cache_addd(((Bit32u)&mem_writed_checked) - (Bit32u)cache.pos-4);
		cache_addw(0xc483);		// add esp,8
		cache_addb(0x08);
		cache_addw(0x012c);		// sub al,1
		cache_addb(0x5a);		// pop edx

		// Restore registers to be used again
		x86gen.regs[X86_REG_EAX]->notusable=false;
		x86gen.regs[X86_REG_ECX]->notusable=false;

		dyn_check_bool_exception_ne();

		gen_fill_jump(jmp_loc);
	} else {
		gen_protectflags();
		gen_call_function((void *)&mem_writew_checked,"%Ddr%Dd",addr,val);
		dyn_check_bool_exception_al();
	}
}

#endif


static void dyn_push_unchecked(DynReg * dynreg) {
	gen_protectflags();
	gen_lea(DREG(STACK),DREG(ESP),0,0,decode.big_op?(-4):(-2));
	gen_dop_word_var(DOP_AND,true,DREG(STACK),&cpu.stack.mask);
	gen_dop_word_var(DOP_AND,true,DREG(ESP),&cpu.stack.notmask);
	gen_dop_word(DOP_OR,true,DREG(ESP),DREG(STACK));
	gen_dop_word(DOP_ADD,true,DREG(STACK),DREG(SS));
	if (decode.big_op) {
		gen_call_function((void *)&mem_writed,"%Drd%Dd",DREG(STACK),dynreg);
	} else {
		//Can just push the whole 32-bit word as operand
		gen_call_function((void *)&mem_writew,"%Drd%Dd",DREG(STACK),dynreg);
	}
}

static void dyn_push(DynReg * dynreg) {
	gen_protectflags();
	gen_lea(DREG(STACK),DREG(ESP),0,0,decode.big_op?(-4):(-2));
	gen_dop_word(DOP_MOV,true,DREG(NEWESP),DREG(ESP));
	gen_dop_word_var(DOP_AND,true,DREG(STACK),&cpu.stack.mask);
	gen_dop_word_var(DOP_AND,true,DREG(NEWESP),&cpu.stack.notmask);
	gen_dop_word(DOP_OR,true,DREG(NEWESP),DREG(STACK));
	gen_dop_word(DOP_ADD,true,DREG(STACK),DREG(SS));
	if (decode.big_op) {
		gen_call_function((void *)&mem_writed_checked,"%Drd%Dd",DREG(STACK),dynreg);
	} else {
		//Can just push the whole 32-bit word as operand
		gen_call_function((void *)&mem_writew_checked,"%Drd%Dd",DREG(STACK),dynreg);
	}
	dyn_check_bool_exception_al();
	/* everything was ok, change register now */
	gen_dop_word(DOP_MOV,true,DREG(ESP),DREG(NEWESP));
	gen_releasereg(DREG(NEWESP));
}

static void dyn_pop(DynReg * dynreg,bool checked=true) {
	gen_protectflags();
	gen_dop_word(DOP_MOV,true,DREG(STACK),DREG(ESP));
	gen_dop_word_var(DOP_AND,true,DREG(STACK),&cpu.stack.mask);
	gen_dop_word(DOP_ADD,true,DREG(STACK),DREG(SS));
	if (checked) {
		if (decode.big_op) {
			gen_call_function((void *)&mem_readd_checked,"%Drd%Id",DREG(STACK),&core_dyn.readdata);
		} else {
			gen_call_function((void *)&mem_readw_checked,"%Drd%Id",DREG(STACK),&core_dyn.readdata);
		}
		dyn_check_bool_exception_al();
		gen_mov_host(&core_dyn.readdata,dynreg,decode.big_op?4:2);
	} else {
		if (decode.big_op) {
			gen_call_function((void *)&mem_readd,"%Rd%Drd",dynreg,DREG(STACK));
		} else {
			gen_call_function((void *)&mem_readw,"%Rw%Drd",dynreg,DREG(STACK));
		}
	}
	if (dynreg!=DREG(ESP)) {
		gen_lea(DREG(STACK),DREG(ESP),0,0,decode.big_op?4:2);
		gen_dop_word_var(DOP_AND,true,DREG(STACK),&cpu.stack.mask);
		gen_dop_word_var(DOP_AND,true,DREG(ESP),&cpu.stack.notmask);
		gen_dop_word(DOP_OR,true,DREG(ESP),DREG(STACK));
	}
}

static INLINE void dyn_get_modrm(void) {
	decode.modrm.val=decode_fetchb();
	decode.modrm.mod=(decode.modrm.val >> 6) & 3;
	decode.modrm.reg=(decode.modrm.val >> 3) & 7;
	decode.modrm.rm=(decode.modrm.val & 7);
}

static void dyn_fill_ea(bool addseg=true, DynReg * reg_ea=DREG(EA)) {
	DynReg * segbase;
	if (!decode.big_addr) {
		Bits imm;
		switch (decode.modrm.mod) {
		case 0:imm=0;break;
		case 1:imm=(Bit8s)decode_fetchb();break;
		case 2:imm=(Bit16s)decode_fetchw();break;
		}
		DynReg * extend_src=reg_ea;
		switch (decode.modrm.rm) {
		case 0:/* BX+SI */
			gen_lea(reg_ea,DREG(EBX),DREG(ESI),0,imm);
			segbase=DREG(DS);
			break;
		case 1:/* BX+DI */
			gen_lea(reg_ea,DREG(EBX),DREG(EDI),0,imm);
			segbase=DREG(DS);
			break;
		case 2:/* BP+SI */
			gen_lea(reg_ea,DREG(EBP),DREG(ESI),0,imm);
			segbase=DREG(SS);
			break;
		case 3:/* BP+DI */
			gen_lea(reg_ea,DREG(EBP),DREG(EDI),0,imm);
			segbase=DREG(SS);
			break;
		case 4:/* SI */
			if (imm) gen_lea(reg_ea,DREG(ESI),0,0,imm);
			else extend_src=DREG(ESI);
			segbase=DREG(DS);
			break;
		case 5:/* DI */
			if (imm) gen_lea(reg_ea,DREG(EDI),0,0,imm);
			else extend_src=DREG(EDI);
			segbase=DREG(DS);
			break;
		case 6:/* imm/BP */
			if (!decode.modrm.mod) {
				imm=decode_fetchw();
                gen_dop_word_imm(DOP_MOV,true,reg_ea,imm);
				segbase=DREG(DS);
				goto skip_extend_word;
			} else {
				gen_lea(reg_ea,DREG(EBP),0,0,imm);
				segbase=DREG(SS);
			}
			break;
		case 7: /* BX */
			if (imm) gen_lea(reg_ea,DREG(EBX),0,0,imm);
			else extend_src=DREG(EBX);
			segbase=DREG(DS);
			break;
		}
		gen_extend_word(false,reg_ea,extend_src);
skip_extend_word:
		if (addseg) {
			gen_lea(reg_ea,reg_ea,decode.segprefix ? decode.segprefix : segbase,0,0);
		}
	} else {
		Bits imm=0;
		DynReg * base=0;DynReg * scaled=0;Bitu scale=0;
		switch (decode.modrm.rm) {
		case 0:base=DREG(EAX);segbase=DREG(DS);break;
		case 1:base=DREG(ECX);segbase=DREG(DS);break;
		case 2:base=DREG(EDX);segbase=DREG(DS);break;
		case 3:base=DREG(EBX);segbase=DREG(DS);break;
		case 4:	/* SIB */
			{
				Bitu sib=decode_fetchb();
				static DynReg * scaledtable[8]={
					DREG(EAX),DREG(ECX),DREG(EDX),DREG(EBX),
							0,DREG(EBP),DREG(ESI),DREG(EDI),
				};
				scaled=scaledtable[(sib >> 3) &7];
				scale=(sib >> 6);
				switch (sib & 7) {
				case 0:base=DREG(EAX);segbase=DREG(DS);break;
				case 1:base=DREG(ECX);segbase=DREG(DS);break;
				case 2:base=DREG(EDX);segbase=DREG(DS);break;
				case 3:base=DREG(EBX);segbase=DREG(DS);break;
				case 4:base=DREG(ESP);segbase=DREG(SS);break;
				case 5:
					if (decode.modrm.mod) {
						base=DREG(EBP);segbase=DREG(SS);
					} else {
						segbase=DREG(DS);
						Bitu val;
						if (decode_fetchd_imm(val)) {
							gen_mov_host((void*)val,DREG(EA),4);
							if (!addseg) {
								gen_lea(reg_ea,DREG(EA),scaled,scale,0);
							} else {
								DynReg** seg = decode.segprefix ? &decode.segprefix : &segbase;
								gen_lea(DREG(EA),DREG(EA),scaled,scale,0);
								gen_lea(reg_ea,DREG(EA),*seg,0,0);
							}
							return;
						}
						imm=(Bit32s)val;
					}
					break;
				case 6:base=DREG(ESI);segbase=DREG(DS);break;
				case 7:base=DREG(EDI);segbase=DREG(DS);break;
				}
			}	
			break;	/* SIB Break */
		case 5:
			if (decode.modrm.mod) {
				base=DREG(EBP);segbase=DREG(SS);
			} else {
				imm=(Bit32s)decode_fetchd();segbase=DREG(DS);
			}
			break;
		case 6:base=DREG(ESI);segbase=DREG(DS);break;
		case 7:base=DREG(EDI);segbase=DREG(DS);break;
		}
		switch (decode.modrm.mod) {
		case 1:imm=(Bit8s)decode_fetchb();break;
		case 2: {
			Bitu val;
			if (decode_fetchd_imm(val)) {
				gen_mov_host((void*)val,DREG(EA),4);
				if (!addseg) {
					gen_lea(DREG(EA),DREG(EA),scaled,scale,0);
					gen_lea(reg_ea,DREG(EA),base,0,0);
				} else {
					DynReg** seg = decode.segprefix ? &decode.segprefix : &segbase;
					if (!base) {
						gen_lea(DREG(EA),DREG(EA),scaled,scale,0);
						gen_lea(reg_ea,DREG(EA),*seg,0,0);
					} else if (!scaled) {
						gen_lea(DREG(EA),DREG(EA),*seg,0,0);
						gen_lea(reg_ea,DREG(EA),base,0,0);
					} else {
						gen_lea(DREG(EA),DREG(EA),scaled,scale,0);
						gen_lea(DREG(EA),DREG(EA),base,0,0);
						gen_lea(reg_ea,DREG(EA),decode.segprefix ? decode.segprefix : segbase,0,0);
					}
				}
				return;
			}
			
			imm=(Bit32s)val;
			break;
			}
		}
		if (!addseg) {
			gen_lea(reg_ea,base,scaled,scale,imm);
		} else {
			DynReg** seg = decode.segprefix ? &decode.segprefix : &segbase;
			if (!base) gen_lea(reg_ea,*seg,scaled,scale,imm);
			else if (!scaled) gen_lea(reg_ea,base,*seg,0,imm);
			else {
				gen_lea(DREG(EA),base,scaled,scale,imm);
				gen_lea(reg_ea,DREG(EA),decode.segprefix ? decode.segprefix : segbase,0,0);
			}
		}
	}
}


static void dyn_dop_word_imm(DualOps op,bool dword,DynReg * dr1) {
	Bitu val;
	if (dword) {
		if (decode_fetchd_imm(val)) {
			gen_dop_word_imm_mem(op,true,dr1,(void*)val);
			return;
		}
	} else {
		if (decode_fetchw_imm(val)) {
			gen_dop_word_imm_mem(op,false,dr1,(void*)val);
			return;
		}
	}
	gen_dop_word_imm(op,dword,dr1,val);
}

static void dyn_dop_byte_imm(DualOps op,DynReg * dr1,Bit8u di1) {
	Bitu val;
	if (decode_fetchb_imm(val)) {
		gen_dop_byte_imm_mem(op,dr1,di1,(void*)val);
	} else {
		gen_dop_byte_imm(op,dr1,di1,(Bit8u)val);
	}
}


#include "helpers.h"
#include "string.h"


static void dyn_dop_ebgb(DualOps op) {
	dyn_get_modrm();DynReg * rm_reg=&DynRegs[decode.modrm.reg&3];
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		if ((op<=DOP_TEST) && (op!=DOP_ADC && op!=DOP_SBB)) set_skipflags(true);
		dyn_read_byte(DREG(EA),DREG(TMPB),false);
		if (op<=DOP_TEST) {
			if (op==DOP_ADC || op==DOP_SBB) gen_needcarry();
			else set_skipflags(false);
		}
		gen_dop_byte(op,DREG(TMPB),0,rm_reg,decode.modrm.reg&4);
		if (op!=DOP_CMP) dyn_write_byte_release(DREG(EA),DREG(TMPB),false);
		else gen_releasereg(DREG(EA));
		gen_releasereg(DREG(TMPB));
	} else {
		if (op<=DOP_TEST) {
			if (op==DOP_ADC || op==DOP_SBB) gen_needcarry();
			else gen_discardflags();
		}
		gen_dop_byte(op,&DynRegs[decode.modrm.rm&3],decode.modrm.rm&4,rm_reg,decode.modrm.reg&4);
	}
}


static void dyn_dop_gbeb(DualOps op) {
	dyn_get_modrm();DynReg * rm_reg=&DynRegs[decode.modrm.reg&3];
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		if ((op<=DOP_TEST) && (op!=DOP_ADC && op!=DOP_SBB)) set_skipflags(true);
		dyn_read_byte_release(DREG(EA),DREG(TMPB),false);
		if (op<=DOP_TEST) {
			if (op==DOP_ADC || op==DOP_SBB) gen_needcarry();
			else set_skipflags(false);
		}
		gen_dop_byte(op,rm_reg,decode.modrm.reg&4,DREG(TMPB),0);
		gen_releasereg(DREG(TMPB));
	} else {
		if (op<=DOP_TEST) {
			if (op==DOP_ADC || op==DOP_SBB) gen_needcarry();
			else gen_discardflags();
		}
		gen_dop_byte(op,rm_reg,decode.modrm.reg&4,&DynRegs[decode.modrm.rm&3],decode.modrm.rm&4);
	}
}

static void dyn_mov_ebib(void) {
	dyn_get_modrm();
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		gen_call_write(DREG(EA),decode_fetchb(),1);
		dyn_check_bool_exception_al();
	} else {
		gen_dop_byte_imm(DOP_MOV,&DynRegs[decode.modrm.rm&3],decode.modrm.rm&4,decode_fetchb());
	}
}

static void dyn_mov_ebgb(void) {
	dyn_get_modrm();
	DynReg * rm_reg=&DynRegs[decode.modrm.reg&3];Bitu rm_regi=decode.modrm.reg&4;
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		dyn_write_byte_release(DREG(EA),rm_reg,rm_regi==4);
	} else {
		gen_dop_byte(DOP_MOV,&DynRegs[decode.modrm.rm&3],decode.modrm.rm&4,rm_reg,rm_regi);
	}
}

static void dyn_mov_gbeb(void) {
	dyn_get_modrm();
	DynReg * rm_reg=&DynRegs[decode.modrm.reg&3];Bitu rm_regi=decode.modrm.reg&4;
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		dyn_read_byte_release(DREG(EA),rm_reg,rm_regi);
	} else {
		gen_dop_byte(DOP_MOV,rm_reg,rm_regi,&DynRegs[decode.modrm.rm&3],decode.modrm.rm&4);
	}
}

static void dyn_dop_evgv(DualOps op) {
	dyn_get_modrm();
	DynReg * rm_reg=&DynRegs[decode.modrm.reg];
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		if ((op<=DOP_TEST) && (op!=DOP_ADC && op!=DOP_SBB)) set_skipflags(true);
		dyn_read_word(DREG(EA),DREG(TMPW),decode.big_op);
		if (op<=DOP_TEST) {
			if (op==DOP_ADC || op==DOP_SBB) gen_needcarry();
			else set_skipflags(false);
		}
		gen_dop_word(op,decode.big_op,DREG(TMPW),rm_reg);
		if (op!=DOP_CMP) dyn_write_word_release(DREG(EA),DREG(TMPW),decode.big_op);
		else gen_releasereg(DREG(EA));
		gen_releasereg(DREG(TMPW));
	} else {
		if (op<=DOP_TEST) {
			if (op==DOP_ADC || op==DOP_SBB) gen_needcarry();
			else gen_discardflags();
		}
		gen_dop_word(op,decode.big_op,&DynRegs[decode.modrm.rm],rm_reg);
	}
}

static void dyn_imul_gvev(Bitu immsize) {
	dyn_get_modrm();DynReg * src;
	DynReg * rm_reg=&DynRegs[decode.modrm.reg];
	if (decode.modrm.mod<3) {
		dyn_fill_ea();dyn_read_word_release(DREG(EA),DREG(TMPW),decode.big_op);
		src=DREG(TMPW);
	} else {
		src=&DynRegs[decode.modrm.rm];
	}
	gen_needflags();
	switch (immsize) {
	case 0:gen_imul_word(decode.big_op,rm_reg,src);break;
	case 1:gen_imul_word_imm(decode.big_op,rm_reg,src,(Bit8s)decode_fetchb());break;
	case 2:gen_imul_word_imm(decode.big_op,rm_reg,src,(Bit16s)decode_fetchw());break;
	case 4:gen_imul_word_imm(decode.big_op,rm_reg,src,(Bit32s)decode_fetchd());break;
	}
	gen_releasereg(DREG(TMPW));
}

static void dyn_dop_gvev(DualOps op) {
	dyn_get_modrm();
	DynReg * rm_reg=&DynRegs[decode.modrm.reg];
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		if ((op<=DOP_TEST) && (op!=DOP_ADC && op!=DOP_SBB)) set_skipflags(true);
		dyn_read_word_release(DREG(EA),DREG(TMPW),decode.big_op);
		if (op<=DOP_TEST) {
			if (op==DOP_ADC || op==DOP_SBB) gen_needcarry();
			else set_skipflags(false);
		}
		gen_dop_word(op,decode.big_op,rm_reg,DREG(TMPW));
		gen_releasereg(DREG(TMPW));
	} else {
		if (op<=DOP_TEST) {
			if (op==DOP_ADC || op==DOP_SBB) gen_needcarry();
			else gen_discardflags();
		}
		gen_dop_word(op,decode.big_op,rm_reg,&DynRegs[decode.modrm.rm]);
	}
}

static void dyn_mov_evgv(void) {
	dyn_get_modrm();
	DynReg * rm_reg=&DynRegs[decode.modrm.reg];
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		dyn_write_word_release(DREG(EA),rm_reg,decode.big_op);
	} else {
		gen_dop_word(DOP_MOV,decode.big_op,&DynRegs[decode.modrm.rm],rm_reg);
	}
}

static void dyn_mov_gvev(void) {
	dyn_get_modrm();
	DynReg * rm_reg=&DynRegs[decode.modrm.reg];
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		dyn_read_word_release(DREG(EA),rm_reg,decode.big_op);
	} else {
		gen_dop_word(DOP_MOV,decode.big_op,rm_reg,&DynRegs[decode.modrm.rm]);
	}
}
static void dyn_mov_eviv(void) {
	dyn_get_modrm();
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		gen_call_write(DREG(EA),decode.big_op ? decode_fetchd() : decode_fetchw(),decode.big_op?4:2);
		dyn_check_bool_exception_al();
	} else {
		gen_dop_word_imm(DOP_MOV,decode.big_op,&DynRegs[decode.modrm.rm],decode.big_op ? decode_fetchd() : decode_fetchw());
	}
}

static void dyn_mov_ev_gb(bool sign) {
	dyn_get_modrm();DynReg * rm_reg=&DynRegs[decode.modrm.reg];
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		dyn_read_byte_release(DREG(EA),DREG(TMPB),false);
		gen_extend_byte(sign,decode.big_op,rm_reg,DREG(TMPB),0);
		gen_releasereg(DREG(TMPB));
	} else {
		gen_extend_byte(sign,decode.big_op,rm_reg,&DynRegs[decode.modrm.rm&3],decode.modrm.rm&4);
	}
}

static void dyn_mov_ev_gw(bool sign) {
	if (!decode.big_op) {
		dyn_mov_gvev();
		return;
	}
	dyn_get_modrm();DynReg * rm_reg=&DynRegs[decode.modrm.reg];
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		dyn_read_word_release(DREG(EA),DREG(TMPW),false);
		gen_extend_word(sign,rm_reg,DREG(TMPW));
		gen_releasereg(DREG(TMPW));
	} else {
		gen_extend_word(sign,rm_reg,&DynRegs[decode.modrm.rm]);
	}
}

static void dyn_cmpxchg_evgv(void) {
	dyn_get_modrm();
	DynReg * rm_reg=&DynRegs[decode.modrm.reg];
	gen_protectflags();
	if (decode.modrm.mod<3) {
		gen_releasereg(DREG(EAX));
		gen_releasereg(DREG(TMPB));
		gen_releasereg(rm_reg);

		dyn_fill_ea();
		dyn_read_word(DREG(EA),DREG(TMPB),decode.big_op);
		gen_dop_word(DOP_CMP,decode.big_op,DREG(EAX),DREG(TMPB));
		Bit8u * branch=gen_create_branch(BR_NZ);

		// eax==mem -> mem:=rm_reg
		dyn_write_word_release(DREG(EA),rm_reg,decode.big_op);
		gen_setzeroflag();
		gen_releasereg(DREG(EAX));
		gen_releasereg(DREG(TMPB));
		gen_releasereg(rm_reg);

		Bit8u * jump=gen_create_jump();

		gen_fill_branch(branch);
		// eax!=mem -> eax:=mem
		dyn_write_word_release(DREG(EA),DREG(TMPB),decode.big_op);	// cmpxchg always issues write
		gen_dop_word(DOP_MOV,decode.big_op,DREG(EAX),DREG(TMPB));
		gen_clearzeroflag();
		gen_releasereg(DREG(EAX));
		gen_releasereg(DREG(TMPB));
		gen_releasereg(rm_reg);

		gen_fill_jump(jump);
	} else {
		gen_releasereg(DREG(EAX));
		gen_releasereg(&DynRegs[decode.modrm.rm]);
		gen_releasereg(rm_reg);

		gen_dop_word(DOP_CMP,decode.big_op,DREG(EAX),&DynRegs[decode.modrm.rm]);
		Bit8u * branch=gen_create_branch(BR_NZ);

		// eax==rm -> rm:=rm_reg
		gen_dop_word(DOP_MOV,decode.big_op,&DynRegs[decode.modrm.rm],rm_reg);
		gen_setzeroflag();
		gen_releasereg(DREG(EAX));
		gen_releasereg(&DynRegs[decode.modrm.rm]);
		gen_releasereg(rm_reg);

		Bit8u * jump=gen_create_jump();

		gen_fill_branch(branch);
		// eax!=rm -> eax:=rm
		gen_dop_word(DOP_MOV,decode.big_op,DREG(EAX),&DynRegs[decode.modrm.rm]);
		gen_clearzeroflag();
		gen_releasereg(DREG(EAX));
		gen_releasereg(&DynRegs[decode.modrm.rm]);
		gen_releasereg(rm_reg);

		gen_fill_jump(jump);
	}
}

static void dyn_dshift_ev_gv(bool left,bool immediate) {
	dyn_get_modrm();
	DynReg * rm_reg=&DynRegs[decode.modrm.reg];
	DynReg * ea_reg;
	if (decode.modrm.mod<3) {
		dyn_fill_ea();ea_reg=DREG(TMPW);
		dyn_read_word(DREG(EA),DREG(TMPW),decode.big_op);
	} else ea_reg=&DynRegs[decode.modrm.rm];
	gen_needflags();
	if (immediate) gen_dshift_imm(decode.big_op,left,ea_reg,rm_reg,decode_fetchb());
	else gen_dshift_cl(decode.big_op,left,ea_reg,rm_reg,DREG(ECX));
	if (decode.modrm.mod<3) {
		dyn_write_word_release(DREG(EA),DREG(TMPW),decode.big_op);
		gen_releasereg(DREG(TMPW));
	}
}


static DualOps grp1_table[8]={DOP_ADD,DOP_OR,DOP_ADC,DOP_SBB,DOP_AND,DOP_SUB,DOP_XOR,DOP_CMP};
static void dyn_grp1_eb_ib(void) {
	dyn_get_modrm();
	DualOps op=grp1_table[decode.modrm.reg];
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		if ((op<=DOP_TEST) && (op!=DOP_ADC && op!=DOP_SBB)) set_skipflags(true);
		dyn_read_byte(DREG(EA),DREG(TMPB),false);
		if (op<=DOP_TEST) {
			if (op==DOP_ADC || op==DOP_SBB) gen_needcarry();
			else set_skipflags(false);
		}
		gen_dop_byte_imm(op,DREG(TMPB),0,decode_fetchb());
		if (op!=DOP_CMP) dyn_write_byte_release(DREG(EA),DREG(TMPB),false);
		else gen_releasereg(DREG(EA));
		gen_releasereg(DREG(TMPB));
	} else {
		if (op<=DOP_TEST) {
			if (op==DOP_ADC || op==DOP_SBB) gen_needcarry();
			else gen_discardflags();
		}
		dyn_dop_byte_imm(op,&DynRegs[decode.modrm.rm&3],decode.modrm.rm&4);
	}
}

static void dyn_grp1_ev_ivx(bool withbyte) {
	dyn_get_modrm();
	DualOps op=grp1_table[decode.modrm.reg];
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		if ((op<=DOP_TEST) && (op!=DOP_ADC && op!=DOP_SBB)) set_skipflags(true);
		dyn_read_word(DREG(EA),DREG(TMPW),decode.big_op);
		if (op<=DOP_TEST) {
			if (op==DOP_ADC || op==DOP_SBB) gen_needcarry();
			else set_skipflags(false);
		}
		if (!withbyte) {
			dyn_dop_word_imm(op,decode.big_op,DREG(TMPW));
		} else {
			gen_dop_word_imm(op,decode.big_op,DREG(TMPW),(Bit8s)decode_fetchb());
		}
		if (op!=DOP_CMP) dyn_write_word_release(DREG(EA),DREG(TMPW),decode.big_op);
		else gen_releasereg(DREG(EA));
		gen_releasereg(DREG(TMPW));
	} else {
		if (op<=DOP_TEST) {
			if (op==DOP_ADC || op==DOP_SBB) gen_needcarry();
			else gen_discardflags();
		}
		if (!withbyte) {
			dyn_dop_word_imm(op,decode.big_op,&DynRegs[decode.modrm.rm]);
		} else {
			gen_dop_word_imm(op,decode.big_op,&DynRegs[decode.modrm.rm],(Bit8s)decode_fetchb());
		}
	}
}

enum grp2_types {
	grp2_1,grp2_imm,grp2_cl,
};

static void dyn_grp2_eb(grp2_types type) {
	dyn_get_modrm();DynReg * src;Bit8u src_i;
	if (decode.modrm.mod<3) {
		dyn_fill_ea();dyn_read_byte(DREG(EA),DREG(TMPB),false);
		src=DREG(TMPB);
		src_i=0;
	} else {
		src=&DynRegs[decode.modrm.rm&3];
		src_i=decode.modrm.rm&4;
	}
	switch (type) {
	case grp2_1:
		/* rotates (first 4 ops) alter cf/of only; shifts (last 4 ops) alter all flags */
		if (decode.modrm.reg < 4) gen_needflags();
		else gen_discardflags();
		gen_shift_byte_imm(decode.modrm.reg,src,src_i,1);
		break;
	case grp2_imm: {
		Bit8u imm=decode_fetchb();
		if (imm) {
			/* rotates (first 4 ops) alter cf/of only; shifts (last 4 ops) alter all flags */
			if (decode.modrm.reg < 4) gen_needflags();
			else gen_discardflags();
			gen_shift_byte_imm(decode.modrm.reg,src,src_i,imm);
		} else return;
		}
		break;
	case grp2_cl:
		gen_needflags();	/* flags must not be changed on ecx==0 */
		gen_shift_byte_cl (decode.modrm.reg,src,src_i,DREG(ECX));
		break;
	}
	if (decode.modrm.mod<3) {
		dyn_write_byte_release(DREG(EA),src,false);
		gen_releasereg(src);
	}
}

static void dyn_grp2_ev(grp2_types type) {
	dyn_get_modrm();DynReg * src;
	if (decode.modrm.mod<3) {
		dyn_fill_ea();dyn_read_word(DREG(EA),DREG(TMPW),decode.big_op);
		src=DREG(TMPW);
	} else {
		src=&DynRegs[decode.modrm.rm];
	}
	switch (type) {
	case grp2_1:
		/* rotates (first 4 ops) alter cf/of only; shifts (last 4 ops) alter all flags */
		if (decode.modrm.reg < 4) gen_needflags();
		else gen_discardflags();
		gen_shift_word_imm(decode.modrm.reg,decode.big_op,src,1);
		break;
	case grp2_imm: {
		Bitu val;
		if (decode_fetchb_imm(val)) {
			if (decode.modrm.reg < 4) gen_needflags();
			else gen_discardflags();
			gen_load_host((void*)val,DREG(TMPB),1);
			gen_shift_word_cl(decode.modrm.reg,decode.big_op,src,DREG(TMPB));
			gen_releasereg(DREG(TMPB));
			break;
		}
		Bit8u imm=(Bit8u)val;
		if (imm) {
			/* rotates (first 4 ops) alter cf/of only; shifts (last 4 ops) alter all flags */
			if (decode.modrm.reg < 4) gen_needflags();
			else gen_discardflags();
			gen_shift_word_imm(decode.modrm.reg,decode.big_op,src,imm);
		} else return;
		}
		break;
	case grp2_cl:
		gen_needflags();	/* flags must not be changed on ecx==0 */
		gen_shift_word_cl (decode.modrm.reg,decode.big_op,src,DREG(ECX));
		break;
	}
	if (decode.modrm.mod<3) {
		dyn_write_word_release(DREG(EA),src,decode.big_op);
		gen_releasereg(src);
	}
}

static void dyn_grp3_eb(void) {
	dyn_get_modrm();DynReg * src;Bit8u src_i;
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		if ((decode.modrm.reg==0) || (decode.modrm.reg==3)) set_skipflags(true);
		dyn_read_byte(DREG(EA),DREG(TMPB),false);
		src=DREG(TMPB);src_i=0;
	} else {
		src=&DynRegs[decode.modrm.rm&3];
		src_i=decode.modrm.rm&4;
	}
	switch (decode.modrm.reg) {
	case 0x0:	/* test eb,ib */
		set_skipflags(false);gen_dop_byte_imm(DOP_TEST,src,src_i,decode_fetchb());
		goto skipsave;
	case 0x2:	/* NOT Eb */
		gen_sop_byte(SOP_NOT,src,src_i);
		break;
	case 0x3:	/* NEG Eb */
		set_skipflags(false);gen_sop_byte(SOP_NEG,src,src_i);
		break;
	case 0x4:	/* mul Eb */
		gen_needflags();gen_mul_byte(false,DREG(EAX),src,src_i);
		goto skipsave;
	case 0x5:	/* imul Eb */
		gen_needflags();gen_mul_byte(true,DREG(EAX),src,src_i);
		goto skipsave;
	case 0x6:	/* div Eb */
	case 0x7:	/* idiv Eb */
		/* EAX could be used, so precache it */
		if (decode.modrm.mod==3)
			gen_dop_byte(DOP_MOV,DREG(TMPB),0,&DynRegs[decode.modrm.rm&3],decode.modrm.rm&4);
		gen_releasereg(DREG(EAX));
		gen_call_function((decode.modrm.reg==6) ? (void *)&dyn_helper_divb : (void *)&dyn_helper_idivb,
			"%Rd%Dd",DREG(TMPB),DREG(TMPB));
		dyn_check_bool_exception(DREG(TMPB));
		goto skipsave;
	}
	/* Save the result if memory op */
	if (decode.modrm.mod<3) dyn_write_byte_release(DREG(EA),src,false);
skipsave:
	gen_releasereg(DREG(TMPB));gen_releasereg(DREG(EA));
}

static void dyn_grp3_ev(void) {
	dyn_get_modrm();DynReg * src;
	if (decode.modrm.mod<3) {
		dyn_fill_ea();src=DREG(TMPW);
		if ((decode.modrm.reg==0) || (decode.modrm.reg==3)) set_skipflags(true);
		dyn_read_word(DREG(EA),DREG(TMPW),decode.big_op);
	} else src=&DynRegs[decode.modrm.rm];
	switch (decode.modrm.reg) {
	case 0x0:	/* test ev,iv */
		set_skipflags(false);gen_dop_word_imm(DOP_TEST,decode.big_op,src,decode.big_op ? decode_fetchd() : decode_fetchw());
		goto skipsave;
	case 0x2:	/* NOT Ev */
		gen_sop_word(SOP_NOT,decode.big_op,src);
		break;
	case 0x3:	/* NEG Eb */
		set_skipflags(false);gen_sop_word(SOP_NEG,decode.big_op,src);
		break;
	case 0x4:	/* mul Eb */
		gen_needflags();gen_mul_word(false,DREG(EAX),DREG(EDX),decode.big_op,src);
		goto skipsave;
	case 0x5:	/* imul Eb */
		gen_needflags();gen_mul_word(true,DREG(EAX),DREG(EDX),decode.big_op,src);
		goto skipsave;
	case 0x6:	/* div Eb */
	case 0x7:	/* idiv Eb */
		/* EAX could be used, so precache it */
		if (decode.modrm.mod==3)
			gen_dop_word(DOP_MOV,decode.big_op,DREG(TMPW),&DynRegs[decode.modrm.rm]);
		gen_releasereg(DREG(EAX));gen_releasereg(DREG(EDX));
		void * func=(decode.modrm.reg==6) ?
			(decode.big_op ? (void *)&dyn_helper_divd : (void *)&dyn_helper_divw) :
			(decode.big_op ? (void *)&dyn_helper_idivd : (void *)&dyn_helper_idivw);
		gen_call_function(func,"%Rd%Dd",DREG(TMPB),DREG(TMPW));
		dyn_check_bool_exception(DREG(TMPB));
		gen_releasereg(DREG(TMPB));
		goto skipsave;
	}
	/* Save the result if memory op */
	if (decode.modrm.mod<3) dyn_write_word_release(DREG(EA),src,decode.big_op);
skipsave:
	gen_releasereg(DREG(TMPW));gen_releasereg(DREG(EA));
}

static void dyn_mov_ev_seg(void) {
	dyn_get_modrm();
	gen_load_host(&Segs.val[decode.modrm.reg],DREG(TMPW),2);
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		dyn_write_word_release(DREG(EA),DREG(TMPW),false);
	} else {
		gen_dop_word(DOP_MOV,decode.big_op,&DynRegs[decode.modrm.rm],DREG(TMPW));
	}
	gen_releasereg(DREG(TMPW));
}

static void dyn_load_seg(SegNames seg,DynReg * src) {
	gen_call_function((void *)&CPU_SetSegGeneral,"%Rd%Id%Drw",DREG(TMPB),seg,src);
	dyn_check_bool_exception(DREG(TMPB));
	gen_releasereg(DREG(TMPB));
	gen_releasereg(&DynRegs[G_ES+seg]);
}

static void dyn_load_seg_off_ea(SegNames seg) {
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		gen_lea(DREG(TMPB),DREG(EA),0,0,decode.big_op ? 4:2);
		dyn_read_word(DREG(TMPB),DREG(TMPB),false);
		dyn_read_word_release(DREG(EA),DREG(TMPW),decode.big_op);
		dyn_load_seg(seg,DREG(TMPB));gen_releasereg(DREG(TMPB));
		gen_dop_word(DOP_MOV,decode.big_op,&DynRegs[decode.modrm.reg],DREG(TMPW));
	} else {
		IllegalOption("dyn_load_seg_off_ea");
	}
}

static void dyn_mov_seg_ev(void) {
	dyn_get_modrm();
	SegNames seg=(SegNames)decode.modrm.reg;
	if (GCC_UNLIKELY(seg==cs)) IllegalOption("dyn_mov_seg_ev");
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		dyn_read_word(DREG(EA),DREG(EA),false);
		dyn_load_seg(seg,DREG(EA));
		gen_releasereg(DREG(EA));
	} else {
		dyn_load_seg(seg,&DynRegs[decode.modrm.rm]);
	}
}

static void dyn_push_seg(SegNames seg) {
	gen_load_host(&Segs.val[seg],DREG(TMPW),2);
	dyn_push(DREG(TMPW));
	gen_releasereg(DREG(TMPW));
}

static void dyn_pop_seg(SegNames seg) {
	gen_releasereg(DREG(ESP));
	gen_call_function((void *)&CPU_PopSeg,"%Rd%Id%Id",DREG(TMPB),seg,decode.big_op);
	dyn_check_bool_exception(DREG(TMPB));
	gen_releasereg(DREG(TMPB));
	gen_releasereg(&DynRegs[G_ES+seg]);
	gen_releasereg(DREG(ESP));
}

static void dyn_pop_ev(void) {
	dyn_pop(DREG(TMPW));
	dyn_get_modrm();
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
//		dyn_write_word_release(DREG(EA),DREG(TMPW),decode.big_op);
		if (decode.big_op) gen_call_function((void *)&mem_writed_inline,"%Ddr%Dd",DREG(EA),DREG(TMPW));
		else gen_call_function((void *)&mem_writew_inline,"%Ddr%Dd",DREG(EA),DREG(TMPW));
	} else {
		gen_dop_word(DOP_MOV,decode.big_op,&DynRegs[decode.modrm.rm],DREG(TMPW));
	}
	gen_releasereg(DREG(TMPW));
}

static void dyn_enter(void) {
	gen_releasereg(DREG(ESP));
	gen_releasereg(DREG(EBP));
	Bitu bytes=decode_fetchw();
	Bitu level=decode_fetchb();
	gen_call_function((void *)&CPU_ENTER,"%Id%Id%Id",decode.big_op,bytes,level);
}

static void dyn_leave(void) {
	gen_protectflags();
	gen_dop_word_var(DOP_MOV,true,DREG(TMPW),&cpu.stack.mask);
	gen_sop_word(SOP_NOT,true,DREG(TMPW));
	gen_dop_word(DOP_AND,true,DREG(ESP),DREG(TMPW));
	gen_dop_word(DOP_MOV,true,DREG(TMPW),DREG(EBP));
	gen_dop_word_var(DOP_AND,true,DREG(TMPW),&cpu.stack.mask);
	gen_dop_word(DOP_OR,true,DREG(ESP),DREG(TMPW));
	dyn_pop(DREG(EBP),false);
	gen_releasereg(DREG(TMPW));
}

static void dyn_segprefix(SegNames seg) {
//	if (GCC_UNLIKELY((Bitu)(decode.segprefix))) IllegalOption("dyn_segprefix");
	decode.segprefix=&DynRegs[G_ES+seg];
}

static void dyn_closeblock(void) {
	//Shouldn't create empty block normally but let's do it like this
	gen_protectflags();
	dyn_fill_blocks();
	cache_closeblock();
}

static void dyn_normal_exit(BlockReturn code) {
	gen_protectflags();
	dyn_reduce_cycles();
	dyn_set_eip_last();
	dyn_save_critical_regs();
	gen_return(code);
	dyn_closeblock();
}

static void dyn_exit_link(Bits eip_change) {
	gen_protectflags();
	gen_dop_word_imm(DOP_ADD,decode.big_op,DREG(EIP),(decode.code-decode.code_start)+eip_change);
	dyn_reduce_cycles();
	dyn_save_critical_regs();
	gen_jmp_ptr(&decode.block->link[0].to,offsetof(CacheBlock,cache.start));
	dyn_closeblock();
}

static void dyn_branched_exit(BranchTypes btype,Bit32s eip_add) {
	Bitu eip_base=decode.code-decode.code_start;
 	gen_needflags();
 	gen_protectflags();
	dyn_save_noncritical_regs();
	gen_releasereg(DREG(FLAGS));
	gen_releasereg(DREG(EIP));

	gen_preloadreg(DREG(CYCLES));
	gen_preloadreg(DREG(EIP));
	DynReg save_cycles,save_eip;
	dyn_saveregister(DREG(CYCLES),&save_cycles);
	dyn_saveregister(DREG(EIP),&save_eip);
	Bit8u * data=gen_create_branch(btype);

 	/* Branch not taken */
	dyn_reduce_cycles();
 	gen_dop_word_imm(DOP_ADD,decode.big_op,DREG(EIP),eip_base);
	gen_releasereg(DREG(CYCLES));
 	gen_releasereg(DREG(EIP));
 	gen_jmp_ptr(&decode.block->link[0].to,offsetof(CacheBlock,cache.start));
 	gen_fill_branch(data);

 	/* Branch taken */
	dyn_restoreregister(&save_cycles,DREG(CYCLES));
	dyn_restoreregister(&save_eip,DREG(EIP));
	dyn_reduce_cycles();
 	gen_dop_word_imm(DOP_ADD,decode.big_op,DREG(EIP),eip_base+eip_add);
	gen_releasereg(DREG(CYCLES));
 	gen_releasereg(DREG(EIP));
 	gen_jmp_ptr(&decode.block->link[1].to,offsetof(CacheBlock,cache.start));
 	dyn_closeblock();
}

enum LoopTypes {
	LOOP_NONE,LOOP_NE,LOOP_E,LOOP_JCXZ
};

static void dyn_loop(LoopTypes type) {
	dyn_reduce_cycles();
	Bits eip_add=(Bit8s)decode_fetchb();
	Bitu eip_base=decode.code-decode.code_start;
	Bit8u * branch1=0;Bit8u * branch2=0;
	dyn_save_critical_regs();
	switch (type) {
	case LOOP_E:
		gen_needflags();
		branch1=gen_create_branch(BR_NZ);
		break;
	case LOOP_NE:
		gen_needflags();
		branch1=gen_create_branch(BR_Z);
		break;
	}
	gen_protectflags();
	switch (type) {
	case LOOP_E:
	case LOOP_NE:
	case LOOP_NONE:
		gen_sop_word(SOP_DEC,decode.big_addr,DREG(ECX));
		gen_releasereg(DREG(ECX));
		branch2=gen_create_branch(BR_Z);
		break;
	case LOOP_JCXZ:
		gen_dop_word(DOP_OR,decode.big_addr,DREG(ECX),DREG(ECX));
		gen_releasereg(DREG(ECX));
		branch2=gen_create_branch(BR_NZ);
		break;
	}
	gen_lea(DREG(EIP),DREG(EIP),0,0,eip_base+eip_add);
	gen_releasereg(DREG(EIP));
	gen_jmp_ptr(&decode.block->link[0].to,offsetof(CacheBlock,cache.start));
	if (branch1) {
		gen_fill_branch(branch1);
		gen_sop_word(SOP_DEC,decode.big_addr,DREG(ECX));
		gen_releasereg(DREG(ECX));
	}
	/* Branch taken */
	gen_fill_branch(branch2);
	gen_lea(DREG(EIP),DREG(EIP),0,0,eip_base);
	gen_releasereg(DREG(EIP));
	gen_jmp_ptr(&decode.block->link[1].to,offsetof(CacheBlock,cache.start));
	dyn_closeblock();
}

static void dyn_ret_near(Bitu bytes) {
	gen_protectflags();
	dyn_reduce_cycles();
	dyn_pop(DREG(EIP));
	if (bytes) gen_dop_word_imm(DOP_ADD,true,DREG(ESP),bytes);
	dyn_save_critical_regs();
	gen_return(BR_Normal);
	dyn_closeblock();
}

static void dyn_call_near_imm(void) {
	Bits imm;
	if (decode.big_op) imm=(Bit32s)decode_fetchd();
	else imm=(Bit16s)decode_fetchw();
	dyn_set_eip_end(DREG(TMPW));
	dyn_push(DREG(TMPW));
	gen_dop_word_imm(DOP_ADD,decode.big_op,DREG(TMPW),imm);
	if (cpu.code.big) gen_dop_word(DOP_MOV,true,DREG(EIP),DREG(TMPW));
	else gen_extend_word(false,DREG(EIP),DREG(TMPW));
	dyn_reduce_cycles();
	dyn_save_critical_regs();
	gen_jmp_ptr(&decode.block->link[0].to,offsetof(CacheBlock,cache.start));
	dyn_closeblock();
}

static void dyn_ret_far(Bitu bytes) {
	gen_protectflags();
	dyn_reduce_cycles();
	dyn_set_eip_last_end(DREG(TMPW));
	dyn_flags_gen_to_host();
	dyn_save_critical_regs();
	gen_call_function((void*)&CPU_RET,"%Id%Id%Drd",decode.big_op,bytes,DREG(TMPW));
	gen_return_fast(BR_Normal);
	dyn_closeblock();
}

static void dyn_call_far_imm(void) {
	Bitu sel,off;
	off=decode.big_op ? decode_fetchd() : decode_fetchw();
	sel=decode_fetchw();
	dyn_reduce_cycles();
	dyn_set_eip_last_end(DREG(TMPW));
	dyn_flags_gen_to_host();
	dyn_save_critical_regs();
	gen_call_function((void*)&CPU_CALL,"%Id%Id%Id%Drd",decode.big_op,sel,off,DREG(TMPW));
	gen_return_fast(BR_Normal);
	dyn_closeblock();
}

static void dyn_jmp_far_imm(void) {
	Bitu sel,off;
	gen_protectflags();
	off=decode.big_op ? decode_fetchd() : decode_fetchw();
	sel=decode_fetchw();
	dyn_reduce_cycles();
	dyn_set_eip_last_end(DREG(TMPW));
	dyn_flags_gen_to_host();
	dyn_save_critical_regs();
	gen_call_function((void*)&CPU_JMP,"%Id%Id%Id%Drd",decode.big_op,sel,off,DREG(TMPW));
	gen_return_fast(BR_Normal);
	dyn_closeblock();
}

static void dyn_iret(void) {
	gen_protectflags();
	dyn_flags_gen_to_host();
	dyn_reduce_cycles();
	dyn_set_eip_last_end(DREG(TMPW));
	dyn_save_critical_regs();
	gen_call_function((void*)&CPU_IRET,"%Id%Drd",decode.big_op,DREG(TMPW));
	gen_return_fast(BR_Iret);
	dyn_closeblock();
}

static void dyn_interrupt(Bitu num) {
	gen_protectflags();
	dyn_flags_gen_to_host();
	dyn_reduce_cycles();
	dyn_set_eip_last_end(DREG(TMPW));
	dyn_save_critical_regs();
	gen_call_function((void*)&CPU_Interrupt,"%Id%Id%Drd",num,CPU_INT_SOFTWARE,DREG(TMPW));
	gen_return_fast(BR_Normal);
	dyn_closeblock();
}

static void dyn_add_iocheck(Bitu access_size) {
	gen_call_function((void *)&CPU_IO_Exception,"%Dw%Id",DREG(EDX),access_size);
	dyn_check_bool_exception_al();
}

static void dyn_add_iocheck_var(Bit8u accessed_port,Bitu access_size) {
	gen_call_function((void *)&CPU_IO_Exception,"%Id%Id",accessed_port,access_size);
	dyn_check_bool_exception_al();
}

#ifdef X86_DYNFPU_DH_ENABLED
#include "dyn_fpu_dh.h"
#define dh_fpu_startup() {		\
	fpu_used=true;				\
	gen_protectflags();			\
	gen_load_host(&dyn_dh_fpu.state_used,DREG(TMPB),4);	\
	gen_dop_word_imm(DOP_CMP,true,DREG(TMPB),0);		\
	gen_releasereg(DREG(TMPB));							\
	save_info[used_save_info].branch_pos=gen_create_branch_long(BR_Z);		\
	dyn_savestate(&save_info[used_save_info].state);	\
	save_info[used_save_info].return_pos=cache.pos;		\
	save_info[used_save_info].type=fpu_restore;			\
	used_save_info++;									\
}
#endif
#include "dyn_fpu.h"

static CacheBlock * CreateCacheBlock(CodePageHandler * codepage,PhysPt start,Bitu max_opcodes) {
	Bits i;
/* Init a load of variables */
	decode.code_start=start;
	decode.code=start;
	Bitu cycles=0;
	decode.page.code=codepage;
	decode.page.index=start&4095;
	decode.page.wmap=codepage->write_map;
	decode.page.invmap=codepage->invalidation_map;
	decode.page.first=start >> 12;
	decode.active_block=decode.block=cache_openblock();
	decode.block->page.start=decode.page.index;
	codepage->AddCacheBlock(decode.block);

	gen_save_host_direct(&cache.block.running,(Bit32u)decode.block);
	for (i=0;i<G_MAX;i++) {
		DynRegs[i].flags&=~(DYNFLG_ACTIVE|DYNFLG_CHANGED);
		DynRegs[i].genreg=0;
	}
	gen_reinit();
	/* Start with the cycles check */
	gen_protectflags();
	gen_dop_word_imm(DOP_CMP,true,DREG(CYCLES),0);
	save_info[used_save_info].branch_pos=gen_create_branch_long(BR_LE);
	save_info[used_save_info].type=cycle_check;
	used_save_info++;
	gen_releasereg(DREG(CYCLES));
	decode.cycles=0;
#ifdef X86_DYNFPU_DH_ENABLED
	bool fpu_used=false;
#endif
	while (max_opcodes--) {
/* Init prefixes */
		decode.big_addr=cpu.code.big;
		decode.big_op=cpu.code.big;
		decode.segprefix=0;
		decode.rep=REP_NONE;
		decode.cycles++;
		decode.op_start=decode.code;
restart_prefix:
		Bitu opcode;
		if (!decode.page.invmap) opcode=decode_fetchb();
		else {
			if (decode.page.index<4096) {
				if (GCC_UNLIKELY(decode.page.invmap[decode.page.index]>=4)) goto illegalopcode;
				opcode=decode_fetchb();
			} else {
				opcode=decode_fetchb();
				if (GCC_UNLIKELY(decode.page.invmap && 
					(decode.page.invmap[decode.page.index-1]>=4))) goto illegalopcode;
			}
		}
		switch (opcode) {

		case 0x00:dyn_dop_ebgb(DOP_ADD);break;
		case 0x01:dyn_dop_evgv(DOP_ADD);break;
		case 0x02:dyn_dop_gbeb(DOP_ADD);break;
		case 0x03:dyn_dop_gvev(DOP_ADD);break;
		case 0x04:gen_discardflags();gen_dop_byte_imm(DOP_ADD,DREG(EAX),0,decode_fetchb());break;
		case 0x05:gen_discardflags();dyn_dop_word_imm(DOP_ADD,decode.big_op,DREG(EAX));break;
		case 0x06:dyn_push_seg(es);break;
		case 0x07:dyn_pop_seg(es);break;

		case 0x08:dyn_dop_ebgb(DOP_OR);break;
		case 0x09:dyn_dop_evgv(DOP_OR);break;
		case 0x0a:dyn_dop_gbeb(DOP_OR);break;
		case 0x0b:dyn_dop_gvev(DOP_OR);break;
		case 0x0c:gen_discardflags();gen_dop_byte_imm(DOP_OR,DREG(EAX),0,decode_fetchb());break;
		case 0x0d:gen_discardflags();gen_dop_word_imm(DOP_OR,decode.big_op,DREG(EAX),decode.big_op ? decode_fetchd() :  decode_fetchw());break;
		case 0x0e:dyn_push_seg(cs);break;
		case 0x0f:
		{
			Bitu dual_code=decode_fetchb();
			switch (dual_code) {
			/* Short conditional jumps */
			case 0x80:case 0x81:case 0x82:case 0x83:case 0x84:case 0x85:case 0x86:case 0x87:	
			case 0x88:case 0x89:case 0x8a:case 0x8b:case 0x8c:case 0x8d:case 0x8e:case 0x8f:	
				dyn_branched_exit((BranchTypes)(dual_code&0xf),
					decode.big_op ? (Bit32s)decode_fetchd() : (Bit16s)decode_fetchw());	
				goto finish_block;
			/* PUSH/POP FS */
			case 0xa0:dyn_push_seg(fs);break;
			case 0xa1:dyn_pop_seg(fs);break;
			/* SHLD Imm/cl*/
			case 0xa4:dyn_dshift_ev_gv(true,true);break;
			case 0xa5:dyn_dshift_ev_gv(true,false);break;
			/* PUSH/POP GS */
			case 0xa8:dyn_push_seg(gs);break;
			case 0xa9:dyn_pop_seg(gs);break;
			/* SHRD Imm/cl*/
			case 0xac:dyn_dshift_ev_gv(false,true);break;
			case 0xad:dyn_dshift_ev_gv(false,false);break;		
			/* Imul Ev,Gv */
			case 0xaf:dyn_imul_gvev(0);break;
			/* CMPXCHG */
			case 0xb1:dyn_cmpxchg_evgv();break;
			/* LFS,LGS */
			case 0xb4:
				dyn_get_modrm();
				if (GCC_UNLIKELY(decode.modrm.mod==3)) goto illegalopcode;
				dyn_load_seg_off_ea(fs);
				break;
			case 0xb5:
				dyn_get_modrm();
				if (GCC_UNLIKELY(decode.modrm.mod==3)) goto illegalopcode;
				dyn_load_seg_off_ea(gs);
				break;
			/* MOVZX Gv,Eb/Ew */
			case 0xb6:dyn_mov_ev_gb(false);break;
			case 0xb7:dyn_mov_ev_gw(false);break;
			/* MOVSX Gv,Eb/Ew */
			case 0xbe:dyn_mov_ev_gb(true);break;
			case 0xbf:dyn_mov_ev_gw(true);break;

			default:
#if DYN_LOG
				LOG_MSG("Unhandled dual opcode 0F%02X",dual_code);
#endif
				goto illegalopcode;
			}
		}break;

		case 0x10:dyn_dop_ebgb(DOP_ADC);break;
		case 0x11:dyn_dop_evgv(DOP_ADC);break;
		case 0x12:dyn_dop_gbeb(DOP_ADC);break;
		case 0x13:dyn_dop_gvev(DOP_ADC);break;
		case 0x14:gen_needcarry();gen_dop_byte_imm(DOP_ADC,DREG(EAX),0,decode_fetchb());break;
		case 0x15:gen_needcarry();gen_dop_word_imm(DOP_ADC,decode.big_op,DREG(EAX),decode.big_op ? decode_fetchd() :  decode_fetchw());break;
		case 0x16:dyn_push_seg(ss);break;
		case 0x17:dyn_pop_seg(ss);break;

		case 0x18:dyn_dop_ebgb(DOP_SBB);break;
		case 0x19:dyn_dop_evgv(DOP_SBB);break;
		case 0x1a:dyn_dop_gbeb(DOP_SBB);break;
		case 0x1b:dyn_dop_gvev(DOP_SBB);break;
		case 0x1c:gen_needcarry();gen_dop_byte_imm(DOP_SBB,DREG(EAX),0,decode_fetchb());break;
		case 0x1d:gen_needcarry();gen_dop_word_imm(DOP_SBB,decode.big_op,DREG(EAX),decode.big_op ? decode_fetchd() :  decode_fetchw());break;
		case 0x1e:dyn_push_seg(ds);break;
		case 0x1f:dyn_pop_seg(ds);break;
		case 0x20:dyn_dop_ebgb(DOP_AND);break;
		case 0x21:dyn_dop_evgv(DOP_AND);break;
		case 0x22:dyn_dop_gbeb(DOP_AND);break;
		case 0x23:dyn_dop_gvev(DOP_AND);break;
		case 0x24:gen_discardflags();gen_dop_byte_imm(DOP_AND,DREG(EAX),0,decode_fetchb());break;
		case 0x25:gen_discardflags();dyn_dop_word_imm(DOP_AND,decode.big_op,DREG(EAX));break;
		case 0x26:dyn_segprefix(es);goto restart_prefix;

		case 0x28:dyn_dop_ebgb(DOP_SUB);break;
		case 0x29:dyn_dop_evgv(DOP_SUB);break;
		case 0x2a:dyn_dop_gbeb(DOP_SUB);break;
		case 0x2b:dyn_dop_gvev(DOP_SUB);break;
		case 0x2c:gen_discardflags();gen_dop_byte_imm(DOP_SUB,DREG(EAX),0,decode_fetchb());break;
		case 0x2d:gen_discardflags();gen_dop_word_imm(DOP_SUB,decode.big_op,DREG(EAX),decode.big_op ? decode_fetchd() :  decode_fetchw());break;
		case 0x2e:dyn_segprefix(cs);goto restart_prefix;

		case 0x30:dyn_dop_ebgb(DOP_XOR);break;
		case 0x31:dyn_dop_evgv(DOP_XOR);break;
		case 0x32:dyn_dop_gbeb(DOP_XOR);break;
		case 0x33:dyn_dop_gvev(DOP_XOR);break;
		case 0x34:gen_discardflags();gen_dop_byte_imm(DOP_XOR,DREG(EAX),0,decode_fetchb());break;
		case 0x35:gen_discardflags();gen_dop_word_imm(DOP_XOR,decode.big_op,DREG(EAX),decode.big_op ? decode_fetchd() :  decode_fetchw());break;
		case 0x36:dyn_segprefix(ss);goto restart_prefix;

		case 0x38:dyn_dop_ebgb(DOP_CMP);break;
		case 0x39:dyn_dop_evgv(DOP_CMP);break;
		case 0x3a:dyn_dop_gbeb(DOP_CMP);break;
		case 0x3b:dyn_dop_gvev(DOP_CMP);break;
		case 0x3c:gen_discardflags();gen_dop_byte_imm(DOP_CMP,DREG(EAX),0,decode_fetchb());break;
		case 0x3d:gen_discardflags();gen_dop_word_imm(DOP_CMP,decode.big_op,DREG(EAX),decode.big_op ? decode_fetchd() :  decode_fetchw());break;
		case 0x3e:dyn_segprefix(ds);goto restart_prefix;

		/* INC/DEC general register */
		case 0x40:case 0x41:case 0x42:case 0x43:case 0x44:case 0x45:case 0x46:case 0x47:	
			gen_needcarry();gen_sop_word(SOP_INC,decode.big_op,&DynRegs[opcode&7]);
			break;
		case 0x48:case 0x49:case 0x4a:case 0x4b:case 0x4c:case 0x4d:case 0x4e:case 0x4f:	
			gen_needcarry();gen_sop_word(SOP_DEC,decode.big_op,&DynRegs[opcode&7]);
			break;
		/* PUSH/POP General register */
		case 0x50:case 0x51:case 0x52:case 0x53:case 0x55:case 0x56:case 0x57:	
			dyn_push(&DynRegs[opcode&7]);
			break;
		case 0x54:		/* PUSH SP is special */
			gen_dop_word(DOP_MOV,true,DREG(TMPW),DREG(ESP));
			dyn_push(DREG(TMPW));
			gen_releasereg(DREG(TMPW));
			break;
		case 0x58:case 0x59:case 0x5a:case 0x5b:case 0x5c:case 0x5d:case 0x5e:case 0x5f:	
			dyn_pop(&DynRegs[opcode&7]);
			break;
		case 0x60:		/* PUSHA */
			gen_dop_word(DOP_MOV,true,DREG(TMPW),DREG(ESP));
			for (i=G_EAX;i<=G_EDI;i++) {
				dyn_push_unchecked((i!=G_ESP) ? &DynRegs[i] : DREG(TMPW));
			}
			gen_releasereg(DREG(TMPW));
			break;
		case 0x61:		/* POPA */
			for (i=G_EDI;i>=G_EAX;i--) {
				dyn_pop((i!=G_ESP) ? &DynRegs[i] : DREG(TMPW),false);
			}
			gen_releasereg(DREG(TMPW));
			break;
		//segprefix FS,GS
		case 0x64:dyn_segprefix(fs);goto restart_prefix;
		case 0x65:dyn_segprefix(gs);goto restart_prefix;
		//Push immediates
		//Operand size		
		case 0x66:decode.big_op=!cpu.code.big;goto restart_prefix;
		//Address size		
		case 0x67:decode.big_addr=!cpu.code.big;goto restart_prefix;
		case 0x68:		/* PUSH Iv */
			gen_dop_word_imm(DOP_MOV,decode.big_op,DREG(TMPW),decode.big_op ? decode_fetchd() : decode_fetchw());
			dyn_push(DREG(TMPW));
			gen_releasereg(DREG(TMPW));
			break;
		/* Imul Ivx */
		case 0x69:dyn_imul_gvev(decode.big_op ? 4 : 2);break;
		case 0x6a:		/* PUSH Ibx */
			gen_dop_word_imm(DOP_MOV,true,DREG(TMPW),(Bit8s)decode_fetchb());
			dyn_push(DREG(TMPW));
			gen_releasereg(DREG(TMPW));
			break;
		/* Imul Ibx */
		case 0x6b:dyn_imul_gvev(1);break;
		/* Short conditional jumps */
		case 0x70:case 0x71:case 0x72:case 0x73:case 0x74:case 0x75:case 0x76:case 0x77:	
		case 0x78:case 0x79:case 0x7a:case 0x7b:case 0x7c:case 0x7d:case 0x7e:case 0x7f:	
			dyn_branched_exit((BranchTypes)(opcode&0xf),(Bit8s)decode_fetchb());	
			goto finish_block;
		/* Group 1 */
		case 0x80:dyn_grp1_eb_ib();break;
		case 0x81:dyn_grp1_ev_ivx(false);break;
		case 0x82:dyn_grp1_eb_ib();break;
		case 0x83:dyn_grp1_ev_ivx(true);break;
		/* TEST Gb,Eb Gv,Ev */
		case 0x84:dyn_dop_gbeb(DOP_TEST);break;
		case 0x85:dyn_dop_gvev(DOP_TEST);break;
		/* XCHG Eb,Gb Ev,Gv */
		case 0x86:dyn_dop_ebgb(DOP_XCHG);break;
		case 0x87:dyn_dop_evgv(DOP_XCHG);break;
		/* MOV e,g and g,e */
		case 0x88:dyn_mov_ebgb();break;
		case 0x89:dyn_mov_evgv();break;
		case 0x8a:dyn_mov_gbeb();break;
		case 0x8b:dyn_mov_gvev();break;
		/* MOV ev,seg */
		case 0x8c:dyn_mov_ev_seg();break;
		/* LEA Gv */
		case 0x8d:
			dyn_get_modrm();
			if (decode.big_op) {
				dyn_fill_ea(false,&DynRegs[decode.modrm.reg]);
			} else {
				dyn_fill_ea(false);
				gen_dop_word(DOP_MOV,decode.big_op,&DynRegs[decode.modrm.reg],DREG(EA));
				gen_releasereg(DREG(EA));
			}
			break;
		/* Mov seg,ev */
		case 0x8e:dyn_mov_seg_ev();break;
		/* POP Ev */
		case 0x8f:dyn_pop_ev();break;
		case 0x90:	//NOP
		case 0x9b:	//WAIT/FWAIT
			break;
		//XCHG ax,reg
		case 0x91:case 0x92:case 0x93:case 0x94:case 0x95:case 0x96:case 0x97:	
			gen_dop_word(DOP_XCHG,decode.big_op,DREG(EAX),&DynRegs[opcode&07]);
			break;
		/* CBW/CWDE */
		case 0x98:
			gen_cbw(decode.big_op,DREG(EAX));
			break;
		/* CWD/CDQ */
		case 0x99:
			gen_cwd(decode.big_op,DREG(EAX),DREG(EDX));
			break;
		/* CALL FAR Ip */
		case 0x9a:dyn_call_far_imm();goto finish_block;
		case 0x9c:		//PUSHF
			gen_protectflags();
			gen_releasereg(DREG(ESP));
			dyn_flags_gen_to_host();
			gen_call_function((void *)&CPU_PUSHF,"%Rd%Id",DREG(TMPB),decode.big_op);
			dyn_check_bool_exception(DREG(TMPB));
			gen_releasereg(DREG(TMPB));
			break;
		case 0x9d:		//POPF
			gen_releasereg(DREG(ESP));
			gen_releasereg(DREG(FLAGS));
			gen_call_function((void *)&CPU_POPF,"%Rd%Id",DREG(TMPB),decode.big_op);
			dyn_check_bool_exception(DREG(TMPB));
			dyn_flags_host_to_gen();
			gen_releasereg(DREG(TMPB));
			dyn_check_irqrequest();
			break;
		/* MOV AL,direct addresses */
		case 0xa0:
			gen_lea(DREG(EA),decode.segprefix ? decode.segprefix : DREG(DS),0,0,
				decode.big_addr ? decode_fetchd() : decode_fetchw());
			dyn_read_byte_release(DREG(EA),DREG(EAX),false);
			break;
		/* MOV AX,direct addresses */
		case 0xa1:
			gen_lea(DREG(EA),decode.segprefix ? decode.segprefix : DREG(DS),0,0,
				decode.big_addr ? decode_fetchd() : decode_fetchw());
			dyn_read_word_release(DREG(EA),DREG(EAX),decode.big_op);
			break;
		/* MOV direct address,AL */
		case 0xa2:
			if (decode.big_addr) {
				Bitu val;
				if (decode_fetchd_imm(val)) {
					gen_lea_imm_mem(DREG(EA),decode.segprefix ? decode.segprefix : DREG(DS),(void*)val);
				} else {
					gen_lea(DREG(EA),decode.segprefix ? decode.segprefix : DREG(DS),0,0,(Bits)val);
				}
				dyn_write_byte_release(DREG(EA),DREG(EAX),false);
			} else {
				gen_lea(DREG(EA),decode.segprefix ? decode.segprefix : DREG(DS),0,0,decode_fetchw());
				dyn_write_byte_release(DREG(EA),DREG(EAX),false);
			}
			break;
		/* MOV direct addresses,AX */
		case 0xa3:
			gen_lea(DREG(EA),decode.segprefix ? decode.segprefix : DREG(DS),0,0,
				decode.big_addr ? decode_fetchd() : decode_fetchw());
			dyn_write_word_release(DREG(EA),DREG(EAX),decode.big_op);
			break;
		/* MOVSB/W/D*/
		case 0xa4:dyn_string(STR_MOVSB);break;
		case 0xa5:dyn_string(decode.big_op ? STR_MOVSD : STR_MOVSW);break;
		/* TEST AL,AX Imm */
		case 0xa8:gen_discardflags();gen_dop_byte_imm(DOP_TEST,DREG(EAX),0,decode_fetchb());break;
		case 0xa9:gen_discardflags();gen_dop_word_imm(DOP_TEST,decode.big_op,DREG(EAX),decode.big_op ? decode_fetchd() :  decode_fetchw());break;
		/* STOSB/W/D*/
		case 0xaa:dyn_string(STR_STOSB);break;
		case 0xab:dyn_string(decode.big_op ? STR_STOSD : STR_STOSW);break;
		/* LODSB/W/D*/
		case 0xac:dyn_string(STR_LODSB);break;
		case 0xad:dyn_string(decode.big_op ? STR_LODSD : STR_LODSW);break;
		//Mov Byte reg,Imm byte
		case 0xb0:case 0xb1:case 0xb2:case 0xb3:case 0xb4:case 0xb5:case 0xb6:case 0xb7:	
			gen_dop_byte_imm(DOP_MOV,&DynRegs[opcode&3],opcode&4,decode_fetchb());
			break;
		//Mov word reg imm byte,word,
		case 0xb8:case 0xb9:case 0xba:case 0xbb:case 0xbc:case 0xbd:case 0xbe:case 0xbf:	
			if (decode.big_op) {
				dyn_dop_word_imm(DOP_MOV,decode.big_op,&DynRegs[opcode&7]);break;
			} else {
				gen_dop_word_imm(DOP_MOV,decode.big_op,&DynRegs[opcode&7],decode_fetchw());break;
			}
			break;
		//GRP2 Eb/Ev,Ib
		case 0xc0:dyn_grp2_eb(grp2_imm);break;
		case 0xc1:dyn_grp2_ev(grp2_imm);break;
		//RET near Iw / Ret
		case 0xc2:dyn_ret_near(decode_fetchw());goto finish_block;
		case 0xc3:dyn_ret_near(0);goto finish_block;
		//LES
		case 0xc4:
			dyn_get_modrm();
			if (GCC_UNLIKELY(decode.modrm.mod==3)) goto illegalopcode;
			dyn_load_seg_off_ea(es);
			break;
		//LDS
		case 0xc5:
			dyn_get_modrm();
			if (GCC_UNLIKELY(decode.modrm.mod==3)) goto illegalopcode;
			dyn_load_seg_off_ea(ds);
			break;
		// MOV Eb/Ev,Ib/Iv
		case 0xc6:dyn_mov_ebib();break;
		case 0xc7:dyn_mov_eviv();break;
		//ENTER and LEAVE
		case 0xc8:dyn_enter();break;
		case 0xc9:dyn_leave();break;
		//RET far Iw / Ret
		case 0xca:dyn_ret_far(decode_fetchw());goto finish_block;
		case 0xcb:dyn_ret_far(0);goto finish_block;
		/* Interrupt */
//		case 0xcd:dyn_interrupt(decode_fetchb());goto finish_block;
		/* IRET */
		case 0xcf:dyn_iret();goto finish_block;

		//GRP2 Eb/Ev,1
		case 0xd0:dyn_grp2_eb(grp2_1);break;
		case 0xd1:dyn_grp2_ev(grp2_1);break;
		//GRP2 Eb/Ev,CL
		case 0xd2:dyn_grp2_eb(grp2_cl);break;
		case 0xd3:dyn_grp2_ev(grp2_cl);break;
		//FPU
#ifdef CPU_FPU
		case 0xd8:
#ifdef X86_DYNFPU_DH_ENABLED
			if (dyn_dh_fpu.dh_fpu_enabled) {
				if (fpu_used) dh_fpu_esc0();
				else {
					dh_fpu_startup();
					dh_fpu_esc0();
				}
			} else
#endif
			dyn_fpu_esc0();
			break;
		case 0xd9:
#ifdef X86_DYNFPU_DH_ENABLED
			if (dyn_dh_fpu.dh_fpu_enabled) {
				if (fpu_used) dh_fpu_esc1();
				else {
					dh_fpu_startup();
					dh_fpu_esc1();
				}
			} else
#endif
			dyn_fpu_esc1();
			break;
		case 0xda:
#ifdef X86_DYNFPU_DH_ENABLED
			if (dyn_dh_fpu.dh_fpu_enabled) {
				if (fpu_used) dh_fpu_esc2();
				else {
					dh_fpu_startup();
					dh_fpu_esc2();
				}
			} else
#endif
			dyn_fpu_esc2();
			break;
		case 0xdb:
#ifdef X86_DYNFPU_DH_ENABLED
			if (dyn_dh_fpu.dh_fpu_enabled) {
				if (fpu_used) dh_fpu_esc3();
				else {
					dh_fpu_startup();
					dh_fpu_esc3();
				}
			} else
#endif
			dyn_fpu_esc3();
			break;
		case 0xdc:
#ifdef X86_DYNFPU_DH_ENABLED
			if (dyn_dh_fpu.dh_fpu_enabled) {
				if (fpu_used) dh_fpu_esc4();
				else {
					dh_fpu_startup();
					dh_fpu_esc4();
				}
			} else
#endif
			dyn_fpu_esc4();
			break;
		case 0xdd:
#ifdef X86_DYNFPU_DH_ENABLED
			if (dyn_dh_fpu.dh_fpu_enabled) {
				if (fpu_used) dh_fpu_esc5();
				else {
					dh_fpu_startup();
					dh_fpu_esc5();
				}
			} else
#endif
			dyn_fpu_esc5();
			break;
		case 0xde:
#ifdef X86_DYNFPU_DH_ENABLED
			if (dyn_dh_fpu.dh_fpu_enabled) {
				if (fpu_used) dh_fpu_esc6();
				else {
					dh_fpu_startup();
					dh_fpu_esc6();
				}
			} else
#endif
			dyn_fpu_esc6();
			break;
		case 0xdf:
#ifdef X86_DYNFPU_DH_ENABLED
			if (dyn_dh_fpu.dh_fpu_enabled) {
				if (fpu_used) dh_fpu_esc7();
				else {
					dh_fpu_startup();
					dh_fpu_esc7();
				}
			} else
#endif
			dyn_fpu_esc7();
			break;
#endif
		//Loops 
		case 0xe2:dyn_loop(LOOP_NONE);goto finish_block;
		case 0xe3:dyn_loop(LOOP_JCXZ);goto finish_block;
		//IN AL/AX,imm
		case 0xe4: {
			Bitu port=decode_fetchb();
			dyn_add_iocheck_var(port,1);
			gen_call_function((void*)&IO_ReadB,"%Id%Rl",port,DREG(EAX));
			} break;
		case 0xe5: {
			Bitu port=decode_fetchb();
			dyn_add_iocheck_var(port,decode.big_op?4:2);
			if (decode.big_op) {
                gen_call_function((void*)&IO_ReadD,"%Id%Rd",port,DREG(EAX));
			} else {
				gen_call_function((void*)&IO_ReadW,"%Id%Rw",port,DREG(EAX));
			}
			} break;
		//OUT imm,AL
		case 0xe6: {
			Bitu port=decode_fetchb();
			dyn_add_iocheck_var(port,1);
			gen_call_function((void*)&IO_WriteB,"%Id%Dl",port,DREG(EAX));
			} break;
		case 0xe7: {
			Bitu port=decode_fetchb();
			dyn_add_iocheck_var(port,decode.big_op?4:2);
			if (decode.big_op) {
                gen_call_function((void*)&IO_WriteD,"%Id%Dd",port,DREG(EAX));
			} else {
				gen_call_function((void*)&IO_WriteW,"%Id%Dw",port,DREG(EAX));
			}
			} break;
		case 0xe8:		/* CALL Ivx */
			dyn_call_near_imm();
			goto finish_block;
		case 0xe9:		/* Jmp Ivx */
			dyn_exit_link(decode.big_op ? (Bit32s)decode_fetchd() : (Bit16s)decode_fetchw());
			goto finish_block;
		case 0xea:		/* JMP FAR Ip */
			dyn_jmp_far_imm();
			goto finish_block;
			/* Jmp Ibx */
		case 0xeb:dyn_exit_link((Bit8s)decode_fetchb());goto finish_block;
		/* IN AL/AX,DX*/
		case 0xec:
			dyn_add_iocheck(1);
			gen_call_function((void*)&IO_ReadB,"%Dw%Rl",DREG(EDX),DREG(EAX));
			break;
		case 0xed:
			dyn_add_iocheck(decode.big_op?4:2);
			if (decode.big_op) {
                gen_call_function((void*)&IO_ReadD,"%Dw%Rd",DREG(EDX),DREG(EAX));
			} else {
				gen_call_function((void*)&IO_ReadW,"%Dw%Rw",DREG(EDX),DREG(EAX));
			}
			break;
		/* OUT DX,AL/AX */
		case 0xee:
			dyn_add_iocheck(1);
			gen_call_function((void*)&IO_WriteB,"%Dw%Dl",DREG(EDX),DREG(EAX));
			break;
		case 0xef:
			dyn_add_iocheck(decode.big_op?4:2);
			if (decode.big_op) {
                gen_call_function((void*)&IO_WriteD,"%Dw%Dd",DREG(EDX),DREG(EAX));
			} else {
				gen_call_function((void*)&IO_WriteW,"%Dw%Dw",DREG(EDX),DREG(EAX));
			}
			break;
		case 0xf0:		//LOCK
			break;
		case 0xf2:		//REPNE/NZ
			decode.rep=REP_NZ;
			goto restart_prefix;
		case 0xf3:		//REPE/Z
			decode.rep=REP_Z;
			goto restart_prefix;
		/* Change carry flag */
		case 0xf5:		//CMC
		case 0xf8:		//CLC
		case 0xf9:		//STC
			gen_needflags();
			cache_addb(opcode);break;
		/* GRP 3 Eb/EV */
		case 0xf6:dyn_grp3_eb();break;
		case 0xf7:dyn_grp3_ev();break;
		/* Change interrupt flag */
		case 0xfa:		//CLI
			gen_releasereg(DREG(FLAGS));
			gen_call_function((void *)&CPU_CLI,"%Rd",DREG(TMPB));
			dyn_check_bool_exception(DREG(TMPB));
			gen_releasereg(DREG(TMPB));
			break;
		case 0xfb:		//STI
			gen_releasereg(DREG(FLAGS));
			gen_call_function((void *)&CPU_STI,"%Rd",DREG(TMPB));
			dyn_check_bool_exception(DREG(TMPB));
			gen_releasereg(DREG(TMPB));
			dyn_check_irqrequest();
			if (max_opcodes<=0) max_opcodes=1;		//Allow 1 extra opcode
			break;
		case 0xfc:		//CLD
			gen_protectflags();
			gen_dop_word_imm(DOP_AND,true,DREG(FLAGS),~FLAG_DF);
			gen_save_host_direct(&cpu.direction,1);
			break;
		case 0xfd:		//STD
			gen_protectflags();
			gen_dop_word_imm(DOP_OR,true,DREG(FLAGS),FLAG_DF);
			gen_save_host_direct(&cpu.direction,-1);
			break;
		/* GRP 4 Eb and callback's */
		case 0xfe:
            dyn_get_modrm();
			switch (decode.modrm.reg) {
			case 0x0://INC Eb
			case 0x1://DEC Eb
				if (decode.modrm.mod<3) {
					dyn_fill_ea();dyn_read_byte(DREG(EA),DREG(TMPB),false);
					gen_needcarry();
					gen_sop_byte(decode.modrm.reg==0 ? SOP_INC : SOP_DEC,DREG(TMPB),0);
					dyn_write_byte_release(DREG(EA),DREG(TMPB),false);
					gen_releasereg(DREG(TMPB));
				} else {
					gen_needcarry();
					gen_sop_byte(decode.modrm.reg==0 ? SOP_INC : SOP_DEC,
						&DynRegs[decode.modrm.rm&3],decode.modrm.rm&4);
				}
				break;
			case 0x7:		//CALBACK Iw
				gen_save_host_direct(&core_dyn.callback,decode_fetchw());
				dyn_set_eip_end();
				dyn_reduce_cycles();
				dyn_save_critical_regs();
				gen_return(BR_CallBack);
				dyn_closeblock();
				goto finish_block;
			}
			break;

		case 0xff: 
			{
            dyn_get_modrm();DynReg * src;
			if (decode.modrm.mod<3) {
				dyn_fill_ea();
				dyn_read_word(DREG(EA),DREG(TMPW),decode.big_op);
				src=DREG(TMPW);
			} else src=&DynRegs[decode.modrm.rm];
			switch (decode.modrm.reg) {
			case 0x0://INC Ev
			case 0x1://DEC Ev
				gen_needcarry();
				gen_sop_word(decode.modrm.reg==0 ? SOP_INC : SOP_DEC,decode.big_op,src);
				if (decode.modrm.mod<3){
					dyn_write_word_release(DREG(EA),DREG(TMPW),decode.big_op);
					gen_releasereg(DREG(TMPW));
				}
				break;
			case 0x2:	/* CALL Ev */
				gen_lea(DREG(TMPB),DREG(EIP),0,0,decode.code-decode.code_start);
				dyn_push(DREG(TMPB));
				gen_releasereg(DREG(TMPB));
				gen_dop_word(DOP_MOV,decode.big_op,DREG(EIP),src);
				goto core_close_block;
			case 0x4:	/* JMP Ev */
				gen_dop_word(DOP_MOV,decode.big_op,DREG(EIP),src);
				goto core_close_block;
			case 0x3:	/* CALL Ep */
			case 0x5:	/* JMP Ep */
				gen_protectflags();
				dyn_flags_gen_to_host();
				gen_lea(DREG(EA),DREG(EA),0,0,decode.big_op ? 4: 2);
				dyn_read_word(DREG(EA),DREG(EA),false);
				dyn_set_eip_last_end(DREG(TMPB));
				dyn_save_critical_regs();
				gen_call_function(
					decode.modrm.reg == 3 ? (void*)&CPU_CALL : (void*)&CPU_JMP,
					decode.big_op ? "%Id%Drw%Drd%Drd" : "%Id%Drw%Drw%Drd",
					decode.big_op,DREG(EA),DREG(TMPW),DREG(TMPB));
				dyn_flags_host_to_gen();
				goto core_close_block;
			case 0x6:		/* PUSH Ev */
				gen_releasereg(DREG(EA));
				dyn_push(src);
				break;
			default:
				LOG(LOG_CPU,LOG_ERROR)("CPU:GRP5:Illegal opcode 0xff");
				goto illegalopcode;
			}}
			break;
		default:
#if DYN_LOG
//			LOG_MSG("Dynamic unhandled opcode %X",opcode);
#endif
			goto illegalopcode;
		}
	}
	// link to next block because the maximum number of opcodes has been reached
	dyn_set_eip_end();
	dyn_reduce_cycles();
	dyn_save_critical_regs();
	gen_jmp_ptr(&decode.block->link[0].to,offsetof(CacheBlock,cache.start));
	dyn_closeblock();
	goto finish_block;
core_close_block:
	dyn_reduce_cycles();
	dyn_save_critical_regs();
	gen_return(BR_Normal);
	dyn_closeblock();
	goto finish_block;
illegalopcode:
	dyn_set_eip_last();
	dyn_reduce_cycles();
	dyn_save_critical_regs();
	gen_return(BR_Opcode);
	dyn_closeblock();
	goto finish_block;
#if (C_DEBUG)
	dyn_set_eip_last();
	dyn_reduce_cycles();
	dyn_save_critical_regs();
	gen_return(BR_OpcodeFull);
	dyn_closeblock();
	goto finish_block;
#endif
finish_block:
	/* Setup the correct end-address */
	decode.active_block->page.end=--decode.page.index;
//	LOG_MSG("Created block size %d start %d end %d",decode.block->cache.size,decode.block->page.start,decode.block->page.end);
	return decode.block;
}
