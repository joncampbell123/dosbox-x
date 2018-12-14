 /*
 *  Copyright (C) 2002-2015  The DOSBox Team
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


#ifndef DOSBOX_VGA_H
#define DOSBOX_VGA_H

#ifndef DOSBOX_DOSBOX_H
#include "dosbox.h"
#endif
#include <iostream>

#define VGA_LFB_MAPPED

#define S3_LFB_BASE_DEFAULT	  0xE0000000u

class PageHandler;

enum VGAModes {
    M_CGA2,         // 0
    M_CGA4,
    M_EGA,
    M_VGA,
    M_LIN4,
    M_LIN8,         // 5
    M_LIN15,
    M_LIN16,
    M_LIN24,
    M_LIN32,
    M_TEXT,         // 10
    M_HERC_GFX,
    M_HERC_TEXT,
    M_CGA16,
    M_TANDY2,
    M_TANDY4,       // 15
    M_TANDY16,
    M_TANDY_TEXT,
    M_AMSTRAD,
    M_PC98,
    M_FM_TOWNS,     // 20 STUB
    M_PACKED4,
    M_ERROR,

    M_MAX
};

extern const char* const mode_texts[M_MAX];

enum VGA_Vsync {
	VS_Off,
	VS_On,
	VS_Force,
	VS_Host,
};

struct vsync_state {
	double period;
	bool manual;		// use manual vsync timing
	bool persistent;	// use persistent timer (to keep in sync even after internal mode switches)
	bool faithful;		// use faithful framerate adjustment
};

extern struct vsync_state vsync;
extern float uservsyncjolt;

#define CLK_25 25175u
#define CLK_28 28322u

#define MIN_VCO	180000u
#define MAX_VCO 360000u

#define S3_CLOCK_REF	14318u	/* KHz */
#define S3_CLOCK(_M,_N,_R)	((S3_CLOCK_REF * (((Bitu)_M) + 2ul)) / ((((Bitu)_N) + 2ul) * (1ul << ((Bitu)_R))))
#define S3_MAX_CLOCK	150000u	/* KHz */

#define S3_XGA_1024		0x00u
#define S3_XGA_1152		0x01u
#define S3_XGA_640		0x40u
#define S3_XGA_800		0x80u
#define S3_XGA_1280		0xc0u
#define S3_XGA_1600		0x81u
#define S3_XGA_WMASK	(S3_XGA_640|S3_XGA_800|S3_XGA_1024|S3_XGA_1152|S3_XGA_1280)

#define S3_XGA_8BPP  0x00u
#define S3_XGA_16BPP 0x10u
#define S3_XGA_32BPP 0x30u
#define S3_XGA_CMASK (S3_XGA_8BPP|S3_XGA_16BPP|S3_XGA_32BPP)

typedef struct {
	bool attrindex;
} VGA_Internal;

typedef struct {
/* Memory handlers */
	Bitu mh_mask;

/* Video drawing */
	Bitu display_start;
	Bitu real_start;
	bool retrace;					/* A retrace is active */
	Bitu scan_len;
	Bitu cursor_start;

/* Some other screen related variables */
	Bitu line_compare;
	bool chained;					/* Enable or Disabled Chain 4 Mode */
	bool compatible_chain4;

	/* Pixel Scrolling */
	Bit8u pel_panning;				/* Amount of pixels to skip when starting horizontal line */
	Bit8u hlines_skip;
	Bit8u bytes_skip;
	Bit8u addr_shift;

/* Specific stuff memory write/read handling */
	
	Bit8u read_mode;
	Bit8u write_mode;
	Bit8u read_map_select;
	Bit8u color_dont_care;
	Bit8u color_compare;
	Bit8u data_rotate;
	Bit8u raster_op;

	Bit32u full_bit_mask;
	Bit32u full_map_mask;
	Bit32u full_not_map_mask;
	Bit32u full_set_reset;
	Bit32u full_not_enable_set_reset;
	Bit32u full_enable_set_reset;
	Bit32u full_enable_and_set_reset;
} VGA_Config;

typedef enum {
	LINE,
	EGALINE
} Drawmode;

typedef struct {
	bool resizing;
	Bitu width;
	Bitu height;
	Bitu blocks;
	Bitu address;
	Bitu panning;
	Bitu bytes_skip;
	Bit8u *linear_base;
	Bitu linear_mask;
    Bitu planar_mask;
	Bitu address_add;
	Bitu line_length;
	Bitu address_line_total;
	Bitu address_line;
	Bitu lines_total;
	Bitu vblank_skip;
	Bitu lines_done;
	Bitu split_line;
	Bitu byte_panning_shift;
    Bitu render_step,render_max;
	struct {
		double framestart;
		double vrstart, vrend;		// V-retrace
		double hrstart, hrend;		// H-retrace
		double hblkstart, hblkend;	// H-blanking
		double vblkstart, vblkend;	// V-Blanking
		double vdend, vtotal;
		double hdend, htotal;
		float singleline_delay;
	} delay;
	double screen_ratio;
	double refresh;
	Bit8u font[516*1024]; /* enlarged to 516KB for PC-98 character font data (256*16) + (128*2*128*16) */
	Bit8u * font_tables[2];
	Bitu blinking;
	bool blink;
	bool char9dot;
	struct {
		Bitu address;
		Bit8u sline,eline;
		Bit8u count,delay;
		Bit8u enabled;
	} cursor;
	Drawmode mode;
	bool has_split;
	bool vret_triggered;
	bool vga_override;
	bool doublescan_set;
	bool doublescan_effect;
	bool char9_set;
	Bitu bpp;
	double clock;
	Bit8u cga_snow[80];			// one bit per horizontal column where snow should occur
} VGA_Draw;

typedef struct {
    struct start_end_t {
        unsigned int            start = 0,end = 0;
    };

    /* pixel 0 is start of display area.
     * next scanline starts when current == total before drawing next pixel.
     */
    struct general_dim {
        unsigned int            crtc_addr = 0;      // CRTC counter address (H) / CRTC counter address at start of line (V)
        unsigned int            current = 0;        // current position in pixels within scan line (H) / number of scan line (V)
        unsigned int            total = 0;          // total pixels in scan line (H) / total scan lines (V)
        start_end_t             active;             // first pixel (H) / scan line (V) that active display STARTs, ENDs (start == 0 usually)
        start_end_t             blank;              // first pixel (H) / scan line (V) that blanking BEGINs, ENDs
        start_end_t             retrace;            // first pixel (H) / scan line (V) that retrace BEGINs, ENDs

        pic_tickindex_t         time_begin;                     // start of scan line (H) / frame (V) PIC full index time

        unsigned char           current_char_pixel = 0;         // current pixel position (H) / scan line (V) within character cell
        unsigned char           char_pixels = 0;                // width (H) / scan lines (V) of a character cell

        bool                    blank_enable = false;           // blank enable
        bool                    active_enable = false;          // active display enable
        bool                    retrace_enable = false;         // retrace enable
    };

    unsigned int                crtc_mask = 0;      // draw from memory ((addr & mask) + add)
    unsigned int                crtc_add = 0;

    inline unsigned int crtc_addr_fetch(void) const {
        return (horz.crtc_addr & crtc_mask) + crtc_add;
    }

    // NTS: horz.char_pixels == 8 for CGA/MDA/etc and EGA/VGA text, but EGA/VGA can select 9 pixels/char.
    //      VGA 320x200x256-color mode will have 4 pixels/char.

    struct dotclock_t {
        double                  rate_invmult = 0;
        double                  rate_mult = 0;
        double                  rate = 0;
        pic_tickindex_t         base = 0;
        signed long long        ticks = 0;
        signed long long        ticks_prev = 0;

        void reset(const pic_tickindex_t now) {
            ticks = ticks_prev = 0;
            base = now;
        }

        // do not call unless all ticks processed
        void set_rate(const double new_rate,const pic_tickindex_t now) {
            if (rate != new_rate) {
                update_ceil(now);
                rebase();

                if ((rate <= 0) || (fabs(now - base) > (0.5 * rate_invmult)))
                    base = now;

                if (new_rate > 0) {
                    rate_invmult = 1000 / new_rate; /* Hz -> ms */
                    rate_mult = new_rate / 1000; /* ms -> Hz */
                }
                else {
                    rate_mult = rate_invmult = 0;
                }
                rate = new_rate;

                update(now);
                ticks_prev = ticks;
            }
        }

        inline pic_tickindex_t ticks2pic_relative(const signed long long t,const pic_tickindex_t now) const {
            return (t * rate_invmult) + (base - now);/* group float operations to maintain precision */
        }

        inline pic_tickindex_t ticks2pic(const signed long long t) const {
            return (t * rate_invmult) + base;
        }

        inline signed long long pic2ticks(const pic_tickindex_t now) const {
            return (signed long long)floor((now - base) * rate_mult);
        }

        inline signed long long pic2ticks_ceil(const pic_tickindex_t now) const {
            return (signed long long)ceil((now - base) * rate_mult);
        }

        // inline and minimal for performance!
        inline void update(const pic_tickindex_t now) {
            /* NTS: now = PIC_FullIndex() which is time in ms (1/1000 of a sec) */
            ticks = pic2ticks(now);
        }

        inline void update_ceil(const pic_tickindex_t now) {
            /* NTS: now = PIC_FullIndex() which is time in ms (1/1000 of a sec) */
            ticks = pic2ticks_ceil(now);
        }

        // retrival of tick count and reset of counter
        inline signed long long delta_peek(void) const {
            return ticks - ticks_prev;
        }

        inline signed long long delta_get(void) {
            signed long long ret = delta_peek();
            ticks_prev = ticks;
            return ret;
        }

        // rebase of the counter.
        // call this every so often (but not too often) in order to prevent floating point
        // precision loss over time as the numbers get larger and larger.
        void rebase(void) {
            if (rate_mult > 0) {
                base += ticks / rate_mult;
                ticks = ticks_prev = 0;
            }
        }
    };

    // use the dot clock to map advancement of emulator time to dot clock ticks.
    // apply the dot clock ticks against the horizontal and vertical current position
    // to emulate the raster of the video output over time.
    //
    // if anything changes dot clock rate, process all ticks and advance hardware
    // state, then set the rate and process all dot clock ticks after that point
    // at the new rate.

    dotclock_t                  dotclock;
    general_dim                 horz,vert;
} VGA_Draw_2;

typedef struct {
	Bit8u curmode;
	Bit16u originx, originy;
	Bit8u fstackpos, bstackpos;
	Bit8u forestack[4];
	Bit8u backstack[4];
	Bit16u startaddr;
	Bit8u posx, posy;
	Bit8u mc[64][64];
} VGA_HWCURSOR;

typedef struct {
	Bit8u reg_lock1;
	Bit8u reg_lock2;
	Bit8u reg_31;
	Bit8u reg_35;
	Bit8u reg_36; // RAM size
	Bit8u reg_3a; // 4/8/doublepixel bit in there
	Bit8u reg_40; // 8415/A functionality register
	Bit8u reg_41; // BIOS flags 
	Bit8u reg_42; // CR42 Mode Control
	Bit8u reg_43;
	Bit8u reg_45; // Hardware graphics cursor
	Bit8u reg_50;
	Bit8u reg_51;
	Bit8u reg_52;
	Bit8u reg_55;
	Bit8u reg_58;
	Bit8u reg_6b; // LFB BIOS scratchpad
	Bit8u ex_hor_overflow;
	Bit8u ex_ver_overflow;
	Bit16u la_window;
	Bit8u misc_control_2;
	Bit8u ext_mem_ctrl;
	Bitu xga_screen_width;
	VGAModes xga_color_mode;
	struct {
		Bit8u r;
		Bit8u n;
		Bit8u m;
	} clk[4],mclk;
	struct {
		Bit8u lock;
		Bit8u cmd;
	} pll;
	VGA_HWCURSOR hgc;
} VGA_S3;

typedef struct {
	Bit8u mode_control;
	Bit8u enable_bits;
	bool blend;
} VGA_HERC;

typedef struct {
	Bit32u mask_plane;
	Bit8u write_plane;
	Bit8u read_plane;
	Bit8u border_color;
} VGA_AMSTRAD;

typedef struct {
	Bit8u index;
	Bit8u htotal;
	Bit8u hdend;
	Bit8u hsyncp;
	Bit8u hsyncw;
	Bit8u vtotal;
	Bit8u vdend;
	Bit8u vadjust;
	Bit8u vsyncp;
	Bit8u vsyncw;
	Bit8u max_scanline;
	Bit16u lightpen;
	bool lightpen_triggered;
	Bit8u cursor_start;
	Bit8u cursor_end;
    Bit8u mcga_mode_control;
} VGA_OTHER;

typedef struct {
	Bit8u pcjr_flipflop;
	Bit8u mode_control;
	Bit8u color_select;
	Bit8u disp_bank;
	Bit8u reg_index;
	Bit8u gfx_control;
	Bit8u palette_mask;
	Bit8u extended_ram;
	Bit8u border_color;
	Bit8u line_mask, line_shift;
	Bit8u draw_bank, mem_bank;
	Bit8u *draw_base, *mem_base;
	Bitu addr_mask;
} VGA_TANDY;

typedef struct {
	Bit8u index;
	Bit8u reset;
	Bit8u clocking_mode;
	Bit8u map_mask;
	Bit8u character_map_select;
	Bit8u memory_mode;
} VGA_Seq;

typedef struct {
	Bit8u palette[16];
	Bit8u mode_control;
	Bit8u horizontal_pel_panning;
	Bit8u overscan_color;
	Bit8u color_plane_enable;
	Bit8u color_select;
	Bit8u index;
	Bit8u disabled; // Used for disabling the screen.
					// Bit0: screen disabled by attribute controller index
					// Bit1: screen disabled by sequencer index 1 bit 5
					// These are put together in one variable for performance reasons:
					// the line drawing function is called maybe 60*480=28800 times/s,
					// and we only need to check one variable for zero this way.
} VGA_Attr;

typedef struct {
	Bit8u horizontal_total;
	Bit8u horizontal_display_end;
	Bit8u start_horizontal_blanking;
	Bit8u end_horizontal_blanking;
	Bit8u start_horizontal_retrace;
	Bit8u end_horizontal_retrace;
	Bit8u vertical_total;
	Bit8u overflow;
	Bit8u preset_row_scan;
	Bit8u maximum_scan_line;
	Bit8u cursor_start;
	Bit8u cursor_end;
	Bit8u start_address_high;
	Bit8u start_address_low;
	Bit8u cursor_location_high;
	Bit8u cursor_location_low;
	Bit8u vertical_retrace_start;
	Bit8u vertical_retrace_end;
	Bit8u vertical_display_end;
	Bit8u offset;
	Bit8u underline_location;
	Bit8u start_vertical_blanking;
	Bit8u end_vertical_blanking;
	Bit8u mode_control;
	Bit8u line_compare;

	Bit8u index;
	bool read_only;
} VGA_Crtc;

typedef struct {
	Bit8u index;
	Bit8u set_reset;
	Bit8u enable_set_reset;
	Bit8u color_compare;
	Bit8u data_rotate;
	Bit8u read_map_select;
	Bit8u mode;
	Bit8u miscellaneous;
	Bit8u color_dont_care;
	Bit8u bit_mask;
} VGA_Gfx;

typedef struct  {
	Bit8u red;
	Bit8u green;
	Bit8u blue;
} RGBEntry;

typedef struct {
	Bit8u bits;						/* DAC bits, usually 6 or 8 */
	Bit8u pel_mask;
	Bit8u pel_index;	
	Bit8u state;
	Bit8u write_index;
	Bit8u read_index;
	Bitu first_changed;
	Bit8u combine[16];
	RGBEntry rgb[0x100];
	Bit16u xlat16[256];
	Bit32u xlat32[256];
	Bit8u hidac_counter;
	Bit8u reg02;
} VGA_Dac;

typedef struct {
	Bitu	readStart, writeStart;
	Bitu	bankMask;
	Bitu	bank_read_full;
	Bitu	bank_write_full;
	Bit8u	bank_read;
	Bit8u	bank_write;
	Bitu	bank_size;
} VGA_SVGA;

typedef union {
	Bit32u d;
	Bit8u b[4];
} VGA_Latch;

typedef struct {
	Bit8u*      linear = NULL;
	Bit8u*      linear_orgptr = NULL;

    uint32_t    memsize = 0;
    uint32_t    memmask = 0;
    uint32_t    memmask_crtc = 0;       // in CRTC-visible units (depends on byte/word/dword mode)
} VGA_Memory;

typedef struct {
	//Add a few more just to be safe
	Bit8u*	map; /* allocated dynamically: [(VGA_MEMORY >> VGA_CHANGE_SHIFT) + 32] */
	Bit8u	checkMask, frame, writeMask;
	bool	active;
	Bit32u  clearMask;
	Bit32u	start, last;
	Bit32u	lastAddress;
} VGA_Changes;

typedef struct {
	Bit32u page;
	Bit32u addr;
	Bit32u mask;
	PageHandler *handler;
} VGA_LFB;

typedef struct {
	VGAModes mode;								/* The mode the vga system is in */
	VGAModes lastmode;
	Bit8u misc_output;
	VGA_Draw draw;
	VGA_Config config;
	VGA_Internal internal;
/* Internal module groups */
	VGA_Seq seq;
	VGA_Attr attr;
	VGA_Crtc crtc;
	VGA_Gfx gfx;
	VGA_Dac dac;
	VGA_Latch latch;
	VGA_S3 s3;
	VGA_SVGA svga;
	VGA_HERC herc;
	VGA_TANDY tandy;
	VGA_AMSTRAD amstrad;
	VGA_OTHER other;
	VGA_Memory mem;
	VGA_LFB lfb;
} VGA_Type;


/* Hercules Palette function */
void Herc_Palette(void);

/* CGA Mono Palette function */
void Mono_CGA_Palette(void);

/* Functions for different resolutions */
void VGA_SetMode(VGAModes mode);
void VGA_DetermineMode(void);
void VGA_SetupHandlers(void);
void VGA_StartResize(Bitu delay=50);
void VGA_SetupDrawing(Bitu val);
void VGA_CheckScanLength(void);
void VGA_ChangedBank(void);

/* Some DAC/Attribute functions */
void VGA_DAC_CombineColor(Bit8u attr,Bit8u pal);
void VGA_DAC_SetEntry(Bitu entry,Bit8u red,Bit8u green,Bit8u blue);
void VGA_ATTR_SetPalette(Bit8u index,Bit8u val);

typedef enum {CGA, EGA, MONO} EGAMonitorMode;

typedef enum {AC_4x4, AC_low4/*4low*/} ACPalRemapMode;

extern unsigned char VGA_AC_remap;

void VGA_ATTR_SetEGAMonitorPalette(EGAMonitorMode m);

/* The VGA Subfunction startups */
void VGA_SetupAttr(void);
void VGA_SetupMemory(void);
void VGA_SetupDAC(void);
void VGA_SetupCRTC(void);
void VGA_SetupMisc(void);
void VGA_SetupGFX(void);
void VGA_SetupSEQ(void);
void VGA_SetupOther(void);
void VGA_SetupXGA(void);

/* Some Support Functions */
void VGA_SetClock(Bitu which,Bitu target);
void VGA_DACSetEntirePalette(void);
void VGA_StartRetrace(void);
void VGA_StartUpdateLFB(void);
void VGA_SetBlinking(Bitu enabled);
void VGA_SetCGA2Table(Bit8u val0,Bit8u val1);
void VGA_SetCGA4Table(Bit8u val0,Bit8u val1,Bit8u val2,Bit8u val3);
void VGA_ActivateHardwareCursor(void);
void VGA_KillDrawing(void);

void VGA_SetOverride(bool vga_override);

extern VGA_Type vga;

/* Support for modular SVGA implementation */
/* Video mode extra data to be passed to FinishSetMode_SVGA().
   This structure will be in flux until all drivers (including S3)
   are properly separated. Right now it contains only three overflow
   fields in S3 format and relies on drivers re-interpreting those.
   For reference:
   ver_overflow:X|line_comp10|X|vretrace10|X|vbstart10|vdispend10|vtotal10
   hor_overflow:X|X|X|hretrace8|X|hblank8|hdispend8|htotal8
   offset is not currently used by drivers (useful only for S3 itself)
   It also contains basic int10 mode data - number, vtotal, htotal
   */
typedef struct {
	Bit8u ver_overflow;
	Bit8u hor_overflow;
	Bitu offset;
	Bitu modeNo;
	Bitu htotal;
	Bitu vtotal;
} VGA_ModeExtraData;

// Vector function prototypes
typedef void (*tWritePort)(Bitu reg,Bitu val,Bitu iolen);
typedef Bitu (*tReadPort)(Bitu reg,Bitu iolen);
typedef void (*tFinishSetMode)(Bitu crtc_base, VGA_ModeExtraData* modeData);
typedef void (*tDetermineMode)();
typedef void (*tSetClock)(Bitu which,Bitu target);
typedef Bitu (*tGetClock)();
typedef bool (*tHWCursorActive)();
typedef bool (*tAcceptsMode)(Bitu modeNo);
typedef void (*tSetupDAC)();
typedef void (*tINT10Extensions)();

struct SVGA_Driver {
	tWritePort write_p3d5;
	tReadPort read_p3d5;
	tWritePort write_p3c5;
	tReadPort read_p3c5;
	tWritePort write_p3c0;
	tReadPort read_p3c1;
	tWritePort write_p3cf;
	tReadPort read_p3cf;

	tFinishSetMode set_video_mode;
	tDetermineMode determine_mode;
	tSetClock set_clock;
	tGetClock get_clock;
	tHWCursorActive hardware_cursor_active;
	tAcceptsMode accepts_mode;
	tSetupDAC setup_dac;
	tINT10Extensions int10_extensions;
};

extern SVGA_Driver svga;
extern int enableCGASnow;

void SVGA_Setup_S3Trio(void);
void SVGA_Setup_TsengET4K(void);
void SVGA_Setup_TsengET3K(void);
void SVGA_Setup_ParadisePVGA1A(void);
void SVGA_Setup_Driver(void);

// Amount of video memory required for a mode, implemented in int10_modes.cpp
Bitu VideoModeMemSize(Bitu mode);

extern Bit32u ExpandTable[256];
extern Bit32u FillTable[16];
extern Bit32u CGA_2_Table[16];
extern Bit32u CGA_4_Table[256];
extern Bit32u CGA_4_HiRes_Table[256];
extern Bit32u CGA_16_Table[256];
extern Bit32u TXT_Font_Table[16];
extern Bit32u TXT_FG_Table[16];
extern Bit32u TXT_BG_Table[16];
extern Bit32u Expand16Table[4][16];
extern Bit32u Expand16BigTable[0x10000];

void VGA_DAC_UpdateColorPalette();

extern uint32_t GFX_Rmask;
extern unsigned char GFX_Rshift;

extern uint32_t GFX_Gmask;
extern unsigned char GFX_Gshift;

extern uint32_t GFX_Bmask;
extern unsigned char GFX_Bshift;

extern uint32_t GFX_Amask;
extern unsigned char GFX_Ashift;

extern unsigned char GFX_bpp;

#endif
