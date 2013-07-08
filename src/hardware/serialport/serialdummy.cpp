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

/* $Id: serialdummy.cpp,v 1.5 2009-05-27 09:15:41 qbix79 Exp $ */

#include "dosbox.h"

#include "setup.h"
#include "serialdummy.h"
#include "serialport.h"

CSerialDummy::CSerialDummy(Bitu id,	CommandLine* cmd):CSerial(id, cmd) {
	CSerial::Init_Registers();
	setRI(false);
	setDSR(false);
	setCD(false);
	setCTS(false);
	InstallationSuccessful=true;
}

CSerialDummy::~CSerialDummy() {
	// clear events
	removeEvent(SERIAL_TX_EVENT);
}

void CSerialDummy::handleUpperEvent(Bit16u type) {
	if(type==SERIAL_TX_EVENT) {
	//LOG_MSG("SERIAL_TX_EVENT");
#ifdef CHECKIT_TESTPLUG
		receiveByte(loopbackdata);
#endif
		ByteTransmitted(); // tx timeout
	}
	else if(type==SERIAL_THR_EVENT){
		//LOG_MSG("SERIAL_THR_EVENT");
		ByteTransmitting();
		setEvent(SERIAL_TX_EVENT,bytetime);
	}

}

/*****************************************************************************/
/* updatePortConfig is called when emulated app changes the serial port     **/
/* parameters baudrate, stopbits, number of databits, parity.               **/
/*****************************************************************************/
void CSerialDummy::updatePortConfig(Bit16u divider, Bit8u lcr) {
	//LOG_MSG("Serial port at 0x%x: Port params changed: %d Baud", base,dcb.BaudRate);
}

void CSerialDummy::updateMSR() {
}
void CSerialDummy::transmitByte(Bit8u val, bool first) {

	if(first) setEvent(SERIAL_THR_EVENT, bytetime/10); 
	else setEvent(SERIAL_TX_EVENT, bytetime);

#ifdef CHECKIT_TESTPLUG
	loopbackdata=val;
#endif
}

/*****************************************************************************/
/* setBreak(val) switches break on or off                                   **/
/*****************************************************************************/

void CSerialDummy::setBreak(bool value) {
	//LOG_MSG("UART 0x%x: Break toggeled: %d", base, value);
}

/*****************************************************************************/
/* setRTSDTR sets the modem control lines                                   **/
/*****************************************************************************/
void CSerialDummy::setRTSDTR(bool rts, bool dtr) {
	setRTS(rts);
	setDTR(dtr);
}
void CSerialDummy::setRTS(bool val) {
#ifdef CHECKIT_TESTPLUG
	setCTS(val);
#endif
}
void CSerialDummy::setDTR(bool val) {
#ifdef CHECKIT_TESTPLUG
	setDSR(val);
	setRI(val);
	setCD(val);
#endif
}
