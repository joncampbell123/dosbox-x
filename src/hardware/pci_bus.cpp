/*
 *  Copyright (C) 2002-2015  The DOSBox Team
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
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

bool pcibus_enable = false;

static Bit32u pci_caddress=0;			// current PCI addressing

static PCI_Device* pci_devices[PCI_MAX_PCIBUSSES][PCI_MAX_PCIDEVICES]={{NULL}};		// registered PCI devices

// PCI address
// 31    - set for a PCI access
// 30-24 - 0
// 23-16 - bus number			(0x00ff0000)
// 15-11 - device number (slot)	(0x0000f800)
// 10- 8 - subfunction number	(0x00000700)
//  7- 2 - config register #	(0x000000fc)

static void write_pci_addr(Bitu port,Bitu val,Bitu iolen) {
	LOG(LOG_PCI,LOG_NORMAL)("Write PCI address :=%x",val);
	pci_caddress=val;
}

static void write_pci(Bitu port,Bitu val,Bitu iolen) {
	LOG(LOG_PCI,LOG_NORMAL)("Write PCI data port %x :=%x (len %d)",port,val,iolen);

	if (pci_caddress & 0x80000000) {
		Bit8u busnum = (Bit8u)((pci_caddress >> 16) & 0xff);
		Bit8u devnum = (Bit8u)((pci_caddress >> 11) & 0x1f);
		Bit8u fctnum = (Bit8u)((pci_caddress >> 8) & 0x7);
		Bit8u regnum = (Bit8u)((pci_caddress & 0xfc) + (port & 0x03));
		LOG(LOG_PCI,LOG_NORMAL)("  Write to device %x register %x (function %x) (:=%x)",devnum,regnum,fctnum,val);

		if (busnum >= PCI_MAX_PCIBUSSES) return;
		if (devnum >= PCI_MAX_PCIDEVICES) return;

		PCI_Device* dev=pci_devices[busnum][devnum];
		if (dev == NULL) return;
		dev->config_write(regnum,iolen,val);
	}
}


static Bitu read_pci_addr(Bitu port,Bitu iolen) {
	LOG(LOG_PCI,LOG_NORMAL)("Read PCI address -> %x",pci_caddress);
	return pci_caddress;
}

static Bitu read_pci(Bitu port,Bitu iolen) {
	LOG(LOG_PCI,LOG_NORMAL)("Read PCI data -> %x",pci_caddress);

	if (pci_caddress & 0x80000000) {
		Bit8u busnum = (Bit8u)((pci_caddress >> 16) & 0xff);
		Bit8u devnum = (Bit8u)((pci_caddress >> 11) & 0x1f);
		Bit8u fctnum = (Bit8u)((pci_caddress >> 8) & 0x7);
		Bit8u regnum = (Bit8u)((pci_caddress & 0xfc) + (port & 0x03));
		LOG(LOG_PCI,LOG_NORMAL)("  Read from device %x register %x (function %x)",devnum,regnum,fctnum);

		if (busnum >= PCI_MAX_PCIBUSSES) return ~0;
		if (devnum >= PCI_MAX_PCIDEVICES) return ~0;

		PCI_Device* dev=pci_devices[busnum][devnum];
		if (dev == NULL) return ~0;
		return dev->config_read(regnum,iolen);
	}

	return ~0;
}


static Bitu PCI_PM_Handler() {
	LOG_MSG("PCI PMode handler, function %x",reg_ax);
	return CBRET_NONE;
}

PCI_Device::~PCI_Device() {
}

PCI_Device::PCI_Device(Bit16u vendor, Bit16u device) {
	memset(config,0,256);		/* zero config space */
	memset(config_writemask,0,256);	/* none of it is writeable */
	setVendorID(vendor);
	setDeviceID(device);

	/* default: allow setting/clearing some bits in the Command register */
	host_writew(config_writemask+0x04,0x0403);	/* allow changing mem/io enable and interrupt disable */
}

class PCI_VGADevice:public PCI_Device {
private:
	static const Bit16u vendor=0x5333;		// S3
	static const Bit16u device=0x8811;		// trio64
//	static const Bit16u device=0x8810;		// trio32
public:
	PCI_VGADevice():PCI_Device(vendor,device) {
		// init (S3 graphics card)
		config[0x08] = 0x00;	// revision ID
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

		host_writed(config_writemask+0x10,0xFF000000);	/* BAR0: memory resource 16MB aligned */
		host_writed(config+0x10,(((Bit32u)S3_LFB_BASE)&0xfffffff0) | 0x8);

		host_writed(config_writemask+0x14,0xFFFF0000);	/* BAR1: memory resource 64KB aligned */
		host_writed(config+0x14,(((Bit32u)(S3_LFB_BASE+0x1000000))&0xfffffff0));
	}
};


class PCI_SSTDevice:public PCI_Device {
private:
	static const Bit16u vendor=0x121a;	// 3dfx
	Bit16u oscillator_ctr;
	Bit16u pci_ctr;
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
		host_writed(config+0x10,(((Bit32u)VOODOO_INITIAL_LFB)&0xfffffff0) | 0x8);

		if (getDeviceID() >= 2) {
			config[0x40] = 0x00;
			config[0x41] = 0x40;	// voodoo2 revision ID (rev4)
			config[0x42] = 0x01;
			config[0x43] = 0x00;
		}
	}

	virtual void config_write(Bit8u regnum,Bitu iolen,Bit32u value) {
		if (iolen == 1) {
			/* configuration write masks apply here as well */
			config[regnum] = (value & config_writemask[regnum]) +
				(config[regnum] & (~config_writemask[regnum]));

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
	virtual Bit32u config_read(Bit8u regnum,Bitu iolen) {
		if (iolen == 1) {
			switch (regnum) {
				case 0x4c: /* FIXME: I hope I ported this right --J.C. */
				case 0x4d:
				case 0x4e:
				case 0x4f:
					LOG_MSG("SST ParseReadRegister STATUS %x",regnum);
					break;

				case 0x54: /* FIXME: I hope I ported this right --J.C. */
					if (getDeviceID() >= 2) {
						oscillator_ctr++;
						pci_ctr--;
						return (oscillator_ctr | ((pci_ctr<<16) & 0x0fff0000)) & 0xff;
					}
					break;
				case 0x55:
					if (getDeviceID() >= 2)
						return ((oscillator_ctr | ((pci_ctr<<16) & 0x0fff0000)) >> 8) & 0xff;
					break;
				case 0x56:
					if (getDeviceID() >= 2)
						return ((oscillator_ctr | ((pci_ctr<<16) & 0x0fff0000)) >> 16) & 0xff;
					break;
				case 0x57:
					if (getDeviceID() >= 2)
						return ((oscillator_ctr | ((pci_ctr<<16) & 0x0fff0000)) >> 24) & 0xff;
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

class PCI:public Module_base{
private:
	bool initialized;

protected:
	IO_WriteHandleObject PCI_WriteHandler[5];
	IO_ReadHandleObject PCI_ReadHandler[5];

	CALLBACK_HandlerObject callback_pci;

public:

	PhysPt GetPModeCallbackPointer(void) {
		return Real2Phys(callback_pci.Get_RealPointer());
	}

	bool IsInitialized(void) {
		return initialized;
	}

	// set up port handlers and configuration data
	void InitializePCI(void) {
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

		if (!initialized) InitializePCI();

		if (pci_devices[bus][slot] != NULL) E_Exit("PCI interface error: attempted to fill slot already taken");
		pci_devices[bus][slot]=device;
		return slot;
	}

	void Deinitialize(void) {
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

		callback_pci.Uninstall();
	}

	PCI(Section* configuration):Module_base(configuration) {
		initialized=false;

		for (Bitu bus=0;bus<PCI_MAX_PCIBUSSES;bus++)
			for (Bitu devct=0;devct<PCI_MAX_PCIDEVICES;devct++)
				pci_devices[bus][devct]=NULL;
	}

	~PCI() {
		unsigned int i;

		for (Bitu bus=0;bus<PCI_MAX_PCIBUSSES;bus++) {
			for (i=0;i < PCI_MAX_PCIDEVICES;i++) {
				if (pci_devices[bus][i] != NULL) {
					delete pci_devices[bus][i];
					pci_devices[bus][i] = NULL;
				}
			}
		}

		initialized=false;
	}
};

static PCI* pci_interface=NULL;
static PCI_Device *S3_PCI=NULL;
static PCI_Device *SST_PCI=NULL;

void PCI_AddSVGAS3_Device(void) {
	if (!pcibus_enable) return;
	if (pci_interface == NULL) E_Exit("PCI device add attempt and PCI interface not initialized");

	if (S3_PCI == NULL) {
		if ((S3_PCI=new PCI_VGADevice()) == NULL)
			return;

		pci_interface->RegisterPCIDevice(S3_PCI);
	}
}

void PCI_RemoveSVGAS3_Device(void) {
	if (pci_interface != NULL) {
		if (S3_PCI != NULL) {
			pci_interface->UnregisterPCIDevice(S3_PCI);
			S3_PCI = NULL;
		}
	}
}

void PCI_AddSST_Device(Bitu type) {
	if (!pcibus_enable) return;
	if (pci_interface == NULL) E_Exit("PCI device add attempt and PCI interface not initialized");

	if (SST_PCI == NULL) {
		Bitu ctype = 1;

		switch (type) {
			case 1:
			case 2:
				ctype = type;
				break;
			default:
				LOG_MSG("PCI:SST: Invalid board type %x specified",type);
				break;
		}

		if ((SST_PCI=new PCI_SSTDevice(ctype)) == NULL)
			return;

		pci_interface->RegisterPCIDevice(SST_PCI);
	}
}

void PCI_RemoveSST_Device(void) {
	if (pci_interface!=NULL) {
		if (SST_PCI != NULL) {
			pci_interface->UnregisterPCIDevice(SST_PCI);
			delete SST_PCI;
			SST_PCI = NULL;
		}
	}
}

PhysPt PCI_GetPModeInterface(void) {
	if (pci_interface) {
		return pci_interface->GetPModeCallbackPointer();
	}
	return 0;
}

bool PCI_IsInitialized() {
	if (pci_interface) return pci_interface->IsInitialized();
	return false;
}


void PCI_ShutDown(Section* sec){
	if (pci_interface != NULL) {
		delete pci_interface;
		pci_interface = NULL;
	}
}

void PCI_Init(Section* sec) {
	sec->AddDestroyFunction(&PCI_ShutDown,false);
}

void PCIBUS_Init(Section *sec) {
	Section_prop * secprop=static_cast<Section_prop *>(sec);

	pcibus_enable = secprop->Get_bool("enable pci bus");

	if (pci_interface == NULL && pcibus_enable)
		pci_interface = new PCI(sec);
}

