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

// include guard
#pragma once

#include "config.h"

#if HAS_CDIRECTLPT
#include "parport.h"

class CDirectLPT : public CParallel {
public:
	CDirectLPT(Bitu nr, uint8_t initIrq, CommandLine* cmd);
	virtual ~CDirectLPT();

	bool InstallationSuccessful;        // check after constructing. If
private:                                // something was wrong, delete it right away.
	bool Putchar(uint8_t) override;

	Bitu Read_PR() override;
	Bitu Read_COM() override;
	Bitu Read_SR() override;

	void Write_PR(Bitu) override;
	void Write_CON(Bitu) override;
	void Write_IOSEL(Bitu) override;

	void handleUpperEvent(uint16_t type) override;

	uint16_t realbaseaddress;
	uint16_t ecraddress;
	uint8_t originalecr;
	bool isecp;
	uint8_t controlreg;
#ifdef LINUX
	bool useppdev;
	int porthandle;
#endif
};

#endif // HAS_CDIRECTLPT
