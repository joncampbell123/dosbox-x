/*
 *  Copyright (C) 2002-2013  The DOSBox Team
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

#ifndef DOSBOX_PIC_H
#define DOSBOX_PIC_H


/* CPU Cycle Timing */
extern Bit32s CPU_Cycles;
extern Bit32s CPU_CycleLeft;
extern Bit32s CPU_CycleMax;

typedef void (PIC_EOIHandler) (void);
typedef void (* PIC_EventHandler)(Bitu val);

enum PIC_irq_hacks {
	PIC_irq_hack_none=0,		// dispatch IRQ normally
	PIC_irq_hack_cs_equ_ds		// do not fire IRQ unless segment registers in the CPU are DS == CS
					//    explanation: a handful of games and demos have Sound Blaster interrupt service
					//    routines that assume DS == CS and they make no attempt to reload DS to refer
					//    to local variables properly. eventually these programs crash or malfunction
					//    because sooner or later, the ISR is called with CS != DS. This hack can be
					//    used to prevent those games/demos from crashing.
};

extern enum PIC_irq_hacks PIC_IRQ_hax[16];

void PIC_Set_IRQ_hack(int IRQ,enum PIC_irq_hacks hack);
enum PIC_irq_hacks PIC_parse_IRQ_hack_string(const char *str);

extern Bitu PIC_IRQCheck;
extern Bitu PIC_Ticks;

static INLINE float PIC_TickIndex(void) {
	return (CPU_CycleMax-CPU_CycleLeft-CPU_Cycles)/(float)CPU_CycleMax;
}

static INLINE Bits PIC_TickIndexND(void) {
	return CPU_CycleMax-CPU_CycleLeft-CPU_Cycles;
}

static INLINE Bits PIC_MakeCycles(double amount) {
	return (Bits)(CPU_CycleMax*amount);
}

static INLINE double PIC_FullIndex(void) {
	return PIC_Ticks+(double)PIC_TickIndex();
}

void PIC_ActivateIRQ(Bitu irq);
void PIC_DeActivateIRQ(Bitu irq);

void PIC_runIRQs(void);
bool PIC_RunQueue(void);

//Delay in milliseconds
void PIC_AddEvent(PIC_EventHandler handler,float delay,Bitu val=0);
void PIC_RemoveEvents(PIC_EventHandler handler);
void PIC_RemoveSpecificEvents(PIC_EventHandler handler, Bitu val);

void PIC_SetIRQMask(Bitu irq, bool masked);
#endif
