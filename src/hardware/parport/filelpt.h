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
#ifndef DOSBOX_FILELPT_H
#define DOSBOX_FILELPT_H

#include "dosbox.h"
#include "parport.h"

typedef enum { FILE_DEV, FILE_CAPTURE, FILE_APPEND } DFTYPE;

class CFileLPT : public CParallel {
public:
	CFileLPT (Bitu nr, uint8_t initIrq, CommandLine* cmd, bool sq = false);

	~CFileLPT();
	
	bool InstallationSuccessful;	// check after constructing. If
									// something was wrong, delete it right away.
	
	bool fileOpen;
	DFTYPE filetype = (DFTYPE)0;			// which mode to operate in (capture,fileappend,device)
	FILE* file = NULL;
	std::string name;			// name of the thing to open
	std::string action1, action2, action3, action4; // open with a program or batch script
	bool addFF;					// add a formfeed character before closing the file/device
	bool addLF;					// if set, add line feed after carriage return if not used by app
    bool squote;

	uint8_t lastChar = 0;				// used to save the previous character to decide wether to add LF
	const uint16_t* codepage_ptr; // pointer to the translation codepage if not null

	bool OpenFile();
	
	bool ack_polarity = false;

	Bitu Read_PR();
	Bitu Read_COM();
	Bitu Read_SR();

	uint8_t datareg = 0;
	uint8_t controlreg;

	void Write_PR(Bitu);
	void Write_CON(Bitu);
	void Write_IOSEL(Bitu);
	bool Putchar(uint8_t);

	bool autofeed = false;
	bool ack;
	unsigned int timeout = 0;
	Bitu lastUsedTick = 0;
	void doAction();
	virtual void handleUpperEvent(uint16_t type);
};

#endif	// include guard
