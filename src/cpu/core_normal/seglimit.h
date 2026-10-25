/*
 *  Copyright (C) 2026  The DOSBox-X Team
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

/* Segment limit checks for the ModR/M memory operand.
 *
 * The EA functions (table_ea.h) record which segment register the operand
 * uses in core.ea_seg (through EA_SEG_DS/EA_SEG_SS). LoadM* and SaveM* then
 * check offset+size-1 against that segment's limit, so the check happens at
 * the actual access with the actual size (LEA never faults, LES checks both
 * the offset and the segment word). Accesses that do not come from a ModR/M
 * operand (code fetch, stack, string instructions, XLAT) have core.ea_seg set
 * to CORE_EA_NONE and are not checked here.
 *
 * A violation throws CoreSegLimitFault, which the core loop catches and turns
 * into #SS(0) for the stack segment or #GP(0) otherwise, with EIP still at
 * the start of the instruction. Real mode and V86 mode use the limits held
 * in the segment descriptor cache (64KB, or 4GB after "unreal" mode).
 *
 * A limit of EANoSegmentLimitMagic (FFFFFFFFh, FFFFh on the 286 core) turns
 * the check off, as table_ea.h explains: a 386 with a 4GB segment does not
 * fault an access that wraps past FFFFFFFFh.
 *
 * The full core (core_full) uses Core_SegLimitCheck with the segment it
 * records in inst.rm_seg, and defines SEGLIMIT_NO_CORE_STRUCT.
 *
 * Controlled by the "segment limits" setting (do_seg_limits). */

#define CORE_EA_NONE		0xFFu

/* same definition as in core_normal/helpers.h, which is included later.
 * A core that does not define CPU_CORE (the full core) defines it before this file */
#ifndef EANoSegmentLimitMagic
#if CPU_CORE <= CPU_ARCHTYPE_286
#define EANoSegmentLimitMagic 0xFFFFul
#else
#define EANoSegmentLimitMagic 0xFFFFFFFFul
#endif
#endif

struct CoreSegLimitFault {
	unsigned int vector;
};

/* check an access of size bytes at linear address lin through segment register seg (CORE_EA_NONE: no check) */
static INLINE void Core_SegLimitCheck(const uint8_t seg,const PhysPt lin,const uint32_t size) {
	if (seg != CORE_EA_NONE && do_seg_limits) {
		const SegNames s = (SegNames)seg;
		const uint32_t off = (uint32_t)(lin - SegPhys(s));
		const uint64_t last = (uint64_t)off + (uint64_t)size - 1ull;
		const uint32_t lim = (uint32_t)SegLimit(s);
		bool bad;

		if (Segs.expanddown[s])
			bad = (off <= lim && lim != 0) || last > 0xFFFFFFFFull;	/* limit 0: see the note in table_ea.h */
		else
			bad = last > (uint64_t)lim && lim != (uint32_t)EANoSegmentLimitMagic;

		if (GCC_UNLIKELY(bad)) {
			CoreSegLimitFault f;
			f.vector = (s == ss) ? EXCEPTION_SS : EXCEPTION_GP;
			throw f;
		}
	}
}

#ifndef SEGLIMIT_NO_CORE_STRUCT
/* normal core variants: the EA functions record the segment in core.ea_seg */
#define EA_SEG_DS		core.ea_seg=(uint8_t)core.base_val_ds;
#define EA_SEG_SS		core.ea_seg=(uint8_t)core.base_val_ss;

static INLINE void Core_EALimitCheck(const PhysPt lin,const uint32_t size) {
	Core_SegLimitCheck(core.ea_seg,lin,size);
}
#endif

/* Stack limit checks for the PUSH/POP type accesses of the instructions this
 * core executes itself (pushes done by CPU_Interrupt and the far control
 * transfers in cpu.cpp are not checked here). The upper bound of an
 * expand-down stack segment comes from its B bit (cpu.stack.big). */
static INLINE void Core_StackLimitCheck(const uint32_t off,const uint32_t size) {
	if (do_seg_limits) {
		const uint64_t last = (uint64_t)off + (uint64_t)size - 1ull;
		const uint32_t lim = (uint32_t)SegLimit(ss);
		bool bad;

		if (Segs.expanddown[ss])
			bad = (off <= lim && lim != 0) || last > (cpu.stack.big ? 0xFFFFFFFFull : 0xFFFFull);
		else
			bad = last > (uint64_t)lim && lim != (uint32_t)EANoSegmentLimitMagic;

		if (GCC_UNLIKELY(bad)) {
			CoreSegLimitFault f;
			f.vector = EXCEPTION_SS;
			throw f;
		}
	}
}

static INLINE void Core_Push16(const uint16_t v) {
	Core_StackLimitCheck((uint32_t)((reg_esp - 2u) & cpu.stack.mask),2u);
	CPU_Push16(v);
}

static INLINE void Core_Push32(const uint32_t v) {
	Core_StackLimitCheck((uint32_t)((reg_esp - 4u) & cpu.stack.mask),4u);
	CPU_Push32(v);
}

static INLINE uint16_t Core_Pop16(void) {
	Core_StackLimitCheck((uint32_t)(reg_esp & cpu.stack.mask),2u);
	return CPU_Pop16();
}

static INLINE uint32_t Core_Pop32(void) {
	Core_StackLimitCheck((uint32_t)(reg_esp & cpu.stack.mask),4u);
	return CPU_Pop32();
}
