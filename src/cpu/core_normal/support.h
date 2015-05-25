/*
 *  Copyright (C) 2002-2015  The DOSBox Team
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


#define LoadMbs(off) (Bit8s)(LoadMb(off))
#define LoadMws(off) (Bit16s)(LoadMw(off))
#define LoadMds(off) (Bit32s)(LoadMd(off))

#define LoadRb(reg) reg
#define LoadRw(reg) reg
#define LoadRd(reg) reg

#define SaveRb(reg,val)	reg=val
#define SaveRw(reg,val)	reg=val
#define SaveRd(reg,val)	reg=val

static INLINE Bit8s Fetchbs() {
	return Fetchb();
}
static INLINE Bit16s Fetchws() {
	return Fetchw();
}

static INLINE Bit32s Fetchds() {
	return Fetchd();
}


#define RUNEXCEPTION() {										\
	CPU_Exception(cpu.exception.which,cpu.exception.error);		\
	continue;													\
}

#define EXCEPTION(blah)										\
	{														\
		CPU_Exception(blah);								\
		continue;											\
	}

/* NTS: At first glance, this looks like code that will only fetch the delta for conditional jumps
 *      if the condition is true. Further examination shows that DOSBox's core has two different
 *      CS:IP variables, reg_ip and core.cseip which Fetchb() modifies. */
//TODO Could probably make all byte operands fast?
#define JumpCond16_b(COND) {						\
	Bit8s adj=Fetchbs();						\
	SAVEIP;								\
	if (COND) reg_ip+=adj;						\
	continue;							\
}

#define JumpCond16_w(COND) {						\
	Bit16s adj=Fetchws();						\
	SAVEIP;								\
	if (COND) reg_ip+=adj;						\
	continue;							\
}

#define JumpCond32_b(COND) {						\
	Bit8s adj=Fetchbs();						\
	SAVEIP;								\
	if (COND) reg_eip+=adj;						\
	continue;							\
}

#define JumpCond32_d(COND) {						\
	Bit32s adj=Fetchds();						\
	SAVEIP;								\
	if (COND) reg_eip+=adj;						\
	continue;							\
}


#define SETcc(cc)							\
	{								\
		GetRM;							\
		if (rm >= 0xc0 ) {GetEArb;*earb=(cc) ? 1 : 0;}		\
		else {GetEAa;SaveMb(eaa,(cc) ? 1 : 0);}			\
	}

#include "helpers.h"
#if CPU_CORE <= CPU_ARCHTYPE_8086
# include "table_ea_8086.h"
#else
# include "table_ea.h"
#endif
#include "../modrm.h"


