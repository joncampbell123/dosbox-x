/*
 *  Copyright (C) 2002-2013  The DOSBox Team
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

// include guard
#ifndef DOSBOX_DIRECTLPT_WIN32_H
#define DOSBOX_DIRECTLPT_WIN32_H

#include "config.h"
#include "setup.h"

#if C_DIRECTLPT
#ifdef WIN32



#define DIRECTLPT_AVAILIBLE
#include "parport.h"
//#include <windows.h>


class CDirectLPT : public CParallel {
public:
	//HANDLE driverHandle;
	Bit32u realbaseaddress;
	Bit8u originalECPControlReg;
	
	CDirectLPT(
			Bitu nr,
			Bit8u initIrq,
			CommandLine* cmd
            );
	

	~CDirectLPT();
	
	bool interruptflag;
	bool isECP;
	bool InstallationSuccessful;	// check after constructing. If
									// something was wrong, delete it right away.
	bool ack_polarity;

	Bitu Read_PR();
	Bitu Read_COM();
	Bitu Read_SR();

	void Write_PR(Bitu);
	void Write_CON(Bitu);
	void Write_IOSEL(Bitu);
	bool Putchar(Bit8u);

	void handleUpperEvent(Bit16u type);
};

#endif	// WIN32
#endif	// C_DIRECTLPT
#endif	// include guard
