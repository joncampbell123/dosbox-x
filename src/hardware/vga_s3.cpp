/*
 *  Copyright (C) 2002-2019  The DOSBox Team
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335, USA.
 */


#include "dosbox.h"
#include "inout.h"
#include "vga.h"
#include "mem.h"
#include "pci_bus.h"

void SVGA_S3_WriteCRTC(Bitu reg,Bitu val,Bitu iolen) {
    (void)iolen;//UNUSED
    switch (reg) {
    case 0x31:  /* CR31 Memory Configuration */
//TODO Base address
        vga.s3.reg_31 = (Bit8u)val;
        vga.config.compatible_chain4 = !(val&0x08);
//        if (vga.config.compatible_chain4) vga.vmemwrap = 256*1024;
//        else vga.vmemwrap = vga.mem.memsize;
        vga.config.display_start = (vga.config.display_start&~0x30000ul)|((val&0x30u)<<12ul);
        VGA_DetermineMode();
        VGA_SetupHandlers();
        break;
        /*
            0   Enable Base Address Offset (CPUA BASE). Enables bank operation if
                set, disables if clear.
            1   Two Page Screen Image. If set enables 2048 pixel wide screen setup
            2   VGA 16bit Memory Bus Width. Set for 16bit, clear for 8bit
            3   Use Enhanced Mode Memory Mapping (ENH MAP). Set to enable access to
                video memory above 256k.
            4-5 Bit 16-17 of the Display Start Address. For the 801/5,928 see index
                51h, for the 864/964 see index 69h.
            6   High Speed Text Display Font Fetch Mode. If set enables Page Mode
                for Alpha Mode Font Access.
            7   (not 864/964) Extended BIOS ROM Space Mapped out. If clear the area
                C6800h-C7FFFh is mapped out, if set it is accessible.
        */
    case 0x35:  /* CR35 CRT Register Lock */
        if (vga.s3.reg_lock1 != 0x48) return;   //Needed for uvconfig detection
        vga.s3.reg_35=val & 0xf0;
        if ((vga.svga.bank_read & 0xf) ^ (val & 0xf)) {
            vga.svga.bank_read&=0xf0;
            vga.svga.bank_read|=val & 0xf;
            vga.svga.bank_write = vga.svga.bank_read;
            VGA_SetupHandlers();
        }
        break;
        /*
            0-3 CPU Base Address. 64k bank number. For the 801/5 and 928 see 3d4h
                index 51h bits 2-3. For the 864/964 see index 6Ah.
            4   Lock Vertical Timing Registers (LOCK VTMG). Locks 3d4h index 6, 7
                (bits 0,2,3,5,7), 9 bit 5, 10h, 11h bits 0-3, 15h, 16h if set
            5   Lock Horizontal Timing Registers (LOCK HTMG). Locks 3d4h index
                0,1,2,3,4,5,17h bit 2 if set
            6   (911/924) Lock VSync Polarity.
            7   (911/924) Lock HSync Polarity.
        */
    case 0x38:  /* CR38 Register Lock 1 */
        vga.s3.reg_lock1=(Bit8u)val;
        break;
    case 0x39:  /* CR39 Register Lock 2 */
        vga.s3.reg_lock2=(Bit8u)val;
        break;
    case 0x3a:
        vga.s3.reg_3a = (Bit8u)val;
        break;
    case 0x40:  /* CR40 System Config */
        vga.s3.reg_40 = (Bit8u)val;
        break;
    case 0x41:  /* CR41 BIOS flags */
        vga.s3.reg_41 = (Bit8u)val;
        break;
    case 0x42:  /* CR42 Mode Control */
        if ((val ^ vga.s3.reg_42) & 0x20) {
            vga.s3.reg_42= (Bit8u)val;
            VGA_StartResize();
        } else vga.s3.reg_42= (Bit8u)val;
        /*
        3d4h index 42h (R/W):  CR42 Mode Control
        bit  0-3  DCLK Select. These bits are effective when the VGA Clock Select
                  (3C2h/3CCh bit 2-3) is 3.
               5  Interlaced Mode if set.
       */
        break;
    case 0x43:  /* CR43 Extended Mode */
        vga.s3.reg_43= (Bit8u)val & ~0x4u;
        if ((((Bit8u)val & 0x4u) ^ (vga.config.scan_len >> 6u)) & 0x4u) {
            vga.config.scan_len&=0x2ffu;
            vga.config.scan_len|=((Bit8u)val & 0x4u) << 6u;
            VGA_CheckScanLength();
        }
        break;
        /*
            2  Logical Screen Width bit 8. Bit 8 of the Display Offset Register/
            (3d4h index 13h). (801/5,928) Only active if 3d4h index 51h bits 4-5
            are 0
        */
    case 0x45:  /* Hardware cursor mode */
        vga.s3.hgc.curmode = (Bit8u)val;
        // Activate hardware cursor code if needed
        VGA_ActivateHardwareCursor();
        break;
    case 0x46:
        vga.s3.hgc.originx = (vga.s3.hgc.originx & 0x00ff) | ((Bit8u)val << 8u);
        break;
    case 0x47:  /*  HGC orgX */
        vga.s3.hgc.originx = (vga.s3.hgc.originx & 0xff00) | (Bit8u)val;
        break;
    case 0x48:
        vga.s3.hgc.originy = (vga.s3.hgc.originy & 0x00ff) | ((Bit8u)val << 8u);
        break;
    case 0x49:  /*  HGC orgY */
        vga.s3.hgc.originy = (vga.s3.hgc.originy & 0xff00) | (Bit8u)val;
        break;
    case 0x4A:  /* HGC foreground stack */
        if (vga.s3.hgc.fstackpos > 2) vga.s3.hgc.fstackpos = 0;
        vga.s3.hgc.forestack[vga.s3.hgc.fstackpos] = (Bit8u)val;
        vga.s3.hgc.fstackpos++;
        break;
    case 0x4B:  /* HGC background stack */
        if (vga.s3.hgc.bstackpos > 2) vga.s3.hgc.bstackpos = 0;
        vga.s3.hgc.backstack[vga.s3.hgc.bstackpos] = (Bit8u)val;
        vga.s3.hgc.bstackpos++;
        break;
    case 0x4c:  /* HGC start address high byte*/
        vga.s3.hgc.startaddr &=0xff;
        vga.s3.hgc.startaddr |= (((Bit8u)val & 0xf) << 8);
        if ((((Bitu)vga.s3.hgc.startaddr)<<10)+((64*64*2)/8) > vga.mem.memsize) {
            vga.s3.hgc.startaddr &= 0xff;   // put it back to some sane area;
                                            // if read back of this address is ever implemented this needs to change
            LOG(LOG_VGAMISC,LOG_NORMAL)("VGA:S3:CRTC: HGC pattern address beyond video memory" );
        }
        break;
    case 0x4d:  /* HGC start address low byte*/
        vga.s3.hgc.startaddr &=0xff00;
        vga.s3.hgc.startaddr |= ((Bit8u)val & 0xff);
        break;
    case 0x4e:  /* HGC pattern start X */
        vga.s3.hgc.posx = (Bit8u)val & 0x3f;   // bits 0-5
        break;
    case 0x4f:  /* HGC pattern start Y */
        vga.s3.hgc.posy = (Bit8u)val & 0x3f;   // bits 0-5
        break;
    case 0x50:  // Extended System Control 1
        vga.s3.reg_50 = (Bit8u)val;
        switch (val & S3_XGA_CMASK) {
            case S3_XGA_32BPP: vga.s3.xga_color_mode = M_LIN32; break;
            case S3_XGA_16BPP: vga.s3.xga_color_mode = M_LIN16; break;
            case S3_XGA_8BPP: vga.s3.xga_color_mode = M_LIN8; break;
        }
        switch (val & S3_XGA_WMASK) {
            case S3_XGA_1024: vga.s3.xga_screen_width = 1024; break;
            case S3_XGA_1152: vga.s3.xga_screen_width = 1152; break;
            case S3_XGA_640:  vga.s3.xga_screen_width = 640; break;
            case S3_XGA_800:  vga.s3.xga_screen_width = 800; break;
            case S3_XGA_1280: vga.s3.xga_screen_width = 1280; break;
            case S3_XGA_1600: vga.s3.xga_screen_width = 1600; break;
            default:  vga.s3.xga_screen_width = 1024; break;
        }
        break;
    case 0x51:  /* Extended System Control 2 */
        vga.s3.reg_51= (Bit8u)val & 0xc0;       //Only store bits 6,7
        vga.config.display_start&=0xF3FFFF;
        vga.config.display_start|=((Bit8u)val & 3u) << 18u;
        if ((vga.svga.bank_read&0x30u) ^ (((Bit8u)val&0xcu)<<2u)) {
            vga.svga.bank_read&=0xcfu;
            vga.svga.bank_read|=((Bit8u)val&0xcu)<<2u;
            vga.svga.bank_write = vga.svga.bank_read;
            VGA_SetupHandlers();
        }
        if (((val & 0x30u) ^ (vga.config.scan_len >> 4u)) & 0x30u) {
            vga.config.scan_len&=0xffu;
            vga.config.scan_len|=((Bit8u)val & 0x30u) << 4u;
            VGA_CheckScanLength();
        }
        break;
        /*
            0   (80x) Display Start Address bit 18
            0-1 (928 +) Display Start Address bit 18-19
                Bits 16-17 are in index 31h bits 4-5, Bits 0-15 are in 3d4h index
                0Ch,0Dh. For the 864/964 see 3d4h index 69h
            2   (80x) CPU BASE. CPU Base Address Bit 18.
            2-3 (928 +) Old CPU Base Address Bits 19-18.
                64K Bank register bits 4-5. Bits 0-3 are in 3d4h index 35h.
                For the 864/964 see 3d4h index 6Ah
            4-5 Logical Screen Width Bit [8-9]. Bits 8-9 of the CRTC Offset register
                (3d4h index 13h). If this field is 0, 3d4h index 43h bit 2 is active
            6   (928,964) DIS SPXF. Disable Split Transfers if set. Spilt Transfers
                allows transferring one half of the VRAM shift register data while
                the other half is being output. For the 964 Split Transfers
                must be enabled in enhanced modes (4AE8h bit 0 set). Guess: They
                probably can't time the VRAM load cycle closely enough while the
                graphics engine is running.
            7   (not 864/964) Enable EPROM Write. If set enables flash memory write
                control to the BIOS ROM address
        */
    case 0x52:  // Extended System Control 1
        vga.s3.reg_52 = (Bit8u)val;
        break;
    case 0x53:
        // Map or unmap MMIO
        // bit 4 = MMIO at A0000
        // bit 3 = MMIO at LFB + 16M (should be fine if its always enabled for now)
        if(vga.s3.ext_mem_ctrl != (Bit8u)val) {
            vga.s3.ext_mem_ctrl = (Bit8u)val;
            VGA_SetupHandlers();
        }
        break;
    case 0x55:  /* Extended Video DAC Control */
        vga.s3.reg_55=(Bit8u)val;
        break;
        /*
            0-1 DAC Register Select Bits. Passed to the RS2 and RS3 pins on the
                RAMDAC, allowing access to all 8 or 16 registers on advanced RAMDACs.
                If this field is 0, 3d4h index 43h bit 1 is active.
            2   Enable General Input Port Read. If set DAC reads are disabled and the
                STRD strobe for reading the General Input Port is enabled for reading
                while DACRD is active, if clear DAC reads are enabled.
            3   (928) Enable External SID Operation if set. If set video data is
                passed directly from the VRAMs to the DAC rather than through the
                VGA chip
            4   Hardware Cursor MS/X11 Mode. If set the Hardware Cursor is in X11
                mode, if clear in MS-Windows mode
            5   (80x,928) Hardware Cursor External Operation Mode. If set the two
                bits of cursor data ,is output on the HC[0-1] pins for the video DAC
                The SENS pin becomes HC1 and the MID2 pin becomes HC0.
            6   ??
            7   (80x,928) Disable PA Output. If set PA[0-7] and VCLK are tristated.
                (864/964) TOFF VCLK. Tri-State Off VCLK Output. VCLK output tri
                -stated if set
        */
    case 0x58:  /* Linear Address Window Control */
        vga.s3.reg_58=(Bit8u)val;
        VGA_StartUpdateLFB();
        break;
        /*
            0-1 Linear Address Window Size. Must be less than or equal to video
                memory size. 0: 64K, 1: 1MB, 2: 2MB, 3: 4MB (928)/8Mb (864/964)
            2   (not 864/964) Enable Read Ahead Cache if set
            3   (80x,928) ISA Latch Address. If set latches address during every ISA
                cycle, unlatches during every ISA cycle if clear.
                (864/964) LAT DEL. Address Latch Delay Control (VL-Bus only). If set
                address latching occours in the T1 cycle, if clear in the T2 cycle
                (I.e. one clock cycle delayed).
            4   ENB LA. Enable Linear Addressing if set.
            5   (not 864/964) Limit Entry Depth for Write-Post. If set limits Write
                -Post Entry Depth to avoid ISA bus timeout due to wait cycle limit.
            6   (928,964) Serial Access Mode (SAM) 256 Words Control. If set SAM
                control is 256 words, if clear 512 words.
            7   (928) RAS 6-MCLK. If set the random read/write cycle time is 6MCLKs,
                if clear 7MCLKs
        */
    case 0x59:  /* Linear Address Window Position High */
        if ((vga.s3.la_window&0xff00) ^ ((Bit8u)val << 8)) {
            vga.s3.la_window=(vga.s3.la_window&0x00ff) | ((Bit8u)val << 8);
            VGA_StartUpdateLFB();
        }
        break;
    case 0x5a:  /* Linear Address Window Position Low */
        if ((vga.s3.la_window&0x00ff) ^ (Bit8u)val) {
            vga.s3.la_window=(vga.s3.la_window&0xff00) | (Bit8u)val;
            VGA_StartUpdateLFB();
        }
        break;
    case 0x5D:  /* Extended Horizontal Overflow */
        if ((val ^ vga.s3.ex_hor_overflow) & 3) {
            vga.s3.ex_hor_overflow=(Bit8u)val;
            VGA_StartResize();
        } else vga.s3.ex_hor_overflow=(Bit8u)val;
        break;
        /*
            0   Horizontal Total bit 8. Bit 8 of the Horizontal Total register (3d4h
                index 0)
            1   Horizontal Display End bit 8. Bit 8 of the Horizontal Display End
                register (3d4h index 1)
            2   Start Horizontal Blank bit 8. Bit 8 of the Horizontal Start Blanking
                register (3d4h index 2).
            3   (864,964) EHB+64. End Horizontal Blank +64. If set the /BLANK pulse
                is extended by 64 DCLKs. Note: Is this bit 6 of 3d4h index 3 or
                does it really extend by 64 ?
            4   Start Horizontal Sync Position bit 8. Bit 8 of the Horizontal Start
                Retrace register (3d4h index 4).
            5   (864,964) EHS+32. End Horizontal Sync +32. If set the HSYNC pulse
                is extended by 32 DCLKs. Note: Is this bit 5 of 3d4h index 5 or
                does it really extend by 32 ?
            6   (928,964) Data Transfer Position bit 8. Bit 8 of the Data Transfer
                Position register (3d4h index 3Bh)
            7   (928,964) Bus-Grant Terminate Position bit 8. Bit 8 of the Bus Grant
                Termination register (3d4h index 5Fh).
        */
    case 0x5e:  /* Extended Vertical Overflow */
        vga.config.line_compare=(vga.config.line_compare & 0x3ffu) | ((Bit8u)val & 0x40u) << 4u;
        if ((val ^ vga.s3.ex_ver_overflow) & 0x3u) {
            vga.s3.ex_ver_overflow=(Bit8u)val;
            VGA_StartResize();
        } else vga.s3.ex_ver_overflow=(Bit8u)val;
        break;
        /*
            0   Vertical Total bit 10. Bit 10 of the Vertical Total register (3d4h
                index 6). Bits 8 and 9 are in 3d4h index 7 bit 0 and 5.
            1   Vertical Display End bit 10. Bit 10 of the Vertical Display End
                register (3d4h index 12h). Bits 8 and 9 are in 3d4h index 7 bit 1
                and 6
            2   Start Vertical Blank bit 10. Bit 10 of the Vertical Start Blanking
                register (3d4h index 15h). Bit 8 is in 3d4h index 7 bit 3 and bit 9
                in 3d4h index 9 bit 5
            4   Vertical Retrace Start bit 10. Bit 10 of the Vertical Start Retrace
                register (3d4h index 10h). Bits 8 and 9 are in 3d4h index 7 bit 2
                and 7.
            6   Line Compare Position bit 10. Bit 10 of the Line Compare register
                (3d4h index 18h). Bit 8 is in 3d4h index 7 bit 4 and bit 9 in 3d4h
                index 9 bit 6.
        */
    case 0x67:  /* Extended Miscellaneous Control 2 */
        /*
            0   VCLK PHS. VCLK Phase With Respect to DCLK. If clear VLKC is inverted
                DCLK, if set VCLK = DCLK.
            2-3 (Trio64V+) streams mode
                    00 disable Streams Processor
                    01 overlay secondary stream on VGA-mode background
                    10 reserved
                    11 full Streams Processor operation
            4-7 Pixel format.
                    0  Mode  0: 8bit (1 pixel/VCLK)
                    1  Mode  8: 8bit (2 pixels/VCLK)
                    3  Mode  9: 15bit (1 pixel/VCLK)
                    5  Mode 10: 16bit (1 pixel/VCLK)
                    7  Mode 11: 24/32bit (2 VCLKs/pixel)
                    13  (732/764) 32bit (1 pixel/VCLK)
        */
        vga.s3.misc_control_2=(Bit8u)val;
        VGA_DetermineMode();
        break;
    case 0x69:  /* Extended System Control 3 */
        if (((vga.config.display_start & 0x1f0000u)>>16u) ^ (val & 0x1fu)) {
            vga.config.display_start&=0xffffu;
            vga.config.display_start|=((Bit8u)val & 0x1fu) << 16u;
        }
        break;
    case 0x6a:  /* Extended System Control 4 */
        vga.svga.bank_read=(Bit8u)val & 0x7f;
        vga.svga.bank_write = vga.svga.bank_read;
        VGA_SetupHandlers();
        break;
    case 0x6b:  // BIOS scratchpad: LFB address
        vga.s3.reg_6b=(Bit8u)val;
        break;
    default:
        LOG(LOG_VGAMISC,LOG_NORMAL)("VGA:S3:CRTC:Write to illegal index %2X", (int)reg );
        break;
    }
}

Bitu SVGA_S3_ReadCRTC( Bitu reg, Bitu iolen) {
    (void)iolen;//UNUSED
    switch (reg) {
    case 0x24:  /* attribute controller index (read only) */
    case 0x26:
        return ((vga.attr.disabled & 1u)?0x00u:0x20u) | (vga.attr.index & 0x1fu);
    case 0x2d:  /* Extended Chip ID (high byte of PCI device ID) */
        return 0x88;
    case 0x2e:  /* New Chip ID  (low byte of PCI device ID) */
        return 0x11;    // Trio64   
    case 0x2f:  /* Revision */
        return 0x00;    // Trio64 (exact value?)
//      return 0x44;    // Trio64 V+
    case 0x30:  /* CR30 Chip ID/REV register */
        return 0xe1;    // Trio+ dual byte
    case 0x31:  /* CR31 Memory Configuration */
//TODO mix in bits from baseaddress;
        return  vga.s3.reg_31;  
    case 0x35:  /* CR35 CRT Register Lock */
        return vga.s3.reg_35|(vga.svga.bank_read & 0xfu);
    case 0x36: /* CR36 Reset State Read 1 */
        return vga.s3.reg_36;
    case 0x37: /* Reset state read 2 */
        return 0x2b;
    case 0x38: /* CR38 Register Lock 1 */
        return vga.s3.reg_lock1;
    case 0x39: /* CR39 Register Lock 2 */
        return vga.s3.reg_lock2;
    case 0x3a:
        return vga.s3.reg_3a;
    case 0x40: /* CR40 system config */
        return vga.s3.reg_40;
    case 0x41: /* CR40 system config */
        return vga.s3.reg_41;
    case 0x42: // not interlaced
        return 0x0d;
    case 0x43:  /* CR43 Extended Mode */
        return vga.s3.reg_43|((vga.config.scan_len>>6)&0x4);
    case 0x45:  /* Hardware cursor mode */
        vga.s3.hgc.bstackpos = 0;
        vga.s3.hgc.fstackpos = 0;
        return vga.s3.hgc.curmode|0xa0u;
    case 0x46:
        return (unsigned int)vga.s3.hgc.originx>>8u;
    case 0x47:  /*  HGC orgX */
        return vga.s3.hgc.originx&0xffu;
    case 0x48:
        return (unsigned int)vga.s3.hgc.originy>>8u;
    case 0x49:  /*  HGC orgY */
        return vga.s3.hgc.originy&0xffu;
    case 0x4A:  /* HGC foreground stack */
        return vga.s3.hgc.forestack[vga.s3.hgc.fstackpos];
    case 0x4B:  /* HGC background stack */
        return vga.s3.hgc.backstack[vga.s3.hgc.bstackpos];
    case 0x50:  // CR50 Extended System Control 1
        return vga.s3.reg_50;
    case 0x51:  /* Extended System Control 2 */
        return ((vga.config.display_start >> 16u) & 3u) |
                ((vga.svga.bank_read & 0x30u) >> 2u) |
                ((vga.config.scan_len & 0x300u) >> 4u) |
                vga.s3.reg_51;
    case 0x52:  // CR52 Extended BIOS flags 1
        return vga.s3.reg_52;
    case 0x53:
        return vga.s3.ext_mem_ctrl;
    case 0x55:  /* Extended Video DAC Control */
        return vga.s3.reg_55;
    case 0x58:  /* Linear Address Window Control */
        return  vga.s3.reg_58;
    case 0x59:  /* Linear Address Window Position High */
        return ((unsigned int)vga.s3.la_window >> 8u);
    case 0x5a:  /* Linear Address Window Position Low */
        return (vga.s3.la_window & 0xff);
    case 0x5D:  /* Extended Horizontal Overflow */
        return vga.s3.ex_hor_overflow;
    case 0x5e:  /* Extended Vertical Overflow */
        return vga.s3.ex_ver_overflow;
    case 0x67:  /* Extended Miscellaneous Control 2 */      
        return vga.s3.misc_control_2;
    case 0x69:  /* Extended System Control 3 */
        return (Bit8u)((vga.config.display_start & 0x1f0000)>>16); 
    case 0x6a:  /* Extended System Control 4 */
        return (Bit8u)(vga.svga.bank_read & 0x7f);
    case 0x6b:  // BIOS scatchpad: LFB address
        return vga.s3.reg_6b; 
    default:
        return 0x00;
    }
}

void SVGA_S3_WriteSEQ(Bitu reg,Bitu val,Bitu iolen) {
    (void)iolen;//UNUSED
    if (reg>0x8 && vga.s3.pll.lock!=0x6) return;
    switch (reg) {
    case 0x08:
        vga.s3.pll.lock=(Bit8u)val;
        break;
    case 0x10:      /* Memory PLL Data Low */
        vga.s3.mclk.n=(Bit8u)val & 0x1f;
        vga.s3.mclk.r=(Bit8u)val >> 5;
        break;
    case 0x11:      /* Memory PLL Data High */
        vga.s3.mclk.m=(Bit8u)val & 0x7f;
        break;
    case 0x12:      /* Video PLL Data Low */
        vga.s3.clk[3].n=(Bit8u)val & 0x1f;
        vga.s3.clk[3].r=(Bit8u)val >> 5;
        break;
    case 0x13:      /* Video PLL Data High */
        vga.s3.clk[3].m=(Bit8u)val & 0x7f;
        break;
    case 0x15:
        vga.s3.pll.cmd=(Bit8u)val;
        VGA_StartResize();
        break;
    default:
        LOG(LOG_VGAMISC,LOG_NORMAL)("VGA:S3:SEQ:Write to illegal index %2X", (int)reg );
        break;
    }
}

// to make the S3 Trio64 BIOS work
const Bit8u reg17ret[] ={0x7b, 0xc0, 0x0, 0xda};
Bit8u reg17index=0;

Bitu SVGA_S3_ReadSEQ(Bitu reg,Bitu iolen) {
    (void)iolen;//UNUSED
    /* S3 specific group */
    if (reg>0x8 && vga.s3.pll.lock!=0x6) {
        if (reg<0x1b) return 0;
        else return reg;
    }
    switch (reg) {
    case 0x08:      /* PLL Unlock */
        return vga.s3.pll.lock;
    case 0x10:      /* Memory PLL Data Low */
        return vga.s3.mclk.n || ((vga.s3.mclk.r << 5) != 0); /* FIXME: What is this testing exactly? */
    case 0x11:      /* Memory PLL Data High */
        return vga.s3.mclk.m;
    case 0x12:      /* Video PLL Data Low */
        return vga.s3.clk[3].n || ((vga.s3.clk[3].r << 5) != 0); /* FIXME: What is this testing exactly? */
    case 0x13:      /* Video Data High */
        return vga.s3.clk[3].m;
    case 0x15:
        return vga.s3.pll.cmd;
    case 0x17: {
            Bit8u retval = reg17ret[reg17index];
            reg17index++;
            if(reg17index>3)reg17index=0;
            return retval;
        }
    default:
        LOG(LOG_VGAMISC,LOG_NORMAL)("VGA:S3:SEQ:Read from illegal index %2X", (int)reg);
        return 0;
    }
}

Bitu SVGA_S3_GetClock(void) {
    Bitu clock = (vga.misc_output >> 2) & 3;
    if (clock == 0)
        clock = 25175000;
    else if (clock == 1)
        clock = 28322000;
    else 
        clock=1000ul * S3_CLOCK(vga.s3.clk[clock].m,vga.s3.clk[clock].n,vga.s3.clk[clock].r);
    /* Check for dual transfer, master clock/2 */
    if (vga.s3.pll.cmd & 0x10) clock/=2;
    return clock;
}

bool SVGA_S3_HWCursorActive(void) {
    return (vga.s3.hgc.curmode & 0x1) != 0;
}

bool SVGA_S3_AcceptsMode(Bitu mode) {
    return VideoModeMemSize(mode) < vga.mem.memsize;
}

void SVGA_Setup_S3Trio(void) {
    svga.write_p3d5 = &SVGA_S3_WriteCRTC;
    svga.read_p3d5 = &SVGA_S3_ReadCRTC;
    svga.write_p3c5 = &SVGA_S3_WriteSEQ;
    svga.read_p3c5 = &SVGA_S3_ReadSEQ;
    svga.write_p3c0 = 0; /* no S3-specific functionality */
    svga.read_p3c1 = 0; /* no S3-specific functionality */

    svga.set_video_mode = 0; /* implemented in core */
    svga.determine_mode = 0; /* implemented in core */
    svga.set_clock = 0; /* implemented in core */
    svga.get_clock = &SVGA_S3_GetClock;
    svga.hardware_cursor_active = &SVGA_S3_HWCursorActive;
    svga.accepts_mode = &SVGA_S3_AcceptsMode;

    if (vga.mem.memsize == 0)
        vga.mem.memsize = 2*1024*1024; // the most common S3 configuration

    // Set CRTC 36 to specify amount of VRAM and PCI
    if (vga.mem.memsize < 1024*1024) {
        vga.mem.memsize = 512*1024;
        vga.s3.reg_36 = 0xfa;       // less than 1mb fast page mode
    } else if (vga.mem.memsize < 2048*1024)    {
        vga.mem.memsize = 1024*1024;
        vga.s3.reg_36 = 0xda;       // 1mb fast page mode
    } else if (vga.mem.memsize < 3072*1024)    {
        vga.mem.memsize = 2048*1024;
        vga.s3.reg_36 = 0x9a;       // 2mb fast page mode
    } else if (vga.mem.memsize < 4096*1024)    {
        vga.mem.memsize = 3072*1024;
        vga.s3.reg_36 = 0x5a;       // 3mb fast page mode
    } else if (vga.mem.memsize < 8192*1024) {  // Trio64 supported only up to 4M
        vga.mem.memsize = 4096*1024;
        vga.s3.reg_36 = 0x1a;       // 4mb fast page mode
    } else if (vga.mem.memsize < 16384*1024) {  // 8M
        vga.mem.memsize = 8192*1024;
        vga.s3.reg_36 = 0x7a;       // 8mb fast page mode
    } else {    // HACK: 16MB mode, with value not supported by actual hardware
        vga.mem.memsize = 16384*1024; // FIXME: This breaks the cursor in Windows 3.1, though Windows 95 has no problem with it
        vga.s3.reg_36 = 0x7a;       // 8mb fast page mode
    }

    // S3 ROM signature
    phys_writes(PhysMake(0xc000,0)+0x003f, "S3 86C764", 10);

    PCI_AddSVGAS3_Device();
}

