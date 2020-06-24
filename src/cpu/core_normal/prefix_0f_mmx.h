/*
 *  Copyright (C) 2002-2019  The DOSBox Team
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335, USA.
 */

/* State Management */
	CASE_0F_MMX(0x77)												/* EMMS */
	{
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PMMXSLOW) goto illegal_opcode;
		setFPUTagEmpty();
		break;
	}


/* Data Movement */
	CASE_0F_MMX(0x6e)												/* MOVD Pq,Ed */
	{
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PMMXSLOW) goto illegal_opcode;
		GetRM;
		MMX_reg* rmrq=lookupRMregMM[rm];
		if (rm>=0xc0) {
			GetEArd;
			rmrq->ud.d0=*(Bit32u*)eard;
			rmrq->ud.d1=0;
		} else {
			GetEAa;
			rmrq->ud.d0=LoadMd(eaa);
			rmrq->ud.d1=0;
		}
		break;
	}
	CASE_0F_MMX(0x7e)												/* MOVD Ed,Pq */
	{
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PMMXSLOW) goto illegal_opcode;
		GetRM;
		MMX_reg* rmrq=lookupRMregMM[rm];
		if (rm>=0xc0) {
			GetEArd;
			*(Bit32u*)eard=rmrq->ud.d0;
		} else {
			GetEAa;
			SaveMd(eaa,rmrq->ud.d0);
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

/* Boolean Logic */
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

/* Shift */
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
		if (src.ub.b0 > 15) dest->q = 0;
		else {
			dest->uw.w0 <<= src.ub.b0;
			dest->uw.w1 <<= src.ub.b0;
			dest->uw.w2 <<= src.ub.b0;
			dest->uw.w3 <<= src.ub.b0;
		}
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
		if (src.ub.b0 > 15) dest->q = 0;
		else {
			dest->uw.w0 >>= src.ub.b0;
			dest->uw.w1 >>= src.ub.b0;
			dest->uw.w2 >>= src.ub.b0;
			dest->uw.w3 >>= src.ub.b0;
		}
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
		if (src.ub.b0 > 15) {
			dest->uw.w0 = (tmp.uw.w0&0x8000)?0xffff:0;
			dest->uw.w1 = (tmp.uw.w1&0x8000)?0xffff:0;
			dest->uw.w2 = (tmp.uw.w2&0x8000)?0xffff:0;
			dest->uw.w3 = (tmp.uw.w3&0x8000)?0xffff:0;
		} else {
			dest->uw.w0 >>= src.ub.b0;
			dest->uw.w1 >>= src.ub.b0;
			dest->uw.w2 >>= src.ub.b0;
			dest->uw.w3 >>= src.ub.b0;
			if (tmp.uw.w0&0x8000) dest->uw.w0 |= (0xffff << (16 - src.ub.b0));
			if (tmp.uw.w1&0x8000) dest->uw.w1 |= (0xffff << (16 - src.ub.b0));
			if (tmp.uw.w2&0x8000) dest->uw.w2 |= (0xffff << (16 - src.ub.b0));
			if (tmp.uw.w3&0x8000) dest->uw.w3 |= (0xffff << (16 - src.ub.b0));
		}
		break;
	}
	CASE_0F_MMX(0x71)												/* PSLLW/PSRLW/PSRAW Pq,Ib */
	{
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PMMXSLOW) goto illegal_opcode;
		GetRM;
		Bit8u op=(rm>>3)&7;
		Bit8u shift=Fetchb();
		MMX_reg* dest=reg_mmx[rm&7];
		switch (op) {
			case 0x06: 	/*PSLLW*/
				if (shift > 15) dest->q = 0;
				else {
					dest->uw.w0 <<= shift;
					dest->uw.w1 <<= shift;
					dest->uw.w2 <<= shift;
					dest->uw.w3 <<= shift;
				}
				break;
			case 0x02:  /*PSRLW*/
				if (shift > 15) dest->q = 0;
				else {
					dest->uw.w0 >>= shift;
					dest->uw.w1 >>= shift;
					dest->uw.w2 >>= shift;
					dest->uw.w3 >>= shift;
				}
				break;
			case 0x04:  /*PSRAW*/
				MMX_reg tmp;
				if (!shift) break;
				tmp.q = dest->q;
				if (shift > 15) {
					dest->uw.w0 = (tmp.uw.w0&0x8000)?0xffff:0;
					dest->uw.w1 = (tmp.uw.w1&0x8000)?0xffff:0;
					dest->uw.w2 = (tmp.uw.w2&0x8000)?0xffff:0;
					dest->uw.w3 = (tmp.uw.w3&0x8000)?0xffff:0;
				} else {
					dest->uw.w0 >>= shift;
					dest->uw.w1 >>= shift;
					dest->uw.w2 >>= shift;
					dest->uw.w3 >>= shift;
					if (tmp.uw.w0&0x8000) dest->uw.w0 |= (0xffff << (16 - shift));
					if (tmp.uw.w1&0x8000) dest->uw.w1 |= (0xffff << (16 - shift));
					if (tmp.uw.w2&0x8000) dest->uw.w2 |= (0xffff << (16 - shift));
					if (tmp.uw.w3&0x8000) dest->uw.w3 |= (0xffff << (16 - shift));
				}
				break;
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
		if (src.ub.b0 > 31) dest->q = 0;
		else {
			dest->ud.d0 <<= src.ub.b0;
			dest->ud.d1 <<= src.ub.b0;
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
		if (src.ub.b0 > 31) dest->q = 0;
		else {
			dest->ud.d0 >>= src.ub.b0;
			dest->ud.d1 >>= src.ub.b0;
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
		if (src.ub.b0 > 31) {
			dest->ud.d0 = (tmp.ud.d0&0x80000000)?0xffffffff:0;
			dest->ud.d1 = (tmp.ud.d1&0x80000000)?0xffffffff:0;
		} else {
			dest->ud.d0 >>= src.ub.b0;
			dest->ud.d1 >>= src.ub.b0;
			if (tmp.ud.d0&0x80000000) dest->ud.d0 |= (0xffffffff << (32 - src.ub.b0));
			if (tmp.ud.d1&0x80000000) dest->ud.d1 |= (0xffffffff << (32 - src.ub.b0));
		}
		break;
	}
	CASE_0F_MMX(0x72)												/* PSLLD/PSRLD/PSRAD Pq,Ib */
	{
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PMMXSLOW) goto illegal_opcode;
		GetRM;
		Bit8u op=(rm>>3)&7;
		Bit8u shift=Fetchb();
		MMX_reg* dest=reg_mmx[rm&7];
		switch (op) {
			case 0x06: 	/*PSLLD*/
				if (shift > 31) dest->q = 0;
				else {
					dest->ud.d0 <<= shift;
					dest->ud.d1 <<= shift;
				}
				break;
			case 0x02:  /*PSRLD*/
				if (shift > 31) dest->q = 0;
				else {
					dest->ud.d0 >>= shift;
					dest->ud.d1 >>= shift;
				}
				break;
			case 0x04:  /*PSRAD*/
				MMX_reg tmp;
				if (!shift) break;
				tmp.q = dest->q;
				if (shift > 31) { 
					dest->ud.d0 = (tmp.ud.d0&0x80000000)?0xffffffff:0;
					dest->ud.d1 = (tmp.ud.d1&0x80000000)?0xffffffff:0;
				} else {
					dest->ud.d0 >>= shift;
					dest->ud.d1 >>= shift;
					if (tmp.ud.d0&0x80000000) dest->ud.d0 |= (0xffffffff << (32 - shift));
					if (tmp.ud.d1&0x80000000) dest->ud.d1 |= (0xffffffff << (32 - shift));
				}
				break;
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
		if (src.ub.b0 > 63) dest->q = 0;
		else dest->q <<= src.ub.b0;
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
		if (src.ub.b0 > 63) dest->q = 0;
		else dest->q >>= src.ub.b0;
		break;
	}
	CASE_0F_MMX(0x73)												/* PSLLQ/PSRLQ Pq,Ib */
	{
		if (CPU_ArchitectureType<CPU_ARCHTYPE_PMMXSLOW) goto illegal_opcode;
		GetRM;
		Bit8u shift=Fetchb();
		MMX_reg* dest=reg_mmx[rm&7];
		if (shift > 63) dest->q = 0;
		else {
			Bit8u op=rm&0x20;
			if (op) {
				dest->q <<= shift;
			} else {
				dest->q >>= shift;
			}
		}
		break;
	}

/* Math */
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
		dest->sb.b0 = SaturateWordSToByteS((Bit16s)dest->sb.b0+(Bit16s)src.sb.b0);
		dest->sb.b1 = SaturateWordSToByteS((Bit16s)dest->sb.b1+(Bit16s)src.sb.b1);
		dest->sb.b2 = SaturateWordSToByteS((Bit16s)dest->sb.b2+(Bit16s)src.sb.b2);
		dest->sb.b3 = SaturateWordSToByteS((Bit16s)dest->sb.b3+(Bit16s)src.sb.b3);
		dest->sb.b4 = SaturateWordSToByteS((Bit16s)dest->sb.b4+(Bit16s)src.sb.b4);
		dest->sb.b5 = SaturateWordSToByteS((Bit16s)dest->sb.b5+(Bit16s)src.sb.b5);
		dest->sb.b6 = SaturateWordSToByteS((Bit16s)dest->sb.b6+(Bit16s)src.sb.b6);
		dest->sb.b7 = SaturateWordSToByteS((Bit16s)dest->sb.b7+(Bit16s)src.sb.b7);
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
		dest->sw.w0 = SaturateDwordSToWordS((Bit32s)dest->sw.w0+(Bit32s)src.sw.w0);
		dest->sw.w1 = SaturateDwordSToWordS((Bit32s)dest->sw.w1+(Bit32s)src.sw.w1);
		dest->sw.w2 = SaturateDwordSToWordS((Bit32s)dest->sw.w2+(Bit32s)src.sw.w2);
		dest->sw.w3 = SaturateDwordSToWordS((Bit32s)dest->sw.w3+(Bit32s)src.sw.w3);
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
		dest->ub.b0 = SaturateWordSToByteU((Bit16s)dest->ub.b0+(Bit16s)src.ub.b0);
		dest->ub.b1 = SaturateWordSToByteU((Bit16s)dest->ub.b1+(Bit16s)src.ub.b1);
		dest->ub.b2 = SaturateWordSToByteU((Bit16s)dest->ub.b2+(Bit16s)src.ub.b2);
		dest->ub.b3 = SaturateWordSToByteU((Bit16s)dest->ub.b3+(Bit16s)src.ub.b3);
		dest->ub.b4 = SaturateWordSToByteU((Bit16s)dest->ub.b4+(Bit16s)src.ub.b4);
		dest->ub.b5 = SaturateWordSToByteU((Bit16s)dest->ub.b5+(Bit16s)src.ub.b5);
		dest->ub.b6 = SaturateWordSToByteU((Bit16s)dest->ub.b6+(Bit16s)src.ub.b6);
		dest->ub.b7 = SaturateWordSToByteU((Bit16s)dest->ub.b7+(Bit16s)src.ub.b7);
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
		dest->uw.w0 = SaturateDwordSToWordU((Bit32s)dest->uw.w0+(Bit32s)src.uw.w0);
		dest->uw.w1 = SaturateDwordSToWordU((Bit32s)dest->uw.w1+(Bit32s)src.uw.w1);
		dest->uw.w2 = SaturateDwordSToWordU((Bit32s)dest->uw.w2+(Bit32s)src.uw.w2);
		dest->uw.w3 = SaturateDwordSToWordU((Bit32s)dest->uw.w3+(Bit32s)src.uw.w3);
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
		dest->sb.b0 = SaturateWordSToByteS((Bit16s)dest->sb.b0-(Bit16s)src.sb.b0);
		dest->sb.b1 = SaturateWordSToByteS((Bit16s)dest->sb.b1-(Bit16s)src.sb.b1);
		dest->sb.b2 = SaturateWordSToByteS((Bit16s)dest->sb.b2-(Bit16s)src.sb.b2);
		dest->sb.b3 = SaturateWordSToByteS((Bit16s)dest->sb.b3-(Bit16s)src.sb.b3);
		dest->sb.b4 = SaturateWordSToByteS((Bit16s)dest->sb.b4-(Bit16s)src.sb.b4);
		dest->sb.b5 = SaturateWordSToByteS((Bit16s)dest->sb.b5-(Bit16s)src.sb.b5);
		dest->sb.b6 = SaturateWordSToByteS((Bit16s)dest->sb.b6-(Bit16s)src.sb.b6);
		dest->sb.b7 = SaturateWordSToByteS((Bit16s)dest->sb.b7-(Bit16s)src.sb.b7);
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
		dest->sw.w0 = SaturateDwordSToWordS((Bit32s)dest->sw.w0-(Bit32s)src.sw.w0);
		dest->sw.w1 = SaturateDwordSToWordS((Bit32s)dest->sw.w1-(Bit32s)src.sw.w1);
		dest->sw.w2 = SaturateDwordSToWordS((Bit32s)dest->sw.w2-(Bit32s)src.sw.w2);
		dest->sw.w3 = SaturateDwordSToWordS((Bit32s)dest->sw.w3-(Bit32s)src.sw.w3);
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
		Bit32s product0 = (Bit32s)dest->sw.w0 * (Bit32s)src.sw.w0;
		Bit32s product1 = (Bit32s)dest->sw.w1 * (Bit32s)src.sw.w1;
		Bit32s product2 = (Bit32s)dest->sw.w2 * (Bit32s)src.sw.w2;
		Bit32s product3 = (Bit32s)dest->sw.w3 * (Bit32s)src.sw.w3;
		dest->uw.w0 = (Bit16u)(product0 >> 16);
		dest->uw.w1 = (Bit16u)(product1 >> 16);
		dest->uw.w2 = (Bit16u)(product2 >> 16);
		dest->uw.w3 = (Bit16u)(product3 >> 16);
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
		Bit32u product0 = (Bit32u)dest->uw.w0 * (Bit32u)src.uw.w0;
		Bit32u product1 = (Bit32u)dest->uw.w1 * (Bit32u)src.uw.w1;
		Bit32u product2 = (Bit32u)dest->uw.w2 * (Bit32u)src.uw.w2;
		Bit32u product3 = (Bit32u)dest->uw.w3 * (Bit32u)src.uw.w3;
		dest->uw.w0 = (product0 & 0xffff);
		dest->uw.w1 = (product1 & 0xffff);
		dest->uw.w2 = (product2 & 0xffff);
		dest->uw.w3 = (product3 & 0xffff);
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
			Bit32s product0 = (Bit32s)dest->sw.w0 * (Bit32s)src.sw.w0;
			Bit32s product1 = (Bit32s)dest->sw.w1 * (Bit32s)src.sw.w1;
			dest->ud.d0 = (uint32_t)(product0 + product1);
		}
		if (dest->ud.d1 == 0x80008000 && src.ud.d1 == 0x80008000)
			dest->ud.d1 = 0x80000000;
		else {
			Bit32s product2 = (Bit32s)dest->sw.w2 * (Bit32s)src.sw.w2;
			Bit32s product3 = (Bit32s)dest->sw.w3 * (Bit32s)src.sw.w3;
			dest->sd.d1 = (int32_t)(product2 + product3);
		}
		break;
	}

/* Comparison */
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

/* Data Packing */
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
