/*
 *  Copyright (C) 2018-2020  Jon Campbell
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

extern bool                 gdc_5mhz_mode;
extern bool                 gdc_5mhz_mode_initial;
extern bool                 GDC_vsync_interrupt;
extern bool                 pc98_256kb_boundary;
extern uint8_t              GDC_display_plane;
extern uint8_t              GDC_display_plane_pending;
extern uint8_t              GDC_display_plane_wait_for_vsync;

double                      gdc_proc_delay = 0.001; /* time from FIFO to processing in GDC (1us) FIXME: Is this right? */
bool                        gdc_proc_delay_set = false;
struct PC98_GDC_state       pc98_gdc[2];

void pc98_update_display_page_ptr(void) {
    if (pc98_256kb_boundary) {
        /* GDC display plane has no effect in 256KB boundary mode */
        pc98_pgraph_current_display_page = vga.mem.linear +
            PC98_VRAM_GRAPHICS_OFFSET;
    }
    else {
        if (pc98_gdc_vramop & (1 << VOPBIT_VGA)) {
            pc98_pgraph_current_display_page = vga.mem.linear +
                PC98_VRAM_GRAPHICS_OFFSET +
                (GDC_display_plane * PC98_VRAM_PAGEFLIP256_SIZE);
        }
        else {
            pc98_pgraph_current_display_page = vga.mem.linear +
                PC98_VRAM_GRAPHICS_OFFSET +
                (GDC_display_plane * PC98_VRAM_PAGEFLIP_SIZE);
        }
    }
}

void pc98_update_cpu_page_ptr(void) {
    if (pc98_gdc_vramop & (1 << VOPBIT_VGA)) {
        /* "Drawing screen selection register" is not valid in extended modes
         * [http://hackipedia.org/browse.cgi/Computer/Platform/PC%2c%20NEC%20PC%2d98/Collections/Undocumented%209801%2c%209821%20Volume%202%20%28webtech.co.jp%29/io%5fdisp%2etxt] */
        pc98_pgraph_current_cpu_page = vga.mem.linear +
            PC98_VRAM_GRAPHICS_OFFSET;
    }
    else {
        pc98_pgraph_current_cpu_page = vga.mem.linear +
            PC98_VRAM_GRAPHICS_OFFSET +
            ((pc98_gdc_vramop & (1 << VOPBIT_ACCESS)) ? PC98_VRAM_PAGEFLIP_SIZE : 0);
    }
}

void pc98_update_page_ptrs(void) {
    pc98_update_display_page_ptr();
    pc98_update_cpu_page_ptr();
}

void gdc_proc_schedule_delay(void);
void gdc_proc_schedule_cancel(void);
void gdc_proc_schedule_done(void);
void GDC_ProcDelay(Bitu /*val*/);
void PC98_show_cursor(bool show);
void pc98_port6A_command_write(unsigned char b);
void pc98_port68_command_write(unsigned char b);

PC98_GDC_state::PC98_GDC_state() {
    memset(param_ram,0,sizeof(param_ram));
    memset(cmd_parm_tmp, 0, sizeof(cmd_parm_tmp));
    memset(rfifo, 0, sizeof(rfifo));
    memset(fifo, 0, sizeof(fifo));

    // make a display partition area to cover the screen, whatever it is.
    param_ram[0] = 0x00;        // SAD=0
    param_ram[1] = 0x00;        // SAD=0
    param_ram[2] = 0xF0;        // LEN=3FF
    param_ram[3] = 0x3F;        // LEN=3FF WD1=0

    IM_bit = false;
    display_partition_mask = 3;
    doublescan = false;
    param_ram_wptr = 0;
    display_partition = 0;
    display_partition_rem_lines = 0;
    active_display_lines = 0;
    active_display_words_per_line = 0;
    row_line = 0;
    row_height = 16;
    scan_address = 0;
    current_command = 0xFF;
    proc_step = 0xFF;
    display_enable = true;
    display_mode = 0;
    display_pitch = 0;
    cursor_enable = true;
    cursor_blink_state = 0;
    cursor_blink_count = 0;
    cursor_blink_rate = 0x20;
    video_framing = 0;
    master_sync = false;
    draw_only_during_retrace = 0;
    dynamic_ram_refresh = 0;
    cursor_blink = true;
    idle = false;
    horizontal_sync_width = 0;
    vertical_sync_width = 0;
    horizontal_front_porch_width = 0;
    horizontal_back_porch_width = 0;
    vertical_front_porch_width = 0;
    vertical_back_porch_width = 0;
    reset_fifo();
    reset_rfifo();
}

size_t PC98_GDC_state::fifo_can_read(void) {
    return (size_t)((unsigned int)fifo_write - (unsigned int)fifo_read);
}

void PC98_GDC_state::take_reset_sync_parameters(void) {
    /* P1 = param[0] = 0 0 C F I D G S
     *  CG = [1:0] = display mode
     *  IS = [1:0] = video framing
     *   F = drawing time window
     *   D = dynamic RAM refresh cycles enable */
    draw_only_during_retrace =      !!(cmd_parm_tmp[0] & 0x10); /* F */
    dynamic_ram_refresh =           !!(cmd_parm_tmp[0] & 0x04); /* D */
    display_mode = /* CG = [1:0] */
        ((cmd_parm_tmp[0] & 0x20) ? 2 : 0) +
        ((cmd_parm_tmp[0] & 0x02) ? 1 : 0);
    video_framing = /* IS = [1:0] */
        ((cmd_parm_tmp[0] & 0x08) ? 2 : 0) +
        ((cmd_parm_tmp[0] & 0x01) ? 1 : 0);

    /* P2 = param[1] = AW = active display words per line - 2. must be even number. */
    active_display_words_per_line = (uint16_t)cmd_parm_tmp[1] + 2u;

    /* P3 = param[2] =
     *   VS(L)[2:0] = [7:5] = low bits of VS
     *   HS = [4:0] = horizontal sync width - 1 */
    horizontal_sync_width = (cmd_parm_tmp[2] & 0x1F) + 1;
    vertical_sync_width = (cmd_parm_tmp[2] >> 5);

    /* P4 = param[3] =
     *   HFP = [7:2] = horizontal front porch width - 1
     *   VS(H)[4:3] = [1:0] = high bits of VS
     *
     *   VS = vertical sync width */
    vertical_sync_width += (cmd_parm_tmp[3] & 3) << 3;
    horizontal_front_porch_width = (cmd_parm_tmp[3] >> 2) + 1;

    /* P5 = param[4] =
     *   0 = [7:6] = 0
     *   HBP = [5:0] = horizontal back porch width - 1 */
    horizontal_back_porch_width = (cmd_parm_tmp[4] & 0x3F) + 1;

    /* P6 = param[5] =
     *   0 = [7:6] = 0
     *   VFP = [5:0] = vertical front porch width */
    vertical_front_porch_width = (cmd_parm_tmp[5] & 0x3F);

    /* P7 = param[6] =
     *   AL(L)[7:0] = [7:0] = Active Display Lines per video field, low bits */
    active_display_lines = (cmd_parm_tmp[6] & 0xFF);

    /* P8 = parm[7] =
     *   VBP = [7:2] = vertical back porch width
     *   AL(H)[9:8] = [1:0] = Active Display Lines per video field, high bits */
    active_display_lines += (cmd_parm_tmp[7] & 3) << 8;
    vertical_back_porch_width = cmd_parm_tmp[7] >> 2;

    LOG_MSG("GDC: RESET/SYNC MASTER=%u DOOR=%u DRAM=%u DISP=%u VFRAME=%u AW=%u HS=%u VS=%u HFP=%u HBP=%u VFP=%u AL=%u VBP=%u",
        master_sync,
        draw_only_during_retrace?1:0,
        dynamic_ram_refresh?1:0,
        display_mode,
        video_framing,
        active_display_words_per_line,
        horizontal_sync_width,
        vertical_sync_width,
        horizontal_front_porch_width,
        horizontal_back_porch_width,
        vertical_front_porch_width,
        active_display_lines,
        vertical_back_porch_width);

    VGA_StartResize();
}

void PC98_GDC_state::cursor_advance(void) {
    cursor_blink_count++;
    if (cursor_blink_count == cursor_blink_rate) {
        cursor_blink_count = 0;

        if ((++cursor_blink_state) >= 4)
            cursor_blink_state = 0;
    }
    else if (cursor_blink_count & 0x40) {
        cursor_blink_count = 0;
    }
}

void PC98_GDC_state::take_cursor_pos(unsigned char bi) {
    /* P1 = param[0] = EAD(L) = address[7:0]
     *
     * P2 = param[1] = EAD(M) = address[15:0]
     *
     * P3 = param[2]
     *   dAD = [7:4] = Dot address within the word
     *   0 = [3:2] = 0
     *   EAD(H) = [1:0] = address[17:16] */
    if (bi == 1) {
		vga.config.cursor_start &= ~(0xFFu << 0u);
		vga.config.cursor_start |=  cmd_parm_tmp[0] << 0u;
    }
    else if (bi == 2) {
		vga.config.cursor_start &= ~(0xFFu << 8u);
		vga.config.cursor_start |=  (unsigned int)cmd_parm_tmp[1] << 8u;
    }
    else if (bi == 3) {
		vga.config.cursor_start &= ~(0x03u << 16u);
		vga.config.cursor_start |=  (cmd_parm_tmp[2] & 3u) << 16u;

        // TODO: "dot address within the word"
    }
}

void PC98_GDC_state::take_cursor_char_setup(unsigned char bi) {
    /* P1 = param[0] =
     *   DC = [7:7] = display cursor if set
     *   0 = [6:5] = 0
     *   LR = [4:0] = lines per character row - 1 */
    if (bi == 1) {
        cursor_enable = !!(cmd_parm_tmp[0] & 0x80);

		vga.crtc.maximum_scan_line = cmd_parm_tmp[0] & 0x1F;
		vga.draw.address_line_total = vga.crtc.maximum_scan_line + 1u;
        row_height = (uint8_t)vga.draw.address_line_total;
        if (!master_sync) doublescan = (row_height > 1);
    }

    /* P2 = param[1] =
     *   BR[1:0] = [7:6] = blink rate
     *   SC = [5:5] = 1=steady cursor  0=blinking cursor
     *   CTOP = [4:0] = cursor top line number in the row */

    /* P3 = param[2] =
     *   CBOT = [7:3] = cursor bottom line number in the row CBOT < LR
     *   BR[4:2] = [2:0] = blink rate */
    if (bi == 3) {
        cursor_blink_rate  = (cmd_parm_tmp[1] >> 6) & 3;
        cursor_blink_rate += (cmd_parm_tmp[2] & 7) << 2;
        if (cursor_blink_rate == 0) cursor_blink_rate = 0x20;
        cursor_blink_rate *= 2;

        cursor_blink = !(cmd_parm_tmp[1] & 0x20);

		vga.crtc.cursor_start = (cmd_parm_tmp[1] & 0x1F);
		vga.draw.cursor.sline = vga.crtc.cursor_start;

		vga.crtc.cursor_end   = (cmd_parm_tmp[2] >> 3) & 0x1F;
		vga.draw.cursor.eline = vga.crtc.cursor_end;
    }

    /* blink-on time + blink-off time = 2 x BR (video frames).
     * attribute blink rate is 3/4 on 1/4 off duty cycle.
     * for interlaced graphics modes, set BR[1:0] = 3 */
}

void PC98_GDC_state::idle_proc(void) {
    uint16_t val;

    if (fifo_empty())
        return;

    val = read_fifo();
    if (val & 0x100) { // command
        current_command = val & 0xFF;
        proc_step = 0;

        switch (current_command) {
            case GDC_CMD_RESET: // 0x00         0 0 0 0 0 0 0 0
                LOG_MSG("GDC: reset");
                display_enable = false;
                idle = true;
                reset_fifo();
                reset_rfifo();
                break;
            case GDC_CMD_DISPLAY_BLANK:  // 0x0C   0 0 0 0 1 1 0 DE
            case GDC_CMD_DISPLAY_BLANK+1:// 0x0D   DE=display enable
                display_enable = !!(current_command & 1); // bit 0 = display enable
                current_command &= ~1;
                break;
            case GDC_CMD_SYNC:  // 0x0E         0 0 0 0 0 0 0 DE
            case GDC_CMD_SYNC+1:// 0x0F         DE=display enable
                display_enable = !!(current_command & 1); // bit 0 = display enable
                current_command &= ~1;
                LOG_MSG("GDC: sync");
                break;
            case GDC_CMD_PITCH_SPEC:          // 0x47        0 1 0 0 0 1 1 1
                break;
            case GDC_CMD_CURSOR_POSITION:     // 0x49        0 1 0 0 1 0 0 1
//              LOG_MSG("GDC: cursor pos");
                break;
            case GDC_CMD_CURSOR_CHAR_SETUP:   // 0x4B        0 1 0 0 1 0 1 1
//              LOG_MSG("GDC: cursor setup");
                break;
            case GDC_CMD_START_DISPLAY:       // 0x6B        0 1 1 0 1 0 1 1
                display_enable = true;
                idle = false;
                break;
            case GDC_CMD_VERTICAL_SYNC_MODE:  // 0x6E        0 1 1 0 1 1 1 M
            case GDC_CMD_VERTICAL_SYNC_MODE+1:// 0x6F        M=generate and output vertical sync (0=or else accept external vsync)
                master_sync = !!(current_command & 1);
                current_command &= ~1;
                LOG_MSG("GDC: vsyncmode master=%u",master_sync);
                break;
            case GDC_CMD_PARAMETER_RAM_LOAD:   // 0x70       0 1 1 1 S S S S
            case GDC_CMD_PARAMETER_RAM_LOAD+1: // 0x71       S=starting byte in parameter RAM
            case GDC_CMD_PARAMETER_RAM_LOAD+2: // 0x72       S=starting byte in parameter RAM
            case GDC_CMD_PARAMETER_RAM_LOAD+3: // 0x73       S=starting byte in parameter RAM
            case GDC_CMD_PARAMETER_RAM_LOAD+4: // 0x74       S=starting byte in parameter RAM
            case GDC_CMD_PARAMETER_RAM_LOAD+5: // 0x75       S=starting byte in parameter RAM
            case GDC_CMD_PARAMETER_RAM_LOAD+6: // 0x76       S=starting byte in parameter RAM
            case GDC_CMD_PARAMETER_RAM_LOAD+7: // 0x77       S=starting byte in parameter RAM
            case GDC_CMD_PARAMETER_RAM_LOAD+8: // 0x78       S=starting byte in parameter RAM
            case GDC_CMD_PARAMETER_RAM_LOAD+9: // 0x79       S=starting byte in parameter RAM
            case GDC_CMD_PARAMETER_RAM_LOAD+10:// 0x7A       S=starting byte in parameter RAM
            case GDC_CMD_PARAMETER_RAM_LOAD+11:// 0x7B       S=starting byte in parameter RAM
            case GDC_CMD_PARAMETER_RAM_LOAD+12:// 0x7C       S=starting byte in parameter RAM
            case GDC_CMD_PARAMETER_RAM_LOAD+13:// 0x7D       S=starting byte in parameter RAM
            case GDC_CMD_PARAMETER_RAM_LOAD+14:// 0x7E       S=starting byte in parameter RAM
            case GDC_CMD_PARAMETER_RAM_LOAD+15:// 0x7F       S=starting byte in parameter RAM
                param_ram_wptr = current_command & 0xF;
                current_command = GDC_CMD_PARAMETER_RAM_LOAD;
                break;
            case GDC_CMD_CURSOR_ADDRESS_READ:  // 0xE0       1 1 1 0 0 0 0 0
                write_rfifo((unsigned char)( vga.config.cursor_start         & 0xFFu));
                write_rfifo((unsigned char)((vga.config.cursor_start >>  8u) & 0xFFu));
                write_rfifo((unsigned char)((vga.config.cursor_start >> 16u) & 0xFFu));
                write_rfifo(0x00); // TODO
                write_rfifo(0x00); // TODO
                break;
            default:
                LOG_MSG("GDC: %s: Unknown command 0x%x",master_sync?"master":"slave",current_command);
                break;
        }
    }
    else {
        /* parameter parsing */
        switch (current_command) {
            /* RESET and SYNC take the same 8 byte parameters */
            case GDC_CMD_RESET:
            case GDC_CMD_SYNC:
                if (proc_step < 8) {
                    cmd_parm_tmp[proc_step] = (uint8_t)val;
                    if ((++proc_step) == 8) {
                        take_reset_sync_parameters();
                    }
                }
                break;
            case GDC_CMD_PITCH_SPEC:
                if (proc_step < 1)
                    display_pitch = (val != 0) ? val : 0x100;
                break;
            case GDC_CMD_CURSOR_POSITION:
                if (proc_step < 3) {
                    cmd_parm_tmp[proc_step++] = (uint8_t)val;
                    take_cursor_pos(proc_step);
                }
                break;
            case GDC_CMD_CURSOR_CHAR_SETUP:
                if (proc_step < 3) {
                    cmd_parm_tmp[proc_step++] = (uint8_t)val;
                    if (proc_step == 1 || proc_step == 3) {
                        take_cursor_char_setup(proc_step);
                    }
                }
                break;
            case GDC_CMD_PARAMETER_RAM_LOAD:
                param_ram[param_ram_wptr] = (uint8_t)val;
                if ((++param_ram_wptr) >= 16) param_ram_wptr = 0;
                break;
        }
    }

    if (!fifo_empty())
        gdc_proc_schedule_delay();
}

bool PC98_GDC_state::fifo_empty(void) {
    return (fifo_read >= fifo_write);
}

uint16_t PC98_GDC_state::read_fifo(void) {
    uint16_t val;

    val = fifo[fifo_read];
    if (fifo_read < fifo_write)
        fifo_read++;

    return val;
}

void PC98_GDC_state::next_line(void) {
    /* */

    row_line++;
    if (row_line == row_height || /*HACK! See comments!*/(!master_sync/*graphics layer*/ && (pc98_gdc_vramop & (1u << VOPBIT_VGA)))) {
        /* NTS: According to real PC-9821 hardware, doublescan is ignored entirely in 256-color mode.
         *      The bits are still there in the GDC, and doublescan comes right back when 256-color mode is switched off.
         *      Perhaps the hardware uses an entirely different address counting register during video raster?
         *      Forcing this case at all times for 256-color mode is meant to emulate that fact.
         *      This fixes issues with booting an HDI image of "Alone in the Dark" */
        scan_address += display_pitch >> (IM_bit ? 1u : 0u);
        row_line = 0;
    }
    else if (row_line & 0x20) {
        row_line = 0;
    }

    if (--display_partition_rem_lines == 0) {
        next_display_partition();
        load_display_partition();
    }
}

void PC98_GDC_state::begin_frame(void) {
    row_line = 0;
    scan_address = 0;
    display_partition = 0;

    /* the actual starting address is determined by the display partition in paramter RAM */
    load_display_partition();
}

void PC98_GDC_state::load_display_partition(void) {
    unsigned char *pram = param_ram + (display_partition * 4);

    scan_address  =  pram[0];
    scan_address +=  pram[1]         << 8;
    scan_address += (pram[2] & 0x03) << 16;

    display_partition_rem_lines  =  pram[2]         >> 4;
    display_partition_rem_lines += (pram[3] & 0x3F) << 4;
    if (display_partition_rem_lines == 0)
        display_partition_rem_lines = 0x400;

    if (master_sync) { /* character mode */
    /* RAM+0 = SAD1 (L)
     *
     * RAM+1 = 0 0 0 SAH1 (M) [4:0]
     *
     * RAM+2 = LEN1 (L) [7:4]  0 0 0 0
     *
     * RAM+3 = WD1 0 LEN1 (H) [5:0] */
        scan_address &= 0x1FFF;
        IM_bit = false;
    }
    else { /* graphics mode */
    /* RAM+0 = SAD1 (L)
     *
     * RAM+1 = SAH1 (M)
     *
     * RAM+2 = LEN1 (L) [7:4]  0 0   SAD1 (H) [1:0]
     *
     * RAM+3 = WD1 IM LEN1 (H) [5:0] */
        IM_bit = !!(pram[3] & 0x40); /* increment the address every other cycle if set, mixed text/graphics only */
    }
}

void PC98_GDC_state::force_fifo_complete(void) {
    while (!fifo_empty())
        idle_proc();
}

void PC98_GDC_state::next_display_partition(void) {
    display_partition = (display_partition + 1) & display_partition_mask;
}

void PC98_GDC_state::reset_fifo(void) {
    fifo_read = fifo_write = 0;
}

void PC98_GDC_state::reset_rfifo(void) {
    rfifo_read = rfifo_write = 0;
}

void PC98_GDC_state::flush_fifo_old(void) {
    if (fifo_read != 0) {
        unsigned int sz = (fifo_read <= fifo_write) ? ((unsigned int)fifo_write - (unsigned int)fifo_read) : 0u;

        for (unsigned int i=0;i < sz;i++)
            fifo[i] = fifo[i+fifo_read];

        fifo_read = 0;
        fifo_write = sz;
    }
}

bool PC98_GDC_state::write_rfifo(const uint8_t c) {
    if (rfifo_write >= PC98_GDC_FIFO_SIZE)
        return false;

    rfifo[rfifo_write++] = c;
    return true;
}

bool PC98_GDC_state::write_fifo(const uint16_t c) {
    if (fifo_write >= PC98_GDC_FIFO_SIZE)
        flush_fifo_old();
    if (fifo_write >= PC98_GDC_FIFO_SIZE)
        return false;

    fifo[fifo_write++] = c;
    gdc_proc_schedule_delay();
    return true;
}

bool PC98_GDC_state::write_fifo_command(const unsigned char c) {
    return write_fifo(c | GDC_COMMAND_BYTE);
}

bool PC98_GDC_state::write_fifo_param(const unsigned char c) {
    return write_fifo(c);
}

bool PC98_GDC_state::rfifo_has_content(void) {
    return (rfifo_read < rfifo_write);
}

uint8_t PC98_GDC_state::read_status(void) {
    double timeInFrame = PIC_FullIndex()-vga.draw.delay.framestart;
    double timeInLine=fmod(timeInFrame,vga.draw.delay.htotal);
    uint8_t ret;

    ret  = 0x00; // light pen not present

    if (timeInLine >= vga.draw.delay.hblkstart && 
        timeInLine <= vga.draw.delay.hblkend)
        ret |= 0x40; // horizontal blanking

    if (timeInFrame >= vga.draw.delay.vrstart &&
        timeInFrame <= vga.draw.delay.vrend)
        ret |= 0x20; // vertical retrace

    // TODO: 0x10 bit 4 DMA execute

    // TODO: 0x08 bit 3 drawing in progress

    if (fifo_write >= PC98_GDC_FIFO_SIZE)
        flush_fifo_old();

    if (fifo_read == fifo_write)
        ret |= 0x04; // FIFO empty
    if (fifo_write >= PC98_GDC_FIFO_SIZE)
        ret |= 0x02; // FIFO full
    if (rfifo_has_content())
        ret |= 0x01; // data ready

    return ret;
}

uint8_t PC98_GDC_state::rfifo_read_data(void) {
    uint8_t ret;

    ret = rfifo[rfifo_read];
    if (rfifo_read < rfifo_write) {
        if (++rfifo_read >= rfifo_write) {
            rfifo_read = rfifo_write = 0;
            rfifo[0] = ret;
        }
    }

    return ret;
}

void gdc_proc_schedule_delay(void) {
    if (!gdc_proc_delay_set) {
        PIC_AddEvent(GDC_ProcDelay,(float)gdc_proc_delay);
        gdc_proc_delay_set = false;
    }
}

void gdc_proc_schedule_cancel(void) {
    if (gdc_proc_delay_set) {
        PIC_RemoveEvents(GDC_ProcDelay);
        gdc_proc_delay_set = false;
    }
}

void gdc_proc_schedule_done(void) {
    gdc_proc_delay_set = false;
}

void PC98_show_cursor(bool show) {
    pc98_gdc[GDC_MASTER].force_fifo_complete();

    pc98_gdc[GDC_MASTER].cursor_enable = show;

    /* NTS: Showing/hiding the cursor involves sending a GDC command that
     *      sets both the cursor visibility bit and the "lines per character
     *      row" field.
     *
     *      The PC-98 BIOS will re-read this from the BIOS data area */
    pc98_gdc[GDC_MASTER].row_height = (mem_readb(0x53B) & 0x1F) + 1;
}

void GDC_ProcDelay(Bitu /*val*/) {
    gdc_proc_schedule_done();

    for (unsigned int i=0;i < 2;i++)
        pc98_gdc[i].idle_proc(); // may schedule another delayed proc
}

bool gdc_5mhz_according_to_bios(void) {
    return !!(mem_readb(0x54D) & 0x04);
}

void gdc_5mhz_mode_update_vars(void) {
    unsigned char b;

    b = mem_readb(0x54D);

    /* bit[5:5] = GDC at 5.0MHz at boot up (copy of DIP switch 2-8 at startup)      1=yes 0=no
     * bit[2:2] = GDC clock                                                         1=5MHz 0=2.5MHz */

    if (gdc_5mhz_mode_initial)
        b |=  0x20;
    else
        b &= ~0x20;

    if (gdc_5mhz_mode)
        b |=  0x04;
    else
        b &= ~0x04;

    mem_writeb(0x54D,b);
}

/*==================================================*/

void pc98_gdc_write(Bitu port,Bitu val,Bitu iolen) {
    (void)iolen;//UNUSED
    PC98_GDC_state *gdc;

    if (port >= 0xA0)
        gdc = &pc98_gdc[GDC_SLAVE];
    else
        gdc = &pc98_gdc[GDC_MASTER];

    switch (port&0xE) {
        case 0x00:      /* 0x60/0xA0 param write fifo */
            if (!gdc->write_fifo_param((unsigned char)val))
                LOG_MSG("GDC warning: FIFO param overrun");
            break;
        case 0x02:      /* 0x62/0xA2 command write fifo */
            if (!gdc->write_fifo_command((unsigned char)val))
                LOG_MSG("GDC warning: FIFO command overrun");
            break;
        case 0x04:      /* 0x64: set trigger to signal vsync interrupt (IRQ 2) */
                        /* 0xA4: Bit 0 select display "plane" */
            if (port == 0x64)
                GDC_vsync_interrupt = true;
            else {
                /* NTS: Testing on real hardware suggests that this bit is NOT double buffered,
                 *      therefore you can cause a tearline flipping the bit mid-screen without
                 *      waiting for vertical blanking.
                 *
                 * But: For the user's preference, we do offer a hack to delay display plane
                 *      change until vsync to try to alleviate tearlines. */
                GDC_display_plane_pending = (val&1);
                if (!GDC_display_plane_wait_for_vsync) {
                    GDC_display_plane = GDC_display_plane_pending;
                    pc98_update_display_page_ptr();
                }
            }
            break;
        case 0x06:      /* 0x66: ??
                           0xA6: Bit 0 select CPU access "plane" */
            if (port == 0xA6) {
                pc98_gdc_vramop &= ~(1 << VOPBIT_ACCESS);
                pc98_gdc_vramop |=  (val&1) << VOPBIT_ACCESS;
                pc98_update_cpu_page_ptr();
            }
            else {
                goto unknown;
            }
            break;
        case 0x08:      /* 0xA8: One of two meanings, depending on 8 or 16/256--color mode */
                        /*         8-color: 0xA8-0xAB are 8 4-bit packed fields remapping the 3-bit GRB colors. This defines colors #3 [7:4] and #7 [3:0]
                         *         16-color: GRB color palette index */
                        /* 0x68: A command */
                        /* NTS: Sadly, "undocumented PC-98" reference does not mention the analog 16-color palette. */
            if (port == 0xA8) {
                if (gdc_analog) { /* 16/256-color mode */
                    pc98_16col_analog_rgb_palette_index = (uint8_t)val; /* it takes all 8 bits I assume because of 256-color mode */
                }
                else {
                    pc98_set_digpal_pair(3,(unsigned char)val);
                }
            }
            else {
                pc98_port68_command_write((unsigned char)val);
            }
            break;
        case 0x0A:      /* 0xAA:
                           8-color: Defines color #1 [7:4] and color #5 [3:0] (FIXME: Or is it 2 and 6, by undocumented PC-98???)
                           16-color: 4-bit green intensity. Color index is low 4 bits of palette index.
                           256-color: 4-bit green intensity. Color index is 8-bit palette index. */
            if (port == 0xAA) { /* TODO: If 8-color... else if 16-color... else if 256-color... */
                if (gdc_analog) { /* 16/256-color mode */
                    if (pc98_gdc_vramop & (1 << VOPBIT_VGA)) {
                        pc98_pal_vga[(3*pc98_16col_analog_rgb_palette_index) + 0] = (uint8_t)val;
                        vga.dac.rgb[pc98_16col_analog_rgb_palette_index].green = (uint8_t)val;
                        VGA_DAC_UpdateColor(pc98_16col_analog_rgb_palette_index);
                    }
                    else {
                        pc98_pal_analog[(3*(pc98_16col_analog_rgb_palette_index&0xF)) + 0] = val&0x0F;
                        vga.dac.rgb[pc98_16col_analog_rgb_palette_index & 0xF].green = dac_4to6(val&0xF); /* re-use VGA DAC */
                        VGA_DAC_UpdateColor(pc98_16col_analog_rgb_palette_index & 0xF);
                    }
                }
                else {
                    pc98_set_digpal_pair(1,(unsigned char)val);
                }
            }
            else {
                pc98_port6A_command_write((unsigned char)val);
            }
            break;
        case 0x0C:      /* 0xAC:
                           8-color: Defines color #2 [7:4] and color #6 [3:0] (FIXME: Or is it 1 and 4, by undocumented PC-98???)
                           16-color: 4-bit red intensity. Color index is low 4 bits of palette index.
                           256-color: 4-bit red intensity. Color index is 8-bit palette index. */
            if (port == 0xAC) { /* TODO: If 8-color... else if 16-color... else if 256-color... */
                if (gdc_analog) { /* 16/256-color mode */
                    if (pc98_gdc_vramop & (1 << VOPBIT_VGA)) {
                        pc98_pal_vga[(3*pc98_16col_analog_rgb_palette_index) + 1] = (uint8_t)val;
                        vga.dac.rgb[pc98_16col_analog_rgb_palette_index].red = (uint8_t)val;
                        VGA_DAC_UpdateColor(pc98_16col_analog_rgb_palette_index);
                    }
                    else {
                        pc98_pal_analog[(3*(pc98_16col_analog_rgb_palette_index&0xF)) + 1] = val&0x0F;
                        vga.dac.rgb[pc98_16col_analog_rgb_palette_index & 0xF].red = dac_4to6(val&0xF); /* re-use VGA DAC */
                        VGA_DAC_UpdateColor(pc98_16col_analog_rgb_palette_index & 0xF);
                    }
                }
                else {
                    pc98_set_digpal_pair(2,(unsigned char)val);
                }
            }
            else {
                goto unknown;
            }
            break;
        case 0x0E:      /* 0xAE:
                           8-color: Defines color #2 [7:4] and color #6 [3:0] (FIXME: Or is it 1 and 4, by undocumented PC-98???)
                           16-color: 4-bit blue intensity. Color index is low 4 bits of palette index.
                           256-color: 4-bit blue intensity. Color index is 8-bit palette index. */
            if (port == 0xAE) { /* TODO: If 8-color... else if 16-color... else if 256-color... */
                if (gdc_analog) { /* 16/256-color mode */
                    if (pc98_gdc_vramop & (1 << VOPBIT_VGA)) {
                        pc98_pal_vga[(3*pc98_16col_analog_rgb_palette_index) + 2] = (uint8_t)val;
                        vga.dac.rgb[pc98_16col_analog_rgb_palette_index].blue = (uint8_t)val;
                        VGA_DAC_UpdateColor(pc98_16col_analog_rgb_palette_index);
                    }
                    else {
                        pc98_pal_analog[(3*(pc98_16col_analog_rgb_palette_index&0xF)) + 2] = val&0x0F;
                        vga.dac.rgb[pc98_16col_analog_rgb_palette_index & 0xF].blue = dac_4to6(val&0xF); /* re-use VGA DAC */
                        VGA_DAC_UpdateColor(pc98_16col_analog_rgb_palette_index & 0xF);
                    }
                }
                else {
                    pc98_set_digpal_pair(0,(unsigned char)val);
                }
            }
            else {
                goto unknown;
            }
            break;
        default:
            unknown:
            LOG_MSG("GDC unexpected write to port 0x%x val=0x%x",(unsigned int)port,(unsigned int)val);
            break;
    }
}

Bitu pc98_gdc_read(Bitu port,Bitu iolen) {
    (void)iolen;//UNUSED
    PC98_GDC_state *gdc;

    if (port >= 0xA0)
        gdc = &pc98_gdc[GDC_SLAVE];
    else
        gdc = &pc98_gdc[GDC_MASTER];

    switch (port&0xE) {
        case 0x00:      /* 0x60/0xA0 read status */
            return gdc->read_status();
        case 0x02:      /* 0x62/0xA2 read fifo */
            if (!gdc->rfifo_has_content())
                return gdc->read_status();//FIXME this stops "Battle Skin Panic" from getting stuck is this correct behavior?

            return gdc->rfifo_read_data();
        case 0x04:      /* 0x64: nothing */
                        /* 0xA4: Bit 0 select display "plane" */
            if (port == 0x64) {
                goto unknown;
            }
            else {
                return GDC_display_plane_pending;
            }
            break;
        case 0x06:      /* 0x66: ??
                           0xA6: Bit 0 indicates current CPU access "plane" */
            if (port == 0xA6) {
                return (pc98_gdc_vramop & (1 << VOPBIT_ACCESS)) ? 1 : 0;
            }
            else {
                goto unknown;
            }
            break;
        case 0x08:
            if (port == 0xA8) {
                if (gdc_analog) { /* 16/256-color mode */
                    return pc98_16col_analog_rgb_palette_index;
                }
                else {
                    return pc98_get_digpal_pair(3);
                }
            }
            else {
                goto unknown;
            }
            break;
        case 0x0A:
            if (port == 0xAA) { /* TODO: If 8-color... else if 16-color... else if 256-color... */
                if (gdc_analog) { /* 16/256-color mode */
                    if (pc98_gdc_vramop & (1 << VOPBIT_VGA))
                        return pc98_pal_vga[(3*pc98_16col_analog_rgb_palette_index) + 0];
                    else
                        return pc98_pal_analog[(3*(pc98_16col_analog_rgb_palette_index&0xF)) + 0];
                }
                else {
                    return pc98_get_digpal_pair(1);
                }
            }
            else {
                goto unknown;
            }
            break;
        case 0x0C:
            if (port == 0xAC) { /* TODO: If 8-color... else if 16-color... else if 256-color... */
                if (gdc_analog) { /* 16/256-color mode */
                    if (pc98_gdc_vramop & (1 << VOPBIT_VGA))
                        return pc98_pal_vga[(3*pc98_16col_analog_rgb_palette_index) + 1];
                    else
                        return pc98_pal_analog[(3*(pc98_16col_analog_rgb_palette_index&0xF)) + 1];
                }
                else {
                    return pc98_get_digpal_pair(2);
                }
            }
            else {
                goto unknown;
            }
            break;
        case 0x0E:
            if (port == 0xAE) { /* TODO: If 8-color... else if 16-color... else if 256-color... */
                if (gdc_analog) { /* 16/256-color mode */
                    if (pc98_gdc_vramop & (1 << VOPBIT_VGA))
                        return pc98_pal_vga[(3*pc98_16col_analog_rgb_palette_index) + 2];
                    else
                        return pc98_pal_analog[(3*(pc98_16col_analog_rgb_palette_index&0xF)) + 2];
                }
                else {
                    return pc98_get_digpal_pair(0);
                }
            }
            else {
                goto unknown;
            }
            break;
        default:
            unknown:
            LOG_MSG("GDC unexpected read from port 0x%x",(unsigned int)port);
            break;
    }

    return ~0ul;
}

