/*
 *  Copyright (C) 2002-2010  The DOSBox Team
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

/* $Id: int10.h,v 1.42 2009-09-06 19:25:34 c2woody Exp $ */

#include "vga.h"

#define S3_LFB_BASE		0xC0000000

#define BIOSMEM_SEG		0x40

#define BIOSMEM_INITIAL_MODE  0x10
#define BIOSMEM_CURRENT_MODE  0x49
#define BIOSMEM_NB_COLS       0x4A
#define BIOSMEM_PAGE_SIZE     0x4C
#define BIOSMEM_CURRENT_START 0x4E
#define BIOSMEM_CURSOR_POS    0x50
#define BIOSMEM_CURSOR_TYPE   0x60
#define BIOSMEM_CURRENT_PAGE  0x62
#define BIOSMEM_CRTC_ADDRESS  0x63
#define BIOSMEM_CURRENT_MSR   0x65
#define BIOSMEM_CURRENT_PAL   0x66
#define BIOSMEM_NB_ROWS       0x84
#define BIOSMEM_CHAR_HEIGHT   0x85
#define BIOSMEM_VIDEO_CTL     0x87
#define BIOSMEM_SWITCHES      0x88
#define BIOSMEM_MODESET_CTL   0x89
#define BIOSMEM_DCC_INDEX     0x8A
#define BIOSMEM_CRTCPU_PAGE   0x8A
#define BIOSMEM_VS_POINTER    0xA8


/*
 *
 * VGA registers
 *
 */
#define VGAREG_ACTL_ADDRESS            0x3c0
#define VGAREG_ACTL_WRITE_DATA         0x3c0
#define VGAREG_ACTL_READ_DATA          0x3c1

#define VGAREG_INPUT_STATUS            0x3c2
#define VGAREG_WRITE_MISC_OUTPUT       0x3c2
#define VGAREG_VIDEO_ENABLE            0x3c3
#define VGAREG_SEQU_ADDRESS            0x3c4
#define VGAREG_SEQU_DATA               0x3c5

#define VGAREG_PEL_MASK                0x3c6
#define VGAREG_DAC_STATE               0x3c7
#define VGAREG_DAC_READ_ADDRESS        0x3c7
#define VGAREG_DAC_WRITE_ADDRESS       0x3c8
#define VGAREG_DAC_DATA                0x3c9

#define VGAREG_READ_FEATURE_CTL        0x3ca
#define VGAREG_READ_MISC_OUTPUT        0x3cc

#define VGAREG_GRDC_ADDRESS            0x3ce
#define VGAREG_GRDC_DATA               0x3cf

#define VGAREG_MDA_CRTC_ADDRESS        0x3b4
#define VGAREG_MDA_CRTC_DATA           0x3b5
#define VGAREG_VGA_CRTC_ADDRESS        0x3d4
#define VGAREG_VGA_CRTC_DATA           0x3d5

#define VGAREG_MDA_WRITE_FEATURE_CTL   0x3ba
#define VGAREG_VGA_WRITE_FEATURE_CTL   0x3da
#define VGAREG_ACTL_RESET              0x3da
#define VGAREG_TDY_RESET               0x3da
#define VGAREG_TDY_ADDRESS             0x3da
#define VGAREG_TDY_DATA                0x3de
#define VGAREG_PCJR_DATA               0x3da

#define VGAREG_MDA_MODECTL             0x3b8
#define VGAREG_CGA_MODECTL             0x3d8
#define VGAREG_CGA_PALETTE             0x3d9

/* Video memory */
#define VGAMEM_GRAPH 0xA000
#define VGAMEM_CTEXT 0xB800
#define VGAMEM_MTEXT 0xB000

#define BIOS_NCOLS Bit16u ncols=real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS);
#define BIOS_NROWS Bit16u nrows=(Bit16u)real_readb(BIOSMEM_SEG,BIOSMEM_NB_ROWS)+1;

extern Bit8u int10_font_08[256 * 8];
extern Bit8u int10_font_14[256 * 14];
extern Bit8u int10_font_16[256 * 16];

struct VideoModeBlock {
	Bit16u	mode;
	VGAModes	type;
	Bitu	swidth, sheight;
	Bitu	twidth, theight;
	Bitu	cwidth, cheight;
	Bitu	ptotal,pstart,plength;

	Bitu	htotal,vtotal;
	Bitu	hdispend,vdispend;
	Bitu	special;
	
};
extern VideoModeBlock ModeList_VGA[];
extern VideoModeBlock * CurMode;

typedef struct {
	struct {
		RealPt font_8_first;
		RealPt font_8_second;
		RealPt font_14;
		RealPt font_16;
		RealPt font_14_alternate;
		RealPt font_16_alternate;
		RealPt static_state;
		RealPt video_save_pointers;
		RealPt video_parameter_table;
		RealPt video_save_pointer_table;
		RealPt video_dcc_table;
		RealPt oemstring;
		RealPt vesa_modes;
		RealPt pmode_interface;
		Bit16u pmode_interface_size;
		Bit16u pmode_interface_start;
		Bit16u pmode_interface_window;
		Bit16u pmode_interface_palette;
		Bit16u used;
	} rom;
	Bit16u vesa_setmode;
	bool vesa_nolfb;
	bool vesa_oldvbe;
} Int10Data;

extern Int10Data int10;

static Bit8u CURSOR_POS_COL(Bit8u page) {
	return real_readb(BIOSMEM_SEG,BIOSMEM_CURSOR_POS+page*2);
}

static Bit8u CURSOR_POS_ROW(Bit8u page) {
	return real_readb(BIOSMEM_SEG,BIOSMEM_CURSOR_POS+page*2+1);
}

bool INT10_SetVideoMode(Bit16u mode);

void INT10_ScrollWindow(Bit8u rul,Bit8u cul,Bit8u rlr,Bit8u clr,Bit8s nlines,Bit8u attr,Bit8u page);

void INT10_SetActivePage(Bit8u page);
void INT10_GetFuncStateInformation(PhysPt save);

void INT10_SetCursorShape(Bit8u first,Bit8u last);
void INT10_SetCursorPos(Bit8u row,Bit8u col,Bit8u page);
void INT10_TeletypeOutput(Bit8u chr,Bit8u attr);
void INT10_TeletypeOutputAttr(Bit8u chr,Bit8u attr,bool useattr);
void INT10_ReadCharAttr(Bit16u * result,Bit8u page);
void INT10_WriteChar(Bit8u chr,Bit8u attr,Bit8u page,Bit16u count,bool showattr);
void INT10_WriteString(Bit8u row,Bit8u col,Bit8u flag,Bit8u attr,PhysPt string,Bit16u count,Bit8u page);

/* Graphics Stuff */
void INT10_PutPixel(Bit16u x,Bit16u y,Bit8u page,Bit8u color);
void INT10_GetPixel(Bit16u x,Bit16u y,Bit8u page,Bit8u * color);

/* Font Stuff */
void INT10_LoadFont(PhysPt font,bool reload,Bitu count,Bitu offset,Bitu map,Bitu height);
void INT10_ReloadFont(void);

/* Palette Group */
void INT10_SetBackgroundBorder(Bit8u val);
void INT10_SetColorSelect(Bit8u val);
void INT10_SetSinglePaletteRegister(Bit8u reg,Bit8u val);
void INT10_SetOverscanBorderColor(Bit8u val);
void INT10_SetAllPaletteRegisters(PhysPt data);
void INT10_ToggleBlinkingBit(Bit8u state);
void INT10_GetSinglePaletteRegister(Bit8u reg,Bit8u * val);
void INT10_GetOverscanBorderColor(Bit8u * val);
void INT10_GetAllPaletteRegisters(PhysPt data);
void INT10_SetSingleDacRegister(Bit8u index,Bit8u red,Bit8u green,Bit8u blue);
void INT10_GetSingleDacRegister(Bit8u index,Bit8u * red,Bit8u * green,Bit8u * blue);
void INT10_SetDACBlock(Bit16u index,Bit16u count,PhysPt data);
void INT10_GetDACBlock(Bit16u index,Bit16u count,PhysPt data);
void INT10_SelectDACPage(Bit8u function,Bit8u mode);
void INT10_GetDACPage(Bit8u* mode,Bit8u* page);
void INT10_SetPelMask(Bit8u mask);
void INT10_GetPelMask(Bit8u & mask);
void INT10_PerformGrayScaleSumming(Bit16u start_reg,Bit16u count);


/* Vesa Group */
Bit8u VESA_GetSVGAInformation(Bit16u seg,Bit16u off);
Bit8u VESA_GetSVGAModeInformation(Bit16u mode,Bit16u seg,Bit16u off);
Bit8u VESA_SetSVGAMode(Bit16u mode);
Bit8u VESA_GetSVGAMode(Bit16u & mode);
Bit8u VESA_SetCPUWindow(Bit8u window,Bit8u address);
Bit8u VESA_GetCPUWindow(Bit8u window,Bit16u & address);
Bit8u VESA_ScanLineLength(Bit8u subcall, Bit16u val, Bit16u & bytes,Bit16u & pixels,Bit16u & lines);
Bit8u VESA_SetDisplayStart(Bit16u x,Bit16u y);
Bit8u VESA_GetDisplayStart(Bit16u & x,Bit16u & y);
Bit8u VESA_SetPalette(PhysPt data,Bitu index,Bitu count);
Bit8u VESA_GetPalette(PhysPt data,Bitu index,Bitu count);

/* Sub Groups */
void INT10_SetupRomMemory(void);
void INT10_SetupRomMemoryChecksum(void);
void INT10_SetupVESA(void);

/* EGA RIL */
RealPt INT10_EGA_RIL_GetVersionPt(void);
void INT10_EGA_RIL_ReadRegister(Bit8u & bl, Bit16u dx);
void INT10_EGA_RIL_WriteRegister(Bit8u & bl, Bit8u bh, Bit16u dx);
void INT10_EGA_RIL_ReadRegisterRange(Bit8u ch, Bit8u cl, Bit16u dx, PhysPt dst);
void INT10_EGA_RIL_WriteRegisterRange(Bit8u ch, Bit8u cl, Bit16u dx, PhysPt dst);
void INT10_EGA_RIL_ReadRegisterSet(Bit16u cx, PhysPt tbl);
void INT10_EGA_RIL_WriteRegisterSet(Bit16u cx, PhysPt tbl);

/* Video State */
Bitu INT10_VideoState_GetSize(Bitu state);
bool INT10_VideoState_Save(Bitu state,RealPt buffer);
bool INT10_VideoState_Restore(Bitu state,RealPt buffer);

/* Video Parameter Tables */
Bit16u INT10_SetupVideoParameterTable(PhysPt basepos);
void INT10_SetupBasicVideoParameterTable(void);
