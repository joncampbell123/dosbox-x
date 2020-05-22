/*
 *  Copyright (C) 2002-2019  The DOSBox Team
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335, USA.
 */

#include "dosbox.h"
#include "inout.h"
#include "cpu.h"
#include "callback.h"
#include "pic.h"
#include "timer.h"
#include "setup.h"
#include "control.h"

#if defined(_MSC_VER)
# pragma warning(disable:4244) /* const fmath::local::uint64_t to double possible loss of data */
#endif

#define PIC_QUEUESIZE 512

unsigned long PIC_irq_delay_ns = 0;

bool never_mark_cascade_in_service = false;
bool ignore_cascade_in_service = false;

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
    Bit8u isr_ignore; // in service bits to ignore
    Bit8u active_irq; //currently active irq


    void set_imr(Bit8u val);

    void check_after_EOI(){
        //Update the active_irq as an EOI is likely to change that.
        update_active_irq();
        if((irr&imrr)&isrr) check_for_irq();
    }

    void update_active_irq() {
        if (auto_eoi) {
            assert(isr == 0);
        }

        if(isr == 0) {active_irq = 8; return;}
        for(Bit8u i = 0, s = 1; i < 8;i++, s<<=1){
            if (isr & s) {
                active_irq = i;
                return;
            }
        }
    }

    void check_for_irq();

    //Signals master/cpu that there is an irq ready.
    void activate();

    //Removes signal to master/cpu that there is an irq ready.
    void deactivate();

    void raise_irq(Bit8u val);

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

void PIC_Controller::check_for_irq(){
    const Bit8u possible_irq = (irr&imrr)&isrr;
    if (possible_irq) {
        Bit8u a_irq = special?8:active_irq;

        if (ignore_cascade_in_service && this == &master && a_irq == (unsigned char)master_cascade_irq)
            a_irq++;

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

void PIC_Controller::raise_irq(Bit8u val){
    Bit8u bit = 1 << (val);
    if((irr & bit)==0) { //value changed (as it is currently not active)
        irr|=bit;
        if((bit&imrr)&isrr) { //not masked and not in service
            if(special || val < active_irq) activate();
            else if (ignore_cascade_in_service && this == &master && val == (unsigned char)master_cascade_irq) activate();
        }
    }
}

void PIC_IRQCheckDelayed(Bitu val) {
    (void)val;//UNUSED
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
            PIC_IRQCheckPending = 1;
            PIC_AddEvent(PIC_IRQCheckDelayed,(double)PIC_irq_delay_ns / 1000000,0);
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
        if (never_mark_cascade_in_service && this == &master && val == master_cascade_irq) {
            /* do nothing */
        }
        else {
            active_irq = val;
            isr |= 1<<(val);
            isrr = (~isr) | isr_ignore;
        }
    } else if (GCC_UNLIKELY(rotate_on_auto_eoi)) {
        LOG_MSG("rotate on auto EOI not handled");
    }
}

struct PICEntry {
    pic_tickindex_t index;
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
    (void)iolen;//UNUSED
    PIC_Controller * pic=&pics[(port==0x20/*IBM*/ || port==0x00/*PC-98*/) ? 0 : 1];

    if (GCC_UNLIKELY(val&0x10)) {       // ICW1 issued
        if (val&0x04) LOG_MSG("PIC: 4 byte interval not handled");
        if (val&0x08) LOG_MSG("PIC: level triggered mode not handled");
        if (val&0xe0) LOG_MSG("PIC: 8080/8085 mode not handled");
        pic->single=(val&0x02)==0x02;
        pic->icw_index=1;           // next is ICW2
        pic->icw_words=2 + (val&0x01);  // =3 if ICW4 needed
    } else if (GCC_UNLIKELY(val&0x08)) {    // OCW3 issued
        if (val&0x04) LOG_MSG("PIC: poll command not handled");
        if (val&0x02) {     // function select
            if (val&0x01) pic->request_issr=true;   /* select read interrupt in-service register */
            else pic->request_issr=false;           /* select read interrupt request register */
        }
        if (val&0x40) {     // special mask select
            if (val&0x20) pic->special = true;
            else pic->special = false;
            //Check if there are irqs ready to run, as the priority system has possibly been changed.
            pic->check_for_irq();
            LOG(LOG_PIC,LOG_NORMAL)("port %X : special mask %s",(int)port,(pic->special)?"ON":"OFF");
        }
    } else {    // OCW2 issued
        if (val&0x20) {     // EOI commands
            if (GCC_UNLIKELY(val&0x80)) LOG_MSG("rotate mode not supported");
            if (val&0x40) {     // specific EOI
                pic->isr &= ~(1<< (val-0x60));
                pic->isrr = (~pic->isr) | pic->isr_ignore;
                pic->check_after_EOI();
//              if (val&0x80);  // perform rotation
            } else {        // nonspecific EOI
                if (pic->active_irq != 8) { 
                    //If there is no irq in service, ignore the call, some games send an eoi to both pics when a sound irq happens (regardless of the irq).
                    pic->isr &= ~(1 << (pic->active_irq));
                    pic->isrr = (~pic->isr) | pic->isr_ignore;
                    pic->check_after_EOI();
                }
//              if (val&0x80);  // perform rotation
            }
        } else {
            if ((val&0x40)==0) {        // rotate in auto EOI mode
                if (val&0x80) pic->rotate_on_auto_eoi=true;
                else pic->rotate_on_auto_eoi=false;
            } else if (val&0x80) {
                LOG(LOG_PIC,LOG_NORMAL)("set priority command not handled");
            }   // else NOP command
        }
    }   // end OCW2
}

static void write_data(Bitu port,Bitu val,Bitu iolen) {
    (void)iolen;//UNUSED
    PIC_Controller * pic=&pics[(port==0x21/*IBM*/ || port==0x02/*PC-98*/) ? 0 : 1];

    switch(pic->icw_index) {
    case 0:                        /* mask register */
        pic->set_imr(val);
        break;
    case 1:                        /* icw2          */
        LOG(LOG_PIC,LOG_NORMAL)("%d:Base vector %X",port==0x21 ? 0 : 1,(int)val);
        pic->vector_base = val&0xf8;
        if(pic->icw_index++ >= pic->icw_words) pic->icw_index=0;
        else if(pic->single) pic->icw_index=3;      /* skip ICW3 in single mode */
        break;
    case 2:                         /* icw 3 */
        LOG(LOG_PIC,LOG_NORMAL)("%d:ICW 3 %X",port==0x21 ? 0 : 1,(int)val);
        if(pic->icw_index++ >= pic->icw_words) pic->icw_index=0;
        break;
    case 3:                         /* icw 4 */
        /*
            0       1 8086/8080  0 mcs-8085 mode
            1       1 Auto EOI   0 Normal EOI
            2-3    0x Non buffer Mode 
                   10 Buffer Mode Slave 
                   11 Buffer mode Master    
            4       Special/Not Special nested mode 
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
    (void)iolen;//UNUSED
    PIC_Controller * pic=&pics[(port==0x20/*IBM*/ || port==0x00/*PC-98*/) ? 0 : 1];
    if (pic->request_issr){
        return pic->isr;
    } else {
        /* HACK: I found a PC-98 game "Steel Gun Nyan" that relies on setting the timer to Mode 3 (Square Wave)
         *       then polling the output through the master PIC's IRR to do delays. */
        if (pic == &master) {
            void TIMER_IRQ0Poll(void);
            TIMER_IRQ0Poll();
        }

        return pic->irr;
    }
}


static Bitu read_data(Bitu port,Bitu iolen) {
    (void)iolen;//UNUSED
    PIC_Controller * pic=&pics[(port==0x21/*IBM*/ || port==0x02/*PC-98*/) ? 0 : 1];
    return pic->imr;
}

/* PC/XT NMI mask register 0xA0. Documentation on the other bits
 * is sparse and spread across the internet, but many seem to
 * agree that bit 7 is used to enable/disable the NMI (1=enable,
 * 0=disable)
 *
 * Confirmed: IBM PCjr technical reference, BIOS source code.
 *            Some part of the code writes 0x80 to this port,
 *            then does some work, then writes 0x00.
 *
 * IBM PCjr definitions:
 *   bit[7]: Enable NMI
 *   bit[6]: IR test enable
 *   bit[5]: Select clock 1 input
 *   bit[4]: Disable HRQ */
static void pc_xt_nmi_write(Bitu port,Bitu val,Bitu iolen) {
    (void)iolen;//UNUSED
    (void)port;//UNUSED
    CPU_NMI_gate = (val & 0x80) ? true : false;
    CPU_Check_NMI();
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

unsigned int PIC_IRQ_hax[16] = { PIC_irq_hack_none };

void PIC_Set_IRQ_hack(int IRQ,unsigned int hack) {
    if (IRQ < 0 || IRQ >= 16) return;
    PIC_IRQ_hax[IRQ] = hack;
}

unsigned int PIC_parse_IRQ_hack_string(const char *str) {
    unsigned int res = PIC_irq_hack_none;
    std::string what;

    while (*str != 0) {
        while (*str == ' ') str++;
        if (*str == 0) break;

        what.clear();
        while (*str != 0 && *str != ' ')
            what += *str++;

        while (*str == ' ') str++;

        LOG_MSG("IRQ HACK: '%s'",what.c_str());

        if (what == "none")
            res  = PIC_irq_hack_none;
        else if (what == "cs_equ_ds")
            res |= PIC_irq_hack_cs_equ_ds;
    }

    return res;
}

static bool IRQ_hack_check_cs_equ_ds(const int IRQ) {
    (void)IRQ;

    uint16_t s_cs = SegValue(cs);
    uint16_t s_ds = SegValue(ds);

    if (s_cs >= 0xA000)
        return true; // don't complain about the BIOS ISR

    if (s_cs != s_ds) {
#if 0
        LOG(LOG_PIC,LOG_DEBUG)("Not dispatching IRQ %d according to IRQ hack. CS != DS",IRQ);
#endif
        return false;
    }

    return true;
}

static void slave_startIRQ(){
    Bit8u pic1_irq = 8;
    bool skipped_irq = false;
    const Bit8u p = (slave.irr & slave.imrr)&slave.isrr;
    const Bit8u max = slave.special?8:slave.active_irq;
    for(Bit8u i = 0,s = 1;i < max;i++, s<<=1) {
        if (p&s) {
            if (PIC_IRQ_hax[i+8] & PIC_irq_hack_cs_equ_ds) {
                if (!IRQ_hack_check_cs_equ_ds(i+8)) {
                    skipped_irq = true;
                    continue; // skip IRQ
                }
            }

            pic1_irq = i;
            break;
        }
    }

    if (GCC_UNLIKELY(pic1_irq == 8)) {
        if (!skipped_irq) {
            /* we have an IRQ routing problem. this code is supposed to emulate the fact that
             * what was once IRQ 2 on PC/XT is routed to IRQ 9 on AT systems, because IRQ 8-15
             * cascade to IRQ 2 on such systems. but it's nothing to E_Exit() over. */
            LOG(LOG_PIC,LOG_ERROR)("ISA PIC problem: IRQ %d (cascade) is active on master PIC without active IRQ 8-15 on slave PIC.",master_cascade_irq);
            slave.lower_irq(master_cascade_irq); /* clear it */
        }

        return;
    }

    slave.start_irq(pic1_irq);
    master.start_irq(master_cascade_irq);
    CPU_HW_Interrupt((unsigned int)slave.vector_base + (unsigned int)pic1_irq);
}

static void inline master_startIRQ(Bitu i){
    master.start_irq(i);
    CPU_HW_Interrupt(master.vector_base + i);
}

void PIC_runIRQs(void) {
    if (!GETFLAG(IF)) return;
    if (GCC_UNLIKELY(!PIC_IRQCheck)) return;
    if (GCC_UNLIKELY(cpudecoder==CPU_Core_Normal_Trap_Run)) return; // FIXME: Why?
    if (GCC_UNLIKELY(CPU_NMI_active)) return;
    if (GCC_UNLIKELY(CPU_NMI_pending)) {
        CPU_NMI_Interrupt(); // converts pending to active
        return; /* NMI has higher priority than PIC */
    }

    const Bit8u p = (master.irr & master.imrr)&master.isrr;
    Bit8u max = master.special?8:master.active_irq;
    Bit8u i,s;

    if (ignore_cascade_in_service && max == (unsigned char)master_cascade_irq)
        max++;

    for (i = 0,s = 1;i < max;i++, s<<=1){
        if (p&s) {
            if (PIC_IRQ_hax[i] & PIC_irq_hack_cs_equ_ds)
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

    if (slave.auto_eoi)
        slave.check_for_irq();
    if (master.auto_eoi)
        master.check_for_irq();

    /* continue (delayed) processing if more interrupts to handle */
    PIC_IRQCheck = 0;
    if (i != max) {
        if (!PIC_IRQCheckPending) {
            PIC_IRQCheckPending = 1;
            PIC_AddEvent(PIC_IRQCheckDelayed,(double)PIC_irq_delay_ns / 1000000,0);
        }
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

void DEBUG_PICSignal(int irq,bool raise) {
    if (irq >= 0 && irq <= 15) {
        if (raise)
            PIC_ActivateIRQ((unsigned int)irq);
        else
            PIC_DeActivateIRQ((unsigned int)irq);
    }
}

void DEBUG_PICAck(int irq) {
    if (irq >= 0 && irq <= 15) {
        PIC_Controller * pic=&pics[irq>7 ? 1 : 0];

        pic->isr &= ~(1u << ((unsigned int)irq & 7U));
        pic->isrr = (~pic->isr) | pic->isr_ignore;
        pic->check_after_EOI();
    }
}

void DEBUG_PICMask(int irq,bool mask) {
    if (irq >= 0 && irq <= 15)
        PIC_SetIRQMask((unsigned int)irq,mask);
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
static pic_tickindex_t srv_lag = 0;

pic_tickindex_t PIC_GetCurrentEventTime(void) {
    if (InEventService)
        return (pic_tickindex_t)PIC_Ticks + srv_lag;
    else
        return PIC_FullIndex();
}

void PIC_AddEvent(PIC_EventHandler handler,pic_tickindex_t delay,Bitu val) {
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

//#define DEBUG_CPU_CYCLE_OVERRUN
//#define DEBUG_PIC_IRQCHECK_VS_IRR

bool PIC_RunQueue(void) {
#if C_DEBUG
    bool IsDebuggerActive(void);
    bool IsDebuggerRunwatch(void);
    if (IsDebuggerActive() && !IsDebuggerRunwatch())
        return false;
#endif
#ifdef DEBUG_CPU_CYCLE_OVERRUN
    /* I/O delay can cause negative CPU_Cycles and PIC event / audio rendering issues */
    cpu_cycles_count_t overrun = -std::min(CPU_Cycles,(cpu_cycles_count_t)0);

    if (overrun > (CPU_CycleMax/100))
        LOG_MSG("PIC_RunQueue: CPU cycles count overrun by %ld (%.3fms)\n",(signed long)overrun,(double)overrun / CPU_CycleMax);
#endif

    /* Check to see if a new millisecond needs to be started */
    CPU_CycleLeft += CPU_Cycles;
    CPU_Cycles = 0;

#ifdef DEBUG_PIC_IRQCHECK_VS_IRR
    // WARNING: If the problem is the cascade interrupt un-acknowledged, this will give a false positive
    if (!PIC_IRQCheck && !PIC_IRQCheckPending && ((master.irr&master.imrr) != 0 || (slave.irr&slave.imrr) != 0))
        LOG_MSG("PIC_IRQCheck not set and interrupts pending");
#endif

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
        int delta = int((PIC_Ticks-PIC_tickstart)*10000/(ticks-PIC_benchstart)+5);
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
    (void)sec;//UNUSED

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
    never_mark_cascade_in_service = section->Get_bool("cascade interrupt never in service");

    {
        std::string x = section->Get_string("cascade interrupt ignore in service");

        if (x == "1" || x == "true")
            ignore_cascade_in_service = true;
        else if (x == "0" || x == "false")
            ignore_cascade_in_service = false;
        else {
            // auto
            if (IS_PC98_ARCH)
                ignore_cascade_in_service = true;
            else
                ignore_cascade_in_service = false;
        }

        LOG(LOG_MISC,LOG_DEBUG)("PIC: Ignore cascade in service=%u",ignore_cascade_in_service);
    }

    if (enable_slave_pic && machine == MCH_PCJR && enable_pc_xt_nmi_mask) {
        LOG(LOG_MISC,LOG_DEBUG)("PIC_Reset(): PCjr emulation with NMI mask register requires disabling slave PIC (IRQ 8-15)");
        enable_slave_pic = false;
    }

    if (!enable_slave_pic && IS_PC98_ARCH)
        LOG(LOG_MISC,LOG_DEBUG)("PIC_Reset(): PC-98 emulation without slave PIC (IRQ 8-15) is unusual");

    /* NTS: This is a good guess. But the 8259 is static circuitry and not driven by a clock.
     *      But the ability to respond to interrupts is limited by the CPU, too. */
    PIC_irq_delay_ns = 1000000000UL / (unsigned long)PIT_TICK_RATE;
    {
        int x = section->Get_int("irq delay ns");
        if (x >= 0) PIC_irq_delay_ns = (unsigned int)x;
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
    for (Bitu i=0;i<2;i++) {
        pics[i].auto_eoi=false;
        pics[i].rotate_on_auto_eoi=false;
        pics[i].request_issr=false;
        pics[i].special=false;
        pics[i].single=false;
        pics[i].icw_index=0;
        pics[i].icw_words=0;
        pics[i].irr = pics[i].isr = pics[i].imrr = 0;
        pics[i].isrr = pics[i].imr = 0xff;
        pics[i].isr_ignore = 0x00;
        pics[i].active_irq = 8;
    }

    /* PC-98: By default (but an option otherwise)
     *        initialize the PIC so that reading the command port
     *        produces the ISR status not the IRR status.
     *
     *        The reason the option is on by default, is that there
     *        is PC-98 programming literature that recommends reading
     *        ISR status before acknowledging interrupts (to avoid
     *        conflicts with other ISR handlers perhaps). So it's
     *        probably a common convention.
     *
     * Notes: "Blackbird" by Vivian needs this in order for the FM interrupt
     *        to continue working. A bug in the FM interrupt routine programs
     *        only the master PIC into this mode but then reads from the slave
     *        which is not necessarily initialized into this mode and may return
     *        the IRR register instead, causing the game to misinterpret
     *        incoming interrupts as in-service. */
    if (IS_PC98_ARCH) {
		Section_prop * pc98_section = static_cast<Section_prop *>(control->GetSection("pc98"));
		if (pc98_section != NULL && pc98_section->Get_bool("pc-98 pic init to read isr"))
			pics[0].request_issr = pics[1].request_issr = true;
	}

    /* IBM: IRQ 0-15 is INT 0x08-0x0F, 0x70-0x7F
     * PC-98: IRQ 0-15 is INT 0x08-0x17 */
    master.vector_base = 0x08;
    slave.vector_base = IS_PC98_ARCH ? 0x10 : 0x70;

    for (Bitu i=0;i < 16;i++)
        PIC_SetIRQMask(i,true);

    PIC_SetIRQMask(0,false);                    /* Enable system timer */
    PIC_SetIRQMask(1,false);                    /* Enable keyboard interrupt */
    PIC_SetIRQMask(8,false);                    /* Enable RTC IRQ */

    if (master_cascade_irq >= 0) {
        PIC_SetIRQMask((unsigned int)master_cascade_irq,false);/* Enable second pic */

        if (ignore_cascade_in_service)
            pics[0].isr_ignore |= 1u << (unsigned char)master_cascade_irq;
    }

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
    (void)sec;//UNUSED
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
    LOG_MSG("ICW %u/%u special=%u auto-eoi=%u rotate-eoi=%u single=%u request_issr=%u vectorbase=0x%02x active_irq=%u isr=%02x isrr=%02x isrignore=%02x",
        (unsigned int)pic.icw_index,
        (unsigned int)pic.icw_words,
        pic.special?1:0,
        pic.auto_eoi?1:0,
        pic.rotate_on_auto_eoi?1:0,
        pic.single?1:0,
        pic.request_issr?1:0,
        pic.vector_base,
        pic.active_irq,
        pic.isr,
        pic.isrr,
        pic.isr_ignore);

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
            ((unsigned int)IRQ == (unsigned int)master_cascade_irq) ? "CASCADE" : "");
    }
}

void DEBUG_LogPIC(void) {
    DEBUG_LogPIC_C(master);
    if (enable_slave_pic) DEBUG_LogPIC_C(slave);
}
#endif


