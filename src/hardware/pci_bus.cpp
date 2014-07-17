/*
 *  Copyright (C) 2002-2013  The DOSBox Team
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


#if defined(PCI_FUNCTIONALITY_ENABLED)

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

static void write_pci_register(PCI_Device* dev,Bit8u regnum,Bit8u value) {
	// vendor/device/class IDs/header type/etc. are read-only
	if ((regnum<0x04) || ((regnum>=0x06) && (regnum<0x0c)) || (regnum==0x0e)) return;
	if (dev==NULL) return;
	switch (dev->config[0x0e]&0x7f) { // header-type specific handling
		case 0x00:
			if ((regnum>=0x28) && (regnum<0x30)) return;	// subsystem information is read-only
			break;
		case 0x01:
		case 0x02:
		default:
			break;
	}

	// call device routine for special actions and the
	// possibility to discard/replace the value that is to be written
	Bits parsed_register=dev->ParseWriteRegister(regnum,value);
	if (parsed_register>=0)
		dev->config[regnum]=(Bit8u)(parsed_register&0xff);
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

		// write data to PCI device/configuration
		switch (iolen) {
			case 1: write_pci_register(dev,regnum+0,(Bit8u)(val&0xff)); break;
			case 2: write_pci_register(dev,regnum+0,(Bit8u)(val&0xff));
					write_pci_register(dev,regnum+1,(Bit8u)((val>>8)&0xff)); break;
			case 4: write_pci_register(dev,regnum+0,(Bit8u)(val&0xff));
					write_pci_register(dev,regnum+1,(Bit8u)((val>>8)&0xff));
					write_pci_register(dev,regnum+2,(Bit8u)((val>>16)&0xff));
					write_pci_register(dev,regnum+3,(Bit8u)((val>>24)&0xff)); break;
		}
	}
}


static Bitu read_pci_addr(Bitu port,Bitu iolen) {
	LOG(LOG_PCI,LOG_NORMAL)("Read PCI address -> %x",pci_caddress);
	return pci_caddress;
}

// read single 8bit value from register file (special register treatment included)
static Bit8u read_pci_register(PCI_Device* dev,Bit8u regnum) {
	switch (regnum) {
		case 0x00:
			return (Bit8u)(dev->VendorID()&0xff);
		case 0x01:
			return (Bit8u)((dev->VendorID()>>8)&0xff);
		case 0x02:
			return (Bit8u)(dev->DeviceID()&0xff);
		case 0x03:
			return (Bit8u)((dev->DeviceID()>>8)&0xff);
		case 0x0e:
			return (dev->config[regnum]&0x7f);
		default:
			break;
	}

	// call device routine for special actions and possibility to discard/remap register
	Bits parsed_regnum=dev->ParseReadRegister(regnum);
	if ((parsed_regnum>=0) && (parsed_regnum<256))
		return dev->config[parsed_regnum];

	Bit8u newval, mask;
	if (dev->OverrideReadRegister(regnum, &newval, &mask)) {
		Bit8u oldval=dev->config[regnum] & (~mask);
		return oldval | (newval & mask);
	}

	return 0xff;
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

		switch (iolen) {
			case 1:
				{
					Bit8u val8=read_pci_register(dev,regnum);
					return val8;
				}
			case 2:
				{
					Bit16u val16=read_pci_register(dev,regnum);
					val16|=(read_pci_register(dev,regnum+1)<<8);
					return val16;
				}
			case 4:
				{
					Bit32u val32=read_pci_register(dev,regnum);
					val32|=(read_pci_register(dev,regnum+1)<<8);
					val32|=(read_pci_register(dev,regnum+2)<<16);
					val32|=(read_pci_register(dev,regnum+3)<<24);
					return val32;
				}
			default:
				break;
		}
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
	memset(config,0,256);
	pci_id=-1;
	pci_subfunction=-1;
	vendor_id=vendor;
	device_id=device;
}

// queued devices (PCI device registering requested before the PCI framework was initialized)
static const Bitu max_rqueued_devices=16;
static Bitu num_rqueued_devices=0;
static PCI_Device* rqueued_devices[max_rqueued_devices];


#include "pci_devices.h"

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

		if (device->InitializeRegisters(device->config)) {
			if (pci_devices[bus][slot] != NULL) E_Exit("PCI interface error: attempted to fill slot already taken");
			pci_devices[bus][slot]=device;
			return slot;
		}

		return -1;
	}

	void Deinitialize(void) {
		initialized=false;
		num_rqueued_devices=0;
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

		if (num_rqueued_devices>0) {
			// register all devices that have been added before the PCI bus was instantiated
			for (Bitu dct=0;dct<num_rqueued_devices;dct++)
				this->RegisterPCIDevice(rqueued_devices[dct]);

			num_rqueued_devices=0;
		}
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
		num_rqueued_devices=0;
	}

};

static PCI* pci_interface=NULL;
static PCI_Device *S3_PCI=NULL;
static PCI_Device *SST_PCI=NULL;

void PCI_AddSVGAS3_Device(void) {
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
	if (pci_interface == NULL)
		pci_interface = new PCI(sec);
}

#else

void PCI_AddSVGAS3_Device(void) {
}

void PCI_RemoveSVGAS3_Device(void) {
}

void PCI_AddSST_Device(Bitu type) {
}

void PCI_RemoveSST_Device(void) {
}

#endif
