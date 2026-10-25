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

#ifndef DOSBOX_CPU_STATE_H
#define DOSBOX_CPU_STATE_H

#include <stdint.h>
#include <string>

/* The complete architectural state of the emulated x86 CPU, in a form that does not depend on
 * how DOSBox-X keeps it, for moving the state between the emulated CPU cores and a hardware
 * virtualization core (KVM). The fields follow what KVM takes (kvm_regs, kvm_sregs, kvm_fpu,
 * kvm_debugregs and MSRs) without depending on Linux headers.
 *
 * DOSBox-X does not keep the access rights of a segment register's descriptor cache, so
 * CPU_GetFullState() builds them from what it does keep (CPL, code and stack size, expand-down)
 * and, in protected mode, from the descriptor tables. CPU_SetFullState() takes only what
 * DOSBox-X keeps. */

struct CPU_FullSegment {
	uint16_t	selector;
	uint32_t	base;
	uint32_t	limit;		/* in bytes, after granularity */
	uint8_t		type;		/* descriptor type, 4 bits */
	uint8_t		dpl;
	bool		s;		/* code or data (not system) segment */
	bool		present;
	bool		db;		/* default operation size / big */
	bool		g;		/* granularity, set when limit > 0xFFFFF */
	bool		avl;
	bool		unusable;	/* null selector in protected mode, or no LDT/TSS loaded */
};

struct CPU_FullTable {
	uint32_t	base;
	uint16_t	limit;
};

struct CPU_FullState {
	/* general registers, in REGI_ order */
	uint32_t	eax,ecx,edx,ebx,esp,ebp,esi,edi;
	uint32_t	eip;
	uint32_t	eflags;

	/* segment registers, indexed by SegNames (es,cs,ss,ds,fs,gs) */
	CPU_FullSegment	seg[6];
	CPU_FullSegment	tr,ldt;
	CPU_FullTable	gdt,idt;

	uint32_t	cr0,cr2,cr3,cr4;
	uint8_t		cpl;
	uint32_t	dr[8];

	/* x87 and SSE state, as FXSAVE stores it */
	uint16_t	fcw,fsw;
	uint8_t		ftw;		/* abridged tag: bit n set = physical register n not empty */
	uint8_t		st[8][10];	/* ST(0)-ST(7), 80-bit */
	uint8_t		xmm[8][16];
	uint32_t	mxcsr;

	/* model specific registers */
	uint32_t	sysenter_cs,sysenter_esp,sysenter_eip;
};

void CPU_GetFullState(CPU_FullState &state);
void CPU_SetFullState(const CPU_FullState &state);

/* Implemented by the FPU, used by the above */
void FPU_GetFullState(CPU_FullState &state);
void FPU_SetFullState(const CPU_FullState &state);

/* Debugging aids: describe the state, and check that CPU_SetFullState(CPU_GetFullState())
 * leaves DOSBox-X's own CPU state as it was. The check returns false and describes each
 * difference in 'report' if it does not. */
std::string CPU_FullStateToString(const CPU_FullState &state);
bool CPU_FullStateRoundTripCheck(std::string &report);

#endif
