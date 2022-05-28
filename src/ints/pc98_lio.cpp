// LIO drawing is implementation based on Neko Project II
//
#include "dosbox.h"
#include "mem.h"
#include "cpu.h"
#include "inout.h"
#include "callback.h"
#include "logging.h"
#include "pc98_gdc.h"
#include "pc98_gdc_const.h"
#include <string.h>
#include <queue>

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

enum {
    LIO_SUCCESS     = 0,
    LIO_ILLEGALFUNC = 5,
    LIO_OUTOFMEMORY = 7
};

enum {
    LIODRAW_PMASK   = 0x03,
    LIODRAW_MONO    = 0x04,
    LIODRAW_UPPER   = 0x20,
    LIODRAW_4BPP    = 0x40
};

struct LIOWORK {
    uint8_t screen_mode;
    uint8_t pos;
    uint8_t plane;
    uint8_t fore_color;
    uint8_t back_color;
    uint8_t padding;
    uint8_t color[8];
    uint16_t view_x1;
    uint16_t view_y1;
    uint16_t view_x2;
    uint16_t view_y2;
    uint8_t disp;
    uint8_t access;
};

struct LIODRAW {
    short x1;
    short y1;
    short x2;
    short y2;
    uint8_t flag;
    uint8_t palette_max;
    uint8_t planes;
    uint8_t *paint_work;
};

struct LIOPUT {
    short x;
    short y;
    uint16_t width;
    uint16_t height;
    uint16_t off;
    uint16_t seg;
    uint8_t mode;
    uint8_t sw;
    uint8_t fore_color;
    uint8_t back_color;
};

struct PUTCNTX {
    PhysPt base;
    uint16_t addr;
    uint16_t shift;
    uint16_t width;
    uint8_t maskl;
    uint8_t maskr;
    uint8_t masklr;
    uint8_t mask;
    uint8_t buffer[84];
};

struct GETCNTX {
    PhysPt base;
    uint16_t addr;
    uint16_t shift;
    uint16_t width;
    uint8_t mask;
};

struct PAINT_POINT {
    short x;
    short y;
};

struct PAINT_LINE {
    short lx;
    short rx;
    short y;
};

static LIOWORK lio_work;
static LIODRAW lio_draw;
static uint8_t lio_palette_mode;
static std::queue<PAINT_POINT> paint_point;
static std::vector<PAINT_LINE> paint_line;
static std::vector<uint8_t> paint_tile;

void lio_read_parameter() {
    uint16_t seg = SegValue(ds);
    MEM_BlockRead(PhysMake(seg, 0x0620), &lio_work, sizeof(lio_work));
    lio_palette_mode = real_readb(seg, 0xa08);
}

void lio_write_parameter() {
    uint16_t seg = SegValue(ds);
    MEM_BlockWrite(PhysMake(seg, 0x0620), &lio_work, sizeof(lio_work));
    real_writeb(seg, 0xa08, lio_palette_mode);
}

static void lio_updatedraw() {
    int16_t lines;

    lio_draw.flag = 0;
    lio_draw.planes = 3;
    lines = 399;
    if(lio_palette_mode == 2) {
        // 16-colors
        lio_draw.flag |= LIODRAW_4BPP;
        lio_draw.planes = 4;
    }
    switch(lio_work.screen_mode) {
        case 0:
            // color 640x200
            if(lio_work.pos & 1) {
                lio_draw.flag |= LIODRAW_UPPER;
            }
            lines = 199;
            break;
        case 1:
            // mono 640x200
            if(lio_work.pos >= lio_draw.planes) {
                lio_draw.flag |= LIODRAW_UPPER;
            }
            lines = 199;
            // through
        case 2:
            // mono 640x400
            lio_draw.flag |= (lio_work.pos % lio_draw.planes);
            break;
        // case 3: color 640x400
    }
    lio_draw.palette_max = 1 << lio_draw.planes;

    lio_draw.x1 = MAX(lio_work.view_x1, 0);
    lio_draw.y1 = MAX(lio_work.view_y1, 0);
    lio_draw.x2 = MIN(lio_work.view_x2, 639);
    lio_draw.y2 = MIN(lio_work.view_y2, lines);
}

static const PhysPt lio_base[4] = { 0xa8000, 0xb0000, 0xb8000, 0xe0000 };
static const uint16_t gdc_base[4] = { 0x4000, 0x8000, 0xc000, 0x0000 };

static void lio_pset(short x, short y, uint8_t palette) {
    PhysPt addr;
    uint8_t bit;
    uint8_t data;

    if(lio_draw.x1 > x || lio_draw.x2 < x || lio_draw.y1 > y || lio_draw.y2 < y) {
        return;
    }
    addr = (y * 80) + (x >> 3);
    bit = 0x80 >> (x & 7);
    if(lio_draw.flag & LIODRAW_UPPER) {
        addr += 16000;
    }
    for(uint8_t i = 0 ; i < lio_draw.planes ; i++) {
        data = mem_readb(lio_base[i] + addr);
        if(palette & (1 << i)) {
            mem_writeb(lio_base[i] + addr, data | bit);
        } else {
            mem_writeb(lio_base[i] + addr, data & ~bit);
        }
    }
}

/* The LIO BIOS must update the BIOS data area regarding draw mode.
 * The GDC maintains the draw mode separately. At no time does the
 * GDC rewrite system memory itself. */
static void lio_bda_and_gdc_set_mode(const uint8_t draw_mode) {
    mem_writeb(0x54D, (mem_readb(0x54D) & 0xfc) | draw_mode);
    pc98_gdc[GDC_SLAVE].set_mode(draw_mode);
}

static void lio_gline_sub(int x1, int y1, int x2, int y2, uint16_t ead, uint8_t dad, int palette) {
    for(uint8_t i = 0 ; i < lio_draw.planes ; i++) {
        pc98_gdc[GDC_SLAVE].set_vectl(x1, y1, x2, y2);
        lio_bda_and_gdc_set_mode((palette & (1 << i)) ? 0x23 : 0x22);
        pc98_gdc[GDC_SLAVE].set_csrw(ead + gdc_base[i], dad);
        pc98_gdc[GDC_SLAVE].exec(0x6c);
    }
}

static void lio_gline(int xx1, int yy1, int xx2, int yy2, int palette, uint16_t pattern) {
    int swap;
    int width;
    int height;
    int d1;
    int d2;
    int x1 = xx1;
    int y1 = yy1;
    int x2 = xx2;
    int y2 = yy2;

    swap = 0;
    if(x1 > x2) {
        std::swap(x1, x2);
        std::swap(y1, y2);
        swap = 1;
    }
    if(x1 > lio_draw.x2 || x2 < lio_draw.x1) {
        return;
    }
    width = x2 - x1;
    height = y2 - y1;
    d1 = lio_draw.x1 - x1;
    d2 = x2 - lio_draw.x2;
    if(d1 > 0) {
        x1 = lio_draw.x1;
        y1 += (((height * d1 * 2) / width) + 1) >> 1;
    }
    if (d2 > 0) {
        x2 = lio_draw.x2;
        y2 -= (((height * d2 * 2) / width) + 1) >> 1;
    }
    if(swap) {
        std::swap(x1, x2);
        std::swap(y1, y2);
    }
    swap = 0;
    if(y1 > y2) {
        std::swap(x1, x2);
        std::swap(y1, y2);
        swap = 1;
    }
    if(y1 > lio_draw.y2 || y2 < lio_draw.y1) {
        return;
    }
    width = x2 - x1;
    height = y2 - y1;
    d1 = lio_draw.y1 - y1;
    d2 = y2 - lio_draw.y2;
    if(d1 > 0) {
        y1 = lio_draw.y1;
        x1 += (((width * d1 * 2) / height) + 1) >> 1;
    }
    if(d2 > 0) {
        y2 = lio_draw.y2;
        x2 -= (((width * d2 * 2) / height) + 1) >> 1;
    }
    if(swap) {
        std::swap(x1, x2);
        std::swap(y1, y2);
    }
    d1 = x1 - xx1;
    if (d1 < 0) {
        d1 = 0 - d1;
    }
    d2 = y1 - yy1;
    if (d2 < 0) {
        d2 = 0 - d2;
    }
    d1 = MAX(d1, d2) & 15;
    pattern = (uint16_t)((pattern >> d1) | (pattern << (16 - d1)));
    pc98_gdc[GDC_SLAVE].set_textw(pattern);

    lio_gline_sub(x1, y1, x2, y2, (y1 * 40) + (x1 >> 4), x1 % 16, palette);
}

static void lio_glineb(int x1, int y1, int x2, int y2, int palette, uint16_t pattern) {
    lio_gline(x1, y1, x2, y1, palette, pattern);
    lio_gline(x2, y1, x2, y2, palette, pattern);
    lio_gline(x2, y2, x1, y2, palette, pattern);
    lio_gline(x1, y2, x1, y1, palette, pattern);
}

unsigned char byte_reverse(unsigned char c) {
    c = ((c & 0xaa) >> 1) | ((c & 0x55) << 1);
    c = ((c & 0xcc) >> 2) | ((c & 0x33) << 2);
    return (unsigned char)((c >> 4) | (c << 4));
}

static void lio_gbox(int px1, int py1, int px2, int py2, int palette, uint8_t *tile, uint8_t length) {
    uint16_t pattern;
    uint8_t r;
    uint8_t *tterm;
    int temp;
    int x1 = px1;
    int y1 = py1;
    int x2 = px2;
    int y2 = py2;

    if(x1 > x2) {
        std::swap(x1, x2);
    }
    if(y1 > y2) {
        std::swap(y1, y2);
    }
    if(x1 > lio_draw.x2 || x2 < lio_draw.x1 || y1 > lio_draw.y2 || y2 < lio_draw.y1) {
        return;
    }
    x1 = MAX(x1, lio_draw.x1);
    y1 = MAX(y1, lio_draw.y1);
    x2 = MIN(x2, lio_draw.x2);
    y2 = MIN(y2, lio_draw.y2);

    if(length == 0) {
        tile = NULL;
        tterm = NULL;
    } else {
        tterm = tile + length;
        temp = (x1 - lio_draw.x1) & 7;
        do {
            r = byte_reverse(*tile);
            *tile = (uint8_t)((r << temp) | (r >> (8 - temp)));
        } while(++tile < tterm);
        tile -= length;
        temp = (y1 - lio_draw.y1) * lio_draw.planes;
        tile += temp % length;
        palette = 0x0f;
    }
    pattern = 0xffff;
    while(y1 <= y2) {
        uint16_t ead = (y1 * 40) + (x1 >> 4);
        uint8_t dad = x1 % 16;

        if(lio_draw.flag & LIODRAW_UPPER) {
            ead += 16000 >> 1;
        }
        r = 0;
        do {
            if(tile) {
                pattern = (*tile << 8) | *tile;
                if(++tile >= tterm) {
                    tile -= length;
                }
            }
            pc98_gdc[GDC_SLAVE].set_textw(pattern);
            pc98_gdc[GDC_SLAVE].set_vectl(x1, y1, x2, y1);

            lio_bda_and_gdc_set_mode((palette & (1 << r)) ? 0x23 : 0x22);
            pc98_gdc[GDC_SLAVE].set_csrw(ead + gdc_base[r], dad);
            pc98_gdc[GDC_SLAVE].exec(0x6c);
        } while(++r < lio_draw.planes);
        y1++;
    }
}

static void lio_init_palette() {
    if(lio_palette_mode == 0) {
        IO_WriteB(0xae, 0x04);
        IO_WriteB(0xaa, 0x15);
        IO_WriteB(0xac, 0x26);
        IO_WriteB(0xa8, 0x37);
    } else {
        int i;
        for(i = 0 ; i < 8 ; i++) {
            IO_WriteB(0xa8, i);
            IO_WriteB(0xae, (i & 1) ? 0x0f : 0); // B
            IO_WriteB(0xac, (i & 2) ? 0x0f : 0); // R
            IO_WriteB(0xaa, (i & 4) ? 0x0f : 0); // G
        }
        IO_WriteB(0xa8, 8);
        IO_WriteB(0xae, 0x07); // B
        IO_WriteB(0xac, 0x07); // R
        IO_WriteB(0xaa, 0x07); // G
        for(i = 9 ; i < 16 ; i++) {
            IO_WriteB(0xa8, i);
            IO_WriteB(0xae, (i & 1) ? 0x0a : 0); // B
            IO_WriteB(0xac, (i & 2) ? 0x0a : 0); // R
            IO_WriteB(0xaa, (i & 4) ? 0x0a : 0); // G
        }
    }
}

uint8_t PC98_BIOS_LIO_GINIT() {
    reg_ah = 0x42; reg_ch = 0x80;
    CALLBACK_RunRealInt(0x18);

    reg_ah = 0x40;
    CALLBACK_RunRealInt(0x18);

    IO_WriteB(0x6a, 0x00);

    memset(&lio_work, 0, sizeof(lio_work));
    lio_work.plane = 1;
    lio_work.fore_color = 7;
    for(uint8_t i = 0 ; i < 8 ; i++) {
        lio_work.color[i] = i;
    }
    lio_work.view_x2 = 639;
    lio_work.view_y2 = 399;
    lio_palette_mode = 0;
    lio_init_palette();

    lio_write_parameter();

    return LIO_SUCCESS;
}

static int mono_plane_palette[8][4] = {
    { 0x00,0x00,0x00,0x00 },
    { 0x77,0x77,0x00,0x00 },
    { 0x77,0x00,0x77,0x00 },
    { 0x77,0x77,0x77,0x00 },
    { 0x07,0x07,0x07,0x07 },
    { 0x77,0x77,0x07,0x07 },
    { 0x77,0x07,0x77,0x07 },
    { 0x77,0x77,0x77,0x07 },
};

uint8_t PC98_BIOS_LIO_GSCREEN() {
    uint8_t screen_mode;
    uint8_t sw;
    uint8_t mono;
    uint8_t active;
    uint8_t disp;
    uint8_t pos;
    uint8_t plane;
    uint8_t plane_max;
    uint8_t mode;
    uint16_t color_bit;
    uint16_t seg = SegValue(ds);
    uint16_t off = reg_bx;

    color_bit = (lio_palette_mode != 2) ? 3 : 4;
    screen_mode = real_readb(seg, off);
    if(screen_mode == 0xff) {
        screen_mode = lio_work.screen_mode;
    } else {
        if(screen_mode >= 2 && !(mem_readb(0x54C) & 0x40)) {
            return LIO_ILLEGALFUNC;
        }
    }
    if(screen_mode >= 4) {
        return LIO_ILLEGALFUNC;
    }
    sw = real_readb(seg, off + 1);
    if(sw != 0xff) {
        reg_ah = (sw & 2) ? 0x41 : 0x40;
        CALLBACK_RunRealInt(0x18);
    }

    mono = ((screen_mode + 1) >> 1) & 1;
    active = real_readb(seg, off + 2);
    if(active == 0xff) {
        if(screen_mode != lio_work.screen_mode) {
            lio_work.pos = 0;
            lio_work.access = 0;
        }
    } else {
        switch(screen_mode) {
            case 0:
                pos = active & 1;
                active >>= 1;
                break;
            case 1:
                pos = active % (color_bit * 2);
                active = active / (color_bit * 2);
                break;
            case 2:
                pos = active % color_bit;
                active = active / color_bit;
                break;
            case 3:
            default:
                pos = 0;
                break;
        }
        if(active >= 2) {
            return LIO_ILLEGALFUNC;
        }
        lio_work.pos = pos;
        lio_work.access = active;
    }
    disp = real_readb(seg, off + 3);
    if(disp == 0xff) {
        if(screen_mode != lio_work.screen_mode) {
            lio_work.plane = 1;
            lio_work.disp = 0;
            disp = 0;
        }
    } else {
        plane = disp & ((2 << color_bit) - 1);
        disp >>= (color_bit + 1);
        if(disp >= 2) {
            return LIO_ILLEGALFUNC;
        }
        lio_work.disp = disp;
        plane_max = 1;
        if(mono) {
            plane_max <<= color_bit;
        }
        if(!(screen_mode & 2)) {
            plane_max <<= 1;
        }
        if((plane > plane_max) && (plane != (1 << color_bit))) {
            return LIO_ILLEGALFUNC;
        }
        lio_work.plane = plane;
        lio_work.disp = disp;
    }
    lio_work.screen_mode = screen_mode;
    pos = lio_work.pos;
    switch(screen_mode) {
        case 0:
            mode = (pos) ? 0x40 : 0x80;
            break;
        case 1:
            mode = (pos >= color_bit) ? 0x60 : 0xa0;
            break;
        case 2:
            mode = 0xe0;
            break;
        case 3:
        default:
            mode = 0xc0;
            break;
    }
    if(lio_palette_mode == 0 && (screen_mode == 1 || screen_mode == 2) && plane < 8) {
        IO_WriteB(0xa8, mono_plane_palette[plane][0]);
        IO_WriteB(0xaa, mono_plane_palette[plane][1]);
        IO_WriteB(0xac, mono_plane_palette[plane][2]);
        IO_WriteB(0xae, mono_plane_palette[plane][3]);
    }
    mode |= disp << 4;
    reg_ch = mode;
    reg_ah = 0x42;
    CALLBACK_RunRealInt(0x18);

    IO_WriteB(0xa6, lio_work.access);
    lio_write_parameter();

    return LIO_SUCCESS;
}

uint8_t PC98_BIOS_LIO_GVIEW() {
    uint16_t seg = SegValue(ds);
    uint16_t off = reg_bx;
    uint8_t palette;

    lio_work.view_x1 = real_readw(seg, off);
    lio_work.view_y1 = real_readw(seg, off + 2);
    lio_work.view_x2 = real_readw(seg, off + 4);
    lio_work.view_y2 = real_readw(seg, off + 6);
    if(lio_work.view_x1 >= lio_work.view_x2 || lio_work.view_y1 >= lio_work.view_y2) {
        return LIO_ILLEGALFUNC;
    }
    lio_updatedraw();
    palette = real_readb(seg, off + 8);
    if(palette != 0xff) {
        lio_gbox(lio_work.view_x1, lio_work.view_y1, lio_work.view_x2, lio_work.view_y2, palette, NULL, 0);
    }
    palette = real_readb(seg, off + 9);
    if(palette != 0xff) {
        lio_glineb(lio_work.view_x1, lio_work.view_y1, lio_work.view_x2, lio_work.view_y2, palette, 0xffff);
    }
    lio_write_parameter();

    return LIO_SUCCESS;
}

uint8_t PC98_BIOS_LIO_GCOLOR1() {
    uint16_t seg = SegValue(ds);
    uint16_t off = reg_bx;
    uint8_t back_color;
    uint8_t fore_color;
    uint8_t palette_mode;

    back_color = real_readb(seg, off + 1);
    if(back_color != 0xff) {
        lio_work.back_color = back_color;
    }
    fore_color = real_readb(seg, off + 3);
    if(fore_color != 0xff) {
        lio_work.fore_color = fore_color;
    }
    palette_mode = real_readb(seg, off + 4);
    if(palette_mode != 0xff) {
        if(!(mem_readb(0x54C) & 1)) {
            palette_mode = 0;
        } else {
            if (!(mem_readb(0x54C) & 4)) {
                return LIO_ILLEGALFUNC;
            }
            IO_WriteB(0x6a, palette_mode ? 1 : 0);
        }
        lio_palette_mode = palette_mode;
        lio_init_palette();
    }
    lio_write_parameter();

    return LIO_SUCCESS;
}

uint8_t PC98_BIOS_LIO_GCOLOR2() {
    uint16_t seg = SegValue(ds);
    uint16_t off = reg_bx;
    uint8_t palette;
    uint8_t color1;
    uint8_t color2=0;

    palette = real_readb(seg, off);
    if(palette >= ((lio_palette_mode == 2) ? 16 : 8)) {
        return LIO_ILLEGALFUNC;
    }
    color1 = real_readb(seg, off + 1);
    if(!lio_palette_mode) {
        const uint8_t port[] = { 0xae, 0xaa, 0xac, 0xa8 };
        uint8_t data;
        data = IO_ReadB(port[palette & 0x03]);
        color1 &= 7;
        lio_work.color[palette] = color1;
        if(palette & 0x04) {
            data = (data & 0xf0) | color1;
        } else {
            data = (data & 0x0f) | (color1 << 4);
        }
        IO_WriteB(port[palette & 0x03], data);
    } else {
        color2 = real_readb(seg, off + 2);
        IO_WriteB(0xa8, palette);
        IO_WriteB(0xaa, color2);
        IO_WriteB(0xac, color1 >> 4);
        IO_WriteB(0xae, color1 & 0x0f);
    }
    lio_write_parameter();

    return LIO_SUCCESS;
}

uint8_t PC98_BIOS_LIO_GCLS() {
    lio_updatedraw();

    lio_gbox(lio_draw.x1, lio_draw.y1, lio_draw.x2, lio_draw.y2, lio_work.back_color, NULL, 0);

    return LIO_SUCCESS;
}

uint8_t PC98_BIOS_LIO_GPSET() {
    uint16_t seg = SegValue(ds);
    uint16_t off = reg_bx;
    int16_t x;
    int16_t y;
    uint8_t palette;

    lio_updatedraw();

    x = real_readw(seg, off);
    y = real_readw(seg, off + 2);
    palette = real_readb(seg, off + 4);
    if(palette == 0xff) {
        if(reg_ah == 1) {
            palette = lio_work.fore_color;
        } else {
            palette = lio_work.back_color;
        }
    }
    lio_pset(x, y, palette);

    return LIO_SUCCESS;
}

uint8_t PC98_BIOS_LIO_GLINE() {
    uint16_t seg = SegValue(ds);
    uint16_t off = reg_bx;

    short x1, y1, x2, y2;
    uint16_t pattern;
    uint8_t palette, type, sw;

    lio_updatedraw();

    x1 = real_readw(seg, off);
    y1 = real_readw(seg, off + 2);
    x2 = real_readw(seg, off + 4);
    y2 = real_readw(seg, off + 6);
    palette = real_readb(seg, off + 8);
    type = real_readb(seg, off + 9);
    sw = real_readb(seg, off + 0x0a);
    if(palette == 0xff) {
        palette = lio_work.fore_color;
    }
    if(palette >= lio_draw.palette_max) {
        return LIO_ILLEGALFUNC;
    }
    pattern = 0xffff;
    if(type < 2) {
        if(sw) {
            pattern = (byte_reverse(real_readb(seg, off + 0x0b)) << 8) | byte_reverse(real_readb(seg, off + 0x0c));
        }
        if(type == 0) {
            lio_gline(x1, y1, x2, y2, palette, pattern);
        } else {
            lio_glineb(x1, y1, x2, y2, palette, pattern);
        }
    } else if(type == 2) {
        uint8_t length = 0;
        uint8_t tile[256];
        if(sw == 2) {
            length = real_readb(seg, off + 0x0d);
            if(length == 0) {
                return LIO_ILLEGALFUNC;
            }
            MEM_BlockRead(PhysMake(real_readw(seg, off + 0x10), real_readw(seg, off + 0x0e)), tile, length);
        }
        if(sw != 1) {
            lio_gbox(x1, y1, x2, y2, palette, tile, length);
            lio_glineb(x1, y1, x2, y2, palette, pattern);
        } else {
            lio_gbox(x1, y1, x2, y2, real_readb(seg, off + 0x0b), tile, length);
            lio_glineb(x1, y1, x2, y2, palette, pattern);
        }
    } else {
        return LIO_ILLEGALFUNC;
    }
    return LIO_SUCCESS;
}

static void lio_set_tile(uint16_t seg, uint16_t off, int count) {
    uint8_t data[4];

    for(int i = 0 ; i < count ; i++) {
        MEM_BlockRead(PhysMake(seg, off), data, lio_draw.planes);
        off += lio_draw.planes;
        for(uint8_t bit = 0x80 ; bit != 0 ; bit >>= 1) {
            uint8_t palette = 0;
            if(data[0] & bit) {
                palette |= 0x01;
            }
            if(data[1] & bit) {
                palette |= 0x02;
            }
            if(data[2] & bit) {
                palette |= 0x04;
            }
            if(lio_draw.planes == 4) {
                if(data[3] & bit) {
                    palette |= 0x08;
                }
            }
            paint_tile.push_back(palette);
        }
    }
}

static void lio_circle_pset(short x, short y, uint8_t palette)
{
    if(x >= lio_draw.x1 && x <= lio_draw.x2 && y >= lio_draw.y1 && y <= lio_draw.y2) {
        lio_draw.paint_work[x + y * 640] = 1;
        lio_pset(x, y, palette);
    }
}

static void lio_circle_fill(short cx, short cy, short my, short py, uint8_t flag, uint8_t count)
{
    short x, y;

    y = cy;
    while(1) {
        if(py == 1 && y > my) {
            break;
        }
        if(py == -1 && y < my) {
            break;
        }
        if(!lio_draw.paint_work[cx + y * 640]) {
            x = cx;
            while(x >= lio_draw.x1) {
                if(lio_draw.paint_work[x + y * 640]) {
                    break;
                }
                if(flag & 0x40) {
                    lio_pset(x, y, paint_tile[(y % count) * 8 + x % 8]);
                } else {
                    lio_pset(x, y, count);
                }
                x--;
            }
            x = cx + 1;
            while(x <= lio_draw.x2) {
                if(lio_draw.paint_work[x + y * 640]) {
                    break;
                }
                if(flag & 0x40) {
                    lio_pset(x, y, paint_tile[(y % count) * 8 + x % 8]);
                } else {
                    lio_pset(x, y, count);
                }
                x++;
            }
        }
        y += py;
    }
}

static int lio_circle_dir(int cx, int cy, int sx, int sy)
{
    if(sy > cy) {
        if(sx > cx) {
            return 1;
        } else if(sx < cx) {
            return 3;
        } else {
            return 2;
        }
    } else if(sy < cy) {
        if(sx > cx) {
            return 7;
        } else if(sx < cx) {
            return 5;
        } else {
            return 6;
        }
    } else {
        if(sx > cx) {
            return 0;
        }
    }
    return 4;
}

uint8_t PC98_BIOS_LIO_GCIRCLE() {
    uint16_t seg = SegValue(ds);
    uint16_t off = reg_bx;

    uint8_t palette;
    uint8_t count;
    uint8_t flag;
    short cx, cy;
    short rx, ry;
    int x, x1, y, y1, r;
    int sx, sy, ex, ey;
    uint8_t draw_flag[8];

    lio_updatedraw();

    cx = real_readw(seg, off);
    cy = real_readw(seg, off + 2);
    rx = real_readw(seg, off + 4);
    ry = real_readw(seg, off + 6);
    palette = real_readb(seg, off + 8);
    if(palette == 0xff) {
        palette = lio_work.fore_color;
    }
    flag = real_readb(seg, off + 9);
    sx = real_readw(seg, off + 0x0a);
    sy = real_readw(seg, off + 0x0c);
    ex = real_readw(seg, off + 0x0e);
    ey = real_readw(seg, off + 0x10);
    memset(draw_flag, 1, sizeof(draw_flag));
    if(flag & 0x1f) {
        LOG_MSG("LIO GCIRCLE not support flags: %02x", flag);
        if((flag & 5) == 5) {
            int sdir, edir;
            sdir = lio_circle_dir(cx, cy, sx, sy);
            edir = lio_circle_dir(cx, cy, ex, ey);
            if(sdir == edir && (flag & 0x10)) {
                lio_pset(sx, sy, palette);
                return LIO_SUCCESS;
            }
            while(sdir != edir) {
                draw_flag[sdir++] = 0;
                if(sdir >= 8) sdir = 0;
            }
        }
    }
    count = real_readb(seg, off + 0x12);
    if(lio_draw.paint_work == NULL) {
        lio_draw.paint_work = new uint8_t[640 * 400];
    }
    if(flag & 0x60) {
        memset(lio_draw.paint_work, 0, 640 * 400);
    }
    if(flag & 0x40) {
        if(count % lio_draw.planes) {
            return LIO_ILLEGALFUNC;
        }
        count /= lio_draw.planes;
        lio_set_tile(real_readw(seg, off + 0x15), real_readw(seg, off + 0x13), count);
    } else {
        if(count == 0xff) {
            count = palette;
        }
    }
    if(rx > ry) {
        x = r = rx;
        y = 0;
        while(x >= y) {
            x1 = x * ry / rx;
            y1 = y * ry / rx;
            if(draw_flag[0]) lio_circle_pset(cx + x, cy + y1, palette);
            if(draw_flag[1]) lio_circle_pset(cx + y, cy + x1, palette);
            if(draw_flag[2]) lio_circle_pset(cx - y, cy + x1, palette);
            if(draw_flag[3]) lio_circle_pset(cx - x, cy + y1, palette);
            if(draw_flag[4]) lio_circle_pset(cx - x, cy - y1, palette);
            if(draw_flag[5]) lio_circle_pset(cx - y, cy - x1, palette);
            if(draw_flag[6]) lio_circle_pset(cx + y, cy - x1, palette);
            if(draw_flag[7]) lio_circle_pset(cx + x, cy - y1, palette);
            if((r -= y++ * 2 + 1) <= 0) {
                r += --x * 2;
            }
        }
    } else {
        x = r = ry;
        y = 0;
        while(x >= y) {
            x1 = x * rx / ry;
            y1 = y * rx / ry;
            if(draw_flag[0]) lio_circle_pset(cx + x1, cy + y, palette);
            if(draw_flag[1]) lio_circle_pset(cx + y1, cy + x, palette);
            if(draw_flag[2]) lio_circle_pset(cx - y1, cy + x, palette);
            if(draw_flag[3]) lio_circle_pset(cx - x1, cy + y, palette);
            if(draw_flag[4]) lio_circle_pset(cx - x1, cy - y, palette);
            if(draw_flag[5]) lio_circle_pset(cx - y1, cy - x, palette);
            if(draw_flag[6]) lio_circle_pset(cx + y1, cy - x, palette);
            if(draw_flag[7]) lio_circle_pset(cx + x1, cy - y, palette);
            if((r -= y++ * 2 + 1) <= 0) {
                r += --x * 2;
            }
        }
    }
    if((flag & 0x03) == 0x03) {
        lio_gline(cx, cy, sx, sy, palette, 0xffff);
    }
    if((flag & 0x0c) == 0x0c) {
        lio_gline(cx, cy, ex, ey, palette, 0xffff);
    }
    if((flag & 0x60) && (flag & 0x0f) == 0) {
        lio_circle_fill(cx, cy, cy - ry, -1, flag, count);
        lio_circle_fill(cx, cy + 1, cy + ry, 1, flag, count);
    }

    paint_tile.clear();
    return LIO_SUCCESS;
}

static uint8_t lio_point(short x, short y) {
    uint16_t addr;
    uint16_t shift;
    uint8_t ret = 0;

    addr = (x >> 3) + (y * 80);
    if(lio_draw.flag & LIODRAW_UPPER) {
        addr += 16000;
    }
    shift = (~x) & 7;
    for(uint16_t plane = 0 ; plane < 3 ; plane++) {
        ret += (((mem_readb(lio_base[plane] + addr) >> shift) & 1) << plane);
    }
    if(lio_draw.flag & LIODRAW_4BPP) {
        ret += (((mem_readb(lio_base[3] + addr) >> shift) & 1) << 3);
    }
    return ret;
}

void lio_scan_line(int lx, int rx, int y, uint8_t border) {
    PAINT_POINT point;

    while(lx <= rx) {
        while(lx <= rx) {
            if(lio_point(lx, y) != border) {
                break;
            }
            lx++;
        }
        if(lio_point(lx, y) == border) {
            break;
        }

        while(lx <= rx) {
            if(lio_point(lx, y) == border) {
                break;
            }
            lx++;
        }
        point.x = lx - 1;
        point.y = y;
        paint_point.push(point);
    }
}

static void lio_paint(short x, short y, uint8_t border) {
    short lx, rx, ly, i;
    PAINT_POINT point;

    if(lio_draw.paint_work == NULL) {
        lio_draw.paint_work = new uint8_t[640 * 400];
    }
    memset(lio_draw.paint_work, 0, 640 * 400);
    point.x = x;
    point.y = y;
    paint_point.push(point);
    while(!paint_point.empty()) {
        point = paint_point.front();
        paint_point.pop();
        lx = rx = point.x;
        ly = point.y;
        if(lio_point(lx, ly) == border || lio_draw.paint_work[lx + ly * 640] != 0) {
            continue;
        }

        while (rx < lio_draw.x2) {
            if(lio_point(rx + 1, ly) == border) {
                break;
            }
            rx++;
        }
        while(lx > lio_draw.x1) {
            if(lio_point(lx - 1, ly) == border) {
                break;
            }
            lx--;
        }
        for(i = lx ; i <= rx ; i++) {
            lio_draw.paint_work[i + ly * 640] = 1;
        }
        PAINT_LINE line;
        line.lx = lx;
        line.rx = rx;
        line.y = ly;
        paint_line.push_back(line);
        if(ly - 1 >= lio_draw.y1) {
            lio_scan_line(lx, rx, ly - 1, border);
        }
        if(ly + 1 <= lio_draw.y2) {
            lio_scan_line(lx, rx, ly + 1, border);
        }
    }
}


uint8_t PC98_BIOS_LIO_GPAINT1() {
    uint16_t seg = SegValue(ds);
    uint16_t off = reg_bx;

    short x, y;
    uint8_t palette, border;
    x = real_readw(seg, off);
    y = real_readw(seg, off + 2);
    palette = real_readb(seg, off + 4);
    if(palette == 0xff) {
        palette = lio_work.fore_color;
    }
    border = real_readb(seg, off + 5);
    if(border == 0xff) {
        border = palette;
    }
    lio_paint(x, y, border);
    for(std::vector<PAINT_LINE>::iterator line = paint_line.begin() ; line != paint_line.end() ; ++line) {
        lio_gline(line->lx, line->y, line->rx, line->y, palette, 0xffff);
    }
    paint_line.clear();
    return LIO_SUCCESS;
}

uint8_t PC98_BIOS_LIO_GPAINT2() {
    uint16_t seg = SegValue(ds);
    uint16_t off = reg_bx;

    short x, y;
    uint8_t border;
    int count;

    x = real_readw(seg, off);
    y = real_readw(seg, off + 2);
    count = real_readb(seg, off + 5);
    if(count % lio_draw.planes) {
        return LIO_ILLEGALFUNC;
    }
    count /= lio_draw.planes;
    lio_set_tile(real_readw(seg, off + 8), real_readw(seg, off + 6), count);
    border = real_readb(seg, off + 0x0a);

    lio_paint(x, y, border);
    for(std::vector<PAINT_LINE>::iterator line = paint_line.begin() ; line != paint_line.end() ; ++line) {
        for(x = line->lx ; x <= line->rx ; x++) {
            lio_pset(x, line->y, paint_tile[(line->y % count) * 8 + x % 8]);
        }
    }
    paint_line.clear();
    paint_tile.clear();
    return LIO_SUCCESS;
}

static void lio_putor(PUTCNTX *pt) {
    PhysPt addr;
    uint8_t *src;
    uint16_t width;
    uint16_t data;

    addr = pt->base + pt->addr;
    src = pt->buffer;
    width = pt->width;
    data = *src++;
    if((pt->shift + width) < 8) {
        mem_writeb(addr, mem_readb(addr) | ((uint8_t)((data >> pt->shift) & pt->masklr)));
    } else {
        mem_writeb(addr, mem_readb(addr) | ((uint8_t)((data >> pt->shift) & pt->maskl)));
        addr++;
        width -= (8 - pt->shift);
        while(width > 8) {
            width -= 8;
            data = (data << 8) + (*src);
            src++;
            mem_writeb(addr, mem_readb(addr) | ((uint8_t)(data >> pt->shift)));
            addr++;
        }
        if(width) {
            data = (data << 8) + (*src);
            mem_writeb(addr, mem_readb(addr) | ((uint8_t)((data >> pt->shift) & pt->maskr)));
        }
    }
}

static void lio_putorn(PUTCNTX *pt) {
    PhysPt addr;
    uint8_t *src;
    uint16_t width;
    uint16_t data;

    addr = pt->base + pt->addr;
    src = pt->buffer;
    width = pt->width;
    data = *src++;
    if((pt->shift + width) < 8) {
        mem_writeb(addr, mem_readb(addr) | ((uint8_t)((~data) >> pt->shift) & pt->masklr));
    } else {
        mem_writeb(addr, mem_readb(addr) | ((uint8_t)((~data) >> pt->shift) & pt->maskl));
        addr++;
        width -= (8 - pt->shift);
        while(width > 8) {
            width -= 8;
            data = (data << 8) + (*src);
            src++;
            mem_writeb(addr, mem_readb(addr) | ((uint8_t)((~data) >> pt->shift)));
            addr++;
        }
        if(width) {
            data = (data << 8) + (*src);
            mem_writeb(addr, mem_readb(addr) | ((uint8_t)((~data) >> pt->shift) & pt->maskr));
        }
    }
}

static void lio_putand(PUTCNTX *pt) {
    PhysPt addr;
    uint8_t *src;
    uint16_t width;
    uint16_t data;

    addr = pt->base + pt->addr;
    src = pt->buffer;
    width = pt->width;
    data = *src++;
    if((pt->shift + width) < 8) {
        mem_writeb(addr, mem_readb(addr) & ((uint8_t)((data >> pt->shift) | (~pt->masklr))));
    } else {
        mem_writeb(addr, mem_readb(addr) & ((uint8_t)((data >> pt->shift) | (~pt->maskl))));
        addr++;
        width -= (8 - pt->shift);
        while(width > 8) {
            width -= 8;
            data = (data << 8) + (*src);
            src++;
            mem_writeb(addr, mem_readb(addr) & ((uint8_t)(data >> pt->shift)));
            addr++;
        }
        if (width) {
            data = (data << 8) + (*src);
            mem_writeb(addr, mem_readb(addr) & ((uint8_t)((data >> pt->shift) | (~pt->maskr))));
        }
    }
}

static void lio_putandn(PUTCNTX *pt) {
    PhysPt addr;
    uint8_t *src;
    uint16_t width;
    uint16_t data;

    addr = pt->base + pt->addr;
    src = pt->buffer;
    width = pt->width;
    data = *src++;
    if((pt->shift + width) < 8) {
        mem_writeb(addr, mem_readb(addr) & ((uint8_t)(~((data >> pt->shift) & pt->masklr))));
    } else {
        mem_writeb(addr, mem_readb(addr) & ((uint8_t)(~((data >> pt->shift) & pt->maskl))));
        addr++;
        width -= (8 - pt->shift);
        while(width > 8) {
            width -= 8;
            data = (data << 8) + (*src);
            src++;
            mem_writeb(addr, mem_readb(addr) & ((uint8_t)((~data) >> pt->shift)));
            addr++;
        }
        if(width) {
            data = (data << 8) + (*src);
            mem_writeb(addr, mem_readb(addr) & ((uint8_t)(~((data >> pt->shift) & pt->maskr))));
        }
    }
}

static void lio_putxor(PUTCNTX *pt) {
    PhysPt addr;
    uint8_t *src;
    uint16_t width;
    uint16_t data;

    addr = pt->base + pt->addr;
    src = pt->buffer;
    width = pt->width;
    data = *src++;
    if ((pt->shift + width) < 8) {
        mem_writeb(addr, mem_readb(addr) ^ ((uint8_t)((data >> pt->shift) & pt->masklr)));
    } else {
        mem_writeb(addr, mem_readb(addr) ^ ((uint8_t)(data >> pt->shift) & pt->maskl));
        addr++;
        width -= (8 - pt->shift);
        while(width > 8) {
            width -= 8;
            data = (data << 8) + (*src);
            src++;
            mem_writeb(addr, mem_readb(addr) ^ ((uint8_t)(data >> pt->shift)));
            addr++;
        }
        if (width) {
            data = (data << 8) + (*src);
            mem_writeb(addr, mem_readb(addr) ^ ((uint8_t)(data >> pt->shift) & pt->maskr));
        }
    }
}

static void lio_putxorn(PUTCNTX *pt) {
    PhysPt addr;
    uint8_t *src;
    uint16_t width;
    uint16_t data;

    addr = pt->base + pt->addr;
    src = pt->buffer;
    width = pt->width;
    data = *src++;
    if((pt->shift + width) < 8) {
        mem_writeb(addr, mem_readb(addr) ^ ((uint8_t)(((~data) >> pt->shift) & pt->masklr)));
    } else {
        mem_writeb(addr, mem_readb(addr) ^ ((uint8_t)(((~data) >> pt->shift) & pt->maskl)));
        addr++;
        width -= (8 - pt->shift);
        while(width > 8) {
            width -= 8;
            data = (data << 8) + (*src);
            src++;
            mem_writeb(addr, mem_readb(addr) ^ ((uint8_t)((~data) >> pt->shift)));
            addr++;
        }
        if(width) {
            data = (data << 8) + (*src);
            mem_writeb(addr, mem_readb(addr) ^ ((uint8_t)(((~data) >> pt->shift) & pt->maskr)));
        }
    }
}

static uint8_t lio_putsub(const LIOPUT *lput) {
    PUTCNTX pt;
    uint16_t addr;
    uint16_t data_count;
    uint16_t off;
    uint16_t height;
    uint16_t flag;
    uint16_t write_count;

    if(lput->x < lio_draw.x1 || lput->y < lio_draw.y1
      || (lput->x + lput->width - 1) > lio_draw.x2 || (lput->y + lput->height - 1) > lio_draw.y2) {
        return LIO_ILLEGALFUNC;
    }
    if(lput->width <= 0 || lput->height <= 0) {
        return LIO_SUCCESS;
    }
    addr = (lput->x >> 3) + (lput->y * 80);
    if(lio_draw.flag & LIODRAW_UPPER) {
        addr += 16000;
    }
    pt.addr = addr;
    pt.shift = lput->x & 7;
    pt.width = lput->width;
    pt.maskl = (uint8_t)(0xff >> pt.shift);
    pt.maskr = (uint8_t)((~0x7f) >> ((pt.width + pt.shift - 1) & 7));
    pt.masklr = pt.maskl;
    short rz = 8 - pt.shift - pt.width;
    if(rz > 0) {
        pt.masklr = (uint8_t)((pt.masklr >> rz) << rz);
    }
    data_count = (lput->width + 7) >> 3;
    off = lput->off;
    flag = (lio_draw.flag & LIODRAW_4BPP) ? 0x0f : 0x07;
    flag |= (lput->fore_color & 15) << 4;
    flag |= (lput->back_color & 15) << 8;

    write_count = 0;
    height = lput->height;
    do {
        flag <<= 4;
        for(uint8_t plane = 0 ; plane < 4 ; plane++) {
            flag >>= 1;
            if(flag & 8) {
                pt.base = lio_base[plane];
                MEM_BlockRead(PhysMake(lput->seg, off), pt.buffer, data_count);
                if(lput->sw) {
                    off += data_count;
                }
                switch(lput->mode) {
                    case 0: // PSET
                        if(flag & (8 << 4)) {
                            lio_putor(&pt);
                        } else {
                            lio_putandn(&pt);
                        }
                        if(flag & (8 << 8)) {
                            lio_putorn(&pt);
                        } else {
                            lio_putand(&pt);
                        }
                        write_count += 2;
                        break;
                    case 1: // NOT
                        if(!(flag & (8 << 4))) {
                            lio_putor(&pt);
                        } else {
                            lio_putandn(&pt);
                        }
                        if(!(flag & (8 << 8))) {
                            lio_putorn(&pt);
                        } else {
                            lio_putand(&pt);
                        }
                        write_count += 2;
                        break;
                    case 2: // OR
                        if(flag & (8 << 4)) {
                            lio_putor(&pt);
                            write_count++;
                        }
                        if(flag & (8 << 8)) {
                            lio_putorn(&pt);
                            write_count++;
                        }
                        break;
                    case 3: // AND
                        if(!(flag & (8 << 4))) {
                            lio_putandn(&pt);
                            write_count++;
                        }
                        if(!(flag & (8 << 8))) {
                            lio_putand(&pt);
                            write_count++;
                        }
                        break;
                    case 4: // XOR
                        if (flag & (8 << 4)) {
                            lio_putxor(&pt);
                            write_count++;
                        }
                        if (flag & (8 << 8)) {
                            lio_putxorn(&pt);
                            write_count++;
                        }
                        break;
                }
            }
        }
        pt.addr += 80;
        if(!lput->sw) {
            off += data_count;
        }
    } while(--height);
    return LIO_SUCCESS;
}

uint8_t PC98_BIOS_LIO_GPUT1() {
    uint16_t seg = SegValue(ds);
    uint16_t off = reg_bx;
    uint16_t length;
    uint16_t size;
    uint8_t sw;
    uint8_t fore_color;
    uint8_t back_color;
    LIOPUT lput;

    lio_updatedraw();

    lput.x = real_readw(seg, off);
    lput.y = real_readw(seg, off + 2);
    lput.off = real_readw(seg, off + 4);
    lput.seg = real_readw(seg, off + 6);
    length = real_readw(seg, off + 8);
    lput.mode = real_readb(seg, off + 0x0a);
    sw = real_readb(seg, off + 0x0b);
    fore_color = real_readb(seg, off + 0x0c);
    back_color = real_readb(seg, off + 0x0d);
    lput.width = real_readw(lput.seg, lput.off);
    lput.height = real_readw(lput.seg, lput.off + 2);
    lput.off += 4;
    size = ((lput.width + 7) >> 3) * lput.height;
    if(length < (size + 4)) {
        return LIO_ILLEGALFUNC;
    }
    if(length < ((size * 3) + 4)) {
        lput.sw = 0;
        if(sw) {
            lput.fore_color = fore_color;
            lput.back_color = back_color;
        } else {
            fore_color = lio_work.fore_color;
            back_color = lio_work.back_color;
        }
    } else {
        if(sw) {
            lput.sw = 0;
            lput.fore_color = fore_color;
            lput.back_color = back_color;
        } else {
            lput.sw = 1;
            lput.fore_color = 0x0f;
            lput.back_color = 0;
        }
    }
    return lio_putsub(&lput);
}

static const uint8_t hiragana_font[] = {
    0x00,0x10,0x10,0x10,0x10,0x10,0x10,0x00,0x00,0x10,0x10,0x10,0x10,0x10,0x10,0x00,
    0x00,0x60,0xda,0x0c,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x30,0x48,0x48,0x30,0x00,
    0x7c,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x00,0x00,0x00,
    0x00,0x00,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x7c,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x40,0x20,0x10,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x30,0x30,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x10,0x10,0x3c,0x60,0x24,0x28,0x1c,0x32,0x52,0x42,0x40,0x62,0x3c,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x10,0x78,0x16,0x1c,0x2c,0x4a,0x5a,0x52,0x30,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x24,0x22,0x22,0x22,0x42,0x42,0x40,0x28,0x10,0x00,
    0x00,0x00,0x00,0x00,0x00,0x38,0x04,0x00,0x1c,0x62,0x02,0x02,0x02,0x04,0x08,0x00,
    0x00,0x00,0x00,0x00,0x00,0x10,0x08,0x1e,0x22,0x42,0x04,0x08,0x08,0x1a,0x24,0x00,
    0x00,0x00,0x00,0x00,0x00,0x12,0x1c,0x6a,0x08,0x1c,0x2a,0x2a,0x2a,0x2a,0x10,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x04,0x2e,0x32,0x66,0x20,0x10,0x10,0x10,0x08,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x48,0x5c,0x6a,0x6a,0x4a,0x0a,0x0c,0x08,0x08,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x0e,0x08,0x08,0x3c,0x4c,0x4a,0x4a,0x4a,0x30,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x1c,0x62,0x02,0x02,0x02,0x02,0x02,0x04,0x18,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x7e,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x20,0x78,0x2e,0x20,0x1c,0x36,0x52,0x52,0x4a,0x5a,0x32,0x02,0x00,0x00,
    0x00,0x00,0x20,0x24,0x24,0x42,0x42,0x42,0x42,0x40,0x40,0x40,0x40,0x28,0x10,0x00,
    0x00,0x00,0x38,0x04,0x00,0x00,0x3c,0x42,0x02,0x02,0x02,0x02,0x04,0x0c,0x08,0x00,
    0x00,0x00,0x10,0x08,0x1c,0x26,0x42,0x02,0x04,0x04,0x08,0x08,0x10,0x1a,0x24,0x00,
    0x00,0x10,0x16,0x1c,0x6a,0x0a,0x08,0x1c,0x2a,0x2a,0x4a,0x4a,0x6a,0x32,0x00,0x00,
    0x00,0x00,0x02,0x12,0x3a,0x54,0x14,0x22,0x22,0x22,0x22,0x22,0x04,0x04,0x00,0x00,
    0x00,0x10,0x10,0x3e,0x68,0x08,0x06,0x7c,0x04,0x1c,0x20,0x40,0x42,0x26,0x18,0x00,
    0x00,0x00,0x06,0x08,0x10,0x20,0x20,0x40,0x40,0x40,0x60,0x30,0x18,0x0e,0x00,0x00,
    0x00,0x00,0x04,0x24,0x24,0x4e,0x54,0x44,0x44,0x44,0x44,0x44,0x4c,0x28,0x00,0x00,
    0x00,0x00,0x3c,0x62,0x02,0x02,0x00,0x00,0x00,0x20,0x40,0x40,0x62,0x3e,0x00,0x00,
    0x00,0x10,0x10,0x10,0x3e,0x48,0x08,0x18,0x20,0x40,0x40,0x40,0x62,0x3e,0x00,0x00,
    0x00,0x00,0x20,0x20,0x20,0x20,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x64,0x38,0x00,
    0x00,0x10,0x10,0x7e,0x48,0x08,0x3c,0x2c,0x4a,0x4a,0x52,0x32,0x04,0x04,0x08,0x00,
    0x00,0x00,0x00,0x24,0x24,0x7e,0x25,0x24,0x44,0x48,0x40,0x40,0x20,0x32,0x0c,0x00,
    0x00,0x00,0x38,0x44,0x08,0x30,0x22,0x4c,0x30,0x20,0x40,0x40,0x42,0x26,0x18,0x00,
    0x00,0x10,0x10,0x78,0x20,0x2c,0x22,0x22,0x40,0x40,0x40,0x40,0x60,0x72,0x5c,0x00,
    0x00,0x10,0x10,0x10,0x7e,0x20,0x20,0x2c,0x32,0x62,0x02,0x02,0x02,0x0c,0x00,0x00,
    0x00,0x00,0x00,0x3c,0x62,0x42,0x02,0x02,0x02,0x02,0x02,0x02,0x04,0x0c,0x10,0x00,
    0x00,0x00,0x00,0x7e,0x0c,0x08,0x10,0x20,0x40,0x40,0x40,0x60,0x22,0x1e,0x00,0x00,
    0x00,0x00,0x10,0x10,0x1c,0x12,0x20,0x40,0x40,0x40,0x40,0x40,0x22,0x3e,0x00,0x00,
    0x00,0x00,0x10,0x10,0x7c,0x22,0x22,0x40,0x44,0x5c,0x14,0x24,0x24,0x1c,0x0a,0x00,
    0x00,0x00,0x0e,0x1a,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x50,0x50,0x32,0x0c,0x00,
    0x00,0x00,0x08,0x08,0x1c,0x2c,0x6a,0x6a,0x2a,0x72,0x56,0x56,0x2a,0x0a,0x04,0x00,
    0x00,0x00,0x20,0x20,0x24,0x6a,0x32,0x22,0x24,0x24,0x6c,0x6a,0x2a,0x26,0x00,0x00,
    0x00,0x00,0x00,0x1c,0x34,0x2a,0x4a,0x4a,0x4a,0x4a,0x4a,0x4a,0x32,0x04,0x00,0x00,
    0x00,0x08,0x08,0x04,0x5e,0x64,0x44,0x4c,0x54,0x56,0x66,0x64,0x64,0x1c,0x00,0x00,
    0x00,0x00,0x60,0x30,0x10,0x24,0x26,0x26,0x44,0x44,0x44,0x44,0x2c,0x38,0x10,0x00,
    0x00,0x00,0x3c,0x04,0x04,0x08,0x08,0x10,0x12,0x4a,0x4a,0x4a,0x0a,0x30,0x00,0x00,
    0x00,0x00,0x00,0x00,0x18,0x34,0x24,0x44,0x42,0x02,0x02,0x02,0x02,0x00,0x00,0x00,
    0x00,0x04,0x1e,0x44,0x44,0x44,0x44,0x5e,0x44,0x4c,0x54,0x56,0x56,0x18,0x00,0x00,
    0x00,0x10,0x10,0x3e,0x48,0x08,0x0e,0x3c,0x48,0x18,0x2c,0x4a,0x4a,0x2a,0x30,0x00,
    0x00,0x30,0x10,0x08,0x08,0x0a,0x3a,0x4e,0x4e,0x4a,0x4b,0x52,0x24,0x04,0x04,0x00,
    0x00,0x04,0x04,0x7e,0x04,0x08,0x3a,0x2a,0x48,0x50,0x50,0x72,0x2a,0x0e,0x04,0x00,
    0x00,0x00,0x08,0x48,0x5c,0x2a,0x2a,0x6a,0x6a,0x4a,0x52,0x52,0x72,0x04,0x00,0x00,
    0x00,0x00,0x08,0x10,0x7c,0x12,0x10,0x10,0x7c,0x24,0x20,0x20,0x22,0x26,0x18,0x00,
    0x00,0x00,0x00,0x44,0x44,0x5c,0x22,0x62,0x22,0x22,0x12,0x10,0x10,0x08,0x08,0x00,
    0x00,0x00,0x08,0x5c,0x6a,0x6a,0x4a,0x4a,0x4a,0x4a,0x4a,0x0c,0x08,0x08,0x00,0x00,
    0x00,0x08,0x08,0x0e,0x08,0x08,0x08,0x3c,0x6c,0x4a,0x4a,0x4a,0x48,0x30,0x00,0x00,
    0x00,0x00,0x18,0x04,0x20,0x20,0x40,0x44,0x5e,0x62,0x02,0x02,0x02,0x02,0x04,0x00,
    0x00,0x00,0x00,0x22,0x42,0x42,0x42,0x42,0x42,0x42,0x52,0x22,0x06,0x04,0x00,0x00,
    0x00,0x00,0x3c,0x4c,0x08,0x10,0x2c,0x36,0x42,0x42,0x1a,0x26,0x22,0x24,0x18,0x00,
    0x00,0x00,0x20,0x20,0x26,0x6a,0x72,0x32,0x22,0x24,0x64,0x64,0x24,0x22,0x00,0x00,
    0x00,0x00,0x3c,0x44,0x08,0x10,0x3c,0x22,0x42,0x42,0x02,0x02,0x02,0x04,0x0c,0x00,
    0x00,0x00,0x10,0x10,0x10,0x7c,0x32,0x12,0x32,0x22,0x62,0x62,0x22,0x24,0x00,0x00,
    0x00,0x00,0x10,0x10,0x30,0x20,0x20,0x28,0x34,0x34,0x24,0x24,0x24,0x46,0x44,0x00,
    0x28,0x28,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x10,0x28,0x28,0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};

uint8_t PC98_BIOS_LIO_GPUT2() {
    uint16_t seg = SegValue(ds);
    uint16_t off = reg_bx;
    uint16_t jis;
    uint8_t sw;
    uint8_t fore_color;
    uint8_t back_color;
    uint8_t code;
    LIOPUT  lput;

    lio_updatedraw();

    lput.x = real_readw(seg, off);
    lput.y = real_readw(seg, off + 2);
    jis = real_readw(seg, off + 4);
    lput.off = 0x104e;
    lput.seg = seg;
    lput.mode = real_readb(seg, off + 6);
    sw = real_readb(seg, off + 7);
    fore_color = real_readb(seg, off + 8);
    back_color = real_readb(seg, off + 9);
    code = 0;
    if(jis < 0x200) {
        if(jis < 0x80) {
            if(jis == 0x7c) {
                code = 1;
            } else if (jis == 0x7e) {
                code = 2;
            } else {
                jis += 0x2900;
            }
        } else if (jis < 0x100) {
            if((jis - 0x20) & 0x40) {
                code = (jis & 0x3f) + 3;
            } else {
                jis += 0x2980;
            }
        } else {
            jis &= 0xff;
        }
    }
    if(!code) {
        reg_bx = seg;
        reg_cx = 0x104c;
        reg_dx = jis;
        reg_ah = 0x14;
        CALLBACK_RunRealInt(0x18);
    } else {
        MEM_BlockWrite(PhysMake(seg, 0x104e), &hiragana_font[(code - 1) * 16], 16);
        real_writeb(seg, 0x104d, 0x01);
        real_writeb(seg, 0x104c, 0x02);
    }
    lput.width = real_readb(seg, 0x104d) * 8;
    lput.height = real_readb(seg, 0x104c) * 8;
    lput.sw = 0;
    if(sw) {
        lput.fore_color = fore_color;
        lput.back_color = back_color;
    } else {
        lput.fore_color = lio_work.fore_color;
        lput.back_color = lio_work.back_color;
    }
    return lio_putsub(&lput);
}

static void lio_getvram(GETCNTX *gt, uint8_t *buffer) {
    PhysPt addr;
    uint16_t width;
    uint16_t shift;
    uint16_t data;

    addr = gt->base + gt->addr;
    width = gt->width;
    shift = 8 - gt->shift;
    data = mem_readb(addr);
    addr++;
    while(width > 8) {
        width -= 8;
        data = (data << 8) + mem_readb(addr);
        addr++;
        *buffer = (uint8_t)(data >> shift);
        buffer++;
    }
    data = (data << 8) + mem_readb(addr);
    *buffer = (uint8_t)((data >> shift) & gt->mask);
}

uint8_t PC98_BIOS_LIO_GGET() {
    uint16_t seg = SegValue(ds);
    uint16_t off = reg_bx;
    uint16_t dseg;
    uint16_t doff;
    uint16_t data_count;
    uint16_t size;
    uint16_t length;
    uint8_t mask;
    short x1;
    short y1;
    short x2;
    short y2;
    uint8_t buffer[84];
    GETCNTX gt;

    lio_updatedraw();

    x1 = real_readw(seg, off);
    y1 = real_readw(seg, off + 2);
    x2 = real_readw(seg, off + 4);
    y2 = real_readw(seg, off + 6);
    if(x1 < lio_draw.x1 || y1 < lio_draw.y1 || x2 > lio_draw.x2 || y2 > lio_draw.y2) {
        return LIO_ILLEGALFUNC;
    }
    x2 = x2 - x1 + 1;
    y2 = y2 - y1 + 1;
    if(x2 <= 0 || y2 <= 0) {
        return LIO_ILLEGALFUNC;
    }
    doff = real_readw(seg, off + 8);
    dseg = real_readw(seg, off + 0x0a);

    data_count = (x2 + 7) >> 3;
    size = data_count * y2;
    length = real_readw(seg, off + 0x0c);
    if(lio_draw.flag & LIODRAW_4BPP) {
        size *= 4;
        mask = 0x0f;
    } else {
        size *= 3;
        mask = 0x07;
    }
    if(length < (size + 4)) {
        return LIO_ILLEGALFUNC;
    }
    real_writew(dseg, doff, x2);
    real_writew(dseg, doff + 2, y2);
    doff += 4;

    gt.addr = (x1 >> 3) + (y1 * 80);
    if(lio_draw.flag & LIODRAW_UPPER) {
        gt.addr += 16000;
    }
    gt.shift = x1 & 7;
    gt.width = x2;
    gt.mask = (uint8_t)((~0x7f) >> ((x2 - 1) & 7));
    do {
        mask <<= 4;
        for(uint16_t plane = 0 ; plane < 4 ; plane++) {
            mask >>= 1;
            if(mask & 8) {
                gt.base = lio_base[plane];
                lio_getvram(&gt, buffer);
                MEM_BlockWrite(PhysMake(dseg, doff), buffer, data_count);
                doff += data_count;
            }
        }
        gt.addr += 80;
    } while(--y2);
    return LIO_SUCCESS;
}

uint8_t PC98_BIOS_LIO_GPOINT2() {
    uint16_t seg = SegValue(ds);
    uint16_t off = reg_bx;
    uint8_t ret;
    short x;
    short y;

    lio_updatedraw();

    x = real_readw(seg, off);
    y = real_readw(seg, off + 2);
    if(lio_draw.x1 > x || lio_draw.x2 < x || lio_draw.y1 > y || lio_draw.y2 < y) {
        ret = 0xff;
    } else {
        ret = lio_point(x, y);
    }
    reg_al = ret;
    return LIO_SUCCESS;
}
