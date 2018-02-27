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


#include <math.h>
#include "dosbox.h"
#include "inout.h"
#include "pic.h"
#include "mem.h"
#include "mixer.h"
#include "timer.h"
#include "setup.h"
#include "control.h"

static INLINE void BIN2BCD(Bit16u& val) {
	Bit16u temp=val%10 + (((val/10)%10)<<4)+ (((val/100)%10)<<8) + (((val/1000)%10)<<12);
	val=temp;
}

static INLINE void BCD2BIN(Bit16u& val) {
	Bit16u temp= (val&0x0f) +((val>>4)&0x0f) *10 +((val>>8)&0x0f) *100 +((val>>12)&0x0f) *1000;
	val=temp;
}

struct PIT_Block {
	Bitu cntr;
	float delay;
	double start;

	Bit16u read_latch;
	Bit16u write_latch;

	Bit8u mode;
	Bit8u latch_mode;
	Bit8u read_state;
	Bit8u write_state;

	bool bcd;
	bool go_read_latch;
	bool new_mode;
	bool counterstatus_set;
	bool counting;
	bool update_count;
};

static PIT_Block pit[3];
static bool gate2;

static Bit8u latched_timerstatus;
// the timer status can not be overwritten until it is read or the timer was 
// reprogrammed.
static bool latched_timerstatus_locked;

unsigned long PIT_TICK_RATE = PIT_TICK_RATE_IBM;

static void PIT0_Event(Bitu /*val*/) {
	PIC_ActivateIRQ(0);
	if (pit[0].mode != 0) {
		pit[0].start += pit[0].delay;

		if (GCC_UNLIKELY(pit[0].update_count)) {
			pit[0].delay=(1000.0f/((float)PIT_TICK_RATE/(float)pit[0].cntr));
			pit[0].update_count=false;
		}
		// regression to r3533 fixes flight simulator 5.0
 		double error = 	pit[0].start - PIC_FullIndex();
 		PIC_AddEvent(PIT0_Event,(float)(pit[0].delay + error));			
//		PIC_AddEvent(PIT0_Event,pit[0].delay); // r3534
	}
}

static bool counter_output(Bitu counter) {
	PIT_Block * p=&pit[counter];
	double index=PIC_FullIndex()-p->start;
	switch (p->mode) {
	case 0:
		if (p->new_mode) return false;
		if (index>p->delay) return true;
		else return false;
		break;
	case 2:
		if (p->new_mode) return true;
		index=fmod(index,(double)p->delay);
		return index>0;
	case 3:
		if (p->new_mode) return true;
		index=fmod(index,(double)p->delay);
		return index*2<p->delay;
	case 4:
		//Only low on terminal count
		// if(fmod(index,(double)p->delay) == 0) return false; //Maybe take one rate tick in consideration
		//Easiest solution is to report always high (Space marines uses this mode)
		return true;
	default:
		LOG(LOG_PIT,LOG_ERROR)("Illegal Mode %d for reading output",p->mode);
		return true;
	}
}
static void status_latch(Bitu counter) {
	// the timer status can not be overwritten until it is read or the timer was 
	// reprogrammed.
	if(!latched_timerstatus_locked)	{
		PIT_Block * p=&pit[counter];
		latched_timerstatus=0;
		// Timer Status Word
		// 0: BCD 
		// 1-3: Timer mode
		// 4-5: read/load mode
		// 6: "NULL" - this is 0 if "the counter value is in the counter" ;)
		// should rarely be 1 (i.e. on exotic modes)
		// 7: OUT - the logic level on the Timer output pin
		if(p->bcd)latched_timerstatus|=0x1;
		latched_timerstatus|=((p->mode&7)<<1);
		if((p->read_state==0)||(p->read_state==3)) latched_timerstatus|=0x30;
		else if(p->read_state==1) latched_timerstatus|=0x10;
		else if(p->read_state==2) latched_timerstatus|=0x20;
		if(counter_output(counter)) latched_timerstatus|=0x80;
		if(p->new_mode) latched_timerstatus|=0x40;
		// The first thing that is being read from this counter now is the
		// counter status.
		p->counterstatus_set=true;
		latched_timerstatus_locked=true;
	}
}
static void counter_latch(Bitu counter) {
	/* Fill the read_latch of the selected counter with current count */
	PIT_Block * p=&pit[counter];
	p->go_read_latch=false;

	//If gate2 is disabled don't update the read_latch
	if (counter == (IS_PC98_ARCH ? 1 : 2) && !gate2 && p->mode !=1) return;

	if (GCC_UNLIKELY(p->new_mode)) {
		double passed_time = PIC_FullIndex() - p->start;
		Bitu ticks_since_then = (Bitu)(passed_time / (1000.0/PIT_TICK_RATE));
		//if (p->mode==3) ticks_since_then /= 2; // TODO figure this out on real hardware
		p->read_latch -= ticks_since_then;
		return;
	}
	double index=PIC_FullIndex()-p->start;
	switch (p->mode) {
	case 4:		/* Software Triggered Strobe */
	case 0:		/* Interrupt on Terminal Count */
		{
			/* Counter keeps on counting after passing terminal count */
			if(p->bcd) {
				index = fmod(index,(1000.0/PIT_TICK_RATE)*10000.0);
				p->read_latch = (Bit16u)(((unsigned long)(p->cntr-index*(PIT_TICK_RATE/1000.0))) % 10000UL);
			} else {
				index = fmod(index,(1000.0/PIT_TICK_RATE)*(double)0x10000);
				p->read_latch = (Bit16u)(p->cntr-index*(PIT_TICK_RATE/1000.0));
			}
		}
		break;
	case 1: // countdown
		if(p->counting) {
			if (index>p->delay) { // has timed out
				p->read_latch = 0xffff; //unconfirmed
			} else {
				p->read_latch=(Bit16u)(p->cntr-index*(PIT_TICK_RATE/1000.0));
			}
		}
		break;
	case 2:		/* Rate Generator */
		index=fmod(index,(double)p->delay);
		p->read_latch=(Bit16u)(p->cntr - (index/p->delay)*p->cntr);
		break;
	case 3:		/* Square Wave Rate Generator */
		index=fmod(index,(double)p->delay);
		index*=2;
		if (index>p->delay) index-=p->delay;
		p->read_latch=(Bit16u)(p->cntr - (index/p->delay)*p->cntr);
		// In mode 3 it never returns odd numbers LSB (if odd number is written 1 will be
		// subtracted on first clock and then always 2)
		// fixes "Corncob 3D"
		p->read_latch&=0xfffe;
		break;
	default:
		LOG(LOG_PIT,LOG_ERROR)("Illegal Mode %d for reading counter %d",(int)p->mode,(int)counter);
		p->read_latch=0xffff;
		break;
	}
}


static void write_latch(Bitu port,Bitu val,Bitu /*iolen*/) {
//LOG(LOG_PIT,LOG_ERROR)("port %X write:%X state:%X",port,val,pit[port-0x40].write_state);

    // HACK: Port translation for this code PC-98 mode.
    //       0x71,0x73,0x75,0x77 => 0x40-0x43
    if (IS_PC98_ARCH) {
        if (port >= 0x3FD9)
            port = ((port - 0x3FD9) >> 1) + 0x40;
        else
            port = ((port - 0x71) >> 1) + 0x40;
    }

	Bitu counter=port-0x40;
	PIT_Block * p=&pit[counter];
	if(p->bcd == true) BIN2BCD(p->write_latch);
   
	switch (p->write_state) {
		case 0:
			p->write_latch = p->write_latch | ((val & 0xff) << 8);
			p->write_state = 3;
			break;
		case 3:
			p->write_latch = val & 0xff;
			p->write_state = 0;
			break;
		case 1:
			p->write_latch = val & 0xff;
			break;
		case 2:
			p->write_latch = (val & 0xff) << 8;
		break;
	}
	if (p->bcd==true) BCD2BIN(p->write_latch);
   	if (p->write_state != 0) {
		if (p->write_latch == 0) {
			if (p->bcd == false) p->cntr = 0x10000;
			else p->cntr=9999;
		} else p->cntr = p->write_latch;

		if ((!p->new_mode) && (p->mode == 2) && (counter == 0)) {
			// In mode 2 writing another value has no direct effect on the count
			// until the old one has run out. This might apply to other modes too.
			// This is not fixed for PIT2 yet!!
			p->update_count=true;
			return;
		}
		p->start=PIC_FullIndex();
		p->delay=(1000.0f/((float)PIT_TICK_RATE/(float)p->cntr));

		switch (counter) {
		case 0x00:			/* Timer hooked to IRQ 0 */
			if (p->new_mode || p->mode == 0 ) {
				if(p->mode==0) PIC_RemoveEvents(PIT0_Event); // DoWhackaDo demo
				PIC_AddEvent(PIT0_Event,p->delay);
			} else LOG(LOG_PIT,LOG_NORMAL)("PIT 0 Timer set without new control word");
			LOG(LOG_PIT,LOG_NORMAL)("PIT 0 Timer at %.4f Hz mode %d",1000.0/p->delay,p->mode);
			break;
        case 0x01:          /* Timer hooked to PC-Speaker (NEC-PC98) */
            if (IS_PC98_ARCH)
                PCSPEAKER_SetCounter(p->cntr,p->mode);
            break;
        case 0x02:			/* Timer hooked to PC-Speaker (IBM PC) */
            if (!IS_PC98_ARCH)
                PCSPEAKER_SetCounter(p->cntr,p->mode);
            break;
        default:
			LOG(LOG_PIT,LOG_ERROR)("PIT:Illegal timer selected for writing");
		}
		p->new_mode=false;
    }
}

static Bitu read_latch(Bitu port,Bitu /*iolen*/) {
//LOG(LOG_PIT,LOG_ERROR)("port read %X",port);

    // HACK: Port translation for this code PC-98 mode.
    //       0x71,0x73,0x75,0x77 => 0x40-0x43
    if (IS_PC98_ARCH) {
        if (port >= 0x3FD9)
            port = ((port - 0x3FD9) >> 1) + 0x40;
        else
            port = ((port - 0x71) >> 1) + 0x40;
    }

	Bit32u counter=port-0x40;
	Bit8u ret=0;
	if(GCC_UNLIKELY(pit[counter].counterstatus_set)){
		pit[counter].counterstatus_set = false;
		latched_timerstatus_locked = false;
		ret = latched_timerstatus;
	} else {
		if (pit[counter].go_read_latch == true) 
			counter_latch(counter);

		if( pit[counter].bcd == true) BIN2BCD(pit[counter].read_latch);

		switch (pit[counter].read_state) {
		case 0: /* read MSB & return to state 3 */
			ret=(pit[counter].read_latch >> 8) & 0xff;
			pit[counter].read_state = 3;
			pit[counter].go_read_latch = true;
			break;
		case 3: /* read LSB followed by MSB */
			ret = pit[counter].read_latch & 0xff;
			pit[counter].read_state = 0;
			break;
		case 1: /* read LSB */
			ret = pit[counter].read_latch & 0xff;
			pit[counter].go_read_latch = true;
			break;
		case 2: /* read MSB */
			ret = (pit[counter].read_latch >> 8) & 0xff;
			pit[counter].go_read_latch = true;
			break;
		default:
			E_Exit("Timer.cpp: error in readlatch");
			break;
		}
		if( pit[counter].bcd == true) BCD2BIN(pit[counter].read_latch);
	}
	return ret;
}

static void write_p43(Bitu /*port*/,Bitu val,Bitu /*iolen*/) {
//LOG(LOG_PIT,LOG_ERROR)("port 43 %X",val);
	Bitu latch=(val >> 6) & 0x03;
	switch (latch) {
	case 0:
	case 1:
	case 2:
		if ((val & 0x30) == 0) {
			/* Counter latch command */
			counter_latch(latch);
		} else {
			// save output status to be used with timer 0 irq
			bool old_output = counter_output(0);
			// save the current count value to be re-used in undocumented newmode
			counter_latch(latch);
			pit[latch].bcd = (val&1)>0;   
			if (val & 1) {
				if(pit[latch].cntr>=9999) pit[latch].cntr=9999;
			}

			// Timer is being reprogrammed, unlock the status
			if(pit[latch].counterstatus_set) {
				pit[latch].counterstatus_set=false;
				latched_timerstatus_locked=false;
			}
			pit[latch].start = PIC_FullIndex(); // for undocumented newmode
			pit[latch].go_read_latch = true;
			pit[latch].update_count = false;
			pit[latch].counting = false;
			pit[latch].read_state  = (val >> 4) & 0x03;
			pit[latch].write_state = (val >> 4) & 0x03;
			Bit8u mode             = (val >> 1) & 0x07;
			if (mode > 5)
				mode -= 4; //6,7 become 2 and 3

			pit[latch].mode = mode;

			/* If the line goes from low to up => generate irq. 
			 *      ( BUT needs to stay up until acknowlegded by the cpu!!! therefore: )
			 * If the line goes to low => disable irq.
			 * Mode 0 starts with a low line. (so always disable irq)
			 * Mode 2,3 start with a high line.
			 * counter_output tells if the current counter is high or low 
			 * So actually a mode 3 timer enables and disables irq al the time. (not handled) */

			if (latch == 0) {
				PIC_RemoveEvents(PIT0_Event);
				if((mode != 0)&& !old_output) {
					PIC_ActivateIRQ(0);
				} else {
					PIC_DeActivateIRQ(0);
				}
			}
			pit[latch].new_mode = true;
			if (latch == (IS_PC98_ARCH ? 1 : 2)) {
				// notify pc speaker code that the control word was written
				PCSPEAKER_SetPITControl(mode);
			}
		}
		break;
    case 3:
		if ((val & 0x20)==0) {	/* Latch multiple pit counters */
			if (val & 0x02) counter_latch(0);
			if (val & 0x04) counter_latch(1);
			if (val & 0x08) counter_latch(2);
		}
		// status and values can be latched simultaneously
		if ((val & 0x10)==0) {	/* Latch status words */
			// but only 1 status can be latched simultaneously
			if (val & 0x02) status_latch(0);
			else if (val & 0x04) status_latch(1);
			else if (val & 0x08) status_latch(2);
		}
		break;
	}
}

void TIMER_SetGate2(bool in) {
	//No changes if gate doesn't change
	if(gate2 == in) return;
	Bit8u & mode=pit[2].mode;
	switch(mode) {
	case 0:
		if(in) pit[2].start = PIC_FullIndex();
		else {
			//Fill readlatch and store it.
			counter_latch(2);
			pit[2].cntr = pit[2].read_latch;
		}
		break;
	case 1:
		// gate 1 on: reload counter; off: nothing
		if(in) {
			pit[2].counting = true;
			pit[2].start = PIC_FullIndex();
		}
		break;
	case 2:
	case 3:
		//If gate is enabled restart counting. If disable store the current read_latch
		if(in) pit[2].start = PIC_FullIndex();
		else counter_latch(2);
		break;
	case 4:
	case 5:
		LOG(LOG_MISC,LOG_WARN)("unsupported gate 2 mode %x",mode);
		break;
	}
	gate2 = in; //Set it here so the counter_latch above works
}

bool TIMER_GetOutput2() {
	return counter_output(2);
}

#include "programs.h"

static IO_ReadHandleObject ReadHandler[4];
static IO_WriteHandleObject WriteHandler[4];

/* PC-98 alias */
static IO_ReadHandleObject ReadHandler2[4];
static IO_WriteHandleObject WriteHandler2[4];

void TIMER_BIOS_INIT_Configure() {
	PIC_RemoveEvents(PIT0_Event);
	PIC_DeActivateIRQ(0);

	/* Setup Timer 0 */
	pit[0].cntr = 0x10000;
	pit[0].write_state = 3;
	pit[0].read_state = 3;
	pit[0].read_latch = 0;
	pit[0].write_latch = 0;
	pit[0].mode = 3;
	pit[0].bcd = false;
	pit[0].go_read_latch = true;
	pit[0].counterstatus_set = false;
	pit[0].update_count = false;
	pit[0].start = PIC_FullIndex();

	pit[1].bcd = false;
	pit[1].write_state = 1;
	pit[1].read_state = 1;
	pit[1].go_read_latch = true;
	pit[1].cntr = 18;
	pit[1].mode = 2;
	pit[1].write_state = 3;
	pit[1].counterstatus_set = false;
	pit[1].start = PIC_FullIndex();

	pit[2].bcd = false;
	pit[2].write_state = 1;
	pit[2].read_state = 1;
	pit[2].go_read_latch = true;
	pit[2].cntr = 18;
	pit[2].mode = 2;
	pit[2].write_state = 3;
	pit[2].counterstatus_set = false;
	pit[2].start = PIC_FullIndex();

    /* TODO: I have observed that on real PC-98 hardware:
     * 
     *   Output 1 (speaker) does not cycle if inhibited by port 35h
     *
     *   Output 2 (RS232C) does not cycle until programmed to cycle
     *   to operate the 8251 for data transfer. It is configured by
     *   the BIOS to countdown and stop, thus the UART is not cycling
     *   until put into active use. */

    int pcspeaker_pit = IS_PC98_ARCH ? 1 : 2; /* IBM: PC speaker on output 2   PC-98: PC speaker on output 1 */

	{
		Section_prop *pcsec = static_cast<Section_prop *>(control->GetSection("speaker"));
		int freq = pcsec->Get_int("initial frequency"); /* original code: 1320 */
		int div;

        /* IBM PC defaults to 903Hz.
         * NEC PC-98 defaults to 2KHz */
        if (freq < 0)
            freq = IS_PC98_ARCH ? 2000 : 903;

		if (freq < 19) {
			div = 1;
		}
		else {
			div = PIT_TICK_RATE / freq;
			if (div > 65535) div = 65535;
		}

		pit[pcspeaker_pit].cntr = div;
		pit[pcspeaker_pit].read_latch = div;
		pit[pcspeaker_pit].write_state = 3; /* Chuck Yeager */
		pit[pcspeaker_pit].read_state = 3;
		pit[pcspeaker_pit].mode = 3;
		pit[pcspeaker_pit].bcd = false;
		pit[pcspeaker_pit].go_read_latch = true;
		pit[pcspeaker_pit].counterstatus_set = false;
		pit[pcspeaker_pit].counting = false;
	    pit[pcspeaker_pit].start = PIC_FullIndex();
	}

	pit[0].delay=(1000.0f/((float)PIT_TICK_RATE/(float)pit[0].cntr));
	pit[1].delay=(1000.0f/((float)PIT_TICK_RATE/(float)pit[1].cntr));
	pit[2].delay=(1000.0f/((float)PIT_TICK_RATE/(float)pit[2].cntr));

	PCSPEAKER_SetCounter(pit[pcspeaker_pit].cntr,pit[pcspeaker_pit].mode);
	PIC_AddEvent(PIT0_Event,pit[0].delay);

    if (IS_PC98_ARCH) {
    /* BIOS data area at 0x501 tells the DOS application which clock rate to use */
        phys_writeb(0x501,
            ((PIT_TICK_RATE == PIT_TICK_RATE_PC98_8MHZ) ? 0x80 : 0x00)      /* bit 7: 1=8MHz  0=5MHz/10MHz */
            );
    }
}

void TIMER_OnPowerOn(Section*) {
	Section_prop * section=static_cast<Section_prop *>(control->GetSection("dosbox"));
	assert(section != NULL);

	// log
	LOG(LOG_MISC,LOG_DEBUG)("TIMER_OnPowerOn(): Reinitializing PIT timer emulation");

	PIC_RemoveEvents(PIT0_Event);

	WriteHandler[0].Uninstall();
	WriteHandler[1].Uninstall();
	WriteHandler[2].Uninstall();
	WriteHandler[3].Uninstall();
	ReadHandler[0].Uninstall();
	ReadHandler[1].Uninstall();
	ReadHandler[2].Uninstall();
	ReadHandler[3].Uninstall();

	WriteHandler2[0].Uninstall();
	WriteHandler2[1].Uninstall();
	WriteHandler2[2].Uninstall();
	WriteHandler2[3].Uninstall();
	ReadHandler2[0].Uninstall();
	ReadHandler2[1].Uninstall();
	ReadHandler2[2].Uninstall();
	ReadHandler2[3].Uninstall();

	WriteHandler[0].Install(0x40,write_latch,IO_MB);
//	WriteHandler[1].Install(0x41,write_latch,IO_MB);
	WriteHandler[2].Install(0x42,write_latch,IO_MB);
	WriteHandler[3].Install(0x43,write_p43,IO_MB);
	ReadHandler[0].Install(0x40,read_latch,IO_MB);
	ReadHandler[1].Install(0x41,read_latch,IO_MB);
	ReadHandler[2].Install(0x42,read_latch,IO_MB);

	latched_timerstatus_locked=false;
	gate2 = false;

    if (IS_PC98_ARCH) {
        void TIMER_OnEnterPC98_Phase2(Section*);
        TIMER_OnEnterPC98_Phase2(NULL);
    }
}

/* NTS: This comes in two phases because we're taking ports 0x71-0x77 which overlap
 *      with ports 0x70-0x71 from CMOS emulation.
 *
 *      Phase 1 is where we unregister our I/O ports. CMOS emulation will do so as
 *      well either before or after our callback procedure.
 *
 *      Phase 2 is where we can then claim the I/O ports without our claim getting
 *      overwritten by CMOS emulation unregistering the I/O port. */

void TIMER_OnEnterPC98_Phase2(Section*) {
	Section_prop * section=static_cast<Section_prop *>(control->GetSection("dosbox"));
	assert(section != NULL);
    int pc98rate;

	PIC_RemoveEvents(PIT0_Event);

	WriteHandler[0].Uninstall();
	WriteHandler[1].Uninstall();
	WriteHandler[2].Uninstall();
	WriteHandler[3].Uninstall();
	ReadHandler[0].Uninstall();
	ReadHandler[1].Uninstall();
	ReadHandler[2].Uninstall();
	ReadHandler[3].Uninstall();

	WriteHandler2[0].Uninstall();
	WriteHandler2[1].Uninstall();
	WriteHandler2[2].Uninstall();
	WriteHandler2[3].Uninstall();
	ReadHandler2[0].Uninstall();
	ReadHandler2[1].Uninstall();
	ReadHandler2[2].Uninstall();
	ReadHandler2[3].Uninstall();

    /* PC-98 has two different rates: 5/10MHz base or 8MHz base. Let the user choose via dosbox.conf */
    pc98rate = section->Get_int("pc-98 timer master frequency");
	if (pc98rate > 6) pc98rate /= 2;
    if (pc98rate == 0) pc98rate = 5; /* Pick the most likely to work with DOS games (FIXME: This is a GUESS!! Is this correct?) */
    else if (pc98rate < 5) pc98rate = 4;
    else pc98rate = 5;

    if (pc98rate >= 5)
        PIT_TICK_RATE = PIT_TICK_RATE_PC98_10MHZ;
    else
        PIT_TICK_RATE = PIT_TICK_RATE_PC98_8MHZ;

    LOG_MSG("PC-98 PIT master clock rate %luHz",PIT_TICK_RATE);

    /* I/O port map (8254)
     *
     * IBM PC/XT/AT      NEC-PC98     A1-A0
     * -----------------------------------
     *  0x40              0x71        0
     *  0x41              0x73        1
     *  0x42              0x75        2
     *  0x43              0x77        3
     */
    /* Timer output connection
     * 
     * IBM PC/XT/AT      NEC-PC98     Timer
     * ------------------------------------
     * Timer int.        Timer int.   0
     * DRAM refresh      Speaker      1
     * Speaker           RS-232C clk  2
     *
     * If I read documentation correctly, PC-98 wires timer output 2
     * to the clock pin of the 8251 UART for COM1 as a way to set the
     * baud rate. */

    /* This code is written to eventually copy-paste out in general */
	WriteHandler[0].Install(IS_PC98_ARCH ? 0x71 : 0x40,write_latch,IO_MB);
	WriteHandler[1].Install(IS_PC98_ARCH ? 0x73 : 0x41,write_latch,IO_MB);
	WriteHandler[2].Install(IS_PC98_ARCH ? 0x75 : 0x42,write_latch,IO_MB);
	WriteHandler[3].Install(IS_PC98_ARCH ? 0x77 : 0x43,write_p43,IO_MB);
	ReadHandler[0].Install(IS_PC98_ARCH ? 0x71 : 0x40,read_latch,IO_MB);
	ReadHandler[1].Install(IS_PC98_ARCH ? 0x73 : 0x41,read_latch,IO_MB);
	ReadHandler[2].Install(IS_PC98_ARCH ? 0x75 : 0x42,read_latch,IO_MB);

    /* Apparently all but the first PC-9801 systems have an alias of these
     * ports at 0x3FD9-0x3FDF odd. This alias is required for games that
     * rely on this alias. */
    if (IS_PC98_ARCH) {
        WriteHandler2[0].Install(0x3FD9,write_latch,IO_MB);
        WriteHandler2[1].Install(0x3FDB,write_latch,IO_MB);
        WriteHandler2[2].Install(0x3FDD,write_latch,IO_MB);
        WriteHandler2[3].Install(0x3FDF,write_p43,IO_MB);
        ReadHandler2[0].Install(0x3FD9,read_latch,IO_MB);
        ReadHandler2[1].Install(0x3FDB,read_latch,IO_MB);
        ReadHandler2[2].Install(0x3FDD,read_latch,IO_MB);
    }

	latched_timerstatus_locked=false;
	gate2 = false;

    TIMER_BIOS_INIT_Configure();
}

void TIMER_Destroy(Section*) {
	PIC_RemoveEvents(PIT0_Event);
}

void TIMER_Init() {
	Bitu i;

	LOG(LOG_MISC,LOG_DEBUG)("TIMER_Init()");

    PIT_TICK_RATE = PIT_TICK_RATE_IBM;

	for (i=0;i < 3;i++) {
		pit[i].cntr = 0x10000;
		pit[i].write_state = 0;
		pit[i].read_state = 0;
		pit[i].read_latch = 0;
		pit[i].write_latch = 0;
		pit[i].mode = 0;
		pit[i].bcd = false;
		pit[i].go_read_latch = false;
		pit[i].counterstatus_set = false;
		pit[i].update_count = false;
		pit[i].delay = (1000.0f/((float)PIT_TICK_RATE/(float)pit[i].cntr));
	}

	AddExitFunction(AddExitFunctionFuncPair(TIMER_Destroy));
	AddVMEventFunction(VM_EVENT_POWERON, AddVMEventFunctionFuncPair(TIMER_OnPowerOn));

    if (IS_PC98_ARCH) /* HACK! Clean this up! */
    	AddVMEventFunction(VM_EVENT_RESET, AddVMEventFunctionFuncPair(TIMER_OnEnterPC98_Phase2));
}

