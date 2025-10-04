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

/* NTS: Hardware notes
 *
 * S3 Virge DX (PCI):
 *
 *   VGA 256-color chained mode appears to work differently than
 *   expected. Groups of 4 pixels are spread across the VGA planes
 *   as expected, but actual memory storage of those 32-bit quantities
 *   are 4 "bytes" apart (probably the inspiration for DOSBox's
 *   chain emulation using addr = ((addr & ~3) << 2) + (addr & 3) when
 *   emulating chained mode).
 *
 *   The attribute controller has a bug where if you attempt to read
 *   the palette indexes 0x00-0x0F with PAS=1 (see FreeVGA for more
 *   info) the returned value will be correct except for bit 5 which
 *   will be 1 i.e. if palette index 0x2 is read in this way and
 *   contains 0x02 you will get 0x22 instead. The trick is to write
 *   the index with PAS=0 and read the data, then issue the index with
 *   PAS=1. Related: the S3 acts as if there are different flip-flops
 *   associated with reading vs writing i.e. writing to 0x3C0, then
 *   reading port 0x3C1, then writing port 0x3C0, will treat the second
 *   write to 0x3C0 as data still, not as an index. Both flip flops are
 *   reset by reading 3xAh though.
 *
 *   VGA BIOS does not support INT 10h TTY functions in VESA BIOS modes.
 *
 *   Raw planar dumps of the VGA memory in alphanumeric modes suggest
 *   that, not only do the cards direct even writes to 0/2 and odd to 1/3,
 *   it does so without shifting down the address. So in a raw planar
 *   dump, you will see on plane 0 something like "C : \ > " where the
 *   spaces are hex 0x00, and in plane 1, something like 0x07 0x00 0x07 0x00 ...
 *   the card however (in even/odd mode) does mask out bit A0 which
 *   explains why the Plane 1 capture is 0x07 0x00 ... not 0x00 0x07.
 *
 * ATI Rage 128 (AGP):
 *
 *   VGA 256-color chained mode appears to work in the same weird way
 *   that S3 Virge DX does (see above).
 *
 *   VGA BIOS supports TTY INT 10h functions in 16 & 256-color modes
 *   only. There are apparently INT 10h functions for 15/16bpp modes
 *   as well, but they don't appear to render anything except shades
 *   of green.
 *
 *   The VESA BIOS interface seems to have INT 10h aliases for many
 *   VBE 1.2 modes i.e. mode 0x110 is also mode 0x42.
 *
 *   The Attribute Controller palette indexes work as expected in all
 *   VGA modes, however in SVGA VESA BIOS modes, the Attribute Controller
 *   palette has no effect on output EXCEPT in 16-color (4bpp) VESA BIOS
 *   modes.
 *
 *   Raw planar layout of alphanumeric text modes apply the same rules
 *   as mentioned above in the S3 Virge DX description.
 *
 * Compaq Elite LTE 4/50CX laptop:
 *
 *   No SVGA modes. All modes work as expected.
 *
 *   VGA 256-color chained mode acts the same weird way as described
 *   above, seems to act like addr = ((addr & ~3) << 2) + (addr & 3)
 *
 *   There seems to be undocumented INT 10h modes:
 *
 *        0x22:  640x480x?? INT 10h text is all green and garbled
 *        0x28:  320x200x?? INT 10h text is all green and garbled
 *        0x32:  640x480x?? INT 10h text is all yellow and garbled
 *        0x5E:  640x400x256-color with bank switching
 *        0x5F:  640x480x256-color with bank switching
 *        0x62:  640x480x?? INT 10h text is all dark gray
 *        0x68:  320x200x?? INT 10h text is all dark gray
 *        0x72:  640x480x?? INT 10h text is all dark gray
 *        0x78:  320x200x?? INT 10h text is all dark gray
 *
 *   And yet, the BIOS does not implement VESA BIOS extensions. Hm..
 *
 * Sharp PC-9030 with Cirrus SVGA (1996):
 *
 *   VGA 256-color chained mode acts the same weird way, as if:
 *   addr = ((addr & ~3) << 2) + (addr & 3)
 * 
 *   All VESA BIOS modes support INT 10h TTY output.
 *
 * Tseng ET4000AX:
 *
 *   The ET4000 cards appear to be the ONLY SVGA chipset out there
 *   that does NOT do the odd VGA 256-color chained mode that
 *   other cards do.
 *
 *   Chained 256-color on ET4000:
 *       addr = addr                             (addr >> 2) byte in planar space, plane select by (addr & 3)
 *
 *   Other VGA cards:
 *       addr = ((addr & ~3) << 2) + (addr & 3)  (addr & ~3) byte in planar space, plane select by (addr & 3)
 *
 *   I suspect that this difference may be the reason several 1992-1993-ish DOS demos have problems clearing
 *   VRAM. It's possible they noticed that zeroing RAM was faster in planar mode, and coded their routines
 *   around ET4000 cards, not knowing that Trident, Cirrus, and every VGA clone thereafter implemented the
 *   chained 256-color modes differently.
 */

#define VGA_INTERNAL

#include <assert.h>

#include "dosbox.h"
#include "logging.h"
#include "setup.h"
#include "video.h"
#include "pic.h"
#include "vga.h"
#include "inout.h"
#include "programs.h"
#include "support.h"
#include "setup.h"
#include "timer.h"
#include "mem.h"
#include "pci_bus.h"
#include "util_units.h"
#include "control.h"
#include "pc98_cg.h"
#include "pc98_dac.h"
#include "pc98_gdc.h"
#include "pc98_gdc_const.h"
#include "pc98_artic.h"
#include "mixer.h"
#include "menu.h"
#include "mem.h"
#include "render.h"
#include "jfont.h"
#include "bitop.h"
#include "sdlmain.h"

#include <string.h>
#include <stdlib.h>
#include <string>
#include <stdio.h>

#if defined(_MSC_VER)
# pragma warning(disable:4244) /* const fmath::local::uint64_t to double possible loss of data */
#endif

#include "zipfile.h"

#include <output/output_ttf.h>

using namespace std;

static uint32_t                     assigned_lfb = 0;

bool                                VGA_PITsync = false;

unsigned int                        vbe_window_granularity = 0;
unsigned int                        vbe_window_size = 0;

/* current dosplay page (controlled by A4h) */
unsigned char*                      pc98_pgraph_current_display_page;
/* current CPU page (controlled by A6h) */
unsigned char*                      pc98_pgraph_current_cpu_page;

bool                                vga_8bit_dac = false;
bool                                enable_vga_8bit_dac = true;
bool                                ignore_sequencer_blanking = false;
bool                                memio_complexity_optimization = true;
bool                                vga_render_on_demand = false; // Render at vsync or specific changes to hardware instead of every scanline
signed char                         vga_render_on_demand_user = -1;

bool                                pc98_crt_mode = false;      // see port 6Ah command 40h/41h.
                                                                // this boolean is the INVERSE of the bit.

uint32_t S3_LFB_BASE =              S3_LFB_BASE_DEFAULT;

bool                                enable_pci_vga = true;

VGA_Type vga;
JEGA_DATA jega;
SVGA_Driver svga;
int enableCGASnow;
int vesa_modelist_cap = 0;
int vesa_mode_width_cap = 0;
int vesa_mode_height_cap = 0;
bool vesa_bios_modelist_in_info = false;
bool vga_3da_polled = false;
bool vga_page_flip_occurred = false;
bool enable_page_flip_debugging_marker = false;
bool enable_vretrace_poll_debugging_marker = false;
bool vga_enable_hretrace_effects = false;
bool vga_enable_hpel_effects = false;
bool vga_enable_3C6_ramdac = false;
bool egavga_per_scanline_hpel = true;
bool vga_sierra_lock_565 = false;
bool vga_fill_inactive_ram = false;
bool enable_vga_resize_delay = false;
bool vga_ignore_hdispend_change_if_smaller = false;
bool ignore_vblank_wraparound = false;
bool non_cga_ignore_oddeven = false;
bool non_cga_ignore_oddeven_engage = false;
bool vga_ignore_extended_memory_bit = false;
bool vga_palette_update_on_full_load = true;
bool vga_double_buffered_line_compare = false;
bool pc98_allow_scanline_effect = true;
bool pc98_allow_4_display_partitions = false;
bool pc98_graphics_hide_odd_raster_200line = false;
bool pc98_attr4_graphic = false;
bool pc98_40col_text = false;
bool gdc_analog = true;
bool pc98_31khz_mode = false;
bool int10_vesa_map_as_128kb = false;

unsigned char VGA_AC_remap = AC_4x4;

unsigned int vga_display_start_hretrace = 0;
float hretrace_fx_avg_weight = 3;

bool allow_vesa_4bpp_packed = true;
bool allow_vesa_lowres_modes = true;
bool allow_unusual_vesa_modes = true;
bool allow_explicit_vesa_24bpp = true;
bool allow_hd_vesa_modes = true;
bool vesa12_modes_32bpp = true;
bool allow_vesa_32bpp = true;
bool allow_vesa_24bpp = true;
bool allow_vesa_16bpp = true;
bool allow_vesa_15bpp = true;
bool allow_vesa_8bpp = true;
bool allow_vesa_4bpp = true;
bool allow_vesa_tty = true;

uint32_t HercBlend_2_Table[16];
uint32_t CGA_2_Table[16];
uint32_t CGA_4_Table[256];
uint32_t CGA_4_HiRes_Table[256];
uint32_t CGA_4_HiRes_TableNP[256];
uint32_t CGA_16_Table[256];
uint32_t TXT_Font_Table[16];
uint32_t TXT_FG_Table[16];
uint32_t TXT_BG_Table[16];
uint32_t ExpandTable[256];
uint32_t Expand16Table[4][16];
uint32_t FillTable[16];
uint32_t ColorTable[16];
double vga_force_refresh_rate = -1;

uint8_t CGAPal2[2] = {0,0};
uint8_t CGAPal4[4] = {0,0,0,0};

void page_flip_debug_notify() {
	if (enable_page_flip_debugging_marker)
		vga_page_flip_occurred = true;
}

void vsync_poll_debug_notify() {
	if (enable_vretrace_poll_debugging_marker)
		vga_3da_polled = true;
}

void VGA_SetModeNow(VGAModes mode) {
	if (vga.mode == mode) return;
	vga.mode=mode;
	VGA_SetupHandlers();
	VGA_StartResize(0);
}

void VGA_SetMode(VGAModes mode) {
	if (vga.mode == mode) return;
	vga.mode=mode;
	VGA_SetupHandlers();
	VGA_StartResize();
}

bool VGA_DetermineMode_IsDCGA(void) {
	return (vga.gfx.miscellaneous & 0x0c) == 0x0c && J3_IsCga4Dcga();
}

void VGA_DetermineMode_StandardVGA(void) { /* and EGA, the extra regs are not used for machine=ega */
	if (vga.attr.mode_control & 1) { // graphics mode
		if (IS_VGA_ARCH && (vga.gfx.mode & 0x40)) {
			VGA_SetMode(M_VGA);
		}
		// NTS: Also handled by M_EGA case
		//          else if (vga.gfx.mode & 0x20) VGA_SetMode(M_CGA4);

		// NTS: Two things here. One is that CGA 2-color mode (and the MCGA 640x480 2-color mode)
		//      are just EGA planar modes with fewer bitplanes enabled. The planar render mode can
		//      display them just fine. The other is that checking for 2-color CGA mode entirely by
		//      whether video RAM is mapped to B8000h is a really lame way to go about it.
		//
		//      The only catch here is that a contributor (Wengier, I think?) tied a DOS/V CGA rendering
		//      mode into M_CGA2 that we need to watch for.
		//
		else if (VGA_DetermineMode_IsDCGA()) {
			VGA_SetMode(M_DCGA);
		}
		else {
			VGA_SetMode(M_EGA);
		}
	} else {
		VGA_SetMode(M_TEXT);
	}
}

void VGA_DetermineMode(void) {
	if (svga.determine_mode)
		svga.determine_mode();
	else
		VGA_DetermineMode_StandardVGA();
}

void VGA_StartResize(Bitu delay /*=50*/) {
	if (!vga.draw.resizing) {
		/* even if the user disables the delay, we can avoid a lot of window resizing by at least having 1ms of delay */
		if (!enable_vga_resize_delay && delay > 1) delay = 1;

		vga.draw.resizing=true;
		if (vga.mode==M_ERROR) delay = 5;
		/* Start a resize after delay (default 50 ms) */
		if (delay==0) VGA_SetupDrawing(0);
		else PIC_AddEvent(VGA_SetupDrawing,(float)delay);
	}
}

void VGA_SetClock(Bitu which,Bitu target) {
	if (svga.set_clock)
		svga.set_clock(which, target);
}

void VGA_SetCGA2Table(uint8_t val0,uint8_t val1) {
	const uint8_t total[2] = {val0,val1};
	for (unsigned int i=0;i < 2;i++) CGAPal2[i] = total[i];
	for (Bitu i=0;i<16u;i++) {
		CGA_2_Table[i]=
#ifdef WORDS_BIGENDIAN
			((Bitu)total[(i >> 0u) & 1u] << 0u  ) | ((Bitu)total[(i >> 1u) & 1u] << 8u  ) |
			((Bitu)total[(i >> 2u) & 1u] << 16u ) | ((Bitu)total[(i >> 3u) & 1u] << 24u );
#else 
		((Bitu)total[(i >> 3u) & 1u] << 0u  ) | ((Bitu)total[(i >> 2u) & 1u] << 8u  ) |
			((Bitu)total[(i >> 1u) & 1u] << 16u ) | ((Bitu)total[(i >> 0u) & 1u] << 24u );
#endif
	}

	if (machine == MCH_MCGA) {
		VGA_DAC_CombineColor(0x0,val0);
		VGA_DAC_CombineColor(0x1,val1);
	}
}

void VGA_SetCGA4Table(uint8_t val0,uint8_t val1,uint8_t val2,uint8_t val3) {
	const uint8_t total[4] = {val0,val1,val2,val3};
	for (unsigned int i=0;i < 4;i++) CGAPal4[i] = total[i];
	for (Bitu i=0;i<256u;i++) {
		CGA_4_Table[i]=
#ifdef WORDS_BIGENDIAN
			((Bitu)total[(i >> 0u) & 3u] << 0u  ) | ((Bitu)total[(i >> 2u) & 3u] << 8u  ) |
			((Bitu)total[(i >> 4u) & 3u] << 16u ) | ((Bitu)total[(i >> 6u) & 3u] << 24u );
#else
		((Bitu)total[(i >> 6u) & 3u] << 0u  ) | ((Bitu)total[(i >> 4u) & 3u] << 8u  ) |
			((Bitu)total[(i >> 2u) & 3u] << 16u ) | ((Bitu)total[(i >> 0u) & 3u] << 24u );
#endif
		CGA_4_HiRes_Table[i]=
#ifdef WORDS_BIGENDIAN
			((Bitu)total[((i >> 0u) & 1u) | ((i >> 3u) & 2u)] << 0u  ) | (Bitu)(total[((i >> 1u) & 1u) | ((i >> 4u) & 2u)] << 8u  ) |
			((Bitu)total[((i >> 2u) & 1u) | ((i >> 5u) & 2u)] << 16u ) | (Bitu)(total[((i >> 3u) & 1u) | ((i >> 6u) & 2u)] << 24u );
#else
		((Bitu)total[((i >> 3u) & 1u) | ((i >> 6u) & 2u)] << 0u  ) | (Bitu)(total[((i >> 2u) & 1u) | ((i >> 5u) & 2u)] << 8u  ) |
			((Bitu)total[((i >> 1u) & 1u) | ((i >> 4u) & 2u)] << 16u ) | (Bitu)(total[((i >> 0u) & 1u) | ((i >> 3u) & 2u)] << 24u );
#endif
		CGA_4_HiRes_TableNP[i]=
#ifdef WORDS_BIGENDIAN
			((Bitu)(((i >> 0u) & 1u) | ((i >> 3u) & 2u)) << 0u  ) | (Bitu)((((i >> 1u) & 1u) | ((i >> 4u) & 2u)) << 8u  ) |
			((Bitu)(((i >> 2u) & 1u) | ((i >> 5u) & 2u)) << 16u ) | (Bitu)((((i >> 3u) & 1u) | ((i >> 6u) & 2u)) << 24u );
#else
		((Bitu)(((i >> 3u) & 1u) | ((i >> 6u) & 2u)) << 0u  ) | (Bitu)((((i >> 2u) & 1u) | ((i >> 5u) & 2u)) << 8u  ) |
			((Bitu)(((i >> 1u) & 1u) | ((i >> 4u) & 2u)) << 16u ) | (Bitu)((((i >> 0u) & 1u) | ((i >> 3u) & 2u)) << 24u );
#endif

	}

	if (machine == MCH_MCGA) {
		VGA_DAC_CombineColor(0x0,val0);
		VGA_DAC_CombineColor(0x1,val1);
		VGA_DAC_CombineColor(0x2,val2);
		VGA_DAC_CombineColor(0x3,val3);
	}
}

void SetPITSync(char *x) {
	if (!strncasecmp(x,"off",3))
		VGA_PITsync = false;
	else if (!strncasecmp(x,"on",3))
		VGA_PITsync = true;
}

void SetRate(char *x) {
	if (!strncasecmp(x,"off",3))
		vga_force_refresh_rate = -1;
	else if (!strncasecmp(x,"ntsc",4))
		vga_force_refresh_rate = 60000.0/1001;
	else if (!strncasecmp(x,"pal",3))
		vga_force_refresh_rate = 50;
	else if (strchr(x,'.'))
		vga_force_refresh_rate = atof(x);
	else {
		/* fraction */
		int major = -1,minor = 0;
		major = strtol(x,&x,0);
		if (*x == '/' || *x == ':') {
			x++; minor = strtol(x,NULL,0);
		}

		if (major > 0) {
			vga_force_refresh_rate = (double)major;
			if (minor > 1) vga_force_refresh_rate /= minor;
		}
	}

	VGA_SetupHandlers();
	VGA_StartResize();
}

#if defined(USE_TTF)
void resetFontSize();
static void resetSize(Bitu /*val*/) {
	resetFontSize();
}
#endif

class VFRCRATE : public Program {
	public:
		void Run(void) override {
			if (cmd->FindExist("/?", false)) {
				WriteOut("Locks or unlocks the video refresh rate.\n\n");
				WriteOut("VFRCRATE [SET [OFF|PAL|NTSC|rate]\n");
				WriteOut("  SET OFF   Unlock the refresh rate\n");
				WriteOut("  SET PAL   Lock to PAL frame rate\n");
				WriteOut("  SET NTSC  Lock to NTSC frame rate\n");
				WriteOut("  SET rate  Lock to integer frame rate, e.g. 15\n");
				WriteOut("  SET rate  Lock to decimal frame rate, e.g. 29.97\n");
				WriteOut("  SET rate  Lock to fractional frame rate, e.g. 60000/1001\n\n");
				WriteOut("  PITSYNC <ON|OFF> Make PIT timer tick at refresh rate if close enough\n\n");
				WriteOut("Type VFRCRATE without a parameter to show the current status.\n");
				return;
			}
			if (cmd->FindString("SET",temp_line,false))
				SetRate((char *)temp_line.c_str());
			if (cmd->FindString("PITSYNC",temp_line,false))
				SetPITSync((char *)temp_line.c_str());
#if defined(USE_TTF)
			if (TTF_using()) PIC_AddEvent(&resetSize, 1);
#endif
			if (vga_force_refresh_rate > 0)
				WriteOut("Video refresh rate is locked to %.3f fps.\n",vga_force_refresh_rate);
			else
				WriteOut("Video refresh rate is unlocked.\n");
		}
};

void VFRCRATE_ProgramStart(Program * * make) {
	*make=new VFRCRATE;
}

/*! \brief          CGASNOW.COM utility to control CGA snow emulation
 *
 *  \description    Utility to enable, disable, or query CGA snow emulation.
 *                  This command is only available when machine=cga and
 *                  the video mode is 80x25 text mode.
 */
class CGASNOW : public Program {
	public:
		/*! \brief      Program entry point, when the command is run
		*/
		void Run(void) override {
			if (cmd->FindExist("/?", false)) {
				WriteOut("Turns CGA snow emulation on or off.\n\n");
				WriteOut("CGASNOW [ON|OFF]\n");
				WriteOut("  ON   Turns on CGA snow emulation.\n");
				WriteOut("  OFF  Turns off CGA snow emulation.\n\n");
				WriteOut("Type CGASNOW without a parameter to show the current status.\n");
				return;
			}
			if(cmd->FindExist("ON")) {
				WriteOut("CGA snow enabled.\n");
				enableCGASnow = 1;
				if (vga.mode == M_TEXT || vga.mode == M_TANDY_TEXT) {
					VGA_SetupHandlers();
					VGA_StartResize();
				}
			}
			else if(cmd->FindExist("OFF")) {
				WriteOut("CGA snow disabled.\n");
				enableCGASnow = 0;
				if (vga.mode == M_TEXT || vga.mode == M_TANDY_TEXT) {
					VGA_SetupHandlers();
					VGA_StartResize();
				}
			}
			else {
				WriteOut("CGA snow is currently %s.\n", enableCGASnow ? "enabled" : "disabled");
			}
		}
};

void CGASNOW_ProgramStart(Program * * make) {
	*make=new CGASNOW;
}

/* TODO: move to general header */
static inline unsigned int int_log2(unsigned int val) {
	unsigned int log = 0;
	while ((val >>= 1u) != 0u) log++;
	return log;
}

VGA_Vsync VGA_Vsync_Decode(const char *vsyncmodestr) {
	if (!strcasecmp(vsyncmodestr,"off")) return VS_Off;
	else if (!strcasecmp(vsyncmodestr,"on")) return VS_On;
	else if (!strcasecmp(vsyncmodestr,"force")) return VS_Force;
	else if (!strcasecmp(vsyncmodestr,"host")) return VS_Host;
	else
		LOG_MSG("Illegal vsync type %s, falling back to off.",vsyncmodestr);

	return VS_Off;
}

void VGA_Reset(Section*) {
	//  All non-PC98 video-related config settings are now in the [video] section

	Section_prop * section=static_cast<Section_prop *>(control->GetSection("video"));
	Section_prop * pc98_section=static_cast<Section_prop *>(control->GetSection("pc98"));

	bool lfb_default = false;
	string str;
	int i;

	uint32_t cpu_addr_bits = MEM_get_address_bits();
	//    uint64_t cpu_max_addr = (uint64_t)1 << (uint64_t)cpu_addr_bits;

	LOG(LOG_MISC,LOG_DEBUG)("VGA_Reset() reinitializing VGA emulation");

	str = section->Get_string("enable supermegazeux tweakmode");
	if (str == "1" || str == "true")
		enable_supermegazeux_256colortext = true;
	else if (str == "0" || str == "false")
		enable_supermegazeux_256colortext = false;
	else
		enable_supermegazeux_256colortext = false; // TODO: Default true if emulating a chipset that supports SuperMegazeux 256-color text mode

	GDC_display_plane_wait_for_vsync = pc98_section->Get_bool("pc-98 buffer page flip");

	enable_pci_vga = section->Get_bool("pci vga");

	S3_LFB_BASE = (uint32_t)section->Get_hex("svga lfb base");
	if (S3_LFB_BASE == 0) {
		if (cpu_addr_bits >= 32)
			S3_LFB_BASE = S3_LFB_BASE_DEFAULT;
		else if (cpu_addr_bits >= 26)
			S3_LFB_BASE = (enable_pci_vga && has_pcibus_enable()) ? 0x02000000 : 0x03400000;
		else if (cpu_addr_bits >= 24)
			S3_LFB_BASE = 0x00C00000;
		else
			S3_LFB_BASE = S3_LFB_BASE_DEFAULT;

		lfb_default = true;
	}

	/* no farther than 64MB below the top, do not overlap the BIOS */
	if (S3_LFB_BASE > 0xFC000000UL)
		S3_LFB_BASE = 0xFC000000UL;

	/* if the user WANTS the base address to be PCI misaligned, then turn off PCI VGA emulation */
	if (enable_pci_vga && has_pcibus_enable() && (S3_LFB_BASE & 0x1FFFFFFul)) {
		if (!lfb_default)
			LOG(LOG_VGA,LOG_DEBUG)("S3 linear framebuffer was set by user to an address not aligned to 32MB, switching off PCI VGA emulation");

		enable_pci_vga = false;
	}
	/* if memalias is below 26 bits, PCI VGA emulation is impossible */
	if (cpu_addr_bits < 26) {
		if (IS_VGA_ARCH && enable_pci_vga && has_pcibus_enable())
			LOG(LOG_VGA,LOG_DEBUG)("CPU memalias setting is below 26 bits, switching off PCI VGA emulation");

		enable_pci_vga = false;
	}

	/* must not overlap system RAM or other devices */
	if (S3_LFB_BASE < (MEM_TotalPages()*4096))
		S3_LFB_BASE = (MEM_TotalPages()*4096);
	if (S3_LFB_BASE < assigned_lfb)
		S3_LFB_BASE = assigned_lfb;

	if (enable_pci_vga && has_pcibus_enable()) {
		/* must be 32MB aligned (PCI) */
		S3_LFB_BASE +=  0x1FFFFFFUL;
		S3_LFB_BASE &= ~0x1FFFFFFUL;
	}
	else {
		/* must be 64KB aligned (ISA) */
		S3_LFB_BASE +=  0xFFFFUL;
		S3_LFB_BASE &= ~0xFFFFUL;
	}

	/* sanity check */
	if (S3_LFB_BASE >= 0xFE000000UL) E_Exit("S3 LFB base 0x%lx would overlap BIOS",(unsigned long)S3_LFB_BASE);

	/* if the constraints we imposed make it impossible to maintain the alignment required for PCI,
	 * then just switch off PCI VGA emulation. */
	if (IS_VGA_ARCH && enable_pci_vga && has_pcibus_enable()) {
		if (S3_LFB_BASE & 0x1FFFFFFUL) { /* not 32MB aligned */
			LOG(LOG_VGA,LOG_DEBUG)("S3 linear framebuffer is not 32MB aligned, switching off PCI VGA emulation");
			enable_pci_vga = false;
		}
	}

	/* announce LFB framebuffer address only if actually emulating the S3 */
	if (IS_VGA_ARCH && svgaCard == SVGA_S3Trio)
		LOG(LOG_VGA,LOG_DEBUG)("S3 linear framebuffer at 0x%lx%s as %s",
				(unsigned long)S3_LFB_BASE,lfb_default?" by default":"",
				(enable_pci_vga && has_pcibus_enable()) ? "PCI" : "(E)ISA");

	/* other applicable warnings: */
	/* Microsoft Windows 3.1 S3 driver:
	 *   If the LFB is set to an address below 16MB, the driver will program the base to something
	 *   odd like 0x73000000 and access MMIO through 0x74000000.
	 *
	 *   Because of this, if memalias < 31 and LFB is below 16MB mark, Windows won't use the
	 *   accelerated features of the S3 emulation properly.
	 *
	 *   If memalias=24, the driver hangs and nothing appears on screen.
	 *
	 *   As far as I can tell, it's mapping for the LFB, not the MMIO. It uses the MMIO in the
	 *   A0000-AFFFF range anyway. The failure to blit and draw seems to be caused by mapping the
	 *   LFB out of range like that and then trying to draw on the LFB.
	 *
	 *   As far as I can tell from http://www.vgamuseum.info and the list of S3 cards, the S3 chipsets
	 *   emulated by DOSBox-X and DOSBox SVN here are all EISA and PCI cards, so it's likely the driver
	 *   is written around the assumption that memory addresses are the full 32 bits to the card, not
	 *   just the low 24 seen on the ISA slot. So it is unlikely the driver could ever support the
	 *   card on a 386SX nor could such a card work on a 386SX. It shouldn't even work on a 486SX
	 *   (26-bit limit), but it could. */
	if (IS_VGA_ARCH && svgaCard == SVGA_S3Trio && cpu_addr_bits <= 24)
		LOG(LOG_VGA,LOG_WARN)("S3 linear framebuffer warning: memalias setting is known to cause the Windows 3.1 S3 driver to crash");
	if (IS_VGA_ARCH && svgaCard == SVGA_S3Trio && cpu_addr_bits < 31 && S3_LFB_BASE < 0x1000000ul) /* below 16MB and memalias == 31 bits */
		LOG(LOG_VGA,LOG_WARN)("S3 linear framebuffer warning: A linear framebuffer below the 16MB mark in physical memory when memalias < 31 is known to have problems with the Windows 3.1 S3 driver");

	pc98_allow_scanline_effect = pc98_section->Get_bool("pc-98 allow scanline effect");
	mainMenu.get_item("pc98_allow_200scanline").check(pc98_allow_scanline_effect).refresh_item(mainMenu);

	// whether the GDC is running at 2.5MHz or 5.0MHz.
	// Some games require the GDC to run at 5.0MHz.
	// To enable these games we default to 5.0MHz.
	// NTS: There are also games that refuse to run if 5MHz switched on (TH03)
	gdc_5mhz_mode = pc98_section->Get_bool("pc-98 start gdc at 5mhz");
	mainMenu.get_item("pc98_5mhz_gdc").check(gdc_5mhz_mode).refresh_item(mainMenu);

	// record the initial setting.
	// the guest can change it later.
	// however the 8255 used to hold dip switch settings needs to reflect the
	// initial setting.
	gdc_5mhz_mode_initial = gdc_5mhz_mode;

	enable_pc98_egc = pc98_section->Get_bool("pc-98 enable egc");
	enable_pc98_grcg = pc98_section->Get_bool("pc-98 enable grcg");
	enable_pc98_16color = pc98_section->Get_bool("pc-98 enable 16-color");
	enable_pc98_256color = pc98_section->Get_bool("pc-98 enable 256-color");
	enable_pc98_188usermod = pc98_section->Get_bool("pc-98 enable 188 user cg");
	enable_pc98_256color_planar = pc98_section->Get_bool("pc-98 enable 256-color planar");

#if 0//TODO: Do not enforce until 256-color mode is fully implemented.
	//      Some users out there may expect the EGC, GRCG, 16-color options to disable the emulation.
	//      Having 256-color mode on by default, auto-enable them, will cause surprises and complaints.
	// 256-color mode implies EGC, 16-color, GRCG
	if (enable_pc98_256color) enable_pc98_grcg = enable_pc98_16color = true;
#endif

	// EGC implies GRCG
	if (enable_pc98_egc) enable_pc98_grcg = true;

	// EGC implies 16-color
	if (enable_pc98_16color) enable_pc98_16color = true;

	str = section->Get_string("vga attribute controller mapping");
	if (str == "4x4")
		VGA_AC_remap = AC_4x4;
	else if (str == "4low")
		VGA_AC_remap = AC_low4;
	else {
		/* auto:
		 *
		 * 4x4 by default.
		 * except for ET4000 which is 4low */
		VGA_AC_remap = AC_4x4;
		if (IS_VGA_ARCH) {
			if (svgaCard == SVGA_TsengET3K || svgaCard == SVGA_TsengET4K)
				VGA_AC_remap = AC_low4;
		}
	}

	str = pc98_section->Get_string("pc-98 video mode");
	if (str == "31khz")
		pc98_31khz_mode = true;
	else if (str == "15khz")/*TODO*/
		pc98_31khz_mode = false;
	else
		pc98_31khz_mode = false;
	//TODO: Announce 31-KHz mode in BIOS config area. --yksoft1

	i = pc98_section->Get_int("pc-98 allow 4 display partition graphics");
	pc98_allow_4_display_partitions = (i < 0/*auto*/ || i == 1/*on*/);
	mainMenu.get_item("pc98_allow_4partitions").check(pc98_allow_4_display_partitions).refresh_item(mainMenu);
	// TODO: "auto" will default to true if old PC-9801, false if PC-9821, or
	//       a more refined automatic choice according to actual hardware.

	mainMenu.get_item("pc98_enable_egc").check(enable_pc98_egc).refresh_item(mainMenu);
	mainMenu.get_item("pc98_enable_grcg").check(enable_pc98_grcg).refresh_item(mainMenu);
	mainMenu.get_item("pc98_enable_analog").check(enable_pc98_16color).refresh_item(mainMenu);
	mainMenu.get_item("pc98_enable_analog256").check(enable_pc98_256color).refresh_item(mainMenu);
	mainMenu.get_item("pc98_enable_188user").check(enable_pc98_188usermod).refresh_item(mainMenu);

	vga_force_refresh_rate = -1;
	str=section->Get_string("forcerate");
	if (str == "ntsc")
		vga_force_refresh_rate = 60000.0 / 1001;
	else if (str == "pal")
		vga_force_refresh_rate = 50;
	else if (str.find_first_of('/') != string::npos) {
		char *p = (char*)str.c_str();
		int num = 1,den = 1;
		num = strtol(p,&p,0);
		if (*p == '/') p++;
		den = strtol(p,&p,0);
		if (num < 1) num = 1;
		if (den < 1) den = 1;
		vga_force_refresh_rate = (double)num / den;
	}
	else {
		vga_force_refresh_rate = atof(str.c_str());
	}

	enableCGASnow = section->Get_bool("cgasnow");
	vesa_modelist_cap = section->Get_int("vesa modelist cap");
	vesa_mode_width_cap = section->Get_int("vesa modelist width limit");
	vesa_mode_height_cap = section->Get_int("vesa modelist height limit");
	vga_enable_3C6_ramdac = section->Get_bool("sierra ramdac");
	vga_enable_hpel_effects = section->Get_bool("allow hpel effects");
	vga_sierra_lock_565 = section->Get_bool("sierra ramdac lock 565");
	vga_fill_inactive_ram = section->Get_bool("vga fill active memory");
	hretrace_fx_avg_weight = section->Get_double("hretrace effect weight");
	egavga_per_scanline_hpel = section->Get_bool("ega per scanline hpel");
	ignore_vblank_wraparound = section->Get_bool("ignore vblank wraparound");
	int10_vesa_map_as_128kb = section->Get_bool("vesa map non-lfb modes to 128kb region");
	vga_enable_hretrace_effects = section->Get_bool("allow hretrace effects");
	enable_page_flip_debugging_marker = section->Get_bool("page flip debug line");
	vga_palette_update_on_full_load = section->Get_bool("vga palette update on full load");
	non_cga_ignore_oddeven = section->Get_bool("ignore odd-even mode in non-cga modes");
	vga_ignore_extended_memory_bit = section->Get_bool("ignore extended memory bit");
	enable_vretrace_poll_debugging_marker = section->Get_bool("vertical retrace poll debug line");
	vga_double_buffered_line_compare = section->Get_bool("double-buffered line compare");
	hack_lfb_xadjust = section->Get_int("vesa lfb pel scanline adjust");
	hack_lfb_yadjust = section->Get_int("vesa lfb base scanline adjust");
	allow_vesa_lowres_modes = section->Get_bool("allow low resolution vesa modes");
	allow_vesa_4bpp_packed = section->Get_bool("allow 4bpp packed vesa modes");
	allow_explicit_vesa_24bpp = section->Get_bool("allow explicit 24bpp vesa modes");
	allow_hd_vesa_modes = section->Get_bool("allow high definition vesa modes");
	allow_unusual_vesa_modes = section->Get_bool("allow unusual vesa modes");
	allow_vesa_32bpp = section->Get_bool("allow 32bpp vesa modes");
	allow_vesa_24bpp = section->Get_bool("allow 24bpp vesa modes");
	allow_vesa_16bpp = section->Get_bool("allow 16bpp vesa modes");
	allow_vesa_15bpp = section->Get_bool("allow 15bpp vesa modes");
	allow_vesa_8bpp = section->Get_bool("allow 8bpp vesa modes");
	allow_vesa_4bpp = section->Get_bool("allow 4bpp vesa modes");
	allow_vesa_tty = section->Get_bool("allow tty vesa modes");
	enable_vga_resize_delay = section->Get_bool("enable vga resize delay");
	vga_ignore_hdispend_change_if_smaller = section->Get_bool("resize only on vga active display width increase");
	vesa_bios_modelist_in_info = section->Get_bool("vesa vbe put modelist in vesa information");

	/* It turns out various S3 cards and their BIOSes vary their preference for 32bpp or 24bpp.
	 * For example most S3 drivers are perfectly happy with and will prefer 32bpp, but S3 ViRGE drivers
	 * for some reason prefer 24bpp, and they even expect the base VESA BIOS modes to be 24bpp.
	 * The Windows drivers for ViRGE chipsets in fact do not even offer "Truecolor (32-bit)" even
	 * though the chipset documentation says that if you turn on "full streams processor operation"
	 * you can take your pick of either one, but then again, the Windows drivers don't have their
	 * own modesetting code anyway, they just call INT 10h VESA BIOS extensions to change video modes
	 * and make assumptions about video modes and formats. No attempt by the drivers are made to
	 * modify the primary streams processor during modesettings.
	 *
	 * This code does respond to those register writes even if not used yet, so maybe someday if
	 * we get Windows NT to work in DOSBox-X, then assuming the NT drivers do not call INT 10h,
	 * we can see a driver do the modesetting work itself and enhance emulation appropriately.
	 *
	 * Therefore "auto" as added as an option to forestall millions of users complaining about
	 * garbled displays whenever they select "16.7 million colors" in Windows 3.1 or "Truecolor (24-bit)"
	 * in Windows 95 with machine=svga_s3virge. It would be much easier to automatically select on auto
	 * than to answer the same question over and over with "just change this option to false if you're
	 * using machine=svga_s3virge".
	 *
	 * This automatic selection may cause DOS games that assume or require 32bpp to fail with
	 * machine=svga_s3virge, but then again, such games would likely fail the same way if run on
	 * real hardware with a real ViRGE PCI card. If your DOS game assumes things about the mode
	 * without using the "get mode" call that has been there since VBE 1.0, then your game
	 * deserves to misrender on screen and it's your fault. Fortunately by 1996 when the ViRGE
	 * chipset came out, most DOS games by that point were using the "get mode" call, except
	 * for some Demoscene coders who evidently did not have VESA BIOS documentation and had to
	 * guess and assume from what they could figure out. */
	str = section->Get_string("vesa vbe 1.2 modes are 32bpp");
	if (str == "true" || str == "1")
		vesa12_modes_32bpp = true;
	else if (str == "false" || str == "0")
		vesa12_modes_32bpp = false;
	else if (svgaCard == SVGA_S3Trio && s3Card >= S3_ViRGE && s3Card <= S3_ViRGEVX)
		vesa12_modes_32bpp = false; /* ViRGE Windows drivers only support 24bpp and they expect the base VESA modes to provide 24bpp */
	else
		vesa12_modes_32bpp = true;

	/* sanity check: "VBE 1.2 modes 32bpp" doesn't make any sense if neither 24bpp or 32bpp is enabled */
	if (!allow_vesa_32bpp && !allow_vesa_24bpp)
		vesa12_modes_32bpp = 0;
	/* sanity check: "VBE 1.2 modes 32bpp=true" doesn't make sense if the user disabled 32bpp */
	else if (vesa12_modes_32bpp && !allow_vesa_32bpp)
		vesa12_modes_32bpp = 0;
	/* sanity check: "VBE 1.2 modes 32bpp=false" doesn't make sense if the user disabled 24bpp */
	else if (!vesa12_modes_32bpp && !allow_vesa_24bpp && allow_vesa_32bpp)
		vesa12_modes_32bpp = 1;

	if (vga_force_refresh_rate > 0)
		LOG(LOG_VGA,LOG_NORMAL)("VGA forced refresh rate active = %.3f",vga_force_refresh_rate);

	vga.draw.resizing=false;

	vga_8bit_dac = false;
	enable_vga_8bit_dac = section->Get_bool("enable 8-bit dac");
	ignore_sequencer_blanking = section->Get_bool("ignore sequencer blanking");
	memio_complexity_optimization = section->Get_bool("memory io optimization 1");

	vga_render_on_demand = false;

	{
		const char *str = section->Get_string("scanline render on demand");
		if (!strcmp(str,"true") || !strcmp(str,"1"))
			vga_render_on_demand_user = 1;
		else if (!strcmp(str,"false") || !strcmp(str,"0"))
			vga_render_on_demand_user = 0;
		else
			vga_render_on_demand_user = -1;
	}

	if (memio_complexity_optimization)
		LOG_MSG("Memory I/O complexity optimization enabled aka option 'memory io optimization 1'. If the game or demo is unable to draw to the screen properly, set the option to false.");

	if (vga_render_on_demand_user > 0)
		LOG_MSG("'scanline render on demand' option is enabled. If this option breaks the game or demo effects or display, set the option to false.");
	else if (vga_render_on_demand_user < 0)
		LOG_MSG("The 'scanline render on demand' option is available and may provide a modest boost in video render performance if set to true.");

	vga_memio_lfb_delay = section->Get_bool("lfb vmemdelay");

	vga_memio_delay_ns = section->Get_int("vmemdelay");
	if (vga_memio_delay_ns < 0) {
		if (IS_EGAVGA_ARCH) {
			if (pcibus_enable) {
				/* some delay based on PCI bus protocol with frame start, turnaround, and burst transfer */
				double t = (1000000000.0 * clockdom_PCI_BCLK.freq_div * (1/*turnaround*/+1/*frame start*/+1/*burst*/-0.25/*fudge*/)) / clockdom_PCI_BCLK.freq;
				vga_memio_delay_ns = (int)floor(t);
			}
			else {
				/* very optimistic setting, ISA bus cycles are longer than 2, but also the 386/486/Pentium pipeline
				 * instruction decoding. so it's not a matter of sucking up enough CPU cycle counts to match the
				 * duration of a memory I/O cycle, because real hardware probably has another instruction decode
				 * going while it does it.
				 *
				 * this is long enough to fix some demo's raster effects to work properly but not enough to
				 * significantly bring DOS games to a crawl. Apparently, this also fixes Future Crew "Panic!"
				 * by making the shadebob take long enough to allow the 3D rotating dot object to finish it's
				 * routine just in time to become the FC logo, instead of sitting there waiting awkwardly
				 * for 3-5 seconds. */
				double t = (1000000000.0 * clockdom_ISA_BCLK.freq_div * 3.75) / clockdom_ISA_BCLK.freq;
				vga_memio_delay_ns = (int)floor(t);
			}
		}
		else if (machine == MCH_CGA || machine == MCH_HERC || machine == MCH_MDA) {
			/* default IBM PC/XT 4.77MHz timing. this is carefully tuned so that Trixter's CGA test program
			 * times our CGA emulation as having about 305KB/sec reading speed. */
			double t = (1000000000.0 * clockdom_ISA_OSC.freq_div * 143) / (clockdom_ISA_OSC.freq * 3);
			vga_memio_delay_ns = (int)floor(t);
		}
		else {
			/* dunno. pick something */
			double t = (1000000000.0 * clockdom_ISA_BCLK.freq_div * 6) / clockdom_ISA_BCLK.freq;
			vga_memio_delay_ns = (int)floor(t);
		}
	}

	LOG(LOG_VGA,LOG_DEBUG)("VGA memory I/O delay %uns",vga_memio_delay_ns);

	vbe_window_granularity = (unsigned int)section->Get_int("vbe window granularity") << 10u; /* KB to bytes */
	vbe_window_size = (unsigned int)section->Get_int("vbe window size") << 10u; /* KB to bytes */

	if (vbe_window_granularity != 0 && !bitop::ispowerof2(vbe_window_granularity))
		LOG(LOG_VGA,LOG_WARN)("User specified VBE window granularity is not a power of 2. May break some programs.");
	if (vbe_window_size != 0 && !bitop::ispowerof2(vbe_window_size))
		LOG(LOG_VGA,LOG_WARN)("User specified VBE window size is not a power of 2. May break some programs.");
	if (vbe_window_size != 0 && vbe_window_granularity != 0 && vbe_window_size < vbe_window_granularity)
		LOG(LOG_VGA,LOG_WARN)("VBE window size is less than window granularity, which prevents full access to video memory and may break some programs.");

	/* mainline compatible vmemsize (in MB)
	 * plus vmemsizekb for KB-level control.
	 * then we round up a page.
	 *
	 * FIXME: If PCjr/Tandy uses system memory as video memory,
	 *        can we get away with pointing at system memory
	 *        directly and not allocate a dedicated char[]
	 *        array for VRAM? Likewise for VGA emulation of
	 *        various motherboard chipsets known to "steal"
	 *        off the top of system RAM, like Intel and
	 *        Chips & Tech VGA implementations? */
	{
		int sz_m = section->Get_int("vmemsize");
		int sz_k = section->Get_int("vmemsizekb");

		if (sz_m >= 0 || sz_k > 0) {
			vga.mem.memsize  = _MB_bytes((unsigned int)sz_m);
			vga.mem.memsize += _KB_bytes((unsigned int)sz_k);
			vga.mem.memsize  = (vga.mem.memsize + 0xFFFu) & (~0xFFFu);
			/* mainline compatible: vmemsize == 0 means 512KB */
			if (vga.mem.memsize == 0) vga.mem.memsize = _KB_bytes(512);
			vga.mem.memsize_original = vga.mem.memsize;

			/* round up to the nearest power of 2 (TODO: Any video hardware that uses non-power-of-2 sizes?).
			 * A lot of DOSBox's VGA emulation code assumes power-of-2 VRAM sizes especially when wrapping
			 * memory addresses with (a & (vmemsize - 1)) type code. */
			if (!is_power_of_2(vga.mem.memsize)) {
				vga.mem.memsize = 1u << (int_log2(vga.mem.memsize) + 1u);
				LOG(LOG_VGA,LOG_WARN)("VGA RAM size requested is not a power of 2, rounding up to %uKB",vga.mem.memsize>>10);
			}
		}
		else {
			vga.mem.memsize_original = 0;
			vga.mem.memsize = 0; /* machine-specific code will choose below */
		}
	}

	/* sanity check according to adapter type.
	 * FIXME: Again it was foolish for DOSBox to standardize on machine=
	 * for selecting machine type AND video card. */
	switch (machine) {
		case MCH_HERC:
			if (hercCard >= HERC_InColor) {
				if (vga.mem.memsize < _KB_bytes(256)) vga.mem.memsize = _KB_bytes(256);
			}
			else {
				if (vga.mem.memsize < _KB_bytes(64)) vga.mem.memsize = _KB_bytes(64);
			}
			break;
		case MCH_MDA:
			if (vga.mem.memsize < _KB_bytes(4)) vga.mem.memsize = _KB_bytes(4);
			break;
		case MCH_CGA:
			if (vga.mem.memsize < _KB_bytes(16)) vga.mem.memsize = _KB_bytes(16);
			break;
		case MCH_TANDY:
		case MCH_PCJR:
			if (vga.mem.memsize < _KB_bytes(128)) vga.mem.memsize = _KB_bytes(128); /* FIXME: Right? */
			break;
		case MCH_EGA:
			// EGA cards supported either 64KB, 128KB or 256KB.
			if (vga.mem.memsize == 0)              vga.mem.memsize = _KB_bytes(256);//default
			else if (vga.mem.memsize <= _KB_bytes(64))  vga.mem.memsize = _KB_bytes(64);
			else if (vga.mem.memsize <= _KB_bytes(128)) vga.mem.memsize = _KB_bytes(128);
			else                                        vga.mem.memsize = _KB_bytes(256);
			break;
		case MCH_VGA:
			// TODO: There are reports of VGA cards that have less than 256KB in the early days of VGA.
			//       How does that work exactly, especially when 640x480 requires about 37KB per plane?
			//       Did these cards have some means to chain two bitplanes odd/even in the same way
			//       than EGA did it?
			if (vga.mem.memsize != 0 || svgaCard == SVGA_None) {
				if (vga.mem.memsize < _KB_bytes(256)) vga.mem.memsize = _KB_bytes(256);
			}
			break;
		case MCH_AMSTRAD:
			if (vga.mem.memsize < _KB_bytes(64)) vga.mem.memsize = _KB_bytes(64); /* FIXME: Right? */
			break;
		case MCH_PC98:
			if (vga.mem.memsize < _KB_bytes(544)) vga.mem.memsize = _KB_bytes(544); /* 544 = 512KB graphics + 32KB text */
			break;
		case MCH_MCGA:
			if (vga.mem.memsize < _KB_bytes(64)) vga.mem.memsize = _KB_bytes(64);
			break;
		default:
			E_Exit("Unexpected machine");
	}

	/* I'm sorry, emulating 640x350 4-color chained EGA graphics is
	 * harder than I thought and would require revision of quite a
	 * bit of VGA planar emulation to update only bitplane 0 and 2
	 * in such a manner. --J.C. */
	if (IS_EGA_ARCH && vga.mem.memsize < _KB_bytes(128))
		LOG_MSG("WARNING: EGA 64KB emulation is very experimental and not well supported");

	/* If video memory is limited by bank switching, then reduce video memory */
	if (vbe_window_granularity != 0 && !IS_PC98_ARCH) {
		const uint32_t sz = GetReportedVideoMemorySize();

		LOG(LOG_VGA,LOG_NORMAL)("Video RAM size %uKB exceeds maximum possible %uKB given VBE window granularity, reducing",
				vga.mem.memsize>>10,(unsigned int)(sz>>10ul));

		/* reduce by half, until video memory is reported or larger but not more than 2x */
		while (vga.mem.memsize > _KB_bytes(512) && (vga.mem.memsize/2ul) >= sz)
			vga.mem.memsize /= 2u;
	}

	// prepare for transition
	if (want_fm_towns) {
		if (vga.mem.memsize < _KB_bytes(640)) vga.mem.memsize = _KB_bytes(640); /* "640KB of RAM, 512KB VRAM and 128KB sprite RAM" */
	}

	if (!IS_PC98_ARCH)
		SVGA_Setup_Driver();        // svga video memory size is set here, possibly over-riding the user's selection

	// NTS: This is WHY the memory size must be a power of 2
	vga.mem.memmask = bitop::rounduppow2mask(vga.mem.memsize - 1u);

	LOG(LOG_VGA,LOG_NORMAL)("Video RAM: %uKB (mask 0x%x)",vga.mem.memsize>>10,(unsigned int)vga.mem.memmask);

	// TODO: If S3 emulation, and linear framebuffer bumps up against the CPU memalias limits,
	//       trim Video RAM to fit (within reasonable limits) or else E_Exit() to let the user
	//       know of impossible constraints.

	mainMenu.get_item("debug_pageflip").check(enable_page_flip_debugging_marker).refresh_item(mainMenu);
	mainMenu.get_item("debug_retracepoll").check(enable_vretrace_poll_debugging_marker).refresh_item(mainMenu);

	VGA_SetupMemory();      // memory is allocated here
	if (!IS_PC98_ARCH) {
		VGA_SetupMisc();
		VGA_SetupDAC();
		VGA_SetupGFX();
		VGA_SetupSEQ();
		VGA_SetupAttr();
		VGA_SetupOther();
		VGA_SetupXGA();
		VGA_SetClock(0,CLK_25);
		VGA_SetClock(1,CLK_28);
		/* Generate tables */
		VGA_SetCGA2Table(0,1);
		VGA_SetCGA4Table(0,1,2,3);

		if (machine == MCH_HERC || machine == MCH_MDA) {
			/* alternate table for blend mode, expect mapping 0=black 1-7=gray 8=black 9-f=white
			 * for use with blend code that reads this table and sums the output with a delayed version of the same */
			const uint8_t total[2] = {0x0,0x05}; /* blending will yield {0,5,10} which is sufficient to recreate the effect */
			for (Bitu i=0;i<16u;i++) {
				HercBlend_2_Table[i]=
#ifdef WORDS_BIGENDIAN
					((Bitu)total[(i >> 0u) & 1u] << 0u  ) | ((Bitu)total[(i >> 1u) & 1u] << 8u  ) |
					((Bitu)total[(i >> 2u) & 1u] << 16u ) | ((Bitu)total[(i >> 3u) & 1u] << 24u );
#else 
				((Bitu)total[(i >> 3u) & 1u] << 0u  ) | ((Bitu)total[(i >> 2u) & 1u] << 8u  ) |
					((Bitu)total[(i >> 1u) & 1u] << 16u ) | ((Bitu)total[(i >> 0u) & 1u] << 24u );
#endif
			}
		}
	}

	Section_prop * section2=static_cast<Section_prop *>(control->GetSection("vsync"));

	const char * vsyncmodestr;
	vsyncmodestr=section2->Get_string("vsyncmode");
	void change_output(int output);
	change_output(9);
	VGA_VsyncUpdateMode(VGA_Vsync_Decode(vsyncmodestr));

	const char * vsyncratestr;
	vsyncratestr=section2->Get_string("vsyncrate");
	double vsyncrate=70;
	if (!strcasecmp(vsyncmodestr,"host")) {
#if defined (WIN32)
		DEVMODE devmode;

		if (EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &devmode))
			vsyncrate=devmode.dmDisplayFrequency;
		else
			sscanf(vsyncratestr,"%lf",&vsyncrate);
#endif
	}
	else {
		sscanf(vsyncratestr,"%lf",&vsyncrate);
	}

	vsync.period = (1000.0F)/vsyncrate;

	if (IS_PC98_ARCH) {
		void VGA_OnEnterPC98(Section *sec);
		void VGA_OnEnterPC98_phase2(Section *sec);
		void PC98_FM_OnEnterPC98(Section *sec);

		VGA_OnEnterPC98(NULL);
		VGA_OnEnterPC98_phase2(NULL);

		// TODO: Move to separate file
		PC98_FM_OnEnterPC98(NULL);
	}
}

void VGA_OnEnterPC98(Section *sec) {
	(void)sec;//UNUSED
	VGA_UnsetupMisc();
	VGA_UnsetupAttr();
	VGA_UnsetupDAC();
	VGA_UnsetupGFX();
	VGA_UnsetupSEQ();

	LOG_MSG("PC-98: GDC is running at %.1fMHz.",gdc_5mhz_mode ? 5.0 : 2.5);

	pc98_egc_srcmask[0] = 0xFF;
	pc98_egc_srcmask[1] = 0xFF;
	pc98_egc_maskef[0] = 0xFF;
	pc98_egc_maskef[1] = 0xFF;
	pc98_egc_mask[0] = 0xFF;
	pc98_egc_mask[1] = 0xFF;

	for (unsigned int i=0;i < 8;i++)
		pc98_pal_digital[i] = i;

	for (unsigned int i=0;i < 8;i++) {
		pc98_pal_analog[(i*3) + 0] = (i & 4) ? 0x0F : 0x00;
		pc98_pal_analog[(i*3) + 1] = (i & 2) ? 0x0F : 0x00;
		pc98_pal_analog[(i*3) + 2] = (i & 1) ? 0x0F : 0x00;

		if (i != 0) {
			pc98_pal_analog[((i+8)*3) + 0] = (i & 4) ? 0x0A : 0x00;
			pc98_pal_analog[((i+8)*3) + 1] = (i & 2) ? 0x0A : 0x00;
			pc98_pal_analog[((i+8)*3) + 2] = (i & 1) ? 0x0A : 0x00;
		}
		else {
			pc98_pal_analog[((i+8)*3) + 0] = 0x07;
			pc98_pal_analog[((i+8)*3) + 1] = 0x07;
			pc98_pal_analog[((i+8)*3) + 2] = 0x07;
		}
	}

	for (unsigned int i=0;i < 256;i++) {
		pc98_pal_vga[(i*3)+0] = i;
		pc98_pal_vga[(i*3)+1] = i;
		pc98_pal_vga[(i*3)+2] = i;
	}

	pc98_update_palette();

	{
		for (unsigned int i=0;i < 8;i++) {
			unsigned char r = (i & 2) ? 255 : 0;
			unsigned char g = (i & 4) ? 255 : 0;
			unsigned char b = (i & 1) ? 255 : 0;

			if (GFX_bpp >= 24) /* FIXME: Assumes 8:8:8. What happens when desktops start using the 10:10:10 format? */
				pc98_text_palette[i] = ((Bitu)(((Bitu)b << GFX_Bshift) + ((Bitu)g << GFX_Gshift) + ((Bitu)r << GFX_Rshift) + (Bitu)GFX_Amask));
			else {
				/* FIXME: PC-98 mode renders as 32bpp regardless (at this time), so we have to fake 32bpp order */
				/*        Since PC-98 itself has 4-bit RGB precision, it might be best to offer a 16bpp rendering mode,
				 *        or even just have PC-98 mode stay in 16bpp entirely. */
				if (GFX_Bshift == 0)
					pc98_text_palette[i] = (Bitu)(((Bitu)b <<  0U) + ((Bitu)g <<  8U) + ((Bitu)r << 16U));
				else
					pc98_text_palette[i] = (Bitu)(((Bitu)b << 16U) + ((Bitu)g <<  8U) + ((Bitu)r <<  0U));
			}
		}
	}

	pc98_gdc_tile_counter=0;
	pc98_gdc_modereg=0;
	for (unsigned int i=0;i < 4;i++) pc98_gdc_tiles[i].w = 0;

	vga.dac.pel_mask = 0xFF;
	vga.crtc.maximum_scan_line = 15;

	/* 200-line tradition on PC-98 seems to be to render only every other scanline */
	pc98_graphics_hide_odd_raster_200line = true;
	pc98_256kb_boundary = false;         /* port 6Ah command 68h/69h */

	// as a transition to PC-98 GDC emulation, move VGA alphanumeric buffer
	// down to A0000-AFFFFh.
	gdc_analog = false;
	pc98_gdc_vramop &= ~(1 << VOPBIT_ANALOG);
	gfx(miscellaneous) &= ~0x0C; /* bits[3:2] = 0 to map A0000-BFFFF */
	VGA_DetermineMode();
	VGA_SetupHandlers();
	VGA_DAC_UpdateColorPalette();
	INT10_PC98_CurMode_Relocate(); /* make sure INT 10h knows */

	if(!pc98_31khz_mode) { /* Set up 24KHz hsync 56.42Hz rate */
		vga.crtc.horizontal_total = 106 - 5;
		vga.crtc.vertical_total = (440 - 2) & 0xFF;
		vga.crtc.end_vertical_blanking = (440 - 2 - 8) & 0xFF; // FIXME
		vga.crtc.vertical_retrace_start = (440 - 2 - 30) & 0xFF; // FIXME
		vga.crtc.vertical_retrace_end = (440 - 2 - 28) & 0xFF; // FIXME
		vga.crtc.start_vertical_blanking = (400 + 8) & 0xFF; // FIXME
		vga.crtc.overflow |=  0x01;
		vga.crtc.overflow &= ~0x20;
	} else { //Set up 31-KHz mode. Values guessed according to other 640x400 modes in int10_modes.cpp.
		//TODO: Find the right values by inspecting a real PC-9821 system.
		vga.crtc.horizontal_total = 100 - 5;
		vga.crtc.vertical_total = (449 - 2) & 0xFF;
		vga.crtc.end_vertical_blanking = (449 - 2 - 8) & 0xFF; // FIXME
		vga.crtc.vertical_retrace_start = (449 - 2 - 30) & 0xFF; // FIXME
		vga.crtc.vertical_retrace_end = (449 - 2 - 28) & 0xFF; // FIXME
		vga.crtc.start_vertical_blanking = (400 + 8) & 0xFF; // FIXME
		vga.crtc.overflow |=  0x01;
		vga.crtc.overflow &= ~0x20;
	}

	/* 8-char wide mode. change to 25MHz clock to match. */
	vga.config.addr_shift = 0;
	seq(clocking_mode) |= 1; /* 8-bit wide char */
	vga.misc_output &= ~0x0C; /* bits[3:2] = 0 25MHz clock */

	/* PC-98 seems to favor a block cursor */
	vga.draw.cursor.enabled = true;
	crtc(cursor_start) = 0;
	vga.draw.cursor.sline = 0;
	crtc(cursor_end) = 15;
	vga.draw.cursor.eline = 15;

	/* now, switch to PC-98 video emulation */
	for (unsigned int i=0;i < 16;i++) VGA_ATTR_SetPalette(i,i);
	for (unsigned int i=0;i < 16;i++) vga.dac.combine[i] = i;

	VGA_SetMode(M_PC98);
	assert(vga.mem.memsize >= 0x80000);
	memset(vga.mem.linear,0,0x80000);

	VGA_StartResize();
}

void updateGDCpartitions4(bool enable) {
	pc98_allow_4_display_partitions = enable;
	pc98_gdc[GDC_SLAVE].display_partition_mask = pc98_allow_4_display_partitions ? 3 : 1;
}

void VGA_OnEnterPC98_phase2(Section *sec) {
	(void)sec;//UNUSED
	VGA_SetupHandlers();

	/* GDC 2.5/5.0MHz setting is also reflected in BIOS data area and DIP switch registers */
	gdc_5mhz_mode_update_vars();

	/* ARTIC (A Relative Time Indication Counter) at 0x5C and 0x5E */
	if (pc98_timestamp5c) {
		IO_RegisterReadHandler(0x5C,pc98_read_artic,IO_MB);
		IO_RegisterReadHandler(0x5D,pc98_read_artic,IO_MB);
		IO_RegisterReadHandler(0x5E,pc98_read_artic,IO_MB);
		IO_RegisterReadHandler(0x5F,pc98_read_artic,IO_MB);
		IO_RegisterReadHandler(0x5C,pc98_read_artic,IO_MW);
		IO_RegisterReadHandler(0x5E,pc98_read_artic,IO_MW);
	}

	/* delay I/O port at 0x5F (0.6us) */
	IO_RegisterWriteHandler(0x5F,pc98_wait_write,IO_MB);

	/* master GDC at 0x60-0x6F (even)
	 * slave GDC at 0xA0-0xAF (even) */
	for (unsigned int i=0x60;i <= 0xA0;i += 0x40) {
		for (unsigned int j=0;j < 0x10;j += 2) {
			IO_RegisterWriteHandler(i+j,pc98_gdc_write,IO_MB);
			IO_RegisterReadHandler(i+j,pc98_gdc_read,IO_MB);
		}
	}

	/* initial implementation of I/O ports 9A0h-9AEh even */
	IO_RegisterReadHandler(0x9A0,pc98_read_9a0,IO_MB);
	IO_RegisterWriteHandler(0x9A0,pc98_write_9a0,IO_MB);

	/* 9A8h which controls 24khz/31khz mode */
	IO_RegisterReadHandler(0x9A8,pc98_read_9a8,IO_MB);
	IO_RegisterWriteHandler(0x9A8,pc98_write_9a8,IO_MB);

	/* There are some font character RAM controls at 0xA1-0xA5 (odd)
	 * combined with A4000-A4FFF. Found by unknown I/O tracing in DOSBox-X
	 * and by tracing INT 18h AH=1Ah on an actual system using DEBUG.COM.
	 *
	 * If I find documentation on what exactly these ports are, I will
	 * list them as such.
	 *
	 * Some games (Touhou Project) load font RAM directly through these
	 * ports instead of using the BIOS. */
	for (unsigned int i=0xA1;i <= 0xA9;i += 2) {
		IO_RegisterWriteHandler(i,pc98_a1_write,IO_MB);
	}
	/* Touhou Project appears to read font RAM as well */
	IO_RegisterReadHandler(0xA9,pc98_a1_read,IO_MB);

	/* CRTC at 0x70-0x7F (even) */
	for (unsigned int j=0x70;j < 0x80;j += 2) {
		IO_RegisterWriteHandler(j,pc98_crtc_write,IO_MB);
		IO_RegisterReadHandler(j,pc98_crtc_read,IO_MB);
	}

	/* EGC at 0x4A0-0x4AF (even).
	 * All I/O ports are 16-bit.
	 * NTS: On real hardware, doing 8-bit I/O on these ports will often hang the system. */
	for (unsigned int i=0;i < 0x10;i += 2) {
		IO_RegisterWriteHandler(i+0x4A0,pc98_egc4a0_write_warning,IO_MB);
		IO_RegisterWriteHandler(i+0x4A0,pc98_egc4a0_write,        IO_MW);
		IO_RegisterWriteHandler(i+0x4A1,pc98_egc4a0_write_warning,IO_MB);
		IO_RegisterWriteHandler(i+0x4A1,pc98_egc4a0_write_warning,IO_MW);

		IO_RegisterReadHandler(i+0x4A0,pc98_egc4a0_read_warning,IO_MB);
		IO_RegisterReadHandler(i+0x4A0,pc98_egc4a0_read,        IO_MW);
		IO_RegisterReadHandler(i+0x4A1,pc98_egc4a0_read_warning,IO_MB);
		IO_RegisterReadHandler(i+0x4A1,pc98_egc4a0_read_warning,IO_MW);
	}

	pc98_gdc[GDC_MASTER].master_sync = true;
	pc98_gdc[GDC_MASTER].display_enable = true;
	pc98_gdc[GDC_MASTER].row_height = 16;
	pc98_gdc[GDC_MASTER].display_pitch = 80;
	pc98_gdc[GDC_MASTER].active_display_words_per_line = 80;
	pc98_gdc[GDC_MASTER].display_partition_mask = 3;

	pc98_gdc[GDC_SLAVE].master_sync = false;
	pc98_gdc[GDC_SLAVE].display_enable = false;//FIXME
	pc98_gdc[GDC_SLAVE].row_height = 1;
	pc98_gdc[GDC_SLAVE].display_pitch = gdc_5mhz_mode ? 80u : 40u;
	pc98_gdc[GDC_SLAVE].display_partition_mask = pc98_allow_4_display_partitions ? 3 : 1;

	const unsigned char *gdcsync_m;
	const unsigned char *gdcsync_s;

	if (!pc98_31khz_mode) {
		gdcsync_m = gdc_defsyncm24;
		gdcsync_s = gdc_defsyncs24;
	}
	else {
		gdcsync_m = gdc_defsyncm31;
		gdcsync_s = gdc_defsyncs31;
	}

	pc98_gdc[GDC_MASTER].write_fifo_command(0x0F/*sync DE=1*/);
	for (unsigned int i=0;i < 8;i++)
		pc98_gdc[GDC_MASTER].write_fifo_param(gdcsync_m[i]);
	pc98_gdc[GDC_MASTER].force_fifo_complete();

	pc98_gdc[GDC_SLAVE].write_fifo_command(0x0F/*sync DE=1*/);
	for (unsigned int i=0;i < 8;i++)
		pc98_gdc[GDC_SLAVE].write_fifo_param(gdcsync_s[i]);
	pc98_gdc[GDC_SLAVE].force_fifo_complete();

	VGA_StartResize();
}

void VGA_Destroy(Section*) {
	void PC98_FM_Destroy(Section *sec);
	PC98_FM_Destroy(NULL);
}

bool debugpollvga_pf_menu_callback(DOSBoxMenu * const xmenu, DOSBoxMenu::item * const menuitem) {
	(void)xmenu;//UNUSED
	(void)menuitem;//UNUSED

	enable_page_flip_debugging_marker = !enable_page_flip_debugging_marker;
	mainMenu.get_item("debug_pageflip").check(enable_page_flip_debugging_marker).refresh_item(mainMenu);

	return true;
}

bool debugpollvga_rtp_menu_callback(DOSBoxMenu * const xmenu, DOSBoxMenu::item * const menuitem) {
	(void)xmenu;//UNUSED
	(void)menuitem;//UNUSED

	enable_vretrace_poll_debugging_marker = !enable_vretrace_poll_debugging_marker;
	mainMenu.get_item("debug_retracepoll").check(enable_vretrace_poll_debugging_marker).refresh_item(mainMenu);

	return true;
}

void VGA_Init() {
	Bitu i,j;

	enveten = false;
	vga.mode=M_ERROR;           //For first init
	vga.other.mcga_mode_control = 0;

	vga.config.chained = false;

	vga.herc.xMode = 0;
	vga.herc.underline = 0xD;
	vga.herc.strikethrough = 0xD;
	vga.herc.latch = 0;
	vga.herc.exception = 0x20;
	vga.herc.planemask_protect = 0;
	vga.herc.planemask_visible = 0xF;
	vga.herc.maskpolarity = 0xFF;
	vga.herc.write_mode = 0;
	vga.herc.dont_care = 0;
	vga.herc.fgcolor = 0xF;
	vga.herc.bgcolor = 0x0;
	vga.herc.latchprotect = 0;
	vga.herc.palette_index = 0;
	for (unsigned int i=0;i < 8;i++) vga.herc.palette[i] = i;
	for (unsigned int i=8;i < 16;i++) vga.herc.palette[i] = i + 0x30;
	vga.draw.render_step = 0;
	vga.draw.render_max = 1;

	vga.tandy.draw_base = NULL;
	vga.tandy.mem_base = NULL;
	LOG(LOG_MISC,LOG_DEBUG)("Initializing VGA");
	LOG(LOG_MISC,LOG_DEBUG)("Render scaler maximum resolution is %u x %u",SCALER_MAXWIDTH,SCALER_MAXHEIGHT);

	/* the purpose of this is so that, if everything is crammed up at the top to make room for system RAM, the S3 and 3Dfx do not conflict */
	if (IS_VGA_ARCH && svgaCard != SVGA_None)
		assigned_lfb = MEM_HardwareAllocate("VGA",32ul << 20ul/*32MB*/);
	else
		LOG(LOG_MISC,LOG_DEBUG)("Emulation does not require allocating or assigning any LFB");

	VGA_TweakUserVsyncOffset(0.0f);

	for (i=0;i<256;i++) {
		ExpandTable[i]=(Bitu)(i + (i << 8u) + (i << 16u) + (i << 24u));
	}
	for (i=0;i<16;i++) {
		TXT_FG_Table[i]=(Bitu)(i + (i << 8u) + (i << 16u) + (i << 24u));
		TXT_BG_Table[i]=(Bitu)(i + (i << 8u) + (i << 16u) + (i << 24u));
#ifdef WORDS_BIGENDIAN
		FillTable[i]=
			((i & 1u) ? 0xff000000u : 0u) |
			((i & 2u) ? 0x00ff0000u : 0u) |
			((i & 4u) ? 0x0000ff00u : 0u) |
			((i & 8u) ? 0x000000ffu : 0u) ;
		TXT_Font_Table[i]=
			((i & 1u) ? 0x000000ffu : 0u) |
			((i & 2u) ? 0x0000ff00u : 0u) |
			((i & 4u) ? 0x00ff0000u : 0u) |
			((i & 8u) ? 0xff000000u : 0u) ;
#else 
		FillTable[i]=
			((i & 1u) ? 0x000000ffu : 0u) |
			((i & 2u) ? 0x0000ff00u : 0u) |
			((i & 4u) ? 0x00ff0000u : 0u) |
			((i & 8u) ? 0xff000000u : 0u) ;
		TXT_Font_Table[i]=  
			((i & 1u) ? 0xff000000u : 0u) |
			((i & 2u) ? 0x00ff0000u : 0u) |
			((i & 4u) ? 0x0000ff00u : 0u) |
			((i & 8u) ? 0x000000ffu : 0u) ;
#endif
	}
	for (j=0;j<4;j++) {
		for (i=0;i<16;i++) {
#ifdef WORDS_BIGENDIAN
			Expand16Table[j][i] =
				((i & 1u) ? 1u <<        j  : 0u) |
				((i & 2u) ? 1u << (8u +  j) : 0u) |
				((i & 4u) ? 1u << (16u + j) : 0u) |
				((i & 8u) ? 1u << (24u + j) : 0u);
#else
			Expand16Table[j][i] =
				((i & 1u) ? 1u << (24u + j) : 0u) |
				((i & 2u) ? 1u << (16u + j) : 0u) |
				((i & 4u) ? 1u << (8u  + j) : 0u) |
				((i & 8u) ? 1u <<        j  : 0u);
#endif
		}
	}

	mainMenu.alloc_item(DOSBoxMenu::item_type_id,"debug_pageflip").set_text("Page flip debug line").set_callback_function(debugpollvga_pf_menu_callback);
	mainMenu.alloc_item(DOSBoxMenu::item_type_id,"debug_retracepoll").set_text("Retrace poll debug line").set_callback_function(debugpollvga_rtp_menu_callback);

	AddExitFunction(AddExitFunctionFuncPair(VGA_Destroy));
	AddVMEventFunction(VM_EVENT_RESET,AddVMEventFunctionFuncPair(VGA_Reset));
}

// Store font
void JEGA_writeFont() {
	jega.RSTAT &= ~0x02;
	Bitu chr = jega.RDFFB;
	Bitu chr_2 = jega.RDFSB;
	// Check the char code is in Wide charset of Shift-JIS
	if ((chr >= 0x40 && chr <= 0x7e) || (chr >= 0x80 && chr <= 0xfc)) {
		if (jega.fontIndex >= 32) jega.fontIndex = 0;
		chr <<= 8;
		//fix vertical char position
		chr |= chr_2;
		if (jega.fontIndex < 16)
			// read font from register and store it
			jfont_dbcs_16[chr * 32 + jega.fontIndex * 2] = jega.RDFAP;// w16xh16 font
		else
			jfont_dbcs_16[chr * 32 + (jega.fontIndex - 16) * 2 + 1] = jega.RDFAP;// w16xh16 font
	}
	else
	{
		if (jega.fontIndex >= 19) jega.fontIndex = 0;
		jfont_sbcs_19[chr * 19 + jega.fontIndex] = jega.RDFAP;// w16xh16 font
	}
	jega.fontIndex++;
	jega.RSTAT |= 0x02;
}

// Read font
void JEGA_readFont() {
	jega.RSTAT &= ~0x02;
	Bitu chr = jega.RDFFB;
	Bitu chr_2 = jega.RDFSB;
	// Check the char code is in Wide charset of Shift-JIS
	if ((chr >= 0x40 && chr <= 0x7e) || (chr >= 0x80 && chr <= 0xfc)) {
		if (jega.fontIndex >= 32) jega.fontIndex = 0;
		chr <<= 8;
		//fix vertical char position
		chr |= chr_2;
		if (jega.fontIndex < 16)
			// get font and set to register
			jega.RDFAP = jfont_dbcs_16[chr * 32 + jega.fontIndex * 2];// w16xh16 font
		else
			jega.RDFAP = jfont_dbcs_16[chr * 32 + (jega.fontIndex - 16) * 2 + 1];
	}
	else
	{
		if (jega.fontIndex >= 19) jega.fontIndex = 0;
		jega.RDFAP = jfont_sbcs_19[chr * 19 + jega.fontIndex];// w16xh16 font
	}
	jega.fontIndex++;
	jega.RSTAT |= 0x02;
}

void write_p3d5_jega(Bitu reg, Bitu val, Bitu iolen) {
    (void)iolen;//UNUSED
	switch (reg) {
	case 0xb9://Mode register 1
		jega.RMOD1 = val;
		break;
	case 0xba://Mode register 2
		jega.RMOD2 = val;
		break;
	case 0xbb://ANK Group sel
		jega.RDAGS = val;
		break;
	case 0xbc:// Font access first byte
		if (jega.RDFFB != val) {
			jega.RDFFB = val;
			jega.fontIndex = 0;
		}
		break;
	case 0xbd:// Font access Second Byte
		if (jega.RDFSB != val) {
			jega.RDFSB = val;
			jega.fontIndex = 0;
		}
		break;
	case 0xbe:// Font Access Pattern
		jega.RDFAP = val;
		JEGA_writeFont();
		break;
	//case 0x09:// end scan line
	//	jega.RPESL = val;
	//	break;
	//case 0x14:// under scan line
	//	jega.RPULP = val;
	//	break;
	case 0xdb:
		jega.RPSSC = val;
		break;
	case 0xd9:
		jega.RPSSU = val;
		break;
	case 0xda:
		jega.RPSSL = val;
		break;
	case 0xdc://super imposed (only AX-2 system, not implemented)
		jega.RPPAJ = val;
		break;
	case 0xdd:
		jega.RCMOD = val;
		break;
	//case 0x0e://Cursor location Upper bits
	//	jega.RCCLH = val;
	//	break;
	//case 0x0f://Cursor location Lower bits
	//	jega.RCCLL = val;
	//	break;
	//case 0x0a://Cursor Start Line
	//	jega.RCCSL = val;
	//	break;
	//case 0x0b://Cursor End Line
	//	jega.RCCEL = val;
	//	break;
	case 0xde://Cursor Skew control
		jega.RCSKW = val;
		break;
	case 0xdf://Unused?
		jega.ROMSL = val;
		break;
	case 0xbf://font r/w register
		jega.RSTAT = val;
		break;
	default:
		LOG(LOG_VGAMISC, LOG_NORMAL)("VGA:GFX:JEGA:Write to illegal index %2X", (unsigned int)reg);
		break;
	}
}
//CRT Control Register can be read from I/O 3D5h, after setting index at I/O 3D4h
Bitu read_p3d5_jega(Bitu reg, Bitu iolen) {
    (void)iolen;//UNUSED
	switch (reg) {
	case 0xb9:
		return jega.RMOD1;
	case 0xba:
		return jega.RMOD2;
	case 0xbb:
		return jega.RDAGS;
	case 0xbc:// BCh RDFFB Font access First Byte
		return jega.RDFFB;
	case 0xbd:// BDh RDFFB Font access Second Byte
		return jega.RDFSB;
	case 0xbe:// BEh RDFAP Font Access Pattern
		JEGA_readFont();
		return jega.RDFAP;
	//case 0x09:
	//	return jega.RPESL;
	//case 0x14:
	//	return jega.RPULP;
	case 0xdb:
		return jega.RPSSC;
	case 0xd9:
		return jega.RPSSU;
	case 0xda:
		return jega.RPSSL;
	case 0xdc:
		return jega.RPPAJ;
	case 0xdd:
		return jega.RCMOD;
	//case 0x0e:
	//	return jega.RCCLH;
	//case 0x0f:
	//	return jega.RCCLL;
	//case 0x0a:
	//	return jega.RCCSL;
	//case 0x0b:
	//	return jega.RCCEL;
	case 0xde:
		return jega.RCSKW;
	case 0xdf:
		return jega.ROMSL;
	case 0xbf:
		return 0x03;//Font always read/writeable
	default:
		LOG(LOG_VGAMISC, LOG_NORMAL)("VGA:GFX:JEGA:Read from illegal index %2X", (unsigned int)reg);
		break;
	}
	return 0x0;
}

void JEGA_setupAX(void) {
	memset(&jega, 0, sizeof(JEGA_DATA));
	jega.RMOD1 = 0xC8;//b9: Mode register 1
	jega.RMOD2 = 0x00;//ba: Mode register 2
	jega.RDAGS = 0x00;//bb: ANK Group sel (not implemented)
	jega.RDFFB = 0x00;//bc: Font access first byte
	jega.RDFSB = 0x00;//bd: second
	jega.RDFAP = 0x00;//be: Font Access Pattern
	jega.RPESL = 0x00;//09: end scan line (superseded by EGA)
	jega.RPULP = 0x00;//14: under scan line (superseded by EGA)
	jega.RPSSC = 1;//db: DBCS start scan line
	jega.RPSSU = 3;//d9: 2x DBCS upper start scan
	jega.RPSSL = 0;//da: 2x DBCS lower start scan
	jega.RPPAJ = 0x00;//dc: super imposed (only AX-2 system, not implemented)
	jega.RCMOD = 0x00;//dd: Cursor Mode (not implemented)
	jega.RCCLH = 0x00;//0e: Cursor location Upper bits (superseded by EGA)
	jega.RCCLL = 0x00;//0f: Cursor location Lower bits (superseded by EGA)
	jega.RCCSL = 0x00;//0a: Cursor Start Line (superseded by EGA)
	jega.RCCEL = 0x00;//0b: Cursor End Line (superseded by EGA)
	jega.RCSKW = 0x20;//de: Cursor Skew control (not implemented)
	jega.ROMSL = 0x00;//df: Unused?
	jega.RSTAT = 0x03;//bf: Font register accessible status
	real_writeb(BIOSMEM_AX_SEG, BIOSMEM_AX_JPNSTATUS, 0);
	real_writeb(BIOSMEM_AX_SEG, BIOSMEM_AX_JEGA_RMOD1, jega.RMOD1);
	real_writeb(BIOSMEM_AX_SEG, BIOSMEM_AX_JEGA_RMOD2, jega.RMOD2);
	real_writeb(BIOSMEM_AX_SEG, BIOSMEM_AX_GRAPH_ATTR, 0);
	real_writeb(BIOSMEM_AX_SEG, BIOSMEM_AX_GRAPH_CHAR, 0);
	real_writeb(BIOSMEM_AX_SEG, BIOSMEM_AX_VTRAM_SEGADDR, 0);
	real_writeb(BIOSMEM_AX_SEG, BIOSMEM_AX_KBDSTATUS, 0x00);
}

void SVGA_Setup_JEGA(void) {
	JEGA_setupAX();
	svga.write_p3d5 = &write_p3d5_jega;
	svga.read_p3d5 = &read_p3d5_jega;

	// Adjust memory to 256K
	if (vga.mem.memsize < 256 * 1024) vga.mem.memsize = 256 * 1024;

	/* JEGA BIOS ROM signature for AX architecture
	To run MS-DOS, signature ("JA") must be 
	put at C000h:N*512-18+2 ;N=Number of ROM blocks (rom_base+2)
	*/
	PhysPt rom_base = PhysMake(0xc000, 0);
	phys_writeb(rom_base + 0x40 * 512 - 18 + 2, 'J');
	phys_writeb(rom_base + 0x40 * 512 - 18 + 3, 'A');
}

void SVGA_Setup_Driver(void) {
	memset(&svga, 0, sizeof(SVGA_Driver));

	switch(svgaCard) {
		case SVGA_S3Trio:
			SVGA_Setup_S3Trio();
			break;
		case SVGA_TsengET4K:
			SVGA_Setup_TsengET4K();
			break;
		case SVGA_TsengET3K:
			SVGA_Setup_TsengET3K();
			break;
		case SVGA_ParadisePVGA1A:
			SVGA_Setup_ParadisePVGA1A();
			break;
		case SVGA_ATI:
			SVGA_Setup_ATI();
			break;
		default:
			if (IS_JEGA_ARCH) SVGA_Setup_JEGA();
			break;
	}
}

extern void POD_Save_VGA_Draw( std::ostream & );
extern void POD_Save_VGA_Seq( std::ostream & );
extern void POD_Save_VGA_Attr( std::ostream & );
extern void POD_Save_VGA_Crtc( std::ostream & );
extern void POD_Save_VGA_Gfx( std::ostream & );
extern void POD_Save_VGA_Dac( std::ostream & );
extern void POD_Save_VGA_S3( std::ostream & );
extern void POD_Save_VGA_Other( std::ostream & );
extern void POD_Save_VGA_Memory( std::ostream & );
extern void POD_Save_VGA_Paradise( std::ostream & );
extern void POD_Save_VGA_Tseng( std::ostream & );
extern void POD_Save_VGA_XGA( std::ostream & );
extern void POD_Load_VGA_Draw( std::istream & );
extern void POD_Load_VGA_Seq( std::istream & );
extern void POD_Load_VGA_Attr( std::istream & );
extern void POD_Load_VGA_Crtc( std::istream & );
extern void POD_Load_VGA_Gfx( std::istream & );
extern void POD_Load_VGA_Dac( std::istream & );
extern void POD_Load_VGA_S3( std::istream & );
extern void POD_Load_VGA_Other( std::istream & );
extern void POD_Load_VGA_Memory( std::istream & );
extern void POD_Load_VGA_Paradise( std::istream & );
extern void POD_Load_VGA_Tseng( std::istream & );
extern void POD_Load_VGA_XGA( std::istream & );

//save state support
void *VGA_SetupDrawing_PIC_Event = (void*)((uintptr_t)VGA_SetupDrawing);


namespace {
class SerializeVga : public SerializeGlobalPOD {
public:
	SerializeVga() : SerializeGlobalPOD("Vga")
	{}

private:
	void getBytes(std::ostream& stream) override
	{
		uint32_t tandy_drawbase_idx, tandy_membase_idx;




		if( vga.tandy.draw_base == vga.mem.linear ) tandy_drawbase_idx=0xffffffff;
		else tandy_drawbase_idx = vga.tandy.draw_base - MemBase;

		if( vga.tandy.mem_base == vga.mem.linear ) tandy_membase_idx=0xffffffff;
		else tandy_membase_idx = vga.tandy.mem_base - MemBase;

		//********************************
		//********************************

		SerializeGlobalPOD::getBytes(stream);


		// - pure data
		WRITE_POD( &vga.mode, vga.mode );
		WRITE_POD( &vga.misc_output, vga.misc_output );

		
		// VGA_Draw.cpp
		POD_Save_VGA_Draw(stream);


		// - pure struct data
		WRITE_POD( &vga.config, vga.config );
		WRITE_POD( &vga.internal, vga.internal );


		// VGA_Seq.cpp / VGA_Attr.cpp / (..)
		POD_Save_VGA_Seq(stream);
		POD_Save_VGA_Attr(stream);
		POD_Save_VGA_Crtc(stream);
		POD_Save_VGA_Gfx(stream);
		POD_Save_VGA_Dac(stream);


		// - pure data
		WRITE_POD( &vga.latch, vga.latch );


		// VGA_S3.cpp
		POD_Save_VGA_S3(stream);


		// - pure struct data
		WRITE_POD( &vga.svga, vga.svga );
		WRITE_POD( &vga.herc, vga.herc );


		// - near-pure struct data
		WRITE_POD( &vga.tandy, vga.tandy );

		// - reloc data
		WRITE_POD( &tandy_drawbase_idx, tandy_drawbase_idx );
		WRITE_POD( &tandy_membase_idx, tandy_membase_idx );


		// vga_other.cpp / vga_memory.cpp
		POD_Save_VGA_Other(stream);
		POD_Save_VGA_Memory(stream);


		// - pure data
		//WRITE_POD( &vga.vmemwrap, vga.vmemwrap );


		// - static ptrs + 'new' data
		//uint8_t* fastmem;
		//uint8_t* fastmem_orgptr;

		// - 'new' data
		//WRITE_POD_SIZE( vga.fastmem_orgptr, sizeof(uint8_t) * ((vga.vmemsize << 1) + 4096 + 16) );


		// - pure data (variable on S3 card)
		WRITE_POD( &vga.mem.memsize, vga.mem.memsize );


#ifdef VGA_KEEP_CHANGES
		// - static ptr
		//uint8_t* map;

		// - 'new' data
		WRITE_POD_SIZE( vga.changes.map, sizeof(uint8_t) * (VGA_MEMORY >> VGA_CHANGE_SHIFT) + 32 );


		// - pure data
		WRITE_POD( &vga.changes.checkMask, vga.changes.checkMask );
		WRITE_POD( &vga.changes.frame, vga.changes.frame );
		WRITE_POD( &vga.changes.writeMask, vga.changes.writeMask );
		WRITE_POD( &vga.changes.active, vga.changes.active );
		WRITE_POD( &vga.changes.clearMask, vga.changes.clearMask );
		WRITE_POD( &vga.changes.start, vga.changes.start );
		WRITE_POD( &vga.changes.last, vga.changes.last );
		WRITE_POD( &vga.changes.lastAddress, vga.changes.lastAddress );
#endif


		// - pure data
		WRITE_POD( &vga.lfb.page, vga.lfb.page );
		WRITE_POD( &vga.lfb.addr, vga.lfb.addr );
		WRITE_POD( &vga.lfb.mask, vga.lfb.mask );

		// - static ptr
		//PageHandler *handler;


		// VGA_paradise.cpp / VGA_tseng.cpp / VGA_xga.cpp
		POD_Save_VGA_Paradise(stream);
		POD_Save_VGA_Tseng(stream);
		POD_Save_VGA_XGA(stream);
	}

	void setBytes(std::istream& stream) override
	{
		uint32_t tandy_drawbase_idx, tandy_membase_idx;



		//********************************
		//********************************

		SerializeGlobalPOD::setBytes(stream);


		// - pure data
		READ_POD( &vga.mode, vga.mode );
		READ_POD( &vga.misc_output, vga.misc_output );

		
		// VGA_Draw.cpp
		POD_Load_VGA_Draw(stream);


		// - pure struct data
		READ_POD( &vga.config, vga.config );
		READ_POD( &vga.internal, vga.internal );


		// VGA_Seq.cpp / VGA_Attr.cpp / (..)
		POD_Load_VGA_Seq(stream);
		POD_Load_VGA_Attr(stream);
		POD_Load_VGA_Crtc(stream);
		POD_Load_VGA_Gfx(stream);
		POD_Load_VGA_Dac(stream);


		// - pure data
		READ_POD( &vga.latch, vga.latch );


		// VGA_S3.cpp
		POD_Load_VGA_S3(stream);


		// - pure struct data
		READ_POD( &vga.svga, vga.svga );
		READ_POD( &vga.herc, vga.herc );


		// - near-pure struct data
		READ_POD( &vga.tandy, vga.tandy );

		// - reloc data
		READ_POD( &tandy_drawbase_idx, tandy_drawbase_idx );
		READ_POD( &tandy_membase_idx, tandy_membase_idx );


		// vga_other.cpp / vga_memory.cpp
		POD_Load_VGA_Other(stream);
		POD_Load_VGA_Memory(stream);


		// - pure data
		//READ_POD( &vga.vmemwrap, vga.vmemwrap );


		// - static ptrs + 'new' data
		//uint8_t* fastmem;
		//uint8_t* fastmem_orgptr;

		// - 'new' data
		//READ_POD_SIZE( vga.fastmem_orgptr, sizeof(uint8_t) * ((vga.vmemsize << 1) + 4096 + 16) );


		// - pure data (variable on S3 card)
		READ_POD( &vga.mem.memsize, vga.mem.memsize );


#ifdef VGA_KEEP_CHANGES
		// - static ptr
		//uint8_t* map;

		// - 'new' data
		READ_POD_SIZE( vga.changes.map, sizeof(uint8_t) * (VGA_MEMORY >> VGA_CHANGE_SHIFT) + 32 );


		// - pure data
		READ_POD( &vga.changes.checkMask, vga.changes.checkMask );
		READ_POD( &vga.changes.frame, vga.changes.frame );
		READ_POD( &vga.changes.writeMask, vga.changes.writeMask );
		READ_POD( &vga.changes.active, vga.changes.active );
		READ_POD( &vga.changes.clearMask, vga.changes.clearMask );
		READ_POD( &vga.changes.start, vga.changes.start );
		READ_POD( &vga.changes.last, vga.changes.last );
		READ_POD( &vga.changes.lastAddress, vga.changes.lastAddress );
#endif


		// - pure data
		READ_POD( &vga.lfb.page, vga.lfb.page );
		READ_POD( &vga.lfb.addr, vga.lfb.addr );
		READ_POD( &vga.lfb.mask, vga.lfb.mask );

		// - static ptr
		//PageHandler *handler;


		// VGA_paradise.cpp / VGA_tseng.cpp / VGA_xga.cpp
		POD_Load_VGA_Paradise(stream);
		POD_Load_VGA_Tseng(stream);
		POD_Load_VGA_XGA(stream);

		//********************************
		//********************************

		if( tandy_drawbase_idx == 0xffffffff ) vga.tandy.draw_base = vga.mem.linear;
		else vga.tandy.draw_base = MemBase + tandy_drawbase_idx;

		if( tandy_membase_idx == 0xffffffff ) vga.tandy.mem_base = vga.mem.linear;
		else vga.tandy.mem_base = MemBase + tandy_membase_idx;
	}
} dummy;
}
