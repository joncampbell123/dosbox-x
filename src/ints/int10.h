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


#include "vga.h"

extern uint32_t S3_LFB_BASE;

#define BIOSMEM_SEG		0x40u

#define BIOSMEM_INITIAL_MODE  0x10u
#define BIOSMEM_CURRENT_MODE  0x49u
#define BIOSMEM_NB_COLS       0x4Au
#define BIOSMEM_PAGE_SIZE     0x4Cu
#define BIOSMEM_CURRENT_START 0x4Eu
#define BIOSMEM_CURSOR_POS    0x50u
#define BIOSMEM_CURSOR_TYPE   0x60u
#define BIOSMEM_CURRENT_PAGE  0x62u
#define BIOSMEM_CRTC_ADDRESS  0x63u
#define BIOSMEM_CURRENT_MSR   0x65u
#define BIOSMEM_CURRENT_PAL   0x66u
#define BIOSMEM_NB_ROWS       0x84u
#define BIOSMEM_CHAR_HEIGHT   0x85u
#define BIOSMEM_VIDEO_CTL     0x87u
#define BIOSMEM_SWITCHES      0x88u
#define BIOSMEM_MODESET_CTL   0x89u
#define BIOSMEM_DCC_INDEX     0x8Au
#define BIOSMEM_CRTCPU_PAGE   0x8Au
#define BIOSMEM_VS_POINTER    0xA8u


/*
 *
 * VGA registers
 *
 */
#define VGAREG_ACTL_ADDRESS            0x3c0u
#define VGAREG_ACTL_WRITE_DATA         0x3c0u
#define VGAREG_ACTL_READ_DATA          0x3c1u

#define VGAREG_INPUT_STATUS            0x3c2u
#define VGAREG_WRITE_MISC_OUTPUT       0x3c2u
#define VGAREG_VIDEO_ENABLE            0x3c3u
#define VGAREG_SEQU_ADDRESS            0x3c4u
#define VGAREG_SEQU_DATA               0x3c5u

#define VGAREG_PEL_MASK                0x3c6u
#define VGAREG_DAC_STATE               0x3c7u
#define VGAREG_DAC_READ_ADDRESS        0x3c7u
#define VGAREG_DAC_WRITE_ADDRESS       0x3c8u
#define VGAREG_DAC_DATA                0x3c9u

#define VGAREG_READ_FEATURE_CTL        0x3cau
#define VGAREG_READ_MISC_OUTPUT        0x3ccu

#define VGAREG_GRDC_ADDRESS            0x3ceu
#define VGAREG_GRDC_DATA               0x3cfu

#define VGAREG_MDA_CRTC_ADDRESS        0x3b4u
#define VGAREG_MDA_CRTC_DATA           0x3b5u
#define VGAREG_VGA_CRTC_ADDRESS        0x3d4u
#define VGAREG_VGA_CRTC_DATA           0x3d5u

#define VGAREG_MDA_WRITE_FEATURE_CTL   0x3bau
#define VGAREG_VGA_WRITE_FEATURE_CTL   0x3dau
#define VGAREG_ACTL_RESET              0x3dau
#define VGAREG_TDY_RESET               0x3dau
#define VGAREG_TDY_ADDRESS             0x3dau
#define VGAREG_TDY_DATA                0x3deu
#define VGAREG_PCJR_DATA               0x3dau

#define VGAREG_MDA_MODECTL             0x3b8u
#define VGAREG_CGA_MODECTL             0x3d8u
#define VGAREG_CGA_PALETTE             0x3d9u

/* Video memory */
#define VGAMEM_GRAPH 0xA000u
#define VGAMEM_CTEXT 0xB800u
#define VGAMEM_MTEXT 0xB000u

/* FIXME: Wait, what?? What the hell kind of preprocessor macro is this??? Kill these macros! --J.C. */
#define BIOS_NCOLS Bit16u ncols=IS_PC98_ARCH ? 80 : real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS);
#define BIOS_NROWS Bit16u nrows=IS_PC98_ARCH ? (Bit16u)(real_readb(0x60,0x112)+1u) : (Bit16u)real_readb(BIOSMEM_SEG,BIOSMEM_NB_ROWS)+1u;

extern Bit8u int10_font_08[256 * 8];
extern Bit8u int10_font_14[256 * 14];
extern Bit8u int10_font_16[256 * 16];
extern Bit8u int10_font_14_alternate[20 * 15 + 1];
extern Bit8u int10_font_16_alternate[19 * 17 + 1];

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
        RealPt video_dynamic_save_area;
		RealPt video_parameter_table;
		RealPt video_save_pointer_table;
		RealPt video_dcc_table;
		RealPt oemstring;
		RealPt vesa_modes;
		RealPt wait_retrace;
		RealPt set_window;
		RealPt pmode_interface;
		Bit16u pmode_interface_size;
		Bit16u pmode_interface_start;
		Bit16u pmode_interface_window;
		Bit16u pmode_interface_palette;
        Bit16u vesa_alloc_modes;
		Bit16u used;
	} rom;
	Bit16u vesa_setmode;
	bool vesa_nolfb;
	bool vesa_oldvbe;
} Int10Data;

#define _EGA_HALF_CLOCK			0x0001
#define _DOUBLESCAN			    0x0002  /* CGA/EGA on VGA doublescan (bit 7 of max scanline) */
#define _VGA_PIXEL_DOUBLE		0x0004
#define _S3_PIXEL_DOUBLE		0x0008
#define _REPEAT1			    0x0010  /* VGA doublescan (bit 0 of max scanline) */
#define _CGA_SYNCDOUBLE			0x0020
#define _HIGH_DEFINITION        0x0040
#define _UNUSUAL_MODE           0x0080
#define _USER_DISABLED          0x4000  /* disabled (cannot set mode) but still listed in modelist */
#define _USER_MODIFIED          0x8000  /* user modified (through VESAMOED) */

extern Int10Data int10;

static inline Bit8u CURSOR_POS_COL(Bit8u page) {
    if (IS_PC98_ARCH)
        return real_readb(0x60,0x11C); /* MS-DOS kernel location */
    else
    	return real_readb(BIOSMEM_SEG,BIOSMEM_CURSOR_POS+page*2u);
}

static inline Bit8u CURSOR_POS_ROW(Bit8u page) {
    if (IS_PC98_ARCH)
        return real_readb(0x60,0x110); /* MS-DOS kernel location */
    else
        return real_readb(BIOSMEM_SEG,BIOSMEM_CURSOR_POS+page*2u+1u);
}

//! \brief Gets the state of INS/OVR mode.
bool INT10_GetInsertState();

bool INT10_SetVideoMode(Bit16u mode);

void INT10_ScrollWindow(Bit8u rul,Bit8u cul,Bit8u rlr,Bit8u clr,Bit8s nlines,Bit8u attr,Bit8u page);

void INT10_SetActivePage(Bit8u page);
bool INT10_SetCurMode(void);
void INT10_DisplayCombinationCode(Bit16u * dcc,bool set);
void INT10_GetFuncStateInformation(PhysPt save);

void INT10_SetCursorShape(Bit8u first,Bit8u last);
void INT10_GetScreenColumns(Bit16u* cols);
void INT10_GetCursorPos(Bit8u *row, Bit8u *col, Bit8u page);
void INT10_SetCursorPos(Bit8u row,Bit8u col,Bit8u page);
void INT10_TeletypeOutput(Bit8u chr,Bit8u attr);
void INT10_TeletypeOutputAttr(Bit8u chr,Bit8u attr,bool useattr);
void INT10_ReadCharAttr(Bit16u * result,Bit8u page);
void INT10_WriteChar(Bit16u chr,Bit8u attr,Bit8u page,Bit16u count,bool showattr);
void INT10_WriteString(Bit8u row,Bit8u col,Bit8u flag,Bit8u attr,PhysPt string,Bit16u count,Bit8u page);

/* Graphics Stuff */
void INT10_PutPixel(Bit16u x,Bit16u y,Bit8u page,Bit8u color);
void INT10_GetPixel(Bit16u x,Bit16u y,Bit8u page,Bit8u * color);

/* Font Stuff */
void INT10_LoadFont(PhysPt font,bool reload,Bit16u count,Bitu offset,Bitu map,Bit8u height);
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
void INT10_SetSingleDACRegister(Bit8u index,Bit8u red,Bit8u green,Bit8u blue);
void INT10_GetSingleDACRegister(Bit8u index,Bit8u * red,Bit8u * green,Bit8u * blue);
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
Bit8u VESA_SetDisplayStart(Bit16u x,Bit16u y,bool wait);
Bit8u VESA_GetDisplayStart(Bit16u & x,Bit16u & y);
Bit8u VESA_SetPalette(PhysPt data,Bitu index,Bitu count,bool wait);
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
void INT10_EGA_RIL_WriteRegisterRange(Bit8u ch, Bit8u cl, Bit16u dx, PhysPt src);
void INT10_EGA_RIL_ReadRegisterSet(Bit16u cx, PhysPt tbl);
void INT10_EGA_RIL_WriteRegisterSet(Bit16u cx, PhysPt tbl);

/* Video State */
Bitu INT10_VideoState_GetSize(Bitu state);
bool INT10_VideoState_Save(Bitu state,RealPt buffer);
bool INT10_VideoState_Restore(Bitu state,RealPt buffer);

/* Video Parameter Tables */
Bit16u INT10_SetupVideoParameterTable(PhysPt basepos);
void INT10_SetupBasicVideoParameterTable(void);
