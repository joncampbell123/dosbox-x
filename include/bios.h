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

#include "regionalloctracking.h"

#ifndef DOSBOX_BIOS_H
#define DOSBOX_BIOS_H

#define BIOS_BASE_ADDRESS_COM1                                  0x400 /* uint16_t */
#define BIOS_BASE_ADDRESS_COM2                                  0x402 /* uint16_t */
#define BIOS_BASE_ADDRESS_COM3                                  0x404 /* uint16_t */
#define BIOS_BASE_ADDRESS_COM4                                  0x406 /* uint16_t */
#define BIOS_ADDRESS_LPT1                                       0x408 /* uint16_t */
#define BIOS_ADDRESS_LPT2                                       0x40a /* uint16_t */
#define BIOS_ADDRESS_LPT3                                       0x40c /* uint16_t */
#define BIOS_ADDRESS_LPT4                                       0x40e /* uint16_t (older systems) */
#define BIOS_EXTENDED_BIOS_DATA_SEGMENT                         0x40e /* uint16_t (newer systems since PS/2) */
#define BIOS_CONFIGURATION                                      0x410 /* uint16_t */
                                                                /* [15:14] = number of LPT devices
                                                                 * [13:12] = reserved
                                                                 * [11: 9] = number of serial devices
                                                                 * [ 8: 8] = reserved
                                                                 * [ 7: 6] = number of floppy drives minus 1
                                                                 * [ 5: 4] = initial video mode
                                                                 *             00b = EGA,VGA,etc
                                                                 *             01b = 40x25 color text
                                                                 *             10b = 80x25 color text
                                                                 *             11b = 80x25 monochrome
                                                                 */
                                                             /* 0x412 is reserved (POST flag?) */
#define BIOS_MEMORY_SIZE                                        0x413 /* uint16_t, in KB, conventional memory */
#define BIOS_TRUE_MEMORY_SIZE                                   0x415 /* uint16_t, in KB, conventional memory (XT) */
                                                             /* 0x415 is reserved (AT error code?) */

#define BIOS_KEYBOARD_STATE                                     0x417
#define BIOS_KEYBOARD_FLAGS1                                    BIOS_KEYBOARD_STATE
#define BIOS_KEYBOARD_FLAGS1_RSHIFT_PRESSED                       (1 << 0)
#define BIOS_KEYBOARD_FLAGS1_LSHIFT_PRESSED                       (1 << 1)
#define BIOS_KEYBOARD_FLAGS1_CTRL_PRESSED                         (1 << 2)
#define BIOS_KEYBOARD_FLAGS1_ALT_PRESSED                          (1 << 3)
#define BIOS_KEYBOARD_FLAGS1_SCROLL_LOCK_ACTIVE                   (1 << 4)
#define BIOS_KEYBOARD_FLAGS1_NUMLOCK_ACTIVE                       (1 << 5)
#define BIOS_KEYBOARD_FLAGS1_CAPS_LOCK_ACTIVE                     (1 << 6)
#define BIOS_KEYBOARD_FLAGS1_INSERT_ACTIVE                        (1 << 7)

#define BIOS_KEYBOARD_FLAGS2                                    0x418
#define BIOS_KEYBOARD_FLAGS2_LCTRL_PRESSED                        (1 << 0)
#define BIOS_KEYBOARD_FLAGS2_LALT_PRESSED                         (1 << 1)
#define BIOS_KEYBOARD_FLAGS2_SYSTEMKEY_HELD                       (1 << 2)
#define BIOS_KEYBOARD_FLAGS2_SUSPENDKEY_TOGGLED                   (1 << 3) /* pause active */
#define BIOS_KEYBOARD_FLAGS2_SCROLL_LOCK_PRESSED                  (1 << 4)
#define BIOS_KEYBOARD_FLAGS2_NUM_LOCK_PRESSED                     (1 << 5)
#define BIOS_KEYBOARD_FLAGS2_CAPS_LOCK_PRESSED                    (1 << 6)
#define BIOS_KEYBOARD_FLAGS2_INSERT_PRESSED                       (1 << 7)

#define BIOS_KEYBOARD_TOKEN                                     0x419 /* Alt+Numpad work area */
#define BIOS_KEYBOARD_BUFFER_HEAD                               0x41a /* uint16_t */
#define BIOS_KEYBOARD_BUFFER_TAIL                               0x41c /* uint16_t */
#define BIOS_KEYBOARD_BUFFER                                    0x41e /* uint16_t[16] */
#define BIOS_DRIVE_ACTIVE                                       0x43e /* Floppy calibration status */
#define BIOS_DRIVE_RUNNING                                      0x43f /* Floppy motor status */
#define BIOS_DISK_MOTOR_TIMEOUT                                 0x440 /* Floppy motor timeout */
#define BIOS_DISK_STATUS                                        0x441 /* Floppy drive status */
#define BIOS_FLOPPY_CONTROLLER_STAT_CMD_REGS                    0x442 /* uint8_t[7] status/command bytes */
#define BIOS_VIDEO_MODE                                         0x449 /* Current INT 10h mode */
#define BIOS_SCREEN_COLUMNS                                     0x44a /* uint16_t */
#define BIOS_VIDEO_MEMORY_USED                                  0x44c /* uint16_t */
#define BIOS_VIDEO_MEMORY_ADDRESS                               0x44e /* uint16_t */
#define BIOS_VIDEO_CURSOR_POS                                   0x450 /* struct { uint8_t col, row } pos[8] */
#define BIOS_CURSOR_SHAPE                                       0x460 /* uint16_t */
#define BIOS_CURSOR_LAST_LINE                                   0x460 /* lower uint8_t */
#define BIOS_CURSOR_FIRST_LINE                                  0x461 /* upper uint8_t */
#define BIOS_CURRENT_SCREEN_PAGE                                0x462 /* uint8_t */
#define BIOS_VIDEO_PORT                                         0x463 /* uint16_t */
#define BIOS_VDU_CONTROL                                        0x465 /* CGA port 3D8h/MDA port 3B8h */
#define BIOS_VDU_COLOR_REGISTER                                 0x466 /* CGA port 3D9h */
#define BIOS_POST_RESET_VECTOR                                  0x467 /* uint32_t FAR 16:16 address to jump to after specific 286 reset */
#define BIOS_LAST_UNEXPECTED_IRQ                                0x46b
#define BIOS_TIMER                                              0x46c /* uint32_t Timer ticks (at 18.2Hz) since midnight */
#define BIOS_24_HOURS_FLAG                                      0x470 /* nonzero if timer tick rollover past midnight */
#define BIOS_CTRL_BREAK_FLAG                                    0x471 /* CTRL+Break flag (bit 7=1) */
#define BIOS_CTRL_ALT_DEL_FLAG                                  0x472 /* uint16_t POST reset flag 1234h=warm boot */
#define BIOS_FIXED_DISK_LAST_OP_STATUS                          0x474
#define BIOS_HARDDISK_COUNT                                     0x475
#define BIOS_FIXED_DISK_CONTROL_BYTE                            0x476 /* XT only */
#define BIOS_FIXED_DISK_IO_PORT_OFFSET                          0x477 /* XT only */
#define BIOS_LPT1_TIMEOUT                                       0x478
#define BIOS_LPT2_TIMEOUT                                       0x479
#define BIOS_LPT3_TIMEOUT                                       0x47a
#define BIOS_LPT4_TIMEOUT                                       0x47b
#define BIOS_VIRTUAL_DMA_SUPPORT                                0x47b /* set bit 5 to signal, see INT 4Bh */
#define BIOS_VIRTUAL_DMA_IS_SUPPORTED                             (1 << 5)
#define BIOS_COM1_TIMEOUT                                       0x47c
#define BIOS_COM2_TIMEOUT                                       0x47d
#define BIOS_COM3_TIMEOUT                                       0x47e
#define BIOS_COM4_TIMEOUT                                       0x47f
#define BIOS_KEYBOARD_BUFFER_START                              0x480 /* uint16_t */
#define BIOS_KEYBOARD_BUFFER_END                                0x482 /* uint16_t */
#define BIOS_ROWS_ON_SCREEN_MINUS_1                             0x484
#define BIOS_FONT_HEIGHT                                        0x485 /* uint16_t */
#define BIOS_VIDEO_INFO_0                                       0x487 /* EGA/VGA control */
#define BIOS_VIDEO_INFO_1                                       0x488 /* EGA/VGA switches */
#define BIOS_VIDEO_INFO_2                                       0x489 /* MCGA/VGA control */
#define BIOS_VIDEO_COMBO                                        0x48a /* MCGA/VGA Display Combination Code */

#define BIOS_KEYBOARD_PCJR_FLAG2                                0x488 /* KB_FLAG_2 (PCjr) */
#define BIOS_KEYBOARD_PCJR_FLAG2_PUTCHAR                          (1 << 0)
#define BIOS_KEYBOARD_PCJR_FLAG2_INIT_DELAY                       (1 << 1)
#define BIOS_KEYBOARD_PCJR_FLAG2_HALF_RATE                        (1 << 2)
#define BIOS_KEYBOARD_PCJR_FLAG2_TYPE_OFF                         (1 << 3)
#define BIOS_KEYBOARD_PCJR_FLAG2_FN_LOCK                          (1 << 4)
#define BIOS_KEYBOARD_PCJR_FLAG2_FN_PENDING                       (1 << 5)
#define BIOS_KEYBOARD_PCJR_FLAG2_FN_BREAK                         (1 << 6)
#define BIOS_KEYBOARD_PCJR_FLAG2_FN_FLAG                          (1 << 7)

#define BIOS_FLOPPY_MEDIA_CONTROL                               0x48b
#define BIOS_FIXED_DISK_CTRL_STATUS                             0x48c
#define BIOS_FIXED_DISK_CTRL_ERROR                              0x48d
#define BIOS_FIXED_DISK_INT_CTRL                                0x48e
#define BIOS_FLOPPY_CONTROLLER_INFORMATION                      0x48f
#define BIOS_FLOPPY_DRIVE_0_MEDIA_STATE                         0x490
#define BIOS_FLOPPY_DRIVE_1_MEDIA_STATE                         0x491
#define BIOS_FLOPPY_DRIVE_0_MEDIA_STATE_AT_START                0x492
#define BIOS_FLOPPY_DRIVE_1_MEDIA_STATE_AT_START                0x493
#define BIOS_FLOPPY_DRIVE_0_CURRENT_TRACK                       0x494
#define BIOS_FLOPPP_DRIVE_1_CURRENT_TRACK                       0x495

#define BIOS_KEYBOARD_FLAGS3                                    0x496
#define BIOS_KEYBOARD_FLAGS3_HIDDEN_E1                            (1 << 0)
#define BIOS_KEYBOARD_FLAGS3_HIDDEN_E0                            (1 << 1)
#define BIOS_KEYBOARD_FLAGS3_RCTRL_PRESSED                        (1 << 2)
#define BIOS_KEYBOARD_FLAGS3_RALT_PRESSED                         (1 << 3)
#define BIOS_KEYBOARD_FLAGS3_ENHANCED_KEYBOARD                    (1 << 4)
#define BIOS_KEYBOARD_FLAGS3_NUM_LOCK_FORCED                      (1 << 5)
#define BIOS_KEYBOARD_FLAGS3_ID_CHAR_WAS_LAST                     (1 << 6)
#define BIOS_KEYBOARD_FLAGS3_ID_READ_IN_PROCESS                   (1 << 7)

#define BIOS_KEYBOARD_LEDS                                      0x497
#define BIOS_KEYBOARD_LEDS_SCROLL_LOCK                            (1 << 0)
#define BIOS_KEYBOARD_LEDS_NUM_LOCK                               (1 << 1)
#define BIOS_KEYBOARD_LEDS_CAPS_LOCK                              (1 << 2)
#define BIOS_KEYBOARD_LEDS_CIRCUS                                 (1 << 3)
#define BIOS_KEYBOARD_LEDS_ACK                                    (1 << 4)
#define BIOS_KEYBOARD_LEDS_RESEND                                 (1 << 5)
#define BIOS_KEYBOARD_LEDS_MODE                                   (1 << 6)
#define BIOS_KEYBOARD_LEDS_TRANSMIT_ERROR                         (1 << 7)

#define BIOS_WAIT_FLAG_POINTER                                  0x498 /* uint32_t FAR 16:16 */
#define BIOS_WAIT_FLAG_COUNT                                    0x49c /* uint32_t */
#define BIOS_WAIT_FLAG_ACTIVE                                   0x4a0
#define BIOS_WAIT_FLAG_TEMP                                     0x4a1
#define BIOS_LAN_BYTES                                          0x4a1 /* uint8_t[7] */
#define BIOS_VIDEO_SAVEPTR                                      0x4a8 /* uint32_t FAR 16:16 */

#define BIOS_PRINT_SCREEN_FLAG                                  0x500

#define CURSOR_SCAN_LINE_NORMAL                                 (0x6)
#define CURSOR_SCAN_LINE_INSERT                                 (0x4)
#define CURSOR_SCAN_LINE_END                                    (0x7)

// NEC PC-98 BIOS data area

#define BIOS98_FLAG2                                            0x400 /* bit 7: 1=RAM drive function present [6:0]=a lot of other various stuff */
#define BIOS98_EXTMEM_286                                       0x401 /* extended memory between 0x100000-0xFFFFFF in 128KB units (286+ BIOS) */
#define BIOS98_SYS_SEL                                          0x402 /* SHIFT/CTRL/GRPH held down, [3:0] = hard disk boot partition selector (Hires) */
                                                             /* 0x403 ITF ROM work area */
#define BIOS98_286_RESET_VECTOR                                 0x404 /* uint32_t FAR 16:16, sets SS:SP to this value and RETF */
                                                             /* 0x408-0x40F various */
#define BIOS98_KB_SHIFT_CODE_HIRES                              0x408 /* Hi-res shift key redefinition table */
#define BIOS98_KB_BUFFER_ADR                                    0x410 /* uint32_t FAR 16:16 keyboard buffer address (hires) */
#define BIOS98_KB_ENTRY_TBL_ADR                                 0x414 /* uint32_t FAR 16:16 key data conversion table (hires) */
#define BIOS98_KB_INT_ADR                                       0x418 /* uint32_t FAR 16:16 keyboard interrupt table (hires) */
#define BIOS98_PR_TIME                                          0x41C /* uint16_t printer timeout in 10ms units */
                                                             /* 0x41E-0x447 various */
#define BIOS98_CAL_BEEP_TIME                                    0x448 /* uint16_t BEEP duration counter (hires) */
#define BIOS98_CAL_TONE                                         0x44A /* uint16_t BEEP tone (8253A counter value) */
#define BIOS98_CAL_USER_OFFSEG                                  0x44C /* uint32_t FAR 16:16 single-event timer timeout callback (hires) */
#define BIOS98_CRT_FONT                                         0x450 /* uint8_t[2] (port A1h,port A3h) value to output to CG (hires) */
#define BIOS98_RT_P1                                            0x452 /* (hires) cursor display, text scanline height */
#define BIOS98_RT_P2                                            0x453 /* (hires) cursor blink, top of cursor */
#define BIOS98_RT_P3                                            0x454 /* (hires) cursor blink, bottom of cursor */
                                                             /* 0x455-0x456 various */
#define BIOS98_IDE_HDD_CAPACITY                                 0x457 /* bit 7=1st IDE 512/256 byte/sect bit 6=2nd IDE 512/256 byte/sec bit 5-3=1st IDE capacity 2-0=2nd IDE capacity 0x00=Does not support IDE 0x3F=supports IDE but no drives */
#define BIOS98_MODEL_ID                                         0x458 /* bits that identify various models */
#define BIOS98_CRT_EXT_STS                                      0x459 /* [7:6]=ext attr mode [5:4]=various [3]=hires interlace [2:1]=various [0]=640x480 if set, 640x400 if not */
#define BIOS98_BIOS_FLAG6                                       0x45A /* either [7:0]=CPU clock or [7:4]=CPU type [3:0]=CPU clock */
#define BIOS98_BIOS_FLAG7                                       0x45B /* time stamper and port 5Fh wait among other various things */
                                                             /* 0x45C-0x45F various */
#define BIOS98_SCSI_PARAMETER_TABLE                             0x460 /* struct { uint8_t various[4]; } scsidev[8]; */
#define BIOS98_BIOS_FLAG1                                       0x480 /* [7]=SASI/IDE new sense [6]=EGC supported [5:0]=various */
#define BIOS98_BIOS_FLAG3                                       0x481 /* [6,3]=keyboard ID [5]=EMS page frame B0000h [4]=B0000h status (VRAM/EMS) various */
#define BIOS98_DISK_EQUIPS                                      0x482 /* [7:0]=SCSI #7-SCSI #0 (matches bit) HDD connected or not */
#define BIOS98_SCSI_STATUS                                      0x483
#define BIOS98_BIOS_FLAG4                                       0x484 /* [7:6]=HDD DMA channel [5]=builtin SCSI [4]=builtin SASI [3:0]=CPU speed */
#define BIOS98_DISK_MOTOR_STOP_COUNTER                          0x485 /* in 100ms units */
#define BIOS98_CPU386                                           0x486 /* uint16_t DX value after CPU reset, 386 or higher */
#define BIOS98_RDISK_EQUIP                                      0x488 /* [7:4]=640KB FD unit 3,2,1,0 [3:0]=1MB FD unit 3,2,1,0 is/isn't RAM drive */
#define BIOS98_RDISK_WORK_JMP                                   0x489 /* uint8_t[5] JMP FAR instruction to BIOS */
#define BIOS98_RDISK_FLAGS                                      0x48E /* [7]=processing [6:4]=SMM out [3]=FDD processing [2]=RAM drive number [1:0]=I/F mode */
                                                             /* 0x48F unused */
#define BIOS98_YET_ANOTHER_BEEP_PITCH                           0x490 /* uint16_t hires beep pitch */
#define BIOS98_DISK_RESET                                       0x492 /* [7:4]=recalibrate request 640KB unit 3,2,1,0 [3:0]=recalibrate request 1MB unit 3,2,1,0 */
#define BIOS98_F2HD_MODE                                        0x493 /* [7:4]=640KB FDD unit 3,2,1,0 80/40 cylinders [3:0]=640KB FDD unit 3,2,1,0 double/single sided */
#define BIOS98_DISK_EQUIP2                                      0x494 /* [7:4]=external FDD unit 3,2,1,0 1MB drive in 640KB mode */
#define BIOS98_GRAPH_CHG                                        0x495 /* GRCG/EGC mode register setting for use by interrupt handlers like mouse drivers */
#define BIOS98_GRAPH_TAL                                        0x496 /* uint8_t[4] GRCG tile registers */
                                                             /* 0x49A-0x4AB various */
#define BIOS98_XROM_PTR                                         0x49C /* uint32_t FAR 16:16 JMP destination address from INT 1Bh to extended BIOS */

extern Bitu BIOS_DEFAULT_IRQ0_LOCATION;                         // RealMake(0xf000,0xfea5)
extern Bitu BIOS_DEFAULT_IRQ1_LOCATION;                         // RealMake(0xf000,0xe987)
extern Bitu BIOS_DEFAULT_IRQ2_LOCATION;                         // RealMake(0xf000,0xff55)
extern Bitu BIOS_DEFAULT_HANDLER_LOCATION;                      // RealMake(0xf000,0xff53)
extern Bitu BIOS_DEFAULT_INT5_LOCATION;                         // RealMake(0xf000,0xff54)
extern Bitu BIOS_VIDEO_TABLE_LOCATION;                          // RealMake(0xf000,0xf0a4)
extern Bitu BIOS_DEFAULT_RESET_LOCATION;                        // RealMake(0xf000,0xe05b)
extern Bitu BIOS_VIDEO_TABLE_SIZE;

Bitu ROMBIOS_GetMemory(Bitu bytes,const char *who=NULL,Bitu alignment=1,Bitu must_be_at=0);

extern RegionAllocTracking rombios_alloc;

/* maximum of scancodes handled by keyboard bios routines */
#define MAX_SCAN_CODE 0x7F

/* The Section handling Bios Disk Access */
//#define BIOS_MAX_DISK 10

//#define MAX_SWAPPABLE_DISKS 20

void BIOS_ZeroExtendedSize(bool in);

bool BIOS_AddKeyToBuffer(uint16_t code);

void INT10_ReloadRomFonts();

void BIOS_SetLPTPort (Bitu port, uint16_t baseaddr);

// Is it safe to call mem_read/write as if part of the guest i.e. from the main GUI?
// If the guest is running in protected mode, NO.
// If the guest is not running under our own DOS kernel, NO.
// This is required to avoid problems with helpful code in this project causing BSODs in Windows NT/2000/XP
// because the BIOS data area addresses are invalid pages in that 32-bit environment!
bool IsSafeToMemIOOnBehalfOfGuest();

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
                unsigned char       ident[9]; /* 72-bit vendor + serial + checksum identity */
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

extern bool ACPI_enabled;
extern uint32_t ACPI_BASE;
extern uint32_t ACPI_REGION_SIZE; // power of 2
extern uint32_t ACPI_version;
extern unsigned char *ACPI_buffer;
extern size_t ACPI_buffer_size;
extern int ACPI_IRQ;
extern unsigned int ACPI_SMI_CMD;

void ACPI_mem_enable(const bool enable);
void ACPI_free();
bool ACPI_init();

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

/* WARNING: do not wrap type macro parameter in parens i.e. (type), it breaks PNP
 *          structures and Windows 9x/ME will fail to boot! */
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
