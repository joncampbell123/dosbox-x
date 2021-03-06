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



#include "dosbox.h"
#include "setup.h"
#include "vga.h"
#include "inout.h"
#include "mem.h"
#include "regs.h"
#include <cstdlib>

extern bool vga_enable_3C6_ramdac;
extern bool vga_sierra_lock_565;

// Tseng ET4K data
typedef struct {
    uint8_t extensionsEnabled;

// Current DAC mode
    uint8_t hicolorDACcmdmode;
// HiColor DAC control register. See comments below. Only bits 5-7 are emulated, close to SC11485 version.
    uint8_t hicolorDACcommand;

// Stored exact values of some registers. Documentation only specifies some bits but hardware checks may
// expect other bits to be preserved.
    Bitu store_3d4_31;
    Bitu store_3d4_32;
    Bitu store_3d4_33;
    Bitu store_3d4_34;
    Bitu store_3d4_35;
    Bitu store_3d4_36;
    Bitu store_3d4_37;
    Bitu store_3d4_3f;

    Bitu store_3c0_16;
    Bitu store_3c0_17;

    Bitu store_3c4_06;
    Bitu store_3c4_07;

    Bitu clockFreq[16];
    Bitu biosMode;
} SVGA_ET4K_DATA;

static SVGA_ET4K_DATA et4k = { 1,0,0,0,0,0,0,0,0,0,0, 0,0, 0,0,
                             { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, 0 };

#define STORE_ET4K(port, index) \
    case 0x##index: \
    et4k.store_##port##_##index = val; \
        break;

#define RESTORE_ET4K(port, index) \
    case 0x##index: \
        return et4k.store_##port##_##index;

// Tseng ET4K implementation
void write_p3d5_et4k(Bitu reg,Bitu val,Bitu iolen) {
    (void)iolen;//UNUSED
    if(!et4k.extensionsEnabled && reg!=0x33)
        return;

    switch(reg) {
    /*
    3d4h index 31h (R/W):  General Purpose
    bit  0-3  Scratch pad
         6-7  Clock Select bits 3-4. Bits 0-1 are in 3C2h/3CCh bits 2-3.
    */
        STORE_ET4K(3d4, 31);

    // 3d4h index 32h - RAS/CAS Configuration (R/W)
    // No effect on emulation. Should not be written by software.
    STORE_ET4K(3d4, 32);

    case 0x33:
        // 3d4 index 33h (R/W): Extended start Address
        // 0-1 Display Start Address bits 16-17
        // 2-3 Cursor start address bits 16-17
        // Used by standard Tseng ID scheme
        et4k.store_3d4_33 = val;
        vga.config.display_start = (vga.config.display_start & 0xffff) | ((val & 0x03)<<16);
        vga.config.cursor_start = (vga.config.cursor_start & 0xffff) | ((val & 0x0c)<<14);
        break;

    /*
    3d4h index 34h (R/W): 6845 Compatibility Control Register
    bit    0  Enable CS0 (alternate clock timing)
           1  Clock Select bit 2.  Bits 0-1 in 3C2h bits 2-3, bits 3-4 are in 3d4h
              index 31h bits 6-7
           2  Tristate ET4000 bus and color outputs if set
           3  Video Subsystem Enable Register at 46E8h if set, at 3C3h if clear.
           4  Enable Translation ROM for reading CRTC and MISCOUT if set
           5  Enable Translation ROM for writing CRTC and MISCOUT if set
           6  Enable double scan in AT&T compatibility mode if set
           7  Enable 6845 compatibility if set
    */
    // TODO: Bit 6 may have effect on emulation
    STORE_ET4K(3d4, 34);

    case 0x35:
    /*
    3d4h index 35h (R/W): Overflow High
    bit    0  Vertical Blank Start Bit 10 (3d4h index 15h).
           1  Vertical Total Bit 10 (3d4h index 6).
           2  Vertical Display End Bit 10 (3d4h index 12h).
           3  Vertical Sync Start Bit 10 (3d4h index 10h).
           4  Line Compare Bit 10 (3d4h index 18h).
           5  Gen-Lock Enabled if set (External sync)
           6  (4000) Read/Modify/Write Enabled if set. Currently not implemented.
           7  Vertical interlace if set. The Vertical timing registers are
            programmed as if the mode was non-interlaced!!
    */
        et4k.store_3d4_35 = val;
        vga.config.line_compare = (vga.config.line_compare & 0x3ff) | ((val&0x10)<<6);
    // Abusing s3 ex_ver_overflow field. This is to be cleaned up later.
        {
            uint8_t s3val =
                ((val & 0x01) << 2) | // vbstart
                ((val & 0x02) >> 1) | // vtotal
                ((val & 0x04) >> 1) | // vdispend
                ((val & 0x08) << 1) | // vsyncstart (?)
                ((val & 0x10) << 2); // linecomp
            if ((s3val ^ vga.s3.ex_ver_overflow) & 0x3) {
                vga.s3.ex_ver_overflow=s3val;
                VGA_StartResize();
            } else vga.s3.ex_ver_overflow=s3val;
        }
        break;

    // 3d4h index 36h - Video System Configuration 1 (R/W)
    // VGADOC provides a lot of info on this register, Ferraro has significantly less detail.
    // This is unlikely to be used by any games. Bit 4 switches chipset into linear mode -
    // that may be useful in some cases if there is any software actually using it.
    // TODO (not near future): support linear addressing
    STORE_ET4K(3d4, 36);

    // 3d4h index 37 - Video System Configuration 2 (R/W)
    // Bits 0,1, and 3 provides information about memory size:
    // 0-1 Bus width (1: 8 bit, 2: 16 bit, 3: 32 bit)
    // 3   Size of RAM chips (0: 64Kx, 1: 256Kx)
    // Other bits have no effect on emulation.
    case 0x37:
        if (val != et4k.store_3d4_37) {
            et4k.store_3d4_37 = val;
            vga.mem.memmask = (((64u*1024u*((val&8u)?4u:1u))<<((val&3u)-1u)) - 1u) & (vga.mem.memsize-1u);
            VGA_SetupHandlers();
        }
        break;

    case 0x3f:
    /*
    3d4h index 3Fh (R/W):
    bit    0  Bit 8 of the Horizontal Total (3d4h index 0)
           2  Bit 8 of the Horizontal Blank Start (3d4h index 3)
           4  Bit 8 of the Horizontal Retrace Start (3d4h index 4)
           7  Bit 8 of the CRTC offset register (3d4h index 13h).
    */
    // The only unimplemented one is bit 7
        et4k.store_3d4_3f = val;
    // Abusing s3 ex_hor_overflow field which very similar. This is
    // to be cleaned up later
        if ((val ^ vga.s3.ex_hor_overflow) & 3) {
            vga.s3.ex_hor_overflow=(val&0x15);
            VGA_StartResize();
        } else vga.s3.ex_hor_overflow=(val&0x15);
        break;
    default:
        LOG(LOG_VGAMISC,LOG_NORMAL)("VGA:CRTC:ET4K:Write to illegal index %2X", (int)reg);
        break;
    }
}

Bitu read_p3d5_et4k(Bitu reg,Bitu iolen) {
    (void)iolen;//UNUSED
    if (!et4k.extensionsEnabled && reg!=0x33)
        return 0x0;
    switch(reg) {
    RESTORE_ET4K(3d4, 31);
    RESTORE_ET4K(3d4, 32);
    RESTORE_ET4K(3d4, 33);
    RESTORE_ET4K(3d4, 34);
    RESTORE_ET4K(3d4, 35);
    RESTORE_ET4K(3d4, 36);
    RESTORE_ET4K(3d4, 37);
    RESTORE_ET4K(3d4, 3f);
    default:
        LOG(LOG_VGAMISC,LOG_NORMAL)("VGA:CRTC:ET4K:Read from illegal index %2X", (int)reg);
        break;
    }
    return 0x0;
}

void write_p3c5_et4k(Bitu reg,Bitu val,Bitu iolen) {
    (void)iolen;//UNUSED
    switch(reg) {
    /*
    3C4h index  6  (R/W): TS State Control
    bit 1-2  Font Width Select in dots/character
            If 3C4h index 4 bit 0 clear:
                0: 9 dots, 1: 10 dots, 2: 12 dots, 3: 6 dots
            If 3C4h index 5 bit 0 set:
                0: 8 dots, 1: 11 dots, 2: 7 dots, 3: 16 dots
            Only valid if 3d4h index 34h bit 3 set.
    */
    // TODO: Figure out if this has any practical use
    STORE_ET4K(3c4, 06);
    // 3C4h index  7  (R/W): TS Auxiliary Mode
    // Unlikely to be used by games (things like ROM enable/disable and emulation of VGA vs EGA)
    STORE_ET4K(3c4, 07);
    default:
        LOG(LOG_VGAMISC,LOG_NORMAL)("VGA:SEQ:ET4K:Write to illegal index %2X", (int)reg);
        break;
    }
}

Bitu read_p3c5_et4k(Bitu reg,Bitu iolen) {
    (void)iolen;//UNUSED
    switch(reg) {
    RESTORE_ET4K(3c4, 06);
    RESTORE_ET4K(3c4, 07);
    default:
        LOG(LOG_VGAMISC,LOG_NORMAL)("VGA:SEQ:ET4K:Read from illegal index %2X", (int)reg);
        break;
    }
    return 0x0;
}

/*
3CDh (R/W): Segment Select
bit 0-3  64k Write bank number (0..15)
    4-7  64k Read bank number (0..15)
*/
void write_p3cd_et4k(Bitu port,Bitu val,Bitu iolen) {
    (void)port;//UNUSED
    (void)iolen;//UNUSED
    vga.svga.bank_write = val & 0x0fu;
    vga.svga.bank_read = (val>>4u) & 0x0fu;
    VGA_SetupHandlers();
}

Bitu read_p3cd_et4k(Bitu port,Bitu iolen) {
    (void)port;//UNUSED
    (void)iolen;//UNUSED
    return (Bitu)((vga.svga.bank_read<<4u)|vga.svga.bank_write);
}

void write_p3c0_et4k(Bitu reg,Bitu val,Bitu iolen) {
    (void)iolen;//UNUSED
    switch(reg) {
    // 3c0 index 16h: ATC Miscellaneous
    // VGADOC provides a lot of information, Ferarro documents only two bits
    // and even those incompletely. The register is used as part of identification
    // scheme.
    // Unlikely to be used by any games but double timing may be useful.
    // TODO: Figure out if this has any practical use
    STORE_ET4K(3c0, 16);
    /*
    3C0h index 17h (R/W):  Miscellaneous 1
    bit   7  If set protects the internal palette ram and redefines the attribute
            bits as follows:
            Monochrome:
            bit 0-2  Select font 0-7
                    3  If set selects blinking
                    4  If set selects underline
                    5  If set prevents the character from being displayed
                    6  If set displays the character at half intensity
                    7  If set selects reverse video
            Color:
            bit 0-1  Selects font 0-3
                    2  Foreground Blue
                    3  Foreground Green
                    4  Foreground Red
                    5  Background Blue
                    6  Background Green
                    7  Background Red
    */
    // TODO: Figure out if this has any practical use
    STORE_ET4K(3c0, 17);
    default:
        LOG(LOG_VGAMISC,LOG_NORMAL)("VGA:ATTR:ET4K:Write to illegal index %2X", (int)reg);
        break;
    }
}

Bitu read_p3c1_et4k(Bitu reg,Bitu iolen) {
    (void)iolen;//UNUSED
    switch(reg) {
    RESTORE_ET4K(3c0, 16);
    RESTORE_ET4K(3c0, 17);
    default:
        LOG(LOG_VGAMISC,LOG_NORMAL)("VGA:ATTR:ET4K:Read from illegal index %2X", (int)reg);
        break;
    }
    return 0x0;
}

/*
These ports are used but have little if any effect on emulation:
    3BFh (R/W): Hercules Compatibility Mode
    3CBh (R/W): PEL Address/Data Wd
    3CEh index 0Dh (R/W): Microsequencer Mode
    3CEh index 0Eh (R/W): Microsequencer Reset
    3d8h (R/W): Display Mode Control
    3DEh (W);  AT&T Mode Control Register
*/

static Bitu get_clock_index_et4k() {
    // Ignoring bit 4, using "only" 16 frequencies. Looks like most implementations had only that
    return ((vga.misc_output>>2)&3) | ((et4k.store_3d4_34<<1)&4) | ((et4k.store_3d4_31>>3)&8);
}

static void set_clock_index_et4k(Bitu index) {
    // Shortwiring register reads/writes for simplicity
    IO_Write(0x3c2, (vga.misc_output&~0x0cu)|((index&3u)<<2u));
    et4k.store_3d4_34 = (et4k.store_3d4_34&~0x02u)|((index&4u)>>1u);
    et4k.store_3d4_31 = (et4k.store_3d4_31&~0xc0u)|((index&8u)<<3u); // (index&0x18) if 32 clock frequencies are to be supported
}

void FinishSetMode_ET4K(Bitu crtc_base, VGA_ModeExtraData* modeData) {
    // Note that switching to a mode always puts the DAC in 15-bit color. An extra BIOS call is necessary to switch to 16-bit color.
    // We are also forcing the mode back to work exactly the way it did on the real hardware
    if (modeData->modeNo & 0x200u) {
        et4k.hicolorDACcommand = 0xa0u;
        modeData->modeNo &= ~0x200u;
    } else {
        et4k.hicolorDACcommand = 0x00u;
    }

    et4k.biosMode = modeData->modeNo;

    IO_Write(0x3cd, 0x00); // both banks to 0

    // Reinterpret hor_overflow. Curiously, three bits out of four are
    // in the same places. Input has hdispend (not supported), output
    // has CRTC offset (also not supported)
    uint8_t et4k_hor_overflow =
        (modeData->hor_overflow & 0x01) |
        (modeData->hor_overflow & 0x04) |
        (modeData->hor_overflow & 0x10);
    IO_Write(crtc_base,0x3f);IO_Write(crtc_base+1,et4k_hor_overflow);

    // Reinterpret ver_overflow
    uint8_t et4k_ver_overflow =
        ((modeData->ver_overflow & 0x01) << 1) | // vtotal10
        ((modeData->ver_overflow & 0x02) << 1) | // vdispend10
        ((modeData->ver_overflow & 0x04) >> 2) | // vbstart10
        ((modeData->ver_overflow & 0x10) >> 1) | // vretrace10 (tseng has vsync start?)
        ((modeData->ver_overflow & 0x40) >> 2);  // line_compare
    IO_Write(crtc_base,0x35);IO_Write(crtc_base+1,et4k_ver_overflow);

    // Clear remaining ext CRTC registers
    IO_Write(crtc_base,0x31);IO_Write(crtc_base+1,0);
    IO_Write(crtc_base,0x32);IO_Write(crtc_base+1,0);
    IO_Write(crtc_base,0x33);IO_Write(crtc_base+1,0);
    IO_Write(crtc_base,0x34);IO_Write(crtc_base+1,0);
    IO_Write(crtc_base,0x36);IO_Write(crtc_base+1,0);
    IO_Write(crtc_base,0x37);IO_Write(crtc_base+1,0x0c|(vga.mem.memsize==1024*1024?3:vga.mem.memsize==512*1024?2:1));
    // Clear ext SEQ
    IO_Write(0x3c4,0x06);IO_Write(0x3c5,0);
    IO_Write(0x3c4,0x07);IO_Write(0x3c5,0);
    // Clear ext ATTR
    IO_Write(0x3c0,0x16);IO_Write(0x3c0,0);
    IO_Write(0x3c0,0x17);IO_Write(0x3c0,0);

    // Select SVGA clock to get close to 60Hz (not particularly clean implementation)
    if (modeData->modeNo > 0x13) {
        Bits target = static_cast<Bits>(modeData->vtotal * 8 * modeData->htotal * 60);
        Bitu best = 1;
        int dist = 100000000;
        for (Bitu i = 0; i < 16; i++) {
            int cdiff = abs( static_cast<int>(target - static_cast<Bits>(et4k.clockFreq[i])) );
            if (cdiff < dist) {
                best = i;
                dist = cdiff;
            }
        }
        set_clock_index_et4k(best);
    }

    if(svga.determine_mode)
        svga.determine_mode();

    // Verified (on real hardware and in a few games): Tseng ET4000 used chain4 implementation
    // different from standard VGA. It was also not limited to 64K in regular mode 13h.
    vga.config.compatible_chain4 = false;
//    vga.vmemwrap = vga.mem.memsize;

    VGA_SetupHandlers();
}

void DetermineMode_ET4K() {
    // Special case for HiColor DAC enabled modes
    if ((et4k.hicolorDACcommand & 0xc0) == 0x80) {
        VGA_SetMode(vga_sierra_lock_565 ? M_LIN16 : M_LIN15);
        return;
    } else if ((et4k.hicolorDACcommand & 0xc0) == 0xc0) {
        VGA_SetMode(M_LIN16);
        return;
    }
    // Close replica from the base implementation. It will stay here
    // until I figure a way to either distinguish M_VGA and M_LIN8 or
    // merge them.
    if (vga.attr.mode_control & 1) {
        if (vga.gfx.mode & 0x40) VGA_SetMode((et4k.biosMode<=0x13)?M_VGA:M_LIN8); // Ugly...
        else if (vga.gfx.mode & 0x20) VGA_SetMode(M_CGA4);
        else if ((vga.gfx.miscellaneous & 0x0c)==0x0c) VGA_SetMode(M_CGA2);
        else VGA_SetMode((et4k.biosMode<=0x13)?M_EGA:M_LIN4);
    } else {
        VGA_SetMode(M_TEXT);
    }
}

void SetClock_ET4K(Bitu which,Bitu target) {
    et4k.clockFreq[which]=1000*target;
    VGA_StartResize();
}

Bitu GetClock_ET4K() {
    return et4k.clockFreq[get_clock_index_et4k()];
}

bool AcceptsMode_ET4K(Bitu mode) {
    return VideoModeMemSize(mode) < vga.mem.memsize;
//  return mode != 0x3d;
}

// Support for HiColor DAC
// Support is sufficient for proper detection (WHATVGA detects UMC 70c178, similar to Sierra SC11487, slightly simpler to work with).
// Control over 15/16-bit color is also implemented. Need better documentation and/or test cases to advance the implementation.
/*
REG06 (R/W):  Command Register
bit   0  (SC11487) (R) Set if bits 5-7 is 1 or 3, clear otherwise
    3-4  (SC11487) Accesses bits 3-4 of the PEL Mask registers (REG02)
      5  (not SC11481/6/8)
         If set two pixel clocks are used to latch the two bytes
         needed for each pixel. Low byte is latched first.
         If clear the low byte is latched on the rising edge of the
         pixel clock and the high byte is latched on the falling edge.
         Only some VGA chips (ET4000 and C&T655x0) can handle this.
      6  (SC11485/7/9, OTI66HC, UM70C178) Set in 16bit (64k) modes (Only valid
           if bit 7 set). On the SC11482/3/4 this bit is read/writable, but
           has no function. On the SC11481/6/8 this bit does not exist.
      7  Set in HiColor (32k/64k) modes, clear in palette modes.
Note:  This register can also be accessed at 3C6h by reading 3C6h four times,
       then all accesses to 3C6h will go the this register until one of the
       registers 3C7h, 3C8h or 3C9h is accessed.
*/

// Need to pass-through all DAC registers
void write_p3c6(Bitu port,Bitu val,Bitu iolen);
void write_p3c7(Bitu port,Bitu val,Bitu iolen);
void write_p3c8(Bitu port,Bitu val,Bitu iolen);
void write_p3c9(Bitu port,Bitu val,Bitu iolen);
Bitu read_p3c6(Bitu port,Bitu iolen);
Bitu read_p3c7(Bitu port,Bitu iolen);
Bitu read_p3c8(Bitu port,Bitu iolen);
Bitu read_p3c9(Bitu port,Bitu iolen);

bool et4k_highcolor_half_pixel_rate() {
    /* if highcolor and NOT using two clocks per pixel */
    return (et4k.hicolorDACcommand & 0x80) && (!(et4k.hicolorDACcommand & 0x20));
}

void write_p3c6_et4k(Bitu port,Bitu val,Bitu iolen) {
    if (et4k.hicolorDACcmdmode <= 3) {
        write_p3c6(port, val, iolen);
    } else {
        uint8_t command = val & 0xe0;
        if (command != et4k.hicolorDACcommand) {
            et4k.hicolorDACcommand = command;
            DetermineMode_ET4K();
        }
    }
}
void write_p3c7_et4k(Bitu port,Bitu val,Bitu iolen) {
    et4k.hicolorDACcmdmode = 0;
    write_p3c7(port, val, iolen);
}
void write_p3c8_et4k(Bitu port,Bitu val,Bitu iolen) {
    et4k.hicolorDACcmdmode = 0;
    write_p3c8(port, val, iolen);
}
void write_p3c9_et4k(Bitu port,Bitu val,Bitu iolen) {
    et4k.hicolorDACcmdmode = 0;
    write_p3c9(port, val, iolen);
}
Bitu read_p3c6_et4k(Bitu port,Bitu iolen) {
    if (et4k.hicolorDACcmdmode <= 3) {
        if (vga_enable_3C6_ramdac)
            et4k.hicolorDACcmdmode++;

        return read_p3c6(port, iolen);
    } else {
        return et4k.hicolorDACcommand;
    }
}
Bitu read_p3c7_et4k(Bitu port,Bitu iolen) {
    et4k.hicolorDACcmdmode = 0;
    return read_p3c7(port, iolen);
}
Bitu read_p3c8_et4k(Bitu port,Bitu iolen) {
    et4k.hicolorDACcmdmode = 0;
    return read_p3c8(port, iolen);
}
Bitu read_p3c9_et4k(Bitu port,Bitu iolen) {
    et4k.hicolorDACcmdmode = 0;
    return read_p3c9(port, iolen);
}

void SetupDAC_ET4K() {
    IO_RegisterWriteHandler(0x3c6,write_p3c6_et4k,IO_MB);
    IO_RegisterReadHandler(0x3c6,read_p3c6_et4k,IO_MB);
    IO_RegisterWriteHandler(0x3c7,write_p3c7_et4k,IO_MB);
    IO_RegisterReadHandler(0x3c7,read_p3c7_et4k,IO_MB);
    IO_RegisterWriteHandler(0x3c8,write_p3c8_et4k,IO_MB);
    IO_RegisterReadHandler(0x3c8,read_p3c8_et4k,IO_MB);
    IO_RegisterWriteHandler(0x3c9,write_p3c9_et4k,IO_MB);
    IO_RegisterReadHandler(0x3c9,read_p3c9_et4k,IO_MB);
}

// BIOS extensions for HiColor-enabled cards
bool INT10_SetVideoMode(uint16_t mode);

void INT10Extensions_ET4K() {
    switch (reg_ax) {
    case 0x10F0: /* ET4000: SET HiColor GRAPHICS MODE */
        if (INT10_SetVideoMode(0x200 | uint16_t(reg_bl))) {
            reg_ax = 0x0010;
        }
        break;
    case 0x10F1: /* ET4000: GET DAC TYPE */
        reg_ax = 0x0010;
        if (vga_enable_3C6_ramdac)
            reg_bl = 0x01;
        else
            reg_bl = 0x00;
        break;
    case 0x10F2: /* ET4000: CHECK/SET HiColor MODE */
        switch (reg_bl) {
        case 0:
            reg_ax = 0x0010;
            break;
        case 1: case 2:
            {
                uint8_t val = (reg_bl == 1) ? 0xa0 : 0xe0;
                if (val != et4k.hicolorDACcommand) {
                    et4k.hicolorDACcommand = val;
                    DetermineMode_ET4K();
                    reg_ax = 0x0010;
                }
            }
            break;
        }
        switch (et4k.hicolorDACcommand & 0xc0) {
        case 0x80:
            reg_bl = 1;
            break;
        case 0xc0:
            reg_bl = 2;
            break;
        default:
            reg_bl = 0;
            break;
        }
    }
}

extern bool VGA_BIOS_use_rom;

void SVGA_Setup_TsengET4K(void) {
    svga.write_p3d5 = &write_p3d5_et4k;
    svga.read_p3d5 = &read_p3d5_et4k;
    svga.write_p3c5 = &write_p3c5_et4k;
    svga.read_p3c5 = &read_p3c5_et4k;
    svga.write_p3c0 = &write_p3c0_et4k;
    svga.read_p3c1 = &read_p3c1_et4k;

    svga.set_video_mode = &FinishSetMode_ET4K;
    svga.determine_mode = &DetermineMode_ET4K;
    svga.set_clock = &SetClock_ET4K;
    svga.get_clock = &GetClock_ET4K;
    svga.accepts_mode = &AcceptsMode_ET4K;
    svga.setup_dac = &SetupDAC_ET4K;
    svga.int10_extensions = &INT10Extensions_ET4K;

    // From the depths of X86Config, probably inexact
    VGA_SetClock(0,CLK_25);
    VGA_SetClock(1,CLK_28);
    VGA_SetClock(2,32400);
    VGA_SetClock(3,35900);
    VGA_SetClock(4,39900);
    VGA_SetClock(5,44700);
    VGA_SetClock(6,31400);
    VGA_SetClock(7,37500);
    VGA_SetClock(8,50000);
    VGA_SetClock(9,56500);
    VGA_SetClock(10,64900);
    VGA_SetClock(11,71900);
    VGA_SetClock(12,79900);
    VGA_SetClock(13,89600);
    VGA_SetClock(14,62800);
    VGA_SetClock(15,74800);

    IO_RegisterReadHandler(0x3cd,read_p3cd_et4k,IO_MB);
    IO_RegisterWriteHandler(0x3cd,write_p3cd_et4k,IO_MB);

    // Default to 1M of VRAM
    if (vga.mem.memsize == 0)
        vga.mem.memsize = 1024*1024;

    if (vga.mem.memsize < 512*1024)
        vga.mem.memsize = 256*1024;
    else if (vga.mem.memsize < 1024*1024)
        vga.mem.memsize = 512*1024;
    else
        vga.mem.memsize = 1024*1024;

    if (!VGA_BIOS_use_rom) {
        // Tseng ROM signature
        phys_writes(PhysMake(0xc000,0)+0x0075, " Tseng ", 8);
    }
}


// Tseng ET3K implementation
typedef struct {
// Stored exact values of some registers. Documentation only specifies some bits but hardware checks may
// expect other bits to be preserved.
    Bitu store_3d4_1b;
    Bitu store_3d4_1c;
    Bitu store_3d4_1d;
    Bitu store_3d4_1e;
    Bitu store_3d4_1f;
    Bitu store_3d4_20;
    Bitu store_3d4_21;
    Bitu store_3d4_23; // note that 22 is missing
    Bitu store_3d4_24;
    Bitu store_3d4_25;

    Bitu store_3c0_16;
    Bitu store_3c0_17;

    Bitu store_3c4_06;
    Bitu store_3c4_07;

    Bitu clockFreq[8];
    Bitu biosMode;
} SVGA_ET3K_DATA;

static SVGA_ET3K_DATA et3k = { 0,0,0,0,0,0,0,0,0,0, 0,0, 0,0, {0,0,0,0,0,0,0,0}, 0 };

#define STORE_ET3K(port, index) \
    case 0x##index: \
    et3k.store_##port##_##index = val; \
        break;

#define RESTORE_ET3K(port, index) \
    case 0x##index: \
        return et3k.store_##port##_##index;


void write_p3d5_et3k(Bitu reg,Bitu val,Bitu iolen) {
    (void)iolen;//UNUSED
    switch(reg) {
    // 3d4 index 1bh-21h: Hardware zoom control registers
    // I am not sure if there was a piece of software that used these.
    // Not implemented and will probably stay this way.
    STORE_ET3K(3d4, 1b);
    STORE_ET3K(3d4, 1c);
    STORE_ET3K(3d4, 1d);
    STORE_ET3K(3d4, 1e);
    STORE_ET3K(3d4, 1f);
    STORE_ET3K(3d4, 20);
    STORE_ET3K(3d4, 21);

    case 0x23:
    /*
    3d4h index 23h (R/W): Extended start ET3000
    bit   0  Cursor start address bit 16
          1  Display start address bit 16
          2  Zoom start address bit 16
          7  If set memory address 8 is output on the MBSL pin (allowing access to
             1MB), if clear the blanking signal is output.
    */
    // Only bits 1 and 2 are supported. Bit 2 is related to hardware zoom, bit 7 is too obscure to be useful
        et3k.store_3d4_23 = val;
        vga.config.display_start = (vga.config.display_start & 0xffff) | ((val & 0x02)<<15);
        vga.config.cursor_start = (vga.config.cursor_start & 0xffff) | ((val & 0x01)<<16);
        break;


    /*
    3d4h index 24h (R/W): Compatibility Control
    bit   0  Enable Clock Translate if set
        1  Clock Select bit 2. Bits 0-1 are in 3C2h/3CCh.
        2  Enable tri-state for all output pins if set
        3  Enable input A8 of 1MB DRAMs from the INTL output if set
        4  Reserved
        5  Enable external ROM CRTC translation if set
        6  Enable Double Scan and Underline Attribute if set
        7  Enable 6845 compatibility if set.
    */
    // TODO: Some of these may be worth implementing.
        STORE_ET3K(3d4, 24);


    case 0x25:
    /*
    3d4h index 25h (R/W): Overflow High
    bit   0  Vertical Blank Start bit 10
          1  Vertical Total Start bit 10
          2  Vertical Display End bit 10
          3  Vertical Sync Start bit 10
          4  Line Compare bit 10
          5-6  Reserved
          7  Vertical Interlace if set
    */
        et3k.store_3d4_25 = val;
        vga.config.line_compare = (vga.config.line_compare & 0x3ff) | ((val&0x10)<<6);
    // Abusing s3 ex_ver_overflow field. This is to be cleaned up later.
        {
            uint8_t s3val =
                ((val & 0x01) << 2) | // vbstart
                ((val & 0x02) >> 1) | // vtotal
                ((val & 0x04) >> 1) | // vdispend
                ((val & 0x08) << 1) | // vsyncstart (?)
                ((val & 0x10) << 2); // linecomp
            if ((s3val ^ vga.s3.ex_ver_overflow) & 0x3) {
                vga.s3.ex_ver_overflow=s3val;
                VGA_StartResize();
            } else vga.s3.ex_ver_overflow=s3val;
        }
        break;

    default:
        LOG(LOG_VGAMISC,LOG_NORMAL)("VGA:CRTC:ET3K:Write to illegal index %2X", (int)reg);
        break;
    }
}

Bitu read_p3d5_et3k(Bitu reg,Bitu iolen) {
    (void)iolen;//UNUSED
    switch(reg) {
    RESTORE_ET3K(3d4, 1b);
    RESTORE_ET3K(3d4, 1c);
    RESTORE_ET3K(3d4, 1d);
    RESTORE_ET3K(3d4, 1e);
    RESTORE_ET3K(3d4, 1f);
    RESTORE_ET3K(3d4, 20);
    RESTORE_ET3K(3d4, 21);
    RESTORE_ET3K(3d4, 23);
    RESTORE_ET3K(3d4, 24);
    RESTORE_ET3K(3d4, 25);
    default:
        LOG(LOG_VGAMISC,LOG_NORMAL)("VGA:CRTC:ET3K:Read from illegal index %2X", (int)reg);
        break;
    }
    return 0x0;
}

void write_p3c5_et3k(Bitu reg,Bitu val,Bitu iolen) {
    (void)iolen;//UNUSED
    switch(reg) {
    // Both registers deal mostly with hardware zoom which is not implemented. Other bits
    // seem to be useless for emulation with the exception of index 7 bit 4 (font select)
    STORE_ET3K(3c4, 06);
    STORE_ET3K(3c4, 07);
    default:
        LOG(LOG_VGAMISC,LOG_NORMAL)("VGA:SEQ:ET3K:Write to illegal index %2X", (int)reg);
        break;
    }
}

Bitu read_p3c5_et3k(Bitu reg,Bitu iolen) {
    (void)iolen;//UNUSED
    switch(reg) {
    RESTORE_ET3K(3c4, 06);
    RESTORE_ET3K(3c4, 07);
    default:
        LOG(LOG_VGAMISC,LOG_NORMAL)("VGA:SEQ:ET3K:Read from illegal index %2X", (int)reg);
        break;
    }
    return 0x0;
}

/*
3CDh (R/W): Segment Select
bit 0-2  64k Write bank number
    3-5  64k Read bank number
    6-7  Segment Configuration.
           0  128K segments
           1   64K segments
           2  1M linear memory
NOTES: 1M linear memory is not supported
*/
void write_p3cd_et3k(Bitu port,Bitu val,Bitu iolen) {
    (void)port;
    (void)iolen;//UNUSED
    vga.svga.bank_write = val & 0x07;
    vga.svga.bank_read = (val>>3) & 0x07;
    vga.svga.bank_size = (val&0x40)?64*1024:128*1024;
    VGA_SetupHandlers();
}

Bitu read_p3cd_et3k(Bitu port,Bitu iolen) {
    (void)port;
    (void)iolen;//UNUSED
    return (Bitu)(((Bitu)vga.svga.bank_read<<3u)|(Bitu)vga.svga.bank_write|((vga.svga.bank_size==128u*1024u)?0u:0x40u));
}

void write_p3c0_et3k(Bitu reg,Bitu val,Bitu iolen) {
    (void)iolen;//UNUSED
// See ET4K notes.
    switch(reg) {
    STORE_ET3K(3c0, 16);
    STORE_ET3K(3c0, 17);
    default:
        LOG(LOG_VGAMISC,LOG_NORMAL)("VGA:ATTR:ET3K:Write to illegal index %2X", (int)reg);
        break;
    }
}

Bitu read_p3c1_et3k(Bitu reg,Bitu iolen) {
    (void)iolen;//UNUSED
    switch(reg) {
    RESTORE_ET3K(3c0, 16);
    RESTORE_ET3K(3c0, 17);
    default:
        LOG(LOG_VGAMISC,LOG_NORMAL)("VGA:ATTR:ET3K:Read from illegal index %2X", (int)reg);
        break;
    }
    return 0x0;
}

/*
These ports are used but have little if any effect on emulation:
    3B8h (W):  Display Mode Control Register
    3BFh (R/W): Hercules Compatibility Mode
    3CBh (R/W): PEL Address/Data Wd
    3CEh index 0Dh (R/W): Microsequencer Mode
    3CEh index 0Eh (R/W): Microsequencer Reset
    3d8h (R/W): Display Mode Control
    3D9h (W): Color Select Register
    3dAh (W):  Feature Control Register
    3DEh (W);  AT&T Mode Control Register
*/

static Bitu get_clock_index_et3k() {
    return ((vga.misc_output>>2)&3) | ((et3k.store_3d4_24<<1)&4);
}

static void set_clock_index_et3k(Bitu index) {
    // Shortwiring register reads/writes for simplicity
    IO_Write(0x3c2, (vga.misc_output&~0x0cu)|((index&3u)<<2u));
    et3k.store_3d4_24 = (et3k.store_3d4_24&~0x02u)|((index&4u)>>1u);
}

void FinishSetMode_ET3K(Bitu crtc_base, VGA_ModeExtraData* modeData) {
    et3k.biosMode = modeData->modeNo;

    IO_Write(0x3cd, 0x40); // both banks to 0, 64K bank size

    // Tseng ET3K does not have horizontal overflow bits
    // Reinterpret ver_overflow
    uint8_t et4k_ver_overflow =
        ((modeData->ver_overflow & 0x01) << 1) | // vtotal10
        ((modeData->ver_overflow & 0x02) << 1) | // vdispend10
        ((modeData->ver_overflow & 0x04) >> 2) | // vbstart10
        ((modeData->ver_overflow & 0x10) >> 1) | // vretrace10 (tseng has vsync start?)
        ((modeData->ver_overflow & 0x40) >> 2);  // line_compare
    IO_Write(crtc_base,0x25);IO_Write(crtc_base+1,et4k_ver_overflow);

    // Clear remaining ext CRTC registers
    for (uint8_t i=0x16; i<=0x21; i++) {
        IO_Write(crtc_base,i);
        IO_Write(crtc_base+1,0);
    }

    IO_Write(crtc_base,0x23);IO_Write(crtc_base+1,0);
    IO_Write(crtc_base,0x24);IO_Write(crtc_base+1,0);
    // Clear ext SEQ
    IO_Write(0x3c4,0x06);IO_Write(0x3c5,0);
    IO_Write(0x3c4,0x07);IO_Write(0x3c5,0x40); // 0 in this register breaks WHATVGA
    // Clear ext ATTR
    IO_Write(0x3c0,0x16);IO_Write(0x3c0,0);
    IO_Write(0x3c0,0x17);IO_Write(0x3c0,0);

    // Select SVGA clock to get close to 60Hz (not particularly clean implementation)
    if (modeData->modeNo > 0x13) {
        Bits target = static_cast<Bits>(modeData->vtotal * 8 * modeData->htotal * 60);
        Bitu best = 1;
        int dist = 100000000;
        for (Bitu i = 0; i < 8; i++) {
            int cdiff = abs( static_cast<int32_t>(target - static_cast<Bits>(et3k.clockFreq[i])) );
            if (cdiff < dist) {
                best = i;
                dist = cdiff;
            }
        }
        set_clock_index_et3k(best);
    }

    if (svga.determine_mode)
        svga.determine_mode();

    // Verified on functioning (at last!) hardware: Tseng ET3000 is the same as ET4000 when
    // it comes to chain4 architecture
    vga.config.compatible_chain4 = false;
//    vga.vmemwrap = vga.mem.memsize;

    VGA_SetupHandlers();
}

void DetermineMode_ET3K() {
    // Close replica from the base implementation. It will stay here
    // until I figure a way to either distinguish M_VGA and M_LIN8 or
    // merge them.
    if (vga.attr.mode_control & 1) {
        if (vga.gfx.mode & 0x40) VGA_SetMode((et3k.biosMode<=0x13)?M_VGA:M_LIN8); // Ugly...
        else if (vga.gfx.mode & 0x20) VGA_SetMode(M_CGA4);
        else if ((vga.gfx.miscellaneous & 0x0c)==0x0c) VGA_SetMode(M_CGA2);
        else VGA_SetMode((et3k.biosMode<=0x13)?M_EGA:M_LIN4);
    } else {
        VGA_SetMode(M_TEXT);
    }
}

void SetClock_ET3K(Bitu which,Bitu target) {
    et3k.clockFreq[which]=1000*target;
    VGA_StartResize();
}

Bitu GetClock_ET3K() {
    return et3k.clockFreq[get_clock_index_et3k()];
}

bool AcceptsMode_ET3K(Bitu mode) {
    return mode <= 0x37 && mode != 0x2f && VideoModeMemSize(mode) < vga.mem.memsize;
}

void SVGA_Setup_TsengET3K(void) {
    svga.write_p3d5 = &write_p3d5_et3k;
    svga.read_p3d5 = &read_p3d5_et3k;
    svga.write_p3c5 = &write_p3c5_et3k;
    svga.read_p3c5 = &read_p3c5_et3k;
    svga.write_p3c0 = &write_p3c0_et3k;
    svga.read_p3c1 = &read_p3c1_et3k;

    svga.set_video_mode = &FinishSetMode_ET3K;
    svga.determine_mode = &DetermineMode_ET3K;
    svga.set_clock = &SetClock_ET3K;
    svga.get_clock = &GetClock_ET3K;
    svga.accepts_mode = &AcceptsMode_ET3K;

    VGA_SetClock(0,CLK_25);
    VGA_SetClock(1,CLK_28);
    VGA_SetClock(2,32400);
    VGA_SetClock(3,35900);
    VGA_SetClock(4,39900);
    VGA_SetClock(5,44700);
    VGA_SetClock(6,31400);
    VGA_SetClock(7,37500);

    IO_RegisterReadHandler(0x3cd,read_p3cd_et3k,IO_MB);
    IO_RegisterWriteHandler(0x3cd,write_p3cd_et3k,IO_MB);

    vga.mem.memsize = 512*1024; // Cannot figure how this was supposed to work on the real card

    // Tseng ROM signature
    PhysPt rom_base=PhysMake(0xc000,0);
    phys_writeb(rom_base+0x0075,' ');
    phys_writeb(rom_base+0x0076,'T');
    phys_writeb(rom_base+0x0077,'s');
    phys_writeb(rom_base+0x0078,'e');
    phys_writeb(rom_base+0x0079,'n');
    phys_writeb(rom_base+0x007a,'g');
    phys_writeb(rom_base+0x007b,' ');
}

// save state support
void POD_Save_VGA_Tseng( std::ostream& stream )
{
	// static globals


	// - pure struct data
	WRITE_POD( &et4k, et4k );
	WRITE_POD( &et3k, et3k );
}


void POD_Load_VGA_Tseng( std::istream& stream )
{
	// static globals


	// - pure struct data
	READ_POD( &et4k, et4k );
	READ_POD( &et3k, et3k );
}
