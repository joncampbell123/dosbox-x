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
			config[regnum] = value = (value & config_writemask[regnum]) +
				(config[regnum] & (~config_writemask[regnum]));

			switch (regnum) { /* FIXME: I hope I ported this right --J.C. */
				case 0x10:
				case 0x11:
				case 0x12:
				case 0x13:
					VOODOO_PCI_SetLFB(value<<24);
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
