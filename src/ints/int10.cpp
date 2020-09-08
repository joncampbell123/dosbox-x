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


#include "dosbox.h"
#include "control.h"
#include "mem.h"
#include "callback.h"
#include "regs.h"
#include "inout.h"
#include "int10.h"
#include "mouse.h"
#include "setup.h"

Int10Data int10;
static Bitu call_10 = 0;
static bool warned_ff=false;

extern bool enable_vga_8bit_dac;
extern bool vga_8bit_dac;

Bitu INT10_Handler(void) {
	// NTS: We do have to check the "current video mode" from the BIOS data area every call.
	//      Some OSes like Windows 95 rely on overwriting the "current video mode" byte in
	//      the BIOS data area to play tricks with the BIOS. If we don't call this, tricks
	//      like the Windows 95 boot logo or INT 10h virtualization in Windows 3.1/9x/ME
	//      within the DOS "box" will not work properly.
	INT10_SetCurMode();

	switch (reg_ah) {
	case 0x00:								/* Set VideoMode */
		Mouse_BeforeNewVideoMode(true);
		INT10_SetVideoMode(reg_al);
		Mouse_AfterNewVideoMode(true);
		break;
	case 0x01:								/* Set TextMode Cursor Shape */
		INT10_SetCursorShape(reg_ch,reg_cl);
		break;
	case 0x02:								/* Set Cursor Pos */
		INT10_SetCursorPos(reg_dh,reg_dl,reg_bh);
		break;
	case 0x03:								/* get Cursor Pos and Cursor Shape*/
//		reg_ah=0;
		reg_dl=CURSOR_POS_COL(reg_bh);
		reg_dh=CURSOR_POS_ROW(reg_bh);
		reg_cx=real_readw(BIOSMEM_SEG,BIOSMEM_CURSOR_TYPE);
		break;
	case 0x04:								/* read light pen pos YEAH RIGHT */
		/* Light pen is not supported */
		reg_ax=0;
		break;
	case 0x05:								/* Set Active Page */
		if ((reg_al & 0x80) && IS_TANDY_ARCH) {
			Bit8u crtcpu=real_readb(BIOSMEM_SEG, BIOSMEM_CRTCPU_PAGE);		
			switch (reg_al) {
			case 0x80:
				reg_bh=crtcpu & 7;
				reg_bl=(crtcpu >> 3) & 0x7;
				break;
			case 0x81:
				crtcpu=(crtcpu & 0xc7) | ((reg_bl & 7) << 3);
				break;
			case 0x82:
				crtcpu=(crtcpu & 0xf8) | (reg_bh & 7);
				break;
			case 0x83:
				crtcpu=(crtcpu & 0xc0) | (reg_bh & 7) | ((reg_bl & 7) << 3);
				break;
			}
			if (machine==MCH_PCJR) {
				/* always return graphics mapping, even for invalid values of AL */
				reg_bh=crtcpu & 7;
				reg_bl=(crtcpu >> 3) & 0x7;
			}
			IO_WriteB(0x3df,crtcpu);
			real_writeb(BIOSMEM_SEG, BIOSMEM_CRTCPU_PAGE,crtcpu);
		}
		else INT10_SetActivePage(reg_al);
		break;	
	case 0x06:								/* Scroll Up */
		INT10_ScrollWindow(reg_ch,reg_cl,reg_dh,reg_dl,-(Bit8s)reg_al,reg_bh,0xFF);
		break;
	case 0x07:								/* Scroll Down */
		INT10_ScrollWindow(reg_ch,reg_cl,reg_dh,reg_dl,(Bit8s)reg_al,reg_bh,0xFF);
		break;
	case 0x08:								/* Read character & attribute at cursor */
		INT10_ReadCharAttr(&reg_ax,reg_bh);
		break;						
	case 0x09:								/* Write Character & Attribute at cursor CX times */
		if (real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_MODE)==0x11)
			INT10_WriteChar(reg_al,(reg_bl&0x80)|0x3f,reg_bh,reg_cx,true);
		else INT10_WriteChar(reg_al,reg_bl,reg_bh,reg_cx,true);
		break;
	case 0x0A:								/* Write Character at cursor CX times */
		INT10_WriteChar(reg_al,reg_bl,reg_bh,reg_cx,false);
		break;
	case 0x0B:								/* Set Background/Border Colour & Set Palette*/
		switch (reg_bh) {
		case 0x00:		//Background/Border color
			INT10_SetBackgroundBorder(reg_bl);
			break;
		case 0x01:		//Set color Select
		default:
			INT10_SetColorSelect(reg_bl);
			break;
		}
		break;
	case 0x0C:								/* Write Graphics Pixel */
		INT10_PutPixel(reg_cx,reg_dx,reg_bh,reg_al);
		break;
	case 0x0D:								/* Read Graphics Pixel */
		INT10_GetPixel(reg_cx,reg_dx,reg_bh,&reg_al);
		break;
	case 0x0E:								/* Teletype OutPut */
		INT10_TeletypeOutput(reg_al,reg_bl);
		break;
	case 0x0F:								/* Get videomode */
		reg_bh=real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE);
		reg_al=real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_MODE)|(real_readb(BIOSMEM_SEG,BIOSMEM_VIDEO_CTL)&0x80);
		reg_ah=(Bit8u)real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS);
		break;					
	case 0x10:								/* Palette functions */
        if (machine==MCH_MCGA) {
            if (!(reg_al == 0x10 || reg_al == 0x12 || reg_al == 0x15 || reg_al == 0x17 || reg_al == 0x18 || reg_al == 0x19))
                break;
        }
        else if (machine==MCH_PCJR) {
            if (reg_al>0x02) /* "Looking at the PCjr tech ref page A-61, ... the BIOS listing stops at subfunction 2." */
                break;
        }
        else {
            if (!IS_EGAVGA_ARCH && (reg_al>0x02)) break;
            else if (!IS_VGA_ARCH && (reg_al>0x03)) break;
        }

		switch (reg_al) {
		case 0x00:							/* SET SINGLE PALETTE REGISTER */
			INT10_SetSinglePaletteRegister(reg_bl,reg_bh);
			break;
		case 0x01:							/* SET BORDER (OVERSCAN) COLOR*/
			INT10_SetOverscanBorderColor(reg_bh);
			break;
		case 0x02:							/* SET ALL PALETTE REGISTERS */
			INT10_SetAllPaletteRegisters(SegPhys(es)+reg_dx);
			break;
		case 0x03:							/* TOGGLE INTENSITY/BLINKING BIT */
			INT10_ToggleBlinkingBit(reg_bl);
			break;
		case 0x07:							/* GET SINGLE PALETTE REGISTER */
			INT10_GetSinglePaletteRegister(reg_bl,&reg_bh);
			break;
		case 0x08:							/* READ OVERSCAN (BORDER COLOR) REGISTER */
			INT10_GetOverscanBorderColor(&reg_bh);
			break;
		case 0x09:							/* READ ALL PALETTE REGISTERS AND OVERSCAN REGISTER */
			INT10_GetAllPaletteRegisters(SegPhys(es)+reg_dx);
			break;
		case 0x10:							/* SET INDIVIDUAL DAC REGISTER */
			INT10_SetSingleDACRegister(reg_bl,reg_dh,reg_ch,reg_cl);
			break;
		case 0x12:							/* SET BLOCK OF DAC REGISTERS */
			INT10_SetDACBlock(reg_bx,reg_cx,SegPhys(es)+reg_dx);
			break;
		case 0x13:							/* SELECT VIDEO DAC COLOR PAGE */
			INT10_SelectDACPage(reg_bl,reg_bh);
			break;
		case 0x15:							/* GET INDIVIDUAL DAC REGISTER */
			INT10_GetSingleDACRegister(reg_bl,&reg_dh,&reg_ch,&reg_cl);
			break;
		case 0x17:							/* GET BLOCK OF DAC REGISTER */
			INT10_GetDACBlock(reg_bx,reg_cx,SegPhys(es)+reg_dx);
			break;
		case 0x18:							/* undocumented - SET PEL MASK */
			INT10_SetPelMask(reg_bl);
			break;
		case 0x19:							/* undocumented - GET PEL MASK */
			INT10_GetPelMask(reg_bl);
			reg_bh=0;	// bx for get mask
			break;
		case 0x1A:							/* GET VIDEO DAC COLOR PAGE */
			INT10_GetDACPage(&reg_bl,&reg_bh);
			break;
		case 0x1B:							/* PERFORM GRAY-SCALE SUMMING */
			INT10_PerformGrayScaleSumming(reg_bx,reg_cx);
			break;
		case 0xF0: case 0xF1: case 0xF2: /* ET4000 Sierra HiColor DAC support */
			if (svgaCard == SVGA_TsengET4K && svga.int10_extensions) {
				svga.int10_extensions();
				break;
			}
		default:
			LOG(LOG_INT10,LOG_ERROR)("Function 10:Unhandled EGA/VGA Palette Function %2X",reg_al);
			break;
		}
		break;
	case 0x11:								/* Character generator functions */
        if (machine==MCH_MCGA) {
            if (!(reg_al == 0x24 || reg_al == 0x30))
                break;
        }
        else {
            if (!IS_EGAVGA_ARCH)
                break;
        }

		if ((reg_al&0xf0)==0x10) Mouse_BeforeNewVideoMode(false);
		switch (reg_al) {
/* Textmode calls */
		case 0x00:			/* Load user font */
		case 0x10:
			INT10_LoadFont(SegPhys(es)+reg_bp,reg_al==0x10,reg_cx,reg_dx,reg_bl&0x7f,reg_bh);
			break;
		case 0x01:			/* Load 8x14 font */
		case 0x11:
			INT10_LoadFont(Real2Phys(int10.rom.font_14),reg_al==0x11,256,0,reg_bl&0x7f,14);
			break;
		case 0x02:			/* Load 8x8 font */
		case 0x12:
			INT10_LoadFont(Real2Phys(int10.rom.font_8_first),reg_al==0x12,256,0,reg_bl&0x7f,8);
			break;
		case 0x03:			/* Set Block Specifier */
			IO_Write(0x3c4,0x3);IO_Write(0x3c5,reg_bl);
			break;
		case 0x04:			/* Load 8x16 font */
		case 0x14:
			if (!IS_VGA_ARCH) break;
			INT10_LoadFont(Real2Phys(int10.rom.font_16),reg_al==0x14,256,0,reg_bl&0x7f,16);
			break;
/* Graphics mode calls */
		case 0x20:			/* Set User 8x8 Graphics characters */
			RealSetVec(0x1f,RealMake(SegValue(es),reg_bp));
			break;
		case 0x21:			/* Set user graphics characters */
			RealSetVec(0x43,RealMake(SegValue(es),reg_bp));
			real_writew(BIOSMEM_SEG,BIOSMEM_CHAR_HEIGHT,reg_cx);
			goto graphics_chars;
		case 0x22:			/* Rom 8x14 set */
			RealSetVec(0x43,int10.rom.font_14);
			real_writew(BIOSMEM_SEG,BIOSMEM_CHAR_HEIGHT,14);
			goto graphics_chars;
		case 0x23:			/* Rom 8x8 double dot set */
			RealSetVec(0x43,int10.rom.font_8_first);
			real_writew(BIOSMEM_SEG,BIOSMEM_CHAR_HEIGHT,8);
			goto graphics_chars;
		case 0x24:			/* Rom 8x16 set */
			if (!IS_VGA_ARCH) break;
			RealSetVec(0x43,int10.rom.font_16);
			real_writew(BIOSMEM_SEG,BIOSMEM_CHAR_HEIGHT,16);
			goto graphics_chars;
graphics_chars:
			switch (reg_bl) {
			case 0x00:real_writeb(BIOSMEM_SEG,BIOSMEM_NB_ROWS,reg_dl-1);break;
			case 0x01:real_writeb(BIOSMEM_SEG,BIOSMEM_NB_ROWS,13);break;
			case 0x03:real_writeb(BIOSMEM_SEG,BIOSMEM_NB_ROWS,42);break;
			case 0x02:
			default:real_writeb(BIOSMEM_SEG,BIOSMEM_NB_ROWS,24);break;
			}
			break;
/* General */
		case 0x30:/* Get Font Information */
			switch (reg_bh) {
			case 0x00:	/* interupt 0x1f vector */
				{
					RealPt int_1f=RealGetVec(0x1f);
					SegSet16(es,RealSeg(int_1f));
					reg_bp=RealOff(int_1f);
				}
				break;
			case 0x01:	/* interupt 0x43 vector */
				{
					RealPt int_43=RealGetVec(0x43);
					SegSet16(es,RealSeg(int_43));
					reg_bp=RealOff(int_43);
				}
				break;
			case 0x02:	/* font 8x14 */
				SegSet16(es,RealSeg(int10.rom.font_14));
				reg_bp=RealOff(int10.rom.font_14);
				break;
			case 0x03:	/* font 8x8 first 128 */
				SegSet16(es,RealSeg(int10.rom.font_8_first));
				reg_bp=RealOff(int10.rom.font_8_first);
				break;
			case 0x04:	/* font 8x8 second 128 */
				SegSet16(es,RealSeg(int10.rom.font_8_second));
				reg_bp=RealOff(int10.rom.font_8_second);
				break;
			case 0x05:	/* alpha alternate 9x14 */
				SegSet16(es,RealSeg(int10.rom.font_14_alternate));
				reg_bp=RealOff(int10.rom.font_14_alternate);
				break;
			case 0x06:	/* font 8x16 */
				if (!IS_VGA_ARCH) break;
				SegSet16(es,RealSeg(int10.rom.font_16));
				reg_bp=RealOff(int10.rom.font_16);
				break;
			case 0x07:	/* alpha alternate 9x16 */
				if (!IS_VGA_ARCH) break;
				SegSet16(es,RealSeg(int10.rom.font_16_alternate));
				reg_bp=RealOff(int10.rom.font_16_alternate);
				break;
			default:
				LOG(LOG_INT10,LOG_ERROR)("Function 11:30 Request for font %2X",reg_bh);	
				break;
			}
			if ((reg_bh<=7) || (svgaCard==SVGA_TsengET4K)) {
				reg_cx=real_readw(BIOSMEM_SEG,BIOSMEM_CHAR_HEIGHT);
				reg_dl=real_readb(BIOSMEM_SEG,BIOSMEM_NB_ROWS);
			}
			break;
		default:
			LOG(LOG_INT10,LOG_ERROR)("Function 11:Unsupported character generator call %2X",reg_al);
			break;
		}
		if ((reg_al&0xf0)==0x10) Mouse_AfterNewVideoMode(false);
		break;
	case 0x12:								/* alternate function select */
		if (!IS_EGAVGA_ARCH && machine != MCH_MCGA) 
			break;
		switch (reg_bl) {
		case 0x10:							/* Get EGA Information */
            if (machine != MCH_MCGA) {
                /* This call makes no sense on MCGA systems because it's designed to return EGA information.
                 *
                 * Furthermore the MS-DOS port of Thexder calls this function and validates the values
                 * returned from this call in a way that suggests MCGA BIOSes do not respond to this call:
                 *
                 * 0302:00000377 32C0                xor  al,al
                 * 0302:00000379 B310                mov  bl,10
                 * 0302:0000037B B7FF                mov  bh,FF
                 * 0302:0000037D B10F                mov  cl,0F
                 * 0302:0000037F B412                mov  ah,12
                 * 0302:00000381 CD10                int  10
                 * 0302:00000383 80F90C              cmp  cl,0C
                 * 0302:00000386 730D                jnc  00000395 ($+d)         (down)
                 * 0302:00000388 80FB03              cmp  bl,03
                 * 0302:0000038B 7708                ja   00000395 ($+8)         (no jmp)
                 * 0302:0000038D 0AFF                or   bh,bh
                 * 0302:0000038F 7504                jne  00000395 ($+4)         (no jmp)
                 * 0302:00000391 8D16CD02            lea  dx,[02CD]              ds:[02CD]=616D
                 * 0302:00000395 E82C00              call 000003C4 ($+2c)
                 *
                 * Basically all checks confirm the returned values are out of range and invalid for what
                 * the BIOS call would normally return for EGA. Either MCGA BIOS leaves them unchanged
                 * or changes them to some other value entirely.
                 *
                 * [see also: http://www.ctyme.com/intr/rb-0162.htm]
                 * [see also: https://github.com/joncampbell123/dosbox-x/issues/1207]
                 *
                 * TODO: What does actual MCGA hardware/BIOS do? CHECK */
                reg_bh=(real_readw(BIOSMEM_SEG,BIOSMEM_CRTC_ADDRESS)==0x3B4);
                if (IS_EGA_ARCH) {
                    if (vga.mem.memsize >= (256*1024))
                        reg_bl=3;	//256 kb
                    else if (vga.mem.memsize >= (192*1024))
                        reg_bl=2;	//192 kb
                    else if (vga.mem.memsize >= (128*1024))
                        reg_bl=1;	//128 kb
                    else
                        reg_bl=0;	//64 kb
                }
                else {
                    reg_bl=3;	//256 kb
                }
                reg_cl=real_readb(BIOSMEM_SEG,BIOSMEM_SWITCHES) & 0x0F;
                reg_ch=real_readb(BIOSMEM_SEG,BIOSMEM_SWITCHES) >> 4;
            }
			break;
		case 0x20:							/* Set alternate printscreen */
			break;
		case 0x30:							/* Select vertical resolution */
			{   
				if (!IS_VGA_ARCH) break;
				LOG(LOG_INT10,LOG_WARN)("Function 12:Call %2X (select vertical resolution)",reg_bl);
				if (svgaCard != SVGA_None) {
					if (reg_al > 2) {
						reg_al=0;		// invalid subfunction
						break;
					}
				}
				Bit8u modeset_ctl = real_readb(BIOSMEM_SEG,BIOSMEM_MODESET_CTL);
				Bit8u video_switches = real_readb(BIOSMEM_SEG,BIOSMEM_SWITCHES)&0xf0;
				switch(reg_al) {
				case 0: // 200
					modeset_ctl &= 0xef;
					modeset_ctl |= 0x80;
					video_switches |= 8;	// ega normal/cga emulation
					break;
				case 1: // 350
					modeset_ctl &= 0x6f;
					video_switches |= 9;	// ega enhanced
					break;
				case 2: // 400
					modeset_ctl &= 0x6f;
					modeset_ctl |= 0x10;	// use 400-line mode at next mode set
					video_switches |= 9;	// ega enhanced
					break;
				default:
					modeset_ctl &= 0xef;
					video_switches |= 8;	// ega normal/cga emulation
					break;
				}
				real_writeb(BIOSMEM_SEG,BIOSMEM_MODESET_CTL,modeset_ctl);
				real_writeb(BIOSMEM_SEG,BIOSMEM_SWITCHES,video_switches);
				reg_al=0x12;	// success
				break;
			}
		case 0x31:							/* Palette loading on modeset */
			{   
				if (!IS_VGA_ARCH) break;
				if (svgaCard==SVGA_TsengET4K) reg_al&=1;
				if (reg_al>1) {
					reg_al=0;		//invalid subfunction
					break;
				}
				Bit8u temp = real_readb(BIOSMEM_SEG,BIOSMEM_MODESET_CTL) & 0xf7;
				if (reg_al&1) temp|=8;		// enable if al=0
				real_writeb(BIOSMEM_SEG,BIOSMEM_MODESET_CTL,temp);
				reg_al=0x12;
				break;	
			}		
		case 0x32:							/* Video addressing */
			if (!IS_VGA_ARCH) break;
			LOG(LOG_INT10,LOG_ERROR)("Function 12:Call %2X not handled",reg_bl);
			if (svgaCard==SVGA_TsengET4K) reg_al&=1;
			if (reg_al>1) reg_al=0;		//invalid subfunction
			else reg_al=0x12;			//fake a success call
			break;
		case 0x33: /* SWITCH GRAY-SCALE SUMMING */
			{   
				if (!IS_VGA_ARCH) break;
				if (svgaCard==SVGA_TsengET4K) reg_al&=1;
				if (reg_al>1) {
					reg_al=0;
					break;
				}
				Bit8u temp = real_readb(BIOSMEM_SEG,BIOSMEM_MODESET_CTL) & 0xfd;
				if (!(reg_al&1)) temp|=2;		// enable if al=0
				real_writeb(BIOSMEM_SEG,BIOSMEM_MODESET_CTL,temp);
				reg_al=0x12;
				break;	
			}		
		case 0x34: /* ALTERNATE FUNCTION SELECT (VGA) - CURSOR EMULATION */
			{   
				// bit 0: 0=enable, 1=disable
				if (!IS_VGA_ARCH) break;
				if (svgaCard==SVGA_TsengET4K) reg_al&=1;
				if (reg_al>1) {
					reg_al=0;
					break;
				}
				Bit8u temp = real_readb(BIOSMEM_SEG,BIOSMEM_VIDEO_CTL) & 0xfe;
				real_writeb(BIOSMEM_SEG,BIOSMEM_VIDEO_CTL,temp|reg_al);
				reg_al=0x12;
				break;	
			}		
		case 0x35:
			if (!IS_VGA_ARCH) break;
			LOG(LOG_INT10,LOG_ERROR)("Function 12:Call %2X not handled",reg_bl);
			reg_al=0x12;
			break;
		case 0x36: {						/* VGA Refresh control */
			if (!IS_VGA_ARCH) break;
			if ((svgaCard==SVGA_S3Trio) && (reg_al>1)) {
				reg_al=0;
				break;
			}
			IO_Write(0x3c4,0x1);
			Bit8u clocking = IO_Read(0x3c5);
			
			if (reg_al==0) clocking &= ~0x20;
			else clocking |= 0x20;
			
			IO_Write(0x3c4,0x1);
			IO_Write(0x3c5,clocking);

			reg_al=0x12; // success
			break;
		}
#if 0 /* TODO: For Tseng ET4000 emulation. ET4000 W32p driver uses it. */
/*

   INT 10 - Tseng ET-4000 BIOS - GET/SET SCREEN REFRESH RATE

   AH = 12h
   BL = F1h
   AL = subfunction
   00h set refresh rate
   01h get refresh rate
   BH = video mode
   00h	 640x480
   01h	 800x600
   02h	 1024x768
   03h	 1280x1024
   CX = new refresh rate (see #00035) if AL = 00h
Return: AL = 12h if supported
CX = current rate (for AL=00h, a changed CX indicates failure)

Values for Tseng ET4000 refresh rate:
CX	640x480	800x600	  1024x768/1280x1024
00h	60 Hz	 56 Hz	   interlaced
01h	72 Hz	 60 Hz	   60 Hz
02h	75 Hz	 72 Hz	   70 Hz
03h	90 Hz	 75 Hz	   75 Hz
04h	--	 90 Hz	   --

 */
#endif
		default:
			LOG(LOG_INT10,LOG_ERROR)("Function 12:Call %2X not handled",reg_bl);
			if (machine!=MCH_EGA) reg_al=0;
			break;
		}
		break;
	case 0x13:								/* Write String */
		INT10_WriteString(reg_dh,reg_dl,reg_al,reg_bl,SegPhys(es)+reg_bp,reg_cx,reg_bh);
		break;
	case 0x1A:								/* Display Combination */
		if (!IS_VGA_ARCH && machine != MCH_MCGA) break;
		if (reg_al<2) {
			INT10_DisplayCombinationCode(&reg_bx,(reg_al==1));
			reg_ax=0x1A;	// high part destroyed or zeroed depending on BIOS
		}
		break;
	case 0x1B:								/* functionality State Information */
		if (!IS_VGA_ARCH) break;
		switch (reg_bx) {
		case 0x0000:
			INT10_GetFuncStateInformation(SegPhys(es)+reg_di);
			reg_al=0x1B;
			break;
		default:
			LOG(LOG_INT10,LOG_ERROR)("1B:Unhandled call BX %2X",reg_bx);
			reg_al=0;
			break;
		}
		break;
	case 0x1C:	/* Video Save Area */
		if (!IS_VGA_ARCH) break;
		switch (reg_al) {
			case 0: {
				Bitu ret=INT10_VideoState_GetSize(reg_cx);
				if (ret) {
					reg_al=0x1c;
					reg_bx=(Bit16u)ret;
				} else reg_al=0;
				}
				break;
			case 1:
				if (INT10_VideoState_Save(reg_cx,RealMake(SegValue(es),reg_bx))) reg_al=0x1c;
				else reg_al=0;
				break;
			case 2:
				if (INT10_VideoState_Restore(reg_cx,RealMake(SegValue(es),reg_bx))) reg_al=0x1c;
				else reg_al=0;
				break;
			default:
				if (svgaCard==SVGA_TsengET4K) reg_ax=0;
				else reg_al=0;
				break;
		}
		break;
	case 0x4f:								/* VESA Calls */
		if ((!IS_VGA_ARCH) || (svgaCard!=SVGA_S3Trio))
			break;
		switch (reg_al) {
		case 0x00:							/* Get SVGA Information */
			reg_al=0x4f;
			reg_ah=VESA_GetSVGAInformation(SegValue(es),reg_di);
			break;
		case 0x01:							/* Get SVGA Mode Information */
			reg_al=0x4f;
			reg_ah=VESA_GetSVGAModeInformation(reg_cx,SegValue(es),reg_di);
			break;
		case 0x02:							/* Set videomode */
			Mouse_BeforeNewVideoMode(true);
			reg_al=0x4f;
			reg_ah=VESA_SetSVGAMode(reg_bx);
			Mouse_AfterNewVideoMode(true);
			break;
		case 0x03:							/* Get videomode */
			reg_al=0x4f;
			reg_ah=VESA_GetSVGAMode(reg_bx);
			break;
		case 0x04:							/* Save/restore state */
			reg_al=0x4f;
			switch (reg_dl) {
				case 0: {
					Bitu ret=INT10_VideoState_GetSize(reg_cx);
					if (ret) {
						reg_ah=0;
						reg_bx=(Bit16u)ret;
					} else reg_ah=1;
					}
					break;
				case 1:
					if (INT10_VideoState_Save(reg_cx,RealMake(SegValue(es),reg_bx))) reg_ah=0;
					else reg_ah=1;
					break;
				case 2:
					if (INT10_VideoState_Restore(reg_cx,RealMake(SegValue(es),reg_bx))) reg_ah=0;
					else reg_ah=1;
					break;
				default:
					reg_ah=1;
					break;
			}
			break;
		case 0x05:							
			if (reg_bh==0) {				/* Set CPU Window */
				reg_ah=VESA_SetCPUWindow(reg_bl,reg_dl);
				reg_al=0x4f;
			} else if (reg_bh == 1) {		/* Get CPU Window */
				reg_ah=VESA_GetCPUWindow(reg_bl,reg_dx);
				reg_al=0x4f;
			} else {
				LOG(LOG_INT10,LOG_ERROR)("Unhandled VESA Function %X Subfunction %X",reg_al,reg_bh);
				reg_ah=0x01;
			}
			break;
		case 0x06:
			reg_al=0x4f;
			reg_ah=VESA_ScanLineLength(reg_bl,reg_cx,reg_bx,reg_cx,reg_dx);
			break;
		case 0x07:
			switch (reg_bl) {
			case 0x80:						/* Set Display Start during retrace */
			case 0x00:						/* Set display Start */
				reg_al=0x4f;
				reg_ah=VESA_SetDisplayStart(reg_cx,reg_dx,reg_bl==0x80);
				break;
			case 0x01:
				reg_al=0x4f;
				reg_bh=0x00;				//reserved
				reg_ah=VESA_GetDisplayStart(reg_cx,reg_dx);
				break;
			default:
				LOG(LOG_INT10,LOG_ERROR)("Unhandled VESA Function %X Subfunction %X",reg_al,reg_bl);
				reg_ah=0x1;
				break;
			}
			break;
        case 0x08:
            switch (reg_bl) {
                case 0x00:                  /* Set DAC width */
                    if (CurMode->type == M_LIN8) {
                        /* TODO: If there is a bit on S3 cards to control DAC width in "pseudocolor" modes, replace this code
                         *       with something to write that bit instead of internal state change like this. */
                        if (reg_bh >= 8 && enable_vga_8bit_dac)
                            vga_8bit_dac = true;
                        else
                            vga_8bit_dac = false;

                        VGA_DAC_UpdateColorPalette();
                        reg_bh=(vga_8bit_dac ? 8 : 6);
                        reg_al=0x4f;
                        reg_ah=0x0;

                        LOG(LOG_INT10,LOG_NORMAL)("VESA BIOS called to set VGA DAC width to %u bits",reg_bh);
                    }
                    else {
                        reg_al=0x4f;
                        reg_ah=0x3;
                    }
                    break;
                case 0x01:                  /* Get DAC width */
                    if (CurMode->type == M_LIN8) {
                        reg_bh=(vga_8bit_dac ? 8 : 6);
                        reg_al=0x4f;
                        reg_ah=0x0;
                    }
                    else {
                        reg_al=0x4f;
                        reg_ah=0x3;
                    }
                    break;
                default:
                    LOG(LOG_INT10,LOG_ERROR)("Unhandled VESA Function %X Subfunction %X",reg_al,reg_bl);
                    reg_ah=0x1;
                    break;
            }
            break;
		case 0x09:
			switch (reg_bl) {
			case 0x80:						/* Set Palette during retrace */
			case 0x00:						/* Set Palette */
				reg_ah=VESA_SetPalette(SegPhys(es)+reg_di,reg_dx,reg_cx,reg_bl==0x80);
				reg_al=0x4f;
				break;
			case 0x01:						/* Get Palette */
				reg_ah=VESA_GetPalette(SegPhys(es)+reg_di,reg_dx,reg_cx);
				reg_al=0x4f;
				break;
			default:
				LOG(LOG_INT10,LOG_ERROR)("Unhandled VESA Function %X Subfunction %X",reg_al,reg_bl);
				reg_ah=0x01;
				break;
			}
			break;
		case 0x0a:							/* Get Pmode Interface */
			if (int10.vesa_oldvbe) {
				reg_ax=0x014f;
				break;
			}
			switch (reg_bl) {
			case 0x00:
				SegSet16(es,RealSeg(int10.rom.pmode_interface));
				reg_di=RealOff(int10.rom.pmode_interface);
				reg_cx=int10.rom.pmode_interface_size;
				reg_ax=0x004f;
				break;
			case 0x01:						/* Get code for "set window" */
				SegSet16(es,RealSeg(int10.rom.pmode_interface));
				reg_di=RealOff(int10.rom.pmode_interface)+(Bit32u)int10.rom.pmode_interface_window;
				reg_cx=int10.rom.pmode_interface_start-int10.rom.pmode_interface_window;
				reg_ax=0x004f;
				break;
			case 0x02:						/* Get code for "set display start" */
				SegSet16(es,RealSeg(int10.rom.pmode_interface));
				reg_di=RealOff(int10.rom.pmode_interface)+(Bit32u)int10.rom.pmode_interface_start;
				reg_cx=int10.rom.pmode_interface_palette-int10.rom.pmode_interface_start;
				reg_ax=0x004f;
				break;
			case 0x03:						/* Get code for "set palette" */
				SegSet16(es,RealSeg(int10.rom.pmode_interface));
				reg_di=RealOff(int10.rom.pmode_interface)+(Bit32u)int10.rom.pmode_interface_palette;
				reg_cx=int10.rom.pmode_interface_size-int10.rom.pmode_interface_palette;
				reg_ax=0x004f;
				break;
			default:
				reg_ax=0x014f;
				break;
			}
			break;

		default:
			LOG(LOG_INT10,LOG_ERROR)("Unhandled VESA Function %X",reg_al);
			reg_al=0x0;
			break;
		}
		break;
	case 0xf0:
		INT10_EGA_RIL_ReadRegister(reg_bl, reg_dx);
		break;
	case 0xf1:
		INT10_EGA_RIL_WriteRegister(reg_bl, reg_bh, reg_dx);
		break;
	case 0xf2:
		INT10_EGA_RIL_ReadRegisterRange(reg_ch, reg_cl, reg_dx, SegPhys(es)+reg_bx);
		break;
	case 0xf3:
		INT10_EGA_RIL_WriteRegisterRange(reg_ch, reg_cl, reg_dx, SegPhys(es)+reg_bx);
		break;
	case 0xf4:
		INT10_EGA_RIL_ReadRegisterSet(reg_cx, SegPhys(es)+reg_bx);
		break;
	case 0xf5:
		INT10_EGA_RIL_WriteRegisterSet(reg_cx, SegPhys(es)+reg_bx);
		break;
	case 0xfa: {
		RealPt pt=INT10_EGA_RIL_GetVersionPt();
		SegSet16(es,RealSeg(pt));
		reg_bx=RealOff(pt);
		}
		break;
	case 0xff:
		if (!warned_ff) LOG(LOG_INT10,LOG_NORMAL)("INT10:FF:Weird NC call");
		warned_ff=true;
		break;
	default:
		LOG(LOG_INT10,LOG_ERROR)("Function %4X not supported",reg_ax);
//		reg_al=0x00;		//Successfull, breaks marriage
		break;
	}
	return CBRET_NONE;
}

static void INT10_Seg40Init(void) {
	// the default char height
	real_writeb(BIOSMEM_SEG,BIOSMEM_CHAR_HEIGHT,16);
	// Clear the screen 
	real_writeb(BIOSMEM_SEG,BIOSMEM_VIDEO_CTL,0x60);
	// Set the basic screen we have
	real_writeb(BIOSMEM_SEG,BIOSMEM_SWITCHES,0xF9);
	// Set the basic modeset options
	real_writeb(BIOSMEM_SEG,BIOSMEM_MODESET_CTL,0x51); // why is display switching enabled (bit 6) ?
	// Set the  default MSR
	real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_MSR,0x09);
	// Set the pointer to video save pointer table
	real_writed(BIOSMEM_SEG, BIOSMEM_VS_POINTER, int10.rom.video_save_pointers);
}

static void INT10_InitVGA(void) {
	if (IS_EGAVGA_ARCH) {
		LOG(LOG_MISC,LOG_DEBUG)("INT 10: initializing EGA/VGA state");

		/* switch to color mode and enable CPU access 480 lines */
		IO_Write(0x3c2,0xc3);
		/* More than 64k */
		IO_Write(0x3c4,0x04);
		IO_Write(0x3c5,0x02);
		if (IS_VGA_ARCH) {
			/* Initialize DAC */
			IO_Write(0x3c8,0);
			for (Bitu i=0;i<3*256;i++) IO_Write(0x3c9,0);
		}
	}
}

static void SetupTandyBios(void) {
	if (machine==MCH_TANDY) {
		unsigned int i;
		static Bit8u TandyConfig[130]= {
		0x21, 0x42, 0x49, 0x4f, 0x53, 0x20, 0x52, 0x4f, 0x4d, 0x20, 0x76, 0x65, 0x72,
		0x73, 0x69, 0x6f, 0x6e, 0x20, 0x30, 0x32, 0x2e, 0x30, 0x30, 0x2e, 0x30, 0x30,
		0x0d, 0x0a, 0x43, 0x6f, 0x6d, 0x70, 0x61, 0x74, 0x69, 0x62, 0x69, 0x6c, 0x69,
		0x74, 0x79, 0x20, 0x53, 0x6f, 0x66, 0x74, 0x77, 0x61, 0x72, 0x65, 0x0d, 0x0a,
		0x43, 0x6f, 0x70, 0x79, 0x72, 0x69, 0x67, 0x68, 0x74, 0x20, 0x28, 0x43, 0x29,
		0x20, 0x31, 0x39, 0x38, 0x34, 0x2c, 0x31, 0x39, 0x38, 0x35, 0x2c, 0x31, 0x39,
		0x38, 0x36, 0x2c, 0x31, 0x39, 0x38, 0x37, 0x0d, 0x0a, 0x50, 0x68, 0x6f, 0x65,
		0x6e, 0x69, 0x78, 0x20, 0x53, 0x6f, 0x66, 0x74, 0x77, 0x61, 0x72, 0x65, 0x20,
		0x41, 0x73, 0x73, 0x6f, 0x63, 0x69, 0x61, 0x74, 0x65, 0x73, 0x20, 0x4c, 0x74,
		0x64, 0x2e, 0x0d, 0x0a, 0x61, 0x6e, 0x64, 0x20, 0x54, 0x61, 0x6e, 0x64, 0x79
		};

		LOG(LOG_MISC,LOG_DEBUG)("Initializing Tandy video state (video BIOS init)");

		for(i=0;i<130;i++)
			phys_writeb(0xf0000+i+0xc000, TandyConfig[i]);
	}
}

bool MEM_map_ROM_physmem(Bitu start,Bitu end);

extern Bitu VGA_BIOS_Size;
extern Bitu VGA_BIOS_SEG;
extern Bitu VGA_BIOS_SEG_END;
extern bool VIDEO_BIOS_disable;
extern Bitu BIOS_VIDEO_TABLE_LOCATION;
extern Bitu BIOS_VIDEO_TABLE_SIZE;

bool ROMBIOS_FreeMemory(Bitu phys);
Bitu RealToPhys(Bitu x);

void BIOS_UnsetupDisks(void);
void BIOS_UnsetupKeyboard(void);
bool MEM_unmap_physmem(Bitu start,Bitu end);
void CALLBACK_DeAllocate(Bitu in);

void INT10_OnResetComplete() {
    if (VGA_BIOS_Size > 0)
        MEM_unmap_physmem(0xC0000,0xC0000+VGA_BIOS_Size-1);

    /* free the table */
    BIOS_VIDEO_TABLE_SIZE = 0;
    if (BIOS_VIDEO_TABLE_LOCATION != (~0U) && BIOS_VIDEO_TABLE_LOCATION != 0) {
        LOG(LOG_MISC,LOG_DEBUG)("INT 10h freeing BIOS VIDEO TABLE LOCATION");
        ROMBIOS_FreeMemory(RealToPhys(BIOS_VIDEO_TABLE_LOCATION));
        BIOS_VIDEO_TABLE_LOCATION = ~0u;		// RealMake(0xf000,0xf0a4)
    }

    void VESA_OnReset_Clear_Callbacks(void);
    VESA_OnReset_Clear_Callbacks();

    if (call_10 != 0) {
        CALLBACK_DeAllocate(call_10);
        call_10 = 0;
    }

    BIOS_UnsetupDisks();
    BIOS_UnsetupKeyboard();
}

extern int vesa_set_display_vsync_wait;
extern bool vesa_bank_switch_window_range_check;
extern bool vesa_bank_switch_window_mirror;
extern bool vesa_zero_on_get_information;
extern bool unmask_irq0_on_int10_setmode;
extern bool int16_unmask_irq1_on_read;
extern bool int10_vga_bios_vector;
extern bool int16_ah_01_cf_undoc;

#if 0 /* reference */
typedef struct tagBITMAPFILEHEADER {
    WORD  bfType;
    DWORD bfSize;
    WORD  bfReserved1;
    WORD  bfReserved2;
    DWORD bfOffBits;
} BITMAPFILEHEADER, *PBITMAPFILEHEADER;

typedef struct tagBITMAPINFO {
    BITMAPINFOHEADER bmiHeader;
    RGBQUAD          bmiColors[1];
} BITMAPINFO, *PBITMAPINFO;

typedef struct tagBITMAPINFOHEADER {
    DWORD biSize;
    LONG  biWidth;
    LONG  biHeight;
    WORD  biPlanes;
    WORD  biBitCount;
    DWORD biCompression;
    DWORD biSizeImage;
    LONG  biXPelsPerMeter;
    LONG  biYPelsPerMeter;
    DWORD biClrUsed;
    DWORD biClrImportant;
} BITMAPINFOHEADER, *PBITMAPINFOHEADER;
#endif

bool Load_FONT_ROM(void) {
    unsigned int hibyte,lowbyte,r;
    unsigned char tmp[256*16]; // 8x16 256 cells
    FILE *fp;

             fp = fopen("font.rom","rb");
    if (!fp) fp = fopen("FONT.rom","rb");
    if (!fp) fp = fopen("font.ROM","rb");
    if (!fp) fp = fopen("FONT.ROM","rb");
    if (!fp) {
        LOG_MSG("PC-98 font loading: FONT.ROM not found");
        return false;
    }

    // prepare
    memset(vga.draw.font,0,sizeof(vga.draw.font));

    /* FONT.ROM is always 288768 bytes large and contains:
     *
     * 256      8x8     Single-wide character cells         at      0x00000
     * 256      8x16    Single-wide character cells         at      0x00800
     * 96 x 92  16x16   Double-wide character cells         at      0x01800 */
    fseek(fp,0,SEEK_END);
    if (ftell(fp) != 288768) {
        LOG_MSG("PC-98 FONT.ROM is not the correct size");
        goto fail;
    }
    fseek(fp,0,SEEK_SET);

    /* NTS: We do not yet use the 8x8 character set */

    /* 8x16 single-wide */
    fseek(fp,0x800,SEEK_SET);
    if (fread(tmp,256*16,1,fp) != 1) goto fail;
    for (lowbyte=0;lowbyte < 256;lowbyte++) {
        for (r=0;r < 16;r++) {
            vga.draw.font[(lowbyte*16)+r] = tmp[(lowbyte*16)+r];
        }
    }

    /* 16x16 double-wide */
    assert(sizeof(tmp) >= (96 * 16 * 2));
    for (lowbyte=0x01;lowbyte < 0x5D;lowbyte++) {
        fseek(fp,long(0x1800u + ((lowbyte - 0x01u) * 96u * 16u * 2u/*16 wide*/)),SEEK_SET);
        if (fread(tmp,96 * 16 * 2/*16 wide*/,1,fp) != 1) goto fail;

        for (hibyte=0;hibyte < 96;hibyte++) {
            unsigned int i;
            unsigned int o;

            i = hibyte * 16 * 2;
            o = (((hibyte + 0x20) * 128) + lowbyte) * 16 * 2;

            for (r=0;r < 16;r++) {
                vga.draw.font[o+(r*2)  ] = tmp[i+r+0];
                vga.draw.font[o+(r*2)+1] = tmp[i+r+16];
            }
        }
    }

    LOG_MSG("FONT.ROM loaded");
    fclose(fp);
    return true;
fail:
    fclose(fp);
    return false;
}

/* ANEX86.BMP from the Anex86 emulator.
 * Holds the font as a giant 2048x2048 1-bit monochromatic bitmap. */
/* We load it separately because I am uncertain whether it is legal or not to
 * incorporate this directly into DOSBox-X. */
bool Load_Anex86_Font(void) {
    unsigned char tmp[(2048/8)*16]; /* enough for one 2048x16 row and bitmap header */
    unsigned int hibyte,lowbyte,r;
    unsigned int bmp_ofs;
    FILE *fp = NULL;

    /* ANEX86.BMP accurate dump of actual font */
    fp = fopen("anex86.bmp","rb");
    if (!fp) fp = fopen("ANEX86.bmp","rb");
    if (!fp) fp = fopen("ANEX86.BMP","rb");

    /* FREECG98.BMP free open source generated copy from system fonts */
    if (!fp) fp = fopen("freecg98.bmp","rb");
    if (!fp) fp = fopen("FREECG98.bmp","rb");
    if (!fp) fp = fopen("FREECG98.BMP","rb");

    /* Linux builds allow FREECG98.BMP in /usr/share/dosbox-x */
    /* Mac OS X builds carry FREECG98.BMP in the Resources subdirectory of the .app bundle */
    {
        std::string resdir,tmpdir;

        Cross::GetPlatformResDir(resdir);
        if (!resdir.empty()) {
            /* FREECG98.BMP free open source generated copy from system fonts */
            if (!fp) {
                tmpdir = resdir + "freecg98.bmp";
                fp = fopen(tmpdir.c_str(),"rb");
            }
            if (!fp) {
                tmpdir = resdir + "FREECG98.BMP";
                fp = fopen(tmpdir.c_str(),"rb");
            }
        }
    }

    if (!fp) {
        LOG_MSG("PC-98 font loading: neither ANEX86.BMP nor FREECG98.BMP found");
        return false;
    }

    if (fread(tmp,14,1,fp) != 1) goto fail; // BITMAPFILEHEADER
    if (memcmp(tmp,"BM",2) != 0) goto fail; // must be "BM"
    bmp_ofs = host_readd((HostPt)(tmp+10)); // bOffBits

    if (fread(tmp,40,1,fp) != 1) goto fail; // BITMAPINFOHEADER
    if (host_readd((HostPt)(tmp+0)) != 40) goto fail; // biSize == 40 or else
    if (host_readd((HostPt)(tmp+4)) != 2048) goto fail; // biWidth == 2048 or else
    if (host_readd((HostPt)(tmp+8)) != 2048) goto fail; // biHeight == 2048 or else
    if (host_readw((HostPt)(tmp+12)) != 1) goto fail; // biPlanes == 1 or else
    if (host_readw((HostPt)(tmp+14)) != 1) goto fail; // biBitCount == 1 or else
    if (host_readd((HostPt)(tmp+16)) != 0) goto fail; // biCompression == 0 or else

    /* first row is 8x16 single width */
    fseek(fp,long(bmp_ofs+((2048u-16u)*(2048u/8u))),SEEK_SET); /* arrrgh bitmaps are upside-down */
    if (fread(tmp,(2048/8)*16,1,fp) != 1) goto fail;
    for (lowbyte=0;lowbyte < 256;lowbyte++) {
        for (r=0;r < 16;r++) {
            vga.draw.font[(lowbyte*16)+r] = tmp[lowbyte+((15-r/*upside-down!*/)*(2048/8))] ^ 0xFF/*ANEX86 has inverse color scheme*/;
        }
    }
    /* everything after is 16x16 fullwidth.
     * note: 2048 / 16 = 128 */
    for (hibyte=1;hibyte < 128;hibyte++) {
        fseek(fp,long(bmp_ofs+((2048u-(16u*hibyte)-16u)*(2048u/8u))),SEEK_SET); /* arrrgh bitmaps are upside-down */
        if (fread(tmp,(2048/8)*16,1,fp) != 1) goto fail;

        for (lowbyte=0;lowbyte < 128;lowbyte++) {
            for (r=0;r < 16;r++) {
                unsigned int i;
                unsigned int o;

                /* NTS: fullwidth is 16x16 128 chars across.
                 * each row of the char bitmap is TWO bytes. */
                i = (lowbyte*2)+((15-r/*upside-down!*/)*(2048/8));
                o = ((((hibyte*128)+lowbyte)*16)+r)*2;

                assert((i+2) <= sizeof(tmp));
                assert((o+2) <= sizeof(vga.draw.font));

                vga.draw.font[o+0] = tmp[i+0] ^ 0xFF;
                vga.draw.font[o+1] = tmp[i+1] ^ 0xFF;
            }
        }
    }

    LOG_MSG("ANEX86.BMP/FREECG98.BMP font loaded");
    fclose(fp);
    return true;
fail:
    LOG_MSG("ANEX86.BMP/FREECG98.BMP invalid, ignoring");
    fclose(fp);
    return false;
}

extern Bit8u int10_font_16[256 * 16];

extern VideoModeBlock PC98_Mode;

bool Load_VGAFont_As_PC98(void) {
    unsigned int i;

    for (i=0;i < (256 * 16);i++)
        vga.draw.font[i] = int10_font_16[i];

    return true;
}

void INT10_EnterPC98(Section *sec) {
    (void)sec;//UNUSED
    /* deprecated */
}

RealPt GetSystemBiosINT10Vector(void) {
    if (call_10 != 0)
        return CALLBACK_RealPointer(call_10);
    else
        return 0;
}

void INT10_Startup(Section *sec) {
    (void)sec;//UNUSED
	LOG(LOG_MISC,LOG_DEBUG)("INT 10h reinitializing");

	vesa_set_display_vsync_wait = static_cast<Section_prop *>(control->GetSection("video"))->Get_int("vesa set display vsync");
	vesa_bank_switch_window_range_check = static_cast<Section_prop *>(control->GetSection("video"))->Get_bool("vesa bank switching window range check");
	vesa_bank_switch_window_mirror = static_cast<Section_prop *>(control->GetSection("video"))->Get_bool("vesa bank switching window mirroring");
	vesa_zero_on_get_information = static_cast<Section_prop *>(control->GetSection("video"))->Get_bool("vesa zero buffer on get information");
	unmask_irq0_on_int10_setmode = static_cast<Section_prop *>(control->GetSection("video"))->Get_bool("unmask timer on int 10 setmode");
	int16_unmask_irq1_on_read = static_cast<Section_prop *>(control->GetSection("video"))->Get_bool("unmask keyboard on int 16 read");
	int16_ah_01_cf_undoc = static_cast<Section_prop *>(control->GetSection("video"))->Get_bool("int16 keyboard polling undocumented cf behavior");
	int10_vga_bios_vector = static_cast<Section_prop *>(control->GetSection("video"))->Get_bool("int 10h points at vga bios");

    if (!IS_PC98_ARCH) {
        INT10_InitVGA();
        if (IS_TANDY_ARCH) SetupTandyBios();
        /* Setup the INT 10 vector */
        call_10=CALLBACK_Allocate();	
        CALLBACK_Setup(call_10,&INT10_Handler,CB_IRET,"Int 10 video");
        RealSetVec(0x10,CALLBACK_RealPointer(call_10));
        //Init the 0x40 segment and init the datastructures in the the video rom area
        INT10_SetupRomMemory();
        INT10_Seg40Init();
        INT10_SetupBasicVideoParameterTable();

        LOG(LOG_MISC,LOG_DEBUG)("INT 10: VGA bios used %d / %d memory",(int)int10.rom.used,(int)VGA_BIOS_Size);
        if (int10.rom.used > VGA_BIOS_Size) /* <- this is fatal, it means the Setup() functions scrozzled over the adjacent ROM or RAM area */
            E_Exit("VGA BIOS size too small %u > %u",(unsigned int)int10.rom.used,(unsigned int)VGA_BIOS_Size);

        /* NTS: Uh, this does seem bass-ackwards... INT 10h making the VGA BIOS appear. Can we refactor this a bit? */
        if (VGA_BIOS_Size > 0) {
            LOG(LOG_MISC,LOG_DEBUG)("VGA BIOS occupies segment 0x%04x-0x%04x",(int)VGA_BIOS_SEG,(int)VGA_BIOS_SEG_END-1);
            if (!MEM_map_ROM_physmem(0xC0000,0xC0000+VGA_BIOS_Size-1))
                LOG(LOG_MISC,LOG_WARN)("INT 10 video: unable to map BIOS");
        }
        else {
            LOG(LOG_MISC,LOG_DEBUG)("Not mapping VGA BIOS");
        }

        INT10_SetVideoMode(0x3);
    }
    else {
        /* load PC-98 character ROM data, if possible */
        {
            /* We can use FONT.ROM as generated by T98Tools */
            bool ok = Load_FONT_ROM();
            /* We can use ANEX86.BMP from the Anex86 emulator */
            if (!ok) ok = Load_Anex86_Font();
            /* Failing all else we can just re-use the IBM VGA 8x16 font to show SOMETHING on the screen.
             * Japanese text will not display properly though. */
            if (!ok) ok = Load_VGAFont_As_PC98();
        }

        CurMode = &PC98_Mode;

        /* FIXME: This belongs in MS-DOS kernel init, because these reside in the CON driver */
        /* Some PC-98 game behavior seems to suggest the BIOS data area stretches all the way from segment 0x40:0x00 to segment 0x7F:0x0F inclusive.
         * Compare that to IBM PC platform, where segment fills only 0x40:0x00 to 0x50:0x00 inclusive and extra state is held in the "Extended BIOS Data Area".
         */

        /* number of text rows on the screen.
         * Touhou Project will not clear/format the text layer properly without this variable. */
        mem_writeb(0x710,0); /* cursor position Y coordinate */
        mem_writeb(0x711,1); /* function definition display status flag */
        mem_writeb(0x712,25 - 1 - 1); /* scroll range lower limit (usually 23 when function key row is visible) */
        mem_writeb(0x713,1); /* normal 25 lines */
        mem_writeb(0x714,0xE1); /* content erase attribute */

        mem_writeb(0x719,0x20); /* content erase character */

        mem_writeb(0x71B,0x01); /* cursor displayed */
        mem_writeb(0x71C,0x00); /* cursor position X coordinate */
        mem_writeb(0x71D,0xE1); /* content display attribute */
        mem_writeb(0x71E,0x00); /* scroll range upper limit (usually 0) */
        mem_writeb(0x71F,0x01); /* scrolling speed is normal */

        /* init text RAM */
        for (unsigned int i=0;i < 0x1FE0;i += 2) {
            mem_writew(0xA0000+i,0);
            mem_writeb(0xA2000+i,0xE1);
        }
        /* clear graphics RAM */
        for (unsigned int i=0;i < 0x8000;i += 2) {
            mem_writew(0xA8000+i,0);
            mem_writew(0xB0000+i,0);
            mem_writew(0xB8000+i,0);
            mem_writew(0xE0000+i,0);
        }
    }
}

void INT10_Init() {
}

//save state support
namespace
{
class SerializeInt10 : public SerializeGlobalPOD
{
public:
    SerializeInt10() : SerializeGlobalPOD("Int10")
    {
        registerPOD(int10);
        //registerPOD(CurMode);
        //registerPOD(call_10);
        //registerPOD(warned_ff);
    }

	 //   virtual void setBytes(std::istream& stream)
    //{
      //  SerializeGlobalPOD::setBytes(stream);
		//}
} dummy;
}
