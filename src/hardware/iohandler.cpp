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


#include <string.h>
#include "control.h"
#include "dosbox.h"
#include "inout.h"
#include "setup.h"
#include "cpu.h"
#include "../src/cpu/lazyflags.h"
#include "callback.h"

//#define ENABLE_PORTLOG

#include <vector>

extern bool pcibus_enable;

#define IO_callouts_max (IO_TYPE_MAX - IO_TYPE_MIN)
#define IO_callouts_index(t) (t - IO_TYPE_MIN)

class IO_callout_vector : public std::vector<IO_CalloutObject> {
public:
    IO_callout_vector() : std::vector<IO_CalloutObject>(), getcounter(0), alloc_from(0) { };
public:
    unsigned int getcounter;
    unsigned int alloc_from;
};

IO_WriteHandler * io_writehandlers[3][IO_MAX];
IO_ReadHandler * io_readhandlers[3][IO_MAX];

static IO_callout_vector IO_callouts[IO_callouts_max];

#if C_DEBUG
void DEBUG_EnableDebugger(void);
#endif

static Bitu IO_ReadBlocked(Bitu /*port*/,Bitu /*iolen*/) {
	return ~0ul;
}

static void IO_WriteBlocked(Bitu /*port*/,Bitu /*val*/,Bitu /*iolen*/) {
#if C_DEBUG && 0
    DEBUG_EnableDebugger();
#endif
}

static Bitu IO_ReadDefault(Bitu port,Bitu iolen) {
	switch (iolen) {
	case 1:
		LOG(LOG_IO,LOG_WARN)("Read from port %04X",(int)port);
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
	return ~0ul;
}

void IO_WriteDefault(Bitu port,Bitu val,Bitu iolen) {
	switch (iolen) {
	case 1:
		LOG(LOG_IO,LOG_WARN)("Writing %02X to port %04X",(int)val,(int)port);
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

template <enum IO_Type_t iotype> static unsigned int IO_Gen_Callout_Read(Bitu &ret,IO_ReadHandler* &f,Bitu port,Bitu iolen) {
    int actual = iotype - IO_TYPE_MIN;
    IO_callout_vector &vec = IO_callouts[actual];
    unsigned int match = 0;
    IO_ReadHandler *t_f;
    size_t scan = 0;

    while (scan < vec.size()) {
        IO_CalloutObject &obj = vec[scan++];
        if (!obj.isInstalled()) continue;
        if (obj.m_r_handler == NULL) continue;
        if (!obj.MatchPort((uint16_t)port)) continue;

        t_f = obj.m_r_handler(obj,port,iolen);
        if (t_f != NULL) {
            if (match != 0) {
                if (iotype == IO_TYPE_ISA)
                    ret &= t_f(port,iolen); /* ISA pullup resisters vs ISA devices pulling data lines down (two conflicting devices) */
            }
            else {
                ret = (/*assign and call*/f=t_f)(port,iolen);
            }

            match++;
            if (iotype == IO_TYPE_PCI) /* PCI bus only one device can respond, no conflicts */
                break;
        }
    }

    return match;
}

template <enum IO_Type_t iotype> static unsigned int IO_Gen_Callout_Write(IO_WriteHandler* &f,Bitu port,Bitu val,Bitu iolen) {
    int actual = iotype - IO_TYPE_MIN;
    IO_callout_vector &vec = IO_callouts[actual];
    unsigned int match = 0;
    IO_WriteHandler *t_f;
    size_t scan = 0;

    while (scan < vec.size()) {
        IO_CalloutObject &obj = vec[scan++];
        if (!obj.isInstalled()) continue;
        if (obj.m_w_handler == NULL) continue;
        if (!obj.MatchPort((uint16_t)port)) continue;

        t_f = obj.m_w_handler(obj,port,iolen);
        if (t_f != NULL) {
            t_f(port,val,iolen);
            if (match == 0) f = t_f;
            match++;
        }
    }

    return match;
}

static unsigned int IO_Motherboard_Callout_Read(Bitu &ret,IO_ReadHandler* &f,Bitu port,Bitu iolen) {
    return IO_Gen_Callout_Read<IO_TYPE_MB>(ret,f,port,iolen);
}

static unsigned int IO_PCI_Callout_Read(Bitu &ret,IO_ReadHandler* &f,Bitu port,Bitu iolen) {
    return IO_Gen_Callout_Read<IO_TYPE_PCI>(ret,f,port,iolen);
}

static unsigned int IO_ISA_Callout_Read(Bitu &ret,IO_ReadHandler* &f,Bitu port,Bitu iolen) {
    return IO_Gen_Callout_Read<IO_TYPE_ISA>(ret,f,port,iolen);
}

static unsigned int IO_Motherboard_Callout_Write(IO_WriteHandler* &f,Bitu port,Bitu val,Bitu iolen) {
    return IO_Gen_Callout_Write<IO_TYPE_MB>(f,port,val,iolen);
}

static unsigned int IO_PCI_Callout_Write(IO_WriteHandler* &f,Bitu port,Bitu val,Bitu iolen) {
    return IO_Gen_Callout_Write<IO_TYPE_PCI>(f,port,val,iolen);
}

static unsigned int IO_ISA_Callout_Write(IO_WriteHandler* &f,Bitu port,Bitu val,Bitu iolen) {
    return IO_Gen_Callout_Write<IO_TYPE_ISA>(f,port,val,iolen);
}

static Bitu IO_ReadSlowPath(Bitu port,Bitu iolen) {
    IO_ReadHandler *f = iolen > 1 ? IO_ReadDefault : IO_ReadBlocked;
    unsigned int match = 0;
    unsigned int porti;
    Bitu ret = ~0ul;

    /* check motherboard devices */
    if ((port & 0xFF00) == 0x0000 || IS_PC98_ARCH) /* motherboard-level I/O */
        match = IO_Motherboard_Callout_Read(/*&*/ret,/*&*/f,port,iolen);

    if (match == 0) {
        /* first PCI bus device, then ISA.
         *
         * Note that calling out ISA devices that conflict with PCI
         * is based on experience with an old Pentium system of mine in the late 1990s
         * when I had a Sound Blaster Live PCI! and a Sound Blaster clone both listening
         * to ports 220h-22Fh in Windows 98, in which both cards responded to a DOS
         * game in the DOS box even though read-wise only the Live PCI!'s data was returned.
         * Both cards responded to I/O writes.
         *
         * Based on that experience, my guess is that to keep ISA from going too slow Intel
         * designed the PIIX PCI/ISA bridge to listen for I/O and start an ISA I/O cycle
         * even while another PCI device is preparing to respond to it. Then if nobody takes
         * it, the PCI/ISA bridge completes the ISA bus cycle and takes up the PCI bus
         * transaction. If another PCI device took the I/O write, then the cycle completes
         * quietly and the result is discarded.
         *
         * That's what I think happened, anyway.
         *
         * I wish I had tools to watch I/O transactions on the ISA bus to verify this. --J.C. */
        if (pcibus_enable) {
            /* PCI and PCI/ISA bridge emulation */
            match = IO_PCI_Callout_Read(/*&*/ret,/*&*/f,port,iolen);

            if (match == 0) {
                /* PCI didn't take it, ask ISA bus */
                match = IO_ISA_Callout_Read(/*&*/ret,/*&*/f,port,iolen);
            }
            else {
                Bitu dummy;

                /* PCI did match. Based on behavior noted above, probe ISA bus anyway and discard data. */
                match += IO_ISA_Callout_Read(/*&*/dummy,/*&*/f,port,iolen);
            }
        }
        else {
            /* Pure ISA emulation */
            match = IO_ISA_Callout_Read(/*&*/ret,/*&*/f,port,iolen);
        }
    }

    /* if nothing matched, assign default handler to IO handler slot.
     * if one device responded, assign it's handler to the IO handler slot.
     * if more than one responded, then do not update the IO handler slot. */
    assert(iolen >= 1 && iolen <= 4);
    porti = (iolen >= 4) ? 2 : (unsigned int)(iolen - 1); /* 1 2 x 4 -> 0 1 1 2 */
    LOG(LOG_MISC,LOG_DEBUG)("IO read slow path port=%x iolen=%u: device matches=%u",(unsigned int)port,(unsigned int)iolen,(unsigned int)match);
    if (match == 0) ret = f(port,iolen); /* if nobody responded, then call the default */
    if (match <= 1) io_readhandlers[porti][port] = f;

	return ret;
}

void IO_WriteSlowPath(Bitu port,Bitu val,Bitu iolen) {
    IO_WriteHandler *f = iolen > 1 ? IO_WriteDefault : IO_WriteBlocked;
    unsigned int match = 0;
    unsigned int porti;

    /* check motherboard devices */
    if ((port & 0xFF00) == 0x0000 || IS_PC98_ARCH) /* motherboard-level I/O */
        match = IO_Motherboard_Callout_Write(/*&*/f,port,val,iolen);

    if (match == 0) {
        /* first PCI bus device, then ISA.
         *
         * Note that calling out ISA devices that conflict with PCI
         * is based on experience with an old Pentium system of mine in the late 1990s
         * when I had a Sound Blaster Live PCI! and a Sound Blaster clone both listening
         * to ports 220h-22Fh in Windows 98, in which both cards responded to a DOS
         * game in the DOS box even though read-wise only the Live PCI!'s data was returned.
         * Both cards responded to I/O writes.
         *
         * Based on that experience, my guess is that to keep ISA from going too slow Intel
         * designed the PIIX PCI/ISA bridge to listen for I/O and start an ISA I/O cycle
         * even while another PCI device is preparing to respond to it. Then if nobody takes
         * it, the PCI/ISA bridge completes the ISA bus cycle and takes up the PCI bus
         * transaction. If another PCI device took the I/O write, then the cycle completes
         * quietly and the result is discarded.
         *
         * That's what I think happened, anyway.
         *
         * I wish I had tools to watch I/O transactions on the ISA bus to verify this. --J.C. */
        if (pcibus_enable) {
            /* PCI and PCI/ISA bridge emulation */
            match = IO_PCI_Callout_Write(/*&*/f,port,val,iolen);

            if (match == 0) {
                /* PCI didn't take it, ask ISA bus */
                match = IO_ISA_Callout_Write(/*&*/f,port,val,iolen);
            }
            else {
                /* PCI did match. Based on behavior noted above, probe ISA bus anyway and discard data. */
                match += IO_ISA_Callout_Write(/*&*/f,port,val,iolen);
            }
        }
        else {
            /* Pure ISA emulation */
            match = IO_ISA_Callout_Write(/*&*/f,port,val,iolen);
        }
    }

    /* if nothing matched, assign default handler to IO handler slot.
     * if one device responded, assign it's handler to the IO handler slot.
     * if more than one responded, then do not update the IO handler slot. */
    assert(iolen >= 1 && iolen <= 4);
    porti = (iolen >= 4) ? 2 : (unsigned int)(iolen - 1); /* 1 2 x 4 -> 0 1 1 2 */
    LOG(LOG_MISC,LOG_DEBUG)("IO write slow path port=%x data=%x iolen=%u: device matches=%u",(unsigned int)port,(unsigned int)val,(unsigned int)iolen,(unsigned int)match);
    if (match == 0) f(port,val,iolen); /* if nobody responded, then call the default */
    if (match <= 1) io_writehandlers[porti][port] = f;

#if C_DEBUG && 0
    if (match == 0) DEBUG_EnableDebugger();
#endif
}

void IO_RegisterReadHandler(Bitu port,IO_ReadHandler * handler,Bitu mask,Bitu range) {
    assert((port+range) <= IO_MAX);
	while (range--) {
		if (mask&IO_MB) io_readhandlers[0][port]=handler;
		if (mask&IO_MW) io_readhandlers[1][port]=handler;
		if (mask&IO_MD) io_readhandlers[2][port]=handler;
		port++;
	}
}

void IO_RegisterWriteHandler(Bitu port,IO_WriteHandler * handler,Bitu mask,Bitu range) {
    assert((port+range) <= IO_MAX);
	while (range--) {
		if (mask&IO_MB) io_writehandlers[0][port]=handler;
		if (mask&IO_MW) io_writehandlers[1][port]=handler;
		if (mask&IO_MD) io_writehandlers[2][port]=handler;
		port++;
	}
}

void IO_FreeReadHandler(Bitu port,Bitu mask,Bitu range) {
    assert((port+range) <= IO_MAX);
	while (range--) {
		if (mask&IO_MB) io_readhandlers[0][port]=IO_ReadSlowPath;
		if (mask&IO_MW) io_readhandlers[1][port]=IO_ReadSlowPath;
		if (mask&IO_MD) io_readhandlers[2][port]=IO_ReadSlowPath;
		port++;
	}
}

void IO_FreeWriteHandler(Bitu port,Bitu mask,Bitu range) {
    assert((port+range) <= IO_MAX);
	while (range--) {
		if (mask&IO_MB) io_writehandlers[0][port]=IO_WriteSlowPath;
		if (mask&IO_MW) io_writehandlers[1][port]=IO_WriteSlowPath;
		if (mask&IO_MD) io_writehandlers[2][port]=IO_WriteSlowPath;
		port++;
	}
}

void IO_InvalidateCachedHandler(Bitu port,Bitu range) {
    assert((port+range) <= IO_MAX);
    for (Bitu mb=0;mb <= 2;mb++) {
        Bitu p = port;
        Bitu r = range;
        while (r--) {
            io_writehandlers[mb][p]=IO_WriteSlowPath;
            io_readhandlers[mb][p]=IO_ReadSlowPath;
            p++;
        }
    }
}

void IO_ReadHandleObject::Install(Bitu port,IO_ReadHandler * handler,Bitu mask,Bitu range) {
	if(!installed) {
		installed=true;
		m_port=port;
		m_mask=mask;
		m_range=range;
		IO_RegisterReadHandler(port,handler,mask,range);
	} else E_Exit("IO_readHandler already installed port %x",(int)port);
}

void IO_ReadHandleObject::Uninstall(){
	if(!installed) return;
	IO_FreeReadHandler(m_port,m_mask,m_range);
	installed=false;
}

IO_ReadHandleObject::~IO_ReadHandleObject(){
	Uninstall();
}

void IO_WriteHandleObject::Install(Bitu port,IO_WriteHandler * handler,Bitu mask,Bitu range) {
	if(!installed) {
		installed=true;
		m_port=port;
		m_mask=mask;
		m_range=range;
		IO_RegisterWriteHandler(port,handler,mask,range);
	} else E_Exit("IO_writeHandler already installed port %x",(int)port);
}

void IO_WriteHandleObject::Uninstall() {
	if(!installed) return;
	IO_FreeWriteHandler(m_port,m_mask,m_range);
	installed=false;
}

IO_WriteHandleObject::~IO_WriteHandleObject(){
	Uninstall();
	//LOG_MSG("FreeWritehandler called with port %X",m_port);
}


/* Some code to make io operations take some virtual time. Helps certain
 * games with their timing of certain operations
 */

/* how much delay to add to I/O in nanoseconds */
int io_delay_ns[3] = {-1,-1,-1};

/* nonzero if we're in a callback */
extern unsigned int last_callback;

inline void IO_USEC_read_delay(const unsigned int szidx) {
	if (io_delay_ns[szidx] > 0 && last_callback == 0/*NOT running within a callback function*/) {
		Bits delaycyc = (CPU_CycleMax * io_delay_ns[szidx]) / 1000000;
		CPU_Cycles -= delaycyc;
		CPU_IODelayRemoved += delaycyc;
	}
}

inline void IO_USEC_write_delay(const unsigned int szidx) {
	if (io_delay_ns[szidx] > 0 && last_callback == 0/*NOT running within a callback function*/) {
		Bits delaycyc = (CPU_CycleMax * io_delay_ns[szidx] * 3) / (1000000 * 4);
		CPU_Cycles -= delaycyc;
		CPU_IODelayRemoved += delaycyc;
	}
}

#ifdef ENABLE_PORTLOG
static uint8_t crtc_index = 0;
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
			if (width==0) crtc_index = (uint8_t)val;
			else if(width==1) crtc_index = (uint8_t)(val>>8);
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
				(int)port, (int)val, (int)SegValue(cs), (int)reg_eip);
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
				(int)port, (int)val, (int)SegValue(cs), (int)reg_eip);
			break;
		}
	}
}
#else
#define log_io(W, X, Y, Z)
#endif


void IO_WriteB(Bitu port,uint8_t val) {
	log_io(0, true, port, val);
	if (GCC_UNLIKELY(GETFLAG(VM) && (CPU_IO_Exception(port,1)))) {
		CPU_ForceV86FakeIO_Out(port,val,1);
	}
	else {
		IO_USEC_write_delay(0);
		io_writehandlers[0][port](port,val,1);
	}
}

void IO_WriteW(Bitu port,uint16_t val) {
	log_io(1, true, port, val);
	if (GCC_UNLIKELY(GETFLAG(VM) && (CPU_IO_Exception(port,2)))) {
		CPU_ForceV86FakeIO_Out(port,val,2);
	}
	else {
		IO_USEC_write_delay(1);
		io_writehandlers[1][port](port,val,2);
	}
}

void IO_WriteD(Bitu port,uint32_t val) {
	log_io(2, true, port, val);
	if (GCC_UNLIKELY(GETFLAG(VM) && (CPU_IO_Exception(port,4)))) {
		CPU_ForceV86FakeIO_Out(port,val,4);
	}
	else {
		IO_USEC_write_delay(2);
		io_writehandlers[2][port](port,val,4);
	}
}

uint8_t IO_ReadB(Bitu port) {
	uint8_t retval;
	if (GCC_UNLIKELY(GETFLAG(VM) && (CPU_IO_Exception(port,1)))) {
		return (uint8_t)CPU_ForceV86FakeIO_In(port,1);
	}
	else {
		IO_USEC_read_delay(0);
		retval = (uint8_t)io_readhandlers[0][port](port,1);
	}
	log_io(0, false, port, retval);
	return retval;
}

uint16_t IO_ReadW(Bitu port) {
	uint16_t retval;
	if (GCC_UNLIKELY(GETFLAG(VM) && (CPU_IO_Exception(port,2)))) {
		return (uint16_t)CPU_ForceV86FakeIO_In(port,2);
	}
	else {
		IO_USEC_read_delay(1);
		retval = (uint16_t)io_readhandlers[1][port](port,2);
	}
	log_io(1, false, port, retval);
	return retval;
}

uint32_t IO_ReadD(Bitu port) {
	uint32_t retval;
	if (GCC_UNLIKELY(GETFLAG(VM) && (CPU_IO_Exception(port,4)))) {
		return (uint32_t)CPU_ForceV86FakeIO_In(port,4);
	}
	else {
		IO_USEC_read_delay(2);
		retval = (uint32_t)io_readhandlers[2][port](port,4);
	}
	log_io(2, false, port, retval);
	return retval;
}

void IO_Reset(Section * /*sec*/) { // Reset or power on
	Section_prop * section=static_cast<Section_prop *>(control->GetSection("dosbox"));

	io_delay_ns[0] = section->Get_int("iodelay");
	if (io_delay_ns[0] < 0) { // 8-bit transfers are said to have a transfer cycle with 4 wait states
		// TODO: How can we emulate Intel 440FX chipsets that allow 8-bit PIO with 1 wait state, or zero wait state devices?
		double t = (1000000000.0 * clockdom_ISA_BCLK.freq_div * 8.5) / clockdom_ISA_BCLK.freq;
		io_delay_ns[0] = (int)floor(t);
	}

	io_delay_ns[1] = section->Get_int("iodelay16");
	if (io_delay_ns[1] < 0) { // 16-bit transfers are said to have a transfer cycle with 1 wait state
		// TODO: How can we emulate ISA devices that support zero wait states?
		double t = (1000000000.0 * clockdom_ISA_BCLK.freq_div * 5.5) / clockdom_ISA_BCLK.freq;
		io_delay_ns[1] = (int)floor(t);
	}

	io_delay_ns[2] = section->Get_int("iodelay32");
	if (io_delay_ns[2] < 0) { // assume ISA bus details that turn 32-bit PIO into two 16-bit I/O transfers
		// TODO: If the device is a PCI device, then 32-bit PIO should take the same amount of time as 8/16-bit PIO
		double t = (1000000000.0 * clockdom_ISA_BCLK.freq_div * (5.5 + 5.5)) / clockdom_ISA_BCLK.freq;
		io_delay_ns[2] = (int)floor(t);
	}

	LOG(LOG_IO,LOG_DEBUG)("I/O 8-bit delay %uns",io_delay_ns[0]);
	LOG(LOG_IO,LOG_DEBUG)("I/O 16-bit delay %uns",io_delay_ns[1]);
	LOG(LOG_IO,LOG_DEBUG)("I/O 32-bit delay %uns",io_delay_ns[2]);
}

void IO_Init() {
	LOG(LOG_MISC,LOG_DEBUG)("Initializing I/O port handler system");

	/* init the ports, rather than risk I/O jumping to random code */
	IO_FreeReadHandler(0,IO_MA,IO_MAX);
	IO_FreeWriteHandler(0,IO_MA,IO_MAX);

	/* please call our reset function on power-on and reset */
	AddVMEventFunction(VM_EVENT_RESET,AddVMEventFunctionFuncPair(IO_Reset));

    /* prepare callouts */
    IO_InitCallouts();
}

void IO_CalloutObject::InvalidateCachedHandlers(void) {
    Bitu p;

    /* NTS: Worst case scenario might be a 10-bit ISA device with 16 I/O ports,
     *      which would then require resetting 1024 entries of the array.
     *
     *      A 10-bit decode ignores the upper 6 bits = 1 << 6 = 64
     *
     *      64 * 16 = 1024 I/O ports to be reset.
     *
     *      Not too bad, really. */

    /* for both the base I/O, as well as it's aliases, revert the I/O ports back to "slow path" */
    for (p=m_port;p < 0x10000ul;p += alias_mask+1ul)
        IO_InvalidateCachedHandler(p,range_mask+1ul);
}

void IO_CalloutObject::Install(Bitu port,Bitu portmask/*IOMASK_ISA_10BIT, etc.*/,IO_ReadCalloutHandler *r_handler,IO_WriteCalloutHandler *w_handler) {
	if(!installed) {
        if (portmask == 0 || (portmask & ~0xFFFFU)) {
            LOG(LOG_MISC,LOG_ERROR)("IO_CalloutObject::Install: Port mask %x is invalid",(unsigned int)portmask);
            return;
        }

        /* we need a mask for the distance between aliases of the port, and the range of I/O ports. */
        /* only the low part of the mask where bits are zero, not the upper.
         * This loop is the reason portmask cannot be ~0 else it would become an infinite loop.
         * This also serves to check that the mask is a proper combination of ISA masking and
         * I/O port range (example: IOMASK_ISA_10BIT & (~0x3) == 0x3FF & 0xFFFFFFFC = 0x3FC for a device with 4 I/O ports).
         * A proper mask has (from MSB to LSB):
         *   - zero or more 0 bits from MSB
         *   - 1 or more 1 bits in the middle
         *   - zero or more 0 bits to LSB */
        {
            Bitu m = 1;
            Bitu test;

            /* compute range mask from zero bits at LSB */
            range_mask = 0;
            test = portmask ^ 0xFFFFU;
            while ((test & m) == m) {
                range_mask = (uint16_t)m;
                m = (m << 1) + 1;
            }

            /* DEBUG */
            if ((portmask & range_mask) != 0 ||
                ((range_mask + 1) & range_mask) != 0/* should be a mask, therefore AND by itself + 1 should be zero (think (0xF + 1) & 0xF = 0x10 & 0xF = 0x0) */) {
                LOG(LOG_MISC,LOG_ERROR)("IO_CalloutObject::Install: portmask(%x) & range_mask(%x) != 0 (%x). You found a corner case that broke this code, fix it.",
                    (unsigned int)portmask,
                    (unsigned int)range_mask,
                    (unsigned int)(portmask & range_mask));
                return;
            }

            /* compute alias mask from middle 1 bits */
            alias_mask = range_mask;
            test = portmask + range_mask; /* will break if portmask & range_mask != 0 */
            while ((test & m) == m) {
                alias_mask = (uint16_t)m;
                m = (m << 1) + 1;
            }

            /* any bits after that should be zero. */
            /* confirm this by XORing portmask by (alias_mask ^ range_mask). */
            /* we already confirmed that portmask & range_mask == 0. */
            /* Example:
             *
             *    Sound Blaster at port 220-22Fh with 10-bit ISA decode would be 0x03F0 therefore:
             *      portmask =    0x03F0
             *      range_mask =  0x000F
             *      alias_mask =  0x03FF
             *
             *      portmask ^ range_mask = 0x3FF
             *      portmask ^ range_mask ^ alias_mask = 0x0000
             *
             * Example of invalid portmask 0x13F0:
             *      portmask =    0x13F0
             *      range_mask =  0x000F
             *      alias_mask =  0x03FF
             *
             *      portmask ^ range_mask = 0x13FF
             *      portmask ^ range_mask ^ alias_mask = 0x1000 */
            if ((portmask ^ range_mask ^ alias_mask) != 0 ||
                ((alias_mask + 1) & alias_mask) != 0/* should be a mask, therefore AND by itself + 1 should be zero */) {
                LOG(LOG_MISC,LOG_ERROR)("IO_CalloutObject::Install: portmask(%x) ^ range_mask(%x) ^ alias_mask(%x) != 0 (%x). Invalid portmask.",
                    (unsigned int)portmask,
                    (unsigned int)range_mask,
                    (unsigned int)alias_mask,
                    (unsigned int)(portmask ^ range_mask ^ alias_mask));
                return;
            }

            if (port & range_mask) {
                LOG(LOG_MISC,LOG_ERROR)("IO_CalloutObject::Install: port %x and port mask %x not aligned (range_mask %x)",
                    (unsigned int)port,(unsigned int)portmask,(unsigned int)range_mask);
                return;
            }
        }

		installed=true;
		m_port=port;
		m_mask=0; /* not used */
		m_range=0; /* not used */
        io_mask=(uint16_t)portmask;
        m_r_handler=r_handler;
        m_w_handler=w_handler;

        /* add this object to the callout array.
         * don't register any I/O handlers. those will be registered during the "slow path"
         * callout process when the CPU goes to access them. to encourage that to happen,
         * we invalidate the I/O ranges */
        LOG(LOG_MISC,LOG_DEBUG)("IO_CalloutObject::Install added device with port=0x%x io_mask=0x%x rangemask=0x%x aliasmask=0x%x",
            (unsigned int)port,(unsigned int)io_mask,(unsigned int)range_mask,(unsigned int)alias_mask);

        InvalidateCachedHandlers();
    }
}

void IO_CalloutObject::Uninstall() {
	if(!installed) return;
    InvalidateCachedHandlers();
    installed=false;
}

void IO_InitCallouts(void) {
    /* make sure each vector has enough for a typical load */
    IO_callouts[IO_callouts_index(IO_TYPE_ISA)].resize(64);
    IO_callouts[IO_callouts_index(IO_TYPE_PCI)].resize(64);
    IO_callouts[IO_callouts_index(IO_TYPE_MB)].resize(64);
}

/* callers maintain a handle to it.
 * if they need to touch it, they get a pointer, which they then have to put back.
 * The way DOSBox/DOSbox-X code handles IO callbacks it's common to declare an IO object,
 * call the install, but then never touch it again, so this should work fine.
 *
 * this allows us to maintain ready-made IO callout objects to return quickly rather
 * than write more complicated code where the caller has to make an IO_CalloutObject
 * and then call install and we have to add it's pointer to a list/vector/whatever.
 * It also avoids problems where if we have to resize the vector, the pointers become
 * invalid, because callers have only handles and they have to put all the pointers
 * back in order for us to resize the vector. */
IO_Callout_t IO_AllocateCallout(IO_Type_t t) {
    if (t < IO_TYPE_MIN || t >= IO_TYPE_MAX)
        return IO_Callout_t_none;

    IO_callout_vector &vec = IO_callouts[t - IO_TYPE_MIN];

try_again:
    while (vec.alloc_from < vec.size()) {
        IO_CalloutObject &obj = vec[vec.alloc_from];

        if (!obj.alloc) {
            obj.alloc = true;
            assert(obj.isInstalled() == false);
            return IO_Callout_t_comb(t,vec.alloc_from++); /* make combination, then increment alloc_from */
        }

        vec.alloc_from++;
    }

    /* okay, double the size of the vector within reason.
     * if anyone has pointers out to our elements, then we cannot resize. vector::resize() may invalidate them. */
    if (vec.size() < 4096 && vec.getcounter == 0) {
        size_t nsz = vec.size() * 2;

        LOG(LOG_MISC,LOG_WARN)("IO_AllocateCallout type %u expanding array to %u",(unsigned int)t,(unsigned int)nsz);
        vec.alloc_from = (unsigned int)vec.size(); /* allocate from end of old vector size */
        vec.resize(nsz);
        goto try_again;
    }

    LOG(LOG_MISC,LOG_WARN)("IO_AllocateCallout type %u no free entries",(unsigned int)t);
    return IO_Callout_t_none;
}

void IO_FreeCallout(IO_Callout_t c) {
    enum IO_Type_t t = IO_Callout_t_type(c);

    if (t < IO_TYPE_MIN || t >= IO_TYPE_MAX)
        return;

    IO_callout_vector &vec = IO_callouts[t - IO_TYPE_MIN];
    uint32_t idx = IO_Callout_t_index(c);

    if (idx >= vec.size())
        return;

    IO_CalloutObject &obj = vec[idx];
    if (!obj.alloc) return;

    if (obj.isInstalled())
        obj.Uninstall();

    obj.alloc = false;
    if (vec.alloc_from > idx)
        vec.alloc_from = idx; /* an empty slot just opened up, you can alloc from there */
}

IO_CalloutObject *IO_GetCallout(IO_Callout_t c) {
    enum IO_Type_t t = IO_Callout_t_type(c);

    if (t < IO_TYPE_MIN || t >= IO_TYPE_MAX)
        return NULL;

    IO_callout_vector &vec = IO_callouts[t - IO_TYPE_MIN];
    uint32_t idx = IO_Callout_t_index(c);

    if (idx >= vec.size())
        return NULL;

    IO_CalloutObject &obj = vec[idx];
    if (!obj.alloc) return NULL;
    obj.getcounter++;

    return &obj;
}

void IO_PutCallout(IO_CalloutObject *obj) {
    if (obj == NULL) return;
    if (obj->getcounter == 0) return;
    obj->getcounter--;
}

//save state support
namespace
{
class SerializeIO : public SerializeGlobalPOD
{
public:
    SerializeIO() : SerializeGlobalPOD("IO handler")
    {
        //io_writehandlers -> quasi constant
        //io_readhandlers  -> quasi constant

        //registerPOD(iof_queue.used); registerPOD(iof_queue.entries);
    }
} dummy;
}
