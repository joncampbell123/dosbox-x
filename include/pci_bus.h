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

#ifndef DOSBOX_PCI_H
#define DOSBOX_PCI_H

#define PCI_MAX_PCIBUSSES		    255
#define PCI_MAX_PCIDEVICES		    32
#define PCI_MAX_PCIFUNCTIONS		8

class PCI_Device {
public:
	/* configuration space */
	unsigned char config[256];
	unsigned char config_writemask[256];

	PCI_Device(uint16_t vendor, uint16_t device);
	virtual ~PCI_Device();

	/* configuration space assistant functions */

	uint16_t getDeviceID() {
		return host_readw(config + 0x02);
	}
	void setDeviceID(const uint16_t device) {
		return host_writew(config + 0x02,device);
	}

	uint16_t getVendorID() {
		return host_readw(config + 0x00);
	}
	void setVendorID(const uint16_t vendor) {
		return host_writew(config + 0x00,vendor);
	}

	/* configuration space I/O */
	virtual void config_write(uint8_t regnum,Bitu iolen,uint32_t value) {
		if (iolen == 1) {
            const unsigned char mask = config_writemask[regnum];
            const unsigned char nmask = ~mask;
			/* only allow writing to the bits marked off as writeable */
			config[regnum] = (config[regnum] & nmask) + ((unsigned char)value & mask);
		}
		else if (iolen == 4 && (regnum&3) == 2) { /* unaligned DWORD write (subdividable into two WORD writes) */
			config_write(regnum,2,value&0xFFFF);
			config_write(regnum+2,2,value>>16);
		}
		else {
			/* subdivide into 8-bit I/O */
			/* NTS: If I recall, this virtual function call means that we'll call the
			 *      C++ subclass's config_write() NOT our own--right? */
			for (Bitu i=0;i < iolen;i++) {
				config_write((uint8_t)(regnum+i),1,value&0xFF);
				value >>= 8U;
			}
		}
	}
	virtual uint32_t config_read(uint8_t regnum,Bitu iolen) {
		/* subdivide into 8-bit I/O */
		uint32_t v=0;

		if (iolen == 1)
			return config[regnum];
		else if (iolen == 4 && (regnum&3) == 2) { /* unaligned DWORD read (subdividable into two WORD reads) */
			v = config_read(regnum,2);
			v += config_read(regnum+2,2) << 16;
		}
		else {
			/* NTS: If I recall, this virtual function call means that we'll call the
			 *      C++ subclass's config_read() NOT our own--right? */
			for (Bitu i=0;i < iolen;i++)
				v += ((config_read((uint8_t)(regnum+i),1)&0xFF) << ((iolen-i-1)*8));
		}

		return v;
	}
};

bool PCI_IsInitialized();

void PCI_AddSVGAS3_Device(void);
void PCI_RemoveSVGAS3_Device(void);

void PCI_AddSST_Device(Bitu type);
void PCI_RemoveSST_Device(void);

RealPt PCI_GetPModeInterface(void);

#endif

