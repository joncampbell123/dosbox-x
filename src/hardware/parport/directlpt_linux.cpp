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


#include "config.h"
#include "setup.h"

#if C_DIRECTLPT

/* Linux version */
#if defined (LINUX)

#include "parport.h"
#include "directlpt_linux.h"
#include "callback.h"
#include <linux/ppdev.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#include <errno.h>
#include <fcntl.h>
#include <SDL.h>

CDirectLPT::CDirectLPT (Bitu nr, uint8_t initIrq, CommandLine* cmd)
                              :CParallel (cmd, nr, initIrq) {
	InstallationSuccessful = false;
	interruptflag=true; // interrupt disabled

	std::string str;

	if(!cmd->FindStringBegin("realport:",str,false)) {
		LOG_MSG("parallel%d: realport parameter missing.",(int)nr+1);
		return;
	}
	porthandle = open(str.c_str(), O_RDWR );
	if(porthandle == -1) {
		LOG_MSG("parallel%d: Could not open port %s.",(int)nr+1,str.c_str());
		if (errno == 2) LOG_MSG ("The specified port does not exist.");
		else if(errno==EBUSY) LOG_MSG("The specified port is already in use.");
		else if(errno==EACCES) LOG_MSG("You are not allowed to access this port.");
		else LOG_MSG("Errno %d occurred.",errno);
		return;
	}

	if(ioctl( porthandle, PPCLAIM, NULL ) == -1) {
		LOG_MSG("parallel%d: failed to claim port.",(int)nr+1);
		return;
	}
	// TODO check return value
	
	// go for it
	ack_polarity=false;
	initialize();

	InstallationSuccessful = true;
}

CDirectLPT::~CDirectLPT () {
	if(porthandle > 0) close(porthandle);
}

bool CDirectLPT::Putchar(uint8_t val)
{	
	//LOG_MSG("putchar: %x",val);

	// check if printer online and not busy
	// PE and Selected: no printer attached
	uint8_t sr=Read_SR();
	//LOG_MSG("SR: %x",sr);
	if((sr&0x30)==0x30)
	{
		LOG_MSG("putchar: no printer");
		return false;
	}
	// error
	if(sr&0x20)
	{
		LOG_MSG("putchar: paper out");
		return false;
	}
	if((sr&0x08)==0)
	{
		LOG_MSG("putchar: printer error");
		return false;
	}

	Write_PR(val);
	// busy
	Bitu timeout = 10000;
	Bitu time = timeout+SDL_GetTicks();

	while(SDL_GetTicks()<time) {
		// wait for the printer to get ready
		for(int i = 0; i < 500; i++) {
			// do NOT run into callback_idle unless we have to (speeds things up)
			sr=Read_SR();
			if(sr&0x80) break;
		}
		if(sr&0x80) break;
		CALLBACK_Idle();
	}
	if(SDL_GetTicks()>=time) {
		LOG_MSG("putchar: busy timeout");
		return false;
	}
	// strobe data out
	// I hope this creates a sufficient long pulse...
	// (I/O-Bus at 7.15 MHz will give some delay)
	
	for(int i = 0; i < 5; i++) Write_CON(0xd); // strobe on
	Write_CON(0xc); // strobe off

#if PARALLEL_DEBUG
	log_par(dbg_putchar,"putchar  0x%2x",val);
	if(dbg_plainputchar) fprintf(debugfp,"%c",val);
#endif

	return true;
}
Bitu CDirectLPT::Read_PR() {
	Bitu retval;
	ioctl(porthandle, PPRDATA, &retval);
	return retval;
}
Bitu CDirectLPT::Read_COM() {
	Bitu retval;
	ioctl(porthandle, PPRCONTROL, &retval);
	return retval;
}
Bitu CDirectLPT::Read_SR() {
	Bitu retval;
	ioctl(porthandle, PPRSTATUS, &retval);
	return retval;
}

void CDirectLPT::Write_PR(Bitu val){
	ioctl(porthandle, PPWDATA, &val); 
}
void CDirectLPT::Write_CON(Bitu val) {
	ioctl(porthandle, PPWCONTROL, &val); 
}
void CDirectLPT::Write_IOSEL(Bitu val) {
	// switches direction old-style TODO
	if((val==0xAA)||(val==0x55)) LOG_MSG("TODO implement IBM-style direction switch");
}
void CDirectLPT::handleUpperEvent(Bit16u type) { (void)type; }
#endif
#endif
