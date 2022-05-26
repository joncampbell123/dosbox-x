// GDC drawing is implementation based on Neko Project II
//
#include "dosbox.h"
#include "mem.h"
#include "cpu.h"
#include "inout.h"
#include "logging.h"
#include "pc98_gdc.h"
#include "pc98_gdc_const.h"
#include <math.h>

/* do not issue CPU-side I/O here -- this code emulates functions that the GDC itself carries out, not on the CPU */
#include "cpu_io_is_forbidden.h"

uint8_t pc98_gdc_vread(const uint32_t addr);
void pc98_gdc_vwrite(const uint32_t addr,const uint8_t b);
uint16_t pc98_gdc_vreadw(const uint32_t addr);
void pc98_gdc_vwritew(const uint32_t addr,const uint16_t b);

uint16_t PC98_GDC_state::gdc_rt[PC98_GDC_state::RT_TABLEMAX + 1];
const PhysPt PC98_GDC_state::gram_base[4] = { 0xe0000, 0xa8000, 0xb0000, 0xb8000 };
const PC98_GDC_state::VECTDIR PC98_GDC_state::vectdir[16] = {
    { 0, 1, 1, 0 }, { 1, 1, 1,-1 },
    { 1, 0, 0,-1 }, { 1,-1,-1,-1 },
    { 0,-1,-1, 0 }, {-1,-1,-1, 1 },
    {-1, 0, 0, 1 }, {-1, 1, 1, 1 },

    { 0, 1, 1, 1 }, { 1, 1, 1, 0 },
    { 1, 0, 1,-1 }, { 1,-1, 0,-1 },
    { 0,-1,-1,-1 }, {-1,-1,-1, 0 },
    {-1, 0,-1, 1 }, {-1, 1, 0, 1 }
};

void PC98_GDC_state::draw_reset(void) {
    draw.dc = 0;
    draw.d = 8;
    draw.d2 = 8;
    draw.d1 = 0xffff;
    draw.dm = 0xffff;
}

void PC98_GDC_state::set_vectl(int x1, int y1, int x2, int y2) {
    int dy;
    int dx;

    draw.dir = 0;
    dy = y2 - y1;
    if(dy < 0) {
        dy = 0 - dy;
    }
    dx = x2 - x1;
    if(dx == 0) {
        if(y1 <= y2) {
            draw.dir = 7;
        } else {
            draw.dir = 3;
        }
    } else {
        if(dx > 0) {
            if(y1 >= y2) {
                draw.dir += 2;
            }
        } else {
            dx = 0 - dx;
            draw.dir += 4;
            if(y1 <= y2) {
                draw.dir += 2;
            }
        }
        if(draw.dir & 2) {
            if(dx <= dy) {
                draw.dir += 1;
            }
        } else {
            if(dx >= dy) {
                draw.dir += 1;
            }
        }
    }
    if(!((draw.dir + 1) & 2)) {
        std::swap(dx, dy);
    }

    draw.ope = 0x08;
    draw.dc = (uint16_t)dx;
    dy = dy * 2;
    draw.d1 = (uint16_t)dy;
    dy -= dx;
    draw.d = (uint16_t)dy;
    dy -= dx;
    draw.d2 = (uint16_t)dy;
}

void PC98_GDC_state::set_mode(uint8_t mode) {
    draw.mode = mode & 0x03;
}

void PC98_GDC_state::set_csrw(uint32_t ead, uint8_t dad) {
    draw.ead = ead;
    draw.dad = dad;
    draw.base = gram_base[(draw.ead >> 14) & 3];
}

void PC98_GDC_state::set_textw(uint16_t pattern) {
    draw.tx[0] = (uint8_t)(pattern & 0xff);
    draw.tx[1] = (uint8_t)(pattern >> 8);
}

void PC98_GDC_state::set_textw(uint8_t *tile, uint8_t len) {
    for(uint8_t i = 0 ; i < len ; i++) {
        draw.tx[i] = *tile++;
    }
}

void PC98_GDC_state::vectw(unsigned char bi) {
    switch(bi) {
        case 1:
            draw.ope = cmd_parm_tmp[0] & 0xf8;
            draw.dir = cmd_parm_tmp[0] & 0x07;
            break;
        case 2:
            draw.dc = cmd_parm_tmp[1];
            break;
        case 3:
            draw.dgd = cmd_parm_tmp[2] & 0x40;
            draw.dc |= (uint16_t)(cmd_parm_tmp[2] & 0x3f) << 8;
            break;
        case 4:
            draw.d = cmd_parm_tmp[3];
            break;
        case 5:
            draw.d |= (uint16_t)(cmd_parm_tmp[4] & 0x3f) << 8;
            break;
        case 6:
            draw.d2 = cmd_parm_tmp[5];
            break;
        case 7:
            draw.d2 |= (uint16_t)(cmd_parm_tmp[6] & 0x3f) << 8;
            break;
        case 8:
            draw.d1 = cmd_parm_tmp[7];
            break;
        case 9:
            draw.d1 |= (uint16_t)(cmd_parm_tmp[8] & 0x3f) << 8;
            break;
        case 10:
            draw.dm = cmd_parm_tmp[9];
            break;
        case 11:
            draw.dm |= (uint16_t)(cmd_parm_tmp[10] & 0x3f) << 8;
            break;
    }
}

void PC98_GDC_state::prepare(void) {
    draw.pattern = ((uint16_t)draw.tx[1] << 8) | draw.tx[0];
    draw.x = (uint16_t)(((draw.ead & 0x3fff) % 40) << 4) + draw.dad;
    draw.y = (uint16_t)((draw.ead & 0x3fff) / 40);
    draw.dots = 0;
}

void PC98_GDC_state::draw_dot(uint16_t x, uint16_t y) {
    uint32_t dpitch = pc98_gdc[GDC_SLAVE].display_pitch;
    uint32_t addr;
    uint16_t dot;
    uint8_t bit;

    dot = draw.pattern & 1;
    draw.pattern = (draw.pattern >> 1) + (dot << 15);
    draw.dots++;

    addr = x >> 4;
    bit = (x ^ 8) & 15; /* flip bit 3 to set correct bit despite little endian byte order */
    if(addr >= dpitch) return;
    addr += y * dpitch;
    if(addr >= 16384u) return;
    addr *= 2u;

    // HACK: If EGC enabled, use alternate path that reads and ignores the value.
    //       This is the only way for GDC drawing commands to draw properly in Windows 3.1,
    //       and possibly any other game that uses EGC. Read/Modify/Write normally with
    //       EGC causes drawing artifacts.
    //
    //       Perhaps NEC ran into this themselves? Perhaps EGC hardware can identify
    //       reads coming from the GDC and it returns 0x0000 in that case to avoid
    //       artifacts?
    //
    //       However, the dummy read is required or else stale data stored from previous
    //       ops ends up in the rendering and artifacts occur.
    //
    //       The GDC is documented to issue 16-bit read/modify/write when drawing, so
    //       that's what this code should do. Additionally, drawing with 8-bit memio
    //       and EGC causes minor artifacts.
    if ((pc98_gdc_vramop & 0xE) == 0xA || (pc98_gdc_vramop & 0xE) == 0xE) {
        if(dot) {
            // REPLACE. COMPLEMENT or SET
            if(draw.mode == 0x00 || draw.mode == 0x01 || draw.mode == 0x03) {
                pc98_gdc_vreadw(draw.base + addr); // read and discard
                pc98_gdc_vwritew(draw.base + addr, 0x8000 >> bit);
            }
        }
    }
    else {
        if(dot == 0) {
            // REPLACE
            if(draw.mode == 0x00) {
                pc98_gdc_vwritew(draw.base + addr, pc98_gdc_vreadw(draw.base + addr) & ~(0x8000 >> bit));
            }
        } else {
            // REPLACE or SET
            if(draw.mode == 0x00 || draw.mode == 0x03) {
                pc98_gdc_vwritew(draw.base + addr, pc98_gdc_vreadw(draw.base + addr) | (0x8000 >> bit));
            } else if(draw.mode == 0x01) {
                // COMPLEMENT
                pc98_gdc_vwritew(draw.base + addr, pc98_gdc_vreadw(draw.base + addr) ^ (0x8000 >> bit));
            } else {
                // CLEAR
                pc98_gdc_vwritew(draw.base + addr, pc98_gdc_vreadw(draw.base + addr) & ~(0x8000 >> bit));
            }
        }
    }
}

void PC98_GDC_state::pset(void) {
    prepare();

    draw_dot(draw.x, draw.y);
}

void PC98_GDC_state::line(void) {
    prepare();

    if(draw.dc == 0) {
        draw_dot(draw.x, draw.y);
    } else {
        uint16_t i;
        uint16_t x = draw.x;
        uint16_t y = draw.y;
        switch(draw.dir) {
            case 0:
                for(i = 0 ; i <= draw.dc ; i++) {
                    draw_dot(x + (uint16_t)((((draw.d1 * i) / draw.dc) + 1) >> 1), y++);
                }
                break;
            case 1:
                for(i = 0 ; i <= draw.dc ; i++) {
                    draw_dot(x++, y + (uint16_t)((((draw.d1 * i) / draw.dc) + 1) >> 1));
                }
                break;
            case 2:
                for(i = 0 ; i <= draw.dc ; i++) {
                    draw_dot(x++, y - (uint16_t)((((draw.d1 * i) / draw.dc) + 1) >> 1));
                }
                break;
            case 3:
                for(i = 0 ; i <= draw.dc ; i++) {
                    draw_dot(x + (uint16_t)((((draw.d1 * i) / draw.dc) + 1) >> 1), y--);
                }
                break;
            case 4:
                for(i = 0 ; i <= draw.dc ; i++) {
                    draw_dot(x - (uint16_t)((((draw.d1 * i) / draw.dc) + 1) >> 1), y--);
                }
                break;
            case 5:
                for(i = 0 ; i <= draw.dc ; i++) {
                    draw_dot(x--, y - (uint16_t)((((draw.d1 * i) / draw.dc) + 1) >> 1));
                }
                break;
            case 6:
                for(i = 0 ; i <= draw.dc ; i++) {
                    draw_dot(x--, y + (uint16_t)((((draw.d1 * i) / draw.dc) + 1) >> 1));
                }
                break;
            case 7:
                for(i = 0 ; i <= draw.dc ; i++) {
                    draw_dot(x - (uint16_t)((((draw.d1 * i) / draw.dc) + 1) >> 1), y++);
                }
                break;
        }
    }
}

void PC98_GDC_state::text(void) {
    prepare();

    uint8_t multiple = draw.zoom + 1;
    uint8_t dir = ((draw.ope & 0x80) >> 4) | draw.dir;
    uint8_t mulx;
    uint8_t muly;
    uint8_t bit;
    uint16_t cx;
    uint16_t cy;
    uint16_t xrem;
    uint16_t patnum = 0;
    uint16_t sx = draw.d;
    uint16_t sy = draw.dc + 1;

    draw.pattern = 0xffff;
    while(sy--) {
        muly = multiple;
        patnum--;
        while(muly--) {
            cx = draw.x;
            cy = draw.y;
            bit = draw.tx[patnum & 7];
            xrem = sx;
            while(xrem--) {
                mulx = multiple;
                if (bit & 1) {
                    bit >>= 1;
                    bit |= 0x80;
                    while(mulx--) {
                        draw_dot(cx, cy);
                        cx += vectdir[dir].x;
                        cy += vectdir[dir].y;
                    }
                } else {
                    bit >>= 1;
                    while(mulx--) {
                        cx += vectdir[dir].x;
                        cy += vectdir[dir].y;
                    }
                }
            }
            draw.x += vectdir[dir].x2;
            draw.y += vectdir[dir].y2;
        }
    }
}

void PC98_GDC_state::circle(void) {
    uint32_t m;

    prepare();
    if(gdc_rt[RT_TABLEMAX] == 0) {
        for(int i = 0 ; i <= RT_TABLEMAX ; i++) {
            /* NTS: 1 / sqrt(2.0) == 0.70710678118654 */
            gdc_rt[i] = (uint16_t)((double)(1 << RT_MULBIT) * (1 - sqrt(1 - pow((0.70710678118654 * i) / RT_TABLEMAX, 2))));
        }
    }

    m = ((uint32_t)draw.d * 10000 + 14141) / 14142;
    if(m == 0) {
        draw_dot(draw.x, draw.y);
    } else {
        uint32_t s;
        uint16_t i = draw.dm;
        uint32_t t = draw.dc;
        uint16_t x = draw.x;
        uint16_t y = draw.y;
        if(t > m) {
            t = m;
        }
        switch(draw.dir) {
            case 0:
                while(i <= t) {
                    s = (gdc_rt[(i << RT_TABLEBIT) / m] * draw.d);
                    s = (s + (1 << (RT_MULBIT - 1))) >> RT_MULBIT;
                    draw_dot((uint16_t)(x + s), (uint16_t)(y + i));
                    i++;
                }
                break;
            case 1:
                while(i <= t) {
                    s = (gdc_rt[(i << RT_TABLEBIT) / m] * draw.d);
                    s = (s + (1 << (RT_MULBIT - 1))) >> RT_MULBIT;
                    draw_dot((uint16_t)(x + i), (uint16_t)(y + s));
                    i++;
                }
                break;
            case 2:
                while(i <= t) {
                    s = (gdc_rt[(i << RT_TABLEBIT) / m] * draw.d);
                    s = (s + (1 << (RT_MULBIT - 1))) >> RT_MULBIT;
                    draw_dot((uint16_t)(x + i), (uint16_t)(y - s));
                    i++;
                }
                break;
            case 3:
                while(i <= t) {
                    s = (gdc_rt[(i << RT_TABLEBIT) / m] * draw.d);
                    s = (s + (1 << (RT_MULBIT - 1))) >> RT_MULBIT;
                    draw_dot((uint16_t)(x + s), (uint16_t)(y - i));
                    i++;
                }
                break;
            case 4:
                while(i <= t) {
                    s = (gdc_rt[(i << RT_TABLEBIT) / m] * draw.d);
                    s = (s + (1 << (RT_MULBIT - 1))) >> RT_MULBIT;
                    draw_dot((uint16_t)(x - s), (uint16_t)(y - i));
                    i++;
                }
                break;
            case 5:
                while(i <= t) {
                    s = (gdc_rt[(i << RT_TABLEBIT) / m] * draw.d);
                    s = (s + (1 << (RT_MULBIT - 1))) >> RT_MULBIT;
                    draw_dot((uint16_t)(x - i), (uint16_t)(y - s));
                    i++;
                }
                break;
            case 6:
                while(i <= t) {
                    s = (gdc_rt[(i << RT_TABLEBIT) / m] * draw.d);
                    s = (s + (1 << (RT_MULBIT - 1))) >> RT_MULBIT;
                    draw_dot((uint16_t)(x - i), (uint16_t)(y + s));
                    i++;
                }
                break;
            case 7:
                while(i <= t) {
                    s = (gdc_rt[(i << RT_TABLEBIT) / m] * draw.d);
                    s = (s + (1 << (RT_MULBIT - 1))) >> RT_MULBIT;
                    draw_dot((uint16_t)(x - s), (uint16_t)(y + i));
                    i++;
                }
                break;
        }
    }
}

void PC98_GDC_state::box(void) {
    prepare();

    uint16_t i;
    uint16_t x = draw.x;
    uint16_t y = draw.y;
    for(i = 0 ; i < draw.d ; i++) {
        draw_dot(x, y);
        x += vectdir[draw.dir].x;
        y += vectdir[draw.dir].y;
    }
    for(i = 0 ; i < draw.d2 ; i++) {
        draw_dot(x, y);
        x += vectdir[draw.dir].x2;
        y += vectdir[draw.dir].y2;
    }
    for(i = 0 ; i < draw.d ; i++) {
        draw_dot(x, y);
        x -= vectdir[draw.dir].x;
        y -= vectdir[draw.dir].y;
    }
    for(i = 0 ; i < draw.d2 ; i++) {
        draw_dot(x, y);
        x -= vectdir[draw.dir].x2;
        y -= vectdir[draw.dir].y2;
    }
}

void PC98_GDC_state::exec(uint8_t command) {
    switch(draw.ope & 0xf8) {
        case 0x00:
            pset();
            break;
        case 0x08:
            line();
            break;
        case 0x10:
        case 0x90:
            if(command == GDC_CMD_TEXTE) {
                text();
            }
            break;
        case 0x20:
            circle();
            break;
        case 0x40:
            box();
            break;
        default:
            break;
    }
    draw_reset();
}
