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

#include "dosbox.h"
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
#include "util_units.h"
#include "control.h"
#include "pc98_cg.h"
#include "pc98_dac.h"
#include "pc98_gdc.h"
#include "pc98_gdc_const.h"
#include "mixer.h"

#include <string.h>
#include <stdlib.h>
#include <string>
#include <stdio.h>

using namespace std;

extern int                          vga_memio_delay_ns;
extern bool                         gdc_5mhz_mode;
extern bool                         enable_pc98_egc;
extern bool                         enable_pc98_grcg;
extern bool                         enable_pc98_16color;
extern bool                         GDC_vsync_interrupt;
extern uint8_t                      GDC_display_plane;

extern uint8_t                      pc98_gdc_tile_counter;
extern uint8_t                      pc98_gdc_modereg;
extern uint8_t                      pc98_gdc_vramop;
extern egc_quad                     pc98_gdc_tiles;

extern uint8_t                      pc98_egc_srcmask[2]; /* host given (Neko: egc.srcmask) */
extern uint8_t                      pc98_egc_maskef[2]; /* effective (Neko: egc.mask2) */
extern uint8_t                      pc98_egc_mask[2]; /* host given (Neko: egc.mask) */

VGA_Type vga;
SVGA_Driver svga;
int enableCGASnow;
int vesa_modelist_cap = 0;
int vesa_mode_width_cap = 0;
int vesa_mode_height_cap = 0;
bool vga_3da_polled = false;
bool vga_page_flip_occurred = false;
bool enable_page_flip_debugging_marker = false;
bool enable_vretrace_poll_debugging_marker = false;
bool vga_enable_hretrace_effects = false;
bool vga_enable_hpel_effects = false;
bool vga_enable_3C6_ramdac = false;
bool vga_sierra_lock_565 = false;
bool enable_vga_resize_delay = false;
bool vga_ignore_hdispend_change_if_smaller = false;
bool ignore_vblank_wraparound = false;
bool vga_double_buffered_line_compare = false;
bool pc98_allow_scanline_effect = true;
bool pc98_allow_4_display_partitions = false;
bool pc98_graphics_hide_odd_raster_200line = false;
bool gdc_analog = true;
bool pc98_31khz_mode = false;

unsigned int vga_display_start_hretrace = 0;
float hretrace_fx_avg_weight = 3;

bool allow_vesa_lowres_modes = true;
bool vesa12_modes_32bpp = true;
bool allow_vesa_32bpp = true;
bool allow_vesa_24bpp = true;
bool allow_vesa_16bpp = true;
bool allow_vesa_15bpp = true;
bool allow_vesa_8bpp = true;
bool allow_vesa_4bpp = true;
bool allow_vesa_tty = true;

void gdc_5mhz_mode_update_vars(void);
void pc98_port6A_command_write(unsigned char b);
void pc98_wait_write(Bitu port,Bitu val,Bitu iolen);
void pc98_crtc_write(Bitu port,Bitu val,Bitu iolen);
void pc98_port68_command_write(unsigned char b);
Bitu pc98_crtc_read(Bitu port,Bitu iolen);
Bitu pc98_a1_read(Bitu port,Bitu iolen);
void pc98_a1_write(Bitu port,Bitu val,Bitu iolen);
void pc98_gdc_write(Bitu port,Bitu val,Bitu iolen);
Bitu pc98_gdc_read(Bitu port,Bitu iolen);
Bitu pc98_egc4a0_read(Bitu port,Bitu iolen);
void pc98_egc4a0_write(Bitu port,Bitu val,Bitu iolen);
Bitu pc98_egc4a0_read_warning(Bitu port,Bitu iolen);
void pc98_egc4a0_write_warning(Bitu port,Bitu val,Bitu iolen);

void page_flip_debug_notify() {
	if (enable_page_flip_debugging_marker)
		vga_page_flip_occurred = true;
}

void vsync_poll_debug_notify() {
	if (enable_vretrace_poll_debugging_marker)
		vga_3da_polled = true;
}

Bit32u CGA_2_Table[16];
Bit32u CGA_4_Table[256];
Bit32u CGA_4_HiRes_Table[256];
Bit32u CGA_16_Table[256];
Bit32u TXT_Font_Table[16];
Bit32u TXT_FG_Table[16];
Bit32u TXT_BG_Table[16];
Bit32u ExpandTable[256];
Bit32u Expand16Table[4][16];
Bit32u FillTable[16];
Bit32u ColorTable[16];
double vga_force_refresh_rate = -1;

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

void VGA_DetermineMode(void) {
	if (svga.determine_mode) {
		svga.determine_mode();
		return;
	}
	/* Test for VGA output active or direct color modes */
	switch (vga.s3.misc_control_2 >> 4) {
	case 0:
		if (vga.attr.mode_control & 1) { // graphics mode
			if (IS_VGA_ARCH && ((vga.gfx.mode & 0x40)||(vga.s3.reg_3a&0x10))) {
				// access above 256k?
				if (vga.s3.reg_31 & 0x8) VGA_SetMode(M_LIN8);
				else VGA_SetMode(M_VGA);
			}
			else if (vga.gfx.mode & 0x20) VGA_SetMode(M_CGA4);
			else if ((vga.gfx.miscellaneous & 0x0c)==0x0c) VGA_SetMode(M_CGA2);
			else {
				// access above 256k?
				if (vga.s3.reg_31 & 0x8) VGA_SetMode(M_LIN4);
				else VGA_SetMode(M_EGA);
			}
		} else {
			VGA_SetMode(M_TEXT);
		}
		break;
	case 1:VGA_SetMode(M_LIN8);break;
	case 3:VGA_SetMode(M_LIN15);break;
	case 5:VGA_SetMode(M_LIN16);break;
	case 7:VGA_SetMode(M_LIN24);break;
	case 13:VGA_SetMode(M_LIN32);break;
	}
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

#define IS_RESET ((vga.seq.reset&0x3)!=0x3)
#define IS_SCREEN_ON ((vga.seq.clocking_mode&0x20)==0)
//static bool hadReset = false;

// disabled for later improvement
// Idea behind this: If the sequencer was reset and screen off we can
// Problem is some programs measure the refresh rate after switching mode,
// and get it wrong because of the 50ms guard time.
// On the other side, buggers like UniVBE switch the screen mode several
// times so the window is flickering.
// Also the demos that switch display end on screen (Dowhackado)
// would need some attention

void VGA_SequReset(bool reset) {
	//if(!reset && !IS_SCREEN_ON) hadReset=true;
}

void VGA_Screenstate(bool enabled) {
	/*if(enabled && hadReset) {
		hadReset=false;
		PIC_RemoveEvents(VGA_SetupDrawing);
		VGA_SetupDrawing(0);
	}*/
}

void VGA_SetClock(Bitu which,Bitu target) {
	if (svga.set_clock) {
		svga.set_clock(which, target);
		return;
	}
	struct{
		Bitu n,m;
		Bits err;
	} best;
	best.err=target;
	best.m=1;
	best.n=1;
	Bitu n,r;
	Bits m;

	for (r = 0; r <= 3; r++) {
		Bitu f_vco = target * (1 << r);
		if (MIN_VCO <= f_vco && f_vco < MAX_VCO) break;
    }
	for (n=1;n<=31;n++) {
		m=(target * (n + 2) * (1 << r) + (S3_CLOCK_REF/2)) / S3_CLOCK_REF - 2;
		if (0 <= m && m <= 127)	{
			Bitu temp_target = S3_CLOCK(m,n,r);
			Bits err = target - temp_target;
			if (err < 0) err = -err;
			if (err < best.err) {
				best.err = err;
				best.m = m;
				best.n = n;
			}
		}
    }
	/* Program the s3 clock chip */
	vga.s3.clk[which].m=best.m;
	vga.s3.clk[which].r=r;
	vga.s3.clk[which].n=best.n;
	VGA_StartResize();
}

void VGA_SetCGA2Table(Bit8u val0,Bit8u val1) {
	Bit8u total[2]={ val0,val1};
	for (Bitu i=0;i<16;i++) {
		CGA_2_Table[i]=
#ifdef WORDS_BIGENDIAN
			(total[(i >> 0) & 1] << 0  ) | (total[(i >> 1) & 1] << 8  ) |
			(total[(i >> 2) & 1] << 16 ) | (total[(i >> 3) & 1] << 24 );
#else 
			(total[(i >> 3) & 1] << 0  ) | (total[(i >> 2) & 1] << 8  ) |
			(total[(i >> 1) & 1] << 16 ) | (total[(i >> 0) & 1] << 24 );
#endif
	}
}

void VGA_SetCGA4Table(Bit8u val0,Bit8u val1,Bit8u val2,Bit8u val3) {
	Bit8u total[4]={ val0,val1,val2,val3};
	for (Bitu i=0;i<256;i++) {
		CGA_4_Table[i]=
#ifdef WORDS_BIGENDIAN
			(total[(i >> 0) & 3] << 0  ) | (total[(i >> 2) & 3] << 8  ) |
			(total[(i >> 4) & 3] << 16 ) | (total[(i >> 6) & 3] << 24 );
#else
			(total[(i >> 6) & 3] << 0  ) | (total[(i >> 4) & 3] << 8  ) |
			(total[(i >> 2) & 3] << 16 ) | (total[(i >> 0) & 3] << 24 );
#endif
		CGA_4_HiRes_Table[i]=
#ifdef WORDS_BIGENDIAN
			(total[((i >> 0) & 1) | ((i >> 3) & 2)] << 0  ) | (total[((i >> 1) & 1) | ((i >> 4) & 2)] << 8  ) |
			(total[((i >> 2) & 1) | ((i >> 5) & 2)] << 16 ) | (total[((i >> 3) & 1) | ((i >> 6) & 2)] << 24 );
#else
			(total[((i >> 3) & 1) | ((i >> 6) & 2)] << 0  ) | (total[((i >> 2) & 1) | ((i >> 5) & 2)] << 8  ) |
			(total[((i >> 1) & 1) | ((i >> 4) & 2)] << 16 ) | (total[((i >> 0) & 1) | ((i >> 3) & 2)] << 24 );
#endif
	}	
}

class VFRCRATE : public Program {
public:
	void Run(void) {
		if (cmd->FindString("SET",temp_line,false)) {
			char *x = (char*)temp_line.c_str();

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
		
		if (vga_force_refresh_rate > 0)
			WriteOut("Video refresh rate locked to %.3ffps\n",vga_force_refresh_rate);
		else
			WriteOut("Video refresh rate unlocked\n");
	}
};

static void VFRCRATE_ProgramStart(Program * * make) {
	*make=new VFRCRATE;
}

class CGASNOW : public Program {
public:
	void Run(void) {
		if(cmd->FindExist("ON")) {
			WriteOut("CGA snow enabled\n");
			enableCGASnow = 1;
			if (vga.mode == M_TEXT || vga.mode == M_TANDY_TEXT) {
				VGA_SetupHandlers();
				VGA_StartResize();
			}
		}
		else if(cmd->FindExist("OFF")) {
			WriteOut("CGA snow disabled\n");
			enableCGASnow = 0;
			if (vga.mode == M_TEXT || vga.mode == M_TANDY_TEXT) {
				VGA_SetupHandlers();
				VGA_StartResize();
			}
		}
		else {
			WriteOut("CGA snow currently %s\n",
				enableCGASnow ? "enabled" : "disabled");
		}
	}
};

static void CGASNOW_ProgramStart(Program * * make) {
	*make=new CGASNOW;
}

/* TODO: move to general header */
static inline int int_log2(int val) {
	int log = 0;
	while ((val >>= 1) != 0) log++;
	return log;
}

extern bool pcibus_enable;
extern int hack_lfb_yadjust;
extern uint8_t GDC_display_plane_wait_for_vsync;

void VGA_VsyncUpdateMode(VGA_Vsync vsyncmode);

void VGA_Reset(Section*) {
	Section_prop * section=static_cast<Section_prop *>(control->GetSection("dosbox"));
	string str;
    int i;

	LOG(LOG_MISC,LOG_DEBUG)("VGA_Reset() reinitializing VGA emulation");

    GDC_display_plane_wait_for_vsync = section->Get_bool("pc-98 buffer page flip");
    pc98_allow_scanline_effect = section->Get_bool("pc-98 allow scanline effect");

    // whether the GDC is running at 2.5MHz or 5.0MHz.
    // Some games require the GDC to run at 5.0MHz.
    // To enable these games we default to 5.0MHz.
    // NTS: There are also games that refuse to run if 5MHz switched on (TH03)
    gdc_5mhz_mode = section->Get_bool("pc-98 start gdc at 5mhz");

    enable_pc98_egc = section->Get_bool("pc-98 enable egc");
    enable_pc98_grcg = section->Get_bool("pc-98 enable grcg");
    enable_pc98_16color = section->Get_bool("pc-98 enable 16-color");

    // EGC implies GRCG
    if (enable_pc98_egc) enable_pc98_grcg = true;

    // EGC implies 16-color
    if (enable_pc98_16color) enable_pc98_16color = true;

	pc98_31khz_mode = section->Get_bool("pc-98 31-khz video mode");
	//TODO: Announce 31-KHz mode in BIOS config area. --yksoft1
	
    i = section->Get_int("pc-98 allow 4 display partition graphics");
    pc98_allow_4_display_partitions = (i < 0/*auto*/ || i == 1/*on*/);
    // TODO: "auto" will default to true if old PC-9801, false if PC-9821, or
    //       a more refined automatic choice according to actual hardware.

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
	hretrace_fx_avg_weight = section->Get_double("hretrace effect weight");
	ignore_vblank_wraparound = section->Get_bool("ignore vblank wraparound");
	vga_enable_hretrace_effects = section->Get_bool("allow hretrace effects");
	enable_page_flip_debugging_marker = section->Get_bool("page flip debug line");
	enable_vretrace_poll_debugging_marker = section->Get_bool("vertical retrace poll debug line");
	vga_double_buffered_line_compare = section->Get_bool("double-buffered line compare");
	hack_lfb_yadjust = section->Get_int("vesa lfb base scanline adjust");
	allow_vesa_lowres_modes = section->Get_bool("allow low resolution vesa modes");
	vesa12_modes_32bpp = section->Get_bool("vesa vbe 1.2 modes are 32bpp");
	allow_vesa_32bpp = section->Get_bool("allow 32bpp vesa modes");
	allow_vesa_24bpp = section->Get_bool("allow 24bpp vesa modes");
	allow_vesa_16bpp = section->Get_bool("allow 16bpp vesa modes");
	allow_vesa_15bpp = section->Get_bool("allow 15bpp vesa modes");
	allow_vesa_8bpp = section->Get_bool("allow 8bpp vesa modes");
	allow_vesa_4bpp = section->Get_bool("allow 4bpp vesa modes");
	allow_vesa_tty = section->Get_bool("allow tty vesa modes");
	enable_vga_resize_delay = section->Get_bool("enable vga resize delay");
	vga_ignore_hdispend_change_if_smaller = section->Get_bool("resize only on vga active display width increase");

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
	vga.mode=M_ERROR;			//For first init

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
		else if (machine == MCH_CGA || machine == MCH_HERC) {
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
	vga.vmemsize  = _MB_bytes(section->Get_int("vmemsize"));
	vga.vmemsize += _KB_bytes(section->Get_int("vmemsizekb"));
	vga.vmemsize  = (vga.vmemsize + 0xFFF) & (~0xFFF);
	/* mainline compatible: vmemsize == 0 means 512KB */
	if (vga.vmemsize == 0) vga.vmemsize = _KB_bytes(512);

	/* round up to the nearest power of 2 (TODO: Any video hardware that uses non-power-of-2 sizes?).
	 * A lot of DOSBox's VGA emulation code assumes power-of-2 VRAM sizes especially when wrapping
	 * memory addresses with (a & (vmemsize - 1)) type code. */
	if (!is_power_of_2(vga.vmemsize)) {
		Bitu i = int_log2(vga.vmemsize)+1;
		vga.vmemsize = 1 << i;
		LOG(LOG_VGA,LOG_WARN)("VGA RAM size requested is not a power of 2, rounding up to %uKB",vga.vmemsize>>10);
	}

	/* sanity check according to adapter type.
	 * FIXME: Again it was foolish for DOSBox to standardize on machine=
	 * for selecting machine type AND video card. */
	switch (machine) {
		case MCH_HERC: /* FIXME: MCH_MDA (4KB) vs MCH_HERC (64KB?) */
			if (vga.vmemsize < _KB_bytes(64)) vga.vmemsize = _KB_bytes(64);
			break;
		case MCH_CGA:
			if (vga.vmemsize < _KB_bytes(16)) vga.vmemsize = _KB_bytes(16);
			break;
		case MCH_TANDY:
		case MCH_PCJR:
			if (vga.vmemsize < _KB_bytes(128)) vga.vmemsize = _KB_bytes(128); /* FIXME: Right? */
			break;
		case MCH_EGA:
			if (vga.vmemsize <= _KB_bytes(128)) vga.vmemsize = _KB_bytes(128); /* Either 128KB or 256KB */
			else vga.vmemsize = _KB_bytes(256);
			break;
		case MCH_VGA:
            if (vga.vmemsize < _KB_bytes(256)) vga.vmemsize = _KB_bytes(256);
            break;
		case MCH_AMSTRAD:
			if (vga.vmemsize < _KB_bytes(64)) vga.vmemsize = _KB_bytes(64); /* FIXME: Right? */
			break;
        case MCH_PC98:
            if (vga.vmemsize < _KB_bytes(512)) vga.vmemsize = _KB_bytes(512);
            break;
		default:
			E_Exit("Unexpected machine");
	};

	vga.vmemwrap = 256*1024;	// default to 256KB VGA mem wrap

    if (!IS_PC98_ARCH)
        SVGA_Setup_Driver();		// svga video memory size is set here, possibly over-riding the user's selection

	LOG(LOG_VGA,LOG_NORMAL)("Video RAM: %uKB",vga.vmemsize>>10);

	VGA_SetupMemory();		// memory is allocated here
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
    }

	Section_prop * section2=static_cast<Section_prop *>(control->GetSection("vsync"));

	const char * vsyncmodestr;
	vsyncmodestr=section2->Get_string("vsyncmode");
	VGA_Vsync vsyncmode;
	if (!strcasecmp(vsyncmodestr,"off")) vsyncmode=VS_Off;
	else if (!strcasecmp(vsyncmodestr,"on")) vsyncmode=VS_On;
	else if (!strcasecmp(vsyncmodestr,"force")) vsyncmode=VS_Force;
	else if (!strcasecmp(vsyncmodestr,"host")) vsyncmode=VS_Host;
	else {
		vsyncmode=VS_Off;
		LOG_MSG("Illegal vsync type %s, falling back to off.",vsyncmodestr);
	}
	void change_output(int output);
	change_output(8);
	VGA_VsyncUpdateMode(vsyncmode);

	const char * vsyncratestr;
	vsyncratestr=section2->Get_string("vsyncrate");
	double vsyncrate=70;
	if (!strcasecmp(vsyncmodestr,"host")) {
#if defined (WIN32)
		DEVMODE	devmode;

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

	// TODO: Code to remove programs added by PROGRAMS_MakeFile

	if (machine == MCH_CGA) PROGRAMS_MakeFile("CGASNOW.COM",CGASNOW_ProgramStart);
	PROGRAMS_MakeFile("VFRCRATE.COM",VFRCRATE_ProgramStart);

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

extern void VGA_TweakUserVsyncOffset(float val);
void INT10_PC98_CurMode_Relocate(void);
void VGA_UnsetupMisc(void);
void VGA_UnsetupAttr(void);
void VGA_UnsetupDAC(void);
void VGA_UnsetupGFX(void);
void VGA_UnsetupSEQ(void);

#define gfx(blah) vga.gfx.blah
#define seq(blah) vga.seq.blah
#define crtc(blah) vga.crtc.blah

void VGA_OnEnterPC98(Section *sec) {
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

    pc98_update_palette();

    {
        unsigned char r,g,b;

        for (unsigned int i=0;i < 8;i++) {
            r = (i & 2) ? 255 : 0;
            g = (i & 4) ? 255 : 0;
            b = (i & 1) ? 255 : 0;

	        if (GFX_bpp >= 24) /* FIXME: Assumes 8:8:8. What happens when desktops start using the 10:10:10 format? */
                pc98_text_palette[i] = (b << GFX_Bshift) | (g << GFX_Gshift) | (r << GFX_Rshift) | GFX_Amask;
            else {
                /* FIXME: PC-98 mode renders as 32bpp regardless (at this time), so we have to fake 32bpp order */
                /*        Since PC-98 itself has 4-bit RGB precision, it might be best to offer a 16bpp rendering mode,
                 *        or even just have PC-98 mode stay in 16bpp entirely. */
                if (GFX_Bshift == 0)
                    pc98_text_palette[i] = (b << 0U) | (g << 8U) | (r << 16U);
                else
                    pc98_text_palette[i] = (b << 16U) | (g << 8U) | (r << 0U);
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

    vga.mode=M_PC98;
    assert(vga.vmemsize >= 0x80000);
    memset(vga.mem.linear,0,0x80000);

    VGA_StartResize();
}

void MEM_ResetPageHandler_Unmapped(Bitu phys_page, Bitu pages);

void updateGDCpartitions4(bool enable) {
	pc98_allow_4_display_partitions = enable;
	pc98_gdc[GDC_SLAVE].display_partition_mask = pc98_allow_4_display_partitions ? 3 : 1;
}

void VGA_OnEnterPC98_phase2(Section *sec) {
    VGA_SetupHandlers();

    /* GDC 2.5/5.0MHz setting is also reflected in BIOS data area and DIP switch registers */
    gdc_5mhz_mode_update_vars();

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

	//TODO: Find the correct GDC SYNC parameters in 31-KHz mode by inspecting a real PC-9821.
	if(!pc98_31khz_mode) { 
		pc98_gdc[GDC_MASTER].force_fifo_complete();
		pc98_gdc[GDC_MASTER].write_fifo_command(0x0F/*sync DE=1*/);
		pc98_gdc[GDC_MASTER].write_fifo_param(0x10);
		pc98_gdc[GDC_MASTER].write_fifo_param(0x4E);
		pc98_gdc[GDC_MASTER].write_fifo_param(0x07);
		pc98_gdc[GDC_MASTER].write_fifo_param(0x25);
		pc98_gdc[GDC_MASTER].force_fifo_complete();
		pc98_gdc[GDC_MASTER].write_fifo_param(0x07);
		pc98_gdc[GDC_MASTER].write_fifo_param(0x07);
		pc98_gdc[GDC_MASTER].write_fifo_param(0x90);
		pc98_gdc[GDC_MASTER].write_fifo_param(0x65);
		pc98_gdc[GDC_MASTER].force_fifo_complete();
	} else { //Use 31KHz HS, VS, VFP, VBP
		pc98_gdc[GDC_MASTER].force_fifo_complete();
		pc98_gdc[GDC_MASTER].write_fifo_command(0x0F/*sync DE=1*/);
		pc98_gdc[GDC_MASTER].write_fifo_param(0x10);
		pc98_gdc[GDC_MASTER].write_fifo_param(0x4E);
		pc98_gdc[GDC_MASTER].write_fifo_param(0x41);
		pc98_gdc[GDC_MASTER].write_fifo_param(0x24);
		pc98_gdc[GDC_MASTER].force_fifo_complete();
		pc98_gdc[GDC_MASTER].write_fifo_param(0x07); 
		pc98_gdc[GDC_MASTER].write_fifo_param(0x0C); 
		pc98_gdc[GDC_MASTER].write_fifo_param(0x90);
		pc98_gdc[GDC_MASTER].write_fifo_param(0x8D);
		pc98_gdc[GDC_MASTER].force_fifo_complete();		
	}

    pc98_gdc[GDC_SLAVE].master_sync = false;
    pc98_gdc[GDC_SLAVE].display_enable = false;//FIXME
    pc98_gdc[GDC_SLAVE].row_height = 1;
    pc98_gdc[GDC_SLAVE].display_pitch = 40;
    pc98_gdc[GDC_SLAVE].active_display_words_per_line = 40; /* 40 16-bit WORDs per line */
    pc98_gdc[GDC_SLAVE].display_partition_mask = pc98_allow_4_display_partitions ? 3 : 1;

	if(!pc98_31khz_mode) {
		pc98_gdc[GDC_SLAVE].force_fifo_complete();
		pc98_gdc[GDC_SLAVE].write_fifo_command(0x0F/*sync DE=1*/);
		pc98_gdc[GDC_SLAVE].write_fifo_param(0x02);
		pc98_gdc[GDC_SLAVE].write_fifo_param(0x26);
		pc98_gdc[GDC_SLAVE].write_fifo_param(0x03);
		pc98_gdc[GDC_SLAVE].write_fifo_param(0x11);
		pc98_gdc[GDC_SLAVE].force_fifo_complete();
		pc98_gdc[GDC_SLAVE].write_fifo_param(0x83);
		pc98_gdc[GDC_SLAVE].write_fifo_param(0x07);
		pc98_gdc[GDC_SLAVE].write_fifo_param(0x90);
		pc98_gdc[GDC_SLAVE].write_fifo_param(0x65);
		pc98_gdc[GDC_SLAVE].force_fifo_complete();
	} else { //Use 31KHz HS, VS, VFP, VBP
		pc98_gdc[GDC_SLAVE].write_fifo_command(0x0F/*sync DE=1*/);
		pc98_gdc[GDC_SLAVE].write_fifo_param(0x02);
		pc98_gdc[GDC_SLAVE].write_fifo_param(0x26);
		pc98_gdc[GDC_SLAVE].write_fifo_param(0x40);
		pc98_gdc[GDC_SLAVE].write_fifo_param(0x10);
		pc98_gdc[GDC_SLAVE].force_fifo_complete();
		pc98_gdc[GDC_SLAVE].write_fifo_param(0x83);
		pc98_gdc[GDC_SLAVE].write_fifo_param(0x0C);
		pc98_gdc[GDC_SLAVE].write_fifo_param(0x90);
		pc98_gdc[GDC_SLAVE].write_fifo_param(0x8D);
		pc98_gdc[GDC_SLAVE].force_fifo_complete();
	}

    VGA_StartResize();
}

void VGA_Init() {
	string str;
	Bitu i,j;

    vga.draw.render_step = 0;
    vga.draw.render_max = 1;

	vga.tandy.draw_base = NULL;
	vga.tandy.mem_base = NULL;
    vga.vmemsize_alloced = 0;
	LOG(LOG_MISC,LOG_DEBUG)("Initializing VGA");

	VGA_TweakUserVsyncOffset(0.0f);

	for (i=0;i<256;i++) {
		ExpandTable[i]=i | (i << 8)| (i <<16) | (i << 24);
	}
	for (i=0;i<16;i++) {
		TXT_FG_Table[i]=i | (i << 8)| (i <<16) | (i << 24);
		TXT_BG_Table[i]=i | (i << 8)| (i <<16) | (i << 24);
#ifdef WORDS_BIGENDIAN
		FillTable[i]=
			((i & 1) ? 0xff000000 : 0) |
			((i & 2) ? 0x00ff0000 : 0) |
			((i & 4) ? 0x0000ff00 : 0) |
			((i & 8) ? 0x000000ff : 0) ;
		TXT_Font_Table[i]=
			((i & 1) ? 0x000000ff : 0) |
			((i & 2) ? 0x0000ff00 : 0) |
			((i & 4) ? 0x00ff0000 : 0) |
			((i & 8) ? 0xff000000 : 0) ;
#else 
		FillTable[i]=
			((i & 1) ? 0x000000ff : 0) |
			((i & 2) ? 0x0000ff00 : 0) |
			((i & 4) ? 0x00ff0000 : 0) |
			((i & 8) ? 0xff000000 : 0) ;
		TXT_Font_Table[i]=	
			((i & 1) ? 0xff000000 : 0) |
			((i & 2) ? 0x00ff0000 : 0) |
			((i & 4) ? 0x0000ff00 : 0) |
			((i & 8) ? 0x000000ff : 0) ;
#endif
	}
	for (j=0;j<4;j++) {
		for (i=0;i<16;i++) {
#ifdef WORDS_BIGENDIAN
			Expand16Table[j][i] =
				((i & 1) ? 1 << j : 0) |
				((i & 2) ? 1 << (8 + j) : 0) |
				((i & 4) ? 1 << (16 + j) : 0) |
				((i & 8) ? 1 << (24 + j) : 0);
#else
			Expand16Table[j][i] =
				((i & 1) ? 1 << (24 + j) : 0) |
				((i & 2) ? 1 << (16 + j) : 0) |
				((i & 4) ? 1 << (8 + j) : 0) |
				((i & 8) ? 1 << j : 0);
#endif
		}
	}

	AddVMEventFunction(VM_EVENT_RESET,AddVMEventFunctionFuncPair(VGA_Reset));
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
	default:
		vga.vmemwrap = 256*1024;
		break;
	}
}
