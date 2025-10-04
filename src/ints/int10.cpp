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

#include <assert.h>

#include "dosbox.h"
#include "control.h"
#include "logging.h"
#include "mem.h"
#include "menu.h"
#include "callback.h"
#include "regs.h"
#include "inout.h"
#include "int10.h"
#include "mouse.h"
#include "setup.h"
#include "render.h"
#include "jfont.h"

Int10Data int10;
bool blinking=true;
static Bitu call_10 = 0;
static bool warned_ff=false;
extern bool enable_vga_8bit_dac;
extern bool vga_8bit_dac;
extern bool wpExtChar;
extern bool ega200;
extern int wpType;

uint16_t GetTextSeg();
void WriteCharTopView(uint16_t off, int count);
uint8_t *GetSbcsFont(Bitu code);
uint8_t *GetSbcs19Font(Bitu code);
uint8_t *GetSbcs24Font(Bitu code);
uint8_t *GetDbcsFont(Bitu code);
uint8_t *GetDbcs24Font(Bitu code);
void DOSV_FillScreen();
std::string GetDOSBoxXPath(bool withexe=false);
void ResolvePath(std::string& in);
void INT10_ReadString(uint8_t row, uint8_t col, uint8_t flag, uint8_t attr, PhysPt string, uint16_t count,uint8_t page);
bool INT10_SetDOSVModeVtext(uint16_t mode, enum DOSV_VTEXT_MODE vtext_mode);
void INT10_SetJ3ModeCGA4(uint16_t mode);
bool J3_IsCga4Dcga();
#if defined(USE_TTF)
extern bool colorChanged, justChanged;
extern bool ttf_dosv;
void ttf_reset(void);
#endif
Bitu INT10_Handler(void) {
	// NTS: We do have to check the "current video mode" from the BIOS data area every call.
	//      Some OSes like Windows 95 rely on overwriting the "current video mode" byte in
	//      the BIOS data area to play tricks with the BIOS. If we don't call this, tricks
	//      like the Windows 95 boot logo or INT 10h virtualization in Windows 3.1/9x/ME
	//      within the DOS "box" will not work properly.
	if(IS_DOSV && DOSV_CheckCJKVideoMode() && reg_ah != 0x03) DOSV_OffCursor();
	else if(J3_IsJapanese()) J3_OffCursor();
	INT10_SetCurMode();

	switch (reg_ah) {
	case 0x00:								/* Set VideoMode */
		Mouse_BeforeNewVideoMode(true);
		SetTrueVideoMode(reg_al);
		if(IS_DOSV && IS_DOS_CJK && (reg_al == 0x03 || (reg_al >= 0x70 && reg_al <= 0x73) || reg_al == 0x78)) {
			uint8_t mode = 0x03;
			if(reg_al == 0x03 || reg_al == 0x72 || reg_al == 0x73) {
				INT10_SetVideoMode(0x12);
				INT10_SetDOSVModeVtext(mode, DOSV_VGA);
				if(reg_al == 0x72 || reg_al == 0x73) {
					real_writeb(BIOSMEM_SEG, BIOSMEM_CURRENT_MODE, reg_al);
				}
			} else if(reg_al == 0x70 || reg_al == 0x71 || reg_al == 0x78) {
				mode = 0x70;
				enum DOSV_VTEXT_MODE vtext_mode = DOSV_GetVtextMode((reg_al == 0x78) ? 1 : 0);
				if(vtext_mode == DOSV_VTEXT_XGA || vtext_mode == DOSV_VTEXT_XGA_24) {
					if(svgaCard == SVGA_TsengET4K) {
						INT10_SetVideoMode(0x37);
					} else {
						INT10_SetVideoMode(0x104);
					}
					INT10_SetDOSVModeVtext(mode, vtext_mode);
				} else if(vtext_mode == DOSV_VTEXT_SXGA || vtext_mode == DOSV_VTEXT_SXGA_24) {
					if(svgaCard == SVGA_TsengET4K) {
						INT10_SetVideoMode(0x3d);
					} else {
						INT10_SetVideoMode(0x106);
					}
					INT10_SetDOSVModeVtext(mode, vtext_mode);
				} else if(vtext_mode == DOSV_VTEXT_SVGA) {
					INT10_SetVideoMode((svgaCard == SVGA_TsengET3K) ? 0x29 : (svgaCard == SVGA_ParadisePVGA1A) ? 0x58 : 0x6a);
					INT10_SetDOSVModeVtext(mode, vtext_mode);
				} else {
					INT10_SetVideoMode(0x12);
					INT10_SetDOSVModeVtext(mode, DOSV_VTEXT_VGA);
				}
				if(reg_al == 0x71) {
					real_writeb(BIOSMEM_SEG, BIOSMEM_CURRENT_MODE, reg_al);
				}
			}
			DOSV_FillScreen();
		} else {
			if(J3_IsCga4Dcga()) {
				INT10_SetVideoMode(0x74);
				INT10_SetJ3ModeCGA4(reg_al);
			} else {
				INT10_SetVideoMode(reg_al);
				if(reg_al == 0x74 && IS_J3100)
					DOSV_FillScreen();
			}
		}
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
			uint8_t crtcpu=real_readb(BIOSMEM_SEG, BIOSMEM_CRTCPU_PAGE);
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
		INT10_ScrollWindow(reg_ch,reg_cl,reg_dh,reg_dl,-(int8_t)reg_al,reg_bh,0xFF);
		break;
	case 0x07:								/* Scroll Down */
		INT10_ScrollWindow(reg_ch,reg_cl,reg_dh,reg_dl,(int8_t)reg_al,reg_bh,0xFF);
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
		if(DOSV_CheckCJKVideoMode()) {
			uint16_t attr;
			INT10_ReadCharAttr(&attr, 0);
			INT10_TeletypeOutput(reg_al, attr >> 8);
		} else
			INT10_TeletypeOutput(reg_al,reg_bl);
		break;
	case 0x0F:								/* Get videomode */
		reg_bh=real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE);
		reg_al=real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_MODE);
		if (IS_EGAVGA_ARCH) reg_al|=real_readb(BIOSMEM_SEG,BIOSMEM_VIDEO_CTL)&0x80;
		reg_ah=(uint8_t)real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS);
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
#if defined(USE_TTF)
			if (IS_EGAVGA_ARCH) colorChanged = justChanged = true;
#endif
			break;
		case 0x01:							/* SET BORDER (OVERSCAN) COLOR*/
			INT10_SetOverscanBorderColor(reg_bh);
			break;
		case 0x02:							/* SET ALL PALETTE REGISTERS */
			INT10_SetAllPaletteRegisters(SegPhys(es)+reg_dx);
#if defined(USE_TTF)
			if (IS_EGAVGA_ARCH) colorChanged = justChanged = true;
#endif
			break;
		case 0x03:							/* TOGGLE INTENSITY/BLINKING BIT */
			blinking=reg_bl==1;
			mainMenu.get_item("text_background").check(!blinking).refresh_item(mainMenu);
			mainMenu.get_item("text_blinking").check(blinking).refresh_item(mainMenu);
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
#if defined(USE_TTF)
			if (ttf.inUse && reg_al == 0x12 && (real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_MODE) == 0x03 || real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_MODE) == 0x55)) {
				real_writeb(BIOSMEM_SEG,BIOSMEM_NB_ROWS,50-1);
				Bitu pagesize=50*real_readb(BIOSMEM_SEG,BIOSMEM_NB_COLS)*2;
				pagesize+=0x100; // bios adds extra on reload
				real_writew(BIOSMEM_SEG,BIOSMEM_PAGE_SIZE,(uint16_t)pagesize);
				ttf_reset();
				break;
			}
#endif
			INT10_LoadFont(Real2Phys(int10.rom.font_8_first),reg_al==0x12,256,0,reg_bl&0x7f,8);
			break;
		case 0x03:			/* Set Block Specifier */
#if defined(USE_TTF)
			if (ttf.inUse&&wpType==1) {
				DOS_Block dos;
				if (mem_readd(((dos.psp()-1)<<4)+8) == 0x5057) // Name of MCB PSP should be WP
					wpExtChar = reg_bl != 0;
			}
#endif
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
			if (machine == MCH_MCGA) {
				if (reg_bh == 5 || reg_bh == 7) /* these don't work on MCGA */
					break;
			}

			switch (reg_bh) {
			case 0x00:	/* interrupt 0x1f vector */
				{
					RealPt int_1f=RealGetVec(0x1f);
					SegSet16(es,RealSeg(int_1f));
					reg_bp=RealOff(int_1f);
				}
				break;
			case 0x01:	/* interrupt 0x43 vector */
				{
					RealPt int_43=RealGetVec(0x43);
					SegSet16(es,RealSeg(int_43));
					reg_bp=RealOff(int_43);
				}
				break;
			case 0x02:	/* font 8x14 */
				if (machine == MCH_MCGA) {
					/* No such font on MCGA, returns 8x16 font */
					SegSet16(es,RealSeg(int10.rom.font_16));
					reg_bp=RealOff(int10.rom.font_16);
				}
				else {
					SegSet16(es,RealSeg(int10.rom.font_14));
					reg_bp=RealOff(int10.rom.font_14);
				}
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
				if (IS_VGA_ARCH || machine == MCH_MCGA) {
					SegSet16(es,RealSeg(int10.rom.font_16));
					reg_bp=RealOff(int10.rom.font_16);
				}
				else if (IS_EGA_ARCH) {
					/* EGA BIOSes reportedly return garbage here */
					SegSet16(es,0xC000);
					reg_bp=0xFB80;
				}
				break;
			case 0x07:	/* alpha alternate 9x16 */
				if (IS_VGA_ARCH) {
					SegSet16(es,RealSeg(int10.rom.font_16_alternate));
					reg_bp=RealOff(int10.rom.font_16_alternate);
				}
				else if (IS_EGA_ARCH) {
					/* EGA BIOSes reportedly return garbage here */
					SegSet16(es,0xC000);
					reg_bp=0x7210;
				}
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

		if (reg_bh == 0x55 && IS_VGA_ARCH && svgaCard == SVGA_ATI) { /* ATI VGA ALTERNATE FUNC SELECT ENHANCED FEATURES */
			/* NTS: When WHATVGA detects an ATI chipset, all modesetting within the program REQUIRES this function or
			 *      else it does not change the video mode. This function is heavily relied on by WHATVGA to detect
			 *      whether or not the video mode exists, INCLUDING the base VGA standard modes! */
			if (!IS_VGA_ARCH) break;
			if (svgaCard!=SVGA_ATI) break;
			if (reg_bl > 6) break;

			switch (reg_bl) {
				case 0x02: /*get status*/
					reg_al = 0x08/*enhanced features enabled*/ | (2/*multisync monitor*/ << 5u);
					break;
				case 0x06: { /*get mode table*/
					unsigned char video_mode = reg_al;
					/* entry: AL = video mode
					 * exit:  If mode valid: ES:BP = pointer to parameter table (ATI mode table), ES:SI = pointer to parameter table supplement
					 *        If mode invalid: ES and BP unchanged, SI unchanged
					 *
					 * Also DI == 0? [http://hackipedia.org/browse.cgi/Computer/Platform/PC%2c%20IBM%20compatible/Video/VGA/SVGA/ATI%2c%20Array%20Technology%20Inc/ATI%20Mach64%20BIOS%20Kit%20Technical%20Reference%20Manuals%20PN%20BIO%2dC012XX1%2d05%20%281994%29%20v5%2epdf] */
					/* FIXME! Need to return pointers to REAL table data!
					 *        This satisfies WHATVGA for now which only cares that this interrupt call signals the mode exists,
					 *        although the video mode info it shows is understandably weird and wacky. */
					/* TODO: Scan an actual parameter table, including one that includes the vendor specific INT 10h modes for SVGA, here */
					LOG(LOG_INT10,LOG_DEBUG)("ATI VGA INT 12h Get Mode Table for mode 0x%2x",video_mode);
					if ((video_mode >= 0 && video_mode <= 7) || (video_mode >= 13 && video_mode <= 19)) {//standard VGA for now!
						SegSet16(es,0xABCD);
						reg_bp = 0x1234;
					}
					} break;
				case 0x00: /*disable enhanced features*/
				case 0x01: /*enable enhanced features*/
				case 0x03: /*disable register trapping (CGA emulation)*/
				case 0x04: /*enable register trapping*/
				case 0x05: /*program mode described by table at ES:BP*/
				default:
					LOG(LOG_INT10,LOG_DEBUG)("Unhandled ATI VGA INT 12h function: AX=%04x BX=%04x",reg_ax,reg_bx);
					break;
			}
		}

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
				uint8_t modeset_ctl = real_readb(BIOSMEM_SEG,BIOSMEM_MODESET_CTL);
				uint8_t video_switches = real_readb(BIOSMEM_SEG,BIOSMEM_SWITCHES)&0xf0;
				switch(reg_al) {
				case 0: // 200
					modeset_ctl &= 0xef;
					modeset_ctl |= 0x80;
					video_switches |= 7;	// ega normal/cga emulation
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
					video_switches |= 7;	// ega normal/cga emulation
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
				uint8_t temp = real_readb(BIOSMEM_SEG,BIOSMEM_MODESET_CTL) & 0xf7;
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
				uint8_t temp = real_readb(BIOSMEM_SEG,BIOSMEM_MODESET_CTL) & 0xfd;
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
				uint8_t temp = real_readb(BIOSMEM_SEG,BIOSMEM_VIDEO_CTL) & 0xfe;
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
			uint8_t clocking = IO_Read(0x3c5);

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
		if((reg_al & 0x10) != 0 && DOSV_CheckCJKVideoMode())
			INT10_ReadString(reg_dh,reg_dl,reg_al,reg_bl,SegPhys(es)+reg_bp,reg_cx,reg_bh);
		else
			INT10_WriteString(reg_dh,reg_dl,reg_al,reg_bl,SegPhys(es)+reg_bp,reg_cx,reg_bh);
		break;
	case 0x18:
#if defined(USE_TTF)
		if((IS_DOSV || ttf_dosv) && DOSV_CheckCJKVideoMode()) {
#else
		if(IS_DOSV && DOSV_CheckCJKVideoMode()) {
#endif
			uint8_t *font;
			Bitu size = 0;
			if(reg_al == 0) {
				reg_al = 1;
				if(reg_bx == 0) {
					if(reg_dh == 8) {
						if(reg_dl == 16) {
							font = GetSbcsFont(reg_cl);
							size = 16;
						} else if(reg_dl == 19) {
							font = GetSbcs19Font(reg_cl);
							size = 19;
						}
					} else if(reg_dh == 16 && reg_dl == 16) {
						font = GetDbcsFont(reg_cx);
						size = 2 * 16;
					} else if(reg_dh == 12 && reg_dl == 24) {
						font = GetSbcs24Font(reg_cl);
						size = 2 * 24;
					} else if(reg_dh == 24 && reg_dl == 24) {
						font = GetDbcs24Font(reg_cx);
						size = 3 * 24;
					}
					if(size > 0) {
						uint16_t seg = SegValue(es);
						uint16_t off = reg_si;
						for(Bitu ct = 0 ; ct < size ; ct++)
							real_writeb(seg, off++, *font++);
						reg_al = 0;
					}
				}
			}
		}
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
					reg_bx=(uint16_t)ret;
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
	case 0x1d:
#if defined(USE_TTF)
		if((IS_DOSV || ttf_dosv) && DOSV_CheckCJKVideoMode()) {
#else
		if(IS_DOSV && DOSV_CheckCJKVideoMode()) {
#endif
			if(reg_al == 0x00) {
				real_writeb(BIOSMEM_SEG, BIOSMEM_NB_ROWS, int10.text_row - reg_bl);
			} else if(reg_al == 0x01) {
				real_writeb(BIOSMEM_SEG, BIOSMEM_NB_ROWS, int10.text_row);
			} else if(reg_al == 0x02) {
				reg_bx = int10.text_row - real_readb(BIOSMEM_SEG, BIOSMEM_NB_ROWS);
			}
		}
		break;
	case 0x4f:								/* VESA Calls */
		if ((!IS_VGA_ARCH) || (svgaCard!=SVGA_S3Trio))
			break;
		if (int10.vesa_oldvbe10 && reg_al >= 6) /* Functions 6 and up did not exist until VBE 1.2 (or 1.1?) */
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
						reg_bx=(uint16_t)ret;
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
				reg_ah=VESA_SetCPUWindow(reg_bl,reg_dx);
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
				reg_di=RealOff(int10.rom.pmode_interface)+(uint32_t)int10.rom.pmode_interface_window;
				reg_cx=int10.rom.pmode_interface_start-int10.rom.pmode_interface_window;
				reg_ax=0x004f;
				break;
			case 0x02:						/* Get code for "set display start" */
				SegSet16(es,RealSeg(int10.rom.pmode_interface));
				reg_di=RealOff(int10.rom.pmode_interface)+(uint32_t)int10.rom.pmode_interface_start;
				reg_cx=int10.rom.pmode_interface_palette-int10.rom.pmode_interface_start;
				reg_ax=0x004f;
				break;
			case 0x03:						/* Get code for "set palette" */
				SegSet16(es,RealSeg(int10.rom.pmode_interface));
				reg_di=RealOff(int10.rom.pmode_interface)+(uint32_t)int10.rom.pmode_interface_palette;
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
	case 0x50:// Set/Read JP/US mode of CRT BIOS
		switch (reg_al) {
			case 0x00:
				LOG(LOG_INT10, LOG_NORMAL)("AX CRT BIOS 5000h is called.");
				if (INT10_AX_SetCRTBIOSMode(reg_bx)) reg_al = 0x00;
				else reg_al = 0x01;
				break;
			case 0x01:
				//LOG(LOG_INT10,LOG_NORMAL)("AX CRT BIOS 5001h is called.");
				reg_bx=INT10_AX_GetCRTBIOSMode();
				reg_al=0;
				break;
			default:
				LOG(LOG_INT10,LOG_ERROR)("Unhandled AX Function %X",reg_al);
				reg_al=0x0;
				break;
		}
		break;
	case 0x51:// Save/Read JFONT pattern
		if(INT10_AX_GetCRTBIOSMode() == 0x01) break;//exit if CRT BIOS is in US mode
		switch (reg_al) {
			//INT 10h/AX=5100h Store user font pattern
			//IN
			//ES:BP=Index of store buffer
			//DX=Char code
			//BH=width bits of char
			//BL=height bits of char
			//OUT
			//AL=status 00h=Success 01h=Failed
		case 0x00:
		{
			LOG(LOG_INT10, LOG_NORMAL)("AX CRT BIOS 5100h is called.");
			Bitu buf_es = SegValue(es);
			Bitu buf_bp = reg_bp;
			Bitu chr = reg_dx;
			Bitu w_chr = reg_bh;
			Bitu h_chr = reg_bl;
			Bitu font;
			if (w_chr == 16 && h_chr == 16) {
				for (Bitu line = 0; line < 16; line++)
				{
					//Order of font pattern is different between FONTX2 and AX(JEGA).
					font = real_readb(buf_es, buf_bp + line);
					jfont_dbcs_16[chr * 32 + line * 2] = font;
					font = real_readb(buf_es, buf_bp + line + 16);
					jfont_dbcs_16[chr * 32 + line * 2 + 1] = font;
				}
				reg_al = 0x00;
			}
			else
				reg_al = 0x01;
			break;
		}
		//INT 10h/AX=5101h Read character pattern
		//IN
		//ES:BP=Index of read buffer
		//DX=Char code
		//BH=width bits of char
		//BL=height bits of char
		//OUT
		//AL=status 00h=Success 01h=Failed
		case 0x01:
		{
			LOG(LOG_INT10, LOG_NORMAL)("AX CRT BIOS 5101h is called.");
			Bitu buf_es = SegValue(es);
			Bitu buf_bp = reg_bp;
			Bitu chr = reg_dx;
			Bitu w_chr = reg_bh;
			Bitu h_chr = reg_bl;
			Bitu font;
			if (w_chr == 8) {
				reg_al = 0x00;
				switch (h_chr)
				{
				case 8:
					for (Bitu line = 0; line < 8; line++)
						real_writeb(buf_es, buf_bp + line, int10_font_08[chr * 8 + line]);
					break;
				case 14:
					for (Bitu line = 0; line < 14; line++)
						real_writeb(buf_es, buf_bp + line, int10_font_14[chr * 14 + line]);
					break;
				case 19:
					for (Bitu line = 0; line < 19; line++)
						real_writeb(buf_es, buf_bp + line, jfont_sbcs_19[chr * 19 + line]);
					break;
				default:
					reg_al = 0x01;
					break;
				}
			}
			else if (w_chr == 16 && h_chr == 16) {
				reg_al = 0x00;
				for (Bitu line = 0; line < 16; line++)
				{
					font = jfont_dbcs_16[chr * 32 + line * 2];
					real_writeb(buf_es, buf_bp + line, font);
					font = jfont_dbcs_16[chr * 32 + line * 2 + 1];
					real_writeb(buf_es, buf_bp + line + 16, font);
				}
			}
			else
				reg_al = 0x01;
			break;
		}
		default:
			LOG(LOG_INT10,LOG_ERROR)("Unhandled AX Function %X",reg_al);
			reg_al=0x1;
			break;
		}
		break;
	case 0x52:// Set/Read virtual text ram buffer when the video mode is JEGA graphic mode
		if(INT10_AX_GetCRTBIOSMode() == 0x01) break;//exit if CRT BIOS is in US mode
		LOG(LOG_INT10,LOG_NORMAL)("AX CRT BIOS 52xxh is called.");
		switch (reg_al) {
		case 0x00:
		{
			if (reg_bx == 0) real_writew(BIOSMEM_AX_SEG, BIOSMEM_AX_VTRAM_SEGADDR, 0);
			else
			{
				LOG(LOG_INT10, LOG_NORMAL)("AX CRT BIOS set VTRAM segment address at %x", reg_bx);
				real_writew(BIOSMEM_AX_SEG, BIOSMEM_AX_VTRAM_SEGADDR, reg_bx);
				/* Fill VTRAM with 0x20(Space) */
				for (int y = 0; y < 25; y++)
					for (int x = 0; x < 80; x++)
						SetVTRAMChar(x, y, 0x20, 0x00);
			}
			break;
		}
		case 0x01:
		{
			Bitu vtram_seg = real_readw(BIOSMEM_AX_SEG, BIOSMEM_AX_VTRAM_SEGADDR);
			if (vtram_seg == 0x0000) reg_bx = 0;
			else reg_bx = vtram_seg;
			break;
		}
		default:
			LOG(LOG_INT10,LOG_ERROR)("Unhandled AX Function %X",reg_al);
			break;
		}
		break;
		case 0x82:// Set/Read the scroll mode when the video mode is JEGA graphic mode
		if (J3_IsJapanese() || (IS_J3100 && GetTrueVideoMode() == 0x75)) {
			if(reg_al == 0x00) {
				// scroll mode
				reg_al = real_readb(BIOSMEM_J3_SEG, BIOSMEM_J3_SCROLL);
				if(reg_bl == 0x00 || reg_bl == 0x01) {
					real_writeb(BIOSMEM_J3_SEG, BIOSMEM_J3_SCROLL, 0x01);
				}
			} else if(reg_al == 0x04) {
				// cursor blink
				reg_al = real_readb(BIOSMEM_J3_SEG, BIOSMEM_J3_BLINK);
				if(reg_bl == 0x00 || reg_bl == 0x01) {
					real_writeb(BIOSMEM_J3_SEG, BIOSMEM_J3_BLINK, reg_bl);
				}
			} else if(reg_al == 0x05) {
				Section_prop *section = static_cast<Section_prop *>(control->GetSection("dosv"));
				if (section && section->Get_bool("j3100colorscroll")) reg_bl = 0x01;
			}
			break;
		}
		if(INT10_AX_GetCRTBIOSMode() == 0x01) break;//exit if CRT BIOS is in US mode
		LOG(LOG_INT10,LOG_NORMAL)("AX CRT BIOS 82xxh is called.");
		switch (reg_al) {
		case 0x00:
			if (reg_bl == -1) {//Read scroll mode
				reg_al = real_readb(BIOSMEM_AX_SEG, BIOSMEM_AX_JPNSTATUS) & 0x01;
			}
			else {//Set scroll mode
				uint8_t tmp = real_readb(BIOSMEM_AX_SEG, BIOSMEM_AX_JPNSTATUS);
				reg_al = tmp & 0x01;//Return previous scroll mode
				tmp |= (reg_bl & 0x01);
				real_writeb(BIOSMEM_AX_SEG, BIOSMEM_AX_JPNSTATUS, tmp);
			}
			break;
		default:
			LOG(LOG_INT10,LOG_ERROR)("Unhandled AX Function %X",reg_al);
			break;
		}
		break;
	case 0x83:// Read the video RAM address and virtual text video RAM
		if (J3_IsJapanese()) {
			// AX=graphics offset, BX=text offset
			reg_ax = 0;
			reg_bx = 0;
			break;
		}
		//Out: AX=base address of video RAM, ES:BX=base address of virtual text video RAM
		if(INT10_AX_GetCRTBIOSMode() == 0x01) break;//exit if CRT BIOS is in US mode
		LOG(LOG_INT10,LOG_NORMAL)("AX CRT BIOS 83xxh is called.");
		switch (reg_al) {
		case 0x00:
		{
			reg_ax = CurMode->pstart;
			Bitu vtram_seg = real_readw(BIOSMEM_AX_SEG, BIOSMEM_AX_VTRAM_SEGADDR);
			RealMakeSeg(es, vtram_seg >> 4);
			reg_bx = vtram_seg << 4;
			break;
		}
		default:
			LOG(LOG_INT10,LOG_ERROR)("Unhandled AX Function %X",reg_al);
			break;
		}
		break;
	case 0x85:
		if (J3_IsJapanese()) {
			// Get attr
			reg_al = GetKanjiAttr();
		}
		break;
	case 0x88:
		if (J3_IsJapanese()) {
			// gaiji font table
			reg_bx = 0x0000;
			SegSet16(es, GetGaijiSeg());
		}
		break;
    case 0xdb:
        // ETen call
        // if (reg_al==1) reg_ax = 0;
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
	case 0xfe:
		if(IS_DOSV && DOSV_CheckCJKVideoMode()) {
			reg_di = 0x0000;
			SegSet16(es, GetTextSeg());
		}
		break;
	case 0xff:
		if(IS_DOSV && DOSV_CheckCJKVideoMode()) {
			WriteCharTopView(reg_di, reg_cx);
		} else {
			if (!warned_ff) LOG(LOG_INT10,LOG_NORMAL)("INT10:FF:Weird NC call");
			warned_ff=true;
		}
		break;
	default:
		LOG(LOG_INT10,LOG_ERROR)("Function %4X not supported",reg_ax);
//		reg_al=0x00;		//Successful, breaks marriage
		break;
	}
	return CBRET_NONE;
}

bool DISP2_Active(void);
static void INT10_Seg40Init(void) {
	// Clear the screen.
	//
	// The byte value at 40:87 (BIOSMEM_VIDEO_CTL) only means something if EGA/VGA.
	// Should be zero for PCjr (which defines it as function key), and Tandy (which does not define it),
	// and MDA/Hercules/CGA (which also does not define this byte).
	//
	// Furthermore, some games such as "Road Runner" by Mindscape use that byte as part of its
	// detection routines to differentiate between CGA, EGA, and Tandy. For the game to work properly
	// in Tandy emulation, this byte must be zero.
	if (IS_EGAVGA_ARCH) {
		real_writeb(BIOSMEM_SEG,BIOSMEM_VIDEO_CTL,0x60);
		if (IS_VGA_ARCH)
			real_writeb(BIOSMEM_SEG,BIOSMEM_CHAR_HEIGHT,16);
		else
			real_writeb(BIOSMEM_SEG,BIOSMEM_CHAR_HEIGHT,ega200 ? 8 : 14);
		// NTS: This is direclty from the configuration switches. The bits are inverted,
		//      1=OFF 0=ON.
		//
		//      sw1 sw2 sw3 sw4     sw4321
		//      ----------------------------------------------------------------------
		//      ON  ON  ON  ON        0000        Color 40x25 EGA
		//      OFF ON  ON  ON        0001        Color 80x25 EGA
		//      ON  OFF ON  ON        0010        Enhanced Display Emulation Mode
		//      OFF OFF ON  ON        0011        Enhanced Display Hi Res Mode
		//      ON  ON  OFF ON        0100        Monochrome (CGA is primary 40x25)
		//      OFF ON  OFF ON        0101        Monochrome (CGA is primary 80x25)
		//      ON  OFF OFF ON        0110        Color 40x25 EGA (MDA secondary)
		//      OFF OFF OFF ON        0111        Color 80x25 EGA (MDA secondary)
		//      ON  ON  ON  OFF       1000        Enhanced Display Emulation Mode (MDA secondary)
		//      OFF ON  ON  OFF       1001        Enhanced Display Hi Res Mode (MDA secondary)
		//      ON  OFF ON  OFF       1010        Monochrome (CGA is secondary 40x25)
		//      OFF OFF ON  OFF       1011        Monochrome (CGA is secondary 80x25)
		//
		//      [http://hackipedia.org/browse.cgi/Computer/Platform/PC%2c%20IBM%20compatible/Video/EGA/IBM/IBM%20Enhanced%20Graphics%20Adapter%20%281984%2d08%2d02%29%2epdf]
		//
		//      Anything else is not valid.
		real_writeb(BIOSMEM_SEG,BIOSMEM_SWITCHES,0xF0 | ((!IS_VGA_ARCH && ega200)?0x7:0x9));
		// Set the pointer to video save pointer table
		real_writed(BIOSMEM_SEG, BIOSMEM_VS_POINTER, int10.rom.video_save_pointers);
	}
	else {
		real_writeb(BIOSMEM_SEG,BIOSMEM_VIDEO_CTL,0x00);
		if (machine == MCH_TANDY || machine == MCH_PCJR)
			real_writeb(BIOSMEM_SEG,BIOSMEM_CHAR_HEIGHT,8); /* FIXME: INT 10h teletext routines depend on this */
		else
			real_writeb(BIOSMEM_SEG,BIOSMEM_CHAR_HEIGHT,0);
		real_writeb(BIOSMEM_SEG,BIOSMEM_SWITCHES,0x00);
	}
#if C_DEBUG
	if (control->opt_display2)
		real_writeb(BIOSMEM_SEG,BIOSMEM_MODESET_CTL,0x10|(DISP2_Active()?0:1));
	else
#endif
	if (IS_EGAVGA_ARCH) {
		real_writeb(BIOSMEM_SEG,BIOSMEM_MODESET_CTL,0x51); // why is display switching enabled (bit 6) ?
	}
	// Set the default MSR
	real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_MSR,0x09);
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
		static uint8_t TandyConfig[130]= {
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

Bitu ROMBIOS_GetMemory(Bitu bytes,const char *who,Bitu alignment,Bitu must_be_at);
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

FILE *Try_Load_FontFile(std::string filename) {
    FILE *fp;
    std::string confdir,resdir,tmpdir,exepath;

    /* try to load file from working directory */
    if ((fp = fopen(filename.c_str(),"rb")))
        return fp;

    /* try to load file from program directory */
    exepath=GetDOSBoxXPath();
    if (!exepath.empty()) {
        tmpdir = exepath + filename;
        if ((fp = fopen(tmpdir.c_str(),"rb")))
            return fp;
    }

    /* try to load file from user config directory */
    Cross::GetPlatformConfigDir(confdir);
    if (!confdir.empty()) {
        tmpdir = confdir + filename;
        if ((fp = fopen(tmpdir.c_str(),"rb")))
            return fp;
    }

    /* try to load file from resources directory */
    Cross::GetPlatformResDir(resdir);
    if (!resdir.empty()) {
        tmpdir = resdir + filename;
        if ((fp = fopen(tmpdir.c_str(),"rb")))
            return fp;
    }

    return nullptr;
}

FILE *Try_Load_FontFiles(std::vector<std::string> filenames) {
    FILE *fp;
    for (auto filename : filenames) {
        if ((fp = Try_Load_FontFile(filename)))
            return fp;
    }

    return nullptr;
}

bool Load_FONT_ROM(void) {
    unsigned int hibyte,lowbyte,r;
    unsigned char tmp[256*16]; // 8x16 256 cells

    FILE *fp = Try_Load_FontFiles({"FONT.ROM", "font.rom", "FONT.rom", "font.ROM"});
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
void Load_Anex86_Font(const char *fontname, bool &sbcs_ok, bool &dbcs_ok) {
    unsigned char tmp[(2048/8)*16]; /* enough for one 2048x16 row and bitmap header */
    unsigned int hibyte,lowbyte,r;
    unsigned int bmp_ofs;
    FILE *fp = NULL;

    /* attempt to load user-specified font file */
    if (fontname&&*fontname) {
        std::string name = fontname;
        ResolvePath(name);
        fp = fopen(name.c_str(),"rb");
    }

    /* attempt to load anex86/freecg98 font files */
    if (!fp) {
        fp = Try_Load_FontFiles(
                {
                    /* ANEX86.BMP accurate dump of actual font */
                    "anex86.bmp",
                    "ANEX86.bmp",
                    "ANEX86.BMP",

                    /* FREECG98.BMP free open source generated copy from system fonts */
                    "freecg98.bmp",
                    "FREECG98.bmp",
                    "FREECG98.BMP",
                }
        );
    }

    if (!fp) {
        LOG_MSG("PC-98 font loading: neither ANEX86.BMP nor FREECG98.BMP found");
        return;
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
    if(!sbcs_ok) {
        for (lowbyte=0;lowbyte < 256;lowbyte++) {
            for (r=0;r < 16;r++) {
                vga.draw.font[(lowbyte*16)+r] = tmp[lowbyte+((15-r/*upside-down!*/)*(2048/8))] ^ 0xFF/*ANEX86 has inverse color scheme*/;
            }
        }
        sbcs_ok = true;
    }
    /* everything after is 16x16 fullwidth.
     * note: 2048 / 16 = 128 */
    if(!dbcs_ok) {
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
        dbcs_ok = true;
    }

    LOG_MSG("ANEX86.BMP/FREECG98.BMP font loaded");
    fclose(fp);
    return;
fail:
    LOG_MSG("ANEX86.BMP/FREECG98.BMP invalid, ignoring");
    fclose(fp);
}

extern VideoModeBlock PC98_Mode;
extern uint8_t pc98_freecg_sbcs[256 * 16];

void Load_JFont_As_PC98(bool &sbcs_ok, bool &dbcs_ok) {
    unsigned int hibyte,lowbyte,r,o,i,i1,i2;

    if(!sbcs_ok) {
        for (lowbyte=0;lowbyte < 256;lowbyte++) {
            for (r=0;r < 16;r++)
                vga.draw.font[(lowbyte*16)+r] = pc98_freecg_sbcs[(lowbyte*16)+r];
        }
        sbcs_ok = true;
    }
    if(!dbcs_ok) {
        for (lowbyte=33;lowbyte < 125;lowbyte++) {
            for (hibyte=32;hibyte < 128;hibyte++) {
                i1=(lowbyte+1)/2+(lowbyte<95?112:176);
                i2=hibyte+(lowbyte%2?31+(hibyte/96):126);
                i = i1 * 0x100 + i2;
                o = ((hibyte * 128) + (lowbyte - 32)) * 16 * 2;
                uint8_t *font = GetDbcsFont(i);
                for (r=0;r < 32;r++) vga.draw.font[o+r] = font[r];
            }
        }
        dbcs_ok = true;
    }
}

extern bool LoadFontxFile(const char *fname, int height, bool dbcs);
extern uint8_t jfont_cache_dbcs_16[];

void Load_FontX2_As_PC98(Section_prop *section, bool &sbcs_ok, bool &dbcs_ok) {
    unsigned int hibyte, lowbyte, r, o, i, i1, i2;

    bool pc98_symbol = section->Get_bool("pc-98 fontx internal symbol");
    Prop_path *pathprop = section->Get_path("pc-98 fontx sbcs");
    if(pathprop) {
        std::string path = pathprop->realpath;
        ResolvePath(path);
        if(LoadFontxFile(path.c_str(), 16, false)) {
            uint8_t *font = GetSbcsFont(0);
            memcpy(vga.draw.font, font, 16 * 256);
            bool flag = true;
            // In the case of a normal FONTX2 file for DOS/V, character codes 80h to AFh are empty.
            // For FONTX2 files created using MKXFNT98.EXE, character codes 80h to AFh are not empty.
            for(r = 0x80 * 16 ; r < 0xa0 * 16 ; r++) {
                if(font[r] != 0) {
                    flag = false;
                    break;
                }
            }
            // When judged as a FONTX2 file for DOS/V, character codes 00h to 1Fh, 80h to AFh,
            // and E0h to FFh use font data for PC-98.
            if(flag || pc98_symbol) {
                memcpy(&vga.draw.font, &pc98_freecg_sbcs, 0x20 * 16);
                memcpy(&vga.draw.font[0x80 * 16], &pc98_freecg_sbcs[0x80 * 16], 0x20 * 16);
                memcpy(&vga.draw.font[0xe0 * 16], &pc98_freecg_sbcs[0xe0 * 16], 0x20 * 16);
            }
            sbcs_ok = true;
        }
    }
    pathprop = section->Get_path("pc-98 fontx dbcs");
    if(pathprop) {
        std::string path = pathprop->realpath;
        ResolvePath(path);
        if(LoadFontxFile(path.c_str(), 16, true)) {
            uint8_t *font = GetDbcsFont(0x8645);
            bool fontx98_flag = true;
            bool empty_flag = true;
            for(r = 0 ; r < 32 ; r++) {
                if(font[r] != 0) {
                    empty_flag = false;
                    if(r >= 16) {
                        fontx98_flag = false;
                    }
                }
            }
            for(lowbyte = 33 ; lowbyte < 125 ; lowbyte++) {
                for(hibyte = 32 ; hibyte < 128 ; hibyte++) {
                    i1 = (lowbyte + 1) / 2 + (lowbyte < 95 ? 112 : 176);
                    i2 = hibyte + (lowbyte % 2 ? 31 + (hibyte / 96) : 126);
                    i = i1 * 0x100 + i2;
                    o = ((hibyte * 128) + (lowbyte - 32)) * 16 * 2;
                    if(i >= 0x8640 && i <= 0x868f && (empty_flag || pc98_symbol)) {
                        // Use internal font data for 2byte hankaku characters.
                        jfont_cache_dbcs_16[i] = 0;
                    }
                    font = GetDbcsFont(i);
                    if(i >= 0x8640 && i <= 0x868f && fontx98_flag) {
                        // For FONTX2 files created with MKXFNT98.EXE,
                        // the data format of 2byte hankaku characters is different.
                        for(r = 0 ; r < 16 ; r++) vga.draw.font[o + r * 2] = font[r];
                    } else {
                        memcpy(&vga.draw.font[o], font, 32);
                    }
                }
            }
            dbcs_ok = true;
        }
    }
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

Bitu INT10_AX_GetCRTBIOSMode(void) {
	if (!IS_JEGA_ARCH) return 0x01;
	if (real_readb(BIOSMEM_AX_SEG, BIOSMEM_AX_JPNSTATUS) & 0x80) return 0x51;//if in US mode
	else return 0x01;
}

bool INT10_AX_SetCRTBIOSMode(Bitu mode) {
	if (!IS_JEGA_ARCH) return false;
	uint8_t tmp = real_readb(BIOSMEM_AX_SEG, BIOSMEM_AX_JPNSTATUS);
	switch (mode) {
		//Todo: verify written value
	case 0x01:
		real_writeb(BIOSMEM_AX_SEG, BIOSMEM_AX_JPNSTATUS, tmp & 0x7F);
		LOG(LOG_INT10, LOG_NORMAL)("AX CRT BIOS has been set to US mode.");
		INT10_SetVideoMode(0x03);
		return true;
		/* -------------------SET to JP mode in CRT BIOS -------------------- */
	case 0x51:
		real_writeb(BIOSMEM_AX_SEG, BIOSMEM_AX_JPNSTATUS, tmp | 0x80);
		LOG(LOG_INT10, LOG_NORMAL)("AX CRT BIOS has been set to JP mode.");
		//		Mouse_BeforeNewVideoMode(true);
		// change to the default video mode (03h) with vram cleared
		INT10_SetVideoMode(0x03);
		//		Mouse_AfterNewVideoMode(true);;
		return true;
	default:
		return false;
	}
}
Bitu INT16_AX_GetKBDBIOSMode(void) {
	if (!IS_JEGA_ARCH) return 0x01;
	if (real_readb(BIOSMEM_AX_SEG, BIOSMEM_AX_JPNSTATUS) & 0x40) return 0x51;//if in US mode
	else return 0x01;
}

bool INT16_AX_SetKBDBIOSMode(Bitu mode) {
	if (!IS_JEGA_ARCH) return false;
	uint8_t tmp = real_readb(BIOSMEM_AX_SEG, BIOSMEM_AX_JPNSTATUS);
	switch (mode) {
		//Todo: verify written value
	case 0x01:
		real_writeb(BIOSMEM_AX_SEG, BIOSMEM_AX_JPNSTATUS, tmp & 0xBF);
		LOG(LOG_INT10, LOG_NORMAL)("AX KBD BIOS has been set to US mode.");
		return true;
	case 0x51:
		real_writeb(BIOSMEM_AX_SEG, BIOSMEM_AX_JPNSTATUS, tmp | 0x40);
		LOG(LOG_INT10, LOG_NORMAL)("AX KBD BIOS has been set to JP mode.");
		return true;
	default:
		return false;
	}
}

extern bool VGA_BIOS_use_rom;

void INT10_Startup(Section *sec) {
    (void)sec;//UNUSED
	LOG(LOG_MISC,LOG_DEBUG)("INT 10h reinitializing");

    Section_prop * video_section = static_cast<Section_prop *>(control->GetSection("video"));

	vesa_set_display_vsync_wait = video_section->Get_int("vesa set display vsync");
	vesa_bank_switch_window_range_check = video_section->Get_bool("vesa bank switching window range check");
	vesa_bank_switch_window_mirror = video_section->Get_bool("vesa bank switching window mirroring");
	vesa_zero_on_get_information = video_section->Get_bool("vesa zero buffer on get information");
	unmask_irq0_on_int10_setmode = video_section->Get_bool("unmask timer on int 10 setmode");
	int16_unmask_irq1_on_read = static_cast<Section_prop *>(control->GetSection("dosbox"))->Get_bool("unmask keyboard on int 16 read");
	int16_ah_01_cf_undoc = static_cast<Section_prop *>(control->GetSection("dosbox"))->Get_bool("int16 keyboard polling undocumented cf behavior");
	int10_vga_bios_vector = video_section->Get_bool("int 10h points at vga bios");
	int size_override = video_section->Get_int("vga bios size override");

	Section_prop * pc98_section = static_cast<Section_prop *>(control->GetSection("pc98"));
	bool try_font_rom = pc98_section->Get_bool("pc-98 try font rom");
	const char *anex86_font = pc98_section->Get_string("pc-98 anex86 font");

    if (!IS_PC98_ARCH) {
        if (!VGA_BIOS_use_rom) {
            INT10_InitVGA();
            if (IS_TANDY_ARCH) SetupTandyBios();
            /* Setup the INT 10 vector */
            call_10=CALLBACK_Allocate();
            CALLBACK_Setup(call_10,&INT10_Handler,CB_IRET,"Int 10 video");
            RealSetVec(0x10,CALLBACK_RealPointer(call_10));
            //Init the 0x40 segment and init the datastructures in the video rom area
            INT10_SetupRomMemory();
            INT10_Seg40Init();
            INT10_SetupBasicVideoParameterTable();

            LOG(LOG_MISC,LOG_DEBUG)("INT 10: VGA bios used %d / %d memory",(int)int10.rom.used,(int)VGA_BIOS_Size);
            if (int10.rom.used > VGA_BIOS_Size && size_override > -512) /* <- this is fatal, it means the Setup() functions scrozzled over the adjacent ROM or RAM area */
                E_Exit("VGA BIOS size too small %u > %u",(unsigned int)int10.rom.used,(unsigned int)VGA_BIOS_Size);
        }

	/* If machine=vgaonly, make a dummy stub routine at 0xF000:0xF065 if the user is using the stock IBM VGA ROM BIOS.
	 * It calls INT 42h for unknown functions. INT 42h is NOT the previous INT 10h handler but is a hardcoded address
	 * specific to VGA-based IBM PCs of the time period. */
	if (IS_VGA_ARCH && svgaCard == SVGA_None && VGA_BIOS_use_rom) {
		LOG(LOG_MISC,LOG_DEBUG)("Creating dummy IBM BIOS INT 10h stub for VGA BIOS ROM image at F000:F065");
		// TODO: This may have to identify the ROM image by file name in case other ROM images have other
		//       specific requirements or hardcoded addresses.
		PhysPt base = ROMBIOS_GetMemory(3,"IBM BIOS dummy INT 10h stub for VGA BIOS",/*align*/1,0xFF065); /* 0xF000:0xF065 */
		phys_writew(base+0,0xC031);		// XOR AX,AX
		phys_writeb(base+2,0xCF);		// IRET
	}

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
        SetTrueVideoMode(0x03);
    }
    else {
        /* load PC-98 character ROM data, if possible */
        {
            /* We can use FONT.ROM as generated by T98Tools */
            bool sbcs_ok = try_font_rom?Load_FONT_ROM():false;
            bool dbcs_ok = sbcs_ok;
             /* We can use FONTX2 font file */
            if(!sbcs_ok) Load_FontX2_As_PC98(pc98_section, sbcs_ok, dbcs_ok);
            /* We can use ANEX86.BMP from the Anex86 emulator */
            if(!sbcs_ok || !dbcs_ok) Load_Anex86_Font(anex86_font, sbcs_ok, dbcs_ok);
            /* Failing all else we can use the internal FREECG 8x16 font and default Japanese font to show text on the screen. */
            if(!sbcs_ok || !dbcs_ok) Load_JFont_As_PC98(sbcs_ok, dbcs_ok);
        }

        CurMode = &PC98_Mode;

        /* FIXME: This belongs in MS-DOS kernel init, because these reside in the CON driver */
        /* Some PC-98 game behavior seems to suggest the BIOS data area stretches all the way from segment 0x40:0x00 to segment 0x7F:0x0F inclusive.
         * Compare that to IBM PC platform, where segment fills only 0x40:0x00 to 0x50:0x00 inclusive and extra state is held in the "Extended BIOS Data Area".
         */

        real_writeb(0x60,0x8A,1); /* kanji/graph mode */
        real_writeb(0x60,0x8B,' '); /* kanji/graph mode indicator */
        real_writeb(0x60,0x8C,' '); /* function row mode 2 indicator */

        /* number of text rows on the screen.
         * Touhou Project will not clear/format the text layer properly without this variable. */
        real_writeb(0x60,0x110,0); /* cursor position Y coordinate */
        real_writeb(0x60,0x111,1); /* function definition display status flag */
        real_writeb(0x60,0x112,25 - 1 - 1); /* scroll range lower limit (usually 23 when function key row is visible) */
        real_writeb(0x60,0x113,1); /* normal 25 lines */
        real_writeb(0x60,0x114,0xE1); /* content erase attribute */

        real_writeb(0x60,0x119,0x20); /* content erase character */

        real_writeb(0x60,0x11B,0x01); /* cursor displayed */
        real_writeb(0x60,0x11C,0x00); /* cursor position X coordinate */
        real_writeb(0x60,0x11D,0xE1); /* content display attribute */
        real_writeb(0x60,0x11E,0x00); /* scroll range upper limit (usually 0) */
        real_writeb(0x60,0x11F,0x01); /* scrolling speed is normal */

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
