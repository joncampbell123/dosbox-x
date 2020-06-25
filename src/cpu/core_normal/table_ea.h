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

typedef PhysPt (*EA_LookupHandler)(void);

/* The MOD/RM Decoder for EA for this decoder's addressing modes */
static PhysPt EA_16_00_n(void) { return BaseDS+(Bit16u)(reg_bx+reg_si); }
static PhysPt EA_16_01_n(void) { return BaseDS+(Bit16u)(reg_bx+reg_di); }
static PhysPt EA_16_02_n(void) { return BaseSS+(Bit16u)(reg_bp+reg_si); }
static PhysPt EA_16_03_n(void) { return BaseSS+(Bit16u)(reg_bp+reg_di); }
static PhysPt EA_16_04_n(void) { return BaseDS+(Bit16u)(reg_si); }
static PhysPt EA_16_05_n(void) { return BaseDS+(Bit16u)(reg_di); }
static PhysPt EA_16_06_n(void) { return BaseDS+(Bit16u)(Fetchw());}
static PhysPt EA_16_07_n(void) { return BaseDS+(Bit16u)(reg_bx); }

static PhysPt EA_16_40_n(void) { return BaseDS+(Bit16u)(reg_bx+reg_si+(Bit16u)Fetchbs()); }
static PhysPt EA_16_41_n(void) { return BaseDS+(Bit16u)(reg_bx+reg_di+(Bit16u)Fetchbs()); }
static PhysPt EA_16_42_n(void) { return BaseSS+(Bit16u)(reg_bp+reg_si+(Bit16u)Fetchbs()); }
static PhysPt EA_16_43_n(void) { return BaseSS+(Bit16u)(reg_bp+reg_di+(Bit16u)Fetchbs()); }
static PhysPt EA_16_44_n(void) { return BaseDS+(Bit16u)(reg_si+(Bit16u)Fetchbs()); }
static PhysPt EA_16_45_n(void) { return BaseDS+(Bit16u)(reg_di+(Bit16u)Fetchbs()); }
static PhysPt EA_16_46_n(void) { return BaseSS+(Bit16u)(reg_bp+(Bit16u)Fetchbs()); }
static PhysPt EA_16_47_n(void) { return BaseDS+(Bit16u)(reg_bx+(Bit16u)Fetchbs()); }

static PhysPt EA_16_80_n(void) { return BaseDS+(Bit16u)(reg_bx+reg_si+(Bit16u)Fetchws()); }
static PhysPt EA_16_81_n(void) { return BaseDS+(Bit16u)(reg_bx+reg_di+(Bit16u)Fetchws()); }
static PhysPt EA_16_82_n(void) { return BaseSS+(Bit16u)(reg_bp+reg_si+(Bit16u)Fetchws()); }
static PhysPt EA_16_83_n(void) { return BaseSS+(Bit16u)(reg_bp+reg_di+(Bit16u)Fetchws()); }
static PhysPt EA_16_84_n(void) { return BaseDS+(Bit16u)(reg_si+(Bit16u)Fetchws()); }
static PhysPt EA_16_85_n(void) { return BaseDS+(Bit16u)(reg_di+(Bit16u)Fetchws()); }
static PhysPt EA_16_86_n(void) { return BaseSS+(Bit16u)(reg_bp+(Bit16u)Fetchws()); }
static PhysPt EA_16_87_n(void) { return BaseDS+(Bit16u)(reg_bx+(Bit16u)Fetchws()); }

static Bit32u SIBZero=0;
static Bit32u * SIBIndex[8]= { &reg_eax,&reg_ecx,&reg_edx,&reg_ebx,&SIBZero,&reg_ebp,&reg_esi,&reg_edi };

static INLINE PhysPt Sib(Bitu mode) {
	Bit8u sib=Fetchb();
	PhysPt base;
	switch (sib&7) {
	case 0:	/* EAX Base */
		base=BaseDS+reg_eax;break;
	case 1:	/* ECX Base */
		base=BaseDS+reg_ecx;break;
	case 2:	/* EDX Base */
		base=BaseDS+reg_edx;break;
	case 3:	/* EBX Base */
		base=BaseDS+reg_ebx;break;
	case 4:	/* ESP Base */
		base=BaseSS+reg_esp;break;
	case 5:	/* #1 Base */
		if (!mode) {
			base=BaseDS+Fetchd();break;
		} else {
			base=BaseSS+reg_ebp;break;
		}
	case 6:	/* ESI Base */
		base=BaseDS+reg_esi;break;
	case 7:	/* EDI Base */
		base=BaseDS+reg_edi;break;
	}
	base+=*SIBIndex[(sib >> 3) &7] << (sib >> 6);
	return base;
}

static PhysPt EA_32_00_n(void) { return BaseDS+reg_eax; }
static PhysPt EA_32_01_n(void) { return BaseDS+reg_ecx; }
static PhysPt EA_32_02_n(void) { return BaseDS+reg_edx; }
static PhysPt EA_32_03_n(void) { return BaseDS+reg_ebx; }
static PhysPt EA_32_04_n(void) { return Sib(0);}
static PhysPt EA_32_05_n(void) { return BaseDS+Fetchd(); }
static PhysPt EA_32_06_n(void) { return BaseDS+reg_esi; }
static PhysPt EA_32_07_n(void) { return BaseDS+reg_edi; }

static PhysPt EA_32_40_n(void) { return BaseDS+reg_eax+(PhysPt)Fetchbs(); }
static PhysPt EA_32_41_n(void) { return BaseDS+reg_ecx+(PhysPt)Fetchbs(); }
static PhysPt EA_32_42_n(void) { return BaseDS+reg_edx+(PhysPt)Fetchbs(); }
static PhysPt EA_32_43_n(void) { return BaseDS+reg_ebx+(PhysPt)Fetchbs(); }
static PhysPt EA_32_44_n(void) { PhysPt temp=Sib(1);return temp+(PhysPt)Fetchbs();}
//static PhysPt EA_32_44_n(void) { return Sib(1)+Fetchbs();}
static PhysPt EA_32_45_n(void) { return BaseSS+reg_ebp+(PhysPt)Fetchbs(); }
static PhysPt EA_32_46_n(void) { return BaseDS+reg_esi+(PhysPt)Fetchbs(); }
static PhysPt EA_32_47_n(void) { return BaseDS+reg_edi+(PhysPt)Fetchbs(); }

static PhysPt EA_32_80_n(void) { return BaseDS+reg_eax+(PhysPt)Fetchds(); }
static PhysPt EA_32_81_n(void) { return BaseDS+reg_ecx+(PhysPt)Fetchds(); }
static PhysPt EA_32_82_n(void) { return BaseDS+reg_edx+(PhysPt)Fetchds(); }
static PhysPt EA_32_83_n(void) { return BaseDS+reg_ebx+(PhysPt)Fetchds(); }
static PhysPt EA_32_84_n(void) { PhysPt temp=Sib(2);return temp+(PhysPt)Fetchds();}
//static PhysPt EA_32_84_n(void) { return Sib(2)+Fetchds();}
static PhysPt EA_32_85_n(void) { return BaseSS+reg_ebp+(PhysPt)Fetchds(); }
static PhysPt EA_32_86_n(void) { return BaseDS+reg_esi+(PhysPt)Fetchds(); }
static PhysPt EA_32_87_n(void) { return BaseDS+reg_edi+(PhysPt)Fetchds(); }

static GetEAHandler EATable[512]={
/* 00 */
	EA_16_00_n,EA_16_01_n,EA_16_02_n,EA_16_03_n,EA_16_04_n,EA_16_05_n,EA_16_06_n,EA_16_07_n,
	EA_16_00_n,EA_16_01_n,EA_16_02_n,EA_16_03_n,EA_16_04_n,EA_16_05_n,EA_16_06_n,EA_16_07_n,
	EA_16_00_n,EA_16_01_n,EA_16_02_n,EA_16_03_n,EA_16_04_n,EA_16_05_n,EA_16_06_n,EA_16_07_n,
	EA_16_00_n,EA_16_01_n,EA_16_02_n,EA_16_03_n,EA_16_04_n,EA_16_05_n,EA_16_06_n,EA_16_07_n,
	EA_16_00_n,EA_16_01_n,EA_16_02_n,EA_16_03_n,EA_16_04_n,EA_16_05_n,EA_16_06_n,EA_16_07_n,
	EA_16_00_n,EA_16_01_n,EA_16_02_n,EA_16_03_n,EA_16_04_n,EA_16_05_n,EA_16_06_n,EA_16_07_n,
	EA_16_00_n,EA_16_01_n,EA_16_02_n,EA_16_03_n,EA_16_04_n,EA_16_05_n,EA_16_06_n,EA_16_07_n,
	EA_16_00_n,EA_16_01_n,EA_16_02_n,EA_16_03_n,EA_16_04_n,EA_16_05_n,EA_16_06_n,EA_16_07_n,
/* 01 */
	EA_16_40_n,EA_16_41_n,EA_16_42_n,EA_16_43_n,EA_16_44_n,EA_16_45_n,EA_16_46_n,EA_16_47_n,
	EA_16_40_n,EA_16_41_n,EA_16_42_n,EA_16_43_n,EA_16_44_n,EA_16_45_n,EA_16_46_n,EA_16_47_n,
	EA_16_40_n,EA_16_41_n,EA_16_42_n,EA_16_43_n,EA_16_44_n,EA_16_45_n,EA_16_46_n,EA_16_47_n,
	EA_16_40_n,EA_16_41_n,EA_16_42_n,EA_16_43_n,EA_16_44_n,EA_16_45_n,EA_16_46_n,EA_16_47_n,
	EA_16_40_n,EA_16_41_n,EA_16_42_n,EA_16_43_n,EA_16_44_n,EA_16_45_n,EA_16_46_n,EA_16_47_n,
	EA_16_40_n,EA_16_41_n,EA_16_42_n,EA_16_43_n,EA_16_44_n,EA_16_45_n,EA_16_46_n,EA_16_47_n,
	EA_16_40_n,EA_16_41_n,EA_16_42_n,EA_16_43_n,EA_16_44_n,EA_16_45_n,EA_16_46_n,EA_16_47_n,
	EA_16_40_n,EA_16_41_n,EA_16_42_n,EA_16_43_n,EA_16_44_n,EA_16_45_n,EA_16_46_n,EA_16_47_n,
/* 10 */
	EA_16_80_n,EA_16_81_n,EA_16_82_n,EA_16_83_n,EA_16_84_n,EA_16_85_n,EA_16_86_n,EA_16_87_n,
	EA_16_80_n,EA_16_81_n,EA_16_82_n,EA_16_83_n,EA_16_84_n,EA_16_85_n,EA_16_86_n,EA_16_87_n,
	EA_16_80_n,EA_16_81_n,EA_16_82_n,EA_16_83_n,EA_16_84_n,EA_16_85_n,EA_16_86_n,EA_16_87_n,
	EA_16_80_n,EA_16_81_n,EA_16_82_n,EA_16_83_n,EA_16_84_n,EA_16_85_n,EA_16_86_n,EA_16_87_n,
	EA_16_80_n,EA_16_81_n,EA_16_82_n,EA_16_83_n,EA_16_84_n,EA_16_85_n,EA_16_86_n,EA_16_87_n,
	EA_16_80_n,EA_16_81_n,EA_16_82_n,EA_16_83_n,EA_16_84_n,EA_16_85_n,EA_16_86_n,EA_16_87_n,
	EA_16_80_n,EA_16_81_n,EA_16_82_n,EA_16_83_n,EA_16_84_n,EA_16_85_n,EA_16_86_n,EA_16_87_n,
	EA_16_80_n,EA_16_81_n,EA_16_82_n,EA_16_83_n,EA_16_84_n,EA_16_85_n,EA_16_86_n,EA_16_87_n,
/* 11 These are illegal so make em 0 */
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
/* 00 */
	EA_32_00_n,EA_32_01_n,EA_32_02_n,EA_32_03_n,EA_32_04_n,EA_32_05_n,EA_32_06_n,EA_32_07_n,
	EA_32_00_n,EA_32_01_n,EA_32_02_n,EA_32_03_n,EA_32_04_n,EA_32_05_n,EA_32_06_n,EA_32_07_n,
	EA_32_00_n,EA_32_01_n,EA_32_02_n,EA_32_03_n,EA_32_04_n,EA_32_05_n,EA_32_06_n,EA_32_07_n,
	EA_32_00_n,EA_32_01_n,EA_32_02_n,EA_32_03_n,EA_32_04_n,EA_32_05_n,EA_32_06_n,EA_32_07_n,
	EA_32_00_n,EA_32_01_n,EA_32_02_n,EA_32_03_n,EA_32_04_n,EA_32_05_n,EA_32_06_n,EA_32_07_n,
	EA_32_00_n,EA_32_01_n,EA_32_02_n,EA_32_03_n,EA_32_04_n,EA_32_05_n,EA_32_06_n,EA_32_07_n,
	EA_32_00_n,EA_32_01_n,EA_32_02_n,EA_32_03_n,EA_32_04_n,EA_32_05_n,EA_32_06_n,EA_32_07_n,
	EA_32_00_n,EA_32_01_n,EA_32_02_n,EA_32_03_n,EA_32_04_n,EA_32_05_n,EA_32_06_n,EA_32_07_n,
/* 01 */
	EA_32_40_n,EA_32_41_n,EA_32_42_n,EA_32_43_n,EA_32_44_n,EA_32_45_n,EA_32_46_n,EA_32_47_n,
	EA_32_40_n,EA_32_41_n,EA_32_42_n,EA_32_43_n,EA_32_44_n,EA_32_45_n,EA_32_46_n,EA_32_47_n,
	EA_32_40_n,EA_32_41_n,EA_32_42_n,EA_32_43_n,EA_32_44_n,EA_32_45_n,EA_32_46_n,EA_32_47_n,
	EA_32_40_n,EA_32_41_n,EA_32_42_n,EA_32_43_n,EA_32_44_n,EA_32_45_n,EA_32_46_n,EA_32_47_n,
	EA_32_40_n,EA_32_41_n,EA_32_42_n,EA_32_43_n,EA_32_44_n,EA_32_45_n,EA_32_46_n,EA_32_47_n,
	EA_32_40_n,EA_32_41_n,EA_32_42_n,EA_32_43_n,EA_32_44_n,EA_32_45_n,EA_32_46_n,EA_32_47_n,
	EA_32_40_n,EA_32_41_n,EA_32_42_n,EA_32_43_n,EA_32_44_n,EA_32_45_n,EA_32_46_n,EA_32_47_n,
	EA_32_40_n,EA_32_41_n,EA_32_42_n,EA_32_43_n,EA_32_44_n,EA_32_45_n,EA_32_46_n,EA_32_47_n,
/* 10 */
	EA_32_80_n,EA_32_81_n,EA_32_82_n,EA_32_83_n,EA_32_84_n,EA_32_85_n,EA_32_86_n,EA_32_87_n,
	EA_32_80_n,EA_32_81_n,EA_32_82_n,EA_32_83_n,EA_32_84_n,EA_32_85_n,EA_32_86_n,EA_32_87_n,
	EA_32_80_n,EA_32_81_n,EA_32_82_n,EA_32_83_n,EA_32_84_n,EA_32_85_n,EA_32_86_n,EA_32_87_n,
	EA_32_80_n,EA_32_81_n,EA_32_82_n,EA_32_83_n,EA_32_84_n,EA_32_85_n,EA_32_86_n,EA_32_87_n,
	EA_32_80_n,EA_32_81_n,EA_32_82_n,EA_32_83_n,EA_32_84_n,EA_32_85_n,EA_32_86_n,EA_32_87_n,
	EA_32_80_n,EA_32_81_n,EA_32_82_n,EA_32_83_n,EA_32_84_n,EA_32_85_n,EA_32_86_n,EA_32_87_n,
	EA_32_80_n,EA_32_81_n,EA_32_82_n,EA_32_83_n,EA_32_84_n,EA_32_85_n,EA_32_86_n,EA_32_87_n,
	EA_32_80_n,EA_32_81_n,EA_32_82_n,EA_32_83_n,EA_32_84_n,EA_32_85_n,EA_32_86_n,EA_32_87_n,
/* 11 These are illegal so make em 0 */
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0
};

extern bool do_seg_limits;

#define GetEADirect(sz)						\
	PhysPt eaa;						\
	if (TEST_PREFIX_ADDR)					\
		eaa=Fetchd();					\
	else							\
		eaa=Fetchw();					\
	if (do_seg_limits) {					\
		if (Segs.expanddown[core.base_val_ds]) {	\
			if (eaa <= SegLimit(core.base_val_ds)) {\
				LOG_MSG("Limit check %x <= %x (E)",(unsigned int)eaa,(unsigned int)SegLimit(core.base_val_ds)); \
				goto gp_fault;			\
			}					\
		}						\
		else {						\
			if ((eaa+(sz)-1UL) > SegLimit(core.base_val_ds)) { \
				LOG_MSG("Limit check %x+%x-1 = %x > %x",(unsigned int)eaa,(unsigned int)sz,(unsigned int)(eaa+(sz)-1U),(unsigned int)SegLimit(core.base_val_ds)); \
				goto gp_fault;			\
			}					\
		}						\
	}							\
	eaa += BaseDS;

