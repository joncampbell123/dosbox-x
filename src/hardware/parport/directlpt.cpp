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

#include "directlpt.h" // for HAS_CDIRECTLPT
#if HAS_CDIRECTLPT
#include <SDL.h>
#ifdef LINUX
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <linux/parport.h>
#include <linux/ppdev.h>
#include <sys/ioctl.h>
#endif
#include "libs/passthroughio/passthroughio.h"
#include "callback.h"
#include "logging.h"

#define PPORT_CON_IRQ 0x10
#ifdef LINUX
#define PPORT_CON_RDIR 0x20
#define PPORT_CON_EMU_MASK (PPORT_CON_RDIR | PPORT_CON_IRQ)
#define PPORT_CON_MASK 0x0f
#endif

CDirectLPT::CDirectLPT(Bitu nr, uint8_t initIrq, CommandLine* cmd)
	: CParallel (cmd, nr, initIrq),
	InstallationSuccessful(false), realbaseaddress(0x378), ecraddress(0), originalecr(0), isecp(false), controlreg(0xc0)
#ifdef LINUX
	, useppdev(true), porthandle(0)
#endif
{
	std::string str;
#if defined __i386__ || defined __x86_64__ || defined _M_IX86 || defined _M_X64
#if defined BSD || defined LINUX
	// Raw I/O is only possible if we have root privileges.
	if(geteuid() == (uid_t)0)
#endif
	{
		if(cmd->FindStringBegin("realbase:", str, false)) {
			if(sscanf(str.c_str(), "%hx", &realbaseaddress) != 1) {
				LOG_MSG("parallel%d: Invalid realbase parameter (%s). Pass-through I/O disabled.", (int)nr + 1, str.c_str());
				return;
			}
		}
#ifdef LINUX
		if(!cmd->FindStringBegin("realport:", str, false)) {
			useppdev = false;
#endif
			if(!initPassthroughIO()) {
				LOG_MSG("Pass-through I/O disabled.");
				return;
			}
#ifdef LINUX
		}
#endif
	}
#if defined BSD || defined LINUX
	else {
		if(cmd->FindStringBegin("realbase:", str, false)) {
			LOG_MSG("parallel%d: realbase (raw I/O) requires root privileges. Pass-through I/O disabled.", (int)nr + 1);
			return;
		}
		else if(cmd->FindStringBegin("ecpbase:", str, false)) {
			LOG_MSG("parallel%d: ecpbase (raw I/O) requires root privileges. Pass-through I/O disabled.", (int)nr + 1);
			return;
		}
	}
#endif
#endif // x86{_64}

#ifdef LINUX
	if(useppdev) {
		if(!cmd->FindStringBegin("realport:", str, false)) {
			LOG_MSG("parallel%d: realport parameter missing. Pass-through I/O disabled.", (int)nr + 1);
			return;
		}
		porthandle = open(str.c_str(), O_RDWR);
		if(porthandle == -1) {
			LOG_MSG("parallel%d: Could not open parallel port %s. Pass-through I/O disabled.", (int)nr + 1, str.c_str());
			if(errno == ENOENT) LOG_MSG ("parallel%d: The specified parallel port does not exist.", (int)nr + 1);
			else if(errno == EBUSY) LOG_MSG("parallel%d: The specified parallel port is already in use.", (int)nr + 1);
			else if(errno == EACCES) LOG_MSG("parallel%d: You are not allowed to access this parallel port.", (int)nr + 1);
			else LOG_MSG("parallel%d: Error (%d): %s", (int)nr + 1, errno, strerror(errno));
			return;
		}

		if(ioctl(porthandle, PPCLAIM, NULL) == -1) {
			LOG_MSG("parallel%d: Failed to claim parallel port. Pass-through I/O disabled.", (int)nr + 1);
			return;
		}

		// set the parallel port to bidirectional mode
		int portmode = IEEE1284_MODE_BYTE;
		if(ioctl(porthandle, PPSETMODE, &portmode) == -1)
			LOG_MSG("parallel%d: Could not set bidirectional mode.", (int)nr + 1); // but pass-through I/O has been enabled

		int datadir = 0; // 0 = output, !0 = input
		ioctl(porthandle, PPDATADIR, &datadir);

		LOG_MSG("parallel%d: Pass-through I/O enabled.", (int)nr + 1);
	} else
#endif
	{
		// make sure the user doesn't touch critical I/O-ports
		if(realbaseaddress < 0x100 || (realbaseaddress & 0x03) ||   // sanity + mainboard res.
			(realbaseaddress >= 0x1f0 && realbaseaddress <= 0x1f7) || // prim. HDD controller
			(realbaseaddress >= 0x170 && realbaseaddress <= 0x177) || // sec. HDD controller
			(realbaseaddress >= 0x3f0 && realbaseaddress <= 0x3f7) || // floppy + prim. HDD
			(realbaseaddress >= 0x370 && realbaseaddress <= 0x377)) { // sec. hdd
			LOG_MSG("parallel%d: Invalid base address (0x%hx). Pass-through I/O disabled.", (int)nr + 1, realbaseaddress);
			return;
		}

		/*
		  The parameter name "ecpbase" is a misnomer, because what is meant is
		  actually the ECP Extended Control register, which is located at
		  address ECP base (first ECP configuration register) + 2.
		*/
		if(cmd->FindStringBegin("ecpbase:", str, false)) {
			if(sscanf(str.c_str(), "%hx", &ecraddress) != 1) {
				LOG_MSG("parallel%d: Invalid ecpbase parameter (%s). Pass-through I/O disabled.", (int)nr + 1, str.c_str());
				return;
			}
			isecp = true;
		} else {
			// 0x3bc cannot be an ECP port
			isecp = (realbaseaddress & 0x07) == 0;
			if(isecp) ecraddress = realbaseaddress + 0x402;
		}

		if(isecp) {
			// check if there is an ECP port (try to set bidir)
			originalecr = inportb(ecraddress);
			uint8_t ecrvalue = originalecr & 0x1f;
			ecrvalue |= 0x20;

			outportb(ecraddress, ecrvalue);
			if(inportb(ecraddress) != ecrvalue) {
				// this is not an ECP port
				outportb(ecraddress, originalecr);
				isecp = false;
			}
		}

#if 0 // do not perform parallel port I/O that may confuse a connected (non-printing) device
		// check if there is a parallel port at all: the autofeed bit
		uint8_t controlreg = inportb(realbaseaddress + 2);
		outportb(realbaseaddress + 2, controlreg | 0x02);
		if(!(inportb(realbaseaddress + 2) & 0x02)) {
			LOG_MSG("parallel%d: No parallel port detected at 0x%x. Pass-through I/O disabled.", (int)nr + 1, realbaseaddress);
			return;
		}
		outportb(realbaseaddress + 2, controlreg & ~0x02);
		if(inportb(realbaseaddress + 2) & 0x02) {
			LOG_MSG("parallel%d: No parallel port detected at 0x%x. Pass-through I/O disabled.", (int)nr + 1, realbaseaddress);
			return;
		}
		outportb(realbaseaddress + 2, controlreg);
#endif
		// the reference to ecraddress below regardless of isecp is intentional (for troubleshooting)
		LOG_MSG("parallel%d: The parallel port at 0x%x (ECR=0x%x) was %sdetected as supporting ECP. Pass-through I/O enabled.",
			(int)nr + 1, realbaseaddress, ecraddress, isecp ? "" : "not ");
	}

#if 0 // do not perform parallel port I/O that may confuse a connected (non-printing) device
	// go for it
	initialize();
#else
#ifdef LINUX
	if(!useppdev)
#endif
	{
		outportb(realbaseaddress + 2, 0x04);
		// On some chipsets EPP Time-out will even block SPP cycles.
		if((inportb(realbaseaddress + 1) & 0x01) != 0) {
			// some reset by reading twice, others by writing 1, others by writing 0
			uint8_t status = inportb(realbaseaddress + 1);
			outportb(realbaseaddress + 1, status | 0x01);
			outportb(realbaseaddress + 1, status & 0xfe);
		}
	}
#endif

	InstallationSuccessful = true;
}

CDirectLPT::~CDirectLPT() {
	if(InstallationSuccessful) {
#ifdef LINUX
		if(useppdev) {
			if(porthandle > 0) {
				ioctl(porthandle, PPRELEASE);
				close(porthandle);
			}
		} else
#endif
			if(isecp) outportb(ecraddress, originalecr);
	}
}

bool CDirectLPT::Putchar(uint8_t val) {
	// check if printer online and not busy
	// PE and Selected: no printer attached
	uint8_t sr = (uint8_t)Read_SR();
	if((sr & 0x30) == 0x30) {
		LOG_MSG("CDirectLPT::Putchar(): no printer");
		return false;
	}
	// error
	if(sr & 0x20) {
		LOG_MSG("CDirectLPT::Putchar(): paper out");
		return false;
	}
	if((sr & 0x08) == 0) {
		LOG_MSG("CDirectLPT::Putchar(): printer error");
		return false;
	}

	Write_PR(val);
	// busy
	Bitu timeout = 10000;
	Bitu time = timeout + SDL_GetTicks();

	while(SDL_GetTicks() < time) {
		// wait for the printer to get ready
		for(int i = 0; i < 500; i++) {
			// do NOT run into callback_idle unless we have to (speeds things up)
			sr = (uint8_t)Read_SR();
			if(sr & 0x80) break;
		}
		if(sr & 0x80) break;
		CALLBACK_Idle();
	}
	if(SDL_GetTicks() >= time) {
		LOG_MSG("CDirectLPT::Putchar(): busy timeout");
		return false;
	}
	// strobe data out
	// I hope this creates a sufficient long pulse...
	// (I/O-Bus at 7.15 MHz will give some delay)

	for(int i = 0; i < 5; i++) Write_CON(0x0d); // strobe on
	Write_CON(0x0c); // strobe off

#if PARALLEL_DEBUG
	log_par(dbg_putchar, "CDirectLPT::Putchar(): 0x%02x", val);
	if(dbg_plainputchar) fputc(val, debugfp);
#endif

	return true;
}

Bitu CDirectLPT::Read_PR() {
	Bitu retval;
#ifdef LINUX
	if(useppdev)
		ioctl(porthandle, PPRDATA, &retval);
	else
#endif
		retval = inportb(realbaseaddress);
	return retval;
}

Bitu CDirectLPT::Read_COM() {
	Bitu retval;
#ifdef LINUX
	if(useppdev) {
		ioctl(porthandle, PPRCONTROL, &retval);
		retval |= controlreg & ~PPORT_CON_MASK;
	} else
#endif
	{
		retval = inportb(realbaseaddress + 2);
		retval |= controlreg & PPORT_CON_IRQ;
	}
	return retval;
}

Bitu CDirectLPT::Read_SR() {
	Bitu retval;
#ifdef LINUX
	if(useppdev)
		ioctl(porthandle, PPRSTATUS, &retval);
	else
#endif
		retval = inportb(realbaseaddress + 1);
	return retval;
}

void CDirectLPT::Write_PR(Bitu val) {
#ifdef LINUX
	if(useppdev)
		ioctl(porthandle, PPWDATA, &val);
	else
#endif
		outportb(realbaseaddress, (uint8_t)val);
}

void CDirectLPT::Write_CON(Bitu val) {
#ifdef LINUX
	if(useppdev) {
		if((controlreg ^ val) & PPORT_CON_RDIR) {
			// direction has to change; 0 = output, !0 = input
			int datadir = val & PPORT_CON_RDIR;
			ioctl(porthandle, PPDATADIR, &datadir);
		}

		controlreg &= ~(PPORT_CON_EMU_MASK | PPORT_CON_MASK);
		controlreg |= (uint8_t)val;
		// only consider the bits that actually change signals
		val &= PPORT_CON_MASK;
		ioctl(porthandle, PPWCONTROL, &val);
	} else
#endif
	{
		controlreg = (uint8_t)val;
		outportb(realbaseaddress + 2, val & ~PPORT_CON_IRQ); // never enable IRQ
	}
}

void CDirectLPT::Write_IOSEL(Bitu val) {
#ifdef LINUX
	if(useppdev) {
		if(val == 0xaa || val == 0x55) LOG_MSG("TODO implement IBM-style direction switch.");
	} else
#endif
		outportb(realbaseaddress + 1, (uint8_t)val);
}

void CDirectLPT::handleUpperEvent(uint16_t type) { (void)type; }

#endif // HAS_CDIRECTLPT
