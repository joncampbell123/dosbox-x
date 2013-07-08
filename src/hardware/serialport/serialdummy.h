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

/* $Id: serialdummy.h,v 1.5 2009-05-27 09:15:41 qbix79 Exp $ */

#ifndef INCLUDEGUARD_SERIALDUMMY_H
#define INCLUDEGUARD_SERIALDUMMY_H

#include "serialport.h"

//#define CHECKIT_TESTPLUG

class CSerialDummy : public CSerial {
public:
	CSerialDummy(Bitu id, CommandLine* cmd);
	~CSerialDummy();

	void setRTSDTR(bool rts, bool dtr);
	void setRTS(bool val);
	void setDTR(bool val);

	void updatePortConfig(Bit16u, Bit8u lcr);
	void updateMSR();
	void transmitByte(Bit8u val, bool first);
	void setBreak(bool value);
	void handleUpperEvent(Bit16u type);

#ifdef CHECKIT_TESTPLUG
	Bit8u loopbackdata;
#endif

};

#endif // INCLUDEGUARD
