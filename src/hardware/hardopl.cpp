#include "hardopl.h"

#if HAS_HARDOPL
#if defined LINUX || defined BSD
#include <unistd.h>
#endif
#include "libs/passthroughio/passthroughio.h"
#include "hardware.h"
#include "inout.h"
#include "logging.h"
#include "pic.h"

static int16_t hardopldiff;
static bool isCMS;
static FILE* logfp = NULL;
static bool hwopl_dirty = false;
static IO_ReadHandleObject* hwOPL_ReadHandler[16];
static IO_WriteHandleObject* hwOPL_WriteHandler[16];
static const uint16_t oplports[] = {
	0x0,0x1,0x2,0x3,0x8,0x9,
	0x388,0x389,0x38A,0x38B
};

static void write_hwio(Bitu port,Bitu val,Bitu /*iolen*/) {
	if(port<0x388) port += hardopldiff;
//	LOG_MSG("write port %x",(unsigned)port);
	outportb((uint16_t)port, (uint8_t)val);
}

static Bitu read_hwio(Bitu port,Bitu /*iolen*/) {
	if(port<0x388) port += hardopldiff;
//	LOG_MSG("read port %x",(unsigned)port);
	Bitu retval = inportb((uint16_t)port);
	return retval;
}

// handlers for Gameblaster pass-through

static void write_hwcmsio(Bitu port,Bitu val,Bitu /*iolen*/) {
	if(logfp) fprintf(logfp,"%4.3f w %3x %2x\r\n",(double)PIC_FullIndex(),(unsigned)port,(unsigned)val);
	outportb((uint16_t)port + hardopldiff, (uint8_t)val);
}

static Bitu read_hwcmsio(Bitu port,Bitu /*iolen*/) {
	Bitu retval = inportb((uint16_t)port + hardopldiff);
	if(logfp) fprintf(logfp,"%4.3f r\t\t%3x %2x\r\n",(double)PIC_FullIndex(),(unsigned)port,(unsigned)retval);
	return retval;
}

void HARDOPL_Init(Bitu hardwareaddr, Bitu blasteraddr, bool isCMSp) {
	isCMS = isCMSp;

#if defined BSD || defined LINUX
	// Make sure that privileges have not been dropped with a previous call to
	// dropPrivileges(). You may have to disabled the call in parport.cpp.
	if(geteuid() != (uid_t)0) {
		LOG_MSG("OPL pass-through: Raw I/O requires root privileges. Pass-through I/O disabled.");
		return;
	}
#endif

	if(!(hardwareaddr == 0x210 || hardwareaddr == 0x220 || hardwareaddr == 0x230 ||
		 hardwareaddr == 0x240 || hardwareaddr == 0x250 || hardwareaddr == 0x260 ||
		 hardwareaddr == 0x280)) {
		LOG_MSG("OPL pass-through: Invalid hardware base address (0x%hx). Pass-through I/O disabled.",(unsigned short)hardwareaddr);
		return;
	}

	if(!initPassthroughIO()) {
		LOG_MSG("OPL pass-through: Pass-through I/O disabled."); // initPassthroughIO() itself prints why it fails
		return;
	}

	hardopldiff = (uint16_t)(hardwareaddr - blasteraddr);
	hwopl_dirty = true;

	// map the port
	LOG_MSG("OPL pass-through: Port mappings hardware -> DOSBox-X:");

	if(isCMS) {
		logfp = OpenCaptureFile("Portlog", ".portlog.txt");
		hwOPL_ReadHandler[0] = new IO_ReadHandleObject();
		hwOPL_WriteHandler[0] = new IO_WriteHandleObject();
		hwOPL_ReadHandler[0]->Install(blasteraddr, read_hwcmsio, IO_MB, 16);
		hwOPL_WriteHandler[0]->Install(blasteraddr, write_hwcmsio, IO_MB, 16);
		LOG_MSG("%x-%x -> %x-%x",(unsigned)hardwareaddr,(unsigned)hardwareaddr + 15,
			(unsigned)blasteraddr,(unsigned)blasteraddr + 15);
	} else {
		for(int i = 0; i < 10; i++) {
			hwOPL_ReadHandler[i] = new IO_ReadHandleObject();
			hwOPL_WriteHandler[i] = new IO_WriteHandleObject();
			uint16_t port = oplports[i];
			if(i < 6) port += (uint16_t)blasteraddr;
			hwOPL_ReadHandler[i]->Install(port, read_hwio, IO_MB);
			hwOPL_WriteHandler[i]->Install(port, write_hwio, IO_MB);

			if(i < 6) port += hardopldiff;
			LOG_MSG("%x -> %x",(unsigned)port,i < 6 ? (unsigned)port - hardopldiff : (unsigned)port);
		}
	}
}

void HWOPL_Cleanup() {
	if(hwopl_dirty) {
		if(logfp) fclose(logfp);
		if(isCMS) {
			delete hwOPL_ReadHandler[0];
			delete hwOPL_WriteHandler[0];
		} else {
			for(int i = 0; i < 10; i++) {
				delete hwOPL_ReadHandler[i];
				delete hwOPL_WriteHandler[i];
			}
		}
		hwopl_dirty = false;
	}
}
#endif // HAS_HARDOPL
