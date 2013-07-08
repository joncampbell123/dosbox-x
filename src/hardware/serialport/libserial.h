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

/* $Id: libserial.h,v 1.2 2009-09-26 09:15:19 h-a-l-9000 Exp $ */

typedef struct _COMPORT *COMPORT;

bool SERIAL_open(const char* portname, COMPORT* port);
void SERIAL_close(COMPORT port);
void SERIAL_getErrorString(char* buffer, int length);

#define SERIAL_1STOP 1
#define SERIAL_2STOP 2
#define SERIAL_15STOP 0

// parity: n, o, e, m, s

bool SERIAL_setCommParameters(COMPORT port,
			int baudrate, char parity, int stopbits, int length);

void SERIAL_setDTR(COMPORT port, bool value);
void SERIAL_setRTS(COMPORT port, bool value);
void SERIAL_setBREAK(COMPORT port, bool value);

#define SERIAL_CTS 0x10
#define SERIAL_DSR 0x20
#define SERIAL_RI 0x40
#define SERIAL_CD 0x80

int SERIAL_getmodemstatus(COMPORT port);
bool SERIAL_setmodemcontrol(COMPORT port, int flags);

bool SERIAL_sendchar(COMPORT port, char data);

// 0-7 char data, higher=flags
#define SERIAL_BREAK_ERR 0x10
#define SERIAL_FRAMING_ERR 0x08
#define SERIAL_PARITY_ERR 0x04
#define SERIAL_OVERRUN_ERR 0x02

int SERIAL_getextchar(COMPORT port);
