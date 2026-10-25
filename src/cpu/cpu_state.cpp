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

/* Conversion between DOSBox-X's CPU state and CPU_FullState (see cpu_state.h) */

#include <stdio.h>
#include <string.h>

#include "dosbox.h"
#include "cpu.h"
#include "cpu_state.h"
#include "fpu.h"
#include "lazyflags.h"
#include "paging.h"
#include "regs.h"

extern uint16_t cpu_sep_cs;
extern uint32_t cpu_sep_esp,cpu_sep_eip;

void CPU_GetTaskRegister(Bitu &selector,Descriptor &desc,PhysPt &base,Bitu &limit,bool &is386,bool &valid);
void CPU_SetTaskRegister(Bitu selector,const Descriptor &desc,PhysPt base,Bitu limit,bool is386,bool valid);

/* Read a descriptor from the GDT or LDT without raising an exception */
static bool ReadDescriptor(Bitu selector,Descriptor &desc) {
	LinearPt base;
	Bitu limit;

	if (selector & 4) {
		if ((cpu.gdt.SLDT() & 0xfffc) == 0) return false;
		base = cpu.gdt.GetLDTBase();
		limit = cpu.gdt.GetLDTLimit();
	}
	else {
		base = cpu.gdt.GetBase();
		limit = cpu.gdt.GetLimit();
	}

	const Bitu ofs = selector & ~(Bitu)7u;
	if (ofs + 7u > limit) return false;

	uint32_t lo = 0,hi = 0;
	const Bitu old_mpl = cpu.mpl;
	cpu.mpl = 0;
	const bool fault = mem_readd_checked((LinearPt)(base+ofs),&lo) || mem_readd_checked((LinearPt)(base+ofs+4u),&hi);
	cpu.mpl = old_mpl;
	if (fault) return false;

	desc.saved.fill[0] = lo;
	desc.saved.fill[1] = hi;
	return true;
}

static void GetSegment(const SegNames sr,CPU_FullSegment &seg,const bool v86) {
	seg.selector = (uint16_t)Segs.val[sr];
	seg.base = (uint32_t)Segs.phys[sr];
	seg.limit = (uint32_t)Segs.limit[sr];
	seg.g = seg.limit > 0xFFFFFu;
	seg.s = true;
	seg.present = true;
	seg.avl = false;
	seg.unusable = false;
	seg.dpl = v86 ? 3 : 0;
	seg.db = false;

	/* real mode: an accessed read/write data segment, or an accessed execute/read code
	 * segment. Virtual 8086 mode: accessed read/write data for all of them, as the CPU
	 * loads them (access rights F3h). */
	if (sr == cs) {
		seg.type = v86 ? 0x3 : 0xB;
		seg.db = cpu.code.big;
	}
	else {
		seg.type = 0x3;
		if (sr == ss) seg.db = cpu.stack.big;
	}

	if (cpu.pmode && !v86) {
		Descriptor desc;

		if (sr != cs && sr != ss && (seg.selector & 0xFFFCu) == 0) {
			seg.unusable = true;
			seg.present = false;
		}
		else if (ReadDescriptor(seg.selector,desc)) {
			seg.type = desc.saved.seg.type & 0xFu;
			seg.s = (desc.saved.seg.type & 0x10u) != 0;
			seg.dpl = (uint8_t)desc.saved.seg.dpl;
			seg.present = desc.saved.seg.p != 0;
			seg.avl = desc.saved.seg.avl != 0;
			if (sr != cs && sr != ss) seg.db = desc.saved.seg.big != 0;
		}
		else {
			seg.dpl = (uint8_t)(seg.selector & 3u);
		}

		/* CS and SS run at the CPL, except for a conforming code segment */
		if (sr == ss || (sr == cs && (seg.type & 0xCu) != 0xCu))
			seg.dpl = (uint8_t)cpu.cpl;
	}

	/* DOSBox-X keeps the expand-down bit of a data segment */
	if (!(seg.type & 8u))
		seg.type = (uint8_t)((seg.type & ~4u) | (Segs.expanddown[sr] ? 4u : 0u));
}

static void SetSegment(const SegNames sr,const CPU_FullSegment &seg) {
	Segs.val[sr] = seg.selector;
	Segs.phys[sr] = seg.base;
	Segs.limit[sr] = seg.limit;
	Segs.expanddown[sr] = seg.s && (seg.type & 0xCu) == 0x4u;
}

/* A descriptor that holds what a CPU_FullSegment describes */
static void MakeDescriptor(const CPU_FullSegment &seg,Descriptor &desc) {
	const uint32_t limit = seg.g ? (seg.limit >> 12u) : seg.limit;

	desc.saved.fill[0] = desc.saved.fill[1] = 0;
	desc.saved.seg.limit_0_15 = limit & 0xFFFFu;
	desc.saved.seg.limit_16_19 = (limit >> 16u) & 0xFu;
	desc.saved.seg.base_0_15 = seg.base & 0xFFFFu;
	desc.saved.seg.base_16_23 = (seg.base >> 16u) & 0xFFu;
	desc.saved.seg.base_24_31 = (seg.base >> 24u) & 0xFFu;
	desc.saved.seg.type = (seg.type & 0xFu) | (seg.s ? 0x10u : 0u);
	desc.saved.seg.dpl = seg.dpl & 3u;
	desc.saved.seg.p = seg.present ? 1 : 0;
	desc.saved.seg.avl = seg.avl ? 1 : 0;
	desc.saved.seg.big = seg.db ? 1 : 0;
	desc.saved.seg.g = seg.g ? 1 : 0;
}

void CPU_GetFullState(CPU_FullState &state) {
	memset(&state,0,sizeof(state));

	FillFlags();
	state.eax = reg_eax; state.ecx = reg_ecx; state.edx = reg_edx; state.ebx = reg_ebx;
	state.esp = reg_esp; state.ebp = reg_ebp; state.esi = reg_esi; state.edi = reg_edi;
	state.eip = reg_eip;
	state.eflags = (uint32_t)reg_flags;

	const bool v86 = cpu.pmode && (reg_flags & FLAG_VM);
	for (unsigned int i=0;i < 6;i++)
		GetSegment((SegNames)i,state.seg[i],v86);

	{
		Bitu selector,limit;
		Descriptor desc;
		PhysPt base;
		bool is386,valid;

		CPU_GetTaskRegister(selector,desc,base,limit,is386,valid);
		CPU_FullSegment &tr = state.tr;
		tr.selector = (uint16_t)selector;
		tr.base = (uint32_t)base;
		tr.limit = (uint32_t)limit;
		tr.g = tr.limit > 0xFFFFFu;
		tr.s = false;
		tr.unusable = !valid;
		if (valid) {
			tr.type = desc.saved.seg.type & 0xFu;
			tr.dpl = (uint8_t)desc.saved.seg.dpl;
			tr.present = desc.saved.seg.p != 0;
			tr.avl = desc.saved.seg.avl != 0;
			tr.db = desc.saved.seg.big != 0;
		}
		else {
			/* no task register loaded: a busy TSS, as the CPU has after reset */
			tr.type = is386 ? 0xB : 0x3;
			tr.present = true;
		}
	}

	{
		CPU_FullSegment &ldt = state.ldt;
		ldt.selector = (uint16_t)cpu.gdt.SLDT();
		ldt.base = (uint32_t)cpu.gdt.GetLDTBase();
		ldt.limit = (uint32_t)cpu.gdt.GetLDTLimit();
		ldt.g = ldt.limit > 0xFFFFFu;
		ldt.type = 0x2;
		ldt.unusable = (ldt.selector & 0xFFFCu) == 0;
		ldt.present = !ldt.unusable;
	}

	state.gdt.base = (uint32_t)cpu.gdt.GetBase();
	state.gdt.limit = (uint16_t)cpu.gdt.GetLimit();
	state.idt.base = (uint32_t)cpu.idt.GetBase();
	state.idt.limit = (uint16_t)cpu.idt.GetLimit();

	state.cr0 = (uint32_t)cpu.cr0;
	state.cr2 = paging.cr2;
	state.cr3 = paging.cr3;
	state.cr4 = (uint32_t)cpu.cr4;
	state.cpl = (uint8_t)cpu.cpl;
	for (unsigned int i=0;i < 8;i++) state.dr[i] = cpu.drx[i];

#if C_FPU
	FPU_GetFullState(state);
#endif

	state.sysenter_cs = cpu_sep_cs;
	state.sysenter_esp = cpu_sep_esp;
	state.sysenter_eip = cpu_sep_eip;
}

void CPU_SetFullState(const CPU_FullState &state) {
	/* control registers first: CR3 before CR0 enables paging, paging before the CPL */
	if (paging.cr3 != state.cr3) CPU_SET_CRX(3,state.cr3);
	CPU_SET_CRX(2,state.cr2);
	CPU_SET_CRX(4,state.cr4);
	if (cpu.cr0 != state.cr0) {
		/* not CPU_SET_CRX(0), which also switches the CPU core and cycles mode on entry to
		 * protected mode */
		if (((cpu.cr0 ^ state.cr0) & CR0_WRITEPROTECT) && CPU_ArchitectureType >= CPU_ARCHTYPE_486OLD)
			PAGING_SetWP((state.cr0 & CR0_WRITEPROTECT) != 0);
		cpu.cr0 = state.cr0;
		cpu.pmode = (state.cr0 & CR0_PROTECTION) != 0;
		PAGING_Enable(cpu.pmode && (state.cr0 & CR0_PAGING) != 0);
	}
	CPU_SetCPL(state.cpl);

	reg_eax = state.eax; reg_ecx = state.ecx; reg_edx = state.edx; reg_ebx = state.ebx;
	reg_esp = state.esp; reg_ebp = state.ebp; reg_esi = state.esi; reg_edi = state.edi;
	reg_eip = state.eip;
	FillFlags(); /* discard pending lazy flags */
	reg_flags = state.eflags;
	cpu.direction = 1 - (int)((reg_flags & FLAG_DF) >> 9U);

	for (unsigned int i=0;i < 6;i++)
		SetSegment((SegNames)i,state.seg[i]);
	cpu.code.big = state.seg[cs].db;
	cpu.stack.big = state.seg[ss].db;
	if (cpu.stack.big) {
		cpu.stack.mask = 0xffffffff;
		cpu.stack.notmask = 0;
	}
	else {
		cpu.stack.mask = 0xffff;
		cpu.stack.notmask = 0xffff0000;
	}

	{
		Bitu selector,limit;
		Descriptor desc;
		PhysPt base;
		bool is386,valid;

		/* without a valid task register, keep the descriptor DOSBox-X has */
		CPU_GetTaskRegister(selector,desc,base,limit,is386,valid);
		if (!state.tr.unusable) MakeDescriptor(state.tr,desc);
		CPU_SetTaskRegister(state.tr.selector,desc,state.tr.base,state.tr.limit,(state.tr.type & 8u) != 0,!state.tr.unusable);
	}
	cpu.gdt.SetLDTCache(state.ldt.selector,state.ldt.base,state.ldt.limit);

	cpu.gdt.SetBase(state.gdt.base);
	cpu.gdt.SetLimit(state.gdt.limit);
	cpu.idt.SetBase(state.idt.base);
	cpu.idt.SetLimit(state.idt.limit);

	for (unsigned int i=0;i < 8;i++) cpu.drx[i] = state.dr[i];

#if C_FPU
	FPU_SetFullState(state);
#endif

	cpu_sep_cs = (uint16_t)state.sysenter_cs;
	cpu_sep_esp = state.sysenter_esp;
	cpu_sep_eip = state.sysenter_eip;
}

static std::string SegmentToString(const char *name,const CPU_FullSegment &seg) {
	char tmp[160];
	snprintf(tmp,sizeof(tmp),"%-4s %04X base=%08X limit=%08X type=%X s=%u dpl=%u p=%u db=%u g=%u avl=%u%s",
		name,seg.selector,seg.base,seg.limit,seg.type,seg.s,seg.dpl,seg.present,seg.db,seg.g,seg.avl,
		seg.unusable ? " unusable" : "");
	return tmp;
}

std::string CPU_FullStateToString(const CPU_FullState &state) {
	static const char *segnames[6] = {"ES","CS","SS","DS","FS","GS"};
	std::string s;
	char tmp[256];

	snprintf(tmp,sizeof(tmp),"EAX=%08X EBX=%08X ECX=%08X EDX=%08X ESI=%08X EDI=%08X\n",
		state.eax,state.ebx,state.ecx,state.edx,state.esi,state.edi);
	s += tmp;
	snprintf(tmp,sizeof(tmp),"EBP=%08X ESP=%08X EIP=%08X EFLAGS=%08X CPL=%u\n",
		state.ebp,state.esp,state.eip,state.eflags,state.cpl);
	s += tmp;
	snprintf(tmp,sizeof(tmp),"CR0=%08X CR2=%08X CR3=%08X CR4=%08X\n",state.cr0,state.cr2,state.cr3,state.cr4);
	s += tmp;
	for (unsigned int i=0;i < 6;i++)
		s += SegmentToString(segnames[i],state.seg[i]) + "\n";
	s += SegmentToString("TR",state.tr) + "\n";
	s += SegmentToString("LDT",state.ldt) + "\n";
	snprintf(tmp,sizeof(tmp),"GDT=%08X/%04X IDT=%08X/%04X\n",state.gdt.base,state.gdt.limit,state.idt.base,state.idt.limit);
	s += tmp;
	snprintf(tmp,sizeof(tmp),"DR0=%08X DR1=%08X DR2=%08X DR3=%08X DR6=%08X DR7=%08X\n",
		state.dr[0],state.dr[1],state.dr[2],state.dr[3],state.dr[6],state.dr[7]);
	s += tmp;
	snprintf(tmp,sizeof(tmp),"FCW=%04X FSW=%04X FTW=%02X MXCSR=%08X SYSENTER CS=%04X ESP=%08X EIP=%08X\n",
		state.fcw,state.fsw,state.ftw,state.mxcsr,state.sysenter_cs,state.sysenter_esp,state.sysenter_eip);
	s += tmp;
	for (unsigned int i=0;i < 8;i++) {
		const uint8_t *r = state.st[i];
		snprintf(tmp,sizeof(tmp),"ST%u=%02X%02X %02X%02X%02X%02X%02X%02X%02X%02X%s",i,
			r[9],r[8],r[7],r[6],r[5],r[4],r[3],r[2],r[1],r[0],(i & 3) == 3 ? "\n" : " ");
		s += tmp;
	}
	for (unsigned int i=0;i < 8;i++) {
		const uint8_t *r = state.xmm[i];
		snprintf(tmp,sizeof(tmp),"XMM%u=",i);
		s += tmp;
		for (int b=15;b >= 0;b--) {
			snprintf(tmp,sizeof(tmp),"%02X",r[b]);
			s += tmp;
		}
		s += (i & 1) ? "\n" : " ";
	}
	return s;
}

/* What CPU_SetFullState() sets, as DOSBox-X keeps it */
struct CPU_NativeSnapshot {
	CPU_Regs	regs;
	Segments	segs;
	Bitu		cpl,cr0,cr4;
	bool		pmode;
	LinearPt	gdt_base,idt_base,ldt_base;
	Bitu		gdt_limit,idt_limit,ldt_limit,ldt_value;
	uint32_t	stack_mask,stack_notmask;
	bool		stack_big,code_big;
	Bits		direction;
	uint32_t	drx[8];
	uint32_t	cr2,cr3;
	bool		paging_enabled,paging_wp;
	Bitu		tr_selector,tr_limit;
	Descriptor	tr_desc;
	PhysPt		tr_base;
	bool		tr_is386,tr_valid;
	uint16_t	sep_cs;
	uint32_t	sep_esp,sep_eip;
#if C_FPU
	uint16_t	fcw,fsw;
	bool		regvalid[8];
	uint64_t	st_l[8];
	uint16_t	st_h[8];
	uint32_t	xmm[8][4];
	uint32_t	mxcsr;
#endif
};

static void TakeNativeSnapshot(CPU_NativeSnapshot &n) {
	FillFlags();
	n.regs = cpu_regs;
	n.segs = Segs;
	n.cpl = cpu.cpl; n.cr0 = cpu.cr0; n.cr4 = cpu.cr4; n.pmode = cpu.pmode;
	n.gdt_base = cpu.gdt.GetBase(); n.gdt_limit = cpu.gdt.GetLimit();
	n.idt_base = cpu.idt.GetBase(); n.idt_limit = cpu.idt.GetLimit();
	n.ldt_value = cpu.gdt.SLDT(); n.ldt_base = cpu.gdt.GetLDTBase(); n.ldt_limit = cpu.gdt.GetLDTLimit();
	n.stack_mask = cpu.stack.mask; n.stack_notmask = cpu.stack.notmask; n.stack_big = cpu.stack.big;
	n.code_big = cpu.code.big;
	n.direction = cpu.direction;
	memcpy(n.drx,cpu.drx,sizeof(n.drx));
	n.cr2 = paging.cr2; n.cr3 = paging.cr3;
	n.paging_enabled = paging.enabled; n.paging_wp = paging.wp;
	CPU_GetTaskRegister(n.tr_selector,n.tr_desc,n.tr_base,n.tr_limit,n.tr_is386,n.tr_valid);
	n.sep_cs = cpu_sep_cs; n.sep_esp = cpu_sep_esp; n.sep_eip = cpu_sep_eip;
#if C_FPU
	n.fcw = fpu.cw; n.fsw = fpu.sw;
	for (unsigned int i=0;i < 8;i++) {
		n.regvalid[i] = fpu.regvalid[i];
		n.st_l[i] = fpu.regs_80[i].raw.l;
		n.st_h[i] = fpu.regs_80[i].raw.h;
		memcpy(n.xmm[i],fpu.xmmreg[i].u32,sizeof(n.xmm[i]));
	}
	n.mxcsr = fpu.mxcsr;
#endif
}

bool CPU_FullStateRoundTripCheck(std::string &report) {
	CPU_NativeSnapshot before,after;
	CPU_FullState state;

	TakeNativeSnapshot(before);
	CPU_GetFullState(state);
	CPU_SetFullState(state);
	TakeNativeSnapshot(after);

	report.clear();
#define CHECK(x) do { if (!(before.x == after.x)) report += std::string(#x) + " "; } while (0)
#define CHECKMEM(x) do { if (memcmp(&before.x,&after.x,sizeof(before.x)) != 0) report += std::string(#x) + " "; } while (0)
	for (unsigned int i=0;i < 8;i++) {
		if (before.regs.regs[i].dword[0] != after.regs.regs[i].dword[0])
			report += "reg" + std::to_string(i) + " ";
	}
	CHECK(regs.ip.dword[0]);
	CHECK(regs.flags);
	for (unsigned int i=0;i < 6;i++) {
		if (before.segs.val[i] != after.segs.val[i] || before.segs.phys[i] != after.segs.phys[i] ||
			before.segs.limit[i] != after.segs.limit[i] || before.segs.expanddown[i] != after.segs.expanddown[i])
			report += "seg" + std::to_string(i) + " ";
	}
	CHECK(cpl); CHECK(cr0); CHECK(cr4); CHECK(pmode);
	CHECK(gdt_base); CHECK(gdt_limit); CHECK(idt_base); CHECK(idt_limit);
	CHECK(ldt_value); CHECK(ldt_base); CHECK(ldt_limit);
	CHECK(stack_mask); CHECK(stack_notmask); CHECK(stack_big); CHECK(code_big);
	CHECK(direction);
	CHECKMEM(drx);
	CHECK(cr2); CHECK(cr3); CHECK(paging_enabled); CHECK(paging_wp);
	CHECK(tr_selector); CHECK(tr_base); CHECK(tr_limit); CHECK(tr_is386); CHECK(tr_valid);
	if (before.tr_valid && before.tr_desc.saved.fill[0] != after.tr_desc.saved.fill[0]) report += "tr_desc[0] ";
	if (before.tr_valid && before.tr_desc.saved.fill[1] != after.tr_desc.saved.fill[1]) report += "tr_desc[1] ";
	CHECK(sep_cs); CHECK(sep_esp); CHECK(sep_eip);
#if C_FPU
	CHECK(fcw); CHECK(fsw);
	CHECKMEM(regvalid); CHECKMEM(st_l); CHECKMEM(st_h); CHECKMEM(xmm);
	CHECK(mxcsr);
#endif
#undef CHECK
#undef CHECKMEM

	return report.empty();
}
