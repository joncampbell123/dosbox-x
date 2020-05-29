/*
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

#if C_PRINTER

#include "parport.h"
//#include "callback.h"
#include "printer_redir.h"

// Purpose of this is to pass LPT register access to the virtual printer 

CPrinterRedir::CPrinterRedir(Bitu nr, Bit8u initIrq, CommandLine* cmd)
                              :CParallel (cmd, nr, initIrq) {
	InstallationSuccessful = PRINTER_isInited();
}

CPrinterRedir::~CPrinterRedir () {
	// close file
}

bool CPrinterRedir::Putchar(Bit8u val)
{	
	Write_CON(0xD4);
	// strobe data out
	Write_PR(val);
	Write_CON(0xD5); // strobe pulse
	Write_CON(0xD4); // strobe off
	Read_SR();		 // clear ack

#if PARALLEL_DEBUG
	log_par(dbg_putchar,"putchar  0x%2x",val);
	if(dbg_plainputchar) fprintf(debugfp,"%c",val);
#endif

	return true;
}
Bitu CPrinterRedir::Read_PR() {
	return PRINTER_readdata(0,1);
}
Bitu CPrinterRedir::Read_COM() {
	return PRINTER_readcontrol(0,1);
}
Bitu CPrinterRedir::Read_SR() {
	return PRINTER_readstatus(0,1);
}
void CPrinterRedir::Write_PR(Bitu val) {
	PRINTER_writedata(0,val,1);
}
void CPrinterRedir::Write_CON(Bitu val) {
	PRINTER_writecontrol(0,val,1);
}
void CPrinterRedir::Write_IOSEL(Bitu val) {
    (void)val; // UNUSED
	// nothing
}
void CPrinterRedir::handleUpperEvent(Bit16u type) {
    (void)type; // UNUSED
}
#endif // C_PRINTER
