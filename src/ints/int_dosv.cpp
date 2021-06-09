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
#if defined(LINUX)
#include <X11/Xlib.h>
#include <X11/Xlocale.h>
#include <X11/Xutil.h>
#endif

#define ID_LEN 6
#define NAME_LEN 8
#define VTEXT_MODE_COUNT 2
#define SBCS16_LEN 256 * 16
#define SBCS19_LEN 256 * 19
#define DBCS16_LEN 65536 * 32
#define DBCS24_LEN 65536 * 72
#define SBCS24_LEN 256 * 48

#if defined(LINUX)
static Display *font_display;
static Window font_window;
static Pixmap font_pixmap;
static GC font_gc;
static XFontSet font_set16;
static XFontSet font_set24;
#endif

const char jfont_name[] = "\x082\x06c\x082\x072\x020\x083\x053\x083\x056\x083\x062\x083\x04e";
static uint8_t jfont_dbcs[96];
uint8_t jfont_sbcs_19[SBCS19_LEN];//256 * 19( * 8)
uint8_t jfont_dbcs_16[DBCS16_LEN];//65536 * 16 * 2 (* 8)
uint8_t jfont_sbcs_16[SBCS16_LEN];//256 * 16( * 8)
uint8_t jfont_dbcs_24[DBCS24_LEN];//65536 * 24 * 3
uint8_t jfont_sbcs_24[SBCS24_LEN];//256 * 12 * 2
uint8_t jfont_cache_dbcs_16[65536];
uint8_t jfont_cache_dbcs_24[65536];

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

static Bitu dosv_font_handler[DOSV_FONT_MAX];
static uint16_t dosv_font_handler_offset[DOSV_FONT_MAX];
static uint16_t jtext_seg;
static uint8_t dosv_cursor_stat;
static Bitu dosv_cursor_x;
static Bitu dosv_cursor_y;
static enum DOSV_VTEXT_MODE dosv_vtext_mode[VTEXT_MODE_COUNT];
static enum DOSV_FEP_CTRL dosv_fep_ctrl;
#if defined(WIN32)
static HFONT jfont_16;
static HFONT jfont_24;
#endif

bool yen_flag = false;
uint8_t TrueVideoMode;
void ResolvePath(std::string& in);
void SetIMPosition();
bool INT10_SetDOSVModeVtext(uint16_t mode, enum DOSV_VTEXT_MODE vtext_mode);
bool CodePageGuestToHostUTF16(uint16_t *d/*CROSS_LEN*/,const char *s/*CROSS_LEN*/);
bool isDBCSCP(), isDBCSLB(uint8_t chr, uint8_t* lead);
extern uint8_t lead[6];

bool isKanji1(uint8_t chr) {
    if (dos.loaded_codepage == 936 || dos.loaded_codepage == 949 || dos.loaded_codepage == 950) {
        for (int i=0; i<6; i++) lead[i] = 0;
        if (isDBCSCP())
            for (int i=0; i<6; i++) {
                lead[i] = mem_readb(Real2Phys(dos.tables.dbcs)+i);
                if (lead[i] == 0) break;
            }
        return isDBCSLB(chr, lead);
    }
    return (chr >= 0x81 && chr <= 0x9f) || (chr >= 0xe0 && chr <= 0xfc);
}

bool isKanji2(uint8_t chr) {
    if (dos.loaded_codepage == 936 || dos.loaded_codepage == 949 || dos.loaded_codepage == 950)
        return chr >= 0x40 && chr <= 0xfc;
    return (chr >= 0x40 && chr <= 0x7e) || (chr >= 0x80 && chr <= 0xfc);
}

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

static bool LoadFontxFile(const char *fname, int height = 16) {
    fontx_h head;
    fontxTbl *table;
    Bitu code;
    uint8_t size;
	if (!fname) return false;
	if(*fname=='\0') return false;
	FILE * mfile=fopen(fname,"rb");
	if (!mfile) {
#if defined(LINUX)
		char *start = strrchr((char *)fname, '/');
		if(start != NULL) {
			char cname[PATH_MAX + 1];
			sprintf(cname, ".%s", start);
			mfile=fopen(cname, "rb");
			if (!mfile) {
				LOG_MSG("MSG: Can't open FONTX2 file: %s",fname);
				return false;
			}
		}
#else
		LOG_MSG("MSG: Can't open FONTX2 file: %s",fname);
		return false;
#endif
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
			for (Bitu i = 0; i < size; i++) {
				for (code = table[i].start; code <= table[i].end; code++) {
					fread(&jfont_dbcs_16[code * 32], sizeof(uint8_t), 32, mfile);
					jfont_cache_dbcs_16[code] = 1;
				}
			}
		}
		else if (head.width == 24 && head.height == 24) {
			size = getc(mfile);
			table = (fontxTbl *)calloc(size, sizeof(fontxTbl));
			readfontxtbl(table, size, mfile);
			for (Bitu i = 0; i < size; i++) {
				for (code = table[i].start ; code <= table[i].end ; code++) {
					fread(&jfont_dbcs_24[code * 72], sizeof(uint8_t), 72, mfile);
					jfont_cache_dbcs_24[code] = 1;
				}
			}
		}
		else {
			fclose(mfile);
			LOG_MSG("MSG: FONTX2 DBCS font size is not correct\n");
			return false;
		}
	}
    else {
		if (head.width == 8 && head.height == 19 && height == 19) {
			fread(jfont_sbcs_19, sizeof(uint8_t), SBCS19_LEN, mfile);
		} else if (head.width == 8 && head.height == 16) {
			if(height == 16) {
				for(int i = 0 ; i < 256 ; i++) {
					fread(&jfont_sbcs_16[i * 16], sizeof(uint8_t), 16, mfile);
				}
			} else if(height == 19) {
				for(int i = 0 ; i < 256 ; i++) {
					fread(&jfont_sbcs_19[i * 19 + 1], sizeof(uint8_t), 16, mfile);
				}
			}
		} else if (head.width == 12 && head.height == 24) {
			fread(jfont_sbcs_24, sizeof(uint8_t), SBCS24_LEN, mfile);
		}
		else {
			fclose(mfile);
			LOG_MSG("MSG: FONTX2 SBCS font size is not correct\n");
			return false;
		}
    }
	fclose(mfile);
	return true;

}

static bool CheckEmptyData(uint8_t *data, Bitu length)
{
	while(length > 0) {
		if(*data++ != 0) {
			return false;
		}
		length--;
	}
	return true;
}

bool GetWindowsFont(Bitu code, uint8_t *buff, int width, int height)
{
#if defined(LINUX)
	XRectangle ir, lr;
	wchar_t text[4];

	if(code < 0x100) {
		if(code == 0x5c) {
			// yen
			text[0] = 0xa5;
		} else if(code >= 0xa1 && code <= 0xdf) {
			// half kana
			text[0] = 0xff61 + (code - 0xa1);
		} else {
			text[0] = code;
		}
	} else {
		char src[4];
		src[0] = code >> 8;
		src[1] = code & 0xff;
		src[2] = 0;
		CodePageGuestToHostUTF16((uint16_t *)text,src);
		text[0] &= 0xffff;
	}
	text[1] = ']';
	text[2] = 0;

	memset(buff, 0, (width / 8) * height);

	if(height == 24) {
		if(font_set24 == NULL) return false;
		XwcTextExtents(font_set24, text, 2, &ir, &lr);
	} else {
		if(font_set16 == NULL) return false;
		XwcTextExtents(font_set16, text, 2, &ir, &lr);
	}
	XSetForeground(font_display, font_gc, BlackPixel(font_display, 0));
	XFillRectangle(font_display, font_pixmap, font_gc, 0, 0, 32, 32);
	XSetForeground(font_display, font_gc, WhitePixel(font_display, 0));
	XwcDrawString(font_display, font_pixmap, height == 24? font_set24 : font_set16, font_gc, 0, lr.height - (ir.height + ir.y), text, 2);
	XImage *image = XGetImage(font_display, font_pixmap, 0, 0, width, lr.height, ~0, XYPixmap);
	if(image != NULL) {
		int x, y;
		for(y = 0 ; y < height ; y++) {
			uint8_t data = 0;
			uint8_t mask = 0x01;
			uint8_t font_mask = 0x80;
			uint8_t *pt = (unsigned char *)image->data + y * image->bytes_per_line;
			for(x = 0 ; x < width ; x++) {
				if(*pt & mask) {
					data |= font_mask;
				}
				mask <<= 1;
				font_mask >>= 1;
				if(font_mask == 0) {
					pt++;
					*buff++ = data;
					data = 0;
					mask = 0x01;
					font_mask = 0x80;
				}
			}
			if(width == 12) {
				pt++;
				*buff++ = data;
				data = 0;
				mask = 0x01;
				font_mask = 0x80;
			}
		}
		XDestroyImage(image);
		return true;
	}
#endif
#if defined(WIN32)
	HFONT font = (height == 16) ? jfont_16 : jfont_24;
	if(font != NULL) {
		HDC hdc = GetDC(NULL);
		HFONT old_font = (HFONT)SelectObject(hdc, font);

		TEXTMETRIC tm;
		GetTextMetrics(hdc, &tm);
		GLYPHMETRICS gm;
		CONST MAT2 mat = { {0,1},{0,0},{0,0},{0,1} };
		DWORD size = GetGlyphOutline(hdc, code, GGO_BITMAP, &gm, 0, NULL, &mat);
		if(size > 0) {
			char *fontbuff = new char[size];
			memset(fontbuff, 0, size);
			GetGlyphOutline(hdc, code, GGO_BITMAP, &gm, size, fontbuff, &mat);

			Bitu off_y = tm.tmAscent - gm.gmptGlyphOrigin.y;
			Bitu pos = off_y;
			Bitu count = (1 + (gm.gmBlackBoxX / 32)) * 4;
			if(width >= 16 || (width == 12 && height == 24)) {
				pos += off_y;
				if(width == 24) {
					pos += off_y;
				}
			}
			for(Bitu y = off_y ; y < off_y + gm.gmBlackBoxY; y++) {
				uint32_t data = 0;
				uint32_t bit = 0x800000 >> ((width - gm.gmBlackBoxX) / 2);
				for (Bitu x = gm.gmptGlyphOrigin.x; x < gm.gmptGlyphOrigin.x + gm.gmBlackBoxX; x++) {
					uint8_t src = *((uint8_t *)fontbuff + count * (y - off_y) + ((x - gm.gmptGlyphOrigin.x) / 8));
					if(src & (1 << (7 - ((x - gm.gmptGlyphOrigin.x) % 8)))) {
						data |= bit;
					}
					bit >>= 1;
				}
				buff[pos++] = (data >> 16) & 0xff;
				if(width >= 16 || (width == 12 && height == 24)) {
					buff[pos++] = (data >> 8) & 0xff;
					if(width == 24) {
						buff[pos++] = data & 0xff;
					}
				}
			}
			delete [] fontbuff;
		}
		SelectObject(hdc, old_font);
		ReleaseDC(NULL, hdc);

		return true;
	}
#endif
	return false;
}

void GetDbcsFrameFont(Bitu code, uint8_t *buff)
{
	if(code >= 0x849f && code <= 0x84be) {
		memcpy(buff, &frame_font_data[(code - 0x849f) * 32], 32);
	}
}

void GetDbcs24FrameFont(Bitu code, uint8_t *buff)
{
	if(code >= 0x849f && code <= 0x84be) {
		memcpy(buff, &frame_font24_data[(code - 0x849f) * 72], 72);
	}
}

uint8_t *GetDbcsFont(Bitu code)
{
	memset(jfont_dbcs, 0, sizeof(jfont_dbcs));
	if(jfont_cache_dbcs_16[code] == 0) {
		if(code >= 0x849f && code <= 0x84be) {
			GetDbcsFrameFont(code, jfont_dbcs);
			memcpy(&jfont_dbcs_16[code * 32], jfont_dbcs, 32);
			jfont_cache_dbcs_16[code] = 1;
		} else if(GetWindowsFont(code, jfont_dbcs, 16, 16)) {
			memcpy(&jfont_dbcs_16[code * 32], jfont_dbcs, 32);
			jfont_cache_dbcs_16[code] = 1;
		} else {
			int p = NAME_LEN+ID_LEN+3;
			uint8_t size = JPNZN16X[p++];
			fontxTbl *table = (fontxTbl *)calloc(size, sizeof(fontxTbl));
			Bitu i=0;
			while (i < size) {
				table[i].start = (JPNZN16X[p] | (JPNZN16X[p+1] << 8));
				table[i].end = (JPNZN16X[p+2] | (JPNZN16X[p+3] << 8));
				i++;
				p+=4;
			}
			for (i = 0; i < size; i++)
				for (uint16_t c = table[i].start; c <= table[i].end; c++) {
					if (c==code) {
						memcpy(&jfont_dbcs_16[code * 32], JPNZN16X+p, 32);
						return JPNZN16X+p;
					}
					p+=32;
				}
		}
		return jfont_dbcs;
	}
	return &jfont_dbcs_16[code * 32];
}

uint8_t *GetDbcs24Font(Bitu code)
{
	memset(jfont_dbcs, 0, sizeof(jfont_dbcs));
	if(jfont_cache_dbcs_24[code] == 0) {
		if(code >= 0x809e && code < 0x80fe) {
			if(GetWindowsFont(code - 0x807e, jfont_dbcs, 12, 24)) {
				Bitu no, pos;
				pos = code * 72;
				for(no = 0 ; no < 24 ; no++) {
					jfont_dbcs_24[pos + no * 3] = jfont_dbcs[no * 2];
					jfont_dbcs_24[pos + no * 3 + 1] = jfont_dbcs[no * 2 + 1] | (jfont_dbcs[no * 2] >> 4);
					jfont_dbcs_24[pos + no * 3 + 2] = (jfont_dbcs[no * 2] << 4) | (jfont_dbcs[no * 2 + 1] >> 4);
				}
				jfont_cache_dbcs_24[code] = 1;
				return &jfont_dbcs_24[pos];
			}
		} else if(code >= 0x8540 && code <= 0x857e) {
			if(GetWindowsFont(code - 0x8540 + 0xa1, jfont_dbcs, 12, 24)) {
				Bitu no, pos;
				pos = code * 72;
				for(no = 0 ; no < 24 ; no++) {
					jfont_dbcs_24[pos + no * 3] = jfont_dbcs[no * 2];
					jfont_dbcs_24[pos + no * 3 + 1] = jfont_dbcs[no * 2 + 1] | (jfont_dbcs[no * 2] >> 4);
					jfont_dbcs_24[pos + no * 3 + 2] = (jfont_dbcs[no * 2] << 4) | (jfont_dbcs[no * 2 + 1] >> 4);
				}
				jfont_cache_dbcs_24[code] = 1;
				return &jfont_dbcs_24[pos];
			}
		} else if(code >= 0x849f && code <= 0x84be) {
			GetDbcs24FrameFont(code, jfont_dbcs);
			memcpy(&jfont_dbcs_24[code * 72], jfont_dbcs, 72);
			jfont_cache_dbcs_24[code] = 1;
		} else {
			if(GetWindowsFont(code, jfont_dbcs, 24, 24)) {
				memcpy(&jfont_dbcs_24[code * 72], jfont_dbcs, 72);
				jfont_cache_dbcs_24[code] = 1;
			}
		}
		return jfont_dbcs;
	}
	return &jfont_dbcs_24[code * 72];
}

uint8_t *GetSbcsFont(Bitu code)
{
	return &jfont_sbcs_16[code * 16];
}

uint8_t *GetSbcs19Font(Bitu code)
{
	return &jfont_sbcs_19[code * 19];
}

uint8_t *GetSbcs24Font(Bitu code)
{
	return &jfont_sbcs_24[code * 2 * 24];
}

void InitFontHandle()
{
#if defined(LINUX)
	int missing_count;
	char **missing_list;
	char *def_string;

	if(!font_display) {
		font_display = XOpenDisplay("");
	}
	if(font_display) {
		if(!font_set16) {
			font_set16 = XCreateFontSet(font_display, "-*-fixed-medium-r-normal--16-*-*-*", &missing_list, &missing_count, &def_string);
			XFreeStringList(missing_list);
		}
		if(!font_set24) {
			font_set24 = XCreateFontSet(font_display, "-*-fixed-medium-r-normal--24-*-*-*", &missing_list, &missing_count, &def_string);
			XFreeStringList(missing_list);
		}
		if(!font_window) {
			font_window = XCreateSimpleWindow(font_display, DefaultRootWindow(font_display), 0, 0, 32, 32, 0, BlackPixel(font_display, DefaultScreen(font_display)), WhitePixel(font_display, DefaultScreen(font_display)));
			font_pixmap = XCreatePixmap(font_display, font_window, 32, 32, DefaultDepth(font_display, 0));
			font_gc = XCreateGC(font_display, font_pixmap, 0, 0);
		}
	}
#endif
#if defined(WIN32)
	if(jfont_16 == NULL || jfont_24 == NULL) {
		LOGFONT lf = { 0 };
		lf.lfHeight = 16;
		lf.lfCharSet = IS_KDOSV?HANGUL_CHARSET:(IS_CDOSV?CHINESEBIG5_CHARSET:(IS_PDOSV?GB2312_CHARSET:SHIFTJIS_CHARSET));
		lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
		lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
		lf.lfQuality = DEFAULT_QUALITY;
		lf.lfPitchAndFamily = FIXED_PITCH;
		strcpy(lf.lfFaceName, jfont_name);
		jfont_16 = CreateFontIndirect(&lf);
		lf.lfHeight = 24;
		jfont_24 = CreateFontIndirect(&lf);
	}
#endif
}

bool MakeSbcs16Font() {
	InitFontHandle();
	for(Bitu code = 0 ; code < 256 ; code++) {
		if(!GetWindowsFont(code, &jfont_sbcs_16[code * 16], 8, 16))
			return false;
	}
	if (IS_JDOSV) memcpy(jfont_sbcs_16, dosv_font16_data, sizeof(dosv_font16_data));
	else if (IS_DOSV) for(Bitu ct = 0 ; ct < 0x100 ; ct++) memcpy(&jfont_sbcs_16[ct * 16], &int10_font_16[ct * 16], 16);
	return true;
}

bool MakeSbcs19Font() {
	InitFontHandle();
	bool fail=false;
	for(Bitu code = 0 ; code < 256 ; code++) {
		if(!GetWindowsFont(code, &jfont_sbcs_19[code * 19 + 1], 8, 16)) {
			fail=true;
			break;
		}
	}
	if (fail) memcpy(jfont_sbcs_19, JPNHN19X+NAME_LEN+ID_LEN+3, SBCS19_LEN);
	if (IS_JDOSV) memcpy(jfont_sbcs_19, dosv_font19_data, sizeof(dosv_font19_data));
	else if (IS_DOSV) for(Bitu ct = 0 ; ct < 0x100 ; ct++) memcpy(&jfont_sbcs_19[ct * 19 + 1], &int10_font_16[ct * 16], 16);
	return true;
}

bool MakeSbcs24Font() {
	InitFontHandle();
	for(Bitu code = 0 ; code < 256 ; code++) {
		if(!GetWindowsFont(code, &jfont_sbcs_24[code * 24 * 2], 12, 24))
			return false;
	}
	if (IS_JDOSV) memcpy(jfont_sbcs_24, dosv_font24_data, sizeof(dosv_font24_data));
	else if (IS_DOSV) for(Bitu ct = 0 ; ct < 0x100 ; ct++) memcpy(&jfont_sbcs_24[ct * 24], &int10_font_16[ct * 16], 16);
	return true;
}

void JFONT_Init() {
#if defined(WIN32) && !defined(HX_DOS) && !defined(C_SDL2) && defined(SDL_DOSBOX_X_SPECIAL)
	SDL_SetCompositionFontName(jfont_name);
#endif
    Section_prop *section = static_cast<Section_prop *>(control->GetSection("render"));
	yen_flag = section->Get_bool("yen");

	Prop_path* pathprop = section->Get_path("jfontsbcs");
	if (pathprop) {
		std::string path=pathprop->realpath;
		ResolvePath(path);
		if(!LoadFontxFile(path.c_str(), 19)) {
			if(!MakeSbcs19Font()) {
				LOG_MSG("MSG: SBCS 8x19 font file path is not specified.\n");
#if defined(LINUX)
				for(Bitu ct = 0 ; ct < 0x100 ; ct++)
					memcpy(&jfont_sbcs_19[ct * 19 + 1], &int10_font_16[ct * 16], 16);
#endif
			}
		} else if(yen_flag) {
			if(!CheckEmptyData(&jfont_sbcs_19[0x7f * 19], 19))
				memcpy(&jfont_sbcs_19[0x5c * 19], &jfont_sbcs_19[0x7f * 19], 19);
		}
	} else {
		if(!MakeSbcs19Font())
			LOG_MSG("MSG: SBCS 8x19 font file path is not specified.\n");
	}
	pathprop = section->Get_path("jfontdbcs");
	if(pathprop) {
		std::string path=pathprop->realpath;
		ResolvePath(path);
		LoadFontxFile(path.c_str());
	}
	if(IS_DOSV) {
		pathprop = section->Get_path("jfontsbcs16");
		if(pathprop) {
			std::string path=pathprop->realpath;
			ResolvePath(path);
			if(!LoadFontxFile(path.c_str())) {
				if(!MakeSbcs16Font()) {
					LOG_MSG("MSG: SBCS 8x16 font file path is not specified.\n");
#if defined(LINUX)
					memcpy(jfont_sbcs_16, int10_font_16, 256 * 16);
#endif
				}
			} else if(yen_flag) {
				if(!CheckEmptyData(&jfont_sbcs_16[0x7f * 16], 16))
					memcpy(&jfont_sbcs_16[0x5c * 16], &jfont_sbcs_16[0x7f * 16], 16);
			}
		} else {
			if(!MakeSbcs16Font()) {
				LOG_MSG("MSG: SBCS 8x16 font file path is not specified.\n");
			}
		}
		pathprop = section->Get_path("jfontdbcs24");
		if(pathprop) {
			std::string path=pathprop->realpath;
			ResolvePath(path);
			LoadFontxFile(path.c_str());
		}
		pathprop = section->Get_path("jfontsbcs24");
		if(pathprop) {
			std::string path=pathprop->realpath;
			ResolvePath(path);
			if(!LoadFontxFile(path.c_str())) {
				if(!MakeSbcs24Font()) {
					LOG_MSG("MSG: SBCS 12x24 font file path is not specified.\n");
				}
			} else if(yen_flag) {
				if(!CheckEmptyData(&jfont_sbcs_24[0x7f * 2 * 24], 2 * 24))
					memcpy(&jfont_sbcs_24[0x5c * 2 * 24], &jfont_sbcs_24[0x7f * 2 * 24], 2 * 24);
			}
		} else {
			if(!MakeSbcs24Font()) {
				LOG_MSG("MSG: SBCS 12x24 font file path is not specified.\n");
			}
		}
	}
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
	PhysPt data	= PhysMake(SegValue(es), reg_si);
	uint8_t *font = &jfont_sbcs_16[reg_cl * 16];
	for(Bitu ct = 0 ; ct < 16 ; ct++) {
		mem_writeb(data++, *font++);
	}
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
	uint16_t *font = (uint16_t *)GetDbcsFont(reg_cx);
	for(Bitu ct = 0 ; ct < 16 ; ct++) {
		mem_writew(data, *font++);
		data += 2;
	}
	reg_al = 0x00;
	return CBRET_NONE;
}

static Bitu font12x24(void)
{
	PhysPt data	= PhysMake(SegValue(es), reg_si);
	uint16_t *font = (uint16_t *)GetSbcs24Font(reg_cl);
	for(Bitu ct = 0 ; ct < 24 ; ct++) {
		mem_writew(data, *font++);
		data += 2;
	}
	reg_al = 0x00;
	return CBRET_NONE;
}

static Bitu font24x24(void)
{
	PhysPt data	= PhysMake(SegValue(es), reg_si);
	uint8_t *font = GetDbcs24Font(reg_cx);
	for(Bitu ct = 0 ; ct < 24 ; ct++) {
		mem_writeb(data++, *font++);
		mem_writeb(data++, *font++);
		mem_writeb(data++, *font++);
	}
	reg_al = 0x00;
	return CBRET_NONE;
}

static Bitu write_font16x16(void)
{
	reg_al = 0x06;
	return CBRET_NONE;
}

static Bitu write_font24x24(void)
{
	reg_al = 0x06;
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
	if(!strcmp(param, "ias"))
		dosv_fep_ctrl = DOSV_FEP_CTRL_IAS;
	else if(!strcmp(param, "mskanji"))
		dosv_fep_ctrl = DOSV_FEP_CTRL_MSKANJI;
	else
		dosv_fep_ctrl = DOSV_FEP_CTRL_BOTH;
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
