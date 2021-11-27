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

#ifndef DOSBOX_PARPORT_H
#define DOSBOX_PARPORT_H

// set to 1 for debug messages and debugging log:
#define PARALLEL_DEBUG 0

#include "inout.h"

#include "programs.h"

class device_LPT : public DOS_Device {
public:
	// Creates a LPT device that communicates with the num-th parallel port, i.e. is LPTnum
	device_LPT(uint8_t num, class CParallel* pp);
	virtual ~device_LPT();
	bool Read(uint8_t * data,uint16_t * size);
	bool Write(const uint8_t * data,uint16_t * size);
	bool Seek(uint32_t * pos,uint32_t type);
	bool Close();
	uint16_t GetInformation(void);
private:
	CParallel* pportclass;
	uint8_t num; // This device is LPTnum
};

enum ParallelTypesE {
	PARALLEL_TYPE_DISABLED = 0,
#if C_DIRECTLPT
	PARALLEL_TYPE_REALLPT,
#endif
	PARALLEL_TYPE_FILE,
#if C_PRINTER
	PARALLEL_TYPE_PRINTER,
#endif
	PARALLEL_TYPE_DISNEY,
	PARALLEL_TYPE_COUNT
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
	CParallel(CommandLine* cmd, Bitu portnr, uint8_t initirq);
	
	virtual ~CParallel();

	IO_ReadHandleObject ReadHandler[3];
	IO_WriteHandleObject WriteHandler[3];

	void setEvent(uint16_t type, float duration);
	void removeEvent(uint16_t type);
	void handleEvent(uint16_t type);
	virtual void handleUpperEvent(uint16_t type)=0;

	void registerDOSDevice();
	void unregisterDOSDevice();

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

	virtual bool Putchar(uint8_t)=0;
	uint8_t getPrinterStatus();
	void initialize();

	// What type of port is this?
	ParallelTypesE parallelType = PARALLEL_TYPE_DISABLED;

	// How was it created?
	std::string commandLineString = "";

	DOS_Device* mydosdevice;
};

extern CParallel* parallelPortObjects[];
extern uint16_t parallel_baseaddr[9];

#endif
