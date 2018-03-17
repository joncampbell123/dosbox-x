
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

extern bool                 pc98_graphics_hide_odd_raster_200line;

bool                        gdc_5mhz_mode = false;
bool                        enable_pc98_egc = false;
bool                        GDC_vsync_interrupt = false;
uint8_t                     GDC_display_plane_wait_for_vsync = false;
uint8_t                     GDC_display_plane_pending = false;
uint8_t                     GDC_display_plane = false;

uint8_t                     pc98_gdc_tile_counter=0;
uint8_t                     pc98_gdc_modereg=0;
uint8_t                     pc98_gdc_vramop=0;
egc_quad                    pc98_gdc_tiles;

void pc98_crtc_write(Bitu port,Bitu val,Bitu iolen) {
    switch (port&0xE) {
        case 0x0C:      // 0x7C: mode reg / vram operation mode (also, reset tile counter)
            pc98_gdc_tile_counter = 0;
            pc98_gdc_modereg = val;
            pc98_gdc_vramop &= ~(3 << VOPBIT_GRCG);
            pc98_gdc_vramop |= (val & 0xC0) >> (6 - VOPBIT_GRCG);
            break;
        case 0x0E:      // 0x7E: tile data
            pc98_gdc_tiles[pc98_gdc_tile_counter].b[0] = val;
            pc98_gdc_tiles[pc98_gdc_tile_counter].b[1] = val;
            pc98_gdc_tile_counter = (pc98_gdc_tile_counter + 1) & 3;
            break;
        default:
            LOG_MSG("PC98 CRTC w: port=0x%02X val=0x%02X unknown",(unsigned int)port,(unsigned int)val);
            break;
    };
}

Bitu pc98_crtc_read(Bitu port,Bitu iolen) {
    LOG_MSG("PC98 CRTC r: port=0x%02X unknown",(unsigned int)port);
    return ~0;
}

/* Port 0x6A command handling */
void pc98_port6A_command_write(unsigned char b) {
    switch (b) {
        case 0x00: // 16-color (analog) disable
            gdc_analog = false;
            pc98_gdc_vramop &= ~(1 << VOPBIT_ANALOG);
            VGA_SetupHandlers();   // confirmed on real hardware: this disables access to E000:0000
            pc98_update_palette(); // Testing on real hardware shows that the "digital" and "analog" palettes are completely different.
                                   // They're both there in hardware, but one or another is active depending on analog enable.
                                   // Also, the 4th bitplane at E000:0000 disappears when switched off from the display and from CPU access.
            break;
        case 0x01: // or enable
            gdc_analog = true;
            pc98_gdc_vramop |= (1 << VOPBIT_ANALOG);
            VGA_SetupHandlers();   // confirmed on real hardware: this enables access to E000:0000
            pc98_update_palette(); // Testing on real hardware shows that the "digital" and "analog" palettes are completely different.
                                   // They're both there in hardware, but one or another is active depending on analog enable.
                                   // Also, the 4th bitplane at E000:0000 disappears when switched off from the display and from CPU access.
            break;
        case 0x04:
            pc98_gdc_vramop &= ~(1 << VOPBIT_EGC);
            break;
        case 0x05:
            if (enable_pc98_egc) pc98_gdc_vramop |= (1 << VOPBIT_EGC);
            break;
        case 0x06: // TODO
        case 0x07: // TODO
            // TODO
            break;
        case 0x0A: // TODO
        case 0x0B: // TODO
            // TODO
            break;
        default:
            LOG_MSG("PC-98 port 6Ah unknown command 0x%02x",b);
            break;
    };
}

/* Port 0x68 command handling */
void pc98_port68_command_write(unsigned char b) {
    switch (b) {
        case 0x08: // 200-line mode: show odd raster
        case 0x09: //                don't show odd raster
            pc98_graphics_hide_odd_raster_200line = !!(b&1);
            break;
        case 0x0A: // TODO
        case 0x0B: // TODO
            // TODO
            break;
        default:
            LOG_MSG("PC-98 port 68h unknown command 0x%02x",b);
            break;
    };
}

