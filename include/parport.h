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

#ifndef DOSBOX_PARPORT_H
#define DOSBOX_PARPORT_H

// set to 1 for debug messages and debugging log:
#define PARALLEL_DEBUG 0

#ifndef DOSBOX_DOSBOX_H
#include "dosbox.h"
#endif
#ifndef DOSBOX_INOUT_H
#include "inout.h"
#endif

#include "control.h"
#include "dos_inc.h"

class device_LPT : public DOS_Device {
public:
	// Creates a LPT device that communicates with the num-th parallel port, i.e. is LPTnum
	device_LPT(Bit8u num, class CParallel* pp);
	virtual ~device_LPT();
	bool Read(Bit8u * data,Bit16u * size);
	bool Write(Bit8u * data,Bit16u * size);
	bool Seek(Bit32u * pos,Bit32u type);
	bool Close();
	Bit16u GetInformation(void);
private:
	CParallel* pportclass;
	Bit8u num; // This device is LPTnum
};


class CParallel {
public:
#if PARALLEL_DEBUG
	FILE * debugfp;
	bool dbg_data;
	bool dbg_putchar;
	bool dbg_cregs;
	bool dbg_plainputchar;
	bool dbg_plaindr;
	void log_par(bool active, char const* format,...);
#endif

	// Constructor
	CParallel(CommandLine* cmd, Bitu portnr, Bit8u initirq);
	
	virtual ~CParallel();

	IO_ReadHandleObject ReadHandler[3];
	IO_WriteHandleObject WriteHandler[3];

	void setEvent(Bit16u type, float duration);
	void removeEvent(Bit16u type);
	void handleEvent(Bit16u type);
	virtual void handleUpperEvent(Bit16u type)=0;
	
	Bitu port_nr;
	Bitu base;
	Bitu irq;
	
	// read data line register
	virtual Bitu Read_PR()=0;
	virtual Bitu Read_COM()=0;
	virtual Bitu Read_SR()=0;

	virtual void Write_PR(Bitu)=0;
	virtual void Write_CON(Bitu)=0;
	virtual void Write_IOSEL(Bitu)=0;

	void Write_reserved(Bit8u data, Bit8u address);

	virtual bool Putchar(Bit8u)=0;
	bool Putchar_default(Bit8u);
	Bit8u getPrinterStatus();
	void initialize();

	DOS_Device* mydosdevice;
};

extern CParallel* parallelPortObjects[];
void PARALLEL_Init (Section * sec);

const Bit16u parallel_baseaddr[3] = {0x378,0x278,0x3bc};

#endif

