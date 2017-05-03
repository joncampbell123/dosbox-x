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


#ifndef DOSBOX_INOUT_H
#define DOSBOX_INOUT_H

#include <stdio.h>

#define IO_MAX (64*1024+3)

#define IO_MB	0x1
#define IO_MW	0x2
#define IO_MD	0x4
#define IO_MA	(IO_MB | IO_MW | IO_MD )

typedef Bitu IO_ReadHandler(Bitu port,Bitu iolen);
typedef void IO_WriteHandler(Bitu port,Bitu val,Bitu iolen);

typedef IO_ReadHandler* (IO_ReadCalloutHandler)(Bitu port,Bitu iolen);
typedef IO_WriteHandler* (IO_WriteCalloutHandler)(Bitu port,Bitu iolen);

extern IO_WriteHandler * io_writehandlers[3][IO_MAX];
extern IO_ReadHandler * io_readhandlers[3][IO_MAX];

void IO_RegisterReadHandler(Bitu port,IO_ReadHandler * handler,Bitu mask,Bitu range=1);
void IO_RegisterWriteHandler(Bitu port,IO_WriteHandler * handler,Bitu mask,Bitu range=1);

void IO_FreeReadHandler(Bitu port,Bitu mask,Bitu range=1);
void IO_FreeWriteHandler(Bitu port,Bitu mask,Bitu range=1);

void IO_InvalidateCachedHandler(Bitu port,Bitu range=1);

void IO_WriteB(Bitu port,Bitu val);
void IO_WriteW(Bitu port,Bitu val);
void IO_WriteD(Bitu port,Bitu val);

Bitu IO_ReadB(Bitu port);
Bitu IO_ReadW(Bitu port);
Bitu IO_ReadD(Bitu port);

static const Bitu IOMASK_ISA_10BIT = 0x3FFU; /* ISA 10-bit decode */
static const Bitu IOMASK_ISA_12BIT = 0xFFFU; /* ISA 12-bit decode */
static const Bitu IOMASK_FULL = 0xFFFFU; /* full 16-bit decode */

/* Classes to manage the IO objects created by the various devices.
 * The io objects will remove itself on destruction.*/
class IO_Base{
protected:
	bool installed;
	Bitu m_port, m_mask/*IO_MB, etc.*/, m_range/*number of ports*/;
public:
	IO_Base() : installed(false), m_port(0), m_mask(0), m_range(0) {};
};
/* NTS: To explain the Install() method, the caller not only provides the IOMASK_.. value, but ANDs
 *      the least significant bits to define the range of I/O ports to respond to. An ISA Sound Blaster
 *      for example would set portmask = (IOMASK_ISA_10BIT & (~0xF)) in order to respond to 220h-22Fh,
 *      240h-24Fh, etc. At I/O callout time, the callout object is tested
 *      if (cpu_ioport & io_mask) == (m_port & io_mask)
 *
 *      This does not prevent emulation of devices that start on non-aligned ports or strange port ranges,
 *      because the callout handler is free to decline the I/O request, leading the callout process to
 *      move on to the next device or mark the I/O port as empty. */
class IO_CalloutObject: private IO_Base {
public:
    IO_CalloutObject() : IO_Base(), io_mask(0xFFFFU), range_mask(0U), alias_mask(0xFFFFU) {};
    void InvalidateCachedHandlers(void);
	void Install(Bitu port,Bitu portmask/*IOMASK_ISA_10BIT, etc.*/,IO_ReadCalloutHandler *r_handler,IO_WriteCalloutHandler *w_handler);
	void Uninstall();
    ~IO_CalloutObject();
public:
    Bit16u io_mask;
    Bit16u range_mask;
    Bit16u alias_mask;
public:
    inline bool MatchPort(const Bit16u p) {
        /* (p & io_mask) == (m_port & io_mask) but this also works.
         * apparently modern x86 processors are faster at addition/subtraction than bitmasking.
         * for this to work, m_port must be a multiple of the I/O range. For example, if the I/O
         * range is 16 ports, then m_port must be a multiple of 16. */
        return ((p - m_port) & io_mask) == 0;
    }
};
class IO_ReadHandleObject: private IO_Base {
public:
    IO_ReadHandleObject() : IO_Base() {};
	void Install(Bitu port,IO_ReadHandler * handler,Bitu mask,Bitu range=1);
	void Uninstall();
	~IO_ReadHandleObject();
};
class IO_WriteHandleObject: private IO_Base{
public:
    IO_WriteHandleObject() : IO_Base() {};
	void Install(Bitu port,IO_WriteHandler * handler,Bitu mask,Bitu range=1);
	void Uninstall();
	~IO_WriteHandleObject();
};

static INLINE void IO_Write(Bitu port,Bit8u val) {
	IO_WriteB(port,val);
}
static INLINE Bit8u IO_Read(Bitu port){
	return (Bit8u)IO_ReadB(port);
}

#endif
