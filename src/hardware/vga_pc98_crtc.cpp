/*
 *  Copyright (C) 2018-2020  Jon Campbell
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
#include "menu.h"
#include "util_units.h"
#include "control.h"
#include "pc98_cg.h"
#include "pc98_dac.h"
#include "pc98_gdc.h"
#include "pc98_gdc_const.h"
#include "mixer.h"

void pc98_update_page_ptrs(void);

extern bool                 pc98_40col_text;
extern bool                 pc98_31khz_mode;
extern bool                 pc98_attr4_graphic;
extern bool                 pc98_display_enable;
extern bool                 pc98_graphics_hide_odd_raster_200line;
extern bool                 pc98_crt_mode;      // see port 6Ah command 40h/41h.

// TODO: Other parts that change gdc_5mhz_mode should update these too!
bool                        gdc_clock_1 = false;
bool                        gdc_clock_2 = false;

bool                        gdc_5mhz_mode = false;
bool                        enable_pc98_egc = true;
bool                        enable_pc98_grcg = true;
bool                        enable_pc98_16color = true;
bool                        enable_pc98_256color = true;
bool                        enable_pc98_256color_planar = true;
bool                        enable_pc98_188usermod = true;
bool                        pc98_256kb_boundary = false;         /* port 6Ah command 68h/69h */
bool                        GDC_vsync_interrupt = false;
bool                        gdc_5mhz_mode_initial = false;
uint8_t                     GDC_display_plane_wait_for_vsync = false;
uint8_t                     GDC_display_plane_pending = false;
uint8_t                     GDC_display_plane = false;

uint8_t                     pc98_gdc_tile_counter=0;
uint8_t                     pc98_gdc_modereg=0;
uint8_t                     pc98_gdc_vramop=0;
egc_quad                    pc98_gdc_tiles;

extern unsigned char        pc98_text_first_row_scanline_start;  /* port 70h */
extern unsigned char        pc98_text_first_row_scanline_end;    /* port 72h */
extern unsigned char        pc98_text_row_scanline_blank_at;     /* port 74h */
extern unsigned char        pc98_text_row_scroll_lines;          /* port 76h */
extern unsigned char        pc98_text_row_scroll_count_start;    /* port 78h */
extern unsigned char        pc98_text_row_scroll_num_lines;      /* port 7Ah */

// the guest can change the GDC to 5MHz by setting both GDC clock bits.
// NTS: For whatever reason, Windows 3.1 will set the GDC to 5MHz when entering a DOS application fullscreen.
void gdc_clock_check(void) {
    bool nresult = gdc_clock_1 && gdc_clock_2;

    if (gdc_5mhz_mode != nresult) {
        gdc_5mhz_mode = nresult;
        LOG_MSG("PC-98: Guest changed GDC clock to %s",gdc_5mhz_mode?"5MHz":"2.5MHz");
        mainMenu.get_item("pc98_5mhz_gdc").check(gdc_5mhz_mode).refresh_item(mainMenu);
    }
}

void pc98_crtc_write(Bitu port,Bitu val,Bitu iolen) {
    (void)iolen;//UNUSED
    switch (port&0xE) {
        case 0x00:      // 0x70: Character row, CG start scanline
            pc98_text_first_row_scanline_start = (unsigned char)val & 0x1F;
            break;
        case 0x02:      // 0x72: Character row, CG end scanline
            pc98_text_first_row_scanline_end = (unsigned char)val & 0x1F;
            break;
        case 0x04:      // 0x74: Character row, number of CG scanlines to display
            pc98_text_row_scanline_blank_at = (unsigned char)val & 0x1F;
            break;
        case 0x06:
            pc98_text_row_scroll_lines = (unsigned char)val & 0x1F;
            break;
        case 0x08:
            pc98_text_row_scroll_count_start = (unsigned char)val & 0x1F;
            break;
        case 0x0A:
            pc98_text_row_scroll_num_lines = (unsigned char)val & 0x1F;
            break;
        case 0x0C:      // 0x7C: mode reg / vram operation mode (also, reset tile counter)
            if (enable_pc98_grcg) {
                pc98_gdc_tile_counter = 0;
                pc98_gdc_modereg = (uint8_t)val;
                /* bit 7: 1=GRGC active  0=GRGC invalid
                 * bit 6: 1=Read/Modify/Write when writing  0=TCR mode at read, TDW mode at write */
                pc98_gdc_vramop &= ~(3 << VOPBIT_GRCG);
                pc98_gdc_vramop |= (val & 0xC0) >> (6 - VOPBIT_GRCG);
            }
            break;
        case 0x0E:      // 0x7E: tile data
            if (enable_pc98_grcg) {
                pc98_gdc_tiles[pc98_gdc_tile_counter].b[0] = (uint8_t)val;
                pc98_gdc_tiles[pc98_gdc_tile_counter].b[1] = (uint8_t)val;
                pc98_gdc_tile_counter = (pc98_gdc_tile_counter + 1) & 3;
            }
            break;
        default:
            LOG_MSG("PC98 CRTC w: port=0x%02X val=0x%02X unknown",(unsigned int)port,(unsigned int)val);
            break;
    }
}

Bitu pc98_crtc_read(Bitu port,Bitu iolen) {
    (void)iolen;//UNUSED
    switch (port&0xE) {
        case 0x00:      // 0x70: Character row, CG start scanline
            return pc98_text_first_row_scanline_start;
        case 0x02:      // 0x72: Character row, CG end scanline
            return pc98_text_first_row_scanline_end;
        case 0x04:      // 0x74: Character row, number of CG scanlines to display
            return pc98_text_row_scanline_blank_at;
        case 0x06:
            return pc98_text_row_scroll_lines;
        case 0x08:
            return pc98_text_row_scroll_count_start;
        case 0x0A:
            return pc98_text_row_scroll_num_lines;
        default:
            LOG_MSG("PC98 CRTC r: port=0x%02X unknown",(unsigned int)port);
            break;
    }

    return ~0ul;
}

bool egc_enable_enable = false;

void update_gdc_analog(void) {
    /* gdc analog mode if analog (16-color) or 256-color mode */
    gdc_analog = (pc98_gdc_vramop & ((1 << VOPBIT_ANALOG)|(1 << VOPBIT_VGA))) != 0;
}

/* Port 0x6A command handling */
void pc98_port6A_command_write(unsigned char b) {
    switch (b) {
        case 0x00: // 16-color (analog) disable
            pc98_gdc_vramop &= ~(1 << VOPBIT_ANALOG);
            update_gdc_analog();
            VGA_SetupHandlers();   // confirmed on real hardware: this disables access to E000:0000
            pc98_update_palette(); // Testing on real hardware shows that the "digital" and "analog" palettes are completely different.
                                   // They're both there in hardware, but one or another is active depending on analog enable.
                                   // Also, the 4th bitplane at E000:0000 disappears when switched off from the display and from CPU access.
            break;
        case 0x01: // or enable
            if (enable_pc98_16color) {
                pc98_gdc_vramop |= (1 << VOPBIT_ANALOG);
                update_gdc_analog();
                VGA_SetupHandlers();   // confirmed on real hardware: this enables access to E000:0000
                pc98_update_palette(); // Testing on real hardware shows that the "digital" and "analog" palettes are completely different.
                                       // They're both there in hardware, but one or another is active depending on analog enable.
                                       // Also, the 4th bitplane at E000:0000 disappears when switched off from the display and from CPU access.
            }
            break;
        case 0x04:
            if (egc_enable_enable)
                pc98_gdc_vramop &= ~(1 << VOPBIT_EGC);
            break;
        case 0x05:
            if (enable_pc98_egc && egc_enable_enable)
                pc98_gdc_vramop |= (1 << VOPBIT_EGC);
            break;
        case 0x06:
            egc_enable_enable = false;
            break;
        case 0x07:
            egc_enable_enable = true;
            break;
        case 0x0A: // TODO
        case 0x0B: // TODO
            // TODO
            break;
        case 0x20: // 256-color mode disable
            if (enable_pc98_egc && egc_enable_enable) {
                pc98_gdc_vramop &= ~(1 << VOPBIT_VGA);
                update_gdc_analog();
                VGA_SetupHandlers(); // memory mapping presented to the CPU changes
                pc98_update_palette();
                pc98_update_page_ptrs();
            }
            break;
        case 0x21: // 256-color mode enable
            if (enable_pc98_egc && egc_enable_enable && enable_pc98_256color) {
                pc98_gdc_vramop |= (1 << VOPBIT_VGA);
                update_gdc_analog();
                VGA_SetupHandlers(); // memory mapping presented to the CPU changes
                pc98_update_palette();
                pc98_update_page_ptrs();
            }
            break;
        case 0x40: // CRT mode
        case 0x41: // Plasma/LCD mode
            pc98_crt_mode = (b&1)==0;
            break;
        case 0x68: // 128KB VRAM boundary
            // TODO: Any conditions?
            pc98_256kb_boundary = false;
            VGA_SetupHandlers(); // memory mapping presented to the CPU changes
            break;
        case 0x69: // 256KB VRAM boundary
            // TODO: Any conditions?
            pc98_256kb_boundary = true;
            VGA_SetupHandlers(); // memory mapping presented to the CPU changes
            break;
        case 0x82: // GDC Clock #1   0=2.5MHz   1=5MHz
        case 0x83:
            gdc_clock_1 = !!(b&1);
            gdc_clock_check();
            break;
        case 0x84: // GDC Clock #2   0=2.5MHz   1=5MHz
        case 0x85:
            gdc_clock_2 = !!(b&1);
            gdc_clock_check();
            break;
        // TODO: 0x8E/0x8F VRAM use selection  0=PC-98 graphics  1=Cirrus Logic CL-GD graphics   (VRAM is shared?)
        default:
            LOG_MSG("PC-98 port 6Ah unknown command 0x%02x",b);
            break;
    }
}

/* Port 0x68 command handling */
void pc98_port68_command_write(unsigned char b) {
    switch (b) {
        case 0x00: // text screeen attribute bit 4 meaning: 0=vertical line
        case 0x01: //                                       1=simple graphic
            pc98_attr4_graphic = !!(b&1);
            break;
        case 0x04: // 40-column mode  0=80-column
        case 0x05: //                 1=40-column
            pc98_40col_text = !!(b&1);
            break;
        case 0x08: // 200-line mode: show odd raster
        case 0x09: //                don't show odd raster
            pc98_graphics_hide_odd_raster_200line = !!(b&1);
            break;
        case 0x0A: // TODO
        case 0x0B: // TODO
            // TODO
            break;
        case 0x0E: // Display enable
        case 0x0F:
            pc98_display_enable = !!(b&1);
            break;
        default:
            LOG_MSG("PC-98 port 68h unknown command 0x%02x",b);
            break;
    }
}

unsigned char sel_9a0 = 0;

/* Port 0x9A0 readback.
 * This is needed to report the GDC setting so that Windows 3.1 doesn't change the
 * GDC to 5MHz arbitrarily and leave PC-98 games confused afterward */
Bitu pc98_read_9a0(Bitu /*port*/,Bitu /*iolen*/) {
    Bitu retval = 0;

    /* bit 0 depends on what was selected by writing to port 9a0.
     * according to undocumented 9821 (not verified), unknown registers and 0x00 will return 0xff here. */
    switch (sel_9a0) {
        case 0x01:      // color/monochrome
            if (true) retval |= 1u;//FIXME
            break;
        case 0x02:      // odd raster mask
            if (pc98_graphics_hide_odd_raster_200line) retval |= 1u;
            break;
        case 0x03:      // display on/off
            if (pc98_display_enable) retval |= 1u;
            break;
        case 0x04:      // palette mode (used by Sim City 2000)
            if (pc98_gdc_vramop & (1 << VOPBIT_ANALOG)) retval |= 1u;
            break;
        default:
            retval |= 0xFF;//FIXME: Is this true?
            break;
    }

    /* bit 1: graphic GDC clock frequency as set in hardware at this moment */
    if (gdc_5mhz_mode)
        retval |= 0x02;

    return retval;
}

void pc98_write_9a0(Bitu port,Bitu val,Bitu iolen) {
    (void)port;
    (void)iolen;
    sel_9a0 = (unsigned char)val; // what to read back in bit 0
}

/* Port 0x9A8
 *
 * bit[1:0]
 *     11 = invalid
 *     10 = invalid
 *     01 = 31.47KHz
 *     00 = 24.83KHz */
Bitu pc98_read_9a8(Bitu /*port*/,Bitu /*iolen*/) {
    Bitu retval = 0;

    if (pc98_31khz_mode)
        retval |= 0x01;/*31khz*/

    return retval;
}

void pc98_write_9a8(Bitu /*port*/,Bitu val,Bitu /*iolen*/) {
    if ((val&1) != (pc98_31khz_mode?1:0)) {
        pc98_31khz_mode = !!(val&1);
        VGA_SetupDrawing(0);
    }
}

