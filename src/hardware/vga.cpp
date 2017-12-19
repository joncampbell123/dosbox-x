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

void VGA_VsyncUpdateMode(VGA_Vsync vsyncmode);

void VGA_Reset(Section*) {
	Section_prop * section=static_cast<Section_prop *>(control->GetSection("dosbox"));
	string str;
    int i;

	LOG(LOG_MISC,LOG_DEBUG)("VGA_Reset() reinitializing VGA emulation");

    pc98_allow_scanline_effect = section->Get_bool("pc-98 allow scanline effect");

    // whether the GDC is running at 2.5MHz or 5.0MHz.
    // Some games require the GDC to run at 5.0MHz.
    // To enable these games we default to 5.0MHz.
    // NTS: There are also games that refuse to run if 5MHz switched on (TH03)
    gdc_5mhz_mode = section->Get_bool("pc-98 start gdc at 5mhz");

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
            if (enable_pc98_jump) {
                if (vga.vmemsize < _KB_bytes(512)) vga.vmemsize = _KB_bytes(512);
            }
            else {
                if (vga.vmemsize < _KB_bytes(256)) vga.vmemsize = _KB_bytes(256);
            }
			break;
		case MCH_AMSTRAD:
			if (vga.vmemsize < _KB_bytes(64)) vga.vmemsize = _KB_bytes(64); /* FIXME: Right? */
			break;
		default:
			E_Exit("Unexpected machine");
	};

	vga.vmemwrap = 256*1024;	// default to 256KB VGA mem wrap
	SVGA_Setup_Driver();		// svga video memory size is set here, possibly over-riding the user's selection
	LOG(LOG_VGA,LOG_NORMAL)("Video RAM: %uKB",vga.vmemsize>>10);

	VGA_SetupMemory();		// memory is allocated here
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

    /* Some PC-98 game behavior seems to suggest the BIOS data area stretches all the way from segment 0x40:0x00 to segment 0x7F:0x0F inclusive.
     * Compare that to IBM PC platform, where segment fills only 0x40:0x00 to 0x50:0x00 inclusive and extra state is held in the "Extended BIOS Data Area".
     */

    /* number of text rows on the screen.
     * Touhou Project will not clear/format the text layer properly without this variable. */
    mem_writeb(0x710,25 - 1); /* cursor position Y coordinate */
    mem_writeb(0x711,1); /* function definition display status flag */
    mem_writeb(0x712,25 - 1); /* number of rows - 1 */
    mem_writeb(0x713,1); /* normal 25 lines */
    mem_writeb(0x714,0xE1); /* content erase attribute */

    mem_writeb(0x719,0x20); /* content erase character */

    mem_writeb(0x71B,0x01); /* cursor displayed */

    mem_writeb(0x71D,0xE1); /* content display attribute */

    mem_writeb(0x71F,0x01); /* scrolling speed is normal */

    {
        unsigned char r,g,b;

        for (unsigned int i=0;i < 8;i++) {
            r = (i & 2) ? 255 : 0;
            g = (i & 4) ? 255 : 0;
            b = (i & 1) ? 255 : 0;

            pc98_text_palette[i] = (b << GFX_Bshift) | (g << GFX_Gshift) | (r << GFX_Rshift) | GFX_Amask;
        }
    }

    pc98_gdc_tile_counter=0;
    pc98_gdc_modereg=0;
    for (unsigned int i=0;i < 4;i++) pc98_gdc_tiles[i].w = 0;

    /* 200-line tradition on PC-98 seems to be to render only every other scanline */
    pc98_graphics_hide_odd_raster_200line = true;

    // as a transition to PC-98 GDC emulation, move VGA alphanumeric buffer
    // down to A0000-AFFFFh.
    gdc_analog = false;
    pc98_gdc_vramop &= ~(1 << VOPBIT_ANALOG);
    gfx(miscellaneous) &= ~0x0C; /* bits[3:2] = 0 to map A0000-BFFFF */
    VGA_DetermineMode();
    VGA_SetupHandlers();
    INT10_PC98_CurMode_Relocate(); /* make sure INT 10h knows */

    /* Set up 24KHz hsync 56.42Hz rate */
    vga.crtc.horizontal_total = 106 - 5;
    vga.crtc.vertical_total = (440 - 2) & 0xFF;
    vga.crtc.end_vertical_blanking = (440 - 2 - 8) & 0xFF; // FIXME
    vga.crtc.vertical_retrace_start = (440 - 2 - 30) & 0xFF; // FIXME
    vga.crtc.vertical_retrace_end = (440 - 2 - 28) & 0xFF; // FIXME
    vga.crtc.start_vertical_blanking = (400 + 8) & 0xFF; // FIXME
    vga.crtc.overflow |=  0x01;
    vga.crtc.overflow &= ~0x20;

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
    for (unsigned int i=0;i < 16;i++) {
        /* GRB order palette */
		vga.dac.rgb[i].red = (i & 2) ? 63 : 0;
		vga.dac.rgb[i].green = (i & 4) ? 63 : 0;
        vga.dac.rgb[i].blue = (i & 1) ? 63 : 0;
        VGA_DAC_UpdateColor(i);
    }
    vga.mode=M_PC98;
    assert(vga.vmemsize >= 0x80000);
    memset(vga.mem.linear,0,0x80000);
    for (unsigned int i=0x2000;i < 0x3fe0;i += 2) vga.mem.linear[i] = 0xE0; /* attribute GRBxxxxx = 11100000 (white) */

    VGA_StartResize();
}

void MEM_ResetPageHandler_Unmapped(Bitu phys_page, Bitu pages);

void PC98_FM_OnEnterPC98(Section *sec);

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
    pc98_gdc[GDC_MASTER].active_display_words_per_line = 80;
    pc98_gdc[GDC_MASTER].display_partition_mask = 3;

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

    pc98_gdc[GDC_SLAVE].master_sync = false;
    pc98_gdc[GDC_SLAVE].display_enable = false;//FIXME
    pc98_gdc[GDC_SLAVE].row_height = 1;
    pc98_gdc[GDC_SLAVE].active_display_words_per_line = 40; /* 40 16-bit WORDs per line */
    pc98_gdc[GDC_SLAVE].display_partition_mask = pc98_allow_4_display_partitions ? 3 : 1;

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

    VGA_StartResize();

    void update_pc98_function_row(bool enable);
    update_pc98_function_row(true);
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
	AddVMEventFunction(VM_EVENT_ENTER_PC98_MODE,AddVMEventFunctionFuncPair(VGA_OnEnterPC98));
	AddVMEventFunction(VM_EVENT_ENTER_PC98_MODE_END,AddVMEventFunctionFuncPair(VGA_OnEnterPC98_phase2));

    // TODO: Move to separate file
	AddVMEventFunction(VM_EVENT_ENTER_PC98_MODE_END,AddVMEventFunctionFuncPair(PC98_FM_OnEnterPC98));
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

#include "np2glue.h"

MixerChannel *pc98_mixer = NULL;

static inline void pcm86io_bind(void) {
    /* dummy */
}

static inline void sound_sync(void) {
    if (pc98_mixer) pc98_mixer->FillUp();
}

// opngen.h

enum {
	OPNCH_MAX		= 30,
	OPNA_CLOCK		= 55466 * 72,

	OPN_CHMASK		= 0x80000000,
	OPN_STEREO		= 0x80000000,
	OPN_MONORAL		= 0x00000000
};


#if defined(OPNGENX86)

enum {
	FMDIV_BITS		= 8,
	FMDIV_ENT		= (1 << FMDIV_BITS),
	FMVOL_SFTBIT	= 4
};

#define SIN_BITS		11
#define EVC_BITS		10
#define ENV_BITS		16
#define KF_BITS			6
#define FREQ_BITS		21
#define ENVTBL_BIT		14
#define SINTBL_BIT		14

#elif defined(OPNGENARM)

enum {
	FMDIV_BITS		= 8,
	FMDIV_ENT		= (1 << FMDIV_BITS),
	FMVOL_SFTBIT	= 4
};

#define SIN_BITS		8
#define	EVC_BITS		7
#define	ENV_BITS		16
#define	KF_BITS			6
#define	FREQ_BITS		20
#define	ENVTBL_BIT		14
#define	SINTBL_BIT		14							// env+sin 30bit max

#else

enum {
	FMDIV_BITS		= 8,
	FMDIV_ENT		= (1 << FMDIV_BITS),
	FMVOL_SFTBIT	= 4
};

#define	SIN_BITS		10
#define	EVC_BITS		10
#define	ENV_BITS		16
#define	KF_BITS			6
#define	FREQ_BITS		21
#define	ENVTBL_BIT		14
#define	SINTBL_BIT		15

#endif

#define PI              M_PI

#define	TL_BITS			(FREQ_BITS+2)
#define	OPM_OUTSB		(TL_BITS + 2 - 16)			// OPM output 16bit

#define	SIN_ENT			(1L << SIN_BITS)
#define	EVC_ENT			(1L << EVC_BITS)

#define	EC_ATTACK		0								// ATTACK start
#define	EC_DECAY		(EVC_ENT << ENV_BITS)			// DECAY start
#define	EC_OFF			((2 * EVC_ENT) << ENV_BITS)		// OFF

#define	TL_MAX			(EVC_ENT * 2)

#define	OPM_ARRATE		 399128L
#define	OPM_DRRATE		5514396L

#define	EG_STEP	(96.0 / EVC_ENT)					// dB step
#define	SC(db)	(SINT32)((db) * ((3.0 / EG_STEP) * (1 << ENV_BITS))) + EC_DECAY
#define	D2(v)	(((double)(6 << KF_BITS) * log((double)(v)) / log(2.0)) + 0.5)
#define	FMASMSHIFT	(32 - 6 - (OPM_OUTSB + 1 + FMDIV_BITS) + FMVOL_SFTBIT)
#define	FREQBASE4096	((double)OPNA_CLOCK / calcrate / 64)

enum {
	PCM86_LOGICALBUF	= 0x8000,
	PCM86_BUFSIZE		= (1 << 16),
	PCM86_BUFMSK		= ((1 << 16) - 1),

	PCM86_DIVBIT		= 10,
	PCM86_DIVENV		= (1 << PCM86_DIVBIT),

	PCM86_RESCUE		= 20
};

#define	PCM86_EXTBUF		pcm86.rescue					// ~ÏØc
#define	PCM86_REALBUFSIZE	(PCM86_LOGICALBUF + PCM86_EXTBUF)

#define RECALC_NOWCLKWAIT(cnt) {										\
		pcm86.virbuf -= (cnt << pcm86.stepbit);							\
		if (pcm86.virbuf < 0) {											\
			pcm86.virbuf &= pcm86.stepmask;								\
		}																\
	}

typedef struct {
	SINT32	divremain;
	SINT32	div;
	SINT32	div2;
	SINT32	smp;
	SINT32	lastsmp;
	SINT32	smp_l;
	SINT32	lastsmp_l;
	SINT32	smp_r;
	SINT32	lastsmp_r;

	UINT32	readpos;			// DSOUNDÄ¶Êu
	UINT32	wrtpos;				// ÝÊu
	SINT32	realbuf;			// DSOUNDpÌf[^
	SINT32	virbuf;				// 86PCM(bufsize:0x8000)Ìf[^
	SINT32	rescue;

	SINT32	fifosize;
	SINT32	volume;
	SINT32	vol5;

	UINT32	lastclock;
	UINT32	stepclock;
	UINT	stepmask;

	UINT8	fifo;
	UINT8	extfunc;
	UINT8	dactrl;
	UINT8	_write;
	UINT8	stepbit;
	UINT8	reqirq;
	UINT8	irqflag;
	UINT8	padding[1];

	UINT8	buffer[PCM86_BUFSIZE];
} _PCM86, *PCM86;

typedef struct {
	UINT	rate;
	UINT	vol;
} PCM86CFG;


#ifdef __cplusplus
extern "C" {
#endif

extern const UINT pcm86rate8[];

// TvO[gÉ8|¯½¨
const UINT pcm86rate8[] = {352800, 264600, 176400, 132300,
							88200,  66150,  44010,  33075};

// 32,24,16,12, 8, 6, 4, 3 - Å­ö{: 96
//  3, 4, 6, 8,12,16,24,32

static const UINT clk25_128[] = {
					0x00001bde, 0x00002527, 0x000037bb, 0x00004a4e,
					0x00006f75, 0x0000949c, 0x0000df5f, 0x00012938};
static const UINT clk20_128[] = {
					0x000016a4, 0x00001e30, 0x00002d48, 0x00003c60,
					0x00005a8f, 0x000078bf, 0x0000b57d, 0x0000f17d};


	PCM86CFG	pcm86cfg;

void pcm86_cb(NEVENTITEM item);

void pcm86gen_initialize(UINT rate);
void pcm86gen_setvol(UINT vol);

void pcm86_reset(void);
void pcm86gen_update(void);
void pcm86_setpcmrate(REG8 val);
void pcm86_setnextintr(void);

void SOUNDCALL pcm86gen_checkbuf(void);
void SOUNDCALL pcm86gen_getpcm(void *hdl, SINT32 *pcm, UINT count);

BOOL pcm86gen_intrq(void);

void pcm86gen_initialize(UINT rate) {

	pcm86cfg.rate = rate;
}

void pcm86gen_setvol(UINT vol) {

	pcm86cfg.vol = vol;
	pcm86gen_update();
}

#ifdef __cplusplus
}
#endif

enum {
	ADTIMING_BIT	= 11,
	ADTIMING		= (1 << ADTIMING_BIT),
	ADPCM_SHIFT		= 3
};

typedef struct {
	UINT8	ctrl1;		// 00
	UINT8	ctrl2;		// 01
	UINT8	start[2];	// 02
	UINT8	stop[2];	// 04
	UINT8	reg06;
	UINT8	reg07;
	UINT8	data;		// 08
	UINT8	delta[2];	// 09
	UINT8	level;		// 0b
	UINT8	limit[2];	// 0c
	UINT8	reg0e;
	UINT8	reg0f;
	UINT8	flag;		// 10
	UINT8	reg11;
	UINT8	reg12;
	UINT8	reg13;
} ADPCMREG;

typedef struct {
	ADPCMREG	reg;
	UINT32		pos;
	UINT32		start;
	UINT32		stop;
	UINT32		limit;
	SINT32		level;
	UINT32		base;
	SINT32		samp;
	SINT32		delta;
	SINT32		remain;
	SINT32		step;
	SINT32		out0;
	SINT32		out1;
	SINT32		fb;
	SINT32		pertim;
	UINT8		status;
	UINT8		play;
	UINT8		mask;
	UINT8		fifopos;
	UINT8		fifo[2];
	UINT8		padding[2];
	UINT8		buf[0x40000];
} _ADPCM, *ADPCM;

typedef struct {
	UINT	rate;
	UINT	vol;
} ADPCMCFG;

#ifdef __cplusplus
extern "C" {
#endif

void adpcm_initialize(UINT rate);
void adpcm_setvol(UINT vol);

void adpcm_reset(ADPCM ad);
void adpcm_update(ADPCM ad);
void adpcm_setreg(ADPCM ad, UINT reg, REG8 value);
REG8 adpcm_status(ADPCM ad);

REG8 SOUNDCALL adpcm_readsample(ADPCM ad);
void SOUNDCALL adpcm_datawrite(ADPCM ad, REG8 data);
void SOUNDCALL adpcm_getpcm(ADPCM ad, SINT32 *buf, UINT count);

	ADPCMCFG	adpcmcfg;

void adpcm_initialize(UINT rate) {

	adpcmcfg.rate = rate;
}

void adpcm_setvol(UINT vol) {

	adpcmcfg.vol = vol;
}

void adpcm_reset(ADPCM ad) {

	ZeroMemory(ad, sizeof(_ADPCM));
	ad->mask = 0;					// (UINT8)~0x1c;
	ad->delta = 127;
	STOREINTELWORD(ad->reg.stop, 0x0002);
	STOREINTELWORD(ad->reg.limit, 0xffff);
	ad->stop = 0x000060;
	ad->limit = 0x200000;
	adpcm_update(ad);
}

void adpcm_update(ADPCM ad) {

	UINT32	addr;

	if (adpcmcfg.rate) {
		ad->base = ADTIMING * (OPNA_CLOCK / 72) / adpcmcfg.rate;
	}
	addr = LOADINTELWORD(ad->reg.delta);
	addr = (addr * ad->base) >> 16;
	if (addr < 0x80) {
		addr = 0x80;
	}
	ad->step = addr;
	ad->pertim = (1 << (ADTIMING_BIT * 2)) / addr;
	ad->level = (ad->reg.level * adpcmcfg.vol) >> 4;
}

#ifdef __cplusplus
}
#endif

typedef struct {
//	PMIXHDR	hdr;
//	PMIXTRK	trk[6];
	UINT	vol;
	UINT8	trkvol[8];
} _RHYTHM, *RHYTHM;


#ifdef __cplusplus
extern "C" {
#endif

void rhythm_initialize(UINT rate);
void rhythm_deinitialize(void);
UINT rhythm_getcaps(void);
void rhythm_setvol(UINT vol);

void rhythm_reset(RHYTHM rhy);
void rhythm_bind(RHYTHM rhy);
void rhythm_update(RHYTHM rhy);
void rhythm_setreg(RHYTHM rhy, UINT reg, REG8 val);

#ifdef __cplusplus
}
#endif

enum {
	PSGFREQPADBIT		= 12,
	PSGADDEDBIT			= 3
};

enum {
	PSGENV_INC			= 15,
	PSGENV_ONESHOT		= 16,
	PSGENV_LASTON		= 32,
	PSGENV_ONECYCLE		= 64
};

typedef struct {
	SINT32	freq;
	SINT32	count;
	SINT32	*pvol;			// !!
	UINT16	puchi;
	UINT8	pan;
	UINT8	padding;
} PSGTONE;

typedef struct {
	SINT32	freq;
	SINT32	count;
	UINT	base;
} PSGNOISE;

typedef struct {
	UINT8	tune[3][2];		// 0
	UINT8	noise;			// 6
	UINT8	mixer;			// 7
	UINT8	vol[3];			// 8
	UINT8	envtime[2];		// b
	UINT8	env;			// d
	UINT8	io1;
	UINT8	io2;
} PSGREG;

typedef struct {
	PSGTONE		tone[3];
	PSGNOISE	noise;
	PSGREG		reg;
	UINT16		envcnt;
	UINT16		envmax;
	UINT8		mixer;
	UINT8		envmode;
	UINT8		envvol;
	SINT8		envvolcnt;
	SINT32		evol;				// !!
	UINT		puchicount;
} _PSGGEN, *PSGGEN;

typedef struct {
	SINT32	volume[16];
	SINT32	voltbl[16];
	UINT	rate;
	UINT32	base;
	UINT16	puchidec;
} PSGGENCFG;


#ifdef __cplusplus
extern "C" {
#endif

void psggen_initialize(UINT rate);
void psggen_setvol(UINT vol);

void psggen_reset(PSGGEN psg);
void psggen_restore(PSGGEN psg);
void psggen_setreg(PSGGEN psg, UINT reg, REG8 val);
REG8 psggen_getreg(PSGGEN psg, UINT reg);
void psggen_setpan(PSGGEN psg, UINT ch, REG8 pan);

void SOUNDCALL psggen_getpcm(PSGGEN psg, SINT32 *pcm, UINT count);

#ifdef __cplusplus
}
#endif

enum {
	OPNSLOT1		= 0,				// slot number
	OPNSLOT2		= 2,
	OPNSLOT3		= 1,
	OPNSLOT4		= 3,

	EM_ATTACK		= 4,
	EM_DECAY1		= 3,
	EM_DECAY2		= 2,
	EM_RELEASE		= 1,
	EM_OFF			= 0
};

enum {
	KEYDISP_MODENONE			= 0,
	KEYDISP_MODEFM,
	KEYDISP_MODEMIDI
};

#define SUPPORT_PX

#if defined(SUPPORT_PX)
enum {
	KEYDISP_CHMAX		= 39,
	KEYDISP_FMCHMAX		= 30,
	KEYDISP_PSGMAX		= 3
};
#else	// defined(SUPPORT_PX)
enum {
	KEYDISP_CHMAX		= 16,
	KEYDISP_FMCHMAX		= 12,
	KEYDISP_PSGMAX		= 3
};
#endif	// defined(SUPPORT_PX)

enum {
	KEYDISP_NOTEMAX		= 16,

	KEYDISP_KEYCX		= 28,
	KEYDISP_KEYCY		= 14,

	KEYDISP_LEVEL		= (1 << 4),

	KEYDISP_WIDTH		= 301,
	KEYDISP_HEIGHT		= (KEYDISP_KEYCY * KEYDISP_CHMAX) + 1,

	KEYDISP_DELAYEVENTS	= 2048,
};

enum {
	KEYDISP_PALBG		= 0,
	KEYDISP_PALFG,
	KEYDISP_PALHIT,

	KEYDISP_PALS
};

enum {
	KEYDISP_FLAGDRAW		= 0x01,
	KEYDISP_FLAGREDRAW		= 0x02,
	KEYDISP_FLAGSIZING		= 0x04
};

enum {
	FNUM_MIN	= 599,
	FNUM_MAX	= 1199
};

enum {
	FTO_MAX		= 491,
	FTO_MIN		= 245
};

typedef struct {
	SINT32		*detune1;			// detune1
	SINT32		totallevel;			// total level
	SINT32		decaylevel;			// decay level
const SINT32	*attack;			// attack ratio
const SINT32	*decay1;			// decay1 ratio
const SINT32	*decay2;			// decay2 ratio
const SINT32	*release;			// release ratio
	SINT32 		freq_cnt;			// frequency count
	SINT32		freq_inc;			// frequency step
	SINT32		multiple;			// multiple
	UINT8		keyscale;			// key scale
	UINT8		env_mode;			// envelope mode
	UINT8		envratio;			// envelope ratio
	UINT8		ssgeg1;				// SSG-EG

	SINT32		env_cnt;			// envelope count
	SINT32		env_end;			// envelope end count
	SINT32		env_inc;			// envelope step
	SINT32		env_inc_attack;		// envelope attack step
	SINT32		env_inc_decay1;		// envelope decay1 step
	SINT32		env_inc_decay2;		// envelope decay2 step
	SINT32		env_inc_release;	// envelope release step
} OPNSLOT;

typedef struct {
	OPNSLOT	slot[4];
	UINT8	algorithm;			// algorithm
	UINT8	feedback;			// self feedback
	UINT8	playing;
	UINT8	outslot;
	SINT32	op1fb;				// operator1 feedback
	SINT32	*connect1;			// operator1 connect
	SINT32	*connect3;			// operator3 connect
	SINT32	*connect2;			// operator2 connect
	SINT32	*connect4;			// operator4 connect
	UINT32	keynote[4];			// key note				// ver0.27

	UINT8	keyfunc[4];			// key function
	UINT8	kcode[4];			// key code
	UINT8	pan;				// pan
	UINT8	extop;				// extendopelator-enable
	UINT8	stereo;				// stereo-enable
	UINT8	padding2;
} OPNCH;

typedef struct {
	UINT	playchannels;
	UINT	playing;
	SINT32	feedback2;
	SINT32	feedback3;
	SINT32	feedback4;
	SINT32	outdl;
	SINT32	outdc;
	SINT32	outdr;
	SINT32	calcremain;
	UINT8	keyreg[OPNCH_MAX];
} _OPNGEN, *OPNGEN;

typedef struct {
	SINT32	calc1024;
	SINT32	fmvol;
	UINT	ratebit;
	UINT	vr_en;
	SINT32	vr_l;
	SINT32	vr_r;

	SINT32	sintable[SIN_ENT];
	SINT32	envtable[EVC_ENT];
	SINT32	envcurve[EVC_ENT*2 + 1];
} OPNCFG;

#if !defined(DISABLE_SOUND)

typedef struct {
	UINT	addr;
	UINT	addr2;
	UINT8	data;
	UINT8	data2;
	UINT16	base;
	UINT8	adpcmmask;
	UINT8	channels;
	UINT8	extend;
	UINT8	_padding;
	UINT8	reg[0x400];
} OPN_T;

typedef struct {
	UINT16	port;
	UINT8	psg3reg;
	UINT8	rhythm;
} AMD98;

typedef struct {
	UINT8	porta;
	UINT8	portb;
	UINT8	portc;
	UINT8	mask;
	UINT8	key[8];
	int		sync;
	int		ch;
} MUSICGEN;

typedef struct {
	UINT16	posx;
	UINT16	pals;
const UINT8	*data;
} KDKEYPOS;

typedef struct {
	UINT8	k[KEYDISP_NOTEMAX];
	UINT8	r[KEYDISP_NOTEMAX];
	UINT	remain;
	UINT8	flag;
	UINT8	padding[3];
} KDCHANNEL;

typedef struct {
	UINT8	ch;
	UINT8	key;
} KDDELAYE;

typedef struct {
	UINT	pos;
	UINT	rem;
	UINT8	warm;
	UINT8	warmbase;
} KDDELAY;

typedef struct {
	UINT16	fnum[4];
	UINT8	lastnote[4];
	UINT8	flag;
	UINT8	extflag;
} KDFMCTRL;

typedef struct {
	UINT16	fto[4];
	UINT8	lastnote[4];
	UINT8	flag;
	UINT8	mix;
	UINT8	padding[2];
} KDPSGCTRL;

typedef struct {
	UINT8		mode;
	UINT8		dispflag;
	UINT8		framepast;
	UINT8		keymax;
	UINT8		fmmax;
	UINT8		psgmax;
	UINT8		fmpos[KEYDISP_FMCHMAX];
	UINT8		psgpos[KEYDISP_PSGMAX];
	const UINT8	*pfmreg[KEYDISP_FMCHMAX];
	KDDELAY		delay;
	KDCHANNEL	ch[KEYDISP_CHMAX];
	KDFMCTRL	fmctl[KEYDISP_FMCHMAX];
	KDPSGCTRL	psgctl[KEYDISP_PSGMAX];
//	UINT8		pal8[KEYDISP_PALS];
//	UINT16		pal16[KEYDISP_LEVEL*2];
//	RGB32		pal32[KEYDISP_LEVEL*2];
	KDKEYPOS	keypos[128];
	KDDELAYE	delaye[KEYDISP_DELAYEVENTS];
} KEYDISP;

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	UINT32	freq;
	UINT32	count;
} TMSCH;

typedef struct {
	TMSCH	ch[8];
	UINT	enable;
} _TMS3631, *TMS3631;

typedef struct {
	UINT	ratesft;
	SINT32	left;
	SINT32	right;
	SINT32	feet[16];
} TMS3631CFG;

static void	(*extfn)(REG8 enable);

struct _FMTIMER {
	UINT16	timera;
	UINT8	timerb;
	UINT8	status;
	UINT8	reg;
	UINT8	intr;
	UINT8	irq;
	UINT8	intdisabel;
};

extern	UINT32		usesound;
extern	OPN_T		opn;
extern	AMD98		amd98;
extern	MUSICGEN	musicgen;

	_FMTIMER	fmtimer;

extern	_TMS3631	tms3631;
extern	_FMTIMER	fmtimer;
extern	_OPNGEN		opngen;
extern	OPNCH		opnch[OPNCH_MAX];
extern	_PSGGEN		__psg[3];
extern	_RHYTHM		rhythm;
extern	_ADPCM		adpcm;
extern	_PCM86		pcm86;
//extern	_CS4231		cs4231;

	UINT32		usesound;
	OPN_T		opn;
	AMD98		amd98;
	MUSICGEN	musicgen;

	_TMS3631	tms3631;
	_OPNGEN		opngen;
	OPNCH		opnch[OPNCH_MAX];
	_PSGGEN		__psg[3];
	_RHYTHM		rhythm;
	_ADPCM		adpcm;
	_PCM86		pcm86;
//	_CS4231		cs4231;

	OPN_T		opn2;
	OPN_T		opn3;
	_RHYTHM		rhythm2;
	_RHYTHM		rhythm3;
	_ADPCM		adpcm2;
	_ADPCM		adpcm3;

#define	psg1	__psg[0]
#define	psg2	__psg[1]
#define	psg3	__psg[2]

extern	OPN_T		opn2;
extern	OPN_T		opn3;
extern	_RHYTHM		rhythm2;
extern	_RHYTHM		rhythm3;
extern	_ADPCM		adpcm2;
extern	_ADPCM		adpcm3;


// ----

//static	REG8	rapids = 0;

// ----

	OPNCFG	opncfg;
#ifdef OPNGENX86
	char	envshift[EVC_ENT];
	char	sinshift[SIN_ENT];
#endif


static	SINT32	detunetable[8][32];
static	SINT32	attacktable[94];
static	SINT32	decaytable[94];

static const SINT32	decayleveltable[16] = {
		 			SC( 0),SC( 1),SC( 2),SC( 3),SC( 4),SC( 5),SC( 6),SC( 7),
		 			SC( 8),SC( 9),SC(10),SC(11),SC(12),SC(13),SC(14),SC(31)};
static const UINT8 multipletable[] = {
			    	1, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30};
static const SINT32 nulltable[] = {
					0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
					0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
static const UINT8 kftable[16] = {0,0,0,0,0,0,0,1,2,3,3,3,3,3,3,3};
static const UINT8 dttable[] = {
					0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
					0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
					0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2,
					2, 3, 3, 3, 4, 4, 4, 5, 5, 6, 6, 7, 8, 8, 8, 8,
					1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5,
					5, 6, 6, 7, 8, 8, 9,10,11,12,13,14,16,16,16,16,
					2, 2, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5, 6, 6, 7,
					8, 8, 9,10,11,12,13,14,16,17,19,20,22,22,22,22};
static const int extendslot[4] = {2, 3, 1, 0};
static const int fmslot[4] = {0, 2, 1, 3};


// FOR I=0 TO 12:K=I-9:HZ# = 440*(2^((K*2-1)/24)):FM#=HZ#*(2^17)/55556!:PRINT INT(FM#);",";:NEXT
static const UINT16 fnumtbl[12] = {635,673,713,755,800,848,
									898,951,1008,1068,1132, 0xffff};


// FOR I=0 TO 12:K=I-9:HZ# = 440*(2^((K*2-1)/24)):PSG#=4000000#*(32*HZ#):PRINT INT(PSG#);",";:NEXT
static const UINT16 ftotbl[12] = {464,438,413,390,368,347,
									328,309,292,276,260, 0};

extern	OPNCFG	opncfg;

static	KEYDISP		keydisp;

	PSGGENCFG	psggencfg;

static const void *psgtbl[3] = {&psg1, &psg2, &psg3};

static void delaysetevent(UINT8 ch, UINT8 key);

static UINT8 getpsgnote(UINT16 tone) {

	UINT8	ret;
	int		i;

	ret = 60;
	tone &= 0xfff;

	while(tone < FTO_MIN) {
		tone <<= 1;
		ret += 12;
		if (ret >= 128) {
			return(127);
		}
	}
	while(tone > FTO_MAX) {
		if (!ret) {
			return(0);
		}
		tone >>= 1;
		ret -= 12;
	}
	for (i=0; tone < ftotbl[i]; i++) {
		ret++;
	}
	if (ret >= 128) {
		return(127);
	}
	return(ret);
}

static void psgmix(UINT8 ch, PSGGEN psg) {

	KDPSGCTRL	*k;

	k = keydisp.psgctl + ch;
	if ((k->mix ^ psg->reg.mixer) & 7) {
		UINT8 i, bit, pos;
		k->mix = psg->reg.mixer;
		pos = keydisp.psgpos[ch];
		for (i=0, bit=1; i<3; i++, pos++, bit<<=1) {
			if (k->flag & bit) {
				k->flag ^= bit;
				delaysetevent(pos, k->lastnote[i]);
			}
			else if ((!(k->mix & bit)) && (psg->reg.vol[i] & 0x1f)) {
				k->flag |= bit;
				k->fto[i] = LOADINTELWORD(psg->reg.tune[i]) & 0xfff;
				k->lastnote[i] = getpsgnote(k->fto[i]);
				delaysetevent(pos, (UINT8)(k->lastnote[i] | 0x80));
			}
		}
	}
}

void keydisp_psgmix(void *psg) {

	UINT8	c;

	if (keydisp.mode != KEYDISP_MODEFM) {
		return;
	}
	for (c=0; c<keydisp.psgmax; c++) {
		if (psgtbl[c] == psg) {
			psgmix(c, (PSGGEN)psg);
			break;
		}
	}
}


static const UINT8 psggen_deftbl[0x10] =
				{0, 0, 0, 0, 0, 0, 0, 0xbf, 0, 0, 0, 0, 0, 0, 0xff, 0xff};

static const UINT8 psgenv_pat[16] = {
					PSGENV_ONESHOT,
					PSGENV_ONESHOT,
					PSGENV_ONESHOT,
					PSGENV_ONESHOT,
					PSGENV_ONESHOT | PSGENV_INC,
					PSGENV_ONESHOT | PSGENV_INC,
					PSGENV_ONESHOT | PSGENV_INC,
					PSGENV_ONESHOT | PSGENV_INC,
					PSGENV_ONECYCLE,
					PSGENV_ONESHOT,
					0,
					PSGENV_ONESHOT | PSGENV_LASTON,
					PSGENV_ONECYCLE | PSGENV_INC,
					PSGENV_ONESHOT | PSGENV_INC | PSGENV_LASTON,
					PSGENV_INC,
					PSGENV_ONESHOT | PSGENV_INC};

static void psgvol(UINT8 ch, PSGGEN psg, UINT8 i) {

	KDPSGCTRL	*k;
	UINT8		bit;
	UINT8		pos;
	UINT16		tune;

	k = keydisp.psgctl + ch;
	bit = (1 << i);
	pos = keydisp.psgpos[ch] + i;
	if (psg->reg.vol[i] & 0x1f) {
		if (!((k->mix | k->flag) & bit)) {
			k->flag |= bit;
			tune = LOADINTELWORD(psg->reg.tune[i]);
			tune &= 0xfff;
			k->fto[i] = tune;
			k->lastnote[i] = getpsgnote(tune);
			delaysetevent(pos, (UINT8)(k->lastnote[i] | 0x80));
		}
	}
	else if (k->flag & bit) {
		k->flag ^= bit;
		delaysetevent(pos, k->lastnote[i]);
	}
}

static void psgkeyreset(void) {

	ZeroMemory(keydisp.psgctl, sizeof(keydisp.psgctl));
}

void pcm86_reset(void) {

	ZeroMemory(&pcm86, sizeof(pcm86));
	pcm86.fifosize = 0x80;
	pcm86.dactrl = 0x32;
	pcm86.stepmask = (1 << 2) - 1;
	pcm86.stepbit = 2;
	pcm86.stepclock = (PIT_TICK_RATE << 6);
	pcm86.stepclock /= 44100;
//	pcm86.stepclock *= pccore.multiple;
	pcm86.rescue = (PCM86_RESCUE * 32) << 2;
}

void pcm86gen_update(void) {

	pcm86.volume = pcm86cfg.vol * pcm86.vol5;
	pcm86_setpcmrate(pcm86.fifo);
}

void pcm86_setpcmrate(REG8 val) {

	SINT32	rate;

	rate = pcm86rate8[val & 7];
	pcm86.stepclock = (PIT_TICK_RATE << 6);
	pcm86.stepclock /= rate;
	pcm86.stepclock *= (1 << 3);
	if (pcm86cfg.rate) {
		pcm86.div = (rate << (PCM86_DIVBIT - 3)) / pcm86cfg.rate;
		pcm86.div2 = (pcm86cfg.rate << (PCM86_DIVBIT + 3)) / rate;
	}
}

void psggen_initialize(UINT rate) {

	double	pom;
	UINT	i;

	ZeroMemory(&psggencfg, sizeof(psggencfg));
	psggencfg.rate = rate;
	pom = (double)0x0c00;
	for (i=15; i; i--) {
		psggencfg.voltbl[i] = (SINT32)pom;
		pom /= 1.41492;
	}
	psggencfg.puchidec = (UINT16)(rate / 11025) * 2;
	if (psggencfg.puchidec == 0) {
		psggencfg.puchidec = 1;
	}
	if (rate) {
		psggencfg.base = (5000 * (1 << (32 - PSGFREQPADBIT - PSGADDEDBIT)))
															/ (rate / 25);
	}
}

void psggen_setvol(UINT vol) {

	UINT	i;

	for (i=1; i<16; i++) {
		psggencfg.volume[i] = (psggencfg.voltbl[i] * vol) >> 
															(6 + PSGADDEDBIT);
	}
}

void psggen_reset(PSGGEN psg) {

	UINT	i;

	ZeroMemory(psg, sizeof(_PSGGEN));
	for (i=0; i<3; i++) {
		psg->tone[i].pvol = psggencfg.volume + 0;
	}
	for (i=0; i<sizeof(psggen_deftbl); i++) {
		psggen_setreg(psg, i, psggen_deftbl[i]);
	}
}

void psggen_restore(PSGGEN psg) {

	UINT	i;

	for (i=0; i<0x0e; i++) {
		psggen_setreg(psg, i, ((UINT8 *)&psg->reg)[i]);
	}
}

void keydisp_psgvol(void *psg, UINT8 ch) {

	UINT8	c;

	if (keydisp.mode != KEYDISP_MODEFM) {
		return;
	}
	for (c=0; c<keydisp.psgmax; c++) {
		if (psgtbl[c] == psg) {
			psgvol(c, (PSGGEN)psg, ch);
			break;
		}
	}
}

void psggen_setreg(PSGGEN psg, UINT reg, REG8 value) {

	UINT	ch;
	UINT	freq;

	reg = reg & 15;
	if (reg < 14) {
		sound_sync();
	}
	((UINT8 *)&psg->reg)[reg] = value;
	switch(reg) {
		case 0:
		case 1:
		case 2:
		case 3:
		case 4:
		case 5:
			ch = reg >> 1;
			freq = LOADINTELWORD(psg->reg.tune[ch]) & 0xfff;
			if (freq > 9) {
				psg->tone[ch].freq = (psggencfg.base / freq) << PSGFREQPADBIT;
			}
			else {
				psg->tone[ch].freq = 0;
			}
			break;

		case 6:
			freq = value & 0x1f;
			if (freq == 0) {
				freq = 1;
			}
			psg->noise.freq = psggencfg.base / freq;
			psg->noise.freq <<= PSGFREQPADBIT;
			break;

		case 7:
			keydisp_psgmix(psg);
			psg->mixer = ~value;
			psg->puchicount = psggencfg.puchidec;
//			TRACEOUT(("psg %x 7 %d", (long)psg, value));
			break;

		case 8:
		case 9:
		case 10:
			ch = reg - 8;
			keydisp_psgvol(psg, (UINT8)ch);
			if (value & 0x10) {
				psg->tone[ch].pvol = &psg->evol;
			}
			else {
				psg->tone[ch].pvol = psggencfg.volume + (value & 15);
			}
			psg->tone[ch].puchi = psggencfg.puchidec;
			psg->puchicount = psggencfg.puchidec;
//			TRACEOUT(("psg %x %x %d", (long)psg, reg, value));
			break;

		case 11:
		case 12:
			freq = LOADINTELWORD(psg->reg.envtime);
			freq = psggencfg.rate * freq / 125000;
			if (freq == 0) {
				freq = 1;
			}
			psg->envmax = freq;
			break;

		case 13:
			psg->envmode = psgenv_pat[value & 0x0f];
			psg->envvolcnt = 16;
			psg->envcnt = 1;
			break;
	}
}

REG8 psggen_getreg(PSGGEN psg, UINT reg) {

	return(((UINT8 *)&psg->reg)[reg & 15]);
}

void psggen_setpan(PSGGEN psg, UINT ch, REG8 pan) {

	if ((psg) && (ch < 3)) {
		psg->tone[ch].pan = pan;
	}
}

#if 0
static void keyallreload(void) {

	UINT	i;

	for (i=0; i<KEYDISP_CHMAX; i++) {
		keydisp.ch[i].flag = 2;
	}
}
#endif

#if 0
static void keyallclear(void) {

	ZeroMemory(keydisp.ch, sizeof(keydisp.ch));
	keyallreload();
}
#endif

static void chkeyoff(UINT ch) {

	UINT		i;
	KDCHANNEL	*kdch;

	kdch = keydisp.ch + ch;
	for (i=0; i<kdch->remain; i++) {
		if ((kdch->r[i] & (~0x80)) >= (KEYDISP_LEVEL - 1)) {
			kdch->r[i] = 0x80 | (KEYDISP_LEVEL - 2);
			kdch->flag |= 1;
		}
	}
}

static void keyalloff(void) {

	UINT8	i;

	for (i=0; i<KEYDISP_CHMAX; i++) {
		chkeyoff(i);
	}
}

static void keyon(UINT ch, UINT8 note) {

	UINT		i;
	KDCHANNEL	*kdch;

	note &= 0x7f;
	kdch = keydisp.ch + ch;
	for (i=0; i<kdch->remain; i++) {
		if (kdch->k[i] == note) {				// qbgµ½
			for (; i<(kdch->remain-1); i++) {
				kdch->k[i] = kdch->k[i+1];
				kdch->r[i] = kdch->r[i+1];
			}
			kdch->k[i] = note;
			kdch->r[i] = 0x80 | (KEYDISP_LEVEL - 1);
			kdch->flag |= 1;
			return;
		}
	}
	if (i < KEYDISP_NOTEMAX) {
		kdch->k[i] = note;
		kdch->r[i] = 0x80 | (KEYDISP_LEVEL - 1);
		kdch->flag |= 1;
		kdch->remain++;
	}
}

static void keyoff(UINT ch, UINT8 note) {

	UINT		i;
	KDCHANNEL	*kdch;

	note &= 0x7f;
	kdch = keydisp.ch + ch;
	for (i=0; i<kdch->remain; i++) {
		if (kdch->k[i] == note) {				// qbgµ½
			kdch->r[i] = 0x80 | (KEYDISP_LEVEL - 2);
			kdch->flag |= 1;
			break;
		}
	}
}

static void delaysetevent(UINT8 ch, UINT8 key) {

	KDDELAYE	*e;

	e = keydisp.delaye;
	if (keydisp.delay.rem < KEYDISP_DELAYEVENTS) {
		e += (keydisp.delay.pos + keydisp.delay.rem) &
													(KEYDISP_DELAYEVENTS - 1);
		keydisp.delay.rem++;
		e->ch = ch;
		e->key = key;
	}
	else {
		e += keydisp.delay.pos;
		keydisp.delay.pos += (keydisp.delay.pos + 1) &
													(KEYDISP_DELAYEVENTS - 1);
		if (e->ch == 0xff) {
			keydisp.delay.warm++;
		}
		else if (e->key & 0x80) {
			keyon(e->ch, e->key);
		}
		else {
			keyoff(e->ch, e->key);
		}
		e->ch = ch;
		e->key = key;
	}
}

static UINT8 getfmnote(UINT16 fnum) {

	UINT8	ret;
	int		i;

	ret = (fnum >> 11) & 7;
	ret *= 12;
	ret += 24;
	fnum &= 0x7ff;

	while(fnum < FNUM_MIN) {
		if (!ret) {
			return(0);
		}
		ret -= 12;
		fnum <<= 1;
	}
	while(fnum > FNUM_MAX) {
		fnum >>= 1;
		ret += 12;
	}
	for (i=0; fnum >= fnumtbl[i]; i++) {
		ret++;
	}
	if (ret > 127) {
		return(127);
	}
	return(ret);
}


static void fmkeyoff(UINT8 ch, KDFMCTRL *k) {

	delaysetevent(keydisp.fmpos[ch], k->lastnote[0]);
}

static void fmkeyon(UINT8 ch, KDFMCTRL *k) {

	const UINT8 *pReg;

	fmkeyoff(ch, k);
	pReg = keydisp.pfmreg[ch];
	if (pReg)
	{
		pReg = pReg + 0xa0;
		k->fnum[0] = ((pReg[4] & 0x3f) << 8) + pReg[0];
		k->lastnote[0] = getfmnote(k->fnum[0]);
		delaysetevent(keydisp.fmpos[ch], (UINT8)(k->lastnote[0] | 0x80));
	}
}

static void fmkeyreset(void) {

	ZeroMemory(keydisp.fmctl, sizeof(keydisp.fmctl));
}


#define	CALCENV(e, c, s)													\
	(c)->slot[(s)].freq_cnt += (c)->slot[(s)].freq_inc;						\
	(c)->slot[(s)].env_cnt += (c)->slot[(s)].env_inc;						\
	if ((c)->slot[(s)].env_cnt >= (c)->slot[(s)].env_end) {					\
		switch((c)->slot[(s)].env_mode) {									\
			case EM_ATTACK:													\
				(c)->slot[(s)].env_mode = EM_DECAY1;						\
				(c)->slot[(s)].env_cnt = EC_DECAY;							\
				(c)->slot[(s)].env_end = (c)->slot[(s)].decaylevel;			\
				(c)->slot[(s)].env_inc = (c)->slot[(s)].env_inc_decay1;		\
				break;														\
			case EM_DECAY1:													\
				(c)->slot[(s)].env_mode = EM_DECAY2;						\
				(c)->slot[(s)].env_cnt = (c)->slot[(s)].decaylevel;			\
				(c)->slot[(s)].env_end = EC_OFF;							\
				(c)->slot[(s)].env_inc = (c)->slot[(s)].env_inc_decay2;		\
				break;														\
			case EM_RELEASE:												\
				(c)->slot[(s)].env_mode = EM_OFF;							\
			case EM_DECAY2:													\
				(c)->slot[(s)].env_cnt = EC_OFF;							\
				(c)->slot[(s)].env_end = EC_OFF + 1;						\
				(c)->slot[(s)].env_inc = 0;									\
				(c)->playing &= ~(1 << (s));								\
				break;														\
		}																	\
	}																		\
	(e) = (c)->slot[(s)].totallevel -										\
					opncfg.envcurve[(c)->slot[(s)].env_cnt >> ENV_BITS];

#define SLOTOUT(s, e, c)													\
		((opncfg.sintable[(((s).freq_cnt + (c)) >>							\
							(FREQ_BITS - SIN_BITS)) & (SIN_ENT - 1)] *		\
				opncfg.envtable[(e)]) >> (ENVTBL_BIT+SINTBL_BIT-TL_BITS))


static void calcratechannel(OPNCH *ch) {

	SINT32	envout;
	SINT32	opout;

	opngen.feedback2 = 0;
	opngen.feedback3 = 0;
	opngen.feedback4 = 0;

	/* SLOT 1 */
	CALCENV(envout, ch, 0);
	if (envout > 0) {
		if (ch->feedback) {
			/* with self feed back */
			opout = ch->op1fb;
			ch->op1fb = SLOTOUT(ch->slot[0], envout,
											(ch->op1fb >> ch->feedback));
			opout = (opout + ch->op1fb) / 2;
		}
		else {
			/* without self feed back */
			opout = SLOTOUT(ch->slot[0], envout, 0);
		}
		/* output slot1 */
		if (!ch->connect1) {
			opngen.feedback2 = opngen.feedback3 = opngen.feedback4 = opout;
		}
		else {
			*ch->connect1 += opout;
		}
	}
	/* SLOT 2 */
	CALCENV(envout, ch, 1);
	if (envout > 0) {
		*ch->connect2 += SLOTOUT(ch->slot[1], envout, opngen.feedback2);
	}
	/* SLOT 3 */
	CALCENV(envout, ch, 2);
	if (envout > 0) {
		*ch->connect3 += SLOTOUT(ch->slot[2], envout, opngen.feedback3);
	}
	/* SLOT 4 */
	CALCENV(envout, ch, 3);
	if (envout > 0) {
		*ch->connect4 += SLOTOUT(ch->slot[3], envout, opngen.feedback4);
	}
}

void SOUNDCALL opngen_getpcm(void *hdl, SINT32 *pcm, UINT count) {

	OPNCH	*fm;
	UINT	i;
	UINT	playing;
	SINT32	samp_l;
	SINT32	samp_r;

	if ((!opngen.playing) || (!count)) {
		return;
	}
	fm = opnch;
	do {
		samp_l = opngen.outdl * (opngen.calcremain * -1);
		samp_r = opngen.outdr * (opngen.calcremain * -1);
		opngen.calcremain += FMDIV_ENT;
		while(1) {
			opngen.outdc = 0;
			opngen.outdl = 0;
			opngen.outdr = 0;
			playing = 0;
			for (i=0; i<opngen.playchannels; i++) {
				if (fm[i].playing & fm[i].outslot) {
					calcratechannel(fm + i);
					playing++;
				}
			}
			opngen.outdl += opngen.outdc;
			opngen.outdr += opngen.outdc;
			opngen.outdl >>= FMVOL_SFTBIT;
			opngen.outdr >>= FMVOL_SFTBIT;
			if (opngen.calcremain > opncfg.calc1024) {
				samp_l += opngen.outdl * opncfg.calc1024;
				samp_r += opngen.outdr * opncfg.calc1024;
				opngen.calcremain -= opncfg.calc1024;
			}
			else {
				break;
			}
		}
		samp_l += opngen.outdl * opngen.calcremain;
		samp_l >>= 8;
		samp_l *= opncfg.fmvol;
		samp_l >>= (OPM_OUTSB + FMDIV_BITS + 1 + 6 - FMVOL_SFTBIT - 8);
		pcm[0] += samp_l;
		samp_r += opngen.outdr * opngen.calcremain;
		samp_r >>= 8;
		samp_r *= opncfg.fmvol;
		samp_r >>= (OPM_OUTSB + FMDIV_BITS + 1 + 6 - FMVOL_SFTBIT - 8);
		pcm[1] += samp_r;
		opngen.calcremain -= opncfg.calc1024;
		pcm += 2;
	} while(--count);
	opngen.playing = playing;
	(void)hdl;
}

void SOUNDCALL opngen_getpcmvr(void *hdl, SINT32 *pcm, UINT count) {

	OPNCH	*fm;
	UINT	i;
	SINT32	samp_l;
	SINT32	samp_r;

	fm = opnch;
	while(count--) {
		samp_l = opngen.outdl * (opngen.calcremain * -1);
		samp_r = opngen.outdr * (opngen.calcremain * -1);
		opngen.calcremain += FMDIV_ENT;
		while(1) {
			opngen.outdc = 0;
			opngen.outdl = 0;
			opngen.outdr = 0;
			for (i=0; i<opngen.playchannels; i++) {
				calcratechannel(fm + i);
			}
			if (opncfg.vr_en) {
				SINT32 tmpl;
				SINT32 tmpr;
				tmpl = (opngen.outdl >> 5) * opncfg.vr_l;
				tmpr = (opngen.outdr >> 5) * opncfg.vr_r;
				opngen.outdl += tmpr;
				opngen.outdr += tmpl;
			}
			opngen.outdl += opngen.outdc;
			opngen.outdr += opngen.outdc;
			opngen.outdl >>= FMVOL_SFTBIT;
			opngen.outdr >>= FMVOL_SFTBIT;
			if (opngen.calcremain > opncfg.calc1024) {
				samp_l += opngen.outdl * opncfg.calc1024;
				samp_r += opngen.outdr * opncfg.calc1024;
				opngen.calcremain -= opncfg.calc1024;
			}
			else {
				break;
			}
		}
		samp_l += opngen.outdl * opngen.calcremain;
		samp_l >>= 8;
		samp_l *= opncfg.fmvol;
		samp_l >>= (OPM_OUTSB + FMDIV_BITS + 1 + 6 - FMVOL_SFTBIT - 8);
		pcm[0] += samp_l;
		samp_r += opngen.outdr * opngen.calcremain;
		samp_r >>= 8;
		samp_r *= opncfg.fmvol;
		samp_r >>= (OPM_OUTSB + FMDIV_BITS + 1 + 6 - FMVOL_SFTBIT - 8);
		pcm[1] += samp_r;
		opngen.calcremain -= opncfg.calc1024;
		pcm += 2;
	}
	(void)hdl;
}


void opngen_initialize(UINT rate) {

	UINT	ratebit;
	int		i;
	char	sft;
	int		j;
	double	pom;
	long	detune;
	double	freq;
	UINT32	calcrate;

	if (rate == 44100) {
		ratebit = 0;
	}
	else if (rate == 22050) {
		ratebit = 1;
	}
	else {
		ratebit = 2;
	}
	calcrate = (OPNA_CLOCK / 72) >> ratebit;
	opncfg.calc1024 = FMDIV_ENT * 44100 / (OPNA_CLOCK / 72);

	for (i=0; i<EVC_ENT; i++) {
#ifdef OPNGENX86
		sft = ENVTBL_BIT;
		while(sft < (ENVTBL_BIT + 8)) {
			pom = (double)(1 << sft) / pow(10.0, EG_STEP*(EVC_ENT-i)/20.0);
			opncfg.envtable[i] = (long)pom;
			envshift[i] = sft - TL_BITS;
			if (opncfg.envtable[i] >= (1 << (ENVTBL_BIT - 1))) {
				break;
			}
			sft++;
		}
#else
		pom = (double)(1 << ENVTBL_BIT) / pow(10.0, EG_STEP*(EVC_ENT-i)/20.0);
		opncfg.envtable[i] = (long)pom;
#endif
	}
	for (i=0; i<SIN_ENT; i++) {
#ifdef OPNGENX86
		char sft;
		sft = SINTBL_BIT;
		while(sft < (SINTBL_BIT + 8)) {
			pom = (double)(1 << sft) * sin(2*PI*i/SIN_ENT);
			opncfg.sintable[i] = (long)pom;
			sinshift[i] = sft;
			if (opncfg.sintable[i] >= (1 << (SINTBL_BIT - 1))) {
				break;
			}
			if (opncfg.sintable[i] <= -1 * (1 << (SINTBL_BIT - 1))) {
				break;
			}
			sft++;
		}
#else
		pom = (double)((1 << SINTBL_BIT) - 1) * sin(2*PI*i/SIN_ENT);
		opncfg.sintable[i] = (long)pom;
#endif
	}
	for (i=0; i<EVC_ENT; i++) {
		pom = pow(((double)(EVC_ENT-1-i)/EVC_ENT), 8) * EVC_ENT;
		opncfg.envcurve[i] = (long)pom;
		opncfg.envcurve[EVC_ENT + i] = i;
	}
	opncfg.envcurve[EVC_ENT*2] = EVC_ENT;

//	opmbaserate = (1L << FREQ_BITS) / (rate * x / 44100) * 55466;
//	Åà¡Í x == 55466¾©çc

//	±±Å FREQ_BITS >= 16ªð
	if (rate == 44100) {
		opncfg.ratebit = 0 + (FREQ_BITS - 16);
	}
	else if (rate == 22050) {
		opncfg.ratebit = 1 + (FREQ_BITS - 16);
	}
	else {
		opncfg.ratebit = 2 + (FREQ_BITS - 16);
	}

	for (i=0; i<4; i++) {
		for (j=0; j<32; j++) {
			detune = dttable[i*32 + j];
			sft = ratebit + (FREQ_BITS - 21);
			if (sft >= 0) {
				detune <<= sft;
			}
			else {
				detune >>= (0 - sft);
			}

			detunetable[i][j]   = detune;
			detunetable[i+4][j] = -detune;
		}
	}
	for (i=0; i<4; i++) {
		attacktable[i] = decaytable[i] = 0;
	}
	for (i=4; i<64; i++) {
		freq = (double)(EVC_ENT << ENV_BITS) * FREQBASE4096;
		if (i < 8) {							// YêÄÜ·B
			freq *= 1.0 + (i & 2) * 0.25;
		}
		else if (i < 60) {
			freq *= 1.0 + (i & 3) * 0.25;
		}
		freq *= (double)(1 << ((i >> 2) - 1));
#if 0
		attacktable[i] = (long)((freq + OPM_ARRATE - 1) / OPM_ARRATE);
		decaytable[i] = (long)((freq + OPM_DRRATE - 1) / OPM_DRRATE);
#else
		attacktable[i] = (long)(freq / OPM_ARRATE);
		decaytable[i] = (long)(freq / OPM_DRRATE);
#endif
		if (attacktable[i] >= EC_DECAY) {
//			TRACEOUT(("attacktable %d %d %ld", i, attacktable[i], EC_DECAY));
		}
		if (decaytable[i] >= EC_DECAY) {
//			TRACEOUT(("decaytable %d %d %ld", i, decaytable[i], EC_DECAY));
		}
	}
	attacktable[62] = EC_DECAY - 1;
	attacktable[63] = EC_DECAY - 1;
	for (i=64; i<94; i++) {
		attacktable[i] = attacktable[63];
		decaytable[i] = decaytable[63];
	}
}

void opngen_setvol(UINT vol) {

	opncfg.fmvol = vol * 5 / 4;
#if defined(OPNGENX86)
	opncfg.fmvol <<= FMASMSHIFT;
#endif
}

void opngen_setVR(REG8 channel, REG8 value) {

	if ((channel & 3) && (value)) {
		opncfg.vr_en = TRUE;
		opncfg.vr_l = (channel & 1)?value:0;
		opncfg.vr_r = (channel & 2)?value:0;
	}
	else {
		opncfg.vr_en = FALSE;
	}
}


// ----

static void set_algorithm(OPNCH *ch) {

	SINT32	*outd;
	UINT8	outslot;

	outd = &opngen.outdc;
	if (ch->stereo) {
		switch(ch->pan & 0xc0) {
			case 0x80:
				outd = &opngen.outdl;
				break;

			case 0x40:
				outd = &opngen.outdr;
				break;
		}
	}
	switch(ch->algorithm) {
		case 0:
			ch->connect1 = &opngen.feedback2;
			ch->connect2 = &opngen.feedback3;
			ch->connect3 = &opngen.feedback4;
			outslot = 0x08;
			break;

		case 1:
			ch->connect1 = &opngen.feedback3;
			ch->connect2 = &opngen.feedback3;
			ch->connect3 = &opngen.feedback4;
			outslot = 0x08;
			break;

		case 2:
			ch->connect1 = &opngen.feedback4;
			ch->connect2 = &opngen.feedback3;
			ch->connect3 = &opngen.feedback4;
			outslot = 0x08;
			break;

		case 3:
			ch->connect1 = &opngen.feedback2;
			ch->connect2 = &opngen.feedback4;
			ch->connect3 = &opngen.feedback4;
			outslot = 0x08;
			break;

		case 4:
			ch->connect1 = &opngen.feedback2;
			ch->connect2 = outd;
			ch->connect3 = &opngen.feedback4;
			outslot = 0x0a;
			break;

		case 5:
			ch->connect1 = 0;
			ch->connect2 = outd;
			ch->connect3 = outd;
			outslot = 0x0e;
			break;

		case 6:
			ch->connect1 = &opngen.feedback2;
			ch->connect2 = outd;
			ch->connect3 = outd;
			outslot = 0x0e;
			break;

		case 7:
		default:
			ch->connect1 = outd;
			ch->connect2 = outd;
			ch->connect3 = outd;
			outslot = 0x0f;
	}
	ch->connect4 = outd;
	ch->outslot = outslot;
}

static void set_dt1_mul(OPNSLOT *slot, REG8 value) {

	slot->multiple = (SINT32)multipletable[value & 0x0f];
	slot->detune1 = detunetable[(value >> 4) & 7];
}

static void set_tl(OPNSLOT *slot, REG8 value) {

#if (EVC_BITS >= 7)
	slot->totallevel = ((~value) & 0x007f) << (EVC_BITS - 7);
#else
	slot->totallevel = ((~value) & 0x007f) >> (7 - EVC_BITS);
#endif
}

static void set_ks_ar(OPNSLOT *slot, REG8 value) {

	slot->keyscale = ((~value) >> 6) & 3;
	value &= 0x1f;
	slot->attack = (value)?(attacktable + (value << 1)):nulltable;
	slot->env_inc_attack = slot->attack[slot->envratio];
	if (slot->env_mode == EM_ATTACK) {
		slot->env_inc = slot->env_inc_attack;
	}
}

static void set_d1r(OPNSLOT *slot, REG8 value) {

	value &= 0x1f;
	slot->decay1 = (value)?(decaytable + (value << 1)):nulltable;
	slot->env_inc_decay1 = slot->decay1[slot->envratio];
	if (slot->env_mode == EM_DECAY1) {
		slot->env_inc = slot->env_inc_decay1;
	}
}

static void set_dt2_d2r(OPNSLOT *slot, REG8 value) {

	value &= 0x1f;
	slot->decay2 = (value)?(decaytable + (value << 1)):nulltable;
	if (slot->ssgeg1) {
		slot->env_inc_decay2 = 0;
	}
	else {
		slot->env_inc_decay2 = slot->decay2[slot->envratio];
	}
	if (slot->env_mode == EM_DECAY2) {
		slot->env_inc = slot->env_inc_decay2;
	}
}

static void set_d1l_rr(OPNSLOT *slot, REG8 value) {

	slot->decaylevel = decayleveltable[(value >> 4)];
	slot->release = decaytable + ((value & 0x0f) << 2) + 2;
	slot->env_inc_release = slot->release[slot->envratio];
	if (slot->env_mode == EM_RELEASE) {
		slot->env_inc = slot->env_inc_release;
		if (value == 0xff) {
			slot->env_mode = EM_OFF;
			slot->env_cnt = EC_OFF;
			slot->env_end = EC_OFF + 1;
			slot->env_inc = 0;
		}
	}
}

static void set_ssgeg(OPNSLOT *slot, REG8 value) {

	value &= 0xf;
	if ((value == 0xb) || (value == 0xd)) {
		slot->ssgeg1 = 1;
		slot->env_inc_decay2 = 0;
	}
	else {
		slot->ssgeg1 = 0;
		slot->env_inc_decay2 = slot->decay2[slot->envratio];
	}
	if (slot->env_mode == EM_DECAY2) {
		slot->env_inc = slot->env_inc_decay2;
	}
}

static void channleupdate(OPNCH *ch) {

	int		i;
	UINT32	fc = ch->keynote[0];						// ver0.27
	UINT8	kc = ch->kcode[0];
	UINT	evr;
	OPNSLOT	*slot;
	int		s;

	slot = ch->slot;

    assert(slot->detune1 != NULL);
    assert(slot->attack != NULL);
    assert(slot->decay1 != NULL);
    assert(slot->decay2 != NULL);
    assert(slot->release != NULL);

	if (!(ch->extop)) {
		for (i=0; i<4; i++, slot++) {
			slot->freq_inc = (fc + slot->detune1[kc]) * slot->multiple;
			evr = kc >> slot->keyscale;
			if (slot->envratio != evr) {
				slot->envratio = evr;
				slot->env_inc_attack = slot->attack[evr];
				slot->env_inc_decay1 = slot->decay1[evr];
				slot->env_inc_decay2 = slot->decay2[evr];
				slot->env_inc_release = slot->release[evr];
			}
		}
	}
	else {
		for (i=0; i<4; i++, slot++) {
			s = extendslot[i];
			slot->freq_inc = (ch->keynote[s] + slot->detune1[ch->kcode[s]])
														* slot->multiple;
			evr = ch->kcode[s] >> slot->keyscale;
			if (slot->envratio != evr) {
				slot->envratio = evr;
				slot->env_inc_attack = slot->attack[evr];
				slot->env_inc_decay1 = slot->decay1[evr];
				slot->env_inc_decay2 = slot->decay2[evr];
				slot->env_inc_release = slot->release[evr];
			}
		}
	}
}

void opngen_setreg(REG8 chbase, UINT reg, REG8 value);

// ----

void opngen_reset(void) {

	OPNCH	*ch;
	UINT	i;
	OPNSLOT	*slot;
	UINT	j;

    LOG_MSG("OPNGEN reset");

	ZeroMemory(&opngen, sizeof(opngen));
	ZeroMemory(opnch, sizeof(opnch));
	opngen.playchannels = 3;

	ch = opnch;
	for (i=0; i<OPNCH_MAX; i++) {
		ch->keynote[0] = 0;
		slot = ch->slot;
		for (j=0; j<4; j++) {
			slot->env_mode = EM_OFF;
			slot->env_cnt = EC_OFF;
			slot->env_end = EC_OFF + 1;
			slot->env_inc = 0;
			slot->detune1 = detunetable[0];
			slot->attack = nulltable;
			slot->decay1 = nulltable;
			slot->decay2 = nulltable;
			slot->release = decaytable;
			slot++;
		}
		ch++;
	}
	for (i=0x30; i<0xc0; i++) {
		opngen_setreg(0, i, 0xff);
		opngen_setreg(3, i, 0xff);
		opngen_setreg(6, i, 0xff);
		opngen_setreg(9, i, 0xff);
	}
}

void opngen_setcfg(REG8 maxch, UINT32 flag) {

	OPNCH	*ch;
	UINT	i;

	opngen.playchannels = maxch;
	ch = opnch;
	if ((flag & OPN_CHMASK) == OPN_STEREO) {
		for (i=0; i<OPNCH_MAX; i++) {
			if (flag & (1 << i)) {
				ch->stereo = TRUE;
				set_algorithm(ch);
			}
			ch++;
		}
	}
	else {
		for (i=0; i<OPNCH_MAX; i++) {
			if (flag & (1 << i)) {
				ch->stereo = FALSE;
				set_algorithm(ch);
			}
			ch++;
		}
	}
}

void opngen_setextch(UINT chnum, REG8 data) {

	OPNCH	*ch;

	ch = opnch;
	ch[chnum].extop = data;
}

void opngen_setreg(REG8 chbase, UINT reg, REG8 value) {

	UINT	chpos;
	OPNCH	*ch;
	OPNSLOT	*slot;
	UINT	fn;
	UINT8	blk;

	chpos = reg & 3;
	if (chpos == 3) {
		return;
	}
	sound_sync();
	ch = opnch + chbase + chpos;
	if (reg < 0xa0) {
		slot = ch->slot + fmslot[(reg >> 2) & 3];
		switch(reg & 0xf0) {
			case 0x30:					// DT1 MUL
				set_dt1_mul(slot, value);
				channleupdate(ch);
				break;

			case 0x40:					// TL
				set_tl(slot, value);
				break;

			case 0x50:					// KS AR
				set_ks_ar(slot, value);
				channleupdate(ch);
				break;

			case 0x60:					// D1R
				set_d1r(slot, value);
				break;

			case 0x70:					// DT2 D2R
				set_dt2_d2r(slot, value);
				channleupdate(ch);
				break;

			case 0x80:					// D1L RR
				set_d1l_rr(slot, value);
				break;

			case 0x90:
				set_ssgeg(slot, value);
				channleupdate(ch);
				break;
		}
	}
	else {
		switch(reg & 0xfc) {
			case 0xa0:
				blk = ch->keyfunc[0] >> 3;
				fn = ((ch->keyfunc[0] & 7) << 8) + value;
				ch->kcode[0] = (blk << 2) | kftable[fn >> 7];
//				ch->keynote[0] = fn * opmbaserate / (1L << (22-blk));
				ch->keynote[0] = (fn << (opncfg.ratebit + blk)) >> 6;
				channleupdate(ch);
				break;

			case 0xa4:
				ch->keyfunc[0] = value & 0x3f;
				break;

			case 0xa8:
				ch = opnch + chbase + 2;
				blk = ch->keyfunc[chpos+1] >> 3;
				fn = ((ch->keyfunc[chpos+1] & 7) << 8) + value;
				ch->kcode[chpos+1] = (blk << 2) | kftable[fn >> 7];
//				ch->keynote[chpos+1] = fn * opmbaserate / (1L << (22-blk));
				ch->keynote[chpos+1] = (fn << (opncfg.ratebit + blk)) >> 6;
				channleupdate(ch);
				break;

			case 0xac:
				ch = opnch + chbase + 2;
				ch->keyfunc[chpos+1] = value & 0x3f;
				break;

			case 0xb0:
				ch->algorithm = (UINT8)(value & 7);
				value = (value >> 3) & 7;
				if (value) {
					ch->feedback = 8 - value;
				}
				else {
					ch->feedback = 0;
				}
				set_algorithm(ch);
				break;

			case 0xb4:
				ch->pan = (UINT8)(value & 0xc0);
				set_algorithm(ch);
				break;
		}
	}
}

static const UINT8 keybrd1[] = {				// ®ÕB
				28, 14,
				0xc4, 0x6c, 0x44, 0x60,
				0xc4, 0x6c, 0x44, 0x60,
				0xc4, 0x6c, 0x44, 0x60,
				0xc4, 0x6c, 0x44, 0x60,
				0xc4, 0x6c, 0x44, 0x60,
				0xc4, 0x6c, 0x44, 0x60,
				0xc4, 0x6c, 0x44, 0x60,
				0xc4, 0x6c, 0x44, 0x60,
				0xee, 0xee, 0xee, 0xe0,
				0xee, 0xee, 0xee, 0xe0,
				0xee, 0xee, 0xee, 0xe0,
				0xee, 0xee, 0xee, 0xe0,
				0xee, 0xee, 0xee, 0xe0,
				0x00, 0x00, 0x00, 0x00};

static const UINT8 keybrd2[] = {				// ®Õ GÅØêéB
				20, 14,
				0xc4, 0x6c, 0x40,
				0xc4, 0x6c, 0x40,
				0xc4, 0x6c, 0x40,
				0xc4, 0x6c, 0x40,
				0xc4, 0x6c, 0x40,
				0xc4, 0x6c, 0x40,
				0xc4, 0x6c, 0x40,
				0xc4, 0x6c, 0x40,
				0xee, 0xee, 0xe0,
				0xee, 0xee, 0xe0,
				0xee, 0xee, 0xe0,
				0xee, 0xee, 0xe0,
				0xee, 0xee, 0xe0,
				0x00, 0x00, 0x00};

static const UINT8 keybrd_s1[] = {				// C, F
				3, 13,
				0xc0, 0xc0, 0xc0, 0xc0,
				0xc0, 0xc0, 0xc0, 0xc0,
				0xe0, 0xe0, 0xe0, 0xe0,
				0xe0};

static const UINT8 keybrd_s2[] = {				// D, G, A
				3, 13,
				0x40, 0x40, 0x40, 0x40,
				0x40, 0x40, 0x40, 0x40,
				0xe0, 0xe0, 0xe0, 0xe0,
				0xe0};

static const UINT8 keybrd_s3[] = {				// E, B
				3, 13,
				0x60, 0x60, 0x60, 0x60,
				0x60, 0x60, 0x60, 0x60,
				0xe0, 0xe0, 0xe0, 0xe0,
				0xe0};

static const UINT8 keybrd_s4[] = {				// C+, D+, F+, G+, A+
				3, 8,
				0xe0, 0xe0, 0xe0, 0xe0,
				0xe0, 0xe0, 0xe0, 0xe0};

static const KDKEYPOS keyposdef[12] = {
				{ 0, 0, keybrd_s1}, { 2, KEYDISP_LEVEL, keybrd_s4},
				{ 4, 0, keybrd_s2}, { 6, KEYDISP_LEVEL, keybrd_s4},
				{ 8, 0, keybrd_s3}, {12, 0, keybrd_s1},
				{14, KEYDISP_LEVEL, keybrd_s4}, {16, 0, keybrd_s2},
				{18, KEYDISP_LEVEL, keybrd_s4}, {20, 0, keybrd_s2},
				{22, KEYDISP_LEVEL, keybrd_s4}, {24, 0, keybrd_s3}};

void keydisp_fmkeyon(UINT8 ch, UINT8 value) {

	KDFMCTRL	*k;

	if (keydisp.mode != KEYDISP_MODEFM) {
		return;
	}
	if (ch < keydisp.fmmax) {
		k = keydisp.fmctl + ch;
		value &= 0xf0;
		if (k->flag != value) {
			if (value) {
				fmkeyon(ch, k);
			}
			else {
				fmkeyoff(ch, k);
			}
			k->flag = value;
		}
	}
}

void opngen_keyon(UINT chnum, REG8 value) {

	OPNCH	*ch;
	OPNSLOT	*slot;
	REG8	bit;
	UINT	i;

	sound_sync();
	opngen.keyreg[chnum] = value;
	opngen.playing++;
	ch = opnch + chnum;
	ch->playing |= value >> 4;
	slot = ch->slot;
	bit = 0x10;
	for (i=0; i<4; i++) {
		if (value & bit) {							// keyon
			if (slot->env_mode <= EM_RELEASE) {
				slot->freq_cnt = 0;
				if (i == OPNSLOT1) {
					ch->op1fb = 0;
				}
				slot->env_mode = EM_ATTACK;
				slot->env_inc = slot->env_inc_attack;
				slot->env_cnt = EC_ATTACK;
				slot->env_end = EC_DECAY;
			}
		}
		else {										// keyoff
			if (slot->env_mode > EM_RELEASE) {
				slot->env_mode = EM_RELEASE;
				if (!(slot->env_cnt & EC_DECAY)) {
					slot->env_cnt = (opncfg.envcurve[slot->env_cnt
										>> ENV_BITS] << ENV_BITS) + EC_DECAY;
				}
				slot->env_end = EC_OFF;
				slot->env_inc = slot->env_inc_release;
			}
		}
		slot++;
		bit <<= 1;
	}
	keydisp_fmkeyon((UINT8)chnum, value);
}

	TMS3631CFG	tms3631cfg;

static const UINT16 tms3631_freqtbl[] = {
			0,	0x051B, 0x0569, 0x05BB, 0x0613, 0x066F, 0x06D1,
				0x0739, 0x07A7, 0x081B, 0x0897, 0x091A, 0x09A4, 0,0,0,
			0,	0x0A37, 0x0AD3, 0x0B77, 0x0C26, 0x0CDF, 0x0DA3,
				0x0E72, 0x0F4E, 0x1037, 0x112E, 0x1234, 0x1349, 0,0,0,
			0,	0x146E, 0x15A6, 0x16EF, 0x184C, 0x19BE, 0x1B46,
				0x1CE5, 0x1E9D, 0x206F, 0x225D, 0x2468, 0x2692, 0,0,0,
			0,	0x28DD, 0x2B4C, 0x2DDF, 0x3099, 0x337D, 0x368D,
				0x39CB, 0x3D3B, 0x40DF, 0x44BA, 0x48D1, 0x4D25, 0x51BB, 0,0};


void tms3631_initialize(UINT rate) {

	UINT	sft;

	ZeroMemory(&tms3631cfg, sizeof(tms3631cfg));
	sft = 0;
	if (rate == 11025) {
		sft = 0;
	}
	else if (rate == 22050) {
		sft = 1;
	}
	else if (rate == 44100) {
		sft = 2;
	}
	tms3631cfg.ratesft = sft;
}

void tms3631_setvol(const UINT8 *vol) {

	UINT	i;
	UINT	j;
	SINT32	data;

	tms3631cfg.left = (vol[0] & 15) << 5;
	tms3631cfg.right = (vol[1] & 15) << 5;
	vol += 2;
	for (i=0; i<16; i++) {
		data = 0;
		for (j=0; j<4; j++) {
			data += (vol[j] & 15) * ((i & (1 << j))?1:-1);
		}
		tms3631cfg.feet[i] = data << 5;
	}
}


// ----

void tms3631_reset(TMS3631 tms) {

	ZeroMemory(tms, sizeof(_TMS3631));
}

void tms3631_setkey(TMS3631 tms, REG8 ch, REG8 key) {

	tms->ch[ch & 7].freq = tms3631_freqtbl[key & 0x3f] >> tms3631cfg.ratesft;
}

void tms3631_setenable(TMS3631 tms, REG8 enable) {

	tms->enable = enable;
}

void fmboard_extreg(void (*ext)(REG8 enable)) {

	extfn = ext;
}

void fmboard_extenable(REG8 enable) {

	if (extfn) {
		(*extfn)(enable);
	}
}



// ----

static void setfmregs(UINT8 *reg) {

	FillMemory(reg + 0x30, 0x60, 0xff);
	FillMemory(reg + 0x90, 0x20, 0x00);
	FillMemory(reg + 0xb0, 0x04, 0x00);
	FillMemory(reg + 0xb4, 0x04, 0xc0);
}

void fmtimer_reset(UINT irq);
void fmtimer_setreg(UINT reg, REG8 value);

static void extendchannel(REG8 enable) {

	opn.extend = enable;
	if (enable) {
		opn.channels = 6;
		opngen_setcfg(6, OPN_STEREO | 0x007);
	}
	else {
		opn.channels = 3;
		opngen_setcfg(3, OPN_MONORAL | 0x007);
		rhythm_setreg(&rhythm, 0x10, 0xff);
	}
}

void board86_reset(const NP2CFG *pConfig) {

	fmtimer_reset(0/*IRQ3*/);
	opngen_setcfg(3, OPN_STEREO | 0x038);
//	if (pConfig->snd86opt & 2) {
//		soundrom_load(0xcc000, OEMTEXT("86"));
//	}
	opn.base = 0x100;//(pConfig->snd86opt & 0x01)?0x000:0x100;
	fmboard_extreg(extendchannel);
}

static void setfmhdl(UINT8 items, UINT base) {

	while(items--) {
		if ((keydisp.keymax < KEYDISP_CHMAX) &&
			(keydisp.fmmax < KEYDISP_FMCHMAX)) {
			keydisp.fmpos[keydisp.fmmax] = keydisp.keymax++;
			keydisp.pfmreg[keydisp.fmmax] = opn.reg + base;
			keydisp.fmmax++;
			base++;
			if ((base & 3) == 3) {
				base += 0x100 - 3;
			}
		}
	}
}

static void setfmhdlex(const OPN_T *pOpn, UINT nItems, UINT nBase) {

	while(nItems--) {
		if ((keydisp.keymax < KEYDISP_CHMAX) &&
			(keydisp.fmmax < KEYDISP_FMCHMAX)) {
			keydisp.fmpos[keydisp.fmmax] = keydisp.keymax++;
			keydisp.pfmreg[keydisp.fmmax] = pOpn->reg + nBase;
			keydisp.fmmax++;
			nBase++;
			if ((nBase & 3) == 3) {
				nBase += 0x100 - 3;
			}
		}
	}
}

static void delayreset(void) {

	keydisp.delay.warm = keydisp.delay.warmbase;
	keydisp.delay.pos = 0;
	keydisp.delay.rem = 0;
	ZeroMemory(keydisp.delaye, sizeof(keydisp.delaye));
	keyalloff();
}

static void setpsghdl(UINT8 items) {

	while(items--) {
		if ((keydisp.keymax <= (KEYDISP_CHMAX - 3)) &&
			(keydisp.psgmax < KEYDISP_PSGMAX)) {
			keydisp.psgpos[keydisp.psgmax++] = keydisp.keymax;
			keydisp.keymax += 3;
		}
	}
}

void keydisp_setfmboard(UINT b) {

	keydisp.keymax = 0;
	keydisp.fmmax = 0;
	keydisp.psgmax = 0;

#if defined(SUPPORT_PX)
	if (b == 0x30)
	{
		setfmhdlex(&opn, 12, 0);
		setfmhdlex(&opn2, 12, 0);
		setpsghdl(2);
		b = 0;
	}
	if (b == 0x50)
	{
		setfmhdlex(&opn, 12, 0);
		setfmhdlex(&opn2, 12, 0);
		setfmhdlex(&opn3, 6, 0);
		setpsghdl(3);
		b = 0;
	}

#endif	// defined(SUPPORT_PX)

	if (b & 0x02) {
		if (!(b & 0x04)) {
			setfmhdl(3, 0);
		}
		else {								// QhµÌWX^Ú®
			setfmhdl(3, 0x200);
		}
		setpsghdl(1);
	}
	if (b & 0x04) {
		setfmhdl(6, 0);
		setpsghdl(1);
	}
	if (b & 0x08) {
		setfmhdl(6, 0);
		setpsghdl(1);
	}
	if (b & 0x20) {
		setfmhdl(6, 0);
		setpsghdl(1);
	}
	if (b & 0x40) {
		setfmhdl(12, 0);
		setpsghdl(1);
	}
	if (b & 0x80) {
		setpsghdl(3);
	}
	delayreset();
	fmkeyreset();
	psgkeyreset();

	if (keydisp.mode == KEYDISP_MODEFM) {
		keydisp.dispflag |= KEYDISP_FLAGSIZING;
	}
}

void fmboard_reset(const NP2CFG *pConfig, UINT32 type) {

//	UINT8	cross;

//	soundrom_reset();
//	beep_reset();												// ver0.27a
//	cross = np2cfg.snd_x;										// ver0.30

    LOG_MSG("FM reset");

	extfn = NULL;
	ZeroMemory(&opn, sizeof(opn));
	setfmregs(opn.reg + 0x000);
	setfmregs(opn.reg + 0x100);
	setfmregs(opn.reg + 0x200);
	setfmregs(opn.reg + 0x300);
	opn.reg[0xff] = 0x01;
	opn.channels = 3;
	opn.adpcmmask = (UINT8)~(0x1c);

	ZeroMemory(&opn2, sizeof(opn2));
	setfmregs(opn2.reg + 0x000);
	setfmregs(opn2.reg + 0x100);
	setfmregs(opn2.reg + 0x200);
	setfmregs(opn2.reg + 0x300);
	opn2.reg[0xff] = 0x01;
	opn2.channels = 3;
	opn2.adpcmmask = (UINT8)~(0x1c);

	ZeroMemory(&opn3, sizeof(opn3));
	setfmregs(opn3.reg + 0x000);
	setfmregs(opn3.reg + 0x100);
	setfmregs(opn3.reg + 0x200);
	setfmregs(opn3.reg + 0x300);
	opn3.reg[0xff] = 0x01;
	opn3.channels = 3;
	opn3.adpcmmask = (UINT8)~(0x1c);

	ZeroMemory(&musicgen, sizeof(musicgen));
	ZeroMemory(&amd98, sizeof(amd98));

	tms3631_reset(&tms3631);
	opngen_reset();
	psggen_reset(&psg1);
	psggen_reset(&psg2);
	psggen_reset(&psg3);
	rhythm_reset(&rhythm);
	rhythm_reset(&rhythm2);
	rhythm_reset(&rhythm3);
	adpcm_reset(&adpcm);
	adpcm_reset(&adpcm2);
	adpcm_reset(&adpcm3);
	pcm86_reset();
//    cs4231_reset();

    board86_reset(pConfig);

    usesound = type;
//    soundmng_setreverse(0);
	keydisp_setfmboard(type);
	opngen_setVR(0,0);//pConfig->spb_vrc, pConfig->spb_vrl);??
}

void board86c_bind(void);

void fmboard_bind(void) {
    board86c_bind();

//    sound_streamregist(&beep, (SOUNDCB)beep_getpcm);
}


// ----

void fmboard_fmrestore(REG8 chbase, UINT bank) {

	REG8	i;
const UINT8	*reg;

	reg = opn.reg + (bank * 0x100);
	for (i=0x30; i<0xa0; i++) {
		opngen_setreg(chbase, i, reg[i]);
	}
	for (i=0xb7; i>=0xa0; i--) {
		opngen_setreg(chbase, i, reg[i]);
	}
	for (i=0; i<3; i++) {
		opngen_keyon(chbase + i, opngen.keyreg[chbase + i]);
	}
}

void fmboard_rhyrestore(RHYTHM rhy, UINT bank) {

const UINT8	*reg;

	reg = opn.reg + (bank * 0x100);
	rhythm_setreg(rhy, 0x11, reg[0x11]);
	rhythm_setreg(rhy, 0x18, reg[0x18]);
	rhythm_setreg(rhy, 0x19, reg[0x19]);
	rhythm_setreg(rhy, 0x1a, reg[0x1a]);
	rhythm_setreg(rhy, 0x1b, reg[0x1b]);
	rhythm_setreg(rhy, 0x1c, reg[0x1c]);
	rhythm_setreg(rhy, 0x1d, reg[0x1d]);
}


void fmboard_fmrestore2(OPN_T* pOpn, REG8 chbase, UINT bank) {

	REG8	i;
const UINT8	*reg;

	reg = pOpn->reg + (bank * 0x100);
	for (i=0x30; i<0xa0; i++) {
		opngen_setreg(chbase, i, reg[i]);
	}
	for (i=0xb7; i>=0xa0; i--) {
		opngen_setreg(chbase, i, reg[i]);
	}
	for (i=0; i<3; i++) {
		opngen_keyon(chbase + i, opngen.keyreg[chbase + i]);
	}
}

void fmboard_rhyrestore2(OPN_T* pOpn, RHYTHM rhy, UINT bank) {

const UINT8	*reg;

	reg = pOpn->reg + (bank * 0x100);
	rhythm_setreg(rhy, 0x11, reg[0x11]);
	rhythm_setreg(rhy, 0x18, reg[0x18]);
	rhythm_setreg(rhy, 0x19, reg[0x19]);
	rhythm_setreg(rhy, 0x1a, reg[0x1a]);
	rhythm_setreg(rhy, 0x1b, reg[0x1b]);
	rhythm_setreg(rhy, 0x1c, reg[0x1c]);
	rhythm_setreg(rhy, 0x1d, reg[0x1d]);
}

#ifdef __cplusplus
extern "C" {
#endif

void tms3631_initialize(UINT rate);
void tms3631_setvol(const UINT8 *vol);

void tms3631_reset(TMS3631 tms);
void tms3631_setkey(TMS3631 tms, REG8 ch, REG8 key);
void tms3631_setenable(TMS3631 tms, REG8 enable);

void SOUNDCALL tms3631_getpcm(TMS3631 tms, SINT32 *pcm, UINT count);

#ifdef __cplusplus
}
#endif

void fmboard_extreg(void (*ext)(REG8 enable));
void fmboard_extenable(REG8 enable);

void fmboard_reset(const NP2CFG *pConfig, UINT32 type);
void fmboard_bind(void);

void fmboard_fmrestore(REG8 chbase, UINT bank);
void fmboard_rhyrestore(RHYTHM rhy, UINT bank);

void fmboard_fmrestore2(OPN_T* pOpn, REG8 chbase, UINT bank);
void fmboard_rhyrestore2(OPN_T* pOpn, RHYTHM rhy, UINT bank);

#ifdef __cplusplus
}
#endif

#else

#define	fmboard_reset(t)
#define	fmboard_bind()

#endif

#ifdef __cplusplus
extern "C" {
#endif

void opngen_initialize(UINT rate);
void opngen_setvol(UINT vol);
void opngen_setVR(REG8 channel, REG8 value);

void opngen_reset(void);
void opngen_setcfg(REG8 maxch, UINT32 flag);
void opngen_setextch(UINT chnum, REG8 data);
void opngen_setreg(REG8 chbase, UINT reg, REG8 value);
void opngen_keyon(UINT chnum, REG8 value);

void SOUNDCALL opngen_getpcm(void *hdl, SINT32 *buf, UINT count);
void SOUNDCALL opngen_getpcmvr(void *hdl, SINT32 *buf, UINT count);

#ifdef __cplusplus
}
#endif

enum {
	NORMAL2608	= 0,
	EXTEND2608	= 1
};

void S98_init(void);
void S98_trash(void);
int S98_open(const OEMCHAR *filename);
void S98_close(void);
void S98_put(REG8 module, UINT addr, REG8 data);
void S98_sync(void);

void S98_put(REG8 module, UINT addr, REG8 data) {
}

static const OEMCHAR file_2608bd[] = OEMTEXT("2608_bd.wav");
static const OEMCHAR file_2608sd[] = OEMTEXT("2608_sd.wav");
static const OEMCHAR file_2608top[] = OEMTEXT("2608_top.wav");
static const OEMCHAR file_2608hh[] = OEMTEXT("2608_hh.wav");
static const OEMCHAR file_2608tom[] = OEMTEXT("2608_tom.wav");
static const OEMCHAR file_2608rim[] = OEMTEXT("2608_rim.wav");

#if 0
static const OEMCHAR *rhythmfile[6] = {
				file_2608bd,	file_2608sd,	file_2608top,
				file_2608hh,	file_2608tom,	file_2608rim};
#endif

typedef struct {
	UINT	rate;
	UINT	pcmexist;
//	PMIXDAT	pcm[6];
	UINT	vol;
	UINT	voltbl[96];
} RHYTHMCFG;

static	RHYTHMCFG	rhythmcfg;


void rhythm_initialize(UINT rate) {

	UINT	i;

	ZeroMemory(&rhythmcfg, sizeof(rhythmcfg));
	rhythmcfg.rate = rate;

	for (i=0; i<96; i++) {
		rhythmcfg.voltbl[i] = (UINT)(32768.0 *
										pow(2.0, (double)i * (-3.0) / 40.0));
	}
}

void rhythm_deinitialize(void) {

	UINT	i;
	SINT16	*ptr;

	for (i=0; i<6; i++) {
//		ptr = rhythmcfg.pcm[i].sample;
//		rhythmcfg.pcm[i].sample = NULL;
//		if (ptr) {
//			_MFREE(ptr);
//		}
	}

    (void)ptr;
}

static void rhythm_load(void) {

	int		i;
//	OEMCHAR	path[MAX_PATH];

	for (i=0; i<6; i++) {
//		if (rhythmcfg.pcm[i].sample == NULL) {
//			getbiospath(path, rhythmfile[i], NELEMENTS(path));
//			pcmmix_regfile(rhythmcfg.pcm + i, path, rhythmcfg.rate);
//		}
	}
}

UINT rhythm_getcaps(void) {

	UINT	ret;
	UINT	i;

	ret = 0;
	for (i=0; i<6; i++) {
//		if (rhythmcfg.pcm[i].sample) {
//			ret |= 1 << i;
//		}
	}
	return(ret);
}

void rhythm_setvol(UINT vol) {

	rhythmcfg.vol = vol;
}

void rhythm_reset(RHYTHM rhy) {

	ZeroMemory(rhy, sizeof(_RHYTHM));
}

void rhythm_bind(RHYTHM rhy) {

	UINT	i;

	rhythm_load();
//	rhy->hdr.enable = 0x3f;
	for (i=0; i<6; i++) {
//		rhy->trk[i].data = rhythmcfg.pcm[i];
	}
	rhythm_update(rhy);
//	sound_streamregist(rhy, (SOUNDCB)pcmmix_getpcm);
}

void rhythm_update(RHYTHM rhy) {

	UINT	i;

	for (i=0; i<6; i++) {
//		rhy->trk[i].volume = (rhythmcfg.voltbl[rhy->vol + rhy->trkvol[i]] *
//														rhythmcfg.vol) >> 10;
	}
}

void rhythm_setreg(RHYTHM rhy, UINT reg, REG8 value) {

//	PMIXTRK	*trk;
	REG8	bit;

	if (reg == 0x10) {
		sound_sync();
//		trk = rhy->trk;
		bit = 0x01;
		do {
			if (value & bit) {
				if (value & 0x80) {
//					rhy->hdr.playing &= ~((UINT)bit);
				}
//				else if (trk->data.sample) {
//					trk->pcm = trk->data.sample;
//					trk->remain = trk->data.samples;
//					rhy->hdr.playing |= bit;
//				}
			}
//			trk++;
			bit <<= 1;
		} while(bit < 0x40);
	}
	else if (reg == 0x11) {
		sound_sync();
		rhy->vol = (~value) & 0x3f;
		rhythm_update(rhy);
	}
	else if ((reg >= 0x18) && (reg < 0x1e)) {
		sound_sync();
//		trk = rhy->trk + (reg - 0x18);
//		trk->flag = ((value & 0x80) >> 7) + ((value & 0x40) >> 5);
		value = (~value) & 0x1f;
		rhy->trkvol[reg - 0x18] = (UINT8)value;
//		trk->volume = (rhythmcfg.voltbl[rhy->vol + value] *
//														rhythmcfg.vol) >> 10;
	}
}

//-----------------------------------------------------------
// Neko Project II: sound/fmtimer.h
//-----------------------------------------------------------

//void fmport_a(NEVENTITEM item);
//void fmport_b(NEVENTITEM item);

/////// from sound/fmboard.c

//-----------------------------------------------------------
// Neko Project II: sound/fmtimer.c
//-----------------------------------------------------------

static const UINT8 irqtable[4] = {0x03, 0x0d, 0x0a, 0x0c};

int pc98_fm_irq = 3; /* TODO: Make configurable */
unsigned int pc98_fm_base = 0x188; /* TODO: Make configurable */

static void set_fmtimeraevent(BOOL absolute);
static void set_fmtimerbevent(BOOL absolute);
void fmport_a(NEVENTITEM item);
void fmport_b(NEVENTITEM item);

bool FMPORT_EventA_set = false;
bool FMPORT_EventB_set = false;

static void FMPORT_EventA(Bitu val) {
    FMPORT_EventA_set = false;
    fmport_a(NULL);
    (void)val;
}

static void FMPORT_EventB(Bitu val) {
    FMPORT_EventB_set = false;
    fmport_b(NULL);
    (void)val;
}

BOOL pcm86gen_intrq(void) {
	if (pcm86.irqflag) {
		return(TRUE);
	}
	if (pcm86.fifo & 0x20) {
		sound_sync();
		if ((pcm86.reqirq) && (pcm86.virbuf <= pcm86.fifosize)) {
			pcm86.reqirq = 0;
			pcm86.irqflag = 1;
			return(TRUE);
		}
	}
	return(FALSE);
}

void fmport_a(NEVENTITEM item) {

    BOOL	intreq = FALSE;

    intreq = pcm86gen_intrq();
    if (fmtimer.reg & 0x04) {
        fmtimer.status |= 0x01;
        intreq = TRUE;
    }

    if (intreq)
        PIC_ActivateIRQ(pc98_fm_irq);

    set_fmtimeraevent(FALSE);
}

void fmport_b(NEVENTITEM item) {

    BOOL	intreq = FALSE;

    intreq = pcm86gen_intrq();
    if (fmtimer.reg & 0x08) {
        fmtimer.status |= 0x02;
        intreq = TRUE;
    }

    if (intreq)
        PIC_ActivateIRQ(pc98_fm_irq);

    set_fmtimerbevent(FALSE);
}

static void set_fmtimeraevent(BOOL absolute) {

	SINT32	l;
    double dt;

	l = 18 * (1024 - fmtimer.timera);
    dt = ((double)l * 1000) / 1000000; // FIXME: GUESS!!!!
//	if (PIT_TICK_RATE == PIT_TICK_RATE_PC98_8MHZ) {	 // 4MHz
//		l = (l * 1248 / 625) * pccore.multiple;    <- NOTE: This becomes l * 1996800Hz * multiple
//	}
//	else {										// 5MHz
//		l = (l * 1536 / 625) * pccore.multiple;    <- NOTE: This becomes l * 2457600Hz * multiple
//	}
//	TRACEOUT(("FMTIMER-A: %08x-%d", l, absolute));
//	nevent_set(NEVENT_FMTIMERA, l, fmport_a, absolute);

    PIC_RemoveEvents(FMPORT_EventA);
    PIC_AddEvent(FMPORT_EventA, dt);
    FMPORT_EventA_set = true;

    (void)l;
}

static void set_fmtimerbevent(BOOL absolute) {

	SINT32	l;
    double dt;

	l = 288 * (256 - fmtimer.timerb);
    dt = ((double)l * 1000) / 1000000; // FIXME: GUESS!!!!
//	if (PIT_TICK_RATE == PIT_TICK_RATE_PC98_8MHZ) {	 // 4MHz
//		l = (l * 1248 / 625) * pccore.multiple;
//	}
//	else {										// 5MHz
//		l = (l * 1536 / 625) * pccore.multiple;
//	}
//	TRACEOUT(("FMTIMER-B: %08x-%d", l, absolute));
//	nevent_set(NEVENT_FMTIMERB, l, fmport_b, absolute);

    PIC_RemoveEvents(FMPORT_EventB);
    PIC_AddEvent(FMPORT_EventB, dt);
    FMPORT_EventB_set = true;

    (void)l;
}

void fmtimer_reset(UINT irq) {

	ZeroMemory(&fmtimer, sizeof(fmtimer));
	fmtimer.intr = irq & 0xc0;
	fmtimer.intdisabel = irq & 0x10;
	fmtimer.irq = irqtable[irq >> 6];
//	pic_registext(fmtimer.irq);
}

void fmtimer_setreg(UINT reg, REG8 value) {

//	TRACEOUT(("fm %x %x [%.4x:%.4x]", reg, value, CPU_CS, CPU_IP));

	switch(reg) {
		case 0x24:
			fmtimer.timera = (value << 2) + (fmtimer.timera & 3);
			break;

		case 0x25:
			fmtimer.timera = (fmtimer.timera & 0x3fc) + (value & 3);
			break;

		case 0x26:
			fmtimer.timerb = value;
			break;

		case 0x27:
			fmtimer.reg = value;
			fmtimer.status &= ~((value & 0x30) >> 4);
			if (value & 0x01) {
                if (!FMPORT_EventA_set)
                    set_fmtimeraevent(0);
            }
            else {
                FMPORT_EventA_set = false;
                PIC_RemoveEvents(FMPORT_EventA);
            }

            if (value & 0x02) {
                if (!FMPORT_EventB_set)
                    set_fmtimerbevent(0);
            }
            else {
                FMPORT_EventB_set = false;
                PIC_RemoveEvents(FMPORT_EventB);
            }

			if (!(value & 0x03)) {
                PIC_DeActivateIRQ(pc98_fm_irq);
            }
			break;
	}
}


/////////////////////////////////////////////////////////////

void board86c_bind(void) {

	fmboard_fmrestore(0, 0);
	fmboard_fmrestore(3, 1);
	psggen_restore(&psg1);
	fmboard_rhyrestore(&rhythm, 0);
//	sound_streamregist(&opngen, (SOUNDCB)opngen_getpcm);
//	sound_streamregist(&psg1, (SOUNDCB)psggen_getpcm);
	rhythm_bind(&rhythm);
//	sound_streamregist(&adpcm, (SOUNDCB)adpcm_getpcm);
	pcm86io_bind();
//	cbuscore_attachsndex(0x188 + opn.base, opnac_o, opnac_i);
}

void pc98_fm_write(Bitu port,Bitu val,Bitu iolen) {
    unsigned char dat = val;

//    LOG_MSG("PC-98 FM: Write port 0x%x val 0x%x",(unsigned int)port,(unsigned int)val);

    switch (port+0x88-pc98_fm_base) {
        case 0x88:
            opn.addr = dat;
            opn.data = dat;
            break;
        case 0x8A:
            {
                UINT    addr;

                opn.data = dat;
                addr = opn.addr;
                if (addr >= 0x100) {
                    return;
                }
                S98_put(NORMAL2608, addr, dat);
                if (addr < 0x10) {
                    if (addr != 0x0e) {
                        psggen_setreg(&psg1, addr, dat);
                    }
                }
                else {
                    if (addr < 0x20) {
                        if (opn.extend) {
                            rhythm_setreg(&rhythm, addr, dat);
                        }
                    }
                    else if (addr < 0x30) {
                        if (addr == 0x28) {
                            if ((dat & 0x0f) < 3) {
                                opngen_keyon(dat & 0x0f, dat);
                            }
                            else if (((dat & 0x0f) != 3) &&
                                    ((dat & 0x0f) < 7)) {
                                opngen_keyon((dat & 0x07) - 1, dat);
                            }
                        }
                        else {
                            fmtimer_setreg(addr, dat);
                            if (addr == 0x27) {
                                opnch[2].extop = dat & 0xc0;
                            }
                        }
                    }
                    else if (addr < 0xc0) {
                        opngen_setreg(0, addr, dat);
                    }
                    opn.reg[addr] = dat;
                }
            }
            break;
        case 0x8C:
            if (opn.extend) {
                opn.addr = dat + 0x100;
                opn.data = dat;
            }
            break;
        case 0x8E:
            {
                UINT    addr;

                if (!opn.extend) {
                    return;
                }
                opn.data = dat;
                addr = opn.addr - 0x100;
                if (addr >= 0x100) {
                    return;
                }
                S98_put(EXTEND2608, addr, dat);
                opn.reg[addr + 0x100] = dat;
                if (addr >= 0x30) {
                    opngen_setreg(3, addr, dat);
                }
                else {
                    if (addr == 0x10) {
                        if (!(dat & 0x80)) {
                            opn.adpcmmask = ~(dat & 0x1c);
                        }
                    }
                }
                (void)port;
            }
            break;
        default:
            LOG_MSG("PC-98 FM: Write port 0x%x val 0x%x unknown",(unsigned int)port,(unsigned int)val);
            break;
    };
}

Bitu pc98_fm_read(Bitu port,Bitu iolen) {
//    LOG_MSG("PC-98 FM: Read port 0x%x",(unsigned int)port);

    switch (port+0x88-pc98_fm_base) {
        case 0x88:
            return fmtimer.status;
        case 0x8A:
            {
                UINT addr;

                addr = opn.addr;
                if (addr == 0x0e) {
                    return 0x3F; // NTS: Returning 0x00 causes games to think a joystick is attached
//                    return(fmboard_getjoy(&psg1));
                }
                else if (addr < 0x10) {
                    return(psggen_getreg(&psg1, addr));
                }
                else if (addr == 0xff) {
                    return(1);
                }
            }
            return(opn.data);
        case 0x8C:
            if (opn.extend) {
                return((fmtimer.status & 3) | (opn.adpcmmask & 8));
            }
            (void)port;
            return(0xff);
        case 0x8E:
            if (opn.extend) {
                UINT addr = opn.addr - 0x100;
                if ((addr == 0x08) || (addr == 0x0f)) {
                    return(opn.reg[addr + 0x100]);
                }
                return(opn.data);
            }
            (void)port;
            return(0xff);
        default:
            LOG_MSG("PC-98 FM: Read port 0x%x unknown",(unsigned int)port);
            break;
    };

    return ~0;
}

static const UINT adpcmdeltatable[8] = {
		//	0.89,	0.89,	0.89,	0.89,	1.2,	1.6,	2.0,	2.4
			228,	228,	228,	228,	308,	408,	512,	612};


REG8 SOUNDCALL adpcm_readsample(ADPCM ad) {

	UINT32	pos;
	REG8	data;
	REG8	ret;

	if ((ad->reg.ctrl1 & 0x60) == 0x20) {
		pos = ad->pos & 0x1fffff;
		if (!(ad->reg.ctrl2 & 2)) {
			data = ad->buf[pos >> 3];
			pos += 8;
		}
		else {
			const UINT8 *ptr;
			REG8 bit;
			UINT tmp;
			ptr = ad->buf + ((pos >> 3) & 0x7fff);
			bit = 1 << (pos & 7);
			tmp = (ptr[0x00000] & bit);
			tmp += (ptr[0x08000] & bit) << 1;
			tmp += (ptr[0x10000] & bit) << 2;
			tmp += (ptr[0x18000] & bit) << 3;
			tmp += (ptr[0x20000] & bit) << 4;
			tmp += (ptr[0x28000] & bit) << 5;
			tmp += (ptr[0x30000] & bit) << 6;
			tmp += (ptr[0x38000] & bit) << 7;
			data = (REG8)(tmp >> (pos & 7));
			pos++;
		}
		if (pos != ad->stop) {
			pos &= 0x1fffff;
			ad->status |= 4;
		}
		if (pos >= ad->limit) {
			pos = 0;
		}
		ad->pos = pos;
	}
	else {
		data = 0;
	}
	pos = ad->fifopos;
	ret = ad->fifo[ad->fifopos];
	ad->fifo[ad->fifopos] = data;
	ad->fifopos ^= 1;
	return(ret);
}

void SOUNDCALL adpcm_datawrite(ADPCM ad, REG8 data) {

	UINT32	pos;

	pos = ad->pos & 0x1fffff;
	if (!(ad->reg.ctrl2 & 2)) {
		ad->buf[pos >> 3] = data;
		pos += 8;
	}
	else {
		UINT8 *ptr;
		UINT8 bit;
		UINT8 mask;
		ptr = ad->buf + ((pos >> 3) & 0x7fff);
		bit = 1 << (pos & 7);
		mask = ~bit;
		ptr[0x00000] &= mask;
		if (data & 0x01) {
			ptr[0x00000] |= bit;
		}
		ptr[0x08000] &= mask;
		if (data & 0x02) {
			ptr[0x08000] |= bit;
		}
		ptr[0x10000] &= mask;
		if (data & 0x04) {
			ptr[0x10000] |= bit;
		}
		ptr[0x18000] &= mask;
		if (data & 0x08) {
			ptr[0x18000] |= bit;
		}
		ptr[0x20000] &= mask;
		if (data & 0x10) {
			ptr[0x20000] |= bit;
		}
		ptr[0x28000] &= mask;
		if (data & 0x20) {
			ptr[0x28000] |= bit;
		}
		ptr[0x30000] &= mask;
		if (data & 0x40) {
			ptr[0x30000] |= bit;
		}
		ptr[0x38000] &= mask;
		if (data & 0x80) {
			ptr[0x38000] |= bit;
		}
		pos++;
	}
	if (pos == ad->stop) {
		pos &= 0x1fffff;
		ad->status |= 4;
	}
	if (pos >= ad->limit) {
		pos = 0;
	}
	ad->pos = pos;
}

void SOUNDCALL pcm86gen_checkbuf(void) {
#if 0
	long	bufs;
	UINT32	past;

	past = CPU_CLOCK + CPU_BASECLOCK - CPU_REMCLOCK;
	past <<= 6;
	past -= pcm86.lastclock;
	if (past >= pcm86.stepclock) {
		past = past / pcm86.stepclock;
		pcm86.lastclock += (past * pcm86.stepclock);
		RECALC_NOWCLKWAIT(past);
	}

	bufs = pcm86.realbuf - pcm86.virbuf;
	if (bufs < 0) {									// ¿Äéc
		bufs &= ~3;
		pcm86.virbuf += bufs;
		if (pcm86.virbuf <= pcm86.fifosize) {
			pcm86.reqirq = 0;
			pcm86.irqflag = 1;
			pic_setirq(fmtimer.irq);
		}
		else {
			pcm86_setnextintr();
		}
	}
	else {
		bufs -= PCM86_EXTBUF;
		if (bufs > 0) {
			bufs &= ~3;
			pcm86.realbuf -= bufs;
			pcm86.readpos += bufs;
		}
	}
#endif
}

#define	PCM86GET8(a)													\
		(a) = (SINT8)pcm86.buffer[pcm86.readpos & PCM86_BUFMSK] << 8;	\
		pcm86.readpos++;

#define	PCM86GET16(a)													\
		(a) = (SINT8)pcm86.buffer[pcm86.readpos & PCM86_BUFMSK] << 8;	\
		pcm86.readpos++;												\
		(a) += pcm86.buffer[pcm86.readpos & PCM86_BUFMSK];				\
		pcm86.readpos++;

#define	BYVOLUME(s)		((((s) >> 6) * pcm86.volume) >> (PCM86_DIVBIT + 4))


static void pcm86mono16(SINT32 *pcm, UINT count) {

	if (pcm86.div < PCM86_DIVENV) {					// Abv³ñÕé
		do {
			SINT32 smp;
			if (pcm86.divremain < 0) {
				SINT32 dat;
				pcm86.divremain += PCM86_DIVENV;
				pcm86.realbuf -= 2;
				if (pcm86.realbuf < 0) {
					goto pm16_bufempty;
				}
				PCM86GET16(dat);
				pcm86.lastsmp = pcm86.smp;
				pcm86.smp = dat;
			}
			smp = (pcm86.lastsmp * pcm86.divremain) -
							(pcm86.smp * (pcm86.divremain - PCM86_DIVENV));
			pcm[0] += BYVOLUME(smp);
			pcm += 2;
			pcm86.divremain -= pcm86.div;
		} while(--count);
	}
	else {
		do {
			SINT32 smp;
			smp = pcm86.smp * (pcm86.divremain * -1);
			pcm86.divremain += PCM86_DIVENV;
			while(1) {
				SINT32 dat;
				pcm86.realbuf -= 2;
				if (pcm86.realbuf < 0) {
					goto pm16_bufempty;
				}
				PCM86GET16(dat);
				pcm86.lastsmp = pcm86.smp;
				pcm86.smp = dat;
				if (pcm86.divremain > pcm86.div2) {
					pcm86.divremain -= pcm86.div2;
					smp += pcm86.smp * pcm86.div2;
				}
				else {
					break;
				}
			}
			smp += pcm86.smp * pcm86.divremain;
			pcm[0] += BYVOLUME(smp);
			pcm += 2;
			pcm86.divremain -= pcm86.div2;
		} while(--count);
	}
	return;

pm16_bufempty:
	pcm86.realbuf += 2;
	pcm86.divremain = 0;
	pcm86.smp = 0;
	pcm86.lastsmp = 0;
}

static void pcm86stereo16(SINT32 *pcm, UINT count) {

	if (pcm86.div < PCM86_DIVENV) {					// Abv³ñÕé
		do {
			SINT32 smp;
			if (pcm86.divremain < 0) {
				SINT32 dat;
				pcm86.divremain += PCM86_DIVENV;
				pcm86.realbuf -= 4;
				if (pcm86.realbuf < 0) {
					goto ps16_bufempty;
				}
				PCM86GET16(dat);
				pcm86.lastsmp_l = pcm86.smp_l;
				pcm86.smp_l = dat;
				PCM86GET16(dat);
				pcm86.lastsmp_r = pcm86.smp_r;
				pcm86.smp_r = dat;
			}
			smp = (pcm86.lastsmp_l * pcm86.divremain) -
							(pcm86.smp_l * (pcm86.divremain - PCM86_DIVENV));
			pcm[0] += BYVOLUME(smp);
			smp = (pcm86.lastsmp_r * pcm86.divremain) -
							(pcm86.smp_r * (pcm86.divremain - PCM86_DIVENV));
			pcm[1] += BYVOLUME(smp);
			pcm += 2;
			pcm86.divremain -= pcm86.div;
		} while(--count);
	}
	else {
		do {
			SINT32 smp_l;
			SINT32 smp_r;
			smp_l = pcm86.smp_l * (pcm86.divremain * -1);
			smp_r = pcm86.smp_r * (pcm86.divremain * -1);
			pcm86.divremain += PCM86_DIVENV;
			while(1) {
				SINT32 dat;
				pcm86.realbuf -= 4;
				if (pcm86.realbuf < 4) {
					goto ps16_bufempty;
				}
				PCM86GET16(dat);
				pcm86.lastsmp_l = pcm86.smp_l;
				pcm86.smp_l = dat;
				PCM86GET16(dat);
				pcm86.lastsmp_r = pcm86.smp_r;
				pcm86.smp_r = dat;
				if (pcm86.divremain > pcm86.div2) {
					pcm86.divremain -= pcm86.div2;
					smp_l += pcm86.smp_l * pcm86.div2;
					smp_r += pcm86.smp_r * pcm86.div2;
				}
				else {
					break;
				}
			}
			smp_l += pcm86.smp_l * pcm86.divremain;
			smp_r += pcm86.smp_r * pcm86.divremain;
			pcm[0] += BYVOLUME(smp_l);
			pcm[1] += BYVOLUME(smp_r);
			pcm += 2;
			pcm86.divremain -= pcm86.div2;
		} while(--count);
	}
	return;

ps16_bufempty:
	pcm86.realbuf += 4;
	pcm86.divremain = 0;
	pcm86.smp_l = 0;
	pcm86.smp_r = 0;
	pcm86.lastsmp_l = 0;
	pcm86.lastsmp_r = 0;
}

static void pcm86mono8(SINT32 *pcm, UINT count) {

	if (pcm86.div < PCM86_DIVENV) {					// Abv³ñÕé
		do {
			SINT32 smp;
			if (pcm86.divremain < 0) {
				SINT32 dat;
				pcm86.divremain += PCM86_DIVENV;
				pcm86.realbuf--;
				if (pcm86.realbuf < 0) {
					goto pm8_bufempty;
				}
				PCM86GET8(dat);
				pcm86.lastsmp = pcm86.smp;
				pcm86.smp = dat;
			}
			smp = (pcm86.lastsmp * pcm86.divremain) -
							(pcm86.smp * (pcm86.divremain - PCM86_DIVENV));
			pcm[0] += BYVOLUME(smp);
			pcm += 2;
			pcm86.divremain -= pcm86.div;
		} while(--count);
	}
	else {
		do {
			SINT32 smp;
			smp = pcm86.smp * (pcm86.divremain * -1);
			pcm86.divremain += PCM86_DIVENV;
			while(1) {
				SINT32 dat;
				pcm86.realbuf--;
				if (pcm86.realbuf < 0) {
					goto pm8_bufempty;
				}
				PCM86GET8(dat);
				pcm86.lastsmp = pcm86.smp;
				pcm86.smp = dat;
				if (pcm86.divremain > pcm86.div2) {
					pcm86.divremain -= pcm86.div2;
					smp += pcm86.smp * pcm86.div2;
				}
				else {
					break;
				}
			}
			smp += pcm86.smp * pcm86.divremain;
			pcm[0] += BYVOLUME(smp);
			pcm += 2;
			pcm86.divremain -= pcm86.div2;
		} while(--count);
	}
	return;

pm8_bufempty:
	pcm86.realbuf += 1;
	pcm86.divremain = 0;
	pcm86.smp = 0;
	pcm86.lastsmp = 0;
}

static void pcm86stereo8(SINT32 *pcm, UINT count) {

	if (pcm86.div < PCM86_DIVENV) {					// Abv³ñÕé
		do {
			SINT32 smp;
			if (pcm86.divremain < 0) {
				SINT32 dat;
				pcm86.divremain += PCM86_DIVENV;
				pcm86.realbuf -= 2;
				if (pcm86.realbuf < 0) {
					goto pm8_bufempty;
				}
				PCM86GET8(dat);
				pcm86.lastsmp_l = pcm86.smp_l;
				pcm86.smp_l = dat;
				PCM86GET8(dat);
				pcm86.lastsmp_r = pcm86.smp_r;
				pcm86.smp_r = dat;
			}
			smp = (pcm86.lastsmp_l * pcm86.divremain) -
							(pcm86.smp_l * (pcm86.divremain - PCM86_DIVENV));
			pcm[0] += BYVOLUME(smp);
			smp = (pcm86.lastsmp_r * pcm86.divremain) -
							(pcm86.smp_r * (pcm86.divremain - PCM86_DIVENV));
			pcm[1] += BYVOLUME(smp);
			pcm += 2;
			pcm86.divremain -= pcm86.div;
		} while(--count);
	}
	else {
		do {
			SINT32 smp_l;
			SINT32 smp_r;
			smp_l = pcm86.smp_l * (pcm86.divremain * -1);
			smp_r = pcm86.smp_r * (pcm86.divremain * -1);
			pcm86.divremain += PCM86_DIVENV;
			while(1) {
				SINT32 dat;
				pcm86.realbuf -= 2;
				if (pcm86.realbuf < 0) {
					goto pm8_bufempty;
				}
				PCM86GET8(dat);
				pcm86.lastsmp_l = pcm86.smp_l;
				pcm86.smp_l = dat;
				PCM86GET8(dat);
				pcm86.lastsmp_r = pcm86.smp_r;
				pcm86.smp_r = dat;
				if (pcm86.divremain > pcm86.div2) {
					pcm86.divremain -= pcm86.div2;
					smp_l += pcm86.smp_l * pcm86.div2;
					smp_r += pcm86.smp_r * pcm86.div2;
				}
				else {
					break;
				}
			}
			smp_l += pcm86.smp_l * pcm86.divremain;
			smp_r += pcm86.smp_r * pcm86.divremain;
			pcm[0] += BYVOLUME(smp_l);
			pcm[1] += BYVOLUME(smp_r);
			pcm += 2;
			pcm86.divremain -= pcm86.div2;
		} while(--count);
	}
	return;

pm8_bufempty:
	pcm86.realbuf += 2;
	pcm86.divremain = 0;
	pcm86.smp_l = 0;
	pcm86.smp_r = 0;
	pcm86.lastsmp_l = 0;
	pcm86.lastsmp_r = 0;
}

void SOUNDCALL pcm86gen_getpcm(void *hdl, SINT32 *pcm, UINT count) {

	if ((count) && (pcm86.fifo & 0x80) && (pcm86.div)) {
		switch(pcm86.dactrl & 0x70) {
			case 0x00:						// 16bit-none
				break;

			case 0x10:						// 16bit-right
				pcm86mono16(pcm + 1, count);
				break;

			case 0x20:						// 16bit-left
				pcm86mono16(pcm, count);
				break;

			case 0x30:						// 16bit-stereo
				pcm86stereo16(pcm, count);
				break;

			case 0x40:						// 8bit-none
				break;

			case 0x50:						// 8bit-right
				pcm86mono8(pcm + 1, count);
				break;

			case 0x60:						// 8bit-left
				pcm86mono8(pcm, count);
				break;

			case 0x70:						// 8bit-stereo
				pcm86stereo8(pcm, count);
				break;
		}
		pcm86gen_checkbuf();
	}
	(void)hdl;
}

#define	ADPCM_NBR	0x80000000

static void SOUNDCALL getadpcmdata(ADPCM ad) {

	UINT32	pos;
	UINT	data;
	UINT	dir;
	SINT32	dlt;
	SINT32	samp;

	pos = ad->pos;
	if (!(ad->reg.ctrl2 & 2)) {
		data = ad->buf[(pos >> 3) & 0x3ffff];
		if (!(pos & ADPCM_NBR)) {
			data >>= 4;
		}
		pos += ADPCM_NBR + 4;
	}
	else {
		const UINT8 *ptr;
		REG8 bit;
		UINT tmp;
		ptr = ad->buf + ((pos >> 3) & 0x7fff);
		bit = 1 << (pos & 7);
		if (!(pos & ADPCM_NBR)) {
			tmp = (ptr[0x20000] & bit);
			tmp += (ptr[0x28000] & bit) << 1;
			tmp += (ptr[0x30000] & bit) << 2;
			tmp += (ptr[0x38000] & bit) << 3;
			data = tmp >> (pos & 7);
			pos += ADPCM_NBR;
		}
		else {
			tmp = (ptr[0x00000] & bit);
			tmp += (ptr[0x08000] & bit) << 1;
			tmp += (ptr[0x10000] & bit) << 2;
			tmp += (ptr[0x18000] & bit) << 3;
			data = tmp >> (pos & 7);
			pos += ADPCM_NBR + 1;
		}
	}
	dir = data & 8;
	data &= 7;
	dlt = adpcmdeltatable[data] * ad->delta;
	dlt >>= 8;
	if (dlt < 127) {
		dlt = 127;
	}
	else if (dlt > 24000) {
		dlt = 24000;
	}
	samp = ad->delta;
	ad->delta = dlt;
	samp *= ((data * 2) + 1);
	samp >>= ADPCM_SHIFT;
	if (!dir) {
		samp += ad->samp;
		if (samp > 32767) {
			samp = 32767;
		}
	}
	else {
		samp = ad->samp - samp;
		if (samp < -32767) {
			samp = -32767;
		}
	}
	ad->samp = samp;

	if (!(pos & ADPCM_NBR)) {
		if (pos == ad->stop) {
			if (ad->reg.ctrl1 & 0x10) {
				pos = ad->start;
				ad->samp = 0;
				ad->delta = 127;
			}
			else {
				pos &= 0x1fffff;
				ad->status |= 4;
				ad->play = 0;
			}
		}
		else if (pos >= ad->limit) {
			pos = 0;
		}
	}
	ad->pos = pos;
	samp *= ad->level;
	samp >>= (10 + 1);
	ad->out0 = ad->out1;
	ad->out1 = samp + ad->fb;
	ad->fb = samp >> 1;
}

void SOUNDCALL adpcm_getpcm(ADPCM ad, SINT32 *pcm, UINT count) {

	SINT32	remain;
	SINT32	samp;

	if ((count == 0) || (ad->play == 0)) {
		return;
	}
	remain = ad->remain;
	if (ad->step <= ADTIMING) {
		do {
			if (remain < 0) {
				remain += ADTIMING;
				getadpcmdata(ad);
				if (ad->play == 0) {
					if (remain > 0) {
						do {
							samp = (ad->out0 * remain) >> ADTIMING_BIT;
							if (ad->reg.ctrl2 & 0x80) {
								pcm[0] += samp;
							}
							if (ad->reg.ctrl2 & 0x40) {
								pcm[1] += samp;
							}
							pcm += 2;
							remain -= ad->step;
						} while((remain > 0) && (--count));
					}
					goto adpcmstop;
				}
			}
			samp = (ad->out0 * remain) + (ad->out1 * (ADTIMING - remain));
			samp >>= ADTIMING_BIT;
			if (ad->reg.ctrl2 & 0x80) {
				pcm[0] += samp;
			}
			if (ad->reg.ctrl2 & 0x40) {
				pcm[1] += samp;
			}
			pcm += 2;
			remain -= ad->step;
		} while(--count);
	}
	else {
		do {
			if (remain > 0) {
				samp = ad->out0 * (ADTIMING - remain);
				do {
					getadpcmdata(ad);
					if (ad->play == 0) {
						goto adpcmstop;
					}
					samp += ad->out0 * min(remain, ad->pertim);
					remain -= ad->pertim;
				} while(remain > 0);
			}
			else {
				samp = ad->out0 * ADTIMING;
			}
			remain += ADTIMING;
			samp >>= ADTIMING_BIT;
			if (ad->reg.ctrl2 & 0x80) {
				pcm[0] += samp;
			}
			if (ad->reg.ctrl2 & 0x40) {
				pcm[1] += samp;
			}
			pcm += 2;
		} while(--count);
	}
	ad->remain = remain;
	return;

adpcmstop:
	ad->out0 = 0;
	ad->out1 = 0;
	ad->fb = 0;
	ad->remain = 0;
}

static	SINT32	randseed = 1;

void rand_setseed(SINT32 seed) {

	randseed = seed;
}

SINT32 rand_get(void) {

	randseed = (randseed * 0x343fd) + 0x269ec3;
	return(randseed >> 16);
}

void SOUNDCALL psggen_getpcm(PSGGEN psg, SINT32 *pcm, UINT count) {

	SINT32	noisevol;
	UINT8	mixer;
	UINT	noisetbl = 0;
	PSGTONE	*tone;
	PSGTONE	*toneterm;
	SINT32	samp;
//	UINT	psgvol;
	SINT32	vol;
	UINT	i;
	UINT	noise;

	if ((psg->mixer & 0x3f) == 0) {
		count = min(count, psg->puchicount);
		psg->puchicount -= count;
	}
	if (count == 0) {
		return;
	}
	do {
		noisevol = 0;
		if (psg->envcnt) {
			psg->envcnt--;
			if (psg->envcnt == 0) {
				psg->envvolcnt--;
				if (psg->envvolcnt < 0) {
					if (psg->envmode & PSGENV_ONESHOT) {
						psg->envvol = (psg->envmode & PSGENV_LASTON)?15:0;
					}
					else {
						psg->envvolcnt = 15;
						if (!(psg->envmode & PSGENV_ONECYCLE)) {
							psg->envmode ^= PSGENV_INC;
						}
						psg->envcnt = psg->envmax;
						psg->envvol = (psg->envvolcnt ^ psg->envmode) & 0x0f;
					}
				}
				else {
					psg->envcnt = psg->envmax;
					psg->envvol = (psg->envvolcnt ^ psg->envmode) & 0x0f;
				}
				psg->evol = psggencfg.volume[psg->envvol];
			}
		}
		mixer = psg->mixer;
		if (mixer & 0x38) {
			for (i=0; i<(1 << PSGADDEDBIT); i++) {
				SINT32 countbak;
				countbak = psg->noise.count;
				psg->noise.count -= psg->noise.freq;
				if (psg->noise.count > countbak) {
//					psg->noise.base = GETRAND() & (1 << (1 << PSGADDEDBIT));
					psg->noise.base = rand_get() & (1 << (1 << PSGADDEDBIT));
				}
				noisetbl += psg->noise.base;
				noisetbl >>= 1;
			}
		}
		tone = psg->tone;
		toneterm = tone + 3;
		do {
			vol = *(tone->pvol);
			if (vol) {
				samp = 0;
				switch(mixer & 9) {
					case 0:							// no mix
						if (tone->puchi) {
							tone->puchi--;
							samp += vol << PSGADDEDBIT;
						}
						break;

					case 1:							// tone only
						for (i=0; i<(1 << PSGADDEDBIT); i++) {
							tone->count += tone->freq;
							samp += vol * ((tone->count>=0)?1:-1);
						}
						break;

					case 8:							// noise only
						noise = noisetbl;
						for (i=0; i<(1 << PSGADDEDBIT); i++) {
							samp += vol * ((noise & 1)?1:-1);
							noise >>= 1;
						}
						break;

					case 9:
						noise = noisetbl;
						for (i=0; i<(1 << PSGADDEDBIT); i++) {
							tone->count += tone->freq;
							if ((tone->count >= 0) || (noise & 1)) {
								samp += vol;
							}
							else {
								samp -= vol;
							}
							noise >>= 1;
						}
						break;
				}
				if (!(tone->pan & 1)) {
					pcm[0] += samp;
				}
				if (!(tone->pan & 2)) {
					pcm[1] += samp;
				}
			}
			mixer >>= 1;
		} while(++tone < toneterm);
		pcm += 2;
	} while(--count);
}

void SOUNDCALL tms3631_getpcm(TMS3631 tms, SINT32 *pcm, UINT count) {

	UINT	ch;
	SINT32	data;
	UINT	i;

	if (tms->enable == 0) {
		return;
	}
	while(count--) {
		ch = 0;
		data = 0;
		do {									// centre
			if ((tms->enable & (1 << ch)) && (tms->ch[ch].freq)) {
				for (i=0; i<4; i++) {
					tms->ch[ch].count += tms->ch[ch].freq;
					data += (tms->ch[ch].count & 0x10000)?1:-1;
				}
			}
		} while(++ch < 2);
		pcm[0] += data * tms3631cfg.left;
		pcm[1] += data * tms3631cfg.right;
		do {									// left
			if ((tms->enable & (1 << ch)) && (tms->ch[ch].freq)) {
				for (i=0; i<4; i++) {
					tms->ch[ch].count += tms->ch[ch].freq;
					pcm[0] += tms3631cfg.feet[(tms->ch[ch].count >> 16) & 15];
				}
			}
		} while(++ch < 5);
		do {									// right
			if ((tms->enable & (1 << ch)) && (tms->ch[ch].freq)) {
				for (i=0; i<4; i++) {
					tms->ch[ch].count += tms->ch[ch].freq;
					pcm[1] += tms3631cfg.feet[(tms->ch[ch].count >> 16) & 15];
				}
			}
		} while(++ch < 8);
		pcm += 2;
	}
}

static void pc98_mix_CallBack(Bitu len) {
    unsigned int s = len;

    if (s > (sizeof(MixTemp)/sizeof(Bit32s)/2))
        s = (sizeof(MixTemp)/sizeof(Bit32s)/2);

    memset(MixTemp,0,sizeof(MixTemp));

    opngen_getpcm(NULL, (SINT32*)MixTemp, s);
    tms3631_getpcm(&tms3631, (SINT32*)MixTemp, s);

    for (unsigned int i=0;i < 3;i++)
        psggen_getpcm(&__psg[i], (SINT32*)MixTemp, s);
    
//    rhythm_getpcm(NULL, (SINT32*)MixTemp, s); FIXME
//    adpcm_getpcm(NULL, (SINT32*)MixTemp, s); FIXME
    pcm86gen_getpcm(NULL, (SINT32*)MixTemp, s);

    pc98_mixer->AddSamples_s32(s, (Bit32s*)MixTemp);
}

void PC98_FM_OnEnterPC98(Section *sec) {
    // TODO:
    //  - Give the user an option in dosbox.conf to enable/disable FM emulation
    //  - Give the user a choice which board to emulate (the borrowed code can emulate 10 different cards)
    //  - Give the user a choice which IRQ to attach it to
    //  - Give the user a choice of base I/O address (0x088 or 0x188)
    //  - Move this code out into it's own file. This is SOUND code. It does not belong in vga.cpp.
    //  - Register the TMS3631, OPNA, PSG, RHYTHM, etc. outputs as individual mixer channels, where
    //    each can then run at their own sample rate, and the user can use DOSBox-X mixer controls to
    //    set volume, record individual tracks with WAV capture, etc.
    //  - Cleanup this code, organize it.
    //  - Make sure this code clearly indicates that it was borrowed and adapted from Neko Project II and
    //    ported to DOSBox-X. I cannot take credit for this code, I can only take credit for porting it
    //    and future refinements in this project.
    LOG_MSG("Initializing FM board at base 0x%x",pc98_fm_base);
    for (unsigned int i=0;i < 8;i += 2) {
        IO_RegisterWriteHandler(pc98_fm_base+i,pc98_fm_write,IO_MB);
        IO_RegisterReadHandler(pc98_fm_base+i,pc98_fm_read,IO_MB);
    }

    // WARNING: Some parts of the borrowed code assume 44100, 22050, or 11025 and
    //          will misrender if given any other sample rate (especially the OPNA synth).
    unsigned int rate = 44100;
    unsigned char vol14[6] = { 15, 15, 15, 15, 15, 15 };

    pc98_mixer = MIXER_AddChannel(pc98_mix_CallBack, rate, "PC-98");
    pc98_mixer->Enable(true);

    fmboard_reset(NULL, 0x14);
    fmboard_extenable(true);

//	fddmtrsnd_initialize(rate);
//	beep_initialize(rate);
//	beep_setvol(3);
	tms3631_initialize(rate);
	tms3631_setvol(vol14);
	opngen_initialize(rate);
	opngen_setvol(128);
	psggen_initialize(rate);
	psggen_setvol(128);
	rhythm_initialize(rate);
	rhythm_setvol(128);
	adpcm_initialize(rate);
	adpcm_setvol(128);
	pcm86gen_initialize(rate);
	pcm86gen_setvol(128);

    board86c_bind();
}

