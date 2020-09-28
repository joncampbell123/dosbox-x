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


#ifndef INCLUDEGUARD_SERIALDUMMY_H
#define INCLUDEGUARD_SERIALDUMMY_H

#include "serialport.h"

//#define CHECKIT_TESTPLUG

class CSerialDummy : public CSerial {
public:
	CSerialDummy(Bitu id, CommandLine* cmd);
	virtual ~CSerialDummy();

	void setRTSDTR(bool rts, bool dtr);
	void setRTS(bool val);
	void setDTR(bool val);

	void updatePortConfig(Bit16u, uint8_t lcr);
	void updateMSR();
	void transmitByte(uint8_t val, bool first);
	void setBreak(bool value);
	void handleUpperEvent(Bit16u type);

#ifdef CHECKIT_TESTPLUG
	uint8_t loopbackdata;
#endif

};

#endif // INCLUDEGUARD
