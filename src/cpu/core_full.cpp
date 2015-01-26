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

#include "dosbox.h"

#include "pic.h"
#include "regs.h"
#include "cpu.h"
#include "lazyflags.h"
#include "paging.h"
#include "fpu.h"
#include "debug.h"
#include "inout.h"
#include "callback.h"


typedef PhysPt EAPoint;
#define SegBase(c)	SegPhys(c)

#define LoadMb(off) mem_readb_inline(off)
#define LoadMw(off) mem_readw_inline(off)
#define LoadMd(off) mem_readd_inline(off)

#define LoadMbs(off) (Bit8s)(LoadMb(off))
#define LoadMws(off) (Bit16s)(LoadMw(off))
#define LoadMds(off) (Bit32s)(LoadMd(off))

#define SaveMb(off,val)	mem_writeb_inline(off,val)
#define SaveMw(off,val)	mem_writew_inline(off,val)
#define SaveMd(off,val)	mem_writed_inline(off,val)

#define LoadD(reg) reg
#define SaveD(reg,val)	reg=val



#include "core_full/loadwrite.h"
#include "core_full/support.h"
#include "core_full/optable.h"
#include "instructions.h"

#define EXCEPTION(blah)										\
	{														\
		Bit8u new_num=blah;									\
		CPU_Exception(new_num,0);							\
		continue;											\
	}

Bits CPU_Core_Normal_Trap_Run(void);

Bits CPU_Core_Full_Run(void) {
	static bool tf_warn=false;
	FullData inst;

	while (CPU_Cycles-->0) {
		cycle_count++;

		/* this core isn't written to emulate the Trap Flag. at least
		 * let the user know. some demos (Second Reality, Future Crew)
		 * use the Trap Flag to self-decrypt on execute, which is why
		 * they normally crash when run under core=full */
		if (GETFLAG(TF)) {
			if (!tf_warn) {
				tf_warn = true;
				LOG(LOG_CPU,LOG_WARN)("Trap Flag single-stepping not supported by full core. Using normal core for TF emulation.");
			}

			return CPU_Core_Normal_Trap_Run();
		}
		else {
			tf_warn = false;
		}

#if C_DEBUG		
#if C_HEAVY_DEBUG
		if (DEBUG_HeavyIsBreakpoint()) {
			FillFlags();
			return debugCallback;
		};
#endif
#endif
		LoadIP();
		inst.entry=cpu.code.big*0x200;
		inst.prefix=cpu.code.big;
restartopcode:
		inst.entry=(inst.entry & 0xffffff00) | Fetchb();
		inst.code=OpCodeTable[inst.entry];
		#include "core_full/load.h"
		#include "core_full/op.h"
		#include "core_full/save.h"
nextopcode:;
		SaveIP();
		continue;
illegalopcode:
		LOG(LOG_CPU,LOG_NORMAL)("Illegal opcode");
		CPU_Exception(0x6,0);
	}
	FillFlags();
	return CBRET_NONE;
}


void CPU_Core_Full_Init(void) {

}
