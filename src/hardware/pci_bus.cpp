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


#include <string.h>
#include "dosbox.h"
#include "inout.h"
#include "paging.h"
#include "mem.h"
#include "pci_bus.h"
#include "setup.h"
#include "debug.h"
#include "callback.h"
#include "regs.h"
#include "../ints/int10.h"
#include "voodoo.h"
#include "control.h"

bool pcibus_enable = false;
bool log_pci = false;

bool has_pcibus_enable(void) {
    return pcibus_enable;
}

static uint32_t pci_caddress=0;			// current PCI addressing

static PCI_Device* pci_devices[PCI_MAX_PCIBUSSES][PCI_MAX_PCIDEVICES]={{NULL}};		// registered PCI devices

// PCI address
// 31    - set for a PCI access
// 30-24 - 0
// 23-16 - bus number			(0x00ff0000)
// 15-11 - device number (slot)	(0x0000f800)
// 10- 8 - subfunction number	(0x00000700)
//  7- 2 - config register #	(0x000000fc)

static void write_pci_addr(Bitu port,Bitu val,Bitu iolen) {
    (void)iolen;//UNUSED
    (void)port;//UNUSED
    if (log_pci) LOG(LOG_PCI,LOG_DEBUG)("Write PCI address :=%x",(int)val);
	pci_caddress=(uint32_t)val;
}

static void write_pci(Bitu port,Bitu val,Bitu iolen) {
	if (log_pci) LOG(LOG_PCI,LOG_DEBUG)("Write PCI data port %x :=%x (len %d)",(int)port,(int)val,(int)iolen);

	if (pci_caddress & 0x80000000) {
		uint8_t busnum = (uint8_t)((pci_caddress >> 16) & 0xff);
		uint8_t devnum = (uint8_t)((pci_caddress >> 11) & 0x1f);
		uint8_t fctnum = (uint8_t)((pci_caddress >> 8) & 0x7);
		uint8_t regnum = (uint8_t)((pci_caddress & 0xfc) + (port & 0x03));
		if (log_pci) LOG(LOG_PCI,LOG_DEBUG)("  Write to device %x register %x (function %x) (:=%x)",(int)devnum,(int)regnum,(int)fctnum,(int)val);

		if (busnum >= PCI_MAX_PCIBUSSES) return;
		if (devnum >= PCI_MAX_PCIDEVICES) return;

		PCI_Device* dev=pci_devices[busnum][devnum];
		if (dev == NULL) return;
		dev->config_write(regnum,iolen,(uint32_t)val);
	}
}


static Bitu read_pci_addr(Bitu port,Bitu iolen) {
    (void)port;//UNUSED
    (void)iolen;//UNUSED
	if (log_pci) LOG(LOG_PCI,LOG_DEBUG)("Read PCI address -> %x",pci_caddress);
	return pci_caddress;
}

static Bitu read_pci(Bitu port,Bitu iolen) {
	if (log_pci) LOG(LOG_PCI,LOG_DEBUG)("Read PCI data -> %x",pci_caddress);

	if (pci_caddress & 0x80000000UL) {
		uint8_t busnum = (uint8_t)((pci_caddress >> 16U) & 0xffU);
		uint8_t devnum = (uint8_t)((pci_caddress >> 11U) & 0x1fU);
		uint8_t fctnum = (uint8_t)((pci_caddress >> 8U) & 0x7U);
		uint8_t regnum = (uint8_t)((pci_caddress & 0xfcu) + (port & 0x03U));
		if (log_pci) LOG(LOG_PCI,LOG_DEBUG)("  Read from device %x register %x (function %x)",(int)devnum,(int)regnum,(int)fctnum);

		if (busnum >= PCI_MAX_PCIBUSSES) return ~0UL;
		if (devnum >= PCI_MAX_PCIDEVICES) return ~0UL;

		PCI_Device* dev=pci_devices[busnum][devnum];
		if (dev == NULL) return ~0UL;
		return dev->config_read(regnum,iolen);
	}

	return ~0UL;
}


static Bitu PCI_PM_Handler() {
	LOG(LOG_PCI,LOG_WARN)("PCI PMode handler, unhandled function %x",reg_ax);
	return CBRET_NONE;
}

PCI_Device::~PCI_Device() {
}

PCI_Device::PCI_Device(uint16_t vendor, uint16_t device) {
	memset(config,0,256);		/* zero config space */
	memset(config_writemask,0,256);	/* none of it is writeable */
	setVendorID(vendor);
	setDeviceID(device);

	/* default: allow setting/clearing some bits in the Command register */
	host_writew(config_writemask+0x04,0x0403);	/* allow changing mem/io enable and interrupt disable */
}

class PCI_VGADevice:public PCI_Device {
private:
	static const uint16_t vendor=0x5333;		// S3
//	static const uint16_t device=0x8811;		// trio64
//	static const uint16_t device=0x8810;		// trio32
public:
	static uint16_t GetDevice(void) {
		switch (s3Card) {
			case S3_86C928:
				return 0x88B0; // FIXME: Datasheet does not list PCI info at all. @TC1995 suggests the PCI version return 0xb0 for CR30, so this is probably the same here.
			case S3_Vision864:
				return 0x88C0; // Vision864, 0x88C0 or 0x88C1
			case S3_Vision868:
				return 0x8880; // Vision868, 0x8880 or 0x8881. S3 didn't list this in their datasheet, but Windows 95 INF files listed it anyway
			case S3_Trio32:
				return 0x8810; // Trio32. 0x8810 or 0x8811
			case S3_Trio64:
			case S3_Trio64V:
				return 0x8811; // Trio64 (rev 00h) / Trio64V+ (rev 40h)
			case S3_ViRGE:
				return 0x5631;
			case S3_ViRGEVX:
				return 0x883D;
			default:
				break;
		};

		return 0x8811; // Trio64 DOSBox SVN default even though SVN is closer to Vision864 functionally
	}
	PCI_VGADevice():PCI_Device(vendor,GetDevice()) {
		// init (S3 graphics card)

		// revision ID
		if (s3Card == S3_Trio64V)
			config[0x08] = 0x40; // Trio64V+ datasheet, page 280, PCI "class code". "Hardwired to 0300004xh" (revision is 40h or more)
		else
			config[0x08] = 0x00; // Trio32/Trio64 datasheet, page 242, PCI "class code". "Hardwired to 03000000h"

		config[0x09] = 0x00;	// interface
		config[0x0a] = 0x00;	// subclass type (vga compatible)
		config[0x0b] = 0x03;	// class type (display controller)
		config[0x0c] = 0x00;	// cache line size
		config[0x0d] = 0x00;	// latency timer
		config[0x0e] = 0x00;	// header type (other)

		config[0x3c] = 0xff;	// no irq

		// reset
		config[0x04] = 0x23;	// command register (vga palette snoop, ports enabled, memory space enabled)
		config[0x05] = 0x00;
		config[0x06] = 0x80;	// status register (medium timing, fast back-to-back)
		config[0x07] = 0x02;

		host_writew(config_writemask+0x04,0x0023);	/* allow changing mem/io enable and VGA palette snoop */

		switch (s3Card) {
			case S3_86C928:
			case S3_Vision864:
			case S3_Trio32:
			case S3_Trio64:
				host_writed(config_writemask+0x10,0xFF800000);	/* BAR0: memory resource 8MB aligned [22:0 reserved] */
				break;
			case S3_Vision868:
			case S3_Trio64V:
			case S3_ViRGE:
			case S3_ViRGEVX:
				host_writed(config_writemask+0x10,0xFC000000);	/* BAR0: memory resource 64MB aligned [25:0 reserved] */
				break;
			default:
				host_writed(config_writemask+0x10,0xFF800000);	/* BAR0: memory resource 8MB aligned [22:0 reserved] */
				break;
		};
		host_writed(config+0x10,(((uint32_t)S3_LFB_BASE)&0xfffffff0) | 0x8);
	}
};


class PCI_SSTDevice:public PCI_Device {
private:
	static const uint16_t vendor=0x121a;	// 3dfx
	uint16_t oscillator_ctr;
	uint16_t pci_ctr;
public:
	PCI_SSTDevice(Bitu type):PCI_Device(vendor,(type==2)?0x0002:0x0001) {
		oscillator_ctr=0;
		pci_ctr=0;

		// init (3dfx voodoo)
		config[0x08] = 0x02;	// revision
		config[0x09] = 0x00;	// interface
		config[0x0a] = 0x00;	// subclass code (video/graphics controller)
		config[0x0b] = 0x04;	// class code (multimedia device)
		config[0x0e] = 0x00;	// header type (other)

		// reset
		config[0x04] = 0x02;	// command register (memory space enabled)
		config[0x05] = 0x00;
		config[0x06] = 0x80;	// status register (fast back-to-back)
		config[0x07] = 0x00;

		config[0x3c] = 0xff;	// no irq

		host_writew(config_writemask+0x04,0x0123);	/* allow changing mem/io enable, B2B enable, and VGA palette snoop */

		host_writed(config_writemask+0x10,0xFF000000);	/* BAR0: memory resource 16MB aligned */
		host_writed(config+0x10,(((uint32_t)VOODOO_INITIAL_LFB)&0xfffffff0) | 0x8);

		if (getDeviceID() >= 2) {
			config[0x40] = 0x00;
			config[0x41] = 0x40;	// voodoo2 revision ID (rev4)
			config[0x42] = 0x01;
			config[0x43] = 0x00;
		}
	}

	virtual void config_write(uint8_t regnum,Bitu iolen,uint32_t value) {
		if (iolen == 1) {
            const unsigned char mask = config_writemask[regnum];
            const unsigned char nmask = ~mask;

			/* configuration write masks apply here as well */
			config[regnum] =
                ((unsigned char)value & mask) +
				(config[regnum] & nmask);

			switch (regnum) { /* FIXME: I hope I ported this right --J.C. */
				case 0x10:
				case 0x11:
				case 0x12:
				case 0x13:
					VOODOO_PCI_SetLFB(host_readd(config+0x10)&0xfffffff0UL); /* need to act on the new (masked off) value */
					break;
				case 0x40:
					VOODOO_PCI_InitEnable(value&7);
					break;
				case 0xc0:
					VOODOO_PCI_Enable(true);
					break;
				case 0xe0:
					VOODOO_PCI_Enable(false);
					break;
				default:
					break;
			}
		}
		else {
			PCI_Device::config_write(regnum,iolen,value); /* which will break down I/O into 8-bit */
		}
	}
	virtual uint32_t config_read(uint8_t regnum,Bitu iolen) {
		if (iolen == 1) {
			switch (regnum) {
				case 0x4c: /* FIXME: I hope I ported this right --J.C. */
				case 0x4d:
				case 0x4e:
				case 0x4f:
					LOG(LOG_PCI,LOG_DEBUG)("SST ParseReadRegister STATUS %x",regnum);
					break;

				case 0x54: /* FIXME: I hope I ported this right --J.C. */
					if (getDeviceID() >= 2) {
						oscillator_ctr++;
						pci_ctr--;
						return (oscillator_ctr | (((unsigned long)pci_ctr<<16ul) & 0x0fff0000ul)) & 0xffu;
					}
					break;
				case 0x55:
					if (getDeviceID() >= 2)
						return ((oscillator_ctr | (((unsigned long)pci_ctr<<16ul) & 0x0fff0000ul)) >> 8ul) & 0xffu;
					break;
				case 0x56:
					if (getDeviceID() >= 2)
						return ((oscillator_ctr | (((unsigned long)pci_ctr<<16ul) & 0x0fff0000ul)) >> 16ul) & 0xffu;
					break;
				case 0x57:
					if (getDeviceID() >= 2)
						return ((oscillator_ctr | (((unsigned long)pci_ctr<<16ul) & 0x0fff0000ul)) >> 24ul) & 0xffu;
					break;
				default:
					break;
			}

			return config[regnum];
		}
		else {
			return PCI_Device::config_read(regnum,iolen); /* which will break down I/O into 8-bit */
		}
	}
};

static bool initialized = false;

static IO_WriteHandleObject PCI_WriteHandler[5];
static IO_ReadHandleObject PCI_ReadHandler[5];

static CALLBACK_HandlerObject callback_pci;

static PhysPt GetPModeCallbackPointer(void) {
	return Real2Phys(callback_pci.Get_RealPointer());
}

static bool IsInitialized(void) {
	return initialized;
}

// set up port handlers and configuration data
static void InitializePCI(void) {
	// log
	LOG(LOG_MISC,LOG_DEBUG)("InitializePCI(): reinitializing PCI bus emulation");

	// install PCI-addressing ports
	PCI_WriteHandler[0].Install(0xcf8,write_pci_addr,IO_MD);
	PCI_ReadHandler[0].Install(0xcf8,read_pci_addr,IO_MD);
	// install PCI-register read/write handlers
	for (Bitu ct=0;ct<4;ct++) {
		PCI_WriteHandler[1+ct].Install(0xcfc+ct,write_pci,IO_MB);
		PCI_ReadHandler[1+ct].Install(0xcfc+ct,read_pci,IO_MB);
	}

	callback_pci.Install(&PCI_PM_Handler,CB_IRETD,"PCI PM");

	initialized=true;
}

bool UnregisterPCIDevice(PCI_Device* device) {
	unsigned int bus,dev;

	for (bus=0;bus < PCI_MAX_PCIBUSSES;bus++) {
		for (dev=0;dev < PCI_MAX_PCIDEVICES;dev++) {
			if (pci_devices[bus][dev] == device) {
				pci_devices[bus][dev] = NULL;
				return true;
			}
		}
	}

	return false;
}

// register PCI device to bus and setup data
Bits RegisterPCIDevice(PCI_Device* device, Bits bus=-1, Bits slot=-1) {
	if (device == NULL) return -1;
	if (bus >= PCI_MAX_PCIBUSSES) return -1;
	if (slot >= PCI_MAX_PCIDEVICES) return -1;

	if (!initialized) InitializePCI();

	if (bus < 0 || slot < 0) {
		Bits try_bus,try_slot;

		try_bus = (bus < 0) ? 0 : bus;
		try_slot = (slot < 0) ? 0 : slot; /* NTS: Most PCI implementations have a motherboard chipset or PCI bus device at slot 0 */
		while (pci_devices[try_bus][try_slot] != NULL) {
			if (slot >= 0 || (try_slot+1) >= PCI_MAX_PCIDEVICES) {
				if (slot < 0) try_slot = 0;
				try_bus++;

				if (bus >= 0 || try_bus >= PCI_MAX_PCIBUSSES)
					break;
			}
			else if (slot < 0) {
				try_slot++;
			}
		}

		bus = try_bus;
		slot = try_slot;
	}

	if (bus >= PCI_MAX_PCIBUSSES || slot >= PCI_MAX_PCIDEVICES)
		return -1;

	if (pci_devices[bus][slot] != NULL) E_Exit("PCI interface error: attempted to fill slot already taken");
	pci_devices[bus][slot]=device;
	return slot;
}

static void Deinitialize(void) {
	initialized=false;
	pci_caddress=0;

	// install PCI-addressing ports
	PCI_WriteHandler[0].Uninstall();
	PCI_ReadHandler[0].Uninstall();
	// install PCI-register read/write handlers
	for (Bitu ct=0;ct<4;ct++) {
		PCI_WriteHandler[1+ct].Uninstall();
		PCI_ReadHandler[1+ct].Uninstall();
	}

	// remove callback
	callback_pci.Uninstall();

	// disconnect all PCI devices
	for (Bitu bus=0;bus<PCI_MAX_PCIBUSSES;bus++) {
		for (Bitu i=0;i < PCI_MAX_PCIDEVICES;i++) {
			if (pci_devices[bus][i] != NULL) {
				delete pci_devices[bus][i];
				pci_devices[bus][i] = NULL;
			}
		}
	}
}

static PCI_Device *S3_PCI=NULL;
static PCI_Device *SST_PCI=NULL;

extern bool enable_pci_vga;

void PCI_AddSVGAS3_Device(void) {
	if (!pcibus_enable || !enable_pci_vga) return;

	if (S3_PCI == NULL) {
		if ((S3_PCI=new PCI_VGADevice()) == NULL)
			return;

		RegisterPCIDevice(S3_PCI);
	}
}

void PCI_RemoveSVGAS3_Device(void) {
	if (S3_PCI != NULL) {
		UnregisterPCIDevice(S3_PCI);
		S3_PCI = NULL;
	}
}

void PCI_AddSST_Device(Bitu type) {
	if (!pcibus_enable) return;

	if (SST_PCI == NULL) {
		Bitu ctype = 1;

		switch (type) {
			case 1:
			case 2:
				ctype = type;
				break;
			default:
				LOG(LOG_PCI,LOG_WARN)("PCI:SST: Invalid board type %x specified",(int)type);
				break;
		}

		LOG(LOG_MISC,LOG_DEBUG)("Initializing Voodoo/3DFX PCI device");
		if ((SST_PCI=new PCI_SSTDevice(ctype)) == NULL)
			return;

		RegisterPCIDevice(SST_PCI);
	}
}

void PCI_RemoveSST_Device(void) {
	if (SST_PCI != NULL) {
		UnregisterPCIDevice(SST_PCI);
		delete SST_PCI;
		SST_PCI = NULL;
	}
}

PhysPt PCI_GetPModeInterface(void) {
	if (!pcibus_enable) return 0;
	return GetPModeCallbackPointer();
}

bool PCI_IsInitialized() {
	return IsInitialized();
}

void PCI_OnPowerOn(Section *sec) {
    (void)sec;//UNUSED
	Section_prop * secprop=static_cast<Section_prop *>(control->GetSection("dosbox"));
	assert(secprop != NULL);

	Deinitialize();

	pcibus_enable = secprop->Get_bool("enable pci bus");
	if (pcibus_enable) InitializePCI();
}

void PCI_ShutDown(Section* sec) {
    (void)sec;//UNUSED
	Deinitialize();
}

void PCIBUS_Init() {
	Section_prop * secprop=static_cast<Section_prop *>(control->GetSection("dosbox"));
	assert(secprop != NULL);

	LOG(LOG_MISC,LOG_DEBUG)("Initializing PCI bus emulation");

	initialized=false;
	for (Bitu bus=0;bus<PCI_MAX_PCIBUSSES;bus++)
		for (Bitu devct=0;devct<PCI_MAX_PCIDEVICES;devct++)
			pci_devices[bus][devct]=NULL;

	AddExitFunction(AddExitFunctionFuncPair(PCI_ShutDown),false);
	AddVMEventFunction(VM_EVENT_POWERON,AddVMEventFunctionFuncPair(PCI_OnPowerOn));

    /* NTS: PCI emulation does not have to change anything when entering into PC-98 mode.
     *      I/O ports for PCI bus control are the SAME on both platforms (0xCF8-0xCFF). */
}

