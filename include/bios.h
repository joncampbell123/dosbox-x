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

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

#ifndef DOSBOX_BIOS_H
#define DOSBOX_BIOS_H

#define BIOS_BASE_ADDRESS_COM1          0x400
#define BIOS_BASE_ADDRESS_COM2          0x402
#define BIOS_BASE_ADDRESS_COM3          0x404
#define BIOS_BASE_ADDRESS_COM4          0x406
#define BIOS_ADDRESS_LPT1               0x408
#define BIOS_ADDRESS_LPT2               0x40a
#define BIOS_ADDRESS_LPT3               0x40c
/* 0x40e is reserved */
#define BIOS_CONFIGURATION              0x410
/* 0x412 is reserved */
#define BIOS_MEMORY_SIZE                0x413
#define BIOS_TRUE_MEMORY_SIZE           0x415
/* #define bios_expansion_memory_size      (*(unsigned int   *) 0x415) */
#define BIOS_KEYBOARD_STATE             0x417
#define BIOS_KEYBOARD_FLAGS1            BIOS_KEYBOARD_STATE
#define BIOS_KEYBOARD_FLAGS2            0x418
#define BIOS_KEYBOARD_TOKEN             0x419
/* used for keyboard input with Alt-Number */
#define BIOS_KEYBOARD_BUFFER_HEAD       0x41a
#define BIOS_KEYBOARD_BUFFER_TAIL       0x41c
#define BIOS_KEYBOARD_BUFFER            0x41e
/* #define bios_keyboard_buffer            (*(unsigned int   *) 0x41e) */
#define BIOS_DRIVE_ACTIVE               0x43e
#define BIOS_DRIVE_RUNNING              0x43f
#define BIOS_DISK_MOTOR_TIMEOUT         0x440
#define BIOS_DISK_STATUS                0x441
/* #define bios_fdc_result_buffer          (*(unsigned short *) 0x442) */
#define BIOS_VIDEO_MODE                 0x449
#define BIOS_SCREEN_COLUMNS             0x44a
#define BIOS_VIDEO_MEMORY_USED          0x44c
#define BIOS_VIDEO_MEMORY_ADDRESS       0x44e
#define BIOS_VIDEO_CURSOR_POS	        0x450


#define BIOS_CURSOR_SHAPE               0x460
#define BIOS_CURSOR_LAST_LINE           0x460
#define BIOS_CURSOR_FIRST_LINE          0x461
#define BIOS_CURRENT_SCREEN_PAGE        0x462
#define BIOS_VIDEO_PORT                 0x463
#define BIOS_VDU_CONTROL                0x465
#define BIOS_VDU_COLOR_REGISTER         0x466
/* 0x467-0x468 is reserved */
#define BIOS_TIMER                      0x46c
#define BIOS_24_HOURS_FLAG              0x470
#define BIOS_CTRL_BREAK_FLAG            0x471
#define BIOS_CTRL_ALT_DEL_FLAG          0x472
#define BIOS_HARDDISK_COUNT		0x475
/* 0x474, 0x476, 0x477 is reserved */
#define BIOS_LPT1_TIMEOUT               0x478
#define BIOS_LPT2_TIMEOUT               0x479
#define BIOS_LPT3_TIMEOUT               0x47a
/* 0x47b is reserved */
#define BIOS_COM1_TIMEOUT               0x47c
#define BIOS_COM2_TIMEOUT               0x47d
#define BIOS_COM3_TIMEOUT               0x47e
#define BIOS_COM4_TIMEOUT               0x47f
/* 0x47e is reserved */ //<- why that?
/* 0x47f-0x4ff is unknow for me */
#define BIOS_KEYBOARD_BUFFER_START      0x480
#define BIOS_KEYBOARD_BUFFER_END        0x482

#define BIOS_ROWS_ON_SCREEN_MINUS_1     0x484
#define BIOS_FONT_HEIGHT                0x485

#define BIOS_VIDEO_INFO_0               0x487
#define BIOS_VIDEO_INFO_1               0x488
#define BIOS_VIDEO_INFO_2               0x489
#define BIOS_VIDEO_COMBO                0x48a

#define BIOS_KEYBOARD_FLAGS3            0x496
#define BIOS_KEYBOARD_LEDS              0x497

#define BIOS_WAIT_FLAG_POINTER          0x498
#define BIOS_WAIT_FLAG_COUNT	        0x49c		
#define BIOS_WAIT_FLAG_ACTIVE			0x4a0
#define BIOS_WAIT_FLAG_TEMP				0x4a1


#define BIOS_PRINT_SCREEN_FLAG          0x500

#define BIOS_VIDEO_SAVEPTR              0x4a8


//#define BIOS_DEFAULT_IRQ0_LOCATION		(RealMake(0xf000,0xfea5))
//#define BIOS_DEFAULT_IRQ1_LOCATION		(RealMake(0xf000,0xe987))
//#define BIOS_DEFAULT_IRQ2_LOCATION		(RealMake(0xf000,0xff55))
//#define BIOS_DEFAULT_HANDLER_LOCATION		(RealMake(0xf000,0xff53))
//#define BIOS_VIDEO_TABLE_LOCATION		(RealMake(0xf000,0xf0a4))
//#define BIOS_DEFAULT_RESET_LOCATION		(RealMake(0xf000,0xe05b))

extern Bitu BIOS_DEFAULT_IRQ0_LOCATION;		// (RealMake(0xf000,0xfea5))
extern Bitu BIOS_DEFAULT_IRQ1_LOCATION;		// (RealMake(0xf000,0xe987))
extern Bitu BIOS_DEFAULT_IRQ2_LOCATION;		// (RealMake(0xf000,0xff55))
extern Bitu BIOS_DEFAULT_HANDLER_LOCATION;	// (RealMake(0xf000,0xff53))
extern Bitu BIOS_VIDEO_TABLE_LOCATION;		// (RealMake(0xf000,0xf0a4))
extern Bitu BIOS_DEFAULT_RESET_LOCATION;	// RealMake(0xf000,0xe05b)
extern Bitu BIOS_VIDEO_TABLE_SIZE;

Bitu ROMBIOS_GetMemory(Bitu bytes,const char *who=NULL,Bitu alignment=1,Bitu must_be_at=0);

/* maximum of scancodes handled by keyboard bios routines */
#define MAX_SCAN_CODE 0x58

/* The Section handling Bios Disk Access */
//#define BIOS_MAX_DISK 10

//#define MAX_SWAPPABLE_DISKS 20

void BIOS_ZeroExtendedSize(bool in);
void char_out(Bit8u chr,Bit32u att,Bit8u page);
void INT10_StartUp(void);
void INT16_StartUp(void);
void INT2A_StartUp(void);
void INT2F_StartUp(void);
void INT33_StartUp(void);
void INT13_StartUp(void);

bool BIOS_AddKeyToBuffer(Bit16u code);

void INT10_ReloadRomFonts();

void BIOS_SetComPorts (Bit16u baseaddr[]);
void BIOS_SetLPTPort (Bitu port, Bit16u baseaddr);

bool ISAPNP_RegisterSysDev(const unsigned char *raw,Bitu len,bool already=false);

class ISAPnPDevice {
public:
	ISAPnPDevice();
	virtual ~ISAPnPDevice();
public:
	void checksum_ident();
public:
	virtual void config(Bitu val);
	virtual void wakecsn(Bitu val);
	virtual void select_logical_device(Bitu val);
	virtual void on_pnp_key();
	/* ISA PnP I/O data read/write */
	virtual uint8_t read(Bitu addr);
	virtual void write(Bitu addr,Bitu val);
public:
	unsigned char		CSN;
	unsigned char		logical_device;
	unsigned char		ident[9];		/* 72-bit vendor + serial + checksum identity */
	unsigned char		ident_bp;		/* bit position of identity read */
	unsigned char		ident_2nd;
	unsigned char		resource_ident;
	unsigned char*		resource_data;
	size_t			resource_data_len;
	unsigned int		resource_data_pos;
};

/* abc = ASCII letters of the alphabet
 * defg = hexadecimal digits */
#define ISAPNP_ID(a,b,c,d,e,f,g) \
	 (((a)&0x1F)    <<  2) | \
	 (((b)&0x1F)    >>  3) | \
	((((b)&0x1F)&7) << 13) | \
	 (((c)&0x1F)    <<  8) | \
	 (((d)&0x1F)    << 20) | \
	 (((e)&0x1F)    << 16) | \
	 (((f)&0x1F)    << 28) | \
	 (((g)&0x1F)    << 24)

#define ISAPNP_TYPE(a,b,c) (a),(b),(c)
#define ISAPNP_SMALL_TAG(t,l) (((t) << 3) | (l))

#define ISAPNP_SYSDEV_HEADER(id,type,ctrl) \
	( (id)        & 0xFF), \
	(((id) >>  8) & 0xFF), \
	(((id) >> 16) & 0xFF), \
	(((id) >> 24) & 0xFF), \
	type, \
	( (ctrl)       & 0xFF), \
	(((ctrl) >> 8) & 0xFF)

#define ISAPNP_IO_RANGE(info,min,max,align,len) \
	ISAPNP_SMALL_TAG(8,7), \
	(info), \
	((min) & 0xFF), (((min) >> 8) & 0xFF), \
	((max) & 0xFF), (((max) >> 8) & 0xFF), \
	(align), \
	(len)

#define ISAPNP_IRQ_SINGLE(irq,flags) \
	ISAPNP_SMALL_TAG(4,3), \
	((1 << (irq)) & 0xFF), \
	(((1 << (irq)) >> 8) & 0xFF), \
	(flags)

#define ISAPNP_IRQ(irqmask,flags) \
	ISAPNP_SMALL_TAG(4,3), \
	((irqmask) & 0xFF), \
	(((irqmask) >> 8) & 0xFF), \
	(flags)

#define ISAPNP_DMA_SINGLE(dma,flags) \
	ISAPNP_SMALL_TAG(5,2), \
	((1 << (dma)) & 0xFF), \
	(flags)

#define ISAPNP_DMA(dma,flags) \
	ISAPNP_SMALL_TAG(5,2), \
	((dma) & 0xFF), \
	(flags)

#define ISAPNP_END \
	ISAPNP_SMALL_TAG(0xF,1), 0x00

void ISA_PNP_devreg(ISAPnPDevice *x);
#endif
