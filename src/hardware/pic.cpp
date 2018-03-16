/*
 *  Copyright (C) 2002-2012  The DOSBox Team
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
#include "inout.h"
#include "cpu.h"
#include "callback.h"
#include "pic.h"
#include "timer.h"
#include "setup.h"
#include "control.h"

#define PIC_QUEUESIZE 512

unsigned long PIC_irq_delay_ns = 0;

struct PIC_Controller {
	Bitu icw_words;
	Bitu icw_index;
	bool special;
	bool auto_eoi;
	bool rotate_on_auto_eoi;
	bool single;
	bool request_issr;
	Bit8u vector_base;

	Bit8u irr;        // request register
	Bit8u imr;        // mask register
	Bit8u imrr;       // mask register reversed (makes bit tests simpler)
	Bit8u isr;        // in service register
	Bit8u isrr;       // in service register reversed (makes bit tests simpler)
	Bit8u active_irq; //currently active irq


	void set_imr(Bit8u val);

	void check_after_EOI(){
		//Update the active_irq as an EOI is likely to change that.
		update_active_irq();
		if((irr&imrr)&isrr) check_for_irq();
	}

	void update_active_irq() {
		if(isr == 0) {active_irq = 8; return;}
		for(Bit8u i = 0, s = 1; i < 8;i++, s<<=1){
			if( isr & s){
				active_irq = i;
				return;
			}
		}
	}

	void check_for_irq(){
		const Bit8u possible_irq = (irr&imrr)&isrr;
		if (possible_irq) {
			const Bit8u a_irq = special?8:active_irq;
			for(Bit8u i = 0, s = 1; i < a_irq;i++, s<<=1){
				if ( possible_irq & s ) {
					//There is an irq ready to be served => signal master and/or cpu
					activate();
					return;
				}
			}
		}
		deactivate(); //No irq, remove signal to master and/or cpu
	}

	//Signals master/cpu that there is an irq ready.
	void activate();

	//Removes signal to master/cpu that there is an irq ready.
	void deactivate();

	void raise_irq(Bit8u val){
		Bit8u bit = 1 << (val);
		if((irr & bit)==0) { //value changed (as it is currently not active)
			irr|=bit;
			if((bit&imrr)&isrr) { //not masked and not in service
				if(special || val < active_irq) activate();
			}
		}
	}

	void lower_irq(Bit8u val){
		Bit8u bit = 1 << ( val);
		if(irr & bit) { //value will change (as it is currently active)
			irr&=~bit;
			if((bit&imrr)&isrr) { //not masked and not in service
				//This irq might have toggled PIC_IRQCheck/caused irq 2 on master, when it was raised.
				//If it is active, then recheck it, we can't just deactivate as there might be more IRQS raised.
				if(special || val < active_irq) check_for_irq();
			}
		}
	}

	//handles all bits and logic related to starting this IRQ, it does NOT start the interrupt on the CPU.
	void start_irq(Bit8u val);
};

int master_cascade_irq = -1;
static PIC_Controller pics[2];
static PIC_Controller& master = pics[0];
static PIC_Controller& slave  = pics[1];
Bitu PIC_Ticks = 0;
Bitu PIC_IRQCheck = 0; //Maybe make it a bool and/or ensure 32bit size (x86 dynamic core seems to assume 32 bit variable size)
Bitu PIC_IRQCheckPending = 0; //Maybe make it a bool and/or ensure 32bit size (x86 dynamic core seems to assume 32 bit variable size)
bool enable_slave_pic = true; /* if set, emulate slave with cascade to master. if clear, emulate only master, and no cascade (IRQ 2 is open) */
bool enable_pc_xt_nmi_mask = false;

void PIC_IRQCheckDelayed(Bitu val) {
    PIC_IRQCheck = 1;
    PIC_IRQCheckPending = 0;
}

void PIC_Controller::set_imr(Bit8u val) {
	Bit8u change = (imr) ^ (val); //Bits that have changed become 1.
	imr  =  val;
	imrr = ~val;

	//Test if changed bits are set in irr and are not being served at the moment
	//Those bits have impact on whether the cpu emulation should be paused or not.
	if((irr & change)&isrr) check_for_irq();
}

void PIC_Controller::activate() { 
	//Stops CPU if master, signals master if slave
	if(this == &master) {
		//cycles 0, take care of the port IO stuff added in raise_irq base caller.
        if (!PIC_IRQCheckPending) {
            /* NTS: PIC_AddEvent by design caps CPU_Cycles to make the event happen on time */
            PIC_AddEvent(PIC_IRQCheckDelayed,(double)PIC_irq_delay_ns / 1000000,0);
            PIC_IRQCheckPending = 1;
        }
	} else {
		master.raise_irq(master_cascade_irq);
	}
}

void PIC_Controller::deactivate() { 
	//removes irq check value if master, signals master if slave
	if(this == &master) {
		/* NTS: DOSBox code used to set PIC_IRQCheck = 0 here.
		 *
		 *      That's actually not the way to handle it. Here's why:
		 *
		 *      What would happen if one device raised an IRQ (setting PIC_IRQCheck=1)
		 *      then before dispatching IRQs, another device lowered an IRQ (setting PIC_IRQCheck=0).
		 *
		 *      Lowering the IRQ would reset the flag, and PIC_runIRQs() would never process
		 *      any pending IRQs even though some are waiting.
		 *
		 *      It's better if we set the flag when raising an IRQ, and leave the flag set until
		 *      PIC_runIRQs() determines there are no more IRQs to dispatch. Then and only then
		 *      will PIC_runIRQs() clear the flag. */
	} else {
		/* just because ONE IRQ on the slave finished doesn't mean there aren't any others needing service! */
		if ((irr&imrr) == 0)
			master.lower_irq(master_cascade_irq);
		else
			LOG_MSG("Slave PIC: still to handle irr=%02x imrr=%02x isrr=%02x",irr,imrr,isrr);
	}
}

void PIC_Controller::start_irq(Bit8u val){
	irr&=~(1<<(val));
	if (!auto_eoi) {
		active_irq = val;
		isr |= 1<<(val);
		isrr = ~isr;
	} else if (GCC_UNLIKELY(rotate_on_auto_eoi)) {
		LOG_MSG("rotate on auto EOI not handled");
	}
}

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
	PIC_Controller * pic=&pics[(port==0x20/*IBM*/ || port==0x00/*PC-98*/) ? 0 : 1];

	if (GCC_UNLIKELY(val&0x10)) {		// ICW1 issued
		if (val&0x04) LOG_MSG("PIC: 4 byte interval not handled");
		if (val&0x08) LOG_MSG("PIC: level triggered mode not handled");
		if (val&0xe0) LOG_MSG("PIC: 8080/8085 mode not handled");
		pic->single=(val&0x02)==0x02;
		pic->icw_index=1;			// next is ICW2
		pic->icw_words=2 + (val&0x01);	// =3 if ICW4 needed
	} else if (GCC_UNLIKELY(val&0x08)) {	// OCW3 issued
		if (val&0x04) LOG_MSG("PIC: poll command not handled");
		if (val&0x02) {		// function select
			if (val&0x01) pic->request_issr=true;	/* select read interrupt in-service register */
			else pic->request_issr=false;			/* select read interrupt request register */
		}
		if (val&0x40) {		// special mask select
			if (val&0x20) pic->special = true;
			else pic->special = false;
			//Check if there are irqs ready to run, as the priority system has possibly been changed.
			pic->check_for_irq();
			LOG(LOG_PIC,LOG_NORMAL)("port %X : special mask %s",(int)port,(pic->special)?"ON":"OFF");
		}
	} else {	// OCW2 issued
		if (val&0x20) {		// EOI commands
			if (GCC_UNLIKELY(val&0x80)) LOG_MSG("rotate mode not supported");
			if (val&0x40) {		// specific EOI
				pic->isr &= ~(1<< ((val-0x60)));
				pic->isrr = ~pic->isr;
				pic->check_after_EOI();
//				if (val&0x80);	// perform rotation
			} else {		// nonspecific EOI
				if (pic->active_irq != 8) { 
					//If there is no irq in service, ignore the call, some games send an eoi to both pics when a sound irq happens (regardless of the irq).
					pic->isr &= ~(1 << (pic->active_irq));
					pic->isrr = ~pic->isr;
					pic->check_after_EOI();
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
	PIC_Controller * pic=&pics[(port==0x21/*IBM*/ || port==0x02/*PC-98*/) ? 0 : 1];

	switch(pic->icw_index) {
	case 0:                        /* mask register */
		pic->set_imr(val);
		break;
	case 1:                        /* icw2          */
		LOG(LOG_PIC,LOG_NORMAL)("%d:Base vector %X",port==0x21 ? 0 : 1,(int)val);
		pic->vector_base = val&0xf8;
		if(pic->icw_index++ >= pic->icw_words) pic->icw_index=0;
		else if(pic->single) pic->icw_index=3;		/* skip ICW3 in single mode */
		break;
	case 2:							/* icw 3 */
		LOG(LOG_PIC,LOG_NORMAL)("%d:ICW 3 %X",port==0x21 ? 0 : 1,(int)val);
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
		
		LOG(LOG_PIC,LOG_NORMAL)("%d:ICW 4 %X",port==0x21 ? 0 : 1,(int)val);

		if ((val&0x01)==0) LOG_MSG("PIC:ICW4: %x, 8085 mode not handled",(int)val);
		if ((val&0x10)!=0) LOG_MSG("PIC:ICW4: %x, special fully-nested mode not handled",(int)val);

		if(pic->icw_index++ >= pic->icw_words) pic->icw_index=0;
		break;
	default:
		LOG(LOG_PIC,LOG_NORMAL)("ICW HUH? %X",(int)val);
		break;
	}
}


static Bitu read_command(Bitu port,Bitu iolen) {
	PIC_Controller * pic=&pics[(port==0x20/*IBM*/ || port==0x00/*PC-98*/) ? 0 : 1];
	if (pic->request_issr){
		return pic->isr;
	} else { 
		return pic->irr;
	}
}


static Bitu read_data(Bitu port,Bitu iolen) {
	PIC_Controller * pic=&pics[(port==0x21/*IBM*/ || port==0x02/*PC-98*/) ? 0 : 1];
	return pic->imr;
}

/* PC/XT NMI mask register 0xA0. Documentation on the other bits
 * is sparse and spread across the internet, but many seem to
 * agree that bit 7 is used to enable/disable the NMI (1=enable,
 * 0=disable) */
static void pc_xt_nmi_write(Bitu port,Bitu val,Bitu iolen) {
	CPU_NMI_gate = (val & 0x80) ? true : false;
}

/* FIXME: This should be called something else that's true to the ISA bus, like PIC_PulseIRQ, not Activate IRQ.
 *        ISA interrupts are edge triggered, not level triggered. */
void PIC_ActivateIRQ(Bitu irq) {
	/* Remember what was once IRQ 2 on PC/XT is IRQ 9 on PC/AT */
    if (IS_PC98_ARCH) {
        if (irq == 7) {
            LOG(LOG_PIC,LOG_ERROR)("Attempted to raise IRQ %u, which is cascade IRQ",(int)irq);
            return; /* don't raise cascade IRQ */
        }
    }
    else if (enable_slave_pic) { /* PC/AT emulation with slave PIC cascade to master */
		if (irq == 2) irq = 9;
	}
	else { /* PC/XT emulation with only master PIC */
		if (irq == 9) irq = 2;
		if (irq >= 8) {
			LOG(LOG_PIC,LOG_ERROR)("Attempted to raise IRQ %u when slave PIC does not exist",(int)irq);
			return;
		}
	}

	Bitu t = irq>7 ? (irq - 8): irq;
	PIC_Controller * pic=&pics[irq>7 ? 1 : 0];

	pic->raise_irq(t);
}

void PIC_DeActivateIRQ(Bitu irq) {
	/* Remember what was once IRQ 2 on PC/XT is IRQ 9 on PC/AT */
    if (IS_PC98_ARCH) {
        if (irq == 7) return;
    }
    else if (enable_slave_pic) { /* PC/AT emulation with slave PIC cascade to master */
		if (irq == 2) irq = 9;
	}
	else { /* PC/XT emulation with only master PIC */
		if (irq == 9) irq = 2;
		if (irq >= 8) {
			LOG(LOG_PIC,LOG_ERROR)("Attempted to lower IRQ %u when slave PIC does not exist",(int)irq);
			return;
		}
	}

	Bitu t = irq>7 ? (irq - 8): irq;
	PIC_Controller * pic=&pics[irq>7 ? 1 : 0];
	pic->lower_irq(t);
}

enum PIC_irq_hacks PIC_IRQ_hax[16] = { PIC_irq_hack_none };

void PIC_Set_IRQ_hack(int IRQ,enum PIC_irq_hacks hack) {
	if (IRQ < 0 || IRQ >= 16) return;
	PIC_IRQ_hax[IRQ] = hack;
}

enum PIC_irq_hacks PIC_parse_IRQ_hack_string(const char *str) {
	if (!strcmp(str,"none"))
		return PIC_irq_hack_none;
	else if (!strcmp(str,"cs_equ_ds"))
		return PIC_irq_hack_cs_equ_ds;

	return PIC_irq_hack_none;
}

static bool IRQ_hack_check_cs_equ_ds(const int IRQ) {
	uint16_t s_cs = SegValue(cs);
	uint16_t s_ds = SegValue(ds);

	if (s_cs >= 0xA000)
		return true; // don't complain about the BIOS ISR

	if (s_cs != s_ds) {
		LOG(LOG_PIC,LOG_DEBUG)("Not dispatching IRQ %d according to IRQ hack. CS != DS",IRQ);
		return false;
	}

	return true;
}

static void slave_startIRQ(){
	Bit8u pic1_irq = 8;
	const Bit8u p = (slave.irr & slave.imrr)&slave.isrr;
	const Bit8u max = slave.special?8:slave.active_irq;
	for(Bit8u i = 0,s = 1;i < max;i++, s<<=1) {
		if (p&s) {
			if (PIC_IRQ_hax[i+8] == PIC_irq_hack_cs_equ_ds)
				if (!IRQ_hack_check_cs_equ_ds(i+8))
					continue; // skip IRQ

			pic1_irq = i;
			break;
		}
	}

	if (GCC_UNLIKELY(pic1_irq == 8)) {
		/* we have an IRQ routing problem. this code is supposed to emulate the fact that
		 * what was once IRQ 2 on PC/XT is routed to IRQ 9 on AT systems, because IRQ 8-15
		 * cascade to IRQ 2 on such systems. but it's nothing to E_Exit() over. */
		LOG(LOG_PIC,LOG_ERROR)("ISA PIC problem: IRQ %d (cascade) is active on master PIC without active IRQ 8-15 on slave PIC.",master_cascade_irq);
		slave.lower_irq(master_cascade_irq); /* clear it */
		return;
	}

	slave.start_irq(pic1_irq);
	master.start_irq(master_cascade_irq);
	CPU_HW_Interrupt(slave.vector_base + pic1_irq);
}

static void inline master_startIRQ(Bitu i){
	master.start_irq(i);
	CPU_HW_Interrupt(master.vector_base + i);
}

void PIC_runIRQs(void) {
	if (!GETFLAG(IF)) return;
	if (GCC_UNLIKELY(!PIC_IRQCheck)) return;
	if (GCC_UNLIKELY(cpudecoder==CPU_Core_Normal_Trap_Run)) return; // FIXME: Why?
	if (GCC_UNLIKELY(CPU_NMI_active) || GCC_UNLIKELY(CPU_NMI_pending)) return; /* NMI has higher priority than PIC */

	const Bit8u p = (master.irr & master.imrr)&master.isrr;
	const Bit8u max = master.special?8:master.active_irq;
	Bit8u i,s;

	for (i = 0,s = 1;i < max;i++, s<<=1){
		if (p&s) {
			if (PIC_IRQ_hax[i] == PIC_irq_hack_cs_equ_ds)
				if (!IRQ_hack_check_cs_equ_ds(i))
					continue; // skip IRQ

			if ((int)i == master_cascade_irq) { //second pic, or will not match if master_cascade_irq == -1
				slave_startIRQ();
			} else {
				master_startIRQ(i);
			}
			break;
		}
	}

	/* if we cleared all IRQs, then stop checking.
	 * otherwise, keep the flag set for the next IRQ to process. */
	if (i == max && (master.irr&master.imrr) == 0) {
        PIC_IRQCheckPending = 0;
        PIC_IRQCheck = 0;
    }
    else if (PIC_IRQCheck) {
        PIC_AddEvent(PIC_IRQCheckDelayed,(double)PIC_irq_delay_ns / 1000000,0);
        PIC_IRQCheckPending = 1;
        PIC_IRQCheck = 0;
    }
}

void PIC_SetIRQMask(Bitu irq, bool masked) {
	Bitu t = irq>7 ? (irq - 8): irq;
	PIC_Controller * pic=&pics[irq>7 ? 1 : 0];
	//clear bit
	Bit8u bit = 1 <<(t);
	Bit8u newmask = pic->imr;
	newmask &= ~bit;
	if (masked) newmask |= bit;
	pic->set_imr(newmask);
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

extern ClockDomain clockdom_DOSBox_cycles;

bool PIC_RunQueue(void) {
	/* Check to see if a new millisecond needs to be started */
	CPU_CycleLeft += CPU_Cycles;
	CPU_Cycles = 0;

	if (CPU_CycleLeft > 0) {
		if (PIC_IRQCheck)
			PIC_runIRQs();

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

        if (PIC_IRQCheck)
            PIC_runIRQs();
    }

	/* if we're out of cycles, then return false. don't execute any more instructions */
	if ((CPU_CycleLeft+CPU_Cycles) <= 0)
		return false;

	return true;
}

/* The TIMER Part */
struct TickerBlock {
	/* TODO: carry const char * field for name! */
	TIMER_TickHandler handler;
	TickerBlock * next;
};

static TickerBlock * firstticker=0;

void TIMER_ShutdownTickHandlers() {
	unsigned int leftovers = 0;

	/* pull in the singly linked list from the front, hand over hand */
	while (firstticker != NULL) {
		TickerBlock *n = firstticker->next;
		delete firstticker;
		firstticker = n;
		leftovers++;
	}

	if (leftovers != 0)
		LOG(LOG_MISC,LOG_DEBUG)("TIMER: %u leftover handlers (clean up!).",leftovers);
}

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

extern Bitu time_limit_ms;

static unsigned long PIC_benchstart = 0;
static unsigned long PIC_tickstart = 0;

extern void GFX_SetTitle(Bit32s cycles, Bits frameskip, Bits timing, bool paused);
void TIMER_AddTick(void) {
	/* Setup new amount of cycles for PIC */
	PIC_Ticks++;
	if ((PIC_Ticks&0x3fff) == 0) {
		unsigned long ticks = GetTicks();
		int delta = (PIC_Ticks-PIC_tickstart)*10000/(ticks-PIC_benchstart)+5;
		GFX_SetTitle(-1,-1,delta,false);
		PIC_benchstart = ticks;
		PIC_tickstart = PIC_Ticks;
	}
	CPU_CycleLeft += CPU_CycleMax + CPU_Cycles;
	CPU_Cycles = 0;

    /* timeout */
    if (time_limit_ms != 0 && PIC_Ticks >= time_limit_ms)
        throw int(1);

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

static IO_WriteHandleObject PCXT_NMI_WriteHandler;

static IO_ReadHandleObject ReadHandler[4];
static IO_WriteHandleObject WriteHandler[4];

void PIC_Reset(Section *sec) {
	Bitu i;

	ReadHandler[0].Uninstall();
	ReadHandler[1].Uninstall();
	WriteHandler[0].Uninstall();
	WriteHandler[1].Uninstall();
	ReadHandler[2].Uninstall();
	ReadHandler[3].Uninstall();
	WriteHandler[2].Uninstall();
	WriteHandler[3].Uninstall();
	PCXT_NMI_WriteHandler.Uninstall();

	/* NTS: Parsing this on reset allows PIC configuration changes on reboot instead of restarting the entire emulator */
	Section_prop * section=static_cast<Section_prop *>(control->GetSection("dosbox"));
	assert(section != NULL);

	enable_slave_pic = section->Get_bool("enable slave pic");
	enable_pc_xt_nmi_mask = section->Get_bool("enable pc nmi mask");

    /* NTS: This is a good guess. But the 8259 is static circuitry and not driven by a clock.
     *      But the ability to respond to interrupts is limited by the CPU, too. */
    PIC_irq_delay_ns = 1000000000UL / (unsigned long)PIT_TICK_RATE;
    {
        int x = section->Get_int("irq delay ns");
        if (x >= 0) PIC_irq_delay_ns = x;
    }

    if (enable_slave_pic)
        master_cascade_irq = IS_PC98_ARCH ? 7 : 2;
    else
        master_cascade_irq = -1;

	// LOG
	LOG(LOG_MISC,LOG_DEBUG)("PIC_Reset(): reinitializing PIC controller (cascade=%d)",master_cascade_irq);

	/* Setup pic0 and pic1 with initial values like DOS has normally */
	PIC_Ticks=0;
	PIC_IRQCheck=0;
	for (i=0;i<2;i++) {
		pics[i].auto_eoi=false;
		pics[i].rotate_on_auto_eoi=false;
		pics[i].request_issr=false;
		pics[i].special=false;
		pics[i].single=false;
		pics[i].icw_index=0;
		pics[i].icw_words=0;
		pics[i].irr = pics[i].isr = pics[i].imrr = 0;
		pics[i].isrr = pics[i].imr = 0xff;
		pics[i].active_irq = 8;
	}

    /* IBM: IRQ 0-15 is INT 0x08-0x0F, 0x70-0x7F
     * PC-98: IRQ 0-15 is INT 0x08-0x17 */
	master.vector_base = 0x08;
	slave.vector_base = IS_PC98_ARCH ? 0x10 : 0x70;

    for (Bitu i=0;i < 16;i++)
        PIC_SetIRQMask(i,true);

	PIC_SetIRQMask(0,false);					/* Enable system timer */
	PIC_SetIRQMask(1,false);					/* Enable system timer */
	PIC_SetIRQMask(2,false);					/* Enable second pic */
	PIC_SetIRQMask(8,false);					/* Enable RTC IRQ */

    /* I/O port map
     *
     * IBM PC/XT/AT     NEC PC-98        A0
     * ---------------------------------------
     * 0x20             0x00             0
     * 0x21             0x02             1
     * 0xA0             0x08             0
     * 0xA1             0x0A             1
     */

	ReadHandler[0].Install(IS_PC98_ARCH ? 0x00 : 0x20,read_command,IO_MB);
	ReadHandler[1].Install(IS_PC98_ARCH ? 0x02 : 0x21,read_data,IO_MB);
	WriteHandler[0].Install(IS_PC98_ARCH ? 0x00 : 0x20,write_command,IO_MB);
	WriteHandler[1].Install(IS_PC98_ARCH ? 0x02 : 0x21,write_data,IO_MB);

	/* the secondary slave PIC takes priority over PC/XT NMI mask emulation */
	if (enable_slave_pic) {
		ReadHandler[2].Install(IS_PC98_ARCH ? 0x08 : 0xa0,read_command,IO_MB);
		ReadHandler[3].Install(IS_PC98_ARCH ? 0x0A : 0xa1,read_data,IO_MB);
		WriteHandler[2].Install(IS_PC98_ARCH ? 0x08 : 0xa0,write_command,IO_MB);
		WriteHandler[3].Install(IS_PC98_ARCH ? 0x0A : 0xa1,write_data,IO_MB);
	}
	else if (!IS_PC98_ARCH && enable_pc_xt_nmi_mask) {
		PCXT_NMI_WriteHandler.Install(0xa0,pc_xt_nmi_write,IO_MB);
	}
}

void PIC_Destroy(Section* sec) {
}

void Init_PIC() {
	Bitu i;

	LOG(LOG_MISC,LOG_DEBUG)("Init_PIC()");

	/* Initialize the pic queue */
	for (i=0;i<PIC_QUEUESIZE-1;i++) {
		pic_queue.entries[i].next=&pic_queue.entries[i+1];

		// savestate compatibility
		pic_queue.entries[i].pic_event = 0;
	}
	pic_queue.entries[PIC_QUEUESIZE-1].next=0;
	pic_queue.free_entry=&pic_queue.entries[0];
	pic_queue.next_entry=0;

	AddExitFunction(AddExitFunctionFuncPair(PIC_Destroy));
	AddVMEventFunction(VM_EVENT_RESET,AddVMEventFunctionFuncPair(PIC_Reset));
}

#if C_DEBUG
void DEBUG_LogPIC_C(PIC_Controller &pic) {
    LOG_MSG("%s interrupt controller state",&pic == &master ? "Master" : "Slave");
    LOG_MSG("ICW %u/%u special=%u auto-eoi=%u rotate-eoi=%u single=%u request_issr=%u vectorbase=0x%02x active_irq=%u",
        pic.icw_index,
        pic.icw_words,
        pic.special?1:0,
        pic.auto_eoi?1:0,
        pic.rotate_on_auto_eoi?1:0,
        pic.single?1:0,
        pic.request_issr?1:0,
        pic.vector_base,
        pic.active_irq);

    LOG_MSG("IRQ INT#  Req /Mask/Serv");
    for (unsigned int si=0;si < 8;si++) {
        unsigned int IRQ = si + (&pic == &slave ? 8 : 0);
        unsigned int CPUINT = pic.vector_base + si;

        LOG_MSG("%3u 0x%02X   %c    %c    %c   %s",
            IRQ,
            CPUINT,
            (pic.irr & (1U << si))?'R':' ',
            (pic.imr & (1U << si))?'M':' ',
            (pic.isr & (1U << si))?'S':' ',
            (IRQ == master_cascade_irq) ? "CASCADE" : "");
    }
}

void DEBUG_LogPIC(void) {
    DEBUG_LogPIC_C(master);
    if (enable_slave_pic) DEBUG_LogPIC_C(slave);
}
#endif


