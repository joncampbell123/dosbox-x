/*
 *  Copyright (C) 2002-2010  The DOSBox Team
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

/* $Id: callback.cpp,v 1.42 2009-08-23 17:24:54 c2woody Exp $ */

#include <stdlib.h>
#include <string.h>

#include "dosbox.h"
#include "callback.h"
#include "mem.h"
#include "cpu.h"

/* CallBack are located at 0xF000:0x1000  (see CB_SEG and CB_SOFFSET in callback.h)
   And they are 16 bytes each and you can define them to behave in certain ways like a
   far return or and IRET
*/

CallBack_Handler CallBack_Handlers[CB_MAX];
char* CallBack_Description[CB_MAX];

static Bitu call_stop,call_idle,call_default,call_default2;
Bitu call_priv_io;

static Bitu illegal_handler(void) {
	E_Exit("Illegal CallBack Called");
	return 1;
}

Bitu CALLBACK_Allocate(void) {
	for (Bitu i=1;(i<CB_MAX);i++) {
		if (CallBack_Handlers[i]==&illegal_handler) {
			CallBack_Handlers[i]=0;
			return i;	
		}
	}
	E_Exit("CALLBACK:Can't allocate handler.");
	return 0;
}

void CALLBACK_DeAllocate(Bitu in) {
	CallBack_Handlers[in]=&illegal_handler;
}

	
void CALLBACK_Idle(void) {
/* this makes the cpu execute instructions to handle irq's and then come back */
	Bitu oldIF=GETFLAG(IF);
	SETFLAGBIT(IF,true);
	Bit16u oldcs=SegValue(cs);
	Bit32u oldeip=reg_eip;
	SegSet16(cs,CB_SEG);
	reg_eip=call_idle*CB_SIZE;
	DOSBOX_RunMachine();
	reg_eip=oldeip;
	SegSet16(cs,oldcs);
	SETFLAGBIT(IF,oldIF);
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

void CALLBACK_RunRealInt(Bit8u intnum) {
	Bit32u oldeip=reg_eip;
	Bit16u oldcs=SegValue(cs);
	reg_eip=CB_SOFFSET+(CB_MAX*CB_SIZE)+(intnum*6);
	SegSet16(cs,CB_SEG);
	DOSBOX_RunMachine();
	reg_eip=oldeip;
	SegSet16(cs,oldcs);
}

void CALLBACK_SZF(bool val) {
	Bit16u tempf = mem_readw(SegPhys(ss)+reg_sp+4); 
	if (val) tempf |= FLAG_ZF; 
	else tempf &= ~FLAG_ZF; 
	mem_writew(SegPhys(ss)+reg_sp+4,tempf); 
}

void CALLBACK_SCF(bool val) {
	Bit16u tempf = mem_readw(SegPhys(ss)+reg_sp+4); 
	if (val) tempf |= FLAG_CF; 
	else tempf &= ~FLAG_CF; 
	mem_writew(SegPhys(ss)+reg_sp+4,tempf); 
}

void CALLBACK_SIF(bool val) {
	Bit16u tempf = mem_readw(SegPhys(ss)+reg_sp+4); 
	if (val) tempf |= FLAG_IF; 
	else tempf &= ~FLAG_IF; 
	mem_writew(SegPhys(ss)+reg_sp+4,tempf); 
}

void CALLBACK_SetDescription(Bitu nr, const char* descr) {
	if (descr) {
		CallBack_Description[nr] = new char[strlen(descr)+1];
		strcpy(CallBack_Description[nr],descr);
	} else
		CallBack_Description[nr] = 0;
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
			phys_writeb(physAddress+0x00,(Bit8u)0xFE);	//GRP 4
			phys_writeb(physAddress+0x01,(Bit8u)0x38);	//Extra Callback instruction
			phys_writew(physAddress+0x02,(Bit16u)callback);	//The immediate word
			physAddress+=4;
		}
		phys_writeb(physAddress+0x00,(Bit8u)0xC3);		//A RETN Instruction
		return (use_cb?5:1);
	case CB_RETF:
		if (use_cb) {
			phys_writeb(physAddress+0x00,(Bit8u)0xFE);	//GRP 4
			phys_writeb(physAddress+0x01,(Bit8u)0x38);	//Extra Callback instruction
			phys_writew(physAddress+0x02,(Bit16u)callback);	//The immediate word
			physAddress+=4;
		}
		phys_writeb(physAddress+0x00,(Bit8u)0xCB);		//A RETF Instruction
		return (use_cb?5:1);
	case CB_RETF8:
		if (use_cb) {
			phys_writeb(physAddress+0x00,(Bit8u)0xFE);	//GRP 4
			phys_writeb(physAddress+0x01,(Bit8u)0x38);	//Extra Callback instruction
			phys_writew(physAddress+0x02,(Bit16u)callback);	//The immediate word
			physAddress+=4;
		}
		phys_writeb(physAddress+0x00,(Bit8u)0xCA);		//A RETF 8 Instruction
		phys_writew(physAddress+0x01,(Bit16u)0x0008);
		return (use_cb?7:3);
	case CB_IRET:
		if (use_cb) {
			phys_writeb(physAddress+0x00,(Bit8u)0xFE);	//GRP 4
			phys_writeb(physAddress+0x01,(Bit8u)0x38);	//Extra Callback instruction
			phys_writew(physAddress+0x02,(Bit16u)callback);		//The immediate word
			physAddress+=4;
		}
		phys_writeb(physAddress+0x00,(Bit8u)0xCF);		//An IRET Instruction
		return (use_cb?5:1);
	case CB_IRETD:
		if (use_cb) {
			phys_writeb(physAddress+0x00,(Bit8u)0xFE);	//GRP 4
			phys_writeb(physAddress+0x01,(Bit8u)0x38);	//Extra Callback instruction
			phys_writew(physAddress+0x02,(Bit16u)callback);		//The immediate word
			physAddress+=4;
		}
		phys_writeb(physAddress+0x00,(Bit8u)0x66);		//An IRETD Instruction
		phys_writeb(physAddress+0x01,(Bit8u)0xCF);
		return (use_cb?6:2);
	case CB_IRET_STI:
		phys_writeb(physAddress+0x00,(Bit8u)0xFB);		//STI
		if (use_cb) {
			phys_writeb(physAddress+0x01,(Bit8u)0xFE);	//GRP 4
			phys_writeb(physAddress+0x02,(Bit8u)0x38);	//Extra Callback instruction
			phys_writew(physAddress+0x03,(Bit16u)callback);	//The immediate word
			physAddress+=4;
		}
		phys_writeb(physAddress+0x01,(Bit8u)0xCF);		//An IRET Instruction
		return (use_cb?6:2);
	case CB_IRET_EOI_PIC1:
		if (use_cb) {
			phys_writeb(physAddress+0x00,(Bit8u)0xFE);	//GRP 4
			phys_writeb(physAddress+0x01,(Bit8u)0x38);	//Extra Callback instruction
			phys_writew(physAddress+0x02,(Bit16u)callback);		//The immediate word
			physAddress+=4;
		}
		phys_writeb(physAddress+0x00,(Bit8u)0x50);		// push ax
		phys_writeb(physAddress+0x01,(Bit8u)0xb0);		// mov al, 0x20
		phys_writeb(physAddress+0x02,(Bit8u)0x20);
		phys_writeb(physAddress+0x03,(Bit8u)0xe6);		// out 0x20, al
		phys_writeb(physAddress+0x04,(Bit8u)0x20);
		phys_writeb(physAddress+0x05,(Bit8u)0x58);		// pop ax
		phys_writeb(physAddress+0x06,(Bit8u)0xcf);		//An IRET Instruction
		return (use_cb?0x0b:0x07);
	case CB_IRQ0:	// timer int8
		if (use_cb) {
			phys_writeb(physAddress+0x00,(Bit8u)0xFE);	//GRP 4
			phys_writeb(physAddress+0x01,(Bit8u)0x38);	//Extra Callback instruction
			phys_writew(physAddress+0x02,(Bit16u)callback);		//The immediate word
			physAddress+=4;
		}
		phys_writeb(physAddress+0x00,(Bit8u)0x50);		// push ax
		phys_writeb(physAddress+0x01,(Bit8u)0x52);		// push dx
		phys_writeb(physAddress+0x02,(Bit8u)0x1e);		// push ds
		phys_writew(physAddress+0x03,(Bit16u)0x1ccd);	// int 1c
		phys_writeb(physAddress+0x05,(Bit8u)0xfa);		// cli
		phys_writeb(physAddress+0x06,(Bit8u)0x1f);		// pop ds
		phys_writeb(physAddress+0x07,(Bit8u)0x5a);		// pop dx
		phys_writew(physAddress+0x08,(Bit16u)0x20b0);	// mov al, 0x20
		phys_writew(physAddress+0x0a,(Bit16u)0x20e6);	// out 0x20, al
		phys_writeb(physAddress+0x0c,(Bit8u)0x58);		// pop ax
		phys_writeb(physAddress+0x0d,(Bit8u)0xcf);		//An IRET Instruction
		return (use_cb?0x12:0x0e);
	case CB_IRQ1:	// keyboard int9
		phys_writeb(physAddress+0x00,(Bit8u)0x50);			// push ax
		phys_writew(physAddress+0x01,(Bit16u)0x60e4);		// in al, 0x60
		phys_writew(physAddress+0x03,(Bit16u)0x4fb4);		// mov ah, 0x4f
		phys_writeb(physAddress+0x05,(Bit8u)0xf9);			// stc
		phys_writew(physAddress+0x06,(Bit16u)0x15cd);		// int 15
		if (use_cb) {
			phys_writew(physAddress+0x08,(Bit16u)0x0473);	// jc skip
			phys_writeb(physAddress+0x0a,(Bit8u)0xFE);		//GRP 4
			phys_writeb(physAddress+0x0b,(Bit8u)0x38);		//Extra Callback instruction
			phys_writew(physAddress+0x0c,(Bit16u)callback);			//The immediate word
			// jump here to (skip):
			physAddress+=6;
		}
		phys_writeb(physAddress+0x08,(Bit8u)0xfa);			// cli
		phys_writew(physAddress+0x09,(Bit16u)0x20b0);		// mov al, 0x20
		phys_writew(physAddress+0x0b,(Bit16u)0x20e6);		// out 0x20, al
		phys_writeb(physAddress+0x0d,(Bit8u)0x58);			// pop ax
		phys_writeb(physAddress+0x0e,(Bit8u)0xcf);			//An IRET Instruction
		return (use_cb?0x15:0x0f);
	case CB_IRQ9:	// pic cascade interrupt
		if (use_cb) {
			phys_writeb(physAddress+0x00,(Bit8u)0xFE);	//GRP 4
			phys_writeb(physAddress+0x01,(Bit8u)0x38);	//Extra Callback instruction
			phys_writew(physAddress+0x02,(Bit16u)callback);		//The immediate word
			physAddress+=4;
		}
		phys_writeb(physAddress+0x00,(Bit8u)0x50);		// push ax
		phys_writew(physAddress+0x01,(Bit16u)0x61b0);	// mov al, 0x61
		phys_writew(physAddress+0x03,(Bit16u)0xa0e6);	// out 0xa0, al
		phys_writew(physAddress+0x05,(Bit16u)0x0acd);	// int a
		phys_writeb(physAddress+0x07,(Bit8u)0xfa);		// cli
		phys_writeb(physAddress+0x08,(Bit8u)0x58);		// pop ax
		phys_writeb(physAddress+0x09,(Bit8u)0xcf);		//An IRET Instruction
		return (use_cb?0x0e:0x0a);
	case CB_IRQ12:	// ps2 mouse int74
		if (!use_cb) E_Exit("int74 callback must implement a callback handler!");
		phys_writeb(physAddress+0x00,(Bit8u)0x1e);		// push ds
		phys_writeb(physAddress+0x01,(Bit8u)0x06);		// push es
		phys_writew(physAddress+0x02,(Bit16u)0x6066);	// pushad
		phys_writeb(physAddress+0x04,(Bit8u)0xfc);		// cld
		phys_writeb(physAddress+0x05,(Bit8u)0xfb);		// sti
		phys_writeb(physAddress+0x06,(Bit8u)0xFE);		//GRP 4
		phys_writeb(physAddress+0x07,(Bit8u)0x38);		//Extra Callback instruction
		phys_writew(physAddress+0x08,(Bit16u)callback);			//The immediate word
		return 0x0a;
	case CB_IRQ12_RET:	// ps2 mouse int74 return
		if (use_cb) {
			phys_writeb(physAddress+0x00,(Bit8u)0xFE);	//GRP 4
			phys_writeb(physAddress+0x01,(Bit8u)0x38);	//Extra Callback instruction
			phys_writew(physAddress+0x02,(Bit16u)callback);		//The immediate word
			physAddress+=4;
		}
		phys_writeb(physAddress+0x00,(Bit8u)0xfa);		// cli
		phys_writew(physAddress+0x01,(Bit16u)0x20b0);	// mov al, 0x20
		phys_writew(physAddress+0x03,(Bit16u)0xa0e6);	// out 0xa0, al
		phys_writew(physAddress+0x05,(Bit16u)0x20e6);	// out 0x20, al
		phys_writew(physAddress+0x07,(Bit16u)0x6166);	// popad
		phys_writeb(physAddress+0x09,(Bit8u)0x07);		// pop es
		phys_writeb(physAddress+0x0a,(Bit8u)0x1f);		// pop ds
		phys_writeb(physAddress+0x0b,(Bit8u)0xcf);		//An IRET Instruction
		return (use_cb?0x10:0x0c);
	case CB_IRQ6_PCJR:	// pcjr keyboard interrupt
		phys_writeb(physAddress+0x00,(Bit8u)0x50);			// push ax
		phys_writew(physAddress+0x01,(Bit16u)0x60e4);		// in al, 0x60
		phys_writew(physAddress+0x03,(Bit16u)0xe03c);		// cmp al, 0xe0
		if (use_cb) {
			phys_writew(physAddress+0x05,(Bit16u)0x0674);	// je skip
			phys_writeb(physAddress+0x07,(Bit8u)0xFE);		//GRP 4
			phys_writeb(physAddress+0x08,(Bit8u)0x38);		//Extra Callback instruction
			phys_writew(physAddress+0x09,(Bit16u)callback);			//The immediate word
			physAddress+=4;
		} else {
			phys_writew(physAddress+0x05,(Bit16u)0x0274);	// je skip
		}
		phys_writew(physAddress+0x07,(Bit16u)0x09cd);		// int 9
		// jump here to (skip):
		phys_writeb(physAddress+0x09,(Bit8u)0xfa);			// cli
		phys_writew(physAddress+0x0a,(Bit16u)0x20b0);		// mov al, 0x20
		phys_writew(physAddress+0x0c,(Bit16u)0x20e6);		// out 0x20, al
		phys_writeb(physAddress+0x0e,(Bit8u)0x58);			// pop ax
		phys_writeb(physAddress+0x0f,(Bit8u)0xcf);			//An IRET Instruction
		return (use_cb?0x14:0x10);
	case CB_MOUSE:
		phys_writew(physAddress+0x00,(Bit16u)0x07eb);		// jmp i33hd
		physAddress+=9;
		// jump here to (i33hd):
		if (use_cb) {
			phys_writeb(physAddress+0x00,(Bit8u)0xFE);	//GRP 4
			phys_writeb(physAddress+0x01,(Bit8u)0x38);	//Extra Callback instruction
			phys_writew(physAddress+0x02,(Bit16u)callback);		//The immediate word
			physAddress+=4;
		}
		phys_writeb(physAddress+0x00,(Bit8u)0xCF);		//An IRET Instruction
		return (use_cb?0x0e:0x0a);
	case CB_INT16:
		phys_writeb(physAddress+0x00,(Bit8u)0xFB);		//STI
		if (use_cb) {
			phys_writeb(physAddress+0x01,(Bit8u)0xFE);	//GRP 4
			phys_writeb(physAddress+0x02,(Bit8u)0x38);	//Extra Callback instruction
			phys_writew(physAddress+0x03,(Bit16u)callback);	//The immediate word
			physAddress+=4;
		}
		phys_writeb(physAddress+0x01,(Bit8u)0xCF);		//An IRET Instruction
		for (Bitu i=0;i<=0x0b;i++) phys_writeb(physAddress+0x02+i,0x90);
		phys_writew(physAddress+0x0e,(Bit16u)0xedeb);	//jmp callback
		return (use_cb?0x10:0x0c);
	case CB_INT29:	// fast console output
		if (use_cb) {
			phys_writeb(physAddress+0x00,(Bit8u)0xFE);	//GRP 4
			phys_writeb(physAddress+0x01,(Bit8u)0x38);	//Extra Callback instruction
			phys_writew(physAddress+0x02,(Bit16u)callback);		//The immediate word
			physAddress+=4;
		}
		phys_writeb(physAddress+0x00,(Bit8u)0x50);		// push ax
		phys_writew(physAddress+0x01,(Bit16u)0x0eb4);	// mov ah, 0x0e
		phys_writew(physAddress+0x03,(Bit16u)0x10cd);	// int 10
		phys_writeb(physAddress+0x05,(Bit8u)0x58);		// pop ax
		phys_writeb(physAddress+0x06,(Bit8u)0xcf);		//An IRET Instruction
		return (use_cb?0x0b:0x07);
	case CB_HOOKABLE:
		phys_writeb(physAddress+0x00,(Bit8u)0xEB);		//jump near
		phys_writeb(physAddress+0x01,(Bit8u)0x03);		//offset
		phys_writeb(physAddress+0x02,(Bit8u)0x90);		//NOP
		phys_writeb(physAddress+0x03,(Bit8u)0x90);		//NOP
		phys_writeb(physAddress+0x04,(Bit8u)0x90);		//NOP
		if (use_cb) {
			phys_writeb(physAddress+0x05,(Bit8u)0xFE);	//GRP 4
			phys_writeb(physAddress+0x06,(Bit8u)0x38);	//Extra Callback instruction
			phys_writew(physAddress+0x07,(Bit16u)callback);		//The immediate word
			physAddress+=4;
		}
		phys_writeb(physAddress+0x05,(Bit8u)0xCB);		//A RETF Instruction
		return (use_cb?0x0a:0x06);
	case CB_TDE_IRET:	// TandyDAC end transfer
		if (use_cb) {
			phys_writeb(physAddress+0x00,(Bit8u)0xFE);	//GRP 4
			phys_writeb(physAddress+0x01,(Bit8u)0x38);	//Extra Callback instruction
			phys_writew(physAddress+0x02,(Bit16u)callback);		//The immediate word
			physAddress+=4;
		}
		phys_writeb(physAddress+0x00,(Bit8u)0x50);		// push ax
		phys_writeb(physAddress+0x01,(Bit8u)0xb8);		// mov ax, 0x91fb
		phys_writew(physAddress+0x02,(Bit16u)0x91fb);
		phys_writew(physAddress+0x04,(Bit16u)0x15cd);	// int 15
		phys_writeb(physAddress+0x06,(Bit8u)0xfa);		// cli
		phys_writew(physAddress+0x07,(Bit16u)0x20b0);	// mov al, 0x20
		phys_writew(physAddress+0x09,(Bit16u)0x20e6);	// out 0x20, al
		phys_writeb(physAddress+0x0b,(Bit8u)0x58);		// pop ax
		phys_writeb(physAddress+0x0c,(Bit8u)0xcf);		//An IRET Instruction
		return (use_cb?0x11:0x0d);
/*	case CB_IPXESR:		// IPX ESR
		if (!use_cb) E_Exit("ipx esr must implement a callback handler!");
		phys_writeb(physAddress+0x00,(Bit8u)0x1e);		// push ds
		phys_writeb(physAddress+0x01,(Bit8u)0x06);		// push es
		phys_writew(physAddress+0x02,(Bit16u)0xa00f);	// push fs
		phys_writew(physAddress+0x04,(Bit16u)0xa80f);	// push gs
		phys_writeb(physAddress+0x06,(Bit8u)0x60);		// pusha
		phys_writeb(physAddress+0x07,(Bit8u)0xFE);		//GRP 4
		phys_writeb(physAddress+0x08,(Bit8u)0x38);		//Extra Callback instruction
		phys_writew(physAddress+0x09,(Bit16u)callback);	//The immediate word
		phys_writeb(physAddress+0x0b,(Bit8u)0xCB);		//A RETF Instruction
		return 0x0c;
	case CB_IPXESR_RET:		// IPX ESR return
		if (use_cb) E_Exit("ipx esr return must not implement a callback handler!");
		phys_writeb(physAddress+0x00,(Bit8u)0xfa);		// cli
		phys_writew(physAddress+0x01,(Bit16u)0x20b0);	// mov al, 0x20
		phys_writew(physAddress+0x03,(Bit16u)0xa0e6);	// out 0xa0, al
		phys_writew(physAddress+0x05,(Bit16u)0x20e6);	// out 0x20, al
		phys_writeb(physAddress+0x07,(Bit8u)0x61);		// popa
		phys_writew(physAddress+0x08,(Bit16u)0xA90F);	// pop gs
		phys_writew(physAddress+0x0a,(Bit16u)0xA10F);	// pop fs
		phys_writeb(physAddress+0x0c,(Bit8u)0x07);		// pop es
		phys_writeb(physAddress+0x0d,(Bit8u)0x1f);		// pop ds
		phys_writeb(physAddress+0x0e,(Bit8u)0xcf);		//An IRET Instruction
		return 0x0f; */
	case CB_INT21:
		phys_writeb(physAddress+0x00,(Bit8u)0xFB);		//STI
		if (use_cb) {
			phys_writeb(physAddress+0x01,(Bit8u)0xFE);	//GRP 4
			phys_writeb(physAddress+0x02,(Bit8u)0x38);	//Extra Callback instruction
			phys_writew(physAddress+0x03,(Bit16u)callback);	//The immediate word
			physAddress+=4;
		}
		phys_writeb(physAddress+0x01,(Bit8u)0xCF);		//An IRET Instruction
		phys_writeb(physAddress+0x02,(Bit8u)0xCB);		//A RETF Instruction
		phys_writeb(physAddress+0x03,(Bit8u)0x51);		// push cx
		phys_writeb(physAddress+0x04,(Bit8u)0xB9);		// mov cx,
		phys_writew(physAddress+0x05,(Bit16u)0x0140);		// 0x140
		phys_writew(physAddress+0x07,(Bit16u)0xFEE2);		// loop $-2
		phys_writeb(physAddress+0x09,(Bit8u)0x59);		// pop cx
		phys_writeb(physAddress+0x0A,(Bit8u)0xCF);		//An IRET Instruction
		return (use_cb?15:11);

	default:
		E_Exit("CALLBACK:Setup:Illegal type %d",type);
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
	for (Bitu i = 0;i < 16;i++) {
		phys_writeb(CALLBACK_PhysPointer(callback)+i ,(Bit8u) 0x00);
	}
}

CALLBACK_HandlerObject::~CALLBACK_HandlerObject(){
	if(!installed) return;
	if(m_type == CALLBACK_HandlerObject::SETUP) {
		if(vectorhandler.installed){
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
}

void CALLBACK_HandlerObject::Install(CallBack_Handler handler,Bitu type,const char* description){
	if(!installed) {
		installed=true;
		m_type=SETUP;
		m_callback=CALLBACK_Allocate();
		CALLBACK_Setup(m_callback,handler,type,description);
	} else E_Exit("Allready installed");
}
void CALLBACK_HandlerObject::Install(CallBack_Handler handler,Bitu type,PhysPt addr,const char* description){
	if(!installed) {
		installed=true;
		m_type=SETUP;
		m_callback=CALLBACK_Allocate();
		CALLBACK_Setup(m_callback,handler,type,addr,description);
	} else E_Exit("Allready installed");
}

void CALLBACK_HandlerObject::Allocate(CallBack_Handler handler,const char* description) {
	if(!installed) {
		installed=true;
		m_type=NONE;
		m_callback=CALLBACK_Allocate();
		CALLBACK_SetDescription(m_callback,description);
		CallBack_Handlers[m_callback]=handler;
	} else E_Exit("Allready installed");
}

void CALLBACK_HandlerObject::Set_RealVec(Bit8u vec){
	if(!vectorhandler.installed) {
		vectorhandler.installed=true;
		vectorhandler.interrupt=vec;
		RealSetVec(vec,Get_RealPointer(),vectorhandler.old_vector);
	} else E_Exit ("double usage of vector handler");
}

void CALLBACK_Init(Section* /*sec*/) {
	Bitu i;
	for (i=0;i<CB_MAX;i++) {
		CallBack_Handlers[i]=&illegal_handler;
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
	call_default2=CALLBACK_Allocate();
	CALLBACK_Setup(call_default2,&default_handler,CB_IRET,"default");
   
	/* Only setup default handler for first part of interrupt table */
	for (Bit16u ct=0;ct<0x60;ct++) {
		real_writed(0,ct*4,CALLBACK_RealPointer(call_default));
	}
	for (Bit16u ct=0x68;ct<0x70;ct++) {
		real_writed(0,ct*4,CALLBACK_RealPointer(call_default));
	}
	/* Setup block of 0xCD 0xxx instructions */
	PhysPt rint_base=CALLBACK_GetBase()+CB_MAX*CB_SIZE;
	for (i=0;i<=0xff;i++) {
		phys_writeb(rint_base,0xCD);
		phys_writeb(rint_base+1,(Bit8u)i);
		phys_writeb(rint_base+2,0xFE);
		phys_writeb(rint_base+3,0x38);
		phys_writew(rint_base+4,(Bit16u)call_stop);
		rint_base+=6;

	}
	// setup a few interrupt handlers that point to bios IRETs by default
	real_writed(0,0x0e*4,CALLBACK_RealPointer(call_default2));	//design your own railroad
	real_writed(0,0x66*4,CALLBACK_RealPointer(call_default));	//war2d
	real_writed(0,0x67*4,CALLBACK_RealPointer(call_default));
	real_writed(0,0x68*4,CALLBACK_RealPointer(call_default));
	real_writed(0,0x5c*4,CALLBACK_RealPointer(call_default));	//Network stuff
	//real_writed(0,0xf*4,0); some games don't like it

	call_priv_io=CALLBACK_Allocate();

	// virtualizable in-out opcodes
	phys_writeb(CALLBACK_PhysPointer(call_priv_io)+0x00,(Bit8u)0xec);	// in al, dx
	phys_writeb(CALLBACK_PhysPointer(call_priv_io)+0x01,(Bit8u)0xcb);	// retf
	phys_writeb(CALLBACK_PhysPointer(call_priv_io)+0x02,(Bit8u)0xed);	// in ax, dx
	phys_writeb(CALLBACK_PhysPointer(call_priv_io)+0x03,(Bit8u)0xcb);	// retf
	phys_writeb(CALLBACK_PhysPointer(call_priv_io)+0x04,(Bit8u)0x66);	// in eax, dx
	phys_writeb(CALLBACK_PhysPointer(call_priv_io)+0x05,(Bit8u)0xed);
	phys_writeb(CALLBACK_PhysPointer(call_priv_io)+0x06,(Bit8u)0xcb);	// retf

	phys_writeb(CALLBACK_PhysPointer(call_priv_io)+0x08,(Bit8u)0xee);	// out dx, al
	phys_writeb(CALLBACK_PhysPointer(call_priv_io)+0x09,(Bit8u)0xcb);	// retf
	phys_writeb(CALLBACK_PhysPointer(call_priv_io)+0x0a,(Bit8u)0xef);	// out dx, ax
	phys_writeb(CALLBACK_PhysPointer(call_priv_io)+0x0b,(Bit8u)0xcb);	// retf
	phys_writeb(CALLBACK_PhysPointer(call_priv_io)+0x0c,(Bit8u)0x66);	// out dx, eax
	phys_writeb(CALLBACK_PhysPointer(call_priv_io)+0x0d,(Bit8u)0xef);
	phys_writeb(CALLBACK_PhysPointer(call_priv_io)+0x0e,(Bit8u)0xcb);	// retf
}
