/*
 *  Copyright (C) 2002-2019  The DOSBox Team
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335, USA.
 */

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

#include "regionalloctracking.h"

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
#define BIOS_KEYBOARD_FLAGS1_RSHIFT_PRESSED			(1 << 0)
#define BIOS_KEYBOARD_FLAGS1_LSHIFT_PRESSED			(1 << 1)
#define BIOS_KEYBOARD_FLAGS1_CTRL_PRESSED			(1 << 2)
#define BIOS_KEYBOARD_FLAGS1_ALT_PRESSED			(1 << 3)
#define BIOS_KEYBOARD_FLAGS1_SCROLL_LOCK_ACTIVE		(1 << 4)
#define BIOS_KEYBOARD_FLAGS1_NUMLOCK_ACTIVE			(1 << 5)
#define BIOS_KEYBOARD_FLAGS1_CAPS_LOCK_ACTIVE		(1 << 6)
#define BIOS_KEYBOARD_FLAGS1_INSERT_ACTIVE			(1 << 7)

#define BIOS_KEYBOARD_FLAGS2            0x418
#define BIOS_KEYBOARD_FLAGS2_LCTRL_PRESSED			(1 << 0)
#define BIOS_KEYBOARD_FLAGS2_LALT_PRESSED			(1 << 1)
#define BIOS_KEYBOARD_FLAGS2_SYSTEMKEY_HELD			(1 << 2)
#define BIOS_KEYBOARD_FLAGS2_SUSPENDKEY_TOGGLED		(1 << 3)
#define BIOS_KEYBOARD_FLAGS2_SCROLL_LOCK_PRESSED	(1 << 4)
#define BIOS_KEYBOARD_FLAGS2_NUM_LOCK_PRESSED		(1 << 5)
#define BIOS_KEYBOARD_FLAGS2_CAPS_LOCK_PRESSED		(1 << 6)
#define BIOS_KEYBOARD_FLAGS2_INSERT_PRESSED			(1 << 7)

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
#define BIOS_LAST_UNEXPECTED_IRQ        0x46b
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
#define BIOS_KEYBOARD_FLAGS3_HIDDEN_E1			(1 << 0)
#define BIOS_KEYBOARD_FLAGS3_HIDDEN_E0			(1 << 1)
#define BIOS_KEYBOARD_FLAGS3_RCTRL_PRESSED		(1 << 2)
#define BIOS_KEYBOARD_FLAGS3_RALT_PRESSED		(1 << 3)
#define BIOS_KEYBOARD_FLAGS3_ENHANCED_KEYBOARD	(1 << 4)
#define BIOS_KEYBOARD_FLAGS3_NUM_LOCK_FORCED	(1 << 5)
#define BIOS_KEYBOARD_FLAGS3_ID_CHAR_WAS_LAST	(1 << 6)
#define BIOS_KEYBOARD_FLAGS3_ID_READ_IN_PROCESS	(1 << 7)

#define BIOS_KEYBOARD_LEDS              0x497
#define BIOS_KEYBOARD_LEDS_SCROLL_LOCK    (1 << 0)
#define BIOS_KEYBOARD_LEDS_NUM_LOCK       (1 << 1)
#define BIOS_KEYBOARD_LEDS_CAPS_LOCK      (1 << 2)
#define BIOS_KEYBOARD_LEDS_CIRCUS         (1 << 3)
#define BIOS_KEYBOARD_LEDS_ACK            (1 << 4)
#define BIOS_KEYBOARD_LEDS_RESEND         (1 << 5)
#define BIOS_KEYBOARD_LEDS_MODE           (1 << 6)
#define BIOS_KEYBOARD_LEDS_TRANSMIT_ERROR (1 << 7)

#define BIOS_WAIT_FLAG_POINTER          0x498
#define BIOS_WAIT_FLAG_COUNT	        0x49c		
#define BIOS_WAIT_FLAG_ACTIVE			0x4a0
#define BIOS_WAIT_FLAG_TEMP				0x4a1


#define BIOS_PRINT_SCREEN_FLAG          0x500

#define BIOS_VIDEO_SAVEPTR              0x4a8

#define CURSOR_SCAN_LINE_NORMAL			(0x6)
#define CURSOR_SCAN_LINE_INSERT			(0x4)
#define CURSOR_SCAN_LINE_END			(0x7)

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
extern Bitu BIOS_DEFAULT_INT5_LOCATION;		// (RealMake(0xf000,0xff54))
extern Bitu BIOS_VIDEO_TABLE_LOCATION;		// (RealMake(0xf000,0xf0a4))
extern Bitu BIOS_DEFAULT_RESET_LOCATION;	// RealMake(0xf000,0xe05b)
extern Bitu BIOS_VIDEO_TABLE_SIZE;

Bitu ROMBIOS_GetMemory(Bitu bytes,const char *who=NULL,Bitu alignment=1,Bitu must_be_at=0);

extern RegionAllocTracking rombios_alloc;

/* maximum of scancodes handled by keyboard bios routines */
#define MAX_SCAN_CODE 0x58

/* The Section handling Bios Disk Access */
//#define BIOS_MAX_DISK 10

//#define MAX_SWAPPABLE_DISKS 20

void BIOS_ZeroExtendedSize(bool in);

bool BIOS_AddKeyToBuffer(Bit16u code);

void INT10_ReloadRomFonts();

void BIOS_SetLPTPort (Bitu port, Bit16u baseaddr);

// \brief Synchronizes emulator num lock state with host.
void BIOS_SynchronizeNumLock();

// \brief Synchronizes emulator caps lock state with host.
void BIOS_SynchronizeCapsLock();

// \brief Synchronizes emulator scroll lock state with host.
void BIOS_SynchronizeScrollLock();

bool ISAPNP_RegisterSysDev(const unsigned char *raw,Bitu len,bool already=false);

enum {
    UNHANDLED_IRQ_SIMPLE=0,
    UNHANDLED_IRQ_COOPERATIVE_2ND       // PC-98 type IRQ 8-15 handling
};

extern int unhandled_irq_method;

class ISAPnPDevice {
public:
	ISAPnPDevice();
	virtual ~ISAPnPDevice();
public:
	void checksum_ident();
public:
	enum SmallTags {
		PlugAndPlayVersionNumber =		0x1,
		LogicalDeviceID =			0x2,
		CompatibleDeviceID =			0x3,
		IRQFormat =				0x4,
		DMAFormat =				0x5,
		StartDependentFunctions =		0x6,
		EndDependentFunctions =			0x7,
		IOPortDescriptor =			0x8,
		FixedLocationIOPortDescriptor =		0x9,
		SmallVendorDefined =			0xE,
		EndTag =				0xF
	};
	enum LargeTags {
		MemoryRangeDescriptor =			0x1,
		IdentifierStringANSI =			0x2,
		IdentifierStringUNICODE =		0x3,
		LargeVendorDefined =			0x4,
		MemoryRange32Descriptor =		0x5,
		FixedLocationMemoryRangeDescriptor =	0x6
	};
	// StartDependentFunction config
	enum DependentFunctionConfig {
		PreferredDependentConfiguration =	0x0,
		AcceptableDependentConfiguration =	0x1,
		SubOptimalDependentConfiguration =	0x2
	};
	// IRQ format, signal types (bitfield)
	enum {
		IRQFormatInfo_HighTrueEdgeSensitive =	0x1,
		IRQFormatInfo_LowTrueEdgeSensitive =	0x2,
		IRQFormatInfo_HighTrueLevelSensitive =	0x4,
		IRQFormatInfo_LowTrueLevelSensitive =	0x8
	};
	// IRQ format, helper IRQ mask generator
	static inline uint16_t irq2mask(const int IRQ) {
		if (IRQ < 0 || IRQ > 15) return 0;
		return (uint16_t)(1U << (unsigned char)IRQ);
	}
	static inline uint16_t irq2mask(const int a,const int b) {
		return irq2mask(a) | irq2mask(b);
	}
	static inline uint16_t irq2mask(const int a,const int b,const int c) {
		return irq2mask(a) | irq2mask(b) | irq2mask(c);
	}
	static inline uint16_t irq2mask(const int a,const int b,const int c,const int d) {
		return irq2mask(a) | irq2mask(b) | irq2mask(c) | irq2mask(d);
	}
	static inline uint16_t irq2mask(const int a,const int b,const int c,const int d,const int e) {
		return irq2mask(a) | irq2mask(b) | irq2mask(c) | irq2mask(d) | irq2mask(e);
	}
	// DMA format transfer type
	enum {
		DMATransferType_8bitOnly=0,
		DMATransferType_8_and_16bit=1,
		DMATransferType_16bitOnly=2
	};
	enum {
		DMASpeedSupported_Compat=0,
		DMASpeedSupported_TypeA=1,
		DMASpeedSupported_TypeB=2,
		DMASpeedSupported_TypeF=3
	};
	// DMA format
	static inline uint16_t dma2mask(const int DMA) {
		if (DMA < 0 || DMA > 7 || DMA == 4) return 0;
		return (uint16_t)(1U << (unsigned char)DMA);
	}
	static inline uint16_t dma2mask(const int a,const int b) {
		return dma2mask(a) | dma2mask(b);
	}
	static inline uint16_t dma2mask(const int a,const int b,const int c) {
		return dma2mask(a) | dma2mask(b) | dma2mask(c);
	}
	static inline uint16_t dma2mask(const int a,const int b,const int c,const int d) {
		return dma2mask(a) | dma2mask(b) | dma2mask(c) | dma2mask(d);
	}
	static inline uint16_t dma2mask(const int a,const int b,const int c,const int d,const int e) {
		return dma2mask(a) | dma2mask(b) | dma2mask(c) | dma2mask(d) | dma2mask(e);
	}
public:
	virtual void config(Bitu val);
	virtual void wakecsn(Bitu val);
	virtual void select_logical_device(Bitu val);
	virtual void on_pnp_key();
	/* ISA PnP I/O data read/write */
	virtual uint8_t read(Bitu addr);
	virtual void write(Bitu addr,Bitu val);
	virtual bool alloc(size_t sz);
	void write_begin_SMALLTAG(const ISAPnPDevice::SmallTags stag,unsigned char len);
	void write_begin_LARGETAG(const ISAPnPDevice::LargeTags stag,unsigned int len);
	void write_nstring(const char *str,const size_t l);
	void write_byte(const unsigned char c);
	void begin_write_res();
	void end_write_res();
	void write_END();
	void write_ISAPnP_version(unsigned char major,unsigned char minor,unsigned char vendor);
	void write_Identifier_String(const char *str);
	void write_Logical_Device_ID(const char c1,const char c2,const char c3,const char c4,const char c5,const char c6,const char c7);
	void write_Compatible_Device_ID(const char c1,const char c2,const char c3,const char c4,const char c5,const char c6,const char c7);
	void write_Device_ID(const char c1,const char c2,const char c3,const char c4,const char c5,const char c6,const char c7);
	void write_Dependent_Function_Start(const ISAPnPDevice::DependentFunctionConfig cfg,const bool force=false);
	void write_IRQ_Format(const uint16_t IRQ_mask,const unsigned char IRQ_signal_type=0);
	void write_DMA_Format(const uint8_t DMA_mask,const unsigned char transfer_type_preference,const bool is_bus_master,const bool byte_mode,const bool word_mode,const unsigned char speed_supported);
	void write_IO_Port(const uint16_t min_port,const uint16_t max_port,const uint8_t count,const uint8_t alignment=1,const bool full16bitdecode=true);
	void write_End_Dependent_Functions();
public:
    unsigned char       CSN = 0;
    unsigned char       logical_device = 0;
	unsigned char		ident[9];		/* 72-bit vendor + serial + checksum identity */
    unsigned char       ident_bp = 0;   /* bit position of identity read */
    unsigned char       ident_2nd = 0;
    unsigned char       resource_ident = 0;
    unsigned char*      resource_data = NULL;
    size_t              resource_data_len = 0;
    unsigned int        resource_data_pos = 0;
    size_t              alloc_write = 0;
    unsigned char*      alloc_res = NULL;
    size_t              alloc_sz = 0;
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
