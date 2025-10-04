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

#include <assert.h>
#include "control.h"
#include "dosbox.h"
#include "mem.h"
#include "cpu.h"
#include "bios.h"
#include "regs.h"
#include "timer.h"
#include "cpu.h"
#include "bitop.h"
#include "callback.h"
#include "inout.h"
#include "pic.h"
#include "hardware.h"
#include "pci_bus.h"
#include "joystick.h"
#include "mouse.h"
#include "callback.h"
#include "setup.h"
#include "bios_disk.h"
#include "serialport.h"
#include "mapper.h"
#include "vga.h"
#include "jfont.h"
#include "shiftjis.h"
#include "pc98_gdc.h"
#include "pc98_gdc_const.h"
#include "regionalloctracking.h"
#include "build_timestamp.h"
#include "int10.h"
extern bool PS1AudioCard;
#include "parport.h"
#include "dma.h"
#include "shell.h"
#include "render.h"
#include "sdlmain.h"
#include <time.h>
#include <sys/stat.h>
#include "version_string.h"

#define DOSBOX_INCLUDE
#include "iglib.h"

#if C_LIBPNG
#include <png.h>
#endif

#if defined(DB_HAVE_CLOCK_GETTIME) && ! defined(WIN32)
//time.h is already included
#else
#include <sys/timeb.h>
#endif

#if C_EMSCRIPTEN
# include <emscripten.h>
#endif

#include <output/output_ttf.h>

#if defined(_MSC_VER)
# pragma warning(disable:4244) /* const fmath::local::uint64_t to double possible loss of data */
# pragma warning(disable:4305) /* truncation from double to float */
#endif

#if defined(WIN32) && !defined(S_ISREG)
# define S_ISREG(x) ((x & S_IFREG) == S_IFREG)
#endif

static unsigned char ig_cpu_cycles_set = 0;
static uint32_t ig_cpu_cycles_value = 0;
static uint8_t ig_cpu_cycles_max_percent = 0;

struct BIOS_E280_entry {
	uint64_t	base = 0;
	uint64_t	length = 0;
	uint32_t	type = 0;
	uint32_t	acpi_ext_addr = 0;
};

#define MAX_E280	16
static BIOS_E280_entry	E280_table[MAX_E280];
static unsigned int	E280_table_entries = 0;

bool VGA_InitBiosLogo(unsigned int w,unsigned int h,unsigned int x,unsigned int y);
void VGA_WriteBiosLogoBMP(unsigned int y,unsigned char *scanline,unsigned int w);
void VGA_WriteBiosLogoPalette(unsigned int start,unsigned int count,unsigned char *rgb);
void VGA_FreeBiosLogo(void);
void VGA_ShowBIOSLogo(void);

extern bool ega200;

unsigned char ACPI_ENABLE_CMD = 0xA1;
unsigned char ACPI_DISABLE_CMD = 0xA0;
unsigned int ACPI_IO_BASE = 0x820;
unsigned int ACPI_PM1A_EVT_BLK = 0x820;
unsigned int ACPI_PM1A_CNT_BLK = 0x824;
unsigned int ACPI_PM_TMR_BLK = 0x830;
/* debug region 0x840-0x84F */
unsigned int ACPI_DEBUG_IO = 0x840;

std::string ibm_rom_basic;
size_t ibm_rom_basic_size = 0;
uint32_t ibm_rom_basic_base = 0;

/* NTS: The "Epson check" code in Windows 2.1 only compares up to the end of "NEC Corporation" */
const std::string pc98_copyright_str = "Copyright (C) 1983 by NEC Corporation / Microsoft Corp.\x0D\x0A";

/* more strange data involved in the "Epson check" */
const unsigned char pc98_epson_check_2[0x27] = {
    0x26,0x8A,0x05,0xA8,0x10,0x75,0x11,0xC6,0x06,0xD6,0x09,0x1B,0xC6,0x06,0xD7,0x09,
    0x4B,0xC6,0x06,0xD8,0x09,0x48,0xEB,0x0F,0xC6,0x06,0xD6,0x09,0x1A,0xC6,0x06,0xD7 ,
    0x09,0x70,0xC6,0x06,0xD8,0x09,0x71
};

bool enable_pc98_copyright_string = false;

/* mouse.cpp */
extern bool pc98_40col_text;
extern bool en_bios_ps2mouse;
extern bool rom_bios_8x8_cga_font;
extern bool pcibus_enable;
extern bool enable_fpu;
extern bool finish_prepare;
extern bool is_ttfswitched_on;

bool pc98_timestamp5c = true; // port 5ch and 5eh "time stamp/hardware wait"

uint32_t Keyb_ig_status();
bool VM_Boot_DOSBox_Kernel();
Bitu bios_post_parport_count();
Bitu bios_post_comport_count();
void pc98_update_cpu_page_ptr(void);
bool KEYBOARD_Report_BIOS_PS2Mouse();
bool gdc_5mhz_according_to_bios(void);
void pc98_update_display_page_ptr(void);
void pc98_update_palette(void);
bool MEM_map_ROM_alias_physmem(Bitu start,Bitu end);
void MOUSE_Startup(Section *sec);
void Mouse_PS2SetSamplingRate(uint8_t rate);
bool Mouse_PS2SetPacketSize(uint8_t packet_size);
void change_output(int output);
void runBoot(const char *str);
void SetIMPosition(void);
bool isDBCSCP();
Bitu INT60_Handler(void);
Bitu INT6F_Handler(void);
bool toOutput(const char* what);
#if defined(USE_TTF)
void ttf_switch_on(bool ss), ttf_switch_off(bool ss), ttf_setlines(int cols, int lins);
#endif
std::string conf_output;

/* Rate limit log entries regarding APM AH=05h CPU IDLE because Windows 98's APM driver likes to call it way too much per second */
pic_tickindex_t APM_log_cpu_idle_next_report = 0;
unsigned long APM_log_cpu_idle = 0;

unsigned long APM_WakeupKeys = 0;

/* APM events (eventually ACPI as well) */
unsigned long PowerButtonClicks = 0;
bool APM_ResumeNotificationFromSuspend = false;
bool APM_ResumeNotificationFromStandby = false;
bool APM_PowerButtonSendsSuspend = true;


bool bochs_port_e9 = false;
bool isa_memory_hole_512kb = false;
bool isa_memory_hole_15mb = false;
bool int15_wait_force_unmask_irq = false;

int unhandled_irq_method = UNHANDLED_IRQ_SIMPLE;

unsigned int reset_post_delay = 0;

Bitu call_irq_default = 0;
uint16_t biosConfigSeg=0;

static constexpr unsigned int ACPI_PMTIMER_CLOCK_RATE = 3579545; /* 3.579545MHz */

pic_tickindex_t ACPI_PMTIMER_BASE_TIME = 0;
uint32_t ACPI_PMTIMER_BASE_COUNT = 0;
uint32_t ACPI_PMTIMER_MASK = 0xFFFFFFu; /* 24-bit mode */

uint32_t ACPI_PMTIMER(void) {
	pic_tickindex_t pt = PIC_FullIndex() - ACPI_PMTIMER_BASE_TIME;
	uint32_t ct = (uint32_t)((pt * ACPI_PMTIMER_CLOCK_RATE) / 1000.0);
	return ct;
}

void ACPI_PMTIMER_Event(Bitu /*val*/);
void ACPI_PMTIMER_ScheduleNext(void) {
	const uint32_t now_ct = ACPI_PMTIMER() & ACPI_PMTIMER_MASK;
	const uint32_t next_delay_ct = (ACPI_PMTIMER_MASK + 1u) - now_ct;
	const pic_tickindex_t delay = (1000.0 * next_delay_ct) / (pic_tickindex_t)ACPI_PMTIMER_CLOCK_RATE;

	LOG(LOG_MISC,LOG_DEBUG)("ACPI PM TIMER SCHEDULE: now=0x%x next=0x%x delay=%.3f",now_ct,next_delay_ct,(double)delay);
	PIC_AddEvent(ACPI_PMTIMER_Event,delay);
}

void ACPI_PMTIMER_CHECK(void) { /* please don't call this often */
	PIC_RemoveEvents(ACPI_PMTIMER_Event);
	LOG(LOG_MISC,LOG_DEBUG)("ACPI PM TIMER CHECK");
	ACPI_PMTIMER_ScheduleNext();
}

Bitu BIOS_PC98_KEYBOARD_TRANSLATION_LOCATION = ~0u;
Bitu BIOS_DEFAULT_IRQ0_LOCATION = ~0u;       // (RealMake(0xf000,0xfea5))
Bitu BIOS_DEFAULT_IRQ1_LOCATION = ~0u;       // (RealMake(0xf000,0xe987))
Bitu BIOS_DEFAULT_IRQ07_DEF_LOCATION = ~0u;  // (RealMake(0xf000,0xff55))
Bitu BIOS_DEFAULT_IRQ815_DEF_LOCATION = ~0u; // (RealMake(0xf000,0xe880))

Bitu BIOS_DEFAULT_HANDLER_LOCATION = ~0u;    // (RealMake(0xf000,0xff53))
Bitu BIOS_DEFAULT_INT5_LOCATION = ~0u;       // (RealMake(0xf000,0xff54))

Bitu BIOS_VIDEO_TABLE_LOCATION = ~0u;        // RealMake(0xf000,0xf0a4)
Bitu BIOS_VIDEO_TABLE_SIZE = 0u;

Bitu BIOS_DEFAULT_RESET_LOCATION = ~0u;      // RealMake(0xf000,0xe05b)
Bitu BIOS_DEFAULT_RESET_CODE_LOCATION = ~0u;

bool allow_more_than_640kb = false;

unsigned int APM_BIOS_connected_minor_version = 0;// what version the OS connected to us with. default to 1.0
unsigned int APM_BIOS_minor_version = 2;    // what version to emulate e.g to emulate 1.2 set this to 2

static bool apm_realmode_connected = false;

/* default bios type/version/date strings */
const char* const bios_type_string = "IBM COMPATIBLE BIOS for DOSBox-X";
const char* const bios_version_string = "DOSBox-X BIOS v1.0";
const char* const bios_date_string = "01/01/92";

bool                        APM_inactivity_timer = true;

RegionAllocTracking             rombios_alloc;

Bitu                        rombios_minimum_location = 0xF0000; /* minimum segment allowed */
Bitu                        rombios_minimum_size = 0x10000;

static bool ACPI_SCI_EN = false;
static bool ACPI_BM_RLD = false;

static IO_Callout_t acpi_iocallout = IO_Callout_t_none;

static unsigned int ACPI_PM1_Enable = 0;
static unsigned int ACPI_PM1_Status = 0;
static constexpr unsigned int ACPI_PM1_Enable_TMR_EN = (1u << 0u);
static constexpr unsigned int ACPI_PM1_Enable_GBL_EN = (1u << 5u);
static constexpr unsigned int ACPI_PM1_Enable_PWRBTN_EN = (1u << 8u);
static constexpr unsigned int ACPI_PM1_Enable_SLPBTN_EN = (1u << 9u);
static constexpr unsigned int ACPI_PM1_Enable_RTC_EN = (1u << 10u);

unsigned int ACPI_buffer_global_lock = 0;

unsigned long ACPI_FACS_GlobalLockValue(void) {
	if (ACPI_buffer && ACPI_buffer_global_lock != 0)
		return host_readd(ACPI_buffer+ACPI_buffer_global_lock);

	return 0;
}

/* Triggered by GBL_RLS bit */
static void ACPI_OnGuestGlobalRelease(void) {
	LOG(LOG_MISC,LOG_DEBUG)("ACPI GBL_RLS event. Global lock = %lx",ACPI_FACS_GlobalLockValue());
}

bool ACPI_GuestEnabled(void) {
	return ACPI_SCI_EN;
}

static void ACPI_SCI_Check(void) {
	if (ACPI_SCI_EN) {
		if (ACPI_PM1_Status & ACPI_PM1_Enable) {
			LOG(LOG_MISC,LOG_DEBUG)("ACPI SCI interrupt");
			PIC_ActivateIRQ(ACPI_IRQ);
		}
	}
}

void ACPI_PowerButtonEvent(void) {
	if (ACPI_SCI_EN) {
		if (!(ACPI_PM1_Status & ACPI_PM1_Enable_PWRBTN_EN)) {
			ACPI_PM1_Status |= ACPI_PM1_Enable_PWRBTN_EN;
			ACPI_SCI_Check();
		}
	}
}

void ACPI_PMTIMER_Event(Bitu /*val*/) {
	if (!(ACPI_PM1_Status & ACPI_PM1_Enable_TMR_EN)) {
		ACPI_PM1_Status |= ACPI_PM1_Enable_TMR_EN;
		ACPI_SCI_Check();
	}

	ACPI_PMTIMER_ScheduleNext();
}

/* you can't very well write strings with this, but you could write codes */
static void acpi_cb_port_debug_w(Bitu /*port*/,Bitu val,Bitu /*iolen*/) {
	LOG(LOG_MISC,LOG_DEBUG)("ACPI debug: 0x%x\n",(unsigned int)val);
}

static void acpi_cb_port_smi_cmd_w(Bitu /*port*/,Bitu val,Bitu /*iolen*/) {
	/* 8-bit SMI_CMD port */
	LOG(LOG_MISC,LOG_DEBUG)("ACPI SMI_CMD %x",(unsigned int)val);

	if (val == ACPI_ENABLE_CMD) {
		if (!ACPI_SCI_EN) {
			LOG(LOG_MISC,LOG_DEBUG)("Guest enabled ACPI");
			ACPI_SCI_EN = true;
			ACPI_PMTIMER_CHECK();
			ACPI_SCI_Check();
		}
	}
	else if (val == ACPI_DISABLE_CMD) {
		if (ACPI_SCI_EN) {
			LOG(LOG_MISC,LOG_DEBUG)("Guest disabled ACPI");
			ACPI_PMTIMER_CHECK();
			ACPI_SCI_EN = false;
		}
	}
}

static Bitu acpi_cb_port_cnt_blk_r(Bitu port,Bitu /*iolen*/) {
	/* 16-bit register (PM1_CNT_LEN == 2) */
	Bitu ret = 0;
	if (ACPI_SCI_EN) ret |= (1u << 0u);
	if (ACPI_BM_RLD) ret |= (1u << 1u);
	/* GBL_RLS is write only */
	/* TODO: bits 3-8 are "reserved by the ACPI driver"? So are they writeable then? */
	/* TODO: SLP_TYPx */
	/* SLP_EN is write-only */

	LOG(LOG_MISC,LOG_DEBUG)("ACPI_PM1_CNT_BLK read port %x ret %x",(unsigned int)port,(unsigned int)ret);
	return ret;
}

static void acpi_cb_port_cnt_blk_w(Bitu port,Bitu val,Bitu iolen) {
	/* 16-bit register (PM1_CNT_LEN == 2) */
	LOG(LOG_MISC,LOG_DEBUG)("ACPI_PM1_CNT_BLK write port %x val %x iolen %x",(unsigned int)port,(unsigned int)val,(unsigned int)iolen);

	/* BIOS controls SCI_EN and therefore guest cannot change it */
	ACPI_BM_RLD = !!(val & (1u << 1u));
	/* GLB_RLS is write only and triggers an SMI interrupt to pass execution to the BIOS, usually to indicate a release of the global lock and set of pending bit */
	if (val & (1u << 2u)/*GBL_RLS*/) ACPI_OnGuestGlobalRelease();
	/* TODO: bits 3-8 are "reserved by the ACPI driver"? So are they writeable then? */
	/* TODO: SLP_TYPx */
	/* SLP_EN is write-only */
}

static Bitu acpi_cb_port_evtst_blk_r(Bitu port,Bitu /*iolen*/) {
	/* 16-bit register (PM1_EVT_LEN/2 == 2) */
	Bitu ret = ACPI_PM1_Status;
	LOG(LOG_MISC,LOG_DEBUG)("ACPI_PM1_EVT_BLK(status) read port %x ret %x",(unsigned int)port,(unsigned int)ret);
	return ret;
}

static void acpi_cb_port_evtst_blk_w(Bitu port,Bitu val,Bitu iolen) {
	/* 16-bit register (PM1_EVT_LEN/2 == 2) */
	LOG(LOG_MISC,LOG_DEBUG)("ACPI_PM1_EVT_BLK(status) write port %x val %x iolen %x",(unsigned int)port,(unsigned int)val,(unsigned int)iolen);
	ACPI_PM1_Status &= (~val);
	ACPI_SCI_Check();
}

static Bitu acpi_cb_port_evten_blk_r(Bitu port,Bitu /*iolen*/) {
	/* 16-bit register (PM1_EVT_LEN/2 == 2) */
	Bitu ret = ACPI_PM1_Enable;
	LOG(LOG_MISC,LOG_DEBUG)("ACPI_PM1_EVT_BLK(enable) read port %x ret %x",(unsigned int)port,(unsigned int)ret);
	return ret;
}

static Bitu acpi_cb_port_tmr_r(Bitu port,Bitu /*iolen*/) {
	/* 24 or 32-bit TMR_VAL (depends on the mask value and whether our ACPI structures tell the OS it's 32-bit wide) */
	Bitu ret = (Bitu)ACPI_PMTIMER();
	LOG(LOG_MISC,LOG_DEBUG)("ACPI_PM_TMR read port %x ret %x",(unsigned int)port,(unsigned int)ret);
	return ret;
}

static void acpi_cb_port_evten_blk_w(Bitu port,Bitu val,Bitu iolen) {
	/* 16-bit register (PM1_EVT_LEN/2 == 2) */
	LOG(LOG_MISC,LOG_DEBUG)("ACPI_PM1_EVT_BLK(enable) write port %x val %x iolen %x",(unsigned int)port,(unsigned int)val,(unsigned int)iolen);
	ACPI_PM1_Enable = val;
	ACPI_SCI_Check();
}

static IO_ReadHandler* acpi_cb_port_r(IO_CalloutObject &co,Bitu port,Bitu iolen) {
	(void)co;
	(void)iolen;

	if ((port & (~1u)) == (ACPI_PM1A_EVT_BLK+0) && iolen >= 2)
		return acpi_cb_port_evtst_blk_r;
	else if ((port & (~1u)) == (ACPI_PM1A_EVT_BLK+2) && iolen >= 2)
		return acpi_cb_port_evten_blk_r;
	else if ((port & (~1u)) == ACPI_PM1A_CNT_BLK && iolen >= 2)
		return acpi_cb_port_cnt_blk_r;
	/* The ACPI specification says nothing about reading SMI_CMD so assume that means write only */
	else if ((port & (~3u)) == ACPI_PM_TMR_BLK && iolen >= 4)
		return acpi_cb_port_tmr_r;

	return NULL;
}

static IO_WriteHandler* acpi_cb_port_w(IO_CalloutObject &co,Bitu port,Bitu iolen) {
	(void)co;
	(void)iolen;

	if ((port & (~1u)) == (ACPI_PM1A_EVT_BLK+0) && iolen >= 2)
		return acpi_cb_port_evtst_blk_w;
	else if ((port & (~1u)) == (ACPI_PM1A_EVT_BLK+2) && iolen >= 2)
		return acpi_cb_port_evten_blk_w;
	else if ((port & (~1u)) == ACPI_PM1A_CNT_BLK && iolen >= 2)
		return acpi_cb_port_cnt_blk_w;
	else if ((port & (~3u)) == ACPI_SMI_CMD)
		return acpi_cb_port_smi_cmd_w;
	else if (port == ACPI_DEBUG_IO && iolen >= 4)
		return acpi_cb_port_debug_w;
	else if ((port & (~3u)) == ACPI_PM_TMR_BLK) {
		LOG(LOG_MISC,LOG_DEBUG)("write ACPI_PM_TMR_BLK port=0x%x iolen=%u",(unsigned int)port,(unsigned int)iolen);
	}

	return NULL;
}

bool MEM_map_ROM_physmem(Bitu start,Bitu end);
bool MEM_unmap_physmem(Bitu start,Bitu end);

static std::string bochs_port_e9_line;

static void bochs_port_e9_flush() {
    if (!bochs_port_e9_line.empty()) {
        LOG_MSG("Bochs port E9h: %s",bochs_port_e9_line.c_str());
        bochs_port_e9_line.clear();
    }
}

void bochs_port_e9_write(Bitu port,Bitu val,Bitu /*iolen*/) {
    (void)port;//UNUSED
    if (val == '\n' || val == '\r') {
        bochs_port_e9_flush();
    }
    else {
        bochs_port_e9_line += (char)val;
        if (bochs_port_e9_line.length() >= 256)
            bochs_port_e9_flush();
    }
}

LoopHandler *DOSBOX_GetLoop(void);

static Bitu APM_SuspendedLoopFunc(void);

static RealPt APM_SuspendedLoopRptr=0;

/* "wakeup" keys were pressed and released */
void APM_Suspend_Wakeup_Key(void) {
        APM_WakeupKeys++;
}

void APM_SuspendedLoopLeaveState() {
	APM_WakeupKeys = 0;
	PowerButtonClicks = 0;

	/* Turn on the VGA display, assuming it was on when suspended */
	if (IS_VGA_ARCH) {
		Bitu crtc;
		Bitu tmp;

		IO_Write(0x3C4,0x01); // clocking mode
		tmp = IO_Read(0x3C5);
		IO_Write(0x3C5,tmp & (~0x20)); // turn on screen

		// NTS: vga_crtc.cpp/vga_draw.cpp does not emulate blanking the display upon disabling sync signals
		crtc = (IO_Read(0x3CC) & 1) ? 0x3D4 : 0x3B4;
		IO_Write(crtc,0x17); // mode control
		tmp = IO_Read(crtc+1);
		IO_Write(crtc+1,tmp | 0x80); // enable sync
	}
}

/* callback for APM suspended loop routine in BIOS */
static Bitu APM_SuspendedLoopFunc(void) {
        if (APM_WakeupKeys != 0 || PowerButtonClicks != 0) {
		APM_SuspendedLoopLeaveState();
                LOG_MSG("APM: leaving suspended state");
                reg_eip += 3; /* skip over HLT+JMP to get to RET */
                return CBRET_NONE;
        }

        return CBRET_NONE;
}

bool PowerManagementEnabledButton() {
	if (IS_PC98_ARCH) /* power management not yet known or implemented */
		return false;

	if (ACPI_GuestEnabled())
		ACPI_PowerButtonEvent();

	if (apm_realmode_connected) /* guest has connected to the APM BIOS */
		return true;

	return false;
}

void APM_BeginSuspendedMode(void) {
        /* Enable interrupts, dumbass. We use HLT here. */
        CPU_STI();

        /* WARNING: This assumes, like all callbacks, that they reside in the same segment.
         * The APM BIOS entry point can be called from real mode, 16-bit or 32-bit protected mode,
         * therefore this code cannot assume any particular segment value and can only safely
         * change reg_(e)ip to direct CPU execution. The most likely scenario is that the
         * return address pushed here will point at the IRET in the INT 15 handler. */
        if (cpu.stack.big) CPU_Push32(reg_eip);
        else CPU_Push16(reg_ip);

        reg_ip = APM_SuspendedLoopRptr & 0xFFFF; /* offset only */

        /* reset counters */
        PowerButtonClicks = 0;
        APM_WakeupKeys = 0;

	/* Turn off the VGA display */
	if (IS_VGA_ARCH) {
		Bitu crtc;
		Bitu tmp;

		IO_Write(0x3C4,0x01); // clocking mode
		tmp = IO_Read(0x3C5);
		IO_Write(0x3C5,tmp | 0x20); // turn off screen

		// NTS: vga_crtc.cpp/vga_draw.cpp does not emulate blanking the display upon disabling sync signals
		crtc = (IO_Read(0x3CC) & 1) ? 0x3D4 : 0x3B4;
		IO_Write(crtc,0x17); // mode control
		tmp = IO_Read(crtc+1);
		IO_Write(crtc+1,tmp & (~0x80)); // disable sync
	}

        LOG_MSG("System is now in APM suspend mode");
}


void ROMBIOS_DumpMemory() {
    rombios_alloc.logDump();
}

void ROMBIOS_SanityCheck() {
    rombios_alloc.sanityCheck();
}

Bitu ROMBIOS_MinAllocatedLoc() {
    Bitu r = rombios_alloc.getMinAddress();

    if (r > (0x100000u - rombios_minimum_size))
        r = (0x100000u - rombios_minimum_size);

    return r & ~0xFFFu;
}

void ROMBIOS_FreeUnusedMinToLoc(Bitu phys) {
    Bitu new_phys;

    if (rombios_minimum_location & 0xFFF) E_Exit("ROMBIOS: FreeUnusedMinToLoc minimum location not page aligned");

    phys &= ~0xFFFUL;
    new_phys = rombios_alloc.freeUnusedMinToLoc(phys) & (~0xFFFUL);
    assert(new_phys >= phys);
    if (phys < new_phys) MEM_unmap_physmem(phys,new_phys-1);
    rombios_minimum_location = new_phys;
    ROMBIOS_SanityCheck();
    ROMBIOS_DumpMemory();
}

bool ROMBIOS_FreeMemory(Bitu phys) {
    return rombios_alloc.freeMemory(phys);
}

Bitu ROMBIOS_GetMemory(Bitu bytes,const char *who,Bitu alignment,Bitu must_be_at) {
    return rombios_alloc.getMemory(bytes,who,alignment,must_be_at);
}

void ROMBIOS_InitForCustomBIOS(void) {
    rombios_alloc.initSetRange(0xD8000,0xE0000);
}

static IO_Callout_t dosbox_int_iocallout = IO_Callout_t_none;

static unsigned char dosbox_int_register_shf = 0;
static uint32_t dosbox_int_register = 0;
static unsigned char dosbox_int_regsel_shf = 0;
static uint32_t dosbox_int_regsel = 0;
static bool dosbox_int_error = false;
static bool dosbox_int_busy = false;

#define STRINGIZE_HELPER(A) #A
#define STRINGIZE(A) STRINGIZE_HELPER(A)
#define PPCAT_HELPER(A, B, C) A ## . ## B ## . ## C
#define PPCAT(A, B, C) PPCAT_HELPER(A, B, C)

#define INTDEV_VERSION_BUMP 0
#define INTDEV_VERSION_MAJOR 1
#define INTDEV_VERSION_MINOR 1
#define INTDEV_VERSION_SUB 0

static const char *dosbox_int_version = "DOSBox-X integration device v" STRINGIZE(PPCAT(INTDEV_VERSION_MAJOR, INTDEV_VERSION_MINOR, INTDEV_VERSION_SUB));
static const char *dosbox_int_ver_read = NULL;

struct dosbox_int_saved_state {
    const char*     dosbox_int_ver_read;
    unsigned char   dosbox_int_register_shf;
    uint32_t        dosbox_int_register;
    unsigned char   dosbox_int_regsel_shf;
    uint32_t        dosbox_int_regsel;
    bool            dosbox_int_error;
    bool            dosbox_int_busy;
};

#define DOSBOX_INT_SAVED_STATE_MAX      4

struct dosbox_int_saved_state       dosbox_int_saved[DOSBOX_INT_SAVED_STATE_MAX];
int                                 dosbox_int_saved_sp = -1;

/* for use with interrupt handlers in DOS/Windows that need to save IG state
 * to ensure that IG state is restored on return in order to not interfere
 * with anything userspace is doing (as an alternative to wrapping all access
 * in CLI/STI or PUSHF/CLI/POPF) */
bool dosbox_int_push_save_state(void) {
	if (dosbox_int_saved_sp >= (DOSBOX_INT_SAVED_STATE_MAX-1))
		return false;

	struct dosbox_int_saved_state *ss = &dosbox_int_saved[++dosbox_int_saved_sp];

	ss->dosbox_int_ver_read =           dosbox_int_ver_read;
	ss->dosbox_int_register_shf =       dosbox_int_register_shf;
	ss->dosbox_int_register =           dosbox_int_register;
	ss->dosbox_int_regsel_shf =         dosbox_int_regsel_shf;
	ss->dosbox_int_regsel =             dosbox_int_regsel;
	ss->dosbox_int_error =              dosbox_int_error;
	ss->dosbox_int_busy =               dosbox_int_busy;
	return true;
}

bool dosbox_int_pop_save_state(void) {
	if (dosbox_int_saved_sp < 0)
		return false;

	struct dosbox_int_saved_state *ss = &dosbox_int_saved[dosbox_int_saved_sp--];

	dosbox_int_ver_read =               ss->dosbox_int_ver_read;
	dosbox_int_register_shf =           ss->dosbox_int_register_shf;
	dosbox_int_register =               ss->dosbox_int_register;
	dosbox_int_regsel_shf =             ss->dosbox_int_regsel_shf;
	dosbox_int_regsel =                 ss->dosbox_int_regsel;
	dosbox_int_error =                  ss->dosbox_int_error;
	dosbox_int_busy =                   ss->dosbox_int_busy;
	return true;
}

bool dosbox_int_discard_save_state(void) {
	if (dosbox_int_saved_sp < 0)
		return false;

	dosbox_int_saved_sp--;
	return true;
}

extern bool user_cursor_locked;
extern int user_cursor_x,user_cursor_y;
extern int user_cursor_sw,user_cursor_sh;
extern int master_cascade_irq,bootdrive;
extern bool enable_slave_pic;
extern bool bootguest, use_quick_reboot;
extern uint16_t countryNo;
bool bootvm = false, bootfast = false;
static std::string dosbox_int_debug_out;

uint32_t Mixer_MIXQ(void);
uint32_t VGA_QuerySizeIG(void);

/* read triggered, update the regsel */
void dosbox_integration_trigger_read() {
	dosbox_int_error = false;

	switch (dosbox_int_regsel) {
		case DOSBOX_ID_REG_IDENTIFY:
			dosbox_int_register = 0xD05B0740;
			break;
		case DOSBOX_ID_REG_TEST:
			break;
		case DOSBOX_ID_REG_VERSION_STRING:
			if (dosbox_int_ver_read == NULL)
				dosbox_int_ver_read = dosbox_int_version;

			dosbox_int_register = 0;
			for (Bitu i=0;i < 4;i++) {
				if (*dosbox_int_ver_read == 0) {
					dosbox_int_ver_read = dosbox_int_version;
					break;
				}

				dosbox_int_register += ((uint32_t)((unsigned char)(*dosbox_int_ver_read++))) << (uint32_t)(i * 8);
			}
			break;
		case DOSBOX_ID_REG_VERSION_NUMBER:
			dosbox_int_register = INTDEV_VERSION_MAJOR + (INTDEV_VERSION_MINOR << 8U) + (INTDEV_VERSION_SUB << 16U) + (INTDEV_VERSION_BUMP << 24U);
			break;
		case DOSBOX_ID_REG_READ_EMTIME: /* current emulator time as 16.16 fixed point */
			dosbox_int_register = (uint32_t)(PIC_FullIndex() * 0x10000);
			break;
		case DOSBOX_ID_REG_DOSBOX_VERSION_MAJOR: // DOSBox-X version number major (e.g. 2022)
			{
				const char * ver = strchr(VERSION, '.');
				dosbox_int_register = ver == NULL ? 0 : atoi(std::string(VERSION).substr(0, strlen(ver) - strlen(VERSION)).c_str());
				break;
			}
		case DOSBOX_ID_REG_DOSBOX_VERSION_MINOR: // DOSBox-X version number minor (e.g. 8)
			{
				const char * ver = strchr(VERSION, '.');
				dosbox_int_register = ver == NULL ? 0 : atoi(ver + 1);
				break;
			}
		case DOSBOX_ID_REG_DOSBOX_VERSION_SUB: // DOSBox-X version number sub (e.g. 0)
			{
				const char * ver = strchr(VERSION, '.');
				ver = strchr(ver + 1, '.');
				dosbox_int_register = ver == NULL ? 0 : atoi(ver + 1);
				break;
			}
		case DOSBOX_ID_REG_DOSBOX_PLATFORM_TYPE: // DOSBox-X platform type
			dosbox_int_register = 0;
#if defined(HX_DOS)
			dosbox_int_register = 4;
#elif defined(WIN32)
			dosbox_int_register = 1;
#elif defined(LINUX)
			dosbox_int_register = 2;
#elif defined(MACOSX)
			dosbox_int_register = 3;
#elif defined(OS2)
			dosbox_int_register = 5;
#else
			dosbox_int_register = 0;
#endif
			if (control->opt_securemode || control->SecureMode()) dosbox_int_register = 0;
#if OS_BIT_INT == 64
			dosbox_int_register += 0x20; // 64-bit
#else
			dosbox_int_register += 0x10; // 32-bit
#endif
#if defined(C_SDL2)
			dosbox_int_register += 0x200; // SDL2
#elif defined(SDL_DOSBOX_X_SPECIAL)
			dosbox_int_register += 0x100; // SDL1 (modified)
#endif
			break;
		case DOSBOX_ID_REG_DOSBOX_MACHINE_TYPE: // DOSBox-X machine type
			dosbox_int_register = machine;
			break;

		case DOSBOX_ID_REG_MIXER_QUERY: /* query mixer output 'MIXQ' */
			/* bits [19:0] = sample rate in Hz or 0 if mixer is not mixing AT ALL
			 * bits [23:20] = number of channels (at this time, always 2 aka stereo)
			 * bits [29:29] = 1=swap stereo  0=normal
			 * bits [30:30] = 1=muted  0=not muted
			 * bits [31:31] = 1=sound  0=nosound */
			dosbox_int_register = Mixer_MIXQ();
			break;

		case DOSBOX_ID_REG_DOS_KERNEL_STATUS: // DOS kernel status
			dosbox_int_register = dos_kernel_disabled ? 0: 1;
			break;
		case DOSBOX_ID_REG_DOS_KERNEL_CODEPAGE: // DOS codepage number
			dosbox_int_register = dos_kernel_disabled ? 0: dos.loaded_codepage;
			break;
		case DOSBOX_ID_REG_DOS_KERNEL_COUNTRY: // DOS country number
			dosbox_int_register = dos_kernel_disabled ? 0: countryNo;
			break;
		case DOSBOX_ID_REG_DOS_KERNEL_VERSION_MAJOR: // DOS version major
			dosbox_int_register = dos_kernel_disabled ? 0: dos.version.major;
			break;
		case DOSBOX_ID_REG_DOS_KERNEL_VERSION_MINOR: // DOS version minor
			dosbox_int_register = dos_kernel_disabled ? 0: dos.version.minor;
			break;
		case DOSBOX_ID_REG_DOS_KERNEL_ERROR_CODE: // DOS error code
			dosbox_int_register = dos_kernel_disabled ? 0 : dos.errorcode;
			break;
		case DOSBOX_ID_REG_DOS_KERNEL_BOOT_DRIVE: // DOS boot drive
			dosbox_int_register = bootdrive;
			break;
		case DOSBOX_ID_REG_DOS_KERNEL_CURRENT_DRIVE: // DOS current drive
			dosbox_int_register = dos_kernel_disabled ? 0 : DOS_GetDefaultDrive();
			break;
		case DOSBOX_ID_REG_DOS_KERNEL_LFN_STATUS: // DOS LFN status
			dosbox_int_register = dos_kernel_disabled || !uselfn ? 0: 1;
			break;

		case DOSBOX_ID_REG_GET_VGA_SIZE: /* query VGA display size */
			dosbox_int_register = VGA_QuerySizeIG();
			break;

		case DOSBOX_ID_REG_GET_WINDOW_SIZE: /* query window size (so user mouse cursor has meaning) */
			dosbox_int_register = user_cursor_sw | (user_cursor_sh << 16);
			break;

		case DOSBOX_ID_REG_8237_INJECT_READ: /* ISA DMA injection, single byte/word (read from memory) */
			break;

		case DOSBOX_ID_REG_8259_PIC_INFO: /* PIC configuration */
			/* bits [7:0] = cascade interrupt or 0xFF if none
			 * bit  [8:8] = primary PIC present
			 * bit  [9:9] = secondary PIC present */
			if (master_cascade_irq >= 0)
				dosbox_int_register = ((unsigned int)master_cascade_irq & 0xFFu);
			else
				dosbox_int_register = 0xFFu;

			dosbox_int_register |= 0x100; // primary always present
			if (enable_slave_pic) dosbox_int_register |= 0x200;
			break;

		case DOSBOX_ID_REG_8042_KB_STATUS: /* keyboard status */
			dosbox_int_register = Keyb_ig_status();
			break;

		case DOSBOX_ID_REG_USER_MOUSE_STATUS: /* read user mouse status */
			dosbox_int_register =
				(user_cursor_locked        ? (1UL << 0UL) : 0UL) |    /* bit 0 = mouse capture lock */
				(sdl.mouse.locked          ? (1UL << 1UL) : 0UL);     /* bit 1 = mouse captured */
			break;

		case DOSBOX_ID_REG_USER_MOUSE_CURSOR: /* read user mouse cursor position */
			dosbox_int_register = (uint32_t((uint16_t)user_cursor_y & 0xFFFFUL) << 16UL) | uint32_t((uint16_t)user_cursor_x & 0xFFFFUL);
			break;

		case DOSBOX_ID_REG_USER_MOUSE_CURSOR_NORMALIZED:
			{ /* read user mouse cursor position (normalized for Windows 1.x/2.x/3.x/95/98/ME mouse driver interface) */
				signed long long x = ((signed long long)user_cursor_x << 16LL) / (signed long long)(user_cursor_sw-1);
				signed long long y = ((signed long long)user_cursor_y << 16LL) / (signed long long)(user_cursor_sh-1);
				if (x < 0x0000LL) x = 0x0000LL;
				if (x > 0xFFFFLL) x = 0xFFFFLL;
				if (y < 0x0000LL) y = 0x0000LL;
				if (y > 0xFFFFLL) y = 0xFFFFLL;
				dosbox_int_register = ((unsigned int)y << 16UL) | (unsigned int)x;
			} break;

		case DOSBOX_ID_REG_SCREENSHOT_TRIGGER: /* Screenshot/capture trigger */
			       /* TODO: This should be designed so that rogue DOS development cannot abuse this interface
				*       to prevent the user from recording anything */
#if (C_SSHOT)
			dosbox_int_register = 0x00000000; // available
			if (CaptureState & CAPTURE_IMAGE)
				dosbox_int_register |= 1 << 0; // Image capture is in progress
			if (CaptureState & CAPTURE_VIDEO)
				dosbox_int_register |= 1 << 1; // Video capture is in progress
			if (CaptureState & CAPTURE_WAVE)
				dosbox_int_register |= 1 << 2; // WAVE capture is in progress
#else
			dosbox_int_register = 0xC0000000; // not available (bit 31 set), not enabled (bit 30 set)
#endif
			break;

		case DOSBOX_ID_RESET_INDEX_CODE: /* interface reset result */
			break;

		case DOSBOX_ID_REG_CPU_CYCLES:
			ig_cpu_cycles_set = 0;
			if (CPU_CycleMax < 0x80000000)
				dosbox_int_register = (uint32_t)CPU_CycleMax;
			else
				dosbox_int_register = (uint32_t)0x7FFFFFFFUL;
			break;

		case DOSBOX_ID_REG_CPU_MAX_PERCENT:
			dosbox_int_register = CPU_CyclePercUsed;
			ig_cpu_cycles_set = 0;
			break;

		case DOSBOX_ID_REG_CPU_CYCLES_INFO:
			ig_cpu_cycles_set = 0;
			dosbox_int_register = 0;
			if (CPU_CycleAutoAdjust)
			       dosbox_int_register |= DOSBOX_ID_REG_CPU_CYCLES_INFO_MAX;
			else if (CPU_AutoDetermineMode &CPU_AUTODETERMINE_CYCLES)
			       dosbox_int_register |= DOSBOX_ID_REG_CPU_CYCLES_INFO_AUTO;
			else
			       dosbox_int_register |= DOSBOX_ID_REG_CPU_CYCLES_INFO_FIXED;
			break;

		default:
			dosbox_int_register = 0xAA55AA55;
			dosbox_int_error = true;
			break;
	}

	LOG(LOG_MISC,LOG_DEBUG)("DOSBox-X integration read 0x%08lx got 0x%08lx (err=%u)\n",
			(unsigned long)dosbox_int_regsel,
			(unsigned long)dosbox_int_register,
			dosbox_int_error?1:0);
}

bool watchdog_set = false;

void Watchdog_Timeout_Event(Bitu /*val*/) {
	LOG_MSG("Watchdog timeout occurred");
	CPU_Raise_NMI();
}

void Watchdog_Timer_Clear(void) {
	if (watchdog_set) {
		PIC_RemoveEvents(Watchdog_Timeout_Event);
		watchdog_set = false;
	}
}

void Watchdog_Timer_Set(uint32_t timeout_ms) {
	Watchdog_Timer_Clear();

	if (timeout_ms != 0) {
		watchdog_set = true;
		PIC_AddEvent(Watchdog_Timeout_Event,(double)timeout_ms);
	}
}

unsigned int mouse_notify_mode = 0;
// 0 = off
// 1 = trigger as PS/2 mouse interrupt

/* write triggered */
void dosbox_integration_trigger_write() {
	dosbox_int_error = false;

	LOG(LOG_MISC,LOG_DEBUG)("DOSBox-X integration write 0x%08lx val 0x%08lx\n",
			(unsigned long)dosbox_int_regsel,
			(unsigned long)dosbox_int_register);

	switch (dosbox_int_regsel) {
		case DOSBOX_ID_REG_TEST:
			break;

		case DOSBOX_ID_REG_VERSION_STRING:
			dosbox_int_ver_read = NULL;
			break;

		case DOSBOX_ID_REG_DEBUG_OUT: /* debug output (to log) */
			for (unsigned int b=0;b < 4;b++) {
				unsigned char c = (unsigned char)(dosbox_int_register >> (b * 8U));
				if (c == '\n' || dosbox_int_debug_out.length() >= 200) {
					LOG_MSG("Client debug message: %s\n",dosbox_int_debug_out.c_str());
					dosbox_int_debug_out.clear();
				}
				else if (c != 0) {
					dosbox_int_debug_out += ((char)c);
				}
				else {
					break;
				}
			}
			dosbox_int_register = 0;
			break;

		case DOSBOX_ID_REG_DEBUG_CLEAR: /* debug output clear */
			dosbox_int_debug_out.clear();
			break;

		case DOSBOX_ID_REG_RELEASE_MOUSE_CAPTURE: /* release mouse capture 'MCR' */
			void GFX_ReleaseMouse(void);
			GFX_ReleaseMouse();
			break;

		case DOSBOX_ID_REG_SET_WATCHDOG: /* Set/clear watchdog timer 'WATD' */
			Watchdog_Timer_Set(dosbox_int_register);
			break;

		case DOSBOX_ID_REG_INJECT_NMI: /* NMI (INT 02h) interrupt injection */
			{
				dosbox_int_register_shf = 0;
				dosbox_int_regsel_shf = 0;
				CPU_Raise_NMI();
			}
			break;

		case DOSBOX_ID_REG_8259_INJECT_IRQ: /* PIC interrupt injection */
			{
				dosbox_int_register_shf = 0;
				dosbox_int_regsel_shf = 0;
				/* bits  [7:0]  = IRQ to signal (must be 0-15)
				 * bit   [8:8]  = 1=raise 0=lower IRQ */
				uint8_t IRQ = dosbox_int_register&0xFFu;
				bool raise = (dosbox_int_register>>8u)&1u;

				if (IRQ < 16) {
					if (raise)
						PIC_ActivateIRQ(IRQ);
					else
						PIC_DeActivateIRQ(IRQ);
				}
			}
			break;

		case DOSBOX_ID_REG_8237_INJECT_WRITE: /* ISA DMA injection, single byte/word (write to memory) */
			{
				dosbox_int_register_shf = 0;
				dosbox_int_regsel_shf = 0;
				/* bits  [7:0]  = data byte if 8-bit DNA
				 * bits [15:0]  = data word if 16-bit DMA
				 * bits [18:16] = DMA channel to send to */
				DmaChannel *ch = GetDMAChannel((dosbox_int_register>>16u)&7u);
				if (ch != NULL) {
					unsigned char tmp[2];

					tmp[0] = (unsigned char)( dosbox_int_register         & 0xFFu);
					tmp[1] = (unsigned char)((dosbox_int_register >>  8u) & 0xFFu);

					/* NTS: DMA channel write will write tmp[0] if 8-bit, tmp[0]/tmp[1] if 16-bit */
					if (ch->Write(1/*one unit of transfer*/,tmp) == 1) {
						dosbox_int_register = 0;
						dosbox_int_error = false;
					}
					else {
						dosbox_int_register = 0x823700;
						dosbox_int_error = true;
					}
				}
				else {
					dosbox_int_register = 0x8237FF;
					dosbox_int_error = true;
				}
			}
			break;

		case DOSBOX_ID_REG_8237_INJECT_READ: /* ISA DMA injection, single byte/word (read from memory) */
			{
				dosbox_int_register_shf = 0;
				dosbox_int_regsel_shf = 0;
				/* bits [18:16] = DMA channel to send to */
				DmaChannel *ch = GetDMAChannel((dosbox_int_register>>16u)&7u);
				if (ch != NULL) {
					unsigned char tmp[2];

					/* NTS: DMA channel write will write tmp[0] if 8-bit, tmp[0]/tmp[1] if 16-bit */
					tmp[0] = tmp[1] = 0;
					if (ch->Read(1/*one unit of transfer*/,tmp) == 1) {
						dosbox_int_register = ((unsigned int)tmp[1] << 8u) + (unsigned int)tmp[0];
						dosbox_int_error = false;
					}
					else {
						dosbox_int_register = 0x823700;
						dosbox_int_error = true;
					}
				}
				else {
					dosbox_int_register = 0x8237FF;
					dosbox_int_error = true;
				}
			}
			break;

		case DOSBOX_ID_REG_8042_KB_INJECT: /* keyboard input injection */
			void Mouse_ButtonPressed(uint8_t button);
			void Mouse_ButtonReleased(uint8_t button);
			void pc98_keyboard_send(const unsigned char b);
			void Mouse_CursorMoved(float xrel,float yrel,float x,float y,bool emulate);
			void KEYBOARD_AUX_Event(float x,float y,Bitu buttons,int scrollwheel);
			void KEYBOARD_AddBuffer(uint16_t data);

			switch ((dosbox_int_register>>8)&0xFF) {
				case 0x00: // keyboard
					if (IS_PC98_ARCH)
						pc98_keyboard_send(dosbox_int_register&0xFF);
					else
						KEYBOARD_AddBuffer(dosbox_int_register&0xFF);
					break;
				case 0x01: // AUX
					if (!IS_PC98_ARCH)
						KEYBOARD_AddBuffer((dosbox_int_register&0xFF)|0x100/*AUX*/);
					else   // no such interface in PC-98 mode
						dosbox_int_error = true;
					break;
				case 0x08: // mouse button injection
					if (dosbox_int_register&0x80) Mouse_ButtonPressed(dosbox_int_register&0x7F);
					else Mouse_ButtonReleased(dosbox_int_register&0x7F);
					break;
				case 0x09: // mouse movement injection (X)
					Mouse_CursorMoved(((dosbox_int_register>>16UL) / 256.0f) - 1.0f,0,0,0,true);
					break;
				case 0x0A: // mouse movement injection (Y)
					Mouse_CursorMoved(0,((dosbox_int_register>>16UL) / 256.0f) - 1.0f,0,0,true);
					break;
				case 0x0B: // mouse scrollwheel injection
					// TODO
					break;
				default:
					dosbox_int_error = true;
					break;
			}
			break;

			/* this command is used to enable notification of mouse movement over the windows even if the mouse isn't captured */
		case DOSBOX_ID_REG_USER_MOUSE_CURSOR: /* read user mouse cursor position */
		case DOSBOX_ID_REG_USER_MOUSE_CURSOR_NORMALIZED: /* read user mouse cursor position (normalized for Windows 3.x) */
			mouse_notify_mode = dosbox_int_register & 0xFF;
			LOG(LOG_MISC,LOG_DEBUG)("Mouse notify mode=%u",mouse_notify_mode);
			break;

		case DOSBOX_ID_REG_SCREENSHOT_TRIGGER: /* Screenshot/capture trigger */
#if (C_SSHOT)
			void CAPTURE_ScreenShotEvent(bool pressed);
			void CAPTURE_VideoEvent(bool pressed);
#endif
			void CAPTURE_WaveEvent(bool pressed);

			/* TODO: It would be wise to grant/deny access to this register through another dosbox-x.conf option
			 *       so that rogue DOS development cannot shit-spam the capture folder */
#if (C_SSHOT)
			if (dosbox_int_register & 1)
				CAPTURE_ScreenShotEvent(true);
			if (dosbox_int_register & 2)
				CAPTURE_VideoEvent(true);
#endif
			if (dosbox_int_register & 4)
				CAPTURE_WaveEvent(true);
			break;

		case DOSBOX_ID_REG_CPU_CYCLES:
			ig_cpu_cycles_set |= 1u;
			ig_cpu_cycles_value = dosbox_int_register;
			if (ig_cpu_cycles_value == 0) ig_cpu_cycles_value = 3000;
			if (ig_cpu_cycles_value > 0x7FFFFFFFul) ig_cpu_cycles_value = 0x7FFFFFFFul;
			break;

		case DOSBOX_ID_REG_CPU_MAX_PERCENT:
			ig_cpu_cycles_set |= 2u;
			ig_cpu_cycles_max_percent = dosbox_int_register;
			if (ig_cpu_cycles_max_percent == 0) ig_cpu_cycles_max_percent = 100;
			if (ig_cpu_cycles_max_percent > 100) ig_cpu_cycles_max_percent = 100;
			break;

		case DOSBOX_ID_REG_CPU_CYCLES_INFO:
			dosbox_int_error = false;
			{
				unsigned int mode = dosbox_int_register & DOSBOX_ID_REG_CPU_CYCLES_INFO_MODE_MASK;
				char setting[256] = {0};

				if (mode == 0) {
					if (CPU_CycleAutoAdjust)
						mode = DOSBOX_ID_REG_CPU_CYCLES_INFO_MAX;
					else if (CPU_AutoDetermineMode &CPU_AUTODETERMINE_CYCLES)
						mode = DOSBOX_ID_REG_CPU_CYCLES_INFO_AUTO;
					else
						mode = DOSBOX_ID_REG_CPU_CYCLES_INFO_FIXED;
				}

				switch (mode) {
					case DOSBOX_ID_REG_CPU_CYCLES_INFO_MAX:
						if (ig_cpu_cycles_set & 2)
							sprintf(setting,"cycles=max %lu%%",(unsigned long)ig_cpu_cycles_max_percent);
						break;
					case DOSBOX_ID_REG_CPU_CYCLES_INFO_AUTO:
						if (ig_cpu_cycles_set & 2)
							sprintf(setting,"cycles=auto %lu%%",(unsigned long)ig_cpu_cycles_max_percent);
						break;
					case DOSBOX_ID_REG_CPU_CYCLES_INFO_FIXED:
						if (ig_cpu_cycles_set & 1)
							sprintf(setting,"cycles=fixed %lu",(unsigned long)ig_cpu_cycles_value);
						break;
					default:
						dosbox_int_error = true;
						break;
				};

				if (setting) {
					LOG(LOG_MISC,LOG_DEBUG)("Integratoin device cycle change: %s\n",setting);
					Section* sec = control->GetSection("cpu");
					if (sec) sec->HandleInputline(setting);
				}
				else {
					LOG(LOG_MISC,LOG_DEBUG)("Integratoin device cycle change ignored (setmask 0x%x mode 0x%x)\n",
						ig_cpu_cycles_set,mode);
				}
			}
			ig_cpu_cycles_set = 0;
			break;

		default:
			dosbox_int_register = 0x55AA55AA;
			dosbox_int_error = true;
			break;
	}
}

/* PORT 0x28: Index
 *      0x29: Data
 *      0x2A: Status(R) or Command(W)
 *      0x2B: Not yet assigned
 *
 *      Registers are 32-bit wide. I/O to index and data rotate through the
 *      bytes of the register depending on I/O length, meaning that one 32-bit
 *      I/O read will read the entire register, while four 8-bit reads will
 *      read one byte out of 4. */

static Bitu dosbox_integration_port00_index_r(Bitu port,Bitu iolen) {
	(void)port;//UNUSED
	Bitu retb = 0;
	Bitu ret = 0;

	while (iolen > 0) {
		ret += ((dosbox_int_regsel >> (dosbox_int_regsel_shf * 8)) & 0xFFU) << (retb * 8);
		if ((++dosbox_int_regsel_shf) >= 4) dosbox_int_regsel_shf = 0;
		iolen--;
		retb++;
	}

	return ret;
}

static void dosbox_integration_port00_index_w(Bitu port,Bitu val,Bitu iolen) {
	(void)port;//UNUSED

	while (iolen > 0) {
		uint32_t msk = 0xFFU << (dosbox_int_regsel_shf * 8);
		dosbox_int_regsel = (dosbox_int_regsel & ~msk) + ((val & 0xFF) << (dosbox_int_regsel_shf * 8));
		if ((++dosbox_int_regsel_shf) >= 4) dosbox_int_regsel_shf = 0;
		val >>= 8U;
		iolen--;
	}
}

static Bitu dosbox_integration_port01_data_r(Bitu port,Bitu iolen) {
	(void)port;//UNUSED
	Bitu retb = 0;
	Bitu ret = 0;

	while (iolen > 0) {
		if (dosbox_int_register_shf == 0) dosbox_integration_trigger_read();
		ret += ((dosbox_int_register >> (dosbox_int_register_shf * 8)) & 0xFFU) << (retb * 8);
		if ((++dosbox_int_register_shf) >= 4) dosbox_int_register_shf = 0;
		iolen--;
		retb++;
	}

	return ret;
}

static void dosbox_integration_port01_data_w(Bitu port,Bitu val,Bitu iolen) {
	(void)port;//UNUSED

	while (iolen > 0) {
		uint32_t msk = 0xFFU << (dosbox_int_register_shf * 8);
		dosbox_int_register = (dosbox_int_register & ~msk) + ((val & 0xFF) << (dosbox_int_register_shf * 8));
		if ((++dosbox_int_register_shf) >= 4) dosbox_int_register_shf = 0;
		if (dosbox_int_register_shf == 0) dosbox_integration_trigger_write();
		val >>= 8U;
		iolen--;
	}
}

static Bitu dosbox_integration_port02_status_r(Bitu port,Bitu iolen) {
	(void)iolen;//UNUSED
	(void)port;//UNUSED
	/* status */
	/* 7:6 = regsel byte index
	 * 5:4 = register byte index
	 * 3:2 = reserved
	 *   1 = error
	 *   0 = busy */
	return
		((unsigned int)dosbox_int_regsel_shf << 6u) + ((unsigned int)dosbox_int_register_shf << 4u) +
		(dosbox_int_error ? 2u : 0u) + (dosbox_int_busy ? 1u : 0u);
}

static void dosbox_integration_port02_command_w(Bitu port,Bitu val,Bitu iolen) {
	(void)port;
	(void)iolen;
	switch (val) {
		case DOSBOX_ID_CMD_RESET_LATCH:
			dosbox_int_register_shf = 0;
			dosbox_int_regsel_shf = 0;
			break;
		case DOSBOX_ID_CMD_FLUSH_WRITE:
			if (dosbox_int_register_shf != 0) {
				dosbox_integration_trigger_write();
				dosbox_int_register_shf = 0;
			}
			break;
		case DOSBOX_ID_CMD_PUSH_STATE:
			if (dosbox_int_push_save_state()) {
				dosbox_int_register_shf = 0;
				dosbox_int_regsel_shf = 0;
				dosbox_int_error = false;
				dosbox_int_busy = false;
				dosbox_int_regsel = DOSBOX_ID_RESET_INDEX_CODE;
				dosbox_int_register = DOSBOX_ID_RESET_DATA_CODE;
				LOG(LOG_MISC,LOG_DEBUG)("DOSBOX-X IG state saved");
			}
			else {
				LOG(LOG_MISC,LOG_DEBUG)("DOSBOX-X IG unable to push state, stack overflow");
				dosbox_int_error = true;
			}
			break;
		case DOSBOX_ID_CMD_POP_STATE:
			if (dosbox_int_pop_save_state()) {
				LOG(LOG_MISC,LOG_DEBUG)("DOSBOX-X IG state restored");
			}
			else {
				LOG(LOG_MISC,LOG_DEBUG)("DOSBOX-X IG unable to pop state, stack underflow");
				dosbox_int_error = true;
			}
			break;
		case DOSBOX_ID_CMD_DISCARD_STATE:
			if (dosbox_int_discard_save_state()) {
				LOG(LOG_MISC,LOG_DEBUG)("DOSBOX-X IG state discarded");
			}
			else {
				LOG(LOG_MISC,LOG_DEBUG)("DOSBOX-X IG unable to discard state, stack underflow");
				dosbox_int_error = true;
			}
			break;
		case DOSBOX_ID_CMD_DISCARD_ALL_STATE:
			while (dosbox_int_discard_save_state());
			break;
		case DOSBOX_ID_CMD_CLEAR_ERROR:
			dosbox_int_error = false;
			break;
		case DOSBOX_ID_CMD_RESET_INTERFACE:
			dosbox_int_busy = false;
			dosbox_int_error = false;
			dosbox_int_regsel = DOSBOX_ID_RESET_INDEX_CODE;
			dosbox_int_register = DOSBOX_ID_RESET_DATA_CODE;
			break;
		default:
			dosbox_int_error = true;
			break;
	}
}

static IO_ReadHandler* const dosbox_integration_cb_ports_r[4] = {
	dosbox_integration_port00_index_r,
	dosbox_integration_port01_data_r,
	dosbox_integration_port02_status_r,
	NULL
};

static IO_ReadHandler* dosbox_integration_cb_port_r(IO_CalloutObject &co,Bitu port,Bitu iolen) {
	(void)co;
	(void)iolen;
	return dosbox_integration_cb_ports_r[port&3];
}

static IO_WriteHandler* const dosbox_integration_cb_ports_w[4] = {
	dosbox_integration_port00_index_w,
	dosbox_integration_port01_data_w,
	dosbox_integration_port02_command_w,
	NULL
};

static IO_WriteHandler* dosbox_integration_cb_port_w(IO_CalloutObject &co,Bitu port,Bitu iolen) {
	(void)co;
	(void)iolen;
	return dosbox_integration_cb_ports_w[port&3];
}

/* if mem_systems 0 then size_extended is reported as the real size else 
 * zero is reported. ems and xms can increase or decrease the other_memsystems
 * counter using the BIOS_ZeroExtendedSize call */
static uint16_t size_extended;
static unsigned int ISA_PNP_WPORT = 0x20B;
static unsigned int ISA_PNP_WPORT_BIOS = 0;
static IO_ReadHandleObject *ISAPNP_PNP_READ_PORT = NULL;        /* 0x200-0x3FF range */
static IO_WriteHandleObject *ISAPNP_PNP_ADDRESS_PORT = NULL;        /* 0x279 */
static IO_WriteHandleObject *ISAPNP_PNP_DATA_PORT = NULL;       /* 0xA79 */
static IO_WriteHandleObject *BOCHS_PORT_E9 = NULL;
//static unsigned char ISA_PNP_CUR_CSN = 0;
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

static RealPt INT15_apm_pmentry=0;
static unsigned char ISA_PNP_KEYMATCH=0;
static Bits other_memsystems=0;
void CMOS_SetRegister(Bitu regNr, uint8_t val); //For setting equipment word
bool enable_integration_device_pnp=false;
bool enable_integration_device=false;
bool ISAPNPBIOS=false;
bool ISAPNPPORT=false;
bool APMBIOS=false;
bool APMBIOS_pnp=false;
bool APMBIOS_allow_realmode=false;
bool APMBIOS_allow_prot16=false;
bool APMBIOS_allow_prot32=false;
int APMBIOS_connect_mode=0;

enum {
    APMBIOS_CONNECT_REAL=0,
    APMBIOS_CONNECT_PROT16,
    APMBIOS_CONNECT_PROT32
};

unsigned int APMBIOS_connected_already_err() {
    switch (APMBIOS_connect_mode) {
        case APMBIOS_CONNECT_REAL:  return 0x02;
        case APMBIOS_CONNECT_PROT16:    return 0x05;
        case APMBIOS_CONNECT_PROT32:    return 0x07;
    }

    return 0x00;
}

ISAPnPDevice::ISAPnPDevice() {
    memset(ident,0,sizeof(ident));
}

bool ISAPnPDevice::alloc(size_t sz) {
    if (sz == alloc_sz)
        return true;

    if (alloc_res == resource_data) {
        resource_data_len = 0;
        resource_data_pos = 0;
        resource_data = NULL;
    }
    if (alloc_res != NULL)
        delete[] alloc_res;

    alloc_res = NULL;
    alloc_write = 0;
    alloc_sz = 0;

    if (sz == 0)
        return true;
    if (sz > 65536)
        return false;

    alloc_res = new unsigned char[sz];
    if (alloc_res == NULL) return false;
    memset(alloc_res,0xFF,sz);
    alloc_sz = sz;
    return true;
}

ISAPnPDevice::~ISAPnPDevice() {
    ISAPnPDevice::alloc(0);
}

void ISAPnPDevice::begin_write_res() {
    if (alloc_res == NULL) return;

    resource_data_pos = 0;
    resource_data_len = 0;
    resource_data = NULL;
    alloc_write = 0;
}

void ISAPnPDevice::write_byte(const unsigned char c) {
    if (alloc_res == NULL || alloc_write >= alloc_sz) return;
    alloc_res[alloc_write++] = c;
}

void ISAPnPDevice::write_begin_SMALLTAG(const ISAPnPDevice::SmallTags stag,unsigned char len) {
    if (len >= 8 || (unsigned int)stag >= 0x10) return;
    write_byte(((unsigned char)stag << 3) + len);
}

void ISAPnPDevice::write_begin_LARGETAG(const ISAPnPDevice::LargeTags stag,unsigned int len) {
    if (len >= 4096) return;
    write_byte(0x80 + ((unsigned char)stag));
    write_byte(len & 0xFF);
    write_byte(len >> 8);
}

void ISAPnPDevice::write_Device_ID(const char c1,const char c2,const char c3,const char c4,const char c5,const char c6,const char c7) {
    write_byte((((unsigned char)c1 & 0x1FU) << 2) + (((unsigned char)c2 & 0x18U) >> 3));
    write_byte((((unsigned char)c2 & 0x07U) << 5) + ((unsigned char)c3 & 0x1FU));
    write_byte((((unsigned char)c4 & 0x0FU) << 4) + ((unsigned char)c5 & 0x0FU));
    write_byte((((unsigned char)c6 & 0x0FU) << 4) + ((unsigned char)c7 & 0x0FU));
}

void ISAPnPDevice::write_Logical_Device_ID(const char c1,const char c2,const char c3,const char c4,const char c5,const char c6,const char c7) {
    write_begin_SMALLTAG(SmallTags::LogicalDeviceID,5);
    write_Device_ID(c1,c2,c3,c4,c5,c6,c7);
    write_byte(0x00);
}

void ISAPnPDevice::write_Compatible_Device_ID(const char c1,const char c2,const char c3,const char c4,const char c5,const char c6,const char c7) {
    write_begin_SMALLTAG(SmallTags::CompatibleDeviceID,4);
    write_Device_ID(c1,c2,c3,c4,c5,c6,c7);
}

void ISAPnPDevice::write_IRQ_Format(const uint16_t IRQ_mask,const unsigned char IRQ_signal_type) {
    bool write_irq_info = (IRQ_signal_type != 0);

    write_begin_SMALLTAG(SmallTags::IRQFormat,write_irq_info?3:2);
    write_byte(IRQ_mask & 0xFF);
    write_byte(IRQ_mask >> 8);
    if (write_irq_info) write_byte(IRQ_signal_type & 0x0F);
}

void ISAPnPDevice::write_DMA_Format(const uint8_t DMA_mask,const unsigned char transfer_type_preference,const bool is_bus_master,const bool byte_mode,const bool word_mode,const unsigned char speed_supported) {
    write_begin_SMALLTAG(SmallTags::DMAFormat,2);
    write_byte(DMA_mask);
    write_byte(
        (transfer_type_preference & 0x03) |
        (is_bus_master ? 0x04 : 0x00) |
        (byte_mode ? 0x08 : 0x00) |
        (word_mode ? 0x10 : 0x00) |
        ((speed_supported & 3) << 5));
}

void ISAPnPDevice::write_IO_Port(const uint16_t min_port,const uint16_t max_port,const uint8_t count,const uint8_t alignment,const bool full16bitdecode) {
    write_begin_SMALLTAG(SmallTags::IOPortDescriptor,7);
    write_byte((full16bitdecode ? 0x01 : 0x00));
    write_byte(min_port & 0xFF);
    write_byte(min_port >> 8);
    write_byte(max_port & 0xFF);
    write_byte(max_port >> 8);
    write_byte(alignment);
    write_byte(count);
}

void ISAPnPDevice::write_Dependent_Function_Start(const ISAPnPDevice::DependentFunctionConfig cfg,const bool force) {
    bool write_cfg_byte = force || (cfg != ISAPnPDevice::DependentFunctionConfig::AcceptableDependentConfiguration);

    write_begin_SMALLTAG(SmallTags::StartDependentFunctions,write_cfg_byte ? 1 : 0);
    if (write_cfg_byte) write_byte((unsigned char)cfg);
}

void ISAPnPDevice::write_End_Dependent_Functions() {
    write_begin_SMALLTAG(SmallTags::EndDependentFunctions,0);
}

void ISAPnPDevice::write_nstring(const char *str,const size_t l) {
    (void)l;

    if (alloc_res == NULL || alloc_write >= alloc_sz) return;

    while (*str != 0 && alloc_write < alloc_sz)
        alloc_res[alloc_write++] = (unsigned char)(*str++);
}

void ISAPnPDevice::write_Identifier_String(const char *str) {
    const size_t l = strlen(str);
    if (l > 4096) return;

    write_begin_LARGETAG(LargeTags::IdentifierStringANSI,(unsigned int)l);
    if (l != 0) write_nstring(str,l);
}

void ISAPnPDevice::write_ISAPnP_version(unsigned char major,unsigned char minor,unsigned char vendor) {
    write_begin_SMALLTAG(SmallTags::PlugAndPlayVersionNumber,2);
    write_byte((major << 4) + minor);
    write_byte(vendor);
}

void ISAPnPDevice::write_END() {
    unsigned char sum = 0;
    size_t i;

    write_begin_SMALLTAG(SmallTags::EndTag,/*length*/1);

    for (i=0;i < alloc_write;i++) sum += alloc_res[i];
    write_byte((0x100 - sum) & 0xFF);
}

void ISAPnPDevice::end_write_res() {
    if (alloc_res == NULL) return;

    write_END();

    if (alloc_write >= alloc_sz) LOG(LOG_MISC,LOG_WARN)("ISA PNP generation overflow");

    resource_data_pos = 0;
    resource_data_len = alloc_sz; // the device usually has a reason for allocating the fixed size it does
    resource_data = alloc_res;
    alloc_write = 0;
}

void ISAPnPDevice::config(Bitu val) {
    (void)val;
}

void ISAPnPDevice::wakecsn(Bitu val) {
    (void)val;
    ident_bp = 0;
    ident_2nd = 0;
    resource_data_pos = 0;
    resource_ident = 0;
}

void ISAPnPDevice::select_logical_device(Bitu val) {
    (void)val;
}
    
void ISAPnPDevice::checksum_ident() {
    unsigned char checksum = 0x6a,bit;

    for (int i=0;i < 8;i++) {
        for (int j=0;j < 8;j++) {
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
    (void)addr;
    return 0x00;
}

void ISAPnPDevice::write(Bitu addr,Bitu val) {
    (void)addr;
    (void)val;
}

#define MAX_ISA_PNP_DEVICES     64
#define MAX_ISA_PNP_SYSDEVNODES     256

static ISAPnPDevice *ISA_PNP_selected = NULL;
static ISAPnPDevice *ISA_PNP_devs[MAX_ISA_PNP_DEVICES] = {NULL}; /* FIXME: free objects on shutdown */
static Bitu ISA_PNP_devnext = 0;

static const unsigned char ISAPnPIntegrationDevice_sysdev[] = {
    ISAPNP_IO_RANGE(
            0x01,                   /* decodes 16-bit ISA addr */
            0x28,0x28,              /* min-max range I/O port */
            0x04,0x04),             /* align=4 length=4 */
    ISAPNP_END
};

class ISAPnPIntegrationDevice : public ISAPnPDevice {
    public:
        ISAPnPIntegrationDevice() : ISAPnPDevice() {
            resource_ident = 0;
            resource_data = (unsigned char*)ISAPnPIntegrationDevice_sysdev;
            resource_data_len = sizeof(ISAPnPIntegrationDevice_sysdev);
            host_writed(ident+0,ISAPNP_ID('D','O','S',0x1,0x2,0x3,0x4)); /* DOS1234 test device */
            host_writed(ident+4,0xFFFFFFFFUL);
            checksum_ident();
        }
};

ISAPnPIntegrationDevice *isapnpigdevice = NULL;

class ISAPNP_SysDevNode {
public:
    ISAPNP_SysDevNode(const unsigned char *ir,size_t len,bool already_alloc=false) {
        if (already_alloc) {
            raw = (unsigned char*)ir;
            raw_len = len;
            own = false;
        }
        else {
            if (len > 65535) E_Exit("ISAPNP_SysDevNode data too long");
            raw = new unsigned char[len+1u];
            if (ir == NULL)
                E_Exit("ISAPNP_SysDevNode cannot allocate buffer");
            else
                memcpy(raw, ir, len);
            raw_len = len;
            raw[len] = 0;
            own = true;
        }
    }
    virtual ~ISAPNP_SysDevNode() {
        if (own) delete[] raw;
    }

    unsigned char*      raw;
    size_t              raw_len;
    bool                own;
};

static ISAPNP_SysDevNode*   ISAPNP_SysDevNodes[MAX_ISA_PNP_SYSDEVNODES] = {NULL};
static Bitu         ISAPNP_SysDevNodeLargest=0;
static Bitu         ISAPNP_SysDevNodeCount=0;

void ISA_PNP_FreeAllSysNodeDevs() {
    for (Bitu i=0;i < MAX_ISA_PNP_SYSDEVNODES;i++) {
        if (ISAPNP_SysDevNodes[i] != NULL) delete ISAPNP_SysDevNodes[i];
        ISAPNP_SysDevNodes[i] = NULL;
    }

    ISAPNP_SysDevNodeLargest=0;
    ISAPNP_SysDevNodeCount=0;
}

void ISA_PNP_FreeAllDevs() {
    Bitu i;

    for (i=0;i < MAX_ISA_PNP_DEVICES;i++) {
        if (ISA_PNP_devs[i] != NULL) {
            delete ISA_PNP_devs[i];
            ISA_PNP_devs[i] = NULL;
        }
    }
    for (i=0;i < MAX_ISA_PNP_SYSDEVNODES;i++) {
        if (ISAPNP_SysDevNodes[i] != NULL) delete ISAPNP_SysDevNodes[i];
        ISAPNP_SysDevNodes[i] = NULL;
    }

    ISAPNP_SysDevNodeLargest=0;
    ISAPNP_SysDevNodeCount=0;
}

void ISA_PNP_devreg(ISAPnPDevice *x) {
    if (ISA_PNP_devnext < MAX_ISA_PNP_DEVICES) {
        if (ISA_PNP_WPORT_BIOS == 0 && ISAPNPPORT) ISA_PNP_WPORT_BIOS = ISA_PNP_WPORT;
        ISA_PNP_devs[ISA_PNP_devnext++] = x;
        x->CSN = ISA_PNP_devnext;
    }
}

static Bitu isapnp_read_port(Bitu port,Bitu /*iolen*/) {
    (void)port;//UNUSED
    Bitu ret=0xff;

    switch (ISA_PNP_CUR_ADDR) {
        case 0x01:  /* serial isolation */
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
        case 0x04:  /* read resource data */
               if (ISA_PNP_selected) {
                   if (ISA_PNP_selected->resource_ident < 9)
                       ret = ISA_PNP_selected->ident[ISA_PNP_selected->resource_ident++];              
                   else {
                       /* real-world hardware testing shows that devices act as if there was some fixed block of ROM,
                        * that repeats every 128, 256, 512, or 1024 bytes if you just blindly read from this port. */
                       if (ISA_PNP_selected->resource_data_pos < ISA_PNP_selected->resource_data_len)
                           ret = ISA_PNP_selected->resource_data[ISA_PNP_selected->resource_data_pos++];

                       /* that means that if you read enough bytes the ROM loops back to returning the ident */
                       if (ISA_PNP_selected->resource_data_pos >= ISA_PNP_selected->resource_data_len) {
                           ISA_PNP_selected->resource_data_pos = 0;
                           ISA_PNP_selected->resource_ident = 0;
                       }
                   }
               }
               break;
        case 0x05:  /* read resource status */
               if (ISA_PNP_selected) {
                   /* real-world hardware testing shows that devices act as if there was some fixed block of ROM,
                    * that repeats every 128, 256, 512, or 1024 bytes if you just blindly read from this port.
                    * therefore, there's always a byte to return. */
                   ret = 0x01;  /* TODO: simulate hardware slowness */
               }
               break;
        case 0x06:  /* card select number */
               if (ISA_PNP_selected) ret = ISA_PNP_selected->CSN;
               break;
        case 0x07:  /* logical device number */
               if (ISA_PNP_selected) ret = ISA_PNP_selected->logical_device;
               break;
        default:    /* pass the rest down to the class */
               if (ISA_PNP_selected) ret = ISA_PNP_selected->read(ISA_PNP_CUR_ADDR);
               break;
    }

//  if (1) LOG_MSG("PnP read(%02X) = %02X\n",ISA_PNP_CUR_ADDR,ret);
    return ret;
}

void isapnp_write_port(Bitu port,Bitu val,Bitu /*iolen*/) {
    Bitu i;

    if (port == 0x279) {
//      if (1) LOG_MSG("PnP addr(%02X)\n",val);
        if (val == isa_pnp_init_keystring[ISA_PNP_KEYMATCH]) {
            if (++ISA_PNP_KEYMATCH == 32) {
//              LOG_MSG("ISA PnP key -> going to sleep\n");
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
//      if (1) LOG_MSG("PnP write(%02X) = %02X\n",ISA_PNP_CUR_ADDR,val);
        switch (ISA_PNP_CUR_ADDR) {
            case 0x00: {    /* RD_DATA */
                unsigned int np = ((val & 0xFF) << 2) | 3;
                if (np != ISA_PNP_WPORT) {
                    if (ISAPNP_PNP_READ_PORT != NULL) {
                        ISAPNP_PNP_READ_PORT = NULL;
                        delete ISAPNP_PNP_READ_PORT;
                    }

                    if (np >= 0x200 && np <= 0x3FF) { /* allowable port I/O range according to spec */
                        LOG_MSG("PNP OS changed I/O read port to 0x%03X (from 0x%03X)\n",np,ISA_PNP_WPORT);

                        ISA_PNP_WPORT = np;
                        ISAPNP_PNP_READ_PORT = new IO_ReadHandleObject;
                        ISAPNP_PNP_READ_PORT->Install(ISA_PNP_WPORT,isapnp_read_port,IO_MB);
                    }
                    else {
                        LOG_MSG("PNP OS I/O read port disabled\n");

                        ISA_PNP_WPORT = 0;
                    }

                    if (ISA_PNP_selected != NULL) {
                        ISA_PNP_selected->ident_bp = 0;
                        ISA_PNP_selected->ident_2nd = 0;
                        ISA_PNP_selected->resource_data_pos = 0;
                    }
                }
            } break;
            case 0x02:  /* config control */
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
            case 0x03: {    /* wake[CSN] */
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
            case 0x06:  /* card select number */
                if (ISA_PNP_selected) ISA_PNP_selected->CSN = val;
                break;
            case 0x07:  /* logical device number */
                if (ISA_PNP_selected) ISA_PNP_selected->select_logical_device(val);
                break;
            default:    /* pass the rest down to the class */
                if (ISA_PNP_selected) ISA_PNP_selected->write(ISA_PNP_CUR_ADDR,val);
                break;
        }
    }
}

// IBM PC/AT CTRL+BREAK interrupt, called by IRQ1 handler.
// Not applicable to PC-98 mode, of course.
Bitu INT1B_Break_Handler(void) {
    // BIOS DATA AREA 40:71 bit 7 is set when Break key is pressed.
    // This is already implemented by IRQ1 handler in src/ints/bios_keyboard.cpp.
    // Ref: [http://hackipedia.org/browse.cgi/Computer/Platform/PC%2c%20IBM%20compatible/Computers/IBM/PS%e2%88%952/IBM%20Personal%20System%e2%88%952%20and%20Personal%20Computer%20BIOS%20Interface%20Technical%20Reference%20%281991%2d09%29%20First%20Edition%2c%20part%203%2epdf]
    return CBRET_NONE;
}


static Bitu INT15_Handler(void);

// FIXME: This initializes both APM BIOS and ISA PNP emulation!
//        Need to separate APM BIOS init from ISA PNP init from ISA PNP BIOS init!
//        It might also be appropriate to move this into the BIOS init sequence.
void ISAPNP_Cfg_Reset(Section *sec) {
    (void)sec;//UNUSED
    const Section_prop* section = static_cast<Section_prop*>(control->GetSection("cpu"));

    LOG(LOG_MISC,LOG_DEBUG)("Initializing ISA PnP emulation");

    enable_integration_device = section->Get_bool("integration device");
    enable_integration_device_pnp = section->Get_bool("integration device pnp");
    ISAPNPBIOS = section->Get_bool("isapnpbios");
    {
	    /* ISAPNPPORT = off|auto|on */
	    const char *s = section->Get_string("isapnpport");

	    if (!strcmp(s,"true") || !strcmp(s,"1"))
		    ISAPNPPORT = true;
	    else if (!strcmp(s,"false") || !strcmp(s,"0"))
		    ISAPNPPORT = false;
	    else /* auto */
		    ISAPNPPORT = ISAPNPBIOS; /* if the PnP BIOS is enabled, then so is the port */
    }
    APMBIOS = section->Get_bool("apmbios");
    APMBIOS_pnp = section->Get_bool("apmbios pnp");
    APMBIOS_allow_realmode = section->Get_bool("apmbios allow realmode");
    APMBIOS_allow_prot16 = section->Get_bool("apmbios allow 16-bit protected mode");
    APMBIOS_allow_prot32 = section->Get_bool("apmbios allow 32-bit protected mode");

    std::string apmbiosver = section->Get_string("apmbios version");

    /* PC-98 does not have the IBM PC/AT APM BIOS interface */
    if (IS_PC98_ARCH) {
        APMBIOS = false;
        APMBIOS_pnp = false;
    }

    if (apmbiosver == "1.0")
        APM_BIOS_minor_version = 0;
    else if (apmbiosver == "1.1")
        APM_BIOS_minor_version = 1;
    else if (apmbiosver == "1.2")
        APM_BIOS_minor_version = 2;
    else//auto
        APM_BIOS_minor_version = 2;

    /* PC-98 does not have APM.
     * I *think* it has Plug & Play, but probably different from ISA PnP and specific to the C-Bus interface,
     * which I have no information on at this time --J.C. */
    if (IS_PC98_ARCH)
        return;

    LOG(LOG_MISC,LOG_DEBUG)("APM BIOS allow: real=%u pm16=%u pm32=%u version=1.%u",
        APMBIOS_allow_realmode,
        APMBIOS_allow_prot16,
        APMBIOS_allow_prot32,
        APM_BIOS_minor_version);

    std::string apmbiospwrbtn = section->Get_string("apm power button event");
    if (apmbiospwrbtn == "standby")
        APM_PowerButtonSendsSuspend = false;
    else
        APM_PowerButtonSendsSuspend = true;


    if (APMBIOS && (APMBIOS_allow_prot16 || APMBIOS_allow_prot32) && INT15_apm_pmentry == 0) {
        Bitu cb,base;

        /* NTS: This is... kind of a terrible hack. It basically tricks Windows into executing our
         *      INT 15h handler as if the APM entry point. Except that instead of an actual INT 15h
         *      triggering the callback, a FAR CALL triggers the callback instead (CB_RETF not CB_IRET). */
        /* TODO: We should really consider moving the APM BIOS code in INT15_Handler() out into its
         *       own function, then having the INT15_Handler() call it as well as directing this callback
         *       directly to it. If you think about it, this hack also lets the "APM entry point" invoke
         *       other arbitrary INT 15h calls which is not valid. */

        cb = CALLBACK_Allocate();
        INT15_apm_pmentry = CALLBACK_RealPointer(cb);
        LOG_MSG("Allocated APM BIOS pm entry point at %04x:%04x\n",INT15_apm_pmentry>>16,INT15_apm_pmentry&0xFFFF);
        CALLBACK_Setup(cb,INT15_Handler,CB_RETF,"APM BIOS protected mode entry point");

        /* NTS: Actually INT15_Handler is written to act like an interrupt (IRETF) type callback.
         *      Prior versions hacked this into something that responds by CB_RETF, however some
         *      poking around reveals that CALLBACK_SCF and friends still assume an interrupt
         *      stack, thus, the cause of random crashes in Windows was simply that we were
         *      flipping flag bits in the middle of the return address on the stack. The other
         *      source of random crashes is that the CF/ZF manipulation in INT 15h wasn't making
         *      its way back to Windows, meaning that when APM BIOS emulation intended to return
         *      an error (by setting CF), Windows didn't get the memo (CF wasn't set on return)
         *      and acted as if the call succeeded, or worse, CF happened to be set on entry and
         *      was never cleared by APM BIOS emulation.
         *
         *      So what we need is:
         *
         *      PUSHF           ; put flags in right place
         *      PUSH    BP      ; dummy FAR pointer
         *      PUSH    BP      ; again
         *      <callback>
         *      POP     BP      ; drop it
         *      POP     BP      ; drop it
         *      POPF
         *      RETF
         *
         *      Then CALLBACK_SCF can work normally this way.
         *
         * NTS: We *still* need to separate APM BIOS calls from the general INT 15H emulation though... */
        base = Real2Phys(INT15_apm_pmentry);
        LOG_MSG("Writing code to %05x\n",(unsigned int)base);

        phys_writeb(base+0x00,0x9C);                             /* pushf */
        phys_writeb(base+0x01,0x55);                             /* push (e)bp */
        phys_writeb(base+0x02,0x55);                             /* push (e)bp */

        phys_writeb(base+0x03,(uint8_t)0xFE);                     //GRP 4
        phys_writeb(base+0x04,(uint8_t)0x38);                     //Extra Callback instruction
        phys_writew(base+0x05,(uint16_t)cb);                      //The immediate word

        phys_writeb(base+0x07,0x5D);                             /* pop (e)bp */
        phys_writeb(base+0x08,0x5D);                             /* pop (e)bp */
        phys_writeb(base+0x09,0x9D);                             /* popf */
        phys_writeb(base+0x0A,0xCB);                             /* retf */

        /* APM suspended mode execution loop */
        cb = CALLBACK_Allocate();
        APM_SuspendedLoopRptr = CALLBACK_RealPointer(cb);
        CALLBACK_Setup(cb,APM_SuspendedLoopFunc,CB_RETF,"APM BIOS suspend/standby loop");

        base = Real2Phys(APM_SuspendedLoopRptr);
        LOG_MSG("Writing code to %05x\n",(unsigned int)base);
        phys_writeb(base+0x04,0xF4);                             /* hlt */
        phys_writew(base+0x05,0xF9EB);                           /* jmp $-5 (EB xx) */
        phys_writeb(base+0x07,0xC3);                             /* ret */
    }
}

void ISAPNP_Cfg_Init() {
    AddVMEventFunction(VM_EVENT_RESET,AddVMEventFunctionFuncPair(ISAPNP_Cfg_Reset));
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

static const unsigned char ISAPNP_sysdev_Keyboard[] = {
    ISAPNP_SYSDEV_HEADER(
            ISAPNP_ID('P','N','P',0x0,0x3,0x0,0x3), /* PNP0303 IBM Enhanced 101/102 key with PS/2 */
            ISAPNP_TYPE(0x09,0x00,0x00),        /* type: input, keyboard */
            0x0001 | 0x0002),           /* can't disable, can't configure */
    /*----------allocated--------*/
    ISAPNP_IO_RANGE(
            0x01,                   /* decodes 16-bit ISA addr */
            0x60,0x60,              /* min-max range I/O port */
            0x01,0x01),             /* align=1 length=1 */
    ISAPNP_IO_RANGE(
            0x01,                   /* decodes 16-bit ISA addr */
            0x64,0x64,              /* min-max range I/O port */
            0x01,0x01),             /* align=1 length=1 */
    ISAPNP_IRQ_SINGLE(
            1,                  /* IRQ 1 */
            0x09),                  /* HTE=1 LTL=1 */
    ISAPNP_END,
    /*----------possible--------*/
    ISAPNP_END,
    /*----------compatible--------*/
    ISAPNP_END
};

static const unsigned char ISAPNP_sysdev_Mouse[] = {
    ISAPNP_SYSDEV_HEADER(
            ISAPNP_ID('P','N','P',0x0,0xF,0x0,0xE), /* PNP0F0E Microsoft compatible PS/2 */
            ISAPNP_TYPE(0x09,0x02,0x00),        /* type: input, keyboard */
            0x0001 | 0x0002),           /* can't disable, can't configure */
    /*----------allocated--------*/
    ISAPNP_IRQ_SINGLE(
            12,                 /* IRQ 12 */
            0x09),                  /* HTE=1 LTL=1 */
    ISAPNP_END,
    /*----------possible--------*/
    ISAPNP_END,
    /*----------compatible--------*/
    ISAPNP_END
};

static const unsigned char ISAPNP_sysdev_DMA_Controller[] = {
    ISAPNP_SYSDEV_HEADER(
            ISAPNP_ID('P','N','P',0x0,0x2,0x0,0x0), /* PNP0200 AT DMA controller */
            ISAPNP_TYPE(0x08,0x01,0x00),        /* type: input, keyboard */
            0x0001 | 0x0002),           /* can't disable, can't configure */
    /*----------allocated--------*/
    ISAPNP_IO_RANGE(
            0x01,                   /* decodes 16-bit ISA addr */
            0x00,0x00,              /* min-max range I/O port (DMA channels 0-3) */
            0x10,0x10),             /* align=16 length=16 */
    ISAPNP_IO_RANGE(
            0x01,                   /* decodes 16-bit ISA addr */
            0x81,0x81,              /* min-max range I/O port (DMA page registers) */
            0x01,0x0F),             /* align=1 length=15 */
    ISAPNP_IO_RANGE(
            0x01,                   /* decodes 16-bit ISA addr */
            0xC0,0xC0,              /* min-max range I/O port (AT DMA channels 4-7) */
            0x20,0x20),             /* align=32 length=32 */
    ISAPNP_DMA_SINGLE(
            4,                  /* DMA 4 */
            0x01),                  /* 8/16-bit transfers, compatible speed */
    ISAPNP_END,
    /*----------possible--------*/
    ISAPNP_END,
    /*----------compatible--------*/
    ISAPNP_END
};

static const unsigned char ISAPNP_sysdev_PIC[] = {
    ISAPNP_SYSDEV_HEADER(
            ISAPNP_ID('P','N','P',0x0,0x0,0x0,0x0), /* PNP0000 Interrupt controller */
            ISAPNP_TYPE(0x08,0x00,0x01),        /* type: ISA interrupt controller */
            0x0001 | 0x0002),           /* can't disable, can't configure */
    /*----------allocated--------*/
    ISAPNP_IO_RANGE(
            0x01,                   /* decodes 16-bit ISA addr */
            0x20,0x20,              /* min-max range I/O port */
            0x01,0x02),             /* align=1 length=2 */
    ISAPNP_IO_RANGE(
            0x01,                   /* decodes 16-bit ISA addr */
            0xA0,0xA0,              /* min-max range I/O port */
            0x01,0x02),             /* align=1 length=2 */
    ISAPNP_IRQ_SINGLE(
            2,                  /* IRQ 2 */
            0x09),                  /* HTE=1 LTL=1 */
    ISAPNP_END,
    /*----------possible--------*/
    ISAPNP_END,
    /*----------compatible--------*/
    ISAPNP_END
};

static const unsigned char ISAPNP_sysdev_Timer[] = {
    ISAPNP_SYSDEV_HEADER(
            ISAPNP_ID('P','N','P',0x0,0x1,0x0,0x0), /* PNP0100 Timer */
            ISAPNP_TYPE(0x08,0x02,0x01),        /* type: ISA timer */
            0x0001 | 0x0002),           /* can't disable, can't configure */
    /*----------allocated--------*/
    ISAPNP_IO_RANGE(
            0x01,                   /* decodes 16-bit ISA addr */
            0x40,0x40,              /* min-max range I/O port */
            0x04,0x04),             /* align=4 length=4 */
    ISAPNP_IRQ_SINGLE(
            0,                  /* IRQ 0 */
            0x09),                  /* HTE=1 LTL=1 */
    ISAPNP_END,
    /*----------possible--------*/
    ISAPNP_END,
    /*----------compatible--------*/
    ISAPNP_END
};

static const unsigned char ISAPNP_sysdev_RTC[] = {
    ISAPNP_SYSDEV_HEADER(
            ISAPNP_ID('P','N','P',0x0,0xB,0x0,0x0), /* PNP0B00 Real-time clock */
            ISAPNP_TYPE(0x08,0x03,0x01),        /* type: ISA real-time clock */
            0x0001 | 0x0002),           /* can't disable, can't configure */
    /*----------allocated--------*/
    ISAPNP_IO_RANGE(
            0x01,                   /* decodes 16-bit ISA addr */
            0x70,0x70,              /* min-max range I/O port */
            0x01,0x02),             /* align=1 length=2 */
    ISAPNP_IRQ_SINGLE(
            8,                  /* IRQ 8 */
            0x09),                  /* HTE=1 LTL=1 */
    ISAPNP_END,
    /*----------possible--------*/
    ISAPNP_END,
    /*----------compatible--------*/
    ISAPNP_END
};

static const unsigned char ISAPNP_sysdev_PC_Speaker[] = {
    ISAPNP_SYSDEV_HEADER(
            ISAPNP_ID('P','N','P',0x0,0x8,0x0,0x0), /* PNP0800 PC speaker */
            ISAPNP_TYPE(0x04,0x01,0x00),        /* type: PC speaker */
            0x0001 | 0x0002),           /* can't disable, can't configure */
    /*----------allocated--------*/
    ISAPNP_IO_RANGE(
            0x01,                   /* decodes 16-bit ISA addr */
            0x61,0x61,              /* min-max range I/O port */
            0x01,0x01),             /* align=1 length=1 */
    ISAPNP_END,
    /*----------possible--------*/
    ISAPNP_END,
    /*----------compatible--------*/
    ISAPNP_END
};

static const unsigned char ISAPNP_sysdev_Numeric_Coprocessor[] = {
    ISAPNP_SYSDEV_HEADER(
            ISAPNP_ID('P','N','P',0x0,0xC,0x0,0x4), /* PNP0C04 Numeric Coprocessor */
            ISAPNP_TYPE(0x0B,0x80,0x00),        /* type: FPU */
            0x0001 | 0x0002),           /* can't disable, can't configure */
    /*----------allocated--------*/
    ISAPNP_IO_RANGE(
            0x01,                   /* decodes 16-bit ISA addr */
            0xF0,0xF0,              /* min-max range I/O port */
            0x10,0x10),             /* align=16 length=16 */
    ISAPNP_IRQ_SINGLE(
            13,                 /* IRQ 13 */
            0x09),                  /* HTE=1 LTL=1 */
    ISAPNP_END,
    /*----------possible--------*/
    ISAPNP_END,
    /*----------compatible--------*/
    ISAPNP_END
};

static const unsigned char ISAPNP_sysdev_System_Board[] = {
    ISAPNP_SYSDEV_HEADER(
            ISAPNP_ID('P','N','P',0x0,0xC,0x0,0x1), /* PNP0C01 System board */
            ISAPNP_TYPE(0x08,0x80,0x00),        /* type: System peripheral, Other */
            0x0001 | 0x0002),           /* can't disable, can't configure */
    /*----------allocated--------*/
    ISAPNP_IO_RANGE(
            0x01,                   /* decodes 16-bit ISA addr */
            0x24,0x24,              /* min-max range I/O port */
            0x04,0x04),             /* align=4 length=4 */
    ISAPNP_END,
    /*----------possible--------*/
    ISAPNP_END,
    /*----------compatible--------*/
    ISAPNP_END
};

/* NTS: If some of my late 1990's laptops are any indication, this resource list can be used
 *      as a hint that the motherboard supports Intel EISA/PCI controller DMA registers that
 *      allow ISA DMA to extend to 32-bit addresses instead of being limited to 24-bit */
static const unsigned char ISAPNP_sysdev_General_ISAPNP[] = {
    ISAPNP_SYSDEV_HEADER(
            ISAPNP_ID('P','N','P',0x0,0xC,0x0,0x2), /* PNP0C02 General ID for reserving resources */
            ISAPNP_TYPE(0x08,0x80,0x00),        /* type: System peripheral, Other */
            0x0001 | 0x0002),           /* can't disable, can't configure */
    /*----------allocated--------*/
    ISAPNP_IO_RANGE(
            0x01,                   /* decodes 16-bit ISA addr */
            0x208,0x208,                /* min-max range I/O port */
            0x04,0x04),             /* align=4 length=4 */
    ISAPNP_END,
    /*----------possible--------*/
    ISAPNP_END,
    /*----------compatible--------*/
    ISAPNP_END
};

/* PnP system entry to tell Windows 95 the obvious: That there's an ISA bus present */
/* NTS: Examination of some old laptops of mine shows that these devices do not list any resources,
 *      or at least, an old Toshiba of mine lists the PCI registers 0xCF8-0xCFF as motherboard resources
 *      and defines no resources for the PCI Bus PnP device. */
static const unsigned char ISAPNP_sysdev_ISA_BUS[] = {
    ISAPNP_SYSDEV_HEADER(
            ISAPNP_ID('P','N','P',0x0,0xA,0x0,0x0), /* PNP0A00 ISA Bus */
            ISAPNP_TYPE(0x06,0x04,0x00),        /* type: System device, peripheral bus */
            0x0001 | 0x0002),           /* can't disable, can't configure */
    /*----------allocated--------*/
    ISAPNP_END,
    /*----------possible--------*/
    ISAPNP_END,
    /*----------compatible--------*/
    ISAPNP_END
};

/* PnP system entry to tell Windows 95 the obvious: That there's a PCI bus present */
static const unsigned char ISAPNP_sysdev_PCI_BUS[] = {
    ISAPNP_SYSDEV_HEADER(
            ISAPNP_ID('P','N','P',0x0,0xA,0x0,0x3), /* PNP0A03 PCI Bus */
            ISAPNP_TYPE(0x06,0x04,0x00),        /* type: System device, peripheral bus */
            0x0001 | 0x0002),           /* can't disable, can't configure */
    /*----------allocated--------*/
    ISAPNP_END,
    /*----------possible--------*/
    ISAPNP_END,
    /*----------compatible--------*/
    ISAPNP_END
};

/* to help convince Windows 95 that the APM BIOS is present */
static const unsigned char ISAPNP_sysdev_APM_BIOS[] = {
    ISAPNP_SYSDEV_HEADER(
            ISAPNP_ID('P','N','P',0x0,0xC,0x0,0x5), /* PNP0C05 APM BIOS */
            ISAPNP_TYPE(0x08,0x80,0x00),        /* type: FIXME is this right?? I can't find any examples or documentation */
            0x0001 | 0x0002),           /* can't disable, can't configure */
    /*----------allocated--------*/
    ISAPNP_END,
    /*----------possible--------*/
    ISAPNP_END,
    /*----------compatible--------*/
    ISAPNP_END
};

bool ISAPNP_RegisterSysDev(const unsigned char *raw,Bitu len,bool already) {
    if (ISAPNP_SysDevNodeCount >= MAX_ISA_PNP_SYSDEVNODES)
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
     * This might also explain why my early experiments with Bochs always had the perpetual
     * APM BIOS that never worked but was always detected.
     *
     * ------------------------------------------------------------------------
     * Windows 95 OSR2:
     *
     * Windows 95 OSR2 however uses a 16-bit stack (where the stack segment is based somewhere
     * around 0xC1xxxxxx), all we have to do to correctly access it is work through the page tables.
     * This is within spec, but now Microsoft sends us a data segment that is based at virtual address
     * 0xC2xxxxxx, which is why I had to disable the "verify selector" routine */
    arg = SegPhys(ss) + (reg_esp&cpu.stack.mask) + (2*2); /* entry point (real and protected) is 16-bit, expected to RETF (skip CS:IP) */

    if (protmode != ISAPNP_CPU_ProtMode()) {
        //LOG_MSG("ISA PnP %s entry point called from %s. On real BIOSes this would CRASH\n",protmode ? "Protected mode" : "Real mode",
        //  ISAPNP_CPU_ProtMode() ? "Protected mode" : "Real mode");
        reg_ax = 0x84;/* BAD_PARAMETER */
        return 0;
    }

    func = mem_readw(arg);
//  LOG_MSG("PnP prot=%u DS=%04x (base=0x%08lx) SS:ESP=%04x:%04x (base=0x%08lx phys=0x%08lx) function=0x%04x\n",
//      (unsigned int)protmode,(unsigned int)SegValue(ds),(unsigned long)SegPhys(ds),
//      (unsigned int)SegValue(ss),(unsigned int)reg_esp,(unsigned long)SegPhys(ss),
//      (unsigned long)arg,(unsigned int)func);

    /* every function takes the form
     *
     * int __cdecl FAR (*entrypoint)(int Function...);
     *
     * so the first argument on the stack is an int that we read to determine what the caller is asking
     *
     * Don't forget in the real-mode world:
     *    sizeof(int) == 16 bits
     *    sizeof(long) == 32 bits
     */    
    switch (func) {
        case 0: {       /* Get Number of System Nodes */
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
        case 1: {       /* Get System Device Node */
            /* int __cdecl FAR (*entrypoint)(int Function,unsigned char FAR *Node,struct DEV_NODE FAR *devNodeBuffer,unsigned int Control,unsigned int BiosSelector);
             *                               ^ +0         ^ +2                    ^ +6                               ^ +10                ^ +12                       = 14 */
            Bitu Node_ptr = mem_readd(arg+2);
            Bitu devNodeBuffer_ptr = mem_readd(arg+6);
            Bitu Control = mem_readw(arg+10);
            BiosSelector = mem_readw(arg+12);
            unsigned char Node;
            Bitu i=0;

            if (!ISAPNP_Verify_BiosSelector(BiosSelector))
                goto badBiosSelector;

            /* control bits 0-1 must be '01' or '10' but not '00' or '11' */
            if (Control == 0 || (Control&3) == 3) {
                LOG_MSG("ISAPNP Get System Device Node: Invalid Control value 0x%04x\n",(int)Control);
                reg_ax = 0x84;/* BAD_PARAMETER */
                break;
            }

            devNodeBuffer_ptr = ISAPNP_xlate_address(devNodeBuffer_ptr);
            Node_ptr = ISAPNP_xlate_address(Node_ptr);
            Node = mem_readb(Node_ptr);
            if (Node >= ISAPNP_SysDevNodeCount) {
                LOG_MSG("ISAPNP Get System Device Node: Invalid Node 0x%02x (max 0x%04x)\n",(int)Node,(int)ISAPNP_SysDevNodeCount);
                reg_ax = 0x84;/* BAD_PARAMETER */
                break;
            }

            const ISAPNP_SysDevNode *nd = ISAPNP_SysDevNodes[Node];

            mem_writew(devNodeBuffer_ptr+0,(uint16_t)(nd->raw_len+3)); /* Length */
            mem_writeb(devNodeBuffer_ptr+2,Node); /* on most PnP BIOS implementations I've seen "handle" is set to the same value as Node */
            for (i=0;i < (Bitu)nd->raw_len;i++)
                mem_writeb(devNodeBuffer_ptr+i+3,nd->raw[i]);

//          LOG_MSG("ISAPNP OS asked for Node 0x%02x\n",Node);

            if (++Node >= ISAPNP_SysDevNodeCount) Node = 0xFF; /* no more nodes */
            mem_writeb(Node_ptr,Node);

            reg_ax = 0x00;/* SUCCESS */
        } break;
        case 4: {       /* Send Message */
            /* int __cdecl FAR (*entrypoint)(int Function,unsigned int Message,unsigned int BiosSelector);
             *                               ^ +0         ^ +2                 ^ +4                        = 6 */
            Bitu Message = mem_readw(arg+2);
            BiosSelector = mem_readw(arg+4);

            if (!ISAPNP_Verify_BiosSelector(BiosSelector))
                goto badBiosSelector;

            switch (Message) {
                case 0x41:  /* POWER_OFF */
                    LOG_MSG("Plug & Play OS requested power off.\n");
                    reg_ax = 0;
                    throw 1;    /* NTS: Based on the Reboot handler code, causes DOSBox-X to cleanly shutdown and exit */
                    break;
                case 0x42:  /* PNP_OS_ACTIVE */
                    LOG_MSG("Plug & Play OS reports itself active\n");
                    reg_ax = 0;
                    break;
                case 0x43:  /* PNP_OS_INACTIVE */
                    LOG_MSG("Plug & Play OS reports itself inactive\n");
                    reg_ax = 0;
                    break;
                default:
                    LOG_MSG("Unknown ISA PnP message 0x%04x\n",(int)Message);
                    reg_ax = 0x82;/* FUNCTION_NOT_SUPPORTED */
                    break;
            }
        } break;
        case 0x40: {        /* Get PnP ISA configuration */
            /* int __cdecl FAR (*entrypoint)(int Function,unsigned char far *struct,unsigned int BiosSelector);
             *                               ^ +0         ^ +2                      ^ +6                        = 8 */
            Bitu struct_ptr = mem_readd(arg+2);
            BiosSelector = mem_readw(arg+6);

            if (!ISAPNP_Verify_BiosSelector(BiosSelector))
                goto badBiosSelector;

            /* struct isapnp_pnp_isa_cfg {
                 uint8_t    revision;
                 uint8_t    total_csn;
                 uint16_t   isa_pnp_port;
                 uint16_t   reserved;
             }; */

            if (struct_ptr != 0) {
                Bitu ph = ISAPNP_xlate_address(struct_ptr);
                mem_writeb(ph+0,0x01);      /* ->revision = 0x01 */
                mem_writeb(ph+1,ISA_PNP_devnext); /* ->total_csn */
                mem_writew(ph+2,ISA_PNP_WPORT_BIOS);    /* ->isa_pnp_port */
                mem_writew(ph+4,0);     /* ->reserved */
            }

            reg_ax = 0x00;/* SUCCESS */
        } break;
        default:
            //LOG_MSG("Unsupported ISA PnP function 0x%04x\n",func);
            reg_ax = 0x82;/* FUNCTION_NOT_SUPPORTED */
            break;
    }

    return 0;
badBiosSelector:
    /* return an error. remind the user (possible developer) how lucky he is, a real
     * BIOS implementation would CRASH when misused like this */
    LOG_MSG("ISA PnP function 0x%04x called with incorrect BiosSelector parameter 0x%04x\n",(int)func,(int)BiosSelector);
    LOG_MSG(" > STACK %04X %04X %04X %04X %04X %04X %04X %04X\n",
        mem_readw(arg),     mem_readw(arg+2),   mem_readw(arg+4),   mem_readw(arg+6),
        mem_readw(arg+8),   mem_readw(arg+10),  mem_readw(arg+12),  mem_readw(arg+14));

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
        uint32_t count=mem_readd(BIOS_WAIT_FLAG_COUNT);
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
    uint16_t port;
    uint8_t irq;
    uint8_t dma;
} tandy_sb;
static struct {
    uint16_t port;
    uint8_t irq;
    uint8_t dma;
} tandy_dac;

static bool Tandy_InitializeSB() {
    /* see if soundblaster module available and at what port/IRQ/DMA */
    Bitu sbport, sbirq, sbdma;
    if (SB_Get_Address(sbport, sbirq, sbdma)) {
        tandy_sb.port=(uint16_t)(sbport&0xffff);
        tandy_sb.irq =(uint8_t)(sbirq&0xff);
        tandy_sb.dma =(uint8_t)(sbdma&0xff);
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
        tandy_dac.port=(uint16_t)(tsport&0xffff);
        tandy_dac.irq =(uint8_t)(tsirq&0xff);
        tandy_dac.dma =(uint8_t)(tsdma&0xff);
        return true;
    } else {
        /* no Tandy DAC accessible */
        tandy_dac.port=0;
        return false;
    }
}

/* check if Tandy DAC is still playing */
static bool Tandy_TransferInProgress(void) {
    if (real_readw(0x40,0xd0)) return true;         /* not yet done */
    if (real_readb(0x40,0xd4)==0xff) return false;  /* still in init-state */

    uint8_t tandy_dma = 1;
    if (tandy_sb.port) tandy_dma = tandy_sb.dma;
    else if (tandy_dac.port) tandy_dma = tandy_dac.dma;

    IO_Write(0x0c,0x00);
    uint16_t datalen = (IO_ReadB(tandy_dma * 2 + 1)) + (IO_ReadB(tandy_dma * 2 + 1) << 8);
    if (datalen==0xffff) return false;  /* no DMA transfer */
    else if ((datalen<0x10) && (real_readb(0x40,0xd4)==0x0f) && (real_readw(0x40,0xd2)==0x1c)) {
        /* stop already requested */
        return false;
    }
    return true;
}

static void Tandy_SetupTransfer(PhysPt bufpt,bool isplayback) {
    Bitu length=real_readw(0x40,0xd0);
    if (length==0) return;  /* nothing to do... */

    if ((tandy_sb.port==0) && (tandy_dac.port==0)) return;

    uint8_t tandy_irq = 7;
    if (tandy_sb.port) tandy_irq = tandy_sb.irq;
    else if (tandy_dac.port) tandy_irq = tandy_dac.irq;
    uint8_t tandy_irq_vector = tandy_irq;
    if (tandy_irq_vector<8) tandy_irq_vector += 8;
    else tandy_irq_vector += (0x70-8);

    /* revector IRQ-handler if necessary */
    RealPt current_irq=RealGetVec(tandy_irq_vector);
    if (current_irq!=tandy_DAC_callback[0]->Get_RealPointer()) {
        real_writed(0x40,0xd6,current_irq);
        RealSetVec(tandy_irq_vector,tandy_DAC_callback[0]->Get_RealPointer());
    }

    uint8_t tandy_dma = 1;
    if (tandy_sb.port) tandy_dma = tandy_sb.dma;
    else if (tandy_dac.port) tandy_dma = tandy_dac.dma;

    if (tandy_sb.port) {
        IO_Write(tandy_sb.port+0xcu,0xd0);               /* stop DMA transfer */
        IO_Write(0x21,IO_Read(0x21)&(~(1u<<tandy_irq))); /* unmask IRQ */
        IO_Write(tandy_sb.port+0xcu,0xd1);               /* turn speaker on */
    } else {
        IO_Write(tandy_dac.port,IO_Read(tandy_dac.port)&0x60);  /* disable DAC */
        IO_Write(0x21,IO_Read(0x21)&(~(1u<<tandy_irq)));         /* unmask IRQ */
    }

    IO_Write(0x0a,0x04|tandy_dma);  /* mask DMA channel */
    IO_Write(0x0c,0x00);            /* clear DMA flipflop */
    if (isplayback) IO_Write(0x0b,0x48|tandy_dma);
    else IO_Write(0x0b,0x44|tandy_dma);
    /* set physical address of buffer */
    uint8_t bufpage=(uint8_t)((bufpt>>16u)&0xff);
    IO_Write(tandy_dma*2u,(uint8_t)(bufpt&0xff));
    IO_Write(tandy_dma*2u,(uint8_t)((bufpt>>8u)&0xff));
    switch (tandy_dma) {
        case 0: IO_Write(0x87,bufpage); break;
        case 1: IO_Write(0x83,bufpage); break;
        case 2: IO_Write(0x81,bufpage); break;
        case 3: IO_Write(0x82,bufpage); break;
    }
    real_writeb(0x40,0xd4,bufpage);

    /* calculate transfer size (respects segment boundaries) */
    uint32_t tlength=length;
    if (tlength+(bufpt&0xffff)>0x10000) tlength=0x10000-(bufpt&0xffff);
    real_writew(0x40,0xd0,(uint16_t)(length-tlength));    /* remaining buffer length */
    tlength--;

    /* set transfer size */
    IO_Write(tandy_dma*2u+1u,(uint8_t)(tlength&0xffu));
    IO_Write(tandy_dma*2u+1u,(uint8_t)((tlength>>8u)&0xffu));

    uint16_t delay=(uint16_t)(real_readw(0x40,0xd2)&0xfff);
    uint8_t amplitude=(uint8_t)(((unsigned int)real_readw(0x40,0xd2)>>13u)&0x7u);
    if (tandy_sb.port) {
        IO_Write(0x0a,tandy_dma);   /* enable DMA channel */
        /* set frequency */
        IO_Write(tandy_sb.port+0xcu,0x40);
        IO_Write(tandy_sb.port+0xcu,256u - delay*100u/358u);
        /* set playback type to 8bit */
        if (isplayback) IO_Write(tandy_sb.port+0xcu,0x14u);
        else IO_Write(tandy_sb.port+0xcu,0x24u);
        /* set transfer size */
        IO_Write(tandy_sb.port+0xcu,(uint8_t)(tlength&0xffu));
        IO_Write(tandy_sb.port+0xcu,(uint8_t)((tlength>>8)&0xffu));
    } else {
        if (isplayback) IO_Write(tandy_dac.port,(IO_Read(tandy_dac.port)&0x7cu) | 0x03u);
        else IO_Write(tandy_dac.port,(IO_Read(tandy_dac.port)&0x7cu) | 0x02u);
        IO_Write(tandy_dac.port+2u,(uint8_t)(delay&0xffu));
        IO_Write(tandy_dac.port+3u,(uint8_t)((((unsigned int)delay>>8u)&0xfu) | ((unsigned int)amplitude<<5u)));
        if (isplayback) IO_Write(tandy_dac.port,(IO_Read(tandy_dac.port)&0x7cu) | 0x1fu);
        else IO_Write(tandy_dac.port,(IO_Read(tandy_dac.port)&0x7c) | 0x1e);
        IO_Write(0x0a,tandy_dma);   /* enable DMA channel */
    }

    if (!isplayback) {
        /* mark transfer as recording operation */
        real_writew(0x40,0xd2,(uint16_t)(delay|0x1000));
    }
}

static Bitu IRQ_TandyDAC(void) {
    if (tandy_dac.port) {
        IO_Read(tandy_dac.port);
    }
    if (real_readw(0x40,0xd0)) {    /* play/record next buffer */
        /* acknowledge IRQ */
        IO_Write(0x20,0x20);
        if (tandy_sb.port) {
            IO_Read(tandy_sb.port+0xeu);
        }

        /* buffer starts at the next page */
        uint8_t npage=real_readb(0x40,0xd4)+1u;
        real_writeb(0x40,0xd4,npage);

        Bitu rb=real_readb(0x40,0xd3);
        if (rb&0x10) {
            /* start recording */
            real_writeb(0x40,0xd3,rb&0xefu);
            Tandy_SetupTransfer((unsigned int)npage<<16u,false);
        } else {
            /* start playback */
            Tandy_SetupTransfer((unsigned int)npage<<16u,true);
        }
    } else {    /* playing/recording is finished */
        uint8_t tandy_irq = 7u;
        if (tandy_sb.port) tandy_irq = tandy_sb.irq;
        else if (tandy_dac.port) tandy_irq = tandy_dac.irq;
        uint8_t tandy_irq_vector = tandy_irq;
        if (tandy_irq_vector<8u) tandy_irq_vector += 8u;
        else tandy_irq_vector += (0x70u-8u);

        RealSetVec(tandy_irq_vector,real_readd(0x40,0xd6));

        /* turn off speaker and acknowledge soundblaster IRQ */
        if (tandy_sb.port) {
            IO_Write(tandy_sb.port+0xcu,0xd3u);
            IO_Read(tandy_sb.port+0xeu);
        }

        /* issue BIOS tandy sound device busy callout */
        SegSet16(cs, RealSeg(tandy_DAC_callback[1]->Get_RealPointer()));
        reg_ip = RealOff(tandy_DAC_callback[1]->Get_RealPointer());
    }
    return CBRET_NONE;
}

static void TandyDAC_Handler(uint8_t tfunction) {
    if ((!tandy_sb.port) && (!tandy_dac.port)) return;
    switch (tfunction) {
    case 0x81:  /* Tandy sound system check */
        if (tandy_dac.port) {
            reg_ax=tandy_dac.port;
        } else {
            reg_ax=0xc4;
        }
        CALLBACK_SCF(Tandy_TransferInProgress());
        break;
    case 0x82:  /* Tandy sound system start recording */
    case 0x83:  /* Tandy sound system start playback */
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
    case 0x84:  /* Tandy sound system stop playing */
        reg_ah=0x00;

        /* setup for a small buffer with silence */
        real_writew(0x40,0xd0,0x0a);
        real_writew(0x40,0xd2,0x1c);
        Tandy_SetupTransfer(PhysMake(0xf000,0xa084),true);
        CALLBACK_SCF(false);
        break;
    case 0x85:  /* Tandy sound system reset */
        if (tandy_dac.port) {
            IO_Write(tandy_dac.port,(uint8_t)(IO_Read(tandy_dac.port)&0xe0));
        }
        reg_ah=0x00;
        CALLBACK_SCF(false);
        break;
    }
}

extern bool date_host_forced;
static uint8_t ReadCmosByte (Bitu index) {
    IO_Write(0x70, index);
    return IO_Read(0x71);
}

static void WriteCmosByte (Bitu index, Bitu val) {
    IO_Write(0x70, index);
    IO_Write(0x71, val);
}

static bool RtcUpdateDone () {
    while ((ReadCmosByte(0x0a) & 0x80) != 0) CALLBACK_Idle();
    return true;            // cannot fail in DOSbox
}

static void InitRtc () {
    // Change the RTC to return BCD and set the 24h bit. Clear the SET bit.
    // That's it. Do not change any other bits.
    //
    // Some games ("The Tales of Peter Rabbit") use the RTC clock periodic
    // interrupt for timing and music at rates other than 1024Hz and we must
    // not change that rate nor clear any interrupt enable bits. Do not clear
    // pending interrupts, either! The periodic interrupt does not affect reading
    // the RTC clock. The point of this function and INT 15h code calling this
    // function is to read the clock.
    WriteCmosByte(0x0b, (ReadCmosByte(0x0b) & 0x7du/*clear=SET[7]|DM[2]*/) | 0x03u/*set=24/12[1]|DSE[0]*/);
}

static Bitu INT1A_Handler(void) {
    CALLBACK_SIF(true);
    switch (reg_ah) {
    case 0x00:  /* Get System time */
        {
            uint32_t ticks=mem_readd(BIOS_TIMER);
            reg_al=mem_readb(BIOS_24_HOURS_FLAG);
            mem_writeb(BIOS_24_HOURS_FLAG,0); // reset the "flag"
            reg_cx=(uint16_t)(ticks >> 16u);
            reg_dx=(uint16_t)(ticks & 0xffff);
            break;
        }
    case 0x01:  /* Set System time */
        mem_writed(BIOS_TIMER,((unsigned int)reg_cx<<16u)|reg_dx);
        break;
    case 0x02:  /* GET REAL-TIME CLOCK TIME (AT,XT286,PS) */
        InitRtc();                          // make sure BCD and no am/pm
        if (RtcUpdateDone()) {              // make sure it's safe to read
            reg_ch = ReadCmosByte(0x04);    // hours
            reg_cl = ReadCmosByte(0x02);    // minutes
            reg_dh = ReadCmosByte(0x00);    // seconds
            reg_dl = ReadCmosByte(0x0b) & 0x01; // daylight saving time
	    /* 2023/10/06 - Let interrupts and CPU cycles catch up and the RTC clock a chance to tick. This is needed for
	     * "Pizza Tycoon" which appears to start by running in a loop reading time from the BIOS and writing
	     * time to INT 21h in a loop until the second value changes. */
            for (unsigned int c=0;c < 4;c++) CALLBACK_Idle();
        }
        CALLBACK_SCF(false);
        break;
    case 0x03:  // set RTC time
        InitRtc();                          // make sure BCD and no am/pm
        if (RtcUpdateDone()) {              // make sure it's safe to read
            WriteCmosByte(0x0b, ReadCmosByte(0x0b) | 0x80u);     // prohibit updates
            WriteCmosByte(0x04, reg_ch);        // hours
            WriteCmosByte(0x02, reg_cl);        // minutes
            WriteCmosByte(0x00, reg_dh);        // seconds
            WriteCmosByte(0x0b, (ReadCmosByte(0x0b) & 0x7eu) | (reg_dh & 0x01u)); // dst + implicitly allow updates
	    /* 2023/10/06 - Let interrupts and CPU cycles catch up and the RTC clock a chance to tick. This is needed for
	     * "Pizza Tycoon" which appears to start by running in a loop reading time from the BIOS and writing
	     * time to INT 21h in a loop until the second value changes. */
            for (unsigned int c=0;c < 4;c++) CALLBACK_Idle();
        }
        break;
    case 0x04:  /* GET REAL-TIME ClOCK DATE  (AT,XT286,PS) */
        InitRtc();                          // make sure BCD and no am/pm
        if (RtcUpdateDone()) {              // make sure it's safe to read
            reg_ch = ReadCmosByte(0x32);    // century
            reg_cl = ReadCmosByte(0x09);    // year
            reg_dh = ReadCmosByte(0x08);    // month
            reg_dl = ReadCmosByte(0x07);    // day
	    /* 2023/10/06 - Let interrupts and CPU cycles catch up and the RTC clock a chance to tick. This is needed for
	     * "Pizza Tycoon" which appears to start by running in a loop reading time from the BIOS and writing
	     * time to INT 21h in a loop until the second value changes. */
            for (unsigned int c=0;c < 4;c++) CALLBACK_Idle();
        }
        CALLBACK_SCF(false);
        break;
    case 0x05:  // set RTC date
        InitRtc();                          // make sure BCD and no am/pm
        if (RtcUpdateDone()) {              // make sure it's safe to read
            WriteCmosByte(0x0b, ReadCmosByte(0x0b) | 0x80);     // prohibit updates
            WriteCmosByte(0x32, reg_ch);    // century
            WriteCmosByte(0x09, reg_cl);    // year
            WriteCmosByte(0x08, reg_dh);    // month
            WriteCmosByte(0x07, reg_dl);    // day
            WriteCmosByte(0x0b, (ReadCmosByte(0x0b) & 0x7f));   // allow updates
	    /* 2023/10/06 - Let interrupts and CPU cycles catch up and the RTC clock a chance to tick. This is needed for
	     * "Pizza Tycoon" which appears to start by running in a loop reading time from the BIOS and writing
	     * time to INT 21h in a loop until the second value changes. */
            for (unsigned int c=0;c < 4;c++) CALLBACK_Idle();
        }
        break;
    case 0x80:  /* Pcjr Setup Sound Multiplexer */
        LOG(LOG_BIOS,LOG_ERROR)("INT1A:80:Setup tandy sound multiplexer to %d",reg_al);
        break;
    case 0x81:  /* Tandy sound system check */
    case 0x82:  /* Tandy sound system start recording */
    case 0x83:  /* Tandy sound system start playback */
    case 0x84:  /* Tandy sound system stop playing */
    case 0x85:  /* Tandy sound system reset */
        TandyDAC_Handler(reg_ah);
        break;
    case 0xb1:      /* PCI Bios Calls */
        if (pcibus_enable) {
            LOG(LOG_BIOS,LOG_WARN)("INT1A:PCI bios call %2X",reg_al);
            switch (reg_al) {
                case 0x01:  // installation check
                    if (PCI_IsInitialized()) {
                        reg_ah=0x00;
                        reg_al=0x01;    // cfg space mechanism 1 supported
                        reg_bx=0x0210;  // ver 2.10
                        reg_cx=0x0000;  // only one PCI bus
                        reg_edx=0x20494350;
                        reg_edi=PCI_GetPModeInterface();
                        CALLBACK_SCF(false);
                    } else {
                        CALLBACK_SCF(true);
                    }
                    break;
                case 0x02: {    // find device
                    Bitu devnr=0u;
                    Bitu count=0x100u;
                    uint32_t devicetag=((unsigned int)reg_cx<<16u)|reg_dx;
                    Bits found=-1;
                    for (Bitu i=0; i<=count; i++) {
                        IO_WriteD(0xcf8,0x80000000u|(i<<8u)); // query unique device/subdevice entries
                        if (IO_ReadD(0xcfc)==devicetag) {
                            if (devnr==reg_si) {
                                found=(Bits)i;
                                break;
                            } else {
                                // device found, but not the SIth device
                                devnr++;
                            }
                        }
                    }
                    if (found>=0) {
                        reg_ah=0x00;
                        reg_bh=0x00;    // bus 0
                        reg_bl=(uint8_t)(found&0xff);
                        CALLBACK_SCF(false);
                    } else {
                        reg_ah=0x86;    // device not found
                        CALLBACK_SCF(true);
                    }
                    }
                    break;
                case 0x03: {    // find device by class code
                    Bitu devnr=0;
                    Bitu count=0x100u;
                    uint32_t classtag=reg_ecx&0xffffffu;
                    Bits found=-1;
                    for (Bitu i=0; i<=count; i++) {
                        IO_WriteD(0xcf8,0x80000000u|(i<<8u)); // query unique device/subdevice entries
                        if (IO_ReadD(0xcfc)!=0xffffffffu) {
                            IO_WriteD(0xcf8,0x80000000u|(i<<8u)|0x08u);
                            if ((IO_ReadD(0xcfc)>>8u)==classtag) {
                                if (devnr==reg_si) {
                                    found=(Bits)i;
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
                        reg_bh=0x00;    // bus 0
                        reg_bl=(uint8_t)found & 0xffu;
                        CALLBACK_SCF(false);
                    } else {
                        reg_ah=0x86;    // device not found
                        CALLBACK_SCF(true);
                    }
                    }
                    break;
                case 0x08:  // read configuration byte
                    IO_WriteD(0xcf8,0x80000000u|((unsigned int)reg_bx<<8u)|(reg_di&0xfcu));
                    reg_cl=IO_ReadB(0xcfc+(reg_di&3u));
                    CALLBACK_SCF(false);
                    reg_ah=0x00;
                    break;
                case 0x09:  // read configuration word
                    IO_WriteD(0xcf8,0x80000000u|((unsigned int)reg_bx<<8u)|(reg_di&0xfcu));
                    reg_cx=IO_ReadW(0xcfc+(reg_di&2u));
                    CALLBACK_SCF(false);
                    reg_ah=0x00;
                    break;
                case 0x0a:  // read configuration dword
                    IO_WriteD(0xcf8,0x80000000u|((unsigned int)reg_bx<<8u)|(reg_di&0xfcu));
                    reg_ecx=IO_ReadD(0xcfc+(reg_di&3u));
                    CALLBACK_SCF(false);
                    reg_ah=0x00;
                    break;
                case 0x0b:  // write configuration byte
                    IO_WriteD(0xcf8,0x80000000u|((unsigned int)reg_bx<<8u)|(reg_di&0xfcu));
                    IO_WriteB(0xcfc+(reg_di&3u),reg_cl);
                    CALLBACK_SCF(false);
                    reg_ah=0x00;
                    break;
                case 0x0c:  // write configuration word
                    IO_WriteD(0xcf8,0x80000000u|((unsigned int)reg_bx<<8u)|(reg_di&0xfcu));
                    IO_WriteW(0xcfc+(reg_di&2u),reg_cx);
                    CALLBACK_SCF(false);
                    reg_ah=0x00;
                    break;
                case 0x0d:  // write configuration dword
                    IO_WriteD(0xcf8,0x80000000u|((unsigned int)reg_bx<<8u)|(reg_di&0xfcu));
                    IO_WriteD(0xcfc+((unsigned int)reg_di&3u),reg_ecx);
                    CALLBACK_SCF(false);
                    reg_ah=0x00;
                    break;
                default:
                    LOG(LOG_BIOS,LOG_ERROR)("INT1A:PCI BIOS: unknown function %x (%x %x %x)",
                        reg_ax,reg_bx,reg_cx,reg_dx);
                    CALLBACK_SCF(true);
                    break;
            }
        }
        else {
            CALLBACK_SCF(true);
        }
        break;
    default:
        LOG(LOG_BIOS,LOG_ERROR)("INT1A:Undefined call %2X",reg_ah);
    }
    return CBRET_NONE;
}   

bool INT16_get_key(uint16_t &code);
bool INT16_peek_key(uint16_t &code);

extern uint8_t                     GDC_display_plane;
extern uint8_t                     GDC_display_plane_pending;
extern bool                        GDC_vsync_interrupt;

unsigned char prev_pc98_mode42 = 0;

unsigned char pc98_function_row_mode = 0;

const char *pc98_func_key_default[10] = {
    " C1  ",
    " CU  ",
    " CA  ",
    " S1  ",
    " SU  ",

    "VOID ",
    "NWL  ",
    "INS  ",
    "REP  ",
    " ^Z  "
};

const char pc98_func_key_escapes_default[10][3] = {
    {0x1B,0x53,0},          // F1
    {0x1B,0x54,0},          // F2
    {0x1B,0x55,0},          // F3
    {0x1B,0x56,0},          // F4
    {0x1B,0x57,0},          // F5
    {0x1B,0x45,0},          // F6
    {0x1B,0x4A,0},          // F7
    {0x1B,0x50,0},          // F8
    {0x1B,0x51,0},          // F9
    {0x1B,0x5A,0}           // F10
};

const char pc98_editor_key_escapes_default[11][3] = {
    {0},                    // ROLL UP                  0x36
    {0},                    // ROLL DOWN                0x37
    {0x1B,0x50,0},          // INS                      0x38
    {0x1B,0x44,0},          // DEL                      0x39
    {0x0B,0},               // UP ARROW                 0x3A
    {0x08,0},               // LEFT ARROW               0x3B
    {0x0C,0},               // RIGHT ARROW              0x3C
    {0x0A,0},               // DOWN ARROW               0x3D
    {0},                    // HOME/CLR                 0x3E
    {0},                    // HELP                     0x3F
    {0}                     // KEYPAD -                 0x40
};

// shortcuts offered by SHIFT F1-F10. You can bring this onscreen using CTRL+F7. This row shows '*' in col 2.
// The text displayed onscreen is obviously just the first 6 chars of the shortcut text.
const char *pc98_shcut_key_defaults[10] = {
    "dir a:\x0D",
    "dir b:\x0D",
    "copy ",
    "del ",
    "ren ",

    "chkdsk a:\x0D",
    "chkdsk b:\x0D",
    "type ",
    "date\x0D",
    "time\x0D"
};

#pragma pack(push,1)
struct pc98_func_key_shortcut_def {
    unsigned char           length;         /* +0x00  length of text */
    unsigned char           shortcut[0x0F]; /* +0x01  Shortcut text to insert into CON device */

    std::string getShortcutText(void) const {
        std::string ret;
        unsigned int i;

        /* Whether a shortcut or escape (0xFE or not) the first 6 chars are displayed always */
        /* TODO: Strings for display are expected to display as Shift-JIS, convert to UTF-8 for display on host */
        for (i=0;i < 0x0F;i++) {
            if (shortcut[i] == 0u)
                break;
            else if (shortcut[i] == 0x1B) {
                ret += "<ESC>";
            }
            else if (shortcut[i] > 0x7Fu || shortcut[i] < 32u) /* 0xFE is invisible on real hardware */
                ret += ' ';
            else
                ret += (char)shortcut[i];
        }

        return ret;
    }

    std::string getDisplayText(void) const {
        unsigned int i;
        char tmp[8];

        /* Whether a shortcut or escape (0xFE or not) the first 6 chars are displayed always */
        /* TODO: Strings for display are expected to display as Shift-JIS, convert to UTF-8 for display on host */
        for (i=0;i < 6;i++) {
            if (shortcut[i] == 0u)
                break;
            else if (shortcut[i] > 0x7Fu || shortcut[i] < 32u) /* 0xFE is invisible on real hardware */
                tmp[i] = ' ';
            else
                tmp[i] = (char)shortcut[i];
        }
        tmp[i] = 0;

        return tmp;
    }

    std::string debugToString(void) const {
        std::string ret;
        char tmp[512];

        if (length == 0)
            return "(none)";

        if (shortcut[0] == 0xFE) {
            sprintf(tmp,"disp=\"%s\" ",getDisplayText().c_str());
            ret += tmp;

            ret += "dispraw={ ";
            for (unsigned int i=0;i < 6;i++) {
                sprintf(tmp,"%02x ",shortcut[i]);
                ret += tmp;
            }
            ret += "} ";

            ret += "esc={ ";
            for (unsigned int i=6;i < length;i++) {
                sprintf(tmp,"%02x ",shortcut[i]);
                ret += tmp;
            }
            ret += "}";
        }
        else {
            sprintf(tmp,"text=\"%s\" ",getShortcutText().c_str());
            ret += tmp;

            ret += "esc={ ";
            for (unsigned int i=0;i < length;i++) {
                sprintf(tmp,"%02x ",shortcut[i]);
                ret += tmp;
            }
            ret += "}";
        }

        return ret;
    }

    // set shortcut.
    // usually a direct string to insert.
    void set_shortcut(const char *str) {
        unsigned int i=0;
        char c;

        while (i < 0x0F && (c = *str++) != 0) shortcut[i++] = (unsigned char)c;
        length = i;

        while (i < 0x0F) shortcut[i++] = 0;
    }

    // set text and escape code. text does NOT include the leading 0xFE char.
    void set_text_and_escape(const char *text,const char *escape) {
        unsigned int i=1;
        char c;

        // this is based on observed MS-DOS behavior on PC-98.
        // the length byte covers both the display text and escape code (sum of the two).
        // the first byte of the shortcut is 0xFE which apparently means the next 5 chars
        // are text to display. The 0xFE is copied as-is to the display when rendered.
        // 0xFE in the CG ROM is a blank space.
        shortcut[0] = 0xFE;
        while (i < 6 && (c = *text++) != 0) shortcut[i++] = (unsigned char)c;
        while (i < 6) shortcut[i++] = ' ';

        while (i < 0x0F && (c = *escape++) != 0) shortcut[i++] = (unsigned char)c;
        length = i;
        while (i < 0x0F) shortcut[i++] = 0;
    }
};                                          /* =0x10 */
#pragma pack(pop)

struct pc98_func_key_shortcut_def   pc98_func_key[10];                  /* F1-F10 */
struct pc98_func_key_shortcut_def   pc98_vfunc_key[5];                  /* VF1-VF5 */
struct pc98_func_key_shortcut_def   pc98_func_key_shortcut[10];         /* Shift+F1 - Shift-F10 */
struct pc98_func_key_shortcut_def   pc98_vfunc_key_shortcut[5];         /* Shift+VF1 - Shift-VF5 */
struct pc98_func_key_shortcut_def   pc98_func_key_ctrl[10];             /* Control+F1 - Control-F10 */
struct pc98_func_key_shortcut_def   pc98_vfunc_key_ctrl[5];             /* Control+VF1 - Control-VF5 */
struct pc98_func_key_shortcut_def   pc98_editor_key_escapes[11];        /* Editor keys */

// FIXME: This is STUPID. Cleanup is needed in order to properly use std::min without causing grief.
#ifdef _MSC_VER
# define MIN(a,b) ((a) < (b) ? (a) : (b))
# define MAX(a,b) ((a) > (b) ? (a) : (b))
#else
# define MIN(a,b) std::min(a,b)
# define MAX(a,b) std::max(a,b)
#endif

void PC98_GetFuncKeyEscape(size_t &len,unsigned char buf[16],const unsigned int i,const struct pc98_func_key_shortcut_def *keylist) {
    if (i >= 1 && i <= 10) {
        const pc98_func_key_shortcut_def &def = keylist[i-1u];
        unsigned int j=0,o=0;

        /* if the shortcut starts with 0xFE then the next 5 chars are intended for display only
         * and the shortcut starts after that. Else the entire string is stuffed into the CON
         * device. */
        if (def.shortcut[0] == 0xFE)
            j = 6;

        while (j < MIN(0x0Fu,(unsigned int)def.length))
            buf[o++] = def.shortcut[j++];

        len = (size_t)o;
        buf[o] = 0;
    }
    else {
        len = 0;
        buf[0] = 0;
    }
}

void PC98_GetEditorKeyEscape(size_t &len,unsigned char buf[16],const unsigned int scan) {
    if (scan >= 0x36 && scan <= 0x40) {
        const pc98_func_key_shortcut_def &def = pc98_editor_key_escapes[scan-0x36];
        unsigned int j=0,o=0;

        while (j < MIN(0x05u,(unsigned int)def.length))
            buf[o++] = def.shortcut[j++];

        len = (size_t)o;
        buf[o] = 0;
    }
    else {
        len = 0;
        buf[0] = 0;
    }
}

void PC98_GetVFKeyEscape(size_t &len,unsigned char buf[16],const unsigned int i,const struct pc98_func_key_shortcut_def *keylist) {
    if (i >= 1 && i <= 5) {
        const pc98_func_key_shortcut_def &def = keylist[i-1];
        unsigned int j=0,o=0;

        /* if the shortcut starts with 0xFE then the next 5 chars are intended for display only
         * and the shortcut starts after that. Else the entire string is stuffed into the CON
         * device. */
        if (def.shortcut[0] == 0xFE)
            j = 6;

        while (j < MIN(0x0Fu,(unsigned int)def.length))
            buf[o++] = def.shortcut[j++];

        len = (size_t)o;
        buf[o] = 0;
    }
    else {
        len = 0;
        buf[0] = 0;
    }
}

void PC98_GetFuncKeyEscape(size_t &len,unsigned char buf[16],const unsigned int i) {
    PC98_GetFuncKeyEscape(len,buf,i,pc98_func_key);
}

void PC98_GetShiftFuncKeyEscape(size_t &len,unsigned char buf[16],const unsigned int i) {
    PC98_GetFuncKeyEscape(len,buf,i,pc98_func_key_shortcut);
}

void PC98_GetCtrlFuncKeyEscape(size_t &len,unsigned char buf[16],const unsigned int i) {
    PC98_GetFuncKeyEscape(len,buf,i,pc98_func_key_ctrl);
}

void PC98_GetVFuncKeyEscape(size_t &len,unsigned char buf[16],const unsigned int i) {
    PC98_GetVFKeyEscape(len,buf,i,pc98_vfunc_key);
}

void PC98_GetShiftVFuncKeyEscape(size_t &len,unsigned char buf[16],const unsigned int i) {
    PC98_GetVFKeyEscape(len,buf,i,pc98_vfunc_key_shortcut);
}

void PC98_GetCtrlVFuncKeyEscape(size_t &len,unsigned char buf[16],const unsigned int i) {
    PC98_GetVFKeyEscape(len,buf,i,pc98_vfunc_key_ctrl);
}

void PC98_InitDefFuncRow(void) {
    for (unsigned int i=0;i < 10;i++) {
        pc98_func_key_shortcut_def &def = pc98_func_key[i];

        def.set_text_and_escape(pc98_func_key_default[i],pc98_func_key_escapes_default[i]);
    }
    for (unsigned int i=0;i < 10;i++) {
        pc98_func_key_shortcut_def &def = pc98_func_key_shortcut[i];

        def.set_shortcut(pc98_shcut_key_defaults[i]);
    }
    for (unsigned int i=0;i < 11;i++) {
        pc98_func_key_shortcut_def &def = pc98_editor_key_escapes[i];

        def.set_shortcut(pc98_editor_key_escapes_default[i]);
    }
    for (unsigned int i=0;i < 10;i++) {
        pc98_func_key_shortcut_def &def = pc98_func_key_ctrl[i];

        def.set_shortcut("");
    }
    /* MS-DOS by default does not assign the VFn keys anything */
    for (unsigned int i=0;i < 5;i++) {
        pc98_func_key_shortcut_def &def = pc98_vfunc_key[i];

        def.set_shortcut("");
    }
    for (unsigned int i=0;i < 5;i++) {
        pc98_func_key_shortcut_def &def = pc98_vfunc_key_shortcut[i];

        def.set_shortcut("");
    }
    for (unsigned int i=0;i < 5;i++) {
        pc98_func_key_shortcut_def &def = pc98_vfunc_key_ctrl[i];

        def.set_shortcut("");
    }
}

void draw_pc98_function_row_elem(unsigned int o, unsigned int co, const struct pc98_func_key_shortcut_def& key) {
    const unsigned char *str = key.shortcut;
    unsigned int j = 0,i = 0;

    // NTS: Some shortcut strings start with 0xFE, which is rendered as an invisible space anyway.

    // NTS: Apparently the strings are Shift-JIS and expected to render to the function key row
    //      the same way the console normally does it.
    ShiftJISDecoder sjis;

    while (j < 6u && str[i] != 0) {
        if (sjis.take(str[i++])) {
            if (sjis.doublewide) {
                /* JIS conversion to WORD value appropriate for text RAM */
                if (sjis.b2 != 0) sjis.b1 -= 0x20;

                uint16_t w = (sjis.b2 << 8) + sjis.b1;
                mem_writew(0xA0000+((o+co+j)*2u),w);
                mem_writeb(0xA2000+((o+co+j)*2u),0xE5); // white  reverse  visible
                j++;
                mem_writew(0xA0000+((o+co+j)*2u),w);
                mem_writeb(0xA2000+((o+co+j)*2u),0xE5); // white  reverse  visible
                j++;
            }
            else {
                mem_writew(0xA0000+((o+co+j)*2u),str[j]);
                mem_writeb(0xA2000+((o+co+j)*2u),0xE5); // white  reverse  visible
                j++;
            }
        }
    }

    while (j < 6u) {
        mem_writew(0xA0000+((o+co+j)*2u),(unsigned char)(' '));
        mem_writeb(0xA2000+((o+co+j)*2u),0xE5); // white  reverse  visible
        j++;
    }
}

void draw_pc98_function_row(unsigned int o, const struct pc98_func_key_shortcut_def* keylist) {
    mem_writew(0xA0000+((o+1)*2),real_readb(0x60,0x8B));
    mem_writeb(0xA2000+((o+1)*2),0xE1);
    for (unsigned int i=0u;i < 5u;i++)
        draw_pc98_function_row_elem(o,4u + (i * 7u),keylist[i]);
    for (unsigned int i=5u;i < 10u;i++)
        draw_pc98_function_row_elem(o,42u + ((i - 5u) * 7u),keylist[i]);
}

unsigned int pc98_DOS_console_rows(void) {
    uint8_t b = real_readb(0x60,0x113);

    return (b & 0x01) ? 25 : 20;
}

void update_pc98_function_row(unsigned char setting,bool force_redraw) {
    if (!force_redraw && pc98_function_row_mode == setting) return;
    pc98_function_row_mode = setting;

    unsigned int total_rows = pc98_DOS_console_rows();
    unsigned char c = real_readb(0x60,0x11C);
    unsigned char r = real_readb(0x60,0x110);
    unsigned int o = 80 * (total_rows - 1);

    if (pc98_function_row_mode != 0) {
        if (r > (total_rows - 2)) {
            r = (total_rows - 2);
            void INTDC_CL10h_AH04h(void);
            INTDC_CL10h_AH04h();
        }
    }

    /* update mode 2 indicator */
    real_writeb(0x60,0x8C,(pc98_function_row_mode == 2) ? '*' : ' ');

    real_writeb(0x60,0x112,total_rows - 1 - ((pc98_function_row_mode != 0) ? 1 : 0));

    if (pc98_function_row_mode == 2) {
        /* draw the function row.
         * based on real hardware:
         *
         * The function key is 72 chars wide. 4 blank chars on each side of the screen.
         * It is divided into two halves, 36 chars each.
         * Within each half, aligned to its side, is 5 x 7 regions.
         * 6 of the 7 are inverted. centered in the white block is the function key. */
        for (unsigned int i=0;i < 40;) {
            mem_writew(0xA0000+((o+i)*2),0x0000);
            mem_writeb(0xA2000+((o+i)*2),0xE1);

            mem_writew(0xA0000+((o+(79-i))*2),0x0000);
            mem_writeb(0xA2000+((o+(79-i))*2),0xE1);

            if (i >= 3 && i < 38)
                i += 7;
            else
                i++;
        }

        mem_writew(0xA0000+((o+2)*2),real_readb(0x60,0x8C));
        mem_writeb(0xA2000+((o+2)*2),0xE1);

        draw_pc98_function_row(o,pc98_func_key_shortcut);
    }
    else if (pc98_function_row_mode == 1) {
        /* draw the function row.
         * based on real hardware:
         *
         * The function key is 72 chars wide. 4 blank chars on each side of the screen.
         * It is divided into two halves, 36 chars each.
         * Within each half, aligned to its side, is 5 x 7 regions.
         * 6 of the 7 are inverted. centered in the white block is the function key. */
        for (unsigned int i=0;i < 40;) {
            mem_writew(0xA0000+((o+i)*2),0x0000);
            mem_writeb(0xA2000+((o+i)*2),0xE1);

            mem_writew(0xA0000+((o+(79-i))*2),0x0000);
            mem_writeb(0xA2000+((o+(79-i))*2),0xE1);

            if (i >= 3 && i < 38)
                i += 7;
            else
                i++;
        }

        draw_pc98_function_row(o,pc98_func_key);
    }
    else {
        /* erase the function row */
        for (unsigned int i=0;i < 80;i++) {
            mem_writew(0xA0000+((o+i)*2),0x0000);
            mem_writeb(0xA2000+((o+i)*2),0xE1);
        }
    }

    real_writeb(0x60,0x11C,c);
    real_writeb(0x60,0x110,r);

    real_writeb(0x60,0x111,(pc98_function_row_mode != 0) ? 0x01 : 0x00);/* function key row display status */

    void vga_pc98_direct_cursor_pos(uint16_t address);
    vga_pc98_direct_cursor_pos((r*80)+c);
}

void pc98_function_row_user_toggle(void) {
    if (pc98_function_row_mode >= 2)
        update_pc98_function_row(0,true);
    else
        update_pc98_function_row(pc98_function_row_mode+1,true);
}

void pc98_set_char_mode(bool mode) {
    real_writeb(0x60,0x8A,mode);
    real_writeb(0x60,0x8B,(mode == true) ? ' ' : 'g');
    update_pc98_function_row(pc98_function_row_mode,true);
}

void pc98_toggle_char_mode(void) {
    pc98_set_char_mode(!real_readb(0x60,0x8A));
}

void pc98_set_digpal_entry(unsigned char ent,unsigned char grb);
void PC98_show_cursor(bool show);

extern bool                         gdc_5mhz_mode;
extern bool                         enable_pc98_egc;
extern bool                         enable_pc98_grcg;
extern bool                         enable_pc98_16color;
extern bool                         enable_pc98_256color;
extern bool                         enable_pc98_188usermod;
extern bool                         pc98_31khz_mode;
extern bool                         pc98_attr4_graphic;

extern unsigned char                pc98_text_first_row_scanline_start;  /* port 70h */
extern unsigned char                pc98_text_first_row_scanline_end;    /* port 72h */
extern unsigned char                pc98_text_row_scanline_blank_at;     /* port 74h */
extern unsigned char                pc98_text_row_scroll_lines;          /* port 76h */
extern unsigned char                pc98_text_row_scroll_count_start;    /* port 78h */
extern unsigned char                pc98_text_row_scroll_num_lines;      /* port 7Ah */

void pc98_update_text_layer_lineheight_from_bda(void) {
//    unsigned char c = mem_readb(0x53C);
    unsigned char lineheight = mem_readb(0x53B) + 1;

    pc98_gdc[GDC_MASTER].force_fifo_complete();
    pc98_gdc[GDC_MASTER].row_height = lineheight;

    if (lineheight > 20) { // usually 24
        pc98_text_first_row_scanline_start = 0x1C;
        pc98_text_first_row_scanline_end = lineheight - 5;
        pc98_text_row_scanline_blank_at = 16;
    }
    else if (lineheight > 16) { // usually 20
        pc98_text_first_row_scanline_start = 0x1E;
        pc98_text_first_row_scanline_end = lineheight - 3;
        pc98_text_row_scanline_blank_at = 16;
    }
    else {
        pc98_text_first_row_scanline_start = 0;
        pc98_text_first_row_scanline_end = lineheight - 1;
        pc98_text_row_scanline_blank_at = lineheight;
    }

    pc98_text_row_scroll_lines = 0;
    pc98_text_row_scroll_count_start = 0;
    pc98_text_row_scroll_num_lines = 0;

    vga.crtc.cursor_start = 0;
    vga.draw.cursor.sline = 0;

    vga.crtc.cursor_end   = lineheight - 1;
    vga.draw.cursor.eline = lineheight - 1;
}

void pc98_update_text_lineheight_from_bda(void) {
    unsigned char b597 = mem_readb(0x597);
    unsigned char c = mem_readb(0x53C);
    unsigned char lineheight;

    if ((b597 & 0x3) == 0x3) {//WARNING: This could be wrong
        if (c & 0x10)/*30-line mode (30x16 = 640x480)*/
            lineheight = 16;
        else if (c & 0x01)/*20-line mode (20x24 = 640x480)*/
            lineheight = 24;
        else/*25-line mode (25x19 = 640x480)*/
            lineheight = 19;
    }
    else {
        if (c & 0x10)/*30-line mode (30x13 = 640x400)*/
            lineheight = 13;//??
        else if (c & 0x01)/*20-line mode (20x20 = 640x400)*/
            lineheight = 20;
        else/*25-line mode (25x16 = 640x400)*/
            lineheight = 16;
    }

    mem_writeb(0x53B,lineheight - 1);
}

static const uint8_t pc98_katakana6x8_font[] = {
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x20,0x50,0x20,0x00,
	0x70,0x40,0x40,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x04,0x04,0x1c,0x00,
	0x00,0x00,0x00,0x00,0x40,0x20,0x10,0x00,0x00,0x00,0x00,0x30,0x30,0x00,0x00,0x00,
	0x7c,0x04,0x7c,0x04,0x04,0x08,0x30,0x00,0x00,0x00,0x7c,0x14,0x18,0x10,0x20,0x00,
	0x00,0x00,0x04,0x08,0x18,0x28,0x08,0x00,0x00,0x00,0x10,0x7c,0x44,0x04,0x18,0x00,
	0x00,0x00,0x00,0x38,0x10,0x10,0x7c,0x00,0x00,0x00,0x08,0x7c,0x18,0x28,0x48,0x00,
	0x00,0x00,0x20,0x7c,0x24,0x28,0x20,0x00,0x00,0x00,0x00,0x38,0x08,0x08,0x7c,0x00,
	0x00,0x00,0x3c,0x04,0x3c,0x04,0x3c,0x00,0x00,0x00,0x00,0x54,0x54,0x04,0x18,0x00,
	0x00,0x00,0x00,0x7c,0x00,0x00,0x00,0x00,0x7c,0x04,0x14,0x18,0x10,0x20,0x40,0x00,
	0x04,0x08,0x10,0x30,0x50,0x10,0x10,0x00,0x10,0x7c,0x44,0x44,0x04,0x08,0x10,0x00,
	0x00,0x7c,0x10,0x10,0x10,0x10,0x7c,0x00,0x08,0x7c,0x18,0x28,0x48,0x08,0x08,0x00,
	0x20,0x7c,0x24,0x24,0x24,0x44,0x08,0x00,0x10,0x7c,0x10,0x7c,0x10,0x10,0x10,0x00,
	0x3c,0x24,0x44,0x04,0x04,0x08,0x30,0x00,0x20,0x3c,0x28,0x48,0x08,0x08,0x30,0x00,
	0x00,0x7c,0x04,0x04,0x04,0x04,0x7c,0x00,0x28,0x28,0x7c,0x28,0x08,0x10,0x20,0x00,
	0x60,0x00,0x64,0x04,0x04,0x08,0x70,0x00,0x7c,0x04,0x04,0x08,0x10,0x28,0x44,0x00,
	0x20,0x7c,0x24,0x28,0x20,0x20,0x18,0x00,0x44,0x44,0x24,0x04,0x04,0x08,0x30,0x00,
	0x3c,0x24,0x34,0x4c,0x04,0x08,0x30,0x00,0x08,0x70,0x10,0x7c,0x10,0x10,0x20,0x00,
	0x54,0x54,0x54,0x04,0x08,0x08,0x30,0x00,0x38,0x00,0x7c,0x10,0x10,0x10,0x20,0x00,
	0x20,0x20,0x20,0x38,0x24,0x20,0x20,0x00,0x10,0x10,0x7c,0x10,0x10,0x20,0x40,0x00,
	0x00,0x38,0x00,0x00,0x00,0x00,0x7c,0x00,0x7c,0x04,0x04,0x28,0x10,0x28,0x40,0x00,
	0x10,0x7c,0x08,0x10,0x38,0x54,0x10,0x00,0x04,0x04,0x04,0x04,0x08,0x10,0x20,0x00,
	0x10,0x08,0x04,0x44,0x44,0x44,0x44,0x00,0x40,0x4c,0x70,0x40,0x40,0x40,0x3c,0x00,
	0x7c,0x04,0x04,0x04,0x04,0x08,0x30,0x00,0x00,0x20,0x50,0x08,0x04,0x04,0x00,0x00,
	0x10,0x7c,0x10,0x10,0x54,0x54,0x10,0x00,0x00,0x7c,0x04,0x04,0x28,0x10,0x08,0x00,
	0x00,0x38,0x00,0x38,0x00,0x38,0x04,0x00,0x10,0x10,0x20,0x40,0x44,0x7c,0x04,0x00,
	0x04,0x04,0x28,0x10,0x28,0x40,0x00,0x00,0x7c,0x10,0x7c,0x10,0x10,0x10,0x0c,0x00,
	0x20,0x7c,0x24,0x24,0x28,0x20,0x20,0x00,0x00,0x38,0x08,0x08,0x08,0x08,0x7c,0x00,
	0x7c,0x04,0x04,0x7c,0x04,0x04,0x7c,0x00,0x38,0x00,0x7c,0x04,0x04,0x08,0x30,0x00,
	0x24,0x24,0x24,0x04,0x04,0x08,0x10,0x00,0x10,0x50,0x50,0x50,0x54,0x58,0x10,0x00,
	0x20,0x20,0x20,0x24,0x24,0x28,0x30,0x00,0x7c,0x44,0x44,0x44,0x44,0x44,0x7c,0x00,
	0x7c,0x44,0x44,0x04,0x04,0x08,0x10,0x00,0x60,0x00,0x04,0x04,0x08,0x10,0x60,0x00,
	0x20,0x10,0x40,0x20,0x00,0x00,0x00,0x00,0x00,0x20,0x50,0x20,0x00,0x00,0x00,0x00
};

unsigned char byte_reverse(unsigned char c);

static void PC98_INT18_DrawShape(void)
{
	PhysPt ucw;
	uint8_t type, dir;
	uint16_t x1, y1;
	uint16_t ead, dad;
	uint16_t dc, d, d2, dm;

	ucw = SegPhys(ds) + reg_bx;
	type = mem_readb(ucw + 0x28);
	dir = mem_readb(ucw + 0x03);
	x1 = mem_readw(ucw + 0x08);
	y1 = mem_readw(ucw + 0x0a);
	if((reg_ch & 0xc0) == 0x40) {
		y1 += 200;
	}
	ead = (y1 * 40) + (x1 >> 4);
	dad = x1 % 16;
	// line pattern
	pc98_gdc[GDC_SLAVE].set_textw(((uint16_t)byte_reverse(mem_readb(ucw + 0x20)) << 8) | byte_reverse(mem_readb(ucw + 0x21)));
	if(type == 0x04) {
		// arc
		dc = mem_readw(ucw + 0x0c);
		d = mem_readw(ucw + 0x1c) - 1;
		d2 = d >> 1;
		dm = mem_readw(ucw + 0x1a);
		if((reg_ch & 0x30) == 0x30) {
			uint8_t plane = mem_readb(ucw + 0x00);
			uint32_t offset = 0x4000;
			for(uint8_t bit = 1 ; bit <= 4 ; bit <<= 1) {
				pc98_gdc[GDC_SLAVE].set_mode((plane & bit) ? 0x03 : 0x02);
				pc98_gdc[GDC_SLAVE].set_vectw(0x20, dir, dc, d, d2, 0x3fff, dm);
				pc98_gdc[GDC_SLAVE].set_csrw(offset + ead, dad);
				pc98_gdc[GDC_SLAVE].exec(0x6c);
				offset += 0x4000;
			}
		} else {
			pc98_gdc[GDC_SLAVE].set_mode(mem_readb(ucw + 0x02));
			pc98_gdc[GDC_SLAVE].set_vectw(0x20, dir, dc, d, d2, 0x3fff, dm);
			pc98_gdc[GDC_SLAVE].set_csrw(0x4000 + ((reg_ch & 0x30) << 10) + ead, dad);
			pc98_gdc[GDC_SLAVE].exec(0x6c);
		}
	} else {
		uint16_t x2, y2, temp;
		x2 = mem_readw(ucw + 0x16);
		y2 = mem_readw(ucw + 0x18);
		if(type == 0x01) {
			// line
			if((reg_ch & 0x30) == 0x30) {
				uint8_t plane = mem_readb(ucw + 0x00);
				uint32_t offset = 0x4000;
				for(uint8_t bit = 1 ; bit <= 4 ; bit <<= 1) {
					pc98_gdc[GDC_SLAVE].set_mode((plane & bit) ? 0x03 : 0x02);
					pc98_gdc[GDC_SLAVE].set_vectl(x1, y1, x2, y2);
					pc98_gdc[GDC_SLAVE].set_csrw(offset + ead, dad);
					pc98_gdc[GDC_SLAVE].exec(0x6c);
					offset += 0x4000;
				}
			} else {
				pc98_gdc[GDC_SLAVE].set_mode(mem_readb(ucw + 0x02));
				pc98_gdc[GDC_SLAVE].set_vectl(x1, y1, x2, y2);
				pc98_gdc[GDC_SLAVE].set_csrw(0x4000 + ((reg_ch & 0x30) << 10) + ead, dad);
				pc98_gdc[GDC_SLAVE].exec(0x6c);
			}
		} else if(type == 0x02) {
			// box
			uint16_t dx, dy;
			if(x1 > x2) {
				temp = x2; x2 = x1; x1 = temp;
			}
			if(y1 > y2) {
				temp = y2; y2 = y1; y1 = temp;
			}
			dx = x2 - x1;
			dy = y2 - y1;
			switch(dir & 3) {
			case 0:
				d = dy;
				d2 = dx;
				break;
			case 1:
				d2 = dx + dy;
				d2 >>= 1;
				d = dx - dy;
				d = (d >> 1) & 0x3fff;
				break;
			case 2:
				d = dx;
				d2 = dy;
				break;
			case 3:
				d2 = dx + dy;
				d2 >>= 1;
				d = dy - dx;
				d = (d >> 1) & 0x3fff;
				break;
			}
			if((reg_ch & 0x30) == 0x30) {
				uint8_t plane = mem_readb(ucw + 0x00);
				uint32_t offset = 0x4000;
				for(uint8_t bit = 1 ; bit <= 4 ; bit <<= 1) {
					pc98_gdc[GDC_SLAVE].set_mode((plane & bit) ? 0x03 : 0x02);
					pc98_gdc[GDC_SLAVE].set_vectw(0x40, dir, 3, d, d2, 0xffff, d);
					pc98_gdc[GDC_SLAVE].set_csrw(offset + ead, dad);
					pc98_gdc[GDC_SLAVE].exec(0x6c);
					offset += 0x4000;
				}
			} else {
				pc98_gdc[GDC_SLAVE].set_mode(mem_readb(ucw + 0x02));
				pc98_gdc[GDC_SLAVE].set_vectw(0x40, dir, 3, d, d2, 0xffff, d);
				pc98_gdc[GDC_SLAVE].set_csrw(0x4000 + ((reg_ch & 0x30) << 10) + ead, dad);
				pc98_gdc[GDC_SLAVE].exec(0x6c);
			}
		}
	}
}

static void PC98_INT18_DrawText(void)
{
	PhysPt ucw;
	uint8_t dir;
	uint8_t tile[8];
    uint16_t len;
	uint16_t x1, y1;
	uint16_t ead, dad;
	uint16_t dc, d;

	ucw = SegPhys(ds) + reg_bx;
	for(uint8_t i = 0 ; i < 8 ; i++) {
		tile[i] = byte_reverse(mem_readb(ucw + 0x20 + i));
	}
	pc98_gdc[GDC_SLAVE].set_textw(tile, 8);
	len = mem_readw(ucw + 0x0c);
	if(len > 0) {
		d = len;
		dc = (mem_readw(ucw + 0x1e) - 1) & 0x3fff;
	} else {
		d = 8;
		dc = 7;
	}
	dir = mem_readb(ucw + 0x03);
	x1 = mem_readw(ucw + 0x08);
	y1 = mem_readw(ucw + 0x0a);
	if((reg_ch & 0xc0) == 0x40) {
		y1 += 200;
	}
	ead = (y1 * 40) + (x1 >> 4);
	dad = x1 % 16;
	if((reg_ch & 0x30) == 0x30) {
		uint8_t plane = mem_readb(ucw + 0x00);
		uint32_t offset = 0x4000;
		for(uint8_t bit = 1 ; bit <= 4 ; bit <<= 1) {
			pc98_gdc[GDC_SLAVE].set_mode((plane & bit) ? 0x03 : 0x02);
			pc98_gdc[GDC_SLAVE].set_vectw(0x10, dir, dc, d, 0, 0, 0);
			pc98_gdc[GDC_SLAVE].set_csrw(offset + ead, dad);
			pc98_gdc[GDC_SLAVE].exec(0x68);
			offset += 0x4000;
		}
	} else {
		pc98_gdc[GDC_SLAVE].set_mode(mem_readb(ucw + 0x02));
		pc98_gdc[GDC_SLAVE].set_vectw(0x10, dir, dc, d, 0, 0, 0);
		pc98_gdc[GDC_SLAVE].set_csrw(0x4000 + ((reg_ch & 0x30) << 10) + ead, dad);
       	pc98_gdc[GDC_SLAVE].exec(0x68);
	}
}

/* TODO: The text and graphics code that talks to the GDC will need to be converted
 *       to CPU I/O read and write calls. I think the reason Windows 3.1's 16-color
 *       driver is causing screen distortion when going fullscreen with COMMAND.COM,
 *       and the reason COMMAND.COM windowed doesn't show anything, has to do with
 *       the fact that Windows 3.1 expects this BIOS call to use I/O so it can trap
 *       and virtualize the GDC and display state.
 *
 *       Obviously for the same reason VGA INT 10h emulation in IBM PC mode needs to
 *       do the same to prevent display and virtualization problems with the IBM PC
 *       version of Windows 3.1.
 *
 *       See also: [https://github.com/joncampbell123/dosbox-x/issues/1066] */
static Bitu INT18_PC98_Handler(void) {
    uint16_t temp16;

#if 0
    if (reg_ah >= 0x0A) {
            LOG_MSG("PC-98 INT 18h unknown call AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X",
                reg_ax,
                reg_bx,
                reg_cx,
                reg_dx,
                reg_si,
                reg_di,
                SegValue(ds),
                SegValue(es));
    }
#endif
#if defined(USE_TTF)
    if(!finish_prepare){
        conf_output= static_cast<Section_prop*>(control->GetSection("sdl"))->Get_string("output");
        if(conf_output == "ttf") { // BIOS screen does not like TTF mode, switch it on later
            toOutput("ttf");
            ttf_switch_off(true);
            is_ttfswitched_on = true;
            //LOG_MSG("TTF output is temporary switched off");
        }
    }
#endif
    /* NTS: Based on information gleaned from Neko Project II source code including comments which
     *      I've run through GNU iconv to convert from SHIFT-JIS to UTF-8 here in case Google Translate
     *      got anything wrong. */
    switch (reg_ah) {
        case 0x00: /* Reading of key data (キー・データの読みだし) */
            /* FIXME: We use the IBM PC/AT keyboard buffer to fake this call.
             *        This will be replaced with PROPER emulation once the PC-98 keyboard handler has been
             *        updated to write the buffer the way PC-98 BIOSes do it.
             *
             *        IBM PC/AT keyboard buffer at 40:1E-40:3D
             *
             *        PC-98 keyboard buffer at 50:02-50:21 */
            /* This call blocks until keyboard input */
            if (INT16_get_key(temp16)) {
                reg_ax = temp16;
            }
            else {
                /* Keyboard checks.
                 * If the interrupt got masked, unmask it.
                 * If the keyboard has data waiting, make sure the interrupt signal is active in case the last interrupt handler
                 * handled the keyboard interrupt and never read the keyboard (Quarth).
                 *
                 * TODO: Is this what real PC-98 BIOSes do? */
                void check_keyboard_fire_IRQ1(void);
                check_keyboard_fire_IRQ1();
                IO_WriteB(0x02,IO_ReadB(0x02) & (~(1u << /*IRQ*/1u))); // unmask IRQ1

                reg_ip += 1; /* step over IRET, to NOPs which then JMP back to callback */
            }
            break;
        case 0x01: /* Sense of key buffer state (キー・バッファ状態のセンス) */
            /* This call returns whether or not there is input waiting.
             * The waiting data is read, but NOT discarded from the buffer. */
            if (INT16_peek_key(temp16)) {
                reg_ax = temp16;
                reg_bh = 1;
            }
            else {
                /* Keyboard checks.
                 * If the interrupt got masked, unmask it.
                 * If the keyboard has data waiting, make sure the interrupt signal is active in case the last interrupt handler
                 * handled the keyboard interrupt and never read the keyboard (Quarth).
                 *
                 * TODO: Is this what real PC-98 BIOSes do? */
                void check_keyboard_fire_IRQ1(void);
                check_keyboard_fire_IRQ1();
                IO_WriteB(0x02,IO_ReadB(0x02) & (~(1u << /*IRQ*/1u))); // unmask IRQ1

                reg_bh = 0;
            }
            break;
        case 0x02: /* Sense of shift key state (シフト・キー状態のセンス) */
            reg_al = mem_readb(0x53A);
            break;
        case 0x03: /* Initialization of keyboard interface (キーボード・インタフェイスの初期化) */
            /* TODO */
            IO_WriteB(0x43, 0x3a);
            IO_WriteB(0x43, 0x32);
            IO_WriteB(0x43, 0x16);
            for (int i=0; i<0x20; i++) mem_writeb(0x502+i, 0);
            for (int i=0; i<0x13; i++) mem_writeb(0x528+i, 0);
            mem_writew(0x522,(unsigned int)(Real2Phys(BIOS_PC98_KEYBOARD_TRANSLATION_LOCATION) - 0xFD800));
            mem_writew(0x524, 0x0502);
            mem_writew(0x526, 0x0502);
            mem_writew(0x5C6,(unsigned int)(Real2Phys(BIOS_PC98_KEYBOARD_TRANSLATION_LOCATION) - 0xFD800));
            mem_writew(0x5C8,0xFD80);
            break;
        case 0x04: /* Sense of key input state (キー入力状態のセンス) */
            reg_ah = mem_readb(0x52A + (unsigned int)(reg_al & 0x0Fu));
            /* Hack for "Shangrlia" by Elf: The game's regulation of animation speed seems to depend on
             * INT 18h AH=0x04 taking some amount of time. If we do not do this, animation will run way
             * too fast and everyone will be talking/moving at a million miles a second.
             *
             * This is based on comparing animation speed vs the same game on real Pentium-class PC-98
             * hardware.
             *
             * Looking at the software loop involved during opening cutscenes, the game is constantly
             * polling INT 18h AH=04h (keyboard state) and INT 33h AH=03h (mouse button/position state)
             * while animating the characters on the screen. Without this delay, animation runs way too
             * fast.
             *
             * This guess is also loosely based on a report by the Touhou Community Reliant Automatic Patcher
             * that Touhou Project directly reads this byte but delays by 0.6ms to handle the fact that
             * the bit in question may toggle while the key is held down due to the scan codes returned by
             * the keyboard.
             *
             * This is a guess, but it seems to help animation speed match that of real hardware regardless
             * of cycle count in DOSBox-X. */
            CPU_Cycles -= (cpu_cycles_count_t)(CPU_CycleMax * 0.006);
            break;
        case 0x05: /* Key input sense (キー入力センス) */
            /* This appears to return a key from the buffer (and remove from
             * buffer) or return BH == 0 to signal no key was pending. */
            if (INT16_get_key(temp16)) {
                reg_ax = temp16;
                reg_bh = 1;
            }
            else {
                /* Keyboard checks.
                 * If the interrupt got masked, unmask it.
                 * If the keyboard has data waiting, make sure the interrupt signal is active in case the last interrupt handler
                 * handled the keyboard interrupt and never read the keyboard (Quarth).
                 *
                 * TODO: Is this what real PC-98 BIOSes do? */
                void check_keyboard_fire_IRQ1(void);
                check_keyboard_fire_IRQ1();
                IO_WriteB(0x02,IO_ReadB(0x02) & (~(1u << /*IRQ*/1u))); // unmask IRQ1

                reg_bh = 0;
            }
            break;
        case 0x0A: /* set CRT mode */
            /* bit      off         on
                0       25lines     20lines
                1       80cols      40cols
                2       v.lines     simp.graphics
                3       K-CG access mode(not used in PC-98) */
            
            //TODO: set 25/20 lines mode and 80/40 columns mode.
            //Attribute bit (bit 2)
            pc98_attr4_graphic = !!(reg_al & 0x04);
            pc98_40col_text = !!(reg_al & 0x02);
#if defined(USE_TTF)
            if (!(reg_al & 0xc) && pc98_gdc[GDC_MASTER].display_enable) ttf_switch_on(false);
#endif

            mem_writeb(0x53C,(mem_readb(0x53C) & 0xF0u) | (reg_al & 0x0Fu));

            if (reg_al & 8)
                LOG_MSG("INT 18H AH=0Ah warning: K-CG dot access mode not supported");

            pc98_update_text_lineheight_from_bda();
            pc98_update_text_layer_lineheight_from_bda();

            /* Apparently, this BIOS call also hides the cursor */
            PC98_show_cursor(0);
            break;
        case 0x0B: /* get CRT mode */
            /* bit      off         on
                0       25lines     20lines
                1       80cols      40cols
                2       v.lines     simp.graphics
                3       K-CG access mode(not used in PC-98) 
                7       std CRT     hi-res CRT */
            /* NTS: I assume that real hardware doesn't offer a way to read back the state of these bits,
             *      so the BIOS's only option is to read the mode byte back from the data area.
             *      Neko Project II agrees. */
            reg_al = mem_readb(0x53C);
            break;
        // TODO: "Edge" is using INT 18h AH=06h, what is that?
        //       (Something to do with the buffer [https://ia801305.us.archive.org/8/items/PC9800TechnicalDataBookBIOS1992/PC-9800TechnicalDataBook_BIOS_1992_text.pdf])
        //       Neko Project is also unaware of such a call.
        case 0x0C: /* text layer enable */
	    /* PROBLEM: Okay, so it's unclear when text layer is or is not allowed.
             *          I was unable to turn on the text layer with this BIOS call on real PC-9821 hardware, so I believed that it did not allow it.
             *
             *          But PC-9821 CD-ROM game "Shamat, The Holy Circlet" expects to turn on the text layer in 640x400 256-color PEGC mode,
             *          because it displays graphics in the background while scrolling Japanese text up over it, and if sound hardware is available,
             *          plays a voice reading the text synchronized to it.
             *
             *          Perhaps in my case it was 640x480 256-color mode, not 640x400 256-color mode, but then, 640x480 also enables a text mode with
             *          either more rows or a taller character cell which is apparently recognized by the MS-DOS console driver.
             *
             *          So then, what exactly decides whether or not to allow this call to enable the text layer? */
            if (pc98_gdc_vramop & (1u << VOPBIT_VGA) && 0/*DISABLED*/) {
               /* NTS: According to tests on real PC-9821 hardware, you can't turn on the text layer in 256-color mode, at least through the BIOS. */
               /* FIXME: Is this a restriction imposed by the BIOS, or the hardware itself? */
               LOG_MSG("INT 18h: Attempt to turn on text layer in 256-color mode");
            }
            else {
                pc98_gdc[GDC_MASTER].force_fifo_complete();
                pc98_gdc[GDC_MASTER].display_enable = true;
#if defined(USE_TTF)
                ttf_switch_on(false);
#endif
            }
            break;
        case 0x0D: /* text layer disable */
#if defined(USE_TTF)
            ttf_switch_off(false);
#endif
            pc98_gdc[GDC_MASTER].force_fifo_complete();
            pc98_gdc[GDC_MASTER].display_enable = false;
            break;
        case 0x0E: /* set text display area (DX=byte offset) */
            pc98_gdc[GDC_MASTER].force_fifo_complete();
            pc98_gdc[GDC_MASTER].param_ram[0] = (reg_dx >> 1) & 0xFF;
            pc98_gdc[GDC_MASTER].param_ram[1] = (reg_dx >> 9) & 0xFF;
            pc98_gdc[GDC_MASTER].param_ram[2] = (400 << 4) & 0xFF;
            pc98_gdc[GDC_MASTER].param_ram[3] = (400 << 4) >> 8;
            break;
        case 0x10: /* set cursor blink */
            PC98_show_cursor(false); /* side effect: hides the cursor */
            pc98_gdc[GDC_MASTER].force_fifo_complete();
            pc98_gdc[GDC_MASTER].cursor_blink = !!(reg_al & 1);
            break;
        case 0x11: /* show cursor */
            PC98_show_cursor(true);
            break;
        case 0x12: /* hide cursor */
            PC98_show_cursor(false);
            break;
        case 0x13: /* set cursor position (DX=byte position) */
            void vga_pc98_direct_cursor_pos(uint16_t address);

            pc98_gdc[GDC_MASTER].force_fifo_complete();
            vga_pc98_direct_cursor_pos(reg_dx >> 1);
            break;
        case 0x14: /* read FONT RAM */
            {
                unsigned int i,o,r;

                /* DX = code (must be 0x76xx or 0x7700)
                 * BX:CX = 34-byte region to write to */
                if (reg_dh == 0x80u) { /* 8x16 ascii */
                    i = ((unsigned int)reg_bx << 4u) + reg_cx + 2u;
                    mem_writew(i-2u,0x0102u);
                    for (r=0;r < 16u;r++) {
                        o = (reg_dl*16u)+r;

                        assert((o+2u) <= sizeof(vga.draw.font));

                        mem_writeb(i+r,vga.draw.font[o]);
                    }
                }
                else if ((reg_dh & 0xFC) == 0x28) { /* 8x16 kanji */
                    i = ((unsigned int)reg_bx << 4u) + reg_cx + 2u;
                    mem_writew(i-2u,0x0102u);
                    for (r=0;r < 16u;r++) {
                        o = (((((reg_dl & 0x7Fu)*128u)+((reg_dh - 0x20u) & 0x7Fu))*16u)+r)*2u;

                        assert((o+2u) <= sizeof(vga.draw.font));

                        mem_writeb(i+r+0u,vga.draw.font[o+0u]);
                    }
                }
                else if (reg_dh != 0) { /* 16x16 kanji */
                    i = ((unsigned int)reg_bx << 4u) + reg_cx + 2u;
                    mem_writew(i-2u,0x0202u);
                    for (r=0;r < 16u;r++) {
                        o = (((((reg_dl & 0x7Fu)*128u)+((reg_dh - 0x20u) & 0x7Fu))*16u)+r)*2u;

                        assert((o+2u) <= sizeof(vga.draw.font));

                        mem_writeb(i+(r*2u)+0u,vga.draw.font[o+0u]);
                        mem_writeb(i+(r*2u)+1u,vga.draw.font[o+1u]);
                    }
                }
                else if(reg_dl < 0x80) { /* 6x8 ascii .. Substitute 8x8 */
                    i = ((unsigned int)reg_bx << 4u) + reg_cx + 2u;
                    mem_writew(i-2u,0x0101u);
                    o = reg_dl * 8;
                    for (r=0;r < 8u;r++) {
                        mem_writeb(i+r, int10_font_08[o + r]);
                    }
                }
                else if(reg_dl >= 0xa0 && reg_dl <= 0xdf) { /* 6x8 kana */
                    i = ((unsigned int)reg_bx << 4u) + reg_cx + 2u;
                    mem_writew(i - 2u, 0x0101u);
                    o = (reg_dl - 0xa0) * 8;
                    for(r = 0; r < 8u; r++) {
                        mem_writeb(i + r, pc98_katakana6x8_font[o + r]);
                    }
                }
                else {
                    LOG_MSG("PC-98 INT 18h AH=14h font RAM read ignored, code 0x%04x not supported",reg_dx);
                }
            }
            break;
        case 0x16: /* fill screen with chr + attr */
            {
                /* DL = character
                 * DH = attribute */
                unsigned int i;

                for (i=0;i < 0x2000;i += 2) {
                    vga.mem.linear[i+0] = reg_dl;
                    vga.mem.linear[i+1] = 0x00;
                }
                for (   ;i < 0x3FE0;i += 2) {
                    vga.mem.linear[i+0] = reg_dh;
                    vga.mem.linear[i+1] = 0x00;
                }
            }
            break;
        case 0x17: /* BELL ON */
            IO_WriteB(0x37,0x06);
            break;
        case 0x18: /* BELL OFF */
            IO_WriteB(0x37,0x07);
            break;
        case 0x1A: /* load FONT RAM */
            {
                /* DX = code (must be 0x76xx or 0x7700)
                 * BX:CX = 34-byte region to read from */
                if ((reg_dh & 0x7Eu) == 0x76u) {
                    unsigned int i = ((unsigned int)reg_bx << 4u) + reg_cx + 2u;
                    for (unsigned int r=0;r < 16u;r++) {
                        unsigned int o = (((((reg_dl & 0x7Fu)*128u)+((reg_dh - 0x20u) & 0x7Fu))*16u)+r)*2u;

                        assert((o+2u) <= sizeof(vga.draw.font));

                        vga.draw.font[o+0u] = mem_readb(i+(r*2u)+0u);
                        vga.draw.font[o+1u] = mem_readb(i+(r*2u)+1u);
                    }
                }
                else {
                    LOG_MSG("PC-98 INT 18h AH=1Ah font RAM load ignored, code 0x%04x out of range",reg_dx);
                }
            }
            break;
        case 0x30: /* Set display mode */
            /* FIXME: There is still a lot that is inaccurate about this call */
            if (enable_pc98_egc) {
                unsigned char b597 = mem_readb(0x597);
                unsigned char tstat = mem_readb(0x53C);
                unsigned char b54C = mem_readb(0x54C);
                unsigned char ret = 0x05; // according to NP2

                // assume the same as AH=42h
                while (!(IO_ReadB(0x60) & 0x20/*vertical retrace*/)) {
                    void CALLBACK_Idle(void);
                    CALLBACK_Idle();
                }

                LOG_MSG("PC-98 INT 18 AH=30h AL=%02Xh BH=%02Xh",reg_al,reg_bh);

                if ((reg_bh & 0x30) == 0x30) { // 640x480
                    if ((reg_al & 0xC) == 0x0C) { // 31KHz sync
                        void PC98_Set31KHz_480line(void);
                        pc98_31khz_mode = true;
                        PC98_Set31KHz_480line();

                        void pc98_port6A_command_write(unsigned char b);
                        pc98_port6A_command_write(0x69); // disable 128KB wrap

                        b54C = (b54C & (~0x20)) + ((reg_al & 0x04) ? 0x20 : 0x00);

#if defined(USE_TTF)
                        ttf_switch_off(false);
#endif
                        pc98_gdc[GDC_MASTER].force_fifo_complete();
                        pc98_gdc[GDC_SLAVE].force_fifo_complete();

                        // according to real hardware, this also hides the text layer for some reason
                        pc98_gdc[GDC_MASTER].display_enable = false;

                        /* clear PRAM, graphics */
                        for (unsigned int i=0;i < 16;i++)
                            pc98_gdc[GDC_SLAVE].param_ram[i] = 0x00;

                        /* reset scroll area of graphics */
                        pc98_gdc[GDC_SLAVE].param_ram[0] = 0;
                        pc98_gdc[GDC_SLAVE].param_ram[1] = 0;

                        pc98_gdc[GDC_SLAVE].param_ram[2] = 0xF0;
                        pc98_gdc[GDC_SLAVE].param_ram[3] = 0x3F + (gdc_5mhz_according_to_bios()?0x40:0x00/*IM bit*/);
                        pc98_gdc[GDC_SLAVE].display_pitch = gdc_5mhz_according_to_bios() ? 80u : 40u;

                        pc98_gdc[GDC_SLAVE].doublescan = false;
                        pc98_gdc[GDC_SLAVE].row_height = 1;

                        b597 = (b597 & ~3u) + ((uint8_t)(reg_bh >> 4u) & 3u);

                        pc98_gdc_vramop &= ~(1 << VOPBIT_ACCESS);
                        pc98_update_cpu_page_ptr();

                        GDC_display_plane = GDC_display_plane_pending = 0;
                        pc98_update_display_page_ptr();

                        /* based on real hardware behavior, this ALSO sets 256-color mode */
                        void pc98_port6A_command_write(unsigned char b);
                        pc98_port6A_command_write(0x07);        // enable EGC
                        pc98_port6A_command_write(0x01);        // enable 16-color
                        pc98_port6A_command_write(0x21);        // enable 256-color
                    }
                    else {
                        // according to Neko Project II, this case is ignored.
                        // this is confirmed on real hardware as well.
                        LOG_MSG("PC-98 INT 18h AH=30h attempt to set 640x480 mode with 24KHz hsync which is not supported by the platform");
                        ret = 0;
                    }
                }
                else { // 640x400 or 640x200
                    // TODO: A PC9821Lt2 laptop's BIOS refuses to allow 31khz except for 640x480 mode.
                    //       Perhaps it's just a technical restriction of the LCD display.
                    //
                    //       Check on other PC-98 hardware to see what the policy is for 31khz in all modes.
                    //       That restriction would make no sense on another system I have that has a VGA
                    //       port and a default setting of 70Hz / 31KHz 640x400.
                    if ((reg_al & 0x0C) < 0x08) { /* bits [3:2] == 0x */
                        LOG_MSG("PC-98 INT 18h AH=30h attempt to set 15KHz hsync which is not yet supported");
                        ret = 0;
                    }
                    else {
                        if ((reg_al ^ (((b54C & 0x20) ? 3 : 2) << 2)) & 0x0C) { /* change in bits [3:2] */
                            LOG_MSG("PC-98 change in hsync frequency to %uHz",(reg_al & 0x04) ? 31 : 24);

                            if (reg_al & 4) {
                                void PC98_Set31KHz(void);
                                pc98_31khz_mode = true;
                                PC98_Set31KHz();
                            }
                            else {
                                void PC98_Set24KHz(void);
                                pc98_31khz_mode = false;
                                PC98_Set24KHz();
                            }

                            b54C = (b54C & (~0x20)) + ((reg_al & 0x04) ? 0x20 : 0x00);
                        }
                    }

                    void pc98_port6A_command_write(unsigned char b);
                    pc98_port6A_command_write(0x68); // restore 128KB wrap

#if defined(USE_TTF)
                    ttf_switch_off(false);
#endif
                    pc98_gdc[GDC_MASTER].force_fifo_complete();
                    pc98_gdc[GDC_SLAVE].force_fifo_complete();

                    // 640x480 forces 256-color mode.
                    // the 400 line modes (this case) do not clear 256-color mode.

                    // according to real hardware, this also hides the text layer for some reason
                    pc98_gdc[GDC_MASTER].display_enable = false;

                    /* clear PRAM, graphics */
                    for (unsigned int i=0;i < 16;i++)
                        pc98_gdc[GDC_SLAVE].param_ram[i] = 0x00;

                    /* reset scroll area of graphics */
                    if ((reg_bh & 0x30) == 0x10) { /* 640x200 upper half    bits [5:4] == 1 */
                        pc98_gdc[GDC_SLAVE].param_ram[0] = (200*40) & 0xFF;
                        pc98_gdc[GDC_SLAVE].param_ram[1] = (200*40) >> 8;
                    }
                    else {
                        pc98_gdc[GDC_SLAVE].param_ram[0] = 0;
                        pc98_gdc[GDC_SLAVE].param_ram[1] = 0;
                    }

                    pc98_gdc[GDC_SLAVE].param_ram[2] = 0xF0;
                    pc98_gdc[GDC_SLAVE].param_ram[3] = 0x3F + (gdc_5mhz_according_to_bios()?0x40:0x00/*IM bit*/);
                    pc98_gdc[GDC_SLAVE].display_pitch = gdc_5mhz_according_to_bios() ? 80u : 40u;

                    if ((reg_bh & 0x20) == 0x00) { /* 640x200 */
                        pc98_gdc[GDC_SLAVE].doublescan = true;
                        pc98_gdc[GDC_SLAVE].row_height = pc98_gdc[GDC_SLAVE].doublescan ? 2 : 1;
                    }
                    else {
                        pc98_gdc[GDC_SLAVE].doublescan = false;
                        pc98_gdc[GDC_SLAVE].row_height = 1;
                    }

                    b597 = (b597 & ~3u) + ((uint8_t)(reg_bh >> 4u) & 3u);

                    pc98_gdc_vramop &= ~(1 << VOPBIT_ACCESS);
                    pc98_update_cpu_page_ptr();

                    GDC_display_plane = GDC_display_plane_pending = 0;
                    pc98_update_display_page_ptr();
                }

                tstat &= ~(0x10 | 0x01);
                if (reg_bh & 2)
                    tstat |= 0x10;
                else if ((reg_bh & 1) == 0)
                    tstat |= 0x01;

                mem_writeb(0x597,b597);
                mem_writeb(0x53C,tstat);
                mem_writeb(0x54C,b54C);

                pc98_update_text_lineheight_from_bda();
                pc98_update_text_layer_lineheight_from_bda();

                // according to real hardware (PC-9821Lt2), AH=5 on success (same as NP2)
                // or AH is unchanged on failure and AL=1 and BH=1 (NOT the same as NP2)
                if (ret == 0x05) reg_ah = ret;
                reg_al = (ret == 0x05) ? 0x00 : 0x01; // according to NP2
                reg_bh = (ret == 0x05) ? 0x00 : 0x01; // according to NP2
            }
            break;
        case 0x31: /* Return display mode and status */
            /* NTS: According to NP II this call shouldn't even work unless you first call AH=30h to set 640x480 mode.
             *      It seems that is wrong. Real hardware will still return the current mode regardless. */
            if (enable_pc98_egc) { /* FIXME: INT 18h AH=31/30h availability is tied to EGC enable */
                unsigned char b597 = mem_readb(0x597);
                unsigned char tstat = mem_readb(0x53C);
                unsigned char b54C = mem_readb(0x54C);

                /* 54Ch:
                 * bit[5:5] = Horizontal sync rate                      1=31.47KHz      0=24.83KHz */

                /* Return values:
                 *
                 * AL =
                 *      bit [7:7] = ?
                 *      bit [6:6] = ?
                 *      bit [5:5] = ?
                 *      bit [4:4] = ?
                 *      bit [3:2] = horizontal sync
                 *                   00 = 15.98KHz
                 *                   01 = ?
                 *                   10 = 24.83KHz
                 *                   11 = 31.47KHz
                 *      bit [1:1] = ?
                 *      bit [0:0] = interlaced (1=yes 0=no)
                 * BH =
                 *      bit [7:7] = ?
                 *      bit [6:6] = ?
                 *      bit [5:4] = graphics video mode
                 *                   00 = 640x200 (upper half)
                 *                   01 = 640x200 (lower half)
                 *                   10 = 640x400
                 *                   11 = 640x480
                 *      bit [3:3] = ?
                 *      bit [2:2] = ?
                 *      bit [1:0] = number of text rows
                 *                   00 = 20 rows
                 *                   01 = 25 rows
                 *                   10 = 30 rows
                 *                   11 = ?
                 */
                reg_al =
                    (((b54C & 0x20) ? 3 : 2) << 2)/*hsync*/;
                reg_bh =
                    ((b597 & 3) << 4)/*graphics video mode*/;
                if (tstat & 0x10)
                    reg_bh |= 2;/*30 rows*/
                else if ((tstat & 0x01) == 0)
                    reg_bh |= 1;/*25 rows*/
            }
            break;
        /* From this point on the INT 18h call list appears to wander off from the keyboard into CRT/GDC/display management. */
        case 0x40: /* Start displaying the graphics screen (グラフィック画面の表示開始) */
            pc98_gdc[GDC_SLAVE].force_fifo_complete();
            pc98_gdc[GDC_SLAVE].display_enable = true;
#if defined(USE_TTF)
            if (!pc98_gdc[GDC_MASTER].display_enable) ttf_switch_off(false);
#endif
 
            {
                unsigned char b = mem_readb(0x54C/*MEMB_PRXCRT*/);
                mem_writeb(0x54C/*MEMB_PRXCRT*/,b | 0x80);
            }
            break;
        case 0x41: /* Stop displaying the graphics screen (グラフィック画面の表示終了) */
            pc98_gdc[GDC_SLAVE].force_fifo_complete();
            pc98_gdc[GDC_SLAVE].display_enable = false;
#if defined(USE_TTF)
            if (pc98_gdc[GDC_MASTER].display_enable) ttf_switch_on(false);
#endif

            {
                unsigned char b = mem_readb(0x54C/*MEMB_PRXCRT*/);
                mem_writeb(0x54C/*MEMB_PRXCRT*/,b & (~0x80));
            }
            break;
        case 0x42: /* Display area setup (表示領域の設定) */
            // HACK for Quarth: If the game has triggered vsync interrupt, wait for it.
            // Quarth's vsync interrupt will reprogram the display partitions back to what
            // it would have set for gameplay after this modeset and cause display problems
            // with the main menu. Waiting one vertical retrace period before mode setting
            // gives Quarth one last frame to reprogram partitions before realizing that
            // it's time to stop it.
            //
            // If the BIOS on real hardware has any check like this, it's probably a loop
            // to wait for vsync.
            //
            // The interrupt does NOT cancel the vertical retrace interrupt. Some games
            // (Rusty) will not work properly if this call cancels the vertical retrace
            // interrupt.
            while (!(IO_ReadB(0x60) & 0x20/*vertical retrace*/)) {
                void CALLBACK_Idle(void);
                CALLBACK_Idle();
            }

            pc98_gdc[GDC_MASTER].force_fifo_complete();
            pc98_gdc[GDC_SLAVE].force_fifo_complete();

            /* clear PRAM, graphics */
            for (unsigned int i=0;i < 16;i++)
                pc98_gdc[GDC_SLAVE].param_ram[i] = 0x00;

            /* reset scroll area of graphics */
            if ((reg_ch & 0xC0) == 0x40) { /* 640x200 G-RAM upper half */
                pc98_gdc[GDC_SLAVE].param_ram[0] = (200*40) & 0xFF;
                pc98_gdc[GDC_SLAVE].param_ram[1] = (200*40) >> 8;
            }
            else {
                pc98_gdc[GDC_SLAVE].param_ram[0] = 0;
                pc98_gdc[GDC_SLAVE].param_ram[1] = 0;
            }

            pc98_gdc[GDC_SLAVE].param_ram[2] = 0xF0;
            pc98_gdc[GDC_SLAVE].param_ram[3] = 0x3F + (gdc_5mhz_according_to_bios()?0x40:0x00/*IM bit*/);
            pc98_gdc[GDC_SLAVE].display_pitch = gdc_5mhz_according_to_bios() ? 80u : 40u;

            // CH
            //   [7:6] = G-RAM setup
            //           00 = no graphics (?)
            //           01 = 640x200 upper half
            //           10 = 640x200 lower half
            //           11 = 640x400
            //   [5:5] = CRT
            //           0 = color
            //           1 = monochrome
            //   [4:4] = Display bank

            // Color or monochrome mode
            IO_WriteB(0x68, (reg_ch & 0x20) ? 0x03 : 0x02);

            // FIXME: This is a guess. I have no idea as to actual behavior, yet.
            //        This seems to help with clearing the text layer when games start the graphics.
            //        This is ALSO how we will detect games that switch on the 200-line double-scan mode vs 400-line mode.
            if ((reg_ch & 0xC0) != 0) {
                pc98_gdc[GDC_SLAVE].doublescan = ((reg_ch & 0xC0) == 0x40) || ((reg_ch & 0xC0) == 0x80);
                pc98_gdc[GDC_SLAVE].row_height = pc98_gdc[GDC_SLAVE].doublescan ? 2 : 1;

                /* update graphics mode bits */
                {
                    unsigned char b = mem_readb(0x597);

                    b &= ~3;
                    b |= ((reg_ch >> 6) - 1) & 3;

                    mem_writeb(0x597,b);
                }
            }
            else {
                pc98_gdc[GDC_SLAVE].doublescan = false;
                pc98_gdc[GDC_SLAVE].row_height = 1;
            }

            {
                unsigned char b = mem_readb(0x54C/*MEMB_PRXCRT*/);

                // Real hardware behavior: graphics selection updated by BIOS to reflect MEMB_PRXCRT state
                pc98_gdc[GDC_SLAVE].display_enable = !!(b & 0x80);
#if defined(USE_TTF)
                if (pc98_gdc[GDC_SLAVE].display_enable)
                    ttf_switch_off(false);
                else if (pc98_gdc[GDC_MASTER].display_enable)
                    ttf_switch_on(false);
#endif
            }

            GDC_display_plane = GDC_display_plane_pending = (reg_ch & 0x10) ? 1 : 0;
            pc98_update_display_page_ptr();

            prev_pc98_mode42 = reg_ch;

            LOG_MSG("PC-98 INT 18 AH=42h CH=0x%02X",reg_ch);
            break;
        case 0x43:  // Palette register settings? Only works in digital mode? --leonier
                    //
                    // This is said to fix Thexder's GAME ARTS logo. --Jonathan C.
                    //
                    // TODO: Validate this against real PC-98 hardware and BIOS
            {
                unsigned int gbcpc = SegValue(ds)*0x10u + reg_bx;
                for(unsigned int i=0;i<4;i++)
                {
                    unsigned char p=mem_readb(gbcpc+4u+i);
                    pc98_set_digpal_entry(7u-2u*i, p&0xFu);
                    pc98_set_digpal_entry(6u-2u*i, p>>4u);
                }
                LOG_MSG("PC-98 INT 18 AH=43h CX=0x%04X DS=0x%04X", reg_cx, SegValue(ds));
                break;
            }
        case 0x47:	// Line, Box
        case 0x48:	// Arc
            PC98_INT18_DrawShape();
            break;
        case 0x49:	// Text
            PC98_INT18_DrawText();
            break;
        case 0x4D:  // 256-color enable
            if (reg_ch == 1) {
                void pc98_port6A_command_write(unsigned char b);
                pc98_port6A_command_write(0x07);        // enable EGC
                pc98_port6A_command_write(0x01);        // enable 16-color
                pc98_port6A_command_write(0x21);        // enable 256-color
                PC98_show_cursor(false);                // apparently hides the cursor?
                mem_writeb(0x54D, mem_readb(0x54D) | 0x80);
            }
            else if (reg_ch == 0) {
                void pc98_port6A_command_write(unsigned char b);
                pc98_port6A_command_write(0x20);        // disable 256-color
                PC98_show_cursor(false);                // apparently hides the cursor?
                mem_writeb(0x54D, mem_readb(0x54D) & ~0x80);
            }
            else {
                LOG_MSG("PC-98 INT 18h AH=4Dh unknown CH=%02xh",reg_ch);
            }
            break;
        default:
            LOG_MSG("PC-98 INT 18h unknown call AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X",
                reg_ax,
                reg_bx,
                reg_cx,
                reg_dx,
                reg_si,
                reg_di,
                SegValue(ds),
                SegValue(es));
            break;
    }

    /* FIXME: What do actual BIOSes do when faced with an unknown INT 18h call? */
    return CBRET_NONE;
}

#define PC98_FLOPPY_HIGHDENSITY     0x01
#define PC98_FLOPPY_2HEAD           0x02
#define PC98_FLOPPY_RPM_3MODE       0x04
#define PC98_FLOPPY_RPM_IBMPC       0x08

unsigned char PC98_BIOS_FLOPPY_BUFFER[32768]; /* 128 << 8 */

static unsigned int PC98_FDC_SZ_TO_BYTES(unsigned int sz) {
    return 128U << sz;
}

int PC98_BIOS_SCSI_POS(imageDisk *floppy,uint32_t &sector) {
    if (reg_al & 0x80) {
        uint32_t img_heads=0,img_cyl=0,img_sect=0,img_ssz=0;

        floppy->Get_Geometry(&img_heads, &img_cyl, &img_sect, &img_ssz);

        /* DL = sector
         * DH = head
         * CX = track */
        if (reg_dl >= img_sect ||
            reg_dh >= img_heads ||
            reg_cx >= img_cyl) {
            return (reg_ah=0x60);
        }

        sector  = reg_cx;
        sector *= img_heads;
        sector += reg_dh;
        sector *= img_sect;
        sector += reg_dl;

//        LOG_MSG("Sector CHS %u/%u/%u -> %u (geo %u/%u/%u)",reg_cx,reg_dh,reg_dl,sector,img_cyl,img_heads,img_sect);
    }
    else {
        /* Linear LBA addressing */
        sector = ((unsigned int)reg_dl << 16UL) + reg_cx;
        /* TODO: SASI caps at 0x1FFFFF according to NP2 */
    }

    return 0;
}

void PC98_BIOS_SCSI_CALL(void) {
    uint32_t img_heads=0,img_cyl=0,img_sect=0,img_ssz=0;
    uint32_t memaddr,size,ssize;
    imageDisk *floppy;
    unsigned int i;
    uint32_t sector;
    int idx;

#if 0
            LOG_MSG("PC-98 INT 1Bh SCSI BIOS call AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X",
                    reg_ax,
                    reg_bx,
                    reg_cx,
                    reg_dx,
                    reg_si,
                    reg_di,
                    SegValue(ds),
                    SegValue(es));
#endif

    idx = (reg_al & 0xF) + 2;
    if (idx < 0 || idx >= MAX_DISK_IMAGES) {
        CALLBACK_SCF(true);
        reg_ah = 0x00;
        /* TODO? Error code? */
        return;
    }

    floppy = imageDiskList[idx];
    if (floppy == NULL) {
        CALLBACK_SCF(true);
        reg_ah = 0x60;
        return;
    }

    /* FIXME: According to NPKai, command is reg_ah & 0x1F not reg_ah & 0x0F. Right? */

    /* what to do is in the lower 4 bits of AH */
    switch (reg_ah & 0x0F) {
        case 0x05: /* write */
            if (PC98_BIOS_SCSI_POS(floppy,/*&*/sector) == 0) {
                floppy->Get_Geometry(&img_heads, &img_cyl, &img_sect, &img_ssz);
                assert(img_ssz != 0);

                size = reg_bx;
                if (size == 0) size = 0x10000U;
                memaddr = ((unsigned int)SegValue(es) << 4u) + reg_bp;

                reg_ah = 0;
                CALLBACK_SCF(false);

//                LOG_MSG("WRITE memaddr=0x%lx size=0x%x sector=0x%lx ES:BP=%04x:%04X",
//                    (unsigned long)memaddr,(unsigned int)size,(unsigned long)sector,SegValue(es),reg_bp);

                while (size != 0) {
                    ssize = size;
                    if (ssize > img_ssz) ssize = img_ssz;

//                    LOG_MSG(" ... memaddr=0x%lx ssize=0x%x sector=0x%lx",
//                        (unsigned long)memaddr,(unsigned int)ssize,(unsigned long)sector);

                    for (i=0;i < ssize;i++) PC98_BIOS_FLOPPY_BUFFER[i] = mem_readb(memaddr+i);

                    if (floppy->Write_AbsoluteSector(sector,PC98_BIOS_FLOPPY_BUFFER) != 0) {
                        reg_ah = 0xD0;
                        CALLBACK_SCF(true);
                        break;
                    }

                    sector++;
                    size -= ssize;
                    memaddr += ssize;
                }
            }
            else {
                CALLBACK_SCF(true);
            }
            break;
        case 0x06: /* read */
            if (PC98_BIOS_SCSI_POS(floppy,/*&*/sector) == 0) {
                floppy->Get_Geometry(&img_heads, &img_cyl, &img_sect, &img_ssz);
                assert(img_ssz != 0);

                size = reg_bx;
                if (size == 0) size = 0x10000U;
                memaddr = ((unsigned int)SegValue(es) << 4u) + reg_bp;

                reg_ah = 0;
                CALLBACK_SCF(false);

//                LOG_MSG("READ memaddr=0x%lx size=0x%x sector=0x%lx ES:BP=%04x:%04X",
//                    (unsigned long)memaddr,(unsigned int)size,(unsigned long)sector,SegValue(es),reg_bp);

                while (size != 0) {
                    ssize = size;
                    if (ssize > img_ssz) ssize = img_ssz;

//                    LOG_MSG(" ... memaddr=0x%lx ssize=0x%x sector=0x%lx",
//                        (unsigned long)memaddr,(unsigned int)ssize,(unsigned long)sector);

                    if (floppy->Read_AbsoluteSector(sector,PC98_BIOS_FLOPPY_BUFFER) == 0) {
                        for (i=0;i < ssize;i++) mem_writeb(memaddr+i,PC98_BIOS_FLOPPY_BUFFER[i]);
                    }
                    else {
                        reg_ah = 0xD0;
                        CALLBACK_SCF(true);
                        break;
                    }

                    sector++;
                    size -= ssize;
                    memaddr += ssize;
                }
            }
            else {
                CALLBACK_SCF(true);
            }
            break;
        case 0x03: /* according to NPKai source code: "negate ack" (cbus/scsicmd.c line 211, and 61) */
            reg_ah = 0x35;      /* according to scsicmd_negate() line 61, as translated by stat2ret[] by code line 228 */
            CALLBACK_SCF(false);
            // NTS: This is needed for an HDI image to boot that apparently contains FreeDOS98
            break;
        case 0x07: /* unknown, always succeeds */
            reg_ah = 0x00;
            CALLBACK_SCF(false);
            break;
        case 0x0E: /* unknown, always fails */
            reg_ah = 0x40;
            CALLBACK_SCF(true);
            break;
        case 0x04: /* drive status */
            if (reg_ah == 0x84) {
                floppy->Get_Geometry(&img_heads, &img_cyl, &img_sect, &img_ssz);

                reg_dl = img_sect;
                reg_dh = img_heads; /* Max 16 */
                reg_cx = img_cyl;   /* Max 4096 */
                reg_bx = img_ssz;

                reg_ah = 0x00;
                CALLBACK_SCF(false);
                break;
            }
            else if (reg_ah == 0x04 || reg_ah == 0x14) {
                reg_ah = 0x00;
                CALLBACK_SCF(false);
            }
            else {
                goto default_goto;
            }
        default:
        default_goto:
            LOG_MSG("PC-98 INT 1Bh unknown SCSI BIOS call AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X",
                    reg_ax,
                    reg_bx,
                    reg_cx,
                    reg_dx,
                    reg_si,
                    reg_di,
                    SegValue(ds),
                    SegValue(es));
            CALLBACK_SCF(true);
            break;
    }
}

void PC98_BIOS_FDC_CALL_GEO_UNPACK(unsigned int &fdc_cyl,unsigned int &fdc_head,unsigned int &fdc_sect,unsigned int &fdc_sz) {
    fdc_cyl = reg_cl;
    fdc_head = reg_dh;
    fdc_sect = reg_dl;
    fdc_sz = reg_ch;
    if (fdc_sz > 8) fdc_sz = 8;
}

/* NTS: FDC calls reset IRQ 0 timer to a specific fixed interval,
 *      because the real BIOS likely does the same in the act of
 *      controlling the floppy drive.
 *
 *      Resetting the interval is required to prevent Ys II from
 *      crashing after disk swap (divide by zero/overflow) because
 *      Ys II reads the timer after INT 1Bh for whatever reason
 *      and the upper half of the timer byte later affects a divide
 *      by 3 in the code. */

void PC98_Interval_Timer_Continue(void);

bool enable_fdc_timer_hack = false;

void FDC_WAIT_TIMER_HACK(void) {
    unsigned int v;
    unsigned int c=0;

    // Explanation:
    //
    // Originally the FDC code here changed the timer interval back to the stock 100hz
    // normally used in PC-98, to fix Ys II. However that seems to break other booter
    // games that hook IRQ 0 directly and set the timer ONCE, then access the disk.
    //
    // For example, "Angelus" ran WAY too slow with the timer hack because it programs
    // the timer to a 600hz interval and expects it to stay that way.
    //
    // So the new method to satisfy both games is to loop here until the timer
    // count is below the maximum that would occur if the 100hz tick count were
    // still in effect, even if the timer interval was reprogrammed.
    //
    // NTS: Writing port 0x77 to relatch the timer also seems to break games
    //
    // TODO: As a safety against getting stuck, perhaps PIC_FullIndex() should be used
    //       to break out of the loop if this runs for more than 1 second, since that
    //       is a sign the timer is in an odd state that will never terminate this loop.

    v = ~0U;
    c = 10;
    do {
        void CALLBACK_Idle(void);
        CALLBACK_Idle();

        unsigned int pv = v;

        v  = (unsigned int)IO_ReadB(0x71);
        v |= (unsigned int)IO_ReadB(0x71) << 8u;

        if (v > pv) {
            /* if the timer rolled around, we might have missed the narrow window we're watching for */
            if (--c == 0) break;
        }
    } while (v >= 0x60);
}

void PC98_BIOS_FDC_CALL(unsigned int flags) {
    static unsigned int fdc_cyl[2]={0,0},fdc_head[2]={0,0},fdc_sect[2]={0,0},fdc_sz[2]={0,0}; // FIXME: Rename and move out. Making "static" is a hack here.
    uint32_t img_heads=0,img_cyl=0,img_sect=0,img_ssz=0;
    unsigned int drive;
    unsigned int status;
    unsigned int size,accsize,unitsize;
    unsigned long memaddr;
    imageDisk *floppy;

    /* AL bits[1:0] = which floppy drive */
    if ((reg_al & 3) >= 2) {
        /* This emulation only supports up to 2 floppy drives */
        CALLBACK_SCF(true);
        reg_ah = 0x00;
        /* TODO? Error code? */
        return;
    }

    floppy = GetINT13FloppyDrive(drive=(reg_al & 3));

    /* what to do is in the lower 4 bits of AH */
    switch (reg_ah & 0x0F) {
        /* TODO: 0x00 = seek to track (in CL) */
        /* TODO: 0x01 = test read? */
        /* TODO: 0x03 = equipment flags? */
        /* TODO: 0x04 = format detect? */
        /* TODO: 0x05 = write disk */
        /* TODO: 0x07 = recalibrate (seek to track 0) */
        /* TODO: 0x0A = Read ID */
        /* TODO: 0x0D = Format track */
        /* TODO: 0x0E = ?? */
        case 0x03: /* equipment flags update (?) */
            // TODO: Update the disk equipment flags in BDA.
            //       For now, make Alantia happy by returning success.
            reg_ah = 0x00;
            CALLBACK_SCF(false);
            break;
        case 0x00: /* seek */
            /* CL = track */
            if (floppy == NULL) {
                CALLBACK_SCF(true);
                reg_ah = 0x00;
                /* TODO? Error code? */
                return;
            }

            if (enable_fdc_timer_hack) {
                // Hack for Ys II
                FDC_WAIT_TIMER_HACK();
            }

            fdc_cyl[drive] = reg_cl;

            reg_ah = 0x00;
            CALLBACK_SCF(false);
            break;
        case 0x01: /* test read */
            /* AH bits[4:4] = If set, seek to track specified */
            /* CL           = cylinder (track) */
            /* CH           = sector size (0=128 1=256 2=512 3=1024 etc) */
            /* DL           = sector number (1-based) */
            /* DH           = head */
            /* BX           = size (in bytes) of data to read */
            /* ES:BP        = buffer to read data into */
            if (floppy == NULL) {
                CALLBACK_SCF(true);
                reg_ah = 0x00;
                /* TODO? Error code? */
                return;
            }
            floppy->Get_Geometry(&img_heads, &img_cyl, &img_sect, &img_ssz);

            if (enable_fdc_timer_hack) {
                // Hack for Ys II
                FDC_WAIT_TIMER_HACK();
            }

            /* Prevent reading 1.44MB floppies using 1.2MB read commands and vice versa.
             * FIXME: It seems MS-DOS 5.0 booted from a HDI image has trouble understanding
             *        when Drive A: (the first floppy) is a 1.44MB drive or not and fails
             *        because it only attempts it using 1.2MB format read commands. */
            if (flags & PC98_FLOPPY_RPM_IBMPC) {
                if (img_ssz == 1024) { /* reject 1.2MB 3-mode format */
                    CALLBACK_SCF(true);
                    reg_ah = 0x00;
                    /* TODO? Error code? */
                    return;
                }
            }
            else {
                if (img_ssz == 512) { /* reject IBM PC 1.44MB format */
                    CALLBACK_SCF(true);
                    reg_ah = 0x00;
                    /* TODO? Error code? */
                    return;
                }
            }

            PC98_BIOS_FDC_CALL_GEO_UNPACK(/*&*/fdc_cyl[drive],/*&*/fdc_head[drive],/*&*/fdc_sect[drive],/*&*/fdc_sz[drive]);
            unitsize = PC98_FDC_SZ_TO_BYTES(fdc_sz[drive]);
            if (0/*unitsize != img_ssz || img_heads == 0 || img_cyl == 0 || img_sect == 0*/) {
                CALLBACK_SCF(true);
                reg_ah = 0x00;
                /* TODO? Error code? */
                return;
            }

            size = reg_bx;
            while (size > 0) {
                accsize = size > unitsize ? unitsize : size;

                if (floppy->Read_Sector(fdc_head[drive],fdc_cyl[drive],fdc_sect[drive],PC98_BIOS_FLOPPY_BUFFER,unitsize) != 0) {
                    CALLBACK_SCF(true);
                    reg_ah = 0x00;
                    /* TODO? Error code? */
                    return;
                }

                size -= accsize;

                if (size == 0) break;

                if ((++fdc_sect[drive]) > img_sect && img_sect != 0) {
                    fdc_sect[drive] = 1;
                    if ((++fdc_head[drive]) >= img_heads && img_heads != 0) {
                        fdc_head[drive] = 0;
                        fdc_cyl[drive]++;
                    }
                }
            }

            reg_ah = 0x00;
            CALLBACK_SCF(false);
            break;
        case 0x02: /* read sectors */
        case 0x06: /* read sectors (what's the difference from 0x02?) */
            /* AH bits[4:4] = If set, seek to track specified */
            /* CL           = cylinder (track) */
            /* CH           = sector size (0=128 1=256 2=512 3=1024 etc) */
            /* DL           = sector number (1-based) */
            /* DH           = head */
            /* BX           = size (in bytes) of data to read */
            /* ES:BP        = buffer to read data into */
            if (floppy == NULL) {
                CALLBACK_SCF(true);
                reg_ah = 0x00;
                /* TODO? Error code? */
                return;
            }
            floppy->Get_Geometry(&img_heads, &img_cyl, &img_sect, &img_ssz);

            if (enable_fdc_timer_hack) {
                // Hack for Ys II
                FDC_WAIT_TIMER_HACK();
            }

            /* Prevent reading 1.44MB floppies using 1.2MB read commands and vice versa.
             * FIXME: It seems MS-DOS 5.0 booted from a HDI image has trouble understanding
             *        when Drive A: (the first floppy) is a 1.44MB drive or not and fails
             *        because it only attempts it using 1.2MB format read commands. */
            if (flags & PC98_FLOPPY_RPM_IBMPC) {
                if (img_ssz == 1024) { /* reject 1.2MB 3-mode format */
                    CALLBACK_SCF(true);
                    reg_ah = 0x00;
                    /* TODO? Error code? */
                    return;
                }
            }
            else {
                if (img_ssz == 512) { /* reject IBM PC 1.44MB format */
                    CALLBACK_SCF(true);
                    reg_ah = 0x00;
                    /* TODO? Error code? */
                    return;
                }
            }

            PC98_BIOS_FDC_CALL_GEO_UNPACK(/*&*/fdc_cyl[drive],/*&*/fdc_head[drive],/*&*/fdc_sect[drive],/*&*/fdc_sz[drive]);
            unitsize = PC98_FDC_SZ_TO_BYTES(fdc_sz[drive]);
            if (0/*unitsize != img_ssz || img_heads == 0 || img_cyl == 0 || img_sect == 0*/) {
                CALLBACK_SCF(true);
                reg_ah = 0x00;
                /* TODO? Error code? */
                return;
            }

            size = reg_bx;
            memaddr = ((unsigned int)SegValue(es) << 4U) + reg_bp;
            while (size > 0) {
                accsize = size > unitsize ? unitsize : size;

                if (floppy->Read_Sector(fdc_head[drive],fdc_cyl[drive],fdc_sect[drive],PC98_BIOS_FLOPPY_BUFFER,unitsize) != 0) {
                    CALLBACK_SCF(true);
                    reg_ah = 0x00;
                    /* TODO? Error code? */
                    return;
                }

                for (unsigned int i=0;i < accsize;i++)
                    mem_writeb(memaddr+i,PC98_BIOS_FLOPPY_BUFFER[i]);

                memaddr += accsize;
                size -= accsize;

                if (size == 0) break;

                if ((++fdc_sect[drive]) > img_sect && img_sect != 0) {
                    fdc_sect[drive] = 1;
                    if ((++fdc_head[drive]) >= img_heads && img_heads != 0) {
                        fdc_head[drive] = 0;
                        fdc_cyl[drive]++;
                    }
                }
            }

            /* need to clear DMA terminal count after read as BIOS would, I assume (Arsys Star Cruiser) */
            {
                DmaChannel *dma = GetDMAChannel(2);
                if (dma) dma->tcount = false;
            }

            reg_ah = 0x00;
            CALLBACK_SCF(false);
            break;
        case 0x04: /* drive status */
            status = 0;

            /* TODO: bit 4 is set if write protected */

            if (reg_al & 0x80) { /* high density */
                status |= 0x01;
            }
            else { /* double density */
                /* TODO: */
                status |= 0x01;
            }

            if ((reg_ax & 0x8F40) == 0x8400) {
                status |= 8;        /* 1MB/640KB format, spindle speed for 3-mode */
                if (reg_ah & 0x40) /* DOSBox-X always supports 1.44MB */
                    status |= 4;    /* 1.44MB format, spindle speed for IBM PC format */
            }

            if (floppy == NULL)
                status |= 0xC0;

            reg_ah = status;
            CALLBACK_SCF(false);
            break;
        /* TODO: 0x00 = seek to track (in CL) */
        /* TODO: 0x01 = test read? */
        /* TODO: 0x03 = equipment flags? */
        /* TODO: 0x04 = format detect? */
        /* TODO: 0x05 = write disk */
        /* TODO: 0x07 = recalibrate (seek to track 0) */
        /* TODO: 0x0A = Read ID */
        /* TODO: 0x0D = Format track */
        /* TODO: 0x0E = ?? */
        case 0x05: /* write sectors */
            /* AH bits[4:4] = If set, seek to track specified */
            /* CL           = cylinder (track) */
            /* CH           = sector size (0=128 1=256 2=512 3=1024 etc) */
            /* DL           = sector number (1-based) */
            /* DH           = head */
            /* BX           = size (in bytes) of data to read */
            /* ES:BP        = buffer to write data from */
            if (floppy == NULL) {
                CALLBACK_SCF(true);
                reg_ah = 0x00;
                /* TODO? Error code? */
                return;
            }
            floppy->Get_Geometry(&img_heads, &img_cyl, &img_sect, &img_ssz);

            if (enable_fdc_timer_hack) {
                // Hack for Ys II
                FDC_WAIT_TIMER_HACK();
            }

            /* TODO: Error if write protected */

            PC98_BIOS_FDC_CALL_GEO_UNPACK(/*&*/fdc_cyl[drive],/*&*/fdc_head[drive],/*&*/fdc_sect[drive],/*&*/fdc_sz[drive]);
            unitsize = PC98_FDC_SZ_TO_BYTES(fdc_sz[drive]);
            if (0/*unitsize != img_ssz || img_heads == 0 || img_cyl == 0 || img_sect == 0*/) {
                CALLBACK_SCF(true);
                reg_ah = 0x00;
                /* TODO? Error code? */
                return;
            }

            size = reg_bx;
            memaddr = ((unsigned int)SegValue(es) << 4U) + reg_bp;
            while (size > 0) {
                accsize = size > unitsize ? unitsize : size;

                for (unsigned int i=0;i < accsize;i++)
                    PC98_BIOS_FLOPPY_BUFFER[i] = mem_readb(memaddr+i);

                if (floppy->Write_Sector(fdc_head[drive],fdc_cyl[drive],fdc_sect[drive],PC98_BIOS_FLOPPY_BUFFER,unitsize) != 0) {
                    CALLBACK_SCF(true);
                    reg_ah = 0x00;
                    /* TODO? Error code? */
                    return;
                }

                memaddr += accsize;
                size -= accsize;

                if (size == 0) break;

                if ((++fdc_sect[drive]) > img_sect && img_sect != 0) {
                    fdc_sect[drive] = 1;
                    if ((++fdc_head[drive]) >= img_heads && img_heads != 0) {
                        fdc_head[drive] = 0;
                        fdc_cyl[drive]++;
                    }
                }
            }

            reg_ah = 0x00;
            CALLBACK_SCF(false);
            break;
        case 0x07: /* recalibrate (seek to track 0) */
            if (floppy == NULL) {
                CALLBACK_SCF(true);
                reg_ah = 0x00;
                /* TODO? Error code? */
                return;
            }

            if (enable_fdc_timer_hack) {
                // Hack for Ys II
                FDC_WAIT_TIMER_HACK();
            }

            fdc_cyl[drive] = 0;

            reg_ah = 0x00;
            CALLBACK_SCF(false);
            break;
        case 0x0D: /* format track */
            if (floppy == NULL) {
                CALLBACK_SCF(true);
                reg_ah = 0x00;
                /* TODO? Error code? */
                return;
            }

            PC98_BIOS_FDC_CALL_GEO_UNPACK(/*&*/fdc_cyl[drive],/*&*/fdc_head[drive],/*&*/fdc_sect[drive],/*&*/fdc_sz[drive]);
            unitsize = PC98_FDC_SZ_TO_BYTES(fdc_sz[drive]);

            if (enable_fdc_timer_hack) {
                // Hack for Ys II
                FDC_WAIT_TIMER_HACK();
            }

            LOG_MSG("WARNING: INT 1Bh FDC format track command not implemented. Formatting is faked, for now on C/H/S/sz %u/%u/%u/%u drive %c.",
                (unsigned int)fdc_cyl[drive],
                (unsigned int)fdc_head[drive],
                (unsigned int)fdc_sect[drive],
                (unsigned int)unitsize,
                drive + 'A');

            reg_ah = 0x00;
            CALLBACK_SCF(false);
            break;
        case 0x0A: /* read ID */
            /* NTS: PC-98 "MEGDOS" used by some games seems to rely heavily on this call to
             *      verify the floppy head is where it thinks it should be! */
            if (floppy == NULL) {
                CALLBACK_SCF(true);
                reg_ah = 0x00;
                /* TODO? Error code? */
                return;
            }

            floppy->Get_Geometry(&img_heads, &img_cyl, &img_sect, &img_ssz);

            if (enable_fdc_timer_hack) {
                // Hack for Ys II
                FDC_WAIT_TIMER_HACK();
            }

            if (reg_ah & 0x10) { // seek to track number in CL
                if (img_cyl != 0 && reg_cl >= img_cyl) {
                    CALLBACK_SCF(true);
                    reg_ah = 0x00;
                    /* TODO? Error code? */
                    return;
                }

                fdc_cyl[drive] = reg_cl;
            }

            if (fdc_sect[drive] == 0)
                fdc_sect[drive] = 1;

            if (img_ssz >= 1024)
                fdc_sz[drive] = 3;
            else if (img_ssz >= 512)
                fdc_sz[drive] = 2;
            else if (img_ssz >= 256)
                fdc_sz[drive] = 1;
            else
                fdc_sz[drive] = 0;

            reg_cl = fdc_cyl[drive];
            reg_dh = fdc_head[drive];
            reg_dl = fdc_sect[drive];
            /* ^ FIXME: A more realistic emulation would return a random number from 1 to N
             *          where N=sectors/track because the floppy motor is running and tracks
             *          are moving past the head. */
            reg_ch = fdc_sz[drive];

            /* per read ID call, increment the sector through the range on disk.
             * This is REQUIRED or else MEGDOS will not attempt to read this disk. */
            if (img_sect != 0) {
                if ((++fdc_sect[drive]) > img_sect)
                    fdc_sect[drive] = 1;
            }

            reg_ah = 0x00;
            CALLBACK_SCF(false);
            break;
        default:
            LOG_MSG("PC-98 INT 1Bh unknown FDC BIOS call AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X",
                    reg_ax,
                    reg_bx,
                    reg_cx,
                    reg_dx,
                    reg_si,
                    reg_di,
                    SegValue(ds),
                    SegValue(es));
            CALLBACK_SCF(true);
            break;
    }
}

static Bitu INT19_PC98_Handler(void) {
    LOG_MSG("PC-98 INT 19h unknown call AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X",
        reg_ax,
        reg_bx,
        reg_cx,
        reg_dx,
        reg_si,
        reg_di,
        SegValue(ds),
        SegValue(es));

    return CBRET_NONE;
}

static Bitu INT1A_PC98_Handler(void) {
    /* HACK: This makes the "test" program in DOSLIB work.
     *       We'll remove this when we implement INT 1Ah */
    if (reg_ax == 0x1000) {
        CALLBACK_SCF(false);
        reg_ax = 0;
    }

    LOG_MSG("PC-98 INT 1Ah unknown call AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X",
        reg_ax,
        reg_bx,
        reg_cx,
        reg_dx,
        reg_si,
        reg_di,
        SegValue(ds),
        SegValue(es));

    return CBRET_NONE;
}

static Bitu INT1B_PC98_Handler(void) {
    /* As BIOS interfaces for disk I/O go, this is fairly unusual */
    switch (reg_al & 0xF0) {
        /* floppy disk access */
        /* AL bits[1:0] = floppy drive number */
        /* Uses INT42 if high density, INT41 if double density */
        /* AH bits[3:0] = command */
        case 0x90: /* 1.2MB HD */
            PC98_BIOS_FDC_CALL(PC98_FLOPPY_HIGHDENSITY|PC98_FLOPPY_2HEAD|PC98_FLOPPY_RPM_3MODE);
            break;
        case 0x30: /* 1.44MB HD (NTS: not supported until the early 1990s) */
        case 0xB0:
            PC98_BIOS_FDC_CALL(PC98_FLOPPY_HIGHDENSITY|PC98_FLOPPY_2HEAD|PC98_FLOPPY_RPM_IBMPC);
            break;
        case 0x70: /* 720KB DD (??) */
        case 0xF0:
            PC98_BIOS_FDC_CALL(PC98_FLOPPY_2HEAD|PC98_FLOPPY_RPM_3MODE); // FIXME, correct??
            break;
        case 0x20: /* SCSI hard disk BIOS */
        case 0xA0: /* SCSI hard disk BIOS */
        case 0x00: /* SASI hard disk BIOS */
        case 0x80: /* SASI hard disk BIOS */
            PC98_BIOS_SCSI_CALL();
            break;
        /* TODO: Other disk formats */
        /* TODO: Future SASI/SCSI BIOS emulation for hard disk images */
        default:
            LOG_MSG("PC-98 INT 1Bh unknown call AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X",
                    reg_ax,
                    reg_bx,
                    reg_cx,
                    reg_dx,
                    reg_si,
                    reg_di,
                    SegValue(ds),
                    SegValue(es));
            CALLBACK_SCF(true);
            break;
    }

    return CBRET_NONE;
}

void PC98_Interval_Timer_Continue(void) {
    /* assume: interrupts are disabled */
    IO_WriteB(0x71,0x00);
    // TODO: What time interval is this supposed to be?
    if (PIT_TICK_RATE == PIT_TICK_RATE_PC98_8MHZ)
        IO_WriteB(0x71,0x4E);
    else
        IO_WriteB(0x71,0x60);

    IO_WriteB(0x02,IO_ReadB(0x02) & (~(1u << /*IRQ*/0u))); // unmask IRQ0
}

unsigned char pc98_dec2bcd(unsigned char c) {
    return ((c / 10u) << 4u) + (c % 10u);
}

static Bitu INT1C_PC98_Handler(void) {
    if (reg_ah == 0x00) { /* get time and date */
        time_t curtime;
        const struct tm *loctime;
        curtime = time (NULL);
        loctime = localtime (&curtime);

        unsigned char tmp[6];

        tmp[0] = pc98_dec2bcd((unsigned int)loctime->tm_year % 100u);
        tmp[1] = (((unsigned int)loctime->tm_mon + 1u) << 4u) + (unsigned int)loctime->tm_wday;
        tmp[2] = pc98_dec2bcd(loctime->tm_mday);
        tmp[3] = pc98_dec2bcd(loctime->tm_hour);
        tmp[4] = pc98_dec2bcd(loctime->tm_min);
        tmp[5] = pc98_dec2bcd(loctime->tm_sec);

        unsigned long mem = ((unsigned int)SegValue(es) << 4u) + reg_bx;

        for (unsigned int i=0;i < 6;i++)
            mem_writeb(mem+i,tmp[i]);
    }
    else if (reg_ah == 0x02) { /* set interval timer (single event) */
        /* es:bx = interrupt handler to execute
         * cx = timer interval in ticks (FIXME: what units of time?) */
        mem_writew(0x1C,reg_bx);
        mem_writew(0x1E,SegValue(es));
        mem_writew(0x58A,reg_cx);

        IO_WriteB(0x77,0x36);   /* mode 3, binary, low-byte high-byte 16-bit counter */

        PC98_Interval_Timer_Continue();
    }
    else if (reg_ah == 0x03) { /* continue interval timer */
        PC98_Interval_Timer_Continue();
    }
    /* TODO: According to the PDF at
     *
     * http://hackipedia.org/browse.cgi/Computer/Platform/PC%2c%20NEC%20PC%2d98/Collections/PC%2d9801%20Bible%20%e6%9d%b1%e4%ba%ac%e7%90%86%e7%a7%91%e5%a4%a7%e5%ad%a6EIC%20%281994%29%2epdf
     *
     * There are additional functions
     *
     *  AH = 04h
     *  ES:BX = ?
     *
     *  ---
     *
     *  AH = 05h
     *  ES:BX = ?
     *
     *  ---
     *
     *  AH = 06h
     *  CX = ? (1-FFFFh)
     *  DX = ? (20h-8000h Hz)
     *
     * If any PC-98 games or applications rely on this, let me know. Adding a case for them is easy enough if anyone is interested. --J.C.
     */
    else {
        LOG_MSG("PC-98 INT 1Ch unknown call AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X",
                reg_ax,
                reg_bx,
                reg_cx,
                reg_dx,
                reg_si,
                reg_di,
                SegValue(ds),
                SegValue(es));
    }

    return CBRET_NONE;
}

// NTS: According to this PDF, chapter 5, INT 1Dh has additional functions on "High Resolution" PC-98 systems.
//      [https://ia801305.us.archive.org/8/items/PC9800TechnicalDataBookBIOS1992/PC-9800TechnicalDataBook_BIOS_1992_text.pdf]
static Bitu INT1D_PC98_Handler(void) {
    LOG_MSG("PC-98 INT 1Dh unknown call AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X",
        reg_ax,
        reg_bx,
        reg_cx,
        reg_dx,
        reg_si,
        reg_di,
        SegValue(ds),
        SegValue(es));

    return CBRET_NONE;
}

static Bitu INT1E_PC98_Handler(void) {
    LOG_MSG("PC-98 INT 1Eh unknown call AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X",
        reg_ax,
        reg_bx,
        reg_cx,
        reg_dx,
        reg_si,
        reg_di,
        SegValue(ds),
        SegValue(es));

    return CBRET_NONE;
}

void PC98_EXTMEMCPY(void) {
    bool enabled = MEM_A20_Enabled();
    MEM_A20_Enable(true);

    Bitu   bytes    = ((reg_cx - 1u) & 0xFFFFu) + 1u; // bytes, except that 0 == 64KB
    PhysPt data     = SegPhys(es)+reg_bx;
    PhysPt source = (mem_readd(data + 0x12u) & 0x00FFFFFFu) + ((unsigned int)mem_readb(data + 0x17u) << 24u) + reg_si;
    PhysPt dest = (mem_readd(data + 0x1Au) & 0x00FFFFFFu) + ((unsigned int)mem_readb(data + 0x1Fu) << 24u) + reg_di;

    LOG_MSG("PC-98 memcpy: src=0x%x dst=0x%x data=0x%x count=0x%x",
        (unsigned int)source,(unsigned int)dest,(unsigned int)data,(unsigned int)bytes);

    MEM_BlockCopy(dest,source,bytes);
    MEM_A20_Enable(enabled);
    Segs.limit[cs] = 0xFFFF;
    Segs.limit[ds] = 0xFFFF;
    Segs.limit[es] = 0xFFFF;
    Segs.limit[ss] = 0xFFFF;

    CALLBACK_SCF(false);
}

static Bitu INT1F_PC98_Handler(void) {
    switch (reg_ah) {
        case 0x90:
            /* Copy extended memory */
            PC98_EXTMEMCPY();
            break;
        default:
            LOG_MSG("PC-98 INT 1Fh unknown call AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X",
                    reg_ax,
                    reg_bx,
                    reg_cx,
                    reg_dx,
                    reg_si,
                    reg_di,
                    SegValue(ds),
                    SegValue(es));
            CALLBACK_SCF(true);
            break;
    }

    return CBRET_NONE;
}

static Bitu INTGEN_PC98_Handler(void) {
    LOG_MSG("PC-98 INT stub unknown call AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X",
        reg_ax,
        reg_bx,
        reg_cx,
        reg_dx,
        reg_si,
        reg_di,
        SegValue(ds),
        SegValue(es));

    return CBRET_NONE;
}

/* This interrupt should only exist while the DOS kernel is active.
 * On actual PC-98 MS-DOS this is a direct interface to MS-DOS's built-in ANSI CON driver.
 *
 * CL = major function call number
 * AH = minor function call number
 * DX = data?? */

void PC98_INTDC_WriteChar(unsigned char b);

void INTDC_LOAD_FUNCDEC(pc98_func_key_shortcut_def &def,const Bitu ofs) {
    unsigned int i;

    for (i=0;i < 0x0F;i++)
        def.shortcut[i] = mem_readb(ofs+0x0+i);

    for (i=0;i < 0x0F && def.shortcut[i] != 0;) i++;
    def.length = i;
}

void INTDC_STORE_FUNCDEC(const Bitu ofs,const pc98_func_key_shortcut_def &def) {
    for (unsigned int i=0;i < 0x0F;i++) mem_writeb(ofs+0x0+i,def.shortcut[i]);
    mem_writeb(ofs+0xF,0);
}

void INTDC_LOAD_EDITDEC(pc98_func_key_shortcut_def &def,const Bitu ofs) {
    unsigned int i;

    for (i=0;i < 0x05;i++)
        def.shortcut[i] = mem_readb(ofs+0x0+i);

    for (i=0;i < 0x05 && def.shortcut[i] != 0;) i++;
    def.length = i;
}

void INTDC_STORE_EDITDEC(const Bitu ofs,const pc98_func_key_shortcut_def &def) {
    for (unsigned int i=0;i < 0x05;i++) mem_writeb(ofs+0x0+i,def.shortcut[i]);
    mem_writeb(ofs+0x5,0);
}

bool inhibited_ControlFn(void) {
    return real_readb(0x60,0x10C) & 0x01;
}

static const char *fneditkeys[11] = {
    "ROLLUP",
    "ROLLDOWN",
    "INS",
    "DEL",
    "UPARROW",
    "LEFTARROW",
    "RIGHTARROW",
    "DOWNARROW",
    "HOMECLR",
    "HELP",
    "KEYPAD-"
};

void DEBUG_INTDC_FnKeyMapInfo(void) {
    if (!IS_PC98_ARCH) {
        DEBUG_ShowMsg("INT DCh has no meaning except in PC-98 mode");
    }
    else if (dos_kernel_disabled) {
        DEBUG_ShowMsg("INT DCh FnKey mapping has no meaning outside the DOS environment");
    }
    else {
        DEBUG_ShowMsg("INT DCh FnKey mapping. Ctrl+Fn builtin inhibited=%s",inhibited_ControlFn()?"yes":"no");
        for (unsigned int i=0;i < 10;i++)
            DEBUG_ShowMsg("  F%u: %s",i+1,pc98_func_key[i].debugToString().c_str());
        for (unsigned int i=0;i < 5;i++)
            DEBUG_ShowMsg("  VF%u: %s",i+1,pc98_vfunc_key[i].debugToString().c_str());

        for (unsigned int i=0;i < 10;i++)
            DEBUG_ShowMsg("  Shift+F%u: %s",i+1,pc98_func_key_shortcut[i].debugToString().c_str());
        for (unsigned int i=0;i < 5;i++)
            DEBUG_ShowMsg("  Shift+VF%u: %s",i+1,pc98_vfunc_key_shortcut[i].debugToString().c_str());

        for (unsigned int i=0;i < 10;i++)
            DEBUG_ShowMsg("  Control+F%u: %s",i+1,pc98_func_key_ctrl[i].debugToString().c_str());
        for (unsigned int i=0;i < 5;i++)
            DEBUG_ShowMsg("  Control+VF%u: %s",i+1,pc98_vfunc_key_ctrl[i].debugToString().c_str());

        for (unsigned int i=0;i < 11;i++)
            DEBUG_ShowMsg("  %s: %s",fneditkeys[i],pc98_editor_key_escapes[i].debugToString().c_str());
    }
}

/* PC-98 application notes, that are NOT DOSBox-X bugs because they occur on real MS-DOS as well:
 *
 * VZ.COM - If the function key row was hidden when VZ.COM is started, VZ.COM will not restore the
 *          function key row. VZ.COM's function key shortcuts affect Fn and Shift+Fn keys and the
 *          text they display even if VZ.COM also disables the Ctrl+F7 shortcut that lets you
 *          toggle the function key row, which makes displaying the Shift+Fn key shortcuts impossible
 *          unless the function key row was left showing that at startup.
 */

static Bitu INTDC_PC98_Handler(void) {
    if (dos_kernel_disabled) goto unknown;

    switch (reg_cl) {
        /* Tracking implementation according to [http://hackipedia.org/browse.cgi/Computer/Platform/PC%2c%20NEC%20PC%2d98/Collections/Undocumented%209801%2c%209821%20Volume%201%20English%20translation/INTDC%2eTXT] */
        case 0x0C: /* CL=0x0C General entry point to read function key state */
/*===================================================================================================
Table            [List of key specification values and corresponding keys]
                 ------------------------+---------------------------------------------------
                 Key specification value | Corresponding key
                 ------------------------+---------------------------------------------------
                 0000h                   | [f･1] to [f･10], [SHIFT] + [f･1] to [SHIFT] + [f･10],
                                         | [ROLL UP], [ROLL DOWN], [INS], [DEL], [↑], [←], [→], [↓],
                                         | [HOME/CLR], [HELP], [SHIFT] + [HOME/CLR]
                 0001 to 000Ah           | [f･1] to [f･10]
                 000B to 0014h           | [SHIFT] + [f･1] to [SHIFT] + [f･10]
                 0015h                   | [ROLL UP]
                 0016h                   | [ROLL DOWN]
                 0017h                   | [INS]
                 0018h                   | [DEL]
                 0019h                   | [↑]
                 001Ah                   | [←]
                 001Bh                   | [→]
                 001Ch                   | [↓]
                 001Dh                   | [HOME/CLR](XA keyboard:[CLR])
                 001Eh                   | [HELP]
                 001Fh                   | [SHIFT]+[HOME/CLR](XA keyboard:[HOME])
                 0020-0024h              | [vf･1]-[vf･5]
                 0025-0029h              | [SHIFT]+[vf･1]-[vf･5]
                 002A-0033h              | [CTRL]+[f･1]-[f･10]
                 0034-0038h              | [CTRL]+[vf･1]-[vf･5]
                 0039h                   | [CTRL]+[XFER]/[NFER] (Undocumented)
                 003Ah                   | [CTRL]+[XFER]/[NFER],[CTRL]+[f･1]～[f･10]
                                         | (Undocumented)
                 00FFh                   | [f･1]〜[f･10],[vf･1]〜[vf･5],
                                         | [SHIFT]+[f･1]〜[SHIFT]+[f･10],[SHIFT]+[vf･1]〜[vf･5],
                                         | [ROLL UP],[ROLL DOWN],[INS],[DEL],[↑],[←],[→],[↓],
                                         | [HOME/CLR],[HELP],[SHIFT]+[HOME/CLR],
                                         | [CTRL]+[f･1] to [f･10], [CTRL]+[vf･1] to [vf･5]
                 ------------------------+---------------------------------------------------

                 Table [Supported range of key specification values for each MS-DOS version]
                 ------------------------+---+---+---+---+---+---+---+---+-----+
                 Key specification value | MS-DOS version (PS98-XXX)
                                         |111|121|122|123|125|127|129|011|XA125
                 ------------------------+---+---+---+---+---+---+---+---+-----+
                 0000 to 001Fh           | ○ | ○ | ○ | ○ | ○ | ○ | ○ | ○ | ○
                 0020 to 0029h           | × | × | × | × | × | × | × | ○ | ○
                 002A-0033h              | × | × | × | × | ○ | ○ | ○ | ○ | △
                 0034-0038h              | × | × | × | × | × | × | × | ○ | △
                 0039h                   | × | × | × | × | × | ○ | ○ | ○ | ×
                 003Ah                   | × | × | × | × | ○ | ○ | ○ | ○ | ×
                 00FFh                   | × | × | × | × | × | × | × | ○ | ○
                 ------------------------+---+---+---+---+---+---+---+---+-----+
                 * PC-98LT/HA is the same as PS98-127.
                 * MS-DOS 3.3(all), 5.0, 5.0A is the same as PS98-011.
                 * For the PS98-XA125 triangle mark, the keys are as follows.
                   Key values 002B to 0033h specify [CTRL]+[f･1] to [f･9].
                   Key values 0035 to 0038h specify [CTRL]+[vf･1] to [vf･4].
===============================================================================================
                 * NTS: According to a translation table in the MS-DOS kernel, where
                 *      AX=1h to AX=29h inclusive look up from this 0x29-element table:
                 *
                 *      Table starts with AX=1h, ends with AX=29h
                 *
                 *                    01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F 10
                 *                     |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |
                 *      0ADC:00003DE0 01 02 03 04 05 06 07 08 09 0A 10 11 12 13 14 15  ................
                 *      0ADC:00003DF0 16 17 18 19 1F 20 21 22 23 24 25 26 27 28 29 0B  ..... !"#$%&'().
                 *      0ADC:00003E00 0C 0D 0E 0F 1A 1B 1C 1D 1E|
                 *
                 *      The table is read, then the byte is decremented by one.
                 *
                 *      If the result of that is less than 0x1E, it's an index into
                 *      the 16 byte/entry Fn key table.
                 *
                 *      If the result is 0x1E or larger, then (result - 0x1E) is an
                 *      index into the editor table, 8 bytes/entry.
                 *
                 *      Meanings:
                 *
                 *                    01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F 10
                 *                     |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |
                 *      0ADC:00003DE0 01 02 03 04 05 06 07 08 09 0A 10 11 12 13 14 15  ................
                 *                   | --- Function keys F1-F10 ---| Fn shift F1-F6 -
                 *      0ADC:00003DF0 16 17 18 19 1F 20 21 22 23 24 25 26 27 28 29 0B  ..... !"#$%&'().
                 *                   | Sh F7-F10 | ------- EDITOR KEYS -----------| -
                 *      0ADC:00003E00 0C 0D 0E 0F 1A 1B 1C 1D 1E|
                 *                   | --------- | ------------ |
===============================================================================================*/
            if (reg_ax == 0x00) { /* Read all state, DS:DX = data to store to */
/*=============================================================================================
                 DS:DX contains

Table            [Programmable key setting data buffer structure]
                 (1) Key specification value 0000h
                 -------+--------------------+--------------------------------------
                 Offset | Key type           | Size of key setting data
                 -------+--------------------+--------------------------------------
                 +0000h | [f･1]              | 16 bytes (15 bytes of key setting data + 00h)
                 +0010h | [f･2]              | 16 bytes (15 bytes of key setting data + 00h)
                 +0020h | [f･3]              | 16 bytes (15 bytes of key setting data + 00h)
                 +0030h | [f･4]              | 16 bytes (15 bytes of key setting data + 00h)
                 +0040h | [f･5]              | 16 bytes (15 bytes of key setting data + 00h)
                 +0050h | [f･6]              | 16 bytes (15 bytes of key setting data + 00h)
                 +0060h | [f･7]              | 16 bytes (15 bytes of key setting data + 00h)
                 +0070h | [f･8]              | 16 bytes (15 bytes of key setting data + 00h)
                 +0080h | [f･9]              | 16 bytes (15 bytes of key setting data + 00h)
                 +0090h | [f･10]             | 16 bytes (15 bytes of key setting data + 00h)
                 +00A0h | [SHIFT]+[f･1]      | 16 bytes (15 bytes of key setting data + 00h)
                 +00B0h | [SHIFT]+[f･2]      | 16 bytes (15 bytes of key setting data + 00h)
                 +00C0h | [SHIFT]+[f･3]      | 16 bytes (15 bytes of key setting data + 00h)
                 +00D0h | [SHIFT]+[f･4]      | 16 bytes (15 bytes of key setting data + 00h)
                 +00E0h | [SHIFT]+[f･5]      | 16 bytes (15 bytes of key setting data + 00h)
                 +00F0h | [SHIFT]+[f･6]      | 16 bytes (15 bytes of key setting data + 00h)
                 +0100h | [SHIFT]+[f･7]      | 16 bytes (15 bytes of key setting data + 00h)
                 +0110h | [SHIFT]+[f･8]      | 16 bytes (15 bytes of key setting data + 00h)
                 +0120h | [SHIFT]+[f･9]      | 16 bytes (15 bytes of key setting data + 00h)
                 +0130h | [SHIFT]+[f･10]     | 16 bytes (15 bytes of key setting data + 00h)
                 +0140h | [ROLL UP]          | 6 bytes (5 bytes of key setting data + 00h)
                 +0146h | [ROLL DOWN]        | 6 bytes (5 bytes of key setting data + 00h)
                 +014Ch | [INS]              | 6 bytes (5 bytes of key setting data + 00h)
                 +0152h | [DEL]              | 6 bytes (5 bytes of key setting data + 00h)
                 +0158h | [↑]                | 6 bytes (5 bytes of key setting data + 00h)
                 +015Eh | [←]                | 6 bytes (5 bytes of key setting data + 00h)
                 +0164h | [→]                | 6 bytes (5 bytes of key setting data + 00h)
                 +016Ah | [↓]                | 6 bytes (5 bytes of key setting data + 00h)
                 +0170h | [HOME/CLR]         | 6 bytes (5 bytes of key setting data + 00h)
                 +0176h | [HELP]             | 6 bytes (5 bytes of key setting data + 00h)
                 +017Ch | [SHIFT]+[HOME/CLR] | 6 bytes (5 bytes of key setting data + 00h)
                 -------+--------------------+--------------------------------------
===============================================================================================*/
                Bitu ofs = (Bitu)(SegValue(ds) << 4ul) + (Bitu)reg_dx;

                /* function keys F1-F10 */
                for (unsigned int f=0;f < 10;f++,ofs += 16)
                    INTDC_STORE_FUNCDEC(ofs,pc98_func_key[f]);
                /* function keys Shift+F1 - Shift+F10 */
                for (unsigned int f=0;f < 10;f++,ofs += 16)
                    INTDC_STORE_FUNCDEC(ofs,pc98_func_key_shortcut[f]);
                /* editor keys */
                for (unsigned int f=0;f < 11;f++,ofs += 6)
                    INTDC_STORE_EDITDEC(ofs,pc98_editor_key_escapes[f]);

                goto done;
            }
            else if (reg_ax >= 0x01 && reg_ax <= 0x0A) { /* Read individual function keys, DS:DX = data to store to */
                Bitu ofs = (Bitu)(SegValue(ds) << 4ul) + (Bitu)reg_dx;
                INTDC_STORE_FUNCDEC(ofs,pc98_func_key[reg_ax - 0x01]);
                goto done;
            }
            else if (reg_ax >= 0x0B && reg_ax <= 0x14) { /* Read individual shift + function keys, DS:DX = data to store to */
                Bitu ofs = (Bitu)(SegValue(ds) << 4ul) + (Bitu)reg_dx;
                INTDC_STORE_FUNCDEC(ofs,pc98_func_key_shortcut[reg_ax - 0x0B]);
                goto done;
            }
            else if (reg_ax >= 0x15 && reg_ax <= 0x1F) { /* Read individual editor keys, DS:DX = data to store to */
                Bitu ofs = (Bitu)(SegValue(ds) << 4ul) + (Bitu)reg_dx;
                INTDC_STORE_EDITDEC(ofs,pc98_editor_key_escapes[reg_ax - 0x15]);
                goto done;
            }
            else if (reg_ax >= 0x20 && reg_ax <= 0x24) { /* Read VF1-VF5 keys, DS:DX = data to store to */
                Bitu ofs = (Bitu)(SegValue(ds) << 4ul) + (Bitu)reg_dx;
                INTDC_STORE_FUNCDEC(ofs,pc98_vfunc_key[reg_ax - 0x20]);
                goto done;
            }
            else if (reg_ax >= 0x25 && reg_ax <= 0x29) { /* Read shift VF1-VF5 keys, DS:DX = data to store to */
                Bitu ofs = (Bitu)(SegValue(ds) << 4ul) + (Bitu)reg_dx;
                INTDC_STORE_FUNCDEC(ofs,pc98_vfunc_key_shortcut[reg_ax - 0x25]);
                goto done;
            }
            else if (reg_ax >= 0x2A && reg_ax <= 0x33) { /* Read individual function keys, DS:DX = data to store to */
                Bitu ofs = (Bitu)(SegValue(ds) << 4ul) + (Bitu)reg_dx;
                INTDC_STORE_FUNCDEC(ofs,pc98_func_key_ctrl[reg_ax - 0x2A]);
                goto done;
            }
            else if (reg_ax >= 0x34 && reg_ax <= 0x38) { /* Read control VF1-VF5 keys, DS:DX = data to store to */
                Bitu ofs = (Bitu)(SegValue(ds) << 4ul) + (Bitu)reg_dx;
                INTDC_STORE_FUNCDEC(ofs,pc98_vfunc_key_ctrl[reg_ax - 0x34]);
                goto done;
            }
            else if (reg_ax == 0xFF) { /* Extended version of the API when AX == 0, DS:DX = data to store to */
                /* DS:DX contains
                 *       16*10 bytes, 16 bytes per entry for function keys F1-F10
                 *       16*5 bytes, 16 bytes per entry for VF1-VF5
                 *       16*10 bytes, 16 bytes per entry for function key shortcuts Shift+F1 to Shift+F10
                 *       16*5 bytes, 16 bytes per entry for shift VF1-VF5
                 *       6*11 bytes, 6 bytes per entry for editor keys
                 *       16*10 bytes, 16 bytes per entry for function key shortcuts Control+F1 to Control+F10
                 *       16*5 bytes, 16 bytes per entry for control VF1-VF5
                 *
                 * For whatever reason, the buffer is copied to the DOS buffer +1, meaning that on write it skips the 0x08 byte. */
                Bitu ofs = (Bitu)(SegValue(ds) << 4ul) + (Bitu)reg_dx;

                /* function keys F1-F10 */
                for (unsigned int f=0;f < 10;f++,ofs += 16)
                    INTDC_STORE_FUNCDEC(ofs,pc98_func_key[f]);
                /* VF1-VF5 */
                for (unsigned int f=0;f < 5;f++,ofs += 16)
                    INTDC_STORE_FUNCDEC(ofs,pc98_vfunc_key[f]);
                /* function keys Shift+F1 - Shift+F10 */
                for (unsigned int f=0;f < 10;f++,ofs += 16)
                    INTDC_STORE_FUNCDEC(ofs,pc98_func_key_shortcut[f]);
                /* VF1-VF5 */
                for (unsigned int f=0;f < 5;f++,ofs += 16)
                    INTDC_STORE_FUNCDEC(ofs,pc98_vfunc_key_shortcut[f]);
                /* editor keys */
                for (unsigned int f=0;f < 11;f++,ofs += 6)
                    INTDC_STORE_EDITDEC(ofs,pc98_editor_key_escapes[f]);
                /* function keys Control+F1 - Control+F10 */
                for (unsigned int f=0;f < 10;f++,ofs += 16)
                    INTDC_STORE_FUNCDEC(ofs,pc98_func_key_ctrl[f]);
                /* VF1-VF5 */
                for (unsigned int f=0;f < 5;f++,ofs += 16)
                    INTDC_STORE_FUNCDEC(ofs,pc98_vfunc_key_ctrl[f]);
 
                goto done;
            }
            goto unknown;
        case 0x0D: /* CL=0x0D General entry point to set function key state */
            if (reg_ax == 0xFF) { /* Extended version of the API when AX == 0, DS:DX = data to set */
                /* DS:DX contains
                 *       16*10 bytes, 16 bytes per entry for function keys F1-F10
                 *       16*5 bytes, 16 bytes per entry for VF1-VF5
                 *       16*10 bytes, 16 bytes per entry for function key shortcuts Shift+F1 to Shift+F10
                 *       16*5 bytes, 16 bytes per entry for shift VF1-VF5
                 *       6*11 bytes, 6 bytes per entry for editor keys
                 *       16*10 bytes, 16 bytes per entry for function key shortcuts Control+F1 to Control+F10
                 *       16*5 bytes, 16 bytes per entry for control VF1-VF5
                 *
                 * For whatever reason, the buffer is copied to the DOS buffer +1, meaning that on write it skips the 0x08 byte. */
                Bitu ofs = (Bitu)(SegValue(ds) << 4ul) + (Bitu)reg_dx;

                /* function keys F1-F10 */
                for (unsigned int f=0;f < 10;f++,ofs += 16)
                    INTDC_LOAD_FUNCDEC(pc98_func_key[f],ofs);
                /* VF1-VF5 */
                for (unsigned int f=0;f < 5;f++,ofs += 16)
                    INTDC_LOAD_FUNCDEC(pc98_vfunc_key[f],ofs);
                /* function keys Shift+F1 - Shift+F10 */
                for (unsigned int f=0;f < 10;f++,ofs += 16)
                    INTDC_LOAD_FUNCDEC(pc98_func_key_shortcut[f],ofs);
                /* Shift+VF1 - Shift+VF5 */
                for (unsigned int f=0;f < 5;f++,ofs += 16)
                    INTDC_LOAD_FUNCDEC(pc98_vfunc_key_shortcut[f],ofs);
                /* editor keys */
                for (unsigned int f=0;f < 11;f++,ofs += 6)
                    INTDC_LOAD_EDITDEC(pc98_editor_key_escapes[f],ofs);
                /* function keys Control+F1 - Control+F10 */
                for (unsigned int f=0;f < 10;f++,ofs += 16)
                    INTDC_LOAD_FUNCDEC(pc98_func_key_ctrl[f],ofs);
                /* Shift+VF1 - Shift+VF5 */
                for (unsigned int f=0;f < 5;f++,ofs += 16)
                    INTDC_LOAD_FUNCDEC(pc98_vfunc_key_ctrl[f],ofs);
 
                update_pc98_function_row(pc98_function_row_mode,true);
                goto done;
            }
            else if (reg_ax >= 0x01 && reg_ax <= 0x0A) { /* Write individual function keys, DS:DX = data to set */
                Bitu ofs = (Bitu)(SegValue(ds) << 4ul) + (Bitu)reg_dx;
                INTDC_LOAD_FUNCDEC(pc98_func_key[reg_ax - 0x01],ofs);
                update_pc98_function_row(pc98_function_row_mode,true);
                goto done;
            }
            else if (reg_ax >= 0x0B && reg_ax <= 0x14) { /* Write individual shift + function keys, DS:DX = data to set */
                Bitu ofs = (Bitu)(SegValue(ds) << 4ul) + (Bitu)reg_dx;
                INTDC_LOAD_FUNCDEC(pc98_func_key_shortcut[reg_ax - 0x0B],ofs);
                update_pc98_function_row(pc98_function_row_mode,true);
                goto done;
            }
            else if (reg_ax >= 0x15 && reg_ax <= 0x1F) { /* Write individual editor keys, DS:DX = data to set */
                Bitu ofs = (Bitu)(SegValue(ds) << 4ul) + (Bitu)reg_dx;
                INTDC_LOAD_EDITDEC(pc98_editor_key_escapes[reg_ax - 0x15],ofs);
                update_pc98_function_row(pc98_function_row_mode,true);
                goto done;
            }
            else if (reg_ax >= 0x20 && reg_ax <= 0x24) { /* Write VF1-VF5 keys, DS:DX = data to set */
                Bitu ofs = (Bitu)(SegValue(ds) << 4ul) + (Bitu)reg_dx;
                INTDC_LOAD_FUNCDEC(pc98_vfunc_key[reg_ax - 0x20],ofs);
                update_pc98_function_row(pc98_function_row_mode,true);
                goto done;
            }
            else if (reg_ax >= 0x25 && reg_ax <= 0x29) { /* Write shift VF1-VF5 keys, DS:DX = data to set */
                Bitu ofs = (Bitu)(SegValue(ds) << 4ul) + (Bitu)reg_dx;
                INTDC_LOAD_FUNCDEC(pc98_vfunc_key_shortcut[reg_ax - 0x25],ofs);
                update_pc98_function_row(pc98_function_row_mode,true);
                goto done;
            }
            else if (reg_ax >= 0x2A && reg_ax <= 0x33) { /* Write individual function keys, DS:DX = data to set */
                Bitu ofs = (Bitu)(SegValue(ds) << 4ul) + (Bitu)reg_dx;
                INTDC_LOAD_FUNCDEC(pc98_func_key_ctrl[reg_ax - 0x2A],ofs);
                update_pc98_function_row(pc98_function_row_mode,true);
                goto done;
            }
            else if (reg_ax >= 0x34 && reg_ax <= 0x38) { /* Write control VF1-VF5 keys, DS:DX = data to set */
                Bitu ofs = (Bitu)(SegValue(ds) << 4ul) + (Bitu)reg_dx;
                INTDC_LOAD_FUNCDEC(pc98_vfunc_key_ctrl[reg_ax - 0x34],ofs);
                update_pc98_function_row(pc98_function_row_mode,true);
                goto done;
            }
            else if (reg_ax == 0x00) { /* Write all state, DS:DX = data to set */
                /* DS:DX contains
                 *       16*10 bytes, 16 bytes per entry for function keys F1-F10
                 *       16*10 bytes, 16 bytes per entry for function key shortcuts Shift+F1 to Shift+F10
                 *       6*11 bytes, 6 bytes per entry of editor keys (INS, DEL, etc) that match a specific scan code range
                 *
                 * For whatever reason, the buffer is copied to the DOS buffer +1, meaning that on write it skips the 0x08 byte. */
                Bitu ofs = (Bitu)(SegValue(ds) << 4ul) + (Bitu)reg_dx;

                /* function keys F1-F10 */
                for (unsigned int f=0;f < 10;f++,ofs += 16)
                    INTDC_LOAD_FUNCDEC(pc98_func_key[f],ofs);
                /* function keys Shift+F1 - Shift+F10 */
                for (unsigned int f=0;f < 10;f++,ofs += 16)
                    INTDC_LOAD_FUNCDEC(pc98_func_key_shortcut[f],ofs);
                /* editor keys */
                for (unsigned int f=0;f < 11;f++,ofs += 6)
                    INTDC_LOAD_EDITDEC(pc98_editor_key_escapes[f],ofs);

                update_pc98_function_row(pc98_function_row_mode,true);
                goto done;
            }
            goto unknown;
        case 0x0F:
            if (reg_ax == 0) { /* inhibit Control+Fn shortcuts */
                real_writeb(0x60,0x10C,real_readb(0x60,0x10C) | 0x01);
                goto done;
            }
            else if (reg_ax == 1) { /* enable Control+Fn shortcuts */
                real_writeb(0x60,0x10C,real_readb(0x60,0x10C) & (~0x01));
                goto done;
            }
            goto unknown;
        case 0x10:
            if (reg_ah == 0x00) { /* CL=0x10 AH=0x00 DL=char write char to CON */
                PC98_INTDC_WriteChar(reg_dl);
                goto done;
            }
            else if (reg_ah == 0x01) { /* CL=0x10 AH=0x01 DS:DX write string to CON */
                /* According to the example at http://tepe.tec.fukuoka-u.ac.jp/HP98/studfile/grth/gt10.pdf
                 * the string ends in '$' just like the main DOS string output function. */
                uint16_t ofs = reg_dx;
                do {
                    unsigned char c = real_readb(SegValue(ds),ofs++);
                    if (c == '$') break;
                    PC98_INTDC_WriteChar(c);
                } while (1);
                goto done;
            }
            else if (reg_ah == 0x02) { /* CL=0x10 AH=0x02 DL=attribute set console output attribute */
                /* Ref: https://nas.jmc/jmcs/docs/browse/Computer/Platform/PC%2c%20NEC%20PC%2d98/Collections/Undocumented%209801%2c%209821%20Volume%202%20%28webtech.co.jp%29%20English%20translation/memdos%2eenglish%2dgoogle%2dtranslate%2etxt
                 *
                 * DL is the attribute byte (in the format written directly to video RAM, not the ANSI code)
                 *
                 * NTS: Reverse engineering INT DCh shows it sets both 71Dh and 73Ch as below */
                real_writeb(0x60,0x11D,reg_dl);
                real_writeb(0x60,0x13C,reg_dx);
                goto done;
            }
            else if (reg_ah == 0x03) { /* CL=0x10 AH=0x03 DL=X-coord DH=Y-coord set cursor position */
                void INTDC_CL10h_AH03h(uint16_t raw);
                INTDC_CL10h_AH03h(reg_dx);
                goto done;
            }
            else if (reg_ah == 0x04) { /* CL=0x10 AH=0x04 Move cursor down one line */
                void INTDC_CL10h_AH04h(void);
                INTDC_CL10h_AH04h();
                goto done;
            }
            else if (reg_ah == 0x05) { /* CL=0x10 AH=0x05 Move cursor up one line */
                void INTDC_CL10h_AH05h(void);
                INTDC_CL10h_AH05h();
                goto done;
            }
            else if (reg_ah == 0x06) { /* CL=0x10 AH=0x06 DX=count Move cursor up multiple lines */
                void INTDC_CL10h_AH06h(uint16_t count);
                INTDC_CL10h_AH06h(reg_dx);
                goto done;
            }
            else if (reg_ah == 0x07) { /* CL=0x10 AH=0x07 DX=count Move cursor down multiple lines */
                void INTDC_CL10h_AH07h(uint16_t count);
                INTDC_CL10h_AH07h(reg_dx);
                goto done;
            }
            else if (reg_ah == 0x08) { /* CL=0x10 AH=0x08 DX=count Move cursor right multiple lines */
                void INTDC_CL10h_AH08h(uint16_t count);
                INTDC_CL10h_AH08h(reg_dx);
                goto done;
            }
            else if (reg_ah == 0x09) { /* CL=0x10 AH=0x09 DX=count Move cursor left multiple lines */
                void INTDC_CL10h_AH09h(uint16_t count);
                INTDC_CL10h_AH09h(reg_dx);
                goto done;
            }
            else if (reg_ah == 0x0a) { /* CL=0x10 AH=0x0A DL=pattern Erase screen */
                void INTDC_CL10h_AH0Ah(uint16_t pattern);
                INTDC_CL10h_AH0Ah(reg_dx);
                goto done;
            }
            else if (reg_ah == 0x0b) { /* CL=0x10 AH=0x0B DL=pattern Erase lines */
                void INTDC_CL10h_AH0Bh(uint16_t pattern);
                INTDC_CL10h_AH0Bh(reg_dx);
                goto done;
            }
            else if (reg_ah == 0x0c) { /* CL=0x10 AH=0x0C DL=count Insert lines */
                void INTDC_CL10h_AH0Ch(uint16_t count);
                INTDC_CL10h_AH0Ch(reg_dx);
                goto done;
            }
            else if (reg_ah == 0x0d) { /* CL=0x10 AH=0x0D DL=count Erase lines */
                void INTDC_CL10h_AH0Dh(uint16_t count);
                INTDC_CL10h_AH0Dh(reg_dx);
                goto done;
            }
            else if (reg_ah == 0x0E) { /* CL=0x10 AH=0x0E DL=mode Change character mode */
                void pc98_set_char_mode(bool mode);
                pc98_set_char_mode(reg_dl == 0);
                goto done;
            }
            goto unknown;
        default: /* some compilers don't like not having a default case */
            goto unknown;
    }

done:
    return CBRET_NONE;

unknown:
    LOG_MSG("PC-98 INT DCh unknown call AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X",
        reg_ax,
        reg_bx,
        reg_cx,
        reg_dx,
        reg_si,
        reg_di,
        SegValue(ds),
        SegValue(es));

    return CBRET_NONE;
}

static Bitu INTF2_PC98_Handler(void) {
    LOG_MSG("PC-98 INT F2h unknown call AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X",
        reg_ax,
        reg_bx,
        reg_cx,
        reg_dx,
        reg_si,
        reg_di,
        SegValue(ds),
        SegValue(es));

    return CBRET_NONE;
}

extern void lio_read_parameter();
extern uint8_t PC98_BIOS_LIO_GINIT();
extern uint8_t PC98_BIOS_LIO_GSCREEN();
extern uint8_t PC98_BIOS_LIO_GVIEW();
extern uint8_t PC98_BIOS_LIO_GCOLOR1();
extern uint8_t PC98_BIOS_LIO_GCOLOR2();
extern uint8_t PC98_BIOS_LIO_GCLS();
extern uint8_t PC98_BIOS_LIO_GPSET();
extern uint8_t PC98_BIOS_LIO_GLINE();
extern uint8_t PC98_BIOS_LIO_GCIRCLE();
extern uint8_t PC98_BIOS_LIO_GPAINT1();
extern uint8_t PC98_BIOS_LIO_GPAINT2();
extern uint8_t PC98_BIOS_LIO_GGET();
extern uint8_t PC98_BIOS_LIO_GPUT1();
extern uint8_t PC98_BIOS_LIO_GPUT2();
extern uint8_t PC98_BIOS_LIO_GPOINT2();

// for more information see [https://ia801305.us.archive.org/8/items/PC9800TechnicalDataBookBIOS1992/PC-9800TechnicalDataBook_BIOS_1992_text.pdf]
static Bitu PC98_BIOS_LIO(void) {
    uint8_t ret = 0;
    const char *call_name = "?";

    lio_read_parameter();
    switch (reg_al) {
        case 0xA0: // GINIT
            ret = PC98_BIOS_LIO_GINIT();
            break;
        case 0xA1: // GSCREEN
            ret = PC98_BIOS_LIO_GSCREEN();
            break;
        case 0xA2: // GVIEW
            ret = PC98_BIOS_LIO_GVIEW();
            break;
        case 0xA3: // GCOLOR1
            ret = PC98_BIOS_LIO_GCOLOR1();
            break;
        case 0xA4: // GCOLOR2
            ret = PC98_BIOS_LIO_GCOLOR2();
            break;
        case 0xA5: // GCLS
            ret = PC98_BIOS_LIO_GCLS();
            break;
        case 0xA6: // GPSET
            ret = PC98_BIOS_LIO_GPSET();
            break;
        case 0xA7: // GLINE
            ret = PC98_BIOS_LIO_GLINE();
            break;
        case 0xA8: // GCIRCLE
            ret = PC98_BIOS_LIO_GCIRCLE();
            break;
        case 0xA9: // GPAINT1
            ret = PC98_BIOS_LIO_GPAINT1();
            break;
        case 0xAA: // GPAINT2
            ret = PC98_BIOS_LIO_GPAINT2();
            break;
        case 0xAB: // GGET
            ret = PC98_BIOS_LIO_GGET();
            break;
        case 0xAC: // GPUT1
            ret = PC98_BIOS_LIO_GPUT1();
            break;
        case 0xAD: // GPUT2
            ret = PC98_BIOS_LIO_GPUT2();
            break;
        case 0xAE: // GROLL
            call_name = "GROLL";
            goto unknown;
        case 0xAF: // GPOINT2
            ret = PC98_BIOS_LIO_GPOINT2();
            break;
        case 0xCE: // GCOPY
            call_name = "GCOPY";
            goto unknown;
        case 0x00: // GRAPH BIO
            call_name = "GRAPH BIO";
            goto unknown;
        default:
        unknown:
            /* on entry, AL (from our BIOS code) is set to the call number that lead here */
                LOG_MSG("PC-98 BIOS LIO graphics call 0x%02x '%s' with AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X",
                        reg_al,
                        call_name,
                        reg_ax,
                        reg_bx,
                        reg_cx,
                        reg_dx,
                        reg_si,
                        reg_di,
                        SegValue(ds),
                        SegValue(es));
                break;
    };
    // from Yksoft1's patch
    reg_ah = ret;

    return CBRET_NONE;
}


extern bool enable_weitek;

static Bitu INT11_Handler(void) {
    if (enable_weitek) reg_eax = (1u << 24u)/*Weitek math coprocessor present*/;
    reg_ax=mem_readw(BIOS_CONFIGURATION);
    return CBRET_NONE;
}
/* 
 * Define the following define to 1 if you want dosbox-x to check
 * the system time every 5 seconds and adjust 1/2 a second to sync them.
 */
#ifndef DOSBOX_CLOCKSYNC
#define DOSBOX_CLOCKSYNC 0
#endif

uint32_t BIOS_HostTimeSync(uint32_t /*ticks*/) {
#if 0//DISABLED TEMPORARILY
    uint32_t milli = 0;
#if defined(DB_HAVE_CLOCK_GETTIME) && ! defined(WIN32)
    struct timespec tp;
    clock_gettime(CLOCK_REALTIME,&tp);
	
    struct tm *loctime;
    loctime = localtime(&tp.tv_sec);
    milli = (uint32_t) (tp.tv_nsec / 1000000);
#else
    /* Setup time and date */
    struct timeb timebuffer;
    ftime(&timebuffer);
    
    const struct tm *loctime;
    loctime = localtime (&timebuffer.time);
    milli = (uint32_t) timebuffer.millitm;
#endif
    /*
    loctime->tm_hour = 23;
    loctime->tm_min = 59;
    loctime->tm_sec = 45;
    loctime->tm_mday = 28;
    loctime->tm_mon = 2-1;
    loctime->tm_year = 2007 - 1900;
    */

// FIXME: Why is the BIOS filling in the DOS kernel's date? That should be done when DOS boots!
    dos.date.day=(uint8_t)loctime->tm_mday;
    dos.date.month=(uint8_t)loctime->tm_mon+1;
    dos.date.year=(uint16_t)loctime->tm_year+1900;

    uint32_t nticks=(uint32_t)(((double)(
        (unsigned int)loctime->tm_hour*3600u*1000u+
        (unsigned int)loctime->tm_min*60u*1000u+
        (unsigned int)loctime->tm_sec*1000u+
        milli))*(((double)PIT_TICK_RATE/65536.0)/1000.0));

    /* avoid stepping back from off by one errors */
    if (nticks == (ticks - 1u))
        nticks = ticks;

    return nticks;
#endif
    return 0;
}

// TODO: make option
bool enable_bios_timer_synchronize_keyboard_leds = true;

void KEYBOARD_SetLEDs(uint8_t bits);

void BIOS_KEYBOARD_SetLEDs(Bitu state) {
    Bitu x = mem_readb(BIOS_KEYBOARD_LEDS);

    x &= ~7u;
    x |= (state & 7u);
    mem_writeb(BIOS_KEYBOARD_LEDS,x);
    KEYBOARD_SetLEDs(state);
}

/* PC-98 IRQ 0 system timer */
static Bitu INT8_PC98_Handler(void) {
    uint16_t counter = mem_readw(0x58A) - 1;
    mem_writew(0x58A,counter);

    /* NTS 2018/02/23: I just confirmed from the ROM BIOS of an actual
     *                 PC-98 system that this implementation and Neko Project II
     *                 are 100% accurate to what the BIOS actually does.
     *                 INT 07h really is the "timer tick" interrupt called
     *                 from INT 08h / IRQ 0, and the BIOS really does call
     *                 INT 1Ch AH=3 from INT 08h if the tick count has not
     *                 yet reached zero.
     *
     *                 I'm guessing NEC's BIOS developers invented this prior
     *                 to the Intel 80286 and it's INT 07h
     *                 "Coprocessor not present" exception. */

    if (counter == 0) {
        /* mask IRQ 0 */
        IO_WriteB(0x02,IO_ReadB(0x02) | 0x01);
        /* ack IRQ 0 */
        IO_WriteB(0x00,0x20);
        /* INT 07h */
        CPU_Interrupt(7,CPU_INT_SOFTWARE,reg_eip);
    }
    else {
        /* ack IRQ 0 */
        IO_WriteB(0x00,0x20);
        /* make sure it continues ticking */
        PC98_Interval_Timer_Continue();
    }

    return CBRET_NONE;
}


extern bool cmos_sync_flag;
extern uint8_t cmos_sync_sec,cmos_sync_min,cmos_sync_hour;

extern bool sync_time, manualtime;
bool sync_time_timerrate_warning = false;

uint32_t PIT0_GetAssignedCounter(void);

static Bitu INT8_Handler(void) {
    /* Increase the bios tick counter */
    uint32_t value = mem_readd(BIOS_TIMER) + 1;
    if(value >= 0x1800B0) {
        // time wrap at midnight
        mem_writeb(BIOS_24_HOURS_FLAG,mem_readb(BIOS_24_HOURS_FLAG)+1);
        value=0;
    }

    /* Legacy BIOS behavior: This isn't documented at all but most BIOSes
       check the BIOS data area for LED keyboard status. If it sees that
       value change, then it sends it to the keyboard. This is why on
       older DOS machines you could change LEDs by writing to 40:17.
       We have to emulate this also because Windows 3.1/9x seems to rely on
       it when handling the keyboard from its own driver. Their driver does
       hook the keyboard and handles keyboard I/O by itself, but it still
       allows the BIOS to do the keyboard magic from IRQ 0 (INT 8h). Yech. */
    if (enable_bios_timer_synchronize_keyboard_leds) {
        Bitu should_be = (mem_readb(BIOS_KEYBOARD_STATE) >> 4) & 7;
        Bitu led_state = (mem_readb(BIOS_KEYBOARD_LEDS) & 7);

        if (should_be != led_state)
            BIOS_KEYBOARD_SetLEDs(should_be);
    }

    if (sync_time && cmos_sync_flag) {
        value = (uint32_t)((cmos_sync_hour*3600+cmos_sync_min*60+cmos_sync_sec)*(float)PIT_TICK_RATE/65536.0);
        cmos_sync_flag = false;
    }
#if 0//DISABLED TEMPORARILY
    if (sync_time&&!manualtime) {
#if DOSBOX_CLOCKSYNC
        static bool check = false;
        if((value %50)==0) {
            if(((value %100)==0) && check) {
                check = false;
                time_t curtime;struct tm *loctime;
                curtime = time (NULL);loctime = localtime (&curtime);
                uint32_t ticksnu = (uint32_t)((loctime->tm_hour*3600+loctime->tm_min*60+loctime->tm_sec)*(float)PIT_TICK_RATE/65536.0);
                int32_t bios = value;int32_t tn = ticksnu;
                int32_t diff = tn - bios;
                if(diff>0) {
                    if(diff < 18) { diff  = 0; } else diff = 9;
                } else {
                    if(diff > -18) { diff = 0; } else diff = -9;
                }

                value += diff;
            } else if((value%100)==50) check = true;
        }
#endif

        /* synchronize time=true is based around the assumption
         * that the timer is left ticking at the standard 18.2Hz
         * rate. If that is not true, and this IRQ0 handler is
         * being called faster, then synchronization will not
         * work properly.
         *
         * Two 1996 demoscene entries sl_fokus.zip and sl_haloo.zip
         * are known to program the timer to run faster (58Hz and
         * 150Hz) yet use BIOS_TIMER from the BIOS data area to
         * track the passage of time. Synchronizing time that way
         * will only lead to BIOS_TIMER values that repeat or go
         * backwards and will break the demo. */
        if (PIT0_GetAssignedCounter() >= 0xFFFF/*Should be 0x10000 but we'll accept some programs might write 0xFFFF*/) {
            uint32_t BIOS_HostTimeSync(uint32_t ticks);
            value = BIOS_HostTimeSync(value);

            if (sync_time_timerrate_warning) {
                sync_time_timerrate_warning = false;
                LOG(LOG_MISC,LOG_WARN)("IRQ0 timer rate restored to 18.2Hz and synchronize time=true, resuming synchronization. BIOS_TIMER may jump backwards suddenly.");
            }
        }
        else {
            if (!sync_time_timerrate_warning) {
                /* Okay, you changed the tick rate. That affects BIOS_TIMER
                 * and therefore counts as manual time. Sorry. */
                sync_time_timerrate_warning = true;
                LOG(LOG_MISC,LOG_WARN)("IRQ0 timer rate is not 18.2Hz and synchronize time=true, disabling synchronization until normal rate restored.");
            }
        }
    }
#endif
    mem_writed(BIOS_TIMER,value);

	if(bootdrive>=0) {
#if (defined(WIN32) && !defined(HX_DOS) || defined(LINUX) && C_X11 || defined(MACOSX)) && (defined(C_SDL2) || defined(SDL_DOSBOX_X_SPECIAL))
        SetIMPosition();
#endif
    } else if (IS_DOSV && DOSV_CheckCJKVideoMode()) {
        INT8_DOSV();
    } else if(J3_IsJapanese()) {
        INT8_J3();
    } else if (IS_DOS_CJK) {
#if (defined(WIN32) && !defined(HX_DOS) || defined(LINUX) && C_X11 || defined(MACOSX)) && (defined(C_SDL2) || defined(SDL_DOSBOX_X_SPECIAL))
        SetIMPosition();
#endif
    }

    /* decrement FDD motor timeout counter; roll over on earlier PC, stop at zero on later PC */
    uint8_t val = mem_readb(BIOS_DISK_MOTOR_TIMEOUT);
    if (val || !IS_EGAVGA_ARCH) mem_writeb(BIOS_DISK_MOTOR_TIMEOUT,val-1);
    /* clear FDD motor bits when counter reaches zero */
    if (val == 1) mem_writeb(BIOS_DRIVE_RUNNING,mem_readb(BIOS_DRIVE_RUNNING) & 0xF0);
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
    if (reg_ah > 0x2 || reg_dx > 0x2) { // 0-2 printer port functions
                                        // and no more than 3 parallel ports
        LOG_MSG("BIOS INT17: Unhandled call AH=%2X DX=%4x",reg_ah,reg_dx);
        return CBRET_NONE;
    }

    switch(reg_ah) {
    case 0x00:      // PRINTER: Write Character
        if(parallelPortObjects[reg_dx]) {
            if(parallelPortObjects[reg_dx]->Putchar(reg_al))
                reg_ah=parallelPortObjects[reg_dx]->getPrinterStatus();
            else reg_ah=1;
        }
        break;
    case 0x01:      // PRINTER: Initialize port
        if(parallelPortObjects[reg_dx]) {
            parallelPortObjects[reg_dx]->initialize();
            reg_ah=parallelPortObjects[reg_dx]->getPrinterStatus();
        }
        break;
    case 0x02:      // PRINTER: Get Status
        if(parallelPortObjects[reg_dx])
            reg_ah=parallelPortObjects[reg_dx]->getPrinterStatus();
        //LOG_MSG("printer status: %x",reg_ah);
        break;
    case 0x20:		/* Some sort of printerdriver install check*/
        break;
    case 0x50: // Printer BIOS for AX
        if (!IS_JEGA_ARCH) break;
        switch (reg_al) {
        case 0x00:// Set JP/US mode in PRT BIOS
            LOG(LOG_BIOS, LOG_NORMAL)("AX PRT BIOS 5000h is called. (not implemented)");
            reg_al = 0x01; // Return error (not implemented)
            break;
        case 0x01:// Get JP/US mode in PRT BIOS
            reg_al = 0x01; // Return US mode (not implemented)
            break;
        default:
            LOG(LOG_BIOS, LOG_ERROR)("Unhandled AX Function 50%2X", reg_al);
            break;
        }
        break;
    }
    return CBRET_NONE;
}

static bool INT14_Wait(uint16_t port, uint8_t mask, uint8_t timeout, uint8_t* retval) {
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

static Bitu INT4B_Handler(void) {
    /* TODO: This is where the Virtual DMA specification is accessed on modern systems.
     *       When we implement that, move this to EMM386 emulation code. */

    if (reg_ax >= 0x8102 && reg_ax <= 0x810D) {
        LOG(LOG_MISC,LOG_DEBUG)("Guest OS attempted Virtual DMA specification call (INT 4Bh AX=%04x BX=%04x CX=%04x DX=%04x",
            reg_ax,reg_bx,reg_cx,reg_dx);
    }
    else if (reg_ah == 0x80) {
        LOG(LOG_MISC,LOG_DEBUG)("Guest OS attempted IBM SCSI interface call");
    }
    else if (reg_ah <= 0x02) {
        LOG(LOG_MISC,LOG_DEBUG)("Guest OS attempted TI Professional PC parallel port function AH=%02x",reg_ah);
    }
    else {
        LOG(LOG_MISC,LOG_DEBUG)("Guest OS attempted unknown INT 4Bh call AX=%04x",reg_ax);
    }
    
    /* Oh, I'm just a BIOS that doesn't know what the hell you're doing. CF=1 */
    CALLBACK_SCF(true);
    return CBRET_NONE;
}

static Bitu INT14_Handler(void) {
    if (reg_ah > 0x3 || reg_dx > 0x3) { // 0-3 serial port functions
                                        // and no more than 4 serial ports
        LOG_MSG("BIOS INT14: Unhandled call AH=%2X DX=%4x",reg_ah,reg_dx);
        return CBRET_NONE;
    }
    
    uint16_t port = real_readw(0x40,reg_dx * 2u); // DX is always port number
    uint8_t timeout = mem_readb((PhysPt)((unsigned int)BIOS_COM1_TIMEOUT + (unsigned int)reg_dx));
    if (port==0)    {
        LOG(LOG_BIOS,LOG_NORMAL)("BIOS INT14: port %d does not exist.",reg_dx);
        return CBRET_NONE;
    }
    switch (reg_ah) {
    case 0x00:  {
        // Initialize port
        // Parameters:              Return:
        // AL: port parameters      AL: modem status
        //                          AH: line status

        // set baud rate
        Bitu baudrate = 9600u;
        uint16_t baudresult;
        Bitu rawbaud=(Bitu)reg_al>>5u;
        
        if (rawbaud==0){ baudrate=110u;}
        else if (rawbaud==1){ baudrate=150u;}
        else if (rawbaud==2){ baudrate=300u;}
        else if (rawbaud==3){ baudrate=600u;}
        else if (rawbaud==4){ baudrate=1200u;}
        else if (rawbaud==5){ baudrate=2400u;}
        else if (rawbaud==6){ baudrate=4800u;}
        else if (rawbaud==7){ baudrate=9600u;}

        baudresult = (uint16_t)(115200u / baudrate);

        IO_WriteB(port+3u, 0x80u);    // enable divider access
        IO_WriteB(port, (uint8_t)baudresult&0xffu);
        IO_WriteB(port+1u, (uint8_t)(baudresult>>8u));

        // set line parameters, disable divider access
        IO_WriteB(port+3u, reg_al&0x1Fu); // LCR
        
        // disable interrupts
        IO_WriteB(port+1u, 0u); // IER

        // get result
        reg_ah=IO_ReadB(port+5u)&0xffu;
        reg_al=IO_ReadB(port+6u)&0xffu;
        CALLBACK_SCF(false);
        break;
    }
    case 0x01: // Transmit character
        // Parameters:              Return:
        // AL: character            AL: unchanged
        // AH: 0x01                 AH: line status from just before the char was sent
        //                              (0x80 | unpredicted) in case of timeout
        //                      [undoc] (0x80 | line status) in case of tx timeout
        //                      [undoc] (0x80 | modem status) in case of dsr/cts timeout

        // set DTR & RTS on
        IO_WriteB(port+4u,0x3u);
        // wait for DSR & CTS
        if (INT14_Wait(port+6u, 0x30u, timeout, &reg_ah)) {
            // wait for TX buffer empty
            if (INT14_Wait(port+5u, 0x20u, timeout, &reg_ah)) {
                // finally send the character
                IO_WriteB(port,reg_al);
            } else
                reg_ah |= 0x80u;
        } else // timed out
            reg_ah |= 0x80u;

        CALLBACK_SCF(false);
        break;
    case 0x02: // Read character
        // Parameters:              Return:
        // AH: 0x02                 AL: received character
        //                      [undoc] will be trashed in case of timeout
        //                          AH: (line status & 0x1E) in case of success
        //                              (0x80 | unpredicted) in case of timeout
        //                      [undoc] (0x80 | line status) in case of rx timeout
        //                      [undoc] (0x80 | modem status) in case of dsr timeout

        // set DTR on
        IO_WriteB(port+4u,0x1u);

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
        reg_ah=IO_ReadB(port+5u)&0xffu;
        reg_al=IO_ReadB(port+6u)&0xffu;
        CALLBACK_SCF(false);
        break;

    }
    return CBRET_NONE;
}

Bits HLT_Decode(void);
void KEYBOARD_AUX_Write(Bitu val);
unsigned char KEYBOARD_AUX_GetType();
unsigned char KEYBOARD_AUX_DevStatus();
unsigned char KEYBOARD_AUX_Resolution();
unsigned char KEYBOARD_AUX_SampleRate();
void KEYBOARD_ClrBuffer(void);
void KEYBOARD_AUX_LowerIRQ();

static Bitu INT15_Handler(void) {
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
            default:
                LOG(LOG_BIOS, LOG_NORMAL)("INT15 Unsupported PC1512 Call %02X", reg_ah);
                return CBRET_NONE;
        }
    }
    switch (reg_ah) {
    case 0x06:
        LOG(LOG_BIOS,LOG_NORMAL)("INT15 Unknown Function 6 (Amstrad?)");
        break;
    case 0x24:      //A20 stuff
        switch (reg_al) {
        case 0: //Disable a20
            MEM_A20_Enable(false);
            reg_ah = 0;                   //call successful
            CALLBACK_SCF(false);             //clear on success
            break;
        case 1: //Enable a20
            MEM_A20_Enable( true );
            reg_ah = 0;                   //call successful
            CALLBACK_SCF(false);             //clear on success
            break;
        case 2: //Query a20
            reg_al = MEM_A20_Enabled() ? 0x1 : 0x0;
            reg_ah = 0;                   //call successful
            CALLBACK_SCF(false);
            break;
        case 3: //Get a20 support
            reg_bx = 0x3;       //Bitmask, keyboard and 0x92
            reg_ah = 0;         //call successful
            CALLBACK_SCF(false);
            break;
        default:
            goto unhandled;
        }
        break;
    case 0xC0:  /* Get Configuration*/
        CPU_SetSegGeneral(es,biosConfigSeg);
        reg_bx = 0;
        reg_ah = 0;
        CALLBACK_SCF(false);
        break;
    case 0x4f:  /* BIOS - Keyboard intercept */
        /* Carry should be set but let's just set it just in case */
        CALLBACK_SCF(true);
        break;
    case 0x83:  /* BIOS - SET EVENT WAIT INTERVAL */
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
            uint32_t count=((uint32_t)reg_cx<<16u)|reg_dx;
            mem_writed(BIOS_WAIT_FLAG_POINTER,RealMake(SegValue(es),reg_bx));
            mem_writed(BIOS_WAIT_FLAG_COUNT,count);
            mem_writeb(BIOS_WAIT_FLAG_ACTIVE,1);
            /* Reprogram RTC to start */
            IO_Write(0x70,0xb);
            IO_Write(0x71,IO_Read(0x71)|0x40);
            CALLBACK_SCF(false);
        }
        break;
    case 0x84:  /* BIOS - JOYSTICK SUPPORT (XT after 11/8/82,AT,XT286,PS) */
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
                reg_ax = (uint16_t)(JOYSTICK_GetMove_X(0)*127+128);
                reg_bx = (uint16_t)(JOYSTICK_GetMove_Y(0)*127+128);
                if(JOYSTICK_IsEnabled(1)) {
                    reg_cx = (uint16_t)(JOYSTICK_GetMove_X(1)*127+128);
                    reg_dx = (uint16_t)(JOYSTICK_GetMove_Y(1)*127+128);
                }
                else {
                    reg_cx = reg_dx = 0;
                }
                CALLBACK_SCF(false);
            } else if (JOYSTICK_IsEnabled(1)) {
                reg_ax = reg_bx = 0;
                reg_cx = (uint16_t)(JOYSTICK_GetMove_X(1)*127+128);
                reg_dx = (uint16_t)(JOYSTICK_GetMove_Y(1)*127+128);
                CALLBACK_SCF(false);
            } else {            
                reg_ax = reg_bx = reg_cx = reg_dx = 0;
                CALLBACK_SCF(true);
            }
        } else {
            LOG(LOG_BIOS,LOG_ERROR)("INT15:84:Unknown Bios Joystick functionality.");
        }
        break;
    case 0x86:  /* BIOS - WAIT (AT,PS) */
        {
            if (mem_readb(BIOS_WAIT_FLAG_ACTIVE)) {
                reg_ah=0x83;
                CALLBACK_SCF(true);
                break;
            }
            uint8_t t;
            uint32_t count=((uint32_t)reg_cx<<16u)|reg_dx;
            mem_writed(BIOS_WAIT_FLAG_POINTER,RealMake(0,BIOS_WAIT_FLAG_TEMP));
            mem_writed(BIOS_WAIT_FLAG_COUNT,count);
            mem_writeb(BIOS_WAIT_FLAG_ACTIVE,1);

            /* if the user has not set the option, warn if IRQs are masked */
            if (!int15_wait_force_unmask_irq) {
                /* make sure our wait function works by unmasking IRQ 2, and IRQ 8.
                 * (bugfix for 1993 demo Yodel "Mayday" demo. this demo keeps masking IRQ 2 for some stupid reason.) */
                if ((t=IO_Read(0x21)) & (1 << 2)) {
                    LOG(LOG_BIOS,LOG_ERROR)("INT15:86:Wait: IRQ 2 masked during wait. "
                        "Consider adding 'int15 wait force unmask irq=true' to your dosbox-x.conf");
                }
                if ((t=IO_Read(0xA1)) & (1 << 0)) {
                    LOG(LOG_BIOS,LOG_ERROR)("INT15:86:Wait: IRQ 8 masked during wait. "
                        "Consider adding 'int15 wait force unmask irq=true' to your dosbox-x.conf");
                }
            }

            /* Reprogram RTC to start */
            IO_Write(0x70,0xb);
            IO_Write(0x71,IO_Read(0x71)|0x40);
            while (mem_readd(BIOS_WAIT_FLAG_COUNT)) {
                if (int15_wait_force_unmask_irq) {
                    /* make sure our wait function works by unmasking IRQ 2, and IRQ 8.
                     * (bugfix for 1993 demo Yodel "Mayday" demo. this demo keeps masking IRQ 2 for some stupid reason.) */
                    if ((t=IO_Read(0x21)) & (1 << 2)) {
                        LOG(LOG_BIOS,LOG_WARN)("INT15:86:Wait: IRQ 2 masked during wait. "
                            "This condition might result in an infinite wait on "
                            "some BIOSes. Unmasking IRQ to keep things moving along.");
                        IO_Write(0x21,t & ~(1 << 2));
                    }
                    if ((t=IO_Read(0xA1)) & (1 << 0)) {
                        LOG(LOG_BIOS,LOG_WARN)("INT15:86:Wait: IRQ 8 masked during wait. "
                            "This condition might result in an infinite wait on some "
                            "BIOSes. Unmasking IRQ to keep things moving along.");
                        IO_Write(0xA1,t & ~(1 << 0));
                    }
                }

                CALLBACK_Idle();
            }
            CALLBACK_SCF(false);
            break;
        }
    case 0x87:  /* Copy extended memory */
        {
            bool enabled = MEM_A20_Enabled();
            MEM_A20_Enable(true);
            Bitu   bytes    = reg_cx * 2u;
            PhysPt data     = SegPhys(es)+reg_si;
            PhysPt source   = (mem_readd(data+0x12u) & 0x00FFFFFFu) + ((unsigned int)mem_readb(data+0x17u)<<24u);
            PhysPt dest     = (mem_readd(data+0x1Au) & 0x00FFFFFFu) + ((unsigned int)mem_readb(data+0x1Fu)<<24u);
            MEM_BlockCopy(dest,source,bytes);
            reg_ax = 0x00;
            MEM_A20_Enable(enabled);
            Segs.limit[cs] = 0xFFFF;
            Segs.limit[ds] = 0xFFFF;
            Segs.limit[es] = 0xFFFF;
            Segs.limit[ss] = 0xFFFF;
            CALLBACK_SCF(false);
            break;
        }   
    case 0x88:  /* SYSTEM - GET EXTENDED MEMORY SIZE (286+) */
        /* This uses the 16-bit value read back from CMOS which is capped at 64MB */
        reg_ax=other_memsystems?0:size_extended;
        LOG(LOG_BIOS,LOG_NORMAL)("INT15:Function 0x88 Remaining %04X kb",reg_ax);
        CALLBACK_SCF(false);
        break;
    case 0x89:  /* SYSTEM - SWITCH TO PROTECTED MODE */
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
            reg_sp+=6;          //Clear stack of interrupt frame
            CPU_SetFlags(0,FMASK_ALL);
            reg_ax=0;
            CPU_JMP(false,0x30,ret,0);
        }
        break;
    case 0x8A:  /* EXTENDED MEMORY SIZE */
        {
            Bitu sz = MEM_TotalPages()*4;
            if (sz >= 1024) sz -= 1024;
            else sz = 0;
            reg_ax = sz & 0xFFFF;
            reg_dx = sz >> 16;
            CALLBACK_SCF(false);
        }
        break;
    case 0x90:  /* OS HOOK - DEVICE BUSY */
    case 0x91:  /* OS HOOK - DEVICE POST */
        CALLBACK_SCF(false);
        reg_ah=0;
        break;
    case 0xc2:  /* BIOS PS2 Pointing Device Support */
            /* TODO: Our reliance on AUX emulation means that at some point, AUX emulation
             *       must always be enabled if BIOS PS/2 emulation is enabled. Future planned change:
             *
             *       If biosps2=true and aux=true, carry on what we're already doing now: emulate INT 15h by
             *         directly writing to the AUX port of the keyboard controller.
             *
             *       If biosps2=false, the aux= setting enables/disables AUX emulation as it already does now.
             *         biosps2=false implies that we're emulating a keyboard controller with AUX but no BIOS
             *         support for it (however rare that might be). This gives the user of DOSBox-X the means
             *         to test that scenario especially in case he/she is developing a homebrew OS and needs
             *         to ensure their code can handle cases like that gracefully.
             *
             *       If biosps2=true and aux=false, AUX emulation is enabled anyway, but the keyboard emulation
             *         must act as if the AUX port is not there so the guest OS cannot control it. Again, not
             *         likely on real hardware, but a useful test case for homebrew OS developers.
             *
             *       If you the user set aux=false, then you obviously want to test a system configuration
             *       where the keyboard controller has no AUX port. If you set biosps2=true, then you want to
             *       test an OS that uses BIOS functions to setup the "pointing device" but you do not want the
             *       guest OS to talk directly to the AUX port on the keyboard controller.
             *
             *       Logically that's not likely to happen on real hardware, but we like giving the end-user
             *       options to play with, so instead, if aux=false and biosps2=true, DOSBox-X should print
             *       a warning stating that INT 15h mouse emulation with a PS/2 port is nonstandard and may
             *       cause problems with OSes that need to talk directly to hardware.
             *
             *       It is noteworthy that PS/2 mouse support in MS-DOS mouse drivers as well as Windows 3.x,
             *       Windows 95, and Windows 98, is carried out NOT by talking directly to the AUX port but
             *       instead by relying on the BIOS INT 15h functions here to do the dirty work. For those
             *       scenarios, biosps2=true and aux=false is perfectly safe and does not cause issues.
             *
             *       OSes that communicate directly with the AUX port however (Linux, Windows NT) will not work
             *       unless aux=true. */
        if (en_bios_ps2mouse) {
//          LOG_MSG("INT 15h AX=%04x BX=%04x\n",reg_ax,reg_bx);
            switch (reg_al) {
                case 0x00:      // enable/disable
                    if (reg_bh==0) {    // disable
                        KEYBOARD_AUX_Write(0xF5);
                        Mouse_SetPS2State(false);
                        reg_ah=0;
                        CALLBACK_SCF(false);
                        KEYBOARD_ClrBuffer();
                    } else if (reg_bh==0x01) {  //enable
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
                case 0x01:      // reset
                    KEYBOARD_AUX_Write(0xFF);
                    Mouse_SetPS2State(false);
                    KEYBOARD_ClrBuffer();
                    reg_bx=0x00aa;  // mouse (BH=device ID  BL=value returned by attached device after reset) [http://www.ctyme.com/intr/rb-1597.htm]
                    LOG_MSG("INT 15h mouse reset\n");
                    KEYBOARD_AUX_Write(0xF6); /* set defaults */
                    Mouse_SetPS2State(false);
                    KEYBOARD_ClrBuffer();
                    KEYBOARD_AUX_LowerIRQ(); /* HACK: Lower IRQ or else it will persist, which can cause problems with Windows 3.1 stock PS/2 mouse drivers */
                    CALLBACK_SCF(false);
                    reg_ah=0; // must return success. Fall through was well intended but, no, causes an error code that confuses mouse drivers
                    break;
                case 0x05:      // initialize
                    if (reg_bh >= 3 && reg_bh <= 4) {
                        /* TODO: BIOSes remember this value as the number of bytes to store before
                         *       calling the device callback. Setting this value to "1" is perfectly
                         *       valid if you want a byte-stream like mode (at the cost of one
                         *       interrupt per byte!). Most OSes will call this with BH=3 for standard
                         *       PS/2 mouse protocol. You can also call this with BH=4 for Intellimouse
                         *       protocol support, though testing (so far with VirtualBox) shows the
                         *       device callback still only gets the first three bytes on the stack.
                         *       Contrary to what you might think, the BIOS does not interpret the
                         *       bytes at all.
                         *
                         *       The source code of CuteMouse 1.9 seems to suggest some BIOSes take
                         *       pains to repack the 4th byte in the upper 8 bits of one of the WORDs
                         *       on the stack in Intellimouse mode at the cost of shifting the W and X
                         *       fields around. I can't seem to find any source on who does that or
                         *       if it's even true, so I disregard the example at this time.
                         *
                         *       Anyway, you need to store off this value somewhere and make use of
                         *       it in src/ints/mouse.cpp device callback emulation to reframe the
                         *       PS/2 mouse bytes coming from AUX (if aux=true) or emulate the
                         *       re-framing if aux=false to emulate this protocol fully. */
                        LOG_MSG("INT 15h mouse initialized to %u-byte protocol\n",reg_bh);
                        Mouse_PS2SetPacketSize(reg_bh);
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
                case 0x02: {        // set sampling rate
                    Mouse_PS2SetSamplingRate(reg_bh);
                    static const unsigned char tbl[7] = {10,20,40,60,80,100,200};
                    KEYBOARD_AUX_Write(0xF3);
                    if (reg_bh > 6) reg_bh = 6;
                    KEYBOARD_AUX_Write(tbl[reg_bh]);
                    KEYBOARD_ClrBuffer();
                    CALLBACK_SCF(false);
                    reg_ah=0;
                    } break;
                case 0x03:      // set resolution
                    KEYBOARD_AUX_Write(0xE8);
                    KEYBOARD_AUX_Write(reg_bh&3);
                    KEYBOARD_ClrBuffer();
                    CALLBACK_SCF(false);
                    reg_ah=0;
                    break;
                case 0x04:      // get type
                    reg_bh=KEYBOARD_AUX_GetType();  // ID
                    KEYBOARD_AUX_LowerIRQ(); /* HACK: Lower IRQ or else it will persist, which can cause problems with Windows 3.1 stock PS/2 mouse drivers */
                    LOG_MSG("INT 15h reporting mouse device ID 0x%02x\n",reg_bh);
                    KEYBOARD_ClrBuffer();
                    CALLBACK_SCF(false);
                    reg_ah=0;
                    break;
                case 0x06:      // extended commands
                    if (reg_bh == 0x00) {
                        /* Read device status and info.
                         * Windows 9x does not appear to use this, but Windows NT 3.1 does (prior to entering
                         * full 32-bit protected mode) */
                        CALLBACK_SCF(false);
                        reg_bx=KEYBOARD_AUX_DevStatus();
                        reg_cx=KEYBOARD_AUX_Resolution();
                        reg_dx=KEYBOARD_AUX_SampleRate();
                        KEYBOARD_AUX_LowerIRQ(); /* HACK: Lower IRQ or else it will persist, which can cause problems with Windows 3.1 stock PS/2 mouse drivers */
                        KEYBOARD_ClrBuffer();
                        reg_ah=0;
                    }
                    else if ((reg_bh==0x01) || (reg_bh==0x02)) { /* set scaling */
                        KEYBOARD_AUX_Write(0xE6u+reg_bh-1u); /* 0xE6 1:1   or 0xE7 2:1 */
                        KEYBOARD_ClrBuffer();
                        CALLBACK_SCF(false); 
                        reg_ah=0;
                    } else {
                        CALLBACK_SCF(true);
                        reg_ah=1;
                    }
                    break;
                case 0x07:      // set callback
                    Mouse_ChangePS2Callback(SegValue(es),reg_bx);
                    CALLBACK_SCF(false);
                    reg_ah=0;
                    break;
                default:
                    LOG_MSG("INT 15h unknown mouse call AX=%04x\n",reg_ax);
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
    case 0xc4:  /* BIOS POS Program option Select */
        LOG(LOG_BIOS,LOG_NORMAL)("INT15:Function %X called, bios mouse not supported",reg_ah);
        CALLBACK_SCF(true);
        break;
    case 0x53: // APM BIOS
        if (APMBIOS) {
            /* Windows 98 calls AH=05h CPU IDLE way too much per second, it makes it difficult to see anything important scroll by.
	     * Rate limit this particular call in the log file. */
            if (reg_al == 0x05) {
                APM_log_cpu_idle++;
                if (PIC_FullIndex() >= APM_log_cpu_idle_next_report) {
                    LOG(LOG_BIOS,LOG_DEBUG)("APM BIOS, %lu calls to AX=%04x BX=0x%04x CX=0x%04x\n",(unsigned long)APM_log_cpu_idle,reg_ax,reg_bx,reg_cx);
                    APM_log_cpu_idle_next_report = PIC_FullIndex() + 1000;
                    APM_log_cpu_idle = 0;
                }
            }
            else {
                LOG(LOG_BIOS,LOG_DEBUG)("APM BIOS call AX=%04x BX=0x%04x CX=0x%04x\n",reg_ax,reg_bx,reg_cx);
            }

            switch(reg_al) {
                case 0x00: // installation check
                    reg_ah = 1;             // major
                    reg_al = APM_BIOS_minor_version;    // minor
                    reg_bx = 0x504d;            // 'PM'
                    reg_cx = (APMBIOS_allow_prot16?0x01:0x00) + (APMBIOS_allow_prot32?0x02:0x00);
                    // 32-bit interface seems to be needed for standby in win95
                    CALLBACK_SCF(false);
                    break;
                case 0x01: // connect real mode interface
                    if(!APMBIOS_allow_realmode) {
                        LOG_MSG("APM BIOS: OS attempted real-mode connection, which is disabled in your dosbox-x.conf\n");
                        reg_ah = 0x86;  // APM not present
                        CALLBACK_SCF(true);         
                        break;
                    }
                    if(reg_bx != 0x0) {
                        reg_ah = 0x09;  // unrecognized device ID
                        CALLBACK_SCF(true);         
                        break;
                    }
                    if(!apm_realmode_connected) { // not yet connected
                        LOG_MSG("APM BIOS: Connected to real-mode interface\n");
                        CALLBACK_SCF(false);
                        APMBIOS_connect_mode = APMBIOS_CONNECT_REAL;
                        PowerButtonClicks=0; /* BIOSes probably clear whatever hardware register this involves... we'll see */
                        APM_ResumeNotificationFromStandby = false;
                        APM_ResumeNotificationFromSuspend = false;
                        apm_realmode_connected=true;
                    } else {
                        LOG_MSG("APM BIOS: OS attempted to connect to real-mode interface when already connected\n");
                        reg_ah = APMBIOS_connected_already_err(); // interface connection already in effect
                        CALLBACK_SCF(true);         
                    }
                    APM_BIOS_connected_minor_version = 0;
                    break;
                case 0x02: // connect 16-bit protected mode interface
                    if(!APMBIOS_allow_prot16) {
                        LOG_MSG("APM BIOS: OS attempted 16-bit protected mode connection, which is disabled in your dosbox-x.conf\n");
                        reg_ah = 0x06;  // not supported
                        CALLBACK_SCF(true);         
                        break;
                    }
                    if(reg_bx != 0x0) {
                        reg_ah = 0x09;  // unrecognized device ID
                        CALLBACK_SCF(true);         
                        break;
                    }
                    if(!apm_realmode_connected) { // not yet connected
                        /* NTS: We use the same callback address for both 16-bit and 32-bit
                         *      because only the DOS callback and RETF instructions are involved,
                         *      which can be executed as either 16-bit or 32-bit code without problems. */
                        LOG_MSG("APM BIOS: Connected to 16-bit protected mode interface\n");
                        CALLBACK_SCF(false);
                        reg_ax = INT15_apm_pmentry >> 16;   // AX = 16-bit code segment (real mode base)
                        reg_bx = INT15_apm_pmentry & 0xFFFF;    // BX = offset of entry point
                        reg_cx = INT15_apm_pmentry >> 16;   // CX = 16-bit data segment (NTS: doesn't really matter)
                        reg_si = 0xFFFF;            // SI = code segment length
                        reg_di = 0xFFFF;            // DI = data segment length
                        APMBIOS_connect_mode = APMBIOS_CONNECT_PROT16;
                        PowerButtonClicks=0; /* BIOSes probably clear whatever hardware register this involves... we'll see */
                        APM_ResumeNotificationFromStandby = false;
                        APM_ResumeNotificationFromSuspend = false;
                        apm_realmode_connected=true;
                    } else {
                        LOG_MSG("APM BIOS: OS attempted to connect to 16-bit protected mode interface when already connected\n");
                        reg_ah = APMBIOS_connected_already_err(); // interface connection already in effect
                        CALLBACK_SCF(true);         
                    }
                    APM_BIOS_connected_minor_version = 0;
                    break;
                case 0x03: // connect 32-bit protected mode interface
                    // Note that Windows 98 will NOT talk to the APM BIOS unless the 32-bit protected mode connection is available.
                    // And if you lie about it in function 0x00 and then fail, Windows 98 will fail with a "Windows protection error".
                    if(!APMBIOS_allow_prot32) {
                        LOG_MSG("APM BIOS: OS attempted 32-bit protected mode connection, which is disabled in your dosbox-x.conf\n");
                        reg_ah = 0x08;  // not supported
                        CALLBACK_SCF(true);         
                        break;
                    }
                    if(reg_bx != 0x0) {
                        reg_ah = 0x09;  // unrecognized device ID
                        CALLBACK_SCF(true);         
                        break;
                    }
                    if(!apm_realmode_connected) { // not yet connected
                        LOG_MSG("APM BIOS: Connected to 32-bit protected mode interface\n");
                        CALLBACK_SCF(false);
                        /* NTS: We use the same callback address for both 16-bit and 32-bit
                         *      because only the DOS callback and RETF instructions are involved,
                         *      which can be executed as either 16-bit or 32-bit code without problems. */
                        reg_ax = INT15_apm_pmentry >> 16;   // AX = 32-bit code segment (real mode base)
                        reg_ebx = INT15_apm_pmentry & 0xFFFF;   // EBX = offset of entry point
                        reg_cx = INT15_apm_pmentry >> 16;   // CX = 16-bit code segment (real mode base)
                        reg_dx = INT15_apm_pmentry >> 16;   // DX = data segment (real mode base) (?? what size?)
                        reg_esi = 0xFFFFFFFF;           // ESI = upper word: 16-bit code segment len  lower word: 32-bit code segment length
                        reg_di = 0xFFFF;            // DI = data segment length
                        APMBIOS_connect_mode = APMBIOS_CONNECT_PROT32;
                        PowerButtonClicks=0; /* BIOSes probably clear whatever hardware register this involves... we'll see */
                        APM_ResumeNotificationFromStandby = false;
                        APM_ResumeNotificationFromSuspend = false;
                        apm_realmode_connected=true;
                    } else {
                        LOG_MSG("APM BIOS: OS attempted to connect to 32-bit protected mode interface when already connected\n");
                        reg_ah = APMBIOS_connected_already_err(); // interface connection already in effect
                        CALLBACK_SCF(true);         
                    }
                    APM_BIOS_connected_minor_version = 0;
                    break;
                case 0x04: // DISCONNECT INTERFACE
                    if(reg_bx != 0x0) {
                        reg_ah = 0x09;  // unrecognized device ID
                        CALLBACK_SCF(true);         
                        break;
                    }
                    if(apm_realmode_connected) {
                        LOG_MSG("APM BIOS: OS disconnected\n");
                        CALLBACK_SCF(false);
                        apm_realmode_connected=false;
                    } else {
                        reg_ah = 0x03;  // interface not connected
                        CALLBACK_SCF(true);         
                    }
                    APM_BIOS_connected_minor_version = 0;
                    break;
                case 0x05: // CPU IDLE
                    if(!apm_realmode_connected) {
                        reg_ah = 0x03;
                        CALLBACK_SCF(true);
                        break;
                    }

                    // Trigger CPU HLT instruction.
                    // NTS: For whatever weird reason, NOT emulating HLT makes Windows 95
                    //      crashy when the APM driver is active! There's something within
                    //      the Win95 kernel that apparently screws up really badly if
                    //      the APM IDLE call returns immediately. The best case scenario
                    //      seems to be that Win95's APM driver has some sort of timing
                    //      logic to it that if it detects an immediate return, immediately
                    //      shuts down and powers off the machine. Windows 98 also seems
                    //      to require a HLT, and will act erratically without it.
                    //
                    //      Also need to note that the choice of "HLT" is not arbitrary
                    //      at all. The APM BIOS standard mentions CPU IDLE either stopping
                    //      the CPU clock temporarily or issuing HLT as a valid method.
                    //
                    // TODO: Make this a dosbox-x.conf configuration option: what do we do
                    //       on APM idle calls? Allow selection between "nothing" "hlt"
                    //       and "software delay".
                    if (!(reg_flags&0x200)) {
                        LOG(LOG_BIOS,LOG_WARN)("APM BIOS warning: CPU IDLE called with IF=0, not HLTing\n");
                    }
                    else if (cpudecoder == &HLT_Decode) { /* do not re-execute HLT, it makes DOSBox-X hang */
                        LOG_MSG("APM BIOS warning: CPU IDLE HLT within HLT (DOSBox-X core failure)\n");
                    }
                    else {
                        CPU_HLT(reg_eip);
                    }
                    break;
                case 0x06: // CPU BUSY
                    if(!apm_realmode_connected) {
                        reg_ah = 0x03;
                        CALLBACK_SCF(true);
                        break;
                    }

                    /* OK. whatever. system no longer idle */
                    CALLBACK_SCF(false);
                    break;
                case 0x07:
                    if(reg_bx != 0x1) {
                        reg_ah = 0x09;  // wrong device ID
                        CALLBACK_SCF(true);         
                        break;
                    }
                    if(!apm_realmode_connected) {
                        reg_ah = 0x03;
                        CALLBACK_SCF(true);
                        break;
                    }
                    switch(reg_cx) {
                        case 0x1: // standby
                            LOG(LOG_MISC,LOG_DEBUG)("Guest attempted to set power state to standby");
                            APM_BeginSuspendedMode();
                            reg_ah = 0x00;//TODO
                            CALLBACK_SCF(false);
                            APM_ResumeNotificationFromStandby = true;
                            break;
                        case 0x2: // suspend
                            LOG(LOG_MISC,LOG_DEBUG)("Guest attempted to set power state to suspend");
                            APM_BeginSuspendedMode();
                            reg_ah = 0x00;//TODO
                            CALLBACK_SCF(false);
                            APM_ResumeNotificationFromSuspend = true;
                            break;
                        case 0x3: // power off
                            throw 0;
                        case 0x4: // last request processing notification (used by Windows ME)
                            LOG(LOG_MISC,LOG_DEBUG)("Guest is considering whether to accept the last returned APM event");
                            reg_ah = 0x00;
                            CALLBACK_SCF(false);
                            break;
                        case 0x5: // reject last request (used by Windows ME)
                            LOG(LOG_MISC,LOG_DEBUG)("Guest has rejected the last APM event");
                            reg_ah = 0x00;
                            CALLBACK_SCF(false);
                            break;
                        default:
                            reg_ah = 0x0A; // invalid parameter value in CX
                            CALLBACK_SCF(true);
                            break;
                    }
                    break;
                case 0x08: // ENABLE/DISABLE POWER MANAGEMENT
                    if(reg_bx != 0x0 && reg_bx != 0x1) {
                        reg_ah = 0x09;  // unrecognized device ID
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
                case 0x0a: // GET POWER STATUS
                    if (!apm_realmode_connected) {
                        reg_ah = 0x03;  // interface not connected
                        CALLBACK_SCF(true);
                        break;
                    }
                    if (reg_bx != 0x0001 && reg_bx != 0x8001) {
                        reg_ah = 0x09;  // unrecognized device ID
                        CALLBACK_SCF(true);         
                        break;
                    }
                    /* FIXME: Allow configuration and shell commands to dictate whether or
                     *        not we emulate a laptop with a battery */
                    reg_bh = 0x01;      // AC line status (1=on-line)
                    reg_bl = 0xFF;      // Battery status (unknown)
                    reg_ch = 0x80;      // Battery flag (no system battery)
                    reg_cl = 0xFF;      // Remaining battery charge (unknown)
                    reg_dx = 0xFFFF;    // Remaining battery life (unknown)
                    reg_si = 0;     // Number of battery units (if called with reg_bx == 0x8001)
                    CALLBACK_SCF(false);
                    break;
                case 0x0b: // GET PM EVENT
                    if (!apm_realmode_connected) {
                        reg_ah = 0x03;  // interface not connected
                        CALLBACK_SCF(true);
                        break;
                    }
                    // power button?
                    if (PowerButtonClicks != 0) { // Hardware and BIOSes probably just set a bit somewhere, so act like it
                        LOG(LOG_MISC,LOG_DEBUG)("Returning APM power button event to guest OS");
                        reg_ah = 0x00;  // FIXME: The standard doesn't say anything about AH on success

			if (APM_PowerButtonSendsSuspend)
                            reg_bx = 0x000A;// user pushed a button, wants to suspend the system
			else
                            reg_bx = 0x0009;// user pushed a button, wants to put the system into standby

                        reg_cx = 0x0000;
                        CALLBACK_SCF(false);
                        PowerButtonClicks = 0;
                        break;
                    }
                    // resume from standby? Windows 98 will spin in a loop for 5+ seconds until it gets this APM message after suspend
                    if (APM_ResumeNotificationFromStandby) {
                        LOG(LOG_MISC,LOG_DEBUG)("Returning APM resume from standby notification event to guest OS");
                        reg_ah = 0x00;  // FIXME: The standard doesn't say anything about AH on success
                        reg_bx = 0x000B;// System Standby Resume Notification
                        reg_cx = 0x0000;
                        CALLBACK_SCF(false);
                        APM_ResumeNotificationFromStandby = false;
                        break;
                    }
                    // resume from suspend? Windows 98 will spin in a loop for 5+ seconds until it gets this APM message after suspend
                    if (APM_ResumeNotificationFromSuspend) {
                        LOG(LOG_MISC,LOG_DEBUG)("Returning APM resume from suspend notification event to guest OS");
                        reg_ah = 0x00;  // FIXME: The standard doesn't say anything about AH on success
                        reg_bx = 0x0003;// Normal Resume System Notification
                        reg_cx = 0x0000;
                        CALLBACK_SCF(false);
                        APM_ResumeNotificationFromSuspend = false;
                        break;
                    }
                    // nothing
                    reg_ah = 0x80; // no power management events pending
                    CALLBACK_SCF(true);
                    break;
                case 0x0d:
                    // NTS: NOT implementing this call can cause Windows 98's APM driver to crash on startup
                    if(reg_bx != 0x0 && reg_bx != 0x1) {
                        reg_ah = 0x09;  // unrecognized device ID
                        CALLBACK_SCF(true);
                        break;
                    } else if(!apm_realmode_connected) {
                        reg_ah = 0x03;
                        CALLBACK_SCF(true);
                        break;
                    }
                    if(reg_cx==0x0) {
                        LOG_MSG("disable APM for device %4x",reg_bx);
                        CALLBACK_SCF(false);
                    }
                    else if(reg_cx==0x1) {
                        LOG_MSG("enable APM for device %4x",reg_bx);
                        CALLBACK_SCF(false);
                    }
                    else {
                        reg_ah = 0x0A; // invalid parameter value in CX
                        CALLBACK_SCF(true);
                    }
                    break;
                case 0x0e:
                    if (APM_BIOS_minor_version != 0) { // APM 1.1 or higher only
                        if(reg_bx != 0x0) {
                            reg_ah = 0x09;  // unrecognized device ID
                            CALLBACK_SCF(true);
                            break;
                        } else if(!apm_realmode_connected) {
                            reg_ah = 0x03;  // interface not connected
                            CALLBACK_SCF(true);
                            break;
                        }
                        reg_ah = reg_ch; /* we are called with desired version in CH,CL, return actual version in AH,AL */
                        reg_al = reg_cl;
                        if(reg_ah != 1) reg_ah = 1;                     // major
                        if(reg_al > APM_BIOS_minor_version) reg_al = APM_BIOS_minor_version;    // minor
                        APM_BIOS_connected_minor_version = reg_al;              // what we decided becomes the interface we emulate
                        LOG_MSG("APM BIOS negotiated to v1.%u",APM_BIOS_connected_minor_version);
                        CALLBACK_SCF(false);
                    }
                    else { // APM 1.0 does not recognize this call
                        reg_ah = 0x0C; // function not supported
                        CALLBACK_SCF(true);
                    }
                    break;
                case 0x0f:
                    if(reg_bx != 0x0 && reg_bx != 0x1) {
                        reg_ah = 0x09;  // unrecognized device ID
                        CALLBACK_SCF(true);         
                        break;
                    } else if(!apm_realmode_connected) {
                        reg_ah = 0x03;
                        CALLBACK_SCF(true);
                        break;
                    }
                    if(reg_cx==0x0) {
                        LOG_MSG("disengage APM for device %4x",reg_bx);
                        CALLBACK_SCF(false);
                    }
                    else if(reg_cx==0x1) {
                        LOG_MSG("engage APM for device %4x",reg_bx);
                        CALLBACK_SCF(false);
                    }
                    else {
                        reg_ah = 0x0A; // invalid parameter value in CX
                        CALLBACK_SCF(true);
                    }
                    break;
                case 0x10:
                    if (!apm_realmode_connected) {
                        reg_ah = 0x03;  // interface not connected
                        CALLBACK_SCF(true);
                        break;
                    }
                    if (reg_bx != 0) {
                        reg_ah = 0x09;  // unrecognized device ID
                        CALLBACK_SCF(true);
                        break;
                    }
                    reg_ah = 0;
                    reg_bl = 0; // number of battery units
                    reg_cx = 0x03; // can enter suspend/standby and will post standby/resume events
                    CALLBACK_SCF(false);
                    break;
                case 0x13://enable/disable/query timer based requests
                    // NTS: NOT implementing this call can cause Windows 98's APM driver to crash on startup
                    if (!apm_realmode_connected) {
                        reg_ah = 0x03;  // interface not connected
                        CALLBACK_SCF(true);
                        break;
                    }
                    if (reg_bx != 0) {
                        reg_ah = 0x09;  // unrecognized device ID
                        CALLBACK_SCF(true);
                        break;
                    }

                    if (reg_cx == 0) { // disable
                        APM_inactivity_timer = false;
                        reg_cx = 0;
                        CALLBACK_SCF(false);
                    }
                    else if (reg_cx == 1) { // enable
                        APM_inactivity_timer = true;
                        reg_cx = 1;
                        CALLBACK_SCF(false);
                    }
                    else if (reg_cx == 2) { // get enabled status
                        reg_cx = APM_inactivity_timer ? 1 : 0;
                        CALLBACK_SCF(false);
                    }
                    else {
                        reg_ah = 0x0A; // invalid parameter value in CX
                        CALLBACK_SCF(true);
                    }
                    break;
                default:
                    LOG_MSG("Unknown APM BIOS call AX=%04x\n",reg_ax);
                    if (!apm_realmode_connected) {
                        reg_ah = 0x03;  // interface not connected
                        CALLBACK_SCF(true);
                        break;
                    }
                    reg_ah = 0x0C; // function not supported
                    CALLBACK_SCF(true);
                    break;
            }
        }
        else {
            reg_ah=0x86;
            CALLBACK_SCF(true);
            LOG_MSG("APM BIOS call attempted. set apmbios=1 if you want power management\n");
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
                    sz /= 64;   /* extended memory size from 16MB in 64KB blocks */
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
                        if (reg_ebx < E280_table_entries) {
                            BIOS_E280_entry &ent = E280_table[reg_ebx];
                            Bitu seg = SegValue(es);

                            /* write to ES:DI */
                            real_writed(seg,reg_di+0x00,ent.base);
                            real_writed(seg,reg_di+0x04,(uint32_t)(ent.base >> (uint64_t)32u));
                            real_writed(seg,reg_di+0x08,ent.length);
                            real_writed(seg,reg_di+0x0C,(uint32_t)(ent.length >> (uint64_t)32u));
                            real_writed(seg,reg_di+0x10,ent.type);
                            reg_ecx = 20;

                            /* return EBX pointing to next entry. wrap around, as most BIOSes do.
                             * the program is supposed to stop on CF=1 or when we return EBX == 0 */
                            if (++reg_ebx >= E280_table_entries) reg_ebx = 0;
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
            unhandled:
                LOG(LOG_BIOS,LOG_ERROR)("INT15:Unknown call ah=E8, al=%2X",reg_al);
                reg_ah=0x86;
                CALLBACK_SCF(true);
                if ((IS_EGAVGA_ARCH) || (machine==MCH_CGA) || (machine==MCH_AMSTRAD)) {
                    /* relict from comparisons, as int15 exits with a retf2 instead of an iret */
                    CALLBACK_SZF(false);
                }
        }
        break;
    case 0x50:
        if(isDBCSCP()) {
            if(reg_al == 0x00) {
                if(reg_bl == 0x00 && reg_bp == 0x00) {
                    enum DOSV_FONT font = DOSV_FONT_MAX;
                    if(reg_bh & 0x01) {
                        if(reg_dh == 16 && reg_dl == 16) {
                            font = DOSV_FONT_16X16;
                        } else if(reg_dh == 24 && reg_dl == 24) {
                            font = DOSV_FONT_24X24;
                        }
                    } else {
                        if(reg_dh == 8) {
                            if(reg_dl == 16) {
                                font = DOSV_FONT_8X16;
                            } else if(reg_dl == 19) {
                                font = DOSV_FONT_8X19;
                            }
                        } else if(reg_dh == 12 && reg_dl == 24) {
                            font = DOSV_FONT_12X24;
                        }
                    }
                    if(font != DOSV_FONT_MAX) {
                        reg_ah = 0x00;
                        SegSet16(es, CB_SEG);
                        reg_bx = DOSV_GetFontHandlerOffset(font);
                        CALLBACK_SCF(false);
                        break;
                    }
                }
            } else if(reg_al == 0x01) {
                if(reg_dh == 16 && reg_dl == 16) {
                    reg_ah = 0x00;
                    SegSet16(es, CB_SEG);
                    reg_bx = DOSV_GetFontHandlerOffset(DOSV_FONT_16X16_WRITE);
                    CALLBACK_SCF(false);
                    break;
                } else if(reg_dh == 24 && reg_dl == 24) {
                    reg_ah = 0x00;
                    SegSet16(es, CB_SEG);
                    reg_bx = DOSV_GetFontHandlerOffset(DOSV_FONT_24X24_WRITE);
                    CALLBACK_SCF(false);
                    break;
                } else {
                    reg_ah = 0x06;		// read only
                }
            }
            CALLBACK_SCF(true);
        }
        break;
    case 0x49:
        if(isDBCSCP()) {
            reg_ah = 0x00;
            reg_bl = 0x00;
            CALLBACK_SCF(false);
        } else {
            CALLBACK_SCF(true);
        }
        break;
    default:
        LOG(LOG_BIOS,LOG_ERROR)("INT15:Unknown call ax=%4X",reg_ax);
        reg_ah=0x86;
        CALLBACK_SCF(true);
        if ((IS_EGAVGA_ARCH) || (machine==MCH_CGA) || (machine==MCH_AMSTRAD)) {
            /* relict from comparisons, as int15 exits with a retf2 instead of an iret */
            CALLBACK_SZF(false);
        }
    }
    return CBRET_NONE;
}

void BIOS_UnsetupKeyboard(void);
void BIOS_SetupKeyboard(void);
void BIOS_UnsetupDisks(void);
void BIOS_SetupDisks(void);
void CPU_Snap_Back_To_Real_Mode();
void CPU_Snap_Back_Restore();

static Bitu Default_IRQ_Handler(void) {
    IO_WriteB(0x20, 0x0b);
    uint8_t master_isr = IO_ReadB(0x20);
    if (master_isr) {
        IO_WriteB(0xa0, 0x0b);
        uint8_t slave_isr = IO_ReadB(0xa0);
        if (slave_isr) {
            IO_WriteB(0xa1, IO_ReadB(0xa1) | slave_isr);
            IO_WriteB(0xa0, 0x20);
        }
        else IO_WriteB(0x21, IO_ReadB(0x21) | (master_isr & ~4));
        IO_WriteB(0x20, 0x20);
#if C_DEBUG
        uint16_t irq = 0;
        uint16_t isr = master_isr;
        if (slave_isr) isr = slave_isr << 8;
        while (isr >>= 1) irq++;
        LOG(LOG_BIOS, LOG_WARN)("Unexpected IRQ %u", irq);
#endif 
    }
    else master_isr = 0xff;
    mem_writeb(BIOS_LAST_UNEXPECTED_IRQ, master_isr);
    return CBRET_NONE;
}

static Bitu IRQ14_Dummy(void) {
    /* FIXME: That's it? Don't I EOI the PIC? */
    return CBRET_NONE;
}

static Bitu IRQ15_Dummy(void) {
    /* FIXME: That's it? Don't I EOI the PIC? */
    return CBRET_NONE;
}

void On_Software_CPU_Reset();

static Bitu INT18_Handler(void) {
	if (ibm_rom_basic_size != 0) {
		/* jump to BASIC (usually F600:0000 for IBM 5150 ROM BASIC) */
		SegSet16(cs, ibm_rom_basic_base >> 4);
		reg_eip = 0;
	}
	else {
		LOG_MSG("Restart by INT 18h requested\n");
		On_Software_CPU_Reset();
		/* does not return */
	}
	return CBRET_NONE;
}

static Bitu INT19_Handler(void) {
    LOG_MSG("Restart by INT 19h requested\n");
    /* FIXME: INT 19h is sort of a BIOS boot BIOS reset-ish thing, not really a CPU reset at all. */
    On_Software_CPU_Reset();
    /* does not return */
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

    if (IS_PC98_ARCH) {
        Bitu mempages = MEM_TotalPages(); /* in 4KB pages */

        /* What applies to IBM PC/AT (zeroing out the extended memory size)
         * also applies to PC-98, when HIMEM.SYS is loaded */
        if (in) mempages = 0;

        /* extended memory size (286 systems, below 16MB) */
        if (mempages > (1024UL/4UL)) {
            unsigned int ext = ((mempages - (1024UL/4UL)) * 4096UL) / (128UL * 1024UL); /* convert to 128KB units */

            /* extended memory, up to 16MB capacity (for 286 systems?)
             *
             * MS-DOS drivers will "allocate" for themselves by taking from the top of
             * extended memory then subtracting from this value.
             *
             * capacity does not include conventional memory below 1MB, nor any memory
             * above 16MB.
             *
             * PC-98 systems may reserve the top 1MB, limiting the top to 15MB instead,
             * for the ISA memory hole needed for DOS games that use the 256-color linear framebuffer.
             *
             * 0x70 = 128KB * 0x70 = 14MB
             * 0x78 = 128KB * 0x70 = 15MB */
            if (isa_memory_hole_15mb) {
                if (ext > 0x70) ext = 0x70;
            }
            else {
                if (ext > 0x78) ext = 0x78;
            }

            mem_writeb(0x401,ext);
        }
        else {
            mem_writeb(0x401,0x00);
        }

        /* extended memory size (386 systems, at or above 16MB) */
        if (mempages > ((1024UL*16UL)/4UL)) {
            unsigned int ext = ((mempages - ((1024UL*16UL)/4UL)) * 4096UL) / (1024UL * 1024UL); /* convert to MB */

            /* extended memory, at or above 16MB capacity (for 386+ systems?)
             *
             * MS-DOS drivers will "allocate" for themselves by taking from the top of
             * extended memory then subtracting from this value.
             *
             * capacity does not include conventional memory below 1MB, nor any memory
             * below 16MB. */
            if (ext > 0xFFFE) ext = 0xFFFE;

            mem_writew(0x594,ext);
        }
        else {
            mem_writew(0x594,0x00);
        }
    }
}

unsigned char do_isapnp_chksum(const unsigned char* d, int i) {
    unsigned char sum = 0;

    while (i-- > 0)
        sum += *d++;

    return (0x100 - sum) & 0xFF;
}

void MEM_ResetPageHandler_Unmapped(Bitu phys_page, Bitu pages);

unsigned int dos_conventional_limit = 0;

Bitu MEM_ConventionalPages(void) {
    if (dos_conventional_limit == 0) return MEM_TotalPages();
    unsigned int x = dos_conventional_limit / 4u;
    if (x == 0) x = 1;
    if (x > MEM_TotalPages()) x = MEM_TotalPages();
    return x;
}

bool AdapterROM_Read(Bitu address,unsigned long *size) {
    unsigned char c[3];
    unsigned int i;

    if ((address & 0x1FF) != 0) {
        LOG(LOG_MISC,LOG_DEBUG)("AdapterROM_Read: Caller attempted ROM scan not aligned to 512-byte boundary");
        return false;
    }

    for (i=0;i < 3;i++)
        c[i] = mem_readb(address+i);

    if (c[0] == 0x55 && c[1] == 0xAA) {
        unsigned char chksum=0;
        *size = (unsigned long)c[2] * 512UL;
        for (i=0;i < (unsigned int)(*size);i++) chksum += mem_readb(address+i);
        if (chksum != 0) {
            LOG(LOG_MISC,LOG_WARN)("AdapterROM_Read: Found ROM at 0x%lx but checksum failed (got %02xh expect %02xh)\n",(unsigned long)address,chksum,0);
            return false;
        }

        return true;
    }

    return false;
}

static int bios_pc98_posx = 0;
extern bool tooutttf;

static void BIOS_Int10RightJustifiedPrint(const int x,int &y,const char *msg, bool boxdraw = false, bool tobold = false) {
    if (tooutttf) {
        tooutttf = false;
        change_output(10);
    }
    if (control->opt_fastlaunch) return;
    const char *s = msg;
    if (machine != MCH_PC98) {
        unsigned int bold = 0;
        while (*s != 0) {
            if (*s == '\n') {
                y++;
                reg_eax = 0x0200u;   // set cursor pos
                reg_ebx = 0;        // page zero
                reg_dh = y;     // row 4
                reg_dl = x;     // column 20
                CALLBACK_RunRealInt(0x10);
                s++;
            }
            else {
                if (tobold&&!bold) {
                    if ((strlen(s)>3&&!strncmp(s, "DEL", 3))||!strncmp(s, "ESC", 3)) bold = 3;
                    else if (strlen(s)>5&&!strncmp(s, "ENTER", 5)) bold = 5;
                    else if (strlen(s)>8&&!strncmp(s, "SPACEBAR", 8)) bold = 8;
                }
                if (bold>0) {
                    bold--;
                    reg_eax = 0x0900u | ((unsigned char)(*s++));
                    reg_ebx = 0x000fu;
                    reg_ecx = 0x0001u;
                    CALLBACK_RunRealInt(0x10);
                    reg_eax = 0x0300u;
                    reg_ebx = 0x0000u;
                    CALLBACK_RunRealInt(0x10);
                    reg_eax = 0x0200u;
                    reg_ebx = 0x0000u;
                    reg_edx++;
                    CALLBACK_RunRealInt(0x10);
                } else {
                    reg_eax = 0x0E00u | ((unsigned char)(*s++));
                    reg_ebx = 0x07u;
                    CALLBACK_RunRealInt(0x10);
                }
            }
        }
    }
    else {
        unsigned int bo;

        while (*s != 0) {
            if (*s == '\n') {
                y++;
                s++;
                bios_pc98_posx = x;

                bo = (((unsigned int)y * 80u) + (unsigned int)bios_pc98_posx) * 2u;
            }
            else if (*s == '\r') {
                s++; /* ignore */
                continue;
            }
            else {
                bo = (((unsigned int)y * 80u) + (unsigned int)(bios_pc98_posx++)) * 2u; /* NTS: note the post increment */
                if (boxdraw) {
                    unsigned int ch = (unsigned char)*s;
                    if (ch==0xcd) ch = 0x250B;
                    else if (ch==0xba) ch = 0x270B;
                    else if (ch==0xc9) ch = 0x330B;
                    else if (ch==0xbb) ch = 0x370B;
                    else if (ch==0xc8) ch = 0x3B0B;
                    else if (ch==0xbc) ch = 0x3F0B;
                    mem_writew(0xA0000+bo,ch);
                } else
                    mem_writew(0xA0000+bo,(unsigned char)*s);
                mem_writeb(0xA2000+bo,0xE1);

                s++;
                bo += 2; /* and keep the cursor following the text */
            }

            reg_eax = 0x1300;   // set cursor pos (PC-98)
            reg_edx = bo;       // byte position
            CALLBACK_RunRealInt(0x18);
        }
    }
}

char *getSetupLine(const char *capt, const char *cont) {
    unsigned int pad1=(unsigned int)(25-strlen(capt)), pad2=(unsigned int)(41-strlen(cont));
    static char line[90];
    sprintf(line, "\x0ba%*c%s%*c%s%*c\x0ba", 12, ' ', capt, pad1, ' ', cont, pad2, ' ');
    return line;
}

const char *GetCPUType();
void updateDateTime(int x, int y, int pos)
{
    (void)x;//UNUSED
    (void)y;//UNUSED
    char str[50];
    time_t curtime = time(NULL);
    struct tm *loctime = localtime (&curtime);
    Bitu time=(Bitu)((100.0/((double)PIT_TICK_RATE/65536.0)) * mem_readd(BIOS_TIMER))/100;
    unsigned int sec=(uint8_t)((Bitu)time % 60);
    time/=60;
    unsigned int min=(uint8_t)((Bitu)time % 60);
    time/=60;
    unsigned int hour=(uint8_t)((Bitu)time % 24);
    int val=0;
    unsigned int bo;
    Bitu edx=0, pdx=0x0500u;
    for (int i=1; i<7; i++) {
        switch (i) {
            case 1:
                val = machine==MCH_PC98?loctime->tm_year+1900:dos.date.year;
                reg_edx = 0x0326u;
                if (i==pos) pdx = reg_edx;
                break;
            case 2:
                val = machine==MCH_PC98?loctime->tm_mon+1:dos.date.month;
                reg_edx = 0x032bu;
                if (i==pos) pdx = reg_edx;
                break;
            case 3:
                val = machine==MCH_PC98?loctime->tm_mday:dos.date.day;
                reg_edx = 0x032eu;
                if (i==pos) pdx = reg_edx;
                break;
            case 4:
                val = machine==MCH_PC98?loctime->tm_hour:hour;
                reg_edx = 0x0426u;
                if (i==pos) pdx = reg_edx;
                break;
            case 5:
                val = machine==MCH_PC98?loctime->tm_min:min;
                reg_edx = 0x0429u;
                if (i==pos) pdx = reg_edx;
                break;
            case 6:
                val = machine==MCH_PC98?loctime->tm_sec:sec;
                reg_edx = 0x042cu;
                if (i==pos) pdx = reg_edx;
                break;
        }
        edx = reg_edx;
        sprintf(str, i==1?"%04u":"%02u",val);
        for (unsigned int j=0; j<strlen(str); j++) {
            if (machine == MCH_PC98) {
                bo = (((unsigned int)(edx/0x100) * 80u) + (unsigned int)(edx%0x100) + j) * 2u;
                mem_writew(0xA0000+bo,str[j]);
                mem_writeb(0xA2000+bo,0xE1);
            } else {
                reg_eax = 0x0200u;
                reg_ebx = 0x0000u;
                reg_edx = edx + j;
                CALLBACK_RunRealInt(0x10);
                reg_eax = 0x0900u+str[j];
                reg_ebx = i==pos?0x001fu:0x001eu;
                reg_ecx = 0x0001u;
                CALLBACK_RunRealInt(0x10);
            }
        }
    }
    if (pos) {
        sprintf(str, "%-30s", GetCPUType());
        for (unsigned int j=0; j<strlen(str); j++) {
            if (machine == MCH_PC98) {
                bo = ((0x0F * 80u) + (unsigned int)(0x26 + j)) * 2u;
                mem_writew(0xA0000+bo,str[j]);
                mem_writeb(0xA2000+bo,0xE1);
            } else {
                reg_eax = 0x0200u;
                reg_ebx = 0x0000u;
                reg_edx = 0x0F26u + j;
                CALLBACK_RunRealInt(0x10);
                reg_eax = 0x0900u+str[j];
                reg_ebx = 0x001eu;
                reg_ecx = 0x0001u;
                CALLBACK_RunRealInt(0x10);
            }
        }
        sprintf(str, "%-30s", (std::to_string(CPU_CycleAutoAdjust?CPU_CyclePercUsed:CPU_CycleMax)+(CPU_CycleAutoAdjust?"%":" cycles/ms")).c_str());
        for (unsigned int j=0; j<strlen(str); j++) {
            if (machine == MCH_PC98) {
                bo = ((0x10 * 80u) + (unsigned int)(0x26 + j)) * 2u;
                mem_writew(0xA0000+bo,str[j]);
                mem_writeb(0xA2000+bo,0xE1);
            } else {
                reg_eax = 0x0200u;
                reg_ebx = 0x0000u;
                reg_edx = 0x1026u + j;
                CALLBACK_RunRealInt(0x10);
                reg_eax = 0x0900u+str[j];
                reg_ebx = 0x001eu;
                reg_ecx = 0x0001u;
                CALLBACK_RunRealInt(0x10);
            }
        }
    }
    if (machine == MCH_PC98) {
        reg_eax = 0x1300;
        reg_edx = 0x1826;
        CALLBACK_RunRealInt(0x18);
    } else {
        reg_eax = 0x0200u;
        reg_ebx = 0x0000u;
        reg_edx = pdx;
        CALLBACK_RunRealInt(0x10);
    }
}

int oldcols = 0, oldlins = 0;
void showBIOSSetup(const char* card, int x, int y) {
    reg_eax = 3;        // 80x25 text
    CALLBACK_RunRealInt(0x10);
    if (machine == MCH_PC98) {
        for (unsigned int i=0;i < (80*400);i++) {
            mem_writeb(0xA8000+i,0);        // B
            mem_writeb(0xB0000+i,0);        // G
            mem_writeb(0xB8000+i,0);        // R
            mem_writeb(0xE0000+i,0);        // E
        }
        reg_eax = 0x1600;
        reg_edx = 0xE100;
        CALLBACK_RunRealInt(0x18);
        reg_eax = 0x1300;
        reg_edx = 0x0000;
        CALLBACK_RunRealInt(0x18);
        x = 0;
        y = 0;
    } else {
        reg_eax = 0x0200u;
        reg_ebx = 0x0000u;
        reg_edx = 0x0000u;
        CALLBACK_RunRealInt(0x10);
        reg_eax = 0x0600u;
        reg_ebx = 0x1e00u;
        reg_ecx = 0x0000u;
        reg_edx =
#if defined(USE_TTF)
        TTF_using()?(ttf.lins-1)*0x100+(ttf.cols-1):
#endif
        0x184Fu;
        CALLBACK_RunRealInt(0x10);
    }
#if defined(USE_TTF)
    if (TTF_using() && (ttf.cols != 80 || ttf.lins != 25)) ttf_setlines(80, 25);
#endif
    char title[]="                               BIOS Setup Utility                               ";
    char *p=machine == MCH_PC98?title+2:title;
    BIOS_Int10RightJustifiedPrint(x,y,p);
    BIOS_Int10RightJustifiedPrint(x,y,"\x0c9\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0bb", true);
    BIOS_Int10RightJustifiedPrint(x,y,getSetupLine("", ""), true);
    BIOS_Int10RightJustifiedPrint(x,y,getSetupLine("System date:", "0000-00-00"), true);
    BIOS_Int10RightJustifiedPrint(x,y,getSetupLine("System time:", "00:00:00"), true);
    updateDateTime(x,y,0);
    BIOS_Int10RightJustifiedPrint(x,y,getSetupLine("Installed OS:", "DOS"), true);
    BIOS_Int10RightJustifiedPrint(x,y,getSetupLine("", ""), true);
#define DOSNAMEBUF 256
    char pcname[DOSNAMEBUF];
    if (control->opt_securemode || control->SecureMode())
        strcpy(pcname, "N/A");
    else {
#if defined(WIN32)
        DWORD size = DOSNAMEBUF;
        GetComputerName(pcname, &size);
        if (!size)
#else
        int result = gethostname(pcname, DOSNAMEBUF);
        if (result)
#endif
            strcpy(pcname, "N/A");
    }
    BIOS_Int10RightJustifiedPrint(x,y,getSetupLine("Computer name:", pcname), true);
    BIOS_Int10RightJustifiedPrint(x,y,getSetupLine("Product name:", ("DOSBox-X "+std::string(VERSION)).c_str()), true);
    BIOS_Int10RightJustifiedPrint(x,y,getSetupLine("Product updated:", UPDATED_STR), true);
    BIOS_Int10RightJustifiedPrint(x,y,getSetupLine("", ""), true);
    BIOS_Int10RightJustifiedPrint(x,y,getSetupLine("BIOS description:", bios_type_string), true);
    BIOS_Int10RightJustifiedPrint(x,y,getSetupLine("BIOS version:", bios_version_string), true);
    uint32_t year,month,day;
    if (sscanf(bios_date_string,"%u/%u/%u",&month,&day,&year)==3) {
        char datestr[30];
        sprintf(datestr, "%04u-%02u-%02u",year<80?2000+year:(year<100?1900+year:year),month,day);
        BIOS_Int10RightJustifiedPrint(x,y,getSetupLine("BIOS date:", datestr), true);
    } else
        BIOS_Int10RightJustifiedPrint(x,y,getSetupLine("BIOS date:", bios_date_string), true);
    BIOS_Int10RightJustifiedPrint(x,y,getSetupLine("", ""), true);
    BIOS_Int10RightJustifiedPrint(x,y,getSetupLine("Processor type:", GetCPUType()), true);
    BIOS_Int10RightJustifiedPrint(x,y,getSetupLine("Processor speed:", (std::to_string(CPU_CycleAutoAdjust?CPU_CyclePercUsed:CPU_CycleMax)+(CPU_CycleAutoAdjust?"%":" cycles/ms")).c_str()), true);
    BIOS_Int10RightJustifiedPrint(x,y,getSetupLine("Coprocessor:", enable_fpu?"Yes":"No"), true);
    BIOS_Int10RightJustifiedPrint(x,y,getSetupLine("", ""), true);
    BIOS_Int10RightJustifiedPrint(x,y,getSetupLine("Video card:", card), true);
    BIOS_Int10RightJustifiedPrint(x,y,getSetupLine("Video memory:", (std::to_string(vga.mem.memsize/1024)+"K").c_str()), true);
    BIOS_Int10RightJustifiedPrint(x,y,getSetupLine("Total memory:", (std::to_string(MEM_TotalPages()*4096/1024)+"K").c_str()), true);
    BIOS_Int10RightJustifiedPrint(x,y,getSetupLine("", ""), true);
    BIOS_Int10RightJustifiedPrint(x,y,"\x0c8\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0cd\x0bc", true);
    if (machine == MCH_PC98)
        BIOS_Int10RightJustifiedPrint(x,y,"                                  ESC = Exit                                  ");
    else
        BIOS_Int10RightJustifiedPrint(x,y,"              ESC: Exit  Arrows: Select Item  +/-: Change Values              ");
}

static Bitu mlimit = 0;
static Bitu t_conv = 0;
static Bitu t_conv_real = 0;
static bool bios_first_init=true;
static bool bios_has_exec_vga_bios=false;
static Bitu adapter_scan_start;

/* FIXME: At global scope their destructors are called after the rest of DOSBox-X has shut down. Move back into BIOS scope. */
static CALLBACK_HandlerObject int4b_callback;
static const size_t callback_count = 20;
static CALLBACK_HandlerObject callback[callback_count]; /* <- fixme: this is stupid. just declare one per interrupt. */
static CALLBACK_HandlerObject cb_bios_post;
static CALLBACK_HandlerObject callback_pc98_lio;
static CALLBACK_HandlerObject callback_pc98_avspcm;

Bitu call_pnp_r = ~0UL;
Bitu call_pnp_rp = 0;

Bitu call_pnp_p = ~0UL;
Bitu call_pnp_pp = 0;

Bitu isapnp_biosstruct_base = 0;

Bitu BIOS_boot_code_offset = 0;
Bitu BIOS_bootfail_code_offset = 0;

bool bios_user_reset_vector_blob_run = false;
Bitu bios_user_reset_vector_blob = 0;

Bitu bios_user_boot_hook = 0;

void CALLBACK_DeAllocate(Bitu in);

void BIOS_OnResetComplete(Section *x);

Bitu call_irq0 = 0;
Bitu call_irq07default = 0;
Bitu call_irq815default = 0;


/* NEC PC-98 detection notes:
 *  - Documented: No ASCII date at F000:FFF5
 *  - CWSDPMI (PC-98 patched version): Call INT 10h AH=0F BH=FF. If registers don't change, it's PC-98
 *  - DJGPP libc, crt1.c (PC-98 patched version): If the WORD at F000:FFF3 (segment part of JMP FAR) is 0xFD80, it's PC-98 */

void write_FFFF_PC98_signature() {
    /* this may overwrite the existing signature.
     * PC-98 systems DO NOT have an ASCII date at F000:FFF5
     * and the WORD value at F000:FFFE is said to be a checksum of the BIOS */

    // The farjump at the processor reset entry point (jumps to POST routine)
    phys_writeb(0xffff0,0xEA);                  // FARJMP

    /* Segment value must be 0xFD80 to satisfy PC-98 patched DJGPP check */
    const uint16_t oseg = RealSeg(BIOS_DEFAULT_RESET_LOCATION);
    phys_writew(0xffff1,RealOff(BIOS_DEFAULT_RESET_LOCATION)-((0xFD80-oseg)*16));  // offset
    phys_writew(0xffff3,0xFD80);  // segment

    // write nothing (not used)
    for(Bitu i = 0; i < 9; i++) phys_writeb(0xffff5+i,0);

    // fake BIOS checksum
    phys_writew(0xffffe,0xABCD);
}

void gdc_egc_enable_update_vars(void) {
    unsigned char b;

    b = mem_readb(0x54D);
    b &= ~0x40;
    if (enable_pc98_egc) b |= 0x40;
    mem_writeb(0x54D,b);

    b = mem_readb(0x597);
    b &= ~0x04;
    if (enable_pc98_egc) b |= 0x04;
    mem_writeb(0x597,b);

    if (!enable_pc98_egc)
        pc98_gdc_vramop &= ~(1 << VOPBIT_EGC);
}

void gdc_grcg_enable_update_vars(void) {
    unsigned char b;

    b = mem_readb(0x54C);
    b &= ~0x02;
    if (enable_pc98_grcg) b |= 0x02;
    mem_writeb(0x54C,b);
    
    //TODO: How to reset GRCG?
}


void gdc_16color_enable_update_vars(void) {
    unsigned char b;

    b = mem_readb(0x54C);
    b &= ~0x05;
    if (enable_pc98_16color) b |= 0x05; // bit0 .. DIPSW 1-8 support GLIO 16-colors
    mem_writeb(0x54C,b);

    if(!enable_pc98_256color) {//force switch to 16-colors mode
        void pc98_port6A_command_write(unsigned char b);
        pc98_port6A_command_write(0x20);
    }
    if(!enable_pc98_16color) {//force switch to 8-colors mode
        void pc98_port6A_command_write(unsigned char b);
        pc98_port6A_command_write(0x00);
    }
}

uint32_t BIOS_get_PC98_INT_STUB(void) {
    return callback[18].Get_RealPointer();
}

Bitu call_pc98_default_stop;

extern bool DOS_BreakFlag;
extern bool DOS_BreakConioFlag;

static Bitu pc98_default_stop_handler(void) {
    // INT 06h, which means someone pressed the STOP key... or the CPU is signalling an invalid opcode.
    // The overlap makes it extremely unclear.
    LOG_MSG("Invalid opcode or unhandled PC-98 STOP key interrupt 06h");

    // try to make it work as CTRL+BREAK in the built-in DOS environment.
    if (!dos_kernel_disabled)
        DOS_BreakFlag = DOS_BreakConioFlag = true;

    return CBRET_NONE;
}

static unsigned char BCD2BIN(unsigned char x) {
	return ((x >> 4) * 10) + (x & 0xF);
}


/* NTS: Remember the 8259 is non-sentient, and the term "slave" is used in a computer programming context */
static Bitu Default_IRQ_Handler_Cooperative_Slave_Pic(void) {
    /* PC-98 style IRQ 8-15 handling.
     *
     * This mimics the recommended procedure [https://www.webtech.co.jp/company/doc/undocumented_mem/io_pic.txt]
     *
     *  mov al,20h      ;Send EOI to SLAVE
     *  out 0008h,al
     *  jmp $+2         ;I/O WAIT
     *  mov al,0Bh      ;ISR read mode set(slave)
     *  out 0008h,al
     *  jmp $+2         ;I/O WAIT
     *  in  al,0008h    ;ISR read(slave)
     *  cmp al,00h      ;slave pic in-service ?
     *  jne EoiEnd
     *  mov al,20h      ;Send EOI to MASTER
     *  out 0000h,al
     */
    IO_WriteB(IS_PC98_ARCH ? 0x08 : 0xA0,0x20); // send EOI to slave
    IO_WriteB(IS_PC98_ARCH ? 0x08 : 0xA0,0x0B); // ISR read mode set
    if (IO_ReadB(IS_PC98_ARCH ? 0x08 : 0xA0) == 0) // if slave pic in service..
        IO_WriteB(IS_PC98_ARCH ? 0x00 : 0x20,0x20); // then EOI the master

    return CBRET_NONE;
}

extern uint32_t tandy_128kbase;

static int bios_post_counter = 0;

extern void BIOSKEY_PC98_Write_Tables(void);
extern Bitu PC98_AVSDRV_PCM_Handler(void);

static unsigned int acpiptr2ofs(unsigned char *w) {
	return w - ACPI_buffer;
}

static PhysPt acpiofs2phys(unsigned int o) {
	return ACPI_BASE + o;
}

class ACPISysDescTableWriter {
public:
	ACPISysDescTableWriter();
	~ACPISysDescTableWriter(void);
public:
	ACPISysDescTableWriter &begin(unsigned char *w,unsigned char *f,size_t n_tablesize=36);
	ACPISysDescTableWriter &setRev(const unsigned char rev);
	ACPISysDescTableWriter &setOemID(const char *id);
	ACPISysDescTableWriter &setSig(const char *sig);
	ACPISysDescTableWriter &setOemTableID(const char *id);
	ACPISysDescTableWriter &setOemRev(const uint32_t rev);
	ACPISysDescTableWriter &setCreatorID(const uint32_t id);
	ACPISysDescTableWriter &setCreatorRev(const uint32_t rev);
	ACPISysDescTableWriter &expandto(size_t sz);
	unsigned char* getptr(size_t ofs=0,size_t sz=1);
	size_t get_tablesize(void) const;
	unsigned char* finish(void);
private:
	size_t				tablesize = 0;
	unsigned char*			base = NULL;
	unsigned char*			f = NULL;
};

size_t ACPISysDescTableWriter::get_tablesize(void) const {
	return tablesize;
}

ACPISysDescTableWriter::ACPISysDescTableWriter() {
}

ACPISysDescTableWriter::~ACPISysDescTableWriter(void) {
	if (tablesize != 0) LOG(LOG_MISC,LOG_ERROR)("ACPI table writer destructor called without completing a table");
}

ACPISysDescTableWriter &ACPISysDescTableWriter::begin(unsigned char *n_w,unsigned char *n_f,size_t n_tablesize) {
	if (tablesize != 0) LOG(LOG_MISC,LOG_ERROR)("ACPI table writer asked to begin a table without completing the last table");
	base = n_w;
	f = n_f;
	tablesize = n_tablesize;
	assert(tablesize >= 36);
	assert((base+tablesize) <= f);
	assert(base != NULL);
	assert(f != NULL);
	assert(base < f);

	memset(base,0,tablesize);
	memcpy(base+10,"DOSBOX",6); // OEM ID
	memcpy(base+16,"DOSBox-X",8); // OEM Table ID
	host_writed(base+24,1); // OEM revision
	memcpy(base+28,"DBOX",4); // Creator ID
	host_writed(base+32,1); // Creator revision

	return *this;
}

ACPISysDescTableWriter &ACPISysDescTableWriter::setRev(const unsigned char rev) {
	assert(base != NULL);
	assert(tablesize >= 36);
	base[8] = rev;
	return *this;
}

ACPISysDescTableWriter &ACPISysDescTableWriter::setOemID(const char *id) {
	assert(id != NULL);
	assert(base != NULL);
	assert(tablesize >= 36);
	unsigned char *wp = base+10;
	for (unsigned int i=0;i < 6;i++) {
		if (*id != 0)
			*wp++ = (unsigned char)(*id++);
		else
			*wp++ = ' ';
	}
	return *this;
}

ACPISysDescTableWriter &ACPISysDescTableWriter::setSig(const char *sig) {
	assert(sig != NULL);
	assert(base != NULL);
	assert(tablesize >= 36);
	unsigned char *wp = base;
	for (unsigned int i=0;i < 4;i++) {
		if (*sig != 0)
			*wp++ = (unsigned char)(*sig++);
		else
			*wp++ = ' ';
	}
	return *this;
}

ACPISysDescTableWriter &ACPISysDescTableWriter::setOemTableID(const char *id) {
	assert(id != NULL);
	assert(base != NULL);
	assert(tablesize >= 36);
	unsigned char *wp = base+16;
	for (unsigned int i=0;i < 8;i++) {
		if (*id != 0)
			*wp++ = (unsigned char)(*id++);
		else
			*wp++ = ' ';
	}
	return *this;
}

ACPISysDescTableWriter &ACPISysDescTableWriter::setOemRev(const uint32_t rev) {
	assert(base != NULL);
	assert(tablesize >= 36);
	host_writed(base+24,rev);
	return *this;
}

ACPISysDescTableWriter &ACPISysDescTableWriter::setCreatorID(const uint32_t id) {
	assert(base != NULL);
	assert(tablesize >= 36);
	host_writed(base+28,id);
	return *this;
}

ACPISysDescTableWriter &ACPISysDescTableWriter::setCreatorRev(const uint32_t rev) {
	assert(base != NULL);
	assert(tablesize >= 36);
	host_writed(base+32,rev);
	return *this;
}

ACPISysDescTableWriter &ACPISysDescTableWriter::expandto(size_t sz) {
	assert(base != NULL);
	assert((base+sz) <= f);
	if (tablesize < sz) tablesize = sz;
	return *this;
}

unsigned char* ACPISysDescTableWriter::getptr(size_t ofs,size_t sz) {
	assert(base != NULL);
	assert((base+ofs+sz) <= f);
	if (tablesize < (ofs+sz)) tablesize = ofs+sz;
	return base+ofs;
}

unsigned char *ACPISysDescTableWriter::finish(void) {
	if (base != NULL) {
		unsigned char *ret = base + tablesize;

		assert((base+tablesize) <= f);
		assert(tablesize >= 36);

		/* update length field */
		host_writed(base+4,tablesize);

		/* update checksum field */
		unsigned int i,c;
		base[9] = 0;
		c = 0; for (i=0;i < tablesize;i++) c += base[i];
		base[9] = (0 - c) & 0xFFu;

		base = f = NULL;
		tablesize = 0;
		return ret;
	}

	return NULL;
}

enum class ACPIRegionSpace {
	SystemMemory=0,
	SystemIO=1,
	PCIConfig=2,
	EmbeddedControl=3,
	SMBus=4
};

namespace ACPIMethodFlags {
	static constexpr unsigned char ArgCount(const unsigned c) {
		return c&3u;
	}
	enum {
		NotSerialized=(0 << 3),
		Serialized=(1 << 3)
	};
}

static constexpr unsigned int ACPIrtIO_16BitDecode = (1u << 0u);

static constexpr unsigned int ACPIrtMR24_Writeable = (1u << 0u);
static constexpr unsigned int ACPIrtMR32_Writeable = (1u << 0u);

namespace ACPIFieldFlag {
	namespace AccessType {
		enum {
			AnyAcc=0,
			ByteAcc=1,
			WordAcc=2,
			DwordAcc=3,
			BlockAcc=4,
			SMBSendRevAcc=5,
			SMBQuickAcc=6
		};
	}
	namespace LockRule {
		enum {
			NoLock=(0 << 4),
			Lock=(1 << 4)
		};
	}
	namespace UpdateRule {
		enum {
			Preserve=(0 << 5),
			WriteAsOnes=(1 << 5),
			WriteAsZeros=(2 << 5)
		};
	}
}

enum class ACPIAMLOpcode:unsigned char {
	ZeroOp = 0x00, // ACPI 1.0+
	OneOp = 0x01, // ACPI 1.0+

	AliasOp = 0x06, // ACPI 1.0+

	NameOp = 0x08, // ACPI 1.0+

	BytePrefix = 0x0A, // ACPI 1.0+
	WordPrefix = 0x0B, // ACPI 1.0+
	DwordPrefix = 0x0C, // ACPI 1.0+
	StringPrefix = 0x0D, // ACPI 1.0+
	QWordPrefix = 0x0E, // ACPI 2.0+

	ScopeOp = 0x10, // ACPI 1.0+
	BufferOP = 0x11, // ACPI 1.0+
	PackageOp = 0x12, // ACPI 1.0+
	VarPackageOp = 0x13, // ACPI 2.0+
	MethodOp = 0x14, // ACPI 1.0+
	ExternalOp = 0x15, // ACPI 6.0+

	DualNamePrefix = 0x2E, // ACPI 1.0+
	MultiNamePrefix = 0x2F, // ACPI 1.0+

	NameCharA = 0x41, // ACPI 1.0b+
	NameCharB = 0x42, // ACPI 1.0b+
	NameCharC = 0x43, // ACPI 1.0b+
	NameCharD = 0x44, // ACPI 1.0b+
	NameCharE = 0x45, // ACPI 1.0b+
	NameCharF = 0x46, // ACPI 1.0b+
	NameCharG = 0x47, // ACPI 1.0b+
	NameCharH = 0x48, // ACPI 1.0b+
	NameCharI = 0x49, // ACPI 1.0b+
	NameCharJ = 0x4A, // ACPI 1.0b+
	NameCharK = 0x4B, // ACPI 1.0b+
	NameCharL = 0x4C, // ACPI 1.0b+
	NameCharM = 0x4D, // ACPI 1.0b+
	NameCharN = 0x4E, // ACPI 1.0b+
	NameCharO = 0x4F, // ACPI 1.0b+
	NameCharP = 0x50, // ACPI 1.0b+
	NameCharQ = 0x51, // ACPI 1.0b+
	NameCharR = 0x52, // ACPI 1.0b+
	NameCharS = 0x53, // ACPI 1.0b+
	NameCharT = 0x54, // ACPI 1.0b+
	NameCharU = 0x55, // ACPI 1.0b+
	NameCharV = 0x56, // ACPI 1.0b+
	NameCharW = 0x57, // ACPI 1.0b+
	NameCharX = 0x58, // ACPI 1.0b+
	NameCharY = 0x59, // ACPI 1.0b+
	NameCharZ = 0x5A, // ACPI 1.0b+

	ExtendedOperatorPrefix = 0x5B, // ACPI 1.0+
	RootNamePrefix = 0x5C, // ACPI 1.0+

	ParentNamePrefix = 0x5E, // ACPI 1.0+
	NameChar_ = 0x5F, // ACPI 2.0+

	Local0 = 0x60, // ACPI 1.0+
	Local1 = 0x61, // ACPI 1.0+
	Local2 = 0x62, // ACPI 1.0+
	Local3 = 0x63, // ACPI 1.0+
	Local4 = 0x64, // ACPI 1.0+
	Local5 = 0x65, // ACPI 1.0+
	Local6 = 0x66, // ACPI 1.0+
	Local7 = 0x67, // ACPI 1.0+
	Arg0 = 0x68, // ACPI 1.0+
	Arg1 = 0x69, // ACPI 1.0+
	Arg2 = 0x6A, // ACPI 1.0+
	Arg3 = 0x6B, // ACPI 1.0+
	Arg4 = 0x6C, // ACPI 1.0+
	Arg5 = 0x6D, // ACPI 1.0+
	Arg6 = 0x6E, // ACPI 1.0+

	StoreOp = 0x70, // ACPI 1.0+
	RefOfOp = 0x71, // ACPI 1.0+
	AddOp = 0x72, // ACPI 1.0+
	ConcatOp = 0x73, // ACPI 1.0+
	SubtractOp = 0x74, // ACPI 1.0+
	IncrementOp = 0x75, // ACPI 1.0+
	DecrementOp = 0x76, // ACPI 1.0+
	MultiplyOp = 0x77, // ACPI 1.0+
	DivideOp = 0x78, // ACPI 1.0+
	ShiftLeftOp = 0x79, // ACPI 1.0+
	ShiftRightOp = 0x7A, // ACPI 1.0+
	AndOp = 0x7B, // ACPI 1.0+
	NAndOp = 0x7C, // ACPI 1.0+
	OrOp = 0x7D, // ACPI 1.0+
	NOrOp = 0x7E, // ACPI 1.0+
	XOrOp = 0x7F, // ACPI 1.0+
	NotOp = 0x80, // ACPI 1.0+
	FindSetLeftBitOp = 0x81, // ACPI 1.0+
	FindSetRightBitOp = 0x82, // ACPI 1.0+
	DerefOfOp = 0x83, // ACPI 2.0+
	ConcatResOp = 0x84, // ACPI 2.0+
	ModOp = 0x85, // ACPI 2.0+
	NotifyOp = 0x86, // ACPI 1.0+
	SizeOfOp = 0x87, // ACPI 1.0+
	IndexOp = 0x88, // ACPI 1.0+
	MatchOp = 0x89, // ACPI 1.0+
	DWordFieldOp = 0x8A, // ACPI 1.0+
	CreateDWordFieldOp = 0x8A, // ACPI 1.0b+
	WordFieldOp = 0x8B, // ACPI 1.0+
	CreateWordFieldOp = 0x8B, // ACPI 1.0b+
	ByteFieldOp = 0x8C, // ACPI 1.0+
	CreateByteFieldOp = 0x8C, // ACPI 1.0b+
	BitFieldOp = 0x8D, // ACPI 1.0+
	CreateBitFieldOp = 0x8D, // ACPI 1.0b+
	ObjTypeOp = 0x8E, // ACPI 1.0+
	CreateQWordField = 0x8F, // ACPI 2.0+
	LAndOp = 0x90, // ACPI 1.0+
	LOrOp = 0x91, // ACPI 1.0+
	LNotOp = 0x92, // ACPI 1.0+
	LEQOp = 0x93, // ACPI 1.0+
	LEqualOp = 0x93, // ACPI 1.0b+ same as LEQOp obviously to make opcode name clearer
/*	LNotEQOp = 0x93 0x92 */ // ACPI 1.0, seems to be an error in the documentation as that is LEqualOp LNotOp which doesn't make sense
/*	LNotEqualOp = 0x92 0x93 */ // ACPI 1.0b+, correction of opcode and to make opcode name clearer. Literally LNotOp LEqualOp
	LGOp = 0x94, // ACPI 1.0+
	LGreaterOp = 0x94, // ACPI 1.0b+ same as LGOp obviously to make opcode name clearer
/*	LLEQOp = 0x94 0x92 */ // ACPI 1.0, seems to be an error in the documentation as that is LEqualOp LNotOp which doesn't make sense
/*	LLessEqualOp = 0x92 0x94 */ // ACPI 1.0b+, correction of opcode and to make opcode name clearer. Literally LNotOp LGreaterOp
	LLOp = 0x95, // ACPI 1.0+
	LLessOp = 0x95, // ACPI 1.0b+ same as LLOp obviously to make opcode name clearer
/*	LGEQOp = 0x95 0x92 */ // ACPI 1.0, seems to be an error in the documentation as that is LEqualOp LNotOp which doesn't make sense
/*	LGreaterEqualOp = 0x95 0x92 */
	// ^ ACPI 1.0b+, um... they kept the same mistake, but does make opcode name clearer, but the definition does correctly say LNotOp LLessOp.
	// ^ Um... in fact ACPI 2.0 keeps the mistake and the corrected definition! They didn't fix THAT error until ACPI 3.0!
	// ^ Would mistakes like this have anything to do with the Linux kernel reportedly not wanting to support any ACPI BIOS made before the year 2000?
/*	LGreaterEqualOp = 0x92 0x95 */ // ACPI 3.0+ corrected byte pattern. Literally LNotOp LLessOp
	BuffOp = 0x96, // ACPI 2.0+
	ToBufferOp = 0x96, // ACPI 2.0a+, correction of opcode and to make opcode name clearer
	DecStrOp = 0x97, // ACPI 2.0+
	ToDecimalStringOp = 0x97, // ACPI 2.0a+, correction of opcode and to make opcode name clearer
	HexStrOp = 0x98, // ACPI 2.0+
	ToHexStringOp = 0x98, // ACPI 2.0a+, correction of opcode and to make opcode name clearer
	IntOp = 0x99, // ACPI 2.0+
	ToIntegerOp = 0x99, // ACPI 2.0a+, correction of opcode and to make opcode name clearer

	StringOp = 0x9C, // ACPI 2.0+
	ToStringOp = 0x9C, // ACPI 2.0a+, correction of opcode and to make opcode name clearer
	CopyOp = 0x9D, // ACPI 2.0+
	CopyObjectOp = 0x9D, // ACPI 2.0a+, correction of opcode and to make opcode name clearer
	MidOp = 0x9E, // ACPI 2.0+
	ContinueOp = 0x9F, // ACPI 2.0+
	IfOp = 0xA0, // ACPI 1.0+
	ElseOp = 0xA1, // ACPI 1.0+
	WhileOp = 0xA2, // ACPI 1.0+
	NoOp = 0xA3, // ACPI 1.0+
	ReturnOp = 0xA4, // ACPI 1.0+
	BreakOp = 0xA5, // ACPI 1.0+

	BreakPointOp = 0xCC, // ACPI 1.0+

	OnesOp = 0xFF // ACPI 1.0+
};

enum class ACPIAMLOpcodeEOP5B:unsigned char {
	/*0x5B*/MutexOp = 0x01, // ACPI 1.0+
	/*0x5B*/EventOp = 0x02, // ACPI 1.0+

	/*0x5B*/ShiftRightBitOp = 0x10, // ACPI 1.0 only, disappeared 1.0b
	/*0x5B*/ShiftLeftBitOp = 0x11, // ACPI 1.0 only, disappeared 1.0b
	/*0x5B*/CondRefOp = 0x12, // ACPI 1.0+
	/*0x5B*/CreateFieldOp = 0x13, // ACPI 1.0+

	/*0x5B*/LocalTableOp = 0x1F, // ACPI 2.0+
	/*0x5B*/LoadOp = 0x20, // ACPI 1.0+
	/*0x5B*/StallOp = 0x21, // ACPI 1.0+
	/*0x5B*/SleepOp = 0x22, // ACPI 1.0+
	/*0x5B*/AcquireOp = 0x23, // ACPI 1.0+
	/*0x5B*/SignalOp = 0x24, // ACPI 1.0+
	/*0x5B*/WaitOp = 0x25, // ACPI 1.0+
	/*0x5B*/ResetOp = 0x26, // ACPI 1.0+
	/*0x5B*/ReleaseOp = 0x27, // ACPI 1.0+
	/*0x5B*/FromBCDOp = 0x28, // ACPI 1.0+
	/*0x5B*/ToBCD = 0x29, // ACPI 1.0+
	/*0x5B*/UnloadOp = 0x2A, // ACPI 1.0+

	/*0x5B*/RevisionOp = 0x30, // ACPI 1.0b+
	/*0x5B*/DebugOp = 0x31, // ACPI 1.0+
	/*0x5B*/FatalOp = 0x32, // ACPI 1.0+
	/*0x5B*/TimerOp = 0x33, // ACPI 3.0+

	/*0x5B*/OpRegionOp = 0x80, // ACPI 1.0+
	/*0x5B*/FieldOp = 0x81, // ACPI 1.0+
	/*0x5B*/DeviceOp = 0x82, // ACPI 1.0+
	/*0x5B*/ProcessorOp = 0x83, // ACPI 1.0+
	/*0x5B*/PowerResOp = 0x84, // ACPI 1.0+
	/*0x5B*/ThermalZoneOp = 0x85, // ACPI 1.0+
	/*0x5B*/IndexFieldOp = 0x86, // ACPI 1.0+
	/*0x5B*/BankFieldOp = 0x87, // ACPI 1.0+
	/*0x5B*/DataRegionOp = 0x88 // ACPI 2.0+
};

#include <stack>

/* ACPI AML (ACPI Machine Language) writer.
 * See also ACPI Specification 1.0b [http://hackipedia.org/browse.cgi/Computer/Platform/PC%2c%20IBM%20compatible/BIOS/ACPI%2c%20Advanced%20Configuration%20and%20Power%20Interface/Advanced%20Configuration%20and%20Power%20Interface%20Specification%20%281999%2d02%2d02%29%20v1%2e0b%2epdf].
 *
 * WARNING: The 1.0 specification [http://hackipedia.org/browse.cgi/Computer/Platform/PC%2c%20IBM%20compatible/BIOS/ACPI%2c%20Advanced%20Configuration%20and%20Power%20Interface/Advanced%20Configuration%20and%20Power%20Interface%20Specification%20%281996%2d12%2d22%29%20v1%2e0%2epdf] seems to have some mistakes in a few opcodes in how they are defined, which probably means if your BIOS is from 1996-1998 it might have those few erroneous AML opcodes. */
class ACPIAMLWriter {
	public:
		static constexpr unsigned int MaxPkgSize = 0xFFFFFFFu;
	public:
		struct pkg_t {
			unsigned char*	pkg_len = NULL;
			unsigned char*	pkg_data = NULL;
			unsigned int	element_count = 0;
		};
		std::stack<pkg_t> pkg_stack;
	public:
		ACPIAMLWriter();
		~ACPIAMLWriter();
	public:
		unsigned char* writeptr(void) const;
		void begin(unsigned char *n_w,unsigned char *n_f);
	public:
		ACPIAMLWriter &rtDMA(const unsigned char bitmask,const unsigned char flags);
		ACPIAMLWriter &rtMemRange24(const unsigned int flags,const unsigned int minr,const unsigned int maxr,const unsigned int alignr,const unsigned int rangr);
		ACPIAMLWriter &rtMemRange32(const unsigned int flags,const unsigned int minr,const unsigned int maxr,const unsigned int alignr,const unsigned int rangr);
		ACPIAMLWriter &rtIO(const unsigned int flags,const uint16_t minport,const uint16_t maxport,const uint8_t alignment,const uint8_t rlength);
		ACPIAMLWriter &rtIRQ(const uint16_t bitmask/*bits [15:0] correspond to IRQ 15-0*/,const bool pciStyle=false);
		ACPIAMLWriter &rtHdrSmall(const unsigned char itemName,const unsigned int length);
		ACPIAMLWriter &rtHdrLarge(const unsigned char itemName,const unsigned int length);
		ACPIAMLWriter &rtBegin(void);
		ACPIAMLWriter &rtEnd(void);
	public:
		ACPIAMLWriter &NameOp(const char *name);
		ACPIAMLWriter &ByteOp(const unsigned char v);
		ACPIAMLWriter &WordOp(const unsigned int v);
		ACPIAMLWriter &DwordOp(const unsigned long v);
		ACPIAMLWriter &StringOp(const char *str);
		ACPIAMLWriter &OpRegionOp(const char *name,const ACPIRegionSpace regionspace);
		ACPIAMLWriter &FieldOp(const char *name,const unsigned int pred_size,const unsigned int fieldflag);
		ACPIAMLWriter &FieldOpEnd(void);
		ACPIAMLWriter &ScopeOp(const unsigned int pred_size=MaxPkgSize);
		ACPIAMLWriter &ScopeOpEnd(void);
		ACPIAMLWriter &PackageOp(const unsigned int pred_size=MaxPkgSize);
		ACPIAMLWriter &RootCharScopeOp(void);
		ACPIAMLWriter &PackageOpEnd(void);
		ACPIAMLWriter &RootCharOp(void);
		ACPIAMLWriter &NothingOp(void);
		ACPIAMLWriter &ZeroOp(void);
		ACPIAMLWriter &OneOp(void);
		ACPIAMLWriter &AliasOp(const char *what,const char *to_what);
		ACPIAMLWriter &BufferOpEnd(void);
		ACPIAMLWriter &BufferOp(const unsigned int pred_size=MaxPkgSize);
		ACPIAMLWriter &BufferOp(const unsigned char *data,const size_t datalen);
		ACPIAMLWriter &DeviceOp(const char *name,const unsigned int pred_size=MaxPkgSize);
		ACPIAMLWriter &DeviceOpEnd(void);
		ACPIAMLWriter &MethodOp(const char *name,const unsigned int pred_size,const unsigned int methodflags);
		ACPIAMLWriter &MethodOpEnd(void);
		ACPIAMLWriter &ReturnOp(void);
		ACPIAMLWriter &IfOp(const unsigned int pred_size=MaxPkgSize);
		ACPIAMLWriter &IfOpEnd(void);
		ACPIAMLWriter &ElseOp(const unsigned int pred_size=MaxPkgSize);
		ACPIAMLWriter &ElseOpEnd(void);
		ACPIAMLWriter &LEqualOp(void);
		ACPIAMLWriter &LNotEqualOp(void);
		ACPIAMLWriter &LNotOp(void);
		ACPIAMLWriter &LAndOp(void);
		ACPIAMLWriter &AndOp(void);
		ACPIAMLWriter &ArgOp(const unsigned int arg); /* Arg0 through Arg6 */
		ACPIAMLWriter &LocalOp(const unsigned int l); /* Local0 through Local7 */
		ACPIAMLWriter &StoreOp(void);
		ACPIAMLWriter &NOrOp(void);
		ACPIAMLWriter &OrOp(void);
		ACPIAMLWriter &NAndOp(void);
	public:// ONLY for writing fields!
		ACPIAMLWriter &FieldOpElement(const char *name,const unsigned int bits);
	public:
		ACPIAMLWriter &PkgLength(const unsigned int len,unsigned char* &wp,const unsigned int minlen=1);
		ACPIAMLWriter &PkgLength(const unsigned int len,const unsigned int minlen=1);
		ACPIAMLWriter &Name(const char *name);
		ACPIAMLWriter &MultiNameOp(void);
		ACPIAMLWriter &DualNameOp(void);
		ACPIAMLWriter &BeginPkg(const unsigned int pred_length=MaxPkgSize);
		ACPIAMLWriter &EndPkg(void);
		ACPIAMLWriter &CountElement(void);
	private:
		unsigned char*		w=NULL,*f=NULL;
		unsigned char*		buffer_len_pl = NULL;
		unsigned char*		rt_start = NULL;
};

/* StoreOp Operand Supername: Store Operand into Supername */
ACPIAMLWriter &ACPIAMLWriter::StoreOp(void) {
	*w++ = 0x70;
	return *this;
}

ACPIAMLWriter &ACPIAMLWriter::LocalOp(const unsigned int l) {
	if (l <= 7)
		*w++ = 0x60 + l; /* 0x60..0x67 -> Local0..Local7 */
	else
		E_Exit("ACPI AML writer LocalOp out of range");

	return *this;
}

ACPIAMLWriter &ACPIAMLWriter::ArgOp(const unsigned int arg) {
	if (arg <= 6)
		*w++ = 0x68 + arg; /* 0x68..0x6E -> Arg0..Arg6 */
	else
		E_Exit("ACPI AML writer ArgOp out of range");

	return *this;
}

/* Binary operators like And and Xor are Operand1 Operand2 Target, and the return value
 * of the operator is the result. What the ACPI specification is very unclear about, but
 * hints at from a sample bit of ASL concerning PowerResource(), is that if you just
 * want to evaluate the operator and do not care to store the result anywhere you can just
 * set Target to Zero.
 *
 * This example doesn't make sense unless you consider that this is how you encode "Nothing"
 * in the example on that page in spec 1.0b:
 *
 * Method(_STA) {
 *   Return (Xor (GIO.IDEI, One, Zero)) // inverse of isolation
 * }
 *
 * See what they did there? */
ACPIAMLWriter &ACPIAMLWriter::RootCharOp(void) {
	*w++ = '\\';
	return *this;
}

ACPIAMLWriter &ACPIAMLWriter::RootCharScopeOp(void) {
	RootCharOp(); /* this is how iasl encodes for example Scope(\) */
	ZeroOp();
	return *this;
}

ACPIAMLWriter &ACPIAMLWriter::NothingOp(void) {
	ZeroOp();
	return *this;
}

ACPIAMLWriter &ACPIAMLWriter::ZeroOp(void) {
	*w++ = 0x00;
	return *this;
}

ACPIAMLWriter &ACPIAMLWriter::OneOp(void) {
	*w++ = 0x01;
	return *this;
}

/* LEqual Operand1 Operand2 */
ACPIAMLWriter &ACPIAMLWriter::LEqualOp(void) {
	*w++ = 0x93;
	return *this;
}

ACPIAMLWriter &ACPIAMLWriter::LNotOp(void) {
	*w++ = 0x92;
	return *this;
}

/* LAndOp Operand1 Operand2 == Operand1 && Operand2 */
ACPIAMLWriter &ACPIAMLWriter::LAndOp(void) {
	*w++ = 0x90;
	return *this;
}

/* NAndOp Operand1 Operand2 Target -> Target = Operand1 & Operand2 */
ACPIAMLWriter &ACPIAMLWriter::NAndOp(void) {
	*w++ = 0x7C;
	return *this;
}

/* AndOp Operand1 Operand2 Target -> Target = Operand1 & Operand2 */
ACPIAMLWriter &ACPIAMLWriter::AndOp(void) {
	*w++ = 0x7B;
	return *this;
}

/* NOrOp Operand1 Operand2 Target -> Target = Operand1 & Operand2 */
ACPIAMLWriter &ACPIAMLWriter::NOrOp(void) {
	*w++ = 0x7E;
	return *this;
}

/* OrOp Operand1 Operand2 Target -> Target = Operand1 & Operand2 */
ACPIAMLWriter &ACPIAMLWriter::OrOp(void) {
	*w++ = 0x7D;
	return *this;
}

/* This makes sense if you think of an AML interpreter as something which encounters a LNotOp()
 * and then runs the interpreter to parse the following token(s) to evaluate an int so it can
 * do a logical NOT on the result of the evaluation. In other words this isn't like x86 assembly
 * which you follow instruction by instruction but more like how you parse and evaluate expressions
 * such as "4+5*3" properly. */
ACPIAMLWriter &ACPIAMLWriter::LNotEqualOp(void) {
	LNotOp();
	LEqualOp();
	return *this;
}

ACPIAMLWriter &ACPIAMLWriter::BufferOp(const unsigned char *data,const size_t datalen) {
	/* Notice this OP was obviously invented by the Department of Redundant Redundancy somewhere deep within Microsoft.
	 * This op stores both a PkgLength containing the overall buffer data and then the first bytes are a ByteOp encoding the length of the buffer.
	 * So basically it stores the length twice. What? Why? */
	*w++ = 0x11;
	BeginPkg(datalen+8/*Byte/Word/DwordOp*/);
	if (datalen >= 0x10000) DwordOp(datalen);
	else if (datalen >= 0x100) WordOp(datalen);
	else ByteOp(datalen);
	if (datalen > 0) {
		memcpy(w,data,datalen);
		w += datalen;
	}
	EndPkg();
	return *this;
}

ACPIAMLWriter &ACPIAMLWriter::BufferOp(const unsigned int pred_size) {
	assert(pred_size >= 10);
	*w++ = 0x11;
	BeginPkg(pred_size);
	DwordOp(0); // placeholder
	buffer_len_pl = w - 4;
	return *this;
}

ACPIAMLWriter &ACPIAMLWriter::BufferOpEnd(void) {
	assert(buffer_len_pl != NULL);
	host_writed(buffer_len_pl,size_t(w - (buffer_len_pl + 4)));
	buffer_len_pl = NULL;
	EndPkg();
	return *this;
}

ACPIAMLWriter &ACPIAMLWriter::AliasOp(const char *what,const char *to_what) {
	*w++ = 0x06;
	Name(what);
	Name(to_what);
	return *this;
}

ACPIAMLWriter &ACPIAMLWriter::ReturnOp(void) {
	*w++ = 0xA4;
	return *this;
}

ACPIAMLWriter &ACPIAMLWriter::IfOp(const unsigned int pred_size) {
	*w++ = 0xA0;
	BeginPkg(pred_size);
	return *this;
}

ACPIAMLWriter &ACPIAMLWriter::IfOpEnd(void) {
	EndPkg();
	return *this;
}

ACPIAMLWriter &ACPIAMLWriter::ElseOp(const unsigned int pred_size) {
	*w++ = 0xA1;
	BeginPkg(pred_size);
	return *this;
}

ACPIAMLWriter &ACPIAMLWriter::ElseOpEnd(void) {
	EndPkg();
	return *this;
}

ACPIAMLWriter &ACPIAMLWriter::rtHdrLarge(const unsigned char itemName,const unsigned int length) {
	assert(length <= 65536);
	assert(itemName < 128);
	*w++ = 0x80 + itemName;
	host_writew(w,length); w += 2;
	return *this;
}

ACPIAMLWriter &ACPIAMLWriter::rtHdrSmall(const unsigned char itemName,const unsigned int length) {
	assert(length < 8);
	assert(itemName < 16);
	*w++ = (itemName << 3) + length;
	return *this;
}

ACPIAMLWriter &ACPIAMLWriter::rtBegin(void) {
	rt_start = w;
	return *this;
}

ACPIAMLWriter &ACPIAMLWriter::rtEnd(void) {
	rtHdrSmall(15/*end tag format*/,1/*length*/);
	if (rt_start != NULL) {
		unsigned char sum = 0;
		for (unsigned char *s=rt_start;s < w;s++) sum += *s++;
		*w++ = 0x100 - sum;
	}
	else {
		*w++ = 0;
	}
	return *this;
}

ACPIAMLWriter &ACPIAMLWriter::rtMemRange24(const unsigned int flags,const unsigned int minr,const unsigned int maxr,const unsigned int alignr,const unsigned int rangr) {
	rtHdrLarge(1/*24-bit memory range format*/,9/*length*/);
	*w++ = flags;
	host_writew(w,minr >> 8u); w += 2;
	host_writew(w,maxr >> 8u); w += 2;
	host_writew(w,(alignr + 0xFFu) >> 8u); w += 2; /* FIXME: Um... alignment in bytes but everything else multiple of 256 bytes? */
	host_writew(w,rangr >> 8u); w += 2;
	return *this;
}

ACPIAMLWriter &ACPIAMLWriter::rtMemRange32(const unsigned int flags,const unsigned int minr,const unsigned int maxr,const unsigned int alignr,const unsigned int rangr) {
	rtHdrLarge(5/*32-bit memory range format*/,17/*length*/);
	*w++ = flags;
	host_writed(w,minr); w += 4;
	host_writed(w,maxr); w += 4;
	host_writed(w,alignr); w += 4;
	host_writed(w,rangr); w += 4;
	return *this;
}

ACPIAMLWriter &ACPIAMLWriter::rtDMA(const unsigned char bitmask,const unsigned char flags) {
	rtHdrSmall(5/*DMA format*/,2/*length*/);
	*w++ = bitmask;
	*w++ = flags;
	return *this;
}

ACPIAMLWriter &ACPIAMLWriter::rtIO(const unsigned int flags,const uint16_t minport,const uint16_t maxport,const uint8_t alignment,const uint8_t rlength) {
	rtHdrSmall(8/*IO format*/,7/*length*/);
	*w++ = (unsigned char)flags;
	host_writew(w,minport); w += 2;
	host_writew(w,maxport); w += 2;
	*w++ = alignment;
	*w++ = rlength;
	return *this;
}

ACPIAMLWriter &ACPIAMLWriter::rtIRQ(const uint16_t bitmask,const bool pciStyle) {
	rtHdrSmall(4/*IRQ format*/,3/*length*/);
	host_writew(w,bitmask); w += 2;
	*w++ = pciStyle ? 0x18/*active low level trigger shareable*/ : 0x01/*active high edge trigger*/;
	return *this;
}

ACPIAMLWriter &ACPIAMLWriter::NameOp(const char *name) {
	*w++ = 0x08; // NameOp
	Name(name);
	return *this;
}

ACPIAMLWriter &ACPIAMLWriter::Name(const char *name) {
	for (unsigned int i=0;i < 4;i++) {
		if (*name) *w++ = *name++;
		else *w++ = '_';
	}

	return *this;
}

ACPIAMLWriter &ACPIAMLWriter::MultiNameOp(void) {
	*w++ = 0x2F; // MultiNamePrefix
	return *this;
}

ACPIAMLWriter &ACPIAMLWriter::DualNameOp() {
	*w++ = 0x2E; // DualNamePrefix
	return *this;
}

ACPIAMLWriter &ACPIAMLWriter::ByteOp(const unsigned char v) {
	*w++ = 0x0A; // ByteOp
	*w++ = v;
	return *this;
}

ACPIAMLWriter &ACPIAMLWriter::WordOp(const unsigned int v) {
	*w++ = 0x0B; // WordOp
	host_writew(w,v); w += 2;
	return *this;
}

ACPIAMLWriter &ACPIAMLWriter::DwordOp(const unsigned long v) {
	*w++ = 0x0C; // DwordOp
	host_writed(w,v); w += 4;
	return *this;
}

ACPIAMLWriter &ACPIAMLWriter::StringOp(const char *str) {
	/* WARNING: Strings are only supposed to have ASCII 0x01-0x7F */
	*w++ = 0x0D; // StringOp
	while (*str != 0) *w++ = *str++;
	*w++ = 0x00;
	return *this;
}

ACPIAMLWriter &ACPIAMLWriter::OpRegionOp(const char *name,const ACPIRegionSpace regionspace) {
	*w++ = 0x5B;
	*w++ = 0x80;
	Name(name);
	*w++ = (unsigned char)regionspace;
	// and then the caller must write the RegionAddress and RegionLength
	return *this;
}

ACPIAMLWriter &ACPIAMLWriter::DeviceOp(const char *name,const unsigned int pred_size) {
	*w++ = 0x5B;
	*w++ = 0x82;
	BeginPkg(pred_size);
	Name(name);
	return *this;
}

ACPIAMLWriter &ACPIAMLWriter::DeviceOpEnd(void) {
	EndPkg();
	return *this;
}

ACPIAMLWriter &ACPIAMLWriter::MethodOp(const char *name,const unsigned int pred_size,const unsigned int methodflags) {
	*w++ = 0x14;
	BeginPkg(pred_size);
	Name(name);
	*w++ = (unsigned char)methodflags;
	return *this;
}

ACPIAMLWriter &ACPIAMLWriter::MethodOpEnd(void) {
	EndPkg();
	return *this;
}

ACPIAMLWriter &ACPIAMLWriter::FieldOp(const char *name,const unsigned int pred_size,const unsigned int fieldflag) {
	*w++ = 0x5B;
	*w++ = 0x81;
	BeginPkg(pred_size);
	Name(name);
	*w++ = fieldflag;
	return *this;
}

ACPIAMLWriter &ACPIAMLWriter::FieldOpEnd(void) {
	EndPkg();
	return *this;
}

ACPIAMLWriter &ACPIAMLWriter::ScopeOp(const unsigned int pred_size) {
	*w++ = 0x10;
	BeginPkg(pred_size);
	return *this;
}

ACPIAMLWriter &ACPIAMLWriter::ScopeOpEnd(void) {
	EndPkg();
	return *this;
}

ACPIAMLWriter &ACPIAMLWriter::PackageOp(const unsigned int pred_size) {
	*w++ = 0x12;
	BeginPkg(pred_size);
	*w++ = 0x00; // placeholder for element count
	return *this;
}

ACPIAMLWriter &ACPIAMLWriter::PackageOpEnd(void) {
	assert(!pkg_stack.empty());

	pkg_t &ent = pkg_stack.top();

	if (ent.element_count > 255u) E_Exit("ACPI AML writer too many elements in package");
	*ent.pkg_data = ent.element_count; /* element count follows PkgLength */

	EndPkg();
	return *this;
}

ACPIAMLWriter &ACPIAMLWriter::PkgLength(const unsigned int len,const unsigned int minlen) {
	return PkgLength(len,w,minlen);
}

ACPIAMLWriter &ACPIAMLWriter::PkgLength(const unsigned int len,unsigned char* &wp,const unsigned int minlen) {
	if (len >= 0x10000000 || minlen > 4) {
		E_Exit("ACPI AML writer PkgLength value too large");
	}
	else if (len >= 0x100000 || minlen >= 4) {
		*wp++ = (unsigned char)( len        & 0x0F) | 0xC0;
		*wp++ = (unsigned char)((len >>  4) & 0xFF);
		*wp++ = (unsigned char)((len >> 12) & 0xFF);
		*wp++ = (unsigned char)((len >> 20) & 0xFF);
	}
	else if (len >= 0x1000 || minlen >= 3) {
		*wp++ = (unsigned char)( len        & 0x0F) | 0x80;
		*wp++ = (unsigned char)((len >>  4) & 0xFF);
		*wp++ = (unsigned char)((len >> 12) & 0xFF);
	}
	else if (len >= 0x40 || minlen >= 2) {
		*wp++ = (unsigned char)( len        & 0x0F) | 0x40;
		*wp++ = (unsigned char)((len >>  4) & 0xFF);
	}
	else {
		*wp++ = (unsigned char)len;
	}

	return *this;
}

ACPIAMLWriter &ACPIAMLWriter::FieldOpElement(const char *name,const unsigned int bits) {
	if (*name != 0)
		Name(name);
	else
		*w++ = 0;

	PkgLength(bits);
	return *this;
}

ACPIAMLWriter &ACPIAMLWriter::BeginPkg(const unsigned int /*pred_length*/) {
	pkg_t ent;

	/* WARNING: Specify a size large enough. Once written, it cannot be extended if
	 *          needed. By default, this code writes an overlarge field to make sure
	 *          it can always update */

	if (pkg_stack.size() >= 32) E_Exit("ACPI AML writer BeginPkg too much recursion");

	ent.pkg_len = w;
	PkgLength(MaxPkgSize);//placeholder
	ent.pkg_data = w;

	pkg_stack.push(std::move(ent));
	return *this;
}

ACPIAMLWriter &ACPIAMLWriter::EndPkg(void) {
	if (pkg_stack.empty()) E_Exit("ACPI AML writer EndPkg with empty stack");

	pkg_t &ent = pkg_stack.top();

	const unsigned long len = (unsigned long)(w - ent.pkg_len);
	const unsigned int lflen = (unsigned int)(ent.pkg_data - ent.pkg_len);
	PkgLength(len,ent.pkg_len,lflen);
	if (ent.pkg_len != ent.pkg_data) E_Exit("ACPI AML writer length update exceeds pkglength field");
	pkg_stack.pop();
	return *this;
}

ACPIAMLWriter &ACPIAMLWriter::CountElement(void) {
	if (pkg_stack.empty()) E_Exit("ACPI AML writer counting elements not supported unless within package");
	pkg_stack.top().element_count++;
	return *this;
}

ACPIAMLWriter::ACPIAMLWriter() {
}

ACPIAMLWriter::~ACPIAMLWriter() {
}

unsigned char* ACPIAMLWriter::writeptr(void) const {
	return w;
}

void ACPIAMLWriter::begin(unsigned char *n_w,unsigned char *n_f) {
	w = n_w;
	f = n_f;
}

void BuildACPITable(void) {
	uint32_t rsdt_reserved = 16384;
	unsigned char *w,*f;
	unsigned int i,c;

	if (ACPI_buffer == NULL || ACPI_buffer_size < 32768) return;
	w = ACPI_buffer;
	f = ACPI_buffer+ACPI_buffer_size-rsdt_reserved;

	/* RSDT starts at last 16KB of ACPI buffer because it needs to build up a list of other tables */
	unsigned char *rsdt = f;

	/* RSD PTR is written to the legacy BIOS region, on a 16-byte boundary */
	Bitu rsdptr = ROMBIOS_GetMemory(20,"ACPI BIOS Root System Description Pointer",/*paragraph align*/16);
	if (rsdptr == 0) E_Exit("ACPI BIOS RSD PTR alloc fail");
	LOG(LOG_MISC,LOG_DEBUG)("ACPI: RSD PTR at 0x%lx",(unsigned long)rsdptr);

	phys_writes(rsdptr +  0,"RSD PTR ",8); // Signature
	phys_writeb(rsdptr +  8,0); // Checksum (fill in later)
	phys_writes(rsdptr +  9,"DOSBOX",6); // OEMID
	phys_writeb(rsdptr + 15,0); // Reserved must be zero
	phys_writed(rsdptr + 16,acpiofs2phys( acpiptr2ofs( rsdt ) )); // RSDT physical address
	c=0; for (i=0;i < 20;i++) c += phys_readb(rsdptr+i);
	phys_writeb(rsdptr +  8,(0u - c)&0xFF); // Checksum

	/* RSDT */
	ACPISysDescTableWriter rsdt_tw;
	rsdt_tw.begin(rsdt,ACPI_buffer+ACPI_buffer_size).setSig("RSDT").setRev(1);
	unsigned int rsdt_tw_ofs = 36;
	// leave open for adding one DWORD per table to the end as we go... this is why RSDT is written to the END of the ACPI region.

	/* FACP, which does not have a checksum and does not follow the normal format */
	unsigned char *facs = w;
	size_t facs_size = 64;
	w += facs_size;
	{
		assert(w <= f);
		memset(facs,0,facs_size);
		memcpy(facs+0x00,"FACS",4);
		host_writed(facs+0x04,facs_size);
		host_writed(facs+0x08,0x12345678UL); // hardware signature
		host_writed(facs+0x0C,0); // firmware waking vector
		ACPI_buffer_global_lock = acpiptr2ofs(facs+0x10);
		host_writed(facs+0x10,0); // global lock
		host_writed(facs+0x14,0); // S4BIOS_REQ not supported
		LOG(LOG_MISC,LOG_DEBUG)("ACPI: FACS at 0x%lx len 0x%lx",(unsigned long)acpiofs2phys( acpiptr2ofs( facs ) ),(unsigned long)facs_size);
	}

	unsigned char *dsdt_base = w;
	{
		ACPISysDescTableWriter dsdt;
		ACPIAMLWriter aml;

		dsdt.begin(w,f).setSig("DSDT").setRev(1);
		aml.begin(dsdt.getptr()+dsdt.get_tablesize(),f);

		/* WARNING: To simplify this code, you are responsible for writing the AML in the syntax required.
		 *          See the ACPI BIOS specification for more details.
		 *
		 * For reference:
		 *
		 * Name := [LeadNameChar NameChar NameChar NameChar] |
		 *         [LeadNameChar NameChar NameChar '_'] |
		 *         [LeadNameChar NameChar '_' '_'] |
		 *         [LeadNameChar '_' '_' '_']
		 *
		 * DefName := NameOp Name DataTerm
		 *     NameOp => 0x08
		 *     Data := DataTerm [DataTerm ...]
		 *     DataTerm := DataItem | DefPackage
		 *     DataItem := DefBuffer | DefNum | DefString
		 *
		 *     How to write: ACPIAML1_NameOp(Name) followed by the necessary functions to write the buffer, string, etc. for the name. */
		aml.ScopeOp().RootCharScopeOp();/* Scope (\) */
			aml.OpRegionOp("DBG",ACPIRegionSpace::SystemIO).WordOp(ACPI_DEBUG_IO).ByteOp(0x10);
			aml.FieldOp("DBG",ACPIAMLWriter::MaxPkgSize,ACPIFieldFlag::AccessType::DwordAcc|ACPIFieldFlag::UpdateRule::WriteAsZeros);
			aml.FieldOpElement("DBGV",32);
			aml.FieldOpEnd();
		aml.ScopeOpEnd(); /* } end of Scope(\) */

		aml.ScopeOp().RootCharOp().Name("_SB");
			if (pcibus_enable) {
				aml.DeviceOp("PCI0");
					aml.NameOp("_HID").DwordOp(ISAPNP_ID('P','N','P',0x00,0x0A,0x00,0x03));
					aml.NameOp("_ADR").DwordOp(0); /* [31:16] device [15:0] function */
					aml.NameOp("_UID").DwordOp(0xD05B0C5);
				aml.NameOp("_CRS").BufferOp().rtBegin(); /* ResourceTemplate() i.e. resource list */
					aml.rtIO(
						ACPIrtIO_16BitDecode,
						0x0CF8,/*min*/
						0x0CF8,/*max*/
						0x01,/*align*/
						0x4/*number of I/O ports req*/);
					aml.rtEnd();
				aml.BufferOpEnd();
			}
			else {
				aml.DeviceOp("ISA");
					aml.NameOp("_HID").DwordOp(ISAPNP_ID('P','N','P',0x00,0x0A,0x00,0x00));
					aml.NameOp("_ADR").DwordOp(0); /* [31:16] device [15:0] function */
					aml.NameOp("_UID").DwordOp(0xD05B0C5);
				aml.DeviceOpEnd();

			}
		aml.ScopeOpEnd();

		assert(aml.writeptr() >= (dsdt.getptr()+dsdt.get_tablesize()));
		assert(aml.writeptr() <= f);
		dsdt.expandto((size_t)(aml.writeptr() - dsdt.getptr()));
		LOG(LOG_MISC,LOG_DEBUG)("ACPI: DSDT at 0x%lx len 0x%lx",(unsigned long)acpiofs2phys( acpiptr2ofs( dsdt_base ) ),(unsigned long)dsdt.get_tablesize());
		w = dsdt.finish();
	}

	{ /* Fixed ACPI Description Table (FACP) */
		ACPISysDescTableWriter facp;
		const PhysPt facp_offset = acpiofs2phys( acpiptr2ofs( w ) );

		host_writed(rsdt_tw.getptr(rsdt_tw_ofs,4),(uint32_t)facp_offset);
		rsdt_tw_ofs += 4;

		facp.begin(w,f,116).setSig("FACP").setRev(1);
		host_writed(w+36,acpiofs2phys( acpiptr2ofs( facs ) ) ); // FIRMWARE_CTRL (FACS table)
		host_writed(w+40,acpiofs2phys( acpiptr2ofs( dsdt_base ) ) ); // DSDT
		w[44] = 0; // dual PIC PC-AT type implementation
		host_writew(w+46,ACPI_IRQ); // SCI_INT
		host_writed(w+48,ACPI_SMI_CMD); // SCI_CMD (I/O port)
		w[52] = ACPI_ENABLE_CMD; // what the guest writes to SMI_CMD to disable SMI ownership from BIOS during bootup
		w[53] = ACPI_DISABLE_CMD; // what the guest writes to SMI_CMD to re-enable SMI ownership to BIOS
		// TODO: S4BIOS_REQ
		host_writed(w+56,ACPI_PM1A_EVT_BLK); // PM1a_EVT_BLK event register block
		host_writed(w+64,ACPI_PM1A_CNT_BLK); // PM1a_CNT_BLK control register block
		host_writed(w+76,ACPI_PM_TMR_BLK); // PM_TMR_BLK power management timer control register block
		w[88] = 4; // PM1_EVT_LEN
		w[89] = 2; // PM1_CNT_LEN
		w[90] = 0; // PM2_CNT_LEN
		w[91] = 4; // PM_TM_LEN
		host_writed(w+112,(1u << 0u)/*WBINVD*/);
		LOG(LOG_MISC,LOG_DEBUG)("ACPI: FACP at 0x%lx len 0x%lx",(unsigned long)facp_offset,(unsigned long)facp.get_tablesize());
		w = facp.finish();
	}

	/* Finish RSDT */
	LOG(LOG_MISC,LOG_DEBUG)("ACPI: RDST at 0x%lx len 0x%lx",(unsigned long)acpiofs2phys( acpiptr2ofs( rsdt ) ),(unsigned long)rsdt_tw.get_tablesize());
	rsdt_tw.finish();
}

#if C_LIBPNG
# include "dosbox224x93.h"
# include "dosbox224x163.h"
# include "dosbox224x186.h"
# include "dosbox224x224.h"

static const unsigned char *BIOSLOGO_PNG_PTR = NULL;
static const unsigned char *BIOSLOGO_PNG_FENCE = NULL;

static void BIOSLOGO_PNG_READ(png_structp context,png_bytep buf,size_t count) {
	(void)context;

	while (count > 0 && BIOSLOGO_PNG_PTR < BIOSLOGO_PNG_FENCE) {
		*buf++ = *BIOSLOGO_PNG_PTR++;
		count--;
	}
	while (count > 0) {
		*buf++ = 0;
		count--;
	}
}

#endif

extern unsigned int INT13Xfer;

class BIOS:public Module_base{
private:
    static Bitu cb_bios_post__func(void) {
        void TIMER_BIOS_INIT_Configure();
#if C_DEBUG
        void DEBUG_CheckCSIP();

# if C_HEAVY_DEBUG
        /* the game/app obviously crashed, which is way more important
         * to log than what we do here in the BIOS at POST */
        void DEBUG_StopLog(void);
        DEBUG_StopLog();
# endif
#endif

	INT13_ElTorito_NoEmuDriveNumber = 0;
	INT13_ElTorito_NoEmuCDROMDrive = 0;
	INT13_ElTorito_IDEInterface = -1;
	INT13Xfer = 0;

	ACPI_mem_enable(false);
	ACPI_REGION_SIZE = 0;
	ACPI_BASE = 0;
	ACPI_enabled = false;
	ACPI_version = 0;
	ACPI_free();
	ACPI_SCI_EN = false;
	ACPI_BM_RLD = false;
	ACPI_PM1_Status = 0;
	ACPI_PM1_Enable = 0;

	E280_table_entries = 0;

	{/*Conventional memory*/
		BIOS_E280_entry &ent = E280_table[E280_table_entries++];
		ent.base = 0x00000000;
		ent.length = 0x9F000; /* 640KB minus the EBDA */
		ent.type = 1;/*Normal RAM*/
	}
	{/*Conventional adapter ROM/RAM/BIOS*/
		BIOS_E280_entry &ent = E280_table[E280_table_entries++];
		ent.base = 0x000C0000;
		ent.length = 0x40000;
		ent.type = 2;/*Reserved*/
	}
	if (MEM_TotalPages() > 0x100) { /* more than 1MB of RAM */
		BIOS_E280_entry &ent = E280_table[E280_table_entries++];
		ent.base = 0x00100000;
		ent.length = (MEM_TotalPages()-0x100u)*4096u;
		ent.type = 1;/*Normal RAM*/
	}
	if (MEM_TotalPagesAt4GB() > 0) { /* anything above 4GB? */
		BIOS_E280_entry &ent = E280_table[E280_table_entries++];
		ent.base = uint64_t(0x100000000ull);
		ent.length = uint64_t(MEM_TotalPagesAt4GB())*uint64_t(4096ul);
		ent.type = 1;/*Normal RAM*/
	}

        /* If we're here because of a JMP to F000:FFF0 from a DOS program, then
         * an actual reset is needed to prevent reentrancy problems with the DOS
         * kernel shell. The WINNT.EXE install program for Windows NT/2000/XP
         * likes to restart the program by JMPing to F000:FFF0 */
        if (!dos_kernel_disabled && first_shell != NULL) {
		LOG(LOG_MISC,LOG_DEBUG)("BIOS POST: JMP to F000:FFF0 detected, initiating proper reset");
		throw int(9);
	}

	{
		Section_prop * section=static_cast<Section_prop *>(control->GetSection("dosbox"));
		int val = section->Get_int("reboot delay");

		if (val < 0)
			val = IS_PC98_ARCH ? 1000 : 500;

		reset_post_delay = (unsigned int)val;

		/* Read the ACPI setting and decide on a ACPI region to use */
		{
			std::string s = section->Get_string("acpi");

			if (IS_PC98_ARCH) {
				/* do not enable ACPI, PC-98 does not have it */
			}
			else if (MEM_get_address_bits() < 32) {
				/* I doubt any 486DX systems with less than 32 address bits has ACPI */
			}
			else if (CPU_ArchitectureType < CPU_ARCHTYPE_386) {
				/* Your 286 does not have ACPI and it never will.
				 * Your 386 as well, but the 386 is 32-bit and the user might change it to 486 or higher later though, so we'll allow that */
			}
			else if (s == "1.0") {
				ACPI_version = 0x100;
				ACPI_REGION_SIZE = (256u << 10u); // 256KB
			}
			else if (s == "1.0b") {
				ACPI_version = 0x10B;
				ACPI_REGION_SIZE = (256u << 10u); // 256KB
			}
		}

		/* TODO: Read from dosbox.conf */
		if (ACPI_version != 0) {
			ACPI_IRQ = 9;
			ACPI_IO_BASE = 0x820;
			ACPI_SMI_CMD = 0x828;
		}
	}

        if (bios_post_counter != 0 && reset_post_delay != 0) {
            /* reboot delay, in case the guest OS/application had something to day before hitting the "reset" signal */
            uint32_t lasttick=GetTicks();
            while ((GetTicks()-lasttick) < reset_post_delay) {
                void CALLBACK_IdleNoInts(void);
                CALLBACK_IdleNoInts();
            }
        }

        if (bios_post_counter != 0) {
            /* turn off the PC speaker if the guest left it on at reset */
            if (IS_PC98_ARCH) {
                IO_Write(0x37,0x07);
            }
            else {
                IO_Write(0x61,IO_Read(0x61) & (~3u));
            }
        }

        bios_post_counter++;

        if (bios_first_init) {
            /* clear the first 1KB-32KB */
            for (uint16_t i=0x400;i<0x8000;i++) real_writeb(0x0,i,0);
        }

        if (IS_PC98_ARCH) {
            for (unsigned int i=0;i < callback_count;i++) callback[i].Uninstall();

            /* clear out 0x50 segment (TODO: 0x40 too?) */
            for (unsigned int i=0;i < 0x100;i++) phys_writeb(0x500+i,0);

            BIOS_ZeroExtendedSize(false);

            if (call_pc98_default_stop == 0)
                call_pc98_default_stop = CALLBACK_Allocate();

            CALLBACK_Setup(call_pc98_default_stop,&pc98_default_stop_handler,CB_IRET,"INT 6h invalid opcode or STOP interrupt");

            unsigned char memsize_real_code = 0;
            Bitu mempages = MEM_TotalPages(); /* in 4KB pages */

            /* NTS: Fill in the 3-bit code in FLAGS1 that represents
             *      how much lower conventional memory is in the system.
             *
             *      Note that MEM.EXE requires this value, or else it
             *      will complain about broken UMB linkage and fail
             *      to show anything else. */
            /* TODO: In the event we eventually support "high resolution mode"
             *       we can indicate 768KB here, code == 5, meaning that
             *       the RAM extends up to 0xBFFFF instead of 0x9FFFF */
            if (mempages >= (640UL/4UL))        /* 640KB */
                memsize_real_code = 4;
            else if (mempages >= (512UL/4UL))   /* 512KB */
                memsize_real_code = 3;
            else if (mempages >= (384UL/4UL))   /* 384KB */
                memsize_real_code = 2;
            else if (mempages >= (256UL/4UL))   /* 256KB */
                memsize_real_code = 1;
            else                                /* 128KB */
                memsize_real_code = 0;

            void pc98_msw3_set_ramsize(const unsigned char b);
            pc98_msw3_set_ramsize(memsize_real_code);

            /* CRT status */
            /* bit[7:6] = 00=conventional compatible  01=extended attr JEH  10=extended attr EGH
             * bit[5:5] = Single event timer in use flag    1=busy  0=not used
             * bit[4:4] = ?
             * bit[3:3] = raster scan    1=non-interlaced     0=interlaced
             * bit[2:2] = Content ruled line color  1=I/O set value    0=attributes of VRAM
             * bit[1:1] = ?
             * bit[0:0] = 480-line mode    1=640x480     0=640x400 or 640x200 */
            mem_writeb(0x459,0x08/*non-interlaced*/);

            /* Time stamper */
            /* bit[7:7] = 1=Port 5Fh exists  0=No such port    Write to port 0x5F to wait 0.6us
             * bit[6:6] = ?
             * bit[5:5] = "Power" ?
             * bit[4:4] = 1=PCMCIA BIOS running 0=not running
             * bit[3:3] = ?
             * bit[2:2] = 1=Time stamper (I/O ports 0x5C and 0x5E) available
             * bit[1:1] = 1=Card I/O slot function 0=No card slot function
             * bit[0:0] = 1=386SL(98)  0=Other */
            mem_writeb(0x45B,(pc98_timestamp5c?0x4:0x0)|0x80/*port 5Fh*/);

            /* CPU/Display */
            /* bit[7:7] = 486SX equivalent (?)                                                                      1=yes
             * bit[6:6] = PC-9821 Extended Graph Architecture supported (FIXME: Is this the same as having EGC?)    1=yes
             * bit[5:5] = LCD display is color                                                                      1=yes 0=no
             * bit[4:4] = ?
             * bit[3:3] = ROM drive allow writing
             * bit[2:2] = 98 NOTE PC-9801N-08 expansion I/O box connected
             * bit[1:1] = 98 NOTE prohibit transition to power saving mode
             * bit[0:0] = 98 NOTE coprocessor function available */
            mem_writeb(0x45C,(enable_pc98_egc ? 0x40/*Extended Graphics*/ : 0x00));

            /* CPU type in bits [1:0] */
            if (CPU_ArchitectureType >= CPU_ARCHTYPE_286) {
                    mem_writeb(0x480,CPU_ArchitectureType >= CPU_ARCHTYPE_386 ? 3 : 1);
            }


            /* Keyboard type */
            /* bit[7:7] = ?
             * bit[6:6] = keyboard type bit 1
             * bit[5:5] = EMS page frame at B0000h 1=present 0=none
             * bit[4:4] = EMS page frame at B0000h 1=page frame 0=G-VRAM
             * bit[3:3] = keyboard type bit 0
             * bit[2:2] = High resolution memory window available
             * bit[1:1] = ?
             * bit[0:0] = ?
             *
             * keyboard bits[1:0] from bit 6 as bit 1 and bit 3 as bit 0 combined:
             * 11 = new keyboard (NUM key, DIP switch 2-7 OFF)
             * 10 = new keyboard (without NUM key)
             * 01 = new keyboard (NUM key, DIP switch 2-7 ON)
             * 00 = old keyboard
             *
             * The old keyboard is documented not to support software control of CAPS and KANA states */
            /* TODO: Make this a dosbox-x.conf option. Default is new keyboard without NUM key because that is
             *       what keyboard emulation currently acts like anyway. */
            mem_writeb(0x481,0x40/*bit 6=1 bit 3=0 new keyboard without NUM key*/);

            /* BIOS flags */
            /* bit[7:7] = Startup            1=hot start    0=cold start
             * bit[6:6] = BASIC type         ??
             * bit[5:5] = Keyboard beep      1=don't beep   0=beep          ... when buffer full
             * bit[4:4] = Expansion conv RAM 1=present      0=absent
             * bit[3:3] = ??
             * bit[2:2] = ??
             * bit[1:1] = HD mode            1=1MB mode     0=640KB mode    ... of the floppy drive
             * bit[0:0] = Model              1=other        0=PC-9801 original */
            /* NTS: MS-DOS 5.0 appears to reduce it's BIOS calls and render the whole
             *      console as green IF bit 0 is clear.
             *
             *      If bit 0 is set, INT 1Ah will be hooked by MS-DOS and, for some odd reason,
             *      MS-DOS's hook proc will call to our INT 1Ah + 0x19 bytes. */
            mem_writeb(0x500,0x01 | 0x02/*high density drive*/);

            /* BIOS flags */
            /* timer setup will set/clear bit 7 */
            /* bit[7:7] = system clock freq  1=8MHz         0=5/10Mhz
             *          = timer clock freq   1=1.9968MHz    0=2.4576MHz
             * bit[6:6] = CPU                1=V30          0=Intel (8086 through Pentium)
             * bit[5:5] = Model info         1=Other model  0=PC-9801 Muji, PC-98XA
             * bit[4:4] = Model info         ...
             * bit[3:3] = Model info         1=High res     0=normal
             * bit[2:0] = Realmode memsize
             *                             000=128KB      001=256KB
             *                             010=384KB      011=512KB
             *                             100=640KB      101=768KB
             *
             * Ref: http://hackipedia.org/browse/Computer/Platform/PC,%20NEC%20PC-98/Collections/Undocumented%209801,%209821%20Volume%202%20(webtech.co.jp)/memsys.txt */
            /* NTS: High resolution means 640x400, not the 1120x750 mode known as super high resolution mode.
             *      DOSBox-X does not yet emulate super high resolution nor does it emulate the 15khz 200-line "standard" mode.
             *      ref: https://github.com/joncampbell123/dosbox-x/issues/906#issuecomment-434513930
             *      ref: https://jisho.org/search?utf8=%E2%9C%93&keyword=%E8%B6%85 */
            mem_writeb(0x501,0x20 | memsize_real_code);

            /* keyboard buffer */
            mem_writew(0x524/*tail*/,0x502);
            mem_writew(0x526/*tail*/,0x502);

            /* number of scanlines per text row - 1 */
            mem_writeb(0x53B,0x0F); // CRT_RASTER, 640x400 24.83KHz-hsync 56.42Hz-vsync

            /* Text screen status.
             * Note that most of the bits are used verbatim in INT 18h AH=0Ah/AH=0Bh */
            /* bit[7:7] = High resolution display                   1=yes           0=no (standard)         NOT super high res
             * bit[6:6] = vsync                                     1=VSYNC wait    0=end of vsync handling
             * bit[5:5] = unused
             * bit[4:4] = Number of lines                           1=30 lines      0=20/25 lines
             * bit[3:3] = K-CG access mode                          1=dot access    0=code access
             * bit[2:2] = Attribute mode (how to handle bit 4)      1=Simp. graphic 0=Vertical line
             * bit[1:1] = Number of columns                         1=40 cols       0=80 cols
             * bit[0:0] = Number of lines                           1=20/30 lines   0=25 lines */
            mem_writeb(0x53C,(true/*TODO*/ ? 0x80/*high res*/ : 0x00/*standard*/));

            /* BIOS raster location */
            mem_writew(0x54A,0x1900);

            /* BIOS flags */
            /* bit[7:7] = Graphics display state                    1=Visible       0=Blanked (hidden)
             * bit[6:6] = CRT type                                  1=high res      0=standard              NOT super high res
             * bit[5:5] = Horizontal sync rate                      1=31.47KHz      0=24.83KHz
             * bit[4:4] = CRT line mode                             1=480-line      0=400-line
             * bit[3:3] = Number of user-defined characters         1=188+          0=63
             * bit[2:2] = Extended graphics RAM (for 16-color)      1=present       0=absent
             * bit[1:1] = Graphics Charger is present               1=present       0=absent
             * bit[0:0] = DIP switch 1-8 at startup                 1=ON            0=OFF (support GLIO 16-colors) */
            mem_writeb(0x54C,(true/*TODO*/ ? 0x40/*high res*/ : 0x00/*standard*/) | (enable_pc98_grcg ? 0x02 : 0x00) | (enable_pc98_16color ? 0x05 : 0x00) | (pc98_31khz_mode ? 0x20/*31khz*/ : 0x00/*24khz*/) | (enable_pc98_188usermod ? 0x08 : 0x00)); // PRXCRT, 16-color G-VRAM, GRCG

            /* BIOS flags */
            /* bit[7:7] = PC-9821 graphics mode (INT 18h AH=4Dh CH=01h/00h)                 1=extend 0=normal
             * bit[6:6] = Enhanced Graphics Charger (EGC) is present
             * bit[5:5] = GDC at 5.0MHz at boot up (copy of DIP switch 2-8 at startup)      1=yes 0=no
             * bit[4:4] = Always "flickerless" drawing mode
             * bit[3:3] = Drawing mode with flicker
             * bit[2:2] = GDC clock                                                         1=5MHz 0=2.5MHz
             * bit[1:0] = Drawing mode of the GDC
             *              00 = REPLACE
             *              01 = COMPLEMENT
             *              10 = CLEAR
             *              11 = SET */
            mem_writeb(0x54D,
                    (enable_pc98_egc ? 0x40 : 0x00) |
                    (gdc_5mhz_mode ? 0x20 : 0x00) |
                    (gdc_5mhz_mode ? 0x04 : 0x00)); // EGC

            /* BIOS flags */
            /* bit[7:7] = INT 18h AH=30h/31h support enabled
             * bit[6:3] = 0 (unused)
             * bit[2:2] = Enhanced Graphics Mode (EGC) supported
             * bit[1:0] = Graphic resolution
             *             00 = 640x200 upper half  (2/8/16-color mode)
             *             01 = 640x200 lower half  (2/8/16-color mode)
             *             10 = 640x400             (2/8/16/256-color mode)
             *             11 = 640x480             256-color mode */
            mem_writeb(0x597,(enable_pc98_egc ? 0x04 : 0x00)/*EGC*/ |
                             (enable_pc98_egc ? 0x80 : 0x00)/*supports INT 18h AH=30h and AH=31h*/ |
                             2/*640x400*/);
            /* TODO: I would like to eventually add a dosbox-x.conf option that controls whether INT 18h AH=30h and 31h
             *       are enabled, so that retro-development can test code to see how it acts on a newer PC-9821
             *       that supports it vs an older PC-9821 that doesn't.
             *
             *       If the user doesn't set the option, then it is "auto" and determined by machine= PC-98 model and
             *       by another option in dosbox-x.conf that determines whether 31khz support is enabled.
             *
             *       NOTED: Neko Project II determines INT 18h AH=30h availability by whether or not it was compiled
             *              with 31khz hsync support (SUPPORT_CRT31KHZ) */

            /* Set up the translation table pointer, which is relative to segment 0xFD80 */
            mem_writew(0x522,(unsigned int)(Real2Phys(BIOS_PC98_KEYBOARD_TRANSLATION_LOCATION) - 0xFD800));
            mem_writew(0x5C6,(unsigned int)(Real2Phys(BIOS_PC98_KEYBOARD_TRANSLATION_LOCATION) - 0xFD800));
            mem_writew(0x5C8,0xFD80);
        }

        if (bios_user_reset_vector_blob != 0 && !bios_user_reset_vector_blob_run) {
            LOG_MSG("BIOS POST: Running user reset vector blob at 0x%lx",(unsigned long)bios_user_reset_vector_blob);
            bios_user_reset_vector_blob_run = true;

            assert((bios_user_reset_vector_blob&0xF) == 0); /* must be page aligned */

            SegSet16(cs,bios_user_reset_vector_blob>>4);
            reg_eip = 0;

#if C_DEBUG
            /* help the debugger reflect the new instruction pointer */
            DEBUG_CheckCSIP();
#endif

            return CBRET_NONE;
        }

        if (cpu.pmode) E_Exit("BIOS error: POST function called while in protected/vm86 mode");

        CPU_CLI();

        /* we need A20 enabled for BIOS boot-up */
        void A20Gate_OverrideOn(Section *sec);
        void MEM_A20_Enable(bool enabled);
        A20Gate_OverrideOn(NULL);
        MEM_A20_Enable(true);

        BIOS_OnResetComplete(NULL);

        adapter_scan_start = 0xC0000;
        bios_has_exec_vga_bios = false;
        LOG(LOG_MISC,LOG_DEBUG)("BIOS: executing POST routine");

	if (ACPI_REGION_SIZE != 0 && !IS_PC98_ARCH) {
		// place it just below the mirror of the BIOS at FFFF0000
		ACPI_BASE = 0xFFFF0000 - ACPI_REGION_SIZE;

		LOG(LOG_MISC,LOG_DEBUG)("ACPI: Setting up version %u.%02x at 0x%lx-0x%lx",
			ACPI_version>>8,ACPI_version&0xFF,
			(unsigned long)ACPI_BASE,(unsigned long)(ACPI_BASE+ACPI_REGION_SIZE-1lu));

		ACPI_init();
		ACPI_enabled = true;
		ACPI_mem_enable(true);
		memset(ACPI_buffer,0,ACPI_buffer_size);
	}

        // TODO: Anything we can test in the CPU here?

        // initialize registers
        SegSet16(ds,0x0000);
        SegSet16(es,0x0000);
        SegSet16(fs,0x0000);
        SegSet16(gs,0x0000);
        SegSet16(ss,0x0000);

        {
            Bitu sz = MEM_ConventionalPages();

            /* The standard BIOS is said to put its stack (at least at OS boot time) 512 bytes past the end of the boot sector
             * meaning that the boot sector loads to 0000:7C00 and the stack is set grow downward from 0000:8000 */

            if (sz > 8) sz = 8; /* 4KB * 8 = 32KB = 0x8000 */
            sz *= 4096;
            reg_esp = sz - 4;
            reg_ebp = 0;
            LOG(LOG_MISC,LOG_DEBUG)("BIOS: POST stack set to 0000:%04x",reg_esp);
        }

        if (dosbox_int_iocallout != IO_Callout_t_none) {
            IO_FreeCallout(dosbox_int_iocallout);
            dosbox_int_iocallout = IO_Callout_t_none;
        }

        if (isapnp_biosstruct_base != 0) {
            ROMBIOS_FreeMemory(isapnp_biosstruct_base);
            isapnp_biosstruct_base = 0;
        }

        if (acpi_iocallout != IO_Callout_t_none) {
            IO_FreeCallout(acpi_iocallout);
            acpi_iocallout = IO_Callout_t_none;
        }

        if (BOCHS_PORT_E9) {
            delete BOCHS_PORT_E9;
            BOCHS_PORT_E9=NULL;
        }
        if (ISAPNP_PNP_ADDRESS_PORT) {
            delete ISAPNP_PNP_ADDRESS_PORT;
            ISAPNP_PNP_ADDRESS_PORT=NULL;
        }
        if (ISAPNP_PNP_DATA_PORT) {
            delete ISAPNP_PNP_DATA_PORT;
            ISAPNP_PNP_DATA_PORT=NULL;
        }
        if (ISAPNP_PNP_READ_PORT) {
            delete ISAPNP_PNP_READ_PORT;
            ISAPNP_PNP_READ_PORT=NULL;
        }

        if (bochs_port_e9) {
            if (BOCHS_PORT_E9 == NULL) {
                BOCHS_PORT_E9 = new IO_WriteHandleObject;
                BOCHS_PORT_E9->Install(0xE9,bochs_port_e9_write,IO_MB);
            }
            LOG(LOG_MISC,LOG_DEBUG)("Bochs port E9h emulation is active");
        }
        else {
            if (BOCHS_PORT_E9 != NULL) {
                delete BOCHS_PORT_E9;
                BOCHS_PORT_E9 = NULL;
            }
        }

        extern Bitu call_default;

        if (IS_PC98_ARCH) {
            /* INT 00h-FFh generic stub routine */
            /* NTS: MS-DOS on PC-98 will fill all yet-unused interrupt vectors with a stub.
             *      No vector is left at 0000:0000. On a related note, PC-98 games apparently
             *      like to call INT 33h (mouse functions) without first checking that the
             *      vector is non-null. */
            callback[18].Uninstall();
            callback[18].Install(&INTGEN_PC98_Handler,CB_IRET,"Int stub ???");
            for (unsigned int i=0x00;i < 0x100;i++) RealSetVec(i,callback[18].Get_RealPointer());

            for (unsigned int i=0x00;i < 0x08;i++)
                real_writed(0,i*4,CALLBACK_RealPointer(call_default));

            // STOP interrupt or invalid opcode
            real_writed(0,0x06*4,CALLBACK_RealPointer(call_pc98_default_stop));

            // Magical Girl Pretty Sammy installer
            // Installer enters an infinite loop if lower 8 bits of the segment portion of int 7 are 0
            real_writew(0, 7*4, real_readw(0, 7*4) - 0x10);
            real_writew(0, 7*4+2, real_readw(0, 7*4+2) + 1);
        }
        else {
            /* Clear the vector table */
            for (uint16_t i=0x70*4;i<0x400;i++) real_writeb(0x00,i,0);

            /* Only setup default handler for first part of interrupt table */
            for (uint16_t ct=0;ct<0x60;ct++) {
                real_writed(0,ct*4,CALLBACK_RealPointer(call_default));
            }
            for (uint16_t ct=0x68;ct<0x70;ct++) {
                if(!IS_J3100 || ct != 0x6f)
                    real_writed(0,ct*4,CALLBACK_RealPointer(call_default));
            }

            // default handler for IRQ 2-7
            for (uint16_t ct=0x0A;ct <= 0x0F;ct++)
                RealSetVec(ct,BIOS_DEFAULT_IRQ07_DEF_LOCATION);
        }

        if (unhandled_irq_method == UNHANDLED_IRQ_COOPERATIVE_2ND) {
            // PC-98 style: Master PIC ack with 0x20 for IRQ 0-7.
            //              For the slave PIC, ack with 0x20 on the slave, then only ack the master (cascade interrupt)
            //              if the ISR register on the slave indicates none are in service.
            CALLBACK_Setup(call_irq07default,NULL,CB_IRET_EOI_PIC1,Real2Phys(BIOS_DEFAULT_IRQ07_DEF_LOCATION),"bios irq 0-7 default handler");
            CALLBACK_Setup(call_irq815default,Default_IRQ_Handler_Cooperative_Slave_Pic,CB_IRET,Real2Phys(BIOS_DEFAULT_IRQ815_DEF_LOCATION),"bios irq 8-15 default handler");
        }
        else {
            // IBM PC style: Master PIC ack with 0x20, slave PIC ack with 0x20, no checking
            CALLBACK_Setup(call_irq07default,NULL,CB_IRET_EOI_PIC1,Real2Phys(BIOS_DEFAULT_IRQ07_DEF_LOCATION),"bios irq 0-7 default handler");
            CALLBACK_Setup(call_irq815default,NULL,CB_IRET_EOI_PIC2,Real2Phys(BIOS_DEFAULT_IRQ815_DEF_LOCATION),"bios irq 8-15 default handler");
        }

        if (IS_PC98_ARCH) {
            BIOS_UnsetupKeyboard();
            BIOS_UnsetupDisks();

            /* no such INT 4Bh */
            int4b_callback.Uninstall();

            /* remove some IBM-style BIOS interrupts that don't exist on PC-98 */
            /* IRQ to INT arrangement
             *
             * IBM          PC-98           IRQ
             * --------------------------------
             * 0x08         0x08            0
             * 0x09         0x09            1
             * 0x0A CASCADE 0x0A            2
             * 0x0B         0x0B            3
             * 0x0C         0x0C            4
             * 0x0D         0x0D            5
             * 0x0E         0x0E            6
             * 0x0F         0x0F CASCADE    7
             * 0x70         0x10            8
             * 0x71         0x11            9
             * 0x72         0x12            10
             * 0x73         0x13            11
             * 0x74         0x14            12
             * 0x75         0x15            13
             * 0x76         0x16            14
             * 0x77         0x17            15
             *
             * As part of the change the IRQ cascade emulation needs to change for PC-98 as well.
             * IBM uses IRQ 2 for cascade.
             * PC-98 uses IRQ 7 for cascade. */

            void INT10_EnterPC98(Section *sec);
            INT10_EnterPC98(NULL); /* INT 10h */

            callback_pc98_lio.Uninstall();

            callback_pc98_avspcm.Uninstall();

            callback[1].Uninstall(); /* INT 11h */
            callback[2].Uninstall(); /* INT 12h */
            callback[3].Uninstall(); /* INT 14h */
            callback[4].Uninstall(); /* INT 15h */
            callback[5].Uninstall(); /* INT 17h */
            callback[6].Uninstall(); /* INT 1Ah */
            callback[7].Uninstall(); /* INT 1Ch */
            callback[10].Uninstall(); /* INT 19h */
            callback[11].Uninstall(); /* INT 76h: IDE IRQ 14 */
            callback[12].Uninstall(); /* INT 77h: IDE IRQ 15 */
            callback[15].Uninstall(); /* INT 18h: Enter BASIC */
            callback[19].Uninstall(); /* INT 1Bh */

            /* IRQ 6 is nothing special */
            callback[13].Uninstall(); /* INT 0Eh: IDE IRQ 6 */
            callback[13].Install(NULL,CB_IRET_EOI_PIC1,"irq 6");

            /* IRQ 8 is nothing special */
            callback[8].Uninstall();
            callback[8].Install(NULL,CB_IRET_EOI_PIC2,"irq 8");

            /* IRQ 9 is nothing special */
            callback[9].Uninstall();
            callback[9].Install(NULL,CB_IRET_EOI_PIC2,"irq 9");

            /* INT 18h keyboard and video display functions */
            callback[1].Install(&INT18_PC98_Handler,CB_INT16,"Int 18 keyboard and display");
            callback[1].Set_RealVec(0x18,/*reinstall*/true);

            /* INT 19h *STUB* */
            callback[2].Install(&INT19_PC98_Handler,CB_IRET,"Int 19 ???");
            callback[2].Set_RealVec(0x19,/*reinstall*/true);

            /* INT 1Ah *STUB* */
            callback[3].Install(&INT1A_PC98_Handler,CB_IRET,"Int 1A ???");
            callback[3].Set_RealVec(0x1A,/*reinstall*/true);

            /* MS-DOS 5.0 FIXUP:
             * - For whatever reason, if we set bits in the BIOS data area that
             *   indicate we're NOT the original model of the PC-98, MS-DOS will
             *   hook our INT 1Ah and then call down to 0x19 bytes into our
             *   INT 1Ah procedure. If anyone can explain this, I'd like to hear it. --J.C.
             *
             * NTS: On real hardware, the BIOS appears to have an INT 1Ah, a bunch of NOPs,
             *      then at 0x19 bytes into the procedure, the actual handler. This is what
             *      MS-DOS is pointing at.
             *
             *      But wait, there's more.
             *
             *      MS-DOS calldown pushes DS and DX onto the stack (after the IRET frame)
             *      before JMPing into the BIOS.
             *
             *      Apparently the function at INT 1Ah + 0x19 is expected to do this:
             *
             *      <function code>
             *      POP     DX
             *      POP     DS
             *      IRET
             *
             *      I can only imaging what a headache this might have caused NEC when
             *      maintaining the platform and compatibility! */
            {
                Bitu addr = callback[3].Get_RealPointer();
                addr = ((addr >> 16) << 4) + (addr & 0xFFFF);

                /* to make this work, we need to pop the two regs, then JMP to our
                 * callback and proceed as normal. */
                phys_writeb(addr + 0x19,0x5A);       // POP DX
                phys_writeb(addr + 0x1A,0x1F);       // POP DS
                phys_writeb(addr + 0x1B,0xEB);       // jmp short ...
                phys_writeb(addr + 0x1C,0x100 - 0x1D);
            }

            /* INT 1Bh *STUB* */
            callback[4].Install(&INT1B_PC98_Handler,CB_IRET,"Int 1B ???");
            callback[4].Set_RealVec(0x1B,/*reinstall*/true);

            /* INT 1Ch *STUB* */
            callback[5].Install(&INT1C_PC98_Handler,CB_IRET,"Int 1C ???");
            callback[5].Set_RealVec(0x1C,/*reinstall*/true);

            /* INT 1Dh *STUB* */
            /* Place it in the PC-98 int vector area at FD80:0000 to satisfy some DOS games
             * that detect PC-98 from the segment value of the vector (issue #927).
             * Note that on real hardware (PC-9821) INT 1Dh appears to be a stub that IRETs immediately. */
            callback[6].Install(&INT1D_PC98_Handler,CB_IRET,"Int 1D ???");
//            callback[6].Set_RealVec(0x1D,/*reinstall*/true);
            {
                Bitu ofs = 0xFD813; /* 0xFD80:0013 try not to look like a phony address */
                unsigned int vec = 0x1D;
                uint32_t target = callback[6].Get_RealPointer();

                phys_writeb(ofs+0,0xEA);        // JMP FAR <callback>
                phys_writed(ofs+1,target);

                phys_writew((vec*4)+0,(ofs-0xFD800));
                phys_writew((vec*4)+2,0xFD80);
            }

            /* INT 1Eh *STUB* */
            callback[7].Install(&INT1E_PC98_Handler,CB_IRET,"Int 1E ???");
            callback[7].Set_RealVec(0x1E,/*reinstall*/true);

            /* INT 1Fh *STUB* */
            callback[10].Install(&INT1F_PC98_Handler,CB_IRET,"Int 1F ???");
            callback[10].Set_RealVec(0x1F,/*reinstall*/true);

            /* INT DCh *STUB* */
            callback[16].Install(&INTDC_PC98_Handler,CB_IRET,"Int DC ???");
            callback[16].Set_RealVec(0xDC,/*reinstall*/true);

            /* INT F2h *STUB* */
            callback[17].Install(&INTF2_PC98_Handler,CB_IRET,"Int F2 ???");
            callback[17].Set_RealVec(0xF2,/*reinstall*/true);

            // default handler for IRQ 2-7
            for (uint16_t ct=0x0A;ct <= 0x0F;ct++)
                RealSetVec(ct,BIOS_DEFAULT_IRQ07_DEF_LOCATION);

            // default handler for IRQ 8-15
            for (uint16_t ct=0;ct < 8;ct++)
                RealSetVec(ct+(IS_PC98_ARCH ? 0x10 : 0x70),BIOS_DEFAULT_IRQ815_DEF_LOCATION);

            // LIO graphics interface (number of entry points, unknown WORD value and offset into the segment).
            // For more information see Chapter 6 of this PDF [https://ia801305.us.archive.org/8/items/PC9800TechnicalDataBookBIOS1992/PC-9800TechnicalDataBook_BIOS_1992_text.pdf]
            {
                callback_pc98_lio.Install(&PC98_BIOS_LIO,CB_IRET,"LIO graphics library");

                Bitu ofs = 0xF990u << 4u; // F990:0000...
                unsigned int entrypoints = 0x11;
                Bitu final_addr = callback_pc98_lio.Get_RealPointer();

                /* NTS: Based on GAME/MAZE 999 behavior, these numbers are interrupt vector numbers.
                 *      The entry point marked 0xA0 is copied by the game to interrupt vector A0 and
                 *      then called with INT A0h even though it blindly assumes the numbers are
                 *      sequential from 0xA0-0xAF. */
                unsigned char entrypoint_indexes[0x11] = {
                    0xA0,   0xA1,   0xA2,   0xA3,       // +0x00
                    0xA4,   0xA5,   0xA6,   0xA7,       // +0x04
                    0xA8,   0xA9,   0xAA,   0xAB,       // +0x08
                    0xAC,   0xAD,   0xAE,   0xAF,       // +0x0C
                    0xCE                                // +0x10
                };

                assert(((entrypoints * 4) + 4) <= 0x50);
                assert((50 + (entrypoints * 7)) <= 0x100); // a 256-byte region is set aside for this!

                phys_writed(ofs+0,entrypoints);
                for (unsigned int ent=0;ent < entrypoints;ent++) {
                    /* each entry point is "MOV AL,<entrypoint> ; JMP FAR <callback>" */
                    /* Yksoft1's patch suggests a segment offset of 0x50 which I'm OK with */
                    unsigned int ins_ofs = ofs + 0x50 + (ent * 7);

                    phys_writew(ofs+4+(ent*4)+0,entrypoint_indexes[ent]);
                    phys_writew(ofs+4+(ent*4)+2,ins_ofs - ofs);

                    phys_writeb(ins_ofs+0,0xB0);                        // MOV AL,(entrypoint index)
                    phys_writeb(ins_ofs+1,entrypoint_indexes[ent]);
                    phys_writeb(ins_ofs+2,0xEA);                        // JMP FAR <callback>
                    phys_writed(ins_ofs+3,final_addr);
                    // total:   ins_ofs+7
                }
            }

            callback_pc98_avspcm.Install(&PC98_AVSDRV_PCM_Handler,CB_IRET,"AVSDRV.SYS PCM driver");
            callback_pc98_avspcm.Set_RealVec(0xd9, true);
        }

        if (IS_PC98_ARCH) {
            real_writew(0,0x58A,0x0000U); // countdown timer value
            PIC_SetIRQMask(0,true); /* PC-98 keeps the timer off unless INT 1Ch is called to set a timer interval */
        }

        bool null_68h = false;

        {
            Section_prop * section=static_cast<Section_prop *>(control->GetSection("dos"));

            null_68h = section->Get_bool("zero unused int 68h");
        }

        /* Default IRQ handler */
        if (call_irq_default == 0)
            call_irq_default = CALLBACK_Allocate();
        CALLBACK_Setup(call_irq_default, &Default_IRQ_Handler, CB_IRET, "irq default");
        RealSetVec(0x0b, CALLBACK_RealPointer(call_irq_default)); // IRQ 3
        RealSetVec(0x0c, CALLBACK_RealPointer(call_irq_default)); // IRQ 4
        RealSetVec(0x0d, CALLBACK_RealPointer(call_irq_default)); // IRQ 5
        RealSetVec(0x0f, CALLBACK_RealPointer(call_irq_default)); // IRQ 7
        if (!IS_PC98_ARCH) {
            RealSetVec(0x72, CALLBACK_RealPointer(call_irq_default)); // IRQ 10
            RealSetVec(0x73, CALLBACK_RealPointer(call_irq_default)); // IRQ 11
        }

        // setup a few interrupt handlers that point to bios IRETs by default
        real_writed(0,0x66*4,CALLBACK_RealPointer(call_default));   //war2d
        real_writed(0,0x67*4,CALLBACK_RealPointer(call_default));
        if (machine==MCH_CGA || null_68h) real_writed(0,0x68*4,0);  //Popcorn
        real_writed(0,0x5c*4,CALLBACK_RealPointer(call_default));   //Network stuff
        //real_writed(0,0xf*4,0); some games don't like it

        bios_first_init = false;

        DispatchVMEvent(VM_EVENT_BIOS_INIT);

        TIMER_BIOS_INIT_Configure();

        void INT10_Startup(Section *sec);
        INT10_Startup(NULL);

        if (!IS_PC98_ARCH) {
            extern uint8_t BIOS_tandy_D4_flag;
            real_writeb(0x40,0xd4,BIOS_tandy_D4_flag);
        }

        /* INT 13 Bios Disk Support */
        BIOS_SetupDisks();

        /* INT 16 Keyboard handled in another file */
        BIOS_SetupKeyboard();

        if (!IS_PC98_ARCH) {
            int4b_callback.Set_RealVec(0x4B,/*reinstall*/true);
            callback[1].Set_RealVec(0x11,/*reinstall*/true);
            callback[2].Set_RealVec(0x12,/*reinstall*/true);
            callback[3].Set_RealVec(0x14,/*reinstall*/true);
            callback[4].Set_RealVec(0x15,/*reinstall*/true);
            callback[5].Set_RealVec(0x17,/*reinstall*/true);
            callback[6].Set_RealVec(0x1A,/*reinstall*/true);
            callback[7].Set_RealVec(0x1C,/*reinstall*/true);
            callback[8].Set_RealVec(0x70,/*reinstall*/true);
            callback[9].Set_RealVec(0x71,/*reinstall*/true);
            callback[10].Set_RealVec(0x19,/*reinstall*/true);
            callback[11].Set_RealVec(0x76,/*reinstall*/true);
            callback[12].Set_RealVec(0x77,/*reinstall*/true);
            callback[13].Set_RealVec(0x0E,/*reinstall*/true);
            callback[15].Set_RealVec(0x18,/*reinstall*/true);
            callback[19].Set_RealVec(0x1B,/*reinstall*/true);
        }

        // FIXME: We're using IBM PC memory size storage even in PC-98 mode.
        //        This cannot be removed, because the DOS kernel uses this variable even in PC-98 mode.
        mem_writew(BIOS_MEMORY_SIZE,t_conv);
        // According to Ripsaw, Tandy systems hold the real memory size in a normally reserved field [https://www.vogons.org/viewtopic.php?p=948898#p948898]
        // According to the PCjr hardware reference library that memory location means the same thing
        if (machine == MCH_PCJR || machine == MCH_TANDY) mem_writew(BIOS_MEMORY_SIZE+2,t_conv_real);

        RealSetVec(0x08,BIOS_DEFAULT_IRQ0_LOCATION);
        // pseudocode for CB_IRQ0:
        //  sti
        //  callback INT8_Handler
        //  push ds,ax,dx
        //  int 0x1c
        //  cli
        //  mov al, 0x20
        //  out 0x20, al
        //  pop dx,ax,ds
        //  iret

        if (!IS_PC98_ARCH) {
            mem_writed(BIOS_TIMER,0);           //Calculate the correct time

            // INT 05h: Print Screen
            // IRQ1 handler calls it when PrtSc key is pressed; does nothing unless hooked
            phys_writeb(Real2Phys(BIOS_DEFAULT_INT5_LOCATION), 0xcf);
            RealSetVec(0x05, BIOS_DEFAULT_INT5_LOCATION);

            phys_writew(Real2Phys(RealGetVec(0x12))+0x12,0x20); //Hack for Jurresic
        }

        phys_writeb(Real2Phys(BIOS_DEFAULT_HANDLER_LOCATION),0xcf); /* bios default interrupt vector location -> IRET */

        if (!IS_PC98_ARCH) {
            // tandy DAC setup
            bool use_tandyDAC=(real_readb(0x40,0xd4)==0xff);

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
                    real_writeb(0x40,0xd4,0xff);    /* tandy DAC init value */
                    real_writed(0x40,0xd6,0x00000000);
                    /* install the DAC callback handler */
                    tandy_DAC_callback[0]=new CALLBACK_HandlerObject();
                    tandy_DAC_callback[1]=new CALLBACK_HandlerObject();
                    tandy_DAC_callback[0]->Install(&IRQ_TandyDAC,CB_IRET,"Tandy DAC IRQ");
                    tandy_DAC_callback[1]->Install(NULL,CB_TDE_IRET,"Tandy DAC end transfer");
                    // pseudocode for CB_TDE_IRET:
                    //  push ax
                    //  mov ax, 0x91fb
                    //  int 15
                    //  cli
                    //  mov al, 0x20
                    //  out 0x20, al
                    //  pop ax
                    //  iret

                    uint8_t tandy_irq = 7;
                    if (tandy_dac_type==1) tandy_irq = tandy_sb.irq;
                    else if (tandy_dac_type==2) tandy_irq = tandy_dac.irq;
                    uint8_t tandy_irq_vector = tandy_irq;
                    if (tandy_irq_vector<8) tandy_irq_vector += 8;
                    else tandy_irq_vector += (0x70-8);

                    RealPt current_irq=RealGetVec(tandy_irq_vector);
                    real_writed(0x40,0xd6,current_irq);
                    for (uint16_t i=0; i<0x10; i++) phys_writeb(PhysMake(0xf000,0xa084+i),0x80);
                } else real_writeb(0x40,0xd4,0x00);
            }
        }

        if (!IS_PC98_ARCH) {
            /* Setup some stuff in 0x40 bios segment */

            // Disney workaround
            //      uint16_t disney_port = mem_readw(BIOS_ADDRESS_LPT1);
            // port timeouts
            // always 1 second even if the port does not exist
            //      BIOS_SetLPTPort(0, disney_port);
            for(Bitu i = 1; i < 3; i++) BIOS_SetLPTPort(i, 0);
            mem_writeb(BIOS_COM1_TIMEOUT,1);
            mem_writeb(BIOS_COM2_TIMEOUT,1);
            mem_writeb(BIOS_COM3_TIMEOUT,1);
            mem_writeb(BIOS_COM4_TIMEOUT,1);

            void BIOS_Post_register_parports();
            BIOS_Post_register_parports();

            void BIOS_Post_register_comports();
            BIOS_Post_register_comports();
        }

        if (!IS_PC98_ARCH) {
            /* Setup equipment list */
            // look http://www.bioscentral.com/misc/bda.htm

            //uint16_t config=0x4400; //1 Floppy, 2 serial and 1 parallel 
            uint16_t config = 0x0;

            config |= bios_post_parport_count() << 14;
            config |= bios_post_comport_count() << 9;

#if (C_FPU)
            //FPU
            if (enable_fpu)
                config|=0x2;
#endif
            switch (machine) {
                case MCH_MDA:
                case MCH_HERC:
                    //Startup monochrome
                    config|=0x30;
                    break;
                case EGAVGA_ARCH_CASE:
                case MCH_CGA:
                case MCH_MCGA:
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

            // PS2 mouse
            if (KEYBOARD_Report_BIOS_PS2Mouse())
                config |= 0x04;

            // DMA *not* supported - Ancient Art of War CGA uses this to identify PCjr
            if (machine==MCH_PCJR) config |= 0x100;

            // Several online sources say bit 0 indicates a floppy drive is installed.
            // Testing of a couple BIOSes from 1992 and 1993 showed bit 0 to always be 1,
            // even with no floppy drives installed or configured in the BIOS.
            // Maybe 0 is possible in older BIOSes.
            config |= 0x01;

            // Bit 6 is 1 if there are 2 floppies connected and configured in the BIOS.
            // Setting to 1 since DOSBox-X can mount floppy images in both drives A and B.
            config |= 0x40;

            // Bit 12 is "game I/O attached" for PCJr, Tandy and PC/XT, and 0 (not used) for PC/AT
            if ((CPU_ArchitectureType == CPU_ARCHTYPE_8086) && (joytype != JOY_NONE))
                config |= 0x1000;

            mem_writew(BIOS_CONFIGURATION,config);
            if (IS_EGAVGA_ARCH) config &= ~0x30; //EGA/VGA startup display mode differs in CMOS
            CMOS_SetRegister(0x14,(uint8_t)(config&0xff)); //Should be updated on changes
        }

        if (!IS_PC98_ARCH) {
            /* Setup extended memory size */
            IO_Write(0x70,0x30);
            size_extended=IO_Read(0x71);
            IO_Write(0x70,0x31);
            size_extended|=(IO_Read(0x71) << 8);

            uint32_t value = 0;

            RtcUpdateDone();
            IO_Write(0x70,0xB);
            IO_Write(0x71,0x02); // BCD

            /* set BIOS_TIMER according to time/date of RTC */
            IO_Write(0x70,0);
            const unsigned char sec = BCD2BIN(IO_Read(0x71));
            IO_Write(0x70,2);
            const unsigned char min = BCD2BIN(IO_Read(0x71));
            IO_Write(0x70,4);
            const unsigned char hour = BCD2BIN(IO_Read(0x71));

            value = (uint32_t)(((hour * 3600.00) + (min * 60.00) + sec) * ((double)PIT_TICK_RATE/65536.0));

            mem_writed(BIOS_TIMER,value);
        }
        else {
            /* Provide a valid memory size anyway */
            size_extended=MEM_TotalPages()*4;
            if (size_extended >= 1024) size_extended -= 1024;
            else size_extended = 0;
        }

        /* PS/2 mouse */
        void BIOS_PS2Mouse_Startup(Section *sec);
        BIOS_PS2Mouse_Startup(NULL);

        if (!IS_PC98_ARCH) {
            /* this belongs HERE not on-demand from INT 15h! */
            biosConfigSeg = ROMBIOS_GetMemory(16/*one paragraph*/,"BIOS configuration (INT 15h AH=0xC0)",/*paragraph align*/16)>>4;
            if (biosConfigSeg != 0) {
                PhysPt data = PhysMake(biosConfigSeg,0);
                phys_writew(data,8);                        // 8 Bytes following
                if (IS_TANDY_ARCH) {
                    if (machine==MCH_TANDY) {
                        // Model ID (Tandy)
                        phys_writeb(data+2,0xFF);
                    } else {
                        // Model ID (PCJR)
                        phys_writeb(data+2,0xFD);
                    }
                    phys_writeb(data+3,0x0A);                   // Submodel ID
                    phys_writeb(data+4,0x10);                   // Bios Revision
                    /* Tandy doesn't have a 2nd PIC, left as is for now */
                    phys_writeb(data+5,(1<<6)|(1<<5)|(1<<4));   // Feature Byte 1
                } else {
                    if (machine==MCH_MCGA) {
                        /* PC/2 model 30 model */
                        phys_writeb(data+2,0xFA);
                        phys_writeb(data+3,0x00);                   // Submodel ID (PS/2) model 30
                    } else if (PS1AudioCard) { /* FIXME: Won't work because BIOS_Init() comes before PS1SOUND_Init() */
                        phys_writeb(data+2,0xFC);                   // Model ID (PC)
                        phys_writeb(data+3,0x0B);                   // Submodel ID (PS/1).
                    } else {
                        phys_writeb(data+2,0xFC);                   // Model ID (PC)
                        phys_writeb(data+3,0x00);                   // Submodel ID
                    }
                    phys_writeb(data+4,0x01);                   // Bios Revision
                    phys_writeb(data+5,(1<<6)|(1<<5)|(1<<4));   // Feature Byte 1
                }
                phys_writeb(data+6,(1<<6));             // Feature Byte 2
                phys_writeb(data+7,0);                  // Feature Byte 3
                phys_writeb(data+8,0);                  // Feature Byte 4
                phys_writeb(data+9,0);                  // Feature Byte 5
            }
        }

        // ISA Plug & Play I/O ports
        if (!IS_PC98_ARCH && ISAPNPPORT) {
            ISAPNP_PNP_ADDRESS_PORT = new IO_WriteHandleObject;
            ISAPNP_PNP_ADDRESS_PORT->Install(0x279,isapnp_write_port,IO_MB);
            ISAPNP_PNP_DATA_PORT = new IO_WriteHandleObject;
            ISAPNP_PNP_DATA_PORT->Install(0xA79,isapnp_write_port,IO_MB);
            ISAPNP_PNP_READ_PORT = new IO_ReadHandleObject;
            ISAPNP_PNP_READ_PORT->Install(ISA_PNP_WPORT,isapnp_read_port,IO_MB);
            LOG(LOG_MISC,LOG_DEBUG)("Registered ISA PnP read port at 0x%03x",ISA_PNP_WPORT);
        }

        if (enable_integration_device) {
            /* integration device callout */
            if (dosbox_int_iocallout == IO_Callout_t_none)
                dosbox_int_iocallout = IO_AllocateCallout(IO_TYPE_MB);
            if (dosbox_int_iocallout == IO_Callout_t_none)
                E_Exit("Failed to get dosbox-x integration IO callout handle");

            {
                IO_CalloutObject *obj = IO_GetCallout(dosbox_int_iocallout);
                if (obj == NULL) E_Exit("Failed to get dosbox-x integration IO callout");

                /* NTS: Ports 28h-2Bh conflict with extended DMA control registers in PC-98 mode.
                 *      TODO: Move again, if DB28h-DB2Bh are taken by something standard on PC-98. */
                obj->Install(IS_PC98_ARCH ? 0xDB28 : 0x28,
                    IOMASK_Combine(IOMASK_FULL,IOMASK_Range(4)),dosbox_integration_cb_port_r,dosbox_integration_cb_port_w);
                IO_PutCallout(obj);
            }

            /* DOSBox-X integration device */
            if (!IS_PC98_ARCH && isapnpigdevice == NULL && enable_integration_device_pnp) {
                isapnpigdevice = new ISAPnPIntegrationDevice;
                ISA_PNP_devreg(isapnpigdevice);
            }
        }

        // ISA Plug & Play BIOS entrypoint
        // NTS: Apparently, Windows 95, 98, and ME will re-enumerate and re-install PnP devices if our entry point changes its address.
        if (!IS_PC98_ARCH && ISAPNPBIOS) {
            Bitu base;
            unsigned int i;
            unsigned char c,tmp[256];

            isapnp_biosstruct_base = base = ROMBIOS_GetMemory(0x21,"ISA Plug & Play BIOS struct",/*paragraph alignment*/0x10);

            if (base == 0) E_Exit("Unable to allocate ISA PnP struct");
            LOG_MSG("ISA Plug & Play BIOS enabled");

            call_pnp_r = CALLBACK_Allocate();
            call_pnp_rp = PNPentry_real = CALLBACK_RealPointer(call_pnp_r);
            CALLBACK_Setup(call_pnp_r,ISAPNP_Handler_RM,CB_RETF,"ISA Plug & Play entry point (real)");
            //LOG_MSG("real entry pt=%08lx\n",PNPentry_real);

            call_pnp_p = CALLBACK_Allocate();
            call_pnp_pp = PNPentry_prot = CALLBACK_RealPointer(call_pnp_p);
            CALLBACK_Setup(call_pnp_p,ISAPNP_Handler_PM,CB_RETF,"ISA Plug & Play entry point (protected)");
            //LOG_MSG("prot entry pt=%08lx\n",PNPentry_prot);

            phys_writeb(base+0,'$');
            phys_writeb(base+1,'P');
            phys_writeb(base+2,'n');
            phys_writeb(base+3,'P');
            phys_writeb(base+4,0x10);       /* Version:     1.0 */
            phys_writeb(base+5,0x21);       /* Length:      0x21 bytes */
            phys_writew(base+6,0x0000);     /* Control field:   Event notification not supported */
            /* skip checksum atm */
            phys_writed(base+9,0);          /* Event notify flag addr: (none) */
            phys_writed(base+0xD,call_pnp_rp);  /* Real-mode entry point */
            phys_writew(base+0x11,call_pnp_pp&0xFFFF); /* Protected mode offset */
            phys_writed(base+0x13,(call_pnp_pp >> 12) & 0xFFFF0); /* Protected mode code segment base */
            phys_writed(base+0x17,ISAPNP_ID('D','O','S',0,8,4,0));      /* OEM device identifier (DOSBox-X 0.84.x) */
            phys_writew(base+0x1B,0xF000);      /* real-mode data segment */
            phys_writed(base+0x1D,0xF0000);     /* protected mode data segment address */
            /* run checksum */
            c=0;
            for (i=0;i < 0x21;i++) {
                if (i != 8) c += phys_readb(base+i);
            }
            phys_writeb(base+8,0x100-c);        /* checksum value: set so that summing bytes across the struct == 0 */

            /* input device (keyboard) */
            if (!ISAPNP_RegisterSysDev(ISAPNP_sysdev_Keyboard,sizeof(ISAPNP_sysdev_Keyboard),true))
                LOG_MSG("ISAPNP register failed\n");

            /* input device (mouse) */
            if (!ISAPNP_RegisterSysDev(ISAPNP_sysdev_Mouse,sizeof(ISAPNP_sysdev_Mouse),true))
                LOG_MSG("ISAPNP register failed\n");

            /* DMA controller */
            if (!ISAPNP_RegisterSysDev(ISAPNP_sysdev_DMA_Controller,sizeof(ISAPNP_sysdev_DMA_Controller),true))
                LOG_MSG("ISAPNP register failed\n");

            /* Interrupt controller */
            if (!ISAPNP_RegisterSysDev(ISAPNP_sysdev_PIC,sizeof(ISAPNP_sysdev_PIC),true))
                LOG_MSG("ISAPNP register failed\n");

            /* Timer */
            if (!ISAPNP_RegisterSysDev(ISAPNP_sysdev_Timer,sizeof(ISAPNP_sysdev_Timer),true))
                LOG_MSG("ISAPNP register failed\n");

            /* Realtime clock */
            if (!ISAPNP_RegisterSysDev(ISAPNP_sysdev_RTC,sizeof(ISAPNP_sysdev_RTC),true))
                LOG_MSG("ISAPNP register failed\n");

            /* PC speaker */
            if (!ISAPNP_RegisterSysDev(ISAPNP_sysdev_PC_Speaker,sizeof(ISAPNP_sysdev_PC_Speaker),true))
                LOG_MSG("ISAPNP register failed\n");

            /* System board */
            if (!ISAPNP_RegisterSysDev(ISAPNP_sysdev_System_Board,sizeof(ISAPNP_sysdev_System_Board),true))
                LOG_MSG("ISAPNP register failed\n");

            /* Motherboard PNP resources and general */
            if (!ISAPNP_RegisterSysDev(ISAPNP_sysdev_General_ISAPNP,sizeof(ISAPNP_sysdev_General_ISAPNP),true))
                LOG_MSG("ISAPNP register failed\n");

            /* ISA bus, meaning, a computer with ISA slots.
             * The purpose of this device is to convince Windows 95 to automatically install it's
             * "ISA Plug and Play bus" so that PnP devices are recognized automatically */
            if (!ISAPNP_RegisterSysDev(ISAPNP_sysdev_ISA_BUS,sizeof(ISAPNP_sysdev_ISA_BUS),true))
                LOG_MSG("ISAPNP register failed\n");

            if (pcibus_enable) {
                /* PCI bus, meaning, a computer with PCI slots.
                 * The purpose of this device is to tell Windows 95 that a PCI bus is present. Without
                 * this entry, PCI devices will not be recognized until you manually install the PCI driver. */
                if (!ISAPNP_RegisterSysDev(ISAPNP_sysdev_PCI_BUS,sizeof(ISAPNP_sysdev_PCI_BUS),true))
                    LOG_MSG("ISAPNP register failed\n");
            }

            /* APM BIOS device. To help Windows 95 see our APM BIOS. */
            if (APMBIOS && APMBIOS_pnp) {
                LOG_MSG("Registering APM BIOS as ISA Plug & Play BIOS device node");
                if (!ISAPNP_RegisterSysDev(ISAPNP_sysdev_APM_BIOS,sizeof(ISAPNP_sysdev_APM_BIOS),true))
                    LOG_MSG("ISAPNP register failed\n");
            }

#if (C_FPU)
            /* Numeric Coprocessor */
            if (!ISAPNP_RegisterSysDev(ISAPNP_sysdev_Numeric_Coprocessor,sizeof(ISAPNP_sysdev_Numeric_Coprocessor),true))
                LOG_MSG("ISAPNP register failed\n");
#endif

            /* RAM resources. we have to construct it */
            /* NTS: We don't do this here, but I have an old Toshiba laptop who's PnP BIOS uses
             *      this device ID to report both RAM and ROM regions. */
            {
                Bitu max = MEM_TotalPages() * 4096;
                const unsigned char h1[9] = {
                    ISAPNP_SYSDEV_HEADER(
                        ISAPNP_ID('P','N','P',0x0,0xC,0x0,0x1), /* PNP0C01 System device, motherboard resources */
                        ISAPNP_TYPE(0x05,0x00,0x00),        /* type: Memory, RAM, general */
                        0x0001 | 0x0002)
                };

                i = 0;
                memcpy(tmp+i,h1,9); i += 9;         /* can't disable, can't configure */
                /*----------allocated--------*/
                tmp[i+0] = 0x80 | 6;                /* 32-bit memory range */
                tmp[i+1] = 9;                   /* length=9 */
                tmp[i+2] = 0;
                tmp[i+3] = 0x01;                /* writeable, no cache, 8-bit, not shadowable, not ROM */
                host_writed(tmp+i+4,0x00000);           /* base */
                host_writed(tmp+i+8,max > 0xA0000 ? 0xA0000 : 0x00000); /* length */
                i += 9+3;

                if (max > 0x100000) {
                    tmp[i+0] = 0x80 | 6;                /* 32-bit memory range */
                    tmp[i+1] = 9;                   /* length=9 */
                    tmp[i+2] = 0;
                    tmp[i+3] = 0x01;
                    host_writed(tmp+i+4,0x100000);          /* base */
                    host_writed(tmp+i+8,max-0x100000);      /* length */
                    i += 9+3;
                }

                tmp[i+0] = 0x79;                /* END TAG */
                tmp[i+1] = 0x00;
                i += 2;
                /*-------------possible-----------*/
                tmp[i+0] = 0x79;                /* END TAG */
                tmp[i+1] = 0x00;
                i += 2;
                /*-------------compatible---------*/
                tmp[i+0] = 0x79;                /* END TAG */
                tmp[i+1] = 0x00;
                i += 2;

                if (!ISAPNP_RegisterSysDev(tmp,i))
                    LOG_MSG("ISAPNP register failed\n");
            }

            /* register parallel ports */
            for (Bitu portn=0;portn < 3;portn++) {
                Bitu port = mem_readw(BIOS_ADDRESS_LPT1+(portn*2));
                if (port != 0) {
                    const unsigned char h1[9] = {
                        ISAPNP_SYSDEV_HEADER(
                            ISAPNP_ID('P','N','P',0x0,0x4,0x0,0x0), /* PNP0400 Standard LPT printer port */
                            ISAPNP_TYPE(0x07,0x01,0x00),        /* type: General parallel port */
                            0x0001 | 0x0002)
                    };

                    i = 0;
                    memcpy(tmp+i,h1,9); i += 9;         /* can't disable, can't configure */
                    /*----------allocated--------*/
                    tmp[i+0] = (8 << 3) | 7;            /* IO resource */
                    tmp[i+1] = 0x01;                /* 16-bit decode */
                    host_writew(tmp+i+2,port);          /* min */
                    host_writew(tmp+i+4,port);          /* max */
                    tmp[i+6] = 0x10;                /* align */
                    tmp[i+7] = 0x03;                /* length */
                    i += 7+1;

                    /* TODO: If/when LPT emulation handles the IRQ, add IRQ resource here */

                    tmp[i+0] = 0x79;                /* END TAG */
                    tmp[i+1] = 0x00;
                    i += 2;
                    /*-------------possible-----------*/
                    tmp[i+0] = 0x79;                /* END TAG */
                    tmp[i+1] = 0x00;
                    i += 2;
                    /*-------------compatible---------*/
                    tmp[i+0] = 0x79;                /* END TAG */
                    tmp[i+1] = 0x00;
                    i += 2;

                    if (!ISAPNP_RegisterSysDev(tmp,i))
                        LOG_MSG("ISAPNP register failed\n");
                }
            }

            void BIOS_Post_register_comports_PNP();
            BIOS_Post_register_comports_PNP();

            void BIOS_Post_register_IDE();
            BIOS_Post_register_IDE();

            void BIOS_Post_register_FDC();
            BIOS_Post_register_FDC();
        }

        if (IS_PC98_ARCH) {
            /* initialize IRQ0 timer to default tick interval.
             * PC-98 does not pre-initialize timer 0 of the PIT to 0xFFFF the way IBM PC/XT/AT do */
            PC98_Interval_Timer_Continue();
            PIC_SetIRQMask(0,true); /* PC-98 keeps the timer off unless INT 1Ch is called to set a timer interval */
        }

        if (!IS_PC98_ARCH) {
            Section_prop * section=static_cast<Section_prop *>(control->GetSection("speaker"));
            bool bit0en = section->Get_bool("pcspeaker clock gate enable at startup");

            if (bit0en) {
                uint8_t x = IO_Read(0x61);
                IO_Write(0x61,(x & (~3u)) | 1u); /* set bits[1:0] = 01  (clock gate enable but output gate disable) */
                LOG_MSG("xxxx");
            }
        }

	if (ACPI_enabled) {
		if (acpi_iocallout == IO_Callout_t_none)
			acpi_iocallout = IO_AllocateCallout(IO_TYPE_MB);
		if (acpi_iocallout == IO_Callout_t_none)
			E_Exit("Failed to get ACPI IO callout handle");

		{
			IO_CalloutObject *obj = IO_GetCallout(acpi_iocallout);
			if (obj == NULL) E_Exit("Failed to get ACPI IO callout");
			obj->Install(ACPI_IO_BASE,IOMASK_Combine(IOMASK_FULL,IOMASK_Range(0x20)),acpi_cb_port_r,acpi_cb_port_w);
			IO_PutCallout(obj);
		}

		BuildACPITable();
	}

        CPU_STI();

        return CBRET_NONE;
    }
    CALLBACK_HandlerObject cb_bios_scan_video_bios;
    static Bitu cb_bios_scan_video_bios__func(void) {
        unsigned long size;

        /* NTS: As far as I can tell, video is integrated into the PC-98 BIOS and there is no separate BIOS */
        if (IS_PC98_ARCH) return CBRET_NONE;

        if (cpu.pmode) E_Exit("BIOS error: VIDEO BIOS SCAN function called while in protected/vm86 mode");

        if (!bios_has_exec_vga_bios) {
            bios_has_exec_vga_bios = true;
            if (IS_EGAVGA_ARCH) {
                /* make sure VGA BIOS is there at 0xC000:0x0000 */
                if (AdapterROM_Read(0xC0000,&size)) {
                    LOG(LOG_MISC,LOG_DEBUG)("BIOS VIDEO ROM SCAN found VGA BIOS (size=%lu)",size);
                    adapter_scan_start = 0xC0000 + size;

                    // step back into the callback instruction that triggered this call
                    reg_eip -= 4;

                    // FAR CALL into the VGA BIOS
                    CPU_CALL(false,0xC000,0x0003,reg_eip);
                    return CBRET_NONE;
                }
                else {
                    LOG(LOG_MISC,LOG_WARN)("BIOS VIDEO ROM SCAN did not find VGA BIOS");
                }
            }
            else {
                // CGA, MDA, Tandy, PCjr. No video BIOS to scan for
            }
        }

        return CBRET_NONE;
    }
    CALLBACK_HandlerObject cb_bios_adapter_rom_scan;
    static Bitu cb_bios_adapter_rom_scan__func(void) {
        unsigned long size;
        uint32_t c1;

        /* FIXME: I have no documentation on how PC-98 scans for adapter ROM or even if it supports it */
        if (IS_PC98_ARCH) return CBRET_NONE;

        if (cpu.pmode) E_Exit("BIOS error: ADAPTER ROM function called while in protected/vm86 mode");

        while (adapter_scan_start < 0xF0000) {
            if (AdapterROM_Read(adapter_scan_start,&size)) {
                uint16_t segm = (uint16_t)(adapter_scan_start >> 4);

                LOG(LOG_MISC,LOG_DEBUG)("BIOS ADAPTER ROM scan found ROM at 0x%lx (size=%lu)",(unsigned long)adapter_scan_start,size);

                c1 = mem_readd(adapter_scan_start+3);
                adapter_scan_start += size;
                if (c1 != 0UL) {
                    LOG(LOG_MISC,LOG_DEBUG)("Running ADAPTER ROM entry point");

                    // step back into the callback instruction that triggered this call
                    reg_eip -= 4;

                    // FAR CALL into the VGA BIOS
                    CPU_CALL(false,segm,0x0003,reg_eip);
                    return CBRET_NONE;
                }
                else {
                    LOG(LOG_MISC,LOG_DEBUG)("FIXME: ADAPTER ROM entry point does not exist");
                }
            }
            else {
                if (IS_EGAVGA_ARCH) // supposedly newer systems only scan on 2KB boundaries by standard? right?
                    adapter_scan_start = (adapter_scan_start | 2047UL) + 1UL;
                else // while older PC/XT systems scanned on 512-byte boundaries? right?
                    adapter_scan_start = (adapter_scan_start | 511UL) + 1UL;
            }
        }

        LOG(LOG_MISC,LOG_DEBUG)("BIOS ADAPTER ROM scan complete");
        return CBRET_NONE;
    }
    CALLBACK_HandlerObject cb_bios_startup_screen;
    static Bitu cb_bios_startup_screen__func(void) {
        const Section_prop* section = static_cast<Section_prop *>(control->GetSection("dosbox"));
        const char *logo_text = section->Get_string("logo text");
        const char *logo = section->Get_string("logo");
        bool fastbioslogo=section->Get_bool("fastbioslogo")||control->opt_fastbioslogo||control->opt_fastlaunch;
        if (fastbioslogo && machine != MCH_PC98) {
#if defined(USE_TTF)
            if (TTF_using()) {
                uint32_t lasttick=GetTicks();
                while ((GetTicks()-lasttick)<500) {
                    reg_eax = 0x0100;
                    CALLBACK_RunRealInt(0x16);
                }
                reg_eax = 3;
                CALLBACK_RunRealInt(0x10);
            }
#endif
            if (control->opt_fastlaunch) return CBRET_NONE;
        }
        extern const char* RunningProgram;
        extern void GFX_SetTitle(int32_t cycles, int frameskip, Bits timing, bool paused);
        RunningProgram = "DOSBOX-X";
        GFX_SetTitle(-1,-1,-1,false);
        const char *msg = "DOSBox-X (C) 2011-" COPYRIGHT_END_YEAR " The DOSBox-X Team\nDOSBox-X project maintainer: joncampbell123\nDOSBox-X project homepage: https://dosbox-x.com\nDOSBox-X user guide: https://dosbox-x.com/wiki\n\n";
        bool textsplash = section->Get_bool("disable graphical splash");
#if defined(USE_TTF)
        if (TTF_using()) {
            textsplash = true;
            if (ttf.cols != 80 || ttf.lins != 25) {
                oldcols = ttf.cols;
                oldlins = ttf.lins;
            } else
                oldcols = oldlins = 0;
        }
#endif

	textsplash = true;

        char logostr[8][34];
        strcpy(logostr[0], "+---------------------+");
        strcpy(logostr[1], "|     Welcome  To     |");
        strcpy(logostr[2], "|                     |");
        strcpy(logostr[3], "|  D O S B o x - X !  |");
        strcpy(logostr[4], "|                     |");
        sprintf(logostr[5],"|     %d-bit %s     |",
        OS_BIT_INT, SDL_STRING);
        sprintf(logostr[6], "| Version %10s  |", VERSION);
        strcpy(logostr[7], "+---------------------+");
startfunction:
        int logo_x,logo_y,x=2,y=2;

        logo_y = 2;
        if (machine == MCH_HERC || machine == MCH_MDA)
            logo_x = 80 - 2 - (224/9);
        else
            logo_x = 80 - 2 - (224/8);

        if (cpu.pmode) E_Exit("BIOS error: STARTUP function called while in protected/vm86 mode");

        if (IS_VGA_ARCH) {
            reg_eax = 18;       // 640x480 16-color
            CALLBACK_RunRealInt(0x10);
        }
        else if (machine == MCH_PC98) {
            // clear the graphics layer
            for (unsigned int i=0;i < (80*400);i++) {
                mem_writeb(0xA8000+i,0);        // B
                mem_writeb(0xB0000+i,0);        // G
                mem_writeb(0xB8000+i,0);        // R
                mem_writeb(0xE0000+i,0);        // E
            }

            reg_eax = 0x0C00;   // enable text layer (PC-98)
            CALLBACK_RunRealInt(0x18);

            reg_eax = 0x1100;   // show cursor (PC-98)
            CALLBACK_RunRealInt(0x18);

            reg_eax = 0x1300;   // set cursor pos (PC-98)
            reg_edx = 0x0000;   // byte position
            CALLBACK_RunRealInt(0x18);

            bios_pc98_posx = x;

            reg_eax = 0x4200;   // setup 640x400 graphics
            reg_ecx = 0xC000;
            CALLBACK_RunRealInt(0x18);

            IO_Write(0x6A, 0x01);    // enable 16-color analog mode (this makes the 4th bitplane appear)
            IO_Write(0x6A, 0x04);    // but we don't need the EGC graphics
            IO_Write(0xA4, 0x00);    // display page 0
            IO_Write(0xA6, 0x00);    // write to page 0

            // setup palette for TTF mode
            for(unsigned int i = 0; i < 16; i++) {
                unsigned int bias = (i & 8) ? 0x5 : 0x0;

                IO_Write(0xA8, i);   // DAC index
                IO_Write(0xAA, ((i & 2) ? 0xA : 0x0) + bias);    // green
                IO_Write(0xAC, ((i & 4) ? 0xA : 0x0) + bias);    // red
                IO_Write(0xAE, ((i & 1) ? 0xA : 0x0) + bias);    // blue
            }
        }
        else {
            reg_eax = 3;        // 80x25 text
            CALLBACK_RunRealInt(0x10);
        }

#if defined(USE_TTF)
        if (TTF_using() && (ttf.cols != 80 || ttf.lins != 25)) ttf_setlines(80, 25);
#endif

        if (machine != MCH_PC98) {
            reg_eax = 0x0200;   // set cursor pos
            reg_ebx = 0;        // page zero
            reg_dh = y;     // row 4
            reg_dl = x;     // column 20
            CALLBACK_RunRealInt(0x10);
        }

        BIOS_Int10RightJustifiedPrint(x,y,msg);

        {
            png_bytep rows[1];
            unsigned char *row = NULL;/*png_width*/
            png_structp png_context = NULL;
            png_infop png_info = NULL;
            png_infop png_end = NULL;
            png_uint_32 png_width = 0,png_height = 0;
            int png_bit_depth = 0,png_color_type = 0,png_interlace = 0,png_filter = 0,png_compression = 0;
            png_color *palette = NULL;
            int palette_count = 0;
            std::string user_filename;
            unsigned int rowheight = 8;
            const char *filename = NULL;
            const unsigned char *inpng = NULL;
            size_t inpng_size = 0;
            FILE *png_fp = NULL;

            /* If the user wants a custom logo, just put it in the same directory as the .conf file and have at it.
             * Requirements: The PNG must be 1/2/4/8bpp with a color palette, not grayscale, not truecolor, and
             * no alpha channel data at all. No interlacing. Must be 224x224 or smaller, and should fit the size
             * indicated in the filename. There are multiple versions, one for each vertical resolution of common
             * CGA/EGA/VGA/etc. modes: 480-line, 400-line, 350-line, and 200-line. All images other than the 480-line
             * one have a non-square pixel aspect ratio. Please take that into consideration. */
            if (IS_VGA_ARCH) {
                if (logo) user_filename = std::string(logo) + "224x224.png";
                filename = "dosbox224x224.png";
                inpng_size = dosbox224x224_png_len;
                inpng = dosbox224x224_png;
                rowheight = 16;
            }
            else if (IS_PC98_ARCH || machine == MCH_MCGA) {
                if (logo) user_filename = std::string(logo) + "224x186.png";
                filename = "dosbox224x186.png";
                inpng_size = dosbox224x186_png_len;
                inpng = dosbox224x186_png;
                rowheight = 16;
            }
            else if (IS_EGA_ARCH) {
                if (ega200) {
                    if (logo) user_filename = std::string(logo) + "224x93.png";
                    filename = "dosbox224x93.png";
                    inpng_size = dosbox224x93_png_len;
                    inpng = dosbox224x93_png;
                }
                else {
                    if (logo) user_filename = std::string(logo) + "224x163.png";
                    filename = "dosbox224x163.png";
                    inpng_size = dosbox224x163_png_len;
                    inpng = dosbox224x163_png;
                    rowheight = 14;
                }
            }
            else if (machine == MCH_HERC || machine == MCH_MDA) {
                if (logo) user_filename = std::string(logo) + "224x163.png";
                filename = "dosbox224x163.png";
                inpng_size = dosbox224x163_png_len;
                inpng = dosbox224x163_png;
                rowheight = 14;
            }
            else {
                if (logo) user_filename = std::string(logo) + "224x93.png";
                filename = "dosbox224x93.png";
                inpng_size = dosbox224x93_png_len;
                inpng = dosbox224x93_png;
            }

            if (png_fp == NULL && !user_filename.empty())
                png_fp = fopen(user_filename.c_str(),"rb");
            if (png_fp == NULL && filename != NULL)
                png_fp = fopen(filename,"rb");

            if (png_fp || inpng) {
                png_context = png_create_read_struct(PNG_LIBPNG_VER_STRING,NULL/*err*/,NULL/*err fn*/,NULL/*warn fn*/);
                if (png_context) {
                    png_info = png_create_info_struct(png_context);
                    if (png_info) {
                        png_set_user_limits(png_context,320,320);
                    }
                }
            }

            if (png_context && png_info) {
                if (png_fp) {
                    LOG(LOG_MISC,LOG_DEBUG)("Using external file logo %s",filename);
                    png_init_io(png_context,png_fp);
                }
                else if (inpng) {
                    LOG(LOG_MISC,LOG_DEBUG)("Using built-in logo");
                    BIOSLOGO_PNG_PTR = inpng;
                    BIOSLOGO_PNG_FENCE = inpng + inpng_size;
                    png_set_read_fn(png_context,NULL,BIOSLOGO_PNG_READ);
                }
                else {
                    abort(); /* should not be here */
                }

                png_read_info(png_context,png_info);
                png_get_IHDR(png_context,png_info,&png_width,&png_height,&png_bit_depth,&png_color_type,&png_interlace,&png_compression,&png_filter);

                LOG(LOG_MISC,LOG_DEBUG)("BIOS png image: w=%u h=%u bitdepth=%u ct=%u il=%u compr=%u filt=%u",
                    png_width,png_height,png_bit_depth,png_color_type,png_interlace,png_compression,png_filter);

                if (png_width != 0 && png_height != 0 && png_bit_depth != 0 && png_bit_depth <= 8 &&
                    (png_color_type&(PNG_COLOR_MASK_PALETTE|PNG_COLOR_MASK_COLOR)) == (PNG_COLOR_MASK_PALETTE|PNG_COLOR_MASK_COLOR)/*palatted color only*/ &&
                    png_interlace == 0/*do not support interlacing*/) {
                    LOG(LOG_MISC,LOG_DEBUG)("PNG accepted");
                    /* please convert everything to 8bpp for us */
                    png_set_strip_16(png_context);
                    png_set_packing(png_context);
                    png_get_PLTE(png_context,png_info,&palette,&palette_count);

                    row = new unsigned char[png_width + 32];
                    rows[0] = row;

                    if (palette != 0 && palette_count > 0 && palette_count <= 256 && row != NULL) {
                        textsplash = false;
                        if (machine == MCH_HERC || machine == MCH_MDA)
                            VGA_InitBiosLogo(png_width,png_height,logo_x*9,logo_y*rowheight);
                        else
                            VGA_InitBiosLogo(png_width,png_height,logo_x*8,logo_y*rowheight);

                        {
                            unsigned char tmp[256*3];
                            for (unsigned int x=0;x < (unsigned int)palette_count;x++) {
                                tmp[(x*3)+0] = palette[x].red;
                                tmp[(x*3)+1] = palette[x].green;
                                tmp[(x*3)+2] = palette[x].blue;
                            }
                            VGA_WriteBiosLogoPalette(0,palette_count,tmp);
                        }

                        for (unsigned int y=0;y < png_height;y++) {
                            png_read_rows(png_context,rows,NULL,1);
                            VGA_WriteBiosLogoBMP(y,row,png_width);
                        }
                        VGA_ShowBIOSLogo();
                    }

                    delete[] row;
                }
            }

            if (png_context) png_destroy_read_struct(&png_context,&png_info,&png_end);
            if (png_fp) fclose(png_fp);
        }

        if (machine == MCH_PC98 && textsplash) {
            unsigned int bo, lastline = 7;
            for (unsigned int i=0; i<=lastline; i++) {
                for (unsigned int j=0; j<strlen(logostr[i]); j++) {
                    bo = (((unsigned int)(i+2) * 80u) + (unsigned int)(j+0x36)) * 2u;
                    mem_writew(0xA0000+bo,i==0&&j==0?0x300B:(i==0&&j==strlen(logostr[0])-1?0x340B:(i==lastline&&j==0?0x380B:(i==lastline&&j==strlen(logostr[lastline])-1?0x3C0B:(logostr[i][j]=='-'&&(i==0||i==lastline)?0x240B:(logostr[i][j]=='|'?0x260B:logostr[i][j]%0xff))))));
                    mem_writeb(0xA2000+bo+1,0xE1);
                }
            }
        }
        if (machine != MCH_PC98 && textsplash) {
            Bitu edx = reg_edx;
            //int oldx = x, oldy = y; UNUSED
            unsigned int lastline = 7;
            for (unsigned int i=0; i<=lastline; i++) {
                for (unsigned int j=0; j<strlen(logostr[i]); j++) {
                    reg_eax = 0x0200u;
                    reg_ebx = 0x0000u;
                    reg_edx = 0x0236u + i*0x100 + j;
                    CALLBACK_RunRealInt(0x10);
                    reg_eax = 0x0900u+(i==0&&j==0?0xDA:(i==0&&j==strlen(logostr[0])-1?0xBF:(i==lastline&&j==0?0xC0:(i==lastline&&j==strlen(logostr[lastline])-1?0xD9:(logostr[i][j]=='-'&&(i==0||i==lastline)?0xC4:(logostr[i][j]=='|'?0xB3:logostr[i][j]%0xff))))));
                    reg_ebx = i!=0&&i!=lastline&&logostr[i][j]!='|'?0x002eu:0x002fu;
                    reg_ecx = 0x0001u;
                    CALLBACK_RunRealInt(0x10);
                }
            }
            reg_eax = 0x0200u;
            reg_ebx = 0x0000u;
            reg_edx = edx;
            CALLBACK_RunRealInt(0x10);
        }

        {
            uint64_t sz = (uint64_t)MEM_TotalPages() * (uint64_t)4096;
            char tmp[512];

            if (sz >= ((uint64_t)128 << (uint64_t)20))
                sprintf(tmp,"%uMB memory installed\r\n",(unsigned int)(sz >> (uint64_t)20));
            else
                sprintf(tmp,"%uKB memory installed\r\n",(unsigned int)(sz >> (uint64_t)10));

            BIOS_Int10RightJustifiedPrint(x,y,tmp);
        }

        const char *card = "Unknown Graphics Adapter";

        switch (machine) {
            case MCH_CGA:
                card = "IBM Color Graphics Adapter";
                break;
            case MCH_MCGA:
                card = "IBM Multi Color Graphics Adapter";
                break;
            case MCH_MDA:
                card = "IBM Monochrome Display Adapter";
                break;
            case MCH_HERC:
                switch (hercCard) {
                    case HERC_GraphicsCardPlus:
                        card = "Hercules+ Graphics Adapter";
                        break;
                    case HERC_InColor:
                        card = "Hercules InColor Graphics Adapter";
                        break;
                    default:
                        card = "Hercules Graphics Adapter";
                        break;
                }
                break;
            case MCH_EGA:
                card = "IBM Enhanced Graphics Adapter";
                break;
            case MCH_PCJR:
                card = "PCjr Graphics Adapter";
                break;
            case MCH_TANDY:
                card = "Tandy Graphics Adapter";
                break;
            case MCH_VGA:
                switch (svgaCard) {
                    case SVGA_TsengET4K:
                        card = "Tseng ET4000 SVGA";
                        break;
                    case SVGA_TsengET3K:
                        card = "Tseng ET3000 SVGA";
                        break;
                    case SVGA_ParadisePVGA1A:
                        card = "Paradise SVGA";
                        break;
                    case SVGA_S3Trio:
                        card = "S3 Trio SVGA";
                        switch (s3Card) {
                            case S3_86C928:     card = "S3 86C928"; break;
                            case S3_Vision864:  card = "S3 Vision864 SVGA"; break;
                            case S3_Vision868:  card = "S3 Vision868 SVGA"; break;
                            case S3_Vision964:  card = "S3 Vision964 SVGA"; break;
                            case S3_Vision968:  card = "S3 Vision968 SVGA"; break;
                            case S3_Trio32:     card = "S3 Trio32 SVGA"; break;
                            case S3_Trio64:     card = "S3 Trio64 SVGA"; break;
                            case S3_Trio64V:    card = "S3 Trio64V+ SVGA"; break;
                            case S3_ViRGE:      card = "S3 ViRGE SVGA"; break;
                            case S3_ViRGEVX:    card = "S3 ViRGE VX SVGA"; break;
                            case S3_Generic:    card = "S3"; break;
                        }
                        break;
                    case SVGA_ATI:
                        card = "ATI SVGA";
                        switch (atiCard) {
                            case ATI_EGAVGAWonder:     card = "ATI EGA/VGA Wonder"; break;
                            case ATI_VGAWonder:        card = "ATI VGA Wonder"; break;
                            case ATI_VGAWonderPlus:    card = "ATI VGA Wonder+"; break;
                            case ATI_VGAWonderXL:      card = "ATI VGA WonderXL"; break;
                            case ATI_VGAWonderXL24:    card = "ATI VGA WonderXL24"; break;
                            case ATI_Mach8:            card = "ATI Mach8"; break;
                            case ATI_Mach32:           card = "ATI Mach32"; break;
                            case ATI_Mach64:           card = "ATI Mach64"; break;
                        }
                        break;
                    default:
                        card = "Standard VGA";
                        break;
                }

                break;
            case MCH_PC98:
                card = "PC98 graphics";
                break;
            case MCH_AMSTRAD:
                card = "Amstrad graphics";
                break;
            default:
                abort(); // should not happen
        }

        {
            char tmp[512];
            sprintf(tmp,"Video card is %s\n",card);
            BIOS_Int10RightJustifiedPrint(x,y,tmp);
        }

        {
            char tmp[512];
            const char *cpuType = "?";

            switch (CPU_ArchitectureType) {
                case CPU_ARCHTYPE_8086:
                    cpuType = "8086";
                    break;
                case CPU_ARCHTYPE_80186:
                    cpuType = "80186";
                    break;
                case CPU_ARCHTYPE_286:
                    cpuType = "286";
                    break;
                case CPU_ARCHTYPE_386:
                    cpuType = "386";
                    break;
                case CPU_ARCHTYPE_486OLD:
                    cpuType = "486 (older generation)";
                    break;
                case CPU_ARCHTYPE_486NEW:
                    cpuType = "486 (later generation)";
                    break;
                case CPU_ARCHTYPE_PENTIUM:
                    cpuType = "Pentium";
                    break;
                case CPU_ARCHTYPE_PMMXSLOW:
                    cpuType = "Pentium MMX";
                    break;
                case CPU_ARCHTYPE_PPROSLOW:
                    cpuType = "Pentium Pro";
		    break;
		case CPU_ARCHTYPE_PENTIUMII:
		    cpuType = "Pentium II";
		    break;
		case CPU_ARCHTYPE_PENTIUMIII:
		    cpuType = "Pentium III";
		    break;
                case CPU_ARCHTYPE_MIXED:
                    cpuType = "Auto (mixed)";
                    break;
                case CPU_ARCHTYPE_EXPERIMENTAL:
                    cpuType = "Experimental";
                    break;
            }

            sprintf(tmp,"%s CPU present",cpuType);
            BIOS_Int10RightJustifiedPrint(x,y,tmp);
            if (enable_fpu) BIOS_Int10RightJustifiedPrint(x,y," with FPU");
            BIOS_Int10RightJustifiedPrint(x,y,"\n");
        }

        if (APMBIOS) {
            BIOS_Int10RightJustifiedPrint(x,y,"Advanced Power Management interface active\n");
        }

        if (ISAPNPBIOS) {
            BIOS_Int10RightJustifiedPrint(x,y,"ISA Plug & Play BIOS active\n");
        }

        if (*logo_text) {
            const size_t max_w = 76;
            const char *s = logo_text;
            const int saved_y = y;
            size_t max_h;
            char tmp[81];
            int x,y;

            x = 0; /* use it here as index to tmp[] */
            if (IS_VGA_ARCH) /* VGA 640x480 has 30 lines (480/16) not 25 */
                max_h = 30;
            else
                max_h = 25;
            y = max_h - 3;

            y--;
            BIOS_Int10RightJustifiedPrint(x+2,y,"\n"); /* sync cursor */

            while (*s) {
                bool newline = false;

                assert((size_t)x < max_w);
                if (isalpha(*s) || isdigit(*s)) {
                    size_t wi = 1;/*we already know s[0] fits the criteria*/
                    while (s[wi] != 0 && (isalpha(s[wi]) || isdigit(s[wi]))) wi++;

                    if (wi >= 24) { /* don't let overlong words crowd out the text */
                        if (((size_t)x+wi) > max_w)
                            wi = max_w - (size_t)x;
                    }

                    if (((size_t)x+wi) < max_w) {
                        memcpy(tmp+x,s,wi);
                        x += wi;
                        s += wi;
                    }
                    else {
                        newline = true;
                    }
                }
                else if (*s == ' ') {
                    if ((size_t)x < max_w) tmp[x++] = *s++;

                    if ((size_t)x == max_w) {
                        while (*s == ' ') s++;
                        newline = true;
                    }
                }
                else if (*s == '\\') {
                    s++;
                    if (*s == 'n') {
                        newline = true; /* \n */
                        s++;
                    }
                    else {
                        s++;
                    }
                }
                else {
                    tmp[x++] = *s++;
                }

                assert((size_t)x <= max_w);
                if ((size_t)x >= max_w || newline) {
                    tmp[x] = 0;
                    BIOS_Int10RightJustifiedPrint(x+2,y,tmp);
                    x = 0;
                    BIOS_Int10RightJustifiedPrint(x+2,y,"\n"); /* next line, which increments y */
                    if ((size_t)y >= max_h) break;
                }
            }

            if (x != 0 && (size_t)y < max_h) {
                tmp[x] = 0;
                BIOS_Int10RightJustifiedPrint(x+2,y,tmp);
                x = 0;
                BIOS_Int10RightJustifiedPrint(x+2,y,"\n"); /* next line, which increments y */
            }

            y = saved_y - 1;
            BIOS_Int10RightJustifiedPrint(x+2,y,"\n"); /* sync cursor */
        }

#if !defined(C_EMSCRIPTEN)
        BIOS_Int10RightJustifiedPrint(x,y,"\nHit SPACEBAR to pause at this screen\n", false, true);
        BIOS_Int10RightJustifiedPrint(x,y,"\nPress DEL to enter BIOS setup screen\n", false, true);
        y--; /* next message should overprint */
        if (machine != MCH_PC98) {
            reg_eax = 0x0200;   // set cursor pos
            reg_ebx = 0;        // page zero
            reg_dh = y;     // row 4
            reg_dl = x;     // column 20
            CALLBACK_RunRealInt(0x10);
        }
        else {
            reg_eax = 0x1300u;   // set cursor pos (PC-98)
            reg_edx = (((unsigned int)y * 80u) + (unsigned int)x) * 2u; // byte position
            CALLBACK_RunRealInt(0x18);
        }
#endif

        // TODO: Then at this screen, we can print messages demonstrating the detection of
        //       IDE devices, floppy, ISA PnP initialization, anything of importance.
        //       I also envision adding the ability to hit DEL or F2 at this point to enter
        //       a "BIOS setup" screen where all DOSBox-X configuration options can be
        //       modified, with the same look and feel of an old BIOS.

#if C_EMSCRIPTEN
        uint32_t lasttick=GetTicks();
        while ((GetTicks()-lasttick)<1000) {
            void CALLBACK_Idle(void);
            CALLBACK_Idle();
            emscripten_sleep(100);
        }
#else
        if (!fastbioslogo&&!bootguest&&!bootfast&&(bootvm||!use_quick_reboot)) {
            bool wait_for_user = false, bios_setup = false;
            int pos=1;
            uint32_t lasttick=GetTicks();
            while ((GetTicks()-lasttick)<1000) {
                if (machine == MCH_PC98) {
                    reg_eax = 0x0100;   // sense key
                    CALLBACK_RunRealInt(0x18);
                    SETFLAGBIT(ZF,reg_bh == 0);
                }
                else {
                    reg_eax = 0x0100;
                    CALLBACK_RunRealInt(0x16);
                }

                if (!GETFLAG(ZF)) {
                    if (machine == MCH_PC98) {
                        reg_eax = 0x0000;   // read key
                        CALLBACK_RunRealInt(0x18);
                    }
                    else {
                        reg_eax = 0x0000;
                        CALLBACK_RunRealInt(0x16);
                    }

                    if (reg_al == 32) { // user hit space
                        BIOS_Int10RightJustifiedPrint(x,y,"Hit ENTER or ESC to continue                    \n", false, true); // overprint
                        BIOS_Int10RightJustifiedPrint(x,y,"\nPress DEL to enter BIOS setup screen\n", false, true);
                        wait_for_user = true;
                        break;
                    }

                    if ((machine != MCH_PC98 && reg_ax == 0x5300) || (machine == MCH_PC98 && reg_ax == 0x3900)) { // user hit Del
                        bios_setup = true;
                        showBIOSSetup(card, x, y);
                        break;
                    }
                }
            }

            while (wait_for_user) {
                if (machine == MCH_PC98) {
                    reg_eax = 0x0000;   // read key
                    CALLBACK_RunRealInt(0x18);
                }
                else {
                    reg_eax = 0x0000;
                    CALLBACK_RunRealInt(0x16);
                }

                if ((machine != MCH_PC98 && reg_ax == 0x5300/*DEL*/) || (machine == MCH_PC98 && reg_ax == 0x3900)) {
                    bios_setup = true;
                    VGA_FreeBiosLogo();
                    showBIOSSetup(card, x, y);
                    break;
                }

                if (reg_al == 27/*ESC*/ || reg_al == 13/*ENTER*/)
                    break;
            }

            lasttick=GetTicks();
            bool askexit = false, mod = false;
            while (bios_setup) {
                if (GetTicks()-lasttick>=500 && !askexit) {
                    lasttick=GetTicks();
                    updateDateTime(x,y,pos);
                }
                if (machine == MCH_PC98) {
                    reg_eax = 0x0100;   // sense key
                    CALLBACK_RunRealInt(0x18);
                    SETFLAGBIT(ZF,reg_bh == 0);
                }
                else {
                    reg_eax = 0x0100;
                    CALLBACK_RunRealInt(0x16);
                }

                if (!GETFLAG(ZF)) {
                    if (machine == MCH_PC98) {
                        reg_eax = 0x0000;   // read key
                        CALLBACK_RunRealInt(0x18);
                    }
                    else {
                        reg_eax = 0x0000;
                        CALLBACK_RunRealInt(0x16);
                    }
                    if (askexit) {
                        if (reg_al == 'Y' || reg_al == 'y') {
                            if (machine == MCH_PC98) {
                                reg_eax = 0x1600;
                                reg_edx = 0xE100;
                                CALLBACK_RunRealInt(0x18);
                            }
                            goto startfunction;
                        } else if (machine == MCH_PC98) {
                            const char *exitstr = "ESC = Exit";
                            unsigned int bo;
                            for (unsigned int i=0; i<strlen(exitstr); i++) {
                                bo = (((unsigned int)24 * 80u) + (unsigned int)0x22+i) * 2u;
                                mem_writew(0xA0000+bo,(unsigned char)exitstr[i]);
                                mem_writeb(0xA2000+bo,0xE1);
                            }
                            askexit = false;
                            continue;
                        } else {
                            reg_eax = 0x0200u;
                            reg_ebx = 0x0000u;
                            reg_edx = 0x1800u;
                            CALLBACK_RunRealInt(0x10);
                            BIOS_Int10RightJustifiedPrint(x,y,"              ESC: Exit  Arrows: Select Item  +/-: Change Values              ");
                            askexit = false;
                            continue;
                        }
                    }
                    if ((machine != MCH_PC98 && reg_ax == 0x4B00) || (machine == MCH_PC98 && reg_ax == 0x3B00)) { // Left key
                        pos=pos>1?pos-1:6;
                        lasttick-=500;
                    } else if ((machine != MCH_PC98 && reg_ax == 0x4D00) || (machine == MCH_PC98 && reg_ax == 0x3C00)) { // Right key
                        pos=pos<6?pos+1:1;
                        lasttick-=500;
                    } else if (((machine != MCH_PC98 && reg_ax == 0x4800) || (machine == MCH_PC98 && reg_ax == 0x3A00)) && pos>3) { // Up key
                        if (pos==4||pos==5) pos=1;
                        else if (pos==6) pos=2;
                        lasttick-=500;
                    } else if (((machine != MCH_PC98 && reg_ax == 0x5000) || (machine == MCH_PC98 && reg_ax == 0x3D00)) && pos<4) { // Down key
                        if (pos==1) pos=4;
                        else if (pos==2||pos==3) pos=6;
                        lasttick-=500;
                    } else if (machine != MCH_PC98 && reg_al == 43) { // '+' key
                        if (pos==1&&dos.date.year<2100) dos.date.year++;
                        else if (pos==2) dos.date.month=dos.date.month<12?dos.date.month+1:1;
                        else if (pos==3) dos.date.day=dos.date.day<(dos.date.month==1||dos.date.month==3||dos.date.month==5||dos.date.month==7||dos.date.month==8||dos.date.month==10||dos.date.month==12?31:(dos.date.month==2?29:30))?dos.date.day+1:1;
                        else if (pos==4||pos==5||pos==6) {
                            Bitu time=(Bitu)((100.0/((double)PIT_TICK_RATE/65536.0)) * mem_readd(BIOS_TIMER))/100;
                            unsigned int sec=(uint8_t)((Bitu)time % 60);
                            time/=60;
                            unsigned int min=(uint8_t)((Bitu)time % 60);
                            time/=60;
                            unsigned int hour=(uint8_t)((Bitu)time % 24);
                            if (pos==4) hour=hour<23?hour+1:0;
                            else if (pos==5) min=min<59?min+1:0;
                            else if (pos==6) sec=sec<59?sec+1:0;
                            mem_writed(BIOS_TIMER,(uint32_t)((double)hour*3600+min*60+sec)*18.206481481);
                        }
                        mod = true;
                        if (sync_time) {manualtime=true;mainMenu.get_item("sync_host_datetime").check(false).refresh_item(mainMenu);}
                        lasttick-=500;
                    } else if (machine != MCH_PC98 && reg_al == 45) { // '-' key
                        if (pos==1&&dos.date.year>1900) dos.date.year--;
                        else if (pos==2) dos.date.month=dos.date.month>1?dos.date.month-1:12;
                        else if (pos==3) dos.date.day=dos.date.day>1?dos.date.day-1:(dos.date.month==1||dos.date.month==3||dos.date.month==5||dos.date.month==7||dos.date.month==8||dos.date.month==10||dos.date.month==12?31:(dos.date.month==2?29:30));
                        else if (pos==4||pos==5||pos==6) {
                            Bitu time=(Bitu)((100.0/((double)PIT_TICK_RATE/65536.0)) * mem_readd(BIOS_TIMER))/100;
                            unsigned int sec=(uint8_t)(time % 60);
                            time/=60;
                            unsigned int min=(uint8_t)(time % 60);
                            time/=60;
                            unsigned int hour=(uint8_t)(time % 24);
                            if (pos==4) hour=hour>0?hour-1:23;
                            else if (pos==5) min=min>0?min-1:59;
                            else if (pos==6) sec=sec>0?sec-1:59;
                            mem_writed(BIOS_TIMER,(uint32_t)((double)hour*3600+min*60+sec)*18.206481481);
                        }
                        mod = true;
                        if (sync_time) {manualtime=true;mainMenu.get_item("sync_host_datetime").check(false).refresh_item(mainMenu);}
                        lasttick-=500;
                    } else if (reg_al == 27/*ESC*/) {
                        if (machine == MCH_PC98) {
                            const char *exitstr = "Exit[Y/N]?";
                            unsigned int bo;
                            for (unsigned int i=0; i<strlen(exitstr); i++) {
                                bo = (((unsigned int)24 * 80u) + (unsigned int)0x22+i) * 2u;
                                mem_writew(0xA0000+bo,(unsigned char)exitstr[i]);
                                mem_writeb(0xA2000+bo,0xE1);
                            }
                        } else {
                            reg_eax = 0x0200u;
                            reg_ebx = 0x0000u;
                            reg_edx = 0x1800u;
                            CALLBACK_RunRealInt(0x10);
                            if (mod)
                                BIOS_Int10RightJustifiedPrint(x,y,"              Save settings and exit the BIOS Setup Utility [Y/N]? ");
                            else
                                BIOS_Int10RightJustifiedPrint(x,y,"              Exit the BIOS Setup Utility and reboot system [Y/N]? ");
                        }
                        askexit = true;
                    }
                }
            }
        }
#endif

        VGA_FreeBiosLogo();
        if (machine == MCH_PC98) {
            reg_eax = 0x4100;   // hide the graphics layer (PC-98)
            CALLBACK_RunRealInt(0x18);

            // clear the graphics layer
            for (unsigned int i=0;i < (80*400);i++) {
                mem_writeb(0xA8000+i,0);        // B
                mem_writeb(0xB0000+i,0);        // G
                mem_writeb(0xB8000+i,0);        // R
                mem_writeb(0xE0000+i,0);        // E
            }

            IO_Write(0x6A,0x00);    // switch back to 8-color mode

            reg_eax = 0x4200;   // setup 640x200 graphics
            reg_ecx = 0x8000;   // lower
            CALLBACK_RunRealInt(0x18);
            if (textsplash) {
                reg_eax = 0x1600;
                reg_edx = 0xE100;
                CALLBACK_RunRealInt(0x18);
            }
        }
        else {
            // restore 80x25 text mode
            reg_eax = 3;
            CALLBACK_RunRealInt(0x10);
        }
#if defined(USE_TTF)
        if (TTF_using() && oldcols>0 && oldlins>0) {
            ttf_setlines(oldcols, oldlins);
            oldcols = oldlins = 0;
        }
#endif

        return CBRET_NONE;
    }
    CALLBACK_HandlerObject cb_bios_boot;
    CALLBACK_HandlerObject cb_bios_bootfail;
    CALLBACK_HandlerObject cb_pc98_rombasic; /* hardcoded entry point used by various PC-98 games that jump to N88 ROM BASIC */
    CALLBACK_HandlerObject cb_ibm_basic; /* hardcoded entry point used by MS-DOS 1.x BASIC.COM and BASICA.COM to jump to IBM ROM BASIC (F600:4C79) */
    static Bitu cb_pc98_entry__func(void) {
        /* the purpose of this function is to say "N88 ROM BASIC NOT FOUND" */
        int x,y;

        x = y = 0;

        /* PC-98 MS-DOS boot sector may RETF back to the BIOS, and this is where execution ends up */
        BIOS_Int10RightJustifiedPrint(x,y,"N88 ROM BASIC NOT IMPLEMENTED");

        return CBRET_NONE;
    }
    static Bitu cb_ibm_basic_entry__func(void) {
        /* the purpose of this function is to say "IBM ROM BASIC NOT FOUND" */
        int x,y;

        x = y = 0;

        /* PC-98 MS-DOS boot sector may RETF back to the BIOS, and this is where execution ends up */
        BIOS_Int10RightJustifiedPrint(x,y,"IBM ROM BASIC NOT IMPLEMENTED");

        return CBRET_NONE;
    }
    static Bitu cb_bios_bootfail__func(void) {
        int x,y;

        x = y = 0;

        /* PC-98 MS-DOS boot sector may RETF back to the BIOS, and this is where execution ends up */
        BIOS_Int10RightJustifiedPrint(x,y,"Guest OS failed to boot, returned failure");

        /* and then after this call, there is a JMP $ to loop endlessly */
        return CBRET_NONE;
    }
    static Bitu cb_bios_boot__func(void) {
        /* Reset/power-on overrides the user's A20 gate preferences.
         * It's time to revert back to what the user wants. */
        void A20Gate_TakeUserSetting(Section *sec);
        void MEM_A20_Enable(bool enabled);
        A20Gate_TakeUserSetting(NULL);
        MEM_A20_Enable(false);

        if (cpu.pmode) E_Exit("BIOS error: BOOT function called while in protected/vm86 mode");
        DispatchVMEvent(VM_EVENT_BIOS_BOOT);

        // TODO: If instructed to, follow the INT 19h boot pattern, perhaps follow the BIOS Boot Specification, etc.

        // TODO: If instructed to boot a guest OS...

        /* wipe out the stack so it's not there to interfere with the system, point it at top of memory or top of segment */
        reg_esp = std::min((unsigned int)((MEM_ConventionalPages() << 12) - 0x600 - 4),0xFFFCu);
        reg_eip = 0;
        CPU_SetSegGeneral(cs, 0x60);
        CPU_SetSegGeneral(ss, 0x60);

        LOG(LOG_MISC,LOG_DEBUG)("BIOS boot SS:SP %04x:%04x",(unsigned int)0x60,(unsigned int)reg_esp);

        for (Bitu i=0;i < 0x400;i++) mem_writeb(0x7C00+i,0);
		if ((bootguest||(!bootvm&&use_quick_reboot))&&!bootfast&&bootdrive>=0&&imageDiskList[bootdrive]) {
			MOUSE_Startup(NULL);
			char drive[] = "-QQ A:";
			drive[4]='A'+bootdrive;
			runBoot(drive);
		}
        if (!bootguest&&!bootvm&&!bootfast&&bootdrive>=0) {
            void IDE_CDROM_DetachAll();
            IDE_CDROM_DetachAll();
        }
		if ((use_quick_reboot||IS_DOSV)&&!bootvm&&!bootfast&&bootdrive<0&&first_shell != NULL) throw int(6);

		bootvm=false;
		bootfast=false;
		bootguest=false;
		bootdrive=-1;
        // Begin booting the DOSBox-X shell. NOTE: VM_Boot_DOSBox_Kernel will change CS:IP instruction pointer!
        if (!VM_Boot_DOSBox_Kernel()) E_Exit("BIOS error: BOOT function failed to boot DOSBox-X kernel");
        return CBRET_NONE;
    }
public:
    void write_FFFF_signature() {
        /* write the signature at 0xF000:0xFFF0 */

        // The farjump at the processor reset entry point (jumps to POST routine)
        phys_writeb(0xffff0,0xEA);                  // FARJMP
        phys_writew(0xffff1,RealOff(BIOS_DEFAULT_RESET_LOCATION));  // offset
        phys_writew(0xffff3,RealSeg(BIOS_DEFAULT_RESET_LOCATION));  // segment

        // write system BIOS date
        for(Bitu i = 0; i < strlen(bios_date_string); i++) phys_writeb(0xffff5+i,(uint8_t)bios_date_string[i]);

        /* model byte */
        if (machine==MCH_TANDY || machine==MCH_AMSTRAD) phys_writeb(0xffffe,0xff);  /* Tandy model */
        else if (machine==MCH_PCJR) phys_writeb(0xffffe,0xfd);  /* PCJr model */
        else if (machine==MCH_MCGA) phys_writeb(0xffffe,0xfa);  /* PC/2 model 30 model */
        else phys_writeb(0xffffe,0xfc); /* PC (FIXME: This is listed as model byte PS/2 model 60) */

        // signature
        if (machine==MCH_TANDY)
            phys_writeb(0xfffff,0xff); // Needed for Ninja (1986)
        else
            phys_writeb(0xfffff,0x55);
    }
    BIOS(Section* configuration):Module_base(configuration){
        isapnp_biosstruct_base = 0;

        { // TODO: Eventually, move this to BIOS POST or init phase
            Section_prop * section=static_cast<Section_prop *>(control->GetSection("dosbox"));
			Section_prop * pc98_section=static_cast<Section_prop *>(control->GetSection("pc98"));

            pc98_timestamp5c = pc98_section->Get_bool("pc-98 time stamp");

            enable_pc98_copyright_string = pc98_section->Get_bool("pc-98 BIOS copyright string");

            // NTS: This setting is also valid in PC-98 mode. According to Undocumented PC-98 by Webtech,
            //      there's nothing at I/O port E9h. I will move the I/O port in PC-98 mode if there is in
            //      fact a conflict. --J.C.
            bochs_port_e9 = section->Get_bool("bochs debug port e9");

            // TODO: motherboard init, especially when we get around to full Intel Triton/i440FX chipset emulation
            {
                std::string s = section->Get_string("isa memory hole at 512kb");

                if (s == "true" || s == "1")
                    isa_memory_hole_512kb = true;
                else if (s == "false" || s == "0")
                    isa_memory_hole_512kb = false;
                else
                    isa_memory_hole_512kb = false;
            }

            // TODO: motherboard init, especially when we get around to full Intel Triton/i440FX chipset emulation
            {
                std::string s = section->Get_string("isa memory hole at 15mb");

                // Do NOT emulate the memory hole if emulating 24 or less address bits! BIOS crashes will result at startup!
                // The whole point of the 15MB memory hole is to emulate a hole into hardware as if a 24-bit 386SX. A memalias
                // setting of 24 makes it redundant. Furthermore memalias=24 and 15MB memory hole prevents the BIOS from
                // mapping correctly and crashes immediately at startup. This is especially necessary for PC-98 mode where
		// memalias==24 and memory hole enabled for the PEGC linear framebuffer prevents booting.

                if (MEM_get_address_bits() <= 24)
                    isa_memory_hole_15mb = false;
                else if (s == "true" || s == "1")
                    isa_memory_hole_15mb = true;
                else if (s == "false" || s == "0")
                    isa_memory_hole_15mb = false;
                else if (IS_PC98_ARCH)
                    isa_memory_hole_15mb = true; // For the sake of some PC-98 DOS games, enable by default
                else
                    isa_memory_hole_15mb = false;
            }

            // FIXME: Erm, well this could've been named better. It refers to the amount of conventional memory
            //        made available to the operating system below 1MB, which is usually DOS.
            dos_conventional_limit = (unsigned int)section->Get_int("dos mem limit");

            // for PC-98: When accessing the floppy through INT 1Bh, when enabled, run through a waiting loop to make sure
            //     the timer count is not too high on exit (Ys II)
            enable_fdc_timer_hack = pc98_section->Get_bool("pc-98 int 1b fdc timer wait");

            {
                std::string s = section->Get_string("unhandled irq handler");

                if (s == "simple")
                    unhandled_irq_method = UNHANDLED_IRQ_SIMPLE;
                else if (s == "cooperative_2nd")
                    unhandled_irq_method = UNHANDLED_IRQ_COOPERATIVE_2ND;
                // pick default
                else if (IS_PC98_ARCH)
                    unhandled_irq_method = UNHANDLED_IRQ_COOPERATIVE_2ND;
                else
                    unhandled_irq_method = UNHANDLED_IRQ_SIMPLE;
            }
        }

	if (IS_PC98_ARCH) {
		/* Keyboard translation tables, must exist at segment 0xFD80:0x0E00 because PC-98 MS-DOS assumes it (it writes 0x522 itself on boot) */
		/* The table must be placed back far enough so that (0x60 * 10) bytes do not overlap the lookup table at 0xE28 */
		BIOS_PC98_KEYBOARD_TRANSLATION_LOCATION = PhysToReal416(ROMBIOS_GetMemory(0x60 * 10,"Keyboard translation tables",/*align*/1,0xFD800+0xA13));
		if (ROMBIOS_GetMemory(0x2 * 10,"Keyboard translation shift tables",/*align*/1,0xFD800+0xE28) == (~0u)) E_Exit("Failed to allocate shift tables");//reserve it
		BIOSKEY_PC98_Write_Tables();
	}

        /* pick locations */
	/* IBM PC mode: See [https://github.com/skiselev/8088_bios/blob/master/bios.asm]. Some values also provided by Allofich.
	 * PCJr: The BIOS jumps to an address much lower in segment F000, low enough that the second byte of the offset is zero.
	 *       "Pitstop II" uses that as a method to test for PCjr [https://www.vogons.org/viewtopic.php?t=50417] */
	if (machine == MCH_PCJR)
		BIOS_DEFAULT_RESET_LOCATION = PhysToReal416(ROMBIOS_GetMemory(3/*JMP NEAR*/,"BIOS default reset location (JMP, PCjr style)",/*align*/1,0xF0043));
        else if (IS_PC98_ARCH)
		BIOS_DEFAULT_RESET_LOCATION = PhysToReal416(ROMBIOS_GetMemory(5/*JMP NEAR*/,"BIOS default reset location (JMP, PC-98)",/*align*/1,IS_PC98_ARCH ? 0 : 0xFE05B));
	else
		BIOS_DEFAULT_RESET_LOCATION = PhysToReal416(ROMBIOS_GetMemory(3/*JMP NEAR*/,"BIOS default reset location (JMP)",/*align*/1,IS_PC98_ARCH ? 0 : 0xFE05B));

        BIOS_DEFAULT_RESET_CODE_LOCATION = PhysToReal416(ROMBIOS_GetMemory(64/*several callbacks*/,"BIOS default reset location (CODE)",/*align*/1));
        BIOS_DEFAULT_HANDLER_LOCATION = PhysToReal416(ROMBIOS_GetMemory(1/*IRET*/,"BIOS default handler location",/*align*/1,IS_PC98_ARCH ? 0 : 0xFFF53));
        BIOS_DEFAULT_INT5_LOCATION = PhysToReal416(ROMBIOS_GetMemory(1/*IRET*/, "BIOS default INT5 location",/*align*/1,IS_PC98_ARCH ? 0 : 0xFFF54));
        BIOS_DEFAULT_IRQ0_LOCATION = PhysToReal416(ROMBIOS_GetMemory(0x13/*see callback.cpp for IRQ0*/,"BIOS default IRQ0 location",/*align*/1,IS_PC98_ARCH ? 0 : 0xFFEA5));
        BIOS_DEFAULT_IRQ1_LOCATION = PhysToReal416(ROMBIOS_GetMemory(0x20/*see callback.cpp for IRQ1*/,"BIOS default IRQ1 location",/*align*/1,IS_PC98_ARCH ? 0 : 0xFE987));
        BIOS_DEFAULT_IRQ07_DEF_LOCATION = PhysToReal416(ROMBIOS_GetMemory(7/*see callback.cpp for EOI_PIC1*/,"BIOS default IRQ2-7 location",/*align*/1,IS_PC98_ARCH ? 0 : 0xFFF55));
        BIOS_DEFAULT_IRQ815_DEF_LOCATION = PhysToReal416(ROMBIOS_GetMemory(9/*see callback.cpp for EOI_PIC1*/,"BIOS default IRQ8-15 location",/*align*/1,IS_PC98_ARCH ? 0 : 0xFE880));

        write_FFFF_signature();
        if (IS_PC98_ARCH)
            write_FFFF_PC98_signature();

        /* Setup all the interrupt handlers the bios controls */

        /* INT 8 Clock IRQ Handler */
        call_irq0=CALLBACK_Allocate();
        if (IS_PC98_ARCH)
            CALLBACK_Setup(call_irq0,INT8_PC98_Handler,CB_IRET,Real2Phys(BIOS_DEFAULT_IRQ0_LOCATION),"IRQ 0 Clock");
        else
            CALLBACK_Setup(call_irq0,INT8_Handler,CB_IRQ0,Real2Phys(BIOS_DEFAULT_IRQ0_LOCATION),"IRQ 0 Clock");

        /* INT 11 Get equipment list */
        callback[1].Install(&INT11_Handler,CB_IRET,"Int 11 Equipment");

        /* INT 12 Memory Size default at 640 kb */
        callback[2].Install(&INT12_Handler,CB_IRET,"Int 12 Memory");

        mlimit = 640;
        t_conv = MEM_TotalPages() << 2; /* convert 4096/byte pages -> 1024/byte KB units */
        /* NTS: Tandy machines, because top of memory shares video memory, need more than 640KB of memory to present 640KB of memory
         *      to DOS. In that case, apparently, that gives 640KB to DOS and 128KB to video memory. 640KB of memory in a Tandy system
         *      means 624KB for DOS and 16KB for Tandy video memory... except that 16-color higher Tandy modes need 32KB of video
         *      memory, so the top of memory has to be adjusted or handled carefully to avoid corruption of the MCB chain. In the 768KB
         *      case video memory is high enough not to conflict with DOS conventional memory at all.
         *
         *      Might and Magic III Isles of Terra will crash in Tandy graphics modes unless we emulate the 768KB Tandy case because the
         *      game doesn't appear to correctly handle the conflict between the DOS MCB chain and video memory (causing an MCB corruption
         *      error) and it appears to make some effort to allocate memory blocks from top of memory which makes the problem worse.
         *
         *      I am fairly certain that there is nothing on Tandy systems to occupy A0000-AFFFFh. Unless of course you install EGA/VGA
         *      hardware in such a system. */
        if (allow_more_than_640kb) {
            if (machine == MCH_CGA)
                mlimit = 736;       /* 640KB + 96KB = 0x00000-0xB7FFF */
            else if (machine == MCH_HERC || machine == MCH_MDA)
                mlimit = 704;       /* 640KB + 64KB = 0x00000-0xAFFFF */
	    else if (machine == MCH_TANDY)
                mlimit = 768;       /* 640KB + 128KB = 0x00000-0xBFFFF */

            /* NTS: Yes, this means Tandy video memory at B8000 overlaps conventional memory, but the
             *      top of conventional memory is stolen as video memory anyway. Tandy documentation
             *      suggests that memory is only installed in multiples of 128KB so there doesn't seem
             *      to be a way to install only 704KB for example. */

            if (t_conv > mlimit) t_conv = mlimit;
            if (t_conv > 640 && machine != MCH_TANDY) {
                /* because the memory emulation has already set things up
                 * HOWEVER Tandy emulation has already properly mapped A0000-BFFFF so don't mess with it */
                bool MEM_map_RAM_physmem(Bitu start,Bitu end);
                MEM_map_RAM_physmem(0xA0000,(t_conv<<10)-1);
                memset(GetMemBase()+(640<<10),0,(t_conv-640)<<10);
            }
        }
        else {
            if (t_conv > 640) t_conv = 640;
        }

        /* allow user to further limit the available memory below 1MB */
        if (dos_conventional_limit != 0 && t_conv > dos_conventional_limit)
            t_conv = dos_conventional_limit;

        // TODO: Allow dosbox-x.conf to specify an option to add an EBDA (Extended BIOS Data Area)
        //       at the top of the DOS conventional limit, which we then reduce further to hold
        //       it. Most BIOSes past 1992 or so allocate an EBDA.

        /* if requested to emulate an ISA memory hole at 512KB, further limit the memory */
        if (isa_memory_hole_512kb && t_conv > 512) t_conv = 512;
        t_conv_real = t_conv;

        if (machine == MCH_TANDY) {
            /* Tandy models are said to have started with 256KB. We'll allow down to 64KB */
            if (t_conv < 64)
                t_conv = 64;
            if (t_conv < 256)
                LOG(LOG_MISC,LOG_WARN)("Warning: Tandy with less than 256KB is unusual");

            /* The shared video/system memory design, and the placement of video RAM at top
             * of conventional memory, means that if conventional memory is less than 640KB
             * and not a multiple of 32KB, things can break. */
            if ((t_conv % 32) != 0)
                LOG(LOG_MISC,LOG_WARN)("Warning: Conventional memory size %uKB in Tandy mode is not a multiple of 32KB, games may not display graphics correctly",(unsigned int)t_conv);
        }
        else if (machine == MCH_PCJR) {
            if (t_conv < 64)
                t_conv = 64;

            /* PCjr also shares video/system memory, but the video memory can only exist
             * below 128KB because IBM intended it to only carry 64KB or 128KB on the
             * motherboard. Any memory past 128KB is likely provided by addons (sidecars) */
            if (t_conv < 128 && (t_conv % 32) != 0)
                LOG(LOG_MISC,LOG_WARN)("Warning: Conventional memory size %uKB in PCjr mode is not a multiple of 32KB, games may not display graphics correctly",(unsigned int)t_conv);
        }

        /* and then unmap RAM between t_conv and mlimit */
        if (t_conv < mlimit) {
            Bitu start = (t_conv+3)/4;  /* start = 1KB to page round up */
            Bitu end = mlimit/4;        /* end = 1KB to page round down */
            if (start < end) MEM_ResetPageHandler_Unmapped(start,end-start);
        }

        if (isa_memory_hole_15mb) MEM_ResetPageHandler_Unmapped(0xf00,0x100); /* 0xF00000-0xFFFFFF */

        if (machine == MCH_TANDY) {
            /* Take 16KB off the top for video RAM.
             * This value never changes after boot, even if you then use the 16-color modes which then moves
             * the video RAM region down 16KB to make a 32KB region. Neither MS-DOS nor INT 10h change this
             * top of memory value. I hope your DOS game doesn't put any important structures or MCBs above
             * the 32KB below top of memory, because it WILL get overwritten with graphics!
             *
             * This is apparently correct behavior, and DOSBox SVN and other forks follow it too.
             *
             * See also: [https://www.vogons.org/viewtopic.php?p=948879#p948879]
             * Issue: [https://github.com/joncampbell123/dosbox-x/issues/2380]
             *
             * Mickeys Space Adventure assumes it can find video RAM by calling INT 12h, subtracting 16KB, and
             * converting KB to paragraphs. Note that it calls INT 12h while in CGA mode, and subtracts 16KB
             * knowing video memory will extend downward 16KB into a 32KB region when it switches into the
             * Tandy/PCjr 16-color mode. */
            /* Tandy systems can present full 640KB of conventional memory with 128KB for video memory if 768KB
             * is installed! */
            if (t_conv > (640+32)) {
                if (t_conv > 640) t_conv = 640;
                if (mlimit > 640) mlimit = 640;

                /* Video memory takes the rest */
                tandy_128kbase = 0xA0000;
            }
            else {
                if (t_conv > 640) t_conv = 640;
                if (mlimit > 640) mlimit = 640;
                t_conv -= 16;
                mlimit -= 16;

                /* if 32KB would cross a 128KB boundary, then adjust again or else
                 * things will horribly break between text and graphics modes */
                if ((t_conv % 128) < 16)
                    t_conv -= 16;

                /* Our choice also affects which 128KB bank within which the 16KB banks
                 * select what system memory becomes video memory.
                 *
                 * FIXME: Is this controlled by the "extended ram page register?" How? */
                tandy_128kbase = ((t_conv - 16u) << 10u) & 0xE0000; /* byte offset = (KB - 16) * 64, round down to multiple of 128KB */
            }
            LOG(LOG_MISC,LOG_DEBUG)("BIOS: setting tandy 128KB base region to %lxh",(unsigned long)tandy_128kbase);
        }
        else if (machine == MCH_PCJR) {
            /* PCjr reserves the top of its internal 128KB of RAM for video RAM.
             * Sidecars can extend it past 128KB but it requires DOS drivers or TSRs
             * to modify the MCB chain so that it a) marks the video memory as reserved
             * and b) creates a new free region above the video RAM region.
             *
             * Therefore, only subtract 16KB if 128KB or less is configured for this machine.
             *
             * Note this is not speculation, it's there in the PCjr BIOS source code:
             * [http://hackipedia.org/browse.cgi/Computer/Platform/PC%2c%20IBM%20compatible/Video/PCjr/IBM%20Personal%20Computer%20PCjr%20Hardware%20Reference%20Library%20Technical%20Reference%20%281983%2d11%29%20First%20Edition%20Revised%2epdf] ROM BIOS source code page A-16 */
            if (t_conv <= (128+16)) {
                if (t_conv > 128) t_conv = 128;
                t_conv -= 16;
            }
            if (mlimit <= (128+16)) {
                if (mlimit > 128) mlimit = 128;
                mlimit -= 16;
            }
        }

        /* INT 4B. Now we can safely signal error instead of printing "Invalid interrupt 4B"
         * whenever we install Windows 95. Note that Windows 95 would call INT 4Bh because
         * that is where the Virtual DMA API lies in relation to EMM386.EXE */
        int4b_callback.Install(&INT4B_Handler,CB_IRET,"INT 4B");

        /* INT 14 Serial Ports */
        callback[3].Install(&INT14_Handler,CB_IRET_STI,"Int 14 COM-port");

        /* INT 15 Misc Calls */
        callback[4].Install(&INT15_Handler,CB_IRET,"Int 15 Bios");

        /* INT 17 Printer Routines */
        callback[5].Install(&INT17_Handler,CB_IRET_STI,"Int 17 Printer");

        /* INT 1A TIME and some other functions */
        callback[6].Install(&INT1A_Handler,CB_IRET_STI,"Int 1a Time");

        /* INT 1C System Timer tick called from INT 8 */
        callback[7].Install(&INT1C_Handler,CB_IRET,"Int 1c Timer");
        
        /* IRQ 8 RTC Handler */
        callback[8].Install(&INT70_Handler,CB_IRET,"Int 70 RTC");

        /* Irq 9 rerouted to irq 2 */
        callback[9].Install(NULL,CB_IRQ9,"irq 9 bios");

        // INT 19h: Boot function
        callback[10].Install(&INT19_Handler,CB_IRET,"int 19");

        // INT 1Bh: IBM PC CTRL+Break
        callback[19].Install(&INT1B_Break_Handler,CB_IRET,"BIOS 1Bh stock CTRL+BREAK handler");

        // INT 76h: IDE IRQ 14
        // This is just a dummy IRQ handler to prevent crashes when
        // IDE emulation fires the IRQ and OS's like Win95 expect
        // the BIOS to handle the interrupt.
        // FIXME: Shouldn't the IRQ send an ACK to the PIC as well?!?
        callback[11].Install(&IRQ14_Dummy,CB_IRET_EOI_PIC2,"irq 14 ide");

        // INT 77h: IDE IRQ 15
        // This is just a dummy IRQ handler to prevent crashes when
        // IDE emulation fires the IRQ and OS's like Win95 expect
        // the BIOS to handle the interrupt.
        // FIXME: Shouldn't the IRQ send an ACK to the PIC as well?!?
        callback[12].Install(&IRQ15_Dummy,CB_IRET_EOI_PIC2,"irq 15 ide");

        // INT 0Eh: IDE IRQ 6
        callback[13].Install(&IRQ15_Dummy,CB_IRET_EOI_PIC1,"irq 6 floppy");

        // INT 18h: Enter BASIC
        // Non-IBM BIOS would display "NO ROM BASIC" here
        callback[15].Install(&INT18_Handler,CB_IRET,"int 18");

        if(IS_J3100) {
            callback[16].Install(&INT60_Handler,CB_IRET,"Int 60 Bios");
            callback[16].Set_RealVec(0x60);

            callback[17].Install(&INT6F_Handler,CB_INT6F_ATOK,"Int 6F Bios");
            callback[17].Set_RealVec(0x6f);
        }

        init_vm86_fake_io();

        /* Irq 2-7 */
        call_irq07default=CALLBACK_Allocate();

        /* Irq 8-15 */
        call_irq815default=CALLBACK_Allocate();

        /* BIOS boot stages */
        cb_bios_post.Install(&cb_bios_post__func,CB_RETF,"BIOS POST");
        cb_bios_scan_video_bios.Install(&cb_bios_scan_video_bios__func,CB_RETF,"BIOS Scan Video BIOS");
        cb_bios_adapter_rom_scan.Install(&cb_bios_adapter_rom_scan__func,CB_RETF,"BIOS Adapter ROM scan");
        cb_bios_startup_screen.Install(&cb_bios_startup_screen__func,CB_RETF,"BIOS Startup screen");
        cb_bios_boot.Install(&cb_bios_boot__func,CB_RETF,"BIOS BOOT");
        cb_bios_bootfail.Install(&cb_bios_bootfail__func,CB_RETF,"BIOS BOOT FAIL");

        if (IS_PC98_ARCH) {
		cb_pc98_rombasic.Install(&cb_pc98_entry__func,CB_RETF,"N88 ROM BASIC");
	}
	else {
		/* IBM ROM BASIC resides at segment F600:0000 just below the 5150 ROM BIOS.
		 * MS-DOS 1.x and 2.x BASIC(A).COM jump to specific addresses in the ROM BASIC to do their thing.
		 * The purpose of these callbacks is to catch those programs and safely halt emulation to
		 * state that ROM BASIC is not present */
		cb_ibm_basic.Install(&cb_ibm_basic_entry__func,CB_RETF,"IBM ROM BASIC entry");
	}

        // Compatible POST routine location: jump to the callback
        {
            Bitu wo_fence;

	    {
		const Bitu wo = Real2Phys(BIOS_DEFAULT_RESET_LOCATION);
		if (IS_PC98_ARCH) {
			const unsigned int ofs = Real2Phys(BIOS_DEFAULT_RESET_CODE_LOCATION) - 0xF0000u;
			phys_writeb(wo+0,0xEA);/*JMP far*/
			phys_writew(wo+1,ofs);
			phys_writew(wo+3,0xF000);
		}
		else {
			const unsigned int delta = (Real2Phys(BIOS_DEFAULT_RESET_CODE_LOCATION) - (Real2Phys(BIOS_DEFAULT_RESET_LOCATION) + 3u)) & 0xFFFFu;
			const Bitu wo = Real2Phys(BIOS_DEFAULT_RESET_LOCATION);
			phys_writeb(wo+0,0xE9);/*JMP near*/
			phys_writew(wo+1,delta);
		}
	    }

            Bitu wo = Real2Phys(BIOS_DEFAULT_RESET_CODE_LOCATION);
            wo_fence = wo + 64;

            // POST
            phys_writeb(wo+0x00,(uint8_t)0xFE);                       //GRP 4
            phys_writeb(wo+0x01,(uint8_t)0x38);                       //Extra Callback instruction
            phys_writew(wo+0x02,(uint16_t)cb_bios_post.Get_callback());           //The immediate word
            wo += 4;

            // video bios scan
            phys_writeb(wo+0x00,(uint8_t)0xFE);                       //GRP 4
            phys_writeb(wo+0x01,(uint8_t)0x38);                       //Extra Callback instruction
            phys_writew(wo+0x02,(uint16_t)cb_bios_scan_video_bios.Get_callback());        //The immediate word
            wo += 4;

            // adapter ROM scan
            phys_writeb(wo+0x00,(uint8_t)0xFE);                       //GRP 4
            phys_writeb(wo+0x01,(uint8_t)0x38);                       //Extra Callback instruction
            phys_writew(wo+0x02,(uint16_t)cb_bios_adapter_rom_scan.Get_callback());       //The immediate word
            wo += 4;

            // startup screen
            phys_writeb(wo+0x00,(uint8_t)0xFE);                       //GRP 4
            phys_writeb(wo+0x01,(uint8_t)0x38);                       //Extra Callback instruction
            phys_writew(wo+0x02,(uint16_t)cb_bios_startup_screen.Get_callback());     //The immediate word
            wo += 4;

            // user boot hook
            if (bios_user_boot_hook != 0) {
                phys_writeb(wo+0x00,0x9C);                          //PUSHF
                phys_writeb(wo+0x01,0x9A);                          //CALL FAR
                phys_writew(wo+0x02,0x0000);                        //seg:0
                phys_writew(wo+0x04,bios_user_boot_hook>>4);
                wo += 6;
            }

            // boot
            BIOS_boot_code_offset = wo;
            phys_writeb(wo+0x00,(uint8_t)0xFE);                       //GRP 4
            phys_writeb(wo+0x01,(uint8_t)0x38);                       //Extra Callback instruction
            phys_writew(wo+0x02,(uint16_t)cb_bios_boot.Get_callback());           //The immediate word
            wo += 4;

            // boot fail
            BIOS_bootfail_code_offset = wo;
            phys_writeb(wo+0x00,(uint8_t)0xFE);                       //GRP 4
            phys_writeb(wo+0x01,(uint8_t)0x38);                       //Extra Callback instruction
            phys_writew(wo+0x02,(uint16_t)cb_bios_bootfail.Get_callback());           //The immediate word
            wo += 4;

            /* fence */
            phys_writeb(wo++,0xEB);                             // JMP $-2
            phys_writeb(wo++,0xFE);

            if (wo > wo_fence) E_Exit("BIOS boot callback overrun");

            if (IS_PC98_ARCH) {
                /* Boot disks that run N88 basic, stopgap */
                PhysPt bo = 0xE8002; // E800:0002

		ROMBIOS_GetMemory(6,"N88 ROM BASIC entry point",/*align*/1,bo);

                phys_writeb(bo+0x00,(uint8_t)0xFE);                       //GRP 4
                phys_writeb(bo+0x01,(uint8_t)0x38);                       //Extra Callback instruction
                phys_writew(bo+0x02,(uint16_t)cb_pc98_rombasic.Get_callback());           //The immediate word

                phys_writeb(bo+0x04,0xEB);                             // JMP $-2
                phys_writeb(bo+0x05,0xFE);

                // On careful examination of BIOS.ROM, there's a JMP instruction at E800:0000 as well.
                // I don't have any test cases that jump there, but add a JMP forward just in case.
                bo = 0xE8000;
                phys_writeb(bo+0x00,(uint8_t)0xEB);                       // JMP $+2 (to next instruction)
                phys_writeb(bo+0x01,(uint8_t)0x00);

                /* "Nut Berry" expects a 8-byte lookup table for [AL&7] -> 1 << (AL&7) at 0xFD80:0x0E3C so it's
                 * custom keyboard interrupt handler can update the keyboard status bitmap in the BIOS data area.
                 * I don't know if the game even uses it. On a BIOS.ROM image I have, and on real hardware, there
                 * is clearly that table but at slightly different addresses (One PC-9821 laptop has it at
                 * 0xFD80:0x0E45) which means whether the game uses it or not the bitmap may have random bits set
                 * when you exit to DOS.
                 *
                 * Assuming no other game does this, this fixed address should be fine.
                 *
                 * NOTE: After disassembling the IRQ1 handler on a real PC-9821 laptop, I noticed this game's
                 *       custom ISR bears a strong resemblance to it. In fact, you might say it's an exact instruction
                 *       for instruction copy of the ISR, except that the table addresses in ROM are slightly different.
                 *       Ha. Theoretically then, that means we could also get this game to work fully properly by patching
                 *       it not to hook the keyboard interrupt at all! */
                for (unsigned int i=0;i < 8;i++) phys_writeb(0xFD800+0xE3C+i,1u << i);

                /* "Nut Berry" also assumes shift state table offsets (for all 16 possible combinations) exist
                 * at 0xFD80:0x0E28. Once again, this means it will not work properly on anything other than the dev's
                 * machine because on a real PC-9821 laptop used for testing, the table offset is slightly different
                 * (0xE31 instead of 0xE28). The table mentioned here is used to update the 0x522 offset WORD in the
                 * BIOS data area to reflect the translation table in effect based on the shift key status, so if you
                 * misread the table you end up pointing it at junk and then keyboard input doesn't work anymore. */
                // NTS: On a real PC-9821 laptop, the table is apparently 10 entries long. If BDA byte 0x53A is less than
                //      8 then it's just a simple lookup. If BDA byte 0x53A has bit 4 set, then use the 8th entry, and
                //      if bit 4 and 3 are set, use the 9th entry.
                for (unsigned int i=0;i < 10;i++) phys_writew(0xFD800+0xE28+(i*2),(unsigned int)(Real2Phys(BIOS_PC98_KEYBOARD_TRANSLATION_LOCATION) - 0xFD800) + (i * 0x60));
            }
	    else {
		    if (ibm_rom_basic_size == 0) {
			    /* IBM MS-DOS 1.x/2.x BASIC and BASICA, stopgap */
			    PhysPt bo;

			    bo = 0xF6000+0x2DB0; // F600:2DB0

			    ROMBIOS_GetMemory(6,"IBM ROM BASIC entry point",/*align*/1,bo);

			    phys_writeb(bo+0x00,(uint8_t)0xFE);                       //GRP 4
			    phys_writeb(bo+0x01,(uint8_t)0x38);                       //Extra Callback instruction
			    phys_writew(bo+0x02,(uint16_t)cb_ibm_basic.Get_callback());           //The immediate word

			    phys_writeb(bo+0x04,0xEB);                             // JMP $-2
			    phys_writeb(bo+0x05,0xFE);

			    bo = 0xF6000+0x4C79; // F600:4C79

			    ROMBIOS_GetMemory(6,"IBM ROM BASIC entry point",/*align*/1,bo);

			    phys_writeb(bo+0x00,(uint8_t)0xFE);                       //GRP 4
			    phys_writeb(bo+0x01,(uint8_t)0x38);                       //Extra Callback instruction
			    phys_writew(bo+0x02,(uint16_t)cb_ibm_basic.Get_callback());           //The immediate word

			    phys_writeb(bo+0x04,0xEB);                             // JMP $-2
			    phys_writeb(bo+0x05,0xFE);
		    }
	    }

            if (IS_PC98_ARCH && enable_pc98_copyright_string) {
                size_t i=0;

		ROMBIOS_GetMemory(pc98_copyright_str.length()+1,"NEC PC-98 copyright string",/*align*/1,0xE8000 + 0x0DD8);

                for (;i < pc98_copyright_str.length();i++)
                    phys_writeb(0xE8000 + 0x0DD8 + (PhysPt)i,(uint8_t)pc98_copyright_str[i]);

                phys_writeb(0xE8000 + 0x0DD8 + (PhysPt)i,0);

		ROMBIOS_GetMemory(sizeof(pc98_epson_check_2),"NEC PC-98 Epson check data #2",/*align*/1,0xF5200 + 0x018E);

                for (i=0;i < sizeof(pc98_epson_check_2);i++)
                    phys_writeb(0xF5200 + 0x018E + (PhysPt)i,(uint8_t)pc98_epson_check_2[i]);
            }
        }
    }
    ~BIOS(){
        /* snap the CPU back to real mode. this code thinks in terms of 16-bit real mode
         * and if allowed to do its thing in a 32-bit guest OS like WinNT, will trigger
         * a page fault. */
        CPU_Snap_Back_To_Real_Mode();

        if (acpi_iocallout != IO_Callout_t_none) {
            IO_FreeCallout(acpi_iocallout);
            acpi_iocallout = IO_Callout_t_none;
        }

        if (BOCHS_PORT_E9) {
            delete BOCHS_PORT_E9;
            BOCHS_PORT_E9=NULL;
        }
        if (ISAPNP_PNP_ADDRESS_PORT) {
            delete ISAPNP_PNP_ADDRESS_PORT;
            ISAPNP_PNP_ADDRESS_PORT=NULL;
        }
        if (ISAPNP_PNP_DATA_PORT) {
            delete ISAPNP_PNP_DATA_PORT;
            ISAPNP_PNP_DATA_PORT=NULL;
        }
        if (ISAPNP_PNP_READ_PORT) {
            delete ISAPNP_PNP_READ_PORT;
            ISAPNP_PNP_READ_PORT=NULL;
        }
        if (isapnpigdevice) {
            /* ISA PnP will auto-free it */
            isapnpigdevice=NULL;
        }

        if (dosbox_int_iocallout != IO_Callout_t_none) {
            IO_FreeCallout(dosbox_int_iocallout);
            dosbox_int_iocallout = IO_Callout_t_none;
        }

        /* abort DAC playing */
        if (tandy_sb.port) {
            IO_Write(tandy_sb.port+0xcu,0xd3u);
            IO_Write(tandy_sb.port+0xcu,0xd0u);
        }
        real_writeb(0x40,0xd4,0x00);
        if (tandy_DAC_callback[0]) {
            uint32_t orig_vector=real_readd(0x40,0xd6);
            if (orig_vector==tandy_DAC_callback[0]->Get_RealPointer()) {
                /* set IRQ vector to old value */
                uint8_t tandy_irq = 7;
                if (tandy_sb.port) tandy_irq = tandy_sb.irq;
                else if (tandy_dac.port) tandy_irq = tandy_dac.irq;
                uint8_t tandy_irq_vector = tandy_irq;
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

        /* encourage the callback objects to uninstall HERE while we're in real mode, NOT during the
         * destructor stage where we're back in protected mode */
        for (unsigned int i=0;i < callback_count;i++) callback[i].Uninstall();

        /* assume these were allocated */
        CALLBACK_DeAllocate(call_irq0);
        CALLBACK_DeAllocate(call_irq07default);
        CALLBACK_DeAllocate(call_irq815default);

        /* done */
        CPU_Snap_Back_Restore();
    }
};

void BIOS_Enter_Boot_Phase(void) {
    /* direct CS:IP right to the instruction that leads to the boot process */
    /* note that since it's a callback instruction it doesn't really matter
     * what CS:IP is as long as it points to the instruction */
    reg_eip = BIOS_boot_code_offset & 0xFUL;
    CPU_SetSegGeneral(cs, BIOS_boot_code_offset >> 4UL);
}

void BIOS_SetCOMPort(Bitu port, uint16_t baseaddr) {
    switch(port) {
    case 0:
        mem_writew(BIOS_BASE_ADDRESS_COM1,baseaddr);
        mem_writeb(BIOS_COM1_TIMEOUT, 10); // FIXME: Right??
        break;
    case 1:
        mem_writew(BIOS_BASE_ADDRESS_COM2,baseaddr);
        mem_writeb(BIOS_COM2_TIMEOUT, 10); // FIXME: Right??
        break;
    case 2:
        mem_writew(BIOS_BASE_ADDRESS_COM3,baseaddr);
        mem_writeb(BIOS_COM3_TIMEOUT, 10); // FIXME: Right??
        break;
    case 3:
        mem_writew(BIOS_BASE_ADDRESS_COM4,baseaddr);
        mem_writeb(BIOS_COM4_TIMEOUT, 10); // FIXME: Right??
        break;
    }
}

void BIOS_SetLPTPort(Bitu port, uint16_t baseaddr) {
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
}

void BIOS_PnP_ComPortRegister(Bitu port,Bitu irq) {
    /* add to PnP BIOS */
    if (ISAPNPBIOS) {
        unsigned char tmp[256];
        unsigned int i;

        /* register serial ports */
        const unsigned char h1[9] = {
            ISAPNP_SYSDEV_HEADER(
                ISAPNP_ID('P','N','P',0x0,0x5,0x0,0x1), /* PNP0501 16550A-compatible COM port */
                ISAPNP_TYPE(0x07,0x00,0x02),        /* type: RS-232 communications device, 16550-compatible */
                0x0001 | 0x0002)
        };

        i = 0;
        memcpy(tmp+i,h1,9); i += 9;         /* can't disable, can't configure */
        /*----------allocated--------*/
        tmp[i+0] = (8 << 3) | 7;            /* IO resource */
        tmp[i+1] = 0x01;                /* 16-bit decode */
        host_writew(tmp+i+2,port);          /* min */
        host_writew(tmp+i+4,port);          /* max */
        tmp[i+6] = 0x10;                /* align */
        tmp[i+7] = 0x08;                /* length */
        i += 7+1;

        if (irq > 0) {
            tmp[i+0] = (4 << 3) | 3;            /* IRQ resource */
            host_writew(tmp+i+1,1 << irq);
            tmp[i+3] = 0x09;                /* HTL=1 LTL=1 */
            i += 3+1;
        }

        tmp[i+0] = 0x79;                /* END TAG */
        tmp[i+1] = 0x00;
        i += 2;
        /*-------------possible-----------*/
        tmp[i+0] = 0x79;                /* END TAG */
        tmp[i+1] = 0x00;
        i += 2;
        /*-------------compatible---------*/
        tmp[i+0] = 0x79;                /* END TAG */
        tmp[i+1] = 0x00;
        i += 2;

        if (!ISAPNP_RegisterSysDev(tmp,i)) {
            //LOG_MSG("ISAPNP register failed\n");
        }
    }
}

static BIOS* test = NULL;

void BIOS_Destroy(Section* /*sec*/){
    ROMBIOS_DumpMemory();
    ISA_PNP_FreeAllDevs();
    if (test != NULL) {
        delete test;
        test = NULL;
    }
}

void BIOS_OnPowerOn(Section* sec) {
    (void)sec;//UNUSED
    if (test) delete test;
    test = new BIOS(control->GetSection("joystick"));
}

void swapInNextDisk(bool pressed);
void swapInNextCD(bool pressed);

void INT10_OnResetComplete();
void CALLBACK_DeAllocate(Bitu in);

void MOUSE_Unsetup_DOS(void);
void MOUSE_Unsetup_BIOS(void);

void BIOS_OnResetComplete(Section *x) {
    (void)x;//UNUSED
    INT10_OnResetComplete();

    if (IS_PC98_ARCH) {
        void PC98_BIOS_Bank_Switch_Reset(void);
        PC98_BIOS_Bank_Switch_Reset();
    }

    if (biosConfigSeg != 0u) {
        ROMBIOS_FreeMemory((Bitu)(biosConfigSeg << 4u)); /* remember it was alloc'd paragraph aligned, then saved >> 4 */
        biosConfigSeg = 0u;
    }

    call_pnp_rp = 0;
    if (call_pnp_r != ~0UL) {
        CALLBACK_DeAllocate(call_pnp_r);
        call_pnp_r = ~0UL;
    }

    call_pnp_pp = 0;
    if (call_pnp_p != ~0UL) {
        CALLBACK_DeAllocate(call_pnp_p);
        call_pnp_p = ~0UL;
    }

    MOUSE_Unsetup_DOS();
    MOUSE_Unsetup_BIOS();

    ISA_PNP_FreeAllSysNodeDevs();
}

void BIOS_Init() {
    DOSBoxMenu::item *item;

    LOG(LOG_MISC,LOG_DEBUG)("Initializing BIOS");

    /* make sure the array is zeroed */
    ISAPNP_SysDevNodeCount = 0;
    ISAPNP_SysDevNodeLargest = 0;
    for (int i=0;i < MAX_ISA_PNP_SYSDEVNODES;i++) ISAPNP_SysDevNodes[i] = NULL;

    /* make sure CD swap and floppy swap mapper events are available */
    MAPPER_AddHandler(swapInNextDisk,MK_o,MMODHOST,"swapimg","Swap floppy drive",&item); /* Originally "Swap Image" but this version does not swap CDs */
    item->set_text("Swap floppy drive");

    MAPPER_AddHandler(swapInNextCD,MK_d,MMODHOST,"swapcd","Swap CD drive",&item); /* Variant of "Swap Image" for CDs */
    item->set_text("Swap CD drive");

    /* NTS: VM_EVENT_BIOS_INIT this callback must be first. */
    AddExitFunction(AddExitFunctionFuncPair(BIOS_Destroy),false);
    AddVMEventFunction(VM_EVENT_POWERON,AddVMEventFunctionFuncPair(BIOS_OnPowerOn));
    AddVMEventFunction(VM_EVENT_RESET_END,AddVMEventFunctionFuncPair(BIOS_OnResetComplete));
}

void write_ID_version_string() {
    Bitu str_id_at,str_ver_at;
    size_t str_id_len,str_ver_len;

    /* NTS: We can't move the version and ID strings, it causes programs like MSD.EXE to lose
     *      track of the "IBM compatible blahblahblah" string. Which means that apparently
     *      programs looking for this information have the address hardcoded ALTHOUGH
     *      experiments show you can move the version string around so long as it's
     *      +1 from a paragraph boundary */
    /* TODO: *DO* allow dynamic relocation however if the dosbox-x.conf indicates that the user
     *       is not interested in IBM BIOS compatibility. Also, it would be really cool if
     *       dosbox-x.conf could override these strings and the user could enter custom BIOS
     *       version and ID strings. Heh heh heh.. :) */
    str_id_at = 0xFE00E;
    str_ver_at = 0xFE061;
    str_id_len = strlen(bios_type_string)+1;
    str_ver_len = strlen(bios_version_string)+1;
    if (!IS_PC98_ARCH) {
        /* need to mark these strings off-limits so dynamic allocation does not overwrite them */
        ROMBIOS_GetMemory((Bitu)str_id_len+1,"BIOS ID string",1,str_id_at);
        ROMBIOS_GetMemory((Bitu)str_ver_len+1,"BIOS version string",1,str_ver_at);
    }
    if (str_id_at != 0) {
        for (size_t i=0;i < str_id_len;i++) phys_writeb(str_id_at+(PhysPt)i,(uint8_t)bios_type_string[i]);
    }
    if (str_ver_at != 0) {
        for (size_t i=0;i < str_ver_len;i++) phys_writeb(str_ver_at+(PhysPt)i,(uint8_t)bios_version_string[i]);
    }
}

void GEN_PowerButton(bool pressed) {
    if (!pressed)
        return;

    if (PowerManagementEnabledButton()) {
        PowerButtonClicks++;
    }
    else {
        LOG(LOG_MISC,LOG_WARN)("Power button: Guest OS is not using power management and is probably ignoring the power button");
    }
}


extern uint16_t j3_font_offset;

/* NTS: Do not use callbacks! This function is called before CALLBACK_Init() */
void ROMBIOS_Init() {
    Section_prop * section=static_cast<Section_prop *>(control->GetSection("dosbox"));
    Bitu oi;

    /* This is GENERIC. Right now it only ties into the APM BIOS emulation.
     * In the future, it will also tie into the ACPI emulation. We'll have
     * menu items to trigger specific APM/ACPI events of course, but for
     * the mapper, we'll try not to confuse the user with APM vs ACPI for
     * the same reason PC manufacturers don't have two power buttons for
     * each standard on the front.
     *
     * Note for PC-98 mode: I'm aware that there are mid to late 1990s
     * PC-98 laptops that also have a power button to send some kind of
     * power off event. If how that works becomes known, it can be tied
     * to this mapper shortcut as well. It's obviously not APM since
     * the APM standard is tied to the IBM compatible PC world. */

    // log
    LOG(LOG_MISC,LOG_DEBUG)("Initializing ROM BIOS");

    ibm_rom_basic.clear();
    ibm_rom_basic_size = 0;

    oi = (Bitu)section->Get_int("rom bios minimum size"); /* in KB */
    oi = (oi + 3u) & ~3u; /* round to 4KB page */
    if (oi > 128u) oi = 128u;
    if (oi == 0u) {
        if (IS_PC98_ARCH)
            oi = 96u; // BIOS standard range is E8000-FFFFF
        else
            oi = 64u;
    }
    if (oi < 8) oi = 8; /* because of some of DOSBox's fixed ROM structures we can only go down to 8KB */
    rombios_minimum_size = (oi << 10); /* convert to minimum, using size coming downward from 1MB */

    oi = (Bitu)section->Get_int("rom bios allocation max"); /* in KB */
    oi = (oi + 3u) & ~3u; /* round to 4KB page */
    if (oi > 128u) oi = 128u;
    if (oi == 0u) {
        if (IS_PC98_ARCH)
            oi = 96u;
        else
            oi = 64u;
    }
    if (oi < 8u) oi = 8u; /* because of some of DOSBox's fixed ROM structures we can only go down to 8KB */
    oi <<= 10u;
    if (oi < rombios_minimum_size) oi = rombios_minimum_size;
    rombios_minimum_location = 0x100000ul - oi; /* convert to minimum, using size coming downward from 1MB */

    LOG(LOG_BIOS,LOG_DEBUG)("ROM BIOS range: 0x%05X-0xFFFFF",(int)rombios_minimum_location);
    LOG(LOG_BIOS,LOG_DEBUG)("ROM BIOS range according to minimum size: 0x%05X-0xFFFFF",(int)(0x100000 - rombios_minimum_size));

    if (IS_PC98_ARCH && rombios_minimum_location > 0xE8000)
        LOG(LOG_BIOS,LOG_DEBUG)("Caution: Minimum ROM base higher than E8000 will prevent use of actual PC-98 BIOS image or N88 BASIC");

    if (!MEM_map_ROM_physmem(rombios_minimum_location,0xFFFFF)) E_Exit("Unable to map ROM region as ROM");

    /* and the BIOS alias at the top of memory (TODO: what about 486/Pentium emulation where the BIOS at the 4GB top is different
     * from the BIOS at the legacy 1MB boundary because of shadowing and/or decompressing from ROM at boot? */
    {
        uint64_t top = (uint64_t)1UL << (uint64_t)MEM_get_address_bits4GB();
        if (top >= ((uint64_t)1UL << (uint64_t)21UL)) { /* 2MB or more */
            unsigned long alias_base,alias_end;

            alias_base = (unsigned long)top + (unsigned long)rombios_minimum_location - 0x100000UL;
            alias_end = (unsigned long)top - 1UL;

            LOG(LOG_BIOS,LOG_DEBUG)("ROM BIOS also mapping alias to 0x%08lx-0x%08lx",alias_base,alias_end);
            if (!MEM_map_ROM_alias_physmem(alias_base,alias_end)) {
                void MEM_cut_RAM_up_to(Bitu addr);

                /* it's possible if memory aliasing is set that memsize is too large to make room.
                 * let memory emulation know where the ROM BIOS starts so it can unmap the RAM pages,
                 * reduce the memory reported to the OS, and try again... */
                LOG(LOG_BIOS,LOG_DEBUG)("No room for ROM BIOS alias, reducing reported memory and unmapping RAM pages to make room");
                MEM_cut_RAM_up_to(alias_base);

                if (!MEM_map_ROM_alias_physmem(alias_base,alias_end))
                    E_Exit("Unable to map ROM region as ROM alias");
            }
        }
    }

    /* set up allocation */
    rombios_alloc.name = "ROM BIOS";
    rombios_alloc.topDownAlloc = true;
    rombios_alloc.initSetRange(rombios_minimum_location,0xFFFF0 - 1);

    if (IS_PC98_ARCH) {
	    /* TODO: Is this needed? And where? */
    }
    else {
	    /* prevent dynamic allocation from taking reserved fixed addresses above F000:E000 in IBM PC mode. */
	    rombios_alloc.setMaxDynamicAllocationAddress(0xFE000 - 1);
    }

    if (!IS_PC98_ARCH) {
	    ibm_rom_basic = section->Get_string("ibm rom basic");
	    if (!ibm_rom_basic.empty()) {
            void ResolvePath(std::string& in);
            ResolvePath(ibm_rom_basic);
		    struct stat st;
		    if (stat(ibm_rom_basic.c_str(),&st) == 0 && S_ISREG(st.st_mode) && st.st_size >= (off_t)(32u*1024u) && st.st_size <= (off_t)(64u*1024u) && (st.st_size % 4096) == 0) {
			    ibm_rom_basic_size = (size_t)st.st_size;
			    ibm_rom_basic_base = rombios_alloc._max_nonfixed + 1 - st.st_size;
			    LOG_MSG("Will load IBM ROM BASIC to %05lx-%05lx",(unsigned long)ibm_rom_basic_base,(unsigned long)(ibm_rom_basic_base+ibm_rom_basic_size-1));
			    Bitu base = ROMBIOS_GetMemory(ibm_rom_basic_size,"IBM ROM BASIC",1u/*page align*/,ibm_rom_basic_base);
			    rombios_alloc.setMaxDynamicAllocationAddress(ibm_rom_basic_base - 1);
			    (void)base;

			    FILE *fp = fopen(ibm_rom_basic.c_str(),"rb");
			    if (fp != NULL) {
				    fread(GetMemBase()+ibm_rom_basic_base,ibm_rom_basic_size,1u,fp);
				    fclose(fp);
			    }
		    }
	    }
    }

    write_ID_version_string();

    if (IS_PC98_ARCH && enable_pc98_copyright_string) { // PC-98 BIOSes have a copyright string at E800:0DD8
        if (ROMBIOS_GetMemory(pc98_copyright_str.length()+1,"PC-98 copyright string",1,0xE8000 + 0x0DD8) == 0)
            LOG_MSG("WARNING: Was not able to mark off E800:0DD8 off-limits for PC-98 copyright string");
        if (ROMBIOS_GetMemory(sizeof(pc98_epson_check_2),"PC-98 unknown data / Epson check",1,0xF5200 + 0x018E) == 0)
            LOG_MSG("WARNING: Was not able to mark off E800:0DD8 off-limits for PC-98 copyright string");
    }
 
    /* some structures when enabled are fixed no matter what */
    if (rom_bios_8x8_cga_font && !IS_PC98_ARCH) {
        /* line 139, int10_memory.cpp: the 8x8 font at 0xF000:FA6E, first 128 chars.
         * allocate this NOW before other things get in the way */
        if (ROMBIOS_GetMemory(128*8,"BIOS 8x8 font (first 128 chars)",1,0xFFA6E) == 0) {
            LOG_MSG("WARNING: Was not able to mark off 0xFFA6E off-limits for 8x8 font");
        }
    }

    /* PC-98 BIOS vectors appear to reside at segment 0xFD80. This is so common some games
     * use it (through INT 1Dh) to detect whether they are running on PC-98 or not (issue #927).
     *
     * Note that INT 1Dh is one of the few BIOS interrupts not intercepted by PC-98 MS-DOS */
    if (IS_PC98_ARCH) {
        if (ROMBIOS_GetMemory(128,"PC-98 INT vector stub segment 0xFD80",1,0xFD800) == 0) {
            LOG_MSG("WARNING: Was not able to mark off 0xFD800 off-limits for PC-98 int vector stubs");
        }
    }

    /* PC-98 BIOSes have a LIO interface at segment F990 with graphic subroutines for Microsoft BASIC */
    if (IS_PC98_ARCH) {
        if (ROMBIOS_GetMemory(256,"PC-98 LIO graphic ROM BIOS library",1,0xF9900) == 0) {
            LOG_MSG("WARNING: Was not able to mark off 0xF9900 off-limits for PC-98 LIO graphics library");
        }
    }

    /* install the font */
    if (rom_bios_8x8_cga_font) {
        for (Bitu i=0;i<128*8;i++) {
            phys_writeb(PhysMake(0xf000,0xfa6e)+i,int10_font_08[i]);
        }
    }

    /* we allow dosbox-x.conf to specify a binary blob to load into ROM BIOS and execute after reset.
     * we allow this for both hacker curiosity and automated CPU testing. */
    {
        std::string path = section->Get_string("call binary on reset");
        struct stat st;

        if (!path.empty() && stat(path.c_str(),&st) == 0 && S_ISREG(st.st_mode) && st.st_size <= (off_t)(128u*1024u)) {
            Bitu base = ROMBIOS_GetMemory((Bitu)st.st_size,"User reset vector binary",16u/*page align*/,0u);

            if (base != 0) {
                FILE *fp = fopen(path.c_str(),"rb");

                if (fp != NULL) {
                    /* NTS: Make sure memory base != NULL, and that it fits within 1MB.
                     *      memory code allocates a minimum 1MB of memory even if
                     *      guest memory is less than 1MB because ROM BIOS emulation
                     *      depends on it. */
                    assert(GetMemBase() != NULL);
                    assert((base+(Bitu)st.st_size) <= 0x100000ul);
                    size_t readResult = fread(GetMemBase()+base,(size_t)st.st_size,1u,fp);
                    fclose(fp);
                    if (readResult != 1) {
                        LOG(LOG_IO, LOG_ERROR) ("Reading error in ROMBIOS_Init\n");
                        return;
                    }

                    LOG_MSG("User reset vector binary '%s' loaded at 0x%lx",path.c_str(),(unsigned long)base);
                    bios_user_reset_vector_blob = base;
                }
                else {
                    LOG_MSG("WARNING: Unable to open file to load user reset vector binary '%s' into ROM BIOS memory",path.c_str());
                }
            }
            else {
                LOG_MSG("WARNING: Unable to load user reset vector binary '%s' into ROM BIOS memory",path.c_str());
            }
        }
    }

    /* we allow dosbox-x.conf to specify a binary blob to load into ROM BIOS and execute just before boot.
     * we allow this for both hacker curiosity and automated CPU testing. */
    {
        std::string path = section->Get_string("call binary on boot");
        struct stat st;

        if (!path.empty() && stat(path.c_str(),&st) == 0 && S_ISREG(st.st_mode) && st.st_size <= (off_t)(128u*1024u)) {
            Bitu base = ROMBIOS_GetMemory((Bitu)st.st_size,"User boot hook binary",16u/*page align*/,0u);

            if (base != 0) {
                FILE *fp = fopen(path.c_str(),"rb");

                if (fp != NULL) {
                    /* NTS: Make sure memory base != NULL, and that it fits within 1MB.
                     *      memory code allocates a minimum 1MB of memory even if
                     *      guest memory is less than 1MB because ROM BIOS emulation
                     *      depends on it. */
                    assert(GetMemBase() != NULL);
                    assert((base+(Bitu)st.st_size) <= 0x100000ul);
                    size_t readResult = fread(GetMemBase()+base,(size_t)st.st_size,1u,fp);
                    fclose(fp);
                    if (readResult != 1) {
                        LOG(LOG_IO, LOG_ERROR) ("Reading error in ROMBIOS_Init\n");
                        return;
                    }

                    LOG_MSG("User boot hook binary '%s' loaded at 0x%lx",path.c_str(),(unsigned long)base);
                    bios_user_boot_hook = base;
                }
                else {
                    LOG_MSG("WARNING: Unable to open file to load user boot hook binary '%s' into ROM BIOS memory",path.c_str());
                }
            }
            else {
                LOG_MSG("WARNING: Unable to load user boot hook binary '%s' into ROM BIOS memory",path.c_str());
            }
        }
    }
    // J-3100's DOS reads 8x16 font data directly from F000:CA00.
    if(IS_J3100) {
        ROMBIOS_GetMemory(256*16, "J-3100 8x16 font data", 1, 0xf0000 + j3_font_offset);
    }
}

//! \brief Updates the state of a lockable key.
void UpdateKeyWithLed(int nVirtKey, int flagAct, int flagLed);

bool IsSafeToMemIOOnBehalfOfGuest()
{
    if(cpu.pmode) return false; // protected mode (including virtual 8086 mode): NO
    if(dos_kernel_disabled) return false; // guest OS not running under our own DOS kernel: NO
    return true;
}

void BIOS_SynchronizeNumLock()
{
#if defined(WIN32)
	UpdateKeyWithLed(VK_NUMLOCK, BIOS_KEYBOARD_FLAGS1_NUMLOCK_ACTIVE, BIOS_KEYBOARD_LEDS_NUM_LOCK);
#endif
}

void BIOS_SynchronizeCapsLock()
{
#if defined(WIN32)
	UpdateKeyWithLed(VK_CAPITAL, BIOS_KEYBOARD_FLAGS1_CAPS_LOCK_ACTIVE, BIOS_KEYBOARD_LEDS_CAPS_LOCK);
#endif
}

void BIOS_SynchronizeScrollLock()
{
#if defined(WIN32)
	UpdateKeyWithLed(VK_SCROLL, BIOS_KEYBOARD_FLAGS1_SCROLL_LOCK_ACTIVE, BIOS_KEYBOARD_LEDS_SCROLL_LOCK);
#endif
}

void UpdateKeyWithLed(int nVirtKey, int flagAct, int flagLed)
{
#if defined(WIN32)

	const auto state = GetKeyState(nVirtKey);

	const auto flags1 = BIOS_KEYBOARD_FLAGS1;
	const auto flags2 = BIOS_KEYBOARD_LEDS;

	auto flag1 = mem_readb(flags1);
	auto flag2 = mem_readb(flags2);

	if (state & 1)
	{
		flag1 |= flagAct;
		flag2 |= flagLed;
	}
	else
	{
		flag1 &= ~flagAct;
		flag2 &= ~flagLed;
	}

	mem_writeb(flags1, flag1);
	mem_writeb(flags2, flag2);

#else

    (void)nVirtKey;
    (void)flagAct;
    (void)flagLed;

#endif
}

