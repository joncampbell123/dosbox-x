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


#include "dosbox.h"

#if C_DIRECTLPT

#if defined(_MSC_VER)
# pragma warning(disable:4244) /* const fmath::local::uint64_t to double possible loss of data */
#endif

/* Windows version */
#if defined (WIN32)

#include "parport.h"
//#include "../../libs/porttalk/porttalk.h"
#include "directlpt_win32.h"
#include "callback.h"
#include <SDL.h> 
#include "setup.h"

//********************************************************
//new funct prototypes for parallel port

/* prototype (function typedef) for DLL function Inp32: */

     typedef short (_stdcall *inpfuncPtr)(short portaddr);
     typedef void (_stdcall *oupfuncPtr)(short portaddr, short datum);

/* After successful initialization, these 2 variables
   will contain function pointers.
 */
     inpfuncPtr inp32fp;
     oupfuncPtr oup32fp;


/* Wrapper functions for the function pointers
    - call these functions to perform I/O.
 */
     short Inp32(short portaddr)
     {
         return inp32fp(portaddr);
     }

     void Out32(short portaddr, short datum)
     {
         oup32fp(portaddr, datum);
     }

//********************************************************



CDirectLPT::CDirectLPT (Bitu nr, uint8_t initIrq, CommandLine* cmd)
                              :CParallel (cmd, nr, initIrq) {
     HINSTANCE hLib;

     /* Load the library for Windows 32/64-bit driver */
     hLib = LoadLibrary("inpout32.dll");

#if defined (_M_AMD64)
     if (hLib == NULL) hLib = LoadLibrary("inpoutx64.dll");
#endif
     if (hLib == NULL) {
          LOG_MSG("LoadLibrary Failed.\n");
          return ;
     }

     /* get the address of the function */

     inp32fp = (inpfuncPtr) GetProcAddress(hLib, "Inp32");

     if (inp32fp == NULL) {
          LOG_MSG("GetProcAddress for Inp32 Failed.\n");
          FreeLibrary(hLib);
          return ;
     }


     oup32fp = (oupfuncPtr) GetProcAddress(hLib, "Out32");

     if (oup32fp == NULL) {
          LOG_MSG("GetProcAddress for Oup32 Failed.\n");
          FreeLibrary(hLib);
          return ;
     }

	
	// LOG_MSG("installing ok");

	InstallationSuccessful = false;
	interruptflag=true; // interrupt disabled
	realbaseaddress = 0x378;

	std::string str;
	if(cmd->FindStringBegin("realbase:",str,false)) {
		if(sscanf(str.c_str(), "%x",&realbaseaddress)!=1) {
			LOG_MSG("parallel%d: Invalid realbase parameter.",nr);
            FreeLibrary(hLib);
			return;
		} 
	}

	if(realbaseaddress>=0x10000) {
		LOG_MSG("Error: Invalid base address.");
        FreeLibrary(hLib);
		return;
	}
	/*
	if(!initPorttalk()) {
		LOG_MSG("Error: could not open PortTalk driver.");
		return;
	}*/
/*
	//if(!IsInpOutDriverOpen()) {
	if(err=Opendriver(IsXP64Bit())) {
		LOG_MSG("Error: could not open new driver, err %d.",err);
		return;
	}
	*/

	// make sure the user doesn't touch critical I/O-ports
	if((realbaseaddress<0x100) || (realbaseaddress&0x3) ||		// sanity + mainboard res.
		((realbaseaddress>=0x1f0)&&(realbaseaddress<=0x1f7)) ||	// prim. HDD controller
		((realbaseaddress>=0x170)&&(realbaseaddress<=0x177)) ||	// sek. HDD controller
		((realbaseaddress>=0x3f0)&&(realbaseaddress<=0x3f7)) ||	// floppy + prim. HDD
		((realbaseaddress>=0x370)&&(realbaseaddress<=0x377))) {	// sek. hdd
		LOG_MSG("Parallel Port: Invalid base address.");
        FreeLibrary(hLib);
		return;
	}
	/*	
	if(realbase!=0x378 && realbase!=0x278 && realbase != 0x3bc)
	{
		// TODO PCI ECP ports can be on funny I/O-port-addresses
		LOG_MSG("Parallel Port: Invalid base address.");
		return;
	}*/
	uint32_t ecpbase = 0;
	if(cmd->FindStringBegin("ecpbase:",str,false)) {
		if(sscanf(str.c_str(), "%x",&ecpbase)!=1) {
			LOG_MSG("parallel%d: Invalid realbase parameter.",nr);
            FreeLibrary(hLib);
			return;
		}
		isECP=true;
	} else {
		// 0x3bc cannot be a ECP port
		isECP= ((realbaseaddress&0x7)==0);
		if (isECP) ecpbase = realbaseaddress+0x402;
	}
	/*
	// add the standard parallel port registers
	addIOPermission((uint16_t)realbaseaddress);
	addIOPermission((uint16_t)realbaseaddress+1);
	addIOPermission((uint16_t)realbaseaddress+2);
	
	// if it could be a ECP port: make the extended control register accessible
	if(isECP)addIOPermission((uint16_t)ecpbase);
	
	// bail out if porttalk fails
	if(!setPermissionList())
	{
		LOG_MSG("ERROR SET PERMLIST");
		return;
	}
	if(isECP) {
		// check if there is a ECP port (try to set bidir)
		originalECPControlReg = inportb(ecpbase);
		uint8_t new_bidir = originalECPControlReg&0x1F;
		new_bidir|=0x20;

		outportb(ecpbase,new_bidir);
		if(inportb(ecpbase)!=new_bidir) {
			// this is not a ECP port
			outportb(ecpbase,originalECPControlReg);
			isECP=false;
		}
	}
	*/

	// check if there is a parallel port at all: the autofeed bit
	/*
	uint8_t controlreg=inportb(realbaseaddress+2);
	outportb(realbaseaddress+2,controlreg|2);
	if(!(inportb(realbaseaddress+2)&0x2))
	{
		LOG_MSG("No parallel port detected at 0x%x!",realbaseaddress);
		// cannot remember 1
		return;
	}
	*/
	//realbaseaddress=0x378;
	uint8_t controlreg=Inp32(realbaseaddress+2);
	Out32(realbaseaddress+2,controlreg|2);
	if(!(Inp32(realbaseaddress+2)&0x2))
	{
		LOG_MSG("No parallel port detected at 0x%x!",realbaseaddress);
		// cannot remember 1
        FreeLibrary(hLib);
		return;
	}
	
	// check 0
	/*
	outportb(realbaseaddress+2,controlreg & ~2);
	if(inportb(realbaseaddress+2)&0x2)
	{
		LOG_MSG("No parallel port detected at 0x%x!",realbaseaddress);
		// cannot remember 0
		return;
	}
	outportb(realbaseaddress+2,controlreg);
	*/
	Out32(realbaseaddress+2,controlreg & ~2);
	if(Inp32(realbaseaddress+2)&0x2)
	{
		LOG_MSG("No parallel port detected at 0x%x!",realbaseaddress);
		// cannot remember 0
        FreeLibrary(hLib);
		return;
	}
	Out32(realbaseaddress+2,controlreg);


	if(isECP) LOG_MSG("The port at 0x%x was detected as ECP port.",realbaseaddress);
	else LOG_MSG("The port at 0x%x is not a ECP port.",realbaseaddress);
	
	/*
	// bidir test
	outportb(realbase+2,0x20);
	for(int i = 0; i < 256; i++) {
		outportb(realbase, i);
		if(inportb(realbase)!=i) LOG_MSG("NOT %x", i);
	}
	*/

	// go for it
	ack_polarity=false;
	initialize();

	InstallationSuccessful = true;
	//LOG_MSG("InstSuccess");
}

CDirectLPT::~CDirectLPT () {
	if(InstallationSuccessful && isECP)
		//outportb(realbaseaddress+0x402,originalECPControlReg);
		Out32(realbaseaddress+0x402,originalECPControlReg);
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
	//return inportb(realbaseaddress);
	return Inp32(realbaseaddress);
}
Bitu CDirectLPT::Read_COM() {
	//uint8_t retval=inportb(realbaseaddress+2);
	uint8_t retval=0;
	retval=Inp32(realbaseaddress+2);
	if(!interruptflag)// interrupt activated
	retval&=~0x10;
	return retval;
}
Bitu CDirectLPT::Read_SR() {
	//return inportb(realbaseaddress+1);
	return Inp32(realbaseaddress+1);
}

void CDirectLPT::Write_PR(Bitu val) {
	//LOG_MSG("%c,%x",(uint8_t)val,val);
	//outportb(realbaseaddress,val);
	Out32(realbaseaddress,val);
}
void CDirectLPT::Write_CON(Bitu val) {
	//do not activate interrupt
	interruptflag = (val&0x10)!=0;
	//outportb(realbaseaddress+2,val|0x10);
	Out32(realbaseaddress+2,val|0x10);
}
void CDirectLPT::Write_IOSEL(Bitu val) {
	//outportb(realbaseaddress+1,val);
	Out32(realbaseaddress+1,val);
}

void CDirectLPT::handleUpperEvent(uint16_t type) { (void)type; }


#endif
#endif
