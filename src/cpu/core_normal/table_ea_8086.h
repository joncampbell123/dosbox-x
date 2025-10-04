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

/* The MOD/RM Decoder for EA for this decoder's addressing modes */
static PhysPt EA86_16_00_n(void) { return BaseDS+(last_ea86_offset=((uint16_t)(reg_bx+(int16_t)reg_si))); }
static PhysPt EA86_16_01_n(void) { return BaseDS+(last_ea86_offset=((uint16_t)(reg_bx+(int16_t)reg_di))); }
static PhysPt EA86_16_02_n(void) { return BaseSS+(last_ea86_offset=((uint16_t)(reg_bp+(int16_t)reg_si))); }
static PhysPt EA86_16_03_n(void) { return BaseSS+(last_ea86_offset=((uint16_t)(reg_bp+(int16_t)reg_di))); }
static PhysPt EA86_16_04_n(void) { return BaseDS+(last_ea86_offset=((uint16_t)(reg_si))); }
static PhysPt EA86_16_05_n(void) { return BaseDS+(last_ea86_offset=((uint16_t)(reg_di))); }
static PhysPt EA86_16_06_n(void) { return BaseDS+(last_ea86_offset=((uint16_t)(Fetchw())));}
static PhysPt EA86_16_07_n(void) { return BaseDS+(last_ea86_offset=((uint16_t)(reg_bx))); }

static PhysPt EA86_16_40_n(void) { return BaseDS+(last_ea86_offset=((uint16_t)(reg_bx+(int16_t)reg_si+Fetchbs()))); }
static PhysPt EA86_16_41_n(void) { return BaseDS+(last_ea86_offset=((uint16_t)(reg_bx+(int16_t)reg_di+Fetchbs()))); }
static PhysPt EA86_16_42_n(void) { return BaseSS+(last_ea86_offset=((uint16_t)(reg_bp+(int16_t)reg_si+Fetchbs()))); }
static PhysPt EA86_16_43_n(void) { return BaseSS+(last_ea86_offset=((uint16_t)(reg_bp+(int16_t)reg_di+Fetchbs()))); }
static PhysPt EA86_16_44_n(void) { return BaseDS+(last_ea86_offset=((uint16_t)(reg_si+Fetchbs()))); }
static PhysPt EA86_16_45_n(void) { return BaseDS+(last_ea86_offset=((uint16_t)(reg_di+Fetchbs()))); }
static PhysPt EA86_16_46_n(void) { return BaseSS+(last_ea86_offset=((uint16_t)(reg_bp+Fetchbs()))); }
static PhysPt EA86_16_47_n(void) { return BaseDS+(last_ea86_offset=((uint16_t)(reg_bx+Fetchbs()))); }

static PhysPt EA86_16_80_n(void) { return BaseDS+(last_ea86_offset=((uint16_t)(reg_bx+(int16_t)reg_si+Fetchws()))); }
static PhysPt EA86_16_81_n(void) { return BaseDS+(last_ea86_offset=((uint16_t)(reg_bx+(int16_t)reg_di+Fetchws()))); }
static PhysPt EA86_16_82_n(void) { return BaseSS+(last_ea86_offset=((uint16_t)(reg_bp+(int16_t)reg_si+Fetchws()))); }
static PhysPt EA86_16_83_n(void) { return BaseSS+(last_ea86_offset=((uint16_t)(reg_bp+(int16_t)reg_di+Fetchws()))); }
static PhysPt EA86_16_84_n(void) { return BaseDS+(last_ea86_offset=((uint16_t)(reg_si+Fetchws()))); }
static PhysPt EA86_16_85_n(void) { return BaseDS+(last_ea86_offset=((uint16_t)(reg_di+Fetchws()))); }
static PhysPt EA86_16_86_n(void) { return BaseSS+(last_ea86_offset=((uint16_t)(reg_bp+Fetchws()))); }
static PhysPt EA86_16_87_n(void) { return BaseDS+(last_ea86_offset=((uint16_t)(reg_bx+Fetchws()))); }

static GetEAHandler EATable8086[512]={
/* 00 */
	EA86_16_00_n,EA86_16_01_n,EA86_16_02_n,EA86_16_03_n,EA86_16_04_n,EA86_16_05_n,EA86_16_06_n,EA86_16_07_n,
	EA86_16_00_n,EA86_16_01_n,EA86_16_02_n,EA86_16_03_n,EA86_16_04_n,EA86_16_05_n,EA86_16_06_n,EA86_16_07_n,
	EA86_16_00_n,EA86_16_01_n,EA86_16_02_n,EA86_16_03_n,EA86_16_04_n,EA86_16_05_n,EA86_16_06_n,EA86_16_07_n,
	EA86_16_00_n,EA86_16_01_n,EA86_16_02_n,EA86_16_03_n,EA86_16_04_n,EA86_16_05_n,EA86_16_06_n,EA86_16_07_n,
	EA86_16_00_n,EA86_16_01_n,EA86_16_02_n,EA86_16_03_n,EA86_16_04_n,EA86_16_05_n,EA86_16_06_n,EA86_16_07_n,
	EA86_16_00_n,EA86_16_01_n,EA86_16_02_n,EA86_16_03_n,EA86_16_04_n,EA86_16_05_n,EA86_16_06_n,EA86_16_07_n,
	EA86_16_00_n,EA86_16_01_n,EA86_16_02_n,EA86_16_03_n,EA86_16_04_n,EA86_16_05_n,EA86_16_06_n,EA86_16_07_n,
	EA86_16_00_n,EA86_16_01_n,EA86_16_02_n,EA86_16_03_n,EA86_16_04_n,EA86_16_05_n,EA86_16_06_n,EA86_16_07_n,
/* 01 */
	EA86_16_40_n,EA86_16_41_n,EA86_16_42_n,EA86_16_43_n,EA86_16_44_n,EA86_16_45_n,EA86_16_46_n,EA86_16_47_n,
	EA86_16_40_n,EA86_16_41_n,EA86_16_42_n,EA86_16_43_n,EA86_16_44_n,EA86_16_45_n,EA86_16_46_n,EA86_16_47_n,
	EA86_16_40_n,EA86_16_41_n,EA86_16_42_n,EA86_16_43_n,EA86_16_44_n,EA86_16_45_n,EA86_16_46_n,EA86_16_47_n,
	EA86_16_40_n,EA86_16_41_n,EA86_16_42_n,EA86_16_43_n,EA86_16_44_n,EA86_16_45_n,EA86_16_46_n,EA86_16_47_n,
	EA86_16_40_n,EA86_16_41_n,EA86_16_42_n,EA86_16_43_n,EA86_16_44_n,EA86_16_45_n,EA86_16_46_n,EA86_16_47_n,
	EA86_16_40_n,EA86_16_41_n,EA86_16_42_n,EA86_16_43_n,EA86_16_44_n,EA86_16_45_n,EA86_16_46_n,EA86_16_47_n,
	EA86_16_40_n,EA86_16_41_n,EA86_16_42_n,EA86_16_43_n,EA86_16_44_n,EA86_16_45_n,EA86_16_46_n,EA86_16_47_n,
	EA86_16_40_n,EA86_16_41_n,EA86_16_42_n,EA86_16_43_n,EA86_16_44_n,EA86_16_45_n,EA86_16_46_n,EA86_16_47_n,
/* 10 */
	EA86_16_80_n,EA86_16_81_n,EA86_16_82_n,EA86_16_83_n,EA86_16_84_n,EA86_16_85_n,EA86_16_86_n,EA86_16_87_n,
	EA86_16_80_n,EA86_16_81_n,EA86_16_82_n,EA86_16_83_n,EA86_16_84_n,EA86_16_85_n,EA86_16_86_n,EA86_16_87_n,
	EA86_16_80_n,EA86_16_81_n,EA86_16_82_n,EA86_16_83_n,EA86_16_84_n,EA86_16_85_n,EA86_16_86_n,EA86_16_87_n,
	EA86_16_80_n,EA86_16_81_n,EA86_16_82_n,EA86_16_83_n,EA86_16_84_n,EA86_16_85_n,EA86_16_86_n,EA86_16_87_n,
	EA86_16_80_n,EA86_16_81_n,EA86_16_82_n,EA86_16_83_n,EA86_16_84_n,EA86_16_85_n,EA86_16_86_n,EA86_16_87_n,
	EA86_16_80_n,EA86_16_81_n,EA86_16_82_n,EA86_16_83_n,EA86_16_84_n,EA86_16_85_n,EA86_16_86_n,EA86_16_87_n,
	EA86_16_80_n,EA86_16_81_n,EA86_16_82_n,EA86_16_83_n,EA86_16_84_n,EA86_16_85_n,EA86_16_86_n,EA86_16_87_n,
	EA86_16_80_n,EA86_16_81_n,EA86_16_82_n,EA86_16_83_n,EA86_16_84_n,EA86_16_85_n,EA86_16_86_n,EA86_16_87_n,
/* 11 These are illegal so make em nullptr */
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
/* 00 */
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
/* 01 */
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
/* 10 */
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
/* 11 These are illegal so make em 0 */
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr
};

#if CPU_CORE <= CPU_ARCHTYPE_8086
#define GetEADirect(sz)							\
	PhysPt eaa;								\
	eaa=BaseDS+Fetchw();
#endif

