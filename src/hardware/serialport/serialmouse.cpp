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

/* Microsoft Serial Mouse compatible emulation.
 * Written by Jonathan Campbell */

#include "dosbox.h"

#include "mouse.h"
#include "setup.h"
#include "serialdummy.h"
#include "serialmouse.h"
#include "serialport.h"

static CSerialMouse *serial_mice[4] = {NULL};

/* this function is the method the GUI and mouse emulation notifies us of movement/button events.
 * if the serial mouse is not installed, it is ignored */
void on_mouse_event_for_serial(int delta_x,int delta_y,Bit8u buttonstate) {
	int i;

	for (i=0;i < 4;i++) {
		if (serial_mice[i] != NULL)
			serial_mice[i]->on_mouse_event(delta_x,delta_y,buttonstate);
	}
}

void CSerialMouse::start_packet() {
	xmit_another_packet = false;

	/* FIX: Default mapping deltas as-is in previous versions of this code
	 *      resulted in serial mouse input that was way too sensitive */
	mouse_delta_x /= 4;
	mouse_delta_y /= 4;

	if (mouse_delta_x < -128) mouse_delta_x = -128;
	else if (mouse_delta_x > 127) mouse_delta_x = 127;
	mouse_delta_x &= 0xFF;

	if (mouse_delta_y < -128) mouse_delta_y = -128;
	else if (mouse_delta_y > 127) mouse_delta_y = 127;
	mouse_delta_y &= 0xFF;

	/* NOTE TO SELF: Do NOT set bit 7. It confuses CTMOUSE.EXE (CuteMouse) serial support.
	 *               Leaving it clear is the only way to make mouse movement possible.
	 *               Microsoft Windows on the other hand doesn't care if bit 7 is set. */
	packet_xmit = 0;
	/* Byte 0:    X   1  LB  RB  Y7  Y6  X7  X6 */
	packet[0] = 0x40 | (mouse_buttons << 4) | (((mouse_delta_y >> 6) & 3) << 2) | ((mouse_delta_x >> 6) & 3);
	/* Byte 1:    X   0  X5-X0 */
	packet[1] = 0x00 | (mouse_delta_x & 0x3F);
	/* Byte 2:    X   0  Y5-Y0 */
	packet[2] = 0x00 | (mouse_delta_y & 0x3F);

	/* clear counters */
	mouse_delta_x = mouse_delta_y = 0;

	setEvent(SERIAL_RX_EVENT, bytetime);
}

void CSerialMouse::on_mouse_event(int delta_x,int delta_y,Bit8u buttonstate) {
	mouse_buttons = ((buttonstate & 1) ? 2 : 0) | ((buttonstate & 2) ? 1 : 0);
	mouse_delta_x += delta_x;
	mouse_delta_y += delta_y;

	/* initiate data transfer and form the packet to transmit. if another packet
	 * is already transmitting now then wait for it to finish before transmitting ours,
	 * and let the mouse motion accumulate in the meantime */
	if (packet_xmit >= 3) start_packet();
	else xmit_another_packet = true;
}

CSerialMouse::CSerialMouse(Bitu id,	CommandLine* cmd):CSerial(id, cmd) {
	CSerial::Init_Registers();
	setRI(false);
	setDSR(false);
	setCD(false);
	setCTS(false);
	InstallationSuccessful=true;
	send_ack=true;
	packet_xmit=0xFF;
	mouse_buttons=0;
	serial_mice[id] = this; /* <- NTS: 'id' is just the index of the serial mouse 0..3 for COM1...COM4 */
	xmit_another_packet=false;
	mouse_delta_x=mouse_delta_y=0;
}

CSerialMouse::~CSerialMouse() {
	// clear events
	removeEvent(SERIAL_TX_EVENT);
}

void CSerialMouse::handleUpperEvent(Bit16u type) {
	if(type==SERIAL_TX_EVENT) {
	//LOG_MSG("SERIAL_TX_EVENT");
		ByteTransmitted(); // tx timeout
	}
	else if(type==SERIAL_THR_EVENT){
		//LOG_MSG("SERIAL_THR_EVENT");
		ByteTransmitting();
		setEvent(SERIAL_TX_EVENT,bytetime);
	}
	else if (type==SERIAL_RX_EVENT) {
		// check for bytes to be sent to port
		if(CSerial::CanReceiveByte()) {
			if (send_ack) {
				send_ack = 0;
				CSerial::receiveByte('M');
				setEvent(SERIAL_RX_EVENT, bytetime);
			}
			else if (packet_xmit < 3) {
				CSerial::receiveByte(packet[packet_xmit++]);
				if (packet_xmit >= 3 && xmit_another_packet)
					start_packet();
				else
					setEvent(SERIAL_RX_EVENT, bytetime);
			}
		}
		else {
			setEvent(SERIAL_RX_EVENT, bytetime);
		}
	}
}

/*****************************************************************************/
/* updatePortConfig is called when emulated app changes the serial port     **/
/* parameters baudrate, stopbits, number of databits, parity.               **/
/*****************************************************************************/
void CSerialMouse::updatePortConfig(Bit16u divider, Bit8u lcr) {
    (void)divider;//UNUSED
    (void)lcr;//UNUSED
	//LOG_MSG("Serial port at 0x%x: Port params changed: %d Baud", base,dcb.BaudRate);
}

void CSerialMouse::updateMSR() {
}
void CSerialMouse::transmitByte(Bit8u val, bool first) {
    (void)val;//UNUSED
	if(first) setEvent(SERIAL_THR_EVENT, bytetime/10); 
	else setEvent(SERIAL_TX_EVENT, bytetime);
}

/*****************************************************************************/
/* setBreak(val) switches break on or off                                   **/
/*****************************************************************************/

void CSerialMouse::setBreak(bool value) {
    (void)value;//UNUSED
	//LOG_MSG("UART 0x%x: Break toggeled: %d", base, value);
}

void CSerialMouse::onMouseReset() {
	send_ack = 1;
	packet_xmit=0xFF;
	mouse_buttons=0;
	xmit_another_packet=false;
	mouse_delta_x=mouse_delta_y=0;
	setEvent(SERIAL_RX_EVENT, bytetime);
	Mouse_AutoLock(true);
}

/*****************************************************************************/
/* setRTSDTR sets the modem control lines                                   **/
/*****************************************************************************/
void CSerialMouse::setRTSDTR(bool rts, bool dtr) {
	if (rts && dtr && !getRTS() && !getDTR()) {
		/* The serial mouse driver turns on the mouse by bringing up
		 * RTS and DTR. Not just for show, but to give the serial mouse
		 * a power source to work from. Likewise, drivers "reset" the
		 * mouse by bringing down the lines, then bringing them back
		 * up. And most drivers turn off the mouse when not in use by
		 * bringing them back down and leaving them that way.
		 *
		 * We're expected to transmit ASCII character 'M' when first
		 * initialized, so that the driver knows we're a Microsoft
		 * compatible serial mouse attached to a COM port. */
		onMouseReset();
	}

	setRTS(rts);
	setDTR(dtr);
}
void CSerialMouse::setRTS(bool val) {
	if (val && !getRTS() && getDTR()) {
		onMouseReset();
	}

	setCTS(val);
}
void CSerialMouse::setDTR(bool val) {
	if (val && !getDTR() && getRTS()) {
		onMouseReset();
	}

	setDSR(val);
	setRI(val);
	setCD(val);
}
