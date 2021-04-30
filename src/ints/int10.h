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
#define BIOS_NCOLS uint16_t ncols=IS_PC98_ARCH ? 80 : real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS);
#define BIOS_NROWS uint16_t nrows=IS_PC98_ARCH ? (uint16_t)(real_readb(0x60,0x112)+1u) : IS_EGAVGA_ARCH?((uint16_t)real_readb(BIOSMEM_SEG,BIOSMEM_NB_ROWS)+1):25;
#define BIOS_CHEIGHT uint8_t cheight=IS_EGAVGA_ARCH?real_readb(BIOSMEM_SEG,BIOSMEM_CHAR_HEIGHT):8;

extern uint8_t int10_font_08[256 * 8];
extern uint8_t int10_font_14[256 * 14];
extern uint8_t int10_font_16[256 * 16];
extern uint8_t int10_font_14_alternate[20 * 15 + 1];
extern uint8_t int10_font_16_alternate[19 * 17 + 1];

struct VideoModeBlock {
	uint16_t	mode;
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
		uint16_t pmode_interface_size;
		uint16_t pmode_interface_start;
		uint16_t pmode_interface_window;
		uint16_t pmode_interface_palette;
        uint16_t vesa_alloc_modes;
		uint16_t used;
	} rom;
	uint16_t vesa_setmode;
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

static inline uint8_t CURSOR_POS_COL(uint8_t page) {
    if (IS_PC98_ARCH)
        return real_readb(0x60,0x11C); /* MS-DOS kernel location */
    else
    	return real_readb(BIOSMEM_SEG,BIOSMEM_CURSOR_POS+page*2u);
}

static inline uint8_t CURSOR_POS_ROW(uint8_t page) {
    if (IS_PC98_ARCH)
        return real_readb(0x60,0x110); /* MS-DOS kernel location */
    else
        return real_readb(BIOSMEM_SEG,BIOSMEM_CURSOR_POS+page*2u+1u);
}

//! \brief Gets the state of INS/OVR mode.
bool INT10_GetInsertState();

bool INT10_SetVideoMode(uint16_t mode);

void INT10_ScrollWindow(uint8_t rul,uint8_t cul,uint8_t rlr,uint8_t clr,int8_t nlines,uint8_t attr,uint8_t page);

void INT10_SetActivePage(uint8_t page);
bool INT10_SetCurMode(void);
void INT10_DisplayCombinationCode(uint16_t * dcc,bool set);
void INT10_GetFuncStateInformation(PhysPt save);

void INT10_SetCursorShape(uint8_t first,uint8_t last);
void INT10_GetScreenColumns(uint16_t* cols);
void INT10_GetCursorPos(uint8_t *row, uint8_t *col, uint8_t page);
void INT10_SetCursorPos(uint8_t row,uint8_t col,uint8_t page);
void INT10_TeletypeOutput(uint8_t chr,uint8_t attr);
void INT10_TeletypeOutputAttr(uint8_t chr,uint8_t attr,bool useattr);
void INT10_ReadCharAttr(uint16_t * result,uint8_t page);
void INT10_WriteChar(uint16_t chr,uint8_t attr,uint8_t page,uint16_t count,bool showattr);
void INT10_WriteString(uint8_t row,uint8_t col,uint8_t flag,uint8_t attr,PhysPt string,uint16_t count,uint8_t page);

/* Graphics Stuff */
void INT10_PutPixel(uint16_t x,uint16_t y,uint8_t page,uint8_t color);
void INT10_GetPixel(uint16_t x,uint16_t y,uint8_t page,uint8_t * color);

/* Font Stuff */
void INT10_LoadFont(PhysPt font,bool reload,uint16_t count,Bitu offset,Bitu map,uint8_t height);
void INT10_ReloadFont(void);

/* Palette Group */
void INT10_SetBackgroundBorder(uint8_t val);
void INT10_SetColorSelect(uint8_t val);
void INT10_SetSinglePaletteRegister(uint8_t reg,uint8_t val);
void INT10_SetOverscanBorderColor(uint8_t val);
void INT10_SetAllPaletteRegisters(PhysPt data);
void INT10_ToggleBlinkingBit(uint8_t state);
void INT10_GetSinglePaletteRegister(uint8_t reg,uint8_t * val);
void INT10_GetOverscanBorderColor(uint8_t * val);
void INT10_GetAllPaletteRegisters(PhysPt data);
void INT10_SetSingleDACRegister(uint8_t index,uint8_t red,uint8_t green,uint8_t blue);
void INT10_GetSingleDACRegister(uint8_t index,uint8_t * red,uint8_t * green,uint8_t * blue);
void INT10_SetDACBlock(uint16_t index,uint16_t count,PhysPt data);
void INT10_GetDACBlock(uint16_t index,uint16_t count,PhysPt data);
void INT10_SelectDACPage(uint8_t function,uint8_t mode);
void INT10_GetDACPage(uint8_t* mode,uint8_t* page);
void INT10_SetPelMask(uint8_t mask);
void INT10_GetPelMask(uint8_t & mask);
void INT10_PerformGrayScaleSumming(uint16_t start_reg,uint16_t count);


/* Vesa Group */
uint8_t VESA_GetSVGAInformation(uint16_t seg,uint16_t off);
uint8_t VESA_GetSVGAModeInformation(uint16_t mode,uint16_t seg,uint16_t off);
uint8_t VESA_SetSVGAMode(uint16_t mode);
uint8_t VESA_GetSVGAMode(uint16_t & mode);
uint8_t VESA_SetCPUWindow(uint8_t window,uint8_t address);
uint8_t VESA_GetCPUWindow(uint8_t window,uint16_t & address);
uint8_t VESA_ScanLineLength(uint8_t subcall, uint16_t val, uint16_t & bytes,uint16_t & pixels,uint16_t & lines);
uint8_t VESA_SetDisplayStart(uint16_t x,uint16_t y,bool wait);
uint8_t VESA_GetDisplayStart(uint16_t & x,uint16_t & y);
uint8_t VESA_SetPalette(PhysPt data,Bitu index,Bitu count,bool wait);
uint8_t VESA_GetPalette(PhysPt data,Bitu index,Bitu count);

/* Sub Groups */
void INT10_SetupRomMemory(void);
void INT10_SetupRomMemoryChecksum(void);
void INT10_SetupVESA(void);

/* EGA RIL */
RealPt INT10_EGA_RIL_GetVersionPt(void);
void INT10_EGA_RIL_ReadRegister(uint8_t & bl, uint16_t dx);
void INT10_EGA_RIL_WriteRegister(uint8_t & bl, uint8_t bh, uint16_t dx);
void INT10_EGA_RIL_ReadRegisterRange(uint8_t ch, uint8_t cl, uint16_t dx, PhysPt dst);
void INT10_EGA_RIL_WriteRegisterRange(uint8_t ch, uint8_t cl, uint16_t dx, PhysPt src);
void INT10_EGA_RIL_ReadRegisterSet(uint16_t cx, PhysPt tbl);
void INT10_EGA_RIL_WriteRegisterSet(uint16_t cx, PhysPt tbl);

/* Video State */
Bitu INT10_VideoState_GetSize(Bitu state);
bool INT10_VideoState_Save(Bitu state,RealPt buffer);
bool INT10_VideoState_Restore(Bitu state,RealPt buffer);

/* Video Parameter Tables */
uint16_t INT10_SetupVideoParameterTable(PhysPt basepos);
void INT10_SetupBasicVideoParameterTable(void);
