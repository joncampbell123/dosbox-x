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
#include "logging.h"
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
#include <limits.h>
#if defined(LINUX) && C_X11
#include <X11/Xlib.h>
#include <X11/Xlocale.h>
#include <X11/Xutil.h>
#endif

#define ID_LEN 6
#define NAME_LEN 8
#define VTEXT_MODE_COUNT 2
#define SBCS16_LEN 256 * 16
#define SBCS19_LEN 256 * 19
#define DBCS14_LEN 65536 * 28
#define DBCS16_LEN 65536 * 32
#define DBCS24_LEN 65536 * 72
#define SBCS24_LEN 256 * 48
#define	GAIJI_COUNT	16

#if defined(LINUX) && C_X11
static Display *font_display;
static Window font_window;
static Pixmap font_pixmap;
static GC font_gc;
static XFontSet font_set16;
static XFontSet font_set14;
static XFontSet font_set24;
static XFontStruct *xfont_16 = NULL; 
#endif

const char jfont_name[] = "\x082\x06c\x082\x072\x020\x083\x053\x083\x056\x083\x062\x083\x04e";
static uint8_t jfont_dbcs[96];
int fontsize14 = 0, fontsize16 = 0, fontsize24 = 0;
uint8_t *fontdata14 = NULL, *fontdata16 = NULL, *fontdata24 = NULL;
uint8_t jfont_sbcs_16[SBCS16_LEN];//256 * 16( * 8)
uint8_t jfont_sbcs_19[SBCS19_LEN];//256 * 19( * 8)
uint8_t jfont_sbcs_24[SBCS24_LEN];//256 * 12 * 2
uint8_t jfont_dbcs_14[DBCS14_LEN];//65536 * 14 * 2 (* 8)
uint8_t jfont_dbcs_16[DBCS16_LEN];//65536 * 16 * 2 (* 8)
uint8_t jfont_dbcs_24[DBCS24_LEN];//65536 * 24 * 3
uint8_t jfont_cache_dbcs_14[65536];
uint8_t jfont_cache_dbcs_16[65536];
uint8_t jfont_cache_dbcs_24[65536];

static uint8_t jfont_yen[32];
static uint8_t jfont_kana[32*64];
static uint8_t jfont_kanji[96];
static uint16_t gaiji_seg;

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
static HFONT jfont_14;
static HFONT jfont_24;
static bool use20pixelfont;
#else
# define use20pixelfont (0) /* try to avoid a lot of #ifdefs here */
#endif
#if defined(USE_TTF)
extern bool autoboxdraw;
extern bool ttf_dosv;
#endif
extern bool showdbcs;
extern bool gbk, chinasea;
extern int bootdrive;
bool del_flag = true;
bool yen_flag = false;
bool jfont_init = false;
bool getsysfont = true;
bool getwqy14 = false;
bool getwqy16 = false;
uint8_t TrueVideoMode;
void ResolvePath(std::string& in);
void SetIMPosition();
bool isDBCSCP();
bool INT10_SetDOSVModeVtext(uint16_t mode, enum DOSV_VTEXT_MODE vtext_mode);
bool CodePageGuestToHostUTF8(char *d/*CROSS_LEN*/,const char *s/*CROSS_LEN*/);
bool CodePageGuestToHostUTF16(uint16_t *d/*CROSS_LEN*/,const char *s/*CROSS_LEN*/);
bool CodePageHostToGuestUTF16(char *d/*CROSS_LEN*/,const uint16_t *s/*CROSS_LEN*/);
std::string GetDOSBoxXPath(bool withexe=false);

bool isKanji1(uint8_t chr) {
    if (dos.loaded_codepage == 936 || IS_PDOSV)
        return chr >= (gbk ? 0x81 : 0xa1) && chr <= 0xfe;
    else if (dos.loaded_codepage == 949 || dos.loaded_codepage == 950 || dos.loaded_codepage == 951 || IS_KDOSV || IS_TDOSV)
        return chr >= 0x81 && chr <= 0xfe;
    else
        return (chr >= 0x81 && chr <= 0x9f) || (chr >= 0xe0 && chr <= 0xfc);
}

bool isKanji1_gbk(uint8_t chr) {
    if (dos.loaded_codepage != 936) return isKanji1(chr);
    bool kk = gbk, ret = false;
    gbk = true;
    ret = isKanji1(chr);
    gbk = kk;
    return ret;
}

bool isKanji2(uint8_t chr) {
#if defined(USE_TTF)
    if (dos.loaded_codepage == 936 || dos.loaded_codepage == 949 || dos.loaded_codepage == 950 || dos.loaded_codepage == 951 || ((IS_DOSV || ttf_dosv) && !IS_JDOSV))
#else
    if (dos.loaded_codepage == 936 || dos.loaded_codepage == 949 || dos.loaded_codepage == 950 || dos.loaded_codepage == 951 || (IS_DOSV && !IS_JDOSV))
#endif
        return chr >= (dos.loaded_codepage == 936 && !gbk? 0xa1 : 0x40) && chr <= 0xfe;
    else
        return (chr >= 0x40 && chr <= 0x7e) || (del_flag && chr == 0x7f) || (chr >= 0x80 && chr <= 0xfc);
}

static inline int Hex2Int(const char *p) {
    if (*p <= '9')
        return *p - '0';
    else if (*p <= 'F')
        return *p - 'A' + 10;
    else
        return *p - 'a' + 10;
}

enum type32 {
  PCF_PROPERTIES	= (1 << 0),
  PCF_ACCELERATORS	= (1 << 1),
  PCF_METRICS		= (1 << 2),
  PCF_BITMAPS		= (1 << 3),
  PCF_INK_METRICS	= (1 << 4),
  PCF_BDF_ENCODINGS	= (1 << 5),
  PCF_SWIDTHS		= (1 << 6),
  PCF_GLYPH_NAMES	= (1 << 7),
  PCF_BDF_ACCELERATORS	= (1 << 8),
};

struct format32 {
  uint32_t id    :24;	// one of four constants below
  uint32_t dummy :2;	// = 0 padding
  uint32_t scan  :2;	// read bitmap by (1 << scan) bytes
  uint32_t bit   :1;	// 0:LSBit first, 1:MSBit first
  uint32_t byte  :1;	// 0:LSByte first, 1:MSByte first
  uint32_t glyph :2;	// a scanline of gryph is aligned by (1 << glyph) bytes
  bool is_little_endian(void) { return !byte; }
};

#define PCF_DEFAULT_FORMAT     0
#define PCF_COMPRESSED_METRICS 1
const format32 BDF_format = { PCF_DEFAULT_FORMAT, 0, 0, 1, 1, 0 };

union sv {
  char *s;
  int32_t v;
};

struct metric_t {
  int16_t leftSideBearing;  // leftmost coordinate of the gryph
  int16_t rightSideBearing; // rightmost coordinate of the gryph
  int16_t characterWidth;   // offset to next gryph
  int16_t ascent;           // pixels below baseline
  int16_t descent;          // pixels above Baseline
  uint16_t attributes;
  uint8_t *bitmaps;         // bitmap pattern of gryph
  int32_t swidth;           // swidth
  sv glyphName;           // name of gryph

  metric_t(void) { bitmaps = NULL; glyphName.s = NULL; }
  int16_t widthBits(void) { return rightSideBearing - leftSideBearing; }
  int16_t height(void) { return ascent + descent; }
  int16_t widthBytes(format32 f) { return bytesPerRow(widthBits(), 1 << f.glyph); }
  static int16_t bytesPerRow(int bits, int nbytes) { return nbytes == 1 ?  ((bits +  7) >> 3) : nbytes == 2 ? (((bits + 15) >> 3) & ~1) : nbytes == 4 ? (((bits + 31) >> 3) & ~3) : nbytes == 8 ? (((bits + 63) >> 3) & ~7) : 0; }
};

int32_t nTables;
struct table_t {
  type32 type;
  format32 format;
  int32_t size;
  int32_t offset;
} *tables;
long read_bytes;
format32 format;

uint8_t *read_byte8s(FILE *file, uint8_t *mem, size_t size) {
  size_t read_size =  fread(mem, 1, size, file);
  if (read_size != size) return NULL;
  read_bytes += size;
  return mem;
}

char read8(FILE *file) {
  int a = fgetc(file);
  read_bytes ++;
  return (char)a;
}

int make_int16(int a, int b) {
  int value;
  value  = (a & 0xff) << 8;
  value |= (b & 0xff);
  return value;
}

int read_int16(FILE *file) {
  int a = read8(file);
  int b = read8(file);
  if (format.is_little_endian())
    return make_int16(b, a);
  else
    return make_int16(a, b);
}

int32_t make_int32(int a, int b, int c, int d) {
  int32_t value;
  value  = (int32_t)(a & 0xff) << 24;
  value |= (int32_t)(b & 0xff) << 16;
  value |= (int32_t)(c & 0xff) <<  8;
  value |= (int32_t)(d & 0xff);
  return value;
}

int32_t read_int32_big(FILE *file) {
  int a = read8(file);
  int b = read8(file);
  int c = read8(file);
  int d = read8(file);
  return make_int32(a, b, c, d);
}

int32_t read_int32_little(FILE *file) {
  int a = read8(file);
  int b = read8(file);
  int c = read8(file);
  int d = read8(file);
  return make_int32(d, c, b, a);
}

int32_t read_int32(FILE *file) {
  return format.is_little_endian() ? read_int32_little(file) : read_int32_big(file);
}

format32 read_format32_little(FILE *file) {
  int32_t v = read_int32_little(file);
  format32 f;
  f.id     = v >> 8;
  f.dummy  = 0;
  f.scan   = v >> 4;
  f.bit    = v >> 3;
  f.byte   = v >> 2;
  f.glyph  = v >> 0;
  return f;
}

void bit_order_invert(uint8_t *data, int size) {
  static const uint8_t invert[16] = { 0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15 };
  for (int i = 0; i < size; i++)
    data[i] = (invert[data[i] & 15] << 4) | invert[(data[i] >> 4) & 15];
}

void two_byte_swap(uint8_t *data, int size) {
  size &= ~1;
  for (int i = 0; i < size; i += 2) {
    uint8_t tmp = data[i];
    data[i] = data[i + 1];
    data[i + 1] = tmp;
  }
}

void four_byte_swap(uint8_t *data, int size) {
  size &= ~3;
  for (int i = 0; i < size; i += 4) {
    uint8_t tmp = data[i];
    data[i] = data[i + 3];
    data[i + 3] = tmp;
    tmp = data[i + 1];
    data[i + 1] = data[i + 2];
    data[i + 2] = tmp;
  }
}

bool seek(FILE *file, type32 type) {
  for (int i = 0; i < nTables; i++)
    if (tables[i].type == type) {
      int s = tables[i].offset - read_bytes;
      if (s < 0)
          return false;
      for (; 0 < s; s--) read8(file);
      return true;
    }
  return false;
}

void read_metric(FILE *file, metric_t *m, bool compressed) {
  m->leftSideBearing  = compressed ? (int16_t)((uint8_t)read8(file) - 0x80) : read_int16(file);
  m->rightSideBearing = compressed ? (int16_t)((uint8_t)read8(file) - 0x80) : read_int16(file);
  m->characterWidth   = compressed ? (int16_t)((uint8_t)read8(file) - 0x80) : read_int16(file);
  m->ascent           = compressed ? (int16_t)((uint8_t)read8(file) - 0x80) : read_int16(file);
  m->descent          = compressed ? (int16_t)((uint8_t)read8(file) - 0x80) : read_int16(file);
  m->attributes       = compressed ? 0 : read_int16(file);
}

bool readPCF(FILE *file, int height) {
  if (file) rewind(file);
  else return false;
  read_bytes = 0;
  int i;
  int32_t nMetrics, version = read_int32_big(file);
  if (read_bytes == 0) version = read_int32_big(file);
  if (version != make_int32(1, 'f', 'c', 'p')) return false;
  nTables = read_int32_little(file);
  if ((tables = new table_t[nTables]) == NULL) return false;
  for (i = 0; i < nTables; i++) {
    tables[i].type   = (type32)read_int32_little(file);
    tables[i].format = read_format32_little(file);
    tables[i].size   = read_int32_little(file);
    tables[i].offset = read_int32_little(file);
  }
  if (!seek(file, PCF_PROPERTIES) || !(format.id == PCF_DEFAULT_FORMAT) || !seek(file, PCF_METRICS)) return false;
  format = read_format32_little(file);
  metric_t *metrics;
  switch (format.id) {
    default:
      return false;
    case PCF_DEFAULT_FORMAT:
      nMetrics = read_int32(file);
      if ((metrics = new metric_t[nMetrics]) == NULL) return false;
      for (i = 0; i < nMetrics; i++)
        read_metric(file, &metrics[i], false);
      break;
    case PCF_COMPRESSED_METRICS:
      nMetrics = read_int16(file);
      if ((metrics = new metric_t[nMetrics]) == NULL) return false;
      for (i = 0; i < nMetrics; i++)
        read_metric(file, &metrics[i], true);
      break;
  }
  if (!seek(file, PCF_BITMAPS)) return false;
  format = read_format32_little(file);
  if (!(format.id == PCF_DEFAULT_FORMAT)) return false;
  int32_t nBitmaps = read_int32(file);
  uint32_t *bitmapOffsets, bitmapSizes[4];
  if ((bitmapOffsets = new uint32_t[nBitmaps]) == NULL) return false;
  for (i = 0; i < nBitmaps; i++)
    bitmapOffsets[i] = (uint32_t)read_int32(file);
  for (i = 0; i < 4; i++)
    bitmapSizes[i] = (uint32_t)read_int32(file);
  uint8_t *bitmaps;
  int32_t bitmapSize = bitmapSizes[format.glyph];
  if ((bitmaps = new uint8_t[bitmapSize]) == NULL) return false;
  if (read_byte8s(file, bitmaps, bitmapSize) == NULL) {delete[] bitmaps;return false;}
  if (format.bit != BDF_format.bit) bit_order_invert(bitmaps, bitmapSize);
  if ((format.bit == format.byte) !=  (BDF_format.bit == BDF_format.byte)) {
    switch (1 << (BDF_format.bit == BDF_format.byte ? format.scan : BDF_format.scan)) {
      case 1: break;
      case 2:
        two_byte_swap(bitmaps, bitmapSize);
        break;
      case 4:
        four_byte_swap(bitmaps, bitmapSize);
        break;
    }
  }
  for (i = 0; i < nMetrics; i++) {
    metric_t &m = metrics[i];
    m.bitmaps = bitmaps + bitmapOffsets[i];
  }
  if (!seek(file, PCF_BDF_ENCODINGS)) {delete[] bitmaps;return false;}
  format = read_format32_little(file);
  if (!(format.id == PCF_DEFAULT_FORMAT)) {delete[] bitmaps;return false;}
  uint16_t firstCol  = read_int16(file);
  uint16_t lastCol   = read_int16(file);
  uint16_t firstRow  = read_int16(file);
  uint16_t lastRow   = read_int16(file);
  uint16_t defaultCh = read_int16(file);
  if (!(firstCol <= lastCol) || !(firstRow <= lastRow)) {delete[] bitmaps;return false;}
  int nEncodings = (lastCol - firstCol + 1) * (lastRow - firstRow + 1);
  uint16_t *encodings;
  if ((encodings = new uint16_t[nEncodings]) == NULL) {delete[] bitmaps;return false;}
  for (i = 0; i < nEncodings; i++) encodings[i] = read_int16(file);
  if (seek(file, PCF_SWIDTHS)) {
    format = read_format32_little(file);
    if (!(format.id == PCF_DEFAULT_FORMAT)) {delete[] bitmaps;return false;}
  }
  if (seek(file, PCF_GLYPH_NAMES)) {
    format = read_format32_little(file);
    if (!(format.id == PCF_DEFAULT_FORMAT)) {delete[] bitmaps;return false;}
  }
  bool apply14 = false;
  if (height == 16) {
      Prop_path* pathprop = static_cast<Section_prop *>(control->GetSection("dosv"))->Get_path("fontxdbcs14");
      if(pathprop && !pathprop->realpath.size()) apply14 = true;
  }
  for (i = 0; i < nEncodings; i++) {
    if (encodings[i] == 0xffff) continue;
    int col = i % (lastCol - firstCol + 1) + firstCol;
    int row = i / (lastCol - firstCol + 1) + firstRow;
    uint16_t charcode = row * 256 + col;
    if (!(encodings[i] < nMetrics)) {delete[] bitmaps;return false;}
    metric_t &m = metrics[encodings[i]];
    int widthBytes = m.widthBytes(format);
    int w = (m.widthBits() + 7) / 8;
    w = w < 1 ? 1 : w;
    uint8_t *b = m.bitmaps;
    unsigned char *bitmap;
    if ((m.height() == height || m.height() == 15) && charcode >= 0x100 && charcode <= 0xffff) {
        bitmap = (unsigned char *)malloc(100 * sizeof(unsigned char));
        size_t j=0;
        for (int r = 0; r < m.height(); r++)
          for (int c = 0; c < widthBytes; c++) {
            if (c < w) bitmap[j++]=*b;
            b++;
          }
        if (j != 30 && j != height * 2) continue;
        char text[10];
        uint16_t uname[4];
        uname[0]=charcode;
        uname[1]=0;
        text[0] = 0;
        text[1] = 0;
        text[2] = 0;
        if (CodePageHostToGuestUTF16(text,uname)) {
            Bitu code = (text[0] & 0xff) * 0x100 + (text[1] & 0xff);
            if ((height == 14 || (height == 16 && m.height() == 15 && apply14)) && jfont_cache_dbcs_14[code] == 0) {
                memcpy(&jfont_dbcs_14[code * 28], bitmap, 28);
                jfont_cache_dbcs_14[code] = 1;
            }
            if (height == 16 && jfont_cache_dbcs_16[code] == 0) {
                memcpy(&jfont_dbcs_16[code * 32], bitmap, height == 15?30:32);
                jfont_cache_dbcs_16[code] = 1;
            }
        }
    }
  }
  delete[] bitmaps;
  return true;
}

bool readBDF(FILE *file, int height) {
    if (file) rewind(file);
    else return false;
    char linebuf[1024], *s, *p;
    int fontboundingbox_width, fontboundingbox_height, fontboundingbox_xoff, fontboundingbox_yoff;
    int chars, i, j, n, scanline, encoding, bbx, bby, bbw, bbh, width;
    unsigned *width_table, *encoding_table;
    unsigned char *bitmap;
    fontboundingbox_width = fontboundingbox_height = fontboundingbox_xoff = fontboundingbox_yoff = chars = 0;
    while (fgets(linebuf, sizeof(linebuf), file) && (s = strtok(linebuf, " \t\n\r"))) {
        if (!strcasecmp(s, "FONTBOUNDINGBOX")) {
            p = strtok(NULL, " \t\n\r");
            fontboundingbox_width = atoi(p);
            p = strtok(NULL, " \t\n\r");
            fontboundingbox_height = atoi(p);
            p = strtok(NULL, " \t\n\r");
            fontboundingbox_xoff = atoi(p);
            p = strtok(NULL, " \t\n\r");
            fontboundingbox_yoff = atoi(p);
        } else if (!strcasecmp(s, "CHARS")) {
            p = strtok(NULL, " \t\n\r");
            chars = atoi(p);
            break;
        }
    }
    if (fontboundingbox_width <= 0 || fontboundingbox_height <= 0 || chars <= 0) return false;
    width_table = (unsigned *)malloc(chars * sizeof(*width_table));
    encoding_table = (unsigned *)malloc(chars * sizeof(*encoding_table));
    bitmap = (unsigned char *)malloc(((fontboundingbox_width + 7) / 8) * fontboundingbox_height);
    if (!width_table || !encoding_table || !bitmap) return false;
    scanline = encoding = -1;
    n = bbx = bby = bbw = bbh = 0;
    width = INT_MIN;
    bool apply14 = false;
    if (height == 16) {
        Prop_path* pathprop = static_cast<Section_prop *>(control->GetSection("dosv"))->Get_path("fontxdbcs14");
        if(pathprop && !pathprop->realpath.size()) apply14 = true;
    }
    while (fgets(linebuf, sizeof(linebuf), file) && (s = strtok(linebuf, " \t\n\r"))) {
        if (!strcasecmp(s, "STARTCHAR")) {
            p = strtok(NULL, " \t\n\r");
        } else if (!strcasecmp(s, "ENCODING")) {
            p = strtok(NULL, " \t\n\r");
            encoding = atoi(p);
        } else if (!strcasecmp(s, "DWIDTH")) {
            p = strtok(NULL, " \t\n\r");
            width = atoi(p);
        } else if (!strcasecmp(s, "BBX")) {
            p = strtok(NULL, " \t\n\r");
            bbw = atoi(p);
            p = strtok(NULL, " \t\n\r");
            bbh = atoi(p);
            p = strtok(NULL, " \t\n\r");
            bbx = atoi(p);
            p = strtok(NULL, " \t\n\r");
            bby = atoi(p);
        } else if (!strcasecmp(s, "BITMAP")) {
            if (width == INT_MIN) return false;
            if (bbx < 0) {
                width -= bbx;
                bbx = 0;
            }
            if (bbx + bbw > width) width = bbx + bbw;
            width_table[n] = width;
            encoding_table[n] = encoding;
            ++n;
            scanline = 0;
            memset(bitmap, 0, ((fontboundingbox_width + 7) / 8) * fontboundingbox_height);
        } else if (!strcasecmp(s, "ENDCHAR")) {
            if (bbx && !(bbx < 0 || bbx > 7)) {
                int x, y, c, o;
                for (y = 0; y < fontboundingbox_height; ++y) {
                    o = 0;
                    for (x = 0; x < fontboundingbox_width; x += 8) {
                        c = bitmap[y * ((fontboundingbox_width + 7) / 8) + x / 8];
                        bitmap[y * ((fontboundingbox_width + 7) / 8) + x / 8] = c >> bbx | o;
                        o = c << (8 - bbx);
                    }
                }
            }
            if ((width == height || width == 15) && encoding >= 0x100 && encoding <= 0xffff) {
                char text[10];
                uint16_t uname[4];
                uname[0]=encoding;
                uname[1]=0;
                text[0] = 0;
                text[1] = 0;
                text[2] = 0;
                if (CodePageHostToGuestUTF16(text,uname)) {
                    Bitu code = (text[0] & 0xff) * 0x100 + (text[1] & 0xff);
                    if ((height == 14 || (height == 16 && width == 15 && apply14)) && jfont_cache_dbcs_14[code] == 0) {
                        memcpy(&jfont_dbcs_14[code * 28], bitmap, 28);
                        jfont_cache_dbcs_14[code] = 1;
                    }
                    if (height == 16 && jfont_cache_dbcs_16[code] == 0) {
                        memcpy(&jfont_dbcs_16[code * 32], bitmap, height == 15?30:32);
                        jfont_cache_dbcs_16[code] = 1;
                    }
                }
            }
            scanline = -1;
            width = INT_MIN;
        } else if (scanline >= 0) {
            p = s;
            j = 0;
            while (*p) {
                i = Hex2Int(p);
                ++p;
                if (*p)
                    i = Hex2Int(p) | i * 16;
                else {
                    bitmap[j + scanline * ((fontboundingbox_width + 7) / 8)] = i;
                    break;
                }
                bitmap[j + scanline * ((fontboundingbox_width + 7) / 8)] = i;
                ++j;
                ++p;
            }
            ++scanline;
        }
    }
    return true;
}

Bitu getfontx2header(FILE *fp, fontx_h *header) {
    fread(header->id, ID_LEN, 1, fp);
    if (strncmp(header->id, "FONTX2", ID_LEN) != 0)
        return 1;
    fread(header->name, NAME_LEN, 1, fp);
    header->width = (uint8_t)getc(fp);
    header->height = (uint8_t)getc(fp);
    header->type = (uint8_t)getc(fp);
    return 0;
}

uint16_t chrtosht(FILE *fp) {
	uint16_t i, j;
	i = (uint8_t)getc(fp);
	j = (uint8_t)getc(fp) << 8;
	return(i | j);
}

void readfontxtbl(fontxTbl *table, Bitu size, FILE *fp) {
    while (size > 0) {
        table->start = chrtosht(fp);
        table->end = chrtosht(fp);
        ++table;
        --size;
    }
}

static bool LoadFontxFile(const char *fname, int height, bool dbcs) {
    fontx_h head;
    fontxTbl *table;
    Bitu code;
    uint8_t size;
	if (!fname) return false;
	if(*fname=='\0') return false;
	FILE * mfile=fopen(fname,"rb");
	std::string config_path, res_path, exepath=GetDOSBoxXPath();
	Cross::GetPlatformConfigDir(config_path), Cross::GetPlatformResDir(res_path);
	if (!mfile && exepath.size()) mfile=fopen((exepath + fname).c_str(),"rb");
	if (!mfile && config_path.size()) mfile=fopen((config_path + fname).c_str(),"rb");
	if (!mfile && res_path.size()) mfile=fopen((res_path + fname).c_str(),"rb");
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
        if (!dbcs) {
            fseek(mfile, 0L, SEEK_END);
            long int sz = ftell(mfile);
            rewind(mfile);
            if (sz == 256 * 15) {
                if (height == 16)
                    for(int i = 0 ; i < 256 ; i++) fread(&jfont_sbcs_16[i * 16], sizeof(uint8_t), 15, mfile);
                else if(height == 19)
                    for(int i = 0 ; i < 256 ; i++) fread(&jfont_sbcs_19[i * 19 + 2], sizeof(uint8_t), 15, mfile);
                if (height == 16 || height == 19) {
                    fclose(mfile);
                    return true;
                }
            } else if (sz == SBCS16_LEN) {
                if(height == 16)
                    for(int i = 0 ; i < 256 ; i++) fread(&jfont_sbcs_16[i * 16], sizeof(uint8_t), 16, mfile);
                else if(height == 19)
                    for(int i = 0 ; i < 256 ; i++) fread(&jfont_sbcs_19[i * 19 + 1], sizeof(uint8_t), 16, mfile);
                if (height == 16 || height == 19) {
                    fclose(mfile);
                    return true;
                }
            } else if ((sz == SBCS24_LEN || sz == 16384) && height == 24) {
                fread(jfont_sbcs_24, sizeof(uint8_t), SBCS24_LEN, mfile);
                fclose(mfile);
                return true;
            }
        } else if ((height==14||height==16) && isDBCSCP() && (readBDF(mfile, height) || readPCF(mfile, height))) {
            fclose(mfile);
            return true;
        } else if (dos.loaded_codepage == 936 || dos.loaded_codepage == 950 || dos.loaded_codepage == 951) {
            fseek(mfile, 0L, SEEK_END);
            long int sz = ftell(mfile);
            rewind(mfile);
            if (height==14) {
                if (!sz||((sz%14)&&(sz%15))) {fclose(mfile);return false;}
                fontdata14 = (uint8_t *)malloc(sizeof(uint8_t)*sz);
                if (!fontdata14) {fclose(mfile);return false;}
                fread(fontdata14, sizeof(uint8_t), sz, mfile);
                fontsize14 = sizeof(uint8_t)*sz;
                fclose(mfile);
                return true;
            } else if (height==16) {
                if (!sz||((sz%15)&&(sz%16))) {fclose(mfile);return false;}
                fontdata16 = (uint8_t *)malloc(sizeof(uint8_t)*sz);
                if (!fontdata16) {fclose(mfile);return false;}
                fread(fontdata16, sizeof(uint8_t), sz, mfile);
                fontsize16 = sizeof(uint8_t)*sz;
                fclose(mfile);
                return true;
            } else if (height==24) {
                if (!sz||(sz%24)) {fclose(mfile);return false;}
                fontdata24 = (uint8_t *)malloc(sizeof(uint8_t)*sz);
                if (!fontdata24) {fclose(mfile);return false;}
                fread(fontdata24, sizeof(uint8_t), sz, mfile);
                fontsize24 = sizeof(uint8_t)*sz;
                fclose(mfile);
                return true;
            }
        }
		fclose(mfile);
		LOG_MSG("MSG: no correct FONTX2 header found\n");
		return false;
	}
	// switch whether the font is DBCS or not
	if (head.type == 1 && dbcs) {
		if (head.width == 14 && head.height == 14 && height == 14) {
			size = getc(mfile);
			table = (fontxTbl *)calloc(size, sizeof(fontxTbl));
			readfontxtbl(table, size, mfile);
			for (Bitu i = 0; i < size; i++) {
				for (code = table[i].start; code <= table[i].end; code++) {
					fread(&jfont_dbcs_14[code * 28], sizeof(uint8_t), 28, mfile);
					jfont_cache_dbcs_14[code] = 1;
				}
			}
		} else if (head.width == 16 && head.height == 16 && height == 16) {
			size = getc(mfile);
			table = (fontxTbl *)calloc(size, sizeof(fontxTbl));
			readfontxtbl(table, size, mfile);
			for (Bitu i = 0; i < size; i++) {
				for (code = table[i].start; code <= table[i].end; code++) {
					fread(&jfont_dbcs_16[code * 32], sizeof(uint8_t), 32, mfile);
					jfont_cache_dbcs_16[code] = 1;
				}
			}
		} else if (head.width == 24 && head.height == 24 && height == 24) {
			size = getc(mfile);
			table = (fontxTbl *)calloc(size, sizeof(fontxTbl));
			readfontxtbl(table, size, mfile);
			for (Bitu i = 0; i < size; i++) {
				for (code = table[i].start ; code <= table[i].end ; code++) {
					fread(&jfont_dbcs_24[code * 72], sizeof(uint8_t), 72, mfile);
					jfont_cache_dbcs_24[code] = 1;
				}
			}
		} else {
			fclose(mfile);
			LOG_MSG("MSG: FONTX2 DBCS font size is not correct\n");
			return false;
		}
	}
	else if (!dbcs) {
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
			} else {
				fclose(mfile);
				LOG_MSG("MSG: FONTX2 SBCS font size is not correct\n");
				return false;
			}
		} else if (head.width == 12 && head.height == 24 && height == 24) {
			fread(jfont_sbcs_24, sizeof(uint8_t), SBCS24_LEN, mfile);
		} else {
			fclose(mfile);
			LOG_MSG("MSG: FONTX2 SBCS font size is not correct\n");
			return false;
		}
	}
	//LOG_MSG("Loaded FONTX2 file (width %d and height %d): %s\n", head.width, head.height, fname);
	fclose(mfile);
	return true;

}

bool CheckEmptyData(uint8_t *data, Bitu length)
{
	while(length > 0) {
		if(*data++ != 0) {
			return false;
		}
		length--;
	}
	return true;
}

#if defined(LINUX) && C_X11
static uint8_t linux_symbol_16[] = {
// 0x815f
  0x80, 0x00, 0x40, 0x00, 0x20, 0x00, 0x10, 0x00, 0x08, 0x00, 0x04, 0x00, 0x02, 0x00, 0x01, 0x00,
  0x00, 0x80, 0x00, 0x40, 0x00, 0x20, 0x00, 0x10, 0x00, 0x08, 0x00, 0x04, 0x00, 0x02, 0x00, 0x01,
// 0x8191
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x03, 0xe0, 0x04, 0x90,
  0x08, 0x80, 0x08, 0x80, 0x08, 0x80, 0x08, 0x80, 0x04, 0x90, 0x03, 0xe0, 0x00, 0x80, 0x00, 0x00,
// 0x8192
  0x00, 0x00, 0x01, 0xe0, 0x02, 0x10, 0x04, 0x00, 0x04, 0x00, 0x04, 0x00, 0x04, 0x00, 0x04, 0x00,
  0x3f, 0xe0, 0x04, 0x00, 0x04, 0x00, 0x04, 0x00, 0x04, 0x00, 0x08, 0x00, 0x1f, 0xf8, 0x00, 0x00,
// 0x81ca
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xfc, 0x00, 0x04,
  0x00, 0x04, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static uint8_t linux_symbol_14[] = {
// 0x815f
  0x80, 0x00, 0x40, 0x00, 0x20, 0x00, 0x10, 0x00, 0x08, 0x00, 0x04, 0x00, 0x02, 0x00, 0x01, 0x00,
  0x00, 0x80, 0x00, 0x40, 0x00, 0x20, 0x00, 0x10, 0x00, 0x08, 0x00, 0x04,
// 0x8191
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x0f, 0x80, 0x12, 0x40, 0x22, 0x00, 0x22, 0x00,
  0x22, 0x00, 0x22, 0x00, 0x12, 0x40, 0x0f, 0x80, 0x02, 0x00, 0x00, 0x00,
// 0x8192
  0x00, 0x00, 0x03, 0xc0, 0x04, 0x20, 0x08, 0x00, 0x08, 0x00, 0x08, 0x00, 0x08, 0x00, 0x3f, 0xc0, 
  0x08, 0x00, 0x08, 0x00, 0x08, 0x00, 0x10, 0x00, 0x3f, 0xf0, 0x00, 0x00,
// 0x81ca
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xf0, 0x00, 0x10, 0x00, 0x10,
  0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static uint8_t linux_symbol_24[] = {
// 0x815f
  0x80, 0x00, 0x00, 0x40, 0x00, 0x00, 0x20, 0x00, 0x00, 0x10, 0x00, 0x00, 0x08, 0x00, 0x00, 0x04, 0x00, 0x00, 0x02, 0x00, 0x00, 0x01, 0x00, 0x00,
  0x00, 0x80, 0x00, 0x00, 0x40, 0x00, 0x00, 0x20, 0x00, 0x00, 0x10, 0x00, 0x00, 0x08, 0x00, 0x00, 0x04, 0x00, 0x00, 0x02, 0x00, 0x00, 0x01, 0x00,
  0x00, 0x00, 0x80, 0x00, 0x00, 0x40, 0x00, 0x00, 0x20, 0x00, 0x00, 0x10, 0x00, 0x00, 0x08, 0x00, 0x00, 0x04, 0x00, 0x00, 0x02, 0x00, 0x00, 0x01,
// 0x8191
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00,
  0x00, 0x08, 0x00, 0x00, 0x7f, 0x00, 0x00, 0xc8, 0x80, 0x01, 0x88, 0xc0, 0x01, 0x88, 0x40, 0x03, 0x08, 0x00, 0x03, 0x08, 0x00, 0x03, 0x08, 0x00,
  0x03, 0x08, 0x00, 0x01, 0x88, 0x40, 0x01, 0x88, 0xc0, 0x00, 0xc9, 0x80, 0x00, 0x7f, 0x00, 0x00, 0x08, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00,
// 0x8192
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0x00, 0x00, 0x40, 0x80, 0x00, 0x80, 0x40, 0x01, 0x80, 0x00, 0x01, 0x80, 0x00, 0x01, 0x80, 0x00,
  0x01, 0x80, 0x00, 0x01, 0x80, 0x00, 0x01, 0x80, 0x00, 0x01, 0x80, 0x00, 0x01, 0x80, 0x00, 0x1f, 0xff, 0x00, 0x01, 0x80, 0x00, 0x01, 0x80, 0x00,
  0x01, 0x80, 0x00, 0x01, 0x80, 0x00, 0x03, 0x80, 0x00, 0x03, 0x00, 0x00, 0x07, 0x00, 0x00, 0x0f, 0xff, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
// 0x81ca
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0xff, 0xf8, 0x00, 0x00, 0x08, 0x00, 0x00, 0x08, 0x00, 0x00, 0x08, 0x00, 0x00, 0x08, 0x00, 0x00, 0x08,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

bool CheckLinuxSymbol(Bitu code, uint8_t *buff, int width, int height)
{
	if (!IS_JDOSV && dos.loaded_codepage != 932)
		return false;
	uint8_t *src;
	int len, offset;
	if(width == 16 && height == 16) {
		src = linux_symbol_16;
		len = 32;
	} else if(width == 14 && height == 14) {
		src = linux_symbol_14;
		len = 28;
	} else if(width == 24 && height == 24) {
		src = linux_symbol_24;
		len = 72;
	} else {
		return false;
	}
	offset = -1;
	if(code == 0x815f) {
		offset = 0;
	} else if(code == 0x8191) {
		offset = len;
	} else if(code == 0x8192) {
		offset = len * 2;
	} else if(code == 0x81ca) {
		offset = len * 3;
	}
	if(offset != -1) {
		memcpy(buff, src + offset, len);
		return true;
	}
	return false;
}
#endif

bool GetWindowsFont(Bitu code, uint8_t *buff, int width, int height)
{
    if (!getsysfont) return false;
#if defined(LINUX) && C_X11
	XRectangle ir, lr;
	XImage *image;
	size_t len;
	wchar_t wtext[4];
	if((height == 24 && font_set24 == NULL) || (height == 14 && font_set14 == NULL) || (height != 24 && height != 14 && font_set16 == NULL)) return false;
	if(code < 0x100) {
		if(code == 0x5c && (IS_JDOSV || dos.loaded_codepage == 932)) // yen
			wtext[0] = 0xa5;
		else if(code >= 0xa1 && code <= 0xdf && (IS_JDOSV || dos.loaded_codepage == 932)) // half kana
			wtext[0] = 0xff61 + (code - 0xa1);
		else
			wtext[0] = code;
	} else if(code == 0x8160 && (IS_JDOSV || dos.loaded_codepage == 932)) {
		wtext[0] = 0x301c;
	} else if(code == 0x8161 && (IS_JDOSV || dos.loaded_codepage == 932)) {
		wtext[0] = 0x2016;
	} else if(code == 0x817c && (IS_JDOSV || dos.loaded_codepage == 932)) {
		wtext[0] = 0x2212;
	} else if(CheckLinuxSymbol(code, buff, width, height)) {
		return true;
	} else {
		char src[4];
		src[0] = code >> 8;
		src[1] = code & 0xff;
		src[2] = 0;
        if (!CodePageGuestToHostUTF16((uint16_t *)wtext,src)) return false;
	}
	memset(buff, 0, (width / 8) * height);
	XSetForeground(font_display, font_gc, BlackPixel(font_display, 0));
	XFillRectangle(font_display, font_pixmap, font_gc, 0, 0, 32, 32);
	XSetForeground(font_display, font_gc, WhitePixel(font_display, 0));
	if(xfont_16 != NULL && width == 16) {
		int direction, ascent, descent;
		XCharStruct xc;
		XChar2b ch[2];
		ch[0].byte1 = (wtext[0] >> 8) & 0xff;
		ch[0].byte2 = wtext[0] & 0xff;
		ch[1].byte1 = 0x00;
		ch[1].byte1 = 0x5d;
	    XSetFont(font_display, font_gc, xfont_16->fid); 
		XTextExtents16(xfont_16, ch, 2, &direction, &ascent, &descent, &xc);
		XDrawString16(font_display, font_pixmap, font_gc, 0, ascent, ch, 2);
		lr.height = height;
	} else {
		XFontSet fontset = font_set16;
		if(height == 24)
			fontset = font_set24;
		else if(height == 14 && !IS_PDOSV)
			fontset = font_set14;
		wtext[1] = ']';
		wtext[2] = 0;
		len = 2;
		XwcTextExtents(fontset, wtext, len, &ir, &lr);
		if(lr.width <= width) return false;
		XwcDrawString(font_display, font_pixmap, fontset, font_gc, 0, lr.height - (ir.height + ir.y), wtext, len);
	}
	image = XGetImage(font_display, font_pixmap, (height == 14 && IS_PDOSV) ? 1 : 0, 0, width, lr.height, ~0, XYPixmap);
	if(image != NULL) {
		int x, y;
		for(y = 0 ; y < height ; y++) {
			uint8_t data = 0;
			uint8_t mask = 0x01;
			uint8_t font_mask = 0x80;
			uint8_t *pt = (unsigned char *)image->data + y * image->bytes_per_line;
			for(x = 0 ; x < width ; x++) {
				uint8_t idata;
				if(height == 14 && IS_PDOSV) {
					idata = *(pt + image->bytes_per_line);
					if(y == 0) {
						idata |= *pt;
					} else if(y == 13) {
						idata |= *(pt + image->bytes_per_line * 2);
					}
				} else {
					idata = *pt;
				}
				if(idata & mask) {
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
			if(width == 12 || width == 14) {
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
	HFONT font = (height == 16) ? jfont_16 : (height == 24) ? jfont_24 : jfont_14;
	if(font != NULL) {
		HDC hdc = GetDC(NULL);
		HFONT old_font = (HFONT)SelectObject(hdc, font);

		TEXTMETRIC tm;
		GetTextMetrics(hdc, &tm);
		GLYPHMETRICS gm;
		CONST MAT2 mat = { {0,1},{0,0},{0,0},{0,1} };
		long size = GetGlyphOutline(hdc, code, GGO_BITMAP, &gm, 0, NULL, &mat);
		if(size > 0) {
			char *fontbuff = new char[size];
			memset(fontbuff, 0, size);
			GetGlyphOutline(hdc, code, GGO_BITMAP, &gm, size, fontbuff, &mat);

			Bitu off_y = tm.tmAscent - gm.gmptGlyphOrigin.y;
			Bitu pos = off_y;
			Bitu count = (1 + (gm.gmBlackBoxX / 32)) * 4;
			if(width >= 14 || (width == 12 && height == 24)) {
				pos += off_y;
				if(width == 24) {
					pos += off_y;
				}
				if(use20pixelfont && height == 24) {
					pos += 4;
					if(width == 24) {
						pos += 2;
					}
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
				if(width >= 14 || (width == 12 && height == 24)) {
					buff[pos++] = (data >> 8) & 0xff;
					if(width == 24) {
						buff[pos++] = data & 0xff;
					}
				}
			}
			delete [] fontbuff;
			SelectObject(hdc, old_font);
			ReleaseDC(NULL, hdc);
		} else {
			SelectObject(hdc, old_font);
			ReleaseDC(NULL, hdc);
			return false;
		}
		return true;
	}
#endif
	return false;
}

int GetConvertedCode(Bitu code, int codepage, bool tounicode = false) {
    char text[10];
    uint16_t uname[4];
    uname[0]=0;
    uname[1]=0;
    text[0] = code >> 8;
    text[1] = code & 0xff;
    text[2] = 0;
    if (CodePageGuestToHostUTF16(uname,text)) {
        int cp = dos.loaded_codepage;
        dos.loaded_codepage = codepage;
        if (CodePageHostToGuestUTF16(text,uname)) {
            code = (text[0] & 0xff) * 0x100 + (text[1] & 0xff);
            dos.loaded_codepage = cp;
            return code;
        } else {
            dos.loaded_codepage = cp;
            return tounicode?-uname[0]:0;
        }
    }
    return 0;
}

void GetDbcsFrameFont(Bitu code, uint8_t *buff)
{
	if(!IS_JDOSV && dos.loaded_codepage != 932) code = GetConvertedCode(code, 932, true);
	if(code >= 0x849f && code <= 0x84be) {
		memcpy(buff, &frame_font_data[(code - 0x849f) * 32], 32);
	}
	int data[] = {0x2550, 0x2551, 0x2554, 0x2557, 0x255a, 0x255d};
	for (int i=0; i<6; i++)
		if ((int)code == -data[i]) {
			memcpy(buff, &double_frame_font_data[i * 32], 32);
			break;
		}
}

void GetDbcs14FrameFont(Bitu code, uint8_t *buff)
{
	if(!IS_JDOSV && dos.loaded_codepage != 932) code = GetConvertedCode(code, 932, true);
	if(code >= 0x849f && code <= 0x84be) {
		memcpy(buff, &frame_font_data[(code - 0x849f) * 32 + 2], 28);
	}
	int data[] = {0x2550, 0x2551, 0x2554, 0x2557, 0x255a, 0x255d};
	for (int i=0; i<6; i++)
		if ((int)code == -data[i]) {
			memcpy(buff, &double_frame_font_data[i * 32 + 2], 28);
			break;
		}
}

void GetDbcs24FrameFont(Bitu code, uint8_t *buff)
{
	if(!IS_JDOSV && dos.loaded_codepage != 932) code = GetConvertedCode(code, 932);
	if(code >= 0x849f && code <= 0x84be) {
		memcpy(buff, &frame_font24_data[(code - 0x849f) * 72], 72);
	}
}

bool isFrameFont(int code, int height) {
    if (!IS_JDOSV && dos.loaded_codepage != 932) code = GetConvertedCode(code, 932, true);
	if ((height == 14 || height == 16) && (code == -0x2550 || code == -0x2551 || code == -0x2554 || code == -0x2557 || code == -0x255a || code == -0x255d)) return true;
    return code >= 0x849f && code <= 0x84be;
}

bool isUserFont(Bitu code) {
    return ((code >= 0x8140 && code <= 0xa0fe) || (code >= 0xc6a1 && code <= 0xc8fe) || (code >= 0xfa40 && code <= 0xfefe)) && (IS_TDOSV || dos.loaded_codepage == 950 || dos.loaded_codepage == 951) && !chinasea;
}

uint8_t *GetDbcsFont(Bitu code)
{
	memset(jfont_dbcs, 0, sizeof(jfont_dbcs));
	if ((IS_JDOSV || dos.loaded_codepage == 932) && del_flag && (code & 0xFF) == 0x7F) code++;
	if(jfont_cache_dbcs_16[code] == 0) {
        if (fontdata16 && fontsize16) {
            if (dos.loaded_codepage == 936 && !(fontsize16%16) && (code/0x100)>0xa0 && (code/0x100)<0xff && (code%0x100)>0xa0 && (code%0x100)<0xfe) {
                int offset = (94 * (unsigned int)((code/0x100) - 0xa0 - 1) + ((code%0x100) - 0xa0 - 1)) * 32;
                if (offset + 32 <= fontsize16) {
                    memcpy(&jfont_dbcs_16[code * 32], fontdata16+offset, 32);
                    jfont_cache_dbcs_16[code] = 1;
                    return &jfont_dbcs_16[code * 32];
                }
            }
            if (((dos.loaded_codepage == 936 && gbk) || dos.loaded_codepage == 950 || dos.loaded_codepage == 951) && !(fontsize16%15) && isKanji1(code/0x100)) {
                Bitu c = code;
                if (dos.loaded_codepage == 936) code = GetConvertedCode(code, 950);
                int offset = -1, ser = (code/0x100 - 161) * 157 + ((code%0x100) - ((code%0x100)>160?161:64)) + ((code%0x100)>160?64:1);
                if (code) {
                    if (ser >= 472 && ser <= 5872) offset = (ser-472)*30;
                    else if (ser >= 6281 && ser <= 13973) offset = (ser-6281)*30+162030;
                }
                code = c;
                if (offset>-1) {
                    memcpy(&jfont_dbcs_16[code * 32], fontdata16+offset, 30);
                    jfont_cache_dbcs_16[code] = 1;
                    return &jfont_dbcs_16[code * 32];
                }
            }
        }
		if (isFrameFont(code, 16)) {
			GetDbcsFrameFont(code, jfont_dbcs);
			memcpy(&jfont_dbcs_16[code * 32], jfont_dbcs, 32);
			jfont_cache_dbcs_16[code] = 1;
		} else if (isUserFont(code)) {
			return jfont_dbcs;
		} else if(GetWindowsFont(code, jfont_dbcs, 16, 16)) {
			memcpy(&jfont_dbcs_16[code * 32], jfont_dbcs, 32);
			jfont_cache_dbcs_16[code] = 1;
		} else {
			if (!IS_JDOSV && (dos.loaded_codepage == 936 || dos.loaded_codepage == 949 || dos.loaded_codepage == 950 || dos.loaded_codepage == 951)) {
				Bitu oldcode = code;
				code = GetConvertedCode(code, 932);
				if (!code) {
                    if (!getwqy16) {
                        getwqy16=true;
                        std::string config_path, res_path, exepath=GetDOSBoxXPath(), fname="wqy_12pt.bdf";
                        Cross::GetPlatformConfigDir(config_path), Cross::GetPlatformResDir(res_path);
                        FILE * mfile=fopen(fname.c_str(),"rb");
                        if (!mfile && exepath.size()) mfile=fopen((exepath + fname).c_str(),"rb");
                        if (!mfile && config_path.size()) mfile=fopen((config_path + fname).c_str(),"rb");
                        if (!mfile && res_path.size()) mfile=fopen((res_path + fname).c_str(),"rb");
                        fname="wqy_12pt.pcf";
                        if (!mfile) mfile=fopen(fname.c_str(),"rb");
                        if (!mfile && exepath.size()) mfile=fopen((exepath + fname).c_str(),"rb");
                        if (!mfile && config_path.size()) mfile=fopen((config_path + fname).c_str(),"rb");
                        if (!mfile && res_path.size()) mfile=fopen((res_path + fname).c_str(),"rb");
                        if (!mfile) return jfont_dbcs;
                        if (readBDF(mfile, 16) || readPCF(mfile, 16)) {
                           fclose(mfile);
                           if (jfont_cache_dbcs_16[oldcode] != 0) return &jfont_dbcs_16[oldcode * 32];
                        } else
                           fclose(mfile);
                    }
                    return jfont_dbcs;
                }
			}
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
						jfont_cache_dbcs_16[code] = 1;
						return JPNZN16X+p;
					}
					p+=32;
				}
		}
		return jfont_dbcs;
	}
	return &jfont_dbcs_16[code * 32];
}

uint8_t *GetDbcs14Font(Bitu code, bool &is14)
{
    memset(jfont_dbcs, 0, sizeof(jfont_dbcs));
    if ((IS_JDOSV || dos.loaded_codepage == 932) && del_flag && (code & 0xFF) == 0x7F) code++;
    if(jfont_cache_dbcs_14[code] == 0) {
        if (fontdata14 && fontsize14) {
            if (dos.loaded_codepage == 936 && !(fontsize14%14) && (code/0x100)>0xa0 && (code/0x100)<0xff && (code%0x100)>0xa0 && (code%0x100)<0xfe) {
                int offset = (94 * (unsigned int)((code/0x100) - 0xa0 - 1) + ((code%0x100) - 0xa0 - 1)) * 28;
                if (offset + 28 <= fontsize14) {
                    memcpy(&jfont_dbcs_14[code * 28], fontdata14+offset, 28);
                    jfont_cache_dbcs_14[code] = 1;
                    is14 = true;
                    return &jfont_dbcs_14[code * 28];
                }
            }
            if (((dos.loaded_codepage == 936 && gbk) || dos.loaded_codepage == 950 || dos.loaded_codepage == 951) && !(fontsize14%15) && isKanji1(code/0x100)) {
                Bitu c = code;
                if (dos.loaded_codepage == 936) code = GetConvertedCode(code, 950);
                int offset = -1, ser = (code/0x100 - 161) * 157 + ((code%0x100) - ((code%0x100)>160?161:64)) + ((code%0x100)>160?64:1);
                if (code) {
                    if (ser >= 472 && ser <= 5872) offset = (ser-472)*30;
                    else if (ser >= 6281 && ser <= 13973) offset = (ser-6281)*30+162030;
                }
                code = c;
                if (offset>-1) {
                    memcpy(&jfont_dbcs_14[code * 28], fontdata14+offset, 28);
                    jfont_cache_dbcs_14[code] = 1;
                    is14 = true;
                    return &jfont_dbcs_14[code * 28];
                }
            }
        }
        if (isFrameFont(code, 14)) {
            GetDbcsFrameFont(code, jfont_dbcs);
            memcpy(&jfont_dbcs_14[code * 28], jfont_dbcs, 28);
            jfont_cache_dbcs_14[code] = 1;
            is14 = true;
            return jfont_dbcs;
        } else if (isUserFont(code)) {
            return jfont_dbcs;
        } else if(GetWindowsFont(code, jfont_dbcs, 14, 14)) {
            memcpy(&jfont_dbcs_14[code * 28], jfont_dbcs, 28);
            jfont_cache_dbcs_14[code] = 1;
            is14 = true;
            return jfont_dbcs;
        } else {
            if (!IS_JDOSV && (dos.loaded_codepage == 936 || dos.loaded_codepage == 949 || dos.loaded_codepage == 950 || dos.loaded_codepage == 951)) {
                Bitu oldcode = code;
                code = GetConvertedCode(code, 932);
                if (!code) {
                    if (!getwqy14) {
                        getwqy14=true;
                        std::string config_path, res_path, exepath=GetDOSBoxXPath(), fname="wqy_11pt.bdf";
                        Cross::GetPlatformConfigDir(config_path), Cross::GetPlatformResDir(res_path);
                        FILE * mfile=fopen(fname.c_str(),"rb");
                        if (!mfile && exepath.size()) mfile=fopen((exepath + fname).c_str(),"rb");
                        if (!mfile && config_path.size()) mfile=fopen((config_path + fname).c_str(),"rb");
                        if (!mfile && res_path.size()) mfile=fopen((res_path + fname).c_str(),"rb");
                        fname="wqy_11pt.pcf";
                        if (!mfile) mfile=fopen(fname.c_str(),"rb");
                        if (!mfile && exepath.size()) mfile=fopen((exepath + fname).c_str(),"rb");
                        if (!mfile && config_path.size()) mfile=fopen((config_path + fname).c_str(),"rb");
                        if (!mfile && res_path.size()) mfile=fopen((res_path + fname).c_str(),"rb");
                        if (!mfile) return jfont_dbcs;
                        if (readBDF(mfile, 14) || readPCF(mfile, 14)) {
                           fclose(mfile);
                           if (jfont_cache_dbcs_14[oldcode] != 0) return &jfont_dbcs_14[oldcode * 28];
                        } else
                           fclose(mfile);
                    }
                    is14 = false;
                    return GetDbcsFont(oldcode);
                }
            }
            int p = NAME_LEN+ID_LEN+3;
            uint8_t size = SHMZN14X[p++];
            fontxTbl *table = (fontxTbl *)calloc(size, sizeof(fontxTbl));
            Bitu i=0;
            while (i < size) {
                table[i].start = (SHMZN14X[p] | (SHMZN14X[p+1] << 8));
                table[i].end = (SHMZN14X[p+2] | (SHMZN14X[p+3] << 8));
                i++;
                p+=4;
            }
            for (i = 0; i < size; i++)
                for (uint16_t c = table[i].start; c <= table[i].end; c++) {
                    if (c==code) {
                        memcpy(&jfont_dbcs_14[code * 28], SHMZN14X+p, 28);
                        jfont_cache_dbcs_14[code] = 1;
                        is14 = true;
                        return SHMZN14X+p;
                    }
                    p+=28;
                }
            is14 = false;
            return GetDbcsFont(code);
        }
    } else {
        is14 = true;
        return &jfont_dbcs_14[code * 28];
    }
}

uint8_t *GetDbcs24Font(Bitu code)
{
	memset(jfont_dbcs, 0, sizeof(jfont_dbcs));
	if ((IS_JDOSV || dos.loaded_codepage == 932) && del_flag && (code & 0xFF) == 0x7F) code++;
	if(jfont_cache_dbcs_24[code] == 0) {
        if (fontdata24 && fontsize24) {
            if (dos.loaded_codepage == 936 && !(fontsize24%24) && (code/0x100)>0xa0 && (code/0x100)<0xff && (code%0x100)>0xa0 && (code%0x100)<0xfe) {
                int offset = (94 * (unsigned int)((code/0x100) - 0xa0 - 1) + ((code%0x100) - 0xa0 - 1)) * 72;
                if (offset + 72 <= fontsize24) {
                    memcpy(&jfont_dbcs_24[code * 72], fontdata24+offset, 72);
                    return &jfont_dbcs_24[code * 72];
                }
            } else if ((dos.loaded_codepage == 950 || dos.loaded_codepage == 951) && !(fontsize24%24) && isKanji1(code/0x100)) {
                int offset = -1, ser = (code/0x100 - 161) * 157 + ((code%0x100) - ((code%0x100)>160?161:64)) + ((code%0x100)>160?64:1);
                if (ser >= 472 && ser <= 5872) offset = (ser-472)*72;
                else if (ser >= 6281 && ser <= 13973) offset = (ser-6281)*72+162030;
                if (offset>-1) {
                    memcpy(&jfont_dbcs_24[code * 72], fontdata24+offset, 72);
                    jfont_cache_dbcs_24[code] = 1;
                    return &jfont_dbcs_24[code * 72];
                }
            }
        }
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
		} else if (isFrameFont(code, 24)) {
			GetDbcs24FrameFont(code, jfont_dbcs);
			memcpy(&jfont_dbcs_24[code * 72], jfont_dbcs, 72);
			jfont_cache_dbcs_24[code] = 1;
		} else if (isUserFont(code)) {
			return jfont_dbcs;
		} else if(GetWindowsFont(code, jfont_dbcs, 24, 24)) {
			memcpy(&jfont_dbcs_24[code * 72], jfont_dbcs, 72);
			jfont_cache_dbcs_24[code] = 1;
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
#if defined(LINUX) && C_X11
	int missing_count;
	char **missing_list;
	char *def_string;

	if(!font_display)
		font_display = XOpenDisplay("");
	if(font_display) {
		if(!font_set16) {
			if(IS_TDOSV) {
				xfont_16 = XLoadQueryFont(font_display, "-wenquanyi-*-medium-r-normal-*-16-*-*-*-*-*-iso10646-*"); 
			}
			font_set16 = XCreateFontSet(font_display, "-*-*-medium-r-normal--16-*-*-*", &missing_list, &missing_count, &def_string);
			XFreeStringList(missing_list);
		}
		if(!font_set14) {
			if(IS_TDOSV) {
				font_set14 = XCreateFontSet(font_display, "-wenquanyi-*-medium-r-normal--14-*-*-*", &missing_list, &missing_count, &def_string);
			} else{
				font_set14 = XCreateFontSet(font_display, "-*-*-medium-r-normal--14-*-*-*", &missing_list, &missing_count, &def_string);
			}
			XFreeStringList(missing_list);
		}
		if(!font_set24) {
			font_set24 = XCreateFontSet(font_display, "-*-*-medium-r-normal--24-*-*-*", &missing_list, &missing_count, &def_string);
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
	if(jfont_16 == NULL || jfont_14 == NULL || jfont_24 == NULL) {
		LOGFONT lf = { 0 };
		lf.lfHeight = 16;
		lf.lfCharSet = IS_KDOSV||(!IS_DOSV&&dos.loaded_codepage==949)?HANGUL_CHARSET:(IS_PDOSV||(!IS_DOSV&&dos.loaded_codepage==936)?GB2312_CHARSET:(IS_TDOSV||(!IS_DOSV&&(dos.loaded_codepage==950||dos.loaded_codepage == 951))?CHINESEBIG5_CHARSET:SHIFTJIS_CHARSET));
		lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
		lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
		lf.lfQuality = DEFAULT_QUALITY;
		lf.lfPitchAndFamily = FIXED_PITCH;
		strcpy(lf.lfFaceName, jfont_name);
		jfont_16 = CreateFontIndirect(&lf);
		lf.lfHeight = 14;
		jfont_14 = CreateFontIndirect(&lf);
		lf.lfHeight = use20pixelfont ? 20 : 24;
		jfont_24 = CreateFontIndirect(&lf);
	}
#endif
}

void clearFontCache() {
    getwqy14 = getwqy16 = false;
    memset(jfont_cache_dbcs_16, 0, sizeof(jfont_cache_dbcs_16));
    memset(jfont_cache_dbcs_14, 0, sizeof(jfont_cache_dbcs_14));
    memset(jfont_cache_dbcs_24, 0, sizeof(jfont_cache_dbcs_24));
}

void ShutFontHandle() {
#if defined(LINUX) && C_X11
    font_set16 = font_set14 = font_set24 = NULL;
#endif
#if defined(WIN32)
    jfont_16 = jfont_14 = jfont_24 = NULL;
#endif
    clearFontCache();
}

bool MakeSbcs16Font() {
	InitFontHandle();
	bool fail=false;
	for(Bitu code = 0 ; code < 256 ; code++) {
		if(!GetWindowsFont(code, &jfont_sbcs_16[code * 16], 8, 16)) {
			fail=true;
			break;
		}
	}
	if (fail) memcpy(jfont_sbcs_16, JPNHN16X+NAME_LEN+ID_LEN+3, SBCS16_LEN);
	if (IS_JDOSV||IS_JEGA_ARCH) memcpy(jfont_sbcs_16, dosv_font16_data, sizeof(dosv_font16_data));
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
	else if (IS_JEGA_ARCH) memcpy(jfont_sbcs_19, ax_font19_data, sizeof(ax_font19_data));
	else if (IS_DOSV) for(Bitu ct = 0 ; ct < 0x100 ; ct++) memcpy(&jfont_sbcs_19[ct * 19], &int10_font_19[ct * 19], 19);
	return true;
}

bool MakeSbcs24Font() {
	InitFontHandle();
	bool fail=false;
	for(Bitu code = 0 ; code < 256 ; code++) {
		if(!GetWindowsFont(code, &jfont_sbcs_24[code * 24 * 2], 12, 24)) {
			fail=true;
			break;
		}
	}
	if (fail) memcpy(jfont_sbcs_24, JPNHN24X+NAME_LEN+ID_LEN+3, SBCS24_LEN);
	if (IS_JDOSV||IS_JEGA_ARCH) memcpy(jfont_sbcs_24, dosv_font24_data, sizeof(dosv_font24_data));
	else if (IS_DOSV) for(Bitu ct = 0 ; ct < 0x100 ; ct++) memcpy(&jfont_sbcs_24[ct * 24 * 2], &int10_font_24[ct * 24 * 2], 48);
	return true;
}

#if defined(WIN32) && !defined(HX_DOS) && defined(C_SDL2)
#include "imm.h"
extern HWND GetHWND(void);
bool IME_GetEnable()
{
    bool state = false;
    HWND wnd = GetHWND();
    HIMC imc = ImmGetContext(wnd);
    if(ImmGetOpenStatus(imc)) {
         state = true;
    }
    ImmReleaseContext(wnd, imc);
    return state;
}

void IME_SetEnable(BOOL state)
{
    HWND wnd = GetHWND();
    HIMC imc = ImmGetContext(wnd);
    ImmSetOpenStatus(imc, state);
    ImmReleaseContext(wnd, imc);
}

static wchar_t CompositionFontName[LF_FACESIZE];

void IME_SetCompositionFontName(const char *name)
{
	int len = MultiByteToWideChar(CP_ACP, 0, name, -1, NULL, 0);
	if(len < LF_FACESIZE) {
		MultiByteToWideChar(CP_ACP, 0, name, -1, CompositionFontName, len);
	}
}

void IME_SetFontSize(int size)
{
    HWND wnd = GetHWND();
    HIMC imc = ImmGetContext(wnd);
    HDC hc = GetDC(wnd);
    HFONT hf = (HFONT)GetCurrentObject(hc, OBJ_FONT);
    LOGFONTW lf;
    GetObjectW(hf, sizeof(lf), &lf);
    ReleaseDC(wnd, hc);
    if(CompositionFontName[0]) {
        wcscpy(lf.lfFaceName, CompositionFontName);
    }
    lf.lfHeight = -size;
    lf.lfWidth = size / 2;
    ImmSetCompositionFontW(imc, &lf);
    ImmReleaseContext(wnd, imc);
}

#endif

void JFONT_Init() {
#if defined(LINUX) && C_X11
	setlocale(LC_CTYPE,"");
#endif
    bool reinit = jfont_init;
	jfont_init = true;
    if (fontdata14) {
        free(fontdata14);
        fontdata14 = NULL;
        fontsize14 = 0;
    }
    if (fontdata16) {
        free(fontdata16);
        fontdata16 = NULL;
        fontsize16 = 0;
    }
    if (fontdata24) {
        free(fontdata24);
        fontdata24 = NULL;
        fontsize24 = 0;
    }
#if (defined(WIN32) && !defined(HX_DOS) || defined(LINUX) && C_X11) && !defined(C_SDL2) && defined(SDL_DOSBOX_X_SPECIAL)
	SDL_SetCompositionFontName(jfont_name);
#elif defined(WIN32) && !defined(HX_DOS) && defined(C_SDL2)
	IME_SetCompositionFontName(jfont_name);
#endif
    Section_prop *section = static_cast<Section_prop *>(control->GetSection("dosv"));
	getsysfont = section->Get_bool("getsysfont");
	yen_flag = section->Get_bool("yen");
#if defined(WIN32)
    use20pixelfont = section->Get_bool("use20pixelfont");
#endif

	Prop_path* pathprop = section->Get_path("fontxsbcs");
	if (pathprop && !reinit) {
		std::string path=pathprop->realpath;
		ResolvePath(path);
		if(!LoadFontxFile(path.c_str(), 19, false)) {
			if(!MakeSbcs19Font()) {
				LOG_MSG("MSG: SBCS 8x19 font file path is not specified.\n");
#if defined(LINUX)
				for(Bitu ct = 0 ; ct < 0x100 ; ct++)
					memcpy(&jfont_sbcs_19[ct * 19 + 1], &int10_font_16[ct * 16], 16);
#endif
			}
		} else if(yen_flag && !(IS_DOSV && !IS_JDOSV)) {
			if(!CheckEmptyData(&jfont_sbcs_19[0x7f * 19], 19))
				memcpy(&jfont_sbcs_19[0x5c * 19], &jfont_sbcs_19[0x7f * 19], 19);
		}
	} else if (!reinit) {
		if(!MakeSbcs19Font())
			LOG_MSG("MSG: SBCS 8x19 font file path is not specified.\n");
	}
	pathprop = section->Get_path("fontxsbcs16");
	if(pathprop && !reinit) {
		std::string path=pathprop->realpath;
		ResolvePath(path);
		if(!LoadFontxFile(path.c_str(), 16, false)) {
			if(!MakeSbcs16Font()) {
				LOG_MSG("MSG: SBCS 8x16 font file path is not specified.\n");
#if defined(LINUX)
				memcpy(jfont_sbcs_16, int10_font_16, 256 * 16);
#endif
			}
		} else if(yen_flag && !(IS_DOSV && !IS_JDOSV)) {
			if(!CheckEmptyData(&jfont_sbcs_16[0x7f * 16], 16))
				memcpy(&jfont_sbcs_16[0x5c * 16], &jfont_sbcs_16[0x7f * 16], 16);
		}
	} else if (!reinit) {
		if(!MakeSbcs16Font()) {
			LOG_MSG("MSG: SBCS 8x16 font file path is not specified.\n");
		}
	}
	pathprop = section->Get_path("fontxdbcs");
	if(pathprop) {
		std::string path=pathprop->realpath;
		ResolvePath(path);
		LoadFontxFile(path.c_str(), 16, true);
	}
	pathprop = section->Get_path("fontxdbcs14");
	if(pathprop) {
		std::string path=pathprop->realpath;
		ResolvePath(path);
		LoadFontxFile(path.c_str(), 14, true);
	}
	if(IS_DOSV) {
#if defined(USE_TTF)
		autoboxdraw = true;
#endif
		pathprop = section->Get_path("fontxdbcs24");
		if(pathprop) {
			std::string path=pathprop->realpath;
			ResolvePath(path);
			LoadFontxFile(path.c_str(), 24, true);
		}
		pathprop = section->Get_path("fontxsbcs24");
		if(pathprop && !reinit) {
			std::string path=pathprop->realpath;
			ResolvePath(path);
			if(!LoadFontxFile(path.c_str(), 24, false)) {
				if(!MakeSbcs24Font()) {
					LOG_MSG("MSG: SBCS 12x24 font file path is not specified.\n");
				}
			} else if(yen_flag && !(IS_DOSV && !IS_JDOSV)) {
				if(!CheckEmptyData(&jfont_sbcs_24[0x7f * 2 * 24], 2 * 24))
					memcpy(&jfont_sbcs_24[0x5c * 2 * 24], &jfont_sbcs_24[0x7f * 2 * 24], 2 * 24);
			}
		} else if (!reinit) {
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
	if(IS_DOS_JAPANESE && (TrueVideoMode == 0x03 || TrueVideoMode == 0x12 || TrueVideoMode == 0x70 || TrueVideoMode == 0x72 || TrueVideoMode == 0x78)) {
		return true;
	}
	return false;
}

bool DOSV_CheckCJKVideoMode()
{
	if(IS_DOS_CJK && (TrueVideoMode == 0x03 || TrueVideoMode == 0x12 || TrueVideoMode == 0x70 || TrueVideoMode == 0x72 || TrueVideoMode == 0x78)) {
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
#if (defined(WIN32) && !defined(HX_DOS) || defined(LINUX) && C_X11) && !defined(C_SDL2) && defined(SDL_DOSBOX_X_SPECIAL)
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
#elif defined(WIN32) && !defined(HX_DOS) && defined(C_SDL2)
		if(mode & 0x8000) {
			if(mode & 0x0001)
				IME_SetEnable(FALSE);
			else if(mode & 0x0002)
				IME_SetEnable(TRUE);
		} else {
			if(IME_GetEnable() == NULL)
				real_writew(param_seg, param_off + 2, 0x000a);
			else
				real_writew(param_seg, param_off + 2, 0x0009);
		}
#endif
		reg_ax = 0;
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

uint8_t StartBankSelect(Bitu &off);
uint8_t CheckBankSelect(uint8_t select, Bitu &off);

void DOSV_CursorXor24(Bitu x, Bitu y, Bitu start, Bitu end)
{
	IO_Write(0x3ce, 0x05); IO_Write(0x3cf, 0x03);
	IO_Write(0x3ce, 0x00); IO_Write(0x3cf, 0x0f);
	IO_Write(0x3ce, 0x03); IO_Write(0x3cf, 0x18);

	Bitu width = (real_readw(BIOSMEM_SEG, BIOSMEM_NB_COLS) == 85) ? 128 : 160;
	volatile uint8_t dummy;
	Bitu off = (y + start) * width + (x * 12) / 8;
	uint8_t select = StartBankSelect(off);
	while(start <= end) {
		if(dosv_cursor_stat == 2) {
			uint8_t cursor_data[2][4] = {{ 0xff, 0xff, 0xff, 0x00 }, { 0x0f, 0xff, 0xff, 0xf0 }};
			for(uint8_t i = 0 ; i < 4 ; i++) {
				dummy = real_readb(0xa000, off);
				real_writeb(0xa000, off, cursor_data[x & 1][i]);
				off++;
				select = CheckBankSelect(select, off);
			}
			off += width - 4;
		} else {
			uint8_t cursor_data[2][2] = { { 0xff, 0xf0 }, { 0x0f, 0xff }};
			for(uint8_t i = 0 ; i < 2 ; i++) {
				dummy = real_readb(0xa000, off);
				real_writeb(0xa000, off, cursor_data[x & 1][i]);
				off++;
				select = CheckBankSelect(select, off);
			}
			off += width - 2;
		}
		select = CheckBankSelect(select, off);
		start++;
	}
	IO_Write(0x3ce, 0x03); IO_Write(0x3cf, 0x00);
	if(svgaCard == SVGA_TsengET4K) {
		IO_Write(0x3cd, 0);
	} else if(svgaCard == SVGA_S3Trio) {
		IO_Write(0x3d4, 0x6a); IO_Write(0x3d5, 0);
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
		uint8_t select = StartBankSelect(off);
		while(start <= end) {
			dummy = real_readb(0xa000, off);
			real_writeb(0xa000, off, 0xff);
			if(dosv_cursor_stat == 2) {
				off++;
				select = CheckBankSelect(select, off);
				dummy = real_readb(0xa000, off);
				real_writeb(0xa000, off, 0xff);
				off += width - 1;
			} else {
				off += width;
			}
			select = CheckBankSelect(select, off);
			start++;
		}
		IO_Write(0x3ce, 0x03); IO_Write(0x3cf, 0x00);
		if(svgaCard == SVGA_TsengET4K) {
			IO_Write(0x3cd, 0);
		} else if(svgaCard == SVGA_S3Trio) {
			IO_Write(0x3d4, 0x6a); IO_Write(0x3d5, 0);
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
#if (defined(WIN32) && !defined(HX_DOS) || defined(LINUX) && C_X11 || defined(MACOSX) && defined(SDL_DOSBOX_X_IME)) && (defined(C_SDL2) || defined(SDL_DOSBOX_X_SPECIAL))
	SetIMPosition();
#endif
	if(!CheckAnotherDisplayDriver() && real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_MODE) != 0x72 && real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_MODE) != 0x12) {
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
	if(svgaCard == SVGA_TsengET4K || svgaCard == SVGA_S3Trio) {
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
	if(vtext == "svga" && svgaCard != SVGA_None) {
		return DOSV_VTEXT_SVGA;
	}
	return DOSV_VTEXT_VGA;
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
	dosv_vtext_mode[0] = DOSV_StringVtextMode(section->Get_string("vtext1"));
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

#define KANJI_ROM_PAGE		(0xE0000/4096)

static uint16_t j3_timer;
static uint8_t j3_cursor_stat;
static uint16_t j3_cursor_x;
static uint16_t j3_cursor_y;
static Bitu j3_text_color;
static Bitu j3_back_color;
static uint16_t j3_machine_code = 0;
uint16_t j3_font_offset = 0xca00;

static uint16_t jis2shift(uint16_t jis)
{
	uint16_t high, low;
	high = jis >> 8;
	low = jis & 0xff;
	if(high & 0x01) {
		low += 0x1f;
	} else {
		low += 0x7d;
	}
	if(low >= 0x7f) {
		low++;
	}
	high = ((high - 0x21) >> 1) + 0x81;
	if(high >= 0xa0) {
		high += 0x40;
	}
	return (high << 8) + low;
}

static uint16_t shift2jis(uint16_t sjis)
{
	uint16_t high, low;
	high = sjis >> 8;
	low = sjis & 0xff;
	if(high > 0x9f) {
		high -= 0xb1;
	} else {
		high -= 0x71;
	}
	high <<= 1;
	high++;
	if(low > 0x7f) {
		low--;
	}
	if(low >= 0x9e) {
		low -= 0x7d;
		high++;
	} else {
		low -= 0x1f;
	}
	return (high << 8) + low;
}

Bitu INT60_Handler(void)
{
	switch (reg_ah) {
	case 0x01:
		reg_dx = jis2shift(reg_dx);
		break;
	case 0x02:
		reg_dx = shift2jis(reg_dx);
		break;
	case 0x03:
		{
			uint16_t code = (reg_al & 0x01) ? jis2shift(reg_dx) : reg_dx;
			SegSet16(es, 0xe000);
			if(reg_al & 0x02) {
				uint8_t *src = GetDbcs24Font(code);
				uint8_t *dest = jfont_kanji;
				for(Bitu y = 0 ; y < 24 ; y++) {
					*dest++ = *src++;
					*dest++ = *src++;
					*dest++ = *src++;
					dest++;
				}
				reg_si = 0x0000;
			} else {
				// yen
				if(code == 0x80da) {
					reg_si = 0x0780;
				} else if(code >= 0x8540 && code <= 0x857E) {
					reg_si = 0x6c20 + (code - 0x8540) * 32;
				} else {
					uint16_t *src = (uint16_t *)GetDbcsFont(code);
					uint16_t *dest = (uint16_t *)jfont_kanji;
					for(Bitu y = 0 ; y < 16 ; y++) {
						*dest++ = *src++;
					}
					reg_si = 0x0000;
				}
			}
			reg_al = 0;
		}
		break;
	case 0x05:
		break;
	case 0x0c:
		if(reg_al == 0xff) {
			reg_al = 25 - real_readb(BIOSMEM_J3_SEG, BIOSMEM_J3_LINE_COUNT);
		} else {
			uint8_t line = 25 - reg_al;
			real_writeb(BIOSMEM_J3_SEG, BIOSMEM_J3_LINE_COUNT, line);
			line--;
			real_writeb(BIOSMEM_SEG, BIOSMEM_NB_ROWS, line);
		}
		break;
	case 0x0e:
		SegSet16(es, GetTextSeg());
		reg_bx = 0;
		break;
	case 0x0f:
		if(reg_al == 0x00) {
			reg_ax = 0;
		} else if(reg_al == 0x01) {
			DOS_ClearKeyMap();
		}
		break;
	case 0x10:
		if(reg_al == 0x00) {
			SegSet16(es, 0xf000);
			reg_bx = j3_font_offset;
		}
		break;
	default:
		LOG(LOG_BIOS,LOG_ERROR)("INT60:Unknown call %4X",reg_ax);
	}
	return CBRET_NONE;
}

Bitu INT6F_Handler(void)
{
	switch(reg_ah) {
	case 0x01:
	case 0x02:
	case 0x03:
	case 0x04:
	case 0x05:
#if (defined(WIN32) && !defined(HX_DOS) || defined(LINUX) && C_X11) && !defined(C_SDL2) && defined(SDL_DOSBOX_X_SPECIAL)
		SDL_SetIMValues(SDL_IM_ONOFF, 1, NULL);
#elif defined(WIN32) && !defined(HX_DOS) && defined(C_SDL2)
		IME_SetEnable(TRUE);
#endif
		break;
	case 0x0b:
#if (defined(WIN32) && !defined(HX_DOS) || defined(LINUX) && C_X11) && !defined(C_SDL2) && defined(SDL_DOSBOX_X_SPECIAL)
		SDL_SetIMValues(SDL_IM_ONOFF, 0, NULL);
#elif defined(WIN32) && !defined(HX_DOS) && defined(C_SDL2)
		IME_SetEnable(FALSE);
#endif
		break;
	case 0x66:
		{
			reg_al = 0x00;
#if (defined(WIN32) && !defined(HX_DOS) || defined(LINUX) && C_X11) && !defined(C_SDL2) && defined(SDL_DOSBOX_X_SPECIAL)
			int onoff;
			if(SDL_GetIMValues(SDL_IM_ONOFF, &onoff, NULL) == NULL) {
				if(onoff) {
					reg_al = 0x01;
				}
			}
#elif defined(WIN32) && !defined(HX_DOS) && defined(C_SDL2)
			if(IME_GetEnable()) {
				reg_al = 0x01;
			}
#endif
		}
		break;
	}
	return CBRET_NONE;
}

class KanjiRomPageHandler : public PageHandler {
private:
	uint8_t bank;
	Bitu GetKanji16(Bitu addr) {
		Bitu total = bank * 0x10000 + addr;
		uint16_t code, offset;
		if(total < 0x10420) {
			total -= 0x10000;
			offset = 0x3a60;
		} else if(total < 0x20000) {
			total -= 0x10420;
			offset = 0x3b21;
		} else if(total < 0x30000) {
			total -= 0x20000;
			offset = 0x5020;
		} else if(total < 0x30820) {
			total -= 0x30000;
			offset = 0x6540;
		} else if(total < 0x40000) {
			total -= 0x30820;
			offset = 0x6621;
		} else if(total < 0x40420) {
			total -= 0x40000;
			offset = 0x7a60;
		} else if(total < 0x41be0) {
			total -= 0x40420;
			offset = 0x7b21;
		} else {
			return 0;
		}
		code = offset + (total / 0xc00) * 0x100 + ((total % 0xc00) / 32);
		code = jis2shift(code);
		GetDbcsFont(code);
		return code * 32 + (total % 32);
	}
	Bitu GetKanji24(Bitu addr) {
		Bitu total = bank * 0x10000 + addr;
		Bitu rest;
		uint16_t code, offset;
		if(total >= 0x40060 && total < 0x42460) {
			total -= 0x40060;
			offset = 0x2021;
		} else if(total < 0x54460) {
			total -= 0x42460;
			offset = 0x2121;
		} else if(total < 0x55c00) {
			total -= 0x54460;
			offset = 0x2921;
		} else if(total < 0x100000) {
			total -= 0x58060;
			offset = 0x3021;
		} else {
			return 0;
		}
		code = offset + (total / 0x2400) * 0x100 + (total % 0x2400) / 96;
		code = jis2shift(code);
		GetDbcs24Font(code);
		rest = total % 4;
		if(rest != 3) {
			return code * 72 + ((total % 96) / 4) * 3 + rest;
		}
		return 0;
	}
public:
	KanjiRomPageHandler() {
		flags = PFLAG_HASROM;
		bank = 0;
	}
	uint8_t readb(PhysPt addr) {
		uint16_t code;
		Bitu offset;
		if(bank == 0) {
			if(addr >= 0xe0000 && addr < 0xe0060) {
				return jfont_kanji[addr - 0xe0000];
			} else if(addr >= 0xe0780 && addr < 0xe07a0) {
				return jfont_yen[addr - 0xe0780];
			} else if(addr >= 0xe0c20 && addr < 0xe6be0) {
				offset = addr - 0xe0c20;
				code = 0x2121 + (offset / 0xc00) * 0x100 + ((offset % 0xc00) / 32);
				code = jis2shift(code);
				GetDbcsFont(code);
				return jfont_dbcs_16[code * 32 + (addr % 32)];
			} else if(addr >= 0xe6c20 && addr < 0xe7400) {
				return jfont_kana[addr - 0xe6c20];
			} else {
				offset = addr - 0xe8020;
				code = 0x3021 + (offset / 0xc00) * 0x100 + ((offset % 0xc00) / 32);
				code = jis2shift(code);
				GetDbcsFont(code);
				return jfont_dbcs_16[code * 32 + (addr % 32)];
			}
		} else if((bank >= 1 && bank <= 3) || (bank == 4 && addr < 0xe0060)) {
			return jfont_dbcs_16[GetKanji16(addr - 0xe0000)];
		} else {
			return jfont_dbcs_24[GetKanji24(addr - 0xe0000)];
		}
		return 0;
	}
	uint16_t readw(PhysPt addr) {
		uint16_t code;
		Bitu offset;
		if(bank == 0) {
			if(addr >= 0xe0000 && addr < 0xe0060) {
				return *(uint16_t *)&jfont_kanji[addr - 0xe0000];
			} else if(addr >= 0xe0780 && addr < 0xe07a0) {
				return *(uint16_t *)&jfont_yen[addr - 0xe0780];
			} else if(addr >= 0xe0c20 && addr < 0xe6be0) {
				offset = addr - 0xe0c20;
				code = 0x2121 + (offset / 0xc00) * 0x100 + ((offset % 0xc00) / 32);
				code = jis2shift(code);
				GetDbcsFont(code);
				return *(uint16_t *)&jfont_dbcs_16[code * 32 + (addr % 32)];
			} else if(addr >= 0xe6c20 && addr < 0xe7400) {
				return *(uint16_t *)&jfont_kana[addr - 0xe6c20];
			} else {
				offset = addr - 0xe8020;
				code = 0x3021 + (offset / 0xc00) * 0x100 + ((offset % 0xc00) / 32);
				code = jis2shift(code);
				GetDbcsFont(code);
				return *(uint16_t *)&jfont_dbcs_16[code * 32 + (addr % 32)];
			}
		} else if((bank >= 1 && bank <= 3) || (bank == 4 && addr < 0xe1be0)) {
			return *(uint16_t *)&jfont_dbcs_16[GetKanji16(addr - 0xe0000)];
		} else {
			return *(uint16_t *)&jfont_dbcs_24[GetKanji24(addr - 0xe0000)];
		}
		return 0;
	}
	uint32_t readd(PhysPt addr) {
		return 0;
	}
	void writeb(PhysPt addr,uint8_t val){
		if((val & 0x80) && val != 0xff) {
			bank = val & 0x7f;
		}
	}
	void writew(PhysPt addr,uint16_t val){
	}
	void writed(PhysPt addr,uint32_t val){
	}
};
KanjiRomPageHandler kanji_rom_handler;

uint16_t GetGaijiSeg()
{
	return gaiji_seg;
}

void INT60_J3_Setup()
{
	uint16_t code;

	gaiji_seg = DOS_GetMemory(GAIJI_COUNT * 2, "J-3100 Gaiji area");

	PhysPt fontdata = Real2Phys(int10.rom.font_16);
	for(code = 0 ; code < 256 ; code++) {
		for(int y = 0 ; y < 16 ; y++) {
			if(code >= 0x20 && code < 0x80) {
				phys_writeb(0xf0000 + j3_font_offset + code * 16 + y, jfont_sbcs_16[code * 16 + y]);
				if(code == 0x5c) {
					jfont_yen[y * 2] = jfont_sbcs_16[code * 16 + y];
					jfont_yen[y * 2 + 1] = jfont_sbcs_16[code * 16 + y];
				}
			} else {
				phys_writeb(0xf0000 + j3_font_offset + code * 16 + y, mem_readb(fontdata + code * 16 + y));
				if(code >= 0xa1 && code <= 0xdf) {
					jfont_kana[(code - 0xa1) * 32 + y * 2] = jfont_sbcs_16[code * 16 + y];
					jfont_kana[(code - 0xa1) * 32 + y * 2 + 1] = jfont_sbcs_16[code * 16 + y];
				}
			}
		}
	}
	MEM_SetPageHandler(KANJI_ROM_PAGE, 16, &kanji_rom_handler);
}

static void J3_CursorXor(Bitu x, Bitu y)
{
	uint16_t end = real_readb(BIOSMEM_SEG, BIOSMEM_CURSOR_TYPE);
	uint16_t start = real_readb(BIOSMEM_SEG, BIOSMEM_CURSOR_TYPE + 1);
	uint16_t off;

	if(start != 0x20 && start <= end) {
		y += start;
		off = (y >> 2) * 80 + 8 * 1024 * (y & 3) + x;
		while(start <= end) {
			real_writeb(0xb800, off, real_readb(0xb800, off) ^ 0xff);
			if(j3_cursor_stat == 2) {
				real_writeb(0xb800, off + 1, real_readb(0xb800, off + 1) ^ 0xff);
			}
			off += 0x2000;
			if(off >= 0x8000) {
				off -= 0x8000;
				off += 80;
			}
			start++;
		}
	}
}

void J3_OffCursor()
{
	if(j3_cursor_stat) {
		J3_CursorXor(j3_cursor_x, j3_cursor_y);
		j3_cursor_stat = 0;
	}
}

void INT8_J3()
{
#if (defined(WIN32) && !defined(HX_DOS) || defined(LINUX) && C_X11 || defined(MACOSX) && defined(SDL_DOSBOX_X_IME)) && (defined(C_SDL2) || defined(SDL_DOSBOX_X_SPECIAL))
	SetIMPosition();
#endif

	j3_timer++;
	if((j3_timer & 0x03) == 0) {
		if((real_readb(BIOSMEM_J3_SEG, BIOSMEM_J3_BLINK) & 0x01) == 0 || j3_cursor_stat == 0) {
			uint16_t x = real_readb(BIOSMEM_SEG, BIOSMEM_CURSOR_POS);
			uint16_t y = real_readb(BIOSMEM_SEG, BIOSMEM_CURSOR_POS + 1) * 16;
			if(j3_cursor_stat == 0) {
				uint8_t attr = GetKanjiAttr();
				if(attr == 0) {
					j3_cursor_stat = 1;
				} else {
					j3_cursor_stat = 1;
					if(attr == 1) {
						j3_cursor_stat = 2;
					}
				}
				j3_cursor_x = x;
				j3_cursor_y = y;
				J3_CursorXor(x, y);
			} else {
				J3_CursorXor(x, y);
				j3_cursor_stat = 0;
			}
		}
	}
}

enum J3_COLOR {
	colorLcdBlue,
	colorLcdWhite,
	colorPlasma,
	colorNormal,
	colorMax
};

static Bitu text_color_list[colorMax] = {
	0x3963f7, 0x3a4b51, 0xff321b, 0xffffff
};

static Bitu back_color_list[colorMax] = {
	0xbed7d4, 0xeaf3f2, 0x6a1d22, 0x000000
};

static struct J3_MACHINE_LIST {
	char *name;
	uint16_t code;
	enum J3_COLOR color;
} j3_machine_list[] = {
	{ "gt", 0x3130, colorPlasma },
	{ "sgt", 0xfc27, colorPlasma },
	{ "gx", 0xfc2d, colorPlasma },
	{ "gl", 0xfc2b, colorLcdBlue },
	{ "sl", 0xfc2c, colorLcdBlue },
	{ "sgx", 0xfc26, colorPlasma },
	{ "ss", 0x3131, colorLcdBlue },
	{ "gs", 0xfc2a, colorLcdBlue },
	{ "sx", 0xfc36, colorLcdBlue },
	{ "sxb", 0xfc36, colorLcdBlue },
	{ "sxw", 0xfc36, colorLcdWhite },
	{ "sxp", 0xfc36, colorPlasma },
	{ "ez", 0xfc87, colorLcdWhite },
	{ "zs", 0xfc25, colorNormal },
	{ "zx", 0xfc4e, colorNormal },
	{ NULL, 0, colorMax }
};

uint16_t J3_GetMachineCode() {
	return j3_machine_code;
}

void J3_SetType(std::string type, std::string back, std::string text) {
	if(type != "default") {
		j3_machine_code = ConvHexWord((char *)type.c_str());
	}
	if(j3_machine_code == 0) j3_machine_code = 0x6a74;
	enum J3_COLOR j3_color = colorNormal;
	for(Bitu count = 0 ; j3_machine_list[count].name != NULL ; count++) {
		if(type == j3_machine_list[count].name) {
			j3_machine_code = j3_machine_list[count].code;
			j3_color = j3_machine_list[count].color;
			break;
		}
	}
	if(back.empty()) {
		j3_back_color = back_color_list[j3_color];
	} else {
		j3_back_color = ConvHexWord((char *)back.c_str());
	}
	if(text.empty()) {
		j3_text_color = text_color_list[j3_color];
	} else {
		j3_text_color = ConvHexWord((char *)text.c_str());
	}
}

void J3_GetPalette(uint8_t no, uint8_t &r, uint8_t &g, uint8_t &b)
{
	if(no == 0) {
		r = (j3_back_color >> 18) & 0x3f;
		g = (j3_back_color >> 10) & 0x3f;
		b = (j3_back_color >> 2) & 0x3f;
	} else {
		r = (j3_text_color >> 18) & 0x3f;
		g = (j3_text_color >> 10) & 0x3f;
		b = (j3_text_color >> 2) & 0x3f;
	}
}

bool J3_IsJapanese()
{
	if(IS_J3100 && real_readb(BIOSMEM_SEG, BIOSMEM_CURRENT_MODE) == 0x74) {
		return true;
	}
	return false;
}

void J3_SetBiosArea(uint16_t mode)
{
	if(mode == 0x74) {
		if(bootdrive < 0) {
			real_writeb(BIOSMEM_J3_SEG, BIOSMEM_J3_MODE, 0x01);
			if(real_readb(BIOSMEM_J3_SEG, BIOSMEM_J3_LINE_COUNT) == 0) {
				real_writeb(BIOSMEM_J3_SEG, BIOSMEM_J3_LINE_COUNT, 25);		// line count
			}
			real_writew(BIOSMEM_J3_SEG, BIOSMEM_J3_CODE_SEG, GetTextSeg());
			real_writew(BIOSMEM_J3_SEG, BIOSMEM_J3_CODE_OFFSET, 0);
			real_writew(BIOSMEM_J3_SEG, BIOSMEM_J3_BLINK, 0x00);
			real_writew(BIOSMEM_SEG, BIOSMEM_CURSOR_TYPE, 0x0f0f);
		}
		real_writeb(BIOSMEM_J3_SEG, BIOSMEM_J3_SCROLL, 0x01);		// soft scroll
	} else {
		real_writeb(BIOSMEM_J3_SEG, BIOSMEM_J3_MODE, 0x00);
	}
}

bool J3_IsCga4Dcga()
{
	return IS_J3100 && (TrueVideoMode == 0x04 || TrueVideoMode == 0x05);
}
