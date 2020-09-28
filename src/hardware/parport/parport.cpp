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
/* FIXME: At some point I would like to roll the Disney Sound Source emulation into this code */

#include <string.h>
#include <ctype.h>

#include "dosbox.h"

#include "support.h"
#include "inout.h"
#include "pic.h"
#include "setup.h"
#include "timer.h"
#include "bios.h"					// SetLPTPort(..)
#include "hardware.h"				// OpenCaptureFile

#include "parport.h"
#include "directlpt_win32.h"
#include "directlpt_linux.h"
#include "printer_redir.h"
#include "filelpt.h"
#include "dos_inc.h"

bool device_LPT::Read(uint8_t * data,Bit16u * size) {
    (void)data;//UNUSED
	*size=0;
	LOG(LOG_DOSMISC,LOG_NORMAL)("LPTDEVICE:Read called");
	return true;
}


bool device_LPT::Write(const uint8_t * data,Bit16u * size) {
	for (Bit16u i=0; i<*size; i++)
	{
		if(!pportclass->Putchar(data[i])) return false;
	}
	return true;
}

bool device_LPT::Seek(Bit32u * pos,Bit32u type) {
    (void)type;//UNUSED
	*pos = 0;
	return true;
}

bool device_LPT::Close() {
	return false;
}

Bit16u device_LPT::GetInformation(void) {
	return 0x80A0;
}

const char* lptname[]={"LPT1","LPT2","LPT3"};
device_LPT::device_LPT(uint8_t num, class CParallel* pp) {
	pportclass = pp;
	SetName(lptname[num]);
	this->num = num;
}

device_LPT::~device_LPT() {
	/* clear reference to myself so that CParallel does not attempt to free me or ask DOS to delete me */
	if (pportclass != NULL && pportclass->mydosdevice == this)
		pportclass->mydosdevice = NULL;

//	LOG_MSG("~device_LPT\n");
	//LOG_MSG("del");
}

static void Parallel_EventHandler(Bitu val) {
	Bitu serclassid=val&0x3;
	if(parallelPortObjects[serclassid]!=0)
		parallelPortObjects[serclassid]->handleEvent((Bit16u)(val>>2ul));
}

void CParallel::setEvent(Bit16u type, float duration) {
    PIC_AddEvent(Parallel_EventHandler,duration,((Bitu)type<<2u)|(Bitu)port_nr);
}

void CParallel::removeEvent(Bit16u type) {
    // TODO
	PIC_RemoveSpecificEvents(Parallel_EventHandler,((Bitu)type<<2u)|(Bitu)port_nr);
}

void CParallel::handleEvent(Bit16u type) {
	handleUpperEvent(type);
}

static Bitu PARALLEL_Read (Bitu port, Bitu iolen) {
    (void)iolen;//UNUSED
	for(Bitu i = 0; i < 3; i++) {
		if(parallel_baseaddr[i]==(port&0xfffc) && (parallelPortObjects[i]!=0)) {
			Bitu retval=0xff;
			switch (port & 0x7) {
				case 0:
					retval = parallelPortObjects[i]->Read_PR();
					break;
				case 1:
					retval = parallelPortObjects[i]->Read_SR();
					break;
				case 2:
					retval = parallelPortObjects[i]->Read_COM();
					break;
			}

#if PARALLEL_DEBUG
			const char* const dbgtext[]= {"DAT","STA","COM","???"};
			parallelPortObjects[i]->log_par(parallelPortObjects[i]->dbg_cregs,
				"read  0x%2x from %s.",retval,dbgtext[port&3]);
#endif
			return retval;	
		}
	}
	return 0xff;
}

static void PARALLEL_Write (Bitu port, Bitu val, Bitu) {
	for(Bitu i = 0; i < 3; i++) {
		if(parallel_baseaddr[i]==(port&0xfffc) && parallelPortObjects[i]) {
#if PARALLEL_DEBUG
			const char* const dbgtext[]={"DAT","IOS","CON","???"};
			parallelPortObjects[i]->log_par(parallelPortObjects[i]->dbg_cregs,
				"write 0x%2x to %s.",val,dbgtext[port&3]);
			if(parallelPortObjects[i]->dbg_plaindr &&!(port & 0x3)) {
				fprintf(parallelPortObjects[i]->debugfp,"%c",val);
			}
#endif
			switch (port & 0x3) {
				case 0:
					parallelPortObjects[i]->Write_PR (val);
					return;
				case 1:
					parallelPortObjects[i]->Write_IOSEL (val);
					return;
				case 2:
					parallelPortObjects[i]->Write_CON (val);
					return;
			}
		}
	}
}

//The Functions

#if PARALLEL_DEBUG
#include <stdarg.h>
void CParallel::log_par(bool active, char const* format,...) {
	if(active) {
		// copied from DEBUG_SHOWMSG
		char buf[512];
		buf[0]=0;
		sprintf(buf,"%12.3f ",PIC_FullIndex());
		va_list msg;
		va_start(msg,format);
		vsprintf(buf+strlen(buf),format,msg);
		va_end(msg);
		// Add newline if not present
		Bitu len=strlen(buf);
		if(buf[len-1]!='\n') strcat(buf,"\r\n");
		fputs(buf,debugfp);
	}
}
#endif

// Initialisation
CParallel::CParallel(CommandLine* cmd, Bitu portnr, uint8_t initirq) {
    (void)cmd;//UNUSED
	base = parallel_baseaddr[portnr];
	irq = initirq;
	port_nr = portnr;

#if PARALLEL_DEBUG
	dbg_data	= cmd->FindExist("dbgdata", false);
	dbg_putchar = cmd->FindExist("dbgput", false);
	dbg_cregs	= cmd->FindExist("dbgregs", false);
	dbg_plainputchar = cmd->FindExist("dbgputplain", false);
	dbg_plaindr = cmd->FindExist("dbgdataplain", false);
	
	if(cmd->FindExist("dbgall", false)) {
		dbg_data= 
		dbg_putchar=
		dbg_cregs=true;
		dbg_plainputchar=dbg_plaindr=false;
	}

	if(dbg_data||dbg_putchar||dbg_cregs||dbg_plainputchar||dbg_plaindr)
		debugfp=OpenCaptureFile("parlog",".parlog.txt");
	else debugfp=0;

	if(debugfp == 0) {
		dbg_data= 
		dbg_putchar=dbg_plainputchar=
		dbg_cregs=false;
	} else {
		std::string cleft;
		cmd->GetStringRemain(cleft);

		log_par(true,"Parallel%d: BASE %xh, initstring \"%s\"\r\n\r\n",
			portnr+1,base,cleft.c_str());
	}
#endif
	LOG_MSG("Parallel%d: BASE %xh",(int)portnr+1,(int)base);

	for (Bitu i = 0; i < 3; i++) {
		/* bugfix: do not register I/O write handler for the status port. it's a *status* port.
		 *         also, this is needed for ISA PnP emulation to work properly even if DOSBox
		 *         is emulating more than one parallel port. */
		if (i != 1) WriteHandler[i].Install (i + base, PARALLEL_Write, IO_MB);
		ReadHandler[i].Install (i + base, PARALLEL_Read, IO_MB);
	}

	mydosdevice = NULL;
}

void CParallel::registerDOSDevice() {
	if (mydosdevice == NULL) {
		LOG(LOG_MISC,LOG_DEBUG)("LPT%d: Registering DOS device",(int)port_nr+1);
		mydosdevice = new device_LPT((uint8_t)port_nr, this);
		DOS_AddDevice(mydosdevice);
	}
}

void CParallel::unregisterDOSDevice() {
	if (mydosdevice != NULL) {
		LOG(LOG_MISC,LOG_DEBUG)("LPT%d: Unregistering DOS device",(int)port_nr+1);
		DOS_DelDevice(mydosdevice); // deletes the pointer for us!
		mydosdevice=NULL;
	}
}

CParallel::~CParallel(void) {
	unregisterDOSDevice();
}

uint8_t CParallel::getPrinterStatus()
{
	/*	7      not busy
		6      acknowledge
		5      out of paper
		4      selected
		3      I/O error
		2-1    unused
		0      timeout  */
	uint8_t statusreg=(uint8_t)Read_SR();

	//LOG_MSG("get printer status: %x",statusreg);
	statusreg^=0x48;
	return statusreg&~0x7;
}

#include "callback.h"

void RunIdleTime(Bitu milliseconds)
{
	Bitu time=SDL_GetTicks()+milliseconds;
	while(SDL_GetTicks()<time)
		CALLBACK_Idle();
}

void CParallel::initialize()
{
	Write_IOSEL(0x55); // output mode
	Write_CON(0x08); // init low
	Write_PR(0);
	RunIdleTime(10);
	Write_CON(0x0c); // init high
	RunIdleTime(500);
	//LOG_MSG("printer init");
}



CParallel* parallelPortObjects[3]={NULL,NULL,NULL};

bool DISNEY_HasInit();
Bitu DISNEY_BasePort();
bool DISNEY_ShouldInit();
void DISNEY_Init(unsigned int base_port);

Bitu bios_post_parport_count() {
	Bitu count = 0;
	unsigned int i;

	for (i=0;i < 3;i++) {
		if (parallelPortObjects[i] != NULL)
			count++;
		else if (DISNEY_HasInit() && parallel_baseaddr[i] == DISNEY_BasePort())
			count++;
	}

	return count;
}

/* at BIOS POST stage, write parallel ports to bios data area */
void BIOS_Post_register_parports() {
	unsigned int i;

	for (i=0;i < 3;i++) {
		if (parallelPortObjects[i] != NULL)
			BIOS_SetLPTPort(i,(Bit16u)parallelPortObjects[i]->base);
		else if (DISNEY_HasInit() && parallel_baseaddr[i] == (Bit16u)DISNEY_BasePort())
			BIOS_SetLPTPort(i,(Bit16u)DISNEY_BasePort());
	}
}
	
class PARPORTS:public Module_base {
public:
	
	PARPORTS (Section * configuration):Module_base (configuration) {

        // TODO: PC-98 does have one parallel port, if at all
        if (IS_PC98_ARCH) return;

#if C_PRINTER
        // we can only have one printer redirection, hence the variable
        bool printer_used = false;
#endif

		// default ports & interrupts
		uint8_t defaultirq[] = { 7, 5, 12};
		Section_prop *section = static_cast <Section_prop*>(configuration);
		
		char pname[]="parallelx";
		// iterate through all 3 lpt ports
		for (Bitu i = 0; i < 3; i++) {
			pname[8] = '1' + (char)i;
			CommandLine cmd(0,section->Get_string(pname));

			std::string str;
			cmd.FindCommand(1,str);
#if C_DIRECTLPT			
			if(str=="reallpt") {
				CDirectLPT* cdlpt= new CDirectLPT(i, defaultirq[i],&cmd);
				if(cdlpt->InstallationSuccessful)
					parallelPortObjects[i]=cdlpt;
				else {
					delete cdlpt;
					parallelPortObjects[i]=0;
				}
			}
			else
#endif
			if(!str.compare("file")) {
				CFileLPT* cflpt= new CFileLPT(i, defaultirq[i], &cmd);
				if(cflpt->InstallationSuccessful)
					parallelPortObjects[i]=cflpt;
				else {
					delete cflpt;
					parallelPortObjects[i]=0;
				}
			}
			else 
#if C_PRINTER
            // allow printer redirection on a single port
            if (str == "printer" && !printer_used)
            {
                CPrinterRedir* cprd = new CPrinterRedir(i, defaultirq[i], &cmd);
                if (cprd->InstallationSuccessful)
                {
                    parallelPortObjects[i] = cprd;
                    printer_used = true;
                }
                else
                {
                    LOG_MSG("Error: printer is not enabled.");
                    delete cprd;
                    parallelPortObjects[i] = 0;
                }
            }
            else
#endif				
            if(str=="disabled") {
				parallelPortObjects[i] = 0;
			} else if (str == "disney") {
				if (!DISNEY_HasInit()) {
					LOG_MSG("LPT%d: User explicitly assigned Disney Sound Source to this port",(int)i+1);
					DISNEY_Init(parallel_baseaddr[i]);
				}
				else {
					LOG_MSG("LPT%d: Disney Sound Source already initialized on a port, cannot init again",(int)i+1);
				}
			} else {
				LOG_MSG ("Invalid type for LPT%d.",(int)i + 1);
				parallelPortObjects[i] = 0;
			}
		} // for lpt 1-3
	}

	~PARPORTS () {
//		LOG_MSG("Parports destructor\n");
		for (Bitu i = 0; i < 3; i++) {
			if (parallelPortObjects[i]) {
				delete parallelPortObjects[i];
				parallelPortObjects[i] = 0;
			}
		}
	}
};

static PARPORTS *testParallelPortsBaseclass = NULL;

void PARALLEL_Destroy (Section * sec) {
    (void)sec;//UNUSED
	if (testParallelPortsBaseclass != NULL) {
		delete testParallelPortsBaseclass;
		testParallelPortsBaseclass = NULL;
	}
}

void PARALLEL_OnPowerOn (Section * sec) {
    (void)sec;//UNUSED
	LOG(LOG_MISC,LOG_DEBUG)("Reinitializing parallel port emulation");

	if (testParallelPortsBaseclass) delete testParallelPortsBaseclass;
	testParallelPortsBaseclass = new PARPORTS (control->GetSection("parallel"));

	/* Mainline DOSBox 0.74 compatible support for "disney=true" setting.
	 * But, don't allocate the Disney Sound Source if LPT1 is already taken. */
	if (!DISNEY_HasInit() && DISNEY_ShouldInit() && parallelPortObjects[0] == NULL) {
		LOG_MSG("LPT: LPT1 not taken, and dosbox.conf says to emulate Disney Sound Source");
		DISNEY_Init(parallel_baseaddr[0]);
	}
}

void PARALLEL_OnDOSKernelInit (Section * sec) {
    (void)sec;//UNUSED
	unsigned int i;

	LOG(LOG_MISC,LOG_DEBUG)("DOS kernel initializing, creating LPTx devices");

	for (i=0;i < 3;i++) {
		if (parallelPortObjects[i] != NULL)
			parallelPortObjects[i]->registerDOSDevice();
	}
}

void PARALLEL_OnDOSKernelExit (Section * sec) {
    (void)sec;//UNUSED
	unsigned int i;

	for (i=0;i < 3;i++) {
		if (parallelPortObjects[i] != NULL)
			parallelPortObjects[i]->unregisterDOSDevice();
	}
}

void PARALLEL_OnReset (Section * sec) {
    (void)sec;//UNUSED
	unsigned int i;

	// FIXME: Unregister/destroy the DOS devices, but consider that the DOS kernel at reset is gone.
	for (i=0;i < 3;i++) {
		if (parallelPortObjects[i] != NULL)
			parallelPortObjects[i]->unregisterDOSDevice();
	}
}

void PARALLEL_Init () {
	LOG(LOG_MISC,LOG_DEBUG)("Initializing parallel port emulation");

	AddExitFunction(AddExitFunctionFuncPair(PARALLEL_Destroy),true);

    if (!IS_PC98_ARCH) {
        AddVMEventFunction(VM_EVENT_RESET,AddVMEventFunctionFuncPair(PARALLEL_OnReset));
        AddVMEventFunction(VM_EVENT_POWERON,AddVMEventFunctionFuncPair(PARALLEL_OnPowerOn));
        AddVMEventFunction(VM_EVENT_DOS_EXIT_BEGIN,AddVMEventFunctionFuncPair(PARALLEL_OnDOSKernelExit));
        AddVMEventFunction(VM_EVENT_DOS_INIT_KERNEL_READY,AddVMEventFunctionFuncPair(PARALLEL_OnDOSKernelInit));
    }
}

