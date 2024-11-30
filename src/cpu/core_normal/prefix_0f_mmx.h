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
/* NTS: Despite the name of the header, this covers MMX and SSE because the
 *      opcodes are mixed together by sharing common opcodes separated by
 *      prefixes repurposed by Intel as a way to expand the opcode space. */

	CASE_0F_MMX(0x10)
	{
		GetRM;
		const unsigned char reg = (rm >> 3) & 7;

		switch (last_prefix) {
			case MP_NONE:									/* 0F 10 MOVUPS reg, r/m */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII || !CPU_SSE()) goto illegal_opcode;
				if (rm >= 0xc0) {
					SSE_MOVUPS(fpu.xmmreg[reg],fpu.xmmreg[rm & 7]);
				} else {
					GetEAa;
					XMM_Reg xmmsrc;
					xmmsrc.u64[0] = LoadMq(eaa);
					xmmsrc.u64[1] = LoadMq(eaa+8u);
					SSE_MOVUPS(fpu.xmmreg[reg],xmmsrc);
				}
				break;
			case MP_F3:									/* F3 0F 10 MOVSS reg, r/m */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII || !CPU_SSE()) goto illegal_opcode;
				if (rm >= 0xc0) {
					SSE_MOVSS(fpu.xmmreg[reg],fpu.xmmreg[rm & 7]);
				} else {
					GetEAa;
					XMM_Reg xmmsrc;
					xmmsrc.u32[0] = LoadMd(eaa);
					SSE_MOVSS(fpu.xmmreg[reg],xmmsrc);
				}
				break;
			default:
				goto illegal_opcode;
		};
		break;
	}
	CASE_0F_MMX(0x11)
	{
		GetRM;
		const unsigned char reg = (rm >> 3) & 7;

		switch (last_prefix) {
			case MP_NONE:									/* 0F 11 MOVUPS r/m, reg */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII || !CPU_SSE()) goto illegal_opcode;
				if (rm >= 0xc0) {
					SSE_MOVUPS(fpu.xmmreg[rm & 7],fpu.xmmreg[reg]);
				} else {
					GetEAa;
					XMM_Reg xmmdst;
					SSE_MOVUPS(xmmdst,fpu.xmmreg[reg]);
					SaveMq(eaa,xmmdst.u64[0]);
					SaveMq(eaa+8u,xmmdst.u64[1]);
				}
				break;
			case MP_F3:									/* F3 0F 11 MOVSS r/m, reg */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII || !CPU_SSE()) goto illegal_opcode;
				if (rm >= 0xc0) {
					SSE_MOVSS(fpu.xmmreg[rm & 7],fpu.xmmreg[reg]);
				} else {
					GetEAa;
					XMM_Reg xmmdst;
					SSE_MOVSS(xmmdst,fpu.xmmreg[reg]);
					SaveMd(eaa,xmmdst.u32[0]);
				}
				break;
			default:
				goto illegal_opcode;
		};
		break;
	}
	CASE_0F_MMX(0x12)
	{
		GetRM;
		const unsigned char reg = (rm >> 3) & 7;

		switch (last_prefix) {
			case MP_NONE:									/* 0F 12 MOVHLPS reg, r/m */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII || !CPU_SSE()) goto illegal_opcode;
				if (rm >= 0xc0) {
					SSE_MOVHLPS(fpu.xmmreg[reg],fpu.xmmreg[rm & 7]);
				} else {								/* 0F 12 MOVLPS reg, r/m */
					GetEAa;
					XMM_Reg xmmsrc;
					xmmsrc.u64[0] = LoadMq(eaa);
					SSE_MOVLPS(fpu.xmmreg[reg],xmmsrc);
				}
				break;
			default:
				goto illegal_opcode;
		};
		break;
	}
	CASE_0F_MMX(0x13)
	{
		GetRM;
		const unsigned char reg = (rm >> 3) & 7;

		switch (last_prefix) {
			case MP_NONE:									/* 0F 13 MOVLPS r/m, reg */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII || !CPU_SSE()) goto illegal_opcode;
				if (rm >= 0xc0) {
					goto illegal_opcode;
				} else {
					GetEAa;
					XMM_Reg xmmdst;
					SSE_MOVLPS(xmmdst,fpu.xmmreg[reg]);
					SaveMq(eaa,xmmdst.u64[0]);
				}
				break;
			default:
				goto illegal_opcode;
		};
		break;
	}
	CASE_0F_MMX(0x14)
	{
		GetRM;
		const unsigned char reg = (rm >> 3) & 7;

		switch (last_prefix) {
			case MP_NONE:									/* 0F 14 UNPCKLPS reg, r/m */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII || !CPU_SSE()) goto illegal_opcode;
				if (rm >= 0xc0) {
					SSE_UNPCKLPS(fpu.xmmreg[reg],fpu.xmmreg[rm & 7]);
				} else {
					GetEAa;
					XMM_Reg xmmsrc;
					xmmsrc.u64[0] = LoadMq(eaa); /* this takes from the LOWER quadword */
					SSE_UNPCKLPS(fpu.xmmreg[reg],xmmsrc);
				}
				break;
			default:
				goto illegal_opcode;
		};
		break;
	}
	CASE_0F_MMX(0x15)
	{
		GetRM;
		const unsigned char reg = (rm >> 3) & 7;

		switch (last_prefix) {
			case MP_NONE:									/* 0F 15 UNPCKHPS reg, r/m */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII || !CPU_SSE()) goto illegal_opcode;
				if (rm >= 0xc0) {
					SSE_UNPCKHPS(fpu.xmmreg[reg],fpu.xmmreg[rm & 7]);
				} else {
					GetEAa;
					XMM_Reg xmmsrc;
					xmmsrc.u64[1] = LoadMq(eaa+8); /* this takes from the UPPER quadword */
					SSE_UNPCKHPS(fpu.xmmreg[reg],xmmsrc);
				}
				break;
			default:
				goto illegal_opcode;
		};
		break;
	}
	CASE_0F_MMX(0x16)
	{
		GetRM;
		const unsigned char reg = (rm >> 3) & 7;

		switch (last_prefix) {
			case MP_NONE:									/* 0F 16 MOVLHPS reg, r/m */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII || !CPU_SSE()) goto illegal_opcode;
				if (rm >= 0xc0) {
					SSE_MOVLHPS(fpu.xmmreg[reg],fpu.xmmreg[rm & 7]);
				} else {								/* 0F 16 MOVHPS reg, r/m */
					GetEAa;
					XMM_Reg xmmsrc;
					xmmsrc.u64[0] = LoadMq(eaa);
					SSE_MOVHPS(fpu.xmmreg[reg],xmmsrc);
				}
				break;
			default:
				goto illegal_opcode;
		};
		break;
	}
	CASE_0F_MMX(0x17)
	{
		GetRM;
		const unsigned char reg = (rm >> 3) & 7;

		switch (last_prefix) {
			case MP_NONE:									/* 0F 17 MOVHPS r/m, reg */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII || !CPU_SSE()) goto illegal_opcode;
				if (rm >= 0xc0) {
					goto illegal_opcode;
				} else {
					GetEAa;
					XMM_Reg xmmdst;
					SSE_MOVHPS(xmmdst,fpu.xmmreg[reg]);
					SaveMq(eaa+8,xmmdst.u64[1]); /* modifies only upper 64 bits */
				}
				break;
			default:
				goto illegal_opcode;
		};
		break;
	}
	CASE_0F_MMX(0x18)
	{
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PPROSLOW) goto illegal_opcode;
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII) break; /* hinting NOP */
		{
			GetRM;
			const unsigned char reg = (rm >> 3) & 7;

			switch (reg) {
				case 0:
					if (rm < 0xC0) /* 0F 18 /0 PREFETCHNTA <m> */
						{ GetEAa; } // do nothing, but GetEAa is required to fully decode the instruction
					else
						goto illegal_opcode;
					break;
				case 1:
					if (rm < 0xC0) /* 0F 18 /1 PREFETCH0 <m> */
						{ GetEAa; } // do nothing, but GetEAa is required to fully decode the instruction
					else
						goto illegal_opcode;
					break;
				case 2:
					if (rm < 0xC0) /* 0F 18 /2 PREFETCH1 <m> */
						{ GetEAa; } // do nothing, but GetEAa is required to fully decode the instruction
					else
						goto illegal_opcode;
					break;
				case 3:
					if (rm < 0xC0) /* 0F 18 /3 PREFETCH2 <m> */
						{ GetEAa; } // do nothing, but GetEAa is required to fully decode the instruction
					else
						goto illegal_opcode;
					break;
				default:
					goto illegal_opcode;
			}
		}
		break;
	}
	CASE_0F_MMX(0x28)
	{
		GetRM;
		const unsigned char reg = (rm >> 3) & 7;

		switch (last_prefix) {
			case MP_NONE:									/* 0F 28 MOVAPS reg, r/m */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII || !CPU_SSE()) goto illegal_opcode;
				if (rm >= 0xc0) {
					SSE_MOVAPS(fpu.xmmreg[reg],fpu.xmmreg[rm & 7]);
				} else {
					GetEAa;
					XMM_Reg xmmsrc;
					if (!SSE_REQUIRE_ALIGNMENT(eaa)) SSE_ALIGN_EXCEPTION();
					xmmsrc.u64[0] = LoadMq(eaa);
					xmmsrc.u64[1] = LoadMq(eaa+8u);
					SSE_MOVAPS(fpu.xmmreg[reg],xmmsrc);
				}
				break;
			default:
				goto illegal_opcode;
		};
		break;
	}
	CASE_0F_MMX(0x29)
	{
		GetRM;
		const unsigned char reg = (rm >> 3) & 7;

		switch (last_prefix) {
			case MP_NONE:									/* 0F 29 MOVAPS r/m, reg */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII || !CPU_SSE()) goto illegal_opcode;
				if (rm >= 0xc0) {
					SSE_MOVAPS(fpu.xmmreg[rm & 7],fpu.xmmreg[reg]);
				} else {
					GetEAa;
					XMM_Reg xmmdst;
					if (!SSE_REQUIRE_ALIGNMENT(eaa)) SSE_ALIGN_EXCEPTION();
					SSE_MOVAPS(xmmdst,fpu.xmmreg[reg]);
					SaveMq(eaa,xmmdst.u64[0]);
					SaveMq(eaa+8u,xmmdst.u64[1]);
				}
				break;
			default:
				goto illegal_opcode;
		};
		break;
	}
	CASE_0F_MMX(0x2A)
	{
		GetRM;
		const unsigned char reg = (rm >> 3) & 7;

		switch (last_prefix) {
			case MP_NONE:									/* 0F 2A CVTPI2PS reg, r/m */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII || !CPU_SSE()) goto illegal_opcode;
				if (rm >= 0xc0) {
					SSE_CVTPI2PS(fpu.xmmreg[reg],*reg_mmx[rm & 7]);
				} else {
					GetEAa;
					MMX_reg mmxsrc;
					mmxsrc.q = LoadMq(eaa);
					SSE_CVTPI2PS(fpu.xmmreg[reg],mmxsrc);
				}
				break;
			case MP_F3:									/* F3 0F 2A CVTSI2SS reg, r/m */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII || !CPU_SSE()) goto illegal_opcode;
				if (rm >= 0xc0) {
					SSE_CVTSI2SS(fpu.xmmreg[reg],cpu_regs.regs[rm & 7].dword[0]);
				} else {
					GetEAa;
					SSE_CVTSI2SS(fpu.xmmreg[reg],LoadMd(eaa));
				}
				break;
			default:
				goto illegal_opcode;
		};
		break;
	}
	CASE_0F_MMX(0x2B)
	{
		GetRM;
		const unsigned char reg = (rm >> 3) & 7;

		switch (last_prefix) {
			case MP_NONE:									/* 0F 2B MOVNTPS r/m, reg */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII || !CPU_SSE()) goto illegal_opcode;
				if (rm >= 0xc0) {
					goto illegal_opcode;
				} else {
					GetEAa;
					if (!SSE_REQUIRE_ALIGNMENT(eaa)) SSE_ALIGN_EXCEPTION();
					SaveMq(eaa,fpu.xmmreg[reg].u64[0]);
					SaveMq(eaa+8,fpu.xmmreg[reg].u64[1]);
				}
				break;
			default:
				goto illegal_opcode;
		};
		break;
	}
	CASE_0F_MMX(0x2C)
	{
		GetRM;
		const unsigned char reg = (rm >> 3) & 7;

		switch (last_prefix) {
			case MP_NONE:									/* 0F 2C CVTTPS2PI reg, r/m */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII || !CPU_SSE()) goto illegal_opcode;
				if (rm >= 0xc0) {
					SSE_CVTTPS2PI(*reg_mmx[reg],fpu.xmmreg[rm & 7]);
				} else {
					GetEAa;
					XMM_Reg xmmsrc;
					xmmsrc.u64[0] = LoadMq(eaa);
					SSE_CVTTPS2PI(*reg_mmx[reg],xmmsrc);
				}
				break;
			case MP_F3:									/* F3 0F 2C CVTTSS2SI reg, r/m */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII || !CPU_SSE()) goto illegal_opcode;
				if (rm >= 0xc0) {
					SSE_CVTTSS2SI(cpu_regs.regs[reg].dword[0],fpu.xmmreg[rm & 7]);
				} else {
					GetEAa;
					XMM_Reg xmmsrc;
					xmmsrc.u32[0] = LoadMd(eaa);
					SSE_CVTTSS2SI(cpu_regs.regs[reg].dword[0],xmmsrc);
				}
				break;
			default:
				goto illegal_opcode;
		};
		break;
	}
	CASE_0F_MMX(0x2D)
	{
		GetRM;
		const unsigned char reg = (rm >> 3) & 7;

		switch (last_prefix) {
			case MP_NONE:									/* 0F 2D CVTPS2PI reg, r/m */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII || !CPU_SSE()) goto illegal_opcode;
				if (rm >= 0xc0) {
					SSE_CVTPS2PI(*reg_mmx[reg],fpu.xmmreg[rm & 7]);
				} else {
					GetEAa;
					XMM_Reg xmmsrc;
					xmmsrc.u64[0] = LoadMq(eaa);
					SSE_CVTPS2PI(*reg_mmx[reg],xmmsrc);
				}
				break;
			case MP_F3:									/* F3 0F 2D CVTSS2SI reg, r/m */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII || !CPU_SSE()) goto illegal_opcode;
				if (rm >= 0xc0) {
					SSE_CVTSS2SI(cpu_regs.regs[reg].dword[0],fpu.xmmreg[rm & 7]);
				} else {
					GetEAa;
					XMM_Reg xmmsrc;
					xmmsrc.u32[0] = LoadMd(eaa);
					SSE_CVTSS2SI(cpu_regs.regs[reg].dword[0],xmmsrc);
				}
				break;
			default:
				goto illegal_opcode;
		};
		break;
	}
	CASE_0F_MMX(0x2E)
	{
		GetRM;
		const unsigned char reg = (rm >> 3) & 7;

		switch (last_prefix) {
			case MP_NONE:									/* 0F 2E UCOMISS reg, r/m */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII || !CPU_SSE()) goto illegal_opcode;
				if (rm >= 0xc0) {
					SSE_UCOMISS(fpu.xmmreg[reg],fpu.xmmreg[rm & 7]);
				} else {
					GetEAa;
					XMM_Reg xmmsrc;
					xmmsrc.u32[0] = LoadMd(eaa);
					SSE_UCOMISS(fpu.xmmreg[reg],xmmsrc);
				}
				break;
			default:
				goto illegal_opcode;
		};
		break;
	}
	CASE_0F_MMX(0x2F)
	{
		GetRM;
		const unsigned char reg = (rm >> 3) & 7;

		switch (last_prefix) {
			case MP_NONE:									/* 0F 2F COMISS reg, r/m */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII || !CPU_SSE()) goto illegal_opcode;
				if (rm >= 0xc0) {
					SSE_COMISS(fpu.xmmreg[reg],fpu.xmmreg[rm & 7]);
				} else {
					GetEAa;
					XMM_Reg xmmsrc;
					xmmsrc.u32[0] = LoadMd(eaa);
					SSE_COMISS(fpu.xmmreg[reg],xmmsrc);
				}
				break;
			default:
				goto illegal_opcode;
		};
		break;
	}
	CASE_0F_MMX(0x50)
	{
		GetRM;
		const unsigned char reg = (rm >> 3) & 7;

		switch (last_prefix) {
			case MP_NONE:									/* 0F 50 MOVMSKPS reg, r/m */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII || !CPU_SSE()) goto illegal_opcode;
				if (rm >= 0xc0)
					SSE_MOVMSKPS(cpu_regs.regs[reg].dword[0],fpu.xmmreg[rm & 7]);
				else
					goto illegal_opcode;
				break;
			default:
				goto illegal_opcode;
		};
		break;
	}
	CASE_0F_MMX(0x51)
	{
		GetRM;
		const unsigned char reg = (rm >> 3) & 7;

		switch (last_prefix) {
			case MP_NONE:									/* 0F 51 SQRTPS reg, r/m */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII || !CPU_SSE()) goto illegal_opcode;
				if (rm >= 0xc0) {
					SSE_SQRTPS(fpu.xmmreg[reg],fpu.xmmreg[rm & 7]);
				} else {
					GetEAa;
					XMM_Reg xmmsrc;
					if (!SSE_REQUIRE_ALIGNMENT(eaa)) SSE_ALIGN_EXCEPTION();
					xmmsrc.u64[0] = LoadMq(eaa);
					xmmsrc.u64[1] = LoadMq(eaa+8u);
					SSE_SQRTPS(fpu.xmmreg[reg],xmmsrc);
				}
				break;
			case MP_F3:									/* F3 0F 51 SQRTSS reg, r/m */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII || !CPU_SSE()) goto illegal_opcode;
				if (rm >= 0xc0) {
					SSE_SQRTSS(fpu.xmmreg[reg],fpu.xmmreg[rm & 7]);
				} else {
					GetEAa;
					XMM_Reg xmmsrc;
					if (!SSE_REQUIRE_ALIGNMENT(eaa)) SSE_ALIGN_EXCEPTION();
					xmmsrc.u32[0] = LoadMd(eaa);
					SSE_SQRTSS(fpu.xmmreg[reg],xmmsrc);
				}
				break;
			default:
				goto illegal_opcode;
		};
		break;
	}
	CASE_0F_MMX(0x52)
	{
		GetRM;
		const unsigned char reg = (rm >> 3) & 7;

		switch (last_prefix) {
			case MP_NONE:									/* 0F 52 RSQRTPS reg, r/m */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII || !CPU_SSE()) goto illegal_opcode;
				if (rm >= 0xc0) {
					SSE_RSQRTPS(fpu.xmmreg[reg],fpu.xmmreg[rm & 7]);
				} else {
					GetEAa;
					XMM_Reg xmmsrc;
					if (!SSE_REQUIRE_ALIGNMENT(eaa)) SSE_ALIGN_EXCEPTION();
					xmmsrc.u64[0] = LoadMq(eaa);
					xmmsrc.u64[1] = LoadMq(eaa+8u);
					SSE_RSQRTPS(fpu.xmmreg[reg],xmmsrc);
				}
				break;
			case MP_F3:									/* F3 0F 52 RSQRTSS reg, r/m */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII || !CPU_SSE()) goto illegal_opcode;
				if (rm >= 0xc0) {
					SSE_RSQRTSS(fpu.xmmreg[reg],fpu.xmmreg[rm & 7]);
				} else {
					GetEAa;
					XMM_Reg xmmsrc;
					if (!SSE_REQUIRE_ALIGNMENT(eaa)) SSE_ALIGN_EXCEPTION();
					xmmsrc.u32[0] = LoadMd(eaa);
					SSE_RSQRTSS(fpu.xmmreg[reg],xmmsrc);
				}
				break;
			default:
				goto illegal_opcode;
		};
		break;
	}
	CASE_0F_MMX(0x53)
	{
		GetRM;
		const unsigned char reg = (rm >> 3) & 7;

		switch (last_prefix) {
			case MP_NONE:									/* 0F 53 RCPPS reg, r/m */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII || !CPU_SSE()) goto illegal_opcode;
				if (rm >= 0xc0) {
					SSE_RCPPS(fpu.xmmreg[reg],fpu.xmmreg[rm & 7]);
				} else {
					GetEAa;
					XMM_Reg xmmsrc;
					if (!SSE_REQUIRE_ALIGNMENT(eaa)) SSE_ALIGN_EXCEPTION();
					xmmsrc.u64[0] = LoadMq(eaa);
					xmmsrc.u64[1] = LoadMq(eaa+8u);
					SSE_RCPPS(fpu.xmmreg[reg],xmmsrc);
				}
				break;
			case MP_F3:									/* F3 0F 53 RCPSS reg, r/m */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII || !CPU_SSE()) goto illegal_opcode;
				if (rm >= 0xc0) {
					SSE_RCPSS(fpu.xmmreg[reg],fpu.xmmreg[rm & 7]);
				} else {
					GetEAa;
					XMM_Reg xmmsrc;
					if (!SSE_REQUIRE_ALIGNMENT(eaa)) SSE_ALIGN_EXCEPTION();
					xmmsrc.u32[0] = LoadMd(eaa);
					SSE_RCPSS(fpu.xmmreg[reg],xmmsrc);
				}
				break;
			default:
				goto illegal_opcode;
		};
		break;
	}
	CASE_0F_MMX(0x54)
	{
		GetRM;
		const unsigned char reg = (rm >> 3) & 7;

		switch (last_prefix) {
			case MP_NONE:									/* 0F 54 ANDPS reg, r/m */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII || !CPU_SSE()) goto illegal_opcode;
				if (rm >= 0xc0) {
					SSE_ANDPS(fpu.xmmreg[reg],fpu.xmmreg[rm & 7]);
				} else {
					GetEAa;
					XMM_Reg xmmsrc;
					if (!SSE_REQUIRE_ALIGNMENT(eaa)) SSE_ALIGN_EXCEPTION();
					xmmsrc.u64[0] = LoadMq(eaa);
					xmmsrc.u64[1] = LoadMq(eaa+8u);
					SSE_ANDPS(fpu.xmmreg[reg],xmmsrc);
				}
				break;
			default:
				goto illegal_opcode;
		};
		break;
	}
	CASE_0F_MMX(0x55)
	{
		GetRM;
		const unsigned char reg = (rm >> 3) & 7;

		switch (last_prefix) {
			case MP_NONE:									/* 0F 55 ANDNPS reg, r/m */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII || !CPU_SSE()) goto illegal_opcode;
				if (rm >= 0xc0) {
					SSE_ANDNPS(fpu.xmmreg[reg],fpu.xmmreg[rm & 7]);
				} else {
					GetEAa;
					XMM_Reg xmmsrc;
					if (!SSE_REQUIRE_ALIGNMENT(eaa)) SSE_ALIGN_EXCEPTION();
					xmmsrc.u64[0] = LoadMq(eaa);
					xmmsrc.u64[1] = LoadMq(eaa+8u);
					SSE_ANDNPS(fpu.xmmreg[reg],xmmsrc);
				}
				break;
			default:
				goto illegal_opcode;
		};
		break;
	}
	CASE_0F_MMX(0x56)
	{
		GetRM;
		const unsigned char reg = (rm >> 3) & 7;

		switch (last_prefix) {
			case MP_NONE:									/* 0F 56 ORPS reg, r/m */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII || !CPU_SSE()) goto illegal_opcode;
				if (rm >= 0xc0) {
					SSE_ORPS(fpu.xmmreg[reg],fpu.xmmreg[rm & 7]);
				} else {
					GetEAa;
					XMM_Reg xmmsrc;
					if (!SSE_REQUIRE_ALIGNMENT(eaa)) SSE_ALIGN_EXCEPTION();
					xmmsrc.u64[0] = LoadMq(eaa);
					xmmsrc.u64[1] = LoadMq(eaa+8u);
					SSE_ORPS(fpu.xmmreg[reg],xmmsrc);
				}
				break;
			default:
				goto illegal_opcode;
		};
		break;
	}
	CASE_0F_MMX(0x57)
	{
		GetRM;
		const unsigned char reg = (rm >> 3) & 7;

		switch (last_prefix) {
			case MP_NONE:									/* 0F 57 XORPS reg, r/m */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII || !CPU_SSE()) goto illegal_opcode;
				if (rm >= 0xc0) {
					SSE_XORPS(fpu.xmmreg[reg],fpu.xmmreg[rm & 7]);
				} else {
					GetEAa;
					XMM_Reg xmmsrc;
					if (!SSE_REQUIRE_ALIGNMENT(eaa)) SSE_ALIGN_EXCEPTION();
					xmmsrc.u64[0] = LoadMq(eaa);
					xmmsrc.u64[1] = LoadMq(eaa+8u);
					SSE_XORPS(fpu.xmmreg[reg],xmmsrc);
				}
				break;
			default:
				goto illegal_opcode;
		};
		break;
	}
	CASE_0F_MMX(0x58)
	{
		GetRM;
		const unsigned char reg = (rm >> 3) & 7;

		switch (last_prefix) {
			case MP_NONE:									/* 0F 58 ADDPS reg, r/m */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII || !CPU_SSE()) goto illegal_opcode;
				if (rm >= 0xc0) {
					SSE_ADDPS(fpu.xmmreg[reg],fpu.xmmreg[rm & 7]);
				} else {
					GetEAa;
					XMM_Reg xmmsrc;
					if (!SSE_REQUIRE_ALIGNMENT(eaa)) SSE_ALIGN_EXCEPTION();
					xmmsrc.u64[0] = LoadMq(eaa);
					xmmsrc.u64[1] = LoadMq(eaa+8u);
					SSE_ADDPS(fpu.xmmreg[reg],xmmsrc);
				}
				break;
			case MP_F3:									/* F3 0F 58 ADDSS reg, r/m */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII || !CPU_SSE()) goto illegal_opcode;
				if (rm >= 0xc0) {
					SSE_ADDSS(fpu.xmmreg[reg],fpu.xmmreg[rm & 7]);
				} else {
					GetEAa;
					XMM_Reg xmmsrc;
					if (!SSE_REQUIRE_ALIGNMENT(eaa)) SSE_ALIGN_EXCEPTION();
					xmmsrc.u64[0] = LoadMq(eaa);
					xmmsrc.u64[1] = LoadMq(eaa+8u);
					SSE_ADDSS(fpu.xmmreg[reg],xmmsrc);
				}
				break;
			default:
				goto illegal_opcode;
		};
		break;
	}
	CASE_0F_MMX(0x59)
	{
		GetRM;
		const unsigned char reg = (rm >> 3) & 7;

		switch (last_prefix) {
			case MP_NONE:									/* 0F 59 MULPS reg, r/m */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII || !CPU_SSE()) goto illegal_opcode;
				if (rm >= 0xc0) {
					SSE_MULPS(fpu.xmmreg[reg],fpu.xmmreg[rm & 7]);
				} else {
					GetEAa;
					XMM_Reg xmmsrc;
					if (!SSE_REQUIRE_ALIGNMENT(eaa)) SSE_ALIGN_EXCEPTION();
					xmmsrc.u64[0] = LoadMq(eaa);
					xmmsrc.u64[1] = LoadMq(eaa+8u);
					SSE_MULPS(fpu.xmmreg[reg],xmmsrc);
				}
				break;
			case MP_F3:									/* F3 0F 59 MULSS reg, r/m */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII || !CPU_SSE()) goto illegal_opcode;
				if (rm >= 0xc0) {
					SSE_MULSS(fpu.xmmreg[reg],fpu.xmmreg[rm & 7]);
				} else {
					GetEAa;
					XMM_Reg xmmsrc;
					if (!SSE_REQUIRE_ALIGNMENT(eaa)) SSE_ALIGN_EXCEPTION();
					xmmsrc.u32[0] = LoadMd(eaa);
					SSE_MULSS(fpu.xmmreg[reg],xmmsrc);
				}
				break;
			default:
				goto illegal_opcode;
		};
		break;
	}
	CASE_0F_MMX(0x5C)
	{
		GetRM;
		const unsigned char reg = (rm >> 3) & 7;

		switch (last_prefix) {
			case MP_NONE:									/* 0F 5C SUBPS reg, r/m */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII || !CPU_SSE()) goto illegal_opcode;
				if (rm >= 0xc0) {
					SSE_SUBPS(fpu.xmmreg[reg],fpu.xmmreg[rm & 7]);
				} else {
					GetEAa;
					XMM_Reg xmmsrc;
					if (!SSE_REQUIRE_ALIGNMENT(eaa)) SSE_ALIGN_EXCEPTION();
					xmmsrc.u64[0] = LoadMq(eaa);
					xmmsrc.u64[1] = LoadMq(eaa+8u);
					SSE_SUBPS(fpu.xmmreg[reg],xmmsrc);
				}
				break;
			case MP_F3:									/* F3 0F 5C SUBSS reg, r/m */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII || !CPU_SSE()) goto illegal_opcode;
				if (rm >= 0xc0) {
					SSE_SUBSS(fpu.xmmreg[reg],fpu.xmmreg[rm & 7]);
				} else {
					GetEAa;
					XMM_Reg xmmsrc;
					if (!SSE_REQUIRE_ALIGNMENT(eaa)) SSE_ALIGN_EXCEPTION();
					xmmsrc.u32[0] = LoadMd(eaa);
					SSE_SUBSS(fpu.xmmreg[reg],xmmsrc);
				}
				break;
			default:
				goto illegal_opcode;
		};
		break;
	}
	CASE_0F_MMX(0x5D)
	{
		GetRM;
		const unsigned char reg = (rm >> 3) & 7;

		switch (last_prefix) {
			case MP_NONE:									/* 0F 5D MINPS reg, r/m */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII || !CPU_SSE()) goto illegal_opcode;
				if (rm >= 0xc0) {
					SSE_MINPS(fpu.xmmreg[reg],fpu.xmmreg[rm & 7]);
				} else {
					GetEAa;
					XMM_Reg xmmsrc;
					if (!SSE_REQUIRE_ALIGNMENT(eaa)) SSE_ALIGN_EXCEPTION();
					xmmsrc.u64[0] = LoadMq(eaa);
					xmmsrc.u64[1] = LoadMq(eaa+8u);
					SSE_MINPS(fpu.xmmreg[reg],xmmsrc);
				}
				break;
			case MP_F3:									/* F3 0F 5D MINSS reg, r/m */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII || !CPU_SSE()) goto illegal_opcode;
				if (rm >= 0xc0) {
					SSE_MINSS(fpu.xmmreg[reg],fpu.xmmreg[rm & 7]);
				} else {
					GetEAa;
					XMM_Reg xmmsrc;
					if (!SSE_REQUIRE_ALIGNMENT(eaa)) SSE_ALIGN_EXCEPTION();
					xmmsrc.u32[0] = LoadMd(eaa);
					SSE_MINSS(fpu.xmmreg[reg],xmmsrc);
				}
				break;
			default:
				goto illegal_opcode;
		};
		break;
	}
	CASE_0F_MMX(0x5E)
	{
		GetRM;
		const unsigned char reg = (rm >> 3) & 7;

		switch (last_prefix) {
			case MP_NONE:									/* 0F 5E DIVPS reg, r/m */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII || !CPU_SSE()) goto illegal_opcode;
				if (rm >= 0xc0) {
					SSE_DIVPS(fpu.xmmreg[reg],fpu.xmmreg[rm & 7]);
				} else {
					GetEAa;
					XMM_Reg xmmsrc;
					if (!SSE_REQUIRE_ALIGNMENT(eaa)) SSE_ALIGN_EXCEPTION();
					xmmsrc.u64[0] = LoadMq(eaa);
					xmmsrc.u64[1] = LoadMq(eaa+8u);
					SSE_DIVPS(fpu.xmmreg[reg],xmmsrc);
				}
				break;
			case MP_F3:									/* F3 0F 5E DIVSS reg, r/m */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII || !CPU_SSE()) goto illegal_opcode;
				if (rm >= 0xc0) {
					SSE_DIVSS(fpu.xmmreg[reg],fpu.xmmreg[rm & 7]);
				} else {
					GetEAa;
					XMM_Reg xmmsrc;
					if (!SSE_REQUIRE_ALIGNMENT(eaa)) SSE_ALIGN_EXCEPTION();
					xmmsrc.u32[0] = LoadMd(eaa);
					SSE_DIVSS(fpu.xmmreg[reg],xmmsrc);
				}
				break;
			default:
				goto illegal_opcode;
		};
		break;
	}
	CASE_0F_MMX(0x5F)
	{
		GetRM;
		const unsigned char reg = (rm >> 3) & 7;

		switch (last_prefix) {
			case MP_NONE:									/* 0F 5F MAXPS reg, r/m */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII || !CPU_SSE()) goto illegal_opcode;
				if (rm >= 0xc0) {
					SSE_MAXPS(fpu.xmmreg[reg],fpu.xmmreg[rm & 7]);
				} else {
					GetEAa;
					XMM_Reg xmmsrc;
					if (!SSE_REQUIRE_ALIGNMENT(eaa)) SSE_ALIGN_EXCEPTION();
					xmmsrc.u64[0] = LoadMq(eaa);
					xmmsrc.u64[1] = LoadMq(eaa+8u);
					SSE_MAXPS(fpu.xmmreg[reg],xmmsrc);
				}
				break;
			case MP_F3:									/* F3 0F 5F MAXSS reg, r/m */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII || !CPU_SSE()) goto illegal_opcode;
				if (rm >= 0xc0) {
					SSE_MAXSS(fpu.xmmreg[reg],fpu.xmmreg[rm & 7]);
				} else {
					GetEAa;
					XMM_Reg xmmsrc;
					if (!SSE_REQUIRE_ALIGNMENT(eaa)) SSE_ALIGN_EXCEPTION();
					xmmsrc.u32[0] = LoadMd(eaa);
					SSE_MAXSS(fpu.xmmreg[reg],xmmsrc);
				}
				break;
			default:
				goto illegal_opcode;
		};
		break;
	}
	CASE_0F_MMX(0x60)												/* PUNPCKLBW Pq,Qq */
	{
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PMMXSLOW) goto illegal_opcode;
		GetRM;
		MMX_reg* dest=lookupRMregMM[rm];
		MMX_reg src;
		if (rm>=0xc0) {
			src.q=reg_mmx[rm&7]->q;
		} else {
			GetEAa;
			src.q = LoadMq(eaa);
		}
		dest->ub.b7 = src.ub.b3;
		dest->ub.b6 = dest->ub.b3;
		dest->ub.b5 = src.ub.b2;
		dest->ub.b4 = dest->ub.b2;
		dest->ub.b3 = src.ub.b1;
		dest->ub.b2 = dest->ub.b1;
		dest->ub.b1 = src.ub.b0;
		dest->ub.b0 = dest->ub.b0;
		break;
	}
	CASE_0F_MMX(0x61)												/* PUNPCKLWD Pq,Qq */
	{
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PMMXSLOW) goto illegal_opcode;
		GetRM;
		MMX_reg* dest=lookupRMregMM[rm];
		MMX_reg src;
		if (rm>=0xc0) {
			src.q=reg_mmx[rm&7]->q;
		} else {
			GetEAa;
			src.q = LoadMq(eaa);
		}
		dest->uw.w3 = src.uw.w1;
		dest->uw.w2 = dest->uw.w1;
		dest->uw.w1 = src.uw.w0;
		dest->uw.w0 = dest->uw.w0;
		break;
	}
	CASE_0F_MMX(0x62)												/* PUNPCKLDQ Pq,Qq */
	{
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PMMXSLOW) goto illegal_opcode;
		GetRM;
		MMX_reg* dest=lookupRMregMM[rm];
		MMX_reg src;
		if (rm>=0xc0) {
			src.q=reg_mmx[rm&7]->q;
		} else {
			GetEAa;
			src.q = LoadMq(eaa);
		}
		dest->ud.d1 = src.ud.d0;
		break;
	}
	CASE_0F_MMX(0x63)												/* PACKSSWB Pq,Qq */
	{
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PMMXSLOW) goto illegal_opcode;
		GetRM;
		MMX_reg* dest=lookupRMregMM[rm];
		MMX_reg src;
		if (rm>=0xc0) {
			src.q=reg_mmx[rm&7]->q;
		} else {
			GetEAa;
			src.q = LoadMq(eaa);
		}
		dest->sb.b0 = SaturateWordSToByteS(dest->sw.w0);
		dest->sb.b1 = SaturateWordSToByteS(dest->sw.w1);
		dest->sb.b2 = SaturateWordSToByteS(dest->sw.w2);
		dest->sb.b3 = SaturateWordSToByteS(dest->sw.w3);
		dest->sb.b4 = SaturateWordSToByteS(src.sw.w0);
		dest->sb.b5 = SaturateWordSToByteS(src.sw.w1);
		dest->sb.b6 = SaturateWordSToByteS(src.sw.w2);
		dest->sb.b7 = SaturateWordSToByteS(src.sw.w3);
		break;
	}
	CASE_0F_MMX(0x64)												/* PCMPGTB Pq,Qq */
	{
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PMMXSLOW) goto illegal_opcode;
		GetRM;
		MMX_reg* dest=lookupRMregMM[rm];
		MMX_reg src;
		if (rm>=0xc0) {
			src.q=reg_mmx[rm&7]->q;
		} else {
			GetEAa;
			src.q = LoadMq(eaa);
		}
		dest->ub.b0 = dest->sb.b0>src.sb.b0?0xff:0;
		dest->ub.b1 = dest->sb.b1>src.sb.b1?0xff:0;
		dest->ub.b2 = dest->sb.b2>src.sb.b2?0xff:0;
		dest->ub.b3 = dest->sb.b3>src.sb.b3?0xff:0;
		dest->ub.b4 = dest->sb.b4>src.sb.b4?0xff:0;
		dest->ub.b5 = dest->sb.b5>src.sb.b5?0xff:0;
		dest->ub.b6 = dest->sb.b6>src.sb.b6?0xff:0;
		dest->ub.b7 = dest->sb.b7>src.sb.b7?0xff:0;
		break;
	}
	CASE_0F_MMX(0x65)												/* PCMPGTW Pq,Qq */
	{
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PMMXSLOW) goto illegal_opcode;
		GetRM;
		MMX_reg* dest=lookupRMregMM[rm];
		MMX_reg src;
		if (rm>=0xc0) {
			src.q=reg_mmx[rm&7]->q;
		} else {
			GetEAa;
			src.q = LoadMq(eaa);
		}
		dest->uw.w0 = dest->sw.w0>src.sw.w0?0xffff:0;
		dest->uw.w1 = dest->sw.w1>src.sw.w1?0xffff:0;
		dest->uw.w2 = dest->sw.w2>src.sw.w2?0xffff:0;
		dest->uw.w3 = dest->sw.w3>src.sw.w3?0xffff:0;
		break;
	}
	CASE_0F_MMX(0x66)												/* PCMPGTD Pq,Qq */
	{
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PMMXSLOW) goto illegal_opcode;
		GetRM;
		MMX_reg* dest=lookupRMregMM[rm];
		MMX_reg src;
		if (rm>=0xc0) {
			src.q=reg_mmx[rm&7]->q;
		} else {
			GetEAa;
			src.q = LoadMq(eaa);
		}
		dest->ud.d0 = dest->sd.d0>src.sd.d0?0xffffffff:0;
		dest->ud.d1 = dest->sd.d1>src.sd.d1?0xffffffff:0;
		break;
	}
	CASE_0F_MMX(0x67)												/* PACKUSWB Pq,Qq */
	{
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PMMXSLOW) goto illegal_opcode;
		GetRM;
		MMX_reg* dest=lookupRMregMM[rm];
		MMX_reg src;
		if (rm>=0xc0) {
			src.q=reg_mmx[rm&7]->q;
		} else {
			GetEAa;
			src.q = LoadMq(eaa);
		}
		dest->ub.b0 = SaturateWordSToByteU(dest->sw.w0);
		dest->ub.b1 = SaturateWordSToByteU(dest->sw.w1);
		dest->ub.b2 = SaturateWordSToByteU(dest->sw.w2);
		dest->ub.b3 = SaturateWordSToByteU(dest->sw.w3);
		dest->ub.b4 = SaturateWordSToByteU(src.sw.w0);
		dest->ub.b5 = SaturateWordSToByteU(src.sw.w1);
		dest->ub.b6 = SaturateWordSToByteU(src.sw.w2);
		dest->ub.b7 = SaturateWordSToByteU(src.sw.w3);
		break;
	}
	CASE_0F_MMX(0x68)												/* PUNPCKHBW Pq,Qq */
	{
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PMMXSLOW) goto illegal_opcode;
		GetRM;
		MMX_reg* dest=lookupRMregMM[rm];
		MMX_reg src;
		if (rm>=0xc0) {
			src.q=reg_mmx[rm&7]->q;
		} else {
			GetEAa;
			src.q = LoadMq(eaa);
		}
		dest->ub.b0 = dest->ub.b4;
		dest->ub.b1 = src.ub.b4;
		dest->ub.b2 = dest->ub.b5;
		dest->ub.b3 = src.ub.b5;
		dest->ub.b4 = dest->ub.b6;
		dest->ub.b5 = src.ub.b6;
		dest->ub.b6 = dest->ub.b7;
		dest->ub.b7 = src.ub.b7;
		break;
	}
	CASE_0F_MMX(0x69)												/* PUNPCKHWD Pq,Qq */
	{
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PMMXSLOW) goto illegal_opcode;
		GetRM;
		MMX_reg* dest=lookupRMregMM[rm];
		MMX_reg src;
		if (rm>=0xc0) {
			src.q=reg_mmx[rm&7]->q;
		} else {
			GetEAa;
			src.q = LoadMq(eaa);
		}
		dest->uw.w0 = dest->uw.w2;
		dest->uw.w1 = src.uw.w2;
		dest->uw.w2 = dest->uw.w3;
		dest->uw.w3 = src.uw.w3;
		break;
	}
	CASE_0F_MMX(0x6A)												/* PUNPCKHDQ Pq,Qq */
	{
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PMMXSLOW) goto illegal_opcode;
		GetRM;
		MMX_reg* dest=lookupRMregMM[rm];
		MMX_reg src;
		if (rm>=0xc0) {
			src.q=reg_mmx[rm&7]->q;
		} else {
			GetEAa;
			src.q = LoadMq(eaa);
		}
		dest->ud.d0 = dest->ud.d1;
		dest->ud.d1 = src.ud.d1;
		break;
	}
	CASE_0F_MMX(0x6B)												/* PACKSSDW Pq,Qq */
	{
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PMMXSLOW) goto illegal_opcode;
		GetRM;
		MMX_reg* dest=lookupRMregMM[rm];
		MMX_reg src;
		if (rm>=0xc0) {
			src.q=reg_mmx[rm&7]->q;
		} else {
			GetEAa;
			src.q = LoadMq(eaa);
		}
		dest->sw.w0 = SaturateDwordSToWordS(dest->sd.d0);
		dest->sw.w1 = SaturateDwordSToWordS(dest->sd.d1);
		dest->sw.w2 = SaturateDwordSToWordS(src.sd.d0);
		dest->sw.w3 = SaturateDwordSToWordS(src.sd.d1);
		break;
	}
	CASE_0F_MMX(0x6e)												/* MOVD Pq,Ed */
	{
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PMMXSLOW) goto illegal_opcode;
		GetRM;
		MMX_reg* rmrq=lookupRMregMM[rm];
		if (rm>=0xc0) {
			GetEArd;
			rmrq->ud.d0=*(uint32_t*)eard;
			rmrq->ud.d1=0;
		} else {
			GetEAa;
			rmrq->ud.d0=LoadMd(eaa);
			rmrq->ud.d1=0;
		}
		break;
	}
	CASE_0F_MMX(0x6f)												/* MOVQ Pq,Qq */
	{
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PMMXSLOW) goto illegal_opcode;
		GetRM;
		MMX_reg* dest=lookupRMregMM[rm];
		if (rm>=0xc0) {
			MMX_reg* src=reg_mmx[rm&7];
			dest->q = src->q;
		} else {
			GetEAa;
			dest->q=LoadMq(eaa);
		}
		break;
	}
	CASE_0F_MMX(0x70)												/* PSHUFW Pq,Qq,imm8 */
	{
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PMMXSLOW) goto illegal_opcode;
		GetRM;
		uint8_t imm8 = Fetchb();
		MMX_reg* dest=lookupRMregMM[rm];
		MMX_reg src;
		if (rm>=0xc0) {
			src.q=reg_mmx[rm&7]->q;
		} else {
			GetEAa;
			src.q = LoadMq(eaa);
		}
		dest->uw.w0 = src.uwa[ imm8     &3u]; /* uwa[0] is uw.w0, see MMX_reg union */
		dest->uw.w1 = src.uwa[(imm8>>2u)&3u];
		dest->uw.w2 = src.uwa[(imm8>>4u)&3u];
		dest->uw.w3 = src.uwa[(imm8>>6u)&3u];
		break;
	}
	CASE_0F_MMX(0x71)												/* PSLLW/PSRLW/PSRAW Pq,Ib */
	{
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PMMXSLOW) goto illegal_opcode;
		GetRM;
		uint8_t op=(rm>>3)&7;
		uint8_t shift=(uint8_t)Fetchb();
		MMX_reg* dest=reg_mmx[rm&7];
		switch (op) {
			case 0x06: /*PSLLW*/
				if (shift > 15) dest->q = 0;
				else {
					dest->uw.w0 <<= shift;
					dest->uw.w1 <<= shift;
					dest->uw.w2 <<= shift;
					dest->uw.w3 <<= shift;
				}
				break;
			case 0x02: /*PSRLW*/
				if (shift > 15) dest->q = 0;
				else {
					dest->uw.w0 >>= shift;
					dest->uw.w1 >>= shift;
					dest->uw.w2 >>= shift;
					dest->uw.w3 >>= shift;
				}
				break;
			case 0x04: /*PSRAW*/
				MMX_reg tmp;
				if (!shift) break;
				tmp.q = dest->q;
				if (shift > 15) {
					dest->uw.w0 = (tmp.uw.w0&0x8000)?0xffff:0;
					dest->uw.w1 = (tmp.uw.w1&0x8000)?0xffff:0;
					dest->uw.w2 = (tmp.uw.w2&0x8000)?0xffff:0;
					dest->uw.w3 = (tmp.uw.w3&0x8000)?0xffff:0;
				} else {
					dest->sw.w0 >>= (int16_t)shift;
					dest->sw.w1 >>= (int16_t)shift;
					dest->sw.w2 >>= (int16_t)shift;
					dest->sw.w3 >>= (int16_t)shift;
				}
				break;
			default:
				goto illegal_opcode;
		}
		break;
	}
	CASE_0F_MMX(0x72)												/* PSLLD/PSRLD/PSRAD Pq,Ib */
	{
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PMMXSLOW) goto illegal_opcode;
		GetRM;
		uint8_t op=(rm>>3)&7;
		uint8_t shift=(uint8_t)Fetchb();
		MMX_reg* dest=reg_mmx[rm&7];
		switch (op) {
			case 0x06: /*PSLLD*/
				if (shift > 31) dest->q = 0;
				else {
					dest->ud.d0 <<= shift;
					dest->ud.d1 <<= shift;
				}
				break;
			case 0x02: /*PSRLD*/
				if (shift > 31) dest->q = 0;
				else {
					dest->ud.d0 >>= shift;
					dest->ud.d1 >>= shift;
				}
				break;
			case 0x04: /*PSRAD*/
				MMX_reg tmp;
				if (!shift) break;
				tmp.q = dest->q;
				if (shift > 31) { 
					dest->ud.d0 = (tmp.ud.d0&0x80000000)?0xffffffff:0;
					dest->ud.d1 = (tmp.ud.d1&0x80000000)?0xffffffff:0;
				} else {
					dest->sd.d0 >>= (int32_t)shift;
					dest->sd.d1 >>= (int32_t)shift;
				}
				break;
			default:
				goto illegal_opcode;
		}
		break;
	}
	CASE_0F_MMX(0x73)												/* PSLLQ/PSRLQ Pq,Ib */
	{
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PMMXSLOW) goto illegal_opcode;
		GetRM;
		uint8_t op=(rm>>3)&7;
		uint8_t shift=(uint8_t)Fetchb();
		MMX_reg* dest=reg_mmx[rm&7];
		switch (op) {
			case 0x06: /*PSLLQ*/
				if (shift > 63) dest->q = 0;
				else dest->q <<= (uint64_t)shift;
				break;
			case 0x02: /*PSRLQ*/
				if (shift > 63) dest->q = 0;
				else dest->q >>= (uint64_t)shift;
				break;
			default:
				goto illegal_opcode;
		}
		break;
	}
	CASE_0F_MMX(0x74)												/* PCMPEQB Pq,Qq */
	{
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PMMXSLOW) goto illegal_opcode;
		GetRM;
		MMX_reg* dest=lookupRMregMM[rm];
		MMX_reg src;
		if (rm>=0xc0) {
			src.q=reg_mmx[rm&7]->q;
		} else {
			GetEAa;
			src.q = LoadMq(eaa);
		}
		dest->ub.b0 = dest->ub.b0==src.ub.b0?0xff:0;
		dest->ub.b1 = dest->ub.b1==src.ub.b1?0xff:0;
		dest->ub.b2 = dest->ub.b2==src.ub.b2?0xff:0;
		dest->ub.b3 = dest->ub.b3==src.ub.b3?0xff:0;
		dest->ub.b4 = dest->ub.b4==src.ub.b4?0xff:0;
		dest->ub.b5 = dest->ub.b5==src.ub.b5?0xff:0;
		dest->ub.b6 = dest->ub.b6==src.ub.b6?0xff:0;
		dest->ub.b7 = dest->ub.b7==src.ub.b7?0xff:0;
		break;
	}
	CASE_0F_MMX(0x75)												/* PCMPEQW Pq,Qq */
	{
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PMMXSLOW) goto illegal_opcode;
		GetRM;
		MMX_reg* dest=lookupRMregMM[rm];
		MMX_reg src;
		if (rm>=0xc0) {
			src.q=reg_mmx[rm&7]->q;
		} else {
			GetEAa;
			src.q = LoadMq(eaa);
		}
		dest->uw.w0 = dest->uw.w0==src.uw.w0?0xffff:0;
		dest->uw.w1 = dest->uw.w1==src.uw.w1?0xffff:0;
		dest->uw.w2 = dest->uw.w2==src.uw.w2?0xffff:0;
		dest->uw.w3 = dest->uw.w3==src.uw.w3?0xffff:0;
		break;
	}
	CASE_0F_MMX(0x76)												/* PCMPEQD Pq,Qq */
	{
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PMMXSLOW) goto illegal_opcode;
		GetRM;
		MMX_reg* dest=lookupRMregMM[rm];
		MMX_reg src;
		if (rm>=0xc0) {
			src.q=reg_mmx[rm&7]->q;
		} else {
			GetEAa;
			src.q = LoadMq(eaa);
		}
		dest->ud.d0 = dest->ud.d0==src.ud.d0?0xffffffff:0;
		dest->ud.d1 = dest->ud.d1==src.ud.d1?0xffffffff:0;
		break;
	}
	CASE_0F_MMX(0x77)												/* EMMS */
	{
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PMMXSLOW) goto illegal_opcode;
		setFPUTagEmpty();
		break;
	}
	CASE_0F_MMX(0x7e)												/* MOVD Ed,Pq */
	{
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PMMXSLOW) goto illegal_opcode;
		GetRM;
		MMX_reg* rmrq=lookupRMregMM[rm];
		if (rm>=0xc0) {
			GetEArd;
			*(uint32_t*)eard=rmrq->ud.d0;
		} else {
			GetEAa;
			SaveMd(eaa,rmrq->ud.d0);
		}
		break;
	}
	CASE_0F_MMX(0x7f)												/* MOVQ Qq,Pq */
	{
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PMMXSLOW) goto illegal_opcode;
		GetRM;
		MMX_reg* dest=lookupRMregMM[rm];
		if (rm>=0xc0) {
			MMX_reg* src=reg_mmx[rm&7];
			src->q = dest->q;
		} else {
			GetEAa;
			SaveMq(eaa,dest->q);
		}
		break;
	}
	CASE_0F_MMX(0xae)
	{
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII) goto illegal_opcode;
		{
			GetRM;
			const unsigned char reg = (rm >> 3) & 7;

			switch (reg) {
				case 0:
					if (rm < 0xC0) {							/* 0F AE /0 FXSAVE <m> */
						GetEAa;
						if (!SSE_REQUIRE_ALIGNMENT(eaa)) SSE_ALIGN_EXCEPTION();
						CPU_FXSAVE(eaa);
					}
					else {
						goto illegal_opcode;
					}
					break;
				case 1:
					if (rm < 0xC0) {							/* 0F AE /1 FXRSTOR <m> */
						GetEAa;
						if (!SSE_REQUIRE_ALIGNMENT(eaa)) SSE_ALIGN_EXCEPTION();
						CPU_FXRSTOR(eaa);
					}
					else {
						goto illegal_opcode;
					}
					break;
				case 2:
					if (rm < 0xC0) {							/* 0F AE /2 LDMXCSR <m> */
						GetEAa;
						if (!CPU_LDMXCSR(eaa)) EXCEPTION(EXCEPTION_GP);
					}
					else {
						goto illegal_opcode;
					}
					break;
				case 3:
					if (rm < 0xC0) {							/* 0F AE /3 STMXCSR <m> */
						GetEAa;
						if (!CPU_STMXCSR(eaa)) EXCEPTION(EXCEPTION_GP);
					}
					else {
						goto illegal_opcode;
					}
					break;
				case 7:
					if (rm >= 0xC0) {							/* 0F AE /7 SFENCE <not m> */
						// DO NOTHING
					}
					else {
						goto illegal_opcode;
					}
					break;
				default:
					goto illegal_opcode;
			};
			break;
		}
	}
	CASE_0F_MMX(0xc2)
	{
		GetRM;
		uint8_t cf;
		const unsigned char reg = (rm >> 3) & 7;

		switch (last_prefix) {
			case MP_NONE:									/* 0F C2 CMPPS reg, r/m, imm8 */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII || !CPU_SSE()) goto illegal_opcode;
				if (rm >= 0xc0) {
					/* FIXME: Documentation says these are "reserved", doesn't say it causes a #UD, what really happens? */
					if ((cf=Fetchb()) > 7) goto illegal_opcode;
					SSE_CMPPS(fpu.xmmreg[reg],fpu.xmmreg[rm & 7],cf);
				} else {
					GetEAa;
					XMM_Reg xmmsrc;
					if (!SSE_REQUIRE_ALIGNMENT(eaa)) SSE_ALIGN_EXCEPTION();
					xmmsrc.u64[0] = LoadMq(eaa);
					xmmsrc.u64[1] = LoadMq(eaa+8u);
					/* FIXME: Documentation says these are "reserved", doesn't say it causes a #UD, what really happens? */
					if ((cf=Fetchb()) > 7) goto illegal_opcode;
					SSE_CMPPS(fpu.xmmreg[reg],xmmsrc,cf);
				}
				break;
			case MP_F3:									/* F3 0F C2 CMPSS reg, r/m, imm8 */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII || !CPU_SSE()) goto illegal_opcode;
				if (rm >= 0xc0) {
					/* FIXME: Documentation says these are "reserved", doesn't say it causes a #UD, what really happens? */
					if ((cf=Fetchb()) > 7) goto illegal_opcode;
					SSE_CMPSS(fpu.xmmreg[reg],fpu.xmmreg[rm & 7],cf);
				} else {
					GetEAa;
					XMM_Reg xmmsrc;
					if (!SSE_REQUIRE_ALIGNMENT(eaa)) SSE_ALIGN_EXCEPTION();
					xmmsrc.u32[0] = LoadMd(eaa);
					/* FIXME: Documentation says these are "reserved", doesn't say it causes a #UD, what really happens? */
					if ((cf=Fetchb()) > 7) goto illegal_opcode;
					SSE_CMPSS(fpu.xmmreg[reg],xmmsrc,cf);
				}
				break;
			default:
				goto illegal_opcode;
		};
		break;
	}
	CASE_0F_MMX(0xc4)
	{
		GetRM;
		uint8_t imm;
		uint32_t src;
		const unsigned char reg = (rm >> 3) & 7;

		switch (last_prefix) {
			case MP_NONE:									/* 0F C4 PINSRW reg, r/m, imm8 */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII) goto illegal_opcode;
				if (rm >= 0xc0) {
					imm = Fetchb();
					MMX_PINSRW(*reg_mmx[reg],cpu_regs.regs[rm & 7].dword[0],imm);
				} else {
					GetEAa;
					src = LoadMd(eaa);
					imm = Fetchb();
					MMX_PINSRW(*reg_mmx[reg],src,imm);
				}
				break;
			case MP_66:									/* 66 0F C4 PINSRW reg, r/m, imm8 */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII || !CPU_SSE()) goto illegal_opcode;
				if (rm >= 0xc0) {
					imm = Fetchb();
					SSE_PINSRW(fpu.xmmreg[reg],cpu_regs.regs[rm & 7].dword[0],imm);
				} else {
					GetEAa;
					src = LoadMd(eaa);
					imm = Fetchb();
					SSE_PINSRW(fpu.xmmreg[reg],src,imm);
				}
				break;
			default:
				goto illegal_opcode;
		};
		break;
	}
	CASE_0F_MMX(0xc5)
	{
		GetRM;
		uint8_t imm;
		const unsigned char reg = (rm >> 3) & 7;

		switch (last_prefix) {
			case MP_NONE:									/* 0F C5 PEXTRW reg, r/m, imm8 */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII) goto illegal_opcode;
				if (rm >= 0xc0) {
					imm = Fetchb();
					MMX_PEXTRW(cpu_regs.regs[reg].dword[0],*reg_mmx[rm & 7],imm);
				} else {
					goto illegal_opcode;
				}
				break;
			case MP_66:									/* 66 0F C5 PEXTRW reg, r/m, imm8 */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII || !CPU_SSE()) goto illegal_opcode;
				if (rm >= 0xc0) {
					imm = Fetchb();
					SSE_PEXTRW(cpu_regs.regs[reg].dword[0],fpu.xmmreg[rm & 7],imm);
				} else {
					goto illegal_opcode;
				}
				break;
			default:
				goto illegal_opcode;
		};
		break;
	}
	CASE_0F_MMX(0xc6)
	{
		XMM_Reg xmmsrc;
		GetRM;
		uint8_t imm;
		const unsigned char reg = (rm >> 3) & 7;

		switch (last_prefix) {
			case MP_NONE:									/* 0F C6 SHUFPS reg, r/m, imm8 */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII || !CPU_SSE()) goto illegal_opcode;
				if (rm >= 0xc0) {
					imm = Fetchb();
					SSE_SHUFPS(fpu.xmmreg[reg],fpu.xmmreg[rm & 7],imm);
				} else {
					GetEAa;
					if (!SSE_REQUIRE_ALIGNMENT(eaa)) SSE_ALIGN_EXCEPTION();
					xmmsrc.u64[0] = LoadMq(eaa);
					xmmsrc.u64[1] = LoadMq(eaa+8u);
					imm = Fetchb();
					SSE_SHUFPS(fpu.xmmreg[reg],xmmsrc,imm);
				}
				break;
			default:
				goto illegal_opcode;
		};
		break;
	}
	CASE_0F_MMX(0xd1)												/* PSRLW Pq,Qq */
	{
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PMMXSLOW) goto illegal_opcode;
		GetRM;
		MMX_reg* dest=lookupRMregMM[rm];
		MMX_reg src;
		if (rm>=0xc0) {
			src.q=reg_mmx[rm&7]->q;
		} else {
			GetEAa;
			src.q=LoadMq(eaa);
		}
		if (src.q > 15) dest->q = 0;
		else {
			dest->uw.w0 >>= src.ub.b0;
			dest->uw.w1 >>= src.ub.b0;
			dest->uw.w2 >>= src.ub.b0;
			dest->uw.w3 >>= src.ub.b0;
		}
		break;
	}
	CASE_0F_MMX(0xd2)												/* PSRLD Pq,Qq */
	{
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PMMXSLOW) goto illegal_opcode;
		GetRM;
		MMX_reg* dest=lookupRMregMM[rm];
		MMX_reg src;
		if (rm>=0xc0) {
			src.q=reg_mmx[rm&7]->q;
		} else {
			GetEAa;
			src.q=LoadMq(eaa);
		}
		if (src.q > 31) dest->q = 0;
		else {
			dest->ud.d0 >>= src.ub.b0;
			dest->ud.d1 >>= src.ub.b0;
		}
		break;
	}
	CASE_0F_MMX(0xd3)												/* PSRLQ Pq,Qq */
	{
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PMMXSLOW) goto illegal_opcode;
		GetRM;
		MMX_reg* dest=lookupRMregMM[rm];
		MMX_reg src;
		if (rm>=0xc0) {
			src.q=reg_mmx[rm&7]->q;
		} else {
			GetEAa;
			src.q=LoadMq(eaa);
		}
		if (src.q > 63) dest->q = 0;
		else dest->q >>= (uint64_t)src.ub.b0;
		break;
	}
	CASE_0F_MMX(0xD5)												/* PMULLW Pq,Qq */
	{
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PMMXSLOW) goto illegal_opcode;
		GetRM;
		MMX_reg* dest=lookupRMregMM[rm];
		MMX_reg src;
		if (rm>=0xc0) {
			src.q = reg_mmx[rm&7]->q;
		} else {
			GetEAa;
			src.q = LoadMq(eaa);
		}
		uint32_t product0 = (uint32_t)dest->uw.w0 * (uint32_t)src.uw.w0;
		uint32_t product1 = (uint32_t)dest->uw.w1 * (uint32_t)src.uw.w1;
		uint32_t product2 = (uint32_t)dest->uw.w2 * (uint32_t)src.uw.w2;
		uint32_t product3 = (uint32_t)dest->uw.w3 * (uint32_t)src.uw.w3;
		dest->uw.w0 = (product0 & 0xffff);
		dest->uw.w1 = (product1 & 0xffff);
		dest->uw.w2 = (product2 & 0xffff);
		dest->uw.w3 = (product3 & 0xffff);
		break;
	}
	CASE_0F_MMX(0xd7)
	{
		GetRM;
		const unsigned char reg = (rm >> 3) & 7;

		switch (last_prefix) {
			case MP_NONE:									/* 0F D7 PMOVMSKB reg, r/m */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII) goto illegal_opcode;
				if (rm >= 0xc0)
					MMX_PMOVMSKB(cpu_regs.regs[reg].dword[0],*reg_mmx[rm & 7]);
				else
					goto illegal_opcode;
				break;
			case MP_66:									/* 66 0F D7 PMOVMSKB reg, r/m */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII || !CPU_SSE()) goto illegal_opcode;
				if (rm >= 0xc0)
					SSE_PMOVMSKB(cpu_regs.regs[reg].dword[0],fpu.xmmreg[rm & 7]);
				else
					goto illegal_opcode;
				break;
			default:
				goto illegal_opcode;
		};
		break;
	}
	CASE_0F_MMX(0xD8)												/* PSUBUSB Pq,Qq */
	{
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PMMXSLOW) goto illegal_opcode;
		GetRM;
		MMX_reg* dest=lookupRMregMM[rm];
		MMX_reg src;
		MMX_reg result;
		if (rm>=0xc0) {
			src.q = reg_mmx[rm&7]->q;
		} else {
			GetEAa;
			src.q = LoadMq(eaa);
		}
		result.q = 0;
		if (dest->ub.b0>src.ub.b0) result.ub.b0 = dest->ub.b0 - src.ub.b0;
		if (dest->ub.b1>src.ub.b1) result.ub.b1 = dest->ub.b1 - src.ub.b1;
		if (dest->ub.b2>src.ub.b2) result.ub.b2 = dest->ub.b2 - src.ub.b2;
		if (dest->ub.b3>src.ub.b3) result.ub.b3 = dest->ub.b3 - src.ub.b3;
		if (dest->ub.b4>src.ub.b4) result.ub.b4 = dest->ub.b4 - src.ub.b4;
		if (dest->ub.b5>src.ub.b5) result.ub.b5 = dest->ub.b5 - src.ub.b5;
		if (dest->ub.b6>src.ub.b6) result.ub.b6 = dest->ub.b6 - src.ub.b6;
		if (dest->ub.b7>src.ub.b7) result.ub.b7 = dest->ub.b7 - src.ub.b7;
		dest->q = result.q;
		break;
	}
	CASE_0F_MMX(0xD9)												/* PSUBUSW Pq,Qq */
	{
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PMMXSLOW) goto illegal_opcode;
		GetRM;
		MMX_reg* dest=lookupRMregMM[rm];
		MMX_reg src;
		MMX_reg result;
		if (rm>=0xc0) {
			src.q = reg_mmx[rm&7]->q;
		} else {
			GetEAa;
			src.q = LoadMq(eaa);
		}
		result.q = 0;
		if (dest->uw.w0>src.uw.w0) result.uw.w0 = dest->uw.w0 - src.uw.w0;
		if (dest->uw.w1>src.uw.w1) result.uw.w1 = dest->uw.w1 - src.uw.w1;
		if (dest->uw.w2>src.uw.w2) result.uw.w2 = dest->uw.w2 - src.uw.w2;
		if (dest->uw.w3>src.uw.w3) result.uw.w3 = dest->uw.w3 - src.uw.w3;
		dest->q = result.q;
		break;
	}
	CASE_0F_MMX(0xda)
	{
		GetRM;
		const unsigned char reg = (rm >> 3) & 7;

		switch (last_prefix) {
			case MP_NONE:									/* 0F DA PMINUB reg, r/m */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII) goto illegal_opcode;
				if (rm >= 0xc0) {
					MMX_PMINUB(*reg_mmx[reg],*reg_mmx[rm & 7]);
				} else {
					GetEAa;
					MMX_reg smmx;
					smmx.q = LoadMq(eaa);
					MMX_PMINUB(*reg_mmx[reg],smmx);
				}
				break;
			case MP_66:									/* 66 0F DA PMINUB reg, r/m */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII || !CPU_SSE()) goto illegal_opcode;
				if (rm >= 0xc0) {
					SSE_PMINUB(fpu.xmmreg[reg],fpu.xmmreg[rm & 7]);
				} else {
					GetEAa;
					XMM_Reg sxmm;
					sxmm.u64[0] = LoadMq(eaa);
					sxmm.u64[1] = LoadMq(eaa+8u);
					SSE_PMINUB(fpu.xmmreg[reg],sxmm);
				}
				break;
			default:
				goto illegal_opcode;
		};
		break;
	}
	CASE_0F_MMX(0xdb)												/* PAND Pq,Qq */
	{
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PMMXSLOW) goto illegal_opcode;
		GetRM;
		MMX_reg* dest=lookupRMregMM[rm];
		if (rm>=0xc0) {
			MMX_reg* src=reg_mmx[rm&7];
			dest->q &= src->q;
		} else {
			GetEAa;
			dest->q &= LoadMq(eaa);
		}
		break;
	}
	CASE_0F_MMX(0xDC)												/* PADDUSB Pq,Qq */
	{
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PMMXSLOW) goto illegal_opcode;
		GetRM;
		MMX_reg* dest=lookupRMregMM[rm];
		MMX_reg src;
		if (rm>=0xc0) {
			src.q = reg_mmx[rm&7]->q;
		} else {
			GetEAa;
			src.q = LoadMq(eaa);
		}
		dest->ub.b0 = SaturateWordSToByteU((uint16_t)dest->ub.b0+(uint16_t)src.ub.b0);
		dest->ub.b1 = SaturateWordSToByteU((uint16_t)dest->ub.b1+(uint16_t)src.ub.b1);
		dest->ub.b2 = SaturateWordSToByteU((uint16_t)dest->ub.b2+(uint16_t)src.ub.b2);
		dest->ub.b3 = SaturateWordSToByteU((uint16_t)dest->ub.b3+(uint16_t)src.ub.b3);
		dest->ub.b4 = SaturateWordSToByteU((uint16_t)dest->ub.b4+(uint16_t)src.ub.b4);
		dest->ub.b5 = SaturateWordSToByteU((uint16_t)dest->ub.b5+(uint16_t)src.ub.b5);
		dest->ub.b6 = SaturateWordSToByteU((uint16_t)dest->ub.b6+(uint16_t)src.ub.b6);
		dest->ub.b7 = SaturateWordSToByteU((uint16_t)dest->ub.b7+(uint16_t)src.ub.b7);
		break;
	}
	CASE_0F_MMX(0xDD)												/* PADDUSW Pq,Qq */
	{
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PMMXSLOW) goto illegal_opcode;
		GetRM;
		MMX_reg* dest=lookupRMregMM[rm];
		MMX_reg src;
		if (rm>=0xc0) {
			src.q = reg_mmx[rm&7]->q;
		} else {
			GetEAa;
			src.q = LoadMq(eaa);
		}
		dest->uw.w0 = SaturateDwordSToWordU((uint32_t)dest->uw.w0+(uint32_t)src.uw.w0);
		dest->uw.w1 = SaturateDwordSToWordU((uint32_t)dest->uw.w1+(uint32_t)src.uw.w1);
		dest->uw.w2 = SaturateDwordSToWordU((uint32_t)dest->uw.w2+(uint32_t)src.uw.w2);
		dest->uw.w3 = SaturateDwordSToWordU((uint32_t)dest->uw.w3+(uint32_t)src.uw.w3);
		break;
	}
	CASE_0F_MMX(0xde)
	{
		GetRM;
		const unsigned char reg = (rm >> 3) & 7;

		switch (last_prefix) {
			case MP_NONE:									/* 0F DE PMAXUB reg, r/m */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII) goto illegal_opcode;
				if (rm >= 0xc0) {
					MMX_PMAXUB(*reg_mmx[reg],*reg_mmx[rm & 7]);
				} else {
					GetEAa;
					MMX_reg smmx;
					smmx.q = LoadMq(eaa);
					MMX_PMAXUB(*reg_mmx[reg],smmx);
				}
				break;
			case MP_66:									/* 66 0F DE PMAXUB reg, r/m */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII || !CPU_SSE()) goto illegal_opcode;
				if (rm >= 0xc0) {
					SSE_PMAXUB(fpu.xmmreg[reg],fpu.xmmreg[rm & 7]);
				} else {
					GetEAa;
					XMM_Reg smmx;
					smmx.u64[0] = LoadMq(eaa);
					smmx.u64[1] = LoadMq(eaa+8u);
					SSE_PMAXUB(fpu.xmmreg[reg],smmx);
				}
				break;
			default:
				goto illegal_opcode;
		};
		break;
	}
	CASE_0F_MMX(0xdf)												/* PANDN Pq,Qq */
	{
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PMMXSLOW) goto illegal_opcode;
		GetRM;
		MMX_reg* dest=lookupRMregMM[rm];
		if (rm>=0xc0) {
			MMX_reg* src=reg_mmx[rm&7];
			dest->q = ~dest->q & src->q;
		} else {
			GetEAa;
			dest->q = ~dest->q & LoadMq(eaa);
		}
		break;
	}
	CASE_0F_MMX(0xe0)
	{
		GetRM;
		const unsigned char reg = (rm >> 3) & 7;

		switch (last_prefix) {
			case MP_NONE:									/* 0F E0 PAVGB reg, r/m */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII) goto illegal_opcode;
				if (rm >= 0xc0) {
					MMX_PAVGB(*reg_mmx[reg],*reg_mmx[rm & 7]);
				} else {
					GetEAa;
					MMX_reg smmx;
					smmx.q = LoadMq(eaa);
					MMX_PAVGB(*reg_mmx[reg],smmx);
				}
				break;
			case MP_66:									/* 66 0F E0 PAVGB reg, r/m */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII || !CPU_SSE()) goto illegal_opcode;
				if (rm >= 0xc0) {
					SSE_PAVGB(fpu.xmmreg[reg],fpu.xmmreg[rm & 7]);
				} else {
					GetEAa;
					XMM_Reg smmx;
					smmx.u64[0] = LoadMq(eaa);
					smmx.u64[1] = LoadMq(eaa+8u);
					SSE_PAVGB(fpu.xmmreg[reg],smmx);
				}
				break;

			default:
				goto illegal_opcode;
		};
		break;
	}
	CASE_0F_MMX(0xe1)												/* PSRAW Pq,Qq */
	{
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PMMXSLOW) goto illegal_opcode;
		GetRM;
		MMX_reg* dest=lookupRMregMM[rm];
		MMX_reg src;
		MMX_reg tmp;
		tmp.q = dest->q;
		if (rm>=0xc0) {
			src.q=reg_mmx[rm&7]->q;
		} else {
			GetEAa;
			src.q=LoadMq(eaa);
		}
		if (!src.q) break;
		if (src.q > 15) {
			dest->uw.w0 = (tmp.uw.w0&0x8000)?0xffff:0;
			dest->uw.w1 = (tmp.uw.w1&0x8000)?0xffff:0;
			dest->uw.w2 = (tmp.uw.w2&0x8000)?0xffff:0;
			dest->uw.w3 = (tmp.uw.w3&0x8000)?0xffff:0;
		} else {
			dest->sw.w0 >>= (int16_t)src.ub.b0;
			dest->sw.w1 >>= (int16_t)src.ub.b0;
			dest->sw.w2 >>= (int16_t)src.ub.b0;
			dest->sw.w3 >>= (int16_t)src.ub.b0;
		}
		break;
	}
	CASE_0F_MMX(0xe2)												/* PSRAD Pq,Qq */
	{
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PMMXSLOW) goto illegal_opcode;
		GetRM;
		MMX_reg* dest=lookupRMregMM[rm];
		MMX_reg src;
		MMX_reg tmp;
		tmp.q = dest->q;
		if (rm>=0xc0) {
			src.q=reg_mmx[rm&7]->q;
		} else {
			GetEAa;
			src.q=LoadMq(eaa);
		}
		if (!src.q) break;
		if (src.q > 31) {
			dest->ud.d0 = (tmp.ud.d0&0x80000000)?0xffffffff:0;
			dest->ud.d1 = (tmp.ud.d1&0x80000000)?0xffffffff:0;
		} else {
			dest->ud.d0 >>= (int32_t)src.ub.b0;
			dest->ud.d1 >>= (int32_t)src.ub.b0;
		}
		break;
	}
	CASE_0F_MMX(0xe3)
	{
		GetRM;
		const unsigned char reg = (rm >> 3) & 7;

		switch (last_prefix) {
			case MP_NONE:									/* 0F E3 PAVGW reg, r/m */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII) goto illegal_opcode;
				if (rm >= 0xc0) {
					MMX_PAVGW(*reg_mmx[reg],*reg_mmx[rm & 7]);
				} else {
					GetEAa;
					MMX_reg smmx;
					smmx.q = LoadMq(eaa);
					MMX_PAVGW(*reg_mmx[reg],smmx);
				}
				break;
			case MP_66:									/* 66 0F E3 PAVGW reg, r/m */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII || !CPU_SSE()) goto illegal_opcode;
				if (rm >= 0xc0) {
					SSE_PAVGW(fpu.xmmreg[reg],fpu.xmmreg[rm & 7]);
				} else {
					GetEAa;
					XMM_Reg smmx;
					smmx.u64[0] = LoadMq(eaa);
					smmx.u64[1] = LoadMq(eaa+8u);
					SSE_PAVGW(fpu.xmmreg[reg],smmx);
				}
				break;
			default:
				goto illegal_opcode;
		};
		break;
	}
	CASE_0F_MMX(0xe4)
	{
		GetRM;
		const unsigned char reg = (rm >> 3) & 7;

		switch (last_prefix) {
			case MP_NONE:									/* 0F E4 PULHUW reg, r/m */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII) goto illegal_opcode;
				if (rm >= 0xc0) {
					MMX_PMULHUW(*reg_mmx[reg],*reg_mmx[rm & 7]);
				} else {
					GetEAa;
					MMX_reg smmx;
					smmx.q = LoadMq(eaa);
					MMX_PMULHUW(*reg_mmx[reg],smmx);
				}
				break;
			case MP_66:									/* 66 0F E4 PULHUW reg, r/m */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII || !CPU_SSE()) goto illegal_opcode;
				if (rm >= 0xc0) {
					SSE_PMULHUW(fpu.xmmreg[reg],fpu.xmmreg[rm & 7]);
				} else {
					GetEAa;
					XMM_Reg sxmm;
					sxmm.u64[0] = LoadMq(eaa);
					sxmm.u64[1] = LoadMq(eaa+8u);
					SSE_PMULHUW(fpu.xmmreg[reg],sxmm);
				}
				break;
			default:
				goto illegal_opcode;
		};
		break;
	}
	CASE_0F_MMX(0xE5)												/* PMULHW Pq,Qq */
	{
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PMMXSLOW) goto illegal_opcode;
		GetRM;
		MMX_reg* dest=lookupRMregMM[rm];
		MMX_reg src;
		if (rm>=0xc0) {
			src.q = reg_mmx[rm&7]->q;
		} else {
			GetEAa;
			src.q = LoadMq(eaa);
		}
		int32_t product0 = (int32_t)dest->sw.w0 * (int32_t)src.sw.w0;
		int32_t product1 = (int32_t)dest->sw.w1 * (int32_t)src.sw.w1;
		int32_t product2 = (int32_t)dest->sw.w2 * (int32_t)src.sw.w2;
		int32_t product3 = (int32_t)dest->sw.w3 * (int32_t)src.sw.w3;
		dest->uw.w0 = (uint16_t)(product0 >> 16);
		dest->uw.w1 = (uint16_t)(product1 >> 16);
		dest->uw.w2 = (uint16_t)(product2 >> 16);
		dest->uw.w3 = (uint16_t)(product3 >> 16);
		break;
	}
	CASE_0F_MMX(0xe7)
	{
		GetRM;
		const unsigned char reg = (rm >> 3) & 7;

		switch (last_prefix) {
			case MP_NONE:									/* 0F E7 MOVNTQ r/m, reg */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII) goto illegal_opcode;
				if (rm >= 0xc0) {
					goto illegal_opcode;
				} else {
					GetEAa;
					SaveMq(eaa,(*reg_mmx[reg]).q);
				}
				break;
			default:
				goto illegal_opcode;
		};
		break;
	}
	CASE_0F_MMX(0xE8)												/* PSUBSB Pq,Qq */
	{
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PMMXSLOW) goto illegal_opcode;
		GetRM;
		MMX_reg* dest=lookupRMregMM[rm];
		MMX_reg src;
		if (rm>=0xc0) {
			src.q = reg_mmx[rm&7]->q;
		} else {
			GetEAa;
			src.q = LoadMq(eaa);
		}
		dest->sb.b0 = SaturateWordSToByteS((int16_t)dest->sb.b0-(int16_t)src.sb.b0);
		dest->sb.b1 = SaturateWordSToByteS((int16_t)dest->sb.b1-(int16_t)src.sb.b1);
		dest->sb.b2 = SaturateWordSToByteS((int16_t)dest->sb.b2-(int16_t)src.sb.b2);
		dest->sb.b3 = SaturateWordSToByteS((int16_t)dest->sb.b3-(int16_t)src.sb.b3);
		dest->sb.b4 = SaturateWordSToByteS((int16_t)dest->sb.b4-(int16_t)src.sb.b4);
		dest->sb.b5 = SaturateWordSToByteS((int16_t)dest->sb.b5-(int16_t)src.sb.b5);
		dest->sb.b6 = SaturateWordSToByteS((int16_t)dest->sb.b6-(int16_t)src.sb.b6);
		dest->sb.b7 = SaturateWordSToByteS((int16_t)dest->sb.b7-(int16_t)src.sb.b7);
		break;
	}
	CASE_0F_MMX(0xE9)												/* PSUBSW Pq,Qq */
	{
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PMMXSLOW) goto illegal_opcode;
		GetRM;
		MMX_reg* dest=lookupRMregMM[rm];
		MMX_reg src;
		if (rm>=0xc0) {
			src.q = reg_mmx[rm&7]->q;
		} else {
			GetEAa;
			src.q = LoadMq(eaa);
		}
		dest->sw.w0 = SaturateDwordSToWordS((int32_t)dest->sw.w0-(int32_t)src.sw.w0);
		dest->sw.w1 = SaturateDwordSToWordS((int32_t)dest->sw.w1-(int32_t)src.sw.w1);
		dest->sw.w2 = SaturateDwordSToWordS((int32_t)dest->sw.w2-(int32_t)src.sw.w2);
		dest->sw.w3 = SaturateDwordSToWordS((int32_t)dest->sw.w3-(int32_t)src.sw.w3);
		break;
	}
	CASE_0F_MMX(0xea)
	{
		GetRM;
		const unsigned char reg = (rm >> 3) & 7;

		switch (last_prefix) {
			case MP_NONE:									/* 0F EA PMINSW reg, r/m */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII) goto illegal_opcode;
				if (rm >= 0xc0) {
					MMX_PMINSW(*reg_mmx[reg],*reg_mmx[rm & 7]);
				} else {
					GetEAa;
					MMX_reg smmx;
					smmx.q = LoadMq(eaa);
					MMX_PMINSW(*reg_mmx[reg],smmx);
				}
				break;
			case MP_66:									/* 66 0F EA PMINSW reg, r/m */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII || !CPU_SSE()) goto illegal_opcode;
				if (rm >= 0xc0) {
					SSE_PMINSW(fpu.xmmreg[reg],fpu.xmmreg[rm & 7]);
				} else {
					GetEAa;
					XMM_Reg sxmm;
					sxmm.u64[0] = LoadMq(eaa);
					sxmm.u64[1] = LoadMq(eaa+8u);
					SSE_PMINSW(fpu.xmmreg[reg],sxmm);
				}
				break;
			default:
				goto illegal_opcode;
		};
		break;
	}
	CASE_0F_MMX(0xeb)												/* POR Pq,Qq */
	{
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PMMXSLOW) goto illegal_opcode;
		GetRM;
		MMX_reg* dest=lookupRMregMM[rm];
		if (rm>=0xc0) {
			MMX_reg* src=reg_mmx[rm&7];
			dest->q |= src->q;
		} else {
			GetEAa;
			dest->q |= LoadMq(eaa);
		}
		break;
	}
	CASE_0F_MMX(0xEC)												/* PADDSB Pq,Qq */
	{
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PMMXSLOW) goto illegal_opcode;
		GetRM;
		MMX_reg* dest=lookupRMregMM[rm];
		MMX_reg src;
		if (rm>=0xc0) {
			src.q = reg_mmx[rm&7]->q;
		} else {
			GetEAa;
			src.q = LoadMq(eaa);
		}
		dest->sb.b0 = SaturateWordSToByteS((int16_t)dest->sb.b0+(int16_t)src.sb.b0);
		dest->sb.b1 = SaturateWordSToByteS((int16_t)dest->sb.b1+(int16_t)src.sb.b1);
		dest->sb.b2 = SaturateWordSToByteS((int16_t)dest->sb.b2+(int16_t)src.sb.b2);
		dest->sb.b3 = SaturateWordSToByteS((int16_t)dest->sb.b3+(int16_t)src.sb.b3);
		dest->sb.b4 = SaturateWordSToByteS((int16_t)dest->sb.b4+(int16_t)src.sb.b4);
		dest->sb.b5 = SaturateWordSToByteS((int16_t)dest->sb.b5+(int16_t)src.sb.b5);
		dest->sb.b6 = SaturateWordSToByteS((int16_t)dest->sb.b6+(int16_t)src.sb.b6);
		dest->sb.b7 = SaturateWordSToByteS((int16_t)dest->sb.b7+(int16_t)src.sb.b7);
		break;
	}
	CASE_0F_MMX(0xED)												/* PADDSW Pq,Qq */
	{
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PMMXSLOW) goto illegal_opcode;
		GetRM;
		MMX_reg* dest=lookupRMregMM[rm];
		MMX_reg src;
		if (rm>=0xc0) {
			src.q = reg_mmx[rm&7]->q;
		} else {
			GetEAa;
			src.q = LoadMq(eaa);
		}
		dest->sw.w0 = SaturateDwordSToWordS((int32_t)dest->sw.w0+(int32_t)src.sw.w0);
		dest->sw.w1 = SaturateDwordSToWordS((int32_t)dest->sw.w1+(int32_t)src.sw.w1);
		dest->sw.w2 = SaturateDwordSToWordS((int32_t)dest->sw.w2+(int32_t)src.sw.w2);
		dest->sw.w3 = SaturateDwordSToWordS((int32_t)dest->sw.w3+(int32_t)src.sw.w3);
		break;
	}
	CASE_0F_MMX(0xee)
	{
		GetRM;
		const unsigned char reg = (rm >> 3) & 7;

		switch (last_prefix) {
			case MP_NONE:									/* 0F EE PMAXSW reg, r/m */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII) goto illegal_opcode;
				if (rm >= 0xc0) {
					MMX_PMAXSW(*reg_mmx[reg],*reg_mmx[rm & 7]);
				} else {
					GetEAa;
					MMX_reg smmx;
					smmx.q = LoadMq(eaa);
					MMX_PMAXSW(*reg_mmx[reg],smmx);
				}
				break;
			case MP_66:									/* 66 0F EE PMAXSW reg, r/m */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII || !CPU_SSE()) goto illegal_opcode;
				if (rm >= 0xc0) {
					SSE_PMAXSW(fpu.xmmreg[reg],fpu.xmmreg[rm & 7]);
				} else {
					GetEAa;
					XMM_Reg sxmm;
					sxmm.u64[0] = LoadMq(eaa);
					sxmm.u64[1] = LoadMq(eaa+8u);
					SSE_PMAXSW(fpu.xmmreg[reg],sxmm);
				}
				break;
			default:
				goto illegal_opcode;
		};
		break;
	}
	CASE_0F_MMX(0xef)												/* PXOR Pq,Qq */
	{
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PMMXSLOW) goto illegal_opcode;
		GetRM;
		MMX_reg* dest=lookupRMregMM[rm];
		if (rm>=0xc0) {
			MMX_reg* src=reg_mmx[rm&7];
			dest->q ^= src->q;
		} else {
			GetEAa;
			dest->q ^= LoadMq(eaa);
		}
		break;
	}
	CASE_0F_MMX(0xf1)												/* PSLLW Pq,Qq */
	{
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PMMXSLOW) goto illegal_opcode;
		GetRM;
		MMX_reg* dest=lookupRMregMM[rm];
		MMX_reg src;
		if (rm>=0xc0) {
			src.q=reg_mmx[rm&7]->q;
		} else {
			GetEAa;
			src.q=LoadMq(eaa);
		}
		if (src.q > 15) dest->q = 0;
		else {
			dest->uw.w0 <<= src.ub.b0;
			dest->uw.w1 <<= src.ub.b0;
			dest->uw.w2 <<= src.ub.b0;
			dest->uw.w3 <<= src.ub.b0;
		}
		break;
	}
	CASE_0F_MMX(0xf2)												/* PSLLD Pq,Qq */
	{
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PMMXSLOW) goto illegal_opcode;
		GetRM;
		MMX_reg* dest=lookupRMregMM[rm];
		MMX_reg src;
		if (rm>=0xc0) {
			src.q=reg_mmx[rm&7]->q;
		} else {
			GetEAa;
			src.q=LoadMq(eaa);
		}
		if (src.q > 31) dest->q = 0;
		else {
			dest->ud.d0 <<= src.ub.b0;
			dest->ud.d1 <<= src.ub.b0;
		}
		break;
	}
	CASE_0F_MMX(0xf3)												/* PSLLQ Pq,Qq */
	{
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PMMXSLOW) goto illegal_opcode;
		GetRM;
		MMX_reg* dest=lookupRMregMM[rm];
		MMX_reg src;
		if (rm>=0xc0) {
			src.q=reg_mmx[rm&7]->q;
		} else {
			GetEAa;
			src.q=LoadMq(eaa);
		}
		if (src.q > 63) dest->q = 0;
		else dest->q <<= (uint64_t)src.ub.b0;
		break;
	}
	CASE_0F_MMX(0xF5)												/* PMADDWD Pq,Qq */
	{
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PMMXSLOW) goto illegal_opcode;
		GetRM;
		MMX_reg* dest=lookupRMregMM[rm];
		MMX_reg src;
		if (rm>=0xc0) {
			src.q = reg_mmx[rm&7]->q;
		} else {
			GetEAa;
			src.q = LoadMq(eaa);
		}
		if (dest->ud.d0 == 0x80008000 && src.ud.d0 == 0x80008000)
			dest->ud.d0 = 0x80000000;
		else {
			int32_t product0 = (int32_t)dest->sw.w0 * (int32_t)src.sw.w0;
			int32_t product1 = (int32_t)dest->sw.w1 * (int32_t)src.sw.w1;
			dest->ud.d0 = (uint32_t)(product0 + product1);
		}
		if (dest->ud.d1 == 0x80008000 && src.ud.d1 == 0x80008000)
			dest->ud.d1 = 0x80000000;
		else {
			int32_t product2 = (int32_t)dest->sw.w2 * (int32_t)src.sw.w2;
			int32_t product3 = (int32_t)dest->sw.w3 * (int32_t)src.sw.w3;
			dest->sd.d1 = (int32_t)(product2 + product3);
		}
		break;
	}
	CASE_0F_MMX(0xf6)
	{
		GetRM;
		const unsigned char reg = (rm >> 3) & 7;

		switch (last_prefix) {
			case MP_NONE:									/* 0F F6 PSADBW reg, r/m */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII) goto illegal_opcode;
				if (rm >= 0xc0) {
					MMX_PSADBW(*reg_mmx[reg],*reg_mmx[rm & 7]);
				} else {
					GetEAa;
					MMX_reg smmx;
					smmx.q = LoadMq(eaa);
					MMX_PSADBW(*reg_mmx[reg],smmx);
				}
				break;
			case MP_66:									/* 66 0F F6 PSADBW reg, r/m */
				if (CPU_ArchitectureType<CPU_ARCHTYPE_PENTIUMIII || !CPU_SSE()) goto illegal_opcode;
				if (rm >= 0xc0) {
					SSE_PSADBW(fpu.xmmreg[reg],fpu.xmmreg[rm & 7]);
				} else {
					GetEAa;
					XMM_Reg sxmm;
					sxmm.u64[0] = LoadMq(eaa);
					sxmm.u64[1] = LoadMq(eaa+8u);
					SSE_PSADBW(fpu.xmmreg[reg],sxmm);
				}
				break;
			default:
				goto illegal_opcode;
		};
		break;
	}
	CASE_0F_MMX(0xf7)
	{
		GetRM;
		const unsigned char reg = (rm >> 3) & 7;

		switch (last_prefix) {
			case MP_NONE:									/* 0F F7 MASKMOVQ reg, r/m */
				if (rm >= 0xc0) {
					/* the MSB of each byte in second operand indicates whether to write the corresponding byte value from the first operand
					 * i.e. MASKMOVQ mm1,mm2 would selectively write bytes from mm1 according to MSBs of each byte in mm1. The target memory
					 * location is DS:EDI (DS can be overridden by a segment prefix). */
					/* On real hardware, this controls the byte enables on the wide data path from the processor.
					 * We don't have that here, so we have to break up the writes */
					PhysPt aa = BaseDS + (reg_edi & AddrMaskTable[core.prefixes & PREFIX_ADDR]);
					const MMX_reg *data = reg_mmx[reg];
					const MMX_reg *msk = reg_mmx[rm & 7];
					if (msk->ub.b0 & 0x80) SaveMb(aa+0,data->ub.b0);
					if (msk->ub.b1 & 0x80) SaveMb(aa+1,data->ub.b1);
					if (msk->ub.b2 & 0x80) SaveMb(aa+2,data->ub.b2);
					if (msk->ub.b3 & 0x80) SaveMb(aa+3,data->ub.b3);
					if (msk->ub.b4 & 0x80) SaveMb(aa+4,data->ub.b4);
					if (msk->ub.b5 & 0x80) SaveMb(aa+5,data->ub.b5);
					if (msk->ub.b6 & 0x80) SaveMb(aa+6,data->ub.b6);
					if (msk->ub.b7 & 0x80) SaveMb(aa+7,data->ub.b7);
				} else {
					goto illegal_opcode;
				}
				break;
			default:
				goto illegal_opcode;
		};
		break;
	}
	CASE_0F_MMX(0xF8)												/* PSUBB Pq,Qq */
	{
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PMMXSLOW) goto illegal_opcode;
		GetRM;
		MMX_reg* dest=lookupRMregMM[rm];
		MMX_reg src;
		if (rm>=0xc0) {
			src.q=reg_mmx[rm&7]->q;
		} else {
			GetEAa;
			src.q = LoadMq(eaa);
		}
		dest->ub.b0 -= src.ub.b0;
		dest->ub.b1 -= src.ub.b1;
		dest->ub.b2 -= src.ub.b2;
		dest->ub.b3 -= src.ub.b3;
		dest->ub.b4 -= src.ub.b4;
		dest->ub.b5 -= src.ub.b5;
		dest->ub.b6 -= src.ub.b6;
		dest->ub.b7 -= src.ub.b7;
		break;
	}
	CASE_0F_MMX(0xF9)												/* PSUBW Pq,Qq */
	{
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PMMXSLOW) goto illegal_opcode;
		GetRM;
		MMX_reg* dest=lookupRMregMM[rm];
		MMX_reg src;
		if (rm>=0xc0) {
			src.q=reg_mmx[rm&7]->q;
		} else {
			GetEAa;
			src.q = LoadMq(eaa);
		}
		dest->uw.w0 -= src.uw.w0;
		dest->uw.w1 -= src.uw.w1;
		dest->uw.w2 -= src.uw.w2;
		dest->uw.w3 -= src.uw.w3;
		break;
	}
	CASE_0F_MMX(0xFA)												/* PSUBD Pq,Qq */
	{
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PMMXSLOW) goto illegal_opcode;
		GetRM;
		MMX_reg* dest=lookupRMregMM[rm];
		MMX_reg src;
		if (rm>=0xc0) {
			src.q=reg_mmx[rm&7]->q;
		} else {
			GetEAa;
			src.q = LoadMq(eaa);
		}
		dest->ud.d0 -= src.ud.d0;
		dest->ud.d1 -= src.ud.d1;
		break;
	}
	CASE_0F_MMX(0xFC)												/* PADDB Pq,Qq */
	{
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PMMXSLOW) goto illegal_opcode;
		GetRM;
		MMX_reg* dest=lookupRMregMM[rm];
		MMX_reg src;
		if (rm>=0xc0) {
			src.q=reg_mmx[rm&7]->q;
		} else {
			GetEAa;
			src.q = LoadMq(eaa);
		}
		dest->ub.b0 += src.ub.b0;
		dest->ub.b1 += src.ub.b1;
		dest->ub.b2 += src.ub.b2;
		dest->ub.b3 += src.ub.b3;
		dest->ub.b4 += src.ub.b4;
		dest->ub.b5 += src.ub.b5;
		dest->ub.b6 += src.ub.b6;
		dest->ub.b7 += src.ub.b7;
		break;
	}
	CASE_0F_MMX(0xFD)												/* PADDW Pq,Qq */
	{
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PMMXSLOW) goto illegal_opcode;
		GetRM;
		MMX_reg* dest=lookupRMregMM[rm];
		MMX_reg src;
		if (rm>=0xc0) {
			src.q=reg_mmx[rm&7]->q;
		} else {
			GetEAa;
			src.q = LoadMq(eaa);
		}
		dest->uw.w0 += src.uw.w0;
		dest->uw.w1 += src.uw.w1;
		dest->uw.w2 += src.uw.w2;
		dest->uw.w3 += src.uw.w3;
		break;
	}
	CASE_0F_MMX(0xFE)												/* PADDD Pq,Qq */
	{
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PMMXSLOW) goto illegal_opcode;
		GetRM;
		MMX_reg* dest=lookupRMregMM[rm];
		MMX_reg src;
		if (rm>=0xc0) {
			src.q=reg_mmx[rm&7]->q;
		} else {
			GetEAa;
			src.q = LoadMq(eaa);
		}
		dest->ud.d0 += src.ud.d0;
		dest->ud.d1 += src.ud.d1;
		break;
	}

