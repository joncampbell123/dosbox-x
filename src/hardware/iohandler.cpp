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

/* $Id: iohandler.cpp,v 1.30 2009-05-27 09:15:41 qbix79 Exp $ */

#include <string.h>
#include "dosbox.h"
#include "inout.h"
#include "setup.h"
#include "cpu.h"
#include "../src/cpu/lazyflags.h"
#include "callback.h"

//#define ENABLE_PORTLOG

IO_WriteHandler * io_writehandlers[3][IO_MAX];
IO_ReadHandler * io_readhandlers[3][IO_MAX];

static Bitu IO_ReadBlocked(Bitu /*port*/,Bitu /*iolen*/) {
	return ~0;
}

static void IO_WriteBlocked(Bitu /*port*/,Bitu /*val*/,Bitu /*iolen*/) {
}

static Bitu IO_ReadDefault(Bitu port,Bitu iolen) {
	switch (iolen) {
	case 1:
		LOG(LOG_IO,LOG_WARN)("Read from port %04X",port);
		io_readhandlers[0][port]=IO_ReadBlocked;
		return 0xff;
	case 2:
		return 
			(io_readhandlers[0][port+0](port+0,1) << 0) |
			(io_readhandlers[0][port+1](port+1,1) << 8);
	case 4:
		return
			(io_readhandlers[1][port+0](port+0,2) << 0) |
			(io_readhandlers[1][port+2](port+2,2) << 16);
	}
	return 0;
}

void IO_WriteDefault(Bitu port,Bitu val,Bitu iolen) {
	switch (iolen) {
	case 1:
		LOG(LOG_IO,LOG_WARN)("Writing %02X to port %04X",val,port);		
		io_writehandlers[0][port]=IO_WriteBlocked;
		break;
	case 2:
		io_writehandlers[0][port+0](port+0,(val >> 0) & 0xff,1);
		io_writehandlers[0][port+1](port+1,(val >> 8) & 0xff,1);
		break;
	case 4:
		io_writehandlers[1][port+0](port+0,(val >> 0 ) & 0xffff,2);
		io_writehandlers[1][port+2](port+2,(val >> 16) & 0xffff,2);
		break;
	}
}

void IO_RegisterReadHandler(Bitu port,IO_ReadHandler * handler,Bitu mask,Bitu range) {
	while (range--) {
		if (mask&IO_MB) io_readhandlers[0][port]=handler;
		if (mask&IO_MW) io_readhandlers[1][port]=handler;
		if (mask&IO_MD) io_readhandlers[2][port]=handler;
		port++;
	}
}

void IO_RegisterWriteHandler(Bitu port,IO_WriteHandler * handler,Bitu mask,Bitu range) {
	while (range--) {
		if (mask&IO_MB) io_writehandlers[0][port]=handler;
		if (mask&IO_MW) io_writehandlers[1][port]=handler;
		if (mask&IO_MD) io_writehandlers[2][port]=handler;
		port++;
	}
}

void IO_FreeReadHandler(Bitu port,Bitu mask,Bitu range) {
	while (range--) {
		if (mask&IO_MB) io_readhandlers[0][port]=IO_ReadDefault;
		if (mask&IO_MW) io_readhandlers[1][port]=IO_ReadDefault;
		if (mask&IO_MD) io_readhandlers[2][port]=IO_ReadDefault;
		port++;
	}
}

void IO_FreeWriteHandler(Bitu port,Bitu mask,Bitu range) {
	while (range--) {
		if (mask&IO_MB) io_writehandlers[0][port]=IO_WriteDefault;
		if (mask&IO_MW) io_writehandlers[1][port]=IO_WriteDefault;
		if (mask&IO_MD) io_writehandlers[2][port]=IO_WriteDefault;
		port++;
	}
}

void IO_ReadHandleObject::Install(Bitu port,IO_ReadHandler * handler,Bitu mask,Bitu range) {
	if(!installed) {
		installed=true;
		m_port=port;
		m_mask=mask;
		m_range=range;
		IO_RegisterReadHandler(port,handler,mask,range);
	} else E_Exit("IO_readHandler allready installed port %x",port);
}

IO_ReadHandleObject::~IO_ReadHandleObject(){
	if(!installed) return;
	IO_FreeReadHandler(m_port,m_mask,m_range);
}

void IO_WriteHandleObject::Install(Bitu port,IO_WriteHandler * handler,Bitu mask,Bitu range) {
	if(!installed) {
		installed=true;
		m_port=port;
		m_mask=mask;
		m_range=range;
		IO_RegisterWriteHandler(port,handler,mask,range);
	} else E_Exit("IO_writeHandler allready installed port %x",port);
}

IO_WriteHandleObject::~IO_WriteHandleObject(){
	if(!installed) return;
	IO_FreeWriteHandler(m_port,m_mask,m_range);
	//LOG_MSG("FreeWritehandler called with port %X",m_port);
}

struct IOF_Entry {
	Bitu cs;
	Bitu eip;
};

#define IOF_QUEUESIZE 16
static struct {
	Bitu used;
	IOF_Entry entries[IOF_QUEUESIZE];
} iof_queue;

static Bits IOFaultCore(void) {
	CPU_CycleLeft+=CPU_Cycles;
	CPU_Cycles=1;
	Bits ret=CPU_Core_Full_Run();
	CPU_CycleLeft+=CPU_Cycles;
	if (ret<0) E_Exit("Got a dosbox close machine in IO-fault core?");
	if (ret) 
		return ret;
	if (!iof_queue.used) E_Exit("IO-faul Core without IO-faul");
	IOF_Entry * entry=&iof_queue.entries[iof_queue.used-1];
	if (entry->cs == SegValue(cs) && entry->eip==reg_eip)
		return -1;
	return 0;
}


/* Some code to make io operations take some virtual time. Helps certain
 * games with their timing of certain operations
 */


#define IODELAY_READ_MICROS 1.0
#define IODELAY_WRITE_MICROS 0.75

inline void IO_USEC_read_delay_old() {
	if(CPU_CycleMax > static_cast<Bit32s>((IODELAY_READ_MICROS*1000.0))) {
		// this could be calculated whenever CPU_CycleMax changes
		Bits delaycyc = static_cast<Bits>((CPU_CycleMax/1000)*IODELAY_READ_MICROS);
		if(CPU_Cycles > delaycyc) CPU_Cycles -= delaycyc;
		else CPU_Cycles = 0;
	}
}

inline void IO_USEC_write_delay_old() {
	if(CPU_CycleMax > static_cast<Bit32s>((IODELAY_WRITE_MICROS*1000.0))) {
		// this could be calculated whenever CPU_CycleMax changes
		Bits delaycyc = static_cast<Bits>((CPU_CycleMax/1000)*IODELAY_WRITE_MICROS); 
		if(CPU_Cycles > delaycyc) CPU_Cycles -= delaycyc;
		else CPU_Cycles = 0;
	}
}


#define IODELAY_READ_MICROSk (Bit32u)(1024/1.0)
#define IODELAY_WRITE_MICROSk (Bit32u)(1024/0.75)

inline void IO_USEC_read_delay() {
	Bits delaycyc = CPU_CycleMax/IODELAY_READ_MICROSk;
	if(GCC_UNLIKELY(CPU_Cycles < 3*delaycyc)) delaycyc = 0; //Else port acces will set cycles to 0. which might trigger problem with games which read 16 bit values
	CPU_Cycles -= delaycyc;
	CPU_IODelayRemoved += delaycyc;
}

inline void IO_USEC_write_delay() {
	Bits delaycyc = CPU_CycleMax/IODELAY_WRITE_MICROSk; 
	if(GCC_UNLIKELY(CPU_Cycles < 3*delaycyc)) delaycyc=0;
	CPU_Cycles -= delaycyc;
	CPU_IODelayRemoved += delaycyc;
}

#ifdef ENABLE_PORTLOG
static Bit8u crtc_index = 0;
const char* const len_type[] = {" 8","16","32"};
void log_io(Bitu width, bool write, Bitu port, Bitu val) {
	switch(width) {
	case 0:
		val&=0xff;
		break;
	case 1:
		val&=0xffff;
		break;
	}
	if (write) {
		// skip the video cursor position spam
		if (port==0x3d4) {
			if (width==0) crtc_index = (Bit8u)val;
			else if(width==1) crtc_index = (Bit8u)(val>>8);
		}
		if (crtc_index==0xe || crtc_index==0xf) {
			if((width==0 && (port==0x3d4 || port==0x3d5))||(width==1 && port==0x3d4))
				return;
		}

		switch(port) {
		//case 0x020: // interrupt command
		//case 0x040: // timer 0
		//case 0x042: // timer 2
		//case 0x043: // timer control
		//case 0x061: // speaker control
		case 0x3c8: // VGA palette
		case 0x3c9: // VGA palette
		// case 0x3d4: // VGA crtc
		// case 0x3d5: // VGA crtc
		// case 0x3c4: // VGA seq
		// case 0x3c5: // VGA seq
			break;
		default:
			LOG_MSG("iow%s % 4x % 4x, cs:ip %04x:%04x", len_type[width],
				port, val, SegValue(cs),reg_eip);
			break;
		}
	} else {
		switch(port) {
		//case 0x021: // interrupt status
		//case 0x040: // timer 0
		//case 0x042: // timer 2
		//case 0x061: // speaker control
		case 0x201: // joystick status
		case 0x3c9: // VGA palette
		// case 0x3d4: // VGA crtc index
		// case 0x3d5: // VGA crtc
		case 0x3da: // display status - a real spammer
			// don't log for the above cases
			break;
		default:
			LOG_MSG("ior%s % 4x % 4x,\t\tcs:ip %04x:%04x", len_type[width],
				port, val, SegValue(cs),reg_eip);
			break;
		}
	}
}
#else
#define log_io(W, X, Y, Z)
#endif


void IO_WriteB(Bitu port,Bitu val) {
	log_io(0, true, port, val);
	if (GCC_UNLIKELY(GETFLAG(VM) && (CPU_IO_Exception(port,1)))) {
		LazyFlags old_lflags;
		memcpy(&old_lflags,&lflags,sizeof(LazyFlags));
		CPU_Decoder * old_cpudecoder;
		old_cpudecoder=cpudecoder;
		cpudecoder=&IOFaultCore;
		IOF_Entry * entry=&iof_queue.entries[iof_queue.used++];
		entry->cs=SegValue(cs);
		entry->eip=reg_eip;
		CPU_Push16(SegValue(cs));
		CPU_Push16(reg_ip);
		Bit8u old_al = reg_al;
		Bit16u old_dx = reg_dx;
		reg_al = val;
		reg_dx = port;
		RealPt icb = CALLBACK_RealPointer(call_priv_io);
		SegSet16(cs,RealSeg(icb));
		reg_eip = RealOff(icb)+0x08;
		CPU_Exception(cpu.exception.which,cpu.exception.error);

		DOSBOX_RunMachine();
		iof_queue.used--;

		reg_al = old_al;
		reg_dx = old_dx;
		memcpy(&lflags,&old_lflags,sizeof(LazyFlags));
		cpudecoder=old_cpudecoder;
	}
	else {
		IO_USEC_write_delay();
		io_writehandlers[0][port](port,val,1);
	}
}

void IO_WriteW(Bitu port,Bitu val) {
	log_io(1, true, port, val);
	if (GCC_UNLIKELY(GETFLAG(VM) && (CPU_IO_Exception(port,2)))) {
		LazyFlags old_lflags;
		memcpy(&old_lflags,&lflags,sizeof(LazyFlags));
		CPU_Decoder * old_cpudecoder;
		old_cpudecoder=cpudecoder;
		cpudecoder=&IOFaultCore;
		IOF_Entry * entry=&iof_queue.entries[iof_queue.used++];
		entry->cs=SegValue(cs);
		entry->eip=reg_eip;
		CPU_Push16(SegValue(cs));
		CPU_Push16(reg_ip);
		Bit16u old_ax = reg_ax;
		Bit16u old_dx = reg_dx;
		reg_al = val;
		reg_dx = port;
		RealPt icb = CALLBACK_RealPointer(call_priv_io);
		SegSet16(cs,RealSeg(icb));
		reg_eip = RealOff(icb)+0x0a;
		CPU_Exception(cpu.exception.which,cpu.exception.error);

		DOSBOX_RunMachine();
		iof_queue.used--;

		reg_ax = old_ax;
		reg_dx = old_dx;
		memcpy(&lflags,&old_lflags,sizeof(LazyFlags));
		cpudecoder=old_cpudecoder;
	}
	else {
		IO_USEC_write_delay();
		io_writehandlers[1][port](port,val,2);
	}
}

void IO_WriteD(Bitu port,Bitu val) {
	log_io(2, true, port, val);
	if (GCC_UNLIKELY(GETFLAG(VM) && (CPU_IO_Exception(port,4)))) {
		LazyFlags old_lflags;
		memcpy(&old_lflags,&lflags,sizeof(LazyFlags));
		CPU_Decoder * old_cpudecoder;
		old_cpudecoder=cpudecoder;
		cpudecoder=&IOFaultCore;
		IOF_Entry * entry=&iof_queue.entries[iof_queue.used++];
		entry->cs=SegValue(cs);
		entry->eip=reg_eip;
		CPU_Push16(SegValue(cs));
		CPU_Push16(reg_ip);
		Bit32u old_eax = reg_eax;
		Bit16u old_dx = reg_dx;
		reg_al = val;
		reg_dx = port;
		RealPt icb = CALLBACK_RealPointer(call_priv_io);
		SegSet16(cs,RealSeg(icb));
		reg_eip = RealOff(icb)+0x0c;
		CPU_Exception(cpu.exception.which,cpu.exception.error);

		DOSBOX_RunMachine();
		iof_queue.used--;

		reg_eax = old_eax;
		reg_dx = old_dx;
		memcpy(&lflags,&old_lflags,sizeof(LazyFlags));
		cpudecoder=old_cpudecoder;
	}
	else io_writehandlers[2][port](port,val,4);
}

Bitu IO_ReadB(Bitu port) {
	Bitu retval;
	if (GCC_UNLIKELY(GETFLAG(VM) && (CPU_IO_Exception(port,1)))) {
		LazyFlags old_lflags;
		memcpy(&old_lflags,&lflags,sizeof(LazyFlags));
		CPU_Decoder * old_cpudecoder;
		old_cpudecoder=cpudecoder;
		cpudecoder=&IOFaultCore;
		IOF_Entry * entry=&iof_queue.entries[iof_queue.used++];
		entry->cs=SegValue(cs);
		entry->eip=reg_eip;
		CPU_Push16(SegValue(cs));
		CPU_Push16(reg_ip);
		Bit16u old_dx = reg_dx;
		reg_dx = port;
		RealPt icb = CALLBACK_RealPointer(call_priv_io);
		SegSet16(cs,RealSeg(icb));
		reg_eip = RealOff(icb)+0x00;
		CPU_Exception(cpu.exception.which,cpu.exception.error);

		DOSBOX_RunMachine();
		iof_queue.used--;

		retval = reg_al;
		reg_dx = old_dx;		
		memcpy(&lflags,&old_lflags,sizeof(LazyFlags));
		cpudecoder=old_cpudecoder;
		return retval;
	}
	else {
		IO_USEC_read_delay();
		retval = io_readhandlers[0][port](port,1);
	}
	log_io(0, false, port, retval);
	return retval;
}

Bitu IO_ReadW(Bitu port) {
	Bitu retval;
	if (GCC_UNLIKELY(GETFLAG(VM) && (CPU_IO_Exception(port,2)))) {
		LazyFlags old_lflags;
		memcpy(&old_lflags,&lflags,sizeof(LazyFlags));
		CPU_Decoder * old_cpudecoder;
		old_cpudecoder=cpudecoder;
		cpudecoder=&IOFaultCore;
		IOF_Entry * entry=&iof_queue.entries[iof_queue.used++];
		entry->cs=SegValue(cs);
		entry->eip=reg_eip;
		CPU_Push16(SegValue(cs));
		CPU_Push16(reg_ip);
		Bit16u old_dx = reg_dx;
		reg_dx = port;
		RealPt icb = CALLBACK_RealPointer(call_priv_io);
		SegSet16(cs,RealSeg(icb));
		reg_eip = RealOff(icb)+0x02;
		CPU_Exception(cpu.exception.which,cpu.exception.error);

		DOSBOX_RunMachine();
		iof_queue.used--;

		retval = reg_ax;
		reg_dx = old_dx;		
		memcpy(&lflags,&old_lflags,sizeof(LazyFlags));
		cpudecoder=old_cpudecoder;
	}
	else {
		IO_USEC_read_delay();
		retval = io_readhandlers[1][port](port,2);
	}
	log_io(1, false, port, retval);
	return retval;
}

Bitu IO_ReadD(Bitu port) {
	Bitu retval;
	if (GCC_UNLIKELY(GETFLAG(VM) && (CPU_IO_Exception(port,4)))) {
		LazyFlags old_lflags;
		memcpy(&old_lflags,&lflags,sizeof(LazyFlags));
		CPU_Decoder * old_cpudecoder;
		old_cpudecoder=cpudecoder;
		cpudecoder=&IOFaultCore;
		IOF_Entry * entry=&iof_queue.entries[iof_queue.used++];
		entry->cs=SegValue(cs);
		entry->eip=reg_eip;
		CPU_Push16(SegValue(cs));
		CPU_Push16(reg_ip);
		Bit16u old_dx = reg_dx;
		reg_dx = port;
		RealPt icb = CALLBACK_RealPointer(call_priv_io);
		SegSet16(cs,RealSeg(icb));
		reg_eip = RealOff(icb)+0x04;
		CPU_Exception(cpu.exception.which,cpu.exception.error);

		DOSBOX_RunMachine();
		iof_queue.used--;

		retval = reg_eax;
		reg_dx = old_dx;		
		memcpy(&lflags,&old_lflags,sizeof(LazyFlags));
		cpudecoder=old_cpudecoder;
	} else {
		retval = io_readhandlers[2][port](port,4);
	}
	log_io(2, false, port, retval);
	return retval;
}

class IO :public Module_base {
public:
	IO(Section* configuration):Module_base(configuration){
	iof_queue.used=0;
	IO_FreeReadHandler(0,IO_MA,IO_MAX);
	IO_FreeWriteHandler(0,IO_MA,IO_MAX);
	}
	~IO()
	{
		//Same as the constructor ?
	}
};

static IO* test;

void IO_Destroy(Section*) {
	delete test;
}

void IO_Init(Section * sect) {
	test = new IO(sect);
	sect->AddDestroyFunction(&IO_Destroy);
}
