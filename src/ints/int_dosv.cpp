/*
 *  Copyright (C) 2019 takapyu and 2021 The DOSBox-X Team
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

#include "dosbox.h"
#include "regs.h"
#include "paging.h"
#include "mem.h"
#include "inout.h"
#include "callback.h"
#include "int10.h"
#include "SDL.h"
#include "render.h"
#include "support.h"
#include "control.h"
#include "dos_inc.h"
#define INCJFONT 1
#include "jfont.h"

#define ID_LEN 6
#define NAME_LEN 8
#define SBCS19_LEN 256 * 19
#define DBCS16_LEN 65536 * 32
#define	VTEXT_MODE_COUNT	2

static Bitu dosv_font_handler[DOSV_FONT_MAX];
static uint16_t dosv_font_handler_offset[DOSV_FONT_MAX];
static uint16_t jtext_seg;
static Bitu dosv_timer;
static uint8_t dosv_cursor_stat;
static Bitu dosv_cursor_x;
static Bitu dosv_cursor_y;
static enum DOSV_VTEXT_MODE dosv_vtext_mode[VTEXT_MODE_COUNT];
static enum DOSV_FEP_CTRL dosv_fep_ctrl;

uint8_t TrueVideoMode;
uint8_t jfont_sbcs_19[SBCS19_LEN];//256 * 19( * 8)
uint8_t jfont_dbcs_16[DBCS16_LEN];//65536 * 16 * 2 (* 8)

void ResolvePath(std::string& in);
void SetIMPosition();
bool INT10_SetDOSVModeVtext(uint16_t mode, enum DOSV_VTEXT_MODE vtext_mode);

typedef struct {
    char id[ID_LEN];
    char name[NAME_LEN];
    unsigned char width;
    unsigned char height;
    unsigned char type;
} fontx_h;

typedef struct {
    uint16_t start;
	uint16_t end;
} fontxTbl;

Bitu getfontx2header(FILE *fp, fontx_h *header)
{
    fread(header->id, ID_LEN, 1, fp);
    if (strncmp(header->id, "FONTX2", ID_LEN) != 0) {
	return 1;
    }
    fread(header->name, NAME_LEN, 1, fp);
    header->width = (uint8_t)getc(fp);
    header->height = (uint8_t)getc(fp);
    header->type = (uint8_t)getc(fp);
    return 0;
}

uint16_t chrtosht(FILE *fp)
{
	uint16_t i, j;
	i = (uint8_t)getc(fp);
	j = (uint8_t)getc(fp) << 8;
	return(i | j);
}

void readfontxtbl(fontxTbl *table, Bitu size, FILE *fp)
{
    while (size > 0) {
        table->start = chrtosht(fp);
        table->end = chrtosht(fp);
        ++table;
        --size;
    }
}

static bool LoadFontxFile(const char * fname, int opt) {
    fontxTbl *table;
    uint8_t size;
    Bitu code;
    if (opt==1) {
        memcpy(jfont_sbcs_19, JPNHN19X+NAME_LEN+ID_LEN+3, SBCS19_LEN * sizeof(uint8_t));
        return true;
    } else if (opt==2) {
        int p=NAME_LEN+ID_LEN+3;
        size = JPNZN16X[p++];
        table = (fontxTbl *)calloc(size, sizeof(fontxTbl));
        Bitu i=0;
        while (i < size) {
            table[i].start = (JPNZN16X[p] | (JPNZN16X[p+1] << 8));
            table[i].end = (JPNZN16X[p+2] | (JPNZN16X[p+3] << 8));
            i++;
            p+=4;
        }
        for (i = 0; i < size; i++)
            for (code = table[i].start; code <= table[i].end; code++) {
                memcpy(&jfont_dbcs_16[code*32], JPNZN16X+p, 32*sizeof(uint8_t));
                p+=32*sizeof(uint8_t);
            }
        return true;
    }
    fontx_h head;
	if (!fname||*fname=='\0') return false;
	FILE * mfile=fopen(fname,"rb");
	if (!mfile) {
		LOG_MSG("MSG: Can't open FONTX2 file: %s",fname);
		return false;
	}
	if (getfontx2header(mfile, &head) != 0) {
		fclose(mfile);
		LOG_MSG("MSG: FONTX2 header is incorrect\n");
		return false;
    }
	// switch whether the font is DBCS or not
	if (head.type == 1) {
		if (head.width == 16 && head.height == 16) {
			size = getc(mfile);
			table = (fontxTbl *)calloc(size, sizeof(fontxTbl));
			readfontxtbl(table, size, mfile);
			for (Bitu i = 0; i < size; i++)
				for (code = table[i].start; code <= table[i].end; code++)
					fread(&jfont_dbcs_16[code*32], sizeof(uint8_t), 32, mfile);
		}
		else {
			fclose(mfile);
			LOG_MSG("MSG: FONTX2 DBCS font size is not correct\n");
			return false;
		}
	}
    else {
		if (head.width == 8 && head.height == 19)
			fread(jfont_sbcs_19, sizeof(uint8_t), SBCS19_LEN, mfile);
		else {
			fclose(mfile);
			LOG_MSG("MSG: FONTX2 SBCS font size is not correct\n");
			return false;
		}
    }
	fclose(mfile);
    return true;
}

void JFONT_Init() {
	std::string file_name;
    Section_prop *section = static_cast<Section_prop *>(control->GetSection("render"));
	Prop_path* pathprop = section->Get_path("jfontsbcs");
	if (pathprop && pathprop->realpath!="") {
        std::string path=pathprop->realpath;
        ResolvePath(path);
        if (!LoadFontxFile(path.c_str(), 0)) LoadFontxFile(NULL, 1);
    } else LoadFontxFile(NULL, 1);
	pathprop = section->Get_path("jfontdbcs");
	if(pathprop && pathprop->realpath!="") {
        std::string path=pathprop->realpath;
        ResolvePath(path);
        if (!LoadFontxFile(path.c_str(), 0)) LoadFontxFile(NULL, 2);
    } else LoadFontxFile(NULL, 2);
}

uint8_t GetTrueVideoMode()
{
	return TrueVideoMode;
}

void SetTrueVideoMode(uint8_t mode)
{
	TrueVideoMode = mode;
}

bool DOSV_CheckJapaneseVideoMode()
{
	if(IS_DOS_JAPANESE && (TrueVideoMode == 0x03 || TrueVideoMode == 0x70 || TrueVideoMode == 0x72 || TrueVideoMode == 0x78)) {
		return true;
	}
	return false;
}

bool DOSV_CheckCJKVideoMode()
{
	if(IS_DOS_CJK && (TrueVideoMode == 0x03 || TrueVideoMode == 0x70 || TrueVideoMode == 0x72 || TrueVideoMode == 0x78)) {
		return true;
	}
	return false;
}

bool INT10_DOSV_SetCRTBIOSMode(Bitu mode)
{
	TrueVideoMode = mode;
	if (!IS_DOSV) return false;
	if (mode == 0x03 || mode == 0x70) {
		LOG(LOG_INT10, LOG_NORMAL)("DOS/V CRT BIOS has been set to JP mode.");
		INT10_SetVideoMode(0x12);
		INT10_SetDOSVModeVtext(mode, DOSV_VGA);
		return true;
	}
	return false;
}

static Bitu font8x16(void)
{
	reg_al = 0x00;
	return CBRET_NONE;
}

static Bitu font8x19(void)
{
	PhysPt data	= PhysMake(SegValue(es), reg_si);
	uint8_t *font = &jfont_sbcs_19[reg_cl * 19];
	for(Bitu ct = 0 ; ct < 19 ; ct++) {
		mem_writeb(data++, *font++);
	}
	reg_al = 0x00;
	return CBRET_NONE;
}

static Bitu font16x16(void)
{
	PhysPt data	= PhysMake(SegValue(es), reg_si);
	uint16_t *font = (uint16_t *)jfont_dbcs_16[reg_cx * 32];
	for(Bitu ct = 0 ; ct < 16 ; ct++) {
		mem_writew(data, *font++);
		data += 2;
	}
	reg_al = 0x00;
	return CBRET_NONE;
}

static Bitu font12x24(void)
{
	reg_al = 0x00;
	return CBRET_NONE;
}

static Bitu font24x24(void)
{
	reg_al = 0x00;
	return CBRET_NONE;
}

static Bitu write_font16x16(void)
{
    reg_al = 0x00;
	return CBRET_NONE;
}

static Bitu write_font24x24(void)
{
	reg_al = 0x00;
	return CBRET_NONE;
}

static Bitu mskanji_api(void)
{
	uint16_t param_seg, param_off;
	uint16_t func, mode;

	param_seg = real_readw(SegValue(ss), reg_sp + 6);
	param_off = real_readw(SegValue(ss), reg_sp + 4);
	func = real_readw(param_seg, param_off);
	mode = real_readw(param_seg, param_off + 2);

	reg_ax = 0xffff;
	if(func == 1) {
		uint16_t kk_seg, kk_off;
		kk_seg = real_readw(param_seg, param_off + 6);
		kk_off = real_readw(param_seg, param_off + 4);
		real_writew(kk_seg, kk_off, 1);
		real_writeb(kk_seg, kk_off + 2, 'I');
		real_writeb(kk_seg, kk_off + 3, 'M');
		real_writeb(kk_seg, kk_off + 4, 'E');
		real_writeb(kk_seg, kk_off + 5, 0);
		reg_ax = 0;
	} else if(func == 5) {
#if defined(WIN32) && !defined(HX_DOS) && !defined(C_SDL2) && defined(SDL_DOSBOX_X_SPECIAL)
		if(mode & 0x8000) {
			if(mode & 0x0001)
				SDL_SetIMValues(SDL_IM_ONOFF, 0, NULL);
			else if(mode & 0x0002)
				SDL_SetIMValues(SDL_IM_ONOFF, 1, NULL);
		} else {
			int onoff;
			if(SDL_GetIMValues(SDL_IM_ONOFF, &onoff, NULL) == NULL) {
				if(onoff)
					real_writew(param_seg, param_off + 2, 0x000a);
				else
					real_writew(param_seg, param_off + 2, 0x0009);
			}
		}
		reg_ax = 0;
#endif
	}
	return CBRET_NONE;
}

static CallBack_Handler font_handler_list[] = {
	font8x16,
	font8x19,
	font16x16,
	font12x24,
	font24x24,
	write_font16x16,
	write_font24x24,

	mskanji_api,
};

uint16_t DOSV_GetFontHandlerOffset(enum DOSV_FONT font)
{
	if(font >= DOSV_FONT_8X16 && font < DOSV_FONT_MAX) {
		return dosv_font_handler_offset[font];
	}
	return 0;
}


static Bitu dosv_cursor_24[] = {
	0, 3, 8, 12, 15, 18, 21, 23
};

static Bitu dosv_cursor_19[] = {
	0, 3, 6, 9, 11, 13, 16, 18
};

static Bitu dosv_cursor_16[] = {
	0, 3, 5, 7, 9, 11, 13, 15
};

void DOSV_CursorXor24(Bitu x, Bitu y, Bitu start, Bitu end)
{
	IO_Write(0x3ce, 0x05); IO_Write(0x3cf, 0x03);
	IO_Write(0x3ce, 0x00); IO_Write(0x3cf, 0x0f);
	IO_Write(0x3ce, 0x03); IO_Write(0x3cf, 0x18);

	Bitu width = (real_readw(BIOSMEM_SEG, BIOSMEM_NB_COLS) == 85) ? 128 : 160;
	volatile uint8_t dummy;
	Bitu off = (y + start) * width + (x * 12) / 8;
	uint8_t select;
	if(svgaCard == SVGA_TsengET4K) {
		if(off >= 0x20000) {
			select = 0x22;
			off -= 0x20000;
		} else if(off >= 0x10000) {
			select = 0x11;
			off -= 0x10000;
		} else {
			select = 0x00;
		}
		IO_Write(0x3cd, select);
	}
	while(start <= end) {
		if(dosv_cursor_stat == 2) {
			if(x & 1) {
				dummy = real_readb(0xa000, off);
				real_writeb(0xa000, off, 0x0f);
				off++;
				if(off >= 0x10000) {
					if(select == 0x00) {
						select = 0x11;
					} else if(select == 0x11) {
						select = 0x22;
					}
					IO_Write(0x3cd, select);
					off -= 0x10000;
				}
				dummy = real_readb(0xa000, off);
				real_writeb(0xa000, off, 0xff);
				off++;
				if(off >= 0x10000) {
					if(select == 0x00) {
						select = 0x11;
					} else if(select == 0x11) {
						select = 0x22;
					}
					IO_Write(0x3cd, select);
					off -= 0x10000;
				}
				dummy = real_readb(0xa000, off);
				real_writeb(0xa000, off, 0xff);
				off++;
				if(off >= 0x10000) {
					if(select == 0x00) {
						select = 0x11;
					} else if(select == 0x11) {
						select = 0x22;
					}
					IO_Write(0x3cd, select);
					off -= 0x10000;
				}
				dummy = real_readb(0xa000, off);
				real_writeb(0xa000, off, 0xf0);
				off += width - 3;
			} else {
				dummy = real_readb(0xa000, off);
				real_writeb(0xa000, off, 0xff);
				off++;
				if(off >= 0x10000) {
					if(select == 0x00) {
						select = 0x11;
					} else if(select == 0x11) {
						select = 0x22;
					}
					IO_Write(0x3cd, select);
					off -= 0x10000;
				}
				dummy = real_readb(0xa000, off);
				real_writeb(0xa000, off, 0xff);
				off++;
				if(off >= 0x10000) {
					if(select == 0x00) {
						select = 0x11;
					} else if(select == 0x11) {
						select = 0x22;
					}
					IO_Write(0x3cd, select);
					off -= 0x10000;
				}
				dummy = real_readb(0xa000, off);
				real_writeb(0xa000, off, 0xff);
				off += width - 2;
			}
		} else {
			if(x & 1) {
				dummy = real_readb(0xa000, off);
				real_writeb(0xa000, off, 0x0f);
				off++;
				if(off >= 0x10000) {
					if(select == 0x00) {
						select = 0x11;
					} else if(select == 0x11) {
						select = 0x22;
					}
					IO_Write(0x3cd, select);
					off -= 0x10000;
				}
				dummy = real_readb(0xa000, off);
				real_writeb(0xa000, off, 0xff);
			} else {
				dummy = real_readb(0xa000, off);
				real_writeb(0xa000, off, 0xff);
				off++;
				if(off >= 0x10000) {
					if(select == 0x00) {
						select = 0x11;
					} else if(select == 0x11) {
						select = 0x22;
					}
					IO_Write(0x3cd, select);
					off -= 0x10000;
				}
				dummy = real_readb(0xa000, off);
				real_writeb(0xa000, off, 0xf0);
			}
			off += width - 1;
		}
		if(svgaCard == SVGA_TsengET4K) {
			if(off >= 0x10000) {
				if(select == 0x00) {
					select = 0x11;
				} else if(select == 0x11) {
					select = 0x22;
				}
				IO_Write(0x3cd, select);
				off -= 0x10000;
			}
		}
		start++;
	}
	IO_Write(0x3ce, 0x03); IO_Write(0x3cf, 0x00);
	if(svgaCard == SVGA_TsengET4K) {
		IO_Write(0x3cd, 0);
	}
}

void DOSV_CursorXor(Bitu x, Bitu y)
{
	Bitu end = real_readb(BIOSMEM_SEG, BIOSMEM_CURSOR_TYPE);
	Bitu start = real_readb(BIOSMEM_SEG, BIOSMEM_CURSOR_TYPE + 1);

	if(start != 0x20 && start <= end) {
		Bitu width = real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS);
		uint16_t height = real_readw(BIOSMEM_SEG, BIOSMEM_CHAR_HEIGHT);
		if(height == 24) {
			if(start < 8) start = dosv_cursor_24[start];
			if(end < 8) end = dosv_cursor_24[end];
			DOSV_CursorXor24(x, y, start, end);
			return;
		} else if(height == 19) {
			if(start < 8) start = dosv_cursor_19[start];
			if(end < 8) end = dosv_cursor_19[end];
		} else if(height == 16) {
			if(start < 8) start = dosv_cursor_16[start];
			if(end < 8) end = dosv_cursor_16[end];
		}
		IO_Write(0x3ce, 0x05); IO_Write(0x3cf, 0x03);
		IO_Write(0x3ce, 0x00); IO_Write(0x3cf, 0x0f);
		IO_Write(0x3ce, 0x03); IO_Write(0x3cf, 0x18);

		volatile uint8_t dummy;
		Bitu off = (y + start) * width + x;
		uint8_t select;
		if(svgaCard == SVGA_TsengET4K) {
			if(off >= 0x20000) {
				select = 0x22;
				off -= 0x20000;
			} else if(off >= 0x10000) {
				select = 0x11;
				off -= 0x10000;
			} else {
				select = 0x00;
			}
			IO_Write(0x3cd, select);
		}
		while(start <= end) {
			dummy = real_readb(0xa000, off);
			real_writeb(0xa000, off, 0xff);
			if(dosv_cursor_stat == 2) {
				off++;
				if(off >= 0x10000) {
					if(select == 0x00) {
						select = 0x11;
					} else if(select == 0x11) {
						select = 0x22;
					}
					IO_Write(0x3cd, select);
					off -= 0x10000;
				}
				dummy = real_readb(0xa000, off);
				real_writeb(0xa000, off, 0xff);
				off += width - 1;
			} else {
				off += width;
			}
			if(svgaCard == SVGA_TsengET4K) {
				if(off >= 0x10000) {
					if(select == 0x00) {
						select = 0x11;
					} else if(select == 0x11) {
						select = 0x22;
					}
					IO_Write(0x3cd, select);
					off -= 0x10000;
				}
			}
			start++;
		}
		IO_Write(0x3ce, 0x03); IO_Write(0x3cf, 0x00);
		if(svgaCard == SVGA_TsengET4K) {
			IO_Write(0x3cd, 0);
		}
	}
}

void DOSV_OffCursor()
{
	if(dosv_cursor_stat) {
		DOSV_CursorXor(dosv_cursor_x, dosv_cursor_y);
		dosv_cursor_stat = 0;
	}
}

bool CheckAnotherDisplayDriver()
{
	if(mem_readw(0x0042) != 0xf000) {
		Bitu addr = mem_readw(0x0042) << 4;
		char text[10];

		MEM_BlockRead(addr + 10, text, 8);
		text[8] = '\0';
		if(!strcmp(text, "$IBMADSP")) {
			return true;
		}
		MEM_BlockRead(addr - 8, text, 4);
		text[4] = '\0';
		if(!strcmp(text, "DSP4")) {
			if(mem_readb(0x0449) == 0x70) {
				return true;
			}
		}
	}
	return false;
}

uint16_t GetTextSeg()
{
	return jtext_seg;
}

void SetTextSeg()
{
	if(jtext_seg == 0) {
		jtext_seg = DOS_GetMemory(0x500);
	}
}

uint8_t GetKanjiAttr(Bitu x, Bitu y)
{
	Bitu cx, pos;
	uint8_t flag;
	uint16_t seg = (IS_JEGA_ARCH) ? 0xb800 : jtext_seg;
	pos = y * real_readw(BIOSMEM_SEG, BIOSMEM_NB_COLS) * 2;
	cx = 0;
	flag = 0x00;
	do {
		if(flag == 0x01) {
			flag = 0x02;
		} else {
			flag = 0x00;
			if(isKanji1(real_readb(seg, pos))) {
				flag = 0x01;
			}
		}
		pos += 2;
		cx++;
	} while(cx <= x);
	return flag;
}

uint8_t GetKanjiAttr()
{
	return GetKanjiAttr(real_readb(BIOSMEM_SEG, BIOSMEM_CURSOR_POS), real_readb(BIOSMEM_SEG, BIOSMEM_CURSOR_POS + 1));
}

void INT8_DOSV()
{
#if defined(WIN32) && !defined(HX_DOS) && !defined(C_SDL2) && defined(SDL_DOSBOX_X_SPECIAL)
	SetIMPosition();
#endif
	if(!CheckAnotherDisplayDriver() && real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_MODE) != 0x72) {
		if(dosv_cursor_stat == 0) {
			Bitu x = real_readb(BIOSMEM_SEG, BIOSMEM_CURSOR_POS);
			Bitu y = real_readb(BIOSMEM_SEG, BIOSMEM_CURSOR_POS + 1) * real_readb(BIOSMEM_SEG,BIOSMEM_CHAR_HEIGHT);
			uint8_t attr = GetKanjiAttr();
			if(attr == 0) {
				dosv_cursor_stat = 1;
			} else {
				dosv_cursor_stat = 1;
				if(attr == 1) {
					dosv_cursor_stat = 2;
				}
			}
			dosv_cursor_x = x;
			dosv_cursor_y = y;
			DOSV_CursorXor(x, y);
		}
	}
}

enum DOSV_VTEXT_MODE DOSV_GetVtextMode(Bitu no)
{
	if(no < VTEXT_MODE_COUNT) {
		return dosv_vtext_mode[no];
	}
	return dosv_vtext_mode[0];
}

enum DOSV_VTEXT_MODE DOSV_StringVtextMode(std::string vtext)
{
	if(vtext == "vga") {
		return DOSV_VTEXT_VGA;
	} else if(svgaCard == SVGA_TsengET4K) {
		if(vtext == "xga") {
			return DOSV_VTEXT_XGA;
		} else if(vtext == "xga24") {
			return DOSV_VTEXT_XGA_24;
		} else if(vtext == "sxga") {
			return DOSV_VTEXT_SXGA;
		} else if(vtext == "sxga24") {
			return DOSV_VTEXT_SXGA_24;
		}
	}
	return DOSV_VTEXT_SVGA;
}

enum DOSV_FEP_CTRL DOSV_GetFepCtrl()
{
	return dosv_fep_ctrl;
}

void DOSV_SetConfig(Section_prop *section)
{
	const char *param = section->Get_string("fepcontrol");
	if(!strcmp(param, "ias")) {
		dosv_fep_ctrl = DOSV_FEP_CTRL_IAS;
	} else if(!strcmp(param, "mskanji")) {
		dosv_fep_ctrl = DOSV_FEP_CTRL_MSKANJI;
	} else {
		dosv_fep_ctrl = DOSV_FEP_CTRL_BOTH;
	}
	dosv_vtext_mode[0] = DOSV_StringVtextMode(section->Get_string("vtext"));
	dosv_vtext_mode[1] = DOSV_StringVtextMode(section->Get_string("vtext2"));
}

void DOSV_Setup()
{
	SetTextSeg();
	for(Bitu ct = 0 ; ct < DOSV_FONT_MAX ; ct++) {
		dosv_font_handler[ct] = CALLBACK_Allocate();
		CallBack_Handlers[dosv_font_handler[ct]] = font_handler_list[ct];
		dosv_font_handler_offset[ct] = CALLBACK_PhysPointer(dosv_font_handler[ct]) & 0xffff;
		phys_writeb(CALLBACK_PhysPointer(dosv_font_handler[ct]) + 0, 0xFE);
		phys_writeb(CALLBACK_PhysPointer(dosv_font_handler[ct]) + 1, 0x38);
		phys_writew(CALLBACK_PhysPointer(dosv_font_handler[ct]) + 2, (uint16_t)dosv_font_handler[ct]);
		if(ct == DOSV_MSKANJI_API) {
			phys_writeb(CALLBACK_PhysPointer(dosv_font_handler[ct]) + 4, 0xca);
			phys_writew(CALLBACK_PhysPointer(dosv_font_handler[ct]) + 5, 0x0004);
		} else {
			phys_writeb(CALLBACK_PhysPointer(dosv_font_handler[ct]) + 4, 0xcb);
		}
	}
}
