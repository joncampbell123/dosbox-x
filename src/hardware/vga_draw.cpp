/*
 *  Copyright (C) 2002-2020  The DOSBox Team
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
#include <math.h>
#include <stdio.h>
#include "dosbox.h"
#if defined (WIN32)
#include <d3d9.h>
#endif
#include "time.h"
#include "timer.h"
#include "setup.h"
#include "support.h"
#include "video.h"
#include "render.h"
#include "../gui/render_scalers.h"
#include "vga.h"
#include "pic.h"
#include "menu.h"
#include "timer.h"
#include "config.h"
#include "control.h"
#include "../ints/int10.h"
#include "pc98_cg.h"
#include "pc98_gdc.h"
#include "pc98_gdc_const.h"

bool mcga_double_scan = false;

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
    "M_ERROR"
};

#if defined(_MSC_VER)
# pragma warning(disable:4244) /* const fmath::local::uint64_t to double possible loss of data */
# pragma warning(disable:4305) /* truncation from double to float */
#endif

//#undef C_DEBUG
//#define C_DEBUG 1
//#define LOG(X,Y) LOG_MSG

bool et4k_highcolor_half_pixel_rate();

double vga_fps = 70;
double vga_mode_time_base = -1;
int vga_mode_frames_since_time_base = 0;

bool pc98_display_enable = true;

extern bool pc98_40col_text;
extern bool vga_3da_polled;
extern bool vga_page_flip_occurred;
extern bool egavga_per_scanline_hpel;
extern bool vga_enable_hpel_effects;
extern bool vga_enable_hretrace_effects;
extern unsigned int vga_display_start_hretrace;
extern float hretrace_fx_avg_weight;
extern bool ignore_vblank_wraparound;
extern bool vga_double_buffered_line_compare;
extern bool pc98_crt_mode;      // see port 6Ah command 40h/41h.

extern bool pc98_31khz_mode;

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

void VGA_Draw2_Recompute_CRTC_MaskAdd(void) {
    if (IS_PC98_ARCH) {
        // nothing yet
    }
    else if (IS_EGAVGA_ARCH) {
        /* mem masking can be generalized for ALL VGA/SVGA modes */
        size_t new_mask = vga.mem.memmask >> (2ul + vga.config.addr_shift);
        size_t new_add = 0;

        if (vga.config.compatible_chain4 || svgaCard == SVGA_None)
            new_mask &= 0xFFFFul >> vga.config.addr_shift; /* 64KB planar (256KB linear when byte mode) */

        /* CGA/Hercules compatible interlacing, unless SVGA graphics mode.
         * Note that ET4000 and ET3000 emulation will NOT set compatible_chain4 */
        if (vga.config.compatible_chain4 || svgaCard == SVGA_None || svgaCard == SVGA_TsengET3K || svgaCard == SVGA_TsengET4K) {
            /* MAP13: If zero, bit 13 is taken from bit 0 of row scan counter (CGA compatible) */
            /* MAP14: If zero, bit 14 is taken from bit 1 of row scan counter (Hercules compatible) */
            if ((vga.crtc.mode_control & 3u) != 3u) {
                const unsigned int shift = 13u - vga.config.addr_shift;
                const unsigned char mask = (vga.crtc.mode_control & 3u) ^ 3u;

                new_mask &= (size_t)(~(size_t(mask) << shift));
                new_add  += (size_t)(vga.draw_2[0].vert.current_char_pixel & mask) << shift;
            }
        }

        /* 4 bitplanes are represented in emulation as 32 bits per planar byte */
        vga.draw_2[0].draw_base = vga.mem.linear;
        vga.draw_2[0].crtc_mask = (unsigned int)new_mask;  // 8KB character clocks (16KB bytes)
        vga.draw_2[0].crtc_add = (unsigned int)new_add;
    }
    else if (machine == MCH_HERC) {
        vga.draw_2[0].draw_base = vga.tandy.mem_base;

        if (vga.herc.mode_control & 2) { /* graphics */
            vga.draw_2[0].crtc_mask = 0xFFFu;  // 4KB character clocks (8KB bytes)
            vga.draw_2[0].crtc_add = (vga.draw_2[0].vert.current_char_pixel & 3u) << 12u;
        }
        else { /* text */
            vga.draw_2[0].crtc_mask = 0x7FFu;  // 2KB character clocks (4KB bytes)
            vga.draw_2[0].crtc_add = 0;
        }
    }
    else if (machine == MCH_MDA) {
        /* MDA/Hercules is emulated as 16 bits per character clock */
        vga.draw_2[0].draw_base = vga.mem.linear;
        vga.draw_2[0].crtc_mask = 0x7FFu;  // 2KB character clocks (4KB bytes)
        vga.draw_2[0].crtc_add = 0;
    }
    else {
        /* TODO: PCjr/Tandy 16-color extended modes */

        /* CGA/MCGA/PCJr/Tandy is emulated as 16 bits per character clock */
        /* PCJr uses system memory < 128KB for video memory.
         * Tandy has an alternate location as well. */
        if (machine == MCH_TANDY || machine == MCH_PCJR)
            vga.draw_2[0].draw_base = vga.tandy.mem_base;
        else
            vga.draw_2[0].draw_base = vga.mem.linear;

        if (vga.tandy.mode_control & 0x2) { /*graphics*/
            vga.draw_2[0].crtc_mask = 0xFFFu;  // 4KB character clocks (8KB bytes)
            vga.draw_2[0].crtc_add = (vga.draw_2[0].vert.current_char_pixel & 1u) << 12u;
        }
        else { /*text*/
            vga.draw_2[0].crtc_mask = 0x1FFFu;  // 8KB character clocks (16KB bytes)
            vga.draw_2[0].crtc_add = 0;
        }
    }
}

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

    for (Bitu x=0;x<vga.draw.blocks;x++) {
        pixels.d = base[vidstart & vga.tandy.addr_mask];
        vidstart += (Bitu)1u << (Bitu)vga.config.addr_shift;

        /* CGA odd/even mode, first plane */
        Bitu val=pixels.b[0];
        for (Bitu i=0;i < 4;i++,val <<= 2)
            *draw++ = vga.attr.palette[(val>>6)&3];

        /* CGA odd/even mode, second plane */
        val=pixels.b[1];
        for (Bitu i=0;i < 4;i++,val <<= 2)
            *draw++ = vga.attr.palette[(val>>6)&3];
    }
    return TempLine;
}

static uint8_t * VGA_Draw_2BPP_Line_as_VGA(Bitu vidstart, Bitu line) {
    const uint32_t *base = (uint32_t*)vga.draw.linear_base + ((line & vga.tandy.line_mask) << vga.tandy.line_shift);
    uint32_t * draw=(uint32_t *)TempLine;
    VGA_Latch pixels;

    for (Bitu x=0;x<vga.draw.blocks;x++) {
        pixels.d = base[vidstart & vga.tandy.addr_mask];
        vidstart += (Bitu)1u << (Bitu)vga.config.addr_shift;

        /* CGA odd/even mode, first plane */
        Bitu val=pixels.b[0];
        for (Bitu i=0;i < 4;i++,val <<= 2)
            *draw++ = vga.dac.xlat32[(val>>6)&3];

        /* CGA odd/even mode, second plane */
        val=pixels.b[1];
        for (Bitu i=0;i < 4;i++,val <<= 2)
            *draw++ = vga.dac.xlat32[(val>>6)&3];
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
    const uint8_t *base = (uint8_t*)vga.tandy.draw_base + ((line & vga.tandy.line_mask) << vga.tandy.line_shift);
    uint32_t * draw=(uint32_t *)TempLine;

    for (Bitu x=0;x<vga.draw.blocks;x++) {
        Bitu val = base[vidstart & vga.tandy.addr_mask];
        vidstart++;

        for (Bitu i=0;i < 8;i++,val <<= 1)
            *draw++ = vga.dac.xlat32[(val>>7)&1];
    }
    return TempLine;
}

static uint8_t * VGA_Draw_1BPP_Line_as_VGA(Bitu vidstart, Bitu line) {
    const uint32_t *base = (uint32_t*)vga.draw.linear_base + ((line & vga.tandy.line_mask) << vga.tandy.line_shift);
    uint32_t * draw=(uint32_t *)TempLine;
    VGA_Latch pixels;

    for (Bitu x=0;x<vga.draw.blocks;x++) {
        pixels.d = base[vidstart & vga.tandy.addr_mask];
        vidstart += (Bitu)1u << (Bitu)vga.config.addr_shift;

        Bitu val=pixels.b[0];
        for (Bitu i=0;i < 8;i++,val <<= 1)
            *draw++ = vga.dac.xlat32[(val>>7)&1];
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

#if SDL_BYTEORDER == SDL_LIL_ENDIAN && defined(MACOSX) /* Mac OS X Intel builds use a weird RGBA order (alpha in the low 8 bits) */
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
#if SDL_BYTEORDER == SDL_LIL_ENDIAN && defined(MACOSX) /* Mac OS X Intel builds use a weird RGBA order (alpha in the low 8 bits) */
    for (i=0;i < vga.draw.width;i++)
        ((uint32_t*)TempLine)[i] = guest_bgr_to_macosx_rgba(*((uint32_t*)(vga.draw.linear_base+offset+(i*3)))) | 0x000000FF;
#else
    for (i=0;i < vga.draw.width;i++)
        ((uint32_t*)TempLine)[i] = *((uint32_t*)(vga.draw.linear_base+offset+(i*3))) | 0xFF000000;
#endif

    return TempLine;
}

static uint8_t * VGA_Draw_Linear_Line(Bitu vidstart, Bitu /*line*/) {
    Bitu offset = vidstart & vga.draw.linear_mask;
    uint8_t* ret = &vga.draw.linear_base[offset];
    
    // in case (vga.draw.line_length + offset) has bits set that
    // are not set in the mask: ((x|y)!=y) equals (x&~y)
    if (GCC_UNLIKELY(((vga.draw.line_length + offset) & (~vga.draw.linear_mask)) != 0u)) {
        // this happens, if at all, only once per frame (1 of 480 lines)
        // in some obscure games
        Bitu end = (Bitu)((Bitu)offset + (Bitu)vga.draw.line_length) & (Bitu)vga.draw.linear_mask;
        
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

static void Alt_VGA_256color_CharClock(uint32_t* &temps,const VGA_Latch &pixels) {
    /* one group of 4 */
    *temps++ = vga.dac.xlat32[pixels.b[0]];
    *temps++ = vga.dac.xlat32[pixels.b[1]];
    *temps++ = vga.dac.xlat32[pixels.b[2]];
    *temps++ = vga.dac.xlat32[pixels.b[3]];
}

static uint8_t * Alt_VGA_256color_Draw_Line_Tseng_ET4000(Bitu /*vidstart*/, Bitu /*line*/) {
    uint32_t* temps = (uint32_t*) TempLine;
    Bitu count = vga.draw.blocks;

    // Tseng ET4000 cards in 256-color mode appear to treat DWORD mode the same as BYTE mode,
    // which is why you can directly draw into the first 128KB and make it visible and even
    // pan to it. Most SVGA cards have DWORD mode ON and wrap 64KB in the stock 256-color mode.
    const unsigned int shift = (vga.config.addr_shift & 1);

    while (count > 0u) {
        const unsigned int addr = vga.draw_2[0].crtc_addr_fetch_and_advance();
        VGA_Latch pixels(*vga.draw_2[0].drawptr<uint32_t>(addr << shift));
        Alt_VGA_256color_CharClock(temps,pixels);
        count--;
    }

    return TempLine;
}

static uint8_t * Alt_VGA_256color_Draw_Line(Bitu /*vidstart*/, Bitu /*line*/) {
    uint32_t* temps = (uint32_t*) TempLine;
    Bitu count = vga.draw.blocks;

    while (count > 0u) {
        const unsigned int addr = vga.draw_2[0].crtc_addr_fetch_and_advance();
        VGA_Latch pixels(*vga.draw_2[0].drawptr<uint32_t>(addr << vga.config.addr_shift));
        Alt_VGA_256color_CharClock(temps,pixels);
        count--;
    }

    return TempLine;
}

#define LOAD_NEXT_PIXEL(n)  nex = pixels.b[n]
#define SHIFTED_PIXEL       *temps++ = vga.dac.xlat32[((cur << 4u) + (nex >> 4u)) & 0xFFu]
#define UNSHIFTED_PIXEL     *temps++ = vga.dac.xlat32[nex]; cur = nex

template <const unsigned int pixelcount> static inline void Alt_VGA_256color_2x4bit_Draw_CharClock(uint32_t* &temps,const VGA_Latch &pixels,unsigned char &cur,unsigned char &nex) {
/* NTS:
 *   pixels == 7 for first char clock on the line
 *   pixels == 8 for the rest of the char clocks
 *   pixels == 1 for the char clock on the end */

/* Real VGA hardware appears to have the first 8-bit pixel fully latched for
 * the first pixel on the scanline when display enable starts. In that case,
 * pixels == 7.
 *
 * After that, intermediate states are visible across the scan line,
 * pixels == 8.
 *
 * The first pixel past end of active display (normally not visible), it's
 * top nibble can be seen as the last clocked out pixel before end of
 * active display, pixels == 1 */

    if (pixelcount == 1) {
        LOAD_NEXT_PIXEL(0);
        SHIFTED_PIXEL;
    }
    else if (pixelcount == 7) {
        LOAD_NEXT_PIXEL(0);
        UNSHIFTED_PIXEL;
    }
    else {
        LOAD_NEXT_PIXEL(0);
        SHIFTED_PIXEL;
        UNSHIFTED_PIXEL;
    }

    if (pixelcount >= 7) {
        LOAD_NEXT_PIXEL(1);
        SHIFTED_PIXEL;
        UNSHIFTED_PIXEL;

        LOAD_NEXT_PIXEL(2);
        SHIFTED_PIXEL;
        UNSHIFTED_PIXEL;

        LOAD_NEXT_PIXEL(3);
        SHIFTED_PIXEL;
        UNSHIFTED_PIXEL;
    }
}

#undef LOAD_NEXT_PIXEL
#undef SHIFTED_PIXEL
#undef UNSHIFTED_PIXEL

/* 256-color mode with 8BIT=0, in which the intermediate shift states are visible between
 * each 8-bit pixel, producing a weird 640x200 256-color mode.
 *
 * Not all SVGA cards emulate this. Tseng ET4000 for example will react by just rendering
 * the 320 pixels horizontally squeezed on the left half of the screen and nothing on the right. */
static uint8_t * Alt_VGA_256color_2x4bit_Draw_Line(Bitu /*vidstart*/, Bitu /*line*/) {
    uint32_t* temps = (uint32_t*) TempLine;
    Bitu count = vga.draw.blocks;

    if (count > 0u) {
        unsigned char cur,nex;
        /* on VGA hardware I've seen, the first pixel is the full 8-bit pixel value of the FIRST pixel in memory. */
        unsigned int addr = vga.draw_2[0].crtc_addr_fetch_and_advance();
        VGA_Latch pixels(*vga.draw_2[0].drawptr<uint32_t>(addr << vga.config.addr_shift));
        Alt_VGA_256color_2x4bit_Draw_CharClock<7>(temps,pixels,cur,nex);
        count--;

        while (count > 0u) {
            addr = vga.draw_2[0].crtc_addr_fetch_and_advance();
            VGA_Latch pixels2(*vga.draw_2[0].drawptr<uint32_t>(addr << vga.config.addr_shift));
            Alt_VGA_256color_2x4bit_Draw_CharClock<8>(temps,pixels2,cur,nex);
            count--;
        }

        /* the top nibble of the first pixel past the end is visible on real hardware */
        {
            addr = vga.draw_2[0].crtc_addr_fetch_and_advance();
            VGA_Latch pixels2(*vga.draw_2[0].drawptr<uint32_t>(addr << vga.config.addr_shift));
            Alt_VGA_256color_2x4bit_Draw_CharClock<1>(temps,pixels,cur,nex);
        }
    }

    return TempLine;
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

        vidstart += (Bitu)((int)x);
    }

    for(Bitu i = 0; i < (vga.draw.line_length>>2); i++)
        temps[i]=vga.dac.xlat32[vga.draw.linear_base[(vidstart+i)&vga.draw.linear_mask]];

    return TempLine;
}

extern uint32_t Expand16Table[4][16];

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

template <const unsigned int card,typename templine_type_t> static inline void Alt_EGA_Planar_Common_Block(templine_type_t * &temps,const uint32_t t) {
    uint32_t tmp;

    tmp =   Expand16Table[0][(t >>  4)&0xF] |
            Expand16Table[1][(t >> 12)&0xF] |
            Expand16Table[2][(t >> 20)&0xF] |
            Expand16Table[3][(t >> 28)&0xF];
    temps[0] = EGA_Planar_Common_Block_xlat<card,templine_type_t>((tmp>> 0ul)&0xFFul);
    temps[1] = EGA_Planar_Common_Block_xlat<card,templine_type_t>((tmp>> 8ul)&0xFFul);
    temps[2] = EGA_Planar_Common_Block_xlat<card,templine_type_t>((tmp>>16ul)&0xFFul);
    temps[3] = EGA_Planar_Common_Block_xlat<card,templine_type_t>((tmp>>24ul)&0xFFul);

    tmp =   Expand16Table[0][(t >>  0)&0xF] |
            Expand16Table[1][(t >>  8)&0xF] |
            Expand16Table[2][(t >> 16)&0xF] |
            Expand16Table[3][(t >> 24)&0xF];
    temps[4] = EGA_Planar_Common_Block_xlat<card,templine_type_t>((tmp>> 0ul)&0xFFul);
    temps[5] = EGA_Planar_Common_Block_xlat<card,templine_type_t>((tmp>> 8ul)&0xFFul);
    temps[6] = EGA_Planar_Common_Block_xlat<card,templine_type_t>((tmp>>16ul)&0xFFul);
    temps[7] = EGA_Planar_Common_Block_xlat<card,templine_type_t>((tmp>>24ul)&0xFFul);

    temps += 8;
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

template <const unsigned int card,typename templine_type_t> static uint8_t * Alt_EGA_Planar_Common_Line() {
    templine_type_t* temps = (templine_type_t*)TempLine;
    Bitu count = vga.draw.blocks + ((vga.draw.panning + 7u) >> 3u);

    while (count > 0u) {
        const unsigned int addr = vga.draw_2[0].crtc_addr_fetch_and_advance();
        VGA_Latch pixels(*vga.draw_2[0].drawptr<uint32_t>(addr << vga.config.addr_shift));
        Alt_EGA_Planar_Common_Block<card,templine_type_t>(temps,pixels.d);
        count--;
    }

    return TempLine + (vga.draw.panning*sizeof(templine_type_t));
}

static uint8_t * Alt_EGA_Planar_Draw_Line(Bitu /*vidstart*/, Bitu /*line*/) {
    return Alt_EGA_Planar_Common_Line<MCH_EGA,uint8_t>();
}

static uint8_t * Alt_VGA_Planar_Draw_Line(Bitu /*vidstart*/, Bitu /*line*/) {
    return Alt_EGA_Planar_Common_Line<MCH_VGA,uint32_t>();
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

static uint8_t * VGA_Draw_VGA_Line_Xlat32_HWMouse( Bitu vidstart, Bitu /*line*/) {
    if (!svga.hardware_cursor_active || !svga.hardware_cursor_active())
        // HW Mouse not enabled, use the tried and true call
        return VGA_Draw_Xlat32_Linear_Line(vidstart, 0);

    Bitu lineat = (vidstart-(vga.config.real_start<<2)) / vga.draw.width;
    if ((vga.s3.hgc.posx >= vga.draw.width) ||
        (lineat < vga.s3.hgc.originy) ||
        (lineat > (vga.s3.hgc.originy + (63U-vga.s3.hgc.posy))) ) {
        // the mouse cursor *pattern* is not on this line
        return VGA_Draw_Xlat32_Linear_Line(vidstart, 0);
    } else {
        // Draw mouse cursor: cursor is a 64x64 pattern which is shifted (inside the
        // 64x64 mouse cursor space) to the right by posx pixels and up by posy pixels.
        // This is used when the mouse cursor partially leaves the screen.
        // It is arranged as bitmap of 16bits of bitA followed by 16bits of bitB, each
        // AB bits corresponding to a cursor pixel. The whole map is 8kB in size.
        uint32_t* temp2 = (uint32_t*)VGA_Draw_Xlat32_Linear_Line(vidstart, 0);
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
        uint32_t* xat = &temp2[vga.s3.hgc.originx]; // mouse data start pos. in scanline
        for (Bitu m = cursorMemStart; m < cursorMemEnd; (m&1)?(m+=3):m++) {
            // for each byte of cursor data
            uint8_t bitsA = vga.mem.linear[m];
            uint8_t bitsB = vga.mem.linear[m+2];
            for (uint8_t bit=(0x80 >> cursorStartBit); bit != 0; bit >>= 1) {
                // for each bit
                cursorStartBit=0; // only the first byte has some bits cut off
                if (bitsA&bit) {
                    if (bitsB&bit) *xat ^= 0xFFFFFFFF; // Invert screen data
                    //else Transparent
                } else if (bitsB&bit) {
                    *xat = vga.dac.xlat32[vga.s3.hgc.forestack[0]]; // foreground color
                } else {
                    *xat = vga.dac.xlat32[vga.s3.hgc.backstack[0]];
                }
                xat++;
            }
        }
        return (uint8_t*)temp2;
    }
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
        uint8_t* xat = &TempLine[vga.s3.hgc.originx]; // mouse data start pos. in scanline
        for (Bitu m = cursorMemStart; m < cursorMemEnd; (m&1)?(m+=3):m++) {
            // for each byte of cursor data
            uint8_t bitsA = vga.mem.linear[m];
            uint8_t bitsB = vga.mem.linear[m+2];
            for (uint8_t bit=(0x80 >> cursorStartBit); bit != 0; bit >>= 1) {
                // for each bit
                cursorStartBit=0; // only the first byte has some bits cut off
                if (bitsA&bit) {
                    if (bitsB&bit) *xat ^= 0xFF; // Invert screen data
                    //else Transparent
                } else if (bitsB&bit) {
                    *xat = vga.s3.hgc.forestack[0]; // foreground color
                } else {
                    *xat = vga.s3.hgc.backstack[0];
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
        memcpy(TempLine, &vga.mem.linear[ vidstart ], vga.draw.width*2);
        Bitu sourceStartBit = ((lineat - vga.s3.hgc.originy) + vga.s3.hgc.posy)*64 + vga.s3.hgc.posx; 
        Bitu cursorMemStart = ((sourceStartBit >> 2) & ~1ul) + (((uint32_t)vga.s3.hgc.startaddr) << 10ul);
        Bitu cursorStartBit = sourceStartBit & 0x7u;
        if (cursorMemStart & 0x2) cursorMemStart--;
        Bitu cursorMemEnd = cursorMemStart + (Bitu)((64 - vga.s3.hgc.posx) >> 2);
        uint16_t* xat = &((uint16_t*)TempLine)[vga.s3.hgc.originx];
        for (Bitu m = cursorMemStart; m < cursorMemEnd; (m&1)?(m+=3):m++) {
            // for each byte of cursor data
            uint8_t bitsA = vga.mem.linear[m];
            uint8_t bitsB = vga.mem.linear[m+2];
            for (uint8_t bit=(0x80 >> cursorStartBit); bit != 0; bit >>= 1) {
                // for each bit
                cursorStartBit=0;
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
                xat++;
            }
        }
        return TempLine;
    }
}

static uint8_t * VGA_Draw_LIN32_Line_HWMouse(Bitu vidstart, Bitu /*line*/) {
#if SDL_BYTEORDER == SDL_LIL_ENDIAN && defined(MACOSX) /* Mac OS X Intel builds use a weird RGBA order (alpha in the low 8 bits) */
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
        memcpy(TempLine, &vga.mem.linear[ vidstart ], vga.draw.width*4);
        Bitu sourceStartBit = ((lineat - vga.s3.hgc.originy) + vga.s3.hgc.posy)*64 + vga.s3.hgc.posx; 
        Bitu cursorMemStart = ((sourceStartBit >> 2) & ~1ul) + (((uint32_t)vga.s3.hgc.startaddr) << 10ul);
        Bitu cursorStartBit = sourceStartBit & 0x7u;
        if (cursorMemStart & 0x2) cursorMemStart--;
        Bitu cursorMemEnd = cursorMemStart + (Bitu)((64 - vga.s3.hgc.posx) >> 2);
        uint32_t* xat = &((uint32_t*)TempLine)[vga.s3.hgc.originx];
        for (Bitu m = cursorMemStart; m < cursorMemEnd; (m&1)?(m+=3):m++) {
            // for each byte of cursor data
            uint8_t bitsA = vga.mem.linear[m];
            uint8_t bitsB = vga.mem.linear[m+2];
            for (uint8_t bit=(0x80 >> cursorStartBit); bit != 0; bit >>= 1) { // for each bit
                cursorStartBit=0;
                if (bitsA&bit) {
                    if (bitsB&bit) *xat ^= ~0U;
                    //else Transparent
                } else if (bitsB&bit) {
                    *xat = *(uint32_t*)vga.s3.hgc.forestack;
                } else {
                    *xat = *(uint32_t*)vga.s3.hgc.backstack;
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

template <const unsigned int card,typename templine_type_t> static inline void Alt_EGAVGA_Common_2BPP_Draw_Line_CharClock(templine_type_t* &draw,const VGA_Latch &pixels) {
    unsigned int val,val2;

    /* CGA odd/even mode, first plane */
    val = pixels.b[0];
    val2 = (unsigned int)pixels.b[2] << 2u;
    for (unsigned int i=0;i < 4;i++,val <<= 2,val2 <<= 2)
        *draw++ = EGA_Planar_Common_Block_xlat<card,templine_type_t>(((val>>6)&0x3) + ((val2>>6)&0xC));

    /* CGA odd/even mode, second plane */
    val = pixels.b[1];
    val2 = (unsigned int)pixels.b[3] << 2u;
    for (unsigned int i=0;i < 4;i++,val <<= 2,val2 <<= 2)
        *draw++ = EGA_Planar_Common_Block_xlat<card,templine_type_t>(((val>>6)&0x3) + ((val2>>6)&0xC));
}

template <const unsigned int card,typename templine_type_t> static inline uint8_t *Alt_EGAVGA_Common_2BPP_Draw_Line(void) {
    templine_type_t* draw = (templine_type_t*)TempLine;
    Bitu blocks = vga.draw.blocks;

    while (blocks--) { // for each character in the line
        const unsigned int addr = vga.draw_2[0].crtc_addr_fetch_and_advance();
        VGA_Latch pixels(*vga.draw_2[0].drawptr<uint32_t>(addr << vga.config.addr_shift));
        Alt_EGAVGA_Common_2BPP_Draw_Line_CharClock<card,templine_type_t>(draw,pixels);
    }

    return TempLine;
}

static uint8_t *Alt_EGA_2BPP_Draw_Line(Bitu /*vidstart*/, Bitu /*line*/) {
    return Alt_EGAVGA_Common_2BPP_Draw_Line<MCH_EGA,uint8_t>();
}

static uint8_t *Alt_VGA_2BPP_Draw_Line(Bitu /*vidstart*/, Bitu /*line*/) {
    return Alt_EGAVGA_Common_2BPP_Draw_Line<MCH_VGA,uint32_t>();
}

static uint8_t *Alt_CGA_2color_Draw_Line(Bitu /*vidstart*/, Bitu /*line*/) {
    uint32_t* draw = (uint32_t*)TempLine; // NTS: This is typecast in this way only to write 4 pixels at once at 8bpp
    Bitu blocks = vga.draw.blocks;

    while (blocks--) { // for each character in the line
        const unsigned int addr = vga.draw_2[0].crtc_addr_fetch_and_advance();
        CGA_Latch pixels(*vga.draw_2[0].drawptr<uint16_t>(addr));

        *draw++=CGA_2_Table[pixels.b[0] >> 4];
        *draw++=CGA_2_Table[pixels.b[0] & 0xf];

        *draw++=CGA_2_Table[pixels.b[1] >> 4];
        *draw++=CGA_2_Table[pixels.b[1] & 0xf];
    }

    return TempLine;
}

static uint8_t *Alt_CGA_4color_Draw_Line(Bitu /*vidstart*/, Bitu /*line*/) {
    uint32_t* draw = (uint32_t*)TempLine; // NTS: This is typecast in this way only to write 4 pixels at once at 8bpp
    Bitu blocks = vga.draw.blocks;

    while (blocks--) { // for each character in the line
        const unsigned int addr = vga.draw_2[0].crtc_addr_fetch_and_advance();
        CGA_Latch pixels(*vga.draw_2[0].drawptr<uint16_t>(addr));

        *draw++=CGA_4_Table[pixels.b[0]];
        *draw++=CGA_4_Table[pixels.b[1]];
    }

    return TempLine;
}

static inline unsigned int Alt_CGA_TEXT_Load_Font_Bitmap(const unsigned char chr,const unsigned char attr,const unsigned int line) {
    return vga.draw.font_tables[((unsigned int)attr >> 3u) & 1u][((unsigned int)chr << 5u) + line];
}

static inline bool Alt_CGA_TEXT_In_Cursor_Row(const unsigned int line) {
    return
        ((vga.draw.cursor.count&0x8) && (line >= vga.draw.cursor.sline) &&
        (line <= vga.draw.cursor.eline) && vga.draw.cursor.enabled);
}

// NTS: 8bpp typecast as uint32_t to speedily draw characters
static inline void Alt_CGA_TEXT_Combined_Draw_Line_RenderBMP(uint32_t* &draw,unsigned int font,unsigned char attr) {
    const uint32_t mask1=TXT_Font_Table[font>>4] & FontMask[attr >> 7];
    const uint32_t mask2=TXT_Font_Table[font&0xf] & FontMask[attr >> 7];
    const uint32_t fg=TXT_FG_Table[attr&0xf];
    const uint32_t bg=TXT_BG_Table[attr>>4];

    *draw++=(fg&mask1) | (bg&~mask1);
    *draw++=(fg&mask2) | (bg&~mask2);
}

static inline unsigned char Alt_CGA_TEXT_Load_Font_Bitmap(const unsigned char chr,const unsigned char attr,const unsigned char line,const unsigned int addr,const bool in_cursor_row) {
    if (GCC_UNLIKELY(in_cursor_row) && addr == vga.config.cursor_start) // cursor
        return 0xff;
    else // the font pattern
        return Alt_CGA_TEXT_Load_Font_Bitmap(chr,attr,line);
}

template <const bool snow> static uint8_t * Alt_CGA_COMMON_TEXT_Draw_Line(void) {
    // keep it aligned:
    uint32_t* draw = (uint32_t*)TempLine; // NTS: This is typecast in this way only to write 4 pixels at once at 8bpp
    Bitu blocks = vga.draw.blocks;

    const unsigned int line = vga.draw_2[0].vert.current_char_pixel & 7;
    const bool in_cursor_row = Alt_CGA_TEXT_In_Cursor_Row(line);

    unsigned int cx = 0;

    if (snow) {
        /* HACK: our code does not have render control during VBLANK, zero our
         *       noise bits on the first scanline */
        if (vga.draw_2[0].vert.current.pixels == 0)
            memset(vga.draw.cga_snow,0,sizeof(vga.draw.cga_snow));
    }

    while (blocks--) { // for each character in the line
        const unsigned int addr = vga.draw_2[0].crtc_addr_fetch_and_advance();
        CGA_Latch pixels(*vga.draw_2[0].drawptr<uint16_t>(addr));

        unsigned char chr = pixels.b[0];
        unsigned char attr = pixels.b[1];

        if (snow && (cx&1) == 0 && cx <= 78) {
            /* Trixter's "CGA test" program and reference video seems to suggest
             * to me that the CGA "snow" might contain the value written by the CPU. */
            if (vga.draw.cga_snow[cx] != 0)
                chr = vga.draw.cga_snow[cx];
            if (vga.draw.cga_snow[cx+1] != 0)
                attr = vga.draw.cga_snow[cx+1];
        }

        Alt_CGA_TEXT_Combined_Draw_Line_RenderBMP(draw,
            Alt_CGA_TEXT_Load_Font_Bitmap(chr,attr,line,addr,in_cursor_row),attr);

        cx++;
    }

    if (snow)
        memset(vga.draw.cga_snow,0,sizeof(vga.draw.cga_snow));

    return TempLine;
}

static uint8_t * Alt_CGA_TEXT_Draw_Line(Bitu /*vidstart*/, Bitu /*line*/) {
    return Alt_CGA_COMMON_TEXT_Draw_Line<false>();
}

static uint8_t * Alt_CGA_CGASNOW_TEXT_Draw_Line(Bitu /*vidstart*/, Bitu /*line*/) {
    return Alt_CGA_COMMON_TEXT_Draw_Line<true>();
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
        // the adress of the attribute that makes up the cell the cursor is in
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

static inline unsigned int Alt_MDA_TEXT_Load_Font_Bitmap(const unsigned char chr,const unsigned int line) {
    return vga.draw.font_tables[0u][((unsigned int)chr << 5u) + line];
}

static inline bool Alt_MDA_TEXT_In_Cursor_Row(const unsigned int line) {
    return
        ((vga.draw.cursor.count&0x8) && (line >= vga.draw.cursor.sline) &&
        (line <= vga.draw.cursor.eline) && vga.draw.cursor.enabled);
}

// NTS: 8bpp typecast as uint32_t to speedily draw characters
static inline void Alt_MDA_TEXT_Combined_Draw_Line_RenderBMP(uint32_t* &draw,unsigned int font,unsigned char attrib) {
    if (!(attrib&0x77)) {
        // 00h, 80h, 08h, 88h produce black space
        *draw++=0;
        *draw++=0;
    } else {
        uint32_t bg, fg;

        if ((attrib&0x77)==0x70) {
            bg = TXT_BG_Table[0x7];
            if (attrib&0x8) fg = TXT_FG_Table[0xf];
            else fg = TXT_FG_Table[0x0];
        } else {
            bg = TXT_BG_Table[0x0];
            if (attrib&0x8) fg = TXT_FG_Table[0xf];
            else fg = TXT_FG_Table[0x7];
        }

        const uint32_t mask1=TXT_Font_Table[font>>4] & FontMask[attrib >> 7]; // blinking
        const uint32_t mask2=TXT_Font_Table[font&0xf] & FontMask[attrib >> 7];
        *draw++=(fg&mask1) | (bg&~mask1);
        *draw++=(fg&mask2) | (bg&~mask2);
    }
}

static inline unsigned char Alt_MDA_TEXT_Load_Font_Bitmap(const unsigned char chr,const unsigned char attrib,const unsigned char line,const unsigned int addr,const bool in_cursor_row) {
    if (GCC_UNLIKELY(in_cursor_row) && addr == vga.config.cursor_start) // cursor
        return 0xff;
    else if ((attrib&0x77) == 0x01 && ((Bitu)(vga.crtc.underline_location&0x1f)==line)) // underline
        return 0xff;
    else // the font pattern
        return Alt_MDA_TEXT_Load_Font_Bitmap(chr,line);
}

static uint8_t * Alt_MDA_COMMON_TEXT_Draw_Line(void) {
    // keep it aligned:
    uint32_t* draw = (uint32_t*)TempLine; // NTS: This is typecast in this way only to write 4 pixels at once at 8bpp
    Bitu blocks = vga.draw.blocks;

    const unsigned int line = vga.draw_2[0].vert.current_char_pixel & 15;
    const bool in_cursor_row = Alt_MDA_TEXT_In_Cursor_Row(line);

    while (blocks--) { // for each character in the line
        const unsigned int addr = vga.draw_2[0].crtc_addr_fetch_and_advance();
        CGA_Latch pixels(*vga.draw_2[0].drawptr<uint16_t>(addr));

        unsigned char chr = pixels.b[0];
        unsigned char attr = pixels.b[1];

        Alt_MDA_TEXT_Combined_Draw_Line_RenderBMP(draw,
            Alt_MDA_TEXT_Load_Font_Bitmap(chr,attr,line,addr,in_cursor_row),attr);
    }

    return TempLine;
}

static uint8_t * Alt_MDA_TEXT_Draw_Line(Bitu /*vidstart*/, Bitu /*line*/) {
    return Alt_MDA_COMMON_TEXT_Draw_Line();
}

template <const unsigned int card,typename templine_type_t> static inline uint8_t* EGAVGA_TEXT_Combined_Draw_Line(Bitu vidstart,Bitu line) {
    // keep it aligned:
    templine_type_t* draw = ((templine_type_t*)TempLine) + 16 - vga.draw.panning;
    const uint32_t* vidmem = VGA_Planar_Memwrap(vidstart); // pointer to chars+attribs
    Bitu blocks = vga.draw.blocks;
    if (vga.draw.panning) blocks++; // if the text is panned part of an 
                                    // additional character becomes visible

    while (blocks--) { // for each character in the line
        VGA_Latch pixels;

        pixels.d = *vidmem;
        vidmem += (uintptr_t)1U << (uintptr_t)vga.config.addr_shift;

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
    // draw the text mode cursor if needed
    if ((vga.draw.cursor.count&0x8) && (line >= vga.draw.cursor.sline) &&
        (line <= vga.draw.cursor.eline) && vga.draw.cursor.enabled) {
        // the adress of the attribute that makes up the cell the cursor is in
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

template <const unsigned int card,typename templine_type_t,const unsigned int pixels> static inline void Alt_EGAVGA_TEXT_Combined_Draw_Line_RenderBMP(templine_type_t* &draw,unsigned int font,const unsigned char foreground,const unsigned char background) {
    const unsigned int fontmask = 1u << (pixels - 1u);

    for (unsigned int n = 0; n < pixels; n++) {
        if (card == MCH_VGA)
            *draw++ = vga.dac.xlat32[(font&fontmask)? foreground:background];
        else /*MCH_EGA*/
            *draw++ = vga.attr.palette[(font&fontmask)? foreground:background];

        font <<= 1;
    }
}

template <const unsigned int card,const unsigned int pixelsperchar> inline unsigned int Alt_VGA_Alpha8to9Expand(unsigned int font,const unsigned char chr) {
    if (pixelsperchar == 9) {
        font <<= 1; // 9 pixels

        // extend to the 9th pixel if needed
        if ((font&0x2) && (vga.attr.mode_control&0x04) && (chr>=0xc0) && (chr<=0xdf)) font |= 1;
    }

    return font;
}
 
template <const unsigned int card> static inline unsigned int Alt_EGAVGA_TEXT_Load_Font_Bitmap(const unsigned char chr,const unsigned char attr,const unsigned int line) {
    return vga.draw.font_tables[(attr >> 3)&1][(chr<<5)+line];
}

template <const unsigned int card> static inline void Alt_EGAVGA_TEXT_GetFGBG(unsigned char &foreground,unsigned char &background,const unsigned char attr,const unsigned char line,const bool in_cursor_row,const unsigned int addr) {
    // if blinking is enabled bit7 is not mapped to attributes
    background = attr >> 4u;
    if (vga.draw.blinking) background &= ~0x8u;

    // choose foreground color if blinking not set for this cell or blink on
    foreground = (vga.draw.blink || (!(attr&0x80))) ? (attr&0xf) : background;

    // underline: all foreground [freevga: 0x77, previous 0x7]
    if (GCC_UNLIKELY(((attr&0x77) == 0x01) && (vga.crtc.underline_location&0x1f)==line))
        background = foreground;

    // text cursor
    if (GCC_UNLIKELY(in_cursor_row) && addr == vga.config.cursor_start)
        background = foreground;
}

template <const unsigned int card> static inline bool Alt_EGAVGA_TEXT_In_Cursor_Row(const unsigned int line) {
    return
        ((vga.draw.cursor.count&0x8) && (line >= vga.draw.cursor.sline) &&
        (line <= vga.draw.cursor.eline) && vga.draw.cursor.enabled);
}

template <const unsigned int card,typename templine_type_t,const unsigned int pixelsperchar> static inline uint8_t* Alt_EGAVGA_TEXT_Combined_Draw_Line(void) {
    // keep it aligned:
    templine_type_t* draw = ((templine_type_t*)TempLine) + 16 - vga.draw.panning;
    Bitu blocks = vga.draw.blocks;
    if (vga.draw.panning) blocks++; // if the text is panned part of an 
                                    // additional character becomes visible

    const unsigned int line = vga.draw_2[0].vert.current_char_pixel & vga.draw_2[0].vert.char_pixel_mask;
    const bool in_cursor_row = Alt_EGAVGA_TEXT_In_Cursor_Row<card>(line);

    unsigned char foreground,background;

    while (blocks--) { // for each character in the line
        const unsigned int addr = vga.draw_2[0].crtc_addr_fetch_and_advance();
        VGA_Latch pixels(*vga.draw_2[0].drawptr<uint32_t>(addr << vga.config.addr_shift));

        const unsigned char chr = pixels.b[0];
        const unsigned char attr = pixels.b[1];

        // the font pattern
        unsigned int font = Alt_EGAVGA_TEXT_Load_Font_Bitmap<card>(chr,attr,line);
        Alt_EGAVGA_TEXT_GetFGBG<card>(foreground,background,attr,line,in_cursor_row,addr);

        // Draw it
        Alt_EGAVGA_TEXT_Combined_Draw_Line_RenderBMP<card,templine_type_t,pixelsperchar>
            (draw,Alt_VGA_Alpha8to9Expand<card,pixelsperchar>(font,chr),foreground,background);
    }

    return TempLine+(16*sizeof(templine_type_t));
}

template <const unsigned int card,typename templine_type_t> static inline uint8_t* Alt_EGAVGA_TEXT_Combined_Draw_Line(void) {
    if (vga.draw.char9dot)
        return Alt_EGAVGA_TEXT_Combined_Draw_Line<card,templine_type_t,9>();
    else
        return Alt_EGAVGA_TEXT_Combined_Draw_Line<card,templine_type_t,8>();
}

// combined 8/9-dot wide text mode 16bpp line drawing function
static uint8_t* Alt_EGA_TEXT_Xlat8_Draw_Line(Bitu /*vidstart*/, Bitu /*line*/) {
    return Alt_EGAVGA_TEXT_Combined_Draw_Line<MCH_EGA,uint8_t>();
}

// combined 8/9-dot wide text mode 16bpp line drawing function
static uint8_t* Alt_VGA_TEXT_Xlat32_Draw_Line(Bitu /*vidstart*/, Bitu /*line*/) {
    return Alt_EGAVGA_TEXT_Combined_Draw_Line<MCH_VGA,uint32_t>();
}

extern bool pc98_attr4_graphic;
extern uint8_t GDC_display_plane;
extern uint8_t GDC_display_plane_pending;
extern bool pc98_graphics_hide_odd_raster_200line;
extern bool pc98_allow_scanline_effect;
extern bool gdc_analog;

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

static void VGA_ProcessSplit() {
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
    if (vga_alt_new_mode)
        vga.draw.split_line = vga.config.line_compare + 1;
    else
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

void VGA_Alt_CheckSplit(void) {
    if (vga.draw_2[0].raster_scanline == vga.draw.split_line) {
        /* VGA line compare. split line */
        vga.draw.has_split = true;
        if (vga.attr.mode_control&0x20) {
            vga.draw_2[0].vert.crtc_addr = 0;
            vga.draw.panning = 0;
        }
        else {
            vga.draw_2[0].vert.crtc_addr = 0 + vga.draw.bytes_skip;
        }
        vga.draw_2[0].vert.current_char_pixel = 0;
    }
}

void VGA_Alt_UpdateCRTCPixels(void) {
    if (IS_EGAVGA_ARCH) {
        vga.draw_2[0].horz.char_pixels = (vga.attr.mode_control & 4/*9 pixels/char*/) ? 9 : 8;
        vga.draw_2[0].vert.char_pixels = (vga.crtc.maximum_scan_line & vga.draw_2[0].vert.char_pixel_mask) + 1u;
    }
    else {
        vga.draw_2[0].horz.char_pixels = 8;
        vga.draw_2[0].vert.char_pixels = (vga.other.max_scanline & vga.draw_2[0].vert.char_pixel_mask) + 1u;
    }
}

void VGA_Alt_UpdateCRTCAdd(void) {
    if (IS_EGAVGA_ARCH) {
        vga.draw_2[0].horz.crtc_addr_add = 1;
        vga.draw_2[0].vert.crtc_addr_add = vga.crtc.offset * 2u;
    }
    else {
        vga.draw_2[0].horz.crtc_addr_add = 1;
        vga.draw_2[0].vert.crtc_addr_add = vga.other.hdend;
    }
}

void VGA_Alt_NextLogScanLine(void) {
    vga.draw_2[0].horz.current = 0;
    vga.draw_2[0].vert.current.pixels++;

    VGA_Alt_UpdateCRTCPixels();
    VGA_Alt_UpdateCRTCAdd();

    vga.draw_2[0].vert.current_char_pixel++;

    // TODO: DOSBox SVN and DOSBox-X main VGA emulation go to next line if row char line >= max.
    //       Real hardware suggests that it only happens when line == max, meaning if you reprogram
    //       the max scanline register in such a way the card misses it, it will count through all
    //       5 bits of the row counter before coming back around to match it again.
    //
    //       It might be a nice emulation option to select comparator function, whether >= or == .
    if ((vga.draw_2[0].vert.current_char_pixel & vga.draw_2[0].vert.char_pixel_mask) ==
        (vga.draw_2[0].vert.char_pixels        & vga.draw_2[0].vert.char_pixel_mask)) {
        vga.draw_2[0].vert.current_char_pixel = 0;
        vga.draw_2[0].vert.crtc_addr += vga.draw_2[0].vert.crtc_addr_add;
    }

    if (IS_EGAVGA_ARCH)
        VGA_Alt_CheckSplit();

    vga.draw_2[0].horz.crtc_addr = vga.draw_2[0].vert.crtc_addr;
    vga.draw_2[0].horz.current_char_pixel = 0;

    VGA_Draw2_Recompute_CRTC_MaskAdd();
}

void VGA_Alt_NextScanLine(void) {
    /* track actual raster line to output */
    vga.draw_2[0].raster_scanline++;

    /* do not advance the vertical count nor carry out new scanline functions
     * if doublescan is set and this is the EVEN scan line */
    if (vga.draw_2[0].doublescan_count >= vga.draw_2[0].doublescan_max) {
        vga.draw_2[0].doublescan_count = 0;
        VGA_Alt_NextLogScanLine();
    }
    else {
        vga.draw_2[0].doublescan_count++;

        if (IS_EGAVGA_ARCH)
            VGA_Alt_CheckSplit();

        vga.draw_2[0].horz.crtc_addr = vga.draw_2[0].vert.crtc_addr;
        vga.draw_2[0].horz.current_char_pixel = 0;

        VGA_Draw2_Recompute_CRTC_MaskAdd();
    }
}

static void VGA_DrawSingleLine(Bitu /*blah*/) {
    unsigned int lines = 0;
    bool skiprender;

again:
    if (vga.draw.render_step == 0)
        skiprender = false;
    else
        skiprender = true;

    if ((++vga.draw.render_step) >= vga.draw.render_max)
        vga.draw.render_step = 0;

    if (!skiprender) {
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

    /* parallel system */
    if (vga_alt_new_mode)
        VGA_Alt_NextScanLine();

    vga.draw.address_line++;
    if (vga.draw.address_line>=vga.draw.address_line_total) {
        vga.draw.address_line=0;
        vga.draw.address+=vga.draw.address_add;
    }

    if (!skiprender) {
        vga.draw.lines_done++;
        if (vga.draw.split_line==vga.draw.lines_done && !vga_alt_new_mode) VGA_ProcessSplit();
    }

    if (mcga_double_scan) {
        if (vga.draw.lines_done < vga.draw.lines_total) {
            if (++lines < 2)
                goto again;
        }
    }

    if (vga.draw.lines_done < vga.draw.lines_total) {
        PIC_AddEvent(VGA_DrawSingleLine,(float)vga.draw.delay.singleline_delay);
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

            if (VGA_IsCaptureEnabled())
                VGA_ProcessScanline(data);

            RENDER_DrawLine(data);
        }
    }

    /* parallel system */
    if (vga_alt_new_mode)
        VGA_Alt_NextScanLine();

    vga.draw.address_line++;
    if (vga.draw.address_line>=vga.draw.address_line_total) {
        vga.draw.address_line=0;
        vga.draw.address+=vga.draw.address_add;
    }

    if (!skiprender) {
        vga.draw.lines_done++;
        if (vga.draw.split_line==vga.draw.lines_done && !vga_alt_new_mode) VGA_ProcessSplit();
    }

    if (vga.draw.lines_done < vga.draw.lines_total) {
        PIC_AddEvent(VGA_DrawEGASingleLine,(float)vga.draw.delay.singleline_delay);
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

static void VGA_VertInterrupt(Bitu /*val*/) {
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
    if (val) PIC_ActivateIRQ(5);
    else PIC_DeActivateIRQ(5);
}

static void VGA_DisplayStartLatch(Bitu /*val*/) {
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

extern uint32_t GFX_Rmask;
extern unsigned char GFX_Rshift;
extern uint32_t GFX_Gmask;
extern unsigned char GFX_Gshift;
extern uint32_t GFX_Bmask;
extern unsigned char GFX_Bshift;
extern uint32_t GFX_Amask;
extern unsigned char GFX_Ashift;
extern unsigned char GFX_bpp;
extern uint32_t vga_capture_stride;

template <const unsigned int bpp,typename BPPT> uint32_t VGA_CaptureConvertPixel(const BPPT raw) {
    unsigned char r,g,b;

    /* FIXME: Someday this code will have to deal with 10:10:10 32-bit RGB.
     * Also the 32bpp case shows how hacky this codebase is with regard to 32bpp color order support */
    if (bpp == 32) {
        if (GFX_bpp >= 24) {
            r = ((uint32_t)raw & (uint32_t)GFX_Rmask) >> (uint32_t)GFX_Rshift;
            g = ((uint32_t)raw & (uint32_t)GFX_Gmask) >> (uint32_t)GFX_Gshift;
            b = ((uint32_t)raw & (uint32_t)GFX_Bmask) >> (uint32_t)GFX_Bshift;
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
            case 32:    VGA_CaptureWriteScanlineChecked<32>((uint32_t*)raw); break;
            case 16:    VGA_CaptureWriteScanlineChecked<16>((uint16_t*)raw); break;
            case 15:    VGA_CaptureWriteScanlineChecked<16>((uint16_t*)raw); break;
            case 8:     VGA_CaptureWriteScanlineChecked< 8>((uint8_t *)raw); break;
        }
    }
    else {
        VGA_CaptureMarkError();
    }
}

bool CodePageGuestToHostUint16(uint16_t *d/*CROSS_LEN*/,const char *s/*CROSS_LEN*/);

static void VGA_VerticalTimer(Bitu /*val*/) {
    double current_time = PIC_GetCurrentEventTime();

    if (IS_PC98_ARCH) {
        GDC_display_plane = GDC_display_plane_pending;
        pc98_update_display_page_ptr();
    }

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

    vga.draw.delay.framestart = current_time; /* FIXME: Anyone use this?? If not, remove it */
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
            jolt_tick = (Bitu)current_tick;

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
                float diff_thing = ticks_since_jolt - (num_host_syncs_in_those_ticks * (double)vsync.period);

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
#if 0
        fv = vdisplayendtimerval + vsync_adj;
        if (fv < 1) fv = 1;
        PIC_AddEvent(VGA_DisplayStartLatch,fv);
#endif
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
    vga.draw.cursor.count++;

    if (IS_PC98_ARCH) {
        for (unsigned int i=0;i < 2;i++)
            pc98_gdc[i].cursor_advance();
    }

    //Check if we can actually render, else skip the rest
    if (vga.draw.vga_override || !RENDER_StartUpdate()) return;

#if defined(USE_TTF)
    if (ttf.inUse) {
#if defined(WIN32)
        typedef wchar_t host_cnv_char_t;
#else
        typedef char host_cnv_char_t;
#endif
        host_cnv_char_t *CodePageGuestToHost(const char *s);
		GFX_StartUpdate(render.scale.outWrite, render.scale.outPitch);
		vga.draw.blink = ((vga.draw.blinking & time(NULL)) || !vga.draw.blinking) ? true : false;	// eventually blink once per second
		vga.draw.cursor.address = vga.config.cursor_start*2;
		Bitu vidstart = vga.config.real_start + vga.draw.bytes_skip;
		vidstart *= 2;

		uint16_t* draw = (uint16_t*)newAttrChar;

        if (IS_PC98_ARCH) {
            const uint16_t* charram = (uint16_t*)&vga.draw.linear_base[0x0000];         // character data
            const uint16_t* attrram = (uint16_t*)&vga.draw.linear_base[0x2000];         // attribute data
            uint16_t uname[4];

            for (Bitu blocks = ttf.cols * ttf.lins; blocks; blocks--) {
                if ((*charram & 0xFF00u) && (*charram & 0xFCu) != 0x08u && (*charram&0x7F7F) == (*(charram+1)&0x7F7F)) {
					*draw=*charram&0x7F7F;
                    uint8_t j1=(*draw%0x100)+0x20, j2=*draw/0x100;
					if (j1>32&&j1<127&&j2>32&&j2<127) {
                        char text[3];
                        text[0]=(j1+1)/2+(j1<95?112:176);
                        text[1]=j2+(j1%2?31+(j2/96):126);
                        text[2]=0;
                        uname[0]=0;
                        uname[1]=0;
                        CodePageGuestToHostUint16(uname,text);
                        assert(uname[1]==0);
                        if (uname[0]!=0)
                            *draw++=uname[0];
                        else
                            *draw++=' ';
                    } else
                        *draw++ = *draw & 0xFF;
                } else if (*charram & 0xFF80u)
                    *draw++ = 0x20; // not properly handled YET
                else
                    *draw++ = *charram & 0xFF;

                Bitu attr = *attrram;
                charram++;
                attrram++;
                // for simplicity at this time, just map PC-98 attributes to VGA colors. Wengier and I can clean this up later --J.C.
                Bitu background = 0;
                Bitu foreground = (attr>>5)&7;
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
                    *draw = 0x20;
                }
                *draw++ = (background<<4) + foreground;
            }
        } else if (CurMode&&CurMode->type==M_TEXT) {
            vga.draw.address_add = ttf.cols * 2;
            if (IS_EGAVGA_ARCH) {
                for (Bitu row=0;row < ttf.lins;row++) {
                    const uint32_t* vidmem = ((uint32_t*)vga.draw.linear_base)+vidstart;	// pointer to chars+attribs (EGA/VGA planar memory)
                    for (Bitu col=0;col < ttf.cols;col++) {
                        // NTS: Note this assumes EGA/VGA text mode that uses the "Odd/Even" mode memory mapping scheme to present video memory
                        //      to the CPU as if CGA compatible text mode. Character data on plane 0, attributes on plane 1.
                        *draw++ = *vidmem;
                        Bitu attr = *((uint8_t*)vidmem+1);
                        vidmem+=2;
                        Bitu background = attr >> 4;
                        if (vga.draw.blinking)									// if blinking is enabled bit7 is not mapped to attributes
                            background &= 7;
                        // choose foreground color if blinking not set for this cell or blink on
                        Bitu foreground = (vga.draw.blink || (!(attr&0x80))) ? (attr&0xf) : background;
                        // How about underline?
                        *draw++ = (background<<4) + foreground;
                    }
                    vidstart += vga.draw.address_add;
                }
            } else {
                for (Bitu row=0;row < ttf.lins;row++) {
                    const uint16_t* vidmem = (uint16_t*)VGA_Text_Memwrap(vidstart);	// pointer to chars+attribs (EGA/VGA planar memory)
                    for (Bitu col=0;col < ttf.cols;col++) {
                        *draw++ = *vidmem;
                        Bitu attr = *((uint8_t*)vidmem+1);
                        vidmem++;
                        Bitu background = attr >> 4;
                        if (vga.draw.blinking)									// if blinking is enabled bit7 is not mapped to attributes
                            background &= 7;
                        // choose foreground color if blinking not set for this cell or blink on
                        Bitu foreground = (vga.draw.blink || (!(attr&0x80))) ? (attr&0xf) : background;
                        // How about underline?
                        *draw++ = (background<<4) + foreground;
                    }
                    vidstart += vga.draw.address_add;
                }
            }
        }

		render.cache.past_y = 1;
		RENDER_EndUpdate(false);
		return;
	}
#endif

    vga.draw.address_line = vga.config.hlines_skip;
    if (IS_EGAVGA_ARCH) VGA_Update_SplitLineCompare();
    vga.draw.address = vga.config.real_start;
    vga.draw.byte_panning_shift = 0;

    /* parallel system */
    if (vga_alt_new_mode) {
        /* the doublescan bit can be changed between frames, it can happen!
         *
         * "Show" by Majic 12: Two parts use 320x200 16-color planar mode, which by default
         *                     is programmed by INT 10h to use the doublescan bit and max scanline == 0.
         *                     These two parts then reprogram that register to turn off doublescan
         *                     and set max scanline == 1. This compensation is needed for those two
         *                     parts to show correctly. */
        if (IS_VGA_ARCH && (vga.crtc.maximum_scan_line & 0x80))
            vga.draw_2[0].doublescan_max = 1;
        else
            vga.draw_2[0].doublescan_max = 0;

        vga.draw_2[0].raster_scanline = 0;
        vga.draw_2[0].doublescan_count = 0;

        if (IS_EGAVGA_ARCH) {
            vga.draw_2[0].horz.current = 0;
            vga.draw_2[0].vert.current = 0;

            vga.draw_2[0].horz.current_char_pixel = 0;
            vga.draw_2[0].vert.current_char_pixel = vga.config.hlines_skip;

            VGA_Alt_UpdateCRTCPixels();
            VGA_Alt_UpdateCRTCAdd();

            vga.draw_2[0].vert.crtc_addr = vga.config.real_start + vga.draw.bytes_skip;
            vga.draw_2[0].horz.crtc_addr = vga.draw_2[0].vert.crtc_addr;

            VGA_Draw2_Recompute_CRTC_MaskAdd();
            VGA_Alt_CheckSplit();
        }
        else {
            vga.draw_2[0].horz.current = 0;
            vga.draw_2[0].vert.current = 0;

            vga.draw_2[0].horz.current_char_pixel = 0;
            vga.draw_2[0].vert.current_char_pixel = 0;

            VGA_Alt_UpdateCRTCPixels();
            VGA_Alt_UpdateCRTCAdd();

            vga.draw_2[0].vert.crtc_addr = vga.config.real_start;
            vga.draw_2[0].horz.crtc_addr = vga.draw_2[0].vert.crtc_addr;

            VGA_Draw2_Recompute_CRTC_MaskAdd();
        }
    }

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
        if (IS_EGAVGA_ARCH)
            vga.draw.cursor.address=vga.config.cursor_start<<vga.config.addr_shift;
        else
            vga.draw.cursor.address=vga.config.cursor_start*2;
        vga.draw.address *= 2;

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
    if (GCC_UNLIKELY((Bits)vga.draw.split_line <= 0) && !vga_alt_new_mode) {
        VGA_ProcessSplit();

        /* if vblank_skip != 0, line compare can become a negative value! Fixes "Warlock" 1992 demo by Warlock */
        if (GCC_UNLIKELY((Bits)vga.draw.split_line < 0)) {
            vga.draw.address_line += (Bitu)(-((Bits)vga.draw.split_line) % (Bits)vga.draw.address_line_total);
            vga.draw.address += vga.draw.address_add * (Bitu)(-((Bits)vga.draw.split_line) / (Bits)vga.draw.address_line_total);
        }
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
        if (vga.draw.mode==EGALINE)
            PIC_AddEvent(VGA_DrawEGASingleLine,(float)(vga.draw.delay.htotal/4.0 + draw_skip));
        else PIC_AddEvent(VGA_DrawSingleLine,(float)(vga.draw.delay.htotal/4.0 + draw_skip));
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
                vga.draw.address_add=vga.config.scan_len*(unsigned int)(2u<<2u);
            }
            else {
                // Other cards (?)
                vga.draw.address_add=vga.config.scan_len*(unsigned int)(2u<<vga.config.addr_shift);
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
    case M_AMSTRAD: // Next line.
        if (IS_EGAVGA_ARCH)
            vga.draw.address_add=vga.config.scan_len*(unsigned int)(2u<<vga.config.addr_shift);
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
    if (hwcursor_active && vga.mode != M_LIN24) {
        switch(vga.mode) {
        case M_LIN32:
            VGA_DrawLine=VGA_Draw_LIN32_Line_HWMouse;
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
#if SDL_BYTEORDER == SDL_LIL_ENDIAN && defined(MACOSX) /* Mac OS X Intel builds use a weird RGBA order (alpha in the low 8 bits) */
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
     *      This will slowly change to accomodate PC-98 display controller over time
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
    if (vdend > vtotal) {
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

    if (vga_alt_new_mode) {
        vga.draw_2[0].doublescan_count = 0;
        vga.draw_2[0].doublescan_max = 0;
    }

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
        case M_PC98:
        case M_TEXT:
            if (!vga_alt_new_mode) {
                // these use line_total internal
                // doublescanning needs to be emulated by renderer doubleheight
                // EGA has no doublescanning bit at 0x80
                if (vga.crtc.maximum_scan_line&0x80) {
                    // vga_draw only needs to draw every second line
                    height /= 2;
                }
                break;
            }
            /* fall through if vga_alt_new_mode */
        default:
            vga.draw.doublescan_effect = vga.draw.doublescan_set;

            if (vga_alt_new_mode) {
                if (IS_VGA_ARCH && (vga.crtc.maximum_scan_line & 0x80))
                    vga.draw_2[0].doublescan_max = 1;
                else
                    vga.draw_2[0].doublescan_max = 0;

                if (!vga.draw.doublescan_effect) {
                    if (IS_VGA_ARCH && (vga.crtc.maximum_scan_line & 0x80)) /* CGA/EGA modes on VGA */
                        height /= 2;
                    else if ((vga.crtc.maximum_scan_line & 1) == 1) /* multiple of 2, 256-color mode on VGA, for example */
                        height /= 2;
                    else
                        vga.draw.doublescan_effect = true;
                }
            }
            else {
                if (vga.crtc.maximum_scan_line & 0x80)
                    vga.draw.address_line_total *= 2;

                /* if doublescan=false and line_total is even, then halve the height.
                 * the VGA raster scan will skip every other line to accomodate that. */
                if ((!vga.draw.doublescan_effect) && (vga.draw.address_line_total & 1) == 0)
                    height /= 2;
                else
                    vga.draw.doublescan_effect = true;
            }

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

    /* parallel system */
    if (vga_alt_new_mode)
        VGA_Draw2_Recompute_CRTC_MaskAdd();

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
            if (vga_alt_new_mode) {
                vga.draw.blocks = width;
                VGA_DrawLine = Alt_VGA_256color_Draw_Line_Tseng_ET4000;
            }
            else {
                VGA_DrawLine = VGA_Draw_Xlat32_Linear_Line;
            }
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
            if (vga_alt_new_mode) {
                vga.draw.blocks = width;

                /* NTS: 8BIT (bit 6) is normally set for 256-color mode. What it does when enabled
                 *      is latch every other pixel clock an 8-bit value to the DAC. It is needed
                 *      because VGA hardware appears to generate a 4-bit (16-color) value per pixel
                 *      clock internally. For 256-color mode, it shifts 4 bits through an 8-bit
                 *      register per pixel clock. You're supposed to set 8BIT so that it latches
                 *      the 8-bit value only when it's completed two 4-bit values to get a proper
                 *      256-color mode. If you turn off 8BIT, then the 8-bit values and the
                 *      intermediate shifted values are emitted to the screen as a sort of weird
                 *      640x200 256-color mode. */
                if (vga.attr.mode_control & 0x40) { /* 8BIT=1 (normal) 256-color mode */
                    VGA_DrawLine = Alt_VGA_256color_Draw_Line;
                }
                else {
                    VGA_DrawLine = Alt_VGA_256color_2x4bit_Draw_Line;
                    pix_per_char = 8;
                }
            }
            else {
                VGA_DrawLine = VGA_Draw_Xlat32_VGA_CRTC_bmode_Line;
            }
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
            if (vga_alt_new_mode)
                VGA_DrawLine = Alt_EGA_Planar_Draw_Line;
            else
                VGA_DrawLine = EGA_Draw_VGA_Planar_Xlat8_Line;

            bpp = 8;
        }
        else {
            if (vga_alt_new_mode)
                VGA_DrawLine = Alt_VGA_Planar_Draw_Line;
            else
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
            if (vga_alt_new_mode)
                VGA_DrawLine=Alt_EGA_2BPP_Draw_Line;
            else
                VGA_DrawLine=EGA_Draw_2BPP_Line_as_EGA;

            bpp = 8;
        }
        else if (IS_EGAVGA_ARCH || IS_PC98_ARCH) {
            vga.draw.blocks=width;
            if (vga_alt_new_mode)
                VGA_DrawLine=Alt_VGA_2BPP_Draw_Line;
            else
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
            if (vga_alt_new_mode) {
                VGA_DrawLine=Alt_CGA_4color_Draw_Line;
                vga.draw.blocks=width;
            }
            else {
                VGA_DrawLine=VGA_Draw_2BPP_Line;
                vga.draw.blocks=width*2;
            }
        }
        break;
    case M_CGA2:
        // CGA 2-color mode on EGA/VGA is just EGA 16-color planar mode with one bitplane enabled and a
        // color palette to match. Therefore CGA 640x200 2-color mode can be rendered correctly using
        // the 16-color planar renderer. The MEM13 bit is configured to replace address bit 13 with
        // character row counter bit 0 to match CGA memory layout, doublescan is set (as if 320x200),
        // max_scanline is set to 1 (2 lines).
        if (IS_EGA_ARCH) {
            vga.draw.blocks=width;
            if (vga_alt_new_mode)
                VGA_DrawLine=Alt_EGA_Planar_Draw_Line;
            else
                VGA_DrawLine=EGA_Draw_1BPP_Line_as_EGA;

            bpp = 8;
        }
        else if (IS_EGAVGA_ARCH) {
            vga.draw.blocks=width;
            if (vga_alt_new_mode)
                VGA_DrawLine=Alt_VGA_Planar_Draw_Line;
            else
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
            if (vga_alt_new_mode) {
                VGA_DrawLine=Alt_CGA_2color_Draw_Line;
                vga.draw.blocks=width;
            }
            else {
                VGA_DrawLine=VGA_Draw_1BPP_Line;
                vga.draw.blocks=width*2;
            }
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
        if ((vga.seq.clocking_mode&0x01) || !vga.draw.char9_set) {
            // 8-pixel wide
            pix_per_char = 8;
            vga.draw.char9dot = false;
        } else {
            // 9-pixel wide
            pix_per_char = 9;
            vga.draw.char9dot = true;
        }

        if (IS_EGA_ARCH) {
            if (vga_alt_new_mode)
                VGA_DrawLine = Alt_EGA_TEXT_Xlat8_Draw_Line;
            else
                VGA_DrawLine = EGA_TEXT_Xlat8_Draw_Line;
            bpp = 8;
        }
        else {
            if (vga_alt_new_mode)
                VGA_DrawLine = Alt_VGA_TEXT_Xlat32_Draw_Line;
            else
                VGA_DrawLine = VGA_TEXT_Xlat32_Draw_Line;
            bpp = 32;
        }
        break;
    case M_HERC_GFX:
        if (vga_alt_new_mode) {
            vga.draw.blocks = width;
            VGA_DrawLine = Alt_CGA_2color_Draw_Line;
        }
        else {
            vga.draw.blocks=width*2;
            if (vga.herc.blend) VGA_DrawLine=VGA_Draw_1BPP_Blend_Line;
            else VGA_DrawLine=VGA_Draw_1BPP_Line;
        }
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

        if (vga_alt_new_mode) {
            vga.draw.blocks = width;
            VGA_DrawLine=Alt_CGA_2color_Draw_Line;
        }
        else {
            VGA_DrawLine=VGA_Draw_1BPP_Line;
        }

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
            if (vga_alt_new_mode) {
                VGA_DrawLine=Alt_CGA_4color_Draw_Line;
                vga.draw.blocks=width;
            }
            else {
                VGA_DrawLine=VGA_Draw_2BPP_Line;
            }
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
        if (vga_alt_new_mode) {
            if (machine==MCH_CGA /*&& !doublewidth*/ && enableCGASnow && (vga.tandy.mode_control & 1)/*80-column mode*/)
                VGA_DrawLine=Alt_CGA_CGASNOW_TEXT_Draw_Line; /* Alternate version that emulates CGA snow */
            else
                VGA_DrawLine=Alt_CGA_TEXT_Draw_Line;
        }
        else {
            if (machine==MCH_CGA /*&& !doublewidth*/ && enableCGASnow && (vga.tandy.mode_control & 1)/*80-column mode*/)
                VGA_DrawLine=VGA_CGASNOW_TEXT_Draw_Line; /* Alternate version that emulates CGA snow */
            else
                VGA_DrawLine=VGA_TEXT_Draw_Line;
        }

        /* MCGA CGA-compatible modes will always refer to the last half of the 64KB of RAM */
        if (machine == MCH_MCGA) {
            vga.tandy.draw_base = vga.mem.linear + 0x8000;
            VGA_DrawLine = MCGA_TEXT_Draw_Line;
            bpp = 32;
        }

        break;
    case M_HERC_TEXT:
        vga.draw.blocks=width;
        if (vga_alt_new_mode)
            VGA_DrawLine=Alt_MDA_TEXT_Draw_Line;
        else
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
    if (vratio == 1.6) screenratio = 4.0 / 3.0;
    else if (vratio == 0.8) screenratio = 4.0 / 3.0;
    else if (vratio == 3.2) screenratio = 4.0 / 3.0;
    else if (vratio == (4.0/3.0)) screenratio = 4.0 / 3.0;
    else if (vratio == (2.0/3.0)) screenratio = 4.0 / 3.0;
    else if ((width >= 800)&&(height>=600)) screenratio = 4.0 / 3.0;

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
