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

#if defined(_MSC_VER)
#pragma warning(disable:4065) /* switch statements without case labels */
#endif

void pc98_egc_shift_reinit();

extern egc_quad             pc98_egc_bgcm;
extern egc_quad             pc98_egc_fgcm;

uint16_t                    pc98_egc_raw_values[8] = {0};

uint8_t                     pc98_egc_access=0;
uint8_t                     pc98_egc_srcmask[2]; /* host given (Neko: egc.srcmask) */
uint8_t                     pc98_egc_maskef[2]; /* effective (Neko: egc.mask2) */
uint8_t                     pc98_egc_mask[2]; /* host given (Neko: egc.mask) */

uint8_t                     pc98_egc_fgc = 0;
uint8_t                     pc98_egc_lead_plane = 0;
uint8_t                     pc98_egc_compare_lead = 0;
uint8_t                     pc98_egc_lightsource = 0;
uint8_t                     pc98_egc_shiftinput = 0;
uint8_t                     pc98_egc_regload = 0;
uint8_t                     pc98_egc_rop = 0xF0;
uint8_t                     pc98_egc_foreground_color = 0;
uint8_t                     pc98_egc_background_color = 0;

bool                        pc98_egc_shift_descend = false;
uint8_t                     pc98_egc_shift_destbit = 0;
uint8_t                     pc98_egc_shift_srcbit = 0;
uint16_t                    pc98_egc_shift_length = 0xF;

Bitu pc98_egc4a0_read(Bitu port,Bitu iolen) {
    (void)iolen;//UNUSED
    /* Neko Project II suggests the I/O ports disappear when not in EGC mode.
     * Is that true? */
    if (!(pc98_gdc_vramop & (1 << VOPBIT_EGC))) {
//        LOG_MSG("EGC 4A0 read port 0x%x when EGC not enabled",(unsigned int)port);
        return ~0ul;
    }

    /* assume: (port & 1) == 0 [even] and iolen == 2 */
    switch (port & 0x0E) {
        default:
            LOG_MSG("PC-98 EGC: Unhandled read from 0x%x",(unsigned int)port);
            break;
    }

    return ~0ul;
}

void pc98_egc4a0_write(Bitu port,Bitu val,Bitu iolen) {
    (void)iolen;//UNUSED
    /* Neko Project II suggests the I/O ports disappear when not in EGC mode.
     * Is that true? */
    if (!(pc98_gdc_vramop & (1 << VOPBIT_EGC))) {
//        LOG_MSG("EGC 4A0 write port 0x%x when EGC not enabled",(unsigned int)port);
        return;
    }

    pc98_egc_raw_values[(port>>1u)&7u] = (uint16_t)val;

    /* assume: (port & 1) == 0 [even] and iolen == 2 */
    switch (port & 0x0E) {
        case 0x0: /* 0x4A0 */
            /* bits [15:8] = 0xFF
             * bits [7:0] = enable writing to plane (NTS: only bits 3-0 have meaning in 16-color mode).
             * as far as I can tell, bits [7:0] correspond to the same enable bits as port 0x7C [3:0] */
            pc98_egc_access = val & 0xFF;
            break;
        case 0x2: /* 0x4A2 */
            /* bits [15:15] = 0
             * bits [14:13] = foreground, background color
             *    11 = invalid
             *    10 = foreground color
             *    01 = background color
             *    00 = pattern register
             * bits [12:12] = 0
             * bits [11:8] = lead plane
             *    0111 = VRAM plane #7
             *    0110 = VRAM plane #6
             *    0101 = VRAM plane #5
             *    0100 = VRAM plane #4
             *    0011 = VRAM plane #3
             *    0010 = VRAM plane #2
             *    0001 = VRAM plane #1
             *    0000 = VRAM plane #0
             * bits [7:0] = unused (0xFF) */
            pc98_egc_fgc = (val >> 13) & 3;
            pc98_egc_lead_plane = (val >> 8) & 15;
            break;
        case 0x4: /* 0x4A4 */
            /* bits [15:14] = 0 (unused)
             * bits [13:13] = 0=compare lead plane  1=don't
             * bits [12:11] = light source
             *    11 = invalid
             *    10 = write the contents of the palette register
             *    01 = write the result of the raster operation
             *    00 = write CPU data
             * bits [10:10] = read source
             *    1 = shifter input is CPU write data
             *    0 = shifter input is VRAM data
             * bits [9:8] = register load
             *    11 = invalid
             *    10 = load VRAM data before writing on VRAM write
             *    01 = load VRAM data into pattern/tile register on VRAM read
             *    00 = Do not change pattern/tile register
             * bits [7:0] = ROP
             *    shifter:       11110000
             *    destination:   11001100
             *    pattern reg:   10101010
             *
             *    examples:
             *    11110000 = VRAM transfer
             *    00001111 = VRAM reverse transfer
             *    11001100 = NOP
             *    00110011 = VRAM inversion
             *    11111111 = VRAM fill
             *    00000000 = VRAM erase
             *    10101010 = Pattern fill
             *    01010101 = Pattern reversal fill */
            pc98_egc_compare_lead = ((val >> 13) & 1) ^ 1;
            pc98_egc_lightsource = (val >> 11) & 3;
            pc98_egc_shiftinput = (val >> 10) & 1;
            pc98_egc_regload = (val >> 8) & 3;
            pc98_egc_rop = (val & 0xFF);
            break;
        case 0x6: /* 0x4A6 */
            /* If FGC = 0 and BGC = 0:
             *   bits [15:0] = 0
             * If FGC = 1 or BGC = 1:
             *   bits [15:8] = 0
             *   bits [7:0] = foreground color (all 8 bits used in 256-color mode) */
            pc98_egc_foreground_color = (uint8_t)val;
            pc98_egc_fgcm[0].w = (val & 1) ? 0xFFFF : 0x0000;
            pc98_egc_fgcm[1].w = (val & 2) ? 0xFFFF : 0x0000;
            pc98_egc_fgcm[2].w = (val & 4) ? 0xFFFF : 0x0000;
            pc98_egc_fgcm[3].w = (val & 8) ? 0xFFFF : 0x0000;
            break;
        case 0x8: /* 0x4A8 */
            if (pc98_egc_fgc == 0)
                *((uint16_t*)pc98_egc_mask) = (uint16_t)val;
            break;
        case 0xA: /* 0x4AA */
            /* If FGC = 0 and BGC = 0:
             *   bits [15:0] = 0
             * If FGC = 1 or BGC = 1:
             *   bits [15:8] = 0
             *   bits [7:0] = foreground color (all 8 bits used in 256-color mode) */
            pc98_egc_background_color = (uint8_t)val;
            pc98_egc_bgcm[0].w = (val & 1) ? 0xFFFF : 0x0000;
            pc98_egc_bgcm[1].w = (val & 2) ? 0xFFFF : 0x0000;
            pc98_egc_bgcm[2].w = (val & 4) ? 0xFFFF : 0x0000;
            pc98_egc_bgcm[3].w = (val & 8) ? 0xFFFF : 0x0000;
            break;
        case 0xC: /* 0x4AC */
            /* bits[15:13] = 0
             * bits[12:12] = shift direction 0=ascend 1=descend
             * bits[11:8] = 0
             * bits[7:4] = destination bit address
             * bits[3:0] = source bit address */
            pc98_egc_shift_descend = !!((val >> 12) & 1);
            pc98_egc_shift_destbit = (val >> 4) & 0xF;
            pc98_egc_shift_srcbit = val & 0xF;
            pc98_egc_shift_reinit();
            break;
        case 0xE: /* 0x4AE */
            /* bits[15:12] = 0
             * bits[11:0] = bit length (0 to 4095) */
            pc98_egc_shift_length = val & 0xFFF;
            pc98_egc_shift_reinit();
            break;
        default:
            // LOG_MSG("PC-98 EGC: Unhandled write to 0x%x val 0x%x",(unsigned int)port,(unsigned int)val);
            break;
    }
}

// I/O access to 0x4A0-0x4AF must be WORD sized and even port, or the system hangs if you try.
Bitu pc98_egc4a0_read_warning(Bitu port,Bitu iolen) {
    /* Neko Project II suggests the I/O ports disappear when not in EGC mode.
     * Is that true? */
    if (!(pc98_gdc_vramop & (1 << VOPBIT_EGC))) {
//        LOG_MSG("EGC 4A0 read port 0x%x when EGC not enabled",(unsigned int)port);
        return ~0ul;
    }

    LOG_MSG("PC-98 EGC warning: I/O read from port 0x%x (len=%u) known to possibly hang the system on real hardware",
        (unsigned int)port,(unsigned int)iolen);

    return ~0ul;
}

// I/O access to 0x4A0-0x4AF must be WORD sized and even port, or the system hangs if you try.
void pc98_egc4a0_write_warning(Bitu port,Bitu val,Bitu iolen) {
    /* Neko Project II suggests the I/O ports disappear when not in EGC mode.
     * Is that true? */
    if (!(pc98_gdc_vramop & (1 << VOPBIT_EGC))) {
//        LOG_MSG("EGC 4A0 write port 0x%x when EGC not enabled",(unsigned int)port);
        return;
    }

    if (port & 1) {
        pc98_egc_raw_values[(port>>1u)&7u] &= ~0xFF00u;
        pc98_egc_raw_values[(port>>1u)&7u] |= val << 8u;
    }
    else {
        pc98_egc_raw_values[(port>>1u)&7u] &= ~0xFFu;
        pc98_egc_raw_values[(port>>1u)&7u] |= val;
    }

    switch (port & 0xF) {
        case 0x6:
            /* if the BIOS reports EGC, many early games will write bytewise I/O to port 4A6h */
            pc98_egc_foreground_color = (uint8_t)val;
            pc98_egc_fgcm[0].w = (val & 1) ? 0xFFFF : 0x0000;
            pc98_egc_fgcm[1].w = (val & 2) ? 0xFFFF : 0x0000;
            pc98_egc_fgcm[2].w = (val & 4) ? 0xFFFF : 0x0000;
            pc98_egc_fgcm[3].w = (val & 8) ? 0xFFFF : 0x0000;
            break;
        default:
            LOG_MSG("PC-98 EGC warning: I/O write to port 0x%x (val=0x%x len=%u) known to possibly hang the system on real hardware",
                (unsigned int)port,(unsigned int)val,(unsigned int)iolen);
            break;
    }
}

