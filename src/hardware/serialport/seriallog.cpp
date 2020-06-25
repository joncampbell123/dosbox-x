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


#include "dosbox.h"

#include "setup.h"
#include "serialdummy.h"
#include "serialport.h"
#include "seriallog.h"

CSerialLog::CSerialLog(Bitu id,	CommandLine* cmd):CSerial(id, cmd) {
	CSerial::Init_Registers();
	// DSR+CTS on to make sure the DOS COM device will not get stuck waiting for them
	setRI(false);
	setCD(false);
	setDSR(true);
	setCTS(true);
	InstallationSuccessful=true;
}

CSerialLog::~CSerialLog() {
	// clear events
	removeEvent(SERIAL_TX_EVENT);
}

void CSerialLog::log_emit() {
	if (!log_line.empty()) {
		LOG_MSG("CSerial Log: %s",log_line.c_str());
		log_line.clear();
	}
}

void CSerialLog::handleUpperEvent(Bit16u type) {
	if(type==SERIAL_TX_EVENT) {
	//LOG_MSG("SERIAL_TX_EVENT");
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
void CSerialLog::updatePortConfig(Bit16u divider, Bit8u lcr) {
    (void)divider;//UNUSED
    (void)lcr;//UNUSED
	//LOG_MSG("Serial port at 0x%x: Port params changed: %d Baud", base,dcb.BaudRate);
}

void CSerialLog::updateMSR() {
}

void CSerialLog::transmitByte(Bit8u val, bool first) {
	if(first) setEvent(SERIAL_THR_EVENT, bytetime/10); 
	else setEvent(SERIAL_TX_EVENT, bytetime);

	if (val == '\n' || val == '\r')
		log_emit();
	else {
		log_line += (char)val;
		if (log_line.length() >= 256) log_emit();
	}
}

/*****************************************************************************/
/* setBreak(val) switches break on or off                                   **/
/*****************************************************************************/

void CSerialLog::setBreak(bool value) {
    (void)value;//UNUSED
	//LOG_MSG("UART 0x%x: Break toggeled: %d", base, value);
}

/*****************************************************************************/
/* setRTSDTR sets the modem control lines                                   **/
/*****************************************************************************/
void CSerialLog::setRTSDTR(bool rts, bool dtr) {
	setRTS(rts);
	setDTR(dtr);
}

void CSerialLog::setRTS(bool val) {
	setCTS(val);
}

void CSerialLog::setDTR(bool val) {
	setDSR(val);
	setRI(val);
	setCD(val);
}
