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


#include <string.h>
#include "dosbox.h"
#include "inout.h"
#include "pic.h"
#include "setup.h"
#include "cpu.h"
#include "support.h"
#include "control.h"
#include "setup.h"

extern bool enable_slave_pic;

void MIDI_RawOutByte(Bit8u data);
bool MIDI_Available(void);

static void MPU401_Event(Bitu);
static void MPU401_Reset(void);
static void MPU401_ResetDone(Bitu);
static void MPU401_EOIHandler(Bitu val=0);
static void MPU401_EOIHandlerDispatch(void);

#define MPU401_VERSION	0x15
#define MPU401_REVISION	0x01
#define MPU401_QUEUE 32
#define MPU401_TIMECONSTANT (60000000/1000.0f)
#define MPU401_RESETBUSY 14.0f

enum MpuMode { M_UART,M_INTELLIGENT };
enum MpuDataType {T_OVERFLOW,T_MARK,T_MIDI_SYS,T_MIDI_NORM,T_COMMAND};

static void MPU401_WriteData(Bitu port,Bitu val,Bitu iolen);

/* Messages sent to MPU-401 from host */
#define MSG_EOX	                        0xf7
#define MSG_OVERFLOW                    0xf8
#define MSG_MARK                        0xfc

/* Messages sent to host from MPU-401 */
#define MSG_MPU_OVERFLOW                0xf8
#define MSG_MPU_COMMAND_REQ             0xf9
#define MSG_MPU_END                     0xfc
#define MSG_MPU_CLOCK                   0xfd
#define MSG_MPU_ACK                     0xfe

static struct {
	bool intelligent;
	MpuMode mode;
	Bitu irq;
	Bit8u queue[MPU401_QUEUE];
	Bitu queue_pos,queue_used;
	struct track {
		Bits counter;
		Bit8u value[8],sys_val;
		Bit8u vlength,length;
		MpuDataType type;
	} playbuf[8],condbuf;
	struct {
		bool conductor,cond_req,cond_set, block_ack;
		bool playing,reset;
		bool wsd,wsm,wsd_start;
		bool run_irq,irq_pending;
		bool send_now;
		bool eoi_scheduled;
		Bits data_onoff;
		Bitu command_byte,cmd_pending;
		Bit8u tmask,cmask,amask;
		Bit16u midi_mask;
		Bit16u req_mask;
		Bit8u channel,old_chan;
	} state;
	struct {
		Bit8u timebase,old_timebase;
		Bit8u tempo,old_tempo;
		Bit8u tempo_rel,old_tempo_rel;
		Bit8u tempo_grad;
		Bit8u cth_rate,cth_counter;
		bool clock_to_host,cth_active;
	} clock;
} mpu;

int MPU401_GetIRQ() {
	return (int)mpu.irq;
}

static void QueueByte(Bit8u data) {
	if (mpu.state.block_ack) {mpu.state.block_ack=false;return;}
	if (mpu.queue_used==0 && mpu.intelligent) {
		mpu.state.irq_pending=true;
		PIC_ActivateIRQ(mpu.irq);
	}
	if (mpu.queue_used<MPU401_QUEUE) {
		Bitu pos=mpu.queue_used+mpu.queue_pos;
		if (mpu.queue_pos>=MPU401_QUEUE) mpu.queue_pos-=MPU401_QUEUE;
		if (pos>=MPU401_QUEUE) pos-=MPU401_QUEUE;
		mpu.queue_used++;
		mpu.queue[pos]=data;
	} else LOG(LOG_MISC,LOG_NORMAL)("MPU401:Data queue full");
}

static void ClrQueue(void) {
	mpu.queue_used=0;
	mpu.queue_pos=0;
}

static Bitu MPU401_ReadStatus(Bitu port,Bitu iolen) {
    (void)iolen;//UNUSED
    (void)port;//UNUSED
	Bit8u ret=0x3f;	/* Bits 6 and 7 clear */
	if (mpu.state.cmd_pending) ret|=0x40;
	if (!mpu.queue_used) ret|=0x80;
	return ret;
}

static void MPU401_WriteCommand(Bitu port,Bitu val,Bitu iolen) {
    (void)iolen;//UNUSED
    (void)port;//UNUSED
    if (mpu.mode==M_UART && val!=0xff) return;
    if (mpu.state.reset) {
        /* THIS CODE IN DISPUTE [https://github.com/joncampbell123/dosbox-x/issues/917#issuecomment-538717798] */
        if (mpu.state.cmd_pending || val!=0xff) {
            mpu.state.cmd_pending=val+1;
            return;
        }
        PIC_RemoveEvents(MPU401_ResetDone);
        mpu.state.reset=false;
    }
	if (val<=0x2f) {
		switch (val&3) { /* MIDI stop, start, continue */
			case 1: {MIDI_RawOutByte(0xfc);break;}
			case 2: {MIDI_RawOutByte(0xfa);break;}
			case 3: {MIDI_RawOutByte(0xfb);break;}
		}
		if (val&0x20) LOG(LOG_MISC,LOG_ERROR)("MPU-401:Unhandled Recording Command %x",(int)val);
		switch (val&0xc) {
			case  0x4:	/* Stop */
				PIC_RemoveEvents(MPU401_Event);
				mpu.state.playing=false;
				for (Bit8u i=0xb0;i<0xbf;i++) {	/* All notes off */
					MIDI_RawOutByte(i);
					MIDI_RawOutByte(0x7b);
					MIDI_RawOutByte(0);
				}
				break;
			case 0x8:	/* Play */
				LOG(LOG_MISC,LOG_NORMAL)("MPU-401:Intelligent mode playback started");
				mpu.state.playing=true;
				PIC_RemoveEvents(MPU401_Event);
				PIC_AddEvent(MPU401_Event,MPU401_TIMECONSTANT/(mpu.clock.tempo*mpu.clock.timebase));
				ClrQueue();
				break;
		}
	}
	else if (val>=0xa0 && val<=0xa7) {	/* Request play counter */
		if (mpu.state.cmask&(1<<(val&7))) QueueByte((Bit8u)mpu.playbuf[val&7].counter);
	}
	else if (val>=0xd0 && val<=0xd7) {	/* Send data */
		mpu.state.old_chan=mpu.state.channel;
		mpu.state.channel=val&7;
		mpu.state.wsd=true;
		mpu.state.wsm=false;
		mpu.state.wsd_start=true;
	}
	else
	switch (val) {
		case 0xdf:	/* Send system message */
			mpu.state.wsd=false;
			mpu.state.wsm=true;
			mpu.state.wsd_start=true;
			break;
		case 0x8e:	/* Conductor */
			mpu.state.cond_set=false;
			break;
		case 0x8f:
			mpu.state.cond_set=true;
			break;
		case 0x94: /* Clock to host */
			mpu.clock.clock_to_host=false;
			break;
		case 0x95:
			mpu.clock.clock_to_host=true;
			break;
		case 0xc2: /* Internal timebase */
			mpu.clock.timebase=48;
			break;
		case 0xc3:
			mpu.clock.timebase=72;
			break;
		case 0xc4:
			mpu.clock.timebase=96;
			break;
		case 0xc5:
			mpu.clock.timebase=120;
			break;
		case 0xc6:
			mpu.clock.timebase=144;
			break;
		case 0xc7:
			mpu.clock.timebase=168;
			break;
		case 0xc8:
			mpu.clock.timebase=192;
			break;
		/* Commands with data byte */
		case 0xe0: case 0xe1: case 0xe2: case 0xe4: case 0xe6: 
		case 0xe7: case 0xec: case 0xed: case 0xee: case 0xef:
			mpu.state.command_byte=val;
			break;
		/* Commands 0xa# returning data */
		case 0xab:	/* Request and clear recording counter */
			QueueByte(MSG_MPU_ACK);
			QueueByte(0);
			return;
		case 0xac:	/* Request version */
			QueueByte(MSG_MPU_ACK);
			QueueByte(MPU401_VERSION);
			return;
		case 0xad:	/* Request revision */
			QueueByte(MSG_MPU_ACK);
			QueueByte(MPU401_REVISION);
			return;
		case 0xaf:	/* Request tempo */
			QueueByte(MSG_MPU_ACK);
			QueueByte(mpu.clock.tempo);
			return;
		case 0xb1:	/* Reset relative tempo */
            mpu.clock.old_tempo_rel=mpu.clock.tempo_rel;
            mpu.clock.tempo_rel=0x40;
			break;
		case 0xb9:	/* Clear play map */
		case 0xb8:	/* Clear play counters */
			for (Bit8u i=0xb0;i<0xbf;i++) {	/* All notes off */
				MIDI_RawOutByte(i);
				MIDI_RawOutByte(0x7b);
				MIDI_RawOutByte(0);
			}
			for (Bitu i=0;i<8;i++) {
				mpu.playbuf[i].counter=0;
				mpu.playbuf[i].type=T_OVERFLOW;
			}
			mpu.condbuf.counter=0;
			mpu.condbuf.type=T_OVERFLOW;
			if (!(mpu.state.conductor=mpu.state.cond_set)) mpu.state.cond_req=0;
			mpu.state.amask=mpu.state.tmask;
			mpu.state.req_mask=0;
			mpu.state.irq_pending=true;
			break;
		case 0xff:	/* Reset MPU-401 */
			LOG(LOG_MISC,LOG_NORMAL)("MPU-401:Reset %X",(int)val);
			PIC_AddEvent(MPU401_ResetDone,MPU401_RESETBUSY);
			mpu.state.reset=true;
			if (mpu.mode==M_UART) {
				MPU401_Reset();
				return;	//do not send ack in UART mode
			}
			MPU401_Reset();
			break;
		case 0x3f:	/* UART mode */
			LOG(LOG_MISC,LOG_NORMAL)("MPU-401:Set UART mode %X",(int)val);
			mpu.mode=M_UART;
			break;
		default:;
			//LOG(LOG_MISC,LOG_NORMAL)("MPU-401:Unhandled command %X",val);
	}
	QueueByte(MSG_MPU_ACK);
}

static Bitu MPU401_ReadData(Bitu port,Bitu iolen) {
    (void)iolen;//UNUSED
    (void)port;//UNUSED
	Bit8u ret=MSG_MPU_ACK;
	if (mpu.queue_used) {
		if (mpu.queue_pos>=MPU401_QUEUE) mpu.queue_pos-=MPU401_QUEUE;
		ret=mpu.queue[mpu.queue_pos];
		mpu.queue_pos++;mpu.queue_used--;
	}
	if (!mpu.intelligent) return ret;

	if (mpu.queue_used == 0) PIC_DeActivateIRQ(mpu.irq);

	if (ret>=0xf0 && ret<=0xf7) { /* MIDI data request */
		mpu.state.channel=ret&7;
		mpu.state.data_onoff=0;
		mpu.state.cond_req=false;
	}
	if (ret==MSG_MPU_COMMAND_REQ) {
		mpu.state.data_onoff=0;
		mpu.state.cond_req=true;
		if (mpu.condbuf.type!=T_OVERFLOW) {
			mpu.state.block_ack=true;
			MPU401_WriteCommand(0x331,mpu.condbuf.value[0],1);
			if (mpu.state.command_byte) MPU401_WriteData(0x330,mpu.condbuf.value[1],1);
		}
	mpu.condbuf.type=T_OVERFLOW;
	}
	if (ret==MSG_MPU_END || ret==MSG_MPU_CLOCK || ret==MSG_MPU_ACK) {
		mpu.state.data_onoff=-1;
		MPU401_EOIHandlerDispatch();
	}
	return ret;
}

static void MPU401_WriteData(Bitu port,Bitu val,Bitu iolen) {
    (void)iolen;//UNUSED
    (void)port;//UNUSED
	if (mpu.mode==M_UART) {MIDI_RawOutByte((Bit8u)val);return;}
	switch (mpu.state.command_byte) {	/* 0xe# command data */
		case 0x00:
			break;
		case 0xe0:	/* Set tempo */
			mpu.state.command_byte=0;
			mpu.clock.tempo=(Bit8u)val;
			return;
		case 0xe1:	/* Set relative tempo */
			mpu.state.command_byte=0;
            mpu.clock.old_tempo_rel=mpu.clock.tempo_rel;
            mpu.clock.tempo_rel=(Bit8u)val;
            if (val != 0x40) LOG(LOG_MISC,LOG_ERROR)("MPU-401:Relative tempo change value 0x%x (%.3f)",(unsigned int)val,(double)val / 0x40);
			return;
		case 0xe7:	/* Set internal clock to host interval */
			mpu.state.command_byte=0;
			mpu.clock.cth_rate=(Bit8u)(val>>2);
			return;
		case 0xec:	/* Set active track mask */
			mpu.state.command_byte=0;
			mpu.state.tmask=(Bit8u)val;
			return;
		case 0xed: /* Set play counter mask */
			mpu.state.command_byte=0;
			mpu.state.cmask=(Bit8u)val;
			return;
		case 0xee: /* Set 1-8 MIDI channel mask */
			mpu.state.command_byte=0;
			mpu.state.midi_mask&=0xff00;
			mpu.state.midi_mask|=val;
			return;
		case 0xef: /* Set 9-16 MIDI channel mask */
			mpu.state.command_byte=0;
			mpu.state.midi_mask&=0x00ff;
			mpu.state.midi_mask|=((Bit16u)val)<<8;
			return;
		//case 0xe2:	/* Set graduation for relative tempo */
		//case 0xe4:	/* Set metronome */
		//case 0xe6:	/* Set metronome measure length */
		default:
			mpu.state.command_byte=0;
			return;
	}
	static Bitu length,cnt,posd;
	if (mpu.state.wsd) {	/* Directly send MIDI message */
		if (mpu.state.wsd_start) {
			mpu.state.wsd_start=0;
			cnt=0;
				switch (val&0xf0) {
					case 0xc0:case 0xd0:
						mpu.playbuf[mpu.state.channel].value[0]=(Bit8u)val;
						length=2;
						break;
					case 0x80:case 0x90:case 0xa0:case 0xb0:case 0xe0:
						mpu.playbuf[mpu.state.channel].value[0]=(Bit8u)val;
						length=3;
						break;
					case 0xf0:
						LOG(LOG_MISC,LOG_ERROR)("MPU-401:Illegal WSD byte");
						mpu.state.wsd=0;
						mpu.state.channel=mpu.state.old_chan;
						return;
					default: /* MIDI with running status */
						cnt++;
						MIDI_RawOutByte(mpu.playbuf[mpu.state.channel].value[0]);
				}
		}
		if (cnt<length) {MIDI_RawOutByte((Bit8u)val);cnt++;}
		if (cnt==length) {
			mpu.state.wsd=0;
			mpu.state.channel=mpu.state.old_chan;
		}
		return;
	}
	if (mpu.state.wsm) {	/* Directly send system message */
		if (val==MSG_EOX) {MIDI_RawOutByte(MSG_EOX);mpu.state.wsm=0;return;}
		if (mpu.state.wsd_start) {
			mpu.state.wsd_start=0;
			cnt=0;
			switch (val) {
				case 0xf2:{ length=3; break;}
				case 0xf3:{ length=2; break;}
				case 0xf6:{ length=1; break;}
				case 0xf0:{ length=0; break;}
				default:
					length=0;
			}
		}
		if (!length || cnt<length) {MIDI_RawOutByte((Bit8u)val);cnt++;}
		if (cnt==length) mpu.state.wsm=0;
		return;
	}
	if (mpu.state.cond_req) { /* Command */
		switch (mpu.state.data_onoff) {
			case -1:
				return;
			case  0: /* Timing byte */
				mpu.condbuf.vlength=0;
				if (val<0xf0) mpu.state.data_onoff++;
				else {
					mpu.state.data_onoff=-1;
					MPU401_EOIHandlerDispatch();
					return;
				}
				if (val==0) mpu.state.send_now=true;
				else mpu.state.send_now=false;
				mpu.condbuf.counter=(Bits)val;
				break;
			case  1: /* Command byte #1 */
				mpu.condbuf.type=T_COMMAND;
				if (val==0xf8 || val==0xf9) mpu.condbuf.type=T_OVERFLOW;
				mpu.condbuf.value[mpu.condbuf.vlength]=(Bit8u)val;
				mpu.condbuf.vlength++;
				if ((val&0xf0)!=0xe0) MPU401_EOIHandlerDispatch();
				else mpu.state.data_onoff++;
				break;
			case  2:/* Command byte #2 */
				mpu.condbuf.value[mpu.condbuf.vlength]=(Bit8u)val;
				mpu.condbuf.vlength++;
				MPU401_EOIHandlerDispatch();
				break;
		}
		return;
	}
	switch (mpu.state.data_onoff) { /* Data */
		case   -1:
			return;
		case    0: /* Timing byte */
			if (val<0xf0) mpu.state.data_onoff=1;
			else {
				mpu.state.data_onoff=-1;
				MPU401_EOIHandlerDispatch();
				return;
			}
			if (val==0) mpu.state.send_now=true;
			else mpu.state.send_now=false;
			mpu.playbuf[mpu.state.channel].counter=(Bits)val;
			break;
		case    1: /* MIDI */
			mpu.playbuf[mpu.state.channel].vlength++;
			posd=mpu.playbuf[mpu.state.channel].vlength;
			if (posd==1) {
				switch (val&0xf0) {
					case 0xf0: /* System message or mark */
						if (val>0xf7) {
							mpu.playbuf[mpu.state.channel].type=T_MARK;
							mpu.playbuf[mpu.state.channel].sys_val=(Bit8u)val;
							length=1;
						} else {
							LOG(LOG_MISC,LOG_ERROR)("MPU-401:Illegal message");
							mpu.playbuf[mpu.state.channel].type=T_MIDI_SYS;
							mpu.playbuf[mpu.state.channel].sys_val=(Bit8u)val;
							length=1;
						}
						break;
					case 0xc0: case 0xd0: /* MIDI Message */
						mpu.playbuf[mpu.state.channel].type=T_MIDI_NORM;
						length=mpu.playbuf[mpu.state.channel].length=2;
						break;
					case 0x80: case 0x90: case 0xa0:  case 0xb0: case 0xe0: 
						mpu.playbuf[mpu.state.channel].type=T_MIDI_NORM;
						length=mpu.playbuf[mpu.state.channel].length=3;
						break;
					default: /* MIDI data with running status */
						posd++;
						mpu.playbuf[mpu.state.channel].vlength++;
						mpu.playbuf[mpu.state.channel].type=T_MIDI_NORM;
						length=mpu.playbuf[mpu.state.channel].length;
						break;
				}
			}
			if (!(posd==1 && val>=0xf0)) mpu.playbuf[mpu.state.channel].value[posd-1]=(Bit8u)val;
			if (posd==length) MPU401_EOIHandlerDispatch();
	}
}

static void MPU401_IntelligentOut(Bit8u chan) {
	Bitu val;
	switch (mpu.playbuf[chan].type) {
		case T_OVERFLOW:
			break;
		case T_MARK:
			val=mpu.playbuf[chan].sys_val;
			if (val==0xfc) {
				MIDI_RawOutByte((Bit8u)val);
				mpu.state.amask&=~(1<<chan);
				mpu.state.req_mask&=~(1<<chan);
			}
			break;
		case T_MIDI_NORM:
			for (Bitu i=0;i<mpu.playbuf[chan].vlength;i++)
				MIDI_RawOutByte(mpu.playbuf[chan].value[i]);
			break;
		default:
			break;
	}
}

static void UpdateTrack(Bit8u chan) {
	MPU401_IntelligentOut(chan);
	if (mpu.state.amask&(1<<chan)) {
		mpu.playbuf[chan].vlength=0;
		mpu.playbuf[chan].type=T_OVERFLOW;
		mpu.playbuf[chan].counter=0xf0;
		mpu.state.req_mask|=(1<<chan);
	} else {
		if (mpu.state.amask==0 && !mpu.state.conductor) mpu.state.req_mask|=(1<<12);
	}
}

static void UpdateConductor(void) {
    for (unsigned int i=0;i < mpu.condbuf.vlength;i++) {
        if (mpu.condbuf.value[i] == 0xfc) {
            mpu.condbuf.value[i] = 0;
            mpu.state.conductor=false;
            mpu.state.req_mask&=~(1<<9);
            if (mpu.state.amask==0) mpu.state.req_mask|=(1<<12);
            return;
        }
    }

	mpu.condbuf.vlength=0;
	mpu.condbuf.counter=0xf0;
	mpu.state.req_mask|=(1<<9);
}

static void MPU401_Event(Bitu val) {
    (void)val;//UNUSED
	if (mpu.mode==M_UART) return;
	if (mpu.state.irq_pending) goto next_event;
	for (Bit8u i=0;i<8;i++) { /* Decrease counters */
		if (mpu.state.amask&(1<<i)) {
			mpu.playbuf[i].counter--;
			if (mpu.playbuf[i].counter<=0) UpdateTrack(i);
		}
	}		
	if (mpu.state.conductor) {
		mpu.condbuf.counter--;
		if (mpu.condbuf.counter<=0) UpdateConductor();
	}
	if (mpu.clock.clock_to_host) {
		mpu.clock.cth_counter++;
		if (mpu.clock.cth_counter >= mpu.clock.cth_rate) {
			mpu.clock.cth_counter=0;
			mpu.state.req_mask|=(1<<13);
		}
	}
	if (!mpu.state.irq_pending && mpu.state.req_mask) MPU401_EOIHandler();
next_event:
	Bitu new_time;
	if ((new_time=(Bitu)((mpu.clock.tempo*mpu.clock.timebase*mpu.clock.tempo_rel)/0x40))==0) return;
	PIC_AddEvent(MPU401_Event,MPU401_TIMECONSTANT/new_time);
}


static void MPU401_EOIHandlerDispatch(void) {
	if (mpu.state.send_now) {
		mpu.state.eoi_scheduled=true;
		PIC_AddEvent(MPU401_EOIHandler,0.06f); //Possible a bit longer
	}
	else if (!mpu.state.eoi_scheduled) MPU401_EOIHandler();
}

//Updates counters and requests new data on "End of Input"
static void MPU401_EOIHandler(Bitu val) {
    (void)val;//UNUSED
	mpu.state.eoi_scheduled=false;
	if (mpu.state.send_now) {
		mpu.state.send_now=false;
		if (mpu.state.cond_req) UpdateConductor();
		else UpdateTrack(mpu.state.channel);
	}
	mpu.state.irq_pending=false;
	if (!mpu.state.playing || !mpu.state.req_mask) return;
	Bit8u i=0;
	do {
		if (mpu.state.req_mask&(1<<i)) {
			QueueByte(0xf0+i);
			mpu.state.req_mask&=~(1<<i);
			break;
		}
	} while ((i++)<16);
}

static void MPU401_ResetDone(Bitu) {
	mpu.state.reset=false;
	if (mpu.state.cmd_pending) {
		MPU401_WriteCommand(0x331,mpu.state.cmd_pending-1,1);
		mpu.state.cmd_pending=0;
	}
}
static void MPU401_Reset(void) {
	PIC_DeActivateIRQ(mpu.irq);
	mpu.mode=(mpu.intelligent ? M_INTELLIGENT : M_UART);
	PIC_RemoveEvents(MPU401_EOIHandler);
	mpu.state.eoi_scheduled=false;
	mpu.state.wsd=false;
	mpu.state.wsm=false;
	mpu.state.conductor=false;
	mpu.state.cond_req=false;
	mpu.state.cond_set=false;
	mpu.state.playing=false;
	mpu.state.run_irq=false;
	mpu.state.irq_pending=false;
	mpu.state.cmask=0xff;
	mpu.state.amask=mpu.state.tmask=0;
	mpu.state.midi_mask=0xffff;
	mpu.state.data_onoff=-1;
	mpu.state.command_byte=0;
	mpu.state.block_ack=false;
	mpu.clock.tempo=mpu.clock.old_tempo=100;
	mpu.clock.timebase=mpu.clock.old_timebase=120;
	mpu.clock.tempo_rel=mpu.clock.old_tempo_rel=0x40;
	mpu.clock.tempo_grad=0;
	mpu.clock.clock_to_host=false;
	mpu.clock.cth_rate=60;
	mpu.clock.cth_counter=0;
	ClrQueue();
	mpu.state.req_mask=0;
	mpu.condbuf.counter=0;
	mpu.condbuf.type=T_OVERFLOW;
	for (Bitu i=0;i<8;i++) {mpu.playbuf[i].type=T_OVERFLOW;mpu.playbuf[i].counter=0;}
}

static void IMF_Write(Bitu port,Bitu val,Bitu iolen) {
    (void)iolen;//UNUSED
	LOG(LOG_MISC,LOG_NORMAL)("IMF:Wr %4X,%X",(int)port,(int)val);
}

class MPU401:public Module_base{
private:
	IO_ReadHandleObject ReadHandler[2];
	IO_WriteHandleObject WriteHandler[2];
	bool installed; /*as it can fail to install by 2 ways (config and no midi)*/
public:
	MPU401(Section* configuration):Module_base(configuration){
		installed = false;
		Section_prop * section=static_cast<Section_prop *>(configuration);
		const char* s_mpu = section->Get_string("mpu401");
		if(strcasecmp(s_mpu,"none") == 0) return;
		if(strcasecmp(s_mpu,"off") == 0) return;
		if(strcasecmp(s_mpu,"false") == 0) return;
		if (!MIDI_Available()) return;
		/*Enabled and there is a Midi */
		installed = true;

        Bitu baseio = (Bitu)section->Get_hex("mpubase");
        if (baseio == 0) {
            if (IS_PC98_ARCH)
                baseio = 0xE0D0; // NTS: This is based on MMD.COM I/O probing behavior
            else
                baseio = 0x330;
        }

        if (baseio >= 0x80D2 && baseio <= 0x80DE) {
            /* TODO: Replace low 8 bits with low 8 bits of Sound Blaster 16 I/O base */
        }

        if (IS_PC98_ARCH) {
            if (baseio >= 0x80D2 && baseio <= 0x80DE)
                { /* OK */ }
            else if (baseio >= 0xC000u && baseio <= 0xF8D0u)
                { /* OK */ }
            else
                baseio = 0xE0D0u;
        }
        else {
            if (baseio < 0x300 || baseio > 0x360)
                baseio = 0x330;
        }

        if (baseio >= 0x80D2 && baseio <= 0x80DE) {
            LOG_MSG("MPU-401 Registering I/O ports as if SB16 MPU-401 at base %xh",(unsigned int)baseio);
            /* SB16 mapping
             * C8D2h, C9D2h (PC-98) */
            assert(IS_PC98_ARCH);
            WriteHandler[0].Install(baseio       ,&MPU401_WriteData,IO_MB);
            WriteHandler[1].Install(baseio+0x100u,&MPU401_WriteCommand,IO_MB);
            ReadHandler[0].Install(baseio       ,&MPU401_ReadData,IO_MB);
            ReadHandler[1].Install(baseio+0x100u,&MPU401_ReadStatus,IO_MB);
        }
        else {
            if (IS_PC98_ARCH)
                LOG_MSG("MPU-401 Registering I/O ports as if PC-98 MPU-401 at base %xh",(unsigned int)baseio);
            else
                LOG_MSG("MPU-401 Registering I/O ports as if IBM PC MPU-401 at base %xh",(unsigned int)baseio);
            /* 330h, 331h (IBM PC)
             * E0D0h, E0D2h (PC-98) */
            WriteHandler[0].Install(baseio                     ,&MPU401_WriteData,IO_MB);
            WriteHandler[1].Install(baseio+(IS_PC98_ARCH?2u:1u),&MPU401_WriteCommand,IO_MB);
            ReadHandler[0].Install(baseio                     ,&MPU401_ReadData,IO_MB);
            ReadHandler[1].Install(baseio+(IS_PC98_ARCH?2u:1u),&MPU401_ReadStatus,IO_MB);
        }

        if (IS_PC98_ARCH) {
        }
        else {
            /*
               IO_RegisterWriteHandler(0x280,&IMF_Write,IO_MB);
               IO_RegisterWriteHandler(0x281,&IMF_Write,IO_MB);
               IO_RegisterReadHandler(0x280,&IMF_Read,IO_MB);
               IO_RegisterReadHandler(0x281,&IMF_Read,IO_MB);
               */
            /*
               IO_RegisterWriteHandler(0x200,&IMF_Write,IO_MB);
               IO_RegisterWriteHandler(0x201,&IMF_Write,IO_MB);
               IO_RegisterWriteHandler(0x202,&IMF_Write,IO_MB);
               IO_RegisterWriteHandler(0x203,&IMF_Write,IO_MB);
               IO_RegisterWriteHandler(0x204,&IMF_Write,IO_MB);

               IO_RegisterReadHandler(0x200,&IMF_Read,IO_MB);
               IO_RegisterReadHandler(0x201,&IMF_Read,IO_MB);
               IO_RegisterReadHandler(0x202,&IMF_Read,IO_MB);
               IO_RegisterReadHandler(0x203,&IMF_Read,IO_MB);
               IO_RegisterReadHandler(0x204,&IMF_Read,IO_MB);
               */
            IO_RegisterWriteHandler(0x2A20,&IMF_Write,IO_MB);
            IO_RegisterWriteHandler(0x2A21,&IMF_Write,IO_MB);
            IO_RegisterWriteHandler(0x2A22,&IMF_Write,IO_MB);
            IO_RegisterWriteHandler(0x2A23,&IMF_Write,IO_MB);
            IO_RegisterWriteHandler(0x2A24,&IMF_Write,IO_MB);
            IO_RegisterWriteHandler(0x2A25,&IMF_Write,IO_MB);
            IO_RegisterWriteHandler(0x2A26,&IMF_Write,IO_MB);
            IO_RegisterWriteHandler(0x2A27,&IMF_Write,IO_MB);
            IO_RegisterWriteHandler(0x2A28,&IMF_Write,IO_MB);
            IO_RegisterWriteHandler(0x2A29,&IMF_Write,IO_MB);
            IO_RegisterWriteHandler(0x2A2A,&IMF_Write,IO_MB);
            IO_RegisterWriteHandler(0x2A2B,&IMF_Write,IO_MB);
            IO_RegisterWriteHandler(0x2A2C,&IMF_Write,IO_MB);
            IO_RegisterWriteHandler(0x2A2D,&IMF_Write,IO_MB);
            IO_RegisterWriteHandler(0x2A2E,&IMF_Write,IO_MB);
            IO_RegisterWriteHandler(0x2A2F,&IMF_Write,IO_MB);
        }

		mpu.queue_used=0;
		mpu.queue_pos=0;
		mpu.mode=M_UART;

        if (IS_PC98_ARCH)
            mpu.irq=6;  /* This is said to be the MIDI card's factory default on PC-98 */
        else if (!enable_slave_pic)
            mpu.irq=2;
        else
    		mpu.irq=9;	/* Princess Maker 2 wants it on irq 9 */

        {
            int x = section->Get_int("mpuirq");

            if (x >= 2)
                mpu.irq = (unsigned int)x;

            if (!IS_PC98_ARCH && enable_slave_pic && mpu.irq == 2)
                mpu.irq = 9;
            else if (!enable_slave_pic && mpu.irq == 9)
                mpu.irq = 2;
        }

        LOG(LOG_MISC,LOG_NORMAL)("MPU IRQ %d",(int)mpu.irq);

		mpu.intelligent = true;	//Default is on
		if(strcasecmp(s_mpu,"uart") == 0) mpu.intelligent = false;
		if (!mpu.intelligent) return;

        if (IS_PC98_ARCH) {
            /* Mask IRQ by default.
             * 
             * MMD.COM, a common MIDI MPU-401 driver in the PC-98 world, seems to have
             * IRQ detection problems unless the IRQ is masked during the detection phase.
             * It will fail to detect any IRQ that is unmasked during it's detection phase.
             * For some reason it's not totally consistent i.e. the MMD.COM driver included
             * with Touhou Project 2 can detect MIDI on IRQ 5 when unmasked, but not 3, 12, or 6,
             * or sometimes only on IRQ 6 if I/O delays are lengthened.
             *
             * Perhaps the driver assumes that if the IRQ is unmasked, that it's probably in
             * use and not safe to probe.
             *
             * Also noted: MMD.COM cannot detect IRQs reliably if I/O delay is disabled in
             *             the emulation (iodelay=0) */
            PIC_SetIRQMask(mpu.irq,true);
        }
        else {
            /*Set IRQ and unmask it(for timequest/princess maker 2) */
            PIC_SetIRQMask(mpu.irq,false);
        }

		MPU401_Reset();
	}
};

static MPU401* test = NULL;

void MPU401_Destroy(Section* sec){
    (void)sec;//UNUSED
	if (test != NULL) {
		delete test;
		test = NULL;
	}
}

void MPU401_Reset(Section* sec) {
    (void)sec;//UNUSED
	if (test == NULL) {
		LOG(LOG_MISC,LOG_DEBUG)("Allocating MPU401 emulation");
		test = new MPU401(control->GetSection("midi"));
	}
}

void MIDI_GUI_OnSectionPropChange(Section *x);

void MIDI_OnSectionPropChange(Section *x) {
    delete test;
    test = NULL;

    LOG(LOG_MISC,LOG_DEBUG)("Resetting MPU401, config change");

    MIDI_GUI_OnSectionPropChange(x);

    test = new MPU401(control->GetSection("midi"));
}

void MPU401_Init() {
	LOG(LOG_MISC,LOG_DEBUG)("Initializing MPU401 emulation");

	AddVMEventFunction(VM_EVENT_RESET,AddVMEventFunctionFuncPair(MPU401_Reset));
	AddExitFunction(AddExitFunctionFuncPair(MPU401_Destroy),true);

	control->GetSection("midi")->onpropchange.push_back(&MIDI_OnSectionPropChange);
}

// save state support
void *MPU401_Event_PIC_Event = (void*)((uintptr_t)MPU401_Event);

void POD_Save_MPU401( std::ostream& stream )
{
	const char pod_name[32] = "MPU401";

	if( stream.fail() ) return;
	if( !test ) return;


	WRITE_POD( &pod_name, pod_name );

	//*******************************************
	//*******************************************
	//*******************************************

	// - pure data
	WRITE_POD( &mpu, mpu );
}


void POD_Load_MPU401( std::istream& stream )
{
	char pod_name[32] = {0};

	if( stream.fail() ) return;
	if( !test ) return;


	// error checking
	READ_POD( &pod_name, pod_name );
	if( strcmp( pod_name, "MPU401" ) ) {
		stream.clear( std::istream::failbit | std::istream::badbit );
		return;
	}

	//************************************************
	//************************************************
	//************************************************

	// - pure data
	READ_POD( &mpu, mpu );
}
