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
#include "serialfile.h"

CSerialFile::CSerialFile(Bitu id,CommandLine* cmd):CSerial(id, cmd) {
	CSerial::Init_Registers();
	// DSR+CTS on to make sure the DOS COM device will not get stuck waiting for them
	setRI(false);
	setCD(false);
	setDSR(true);
	setCTS(true);

    filename = "serial"; // Default output filename
    cmd->FindStringBegin("file:", filename, false); // if the user specifies serial1=file file:something, set it to that
    LOG_MSG("Serial: port %d will write to file %s", int(id), filename.c_str());
	
    InstallationSuccessful=true;
}

CSerialFile::~CSerialFile() {
    if (fp != NULL) {
        fclose(fp);
        fp = NULL;
    }

	// clear events
	removeEvent(SERIAL_TX_EVENT);
}

void CSerialFile::handleUpperEvent(Bit16u type) {
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
void CSerialFile::updatePortConfig(Bit16u divider, uint8_t lcr) {
    (void)divider;//UNUSED
    (void)lcr;//UNUSED
	//LOG_MSG("Serial port at 0x%x: Port params changed: %d Baud", base,dcb.BaudRate);
}

void CSerialFile::updateMSR() {
}

void CSerialFile::transmitByte(uint8_t val, bool first) {
	if(first) setEvent(SERIAL_THR_EVENT, bytetime/10); 
	else setEvent(SERIAL_TX_EVENT, bytetime);

    if (fp == NULL) {
        fp = fopen(filename.c_str(),"wb");
        if (fp != NULL) setbuf(fp,NULL); // disable buffering
    }
    if (fp != NULL)
        fwrite(&val,1,1,fp);
}

/*****************************************************************************/
/* setBreak(val) switches break on or off                                   **/
/*****************************************************************************/

void CSerialFile::setBreak(bool value) {
    (void)value;//UNUSED
	//LOG_MSG("UART 0x%x: Break toggeled: %d", base, value);
}

/*****************************************************************************/
/* setRTSDTR sets the modem control lines                                   **/
/*****************************************************************************/
void CSerialFile::setRTSDTR(bool rts, bool dtr) {
	setRTS(rts);
	setDTR(dtr);
}

void CSerialFile::setRTS(bool val) {
	setCTS(val);
}

void CSerialFile::setDTR(bool val) {
	setDSR(val);
	setRI(val);
	setCD(val);
}
