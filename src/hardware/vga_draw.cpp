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


#include <string.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "dosbox.h"
#if defined (WIN32)
#include <d3d9.h>
#endif
#include "logging.h"
#include "time.h"
#include "timer.h"
#include "setup.h"
#include "support.h"
#include "video.h"
#include "render.h"
#include "../gui/render_scalers.h"
#include "vga.h"
#include "pic.h"
#include "jfont.h"
#include "menu.h"
#include "timer.h"
#include "config.h"
#include "control.h"
#include "sdlmain.h"
#include "shiftjis.h"
#include "../ints/int10.h"
#include "pc98_cg.h"
#include "pc98_gdc.h"
#include "pc98_gdc_const.h"

/* do not issue CPU-side I/O here -- this code emulates functions that the GDC itself carries out, not on the CPU */
#include "cpu_io_is_forbidden.h"

bool ega200 = false;
bool mcga_double_scan = false;
bool dbg_event_maxscan = false;
bool dbg_event_scanstep = false;
bool dbg_event_hretrace = false;
bool dbg_event_color_select = false;
bool dbg_event_color_plane_enable = false;

extern bool vga_render_on_demand;

/* S3 streams processor state.
 * Registers are only loaded into hardware on vertical sync anyway. */
struct s3drawstream {
    unsigned int        starty,endy; /* scanlines to draw, starty <= y < endy */
    unsigned int        startx,endx; /* pixels to draw, startx <= x < endx */
    uint32_t            vmem_addr;
    uint32_t            stride;
    uint8_t             pixfmt;
    uint8_t             filter;
    bool                draw;
    bool                evf;

    int32_t             vaccum;
    unsigned int        currentline;

    /* temporary line to process and filter a scanline.
     * maximum scanline of an overlay is 1024. */
    union tmpscanline {
        uint8_t         yuv[1024*3];        // Y U V packed (good for the CPU cache)
    };
    union tmpscanline   tmpscan1;           // rendering version
    union tmpscanline   tmpscan2;           // rendering version 2 for vertical interpolation
    union tmpscanline   *pscan,*cscan;
    bool                cscan_load;
};

struct s3drawstream S3SSdraw = {0};

enum {
	DBGEV_SPLIT=0		// EGA/VGA splitscreen
};

struct debugline_event {
	unsigned int	colorline = 0;
	int		event = -1;
	unsigned int	x = 0;
	uint8_t		w = 0;
	uint8_t		trow = 0;
	size_t		tline = 0;
	bool		done = false;
	std::vector<std::string> text;

	int drawwidth(void) const {
		return w;
	}
	int drawheight(void) const {
		return text.size() * 8;
	}
	void addline(const char *txt) {
		addline(std::string(txt));
	}
	void addline(const std::string &txt) {
		if (w < 8u*txt.length()) w = 8u*txt.length();
		text.push_back(txt);
	}
};

const char* const mode_texts[M_MAX] = {
    "M_CGA2",           // 0
    "M_CGA4",
    "M_EGA",
    "M_VGA",
    "M_LIN4",
    "M_LIN8",           // 5
    "M_LIN15",
    "M_LIN16",
    "M_LIN24",
    "M_LIN32",
    "M_TEXT",           // 10
    "M_HERC_GFX",
    "M_HERC_TEXT",
    "M_CGA16",
    "M_TANDY2",
    "M_TANDY4",         // 15
    "M_TANDY16",
    "M_TANDY_TEXT",
    "M_AMSTRAD",
    "M_PC98",
    "M_FM_TOWNS",       // 20 STUB
    "M_PACKED4",
    "M_DCGA",
    "M_ERROR"
};

#if defined(_MSC_VER)
# pragma warning(disable:4244) /* const fmath::local::uint64_t to double possible loss of data */
# pragma warning(disable:4305) /* truncation from double to float */
#else
# pragma GCC diagnostic ignored "-Wparentheses"
# pragma GCC diagnostic ignored "-Wsign-compare"
# pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

//#undef C_DEBUG
//#define C_DEBUG 1
//#define LOG(X,Y) LOG_MSG

bool et4k_highcolor_half_pixel_rate();

double vga_fps = 70;
double vga_mode_time_base = -1;
int vga_mode_frames_since_time_base = 0;

bool showdbcs = false;
bool pc98_display_enable = true;
bool pc98_monochrome_mode = false;

extern bool pc98_40col_text;
extern bool vga_3da_polled;
extern bool video_debug_overlay;
extern bool vga_page_flip_occurred;
extern bool egavga_per_scanline_hpel;
extern bool vga_enable_hpel_effects;
extern bool vga_enable_hretrace_effects;
extern const char* RunningProgram;
extern unsigned int vga_display_start_hretrace;
extern float hretrace_fx_avg_weight;
extern bool ignore_vblank_wraparound;
extern bool vga_double_buffered_line_compare;
extern bool pc98_crt_mode;      // see port 6Ah command 40h/41h.
extern bool pc98_31khz_mode;
extern bool auto_save_state, enable_autosave, enable_dbcs_tables, showdbcs, dbcs_sbcs, autoboxdraw, halfwidthkana, ticksLocked;
extern int checkcol, autosave_second, autosave_count, autosave_start[10], autosave_end[10], autosave_last[10];
extern uint32_t turbolasttick;
extern std::string failName, autosave_name[10];
extern std::map<int, int> lowboxdrawmap, pc98boxdrawmap;
extern bool isemptyhit(uint16_t code), CodePageGuestToHostUTF16(uint16_t *d/*CROSS_LEN*/,const char *s/*CROSS_LEN*/);
void SetGameState_Run(int value), SaveGameState_Run(void), DOSBOX_UnlockSpeed2( bool pressed ), ttf_switch_off(bool ss=true);
size_t GetGameState_Run(void);
uint8_t lead[6], ccount = 0, *GetDbcsFont(Bitu code), *GetDbcs14Font(Bitu code, bool &is14);
uint32_t ticksPrev = 0;

#if defined(USE_TTF)
extern bool ttf_dosv;
#endif

void memxor(void *_d,unsigned int byte,size_t count) {
    unsigned char *d = (unsigned char*)_d;
    while (count-- > 0) *d++ ^= byte;
}

void memxor_greendotted_16bpp(uint16_t *d,unsigned int count,unsigned int line) {
    static const uint16_t greenptrn[2] = { (0x3F << 5), 0 };
    line &= 1;
    count >>= 1;
    while (count-- > 0) {
        *d++ ^= greenptrn[line];
        *d++ ^= greenptrn[line^1];
    }
}

void memxor_greendotted_32bpp(uint32_t *d,unsigned int count,unsigned int line) {
    static const uint32_t greenptrn[2] = { (0xFF << 8), 0 };
    line &= 1;
    count >>= 2;
    while (count-- > 0) {
        *d++ ^= greenptrn[line];
        *d++ ^= greenptrn[line^1];
    }
}

typedef uint8_t * (* VGA_Line_Handler)(Bitu vidstart, Bitu line);

static VGA_Line_Handler VGA_DrawLine;
static uint8_t TempLine[SCALER_MAXWIDTH * 4 + 256];
static float hretrace_fx_avg = 0;

void VGA_MarkCaptureAcquired(void);
void VGA_MarkCaptureInProgress(bool en);
void pc98_update_display_page_ptr(void);
bool VGA_CaptureValidateCurrentFrame(void);
void VGA_CaptureStartNextFrame(void);
void VGA_MarkCaptureRetrace(void);
void VGA_CaptureMarkError(void);
bool VGA_IsCaptureEnabled(void);
bool VGA_IsCapturePending(void);
void VGA_CaptureWriteScanline(const uint8_t *raw);
void VGA_ProcessScanline(const uint8_t *raw);
bool VGA_IsCaptureInProgress(void);

static uint8_t * VGA_Draw_AMS_4BPP_Line(Bitu vidstart, Bitu line) {
    const uint8_t *base = vga.tandy.draw_base + ((line & vga.tandy.line_mask) << vga.tandy.line_shift);
    const uint8_t *lbase;
    uint32_t *draw = (uint32_t *)TempLine;
    for (Bitu x=vga.draw.blocks;x>0;x--, vidstart++) {
        lbase = &base[ vidstart & (8 * 1024 -1) ];
        Bitu val0 = lbase[ 0 ];
        Bitu val1 = lbase[ 16384 ];
        Bitu val2 = lbase[ 32768 ];
        Bitu val3 = lbase[ 49152 ];
        
        *draw++=( ( CGA_2_Table[ val0 >> 4 ] << 0 ) | 
            ( CGA_2_Table[ val1 >> 4 ] << 1 ) |
            ( CGA_2_Table[ val2 >> 4 ] << 2 ) |
            ( CGA_2_Table[ val3 >> 4 ] << 3 ) ) & vga.amstrad.mask_plane;
        *draw++=( ( CGA_2_Table[ val0 & 0x0F ] << 0 ) | 
            ( CGA_2_Table[ val1 & 0x0F ] << 1 ) |
            ( CGA_2_Table[ val2 & 0x0F ] << 2 ) |
            ( CGA_2_Table[ val3 & 0x0F ] << 3 ) ) & vga.amstrad.mask_plane;
    }
    return TempLine;
}

struct vsync_state vsync;

float uservsyncjolt=0.0f;

VGA_Vsync vsyncmode_current = VS_Off;

void VGA_VsyncUpdateMode(VGA_Vsync vsyncmode) {
    vsyncmode_current = vsyncmode;

    mainMenu.get_item("vsync_off").check(vsyncmode_current == VS_Off).refresh_item(mainMenu);
    mainMenu.get_item("vsync_on").check(vsyncmode_current == VS_On).refresh_item(mainMenu);
    mainMenu.get_item("vsync_force").check(vsyncmode_current == VS_Force).refresh_item(mainMenu);
    mainMenu.get_item("vsync_host").check(vsyncmode_current == VS_Host).refresh_item(mainMenu);

    switch(vsyncmode) {
    case VS_Off:
        vsync.manual    = false;
        vsync.persistent= false;
        vsync.faithful  = false;
        break;
    case VS_On:
        vsync.manual    = true;
        vsync.persistent= true;
        vsync.faithful  = true;
        break;
    case VS_Force:
    case VS_Host:
        vsync.manual    = true;
        vsync.persistent= true;
        vsync.faithful  = false;
        break;
    default:
        LOG_MSG("VGA_VsyncUpdateMode: Invalid mode, using defaults.");
        vsync.manual    = false;
        vsync.persistent= false;
        vsync.faithful  = false;
        break;
    }
}

void VGA_TweakUserVsyncOffset(float val) { uservsyncjolt = val; }

static uint8_t * VGA_Draw_1BPP_Line(Bitu vidstart, Bitu line) {
    const uint8_t *base = vga.tandy.draw_base + ((line & vga.tandy.line_mask) << vga.tandy.line_shift);
    uint32_t *draw = (uint32_t *)TempLine;
    for (Bitu x=vga.draw.blocks;x>0;x--, vidstart++) {
        Bitu val = base[vidstart & (8 * 1024 -1)];
        *draw++=CGA_2_Table[val >> 4];
        *draw++=CGA_2_Table[val & 0xf];
    }
    return TempLine;
}

static uint8_t * VGA_Draw_1BPP_Blend_Line(Bitu vidstart, Bitu line) {
    const uint8_t *base = vga.tandy.draw_base + ((line & vga.tandy.line_mask) << vga.tandy.line_shift);
    uint32_t *draw = (uint32_t *)TempLine;
    Bitu carry = 0;
    for (Bitu x=vga.draw.blocks;x>0;x--, vidstart++) {
        Bitu val1 = base[vidstart & (8 * 1024 -1)];
        Bitu val2 = (val1 >> 1) + carry;
        carry = (val1 & 1) << 7;
        *draw++=CGA_2_Table[val1 >> 4] + CGA_2_Table[val2 >> 4];
        *draw++=CGA_2_Table[val1 & 0xf] + CGA_2_Table[val2 & 0xf];
    }
    return TempLine;
}

static uint8_t * EGA_Draw_2BPP_Line_as_EGA(Bitu vidstart, Bitu line) {
    const uint32_t *base = (uint32_t*)vga.draw.linear_base + ((line & vga.tandy.line_mask) << vga.tandy.line_shift);
    uint8_t * draw=(uint8_t *)TempLine;
    VGA_Latch pixels;

    /* NTS: In reality the 2bpp "shift reg" mode of EGA/VGA bundles odd/even bits for CGA 4-color
     *      across bitplanes 0+1 and 2+3, but the INT 10h CGA 4-color mode disables bitplanes 2 and 3
     *      so that all you see are bitplanes 0+1 set up to emulate CGA video memory. However some
     *      games are said to enable bitplanes 2 and 3 and then use the CGA-like 2bpp mode as a hack
     *      for filling in EGA colors faster, usually with dithering
     *
     *      (ref: "Leather Goddesses of Phobos 2" according to ripsaw8080 when machine=ega). */

    for (Bitu x=0;x<vga.draw.blocks;x++) {
        pixels.d = base[vidstart & vga.tandy.addr_mask];
        vidstart += (Bitu)1u << (Bitu)vga.config.addr_shift;

        /* CGA odd/even mode, first plane and maybe third plane */
        Bitu val=pixels.b[0],val2=pixels.b[2]<<2;
        for (Bitu i=0;i < 4;i++,val <<= 2,val2 <<= 2)
            *draw++ = vga.attr.palette[(((val>>6)&0x3)|((val2>>6)&0xC))&vga.attr.color_plane_enable];

        /* CGA odd/even mode, second plane and maybe fourth plane */
        val=pixels.b[1],val2=pixels.b[3]<<2;
        for (Bitu i=0;i < 4;i++,val <<= 2,val2 <<= 2)
            *draw++ = vga.attr.palette[(((val>>6)&0x3)|((val2>>6)&0xC))&vga.attr.color_plane_enable];
    }
    return TempLine;
}

static uint8_t * VGA_Draw_2BPP_Line_as_VGA(Bitu vidstart, Bitu line) {
    const uint32_t *base = (uint32_t*)vga.draw.linear_base + ((line & vga.tandy.line_mask) << vga.tandy.line_shift);
    uint32_t * draw=(uint32_t *)TempLine;
    VGA_Latch pixels;

    /* NTS: In reality the 2bpp "shift reg" mode of EGA/VGA bundles odd/even bits for CGA 4-color
     *      across bitplanes 0+1 and 2+3, but the INT 10h CGA 4-color mode disables bitplanes 2 and 3
     *      so that all you see are bitplanes 0+1 set up to emulate CGA video memory. However some
     *      games are said to enable bitplanes 2 and 3 and then use the CGA-like 2bpp mode as a hack
     *      for filling in EGA colors faster, usually with dithering
     *
     *      (ref: "Leather Goddesses of Phobos 2" according to ripsaw8080 when machine=ega). */

    for (Bitu x=0;x<vga.draw.blocks;x++) {
        pixels.d = base[vidstart & vga.tandy.addr_mask];
        vidstart += (Bitu)1u << (Bitu)vga.config.addr_shift;

        /* CGA odd/even mode, first plane and maybe third plane */
        Bitu val=pixels.b[0],val2=pixels.b[2]<<2;
        for (Bitu i=0;i < 4;i++,val <<= 2,val2 <<= 2)
            *draw++ = vga.dac.xlat32[(((val>>6)&0x3)|((val2>>6)&0xC))&vga.attr.color_plane_enable];

        /* CGA odd/even mode, second plane and maybe fourth plane */
        val=pixels.b[1],val2=pixels.b[3]<<2;
        for (Bitu i=0;i < 4;i++,val <<= 2,val2 <<= 2)
            *draw++ = vga.dac.xlat32[(((val>>6)&0x3)|((val2>>6)&0xC))&vga.attr.color_plane_enable];
    }
    return TempLine;
}

static uint8_t * EGA_Draw_1BPP_Line_as_EGA(Bitu vidstart, Bitu line) {
    const uint32_t *base = (uint32_t*)vga.draw.linear_base + ((line & vga.tandy.line_mask) << vga.tandy.line_shift);
    uint8_t * draw=(uint8_t *)TempLine;
    VGA_Latch pixels;

    for (Bitu x=0;x<vga.draw.blocks;x++) {
        pixels.d = base[vidstart & vga.tandy.addr_mask];
        vidstart += (Bitu)1u << (Bitu)vga.config.addr_shift;

        Bitu val=pixels.b[0];
        for (Bitu i=0;i < 8;i++,val <<= 1)
            *draw++ = vga.attr.palette[(val>>7)&1];
    }
    return TempLine;
}

static uint8_t * VGA_Draw_1BPP_Line_as_MCGA(Bitu vidstart, Bitu line) {
    const uint8_t *base = vga.tandy.draw_base + ((line & vga.tandy.line_mask) << vga.tandy.line_shift);
    uint32_t * draw=(uint32_t *)TempLine;

    for (Bitu x=0;x<vga.draw.blocks;x++) {
        Bitu val = base[vidstart & vga.tandy.addr_mask];
        vidstart++;

        for (Bitu i=0;i < 8;i++,val <<= 1)
            *draw++ = vga.dac.xlat32[(val>>7)&1];
    }
    return TempLine;
}

bool J3_IsCga4Dcga();

static uint8_t LastLine[2][80];
static uint8_t DcgaColor[2][2][4] = {
	{
		{ 0, 1, 1, 1 },{ 0, 0, 0, 1 }
	}, {
		{ 0, 0, 0, 1 },{ 0, 0, 1, 1 }
	}
};

static uint8_t * VGA_Draw_1BPP_Line_as_VGA(Bitu vidstart, Bitu line) {
    const uint32_t *base = (uint32_t*)vga.draw.linear_base + ((line & vga.tandy.line_mask) << vga.tandy.line_shift);
    uint32_t * draw=(uint32_t *)TempLine;
    VGA_Latch pixels;

	if(J3_IsCga4Dcga()) {
		uint8_t val, b;
		for (Bitu x = 0 ; x < vga.draw.blocks ; x++, vidstart++) {
			val = base[(vidstart & (8 * 1024 -1))];
			if(line == 0) {
				LastLine[0][x] = val;
			} else if(line == 1) {
				LastLine[1][x] = val;
				val = LastLine[0][x];
			} else if(line == 2) {
				val = LastLine[1][x];
			} else {
				val = LastLine[1][x];
			}
			for(int8_t n = 6 ; n >= 0 ; n -= 2) {
				b = (val >> n) & 0x03;
				*draw++ = vga.dac.xlat32[DcgaColor[0][line & 1][b]];
				*draw++ = vga.dac.xlat32[DcgaColor[1][line & 1][b]];
			}
		}
	} else {
	    for (Bitu x=0;x<vga.draw.blocks;x++) {
	        pixels.d = base[vidstart & vga.tandy.addr_mask];
	        vidstart += (Bitu)1u << (Bitu)vga.config.addr_shift;

	        Bitu val=pixels.b[0];
	        for (Bitu i=0;i < 8;i++,val <<= 1)
	            *draw++ = vga.dac.xlat32[(val>>7)&1];
	    }
    }
    return TempLine;
}

static uint8_t * VGA_Draw_2BPP_Line_as_MCGA(Bitu vidstart, Bitu line) {
    const uint8_t *base = vga.tandy.draw_base + ((line & vga.tandy.line_mask) << vga.tandy.line_shift);
    uint32_t * draw=(uint32_t *)TempLine;

    for (Bitu x=0;x<vga.draw.blocks;x++) {
        unsigned char val = base[vidstart & vga.tandy.addr_mask];
        vidstart++;

        for (unsigned int i=0;i < 4;i++,val <<= 2)
            *draw++ = vga.dac.xlat32[(val>>6)&3];
    }

    return TempLine;
}

static uint8_t * VGA_Draw_2BPP_Line(Bitu vidstart, Bitu line) {
    const uint8_t *base = vga.tandy.draw_base + ((line & vga.tandy.line_mask) << vga.tandy.line_shift);
    uint32_t * draw=(uint32_t *)TempLine;
    for (Bitu x=0;x<vga.draw.blocks;x++) {
        Bitu val = base[vidstart & vga.tandy.addr_mask];
        vidstart++;
        *draw++=CGA_4_Table[val];
    }
    return TempLine;
}

static uint8_t * VGA_Draw_2BPPHiRes_Line(Bitu vidstart, Bitu line) {
    const uint8_t *base = vga.tandy.draw_base + ((line & vga.tandy.line_mask) << vga.tandy.line_shift);
    uint32_t * draw=(uint32_t *)TempLine;
    for (Bitu x=0;x<vga.draw.blocks;x++) {
        Bitu val1 = base[vidstart & vga.tandy.addr_mask];
        ++vidstart;
        Bitu val2 = base[vidstart & vga.tandy.addr_mask];
        ++vidstart;
        *draw++=CGA_4_HiRes_Table[(val1>>4)|(val2&0xf0)];
        *draw++=CGA_4_HiRes_Table[(val1&0x0f)|((val2&0x0f)<<4)];
    }
    return TempLine;
}

static Bitu temp[643]={0};

static uint8_t * VGA_Draw_CGA16_Line(Bitu vidstart, Bitu line) {
    const uint8_t *base = vga.tandy.draw_base + ((line & vga.tandy.line_mask) << vga.tandy.line_shift);
#define CGA16_READER(OFF) (base[(vidstart +(OFF))& (8*1024 -1)])
    uint32_t * draw=(uint32_t *)TempLine;
    //There are 640 hdots in each line of the screen.
    //The color of an even hdot always depends on only 4 bits of video RAM.
    //The color of an odd hdot depends on 4 bits of video RAM in
    //1-hdot-per-pixel modes and 6 bits of video RAM in 2-hdot-per-pixel
    //modes. We always assume 6 and use duplicate palette entries in
    //1-hdot-per-pixel modes so that we can use the same routine for all
    //composite modes.
  temp[1] = (CGA16_READER(0) >> 6) & 3;
    for(Bitu x = 2; x < 640; x+=2) {
        temp[x] = (temp[x-1] & 0xf);
        temp[x+1] = (temp[x] << 2) | ((( CGA16_READER(x>>3)) >> (6-(x&6)) )&3);
    }
    temp[640] = temp[639] & 0xf;
    temp[641] = temp[640] << 2;
    temp[642] = temp[641] & 0xf;

    Bitu i = 2;
    for (Bitu x=0;x<vga.draw.blocks;x++) {
        *draw++ = 0xc0708030 | temp[i] | (temp[i+1] << 8) | (temp[i+2] << 16) | (temp[i+3] << 24);
        i += 4;
        *draw++ = 0xc0708030 | temp[i] | (temp[i+1] << 8) | (temp[i+2] << 16) | (temp[i+3] << 24);
        i += 4;
    }
    return TempLine;
#undef CGA16_READER
}

static uint8_t * VGA_Draw_4BPP_Line(Bitu vidstart, Bitu line) {
    const uint8_t *base = vga.tandy.draw_base + ((line & vga.tandy.line_mask) << vga.tandy.line_shift);
    uint8_t* draw=TempLine;
    Bitu end = vga.draw.blocks*2;
    while(end) {
        uint8_t byte = base[vidstart & vga.tandy.addr_mask];
        *draw++=vga.attr.palette[byte >> 4];
        *draw++=vga.attr.palette[byte & 0x0f];
        vidstart++;
        end--;
    }
    return TempLine;
}

static uint8_t * VGA_Draw_4BPP_Line_Double(Bitu vidstart, Bitu line) {
    const uint8_t *base = vga.tandy.draw_base + ((line & vga.tandy.line_mask) << vga.tandy.line_shift);
    uint8_t* draw=TempLine;
    Bitu end = vga.draw.blocks;
    while(end) {
        uint8_t byte = base[vidstart & vga.tandy.addr_mask];
        uint8_t data = vga.attr.palette[byte >> 4];
        *draw++ = data; *draw++ = data;
        data = vga.attr.palette[byte & 0x0f];
        *draw++ = data; *draw++ = data;
        vidstart++;
        end--;
    }
    return TempLine;
}

#if SDL_BYTEORDER == SDL_LIL_ENDIAN && defined(MACOSX) && !defined(C_SDL2) /* Mac OS X Intel builds use a weird RGBA order (alpha in the low 8 bits) */
static inline uint32_t guest_bgr_to_macosx_rgba(const uint32_t x) {
    /* guest: XRGB      X   R   G   B
     * host:  RGBX      B   G   R   X */
    return      ((x & 0x000000FFU) << 24U) +      /* BBxxxxxx */
                ((x & 0x0000FF00U) <<  8U) +      /* xxGGxxxx */
                ((x & 0x00FF0000U) >>  8U);       /* xxxxRRxx */
}
#endif

static uint8_t * VGA_Draw_Linear_Line_24_to_32(Bitu vidstart, Bitu /*line*/) {
    Bitu offset = vidstart & vga.draw.linear_mask;
    Bitu i;

    /* DOSBox's render/scalar code can't handle 24bpp natively, so we have
     * to convert 24bpp -> 32bpp.
     *
     * WARNING: My clever trick might crash on processors that don't support
     *          unaligned memory addressing. To explain what it's doing, is
     *          it's using a DWORD read to fetch the 24bpp pixel (plus an
     *          extra byte), then overwrites the extra byte with 0xFF to
     *          produce a valid RGBA 8:8:8:8 value with the original pixel's
     *          RGB plus alpha channel value of 0xFF. */
#if SDL_BYTEORDER == SDL_LIL_ENDIAN && defined(MACOSX) && !defined(C_SDL2) /* Mac OS X Intel builds use a weird RGBA order (alpha in the low 8 bits) */
    for (i=0;i < vga.draw.width;i++)
        ((uint32_t*)TempLine)[i] = guest_bgr_to_macosx_rgba(*((uint32_t*)(vga.draw.linear_base+offset+(i*3)))) | 0x000000FF;
#else
    for (i=0;i < vga.draw.width;i++)
        ((uint32_t*)TempLine)[i] = *((uint32_t*)(vga.draw.linear_base+offset+(i*3))) | 0xFF000000;
#endif

    return TempLine;
}

static uint8_t * VGA_Draw_Linear_Line_24_to_32_HWMouse(Bitu vidstart, Bitu /*line*/) {
    VGA_Draw_Linear_Line_24_to_32(vidstart,0/*ignored*/); /* always returns TempLine */

    if (!svga.hardware_cursor_active || !svga.hardware_cursor_active())
        return TempLine;

    Bitu lineat = ((vidstart-(vga.config.real_start<<2)) / 3) / vga.draw.width;
    if ((vga.s3.hgc.posx >= vga.draw.width) ||
        (lineat < vga.s3.hgc.originy) || 
        (lineat > (vga.s3.hgc.originy + (63U-vga.s3.hgc.posy))) ) {
        return TempLine;
    } else {
        unsigned int hpos;

        // On 86C928 cards, the "horizontal stretch" modes determine how the cursor is formatted
        // to the DAC. Based on Windows 3.1/95 behavior, it apparently also affects the X coordinate
        // which must match the BYTE offset when run through the DAC. Without this code, the
        // cursor will be placed at 2x (in 16bpp) or 4x (in 32bpp) the actual position it should be.
        if (svgaCard == SVGA_S3Trio && (s3Card >= S3_86C928 && s3Card <= S3_Vision968)) {
            /* NTS: S3 datasheets document bits 2-3 as follows:
             *
             *      bit 2: Hardware cursor horizontal stretch 2 - twice the width (16bpp, apparently)
             *      bit 3: Hardware cursor horizontal stretch 3 - triple the width (24bpp, apparently)
             *
             * Windows 3.1 sets BOTH bits, which apparently means quadruple the width (32bpp)
             * Windows 95 does the same, which suggests that perhaps the hardware behaves as such.
             *
             * I don't know if this is how real hardware behaves but it's what Windows apparently
             * expects to do with the hardware based on how it's setting the X coordinate of the cursor. -- J.C. */
            hpos = vga.s3.hgc.originx / (1 + ((vga.s3.hgc.curmode >> 2u) & 3u));
        }
        else {
            hpos = vga.s3.hgc.originx;
        }

        Bitu sourceStartBit = ((lineat - vga.s3.hgc.originy) + vga.s3.hgc.posy)*64 + vga.s3.hgc.posx; 
        Bitu cursorMemStart = ((sourceStartBit >> 2) & ~1ul) + (((uint32_t)vga.s3.hgc.startaddr) << 10ul);
        Bitu cursorStartBit = sourceStartBit & 0x7u;
        if (cursorMemStart & 0x2) cursorMemStart--;
        Bitu cursorMemEnd = cursorMemStart + (Bitu)((64 - vga.s3.hgc.posx) >> 2);
        uint32_t* xat = &((uint32_t*)TempLine)[hpos]; // NTS: 24bpp is converted to 32bpp first, so by this point the line is 32bpp
        for (Bitu m = cursorMemStart; m < cursorMemEnd; (m&1)?(m+=3):m++) {
            // for each byte of cursor data
            uint8_t bitsA = vga.mem.linear[m];
            uint8_t bitsB = vga.mem.linear[m+2];
            for (uint8_t bit=(0x80 >> cursorStartBit); bit != 0; bit >>= 1) { // for each bit
                cursorStartBit=0;
                if (vga.s3.reg_55 & 0x10) {
                    // X11 mode: draw when mask bit is set, otherwise transparent
                    if (bitsA & bit) {
                        if (bitsB & bit) {
                            *xat = *(uint16_t*)vga.s3.hgc.forestack;
                        } else {
                            *xat = *(uint16_t*)vga.s3.hgc.backstack;
                        }
                    }
                } else {
                    // MS Windows mode
                    if (bitsA&bit) {
                        if (bitsB&bit) *xat ^= ~0U;
                        //else Transparent
                    } else if (bitsB&bit) {
                        *xat = *(uint32_t*)vga.s3.hgc.forestack;
                    } else {
                        *xat = *(uint32_t*)vga.s3.hgc.backstack;
                    }
                }
                xat++;
            }
        }
        return TempLine;
    }
}

static uint8_t * VGA_Draw_Linear_Line(Bitu vidstart, Bitu /*line*/) {
    Bitu offset = vidstart & vga.draw.linear_mask;
    uint8_t* ret = &vga.draw.linear_base[offset];
    
    // in case (vga.draw.line_length + offset) has bits set that
    // are not set in the mask: ((x|y)!=y) equals (x&~y)
    if (GCC_UNLIKELY(((vga.draw.line_length + offset) & (~vga.draw.linear_mask)) != 0u)) {
        // this happens, if at all, only once per frame (1 of 480 lines)
        // in some obscure games
        Bitu end = (offset + vga.draw.line_length) & vga.draw.linear_mask;
        
        // assuming lines not longer than 4096 pixels
        Bitu wrapped_len = end & 0xFFF;
        Bitu unwrapped_len = vga.draw.line_length-wrapped_len;
        
        // unwrapped chunk: to top of memory block
        memcpy(TempLine, &vga.draw.linear_base[offset], unwrapped_len);
        // wrapped chunk: from base of memory block
        memcpy(&TempLine[unwrapped_len], vga.draw.linear_base, wrapped_len);

        ret = TempLine;
    }

#if !defined(C_UNALIGNED_MEMORY)
    if (GCC_UNLIKELY( ((Bitu)ret) & (sizeof(Bitu)-1)) ) {
        memcpy( TempLine, ret, vga.draw.line_length );
        return TempLine;
    }
#endif
    return ret;
}

/* WARNING: This routine assumes (vidstart&3) == 0 */
static uint8_t * VGA_Draw_Xlat32_VGA_CRTC_bmode_Line(Bitu vidstart, Bitu /*line*/) {
    uint32_t* temps = (uint32_t*) TempLine;
    unsigned int poff = 0;
    Bitu skip; /* how much to skip after drawing 4 pixels */

    skip = 4u << vga.config.addr_shift;

    /* *sigh* it looks like DOSBox's VGA scanline code will pass nonzero bits 0-1 in vidstart */
    poff += vidstart & 3u;
    vidstart &= ~3ul;

    /* hack for Surprise! productions "copper" demo.
     * when the demo talks about making the picture waver, what it's doing is diddling
     * with the Start Horizontal Retrace register of the CRTC once per scanline.
     * ...yeah, really. It's a wonder in retrospect the programmer didn't burn out his
     * VGA monitor, and I bet this makes the demo effect unwatchable on LCD flat panels or
     * scan converters that rely on the pulses to detect VGA mode changes! */
    if (vga_enable_hretrace_effects) {
        /* NTS: This is NOT BACKWARDS. It makes sense if you think about it: the monitor
         *      begins swinging the electron beam back on horizontal retract, so if the
         *      retrace starts sooner, then the blanking on the left side appears to last
         *      longer because there are more clocks until active display.
         *
         *      Also don't forget horizontal total/blank/retrace etc. registers are in
         *      character clocks not pixels. In 320x200x256 mode, one character clock is
         *      4 pixels.
         *
         *      Finally, we average it with "weight" because CRTs have "inertia" */
        float a = 1.0 / (hretrace_fx_avg_weight + 1);

        hretrace_fx_avg *= 1.0 - a;
        hretrace_fx_avg += a * 4 * ((int)vga_display_start_hretrace - (int)vga.crtc.start_horizontal_retrace);
        int x = (int)floor(hretrace_fx_avg + 0.5);

        vidstart += (Bitu)((int)skip * (x >> 2));
        poff += x & 3;
    }

    for(Bitu i = 0; i < ((vga.draw.line_length>>(2/*32bpp*/+2/*4 pixels*/))+((poff+3)>>2)); i++) {
        uint8_t *ret = &vga.draw.linear_base[ vidstart & vga.draw.linear_mask ];

        /* one group of 4 */
        *temps++ = vga.dac.xlat32[*ret++];
        *temps++ = vga.dac.xlat32[*ret++];
        *temps++ = vga.dac.xlat32[*ret++];
        *temps++ = vga.dac.xlat32[*ret++];
        /* and skip */
        vidstart += skip;
    }

    return TempLine + (poff * 4);
}

static uint8_t * VGA_Draw_Xlat32_Linear_Line(Bitu vidstart, Bitu /*line*/) {
    uint32_t* temps = (uint32_t*) TempLine;

    /* hack for Surprise! productions "copper" demo.
     * when the demo talks about making the picture waver, what it's doing is diddling
     * with the Start Horizontal Retrace register of the CRTC once per scanline.
     * ...yeah, really. It's a wonder in retrospect the programmer didn't burn out his
     * VGA monitor, and I bet this makes the demo effect unwatchable on LCD flat panels or
     * scan converters that rely on the pulses to detect VGA mode changes! */
    if (vga_enable_hretrace_effects) {
        /* NTS: This is NOT BACKWARDS. It makes sense if you think about it: the monitor
         *      begins swinging the electron beam back on horizontal retract, so if the
         *      retrace starts sooner, then the blanking on the left side appears to last
         *      longer because there are more clocks until active display.
         *
         *      Also don't forget horizontal total/blank/retrace etc. registers are in
         *      character clocks not pixels. In 320x200x256 mode, one character clock is
         *      4 pixels.
         *
         *      Finally, we average it with "weight" because CRTs have "inertia" */
        float a = 1.0 / (hretrace_fx_avg_weight + 1);

        hretrace_fx_avg *= 1.0 - a;
        hretrace_fx_avg += a * 4 * ((int)vga_display_start_hretrace - (int)vga.crtc.start_horizontal_retrace);
        int x = (int)floor(hretrace_fx_avg + 0.5);

        vidstart += (Bitu)x;
    }

    for(Bitu i = 0; i < (vga.draw.line_length>>2); i++)
        temps[i]=vga.dac.xlat32[vga.draw.linear_base[(vidstart+i)&vga.draw.linear_mask]];

    return TempLine;
}

template <const unsigned int card,typename templine_type_t> static inline templine_type_t EGA_Planar_Common_Block_xlat(const uint8_t t) {
    if (card == MCH_VGA)
        return vga.dac.xlat32[t];
    else if (card == MCH_EGA)
        return vga.attr.palette[t&vga.attr.color_plane_enable];

    return 0;
}

template <const unsigned int card,typename templine_type_t> static inline void EGA_Planar_Common_Block(templine_type_t * const temps,const uint32_t t1,const uint32_t t2) {
    uint32_t tmp;

    tmp =   Expand16Table[0][(t1>>0)&0xFF] |
            Expand16Table[1][(t1>>8)&0xFF] |
            Expand16Table[2][(t1>>16)&0xFF] |
            Expand16Table[3][(t1>>24)&0xFF];
    temps[0] = EGA_Planar_Common_Block_xlat<card,templine_type_t>((tmp>> 0ul)&0xFFul);
    temps[1] = EGA_Planar_Common_Block_xlat<card,templine_type_t>((tmp>> 8ul)&0xFFul);
    temps[2] = EGA_Planar_Common_Block_xlat<card,templine_type_t>((tmp>>16ul)&0xFFul);
    temps[3] = EGA_Planar_Common_Block_xlat<card,templine_type_t>((tmp>>24ul)&0xFFul);

    tmp =   Expand16Table[0][(t2>>0)&0xFF] |
            Expand16Table[1][(t2>>8)&0xFF] |
            Expand16Table[2][(t2>>16)&0xFF] |
            Expand16Table[3][(t2>>24)&0xFF];
    temps[4] = EGA_Planar_Common_Block_xlat<card,templine_type_t>((tmp>> 0ul)&0xFFul);
    temps[5] = EGA_Planar_Common_Block_xlat<card,templine_type_t>((tmp>> 8ul)&0xFFul);
    temps[6] = EGA_Planar_Common_Block_xlat<card,templine_type_t>((tmp>>16ul)&0xFFul);
    temps[7] = EGA_Planar_Common_Block_xlat<card,templine_type_t>((tmp>>24ul)&0xFFul);
}

template <const unsigned int card,typename templine_type_t> static uint8_t * EGA_Planar_Common_Line(Bitu vidstart, Bitu /*line*/) {
    templine_type_t* temps = (templine_type_t*)TempLine;
    Bitu count = vga.draw.blocks + ((vga.draw.panning + 7u) >> 3u);
    Bitu i = 0;

    while (count > 0u) {
        uint32_t t1,t2;
        t1 = t2 = *((uint32_t*)(&vga.draw.linear_base[ vidstart & vga.draw.linear_mask ]));
        t1 = (t1 >> 4) & 0x0f0f0f0f;
        t2 &= 0x0f0f0f0f;
        vidstart += (uintptr_t)4 << (uintptr_t)vga.config.addr_shift;
        EGA_Planar_Common_Block<card,templine_type_t>(temps+i,t1,t2);
        count--;
        i += 8;
    }

    return TempLine + (vga.draw.panning*sizeof(templine_type_t));
}

static uint8_t * EGA_Draw_VGA_Planar_Xlat8_Line(Bitu vidstart, Bitu line) {
    return EGA_Planar_Common_Line<MCH_EGA,uint8_t>(vidstart,line);
}

static uint8_t * VGA_Draw_VGA_Planar_Xlat32_Line(Bitu vidstart, Bitu line) {
    return EGA_Planar_Common_Line<MCH_VGA,uint32_t>(vidstart,line);
}

static uint8_t * VGA_Draw_VGA_Packed4_Xlat32_Line(Bitu vidstart, Bitu /*line*/) {
    uint32_t* temps = (uint32_t*) TempLine;

    for (Bitu i = 0; i < ((vga.draw.line_length>>2)+vga.draw.panning); i += 2) {
        uint8_t t = vga.draw.linear_base[ vidstart & vga.draw.linear_mask ];
        vidstart++;

        temps[i+0] = vga.dac.xlat32[(t>>4)&0xF];
        temps[i+1] = vga.dac.xlat32[(t>>0)&0xF];
    }

    return TempLine + (vga.draw.panning*4);
}

//Test version, might as well keep it
/* static uint8_t * VGA_Draw_Chain_Line(Bitu vidstart, Bitu line) {
    Bitu i = 0;
    for ( i = 0; i < vga.draw.width;i++ ) {
        Bitu addr = vidstart + i;
        TempLine[i] = vga.mem.linear[((addr&~3)<<2)+(addr&3)];
    }
    return TempLine;
} */

static inline uint8_t S3StreamVRAMRead8(const uint32_t a) {
    return vga.mem.linear[a & vga.mem.memmask];
}

static inline uint8_t clampu8(int x) {
    if (x > 255)
        return 255;
    else if (x < 0)
        return 0;
    else
        return x;
}

#define MPEGFP8(x) ((int)((x) * 0x100))

uint32_t YUVMPEG2RGB32(const uint8_t Y,const uint8_t U,const uint8_t V) {
    /*
        B = 1.164(Y - 16)                  + 2.018(U - 128)
        G = 1.164(Y - 16) - 0.813(V - 128) - 0.391(U - 128)
        R = 1.164(Y - 16) + 1.596(V - 128)
     */

    /* WARNING: >> 8 must be signed arithmetic shift */
    uint8_t R = clampu8(((MPEGFP8(1.164) * ((int)Y - 16)) + (MPEGFP8(1.596) * (V - 128)))>>8);
    uint8_t G = clampu8(((MPEGFP8(1.164) * ((int)Y - 16)) - (MPEGFP8(0.813) * (V - 128)) - (MPEGFP8(0.391) * (U - 128)))>>8);
    uint8_t B = clampu8(((MPEGFP8(1.164) * ((int)Y - 16)) + (MPEGFP8(2.018) * (U - 128)))>>8);

    return (R << 16) | (G << 8) | B;
}

static inline bool S3_XGA_OverlayKeyMatch(const uint32_t match,const uint32_t key) {
    return (match == key);
}

extern bool vga_8bit_dac;

static inline uint8_t S3EVF8int(const uint8_t p,const uint8_t c,int a) {
    return (uint8_t)(((p * (256-a)) + (c * a) + 128) >> 8);
}

void S3_XGA_RenderYUY2MPEGcolorkeyEVF(uint32_t* temp2/*already adjusted to X coordinate in row*/,unsigned char *psrcyuv3,unsigned char *srcyuv3,int count,int a) {
    uint32_t mask = (0xFFu << (7u - vga.s3.streams.ckctl_rgb_cc)) * 0x010101u;

    // HACK: DOSBox/DOSBox-X VGA emulation, unless otherwise, maps the 6-bit RGB VGA palette to 8-bit by shifting over by 2.
    //       Unfortunately, S3's DCI driver and XingMPEG uses 0xFF00FF bright magenta to color key.
    //       Mask off the low 2 bits if not 8-bit VGA or the color key will never work.
    if (!vga_8bit_dac) mask &= 0xFCFCFC;

    const uint32_t key = (((uint32_t)vga.s3.streams.ckctl_b_lb) | ((uint32_t)vga.s3.streams.ckctl_g_lb << 8u) | ((uint32_t)vga.s3.streams.ckctl_r_lb << 16)) & mask;
    int o = 0;

    while (count-- > 0) {
        if (S3_XGA_OverlayKeyMatch(temp2[o] & mask,key)) {
            temp2[o] = YUVMPEG2RGB32(
                S3EVF8int(psrcyuv3[0],srcyuv3[0],a),
                S3EVF8int(psrcyuv3[1],srcyuv3[1],a),
                S3EVF8int(psrcyuv3[2],srcyuv3[2],a));
        }

        psrcyuv3 += 3;
        srcyuv3 += 3;
        o++;
    }
}

void S3_XGA_RenderYUY2MPEGcolorkey(uint32_t* temp2/*already adjusted to X coordinate in row*/,unsigned char *srcyuv3,int count) {
    uint32_t mask = (0xFFu << (7u - vga.s3.streams.ckctl_rgb_cc)) * 0x010101u;

    // HACK: DOSBox/DOSBox-X VGA emulation, unless otherwise, maps the 6-bit RGB VGA palette to 8-bit by shifting over by 2.
    //       Unfortunately, S3's DCI driver and XingMPEG uses 0xFF00FF bright magenta to color key.
    //       Mask off the low 2 bits if not 8-bit VGA or the color key will never work.
    if (!vga_8bit_dac) mask &= 0xFCFCFC;

    const uint32_t key = (((uint32_t)vga.s3.streams.ckctl_b_lb) | ((uint32_t)vga.s3.streams.ckctl_g_lb << 8u) | ((uint32_t)vga.s3.streams.ckctl_r_lb << 16)) & mask;
    int o = 0;

    while (count-- > 0) {
        if (S3_XGA_OverlayKeyMatch(temp2[o] & mask,key))
            temp2[o] = YUVMPEG2RGB32(srcyuv3[0],srcyuv3[1],srcyuv3[2]);

        srcyuv3 += 3;
        o++;
    }
}

void S3_XGA_RenderYUY2MPEGEVF(uint32_t* temp2/*already adjusted to X coordinate in row*/,unsigned char *psrcyuv3,unsigned char *srcyuv3,int count,int a) {
    int o = 0;

    while (count-- > 0) {
        temp2[o] = YUVMPEG2RGB32(
            S3EVF8int(psrcyuv3[0],srcyuv3[0],a),
            S3EVF8int(psrcyuv3[1],srcyuv3[1],a),
            S3EVF8int(psrcyuv3[2],srcyuv3[2],a));
        psrcyuv3 += 3;
        srcyuv3 += 3;
        o++;
    }
}

void S3_XGA_RenderYUY2MPEG(uint32_t* temp2/*already adjusted to X coordinate in row*/,unsigned char *srcyuv3,int count) {
    int o = 0;

    while (count-- > 0) {
        temp2[o] = YUVMPEG2RGB32(srcyuv3[0],srcyuv3[1],srcyuv3[2]);
        srcyuv3 += 3;
        o++;
    }
}

void S3_XGA_YUY2HProc(unsigned char *dst3yuv,uint32_t vram,int count) {
    /* nearest neighbor */
    if (vga.s3.streams.ssctl_sfc == 0) {
        int32_t haccum = vga.s3.streams.ssctl_dda_haccum;
        int o = 0;

        /* To explain the reads below, data in VRAM is 8-bit Y U Y V.
         * This is a way to nearest neighbor read while stepping across
         * Y U Y V properly. */
        while (count-- > 0) {
            dst3yuv[o+0] = S3StreamVRAMRead8(vram+0);             // Y
            dst3yuv[o+1] = S3StreamVRAMRead8((vram&(~3u))+1);     // U
            dst3yuv[o+2] = S3StreamVRAMRead8((vram&(~3u))+3);     // V
            o += 3;

            haccum += vga.s3.streams.ssctl_k1_hscale;
            if (haccum >= 0) {
                haccum -= vga.s3.streams.ssctl_k1_hscale;
                haccum += vga.s3.streams.ssctl_k2_hscale;
                vram += 2;
            }
        }
    }
    else {
        /* linear interpolation */

        /* Y */
        {
            int32_t haccum = vga.s3.streams.ssctl_dda_haccum;
            uint32_t lv = vram;
            int lc = count;
            int o = 0;
            int a = 0;
            int adiv;

            adiv = vga.s3.streams.ssctl_k1_hscale - vga.s3.streams.ssctl_k2_hscale;
            if (adiv == 0) adiv = 1;

            while (lc-- > 0) {
                if (adiv != 0) {
                    a = 256 + ((haccum * 0x100) / adiv);
                    if (a < 0) a = 0;
                    if (a > 255) a = 255;
                }

                dst3yuv[o+0] = ((S3StreamVRAMRead8(lv+0) * (256-a)) + (S3StreamVRAMRead8(lv+2) * a) + 128) >> 8;                // Y
                o += 3;

                haccum += vga.s3.streams.ssctl_k1_hscale;
                if (haccum >= 0) {
                    haccum -= vga.s3.streams.ssctl_k1_hscale;
                    haccum += vga.s3.streams.ssctl_k2_hscale;
                    lv += 2;
                }
            }
        }

        /* U and V */
        {
            int32_t haccum = vga.s3.streams.ssctl_dda_haccum * 2; /* U and V have half resolution, modify DDA appropriately */
            int o = 0;
            int a = 0;
            int adiv;

            adiv = (vga.s3.streams.ssctl_k1_hscale - vga.s3.streams.ssctl_k2_hscale) * 2;
            if (adiv == 0) adiv = 1;

            while (count-- > 0) {
                if (adiv != 0) {
                    a = 256 + ((haccum * 0x100) / adiv);
                    if (a < 0) a = 0;
                    if (a > 255) a = 255;
                }

                dst3yuv[o+1] = ((S3StreamVRAMRead8(vram+1) * (256-a)) + (S3StreamVRAMRead8(vram+5) * a)) >> 8;                  // U
                dst3yuv[o+2] = ((S3StreamVRAMRead8(vram+3) * (256-a)) + (S3StreamVRAMRead8(vram+7) * a)) >> 8;                  // V
                o += 3;

                haccum += vga.s3.streams.ssctl_k1_hscale;
                if (haccum >= 0) {
                    haccum -= vga.s3.streams.ssctl_k1_hscale * 2;
                    haccum += vga.s3.streams.ssctl_k2_hscale * 2;
                    vram += 4;
                }
            }
        }
    }
}

void S3_XGA_SecondaryStreamRender(uint32_t* temp2) {
    if (S3SSdraw.draw) {
        if (S3SSdraw.currentline >= S3SSdraw.starty && S3SSdraw.currentline < S3SSdraw.endy) {
            // FIXME: This assumes YUY2 16-240 range (MPEG-style), check format code.
            if (S3SSdraw.cscan_load)
                S3_XGA_YUY2HProc(S3SSdraw.cscan->yuv,S3SSdraw.vmem_addr,S3SSdraw.endx - S3SSdraw.startx);

            if (vga.s3.streams.evf) { /* vertical interpolation (S3 ViRGE and higher only) */
                int a = 0;
                int adiv;

                adiv = vga.s3.streams.k1_vscale_factor - vga.s3.streams.k2_vscale_factor;
                if (adiv == 0) adiv = 1;
                a = 256 + ((S3SSdraw.vaccum * 0x100) / adiv);
                if (a < 0) a = 0;
                if (a > 255) a = 255;

                if (vga.s3.streams.blendctl_composemode == 5/*color key on primary stream, secondary overlay on primary*/)
                    S3_XGA_RenderYUY2MPEGcolorkeyEVF(temp2+S3SSdraw.startx,S3SSdraw.pscan->yuv,S3SSdraw.cscan->yuv,S3SSdraw.endx - S3SSdraw.startx,a);
                else if (vga.s3.streams.blendctl_composemode == 0/*opaque secondary on primary*/)
                    S3_XGA_RenderYUY2MPEGEVF(temp2+S3SSdraw.startx,S3SSdraw.pscan->yuv,S3SSdraw.cscan->yuv,S3SSdraw.endx - S3SSdraw.startx,a);
            }
            else {
                if (vga.s3.streams.blendctl_composemode == 5/*color key on primary stream, secondary overlay on primary*/)
                    S3_XGA_RenderYUY2MPEGcolorkey(temp2+S3SSdraw.startx,S3SSdraw.cscan->yuv,S3SSdraw.endx - S3SSdraw.startx);
                else if (vga.s3.streams.blendctl_composemode == 0/*opaque secondary on primary*/)
                    S3_XGA_RenderYUY2MPEG(temp2+S3SSdraw.startx,S3SSdraw.cscan->yuv,S3SSdraw.endx - S3SSdraw.startx);
            }

            /* it's not clear from the datasheet, but I think what the card is doing is a
             * DDA to vertically scale the image, and K1/K2 are just terms to add/subtract
             * to vertically scale.
             *
             * The code below seems to work well enough with Windows 3.1 and Windows 98.
             * Note of course this algorithm doesn't allow scaling DOWN YUV playback, a
             * hardware limitation acknowledged by both Windows 3.1 and Windows 98
             * S3 Trio64V+ drivers. ActiveMovie for Windows 98 will disable the YUV overlay
             * if you scale down below 100% in any dimension.
             *
             * Note that K1 = original height - 1
             *           K2 = (original height) - (final height)
             *
             * NTS: Windows 3.1 DCI and Windows 98 DirectX drivers will set initial accumulator
             *      and both K1/K2 scale factors to zero if the vertical scale is 1:1. */
            S3SSdraw.vaccum += vga.s3.streams.k1_vscale_factor;
            if (S3SSdraw.vaccum >= 0) {
                S3SSdraw.vaccum -= vga.s3.streams.k1_vscale_factor;
                S3SSdraw.vaccum += vga.s3.streams.k2_vscale_factor; /* usually negative value */
                S3SSdraw.vmem_addr += S3SSdraw.stride;

                std::swap(S3SSdraw.cscan,S3SSdraw.pscan);
                S3SSdraw.cscan_load = true;
            }
        }

        S3SSdraw.currentline++;
    }
}

static uint8_t * VGA_Draw_VGA_Line_Xlat32_HWMouse( Bitu vidstart, Bitu /*line*/) {
    uint32_t* temp2 = (uint32_t*)VGA_Draw_Xlat32_Linear_Line(vidstart, 0);

    /* streams processor overlay: secondary stream (commonly, YUV overlay for MPEG playback) */
    /* NTS: Ignore the "primary stream" first, because I'm not sure whether that's another overlay
     * or just the main display. */
    S3_XGA_SecondaryStreamRender(temp2);

    /* hardware cursor */
    if (svga.hardware_cursor_active != NULL && svga.hardware_cursor_active()) {
        Bitu lineat = (vidstart-(vga.config.real_start<<2)) / vga.draw.width;
        if ((vga.s3.hgc.posx >= vga.draw.width) ||
            (lineat < vga.s3.hgc.originy) ||
            (lineat > (vga.s3.hgc.originy + (63U-vga.s3.hgc.posy))) ) {
            // the mouse cursor *pattern* is not on this line, do nothing
        } else {
            unsigned int hpos;
            uint8_t bg,fg;

            // S3 86C928: In 256-color modes, CRTC registers 0Eh-0Fh, normally used to hold
            // cursor position in VGA modes, becomes the foreground/background colors of
            // the hardware cursor. This is needed for the Windows 3.1 driver to set the
            // colors of the cursor correctly. The 86C928 appears to be the only card not
            // to set 256-color cursor colors using the foreground/background stack registers
            // of later cards. Truecolor/highcolor cards still use foreground/background
            // stack on 86C928 cards.
            //
            // Windows 95 behavior suggests that S3 Vision864/Vision868 cards behave the same way in
            // 256-color mode. Perhaps it was there all along with Windows 3.1, but it happens
            // to write all the right values in a way that masks the issue.
            //
            // FIXME: On 86C928 cards, bits 2 & 3 of the Hardware Graphics Cursor Mode Register (CR45)
            //        enable horizontal stretch which also determines how the foreground/background
            //        stack is used to render the cursor.
            if (svgaCard == SVGA_S3Trio && (s3Card >= S3_86C928 && s3Card <= S3_Vision968)) {
                fg = (vga.config.cursor_start >> 8u) & 0xFFu; // register 0Eh
                bg = (vga.config.cursor_start & 0xFFu); // register 0Fh
            }
            // all other cards use the fore/back stack as expected
            else {
                fg = vga.s3.hgc.forestack[0];
                bg = vga.s3.hgc.backstack[0];
            }

            // On 86C928 cards, the "horizontal stretch" modes determine how the cursor is formatted
            // to the DAC. Based on Windows 3.1/95 behavior, it apparently also affects the X coordinate
            // which must match the BYTE offset when run through the DAC. Without this code, the
            // cursor will be placed at 2x (in 16bpp) or 4x (in 32bpp) the actual position it should be.
            if (svgaCard == SVGA_S3Trio && (s3Card >= S3_86C928 && s3Card <= S3_Vision968)) {
                /* NTS: S3 datasheets document bits 2-3 as follows:
                 *
                 *      bit 2: Hardware cursor horizontal stretch 2 - twice the width (16bpp, apparently)
                 *      bit 3: Hardware cursor horizontal stretch 3 - triple the width (24bpp, apparently)
                 *
                 * Windows 3.1 sets BOTH bits, which apparently means quadruple the width (32bpp)
                 * Windows 95 does the same, which suggests that perhaps the hardware behaves as such.
                 *
                 * I don't know if this is how real hardware behaves but it's what Windows apparently
                 * expects to do with the hardware based on how it's setting the X coordinate of the cursor. -- J.C. */
                hpos = vga.s3.hgc.originx / (1 + ((vga.s3.hgc.curmode >> 2u) & 3u));
            }
            else {
                hpos = vga.s3.hgc.originx;
            }

            // Draw mouse cursor: cursor is a 64x64 pattern which is shifted (inside the
            // 64x64 mouse cursor space) to the right by posx pixels and up by posy pixels.
            // This is used when the mouse cursor partially leaves the screen.
            // It is arranged as bitmap of 16bits of bitA followed by 16bits of bitB, each
            // AB bits corresponding to a cursor pixel. The whole map is 8kB in size.
            //memcpy(TempLine, &vga.mem.linear[ vidstart ], vga.draw.width);

            // the index of the bit inside the cursor bitmap we start at:
            Bitu sourceStartBit = ((lineat - vga.s3.hgc.originy) + vga.s3.hgc.posy)*64 + vga.s3.hgc.posx;
            // convert to video memory addr and bit index
            // start adjusted to the pattern structure (thus shift address by 2 instead of 3)
            // Need to get rid of the third bit, so "/8 *2" becomes ">> 2 & ~1"
            Bitu cursorMemStart = ((sourceStartBit >> 2ul) & ~1ul) + (((uint32_t)vga.s3.hgc.startaddr) << 10ul);
            Bitu cursorStartBit = sourceStartBit & 0x7u;
            // stay at the right position in the pattern
            if (cursorMemStart & 0x2) cursorMemStart--;
            Bitu cursorMemEnd = cursorMemStart + (Bitu)((64 - vga.s3.hgc.posx) >> 2);
            uint32_t* xat = &temp2[hpos]; // mouse data start pos. in scanline
            for (Bitu m = cursorMemStart; m < cursorMemEnd; (m&1)?(m+=3):m++) {
                // for each byte of cursor data
                uint8_t bitsA = vga.mem.linear[m];
                uint8_t bitsB = vga.mem.linear[m+2];
                for (uint8_t bit=(0x80 >> cursorStartBit); bit != 0; bit >>= 1) {
                    // for each bit
                    cursorStartBit=0; // only the first byte has some bits cut off
                    if (vga.s3.reg_55 & 0x10) {
                        // X11 mode: draw when mask bit is set, otherwise transparent
                        if (bitsA & bit) {
                            if (bitsB & bit) {
                                *xat = *(uint16_t*)vga.s3.hgc.forestack;
                            } else {
                                *xat = *(uint16_t*)vga.s3.hgc.backstack;
                            }
                        }
                    } else {
                        // MS Windows mode
                        if (bitsA&bit) {
                            if (bitsB&bit) *xat ^= 0xFFFFFFFF; // Invert screen data
                            //else Transparent
                        } else if (bitsB&bit) {
                            *xat = vga.dac.xlat32[fg]; // foreground color
                        } else {
                            *xat = vga.dac.xlat32[bg];
                        }
                    }
                    xat++;
                }
            }
        }
    }

    return (uint8_t*)temp2;
}

static uint8_t * VGA_Draw_VGA_Line_HWMouse( Bitu vidstart, Bitu /*line*/) {
    if (!svga.hardware_cursor_active || !svga.hardware_cursor_active())
        // HW Mouse not enabled, use the tried and true call
        return &vga.mem.linear[vidstart];

    Bitu lineat = (vidstart-(vga.config.real_start<<2)) / vga.draw.width;
    if ((vga.s3.hgc.posx >= vga.draw.width) ||
        (lineat < vga.s3.hgc.originy) || 
        (lineat > (vga.s3.hgc.originy + (63U-vga.s3.hgc.posy))) ) {
        // the mouse cursor *pattern* is not on this line
        return &vga.mem.linear[ vidstart ];
    } else {
        unsigned int hpos;

        // On 86C928 cards, the "horizontal stretch" modes determine how the cursor is formatted
        // to the DAC. Based on Windows 3.1/95 behavior, it apparently also affects the X coordinate
        // which must match the BYTE offset when run through the DAC. Without this code, the
        // cursor will be placed at 2x (in 16bpp) or 4x (in 32bpp) the actual position it should be.
        if (svgaCard == SVGA_S3Trio && (s3Card >= S3_86C928 && s3Card <= S3_Vision968)) {
            /* NTS: S3 datasheets document bits 2-3 as follows:
             *
             *      bit 2: Hardware cursor horizontal stretch 2 - twice the width (16bpp, apparently)
             *      bit 3: Hardware cursor horizontal stretch 3 - triple the width (24bpp, apparently)
             *
             * Windows 3.1 sets BOTH bits, which apparently means quadruple the width (32bpp)
             * Windows 95 does the same, which suggests that perhaps the hardware behaves as such.
             *
             * I don't know if this is how real hardware behaves but it's what Windows apparently
             * expects to do with the hardware based on how it's setting the X coordinate of the cursor. -- J.C. */
            hpos = vga.s3.hgc.originx / (1 + ((vga.s3.hgc.curmode >> 2u) & 3u));
        }
        else {
            hpos = vga.s3.hgc.originx;
        }

        // Draw mouse cursor: cursor is a 64x64 pattern which is shifted (inside the
        // 64x64 mouse cursor space) to the right by posx pixels and up by posy pixels.
        // This is used when the mouse cursor partially leaves the screen.
        // It is arranged as bitmap of 16bits of bitA followed by 16bits of bitB, each
        // AB bits corresponding to a cursor pixel. The whole map is 8kB in size.
        memcpy(TempLine, &vga.mem.linear[ vidstart ], vga.draw.width);
        // the index of the bit inside the cursor bitmap we start at:
        Bitu sourceStartBit = ((lineat - vga.s3.hgc.originy) + vga.s3.hgc.posy)*64 + vga.s3.hgc.posx; 
        // convert to video memory addr and bit index
        // start adjusted to the pattern structure (thus shift address by 2 instead of 3)
        // Need to get rid of the third bit, so "/8 *2" becomes ">> 2 & ~1"
        Bitu cursorMemStart = ((sourceStartBit >> 2) & ~1ul) + (((uint32_t)vga.s3.hgc.startaddr) << 10ul);
        Bitu cursorStartBit = sourceStartBit & 0x7u;
        // stay at the right position in the pattern
        if (cursorMemStart & 0x2) cursorMemStart--;
        Bitu cursorMemEnd = cursorMemStart + (Bitu)((64 - vga.s3.hgc.posx) >> 2);
        uint8_t* xat = &TempLine[hpos]; // mouse data start pos. in scanline
        for (Bitu m = cursorMemStart; m < cursorMemEnd; (m&1)?(m+=3):m++) {
            // for each byte of cursor data
            uint8_t bitsA = vga.mem.linear[m];
            uint8_t bitsB = vga.mem.linear[m+2];
            for (uint8_t bit=(0x80 >> cursorStartBit); bit != 0; bit >>= 1) {
                // for each bit
                cursorStartBit=0; // only the first byte has some bits cut off
                if (vga.s3.reg_55 & 0x10) {
                    // X11 mode: draw when mask bit is set, otherwise transparent
                    if (bitsA & bit) {
                        if (bitsB & bit) {
                            *xat = *(uint16_t*)vga.s3.hgc.forestack;
                        } else {
                            *xat = *(uint16_t*)vga.s3.hgc.backstack;
                        }
                    }
                } else {
                    // MS Windows mode
                    if (bitsA&bit) {
                        if (bitsB&bit) *xat ^= 0xFF; // Invert screen data
                        //else Transparent
                    } else if (bitsB&bit) {
                        *xat = vga.s3.hgc.forestack[0]; // foreground color
                    } else {
                        *xat = vga.s3.hgc.backstack[0];
                    }
                }
                xat++;
            }
        }
        return TempLine;
    }
}

/* render 16bpp line DOUBLED horizontally */
static uint8_t * VGA_Draw_LIN16_Line_2x(Bitu vidstart, Bitu /*line*/) {
    uint16_t *s = (uint16_t*)(&vga.mem.linear[vidstart]);
    uint16_t *d = (uint16_t*)TempLine;

    for (Bitu i = 0; i < (vga.draw.line_length>>2); i++) {
        d[0] = d[1] = *s++;
        d += 2;
    }

    return TempLine;
}

static uint8_t * VGA_Draw_LIN16_Line_HWMouse(Bitu vidstart, Bitu /*line*/) {
    if (!svga.hardware_cursor_active || !svga.hardware_cursor_active())
        return &vga.mem.linear[vidstart];

    Bitu lineat = ((vidstart-(vga.config.real_start<<2)) >> 1) / vga.draw.width;
    if ((vga.s3.hgc.posx >= vga.draw.width) ||
        (lineat < vga.s3.hgc.originy) || 
        (lineat > (vga.s3.hgc.originy + (63U-vga.s3.hgc.posy))) ) {
        return &vga.mem.linear[vidstart];
    } else {
        unsigned int hpos;

        // On 86C928 cards, the "horizontal stretch" modes determine how the cursor is formatted
        // to the DAC. Based on Windows 3.1/95 behavior, it apparently also affects the X coordinate
        // which must match the BYTE offset when run through the DAC. Without this code, the
        // cursor will be placed at 2x (in 16bpp) or 4x (in 32bpp) the actual position it should be.
        if (svgaCard == SVGA_S3Trio && (s3Card >= S3_86C928 && s3Card <= S3_Vision968)) {
            /* NTS: S3 datasheets document bits 2-3 as follows:
             *
             *      bit 2: Hardware cursor horizontal stretch 2 - twice the width (16bpp, apparently)
             *      bit 3: Hardware cursor horizontal stretch 3 - triple the width (24bpp, apparently)
             *
             * Windows 3.1 sets BOTH bits, which apparently means quadruple the width (32bpp)
             * Windows 95 does the same, which suggests that perhaps the hardware behaves as such.
             *
             * I don't know if this is how real hardware behaves but it's what Windows apparently
             * expects to do with the hardware based on how it's setting the X coordinate of the cursor. -- J.C. */
            hpos = vga.s3.hgc.originx / (1 + ((vga.s3.hgc.curmode >> 2u) & 3u));
        }
        else {
            hpos = vga.s3.hgc.originx;
        }

        memcpy(TempLine, &vga.mem.linear[ vidstart ], vga.draw.width*2);
        Bitu sourceStartBit = ((lineat - vga.s3.hgc.originy) + vga.s3.hgc.posy)*64 + vga.s3.hgc.posx; 
        Bitu cursorMemStart = ((sourceStartBit >> 2) & ~1ul) + (((uint32_t)vga.s3.hgc.startaddr) << 10ul);
        Bitu cursorStartBit = sourceStartBit & 0x7u;
        if (cursorMemStart & 0x2) cursorMemStart--;
        Bitu cursorMemEnd = cursorMemStart + (Bitu)((64 - vga.s3.hgc.posx) >> 2);
        uint16_t* xat = &((uint16_t*)TempLine)[hpos];
        for (Bitu m = cursorMemStart; m < cursorMemEnd; (m&1)?(m+=3):m++) {
            // for each byte of cursor data
            uint8_t bitsA = vga.mem.linear[m];
            uint8_t bitsB = vga.mem.linear[m+2];
            for (uint8_t bit=(0x80 >> cursorStartBit); bit != 0; bit >>= 1) {
                // for each bit
                cursorStartBit=0;
                if (vga.s3.reg_55 & 0x10) {
                    // X11 mode: draw when mask bit is set, otherwise transparent
                    if (bitsA & bit) {
                        if (bitsB & bit) {
                            *xat = *(uint16_t*)vga.s3.hgc.forestack;
                        } else {
                            *xat = *(uint16_t*)vga.s3.hgc.backstack;
                        }
                    }
                } else {
                    // MS Windows mode
                    if (bitsA&bit) {
                        // byte order doesn't matter here as all bits get flipped
                        if (bitsB&bit) *xat ^= ~0U;
                        //else Transparent
                    } else if (bitsB&bit) {
                        // Source as well as destination are uint8_t arrays, 
                        // so this should work out endian-wise?
                        *xat = *(uint16_t*)vga.s3.hgc.forestack;
                    } else {
                        *xat = *(uint16_t*)vga.s3.hgc.backstack;
                    }
                }
                xat++;
            }
        }
        return TempLine;
    }
}

static uint8_t * VGA_Draw_LIN32_Line_HWMouse(Bitu vidstart, Bitu /*line*/) {
#if SDL_BYTEORDER == SDL_LIL_ENDIAN && defined(MACOSX) && !defined(C_SDL2) /* Mac OS X Intel builds use a weird RGBA order (alpha in the low 8 bits) */
    Bitu offset = vidstart & vga.draw.linear_mask;
    Bitu i;

    for (i=0;i < vga.draw.width;i++)
        ((uint32_t*)TempLine)[i] = guest_bgr_to_macosx_rgba((((uint32_t*)(vga.draw.linear_base+offset))[i]));

    return TempLine;
#else
    if (!svga.hardware_cursor_active || !svga.hardware_cursor_active())
        return &vga.mem.linear[vidstart];

    Bitu lineat = ((vidstart-(vga.config.real_start<<2)) >> 2) / vga.draw.width;
    if ((vga.s3.hgc.posx >= vga.draw.width) ||
        (lineat < vga.s3.hgc.originy) || 
        (lineat > (vga.s3.hgc.originy + (63U-vga.s3.hgc.posy))) ) {
        return &vga.mem.linear[ vidstart ];
    } else {
        unsigned int hpos;

        // On 86C928 cards, the "horizontal stretch" modes determine how the cursor is formatted
        // to the DAC. Based on Windows 3.1/95 behavior, it apparently also affects the X coordinate
        // which must match the BYTE offset when run through the DAC. Without this code, the
        // cursor will be placed at 2x (in 16bpp) or 4x (in 32bpp) the actual position it should be.
        if (svgaCard == SVGA_S3Trio && (s3Card >= S3_86C928 && s3Card <= S3_Vision968)) {
            /* NTS: S3 datasheets document bits 2-3 as follows:
             *
             *      bit 2: Hardware cursor horizontal stretch 2 - twice the width (16bpp, apparently)
             *      bit 3: Hardware cursor horizontal stretch 3 - triple the width (24bpp, apparently)
             *
             * Windows 3.1 sets BOTH bits, which apparently means quadruple the width (32bpp)
             * Windows 95 does the same, which suggests that perhaps the hardware behaves as such.
             *
             * I don't know if this is how real hardware behaves but it's what Windows apparently
             * expects to do with the hardware based on how it's setting the X coordinate of the cursor. -- J.C. */
            hpos = vga.s3.hgc.originx / (1 + ((vga.s3.hgc.curmode >> 2u) & 3u));
        }
        else {
            hpos = vga.s3.hgc.originx;
        }

        memcpy(TempLine, &vga.mem.linear[ vidstart ], vga.draw.width*4);
        Bitu sourceStartBit = ((lineat - vga.s3.hgc.originy) + vga.s3.hgc.posy)*64 + vga.s3.hgc.posx; 
        Bitu cursorMemStart = ((sourceStartBit >> 2) & ~1ul) + (((uint32_t)vga.s3.hgc.startaddr) << 10ul);
        Bitu cursorStartBit = sourceStartBit & 0x7u;
        if (cursorMemStart & 0x2) cursorMemStart--;
        Bitu cursorMemEnd = cursorMemStart + (Bitu)((64 - vga.s3.hgc.posx) >> 2);
        uint32_t* xat = &((uint32_t*)TempLine)[hpos];
        for (Bitu m = cursorMemStart; m < cursorMemEnd; (m&1)?(m+=3):m++) {
            // for each byte of cursor data
            uint8_t bitsA = vga.mem.linear[m];
            uint8_t bitsB = vga.mem.linear[m+2];
            for (uint8_t bit=(0x80 >> cursorStartBit); bit != 0; bit >>= 1) { // for each bit
                cursorStartBit=0;
                if (vga.s3.reg_55 & 0x10) {
                    // X11 mode: draw when mask bit is set, otherwise transparent
                    if (bitsA & bit) {
                        if (bitsB & bit) {
                            *xat = *(uint16_t*)vga.s3.hgc.forestack;
                        } else {
                            *xat = *(uint16_t*)vga.s3.hgc.backstack;
                        }
                    }
                } else {
                    // MS Windows mode
                    if (bitsA&bit) {
                        if (bitsB&bit) *xat ^= ~0U;
                        //else Transparent
                    } else if (bitsB&bit) {
                        *xat = *(uint32_t*)vga.s3.hgc.forestack;
                    } else {
                        *xat = *(uint32_t*)vga.s3.hgc.backstack;
                    }
                }
                xat++;
            }
        }
        return TempLine;
    }
#endif
}

static const uint32_t* VGA_Planar_Memwrap(Bitu vidstart) {
    return (const uint32_t*)vga.mem.linear + (vidstart & vga.draw.planar_mask);
}

static const uint8_t* VGA_Text_Memwrap(Bitu vidstart) {
    vidstart &= vga.draw.linear_mask;
    Bitu line_end = 2 * vga.draw.blocks;
    if (GCC_UNLIKELY((vidstart + line_end) > vga.draw.linear_mask)) {
        // wrapping in this line
        Bitu break_pos = (vga.draw.linear_mask - vidstart) + 1;
        // need a temporary storage - TempLine/2 is ok for a bit more than 132 columns
        memcpy(&TempLine[sizeof(TempLine)/2], &vga.tandy.draw_base[vidstart], break_pos);
        memcpy(&TempLine[sizeof(TempLine)/2 + break_pos],&vga.tandy.draw_base[0], line_end - break_pos);
        return &TempLine[sizeof(TempLine)/2];
    } else return &vga.tandy.draw_base[vidstart];
}

static uint32_t FontMask[2]={0xffffffff,0x0};

template <const bool snow> static uint8_t * CGA_COMMON_TEXT_Draw_Line(Bitu vidstart, Bitu line) {
    Bits font_addr;
    uint32_t * draw=(uint32_t *)TempLine;
    const uint8_t* vidmem = VGA_Text_Memwrap(vidstart);

    if (snow) {
        /* HACK: our code does not have render control during VBLANK, zero our
         *       noise bits on the first scanline */
        if (line == 0)
            memset(vga.draw.cga_snow,0,sizeof(vga.draw.cga_snow));
    }

    for (Bitu cx=0;cx<vga.draw.blocks;cx++) {
        Bitu chr,col;
        chr=vidmem[cx*2];
        col=vidmem[cx*2+1];
        if (snow && (cx&1) == 0 && cx <= 78) {
            /* Trixter's "CGA test" program and reference video seems to suggest
             * to me that the CGA "snow" might contain the value written by the CPU. */
            if (vga.draw.cga_snow[cx] != 0)
                chr = vga.draw.cga_snow[cx];
            if (vga.draw.cga_snow[cx+1] != 0)
                col = vga.draw.cga_snow[cx+1];
        }

        Bitu font=vga.draw.font_tables[(col >> 3)&1][chr*32+line];
        uint32_t mask1=TXT_Font_Table[font>>4] & FontMask[col >> 7];
        uint32_t mask2=TXT_Font_Table[font&0xf] & FontMask[col >> 7];
        uint32_t fg=TXT_FG_Table[col&0xf];
        uint32_t bg=TXT_BG_Table[col>>4];
        *draw++=(fg&mask1) | (bg&~mask1);
        *draw++=(fg&mask2) | (bg&~mask2);
    }

    if (snow)
        memset(vga.draw.cga_snow,0,sizeof(vga.draw.cga_snow));

    if (!vga.draw.cursor.enabled || !(vga.draw.cursor.count&0x8)) goto skip_cursor;
    font_addr = ((Bits)vga.draw.cursor.address - (Bits)vidstart) >> 1ll;
    if (font_addr>=0 && font_addr<(Bits)vga.draw.blocks) {
        if (line<vga.draw.cursor.sline) goto skip_cursor;
        if (line>vga.draw.cursor.eline) goto skip_cursor;
        draw=(uint32_t *)&TempLine[(unsigned long)font_addr*8ul];
        uint32_t att=TXT_FG_Table[vga.tandy.draw_base[vga.draw.cursor.address+1ul]&0xfu];
        *draw++=att;*draw++=att;
    }
skip_cursor:
    return TempLine;
}

static uint8_t * VGA_TEXT_Draw_Line(Bitu vidstart, Bitu line) {
    return CGA_COMMON_TEXT_Draw_Line<false>(vidstart,line);
}

static uint8_t * VGA_CGASNOW_TEXT_Draw_Line(Bitu vidstart, Bitu line) {
    return CGA_COMMON_TEXT_Draw_Line<true>(vidstart,line);
}

static uint8_t * MCGA_TEXT_Draw_Line(Bitu vidstart, Bitu line) {
    // keep it aligned:
    uint32_t* draw = (uint32_t*)TempLine;
    const uint8_t* vidmem = VGA_Text_Memwrap(vidstart);
    Bitu blocks = vga.draw.blocks;
    if (vga.draw.panning) blocks++; // if the text is panned part of an
                                    // additional character becomes visible
    while (blocks--) { // for each character in the line
        Bitu chr = *vidmem++;
        Bitu attr = *vidmem++;
        // the font pattern
        Bitu font = vga.draw.font_tables[(attr >> 3u)&1u][(chr<<5u)+line];
        
        Bitu background = attr >> 4u;
        // if blinking is enabled bit7 is not mapped to attributes
        if (vga.draw.blinking) background &= ~0x8u;
        // choose foreground color if blinking not set for this cell or blink on
        Bitu foreground = (vga.draw.blink || (!(attr&0x80)))?
            (attr&0xf):background;
        // underline: all foreground [freevga: 0x77, previous 0x7]
        if (GCC_UNLIKELY(((attr&0x77) == 0x01) &&
            (vga.crtc.underline_location&0x1f)==line))
                background = foreground;
        if (vga.draw.char9dot) {
            font <<=1; // 9 pixels
            // extend to the 9th pixel if needed
            if ((font&0x2) && (vga.attr.mode_control&0x04) &&
                (chr>=0xc0) && (chr<=0xdf)) font |= 1;
            for (Bitu n = 0; n < 9; n++) {
                *draw++ = vga.dac.xlat32[(font&0x100)? foreground:background];
                font <<= 1;
            }
        } else {
            for (Bitu n = 0; n < 8; n++) {
                *draw++ = vga.dac.xlat32[(font&0x80)? foreground:background];
                font <<= 1;
            }
        }
    }

    // cursor appearance is also affected by the MCGA double-scanning
    if (mcga_double_scan)
        line >>= 1ul;

    // draw the text mode cursor if needed
    if ((vga.draw.cursor.count&0x8) && (line >= vga.draw.cursor.sline) &&
        (line <= vga.draw.cursor.eline) && vga.draw.cursor.enabled) {
        // the address of the attribute that makes up the cell the cursor is in
        Bits attr_addr = ((Bits)vga.draw.cursor.address - (Bits)vidstart) >> (Bits)1; /* <- FIXME: This right? */
        if (attr_addr >= 0 && attr_addr < (Bits)vga.draw.blocks) {
            Bitu index = (Bitu)attr_addr * (vga.draw.char9dot?9u:8u) * 4u;
            draw = (uint32_t*)(&TempLine[index]);
            
            Bitu foreground = vga.tandy.draw_base[(vga.draw.cursor.address<<1)+1] & 0xf;
            for (Bitu i = 0; i < 8; i++) {
                *draw++ = vga.dac.xlat32[foreground];
            }
        }
    }
    return TempLine;
}

static uint8_t * VGA_TEXT_Herc_Draw_Line(Bitu vidstart, Bitu line) {
    Bits font_addr;
    uint32_t * draw=(uint32_t *)TempLine;
    const uint8_t* vidmem = VGA_Text_Memwrap(vidstart);

    for (Bitu cx=0;cx<vga.draw.blocks;cx++) {
        Bitu chr=vidmem[cx*2];
        Bitu attrib=vidmem[cx*2+1];
        if (!(attrib&0x77)) {
            // 00h, 80h, 08h, 88h produce black space
            *draw++=0;
            *draw++=0;
        } else {
            uint32_t bg, fg;
            bool underline=false;
            if ((attrib&0x77)==0x70) {
                bg = TXT_BG_Table[0x7];
                if (attrib&0x8) fg = TXT_FG_Table[0xf];
                else fg = TXT_FG_Table[0x0];
            } else {
                if (((Bitu)(vga.crtc.underline_location&0x1f)==line) && ((attrib&0x77)==0x1)) underline=true;
                bg = TXT_BG_Table[0x0];
                if (attrib&0x8) fg = TXT_FG_Table[0xf];
                else fg = TXT_FG_Table[0x7];
            }
            uint32_t mask1, mask2;
            if (GCC_UNLIKELY(underline)) mask1 = mask2 = FontMask[attrib >> 7];
            else {
                Bitu font=vga.draw.font_tables[0][chr*32+line];
                mask1=TXT_Font_Table[font>>4] & FontMask[attrib >> 7]; // blinking
                mask2=TXT_Font_Table[font&0xf] & FontMask[attrib >> 7];
            }
            *draw++=(fg&mask1) | (bg&~mask1);
            *draw++=(fg&mask2) | (bg&~mask2);
        }
    }
    if (!vga.draw.cursor.enabled || !(vga.draw.cursor.count&0x8)) goto skip_cursor;
    font_addr = ((Bits)vga.draw.cursor.address - (Bits)vidstart) >> 1ll;
    if (font_addr>=0 && font_addr<(Bits)vga.draw.blocks) {
        if (line<vga.draw.cursor.sline) goto skip_cursor;
        if (line>vga.draw.cursor.eline) goto skip_cursor;
        draw=(uint32_t *)&TempLine[(unsigned long)font_addr*8ul];
        uint8_t attr = vga.tandy.draw_base[vga.draw.cursor.address+1];
        uint32_t cg;
        if (attr&0x8) {
            cg = TXT_FG_Table[0xf];
        } else if ((attr&0x77)==0x70) {
            cg = TXT_FG_Table[0x0];
        } else {
            cg = TXT_FG_Table[0x7];
        }
        *draw++=cg;*draw++=cg;
    }
skip_cursor:
    return TempLine;
}

// Wengier: Auto-detect box-drawing characters in CJK mode for TTF output
bool CheckBoxDrawing(uint8_t c1, uint8_t c2, uint8_t c3, uint8_t c4) {
    if (dos.loaded_codepage == 932
#if defined(USE_TTF)
    && halfwidthkana
#endif
    ) return false;
    return (c1 == 196 && c2 == 196 && c3 == 196 && (c4 == 180 || c4 == 182 || c4 == 183 || c4 == 189 || c4 == 191 || c4 == 193 || c4 == 194 || c4 == 196 || c4 == 197 || c4 == 208 || c4 == 210 || c4 == 215 || c4 == 217)) ||
    ((c1 == 192 || c1 == 193 || c1 == 194 || c1 == 195 || c1 == 196 || c1 == 197 || c1 == 199 || c1 == 208 || c1 == 210 || c1 == 211 || c1 == 214 || c1 == 215 || c1 == 218) && c2 == 196 && c3 == 196 && c4 == 196) ||
    (c1 == 205 && c2 == 205 && c3 == 205 && (c4 == 181 || c4 == 184 || c4 == 185 || c4 == 187 || c4 == 188 || c4 == 189 || c4 == 190 || c4 == 202 || c4 == 203 || c4 == 205 || c4 == 207 || c4 == 209 || c4 == 216)) ||
    ((c1 == 198 || c1 == 200 || c1 == 201 || c1 == 202 || c1 == 203 || c1 == 204 || c1 == 205 || c1 == 206 || c1 == 207 || c1 == 209 || c1 == 212 || c1 == 213 || c1 == 216) && c2 == 205 && c3 == 205 && c4 == 205) ||
    (c1 == 196 && c2 == 196 && (c3 == 193 || c3 == 194 || c3 == 197) && c4 == 196) || (c1 == 196 && (c2 == 193 || c2 == 194 || c2 == 197) && c3 == 196 && c4 == 196) ||
    (c1 == 176 && c2 == 176 && c3 == 176 && c4 == 176) || (c1 == 177 && c2 == 177 && c3 == 177 && c4 == 177) || (c1 == 178 && c2 == 178 && c3 == 178 && c4 == 178) ||
    (c1 == 192 && c2 == 193 && c3 == 193 && c4 == 193) || (c1 == 193 && c2 == 193 && c3 == 193 && c4 == 193) || (c1 == 218 && c2 == 194 && c3 == 194 && c4 == 194) ||
    (c1 == 194 && c2 == 194 && c3 == 194 && c4 == 194) || (c1 == 195 && c2 == 197 && c3 == 197 && c4 == 197) || (c1 == 197 && c2 == 197 && c3 == 197 && c4 == 197) ||
    (c1 == 202 && c2 == 202 && c3 == 202 && c4 == 202) || (c1 == 203 && c2 == 203 && c3 == 203 && c4 == 203) || (c1 == 206 && c2 == 206 && c3 == 206 && c4 == 206) ||
    (c1 == 216 && c2 == 216 && c3 == 216 && c4 == 216) || (c1 == 221 && c2 == 221 && c3 == 221 && c4 == 221) || (c1 == 219 && c2 == 219 && c3 == 219 && c4 == 219) ||
    (c1 == 220 && c2 == 220 && c3 == 220 && c4 == 219) || (c1 == 220 && c2 == 220 && c3 == 220 && c4 == 220) || (c1 == 222 && c2 == 222 && c3 == 222 && c4 == 222) ||
    (c1 == 207 && c2 == 207 && c3 == 207 && c4 == 207) || (c1 == 208 && c2 == 208 && c3 == 208 && c4 == 208) || (c1 == 254 && c2 == 177 && c3 == 177 && c4 == 177) ||
    (c1 == 177 && c2 == 254 && c3 == 177 && c4 == 177) || (c1 == 177 && c2 == 177 && c3 == 254 && c4 == 177) || (c1 == 177 && c2 == 177 && c3 == 177 && c4 == 254) ||
    (c1 == 222 && c2 == 223 && c3 == 223 && c4 == 223) || (c1 == 222 && c2 == 250 && c3 == 250 && c4 == 250) || (c1 == 222 && c2 == 220 && c3 == 220 && c4 == 220) || (c1 == 223 && c2 == 223 && c3 == 223 && c4 == 221) ||
    (c1 == 223 && c2 == 223 && (c3 == 88 || c3 == 223) && c4 == 223) || (c1 == 223 && c2 == 220 && c3 == 220 && c4 == 220) || (c1 == 240 && c2 == 240 && c3 == 240 && c4 == 240) ||
    ((c1 == 196 || c1 == 205) && c2 == 91 && (c3 == 15 || c3 == 49 || c3 == 254) && c4 == 93) || (c1 == 91 && (c2 == 15 || c2 == 49 || c2 == 254) && c3 == 93 && (c4 == 196 || c4 == 205));
}

// Workaround for Turbo Pascal, Turbo C/C++ 3, and DOS Navigator 2
bool CheckBoxDrawLast(Bitu col, uint8_t chr0, uint8_t chr1, uint8_t chr2, uint8_t chr3, uint8_t chr4, uint8_t chr5, uint8_t chr6, uint8_t chr7, uint8_t chr8) {
    if (dos.loaded_codepage == 932
#if defined(USE_TTF)
    && halfwidthkana
#endif
    ) return false;
    return (col == 71 && (chr0 == 196 || chr0 == 205) && (chr1 == 196 || chr1 == 205) && (chr3 == 196 || chr3 == 205) && chr4 == 91 && (chr5 == 15 || chr5 == 18 || chr5 == 24) && chr6 == 93 && (chr7 == 196 || chr7 == 205) && ((chr7 == 205 && chr8 == 187) || (chr7 == 196 && chr8 == 191))) ||
           (col == 72 && (chr0 == 196 || chr0 == 205) && (chr2 == 196 || chr2 == 205) && chr3 == 91 && (chr4 == 15 || chr4 == 18 || chr4 == 24) && chr5 == 93 && (chr6 == 196 || chr6 == 205) && ((chr6 == 205 && chr7 == 187) || (chr6 == 196 && chr7 == 191))) ||
           (col == 73 && (chr1 == 196 || chr1 == 205) && chr2 == 91 && (chr3 == 15 || chr3 == 18 || chr3 == 24) && chr4 == 93 && (chr5 == 196 || chr5 == 205) && ((chr5 == 205 && chr6 == 187) || (chr5 == 196 && chr6 == 191))) ||
           (col == 74 && chr0 == 91 && (chr2 == 15 || chr2 == 18 || chr2 == 24) && chr3 == 93 && (chr4 == 196 || chr4 == 205) && ((chr4 == 205 && chr5 == 187) || (chr4 == 196 && chr5 == 191)));
}

bool connectLeft(uint8_t c, bool db, bool line) {
    if (db) return (!line && (c == 200 || c == 201 || c == 202 || c == 203)) || c == 204 || c == 205 || c == 206;
    else return (!line && (c == 192 || c == 193 || c == 194 || c == 218)) || c == 195 || c == 196 || c == 197;
}

bool connectRight(uint8_t c, bool db, bool line) {
    if (db) return (!line && (c == 187 || c == 188 || c == 202 || c == 203)) || c == 185 || c == 205 || c == 206;
    else return (!line && (c == 191 || c == 193 || c == 194 || c == 217)) || c == 180 || c == 196 || c == 197;
}

bool connectUp(uint8_t c, bool db) {
    if (db) return c == 182 || c == 183 || c == 185 || c == 186 || c == 187 || c == 199 || c == 201 || c == 203 || c == 204 || c == 206 || c == 210 || c == 214 || c == 215;
    else return c == 179 || c == 180 || c == 181 || c == 184 || c == 191 || c == 194 || c == 195 || c == 197 || c == 198 || c == 209 || c == 213 || c == 216 || c == 218;
}

bool connectDown(uint8_t c, bool db) {
    if (db) return c == 182 || c == 185 || c == 186 || c == 188 || c == 189 || c == 199 || c == 200 || c == 202 || c == 204 || c == 206 || c == 208 || c == 211 || c == 215;
    else return c == 179 || c == 180 || c == 181 || c == 190 || c == 192 || c == 193 || c == 195 || c == 197 || c == 198 || c == 207 || c == 212 || c == 216 || c == 217;
}

bool connectHalf(uint8_t cl, uint8_t cr, bool first) {
    if (!first) {uint8_t tmp=cr;cr=cl;cl=tmp;}
    return cr == 183 || cr == 184 || (cr >= 187 && cr <= 191) || (cl >= 192 && cl <= 194) || (cr >= 193 && cr <= 194) || (cl >= 200 && cl <= 203) || (cr >= 202 && cr <= 203) || (cl >= 207 && cl <= 210) || (cr >= 207 && cr <= 210) || (cl >= 211 && cl <= 214) || cr == 217 || cl == 218;
}

bool isBDV(uint8_t c1, uint8_t c2, uint8_t c3, uint8_t c4, uint8_t c5, uint8_t c6, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4, uint8_t b5, bool first) {
    return (c1 == 179 && connectUp(b4, false) && c2 == 179 && !connectHalf(b2, c5, first) || (c3 == 193 || c3 == 197) && c1 == 197 && connectLeft(first?b1:c4, false, true) && connectRight(first?c4:b1, false, true) && c2 == 197 && connectLeft(first?b2:c5, false, true) && connectRight(first?c5:b2, false, true)) && (c3 == 179 && connectDown(b5, false) || c3 == 180 && connectLeft(first?b3:c6, false, true) || c3 == 192 && connectRight(first?c6:b3, false, false) || c3 == 193 && connectRight(first?c6:b3, false, false) && connectLeft(first?b3:c6, false, false) || c3 == 195 && connectRight(first?c6:b3, false, true) || c3 == 197 && connectLeft(first?b3:c6, false, true) && connectRight(first?c6:b3, false, true) || c3 == 217 && connectLeft(first?b3:c6, false, false)) ||
           ((c1 == 180 && connectLeft(first?b1:c4, false, true) || c1 == 191 && connectLeft(first?b1:c4, false, false)) && (c2 == 179 && !connectHalf(b2, c5, first) || c2 == 180 && connectLeft(first?b2:c5, true, true)) && (c3 == 179 && connectDown(b5, false) || c3 == 180 && connectLeft(first?b3:c6, false, true) || c3 == 217 && connectLeft(first?b3:c6, false, false))) ||
           ((c1 == 195 && connectRight(first?c4:b1, false, true) || c1 == 218 && connectRight(first?c4:b1, false, false)) && (c2 == 179 && !connectHalf(b2, c5, first) || c2 == 195 && connectRight(first?c5:b2, false, true)) && (c3 == 179 && connectDown(b5, false) || c3 == 192 && connectRight(first?c6:b3, false, false) || c3 == 195 && connectRight(first?c6:b3, false, true))) ||
           ((c1 == 180 && connectLeft(first?b1:c4, false, true) || c1 == 191 && connectLeft(first?b1:c4, false, false) || c1 == 194 && connectLeft(first?b1:c4, false, false) && connectRight(first?c4:b1, false, false) || c1 == 195 && connectRight(first?c4:b1, false, true) || c1 == 197 && connectLeft(first?b1:c4, false, true) && connectRight(first?c4:b1, false, true) || c1 == 218 && connectRight(first?c4:b1, false, false)) && (c2 == 179 && !connectHalf(b2, c5, first) && c3 == 179 && connectDown(b5, false) || (c1 == 194 || c1 == 197) && c2 == 197 && connectLeft(first?b2:c5, false, true) && connectRight(first?c5:b2, false, true) && c3 == 197 && connectLeft(first?b3:c6, false, true) && connectRight(first?c6:b3, false, true))) ||
           (c1 == 179 && connectUp(b4, false) && (c2 == 180 && connectLeft(first?b2:c5, false, true) || c2 == 195 && connectRight(first?c5:b2, false, true) || c2 == 197 && connectLeft(first?b2:c5, false, true) && connectRight(first?c5:b2, false, true)) && c3 == 179 && connectDown(b5, false)) ||
           ((c1 == 186 && connectUp(b4, true) && c2 == 186 && !connectHalf(b2, c5, first) || (c3 == 202 || c3 == 206) && c1 == 206 && connectLeft(first?b1:c4, true, true) && connectRight(first?c4:b1, true, true) && c2 == 206 && connectLeft(first?b2:c5, true, true) && connectRight(first?c5:b2, true, true)) && (c3 == 185 && connectLeft(first?b3:c6, true, true) || c3 == 186 && connectDown(b5, true) || c3 == 188 && connectLeft(first?b3:c6, true, false) || c3 == 200 && connectRight(first?c6:b3, true, false) || c3 == 202 && connectRight(first?c6:b3, true, false) && connectRight(first?c6:b3, true, false) || c3 == 204 && connectRight(first?c6:b3, true, true) || c3 == 206 && connectRight(first?c6:b3, true, true) && connectRight(first?c6:b3, true, true))) ||
           ((c1 == 185 && connectLeft(first?b1:c4, true, true) || c1 == 187 && connectLeft(first?b1:c4, true, false)) && (c2 == 185 && connectLeft(first?b2:c5, true, true) || c2 == 186 && !connectHalf(b2, c5, first)) && (c3 == 185 && connectLeft(first?b3:c6, true, true) || c3 == 186 && connectDown(b5, true) || c3 == 188 && connectLeft(first?b3:c6, true, false))) ||
           ((c1 == 201 && connectRight(first?c4:b1, true, false) || c1 == 204 && connectRight(first?c4:b1, true, true)) && (c2 == 186 && !connectHalf(b2, c5, first) || c2 == 204 && connectLeft(first?b2:c5, true, true)) && (c3 == 186 && connectDown(b5, true) || c3 == 200 && connectRight(first?c6:b3, true, false) || c3 == 204 && connectRight(first?c6:b3, true, true))) ||
           ((c1 == 185 && connectLeft(first?b1:c4, true, true) || c1 == 187 && connectLeft(first?b1:c4, true, false) || c1 == 201 && connectRight(first?c4:b1, true, false) || c1 == 203 && connectLeft(first?b1:c4, true, false) && connectRight(first?c4:b1, true, false) || c1 == 204 && connectRight(first?c4:b1, true, true) || c1 == 206 && connectLeft(first?b1:c4, true, true) && connectRight(first?c4:b1, true, true)) && (c2 == 186 && !connectHalf(b2, c5, first) && c3 == 186 && connectDown(b5, true) || (c1 == 203 || c1 == 206) && c2 == 206 && connectLeft(first?b3:c6, true, true) && connectRight(first?c5:b2, true, true) && c3 == 206 && connectLeft(first?b3:c6, true, true) && connectRight(first?c6:b3, true, true))) ||
           (c1 == 186 && connectUp(b4, true) && (c2 == 185 && connectLeft(first?b2:c5, true, true) || c2 == 204 && connectRight(first?c5:b2, true, true) || c2 == 206 && connectLeft(first?b2:c5, true, true) && connectRight(first?c5:b2, true, true)) && c3 == 186 && connectDown(b5, true));
}

bool CheckBoxDrawingV(uint8_t c1, uint8_t c2, uint8_t c3, uint8_t c4, uint8_t c5, uint8_t c6, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4, uint8_t b5, uint8_t b6, uint8_t b7, uint8_t b8, uint8_t b9, uint8_t b10) {
    if (dos.loaded_codepage == 932
#if defined(USE_TTF)
    && halfwidthkana
#endif
    ) return false;
    if ((c1 == c2 && c1 == c3 && c1 == c4 && c1 == c5 && c1 == c6) && c1 >= 176 && c1 <= 178) return true;
    if ((c1 < 179 || c1 > 218 || c2 < 179 || c2 > 218 || c3 < 179 || c3 > 218) && (c4 < 179 || c4 > 218 || c5 < 179 || c5 > 218 || c6 < 179 || c6 > 218)) return false;
    return isBDV(c1, c2, c3, c4, c5, c6, b1, b2, b3, b7, b8, true) || isBDV(c4, c5, c6, c1, c2, c3, b4, b5, b6, b9, b10, false);
}

#if defined(USE_TTF)
void checkChar(Bitu col, ttf_cell *draw, uint16_t &last) {
	(*draw).chr = ' ';
	if (col && (last == 0x2014 || last == 0x2025 || last == 0x2026 || last == 0x2261 || last == 0x2500 || last == 0x2501 || last == 0x250f || last == 0x2517 || last == 0x250c || last == 0x2514 || last == 0x251c || last == 0x2520 || last == 0x2523 || last == 0x252c || last == 0x252f || last == 0x2533 || last == 0x2534 || last == 0x2537 || last == 0x253b || last == 0x253c || last == 0x2543 || last == 0x2544 || last == 0x2545 || last == 0x2546 || last == 0x2550 || last == 0x2554 || last == 0x255A || last == 0x2560 || last == 0x256C || last == 0x256D || last == 0x2570)) {
	   (*draw).unicode = 1;
	   (*draw).chr = last == 0x2554 || last == 0x255A || last == 0x2560 || last == 0x256C ? 0x2550 : (last == 0x250c || last == 0x2514 || last == 0x251c || last == 0x2520 || last == 0x252c || last == 0x2534 || last == 0x253c || last == 0x2543 || last == 0x2545 || last == 0x256D || last == 0x2570 ? 0x2500 : (last >= 0x250f && last != 0x2550 ? 0x2501 : last));
	}
	last = 0;
}

void handleChar(uint16_t *uname, ttf_cell *draw, bool &dbw, bool &dex, uint16_t &last) {
    if (uname[0]!=0&&uname[1]==0) {
        int width, height, res;
        (*draw).chr=uname[0];
        (*draw).unicode=1;
        res = width = 0;
        if (ttf.SDL_font) res = TTF_SizeUNICODE(ttf.SDL_font, uname, &width, &height);
        if (res >= 0 && width <= ttf.width+1) { // Single wide, yet DBCS encoding
            if (!width) (*draw).chr=' ';
            last = (*draw).chr;
            dbw=false;
            dex=true;
        } else {
            (*draw).doublewide = 1;
            dbw=true;
            dex=false;
        }
    } else {
        (*draw).chr=' ';
        dbw=false;
        dex=true;
    }
}
#endif

bool isDBCSCP() {
    return !IS_PC98_ARCH && (IS_JEGA_ARCH||IS_DOSV||dos.loaded_codepage==932||dos.loaded_codepage==936||dos.loaded_codepage==949||dos.loaded_codepage==950||dos.loaded_codepage==951) && enable_dbcs_tables;
}

#if 0//not used
bool isDBCSLB(uint8_t chr) {
    for (int i=0; i<6; i++) lead[i] = 0;
    if (isDBCSCP())
        for (int i=0; i<6; i++) {
            lead[i] = mem_readb(Real2Phys(dos.tables.dbcs)+i+2);
            if (lead[i] == 0) break;
        }
    return isDBCSCP() && ((lead[0]>=0x80 && lead[1] > lead[0] && chr >= lead[0] && chr <= lead[1]) || (lead[2]>=0x80 && lead[3] > lead[2] && chr >= lead[2] && chr <= lead[3]) || (lead[4]>=0x80 && lead[5] > lead[4] && chr >= lead[4] && chr <= lead[5]));
}
#endif

std::vector<std::pair<int,int>> jtbs = {}, dbox = {};
struct first_equal {
    const int value;
    first_equal(int v):value(v) {}
    bool operator()(std::pair<int,int> &pair) {
        return pair.first == value;
    }
};

template <const unsigned int card,typename templine_type_t> static inline uint8_t* EGAVGA_TEXT_Combined_Draw_Line(Bitu vidstart,Bitu line) {
    // keep it aligned:
    templine_type_t* draw = ((templine_type_t*)TempLine) + 16 - vga.draw.panning;
    const uint32_t* vidmem = VGA_Planar_Memwrap(vidstart); // pointer to chars+attribs
    Bitu blocks = vga.draw.blocks;
    if (vga.draw.panning) blocks++; // if the text is panned part of an 
                                    // additional character becomes visible
    Bitu background = 0, foreground = 0;
    Bitu chr, chr_left = 0, p3 = 0, attr, bsattr;
    bool chr_wide = false;
    bool usedbcs = (isJEGAEnabled() || (isDBCSCP()
#if defined(USE_TTF)
                   && dbcs_sbcs
#endif
                   && showdbcs) && CurMode && CurMode->type == M_TEXT && !dos_kernel_disabled && strcmp(RunningProgram, "LOADLIN"));

    if (usedbcs && (vga.draw.height < 16u || vga.draw.width < 8u)) return TempLine;

    unsigned int row = usedbcs ? ((vidstart - vga.config.real_start - vga.draw.bytes_skip) / vga.draw.address_add) : 0, col = 0;
    unsigned int rows = usedbcs ? ((vga.draw.height / 16u) - 1u) : 0, cols = usedbcs ? ((vga.draw.address_add / 2u) - 1u) : 0;

    if (usedbcs && line == 1) {
        if (!jtbs.empty()) jtbs.erase(std::remove_if(jtbs.begin(), jtbs.end(), first_equal(row)), jtbs.end());
        if (!dbox.empty()) dbox.erase(std::remove_if(dbox.begin(), dbox.end(), first_equal(row)), dbox.end());
    }

    VGA_Latch pixeln1, pixeln2, pixeln3, pixelp1, pixelp2, pixelp3;
    while (blocks--) {
        if (!col) p3 = 0;
        if (usedbcs) { // DBCS case
            VGA_Latch pixels;
            pixels.d = *vidmem;
            chr = pixels.b[0];
            attr = pixels.b[1];
            pixeln1.d = *(vidmem + ((uintptr_t)1U << (uintptr_t)vga.config.addr_shift));
            pixeln2.d = *(vidmem + 2 * ((uintptr_t)1U << (uintptr_t)vga.config.addr_shift));
            pixeln3.d = *(vidmem + 3 * ((uintptr_t)1U << (uintptr_t)vga.config.addr_shift));
            pixelp1.d = *(vidmem - ((uintptr_t)1U << (uintptr_t)vga.config.addr_shift));
            pixelp2.d = *(vidmem - 2 * ((uintptr_t)1U << (uintptr_t)vga.config.addr_shift));
            //pixelp3.d = *(vidmem - 3 * ((uintptr_t)1U << (uintptr_t)vga.config.addr_shift));
            if (!chr_wide) {
                if (!isJEGAEnabled() || !(jega.RMOD2 & 0x80)) {
                    background = attr >> 4;
                    if (vga.draw.blinking) background &= ~0x8;
                    foreground = (vga.draw.blink || (!(attr & 0x80))) ? (attr & 0xf) : background;
                    bsattr = 0;
                } else {
                    foreground = (vga.draw.blink || (!(attr & 0x80))) ? (attr & 0xf) : background;
                    background = 0;
                    bsattr = attr;
                    if (bsattr & 0x40) {
                        Bitu tmp = background;
                        background = foreground;
                        foreground = tmp;
                    }
                }
                if (isKanji1(chr) && col < cols && isKanji2(pixeln1.b[0]) && !isemptyhit(chr*0x100+pixeln1.b[0]) && !(!isJEGAEnabled() &&
#if defined(USE_TTF)
                autoboxdraw &&
#endif
                ((col < cols-3 && CheckBoxDrawing(chr, pixeln1.b[0], pixeln2.b[0], pixeln3.b[0])) ||
                (col && col < cols-2 && CheckBoxDrawing(pixelp1.b[0], chr, pixeln1.b[0], pixeln2.b[0])) ||
                (col > 1 && col < cols-1 && CheckBoxDrawing(pixelp2.b[0], pixelp1.b[0], chr, pixeln1.b[0])) ||
                (col > 2 && CheckBoxDrawing(p3, pixelp2.b[0], pixelp1.b[0], chr)) ||
                (cols >= 79 && col == 74 && row < rows && CheckBoxDrawLast(col-3, *(vidmem-6), *(vidmem-4), *(vidmem-2), chr, *(vidmem+2), *(vidmem+4), *(vidmem+6), *(vidmem+8), *(vidmem+10))) ||
                (cols >= 79 && col == 78 && row < rows && CheckBoxDrawLast(col-7, *(vidmem-14), *(vidmem-12), *(vidmem-10), *(vidmem-8), *(vidmem-6), *(vidmem-4), *(vidmem-2), chr, *(vidmem+2))) ||
                (row && col>3 && (((uint8_t)*(vidmem-4)==177||(uint8_t)*(vidmem-4)==254)&&(uint8_t)*(vidmem-2)==16&&(uint8_t)chr==196&&(uint8_t)*(vidmem+2)==217)||((uint8_t)*(vidmem-4)==176&&(uint8_t)*(vidmem-2)==176&&(uint8_t)chr==176&&(uint8_t)*(vidmem+2)==179)) ||
                (row < rows-2 && CheckBoxDrawingV(chr, *(vidmem+vga.draw.address_add), *(vidmem+2*vga.draw.address_add), *(vidmem+2), *(vidmem+2+vga.draw.address_add), *(vidmem+2+2*vga.draw.address_add), col?*(vidmem-2):0, col?*(vidmem-2+vga.draw.address_add):0, col?*(vidmem-2+2*vga.draw.address_add):0, col < cols-2?*(vidmem+4):0, col < cols-2?*(vidmem+4+vga.draw.address_add):0, col < cols-2?*(vidmem+4+2*vga.draw.address_add):0, row?*(vidmem-vga.draw.address_add):0, row < rows-3?*(vidmem+3*vga.draw.address_add):0, row?*(vidmem+2-vga.draw.address_add):0, row < rows-3?*(vidmem+2+3*vga.draw.address_add):0)) ||
                (row && row < rows-1 && CheckBoxDrawingV(*(vidmem-vga.draw.address_add), chr, *(vidmem+vga.draw.address_add), *(vidmem+2-vga.draw.address_add), *(vidmem+2), *(vidmem+2+vga.draw.address_add), col?*(vidmem-2-vga.draw.address_add):0, col?*(vidmem-2):0, col?*(vidmem-2+vga.draw.address_add):0, col < cols-2?*(vidmem+4-vga.draw.address_add):0, col < cols-2?*(vidmem+4):0, col < cols-2?*(vidmem+4+vga.draw.address_add):0, row > 1?*(vidmem-2*vga.draw.address_add):0, row < rows-2?*(vidmem+2*vga.draw.address_add):0, row > 1?*(vidmem+2-2*vga.draw.address_add):0, row < rows-2?*(vidmem+2+2*vga.draw.address_add):0)) ||
                (row > 1 && CheckBoxDrawingV(*(vidmem-2*vga.draw.address_add), *(vidmem-vga.draw.address_add), chr, *(vidmem+2-2*vga.draw.address_add), *(vidmem+2-vga.draw.address_add), *(vidmem+2), col?*(vidmem-2-2*vga.draw.address_add):0, col?*(vidmem-2-vga.draw.address_add):0, col?*(vidmem-2):0, col < cols-2?*(vidmem+4-2*vga.draw.address_add):0, col < cols-2?*(vidmem+4-vga.draw.address_add):0, col?*(vidmem+4):0, row > 2?*(vidmem-3*vga.draw.address_add):0, row < rows-1?*(vidmem+vga.draw.address_add):0, row > 2?*(vidmem+2-3*vga.draw.address_add):0, row < rows-1?*(vidmem+2+vga.draw.address_add):0))
                ))) {
                    chr_left=chr;
                    chr_wide=true;
                    blocks++;
                } else {
                    Bitu font = dos.loaded_codepage==932
#if defined(USE_TTF)
                    && halfwidthkana
#endif
                    ? (IS_JEGA_ARCH ? jfont_sbcs_19[chr*19+line] : jfont_sbcs_16[chr*16+line]) : int10_font_16[chr*16+line];
                    if (line == 1) dbox.push_back(std::make_pair(row, col));
                    if (vga.draw.char9dot) {
                        font <<=1; // 9 pixels
                        // extend to the 9th pixel if needed
                        if ((font&0x2) && (vga.attr.mode_control&0x04) &&
                            (chr>=0xc0) && (chr<=0xdf)) font |= 1;
                    }
                    int width = vga.draw.char9dot ? 9 : 8;
                    for (Bitu n = 0; n < width; n++) {
                        if (card == MCH_VGA)
                            *draw++ = vga.dac.xlat32[(font & (vga.draw.char9dot?0x100:0x80))? foreground : background];
                        else
                            *draw++ = vga.attr.palette[(font & (vga.draw.char9dot?0x100:0x80)) ? foreground : background];
                        font <<= 1;
                    }
                    if (bsattr & 0x20) {
                        draw -= 8;
                        if (card == MCH_VGA)
                            *draw = vga.dac.xlat32[foreground];
                        else
                            *draw = vga.attr.palette[foreground];
                        draw += 8;
                    }
                    if (line == 18 && (bsattr & 0x10)) {
                        draw -= 8;
                        for (Bitu n = 0; n < 8; n++)
                            if (card == MCH_VGA)
                                *draw++ = vga.dac.xlat32[foreground];
                            else
                                *draw++ = vga.attr.palette[foreground];
                    }
                    chr_wide=false;
                }
            }
            else
            {
                Bitu pad_y = jega.RPSSC;
                Bitu exattr = 0;
                if (isJEGAEnabled() && (jega.RMOD2 & 0x40)) {
                    exattr = attr;
                    if ((exattr & 0x30) == 0x30) pad_y = jega.RPSSL;
                    else if (exattr & 0x30) pad_y = jega.RPSSU;
                }
                if (line >= pad_y && line < 16 + pad_y) {
                    if (isKanji2(chr)) {
                        if (line == 1) jtbs.push_back(std::make_pair(row, col));
                        Bitu fline = line - pad_y;
                        chr_left <<= 8;
                        chr |= chr_left;
                        if (exattr & 0x20) {
                            if (exattr & 0x10) fline = (fline >> 1) + 8;
                            else fline = fline >> 1;
                        }
                        bool is14 = false;
                        uint8_t *f = IS_JEGA_ARCH || card == MCH_VGA ? GetDbcsFont(chr) : GetDbcs14Font(chr, is14);
                        if (exattr & 0x40) {
                            Bitu font = f[fline * 2];
                            if (!(exattr & 0x08))
                                font = f[fline * 2 + 1];
                            if (vga.draw.char9dot) {
                                font <<=1; // 9 pixels
                                // extend to the 9th pixel if needed
                                if ((font&0x2) && (vga.attr.mode_control&0x04) &&
                                    (chr>=0xc0) && (chr<=0xdf)) font |= 1;
                            }
                            int width = vga.draw.char9dot ? 9 : 8;
                            for (Bitu n = 0; n < width; n++) {
                                if (card == MCH_VGA) {
                                    *draw++ = vga.dac.xlat32[(font & (vga.draw.char9dot?0x100:0x80)) ? foreground : background];
                                    *draw++ = vga.dac.xlat32[(font & (vga.draw.char9dot?0x100:0x80)) ? foreground : background];
                                } else {
                                    *draw++ = vga.attr.palette[(font & (vga.draw.char9dot?0x100:0x80)) ? foreground : background];
                                    *draw++ = vga.attr.palette[(font & (vga.draw.char9dot?0x100:0x80)) ? foreground : background];
                                }
                                font <<= 1;
                            }
                        } else {
                            Bitu font = f[fline * 2];
                            font <<= 8;
                            font |= f[fline * 2 + 1];
                            if (exattr &= 0x80)
                            {
                                Bitu font2 = font;
                                font2 >>= 1;
                                font |= font2;
                            }
                            if (vga.draw.char9dot) {
                                font <<=1; // 9 pixels
                                // extend to the 9th pixel if needed
                                if ((font&0x2) && (vga.attr.mode_control&0x04) &&
                                    (chr>=0xc0) && (chr<=0xdf)) font |= 1;
                            }
                            int width = vga.draw.char9dot ? 9 : 8;
                            for (Bitu n = 0; n < width * 2; n++) {
                                if (card == MCH_VGA)
                                    *draw++ = vga.dac.xlat32[(font & (vga.draw.char9dot?0x10000:0x8000))? foreground : background];
                                else
                                    *draw++ = vga.attr.palette[(font & (vga.draw.char9dot?0x10000:0x8000)) ? foreground : background];
                                font <<= 1;
                            }
                        }
                    } else for (Bitu n = 0; n < 16; n++) {
                        if (card == MCH_VGA)
                            *draw++ = vga.dac.xlat32[background];
                        else
                            *draw++ = vga.attr.palette[background];
                    }
                } else if (line == (17 + pad_y) && (bsattr & 0x10)) {
                        for (Bitu n = 0; n < 16; n++) {
                            if (card == MCH_VGA)
                                *draw++ = vga.dac.xlat32[foreground];
                            else
                                *draw++ = vga.attr.palette[foreground];
                        }
                } else for (Bitu n = 0; n < 16; n++) {
                    if (card == MCH_VGA)
                        *draw++ = vga.dac.xlat32[background];
                    else
                        *draw++ = vga.attr.palette[background];
                }
                if (bsattr & 0x20) {
                    draw -= 16;
                    if (card == MCH_VGA)
                        *draw = vga.dac.xlat32[foreground];
                    else
                        *draw = vga.attr.palette[foreground];
                    draw += 16;
                }
                chr_wide=false;
                blocks--;
            }
            pixelp2.d = *(vidmem - 2 * ((uintptr_t)1U << (uintptr_t)vga.config.addr_shift));
            p3 = col>2?pixelp2.b[0]:0;
            col++;
        } else { // SBCS case
            VGA_Latch pixels;

            pixels.d = *vidmem;

            Bitu chr = pixels.b[0];
            Bitu attr = pixels.b[1];
            // the font pattern
            Bitu font = vga.draw.font_tables[(attr >> 3)&1][(chr<<5)+line];
            Bitu background = attr >> 4u;
            // if blinking is enabled bit7 is not mapped to attributes
            if (vga.draw.blinking) background &= ~0x8u;
            // choose foreground color if blinking not set for this cell or blink on
            Bitu foreground = (vga.draw.blink || (!(attr&0x80)))?
                (attr&0xf):background;
            // underline: all foreground [freevga: 0x77, previous 0x7]
            if (GCC_UNLIKELY(((attr&0x77) == 0x01) &&
                (vga.crtc.underline_location&0x1f)==line))
                    background = foreground;
            if (vga.draw.char9dot) {
                font <<=1; // 9 pixels
                // extend to the 9th pixel if needed
                if ((font&0x2) && (vga.attr.mode_control&0x04) &&
                    (chr>=0xc0) && (chr<=0xdf)) font |= 1;
                for (Bitu n = 0; n < 9; n++) {
                    if (card == MCH_VGA)
                        *draw++ = vga.dac.xlat32[(font&0x100)? foreground:background];
                    else /*MCH_EGA*/
                        *draw++ = vga.attr.palette[(font&0x100)? foreground:background];

                    font <<= 1;
                }
            } else {
                for (Bitu n = 0; n < 8; n++) {
                    if (card == MCH_VGA)
                        *draw++ = vga.dac.xlat32[(font&0x80)? foreground:background];
                    else /*MCH_EGA*/
                        *draw++ = vga.attr.palette[(font&0x80)? foreground:background];

                    font <<= 1;
                }
            }
        }
        vidmem += (uintptr_t)1U << (uintptr_t)vga.config.addr_shift;
    }

    // draw the text mode cursor if needed
    if ((vga.draw.cursor.count&0x8) && (line >= vga.draw.cursor.sline) && (line <= vga.draw.cursor.eline) && vga.draw.cursor.enabled) {
        // the address of the attribute that makes up the cell the cursor is in
        Bits attr_addr = ((Bits)vga.draw.cursor.address - (Bits)vidstart) >> (Bits)vga.config.addr_shift; /* <- FIXME: This right? */
        if (attr_addr >= 0 && attr_addr < (Bits)vga.draw.blocks) {
            Bitu index = (Bitu)attr_addr * (vga.draw.char9dot ? 9u : 8u);
            draw = (((templine_type_t*)TempLine) + index) + 16 - vga.draw.panning;
            
            Bitu foreground = vga.tandy.draw_base[(vga.draw.cursor.address<<2ul)+1] & 0xf;
            for (Bitu i = 0; i < 8; i++) {
                if (card == MCH_VGA)
                    *draw++ = vga.dac.xlat32[foreground];
                else /*MCH_EGA*/
                    *draw++ = vga.attr.palette[foreground];
            }
        }
    }

    return TempLine+(16*sizeof(templine_type_t));
}

// combined 8/9-dot wide text mode 16bpp line drawing function
static uint8_t* EGA_TEXT_Xlat8_Draw_Line(Bitu vidstart, Bitu line) {
    return EGAVGA_TEXT_Combined_Draw_Line<MCH_EGA,uint8_t>(vidstart,line);
}

// combined 8/9-dot wide text mode 16bpp line drawing function
static uint8_t* VGA_TEXT_Xlat32_Draw_Line(Bitu vidstart, Bitu line) {
    return EGAVGA_TEXT_Combined_Draw_Line<MCH_VGA,uint32_t>(vidstart,line);
}

extern bool pc98_attr4_graphic;
extern uint8_t GDC_display_plane;
extern uint8_t GDC_display_plane_pending;
extern bool pc98_graphics_hide_odd_raster_200line;
extern bool pc98_allow_scanline_effect;

unsigned char       pc98_text_first_row_scanline_start = 0x00;  /* port 70h */
unsigned char       pc98_text_first_row_scanline_end = 0x0F;    /* port 72h */
unsigned char       pc98_text_row_scanline_blank_at = 0x10;     /* port 74h */
unsigned char       pc98_text_row_scroll_lines = 0x00;          /* port 76h */
unsigned char       pc98_text_row_scroll_count_start = 0x00;    /* port 78h */
unsigned char       pc98_text_row_scroll_num_lines = 0x00;      /* port 7Ah */

// Text layer rendering state.
// Track row, row count, scanline row within the cell,
// for accurate emulation
struct Text_Draw_State {
    unsigned int        scroll_vmem;
    unsigned char       scroll_scanline_cg;
    unsigned char       row_scanline_cg;            /* scanline within row, CG bitmap */
    unsigned char       row_char;                   /* character row. scroll region 0 <= num_lines */
    unsigned char       row_scroll_countdown;

    void begin_frame(void) {
        row_scroll_countdown = 0xFF;
        row_scanline_cg = pc98_text_first_row_scanline_start;
        row_char = pc98_text_row_scroll_count_start & 0x1Fu;
        check_scroll_region();

        if (row_scroll_countdown != 0xFF)
            update_scroll_line();
    }
    void next_line(void) {
        if (row_scanline_cg == pc98_text_first_row_scanline_end) {
            row_scanline_cg = pc98_text_first_row_scanline_start;
            next_character_row();
        }
        else {
            row_scanline_cg = (row_scanline_cg + 1u) & 0x1Fu;
        }

        if (row_scroll_countdown != 0xFF)
            update_scroll_line();
    }
    void update_scroll_line(void) {
        scroll_vmem = 0;
        scroll_scanline_cg = row_scanline_cg;
        for (unsigned int i=0;i < pc98_text_row_scroll_lines;i++) {
            if (scroll_scanline_cg == pc98_text_first_row_scanline_end) {
                scroll_scanline_cg = pc98_text_first_row_scanline_start;
                scroll_vmem += pc98_gdc[GDC_MASTER].display_pitch;
            }
            else {
                scroll_scanline_cg = (scroll_scanline_cg + 1u) & 0x1Fu;
            }
        }
    }
    void next_character_row(void) {
        row_char = (row_char + 1u) & 0x1Fu;
        check_scroll_region();
    }
    void check_scroll_region(void) {
        if (row_char == 0) {
            /* begin scroll region */
            /* NTS: Confirmed on real hardware: The scroll count ADDs to the vertical offset already applied to the text.
             *      For example in 20-line text mode (20 pixels high) setting the scroll region offset to 2 pixels cancels
             *      out the 2 pixel centering of the text. */
            row_scroll_countdown = pc98_text_row_scroll_num_lines & 0x1Fu;
        }
        else if (row_scroll_countdown == 0) {
            /* end scroll region */
            row_scroll_countdown = 0xFF;
        }
        else if (row_scroll_countdown != 0xFF) {
            row_scroll_countdown--;
        }
    }
    bool in_scroll_region(void) const {
        return row_scroll_countdown != 0xFFu;
    }
};

Text_Draw_State     pc98_text_draw;

/* NEC PC-9821Lt2 memory layout notes:
 *
 * - At first glance, the 8/16 color modes appear to be a sequence of bytes that are read
 *   in parallel to form 8 or 16 colors.
 * - Switching on 256-color mode disables the planar memory mapping, and appears to reveal
 *   how the planar memory is ACTUALLY laid out.
 * - Much like how VGA planar memory could be thought of as 32 bits per unit, 8 bits per plane,
 *   PC-98 planar memory seems to behave as 64 bits per unit, 16 bits per plane.
 * - 256-color mode seems to reveal that the bitplanes exposed at A800, B000, B800, E000
 *   are in fact just interleaved WORDS in video memory in BGRE order, meaning that the
 *   first 4 WORDs visible in 256-color mode are the same as, in 16-color mode, the first
 *   word of (in this order) A800, B800, B000, E000. The traditional E/G/R/B
 *   (E000, B800, B000, A800) planar order is actually stored in memory in BGRE order.
 *
 *   Example:
 *
 *      1. Enable 16-color mode
 *      2. Turn OFF 256-color mode
 *      3. Write 0x11 0x22 to A800:0000
 *      4. Write 0x33 0x44 to B000:0000
 *      5. Write 0x55 0x66 to B800:0000
 *      6. Write 0x77 0x88 to E000:0000
 *      7. Turn ON 256-color mode. Memory map will change.
 *      8. Observe at A800:0000 the values 0x11 0x22 0x55 0x66 0x33 0x44 0x77 0x88
 *
 * Also, if you use I/O port A6h to write to page 1 instead of page 0, what you write will
 * appear in 256-color mode at SVGA memory bank 4 (offset 128KB) instead of memory bank 0
 * (offset 0KB). Considering 4 planes * 32KB = 128KB this makes sense.
 *
 * So either the video memory is planar in design, and the 256-color mode is just the bitplanes
 * "chained" together (same as 256-color VGA mode), OR, the 256-color mode reflects how memory
 * is actually laid out and the planar memory is just emulated by packing each bitplane's WORDs
 * together like that.
 *
 * Proper emulation will require determining which is true and rewriting the draw and memory
 * access code to reflect the true layout so mode changes behave in DOSBox-X exactly as they
 * behave on real hardware.
 *
 * I have a hunch it's the case of 4 16-bit bitplanes shifted a byte at a time since doing that
 * allows 256-color mode without having to reprogram any other parameters of the GDC or change
 * anything significant about the hardware.
 *
 * Think about it: VGA has planar memory, and each 8-bit byte is shifted out one bit at a time
 * in parallel to produce a 4-bit value for each pixel, but VGA also permits the hardware to
 * switch to shifting out each planar byte instead to produce a 256-color packed mode.
 *
 * As noted elsewhere in this source code, VGA memory is 4 planes tied together, even in standard
 * 256-color mode where the planes are chained together to give the CPU the impression of a linear
 * packed framebuffer.
 *
 * Since PC-98 also has planar memory, it wouldn't surprise me if the 256-color mode is just the
 * same trick except with 16-bit quantitites loaded from memory instead of VGA's 8-bit quantities.
 *
 * The behavior of the hardware suggest to me that it also allowed NEC to change as little about
 * the video hardware as possible, except how it shifts and interprets the planar word data.
 *
 * The distorted screen that the PC-98 version of Windows 3.1 presents if you select the 640x400
 * 256-color driver seems to confirm my theory, along with the fact that you can apparently use
 * EGC ROPs in 256-color mode.
 *
 * However it's very likely the few PC-98 games that use the 256-color mode only care about the
 * mode as it exists, and that they don't care about what the prior contents of video memory look
 * like, so this isn't a problem in running the games, only a minor problem in emulation accuracy.
 *
 * Very likely, the same as IBM PC games that set up VGA unchained modes and do not necessarily
 * care what happens to the display of prior video memory contents (except of course some lazy
 * written code in the Demoscene that switches freely between the two modes).
 *
 * Please note this behavior so far has been noted from one PC-9821 laptop. It may be consistent
 * across other models I have available for testing, or it may not. --J.C.
 */

extern uint8_t              pc98_pal_digital[8];    /* G R B    0x0..0x7 */
extern bool                 pc98_256kb_boundary;
extern bool                 gdc_5mhz_mode;

static uint8_t* VGA_PC98_Xlat32_Draw_Line(Bitu vidstart, Bitu line) {
    // keep it aligned:
    uint32_t* draw = ((uint32_t*)TempLine);
    Bitu blocks = vga.draw.blocks;
    uint32_t vidmem = vidstart;
    uint16_t chr = 0,attr = 0;
    uint16_t lineoverlay = 0; // vertical + underline overlay over the character cell, but apparently with a 4-pixel delay
    bool doublewide = false;
    unsigned int disp_off = 0;
    unsigned char font,foreground;
    unsigned char fline;
    bool ok_raster = true;
    bool monoproc = false;

    // simulate 1-pixel shift in CRT mode by offsetting everything by 1 then rendering text without the offset
    disp_off = pc98_crt_mode ? 1 : 0;

    // 200-line modes: The BIOS or DOS game can elect to hide odd raster lines
    // NTS: Doublescan seems to be ignored in 256-color mode, thus the HACK! below, according to real hardware.
    if (pc98_gdc[GDC_SLAVE].doublescan && pc98_graphics_hide_odd_raster_200line && pc98_allow_scanline_effect &&
        /*HACK!*/(pc98_gdc_vramop & (1u << VOPBIT_VGA)) == 0)
        ok_raster = (vga.draw.lines_done & 1) == 0;

    // Generally the master and slave GDC are given the same active display area, timing, etc.
    // however some games reprogram the slave (graphics) GDC to reduce the active display area.
    //
    // Without this consideration, graphics display will be incorrect relative to actual hardware.
    //
    // This will NOT cause correct display if other parameters like blanking area are changed!
    //
    // Examples:
    //  - "First Queen" and "First Queen II" (reduces active lines count to 384 to display status bar at the bottom of the screen)
    if (vga.draw.lines_done >= pc98_gdc[GDC_SLAVE].active_display_lines)
        ok_raster = false;

    // Graphic RAM layer (or blank)
    // Think of it as a 3-plane GRB color graphics mode, each plane is 1 bit per pixel.
    // G-RAM is addressed 16 bits per RAM cycle.
    if (pc98_gdc[GDC_SLAVE].display_enable && ok_raster && pc98_display_enable) {
        draw = ((uint32_t*)TempLine) + disp_off;
        blocks = vga.draw.blocks;

        if (pc98_gdc_vramop & (1 << VOPBIT_VGA)) {
            /* WARNING: This code ASSUMES the port A4h page flip emulation will always
             *          set current_display_page to the same base graphics memory address
             *          when the 256KB boundary is enabled! If that assumption is WRONG,
             *          this code will read 256KB past the end of the buffer and possibly
             *          segfault. */
            const unsigned long vmask = pc98_256kb_boundary ? 0x7FFFFu : 0x3FFFFu;

            vidmem = (unsigned int)pc98_gdc[GDC_SLAVE].scan_address << (1u+3u); /* as if reading across bitplanes */

            while (blocks--) {
                const unsigned char *s = (const unsigned char*)(&pc98_pgraph_current_display_page[vidmem & vmask]);
                for (unsigned char i=0;i < 8;i++) *draw++ = vga.dac.xlat32[*s++];

                vidmem += 8;
            }
        }
        else if (pc98_monochrome_mode && (pc98_gdc_vramop & (1 << VOPBIT_ANALOG)) == 0/*8-color digital mode ONLY*/) {
            /* NTS: According to real hardware, monochrome reads all three bitplanes (8-color mode), maps through
             *      the digital palette, then uses only bit 2 as a monochrome bitplane.
             *
             *      According to Nanshiki, though not yet confirmed (2022/05/26), the monochrome planar graphics
             *      take on the color attribute of the text layer (test case MONO.C/MONO.EXE).
             *
             *      Monochrome mode is said to be a carry-over from PC-88 as a backwards compatible graphics
             *      display mode, and later removed in later hardware. Likely at a time when Windows 95/98 was
             *      becoming the dominant mode of operation.
             *
             *      As a possibly unsupported/unintended mode, monochrome mode can be enabled in 16-color analog
             *      graphics mode, in which case it behaves like a 4-bit digital graphics mode, uses the 8-color
             *      palette, and the pixel is set if the combined 4-bit value is (v & 0xC) == 0x8. Setting the
             *      corresponding bit in plane 3 cancels out bit 2, apparently. As seen on a 486 PC9821 laptop.
             *      Perhaps this unintended mode differs on later/earlier hardware.
             *
             *      To avoid cluttering this code, DOSBox-X will not render monochrome mode if the 16-color
             *      analog mode is active. */
            /* NOTICE: Copy-paste of 8/16-color planar mode below */
            const unsigned long vmask = 0x7FFFu;

            vidmem = (unsigned int)pc98_gdc[GDC_SLAVE].scan_address << 1u;

            while (blocks--) {
                /* In monochrome mode, the 3-bit value is read just like color mode, then remapped through the
                 * digital palette, then only bit 2 (normally green) is used on the display. */
                uint8_t g8 = pc98_pgraph_current_display_page[(vidmem & vmask) + pc98_pgram_bitplane_offset(2)];      /* B8000-BFFFF */
                uint8_t r8 = pc98_pgraph_current_display_page[(vidmem & vmask) + pc98_pgram_bitplane_offset(1)];      /* B0000-B7FFF */
                uint8_t b8 = pc98_pgraph_current_display_page[(vidmem & vmask) + pc98_pgram_bitplane_offset(0)];      /* A8000-AFFFF */

                for (unsigned char i=0;i < 8;i++) {
                    foreground  = (g8 & 0x80) ? 4 : 0;
                    foreground += (r8 & 0x80) ? 2 : 0;
                    foreground += (b8 & 0x80) ? 1 : 0;

                    g8 <<= 1;
                    r8 <<= 1;
                    b8 <<= 1;

		    /* bit 2 becomes the pixel, map to black or white and let the text layer below color it */
                    *draw++ = (pc98_pal_digital[foreground] & 4) ? 0xFFFFFFFF : 0x00000000;
                }

                vidmem++;
            }

	    /* the text layer needs to color our output */
            monoproc = true;
        }
        else {
            /* NTS: According to real hardware, the 128KB/256KB boundary control bit ONLY works in 256-color mode.
             *      It has no effect in 8/16-color planar modes, which is probably why the BIOS on such systems
             *      will not allow a 640x480 16-color mode since the VRAM required exceeds 32KB per bitplane. */
            const unsigned long vmask = 0x7FFFu;

            vidmem = (unsigned int)pc98_gdc[GDC_SLAVE].scan_address << 1u;

            while (blocks--) {
                // NTS: Testing on real hardware shows that, when you switch the GDC back to 8-color mode,
                //      the 4th bitplane is no longer rendered.
                uint8_t e8;

                if (gdc_analog)
                    e8 = pc98_pgraph_current_display_page[(vidmem & vmask) + pc98_pgram_bitplane_offset(3)];  /* E0000-E7FFF */
                else
                    e8 = 0x00;

                uint8_t g8 = pc98_pgraph_current_display_page[(vidmem & vmask) + pc98_pgram_bitplane_offset(2)];      /* B8000-BFFFF */
                uint8_t r8 = pc98_pgraph_current_display_page[(vidmem & vmask) + pc98_pgram_bitplane_offset(1)];      /* B0000-B7FFF */
                uint8_t b8 = pc98_pgraph_current_display_page[(vidmem & vmask) + pc98_pgram_bitplane_offset(0)];      /* A8000-AFFFF */

                for (unsigned char i=0;i < 8;i++) {
                    foreground  = (e8 & 0x80) ? 8 : 0;
                    foreground += (g8 & 0x80) ? 4 : 0;
                    foreground += (r8 & 0x80) ? 2 : 0;
                    foreground += (b8 & 0x80) ? 1 : 0;

                    e8 <<= 1;
                    g8 <<= 1;
                    r8 <<= 1;
                    b8 <<= 1;

                    *draw++ = vga.dac.xlat32[foreground];
                }

                vidmem++;
            }
        }
    }
    else {
        memset(TempLine,0,4 * 8 * blocks);
    }

    // Text RAM layer
    if (pc98_gdc[GDC_MASTER].display_enable && pc98_display_enable) {
        Bitu gdcvidmem = pc98_gdc[GDC_MASTER].scan_address;

        draw = ((uint32_t*)TempLine);/* without the disp_off, to emulate 1-pixel cutoff in CRT mode */
        blocks = vga.draw.blocks;

        vidmem = pc98_gdc[GDC_MASTER].scan_address;
        if (pc98_text_draw.in_scroll_region()) {
            vidmem += pc98_text_draw.scroll_vmem;
            fline = pc98_text_draw.scroll_scanline_cg;
        }
        else {
            fline = pc98_text_draw.row_scanline_cg;
        }

        /* NTS: This code will render the cursor in the scroll region with a weird artifact
         *      when the character under it is double-wide, and the cursor covers the top half.
         *
         *      For example:
         *
         *      ＡＡＡ
         *      ＡＡＡ
         *      ＡＡＡＡ     <- scroll region ends here
         *      ＡＡＡＡ
         *
         *      Placing the cursor on the third row of Ａ's, on the right, will produce an
         *      oddly shaped cursor.
         *
         *      Before filing a bug about this, consider from my testing that real PC-9821
         *      hardware will also render a strangely shaped cursor there as well. --J.C. */

        while (blocks--) { // for each character in the line
            bool was_doublewide = doublewide;

            /* Amusing question: How does it handle the "simple graphics" in 20-line mode? */

            if (!doublewide) {
interrupted_char_begin:
                chr = ((uint16_t*)vga.mem.linear)[(vidmem & 0xFFFU) + 0x0000U];
                attr = ((uint16_t*)vga.mem.linear)[(vidmem & 0xFFFU) + 0x1000U];

                if (pc98_attr4_graphic && (attr & 0x10)) {
                    /* the "vertical line" attribute (bit 4) can be redefined as a signal
                     * to interpret the character as a low res bitmap instead compatible with
                     * "simple graphics" of the PC-8001 (ref. Carat) */
                    /* Contrary to what you normally expect of a "bitmap", the pixels are
                     * in column order.
                     *     0 1
                     *     col
                     *     0 4  r 0
                     *     1 5  o 1
                     *     2 6  w 2
                     *     3 7    3
                     */
                    /* The only way a direct bitmap can be encoded in 8 bits is if one character
                     * cell were 2 pixels wide 4 pixels high, and each pixel was repeated 4 times.
                     * In case you're wondering, the high byte doesn't appear to be used in this mode.
                     *
                     * Setting the high byte seems to blank the cell entirely */
                    if ((chr & 0xFF00) == 0x00) {
                        unsigned char bits2 = (chr >> (pc98_gdc[GDC_MASTER].row_line >> 2)) & 0x11;

                        font =  ((bits2 & 0x01) ? 0xF0 : 0x00) +
                                ((bits2 & 0x10) ? 0x0F : 0x00);
                    }
                    else {
                        font = 0;
                    }
                }
                else {
                    // NTS: The display handles single-wide vs double-wide by whether or not the 8 bits are nonzero.
                    //      If zero, the char is one cell wide.
                    //      If nonzero, the char is two cells wide (doublewide) and the current character is rendered
                    //      into both cells (the character code in the next cell is ignored). The attribute (as far
                    //      as I know) repeats for both.
                    //
                    //      NTS: It seems different character ROM is used between single and double wide chars.
                    //           Contrary to what this suggests, (chr & 0xFF00) == 0x8000 is doublewide but not the
                    //           same as single-wide (chr & 0xFF00) == 0x0000.
                    //
                    //      Specific ranges that would be fullwidth where bits[6:0] are 0x08 to 0x0B inclusive are
                    //      apparently not fullwidth (the halfwidth char repeats) if both cells filled in.
                    if ((chr & 0xFF00) != 0 && (chr & 0x7CU) != 0x08) {
                        // left half of doublewide char. it appears only bits[14:8] and bits[6:0] have any real effect on which char is displayed.
                        doublewide = true;
                    }

                    if (fline < pc98_text_row_scanline_blank_at)
                        font = pc98_font_char_read(chr,fline,0);
                    else
                        font = 0;
                }
            }
            else {
                // right half of doublewide char.
                //
                // NTS: Strange idiosyncratic behavior observed on real hardware shows that MOST fullwidth codes
                //      fill two cells and ignore the other cell, EXCEPT, that specific ranges require you to
                //      enter the same fullwidth code in both cells.
                doublewide = false;

                // It seems that for any fullwidth char, you need the same code in both cells for bit[6:0] values
                // from 0x08 to 0x0F inclusive. 0x08 to 0x0B inclusive are not fullwidth, apparently.
                // Same applies 0x56 to 0x5F.
                //
                // Real hardware seems to show that this code will show the other half of the character IF the
                // character code matches. If it does not match, then it will show the first half of the new code.
                //
                // This fix is needed for Touhou Project to show some level titles correctly. The reason this fix
                // affects it, is that the text RAM covering the playfield is not space or any traditionally empty
                // cell but a custom character code that is generally empty, but the character cell bitmap is animated
                // (changed per frame) when doing fade/wipe transitions between levels. Some of the level titles
                // are displayed starting at an odd column cell number, which means that the Kanji intended for
                // display "interrupts" the blank custom character cell code. TH02 ~idnight bug fix.
                if ((chr&0x78U) == 0x08 || (chr&0x7FU) >= 0x56) {
                    uint16_t n_chr;

                    n_chr = ((uint16_t*)vga.mem.linear)[(vidmem & 0xFFFU) + 0x0000U];
                    attr = ((uint16_t*)vga.mem.linear)[(vidmem & 0xFFFU) + 0x1000U];

                    if ((chr&0x7F7F) != (n_chr&0x7F7F))
                        goto interrupted_char_begin;
                }

                if (fline < pc98_text_row_scanline_blank_at)
                    font = pc98_font_char_read(chr,fline,1);
                else
                    font = 0;
            }

            lineoverlay <<= 8;

            /* the character is not rendered if "~secret" (bit 0) is not set */
            if (!(attr & 1)) font = 0;

            /* "blink" seems to count at the same speed as the cursor blink rate,
             * through a 4-cycle pattern in which the character is invisible only
             * at the first count. */
            if ((attr & 0x02/*blink*/) && pc98_gdc[GDC_MASTER].cursor_blink_state == 0) font = 0;

            /* reverse attribute. seems to take effect BEFORE vertical & underline attributes */
            if (attr & 0x04/*reverse*/) font ^= 0xFF;

            /* based on real hardware, the cursor seems to act like a reverse attribute */
            /* if the character is double-wide, and the cursor is on the left half, the cursor affects the right half too. */
            if (((gdcvidmem == vga.draw.cursor.address) || (was_doublewide && gdcvidmem == (vga.draw.cursor.address+1))) &&
                pc98_gdc[GDC_MASTER].cursor_enable &&
                ((!pc98_gdc[GDC_MASTER].cursor_blink) || (pc98_gdc[GDC_MASTER].cursor_blink_state&1)) &&
                (line >= vga.draw.cursor.sline) &&
                (line <= vga.draw.cursor.eline)) {
                font ^= 0xFF;
            }

            /* "vertical line" bit puts a vertical line on the 4th pixel of the cell */
            if (!pc98_attr4_graphic && (attr & 0x10)) lineoverlay |= 1U << 7U;

            /* underline fills the row to underline the text */
            if ((attr & 0x08) && line == (vga.crtc.maximum_scan_line & 0x1FU)) lineoverlay |= 0xFFU;

            /* lineoverlay overlays font with 4-pixel delay */
            font |= (unsigned char)(((unsigned int)lineoverlay >> 4u) & 0xFFU);

            /* color? */
            foreground = ((unsigned int)attr >> 5u) & 7u; /* bits[7:5] are GRB foreground color */

            /* draw it!
             * NTS: Based on real hardware (and this is probably why there's no provisions for both fore and background color)
             *      any bit in the font overlays the graphic output (after reverse, etc) or else does not output anything. */
            if (!pc98_40col_text) {
                /* 80-col */
                for (Bitu n = 0; n < 8; n++) {
                    if (font & 0x80)
                        *draw++ = pc98_text_palette[foreground];
                    else if (monoproc && *draw != 0) /* monochrome mode: text attribute colors graphics, therefore color if pixel set */
                        *draw++ = pc98_text_palette[foreground];
                    else
                        draw++;

                    font <<= 1u;
                }

                vidmem++;
                gdcvidmem++;
            }
            else {
                /* 40-col */
                for (Bitu n = 0; n < 8; n++) {
                    if (font & 0x80)
                        draw[0] = draw[1] = pc98_text_palette[foreground];
                    else if (monoproc) {
                        if (draw[0]) draw[0] = pc98_text_palette[foreground];
                        if (draw[1]) draw[1] = pc98_text_palette[foreground];
                    }

                    font <<= 1u;
                    draw += 2;
                }

                vidmem += 2;
                gdcvidmem += 2;
                if (blocks > 0) blocks--;
            }
        }
    }

    return TempLine + (disp_off * 4);
}

void VGA_DebugAddEvent_VGASplit(void);

static void VGA_ProcessSplit() {
    if (video_debug_overlay) VGA_DebugAddEvent_VGASplit();
    vga.draw.has_split = true;
    if (vga.attr.mode_control&0x20) {
        vga.draw.address=0;
        // reset panning to 0 here so we don't have to check for 
        // it in the character draw functions. It will be set back
        // to its proper value in v-retrace
        vga.draw.panning=0; 
    } else {
        // In text mode only the characters are shifted by panning, not the address;
        // this is done in the text line draw function. EGA/VGA planar is handled the same way.
        vga.draw.address = vga.draw.byte_panning_shift*vga.draw.bytes_skip;
        if (machine != MCH_EGA) {
            switch (vga.mode) {
                case M_PC98:
                case M_TEXT:
                case M_EGA:
                case M_LIN4:
                case M_PACKED4:
                    /* ignore */
                    break;
                default:
                    vga.draw.address += vga.draw.panning;
                    break;
            }
        }
    }
    vga.draw.address_line=0;
}

static uint8_t bg_color_index = 0; // screen-off black index
static uint8_t VGA_GetBlankedIndex() {
    if (vga.dac.xlat16[bg_color_index] != 0) {
        for(Bitu i = 0; i < 256; i++)
            if (vga.dac.xlat16[i] == 0) {
                bg_color_index = i;
                break;
            }
    }
    return bg_color_index;
}

/* this is now called PER LINE because most VGA cards do not double-buffer the value.
 * a few demos rely on line compare schenanigans to play with the raster, as does my own VGA test program --J.C. */
void VGA_Update_SplitLineCompare() {
    vga.draw.split_line = (vga.config.line_compare + 1) / vga.draw.render_max;

    if (svgaCard==SVGA_S3Trio) {
        /* FIXME: Is this really necessary? Is this what S3 chipsets do?
         *        What is supposed to happen is that line_compare == 0 on normal VGA will cause the first
         *        scanline to repeat twice. Do S3 chipsets NOT reproduce that quirk?
         *
         *        The other theory I have about whoever wrote this code is that they wanted to multiply
         *        the scan line by two but had to compensate for the line_compare+1 assignment above.
         *        Rather than end up with a splitscreen too far down, they did that.
         *
         *        I think the proper code to put here would be:
         *
         *        if (vga.s3.reg_42 & 0x20) {
         *            vga.draw.split_line--;
         *            vga.draw.split_line *= 2;
         *            vga.draw.split_line++;
         *        }
         *
         *        Is that right?
         *
         *        This behavior is the reason for Issue #40 "Flash productions "monstra" extra white line at top of screen"
         *        because it causes line compare to miss and the first scanline of the white bar appears on scanline 2,
         *        when the demo coder obviously intended for line_compare == 0 to repeat the first scanline twice so that
         *        on line 3, it can begin updating line compare to continue repeating the first scanline.
         *
         * TODO: Pull out some S3 graphics cards and check split line behavior when line_compare == 0 */
        if (vga.config.line_compare==0) vga.draw.split_line=0;
        if (vga.s3.reg_42 & 0x20) { // interlaced mode
            vga.draw.split_line *= 2;
        }
    }
    vga.draw.split_line -= vga.draw.vblank_skip;
}

void VGA_DAC_DeferredUpdateColorPalette();
void VGA_DebugAddEvent(debugline_event &ev);
void VGA_DrawDebugLine(uint8_t *line,unsigned int w);

static void VGA_DrawSingleLine(Bitu /*blah*/) {
    unsigned int lines = 0;
    bool skiprender;

    vga.draw.hsync_events++;

again:
    if (vga.draw.render_step == 0)
        skiprender = false;
    else
        skiprender = true;

    if ((++vga.draw.render_step) >= vga.draw.render_max)
        vga.draw.render_step = 0;

    if (!skiprender) {
        if (video_debug_overlay && machine == MCH_PC98) {
            for (unsigned int i=0;i < 2;i++) {
                if (pc98_gdc[i].dbg_ev_partition) {
                    debugline_event ev;
                    char name[20];

                    sprintf(name,"%cPART%u",i==1?'G':'T',pc98_gdc[i].display_partition);
                    ev.event = DBGEV_SPLIT;
                    ev.addline(name);

                    sprintf(name,"%04x",pc98_gdc[i].scan_address);
                    ev.addline(name);

                    VGA_DebugAddEvent(ev);

                    pc98_gdc[i].dbg_ev_partition = false;
                }
            }
        }

        VGA_DAC_DeferredUpdateColorPalette();
        if (GCC_UNLIKELY(vga.attr.disabled)) {
            switch(machine) {
                case MCH_PCJR:
                    // Displays the border color when screen is disabled
                    bg_color_index = vga.tandy.border_color;
                    break;
                case MCH_TANDY:
                    // Either the PCJr way or the CGA way
                    if (vga.tandy.gfx_control& 0x4) { 
                        bg_color_index = vga.tandy.border_color;
                    } else if (vga.mode==M_TANDY4) 
                        bg_color_index = vga.attr.palette[0];
                    else bg_color_index = 0;
                    break;
                case MCH_CGA:
                case MCH_MCGA:
                    // the background color
                    bg_color_index = vga.attr.overscan_color;
                    break;
                case MCH_EGA:
                case MCH_VGA:
                case MCH_PC98:
                    // DoWhackaDo, Alien Carnage, TV sports Football
                    // when disabled by attribute index bit 5:
                    //  ET3000, ET4000, Paradise display the border color
                    //  S3 displays the content of the currently selected attribute register
                    // when disabled by sequencer the screen is black "257th color"

                    // the DAC table may not match the bits of the overscan register
                    // so use black for this case too...
                    //if (vga.attr.disabled& 2) {
                    VGA_GetBlankedIndex();
                    //} else 
                    //    bg_color_index = vga.attr.overscan_color;
                    break;
                default:
                    bg_color_index = 0;
                    break;
            }
            if (vga.draw.bpp==8) {
                memset(TempLine, bg_color_index, sizeof(TempLine));
            } else if (vga.draw.bpp==16) {
                uint16_t* wptr = (uint16_t*) TempLine;
                uint16_t value = vga.dac.xlat16[bg_color_index];
                for (Bitu i = 0; i < sizeof(TempLine)/2; i++) {
                    wptr[i] = value;
                }
            } else if (vga.draw.bpp==32) {
                uint32_t* wptr = (uint32_t*) TempLine;
                uint32_t value = vga.dac.xlat32[bg_color_index];
                for (Bitu i = 0; i < sizeof(TempLine)/4; i++) {
                    wptr[i] = value;
                }
            }

            if (vga_page_flip_occurred) {
                memxor(TempLine,0xFF,vga.draw.width*(vga.draw.bpp>>3));
                vga_page_flip_occurred = false;
            }
            if (vga_3da_polled) {
                if (vga.draw.bpp==32)
                    memxor_greendotted_32bpp((uint32_t*)TempLine,(vga.draw.width>>1)*(vga.draw.bpp>>3),vga.draw.lines_done);
                else
                    memxor_greendotted_16bpp((uint16_t*)TempLine,(vga.draw.width>>1)*(vga.draw.bpp>>3),vga.draw.lines_done);
                vga_3da_polled = false;
            }
            RENDER_DrawLine(TempLine);
        } else {
            uint8_t * data=VGA_DrawLine( vga.draw.address, vga.draw.address_line );
            /* WARNING: For magic reasons possibly related to gremlins added by the GNU C++ compiler or other otherwordly phenomona,
             *          modifying the rendered scanline pointed to by *data somehow corrupts the video memory of the guest, even though
             *          *data is 8bpp or 32bpp pixel data that was translated FROM the guest video memory TO a host bitmap and writing
             *          over *data in any way should have no effect on the guest video memory it rendered from, but somehow, it does.
             *          No, it has nothing to do with whether the templated function the EGA/VGA text calls is inline or not.
             *
             *          Modifying TempLine directly, which is the SAME memory location pointed to by data, does not cause this effect.
             *          Why???
             *
             *          What the fuck? Clang/LLVM causes the same behavior too??
             *
             *          For this reason, this code never uses the data pointer, it requires an offset relative to TempLine to avoid this
             *          weird flaw. */
            if (video_debug_overlay && vga.draw.width < render.src.width) {
                if (data >= TempLine && data < (TempLine+(64*4))) {
                    VGA_DrawDebugLine(TempLine+size_t(data-TempLine)+(vga.draw.width*((vga.draw.bpp+7u)>>3u)),render.src.width-vga.draw.width);
                }
	    }
            if (vga_page_flip_occurred) {
                memxor(data,0xFF,vga.draw.width*(vga.draw.bpp>>3));
                vga_page_flip_occurred = false;
            }
            if (vga_3da_polled) {
                if (vga.draw.bpp==32)
                    memxor_greendotted_32bpp((uint32_t*)data,(vga.draw.width>>1)*(vga.draw.bpp>>3),vga.draw.lines_done);
                else
                    memxor_greendotted_16bpp((uint16_t*)data,(vga.draw.width>>1)*(vga.draw.bpp>>3),vga.draw.lines_done);
                vga_3da_polled = false;
            }

            if (VGA_IsCaptureEnabled())
                VGA_ProcessScanline(data);

            RENDER_DrawLine(data);
        }
    }

    vga.draw.address_line++;
    if (vga.draw.address_line>=vga.draw.address_line_total) {
        vga.draw.address_line=0;
        vga.draw.address+=vga.draw.address_add;
    }

    if (!skiprender) {
        vga.draw.lines_done++;
        if (vga.draw.split_line==vga.draw.lines_done && machine != MCH_PC98) VGA_ProcessSplit();
    }

    if (mcga_double_scan) {
        if (vga.draw.lines_done < vga.draw.lines_total) {
            if (++lines < 2)
                goto again;
        }
    }

    if (vga.draw.lines_done < vga.draw.lines_total) {
        if (!vga_render_on_demand)
            PIC_AddEvent(VGA_DrawSingleLine,vga.draw.delay.singleline_delay);
    } else {
        vga_mode_frames_since_time_base++;

        if (VGA_IsCaptureEnabled())
            VGA_ProcessScanline(NULL);

        RENDER_EndUpdate(false);
    }

    if (IS_PC98_ARCH) {
        for (unsigned int i=0;i < 2;i++)
            pc98_gdc[i].next_line();

        pc98_text_draw.next_line();
    }

    /* some VGA cards (ATI chipsets especially) do not double-buffer the
     * horizontal panning register. some DOS demos take advantage of that
     * to make the picture "waver".
     *
     * We stop doing this though if the attribute controller is setup to set hpel=0 at splitscreen.
     *
     * EGA allows per scanline hpel according to DOSBox SVN source code. */
    if ((IS_VGA_ARCH && vga_enable_hpel_effects) || (IS_EGA_ARCH && egavga_per_scanline_hpel)) {
        /* Attribute Mode Controller: If bit 5 (Pixel Panning Mode) is set, then upon line compare the bottom portion is displayed as if Pixel Shift Count and Byte Panning are set to 0.
         * This ensures some demos like Future Crew "Yo" display correctly instead of the bottom non-scroller half jittering because the top half is scrolling. */
        if (vga.draw.has_split && (vga.attr.mode_control&0x20))
            vga.draw.panning = 0;
        else
            vga.draw.panning = vga.config.pel_panning;
    }

    if (IS_EGAVGA_ARCH && !vga_double_buffered_line_compare) VGA_Update_SplitLineCompare();
}

static void VGA_DrawEGASingleLine(Bitu /*blah*/) {
    bool skiprender;

    if (vga.draw.render_step == 0)
        skiprender = false;
    else
        skiprender = true;

    if ((++vga.draw.render_step) >= vga.draw.render_max)
        vga.draw.render_step = 0;

    if (!skiprender) {
        if (GCC_UNLIKELY(vga.attr.disabled)) {
            memset(TempLine, 0, sizeof(TempLine));
            RENDER_DrawLine(TempLine);
        } else {
            Bitu address = vga.draw.address;
            if (machine != MCH_EGA) {
                switch (vga.mode) {
                    case M_PC98:
                    case M_TEXT:
                    case M_EGA:
                    case M_LIN4:
                    case M_PACKED4:
                        /* ignore */
                        break;
                    default:
                        vga.draw.address += vga.draw.panning;
                        break;
                }
            }
            uint8_t * data=VGA_DrawLine(address, vga.draw.address_line ); 
            if (video_debug_overlay && vga.draw.width < render.src.width) VGA_DrawDebugLine(data+(vga.draw.width*((vga.draw.bpp+7u)>>3u)),render.src.width-vga.draw.width);

            if (VGA_IsCaptureEnabled())
                VGA_ProcessScanline(data);

            RENDER_DrawLine(data);
        }
    }

    vga.draw.address_line++;
    if (vga.draw.address_line>=vga.draw.address_line_total) {
        vga.draw.address_line=0;
        vga.draw.address+=vga.draw.address_add;
    }

    if (!skiprender) {
        vga.draw.lines_done++;
        if (vga.draw.split_line==vga.draw.lines_done && machine != MCH_PC98) VGA_ProcessSplit();
    }

    if (vga.draw.lines_done < vga.draw.lines_total) {
        PIC_AddEvent(VGA_DrawEGASingleLine,vga.draw.delay.singleline_delay);
    } else {
        vga_mode_frames_since_time_base++;

        if (VGA_IsCaptureEnabled())
            VGA_ProcessScanline(NULL);

        RENDER_EndUpdate(false);
    }

    /* some VGA cards (ATI chipsets especially) do not double-buffer the
     * horizontal panning register. some DOS demos take advantage of that
     * to make the picture "waver".
     *
     * We stop doing this though if the attribute controller is setup to set hpel=0 at splitscreen.
     *
     * EGA allows per scanline hpel according to DOSBox SVN source code. */
    if ((IS_VGA_ARCH && vga_enable_hpel_effects) || (IS_EGA_ARCH && egavga_per_scanline_hpel)) {
        /* Attribute Mode Controller: If bit 5 (Pixel Panning Mode) is set, then upon line compare the bottom portion is displayed as if Pixel Shift Count and Byte Panning are set to 0.
         * This ensures some demos like Future Crew "Yo" display correctly instead of the bottom non-scroller half jittering because the top half is scrolling. */
        if (vga.draw.has_split && (vga.attr.mode_control&0x20))
            vga.draw.panning = 0;
        else
            vga.draw.panning = vga.config.pel_panning;
    }

    if (IS_EGAVGA_ARCH && !vga_double_buffered_line_compare) VGA_Update_SplitLineCompare();
}

void VGA_SetBlinking(Bitu enabled) {
    Bitu b;
    LOG(LOG_VGA,LOG_NORMAL)("Blinking %d",(int)enabled);
    if (enabled) {
        b=0;vga.draw.blinking=1; //used to -1 but blinking is unsigned
        vga.attr.mode_control|=0x08;
        vga.tandy.mode_control|=0x20;
    } else {
        b=8;vga.draw.blinking=0;
        vga.attr.mode_control&=~0x08;
        vga.tandy.mode_control&=~0x20;
    }
    for (Bitu i=0;i<8;i++) TXT_BG_Table[i+8]=(b+i) | ((b+i) << 8)| ((b+i) <<16) | ((b+i) << 24);
}

extern bool                        GDC_vsync_interrupt;

void VGA_RenderOnDemandUpTo(void) {
    /* dt calculation is designed to match PIC_AddEvent() calls for the same scanline by scanline rendering without the on demand rendering mode */
    const pic_tickindex_t dt = PIC_FullIndex() - vga.draw.delay.framestart;
    signed int scanline = (signed int)floor((double)(1.0 + ((dt - (vga.draw.delay.htotal/4.0)) / vga.draw.delay.singleline_delay)));
    int patience = 4096;

    if (scanline < 0) scanline = 0;
    while (vga.draw.lines_done < vga.draw.lines_total && vga.draw.hsync_events < (unsigned int)scanline && patience-- > 0)
        VGA_DrawSingleLine(0);
}

void VGA_RenderOnDemandComplete(void) {
    int patience = 4096;

    while (vga.draw.lines_done < vga.draw.lines_total && patience-- > 0)
        VGA_DrawSingleLine(0);
}

static void VGA_VertInterrupt(Bitu /*val*/) {
    VGA_RenderOnDemandComplete();
    if (IS_PC98_ARCH) {
        if (GDC_vsync_interrupt) {
            GDC_vsync_interrupt = false;
            PIC_ActivateIRQ(2);
        }
    }
    else {
        if ((!vga.draw.vret_triggered) && ((vga.crtc.vertical_retrace_end&0x30)==0x10)) {
            vga.draw.vret_triggered=true;
            if (GCC_UNLIKELY(machine==MCH_EGA)) PIC_ActivateIRQ(9);
        }
    }
}

static void VGA_Other_VertInterrupt(Bitu val) {
    VGA_RenderOnDemandComplete();
    if (val) PIC_ActivateIRQ(5);
    else PIC_DeActivateIRQ(5);
}

static void VGA_DisplayStartLatch(Bitu /*val*/) {
    VGA_RenderOnDemandComplete();

    /* hretrace fx support: store the hretrace value at start of picture so we have
     * a point of reference how far to displace the scanline when wavy effects are
     * made */
    vga_display_start_hretrace = vga.crtc.start_horizontal_retrace;
    vga.config.real_start=vga.config.display_start & vga.mem.memmask;
    vga.draw.bytes_skip = vga.config.bytes_skip;

    /* TODO: When does 640x480 2-color mode latch foreground/background colors from the DAC? */
    if (machine == MCH_MCGA && (vga.other.mcga_mode_control & 2)) {//640x480 2-color mode MCGA
        VGA_DAC_UpdateColorPalette();
    }
}
 
static void VGA_PanningLatch(Bitu /*val*/) {
    VGA_RenderOnDemandComplete();
    vga.draw.panning = vga.config.pel_panning;

    if (IS_PC98_ARCH) {
        for (unsigned int i=0;i < 2;i++)
            pc98_gdc[i].begin_frame();

        pc98_text_draw.begin_frame();
    }
}

extern SDL_Rect                            vga_capture_current_rect;
extern uint32_t                            vga_capture_current_address;
extern uint32_t                            vga_capture_write_address;

void VGA_ProcessScanline(const uint8_t *raw) {
    if (raw == NULL) { // end of the frame
        if (VGA_IsCaptureInProgress()) {
            VGA_MarkCaptureInProgress(false);
            VGA_MarkCaptureAcquired();
        }

        return;
    }

    // assume VGA_IsCaptureEnabled()
    if (!VGA_IsCaptureInProgress()) {
        if (vga_capture_current_address != (uint32_t)0 && (unsigned int)vga.draw.lines_done == (unsigned int)vga_capture_current_rect.y) { // start
            VGA_MarkCaptureInProgress(true);
            VGA_CaptureWriteScanline(raw);
        }
    }
    else {
        if ((unsigned int)vga.draw.lines_done == ((unsigned int)vga_capture_current_rect.y+vga_capture_current_rect.h)) { // first line past end
            VGA_MarkCaptureInProgress(false);
            VGA_MarkCaptureAcquired();
        }
        else {
            VGA_CaptureWriteScanline(raw);
        }
    }
}

extern uint32_t vga_capture_stride;

template <const unsigned int bpp,typename BPPT> uint32_t VGA_CaptureConvertPixel(const BPPT raw) {
    unsigned char r,g,b;

    /* FIXME: Someday this code will have to deal with 10:10:10 32-bit RGB.
     * Also the 32bpp case shows how hacky this codebase is with regard to 32bpp color order support */
    if (bpp == 32) {
        if (GFX_bpp >= 24) {
            r = ((uint32_t)raw & GFX_Rmask) >> (uint32_t)GFX_Rshift;
            g = ((uint32_t)raw & GFX_Gmask) >> (uint32_t)GFX_Gshift;
            b = ((uint32_t)raw & GFX_Bmask) >> (uint32_t)GFX_Bshift;
        }
        else {
            // hack alt, see vga_dac.cpp
            return raw;
        }
    }
    else if (bpp == 16) {
        /* 5:5:5 or 5:6:5 */
        r = ((uint16_t)raw & (uint16_t)GFX_Rmask) >> (uint16_t)GFX_Rshift;
        g = ((uint16_t)raw & (uint16_t)GFX_Gmask) >> (uint16_t)GFX_Gshift;
        b = ((uint16_t)raw & (uint16_t)GFX_Bmask) >> (uint16_t)GFX_Bshift;

        r <<= 3;
        g <<= (GFX_Gmask == 0x3F ? 2/*5:6:5*/ : 3/*5:5:5*/);
        b <<= 3;
    }
    else if (bpp == 8) {
        r = render.pal.rgb[raw].red;
        g = render.pal.rgb[raw].green;
        b = render.pal.rgb[raw].blue;
    }
    else {
        r = g = b = 0;
    }

    /* XRGB */
    return  ((uint32_t)r << (uint32_t)16ul) +
            ((uint32_t)g << (uint32_t) 8ul) +
            ((uint32_t)b                  );
}

template <const unsigned int bpp,typename BPPT> void VGA_CaptureWriteScanlineChecked(const BPPT *raw) {
    raw += vga_capture_current_rect.x;

    /* output is ALWAYS 32-bit XRGB */
    for (unsigned int i=0;(int)i < vga_capture_current_rect.w;i++)
        phys_writed(vga_capture_write_address+(i*4),
            VGA_CaptureConvertPixel<bpp,BPPT>(raw[i]));

    vga_capture_write_address += vga_capture_stride;
}

void VGA_CaptureWriteScanline(const uint8_t *raw) {
    // NTS: phys_writew() will cause a segfault if the address is beyond the end of memory,
    //      because it computes MemBase+addr
    PhysPt MemMax = (PhysPt)MEM_TotalPages() * (PhysPt)4096ul;

    if (vga_capture_write_address != (uint32_t)0 &&
        vga_capture_write_address < 0xFFFF0000ul &&
        (vga_capture_write_address + (vga_capture_current_rect.w*4ul)) <= MemMax) {
        switch (vga.draw.bpp) {
            case 32:    VGA_CaptureWriteScanlineChecked<32>((const uint32_t*)raw); break;
            case 16:    VGA_CaptureWriteScanlineChecked<16>((const uint16_t*)raw); break;
            case 15:    VGA_CaptureWriteScanlineChecked<16>((const uint16_t*)raw); break;
            case 8:     VGA_CaptureWriteScanlineChecked< 8>(raw); break;
        }
    }
    else {
        VGA_CaptureMarkError();
    }
}

/* VGA debug screen */
struct VGA_debug_screen_func_t {
	void		(*clear)(unsigned int color);
	void		(*rect)(int x,int y,int w,int h,unsigned int color);
	void		(*bitblt)(int x,int y,int w,int h,size_t stride,const unsigned char *bitmap,unsigned int color);
};

static const struct VGA_debug_screen_func_t* VGA_debug_screen_func = NULL;
static unsigned char *VGA_debug_screen = NULL;
static size_t VGA_debug_screen_stride = 0;
static size_t VGA_debug_screen_w = 0;
static size_t VGA_debug_screen_h = 0;
static size_t VGA_debug_screen_bpp = 0;

template <typename T> static T* VGA_debug_screen_ptr_fast(const unsigned int y) {
	return (T*)(VGA_debug_screen + ((unsigned int)y * VGA_debug_screen_stride));
}

template <typename T> static T* VGA_debug_screen_ptr(const int y) {
	if (y >= 0 && y < (int)VGA_debug_screen_h)
		return VGA_debug_screen_ptr_fast<T>((unsigned int)y);

	return NULL;
}

template <typename T> static void VGA_debug_screen_func_clear(unsigned int color) {
	for (unsigned int y=0;y < VGA_debug_screen_h;y++) {
		T* row = VGA_debug_screen_ptr_fast<T>(y);
		for (unsigned int x=0;x < VGA_debug_screen_w;x++) *row++ = color;
	}
}

template <typename T> static void VGA_debug_screen_func_rect(int x1,int y1,int x2,int y2,unsigned int color) {
	if (x1 < 0) x1 = 0;
	if (y1 < 0) y1 = 0;
	if (x2 > (int)VGA_debug_screen_w) x2 = (int)VGA_debug_screen_w;
	if (y2 > (int)VGA_debug_screen_h) y2 = (int)VGA_debug_screen_h;
	while (y1 < y2) {
		T* row = VGA_debug_screen_ptr_fast<T>(y1++) + (unsigned int)x1;
		for (int x=x1;x < x2;x++) *row++ = color;
	}
}

template <typename T> static void VGA_debug_screen_func_bitblt(int x,int y,int w,int h,size_t stride,const unsigned char *bitmap,unsigned int color) {
	if (w <= 0 || x < 0 || (x+w) > (int)VGA_debug_screen_w) return;
	if (h <= 0 || y < 0 || (y+h) > (int)VGA_debug_screen_h) return;

	while (h > 0) {
		{
			T *row = VGA_debug_screen_ptr_fast<T>(y) + x;
			const unsigned char *s = bitmap;
			unsigned char tmp;
			size_t r = w;

			while (r >= 8) {
				tmp = *s++;
				for (size_t b=0;b < 8;b++) {
					if (tmp & 0x80) *row = color;
					tmp <<= 1u;
					row++;
				}
				r -= 8;
			}

			if (r > 0) {
				tmp = *s++;
				do {
					if (tmp & 0x80) *row++ = color;
					tmp <<= 1u;
					row++;
					r--;
				} while (r > 0);
			}
		}

		bitmap += stride;
		y++;
		h--;
	}
}

static const VGA_debug_screen_func_t VGA_debug_screen_funcs8 = {
	&VGA_debug_screen_func_clear<uint8_t>,
	&VGA_debug_screen_func_rect<uint8_t>,
	&VGA_debug_screen_func_bitblt<uint8_t>
};

static const VGA_debug_screen_func_t VGA_debug_screen_funcs16 = {
	&VGA_debug_screen_func_clear<uint16_t>,
	&VGA_debug_screen_func_rect<uint16_t>,
	&VGA_debug_screen_func_bitblt<uint16_t>
};

static const VGA_debug_screen_func_t VGA_debug_screen_funcs32 = {
	&VGA_debug_screen_func_clear<uint32_t>,
	&VGA_debug_screen_func_rect<uint32_t>,
	&VGA_debug_screen_func_bitblt<uint32_t>
};

extern uint8_t int10_font_08[256 * 8];

static int VGA_debug_screen_putc8(int x,int y,unsigned char c,unsigned int color) {
	VGA_debug_screen_func->bitblt(x,y,8,8,1,int10_font_08 + ((unsigned int)c * 8),color);
	x += 8;
	return x;
}

static int VGA_debug_screen_puts8(int x,int y,const char *msg,unsigned int color) {
	while (*msg != 0) {
		VGA_debug_screen_func->bitblt(x,y,8,8,1,int10_font_08 + (((unsigned int)((unsigned char)(*msg++))) * 8u),color);
		x += 8;
	}

	return x;
}

static void VGA_debug_screen_free(void) {
	if (VGA_debug_screen != NULL) {
		free(VGA_debug_screen);
		VGA_debug_screen = NULL;
		VGA_debug_screen_h = 0;
	}
}

static void VGA_debug_screen_alloc(size_t w,size_t h,size_t bpp) {
	assert(VGA_debug_screen == NULL);
	VGA_debug_screen_w = w;
	VGA_debug_screen_h = h;
	VGA_debug_screen_bpp = bpp;
	VGA_debug_screen_stride = ((w*((bpp+7)>>3))+7)&(~7);
	VGA_debug_screen = (unsigned char*)malloc(VGA_debug_screen_stride * VGA_debug_screen_h);

	switch (bpp) {
		case 8:
			VGA_debug_screen_func = &VGA_debug_screen_funcs8;
			break;
		case 16:
			VGA_debug_screen_func = &VGA_debug_screen_funcs16;
			break;
		case 32:
			VGA_debug_screen_func = &VGA_debug_screen_funcs32;
			break;
		default:
			VGA_debug_screen_func = NULL;
			break;
	};
}

static void VGA_debug_screen_resize(size_t w,size_t h,size_t bpp) {
	if (w == 0 || h == 0 || bpp == 0) {
		VGA_debug_screen_free();
	}
	else if (w != VGA_debug_screen_w || h != VGA_debug_screen_h || bpp != VGA_debug_screen_bpp) {
		VGA_debug_screen_free();
		VGA_debug_screen_alloc(w,h,bpp);
	}
	else if (VGA_debug_screen == NULL) {
		VGA_debug_screen_alloc(w,h,bpp);
	}
}

void VGA_DebugOverlay() {
    if (VGA_debug_screen == NULL || VGA_debug_screen_w < render.src.width) return;

    for (unsigned int y=0;y < VGA_debug_screen_h && render.scale.inLine < render.src.height;y++)
        RENDER_DrawLine(VGA_debug_screen+(y*VGA_debug_screen_stride));
}

EGAMonitorMode egaMonitorMode(void);

extern uint8_t CGAPal2[2];
extern uint8_t CGAPal4[4];

static std::vector<debugline_event> debugline_events;
static unsigned int debugline_event_alloc_x = 0;

void VGA_DebugAddEvent(debugline_event &ev) {
	bool is_ega64 = (machine == MCH_EGA) && (egaMonitorMode() == EGA);
	unsigned int minw = 0;

	if (machine == MCH_EGA) {
		minw = 4+(16*2)+4+(4*2)+4;
	}
	else if (machine == MCH_VGA) {
		minw = 4+16+4;
		if (vga.mode == M_VGA || vga.mode == M_LIN8) minw += 256+4;
	}
	else if (machine == MCH_PC98) {
		minw = 4+4;
		if (pc98_gdc_vramop & (1u << VOPBIT_VGA)) {
		}
		else if (pc98_gdc_vramop & (1u << VOPBIT_ANALOG)) {
			minw += 16*2;
			minw += 4;
		}
		else {
			minw += 8*2;
			minw += 4;
		}
	}

	if (debugline_events.empty()) debugline_event_alloc_x = minw;

	if ((debugline_event_alloc_x+ev.drawwidth()) > (render.src.width-vga.draw.width))
		debugline_event_alloc_x = minw;

	debugline_event_alloc_x += 8;

	ev.x = debugline_event_alloc_x;

	debugline_event_alloc_x += ev.drawwidth();
	if (debugline_event_alloc_x >= (render.src.width-vga.draw.width))
		debugline_event_alloc_x = minw;

	if (ev.colorline == 0) {
		switch (ev.event) {
			case DBGEV_SPLIT:
				if (vga.draw.bpp == 8)
					ev.colorline = is_ega64 ? 0x12 : 0x0A;
				else
					ev.colorline = GFX_Gmask;
				break;
			default:
				if (vga.draw.bpp == 8)
					ev.colorline = is_ega64 ? 0x3F : 0x0F;
				else
					ev.colorline = GFX_Rmask | GFX_Gmask | GFX_Bmask;
				break;
		}
	}

	debugline_events.push_back(std::move(ev));
}

void VGA_DebugAddEvent_VGASplit(void) {
	debugline_event ev;

	ev.event = DBGEV_SPLIT;
	ev.addline("SPLIT");
	ev.addline("LNCMP");

	VGA_DebugAddEvent(ev);
}

void VGA_DrawDebugLine(uint8_t *line,unsigned int w) {
	const unsigned int dacshift = vga_8bit_dac?0:2;
	unsigned int white,dkgray;
	unsigned int minw = 0;

	if (dbg_event_maxscan) {
		debugline_event ev;

		ev.event = DBGEV_SPLIT;
		ev.addline("MXS");

		VGA_DebugAddEvent(ev);

		dbg_event_maxscan = false;
	}

	if (dbg_event_scanstep) {
		debugline_event ev;

		ev.event = DBGEV_SPLIT;
		ev.addline("OFS");

		VGA_DebugAddEvent(ev);

		dbg_event_scanstep = false;
	}

	if (dbg_event_hretrace) {
		debugline_event ev;

		ev.event = DBGEV_SPLIT;
		ev.addline("HRT");

		VGA_DebugAddEvent(ev);

		dbg_event_hretrace = false;
	}

	if (dbg_event_color_plane_enable) {
		debugline_event ev;

		ev.event = DBGEV_SPLIT;
		ev.addline("CPE");

		VGA_DebugAddEvent(ev);

		dbg_event_color_plane_enable = false;
	}

	if (dbg_event_color_select) {
		debugline_event ev;

		ev.event = DBGEV_SPLIT;
		ev.addline("CSL");

		VGA_DebugAddEvent(ev);

		dbg_event_color_select = false;
	}

	/* line points into part of the image past active display */
	switch (VGA_debug_screen_bpp) {
		case 8:
			// CGA/Tandy/PCjr/Herc/MDA
			if (machine == MCH_HERC || machine == MCH_MDA) {
				white = 1;
			}
			else if (machine == MCH_EGA) {
				dkgray = (egaMonitorMode() == EGA) ? 0x38 : 0x10;
				white = 0x3F;
			}
			else {
				white = 0xF;
			}
			break;
		case 32: // VGA/MCGA/SVGA/PC98
			white = GFX_Bmask | GFX_Gmask | GFX_Rmask;
			break;
		default:
			return;
	};

	if (machine == MCH_PC98) {
		if (vga.draw.bpp == 32) { /* Doesn't use anything else */
			uint32_t *draw = (uint32_t*)line;
			unsigned int dw = w;

			if (dw <= 4) return;
			for (unsigned int c=0;c < 4;c++) {
				*draw++ = 0;
				dw--;
			}

			if (pc98_gdc_vramop & (1u << VOPBIT_VGA)) {
			}
			else if (pc98_gdc_vramop & (1u << VOPBIT_ANALOG)) {
				if (dw <= (16*2)) return;
				for (unsigned int c=0;c < 16;c++) {
					draw[0] = draw[1] = vga.dac.xlat32[c];
					draw += 2;
					dw -= 2;
				}

				if (dw <= 4) return;
				for (unsigned int c=0;c < 4;c++) {
					*draw++ = 0;
					dw--;
				}
			}
			else {
				if (dw <= (8*2)) return;
				for (unsigned int c=0;c < 8;c++) {
					draw[0] = draw[1] = vga.dac.xlat32[c];
					draw += 2;
					dw -= 2;
				}

				if (dw <= 4) return;
				for (unsigned int c=0;c < 4;c++) {
					*draw++ = 0;
					dw--;
				}
			}

			minw = (unsigned int)(draw+4-(uint32_t*)line);

			while (dw > 0) {
				*draw++ = 0;
				dw--;
			}
		}
	}
	else if (machine == MCH_VGA) {
		if (vga.draw.bpp == 32) { /* Doesn't use anything else */
			uint32_t *draw = (uint32_t*)line;
			unsigned int dw = w;

			if (dw <= 4) return;
			for (unsigned int c=0;c < 4;c++) {
				*draw++ = 0;
				dw--;
			}

			if (vga.mode == M_VGA || vga.mode == M_LIN8) {
				if (dw <= 256) return;
				for (unsigned int c=0;c < 256;c++) {
					*draw++ = vga.dac.xlat32[c];
					dw--;
				}

				if (dw <= 4) return;
				for (unsigned int c=0;c < 4;c++) {
					*draw++ = 0;
					dw--;
				}
			}

			if (dw <= 16) return;
			for (unsigned int c=0;c < 16;c++) {
				const unsigned int idx = vga.dac.combine[c]; /* vga_dac.cpp considers color select */
				const unsigned int color = SDL_MapRGB(
					sdl.surface->format,
					((vga.dac.rgb[idx].red << dacshift) & 0xFF),
					((vga.dac.rgb[idx].green << dacshift) & 0xFF),
					((vga.dac.rgb[idx].blue << dacshift) & 0xFF));
				*draw++ = color;
				dw--;
			}

			if (dw <= 4) return;
			for (unsigned int c=0;c < 4;c++) {
				*draw++ = 0;
				dw--;
			}

			minw = (unsigned int)(draw+4-(uint32_t*)line);

			while (dw > 0) {
				*draw++ = 0;
				dw--;
			}
		}
	}
	else if (machine == MCH_EGA) {
		if (vga.draw.bpp == 8) { /* Doesn't use anything else */
			uint8_t *draw = line;
			unsigned int dw = w;

			if (dw <= 4) return;
			for (unsigned int c=0;c < 4;c++) {
				*draw++ = 0;
				dw--;
			}

			if (dw <= (16*2)) return;
			for (unsigned int c=0;c < 16;c++) {
				draw[0] = draw[1] = vga.attr.palette[c&vga.attr.color_plane_enable];
				draw += 2;
				dw -= 2;
			}

			if (dw <= 4) return;
			for (unsigned int c=0;c < 4;c++) {
				*draw++ = 0;
				dw--;
			}

			if (dw <= (4*2)) return;
			for (unsigned int c=0;c < 4;c++) {
				draw[0] = draw[1] = ((vga.attr.color_plane_enable << c) & 8) ? white : dkgray;
				draw += 2;
				dw -= 2;
			}

			minw = (unsigned int)(draw+4-line);

			while (dw > 0) {
				*draw++ = 0;
				dw--;
			}
		}
	}

	bool allclear = true;

	for (auto i=debugline_events.begin();i!=debugline_events.end();i++) {
		auto &ev = *i;
		if (!ev.done && ev.tline == 0 && ev.trow == 0) {
			if (vga.draw.bpp == 8) {
				for (unsigned int x=minw;x < w && x < ev.x;x++) line[x] = ev.colorline;
			}
			else if (vga.draw.bpp == 32) {
				for (unsigned int x=minw;x < w && x < ev.x;x++) ((uint32_t*)line)[x] = ev.colorline;
			}

			if (!ev.done) allclear = false;
		}
	}

	for (auto i=debugline_events.begin();i!=debugline_events.end();i++) {
		auto &ev = *i;
		if (!ev.done) {
			if (ev.tline < ev.text.size()) {
				if (ev.trow < 8) {
					if (vga.draw.bpp == 8) {
						if ((ev.x+ev.w) <= w) {
							uint8_t *dp = line+ev.x;
							const char *str = ev.text[ev.tline].c_str();
							unsigned int dw = ev.w;
							while (*str != 0 && dw >= 8) {
								unsigned char c = (unsigned char)(*str++);
								unsigned char b = int10_font_08[(c*8)+ev.trow];
								for (unsigned int x=0;x < 8;x++) {
									*dp++ = (b & 0x80) ? 0 : ev.colorline;
									b <<= 1u;
								}
								dw -= 8;
							}
							while (dw >= 8) {
								for (unsigned int x=0;x < 8;x++) *dp++ = ev.colorline;
								dw -= 8;
							}
						}
					}
					else if (vga.draw.bpp == 32) {
						if ((ev.x+ev.w) <= w) {
							uint32_t *dp = (uint32_t*)line+ev.x;
							const char *str = ev.text[ev.tline].c_str();
							unsigned int dw = ev.w;
							while (*str != 0 && dw >= 8) {
								unsigned char c = (unsigned char)(*str++);
								unsigned char b = int10_font_08[(c*8)+ev.trow];
								for (unsigned int x=0;x < 8;x++) {
									*dp++ = (b & 0x80) ? 0 : ev.colorline;
									b <<= 1u;
								}
								dw -= 8;
							}
							while (dw >= 8) {
								for (unsigned int x=0;x < 8;x++) *dp++ = ev.colorline;
								dw -= 8;
							}
						}
					}

					ev.trow++;
				}
				if (ev.trow >= 8) {
					ev.tline++;
					ev.trow = 0;
				}
			}

			if (ev.tline >= ev.text.size()) ev.done = true;
			if (!ev.done) allclear = false;
		}
	}

	if (allclear) debugline_events.clear();
}

void VGA_sof_debug_video_info(void) {
	unsigned int green,white;
	char tmp[256];
	int x,y;

	switch (VGA_debug_screen_bpp) {
		case 8:
			// CGA/Tandy/PCjr/Herc/MDA
			if (machine == MCH_HERC || machine == MCH_MDA) {
				white = 1;
				green = 1;
			}
			else if (machine == MCH_EGA) {
				white = 0x3F;
				green = 0x12; /* xxRGBrgb */
			}
			else {
				white = 0xF;
				green = 0xA; /* xxxxIRGB */
			}
			break;
		case 32: // VGA/MCGA/SVGA/PC98
			green = GFX_Gmask;
			white = GFX_Bmask | GFX_Gmask | GFX_Rmask;
			break;
		default:
			return;
	};

	x = y = 4;
	x = VGA_debug_screen_puts8(x,y,mode_texts[vga.mode],green) + 8;
	if (vga.mode == M_PC98) {
		/* PC-98 has two video "layers" that can contain both text and graphics at the same time.
		 * Each one can be turned off at any time and it's helpful here to indicate if that's the case. */
		char *d = tmp;

		/* text */
		if (pc98_gdc[GDC_MASTER].display_enable) {
			d += sprintf(d,"T%ux%u",
				pc98_gdc[GDC_MASTER].active_display_words_per_line / (pc98_40col_text?2:1),
				pc98_gdc[GDC_MASTER].active_display_lines / pc98_gdc[GDC_MASTER].row_height);
		}
		else {
			d += sprintf(d,"T---");
		}

		*d++ = '/';

		/* graphics */
		if (pc98_gdc[GDC_SLAVE].display_enable) {
			unsigned int rowsize = pc98_gdc[GDC_SLAVE].row_height;

			/* NTS: Testing with real hardware shows 256-color mode ignores row height, or else the PC-98 port of "Alone in the Dark" would look wrong */
			if (pc98_gdc_vramop & (1 << VOPBIT_VGA))
				rowsize = 1;

			/* FIXME: Pixels count is incorrect for PC-9821 DOS utility "Paint tool" by Login (1280x480?)... but correct for 256-color PC-9821 version
			 *        of Battle Skin Panic (640x480) */
			d += sprintf(d,"G%ux%u",
				pc98_gdc[GDC_SLAVE].active_display_words_per_line * (gdc_5mhz_mode?8:16)/*character clocks to pixels*/,
				pc98_gdc[GDC_SLAVE].active_display_lines / rowsize);

			if (pc98_gdc_vramop & (1 << VOPBIT_VGA))
				d += sprintf(d,"-256c");
			else if (pc98_gdc_vramop & (1 << VOPBIT_ANALOG))
				d += sprintf(d,"-16c");
			else if (pc98_monochrome_mode)
				d += sprintf(d,"-2c");
			else
				d += sprintf(d,"-8c");

			if (pc98_graphics_hide_odd_raster_200line && pc98_gdc[GDC_SLAVE].row_height > 1 && !(pc98_gdc_vramop & (1 << VOPBIT_VGA)))
				d += sprintf(d,"-r"); /* raster effect but you can't do it in 256-color mode and row height must be greater than 1 */
		}
		else {
			d += sprintf(d,"G---");
		}

		d += sprintf(d," GDC:%sMHz",gdc_5mhz_mode?"5":"2.5");
	}
	else if (vga.mode == M_TEXT || vga.mode == M_TANDY_TEXT || vga.mode == M_HERC_TEXT) {
		unsigned int pixperclock = 8;

		if (machine == MCH_EGA || machine == MCH_VGA)
			pixperclock = ((vga.seq.clocking_mode&1)?8:9);
		else if (machine == MCH_HERC)
			pixperclock = 8;

		sprintf(tmp,"T%ux%u>%ux%u",
			(unsigned int)vga.draw.width / pixperclock,(unsigned int)vga.draw.height / (unsigned int)vga.draw.address_line_total,
			(unsigned int)vga.draw.width,(unsigned int)vga.draw.height);
	}
	else {
		unsigned int rowdiv = (unsigned int)vga.draw.address_line_total;
		unsigned int interleave_mul = 1;

		if (machine == MCH_CGA || machine == MCH_TANDY || machine == MCH_PCJR || machine == MCH_HERC || machine == MCH_AMSTRAD) {
			if (rowdiv == 2 || rowdiv == 4) rowdiv = 1; /* CGA graphics use interleaving to accomplish 200 lines, Tandy and Hercules use 4-way interleaving in some modes */
		}
		else if (machine == MCH_EGA || machine == MCH_VGA) {
			/* EGA/VGA have bits set to display video memory 2-way interleave like CGA and even 4-way interleave like Hercules */
			if (rowdiv == 4 && (vga.tandy.line_mask & 2)) rowdiv = 1;
			else if (rowdiv == 2 && (vga.tandy.line_mask & 1)) rowdiv = 1;
		}

		sprintf(tmp,"G%ux%u>%ux%u",
			(unsigned int)vga.draw.width,((unsigned int)vga.draw.height * interleave_mul) / rowdiv,
			(unsigned int)vga.draw.width,(unsigned int)vga.draw.height);
	}
	x = VGA_debug_screen_puts8(x,y,tmp,white) + 8;

	if (vga.mode == M_PC98) {
		char *d = tmp;

		d += sprintf(d,"T@%04x+%03x/",
			(unsigned int)pc98_gdc[GDC_MASTER].scan_address,
			(unsigned int)pc98_gdc[GDC_MASTER].display_pitch);

		if (pc98_gdc_vramop & (1 << VOPBIT_VGA)) {
			d += sprintf(d,"G@0");
		}
		else {
			/* TODO: Show stride, then both display partitions, including start address and number of lines.
			 *       It might be helpful to the curious to see how vertical scrolling is actually done with
			 *       most PC-98 games. Don't bother showing all 4 data partitions because you're supposed to
			 *       only use the first 8 bytes for two and the latter 8 bytes for "parameters" to GDC commands.
			 *       Older PC-98 games exploit the fact that the hardware will happily allow the latter 8 to
			 *       be used as a 3rd and 4th display partition (Edge, Steel Hearts) even when newer hardware
			 *       fixes the "bug" and forces only two partitions. Therefore, show only the first two.
			 *
			 *       As for the text display, you can use all 4 display partitions, however I have yet to see
			 *       any PC-98 game use partitions at all on the text layer. Some might adjust the first
			 *       partition for text scrolling... that's about it. */
			d += sprintf(d,"G@%04x+%03x",
				(unsigned int)pc98_gdc[GDC_SLAVE].scan_address,
				(unsigned int)pc98_gdc[GDC_MASTER].display_pitch);
		}

		d += sprintf(d," pg:c%ud%u",(pc98_gdc_vramop & (1 << VOPBIT_ACCESS))?1:0,GDC_display_plane_pending);
	}
	else if (IS_EGAVGA_ARCH) {
		char *d = tmp;

		if (IS_VGA_ARCH && svgaCard != SVGA_None)
			d += sprintf(d,"@%05x+%03x",(unsigned int)vga.config.display_start,(unsigned int)vga.config.scan_len*2);
		else
			d += sprintf(d,"@%04x+%02x",(unsigned int)vga.config.display_start,(unsigned int)vga.config.scan_len*2);

		switch (vga.config.addr_shift) {
			case 0: *d++ = '-'; *d++ = 'B'; break;
			case 1: *d++ = '-'; *d++ = 'W'; break;
			case 2: *d++ = '-'; *d++ = 'D'; break;
			default: break;
		}

		if (IS_VGA_ARCH && (vga.mode == M_LIN8 || vga.mode == M_VGA)) {
			/* maybe the user might want to know if 256-color mode is chained or unchained */
			if (vga.seq.memory_mode & 8) d += sprintf(d,"ch4"); /* if the "chain 4" bit is set, normal chained 4 */
			else if (vga.config.addr_shift == 0) d += sprintf(d,"uch"); /* if not set, and CRTC mode is byte mode, unchained */
			/* anything else is weird */
		}

		*d = 0;
	}
	else {
		sprintf(tmp,"@%04x+%02x",(unsigned int)vga.config.display_start,(unsigned int)vga.other.hdend);
	}
	x = VGA_debug_screen_puts8(x,y,tmp,white) + 8;

	if (machine == MCH_EGA) {
		char *d = tmp;
		EGAMonitorMode m = egaMonitorMode();
		if (m == EGA) d += sprintf(d,"64c");
		else if (m == CGA) d += sprintf(d,"16c");
		else if (m == MONO) d += sprintf(d,"mono");

		x = VGA_debug_screen_puts8(x,y,tmp,white) + 8;
	}

	/* next line: The color palette. Show a) the raw palette and b) the effective palette after all bit masking.
	 * What we show depends on the hardware. For MDA/Hercules, you have ON and OFF so there's really no point in drawing it.
	 * For CGA, you have all 16 colors in text mode, 4 colors for 320x200 from one 3 palettes (I'm counting the unofficial
	 * palette with red instead of magenta) and a background color, and 2 colors for 640x200 (black + foreground color).
	 *
	 * PCjr and Tandy allow remapping the IRGB colors to... uh... other IRGB colors.
	 *
	 * EGA remaps the 16-color palette through the Attribute Controller (first 16 registers) which then either becomes a
	 * 4-bit IRGB color for 200-line modes or a 6-bit xxRGBrgb (2-bit RGB = one of 64 colors) color. This is also affected
	 * by a register that controls which bitplanes are sent to the display. I don't think EGA has a pel mask register.
	 *
	 * MCGA, except for 256-color mode, could be thought of as a fancy CGA card that produces a 4-bit IRGB value, which is
	 * then treated like any other 8-bit value and sent to the DAC as-is. The 256-color mode is just 8-bit values sent to
	 * the DAC as-is. Testing on a real PS/2 shows that MCGA systems have a VGA-like PEL mask register.
	 *
	 * VGA could be thought of as hardware that latches 4-bit pixel values around, which makes sense when you consider that
	 * all standard modes OTHER than 256-color mode all boil down to: load, shift, mask, 4-bit pixel, send to output. Testing
	 * shows that 256-color mode is even affected by this design. What looks like 8-bit values on display are apparently just
	 * bytes from RAM shifted in 4 bits at a time per dot clock. You don't see that because another bit is set to hold the
	 * DAC at the last whole value to hide the "halfway" byte underneath. It does explain why 256-color mode has only 320
	 * pixels across and yet CRTC horizontal timing and dot clock values are programmed as if a 640 pixel wide mode, and why
	 * standard VGA hardware cannot do a 640-pixel wide 256-color mode. Anyway, the 4-bit pixels going through the VGA hardware
	 * could be thought of as going through a bitplane mask register, then the attribute controller which is then expanded to
	 * a 6-bit value (EGA compatibility). If the right bits are set, you can fill in the top 2 bits from the color select
	 * registers. The result is an 8-bit value which is then masked off through the PEL mask and then sent to the DAC through
	 * which the final color is determined by the VGA palette. The two 4-bit values that make up the 8-bit 256-color mode
	 * are handled in exactly the same way, the Attribute Controller palette can affect 256-color mode! The difference is
	 * that color select doesn't have any effect because only the low 4 bits are used to produce the final 8 bits (although
	 * the 1992 demo "Copper" exploits a hardware bug on Tseng ET4000 cards regarding color select to do those nifty "line
	 * fading" demo effects).
	 *
	 * PC-98 hardware could be thought of as having 3 hardware palettes: the 8-color "digital" mode, the 16-color "analog"
	 * mode, and the 256-color "vga" mode. At least the hardware I've tested on seems to behave as if somewhere in the
	 * hardware, all 3 palettes exist simultaneously at once. The 8-color "digital" mode can remap the GRB colors from
	 * any 3-bit value to any other 3-bit value. The 16-color analog mode offers RGB output with 4 bits per channel, which
	 * incidentally, is why when PC-98 games do palette fades, they aren't as smooth as VGA or SVGA palette fades. However
	 * the 256-color mode has a full 8 bits per channel to work with (2 more bits than VGA's 6 bits per channel!). Which
	 * palette we show therefore depends on which mode the graphics plane is. There is also a "monochrome" mode which reduces
	 * the 8-color mode to only bitplane 2 (green) and then the final color is controlled by the color attribute of the
	 * text layer. As for the text mode, it has 3 bits to encode GRB and there's no palette to remap it, therefore nothing
	 * to see here. There's no funny bitplane masking to the display that I'm aware of unlike VGA, which should make the
	 * display code simpler here. */
	y += 8;
	x = 4;
	if (machine == MCH_PC98) {
		if (vga.draw.bpp == 32) { /* PC-98 emulation doesn't use anything else */
			const unsigned int dkgray =
				((GFX_Rmask >> 2) & GFX_Rmask) |
				((GFX_Gmask >> 2) & GFX_Gmask) |
				((GFX_Bmask >> 2) & GFX_Bmask);

			/* NTS: We *could* show all 3 hardware palettes, but I don't think anything out there
			 *      depends on flipping between them like that. I doubt the hardware could support
			 *      anything funky like flipping between 8/16-color mode mid-scanline like that. */
			if (pc98_gdc_vramop & (1u << VOPBIT_VGA)) {
				x = VGA_debug_screen_puts8(x,y,"PAL256:",white) + 8;
				VGA_debug_screen_func->rect(x-1,y,x,y+7,dkgray);
				VGA_debug_screen_func->rect(x-1,y,x+(4*128),y+1,dkgray);
				VGA_debug_screen_func->rect(x-1,y+(4*2)-1,x+(4*128),y+(4*2),dkgray);
				for (unsigned int c=0;c < 256;c++) {
					const int bx = x+((c&127)*4),by = y+((c>>7)*4);
					VGA_debug_screen_func->rect(bx,by+1,bx+3,by+4,vga.dac.xlat32[c]); /* xlat32[] already contains the translated color */
					VGA_debug_screen_func->rect(bx+3,by+1,bx+4,by+4,dkgray);
				}

				x += 8 + (4*128);
			}
			else if (pc98_gdc_vramop & (1u << VOPBIT_ANALOG)) {
				x = VGA_debug_screen_puts8(x,y,"PAL16:",white) + 8;
				VGA_debug_screen_func->rect(x-1,y,x,y+7,dkgray);
				VGA_debug_screen_func->rect(x-1,y,x+(8*16),y+1,dkgray);
				VGA_debug_screen_func->rect(x-1,y+7,x+(8*16),y+8,dkgray);
				for (unsigned int c=0;c < 16;c++) {
					VGA_debug_screen_func->rect(x,y+1,x+7,y+7,vga.dac.xlat32[c]); /* xlat32[] already contains the translated color */
					VGA_debug_screen_func->rect(x+7,y+1,x+8,y+7,dkgray);
					x += 8;
				}

				x += 8;
			}
			else {
				x = VGA_debug_screen_puts8(x,y,"PAL8:",white) + 8;
				VGA_debug_screen_func->rect(x-1,y,x,y+7,dkgray);
				VGA_debug_screen_func->rect(x-1,y,x+(8*8),y+1,dkgray);
				VGA_debug_screen_func->rect(x-1,y+7,x+(8*8),y+8,dkgray);
				for (unsigned int c=0;c < 8;c++) {
					VGA_debug_screen_func->rect(x,y+1,x+7,y+7,vga.dac.xlat32[c]); /* xlat32[] already contains the translated color */
					VGA_debug_screen_func->rect(x+7,y+1,x+8,y+7,dkgray);
					x += 8;
				}

				x += 8;

				if (pc98_monochrome_mode) {
					x = VGA_debug_screen_puts8(x,y,"PALM:",white) + 8;
					VGA_debug_screen_func->rect(x-1,y,x,y+7,dkgray);
					VGA_debug_screen_func->rect(x-1,y,x+(8*2),y+1,dkgray);
					VGA_debug_screen_func->rect(x-1,y+7,x+(8*2),y+8,dkgray);
					for (unsigned int c=0;c < 2;c++) {
						VGA_debug_screen_func->rect(x,y,x+7,y+7,vga.dac.xlat32[c<<4u]); /* xlat32[] already contains the translated color, green -> mono */
						VGA_debug_screen_func->rect(x+7,y,x+8,y+7,dkgray);
						x += 8;
					}
				}

				x += 8;
			}

			/* point out where side debug info is */
			x = vga.draw.width;
			y = 0;

			x += 4;
			if (pc98_gdc_vramop & (1u << VOPBIT_VGA)) {
			}
			else if (pc98_gdc_vramop & (1u << VOPBIT_ANALOG)) {
				VGA_debug_screen_func->rect(x,y,x+(8*4),y+(8*1),white);
				VGA_debug_screen_puts8(x,y,"EPAL",0);
				x += 16*2;
				x += 4;
			}
			else {
				VGA_debug_screen_func->rect(x,y,x+(8*2),y+(8*2),white);
				VGA_debug_screen_puts8(x,y,"EP",0);
				VGA_debug_screen_puts8(x,y+8,"AL",0);
				x += 8*2;
				x += 4;
			}
		}
	}
	else if (machine == MCH_CGA) {
		if (vga.draw.bpp == 8 && vga.mode != M_CGA16) { /* CGA emulation doesn't use anything else, and do not draw palette in "composite" mode */
			x = VGA_debug_screen_puts8(x,y,"PAL:",white) + 8;
			VGA_debug_screen_func->rect(x-1,y,x,y+7,0x8/*dkgray*/);
			if (vga.mode == M_CGA4 || vga.mode == M_TANDY4) {
				VGA_debug_screen_func->rect(x-1,y,x+(8*4),y+1,0x8);
				VGA_debug_screen_func->rect(x-1,y+7,x+(8*4),y+8,0x8);
				for (unsigned int c=0;c < 4;c++) {
					VGA_debug_screen_func->rect(x,y+1,x+7,y+7,CGAPal4[c]);
					VGA_debug_screen_func->rect(x+7,y+1,x+8,y+7,0x8);
					x += 8;
				}
			}
			else if (vga.mode == M_CGA2 || vga.mode == M_TANDY2) {
				VGA_debug_screen_func->rect(x-1,y,x+(8*2),y+1,0x8);
				VGA_debug_screen_func->rect(x-1,y+7,x+(8*2),y+8,0x8);
				for (unsigned int c=0;c < 2;c++) {
					VGA_debug_screen_func->rect(x,y+1,x+7,y+7,CGAPal2[c]);
					VGA_debug_screen_func->rect(x+7,y+1,x+8,y+7,0x8);
					x += 8;
				}
			}
			else {
				VGA_debug_screen_func->rect(x-1,y,x+(8*16),y+1,0x8);
				VGA_debug_screen_func->rect(x-1,y+7,x+(8*16),y+8,0x8);
				for (unsigned int c=0;c < 16;c++) {
					VGA_debug_screen_func->rect(x,y+1,x+7,y+7,c);
					VGA_debug_screen_func->rect(x+7,y+1,x+8,y+7,0x8);
					x += 8;
				}
			}

			x += 8;
		}
	}
	else if (machine == MCH_PCJR || machine == MCH_TANDY) {
		/* Tandy/PCjr code re-uses the VGA attribute controller palette. CGA emulation re-uses CGA remaps. */
		if (vga.draw.bpp == 8) { /* Doesn't use anything else */
			x = VGA_debug_screen_puts8(x,y,"HWPAL:",white) + 8;
			VGA_debug_screen_func->rect(x-1,y,x,y+7,0x8/*dkgray*/);
			VGA_debug_screen_func->rect(x-1,y,x+(8*16),y+1,0x8);
			VGA_debug_screen_func->rect(x-1,y+7,x+(8*16),y+8,0x8);
			for (unsigned int c=0;c < 16;c++) {
				VGA_debug_screen_func->rect(x,y+1,x+7,y+7,vga.attr.palette[c]);
				VGA_debug_screen_func->rect(x+7,y+1,x+8,y+7,0x8);
				x += 8;
			}

			x += 8;

			if (vga.mode == M_CGA4 || vga.mode == M_TANDY4) {
				x = VGA_debug_screen_puts8(x,y,"MDPAL:",white) + 8;
				VGA_debug_screen_func->rect(x-1,y,x,y+7,0x8/*dkgray*/);
				VGA_debug_screen_func->rect(x-1,y,x+(8*4),y+1,0x8);
				VGA_debug_screen_func->rect(x-1,y+7,x+(8*4),y+8,0x8);
				for (unsigned int c=0;c < 4;c++) {
					VGA_debug_screen_func->rect(x,y+1,x+7,y+7,CGAPal4[c]);//already remapped, vga_other.cpp
					VGA_debug_screen_func->rect(x+7,y+1,x+8,y+7,0x8);
					x += 8;
				}
			}
			else if (vga.mode == M_CGA2 || vga.mode == M_TANDY2) {
				x = VGA_debug_screen_puts8(x,y,"MDPAL:",white) + 8;
				VGA_debug_screen_func->rect(x-1,y,x,y+7,0x8/*dkgray*/);
				VGA_debug_screen_func->rect(x-1,y,x+(8*2),y+1,0x8);
				VGA_debug_screen_func->rect(x-1,y+7,x+(8*2),y+8,0x8);
				for (unsigned int c=0;c < 2;c++) {
					VGA_debug_screen_func->rect(x,y+1,x+7,y+7,CGAPal2[c]);//already remapped, vga_other.cpp
					VGA_debug_screen_func->rect(x+7,y+1,x+8,y+7,0x8);
					x += 8;
				}
			}
			else {
				/* would be redundant copy of HWPAL */
			}

			x += 8;
		}
	}
	else if (machine == MCH_VGA || machine == MCH_MCGA) {
		/* MCGA: Each mode produces an 8-bit pixel, which is masked by PEL mask and sent to the DAC.
		 *       Text modes produce values 0-15, CGA 4-color 0-3, CGA 2-color and MCGA monochrome graphics 0-1,
		 *       and of course 256-color mode 0-255. Correct colors are set by the BIOS through programming
		 *       the color palette. There is no attribute controller, but then again, there is no support for
		 *       planar 16-color modes, or 16-color at all (although poking at registers reveals that you can
		 *       hack yourself a packed 4-bit 16-color mode that renders only every other pixel).
		 *
		 * VGA: Each mode produces 4-bit pixels per dot clock (256-color mode loads 2 4-bit pixels and emits the 8-bit value per every other dot clock).
		 *      Text modes and EGA/VGA 16-color modes produce values 0-15, CGA 4-color 0-3, CGA 2-color and monochrome EGA/MCGA 0-1,
		 *      and 256-color mode 0-15 for each half of the 8-bit pixel value. Note that on EGA/VGA, there's a "color plane enable"
		 *      register that is used to make sure CGA 2-color and monochrone MCGA modes are limited to one bitplane, in fact CGA 2-color
		 *      and MCGA monochrome modes are really just EGA 16-color modes with only one bitplane enabled for display and writing
		 *      and the memory map set up to emulate non-planar video memory like the DOS application expects. CGA 4-color is still one
		 *      bitplane with odd/even mode enabled and a bit that instructs the hardware to latch 2 bits at a time which are then internally
		 *      mapped as if bitplane 0 and 1.
		 *
		 *      Then, the 4-bit value is remapped through the Attribute Controller (which helps make VGA backwards compatible with EGA
		 *      palette tricks) to produce a 6-bit value. On the EGA hardware this 6-bit value went directly to the TTL pins, while on
		 *      VGA, the value eventually becomes an index into the VGA color palette. If Color Select is enabled, an additional 2 bits
		 *      are filled in at the top to produce an 8-bit value (though Tseng ET4000 cards have Color Select hardware bugs that a
		 *      few demoscene productions exploit for an effect ref. 1992 demo "Copper").
		 *
		 *      The 8-bit value is masked by the PEL mask register before it is finally sent to the DAC and translated by the color
		 *      palette registers to the analog VGA output sent to the monitor.
		 *
		 *      Note that since 256-color mode is just two 4-bit values internally, it too is affected by the attribute controller,
		 *      though only the 4 bits are used coming out and Color Select has no effect (but see comments on Tseng ET4000 hardware
		 *      bugs farther up). Don't believe me? Bring up 256-color mode, clear the "Shift256" bit in the Graphics registers, and
		 *      you'll see that every odd column corresponds to some intermediate 8-bit value made from 4 bits of the previous pixel
		 *      and 4 bits of the next pixel. In this way you kind of sort of have a 640x200 256-color mode but you have to use it
		 *      carefully knowing the odd pixel columns are drawn that way (NTS: According to my tests this trick does not work on
		 *      Tseng ET4000 cards... doing that will instead give you a 320x200 screen that is not pixel doubled and is squeezed on
		 *      the right half of the screen with garbage data on the left half).
		 *
		 *      Got it? Good. This complexity is why most games written for EGA work pefectly fine on VGA, even if they play with the
		 *      EGA palette in the Attribute Controller or color plane masks.
		 *
		 * NOTES: A good test case for the color plane enable:
		 *      - Any CGA graphics mode or monochrome mode where INT 10h uses color plane enable to limit the modes to one bitplane.
		 *      - "Megademo" by Space Pigs which plays with the color plane enable and EGA palette a lot for demo effects. Pay particular
		 *        attention to the end "credits" which uses color plane enable as both a crude method of "page flipping" for the rotating
		 *        dot animation in the background AND as a way to store both pages of credit text in bitplanes 2 and 3 while only showing
		 *        bitplane 2 the first time and bitplane 3 the second time as it hardware scrolls. */
		if (vga.draw.bpp == 32 && !(vga.mode == M_LIN15 || vga.mode == M_LIN16 || vga.mode == M_LIN24 || vga.mode == M_LIN32)) {
			const unsigned int dacshift =
				vga_8bit_dac?0:2;
			const unsigned int dkgray =
				((GFX_Rmask >> 2) & GFX_Rmask) |
				((GFX_Gmask >> 2) & GFX_Gmask) |
				((GFX_Bmask >> 2) & GFX_Bmask);

			/* VGA emulation always renders 32bpp. Do not show palette in highcolor/truecolor 15/16/24/32bpp modes because
			 * there is no palette to worry about.
			 *
			 * FIXME: That's not entirely true. SVGA chipset manufacturers at some point in the mid to late 1990s decided that
			 *        it would be neat-o if the user could go into the Control Panel of their Windows system and have the ability
			 *        to adjust the gamma curve of their display. Since the DAC palette is otherwise unused in highcolor/truecolor
			 *        modes, they decided to re-use the DAC palette as a way to remap the R, G, and B values of the mode so gamma
			 *        curve adjustments are possible. Many SVGA chipsets from the 2000s on have some form of this "VGA palette
			 *        as gamma curve" function including Intel GPUs and ATI Radeon cards. */

			/* NTS: Render as 64x4 grid instead of a 128x2 grid. Unlike the PC-98 case where we can always assume a 640-wide render,
			 *      the VGA emulation might double the pixels horizontally for 320-pixel wide modes. */

			/* raw PAL (actual DAC contents) */
			sprintf(tmp,"RPAL%u:",vga_8bit_dac?8:6);
			x = VGA_debug_screen_puts8(x,y,tmp,white) + 8;
			VGA_debug_screen_func->rect(x-1,y,x,y+(4*4),dkgray);
			VGA_debug_screen_func->rect(x-1,y,x+(4*64),y+1,dkgray);
			VGA_debug_screen_func->rect(x-1,y+(4*4)-1,x+(4*64),y+(4*4),dkgray);
			for (unsigned int c=0;c < 256;c++) {
				const int bx = x+((c&63)*4),by = y+((c>>6)*4);
				const unsigned int color = SDL_MapRGB(
					sdl.surface->format,
					((vga.dac.rgb[c].red << dacshift) & 0xFF),
					((vga.dac.rgb[c].green << dacshift) & 0xFF),
					((vga.dac.rgb[c].blue << dacshift) & 0xFF));
				VGA_debug_screen_func->rect(bx,by+1,bx+3,by+4,color);
				VGA_debug_screen_func->rect(bx+3,by+1,bx+4,by+4,dkgray);
			}

			x = 4;
			y += 8*2; /* next line */

			/* effective PAL (after all remapping) */
			sprintf(tmp,"EPAL%u:",vga_8bit_dac?8:6);
			x = VGA_debug_screen_puts8(x,y,tmp,white) + 8;
			if (vga.mode == M_LIN8 || vga.mode == M_VGA) {
				VGA_debug_screen_func->rect(x-1,y,x,y+(4*4),dkgray);
				VGA_debug_screen_func->rect(x-1,y,x+(4*64),y+1,dkgray);
				VGA_debug_screen_func->rect(x-1,y+(4*4)-1,x+(4*64),y+(4*4),dkgray);
				for (unsigned int c=0;c < 256;c++) {
					const int bx = x+((c&63)*4),by = y+((c>>6)*4);
					VGA_debug_screen_func->rect(bx,by+1,bx+3,by+4,vga.dac.xlat32[c]); /* xlat32[] already contains the translated color */
					VGA_debug_screen_func->rect(bx+3,by+1,bx+4,by+4,dkgray);
				}
			}
			else {
				unsigned int colors = 16;

				if (!(vga.mode == M_TEXT || vga.mode == M_TANDY_TEXT || vga.mode == M_HERC_TEXT)) {
					unsigned char chk = vga.attr.color_plane_enable;

					if (vga.gfx.mode & 0x20/*CGA 4-color*/)
						chk |= (chk & 0x5u) << 1u; /* bit 0->1 and 2->3 */

					if (chk & 8) colors = 16;
					else if (chk & 4) colors = 8;
					else if (chk & 2) colors = 4;
					else colors = 2;
				}

				VGA_debug_screen_func->rect(x-1,y,x+(8*colors),y+1,dkgray);
				VGA_debug_screen_func->rect(x-1,y+7,x+(8*colors),y+8,dkgray);
				for (unsigned int c=0;c < colors;c++) {
					VGA_debug_screen_func->rect(x,y+1,x+7,y+7,vga.dac.xlat32[c]);
					VGA_debug_screen_func->rect(x+7,y+1,x+8,y+7,dkgray);
					x += 8;
				}
			}

			x = 4;
			y += 8*2; /* next line */

			if (IS_VGA_ARCH) {
				/* attribute controller PAL */
				x = VGA_debug_screen_puts8(x,y,"ACPAL:",white) + 8;
				VGA_debug_screen_func->rect(x-1,y,x+(8*16),y+1,dkgray);
				VGA_debug_screen_func->rect(x-1,y+7,x+(8*16),y+8,dkgray);
				for (unsigned int c=0;c < 16;c++) {
					const unsigned int idx = vga.attr.palette[c]&0x3F;
					const unsigned int color = SDL_MapRGB(
						sdl.surface->format,
						((vga.dac.rgb[idx].red << dacshift) & 0xFF),
						((vga.dac.rgb[idx].green << dacshift) & 0xFF),
						((vga.dac.rgb[idx].blue << dacshift) & 0xFF));
					VGA_debug_screen_func->rect(x,y+1,x+7,y+7,color);
					VGA_debug_screen_func->rect(x+7,y+1,x+8,y+7,dkgray);
					x += 8;
				}

				x += 8;

				sprintf(tmp,"CPE%x HPL%x YP%02x",vga.attr.color_plane_enable&0xF,vga.config.pel_panning&0xF,vga.config.hlines_skip); /* 4 bits, 4 bitplanes, one hex digit */
				x = VGA_debug_screen_puts8(x,y,tmp,white) + 8;

				x = 4;
				y += 8; /* next line */

				/* attribute controller PAL with color select and other in force */
				x = VGA_debug_screen_puts8(x,y,"CSPAL:",white) + 8;
				VGA_debug_screen_func->rect(x-1,y,x+(8*16),y+1,dkgray);
				VGA_debug_screen_func->rect(x-1,y+7,x+(8*16),y+8,dkgray);
				for (unsigned int c=0;c < 16;c++) {
					const unsigned int idx = vga.dac.combine[c]; /* vga_dac.cpp considers color select */
					const unsigned int color = SDL_MapRGB(
						sdl.surface->format,
						((vga.dac.rgb[idx].red << dacshift) & 0xFF),
						((vga.dac.rgb[idx].green << dacshift) & 0xFF),
						((vga.dac.rgb[idx].blue << dacshift) & 0xFF));
					VGA_debug_screen_func->rect(x,y+1,x+7,y+7,color);
					VGA_debug_screen_func->rect(x+7,y+1,x+8,y+7,dkgray);
					x += 8;
				}

				x += 8;

				sprintf(tmp,"PM%02x MD%02x CS%02x",vga.dac.pel_mask,vga.attr.mode_control,vga.attr.color_select);
				x = VGA_debug_screen_puts8(x,y,tmp,white) + 8;
			}

			/* point out where side debug info is */
			x = vga.draw.width;
			y = 0;

			x += 4;

			if (vga.mode == M_VGA || vga.mode == M_LIN8) {
				VGA_debug_screen_func->rect(x,y,x+256,y+8,white);
				VGA_debug_screen_puts8(x,y,"EPAL",0);
				x += 256;
				x += 4;
			}

			VGA_debug_screen_func->rect(x,y,x+(8*2),y+(8*2),white);
			VGA_debug_screen_puts8(x,y,"CS",0);
			VGA_debug_screen_puts8(x,y+8,"PL",0);
			x += 8*2;
			x += 4;
		}
	}
	else if (machine == MCH_EGA) {
		/* Everything on EGA goes through the Attribute Controller.
		 * Two palettes are shown because what's on the screen is also controlled by a register that masks off bitplanes.
		 * The Attribute Controller maps the 4-bit color code to a 6-bit TTL output on the EGA connector. For extra fun,
		 * the meaning of the pins changes depending on whether the card is emitting 200-line output compatible with a
		 * CGA monitor or 350-line output for an EGA monitor. In the 200-line mode only the 4 bits have meaning and they
		 * are handled the same as CGA IRGB output. In 350-line mode the 6 bits define one of 64 possible colors in the
		 * form rgbRGB as binary bits where the least signficant bits are "rgb" and most significant bits are "RGB".
		 * This is why you can't do more than 16 colors except in 350-line modes. */
		if (vga.draw.bpp == 8) { /* Doesn't use anything else */
			unsigned int dkgray = (egaMonitorMode() == EGA) ? 0x38 : 0x10;

			x = VGA_debug_screen_puts8(x,y,"ACPAL:",white) + 8;
			VGA_debug_screen_func->rect(x-1,y,x,y+7,dkgray/*dkgray*/);
			VGA_debug_screen_func->rect(x-1,y,x+(8*16),y+1,dkgray);
			VGA_debug_screen_func->rect(x-1,y+7,x+(8*16),y+8,dkgray);
			for (unsigned int c=0;c < 16;c++) {
				VGA_debug_screen_func->rect(x,y+1,x+7,y+7,vga.attr.palette[c]);
				VGA_debug_screen_func->rect(x+7,y+1,x+8,y+7,dkgray);
				x += 8;
			}

			x += 8;

			// show the normally invisible (in the emulator) overscan color
			x = VGA_debug_screen_puts8(x,y,"OVC:",white) + 8;
			sprintf(tmp,"%02X",vga.attr.overscan_color);
			x = VGA_debug_screen_puts8(x,y,tmp,white) + 8;

			sprintf(tmp,"YPN:%02x",vga.config.hlines_skip);
			x = VGA_debug_screen_puts8(x,y,tmp,white) + 8;

			// in 320-pixel wide modes both won't fit!
			x = 4;
			y += 8;

			x = VGA_debug_screen_puts8(x,y,"MDPAL:",white) + 8;
			VGA_debug_screen_func->rect(x-1,y,x,y+7,dkgray/*dkgray*/);
			VGA_debug_screen_func->rect(x-1,y,x+(8*16),y+1,dkgray);
			VGA_debug_screen_func->rect(x-1,y+7,x+(8*16),y+8,dkgray);
			for (unsigned int c=0;c < 16;c++) {
				VGA_debug_screen_func->rect(x,y+1,x+7,y+7,vga.attr.palette[c&vga.attr.color_plane_enable]);
				VGA_debug_screen_func->rect(x+7,y+1,x+8,y+7,dkgray);
				x += 8;
			}

			x += 8;

			sprintf(tmp,"CPE:%x HPEL:%x",vga.attr.color_plane_enable&0xF,vga.config.pel_panning&0xF); /* 4 bits, 4 bitplanes, one hex digit */
			x = VGA_debug_screen_puts8(x,y,tmp,white);

			/* point out where side debug info is */
			x = vga.draw.width;
			y = 0;

			x += 4;

			VGA_debug_screen_func->rect(x,y,x+(16*2),y+8,white);
			VGA_debug_screen_puts8(x,y,"MPAL",0);
			x += 16*2;

			x += 4;

			VGA_debug_screen_func->rect(x,y,x+8,y+(8*3),white);
			VGA_debug_screen_puts8(x,y,"C",0);
			VGA_debug_screen_puts8(x,y+8,"P",0);
			VGA_debug_screen_puts8(x,y+16,"E",0);
		}
	}
}

static void VGA_VerticalTimer(Bitu /*val*/) {
    double current_time = PIC_GetCurrentEventTime();

    dbg_event_maxscan = false;
    dbg_event_scanstep = false;
    dbg_event_hretrace = false;
    dbg_event_color_select = false;
    dbg_event_color_plane_enable = false;
    debugline_events.clear();

    if (IS_PC98_ARCH) {
        GDC_display_plane = GDC_display_plane_pending;
        pc98_update_display_page_ptr();
    }

    if (vga_render_on_demand)
        VGA_RenderOnDemandComplete();

    if (VGA_IsCaptureEnabled()) {
        if (VGA_IsCaptureInProgress()) {
            VGA_MarkCaptureInProgress(false);
            VGA_MarkCaptureAcquired();
        }

        VGA_MarkCaptureRetrace();
        VGA_CaptureStartNextFrame();
        if (!VGA_CaptureValidateCurrentFrame())
            VGA_CaptureMarkError();
    }

    vga.draw.hsync_events = 0;
    vga.draw.delay.framestart = current_time; /* used by port 3DAh, for example */
    vga_page_flip_occurred = false;
    vga.draw.has_split = false;
    vga_3da_polled = false;

    // FIXME: While this code is quite good at keeping time, I'm seeing drift "reset" back to
    //        14-30ms every video mode change. Is our INT 10h code that slow?
    /* compensate for floating point drift, make sure we're keeping the frame rate.
     * be very gentle about it. generally the drift is very small, and large adjustments can cause
     * DOS games dependent on vsync to fail/hang. */
    double shouldbe = (((double)vga_mode_frames_since_time_base * 1000.0) / vga_fps) + vga_mode_time_base;
    double vsync_err = shouldbe - current_time; /* < 0 too slow     > 0 too fast */
    double vsync_adj = vsync_err * 0.25;
    if (vsync_adj < -0.1) vsync_adj = -0.1;
    else if (vsync_adj > 0.1) vsync_adj = 0.1;

//  LOG_MSG("Vsync err %.6fms adj=%.6fms",vsync_err,vsync_adj);

    float vsynctimerval;
    float vdisplayendtimerval;
    if( vsync.manual ) {
        static float hack_memory = 0.0f;
        if( hack_memory > 0.0f ) {
            uservsyncjolt+=hack_memory;
            hack_memory = 0.0f;
        }

        float faithful_framerate_adjustment_delay = 0.0f;
        if( vsync.faithful ) {
            const float gfxmode_vsyncrate   = 1000.0f/vga.draw.delay.vtotal;
            const float user_vsyncrate      = 1000.0f/vsync.period;
            const float framerate_diff      = user_vsyncrate - gfxmode_vsyncrate;
            if( framerate_diff >= 0 ) {
                static float counter = 0.0f;
                // User vsync rate is greater than the target vsync rate
                const float adjustment_deadline = gfxmode_vsyncrate / framerate_diff;
                counter += 1.0f;
                if(counter >= adjustment_deadline) {
                    // double vsync duration this frame to resynchronize with the target vsync timing
                    faithful_framerate_adjustment_delay = vsync.period;
                    counter -= adjustment_deadline;
                }
            } else {
                // User vsync rate is less than the target vsync rate

                // I don't have a good solution for this right now.
                // I also don't have access to a 60Hz display for proper testing.
                // Making an instant vsync is more difficult than making a long vsync.. because
                // the emulated app's retrace loop must be able to detect that the retrace has both
                // begun and ended.  So it's not as easy as adjusting timer durations.
                // I think adding a hack to cause port 3da's code to temporarily force the
                // vertical retrace bit to cycle could work.. Even then, it's possible that
                // some shearing could be seen when the app draws two new frames during a single
                // host refresh.
                // Anyway, this could be worth dealing with for console ports since they'll be
                // running at either 60 or 50Hz (below 70Hz).
                /*
                const float adjustment_deadline = -(gfxmode_vsyncrate / framerate_diff);
                counter += 1.0f;
                if(counter >= adjustment_deadline) {
                    // nullify vsync duration this frame to resynchronize with the target vsync timing
                    // TODO(AUG): proper low user vsync rate synchronization
                    faithful_framerate_adjustment_delay = -uservsyncperiod + 1.2f;
                    vsync_hackval = 10;
                    hack_memory = -1.2f;
                    counter -= adjustment_deadline;
                }
                */
            }
        }

        const Bitu persistent_sync_update_interval = 100;
        static Bitu persistent_sync_counter = persistent_sync_update_interval;
        Bitu current_tick = GetTicks();
        static Bitu jolt_tick = 0;
        if( uservsyncjolt > 0.0f ) {
            jolt_tick = current_tick;

            // set the update counter to a low value so that the user will almost
            // immediately see the effects of an auto-correction.  This gives the
            // user a chance to compensate for it.
            persistent_sync_counter = 50;
        }

        float real_diff = 0.0f;
        if( vsync.persistent ) {
            if( persistent_sync_counter == 0 ) {
                float ticks_since_jolt = (signed long)current_tick - (signed long)jolt_tick;
                double num_host_syncs_in_those_ticks = floor(ticks_since_jolt / vsync.period);
                float diff_thing = ticks_since_jolt - (num_host_syncs_in_those_ticks * vsync.period);

                if( diff_thing > (vsync.period / 2.0f) ) real_diff = diff_thing - vsync.period;
                else real_diff = diff_thing;

//              LOG_MSG("diff is %f",real_diff);

                if( ((real_diff > 0.0f) && (real_diff < 1.5f)) || ((real_diff < 0.0f) && (real_diff > -1.5f)) )
                    real_diff = 0.0f;

                persistent_sync_counter = persistent_sync_update_interval;
            } else --persistent_sync_counter;
        }

//      vsynctimerval       = uservsyncperiod + faithful_framerate_adjustment_delay + uservsyncjolt;
        vsynctimerval       = vsync.period - (real_diff/1.0f);  // formerly /2.0f
        vsynctimerval       += faithful_framerate_adjustment_delay + uservsyncjolt;

        // be sure to provide delay between end of one refresh, and start of the next
//      vdisplayendtimerval = vsynctimerval - 1.18f;

        // be sure to provide delay between end of one refresh, and start of the next
        vdisplayendtimerval = vsynctimerval - (vga.draw.delay.vtotal - vga.draw.delay.vrstart);

        // in case some calculations above cause this.  this really shouldn't happen though.
        if( vdisplayendtimerval < 0.0f ) vdisplayendtimerval = 0.0f;

        uservsyncjolt = 0.0f;
    } else {
        // Standard vsync behaviour
        vsynctimerval       = (float)vga.draw.delay.vtotal;
        vdisplayendtimerval = (float)vga.draw.delay.vrstart;
    }

    {
        double fv;

        fv = vsynctimerval + vsync_adj;
        if (fv < 1) fv = 1;
        PIC_AddEvent(VGA_VerticalTimer,fv);
    }
    
    switch(machine) {
    case MCH_PCJR:
    case MCH_TANDY:
        // PCJr: Vsync is directly connected to the IRQ controller
        // Some earlier Tandy models are said to have a vsync interrupt too
        PIC_AddEvent(VGA_Other_VertInterrupt, (float)vga.draw.delay.vrstart, 1);
        PIC_AddEvent(VGA_Other_VertInterrupt, (float)vga.draw.delay.vrend, 0);
        // fall-through
    case MCH_AMSTRAD:
    case MCH_CGA:
    case MCH_MDA:
    case MCH_MCGA:
    case MCH_HERC:
        // MC6845-powered graphics: Loading the display start latch happens somewhere
        // after vsync off and before first visible scanline, so probably here
        VGA_DisplayStartLatch(0);
        break;
    case MCH_VGA:
        PIC_AddEvent(VGA_DisplayStartLatch, (float)vga.draw.delay.vrstart);
        PIC_AddEvent(VGA_PanningLatch, (float)vga.draw.delay.vrend);
        // EGA: 82c435 datasheet: interrupt happens at display end
        // VGA: checked with scope; however disabled by default by jumper on VGA boards
        // add a little amount of time to make sure the last drawpart has already fired
        PIC_AddEvent(VGA_VertInterrupt,(float)(vga.draw.delay.vdend + 0.005));
        break;
    case MCH_PC98:
        PIC_AddEvent(VGA_PanningLatch, (float)vga.draw.delay.vrend);
        PIC_AddEvent(VGA_VertInterrupt,(float)(vga.draw.delay.vrstart + 0.0001));
        break;
    case MCH_EGA:
        PIC_AddEvent(VGA_DisplayStartLatch, (float)vga.draw.delay.vrend);
        PIC_AddEvent(VGA_PanningLatch, (float)vga.draw.delay.vrend);
        PIC_AddEvent(VGA_VertInterrupt,(float)(vga.draw.delay.vdend + 0.005));
        break;
    default:
        //E_Exit("This new machine needs implementation in VGA_VerticalTimer too.");
        PIC_AddEvent(VGA_DisplayStartLatch, (float)vga.draw.delay.vrstart);
        PIC_AddEvent(VGA_PanningLatch, (float)vga.draw.delay.vrend);
        PIC_AddEvent(VGA_VertInterrupt,(float)(vga.draw.delay.vdend + 0.005));
        break;
    }
    // for same blinking frequency with higher frameskip
    if (IS_JEGA_ARCH) {
        ccount++;
        if (ccount>=0x20) {
            ccount=0;
            vga.draw.cursor.count++;
        }
    } else
        vga.draw.cursor.count++;

    if (IS_PC98_ARCH) {
        for (unsigned int i=0;i < 2;i++)
            pc98_gdc[i].cursor_advance();
    }

    //Check if we can actually render, else skip the rest
    if (vga.draw.vga_override || !RENDER_StartUpdate()) return;

    if (svgaCard == SVGA_S3Trio) {
        if (s3Card >= S3_ViRGE || s3Card == S3_Trio64V) {
            /* NTS: The Windows 3.1 S3 Trio64V+ driver appears to "shut down" the overlay by
             *      putting it in the upper left corner of the screen as a 7x8 window, and
             *      then setting the FIFO allocation so that all FIFO slots go to the
             *      primary stream, or to put it another way, by starving the secondary
             *      stream of any FIFO slots. If we don't check FIFO allocation, then
             *      the overlay will be "stuck" in the upper left hand corner when you close
             *      XingMPEG.
             *
             *      Of course other oddities happen in Windows 3.1, such as the hardware cursor
             *      turning blue when the overlay is loaded, and if you directly close the
             *      playback window without selecting "Close" from the file menu, the overlay
             *      remains on the desktop (XingMPEG devs probably missed that because they're
             *      using the color key feature to key against bright magenta, and the Windows
             *      desktop usually doesn't have that color). */
            if (vga.s3.streams.sswnd_height != 0 && vga.s3.streams.sswnd_start_x != 0 && vga.s3.streams.sswnd_start_y != 0 &&
                    (vga.s3.streams.blendctl_composemode == 0/*opaque secondary overlay*/ || vga.s3.streams.blendctl_composemode == 5/*color key secondary on primary*/)) {
                unsigned int ssbuf = (vga.s3.streams.ss_bufsel == 1) ? 1 : 0;
                S3SSdraw.startx = vga.s3.streams.sswnd_start_x-1; /* X coordinate written is X + 1 */
                S3SSdraw.starty = vga.s3.streams.sswnd_start_y-1; /* Y coordinate written is Y + 1 */
                S3SSdraw.endx = (vga.s3.streams.sswnd_start_x-1)+vga.s3.streams.sswnd_width+1; /* register is width - 1 */
                S3SSdraw.endy = (vga.s3.streams.sswnd_start_y-1)+vga.s3.streams.sswnd_height; /* height is written as-is */
                S3SSdraw.vmem_addr = vga.s3.streams.ss_fba[ssbuf] & ~7u; /* "must be quadword aligned" */
                S3SSdraw.stride = vga.s3.streams.ss_stride; /* datasheet doesn't say anything about alignment requirements here */
                S3SSdraw.pixfmt = vga.s3.streams.ssctl_sdif;
                S3SSdraw.filter = vga.s3.streams.ssctl_sfc;
                S3SSdraw.evf = vga.s3.streams.evf != 0u;
                S3SSdraw.vaccum = vga.s3.streams.dda_vaccum_iv;
                S3SSdraw.pscan = &S3SSdraw.tmpscan2;
                S3SSdraw.cscan = &S3SSdraw.tmpscan1;
                S3SSdraw.cscan_load = true;
                S3SSdraw.currentline = 0;
                S3SSdraw.draw = true;
            }
            else {
                S3SSdraw.draw = false;
            }
        }
    }

    vga.draw.address_line = vga.config.hlines_skip;
    if (IS_EGAVGA_ARCH) VGA_Update_SplitLineCompare();
    vga.draw.address = vga.config.real_start;
    vga.draw.byte_panning_shift = 0;

    switch (vga.mode) {
    case M_EGA:
        if (vga.mem.memmask >= 0x1FFFFu) {
            /* EGA/VGA Mode control register 0x17 effects on the linear mask */
            if ((vga.crtc.maximum_scan_line&0x1fu) == 0) {
                /* WARNING: These hacks only work IF max scanline value == 0 (no doubling).
                 *          The bit 0 (bit 13 replacement) mode here is needed for
                 *          Prehistorik 2 to display it's mode select/password entry screen
                 *          (the one with the scrolling background of various cavemen) */

                /* if bit 0 is cleared, CGA compatible addressing is enabled.
                 * bit 13 is replaced by bit 0 of the row counter */
                if (!(vga.crtc.mode_control&0x1u)) vga.draw.linear_mask &= ~0x8000u;
                else vga.draw.linear_mask |= 0x8000u;

                /* if bit 1 is cleared, Hercules compatible addressing is enabled.
                 * bit 14 is replaced by bit 0 of the row counter */
                if (!(vga.crtc.mode_control&0x2u)) vga.draw.linear_mask &= ~0x10000u;
                else vga.draw.linear_mask |= 0x10000u;
            }
            else {
                if ((vga.crtc.mode_control&0x03u) != 0x03u) {
                    LOG(LOG_VGAMISC,LOG_WARN)("Guest is attempting to use CGA/Hercules compatible display mapping in M_EGA mode with max_scanline != 0, which is not yet supported");
                }
            }
        }
        /* fall through */
    case M_LIN4:
        vga.draw.byte_panning_shift = 4u;
        vga.draw.address += vga.draw.bytes_skip;
        vga.draw.address *= vga.draw.byte_panning_shift;
        break;
    case M_VGA:
        /* TODO: Various SVGA chipsets have a bit to enable/disable 256KB wrapping */
        vga.draw.linear_mask = 0x3ffffu;
        if (svgaCard == SVGA_TsengET3K || svgaCard == SVGA_TsengET4K) {
            if (vga.config.addr_shift == 1) /* NTS: Remember the ET4K steps by 4 pixels, one per byteplane, treats BYTE and DWORD modes the same */
                vga.draw.address *= 2u;
        }
        else if (machine == MCH_MCGA) {
            vga.draw.linear_mask = 0xffffu;
            vga.draw.address *= 2u;
            break;// don't fall through
        }
        else {
            vga.draw.address *= (Bitu)1u << (Bitu)vga.config.addr_shift; /* NTS: Remember the bizarre 4 x 4 mode most SVGA chipsets do */
        }
        /* fall through */
    case M_LIN8:
        if (svgaCard == SVGA_TsengET3K || svgaCard == SVGA_TsengET4K) {
            // HACK: Disable 256KB VGA masking if we detect the SVGA modes.
            if (vga.attr.mode_control & 1) {
                if (vga.gfx.mode & 0x40) {
                    if (!(vga.attr.mode_control & 0x40)) {
                        vga.draw.linear_mask = vga.mem.memmask; // SVGA text mode
                    }
                }
            }
        }
        /* fall through */
    case M_LIN15:
    case M_LIN16:
    case M_LIN24:
    case M_LIN32:
        vga.draw.byte_panning_shift = 4;
        vga.draw.address += vga.draw.bytes_skip;
        vga.draw.address *= vga.draw.byte_panning_shift;
        vga.draw.address += vga.draw.panning;
        break;
    case M_PACKED4:
        vga.draw.byte_panning_shift = 4u;
        vga.draw.address += vga.draw.bytes_skip;
        vga.draw.address *= vga.draw.byte_panning_shift;
        break;
    case M_PC98:
        vga.draw.linear_mask = 0xfff; // 1 page
        vga.draw.byte_panning_shift = 2;
        vga.draw.address += vga.draw.bytes_skip;
        vga.draw.cursor.address = vga.config.cursor_start;
        break;
    case M_TEXT:
        vga.draw.byte_panning_shift = 2;
        vga.draw.address += vga.draw.bytes_skip;
        // fall-through
    case M_TANDY_TEXT:
    case M_HERC_TEXT:
        if (machine==MCH_HERC || machine==MCH_MDA) vga.draw.linear_mask = 0xfff; // 1 page
        else if (IS_EGAVGA_ARCH || machine == MCH_MCGA) {
            if (vga.config.compatible_chain4 || svgaCard == SVGA_None)
                vga.draw.linear_mask = vga.mem.memmask & 0x3ffff;
            else
                vga.draw.linear_mask = vga.mem.memmask; // SVGA text mode
        }
        else vga.draw.linear_mask = 0x3fff; // CGA, Tandy 4 pages
        if (IS_EGAVGA_ARCH) {
            vga.draw.cursor.address=vga.config.cursor_start<<vga.config.addr_shift;
            vga.draw.address <<= vga.config.addr_shift;
        }
        else {
            vga.draw.cursor.address=vga.config.cursor_start*2;
            vga.draw.address *= 2;
        }

        /* check for blinking and blinking change delay */
        FontMask[1]=(vga.draw.blinking & (unsigned int)(vga.draw.cursor.count >> 4u)) ?
            0 : 0xffffffff;
        /* if blinking is enabled, 'blink' will toggle between true
         * and false. Otherwise it's true */
        vga.draw.blink = ((vga.draw.blinking & (unsigned int)(vga.draw.cursor.count >> 4u))
            || !vga.draw.blinking) ? true:false;
        break;
    case M_HERC_GFX:
    case M_CGA4:
    case M_CGA2:
    case M_DCGA:
        vga.draw.address=(vga.draw.address*2u)&0x1fffu;
        break;
    case M_AMSTRAD: // Base address: No difference?
        vga.draw.address=(vga.draw.address*2u)&0xffffu;
        break;
    case M_CGA16:
    case M_TANDY2:case M_TANDY4:case M_TANDY16:
        vga.draw.address *= 2u;
        break;
    default:
        break;
    }

    if (IS_EGAVGA_ARCH)
        vga.draw.planar_mask = vga.draw.linear_mask >> 2;
    else
        vga.draw.planar_mask = vga.draw.linear_mask >> 1;

    /* ET4000 High Sierra DAC programs can change SVGA mode */
    if ((vga.mode == M_LIN15 || vga.mode == M_LIN16) && (svgaCard == SVGA_TsengET3K || svgaCard == SVGA_TsengET4K)) {
        if (et4k_highcolor_half_pixel_rate())
            VGA_DrawLine=VGA_Draw_LIN16_Line_2x;
        else
            VGA_DrawLine=VGA_Draw_LIN16_Line_HWMouse;
    }

    // check if some lines at the top off the screen are blanked
    float draw_skip = 0.0;
    if (GCC_UNLIKELY(vga.draw.vblank_skip > 0)) {
        draw_skip = (float)(vga.draw.delay.htotal * vga.draw.vblank_skip);
        vga.draw.address_line += (vga.draw.vblank_skip % vga.draw.address_line_total);
        vga.draw.address += vga.draw.address_add * (vga.draw.vblank_skip / vga.draw.address_line_total);
    }

    /* do VGA split now if line compare <= 0. NTS: vga.draw.split_line is defined as Bitu (unsigned integer) so we need the typecast. */
    if (GCC_UNLIKELY((Bits)vga.draw.split_line <= 0) && machine != MCH_PC98) {
        VGA_ProcessSplit();

        /* if vblank_skip != 0, line compare can become a negative value! Fixes "Warlock" 1992 demo by Warlock */
        if (GCC_UNLIKELY((Bits)vga.draw.split_line < 0)) {
            vga.draw.address_line += (Bitu)(-((Bits)vga.draw.split_line) % (Bits)vga.draw.address_line_total);
            vga.draw.address += vga.draw.address_add * (Bitu)(-((Bits)vga.draw.split_line) / (Bits)vga.draw.address_line_total);
        }
    }

    // NTS: To be moved
    if (autosave_second>0&&enable_autosave) {
        uint32_t ticksNew=GetTicks();
        if (ticksNew-ticksPrev>(unsigned int)autosave_second*1000) {
            auto_save_state=true;
            int index=0;
            for (int i=1; i<10&&i<=autosave_count; i++) if (autosave_name[i].size()&&!strcasecmp(RunningProgram, autosave_name[i].c_str())) index=i;
            if (autosave_start[index]>=1&&autosave_start[index]<=100) {
                if (autosave_end[index]>=autosave_start[index]&&autosave_end[index]<=100&&autosave_end[index]>autosave_start[index]) {
                    if (autosave_end[index]>autosave_last[index]&&autosave_last[index]>=autosave_start[index]) autosave_last[index]++;
                    else autosave_last[index]=autosave_start[index];
                } else autosave_last[index]=autosave_start[index];
                int state = (int)GetGameState_Run();
                SetGameState_Run(autosave_last[index]-1);
                SaveGameState_Run();
                SetGameState_Run(state);
            } else if (!autosave_start[index]) {
                SaveGameState_Run();
                autosave_last[index]=(int)(GetGameState_Run()+1);
            }
            auto_save_state=false;
            ticksPrev=ticksNew;
        }
    }

    int sec = static_cast<Section_prop *>(control->GetSection("cpu"))->Get_int("stop turbo after second");
    if (ticksLocked && turbolasttick && sec>0 && GetTicks()-turbolasttick>=1000*sec) DOSBOX_UnlockSpeed2(true);

#if defined(USE_TTF)
    if (ttf.inUse && checkcol && vga.draw.address_add != ttf.cols * 2) ttf_switch_off(checkcol==2);
    if (ttf.inUse) {
        checkcol = 0;
        GFX_StartUpdate(render.scale.outWrite, render.scale.outPitch);
        vga.draw.blink = ((vga.draw.blinking & (GetTicks()/300)) || !vga.draw.blinking) ? true : false;	// eventually blink about thrice per second
        vga.draw.cursor.address = vga.config.cursor_start*2;
        Bitu vidstart = vga.config.real_start + vga.draw.bytes_skip;
        vidstart *= 2;
        ttf_cell* draw = newAttrChar;
        ttf_cell* drawc = curAttrChar;
        uint16_t uname[4];

        if (IS_PC98_ARCH) {
            const uint16_t* charram = (uint16_t*)&vga.draw.linear_base[0x0000];         // character data
            const uint16_t* attrram = (uint16_t*)&vga.draw.linear_base[0x2000];         // attribute data

            // TTF output only looks at VGA emulation state for cursor status,
            // which PC-98 mode does not update.
            vga.draw.cursor.enabled = pc98_gdc[GDC_MASTER].cursor_enable;

            bool null = true;
            for (Bitu blocks = ttf.cols * ttf.lins; blocks; blocks--) {
                bool dbw=false;

                *draw = ttf_cell();
                (*draw).selected = (*drawc).selected;

                /* NTS: PC-98 hardware does not require both cells of a double-wide to match,
                 *      in fact if the hardware sees a double-wide in the first cell it will just render the double-wide
                 *      for two cells and ignore the second cell. There are some exceptions though, including the custom
                 *      modificable cells in RAM (responsible for such bugs as the Touhou Project ~idnight level name display bug). */
                if (*charram & 0xFF00u) {
                    if ((*charram & 0x7Cu) == 0x08u && (*charram%0xff) - (*charram/0x100) == 0xB) {
                        /* Single wide, yet DBCS encoding.
                         * This includes proprietary box characters specific to PC-98 */
                        // Convert box characters to Unicode (incomplete for now)
                        (*draw).chr = ' ';
                        uint8_t val = *charram/0x100+31;
                        for (auto it = pc98boxdrawmap.begin(); it != pc98boxdrawmap.end(); ++it) {
                            if (it->second == val) {
                                dos.loaded_codepage = 437;
                                char text[3];
                                text[0]=it->first;
                                text[1]=0;
                                uname[0]=0;
                                uname[1]=0;
                                CodePageGuestToHostUTF16(uname,text);
                                dos.loaded_codepage = 932;
                                if (uname[0]!=0&&uname[1]==0) {
                                    (*draw).chr=uname[0];
                                    (*draw).unicode=1;
                                }
                                break;
                            }
                        }
                    }
                    else {
                        uint16_t ch = *charram&0x7F7Fu;
                        uint8_t j1=(ch%0x100)+0x20, j2=ch/0x100;
                        if (j1>32&&j1<127&&j2>32&&j2<127) {
                            char text[3];
                            text[0]=(j1+1)/2+(j1<95?112:176);
                            text[1]=j2+(j1%2?31+(j2/96):126);
                            text[2]=0;
                            uname[0]=0;
                            uname[1]=0;
                            if (del_flag && text[1] == 0x7F) text[1]++;
                            CodePageGuestToHostUTF16(uname,text);
                            if (uname[0]!=0&&uname[1]==0) {
                                (*draw).chr=uname[0];
                                (*draw).doublewide=1;
                                (*draw).unicode=1;
                                dbw=true;
                            }
                            else {
                                (*draw).chr=' ';
                            }
                        } else {
                            (*draw).chr=' ';
                        }
                    }
                } else
                    (*draw).chr = *charram & 0xFF;
                if ((*draw).chr && (*draw).chr != 0x20) null = false;
                charram++;

                Bitu attr = *attrram;
                attrram++;
                // for simplicity at this time, just map PC-98 attributes to VGA colors. Wengier and I can clean this up later --J.C.
                Bitu background = 0;
                Bitu foreground = 0;
                // PC-98 does not use RGB, it uses GRB. Remap accordingly.
                if (attr & 0x80) foreground += 2;
                if (attr & 0x40) foreground += 4;
                if (attr & 0x20) foreground += 1;
                if (foreground) foreground += 8; // everything is fullbright on PC-98

                if (attr & 8) {//underline
                    // TODO
                }
                if (attr & 4) {//reverse
                    background = foreground;
                    foreground = 0;
                }
                if (attr & 2) {//blink
                    // TODO
                }
                if (!(attr & 1)) {//invisible
                    (*draw).chr = 0x20;
                }
                (*draw).fg = foreground;
                (*draw).bg = background;
                draw++;
                drawc++;

                if (dbw) {
                    /* extra cell. Should not draw */
                    *draw = ttf_cell();
                    (*draw).selected = (*drawc).selected;
                    (*draw).skipped = 1;
                    (*draw).chr = 'x'; // should not see this
                    (*draw).fg = 4|8; // bright red, in case this is visible
                    (*draw).bg = 4; // dark red, in case this is visible
                    draw++;
                    drawc++;
                    charram++;
                    attrram++;
                    if (blocks != 0) blocks--; /* careful! The for loop is written to stop when blocks == 0 */
                }
            }
            if (null && pc98_gdc[GDC_SLAVE].display_enable) ttf_switch_off(false);
        } else if (CurMode&&CurMode->type==M_TEXT) {
            bool dbw, dex, bd[txtMaxCols];
            if (IS_EGAVGA_ARCH) {
                for (Bitu row=0;row < ttf.lins;row++) {
                    const uint32_t* vidmem = ((uint32_t*)vga.draw.linear_base)+vidstart;	// pointer to chars+attribs (EGA/VGA planar memory)
                    uint16_t last = 0;
                    for (int i=0; i<txtMaxCols; i++) bd[i] = false;
                    dbw=dex=false;
                    for (Bitu col=0;col < ttf.cols;col++) {
                        // NTS: Note this assumes EGA/VGA text mode that uses the "Odd/Even" mode memory mapping scheme to present video memory
                        //      to the CPU as if CGA compatible text mode. Character data on plane 0, attributes on plane 1.
                        *draw = ttf_cell();
                        (*draw).selected = (*drawc).selected;
                        (*draw).chr = *vidmem & 0xFF;
                        if (dex) {
							checkChar(col, draw, last);
                            dbw=dex=false;
                        } else if (dbw) {
                            (*draw).skipped = 1;
                            dbw=dex=false;
                        } else if (isDBCSCP() && dbcs_sbcs && col==ttf.cols-1 && isKanji2((*draw).chr) && bd[ttf.cols-2]) {
                            bd[col]=true;
                            (*draw).boxdraw = 1;
                        } else if (isDBCSCP() && dbcs_sbcs && col<ttf.cols-1 && isKanji1((*draw).chr) && isKanji2(*(vidmem+2))) {
                            bool boxv = autoboxdraw && ((row < ttf.lins-2 && CheckBoxDrawingV((*draw).chr, *(vidmem+vga.draw.address_add), *(vidmem+2*vga.draw.address_add), *(vidmem+2), *(vidmem+2+vga.draw.address_add), *(vidmem+2+2*vga.draw.address_add), col?*(vidmem-2):0, col?*(vidmem-2+vga.draw.address_add):0, col?*(vidmem-2+2*vga.draw.address_add):0, col < ttf.cols-2?*(vidmem+4):0, col < ttf.cols-2?*(vidmem+4+vga.draw.address_add):0, col < ttf.cols-2?*(vidmem+4+2*vga.draw.address_add):0, row?*(vidmem-vga.draw.address_add):0, row < ttf.lins-3?*(vidmem+3*vga.draw.address_add):0, row?*(vidmem+2-vga.draw.address_add):0, row < ttf.lins-3?*(vidmem+2+3*vga.draw.address_add):0)) ||
                                                        (row && row < ttf.lins-1 && CheckBoxDrawingV(*(vidmem-vga.draw.address_add), (*draw).chr, *(vidmem+vga.draw.address_add), *(vidmem+2-vga.draw.address_add), *(vidmem+2), *(vidmem+2+vga.draw.address_add), col?*(vidmem-2-vga.draw.address_add):0, col?*(vidmem-2):0, col?*(vidmem-2+vga.draw.address_add):0, col < ttf.cols-2?*(vidmem+4-vga.draw.address_add):0, col < ttf.cols-2?*(vidmem+4):0, col < ttf.cols-2?*(vidmem+4+vga.draw.address_add):0, row > 1?*(vidmem-2*vga.draw.address_add):0, row < ttf.lins-2?*(vidmem+2*vga.draw.address_add):0, row > 1?*(vidmem+2-2*vga.draw.address_add):0, row < ttf.lins-2?*(vidmem+2+2*vga.draw.address_add):0)) ||
                                                        (row > 1 && CheckBoxDrawingV(*(vidmem-2*vga.draw.address_add), *(vidmem-vga.draw.address_add), (*draw).chr, *(vidmem+2-2*vga.draw.address_add), *(vidmem+2-vga.draw.address_add), *(vidmem+2), col?*(vidmem-2-2*vga.draw.address_add):0, col?*(vidmem-2-vga.draw.address_add):0, col?*(vidmem-2):0, col < ttf.cols-2?*(vidmem+4-2*vga.draw.address_add):0, col < ttf.cols-2?*(vidmem+4-vga.draw.address_add):0, col?*(vidmem+4):0, row > 2?*(vidmem-3*vga.draw.address_add):0, row < ttf.lins-1?*(vidmem+vga.draw.address_add):0, row > 2?*(vidmem+2-3*vga.draw.address_add):0, row < ttf.lins-1?*(vidmem+2+vga.draw.address_add):0)));
                            if (boxv) (*draw).boxdraw = 1;
                            else {
                                bool boxdefault = (!autoboxdraw || col>=ttf.cols-3) && !bd[col];
                                if (!boxdefault && col<ttf.cols-3) {
                                    if (CheckBoxDrawing((uint8_t)((*draw).chr), (uint8_t)*(vidmem+2), (uint8_t)*(vidmem+4), (uint8_t)*(vidmem+6))) {
                                        bd[col]=bd[col+1]=bd[col+2]=bd[col+3]=true;
                                    } else if (ttf.cols >= 80 && col > 70 && col < 75 && row < ttf.lins-1 && CheckBoxDrawLast(col, (*draw).chr, *(vidmem+2), *(vidmem+4), *(vidmem+6), *(vidmem+8), *(vidmem+10), *(vidmem+12), *(vidmem+14), *(vidmem+16)))
                                        bd[col]=bd[col+1]=bd[col+2]=bd[col+3]=bd[col+4]=bd[col+5]=bd[col+6]=bd[col+7]=bd[col+8]=true;
                                    else if (!bd[col])
                                        boxdefault=true;
                                }
                                if (autoboxdraw && bd[col]) (*draw).boxdraw = 1;
                                if (boxdefault) {
                                    char text[3];
                                    text[0]=(*draw).chr & 0xFF;
                                    text[1]=*(vidmem+2) & 0xFF;
                                    text[2]=0;
                                    uname[0]=0;
                                    uname[1]=0;
                                    if ((IS_JDOSV || dos.loaded_codepage == 932) && del_flag && text[1] == 0x7F) text[1]++;
                                    CodePageGuestToHostUTF16(uname,text);
                                    if (autoboxdraw&&row&&col>3&&(((uint8_t)*(vidmem-4)==177||(uint8_t)*(vidmem-4)==254)&&(uint8_t)*(vidmem-2)==16&&(uint8_t)text[0]==196&&(uint8_t)text[1]==217)||((uint8_t)*(vidmem-4)==176&&(uint8_t)*(vidmem-2)==176&&(uint8_t)text[0]==176&&(uint8_t)text[1]==179)) {
                                        boxdefault=false;
                                        (*draw).boxdraw = 1;
                                    } else
                                        handleChar(uname, draw, dbw, dex, last);
                                }
                            }
                        } else if (isDBCSCP() && (!dbcs_sbcs||dos.loaded_codepage!=932||(dos.loaded_codepage==932&&!halfwidthkana)) && (*draw).chr>=0x80 && (*draw).chr<=0xFF)
                            (*draw).boxdraw = 1;
                        Bitu attr = (*vidmem >> 8u) & 0xFFu;
                        vidmem+=2; // because planar EGA/VGA, and odd/even mode as real hardware arranges alphanumeric mode in VRAM
                        Bitu background = attr >> 4;
                        if (vga.draw.blinking && !ttf_dosv)							// if blinking is enabled bit7 is not mapped to attributes
                            background &= 7;
                        // choose foreground color if blinking not set for this cell or blink on
                        Bitu foreground = (vga.draw.blink || (!(attr&0x80)) || ttf_dosv) ? (attr&0xf) : background;
                        // How about underline?
                        (*draw).fg = foreground;
                        (*draw).bg = background;
                        draw++;
                        drawc++;
                    }
                    vidstart += vga.draw.address_add;
                }
            } else {
                for (Bitu row=0;row < ttf.lins;row++) {
                    const uint16_t* vidmem = (uint16_t*)VGA_Text_Memwrap(vidstart);	// pointer to chars+attribs (EGA/VGA planar memory)
                    uint16_t last = 0;
                    for (int i=0; i<txtMaxCols; i++) bd[i] = false;
                    dbw=dex=false;
                    for (Bitu col=0;col < ttf.cols;col++) {
                        *draw = ttf_cell();
                        (*draw).selected = (*drawc).selected;
                        (*draw).chr = *vidmem & 0xFF;
                        if (dex) {
							checkChar(col, draw, last);
                            dbw=dex=false;
                        } else if (dbw) {
                            (*draw).skipped = 1;
                            dbw=dex=false;
                        } else if (isDBCSCP() && dbcs_sbcs && col==ttf.cols-1 && isKanji2((*draw).chr) && bd[ttf.cols-2]) {
                            bd[col]=true;
                            (*draw).boxdraw = 1;
                        } else if (isDBCSCP() && dbcs_sbcs && col<ttf.cols-1 && isKanji1((*draw).chr) && isKanji2(*(vidmem+1))) {
                            bool boxv = autoboxdraw && ((row < ttf.lins-2 && CheckBoxDrawingV((*draw).chr, *(vidmem+vga.draw.address_add/2), *(vidmem+vga.draw.address_add), *(vidmem+1), *(vidmem+1+vga.draw.address_add/2), *(vidmem+1+vga.draw.address_add), col?*(vidmem-1):0, col?*(vidmem-1+vga.draw.address_add/2):0, col?*(vidmem-1+vga.draw.address_add):0, col < ttf.cols-2?*(vidmem+2):0, col < ttf.cols-2?*(vidmem+2+vga.draw.address_add/2):0, col < ttf.cols-2?*(vidmem+2+vga.draw.address_add):0, row?*(vidmem-vga.draw.address_add/2):0, row < ttf.lins-3?*(vidmem+3*vga.draw.address_add/2):0, row?*(vidmem+1-vga.draw.address_add/2):0, row < ttf.lins-3?*(vidmem+1+3*vga.draw.address_add/2):0)) ||
                                                        (row && row < ttf.lins-1 && CheckBoxDrawingV(*(vidmem-vga.draw.address_add/2), (*draw).chr, *(vidmem+vga.draw.address_add/2), *(vidmem+1-vga.draw.address_add/2), *(vidmem+1), *(vidmem+1+vga.draw.address_add/2), col?*(vidmem-1-vga.draw.address_add/2):0, col?*(vidmem-1):0, col?*(vidmem-1+vga.draw.address_add/2):0, col < ttf.cols-2?*(vidmem+2-vga.draw.address_add/2):0, col < ttf.cols-2?*(vidmem+2):0, col < ttf.cols-2?*(vidmem+2+vga.draw.address_add/2):0, row > 1?*(vidmem-vga.draw.address_add):0, row < ttf.lins-2?*(vidmem+vga.draw.address_add):0, row > 1?*(vidmem+1-vga.draw.address_add):0, row < ttf.lins-2?*(vidmem+1+vga.draw.address_add):0)) ||
                                                        (row > 1 && CheckBoxDrawingV(*(vidmem-vga.draw.address_add), *(vidmem-vga.draw.address_add/2), (*draw).chr, *(vidmem+1-vga.draw.address_add), *(vidmem+1-vga.draw.address_add/2), *(vidmem+1), col?*(vidmem-1-vga.draw.address_add):0, col?*(vidmem-1-vga.draw.address_add/2):0, col?*(vidmem-1):0, col < ttf.cols-2?*(vidmem+2-vga.draw.address_add):0, col < ttf.cols-2?*(vidmem+2-vga.draw.address_add/2):0, col?*(vidmem+2):0, row > 2?*(vidmem-3*vga.draw.address_add/2):0, row < ttf.lins-1?*(vidmem+vga.draw.address_add/2):0, row > 2?*(vidmem+1-3*vga.draw.address_add/2):0, row < ttf.lins-1?*(vidmem+1+vga.draw.address_add/2):0)));
                            if (boxv) (*draw).boxdraw = 1;
                            else {
                                bool boxdefault = (!autoboxdraw || col>=ttf.cols-3) && !bd[col];
                                if (!boxdefault && col<ttf.cols-3) {
                                    if (CheckBoxDrawing((uint8_t)((*draw).chr), (uint8_t)*(vidmem+1), (uint8_t)*(vidmem+2), (uint8_t)*(vidmem+3)))
                                        bd[col]=bd[col+1]=bd[col+2]=bd[col+3]=true;
                                    else if (ttf.cols >= 80 && col > 70 && col < 75 && row < ttf.lins-1 && CheckBoxDrawLast(col, (*draw).chr, *(vidmem+1), *(vidmem+2), *(vidmem+3), *(vidmem+4), *(vidmem+5), *(vidmem+6), *(vidmem+7), *(vidmem+8)))
                                        bd[col]=bd[col+1]=bd[col+2]=bd[col+3]=bd[col+4]=bd[col+5]=bd[col+6]=bd[col+7]=bd[col+8]=true;
                                    else if (!bd[col])
                                        boxdefault=true;
                                }
                                if (autoboxdraw && bd[col]) (*draw).boxdraw = 1;
                                if (boxdefault) {
                                    char text[3];
                                    text[0]=(*draw).chr & 0xFF;
                                    text[1]=*(vidmem+1) & 0xFF;
                                    text[2]=0;
                                    uname[0]=0;
                                    uname[1]=0;
                                    if ((IS_JDOSV || dos.loaded_codepage == 932) && del_flag && text[1] == 0x7F) text[1]++;
                                    CodePageGuestToHostUTF16(uname,text);
                                    if (autoboxdraw&&row&&col>3&&(((uint8_t)*(vidmem-2)==177||(uint8_t)*(vidmem-2)==254)&&(uint8_t)*(vidmem-1)==16&&(uint8_t)text[0]==196&&(uint8_t)text[1]==217)||((uint8_t)*(vidmem-2)==176&&(uint8_t)*(vidmem-1)==176&&(uint8_t)text[0]==176&&(uint8_t)text[1]==179)) {
                                        boxdefault=false;
                                        (*draw).boxdraw = 1;
                                    } else
                                        handleChar(uname, draw, dbw, dex, last);
                                }
                            }
                        } else if (isDBCSCP() && (!dbcs_sbcs||dos.loaded_codepage!=932||(dos.loaded_codepage==932&&!halfwidthkana)) && (*draw).chr>=0x80 && (*draw).chr<=0xFF)
                            (*draw).boxdraw = 1;
                        Bitu attr = (*vidmem >> 8u) & 0xFFu;
                        vidmem++;
                        Bitu background = attr >> 4;
                        if (vga.draw.blinking)									// if blinking is enabled bit7 is not mapped to attributes
                            background &= 7;
                        // choose foreground color if blinking not set for this cell or blink on
                        Bitu foreground = (vga.draw.blink || (!(attr&0x80))) ? (attr&0xf) : background;
                        // How about underline?
                        (*draw).fg = foreground;
                        (*draw).bg = machine == MCH_MDA || machine == MCH_HERC ? 0 : background;
                        draw++;
                        drawc++;
                    }
                    vidstart += vga.draw.address_add;
                }
            }
        }
        if (ttf.inUse) {
            RENDER_EndUpdate(false);
            return;
        }
    }
#endif

    if (video_debug_overlay && render.src.height > vga.draw.height && vga.draw.bpp == render.src.bpp)
        VGA_debug_screen_resize(render.src.width,render.src.height - vga.draw.height,vga.draw.bpp);
    else
        VGA_debug_screen_free();

    if (video_debug_overlay && VGA_debug_screen) {
	VGA_debug_screen_func->clear(0);
	VGA_sof_debug_video_info();
    }

    // add the draw event
    switch (vga.draw.mode) {
    case DRAWLINE:
    case EGALINE:
        if (GCC_UNLIKELY(vga.draw.lines_done < vga.draw.lines_total)) {
            LOG(LOG_VGAMISC,LOG_NORMAL)( "Lines left: %d", 
                (int)(vga.draw.lines_total-vga.draw.lines_done));
            if (vga.draw.mode==EGALINE) PIC_RemoveEvents(VGA_DrawEGASingleLine);
            else PIC_RemoveEvents(VGA_DrawSingleLine);
            vga_mode_frames_since_time_base++;

            if (VGA_IsCaptureEnabled())
                VGA_ProcessScanline(NULL);

            RENDER_EndUpdate(true);
        }
        vga.draw.lines_done = 0;
        if (!vga_render_on_demand) {
            if (vga.draw.mode==EGALINE)
                PIC_AddEvent(VGA_DrawEGASingleLine,(float)(vga.draw.delay.htotal/4.0 + draw_skip));
            else
                PIC_AddEvent(VGA_DrawSingleLine,(float)(vga.draw.delay.htotal/4.0 + draw_skip));
	}
        break;
    }
}

void VGA_CheckScanLength(void) {
    switch (vga.mode) {
    case M_EGA:
    case M_LIN4:
        if ((machine==MCH_EGA)&&(vga.crtc.mode_control&0x8))
            vga.draw.address_add=vga.config.scan_len*16; // TODO
        else
            vga.draw.address_add=vga.config.scan_len*(8u<<(unsigned int)vga.config.addr_shift);

        if (IS_EGA_ARCH && (vga.seq.clocking_mode&4))
            vga.draw.address_add*=2;
        break;
    case M_PACKED4:
        vga.draw.address_add=vga.config.scan_len*8;
        break;
    case M_VGA:
    case M_LIN8:
    case M_LIN15:
    case M_LIN16:
    case M_LIN24:
    case M_LIN32:
        if (vga.mode == M_VGA) {
            if (svgaCard == SVGA_TsengET3K || svgaCard == SVGA_TsengET4K) {
                /* Observed ET4000AX behavior:
                 *    byte mode OR dword mode scans 256-color 4-pixel groups byte by byte from
                 *    planar RAM. word mode scans every other byte (skips by two).
                 *    We can just scan the buffer normally as linear because of the address
                 *    translation carried out by the ET4000 in chained mode:
                 *
                 *    plane = (addr & 3)   addr = (addr >> 2)
                 *
                 *    TODO: Validate that this is correct. */
                vga.draw.address_add=vga.config.scan_len*((vga.config.addr_shift == 1)?16:8);
            }
            else if (machine == MCH_MCGA) {
                vga.draw.address_add=vga.draw.blocks*4;
            }
            else {
                /* Most (almost ALL) VGA clones render chained modes as 4 8-bit planes one DWORD apart.
                 * They all act as if writes to chained VGA memory are translated as:
                 * addr = ((addr & ~3) << 2) + (addr & 3) */
                vga.draw.address_add=(unsigned int)vga.config.scan_len*((unsigned int)(2*4)<<(unsigned int)vga.config.addr_shift);
            }
        }
        else {
            /* the rest (SVGA modes) can be rendered with sanity */
            if (svgaCard == SVGA_TsengET3K || svgaCard == SVGA_TsengET4K || svgaCard == SVGA_S3Trio) {
                // Real hardware testing (Tseng ET4000) shows that in SVGA modes the
                // standard VGA byte/word/dword mode bits have no effect.
                //
                // This was verified by using TMOTSENG.EXE on real hardware, setting
                // mode 0x2F (640x400 256-color mode) and then playing with the
                // word/dword/byte mode bits.
                //
                // Also noted is that the "count memory clock by 4" bit is set. If
                // you clear it, the hardware acts as if it completes the scanline
                // early and 3/4ths of the screen is the last 4-pixel block repeated.
                //
                // S3 Trio: Testing on a real S3 Virge PCI card shows that the
                //          byte/word/dword bits have no effect on SVGA modes other
                //          than the 16-color 800x600 SVGA mode.
                vga.draw.address_add=vga.config.scan_len*(2u<<2u);
            }
            else {
                // Other cards (?)
                vga.draw.address_add=vga.config.scan_len*(2u<<vga.config.addr_shift);
            }
        }
        break;
    case M_PC98:
        vga.draw.address_add=vga.draw.blocks;
        break;
    case M_TEXT:
    case M_CGA2:
    case M_CGA4:
    case M_CGA16:
    case M_DCGA:
    case M_AMSTRAD: // Next line.
        if (IS_EGAVGA_ARCH)
            vga.draw.address_add=vga.config.scan_len*(2u<<vga.config.addr_shift);
        else
            vga.draw.address_add=vga.draw.blocks;
        break;
    case M_TANDY2:
        if (machine == MCH_MCGA)
            vga.draw.address_add=vga.draw.blocks;
        else
            vga.draw.address_add=vga.draw.blocks/4u;
        break;
    case M_TANDY4:
        vga.draw.address_add=vga.draw.blocks;
        break;
    case M_TANDY16:
        vga.draw.address_add=vga.draw.blocks;
        break;
    case M_TANDY_TEXT:
        vga.draw.address_add=vga.draw.blocks*2;
        break;
    case M_HERC_TEXT:
        vga.draw.address_add=vga.draw.blocks*2;
        break;
    case M_HERC_GFX:
        vga.draw.address_add=vga.draw.blocks;
        break;
    default:
        vga.draw.address_add=vga.draw.blocks*8;
        break;
    }
}

void VGA_ActivateHardwareCursor(void) {
    bool hwcursor_active=false;
    if (svga.hardware_cursor_active) {
        if (svga.hardware_cursor_active()) hwcursor_active=true;
    }
    if (hwcursor_active) {
        switch(vga.mode) {
        case M_LIN32:
            VGA_DrawLine=VGA_Draw_LIN32_Line_HWMouse;
            break;
        case M_LIN24:
            VGA_DrawLine=VGA_Draw_Linear_Line_24_to_32_HWMouse;
            break;
        case M_LIN15:
        case M_LIN16:
            if ((svgaCard == SVGA_TsengET3K || svgaCard == SVGA_TsengET4K) && et4k_highcolor_half_pixel_rate())
                VGA_DrawLine=VGA_Draw_LIN16_Line_2x;
            else
                VGA_DrawLine=VGA_Draw_LIN16_Line_HWMouse;
            break;
        case M_LIN8:
            VGA_DrawLine=VGA_Draw_VGA_Line_Xlat32_HWMouse;
            break;
        default:
            VGA_DrawLine=VGA_Draw_VGA_Line_HWMouse;
            break;
        }
    } else {
        switch(vga.mode) {
        case M_LIN8:
            VGA_DrawLine=VGA_Draw_Xlat32_Linear_Line;
            break;
        case M_LIN24:
            VGA_DrawLine=VGA_Draw_Linear_Line_24_to_32;
            break;
#if SDL_BYTEORDER == SDL_LIL_ENDIAN && defined(MACOSX) && !defined(C_SDL2) /* Mac OS X Intel builds use a weird RGBA order (alpha in the low 8 bits) */
        case M_LIN32:
            VGA_DrawLine=VGA_Draw_LIN32_Line_HWMouse;
            break;
#endif
        default:
            VGA_DrawLine=VGA_Draw_Linear_Line;
            break;
        }
    }
}

void VGA_SetupDrawing(Bitu /*val*/) {
    if (vga.mode==M_ERROR) {
        PIC_RemoveEvents(VGA_VerticalTimer);
        PIC_RemoveEvents(VGA_PanningLatch);
        PIC_RemoveEvents(VGA_DisplayStartLatch);
        return;
    }
    // user choosable special trick support
    // multiscan -- zooming effects - only makes sense if linewise is enabled
    // linewise -- scan display line by line instead of 4 blocks
    // keep compatibility with other builds of DOSBox for vgaonly.
    vga.draw.doublescan_effect = true;
    vga.draw.render_step = 0;
    vga.draw.render_max = 1;

    // set the drawing mode
    switch (machine) {
    case MCH_CGA:
    case MCH_MCGA:
    case MCH_PCJR:
    case MCH_TANDY:
        vga.draw.mode = DRAWLINE;
        break;
    case MCH_EGA:
        // Note: The Paradise SVGA uses the same panning mechanism as EGA
        vga.draw.mode = EGALINE;
        break;
    case MCH_VGA:
    case MCH_PC98:
        if (svgaCard==SVGA_None) {
            vga.draw.mode = DRAWLINE;
            break;
        }
        // fall-through
    default:
        vga.draw.mode = DRAWLINE;
        break;
    }
    
    /* Calculate the FPS for this screen */
    Bitu oscclock = 0, clock;
    Bitu htotal, hdend, hbstart, hbend, hrstart, hrend;
    Bitu vtotal, vdend, vbstart, vbend, vrstart, vrend;
    Bitu hbend_mask, vbend_mask;
    Bitu vblank_skip;

    /* NTS: PC-98 emulation re-uses VGA state FOR NOW.
     *      This will slowly change to accommodate PC-98 display controller over time
     *      and IS_PC98_ARCH will become it's own case statement. */

    if (IS_PC98_ARCH) {
        hdend = pc98_gdc[GDC_MASTER].active_display_words_per_line;
        hbstart = hdend;
        hrstart = hdend + pc98_gdc[GDC_MASTER].horizontal_front_porch_width;
        hrend = hrstart + pc98_gdc[GDC_MASTER].horizontal_sync_width;
        htotal = hrend + pc98_gdc[GDC_MASTER].horizontal_back_porch_width;
        hbend = htotal;

        vdend = pc98_gdc[GDC_MASTER].active_display_lines;
        vbstart = vdend;
        vrstart = vdend + pc98_gdc[GDC_MASTER].vertical_front_porch_width;
        vrend = vrstart + pc98_gdc[GDC_MASTER].vertical_sync_width;
        vtotal = vrend + pc98_gdc[GDC_MASTER].vertical_back_porch_width;
        vbend = vtotal;

        // perhaps if the game makes a custom mode, it might choose different active display regions
        // for text and graphics. allow that here.
        // NTS: Remember that the graphics (slave) GDC is programmed in "words" which in graphics mode
        //      means 16-pixel wide units.
        if (hdend < (pc98_gdc[GDC_SLAVE].active_display_words_per_line * 2U))
            hdend = (pc98_gdc[GDC_SLAVE].active_display_words_per_line * 2U);
        if (vdend < (pc98_gdc[GDC_SLAVE].active_display_lines))
            vdend = (pc98_gdc[GDC_SLAVE].active_display_lines);

        // TODO: The GDC rendering should allow different active display regions to render
        //       properly over one another.

        // TODO: Found a monitor document that lists two different scan rates for PC-98:
        //
        //       640x400  25.175MHz dot clock  70.15Hz refresh  31.5KHz horizontal refresh (basically, VGA)
        //       640x400  21.05MHz dot clock   56.42Hz refresh  24.83Hz horizontal refresh (original spec?)

        if (false/*future 15KHz hsync*/) {
            oscclock = 14318180;
        }
        else if (!pc98_31khz_mode/*24KHz hsync*/) {
            oscclock = 21052600;
        }
        else {/*31KHz VGA-like hsync*/
            oscclock = 25175000;
        }

        clock = oscclock / 8;
    }
    else if (IS_EGAVGA_ARCH) {
        htotal = vga.crtc.horizontal_total;
        hdend = vga.crtc.horizontal_display_end;
        hbend = vga.crtc.end_horizontal_blanking&0x1F;
        hbstart = vga.crtc.start_horizontal_blanking;
        hrstart = vga.crtc.start_horizontal_retrace;

        vtotal= vga.crtc.vertical_total | ((vga.crtc.overflow & 1u) << 8u);
        vdend = vga.crtc.vertical_display_end | ((vga.crtc.overflow & 2u) << 7u);
        vbstart = vga.crtc.start_vertical_blanking | ((vga.crtc.overflow & 0x08u) << 5u);
        vrstart = vga.crtc.vertical_retrace_start + ((vga.crtc.overflow & 0x04u) << 6u);
        
        if (IS_VGA_ARCH || IS_PC98_ARCH) {
            // additional bits only present on vga cards
            htotal |= (vga.s3.ex_hor_overflow & 0x1u) << 8u;
            htotal += 3u;
            hdend |= (vga.s3.ex_hor_overflow & 0x2u) << 7u;
            hbend |= (vga.crtc.end_horizontal_retrace&0x80u) >> 2u;
            hbstart |= (vga.s3.ex_hor_overflow & 0x4u) << 6u;
            hrstart |= (vga.s3.ex_hor_overflow & 0x10u) << 4u;
            hbend_mask = 0x3fu;
            
            vtotal |= (vga.crtc.overflow & 0x20u) << 4u;
            vtotal |= (vga.s3.ex_ver_overflow & 0x1u) << 10u;
            vdend |= (vga.crtc.overflow & 0x40u) << 3u; 
            vdend |= (vga.s3.ex_ver_overflow & 0x2u) << 9u;
            vbstart |= (vga.crtc.maximum_scan_line & 0x20u) << 4u;
            vbstart |= (vga.s3.ex_ver_overflow & 0x4u) << 8u;
            vrstart |= ((vga.crtc.overflow & 0x80u) << 2u);
            vrstart |= (vga.s3.ex_ver_overflow & 0x10u) << 6u;
            vbend_mask = 0xffu;
        } else { // EGA
            hbend_mask = 0x1fu;
            vbend_mask = 0x1fu;
        }
        htotal += 2;
        vtotal += 2;
        hdend += 1;
        vdend += 1;

        // horitzontal blanking
        if (hbend <= (hbstart & hbend_mask)) hbend += hbend_mask + 1;
        hbend += hbstart - (hbstart & hbend_mask);
        
        // horizontal retrace
        hrend = vga.crtc.end_horizontal_retrace & 0x1f;
        if (hrend <= (hrstart&0x1f)) hrend += 32;
        hrend += hrstart - (hrstart&0x1f);
        if (hrend > hbend) hrend = hbend; // S3 BIOS (???)
        
        // vertical retrace
        vrend = vga.crtc.vertical_retrace_end & 0xf;
        if (vrend <= (vrstart&0xf)) vrend += 16;
        vrend += vrstart - (vrstart&0xf);

        // vertical blanking
        vbend = vga.crtc.end_vertical_blanking & vbend_mask;
        if (vbstart != 0) {
        // Special case vbstart==0:
        // Most graphics cards agree that lines zero to vbend are
        // blanked. ET4000 doesn't blank at all if vbstart==vbend.
        // ET3000 blanks lines 1 to vbend (255/6 lines).
            vbstart += 1;
            if (vbend <= (vbstart & vbend_mask)) vbend += vbend_mask + 1;
            vbend += vbstart - (vbstart & vbend_mask);
        }
        vbend++;

        // TODO: Found a monitor document that lists two different scan rates for PC-98:
        //
        //       640x400  25.175MHz dot clock  70.15Hz refresh  31.5KHz horizontal refresh (basically, VGA)
        //       640x400  21.05MHz dot clock   56.42Hz refresh  24.83Hz horizontal refresh (original spec?)

        if (svga.get_clock) {
            oscclock = svga.get_clock();
        } else if (vga.mode == M_PC98) {
            if (false/*future 15KHz hsync*/) {
                oscclock = 14318180;
            }
            else if (!pc98_31khz_mode/*24KHz hsync*/) {
                oscclock = 21052600;
            }
            else {/*31KHz VGA-like hsync*/
                oscclock = 25175000;
            }
        } else {
            switch ((vga.misc_output >> 2) & 3) {
            case 0: 
                oscclock = (machine==MCH_EGA) ? (PIT_TICK_RATE*12) : 25175000;
                break;
            case 1:
            default:
                oscclock = (machine==MCH_EGA) ? 16257000 : 28322000;
                break;
            }
        }

        /* Check for 8 or 9 character clock mode */
        if (vga.seq.clocking_mode & 1 ) clock = oscclock/8; else clock = oscclock/9;
        if (vga.mode==M_LIN15 || vga.mode==M_LIN16) clock *= 2;
        /* Check for pixel doubling, master clock/2 */
        /* NTS: VGA 256-color mode has a dot clock NOT divided by two, because real hardware reveals
         *      that internally the card processes pixels 4 bits per cycle through a 8-bit shift
         *      register and a bit is set to latch the 8-bit value out to the DAC every other cycle. */
        if (vga.seq.clocking_mode & 0x8) {
            clock /=2;
            oscclock /= 2;
        }

        if (svgaCard==SVGA_S3Trio) {
            // support for interlacing used by the S3 BIOS and possibly other drivers
            if (vga.s3.reg_42 & 0x20) {
                vtotal *= 2;    vdend *= 2;
                vbstart *= 2;   vbend *= 2;
                vrstart *= 2;   vrend *= 2;
                //clock /= 2;
        }
        }

        /* FIXME: This is based on correcting the hicolor mode for MFX/Transgression 2.
         *        I am not able to test against the Windows drivers at this time. */
        if ((svgaCard == SVGA_TsengET3K || svgaCard == SVGA_TsengET4K) && et4k_highcolor_half_pixel_rate())
            clock /= 2;
    } else {
        // not EGAVGA_ARCH
        vga.draw.split_line = 0x10000;  // don't care

        htotal = vga.other.htotal + 1u;
        hdend = vga.other.hdend;

        if (machine == MCH_MCGA) {
            // it seems MCGA follows the EGA/VGA model of encoding active display
            // as N - 1 rather than CGA/MDA model of N.
            //
            // TODO: Verify this on real hardware. Some code in DOSLIB hw/vga2/test5.c
            //       attempts to set active display width which can confirm this.
            //
            //       This is so far based on CRTC register dumps from real MCGA hardware
            //       for each mode.
            hdend++;
        }

        hbstart = hdend;
        hbend = htotal;
        hrstart = vga.other.hsyncp;
        hrend = hrstart + (vga.other.hsyncw) ;

        vga.draw.address_line_total = vga.other.max_scanline + 1u;

        if (machine == MCH_MCGA) {
            // mode 0x11 and 0x13 on real hardware have max_scanline == 0,
            // will need doubling again to work properly.
            if (vga.other.mcga_mode_control & 3)
                vga.draw.address_line_total *= 2;

            // 80x25 has horizontal timings like 40x25, but the hardware
            // knows if 80x25 text is intended by bit 0 of port 3D8h and
            // adjusts appropriately.
            //
            // The BIOS will only set bit 0 for modes 2 and 3 (80x25) and
            // will not set it for any other mode.
            if (vga.tandy.mode_control & 1) {
                htotal *= 2;
                hdend *= 2;
                hbstart *= 2;
                hbend *= 2;
                hrstart *= 2;
                hrend *= 2;
            }
        }

        vtotal = vga.draw.address_line_total * (vga.other.vtotal+1u)+vga.other.vadjust;
        vdend = vga.draw.address_line_total * vga.other.vdend;
        vrstart = vga.draw.address_line_total * vga.other.vsyncp;
        vrend = vrstart + 16; // vsync width is fixed to 16 lines on the MC6845 TODO Tandy
        vbstart = vdend;
        vbend = vtotal;

        switch (machine) {
        case MCH_AMSTRAD:
            clock=(16000000/2)/8;
            break;
        case MCH_CGA:
        case TANDY_ARCH_CASE:
            clock = (PIT_TICK_RATE*12)/8;
            // FIXME: This is wrong for Tandy/PCjr 16-color modes and 640-wide 4-color mode
            if (vga.mode != M_TANDY2) {
                if (!(vga.tandy.mode_control & 1)) clock /= 2;
            }
            oscclock = clock * 8;
            break;
        case MCH_MCGA:
            clock = 25175000 / 2 / 8;//FIXME: Guess. Verify
            if (!(vga.tandy.mode_control & 1)) clock /= 2;
            oscclock = clock * 2 * 8;
            break;
        case MCH_MDA:
        case MCH_HERC:
            oscclock=16257000;
            if (vga.mode == M_HERC_GFX)
                clock=oscclock/8;
            else
                clock=oscclock/9;

            if (vga.herc.mode_control & 0x2) clock /= 2;
            break;
        default:
            clock = (PIT_TICK_RATE*12)/8;
            oscclock = clock * 8;
            break;
        }
        vga.draw.delay.hdend = hdend*1000.0/clock; //in milliseconds
    }
#if C_DEBUG
    LOG(LOG_VGA,LOG_NORMAL)("h total %3d end %3d blank (%3d/%3d) retrace (%3d/%3d)",
        (int)htotal, (int)hdend, (int)hbstart, (int)hbend, (int)hrstart, (int)hrend );
    LOG(LOG_VGA,LOG_NORMAL)("v total %3d end %3d blank (%3d/%3d) retrace (%3d/%3d)",
        (int)vtotal, (int)vdend, (int)vbstart, (int)vbend, (int)vrstart, (int)vrend );
#endif
    if (!htotal) return;
    if (!vtotal) return;
    
    // The screen refresh frequency
    double fps;
    extern double vga_force_refresh_rate;
    if (vga_force_refresh_rate > 0) {
        /* force the VGA refresh rate by setting fps and bending the clock to our will */
        LOG(LOG_VGA,LOG_NORMAL)("VGA forced refresh rate in effect, %.3f",vga_force_refresh_rate);
        fps=vga_force_refresh_rate;
        clock=((double)(vtotal*htotal))*fps;
    }
    else {
        // The screen refresh frequency
        fps=(double)clock/(vtotal*htotal);
        LOG(LOG_VGA,LOG_NORMAL)("VGA refresh rate is now, %.3f",fps);
    }

    /* clip display end to stay within vtotal ("Monolith" demo part 4 320x570 mode fix) */
    if (vdend > vtotal && !IS_JEGA_ARCH) {
        LOG(LOG_VGA,LOG_WARN)("VGA display end greater than vtotal!");
        vdend = vtotal;
    }

    // Horizontal total (that's how long a line takes with whistles and bells)
    vga.draw.delay.htotal = htotal*1000.0/clock; //in milliseconds
    // Start and End of horizontal blanking
    vga.draw.delay.hblkstart = hbstart*1000.0/clock; //in milliseconds
    vga.draw.delay.hblkend = hbend*1000.0/clock; 
    // Start and End of horizontal retrace
    vga.draw.delay.hrstart = hrstart*1000.0/clock;
    vga.draw.delay.hrend = hrend*1000.0/clock;
    // Start and End of vertical blanking
    vga.draw.delay.vblkstart = vbstart * vga.draw.delay.htotal;
    vga.draw.delay.vblkend = vbend * vga.draw.delay.htotal;
    // Start and End of vertical retrace pulse
    vga.draw.delay.vrstart = vrstart * vga.draw.delay.htotal;
    vga.draw.delay.vrend = vrend * vga.draw.delay.htotal;

    // Vertical blanking tricks
    vblank_skip = 0;
    if ((IS_VGA_ARCH || IS_PC98_ARCH) && !ignore_vblank_wraparound) { // others need more investigation
        if (vbstart < vtotal) { // There will be no blanking at all otherwise
            if (vbend > vtotal) {
                // blanking wraps to the start of the screen
                vblank_skip = vbend&0x7f;
                
                // on blanking wrap to 0, the first line is not blanked
                // this is used by the S3 BIOS and other S3 drivers in some SVGA modes
                if ((vbend&0x7f)==1) vblank_skip = 0;
                
                // it might also cut some lines off the bottom
                if (vbstart < vdend) {
                    vdend = vbstart;
                }
                LOG(LOG_VGA,LOG_WARN)("Blanking wrap to line %d", (int)vblank_skip);
            } else if (vbstart<=1) {
                // blanking is used to cut lines at the start of the screen
                vblank_skip = vbend;
                LOG(LOG_VGA,LOG_WARN)("Upper %d lines of the screen blanked", (int)vblank_skip);
            } else if (vbstart < vdend) {
                if (vbend < vdend) {
                    // the game wants a black bar somewhere on the screen
                    LOG(LOG_VGA,LOG_WARN)("Unsupported blanking: line %d-%d",(int)vbstart,(int)vbend);
                } else {
                    // blanking is used to cut off some lines from the bottom
                    vdend = vbstart;
                }
            }
            vdend -= vblank_skip;
        }
    }
    vga.draw.vblank_skip = vblank_skip;

    // Display end
    vga.draw.delay.vdend = vdend * vga.draw.delay.htotal;

    // EGA frequency dependent monitor palette
    if (machine == MCH_EGA) {
        if (vga.misc_output & 1) {
            // EGA card is in color mode
            if ((1.0f/vga.draw.delay.htotal) > 19.0f) {
                // 64 color EGA mode
                VGA_ATTR_SetEGAMonitorPalette(EGA);
            } else {
                // 16 color CGA mode compatibility
                VGA_ATTR_SetEGAMonitorPalette(CGA);
            }
        } else {
            // EGA card in monochrome mode
            // It is not meant to be autodetected that way, you either
            // have a monochrome or color monitor connected and
            // the EGA switches configured appropriately.
            // But this would only be a problem if a program sets 
            // the adapter to monochrome mode and still expects color output.
            // Such a program should be shot to the moon...
            VGA_ATTR_SetEGAMonitorPalette(MONO);
        }
    }

    /*
      6  Horizontal Sync Polarity. Negative if set
      7  Vertical Sync Polarity. Negative if set
         Bit 6-7 indicates the number of lines on the display:
            1:  400, 2: 350, 3: 480
    */
    //Try to determine the pixel size, aspect correct is based around square pixels

    //Base pixel width around 100 clocks horizontal
    //For 9 pixel text modes this should be changed, but we don't support that anyway :)
    //Seems regular vga only listens to the 9 char pixel mode with character mode enabled
    //Base pixel height around vertical totals of modes that have 100 clocks horizontal
    //Different sync values gives different scaling of the whole vertical range
    //VGA monitor just seems to thighten or widen the whole vertical range

    vga.draw.resizing=false;
    vga.draw.has_split=false;
    vga.draw.vret_triggered=false;

    //Check to prevent useless black areas
    if (hbstart<hdend) hdend=hbstart;
    if ((!(IS_VGA_ARCH || IS_PC98_ARCH)) && (vbstart<vdend)) vdend=vbstart;

    Bitu width=hdend;
    Bitu height=vdend;

    if (IS_EGAVGA_ARCH || IS_PC98_ARCH) {
        vga.draw.address_line_total=(vga.crtc.maximum_scan_line&0x1fu)+1u;
        switch(vga.mode) {
        case M_CGA16:
        case M_CGA2:
        case M_CGA4:
        case M_DCGA:
        case M_PC98:
        case M_TEXT:
            // these use line_total internal
            // doublescanning needs to be emulated by renderer doubleheight
            // EGA has no doublescanning bit at 0x80
            if (vga.crtc.maximum_scan_line&0x80) {
                // vga_draw only needs to draw every second line
                height /= 2;
            }
            break;
        default:
            vga.draw.doublescan_effect = vga.draw.doublescan_set;

            if (vga.crtc.maximum_scan_line & 0x80)
                vga.draw.address_line_total *= 2;

            /* if doublescan=false and line_total is even, then halve the height.
             * the VGA raster scan will skip every other line to accommodate that. */
            if ((!vga.draw.doublescan_effect) && (vga.draw.address_line_total & 1) == 0)
                height /= 2;
            else
                vga.draw.doublescan_effect = true;

            break;
        }
    }

    if (!vga.draw.doublescan_effect)
        vga.draw.render_max = 2; /* count all lines but render only every other line */

    //Set the bpp
    Bitu bpp;
    switch (vga.mode) {
    case M_LIN15:
        bpp = 15;
        break;
    case M_LIN16:
        bpp = 16;
        break;
    case M_LIN24:
    case M_LIN32:
        bpp = 32;
        break;
    default:
        bpp = 8;
        break;
    }
    vga.draw.linear_base = vga.mem.linear;
    vga.draw.linear_mask = vga.mem.memmask;

    /* Some games and plenty of demoscene productions like to rely on
     * the fact that the standard VGA modes wrap around at 256KB even
     * on SVGA hardware. Without this check, those demos will show
     * credits that scroll upward to blackness before "popping" back
     * onto the screen. */
    if (IS_VGA_ARCH) {
        /* NTS: S3 emulation ties "compatible chain4" to CRTC register 31 bit 3 which controls
         *      whether access to > 256KB of video RAM is enabled, which is why it's used here */
        if (vga.config.compatible_chain4 || svgaCard == SVGA_None)
            vga.draw.linear_mask &= 0x3FFFF;
    }
    else if (IS_EGA_ARCH) {
        vga.draw.linear_mask &= 0x3FFFF;
    }

    if (IS_EGAVGA_ARCH)
        vga.draw.planar_mask = vga.draw.linear_mask >> 2;
    else
        vga.draw.planar_mask = vga.draw.linear_mask >> 1;

    Bitu pix_per_char = 8;
    switch (vga.mode) {
    case M_VGA:
        // hack for tgr2 -hc high color mode demo
        if (vga.dac.reg02==0x80) {
            bpp=16;
            vga.mode=M_LIN16;
            VGA_SetupHandlers();
            VGA_DrawLine=VGA_Draw_LIN16_Line_2x;
            pix_per_char = 4;
            break;
        }
        bpp = 32;
        pix_per_char = 4;
        if (vga.mode == M_VGA && (svgaCard == SVGA_TsengET3K || svgaCard == SVGA_TsengET4K)) {
            /* ET4000 chipsets handle the chained mode (in my opinion) with sanity and we can scan linearly for it.
             * Chained VGA mode maps planar byte addr = (addr >> 2) and plane = (addr & 3) */
            VGA_DrawLine = VGA_Draw_Xlat32_Linear_Line;
        }
        else if (machine == MCH_MCGA) {
            pix_per_char = 8;
            VGA_DrawLine = VGA_Draw_Xlat32_Linear_Line;
            vga.tandy.draw_base = vga.mem.linear;
            vga.draw.address_line_total = 1;
        }
        else {
            /* other SVGA chipsets appear to handle chained mode by writing 4 pixels to 4 planes, and showing
             * only every 4th byte, which is why when you switch the CRTC to byte or word mode on these chipsets,
             * you see 16-pixel groups with 4 pixels from the chained display you expect followed by 12 pixels
             * of whatever contents of memory remain. but when you unchain the bitplanes the card will allow
             * "planar" writing to all 16 pixels properly. Chained VGA maps like planar byte = (addr & ~3) and
             * plane = (addr & 3) */
            VGA_DrawLine = VGA_Draw_Xlat32_VGA_CRTC_bmode_Line;
        }
        break;
    case M_LIN8:
        bpp = 32;
        VGA_DrawLine = VGA_Draw_Xlat32_Linear_Line;

        if ((vga.s3.reg_3a & 0x10)||(svgaCard!=SVGA_S3Trio))
            pix_per_char = 8; // TODO fiddle out the bits for other svga cards
        else
            pix_per_char = 4;

        VGA_ActivateHardwareCursor();
        break;
    case M_LIN24:
    case M_LIN32:
        VGA_ActivateHardwareCursor();
        break;
    case M_LIN15:
    case M_LIN16:
        pix_per_char = 4; // 15/16 bpp modes double the horizontal values
        VGA_ActivateHardwareCursor();
        break;
    case M_PACKED4:
        bpp = 32;
        vga.draw.blocks = width;
        VGA_DrawLine = VGA_Draw_VGA_Packed4_Xlat32_Line;
        break;
    case M_LIN4:
    case M_EGA:
        vga.draw.blocks = width;

        if (IS_EGA_ARCH) {
            VGA_DrawLine = EGA_Draw_VGA_Planar_Xlat8_Line;
            bpp = 8;
        }
        else {
            VGA_DrawLine = VGA_Draw_VGA_Planar_Xlat32_Line;
            bpp = 32;
        }
        break;
    case M_CGA16:
        vga.draw.blocks=width*2;
        pix_per_char = 16;
        VGA_DrawLine=VGA_Draw_CGA16_Line;
        break;
    case M_CGA4:
        if (IS_EGA_ARCH) {
            vga.draw.blocks=width;
            VGA_DrawLine=EGA_Draw_2BPP_Line_as_EGA;
            bpp = 8;
        }
        else if (IS_EGAVGA_ARCH || IS_PC98_ARCH) {
            vga.draw.blocks=width;
            VGA_DrawLine=VGA_Draw_2BPP_Line_as_VGA;
            bpp = 32;
        }
        else if (machine == MCH_MCGA) {
            vga.draw.blocks=width*2;
            VGA_DrawLine=VGA_Draw_2BPP_Line_as_MCGA;
            bpp = 32;

            /* MCGA CGA-compatible modes will always refer to the last half of the 64KB of RAM */
            vga.tandy.draw_base = vga.mem.linear + 0x8000;
        }
        else {
            VGA_DrawLine=VGA_Draw_2BPP_Line;
            vga.draw.blocks=width*2;
        }
        break;
    case M_DCGA:
    case M_CGA2:
        // CGA 2-color mode on EGA/VGA is just EGA 16-color planar mode with one bitplane enabled and a
        // color palette to match. Therefore CGA 640x200 2-color mode can be rendered correctly using
        // the 16-color planar renderer. The MEM13 bit is configured to replace address bit 13 with
        // character row counter bit 0 to match CGA memory layout, doublescan is set (as if 320x200),
        // max_scanline is set to 1 (2 lines).
        if (IS_EGA_ARCH) {
            vga.draw.blocks=width;
            VGA_DrawLine=EGA_Draw_1BPP_Line_as_EGA;
            bpp = 8;
        }
        else if (IS_EGAVGA_ARCH) {
            vga.draw.blocks=width;
            VGA_DrawLine=VGA_Draw_1BPP_Line_as_VGA;
            bpp = 32;
        }
        else if (machine == MCH_MCGA) {
            vga.draw.blocks=width;
            VGA_DrawLine=VGA_Draw_1BPP_Line_as_MCGA;
            pix_per_char = 16;
            bpp = 32;

            /* MCGA CGA-compatible modes will always refer to the last half of the 64KB of RAM */
            if (vga.other.mcga_mode_control & 3) // 320x200 256-color or 640x480 2-color
                vga.tandy.draw_base = vga.mem.linear;
            else
                vga.tandy.draw_base = vga.mem.linear + 0x8000;

            if (vga.other.mcga_mode_control & 2) // 640x480 2-color
                vga.draw.address_line_total = 1;
        }
        else {
            VGA_DrawLine=VGA_Draw_1BPP_Line;
            vga.draw.blocks=width*2;
        }
        break;
    case M_PC98:
        vga.draw.blocks=width;
        vga.draw.char9dot = false;
        VGA_DrawLine=VGA_PC98_Xlat32_Draw_Line;
        bpp = 32;
        break;
    case M_TEXT:
        vga.draw.blocks=width;
        // if char9_set is true, allow 9-pixel wide fonts
        if (isJEGAEnabled()) { //if Kanji Text Disable is off
            vga.draw.char9dot = false;
        }
        else if ((vga.seq.clocking_mode&0x01) || !vga.draw.char9_set) {
            // 8-pixel wide
            pix_per_char = 8;
            vga.draw.char9dot = false;
        } else {
            // 9-pixel wide
            pix_per_char = 9;
            vga.draw.char9dot = true;
        }

        /* FIXME: This is still peeking into DOS state, which should be turned into an INT 10h call or some such
         *        when KEYB or other utility changes code page, but it does not involve reading guest memory */
        if (IS_EGA_ARCH) {
            VGA_DrawLine = EGA_TEXT_Xlat8_Draw_Line;
            bpp = 8;
        }
        else {
            VGA_DrawLine = VGA_TEXT_Xlat32_Draw_Line;
            bpp = 32;
        }
        break;
    case M_HERC_GFX:
        vga.draw.blocks=width*2;
        if (vga.herc.blend) VGA_DrawLine=VGA_Draw_1BPP_Blend_Line;
        else VGA_DrawLine=VGA_Draw_1BPP_Line;
        pix_per_char = 16;
        break;
    case M_TANDY2:
        if (((machine==MCH_PCJR)&&(vga.tandy.gfx_control & 0x8)) ||
            (vga.tandy.mode_control & 0x10)) {
            vga.draw.blocks=width * 8;
            pix_per_char = 16;
        } else {
            vga.draw.blocks=width * 4;
            pix_per_char = 8;
        }

        VGA_DrawLine=VGA_Draw_1BPP_Line;

        /* MCGA CGA-compatible modes will always refer to the last half of the 64KB of RAM */
        if (machine == MCH_MCGA) {
            VGA_DrawLine=VGA_Draw_1BPP_Line_as_MCGA;
            vga.draw.blocks=width * 2;
            pix_per_char = 16;
            bpp = 32;

            /* MCGA CGA-compatible modes will always refer to the last half of the 64KB of RAM */
            if (vga.other.mcga_mode_control & 3) // 320x200 256-color or 640x480 2-color
                vga.tandy.draw_base = vga.mem.linear;
            else
                vga.tandy.draw_base = vga.mem.linear + 0x8000;

            if (vga.other.mcga_mode_control & 2) // 640x480 2-color
                vga.draw.address_line_total = 1;
        }

        break;
    case M_TANDY4:
        vga.draw.blocks=width * 2;
        pix_per_char = 8;
        if ((machine==MCH_TANDY && (vga.tandy.gfx_control & 0x8)) ||
            (machine==MCH_PCJR && (vga.tandy.mode_control==0x0b))) {
            VGA_DrawLine=VGA_Draw_2BPPHiRes_Line;
        }
        else {
            VGA_DrawLine=VGA_Draw_2BPP_Line;
        }

        /* MCGA CGA-compatible modes will always refer to the last half of the 64KB of RAM */
        if (machine == MCH_MCGA) {
            vga.tandy.draw_base = vga.mem.linear + 0x8000;
            vga.draw.blocks=width * 2;
            VGA_DrawLine=VGA_Draw_2BPP_Line_as_MCGA;
            bpp = 32;
        }

        break;
    case M_TANDY16:
        if (vga.tandy.mode_control & 0x1) {
            if (( machine==MCH_TANDY ) && ( vga.tandy.mode_control & 0x10 )) {
                vga.draw.blocks=width*4;
                pix_per_char = 8;
            } else {
                vga.draw.blocks=width*2;
                pix_per_char = 4;
            }
            VGA_DrawLine=VGA_Draw_4BPP_Line;
        } else {
            vga.draw.blocks=width*2;
            pix_per_char = 8;
            VGA_DrawLine=VGA_Draw_4BPP_Line_Double;
        }
        break;
    case M_TANDY_TEXT: /* Also CGA */
        vga.draw.blocks=width;
        if (machine==MCH_CGA /*&& !doublewidth*/ && enableCGASnow && (vga.tandy.mode_control & 1)/*80-column mode*/)
            VGA_DrawLine=VGA_CGASNOW_TEXT_Draw_Line; /* Alternate version that emulates CGA snow */
        else
            VGA_DrawLine=VGA_TEXT_Draw_Line;

        /* MCGA CGA-compatible modes will always refer to the last half of the 64KB of RAM */
        if (machine == MCH_MCGA) {
            vga.tandy.draw_base = vga.mem.linear + 0x8000;
            VGA_DrawLine = MCGA_TEXT_Draw_Line;
            bpp = 32;
        }

        break;
    case M_HERC_TEXT:
        vga.draw.blocks=width;
        VGA_DrawLine=VGA_TEXT_Herc_Draw_Line;
        break;
    case M_AMSTRAD: // Probably OK?
        pix_per_char = 16;
        vga.draw.blocks=width*2;
        VGA_DrawLine=VGA_Draw_AMS_4BPP_Line;
//      VGA_DrawLine=VGA_Draw_4BPP_Line;
/*      doubleheight=true;
        vga.draw.blocks = 2*width; width<<=4;
        vga.draw.linear_base = vga.mem.linear + VGA_CACHE_OFFSET;
        vga.draw.linear_mask = 512 * 1024 - 1; */
        break;
    default:
        LOG(LOG_VGA,LOG_ERROR)("Unhandled VGA mode %d while checking for resolution",vga.mode);
        break;
    }
    width *= pix_per_char;
    VGA_CheckScanLength();
#if defined(USE_TTF)
    if (ttf.inUse) vga.draw.address_add = ttf.cols * 2;
#endif

    /* for MCGA, need to "double scan" the screen in some cases */
    if (vga.other.mcga_mode_control & 2) { // 640x480 2-color
        height *= 2;
        mcga_double_scan = true;
    }
    else if (machine == MCH_MCGA && vga.mode == M_TANDY_TEXT) { // MCGA text mode
        height *= 2;
        mcga_double_scan = true;
        vga.draw.address_line_total *= 2;
    }
    else {
        mcga_double_scan = false;
    }
    
    vga.draw.lines_total=height;
    vga.draw.line_length = width * ((bpp + 1) / 8);
    vga.draw.oscclock = oscclock;
    vga.draw.clock = clock;

    double vratio = ((double)width)/(double)height; // ratio if pixels were square

    // the picture ratio factor
    double scanratio =  ((double)hdend/(double)(htotal-(hrend-hrstart)))/
                        ((double)vdend/(double)(vtotal-(vrend-vrstart)));
    double scanfield_ratio = 4.0/3.0;
    switch(machine) {
        case MCH_CGA:
        case MCH_MCGA:
        case MCH_PCJR:
        case MCH_TANDY:
            scanfield_ratio = 1.382;
            break;
        case MCH_MDA:
        case MCH_HERC:
            scanfield_ratio = 1.535;
            break;
        case MCH_EGA:
            switch (vga.misc_output >> 6) {
            case 0: // 200 lines:
                scanfield_ratio = 1.085; // DOSBugs
                //scanfield_ratio = 1.375; // IBM EGA BIOS
                break;
            case 2: // 350 lines
                // TODO monitors seem to display this with a bit of black borders on top and bottom
                scanfield_ratio = 1.45;
                break;
            default:
                // other cases are undefined for EGA - scale them to 4:3
                scanfield_ratio = (4.0/3.0) / scanratio;
                break;
            }
            break;
	case MCH_PC98:
	    /* NTS: According to Spaztron64 PC-98 monitors have a 16:10 aspect ratio, not a 4:3 aspect ratio.
	     *      [https://github.com/joncampbell123/dosbox-x/issues/3032] */
	    if (height >= 480)
	            scanfield_ratio = (4.0/3.0) / scanratio;
	    else
	            scanfield_ratio = (16.0/10.0) / scanratio;
	    break;

        default: // VGA
            switch (vga.misc_output >> 6) {
            case 0: // VESA: "OTHER" scanline amount
                scanfield_ratio = (4.0/3.0) / scanratio;
                break;
            case 1: // 400 lines
                scanfield_ratio = 1.312;
                break;
            case 2: // 350 lines
                scanfield_ratio = 1.249;
                break;
            case 3: // 480 lines
                scanfield_ratio = 1.345;
                break;
            }
            break;
    }
    // calculate screen ratio
    double screenratio = scanratio * scanfield_ratio;

    // override screenratio for certain cases:
    if (!IS_PC98_ARCH) {
	    if (vratio == 1.6) screenratio = 4.0 / 3.0;
	    else if (vratio == 0.8) screenratio = 4.0 / 3.0;
	    else if (vratio == 3.2) screenratio = 4.0 / 3.0;
	    else if (vratio == (4.0/3.0)) screenratio = 4.0 / 3.0;
	    else if (vratio == (2.0/3.0)) screenratio = 4.0 / 3.0;
	    else if ((width >= 800)&&(height>=600)) screenratio = 4.0 / 3.0;
	    else if (render.aspect) screenratio = 4.0 / 3.0;
    }

#if C_DEBUG
            LOG(LOG_VGA,LOG_NORMAL)("screen: %1.3f, scanfield: %1.3f, scan: %1.3f, vratio: %1.3f",
                screenratio, scanfield_ratio, scanratio, vratio);
#endif

    bool fps_changed = false;

#if C_DEBUG
    LOG(LOG_VGA,LOG_NORMAL)("h total %2.5f (%3.2fkHz) blank(%02.5f/%02.5f) retrace(%02.5f/%02.5f)",
        vga.draw.delay.htotal,(1.0/vga.draw.delay.htotal),
        vga.draw.delay.hblkstart,vga.draw.delay.hblkend,
        vga.draw.delay.hrstart,vga.draw.delay.hrend);
    LOG(LOG_VGA,LOG_NORMAL)("v total %2.5f (%3.2fHz) blank(%02.5f/%02.5f) retrace(%02.5f/%02.5f)",
        vga.draw.delay.vtotal,(1000.0/vga.draw.delay.vtotal),
        vga.draw.delay.vblkstart,vga.draw.delay.vblkend,
        vga.draw.delay.vrstart,vga.draw.delay.vrend);

    LOG(LOG_VGA,LOG_NORMAL)("video clock: %3.2fMHz mode %s",
        oscclock/1000000.0, mode_texts[vga.mode]);
#endif

    // need to change the vertical timing?
    if (vga_mode_time_base < 0 || fabs(vga.draw.delay.vtotal - 1000.0 / fps) > 0.0001)
        fps_changed = true;

    // need to resize the output window?
    if ((width != vga.draw.width) ||
        (height != vga.draw.height) ||
        (fabs(screenratio - vga.draw.screen_ratio) > 0.0001) ||
        (vga.draw.bpp != bpp) || fps_changed) {

        VGA_KillDrawing();

        vga.draw.width = width;
        vga.draw.height = height;
        vga.draw.screen_ratio = screenratio;
        vga.draw.bpp = bpp;
#if C_DEBUG
        LOG(LOG_VGA,LOG_NORMAL)("%dx%d, %3.2fHz, %dbpp, screen %1.3f",(int)width,(int)height,fps,(int)bpp,screenratio);
#endif
        if (!vga.draw.vga_override)
            RENDER_SetSize(width,height,bpp,(float)fps,screenratio);

        if (fps_changed) {
            vga_mode_time_base = PIC_GetCurrentEventTime();
            vga_mode_frames_since_time_base = 0;
            PIC_RemoveEvents(VGA_Other_VertInterrupt);
            PIC_RemoveEvents(VGA_VerticalTimer);
            PIC_RemoveEvents(VGA_PanningLatch);
            PIC_RemoveEvents(VGA_DisplayStartLatch);
            vga.draw.delay.vtotal = 1000.0 / fps;
            vga.draw.lines_done = vga.draw.lines_total;
            vga_fps = fps;
            VGA_VerticalTimer(0);
        }
    }
    vga.draw.delay.singleline_delay = (float)vga.draw.delay.htotal;

    if (machine == MCH_HERC || machine == MCH_MDA) {
        Herc_Palette();
    }
    else {
        /* FIXME: Why is this required to prevent VGA palette errors with Crystal Dream II?
         *        What is this code doing to change the palette prior to this point? */
        VGA_DAC_UpdateColorPalette();
    }
}

void VGA_KillDrawing(void) {
    PIC_RemoveEvents(VGA_DrawSingleLine);
    PIC_RemoveEvents(VGA_DrawEGASingleLine);
}

void VGA_SetOverride(bool vga_override) {
    if (vga.draw.vga_override!=vga_override) {
        
        if (vga_override) {
            VGA_KillDrawing();
            vga.draw.vga_override=true;
        } else {
            vga.draw.vga_override=false;
            vga.draw.width=0; // change it so the output window gets updated
            VGA_SetupDrawing(0);
        }
    }
}

uint32_t VGA_QuerySizeIG(void) {
    return  ((uint32_t)vga.draw.height << (uint32_t)16ul) |
             (uint32_t)vga.draw.width;
}

void VGA_DumpFontRamBIN(const char *filename) {
	FILE *fp = fopen(filename,"wb");
	if (!fp) {
		LOG_MSG("VGA: Unable to open %s for writing",filename);
		return;
	}

	unsigned int im;
	unsigned int i,j;
	unsigned char tmp[256];
	const uint32_t *plm = (const uint32_t*)vga.mem.linear;

	/* converting total bytes to planar bytes (256KB bytes -> 64KB bytes per bitplane) divide by 256 */
	im = ((vga.mem.memmask + 1u) >> 2u) / 256u;
	if (im > 256) im = 256;

	LOG_MSG("Writing %s as raw %uKB dump of VGA font RAM",filename,(im + 3u) / 4u);

	/* 256*256 == 65536 */
	for (i=0;i < im;i++,plm += 256) {
		for (j=0;j < 256;j++) {
			VGA_Latch p(plm[j]);
			tmp[j] = p.b[2];
		}

		fwrite(tmp,256,1,fp);
	}

	fclose(fp);
}

void VGA_DumpFontRamBMP(const char *filename) {
	FILE *fp = fopen(filename,"wb");
	if (!fp) {
		LOG_MSG("VGA: Unable to open %s for writing",filename);
		return;
	}

	unsigned int row,col,sl;
	unsigned int rowheight = (vga.crtc.maximum_scan_line & 0x1Fu) + 1;
	unsigned char tmp[256];

	unsigned int bits_size = 16u/*width in bytes*/ * (16u * rowheight); /* 1bpp BYTE aligned */
	unsigned int header_size = 40u + (4u * 2u); /* BITMAPINFOHEADER + 2 colors */

	LOG_MSG("Writing %s as %d x %d bitmap rowheight %d",filename,16u * 8u,16u * rowheight,rowheight);

	/* BITMAPFILEHEADER */
	host_writew(tmp+0x00,0x4D42); /* "BM" */
	host_writed(tmp+0x02,bits_size + header_size + 14/*BITMAPFILEHEADER*/);
	host_writew(tmp+0x06,0);
	host_writew(tmp+0x08,0);
	host_writed(tmp+0x0A,14 + header_size);
	fwrite(tmp,14,1,fp); /* write it */

	/* BITMAPINFOHEADER + palette */
	host_writed(tmp+0x00,40); // biSize
	host_writed(tmp+0x04,16u * 8u); // biWidth
	host_writed(tmp+0x08,16u * rowheight); // biHeight
	host_writew(tmp+0x0C,1); // biPlanes
	host_writew(tmp+0x0E,1); // biBitCount
	host_writed(tmp+0x10,0); // biCompression (BI_RGB)
	host_writed(tmp+0x14,bits_size); // biSizeImage
	host_writed(tmp+0x18,0); // biXPelsPerMeter
	host_writed(tmp+0x1C,0); // biYPelsPerMeter
	host_writed(tmp+0x20,2); // biClrUsed
	host_writed(tmp+0x24,2); // biClrImportant
	/* palette (at offset 40) */
	host_writed(tmp+0x28,0x00000000); // XRGB 0,0,0
	host_writed(tmp+0x2C,0x00FFFFFF); // XRGB 255,255,255
	fwrite(tmp,header_size,1,fp);

	const uint32_t *plm = (const uint32_t*)vga.mem.linear;

	/* Scanlines.
	 * NTS: Remember that Windows bitmaps store the scanlines bottom to top (upside down) */
	row = 15u;
	do {
		unsigned int cc = row * 16u;

		sl = rowheight - 1;
		do {
			for (col=0;col < 16;col++) {
				VGA_Latch p(plm[((cc+col)*32u)+sl/*planar byte offset*/]);
				tmp[col] = p.b[2]/*bitplane 2*/;
			}

			fwrite(tmp,16,1,fp);
		} while ((sl--) != 0u); /* stop when sl == 0 */
	} while ((row--) != 0u); /* test pre-decrement to stop when row == 0 */

	/* done */
	fclose(fp);
}

// save state support
void *VGA_DisplayStartLatch_PIC_Event = (void*)((uintptr_t)VGA_DisplayStartLatch);
void *VGA_DrawEGASingleLine_PIC_Event = (void*)((uintptr_t)VGA_DrawEGASingleLine);
//void *VGA_DrawPart_PIC_Event = (void*)VGA_DrawPart;
void *VGA_DrawSingleLine_PIC_Event = (void*)((uintptr_t)VGA_DrawSingleLine);
void *VGA_Other_VertInterrupt_PIC_Event = (void*)((uintptr_t)VGA_Other_VertInterrupt);
void *VGA_PanningLatch_PIC_Event = (void*)((uintptr_t)VGA_PanningLatch);
void *VGA_VertInterrupt_PIC_Event = (void*)((uintptr_t)VGA_VertInterrupt);
void *VGA_VerticalTimer_PIC_Event = (void*)((uintptr_t)VGA_VerticalTimer);


void POD_Save_VGA_Draw( std::ostream& stream )
{
	uint8_t linear_base_idx;
	uint8_t font_tables_idx[2];
	uint8_t drawline_idx;


	if(0) {}
	else if( vga.draw.linear_base == vga.mem.linear ) linear_base_idx = 0;
	//else if( vga.draw.linear_base == vga.fastmem ) linear_base_idx = 1;


	for( int lcv=0; lcv<2; lcv++ ) {
		if(0) {}
		else if( vga.draw.font_tables[lcv] == &(vga.draw.font[0*1024]) ) font_tables_idx[lcv] = 0;
		else if( vga.draw.font_tables[lcv] == &(vga.draw.font[8*1024]) ) font_tables_idx[lcv] = 1;
		else if( vga.draw.font_tables[lcv] == &(vga.draw.font[16*1024]) ) font_tables_idx[lcv] = 2;
		else if( vga.draw.font_tables[lcv] == &(vga.draw.font[24*1024]) ) font_tables_idx[lcv] = 3;
		else if( vga.draw.font_tables[lcv] == &(vga.draw.font[32*1024]) ) font_tables_idx[lcv] = 4;
		else if( vga.draw.font_tables[lcv] == &(vga.draw.font[40*1024]) ) font_tables_idx[lcv] = 5;
		else if( vga.draw.font_tables[lcv] == &(vga.draw.font[48*1024]) ) font_tables_idx[lcv] = 6;
		else if( vga.draw.font_tables[lcv] == &(vga.draw.font[56*1024]) ) font_tables_idx[lcv] = 7;
	}


	if(0) {}
	else if( VGA_DrawLine == VGA_Draw_1BPP_Line ) drawline_idx = 1;
	else if( VGA_DrawLine == VGA_Draw_2BPP_Line ) drawline_idx = 3;
	else if( VGA_DrawLine == VGA_Draw_2BPPHiRes_Line ) drawline_idx = 4;
	else if( VGA_DrawLine == VGA_Draw_CGA16_Line ) drawline_idx = 5;
	else if( VGA_DrawLine == VGA_Draw_4BPP_Line ) drawline_idx = 6;
	else if( VGA_DrawLine == VGA_Draw_4BPP_Line_Double ) drawline_idx = 7;
	else if( VGA_DrawLine == VGA_Draw_Linear_Line ) drawline_idx = 8;
	else if( VGA_DrawLine == VGA_Draw_Xlat32_Linear_Line ) drawline_idx = 9;
	else if( VGA_DrawLine == VGA_Draw_VGA_Line_HWMouse ) drawline_idx = 11;
	else if( VGA_DrawLine == VGA_Draw_LIN16_Line_HWMouse ) drawline_idx = 12;
	else if( VGA_DrawLine == VGA_Draw_LIN32_Line_HWMouse ) drawline_idx = 13;
	else if( VGA_DrawLine == VGA_TEXT_Draw_Line ) drawline_idx = 14;
	else if( VGA_DrawLine == VGA_TEXT_Herc_Draw_Line ) drawline_idx = 15;
	else if( VGA_DrawLine == VGA_TEXT_Xlat32_Draw_Line ) drawline_idx = 17;

	//**********************************************
	//**********************************************

	// - near-pure (struct) data
	WRITE_POD( &vga.draw, vga.draw );


	// - reloc ptr
	WRITE_POD( &linear_base_idx, linear_base_idx );
	WRITE_POD( &font_tables_idx, font_tables_idx );

	//**********************************************
	//**********************************************

	// static globals

	// - reloc function ptr
	WRITE_POD( &drawline_idx, drawline_idx );


	// - pure data
	WRITE_POD( &TempLine, TempLine );


	// - system data
	//WRITE_POD( &vsync, vsync );
	//WRITE_POD( &uservsyncjolt, uservsyncjolt );


	// - pure data
	WRITE_POD( &temp, temp );
	WRITE_POD( &FontMask, FontMask );
	WRITE_POD( &bg_color_index, bg_color_index );
}


void POD_Load_VGA_Draw( std::istream& stream )
{
	uint8_t linear_base_idx;
	uint8_t font_tables_idx[2];
	uint8_t drawline_idx;

	//**********************************************
	//**********************************************

	// - near-pure (struct) data
	READ_POD( &vga.draw, vga.draw );


	// - reloc ptr
	READ_POD( &linear_base_idx, linear_base_idx );
	READ_POD( &font_tables_idx, font_tables_idx );

	//**********************************************
	//**********************************************

	// static globals

	// - reloc function ptr
	READ_POD( &drawline_idx, drawline_idx );


	// - pure data
	READ_POD( &TempLine, TempLine );


	// - system data
	//READ_POD( &vsync, vsync );
	//READ_POD( &uservsyncjolt, uservsyncjolt );


	// - pure data
	READ_POD( &temp, temp );
	READ_POD( &FontMask, FontMask );
	READ_POD( &bg_color_index, bg_color_index );

	//**********************************************
	//**********************************************

	switch( linear_base_idx ) {
		case 0: vga.draw.linear_base = vga.mem.linear; break;
		//case 1: vga.draw.linear_base = vga.fastmem; break;
	}


	for( int lcv=0; lcv<2; lcv++ ) {
		switch( font_tables_idx[lcv] ) {
			case 0: vga.draw.font_tables[lcv] = &(vga.draw.font[0*1024]); break;
			case 1: vga.draw.font_tables[lcv] = &(vga.draw.font[8*1024]); break;
			case 2: vga.draw.font_tables[lcv] = &(vga.draw.font[16*1024]); break;
			case 3: vga.draw.font_tables[lcv] = &(vga.draw.font[24*1024]); break;
			case 4: vga.draw.font_tables[lcv] = &(vga.draw.font[32*1024]); break;
			case 5: vga.draw.font_tables[lcv] = &(vga.draw.font[40*1024]); break;
			case 6: vga.draw.font_tables[lcv] = &(vga.draw.font[48*1024]); break;
			case 7: vga.draw.font_tables[lcv] = &(vga.draw.font[56*1024]); break;
		}
	}


	switch( drawline_idx ) {
		case 1: VGA_DrawLine = VGA_Draw_1BPP_Line; break;
		case 3: VGA_DrawLine = VGA_Draw_2BPP_Line; break;
		case 4: VGA_DrawLine = VGA_Draw_2BPPHiRes_Line; break;
		case 5: VGA_DrawLine = VGA_Draw_CGA16_Line; break;
		case 6: VGA_DrawLine = VGA_Draw_4BPP_Line; break;
		case 7: VGA_DrawLine = VGA_Draw_4BPP_Line_Double; break;
		case 8: VGA_DrawLine = VGA_Draw_Linear_Line; break;
		case 9: VGA_DrawLine = VGA_Draw_Xlat32_Linear_Line; break;
		case 11: VGA_DrawLine = VGA_Draw_VGA_Line_HWMouse; break;
		case 12: VGA_DrawLine = VGA_Draw_LIN16_Line_HWMouse; break;
		case 13: VGA_DrawLine = VGA_Draw_LIN32_Line_HWMouse; break;
		case 14: VGA_DrawLine = VGA_TEXT_Draw_Line; break;
		case 15: VGA_DrawLine = VGA_TEXT_Herc_Draw_Line; break;
		case 17: VGA_DrawLine = VGA_TEXT_Xlat32_Draw_Line; break;
	}
}
