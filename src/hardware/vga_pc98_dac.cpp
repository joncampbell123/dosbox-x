
#include "dosbox.h"
#include "setup.h"
#include "video.h"
#include "pic.h"
#include "vga.h"
#include "inout.h"
#include "programs.h"
#include "support.h"
#include "setup.h"
#include "timer.h"
#include "mem.h"
#include "util_units.h"
#include "control.h"
#include "pc98_cg.h"
#include "pc98_dac.h"
#include "pc98_gdc.h"
#include "pc98_gdc_const.h"
#include "mixer.h"

#include <string.h>
#include <stdlib.h>
#include <string>
#include <stdio.h>

using namespace std;

uint32_t                    pc98_text_palette[8];
uint8_t                     pc98_16col_analog_rgb_palette_index = 0;

uint8_t                     pc98_pal_analog[256*3]; /* G R B    0x0..0xF */
uint8_t                     pc98_pal_digital[8];    /* G R B    0x0..0x7 */

void pc98_update_palette(void) {
    if (pc98_gdc_vramop & (1 << VOPBIT_ANALOG)) {
        for (unsigned int i=0;i < 16;i++) {
            vga.dac.rgb[i].green = dac_4to6(pc98_pal_analog[(3*i) + 0]&0xF); /* re-use VGA DAC */
            vga.dac.rgb[i].red   = dac_4to6(pc98_pal_analog[(3*i) + 1]&0xF); /* re-use VGA DAC */
            vga.dac.rgb[i].blue  = dac_4to6(pc98_pal_analog[(3*i) + 2]&0xF); /* re-use VGA DAC */
            VGA_DAC_UpdateColor(i);
        }
    }
    else {
        for (unsigned int i=0;i < 8;i++) {
            pc98_update_digpal(i);
            VGA_DAC_UpdateColor(i);
        }
    }
}

void pc98_update_digpal(unsigned char ent) {
    unsigned char grb = pc98_pal_digital[ent];

    vga.dac.rgb[ent].green = (grb & 4) ? 0x3F : 0x00;
    vga.dac.rgb[ent].red =   (grb & 2) ? 0x3F : 0x00;
    vga.dac.rgb[ent].blue =  (grb & 1) ? 0x3F : 0x00;
    VGA_DAC_UpdateColor(ent);
}

void pc98_set_digpal_entry(unsigned char ent,unsigned char grb) {
    pc98_pal_digital[ent] = grb;

    if (!gdc_analog)
        pc98_update_digpal(ent);
}

void pc98_set_digpal_pair(unsigned char start,unsigned char pair) {
    /* assume start 0..3 */
    pc98_set_digpal_entry(start,  pair >> 4);
    pc98_set_digpal_entry(start+4,pair & 0xF);
}

unsigned char pc98_get_digpal_pair(unsigned char start) {
    return (pc98_pal_digital[start] << 4) + pc98_pal_digital[start+4];
}

