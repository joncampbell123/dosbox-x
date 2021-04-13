/*
 *  Copyright (C) 2002-2021  The DOSBox Team
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
#define S3_CLOCK(_M,_N,_R)	((S3_CLOCK_REF * (((Bitu)_M) + 2ul)) / ((((Bitu)_N) + 2ul) * ((Bitu)1ul << ((Bitu)_R))))
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
	uint8_t pel_panning;				/* Amount of pixels to skip when starting horizontal line */
	uint8_t hlines_skip;
	uint8_t bytes_skip;
	uint8_t addr_shift;

/* Specific stuff memory write/read handling */
	
	uint8_t read_mode;
	uint8_t write_mode;
	uint8_t read_map_select;
	uint8_t color_dont_care;
	uint8_t color_compare;
	uint8_t data_rotate;
	uint8_t raster_op;

	uint32_t full_bit_mask;
	uint32_t full_map_mask;
	uint32_t full_not_map_mask;
	uint32_t full_set_reset;
	uint32_t full_not_enable_set_reset;
	uint32_t full_enable_set_reset;
	uint32_t full_enable_and_set_reset;
} VGA_Config;

typedef enum {
	DRAWLINE,
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
	uint8_t *linear_base;
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
	uint8_t font[516*1024]; /* enlarged to 516KB for PC-98 character font data (256*16) + (128*2*128*16) */
	uint8_t * font_tables[2];
	Bitu blinking;
	bool blink;
	bool char9dot;
	struct {
		Bitu address;
		uint8_t sline,eline;
		uint8_t count,delay;
		bool blinkon;
		uint8_t enabled;
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
	double oscclock;
	uint8_t cga_snow[80];			// one bit per horizontal column where snow should occur

	/*Color and brightness for monochrome display*/
	uint8_t monochrome_pal;
	uint8_t monochrome_bright;
} VGA_Draw;

/* enable switch for the "alternative video system" */
extern bool vga_alt_new_mode;

/* NTS: Usage of this general struct will vary between the various video modes.
 *
 *      MDA/Hercules/CGA/PCjr/Tandy: Video hardware is based on the 6845 which
 *      counts horizontal AND vertical timing based on character cells. The
 *      fact that vertical timing is based on character cells is the reason
 *      why changing character cell height requires reprogramming the vertical
 *      timings. Most video modes are not entirely a multiple of the character
 *      cell height, which is why the 6845 has a "vertical adjust" to add to
 *      the total. For example, CGA produces a video signal with NTSC timing
 *      by programming enough character cells vertically with a vertical adjust
 *      to bring video output to the 262 scanlines required by one field of
 *      NTSC video, or 524 scanlines per frame. This isn't quite NTSC since
 *      CGA does not emit the half-a-scanline needed for interlaced (to produce
 *      262.5 lines per field or 525 scanlines per frame) but it happens to
 *      work with most TV sets (although incompatible with Happauge video
 *      capture cards).
 *
 *      Note that the CGA is not the only 80s hardware to emit non-interlaced
 *      NTSC, most video game consoles of the time period do as well. Your
 *      old Nintendo Entertainment System does it too.
 *
 *      Because of the character cell-based vertical timing, CGA emulation here
 *      will probably not rely so much on vert.total as it will on counting
 *      scan lines of the character cell.
 *
 *
 *      EGA/VGA/SVGA: Horizontal timing is based on character cells (which
 *      varies according to the mode and configuration). Vertical timing is
 *      based on scanlines, which is why it is easy to change character cell
 *      height without having to reprogram vertical timing.
 *
 *      Because of that, VGA emulation will count vertical timing entirely by
 *      vert.current and vert.total.
 *
 *
 *      MCGA: Not sure. This is weird hardware. Needs more study. It looks a
 *      lot like the marriage of CGA with a VGA DAC and a 256-color mode tied
 *      to 64KB of memory, and a CRTC that emulates a 6845 but generally ignores
 *      some horizontal and vertical values and hacks others and possibly
 *      carries video line doubling circuitry in order to produce 400-line video
 *      from 200-line video timings (except the 640x480 2-color mode).
 *
 *
 *      NEC PC-98: Two instances of this C++ class will be used in parallel,
 *      with the same dot clock, to emulate the text and graphics "layers" of
 *      PC-98 video. Both instances will generally have the same horizontal
 *      and vertical timing but they don't have to, in which case the VGA
 *      render code will generate the gibberish that would occur on real
 *      hardware when the two are not synchronized.
 */
typedef struct VGA_Experimental_Model_1_t {
    template <typename T> struct pix_char_t {
        T                       pixels;
        T                       chars;

        pix_char_t() { }
        pix_char_t(const T val) : pixels(val), chars(val) { }
    };

    template <typename T> struct start_end_t {
        T                       start;
        T                       end;

        start_end_t() { }
        start_end_t(const T val) : start(val), end(val) { }
        start_end_t(const unsigned int val) : start(val), end(val) { }
    };

    /* pixel 0 is start of display area.
     * next scanline starts when current == total before drawing next pixel.
     */
    struct general_dim {
        // CRTC counter address (H) / CRTC counter address at start of line (V)
        unsigned int                                crtc_addr = 0;

        // CRTC counter address to add per character clock (H) / per scan line (V)
        unsigned int                                crtc_addr_add = 0;

        // current position in pixels within scan line (H) / number of scan line (V)
        pix_char_t<unsigned int>                    current = 0;

        // total pixels in scan line (H) / total scan lines (V)
        pix_char_t<unsigned int>                    total = 0;

        // first pixel (H) / scan line (V) that active display STARTs, ENDs (start == 0 usually)
        start_end_t< pix_char_t<unsigned int> >     active = 0;

        // first pixel (H) / scan line (V) that blanking BEGINs, ENDs
        start_end_t< pix_char_t<unsigned int> >     blank = 0;

        // first pixel (H) / scan line (V) that retrace BEGINs, ENDs
        start_end_t< pix_char_t<unsigned int> >     retrace = 0;

        // largest horizontal active.end value during the entire frame (H) for demos like DoWhackaDo.
        // largest vertical active.end value during the entire frame (V).
        // reset to active.end at start of active display. (H/V)
        pix_char_t<unsigned int>                    active_max = 0;

        // start of scan line (H) / frame (V) PIC full index time
        pic_tickindex_t                             time_begin = 0;

        // length of scan line (H) / length of frame (V)
        pic_tickindex_t                             time_duration = 0;

        // current pixel position (H) / scan line (V) within character cell
        unsigned char                               current_char_pixel = 0;

        // width (H) / scan lines (V) of a character cell
        unsigned char                               char_pixels = 0;

        // bit mask for character row compare
        unsigned char                               char_pixel_mask = 0;

        bool                                        blank_enable = false;           // blank enable
        bool                                        display_enable = false;         // display enable (active area)
        bool                                        retrace_enable = false;         // retrace enable
    };

    // NTS: If start < end, cell starts with enable = false. at start of line,
    //      toggle enable (true) when line == start, then toggle enable (false) when
    //      line == end, then draw.
    //
    //      If start >= end, cell starts with enable = true, at start of line,
    //      toggle enable (false) when line == start, then toggle enable (true) when
    //      line == end, then draw.

    bool                        cursor_enable = false;  // if set, show cursor

    unsigned char               cursor_start = 0;       // cursor starts on this line (toggle cursor enable)
    unsigned char               cursor_end = 0;         // cursor stops on this line (first line to toggle again to disable)

    unsigned int                crtc_cursor_addr = 0;   // crtc address to display cursor at

    unsigned int                crtc_mask = 0;      // draw from memory ((addr & mask) + add)
    unsigned int                crtc_add = 0;       // NTS: For best results crtc_add should only change bits that are masked off

    inline unsigned int crtc_addr_fetch(void) const {
        return (horz.crtc_addr & crtc_mask) + crtc_add;
    }

    inline unsigned int crtc_addr_fetch_and_advance(void) {
        const unsigned int ret = crtc_addr_fetch();
        horz.crtc_addr += horz.crtc_addr_add;
        return ret;
    }

    unsigned int                raster_scanline = 0;    // actual scan line out to display

    unsigned char               doublescan_count = 0;   // VGA doublescan counter
    unsigned char               doublescan_max = 0;     // Advance scanline at this count

    // NTS: horz.char_pixels == 8 for CGA/MDA/etc and EGA/VGA text, but EGA/VGA can select 9 pixels/char.
    //      VGA 320x200x256-color mode will have 4 pixels/char. A hacked version of 320x200x256-color mode
    //      in which the 8BIT bit is cleared (which makes it a sort of 640x200x256-color-ish mode that
    //      reveals the intermediate register states normally hidden) will have 8 pixels/char.
    //
    //      MCGA 320x200x256-color will have horz.char_pixels == 8. A register dump from real hardware shows
    //      that Mode 13 has the same horizontal timings as every other mode (as if 320x200 CGA graphics!).
    //      This is very different from VGA where 320x200x256 is programmed as if a 640x200 graphics mode.
    //
    //      PC-98 will render as if horz.char_pixels == 8 on the text layer. It may set horz.char_pixels == 16
    //      on some text cells if the hardware is to render a double-wide character. The graphics layer is
    //      generally programmed into WORDs mode which means horz.char_pixels == 16 at all times. If it is
    //      ever programmed into byte mode then it will set horz.char_pixels == 8.

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
                update(now);
                rebase();

                if ((rate <= 0) || (fabs(now - base) > (0.5 * rate_invmult)))
                    base = now;

                if (new_rate > 0) {
                    rate_invmult = 1000 / new_rate; /* Hz -> ms */
                    rate_mult = new_rate / 1000; /* ms -> Hz */
                    rate = new_rate;
                }
                else {
                    rate_invmult = 0;
                    rate_mult = 0;
                    rate = 0;
                }

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
            /* typecasting rounds down to 0 apparently. floor() is slower. */
            return (signed long long)((now - base) * rate_mult);
        }

        // inline and minimal for performance!
        inline void update(const pic_tickindex_t now) {
            /* NTS: now = PIC_FullIndex() which is time in ms (1/1000 of a sec) */
            ticks = pic2ticks(now);
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
                base += ticks * rate_invmult;
                ticks = ticks_prev = 0;
            }
        }
    };

    /* integer fraction.
     * If you need more precision (4.1:3 instead of 4:3) just scale up the values (4.1:3 -> 41:30) */
    struct int_fraction_t {
        unsigned int            numerator = 0;
        unsigned int            denominator = 0;

        int_fraction_t() { }
        int_fraction_t(const unsigned int n,const unsigned int d) : numerator(n), denominator(d) { }
    };

    /* 2D display dimensions */
    struct dimensions_t {
        unsigned int            width = 0;
        unsigned int            height = 0;

        dimensions_t() { }
        dimensions_t(const unsigned int w,const unsigned int h) : width(w), height(h) { }
    };

    /* 2D coordinate */
    struct int_point2d_t {
        int                     x = 0;
        int                     y = 0;

        int_point2d_t() { }
        int_point2d_t(const int nx,const int ny) : x(nx), y(ny) { }
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

    // monitor emulation
    dimensions_t                monitor_display;                // image sent to GFX (may include overscan, blanking, etc)
    int_point2d_t               monitor_start_point;            // pixel(x)/scanline(y) counter of the CRTC that is start of line(x)/frame(y)
    int_fraction_t              monitor_aspect_ratio = {4,3};   // display aspect ratio of the video frame

    // The GFX/scaler system will be sent a frame of dimensions monitor_display.
    // The start of the frame will happen when the CRTC pixel count matches the monitor_start_point.
    // monitor_start_point will be set to 0,0 if DOSBox-X is set only to show active area.
    // it will be set to match on the first clock/scanline of the non-blanking area (overscan), upper left corner if set to do so.
    // it will be set to some point of the blanking area if asked to do so to approximate how a VGA monitor centers the image.
    // finally, a debug mode will be offered to show the ENTIRE frame (htotal/vtotal) with markings for retrace if wanted by the user.

    // Pointers to draw from. This represents CRTC character clock 0.
    uint8_t*                      draw_base = NULL;

    template <typename T> inline const T* drawptr(const size_t offset) const {
        return (const T*)draw_base + offset; /* equiv T* ptr = (T*)draw_base; return &ptr[offset]; */
    }

    template <typename T> inline T* drawptr_rw(const size_t offset) const {
        return (T*)draw_base + offset; /* equiv T* ptr = (T*)draw_base; return &ptr[offset]; */
    }
} VGA_Draw_2;

typedef struct {
	uint8_t curmode;
	uint16_t originx, originy;
	uint8_t fstackpos, bstackpos;
	uint8_t forestack[4];
	uint8_t backstack[4];
	uint16_t startaddr;
	uint8_t posx, posy;
	uint8_t mc[64][64];
} VGA_HWCURSOR;

typedef struct {
	uint8_t reg_lock1;
	uint8_t reg_lock2;
	uint8_t reg_31;
	uint8_t reg_35;
	uint8_t reg_36; // RAM size
	uint8_t reg_3a; // 4/8/doublepixel bit in there
	uint8_t reg_40; // 8415/A functionality register
	uint8_t reg_41; // BIOS flags 
	uint8_t reg_42; // CR42 Mode Control
	uint8_t reg_43;
	uint8_t reg_45; // Hardware graphics cursor
	uint8_t reg_50;
	uint8_t reg_51;
	uint8_t reg_52;
	uint8_t reg_55;
	uint8_t reg_58;
	uint8_t reg_63; // Extended control register
	uint8_t reg_6b; // LFB BIOS scratchpad
	uint8_t ex_hor_overflow;
	uint8_t ex_ver_overflow;
	uint16_t la_window;
	uint8_t misc_control_2;
	uint8_t ext_mem_ctrl;
	Bitu xga_screen_width;
	VGAModes xga_color_mode;
	struct {
		uint8_t r;
		uint8_t n;
		uint8_t m;
	} clk[4],mclk;
	struct {
		uint8_t lock;
		uint8_t cmd;
	} pll;
	VGA_HWCURSOR hgc;
	struct {
		// (MM8180 Primary Stream Control)
		uint8_t			psctl_psfc;			// [30:28] PSFC Primary Stream Filter Characteristics
											// 0=RGB-8 CLUT or XRGB-32 (X.8.8.8)  3=KRGB-16 (1.5.5.5)  5=RGB-16 (5.6.5)
		uint8_t			psctl_psidf;		// [26:24] PSIDF Primary Stream Input Data Format
											// 0=Primary Stream  1=..for 2X stretch (replication)  2=..bilinear for 2X stretch (interpolation)
		// (MM8184 Color/Chroma Key Control)
		uint8_t			ckctl_b_lb;			// [ 7: 0] B/V/Cr KEY (LOW)
		uint8_t			ckctl_g_lb;			// [15: 8] G/U/Cb KEY (LOW)
		uint8_t			ckctl_r_lb;			// [23:16] R/Y KEY (LOW)
		uint8_t			ckctl_rgb_cc;		// [26:24] RGB CC - RGB Color Comparison Precision
											// 0=Compare bit 7 of RGB
											// 1=Compare bits 7-6 of RGB
											// 2=Compare bits 7-5 of RGB
											// 3=Compare bits 7-4 of RGB
											// 4=Compare bits 7-3 of RGB
											// 5=Compare bits 7-2 of RGB
											// 6=Compare bits 7-1 of RGB
											// 7=Compare bits 7-0 of RGB
		uint8_t			ckctl_kc;			// [28:28] Key Control
											// 0=Use K bit for keying in KRGB-16 (1.5.5.5) mode
											// 1=Enable color or chroma keying for all modes other than KRGB-16

		// (MM8190 Secondary Stream Control)
		int16_t			ssctl_dda_haccum;	// [11: 0] DDA Horizontal Accumulator (signed 2's complement)
											// Set to: 2*(W0-1) - (W1-1) [FIXME: Am I reading that right? Datasheet says 2 (W0-1) - (W1-1)
											// Windows 3.1 DCI seems to compute instead: ((2*(W0-1) - (W1-1)) / 2)
											// W0 = line length in pixels before scaling
											// W1 = line length in pixels after scaling
		uint8_t			ssctl_sdif;			// [26:24] SDIF Secondary Stream Input Data Format
											// 1=YCbCr-16 (4.2.2), 16-240 input range (NTS: known as 'YUY2', 8-bit YUYV, for MPEG playback)
											// 2=YUV-16 (4.2.2), 0-255 input range (NTS: 8-bit YUYV but full range, perhaps appropriate for JPEG)
											// 3=KRGB-16 (1.5.5.5)
											// 4=YUV (2.1.1) (NTS: I think this means planar YUV, U and V are one quarter resolution. Also known in the industry as 4:2:0)
											// 5=RGB-16 (5.6.5)
											// 7=XRGB-32 (X.8.8.8)
		uint8_t			ssctl_sfc;			// [30:28] SFC - Secondary Stream Filter Characteristics
											// 0=Secondary Stream  1=..linear 0-2-4-2-0, for X stretch  2=..bilinear for 2X to 4X stretch
											// 3=..linear 1-2-2-2-1, for 4X stretch

		// (MM8194 Chroma Key Upper Bound)
		uint8_t			ckctl_b_ub;			// [ 7: 0] B/V/Cr KEY (UPPER)
		uint8_t			ckctl_g_ub;			// [15: 8] G/U/Cb KEY (UPPER)
		uint8_t			ckctl_r_ub;			// [23:16] R/Y KEY (UPPER)

		// (MM8198 Secondary Stream Stretch/Filter Constants)
		uint16_t		ssctl_k1_hscale;	// [10: 0] K1 horizontal scale factor
											// Set to: W0-1, where W0 is width of pixels of the initial output window before scaling
		int16_t			ssctl_k2_hscale;	// [26:16] K2 horizontal scale factor
											// Set to: W0-W1, where W1 is the width of pixels of the final scaled output window.
											// "This value is signed and will always be negative" (FIXME: Does that imply the card cannot *downscale* YUV overlays?)

		// (MM81A0 Blend Control)
		uint8_t			blendctl_ks;		// [ 4: 2] secondary stream blend coefficient
		uint8_t			blendctl_kp;		// [12:10] primary stream blend coefficient
		uint8_t			blendctl_composemode;//[26:24] compose mode
											// 0=secondary stream opaque overlay on primary stream
											// 1=primary stream opaque overlay on secondary stream
											// 2=dissolve, [Pp x Kp + Ps x (8 - Kp)]/8, ignore Ks
											// 3=Fade, [Pp x Kp + Ps x Ks]/8, where Kp + Ks must be <= 8
											// 5=Color key on primary stream (secondary stream overlay on primary stream)
											// 6=Color or chroma key on secondary stream (primary stream overlay on secondary stream)

		// (MM81C0 Primary Stream Frame Buffer Address 0)
		// (MM81C4 Primary Stream Frame Buffer Address 1)
		uint32_t		ps_fba[2];			// [21: 0] Primary Buffer Address

		// (MM81C8 Primary Stream Stride)
		uint32_t		ps_stride;			// [11: 0] Primary Stream Stride

		// (MM81CC Double Buffer/LPB Support)
		uint8_t			ps_bufsel;			// [ 0: 0] Primary Stream Buffer Select (0=address 0  1=address 1)
		uint8_t			ss_bufsel;			// [ 2: 1] Secondary Stream Buffer Select
											// 0=address 0
											// 1=address 1
											// 2=address (LPB input buffer select ^ 0) and opposite is for LPB
											// 3=address (LPB input buffer select ^ 1) and opposite is for LPB
		uint8_t			lpb_in_bufsel;		// [ 4: 4] LIS LPB Input Buffer Select
		uint8_t			lpb_in_bufselloading;//[ 5: 5] LSL LPB Input Buffer Select Loading
		uint8_t			lpb_in_bufseltoggle;// [ 6: 6] LST LPB Input Buffer Select Toggle

		// (MM81D0 Secondary Stream Frame Buffer Address 0)
		// (MM81D4 Secondary Stream Frame Buffer Address 1)
		uint32_t		ss_fba[2];			// [21: 0] Secondary Buffer Address

		// (MM81D8 Secondary Stream Stride)
		uint32_t		ss_stride;			// [11: 0] Secondary Stream Stride

		// (MM81DC Opaque Overlay Control)
		uint16_t		ooc_pixfetch_stop;  // [12: 3] Pixel stop fetch
		uint16_t		ooc_pixfetch_resume;// [28:19] Pixel resume fetch
		uint8_t			ooc_tss;			// [30:30] Top Stream Select  0=Secondary on top  1=Primary on top
		uint8_t			ooc_ooc_enable;		// [31:31] Opaque Overlay Control Enable  0=disabled  1=enabled

		// (MM81E0 K1 Vertical Scale Factor)
		uint16_t		k1_vscale_factor;	// [10: 0] K1 Vertical Scale Factor
											// Set to: [height in lines of initial output window before scaling] - 1

		// (MM81E4 K2 Vertical Scale Factor)
		int16_t			k2_vscale_factor;	// [10: 0] K2 Vertical Scale Factor
											// Set to: (height in lines before scale) - (height in lines of final window after scaling)

		// (MM81E8 DDA Vertical Accumulator Initial Value)
		int16_t			dda_vaccum_iv;		// [11: 0] DDA Vertical Accumulator
											// Set to: -((height in lines after scaling) - 1)
		uint8_t			evf;				// [15:15] EVF Enable Vertical Filtering

		// (MM81EC Stream FIFO and RAS Controls)
		uint8_t			fifo_alloc_ps;		// Interpretation of [4:0], where 5 bits are number of slots alloted to secondary stream.
											// N = secondary stream slots	 This value is set to 24 - N
		uint8_t			fifo_alloc_ss;		// Interpretation of [4:0], set to N (up to 24)
		uint8_t			fifo_ss_threshhold;	// Threshhold at which FIFO refill of secondary stream is triggered (low water point). Must be <= alloc_ss
		uint8_t			fifo_ps_threshhold;	// Threshhold at which FIFO refill of primary stream is triggered (low water point). Must be <= alloc_ps
		uint8_t			ras_rl;				// [15:15] RL RAS Low Time Control
		uint8_t			ras_rp;				// [16:16] RP RAS Pre-Charge Control
		uint8_t			edo_wsctl;			// [18:18] EDO Memory Wait State Control

		// (MM81F0 Primary Stream Window Start Coordinates)
		uint16_t		pswnd_start_y;		// [10: 0] Primary Stream Y start
											// Set to: Screen line number + 1
		uint16_t		pswnd_start_x;		// [26:16] Primary Stream X start
											// Set to: Screen pixel number + 1

		// (MM81F4 Primary Stream Window Size)
		uint16_t		pswnd_height;		// [10: 0] Primary Stream Height
											// Set to: number of lines
		uint16_t		pswnd_width;		// [26:16] Primary Stream Width
											// Set to: number of pixels - 1

		// (MM81F8 Secondary Stream Window Start Coordinates)
		uint16_t		sswnd_start_y;		// [10: 0] Secondary Stream Y start
											// Set to: Screen line number + 1
		uint16_t		sswnd_start_x;		// [26:16] Secondary Stream X start
											// Set to: Screen pixel number + 1

		// (MM81FC Secondary Stream Window Size)
		uint16_t		sswnd_height;		// [10: 0] Secondary Stream Height
											// Set to: number of lines
		uint16_t		sswnd_width;		// [26:16] Secondary Stream Width
											// Set to: number of pixels - 1
	} streams; // hardware YUV/RGB overlay i.e. for MPEG playback
} VGA_S3;

typedef struct {
	uint8_t mode_control;
	uint8_t enable_bits;
	bool blend;
} VGA_HERC;

typedef struct {
	uint32_t mask_plane;
	uint8_t write_plane;
	uint8_t read_plane;
	uint8_t border_color;
} VGA_AMSTRAD;

typedef struct {
	uint8_t index;
	uint8_t htotal;
	uint8_t hdend;
	uint8_t hsyncp;
	uint8_t hsyncw;
	uint8_t vtotal;
	uint8_t vdend;
	uint8_t vadjust;
	uint8_t vsyncp;
	uint8_t vsyncw;
	uint8_t max_scanline;
	uint16_t lightpen;
	bool lightpen_triggered;
	uint8_t cursor_start;
	uint8_t cursor_end;
    uint8_t mcga_mode_control;
} VGA_OTHER;

typedef struct {
	uint8_t pcjr_flipflop;
	uint8_t mode_control;
	uint8_t color_select;
	uint8_t disp_bank;
	uint8_t reg_index;
	uint8_t gfx_control;
	uint8_t palette_mask;
	uint8_t extended_ram;
	uint8_t border_color;
	uint8_t line_mask, line_shift;
	uint8_t draw_bank, mem_bank;
	uint8_t *draw_base, *mem_base;
	Bitu addr_mask;
} VGA_TANDY;

typedef struct {
	uint8_t index;
	uint8_t reset;
	uint8_t clocking_mode;
	uint8_t map_mask;
	uint8_t character_map_select;
	uint8_t memory_mode;
} VGA_Seq;

typedef struct {
	uint8_t palette[16];
	uint8_t mode_control;
	uint8_t horizontal_pel_panning;
	uint8_t overscan_color;
	uint8_t color_plane_enable;
	uint8_t color_select;
	uint8_t index;
	uint8_t disabled; // Used for disabling the screen.
					// Bit0: screen disabled by attribute controller index
					// Bit1: screen disabled by sequencer index 1 bit 5
					// These are put together in one variable for performance reasons:
					// the line drawing function is called maybe 60*480=28800 times/s,
					// and we only need to check one variable for zero this way.
} VGA_Attr;

typedef struct {
	uint8_t horizontal_total;
	uint8_t horizontal_display_end;
	uint8_t start_horizontal_blanking;
	uint8_t end_horizontal_blanking;
	uint8_t start_horizontal_retrace;
	uint8_t end_horizontal_retrace;
	uint8_t vertical_total;
	uint8_t overflow;
	uint8_t preset_row_scan;
	uint8_t maximum_scan_line;
	uint8_t cursor_start;
	uint8_t cursor_end;
	uint8_t start_address_high;
	uint8_t start_address_low;
	uint8_t cursor_location_high;
	uint8_t cursor_location_low;
	uint8_t vertical_retrace_start;
	uint8_t vertical_retrace_end;
	uint8_t vertical_display_end;
	uint8_t offset;
	uint8_t underline_location;
	uint8_t start_vertical_blanking;
	uint8_t end_vertical_blanking;
	uint8_t mode_control;
	uint8_t line_compare;

	uint8_t index;
	bool read_only;
} VGA_Crtc;

typedef struct {
	uint8_t index;
	uint8_t set_reset;
	uint8_t enable_set_reset;
	uint8_t color_compare;
	uint8_t data_rotate;
	uint8_t read_map_select;
	uint8_t mode;
	uint8_t miscellaneous;
	uint8_t color_dont_care;
	uint8_t bit_mask;
} VGA_Gfx;

typedef struct  {
	uint8_t red;
	uint8_t green;
	uint8_t blue;
} RGBEntry;

typedef struct {
	uint8_t bits;						/* DAC bits, usually 6 or 8 */
	uint8_t pel_mask;
	uint8_t pel_index;	
	uint8_t state;
	uint8_t write_index;
	uint8_t read_index;
	Bitu first_changed;
	uint8_t combine[16];
	RGBEntry rgb[0x100];
	uint16_t xlat16[256];
	uint32_t xlat32[256];
	uint8_t hidac_counter;
	uint8_t reg02;
} VGA_Dac;

typedef struct {
	Bitu	readStart, writeStart;
	Bitu	bankMask;
	Bitu	bank_read_full;
	Bitu	bank_write_full;
	uint8_t	bank_read;
	uint8_t	bank_write;
	Bitu	bank_size;
} VGA_SVGA;

typedef union CGA_Latch {
	uint16_t d;
    uint8_t b[2] = {};

    CGA_Latch() { }
    CGA_Latch(const uint16_t raw) : d(raw) { }
} CGA_Latch;

typedef union VGA_Latch {
	uint32_t d;
    uint8_t b[4] = {};

    VGA_Latch() { }
    VGA_Latch(const uint32_t raw) : d(raw) { }
} VGA_Latch;

typedef struct VGA_Memory_t {
	uint8_t*      linear = NULL;
	uint8_t*      linear_orgptr = NULL;

    uint32_t    memsize = 0;
    uint32_t    memmask = 0;
    uint32_t    memmask_crtc = 0;       // in CRTC-visible units (depends on byte/word/dword mode)
} VGA_Memory;

typedef struct {
	uint32_t page;
	uint32_t addr;
	uint32_t mask;
	PageHandler *handler;
} VGA_LFB;

static const size_t VGA_Draw_2_elem = 2;

typedef struct VGA_Type_t {
    VGAModes mode = {};                              /* The mode the vga system is in */
    VGAModes lastmode = {};
    uint8_t misc_output = 0;
    VGA_Draw_2 draw_2[VGA_Draw_2_elem];         /* new parallel video emulation. PC-98 mode will use both, all others only the first. */
    VGA_Draw draw = {};
    VGA_Config config = {};
    VGA_Internal internal = {};
    /* Internal module groups */
    VGA_Seq seq = {};
    VGA_Attr attr = {};
    VGA_Crtc crtc = {};
    VGA_Gfx gfx = {};
    VGA_Dac dac = {};
    VGA_Latch latch;
    VGA_S3 s3 = {};
    VGA_SVGA svga = {};
    VGA_HERC herc = {};
    VGA_TANDY tandy = {};
    VGA_AMSTRAD amstrad = {};
    VGA_OTHER other = {};
    VGA_Memory mem;
    VGA_LFB lfb = {};
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
void VGA_DAC_CombineColor(uint8_t attr,uint8_t pal);
void VGA_DAC_SetEntry(Bitu entry,uint8_t red,uint8_t green,uint8_t blue);
void VGA_ATTR_SetPalette(uint8_t index,uint8_t val);

typedef enum {CGA, EGA, MONO} EGAMonitorMode;

typedef enum {AC_4x4, AC_low4/*4low*/} ACPalRemapMode;

extern unsigned char VGA_AC_remap;

void VGA_ATTR_SetEGAMonitorPalette(EGAMonitorMode m);

/* The VGA Subfunction startups */
void VGA_SetupAttr(void);
void VGA_SetupMemory(void);
void VGA_SetupDAC(void);
void VGA_SetupMisc(void);
void VGA_SetupGFX(void);
void VGA_SetupSEQ(void);
void VGA_SetupOther(void);
void VGA_SetupXGA(void);

/* Some Support Functions */
void VGA_SetClock(Bitu which,Bitu target);
void VGA_StartUpdateLFB(void);
void VGA_SetBlinking(Bitu enabled);
void VGA_SetCGA2Table(uint8_t val0,uint8_t val1);
void VGA_SetCGA4Table(uint8_t val0,uint8_t val1,uint8_t val2,uint8_t val3);
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
	uint8_t ver_overflow;
	uint8_t hor_overflow;
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

extern uint32_t ExpandTable[256];
extern uint32_t FillTable[16];
extern uint32_t CGA_2_Table[16];
extern uint32_t CGA_4_Table[256];
extern uint32_t CGA_4_HiRes_Table[256];
extern uint32_t CGA_16_Table[256];
extern uint32_t TXT_Font_Table[16];
extern uint32_t TXT_FG_Table[16];
extern uint32_t TXT_BG_Table[16];
extern uint32_t Expand16Table[4][16];
extern uint32_t Expand16BigTable[0x10000];

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

/* current dosplay page (controlled by A4h) */
extern unsigned char *pc98_pgraph_current_display_page;
/* current CPU page (controlled by A6h) */
extern unsigned char *pc98_pgraph_current_cpu_page;

/* functions to help cleanup memory map access instead of hardcoding offsets.
 * your C++ compiler should be smart enough to inline these into the body of this function. */

/* TODO: Changes to memory layout relative to vga.mem.linear:
 *
 *       Text to take 0x00000-0x03FFF instead of 0x00000-0x07FFF.
 *
 *       Graphics to start at 0x04000
 *
 *       Each bitplane will be 64KB (0x10000) bytes long, and the page flip bit in 0xA6
 *       will select which 32KB half within the 64KB block to use, or if another bit is
 *       set as documented in Undocumented PC-98, the full 64KB block.
 *
 *       The 512KB + 32KB will be reduced slightly to 512KB + 16KB to match the layout.
 *
 *       The bitplane layout change will permit emulating an 8 bitplane 256-color mode
 *       suggested by Yksoft1 that early PC-9821 systems supported and that the 256-color
 *       driver shipped with Windows 3.1 (for PC-98) uses. Based on Windows 3.1 behavior
 *       that also means the linear framebuffer at 0xF00000 must also change in planar
 *       mode to spread all 8 bits across the planes on write and gather all 8 bits
 *       on read. As far as I can tell the Windows 3.1 256-color driver uses planar
 *       and EGC functions as it would in 16-color mode, but draws bitmaps using the
 *       LFB. The picture is wrong EXCEPT when Windows icons and bitmaps are drawn.
 *
 *       256-color packed mode will be retained as direct LFB mapping from the start of
 *       graphics RAM.
 *
 *       On a real PC-9821 laptop, contents accessible to the CPU noticeably shift order
 *       and position when you switch on/off 256-color packed mode, suggesting that the
 *       planar mode is simply reordered memory access in hardware OR that 256-color
 *       mode is "chained" (much like 256-color packed mode on IBM VGA hardware) across
 *       bitplanes. */

#define PC98_VRAM_TEXT_OFFSET           ( 0x00000u )        /* 16KB memory (8KB text + 8KB attributes) */
#define PC98_VRAM_GRAPHICS_OFFSET       ( 0x04000u )        /* where graphics memory begins */

#define PC98_VRAM_BITPLANE_SIZE         ( 0x10000u )        /* one bitplane */

#define PC98_VRAM_PAGEFLIP_SIZE         ( 0x08000u )        /* add this amount for the second page in 8/16/256-color planar mode */
#define PC98_VRAM_PAGEFLIP256_SIZE      ( 0x40000u )        /* add this amount for the second page in 256-color packed mode */

#define PC98_VRAM_256BANK_SIZE          ( 0x08000u )        /* window/bank size (256-color packed) */

extern uint32_t pc98_pegc_banks[2];

static inline unsigned char *pc98_vram_text(void) {
    return vga.mem.linear + PC98_VRAM_TEXT_OFFSET;
}

/* return value is relative to current CPU page or current display page ptr */
static inline constexpr unsigned int pc98_pgram_bitplane_offset(const unsigned int b) {
    /* WARNING: b is not range checked for performance! Do not call with b >= 8 if memsize = 512KB or b >= 4 if memsize >= 256KB */
    return (b * PC98_VRAM_BITPLANE_SIZE);
}

static inline unsigned char *pc98_vram_256bank_from_window(const unsigned int b) {
    /* WARNING: b is not range checked for performance! Do not call with b >= 2 */
    return vga.mem.linear + PC98_VRAM_GRAPHICS_OFFSET + pc98_pegc_banks[b];
}

#define VRAM98_TEXT         ( pc98_vram_text() )

#endif
