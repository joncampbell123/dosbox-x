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


#ifndef INCLUDEGUARD_SERIALLOG_H
#define INCLUDEGUARD_SERIALLOG_H

#include <string>

#include "serialport.h"

//#define CHECKIT_TESTPLUG

class CSerialLog : public CSerial {
public:
	CSerialLog(Bitu id, CommandLine* cmd);
	virtual ~CSerialLog();

	void setRTSDTR(bool rts, bool dtr);
	void setRTS(bool val);
	void setDTR(bool val);

	void updatePortConfig(Bit16u, Bit8u lcr);
	void updateMSR();
	void transmitByte(Bit8u val, bool first);
	void setBreak(bool value);
	void handleUpperEvent(Bit16u type);

	void log_emit();

	std::string log_line;
};

#endif // INCLUDEGUARD
