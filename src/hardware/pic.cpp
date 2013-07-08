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

/* $Id: pic.cpp,v 1.44 2009-05-27 09:15:41 qbix79 Exp $ */

#include <list>

#include "dosbox.h"
#include "inout.h"
#include "cpu.h"
#include "callback.h"
#include "pic.h"
#include "timer.h"
#include "setup.h"

#define PIC_QUEUESIZE 512

struct IRQ_Block {
	bool masked;
	bool active;
	bool inservice;
	Bitu vector;
};

struct PIC_Controller {
	Bitu icw_words;
	Bitu icw_index;
/*	Bitu masked; */

	bool special;
	bool auto_eoi;
	bool rotate_on_auto_eoi;
	bool single;
	bool request_issr;
	Bit8u vector_base;
};

Bitu PIC_Ticks=0;
Bitu PIC_IRQCheck;
Bitu PIC_IRQOnSecondPicActive;
Bitu PIC_IRQActive;


static IRQ_Block irqs[16];
static PIC_Controller pics[2];
static bool PIC_Special_Mode = false; //Saves one compare in the pic_run_irqloop
struct PICEntry {
	float index;
	Bitu value;
	PIC_EventHandler pic_event;
	PICEntry * next;
};

static struct {
	PICEntry entries[PIC_QUEUESIZE];
	PICEntry * free_entry;
	PICEntry * next_entry;
} pic_queue;

static void write_command(Bitu port,Bitu val,Bitu iolen) {
	PIC_Controller * pic=&pics[port==0x20 ? 0 : 1];
	Bitu irq_base=port==0x20 ? 0 : 8;
	Bitu i;
	static Bit16u IRQ_priority_table[16] = 
		{ 0,1,2,8,9,10,11,12,13,14,15,3,4,5,6,7 };
	if (GCC_UNLIKELY(val&0x10)) {		// ICW1 issued
		if (val&0x04) E_Exit("PIC: 4 byte interval not handled");
		if (val&0x08) E_Exit("PIC: level triggered mode not handled");
		if (val&0xe0) E_Exit("PIC: 8080/8085 mode not handled");
		pic->single=(val&0x02)==0x02;
		pic->icw_index=1;			// next is ICW2
		pic->icw_words=2 + (val&0x01);	// =3 if ICW4 needed
	} else if (GCC_UNLIKELY(val&0x08)) {	// OCW3 issued
		if (val&0x04) E_Exit("PIC: poll command not handled");
		if (val&0x02) {		// function select
			if (val&0x01) pic->request_issr=true;	/* select read interrupt in-service register */
			else pic->request_issr=false;			/* select read interrupt request register */
		}
		if (val&0x40) {		// special mask select
			if (val&0x20) pic->special=true;
			else pic->special=false;
			if(pic[0].special || pics[1].special) 
				PIC_Special_Mode = true; else 
				PIC_Special_Mode = false;
			if (PIC_IRQCheck) { //Recheck irqs
				CPU_CycleLeft += CPU_Cycles;
				CPU_Cycles = 0;
			}
			LOG(LOG_PIC,LOG_NORMAL)("port %X : special mask %s",port,(pic->special)?"ON":"OFF");
		}
	} else {	// OCW2 issued
		if (val&0x20) {		// EOI commands
			if (GCC_UNLIKELY(val&0x80)) E_Exit("rotate mode not supported");
			if (val&0x40) {		// specific EOI
				if (PIC_IRQActive==(irq_base+val-0x60U)) {
					irqs[PIC_IRQActive].inservice=false;
					PIC_IRQActive=PIC_NOIRQ;
					for (i=0; i<=15; i++) {
						if (irqs[IRQ_priority_table[i]].inservice) {
							PIC_IRQActive=IRQ_priority_table[i];
							break;
						}
					}
				}
//				if (val&0x80);	// perform rotation
			} else {		// nonspecific EOI
				if (PIC_IRQActive<(irq_base+8)) {
					irqs[PIC_IRQActive].inservice=false;
					PIC_IRQActive=PIC_NOIRQ;
					for (i=0; i<=15; i++){
						if(GCC_UNLIKELY(irqs[IRQ_priority_table[i]].inservice)) {
							PIC_IRQActive=IRQ_priority_table[i];
							break;
						}
					}
				}
//				if (val&0x80);	// perform rotation
			}
		} else {
			if ((val&0x40)==0) {		// rotate in auto EOI mode
				if (val&0x80) pic->rotate_on_auto_eoi=true;
				else pic->rotate_on_auto_eoi=false;
			} else if (val&0x80) {
				LOG(LOG_PIC,LOG_NORMAL)("set priority command not handled");
			}	// else NOP command
		}
	}	// end OCW2
}

static void write_data(Bitu port,Bitu val,Bitu iolen) {
	PIC_Controller * pic=&pics[port==0x21 ? 0 : 1];
	Bitu irq_base=(port==0x21) ? 0 : 8;
	Bitu i;
	bool old_irq2_mask = irqs[2].masked;
	switch(pic->icw_index) {
	case 0:                        /* mask register */
		LOG(LOG_PIC,LOG_NORMAL)("%d mask %X",port==0x21 ? 0 : 1,val);
		for (i=0;i<=7;i++) {
			irqs[i+irq_base].masked=(val&(1<<i))>0;
			if(port==0x21) {
				if (irqs[i+irq_base].active && !irqs[i+irq_base].masked) PIC_IRQCheck|=(1 << (i+irq_base));
				else PIC_IRQCheck&=~(1 << (i+irq_base));
			} else {
				if (irqs[i+irq_base].active && !irqs[i+irq_base].masked && !irqs[2].masked) PIC_IRQCheck|=(1 << (i+irq_base));
				else PIC_IRQCheck&=~(1 << (i+irq_base));
			}
		}
		if (machine==MCH_PCJR) {
			/* irq6 cannot be disabled as it serves as pseudo-NMI */
			irqs[6].masked=false;
		}
		if(irqs[2].masked != old_irq2_mask) {
		/* Irq 2 mask has changed recheck second pic */
			for(i=8;i<=15;i++) {
				if (irqs[i].active && !irqs[i].masked && !irqs[2].masked) PIC_IRQCheck|=(1 << (i));
				else PIC_IRQCheck&=~(1 << (i));
			}
		}
		if (PIC_IRQCheck) {
			CPU_CycleLeft+=CPU_Cycles;
			CPU_Cycles=0;
		}
		break;
	case 1:                        /* icw2          */
		LOG(LOG_PIC,LOG_NORMAL)("%d:Base vector %X",port==0x21 ? 0 : 1,val);
		for (i=0;i<=7;i++) {
			irqs[i+irq_base].vector=(val&0xf8)+i;
		};
		if(pic->icw_index++ >= pic->icw_words) pic->icw_index=0;
		else if(pic->single) pic->icw_index=3;		/* skip ICW3 in single mode */
		break;
	case 2:							/* icw 3 */
		LOG(LOG_PIC,LOG_NORMAL)("%d:ICW 3 %X",port==0x21 ? 0 : 1,val);
		if(pic->icw_index++ >= pic->icw_words) pic->icw_index=0;
		break;
	case 3:							/* icw 4 */
		/*
			0	    1 8086/8080  0 mcs-8085 mode
			1	    1 Auto EOI   0 Normal EOI
			2-3	   0x Non buffer Mode 
				   10 Buffer Mode Slave 
				   11 Buffer mode Master	
			4		Special/Not Special nested mode 
		*/
		pic->auto_eoi=(val & 0x2)>0;
		
		LOG(LOG_PIC,LOG_NORMAL)("%d:ICW 4 %X",port==0x21 ? 0 : 1,val);

		if ((val&0x01)==0) E_Exit("PIC:ICW4: %x, 8085 mode not handled",val);
		if ((val&0x10)!=0) LOG_MSG("PIC:ICW4: %x, special fully-nested mode not handled",val);

		if(pic->icw_index++ >= pic->icw_words) pic->icw_index=0;
		break;
	default:
		LOG(LOG_PIC,LOG_NORMAL)("ICW HUH? %X",val);
		break;
	}
}


static Bitu read_command(Bitu port,Bitu iolen) {
	PIC_Controller * pic=&pics[port==0x20 ? 0 : 1];
	Bitu irq_base=(port==0x20) ? 0 : 8;
	Bitu i;Bit8u ret=0;Bit8u b=1;
	if (pic->request_issr) {
		for (i=irq_base;i<irq_base+8;i++) {
			if (irqs[i].inservice) ret|=b;
			b <<= 1;
		}
	} else {
		for (i=irq_base;i<irq_base+8;i++) {
			if (irqs[i].active)	ret|=b;
			b <<= 1;
		}
		if (irq_base==0 && (PIC_IRQCheck&0xff00)) ret |=4;
	}
	return ret;
}

static Bitu read_data(Bitu port,Bitu iolen) {
	Bitu irq_base=(port==0x21) ? 0 : 8;
	Bitu i;Bit8u ret=0;Bit8u b=1;
	for (i=irq_base;i<=irq_base+7;i++) {
		if (irqs[i].masked)	ret|=b;
		b <<= 1;
	}

	return ret;
}


void PIC_ActivateIRQ(Bitu irq) {
	if( irq < 8 ) {
		irqs[irq].active = true;
		if (!irqs[irq].masked) {
			PIC_IRQCheck|=(1 << irq);
		}
	} else 	if (irq < 16) {
		irqs[irq].active = true;
		PIC_IRQOnSecondPicActive|=(1 << irq);
		if (!irqs[irq].masked && !irqs[2].masked) {
			PIC_IRQCheck|=(1 << irq);
		}
	}
}

void PIC_DeActivateIRQ(Bitu irq) {
	if (irq<16) {
		irqs[irq].active=false;
		PIC_IRQCheck&=~(1 << irq);
		PIC_IRQOnSecondPicActive&=~(1 << irq);
	}
}
static inline bool PIC_startIRQ(Bitu i) {
	/* irqs on second pic only if irq 2 isn't masked */
	if( i > 7 && irqs[2].masked) return false;
	irqs[i].active = false;
	PIC_IRQCheck&= ~(1 << i);
	PIC_IRQOnSecondPicActive&= ~(1 << i);
	CPU_HW_Interrupt(irqs[i].vector);
	Bitu pic=(i&8)>>3;
	if (!pics[pic].auto_eoi) { //irq 0-7 => pic 0 else pic 1 
		PIC_IRQActive = i;
		irqs[i].inservice = true;
	} else if (GCC_UNLIKELY(pics[pic].rotate_on_auto_eoi)) {
		E_Exit("rotate on auto EOI not handled");
	}
	return true;
}

void PIC_runIRQs(void) {
	if (!GETFLAG(IF)) return;
	if (GCC_UNLIKELY(!PIC_IRQCheck)) return;
	if (GCC_UNLIKELY(cpudecoder==CPU_Core_Normal_Trap_Run)) return;

	static Bitu IRQ_priority_order[16] = 
		{ 0,1,2,8,9,10,11,12,13,14,15,3,4,5,6,7 };
	static Bit16u IRQ_priority_lookup[17] =
		{ 0,1,2,11,12,13,14,15,3,4,5,6,7,8,9,10,16 };
	Bit16u activeIRQ = PIC_IRQActive;
	if (activeIRQ == PIC_NOIRQ) activeIRQ = 16;
	/* Get the priority of the active irq */
	Bit16u Priority_Active_IRQ = IRQ_priority_lookup[activeIRQ];

	Bitu i,j;

	/* j is the priority (walker)
	 * i is the irq at the current priority */

	/* If one of the pics is in special mode use a check that cares for that. */
	if(!PIC_Special_Mode) {
		for (j = 0; j < Priority_Active_IRQ; j++) {
			i = IRQ_priority_order[j];
			if (!irqs[i].masked && irqs[i].active) {
				if(GCC_LIKELY(PIC_startIRQ(i))) return;
			}
		}
	} else {	/* Special mode variant */
		for (j = 0; j<= 15; j++) {
			i = IRQ_priority_order[j];
			if ( (j < Priority_Active_IRQ) || (pics[ ((i&8)>>3) ].special) ) {
				if (!irqs[i].masked && irqs[i].active) {
					/* the irq line is active. it's not masked and
					 * the irq is allowed priority wise. So let's start it */
					/* If started successfully return, else go for the next */
					if(PIC_startIRQ(i)) return;
				}
			}
		}
	}
}

void PIC_SetIRQMask(Bitu irq, bool masked) {
	if(irqs[irq].masked == masked) return;	/* Do nothing if mask doesn't change */
	bool old_irq2_mask = irqs[2].masked;
	irqs[irq].masked=masked;
	if(irq < 8) {
		if (irqs[irq].active && !irqs[irq].masked) { 
			PIC_IRQCheck|=(1 << (irq));
		} else { 
			PIC_IRQCheck&=~(1 << (irq));
		}
	} else {
		if (irqs[irq].active && !irqs[irq].masked && !irqs[2].masked) {
			PIC_IRQCheck|=(1 << (irq));
		} else { 
			PIC_IRQCheck&=~(1 << (irq));
		}
	}
	if(irqs[2].masked != old_irq2_mask) {
	/* Irq 2 mask has changed recheck second pic */
		for(Bitu i=8;i<=15;i++) {
			if (irqs[i].active && !irqs[i].masked && !irqs[2].masked) PIC_IRQCheck|=(1 << (i));
			else PIC_IRQCheck&=~(1 << (i));
		}
	}
	if (PIC_IRQCheck) {
		CPU_CycleLeft+=CPU_Cycles;
		CPU_Cycles=0;
	}
}

static void AddEntry(PICEntry * entry) {
	PICEntry * find_entry=pic_queue.next_entry;
	if (GCC_UNLIKELY(find_entry ==0)) {
		entry->next=0;
		pic_queue.next_entry=entry;
	} else if (find_entry->index>entry->index) {
		pic_queue.next_entry=entry;
		entry->next=find_entry;
	} else while (find_entry) {
		if (find_entry->next) {
			/* See if the next index comes later than this one */
			if (find_entry->next->index > entry->index) {
				entry->next=find_entry->next;
				find_entry->next=entry;
				break;
			} else {
				find_entry=find_entry->next;
			}
		} else {
			entry->next=find_entry->next;
			find_entry->next=entry;
			break;
		}
	}
	Bits cycles=PIC_MakeCycles(pic_queue.next_entry->index-PIC_TickIndex());
	if (cycles<CPU_Cycles) {
		CPU_CycleLeft+=CPU_Cycles;
		CPU_Cycles=0;
	}
}
static bool InEventService = false;
static float srv_lag = 0;

void PIC_AddEvent(PIC_EventHandler handler,float delay,Bitu val) {
	if (GCC_UNLIKELY(!pic_queue.free_entry)) {
		LOG(LOG_PIC,LOG_ERROR)("Event queue full");
		return;
	}
	PICEntry * entry=pic_queue.free_entry;
	if(InEventService) entry->index = delay + srv_lag;
	else entry->index = delay + PIC_TickIndex();

	entry->pic_event=handler;
	entry->value=val;
	pic_queue.free_entry=pic_queue.free_entry->next;
	AddEntry(entry);
}

void PIC_RemoveSpecificEvents(PIC_EventHandler handler, Bitu val) {
	PICEntry * entry=pic_queue.next_entry;
	PICEntry * prev_entry;
	prev_entry = 0;
	while (entry) {
		if (GCC_UNLIKELY((entry->pic_event == handler)) && (entry->value == val)) {
			if (prev_entry) {
				prev_entry->next=entry->next;
				entry->next=pic_queue.free_entry;
				pic_queue.free_entry=entry;
				entry=prev_entry->next;
				continue;
			} else {
				pic_queue.next_entry=entry->next;
				entry->next=pic_queue.free_entry;
				pic_queue.free_entry=entry;
				entry=pic_queue.next_entry;
				continue;
			}
		}
		prev_entry=entry;
		entry=entry->next;
	}	
}

void PIC_RemoveEvents(PIC_EventHandler handler) {
	PICEntry * entry=pic_queue.next_entry;
	PICEntry * prev_entry;
	prev_entry=0;
	while (entry) {
		if (GCC_UNLIKELY(entry->pic_event==handler)) {
			if (prev_entry) {
				prev_entry->next=entry->next;
				entry->next=pic_queue.free_entry;
				pic_queue.free_entry=entry;
				entry=prev_entry->next;
				continue;
			} else {
				pic_queue.next_entry=entry->next;
				entry->next=pic_queue.free_entry;
				pic_queue.free_entry=entry;
				entry=pic_queue.next_entry;
				continue;
			}
		}
		prev_entry=entry;
		entry=entry->next;
	}	
}


bool PIC_RunQueue(void) {
	/* Check to see if a new milisecond needs to be started */
	CPU_CycleLeft+=CPU_Cycles;
	CPU_Cycles=0;
	if (CPU_CycleLeft<=0) {
		return false;
	}
	/* Check the queue for an entry */
	Bits index_nd=PIC_TickIndexND();
	InEventService = true;
	while (pic_queue.next_entry && (pic_queue.next_entry->index*CPU_CycleMax<=index_nd)) {
		PICEntry * entry=pic_queue.next_entry;
		pic_queue.next_entry=entry->next;

		srv_lag = entry->index;
		(entry->pic_event)(entry->value); // call the event handler

		/* Put the entry in the free list */
		entry->next=pic_queue.free_entry;
		pic_queue.free_entry=entry;
	}
	InEventService = false;

	/* Check when to set the new cycle end */
	if (pic_queue.next_entry) {
		Bits cycles=(Bits)(pic_queue.next_entry->index*CPU_CycleMax-index_nd);
		if (GCC_UNLIKELY(!cycles)) cycles=1;
		if (cycles<CPU_CycleLeft) {
			CPU_Cycles=cycles;
		} else {
			CPU_Cycles=CPU_CycleLeft;
		}
	} else CPU_Cycles=CPU_CycleLeft;
	CPU_CycleLeft-=CPU_Cycles;
	if 	(PIC_IRQCheck)	PIC_runIRQs();
	return true;
}

/* The TIMER Part */
struct TickerBlock {
	TIMER_TickHandler handler;
	TickerBlock * next;
};

static TickerBlock * firstticker=0;


void TIMER_DelTickHandler(TIMER_TickHandler handler) {
	TickerBlock * ticker=firstticker;
	TickerBlock * * tick_where=&firstticker;
	while (ticker) {
		if (ticker->handler==handler) {
			*tick_where=ticker->next;
			delete ticker;
			return;
		}
		tick_where=&ticker->next;
		ticker=ticker->next;
	}
}

void TIMER_AddTickHandler(TIMER_TickHandler handler) {
	TickerBlock * newticker=new TickerBlock;
	newticker->next=firstticker;
	newticker->handler=handler;
	firstticker=newticker;
}

void TIMER_AddTick(void) {
	/* Setup new amount of cycles for PIC */
	CPU_CycleLeft=CPU_CycleMax;
	CPU_Cycles=0;
	PIC_Ticks++;
	/* Go through the list of scheduled events and lower their index with 1000 */
	PICEntry * entry=pic_queue.next_entry;
	while (entry) {
		entry->index -= 1.0;
		entry=entry->next;
	}
	/* Call our list of ticker handlers */
	TickerBlock * ticker=firstticker;
	while (ticker) {
		TickerBlock * nextticker=ticker->next;
		ticker->handler();
		ticker=nextticker;
	}
}


class PIC:public Module_base{
private:
	IO_ReadHandleObject ReadHandler[4];
	IO_WriteHandleObject WriteHandler[4];
public:
	PIC(Section* configuration):Module_base(configuration){
		/* Setup pic0 and pic1 with initial values like DOS has normally */
		PIC_IRQCheck=0;
		PIC_IRQActive=PIC_NOIRQ;
		PIC_Ticks=0;
		Bitu i;
		for (i=0;i<2;i++) {
//			pics[i].masked=0xff;
			pics[i].auto_eoi=false;
			pics[i].rotate_on_auto_eoi=false;
			pics[i].request_issr=false;
			pics[i].special=false;
			pics[i].single=false;
			pics[i].icw_index=0;
			pics[i].icw_words=0;
		}
		for (i=0;i<=7;i++) {
			irqs[i].active=false;
			irqs[i].masked=true;
			irqs[i].inservice=false;
			irqs[i+8].active=false;
			irqs[i+8].masked=true;
			irqs[i+8].inservice=false;
			irqs[i].vector=0x8+i;
			irqs[i+8].vector=0x70+i;	
		}
		irqs[0].masked=false;					/* Enable system timer */
		irqs[1].masked=false;					/* Enable Keyboard IRQ */
		irqs[2].masked=false;					/* Enable second pic */
		irqs[8].masked=false;					/* Enable RTC IRQ */
		if (machine==MCH_PCJR) {
			/* Enable IRQ6 (replacement for the NMI for PCJr) */
			irqs[6].masked=false;
		}
		ReadHandler[0].Install(0x20,read_command,IO_MB);
		ReadHandler[1].Install(0x21,read_data,IO_MB);
		WriteHandler[0].Install(0x20,write_command,IO_MB);
		WriteHandler[1].Install(0x21,write_data,IO_MB);
		ReadHandler[2].Install(0xa0,read_command,IO_MB);
		ReadHandler[3].Install(0xa1,read_data,IO_MB);
		WriteHandler[2].Install(0xa0,write_command,IO_MB);
		WriteHandler[3].Install(0xa1,write_data,IO_MB);
		/* Initialize the pic queue */
		for (i=0;i<PIC_QUEUESIZE-1;i++) {
			pic_queue.entries[i].next=&pic_queue.entries[i+1];
		}
		pic_queue.entries[PIC_QUEUESIZE-1].next=0;
		pic_queue.free_entry=&pic_queue.entries[0];
		pic_queue.next_entry=0;
	}
	~PIC(){
	}
};

static PIC* test;

void PIC_Destroy(Section* sec){
	delete test;
}

void PIC_Init(Section* sec) {
	test = new PIC(sec);
	sec->AddDestroyFunction(&PIC_Destroy);
}
