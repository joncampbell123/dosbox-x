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

#ifndef DOSBOX_PCI_H
#define DOSBOX_PCI_H

#define PCI_MAX_PCIBUSSES		256
#define PCI_MAX_PCIDEVICES		32
#define PCI_MAX_PCIFUNCTIONS		8

class PCI_Device {
public:
	/* configuration space */
	unsigned char config[256];
	unsigned char config_writemask[256];

	PCI_Device(Bit16u vendor, Bit16u device);
	virtual ~PCI_Device();

	/* configuration space assistant functions */

	Bit16u getDeviceID() {
		return host_readw(config + 0x02);
	}
	void setDeviceID(const Bit16u device) {
		return host_writew(config + 0x02,device);
	}

	Bit16u getVendorID() {
		return host_readw(config + 0x00);
	}
	void setVendorID(const Bit16u vendor) {
		return host_writew(config + 0x00,vendor);
	}

	/* configuration space I/O */
	virtual void config_write(Bit8u regnum,Bitu iolen,Bit32u value) {
		if (iolen == 1) {
			/* only allow writing to the bits marked off as writeable */
			config[regnum] = (config[regnum] & (~config_writemask[regnum])) + (config_writemask[regnum] & value);
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
				config_write(regnum+i,1,value&0xFF);
				value >>= 8U;
			}
		}
	}
	virtual Bit32u config_read(Bit8u regnum,Bitu iolen) {
		/* subdivide into 8-bit I/O */
		Bit32u v=0;

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
				v += ((config_read(regnum+i,1)&0xFF) << ((iolen-i-1)*8));
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

