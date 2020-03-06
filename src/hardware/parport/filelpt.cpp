/*
 *  Copyright (C) 2002-2019  The DOSBox Team
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335, USA.
 */


#include "dosbox.h"

#include "parport.h"
#include "filelpt.h"
#include "callback.h"
#include "pic.h"
#include "hardware.h" //OpenCaptureFile
#include <stdio.h>

#include "printer_charmaps.h"

CFileLPT::CFileLPT (Bitu nr, Bit8u initIrq, CommandLine* cmd)
                              :CParallel (cmd, nr,initIrq) {
    bool is_file = false;
	InstallationSuccessful = false;
	fileOpen = false;
	controlreg = 0;
    timeout = ~0u;
	std::string str;
	ack = false;

	// add a formfeed when closing?
	if(cmd->FindStringBegin("addFF",str,false))	addFF = true;
	else addFF = false;

	// add a formfeed when closing?
	if(cmd->FindStringBegin("addLF",str,false))	addLF = true;
	else addLF = false;

	// find the codepage
	unsigned int temp=0;
	codepage_ptr = NULL;
	if(cmd->FindStringBegin("cp:",str,false)) {
		if(sscanf(str.c_str(), "%u",&temp)!=1) {
			LOG_MSG("parallel%d: Invalid codepage parameter.",(int)nr+1);
			return;
		} else {
			Bitu i = 0;
			while(charmap[i].codepage!=0) {
				if(charmap[i].codepage==temp) {
					codepage_ptr = charmap[i].map;
					break;
				}
				i++;
			}
		}
	}
	temp=0;

	if(cmd->FindStringBegin("dev:",str,false)) {
		name = str.c_str();
		filetype = FILE_DEV;
	} else if(cmd->FindStringBegin("file:",str,false)) {
		name = str.c_str();
		filetype = FILE_DEV;
        is_file = true;
	} else if(cmd->FindStringBegin("append:",str,false)) {
		name = str.c_str();
		filetype = FILE_APPEND;
	} else filetype = FILE_CAPTURE;

	if (cmd->FindStringBegin("timeout:",str,false)) {
		if(sscanf(str.c_str(), "%u",&timeout)!=1) {
			LOG_MSG("parallel%d: Invalid timeout parameter.",(int)nr+1);
			return;
		}
	}

	if (timeout == ~0u)
		timeout = is_file ? 0 : 500;

	InstallationSuccessful = true;
}

CFileLPT::~CFileLPT () {
	// close file
	if(fileOpen)
		fclose(file);
	// remove tick handler
	removeEvent(0);
}

bool CFileLPT::OpenFile() {
	switch(filetype) {
	case FILE_DEV:
		file = fopen(name.c_str(),"wb");
        if (file != NULL) setbuf(file,NULL); // disable buffering
		break;
	case FILE_CAPTURE:
		file = OpenCaptureFile("Parallel Port Stream",".prt");
        if (file != NULL) setbuf(file,NULL); // disable buffering
		break;
	case FILE_APPEND:
		file = fopen(name.c_str(),"ab");
        if (file != NULL) setbuf(file,NULL); // disable buffering
		break;
	}

	if(timeout != 0) setEvent(0, (float)(timeout + 1));

	if(file==NULL) {
		LOG_MSG("Parallel %d: Failed to open %s",(int)port_nr+1,name.c_str());
		fileOpen = false;
		return false;
	} else {
		fileOpen = true;
		return true;
	}
}

bool CFileLPT::Putchar(Bit8u val)
{	
#if PARALLEL_DEBUG
	log_par(dbg_putchar,"putchar  0x%2x",val);
	if(dbg_plainputchar) fprintf(debugfp,"%c",val);
#endif
	
	// write to file (or not)
	lastUsedTick = PIC_Ticks;
	if(!fileOpen) if(!OpenFile()) return false;

	if(codepage_ptr!=NULL) {
		Bit16u extchar = codepage_ptr[val];
		if(extchar & 0xFF00) fputc((int)((Bit8u)(extchar >> 8)),file);
		fputc((Bitu)(extchar & 0xFF),file);

	} else fputc((Bitu)val,file);
	if(addLF) {
		if((lastChar == 0x0d) && (val != 0x0a)) {
			fputc(0xa,file);
		}
		lastChar = val;
	}

	return true;
}
Bitu CFileLPT::Read_PR() {
	return datareg;
}
Bitu CFileLPT::Read_COM() {
	return controlreg;
}
Bitu CFileLPT::Read_SR() {
	Bit8u status =0x9f;
	if(!ack) status |= 0x40;
	ack=false;
	return status;
}

void CFileLPT::Write_PR(Bitu val) {
	datareg = (Bit8u)val;
}
void CFileLPT::Write_CON(Bitu val) {
	// init printer if bit 4 is switched on
	// ...
	autofeed = ((val & 0x02)!=0); // autofeed adds 0xa if 0xd is sent

	// data is strobed to the parallel printer on the falling edge of strobe bit
	if((!(val&0x1)) && (controlreg & 0x1)) {
		Putchar(datareg);
		if(autofeed && (datareg==0xd)) Putchar(0xa);
		ack = true;
	}
	controlreg=val&0xF; /* do NOT store bit 5, we do not emulate bidirectional LPT ports, yet */
}
void CFileLPT::Write_IOSEL(Bitu val) {
    (void)val;//UNUSED
	// not needed for file printing functionality
}
void CFileLPT::handleUpperEvent(Bit16u type) {
    (void)type;//UNUSED
	if(fileOpen && timeout != 0) {
		if(lastUsedTick + timeout < PIC_Ticks) {
			if(addFF) {
				fputc(12,file);
			}
			fclose(file);
			lastChar = 0;
			fileOpen=false;
			LOG_MSG("Parallel %d: File closed.",(int)port_nr+1);
		} else {
			// Port has been touched in the meantime, try again later
			float new_delay = (float)((timeout + 1) - (PIC_Ticks - lastUsedTick));
			setEvent(0, new_delay);
		}
	}
}

