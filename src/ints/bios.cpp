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

#include <assert.h>
#include "dosbox.h"
#include "mem.h"
#include "cpu.h"
#include "bios.h"
#include "regs.h"
#include "cpu.h"
#include "callback.h"
#include "inout.h"
#include "pic.h"
#include "hardware.h"
#include "pci_bus.h"
#include "joystick.h"
#include "mouse.h"
#include "setup.h"
#include "serialport.h"
#include "vga.h"
extern bool PS1AudioCard;
#include "parport.h"
#include <time.h>
#include <sys/timeb.h>
/* mouse.cpp */
extern bool en_bios_ps2mouse;

/* if mem_systems 0 then size_extended is reported as the real size else 
 * zero is reported. ems and xms can increase or decrease the other_memsystems
 * counter using the BIOS_ZeroExtendedSize call */
static Bit16u size_extended;
static unsigned int ISA_PNP_WPORT = 0x20B;
static unsigned int ISA_PNP_WPORT_BIOS = 0x20B;
static IO_ReadHandleObject *ISAPNP_PNP_READ_PORT = NULL;		/* 0x200-0x3FF range */
static IO_WriteHandleObject *ISAPNP_PNP_ADDRESS_PORT = NULL;		/* 0x279 */
static IO_WriteHandleObject *ISAPNP_PNP_DATA_PORT = NULL;		/* 0xA79 */
static unsigned char ISA_PNP_CUR_CSN = 0;
static unsigned char ISA_PNP_CUR_ADDR = 0;
static unsigned char ISA_PNP_CUR_STATE = 0;
enum {
	ISA_PNP_WAIT_FOR_KEY=0,
	ISA_PNP_SLEEP,
	ISA_PNP_ISOLATE,
	ISA_PNP_CONFIG
};

const unsigned char isa_pnp_init_keystring[32] = {
	0x6A,0xB5,0xDA,0xED,0xF6,0xFB,0x7D,0xBE,
	0xDF,0x6F,0x37,0x1B,0x0D,0x86,0xC3,0x61,
	0xB0,0x58,0x2C,0x16,0x8B,0x45,0xA2,0xD1,
	0xE8,0x74,0x3A,0x9D,0xCE,0xE7,0x73,0x39
};

static unsigned char ISA_PNP_KEYMATCH=0;
static Bits other_memsystems=0;
static bool apm_realmode_connected = false;
void CMOS_SetRegister(Bitu regNr, Bit8u val); //For setting equipment word
bool ISAPNPBIOS=false;
bool APMBIOS=false;

ISAPnPDevice::ISAPnPDevice() {
	CSN = 0;
	logical_device = 0;
	memset(ident,0,sizeof(ident));
	ident_bp = 0;
	ident_2nd = 0;
	resource_data_pos = 0;
}

void ISAPnPDevice::config(Bitu val) {
}

void ISAPnPDevice::wakecsn(Bitu val) {
	ident_bp = 0;
	ident_2nd = 0;
	resource_data_pos = 0;
	resource_ident = 0;
}

void ISAPnPDevice::select_logical_device(Bitu val) {
}
	
void ISAPnPDevice::checksum_ident() {
	unsigned char checksum = 0x6a,bit;
	int i,j;

	for (i=0;i < 8;i++) {
		for (j=0;j < 8;j++) {
			bit = (ident[i] >> j) & 1;
			checksum = ((((checksum ^ (checksum >> 1)) & 1) ^ bit) << 7) | (checksum >> 1);
		}
	}

	ident[8] = checksum;
}

void ISAPnPDevice::on_pnp_key() {
	resource_ident = 0;
}

uint8_t ISAPnPDevice::read(Bitu addr) {
	return 0x00;
}

void ISAPnPDevice::write(Bitu addr,Bitu val) {
}

#define MAX_ISA_PNP_DEVICES	64

static ISAPnPDevice *ISA_PNP_selected = NULL;
static ISAPnPDevice *ISA_PNP_devs[MAX_ISA_PNP_DEVICES] = {NULL};
static Bitu ISA_PNP_devnext = 0;

static const unsigned char ISAPnPTestDevice_sysdev[] = {
	ISAPNP_IO_RANGE(
			0x01,					/* decodes 16-bit ISA addr */
			0x28,0x28,				/* min-max range I/O port */
			0x04,0x04),				/* align=4 length=4 */
	ISAPNP_IRQ_SINGLE(
			8,					/* IRQ 8 */
			0x09),					/* HTE=1 LTL=1 */
	ISAPNP_END
};

class ISAPnPTestDevice : public ISAPnPDevice {
	public:
		ISAPnPTestDevice() : ISAPnPDevice() {
			resource_ident = 0;
			resource_data = (unsigned char*)ISAPnPTestDevice_sysdev;
			resource_data_len = sizeof(ISAPnPTestDevice_sysdev);
			*((uint32_t*)(ident+0)) = ISAPNP_ID('D','O','S',0x1,0x2,0x3,0x4);
			*((uint32_t*)(ident+4)) = 0xFFFFFFFFUL;
			checksum_ident();
		}
};

static ISAPnPTestDevice *isapnp_test = NULL;

void ISA_PNP_devreg(ISAPnPDevice *x) {
	if (ISA_PNP_devnext < MAX_ISA_PNP_DEVICES) {
		ISA_PNP_devs[ISA_PNP_devnext++] = x;
		x->CSN = ISA_PNP_devnext;
	}
	//else
	//	fprintf(stderr,"WARNING: ignored PnP device registration\n");
}

static Bitu isapnp_read_port(Bitu port,Bitu /*iolen*/) {
	Bitu ret=0xff;

	switch (ISA_PNP_CUR_ADDR) {
		case 0x01:	/* serial isolation */
			   if (ISA_PNP_selected && ISA_PNP_selected->CSN == 0) {
				   if (ISA_PNP_selected->ident_bp < 72) {
					   if (ISA_PNP_selected->ident[ISA_PNP_selected->ident_bp>>3] & (1 << (ISA_PNP_selected->ident_bp&7)))
						   ret = ISA_PNP_selected->ident_2nd ? 0xAA : 0x55;
					   else
						   ret = 0xFF;

					   if (++ISA_PNP_selected->ident_2nd >= 2) {
						   ISA_PNP_selected->ident_2nd = 0;
						   ISA_PNP_selected->ident_bp++;
					   }
				   }
			   }
			   else {
				   ret = 0xFF;
			   }
			   break;
		case 0x04:	/* read resource data */
			   if (ISA_PNP_selected) {
				   if (ISA_PNP_selected->resource_ident < 9)
					   ret = ISA_PNP_selected->ident[ISA_PNP_selected->resource_ident++];			   
				   else if (ISA_PNP_selected->resource_data_pos < ISA_PNP_selected->resource_data_len)
					   ret = ISA_PNP_selected->resource_data[ISA_PNP_selected->resource_data_pos++];
			   }
			   break;
		case 0x05:	/* read resource status */
			   if (ISA_PNP_selected) {
				   if (ISA_PNP_selected->resource_data_pos < ISA_PNP_selected->resource_data_len)
					   ret = 0x01;	/* TODO: simulate hardware slowness */
			   }
			   break;
		case 0x06:	/* card select number */
			   if (ISA_PNP_selected) ret = ISA_PNP_selected->CSN;
			   break;
		case 0x07:	/* logical device number */
			   if (ISA_PNP_selected) ret = ISA_PNP_selected->logical_device;
			   break;
		default:	/* pass the rest down to the class */
			   if (ISA_PNP_selected) ret = ISA_PNP_selected->read(ISA_PNP_CUR_ADDR);
			   break;
	}

//	if (1) fprintf(stderr,"PnP read(%02X) = %02X\n",ISA_PNP_CUR_ADDR,ret);
	return ret;
}

static void isapnp_write_port(Bitu port,Bitu val,Bitu /*iolen*/) {
	Bitu i;

	if (port == 0x279) {
//		if (1) fprintf(stderr,"PnP addr(%02X)\n",val);
		if (val == isa_pnp_init_keystring[ISA_PNP_KEYMATCH]) {
			if (++ISA_PNP_KEYMATCH == 32) {
//				fprintf(stderr,"ISA PnP key -> going to sleep\n");
				ISA_PNP_CUR_STATE = ISA_PNP_SLEEP;
				ISA_PNP_KEYMATCH = 0;
				for (i=0;i < MAX_ISA_PNP_DEVICES;i++) {
					if (ISA_PNP_devs[i] != NULL) {
						ISA_PNP_devs[i]->on_pnp_key();
					}
				}
			}
		}
		else {
			ISA_PNP_KEYMATCH = 0;
		}

		ISA_PNP_CUR_ADDR = val;
	}
	else if (port == 0xA79) {
//		if (1) fprintf(stderr,"PnP write(%02X) = %02X\n",ISA_PNP_CUR_ADDR,val);
		switch (ISA_PNP_CUR_ADDR) {
			case 0x00: {	/* RD_DATA */
				unsigned int np = ((val & 0xFF) << 2) | 3;
				if (np != ISA_PNP_WPORT) {
					unsigned int old = ISA_PNP_WPORT;
					ISA_PNP_WPORT = np;
					delete ISAPNP_PNP_READ_PORT;
					ISAPNP_PNP_READ_PORT = new IO_ReadHandleObject;
					//fprintf(stderr,"PNP OS changed I/O read port to 0x%03X (from 0x%03X)\n",ISA_PNP_WPORT,old);
					ISAPNP_PNP_READ_PORT->Install(ISA_PNP_WPORT,isapnp_read_port,IO_MB);
					if (ISA_PNP_selected != NULL) {
						ISA_PNP_selected->ident_bp = 0;
						ISA_PNP_selected->ident_2nd = 0;
						ISA_PNP_selected->resource_data_pos = 0;
					}
				}
			} break;
			case 0x02:	/* config control */
				   if (val & 4) {
					   /* ALL CARDS RESET CSN to 0 */
					   for (i=0;i < MAX_ISA_PNP_DEVICES;i++) {
						   if (ISA_PNP_devs[i] != NULL) {
							   ISA_PNP_devs[i]->CSN = 0;
						   }
					   }
				   }
				   if (val & 2) ISA_PNP_CUR_STATE = ISA_PNP_WAIT_FOR_KEY;
				   if ((val & 1) && ISA_PNP_selected) ISA_PNP_selected->config(val);
				   for (i=0;i < MAX_ISA_PNP_DEVICES;i++) {
					   if (ISA_PNP_devs[i] != NULL) {
						   ISA_PNP_devs[i]->ident_bp = 0;
						   ISA_PNP_devs[i]->ident_2nd = 0;
						   ISA_PNP_devs[i]->resource_data_pos = 0;
					   }
				   }
				   break;
			case 0x03: {	/* wake[CSN] */
				ISA_PNP_selected = NULL;
				for (i=0;ISA_PNP_selected == NULL && i < MAX_ISA_PNP_DEVICES;i++) {
					if (ISA_PNP_devs[i] == NULL)
						continue;
					if (ISA_PNP_devs[i]->CSN == val) {
						ISA_PNP_selected = ISA_PNP_devs[i];
						ISA_PNP_selected->wakecsn(val);
					}
				}
				if (val == 0)
					ISA_PNP_CUR_STATE = ISA_PNP_ISOLATE;
				else
					ISA_PNP_CUR_STATE = ISA_PNP_CONFIG;
				} break;
			case 0x06:	/* card select number */
				if (ISA_PNP_selected) ISA_PNP_selected->CSN = val;
				break;
			case 0x07:	/* logical device number */
				if (ISA_PNP_selected) ISA_PNP_selected->select_logical_device(val);
				break;
			default:	/* pass the rest down to the class */
				if (ISA_PNP_selected) ISA_PNP_selected->write(ISA_PNP_CUR_ADDR,val);
				break;
		}
	}
}

void ISAPNP_Cfg_Init(Section *s) {
	Section_prop * section=static_cast<Section_prop *>(s);
	ISAPNPBIOS = section->Get_bool("isapnpbios");
	APMBIOS = section->Get_bool("apmbios");
}

/* the PnP callback registered two entry points. One for real, one for protected mode. */
static Bitu PNPentry_real,PNPentry_prot;

static bool ISAPNP_Verify_BiosSelector(Bitu seg) {
	if (!cpu.pmode || (reg_flags & FLAG_VM)) {
		return (seg == 0xF000);
	} else if (seg == 0)
		return 0;
	else {
#if 1
		/* FIXME: Always return true. But figure out how to ask DOSBox the linear->phys
			  mapping to determine whether the segment's base address maps to 0xF0000.
			  In the meantime disabling this check makes PnP BIOS emulation work with
			  Windows 95 OSR2 which appears to give us a segment mapped to a virtual
			  address rather than linearly mapped to 0xF0000 as Windows 95 original
			  did. */
		return true;
#else
		Descriptor desc;
		cpu.gdt.GetDescriptor(seg,desc);

		/* TODO: Check desc.Type() to make sure it's a writeable data segment */
		return (desc.GetBase() == 0xF0000);
#endif
	}
}

static bool ISAPNP_CPU_ProtMode() {
	if (!cpu.pmode || (reg_flags & FLAG_VM))
		return 0;

	return 1;
}

static Bitu ISAPNP_xlate_address(Bitu far_ptr) {
	if (!cpu.pmode || (reg_flags & FLAG_VM))
		return Real2Phys(far_ptr);
	else {
		Descriptor desc;
		cpu.gdt.GetDescriptor(far_ptr >> 16,desc);

		/* TODO: Check desc.Type() to make sure it's a writeable data segment */
		return (desc.GetBase() + (far_ptr & 0xFFFF));
	}
}

/* TODO: ISA PnP BIOS emulation complete. Now what we need is ISA PnP I/O port emulation. That means catching
 *       writes to 0x279 (ADDRESS), 0xA79 (WRITE_DATA), and emulating a moveable I/O port (READ_DATA) */

class ISAPNP_SysDevNode {
public:
	ISAPNP_SysDevNode(const unsigned char *ir,int len,bool already_alloc=false) {
		if (already_alloc) {
			raw = (unsigned char*)ir;
			raw_len = len;
			own = false;
		}
		else {
			if (len > 65535) E_Exit("ISAPNP_SysDevNode data too long");
			raw = new unsigned char[len+1];
			if (ir == NULL) E_Exit("ISAPNP_SysDevNode cannot allocate buffer");
			memcpy(raw,ir,len);
			raw_len = len;
			raw[len] = 0;
			own = true;
		}
	}
	~ISAPNP_SysDevNode() {
		if (own) delete[] raw;
	}
public:
	unsigned char*		raw;
	int			raw_len;
	bool			own;
};

static const unsigned char ISAPNP_sysdev_Keyboard[] = {
	ISAPNP_SYSDEV_HEADER(
			ISAPNP_ID('P','N','P',0x0,0x3,0x0,0x3), /* PNP0303 IBM Enhanced 101/102 key with PS/2 */
			ISAPNP_TYPE(0x09,0x00,0x00),		/* type: input, keyboard */
			0x0001 | 0x0002),			/* can't disable, can't configure */
	/*----------allocated--------*/
	ISAPNP_IO_RANGE(
			0x01,					/* decodes 16-bit ISA addr */
			0x60,0x60,				/* min-max range I/O port */
			0x01,0x01),				/* align=1 length=1 */
	ISAPNP_IO_RANGE(
			0x01,					/* decodes 16-bit ISA addr */
			0x64,0x64,				/* min-max range I/O port */
			0x01,0x01),				/* align=1 length=1 */
	ISAPNP_IRQ_SINGLE(
			1,					/* IRQ 1 */
			0x09),					/* HTE=1 LTL=1 */
	ISAPNP_END,
	/*----------possible--------*/
	ISAPNP_END,
	/*----------compatible--------*/
	ISAPNP_END
};

static const unsigned char ISAPNP_sysdev_Mouse[] = {
	ISAPNP_SYSDEV_HEADER(
			ISAPNP_ID('P','N','P',0x0,0xF,0x0,0xE), /* PNP0F0E Microsoft compatible PS/2 */
			ISAPNP_TYPE(0x09,0x02,0x00),		/* type: input, keyboard */
			0x0001 | 0x0002),			/* can't disable, can't configure */
	/*----------allocated--------*/
	ISAPNP_IRQ_SINGLE(
			12,					/* IRQ 12 */
			0x09),					/* HTE=1 LTL=1 */
	ISAPNP_END,
	/*----------possible--------*/
	ISAPNP_END,
	/*----------compatible--------*/
	ISAPNP_END
};

static const unsigned char ISAPNP_sysdev_DMA_Controller[] = {
	ISAPNP_SYSDEV_HEADER(
			ISAPNP_ID('P','N','P',0x0,0x2,0x0,0x0), /* PNP0200 AT DMA controller */
			ISAPNP_TYPE(0x08,0x01,0x00),		/* type: input, keyboard */
			0x0001 | 0x0002),			/* can't disable, can't configure */
	/*----------allocated--------*/
	ISAPNP_IO_RANGE(
			0x01,					/* decodes 16-bit ISA addr */
			0x00,0x00,				/* min-max range I/O port (DMA channels 0-3) */
			0x10,0x10),				/* align=16 length=16 */
	ISAPNP_IO_RANGE(
			0x01,					/* decodes 16-bit ISA addr */
			0x81,0x81,				/* min-max range I/O port (DMA page registers) */
			0x01,0x0F),				/* align=1 length=15 */
	ISAPNP_IO_RANGE(
			0x01,					/* decodes 16-bit ISA addr */
			0xC0,0xC0,				/* min-max range I/O port (AT DMA channels 4-7) */
			0x20,0x20),				/* align=32 length=32 */
	ISAPNP_DMA_SINGLE(
			4,					/* DMA 4 */
			0x01),					/* 8/16-bit transfers, compatible speed */
	ISAPNP_END,
	/*----------possible--------*/
	ISAPNP_END,
	/*----------compatible--------*/
	ISAPNP_END
};

static const unsigned char ISAPNP_sysdev_PIC[] = {
	ISAPNP_SYSDEV_HEADER(
			ISAPNP_ID('P','N','P',0x0,0x0,0x0,0x0), /* PNP0000 Interrupt controller */
			ISAPNP_TYPE(0x08,0x00,0x01),		/* type: ISA interrupt controller */
			0x0001 | 0x0002),			/* can't disable, can't configure */
	/*----------allocated--------*/
	ISAPNP_IO_RANGE(
			0x01,					/* decodes 16-bit ISA addr */
			0x20,0x20,				/* min-max range I/O port */
			0x01,0x02),				/* align=1 length=2 */
	ISAPNP_IO_RANGE(
			0x01,					/* decodes 16-bit ISA addr */
			0xA0,0xA0,				/* min-max range I/O port */
			0x01,0x02),				/* align=1 length=2 */
	ISAPNP_IRQ_SINGLE(
			2,					/* IRQ 2 */
			0x09),					/* HTE=1 LTL=1 */
	ISAPNP_END,
	/*----------possible--------*/
	ISAPNP_END,
	/*----------compatible--------*/
	ISAPNP_END
};

static const unsigned char ISAPNP_sysdev_Timer[] = {
	ISAPNP_SYSDEV_HEADER(
			ISAPNP_ID('P','N','P',0x0,0x1,0x0,0x0), /* PNP0100 Timer */
			ISAPNP_TYPE(0x08,0x02,0x01),		/* type: ISA timer */
			0x0001 | 0x0002),			/* can't disable, can't configure */
	/*----------allocated--------*/
	ISAPNP_IO_RANGE(
			0x01,					/* decodes 16-bit ISA addr */
			0x40,0x40,				/* min-max range I/O port */
			0x04,0x04),				/* align=4 length=4 */
	ISAPNP_IRQ_SINGLE(
			0,					/* IRQ 0 */
			0x09),					/* HTE=1 LTL=1 */
	ISAPNP_END,
	/*----------possible--------*/
	ISAPNP_END,
	/*----------compatible--------*/
	ISAPNP_END
};

static const unsigned char ISAPNP_sysdev_RTC[] = {
	ISAPNP_SYSDEV_HEADER(
			ISAPNP_ID('P','N','P',0x0,0xB,0x0,0x0), /* PNP0B00 Real-time clock */
			ISAPNP_TYPE(0x08,0x03,0x01),		/* type: ISA real-time clock */
			0x0001 | 0x0002),			/* can't disable, can't configure */
	/*----------allocated--------*/
	ISAPNP_IO_RANGE(
			0x01,					/* decodes 16-bit ISA addr */
			0x70,0x70,				/* min-max range I/O port */
			0x01,0x02),				/* align=1 length=2 */
	ISAPNP_IRQ_SINGLE(
			8,					/* IRQ 8 */
			0x09),					/* HTE=1 LTL=1 */
	ISAPNP_END,
	/*----------possible--------*/
	ISAPNP_END,
	/*----------compatible--------*/
	ISAPNP_END
};

static const unsigned char ISAPNP_sysdev_PC_Speaker[] = {
	ISAPNP_SYSDEV_HEADER(
			ISAPNP_ID('P','N','P',0x0,0x8,0x0,0x0), /* PNP0800 PC speaker */
			ISAPNP_TYPE(0x04,0x01,0x00),		/* type: PC speaker */
			0x0001 | 0x0002),			/* can't disable, can't configure */
	/*----------allocated--------*/
	ISAPNP_IO_RANGE(
			0x01,					/* decodes 16-bit ISA addr */
			0x61,0x61,				/* min-max range I/O port */
			0x01,0x01),				/* align=1 length=1 */
	ISAPNP_END,
	/*----------possible--------*/
	ISAPNP_END,
	/*----------compatible--------*/
	ISAPNP_END
};

static const unsigned char ISAPNP_sysdev_Numeric_Coprocessor[] = {
	ISAPNP_SYSDEV_HEADER(
			ISAPNP_ID('P','N','P',0x0,0xC,0x0,0x4), /* PNP0C04 Numeric Coprocessor */
			ISAPNP_TYPE(0x0B,0x80,0x00),		/* type: FPU */
			0x0001 | 0x0002),			/* can't disable, can't configure */
	/*----------allocated--------*/
	ISAPNP_IO_RANGE(
			0x01,					/* decodes 16-bit ISA addr */
			0xF0,0xF0,				/* min-max range I/O port */
			0x10,0x10),				/* align=16 length=16 */
	ISAPNP_IRQ_SINGLE(
			13,					/* IRQ 13 */
			0x09),					/* HTE=1 LTL=1 */
	ISAPNP_END,
	/*----------possible--------*/
	ISAPNP_END,
	/*----------compatible--------*/
	ISAPNP_END
};

static const unsigned char ISAPNP_sysdev_System_Board[] = {
	ISAPNP_SYSDEV_HEADER(
			ISAPNP_ID('P','N','P',0x0,0xC,0x0,0x1), /* PNP0C01 System board */
			ISAPNP_TYPE(0x08,0x80,0x00),		/* type: System peripheral, Other */
			0x0001 | 0x0002),			/* can't disable, can't configure */
	/*----------allocated--------*/
	ISAPNP_IO_RANGE(
			0x01,					/* decodes 16-bit ISA addr */
			0x24,0x24,				/* min-max range I/O port */
			0x01,0x0C),				/* align=1 length=12 */
	ISAPNP_END,
	/*----------possible--------*/
	ISAPNP_END,
	/*----------compatible--------*/
	ISAPNP_END
};

static ISAPNP_SysDevNode*	ISAPNP_SysDevNodes[256];
static Bitu			ISAPNP_SysDevNodeCount=0;
static Bitu			ISAPNP_SysDevNodeLargest=0;

bool ISAPNP_RegisterSysDev(const unsigned char *raw,int len,bool already) {
	if (ISAPNP_SysDevNodeCount >= 0xFF)
		return false;

	ISAPNP_SysDevNodes[ISAPNP_SysDevNodeCount] = new ISAPNP_SysDevNode(raw,len,already);
	if (ISAPNP_SysDevNodes[ISAPNP_SysDevNodeCount] == NULL)
		return false;
	
	ISAPNP_SysDevNodeCount++;
	if (ISAPNP_SysDevNodeLargest < (len+3))
		ISAPNP_SysDevNodeLargest = len+3;

	return true;
}

/* ISA PnP function calls have their parameters stored on the stack "C" __cdecl style. Parameters
 * are either int, long, or FAR pointers. Like __cdecl an assembly language implementation pushes
 * the function arguments on the stack BACKWARDS */
static Bitu ISAPNP_Handler(bool protmode /* called from protected mode interface == true */) {
	Bitu arg;
	Bitu func,BiosSelector;

	/* I like how the ISA PnP spec says that the 16-bit entry points (real and protected) are given 16-bit data segments
	 * which implies that all segments involved might as well be 16-bit.
	 *
	 * Right?
	 *
	 * Well, guess what Windows 95 gives us when calling this entry point:
	 *
	 *     Segment SS = DS = 0x30  base=0 limit=0xFFFFFFFF
	 *       SS:SP = 0x30:0xC138BADF or something like that from within BIOS.VXD
	 *
	 * Yeah... for a 16-bit code segment call. Right. Typical Microsoft. >:(
	 *
	 * ------------------------------------------------------------------------
	 * Windows 95 OSR2:
	 *
	 * Windows 95 OSR2 however uses a 16-bit stack (where the stack segment is based somewhere
	 * around 0xC1xxxxxx), all we have to do to correctly access it is work through the page tables.
	 * This is within spec, but now Microsoft sends us a data segment that is based at virtual address
	 * 0xC2xxxxxx, which is why I had to disable the "verify selector" routine */
	if (protmode)
		arg = SegPhys(ss) + reg_esp + (2*2); /* entry point (real and protected) is 16-bit, expected to RETF (skip CS:IP) */
	else
		arg = SegPhys(ss) + reg_sp + (2*2); /* entry point (real and protected) is 16-bit, expected to RETF (skip CS:IP) */

	if (protmode != ISAPNP_CPU_ProtMode()) {
		//fprintf(stderr,"ISA PnP %s entry point called from %s. On real BIOSes this would CRASH\n",protmode ? "Protected mode" : "Real mode",
		//	ISAPNP_CPU_ProtMode() ? "Protected mode" : "Real mode");
		reg_ax = 0x84;/* BAD_PARAMETER */
		return 0;
	}

	//fprintf(stderr,"PnP prot=%u DS=%04x (base=0x%08lx) SS:ESP=%04x:%04x (base=0x%08lx phys=0x%08lx)\n",protmode,
	//	SegValue(ds),SegPhys(ds),
	//	SegValue(ss),reg_esp,SegPhys(ss),arg);

	/* every function takes the form
	 *
	 * int __cdecl FAR (*entrypoint)(int Function...);
	 *
	 * so the first argument on the stack is an int that we read to determine what the caller is asking
	 *
	 * Dont forget in the real-mode world:
	 *    sizeof(int) == 16 bits
	 *    sizeof(long) == 32 bits
	 */    
	func = mem_readw(arg);
	switch (func) {
		case 0: {		/* Get Number of System Nodes */
			/* int __cdecl FAR (*entrypoint)(int Function,unsigned char FAR *NumNodes,unsigned int FAR *NodeSize,unsigned int BiosSelector);
			 *                               ^ +0         ^ +2                        ^ +6                       ^ +10                       = 12 */
			Bitu NumNodes_ptr = mem_readd(arg+2);
			Bitu NodeSize_ptr = mem_readd(arg+6);
			BiosSelector = mem_readw(arg+10);

			if (!ISAPNP_Verify_BiosSelector(BiosSelector))
				goto badBiosSelector;

			if (NumNodes_ptr != 0) mem_writeb(ISAPNP_xlate_address(NumNodes_ptr),ISAPNP_SysDevNodeCount);
			if (NodeSize_ptr != 0) mem_writew(ISAPNP_xlate_address(NodeSize_ptr),ISAPNP_SysDevNodeLargest);

			reg_ax = 0x00;/* SUCCESS */
		} break;
		case 1: {		/* Get System Device Node */
			/* int __cdecl FAR (*entrypoint)(int Function,unsigned char FAR *Node,struct DEV_NODE FAR *devNodeBuffer,unsigned int Control,unsigned int BiosSelector);
			 *                               ^ +0         ^ +2                    ^ +6                               ^ +10                ^ +12                       = 14 */
			Bitu Node_ptr = mem_readd(arg+2);
			Bitu devNodeBuffer_ptr = mem_readd(arg+6);
			Bitu Control = mem_readw(arg+10);
			BiosSelector = mem_readw(arg+12);
			unsigned char Node;
			Bitu i;

			if (!ISAPNP_Verify_BiosSelector(BiosSelector))
				goto badBiosSelector;

			/* control bits 0-1 must be '01' or '10' but not '00' or '11' */
			if (Control == 0 || (Control&3) == 3) {
				reg_ax = 0x84;/* BAD_PARAMETER */
				break;
			}

			//fprintf(stderr,"devNodePtr = 0x%08lx\n",devNodeBuffer_ptr);
			devNodeBuffer_ptr = ISAPNP_xlate_address(devNodeBuffer_ptr);
			//fprintf(stderr,"        to = 0x%08lx\n",devNodeBuffer_ptr);

			Node_ptr = ISAPNP_xlate_address(Node_ptr);
			Node = mem_readb(Node_ptr);
			if (Node >= ISAPNP_SysDevNodeCount) {
				reg_ax = 0x84;/* BAD_PARAMETER */
				break;
			}

			ISAPNP_SysDevNode *nd = ISAPNP_SysDevNodes[Node];

			mem_writew(devNodeBuffer_ptr+i+0,nd->raw_len+3); /* Length */
			mem_writeb(devNodeBuffer_ptr+i+2,Node); /* on most PnP BIOS implementations I've seen "handle" is set to the same value as Node */
			for (i=0;i < nd->raw_len;i++)
				mem_writeb(devNodeBuffer_ptr+i+3,nd->raw[i]);

			if (++Node >= ISAPNP_SysDevNodeCount) Node = 0xFF; /* no more nodes */
			mem_writeb(Node_ptr,Node);

			reg_ax = 0x00;/* SUCCESS */
		} break;
		case 4: {		/* Send Message */
			/* int __cdecl FAR (*entrypoint)(int Function,unsigned int Message,unsigned int BiosSelector);
			 *                               ^ +0         ^ +2                 ^ +4                        = 6 */
			Bitu Message = mem_readw(arg+2);
			BiosSelector = mem_readw(arg+4);

			if (!ISAPNP_Verify_BiosSelector(BiosSelector))
				goto badBiosSelector;

			switch (Message) {
				case 0x41:	/* POWER_OFF */
					//fprintf(stderr,"Plug & Play OS requested power off.\n");
					throw 1;	/* NTS: Based on the Reboot handler code, causes DOSBox to cleanly shutdown and exit */
					reg_ax = 0;
					break;
				case 0x42:	/* PNP_OS_ACTIVE */
					//fprintf(stderr,"Plug & Play OS reports itself active\n");
					reg_ax = 0;
					break;
				case 0x43:	/* PNP_OS_INACTIVE */
					//fprintf(stderr,"Plug & Play OS reports itself inactive\n");
					reg_ax = 0;
					break;
				default:
					//fprintf(stderr,"Unknown ISA PnP message 0x%04x\n",Message);
					reg_ax = 0x82;/* FUNCTION_NOT_SUPPORTED */
					break;
			}
		} break;
		case 0x40: {		/* Get PnP ISA configuration */
			/* int __cdecl FAR (*entrypoint)(int Function,unsigned char far *struct,unsigned int BiosSelector);
			 *                               ^ +0         ^ +2                      ^ +6                        = 8 */
			Bitu struct_ptr = mem_readd(arg+2);
			BiosSelector = mem_readw(arg+6);

			if (!ISAPNP_Verify_BiosSelector(BiosSelector))
				goto badBiosSelector;

			/* struct isapnp_pnp_isa_cfg {
				 uint8_t	revision;
				 uint8_t	total_csn;
				 uint16_t	isa_pnp_port;
				 uint16_t	reserved;
			 }; */

			if (struct_ptr != 0) {
				Bitu ph = ISAPNP_xlate_address(struct_ptr);
				mem_writeb(ph+0,0x01);		/* ->revision = 0x01 */
				mem_writeb(ph+1,ISA_PNP_devnext); /* ->total_csn (FIXME) */
				mem_writew(ph+2,ISA_PNP_WPORT_BIOS);	/* ->isa_pnp_port */
				mem_writew(ph+4,0);		/* ->reserved */
			}

			reg_ax = 0x00;/* SUCCESS */
		} break;
		default:
			//fprintf(stderr,"Unsupported ISA PnP function 0x%04x\n",func);
			reg_ax = 0x82;/* FUNCTION_NOT_SUPPORTED */
			break;
	};

	return 0;
badBiosSelector:
	/* return an error. remind the user (possible developer) how lucky he is, a real
	 * BIOS implementation would CRASH when misused like this */
	//fprintf(stderr,"ISA PnP function 0x%04x called with incorrect BiosSelector parameter 0x%04x\n",func,BiosSelector);
	//fprintf(stderr," > STACK %04X %04X %04X %04X %04X %04X %04X %04X\n",
	//	mem_readw(arg),
	//	mem_readw(arg+2),
	//	mem_readw(arg+4),
	//	mem_readw(arg+6),
	//	mem_readw(arg+8),
	//	mem_readw(arg+10),
	//	mem_readw(arg+12),
	//	mem_readw(arg+14));

	if (cpu.pmode && !(reg_flags & FLAG_VM) && BiosSelector != 0) {
		Descriptor desc;
		if (cpu.gdt.GetDescriptor(BiosSelector,desc)) {
		//	fprintf(stderr," > BiosSelector base=0x%08lx\n",(unsigned long)desc.GetBase());
		} else {
		//	fprintf(stderr," > BiosSelector N/A\n");
		}
	}

	reg_ax = 0x84;/* BAD_PARAMETER */
	return 0;
}

static Bitu ISAPNP_Handler_PM(void) {
	return ISAPNP_Handler(true);
}

static Bitu ISAPNP_Handler_RM(void) {
	return ISAPNP_Handler(false);
}

static Bitu INT70_Handler(void) {
	/* Acknowledge irq with cmos */
	IO_Write(0x70,0xc);
	IO_Read(0x71);
	if (mem_readb(BIOS_WAIT_FLAG_ACTIVE)) {
		Bit32u count=mem_readd(BIOS_WAIT_FLAG_COUNT);
		if (count>997) {
			mem_writed(BIOS_WAIT_FLAG_COUNT,count-997);
		} else {
			mem_writed(BIOS_WAIT_FLAG_COUNT,0);
			PhysPt where=Real2Phys(mem_readd(BIOS_WAIT_FLAG_POINTER));
			mem_writeb(where,mem_readb(where)|0x80);
			mem_writeb(BIOS_WAIT_FLAG_ACTIVE,0);
			mem_writed(BIOS_WAIT_FLAG_POINTER,RealMake(0,BIOS_WAIT_FLAG_TEMP));
			IO_Write(0x70,0xb);
			IO_Write(0x71,IO_Read(0x71)&~0x40);
		}
	} 
	/* Signal EOI to both pics */
	IO_Write(0xa0,0x20);
	IO_Write(0x20,0x20);
	return 0;
}

CALLBACK_HandlerObject* tandy_DAC_callback[2];
static struct {
	Bit16u port;
	Bit8u irq;
	Bit8u dma;
} tandy_sb;
static struct {
	Bit16u port;
	Bit8u irq;
	Bit8u dma;
} tandy_dac;

static bool Tandy_InitializeSB() {
	/* see if soundblaster module available and at what port/IRQ/DMA */
	Bitu sbport, sbirq, sbdma;
	if (SB_Get_Address(sbport, sbirq, sbdma)) {
		tandy_sb.port=(Bit16u)(sbport&0xffff);
		tandy_sb.irq =(Bit8u)(sbirq&0xff);
		tandy_sb.dma =(Bit8u)(sbdma&0xff);
		return true;
	} else {
		/* no soundblaster accessible, disable Tandy DAC */
		tandy_sb.port=0;
		return false;
	}
}

static bool Tandy_InitializeTS() {
	/* see if Tandy DAC module available and at what port/IRQ/DMA */
	Bitu tsport, tsirq, tsdma;
	if (TS_Get_Address(tsport, tsirq, tsdma)) {
		tandy_dac.port=(Bit16u)(tsport&0xffff);
		tandy_dac.irq =(Bit8u)(tsirq&0xff);
		tandy_dac.dma =(Bit8u)(tsdma&0xff);
		return true;
	} else {
		/* no Tandy DAC accessible */
		tandy_dac.port=0;
		return false;
	}
}

/* check if Tandy DAC is still playing */
static bool Tandy_TransferInProgress(void) {
	if (real_readw(0x40,0xd0)) return true;			/* not yet done */
	if (real_readb(0x40,0xd4)==0xff) return false;	/* still in init-state */

	Bit8u tandy_dma = 1;
	if (tandy_sb.port) tandy_dma = tandy_sb.dma;
	else if (tandy_dac.port) tandy_dma = tandy_dac.dma;

	IO_Write(0x0c,0x00);
	Bit16u datalen=(Bit8u)(IO_ReadB(tandy_dma*2+1)&0xff);
	datalen|=(IO_ReadB(tandy_dma*2+1)<<8);
	if (datalen==0xffff) return false;	/* no DMA transfer */
	else if ((datalen<0x10) && (real_readb(0x40,0xd4)==0x0f) && (real_readw(0x40,0xd2)==0x1c)) {
		/* stop already requested */
		return false;
	}
	return true;
}

static void Tandy_SetupTransfer(PhysPt bufpt,bool isplayback) {
	Bitu length=real_readw(0x40,0xd0);
	if (length==0) return;	/* nothing to do... */

	if ((tandy_sb.port==0) && (tandy_dac.port==0)) return;

	Bit8u tandy_irq = 7;
	if (tandy_sb.port) tandy_irq = tandy_sb.irq;
	else if (tandy_dac.port) tandy_irq = tandy_dac.irq;
	Bit8u tandy_irq_vector = tandy_irq;
	if (tandy_irq_vector<8) tandy_irq_vector += 8;
	else tandy_irq_vector += (0x70-8);

	/* revector IRQ-handler if necessary */
	RealPt current_irq=RealGetVec(tandy_irq_vector);
	if (current_irq!=tandy_DAC_callback[0]->Get_RealPointer()) {
		real_writed(0x40,0xd6,current_irq);
		RealSetVec(tandy_irq_vector,tandy_DAC_callback[0]->Get_RealPointer());
	}

	Bit8u tandy_dma = 1;
	if (tandy_sb.port) tandy_dma = tandy_sb.dma;
	else if (tandy_dac.port) tandy_dma = tandy_dac.dma;

	if (tandy_sb.port) {
		IO_Write(tandy_sb.port+0xc,0xd0);				/* stop DMA transfer */
		IO_Write(0x21,IO_Read(0x21)&(~(1<<tandy_irq)));	/* unmask IRQ */
		IO_Write(tandy_sb.port+0xc,0xd1);				/* turn speaker on */
	} else {
		IO_Write(tandy_dac.port,IO_Read(tandy_dac.port)&0x60);	/* disable DAC */
		IO_Write(0x21,IO_Read(0x21)&(~(1<<tandy_irq)));			/* unmask IRQ */
	}

	IO_Write(0x0a,0x04|tandy_dma);	/* mask DMA channel */
	IO_Write(0x0c,0x00);			/* clear DMA flipflop */
	if (isplayback) IO_Write(0x0b,0x48|tandy_dma);
	else IO_Write(0x0b,0x44|tandy_dma);
	/* set physical address of buffer */
	Bit8u bufpage=(Bit8u)((bufpt>>16)&0xff);
	IO_Write(tandy_dma*2,(Bit8u)(bufpt&0xff));
	IO_Write(tandy_dma*2,(Bit8u)((bufpt>>8)&0xff));
	switch (tandy_dma) {
		case 0: IO_Write(0x87,bufpage); break;
		case 1: IO_Write(0x83,bufpage); break;
		case 2: IO_Write(0x81,bufpage); break;
		case 3: IO_Write(0x82,bufpage); break;
	}
	real_writeb(0x40,0xd4,bufpage);

	/* calculate transfer size (respects segment boundaries) */
	Bit32u tlength=length;
	if (tlength+(bufpt&0xffff)>0x10000) tlength=0x10000-(bufpt&0xffff);
	real_writew(0x40,0xd0,(Bit16u)(length-tlength));	/* remaining buffer length */
	tlength--;

	/* set transfer size */
	IO_Write(tandy_dma*2+1,(Bit8u)(tlength&0xff));
	IO_Write(tandy_dma*2+1,(Bit8u)((tlength>>8)&0xff));

	Bit16u delay=(Bit16u)(real_readw(0x40,0xd2)&0xfff);
	Bit8u amplitude=(Bit8u)((real_readw(0x40,0xd2)>>13)&0x7);
	if (tandy_sb.port) {
		IO_Write(0x0a,tandy_dma);	/* enable DMA channel */
		/* set frequency */
		IO_Write(tandy_sb.port+0xc,0x40);
		IO_Write(tandy_sb.port+0xc,256-delay*100/358);
		/* set playback type to 8bit */
		if (isplayback) IO_Write(tandy_sb.port+0xc,0x14);
		else IO_Write(tandy_sb.port+0xc,0x24);
		/* set transfer size */
		IO_Write(tandy_sb.port+0xc,(Bit8u)(tlength&0xff));
		IO_Write(tandy_sb.port+0xc,(Bit8u)((tlength>>8)&0xff));
	} else {
		if (isplayback) IO_Write(tandy_dac.port,(IO_Read(tandy_dac.port)&0x7c) | 0x03);
		else IO_Write(tandy_dac.port,(IO_Read(tandy_dac.port)&0x7c) | 0x02);
		IO_Write(tandy_dac.port+2,(Bit8u)(delay&0xff));
		IO_Write(tandy_dac.port+3,(Bit8u)(((delay>>8)&0xf) | (amplitude<<5)));
		if (isplayback) IO_Write(tandy_dac.port,(IO_Read(tandy_dac.port)&0x7c) | 0x1f);
		else IO_Write(tandy_dac.port,(IO_Read(tandy_dac.port)&0x7c) | 0x1e);
		IO_Write(0x0a,tandy_dma);	/* enable DMA channel */
	}

	if (!isplayback) {
		/* mark transfer as recording operation */
		real_writew(0x40,0xd2,(Bit16u)(delay|0x1000));
	}
}

static Bitu IRQ_TandyDAC(void) {
	if (tandy_dac.port) {
		IO_Read(tandy_dac.port);
	}
	if (real_readw(0x40,0xd0)) {	/* play/record next buffer */
		/* acknowledge IRQ */
		IO_Write(0x20,0x20);
		if (tandy_sb.port) {
			IO_Read(tandy_sb.port+0xe);
		}

		/* buffer starts at the next page */
		Bit8u npage=real_readb(0x40,0xd4)+1;
		real_writeb(0x40,0xd4,npage);

		Bitu rb=real_readb(0x40,0xd3);
		if (rb&0x10) {
			/* start recording */
			real_writeb(0x40,0xd3,rb&0xef);
			Tandy_SetupTransfer(npage<<16,false);
		} else {
			/* start playback */
			Tandy_SetupTransfer(npage<<16,true);
		}
	} else {	/* playing/recording is finished */
		Bit8u tandy_irq = 7;
		if (tandy_sb.port) tandy_irq = tandy_sb.irq;
		else if (tandy_dac.port) tandy_irq = tandy_dac.irq;
		Bit8u tandy_irq_vector = tandy_irq;
		if (tandy_irq_vector<8) tandy_irq_vector += 8;
		else tandy_irq_vector += (0x70-8);

		RealSetVec(tandy_irq_vector,real_readd(0x40,0xd6));

		/* turn off speaker and acknowledge soundblaster IRQ */
		if (tandy_sb.port) {
			IO_Write(tandy_sb.port+0xc,0xd3);
			IO_Read(tandy_sb.port+0xe);
		}

		/* issue BIOS tandy sound device busy callout */
		SegSet16(cs, RealSeg(tandy_DAC_callback[1]->Get_RealPointer()));
		reg_ip = RealOff(tandy_DAC_callback[1]->Get_RealPointer());
	}
	return CBRET_NONE;
}

static void TandyDAC_Handler(Bit8u tfunction) {
	if ((!tandy_sb.port) && (!tandy_dac.port)) return;
	switch (tfunction) {
	case 0x81:	/* Tandy sound system check */
		if (tandy_dac.port) {
			reg_ax=tandy_dac.port;
		} else {
			reg_ax=0xc4;
		}
		CALLBACK_SCF(Tandy_TransferInProgress());
		break;
	case 0x82:	/* Tandy sound system start recording */
	case 0x83:	/* Tandy sound system start playback */
		if (Tandy_TransferInProgress()) {
			/* cannot play yet as the last transfer isn't finished yet */
			reg_ah=0x00;
			CALLBACK_SCF(true);
			break;
		}
		/* store buffer length */
		real_writew(0x40,0xd0,reg_cx);
		/* store delay and volume */
		real_writew(0x40,0xd2,(reg_dx&0xfff)|((reg_al&7)<<13));
		Tandy_SetupTransfer(PhysMake(SegValue(es),reg_bx),reg_ah==0x83);
		reg_ah=0x00;
		CALLBACK_SCF(false);
		break;
	case 0x84:	/* Tandy sound system stop playing */
		reg_ah=0x00;

		/* setup for a small buffer with silence */
		real_writew(0x40,0xd0,0x0a);
		real_writew(0x40,0xd2,0x1c);
		Tandy_SetupTransfer(PhysMake(0xf000,0xa084),true);
		CALLBACK_SCF(false);
		break;
	case 0x85:	/* Tandy sound system reset */
		if (tandy_dac.port) {
			IO_Write(tandy_dac.port,(Bit8u)(IO_Read(tandy_dac.port)&0xe0));
		}
		reg_ah=0x00;
		CALLBACK_SCF(false);
		break;
	}
}

extern bool date_host_forced;
static Bit8u ReadCmosByte (Bitu index) {
	IO_Write(0x70, index);
	return IO_Read(0x71);
}

static void WriteCmosByte (Bitu index, Bitu val) {
	IO_Write(0x70, index);
	IO_Write(0x71, val);
}

static bool RtcUpdateDone () {
	while ((ReadCmosByte(0x0a) & 0x80) != 0) CALLBACK_Idle();
	return true;			// cannot fail in DOSbox
}

static void InitRtc () {
	WriteCmosByte(0x0a, 0x26);		// default value (32768Hz, 1024Hz)

	// leave bits 6 (pirq), 5 (airq), 0 (dst) untouched
	// reset bits 7 (freeze), 4 (uirq), 3 (sqw), 2 (bcd)
	// set bit 1 (24h)
	WriteCmosByte(0x0b, (ReadCmosByte(0x0b) & 0x61) | 0x02);

	ReadCmosByte(0x0c);				// clear any bits set
}

static Bitu INT1A_Handler(void) {
	CALLBACK_SIF(true);
	switch (reg_ah) {
	case 0x00:	/* Get System time */
		{
			Bit32u ticks=mem_readd(BIOS_TIMER);
			reg_al=mem_readb(BIOS_24_HOURS_FLAG);
			mem_writeb(BIOS_24_HOURS_FLAG,0); // reset the "flag"
			reg_cx=(Bit16u)(ticks >> 16);
			reg_dx=(Bit16u)(ticks & 0xffff);
			break;
		}
	case 0x01:	/* Set System time */
		mem_writed(BIOS_TIMER,(reg_cx<<16)|reg_dx);
		break;
	case 0x02:	/* GET REAL-TIME CLOCK TIME (AT,XT286,PS) */
		if(date_host_forced) {
			InitRtc();							// make sure BCD and no am/pm
			if (RtcUpdateDone()) {				// make sure it's safe to read
				reg_ch = ReadCmosByte(0x04);	// hours
				reg_cl = ReadCmosByte(0x02);	// minutes
				reg_dh = ReadCmosByte(0x00);	// seconds
				reg_dl = ReadCmosByte(0x0b) & 0x01;	// daylight saving time
			}
			CALLBACK_SCF(false);
			break;
		}
		IO_Write(0x70,0x04);		//Hours
		reg_ch=IO_Read(0x71);
		IO_Write(0x70,0x02);		//Minutes
		reg_cl=IO_Read(0x71);
		IO_Write(0x70,0x00);		//Seconds
		reg_dh=IO_Read(0x71);
		reg_dl=0;			//Daylight saving disabled
		CALLBACK_SCF(false);
		break;
	case 0x03:	// set RTC time
		if(date_host_forced) {
			InitRtc();							// make sure BCD and no am/pm
			WriteCmosByte(0x0b, ReadCmosByte(0x0b) | 0x80);		// prohibit updates
			WriteCmosByte(0x04, reg_ch);		// hours
			WriteCmosByte(0x02, reg_cl);		// minutes
			WriteCmosByte(0x00, reg_dh);		// seconds
			WriteCmosByte(0x0b, (ReadCmosByte(0x0b) & 0x7e) | (reg_dh & 0x01));	// dst + implicitly allow updates
		}
		break;
	case 0x04:	/* GET REAL-TIME ClOCK DATE  (AT,XT286,PS) */
		if(date_host_forced) {
			InitRtc();							// make sure BCD and no am/pm
			if (RtcUpdateDone()) {				// make sure it's safe to read
				reg_ch = ReadCmosByte(0x32);	// century
				reg_cl = ReadCmosByte(0x09);	// year
				reg_dh = ReadCmosByte(0x08);	// month
				reg_dl = ReadCmosByte(0x07);	// day
			}
			CALLBACK_SCF(false);
			break;
		}
		IO_Write(0x70,0x32);		//Centuries
		reg_ch=IO_Read(0x71);
		IO_Write(0x70,0x09);		//Years
		reg_cl=IO_Read(0x71);
		IO_Write(0x70,0x08);		//Months
		reg_dh=IO_Read(0x71);
		IO_Write(0x70,0x07);		//Days
		reg_dl=IO_Read(0x71);
		CALLBACK_SCF(false);
		break;
	case 0x05:	// set RTC date
		if(date_host_forced) {
			InitRtc();							// make sure BCD and no am/pm
			WriteCmosByte(0x0b, ReadCmosByte(0x0b) | 0x80);		// prohibit updates
			WriteCmosByte(0x32, reg_ch);	// century
			WriteCmosByte(0x09, reg_cl);	// year
			WriteCmosByte(0x08, reg_dh);	// month
			WriteCmosByte(0x07, reg_dl);	// day
			WriteCmosByte(0x0b, (ReadCmosByte(0x0b) & 0x7f));	// allow updates
		}
		break;
	case 0x80:	/* Pcjr Setup Sound Multiplexer */
		LOG(LOG_BIOS,LOG_ERROR)("INT1A:80:Setup tandy sound multiplexer to %d",reg_al);
		break;
	case 0x81:	/* Tandy sound system check */
	case 0x82:	/* Tandy sound system start recording */
	case 0x83:	/* Tandy sound system start playback */
	case 0x84:	/* Tandy sound system stop playing */
	case 0x85:	/* Tandy sound system reset */
		TandyDAC_Handler(reg_ah);
		break;
	case 0xb1:		/* PCI Bios Calls */
		LOG(LOG_BIOS,LOG_WARN)("INT1A:PCI bios call %2X",reg_al);
#if defined(PCI_FUNCTIONALITY_ENABLED)
		switch (reg_al) {
			case 0x01:	// installation check
				if (PCI_IsInitialized()) {
					reg_ah=0x00;
					reg_al=0x01;	// cfg space mechanism 1 supported
					reg_bx=0x0210;	// ver 2.10
					reg_cx=0x0000;	// only one PCI bus
					reg_edx=0x20494350;
					reg_edi=PCI_GetPModeInterface();
					CALLBACK_SCF(false);
				} else {
					CALLBACK_SCF(true);
				}
				break;
			case 0x02: {	// find device
				Bitu devnr=0;
				Bitu count=0x100;
				Bit32u devicetag=(reg_cx<<16)|reg_dx;
				Bits found=-1;
				for (Bitu i=0; i<=count; i++) {
					IO_WriteD(0xcf8,0x80000000|(i<<8));	// query unique device/subdevice entries
					if (IO_ReadD(0xcfc)==devicetag) {
						if (devnr==reg_si) {
							found=i;
							break;
						} else {
							// device found, but not the SIth device
							devnr++;
						}
					}
				}
				if (found>=0) {
					reg_ah=0x00;
					reg_bh=0x00;	// bus 0
					reg_bl=(Bit8u)(found&0xff);
					CALLBACK_SCF(false);
				} else {
					reg_ah=0x86;	// device not found
					CALLBACK_SCF(true);
				}
				}
				break;
			case 0x03: {	// find device by class code
				Bitu devnr=0;
				Bitu count=0x100;
				Bit32u classtag=reg_ecx&0xffffff;
				Bits found=-1;
				for (Bitu i=0; i<=count; i++) {
					IO_WriteD(0xcf8,0x80000000|(i<<8));	// query unique device/subdevice entries
					if (IO_ReadD(0xcfc)!=0xffffffff) {
						IO_WriteD(0xcf8,0x80000000|(i<<8)|0x08);
						if ((IO_ReadD(0xcfc)>>8)==classtag) {
							if (devnr==reg_si) {
								found=i;
								break;
							} else {
								// device found, but not the SIth device
								devnr++;
							}
						}
					}
				}
				if (found>=0) {
					reg_ah=0x00;
					reg_bh=0x00;	// bus 0
					reg_bl=(Bit8u)(found&0xff);
					CALLBACK_SCF(false);
				} else {
					reg_ah=0x86;	// device not found
					CALLBACK_SCF(true);
				}
				}
				break;
			case 0x08:	// read configuration byte
				IO_WriteD(0xcf8,0x80000000|(reg_bx<<8)|(reg_di&0xfc));
				reg_cl=IO_ReadB(0xcfc+(reg_di&3));
				CALLBACK_SCF(false);
				break;
			case 0x09:	// read configuration word
				IO_WriteD(0xcf8,0x80000000|(reg_bx<<8)|(reg_di&0xfc));
				reg_cx=IO_ReadW(0xcfc+(reg_di&2));
				CALLBACK_SCF(false);
				break;
			case 0x0a:	// read configuration dword
				IO_WriteD(0xcf8,0x80000000|(reg_bx<<8)|(reg_di&0xfc));
				reg_ecx=IO_ReadD(0xcfc+(reg_di&3));
				CALLBACK_SCF(false);
				break;
			case 0x0b:	// write configuration byte
				IO_WriteD(0xcf8,0x80000000|(reg_bx<<8)|(reg_di&0xfc));
				IO_WriteB(0xcfc+(reg_di&3),reg_cl);
				CALLBACK_SCF(false);
				break;
			case 0x0c:	// write configuration word
				IO_WriteD(0xcf8,0x80000000|(reg_bx<<8)|(reg_di&0xfc));
				IO_WriteW(0xcfc+(reg_di&2),reg_cx);
				CALLBACK_SCF(false);
				break;
			case 0x0d:	// write configuration dword
				IO_WriteD(0xcf8,0x80000000|(reg_bx<<8)|(reg_di&0xfc));
				IO_WriteD(0xcfc+(reg_di&3),reg_ecx);
				CALLBACK_SCF(false);
				break;
			default:
				LOG(LOG_BIOS,LOG_ERROR)("INT1A:PCI BIOS: unknown function %x (%x %x %x)",
					reg_ax,reg_bx,reg_cx,reg_dx);
				CALLBACK_SCF(true);
				break;
		}
#else
		CALLBACK_SCF(true);
#endif
		break;
	default:
		LOG(LOG_BIOS,LOG_ERROR)("INT1A:Undefined call %2X",reg_ah);
	}
	return CBRET_NONE;
}	

static Bitu INT11_Handler(void) {
	reg_ax=mem_readw(BIOS_CONFIGURATION);
	return CBRET_NONE;
}
/* 
 * Define the following define to 1 if you want dosbox to check 
 * the system time every 5 seconds and adjust 1/2 a second to sync them.
 */
#ifndef DOSBOX_CLOCKSYNC
#define DOSBOX_CLOCKSYNC 0
#endif

static void BIOS_HostTimeSync() {
	/* Setup time and date */
	struct timeb timebuffer;
	ftime(&timebuffer);
	
	struct tm *loctime;
	loctime = localtime (&timebuffer.time);

	/*
	loctime->tm_hour = 23;
	loctime->tm_min = 59;
	loctime->tm_sec = 45;
	loctime->tm_mday = 28;
	loctime->tm_mon = 2-1;
	loctime->tm_year = 2007 - 1900;
	*/

	dos.date.day=(Bit8u)loctime->tm_mday;
	dos.date.month=(Bit8u)loctime->tm_mon+1;
	dos.date.year=(Bit16u)loctime->tm_year+1900;

	Bit32u ticks=(Bit32u)(((double)(
		loctime->tm_hour*3600*1000+
		loctime->tm_min*60*1000+
		loctime->tm_sec*1000+
		timebuffer.millitm))*(((double)PIT_TICK_RATE/65536.0)/1000.0));
	mem_writed(BIOS_TIMER,ticks);
}

static Bitu INT8_Handler(void) {
	/* Increase the bios tick counter */
	Bit32u value = mem_readd(BIOS_TIMER) + 1;
	if(value >= 0x1800B0) {
		// time wrap at midnight
		mem_writeb(BIOS_24_HOURS_FLAG,mem_readb(BIOS_24_HOURS_FLAG)+1);
		value=0;
	}

#if DOSBOX_CLOCKSYNC
	static bool check = false;
	if((value %50)==0) {
		if(((value %100)==0) && check) {
			check = false;
			time_t curtime;struct tm *loctime;
			curtime = time (NULL);loctime = localtime (&curtime);
			Bit32u ticksnu = (Bit32u)((loctime->tm_hour*3600+loctime->tm_min*60+loctime->tm_sec)*(float)PIT_TICK_RATE/65536.0);
			Bit32s bios = value;Bit32s tn = ticksnu;
			Bit32s diff = tn - bios;
			if(diff>0) {
				if(diff < 18) { diff  = 0; } else diff = 9;
			} else {
				if(diff > -18) { diff = 0; } else diff = -9;
			}
	     
			value += diff;
		} else if((value%100)==50) check = true;
	}
#endif
	mem_writed(BIOS_TIMER,value);

	/* decrease floppy motor timer */
	Bit8u val = mem_readb(BIOS_DISK_MOTOR_TIMEOUT);
	if (val) mem_writeb(BIOS_DISK_MOTOR_TIMEOUT,val-1);
	/* and running drive */
	mem_writeb(BIOS_DRIVE_RUNNING,mem_readb(BIOS_DRIVE_RUNNING) & 0xF0);
	return CBRET_NONE;
}
#undef DOSBOX_CLOCKSYNC

static Bitu INT1C_Handler(void) {
	return CBRET_NONE;
}

static Bitu INT12_Handler(void) {
	reg_ax=mem_readw(BIOS_MEMORY_SIZE);
	return CBRET_NONE;
}

static Bitu INT17_Handler(void) {
	if (reg_ah > 0x2 || reg_dx > 0x2) {	// 0-2 printer port functions
										// and no more than 3 parallel ports
		LOG_MSG("BIOS INT17: Unhandled call AH=%2X DX=%4x",reg_ah,reg_dx);
		return CBRET_NONE;
	}

	switch(reg_ah) {
	case 0x00:		// PRINTER: Write Character
		if(parallelPortObjects[reg_dx]!=0) {
			if(parallelPortObjects[reg_dx]->Putchar(reg_al))
				reg_ah=parallelPortObjects[reg_dx]->getPrinterStatus();
			else reg_ah=1;
		}
		break;
	case 0x01:		// PRINTER: Initialize port
		if(parallelPortObjects[reg_dx]!= 0) {
			parallelPortObjects[reg_dx]->initialize();
			reg_ah=parallelPortObjects[reg_dx]->getPrinterStatus();
		}
		break;
	case 0x02:		// PRINTER: Get Status
		if(parallelPortObjects[reg_dx] != 0)
			reg_ah=parallelPortObjects[reg_dx]->getPrinterStatus();
		//LOG_MSG("printer status: %x",reg_ah);
		break;
	};
	return CBRET_NONE;
}

static bool INT14_Wait(Bit16u port, Bit8u mask, Bit8u timeout, Bit8u* retval) {
	double starttime = PIC_FullIndex();
	double timeout_f = timeout * 1000.0;
	while (((*retval = IO_ReadB(port)) & mask) != mask) {
		if (starttime < (PIC_FullIndex() - timeout_f)) {
			return false;
		}
		CALLBACK_Idle();
	}
	return true;
}

static Bitu INT14_Handler(void) {
	if (reg_ah > 0x3 || reg_dx > 0x3) {	// 0-3 serial port functions
										// and no more than 4 serial ports
		LOG_MSG("BIOS INT14: Unhandled call AH=%2X DX=%4x",reg_ah,reg_dx);
		return CBRET_NONE;
	}
	
	Bit16u port = real_readw(0x40,reg_dx*2); // DX is always port number
	Bit8u timeout = mem_readb(BIOS_COM1_TIMEOUT + reg_dx);
	if (port==0)	{
		LOG(LOG_BIOS,LOG_NORMAL)("BIOS INT14: port %d does not exist.",reg_dx);
		return CBRET_NONE;
	}
	switch (reg_ah)	{
	case 0x00:	{
		// Initialize port
		// Parameters:				Return:
		// AL: port parameters		AL: modem status
		//							AH: line status

		// set baud rate
		Bitu baudrate = 9600;
		Bit16u baudresult;
		Bitu rawbaud=reg_al>>5;
		
		if (rawbaud==0){ baudrate=110;}
		else if (rawbaud==1){ baudrate=150;}
		else if (rawbaud==2){ baudrate=300;}
		else if (rawbaud==3){ baudrate=600;}
		else if (rawbaud==4){ baudrate=1200;}
		else if (rawbaud==5){ baudrate=2400;}
		else if (rawbaud==6){ baudrate=4800;}
		else if (rawbaud==7){ baudrate=9600;}

		baudresult = (Bit16u)(115200 / baudrate);

		IO_WriteB(port+3, 0x80);	// enable divider access
		IO_WriteB(port, (Bit8u)baudresult&0xff);
		IO_WriteB(port+1, (Bit8u)(baudresult>>8));

		// set line parameters, disable divider access
		IO_WriteB(port+3, reg_al&0x1F); // LCR
		
		// disable interrupts
		IO_WriteB(port+1, 0); // IER

		// get result
		reg_ah=(Bit8u)(IO_ReadB(port+5)&0xff);
		reg_al=(Bit8u)(IO_ReadB(port+6)&0xff);
		CALLBACK_SCF(false);
		break;
	}
	case 0x01: // Transmit character
		// Parameters:				Return:
		// AL: character			AL: unchanged
		// AH: 0x01					AH: line status from just before the char was sent
		//								(0x80 | unpredicted) in case of timeout
		//						[undoc]	(0x80 | line status) in case of tx timeout
		//						[undoc]	(0x80 | modem status) in case of dsr/cts timeout

		// set DTR & RTS on
		IO_WriteB(port+4,0x3);
		// wait for DSR & CTS
		if (INT14_Wait(port+6, 0x30, timeout, &reg_ah)) {
			// wait for TX buffer empty
			if (INT14_Wait(port+5, 0x20, timeout, &reg_ah)) {
				// fianlly send the character
				IO_WriteB(port,reg_al);
			} else
				reg_ah |= 0x80;
		} else // timed out
			reg_ah |= 0x80;

		CALLBACK_SCF(false);
		break;
	case 0x02: // Read character
		// Parameters:				Return:
		// AH: 0x02					AL: received character
		//						[undoc]	will be trashed in case of timeout
		//							AH: (line status & 0x1E) in case of success
		//								(0x80 | unpredicted) in case of timeout
		//						[undoc]	(0x80 | line status) in case of rx timeout
		//						[undoc]	(0x80 | modem status) in case of dsr timeout

		// set DTR on
		IO_WriteB(port+4,0x1);

		// wait for DSR
		if (INT14_Wait(port+6, 0x20, timeout, &reg_ah)) {
			// wait for character to arrive
			if (INT14_Wait(port+5, 0x01, timeout, &reg_ah)) {
				reg_ah &= 0x1E;
				reg_al = IO_ReadB(port);
			} else
				reg_ah |= 0x80;
		} else
			reg_ah |= 0x80;

		CALLBACK_SCF(false);
		break;
	case 0x03: // get status
		reg_ah=(Bit8u)(IO_ReadB(port+5)&0xff);
		reg_al=(Bit8u)(IO_ReadB(port+6)&0xff);
		CALLBACK_SCF(false);
		break;

	}
	return CBRET_NONE;
}

void KEYBOARD_AUX_Write(Bitu val);
unsigned char KEYBOARD_AUX_GetType();
unsigned char KEYBOARD_AUX_DevStatus();
unsigned char KEYBOARD_AUX_Resolution();
unsigned char KEYBOARD_AUX_SampleRate();
void KEYBOARD_ClrBuffer(void);

static Bitu INT15_Handler(void) {
	static Bit16u biosConfigSeg=0;

	if( ( machine==MCH_AMSTRAD ) && ( reg_ah<0x07 ) ) {
		switch(reg_ah) {
			case 0x00:
				// Read/Reset Mouse X/Y Counts.
				// CX = Signed X Count.
				// DX = Signed Y Count.
				// CC.
			case 0x01:
				// Write NVR Location.
				// AL = NVR Address to be written (0-63).
				// BL = NVR Data to be written.

				// AH = Return Code.
				// 00 = NVR Written Successfully.
				// 01 = NVR Address out of range.
				// 02 = NVR Data write error.
				// CC.
			case 0x02:
				// Read NVR Location.
				// AL = NVR Address to be read (0-63).

				// AH = Return Code.
				// 00 = NVR read successfully.
				// 01 = NVR Address out of range.
				// 02 = NVR checksum error.
				// AL = Byte read from NVR.
				// CC.
			default:
				LOG(LOG_BIOS,LOG_NORMAL)("INT15 Unsupported PC1512 Call %02X",reg_ah);
				return CBRET_NONE;
			case 0x03:
				// Write VDU Colour Plane Write Register.
				vga.amstrad.write_plane = reg_al & 0x0F;
				CALLBACK_SCF(false);
				break;
			case 0x04:
				// Write VDU Colour Plane Read Register.
				vga.amstrad.read_plane = reg_al & 0x03;
				CALLBACK_SCF(false);
				break;
			case 0x05:
				// Write VDU Graphics Border Register.
				vga.amstrad.border_color = reg_al & 0x0F;
				CALLBACK_SCF(false);
				break;
			case 0x06:
				// Return ROS Version Number.
				reg_bx = 0x0001;
				CALLBACK_SCF(false);
				break;
		}
	}
	switch (reg_ah) {
	case 0x06:
		LOG(LOG_BIOS,LOG_NORMAL)("INT15 Unkown Function 6 (Amstrad?)");
		break;
	case 0xC0:	/* Get Configuration*/
		{
			if (biosConfigSeg==0) biosConfigSeg = BIOS_GetMemory(1); //We have 16 bytes
			PhysPt data	= PhysMake(biosConfigSeg,0);
			phys_writew(data,8);						// 8 Bytes following
			if (IS_TANDY_ARCH) {
				if (machine==MCH_TANDY) {
					// Model ID (Tandy)
					phys_writeb(data+2,0xFF);
				} else {
					// Model ID (PCJR)
					phys_writeb(data+2,0xFD);
				}
				phys_writeb(data+3,0x0A);					// Submodel ID
				phys_writeb(data+4,0x10);					// Bios Revision
				/* Tandy doesn't have a 2nd PIC, left as is for now */
				phys_writeb(data+5,(1<<6)|(1<<5)|(1<<4));	// Feature Byte 1
			} else {
				if( PS1AudioCard ) {
					phys_writeb(data+2,0xFC);					// Model ID (PC)
					phys_writeb(data+3,0x0B);					// Submodel ID (PS/1).
				} else {
					phys_writeb(data+2,0xFC);					// Model ID (PC)
					phys_writeb(data+3,0x00);					// Submodel ID
				}
				phys_writeb(data+4,0x01);					// Bios Revision
				phys_writeb(data+5,(1<<6)|(1<<5)|(1<<4));	// Feature Byte 1
			}
			phys_writeb(data+6,(1<<6));				// Feature Byte 2
			phys_writeb(data+7,0);					// Feature Byte 3
			phys_writeb(data+8,0);					// Feature Byte 4
			phys_writeb(data+9,0);					// Feature Byte 5
			CPU_SetSegGeneral(es,biosConfigSeg);
			reg_bx = 0;
			reg_ah = 0;
			CALLBACK_SCF(false);
		}; break;
	case 0x4f:	/* BIOS - Keyboard intercept */
		/* Carry should be set but let's just set it just in case */
		CALLBACK_SCF(true);
		break;
	case 0x83:	/* BIOS - SET EVENT WAIT INTERVAL */
		{
			if(reg_al == 0x01) { /* Cancel it */
				mem_writeb(BIOS_WAIT_FLAG_ACTIVE,0);
				IO_Write(0x70,0xb);
				IO_Write(0x71,IO_Read(0x71)&~0x40);
				CALLBACK_SCF(false);
				break;
			}
			if (mem_readb(BIOS_WAIT_FLAG_ACTIVE)) {
				reg_ah=0x80;
				CALLBACK_SCF(true);
				break;
			}
			Bit32u count=(reg_cx<<16)|reg_dx;
			mem_writed(BIOS_WAIT_FLAG_POINTER,RealMake(SegValue(es),reg_bx));
			mem_writed(BIOS_WAIT_FLAG_COUNT,count);
			mem_writeb(BIOS_WAIT_FLAG_ACTIVE,1);
			/* Reprogram RTC to start */
			IO_Write(0x70,0xb);
			IO_Write(0x71,IO_Read(0x71)|0x40);
			CALLBACK_SCF(false);
		}
		break;
	case 0x84:	/* BIOS - JOYSTICK SUPPORT (XT after 11/8/82,AT,XT286,PS) */
		if (reg_dx == 0x0000) {
			// Get Joystick button status
			if (JOYSTICK_IsEnabled(0) || JOYSTICK_IsEnabled(1)) {
				reg_al = IO_ReadB(0x201)&0xf0;
				CALLBACK_SCF(false);
			} else {
				// dos values
				reg_ax = 0x00f0; reg_dx = 0x0201;
				CALLBACK_SCF(true);
			}
		} else if (reg_dx == 0x0001) {
			if (JOYSTICK_IsEnabled(0)) {
				reg_ax = (Bit16u)(JOYSTICK_GetMove_X(0)*127+128);
				reg_bx = (Bit16u)(JOYSTICK_GetMove_Y(0)*127+128);
				if(JOYSTICK_IsEnabled(1)) {
					reg_cx = (Bit16u)(JOYSTICK_GetMove_X(1)*127+128);
					reg_dx = (Bit16u)(JOYSTICK_GetMove_Y(1)*127+128);
				}
				else {
					reg_cx = reg_dx = 0;
				}
				CALLBACK_SCF(false);
			} else if (JOYSTICK_IsEnabled(1)) {
				reg_ax = reg_bx = 0;
				reg_cx = (Bit16u)(JOYSTICK_GetMove_X(1)*127+128);
				reg_dx = (Bit16u)(JOYSTICK_GetMove_Y(1)*127+128);
				CALLBACK_SCF(false);
			} else {			
				reg_ax = reg_bx = reg_cx = reg_dx = 0;
				CALLBACK_SCF(true);
			}
		} else {
			LOG(LOG_BIOS,LOG_ERROR)("INT15:84:Unknown Bios Joystick functionality.");
		}
		break;
	case 0x86:	/* BIOS - WAIT (AT,PS) */
		{
			if (mem_readb(BIOS_WAIT_FLAG_ACTIVE)) {
				reg_ah=0x83;
				CALLBACK_SCF(true);
				break;
			}
			Bit32u count=(reg_cx<<16)|reg_dx;
			mem_writed(BIOS_WAIT_FLAG_POINTER,RealMake(0,BIOS_WAIT_FLAG_TEMP));
			mem_writed(BIOS_WAIT_FLAG_COUNT,count);
			mem_writeb(BIOS_WAIT_FLAG_ACTIVE,1);
			/* Reprogram RTC to start */
			IO_Write(0x70,0xb);
			IO_Write(0x71,IO_Read(0x71)|0x40);
			while (mem_readd(BIOS_WAIT_FLAG_COUNT)) {
				CALLBACK_Idle();
			}
			CALLBACK_SCF(false);
			break;
		}
	case 0x87:	/* Copy extended memory */
		{
			bool enabled = MEM_A20_Enabled();
			MEM_A20_Enable(true);
			Bitu   bytes	= reg_cx * 2;
			PhysPt data		= SegPhys(es)+reg_si;
			PhysPt source	= (mem_readd(data+0x12) & 0x00FFFFFF) + (mem_readb(data+0x17)<<24);
			PhysPt dest		= (mem_readd(data+0x1A) & 0x00FFFFFF) + (mem_readb(data+0x1F)<<24);
			MEM_BlockCopy(dest,source,bytes);
			reg_ax = 0x00;
			MEM_A20_Enable(enabled);
			CALLBACK_SCF(false);
			break;
		}	
	case 0x88:	/* SYSTEM - GET EXTENDED MEMORY SIZE (286+) */
		/* This uses the 16-bit value read back from CMOS which is capped at 64MB */
		reg_ax=other_memsystems?0:size_extended;
		LOG(LOG_BIOS,LOG_NORMAL)("INT15:Function 0x88 Remaining %04X kb",reg_ax);
		CALLBACK_SCF(false);
		break;
	case 0x89:	/* SYSTEM - SWITCH TO PROTECTED MODE */
		{
			IO_Write(0x20,0x10);IO_Write(0x21,reg_bh);IO_Write(0x21,0);IO_Write(0x21,0xFF);
			IO_Write(0xA0,0x10);IO_Write(0xA1,reg_bl);IO_Write(0xA1,0);IO_Write(0xA1,0xFF);
			MEM_A20_Enable(true);
			PhysPt table=SegPhys(es)+reg_si;
			CPU_LGDT(mem_readw(table+0x8),mem_readd(table+0x8+0x2) & 0xFFFFFF);
			CPU_LIDT(mem_readw(table+0x10),mem_readd(table+0x10+0x2) & 0xFFFFFF);
			CPU_SET_CRX(0,CPU_GET_CRX(0)|1);
			CPU_SetSegGeneral(ds,0x18);
			CPU_SetSegGeneral(es,0x20);
			CPU_SetSegGeneral(ss,0x28);
			Bitu ret = mem_readw(SegPhys(ss)+reg_sp);
			reg_sp+=6;			//Clear stack of interrupt frame
			CPU_SetFlags(0,FMASK_ALL);
			reg_ax=0;
			CPU_JMP(false,0x30,ret,0);
		}
		break;
	case 0x8A:	/* EXTENDED MEMORY SIZE */
		{
			Bitu sz = MEM_TotalPages()*4;
			if (sz >= 1024) sz -= 1024;
			else sz = 0;
			reg_ax = sz & 0xFFFF;
			reg_dx = sz >> 16;
			CALLBACK_SCF(false);
		}
		break;
	case 0x90:	/* OS HOOK - DEVICE BUSY */
		CALLBACK_SCF(false);
		reg_ah=0;
		break;
	case 0x91:	/* OS HOOK - DEVICE POST */
		CALLBACK_SCF(false);
		reg_ah=0;
		break;
	case 0xc2:	/* BIOS PS2 Pointing Device Support */
		if (en_bios_ps2mouse) {
			fprintf(stderr,"INT 15h AX=%04x BX=%04x\n",reg_ax,reg_bx);
			switch (reg_al) {
				case 0x00:		// enable/disable
					if (reg_bh==0) {	// disable
						KEYBOARD_AUX_Write(0xF5);
						Mouse_SetPS2State(false);
						reg_ah=0;
						CALLBACK_SCF(false);
						KEYBOARD_ClrBuffer();
					} else if (reg_bh==0x01) {	//enable
						if (!Mouse_SetPS2State(true)) {
							reg_ah=5;
							CALLBACK_SCF(true);
							break;
						}
						KEYBOARD_AUX_Write(0xF4);
						KEYBOARD_ClrBuffer();
						reg_ah=0;
						CALLBACK_SCF(false);
					} else {
						CALLBACK_SCF(true);
						reg_ah=1;
					}
					break;
				case 0x01:		// reset
					KEYBOARD_AUX_Write(0xFF);
					KEYBOARD_ClrBuffer();
					reg_bx=0x00aa;	// mouse
					// fall through
				case 0x05:		// initialize
					if (reg_bh >= 3 && reg_bh <= 4) {
						fprintf(stderr,"INT 15h mouse initialized to %u-byte protocol\n",reg_bh);
						KEYBOARD_AUX_Write(0xF6); /* set defaults */
						Mouse_SetPS2State(false);
						KEYBOARD_ClrBuffer();
						CALLBACK_SCF(false);
						reg_ah=0;
					}
					else {
						CALLBACK_SCF(false);
						reg_ah=0x02; /* invalid input */
					}
					break;
				case 0x02: {		// set sampling rate
					static const unsigned char tbl[7] = {10,20,40,60,80,100,200};
					KEYBOARD_AUX_Write(0xF3);
					if (reg_bl > 6) reg_bl = 6;
					KEYBOARD_AUX_Write(tbl[reg_bh]);
					KEYBOARD_ClrBuffer();
					CALLBACK_SCF(false);
					reg_ah=0;
					} break;
				case 0x03:		// set resolution
					KEYBOARD_AUX_Write(0xE8);
					KEYBOARD_AUX_Write(reg_bh&3);
					KEYBOARD_ClrBuffer();
					CALLBACK_SCF(false);
					reg_ah=0;
					break;
				case 0x04:		// get type
					reg_bh=KEYBOARD_AUX_GetType();	// ID
					fprintf(stderr,"INT 15h reporting mouse device ID 0x%02x\n",reg_bh);
					KEYBOARD_ClrBuffer();
					CALLBACK_SCF(false);
					reg_ah=0;
					break;
				case 0x06:		// extended commands
					if (reg_bh == 0x00) {
						/* Read device status and info.
						 * Windows 9x does not appear to use this, but Windows NT 3.1 does (prior to entering
						 * full 32-bit protected mode) */
						CALLBACK_SCF(false);
						reg_bx=KEYBOARD_AUX_DevStatus();
						reg_cx=KEYBOARD_AUX_Resolution();
						reg_dx=KEYBOARD_AUX_SampleRate();
						KEYBOARD_ClrBuffer();
						reg_ah=0;
					}
					else if ((reg_bh==0x01) || (reg_bh==0x02)) { /* set scaling */
						KEYBOARD_AUX_Write(0xE6+reg_bh-1); /* 0xE6 1:1   or 0xE7 2:1 */
						KEYBOARD_ClrBuffer();
						CALLBACK_SCF(false); 
						reg_ah=0;
					} else {
						CALLBACK_SCF(true);
						reg_ah=1;
					}
					break;
				case 0x07:		// set callback
					Mouse_ChangePS2Callback(SegValue(es),reg_bx);
					CALLBACK_SCF(false);
					reg_ah=0;
					break;
				default:
					fprintf(stderr,"INT 15h unknown mouse call AX=%04x\n",reg_ax);
					CALLBACK_SCF(true);
					reg_ah=1;
					break;
			}
		}
		else {
			reg_ah=0x86;
			CALLBACK_SCF(true);
			if ((IS_EGAVGA_ARCH) || (machine==MCH_CGA)) {
				/* relict from comparisons, as int15 exits with a retf2 instead of an iret */
				CALLBACK_SZF(false);
			}
		}
		break;
	case 0xc3:      /* set carry flag so BorlandRTM doesn't assume a VECTRA/PS2 */
		reg_ah=0x86;
		CALLBACK_SCF(true);
		break;
	case 0xc4:	/* BIOS POS Programm option Select */
		LOG(LOG_BIOS,LOG_NORMAL)("INT15:Function %X called, bios mouse not supported",reg_ah);
		CALLBACK_SCF(true);
		break;
	case 0x53: // APM BIOS
		if (APMBIOS) {
			switch(reg_al) {
				case 0x00: // installation check
					reg_ah = 1;			// APM 1.2
					reg_al = 2;
					reg_bx = 0x504d;	// 'PM'
					reg_cx = 0;			// about no capabilities 
					// 32-bit interface seems to be needed for standby in win95
					break;
				case 0x01: // connect real mode interface
					if(reg_bx != 0x0) {
						reg_ah = 0x09;	// unrecognized device ID
						CALLBACK_SCF(true);			
						break;
					}
					if(!apm_realmode_connected) { // not yet connected
						CALLBACK_SCF(false);
						apm_realmode_connected=true;
					} else {
						reg_ah = 0x02;	// interface connection already in effect
						CALLBACK_SCF(true);			
					}
					break;
				case 0x04: // DISCONNECT INTERFACE
					if(reg_bx != 0x0) {
						reg_ah = 0x09;	// unrecognized device ID
						CALLBACK_SCF(true);			
						break;
					}
					if(apm_realmode_connected) {
						CALLBACK_SCF(false);
						apm_realmode_connected=false;
					} else {
						reg_ah = 0x03;	// interface not connected
						CALLBACK_SCF(true);			
					}
					break;
				case 0x07:
					if(reg_bx != 0x1) {
						reg_ah = 0x09;	// wrong device ID
						CALLBACK_SCF(true);			
						break;
					}
					if(!apm_realmode_connected) {
						reg_ah = 0x03;
						CALLBACK_SCF(true);
						break;
					}
					switch(reg_cx) {
						case 0x3: // power off
							throw(0);
							break;
						default:
							reg_ah = 0x0A; // invalid parameter value in CX
							CALLBACK_SCF(true);
							break;
					}
					break;
				case 0x08: // ENABLE/DISABLE POWER MANAGEMENT
					if(reg_bx != 0x0 && reg_bx != 0x1) {
						reg_ah = 0x09;	// unrecognized device ID
						CALLBACK_SCF(true);			
						break;
					} else if(!apm_realmode_connected) {
						reg_ah = 0x03;
						CALLBACK_SCF(true);
						break;
					}
					if(reg_cx==0x0) LOG_MSG("disable APM for device %4x",reg_bx);
					else if(reg_cx==0x1) LOG_MSG("enable APM for device %4x",reg_bx);
					else {
						reg_ah = 0x0A; // invalid parameter value in CX
						CALLBACK_SCF(true);
					}
					break;
				case 0x0e:
					if(reg_bx != 0x0) {
						reg_ah = 0x09;	// unrecognized device ID
						CALLBACK_SCF(true);			
						break;
					} else if(!apm_realmode_connected) {
						reg_ah = 0x03;	// interface not connected
						CALLBACK_SCF(true);
						break;
					}
					if(reg_ah < 1) reg_ah = 1;
					if(reg_al < 2) reg_al = 2;
					CALLBACK_SCF(false);
					break;
				case 0x0f:
					if(reg_bx != 0x0 && reg_bx != 0x1) {
						reg_ah = 0x09;	// unrecognized device ID
						CALLBACK_SCF(true);			
						break;
					} else if(!apm_realmode_connected) {
						reg_ah = 0x03;
						CALLBACK_SCF(true);
						break;
					}
					if(reg_cx==0x0) LOG_MSG("disengage APM for device %4x",reg_bx);
					else if(reg_cx==0x1) LOG_MSG("engage APM for device %4x",reg_bx);
					else {
						reg_ah = 0x0A; // invalid parameter value in CX
						CALLBACK_SCF(true);
					}
					break;
				default:
					LOG(LOG_BIOS,LOG_NORMAL)("unknown APM BIOS call %x",reg_ax);
					break;
			}
			CALLBACK_SCF(false);
		}
		else {
			reg_ah=0x86;
			CALLBACK_SCF(true);
			fprintf(stderr,"APM BIOS call attempted. set apmbios=1 if you want power management\n");
			if ((IS_EGAVGA_ARCH) || (machine==MCH_CGA) || (machine==MCH_AMSTRAD)) {
				/* relict from comparisons, as int15 exits with a retf2 instead of an iret */
				CALLBACK_SZF(false);
			}
		}
		break;
	case 0xe8:
		switch (reg_al) {
			case 0x01: { /* E801: memory size */
					Bitu sz = MEM_TotalPages()*4;
					if (sz >= 1024) sz -= 1024;
					else sz = 0;
					reg_ax = reg_cx = (sz > 0x3C00) ? 0x3C00 : sz; /* extended memory between 1MB and 16MB in KBs */
					sz -= reg_ax;
					sz /= 64;	/* extended memory size from 16MB in 64KB blocks */
					if (sz > 65535) sz = 65535;
					reg_bx = reg_dx = sz;
					CALLBACK_SCF(false);
				}
				break;
			case 0x20: { /* E820: MEMORY LISTING */
					if (reg_edx == 0x534D4150 && reg_ecx >= 20 && (MEM_TotalPages()*4) >= 24000) {
						/* return a minimalist list:
						 *
						 *    0) 0x000000-0x09EFFF       Free memory
						 *    1) 0x0C0000-0x0FFFFF       Reserved
						 *    2) 0x100000-...            Free memory (no ACPI tables) */
						if (reg_ebx < 3) {
							uint32_t base,len,type;
							Bitu seg = SegValue(es);

							assert((MEM_TotalPages()*4096) >= 0x100000);

							switch (reg_ebx) {
								case 0:	base=0x000000; len=0x09F000; type=1; break;
								case 1: base=0x0C0000; len=0x040000; type=2; break;
								case 2: base=0x100000; len=(MEM_TotalPages()*4096)-0x100000; type=1; break;
								default: E_Exit("Despite checks EBX is wrong value"); /* BUG! */
							};

							/* write to ES:DI */
							real_writed(seg,reg_di+0x00,base);
							real_writed(seg,reg_di+0x04,0);
							real_writed(seg,reg_di+0x08,len);
							real_writed(seg,reg_di+0x0C,0);
							real_writed(seg,reg_di+0x10,type);
							reg_ecx = 20;

							/* return EBX pointing to next entry. wrap around, as most BIOSes do.
							 * the program is supposed to stop on CF=1 or when we return EBX == 0 */
							if (++reg_ebx >= 3) reg_ebx = 0;
						}
						else {
							CALLBACK_SCF(true);
						}

						reg_eax = 0x534D4150;
					}
					else {
						reg_eax = 0x8600;
						CALLBACK_SCF(true);
					}
				}
				break;
			default:
				LOG(LOG_BIOS,LOG_ERROR)("INT15:Unknown call %4X",reg_ax);
				reg_ah=0x86;
				CALLBACK_SCF(true);
				if ((IS_EGAVGA_ARCH) || (machine==MCH_CGA) || (machine==MCH_AMSTRAD)) {
					/* relict from comparisons, as int15 exits with a retf2 instead of an iret */
					CALLBACK_SZF(false);
				}
		}
		break;
	default:
		LOG(LOG_BIOS,LOG_ERROR)("INT15:Unknown call %4X",reg_ax);
		reg_ah=0x86;
		CALLBACK_SCF(true);
		if ((IS_EGAVGA_ARCH) || (machine==MCH_CGA) || (machine==MCH_AMSTRAD)) {
			/* relict from comparisons, as int15 exits with a retf2 instead of an iret */
			CALLBACK_SZF(false);
		}
	}
	return CBRET_NONE;
}

void restart_program(std::vector<std::string> & parameters);

static Bitu IRQ14_Dummy(void) {
	/* FIXME: That's it? Don't I EOI the PIC? */
	return CBRET_NONE;
}

static Bitu IRQ15_Dummy(void) {
	/* FIXME: That's it? Don't I EOI the PIC? */
	return CBRET_NONE;
}

static Bitu Reboot_Handler(void) {
	// switch to text mode, notify user (let's hope INT10 still works)
	const char* const text = "\n\n   Restart requested by application.";
	reg_ax = 0;
	CALLBACK_RunRealInt(0x10);
	reg_ah = 0xe;
	reg_bx = 0;
	for(Bitu i = 0; i < strlen(text);i++) {
		reg_al = text[i];
		CALLBACK_RunRealInt(0x10);
	}
	LOG_MSG(text);
	double start = PIC_FullIndex();
	while((PIC_FullIndex()-start)<3000) CALLBACK_Idle();
	control->startup_params.insert(control->startup_params.begin(),control->cmdline->GetFileName());
	restart_program(control->startup_params);
	return CBRET_NONE;
}

void bios_enable_ps2() {
	mem_writew(BIOS_CONFIGURATION,
		mem_readw(BIOS_CONFIGURATION)|0x04); /* PS/2 mouse */
}

void BIOS_ZeroExtendedSize(bool in) {
	if(in) other_memsystems++; 
	else other_memsystems--;
	if(other_memsystems < 0) other_memsystems=0;
}

void BIOS_SetupKeyboard(void);
void BIOS_SetupDisks(void);

static void b_writew(unsigned char *d,Bitu v) {
	d[0] = v;
	d[1] = v >> 8;
}

static void b_writed(unsigned char *d,Bitu v) {
	d[0] = v;
	d[1] = v >> 8;
	d[2] = v >> 16;
	d[3] = v >> 24;
}

static unsigned char do_isapnp_chksum(unsigned char *d,int i) {
	unsigned char sum = 0;

	while (i-- > 0)
		sum += *d++;

	return (0x100 - sum) & 0xFF;
}

class BIOS:public Module_base{
private:
	CALLBACK_HandlerObject callback[13];
public:
	BIOS(Section* configuration):Module_base(configuration){
		/* tandy DAC can be requested in tandy_sound.cpp by initializing this field */
		bool use_tandyDAC=(real_readb(0x40,0xd4)==0xff);

		// Disney workaround
		Bit16u disney_port = mem_readw(BIOS_ADDRESS_LPT1);

		/* Clear the Bios Data Area (0x400-0x5ff, 0x600- is accounted to DOS) */
		for (Bit16u i=0;i<0x200;i++) real_writeb(0x40,i,0);

		/* Setup all the interrupt handlers the bios controls */

		/* INT 8 Clock IRQ Handler */
		Bitu call_irq0=CALLBACK_Allocate();	
		CALLBACK_Setup(call_irq0,INT8_Handler,CB_IRQ0,Real2Phys(BIOS_DEFAULT_IRQ0_LOCATION),"IRQ 0 Clock");
		RealSetVec(0x08,BIOS_DEFAULT_IRQ0_LOCATION);
		// pseudocode for CB_IRQ0:
		//	sti
		//	callback INT8_Handler
		//	push ds,ax,dx
		//	int 0x1c
		//	cli
		//	mov al, 0x20
		//	out 0x20, al
		//	pop dx,ax,ds
		//	iret

		mem_writed(BIOS_TIMER,0);			//Calculate the correct time

		/* INT 11 Get equipment list */
		callback[1].Install(&INT11_Handler,CB_IRET,"Int 11 Equipment");
		callback[1].Set_RealVec(0x11);

		/* INT 12 Memory Size default at 640 kb */
		callback[2].Install(&INT12_Handler,CB_IRET,"Int 12 Memory");
		callback[2].Set_RealVec(0x12);

		Bitu t_conv = MEM_TotalPages() << 2; /* convert 4096/byte pages -> 1024/byte KB units */
		if (t_conv > 640) t_conv = 640;

		if (IS_TANDY_ARCH) {
			/* reduce reported memory size for the Tandy (32k graphics memory
			   at the end of the conventional 640k) */
			if (machine==MCH_TANDY && t_conv > 624) t_conv = 624;
		}
		mem_writew(BIOS_MEMORY_SIZE,t_conv);

		/* INT 13 Bios Disk Support */
		BIOS_SetupDisks();

		/* INT 14 Serial Ports */
		callback[3].Install(&INT14_Handler,CB_IRET_STI,"Int 14 COM-port");
		callback[3].Set_RealVec(0x14);

		/* INT 15 Misc Calls */
		callback[4].Install(&INT15_Handler,CB_IRET,"Int 15 Bios");
		callback[4].Set_RealVec(0x15);

		/* INT 16 Keyboard handled in another file */
		BIOS_SetupKeyboard();

		/* INT 17 Printer Routines */
		callback[5].Install(&INT17_Handler,CB_IRET_STI,"Int 17 Printer");
		callback[5].Set_RealVec(0x17);

		/* INT 1A TIME and some other functions */
		callback[6].Install(&INT1A_Handler,CB_IRET_STI,"Int 1a Time");
		callback[6].Set_RealVec(0x1A);

		/* INT 1C System Timer tick called from INT 8 */
		callback[7].Install(&INT1C_Handler,CB_IRET,"Int 1c Timer");
		callback[7].Set_RealVec(0x1C);
		
		/* IRQ 8 RTC Handler */
		callback[8].Install(&INT70_Handler,CB_IRET,"Int 70 RTC");
		callback[8].Set_RealVec(0x70);

		/* Irq 9 rerouted to irq 2 */
		callback[9].Install(NULL,CB_IRQ9,"irq 9 bios");
		callback[9].Set_RealVec(0x71);

		/* Reboot */
		// This handler is an exit for more than only reboots, since we
		// don't handle these cases
		callback[10].Install(&Reboot_Handler,CB_IRET,"reboot");
		
		// INT 18h: Enter BASIC
		// Non-IBM BIOS would display "NO ROM BASIC" here
		callback[10].Set_RealVec(0x18);
		RealPt rptr = callback[10].Get_RealPointer();

		// INT 19h: Boot function
		// This is not a complete reboot as it happens after the POST
		// We don't handle it, so use the reboot function as exit.
		RealSetVec(0x19,rptr);

		// INT 76h: IDE IRQ 14
		// This is just a dummy IRQ handler to prevent crashes when
		// IDE emulation fires the IRQ and OS's like Win95 expect
		// the BIOS to handle the interrupt.
		// FIXME: Shouldn't the IRQ send an ACK to the PIC as well?!?
		callback[11].Install(&IRQ14_Dummy,CB_IRET,"irq 14 ide");
		callback[11].Set_RealVec(0x76);

		// INT 77h: IDE IRQ 15
		// This is just a dummy IRQ handler to prevent crashes when
		// IDE emulation fires the IRQ and OS's like Win95 expect
		// the BIOS to handle the interrupt.
		// FIXME: Shouldn't the IRQ send an ACK to the PIC as well?!?
		callback[12].Install(&IRQ15_Dummy,CB_IRET,"irq 15 ide");
		callback[12].Set_RealVec(0x77);

		init_vm86_fake_io();

		// The farjump at the processor reset entry point (jumps to POST routine)
		phys_writeb(0xFFFF0,0xEA);		// FARJMP
		phys_writew(0xFFFF1,RealOff(BIOS_DEFAULT_RESET_LOCATION));	// offset
		phys_writew(0xFFFF3,RealSeg(BIOS_DEFAULT_RESET_LOCATION));	// segment

		// Compatible POST routine location: jump to the callback
		phys_writeb(Real2Phys(BIOS_DEFAULT_RESET_LOCATION)+0,0xEA);				// FARJMP
		phys_writew(Real2Phys(BIOS_DEFAULT_RESET_LOCATION)+1,RealOff(rptr));	// offset
		phys_writew(Real2Phys(BIOS_DEFAULT_RESET_LOCATION)+3,RealSeg(rptr));	// segment

		/* Irq 2 */
		Bitu call_irq2=CALLBACK_Allocate();	
		CALLBACK_Setup(call_irq2,NULL,CB_IRET_EOI_PIC1,Real2Phys(BIOS_DEFAULT_IRQ2_LOCATION),"irq 2 bios");
		RealSetVec(0x0a,BIOS_DEFAULT_IRQ2_LOCATION);

		/* Some hardcoded vectors */
		phys_writeb(Real2Phys(BIOS_DEFAULT_HANDLER_LOCATION),0xcf);	/* bios default interrupt vector location -> IRET */
		phys_writew(Real2Phys(RealGetVec(0x12))+0x12,0x20); //Hack for Jurresic

		if (machine==MCH_TANDY || machine==MCH_AMSTRAD) phys_writeb(0xffffe,0xff)	;	/* Tandy model */
		else if (machine==MCH_PCJR) phys_writeb(0xffffe,0xfd);	/* PCJr model */
		else phys_writeb(0xffffe,0xfc);	/* PC */

		// System BIOS identification
		const char* const b_type =
			"IBM COMPATIBLE 486 BIOS COPYRIGHT The DOSBox Team.";
		for(Bitu i = 0; i < strlen(b_type); i++) phys_writeb(0xfe00e + i,b_type[i]);
		
		// System BIOS version
		const char* const b_vers =
			"DOSBox FakeBIOS v1.0";
		for(Bitu i = 0; i < strlen(b_vers); i++) phys_writeb(0xfe061+i,b_vers[i]);

		// write system BIOS date
		const char* const b_date = "01/01/92";
		for(Bitu i = 0; i < strlen(b_date); i++) phys_writeb(0xffff5+i,b_date[i]);
		phys_writeb(0xfffff,0x55); // signature

		// program system timer
		// timer 2
		IO_Write(0x43,0xb6);
		IO_Write(0x42,1320&0xff);
		IO_Write(0x42,1320>>8);

		// tandy DAC setup
		tandy_sb.port=0;
		tandy_dac.port=0;
		if (use_tandyDAC) {
			/* tandy DAC sound requested, see if soundblaster device is available */
			Bitu tandy_dac_type = 0;
			if (Tandy_InitializeSB()) {
				tandy_dac_type = 1;
			} else if (Tandy_InitializeTS()) {
				tandy_dac_type = 2;
			}
			if (tandy_dac_type) {
				real_writew(0x40,0xd0,0x0000);
				real_writew(0x40,0xd2,0x0000);
				real_writeb(0x40,0xd4,0xff);	/* tandy DAC init value */
				real_writed(0x40,0xd6,0x00000000);
				/* install the DAC callback handler */
				tandy_DAC_callback[0]=new CALLBACK_HandlerObject();
				tandy_DAC_callback[1]=new CALLBACK_HandlerObject();
				tandy_DAC_callback[0]->Install(&IRQ_TandyDAC,CB_IRET,"Tandy DAC IRQ");
				tandy_DAC_callback[1]->Install(NULL,CB_TDE_IRET,"Tandy DAC end transfer");
				// pseudocode for CB_TDE_IRET:
				//	push ax
				//	mov ax, 0x91fb
				//	int 15
				//	cli
				//	mov al, 0x20
				//	out 0x20, al
				//	pop ax
				//	iret

				Bit8u tandy_irq = 7;
				if (tandy_dac_type==1) tandy_irq = tandy_sb.irq;
				else if (tandy_dac_type==2) tandy_irq = tandy_dac.irq;
				Bit8u tandy_irq_vector = tandy_irq;
				if (tandy_irq_vector<8) tandy_irq_vector += 8;
				else tandy_irq_vector += (0x70-8);

				RealPt current_irq=RealGetVec(tandy_irq_vector);
				real_writed(0x40,0xd6,current_irq);
				for (Bit16u i=0; i<0x10; i++) phys_writeb(PhysMake(0xf000,0xa084+i),0x80);
			} else real_writeb(0x40,0xd4,0x00);
		}
	
		/* Setup some stuff in 0x40 bios segment */
		
		// port timeouts
		// always 1 second even if the port does not exist
		BIOS_SetLPTPort(0, disney_port);
		for(Bitu i = 1; i < 3; i++) BIOS_SetLPTPort(i, 0);
		mem_writeb(BIOS_COM1_TIMEOUT,1);
		mem_writeb(BIOS_COM2_TIMEOUT,1);
		mem_writeb(BIOS_COM3_TIMEOUT,1);
		mem_writeb(BIOS_COM4_TIMEOUT,1);
		
		/* Setup equipment list */
		// look http://www.bioscentral.com/misc/bda.htm
		
		//Bit16u config=0x4400;	//1 Floppy, 2 serial and 1 parallel 
		Bit16u config = 0x0;
		
#if (C_FPU)
		//FPU
		config|=0x2;
#endif
		switch (machine) {
		case MCH_HERC:
			//Startup monochrome
			config|=0x30;
			break;
		case EGAVGA_ARCH_CASE:
		case MCH_CGA:
		case TANDY_ARCH_CASE:
		case MCH_AMSTRAD:
			//Startup 80x25 color
			config|=0x20;
			break;
		default:
			//EGA VGA
			config|=0;
			break;
		}
#if 0
		// PS2 mouse
		config |= 0x04;
#endif
		// Gameport
		config |= 0x1000;
		mem_writew(BIOS_CONFIGURATION,config);
		CMOS_SetRegister(0x14,(Bit8u)(config&0xff)); //Should be updated on changes
		/* Setup extended memory size */
		IO_Write(0x70,0x30);
		size_extended=IO_Read(0x71);
		IO_Write(0x70,0x31);
		size_extended|=(IO_Read(0x71) << 8);
		BIOS_HostTimeSync();

		// ISA Plug & Play I/O ports
		if (1) {
			ISAPNP_PNP_ADDRESS_PORT = new IO_WriteHandleObject;
			ISAPNP_PNP_ADDRESS_PORT->Install(0x279,isapnp_write_port,IO_MB);
			ISAPNP_PNP_DATA_PORT = new IO_WriteHandleObject;
			ISAPNP_PNP_DATA_PORT->Install(0xA79,isapnp_write_port,IO_MB);
			ISAPNP_PNP_READ_PORT = new IO_ReadHandleObject;
			ISAPNP_PNP_READ_PORT->Install(ISA_PNP_WPORT,isapnp_read_port,IO_MB);

//			ISA_PNP_devreg((isapnp_test = new ISAPnPTestDevice()));
		}

		// ISA Plug & Play BIOS entrypoint
		if (ISAPNPBIOS) {
			int i,port;
			unsigned char c,tmp[256];
			Bitu base = 0xfe100; /* take the unused space just after the fake BIOS signature */

			LOG_MSG("ISA Plug & Play BIOS enabled");

			Bitu call_pnp_r = CALLBACK_Allocate();
			Bitu call_pnp_rp = PNPentry_real = CALLBACK_RealPointer(call_pnp_r);
			CALLBACK_Setup(call_pnp_r,ISAPNP_Handler_RM,CB_RETF,"ISA Plug & Play entry point (real)");
			//fprintf(stderr,"real entry pt=%08lx\n",PNPentry_real);

			Bitu call_pnp_p = CALLBACK_Allocate();
			Bitu call_pnp_pp = PNPentry_prot = CALLBACK_RealPointer(call_pnp_p);
			CALLBACK_Setup(call_pnp_p,ISAPNP_Handler_PM,CB_RETF,"ISA Plug & Play entry point (protected)");
			//fprintf(stderr,"prot entry pt=%08lx\n",PNPentry_prot);

			phys_writeb(base+0,'$');
			phys_writeb(base+1,'P');
			phys_writeb(base+2,'n');
			phys_writeb(base+3,'P');
			phys_writeb(base+4,0x10);		/* Version:		1.0 */
			phys_writeb(base+5,0x21);		/* Length:		0x21 bytes */
			phys_writew(base+6,0x0000);		/* Control field:	Event notification not supported */
			/* skip checksum atm */
			phys_writed(base+9,0);			/* Event notify flag addr: (none) */
			phys_writed(base+0xD,call_pnp_rp);	/* Real-mode entry point */
			phys_writew(base+0x11,call_pnp_pp&0xFFFF); /* Protected mode offset */
			phys_writed(base+0x13,(call_pnp_pp >> 12) & 0xFFFF0); /* Protected mode code segment base */
			phys_writed(base+0x17,ISAPNP_ID('D','O','S',0,7,4,0));		/* OEM device identifier (TODO) */
			phys_writew(base+0x1B,0xF000);		/* real-mode data segment */
			phys_writed(base+0x1D,0xF0000);		/* protected mode data segment address */
			/* run checksum */
			c=0;
			for (i=0;i < 0x21;i++) {
				if (i != 8) c += phys_readb(base+i);
			}
			phys_writeb(base+8,0x100-c);		/* checksum value: set so that summing bytes across the struct == 0 */

			/* input device (keyboard) */
			if (!ISAPNP_RegisterSysDev(ISAPNP_sysdev_Keyboard,sizeof(ISAPNP_sysdev_Keyboard),true))
				fprintf(stderr,"ISAPNP register failed\n");

			/* input device (mouse) */
			if (!ISAPNP_RegisterSysDev(ISAPNP_sysdev_Mouse,sizeof(ISAPNP_sysdev_Mouse),true))
				fprintf(stderr,"ISAPNP register failed\n");

			/* DMA controller */
			if (!ISAPNP_RegisterSysDev(ISAPNP_sysdev_DMA_Controller,sizeof(ISAPNP_sysdev_DMA_Controller),true))
				fprintf(stderr,"ISAPNP register failed\n");

			/* Interrupt controller */
			if (!ISAPNP_RegisterSysDev(ISAPNP_sysdev_PIC,sizeof(ISAPNP_sysdev_PIC),true))
				fprintf(stderr,"ISAPNP register failed\n");

			/* Timer */
			if (!ISAPNP_RegisterSysDev(ISAPNP_sysdev_Timer,sizeof(ISAPNP_sysdev_Timer),true))
				fprintf(stderr,"ISAPNP register failed\n");

			/* Realtime clock */
			if (!ISAPNP_RegisterSysDev(ISAPNP_sysdev_RTC,sizeof(ISAPNP_sysdev_RTC),true))
				fprintf(stderr,"ISAPNP register failed\n");

			/* PC speaker */
			if (!ISAPNP_RegisterSysDev(ISAPNP_sysdev_PC_Speaker,sizeof(ISAPNP_sysdev_PC_Speaker),true))
				fprintf(stderr,"ISAPNP register failed\n");

			/* System board */
			if (!ISAPNP_RegisterSysDev(ISAPNP_sysdev_System_Board,sizeof(ISAPNP_sysdev_System_Board),true))
				fprintf(stderr,"ISAPNP register failed\n");

#if (C_FPU)
			/* Numeric Coprocessor */
			if (!ISAPNP_RegisterSysDev(ISAPNP_sysdev_Numeric_Coprocessor,sizeof(ISAPNP_sysdev_Numeric_Coprocessor),true))
				fprintf(stderr,"ISAPNP register failed\n");
#endif

			/* RAM resources. we have to construct it */
			{
				Bitu max = MEM_TotalPages() * 4096;
				const unsigned char h1[9] = {
					ISAPNP_SYSDEV_HEADER(
						ISAPNP_ID('P','N','P',0x0,0xC,0x0,0x1), /* PNP0C01 System device, motherboard resources */
						ISAPNP_TYPE(0x05,0x00,0x00),		/* type: Memory, RAM, general */
						0x0001 | 0x0002)
				};

				i = 0;
				memcpy(tmp+i,h1,9); i += 9;			/* can't disable, can't configure */
				/*----------allocated--------*/
				tmp[i+0] = 0x80 | 6;				/* 32-bit memory range */
				tmp[i+1] = 9;					/* length=9 */
				tmp[i+2] = 0;
				tmp[i+3] = 0x01;				/* writeable, no cache, 8-bit, not shadowable, not ROM */
				host_writed(tmp+i+4,0x00000);			/* base */
				host_writed(tmp+i+8,max > 0xA0000 ? 0xA0000 : 0x00000); /* length */
				i += 9+3;

#if 0
				tmp[i+0] = 0x80 | 6;				/* 32-bit memory range */
				tmp[i+1] = 9;					/* length=9 */
				tmp[i+2] = 0;
				tmp[i+3] = 0x40;				/* read only, ROM, no cache, 8-bit, not shadowable */
				host_writed(tmp+i+4,0xF0000);			/* base */
				host_writed(tmp+i+8,0x10000);			/* length */
				i += 9+3;
#endif

				if (max > 0x100000) {
					tmp[i+0] = 0x80 | 6;				/* 32-bit memory range */
					tmp[i+1] = 9;					/* length=9 */
					tmp[i+2] = 0;
					tmp[i+3] = 0x01;
					host_writed(tmp+i+4,0x100000);			/* base */
					host_writed(tmp+i+8,max-0x100000);		/* length */
					i += 9+3;
				}

				tmp[i+0] = 0x79;				/* END TAG */
				tmp[i+1] = 0x00;
				i += 2;
				/*-------------possible-----------*/
				tmp[i+0] = 0x79;				/* END TAG */
				tmp[i+1] = 0x00;
				i += 2;
				/*-------------compatible---------*/
				tmp[i+0] = 0x79;				/* END TAG */
				tmp[i+1] = 0x00;
				i += 2;

				if (!ISAPNP_RegisterSysDev(tmp,i))
					fprintf(stderr,"ISAPNP register failed\n");
			}

			/* register parallel ports */
			for (port=0;port < 3;port++) {
				Bitu port = mem_readw(BIOS_ADDRESS_LPT1+(port*2));
				if (port != 0) {
					const unsigned char h1[9] = {
						ISAPNP_SYSDEV_HEADER(
							ISAPNP_ID('P','N','P',0x0,0x4,0x0,0x0), /* PNP0400 Standard LPT printer port */
							ISAPNP_TYPE(0x07,0x01,0x00),		/* type: General parallel port */
							0x0001 | 0x0002)
					};

					i = 0;
					memcpy(tmp+i,h1,9); i += 9;			/* can't disable, can't configure */
					/*----------allocated--------*/
					tmp[i+0] = (8 << 3) | 7;			/* IO resource */
					tmp[i+1] = 0x01;				/* 16-bit decode */
					host_writew(tmp+i+2,port);			/* min */
					host_writew(tmp+i+4,port);			/* max */
					tmp[i+6] = 0x10;				/* align */
					tmp[i+7] = 0x03;				/* length */
					i += 7+1;

					/* TODO: If/when LPT emulation handles the IRQ, add IRQ resource here */

					tmp[i+0] = 0x79;				/* END TAG */
					tmp[i+1] = 0x00;
					i += 2;
					/*-------------possible-----------*/
					tmp[i+0] = 0x79;				/* END TAG */
					tmp[i+1] = 0x00;
					i += 2;
					/*-------------compatible---------*/
					tmp[i+0] = 0x79;				/* END TAG */
					tmp[i+1] = 0x00;
					i += 2;

					if (!ISAPNP_RegisterSysDev(tmp,i))
						fprintf(stderr,"ISAPNP register failed\n");
				}
			}
		}
	}
	~BIOS(){
		/* abort DAC playing */
		if (tandy_sb.port) {
			IO_Write(tandy_sb.port+0xc,0xd3);
			IO_Write(tandy_sb.port+0xc,0xd0);
		}
		real_writeb(0x40,0xd4,0x00);
		if (tandy_DAC_callback[0]) {
			Bit32u orig_vector=real_readd(0x40,0xd6);
			if (orig_vector==tandy_DAC_callback[0]->Get_RealPointer()) {
				/* set IRQ vector to old value */
				Bit8u tandy_irq = 7;
				if (tandy_sb.port) tandy_irq = tandy_sb.irq;
				else if (tandy_dac.port) tandy_irq = tandy_dac.irq;
				Bit8u tandy_irq_vector = tandy_irq;
				if (tandy_irq_vector<8) tandy_irq_vector += 8;
				else tandy_irq_vector += (0x70-8);

				RealSetVec(tandy_irq_vector,real_readd(0x40,0xd6));
				real_writed(0x40,0xd6,0x00000000);
			}
			delete tandy_DAC_callback[0];
			delete tandy_DAC_callback[1];
			tandy_DAC_callback[0]=NULL;
			tandy_DAC_callback[1]=NULL;
		}
	}
};

// set com port data in bios data area
// parameter: array of 4 com port base addresses, 0 = none
void BIOS_SetComPorts(Bit16u baseaddr[]) {
	Bit16u portcount=0;
	Bit16u equipmentword;
	for(Bitu i = 0; i < 4; i++) {
		if(baseaddr[i]!=0) portcount++;
		if(i==0)		mem_writew(BIOS_BASE_ADDRESS_COM1,baseaddr[i]);
		else if(i==1)	mem_writew(BIOS_BASE_ADDRESS_COM2,baseaddr[i]);
		else if(i==2)	mem_writew(BIOS_BASE_ADDRESS_COM3,baseaddr[i]);
		else			mem_writew(BIOS_BASE_ADDRESS_COM4,baseaddr[i]);
	}
	// set equipment word
	equipmentword = mem_readw(BIOS_CONFIGURATION);
	equipmentword &= (~0x0E00);
	equipmentword |= (portcount << 9);
	mem_writew(BIOS_CONFIGURATION,equipmentword);
	CMOS_SetRegister(0x14,(Bit8u)(equipmentword&0xff)); //Should be updated on changes
}

void BIOS_SetLPTPort(Bitu port, Bit16u baseaddr) {
	switch(port) {
	case 0:
		mem_writew(BIOS_ADDRESS_LPT1,baseaddr);
		mem_writeb(BIOS_LPT1_TIMEOUT, 10);
		break;
	case 1:
		mem_writew(BIOS_ADDRESS_LPT2,baseaddr);
		mem_writeb(BIOS_LPT2_TIMEOUT, 10);
		break;
	case 2:
		mem_writew(BIOS_ADDRESS_LPT3,baseaddr);
		mem_writeb(BIOS_LPT3_TIMEOUT, 10);
		break;
	}

	// set equipment word: count ports
	Bit16u portcount=0;
	if(mem_readw(BIOS_ADDRESS_LPT1) != 0) portcount++;
	if(mem_readw(BIOS_ADDRESS_LPT2) != 0) portcount++;
	if(mem_readw(BIOS_ADDRESS_LPT3) != 0) portcount++;
	
	Bit16u equipmentword = mem_readw(BIOS_CONFIGURATION);
	equipmentword &= (~0xC000);
	equipmentword |= (portcount << 14);
	mem_writew(BIOS_CONFIGURATION,equipmentword);
}

void BIOS_PnP_ComPortRegister(Bitu port,Bitu irq) {
	/* add to PnP BIOS */
	if (ISAPNPBIOS) {
		unsigned char tmp[256];
		int i;

		/* register serial ports */
		const unsigned char h1[9] = {
			ISAPNP_SYSDEV_HEADER(
				ISAPNP_ID('P','N','P',0x0,0x5,0x0,0x1), /* PNP0501 16550A-compatible COM port */
				ISAPNP_TYPE(0x07,0x00,0x02),		/* type: RS-232 communcations device, 16550-compatible */
				0x0001 | 0x0002)
		};

		i = 0;
		memcpy(tmp+i,h1,9); i += 9;			/* can't disable, can't configure */
		/*----------allocated--------*/
		tmp[i+0] = (8 << 3) | 7;			/* IO resource */
		tmp[i+1] = 0x01;				/* 16-bit decode */
		host_writew(tmp+i+2,port);			/* min */
		host_writew(tmp+i+4,port);			/* max */
		tmp[i+6] = 0x10;				/* align */
		tmp[i+7] = 0x08;				/* length */
		i += 7+1;

		if (irq > 0) {
			tmp[i+0] = (4 << 3) | 3;			/* IRQ resource */
			host_writew(tmp+i+1,1 << irq);
			tmp[i+3] = 0x09;				/* HTL=1 LTL=1 */
			i += 3+1;
		}

		tmp[i+0] = 0x79;				/* END TAG */
		tmp[i+1] = 0x00;
		i += 2;
		/*-------------possible-----------*/
		tmp[i+0] = 0x79;				/* END TAG */
		tmp[i+1] = 0x00;
		i += 2;
		/*-------------compatible---------*/
		tmp[i+0] = 0x79;				/* END TAG */
		tmp[i+1] = 0x00;
		i += 2;

		if (!ISAPNP_RegisterSysDev(tmp,i)) {
			//fprintf(stderr,"ISAPNP register failed\n");
		}
	}
}

static BIOS* test;

void BIOS_Destroy(Section* /*sec*/){
	int i;
	for (i=0;i < 0x100;i++) {
		if (ISAPNP_SysDevNodes[i] != NULL) delete ISAPNP_SysDevNodes[i];
		ISAPNP_SysDevNodes[i] = NULL;
	}
	delete test;
}

void BIOS_Init(Section* sec) {
	int i;

	ISAPNP_SysDevNodeCount = 0;
	ISAPNP_SysDevNodeLargest = 0;
	for (i=0;i < 0x100;i++) ISAPNP_SysDevNodes[i] = NULL;

	test = new BIOS(sec);
	sec->AddDestroyFunction(&BIOS_Destroy,false);
}
