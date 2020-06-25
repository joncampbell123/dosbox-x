/*
 *  Hq2x scaler pixel shader version support code by Mitja Gros (Mitja.Gros@gmail.com)
 *
 *  Original OpenGL-HQ rendering code
 *  Copyright (C) 2004-2005 Jorg Walter <jwalt@garni.ch>
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

#include "hq2x_d3d.h"

#if C_D3DSHADERS

#define fmax(x,y) ((x)>(y)?(x):(y))
#define fmin(x,y) ((x)<(y)?(x):(y))
#define R 1
#define T 2
#define RT 4
#define RT2 8
#define L 16
#define LB2 32
#define LT2 64
#define LT 128
#define LB 256
#define B 512
#define RB2 1024
#define RB 2048

#define NODIAG 0x90
#define H 1
#define V 2
#define D 4

#define hmirror(p) swap_bits(swap_bits(swap_bits(swap_bits(swap_bits(p,R,L),RT,LT),RT2,LT2),RB,LB),RB2,LB2)
#define vmirror(p) swap_bits(swap_bits(swap_bits(swap_bits(swap_bits(p,T,B),RT,RB),RT2,RB2),LT,LB),LT2,LB2)
#define NO_BORDER(x) ((b&(x)) == 0)
#define IS_BORDER(x) ((b&(x)) == (x))
#define SETINTERP(percentage_inside) setinterp(xcenter,ycenter,percentage_inside, \
                        NO_BORDER(R),NO_BORDER(T),NO_BORDER(RT), \
                        IS_BORDER(R),IS_BORDER(T),IS_BORDER(RT), \
                        texture+((x+(border%16)*HQ2X_RESOLUTION+y*16*HQ2X_RESOLUTION+(border&~15)*HQ2X_RESOLUTION*HQ2X_RESOLUTION)*4))

static double sign(double a) {
    return (a < 0?-1:1);
}

/*
 This function calculates what percentage of a rectangle intersected by a line lies near the center of the
 cordinate system. It is mathematically exact, and well-tested for xcenter > 0 and ycenter > 0 (it's only
 used that way). It should be correct for other cases as well, but well... famous last words :)
*/
static double intersect_any(double xcenter, double ycenter, double xsize, double ysize, double yoffset, double gradient) {
    double g = fabs(gradient)*xsize/ysize;
    double o = -((yoffset-ycenter) + gradient*xcenter)/ysize*sign(ycenter)*sign(yoffset)-g*0.5+0.5;
    double yl = o, yr = o+g, xb = -o/g, xt = (1-o)/g;
    double area = 1.0;

    if (yl >= 1.0) xt = xb = area = 0.0;
    else if (yl > 0.0) {
        area = 1.0-yl;
        xb = 0.0;
    }
    else if (yr <= 0.0) yl = yr = area = 1.0;
    else yl = o+xb*g;

    if (xt <= 0.0) yr = yl = area = 0.0;
    else if (xt < 1.0) {
        area *= xt;
        yr = 1.0;
    }
    else if (xb >= 1.0) xb = xt = area = 1.0;
    else xt = (yr-o)/g;

    area -= (xt-xb)*(yr-yl)/2;

    return area;
}

static double intersect_h(double xcenter, double ycenter, double xsize, double ysize) {
    (void)ycenter;//UNUSED
    (void)ysize;
    return fmax(0.0,fmin(1.0,(.55-fabs(xcenter)+xsize/2.0)/xsize));
}

static double intersect_any_h(double xcenter, double ycenter, double xsize, double ysize, double yoffset, double gradient) {
    double hinside = intersect_h(xcenter,ycenter,xsize,ysize);
    return hinside*hinside*intersect_any(xcenter,ycenter,xsize,ysize,yoffset,gradient);
}

static double intersect_v(double xcenter, double ycenter, double xsize, double ysize) {
    (void)xsize;
    (void)ysize;
    (void)xcenter;
    return fmax(0.0,fmin(1.0,(.55-fabs(ycenter)+ysize/2.0)/ysize));
}

static double intersect_any_v(double xcenter, double ycenter, double xsize, double ysize, double yoffset, double gradient) {
    double vinside = intersect_v(xcenter,ycenter,xsize,ysize);
    return vinside*vinside*intersect_any(xcenter,ycenter,xsize,ysize,yoffset,gradient);
}

static double intersect_hv(double xcenter, double ycenter, double xsize, double ysize) {
    double hinside = intersect_h(xcenter,ycenter,xsize,ysize);
    double vinside = intersect_v(xcenter,ycenter,xsize,ysize);
    return (1-hinside)*(1-vinside)+hinside*vinside;
}

/* FIXME: not sure if this is correct, but it is rare enough and most likely near enough. fixes welcome :) */
static double intersect_any_hv(double xcenter, double ycenter, double xsize, double ysize, double yoffset, double gradient) {
    double hvinside = intersect_hv(xcenter,ycenter,xsize,ysize);
    return hvinside*hvinside*intersect_any(xcenter,ycenter,xsize,ysize,yoffset,gradient);
}

static double intersect_hvd(double xcenter, double ycenter, double xsize, double ysize) {
    return intersect_h(xcenter,ycenter,xsize,ysize)*intersect_v(xcenter,ycenter,xsize,ysize);
}

static void setinterp(double xcenter, double ycenter, double percentage_inside, int i1, int i2, int i3, int o1, int o2, int o3, unsigned char *factors) {
    double d0, d1, d2, d3, percentage_outside, totaldistance_i, totaldistance_o;
    xcenter = fabs(xcenter);
    ycenter = fabs(ycenter);
    d0 = (1-xcenter)*(1-ycenter);
    d1 = xcenter*(1-ycenter);
    d2 = (1-xcenter)*ycenter;
    d3 = xcenter*ycenter;
    if (i1 && i2) i3 = 0;
    if (o1 && o2) o3 = 0;
    percentage_outside = 1.0-percentage_inside;
    totaldistance_i = d0+i1*d1+i2*d2+i3*d3;
    totaldistance_o = o1*d1+o2*d2+o3*d3+1e-12; /* +1e-12: prevent division by zero */

    factors[1] = (unsigned char)(((d1/totaldistance_i*percentage_inside*i1)+(d1/totaldistance_o*percentage_outside*o1))*255+.5);
    factors[2] = (unsigned char)(((d2/totaldistance_i*percentage_inside*i2)+(d2/totaldistance_o*percentage_outside*o2))*255+.5);
    factors[3] = (unsigned char)(((d3/totaldistance_i*percentage_inside*i3)+(d3/totaldistance_o*percentage_outside*o3))*255+.5);
    factors[0] = 255-factors[1]-factors[2]-factors[3];/*(unsigned char)((d0/totaldistance_i*percentage_inside)*255+.5);*/
}

/* Wanna have gcc fun? #define this as a macro, get a fast machine and go fetch a coffe or two. See how it is used to get an idea why.
   I aborted compilation after 5 minutes of CPU time on an Athlon64 3700+. */
static int swap_bits(int num, int bit1, int bit2) {
    return ((num & ~(bit1|bit2))|((num&bit1)?bit2:0)|((num&bit2)?bit1:0));
}


// width, height == rwidth, rheight
// outwidth, outheight == width, height
void BuildHq2xLookupTexture(Bitu outWidth, Bitu outHeight, Bitu rwidth, Bitu rheight, Bit8u* texture)
{
    double xsize, ysize;
    unsigned char table[4096] = HQ2X_D3D_TABLE_DATA;

    xsize = (double)rwidth / (double)outWidth;
    ysize = (double)rheight / (double)outHeight;

    for (int border = 0; border < 4096; border++) {
	for (int y = 0; y < HQ2X_RESOLUTION; y++) {
            for (int x = 0; x < HQ2X_RESOLUTION; x++) {
                double xcenter = fabs((((double)x)+0.5) / (double)(HQ2X_RESOLUTION)-0.5)/0.958;
                double ycenter = fabs((((double)y)+0.5) / (double)(HQ2X_RESOLUTION)-0.5)/0.958;
                int sx = (x < HQ2X_RESOLUTION/2?-1:1);
                int sy = (y < HQ2X_RESOLUTION/2?-1:1);
                int b = (sy > 0?(sx > 0?border:hmirror(border)):(sx > 0?vmirror(border):vmirror(hmirror(border))));

                if ((table[b] & NODIAG) == NODIAG) {
                    if (table[b] & H) {
                        if (table[b] & V) {
                            if (table[b] & D) SETINTERP(intersect_hvd(xcenter,ycenter,xsize,ysize));
                            else SETINTERP(intersect_hv(xcenter,ycenter,xsize,ysize));
                        } else {
                            SETINTERP(intersect_h(xcenter,ycenter,xsize,ysize));
                        }
                    } else if (table[b] & V) {
                        SETINTERP(intersect_v(xcenter,ycenter,xsize,ysize));
                    } else {
                        SETINTERP(1.0);
                    }
                } else {
                    double yoff = (table[b]&4?1:-1)*(((table[b] >> 3) & 3) + 1)/4.0;
                    double grad = (table[b]&32?1:-1)*(((table[b] >> 6) & 3) + 1)/2.0;
                    if (table[b] & H) {
                        if (table[b] & V) {
                            SETINTERP(intersect_any_hv(xcenter,ycenter,xsize,ysize,yoff,grad));
                        } else {
                            SETINTERP(intersect_any_h(xcenter,ycenter,xsize,ysize,yoff,grad));
                        }
                    } else if (table[b] & V) {
                        SETINTERP(intersect_any_v(xcenter,ycenter,xsize,ysize,yoff,grad));
                    } else {
                        SETINTERP(intersect_any(xcenter,ycenter,xsize,ysize,yoff,grad));
                    }
                }

            }
        }
    }
}

#endif
