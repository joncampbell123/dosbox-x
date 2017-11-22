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
#include "programs.h"
#include "support.h"
#include "setup.h"
#include "mem.h"
#include "util_units.h"
#include "control.h"

#include <string.h>
#include <stdlib.h>
#include <string>
#include <stdio.h>

using namespace std;

extern int vga_memio_delay_ns;

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

	LOG(LOG_MISC,LOG_DEBUG)("VGA_Reset() reinitializing VGA emulation");

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

double gdc_proc_delay = 0.001; /* time from FIFO to processing in GDC (1us) FIXME: Is this right? */
bool gdc_proc_delay_set = false;

void GDC_ProcDelay(Bitu /*val*/);

void gdc_proc_schedule_delay(void) {
    if (!gdc_proc_delay_set) {
        PIC_AddEvent(GDC_ProcDelay,(float)gdc_proc_delay);
        gdc_proc_delay_set = false;
    }
}

void gdc_proc_schedule_cancel(void) {
    if (gdc_proc_delay_set) {
        PIC_RemoveEvents(GDC_ProcDelay);
        gdc_proc_delay_set = false;
    }
}

void gdc_proc_schedule_done(void) {
    gdc_proc_delay_set = false;
}

void VGA_DAC_UpdateColor( Bitu index );

#include "inout.h"

PC98_GDC_state::PC98_GDC_state() {
    memset(param_ram,0,sizeof(param_ram));

    // make a display partition area to cover the screen, whatever it is.
    param_ram[0] = 0x00;        // SAD=0
    param_ram[1] = 0x00;        // SAD=0
    param_ram[2] = 0xF0;        // LEN=3FF
    param_ram[3] = 0x3F;        // LEN=3FF WD1=0

    doublescan = false;
    param_ram_wptr = 0;
    display_partition = 0;
    row_line = 0;
    row_height = 16;
    scan_address = 0;
    current_command = 0xFF;
    proc_step = 0xFF;
    display_enable = true;
    display_mode = 0;
    cursor_enable = true;
    cursor_blink_state = 0;
    cursor_blink_count = 0;
    cursor_blink_rate = 0x20;
    video_framing = 0;
    master_sync = false;
    draw_only_during_retrace = 0;
    dynamic_ram_refresh = 0;
    cursor_blink = true;
    idle = false;
    reset_fifo();
    reset_rfifo();
}

enum {
    GDC_CMD_RESET = 0x00,                       // 0   0   0   0   0   0   0   0
    GDC_CMD_DISPLAY_BLANK = 0x0C,               // 0   0   0   0   1   1   0   DE
    GDC_CMD_SYNC = 0x0E,                        // 0   0   0   0   1   1   1   DE
    GDC_CMD_CURSOR_POSITION = 0x49,             // 0   1   0   0   1   0   0   1
    GDC_CMD_CURSOR_CHAR_SETUP = 0x4B,           // 0   1   0   0   1   0   1   1
    GDC_CMD_PITCH_SPEC = 0x47,                  // 0   1   0   0   0   1   1   1
    GDC_CMD_START_DISPLAY = 0x6B,               // 0   1   1   0   1   0   1   1
    GDC_CMD_VERTICAL_SYNC_MODE = 0x6E,          // 0   1   1   0   1   1   1   M
    GDC_CMD_PARAMETER_RAM_LOAD = 0x70           // 0   1   1   1   S   S   S   S    S[3:0] = starting address in parameter RAM
};

size_t PC98_GDC_state::fifo_can_read(void) {
    return fifo_write - fifo_read;
}

void PC98_GDC_state::take_reset_sync_parameters(void) {
    /* P1 = param[0] = 0 0 C F I D G S
     *  CG = [1:0] = display mode
     *  IS = [1:0] = video framing
     *   F = drawing time window
     *   D = dynamic RAM refresh cycles enable */
    draw_only_during_retrace =      !!(cmd_parm_tmp[0] & 0x10); /* F */
    dynamic_ram_refresh =           !!(cmd_parm_tmp[0] & 0x04); /* D */
    display_mode = /* CG = [1:0] */
        ((cmd_parm_tmp[0] & 0x20) ? 2 : 0) +
        ((cmd_parm_tmp[0] & 0x02) ? 1 : 0);
    video_framing = /* IS = [1:0] */
        ((cmd_parm_tmp[0] & 0x08) ? 2 : 0) +
        ((cmd_parm_tmp[0] & 0x01) ? 1 : 0);

    /* P2 = param[1] = AW = active display words per line - 2. must be even number. */
    active_display_words_per_line = (uint16_t)cmd_parm_tmp[1] + 2u;

    /* P3 = param[2] =
     *   VS(L)[2:0] = [7:5] = low bits of VS
     *   HS = [4:0] = horizontal sync width - 1 */
    horizontal_sync_width = (cmd_parm_tmp[2] & 0x1F) + 1;
    vertical_sync_width = (cmd_parm_tmp[2] >> 5);

    /* P4 = param[3] =
     *   HFP = [7:2] = horizontal front porch width - 1
     *   VS(H)[4:3] = [1:0] = high bits of VS
     *
     *   VS = vertical sync width */
    vertical_sync_width += (cmd_parm_tmp[3] & 3) << 3;
    horizontal_front_porch_width = (cmd_parm_tmp[3] >> 2) + 1;

    /* P5 = param[4] =
     *   0 = [7:6] = 0
     *   HBP = [5:0] = horizontal back porch width - 1 */
    horizontal_back_porch_width = (cmd_parm_tmp[4] & 0x3F) + 1;

    /* P6 = param[5] =
     *   0 = [7:6] = 0
     *   VFP = [5:0] = vertical front porch width */
    vertical_front_porch_width = (cmd_parm_tmp[5] & 0x3F);

    /* P7 = param[6] =
     *   AL(L)[7:0] = [7:0] = Active Display Lines per video field, low bits */
    active_display_lines = (cmd_parm_tmp[6] & 0xFF);

    /* P8 = parm[7] =
     *   VBP = [7:2] = vertical back porch width
     *   AL(H)[9:8] = [1:0] = Active Display Lines per video field, high bits */
    active_display_lines += (cmd_parm_tmp[7] & 3) << 8;
    vertical_back_porch_width = cmd_parm_tmp[7] >> 2;

    LOG_MSG("GDC: RESET/SYNC DOOR=%u DRAM=%u DISP=%u VFRAME=%u AW=%u HS=%u VS=%u HFP=%u HBP=%u VFP=%u AL=%u VBP=%u",
        draw_only_during_retrace?1:0,
        dynamic_ram_refresh?1:0,
        display_mode,
        video_framing,
        active_display_words_per_line,
        horizontal_sync_width,
        vertical_sync_width,
        horizontal_front_porch_width,
        horizontal_back_porch_width,
        vertical_front_porch_width,
        active_display_lines,
        vertical_back_porch_width);
}

void PC98_GDC_state::apply_to_video_output(void) {
    /* take our SYNC parameters and feed it into the incumbent VGA raster emulation.
     * do not call this function unless this GDC runs as the master sync source. */

    // TODO
}

void PC98_GDC_state::cursor_advance(void) {
    cursor_blink_count++;
    if (cursor_blink_count == cursor_blink_rate) {
        cursor_blink_count = 0;

        if ((++cursor_blink_state) >= 4)
            cursor_blink_state = 0;
    }
    else if (cursor_blink_count & 0x40) {
        cursor_blink_count = 0;
    }
}

void PC98_GDC_state::take_cursor_pos(unsigned char bi) {
    /* P1 = param[0] = EAD(L) = address[7:0]
     *
     * P2 = param[1] = EAD(M) = address[15:0]
     *
     * P3 = param[2]
     *   dAD = [7:4] = Dot address within the word
     *   0 = [3:2] = 0
     *   EAD(H) = [1:0] = address[17:16] */
    if (bi == 1) {
		vga.config.cursor_start &= ~(0xFF << 0);
		vga.config.cursor_start |=  cmd_parm_tmp[0] << 0;
    }
    else if (bi == 2) {
		vga.config.cursor_start &= ~(0xFF << 8);
		vga.config.cursor_start |=  cmd_parm_tmp[1] << 8;
    }
    else if (bi == 3) {
		vga.config.cursor_start &= ~(0x03 << 16);
		vga.config.cursor_start |=  (cmd_parm_tmp[2] & 3) << 16;

        // TODO: "dot address within the word"
    }
}

void PC98_GDC_state::take_cursor_char_setup(unsigned char bi) {
    /* P1 = param[0] =
     *   DC = [7:7] = display cursor if set
     *   0 = [6:5] = 0
     *   LR = [4:0] = lines per character row - 1 */
    if (bi == 1) {
        cursor_enable = !!(cmd_parm_tmp[0] & 0x80);

		vga.crtc.maximum_scan_line = cmd_parm_tmp[0] & 0x1F;
		vga.draw.address_line_total = vga.crtc.maximum_scan_line + 1;
    }

    /* P2 = param[1] =
     *   BR[1:0] = [7:6] = blink rate
     *   SC = [5:5] = 1=steady cursor  0=blinking cursor
     *   CTOP = [4:0] = cursor top line number in the row */

    /* P3 = param[2] =
     *   CBOT = [7:3] = cursor bottom line number in the row CBOT < LR
     *   BR[4:2] = [2:0] = blink rate */
    if (bi == 3) {
        cursor_blink_rate  = (cmd_parm_tmp[1] >> 6) & 3;
        cursor_blink_rate += (cmd_parm_tmp[2] & 7) << 2;
        if (cursor_blink_rate == 0) cursor_blink_rate = 0x20;
        cursor_blink_rate *= 2;

        cursor_blink = !(cmd_parm_tmp[1] & 0x20);

		vga.crtc.cursor_start = (cmd_parm_tmp[1] & 0x1F);
		vga.draw.cursor.sline = vga.crtc.cursor_start;

		vga.crtc.cursor_end   = (cmd_parm_tmp[2] >> 3) & 0x1F;
		vga.draw.cursor.eline = vga.crtc.cursor_end;
    }

    /* blink-on time + blink-off time = 2 x BR (video frames).
     * attribute blink rate is 3/4 on 1/4 off duty cycle.
     * for interlaced graphics modes, set BR[1:0] = 3 */
}

void PC98_GDC_state::idle_proc(void) {
    Bit16u val;

    if (fifo_empty())
        return;

    val = read_fifo();
    if (val & 0x100) { // command
        current_command = val & 0xFF;
        proc_step = 0;

        switch (current_command) {
            case GDC_CMD_RESET: // 0x00         0 0 0 0 0 0 0 0
                LOG_MSG("GDC: reset");
                display_enable = false;
                idle = true;
                reset_fifo();
                reset_rfifo();
                break;
            case GDC_CMD_DISPLAY_BLANK:  // 0x0C   0 0 0 0 1 1 0 DE
            case GDC_CMD_DISPLAY_BLANK+1:// 0x0D   DE=display enable
                display_enable = !!(current_command & 1); // bit 0 = display enable
                break;
            case GDC_CMD_SYNC:  // 0x0E         0 0 0 0 0 0 0 DE
            case GDC_CMD_SYNC+1:// 0x0F         DE=display enable
                display_enable = !!(current_command & 1); // bit 0 = display enable
                LOG_MSG("GDC: sync");
                break;
            case GDC_CMD_PITCH_SPEC:          // 0x47        0 1 0 0 0 1 1 1
                break;
            case GDC_CMD_CURSOR_POSITION:     // 0x49        0 1 0 0 1 0 0 1
                LOG_MSG("GDC: cursor pos");
                break;
            case GDC_CMD_CURSOR_CHAR_SETUP:   // 0x4B        0 1 0 0 1 0 1 1
                LOG_MSG("GDC: cursor setup");
                break;
            case GDC_CMD_START_DISPLAY:       // 0x6B        0 1 1 0 1 0 1 1
                idle = false;
                break;
            case GDC_CMD_VERTICAL_SYNC_MODE:  // 0x6E        0 1 1 0 1 1 1 M
            case GDC_CMD_VERTICAL_SYNC_MODE+1:// 0x6F        M=generate and output vertical sync (0=or else accept external vsync)
                master_sync = !!(current_command & 1);
                LOG_MSG("GDC: vsyncmode master=%u",master_sync);
                break;
            case GDC_CMD_PARAMETER_RAM_LOAD:   // 0x70       0 1 1 1 S S S S
            case GDC_CMD_PARAMETER_RAM_LOAD+1: // 0x71       S=starting byte in parameter RAM
            case GDC_CMD_PARAMETER_RAM_LOAD+2: // 0x72       S=starting byte in parameter RAM
            case GDC_CMD_PARAMETER_RAM_LOAD+3: // 0x73       S=starting byte in parameter RAM
            case GDC_CMD_PARAMETER_RAM_LOAD+4: // 0x74       S=starting byte in parameter RAM
            case GDC_CMD_PARAMETER_RAM_LOAD+5: // 0x75       S=starting byte in parameter RAM
            case GDC_CMD_PARAMETER_RAM_LOAD+6: // 0x76       S=starting byte in parameter RAM
            case GDC_CMD_PARAMETER_RAM_LOAD+7: // 0x77       S=starting byte in parameter RAM
            case GDC_CMD_PARAMETER_RAM_LOAD+8: // 0x78       S=starting byte in parameter RAM
            case GDC_CMD_PARAMETER_RAM_LOAD+9: // 0x79       S=starting byte in parameter RAM
            case GDC_CMD_PARAMETER_RAM_LOAD+10:// 0x7A       S=starting byte in parameter RAM
            case GDC_CMD_PARAMETER_RAM_LOAD+11:// 0x7B       S=starting byte in parameter RAM
            case GDC_CMD_PARAMETER_RAM_LOAD+12:// 0x7C       S=starting byte in parameter RAM
            case GDC_CMD_PARAMETER_RAM_LOAD+13:// 0x7D       S=starting byte in parameter RAM
            case GDC_CMD_PARAMETER_RAM_LOAD+14:// 0x7E       S=starting byte in parameter RAM
            case GDC_CMD_PARAMETER_RAM_LOAD+15:// 0x7F       S=starting byte in parameter RAM
                param_ram_wptr = current_command & 0xF;
                current_command = GDC_CMD_PARAMETER_RAM_LOAD;
                break;
            default:
                LOG_MSG("GDC: Unknown command 0x%x",current_command);
                break;
        };
    }
    else {
        /* parameter parsing */
        switch (current_command) {
            /* RESET and SYNC take the same 8 byte parameters */
            case GDC_CMD_RESET:
            case GDC_CMD_SYNC:
                if (proc_step < 8) {
                    cmd_parm_tmp[proc_step] = (uint8_t)val;
                    if ((++proc_step) == 8) {
                        take_reset_sync_parameters();
                        if (master_sync) apply_to_video_output();
                    }
                }
                break;
            case GDC_CMD_PITCH_SPEC:
                if (proc_step < 1)
                    active_display_words_per_line = (val != 0) ? val : 0x100;
                break;
            case GDC_CMD_CURSOR_POSITION:
                if (proc_step < 3) {
                    cmd_parm_tmp[proc_step++] = (uint8_t)val;
                    take_cursor_pos(proc_step);
                }
                break;
            case GDC_CMD_CURSOR_CHAR_SETUP:
                if (proc_step < 3) {
                    cmd_parm_tmp[proc_step++] = (uint8_t)val;
                    if (proc_step == 1 || proc_step == 3) {
                        take_cursor_char_setup(proc_step);
                    }
                }
                break;
            case GDC_CMD_PARAMETER_RAM_LOAD:
                param_ram[param_ram_wptr] = (uint8_t)val;
                if ((++param_ram_wptr) >= 16) param_ram_wptr = 0;
                break;
        };
    }

    if (!fifo_empty())
        gdc_proc_schedule_delay();
}

bool PC98_GDC_state::fifo_empty(void) {
    return (fifo_read >= fifo_write);
}

Bit16u PC98_GDC_state::read_fifo(void) {
    Bit16u val;

    val = fifo[fifo_read];
    if (fifo_read < fifo_write)
        fifo_read++;

    return val;
}

void PC98_GDC_state::next_line(void) {
    if ((++row_line) == row_height) {
        scan_address += active_display_words_per_line;
        row_line = 0;
    }
    else if (row_line & 0x20) {
        row_line = 0;
    }

    if (--display_partition_rem_lines == 0) {
        next_display_partition();
        load_display_partition();
    }
}

void PC98_GDC_state::begin_frame(void) {
    row_line = 0;
    scan_address = 0;
    display_partition = 0;

    /* the actual starting address is determined by the display partition in paramter RAM */
    load_display_partition();
}

void PC98_GDC_state::load_display_partition(void) {
    unsigned char *pram = param_ram + (display_partition * 4);

    scan_address  =  pram[0];
    scan_address +=  pram[1]         << 8;
    scan_address += (pram[2] & 0x03) << 16;

    display_partition_rem_lines  =  pram[2]         >> 4;
    display_partition_rem_lines += (pram[3] & 0x3F) << 4;
    if (display_partition_rem_lines == 0)
        display_partition_rem_lines = 0x400;

    if (master_sync) { /* character mode */
    /* RAM+0 = SAD1 (L)
     *
     * RAM+1 = 0 0 0 SAH1 (M) [4:0]
     *
     * RAM+2 = LEN1 (L) [7:4]  0 0 0 0
     *
     * RAM+3 = WD1 0 LEN1 (H) [5:0] */
        scan_address &= 0x1FFF;
    }
    else { /* graphics mode */
    /* RAM+0 = SAD1 (L)
     *
     * RAM+1 = SAH1 (M)
     *
     * RAM+2 = LEN1 (L) [7:4]  0 0   SAD1 (H) [1:0]
     *
     * RAM+3 = WD1 IM LEN1 (H) [5:0] */
    }
}

void PC98_GDC_state::force_fifo_complete(void) {
    while (!fifo_empty())
        idle_proc();
}

void PC98_GDC_state::next_display_partition(void) {
    if ((++display_partition) == 4)
        display_partition = 0;
}

void PC98_GDC_state::reset_fifo(void) {
    fifo_read = fifo_write = 0;
}

void PC98_GDC_state::reset_rfifo(void) {
    rfifo_read = rfifo_write = 0;
}

void PC98_GDC_state::flush_fifo_old(void) {
    if (fifo_read != 0) {
        unsigned int sz = (fifo_read <= fifo_write) ? (fifo_write - fifo_read) : 0;

        for (unsigned int i=0;i < sz;i++)
            fifo[i] = fifo[i+fifo_read];

        fifo_read = 0;
        fifo_write = sz;
    }
}

bool PC98_GDC_state::write_fifo(const uint16_t c) {
    if (fifo_write >= PC98_GDC_FIFO_SIZE)
        flush_fifo_old();
    if (fifo_write >= PC98_GDC_FIFO_SIZE)
        return false;

    fifo[fifo_write++] = c;
    gdc_proc_schedule_delay();
    return true;
}

bool PC98_GDC_state::write_fifo_command(const unsigned char c) {
    return write_fifo(c | GDC_COMMAND_BYTE);
}

bool PC98_GDC_state::write_fifo_param(const unsigned char c) {
    return write_fifo(c);
}

bool PC98_GDC_state::rfifo_has_content(void) {
    return (rfifo_read < rfifo_write);
}

uint8_t PC98_GDC_state::read_status(void) {
    double timeInFrame = PIC_FullIndex()-vga.draw.delay.framestart;
    double timeInLine=fmod(timeInFrame,vga.draw.delay.htotal);
    uint8_t ret;

    ret  = 0x00; // light pen not present

	if (timeInFrame >= vga.draw.delay.vdend) {
        ret |= 0x40; // vertical blanking
    }
    else {
        if (timeInLine >= vga.draw.delay.hblkstart && 
            timeInLine <= vga.draw.delay.hblkend)
            ret |= 0x40; // horizontal blanking
    }

    if (timeInFrame >= vga.draw.delay.vrstart &&
        timeInFrame <= vga.draw.delay.vrend)
        ret |= 0x20; // vertical retrace

    // TODO: 0x10 bit 4 DMA execute

    // TODO: 0x08 bit 3 drawing in progress

    if (fifo_write >= PC98_GDC_FIFO_SIZE)
        flush_fifo_old();

    if (fifo_read == fifo_write)
        ret |= 0x04; // FIFO empty
    if (fifo_write >= PC98_GDC_FIFO_SIZE)
        ret |= 0x02; // FIFO full
    if (rfifo_has_content())
        ret |= 0x01; // data ready

    return ret;
}

uint8_t PC98_GDC_state::rfifo_read_data(void) {
    uint8_t ret;

    ret = rfifo[rfifo_read];
    if (rfifo_read < rfifo_write) {
        if (++rfifo_read >= rfifo_write) {
            rfifo_read = rfifo_write = 0;
            rfifo[0] = ret;
        }
    }

    return ret;
}

uint32_t                    pc98_text_palette[8];
uint8_t                     pc98_gdc_tile_counter=0;
uint8_t                     pc98_gdc_modereg=0;
uint8_t                     pc98_gdc_vramop=0;
union pc98_tile             pc98_gdc_tiles[4];
struct PC98_GDC_state       pc98_gdc[2];
bool                        GDC_vsync_interrupt = false;
uint8_t                     GDC_display_plane = false;
uint8_t                     pc98_16col_analog_rgb_palette_index = 0;

/* 4-bit to 6-bit expansion */
static inline unsigned char dac_4to6(unsigned char c4) {
    /* a b c d . .
     *
     * becomes
     *
     * a b c d a b */
    return (c4 << 2) | (c4 >> 2);
}

void GDC_ProcDelay(Bitu /*val*/) {
    gdc_proc_schedule_done();

    for (unsigned int i=0;i < 2;i++)
        pc98_gdc[i].idle_proc(); // may schedule another delayed proc
}

void pc98_crtc_write(Bitu port,Bitu val,Bitu iolen) {
    switch (port&0xE) {
        case 0x0C:      // 0x7C: mode reg / vram operation mode (also, reset tile counter)
            pc98_gdc_tile_counter = 0;
            pc98_gdc_modereg = val;
            pc98_gdc_vramop &= ~(3 << VOPBIT_GRCG);
            pc98_gdc_vramop |= (val & 0xC0) >> (6 - VOPBIT_GRCG);
            break;
        case 0x0E:      // 0x7E: tile data
            pc98_gdc_tiles[pc98_gdc_tile_counter].b[0] = val;
            pc98_gdc_tiles[pc98_gdc_tile_counter].b[1] = val;
            pc98_gdc_tile_counter = (pc98_gdc_tile_counter + 1) & 3;
            break;
        default:
            LOG_MSG("PC98 CRTC w: port=0x%02X val=0x%02X unknown",(unsigned int)port,(unsigned int)val);
            break;
    };
}

Bitu pc98_crtc_read(Bitu port,Bitu iolen) {
    LOG_MSG("PC98 CRTC r: port=0x%02X unknown",(unsigned int)port);
    return ~0;
}

/* Character Generator (CG) font access state */
uint16_t a1_font_load_addr = 0;
uint8_t a1_font_char_offset = 0;

/* Character Generator ports.
 * This is in fact officially documented by NEC in
 * a 1986 book published about NEC BIOS and BASIC ROM. */
Bitu pc98_a1_read(Bitu port,Bitu iolen) {
    switch (port) {
        case 0xA9: // an 8-bit I/O port to access font RAM by...
            return pc98_font_char_read(a1_font_load_addr,a1_font_char_offset & 0xF,(a1_font_char_offset & 0x20) ? 0 : 1);
        default:
            break;
    };

    return ~0;
}

/* Character Generator ports.
 * This is in fact officially documented by NEC in
 * a 1986 book published about NEC BIOS and BASIC ROM. */
void pc98_a1_write(Bitu port,Bitu val,Bitu iolen) {
    switch (port) {
        /* A3,A1 (out only) two JIS bytes that make up the char code */
        case 0xA1:
            a1_font_load_addr &= 0x00FF;
            a1_font_load_addr |= (val & 0xFF) << 8;
            break;
        case 0xA3:
            a1_font_load_addr &= 0xFF00;
            a1_font_load_addr |= (val & 0xFF);
            break;
        case 0xA5:
            /* From documentation:
             *
             *    bit [7:6] = Dont care
             *    bit [5]   = L/R
             *    bit [4]   = 0
             *    bit [3:0] = C3-C0
             *
             * This so far is consistent with real hardware behavior */
            a1_font_char_offset = val;
            break;
        // TODO: "Edge" is writing 0x00 to port 0xA7 for some reason.
        //       Anything there?
        case 0xA9: // an 8-bit I/O port to access font RAM by...
                   // this is what Touhou Project uses to load fonts.
                   // never mind decompiling INT 18h on real hardware shows instead
                   // a similar sequence with REP MOVSW to A400:0000...
            pc98_font_char_write(a1_font_load_addr,a1_font_char_offset & 0xF,(a1_font_char_offset & 0x20) ? 0 : 1,val);
            break;
        default:
            LOG_MSG("A1 port %lx val %lx unexpected",port,val);
            break;
    };
}

void pc98_gdc_write(Bitu port,Bitu val,Bitu iolen) {
    PC98_GDC_state *gdc;

    if (port >= 0xA0)
        gdc = &pc98_gdc[GDC_SLAVE];
    else
        gdc = &pc98_gdc[GDC_MASTER];

    switch (port&0xE) {
        case 0x00:      /* 0x60/0xA0 param write fifo */
            if (!gdc->write_fifo_param(val))
                LOG_MSG("GDC warning: FIFO param overrun");
            break;
        case 0x02:      /* 0x62/0xA2 command write fifo */
            if (!gdc->write_fifo_command(val))
                LOG_MSG("GDC warning: FIFO command overrun");
            break;
        case 0x04:      /* 0x64: set trigger to signal vsync interrupt (IRQ 2) */
                        /* 0xA4: Bit 0 select display "plane" */
            if (port == 0x64)
                GDC_vsync_interrupt = true;
            else
                GDC_display_plane = (val&1);
            break;
        case 0x06:      /* 0x66: ??
                           0xA6: Bit 0 select CPU access "plane" */
            if (port == 0xA6) {
                pc98_gdc_vramop &= ~(1 << VOPBIT_ACCESS);
                pc98_gdc_vramop |=  (val&1) << VOPBIT_ACCESS;
            }
            else {
                goto unknown;
            }
            break;
        case 0x08:      /* 0xA8: One of two meanings, depending on 8 or 16/256--color mode */
                        /*         8-color: 0xA8-0xAB are 8 4-bit packed fields remapping the 3-bit GRB colors. This defines colors #3 [7:4] and #7 [3:0]
                         *         16-color: GRB color palette index */
                        /* 0x68: ?? */
                        /* NTS: Sadly, "undocumented PC-98" reference does not mention the analog 16-color palette. */
            if (port == 0xA8) /* TODO: If 8-color mode.... else if 16-color mode... */
                pc98_16col_analog_rgb_palette_index = val; /* it takes all 8 bits I assume because of 256-color mode */
            else
                goto unknown;
            break;
        case 0x0A:      /* 0xAA:
                           8-color: Defines color #1 [7:4] and color #5 [3:0] (FIXME: Or is it 2 and 6, by undocumented PC-98???)
                           16-color: 4-bit green intensity. Color index is low 4 bits of palette index.
                           256-color: 4-bit green intensity. Color index is 8-bit palette index. */
            if (port == 0xAA) { /* TODO: If 8-color... else if 16-color... else if 256-color... */
                vga.dac.rgb[pc98_16col_analog_rgb_palette_index & 0xF].green = dac_4to6(val&0xF); /* re-use VGA DAC */
                VGA_DAC_UpdateColor(pc98_16col_analog_rgb_palette_index & 0xF);
            }
            else {
                goto unknown;
            }
            break;
        case 0x0C:      /* 0xAC:
                           8-color: Defines color #2 [7:4] and color #6 [3:0] (FIXME: Or is it 1 and 4, by undocumented PC-98???)
                           16-color: 4-bit red intensity. Color index is low 4 bits of palette index.
                           256-color: 4-bit red intensity. Color index is 8-bit palette index. */
            if (port == 0xAC) { /* TODO: If 8-color... else if 16-color... else if 256-color... */
                vga.dac.rgb[pc98_16col_analog_rgb_palette_index & 0xF].red = dac_4to6(val&0xF); /* re-use VGA DAC */
                VGA_DAC_UpdateColor(pc98_16col_analog_rgb_palette_index & 0xF);
            }
            else {
                goto unknown;
            }
            break;
        case 0x0E:      /* 0xAE:
                           8-color: Defines color #2 [7:4] and color #6 [3:0] (FIXME: Or is it 1 and 4, by undocumented PC-98???)
                           16-color: 4-bit blue intensity. Color index is low 4 bits of palette index.
                           256-color: 4-bit blue intensity. Color index is 8-bit palette index. */
            if (port == 0xAE) { /* TODO: If 8-color... else if 16-color... else if 256-color... */
                vga.dac.rgb[pc98_16col_analog_rgb_palette_index & 0xF].blue = dac_4to6(val&0xF); /* re-use VGA DAC */
                VGA_DAC_UpdateColor(pc98_16col_analog_rgb_palette_index & 0xF);
            }
            else {
                goto unknown;
            }
            break;
        default:
            unknown:
            LOG_MSG("GDC unexpected write to port 0x%x val=0x%x",(unsigned int)port,(unsigned int)val);
            break;
    };
}

Bitu pc98_gdc_read(Bitu port,Bitu iolen) {
    PC98_GDC_state *gdc;

    if (port >= 0xA0)
        gdc = &pc98_gdc[GDC_SLAVE];
    else
        gdc = &pc98_gdc[GDC_MASTER];

    switch (port&0xE) {
        case 0x00:      /* 0x60/0xA0 read status */
            return gdc->read_status();
        case 0x02:      /* 0x62/0xA2 read fifo */
            if (!gdc->rfifo_has_content())
                LOG_MSG("GDC warning: FIFO read underrun");
            return gdc->rfifo_read_data();
        default:
            LOG_MSG("GDC unexpected read from port 0x%x",(unsigned int)port);
            break;
    };

    return ~0;
}

void VGA_OnEnterPC98(Section *sec) {
    VGA_UnsetupMisc();
    VGA_UnsetupAttr();
    VGA_UnsetupDAC();
    VGA_UnsetupGFX();
    VGA_UnsetupSEQ();

    /* Some PC-98 game behavior seems to suggest the BIOS data area stretches all the way from segment 0x40:0x00 to segment 0x7F:0x0F inclusive.
     * Compare that to IBM PC platform, where segment fills only 0x40:0x00 to 0x50:0x00 inclusive and extra state is held in the "Extended BIOS Data Area".
     */

    /* number of text rows on the screen.
     * Touhou Project will not clear/format the text layer properly without this variable. */
    mem_writeb(0x712,25 - 1); /* number of rows - 1 */

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

    pc98_gdc[GDC_MASTER].master_sync = true;
    pc98_gdc[GDC_MASTER].display_enable = true;
    pc98_gdc[GDC_MASTER].row_height = 16;
    pc98_gdc[GDC_MASTER].active_display_words_per_line = 80;

    pc98_gdc[GDC_SLAVE].master_sync = false;
    pc98_gdc[GDC_SLAVE].display_enable = false;//FIXME
    pc98_gdc[GDC_SLAVE].row_height = 1;
    pc98_gdc[GDC_SLAVE].active_display_words_per_line = 40; /* 40 16-bit WORDs per line */

    // as a transition to PC-98 GDC emulation, move VGA alphanumeric buffer
    // down to A0000-AFFFFh.
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
    VGA_StartResize();

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
}

void VGA_OnEnterPC98_phase2(Section *sec) {
    VGA_SetupHandlers();

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
}

void VGA_Init() {
	string str;
	Bitu i,j;

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

