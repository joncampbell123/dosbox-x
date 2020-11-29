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


#ifndef DOSBOX_CALLBACK_H
#define DOSBOX_CALLBACK_H

#ifndef DOSBOX_MEM_H
#include "mem.h"
#endif 

typedef Bitu (*CallBack_Handler)(void);
extern CallBack_Handler CallBack_Handlers[];

enum { CB_RETN,CB_RETF,CB_RETF8,CB_IRET,CB_IRETD,CB_IRET_STI,CB_IRET_EOI_PIC1,
		CB_IRQ0,CB_IRQ1,CB_IRQ1_BREAK,CB_IRQ9,CB_IRQ12,CB_IRQ12_RET,CB_IRQ6_PCJR,CB_MOUSE,
		/*CB_INT28,*/CB_INT29,CB_INT16,CB_HOOKABLE,CB_TDE_IRET,CB_IPXESR,CB_IPXESR_RET,
		CB_INT21,CB_INT13,CB_VESA_WAIT,CB_VESA_PM,CB_IRET_EOI_PIC2,CB_CPM,
        CB_RETF_STI,CB_RETF_CLI};

/* NTS: Cannot make runtime configurable, because CB_MAX is used to define an array */
#define CB_MAX		128U
#define CB_SIZE		32U

/* we can make THESE configurable though! */
//#define CB_SEG	0xF000
//#define CB_SOFFSET	0x1000
extern uint16_t CB_SEG,CB_SOFFSET;

enum {	
	CBRET_NONE=0,CBRET_STOP=1
};

extern uint8_t lastint;

static INLINE RealPt CALLBACK_RealPointer(Bitu callback) {
	return RealMake(CB_SEG,(uint16_t)(CB_SOFFSET+callback*CB_SIZE));
}
static INLINE PhysPt CALLBACK_PhysPointer(Bitu callback) {
	return PhysMake(CB_SEG,(uint16_t)(CB_SOFFSET+callback*CB_SIZE));
}

static inline PhysPt CALLBACK_GetBase(void) {
	return (PhysPt)(((PhysPt)CB_SEG << (PhysPt)4U) + (PhysPt)CB_SOFFSET);
}

uint8_t CALLBACK_Allocate();

void CALLBACK_Idle(void);


void CALLBACK_RunRealInt(uint8_t intnum);
void CALLBACK_RunRealFar(uint16_t seg,uint16_t off);
void CALLBACK_RunRealFarInt(uint16_t seg,uint16_t off);

bool CALLBACK_Setup(Bitu callback,CallBack_Handler handler,Bitu type,const char* descr);
Bitu CALLBACK_Setup(Bitu callback,CallBack_Handler handler,Bitu type,PhysPt addr,const char* descr);

void CALLBACK_SetDescription(Bitu nr, const char* descr);
const char* CALLBACK_GetDescription(Bitu nr);

void CALLBACK_SCF(bool val);
void CALLBACK_SZF(bool val);
void CALLBACK_SIF(bool val);

extern Bitu call_priv_io;


class CALLBACK_HandlerObject{
private:
	bool installed;
	Bitu m_callback;
	enum {NONE,SETUP,SETUPAT} m_type;
    struct {	
		RealPt old_vector;
		uint8_t interrupt;
		bool installed;
	} vectorhandler;
public:
	CALLBACK_HandlerObject():installed(false),m_callback(0/*NULL*/),m_type(NONE) {
		vectorhandler.installed=false;
	}
	~CALLBACK_HandlerObject();

	//Install and allocate a callback.
	void Install(CallBack_Handler handler,Bitu type,const char* description);
	void Install(CallBack_Handler handler,Bitu type,PhysPt addr,const char* description);

	void Uninstall();

	//Only allocate a callback number
	void Allocate(CallBack_Handler handler,const char* description=0);
	uint16_t Get_callback() {
		return (uint16_t)m_callback;
	}
	RealPt Get_RealPointer() {
		return CALLBACK_RealPointer(m_callback);
	}
	void Set_RealVec(uint8_t vec,bool reinstall=false);
};
#endif
