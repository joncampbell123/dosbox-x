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

/* $Id: serialdummy.h,v 1.5 2009-05-27 09:15:41 qbix79 Exp $ */

#ifndef INCLUDEGUARD_SERIALMOUSE_H
#define INCLUDEGUARD_SERIALMOUSE_H

#include "serialport.h"

//#define CHECKIT_TESTPLUG

class CSerialMouse : public CSerial {
public:
	CSerialMouse(Bitu id, CommandLine* cmd);
	virtual ~CSerialMouse();

	void setRTSDTR(bool rts, bool dtr);
	void setRTS(bool val);
	void setDTR(bool val);

	void updatePortConfig(uint16_t, uint8_t lcr);
	void updateMSR();
	void transmitByte(uint8_t val, bool first);
	void setBreak(bool value);
	void handleUpperEvent(uint16_t type);
	void onMouseReset();
	void on_mouse_event(int delta_x,int delta_y,uint8_t buttonstate);
	void start_packet();

	uint8_t send_ack;
    uint8_t packet[3] = {};
	uint8_t packet_xmit;
	uint8_t mouse_buttons;	/* bit 0 = left   bit 1 = right     becomes bits 5 (L) and 4 (R) in packet[0] */
	uint8_t xmit_another_packet;
	int mouse_delta_x,mouse_delta_y;
#ifdef CHECKIT_TESTPLUG
	uint8_t loopbackdata;
#endif

};

#endif // INCLUDEGUARD
