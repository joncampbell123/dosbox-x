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
static PhysPt EA_16_00_n(void) { return BaseDS+(last_ea86_offset=((uint16_t)(reg_bx+(Bit16s)reg_si))); }
static PhysPt EA_16_01_n(void) { return BaseDS+(last_ea86_offset=((uint16_t)(reg_bx+(Bit16s)reg_di))); }
static PhysPt EA_16_02_n(void) { return BaseSS+(last_ea86_offset=((uint16_t)(reg_bp+(Bit16s)reg_si))); }
static PhysPt EA_16_03_n(void) { return BaseSS+(last_ea86_offset=((uint16_t)(reg_bp+(Bit16s)reg_di))); }
static PhysPt EA_16_04_n(void) { return BaseDS+(last_ea86_offset=((uint16_t)(reg_si))); }
static PhysPt EA_16_05_n(void) { return BaseDS+(last_ea86_offset=((uint16_t)(reg_di))); }
static PhysPt EA_16_06_n(void) { return BaseDS+(last_ea86_offset=((uint16_t)(Fetchw())));}
static PhysPt EA_16_07_n(void) { return BaseDS+(last_ea86_offset=((uint16_t)(reg_bx))); }

static PhysPt EA_16_40_n(void) { return BaseDS+(last_ea86_offset=((uint16_t)(reg_bx+(Bit16s)reg_si+Fetchbs()))); }
static PhysPt EA_16_41_n(void) { return BaseDS+(last_ea86_offset=((uint16_t)(reg_bx+(Bit16s)reg_di+Fetchbs()))); }
static PhysPt EA_16_42_n(void) { return BaseSS+(last_ea86_offset=((uint16_t)(reg_bp+(Bit16s)reg_si+Fetchbs()))); }
static PhysPt EA_16_43_n(void) { return BaseSS+(last_ea86_offset=((uint16_t)(reg_bp+(Bit16s)reg_di+Fetchbs()))); }
static PhysPt EA_16_44_n(void) { return BaseDS+(last_ea86_offset=((uint16_t)(reg_si+Fetchbs()))); }
static PhysPt EA_16_45_n(void) { return BaseDS+(last_ea86_offset=((uint16_t)(reg_di+Fetchbs()))); }
static PhysPt EA_16_46_n(void) { return BaseSS+(last_ea86_offset=((uint16_t)(reg_bp+Fetchbs()))); }
static PhysPt EA_16_47_n(void) { return BaseDS+(last_ea86_offset=((uint16_t)(reg_bx+Fetchbs()))); }

static PhysPt EA_16_80_n(void) { return BaseDS+(last_ea86_offset=((uint16_t)(reg_bx+(Bit16s)reg_si+Fetchws()))); }
static PhysPt EA_16_81_n(void) { return BaseDS+(last_ea86_offset=((uint16_t)(reg_bx+(Bit16s)reg_di+Fetchws()))); }
static PhysPt EA_16_82_n(void) { return BaseSS+(last_ea86_offset=((uint16_t)(reg_bp+(Bit16s)reg_si+Fetchws()))); }
static PhysPt EA_16_83_n(void) { return BaseSS+(last_ea86_offset=((uint16_t)(reg_bp+(Bit16s)reg_di+Fetchws()))); }
static PhysPt EA_16_84_n(void) { return BaseDS+(last_ea86_offset=((uint16_t)(reg_si+Fetchws()))); }
static PhysPt EA_16_85_n(void) { return BaseDS+(last_ea86_offset=((uint16_t)(reg_di+Fetchws()))); }
static PhysPt EA_16_86_n(void) { return BaseSS+(last_ea86_offset=((uint16_t)(reg_bp+Fetchws()))); }
static PhysPt EA_16_87_n(void) { return BaseDS+(last_ea86_offset=((uint16_t)(reg_bx+Fetchws()))); }

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
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
/* 01 */
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
/* 10 */
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
/* 11 These are illegal so make em 0 */
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0
};

#define GetEADirect(sz)							\
	PhysPt eaa;								\
	eaa=BaseDS+Fetchw();				\

