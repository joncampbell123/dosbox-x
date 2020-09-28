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

{
	EAPoint seg_base;
	uint16_t off;
	switch ((inst.rm_mod<<3)|inst.rm_eai) {
	case 0x00:
		off=reg_bx+reg_si;
		seg_base=SegBase(ds);
		break;
	case 0x01:
		off=reg_bx+reg_di;
		seg_base=SegBase(ds);
		break;
	case 0x02:
		off=reg_bp+reg_si;
		seg_base=SegBase(ss);
		break;
	case 0x03:
		off=reg_bp+reg_di;
		seg_base=SegBase(ss);
		break;
	case 0x04:
		off=reg_si;
		seg_base=SegBase(ds);
		break;
	case 0x05:
		off=reg_di;
		seg_base=SegBase(ds);
		break;
	case 0x06:
		off=Fetchw();
		seg_base=SegBase(ds);
		break;
	case 0x07:
		off=reg_bx;
		seg_base=SegBase(ds);
		break;

	case 0x08:
		off=reg_bx+reg_si+Fetchbs();
		seg_base=SegBase(ds);
		break;
	case 0x09:
		off=reg_bx+reg_di+Fetchbs();
		seg_base=SegBase(ds);
		break;
	case 0x0a:
		off=reg_bp+reg_si+Fetchbs();
		seg_base=SegBase(ss);
		break;
	case 0x0b:
		off=reg_bp+reg_di+Fetchbs();
		seg_base=SegBase(ss);
		break;
	case 0x0c:
		off=reg_si+Fetchbs();
		seg_base=SegBase(ds);
		break;
	case 0x0d:
		off=reg_di+Fetchbs();
		seg_base=SegBase(ds);
		break;
	case 0x0e:
		off=reg_bp+Fetchbs();
		seg_base=SegBase(ss);
		break;
	case 0x0f:
		off=reg_bx+Fetchbs();
		seg_base=SegBase(ds);
		break;
	
	case 0x10:
		off=reg_bx+reg_si+Fetchws();
		seg_base=SegBase(ds);
		break;
	case 0x11:
		off=reg_bx+reg_di+Fetchws();
		seg_base=SegBase(ds);
		break;
	case 0x12:
		off=reg_bp+reg_si+Fetchws();
		seg_base=SegBase(ss);
		break;
	case 0x13:
		off=reg_bp+reg_di+Fetchws();
		seg_base=SegBase(ss);
		break;
	case 0x14:
		off=reg_si+Fetchws();
		seg_base=SegBase(ds);
		break;
	case 0x15:
		off=reg_di+Fetchws();
		seg_base=SegBase(ds);
		break;
	case 0x16:
		off=reg_bp+Fetchws();
		seg_base=SegBase(ss);
		break;
	case 0x17:
		off=reg_bx+Fetchws();
		seg_base=SegBase(ds);
		break;
	default:
		off=seg_base=0;
		break;
	}
	inst.rm_off=off;
	if (inst.prefix & PREFIX_SEG) {
		inst.rm_eaa=inst.seg.base+off;
	} else {
		inst.rm_eaa=seg_base+off;
	}
} else  {


#define SIB(MODE)	{												\
	Bitu sib=Fetchb();												\
	switch (sib&7) {												\
	case 0:seg_base=SegBase(ds);off=reg_eax;break;					\
	case 1:seg_base=SegBase(ds);off=reg_ecx;break;					\
	case 2:seg_base=SegBase(ds);off=reg_edx;break;					\
	case 3:seg_base=SegBase(ds);off=reg_ebx;break;					\
	case 4:seg_base=SegBase(ss);off=reg_esp;break;					\
	case 5:if (!MODE) {	seg_base=SegBase(ds);off=Fetchd();break;	\
		   } else { seg_base=SegBase(ss);off=reg_ebp;break;}		\
	case 6:seg_base=SegBase(ds);off=reg_esi;break;					\
	case 7:seg_base=SegBase(ds);off=reg_edi;break;					\
	}																\
	off+=*SIBIndex[(sib >> 3) &7] << (sib >> 6);					\
};																	
	static uint32_t SIBZero=0;
	static uint32_t * SIBIndex[8]= { &reg_eax,&reg_ecx,&reg_edx,&reg_ebx,&SIBZero,&reg_ebp,&reg_esi,&reg_edi };
	EAPoint seg_base;
	uint32_t off;

	switch ((inst.rm_mod<<3)|inst.rm_eai) {
	case 0x00:
		off=reg_eax;
		seg_base=SegBase(ds);
		break;
	case 0x01:
		off=reg_ecx;
		seg_base=SegBase(ds);
		break;
	case 0x02:
		off=reg_edx;
		seg_base=SegBase(ds);
		break;
	case 0x03:
		off=reg_ebx;
		seg_base=SegBase(ds);
		break;
	case 0x04:
		SIB(0);
		break;
	case 0x05:
		off=Fetchd();
		seg_base=SegBase(ds);
		break;
	case 0x06:
		off=reg_esi;
		seg_base=SegBase(ds);
		break;
	case 0x07:
		off=reg_edi;
		seg_base=SegBase(ds);
		break;
	
	case 0x08:
		off=reg_eax+(uint32_t)Fetchbs();
		seg_base=SegBase(ds);
		break;
	case 0x09:
		off=reg_ecx+(uint32_t)Fetchbs();
		seg_base=SegBase(ds);
		break;
	case 0x0a:
		off=reg_edx+(uint32_t)Fetchbs();
		seg_base=SegBase(ds);
		break;
	case 0x0b:
		off=reg_ebx+(uint32_t)Fetchbs();
		seg_base=SegBase(ds);
		break;
	case 0x0c:
		SIB(1);
		off+=(uint32_t)Fetchbs();
		break;
	case 0x0d:
		off=reg_ebp+(uint32_t)Fetchbs();
		seg_base=SegBase(ss);
		break;
	case 0x0e:
		off=reg_esi+(uint32_t)Fetchbs();
		seg_base=SegBase(ds);
		break;
	case 0x0f:
		off=reg_edi+(uint32_t)Fetchbs();
		seg_base=SegBase(ds);
		break;

	case 0x10:
		off=reg_eax+(uint32_t)Fetchds();
		seg_base=SegBase(ds);
		break;
	case 0x11:
		off=reg_ecx+(uint32_t)Fetchds();
		seg_base=SegBase(ds);
		break;
	case 0x12:
		off=reg_edx+(uint32_t)Fetchds();
		seg_base=SegBase(ds);
		break;
	case 0x13:
		off=reg_ebx+(uint32_t)Fetchds();
		seg_base=SegBase(ds);
		break;
	case 0x14:
		SIB(1);
		off+=(uint32_t)Fetchds();
		break;
	case 0x15:
		off=reg_ebp+(uint32_t)Fetchds();
		seg_base=SegBase(ss);
		break;
	case 0x16:
		off=reg_esi+(uint32_t)Fetchds();
		seg_base=SegBase(ds);
		break;
	case 0x17:
		off=reg_edi+(uint32_t)Fetchds();
		seg_base=SegBase(ds);
		break;
	default:
		off=seg_base=0;
		break;
	}
	inst.rm_off=off;
	if (inst.prefix & PREFIX_SEG) {
		inst.rm_eaa=inst.seg.base+off;
	} else {
		inst.rm_eaa=seg_base+off;
	}
}
