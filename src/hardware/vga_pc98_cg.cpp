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

/* Character Generator (CG) font access state */
uint16_t                    a1_font_load_addr = 0;
uint8_t                     a1_font_char_offset = 0;

/* Character Generator ports.
 * This is in fact officially documented by NEC in
 * a 1986 book published about NEC BIOS and BASIC ROM. */
Bitu pc98_a1_read(Bitu port,Bitu iolen) {
    (void)iolen;//UNUSED
    switch (port) {
        case 0xA9: // an 8-bit I/O port to access font RAM by...
            // NOTES: On a PC-9821 Lt2 laptop, the character ROM doesn't seem to latch valid data beyond
            //        0xxx5D. Often this reads back as zero, but depending on whatever random data is floating
            //        on the bus can read back nonzero. This doesn't apply to 0x0000-0x00FF of course (single wide
            //        characters), but only to the double-wide character set where (c & 0x007F) >= 0x5D.
            //        This behavior should be emulated. */
            return pc98_font_char_read(a1_font_load_addr,a1_font_char_offset & 0xF,(a1_font_char_offset & 0x20) ? 0 : 1);
        default:
            break;
    }

    return ~0ul;
}

/* Character Generator ports.
 * This is in fact officially documented by NEC in
 * a 1986 book published about NEC BIOS and BASIC ROM. */
void pc98_a1_write(Bitu port,Bitu val,Bitu iolen) {
    (void)iolen;//UNUSED
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
            a1_font_char_offset = (uint8_t)val;
            break;
        case 0xA7:
            /* TODO: Various controls for the text layer */
            break;
        case 0xA9: // an 8-bit I/O port to access font RAM by...
                   // this is what Touhou Project uses to load fonts.
                   // never mind decompiling INT 18h on real hardware shows instead
                   // a similar sequence with REP MOVSW to A400:0000...
                   //
                   // there's a restriction noted with INT 18h AH=1Ah where the only
                   // codes you can overwrite are 0xxx76 and 0xxx77. I'm guessing that
                   // having 512KB of RAM out there dedicated to nothing but fonts
                   // is probably not economical to NEC's bottom line, and this
                   // restriction makes me wonder if the font is held in ROM except
                   // for this narrow sliver of codes, which map to about 8KB of RAM
                   // (128*2*16) * 2 = 8192 bytes
                   //
                   // I'm also guessing that this RAM is not involved with the single-wide
                   // character set, which is why writes to 0x0056/0x0057 are redirected to
                   // 0x8056/0x8057. Without this hack, Touhou Project 2 will overwrite
                   // the letter 'W' when loading it's font data (Level 1 will show "Eastern  ind"
                   // instead of "Eastern Wind" for the music title as a result).
                   //
                   // On real hardware it seems, attempts to write anywhere outside 0xxx56/0xxx57
                   // are ignored. They are not remapped. Attempts to write to 0x0056 are ignored
                   // by the hardware (since that conflicts with single-wide chars) but you can
                   // write to that cell if you write to 0x8056 instead.
            if ((a1_font_load_addr & 0x007E) == 0x0056 && (a1_font_load_addr & 0xFF00) != 0x0000)
                pc98_font_char_write(a1_font_load_addr,a1_font_char_offset & 0xF,(a1_font_char_offset & 0x20) ? 0 : 1,(uint8_t)val);
            else
                LOG_MSG("A1 port attempt to write FONT ROM char 0x%x",a1_font_load_addr);
            break;
        default:
            LOG_MSG("A1 port %lx val %lx unexpected",(unsigned long)port,(unsigned long)val);
            break;
    }
}

