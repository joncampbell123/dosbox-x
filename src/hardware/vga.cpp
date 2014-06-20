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


#include "dosbox.h"
#include "setup.h"
#include "video.h"
#include "pic.h"
#include "vga.h"
#include "programs.h"
#include "support.h"
#include "setup.h"
#include "mem.h"

#include <string.h>
#include <stdlib.h>
#include <string>
#include <stdio.h>

using namespace std;

VGA_Type vga;
SVGA_Driver svga;
int enableCGASnow;
bool vga_3da_polled = false;
bool vga_page_flip_occurred = false;
bool enable_page_flip_debugging_marker = false;
bool enable_vretrace_poll_debugging_marker = false;

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
	case 13:VGA_SetMode(M_LIN32);break;
	}
}

void VGA_StartResize(Bitu delay /*=50*/) {
	if (!vga.draw.resizing) {
		vga.draw.resizing=true;
		if (vga.mode==M_ERROR) delay = 5;
		/* Start a resize after delay (default 50 ms) */
		if (delay==0) VGA_SetupDrawing(0);
		else PIC_AddEvent(VGA_SetupDrawing,(float)delay);
	}
}

#define IS_RESET ((vga.seq.reset&0x3)!=0x3)
#define IS_SCREEN_ON ((vga.seq.clocking_mode&0x20)==0)
static bool hadReset = false;

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

void VGA_Init(Section* sec) {
	Section_prop * section=static_cast<Section_prop *>(sec);
	string str;

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
	enable_page_flip_debugging_marker = section->Get_bool("page flip debug line");
	enable_vretrace_poll_debugging_marker = section->Get_bool("vertical retrace poll debug line");

	if (vga_force_refresh_rate > 0)
		LOG(LOG_VGA,LOG_NORMAL)("VGA forced refresh rate active = %.3f",vga_force_refresh_rate);

	vga.draw.resizing=false;
	vga.mode=M_ERROR;			//For first init
	vga.vmemsize=section->Get_int("vmemsize")*1024*1024;
	SVGA_Setup_Driver();		// svga video memory size is set here
	VGA_SetupMemory(sec);		// memory is allocated here
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
	Bitu i,j;
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

	if (machine == MCH_CGA) PROGRAMS_MakeFile("CGASNOW.COM",CGASNOW_ProgramStart);
	PROGRAMS_MakeFile("VFRCRATE.COM",VFRCRATE_ProgramStart);
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
		vga.vmemsize = vga.vmemwrap = 256*1024;
		break;
	}
}

//save state support
void *VGA_SetupDrawing_PIC_Event = (void*)VGA_SetupDrawing;

