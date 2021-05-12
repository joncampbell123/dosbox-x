/*
 *  Copyright (C) 2002-2021  The DOSBox Team
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


#include <math.h>
#include "dosbox.h"
#include "inout.h"
#include "pic.h"
#include "cpu.h"
#include "mem.h"
#include "mixer.h"
#include "timer.h"
#include "setup.h"
#include "control.h"

// This is only set in PC-98 mode and only if emulating PC-9801.
// There is at least one game (PC-98 port of Thexder) that depends on PC-9801 PIT 1
// behavior where the counter cycles at all times whether or not the PC speaker is
// "on". This does not force the PC speaker output on (does not force an audible beep),
// it only forces the clock gate on and PIT 1 to cycle.
bool speaker_clock_lock_on = false;

static INLINE void BIN2BCD(uint16_t& val) {
	uint16_t temp=val%10 + (((val/10)%10)<<4)+ (((val/100)%10)<<8) + (((val/1000)%10)<<12);
	val=temp;
}

static INLINE void BCD2BIN(uint16_t& val) {
	uint16_t temp= (val&0x0f) +((val>>4)&0x0f) *10 +((val>>8)&0x0f) *100 +((val>>12)&0x0f) *1000;
	val=temp;
}

struct PIT_Block {
    struct read_counter_result {
        uint16_t          counter = 0xFFFFu;
        uint16_t          cycle = 0;          // cycle (Mode 3: 0 or 1)
    };

    Bitu cntr = 0;          /* counter value written to 40h-42h as the interval. may take effect immediately (after port 43h) or after count expires */
    Bitu cntr_cur = 0;      /* current counter value in effect */
    double delay = 0;       /* interval (in ms) between one full count cycle */
    double start = 0;       /* time base (in ms) that cycle started at */
    double now = 0;         /* current time (in ms) */

    uint16_t read_latch = 0;  /* counter value, latched for read back */
    uint16_t write_latch = 0; /* counter value, written by host */

    uint8_t mode = 0;         /* 8254 mode (mode 0 through 5 inclusive) */
    uint8_t read_state = 0;   /* 0=read MSB, switch to LSB, 1=LSB only, 2=MSB only, 3=read LSB, switch to MSB, latch next value */
    uint8_t write_state = 0;  /* 0=write MSB, switch to LSB, 1=LSB only, 2=MSB only, 3=write MSB, switch to LSB, accept value */
    uint8_t cycle_base = 0;

    bool bcd = false;               /* BCD mode */
    bool go_read_latch = false;     /* reading should latch another value */
    bool new_mode = false;          /* a new mode has been written to port 43h for this timer */
    bool counterstatus_set = false; /* set by status_latch(), when using 8254 command to latch multiple counters */
    bool counting = false;          /* is counting (?) */
    bool update_count = false;      /* update count on completion */

    bool gate = true;       /* gate signal (IN) */
    bool output = true;     /* output signal (OUT) */

    read_counter_result     last_counter;       /* what to return when gate == false (not counting) */

    void set_output(bool on) {
        output = on;
        // TODO: Event callback
    }

    void set_next_counter(Bitu new_cntr) {
        update_count = true;
        cntr = new_cntr;
    }
    void set_active_counter(Bitu new_cntr) {
        assert(new_cntr != 0);

        cntr_cur = new_cntr;
        delay = ((double)(1000ul * cntr_cur)) / PIT_TICK_RATE;
    }
    void latch_next_counter(void) {
        set_active_counter(cntr);
    }
    void reset_count_at(pic_tickindex_t t) {
        start = now = t;
        cycle_base = 0;
    }
    void restart_counter_at(pic_tickindex_t t,uint16_t counter) {
        double c_delay;

        if (counter == 0)
            c_delay = ((double)(1000ull * 0x10000)) / PIT_TICK_RATE;
        else
            c_delay = ((double)(1000ull * counter)) / PIT_TICK_RATE;

        start = (t - c_delay);
    }
    void track_time(pic_tickindex_t t) {
        now = t;

        /* Mode 0 will always reset the count whether "new mode" or not.
         * Mode 1 will count down and stop. TODO: Writing a new counter without "new mode" starts another countdown? */
        /* if any periodic mode (Mode 2, 3, 4, 5), then process fully. */
        if (mode == 3) {
            const double half = delay / 2;

            if (now >= (start+half)) {
                cycle_base = (cycle_base + 1u) & 1u;
                start += half;

                if (update_count) {
                    latch_next_counter();
                    update_count = false;
                }

                if (now >= (start+half)) {
                    unsigned int cnt = (unsigned int)floor((now - start) / half);
                    cycle_base = (cycle_base + cnt) & 1u;
                    start += cnt * half;
                }
            }
        }
        else if (mode >= 2) {
            if (now >= (start+delay)) {
                start += delay;

                if (update_count) {
                    latch_next_counter();
                    update_count = false;
                }

                if (now >= (start+delay))
                    start += floor((now - start) / delay) * delay;
            }
        }

        if (now < start)
            now = start;
    }
    double reltime(void) const {
        return now - start;
    }

    void set_gate(bool on) {
        if (gate != on) {
            if (!on)/*on=false gate=true*/
                last_counter = read_counter();

            // restart aka "trigger" the counters
            switch (mode) {
                case 0:     /* Interrupt on Terminal Count */
                case 4:     /* Software Triggered Strobe */
                    restart_counter_at(now,last_counter.counter);
                    break;
                case 1:     /* Hardware Triggered one-shot */
                    /* output goes LOW when triggered, returns HIGH when counter expires */
                    if (on) {
                        reset_count_at(now);
                        latch_next_counter();
                        set_output(false);
                    }
                    /* TODO */
                    break;
                case 2:     /* Rate Generator */
                    /* output goes HIGH immediately */
                    if (on) {
                        reset_count_at(now);
                        latch_next_counter();
                    }
                    else {
                        set_output(true);
                    }
                    /* TODO */
                    break;
                case 3:     /* Square Wave Mode */
                    if (on) {
                        reset_count_at(now);
                        latch_next_counter();
                    }
                    else {
                        set_output(true);
                    }
                    /* TODO */
                    break;
                case 5:     /* Hardware Triggered Strobe */
                    if (on) {
                        reset_count_at(now);
                        latch_next_counter();
                        set_output(true);
                    }
                    break;
            }

            gate = on;
        }
    }

    void update_output_from_counter(const read_counter_result &res) {
        set_output(get_output_from_counter(res));
    }

    bool get_output_from_counter(const read_counter_result &res) {
        switch (mode) {
            case 0:
                if (new_mode) return false;
                if (res.cycle != 0u/*index > delay*/) return true;
                else return false;
            case 2:
                if (new_mode) return true;
                return res.counter != 0;
            case 3:
                if (new_mode) return true;
                return res.cycle == 0;
            case 4:
                return true;
            default:
                break;
        }

        return true;
    }

    read_counter_result read_counter(void) const {//This assumes you call track_time()
        if (!gate)
            return last_counter;

        const double index = reltime();
        read_counter_result ret;

        switch (mode) {
            case 4:		/* Software Triggered Strobe */
            case 0:		/* Interrupt on Terminal Count */
                {
                    double tmp;

                    /* Counter keeps on counting after passing terminal count */
                    if (bcd) {
                        tmp = fmod(index,((double)(1000ul *   10000ul)) / PIT_TICK_RATE);
                        ret.counter = (uint16_t)(((unsigned long)(cntr_cur - ((tmp * PIT_TICK_RATE) / 1000.0))) %   10000ul);
                    } else {
                        tmp = fmod(index,((double)(1000ul * 0x10000ul)) / PIT_TICK_RATE);
                        ret.counter = (uint16_t)(((unsigned long)(cntr_cur - ((tmp * PIT_TICK_RATE) / 1000.0))) % 0x10000ul);
                    }

                    if (mode == 0) {
                        if (index > delay)
                            ret.cycle = 1;
                    }
                }
                break;
            case 5:     /* Hardware Triggered Strobe */
            case 1:     /* Hardware Retriggerable one-shot */
                if (index > delay) // has timed out
                    ret.counter = 0xFFFF;
                else
                    ret.counter = (uint16_t)(cntr_cur - (index * (PIT_TICK_RATE / 1000.0)));
                break;
            case 2:		/* Rate Generator */
                ret.counter = (uint16_t)(cntr_cur - ((fmod(index,delay) / delay) * cntr_cur));
                break;
            case 3:		/* Square Wave Rate Generator */
                {
                    double tmp = fmod(index,(double)delay) * 2;

                    if (tmp < 0) {
                        fprintf(stderr,"tmp %.9f index %.9f delay %.9f now %.3f start %.3f\n",tmp,index,delay,now,start);
                        abort();
                    }

                    ret.cycle = cycle_base;
                    if (tmp >= delay) {
                        tmp -= delay;
                        ret.cycle = (ret.cycle + 1u) & 1u;
                    }

                    ret.counter = ((uint16_t)(cntr_cur - ((tmp * cntr_cur) / delay))) & 0xFFFEu; /* always even value */
                }
                break;
            default:
                break;
        }

        return ret;
    }
};

static PIT_Block pit[3];

static uint8_t latched_timerstatus;
// the timer status can not be overwritten until it is read or the timer was 
// reprogrammed.
static bool latched_timerstatus_locked;

unsigned long PIT_TICK_RATE = PIT_TICK_RATE_IBM;

static void PIT0_Event(Bitu /*val*/) {
	PIC_ActivateIRQ(0);
	if (pit[0].mode != 0) {
		pit[0].track_time(PIC_FullIndex());

        /* event timing error checking */
        double err = PIC_GetCurrentEventTime() - pit[0].start;

        if (err >= (pit[0].delay/2))
            err -=  pit[0].delay;

#if 0//change if debug information wanted
        if (fabs(err) >= (0.5 / CPU_CycleMax))
            LOG_MSG("PIT0_Event timing error %.6fms",err);
#endif

        PIC_AddEvent(PIT0_Event,pit[0].delay - (err * 0.05));
	}
}

uint32_t PIT0_GetAssignedCounter(void) {
    return (uint32_t)pit[0].cntr;
}

static bool counter_output(Bitu counter) {
	PIT_Block *p = &pit[counter];
    p->track_time(PIC_FullIndex());

    PIT_Block::read_counter_result res = p->read_counter();
    p->update_output_from_counter(res);

    return p->output;
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

static void counter_latch(Bitu counter,bool do_latch=true) {
	PIT_Block *p = &pit[counter];

    p->track_time(PIC_FullIndex());

    PIT_Block::read_counter_result res = p->read_counter();
    p->update_output_from_counter(res);

    if (do_latch) {
        p->go_read_latch = false;
        p->read_latch = res.counter;
    }

    if (counter == 0/*IRQ 0*/) {
        if (!p->output)
            PIC_DeActivateIRQ(0);
    }
}

void TIMER_IRQ0Poll(void) {
    counter_latch(0,false/*do not latch*/);
}

pic_tickindex_t speaker_pit_delta(void) {
    unsigned int speaker_pit = IS_PC98_ARCH ? 1 : 2;
    return fmod(pit[speaker_pit].now - pit[speaker_pit].start, pit[speaker_pit].delay);
}

void speaker_pit_update(void) {
    unsigned int speaker_pit = IS_PC98_ARCH ? 1 : 2;
    pit[speaker_pit].track_time(PIC_FullIndex());
}

void PCSPEAKER_UpdateType(void);

bool TIMER2_ClockGateEnabled(void) {
    /* PC speaker emulation should treat "new mode" as if the clock gate is disabled.
     * On real hardware, mode 3 does not cycle if you write a control word but then
     * do not write a counter value. */
    return !pit[IS_PC98_ARCH ? 1 : 2].new_mode;
}

static void write_latch(Bitu port,Bitu val,Bitu /*iolen*/) {
//LOG(LOG_PIT,LOG_ERROR)("port %X write:%X state:%X",port,val,pit[port-0x40].write_state);

    // HACK: Port translation for this code PC-98 mode.
    //       0x71,0x73,0x75,0x77 => 0x40-0x43
    if (IS_PC98_ARCH) {
        if (port >= 0x3FD9)
            port = ((port - 0x3FD9) >> 1) + 0x40;
        else if (port >=0x71 && port <= 0x75)
            port = ((port - 0x71) >> 1) + 0x40;
        else {
            E_Exit("PIT: PC-98 port in write_latch is out of range.");
            return;
        }
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
        Bitu old_cntr = p->cntr;

        p->track_time(PIC_FullIndex());

        if (p->write_latch == 0) {
            if (p->bcd == false)
                p->set_next_counter(0x10000);
            else
                p->set_next_counter(9999/*check this*/);
        }
        else if (p->write_latch == 1 && p->mode == 3/*square wave, count by 2*/) { /* counter==1 and mode==3 makes a low frequency buzz (Paratrooper) */
            if (p->bcd == false)
                p->set_next_counter(0x10001);
            else
                p->set_next_counter(10000/*check this*/);
        }
        else {
            p->set_next_counter(p->write_latch);
        }

        if (!p->new_mode) {
            if ((p->mode == 2/*common IBM PC mode*/ || p->mode == 3/*common PC-98 mode*/) && (counter == 0)) {
                // In mode 2 writing another value has no direct effect on the count
                // until the old one has run out. This might apply to other modes too.
                // This is not fixed for PIT2 yet!!
                p->update_count=true;
                return;
            }
            else if ((p->mode == 3) && (counter == (IS_PC98_ARCH ? 1 : 2))) {
                void PCSPEAKER_SetCounter_NoNewMode(Bitu cntr);

                // PC speaker
                PCSPEAKER_SetCounter_NoNewMode(p->cntr);
                p->update_count=true;
                return;
            }

            if (p->mode == 0) {
                /* Mode 0 is the only mode NOT to wait for the current counter to finish if you write another counter value
                 * according to the Intel 8254 datasheet.
                 *
                 * For timer 0 (system timer) this is used by DoWhackaDo as a sort of one-shot timer interrupt.
                 * For timer 2 (PC speaker) this is used to do PWM "realsound" digitized speech in some games. */
            }
            else {
                // this debug message will help development trace down cases where writing without a new mode
                // would incorrectly restart the counter instead of letting the current count complete before
                // writing a new one.
                LOG(LOG_PIT,LOG_NORMAL)("WARNING: Writing counter %u in mode %u without writing port 43h not yet supported, will be handled as if new mode and reset of the cycle",(int)counter,(int)p->mode);
            }
        }

        p->reset_count_at(PIC_FullIndex());
        p->latch_next_counter();

		p->new_mode=false;
		switch (counter) {
		case 0x00:			/* Timer hooked to IRQ 0 */
            PIC_RemoveEvents(PIT0_Event);
            PIC_AddEvent(PIT0_Event,p->delay);

#if 0//change to #if 1 if you want to debug Mode 0 one-shot events
            if (p->mode == 0)
                LOG(LOG_PIT,LOG_NORMAL)("PIT 0 Timer one-shot event %.3fms",p->delay);
#endif

            //please do not spam the log and console if a game is writing the SAME counter value constantly,
            //and do not spam the console if Mode 0 is used because events are not consistent.
            if (p->cntr != old_cntr && p->mode != 0)
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
    }
    else { /* write state == 0 */
        /* If a new count is written to the Counter, it will be
         * loaded on the next CLK pulse and counting will con-
         * tinue from the new count. If a two-byte count is writ-
         * ten, the following happens:
         * 1) Writing the first byte disables counting. OUT is set
         * low immediately (no clock pulse required)
         * 2) Writing the second byte allows the new count to
         * be loaded on the next CLK pulse. */
        if (p->mode == 0) {
            if (counter == 0) {
                PIC_RemoveEvents(PIT0_Event);
                PIC_DeActivateIRQ(0);
            }
            p->update_count = false;
        }
    }
}

static Bitu read_latch(Bitu port,Bitu /*iolen*/) {
//LOG(LOG_PIT,LOG_ERROR)("port read %X",port);

    // HACK: Port translation for this code PC-98 mode.
    //       0x71,0x73,0x75,0x77 => 0x40-0x43
    if (IS_PC98_ARCH) {
        if (port >= 0x3FD9)
            port = ((port - 0x3FD9) >> 1) + 0x40;
        else if (port >=0x71 && port <= 0x75)
            port = ((port - 0x71) >> 1) + 0x40;
        else {
            E_Exit("PIT: PC-98 port in read_latch is out of range.");
            return 0;
        }
    }

	uint32_t counter=(uint32_t)(port-0x40);
	uint8_t ret=0;
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
//			pit[latch].reset_count_at(PIC_FullIndex()); // for undocumented newmode
			pit[latch].go_read_latch = true;
			pit[latch].update_count = false;
			pit[latch].counting = false;
			pit[latch].read_state  = (val >> 4) & 0x03;
			pit[latch].write_state = (val >> 4) & 0x03;
			uint8_t mode             = (val >> 1) & 0x07;
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

            /* Jon C: Oh yeah? Nobody abuses counter == 0 on IBM PC that way, but there is a PC-98
             *        game that relies on that behavior: Steel Gun Nyan! */

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
				// notify pc speaker code that the control word was written.
                // until a counter value is written, the PC speaker should
                // treat the timer as if the clock gate were disabled.
                PCSPEAKER_UpdateType();
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

// FIXME: I am assuming that the "buzzer inhibit" on PC-98 controls the "trigger" pin
//        that either enables the PIT to count or stops it and resets the counter.
//        Verify this on real hardware (DOSLIB TPCRAPI6.EXE)
//
//        Note that on IBM PC/XT hardware, ports 60h-63h are the same PPI used in the
//        PC-98 systems, though wired differently. It is configured (According to IBM).
//           - Port A (input)           Keyboard scan code / SW1 dip switches (depends on port 61h bit 7)
//           - Port B (output)          Timer 2 gate speaker / Speaker data aka output gate / ... / bit 7 set to clear keyboard and read SW1
//           - Port C (input)           I/O Read/Write Memory SW2 / Cassette Data In / Timer Channel 2 Out / ...
//           - Command byte             0x99 (IBM Technical Ref listing)
//
//        This is the picture I have of the hardware:
//
//        IBM PC/XT:
//
//        Port 61h
//        - bit 0 PIT 2 counter gate (write)
//        - bit 1 PIT 2 counter output gate (write)
//        Port 62h
//        - bit 5 PIT 2 counter output (read). The connection point lies BEFORE the AND gate.
//            You will see the output toggle even if the speaker was muted by clearing the output gate bit.
//
//        IBM PC/AT:
//
//        Port 61h
//        - bit 0 PIT 2 counter gate (write)
//        - bit 1 PIT 2 counter output gate (write)
//        - bit 5 PIT 2 counter output (read). The connection point lies BEFORE the AND gate.
//            You will see the output toggle even if the speaker was muted by clearing the output gate bit.
//
//        PC-98:
//
//        Port 35h (Intel 8255 PPI Port C)
//        - bit 3 PIT 1 counter gate (there is no output gate). Setting the bit inhibits the counter (and therefore PC speaker)
//        - On PC-9821, this bit controls the clock gate of PIT 1 and therefore whether the PC speaker makes sound
//        - On PC-9801, the clock gate of PIT 1 is always on, and this bit controls whether the PC speaker makes sound
//
//        IBM PC/XT/AT:
//
//        counter output readback <- --------+
//                                           |
//                        +------+           |        +----------+
//        counter gate -> | 8254 | -> PIT 2 output -> | AND GATE | -> PC speaker
//                        +------+                    +----------+
//                                                         |
//        counter output gate -> --------------------------+
//
//        PC-9821:
//
//                        +------+
//        counter gate -> | 8254 | -> PC speaker
//                        +------+
//
//        PC-9801:
//
//                        +------+    +----------+
//        logic high ->   | 8254 | -> | AND GATE | -> PC speaker
//         (always on)    +------+    +----------+
//                                         |
//        inverse of bit 3 of port 37h ----+
//          (bit 3 is inhibit)
void TIMER_SetGate2(bool in) {
    unsigned int speaker_pit = IS_PC98_ARCH ? 1 : 2;
    pit[speaker_pit].track_time(PIC_FullIndex());
    pit[speaker_pit].set_gate(in || speaker_clock_lock_on);
}

bool TIMER_GetOutput2() {
    unsigned int speaker_pit = IS_PC98_ARCH ? 1 : 2;//NTS: For completion sake, even though there is no readback bit on PC-98

	return counter_output(speaker_pit);
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
    pit[0].output = true;
    pit[0].gate = true;
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
	pit[0].reset_count_at(PIC_FullIndex());
    pit[0].track_time(PIC_FullIndex());

    pit[1].output = true;
    pit[1].gate = true;
	pit[1].bcd = false;
	pit[1].read_state = 1;
	pit[1].go_read_latch = true;
	pit[1].cntr = 18;
	pit[1].mode = 2;
	pit[1].write_state = 3;
	pit[1].counterstatus_set = false;
	pit[1].reset_count_at(PIC_FullIndex());
    pit[1].track_time(PIC_FullIndex());

    pit[2].output = true;
    pit[2].gate = false;
	pit[2].bcd = false;
	pit[2].read_state = 1;
	pit[2].go_read_latch = true;
	pit[2].cntr = 18;
	pit[2].mode = 2;
	pit[2].write_state = 3;
	pit[2].counterstatus_set = false;
	pit[2].reset_count_at(PIC_FullIndex());
    pit[2].track_time(PIC_FullIndex());

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
		unsigned int div;

        /* IBM PC defaults to 903Hz.
         * NEC PC-98 defaults to 2KHz */
        if (freq < 0)
            freq = IS_PC98_ARCH ? 2000 : 903;

		if (freq < 1) {
			div = 65535;
		}
		else {
			div = (unsigned int)PIT_TICK_RATE / (unsigned int)freq;
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
	    pit[pcspeaker_pit].reset_count_at(PIC_FullIndex());
	}

	pit[0].latch_next_counter();
	pit[1].latch_next_counter();
	pit[2].latch_next_counter();

	PCSPEAKER_SetCounter(pit[pcspeaker_pit].cntr,pit[pcspeaker_pit].mode);
	PIC_AddEvent(PIT0_Event,pit[0].delay);

    if (IS_PC98_ARCH) {
    /* BIOS data area at 0x501 tells the DOS application which clock rate to use */
        phys_writeb(0x501,
            (phys_readb(0x501) & 0x7F) |
            ((PIT_TICK_RATE == PIT_TICK_RATE_PC98_8MHZ) ? 0x80 : 0x00)      /* bit 7: 1=8MHz  0=5MHz/10MHz */
            );

        /* Turn off PC speaker.
         * Note for PC9801 behavior this will help start the PIT cycling anyway. */
        TIMER_SetGate2(false);

        /* The timer is always on, there's no clock gate that I know of.
         * There's a bit 6 port 434h that might gate it on some hardware, but that doesn't seem to be the case on anything I have.
         *
         * NTS: If you run 8254.EXE from DOSLIB on PC-98 hardware and notice PIT 2 isn't cycling, try writing values to 75h
         *      and see if it begins counting again. A PC-9821Lt2 laptop seems to have a bios that writes a mode byte for
         *      it to 77h but then never writes to 75h, which leaves the timer idle. */
        pit[2].track_time(PIC_FullIndex());
        pit[2].set_gate(true);
    }
}

void TIMER_OnPowerOn(Section*) {
	Section_prop * pc98_section=static_cast<Section_prop *>(control->GetSection("pc98"));
	assert(pc98_section != NULL);

	// log
	LOG(LOG_MISC,LOG_DEBUG)("TIMER_OnPowerOn(): Reinitializing PIT timer emulation");

	PIC_RemoveEvents(PIT0_Event);

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

    if (IS_PC98_ARCH) {
        /* This code is written to eventually copy-paste out in general */
        WriteHandler[0].Install(0x71,write_latch,IO_MB);
        WriteHandler[1].Install(0x73,write_latch,IO_MB);
        WriteHandler[2].Install(0x75,write_latch,IO_MB);
        WriteHandler[3].Install(0x77,write_p43,IO_MB);
        ReadHandler[0].Install(0x71,read_latch,IO_MB);
        ReadHandler[1].Install(0x73,read_latch,IO_MB);
        ReadHandler[2].Install(0x75,read_latch,IO_MB);

        /* Apparently all but the first PC-9801 systems have an alias of these
         * ports at 0x3FD9-0x3FDF odd. This alias is required for games that
         * rely on this alias. */
        WriteHandler2[0].Install(0x3FD9,write_latch,IO_MB);
        WriteHandler2[1].Install(0x3FDB,write_latch,IO_MB);
        WriteHandler2[2].Install(0x3FDD,write_latch,IO_MB);
        WriteHandler2[3].Install(0x3FDF,write_p43,IO_MB);
        ReadHandler2[0].Install(0x3FD9,read_latch,IO_MB);
        ReadHandler2[1].Install(0x3FDB,read_latch,IO_MB);
        ReadHandler2[2].Install(0x3FDD,read_latch,IO_MB);
    }
    else {
        WriteHandler[0].Install(0x40,write_latch,IO_MB);
//	    WriteHandler[1].Install(0x41,write_latch,IO_MB);
        WriteHandler[2].Install(0x42,write_latch,IO_MB);
        WriteHandler[3].Install(0x43,write_p43,IO_MB);
        ReadHandler[0].Install(0x40,read_latch,IO_MB);
        ReadHandler[1].Install(0x41,read_latch,IO_MB);
        ReadHandler[2].Install(0x42,read_latch,IO_MB);
    }

	latched_timerstatus_locked=false;

    if (IS_PC98_ARCH) {
        int pc98rate;

        {
            const char *s = pc98_section->Get_string("pc-98 timer always cycles");

            if (!strcmp(s,"true") || !strcmp(s,"1"))
                speaker_clock_lock_on = true; // PC-9801 behavior
            else if (!strcmp(s,"false") || !strcmp(s,"0"))
                speaker_clock_lock_on = false; // PC-9821 behavior
            else // anything else is handled as "auto"
                speaker_clock_lock_on = false; // PC-9821 behavior
        }

        /* PC-98 has two different rates: 5/10MHz base or 8MHz base. Let the user choose via dosbox.conf */
        pc98rate = pc98_section->Get_int("pc-98 timer master frequency");
        if (pc98rate > 6) pc98rate /= 2;
        if (pc98rate == 0) pc98rate = 5; /* Pick the most likely to work with DOS games (FIXME: This is a GUESS!! Is this correct?) */
        else if (pc98rate < 5) pc98rate = 4;
        else pc98rate = 5;

        if (pc98rate >= 5)
            PIT_TICK_RATE = PIT_TICK_RATE_PC98_10MHZ;
        else
            PIT_TICK_RATE = PIT_TICK_RATE_PC98_8MHZ;

        LOG_MSG("PC-98 PIT master clock rate %luHz",PIT_TICK_RATE);

        latched_timerstatus_locked=false;
    }
}

void TIMER_OnEnterPC98_Phase2_UpdateBDA(void) {
	if (!cpu.pmode) {
		/* BIOS data area at 0x501 tells the DOS application which clock rate to use */
		phys_writeb(0x501,
            (phys_readb(0x501) & 0x7F) |
			((PIT_TICK_RATE == PIT_TICK_RATE_PC98_8MHZ) ? 0x80 : 0x00)      /* bit 7: 1=8MHz  0=5MHz/10MHz */
		);
	}
	else {
		LOG_MSG("PC-98 warning: PIT timer change cannot be reflected to BIOS data area in protected/vm86 mode");
	}
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
        pit[i].latch_next_counter();
	}

	AddExitFunction(AddExitFunctionFuncPair(TIMER_Destroy));
	AddVMEventFunction(VM_EVENT_POWERON, AddVMEventFunctionFuncPair(TIMER_OnPowerOn));
}

//save state support
void *PIT0_Event_PIC_Event = (void*)((uintptr_t)PIT0_Event);

namespace
{
class SerializeTimer : public SerializeGlobalPOD
{
public:
    SerializeTimer() : SerializeGlobalPOD("IntTimer10")
    {
        registerPOD(pit);
        //registerPOD(gate2);
        registerPOD(latched_timerstatus);
		registerPOD(latched_timerstatus_locked);
    }
} dummy;
}
