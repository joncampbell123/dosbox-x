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
#include "callback.h"
#include "setup.h"
#include "serialport.h"
#include "mapper.h"
#include "vga.h"
#include "regionalloctracking.h"
extern bool PS1AudioCard;
#include "parport.h"
#include <time.h>
#include <sys/timeb.h>
/* mouse.cpp */
extern bool en_bios_ps2mouse;
extern bool mainline_compatible_bios_mapping;
extern bool rom_bios_8x8_cga_font;
extern bool pcibus_enable;

bool VM_Boot_DOSBox_Kernel();

bool bochs_port_e9 = false;
bool isa_memory_hole_512kb = false;
bool int15_wait_force_unmask_irq = false;

Bit16u biosConfigSeg=0;

Bitu BIOS_DEFAULT_IRQ0_LOCATION = ~0;		// (RealMake(0xf000,0xfea5))
Bitu BIOS_DEFAULT_IRQ1_LOCATION = ~0;		// (RealMake(0xf000,0xe987))
Bitu BIOS_DEFAULT_IRQ07_DEF_LOCATION = ~0;	// (RealMake(0xf000,0xff55))
Bitu BIOS_DEFAULT_IRQ815_DEF_LOCATION = ~0;	// (RealMake(0xf000,0xe880))

Bitu BIOS_DEFAULT_HANDLER_LOCATION = ~0;	// (RealMake(0xf000,0xff53))

Bitu BIOS_VIDEO_TABLE_LOCATION = ~0;		// RealMake(0xf000,0xf0a4)
Bitu BIOS_VIDEO_TABLE_SIZE = 0;

Bitu BIOS_DEFAULT_RESET_LOCATION = ~0;		// RealMake(0xf000,0xe05b)

bool allow_more_than_640kb = false;

unsigned int APM_BIOS_connected_minor_version = 0;// what version the OS connected to us with. default to 1.0
unsigned int APM_BIOS_minor_version = 2;	// what version to emulate e.g to emulate 1.2 set this to 2

/* default bios type/version/date strings */
const char* const bios_type_string = "IBM COMPATIBLE 486 BIOS COPYRIGHT The DOSBox Team.";
const char* const bios_version_string = "DOSBox FakeBIOS v1.0";
const char* const bios_date_string = "01/01/92";

bool						APM_inactivity_timer = true;

RegionAllocTracking				rombios_alloc;

Bitu						rombios_minimum_location = 0xF0000; /* minimum segment allowed */
Bitu						rombios_minimum_size = 0x10000;

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
	if (val == '\n' || val == '\r') {
		bochs_port_e9_flush();
	}
	else {
		bochs_port_e9_line += (char)val;
		if (bochs_port_e9_line.length() >= 256)
			bochs_port_e9_flush();
	}
}

void ROMBIOS_DumpMemory() {
	rombios_alloc.logDump();
}

void ROMBIOS_SanityCheck() {
	rombios_alloc.sanityCheck();
}

Bitu ROMBIOS_MinAllocatedLoc() {
	Bitu r = rombios_alloc.getMinAddress();

	if (r > (0x100000 - rombios_minimum_size))
		r = (0x100000 - rombios_minimum_size);

	return r & ~0xFFF;
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

static IO_Callout_t dosbox_int_iocallout = IO_Callout_t_none;

static unsigned char dosbox_int_register_shf = 0;
static uint32_t dosbox_int_register = 0;
static unsigned char dosbox_int_regsel_shf = 0;
static uint32_t dosbox_int_regsel = 0;
static bool dosbox_int_error = false;
static bool dosbox_int_busy = false;
static const char *dosbox_int_version = "DOSBox-X integration device v1.0";
static const char *dosbox_int_ver_read = NULL;

struct dosbox_int_saved_state {
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

static std::string dosbox_int_debug_out;

/* read triggered, update the regsel */
void dosbox_integration_trigger_read() {
	dosbox_int_error = false;

	switch (dosbox_int_regsel) {
		case 0: /* Identification */
			dosbox_int_register = 0xD05B0740;
			break;
		case 1: /* test */
			break;
		case 2: /* version string */
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
		case 3: /* version number */
			dosbox_int_register = (0x01U/*major*/) + (0x00U/*minor*/ << 8U) + (0x00U/*subver*/ << 16U) + (0x01U/*bump*/ << 24U);
			break;

//		case 0x804200: /* keyboard input injection -- not supposed to read */
//			break;

		case 0x804201: /* keyboard status */
			uint32_t Keyb_ig_status();
			dosbox_int_register = Keyb_ig_status();
			break;

        case 0x434D54: /* read user mouse status */
            dosbox_int_register =
                (user_cursor_locked ? (1UL << 0UL) : 0UL);      /* bit 0 = mouse capture lock */
            break;

        case 0x434D55: /* read user mouse cursor position */
            dosbox_int_register = ((user_cursor_y & 0xFFFFUL) << 16UL) | (user_cursor_x & 0xFFFFUL);
            break;

        case 0x434D56: { /* read user mouse cursor position (normalized for Windows 3.x) */
            signed long long x = ((signed long long)user_cursor_x << 16LL) / (signed long long)(user_cursor_sw-1);
            signed long long y = ((signed long long)user_cursor_y << 16LL) / (signed long long)(user_cursor_sh-1);
            if (x < 0x0000LL) x = 0x0000LL;
            if (x > 0xFFFFLL) x = 0xFFFFLL;
            if (y < 0x0000LL) y = 0x0000LL;
            if (y > 0xFFFFLL) y = 0xFFFFLL;
            dosbox_int_register = ((unsigned int)y << 16UL) | (unsigned int)x;
            } break;

		case 0xC54010: /* Screenshot/capture trigger */
			/* TODO: This should also be hidden behind an enable switch, so that rogue DOS development
			 *       can't retaliate if the user wants to capture video or screenshots. */
#if (C_SSHOT)
			extern Bitu CaptureState;
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

		case 0xAA55BB66UL: /* interface reset result */
			break;

		default:
			dosbox_int_register = 0xAA55AA55;
			dosbox_int_error = true;
			break;
	};

	LOG(LOG_MISC,LOG_DEBUG)("DOSBox integration read 0x%08lx got 0x%08lx (err=%u)\n",
		(unsigned long)dosbox_int_regsel,
		(unsigned long)dosbox_int_register,
		dosbox_int_error?1:0);
}

unsigned int mouse_notify_mode = 0;
// 0 = off
// 1 = trigger as PS/2 mouse interrupt

/* write triggered */
void dosbox_integration_trigger_write() {
	dosbox_int_error = false;

	LOG(LOG_MISC,LOG_DEBUG)("DOSBox integration write 0x%08lx val 0x%08lx\n",
		(unsigned long)dosbox_int_regsel,
		(unsigned long)dosbox_int_register);

	switch (dosbox_int_regsel) {
		case 1: /* test */
			break;

		case 2: /* version string */
			dosbox_int_ver_read = NULL;
			break;

		case 0xDEB0: /* debug output (to log) */
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

		case 0xDEB1: /* debug output clear */
			dosbox_int_debug_out.clear();
			break;

		case 0x52434D: /* release mouse capture 'MCR' */
            void GFX_ReleaseMouse(void);
            GFX_ReleaseMouse();
            break;

		case 0x804200: /* keyboard input injection */
			void Mouse_ButtonPressed(Bit8u button);
			void Mouse_ButtonReleased(Bit8u button);
			void Mouse_CursorMoved(float xrel,float yrel,float x,float y,bool emulate);
			void KEYBOARD_AUX_Event(float x,float y,Bitu buttons,int scrollwheel);
			void KEYBOARD_AddBuffer(Bit16u data);

			switch ((dosbox_int_register>>8)&0xFF) {
				case 0x00: // keyboard
					KEYBOARD_AddBuffer(dosbox_int_register&0xFF);
					break;
				case 0x01: // AUX
					KEYBOARD_AddBuffer((dosbox_int_register&0xFF)|0x100/*AUX*/);
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

//		case 0x804201: /* keyboard status do not write */
//			break;

        /* this command is used to enable notification of mouse movement over the windows even if the mouse isn't captured */
        case 0x434D55: /* read user mouse cursor position */
        case 0x434D56: /* read user mouse cursor position (normalized for Windows 3.x) */
            mouse_notify_mode = dosbox_int_register & 0xFF;
            LOG(LOG_MISC,LOG_DEBUG)("Mouse notify mode=%u",mouse_notify_mode);
            break;
 
		case 0xC54010: /* Screenshot/capture trigger */
#if (C_SSHOT)
			void CAPTURE_ScreenShotEvent(bool pressed);
			void CAPTURE_VideoEvent(bool pressed);
#endif
			void CAPTURE_WaveEvent(bool pressed);

			/* TODO: It would be wise to grant/deny access to this register through another dosbox.conf option
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

		default:
			dosbox_int_register = 0x55AA55AA;
			dosbox_int_error = true;
			break;
	};
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
	uint32_t msk;

    while (iolen > 0) {
        msk = 0xFFU << (dosbox_int_regsel_shf * 8);
        dosbox_int_regsel = (dosbox_int_regsel & ~msk) + ((val & 0xFF) << (dosbox_int_regsel_shf * 8));
        if ((++dosbox_int_regsel_shf) >= 4) dosbox_int_regsel_shf = 0;
        val >>= 8U;
        iolen--;
    }
}

static Bitu dosbox_integration_port01_data_r(Bitu port,Bitu iolen) {
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
	uint32_t msk;

    while (iolen > 0) {
        msk = 0xFFU << (dosbox_int_register_shf * 8);
        dosbox_int_register = (dosbox_int_register & ~msk) + ((val & 0xFF) << (dosbox_int_register_shf * 8));
        if ((++dosbox_int_register_shf) >= 4) dosbox_int_register_shf = 0;
        if (dosbox_int_register_shf == 0) dosbox_integration_trigger_write();
        val >>= 8U;
        iolen--;
    }
}

static Bitu dosbox_integration_port02_status_r(Bitu port,Bitu iolen) {
	/* status */
    /* 7:6 = regsel byte index
     * 5:4 = register byte index
     * 3:2 = reserved
     *   1 = error
     *   0 = busy */
    return (dosbox_int_regsel_shf << 6) + (dosbox_int_register_shf << 4) +
        (dosbox_int_error ? 2 : 0) + (dosbox_int_busy ? 1 : 0);
}

static void dosbox_integration_port02_command_w(Bitu port,Bitu val,Bitu iolen) {
    switch (val) {
        case 0x00: /* reset latch */
            dosbox_int_register_shf = 0;
            dosbox_int_regsel_shf = 0;
            break;
        case 0x01: /* flush write */
            if (dosbox_int_register_shf != 0) {
                dosbox_integration_trigger_write();
                dosbox_int_register_shf = 0;
            }
            break;
        case 0x20: /* push state */
            if (dosbox_int_push_save_state()) {
                dosbox_int_register_shf = 0;
                dosbox_int_regsel_shf = 0;
                dosbox_int_error = false;
                dosbox_int_busy = false;
                dosbox_int_regsel = 0xAA55BB66;
                dosbox_int_register = 0xD05B0C5;
                LOG(LOG_MISC,LOG_DEBUG)("DOSBOX IG state saved");
            }
            else {
                LOG(LOG_MISC,LOG_DEBUG)("DOSBOX IG unable to push state, stack overflow");
                dosbox_int_error = true;
            }
            break;
        case 0x21: /* pop state */
            if (dosbox_int_pop_save_state()) {
                LOG(LOG_MISC,LOG_DEBUG)("DOSBOX IG state restored");
            }
            else {
                LOG(LOG_MISC,LOG_DEBUG)("DOSBOX IG unable to pop state, stack underflow");
                dosbox_int_error = true;
            }
            break;
        case 0x22: /* discard state */
            if (dosbox_int_discard_save_state()) {
                LOG(LOG_MISC,LOG_DEBUG)("DOSBOX IG state discarded");
            }
            else {
                LOG(LOG_MISC,LOG_DEBUG)("DOSBOX IG unable to discard state, stack underflow");
                dosbox_int_error = true;
            }
            break;
        case 0x23: /* discard all state */
            while (dosbox_int_discard_save_state());
            break;
        case 0xFE: /* clear error */
            dosbox_int_error = false;
            break;
        case 0xFF: /* reset interface */
            dosbox_int_busy = false;
            dosbox_int_error = false;
            dosbox_int_regsel = 0xAA55BB66;
            dosbox_int_register = 0xD05B0C5;
            break;
        default:
            dosbox_int_error = true;
            break;
    }
}

static const IO_ReadHandler* dosbox_integration_cb_ports_r[4] = {
    dosbox_integration_port00_index_r,
    dosbox_integration_port01_data_r,
    dosbox_integration_port02_status_r,
    NULL
};

static IO_ReadHandler* dosbox_integration_cb_port_r(IO_CalloutObject &co,Bitu port,Bitu iolen) {
    return dosbox_integration_cb_ports_r[port&3];
}

static const IO_WriteHandler* dosbox_integration_cb_ports_w[4] = {
    dosbox_integration_port00_index_w,
    dosbox_integration_port01_data_w,
    dosbox_integration_port02_command_w,
    NULL
};

static IO_WriteHandler* dosbox_integration_cb_port_w(IO_CalloutObject &co,Bitu port,Bitu iolen) {
    return dosbox_integration_cb_ports_w[port&3];
}

/* if mem_systems 0 then size_extended is reported as the real size else 
 * zero is reported. ems and xms can increase or decrease the other_memsystems
 * counter using the BIOS_ZeroExtendedSize call */
static Bit16u size_extended;
static unsigned int ISA_PNP_WPORT = 0x20B;
static unsigned int ISA_PNP_WPORT_BIOS = 0;
static IO_ReadHandleObject *ISAPNP_PNP_READ_PORT = NULL;		/* 0x200-0x3FF range */
static IO_WriteHandleObject *ISAPNP_PNP_ADDRESS_PORT = NULL;		/* 0x279 */
static IO_WriteHandleObject *ISAPNP_PNP_DATA_PORT = NULL;		/* 0xA79 */
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
static bool apm_realmode_connected = false;
void CMOS_SetRegister(Bitu regNr, Bit8u val); //For setting equipment word
bool enable_integration_device=false;
bool ISAPNPBIOS=false;
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
		case APMBIOS_CONNECT_REAL:	return 0x02;
		case APMBIOS_CONNECT_PROT16:	return 0x05;
		case APMBIOS_CONNECT_PROT32:	return 0x07;
	}

	return 0x00;
}

ISAPnPDevice::ISAPnPDevice() {
	CSN = 0;
	logical_device = 0;
	memset(ident,0,sizeof(ident));
	ident_bp = 0;
	ident_2nd = 0;
	resource_data_len = 0;
	resource_data_pos = 0;
	alloc_res = NULL;
	alloc_write = 0;
	alloc_sz = 0;
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
	alloc(0);
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
	write_byte((((unsigned char)c2 & 0x07U) << 5) + (((unsigned char)c3 & 0x1FU)     ));
	write_byte((((unsigned char)c4 & 0x0FU) << 4) + (((unsigned char)c5 & 0x0FU)     ));
	write_byte((((unsigned char)c6 & 0x0FU) << 4) + (((unsigned char)c7 & 0x0FU)     ));
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
	if (write_irq_info) write_byte(((unsigned char)IRQ_signal_type & 0x0F));
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
	if (alloc_res == NULL || alloc_write >= alloc_sz) return;

	while (*str != 0 && alloc_write < alloc_sz)
		alloc_res[alloc_write++] = *str++;
}

void ISAPnPDevice::write_Identifier_String(const char *str) {
	const size_t l = strlen(str);
	if (l > 4096) return;

	write_begin_LARGETAG(LargeTags::IdentifierStringANSI,l);
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

#define MAX_ISA_PNP_DEVICES		64
#define MAX_ISA_PNP_SYSDEVNODES		256

static ISAPnPDevice *ISA_PNP_selected = NULL;
static ISAPnPDevice *ISA_PNP_devs[MAX_ISA_PNP_DEVICES] = {NULL}; /* FIXME: free objects on shutdown */
static Bitu ISA_PNP_devnext = 0;

static const unsigned char ISAPnPIntegrationDevice_sysdev[] = {
	ISAPNP_IO_RANGE(
			0x01,					/* decodes 16-bit ISA addr */
			0x28,0x28,				/* min-max range I/O port */
			0x04,0x04),				/* align=4 length=4 */
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
	virtual ~ISAPNP_SysDevNode() {
		if (own) delete[] raw;
	}
public:
	unsigned char*		raw;
	int			raw_len;
	bool			own;
};

static ISAPNP_SysDevNode*	ISAPNP_SysDevNodes[MAX_ISA_PNP_SYSDEVNODES] = {NULL};
static Bitu			ISAPNP_SysDevNodeLargest=0;
static Bitu			ISAPNP_SysDevNodeCount=0;

void ISA_PNP_FreeAllSysNodeDevs() {
	Bitu i;

	for (i=0;i < MAX_ISA_PNP_SYSDEVNODES;i++) {
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
		if (ISA_PNP_WPORT_BIOS == 0) ISA_PNP_WPORT_BIOS = ISA_PNP_WPORT;
		ISA_PNP_devs[ISA_PNP_devnext++] = x;
		x->CSN = ISA_PNP_devnext;
	}
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
		case 0x05:	/* read resource status */
			   if (ISA_PNP_selected) {
				   /* real-world hardware testing shows that devices act as if there was some fixed block of ROM,
				    * that repeats every 128, 256, 512, or 1024 bytes if you just blindly read from this port.
				    * therefore, there's always a byte to return. */
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

//	if (1) LOG_MSG("PnP read(%02X) = %02X\n",ISA_PNP_CUR_ADDR,ret);
	return ret;
}

void isapnp_write_port(Bitu port,Bitu val,Bitu /*iolen*/) {
	Bitu i;

	if (port == 0x279) {
//		if (1) LOG_MSG("PnP addr(%02X)\n",val);
		if (val == isa_pnp_init_keystring[ISA_PNP_KEYMATCH]) {
			if (++ISA_PNP_KEYMATCH == 32) {
//				LOG_MSG("ISA PnP key -> going to sleep\n");
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
//		if (1) LOG_MSG("PnP write(%02X) = %02X\n",ISA_PNP_CUR_ADDR,val);
		switch (ISA_PNP_CUR_ADDR) {
			case 0x00: {	/* RD_DATA */
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

static Bitu INT15_Handler(void);

// FIXME: This initializes both APM BIOS and ISA PNP emulation!
//        Need to separate APM BIOS init from ISA PNP init from ISA PNP BIOS init!
//        It might also be appropriate to move this into the BIOS init sequence.
void ISAPNP_Cfg_Reset(Section *sec) {
	Section_prop * section=static_cast<Section_prop *>(control->GetSection("cpu"));

	LOG(LOG_MISC,LOG_DEBUG)("Initializing ISA PnP emulation");

	enable_integration_device = section->Get_bool("integration device");
	ISAPNPBIOS = section->Get_bool("isapnpbios");
	APMBIOS = section->Get_bool("apmbios");
	APMBIOS_pnp = section->Get_bool("apmbios pnp");
	APMBIOS_allow_realmode = section->Get_bool("apmbios allow realmode");
	APMBIOS_allow_prot16 = section->Get_bool("apmbios allow 16-bit protected mode");
	APMBIOS_allow_prot32 = section->Get_bool("apmbios allow 32-bit protected mode");

	std::string apmbiosver = section->Get_string("apmbios version");

	if (apmbiosver == "1.0")
		APM_BIOS_minor_version = 0;
	else if (apmbiosver == "1.1")
		APM_BIOS_minor_version = 1;
	else if (apmbiosver == "1.2")
		APM_BIOS_minor_version = 2;
	else//auto
		APM_BIOS_minor_version = 2;

	LOG(LOG_MISC,LOG_DEBUG)("APM BIOS allow: real=%u pm16=%u pm32=%u version=1.%u",
		APMBIOS_allow_realmode,
		APMBIOS_allow_prot16,
		APMBIOS_allow_prot32,
		APM_BIOS_minor_version);

	if (APMBIOS && (APMBIOS_allow_prot16 || APMBIOS_allow_prot32) && INT15_apm_pmentry == 0) {
		Bitu cb,base;

		/* NTS: This is... kind of a terrible hack. It basically tricks Windows into executing our
		 *      INT 15h handler as if the APM entry point. Except that instead of an actual INT 15h
		 *      triggering the callback, a FAR CALL triggers the callback instead (CB_RETF not CB_IRET). */
		/* TODO: We should really consider moving the APM BIOS code in INT15_Handler() out into it's
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
         *      it's way back to Windows, meaning that when APM BIOS emulation intended to return
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

        phys_writeb(base+0x03,(Bit8u)0xFE);						//GRP 4
        phys_writeb(base+0x04,(Bit8u)0x38);						//Extra Callback instruction
        phys_writew(base+0x05,(Bit16u)cb);               		//The immediate word

        phys_writeb(base+0x07,0x5D);                             /* pop (e)bp */
        phys_writeb(base+0x08,0x5D);                             /* pop (e)bp */
        phys_writeb(base+0x09,0x9D);                             /* popf */
        phys_writeb(base+0x0A,0xCB);                             /* retf */
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
			0x04,0x04),				/* align=4 length=4 */
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
			ISAPNP_TYPE(0x08,0x80,0x00),		/* type: System peripheral, Other */
			0x0001 | 0x0002),			/* can't disable, can't configure */
	/*----------allocated--------*/
	ISAPNP_IO_RANGE(
			0x01,					/* decodes 16-bit ISA addr */
			0x208,0x208,				/* min-max range I/O port */
			0x04,0x04),				/* align=4 length=4 */
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
			ISAPNP_TYPE(0x06,0x04,0x00),		/* type: System device, peripheral bus */
			0x0001 | 0x0002),			/* can't disable, can't configure */
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
			ISAPNP_TYPE(0x06,0x04,0x00),		/* type: System device, peripheral bus */
			0x0001 | 0x0002),			/* can't disable, can't configure */
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
			ISAPNP_TYPE(0x08,0x80,0x00),		/* type: FIXME is this right?? I can't find any examples or documentation */
			0x0001 | 0x0002),			/* can't disable, can't configure */
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
		//	ISAPNP_CPU_ProtMode() ? "Protected mode" : "Real mode");
		reg_ax = 0x84;/* BAD_PARAMETER */
		return 0;
	}

	func = mem_readw(arg);
//	LOG_MSG("PnP prot=%u DS=%04x (base=0x%08lx) SS:ESP=%04x:%04x (base=0x%08lx phys=0x%08lx) function=0x%04x\n",
//		(unsigned int)protmode,(unsigned int)SegValue(ds),(unsigned long)SegPhys(ds),
//		(unsigned int)SegValue(ss),(unsigned int)reg_esp,(unsigned long)SegPhys(ss),
//		(unsigned long)arg,(unsigned int)func);

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

			ISAPNP_SysDevNode *nd = ISAPNP_SysDevNodes[Node];

			mem_writew(devNodeBuffer_ptr+0,nd->raw_len+3); /* Length */
			mem_writeb(devNodeBuffer_ptr+2,Node); /* on most PnP BIOS implementations I've seen "handle" is set to the same value as Node */
			for (i=0;i < (Bitu)nd->raw_len;i++)
				mem_writeb(devNodeBuffer_ptr+i+3,nd->raw[i]);

//			LOG_MSG("ISAPNP OS asked for Node 0x%02x\n",Node);

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
					LOG_MSG("Plug & Play OS requested power off.\n");
					throw 1;	/* NTS: Based on the Reboot handler code, causes DOSBox to cleanly shutdown and exit */
					reg_ax = 0;
					break;
				case 0x42:	/* PNP_OS_ACTIVE */
					LOG_MSG("Plug & Play OS reports itself active\n");
					reg_ax = 0;
					break;
				case 0x43:	/* PNP_OS_INACTIVE */
					LOG_MSG("Plug & Play OS reports itself inactive\n");
					reg_ax = 0;
					break;
				default:
					LOG_MSG("Unknown ISA PnP message 0x%04x\n",(int)Message);
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
				mem_writeb(ph+1,ISA_PNP_devnext); /* ->total_csn */
				mem_writew(ph+2,ISA_PNP_WPORT_BIOS);	/* ->isa_pnp_port */
				mem_writew(ph+4,0);		/* ->reserved */
			}

			reg_ax = 0x00;/* SUCCESS */
		} break;
		default:
			//LOG_MSG("Unsupported ISA PnP function 0x%04x\n",func);
			reg_ax = 0x82;/* FUNCTION_NOT_SUPPORTED */
			break;
	};

	return 0;
badBiosSelector:
	/* return an error. remind the user (possible developer) how lucky he is, a real
	 * BIOS implementation would CRASH when misused like this */
	LOG_MSG("ISA PnP function 0x%04x called with incorrect BiosSelector parameter 0x%04x\n",(int)func,(int)BiosSelector);
	LOG_MSG(" > STACK %04X %04X %04X %04X %04X %04X %04X %04X\n",
		mem_readw(arg),		mem_readw(arg+2),	mem_readw(arg+4),	mem_readw(arg+6),
		mem_readw(arg+8),	mem_readw(arg+10),	mem_readw(arg+12),	mem_readw(arg+14));

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
		if (pcibus_enable) {
			LOG(LOG_BIOS,LOG_WARN)("INT1A:PCI bios call %2X",reg_al);
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
					reg_ah=0x00;
					break;
				case 0x09:	// read configuration word
					IO_WriteD(0xcf8,0x80000000|(reg_bx<<8)|(reg_di&0xfc));
					reg_cx=IO_ReadW(0xcfc+(reg_di&2));
					CALLBACK_SCF(false);
					reg_ah=0x00;
					break;
				case 0x0a:	// read configuration dword
					IO_WriteD(0xcf8,0x80000000|(reg_bx<<8)|(reg_di&0xfc));
					reg_ecx=IO_ReadD(0xcfc+(reg_di&3));
					CALLBACK_SCF(false);
					reg_ah=0x00;
					break;
				case 0x0b:	// write configuration byte
					IO_WriteD(0xcf8,0x80000000|(reg_bx<<8)|(reg_di&0xfc));
					IO_WriteB(0xcfc+(reg_di&3),reg_cl);
					CALLBACK_SCF(false);
					reg_ah=0x00;
					break;
				case 0x0c:	// write configuration word
					IO_WriteD(0xcf8,0x80000000|(reg_bx<<8)|(reg_di&0xfc));
					IO_WriteW(0xcfc+(reg_di&2),reg_cx);
					CALLBACK_SCF(false);
					reg_ah=0x00;
					break;
				case 0x0d:	// write configuration dword
					IO_WriteD(0xcf8,0x80000000|(reg_bx<<8)|(reg_di&0xfc));
					IO_WriteD(0xcfc+(reg_di&3),reg_ecx);
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

bool INT16_get_key(Bit16u &code);
bool INT16_peek_key(Bit16u &code);

extern uint8_t                     GDC_display_plane;

unsigned char prev_pc98_mode42 = 0;

static Bitu INT18_PC98_Handler(void) {
    Bit16u temp16;

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
                reg_bh = 0;
            }
            break;
        case 0x02: /* Sense of shift key state (シフト・キー状態のセンス) */
            reg_al = 0x00; /* TODO */
            break;
        case 0x03: /* Initialization of keyboard interface (キーボード・インタフェイスの初期化) */
            /* TODO */
            break;
        case 0x04: /* Sense of key input state (キー入力状態のセンス) */
            reg_ah = mem_readb(0x52A + (reg_al & 0x0F));
            break;
        case 0x05: /* Key input sense (キー入力センス) */
            /* This appears to return a key from the buffer (and remove from
             * buffer) or return BH == 0 to signal no key was pending. */
            if (INT16_get_key(temp16)) {
                reg_ax = temp16;
                reg_bh = 1;
            }
            else {
                reg_bh = 0;
            }
            break;
        // TODO: "Edge" is using INT 18h AH=06h, what is that?
        //       Neko Project is also unaware of such a call.
        case 0x0C: /* graphics layer enable */
            pc98_gdc[GDC_SLAVE].force_fifo_complete();
            pc98_gdc[GDC_SLAVE].display_enable = true;
            break;
        case 0x0D: /* graphics layer disable */
            pc98_gdc[GDC_SLAVE].force_fifo_complete();
            pc98_gdc[GDC_SLAVE].display_enable = false;
            break;
        case 0x11: /* show cursor */
            pc98_gdc[GDC_MASTER].force_fifo_complete();
            pc98_gdc[GDC_MASTER].cursor_enable = true;
            break;
        case 0x12: /* hide cursor */
            pc98_gdc[GDC_MASTER].force_fifo_complete();
            pc98_gdc[GDC_MASTER].cursor_enable = false;
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
        case 0x1A: /* load FONT RAM */
            {
                unsigned int i,o,r;

                /* DX = code (must be 0x76xx or 0x7700)
                 * BX:CX = 34-byte region to read from */
                if ((reg_dh & 0x7E) == 0x76) {
                    i = (reg_bx << 4) + reg_cx + 2;
                    for (r=0;r < 16;r++) {
                        o = (((((reg_dl & 0x7F)*128)+((reg_dh - 0x20) & 0x7F))*16)+r)*2;

                        assert((o+2) <= sizeof(vga.draw.font));

                        vga.draw.font[o+0] = mem_readb(i+(r*2)+0);
                        vga.draw.font[o+1] = mem_readb(i+(r*2)+1);
                    }
                }
                else {
                    LOG_MSG("PC-98 INT 18h AH=1Ah font RAM load ignored, code 0x%04x out of range",reg_dx);
                }
            }
            break;
        /* From this point on the INT 18h call list appears to wander off from the keyboard into CRT/GDC/display management. */
        case 0x40: /* Start displaying the graphics screen (グラフィック画面の表示開始) */
            pc98_gdc[GDC_SLAVE].force_fifo_complete();
            pc98_gdc[GDC_SLAVE].display_enable = true;
 
            {
                unsigned char b = mem_readb(0x54C/*MEMB_PRXCRT*/);
                mem_writeb(0x54C/*MEMB_PRXCRT*/,b | 0x80);
            }
            break;
        case 0x41: /* Stop displaying the graphics screen (グラフィック画面の表示終了) */
            pc98_gdc[GDC_SLAVE].force_fifo_complete();
            pc98_gdc[GDC_SLAVE].display_enable = false;

            {
                unsigned char b = mem_readb(0x54C/*MEMB_PRXCRT*/);
                mem_writeb(0x54C/*MEMB_PRXCRT*/,b & (~0x80));
            }
            break;
        case 0x42: /* Display area setup (表示領域の設定) */
            pc98_gdc[GDC_MASTER].force_fifo_complete();
            pc98_gdc[GDC_SLAVE].force_fifo_complete();
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
            pc98_gdc[GDC_SLAVE].param_ram[3] = 0x3F;
            pc98_gdc[GDC_SLAVE].active_display_words_per_line = 40; /* 40 x 16 = 640 pixel wide graphics */

            // CH
            //   [7:6] = G-RAM setup
            //           00 = no graphics (?)
            //           01 = 640x200 upper half
            //           10 = 640x200 lower half
            //           11 = 640x400

            // FIXME: This is a guess. I have no idea as to actual behavior, yet.
            //        This seems to help with clearing the text layer when games start the graphics.
            //        This is ALSO how we will detect games that switch on the 200-line double-scan mode vs 400-line mode.
            if ((reg_ch & 0xC0) != 0) {
                pc98_gdc[GDC_SLAVE].doublescan = ((reg_ch & 0xC0) == 0x40) || ((reg_ch & 0xC0) == 0x80);
                pc98_gdc[GDC_SLAVE].row_height = pc98_gdc[GDC_SLAVE].doublescan ? 2 : 1;
            }
            else {
                pc98_gdc[GDC_SLAVE].doublescan = false;
                pc98_gdc[GDC_SLAVE].row_height = 1;
            }

            {
                unsigned char b = mem_readb(0x54C/*MEMB_PRXCRT*/);

                // Real hardware behavior: graphics selection updated by BIOS to reflect MEMB_PRXCRT state
                pc98_gdc[GDC_SLAVE].display_enable = !!(b & 0x80);
            }

            pc98_gdc_vramop &= ~(1 << VOPBIT_ACCESS);
            GDC_display_plane = 0;

            prev_pc98_mode42 = reg_ch;

            LOG_MSG("PC-98 INT 18 AH=42h CH=0x%02X",reg_ch);
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
    };

    /* FIXME: What do actual BIOSes do when faced with an unknown INT 18h call? */
    return CBRET_NONE;
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
    LOG_MSG("PC-98 INT 1Bh unknown call AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X",
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

static Bitu INT1C_PC98_Handler(void) {
    LOG_MSG("PC-98 INT 1Ch unknown call AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X",
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

static Bitu INT1F_PC98_Handler(void) {
    LOG_MSG("PC-98 INT 1Fh unknown call AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X",
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

// TODO: make option
bool enable_bios_timer_synchronize_keyboard_leds = true;

void KEYBOARD_SetLEDs(Bit8u bits);

void BIOS_KEYBOARD_SetLEDs(Bitu state) {
	Bitu x = mem_readb(BIOS_KEYBOARD_LEDS);

	x &= ~7;
	x |= (state & 7);
	mem_writeb(BIOS_KEYBOARD_LEDS,x);
	KEYBOARD_SetLEDs(state);
}

static Bitu INT8_Handler(void) {
	/* Increase the bios tick counter */
	Bit32u value = mem_readd(BIOS_TIMER) + 1;
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
	   it when handling the keyboard from it's own driver. Their driver does
	   hook the keyboard and handles keyboard I/O by itself, but it still
	   allows the BIOS to do the keyboard magic from IRQ 0 (INT 8h). Yech. */
	if (enable_bios_timer_synchronize_keyboard_leds) {
		Bitu should_be = (mem_readb(BIOS_KEYBOARD_STATE) >> 4) & 7;
		Bitu led_state = (mem_readb(BIOS_KEYBOARD_LEDS) & 7);

		if (should_be != led_state)
			BIOS_KEYBOARD_SetLEDs(should_be);
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

Bits HLT_Decode(void);
void KEYBOARD_AUX_Write(Bitu val);
unsigned char KEYBOARD_AUX_GetType();
unsigned char KEYBOARD_AUX_DevStatus();
unsigned char KEYBOARD_AUX_Resolution();
unsigned char KEYBOARD_AUX_SampleRate();
void KEYBOARD_ClrBuffer(void);

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
		CPU_SetSegGeneral(es,biosConfigSeg);
		reg_bx = 0;
		reg_ah = 0;
		CALLBACK_SCF(false);
		break;
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
			Bit8u t;
			Bit32u count=(reg_cx<<16)|reg_dx;
			mem_writed(BIOS_WAIT_FLAG_POINTER,RealMake(0,BIOS_WAIT_FLAG_TEMP));
			mem_writed(BIOS_WAIT_FLAG_COUNT,count);
			mem_writeb(BIOS_WAIT_FLAG_ACTIVE,1);

			/* if the user has not set the option, warn if IRQs are masked */
			if (!int15_wait_force_unmask_irq) {
				/* make sure our wait function works by unmasking IRQ 2, and IRQ 8.
				 * (bugfix for 1993 demo Yodel "Mayday" demo. this demo keeps masking IRQ 2 for some stupid reason.) */
				if ((t=IO_Read(0x21)) & (1 << 2)) {
					LOG(LOG_BIOS,LOG_ERROR)("INT15:86:Wait: IRQ 2 masked during wait. "
						"Consider adding 'int15 wait force unmask irq=true' to your dosbox.conf");
				}
				if ((t=IO_Read(0xA1)) & (1 << 0)) {
					LOG(LOG_BIOS,LOG_ERROR)("INT15:86:Wait: IRQ 8 masked during wait. "
						"Consider adding 'int15 wait force unmask irq=true' to your dosbox.conf");
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
			Segs.limit[cs] = 0xFFFF;
			Segs.limit[ds] = 0xFFFF;
			Segs.limit[es] = 0xFFFF;
			Segs.limit[ss] = 0xFFFF;
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
//			LOG_MSG("INT 15h AX=%04x BX=%04x\n",reg_ax,reg_bx);
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
					Mouse_SetPS2State(false);
					KEYBOARD_ClrBuffer();
					reg_bx=0x00aa;	// mouse
					// fall through
				case 0x05:		// initialize
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
					LOG_MSG("INT 15h reporting mouse device ID 0x%02x\n",reg_bh);
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
	case 0xc4:	/* BIOS POS Programm option Select */
		LOG(LOG_BIOS,LOG_NORMAL)("INT15:Function %X called, bios mouse not supported",reg_ah);
		CALLBACK_SCF(true);
		break;
	case 0x53: // APM BIOS
		if (APMBIOS) {
			LOG(LOG_BIOS,LOG_DEBUG)("APM BIOS call AX=%04x BX=0x%04x CX=0x%04x\n",reg_ax,reg_bx,reg_cx);
			switch(reg_al) {
				case 0x00: // installation check
					reg_ah = 1;				// major
					reg_al = APM_BIOS_minor_version;	// minor
					reg_bx = 0x504d;			// 'PM'
					reg_cx = (APMBIOS_allow_prot16?0x01:0x00) + (APMBIOS_allow_prot32?0x02:0x00);
					// 32-bit interface seems to be needed for standby in win95
					CALLBACK_SCF(false);
					break;
				case 0x01: // connect real mode interface
					if(!APMBIOS_allow_realmode) {
						LOG_MSG("APM BIOS: OS attemped real-mode connection, which is disabled in your dosbox.conf\n");
						reg_ah = 0x86;	// APM not present
						CALLBACK_SCF(true);			
						break;
					}
					if(reg_bx != 0x0) {
						reg_ah = 0x09;	// unrecognized device ID
						CALLBACK_SCF(true);			
						break;
					}
					if(!apm_realmode_connected) { // not yet connected
						LOG_MSG("APM BIOS: Connected to real-mode interface\n");
						CALLBACK_SCF(false);
						APMBIOS_connect_mode = APMBIOS_CONNECT_REAL;
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
						LOG_MSG("APM BIOS: OS attemped 16-bit protected mode connection, which is disabled in your dosbox.conf\n");
						reg_ah = 0x06;	// not supported
						CALLBACK_SCF(true);			
						break;
					}
					if(reg_bx != 0x0) {
						reg_ah = 0x09;	// unrecognized device ID
						CALLBACK_SCF(true);			
						break;
					}
					if(!apm_realmode_connected) { // not yet connected
						/* NTS: We use the same callback address for both 16-bit and 32-bit
						 *      because only the DOS callback and RETF instructions are involved,
						 *      which can be executed as either 16-bit or 32-bit code without problems. */
						LOG_MSG("APM BIOS: Connected to 16-bit protected mode interface\n");
						CALLBACK_SCF(false);
						reg_ax = INT15_apm_pmentry >> 16;	// AX = 16-bit code segment (real mode base)
						reg_bx = INT15_apm_pmentry & 0xFFFF;	// BX = offset of entry point
						reg_cx = INT15_apm_pmentry >> 16;	// CX = 16-bit data segment (NTS: doesn't really matter)
						reg_si = 0xFFFF;			// SI = code segment length
						reg_di = 0xFFFF;			// DI = data segment length
						APMBIOS_connect_mode = APMBIOS_CONNECT_PROT16;
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
						LOG_MSG("APM BIOS: OS attemped 32-bit protected mode connection, which is disabled in your dosbox.conf\n");
						reg_ah = 0x08;	// not supported
						CALLBACK_SCF(true);			
						break;
					}
					if(reg_bx != 0x0) {
						reg_ah = 0x09;	// unrecognized device ID
						CALLBACK_SCF(true);			
						break;
					}
					if(!apm_realmode_connected) { // not yet connected
						LOG_MSG("APM BIOS: Connected to 32-bit protected mode interface\n");
						CALLBACK_SCF(false);
						/* NTS: We use the same callback address for both 16-bit and 32-bit
						 *      because only the DOS callback and RETF instructions are involved,
						 *      which can be executed as either 16-bit or 32-bit code without problems. */
						reg_ax = INT15_apm_pmentry >> 16;	// AX = 32-bit code segment (real mode base)
						reg_ebx = INT15_apm_pmentry & 0xFFFF;	// EBX = offset of entry point
						reg_cx = INT15_apm_pmentry >> 16;	// CX = 16-bit code segment (real mode base)
						reg_dx = INT15_apm_pmentry >> 16;	// DX = data segment (real mode base) (?? what size?)
						reg_esi = 0xFFFFFFFF;			// ESI = upper word: 16-bit code segment len  lower word: 32-bit code segment length
						reg_di = 0xFFFF;			// DI = data segment length
						APMBIOS_connect_mode = APMBIOS_CONNECT_PROT32;
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
						reg_ah = 0x09;	// unrecognized device ID
						CALLBACK_SCF(true);			
						break;
					}
					if(apm_realmode_connected) {
						LOG_MSG("APM BIOS: OS disconnected\n");
						CALLBACK_SCF(false);
						apm_realmode_connected=false;
					} else {
						reg_ah = 0x03;	// interface not connected
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
					// TODO: Make this a dosbox.conf configuration option: what do we do
					//       on APM idle calls? Allow selection between "nothing" "hlt"
					//       and "software delay".
					if (!(reg_flags&0x200)) {
						LOG_MSG("APM BIOS warning: CPU IDLE called with IF=0, not HLTing\n");
					}
					else if (cpudecoder == &HLT_Decode) { /* do not re-execute HLT, it makes DOSBox hang */
						LOG_MSG("APM BIOS warning: CPU IDLE HLT within HLT (DOSBox core failure)\n");
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
				case 0x0a: // GET POWER STATUS
					if (!apm_realmode_connected) {
						reg_ah = 0x03;	// interface not connected
						CALLBACK_SCF(true);
						break;
					}
					if (reg_bx != 0x0001 && reg_bx != 0x8001) {
						reg_ah = 0x09;	// unrecognized device ID
						CALLBACK_SCF(true);			
						break;
					}
					/* FIXME: Allow configuration and shell commands to dictate whether or
					 *        not we emulate a laptop with a battery */
					reg_bh = 0x01;		// AC line status (1=on-line)
					reg_bl = 0xFF;		// Battery status (unknown)
					reg_ch = 0x80;		// Battery flag (no system battery)
					reg_cl = 0xFF;		// Remaining battery charge (unknown)
					reg_dx = 0xFFFF;	// Remaining battery life (unknown)
					reg_si = 0;		// Number of battery units (if called with reg_bx == 0x8001)
					CALLBACK_SCF(false);
					break;
				case 0x0b: // GET PM EVENT
					if (!apm_realmode_connected) {
						reg_ah = 0x03;	// interface not connected
						CALLBACK_SCF(true);
						break;
					}
					reg_ah = 0x80; // no power management events pending
					CALLBACK_SCF(true);
					break;
				case 0x0d:
					// NTS: NOT implementing this call can cause Windows 98's APM driver to crash on startup
					if(reg_bx != 0x0 && reg_bx != 0x1) {
						reg_ah = 0x09;	// unrecognized device ID
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
							reg_ah = 0x09;	// unrecognized device ID
							CALLBACK_SCF(true);
							break;
						} else if(!apm_realmode_connected) {
							reg_ah = 0x03;	// interface not connected
							CALLBACK_SCF(true);
							break;
						}
						reg_ah = reg_ch; /* we are called with desired version in CH,CL, return actual version in AH,AL */
						reg_al = reg_cl;
						if(reg_ah != 1) reg_ah = 1;						// major
						if(reg_al > APM_BIOS_minor_version) reg_al = APM_BIOS_minor_version;	// minor
						APM_BIOS_connected_minor_version = reg_al;				// what we decided becomes the interface we emulate
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
						reg_ah = 0x09;	// unrecognized device ID
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
						reg_ah = 0x03;	// interface not connected
						CALLBACK_SCF(true);
						break;
					}
					if (reg_bx != 0) {
						reg_ah = 0x09;	// unrecognized device ID
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
						reg_ah = 0x03;	// interface not connected
						CALLBACK_SCF(true);
						break;
					}
					if (reg_bx != 0) {
						reg_ah = 0x09;	// unrecognized device ID
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
						reg_ah = 0x03;	// interface not connected
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
				LOG(LOG_BIOS,LOG_ERROR)("INT15:Unknown call ah=E8, al=%2X",reg_al);
				reg_ah=0x86;
				CALLBACK_SCF(true);
				if ((IS_EGAVGA_ARCH) || (machine==MCH_CGA) || (machine==MCH_AMSTRAD)) {
					/* relict from comparisons, as int15 exits with a retf2 instead of an iret */
					CALLBACK_SZF(false);
				}
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

void restart_program(std::vector<std::string> & parameters);

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
	LOG_MSG("Restart by INT 18h requested\n");
	On_Software_CPU_Reset();
	/* does not return */
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
}

unsigned char do_isapnp_chksum(unsigned char *d,int i) {
	unsigned char sum = 0;

	while (i-- > 0)
		sum += *d++;

	return (0x100 - sum) & 0xFF;
}

void MEM_ResetPageHandler_Unmapped(Bitu phys_page, Bitu pages);

unsigned int dos_conventional_limit = 0;

bool AdapterROM_Read(Bitu address,unsigned long *size) {
	unsigned char chksum=0;
	unsigned char c[3];
	unsigned int i;

	if ((address & 0x1FF) != 0) {
		LOG(LOG_MISC,LOG_DEBUG)("AdapterROM_Read: Caller attempted ROM scan not aligned to 512-byte boundary");
		return false;
	}

	for (i=0;i < 3;i++)
		c[i] = mem_readb(address+i);

	if (c[0] == 0x55 && c[1] == 0xAA) {
		*size = (unsigned long)c[2] * 512UL;
		for (i=0;i < (unsigned int)(*size);i++) chksum += mem_readb(address+i);
		if (chksum != 0) {
			LOG(LOG_MISC,LOG_WARN)("AdapterROM_Read: Found ROM at 0x%lx but checksum failed\n",(unsigned long)address);
			return false;
		}

		return true;
	}

	return false;
}

#include "src/gui/dosbox.vga16.bmp.h"
#include "src/gui/dosbox.cga640.bmp.h"

void DrawDOSBoxLogoCGA6(unsigned int x,unsigned int y) {
	unsigned char *s = dosbox_cga640_bmp;
	unsigned char *sf = s + sizeof(dosbox_cga640_bmp);
	uint32_t width,height;
	unsigned int dx,dy;
	uint32_t vram;
	uint32_t off;
	uint32_t sz;

	if (memcmp(s,"BM",2)) return;
	sz = host_readd(s+2); // size of total bitmap
	off = host_readd(s+10); // offset of bitmap
	if ((s+sz) > sf) return;
	if ((s+14+40) > sf) return;

	sz = host_readd(s+34); // biSize
	if ((s+off+sz) > sf) return;
	if (host_readw(s+26) != 1) return; // biBitPlanes
	if (host_readw(s+28) != 1)  return; // biBitCount

	width = host_readd(s+18);
	height = host_readd(s+22);
	if (width > (640-x) || height > (200-y)) return;

	LOG(LOG_MISC,LOG_DEBUG)("Drawing CGA logo (%u x %u)",(int)width,(int)height);
	for (dy=0;dy < height;dy++) {
		vram  = ((y+dy) >> 1) * 80;
		vram += ((y+dy) & 1) * 0x2000;
		vram += (x / 8);
		s = dosbox_cga640_bmp + off + ((height-(dy+1))*((width+7)/8));
		for (dx=0;dx < width;dx += 8) {
			mem_writeb(0xB8000+vram,*s);
			vram++;
			s++;
		}
	}
}

void DrawDOSBoxLogoVGA(unsigned int x,unsigned int y) {
	unsigned char *s = dosbox_vga16_bmp;
	unsigned char *sf = s + sizeof(dosbox_vga16_bmp);
	unsigned int bit,dx,dy;
	uint32_t width,height;
	uint32_t vram;
	uint32_t off;
	uint32_t sz;

	if (memcmp(s,"BM",2)) return;
	sz = host_readd(s+2); // size of total bitmap
	off = host_readd(s+10); // offset of bitmap
	if ((s+sz) > sf) return;
	if ((s+14+40) > sf) return;

	sz = host_readd(s+34); // biSize
	if ((s+off+sz) > sf) return;
	if (host_readw(s+26) != 1) return; // biBitPlanes
	if (host_readw(s+28) != 4)  return; // biBitCount

	width = host_readd(s+18);
	height = host_readd(s+22);
	if (width > (640-x) || height > (350-y)) return;

	// EGA/VGA Write Mode 2
	LOG(LOG_MISC,LOG_DEBUG)("Drawing VGA logo (%u x %u)",(int)width,(int)height);
	IO_Write(0x3CE,0x05); // graphics mode
	IO_Write(0x3CF,0x02); // read=0 write=2 odd/even=0 shift=0 shift256=0
	IO_Write(0x3CE,0x03); // data rotate
	IO_Write(0x3CE,0x00); // no rotate, no XOP
	for (bit=0;bit < 8;bit++) {
		const unsigned char shf = ((bit & 1) ^ 1) * 4;

		IO_Write(0x3CE,0x08); // bit mask
		IO_Write(0x3CF,0x80 >> bit);

		for (dy=0;dy < height;dy++) {
			vram = ((y+dy) * 80) + (x / 8);
			s = dosbox_vga16_bmp + off + (bit/2) + ((height-(dy+1))*((width+1)/2));
			for (dx=bit;dx < width;dx += 8) {
				mem_readb(0xA0000+vram); // load VGA latches
				mem_writeb(0xA0000+vram,(*s >> shf) & 0xF);
				vram++;
				s += 4;
			}
		}
	}
	// restore write mode 0
	IO_Write(0x3CE,0x05); // graphics mode
	IO_Write(0x3CF,0x00); // read=0 write=0 odd/even=0 shift=0 shift256=0
	IO_Write(0x3CE,0x08); // bit mask
	IO_Write(0x3CF,0xFF);
}

static void BIOS_Int10RightJustifiedPrint(const int x,int &y,const char *msg) {
	const char *s = msg;
	while (*s != 0) {
		if (*s == '\n') {
			y++;
			reg_eax = 0x0200;	// set cursor pos
			reg_ebx = 0;		// page zero
			reg_dh = y;		// row 4
			reg_dl = x;		// column 20
			CALLBACK_RunRealInt(0x10);
			s++;
		}
		else {
			reg_eax = 0x0E00 | ((unsigned char)(*s++));
			reg_ebx = 0x07;
			CALLBACK_RunRealInt(0x10);
		}
	}
}

static Bitu ulimit = 0;
static Bitu t_conv = 0;
static bool bios_first_init=true;
static bool bios_has_exec_vga_bios=false;
static Bitu adapter_scan_start;

/* FIXME: At global scope their destructors are called after the rest of DOSBox has shut down. Move back into BIOS scope. */
static CALLBACK_HandlerObject int4b_callback;
static CALLBACK_HandlerObject callback[16]; /* <- fixme: this is stupid. just declare one per interrupt. */
static CALLBACK_HandlerObject cb_bios_post;

Bitu call_pnp_r = ~0UL;
Bitu call_pnp_rp = 0;

Bitu call_pnp_p = ~0UL;
Bitu call_pnp_pp = 0;

Bitu isapnp_biosstruct_base = 0;

Bitu BIOS_boot_code_offset = 0;

void BIOS_OnResetComplete(Section *x);

class BIOS:public Module_base{
private:
	static Bitu cb_bios_post__func(void) {
		void TIMER_BIOS_INIT_Configure();

		if (cpu.pmode) E_Exit("BIOS error: POST function called while in protected/vm86 mode");

		/* we need A20 enabled for BIOS boot-up */
		void A20Gate_OverrideOn(Section *sec);
		void MEM_A20_Enable(bool enabled);
		A20Gate_OverrideOn(NULL);
		MEM_A20_Enable(true);

        BIOS_OnResetComplete(NULL);

		adapter_scan_start = 0xC0000;
		bios_has_exec_vga_bios = false;
		LOG(LOG_MISC,LOG_DEBUG)("BIOS: executing POST routine");

		// TODO: Anything we can test in the CPU here?

		// initialize registers
		SegSet16(ds,0x0000);
		SegSet16(es,0x0000);
		SegSet16(fs,0x0000);
		SegSet16(gs,0x0000);
		SegSet16(ss,0x0000);

		{
			Bitu sz = MEM_TotalPages();

			/* The standard BIOS is said to put it's stack (at least at OS boot time) 512 bytes past the end of the boot sector
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

		if (bios_first_init) {
			/* clear the first 1KB-32KB */
			for (Bit16u i=0x400;i<0x8000;i++) real_writeb(0x0,i,0);
		}

		extern Bitu call_default,call_default2;

		/* Clear the vector table */
		for (Bit16u i=0x70*4;i<0x400;i++) real_writeb(0x00,i,0);

		/* Only setup default handler for first part of interrupt table */
		for (Bit16u ct=0;ct<0x60;ct++) {
			real_writed(0,ct*4,CALLBACK_RealPointer(call_default));
		}
		for (Bit16u ct=0x68;ct<0x70;ct++) {
			real_writed(0,ct*4,CALLBACK_RealPointer(call_default));
		}

		// default handler for IRQ 2-7
		for (Bit16u ct=0x0A;ct <= 0x0F;ct++)
			RealSetVec(ct,BIOS_DEFAULT_IRQ07_DEF_LOCATION);

		// default handler for IRQ 8-15
		for (Bit16u ct=0x70;ct <= 0x77;ct++)
			RealSetVec(ct,BIOS_DEFAULT_IRQ815_DEF_LOCATION);

		// setup a few interrupt handlers that point to bios IRETs by default
		real_writed(0,0x0e*4,CALLBACK_RealPointer(call_default2));	//design your own railroad
		real_writed(0,0x66*4,CALLBACK_RealPointer(call_default));	//war2d
		real_writed(0,0x67*4,CALLBACK_RealPointer(call_default));
		real_writed(0,0x68*4,CALLBACK_RealPointer(call_default));
		real_writed(0,0x5c*4,CALLBACK_RealPointer(call_default));	//Network stuff
		//real_writed(0,0xf*4,0); some games don't like it

		bios_first_init = false;

		DispatchVMEvent(VM_EVENT_BIOS_INIT);

		TIMER_BIOS_INIT_Configure();

		void INT10_Startup(Section *sec);
		INT10_Startup(NULL);

		extern Bit8u BIOS_tandy_D4_flag;
		real_writeb(0x40,0xd4,BIOS_tandy_D4_flag);

		bool use_tandyDAC=(real_readb(0x40,0xd4)==0xff);

		/* INT 13 Bios Disk Support */
		BIOS_SetupDisks();

		/* INT 16 Keyboard handled in another file */
		BIOS_SetupKeyboard();

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

		mem_writew(BIOS_MEMORY_SIZE,t_conv);

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

		/* Some hardcoded vectors */
		phys_writeb(Real2Phys(BIOS_DEFAULT_HANDLER_LOCATION),0xcf);	/* bios default interrupt vector location -> IRET */
		phys_writew(Real2Phys(RealGetVec(0x12))+0x12,0x20); //Hack for Jurresic

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

		// Disney workaround
//		Bit16u disney_port = mem_readw(BIOS_ADDRESS_LPT1);
		// port timeouts
		// always 1 second even if the port does not exist
//		BIOS_SetLPTPort(0, disney_port);
		for(Bitu i = 1; i < 3; i++) BIOS_SetLPTPort(i, 0);
		mem_writeb(BIOS_COM1_TIMEOUT,1);
		mem_writeb(BIOS_COM2_TIMEOUT,1);
		mem_writeb(BIOS_COM3_TIMEOUT,1);
		mem_writeb(BIOS_COM4_TIMEOUT,1);

		void BIOS_Post_register_parports();
		BIOS_Post_register_parports();

		void BIOS_Post_register_comports();
		BIOS_Post_register_comports();

		/* Setup equipment list */
		// look http://www.bioscentral.com/misc/bda.htm
		
		//Bit16u config=0x4400;	//1 Floppy, 2 serial and 1 parallel 
		Bit16u config = 0x0;

		Bitu bios_post_parport_count();
		config |= bios_post_parport_count() << 14;

		Bitu bios_post_comport_count();
		config |= bios_post_comport_count() << 9;
		
#if (C_FPU)
		extern bool enable_fpu;

		//FPU
		if (enable_fpu)
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

		// PS2 mouse
		bool KEYBOARD_Report_BIOS_PS2Mouse();
		if (KEYBOARD_Report_BIOS_PS2Mouse())
			config |= 0x04;

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

		/* PS/2 mouse */
		void BIOS_PS2Mouse_Startup(Section *sec);
		BIOS_PS2Mouse_Startup(NULL);

		/* this belongs HERE not on-demand from INT 15h! */
		biosConfigSeg = ROMBIOS_GetMemory(16/*one paragraph*/,"BIOS configuration (INT 15h AH=0xC0)",/*paragraph align*/16)>>4;
		if (biosConfigSeg != 0) {
			PhysPt data = PhysMake(biosConfigSeg,0);
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
				if (PS1AudioCard) { /* FIXME: Won't work because BIOS_Init() comes before PS1SOUND_Init() */
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
		}

		// ISA Plug & Play I/O ports
		if (1) {
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
                E_Exit("Failed to get dosbox integration IO callout handle");

            {
                IO_CalloutObject *obj = IO_GetCallout(dosbox_int_iocallout);
                if (obj == NULL) E_Exit("Failed to get dosbox integration IO callout");
                obj->Install(0x28,IOMASK_Combine(IOMASK_FULL,IOMASK_Range(4)),dosbox_integration_cb_port_r,dosbox_integration_cb_port_w);
                IO_PutCallout(obj);
            }

            /* DOSBox integration device */
            if (isapnpigdevice == NULL) {
                isapnpigdevice = new ISAPnPIntegrationDevice;
                ISA_PNP_devreg(isapnpigdevice);
            }
		}

		// ISA Plug & Play BIOS entrypoint
        // NTS: Apparently, Windows 95, 98, and ME will re-enumerate and re-install PnP devices if our entry point changes it's address.
		if (ISAPNPBIOS) {
			int i;
			Bitu base;
			unsigned char c,tmp[256];

			if (mainline_compatible_bios_mapping)
				isapnp_biosstruct_base = base = 0xFE100; /* take the unused space just after the fake BIOS signature */
			else
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
			phys_writeb(base+4,0x10);		/* Version:		1.0 */
			phys_writeb(base+5,0x21);		/* Length:		0x21 bytes */
			phys_writew(base+6,0x0000);		/* Control field:	Event notification not supported */
			/* skip checksum atm */
			phys_writed(base+9,0);			/* Event notify flag addr: (none) */
			phys_writed(base+0xD,call_pnp_rp);	/* Real-mode entry point */
			phys_writew(base+0x11,call_pnp_pp&0xFFFF); /* Protected mode offset */
			phys_writed(base+0x13,(call_pnp_pp >> 12) & 0xFFFF0); /* Protected mode code segment base */
			phys_writed(base+0x17,ISAPNP_ID('D','O','S',0,7,4,0));		/* OEM device identifier (DOSBox 0.740, get it?) */
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
					LOG_MSG("ISAPNP register failed\n");
			}

			/* register parallel ports */
			for (Bitu portn=0;portn < 3;portn++) {
				Bitu port = mem_readw(BIOS_ADDRESS_LPT1+(portn*2));
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

		return CBRET_NONE;
	}
	CALLBACK_HandlerObject cb_bios_scan_video_bios;
	static Bitu cb_bios_scan_video_bios__func(void) {
		unsigned long size;

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
		Bit32u c1;

		if (cpu.pmode) E_Exit("BIOS error: ADAPTER ROM function called while in protected/vm86 mode");

		while (adapter_scan_start < 0xF0000) {
			if (AdapterROM_Read(adapter_scan_start,&size)) {
				Bit16u segm = (Bit16u)(adapter_scan_start >> 4);

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
		const char *msg = PACKAGE_STRING " (C) 2002-2017 The DOSBox Team\nA fork of DOSBox 0.74 by TheGreatCodeholio\nFor more info visit http://dosbox-x.com\nBased on DOSBox (http://dosbox.com)\n\n";
		int logo_x,logo_y,x,y,rowheight=8;

		y = 2;
		x = 2;
		logo_y = 2;
		logo_x = 80 - 2 - (224/8);

		if (cpu.pmode) E_Exit("BIOS error: STARTUP function called while in protected/vm86 mode");

		// TODO: For those who would rather not use the VGA graphical modes, add a configuration option to "disable graphical splash".
		//       We would then revert to a plain text copyright and status message here (and maybe an ASCII art version of the DOSBox logo).
		if (IS_VGA_ARCH) {
			rowheight = 16;
			reg_eax = 18;		// 640x480 16-color
			CALLBACK_RunRealInt(0x10);
			DrawDOSBoxLogoVGA(logo_x*8,logo_y*rowheight);
		}
		else if (machine == MCH_EGA) {
			rowheight = 14;
			reg_eax = 16;		// 640x350 16-color
			CALLBACK_RunRealInt(0x10);

			// color correction: change Dark Puke Yellow to brown
			IO_Read(0x3DA); IO_Read(0x3BA);
			IO_Write(0x3C0,0x06);
			IO_Write(0x3C0,0x14); // red=1 green=1 blue=0
			IO_Read(0x3DA); IO_Read(0x3BA);
			IO_Write(0x3C0,0x20);

			DrawDOSBoxLogoVGA(logo_x*8,logo_y*rowheight);
		}
		else if (machine == MCH_CGA || machine == MCH_PCJR || machine == MCH_AMSTRAD || machine == MCH_TANDY) {
			rowheight = 8;
			reg_eax = 6;		// 640x200 2-color
			CALLBACK_RunRealInt(0x10);

			DrawDOSBoxLogoCGA6(logo_x*8,logo_y*rowheight);
		}
		else {
			reg_eax = 3;		// 80x25 text
			CALLBACK_RunRealInt(0x10);

			// TODO: For CGA, PCjr, and Tandy, we could render a 4-color CGA version of the same logo.
			//       And for MDA/Hercules, we could render a monochromatic ASCII art version.
		}

		{
			reg_eax = 0x0200;	// set cursor pos
			reg_ebx = 0;		// page zero
			reg_dh = y;		// row 4
			reg_dl = x;		// column 20
			CALLBACK_RunRealInt(0x10);
		}

		BIOS_Int10RightJustifiedPrint(x,y,msg);

		{
			uint64_t sz = (uint64_t)MEM_TotalPages() * (uint64_t)4096;
			char tmp[512];

			if (sz >= ((uint64_t)128 << (uint64_t)20))
				sprintf(tmp,"%uMB memory installed\r\n",(unsigned int)(sz >> (uint64_t)20));
			else
				sprintf(tmp,"%uKB memory installed\r\n",(unsigned int)(sz >> (uint64_t)10));

			BIOS_Int10RightJustifiedPrint(x,y,tmp);
		}

		{
			char tmp[512];
			const char *card = "?";

			switch (machine) {
				case MCH_CGA:
					card = "IBM Color Graphics Adapter";
					break;
				case MCH_HERC:
					card = "IBM Monochrome Display Adapter (Hercules)";
					break;
				case MCH_EGA:
					card = "IBM Enhanced Graphics Adapter";
					break;
				case MCH_PCJR:
					card = "PCjr graohics adapter";
					break;
				case MCH_TANDY:
					card = "Tandy graohics adapter";
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
			};

			sprintf(tmp,"Video card is %s\n",card);
			BIOS_Int10RightJustifiedPrint(x,y,tmp);
		}

		{
			char tmp[512];
			const char *cpu = "?";

			switch (CPU_ArchitectureType) {
				case CPU_ARCHTYPE_8086:
					cpu = "8086";
					break;
				case CPU_ARCHTYPE_80186:
					cpu = "80186";
					break;
				case CPU_ARCHTYPE_286:
					cpu = "286";
					break;
				case CPU_ARCHTYPE_386:
					cpu = "386";
					break;
				case CPU_ARCHTYPE_486OLD:
					cpu = "486 (older generation)";
					break;
				case CPU_ARCHTYPE_486NEW:
					cpu = "486 (later generation)";
					break;
				case CPU_ARCHTYPE_PENTIUM:
					cpu = "Pentium";
					break;
				case CPU_ARCHTYPE_P55CSLOW:
					cpu = "Pentium MMX";
					break;
			};

			extern bool enable_fpu;

			sprintf(tmp,"%s CPU present",cpu);
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

		BIOS_Int10RightJustifiedPrint(x,y,"\nHit SPACEBAR to pause at this screen\n");
		y--; /* next message should overprint */
		{
			reg_eax = 0x0200;	// set cursor pos
			reg_ebx = 0;		// page zero
			reg_dh = y;		// row 4
			reg_dl = x;		// column 20
			CALLBACK_RunRealInt(0x10);
		}

		// TODO: Then at this screen, we can print messages demonstrating the detection of
		//       IDE devices, floppy, ISA PnP initialization, anything of importance.
		//       I also envision adding the ability to hit DEL or F2 at this point to enter
		//       a "BIOS setup" screen where all DOSBox configuration options can be
		//       modified, with the same look and feel of an old BIOS.

		bool wait_for_user = false;
		Bit32u lasttick=GetTicks();
		while ((GetTicks()-lasttick)<1000) {
			reg_eax = 0x0100;
			CALLBACK_RunRealInt(0x16);

			if (!GETFLAG(ZF)) {
				reg_eax = 0x0000;
				CALLBACK_RunRealInt(0x16);

				if (reg_al == 32) { // user hit space
					BIOS_Int10RightJustifiedPrint(x,y,"Hit ENTER or ESC to continue                    \n"); // overprint
					wait_for_user = true;
					break;
				}
			}
		}

        while (wait_for_user) {
            reg_eax = 0x0000;
            CALLBACK_RunRealInt(0x16);

            if (reg_al == 27/*ESC*/ || reg_al == 13/*ENTER*/)
                break;
        }

		// restore 80x25 text mode
		reg_eax = 3;
		CALLBACK_RunRealInt(0x10);

		return CBRET_NONE;
	}
	CALLBACK_HandlerObject cb_bios_boot;
	static Bitu cb_bios_boot__func(void) {
		/* Reset/power-on overrides the user's A20 gate preferences.
		 * It's time to revert back to what the user wants. */
		void A20Gate_TakeUserSetting(Section *sec);
		void MEM_A20_Enable(bool enabled);
		A20Gate_TakeUserSetting(NULL);
		MEM_A20_Enable(false);

		if (cpu.pmode) E_Exit("BIOS error: BOOT function called while in protected/vm86 mode");
		DispatchVMEvent(VM_EVENT_BIOS_BOOT);

        /* if we're supposed to run in PC-98 mode, then do it NOW.
         * sdlmain.cpp will come back around when it's made the change to this call. */
        if (enable_pc98_jump)
            throw int(5);

		// TODO: If instructed to, follow the INT 19h boot pattern, perhaps follow the BIOS Boot Specification, etc.

		// TODO: If instructed to boot a guest OS...

		// Begin booting the DOSBox shell. NOTE: VM_Boot_DOSBox_Kernel will change CS:IP instruction pointer!
		if (!VM_Boot_DOSBox_Kernel()) E_Exit("BIOS error: BOOT function failed to boot DOSBox kernel");
		return CBRET_NONE;
	}
public:
	void write_FFFF_signature() {
		/* write the signature at 0xF000:0xFFF0 */

		// The farjump at the processor reset entry point (jumps to POST routine)
		phys_writeb(0xffff0,0xEA);					// FARJMP
		phys_writew(0xffff1,RealOff(BIOS_DEFAULT_RESET_LOCATION));	// offset
		phys_writew(0xffff3,RealSeg(BIOS_DEFAULT_RESET_LOCATION));	// segment

		// write system BIOS date
		for(Bitu i = 0; i < strlen(bios_date_string); i++) phys_writeb(0xffff5+i,bios_date_string[i]);

		/* model byte */
		if (machine==MCH_TANDY || machine==MCH_AMSTRAD) phys_writeb(0xffffe,0xff);	/* Tandy model */
		else if (machine==MCH_PCJR) phys_writeb(0xffffe,0xfd);	/* PCJr model */
		else phys_writeb(0xffffe,0xfc);	/* PC */

		// signature
		phys_writeb(0xfffff,0x55);
	}
    void write_FFFF_PC98_signature() {
        /* this may overwrite the existing signature.
         * PC-98 systems DO NOT have an ASCII date at F000:FFF5
         * and the WORD value at F000:FFFE is said to be a checksum of the BIOS */
 
		// The farjump at the processor reset entry point (jumps to POST routine)
		phys_writeb(0xffff0,0xEA);					// FARJMP
		phys_writew(0xffff1,RealOff(BIOS_DEFAULT_RESET_LOCATION));	// offset
		phys_writew(0xffff3,RealSeg(BIOS_DEFAULT_RESET_LOCATION));	// segment

		// write nothing (not used)
		for(Bitu i = 0; i < 9; i++) phys_writeb(0xffff5+i,0);

        // fake BIOS checksum
        phys_writew(0xffffe,0xABCD);
    }
	BIOS(Section* configuration):Module_base(configuration){
		/* tandy DAC can be requested in tandy_sound.cpp by initializing this field */
		Bitu wo;

        isapnp_biosstruct_base = 0;

		{ // TODO: Eventually, move this to BIOS POST or init phase
			Section_prop * section=static_cast<Section_prop *>(control->GetSection("dosbox"));

			bochs_port_e9 = section->Get_bool("bochs debug port e9");

			// TODO: motherboard init, especially when we get around to full Intel Triton/i440FX chipset emulation
			isa_memory_hole_512kb = section->Get_bool("isa memory hole at 512kb");

			// FIXME: Erm, well this couldv'e been named better. It refers to the amount of conventional memory
			//        made available to the operating system below 1MB, which is usually DOS.
			dos_conventional_limit = section->Get_int("dos mem limit");
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

		/* pick locations */
		if (mainline_compatible_bios_mapping) { /* mapping BIOS the way mainline DOSBox does */
			BIOS_DEFAULT_RESET_LOCATION = RealMake(0xf000,0xe05b);
			BIOS_DEFAULT_HANDLER_LOCATION = RealMake(0xf000,0xff53);
			BIOS_DEFAULT_IRQ0_LOCATION = RealMake(0xf000,0xfea5);
			BIOS_DEFAULT_IRQ1_LOCATION = RealMake(0xf000,0xe987);
			BIOS_DEFAULT_IRQ07_DEF_LOCATION = RealMake(0xf000,0xff55);
			BIOS_DEFAULT_IRQ815_DEF_LOCATION = RealMake(0xf000,0xe880);
		}
		else {
			BIOS_DEFAULT_RESET_LOCATION = PhysToReal416(ROMBIOS_GetMemory(64/*several callbacks*/,"BIOS default reset location",/*align*/4));
			BIOS_DEFAULT_HANDLER_LOCATION = PhysToReal416(ROMBIOS_GetMemory(1/*IRET*/,"BIOS default handler location",/*align*/4));
			BIOS_DEFAULT_IRQ0_LOCATION = PhysToReal416(ROMBIOS_GetMemory(0x13/*see callback.cpp for IRQ0*/,"BIOS default IRQ0 location",/*align*/4));
			BIOS_DEFAULT_IRQ1_LOCATION = PhysToReal416(ROMBIOS_GetMemory(0x15/*see callback.cpp for IRQ1*/,"BIOS default IRQ1 location",/*align*/4));
			BIOS_DEFAULT_IRQ07_DEF_LOCATION = PhysToReal416(ROMBIOS_GetMemory(7/*see callback.cpp for EOI_PIC1*/,"BIOS default IRQ2-7 location",/*align*/4));
			BIOS_DEFAULT_IRQ815_DEF_LOCATION = PhysToReal416(ROMBIOS_GetMemory(9/*see callback.cpp for EOI_PIC1*/,"BIOS default IRQ8-15 location",/*align*/4));
		}

		write_FFFF_signature();

		/* Setup all the interrupt handlers the bios controls */

		/* INT 8 Clock IRQ Handler */
		call_irq0=CALLBACK_Allocate();	
		CALLBACK_Setup(call_irq0,INT8_Handler,CB_IRQ0,Real2Phys(BIOS_DEFAULT_IRQ0_LOCATION),"IRQ 0 Clock");

		/* INT 11 Get equipment list */
		callback[1].Install(&INT11_Handler,CB_IRET,"Int 11 Equipment");

		/* INT 12 Memory Size default at 640 kb */
		callback[2].Install(&INT12_Handler,CB_IRET,"Int 12 Memory");

		ulimit = 640;
		t_conv = MEM_TotalPages() << 2; /* convert 4096/byte pages -> 1024/byte KB units */
		if (allow_more_than_640kb) {
			if (machine == MCH_CGA)
				ulimit = 736;		/* 640KB + 64KB + 32KB  0x00000-0xB7FFF */
			else if (machine == MCH_HERC)
				ulimit = 704;		/* 640KB + 64KB = 0x00000-0xAFFFF */

			if (t_conv > ulimit) t_conv = ulimit;
			if (t_conv > 640) { /* because the memory emulation has already set things up */
				bool MEM_map_RAM_physmem(Bitu start,Bitu end);
				MEM_map_RAM_physmem(0xA0000,(t_conv<<10)-1);
				memset(GetMemBase()+(640<<10),0,(t_conv-640)<<10);
			}
		}
		else {
			if (t_conv > 640) t_conv = 640;
		}

		if (IS_TANDY_ARCH) {
			/* reduce reported memory size for the Tandy (32k graphics memory
			   at the end of the conventional 640k) */
			if (machine==MCH_TANDY && t_conv > 624) t_conv = 624;
		}

		/* allow user to further limit the available memory below 1MB */
		if (dos_conventional_limit != 0 && t_conv > dos_conventional_limit)
			t_conv = dos_conventional_limit;

		// TODO: Allow dosbox.conf to specify an option to add an EBDA (Extended BIOS Data Area)
		//       at the top of the DOS conventional limit, which we then reduce further to hold
		//       it. Most BIOSes past 1992 or so allocate an EBDA.

		/* if requested to emulate an ISA memory hole at 512KB, further limit the memory */
		if (isa_memory_hole_512kb && t_conv > 512) t_conv = 512;

		/* and then unmap RAM between t_conv and ulimit */
		if (t_conv < ulimit) {
			Bitu start = (t_conv+3)/4;	/* start = 1KB to page round up */
			Bitu end = ulimit/4;		/* end = 1KB to page round down */
			if (start < end) MEM_ResetPageHandler_Unmapped(start,end-start);
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

		init_vm86_fake_io();

		/* Irq 2-7 */
		call_irq07default=CALLBACK_Allocate();
		CALLBACK_Setup(call_irq07default,NULL,CB_IRET_EOI_PIC1,Real2Phys(BIOS_DEFAULT_IRQ07_DEF_LOCATION),"bios irq 0-7 default handler");

		/* Irq 8-15 */
		call_irq815default=CALLBACK_Allocate();
		CALLBACK_Setup(call_irq815default,NULL,CB_IRET_EOI_PIC2,Real2Phys(BIOS_DEFAULT_IRQ815_DEF_LOCATION),"bios irq 8-15 default handler");

		/* BIOS boot stages */
		cb_bios_post.Install(&cb_bios_post__func,CB_RETF,"BIOS POST");
		cb_bios_scan_video_bios.Install(&cb_bios_scan_video_bios__func,CB_RETF,"BIOS Scan Video BIOS");
		cb_bios_adapter_rom_scan.Install(&cb_bios_adapter_rom_scan__func,CB_RETF,"BIOS Adapter ROM scan");
		cb_bios_startup_screen.Install(&cb_bios_startup_screen__func,CB_RETF,"BIOS Startup screen");
		cb_bios_boot.Install(&cb_bios_boot__func,CB_RETF,"BIOS BOOT");

		// Compatible POST routine location: jump to the callback
		{
			Bitu wo_fence;

			wo = Real2Phys(BIOS_DEFAULT_RESET_LOCATION);
			wo_fence = wo + 64;

			// POST
			phys_writeb(wo+0x00,(Bit8u)0xFE);						//GRP 4
			phys_writeb(wo+0x01,(Bit8u)0x38);						//Extra Callback instruction
			phys_writew(wo+0x02,(Bit16u)cb_bios_post.Get_callback());			//The immediate word
			wo += 4;

			// video bios scan
			phys_writeb(wo+0x00,(Bit8u)0xFE);						//GRP 4
			phys_writeb(wo+0x01,(Bit8u)0x38);						//Extra Callback instruction
			phys_writew(wo+0x02,(Bit16u)cb_bios_scan_video_bios.Get_callback());		//The immediate word
			wo += 4;

			// adapter ROM scan
			phys_writeb(wo+0x00,(Bit8u)0xFE);						//GRP 4
			phys_writeb(wo+0x01,(Bit8u)0x38);						//Extra Callback instruction
			phys_writew(wo+0x02,(Bit16u)cb_bios_adapter_rom_scan.Get_callback());		//The immediate word
			wo += 4;

			// startup screen
			phys_writeb(wo+0x00,(Bit8u)0xFE);						//GRP 4
			phys_writeb(wo+0x01,(Bit8u)0x38);						//Extra Callback instruction
			phys_writew(wo+0x02,(Bit16u)cb_bios_startup_screen.Get_callback());		//The immediate word
			wo += 4;

			// boot
            BIOS_boot_code_offset = wo;
			phys_writeb(wo+0x00,(Bit8u)0xFE);						//GRP 4
			phys_writeb(wo+0x01,(Bit8u)0x38);						//Extra Callback instruction
			phys_writew(wo+0x02,(Bit16u)cb_bios_boot.Get_callback());			//The immediate word
			wo += 4;

			/* fence */
			phys_writeb(wo++,0xEB);								// JMP $-2
			phys_writeb(wo++,0xFE);

			if (wo > wo_fence) E_Exit("BIOS boot callback overrun");
		}
	}
	~BIOS(){
		/* snap the CPU back to real mode. this code thinks in terms of 16-bit real mode
		 * and if allowed to do it's thing in a 32-bit guest OS like WinNT, will trigger
		 * a page fault. */
		CPU_Snap_Back_To_Real_Mode();

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

		/* encourage the callback objects to uninstall HERE while we're in real mode, NOT during the
		 * destructor stage where we're back in protected mode */
		for (unsigned int i=0;i < 13;i++) callback[i].Uninstall();

		/* done */
		CPU_Snap_Back_Restore();
	}
    /* PC-98 change code */
    void rewrite_IRQ_handlers(void) {
        CALLBACK_Setup(call_irq0,INT8_Handler,CB_IRET_EOI_PIC1,Real2Phys(BIOS_DEFAULT_IRQ0_LOCATION),"IRQ 0 Clock");
        CALLBACK_Setup(call_irq07default,NULL,CB_IRET_EOI_PIC1,Real2Phys(BIOS_DEFAULT_IRQ07_DEF_LOCATION),"bios irq 0-7 default handler");
        CALLBACK_Setup(call_irq815default,NULL,CB_IRET_EOI_PIC2,Real2Phys(BIOS_DEFAULT_IRQ815_DEF_LOCATION),"bios irq 8-15 default handler");

        BIOS_UnsetupKeyboard();
        BIOS_UnsetupDisks();

        /* no such INT 4Bh */
		int4b_callback.Uninstall();
        RealSetVec(0x4B,0);

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

		callback[1].Uninstall(); /* INT 11h */
        RealSetVec(0x11,0);

		callback[2].Uninstall(); /* INT 12h */
        RealSetVec(0x12,0);

		callback[3].Uninstall(); /* INT 14h */
        RealSetVec(0x14,0);

		callback[4].Uninstall(); /* INT 15h */
        RealSetVec(0x15,0);

		callback[5].Uninstall(); /* INT 17h */
        RealSetVec(0x17,0);

        callback[6].Uninstall(); /* INT 1Ah */
        RealSetVec(0x1A,0);

        RealSetVec(0x1B,0);     /* INT 1Bh */

        callback[7].Uninstall(); /* INT 1Ch */
        RealSetVec(0x1C,0);

        RealSetVec(0x1D,0);     /* INT 1Dh */
        RealSetVec(0x1E,0);     /* INT 1Eh */
        RealSetVec(0x1F,0);     /* INT 1Fh */

		callback[10].Uninstall(); /* INT 19h */
        RealSetVec(0x19,0);

		callback[11].Uninstall(); /* INT 76h: IDE IRQ 14 */
        RealSetVec(0x76,0);

		callback[12].Uninstall(); /* INT 77h: IDE IRQ 15 */
        RealSetVec(0x77,0);

		callback[15].Uninstall(); /* INT 18h: Enter BASIC */
        RealSetVec(0x18,0);

        /* IRQ 6 is nothing special */
		callback[13].Uninstall(); /* INT 0Eh: IDE IRQ 6 */
        callback[13].Install(NULL,CB_IRET_EOI_PIC1,"irq 6");

        /* IRQ 8 is nothing special */
        callback[8].Uninstall();
        callback[8].Install(NULL,CB_IRET_EOI_PIC2,"irq 8");

        /* IRQ 9 is nothing special */
        callback[9].Uninstall();
        callback[9].Install(NULL,CB_IRET_EOI_PIC2,"irq 9");

        /* PIC emulation (correctly) moves IRQ 8-15 down to INT 0x10-0x17 to match PC-98 */
		for (Bit16u ct=0x10;ct <= 0x17;ct++) /* write default IRQ handlers down here */
			RealSetVec(ct,BIOS_DEFAULT_IRQ815_DEF_LOCATION);
		for (Bit16u ct=0x70;ct <= 0x77;ct++) /* zero out IBM PC IRQ 8-15 vectors */
			RealSetVec(ct,0);

        BIOS_SetupKeyboard();
        BIOS_SetupDisks(); /* In PC-98 mode, will zero INT 13h */

		/* INT 18h keyboard and video display functions */
		callback[1].Install(&INT18_PC98_Handler,CB_INT16,"Int 18 keyboard and display");
		callback[1].Set_RealVec(0x18,/*reinstall*/true);

		/* INT 19h *STUB* */
		callback[2].Install(&INT19_PC98_Handler,CB_IRET,"Int 19 ???");
		callback[2].Set_RealVec(0x19,/*reinstall*/true);

		/* INT 1Ah *STUB* */
		callback[3].Install(&INT1A_PC98_Handler,CB_IRET,"Int 1A ???");
		callback[3].Set_RealVec(0x1A,/*reinstall*/true);

		/* INT 1Bh *STUB* */
		callback[4].Install(&INT1B_PC98_Handler,CB_IRET,"Int 1B ???");
		callback[4].Set_RealVec(0x1B,/*reinstall*/true);

		/* INT 1Ch *STUB* */
		callback[5].Install(&INT1C_PC98_Handler,CB_IRET,"Int 1C ???");
		callback[5].Set_RealVec(0x1C,/*reinstall*/true);

		/* INT 1Dh *STUB* */
		callback[6].Install(&INT1D_PC98_Handler,CB_IRET,"Int 1D ???");
		callback[6].Set_RealVec(0x1D,/*reinstall*/true);

		/* INT 1Eh *STUB* */
		callback[7].Install(&INT1E_PC98_Handler,CB_IRET,"Int 1E ???");
		callback[7].Set_RealVec(0x1E,/*reinstall*/true);

		/* INT 1Fh *STUB* */
		callback[10].Install(&INT1F_PC98_Handler,CB_IRET,"Int 1F ???");
		callback[10].Set_RealVec(0x1F,/*reinstall*/true);
    }
public:
    Bitu call_irq0;
    Bitu call_irq07default;
    Bitu call_irq815default;
};

void BIOS_Enter_Boot_Phase(void) {
    /* direct CS:IP right to the instruction that leads to the boot process */
    /* note that since it's a callback instruction it doesn't really matter
     * what CS:IP is as long as it points to the instruction */
    reg_eip = BIOS_boot_code_offset & 0xFUL;
	CPU_SetSegGeneral(cs, BIOS_boot_code_offset >> 4UL);
}

void BIOS_SetCOMPort(Bitu port, Bit16u baseaddr) {
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
	if (test) delete test;
	test = new BIOS(control->GetSection("joystick"));
}

void BIOS_OnEnterPC98Mode(Section* sec) {
    if (test) {
        test->write_FFFF_PC98_signature();

        /* clear out 0x50 segment */
        for (unsigned int i=0;i < 0x100;i++) phys_writeb(0x500+i,0);

        /* set up some default state */
        mem_writeb(0x54C/*MEMB_PRXCRT*/,0x4F); /* default graphics layer off, 24KHz hsync */
    }
}

void BIOS_OnEnterPC98Mode_phase2(Section* sec) {
    if (test) {
        test->rewrite_IRQ_handlers();
    }
}

void swapInNextDisk(bool pressed);
void swapInNextCD(bool pressed);

void INT10_OnResetComplete();
void CALLBACK_DeAllocate(Bitu in);

void MOUSE_Unsetup_DOS(void);
void MOUSE_Unsetup_BIOS(void);

void BIOS_OnResetComplete(Section *x) {
    INT10_OnResetComplete();

    if (biosConfigSeg != 0) {
        ROMBIOS_FreeMemory(biosConfigSeg << 4); /* remember it was alloc'd paragraph aligned, then saved >> 4 */
        biosConfigSeg = 0;
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
	LOG(LOG_MISC,LOG_DEBUG)("Initializing BIOS");

	/* make sure the array is zeroed */
	ISAPNP_SysDevNodeCount = 0;
	ISAPNP_SysDevNodeLargest = 0;
	for (int i=0;i < MAX_ISA_PNP_SYSDEVNODES;i++) ISAPNP_SysDevNodes[i] = NULL;

	/* make sure CD swap and floppy swap mapper events are available */
	MAPPER_AddHandler(swapInNextDisk,MK_f4,MMOD1,"swapimg","SwapFloppy"); /* Originally "Swap Image" but this version does not swap CDs */
	MAPPER_AddHandler(swapInNextCD,MK_f3,MMOD1,"swapcd","SwapCD"); /* Variant of "Swap Image" for CDs */

	/* NTS: VM_EVENT_BIOS_INIT this callback must be first. */
	AddExitFunction(AddExitFunctionFuncPair(BIOS_Destroy),false);
	AddVMEventFunction(VM_EVENT_POWERON,AddVMEventFunctionFuncPair(BIOS_OnPowerOn));
	AddVMEventFunction(VM_EVENT_RESET_END,AddVMEventFunctionFuncPair(BIOS_OnResetComplete));

    /* PC-98 support */
	AddVMEventFunction(VM_EVENT_ENTER_PC98_MODE,AddVMEventFunctionFuncPair(BIOS_OnEnterPC98Mode));
	AddVMEventFunction(VM_EVENT_ENTER_PC98_MODE_END,AddVMEventFunctionFuncPair(BIOS_OnEnterPC98Mode_phase2));
}

void write_ID_version_string() {
	Bitu str_id_at,str_ver_at;
	size_t str_id_len,str_ver_len;

	/* NTS: We can't move the version and ID strings, it causes programs like MSD.EXE to lose
	 *      track of the "IBM compatible blahblahblah" string. Which means that apparently
	 *      programs looking for this information have the address hardcoded ALTHOUGH
	 *      experiments show you can move the version string around so long as it's
	 *      +1 from a paragraph boundary */
	/* TODO: *DO* allow dynamic relocation however if the dosbox.conf indicates that the user
	 *       is not interested in IBM BIOS compatability. Also, it would be really cool if
	 *       dosbox.conf could override these strings and the user could enter custom BIOS
	 *       version and ID strings. Heh heh heh.. :) */
	str_id_at = 0xFE00E;
	str_ver_at = 0xFE061;
	str_id_len = strlen(bios_type_string)+1;
	str_ver_len = strlen(bios_version_string)+1;
	if (!mainline_compatible_bios_mapping) {
		/* need to mark these strings off-limits so dynamic allocation does not overwrite them */
		ROMBIOS_GetMemory(str_id_len+1,"BIOS ID string",1,str_id_at);
		ROMBIOS_GetMemory(str_ver_len+1,"BIOS version string",1,str_ver_at);
	}
	if (str_id_at != 0) {
		for (size_t i=0;i < str_id_len;i++) phys_writeb(str_id_at+i,bios_type_string[i]);
	}
	if (str_ver_at != 0) {
		for (size_t i=0;i < str_ver_len;i++) phys_writeb(str_ver_at+i,bios_version_string[i]);
	}
}

extern Bit8u int10_font_08[256 * 8];

/* NTS: Do not use callbacks! This function is called before CALLBACK_Init() */
void ROMBIOS_Init() {
	Section_prop * section=static_cast<Section_prop *>(control->GetSection("dosbox"));
	Bitu oi,i;

	// log
	LOG(LOG_MISC,LOG_DEBUG)("Initializing ROM BIOS");

	oi = section->Get_int("rom bios minimum size"); /* in KB */
	oi = (oi + 3) & ~3; /* round to 4KB page */
	if (oi > 128) oi = 128;
	if (oi == 0) oi = (mainline_compatible_bios_mapping && machine != MCH_PCJR) ? 128 : 64;
	if (oi < 8) oi = 8; /* because of some of DOSBox's fixed ROM structures we can only go down to 8KB */
	rombios_minimum_size = (oi << 10); /* convert to minimum, using size coming downward from 1MB */

	oi = section->Get_int("rom bios allocation max"); /* in KB */
	oi = (oi + 3) & ~3; /* round to 4KB page */
	if (oi > 128) oi = 128;
	if (oi == 0) oi = (mainline_compatible_bios_mapping && machine != MCH_PCJR) ? 128 : 64;
	if (oi < 8) oi = 8; /* because of some of DOSBox's fixed ROM structures we can only go down to 8KB */
	oi <<= 10;
	if (oi < rombios_minimum_size) oi = rombios_minimum_size;
	rombios_minimum_location = 0x100000 - oi; /* convert to minimum, using size coming downward from 1MB */

	/* in mainline compatible, make sure we cover the 0xF0000-0xFFFFF range */
	if (mainline_compatible_bios_mapping && rombios_minimum_location > 0xF0000) {
		rombios_minimum_location = 0xF0000;
		rombios_minimum_size = 0x10000;
	}

	LOG(LOG_BIOS,LOG_DEBUG)("ROM BIOS range: 0x%05X-0xFFFFF",(int)rombios_minimum_location);
	LOG(LOG_BIOS,LOG_DEBUG)("ROM BIOS range according to minimum size: 0x%05X-0xFFFFF",(int)(0x100000 - rombios_minimum_size));

	if (!MEM_map_ROM_physmem(rombios_minimum_location,0xFFFFF)) E_Exit("Unable to map ROM region as ROM");

	/* and the BIOS alias at the top of memory (TODO: what about 486/Pentium emulation where the BIOS at the 4GB top is different
	 * from the BIOS at the legacy 1MB boundary because of shadowing and/or decompressing from ROM at boot? */
	{
		bool MEM_map_ROM_alias_physmem(Bitu start,Bitu end);
		Bit32u MEM_get_address_bits();

		uint64_t top = (uint64_t)1UL << (uint64_t)MEM_get_address_bits();
		if (top >= ((uint64_t)1UL << (uint64_t)21UL)) { /* 2MB or more */
			unsigned long alias_base,alias_end;

			alias_base = (unsigned long)top + (unsigned long)rombios_minimum_location - (unsigned long)0x100000UL;
			alias_end = (unsigned long)top - (unsigned long)1UL;

			LOG(LOG_BIOS,LOG_DEBUG)("ROM BIOS also mapping alias to 0x%08lx-0x%08lx",alias_base,alias_end);
			if (!MEM_map_ROM_alias_physmem(alias_base,alias_end)) E_Exit("Unable to map ROM region as ROM alias");
		}
	}

	/* set up allocation */
	rombios_alloc.name = "ROM BIOS";
	rombios_alloc.topDownAlloc = true;
	rombios_alloc.initSetRange(rombios_minimum_location,0xFFFF0 - 1);

	write_ID_version_string();

	/* some structures when enabled are fixed no matter what */
	if (!mainline_compatible_bios_mapping && rom_bios_8x8_cga_font) {
		/* line 139, int10_memory.cpp: the 8x8 font at 0xF000:FA6E, first 128 chars.
		 * allocate this NOW before other things get in the way */
		if (ROMBIOS_GetMemory(128*8,"BIOS 8x8 font (first 128 chars)",1,0xFFA6E) == 0) {
			LOG_MSG("WARNING: Was not able to mark off 0xFFA6E off-limits for 8x8 font");
		}
	}

	/* install the font */
	if (rom_bios_8x8_cga_font) {
		for (i=0;i<128*8;i++) {
			phys_writeb(PhysMake(0xf000,0xfa6e)+i,int10_font_08[i]);
		}
	}

	if (mainline_compatible_bios_mapping) {
		/* then mark the region 0xE000-0xFFF0 as off-limits.
		 * believe it or not, there's this whole range between 0xF3000 and 0xFE000 that remains unused! */
		if (ROMBIOS_GetMemory(0xFFFF0-0xFE000,"BIOS with fixed layout",1,0xFE000) == 0)
			E_Exit("Mainline compat bios mapping: failed to declare entire BIOS area off-limits");
	}
}

