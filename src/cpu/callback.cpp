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


#include <stdlib.h>
#include <string.h>

#include "dosbox.h"
#include "callback.h"
#include "logging.h"
#include "bios.h"
#include "mem.h"
#include "cpu.h"

#if C_EMSCRIPTEN
# include <emscripten.h>
#endif

Bit16u CB_SEG=0,CB_SOFFSET=0;
extern Bitu vm86_fake_io_seg;
extern Bitu vm86_fake_io_off;

unsigned int last_callback = 0;

/* CallBack are located at 0xF000:0x1000  (see CB_SEG and CB_SOFFSET in callback.h)
   And they are 16 bytes each and you can define them to behave in certain ways like a
   far return or and IRET
*/

CallBack_Handler CallBack_Handlers[CB_MAX] = {NULL};
char* CallBack_Description[CB_MAX] = {NULL};

Bitu call_stop,call_default;
uint8_t call_idle;
Bitu call_priv_io;

static Bitu illegal_handler(void) {
	E_Exit("Illegal CallBack #%u Called",last_callback);
	return 1;
}

void DBG_CALLBACK_Dump(void) {
	LOG_MSG("Callbacks");
    for (Bitu i=0;i < CB_MAX;i++) {
        if (CallBack_Handlers[i] == &illegal_handler)
            continue;

        LOG_MSG("  [%u] func=%p desc='%s'",
            (unsigned int)i,
            (void*)((uintptr_t)CallBack_Handlers[i]), /* shut the compiler up by func -> uintptr_t -> void* conversion */
            CallBack_Description[i] != NULL ? CallBack_Description[i] : "");
    }
	LOG_MSG("--------------");
}

void CALLBACK_Dump(void) {
	LOG(LOG_CPU,LOG_DEBUG)("Callback dump");
    for (Bitu i=0;i < CB_MAX;i++) {
        if (CallBack_Handlers[i] == &illegal_handler)
            continue;

        LOG(LOG_CPU,LOG_DEBUG)("  [%u] func=%p desc='%s'",
            (unsigned int)i,
            (void*)((uintptr_t)CallBack_Handlers[i]), /* shut the compiler up by func -> uintptr_t -> void* conversion */
            CallBack_Description[i] != NULL ? CallBack_Description[i] : "");
    }
	LOG(LOG_CPU,LOG_DEBUG)("--------------");
}

void CALLBACK_Shutdown(void) {
	for (Bitu i=0;(i<CB_MAX);i++) {
		CallBack_Handlers[i] = &illegal_handler;
		CALLBACK_SetDescription(i,NULL);
	}
}

uint8_t CALLBACK_Allocate(void) {
	for (uint8_t i=1;(i<CB_MAX);i++) {
		if (CallBack_Handlers[i]==&illegal_handler) {
			if (CallBack_Description[i] != NULL) LOG_MSG("CALLBACK_Allocate() warning: empty slot still has description string!\n");
			CallBack_Handlers[i]=0;
			return i;
		}
	}
	E_Exit("CALLBACK:Can't allocate handler.");
	return 0;
}

void CALLBACK_DeAllocate(Bitu in) {
    assert(in != 0);
    assert(in < CB_MAX);

	CallBack_Handlers[in]=&illegal_handler;
	CALLBACK_SetDescription(in,NULL);
}


void CALLBACK_Idle(void) {
#if C_EMSCRIPTEN
    void GFX_Events();
    GFX_Events();
#endif

/* this makes the cpu execute instructions to handle irq's and then come back */
	Bitu oldIF=GETFLAG(IF);
	SETFLAGBIT(IF,true);
	Bit16u oldcs=SegValue(cs);
	Bit32u oldeip=reg_eip;
	SegSet16(cs,CB_SEG);
	reg_eip=CB_SOFFSET+call_idle*CB_SIZE;
	DOSBOX_RunMachine();
	reg_eip=oldeip;
	SegSet16(cs,oldcs);
	SETFLAGBIT(IF,oldIF);
	if (!CPU_CycleAutoAdjust && CPU_Cycles>0)
		CPU_Cycles=0;
}

void CALLBACK_IdleNoInts(void) {
#if C_EMSCRIPTEN
    void GFX_Events();
    GFX_Events();
#endif

/* this makes the cpu execute instructions to handle irq's and then come back */
//	Bitu oldIF=GETFLAG(IF);
//	SETFLAGBIT(IF,true);
	Bit16u oldcs=SegValue(cs);
	Bit32u oldeip=reg_eip;
	SegSet16(cs,CB_SEG);
	reg_eip=CB_SOFFSET+call_idle*CB_SIZE;
	DOSBOX_RunMachine();
	reg_eip=oldeip;
	SegSet16(cs,oldcs);
//	SETFLAGBIT(IF,oldIF);
	if (!CPU_CycleAutoAdjust && CPU_Cycles>0)
		CPU_Cycles=0;
}

static Bitu default_handler(void) {
	LOG(LOG_CPU,LOG_ERROR)("Illegal Unhandled Interrupt Called %X",lastint);
	return CBRET_NONE;
}

static Bitu stop_handler(void) {
	return CBRET_STOP;
}

Bitu FillFlags(void);

void CALLBACK_RunRealFarInt(Bit16u seg,Bit16u off) {
	FillFlags();

	reg_sp-=6;
	mem_writew(SegPhys(ss)+reg_sp,RealOff(CALLBACK_RealPointer(call_stop)));
	mem_writew(SegPhys(ss)+reg_sp+2,RealSeg(CALLBACK_RealPointer(call_stop)));
	mem_writew(SegPhys(ss)+reg_sp+4,(Bit16u)reg_flags);
	Bit32u oldeip=reg_eip;
	Bit16u oldcs=SegValue(cs);
	reg_eip=off;
	SegSet16(cs,seg);
	DOSBOX_RunMachine();
	reg_eip=oldeip;
	SegSet16(cs,oldcs);
}

void CALLBACK_RunRealFar(Bit16u seg,Bit16u off) {
	reg_sp-=4;
	mem_writew(SegPhys(ss)+reg_sp,RealOff(CALLBACK_RealPointer(call_stop)));
	mem_writew(SegPhys(ss)+reg_sp+2,RealSeg(CALLBACK_RealPointer(call_stop)));
	Bit32u oldeip=reg_eip;
	Bit16u oldcs=SegValue(cs);
	reg_eip=off;
	SegSet16(cs,seg);
	DOSBOX_RunMachine();
	reg_eip=oldeip;
	SegSet16(cs,oldcs);
}

void CALLBACK_RunRealInt_retcsip(uint8_t intnum,Bitu &rcs,Bitu &rip) {
	Bit32u oldeip=reg_eip;
	Bit16u oldcs=SegValue(cs);
	reg_eip=CB_SOFFSET+(CB_MAX*CB_SIZE)+(intnum*6U);
	SegSet16(cs,CB_SEG);
	DOSBOX_RunMachine();
	rcs = SegValue(cs);
	rip = reg_ip;
	reg_eip=oldeip;
	SegSet16(cs,oldcs);
}

void CALLBACK_RunRealInt(uint8_t intnum) {
	Bit32u oldeip=reg_eip;
	Bit16u oldcs=SegValue(cs);
	reg_eip=CB_SOFFSET+(CB_MAX*CB_SIZE)+(intnum*6U);
	SegSet16(cs,CB_SEG);
	DOSBOX_RunMachine();
	reg_eip=oldeip;
	SegSet16(cs,oldcs);
}

void CALLBACK_SZF(bool val) {
    Bit32u tempf;

    if (cpu.stack.big)
        tempf = mem_readd(SegPhys(ss)+reg_esp+8); // first word past FAR 32:32
    else
        tempf = mem_readw(SegPhys(ss)+reg_sp+4); // first word past FAR 16:16

    if (val) tempf |= FLAG_ZF;
    else tempf &= ~FLAG_ZF;

    if (cpu.stack.big)
        mem_writed(SegPhys(ss)+reg_esp+8,tempf);
    else
        mem_writew(SegPhys(ss)+reg_sp+4,(Bit16u)tempf);
}

void CALLBACK_SCF(bool val) {
    Bit32u tempf;

    if (cpu.stack.big)
        tempf = mem_readd(SegPhys(ss)+reg_esp+8); // first word past FAR 32:32
    else
        tempf = mem_readw(SegPhys(ss)+reg_sp+4); // first word past FAR 16:16

    if (val) tempf |= FLAG_CF;
    else tempf &= ~FLAG_CF;

    if (cpu.stack.big)
        mem_writed(SegPhys(ss)+reg_esp+8,tempf);
    else
        mem_writew(SegPhys(ss)+reg_sp+4,(Bit16u)tempf);
}

void CALLBACK_SIF(bool val) {
    Bit32u tempf;

    if (cpu.stack.big)
        tempf = mem_readd(SegPhys(ss)+reg_esp+8); // first word past FAR 32:32
    else
        tempf = mem_readw(SegPhys(ss)+reg_sp+4); // first word past FAR 16:16

    if (val) tempf |= FLAG_IF;
    else tempf &= ~FLAG_IF;

    if (cpu.stack.big)
        mem_writed(SegPhys(ss)+reg_esp+8,tempf);
    else
        mem_writew(SegPhys(ss)+reg_sp+4,(Bit16u)tempf);
}

void CALLBACK_SetDescription(Bitu nr, const char* descr) {
	if (CallBack_Description[nr]) delete[] CallBack_Description[nr];
	CallBack_Description[nr] = 0;

	if (descr != NULL) {
		CallBack_Description[nr] = new char[strlen(descr)+1];
		strcpy(CallBack_Description[nr],descr);
	}
}

const char* CALLBACK_GetDescription(Bitu nr) {
	if (nr>=CB_MAX) return 0;
	return CallBack_Description[nr];
}

Bitu CALLBACK_SetupExtra(Bitu callback, Bitu type, PhysPt physAddress, bool use_cb=true) {
	if (callback>=CB_MAX)
		return 0;
	switch (type) {
	case CB_RETN:
		if (use_cb) {
			phys_writeb(physAddress+0x00,(uint8_t)0xFE);	//GRP 4
			phys_writeb(physAddress+0x01,(uint8_t)0x38);	//Extra Callback instruction
			phys_writew(physAddress+0x02,(Bit16u)callback);	//The immediate word
			physAddress+=4;
		}
		phys_writeb(physAddress+0x00,(uint8_t)0xC3);		//A RETN Instruction
		return (use_cb?5:1);
	case CB_RETF:
		if (use_cb) {
			phys_writeb(physAddress+0x00,(uint8_t)0xFE);	//GRP 4
			phys_writeb(physAddress+0x01,(uint8_t)0x38);	//Extra Callback instruction
			phys_writew(physAddress+0x02,(Bit16u)callback);	//The immediate word
			physAddress+=4;
		}
		phys_writeb(physAddress+0x00,(uint8_t)0xCB);		//A RETF Instruction
		return (use_cb?5:1);
	case CB_RETF8:
		if (use_cb) {
			phys_writeb(physAddress+0x00,(uint8_t)0xFE);	//GRP 4
			phys_writeb(physAddress+0x01,(uint8_t)0x38);	//Extra Callback instruction
			phys_writew(physAddress+0x02,(Bit16u)callback);	//The immediate word
			physAddress+=4;
		}
		phys_writeb(physAddress+0x00,(uint8_t)0xCA);		//A RETF 8 Instruction
		phys_writew(physAddress+0x01,(Bit16u)0x0008);
		return (use_cb?7:3);
	case CB_RETF_STI:
		phys_writeb(physAddress+0x00,(uint8_t)0xFB);		//STI
		if (use_cb) {
			phys_writeb(physAddress+0x01,(uint8_t)0xFE);	//GRP 4
			phys_writeb(physAddress+0x02,(uint8_t)0x38);	//Extra Callback instruction
			phys_writew(physAddress+0x03,(Bit16u)callback);	//The immediate word
			physAddress+=4;
		}
		phys_writeb(physAddress+0x01,(uint8_t)0xCB);		//A RETF Instruction
		return (use_cb?6:2);
	case CB_RETF_CLI:
		phys_writeb(physAddress+0x00,(uint8_t)0xFA);		//CLI
		if (use_cb) {
			phys_writeb(physAddress+0x01,(uint8_t)0xFE);	//GRP 4
			phys_writeb(physAddress+0x02,(uint8_t)0x38);	//Extra Callback instruction
			phys_writew(physAddress+0x03,(Bit16u)callback);	//The immediate word
			physAddress+=4;
		}
		phys_writeb(physAddress+0x01,(uint8_t)0xCB);		//A RETF Instruction
		return (use_cb?6:2);
	case CB_IRET:
		if (use_cb) {
			phys_writeb(physAddress+0x00,(uint8_t)0xFE);	//GRP 4
			phys_writeb(physAddress+0x01,(uint8_t)0x38);	//Extra Callback instruction
			phys_writew(physAddress+0x02,(Bit16u)callback);		//The immediate word
			physAddress+=4;
		}
		phys_writeb(physAddress+0x00,(uint8_t)0xCF);		//An IRET Instruction
		return (use_cb?5:1);
	case CB_IRETD:
		if (use_cb) {
			phys_writeb(physAddress+0x00,(uint8_t)0xFE);	//GRP 4
			phys_writeb(physAddress+0x01,(uint8_t)0x38);	//Extra Callback instruction
			phys_writew(physAddress+0x02,(Bit16u)callback);		//The immediate word
			physAddress+=4;
		}
		phys_writeb(physAddress+0x00,(uint8_t)0x66);		//An IRETD Instruction
		phys_writeb(physAddress+0x01,(uint8_t)0xCF);
		return (use_cb?6:2);
	case CB_IRET_STI:
		phys_writeb(physAddress+0x00,(uint8_t)0xFB);		//STI
		if (use_cb) {
			phys_writeb(physAddress+0x01,(uint8_t)0xFE);	//GRP 4
			phys_writeb(physAddress+0x02,(uint8_t)0x38);	//Extra Callback instruction
			phys_writew(physAddress+0x03,(Bit16u)callback);	//The immediate word
			physAddress+=4;
		}
		phys_writeb(physAddress+0x01,(uint8_t)0xCF);		//An IRET Instruction
		return (use_cb?6:2);
	case CB_IRET_EOI_PIC1:
		if (use_cb) {
			phys_writeb(physAddress+0x00,(uint8_t)0xFE);	//GRP 4
			phys_writeb(physAddress+0x01,(uint8_t)0x38);	//Extra Callback instruction
			phys_writew(physAddress+0x02,(Bit16u)callback);		//The immediate word
			physAddress+=4;
		}
		phys_writeb(physAddress+0x00,(uint8_t)0x50);		// push ax
		phys_writeb(physAddress+0x01,(uint8_t)0xb0);		// mov al, 0x20
		phys_writeb(physAddress+0x02,(uint8_t)0x20);
		phys_writeb(physAddress+0x03,(uint8_t)0xe6);		// out 0x20, al (IBM) / out 0x00, al (PC-98)
		phys_writeb(physAddress+0x04,(uint8_t)(IS_PC98_ARCH ? 0x00 : 0x20));
		phys_writeb(physAddress+0x05,(uint8_t)0x58);		// pop ax
		phys_writeb(physAddress+0x06,(uint8_t)0xcf);		//An IRET Instruction
		return (use_cb?0x0b:0x07);
	case CB_IRQ0:	// timer int8
		phys_writeb(physAddress+0x00,(uint8_t)0xFB);		//STI
		if (use_cb) {
			phys_writeb(physAddress+0x01,(uint8_t)0xFE);	//GRP 4
			phys_writeb(physAddress+0x02,(uint8_t)0x38);	//Extra Callback instruction
			phys_writew(physAddress+0x03,(Bit16u)callback);		//The immediate word
			physAddress+=4;
		}
		phys_writeb(physAddress+0x01,(uint8_t)0x1e);		// push ds
		phys_writeb(physAddress+0x02,(uint8_t)0x50);		// push ax
		phys_writeb(physAddress+0x03,(uint8_t)0x52);		// push dx
		phys_writew(physAddress+0x04,(Bit16u)0x1ccd);	// int 1c
		phys_writeb(physAddress+0x06,(uint8_t)0xfa);		// cli
		phys_writew(physAddress+0x07,(Bit16u)0x20b0);	// mov al, 0x20
		phys_writew(physAddress+0x09,(Bit16u)(IS_PC98_ARCH ? 0x00e6 : 0x20e6));	// out 0x20, al / out 0x00, al (PC-98) (FIXME: PC-98 does not have INT 1Ch)
		phys_writeb(physAddress+0x0b,(uint8_t)0x5a);		// pop dx
		phys_writeb(physAddress+0x0c,(uint8_t)0x58);		// pop ax
		phys_writeb(physAddress+0x0d,(uint8_t)0x1f);		// pop ds
		phys_writeb(physAddress+0x0e,(uint8_t)0xcf);		//An IRET Instruction
		return (use_cb?0x13:0x0f);
	case CB_IRQ1:	// keyboard int9
		phys_writeb(physAddress+0x00,(uint8_t)0x50);			// push ax
        if (machine == MCH_PCJR || IS_PC98_ARCH) {
            /* NTS: NEC PC-98 does not have keyboard input on port 60h, it's a 8251 UART elsewhere.
             *
             *      IBM PCjr reads the infared input on NMI interrupt, which then calls INT 48h to
             *      translate to IBM PC/XT scan codes before passing AL directly to IRQ1 (INT 9).
             *      PCjr keyboard handlers, including games made for the PCjr, assume the scan code
             *      is in AL and do not read the I/O port */
            phys_writew(physAddress+0x01,(Bit16u)0x9090);		// nop, nop
        }
        else {
            phys_writew(physAddress+0x01,(Bit16u)0x60e4);		// in al, 0x60
        }
        if (IS_PC98_ARCH || IS_TANDY_ARCH) {
            phys_writew(physAddress+0x03,(Bit16u)0x9090);		// nop, nop
            phys_writeb(physAddress+0x05,(uint8_t)0x90);			// nop
            phys_writew(physAddress+0x06,(Bit16u)0x9090);		// nop, nop (PC-98 does not have INT 15h keyboard hook)
        }
        else {
            phys_writew(physAddress+0x03,(Bit16u)0x4fb4);		// mov ah, 0x4f
            phys_writeb(physAddress+0x05,(uint8_t)0xf9);			// stc
            phys_writew(physAddress+0x06,(Bit16u)0x15cd);		// int 15
        }

		if (use_cb) {
            if (IS_PC98_ARCH || IS_TANDY_ARCH)
                phys_writew(physAddress+0x08,(Bit16u)0x9090);	// nop nop
            else
                phys_writew(physAddress+0x08,(Bit16u)0x0473);	// jc skip

			phys_writeb(physAddress+0x0a,(uint8_t)0xFE);		//GRP 4
			phys_writeb(physAddress+0x0b,(uint8_t)0x38);		//Extra Callback instruction
			phys_writew(physAddress+0x0c,(Bit16u)callback);			//The immediate word
			// jump here to (skip):
			physAddress+=6;
		}
		phys_writeb(physAddress+0x08,(uint8_t)0xfa);			// cli
		phys_writew(physAddress+0x09,(Bit16u)0x20b0);		// mov al, 0x20
		phys_writew(physAddress+0x0b,(Bit16u)(IS_PC98_ARCH ? 0x00e6 : 0x20e6));		// out 0x20, al
		phys_writeb(physAddress+0x0d,(uint8_t)0x58);			// pop ax
		phys_writeb(physAddress+0x0e,(uint8_t)0xcf);			//An IRET Instruction
        phys_writeb(physAddress+0x0f,(uint8_t)0xfa);			// cli
        phys_writew(physAddress+0x10,(Bit16u)0x20b0);		// mov al, 0x20
        phys_writew(physAddress+0x12,(Bit16u)0x20e6);		// out 0x20, al
        phys_writeb(physAddress+0x14,(uint8_t)0x55);			// push bp
        phys_writew(physAddress+0x15,(Bit16u)0x05cd);		// int 5
        phys_writeb(physAddress+0x17,(uint8_t)0x5d);			// pop bp
        phys_writeb(physAddress+0x18,(uint8_t)0x58);			// pop ax
        phys_writeb(physAddress+0x19,(uint8_t)0xcf);			//An IRET Instruction
        return (use_cb ?0x20:0x1a);
	case CB_IRQ1_BREAK:	// return from int9, when Ctrl-Break is detected; invokes int 1b
		phys_writew(physAddress+0x00,(Bit16u)0x1bcd);		// int 1b
		phys_writeb(physAddress+0x02,(uint8_t)0xfa);		// cli
		if (use_cb) {
			phys_writeb(physAddress+0x03,(uint8_t)0xFE);	//GRP 4
			phys_writeb(physAddress+0x04,(uint8_t)0x38);	//Extra Callback instruction
			phys_writew(physAddress+0x05,(Bit16u)callback);		//The immediate word
			physAddress+=4;
		}
		phys_writew(physAddress+0x03,(Bit16u)0x20b0);		// mov al, 0x20
		phys_writew(physAddress+0x05,(Bit16u)(IS_PC98_ARCH ? 0x00e6 : 0x20e6));		// out 0x20, al
		phys_writeb(physAddress+0x07,(uint8_t)0x58);			// pop ax
		phys_writeb(physAddress+0x08,(uint8_t)0xcf);			//An IRET Instruction
		return (use_cb?0x0d:0x09);
	case CB_IRQ9:	// pic cascade interrupt
		if (use_cb) {
			phys_writeb(physAddress+0x00,(uint8_t)0xFE);	//GRP 4
			phys_writeb(physAddress+0x01,(uint8_t)0x38);	//Extra Callback instruction
			phys_writew(physAddress+0x02,(Bit16u)callback);		//The immediate word
			physAddress+=4;
		}
		phys_writeb(physAddress+0x00,(uint8_t)0x50);		// push ax
		phys_writew(physAddress+0x01,(Bit16u)0x61b0);	// mov al, 0x61
		phys_writew(physAddress+0x03,(Bit16u)0xa0e6);	// out 0xa0, al
		phys_writew(physAddress+0x05,(Bit16u)0x0acd);	// int a
		phys_writeb(physAddress+0x07,(uint8_t)0xfa);		// cli
		phys_writeb(physAddress+0x08,(uint8_t)0x58);		// pop ax
		phys_writeb(physAddress+0x09,(uint8_t)0xcf);		//An IRET Instruction
		return (use_cb?0x0e:0x0a);
	case CB_IRQ12:	// ps2 mouse int74
		if (!use_cb) E_Exit("int74 callback must implement a callback handler!");
		phys_writeb(physAddress++,(uint8_t)0xfb);		// sti
		phys_writeb(physAddress++,(uint8_t)0x1e);		// push ds
		phys_writeb(physAddress++,(uint8_t)0x06);		// push es
		if (CPU_ArchitectureType>=CPU_ARCHTYPE_386) {
			phys_writew(physAddress,(Bit16u)0x6066);// pushad
			physAddress += 2;
		}
		else if (CPU_ArchitectureType>=CPU_ARCHTYPE_80186) {
			phys_writeb(physAddress++,(uint8_t)0x60);	// pusha
		}
		else {
			// 8086-level tedium, PUSHA not available
			phys_writeb(physAddress++,(uint8_t)0x50);	// push ax
			phys_writeb(physAddress++,(uint8_t)0x51);	// push cx
			phys_writeb(physAddress++,(uint8_t)0x52);	// push dx
			phys_writeb(physAddress++,(uint8_t)0x53);	// push bx
			phys_writeb(physAddress++,(uint8_t)0x55);	// push bp
			phys_writeb(physAddress++,(uint8_t)0x56);	// push si
			phys_writeb(physAddress++,(uint8_t)0x57);	// push di
		}
		phys_writeb(physAddress++,(uint8_t)0xFE);		//GRP 4
		phys_writeb(physAddress++,(uint8_t)0x38);		//Extra Callback instruction
		phys_writew(physAddress,(Bit16u)callback);			//The immediate word
		physAddress += 2;
		phys_writeb(physAddress++,(uint8_t)0x50);		// push ax
		phys_writew(physAddress,(Bit16u)0x20b0);	// mov al, 0x20
		physAddress += 2;
		phys_writew(physAddress,(Bit16u)0xa0e6);	// out 0xa0, al
		physAddress += 2;
		phys_writew(physAddress,(Bit16u)0x20e6);	// out 0x20, al
		physAddress += 2;
		phys_writeb(physAddress++,(uint8_t)0x58);		// pop ax
		phys_writeb(physAddress++,(uint8_t)0xfc);		// cld
		phys_writeb(physAddress++,(uint8_t)0xCB);		//A RETF Instruction
		return 0x13;
	case CB_IRQ12_RET:	// ps2 mouse int74 return
		phys_writeb(physAddress++,(uint8_t)0xfa);		// cli
		phys_writew(physAddress,(Bit16u)0x20b0);	// mov al, 0x20
		physAddress += 2;
		phys_writew(physAddress,(Bit16u)0xa0e6);	// out 0xa0, al
		physAddress += 2;
		phys_writew(physAddress,(Bit16u)0x20e6);	// out 0x20, al
		physAddress += 2;
		if (use_cb) {
			phys_writeb(physAddress++,(uint8_t)0xFE);	//GRP 4
			phys_writeb(physAddress++,(uint8_t)0x38);	//Extra Callback instruction
			phys_writew(physAddress,(Bit16u)callback);		//The immediate word
			physAddress+=2;
		}
		if (CPU_ArchitectureType>=CPU_ARCHTYPE_386) {
			phys_writew(physAddress,(Bit16u)0x6166);// popad
			physAddress += 2;
		}
		else if (CPU_ArchitectureType>=CPU_ARCHTYPE_80186) {
			phys_writeb(physAddress++,(uint8_t)0x61);	// popa
		}
		else {
			// 8086-level tedium, POPA not available
			phys_writeb(physAddress++,(uint8_t)0x5F);	// pop di
			phys_writeb(physAddress++,(uint8_t)0x5E);	// pop si
			phys_writeb(physAddress++,(uint8_t)0x5D);	// pop bp
			phys_writeb(physAddress++,(uint8_t)0x5B);	// pop bx
			phys_writeb(physAddress++,(uint8_t)0x5A);	// pop dx
			phys_writeb(physAddress++,(uint8_t)0x59);	// pop cx
			phys_writeb(physAddress++,(uint8_t)0x58);	// pop ax
		}
		phys_writeb(physAddress++,(uint8_t)0x07);		// pop es
		phys_writeb(physAddress++,(uint8_t)0x1f);		// pop ds
		phys_writeb(physAddress++,(uint8_t)0xcf);		//An IRET Instruction
		return (use_cb?0x10:0x0c);
	case CB_IRQ6_PCJR:	// pcjr keyboard interrupt
		phys_writeb(physAddress+0x00,(uint8_t)0x50);			// push ax
		phys_writew(physAddress+0x01,(Bit16u)0x60e4);		// in al, 0x60
		phys_writew(physAddress+0x03,(Bit16u)0xe03c);		// cmp al, 0xe0
		if (use_cb) {
			phys_writew(physAddress+0x05,(Bit16u)0x0b74);	// je skip
			phys_writeb(physAddress+0x07,(uint8_t)0xFE);		//GRP 4
			phys_writeb(physAddress+0x08,(uint8_t)0x38);		//Extra Callback instruction
			phys_writew(physAddress+0x09,(Bit16u)callback);			//The immediate word
			physAddress+=4;
		} else {
			phys_writew(physAddress+0x05,(Bit16u)0x0774);	// je skip
		}
		phys_writeb(physAddress+0x07,(uint8_t)0x1e);			// push ds
		phys_writew(physAddress+0x08,(Bit16u)0x406a);		// push 0x0040
		phys_writeb(physAddress+0x0a,(uint8_t)0x1f);			// pop ds
		phys_writew(physAddress+0x0b,(Bit16u)0x09cd);		// int 9
		phys_writeb(physAddress+0x0d,(uint8_t)0x1f);			// pop ds
		// jump here to (skip):
		phys_writeb(physAddress+0x0e,(uint8_t)0xfa);			// cli
		phys_writew(physAddress+0x0f,(Bit16u)0x20b0);		// mov al, 0x20
		phys_writew(physAddress+0x11,(Bit16u)0x20e6);		// out 0x20, al
		phys_writeb(physAddress+0x13,(uint8_t)0x58);			// pop ax
		phys_writeb(physAddress+0x14,(uint8_t)0xcf);			//An IRET Instruction
		return (use_cb?0x19:0x15);
	case CB_MOUSE:
		phys_writew(physAddress+0x00,(Bit16u)0x07eb);		// jmp i33hd
		physAddress+=9;
		// jump here to (i33hd):
		if (use_cb) {
			phys_writeb(physAddress+0x00,(uint8_t)0xFE);	//GRP 4
			phys_writeb(physAddress+0x01,(uint8_t)0x38);	//Extra Callback instruction
			phys_writew(physAddress+0x02,(Bit16u)callback);		//The immediate word
			physAddress+=4;
		}
		phys_writeb(physAddress+0x00,(uint8_t)0xCF);		//An IRET Instruction
		return (use_cb?0x0e:0x0a);
	case CB_INT16:
		phys_writeb(physAddress+0x00,(uint8_t)0xFB);		//STI
		if (use_cb) {
			phys_writeb(physAddress+0x01,(uint8_t)0xFE);	//GRP 4
			phys_writeb(physAddress+0x02,(uint8_t)0x38);	//Extra Callback instruction
			phys_writew(physAddress+0x03,(Bit16u)callback);	//The immediate word
			physAddress+=4;
		}
		phys_writeb(physAddress+0x01,(uint8_t)0xCF);		//An IRET Instruction
		for (uint8_t i=0;i<=0x0b;i++) phys_writeb(physAddress+0x02+i,0x90);
		phys_writew(physAddress+0x0e,(Bit16u)0xedeb);	//jmp callback
		return (use_cb?0x10:0x0c);
	/*case CB_INT28:	// DOS idle
		phys_writeb(physAddress+0x00,(uint8_t)0xFB);		// STI
		phys_writeb(physAddress+0x01,(uint8_t)0xF4);		// HLT
		phys_writeb(physAddress+0x02,(uint8_t)0xcf);		// An IRET Instruction
		return (0x04);*/
	case CB_INT29:	// fast console output
        if (IS_PC98_ARCH) LOG_MSG("WARNING: CB_INT29 callback setup not appropriate for PC-98 mode (INT 10h no longer BIOS call)");
		if (use_cb) {
			phys_writeb(physAddress+0x00,(uint8_t)0xFE);	//GRP 4
			phys_writeb(physAddress+0x01,(uint8_t)0x38);	//Extra Callback instruction
			phys_writew(physAddress+0x02,(Bit16u)callback);		//The immediate word
			physAddress+=4;
		}
		phys_writeb(physAddress+0x00,(uint8_t)0x50);	// push ax
		phys_writeb(physAddress+0x01,(uint8_t)0x53);	// push bx
		phys_writew(physAddress+0x02,(Bit16u)0x0eb4);	// mov ah, 0x0e
		phys_writeb(physAddress+0x04,(uint8_t)0xbb);	// mov bx,
		phys_writew(physAddress+0x05,(Bit16u)0x0007);	// 0x0007
		phys_writew(physAddress+0x07,(Bit16u)0x10cd);	// int 10
		phys_writeb(physAddress+0x09,(uint8_t)0x5b);	// pop bx
		phys_writeb(physAddress+0x0a,(uint8_t)0x58);	// pop ax
		phys_writeb(physAddress+0x0b,(uint8_t)0xcf);	//An IRET Instruction
		return (use_cb?0x10:0x0c);
	case CB_HOOKABLE:
		phys_writeb(physAddress+0x00,(uint8_t)0xEB);		//jump near
		phys_writeb(physAddress+0x01,(uint8_t)0x03);		//offset
		phys_writeb(physAddress+0x02,(uint8_t)0x90);		//NOP
		phys_writeb(physAddress+0x03,(uint8_t)0x90);		//NOP
		phys_writeb(physAddress+0x04,(uint8_t)0x90);		//NOP
		if (use_cb) {
			phys_writeb(physAddress+0x05,(uint8_t)0xFE);	//GRP 4
			phys_writeb(physAddress+0x06,(uint8_t)0x38);	//Extra Callback instruction
			phys_writew(physAddress+0x07,(Bit16u)callback);		//The immediate word
			physAddress+=4;
		}
		phys_writeb(physAddress+0x05,(uint8_t)0xCB);		//A RETF Instruction
		return (use_cb?0x0a:0x06);
	case CB_TDE_IRET:	// TandyDAC end transfer
		if (use_cb) {
			phys_writeb(physAddress+0x00,(uint8_t)0xFE);	//GRP 4
			phys_writeb(physAddress+0x01,(uint8_t)0x38);	//Extra Callback instruction
			phys_writew(physAddress+0x02,(Bit16u)callback);		//The immediate word
			physAddress+=4;
		}
		phys_writeb(physAddress+0x00,(uint8_t)0x50);		// push ax
		phys_writeb(physAddress+0x01,(uint8_t)0xb8);		// mov ax, 0x91fb
		phys_writew(physAddress+0x02,(Bit16u)0x91fb);
		phys_writew(physAddress+0x04,(Bit16u)0x15cd);	// int 15
		phys_writeb(physAddress+0x06,(uint8_t)0xfa);		// cli
		phys_writew(physAddress+0x07,(Bit16u)0x20b0);	// mov al, 0x20
		phys_writew(physAddress+0x09,(Bit16u)0x20e6);	// out 0x20, al
		phys_writeb(physAddress+0x0b,(uint8_t)0x58);		// pop ax
		phys_writeb(physAddress+0x0c,(uint8_t)0xcf);		//An IRET Instruction
		return (use_cb?0x11:0x0d);
/*	case CB_IPXESR:		// IPX ESR
		if (!use_cb) E_Exit("ipx esr must implement a callback handler!");
		phys_writeb(physAddress+0x00,(uint8_t)0x1e);		// push ds
		phys_writeb(physAddress+0x01,(uint8_t)0x06);		// push es
		phys_writew(physAddress+0x02,(Bit16u)0xa00f);	// push fs
		phys_writew(physAddress+0x04,(Bit16u)0xa80f);	// push gs
		phys_writeb(physAddress+0x06,(uint8_t)0x60);		// pusha
		phys_writeb(physAddress+0x07,(uint8_t)0xFE);		//GRP 4
		phys_writeb(physAddress+0x08,(uint8_t)0x38);		//Extra Callback instruction
		phys_writew(physAddress+0x09,(Bit16u)callback);	//The immediate word
		phys_writeb(physAddress+0x0b,(uint8_t)0xCB);		//A RETF Instruction
		return 0x0c;
	case CB_IPXESR_RET:		// IPX ESR return
		if (use_cb) E_Exit("ipx esr return must not implement a callback handler!");
		phys_writeb(physAddress+0x00,(uint8_t)0xfa);		// cli
		phys_writew(physAddress+0x01,(Bit16u)0x20b0);	// mov al, 0x20
		phys_writew(physAddress+0x03,(Bit16u)0xa0e6);	// out 0xa0, al
		phys_writew(physAddress+0x05,(Bit16u)0x20e6);	// out 0x20, al
		phys_writeb(physAddress+0x07,(uint8_t)0x61);		// popa
		phys_writew(physAddress+0x08,(Bit16u)0xA90F);	// pop gs
		phys_writew(physAddress+0x0a,(Bit16u)0xA10F);	// pop fs
		phys_writeb(physAddress+0x0c,(uint8_t)0x07);		// pop es
		phys_writeb(physAddress+0x0d,(uint8_t)0x1f);		// pop ds
		phys_writeb(physAddress+0x0e,(uint8_t)0xcf);		//An IRET Instruction
		return 0x0f; */
	case CB_INT21:
		phys_writeb(physAddress+0x00,(uint8_t)0xFB);		//STI
		if (use_cb) {
			phys_writeb(physAddress+0x01,(uint8_t)0xFE);	//GRP 4
			phys_writeb(physAddress+0x02,(uint8_t)0x38);	//Extra Callback instruction
			phys_writew(physAddress+0x03,(Bit16u)callback);	//The immediate word
			physAddress+=4;
		}
		phys_writeb(physAddress+0x01,(uint8_t)0xCF);		//An IRET Instruction
		phys_writeb(physAddress+0x02,(uint8_t)0xCB);		//A RETF Instruction
		phys_writeb(physAddress+0x03,(uint8_t)0x51);		// push cx
		phys_writeb(physAddress+0x04,(uint8_t)0xB9);		// mov cx,
		phys_writew(physAddress+0x05,(Bit16u)0x0140);		// 0x140
		phys_writew(physAddress+0x07,(Bit16u)0xFEE2);		// loop $-2
		phys_writeb(physAddress+0x09,(uint8_t)0x59);		// pop cx
		phys_writeb(physAddress+0x0A,(uint8_t)0xCF);		//An IRET Instruction
		return (use_cb?15:11);
	case CB_INT13:
		phys_writeb(physAddress+0x00,(uint8_t)0xFB);		//STI
		if (use_cb) {
			phys_writeb(physAddress+0x01,(uint8_t)0xFE);	//GRP 4
			phys_writeb(physAddress+0x02,(uint8_t)0x38);	//Extra Callback instruction
			phys_writew(physAddress+0x03,(Bit16u)callback);	//The immediate word
			physAddress+=4;
		}
		phys_writeb(physAddress+0x01,(uint8_t)0xCF);		//An IRET Instruction
		phys_writew(physAddress+0x02,(Bit16u)0x0ECD);		// int 0e
		phys_writeb(physAddress+0x04,(uint8_t)0xCF);		//An IRET Instruction
		return (use_cb?9:5);
	case CB_VESA_WAIT:
		if (use_cb) E_Exit("VESA wait must not implement a callback handler!");
		phys_writeb(physAddress+0x00,(uint8_t)0xFB);		// sti
		phys_writeb(physAddress+0x01,(uint8_t)0x50);		// push ax
		phys_writeb(physAddress+0x02,(uint8_t)0x52);		// push dx
		phys_writeb(physAddress+0x03,(uint8_t)0xBA);		// mov dx,
		phys_writew(physAddress+0x04,(Bit16u)0x03DA);	// 0x3da
		phys_writeb(physAddress+0x06,(uint8_t)0xEC);		// in al,dx
		phys_writew(physAddress+0x07,(Bit16u)0x08A8);	// test al,8
		phys_writew(physAddress+0x09,(Bit16u)0xFB75);	// jne $-5
		phys_writeb(physAddress+0x0B,(uint8_t)0xEC);		// in al,dx
		phys_writew(physAddress+0x0C,(Bit16u)0x08A8);	// test al,8
		phys_writew(physAddress+0x0E,(Bit16u)0xFB74);	// je $-5
		phys_writeb(physAddress+0x10,(uint8_t)0x5A);		// pop dx
		phys_writeb(physAddress+0x11,(uint8_t)0x58);		// pop ax
		phys_writeb(physAddress+0x12,(uint8_t)0xCB);		//A RETF Instruction
		return 19;
	case CB_VESA_PM:
		if (use_cb) {
			phys_writeb(physAddress+0x00,(uint8_t)0xFE);	//GRP 4
			phys_writeb(physAddress+0x01,(uint8_t)0x38);	//Extra Callback instruction
			phys_writew(physAddress+0x02,(Bit16u)callback);	//The immediate word
			physAddress+=4;
		}
		phys_writew(physAddress+0x00,(Bit16u)0xC3F6);	// test bl,
		phys_writeb(physAddress+0x02,(uint8_t)0x80);		// 0x80
		phys_writew(physAddress+0x03,(Bit16u)0x1674);	// je $+22
		phys_writew(physAddress+0x05,(Bit16u)0x5066);	// push ax
		phys_writew(physAddress+0x07,(Bit16u)0x5266);	// push dx
		phys_writew(physAddress+0x09,(Bit16u)0xBA66);	// mov dx,
		phys_writew(physAddress+0x0B,(Bit16u)0x03DA);	// 0x3da
		phys_writeb(physAddress+0x0D,(uint8_t)0xEC);		// in al,dx
		phys_writew(physAddress+0x0E,(Bit16u)0x08A8);	// test al,8
		phys_writew(physAddress+0x10,(Bit16u)0xFB75);	// jne $-5
		phys_writeb(physAddress+0x12,(uint8_t)0xEC);		// in al,dx
		phys_writew(physAddress+0x13,(Bit16u)0x08A8);	// test al,8
		phys_writew(physAddress+0x15,(Bit16u)0xFB74);	// je $-5
		phys_writew(physAddress+0x17,(Bit16u)0x5A66);	// pop dx
		phys_writew(physAddress+0x19,(Bit16u)0x5866);	// pop ax
		if (use_cb)
			phys_writeb(physAddress+0x1B,(uint8_t)0xC3);	//A RETN Instruction
		return (use_cb?32:27);
	case CB_IRET_EOI_PIC2:
		if (use_cb) {
			phys_writeb(physAddress+0x00,(uint8_t)0xFE);	//GRP 4
			phys_writeb(physAddress+0x01,(uint8_t)0x38);	//Extra Callback instruction
			phys_writew(physAddress+0x02,(Bit16u)callback);		//The immediate word
			physAddress+=4;
		}
		phys_writeb(physAddress+0x00,(uint8_t)0x50);		// push ax
		phys_writeb(physAddress+0x01,(uint8_t)0xb0);		// mov al, 0x20
		phys_writeb(physAddress+0x02,(uint8_t)0x20);
		phys_writeb(physAddress+0x03,(uint8_t)0xe6);		// out 0xA0, al (IBM) / out 0x08, al (PC-98)
		phys_writeb(physAddress+0x04,(uint8_t)(IS_PC98_ARCH ? 0x08 : 0xA0));
		phys_writeb(physAddress+0x05,(uint8_t)0xe6);		// out 0x20, al (IBM) / out 0x00, al (PC-98)
		phys_writeb(physAddress+0x06,(uint8_t)(IS_PC98_ARCH ? 0x00 : 0x20));
		phys_writeb(physAddress+0x07,(uint8_t)0x58);		// pop ax
		phys_writeb(physAddress+0x08,(uint8_t)0xcf);		//An IRET Instruction
		return (use_cb?0x0d:0x09);
	case CB_CPM:
		phys_writeb(physAddress+0x00,(uint8_t)0x9C);		//PUSHF
		return CALLBACK_SetupExtra(callback,CB_INT21,physAddress+1,use_cb)+1;
	default:
		E_Exit("CALLBACK:Setup:Illegal type %u",(unsigned int)type);
	}
	return 0;
}

bool CALLBACK_Setup(Bitu callback,CallBack_Handler handler,Bitu type,const char* descr) {
	if (callback>=CB_MAX) return false;
	CALLBACK_SetupExtra(callback,type,CALLBACK_PhysPointer(callback)+0,(handler!=NULL));
	CallBack_Handlers[callback]=handler;
	CALLBACK_SetDescription(callback,descr);
	return true;
}

Bitu CALLBACK_Setup(Bitu callback,CallBack_Handler handler,Bitu type,PhysPt addr,const char* descr) {
	if (callback>=CB_MAX) return 0;
	Bitu csize=CALLBACK_SetupExtra(callback,type,addr,(handler!=NULL));
	if (csize>0) {
		CallBack_Handlers[callback]=handler;
		CALLBACK_SetDescription(callback,descr);
	}
	return csize;
}

void CALLBACK_RemoveSetup(Bitu callback) {
	if (MemBase == NULL) {
		/* avoid crash */
		return;
	}

	for (uint8_t i = 0;i < CB_SIZE;i++) {
		phys_writeb(CALLBACK_PhysPointer(callback)+i ,(uint8_t) 0x00);
	}
}

void CALLBACK_HandlerObject::Uninstall(){
	if(!installed) return;
	if(m_type == CALLBACK_HandlerObject::SETUP) {
		if(vectorhandler.installed && MemBase != NULL){
			//See if we are the current handler. if so restore the old one
			if(RealGetVec(vectorhandler.interrupt) == Get_RealPointer()) {
				RealSetVec(vectorhandler.interrupt,vectorhandler.old_vector);
			} else
				LOG(LOG_MISC,LOG_WARN)("Interrupt vector changed on %X %s",vectorhandler.interrupt,CALLBACK_GetDescription(m_callback));
		}
		CALLBACK_RemoveSetup(m_callback);
	} else if(m_type == CALLBACK_HandlerObject::SETUPAT){
		E_Exit("Callback:SETUP at not handled yet.");
	} else if(m_type == CALLBACK_HandlerObject::NONE){
		//Do nothing. Merely DeAllocate the callback
	} else E_Exit("what kind of callback is this!");
	if(CallBack_Description[m_callback]) delete [] CallBack_Description[m_callback];
	CallBack_Description[m_callback] = 0;
	CALLBACK_DeAllocate(m_callback);
	installed=false;
}

CALLBACK_HandlerObject::~CALLBACK_HandlerObject(){
	Uninstall();
}

void CALLBACK_HandlerObject::Install(CallBack_Handler handler,Bitu type,const char* description){
	if(!installed) {
		installed=true;
		m_type=SETUP;
		m_callback=CALLBACK_Allocate();
		CALLBACK_Setup(m_callback,handler,type,description);
	} else E_Exit("Callback handler object already installed");
}
void CALLBACK_HandlerObject::Install(CallBack_Handler handler,Bitu type,PhysPt addr,const char* description){
	if(!installed) {
		installed=true;
		m_type=SETUP;
		m_callback=CALLBACK_Allocate();
		CALLBACK_Setup(m_callback,handler,type,addr,description);
	} else E_Exit("Callback handler object already installed");
}

void CALLBACK_HandlerObject::Allocate(CallBack_Handler handler,const char* description) {
	if(!installed) {
		installed=true;
		m_type=NONE;
		m_callback=CALLBACK_Allocate();
		CALLBACK_SetDescription(m_callback,description);
		CallBack_Handlers[m_callback]=handler;
	} else E_Exit("Callback handler object already installed");
}

void CALLBACK_HandlerObject::Set_RealVec(uint8_t vec,bool reinstall){
	if(!vectorhandler.installed || reinstall) {
		vectorhandler.installed=true;
		vectorhandler.interrupt=vec;
		RealSetVec(vec,Get_RealPointer(),vectorhandler.old_vector);
	} else E_Exit ("double usage of vector handler");
}

extern bool custom_bios;

void CALLBACK_Init() {
	{
		/* NTS: Layout of the callback area:
		 *
		 * CB_MAX entries CB_SIZE each, where executable x86 code is written per callback,
		 * followed by 256 entries 6 bytes each corresponding to an interrupt call */
		Bitu o;

		LOG(LOG_MISC,LOG_DEBUG)("Initializing DOSBox callback instruction system");

		o = ROMBIOS_GetMemory((CB_MAX*CB_SIZE)+(256*6),"DOSBox callback area",/*align*/4);
		if (o == 0) E_Exit("Cannot allocate callback area");
		CB_SOFFSET = o&0xFFFF;
		CB_SEG = (o>>4)&0xF000;
		if (((Bitu)CB_SOFFSET + (CB_MAX*CB_SIZE) + (256*6)) > 0x10000) E_Exit("Callback area spans 64KB segment");

		o = ROMBIOS_GetMemory(14/*2+2+3+2+2+3*/,"DOSBox vm86 hack",/*align*/4);
		if (o == 0) E_Exit("Cannot allocate vm86 hack");
		vm86_fake_io_off = o&0xFFFF;
		vm86_fake_io_seg = (o>>4)&0xF000;
		if ((vm86_fake_io_off+14) > 0x1000000) E_Exit("vm86 area spans 64KB segment");
	}

	LOG(LOG_CPU,LOG_DEBUG)("Callback area starts at %04x:%04x",CB_SEG,CB_SOFFSET);

	Bit16u i;
	for (i=0;i<CB_MAX;i++) {
		CallBack_Handlers[i]=&illegal_handler;
		CallBack_Description[i]=NULL;
	}

	/* Setup the Stop Handler */
	call_stop=CALLBACK_Allocate();
	CallBack_Handlers[call_stop]=stop_handler;
	CALLBACK_SetDescription(call_stop,"stop");
	phys_writeb(CALLBACK_PhysPointer(call_stop)+0,0xFE);
	phys_writeb(CALLBACK_PhysPointer(call_stop)+1,0x38);
	phys_writew(CALLBACK_PhysPointer(call_stop)+2,(Bit16u)call_stop);

	/* Setup the idle handler */
	call_idle=CALLBACK_Allocate();
	CallBack_Handlers[call_idle]=stop_handler;
	CALLBACK_SetDescription(call_idle,"idle");
	for (i=0;i<=11;i++) phys_writeb(CALLBACK_PhysPointer(call_idle)+i,0x90);
	phys_writeb(CALLBACK_PhysPointer(call_idle)+12,0xFE);
	phys_writeb(CALLBACK_PhysPointer(call_idle)+13,0x38);
	phys_writew(CALLBACK_PhysPointer(call_idle)+14,(Bit16u)call_idle);

	/* Default handlers for unhandled interrupts that have to be non-null */
	call_default=CALLBACK_Allocate();
	CALLBACK_Setup(call_default,&default_handler,CB_IRET,"default");

	/* Setup block of 0xCD 0xxx instructions */
	PhysPt rint_base=CALLBACK_GetBase()+CB_MAX*CB_SIZE;
	for (i=0;i<=0xff;i++) {
		phys_writeb(rint_base,0xCD);
		phys_writeb(rint_base+1,(uint8_t)i);
		phys_writeb(rint_base+2,0xFE);
		phys_writeb(rint_base+3,0x38);
		phys_writew(rint_base+4,(Bit16u)call_stop);
		rint_base+=6;

	}

	call_priv_io=CALLBACK_Allocate();

	// virtualizable in-out opcodes
	phys_writeb(CALLBACK_PhysPointer(call_priv_io)+0x00,(uint8_t)0xec);	// in al, dx
	phys_writeb(CALLBACK_PhysPointer(call_priv_io)+0x01,(uint8_t)0xcb);	// retf
	phys_writeb(CALLBACK_PhysPointer(call_priv_io)+0x02,(uint8_t)0xed);	// in ax, dx
	phys_writeb(CALLBACK_PhysPointer(call_priv_io)+0x03,(uint8_t)0xcb);	// retf
	phys_writeb(CALLBACK_PhysPointer(call_priv_io)+0x04,(uint8_t)0x66);	// in eax, dx
	phys_writeb(CALLBACK_PhysPointer(call_priv_io)+0x05,(uint8_t)0xed);
	phys_writeb(CALLBACK_PhysPointer(call_priv_io)+0x06,(uint8_t)0xcb);	// retf

	phys_writeb(CALLBACK_PhysPointer(call_priv_io)+0x08,(uint8_t)0xee);	// out dx, al
	phys_writeb(CALLBACK_PhysPointer(call_priv_io)+0x09,(uint8_t)0xcb);	// retf
	phys_writeb(CALLBACK_PhysPointer(call_priv_io)+0x0a,(uint8_t)0xef);	// out dx, ax
	phys_writeb(CALLBACK_PhysPointer(call_priv_io)+0x0b,(uint8_t)0xcb);	// retf
	phys_writeb(CALLBACK_PhysPointer(call_priv_io)+0x0c,(uint8_t)0x66);	// out dx, eax
	phys_writeb(CALLBACK_PhysPointer(call_priv_io)+0x0d,(uint8_t)0xef);
	phys_writeb(CALLBACK_PhysPointer(call_priv_io)+0x0e,(uint8_t)0xcb);	// retf
}
