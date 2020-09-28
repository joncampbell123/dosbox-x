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

// include guard
#ifndef DOSBOX_DIRECTLPT_LINUX_H
#define DOSBOX_DIRECTLPT_LINUX_H

#include "config.h"

#if C_DIRECTLPT
#ifdef LINUX

#define DIRECTLPT_AVAILIBLE
#include "parport.h"


class CDirectLPT : public CParallel {
public:
	CDirectLPT(Bitu nr, uint8_t initIrq, CommandLine* cmd);
	

	~CDirectLPT();

	int porthandle;
	
	bool interruptflag;
	bool InstallationSuccessful;	// check after constructing. If
									// something was wrong, delete it right away.
	bool ack_polarity;

	Bitu Read_PR();
	Bitu Read_COM();
	Bitu Read_SR();

	void Write_PR(Bitu);
	void Write_CON(Bitu);
	void Write_IOSEL(Bitu);
	bool Putchar(uint8_t);

	void handleUpperEvent(Bit16u type);
};

#endif	// WIN32
#endif	// C_DIRECTLPT
#endif	// include guard
