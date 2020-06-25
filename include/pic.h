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

#ifndef DOSBOX_PIC_H
#define DOSBOX_PIC_H

#include "dosbox.h"

/* CPU Cycle Timing */
extern cpu_cycles_count_t CPU_Cycles;
extern cpu_cycles_count_t CPU_CycleLeft;
extern cpu_cycles_count_t CPU_CycleMax;

typedef void (PIC_EOIHandler) (void);
typedef void (* PIC_EventHandler)(Bitu val);

enum PIC_irq_hacks {
	PIC_irq_hack_none=0,		        // dispatch IRQ normally
	PIC_irq_hack_cs_equ_ds=(1u<<0u)		// do not fire IRQ unless segment registers in the CPU are DS == CS
        //    explanation: a handful of games and demos have Sound Blaster interrupt service
        //    routines that assume DS == CS and they make no attempt to reload DS to refer
        //    to local variables properly. eventually these programs crash or malfunction
        //    because sooner or later, the ISR is called with CS != DS. This hack can be
        //    used to prevent those games/demos from crashing.
};

extern unsigned int PIC_IRQ_hax[16];

void PIC_Set_IRQ_hack(int IRQ,unsigned int hack);
unsigned int PIC_parse_IRQ_hack_string(const char *str);

extern Bitu PIC_IRQCheck;
extern Bitu PIC_Ticks;

typedef double pic_tickindex_t;

pic_tickindex_t PIC_GetCurrentEventTime(void);

static INLINE pic_tickindex_t PIC_TickIndex(void) {
	return ((pic_tickindex_t)(CPU_CycleMax-CPU_CycleLeft-CPU_Cycles)) / ((pic_tickindex_t)CPU_CycleMax);
}

static INLINE Bits PIC_TickIndexND(void) {
	return CPU_CycleMax-CPU_CycleLeft-CPU_Cycles;
}

static INLINE Bits PIC_MakeCycles(const pic_tickindex_t amount) {
	return (Bits)((pic_tickindex_t)CPU_CycleMax * amount);
}

static INLINE pic_tickindex_t PIC_FullIndex(void) {
	return (pic_tickindex_t)PIC_Ticks + PIC_TickIndex();
}

void PIC_ActivateIRQ(Bitu irq);
void PIC_DeActivateIRQ(Bitu irq);

void PIC_runIRQs(void);
bool PIC_RunQueue(void);

//Delay in milliseconds
void PIC_AddEvent(PIC_EventHandler handler,pic_tickindex_t delay,Bitu val=0);
void PIC_RemoveEvents(PIC_EventHandler handler);
void PIC_RemoveSpecificEvents(PIC_EventHandler handler, Bitu val);

void PIC_SetIRQMask(Bitu irq, bool masked);
#endif
